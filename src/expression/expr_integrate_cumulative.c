/**
 * @file expr_integrate_cumulative.c
 * @brief Closed primitives for Gaussian cumulative integrals with live symbolic parameters.
 *
 * Applies integration by parts to an affine upper endpoint before general substitution
 * search. Completing the square supplies the cumulative term through erf/erfc, avoiding
 * nested numerical quadrature. The rule retains the Gaussian decay condition
 * Re(-a) > 0 for the quadratic coefficient a, including finite endpoints.
 * This is an expression implementation detail, not browser-side mathematical rewriting.
 */
#include <string.h>

#define MARS_EXPR_INTEGRATE_INTERNAL_ACCESS
#include "expr_integrate_internal.h"

/* Reject non-polynomial operators before using derivatives to collect three coefficients. */
static int integrate_cumulative_degree(const expr_t *expr, const expr_t *dummy)
{
    if (!expr)
        return -1;
    if (!depends_on_wrt(expr, dummy))
        return 0;
    if (is_wrt(expr, dummy))
        return 1;
    if (expr_is_neg(expr))
        return integrate_cumulative_degree(expr->a, dummy);
    if (expr_is_op(expr, &ops_div) && !depends_on_wrt(expr->b, dummy))
        return integrate_cumulative_degree(expr->a, dummy);
    if (expr_is_addsub(expr) || expr_is_op(expr, &ops_mul)) {
        int left = integrate_cumulative_degree(expr->a, dummy);
        int right = integrate_cumulative_degree(expr->b, dummy);
        int degree = expr_is_op(expr, &ops_mul) ? left + right : (left > right ? left : right);
        return left < 0 || right < 0 || degree > 2 ? -1 : degree;
    }
    if ((expr_is_op(expr, &ops_pow_d) && num_eq(expr->c, NUM_TWO)) ||
        (expr_is_op(expr, &ops_pow) && expr_is_unnamed_const(expr->b) && num_eq(expr->b->c, NUM_TWO))) {
        int degree = integrate_cumulative_degree(expr->a, dummy);
        return degree == 1 ? 2 : -1;
    }
    return -1;
}

static bool integrate_cumulative_coefficients(const expr_t *quadratic, const expr_t *dummy,
                                               expr_t **a, expr_t **b, expr_t **c)
{
    if (integrate_cumulative_degree(quadratic, dummy) != 2)
        return false;
    expr_t *zero = expr_new_const(NUM_ZERO);
    expr_t *first = expr_create_deriv(quadratic, dummy);
    expr_t *second = first ? expr_create_deriv(first, dummy) : NULL;
    *a = second ? simplify_owned(expr_div_long(second, 2)) : NULL;
    *b = first && zero ? simplify_owned(expr_substitute(first, dummy, zero)) : NULL;
    *c = zero ? simplify_owned(expr_substitute(quadratic, dummy, zero)) : NULL;
    bool ok = *a && *b && *c && !depends_on_wrt(*a, dummy) && !expr_const_is_zero(*a);
    expr_free(second);
    expr_free(first);
    expr_free(zero);
    return ok;
}

static int integrate_cumulative_infinity(const expr_t *bound)
{
    if (expr_is_op(bound, &ops_neg))
        return -integrate_cumulative_infinity(bound->a);
    if (!expr_is_const(bound) || !num_is_inf(bound->c))
        return 0;
    const char *name = expr_symbol_name(bound);
    const char *canonical = name ? expr_default_constant_canonical_name(name) : NULL;
    if (name && (!canonical || strcmp(canonical, "inf")))
        return 0;
    return num_lt(bound->c, NUM_ZERO) ? -1 : 1;
}

