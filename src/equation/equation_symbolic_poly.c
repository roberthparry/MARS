#include <stdbool.h>
#include <stddef.h>

#include "equation_internal.h"
#include "expression/expr_internal.h"

enum {
    EQUATION_SYMBOLIC_POLY_MAX_DEGREE = 3u,
    EQUATION_SYMBOLIC_POLY_COEFFS = EQUATION_SYMBOLIC_POLY_MAX_DEGREE + 1u
};

typedef struct equation_symbolic_poly {
    expr_t *coeff[EQUATION_SYMBOLIC_POLY_COEFFS];
} equation_symbolic_poly_t;

static expr_t *equation_const_long_expr(long value)
{
    number_t number = num_create_from_long(value);
    expr_t *expr = expr_new_const(number);

    num_destroy(&number);
    return expr;
}

static expr_t *equation_retain_expr(const expr_t *expr)
{
    if (!expr)
        return NULL;

    expr_retain(expr);
    return (expr_t *)expr;
}

static expr_t *equation_simplify_expr_owned(expr_t *expr)
{
    expr_t *simplified;

    if (!expr)
        return NULL;

    simplified = expr_simplify(expr);
    expr_free(expr);
    return simplified;
}

static void equation_symbolic_poly_clear(equation_symbolic_poly_t *poly)
{
    if (!poly)
        return;

    for (size_t i = 0u; i < EQUATION_SYMBOLIC_POLY_COEFFS; ++i) {
        expr_free(poly->coeff[i]);
        poly->coeff[i] = NULL;
    }
}

static bool equation_symbolic_coeff_is_zero(const expr_t *expr)
{
    number_t value = num_new();
    bool zero = false;

    if (!expr)
        return true;

    if (expr_match_const_value(expr, &value))
        zero = num_is_zero(value);
    num_destroy(&value);
    return zero;
}

static bool equation_symbolic_add_owned(equation_symbolic_poly_t *poly,
                                        size_t degree,
                                        expr_t *term)
{
    expr_t *sum;

    if (!poly || degree >= EQUATION_SYMBOLIC_POLY_COEFFS) {
        expr_free(term);
        return false;
    }
    if (!term)
        return false;

    if (!poly->coeff[degree]) {
        poly->coeff[degree] = equation_simplify_expr_owned(term);
        return poly->coeff[degree] != NULL;
    }

    sum = expr_add(poly->coeff[degree], term);
    expr_free(term);
    expr_free(poly->coeff[degree]);
    poly->coeff[degree] = equation_simplify_expr_owned(sum);
    return poly->coeff[degree] != NULL;
}

static bool equation_symbolic_add_const(equation_symbolic_poly_t *poly,
                                        size_t degree,
                                        long value)
{
    return equation_symbolic_add_owned(poly, degree,
                                       equation_const_long_expr(value));
}

static bool equation_symbolic_copy_into(equation_symbolic_poly_t *out,
                                        const equation_symbolic_poly_t *in)
{
    for (size_t i = 0u; i < EQUATION_SYMBOLIC_POLY_COEFFS; ++i) {
        if (!in->coeff[i])
            continue;
        if (!equation_symbolic_add_owned(out, i,
                                         equation_retain_expr(in->coeff[i])))
            return false;
    }

    return true;
}

static bool equation_expr_depends_on_wrt(const expr_t *expr, const expr_t *wrt)
{
    expr_t *vars[1];
    bool used = false;

    if (!expr || !wrt)
        return false;

    vars[0] = (expr_t *)wrt;
    return expr_collect_var_usage(expr, 1u, vars, &used) && used;
}

static bool equation_expr_is_wrt(const expr_t *expr, const expr_t *wrt)
{
    expr_t *vars[1];
    size_t index = 0u;

    if (!expr || !wrt)
        return false;

    vars[0] = (expr_t *)wrt;
    return expr_match_var_expr(expr, 1u, vars, &index) && index == 0u;
}

static bool equation_symbolic_poly_collect(const expr_t *expr,
                                           const expr_t *wrt,
                                           equation_symbolic_poly_t *poly);

static bool equation_symbolic_poly_multiply(
    const equation_symbolic_poly_t *left,
    const equation_symbolic_poly_t *right,
    equation_symbolic_poly_t *out)
{
    for (size_t i = 0u; i < EQUATION_SYMBOLIC_POLY_COEFFS; ++i) {
        if (!left->coeff[i])
            continue;
        for (size_t j = 0u; j < EQUATION_SYMBOLIC_POLY_COEFFS; ++j) {
            expr_t *term;

            if (!right->coeff[j])
                continue;
            if (i + j >= EQUATION_SYMBOLIC_POLY_COEFFS)
                return false;

            term = expr_mul(left->coeff[i], right->coeff[j]);
            if (!equation_symbolic_add_owned(out, i + j, term))
                return false;
        }
    }

    return true;
}

