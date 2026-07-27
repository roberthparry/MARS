#include <stdbool.h>
#include <stddef.h>

#define MARS_EQUATION_INTERNAL_ACCESS
#include "equation_internal.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

enum {
    EQUATION_SYMBOLIC_POLY_MAX_DEGREE = 3u,
    EQUATION_SYMBOLIC_POLY_COEFFS = EQUATION_SYMBOLIC_POLY_MAX_DEGREE + 1u
};

typedef struct equation_symbolic_poly {
    expr_t *coeff[EQUATION_SYMBOLIC_POLY_COEFFS];
} equation_symbolic_poly_t;

static void equ_symbolic_poly_clear(equation_symbolic_poly_t *poly)
{
    if (!poly)
        return;

    for (size_t i = 0u; i < EQUATION_SYMBOLIC_POLY_COEFFS; ++i) {
        expr_free(poly->coeff[i]);
        poly->coeff[i] = NULL;
    }
}

static bool equ_symbolic_coeff_is_zero(const expr_t *expr)
{
    number_t value = num_new();
    bool zero = false;

    if (!expr) {
        num_destroy(&value);
        return true;
    }

    if (expr_match_const_value(expr, &value))
        zero = num_is_zero(value);
    num_destroy(&value);
    return zero;
}

static bool equ_symbolic_add_owned(equation_symbolic_poly_t *poly,
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
        poly->coeff[degree] = expr_simplify_owned(term);
        return poly->coeff[degree] != NULL;
    }

    sum = expr_add(poly->coeff[degree], term);
    expr_free(term);
    expr_free(poly->coeff[degree]);
    poly->coeff[degree] = expr_simplify_owned(sum);
    return poly->coeff[degree] != NULL;
}

static bool equ_symbolic_add_const(equation_symbolic_poly_t *poly,
                                        size_t degree,
                                        long value)
{
    return equ_symbolic_add_owned(poly, degree,
                                       expr_const_long(value));
}

static bool equ_symbolic_copy_into(equation_symbolic_poly_t *out,
                                        const equation_symbolic_poly_t *in)
{
    for (size_t i = 0u; i < EQUATION_SYMBOLIC_POLY_COEFFS; ++i) {
        if (!in->coeff[i])
            continue;
        if (!equ_symbolic_add_owned(out, i,
                                         expr_retain_expr(in->coeff[i])))
            return false;
    }

    return true;
}

static bool equ_expr_depends_on_wrt(const expr_t *expr, const expr_t *wrt)
{
    expr_t *vars[1];
    bool used = false;

    if (!expr || !wrt)
        return false;

    vars[0] = (expr_t *)wrt;
    return expr_collect_var_usage(expr, 1u, vars, &used) && used;
}

static bool equ_expr_is_wrt(const expr_t *expr, const expr_t *wrt)
{
    expr_t *vars[1];
    size_t index = 0u;

    if (!expr || !wrt)
        return false;

    vars[0] = (expr_t *)wrt;
    return expr_match_var_expr(expr, 1u, vars, &index) && index == 0u;
}

static bool equ_symbolic_poly_collect(const expr_t *expr,
                                           const expr_t *wrt,
                                           equation_symbolic_poly_t *poly);

static bool equ_symbolic_poly_multiply(
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
            if (!equ_symbolic_add_owned(out, i + j, term))
                return false;
        }
    }

    return true;
}

static bool equ_symbolic_poly_collect_neg(const expr_t *arg,
                                               const expr_t *wrt,
                                               equation_symbolic_poly_t *poly)
{
    equation_symbolic_poly_t inner = { 0 };
    bool ok = false;

    if (!equ_symbolic_poly_collect(arg, wrt, &inner))
        goto cleanup;

    for (size_t i = 0u; i < EQUATION_SYMBOLIC_POLY_COEFFS; ++i) {
        expr_t *negated;

        if (!inner.coeff[i])
            continue;
        negated = expr_neg(inner.coeff[i]);
        if (!equ_symbolic_add_owned(poly, i, negated))
            goto cleanup;
    }
    ok = true;

cleanup:
    equ_symbolic_poly_clear(&inner);
    return ok;
}

