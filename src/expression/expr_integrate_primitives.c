#include <stdlib.h>

#define MARS_EXPR_INTEGRATE_INTERNAL_ACCESS
#include "expr_integrate_internal.h"
#include "expr_maths.h"

/* Match a numeric monomial a*x^p.  Keeping this matcher structural makes the
 * Bessel rule apply to a family of powers rather than to a catalogue of
 * individual orders and exponents. */
static bool match_numeric_wrt_monomial_primitives(const expr_t *expr, const expr_t *wrt, expr_t **coefficient_out,
                                                  expr_t **exponent_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    expr_t *left_coefficient = NULL;
    expr_t *right_coefficient = NULL;
    expr_t *left_exponent = NULL;
    expr_t *right_exponent = NULL;
    expr_t *coefficient = NULL;
    expr_t *exponent = NULL;
    number_t value = num_new();
    bool ok = false;

    if (!expr || !wrt || !coefficient_out || !exponent_out)
        goto cleanup;

    if (is_wrt(expr, wrt)) {
        coefficient = expr_new_const(NUM_ONE);
        exponent = expr_new_const(NUM_ONE);
        goto matched;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_SQRT && expr->a && is_wrt(expr->a, wrt)) {
        coefficient = expr_new_const(NUM_ONE);
        exponent = expr_new_const(NUM_HALF);
        goto matched;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW_D && expr->a && is_wrt(expr->a, wrt)) {
        coefficient = expr_new_const(NUM_ONE);
        exponent = expr_new_const(expr->c);
        goto matched;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW && expr->a && expr->b && is_wrt(expr->a, wrt) &&
        expr_match_const_value(expr->b, &value)) {
        coefficient = expr_new_const(NUM_ONE);
        exponent = expr_new_const(value);
        goto matched;
    }

    if (expr_match_const_value(expr, &value)) {
        coefficient = expr_new_const(value);
        exponent = expr_new_const(NUM_ZERO);
        goto matched;
    }

    if (expr_match_mul_expr(expr, &left, &right)) {
        if (!match_numeric_wrt_monomial_primitives(left, wrt, &left_coefficient, &left_exponent) ||
            !match_numeric_wrt_monomial_primitives(right, wrt, &right_coefficient, &right_exponent))
            goto cleanup;
        coefficient = expr_mul(left_coefficient, right_coefficient);
        exponent = expr_add(left_exponent, right_exponent);
        goto matched;
    }

    if (expr_is_op(expr, &ops_div) && expr->a && expr->b) {
        if (!match_numeric_wrt_monomial_primitives(expr->a, wrt, &left_coefficient, &left_exponent) ||
            !match_numeric_wrt_monomial_primitives(expr->b, wrt, &right_coefficient, &right_exponent))
            goto cleanup;
        coefficient = expr_div(left_coefficient, right_coefficient);
        exponent = expr_sub(left_exponent, right_exponent);
        goto matched;
    }

    goto cleanup;

matched:
    coefficient = simplify_owned(coefficient);
    exponent = simplify_owned(exponent);
    if (!coefficient || !exponent)
        goto cleanup;
    *coefficient_out = coefficient;
    *exponent_out = exponent;
    coefficient = NULL;
    exponent = NULL;
    ok = true;

cleanup:
    num_destroy(&value);
    expr_free(exponent);
    expr_free(coefficient);
    expr_free(right_exponent);
    expr_free(left_exponent);
    expr_free(right_coefficient);
    expr_free(left_coefficient);
    return ok;
}

