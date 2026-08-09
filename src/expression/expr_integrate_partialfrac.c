#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#define MARS_EXPR_INTEGRATE_INTERNAL_ACCESS
#include "expr_integrate_internal.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"

typedef struct {
    number_t shift;
    size_t multiplicity;
} partial_fraction_factor_t;

typedef struct {
    number_t scale;
    partial_fraction_factor_t *factors;
    size_t count;
    size_t capacity;
    size_t total_multiplicity;
} partial_fraction_factorization_t;

enum {
    partial_fraction_poly_coeff_count = 5u,
    laurent_poly_coeff_count = 9u,
    rational_power_poly_coeff_count = 17u,
    rational_power_system_size = 32u
};

static void partial_fraction_array_zero(number_t *values, size_t count)
{
    for (size_t i = 0; i < count; ++i)
        values[i] = num_clone(NUM_ZERO);
}

static number_t partial_fraction_normalize_small_rational(number_t value)
{
    double value_d;

    if (!num_is_real(value) || !num_is_finite(value))
        return num_clone(value);
    {
        long numerator;
        long denominator;

        if (num_get_small_rational(value, &numerator, &denominator))
            return num_clone(value);
    }

    value_d = num_to_double(value);
    if (!isfinite(value_d))
        return num_clone(value);

    for (long denominator = 1L; denominator <= 64L; ++denominator) {
        long numerator = lround(value_d * (double)denominator);
        number_t candidate;
        number_t diff;
        number_t abs_diff;
        double err;
        double scale;

        if (numerator < -1024L || numerator > 1024L)
            continue;

        candidate = num_create_from_frac(numerator, denominator);
        diff = num_sub(value, candidate);
        abs_diff = num_abs(diff);
        err = fabs(num_to_double(abs_diff));
        scale = fmax(1.0, fabs(value_d));

        num_destroy(&abs_diff);
        num_destroy(&diff);
        if (err <= scale * 1e-30)
            return candidate;
        num_destroy(&candidate);
    }

    return num_clone(value);
}

static void partial_fraction_factorization_init(partial_fraction_factorization_t *out)
{
    out->scale = num_clone(NUM_ONE);
    out->factors = NULL;
    out->count = 0u;
    out->capacity = 0u;
    out->total_multiplicity = 0u;
}

static void partial_fraction_factorization_clear(partial_fraction_factorization_t *value)
{
    if (!value)
        return;
    for (size_t i = 0; i < value->count; ++i)
        num_destroy(&value->factors[i].shift);
    free(value->factors);
    value->factors = NULL;
    value->count = 0u;
    value->capacity = 0u;
    value->total_multiplicity = 0u;
    num_destroy(&value->scale);
}

static bool small_positive_int_from_number(number_t value, size_t *out)
{
    long numerator;
    long denominator;

    if (!out || !num_is_real(value) || !num_is_integer(value))
        return false;
    if (!num_get_small_rational(value, &numerator, &denominator) || denominator != 1 || numerator <= 0)
        return false;
    *out = (size_t)numerator;
    return true;
}

static bool poly_is_exact_real_local(const number_t *coeffs, size_t degree)
{
    for (size_t i = 0; i <= degree; ++i) {
        if (num_eq(coeffs[i], NUM_ZERO))
            continue;
        if (!num_is_real(coeffs[i]))
            return false;
    }
    return true;
}

static size_t poly_degree_local(const number_t *coeffs, size_t count);

static bool same_wrt_var_local(const expr_t *expr, const expr_t *wrt)
{
    if (!expr || !wrt || !expr_is_var(expr) || !expr_is_var(wrt))
        return false;
    if (is_wrt(expr, wrt))
        return true;
    if (expr->name && wrt->name)
        return strcmp(expr->name, wrt->name) == 0;
    return false;
}

static void poly_add_scaled_term_local(number_t *coeffs, size_t index, number_t term)
{
    number_t next = num_add(coeffs[index], term);

    num_destroy(&coeffs[index]);
    coeffs[index] = next;
}

static bool poly_match_affine_basis_deg4(const expr_t *expr, const expr_t *wrt, number_t *coeffs, size_t *degree_out)
{
    expr_t *vars[1];
    number_t basis_constant = num_new();
    number_t basis_coeffs[1];
    number_t affine_poly[5];
    bool ok = false;

    vars[0] = (expr_t *)wrt;
    basis_coeffs[0] = num_new();
    partial_fraction_array_zero(affine_poly, 5);
    partial_fraction_array_zero(coeffs, 5);

    if (!expr_match_affine_poly_deg4(expr, 1u, vars, affine_poly, &basis_constant, basis_coeffs))
        goto cleanup;

    for (size_t power = 0; power < 5u; ++power) {
        static const long binom[5][5] = {
            {1, 0, 0, 0, 0}, {1, 1, 0, 0, 0}, {1, 2, 1, 0, 0}, {1, 3, 3, 1, 0}, {1, 4, 6, 4, 1},
        };

        if (num_eq(affine_poly[power], NUM_ZERO))
            continue;
        for (size_t x_power = 0; x_power <= power; ++x_power) {
            size_t const_power = power - x_power;
            number_t choose = num_create_from_long(binom[power][x_power]);
            number_t const_exp = num_create_from_long((long)const_power);
            number_t x_exp = num_create_from_long((long)x_power);
            number_t term = num_mul(affine_poly[power], choose);
            number_t const_factor = num_pow(basis_constant, const_exp);
            number_t x_factor = num_pow(basis_coeffs[0], x_exp);
            number_t basis_scale = num_mul(const_factor, x_factor);
            number_t scaled = num_mul(term, basis_scale);

            poly_add_scaled_term_local(coeffs, x_power, scaled);
            num_destroy(&scaled);
            num_destroy(&basis_scale);
            num_destroy(&x_factor);
            num_destroy(&const_factor);
            num_destroy(&term);
            num_destroy(&x_exp);
            num_destroy(&const_exp);
            num_destroy(&choose);
        }
    }

    if (!poly_is_exact_real_local(coeffs, 4u))
        goto cleanup;

    if (degree_out)
        *degree_out = poly_degree_local(coeffs, 5u);
    ok = true;

cleanup:
    number_array_clear_local(affine_poly, 5);
    num_destroy(&basis_coeffs[0]);
    num_destroy(&basis_constant);
    if (!ok)
        number_array_clear_local(coeffs, 5);
    return ok;
}

static void poly_copy_local(number_t *dst, const number_t *src, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        num_destroy(&dst[i]);
        dst[i] = num_clone(src[i]);
    }
}

static bool split_wrt_independent_product_factor(const expr_t *expr, const expr_t *wrt, expr_t **factor_out,
                                                 expr_t **rest_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    expr_t *factor = NULL;
    expr_t *rest = NULL;
    expr_t *inner_rest = NULL;
    bool ok = false;

    if (!expr || !wrt || !factor_out || !rest_out)
        return false;

    if (!depends_on_wrt(expr, wrt)) {
        factor = expr_clone(expr);
        rest = expr_new_const(NUM_ONE);
        ok = factor && rest;
        goto cleanup;
    }

    if (!expr_match_mul_expr(expr, &left, &right))
        return false;

    if (!depends_on_wrt(left, wrt)) {
        factor = expr_clone(left);
        rest = expr_clone(right);
        ok = factor && rest;
        goto cleanup;
    }

    if (!depends_on_wrt(right, wrt)) {
        factor = expr_clone(right);
        rest = expr_clone(left);
        ok = factor && rest;
        goto cleanup;
    }

    if (split_wrt_independent_product_factor(left, wrt, &factor, &inner_rest)) {
        rest = (inner_rest && right) ? expr_mul(inner_rest, right) : NULL;
        ok = factor && rest;
        goto cleanup;
    }

    if (split_wrt_independent_product_factor(right, wrt, &factor, &inner_rest)) {
        rest = (left && inner_rest) ? expr_mul(left, inner_rest) : NULL;
        ok = factor && rest;
        goto cleanup;
    }

cleanup:
    expr_free(inner_rest);
    if (ok) {
        *factor_out = factor;
        *rest_out = rest;
    } else {
        expr_free(factor);
        expr_free(rest);
    }
    return ok;
}

