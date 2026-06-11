#include <stdbool.h>
#include <stddef.h>

#include "equation_internal.h"
#include "expression/expr_internal.h"

enum {
    EQUATION_POLY_MAX_DEGREE = 3u,
    EQUATION_POLY_MAX_COEFFS = EQUATION_POLY_MAX_DEGREE + 1u,
    EQUATION_POLY_MAX_PRODUCT_COEFFS = 2u * EQUATION_POLY_MAX_DEGREE + 1u
};

static void equation_poly_init(number_t *poly, size_t count)
{
    for (size_t i = 0u; i < count; ++i)
        poly[i] = num_new();
}

static void equation_poly_destroy(number_t *poly, size_t count)
{
    for (size_t i = 0u; i < count; ++i)
        num_destroy(&poly[i]);
}

static void equation_poly_zero(number_t *poly, size_t count)
{
    for (size_t i = 0u; i < count; ++i) {
        num_destroy(&poly[i]);
        poly[i] = num_new();
    }
}

static void equation_poly_copy(const number_t *src, number_t *dst, size_t count)
{
    for (size_t i = 0u; i < count; ++i) {
        num_destroy(&dst[i]);
        dst[i] = num_clone(src[i]);
    }
}

static bool equation_poly_scale(const number_t *src,
                                number_t scale,
                                number_t *dst,
                                size_t count)
{
    if (!num_is_real(scale))
        return false;

    for (size_t i = 0u; i < count; ++i) {
        number_t scaled = num_mul(src[i], scale);

        num_destroy(&dst[i]);
        dst[i] = scaled;
    }
    return true;
}

static bool equation_poly_add_sub(const number_t *left,
                                  const number_t *right,
                                  bool subtract,
                                  number_t *out,
                                  size_t count)
{
    for (size_t i = 0u; i < count; ++i) {
        number_t value = subtract ? num_sub(left[i], right[i])
                                  : num_add(left[i], right[i]);

        num_destroy(&out[i]);
        out[i] = value;
    }
    return true;
}

static bool equation_poly_mul(const number_t *left,
                              const number_t *right,
                              number_t *out,
                              size_t max_degree)
{
    size_t count = max_degree + 1u;
    size_t product_count = max_degree * 2u + 1u;
    number_t tmp[EQUATION_POLY_MAX_PRODUCT_COEFFS];
    bool ok;

    for (size_t i = 0u; i < product_count; ++i)
        tmp[i] = num_new();

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
        equation_poly_copy(tmp, out, count);

    equation_poly_destroy(tmp, product_count);
    return ok;
}

static bool equation_collect_poly(const expr_t *expr,
                                  const expr_t *wrt,
                                  size_t max_degree,
                                  number_t *out);

static bool equation_expr_numeric_parameter_value(const expr_t *expr,
                                                  const expr_t *wrt,
                                                  number_t *value_out)
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

static bool equation_collect_scaled_poly(const expr_t *expr,
                                         const expr_t *wrt,
                                         size_t max_degree,
                                         number_t *out)
{
    size_t count = max_degree + 1u;
    number_t scale = num_new();
    const expr_t *base = NULL;
    number_t base_poly[EQUATION_POLY_MAX_COEFFS];
    bool ok = false;

    if (!expr_match_scaled_expr(expr, &scale, &base) || !base || base == expr)
        goto cleanup_scale;

    equation_poly_init(base_poly, count);
    ok = equation_collect_poly(base, wrt, max_degree, base_poly) &&
         equation_poly_scale(base_poly, scale, out, count);
    equation_poly_destroy(base_poly, count);

cleanup_scale:
    num_destroy(&scale);
    return ok;
}

