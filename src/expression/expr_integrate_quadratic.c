#include <stdbool.h>
#include <stddef.h>

#include "expr_integrate_internal.h"

typedef struct {
    expr_t *coeff[3];
} symbolic_poly2_t;

static expr_t *retain_expr_local_quadratic(const expr_t *expr)
{
    if (!expr)
        return NULL;
    expr_retain(expr);
    return (expr_t *)expr;
}

static void symbolic_poly2_clear(symbolic_poly2_t *poly)
{
    if (!poly)
        return;
    for (size_t i = 0; i < 3u; ++i) {
        expr_free(poly->coeff[i]);
        poly->coeff[i] = NULL;
    }
}

static expr_t *expr_const_long_local(long value)
{
    number_t number = num_create_from_long(value);
    expr_t *expr = expr_new_const(number);

    num_destroy(&number);
    return expr;
}

static bool symbolic_poly2_add_owned(symbolic_poly2_t *poly, size_t degree, expr_t *term)
{
    expr_t *sum;

    if (!poly || degree >= 3u) {
        expr_free(term);
        return false;
    }
    if (!term)
        return false;

    if (!poly->coeff[degree]) {
        poly->coeff[degree] = simplify_owned(term);
        return poly->coeff[degree] != NULL;
    }

    sum = expr_add(poly->coeff[degree], term);
    expr_free(term);
    expr_free(poly->coeff[degree]);
    poly->coeff[degree] = simplify_owned(sum);
    return poly->coeff[degree] != NULL;
}

static bool symbolic_poly2_add_const(symbolic_poly2_t *poly, size_t degree, long value)
{
    return symbolic_poly2_add_owned(poly, degree, expr_const_long_local(value));
}

static bool symbolic_poly2_collect(const expr_t *expr, const expr_t *wrt, symbolic_poly2_t *poly);

static bool symbolic_poly2_collect_mul(const expr_t *left,
                                       const expr_t *right,
                                       const expr_t *wrt,
                                       symbolic_poly2_t *poly)
{
    symbolic_poly2_t left_poly = { 0 };
    symbolic_poly2_t right_poly = { 0 };
    bool ok = false;

    if (!symbolic_poly2_collect(left, wrt, &left_poly) ||
        !symbolic_poly2_collect(right, wrt, &right_poly))
        goto cleanup;

    for (size_t i = 0; i < 3u; ++i) {
        if (!left_poly.coeff[i])
            continue;
        for (size_t j = 0; j < 3u; ++j) {
            expr_t *term;

            if (!right_poly.coeff[j])
                continue;
            if (i + j >= 3u)
                goto cleanup;

            term = expr_mul(left_poly.coeff[i], right_poly.coeff[j]);
            if (!symbolic_poly2_add_owned(poly, i + j, term))
                goto cleanup;
        }
    }

    ok = true;

cleanup:
    symbolic_poly2_clear(&right_poly);
    symbolic_poly2_clear(&left_poly);
    return ok;
}