static size_t poly_degree_local(const number_t *coeffs, size_t count)
{
    for (size_t i = count; i-- > 0u;) {
        if (!num_eq(coeffs[i], NUM_ZERO))
            return i;
    }
    return 0u;
}

static bool poly_match_direct_limited_rec(const expr_t *expr, const expr_t *wrt, number_t *coeffs, size_t count)
{
    number_t constant = num_new();
    number_t exponent = num_new();
    number_t lhs[rational_power_poly_coeff_count];
    number_t rhs[rational_power_poly_coeff_count];
    bool coeffs_ready = false;
    bool lhs_ready = false;
    bool rhs_ready = false;
    bool ok = false;

    if (count == 0u || count > rational_power_poly_coeff_count)
        goto cleanup;

    partial_fraction_array_zero(coeffs, count);
    coeffs_ready = true;

    if (!expr) {
        ok = false;
        goto cleanup;
    }

    if (expr_match_const_value(expr, &constant) && num_is_real(constant)) {
        num_destroy(&coeffs[0]);
        coeffs[0] = num_clone(constant);
        ok = true;
        goto cleanup;
    }

    if (same_wrt_var_local(expr, wrt)) {
        num_destroy(&coeffs[1]);
        coeffs[1] = num_clone(NUM_ONE);
        ok = true;
        goto cleanup;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_NEG) {
        if (!poly_match_direct_limited_rec(expr->a, wrt, lhs, count))
            goto cleanup;
        lhs_ready = true;
        for (size_t i = 0; i < count; ++i) {
            num_destroy(&coeffs[i]);
            coeffs[i] = num_neg(lhs[i]);
        }
        ok = true;
        goto cleanup;
    }

    if (expr->ops && (expr->ops->kind == EXPR_KIND_ADD || expr->ops->kind == EXPR_KIND_SUB)) {
        if (!poly_match_direct_limited_rec(expr->a, wrt, lhs, count)) {
            goto cleanup;
        }
        lhs_ready = true;
        if (!poly_match_direct_limited_rec(expr->b, wrt, rhs, count)) {
            goto cleanup;
        }
        rhs_ready = true;
        for (size_t i = 0; i < count; ++i) {
            number_t next = (expr->ops->kind == EXPR_KIND_ADD) ? num_add(lhs[i], rhs[i]) : num_sub(lhs[i], rhs[i]);

            num_destroy(&coeffs[i]);
            coeffs[i] = next;
        }
        ok = true;
        goto cleanup;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_MUL) {
        if (!poly_match_direct_limited_rec(expr->a, wrt, lhs, count)) {
            goto cleanup;
        }
        lhs_ready = true;
        if (!poly_match_direct_limited_rec(expr->b, wrt, rhs, count)) {
            goto cleanup;
        }
        rhs_ready = true;
        if (poly_degree_local(lhs, count) + poly_degree_local(rhs, count) >= count)
            goto cleanup;
        for (size_t i = 0; i < count; ++i) {
            for (size_t j = 0; i + j < count; ++j) {
                number_t term = num_mul(lhs[i], rhs[j]);
                number_t next = num_add(coeffs[i + j], term);

                num_destroy(&coeffs[i + j]);
                coeffs[i + j] = next;
                num_destroy(&term);
            }
        }
        ok = true;
        goto cleanup;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_DIV && expr_match_const_value(expr->b, &constant) &&
        num_is_real(constant) && !num_eq(constant, NUM_ZERO)) {
        if (!poly_match_direct_limited_rec(expr->a, wrt, lhs, count))
            goto cleanup;
        lhs_ready = true;
        for (size_t i = 0; i < count; ++i) {
            number_t next = num_div(lhs[i], constant);

            num_destroy(&coeffs[i]);
            coeffs[i] = next;
        }
        ok = true;
        goto cleanup;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW_D) {
        size_t power = 0u;

        if (!small_positive_int_from_number(expr->c, &power) || power >= count ||
            !poly_match_direct_limited_rec(expr->a, wrt, lhs, count)) {
            goto cleanup;
        }
        lhs_ready = true;

        num_destroy(&coeffs[0]);
        coeffs[0] = num_clone(NUM_ONE);
        for (size_t iter = 0; iter < power; ++iter) {
            number_t next[rational_power_poly_coeff_count];

            if (poly_degree_local(coeffs, count) + poly_degree_local(lhs, count) >= count)
                goto cleanup;
            partial_fraction_array_zero(next, count);
            for (size_t i = 0; i < count; ++i) {
                for (size_t j = 0; i + j < count; ++j) {
                    number_t term = num_mul(coeffs[i], lhs[j]);
                    number_t sum = num_add(next[i + j], term);

                    num_destroy(&next[i + j]);
                    next[i + j] = sum;
                    num_destroy(&term);
                }
            }
            poly_copy_local(coeffs, next, count);
            number_array_clear_local(next, count);
        }
        ok = true;
        goto cleanup;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW && expr->b && expr_match_const_value(expr->b, &exponent)) {
        size_t power = 0u;

        if (!small_positive_int_from_number(exponent, &power) || power >= count ||
            !poly_match_direct_limited_rec(expr->a, wrt, lhs, count)) {
            goto cleanup;
        }
        lhs_ready = true;

        num_destroy(&coeffs[0]);
        coeffs[0] = num_clone(NUM_ONE);
        for (size_t iter = 0; iter < power; ++iter) {
            number_t next[rational_power_poly_coeff_count];

            if (poly_degree_local(coeffs, count) + poly_degree_local(lhs, count) >= count)
                goto cleanup;
            partial_fraction_array_zero(next, count);
            for (size_t i = 0; i < count; ++i) {
                for (size_t j = 0; i + j < count; ++j) {
                    number_t term = num_mul(coeffs[i], lhs[j]);
                    number_t sum = num_add(next[i + j], term);

                    num_destroy(&next[i + j]);
                    next[i + j] = sum;
                    num_destroy(&term);
                }
            }
            poly_copy_local(coeffs, next, count);
            number_array_clear_local(next, count);
        }
        ok = true;
        goto cleanup;
    }

cleanup:
    if (rhs_ready)
        number_array_clear_local(rhs, count);
    if (lhs_ready)
        number_array_clear_local(lhs, count);
    num_destroy(&exponent);
    num_destroy(&constant);
    if (!ok && coeffs_ready)
        number_array_clear_local(coeffs, count);
    return ok;
}

static bool poly_match_direct_deg4(const expr_t *expr, const expr_t *wrt, number_t *coeffs, size_t *degree_out)
{
    if (!poly_match_direct_limited_rec(expr, wrt, coeffs, partial_fraction_poly_coeff_count)) {
        if (!poly_match_affine_basis_deg4(expr, wrt, coeffs, degree_out))
            return false;
        return true;
    }

    if (!poly_is_exact_real_local(coeffs, 4u))
        goto fail;

    if (degree_out)
        *degree_out = poly_degree_local(coeffs, 5u);
    return true;

fail:
    number_array_clear_local(coeffs, 5);
    return false;
}