static expr_t *integrate_bessel_kind_rule(const expr_t *expr, const expr_t *wrt, const expr_ops_t *expected_ops,
                                          expr_t *(*builder)(const expr_t *, const expr_t *))
{
    NUM_SCOPE(scope);
    expr_t *coefficient = NULL;
    expr_t *exponent = NULL;
    expr_t *mu_expr = NULL;
    expr_t *negative_inverse_power_expr = NULL;
    expr_t *coefficient_power = NULL;
    expr_t *outside = NULL;
    expr_t *one = NULL;
    expr_t *lower_order = NULL;
    expr_t *upper_order = NULL;
    expr_t *j = NULL;
    expr_t *j_lower = NULL;
    expr_t *j_upper = NULL;
    expr_t *j_difference = NULL;
    expr_t *j_prime = NULL;
    expr_t *lommel = NULL;
    expr_t *lommel_prime = NULL;
    expr_t *left_product = NULL;
    expr_t *right_product = NULL;
    expr_t *wronskian = NULL;
    expr_t *weighted_wronskian = NULL;
    expr_t *result = NULL;
    expr_t *out = NULL;
    number_t coefficient_value = num_new();
    number_t power_value = num_new();
    number_t inverse_power = num_new();
    number_t mu = num_new();
    number_t negative_inverse_power = num_new();

    if (!expr || !wrt || !expr->a || !expr->b || !expected_ops || !builder || !expr_is_op(expr, expected_ops) ||
        depends_on_wrt(expr->a, wrt) || !match_numeric_wrt_monomial_primitives(expr->b, wrt, &coefficient, &exponent) ||
        !expr_match_const_value(coefficient, &coefficient_value) || !expr_match_const_value(exponent, &power_value) ||
        !num_is_exact(coefficient_value) || !num_is_real(coefficient_value) || !num_gt(coefficient_value, NUM_ZERO) ||
        !num_is_exact(power_value) || !num_is_real(power_value) || num_is_zero(power_value))
        goto cleanup;

    /* For z=a*x^p and mu=1/p-1, the Lommel equation and Lagrange's
     * identity give
     *
     * d/dz { z[J_nu(z)s'_(mu,nu)(z)-J'_nu(z)s_(mu,nu)(z)] }
     *     = z^mu J_nu(z).
     *
     * Substituting z=a*x^p supplies the remaining factor a^(-1/p)/p. */
    inverse_power = num_inv(power_value);
    mu = num_sub(inverse_power, NUM_ONE);
    negative_inverse_power = num_neg(inverse_power);

    mu_expr = expr_new_const(mu);
    negative_inverse_power_expr = expr_new_const(negative_inverse_power);
    coefficient_power =
        (coefficient && negative_inverse_power_expr) ? expr_pow_xp(coefficient, negative_inverse_power_expr) : NULL;
    outside = (coefficient_power && exponent) ? expr_div(coefficient_power, exponent) : NULL;

    one = expr_new_const(NUM_ONE);
    lower_order = one ? expr_sub(expr->a, one) : NULL;
    upper_order = one ? expr_add(expr->a, one) : NULL;
    j = builder(expr->a, expr->b);
    j_lower = lower_order ? builder(lower_order, expr->b) : NULL;
    j_upper = upper_order ? builder(upper_order, expr->b) : NULL;
    j_difference = (j_lower && j_upper) ? expr_sub(j_lower, j_upper) : NULL;
    j_prime = j_difference ? expr_div_num(j_difference, &NUM_TWO) : NULL;
    lommel = mu_expr ? expr_lommel_s(mu_expr, expr->a, expr->b) : NULL;
    lommel_prime = mu_expr ? expr_lommel_s_argument_derivative_expansion(mu_expr, expr->a, expr->b) : NULL;
    left_product = (j && lommel_prime) ? expr_mul(j, lommel_prime) : NULL;
    right_product = (j_prime && lommel) ? expr_mul(j_prime, lommel) : NULL;
    wronskian = (left_product && right_product) ? expr_sub(left_product, right_product) : NULL;
    weighted_wronskian = wronskian ? expr_mul(expr->b, wronskian) : NULL;
    result = (outside && weighted_wronskian) ? expr_mul(outside, weighted_wronskian) : NULL;
    out = simplify_owned(result);
    result = NULL;

cleanup:
    expr_free(result);
    expr_free(weighted_wronskian);
    expr_free(wronskian);
    expr_free(right_product);
    expr_free(left_product);
    expr_free(lommel_prime);
    expr_free(lommel);
    expr_free(j_prime);
    expr_free(j_difference);
    expr_free(j_upper);
    expr_free(j_lower);
    expr_free(j);
    expr_free(upper_order);
    expr_free(lower_order);
    expr_free(one);
    expr_free(outside);
    expr_free(coefficient_power);
    expr_free(negative_inverse_power_expr);
    expr_free(mu_expr);
    expr_free(exponent);
    expr_free(coefficient);
    return out;
}

expr_t *integrate_bessel_j_rule(const expr_t *expr, const expr_t *wrt)
{
    return integrate_bessel_kind_rule(expr, wrt, &ops_bessel_j, expr_bessel_j);
}

expr_t *integrate_bessel_y_rule(const expr_t *expr, const expr_t *wrt)
{
    return integrate_bessel_kind_rule(expr, wrt, &ops_bessel_y, expr_bessel_y);
}