/* Return a closed Gaussian cumulative primitive without freezing parameter bindings. */
expr_t *integrate_cumulative_gaussian_rule(const expr_t *expr, const expr_t *wrt)
{
    const expr_t *dummy = expr_integral_dummy_expr(expr);
    const expr_t *upper = expr_integral_upper_bound_expr(expr);
    const expr_t *lower = expr_integral_lower_bound_expr(expr);
    expr_t *integrand = NULL, *a = NULL, *b = NULL, *c = NULL;
    expr_t *offset = NULL, *slope = NULL, *two_a = NULL, *shift = NULL;
    expr_t *negative_a = NULL, *variance = NULL, *root = NULL, *centre = NULL, *argument = NULL;
    expr_t *b_shift = NULL, *correction = NULL, *height = NULL, *scale = NULL;
    expr_t *pi = NULL, *sqrt_pi = NULL, *numerator = NULL, *denominator = NULL;
    expr_t *lower_centre = NULL, *lower_argument = NULL, *lower_erf = NULL;
    expr_t *upper_erf = NULL, *difference = NULL, *cumulative_product = NULL, *cumulative = NULL;
    expr_t *endpoint = NULL, *density = NULL, *first = NULL, *second = NULL;
    expr_t *difference_primitive = NULL, *primitive = NULL, *condition = NULL, *slope_condition = NULL;
    expr_t *zero = NULL, *out = NULL;
    int infinite_lower = integrate_cumulative_infinity(lower);
    if (!expr || !expr_is_op(expr, &ops_integral) || !dummy || !upper ||
        depends_on_wrt(expr->a, wrt) || (lower && depends_on_wrt(lower, wrt)) ||
        !match_symbolic_affine_constant_and_coeff(upper, wrt, &offset, &slope) || expr_const_is_zero(slope))
        goto cleanup;
    integrand = expr_simplify(expr->a);
    if (!expr_is_op(integrand, &ops_exp) ||
        !integrate_cumulative_coefficients(integrand->a, dummy, &a, &b, &c))
        goto cleanup;
    if (!a || !b || !c || expr_const_is_zero(a) ||
        (expr_is_unnamed_const(a) && num_is_real(a->c) && !num_lt(a->c, NUM_ZERO)))
        goto cleanup;
    two_a = expr_mul_long(a, 2);
    shift = two_a ? simplify_owned(expr_div(b, two_a)) : NULL;
    negative_a = expr_neg(a);
    /* Re(-a)>0 also makes Re(-1/a)>0. Its principal square root is the
     * reciprocal of sqrt(-a), without assuming a sign for a symbolic width. */
    variance = negative_a ? simplify_owned(expr_num_div(&NUM_ONE, negative_a)) : NULL;
    root = variance ? simplify_owned(expr_sqrt(variance)) : NULL;
    centre = shift ? simplify_owned(expr_add(upper, shift)) : NULL;
    argument = root && centre ? simplify_owned(expr_div(centre, root)) : NULL;
    b_shift = shift ? expr_mul(b, shift) : NULL;
    correction = b_shift ? expr_div_long(b_shift, 2) : NULL;
    height = correction ? simplify_owned(expr_sub(c, correction)) : NULL;
    scale = height ? expr_exp(height) : NULL;
    pi = expr_new_named_const(NUM_PI, "@pi");
    sqrt_pi = pi ? expr_sqrt(pi) : NULL;
    numerator = scale && sqrt_pi ? expr_mul(scale, sqrt_pi) : NULL;
    denominator = expr_const_long(2);
    if (infinite_lower) {
        bool from_left = infinite_lower < 0;
        lower_argument = argument ? (from_left ? expr_neg(argument) : expr_clone(argument)) : NULL;
        lower_erf = lower_argument ? expr_erfc(lower_argument) : NULL;
        difference = lower_erf ? (from_left ? expr_clone(lower_erf) : expr_neg(lower_erf)) : NULL;
        condition = negative_a ? expr_clone(negative_a) : NULL;
    } else {
        lower_centre = shift ? (lower ? expr_add(lower, shift) : expr_clone(shift)) : NULL;
        lower_argument = root && lower_centre ? expr_div(lower_centre, root) : NULL;
        lower_erf = lower_argument ? expr_erf(lower_argument) : NULL;
        upper_erf = argument ? expr_erf(argument) : NULL;
        difference = upper_erf && lower_erf ? expr_sub(upper_erf, lower_erf) : NULL;
        condition = negative_a ? expr_clone(negative_a) : NULL;
    }
    cumulative_product = numerator && root ? expr_mul(numerator, root) : NULL;
    if (cumulative_product && difference) {
        expr_t *product = expr_mul(cumulative_product, difference);
        expr_free(cumulative_product);
        cumulative_product = product;
    }
    cumulative = cumulative_product ? simplify_owned(expr_div(cumulative_product, denominator)) : NULL;
    endpoint = expr_substitute(integrand->a, dummy, upper);
    density = endpoint ? expr_exp(endpoint) : NULL;
    first = centre && cumulative ? expr_mul(centre, cumulative) : NULL;
    second = density && two_a ? expr_div(density, two_a) : NULL;
    difference_primitive = first && second ? expr_sub(first, second) : NULL;
    primitive = difference_primitive ? expr_div(difference_primitive, slope) : NULL;
    zero = expr_new_const(NUM_ZERO);
    slope_condition = expr_abs(slope);
    expr_t *args[] = {primitive, condition, zero, slope_condition, zero};
    out = primitive && condition && slope_condition && zero ? expr_real_domain_from_args(5u, args) : NULL;

cleanup:
    expr_free(zero);
    expr_free(slope_condition);
    expr_free(condition);
    expr_free(primitive);
    expr_free(difference_primitive);
    expr_free(second);
    expr_free(first);
    expr_free(density);
    expr_free(endpoint);
    expr_free(cumulative);
    expr_free(cumulative_product);
    expr_free(difference);
    expr_free(upper_erf);
    expr_free(lower_erf);
    expr_free(lower_argument);
    expr_free(lower_centre);
    expr_free(denominator);
    expr_free(numerator);
    expr_free(sqrt_pi);
    expr_free(pi);
    expr_free(scale);
    expr_free(height);
    expr_free(correction);
    expr_free(b_shift);
    expr_free(argument);
    expr_free(centre);
    expr_free(root);
    expr_free(variance);
    expr_free(negative_a);
    expr_free(shift);
    expr_free(two_a);
    expr_free(slope);
    expr_free(offset);
    expr_free(c);
    expr_free(b);
    expr_free(a);
    expr_free(integrand);
    return out;
}