static bool partial_fraction_append_shift(partial_fraction_factorization_t *out, number_t shift, size_t multiplicity)
{
    for (size_t i = 0; i < out->count; ++i) {
        if (num_eq(out->factors[i].shift, shift)) {
            out->factors[i].multiplicity += multiplicity;
            out->total_multiplicity += multiplicity;
            return true;
        }
    }

    if (out->count == out->capacity) {
        size_t next_capacity = out->capacity ? (out->capacity * 2u) : 4u;
        partial_fraction_factor_t *grown = realloc(out->factors, next_capacity * sizeof(*grown));

        if (!grown)
            return false;
        out->factors = grown;
        out->capacity = next_capacity;
    }

    out->factors[out->count].shift = num_clone(shift);
    out->factors[out->count].multiplicity = multiplicity;
    out->count += 1u;
    out->total_multiplicity += multiplicity;
    return true;
}

static bool partial_fraction_scale_mul(partial_fraction_factorization_t *out, number_t factor, size_t power)
{
    for (size_t i = 0; i < power; ++i) {
        number_t next = num_mul(out->scale, factor);

        num_destroy(&out->scale);
        out->scale = next;
    }
    return true;
}

static bool partial_fraction_factor_poly_coeffs(const number_t *coeffs, size_t degree,
                                                partial_fraction_factorization_t *out)
{
    if (degree == 0u) {
        return partial_fraction_scale_mul(out, coeffs[0], 1u);
    }

    if (degree == 1u) {
        number_t shift = num_div(coeffs[0], coeffs[1]);
        bool ok = partial_fraction_scale_mul(out, coeffs[1], 1u) && partial_fraction_append_shift(out, shift, 1u);

        num_destroy(&shift);
        return ok;
    }

    if (degree == 2u) {
        number_t two = num_create_from_long(2);
        number_t four = num_create_from_long(4);
        number_t minus_b = num_neg(coeffs[1]);
        number_t two_a = num_mul(two, coeffs[2]);
        number_t disc_left = num_mul(coeffs[1], coeffs[1]);
        number_t disc_product = num_mul(coeffs[2], coeffs[0]);
        number_t disc_right = num_mul(four, disc_product);
        number_t disc = num_sub(disc_left, disc_right);
        number_t sqrt_disc = num_sqrt(disc);
        number_t sqrt_disc_sq = num_mul(sqrt_disc, sqrt_disc);
        bool exact_square = num_is_real(sqrt_disc) && num_eq(sqrt_disc_sq, disc);

        if (exact_square) {
            number_t root1_numer = num_add(minus_b, sqrt_disc);
            number_t root2_numer = num_sub(minus_b, sqrt_disc);
            number_t root1 = num_div(root1_numer, two_a);
            number_t root2 = num_div(root2_numer, two_a);
            number_t shift1 = num_neg(root1);
            number_t shift2 = num_neg(root2);
            bool ok = partial_fraction_scale_mul(out, coeffs[2], 1u) &&
                      partial_fraction_append_shift(out, shift1, 1u) && partial_fraction_append_shift(out, shift2, 1u);

            num_destroy(&shift2);
            num_destroy(&shift1);
            num_destroy(&root2);
            num_destroy(&root1);
            num_destroy(&root2_numer);
            num_destroy(&root1_numer);
            num_destroy(&sqrt_disc_sq);
            num_destroy(&sqrt_disc);
            num_destroy(&disc);
            num_destroy(&disc_right);
            num_destroy(&disc_product);
            num_destroy(&disc_left);
            num_destroy(&two_a);
            num_destroy(&minus_b);
            num_destroy(&four);
            num_destroy(&two);
            return ok;
        }

        num_destroy(&sqrt_disc_sq);
        num_destroy(&sqrt_disc);
        num_destroy(&disc);
        num_destroy(&disc_right);
        num_destroy(&disc_product);
        num_destroy(&disc_left);
        num_destroy(&two_a);
        num_destroy(&minus_b);
        num_destroy(&four);
        num_destroy(&two);
        return false;
    }

    if (num_eq(coeffs[0], NUM_ZERO)) {
        number_t tail[5];
        bool ok;

        partial_fraction_array_zero(tail, 5);
        for (size_t i = 0; i < degree; ++i) {
            num_destroy(&tail[i]);
            tail[i] = num_clone(coeffs[i + 1u]);
        }
        ok = partial_fraction_append_shift(out, NUM_ZERO, 1u) &&
             partial_fraction_factor_poly_coeffs(tail, degree - 1u, out);
        number_array_clear_local(tail, 5);
        return ok;
    }

    return false;
}

static bool partial_fraction_collect_linear_factors(const expr_t *expr, const expr_t *wrt,
                                                    partial_fraction_factorization_t *out)
{
    number_t constant = num_new();
    number_t coeff = num_new();
    number_t exponent = num_new();
    number_t poly[5];
    size_t degree = 0u;
    bool poly_ready = false;
    bool ok = false;

    if (!expr) {
        ok = false;
        goto cleanup;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_MUL) {
        ok = partial_fraction_collect_linear_factors(expr->a, wrt, out) &&
             partial_fraction_collect_linear_factors(expr->b, wrt, out);
        if (ok)
            goto cleanup;
    }

    if (expr_match_const_value(expr, &constant) && num_is_real(constant)) {
        ok = partial_fraction_scale_mul(out, constant, 1u);
        goto cleanup;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW_D && expr->a &&
        small_positive_int_from_number(expr->c, &degree)) {
        partial_fraction_factorization_t base;

        partial_fraction_factorization_init(&base);
        if (!partial_fraction_collect_linear_factors(expr->a, wrt, &base)) {
            partial_fraction_factorization_clear(&base);
        } else {
            if (!partial_fraction_scale_mul(out, base.scale, degree)) {
                partial_fraction_factorization_clear(&base);
                goto cleanup;
            }
            for (size_t i = 0; i < base.count; ++i) {
                if (!partial_fraction_append_shift(out, base.factors[i].shift, base.factors[i].multiplicity * degree)) {
                    partial_fraction_factorization_clear(&base);
                    goto cleanup;
                }
            }
            partial_fraction_factorization_clear(&base);
            ok = true;
            goto cleanup;
        }
        partial_fraction_factorization_clear(&base);
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW && expr->a && expr->b &&
        expr_match_const_value(expr->b, &exponent) && small_positive_int_from_number(exponent, &degree)) {
        partial_fraction_factorization_t base;

        partial_fraction_factorization_init(&base);
        if (!partial_fraction_collect_linear_factors(expr->a, wrt, &base)) {
            partial_fraction_factorization_clear(&base);
        } else {
            if (!partial_fraction_scale_mul(out, base.scale, degree)) {
                partial_fraction_factorization_clear(&base);
                goto cleanup;
            }
            for (size_t i = 0; i < base.count; ++i) {
                if (!partial_fraction_append_shift(out, base.factors[i].shift, base.factors[i].multiplicity * degree)) {
                    partial_fraction_factorization_clear(&base);
                    goto cleanup;
                }
            }
            partial_fraction_factorization_clear(&base);
            ok = true;
            goto cleanup;
        }
        partial_fraction_factorization_clear(&base);
    }

    if (poly_match_direct_deg4(expr, wrt, poly, &degree)) {
        poly_ready = true;
        ok = partial_fraction_factor_poly_coeffs(poly, degree, out);
        goto cleanup;
    }

    if (match_nonconstant_affine_linear_expr(expr, wrt, &constant, &coeff) && num_is_real(constant) &&
        num_is_real(coeff) && !num_eq(coeff, NUM_ZERO)) {
        number_t shift = num_div(constant, coeff);

        ok = partial_fraction_scale_mul(out, coeff, 1u) && partial_fraction_append_shift(out, shift, 1u);
        num_destroy(&shift);
        goto cleanup;
    }

cleanup:
    if (poly_ready)
        number_array_clear_local(poly, 5);
    num_destroy(&exponent);
    num_destroy(&coeff);
    num_destroy(&constant);
    return ok;
}