expr_t *integrate_lommel_s_rule(const expr_t *expr, const expr_t *wrt)
{
    NUM_SCOPE(scope);
    const expr_t *mu_expr_source = NULL;
    const expr_t *nu_expr_source = NULL;
    const expr_t *argument = NULL;
    expr_t *coefficient = NULL;
    expr_t *power = NULL;
    expr_t *coefficient_exponent_expr = NULL;
    expr_t *coefficient_factor = NULL;
    expr_t *outside = NULL;
    expr_t *lambda_expr = NULL;
    expr_t *one_expr = NULL;
    expr_t *a2_expr = NULL;
    expr_t *b1_expr = NULL;
    expr_t *b2_expr = NULL;
    expr_t *b3_expr = NULL;
    expr_t *argument_squared = NULL;
    expr_t *negative_argument_squared = NULL;
    expr_t *hyper_argument = NULL;
    expr_t *argument_power = NULL;
    expr_t *hypergeometric = NULL;
    expr_t *numerator = NULL;
    expr_t *scaled = NULL;
    expr_t *result = NULL;
    expr_t *out = NULL;
    number_t mu = num_new();
    number_t nu = num_new();
    number_t coefficient_value = num_new();
    number_t power_value = num_new();
    number_t inverse_power = num_new();
    number_t coefficient_exponent = num_new();
    number_t lambda = num_new();
    number_t a2 = num_new();
    number_t b1 = num_new();
    number_t b2 = num_new();
    number_t b3 = num_new();
    number_t mu_plus_one = num_new();
    number_t denominator = num_new();
    number_t three = num_create_from_long(3);
    number_t four = num_create_from_long(4);

    if (!expr || !wrt || !expr_lommel_s_unpack(expr, &mu_expr_source, &nu_expr_source, &argument) ||
        !expr_match_const_value(mu_expr_source, &mu) || !expr_match_const_value(nu_expr_source, &nu) ||
        !num_is_exact(mu) || !num_is_real(mu) || !num_is_exact(nu) || !num_is_real(nu) ||
        !match_numeric_wrt_monomial_primitives(argument, wrt, &coefficient, &power) ||
        !expr_match_const_value(coefficient, &coefficient_value) || !expr_match_const_value(power, &power_value) ||
        !num_is_exact(coefficient_value) || !num_is_real(coefficient_value) || !num_gt(coefficient_value, NUM_ZERO) ||
        !num_is_exact(power_value) || !num_is_real(power_value) || num_is_zero(power_value))
        goto cleanup;

    /* The defining series
     *
     * s_(mu,nu)(z) = z^(mu+1) / ((mu+1)^2-nu^2)
     *                 * 1F2(1; (mu-nu+3)/2, (mu+nu+3)/2; -z^2/4)
     *
     * can be integrated term by term after z=a*x^p.  With
     * lambda=mu+1/p+1, integration raises 1F2 to 2F3.  This is a
     * parameter rule for the whole family, not a table of special cases. */
    inverse_power = num_inv(power_value);
    coefficient_exponent = num_neg(inverse_power);
    lambda = num_add(num_add(mu, inverse_power), NUM_ONE);
    mu_plus_one = num_add(mu, NUM_ONE);
    denominator = num_mul(lambda, num_sub(num_mul(mu_plus_one, mu_plus_one), num_mul(nu, nu)));
    if (num_is_zero(lambda) || num_is_zero(denominator))
        goto cleanup;

    a2 = num_div(lambda, NUM_TWO);
    b1 = num_div(num_add(num_sub(mu, nu), three), NUM_TWO);
    b2 = num_div(num_add(num_add(mu, nu), three), NUM_TWO);
    b3 = num_add(a2, NUM_ONE);

    coefficient_exponent_expr = expr_new_const(coefficient_exponent);
    coefficient_factor = coefficient_exponent_expr ? expr_pow_xp(coefficient, coefficient_exponent_expr) : NULL;
    outside = coefficient_factor ? expr_div(coefficient_factor, power) : NULL;
    lambda_expr = expr_new_const(lambda);
    one_expr = expr_new_const(NUM_ONE);
    a2_expr = expr_new_const(a2);
    b1_expr = expr_new_const(b1);
    b2_expr = expr_new_const(b2);
    b3_expr = expr_new_const(b3);
    argument_squared = expr_pow(argument, &NUM_TWO);
    negative_argument_squared = argument_squared ? expr_neg(argument_squared) : NULL;
    hyper_argument = negative_argument_squared ? expr_div_num(negative_argument_squared, &four) : NULL;
    argument_power = lambda_expr ? expr_pow_xp(argument, lambda_expr) : NULL;
    if (one_expr && a2_expr && b1_expr && b2_expr && b3_expr && hyper_argument) {
        const expr_t *upper[2] = {one_expr, a2_expr};
        const expr_t *lower[3] = {b1_expr, b2_expr, b3_expr};

        hypergeometric = expr_hypergeometric_pFq(2u, upper, 3u, lower, hyper_argument);
    }
    numerator = argument_power && hypergeometric ? expr_mul(argument_power, hypergeometric) : NULL;
    scaled = numerator ? expr_div_num(numerator, &denominator) : NULL;
    result = outside && scaled ? expr_mul(outside, scaled) : NULL;
    out = simplify_owned(result);
    result = NULL;

cleanup:
    expr_free(result);
    expr_free(scaled);
    expr_free(numerator);
    expr_free(hypergeometric);
    expr_free(argument_power);
    expr_free(hyper_argument);
    expr_free(negative_argument_squared);
    expr_free(argument_squared);
    expr_free(b3_expr);
    expr_free(b2_expr);
    expr_free(b1_expr);
    expr_free(a2_expr);
    expr_free(one_expr);
    expr_free(lambda_expr);
    expr_free(outside);
    expr_free(coefficient_factor);
    expr_free(coefficient_exponent_expr);
    expr_free(power);
    expr_free(coefficient);
    return out;
}

expr_t *integrate_hypergeometric_pFq_rule(const expr_t *expr, const expr_t *wrt)
{
    NUM_SCOPE(scope);
    const expr_t **upper = NULL;
    const expr_t **lower = NULL;
    const expr_t *argument = NULL;
    const expr_t **integrated_upper = NULL;
    const expr_t **integrated_lower = NULL;
    expr_t *coefficient = NULL;
    expr_t *power = NULL;
    expr_t *inverse_power_expr = NULL;
    expr_t *successor_expr = NULL;
    expr_t *hypergeometric = NULL;
    expr_t *product = NULL;
    expr_t *out = NULL;
    number_t power_value = num_new();
    number_t inverse_power = num_new();
    number_t successor = num_new();
    size_t upper_count = 0u;
    size_t lower_count = 0u;

    if (!expr || !wrt || !expr_hypergeometric_pFq_unpack(expr, &upper, &upper_count, &lower, &lower_count, &argument))
        goto cleanup;

    for (size_t i = 0u; i < upper_count; ++i) {
        if (depends_on_wrt(upper[i], wrt))
            goto cleanup;
    }
    for (size_t i = 0u; i < lower_count; ++i) {
        if (depends_on_wrt(lower[i], wrt))
            goto cleanup;
    }

    if (!match_numeric_wrt_monomial_primitives(argument, wrt, &coefficient, &power) ||
        !expr_match_const_value(power, &power_value) || !num_is_exact(power_value) || !num_is_real(power_value) ||
        num_is_zero(power_value))
        goto cleanup;

    inverse_power = num_inv(power_value);
    successor = num_add(inverse_power, NUM_ONE);
    if (num_is_zero(successor))
        goto cleanup;

    integrated_upper = calloc(upper_count + 1u, sizeof(*integrated_upper));
    integrated_lower = calloc(lower_count + 1u, sizeof(*integrated_lower));
    if (!integrated_upper || !integrated_lower)
        goto cleanup;
    for (size_t i = 0u; i < upper_count; ++i)
        integrated_upper[i] = upper[i];
    for (size_t i = 0u; i < lower_count; ++i)
        integrated_lower[i] = lower[i];

    inverse_power_expr = expr_new_const(inverse_power);
    successor_expr = expr_new_const(successor);
    if (!inverse_power_expr || !successor_expr)
        goto cleanup;
    integrated_upper[upper_count] = inverse_power_expr;
    integrated_lower[lower_count] = successor_expr;
    hypergeometric =
        expr_hypergeometric_pFq(upper_count + 1u, integrated_upper, lower_count + 1u, integrated_lower, argument);
    product = hypergeometric ? expr_mul(wrt, hypergeometric) : NULL;
    out = simplify_owned(product);
    product = NULL;

cleanup:
    expr_free(product);
    expr_free(hypergeometric);
    expr_free(successor_expr);
    expr_free(inverse_power_expr);
    expr_free(power);
    expr_free(coefficient);
    free(integrated_lower);
    free(integrated_upper);
    free(lower);
    free(upper);
    return out;
}