static bool equ_symbolic_poly_collect_sub(const expr_t *left,
                                               const expr_t *right,
                                               const expr_t *wrt,
                                               equation_symbolic_poly_t *poly)
{
    equation_symbolic_poly_t right_poly = { 0 };
    bool ok = false;

    if (!equ_symbolic_poly_collect(left, wrt, poly) ||
        !equ_symbolic_poly_collect(right, wrt, &right_poly))
        goto cleanup;

    for (size_t i = 0u; i < EQUATION_SYMBOLIC_POLY_COEFFS; ++i) {
        expr_t *negated;

        if (!right_poly.coeff[i])
            continue;
        negated = expr_neg(right_poly.coeff[i]);
        if (!equ_symbolic_add_owned(poly, i, negated))
            goto cleanup;
    }
    ok = true;

cleanup:
    equ_symbolic_poly_clear(&right_poly);
    return ok;
}

static bool equ_symbolic_poly_collect_mul(const expr_t *left,
                                               const expr_t *right,
                                               const expr_t *wrt,
                                               equation_symbolic_poly_t *poly)
{
    equation_symbolic_poly_t left_poly = { 0 };
    equation_symbolic_poly_t right_poly = { 0 };
    bool ok = false;

    if (!equ_symbolic_poly_collect(left, wrt, &left_poly) ||
        !equ_symbolic_poly_collect(right, wrt, &right_poly))
        goto cleanup;

    ok = equ_symbolic_poly_multiply(&left_poly, &right_poly, poly);

cleanup:
    equ_symbolic_poly_clear(&right_poly);
    equ_symbolic_poly_clear(&left_poly);
    return ok;
}

static long equ_symbolic_power_number_to_long(number_t value)
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

static long equ_symbolic_power_exponent(const expr_t *expr, const expr_t **base_out)
{
    number_t exponent = num_new();
    const expr_t *base = NULL;
    const expr_t *exponent_expr = NULL;
    long out = -1L;

    if (!expr)
        goto cleanup;

    if (expr_match_pow_const(expr, &base, &exponent)) {
        out = equ_symbolic_power_number_to_long(exponent);
        goto cleanup;
    }

    if (expr_match_pow_expr(expr, &base, &exponent_expr) &&
        expr_match_const_value(exponent_expr, &exponent))
        out = equ_symbolic_power_number_to_long(exponent);

cleanup:
    if (out >= 0L && base_out)
        *base_out = base;
    num_destroy(&exponent);
    return out;
}

static bool equ_symbolic_poly_copy_power(const expr_t *base,
                                              const expr_t *wrt,
                                              long exponent,
                                              equation_symbolic_poly_t *poly)
{
    equation_symbolic_poly_t base_poly = { 0 };
    equation_symbolic_poly_t square_poly = { 0 };
    equation_symbolic_poly_t cube_poly = { 0 };
    bool ok = false;

    if (exponent == 0L)
        return equ_symbolic_add_const(poly, 0u, 1L);
    if (exponent == 1L)
        return equ_symbolic_poly_collect(base, wrt, poly);

    if (!equ_symbolic_poly_collect(base, wrt, &base_poly) ||
        !equ_symbolic_poly_multiply(&base_poly, &base_poly, &square_poly))
        goto cleanup;

    if (exponent == 2L) {
        ok = equ_symbolic_copy_into(poly, &square_poly);
        goto cleanup;
    }

    if (!equ_symbolic_poly_multiply(&square_poly, &base_poly, &cube_poly))
        goto cleanup;
    ok = equ_symbolic_copy_into(poly, &cube_poly);

cleanup:
    equ_symbolic_poly_clear(&cube_poly);
    equ_symbolic_poly_clear(&square_poly);
    equ_symbolic_poly_clear(&base_poly);
    return ok;
}

static bool equ_symbolic_poly_collect_power(const expr_t *expr,
                                                 const expr_t *wrt,
                                                 equation_symbolic_poly_t *poly)
{
    const expr_t *base = NULL;
    long exponent = equ_symbolic_power_exponent(expr, &base);

    if (exponent < 0L || !base)
        return false;
    return equ_symbolic_poly_copy_power(base, wrt, exponent, poly);
}

