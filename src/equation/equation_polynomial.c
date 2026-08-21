#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define MARS_EQUATION_INTERNAL_ACCESS
#include "equation_internal.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

static void equ_poly_init(number_t *poly, size_t count)
{
    for (size_t i = 0u; i < count; ++i)
        poly[i] = num_clone(NUM_ZERO);
}

static void equ_poly_destroy(number_t *poly, size_t count)
{
    for (size_t i = 0u; i < count; ++i)
        num_destroy(&poly[i]);
}

static number_t *equ_poly_new(size_t count)
{
    number_t *poly;

    if (count == 0u || count > SIZE_MAX / sizeof(*poly))
        return NULL;
    poly = malloc(count * sizeof(*poly));
    if (poly)
        equ_poly_init(poly, count);
    return poly;
}

static void equ_poly_free(number_t *poly, size_t count)
{
    if (!poly)
        return;
    equ_poly_destroy(poly, count);
    free(poly);
}

static void equ_poly_zero(number_t *poly, size_t count)
{
    for (size_t i = 0u; i < count; ++i) {
        num_destroy(&poly[i]);
        poly[i] = num_clone(NUM_ZERO);
    }
}

static void equ_poly_copy(const number_t *src, number_t *dst, size_t count)
{
    for (size_t i = 0u; i < count; ++i) {
        num_destroy(&dst[i]);
        dst[i] = num_clone(src[i]);
    }
}

static void equ_poly_canonicalize_real(number_t *poly, size_t count)
{
    for (size_t i = 0u; i < count; ++i) {
        number_t imaginary;

        imaginary = num_imag_part(poly[i]);
        if (num_is_zero(imaginary)) {
            number_t real = num_real_part(poly[i]);

            num_destroy(&poly[i]);
            poly[i] = real;
        }
        num_destroy(&imaginary);
    }
}

static bool equ_poly_scale(const number_t *src, number_t scale, number_t *dst, size_t count)
{
    if (!num_is_finite(scale))
        return false;

    for (size_t i = 0u; i < count; ++i) {
        number_t scaled = num_mul(src[i], scale);

        num_destroy(&dst[i]);
        dst[i] = scaled;
    }
    return true;
}

static bool equ_poly_add_sub(const number_t *left, const number_t *right, bool subtract, number_t *out, size_t count)
{
    for (size_t i = 0u; i < count; ++i) {
        number_t value = subtract ? num_sub(left[i], right[i]) : num_add(left[i], right[i]);

        num_destroy(&out[i]);
        out[i] = value;
    }
    return true;
}

static bool equ_poly_mul(const number_t *left, const number_t *right, number_t *out, size_t max_degree)
{
    size_t count = max_degree + 1u;
    size_t product_count;
    number_t *tmp;
    bool ok;

    if (max_degree > (SIZE_MAX - 1u) / 2u)
        return false;
    product_count = max_degree * 2u + 1u;
    tmp = equ_poly_new(product_count);
    if (!tmp)
        return false;

    for (size_t i = 0u; i < count; ++i) {
        for (size_t j = 0u; j < count; ++j) {
            number_t product = num_mul(left[i], right[j]);
            number_t next = num_add(tmp[i + j], product);

            num_destroy(&tmp[i + j]);
            tmp[i + j] = next;
            num_destroy(&product);
        }
    }

    ok = true;
    for (size_t i = count; i < product_count; ++i)
        ok = ok && num_is_zero(tmp[i]);
    if (ok)
        equ_poly_copy(tmp, out, count);

    equ_poly_free(tmp, product_count);
    return ok;
}

static bool equ_collect_poly(const expr_t *expr, const expr_t *wrt, size_t max_degree, number_t *out);

static bool equ_expr_numeric_parameter_value(const expr_t *expr, const expr_t *wrt, number_t *value_out)
{
    expr_t *vars[1];
    bool used = false;
    number_t value;

    if (!expr || !wrt || !value_out)
        return false;

    vars[0] = (expr_t *)wrt;
    if (!expr_collect_var_usage(expr, 1u, vars, &used) || used)
        return false;

    value = expr_eval(expr);
    if (!num_is_finite(value)) {
        num_destroy(&value);
        return false;
    }

    num_destroy(value_out);
    *value_out = value;
    return true;
}