static bool is_wrt_square_power_primitives(const expr_t *expr, const expr_t *wrt)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    number_t exponent = num_new();
    bool ok = false;

    if (!expr || !wrt)
        goto cleanup;

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW_D && expr->a && is_wrt(expr->a, wrt)) {
        ok = num_eq(expr->c, NUM_TWO);
        goto cleanup;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW && expr->a && expr->b && is_wrt(expr->a, wrt) &&
        expr_match_const_value(expr->b, &exponent)) {
        ok = num_eq(exponent, NUM_TWO);
        goto cleanup;
    }

    if (expr_match_mul_expr(expr, &left, &right) && is_wrt(left, wrt) && is_wrt(right, wrt)) {
        ok = true;
        goto cleanup;
    }

cleanup:
    num_destroy(&exponent);
    return ok;
}

static expr_t *match_symbolic_wrt_factor_coeff_primitives(const expr_t *expr, const expr_t *wrt)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;

    if (!expr || !wrt)
        return NULL;

    if (is_wrt(expr, wrt))
        return expr_new_const(NUM_ONE);

    if (expr_is_neg(expr))
        return expr_negate_owned(match_symbolic_wrt_factor_coeff_primitives(expr->a, wrt));

    if (expr_match_mul_expr(expr, &left, &right)) {
        expr_t *left_coeff = match_symbolic_wrt_factor_coeff_primitives(left, wrt);
        expr_t *right_coeff = match_symbolic_wrt_factor_coeff_primitives(right, wrt);

        if (left_coeff && !depends_on_wrt(right, wrt)) {
            expr_t *right_clone = expr_retain_expr(right);
            expr_t *product = right_clone ? expr_mul(left_coeff, right_clone) : NULL;

            expr_free(right_clone);
            expr_free(right_coeff);
            expr_free(left_coeff);
            return simplify_owned(product);
        }

        if (right_coeff && !depends_on_wrt(left, wrt)) {
            expr_t *left_clone = expr_retain_expr(left);
            expr_t *product = left_clone ? expr_mul(left_clone, right_coeff) : NULL;

            expr_free(left_clone);
            expr_free(right_coeff);
            expr_free(left_coeff);
            return simplify_owned(product);
        }

        expr_free(right_coeff);
        expr_free(left_coeff);

        if (is_wrt(left, wrt) && !depends_on_wrt(right, wrt))
            return expr_retain_expr(right);
        if (is_wrt(right, wrt) && !depends_on_wrt(left, wrt))
            return expr_retain_expr(left);
    }

    if (expr_is_op(expr, &ops_div) && expr->a && expr->b && !depends_on_wrt(expr->b, wrt)) {
        expr_t *numer_coeff = match_symbolic_wrt_factor_coeff_primitives(expr->a, wrt);
        expr_t *denom = expr_retain_expr(expr->b);
        expr_t *quotient = (numer_coeff && denom) ? expr_div(numer_coeff, denom) : NULL;

        expr_free(denom);
        expr_free(numer_coeff);
        return simplify_owned(quotient);
    }

    return NULL;
}

static expr_t *match_symbolic_wrt_square_coeff_primitives(const expr_t *expr, const expr_t *wrt)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;

    if (!expr || !wrt)
        return NULL;

    if (is_wrt_square_power_primitives(expr, wrt))
        return expr_new_const(NUM_ONE);

    if (expr_is_neg(expr))
        return expr_negate_owned(match_symbolic_wrt_square_coeff_primitives(expr->a, wrt));

    if (expr_match_mul_expr(expr, &left, &right)) {
        expr_t *left_coeff = match_symbolic_wrt_square_coeff_primitives(left, wrt);
        expr_t *right_coeff = match_symbolic_wrt_square_coeff_primitives(right, wrt);
        expr_t *left_linear = NULL;
        expr_t *right_linear = NULL;

        if (left_coeff && !depends_on_wrt(right, wrt)) {
            expr_t *right_clone = expr_retain_expr(right);
            expr_t *product = right_clone ? expr_mul(left_coeff, right_clone) : NULL;

            expr_free(right_clone);
            expr_free(right_coeff);
            expr_free(left_coeff);
            return simplify_owned(product);
        }

        if (right_coeff && !depends_on_wrt(left, wrt)) {
            expr_t *left_clone = expr_retain_expr(left);
            expr_t *product = left_clone ? expr_mul(left_clone, right_coeff) : NULL;

            expr_free(left_clone);
            expr_free(right_coeff);
            expr_free(left_coeff);
            return simplify_owned(product);
        }

        left_linear = match_symbolic_wrt_factor_coeff_primitives(left, wrt);
        right_linear = match_symbolic_wrt_factor_coeff_primitives(right, wrt);
        if (left_linear && right_linear) {
            expr_t *product = expr_mul(left_linear, right_linear);

            expr_free(right_linear);
            expr_free(left_linear);
            expr_free(right_coeff);
            expr_free(left_coeff);
            return simplify_owned(product);
        }

        expr_free(right_linear);
        expr_free(left_linear);
        expr_free(right_coeff);
        expr_free(left_coeff);
    }

    if (expr_is_op(expr, &ops_div) && expr->a && expr->b && !depends_on_wrt(expr->b, wrt)) {
        expr_t *numer_coeff = match_symbolic_wrt_square_coeff_primitives(expr->a, wrt);
        expr_t *denom = expr_retain_expr(expr->b);
        expr_t *quotient = (numer_coeff && denom) ? expr_div(numer_coeff, denom) : NULL;

        expr_free(denom);
        expr_free(numer_coeff);
        return simplify_owned(quotient);
    }

    return NULL;
}