static bool symbolic_poly2_collect(const expr_t *expr, const expr_t *wrt, symbolic_poly2_t *poly)
{
    number_t exponent = num_new();
    bool ok = false;

    if (!expr || !wrt || !poly)
        goto cleanup;

    if (!depends_on_wrt(expr, wrt)) {
        ok = symbolic_poly2_add_owned(poly, 0u, retain_expr_local_quadratic(expr));
        goto cleanup;
    }

    if (is_wrt(expr, wrt)) {
        ok = symbolic_poly2_add_const(poly, 1u, 1L);
        goto cleanup;
    }

    if (expr_is_op(expr, &ops_neg) && expr->a) {
        symbolic_poly2_t inner = { 0 };

        if (!symbolic_poly2_collect(expr->a, wrt, &inner)) {
            symbolic_poly2_clear(&inner);
            goto cleanup;
        }
        for (size_t i = 0; i < 3u; ++i) {
            expr_t *negated;

            if (!inner.coeff[i])
                continue;
            negated = expr_neg(inner.coeff[i]);
            if (!symbolic_poly2_add_owned(poly, i, negated)) {
                symbolic_poly2_clear(&inner);
                goto cleanup;
            }
        }
        symbolic_poly2_clear(&inner);
        ok = true;
        goto cleanup;
    }

    if (expr_is_op(expr, &ops_add) && expr->a && expr->b) {
        ok = symbolic_poly2_collect(expr->a, wrt, poly) &&
             symbolic_poly2_collect(expr->b, wrt, poly);
        goto cleanup;
    }

    if (expr_is_op(expr, &ops_sub) && expr->a && expr->b) {
        symbolic_poly2_t right_poly = { 0 };

        if (!symbolic_poly2_collect(expr->a, wrt, poly) ||
            !symbolic_poly2_collect(expr->b, wrt, &right_poly)) {
            symbolic_poly2_clear(&right_poly);
            goto cleanup;
        }
        for (size_t i = 0; i < 3u; ++i) {
            expr_t *negated;

            if (!right_poly.coeff[i])
                continue;
            negated = expr_neg(right_poly.coeff[i]);
            if (!symbolic_poly2_add_owned(poly, i, negated)) {
                symbolic_poly2_clear(&right_poly);
                goto cleanup;
            }
        }
        symbolic_poly2_clear(&right_poly);
        ok = true;
        goto cleanup;
    }

    if (expr_is_op(expr, &ops_mul) && expr->a && expr->b) {
        ok = symbolic_poly2_collect_mul(expr->a, expr->b, wrt, poly);
        goto cleanup;
    }

    if (expr_is_pow_d_expr(expr) && is_wrt(expr->a, wrt) &&
        num_eq(expr->c, NUM_TWO)) {
        ok = symbolic_poly2_add_const(poly, 2u, 1L);
        goto cleanup;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW &&
        expr->a && expr->b &&
        is_wrt(expr->a, wrt) &&
        expr_match_const_value(expr->b, &exponent) &&
        num_eq(exponent, NUM_TWO)) {
        ok = symbolic_poly2_add_const(poly, 2u, 1L);
        goto cleanup;
    }

cleanup:
    num_destroy(&exponent);
    return ok;
}

static expr_t *symbolic_poly2_coeff_or_zero(const symbolic_poly2_t *poly, size_t degree)
{
    if (!poly || degree >= 3u || !poly->coeff[degree])
        return expr_const_long_local(0L);
    return retain_expr_local_quadratic(poly->coeff[degree]);
}

static bool symbolic_poly2_all_coeffs_numeric(const symbolic_poly2_t *poly)
{
    number_t value = num_new();
    bool all_numeric = true;

    if (!poly) {
        num_destroy(&value);
        return true;
    }

    for (size_t i = 0; i < 3u; ++i) {
        if (!poly->coeff[i])
            continue;
        if (!expr_match_const_value(poly->coeff[i], &value)) {
            all_numeric = false;
            break;
        }
    }

    num_destroy(&value);
    return all_numeric;
}

static expr_t *symbolic_general_quadratic_inverse_integral(const expr_t *wrt,
                                                           const expr_t *a,
                                                           const expr_t *b,
                                                           const expr_t *c)
{
    expr_t *four = expr_const_long_local(4L);
    expr_t *two = expr_const_long_local(2L);
    expr_t *ac = (a && c) ? expr_mul(a, c) : NULL;
    expr_t *four_ac = (four && ac) ? expr_mul(four, ac) : NULL;
    expr_t *b_sq = b ? expr_mul(b, b) : NULL;
    expr_t *delta_raw = (four_ac && b_sq) ? expr_sub(four_ac, b_sq) : NULL;
    expr_t *delta = simplify_owned(delta_raw);
    expr_t *sqrt_delta = delta ? expr_sqrt(delta) : NULL;
    expr_t *two_a = (two && a) ? expr_mul(two, a) : NULL;
    expr_t *two_ax = (two_a && wrt) ? expr_mul(two_a, wrt) : NULL;
    expr_t *arg_num = (two_ax && b) ? expr_add(two_ax, b) : NULL;
    expr_t *arg = (arg_num && sqrt_delta) ? expr_div(arg_num, sqrt_delta) : NULL;
    expr_t *atan_arg = arg ? expr_atan(arg) : NULL;
    expr_t *scaled_atan = two ? expr_mul(two, atan_arg) : NULL;
    expr_t *out = (scaled_atan && sqrt_delta) ? expr_div(scaled_atan, sqrt_delta) : NULL;

    expr_free(scaled_atan);
    expr_free(atan_arg);
    expr_free(arg);
    expr_free(arg_num);
    expr_free(two_ax);
    expr_free(two_a);
    expr_free(sqrt_delta);
    expr_free(delta);
    expr_free(b_sq);
    expr_free(four_ac);
    expr_free(ac);
    expr_free(two);
    expr_free(four);
    return out;
}

