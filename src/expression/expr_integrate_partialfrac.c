#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "expr_integrate_internal.h"
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
    if (expr == wrt)
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

static bool poly_match_affine_basis_deg4(const expr_t *expr,
                                         const expr_t *wrt,
                                         number_t *coeffs,
                                         size_t *degree_out)
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
            { 1, 0, 0, 0, 0 },
            { 1, 1, 0, 0, 0 },
            { 1, 2, 1, 0, 0 },
            { 1, 3, 3, 1, 0 },
            { 1, 4, 6, 4, 1 },
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

static size_t poly_degree_local(const number_t *coeffs, size_t count)
{
    for (size_t i = count; i-- > 0u;) {
        if (!num_eq(coeffs[i], NUM_ZERO))
            return i;
    }
    return 0u;
}

static bool poly_match_direct_deg4_rec(const expr_t *expr,
                                       const expr_t *wrt,
                                       number_t *coeffs)
{
    number_t constant = num_new();
    number_t exponent = num_new();
    number_t lhs[5];
    number_t rhs[5];
    bool lhs_ready = false;
    bool rhs_ready = false;
    bool ok = false;

    partial_fraction_array_zero(coeffs, 5);

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
        if (!poly_match_direct_deg4_rec(expr->a, wrt, lhs))
            goto cleanup;
        lhs_ready = true;
        for (size_t i = 0; i < 5u; ++i) {
            num_destroy(&coeffs[i]);
            coeffs[i] = num_neg(lhs[i]);
        }
        ok = true;
        goto cleanup;
    }

    if (expr->ops &&
        (expr->ops->kind == EXPR_KIND_ADD || expr->ops->kind == EXPR_KIND_SUB)) {
        if (!poly_match_direct_deg4_rec(expr->a, wrt, lhs)) {
            goto cleanup;
        }
        lhs_ready = true;
        if (!poly_match_direct_deg4_rec(expr->b, wrt, rhs)) {
            goto cleanup;
        }
        rhs_ready = true;
        for (size_t i = 0; i < 5u; ++i) {
            number_t next = (expr->ops->kind == EXPR_KIND_ADD)
                                ? num_add(lhs[i], rhs[i])
                                : num_sub(lhs[i], rhs[i]);

            num_destroy(&coeffs[i]);
            coeffs[i] = next;
        }
        ok = true;
        goto cleanup;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_MUL) {
        if (!poly_match_direct_deg4_rec(expr->a, wrt, lhs)) {
            goto cleanup;
        }
        lhs_ready = true;
        if (!poly_match_direct_deg4_rec(expr->b, wrt, rhs)) {
            goto cleanup;
        }
        rhs_ready = true;
        for (size_t i = 0; i < 5u; ++i) {
            for (size_t j = 0; i + j < 5u; ++j) {
                number_t term = num_mul(lhs[i], rhs[j]);
                number_t next = num_add(coeffs[i + j], term);

                num_destroy(&coeffs[i + j]);
                coeffs[i + j] = next;
                num_destroy(&term);
            }
        }
        if (poly_degree_local(lhs, 5u) + poly_degree_local(rhs, 5u) > 4u)
            goto cleanup;
        ok = true;
        goto cleanup;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_DIV &&
        expr_match_const_value(expr->b, &constant) &&
        num_is_real(constant) &&
        !num_eq(constant, NUM_ZERO)) {
        if (!poly_match_direct_deg4_rec(expr->a, wrt, lhs))
            goto cleanup;
        lhs_ready = true;
        for (size_t i = 0; i < 5u; ++i) {
            number_t next = num_div(lhs[i], constant);

            num_destroy(&coeffs[i]);
            coeffs[i] = next;
        }
        ok = true;
        goto cleanup;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW_D) {
        size_t power = 0u;

        if (!small_positive_int_from_number(expr->c, &power) || power > 4u ||
            !poly_match_direct_deg4_rec(expr->a, wrt, lhs)) {
            goto cleanup;
        }
        lhs_ready = true;

        num_destroy(&coeffs[0]);
        coeffs[0] = num_clone(NUM_ONE);
        for (size_t iter = 0; iter < power; ++iter) {
            number_t next[5];

            partial_fraction_array_zero(next, 5);
            for (size_t i = 0; i < 5u; ++i) {
                for (size_t j = 0; i + j < 5u; ++j) {
                    number_t term = num_mul(coeffs[i], lhs[j]);
                    number_t sum = num_add(next[i + j], term);

                    num_destroy(&next[i + j]);
                    next[i + j] = sum;
                    num_destroy(&term);
                }
            }
            if (poly_degree_local(coeffs, 5u) + poly_degree_local(lhs, 5u) > 4u) {
                number_array_clear_local(next, 5);
                goto cleanup;
            }
            poly_copy_local(coeffs, next, 5u);
            number_array_clear_local(next, 5);
        }
        ok = true;
        goto cleanup;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW &&
        expr->b && expr_match_const_value(expr->b, &exponent)) {
        size_t power = 0u;

        if (!small_positive_int_from_number(exponent, &power) || power > 4u ||
            !poly_match_direct_deg4_rec(expr->a, wrt, lhs)) {
            goto cleanup;
        }
        lhs_ready = true;

        num_destroy(&coeffs[0]);
        coeffs[0] = num_clone(NUM_ONE);
        for (size_t iter = 0; iter < power; ++iter) {
            number_t next[5];

            partial_fraction_array_zero(next, 5);
            for (size_t i = 0; i < 5u; ++i) {
                for (size_t j = 0; i + j < 5u; ++j) {
                    number_t term = num_mul(coeffs[i], lhs[j]);
                    number_t sum = num_add(next[i + j], term);

                    num_destroy(&next[i + j]);
                    next[i + j] = sum;
                    num_destroy(&term);
                }
            }
            if (poly_degree_local(coeffs, 5u) + poly_degree_local(lhs, 5u) > 4u) {
                number_array_clear_local(next, 5);
                goto cleanup;
            }
            poly_copy_local(coeffs, next, 5u);
            number_array_clear_local(next, 5);
        }
        ok = true;
        goto cleanup;
    }