static bool equ_collect_scaled_poly(const expr_t *expr, const expr_t *wrt, size_t max_degree, number_t *out)
{
    size_t count = max_degree + 1u;
    number_t scale = num_new();
    const expr_t *base = NULL;
    number_t *base_poly = NULL;
    bool ok = false;

    if (!expr_match_scaled_expr(expr, &scale, &base) || !base || base == expr)
        goto cleanup_scale;

    base_poly = equ_poly_new(count);
    if (!base_poly)
        goto cleanup_scale;
    ok = equ_collect_poly(base, wrt, max_degree, base_poly) && equ_poly_scale(base_poly, scale, out, count);

cleanup_scale:
    equ_poly_free(base_poly, count);
    num_destroy(&scale);
    return ok;
}

static long equ_power_exponent(number_t exponent, size_t max_degree)
{
    if (!num_is_integer(exponent) || !num_is_real(exponent))
        return -1L;

    if (max_degree > (size_t)LONG_MAX)
        return -1L;

    for (size_t i = 0u; i <= max_degree; ++i) {
        number_t candidate = num_create_from_long((long)i);
        bool ok = num_eq(exponent, candidate);

        num_destroy(&candidate);
        if (ok)
            return (long)i;
    }
    return -1L;
}

static bool equ_collect_power_poly(const expr_t *expr, const expr_t *wrt, size_t max_degree, number_t *out)
{
    size_t count = max_degree + 1u;
    number_t *base_poly = NULL;
    number_t *factor_poly = NULL;
    number_t *result_poly = NULL;
    const expr_t *base = NULL;
    number_t exponent_value = num_new();
    long exponent;
    bool ok = false;

    if (!expr_match_pow_const(expr, &base, &exponent_value))
        goto cleanup_exponent;

    exponent = equ_power_exponent(exponent_value, max_degree);
    if (exponent < 0L)
        goto cleanup_exponent;

    if (exponent == 0L) {
        equ_poly_zero(out, count);
        num_destroy(&out[0]);
        out[0] = num_clone(NUM_ONE);
        ok = true;
        goto cleanup_exponent;
    }
    if (exponent == 1L)
        ok = equ_collect_poly(base, wrt, max_degree, out);
    if (exponent == 1L)
        goto cleanup_exponent;

    base_poly = equ_poly_new(count);
    factor_poly = equ_poly_new(count);
    result_poly = equ_poly_new(count);
    if (!base_poly || !factor_poly || !result_poly)
        goto cleanup_polys;

    ok = equ_collect_poly(base, wrt, max_degree, base_poly);
    if (!ok)
        goto cleanup_polys;
    equ_poly_copy(base_poly, factor_poly, count);
    num_destroy(&result_poly[0]);
    result_poly[0] = num_clone(NUM_ONE);

    for (size_t power = (size_t)exponent; ok && power > 0u; power >>= 1u) {
        if ((power & 1u) != 0u)
            ok = equ_poly_mul(result_poly, factor_poly, result_poly, max_degree);
        if (ok && power > 1u)
            ok = equ_poly_mul(factor_poly, factor_poly, factor_poly, max_degree);
    }
    if (ok)
        equ_poly_copy(result_poly, out, count);

cleanup_polys:
    equ_poly_free(result_poly, count);
    equ_poly_free(factor_poly, count);
    equ_poly_free(base_poly, count);

cleanup_exponent:
    num_destroy(&exponent_value);
    return ok;
}

static bool equ_collect_poly(const expr_t *expr, const expr_t *wrt, size_t max_degree, number_t *out)
{
    size_t count = max_degree + 1u;
    expr_t *vars[1];
    size_t index = 0u;
    number_t value = num_new();
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    number_t *left_poly = NULL;
    number_t *right_poly = NULL;
    bool is_sub = false;
    bool ok = false;

    if (!expr || !wrt || !out)
        goto cleanup_value;

    if (expr_match_const_value(expr, &value)) {
        equ_poly_zero(out, count);
        num_destroy(&out[0]);
        out[0] = num_clone(value);
        ok = true;
        goto cleanup_value;
    }

    if (equ_expr_numeric_parameter_value(expr, wrt, &value)) {
        equ_poly_zero(out, count);
        num_destroy(&out[0]);
        out[0] = num_clone(value);
        ok = true;
        goto cleanup_value;
    }

    vars[0] = (expr_t *)wrt;
    if (max_degree >= 1u && expr_match_var_expr(expr, 1u, vars, &index) && index == 0u) {
        equ_poly_zero(out, count);
        num_destroy(&out[1]);
        out[1] = num_clone(NUM_ONE);
        ok = true;
        goto cleanup_value;
    }

    if (equ_collect_scaled_poly(expr, wrt, max_degree, out)) {
        ok = true;
        goto cleanup_value;
    }

    if (equ_collect_power_poly(expr, wrt, max_degree, out)) {
        ok = true;
        goto cleanup_value;
    }

    left_poly = equ_poly_new(count);
    right_poly = equ_poly_new(count);
    if (!left_poly || !right_poly)
        goto cleanup_polys;
    if (expr_match_add_sub_expr(expr, &left, &right, &is_sub)) {
        ok = equ_collect_poly(left, wrt, max_degree, left_poly) &&
             equ_collect_poly(right, wrt, max_degree, right_poly) &&
             equ_poly_add_sub(left_poly, right_poly, is_sub, out, count);
    } else if (expr_match_mul_expr(expr, &left, &right)) {
        ok = equ_collect_poly(left, wrt, max_degree, left_poly) &&
             equ_collect_poly(right, wrt, max_degree, right_poly) &&
             equ_poly_mul(left_poly, right_poly, out, max_degree);
    }
cleanup_polys:
    equ_poly_free(right_poly, count);
    equ_poly_free(left_poly, count);

cleanup_value:
    num_destroy(&value);
    return ok;
}