static void poly_mul_by_linear_in_place(number_t *coeffs, size_t *degree, number_t shift)
{
    number_t next[5];

    partial_fraction_array_zero(next, 5);
    for (size_t i = 0; i <= *degree; ++i) {
        number_t shifted = num_mul(coeffs[i], shift);
        number_t sum0 = num_add(next[i], shifted);

        num_destroy(&next[i]);
        next[i] = sum0;
        num_destroy(&shifted);

        if (i + 1u < 5u) {
            number_t sum1 = num_add(next[i + 1u], coeffs[i]);

            num_destroy(&next[i + 1u]);
            next[i + 1u] = sum1;
        }
    }
    *degree += 1u;
    poly_copy_local(coeffs, next, 5u);
    number_array_clear_local(next, 5);
}

static bool build_normalized_denominator_poly(const partial_fraction_factorization_t *factors, number_t *coeffs)
{
    size_t degree = 0u;

    partial_fraction_array_zero(coeffs, 5);
    num_destroy(&coeffs[0]);
    coeffs[0] = num_clone(NUM_ONE);

    if (factors->total_multiplicity > 4u)
        return false;

    for (size_t i = 0; i < factors->count; ++i) {
        for (size_t repeat = 0; repeat < factors->factors[i].multiplicity; ++repeat) {
            if (degree >= 4u)
                return false;
            poly_mul_by_linear_in_place(coeffs, &degree, factors->factors[i].shift);
        }
    }
    return true;
}

static void build_partial_fraction_basis_poly(const partial_fraction_factorization_t *factors, size_t factor_index,
                                              size_t power, number_t *coeffs)
{
    size_t degree = 0u;

    partial_fraction_array_zero(coeffs, 5);
    num_destroy(&coeffs[0]);
    coeffs[0] = num_clone(NUM_ONE);

    for (size_t i = 0; i < factors->count; ++i) {
        size_t repeats = factors->factors[i].multiplicity;

        if (i == factor_index)
            repeats -= power;
        for (size_t repeat = 0; repeat < repeats; ++repeat)
            poly_mul_by_linear_in_place(coeffs, &degree, factors->factors[i].shift);
    }
}

static bool poly_divide_local(const number_t *numerator, size_t num_degree, const number_t *denominator,
                              size_t den_degree, number_t *quotient, number_t *remainder)
{
    number_t work[5];

    if (den_degree == 0u || num_eq(denominator[den_degree], NUM_ZERO))
        return false;

    partial_fraction_array_zero(work, 5);
    partial_fraction_array_zero(quotient, 5);
    partial_fraction_array_zero(remainder, 5);
    poly_copy_local(work, numerator, 5u);

    while (num_degree >= den_degree && !num_eq(work[num_degree], NUM_ZERO)) {
        size_t shift = num_degree - den_degree;
        number_t lead = num_div(work[num_degree], denominator[den_degree]);

        num_destroy(&quotient[shift]);
        quotient[shift] = num_add(quotient[shift], lead);
        for (size_t i = 0; i <= den_degree; ++i) {
            number_t scaled = num_mul(lead, denominator[i]);
            number_t next = num_sub(work[i + shift], scaled);

            num_destroy(&work[i + shift]);
            work[i + shift] = next;
            num_destroy(&scaled);
        }
        num_destroy(&lead);
        while (num_degree > 0u && num_eq(work[num_degree], NUM_ZERO))
            --num_degree;
        if (num_degree < den_degree)
            break;
    }

    poly_copy_local(remainder, work, 5u);
    number_array_clear_local(work, 5);
    return true;
}

static bool solve_linear_system_local(size_t n, number_t matrix[4][5], number_t *solution)
{
    for (size_t col = 0; col < n; ++col) {
        size_t pivot = col;

        while (pivot < n && num_eq(matrix[pivot][col], NUM_ZERO))
            ++pivot;
        if (pivot == n)
            return false;
        if (pivot != col) {
            for (size_t j = col; j <= n; ++j) {
                number_t tmp = matrix[col][j];

                matrix[col][j] = matrix[pivot][j];
                matrix[pivot][j] = tmp;
            }
        }

        {
            number_t pivot_value = num_clone(matrix[col][col]);

            for (size_t j = col; j <= n; ++j) {
                number_t next = num_div(matrix[col][j], pivot_value);

                num_destroy(&matrix[col][j]);
                matrix[col][j] = next;
            }
            num_destroy(&pivot_value);
        }

        for (size_t row = 0; row < n; ++row) {
            if (row == col || num_eq(matrix[row][col], NUM_ZERO))
                continue;

            {
                number_t factor = num_clone(matrix[row][col]);

                for (size_t j = col; j <= n; ++j) {
                    number_t scaled = num_mul(factor, matrix[col][j]);
                    number_t next = num_sub(matrix[row][j], scaled);

                    num_destroy(&matrix[row][j]);
                    matrix[row][j] = next;
                    num_destroy(&scaled);
                }
                num_destroy(&factor);
            }
        }
    }

    for (size_t i = 0; i < n; ++i) {
        num_destroy(&solution[i]);
        solution[i] = num_clone(matrix[i][n]);
    }
    return true;
}

static expr_t *build_polynomial_antiderivative(const expr_t *wrt, const number_t *coeffs)
{
    number_t anti[5];
    expr_t *out;

    partial_fraction_array_zero(anti, 5);
    for (size_t i = 0; i < 4u; ++i) {
        number_t denom = num_create_from_long((long)(i + 1u));
        number_t next = num_div(coeffs[i], denom);

        num_destroy(&anti[i + 1u]);
        anti[i + 1u] = next;
        num_destroy(&denom);
    }
    out = build_polynomial_expr(wrt, anti, 5u);
    number_array_clear_local(anti, 5);
    return out;
}

static bool poly_match_direct_deg8(const expr_t *expr, const expr_t *wrt, number_t *coeffs, size_t *degree_out)
{
    if (!poly_match_direct_limited_rec(expr, wrt, coeffs, laurent_poly_coeff_count))
        return false;

    if (!poly_is_exact_real_local(coeffs, laurent_poly_coeff_count - 1u))
        goto fail;

    if (degree_out)
        *degree_out = poly_degree_local(coeffs, laurent_poly_coeff_count);
    return true;

fail:
    number_array_clear_local(coeffs, laurent_poly_coeff_count);
    return false;
}

static bool poly_match_direct_deg16(const expr_t *expr, const expr_t *wrt, number_t *coeffs, size_t *degree_out)
{
    if (!poly_match_direct_limited_rec(expr, wrt, coeffs, rational_power_poly_coeff_count))
        return false;

    if (!poly_is_exact_real_local(coeffs, rational_power_poly_coeff_count - 1u))
        goto fail;

    if (degree_out)
        *degree_out = poly_degree_local(coeffs, rational_power_poly_coeff_count);
    return true;

fail:
    number_array_clear_local(coeffs, rational_power_poly_coeff_count);
    return false;
}

static bool match_positive_polynomial_power_local(const expr_t *expr, const expr_t **base_out, size_t *power_out)
{
    number_t exponent = num_new();
    size_t power = 0u;
    bool ok = false;

    if (!expr || !base_out || !power_out)
        goto cleanup;

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW_D && expr->a && small_positive_int_from_number(expr->c, &power)) {
        *base_out = expr->a;
        *power_out = power;
        ok = true;
        goto cleanup;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW && expr->a && expr->b &&
        expr_match_const_value(expr->b, &exponent) && small_positive_int_from_number(exponent, &power)) {
        *base_out = expr->a;
        *power_out = power;
        ok = true;
    }

cleanup:
    num_destroy(&exponent);
    return ok;
}