static bool equ_symbolic_poly_collect(const expr_t *expr,
                                           const expr_t *wrt,
                                           equation_symbolic_poly_t *poly)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    bool is_sub = false;

    if (!expr || !wrt || !poly)
        return false;

    if (!equ_expr_depends_on_wrt(expr, wrt))
        return equ_symbolic_add_owned(poly, 0u, expr_retain_expr(expr));

    if (equ_expr_is_wrt(expr, wrt))
        return equ_symbolic_add_const(poly, 1u, 1L);

    if (expr_match_neg_expr(expr, &left))
        return equ_symbolic_poly_collect_neg(left, wrt, poly);

    if (expr_match_add_sub_expr(expr, &left, &right, &is_sub)) {
        if (is_sub)
            return equ_symbolic_poly_collect_sub(left, right, wrt, poly);
        return equ_symbolic_poly_collect(left, wrt, poly) &&
               equ_symbolic_poly_collect(right, wrt, poly);
    }

    if (expr_match_mul_expr(expr, &left, &right))
        return equ_symbolic_poly_collect_mul(left, right, wrt, poly);

    if (equ_symbolic_poly_collect_power(expr, wrt, poly))
        return true;

    return false;
}

static expr_t *equ_symbolic_coeff_or_zero(equation_symbolic_poly_t *poly,
                                               size_t degree)
{
    if (!poly || degree >= EQUATION_SYMBOLIC_POLY_COEFFS || !poly->coeff[degree])
        return expr_const_long(0L);

    return expr_retain_expr(poly->coeff[degree]);
}

bool equ_match_symbolic_linear_expr(const expr_t *expr,
                                         const expr_t *wrt,
                                         expr_t **constant_out,
                                         expr_t **linear_out)
{
    equation_symbolic_poly_t poly = { 0 };
    bool ok;

    if (!expr || !wrt || !constant_out || !linear_out)
        return false;

    ok = equ_symbolic_poly_collect(expr, wrt, &poly) &&
         equ_symbolic_coeff_is_zero(poly.coeff[2]) &&
         equ_symbolic_coeff_is_zero(poly.coeff[3]) &&
         !equ_symbolic_coeff_is_zero(poly.coeff[1]);
    if (!ok)
        goto cleanup;

    *constant_out = equ_symbolic_coeff_or_zero(&poly, 0u);
    *linear_out = equ_symbolic_coeff_or_zero(&poly, 1u);
    ok = *constant_out && *linear_out;

cleanup:
    equ_symbolic_poly_clear(&poly);
    return ok;
}

bool equ_match_symbolic_quadratic_expr(const expr_t *expr,
                                            const expr_t *wrt,
                                            expr_t **constant_out,
                                            expr_t **linear_out,
                                            expr_t **quadratic_out)
{
    equation_symbolic_poly_t poly = { 0 };
    bool ok;

    if (!expr || !wrt || !constant_out || !linear_out || !quadratic_out)
        return false;

    ok = equ_symbolic_poly_collect(expr, wrt, &poly) &&
         equ_symbolic_coeff_is_zero(poly.coeff[3]) &&
         !equ_symbolic_coeff_is_zero(poly.coeff[2]);
    if (!ok)
        goto cleanup;

    *constant_out = equ_symbolic_coeff_or_zero(&poly, 0u);
    *linear_out = equ_symbolic_coeff_or_zero(&poly, 1u);
    *quadratic_out = equ_symbolic_coeff_or_zero(&poly, 2u);
    ok = *constant_out && *linear_out && *quadratic_out;

cleanup:
    equ_symbolic_poly_clear(&poly);
    return ok;
}

bool equ_match_symbolic_cubic_expr(const expr_t *expr,
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

    ok = equ_symbolic_poly_collect(expr, wrt, &poly) &&
         !equ_symbolic_coeff_is_zero(poly.coeff[3]);
    if (!ok)
        goto cleanup;

    *constant_out = equ_symbolic_coeff_or_zero(&poly, 0u);
    *linear_out = equ_symbolic_coeff_or_zero(&poly, 1u);
    *quadratic_out = equ_symbolic_coeff_or_zero(&poly, 2u);
    *cubic_out = equ_symbolic_coeff_or_zero(&poly, 3u);
    ok = *constant_out && *linear_out && *quadratic_out && *cubic_out;

cleanup:
    equ_symbolic_poly_clear(&poly);
    return ok;
}