static expr_t *integrate_exp_symbolic_proportional_wrt(const expr_t *expr, const expr_t *wrt)
{
    expr_t *coeff = NULL;
    expr_t *quotient = NULL;
    expr_t *out = NULL;

    if (!expr || !wrt || !expr->a || !expr_is_op(expr, &ops_exp))
        return NULL;

    coeff = match_symbolic_wrt_factor_coeff_primitives(expr->a, wrt);
    if (!coeff || expr_const_is_zero(coeff))
        goto cleanup;

    quotient = expr_div(expr, coeff);
    out = simplify_owned(quotient);
    quotient = NULL;

cleanup:
    expr_free(quotient);
    expr_free(coeff);
    return out;
}

static expr_t *integrate_exp_symbolic_square_wrt(const expr_t *expr, const expr_t *wrt)
{
    expr_t *coeff = NULL;
    expr_t *neg_coeff = NULL;
    expr_t *sqrt_neg_coeff = NULL;
    expr_t *arg = NULL;
    expr_t *erf_arg = NULL;
    expr_t *pi_const = NULL;
    expr_t *sqrt_pi = NULL;
    expr_t *numer = NULL;
    expr_t *two = NULL;
    expr_t *denom = NULL;
    expr_t *quotient = NULL;
    expr_t *out = NULL;

    if (!expr || !wrt || !expr->a || !expr_is_op(expr, &ops_exp))
        return NULL;

    coeff = match_symbolic_wrt_square_coeff_primitives(expr->a, wrt);
    if (!coeff || expr_const_is_zero(coeff))
        goto cleanup;

    neg_coeff = expr_neg(coeff);
    sqrt_neg_coeff = neg_coeff ? expr_sqrt(neg_coeff) : NULL;
    arg = (sqrt_neg_coeff && wrt) ? expr_mul(sqrt_neg_coeff, wrt) : NULL;
    erf_arg = arg ? expr_erf(arg) : NULL;
    pi_const = expr_new_named_const(NUM_PI, "@pi");
    sqrt_pi = pi_const ? expr_sqrt(pi_const) : NULL;
    numer = (sqrt_pi && erf_arg) ? expr_mul(sqrt_pi, erf_arg) : NULL;
    two = expr_new_const(NUM_TWO);
    denom = (two && sqrt_neg_coeff) ? expr_mul(two, sqrt_neg_coeff) : NULL;
    quotient = (numer && denom) ? expr_div(numer, denom) : NULL;
    out = simplify_owned(quotient);
    quotient = NULL;

cleanup:
    expr_free(quotient);
    expr_free(denom);
    expr_free(two);
    expr_free(numer);
    expr_free(sqrt_pi);
    expr_free(pi_const);
    expr_free(erf_arg);
    expr_free(arg);
    expr_free(sqrt_neg_coeff);
    expr_free(neg_coeff);
    expr_free(coeff);
    return out;
}

static expr_t *integrate_tanh_symbolic_proportional_wrt(const expr_t *expr, const expr_t *wrt)
{
    expr_t *coeff = NULL;
    expr_t *cosh_arg = NULL;
    expr_t *log_cosh = NULL;
    expr_t *quotient = NULL;
    expr_t *out = NULL;

    if (!expr || !wrt || !expr->a || !expr_is_op(expr, &ops_tanh))
        return NULL;

    coeff = match_symbolic_wrt_factor_coeff_primitives(expr->a, wrt);
    if (!coeff || expr_const_is_zero(coeff))
        goto cleanup;

    cosh_arg = expr_cosh(expr->a);
    log_cosh = cosh_arg ? expr_log(cosh_arg) : NULL;
    quotient = (log_cosh && coeff) ? expr_div(log_cosh, coeff) : NULL;
    out = simplify_owned(quotient);
    quotient = NULL;

cleanup:
    expr_free(quotient);
    expr_free(log_cosh);
    expr_free(cosh_arg);
    expr_free(coeff);
    return out;
}

static expr_t *integrate_sinh_symbolic_proportional_wrt(const expr_t *expr, const expr_t *wrt)
{
    expr_t *coeff = NULL;
    expr_t *cosh_arg = NULL;
    expr_t *quotient = NULL;
    expr_t *out = NULL;

    if (!expr || !wrt || !expr->a || !expr_is_op(expr, &ops_sinh))
        return NULL;

    coeff = match_symbolic_wrt_factor_coeff_primitives(expr->a, wrt);
    if (!coeff || expr_const_is_zero(coeff))
        goto cleanup;

    cosh_arg = expr_cosh(expr->a);
    quotient = (cosh_arg && coeff) ? expr_div(cosh_arg, coeff) : NULL;
    out = simplify_owned(quotient);
    quotient = NULL;

cleanup:
    expr_free(quotient);
    expr_free(cosh_arg);
    expr_free(coeff);
    return out;
}

static expr_t *integrate_cosh_symbolic_proportional_wrt(const expr_t *expr, const expr_t *wrt)
{
    expr_t *coeff = NULL;
    expr_t *sinh_arg = NULL;
    expr_t *quotient = NULL;
    expr_t *out = NULL;

    if (!expr || !wrt || !expr->a || !expr_is_op(expr, &ops_cosh))
        return NULL;

    coeff = match_symbolic_wrt_factor_coeff_primitives(expr->a, wrt);
    if (!coeff || expr_const_is_zero(coeff))
        goto cleanup;

    sinh_arg = expr_sinh(expr->a);
    quotient = (sinh_arg && coeff) ? expr_div(sinh_arg, coeff) : NULL;
    out = simplify_owned(quotient);
    quotient = NULL;

cleanup:
    expr_free(quotient);
    expr_free(sinh_arg);
    expr_free(coeff);
    return out;
}