static bool solve_rectangular_system_local(size_t row_count, size_t column_count,
                                           number_t matrix[rational_power_system_size][rational_power_system_size + 1u],
                                           number_t *solution)
{
    size_t pivot_columns[rational_power_system_size];
    size_t pivot_row = 0u;

    if (row_count == 0u || column_count == 0u || row_count > rational_power_system_size ||
        column_count > rational_power_system_size)
        return false;

    for (size_t column = 0u; column < column_count && pivot_row < row_count; ++column) {
        size_t pivot = pivot_row;

        while (pivot < row_count && num_eq(matrix[pivot][column], NUM_ZERO))
            ++pivot;
        if (pivot == row_count)
            continue;

        if (pivot != pivot_row) {
            for (size_t j = 0u; j <= column_count; ++j) {
                number_t tmp = matrix[pivot_row][j];

                matrix[pivot_row][j] = matrix[pivot][j];
                matrix[pivot][j] = tmp;
            }
        }

        {
            number_t pivot_value = num_clone(matrix[pivot_row][column]);

            for (size_t j = column; j <= column_count; ++j) {
                number_t next = num_div(matrix[pivot_row][j], pivot_value);

                num_destroy(&matrix[pivot_row][j]);
                matrix[pivot_row][j] = next;
            }
            num_destroy(&pivot_value);
        }

        for (size_t row = 0u; row < row_count; ++row) {
            number_t factor;

            if (row == pivot_row || num_eq(matrix[row][column], NUM_ZERO))
                continue;

            factor = num_clone(matrix[row][column]);
            for (size_t j = column; j <= column_count; ++j) {
                number_t scaled = num_mul(factor, matrix[pivot_row][j]);
                number_t next = num_sub(matrix[row][j], scaled);

                num_destroy(&matrix[row][j]);
                matrix[row][j] = next;
                num_destroy(&scaled);
            }
            num_destroy(&factor);
        }

        pivot_columns[pivot_row] = column;
        ++pivot_row;
    }

    for (size_t row = pivot_row; row < row_count; ++row) {
        bool all_zero = true;

        for (size_t column = 0u; column < column_count; ++column) {
            if (!num_eq(matrix[row][column], NUM_ZERO)) {
                all_zero = false;
                break;
            }
        }
        if (all_zero && !num_eq(matrix[row][column_count], NUM_ZERO))
            return false;
    }

    if (pivot_row != column_count)
        return false;

    for (size_t row = 0u; row < pivot_row; ++row) {
        size_t column = pivot_columns[row];

        num_destroy(&solution[column]);
        solution[column] = num_clone(matrix[row][column_count]);
    }
    return true;
}

static void rational_power_matrix_add_scaled_local(number_t *cell, number_t coefficient, long scale)
{
    number_t factor = num_create_from_long(scale);
    number_t scaled = num_mul(coefficient, factor);
    number_t next = num_add(*cell, scaled);

    num_destroy(cell);
    *cell = next;
    num_destroy(&scaled);
    num_destroy(&factor);
}

static expr_t *build_expanded_polynomial_local(const expr_t *wrt, const number_t *coefficients, size_t count)
{
    expr_t *sum = NULL;

    if (!wrt || !coefficients)
        return NULL;

    for (size_t power = count; power-- > 0u;) {
        expr_t *base = NULL;
        expr_t *term = NULL;

        if (num_eq(coefficients[power], NUM_ZERO))
            continue;
        if (power == 0u) {
            term = expr_new_const(coefficients[power]);
        } else {
            number_t exponent = num_create_from_long((long)power);

            base = power == 1u ? expr_retain_expr(wrt) : expr_pow(wrt, &exponent);
            if (base && num_eq(coefficients[power], NUM_ONE))
                term = expr_retain_expr(base);
            else if (base && num_eq(coefficients[power], NUM_NEG_ONE))
                term = expr_neg(base);
            else
                term = base ? expr_mul_num(base, &coefficients[power]) : NULL;
            num_destroy(&exponent);
        }
        expr_free(base);
        if (!term) {
            expr_free(sum);
            return NULL;
        }
        sum = expr_add_owned(sum, term);
        if (!sum)
            return NULL;
    }

    return sum ? sum : expr_new_const(NUM_ZERO);
}

/*
 * Recognise exact derivatives in Q-adic rational form.  For polynomial A and
 * Q,
 *
 *   d/dx (A / Q^(m-1))
 *       = (A'Q - (m-1)AQ') / Q^m.
 *
 * The coefficients of A are found by exact Gaussian elimination.  This is a
 * general rational-integration rule; no coefficients from a particular
 * integrand are embedded here.
 */
static expr_t *integrate_rational_power_exact_derivative(const expr_t *expr, const expr_t *wrt)
{
    number_t numerator[rational_power_poly_coeff_count];
    number_t denominator[rational_power_poly_coeff_count];
    number_t solution[rational_power_poly_coeff_count];
    number_t matrix[rational_power_system_size][rational_power_system_size + 1u];
    const expr_t *denominator_base = NULL;
    size_t denominator_power = 0u;
    size_t numerator_degree = 0u;
    size_t denominator_degree = 0u;
    size_t antiderivative_degree = 0u;
    size_t row_count = 0u;
    size_t column_count = 0u;
    expr_t *numerator_expr = NULL;
    expr_t *denominator_expr = NULL;
    expr_t *out = NULL;
    bool numerator_ready = false;
    bool denominator_ready = false;
    bool solution_ready = false;
    bool matrix_ready = false;

    if (!expr || !wrt || !expr->a || !expr->b ||
        !match_positive_polynomial_power_local(expr->b, &denominator_base, &denominator_power) ||
        denominator_power < 2u)
        goto cleanup;

    numerator_ready = poly_match_direct_deg16(expr->a, wrt, numerator, &numerator_degree);
    denominator_ready = poly_match_direct_deg16(denominator_base, wrt, denominator, &denominator_degree);
    if (!numerator_ready || !denominator_ready || denominator_degree < 3u ||
        denominator_power > SIZE_MAX / denominator_degree || numerator_degree >= denominator_power * denominator_degree)
        goto cleanup;

    antiderivative_degree = (denominator_power - 1u) * denominator_degree - 1u;
    column_count = antiderivative_degree + 1u;
    row_count = antiderivative_degree + denominator_degree;
    if (column_count > rational_power_poly_coeff_count || row_count > rational_power_system_size)
        goto cleanup;

    partial_fraction_array_zero(solution, column_count);
    solution_ready = true;
    for (size_t row = 0u; row < row_count; ++row)
        partial_fraction_array_zero(matrix[row], column_count + 1u);
    matrix_ready = true;

    for (size_t a_power = 0u; a_power <= antiderivative_degree; ++a_power) {
        if (a_power > 0u) {
            for (size_t q_power = 0u; q_power <= denominator_degree; ++q_power) {
                size_t row = a_power - 1u + q_power;

                rational_power_matrix_add_scaled_local(&matrix[row][a_power], denominator[q_power], (long)a_power);
            }
        }

        for (size_t q_power = 1u; q_power <= denominator_degree; ++q_power) {
            size_t row = a_power + q_power - 1u;
            long scale = -(long)(denominator_power - 1u) * (long)q_power;

            rational_power_matrix_add_scaled_local(&matrix[row][a_power], denominator[q_power], scale);
        }
    }

    for (size_t row = 0u; row < row_count; ++row) {
        num_destroy(&matrix[row][column_count]);
        matrix[row][column_count] = row <= numerator_degree ? num_clone(numerator[row]) : num_clone(NUM_ZERO);
    }

    if (!solve_rectangular_system_local(row_count, column_count, matrix, solution))
        goto cleanup;

    numerator_expr = build_expanded_polynomial_local(wrt, solution, column_count);
    if (denominator_power == 2u) {
        denominator_expr = expr_retain_expr(denominator_base);
    } else {
        number_t exponent = num_create_from_long((long)(denominator_power - 1u));

        denominator_expr = expr_pow(denominator_base, &exponent);
        num_destroy(&exponent);
    }
    out = (numerator_expr && denominator_expr) ? expr_div(numerator_expr, denominator_expr) : NULL;

cleanup:
    expr_free(denominator_expr);
    expr_free(numerator_expr);
    if (matrix_ready) {
        for (size_t row = 0u; row < row_count; ++row)
            number_array_clear_local(matrix[row], column_count + 1u);
    }
    if (solution_ready)
        number_array_clear_local(solution, column_count);
    if (denominator_ready)
        number_array_clear_local(denominator, rational_power_poly_coeff_count);
    if (numerator_ready)
        number_array_clear_local(numerator, rational_power_poly_coeff_count);
    return out;
}