cleanup:
    if (rhs_ready)
        number_array_clear_local(rhs, 5);
    if (lhs_ready)
        number_array_clear_local(lhs, 5);
    num_destroy(&exponent);
    num_destroy(&constant);
    if (!ok)
        number_array_clear_local(coeffs, 5);
    return ok;
}

static bool poly_match_direct_deg4(const expr_t *expr,
                                   const expr_t *wrt,
                                   number_t *coeffs,
                                   size_t *degree_out)
{
    if (!poly_match_direct_deg4_rec(expr, wrt, coeffs)) {
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

static bool partial_fraction_append_shift(partial_fraction_factorization_t *out,
                                          number_t shift,
                                          size_t multiplicity)
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

static bool partial_fraction_factor_poly_coeffs(const number_t *coeffs,
                                                size_t degree,
                                                partial_fraction_factorization_t *out)
{
    if (degree == 0u) {
        return partial_fraction_scale_mul(out, coeffs[0], 1u);
    }

    if (degree == 1u) {
        number_t shift = num_div(coeffs[0], coeffs[1]);
        bool ok = partial_fraction_scale_mul(out, coeffs[1], 1u) &&
                  partial_fraction_append_shift(out, shift, 1u);

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
        bool exact_square = num_is_real(sqrt_disc) &&
                            num_eq(sqrt_disc_sq, disc);

        if (exact_square) {
            number_t root1_numer = num_add(minus_b, sqrt_disc);
            number_t root2_numer = num_sub(minus_b, sqrt_disc);
            number_t root1 = num_div(root1_numer, two_a);
            number_t root2 = num_div(root2_numer, two_a);
            number_t shift1 = num_neg(root1);
            number_t shift2 = num_neg(root2);
            bool ok = partial_fraction_scale_mul(out, coeffs[2], 1u) &&
                      partial_fraction_append_shift(out, shift1, 1u) &&
                      partial_fraction_append_shift(out, shift2, 1u);

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

static bool partial_fraction_collect_linear_factors(const expr_t *expr,
                                                    const expr_t *wrt,
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
                if (!partial_fraction_append_shift(out, base.factors[i].shift,
                                                   base.factors[i].multiplicity * degree)) {
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
        expr_match_const_value(expr->b, &exponent) &&
        small_positive_int_from_number(exponent, &degree)) {
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
                if (!partial_fraction_append_shift(out, base.factors[i].shift,
                                                   base.factors[i].multiplicity * degree)) {
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

    if (match_nonconstant_affine_linear_expr(expr, wrt, &constant, &coeff) &&
        num_is_real(constant) &&
        num_is_real(coeff) &&
        !num_eq(coeff, NUM_ZERO)) {
        number_t shift = num_div(constant, coeff);

        ok = partial_fraction_scale_mul(out, coeff, 1u) &&
             partial_fraction_append_shift(out, shift, 1u);
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

static bool build_normalized_denominator_poly(const partial_fraction_factorization_t *factors,
                                              number_t *coeffs)
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

static void build_partial_fraction_basis_poly(const partial_fraction_factorization_t *factors,
                                              size_t factor_index,
                                              size_t power,
                                              number_t *coeffs)
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

static bool poly_divide_local(const number_t *numerator,
                              size_t num_degree,
                              const number_t *denominator,
                              size_t den_degree,
                              number_t *quotient,
                              number_t *remainder)
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

static expr_t *build_partial_fraction_antiderivative(const partial_fraction_factorization_t *factors,
                                                     const expr_t *wrt,
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