static long equation_power_exponent(number_t exponent, size_t max_degree)
{
    if (!num_is_integer(exponent) || !num_is_real(exponent))
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

static bool equation_collect_power_poly(const expr_t *expr,
                                        const expr_t *wrt,
                                        size_t max_degree,
                                        number_t *out)
{
    size_t count = max_degree + 1u;
    number_t base_poly[EQUATION_POLY_MAX_COEFFS];
    number_t square_poly[EQUATION_POLY_MAX_COEFFS];
    long exponent;
    bool ok = false;

    if (!expr || !expr->ops || expr->ops->kind != EXPR_KIND_POW_D)
        return false;

    exponent = equation_power_exponent(expr->c, max_degree);
    if (exponent < 0L)
        return false;

    if (exponent == 0L) {
        equation_poly_zero(out, count);
        num_destroy(&out[0]);
        out[0] = num_clone(NUM_ONE);
        return true;
    }
    if (exponent == 1L)
        return equation_collect_poly(expr->a, wrt, max_degree, out);

    equation_poly_init(base_poly, count);
    equation_poly_init(square_poly, count);
    ok = equation_collect_poly(expr->a, wrt, max_degree, base_poly) &&
         equation_poly_mul(base_poly, base_poly, square_poly, max_degree) &&
         (exponent == 2L ||
          equation_poly_mul(square_poly, base_poly, out, max_degree));
    if (ok && exponent == 2L)
        equation_poly_copy(square_poly, out, count);
    equation_poly_destroy(square_poly, count);
    equation_poly_destroy(base_poly, count);
    return ok;
}

static bool equation_collect_poly(const expr_t *expr,
                                  const expr_t *wrt,
                                  size_t max_degree,
                                  number_t *out)
{
    size_t count = max_degree + 1u;
    expr_t *vars[1];
    size_t index = 0u;
    number_t value = num_new();
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    number_t left_poly[EQUATION_POLY_MAX_COEFFS];
    number_t right_poly[EQUATION_POLY_MAX_COEFFS];
    bool is_sub = false;
    bool ok = false;

    if (!expr || !wrt || !out)
        goto cleanup_value;

    if (expr_match_const_value(expr, &value)) {
        equation_poly_zero(out, count);
        num_destroy(&out[0]);
        out[0] = num_clone(value);
        ok = true;
        goto cleanup_value;
    }

    if (equation_expr_numeric_parameter_value(expr, wrt, &value)) {
        equation_poly_zero(out, count);
        num_destroy(&out[0]);
        out[0] = num_clone(value);
        ok = true;
        goto cleanup_value;
    }

    vars[0] = (expr_t *)wrt;
    if (max_degree >= 1u &&
        expr_match_var_expr(expr, 1u, vars, &index) && index == 0u) {
        equation_poly_zero(out, count);
        num_destroy(&out[1]);
        out[1] = num_clone(NUM_ONE);
        ok = true;
        goto cleanup_value;
    }

    if (equation_collect_scaled_poly(expr, wrt, max_degree, out)) {
        ok = true;
        goto cleanup_value;
    }

    if (equation_collect_power_poly(expr, wrt, max_degree, out)) {
        ok = true;
        goto cleanup_value;
    }

    equation_poly_init(left_poly, count);
    equation_poly_init(right_poly, count);
    if (expr_match_add_sub_expr(expr, &left, &right, &is_sub)) {
        ok = equation_collect_poly(left, wrt, max_degree, left_poly) &&
             equation_collect_poly(right, wrt, max_degree, right_poly) &&
             equation_poly_add_sub(left_poly, right_poly, is_sub, out, count);
    } else if (expr_match_mul_expr(expr, &left, &right)) {
        ok = equation_collect_poly(left, wrt, max_degree, left_poly) &&
             equation_collect_poly(right, wrt, max_degree, right_poly) &&
             equation_poly_mul(left_poly, right_poly, out, max_degree);
    }
    equation_poly_destroy(right_poly, count);
    equation_poly_destroy(left_poly, count);

cleanup_value:
    num_destroy(&value);
    return ok;
}

bool equation_match_polynomial_expr(const expr_t *expr,
                                    const expr_t *wrt,
                                    size_t max_degree,
                                    number_t *coeffs_out)
{
    size_t count = max_degree + 1u;
    number_t coeffs[EQUATION_POLY_MAX_COEFFS];
    bool ok;

    if (!expr || !wrt || !coeffs_out || max_degree > EQUATION_POLY_MAX_DEGREE)
        return false;

    equation_poly_init(coeffs, count);
    ok = equation_collect_poly(expr, wrt, max_degree, coeffs);
    if (ok)
        equation_poly_copy(coeffs, coeffs_out, count);
    equation_poly_destroy(coeffs, count);
    return ok;
}