static bool match_monomial_wrt_power_denominator_rec(const expr_t *expr, const expr_t *wrt, number_t *scale_io,
                                                     size_t *power_io)
{
    number_t constant = num_new();
    number_t exponent = num_new();
    bool ok = false;

    if (!expr || !wrt || !scale_io || !power_io)
        goto cleanup;

    if (expr_match_const_value(expr, &constant) && num_is_real(constant) && !num_eq(constant, NUM_ZERO)) {
        number_t next = num_mul(*scale_io, constant);

        num_destroy(scale_io);
        *scale_io = next;
        ok = true;
        goto cleanup;
    }

    if (same_wrt_var_local(expr, wrt)) {
        *power_io += 1u;
        ok = true;
        goto cleanup;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_MUL) {
        ok = match_monomial_wrt_power_denominator_rec(expr->a, wrt, scale_io, power_io) &&
             match_monomial_wrt_power_denominator_rec(expr->b, wrt, scale_io, power_io);
        goto cleanup;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW_D && expr->a && same_wrt_var_local(expr->a, wrt)) {
        size_t power = 0u;

        if (!small_positive_int_from_number(expr->c, &power))
            goto cleanup;
        *power_io += power;
        ok = true;
        goto cleanup;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW && expr->a && expr->b && same_wrt_var_local(expr->a, wrt) &&
        expr_match_const_value(expr->b, &exponent)) {
        size_t power = 0u;

        if (!small_positive_int_from_number(exponent, &power))
            goto cleanup;
        *power_io += power;
        ok = true;
        goto cleanup;
    }

cleanup:
    num_destroy(&exponent);
    num_destroy(&constant);
    return ok;
}

static bool match_monomial_wrt_power_denominator(const expr_t *expr, const expr_t *wrt, number_t *scale_out,
                                                 size_t *power_out)
{
    number_t scale = num_clone(NUM_ONE);
    size_t power = 0u;
    bool ok = match_monomial_wrt_power_denominator_rec(expr, wrt, &scale, &power) && power > 0u;

    if (ok) {
        num_destroy(scale_out);
        *scale_out = scale;
        *power_out = power;
    } else {
        num_destroy(&scale);
    }
    return ok;
}

static expr_t *build_laurent_integral_term(const expr_t *wrt, number_t coeff, long exponent)
{
    expr_t *base = NULL;
    expr_t *term = NULL;

    if (exponent == -1L) {
        base = expr_log(wrt);
        term = base ? expr_mul_num(base, &coeff) : NULL;
        expr_free(base);
        return term;
    }

    {
        long integrated_power = exponent + 1L;
        number_t denom = num_create_from_long(integrated_power);
        number_t scale = num_div(coeff, denom);

        if (integrated_power == 1L) {
            base = expr_retain_expr(wrt);
        } else {
            number_t power = num_create_from_long(integrated_power);

            base = expr_pow(wrt, &power);
            num_destroy(&power);
        }
        term = base ? expr_mul_num(base, &scale) : NULL;
        expr_free(base);
        num_destroy(&scale);
        num_destroy(&denom);
    }
    return term;
}

expr_t *integrate_polynomial_over_monomial_power(const expr_t *expr, const expr_t *wrt)
{
    number_t numerator[laurent_poly_coeff_count];
    number_t denom_scale = num_new();
    size_t numerator_degree = 0u;
    size_t denominator_power = 0u;
    expr_t *sum = NULL;
    bool numerator_ready = false;

    if (!expr || !wrt || !expr->a || !expr->b)
        goto cleanup;

    if (!poly_match_direct_deg8(expr->a, wrt, numerator, &numerator_degree)) {
        expr_t *scale = NULL;
        expr_t *reduced_numerator = NULL;
        expr_t *reduced_expr = NULL;
        expr_t *inner = NULL;

        if (!split_wrt_independent_product_factor(expr->a, wrt, &scale, &reduced_numerator)) {
            goto cleanup;
        }

        reduced_expr = reduced_numerator ? expr_div(reduced_numerator, expr->b) : NULL;
        inner = reduced_expr ? integrate_polynomial_over_monomial_power(reduced_expr, wrt) : NULL;
        sum = (scale && inner) ? expr_mul(scale, inner) : NULL;

        expr_free(inner);
        expr_free(reduced_expr);
        expr_free(reduced_numerator);
        expr_free(scale);
        goto cleanup;
    }
    numerator_ready = true;

    if (!match_monomial_wrt_power_denominator(expr->b, wrt, &denom_scale, &denominator_power) ||
        num_eq(denom_scale, NUM_ZERO)) {
        goto cleanup;
    }

    for (size_t i = 0u; i <= numerator_degree; ++i) {
        number_t coeff;
        expr_t *term = NULL;
        long exponent;

        if (num_eq(numerator[i], NUM_ZERO))
            continue;

        coeff = num_div(numerator[i], denom_scale);
        exponent = (long)i - (long)denominator_power;
        term = build_laurent_integral_term(wrt, coeff, exponent);
        num_destroy(&coeff);
        if (!term) {
            expr_free(sum);
            sum = NULL;
            goto cleanup;
        }
        sum = expr_add_owned(sum, term);
    }

cleanup:
    if (numerator_ready)
        number_array_clear_local(numerator, laurent_poly_coeff_count);
    num_destroy(&denom_scale);
    return simplify_owned(sum);
}

static expr_t *build_partial_fraction_antiderivative(const partial_fraction_factorization_t *factors, const expr_t *wrt,
                                                     const number_t *coeffs)
{
    expr_t *sum = NULL;
    size_t index = 0u;

    for (size_t i = 0; i < factors->count; ++i) {
        expr_t *u = NULL;

        for (size_t power = 1u; power <= factors->factors[i].multiplicity; ++power, ++index) {
            expr_t *term = NULL;
            number_t coeff;

            if (num_eq(coeffs[index], NUM_ZERO))
                continue;
            coeff = partial_fraction_normalize_small_rational(coeffs[index]);

            u = expr_add_num(wrt, &factors->factors[i].shift);
            if (!u) {
                num_destroy(&coeff);
                expr_free(sum);
                return NULL;
            }

            if (power == 1u) {
                expr_t *log_u = expr_log(u);

                term = log_u ? expr_mul_num(log_u, &coeff) : NULL;
                expr_free(log_u);
            } else {
                number_t exponent = num_create_from_long(1l - (long)power);
                number_t scale = num_div(coeff, exponent);
                expr_t *pow_u = expr_pow(u, &exponent);

                term = pow_u ? expr_mul_num(pow_u, &scale) : NULL;
                expr_free(pow_u);
                num_destroy(&scale);
                num_destroy(&exponent);
            }

            expr_free(u);
            num_destroy(&coeff);
            if (!term) {
                expr_free(sum);
                return NULL;
            }
            if (sum) {
                expr_t *next = expr_add(sum, term);

                expr_free(sum);
                expr_free(term);
                sum = next;
            } else {
                sum = term;
            }
        }
    }

    return simplify_owned(sum);
}