expr_t *integrate_exp_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *out = integrate_affine_unary_kind(expr, wrt, EXPR_PATTERN_UNARY_EXP, expr_exp, NUM_ONE);

    if (out)
        return out;
    out = integrate_exp_of_negative_quadratic(expr, wrt);
    if (out)
        return out;
    out = integrate_exp_symbolic_proportional_wrt(expr, wrt);
    if (out)
        return out;
    return integrate_exp_symbolic_square_wrt(expr, wrt);
}

expr_t *integrate_sin_rule(const expr_t *expr, const expr_t *wrt)
{
    return integrate_affine_unary_kind(expr, wrt, EXPR_PATTERN_UNARY_SIN, expr_cos, NUM_NEG_ONE);
}

expr_t *integrate_cos_rule(const expr_t *expr, const expr_t *wrt)
{
    return integrate_affine_unary_kind(expr, wrt, EXPR_PATTERN_UNARY_COS, expr_sin, NUM_ONE);
}

expr_t *integrate_tan_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *vars[1];
    number_t constant = num_new();
    number_t coeffs[1];
    expr_t *cos_arg;
    expr_t *log_cos;
    expr_t *negated;
    expr_t *out;

    vars[0] = (expr_t *)wrt;
    coeffs[0] = num_new();
    if (!expr_match_unary_affine_kind(expr, EXPR_PATTERN_UNARY_TAN, 1u, vars, &constant, coeffs) ||
        num_eq(coeffs[0], NUM_ZERO)) {
        num_destroy(&coeffs[0]);
        num_destroy(&constant);
        return NULL;
    }

    cos_arg = expr_cos(expr->a);
    log_cos = cos_arg ? expr_log(cos_arg) : NULL;
    negated = log_cos ? expr_neg(log_cos) : NULL;
    out = div_number_owned(negated, coeffs[0]);

    expr_free(log_cos);
    expr_free(cos_arg);
    num_destroy(&coeffs[0]);
    num_destroy(&constant);
    return out;
}

expr_t *integrate_sec_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *vars[1];
    number_t constant = num_new();
    number_t coeffs[1];
    expr_t *sec_arg;
    expr_t *tan_arg;
    expr_t *sum;
    expr_t *log_sum;
    expr_t *out;

    vars[0] = (expr_t *)wrt;
    coeffs[0] = num_new();
    if (!expr_match_unary_affine_kind(expr, EXPR_PATTERN_UNARY_SEC, 1u, vars, &constant, coeffs) ||
        num_eq(coeffs[0], NUM_ZERO)) {
        num_destroy(&coeffs[0]);
        num_destroy(&constant);
        return NULL;
    }

    sec_arg = expr_sec(expr->a);
    tan_arg = expr_tan(expr->a);
    sum = (sec_arg && tan_arg) ? expr_add(sec_arg, tan_arg) : NULL;
    log_sum = sum ? expr_log(sum) : NULL;
    out = div_number_owned(log_sum, coeffs[0]);

    expr_free(sum);
    expr_free(tan_arg);
    expr_free(sec_arg);
    num_destroy(&coeffs[0]);
    num_destroy(&constant);
    return out;
}

expr_t *integrate_cosec_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *vars[1];
    number_t constant = num_new();
    number_t coeffs[1];
    expr_t *cosec_arg;
    expr_t *cot_arg;
    expr_t *sum;
    expr_t *log_sum;
    expr_t *negated;
    expr_t *out;

    vars[0] = (expr_t *)wrt;
    coeffs[0] = num_new();
    if (!expr_match_unary_affine_kind(expr, EXPR_PATTERN_UNARY_COSEC, 1u, vars, &constant, coeffs) ||
        num_eq(coeffs[0], NUM_ZERO)) {
        num_destroy(&coeffs[0]);
        num_destroy(&constant);
        return NULL;
    }

    cosec_arg = expr_cosec(expr->a);
    cot_arg = expr_cot(expr->a);
    sum = (cosec_arg && cot_arg) ? expr_add(cosec_arg, cot_arg) : NULL;
    log_sum = sum ? expr_log(sum) : NULL;
    negated = log_sum ? expr_neg(log_sum) : NULL;
    out = div_number_owned(negated, coeffs[0]);

    expr_free(log_sum);
    expr_free(sum);
    expr_free(cot_arg);
    expr_free(cosec_arg);
    num_destroy(&coeffs[0]);
    num_destroy(&constant);
    return out;
}

expr_t *integrate_cot_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *vars[1];
    number_t constant = num_new();
    number_t coeffs[1];
    expr_t *sin_arg;
    expr_t *log_sin;
    expr_t *out;

    vars[0] = (expr_t *)wrt;
    coeffs[0] = num_new();
    if (!expr_match_unary_affine_kind(expr, EXPR_PATTERN_UNARY_COT, 1u, vars, &constant, coeffs) ||
        num_eq(coeffs[0], NUM_ZERO)) {
        num_destroy(&coeffs[0]);
        num_destroy(&constant);
        return NULL;
    }

    sin_arg = expr_sin(expr->a);
    log_sin = sin_arg ? expr_log(sin_arg) : NULL;
    out = div_number_owned(log_sin, coeffs[0]);

    expr_free(sin_arg);
    num_destroy(&coeffs[0]);
    num_destroy(&constant);
    return out;
}

expr_t *integrate_sinh_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *out = integrate_affine_unary_kind(expr, wrt, EXPR_PATTERN_UNARY_SINH, expr_cosh, NUM_ONE);

    if (out)
        return out;
    return integrate_sinh_symbolic_proportional_wrt(expr, wrt);
}

expr_t *integrate_cosh_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *out = integrate_affine_unary_kind(expr, wrt, EXPR_PATTERN_UNARY_COSH, expr_sinh, NUM_ONE);

    if (out)
        return out;
    return integrate_cosh_symbolic_proportional_wrt(expr, wrt);
}