static bool equation_symbolic_poly_collect_neg(const expr_t *arg,
                                               const expr_t *wrt,
                                               equation_symbolic_poly_t *poly)
{
    equation_symbolic_poly_t inner = { 0 };
    bool ok = false;

    if (!equation_symbolic_poly_collect(arg, wrt, &inner))
        goto cleanup;

    for (size_t i = 0u; i < EQUATION_SYMBOLIC_POLY_COEFFS; ++i) {
        expr_t *negated;

        if (!inner.coeff[i])
            continue;
        negated = expr_neg(inner.coeff[i]);
        if (!equation_symbolic_add_owned(poly, i, negated))
            goto cleanup;
    }
    ok = true;

cleanup:
    equation_symbolic_poly_clear(&inner);
    return ok;
}

static bool equation_symbolic_poly_collect_sub(const expr_t *left,
                                               const expr_t *right,
                                               const expr_t *wrt,
                                               equation_symbolic_poly_t *poly)
{
    equation_symbolic_poly_t right_poly = { 0 };
    bool ok = false;

    if (!equation_symbolic_poly_collect(left, wrt, poly) ||
        !equation_symbolic_poly_collect(right, wrt, &right_poly))
        goto cleanup;

    for (size_t i = 0u; i < EQUATION_SYMBOLIC_POLY_COEFFS; ++i) {
        expr_t *negated;

        if (!right_poly.coeff[i])
            continue;
        negated = expr_neg(right_poly.coeff[i]);
        if (!equation_symbolic_add_owned(poly, i, negated))
            goto cleanup;
    }
    ok = true;

cleanup:
    equation_symbolic_poly_clear(&right_poly);
    return ok;
}

static bool equation_symbolic_poly_collect_mul(const expr_t *left,
                                               const expr_t *right,
                                               const expr_t *wrt,
                                               equation_symbolic_poly_t *poly)
{
    equation_symbolic_poly_t left_poly = { 0 };
    equation_symbolic_poly_t right_poly = { 0 };
    bool ok = false;

    if (!equation_symbolic_poly_collect(left, wrt, &left_poly) ||
        !equation_symbolic_poly_collect(right, wrt, &right_poly))
        goto cleanup;

    ok = equation_symbolic_poly_multiply(&left_poly, &right_poly, poly);

cleanup:
    equation_symbolic_poly_clear(&right_poly);
    equation_symbolic_poly_clear(&left_poly);
    return ok;
}

static long equation_symbolic_power_number_to_long(number_t value)
{
    if (!num_is_integer(value) || !num_is_real(value))
        return -1L;

    for (size_t i = 0u; i <= EQUATION_SYMBOLIC_POLY_MAX_DEGREE; ++i) {
        number_t candidate = num_create_from_long((long)i);
        bool ok = num_eq(value, candidate);

        num_destroy(&candidate);
        if (ok)
            return (long)i;
    }

    return -1L;
}

static long equation_symbolic_power_exponent(const expr_t *expr)
{
    number_t exponent = num_new();
    long out = -1L;

    if (!expr)
        goto cleanup;

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW_D) {
        out = equation_symbolic_power_number_to_long(expr->c);
        goto cleanup;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW &&
        expr->b && expr_match_const_value(expr->b, &exponent))
        out = equation_symbolic_power_number_to_long(exponent);

cleanup:
    num_destroy(&exponent);
    return out;
}

static bool equation_symbolic_poly_copy_power(const expr_t *base,
                                              const expr_t *wrt,
                                              long exponent,
                                              equation_symbolic_poly_t *poly)
{
    equation_symbolic_poly_t base_poly = { 0 };
    equation_symbolic_poly_t square_poly = { 0 };
    equation_symbolic_poly_t cube_poly = { 0 };
    bool ok = false;

    if (exponent == 0L)
        return equation_symbolic_add_const(poly, 0u, 1L);
    if (exponent == 1L)
        return equation_symbolic_poly_collect(base, wrt, poly);

    if (!equation_symbolic_poly_collect(base, wrt, &base_poly) ||
        !equation_symbolic_poly_multiply(&base_poly, &base_poly, &square_poly))
        goto cleanup;

    if (exponent == 2L) {
        ok = equation_symbolic_copy_into(poly, &square_poly);
        goto cleanup;
    }

    if (!equation_symbolic_poly_multiply(&square_poly, &base_poly, &cube_poly))
        goto cleanup;
    ok = equation_symbolic_copy_into(poly, &cube_poly);

cleanup:
    equation_symbolic_poly_clear(&cube_poly);
    equation_symbolic_poly_clear(&square_poly);
    equation_symbolic_poly_clear(&base_poly);
    return ok;
}

static bool equation_symbolic_poly_collect_power(const expr_t *expr,
                                                 const expr_t *wrt,
                                                 equation_symbolic_poly_t *poly)
{
    long exponent = equation_symbolic_power_exponent(expr);

    if (exponent < 0L)
        return false;
    return equation_symbolic_poly_copy_power(expr->a, wrt, exponent, poly);
}