static bool add_integral_term_owned(expr_t **sum, expr_t *term)
{
    expr_t *next;

    if (!sum || !term) {
        expr_free(term);
        return false;
    }
    if (!*sum) {
        *sum = term;
        return true;
    }

    next = expr_add(*sum, term);
    expr_free(term);
    expr_free(*sum);
    *sum = next;
    return next != NULL;
}

static expr_t *build_exact_radical_scale(number_t rational_scale, number_t radical)
{
    number_t combined = num_mul(rational_scale, radical);
    number_t normalised = partial_fraction_normalize_small_rational(combined);
    long numerator;
    long denominator;
    expr_t *out;

    if (num_get_small_rational(normalised, &numerator, &denominator)) {
        out = expr_new_const(normalised);
    } else {
        expr_t *radical_expr = expr_new_const(radical);

        out = radical_expr ? expr_mul_num(radical_expr, &rational_scale) : NULL;
        expr_free(radical_expr);
    }

    num_destroy(&normalised);
    num_destroy(&combined);
    return out;
}

static expr_t *integrate_general_palindromic_quartic(const expr_t *expr, const expr_t *wrt)
{
    number_t numerator[5];
    number_t denominator[5];
    number_t two = num_create_from_long(2L);
    number_t four = num_create_from_long(4L);
    number_t middle = num_new();
    number_t quotient = num_new();
    number_t n0 = num_new();
    number_t n1 = num_new();
    number_t n2 = num_new();
    number_t n3 = num_new();
    number_t quotient_middle = num_new();
    number_t even_sum = num_new();
    number_t even_difference = num_new();
    number_t even_atan_scale = num_new();
    number_t even_log_scale = num_new();
    number_t odd_log_scale = num_new();
    number_t odd_remainder = num_new();
    number_t half_middle_n3 = num_new();
    number_t atan_width_squared = num_new();
    number_t factor_width_squared = num_new();
    number_t atan_width = num_new();
    number_t factor_width = num_new();
    number_t odd_width = num_new();
    size_t numerator_degree = 0u;
    size_t denominator_degree = 0u;
    expr_t *x_squared = NULL;
    expr_t *one = NULL;
    expr_t *term = NULL;
    expr_t *sum = NULL;
    bool numerator_ready = false;
    bool denominator_ready = false;

    if (!expr || !expr->a || !expr->b || !wrt)
        goto cleanup;

    numerator_ready = poly_match_direct_deg4(expr->a, wrt, numerator, &numerator_degree);
    denominator_ready = poly_match_direct_deg4(expr->b, wrt, denominator, &denominator_degree);
    if (!numerator_ready || !denominator_ready || numerator_degree > 4u || denominator_degree != 4u ||
        num_eq(denominator[4], NUM_ZERO) || !num_eq(denominator[0], denominator[4]) ||
        !num_eq(denominator[1], NUM_ZERO) || !num_eq(denominator[3], NUM_ZERO))
        goto cleanup;

    num_destroy(&middle);
    middle = num_div(denominator[2], denominator[4]);
    num_destroy(&quotient);
    quotient = num_div(numerator[4], denominator[4]);
    num_destroy(&quotient_middle);
    quotient_middle = num_mul(quotient, middle);

    num_destroy(&n0);
    n0 = num_div(numerator[0], denominator[4]);
    {
        number_t next = num_sub(n0, quotient);

        num_destroy(&n0);
        n0 = next;
    }
    num_destroy(&n1);
    n1 = num_div(numerator[1], denominator[4]);
    num_destroy(&n2);
    n2 = num_div(numerator[2], denominator[4]);
    {
        number_t next = num_sub(n2, quotient_middle);

        num_destroy(&n2);
        n2 = next;
    }
    num_destroy(&n3);
    n3 = num_div(numerator[3], denominator[4]);

    num_destroy(&atan_width_squared);
    atan_width_squared = num_add(two, middle);
    num_destroy(&factor_width_squared);
    factor_width_squared = num_sub(two, middle);
    if (!num_is_real(atan_width_squared) || !num_is_real(factor_width_squared) ||
        num_get_sign(atan_width_squared) <= 0 || num_get_sign(factor_width_squared) <= 0)
        goto cleanup;

    num_destroy(&atan_width);
    atan_width = num_sqrt(atan_width_squared);
    num_destroy(&factor_width);
    factor_width = num_sqrt(factor_width_squared);
    num_destroy(&odd_width);
    odd_width = num_mul(atan_width, factor_width);
    num_destroy(&even_sum);
    even_sum = num_add(n2, n0);
    num_destroy(&even_difference);
    even_difference = num_sub(n2, n0);
    num_destroy(&even_atan_scale);
    even_atan_scale = num_div(even_sum, two);
    num_destroy(&even_log_scale);
    {
        number_t denominator_scale = num_mul(four, factor_width_squared);

        even_log_scale = num_div(even_difference, denominator_scale);
        num_destroy(&denominator_scale);
    }
    num_destroy(&odd_log_scale);
    odd_log_scale = num_div(n3, four);
    num_destroy(&half_middle_n3);
    half_middle_n3 = num_mul(middle, n3);
    {
        number_t next = num_div(half_middle_n3, two);

        num_destroy(&half_middle_n3);
        half_middle_n3 = next;
    }
    num_destroy(&odd_remainder);
    odd_remainder = num_sub(n1, half_middle_n3);

    x_squared = expr_pow(wrt, &NUM_TWO);
    one = expr_new_const(NUM_ONE);
    if (!num_eq(quotient, NUM_ZERO)) {
        term = expr_mul_num(wrt, &quotient);
        if (!add_integral_term_owned(&sum, term))
            goto cleanup;
        term = NULL;
    }

    if (!num_eq(even_atan_scale, NUM_ZERO)) {
        expr_t *width_expr = expr_new_const(atan_width);
        expr_t *width_x = width_expr ? expr_mul(width_expr, wrt) : NULL;
        expr_t *denom = (one && x_squared) ? expr_sub(one, x_squared) : NULL;
        expr_t *argument = (width_x && denom) ? expr_div(width_x, denom) : NULL;
        expr_t *angle = argument ? expr_atan(argument) : NULL;
        number_t rational_scale = num_div(even_atan_scale, atan_width_squared);
        expr_t *exact_scale = build_exact_radical_scale(rational_scale, atan_width);

        term = (angle && exact_scale) ? expr_mul(exact_scale, angle) : NULL;
        expr_free(exact_scale);
        num_destroy(&rational_scale);
        expr_free(angle);
        expr_free(argument);
        expr_free(denom);
        expr_free(width_x);
        expr_free(width_expr);
        if (!add_integral_term_owned(&sum, term))
            goto cleanup;
        term = NULL;
    }

    if (!num_eq(even_log_scale, NUM_ZERO)) {
        expr_t *factor_expr = expr_new_const(factor_width);
        expr_t *factor_x = factor_expr ? expr_mul(factor_expr, wrt) : NULL;
        expr_t *minus = (x_squared && factor_x) ? expr_sub(x_squared, factor_x) : NULL;
        expr_t *plus = (x_squared && factor_x) ? expr_add(x_squared, factor_x) : NULL;
        expr_t *minus_one = minus ? expr_add_num(minus, &NUM_ONE) : NULL;
        expr_t *plus_one = plus ? expr_add_num(plus, &NUM_ONE) : NULL;
        expr_t *ratio = (minus_one && plus_one) ? expr_div(minus_one, plus_one) : NULL;
        expr_t *log_ratio = ratio ? expr_log(ratio) : NULL;

        expr_t *exact_scale = build_exact_radical_scale(even_log_scale, factor_width);

        term = (log_ratio && exact_scale) ? expr_mul(exact_scale, log_ratio) : NULL;
        expr_free(exact_scale);
        expr_free(log_ratio);
        expr_free(ratio);
        expr_free(plus_one);
        expr_free(minus_one);
        expr_free(plus);
        expr_free(minus);
        expr_free(factor_x);
        expr_free(factor_expr);
        if (!add_integral_term_owned(&sum, term))
            goto cleanup;
        term = NULL;
    }

    if (!num_eq(odd_log_scale, NUM_ZERO)) {
        expr_t *log_denominator = expr_log(expr->b);

        term = log_denominator ? expr_mul_num(log_denominator, &odd_log_scale) : NULL;
        expr_free(log_denominator);
        if (!add_integral_term_owned(&sum, term))
            goto cleanup;
        term = NULL;
    }

    if (!num_eq(odd_remainder, NUM_ZERO)) {
        expr_t *twice_x_squared = x_squared ? expr_mul_num(x_squared, &two) : NULL;
        expr_t *numerator_expr = twice_x_squared ? expr_add_num(twice_x_squared, &middle) : NULL;
        expr_t *width_expr = expr_new_const(odd_width);
        expr_t *argument = (numerator_expr && width_expr) ? expr_div(numerator_expr, width_expr) : NULL;
        expr_t *angle = argument ? expr_atan(argument) : NULL;
        number_t width_squared = num_mul(atan_width_squared, factor_width_squared);
        number_t odd_scale = num_div(odd_remainder, width_squared);
        expr_t *exact_scale = build_exact_radical_scale(odd_scale, odd_width);

        term = (angle && exact_scale) ? expr_mul(exact_scale, angle) : NULL;
        expr_free(exact_scale);
        num_destroy(&width_squared);
        num_destroy(&odd_scale);
        expr_free(angle);
        expr_free(argument);
        expr_free(width_expr);
        expr_free(numerator_expr);
        expr_free(twice_x_squared);
        if (!add_integral_term_owned(&sum, term))
            goto cleanup;
        term = NULL;
    }

    sum = simplify_owned(sum);

cleanup:
    expr_free(term);
    expr_free(one);
    expr_free(x_squared);
    num_destroy(&odd_width);
    num_destroy(&factor_width);
    num_destroy(&atan_width);
    num_destroy(&factor_width_squared);
    num_destroy(&atan_width_squared);
    num_destroy(&half_middle_n3);
    num_destroy(&odd_remainder);
    num_destroy(&odd_log_scale);
    num_destroy(&even_log_scale);
    num_destroy(&even_atan_scale);
    num_destroy(&even_difference);
    num_destroy(&even_sum);
    num_destroy(&quotient_middle);
    num_destroy(&n3);
    num_destroy(&n2);
    num_destroy(&n1);
    num_destroy(&n0);
    num_destroy(&quotient);
    num_destroy(&middle);
    num_destroy(&four);
    num_destroy(&two);
    if (denominator_ready)
        number_array_clear_local(denominator, 5u);
    if (numerator_ready)
        number_array_clear_local(numerator, 5u);
    return sum;
}