static expr_t *add_symbolic_parts_owned(expr_t *left, expr_t *right)
{
    expr_t *sum;

    if (!left)
        return right;
    if (!right)
        return left;

    sum = expr_add(left, right);
    expr_free(right);
    expr_free(left);
    return sum;
}

expr_t *integrate_linear_over_symbolic_quadratic(const expr_t *expr, const expr_t *wrt)
{
    symbolic_poly2_t numer = { 0 };
    symbolic_poly2_t denom = { 0 };
    expr_t *a = NULL;
    expr_t *b = NULL;
    expr_t *c = NULL;
    expr_t *m = NULL;
    expr_t *n = NULL;
    expr_t *two = NULL;
    expr_t *two_a = NULL;
    expr_t *alpha = NULL;
    expr_t *alpha_for_log = NULL;
    expr_t *log_denom = NULL;
    expr_t *log_part = NULL;
    expr_t *alpha_b = NULL;
    expr_t *remainder = NULL;
    expr_t *inverse_integral = NULL;
    expr_t *remainder_part = NULL;
    expr_t *sum = NULL;

    if (!expr || !expr->a || !expr->b)
        return NULL;

    if (!symbolic_poly2_collect(expr->a, wrt, &numer) ||
        !symbolic_poly2_collect(expr->b, wrt, &denom) ||
        numer.coeff[2] ||
        !denom.coeff[2] ||
        (symbolic_poly2_all_coeffs_numeric(&numer) &&
         symbolic_poly2_all_coeffs_numeric(&denom)))
        goto cleanup;

    a = symbolic_poly2_coeff_or_zero(&denom, 2u);
    b = symbolic_poly2_coeff_or_zero(&denom, 1u);
    c = symbolic_poly2_coeff_or_zero(&denom, 0u);
    m = symbolic_poly2_coeff_or_zero(&numer, 1u);
    n = symbolic_poly2_coeff_or_zero(&numer, 0u);
    two = expr_const_long_local(2L);
    two_a = (two && a) ? expr_mul(two, a) : NULL;
    alpha = (m && two_a) ? expr_div(m, two_a) : NULL;
    alpha = simplify_owned(alpha);
    alpha_for_log = retain_expr_local_quadratic(alpha);
    log_denom = expr_log(expr->b);
    log_part = (alpha_for_log && log_denom) ? expr_mul(alpha_for_log, log_denom) : NULL;
    alpha_b = (alpha && b) ? expr_mul(alpha, b) : NULL;
    remainder = (n && alpha_b) ? expr_sub(n, alpha_b) : NULL;
    remainder = simplify_owned(remainder);
    inverse_integral = symbolic_general_quadratic_inverse_integral(wrt, a, b, c);
    remainder_part = (remainder && inverse_integral)
                         ? expr_mul(remainder, inverse_integral)
                         : NULL;
    sum = add_symbolic_parts_owned(log_part, remainder_part);
    log_part = NULL;
    remainder_part = NULL;

cleanup:
    expr_free(remainder_part);
    expr_free(inverse_integral);
    expr_free(remainder);
    expr_free(alpha_b);
    expr_free(log_part);
    expr_free(log_denom);
    expr_free(alpha_for_log);
    expr_free(alpha);
    expr_free(two_a);
    expr_free(two);
    expr_free(n);
    expr_free(m);
    expr_free(c);
    expr_free(b);
    expr_free(a);
    symbolic_poly2_clear(&denom);
    symbolic_poly2_clear(&numer);
    return sum;
}