static bool equation_symbolic_poly_collect(const expr_t *expr,
                                           const expr_t *wrt,
                                           equation_symbolic_poly_t *poly)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    bool is_sub = false;

    if (!expr || !wrt || !poly)
        return false;

    if (!equation_expr_depends_on_wrt(expr, wrt))
        return equation_symbolic_add_owned(poly, 0u, equation_retain_expr(expr));

    if (equation_expr_is_wrt(expr, wrt))
        return equation_symbolic_add_const(poly, 1u, 1L);

    if (expr_match_unary_op(expr, EXPR_KIND_NEG, &left))
        return equation_symbolic_poly_collect_neg(left, wrt, poly);

    if (expr_match_add_sub_expr(expr, &left, &right, &is_sub)) {
        if (is_sub)
            return equation_symbolic_poly_collect_sub(left, right, wrt, poly);
        return equation_symbolic_poly_collect(left, wrt, poly) &&
               equation_symbolic_poly_collect(right, wrt, poly);
    }

    if (expr_match_mul_expr(expr, &left, &right))
        return equation_symbolic_poly_collect_mul(left, right, wrt, poly);

    if (equation_symbolic_poly_collect_power(expr, wrt, poly))
        return true;

    return false;
}

static expr_t *equation_symbolic_coeff_or_zero(equation_symbolic_poly_t *poly,
                                               size_t degree)
{
    if (!poly || degree >= EQUATION_SYMBOLIC_POLY_COEFFS || !poly->coeff[degree])
        return equation_const_long_expr(0L);

    return equation_retain_expr(poly->coeff[degree]);
}

bool equation_match_symbolic_linear_expr(const expr_t *expr,
                                         const expr_t *wrt,
                                         expr_t **constant_out,
                                         expr_t **linear_out)
{
    equation_symbolic_poly_t poly = { 0 };
    bool ok;

    if (!expr || !wrt || !constant_out || !linear_out)
        return false;

    ok = equation_symbolic_poly_collect(expr, wrt, &poly) &&
         equation_symbolic_coeff_is_zero(poly.coeff[2]) &&
         equation_symbolic_coeff_is_zero(poly.coeff[3]) &&
         !equation_symbolic_coeff_is_zero(poly.coeff[1]);
    if (!ok)
        goto cleanup;

    *constant_out = equation_symbolic_coeff_or_zero(&poly, 0u);
    *linear_out = equation_symbolic_coeff_or_zero(&poly, 1u);
    ok = *constant_out && *linear_out;

cleanup:
    equation_symbolic_poly_clear(&poly);
    return ok;
}

bool equation_match_symbolic_quadratic_expr(const expr_t *expr,
                                            const expr_t *wrt,
                                            expr_t **constant_out,
                                            expr_t **linear_out,
                                            expr_t **quadratic_out)
{
    equation_symbolic_poly_t poly = { 0 };
    bool ok;

    if (!expr || !wrt || !constant_out || !linear_out || !quadratic_out)
        return false;

    ok = equation_symbolic_poly_collect(expr, wrt, &poly) &&
         equation_symbolic_coeff_is_zero(poly.coeff[3]) &&
         !equation_symbolic_coeff_is_zero(poly.coeff[2]);
    if (!ok)
        goto cleanup;

    *constant_out = equation_symbolic_coeff_or_zero(&poly, 0u);
    *linear_out = equation_symbolic_coeff_or_zero(&poly, 1u);
    *quadratic_out = equation_symbolic_coeff_or_zero(&poly, 2u);
    ok = *constant_out && *linear_out && *quadratic_out;

cleanup:
    equation_symbolic_poly_clear(&poly);
    return ok;
}

bool equation_match_symbolic_cubic_expr(const expr_t *expr,
                                        const expr_t *wrt,
                                        expr_t **constant_out,
                                        expr_t **linear_out,
                                        expr_t **quadratic_out,
                                        expr_t **cubic_out)
{
    equation_symbolic_poly_t poly = { 0 };
    bool ok;

    if (!expr || !wrt || !constant_out || !linear_out ||
        !quadratic_out || !cubic_out)
        return false;

    ok = equation_symbolic_poly_collect(expr, wrt, &poly) &&
         !equation_symbolic_coeff_is_zero(poly.coeff[3]);
    if (!ok)
        goto cleanup;

    *constant_out = equation_symbolic_coeff_or_zero(&poly, 0u);
    *linear_out = equation_symbolic_coeff_or_zero(&poly, 1u);
    *quadratic_out = equation_symbolic_coeff_or_zero(&poly, 2u);
    *cubic_out = equation_symbolic_coeff_or_zero(&poly, 3u);
    ok = *constant_out && *linear_out && *quadratic_out && *cubic_out;

cleanup:
    equation_symbolic_poly_clear(&poly);
    return ok;
}