expr_t *integrate_rational_partial_fractions(const expr_t *expr, const expr_t *wrt)
{
    partial_fraction_factorization_t factors;
    number_t numerator[5];
    number_t denominator[5];
    number_t quotient[5];
    number_t remainder[5];
    number_t rhs[4];
    number_t solution[4];
    number_t matrix_storage[4][5];
    size_t num_degree = 0u;
    size_t den_degree = 0u;
    size_t unknown_count = 0u;
    expr_t *poly_part = NULL;
    expr_t *frac_part = NULL;
    expr_t *sum = NULL;
    bool numerator_ready = false;
    bool denominator_ready = false;
    bool quotient_ready = false;
    bool remainder_ready = false;
    bool ok = false;

    if (!expr || !expr->a || !expr->b)
        return NULL;

    sum = integrate_rational_power_exact_derivative(expr, wrt);
    if (sum)
        return sum;
    sum = integrate_general_palindromic_quartic(expr, wrt);
    if (sum)
        return sum;
    partial_fraction_factorization_init(&factors);
    partial_fraction_array_zero(rhs, 4);
    partial_fraction_array_zero(solution, 4);
    for (size_t row = 0; row < 4u; ++row)
        partial_fraction_array_zero(matrix_storage[row], 5);

    {
        bool num_ok = poly_match_direct_deg4(expr->a, wrt, numerator, &num_degree);
        bool factor_ok;
        bool total_ok;
        bool scale_ok;
        bool denom_ok;

        numerator_ready = num_ok;
        factor_ok = num_ok && partial_fraction_collect_linear_factors(expr->b, wrt, &factors);
        total_ok = factor_ok && factors.total_multiplicity > 0u && factors.total_multiplicity <= 4u;
        scale_ok = total_ok && !num_eq(factors.scale, NUM_ZERO);
        denom_ok = false;
        if (scale_ok) {
            denom_ok = build_normalized_denominator_poly(&factors, denominator);
            denominator_ready = true;
        }

        if (!denom_ok)
            goto cleanup;
    }
    for (size_t i = 0; i < factors.count; ++i) {
        if (factors.factors[i].multiplicity != 1u)
            goto cleanup;
    }
    den_degree = factors.total_multiplicity;
    if (num_degree >= den_degree)
        goto cleanup;
    if (!poly_divide_local(numerator, num_degree, denominator, den_degree, quotient, remainder))
        goto cleanup;
    quotient_ready = true;
    remainder_ready = true;

    poly_part = build_polynomial_antiderivative(wrt, quotient);
    if (!poly_part && !num_eq(quotient[0], NUM_ZERO))
        goto cleanup;

    for (size_t i = 0; i < den_degree; ++i) {
        number_t next = num_div(remainder[i], factors.scale);

        num_destroy(&rhs[i]);
        rhs[i] = next;
    }

    unknown_count = factors.total_multiplicity;
    {
        size_t col = 0u;

        for (size_t i = 0; i < factors.count; ++i) {
            for (size_t power = 1u; power <= factors.factors[i].multiplicity; ++power, ++col) {
                number_t basis[5];

                build_partial_fraction_basis_poly(&factors, i, power, basis);
                for (size_t row = 0; row < unknown_count; ++row) {
                    num_destroy(&matrix_storage[row][col]);
                    matrix_storage[row][col] = num_clone(basis[row]);
                }
                number_array_clear_local(basis, 5);
            }
        }
        for (size_t row = 0; row < unknown_count; ++row) {
            num_destroy(&matrix_storage[row][unknown_count]);
            matrix_storage[row][unknown_count] = num_clone(rhs[row]);
        }
    }

    if (!solve_linear_system_local(unknown_count, matrix_storage, solution))
        goto cleanup;

    frac_part = build_partial_fraction_antiderivative(&factors, wrt, solution);
    if (!frac_part && unknown_count > 0u)
        goto cleanup;

    if (poly_part && frac_part) {
        sum = expr_add(poly_part, frac_part);
    } else if (poly_part) {
        sum = poly_part;
        poly_part = NULL;
    } else if (frac_part) {
        sum = frac_part;
        frac_part = NULL;
    }

    ok = (sum != NULL);

cleanup:
    expr_free(frac_part);
    expr_free(poly_part);
    partial_fraction_factorization_clear(&factors);
    for (size_t row = 0; row < 4u; ++row)
        number_array_clear_local(matrix_storage[row], 5);
    number_array_clear_local(solution, 4);
    number_array_clear_local(rhs, 4);
    if (remainder_ready)
        number_array_clear_local(remainder, 5);
    if (quotient_ready)
        number_array_clear_local(quotient, 5);
    if (denominator_ready)
        number_array_clear_local(denominator, 5);
    if (numerator_ready)
        number_array_clear_local(numerator, 5);

    return ok ? simplify_owned(sum) : (expr_free(sum), NULL);
}