expr_t *integrate_cosech_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *vars[1];
    number_t constant = num_new();
    number_t coeffs[1];
    expr_t *half_arg;
    expr_t *tanh_half_arg;
    expr_t *log_term;
    expr_t *out;

    vars[0] = (expr_t *)wrt;
    coeffs[0] = num_new();
    if (!expr_match_unary_affine_kind(expr, EXPR_PATTERN_UNARY_COSECH, 1u, vars, &constant, coeffs) ||
        num_eq(coeffs[0], NUM_ZERO)) {
        num_destroy(&coeffs[0]);
        num_destroy(&constant);
        return NULL;
    }

    half_arg = expr_mul_num(expr->a, &NUM_HALF);
    tanh_half_arg = half_arg ? expr_tanh(half_arg) : NULL;
    log_term = tanh_half_arg ? expr_log(tanh_half_arg) : NULL;
    out = div_number_owned(log_term, coeffs[0]);

    expr_free(tanh_half_arg);
    expr_free(half_arg);
    num_destroy(&coeffs[0]);
    num_destroy(&constant);
    return out;
}

expr_t *integrate_tanh_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *vars[1];
    number_t constant = num_new();
    number_t coeffs[1];
    expr_t *cosh_arg;
    expr_t *log_cosh;
    expr_t *out;

    vars[0] = (expr_t *)wrt;
    coeffs[0] = num_new();
    if (!expr_match_unary_affine_kind(expr, EXPR_PATTERN_UNARY_TANH, 1u, vars, &constant, coeffs) ||
        num_eq(coeffs[0], NUM_ZERO)) {
        num_destroy(&coeffs[0]);
        num_destroy(&constant);
        return integrate_tanh_symbolic_proportional_wrt(expr, wrt);
    }

    cosh_arg = expr_cosh(expr->a);
    log_cosh = cosh_arg ? expr_log(cosh_arg) : NULL;
    out = div_number_owned(log_cosh, coeffs[0]);

    expr_free(cosh_arg);
    num_destroy(&coeffs[0]);
    num_destroy(&constant);
    return out;
}

expr_t *integrate_sech_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *vars[1];
    number_t constant = num_new();
    number_t coeffs[1];
    expr_t *sinh_arg;
    expr_t *atan_sinh;
    expr_t *out;

    vars[0] = (expr_t *)wrt;
    coeffs[0] = num_new();
    if (!expr_match_unary_affine_kind(expr, EXPR_PATTERN_UNARY_SECH, 1u, vars, &constant, coeffs) ||
        num_eq(coeffs[0], NUM_ZERO)) {
        num_destroy(&coeffs[0]);
        num_destroy(&constant);
        return NULL;
    }

    sinh_arg = expr_sinh(expr->a);
    atan_sinh = sinh_arg ? expr_atan(sinh_arg) : NULL;
    out = div_number_owned(atan_sinh, coeffs[0]);

    expr_free(sinh_arg);
    num_destroy(&coeffs[0]);
    num_destroy(&constant);
    return out;
}

expr_t *integrate_coth_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *vars[1];
    number_t constant = num_new();
    number_t coeffs[1];
    expr_t *sinh_arg;
    expr_t *log_sinh;
    expr_t *out;

    vars[0] = (expr_t *)wrt;
    coeffs[0] = num_new();
    if (!expr_match_unary_affine_kind(expr, EXPR_PATTERN_UNARY_COTH, 1u, vars, &constant, coeffs) ||
        num_eq(coeffs[0], NUM_ZERO)) {
        num_destroy(&coeffs[0]);
        num_destroy(&constant);
        return NULL;
    }

    sinh_arg = expr_sinh(expr->a);
    log_sinh = sinh_arg ? expr_log(sinh_arg) : NULL;
    out = div_number_owned(log_sinh, coeffs[0]);

    expr_free(sinh_arg);
    num_destroy(&coeffs[0]);
    num_destroy(&constant);
    return out;
}

expr_t *integrate_erf_rule(const expr_t *expr, const expr_t *wrt)
{
    number_t constant = num_new();
    number_t coeff = num_new();
    expr_t *u_erf_u;
    expr_t *u_sq;
    expr_t *neg_u_sq;
    expr_t *exp_term;
    expr_t *gaussian_term;
    expr_t *raw;

    if (!match_affine_unary(expr, wrt, EXPR_PATTERN_UNARY_ERF, &constant, &coeff)) {
        num_destroy(&coeff);
        num_destroy(&constant);
        return NULL;
    }

    u_erf_u = expr_mul(expr->a, expr);
    u_sq = expr_pow(expr->a, &NUM_TWO);
    neg_u_sq = u_sq ? expr_neg(u_sq) : NULL;
    exp_term = neg_u_sq ? expr_exp(neg_u_sq) : NULL;
    gaussian_term = exp_term ? expr_mul_num(exp_term, &NUM_SQRT1ONPI) : NULL;
    raw = (u_erf_u && gaussian_term) ? expr_add(u_erf_u, gaussian_term) : NULL;

    expr_free(gaussian_term);
    expr_free(exp_term);
    expr_free(neg_u_sq);
    expr_free(u_sq);
    expr_free(u_erf_u);
    num_destroy(&constant);
    return div_number_owned_consuming(raw, &coeff);
}