bool equ_match_polynomial_expr(const expr_t *expr, const expr_t *wrt, size_t max_degree, number_t *coeffs_out)
{
    size_t count = max_degree + 1u;
    number_t *coeffs;
    bool ok;

    if (!expr || !wrt || !coeffs_out || max_degree == SIZE_MAX)
        return false;

    coeffs = equ_poly_new(count);
    if (!coeffs)
        return false;
    ok = equ_collect_poly(expr, wrt, max_degree, coeffs);
    if (ok) {
        equ_poly_canonicalize_real(coeffs, count);
        equ_poly_copy(coeffs, coeffs_out, count);
    }
    equ_poly_free(coeffs, count);
    return ok;
}

static bool equ_polynomial_exponent(number_t value, size_t *exponent_out)
{
    double approximate;
    size_t exponent;
    number_t exact;
    bool ok;

    if (!exponent_out || !num_is_real(value) || !num_is_integer(value))
        return false;
    approximate = num_to_double(value);
    if (!isfinite(approximate) || approximate < 0.0 || approximate > (double)LONG_MAX)
        return false;

    exponent = (size_t)approximate;
    if ((double)exponent != approximate)
        return false;
    exact = num_create_from_long((long)exponent);
    ok = num_eq(value, exact);
    num_destroy(&exact);
    if (ok)
        *exponent_out = exponent;
    return ok;
}

static bool equ_polynomial_degree(const expr_t *expr, const expr_t *wrt, size_t *degree_out)
{
    expr_t *vars[1];
    size_t index = 0u;
    size_t left_degree;
    size_t right_degree;
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    const expr_t *base = NULL;
    number_t value = num_new();
    number_t scale = num_new();
    size_t exponent;
    bool is_sub = false;
    bool ok = false;

    if (!expr || !wrt || !degree_out)
        goto cleanup;

    if (expr_match_const_value(expr, &value) || equ_expr_numeric_parameter_value(expr, wrt, &value)) {
        *degree_out = 0u;
        ok = true;
        goto cleanup;
    }

    vars[0] = (expr_t *)wrt;
    if (expr_match_var_expr(expr, 1u, vars, &index) && index == 0u) {
        *degree_out = 1u;
        ok = true;
        goto cleanup;
    }

    if (expr_match_scaled_expr(expr, &scale, &base) && base && base != expr && num_is_finite(scale)) {
        ok = equ_polynomial_degree(base, wrt, degree_out);
        goto cleanup;
    }

    if (expr_match_pow_const(expr, &base, &value) && equ_polynomial_exponent(value, &exponent) &&
        equ_polynomial_degree(base, wrt, &left_degree) && (left_degree == 0u || exponent <= SIZE_MAX / left_degree)) {
        *degree_out = left_degree * exponent;
        ok = true;
        goto cleanup;
    }

    if (expr_match_add_sub_expr(expr, &left, &right, &is_sub) && equ_polynomial_degree(left, wrt, &left_degree) &&
        equ_polynomial_degree(right, wrt, &right_degree)) {
        (void)is_sub;
        *degree_out = left_degree > right_degree ? left_degree : right_degree;
        ok = true;
        goto cleanup;
    }

    if (expr_match_mul_expr(expr, &left, &right) && equ_polynomial_degree(left, wrt, &left_degree) &&
        equ_polynomial_degree(right, wrt, &right_degree) && left_degree <= SIZE_MAX - right_degree) {
        *degree_out = left_degree + right_degree;
        ok = true;
    }

cleanup:
    num_destroy(&scale);
    num_destroy(&value);
    return ok;
}