expr_t *integrate_erfc_rule(const expr_t *expr, const expr_t *wrt)
{
    number_t constant = num_new();
    number_t coeff = num_new();
    expr_t *u_erfc_u;
    expr_t *u_sq;
    expr_t *neg_u_sq;
    expr_t *exp_term;
    expr_t *gaussian_term;
    expr_t *raw;

    if (!match_affine_unary(expr, wrt, EXPR_PATTERN_UNARY_ERFC, &constant, &coeff)) {
        num_destroy(&coeff);
        num_destroy(&constant);
        return NULL;
    }

    u_erfc_u = expr_mul(expr->a, expr);
    u_sq = expr_pow(expr->a, &NUM_TWO);
    neg_u_sq = u_sq ? expr_neg(u_sq) : NULL;
    exp_term = neg_u_sq ? expr_exp(neg_u_sq) : NULL;
    gaussian_term = exp_term ? expr_mul_num(exp_term, &NUM_SQRT1ONPI) : NULL;
    raw = (u_erfc_u && gaussian_term) ? expr_sub(u_erfc_u, gaussian_term) : NULL;

    expr_free(gaussian_term);
    expr_free(exp_term);
    expr_free(neg_u_sq);
    expr_free(u_sq);
    expr_free(u_erfc_u);
    num_destroy(&constant);
    return div_number_owned_consuming(raw, &coeff);
}

expr_t *integrate_normal_pdf_rule(const expr_t *expr, const expr_t *wrt)
{
    return integrate_affine_unary_kind(expr, wrt, EXPR_PATTERN_UNARY_NORMAL_PDF, expr_normal_cdf, NUM_ONE);
}

expr_t *integrate_normal_cdf_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *vars[1];
    number_t constant = num_new();
    number_t coeffs[1];
    expr_t *u_phi;
    expr_t *phi;
    expr_t *sum;
    expr_t *out;

    vars[0] = (expr_t *)wrt;
    coeffs[0] = num_new();
    if (!expr_match_unary_affine_kind(expr, EXPR_PATTERN_UNARY_NORMAL_CDF, 1u, vars, &constant, coeffs) ||
        num_eq(coeffs[0], NUM_ZERO)) {
        num_destroy(&coeffs[0]);
        num_destroy(&constant);
        return NULL;
    }

    u_phi = expr_mul(expr->a, expr);
    phi = expr_normal_pdf(expr->a);
    sum = (u_phi && phi) ? expr_add(u_phi, phi) : NULL;
    out = div_number_owned(sum, coeffs[0]);

    expr_free(phi);
    expr_free(u_phi);
    num_destroy(&coeffs[0]);
    num_destroy(&constant);
    return out;
}

expr_t *integrate_normal_logpdf_rule(const expr_t *expr, const expr_t *wrt)
{
    number_t constant = num_new();
    number_t coeff = num_new();
    number_t neg_one_sixth = num_neg(NUM_ONE_SIXTH);
    number_t neg_log_sqrt_2pi = num_neg(NUM_LOG_SQRT_2PI);
    expr_t *u = NULL;
    expr_t *u_sq = NULL;
    expr_t *u_cu = NULL;
    expr_t *cubic_term = NULL;
    expr_t *linear_term = NULL;
    expr_t *raw = NULL;

    if (!match_affine_unary(expr, wrt, EXPR_PATTERN_UNARY_NORMAL_LOGPDF, &constant, &coeff)) {
        num_destroy(&neg_log_sqrt_2pi);
        num_destroy(&neg_one_sixth);
        num_destroy(&coeff);
        num_destroy(&constant);
        return NULL;
    }

    u = build_affine_from_match(wrt, constant, coeff);
    u_sq = u ? expr_pow(u, &NUM_TWO) : NULL;
    u_cu = (u && u_sq) ? expr_mul(u_sq, u) : NULL;
    cubic_term = u_cu ? expr_mul_num(u_cu, &neg_one_sixth) : NULL;
    linear_term = u ? expr_mul_num(u, &neg_log_sqrt_2pi) : NULL;
    raw = (cubic_term && linear_term) ? expr_add(cubic_term, linear_term) : NULL;

    expr_free(linear_term);
    expr_free(cubic_term);
    expr_free(u_cu);
    expr_free(u_sq);
    expr_free(u);
    num_destroy(&neg_log_sqrt_2pi);
    num_destroy(&neg_one_sixth);
    num_destroy(&constant);
    return div_number_owned_consuming(raw, &coeff);
}

expr_t *integrate_ei_rule(const expr_t *expr, const expr_t *wrt)
{
    number_t constant = num_new();
    number_t coeff = num_new();
    expr_t *u_ei_u;
    expr_t *exp_u;
    expr_t *raw;

    if (!match_affine_unary(expr, wrt, EXPR_PATTERN_UNARY_EI, &constant, &coeff)) {
        num_destroy(&coeff);
        num_destroy(&constant);
        return NULL;
    }

    u_ei_u = expr_mul(expr->a, expr);
    exp_u = expr_exp(expr->a);
    raw = (u_ei_u && exp_u) ? expr_sub(u_ei_u, exp_u) : NULL;

    expr_free(exp_u);
    expr_free(u_ei_u);
    num_destroy(&constant);
    return div_number_owned_consuming(raw, &coeff);
}

expr_t *integrate_e1_rule(const expr_t *expr, const expr_t *wrt)
{
    number_t constant = num_new();
    number_t coeff = num_new();
    expr_t *u_e1_u;
    expr_t *neg_u;
    expr_t *exp_neg_u;
    expr_t *raw;

    if (!match_affine_unary(expr, wrt, EXPR_PATTERN_UNARY_E1, &constant, &coeff)) {
        num_destroy(&coeff);
        num_destroy(&constant);
        return NULL;
    }

    u_e1_u = expr_mul(expr->a, expr);
    neg_u = expr_neg(expr->a);
    exp_neg_u = neg_u ? expr_exp(neg_u) : NULL;
    raw = (u_e1_u && exp_neg_u) ? expr_sub(u_e1_u, exp_neg_u) : NULL;

    expr_free(exp_neg_u);
    expr_free(neg_u);
    expr_free(u_e1_u);
    num_destroy(&constant);
    return div_number_owned_consuming(raw, &coeff);
}