bool equ_match_polynomial_alloc(const expr_t *expr, const expr_t *wrt, number_t **coeffs_out, size_t *degree_out)
{
    number_t *coeffs = NULL;
    number_t *trimmed = NULL;
    size_t degree;
    size_t allocated_degree;
    size_t count;

    if (!coeffs_out || !degree_out || !equ_polynomial_degree(expr, wrt, &degree) || degree == SIZE_MAX)
        return false;

    allocated_degree = degree;
    count = allocated_degree + 1u;
    coeffs = equ_poly_new(count);
    if (!coeffs)
        return false;
    if (!equ_collect_poly(expr, wrt, degree, coeffs)) {
        equ_poly_free(coeffs, count);
        return false;
    }
    equ_poly_canonicalize_real(coeffs, count);

    while (degree > 0u && num_is_zero(coeffs[degree]))
        --degree;
    if (degree != allocated_degree) {
        trimmed = equ_poly_new(degree + 1u);
        if (!trimmed) {
            equ_poly_free(coeffs, count);
            return false;
        }
        equ_poly_copy(coeffs, trimmed, degree + 1u);
        equ_poly_free(coeffs, count);
        coeffs = trimmed;
    }
    *coeffs_out = coeffs;
    *degree_out = degree;
    return true;
}

static expr_t *equ_polynomial_expression_from_coefficients(const number_t *coeffs, size_t degree, const expr_t *wrt)
{
    expr_t *sum = NULL;

    if (!coeffs || !wrt)
        return NULL;

    for (size_t power = degree + 1u; power-- > 0u;) {
        expr_t *base = NULL;
        expr_t *term = NULL;
        expr_t *next;

        if (num_is_zero(coeffs[power]))
            continue;

        if (power == 0u) {
            term = expr_new_const(coeffs[power]);
        } else {
            if (power == 1u) {
                base = expr_clone(wrt);
            } else {
                number_t exponent = num_create_from_long((long)power);

                base = expr_pow(wrt, &exponent);
                num_destroy(&exponent);
            }
            if (!base)
                goto fail;

            if (num_is_one(coeffs[power])) {
                term = base;
                base = NULL;
            } else if (num_eq(coeffs[power], NUM_NEG_ONE)) {
                term = expr_neg(base);
            } else {
                expr_t *coefficient = expr_new_const(coeffs[power]);

                term = coefficient ? expr_mul(coefficient, base) : NULL;
                expr_free(coefficient);
            }
            expr_free(base);
        }

        if (!term)
            goto fail;
        if (!sum) {
            sum = term;
            continue;
        }

        next = expr_add(sum, term);
        expr_free(term);
        expr_free(sum);
        if (!next)
            return NULL;
        sum = next;
    }

    return sum ? sum : expr_new_const(NUM_ZERO);

fail:
    expr_free(sum);
    return NULL;
}

static expr_t *equ_display_expanded_side(const expr_t *expr, const expr_t *wrt)
{
    number_t *coeffs = NULL;
    size_t degree = 0u;
    expr_t *expanded = NULL;

    if (expr && wrt && equ_match_polynomial_alloc(expr, wrt, &coeffs, &degree)) {
        expanded = equ_polynomial_expression_from_coefficients(coeffs, degree, wrt);
    }

    if (coeffs) {
        equ_poly_destroy(coeffs, degree + 1u);
        free(coeffs);
    }
    return expanded ? expanded : expr_display_expanded(expr);
}

equation_t *equ_display_expanded(const equation_t *equation, const expr_t *wrt)
{
    expr_t *lhs;
    expr_t *rhs;
    equation_t *expanded;

    if (!equation)
        return NULL;

    lhs = equ_display_expanded_side(equ_lhs(equation), wrt);
    rhs = equ_display_expanded_side(equ_rhs(equation), wrt);
    if (!lhs || !rhs) {
        expr_free(rhs);
        expr_free(lhs);
        return NULL;
    }

    expanded = equ_new(lhs, rhs);
    if (expanded && equ_set_display_TeX(expanded, equ_lhs_display_TeX(equation), equ_rhs_display_TeX(equation)) != 0) {
        equ_free(expanded);
        expanded = NULL;
    }
    expr_free(rhs);
    expr_free(lhs);
    return expanded;
}
