#include <stdbool.h>

#include "expr_integrate_internal.h"

expr_t *expr_integrate_build_unsigned_expr_power(const expr_t *base,
                                                 unsigned int exponent)
{
    number_t n;
    expr_t *out;

    if (!base)
        return NULL;
    if (exponent == 0u)
        return expr_new_const(NUM_ONE);
    if (exponent == 1u) {
        expr_retain(base);
        return (expr_t *)base;
    }

    n = num_create_from_long((long)exponent);
    out = expr_pow(base, &n);
    num_destroy(&n);
    return out;
}

bool expr_integrate_number_matches_uint_at_most(number_t value,
                                                unsigned int max_value,
                                                unsigned int *out)
{
    for (unsigned int i = 0u; i <= max_value; ++i) {
        number_t candidate = num_create_from_long((long)i);
        bool ok = num_eq(value, candidate);

        num_destroy(&candidate);
        if (ok) {
            if (out)
                *out = i;
            return true;
        }
    }

    return false;
}

bool match_exp_proportional_wrt_coeff(const expr_t *expr,
                                      const expr_t *wrt,
                                      expr_t **coeff_out)
{
    expr_t *constant = NULL;
    expr_t *coeff = NULL;
    bool ok = false;

    if (!expr || !wrt || !coeff_out ||
        !expr->ops || expr->ops->kind != EXPR_KIND_EXP || !expr->a)
        return false;

    if (!match_symbolic_affine_constant_and_coeff(expr->a, wrt, &constant, &coeff))
        goto cleanup;
    if (!expr_const_is_zero(constant) || expr_const_is_zero(coeff))
        goto cleanup;

    *coeff_out = coeff;
    coeff = NULL;
    ok = true;

cleanup:
    expr_free(coeff);
    expr_free(constant);
    return ok;
}

bool expr_integrate_match_wrt_power_factor_exponent(const expr_t *expr,
                                                    const expr_t *wrt,
                                                    expr_t **exponent_out)
{
    if (!expr || !wrt || !exponent_out)
        return false;

    if (is_wrt(expr, wrt)) {
        *exponent_out = expr_new_const(NUM_ONE);
        return *exponent_out != NULL;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_SQRT &&
        expr->a && is_wrt(expr->a, wrt)) {
        *exponent_out = expr_new_const(NUM_HALF);
        return *exponent_out != NULL;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW_D &&
        expr->a && is_wrt(expr->a, wrt)) {
        *exponent_out = expr_new_const(expr->c);
        return *exponent_out != NULL;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW &&
        expr->a && expr->b && is_wrt(expr->a, wrt) &&
        !depends_on_wrt(expr->b, wrt)) {
        *exponent_out = expr_integrate_clone_expr(expr->b);
        return *exponent_out != NULL;
    }

    return false;
}

static bool match_power_exp_product(const expr_t *expr,
                                    const expr_t *wrt,
                                    const expr_t **power_out,
                                    const expr_t **exp_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    expr_t *exponent = NULL;
    expr_t *coeff = NULL;

    if (!expr || !wrt || !power_out || !exp_out ||
        !expr_match_mul_expr(expr, &left, &right))
        return false;

    if (expr_integrate_match_wrt_power_factor_exponent(left, wrt, &exponent) &&
        match_exp_proportional_wrt_coeff(right, wrt, &coeff)) {
        expr_free(coeff);
        expr_free(exponent);
        *power_out = left;
        *exp_out = right;
        return true;
    }
    expr_free(coeff);
    expr_free(exponent);
    coeff = NULL;
    exponent = NULL;

    if (expr_integrate_match_wrt_power_factor_exponent(right, wrt, &exponent) &&
        match_exp_proportional_wrt_coeff(left, wrt, &coeff)) {
        expr_free(coeff);
        expr_free(exponent);
        *power_out = right;
        *exp_out = left;
        return true;
    }

    expr_free(coeff);
    expr_free(exponent);
    return false;
}

static expr_t *build_symbolic_integer_power_exp_integral(unsigned int degree,
                                                         const expr_t *exp_expr,
                                                         const expr_t *coeff,
                                                         const expr_t *wrt)
{
    expr_t *sum = NULL;
    long falling = 1L;

    if (!exp_expr || !coeff || !wrt)
        return NULL;

    for (unsigned int k = 0u; k <= degree; ++k) {
        long signed_falling;
        expr_t *x_power = NULL;
        expr_t *coeff_power = NULL;
        expr_t *term = NULL;
        expr_t *exp_clone = NULL;
        expr_t *term_product = NULL;

        if (k > 0u)
            falling *= (long)(degree - k + 1u);
        signed_falling = (k % 2u) ? -falling : falling;

        if (degree == k && signed_falling != 1L) {
            number_t scale = num_create_from_long(signed_falling);

            x_power = expr_new_const(scale);
            num_destroy(&scale);
        } else {
            x_power = expr_integrate_build_unsigned_expr_power(wrt, degree - k);
        }
        if (x_power && degree != k && signed_falling != 1L) {
            number_t scale = num_create_from_long(signed_falling);

            x_power = mul_number_owned(x_power, scale);
            num_destroy(&scale);
        }
        coeff_power = expr_integrate_build_unsigned_expr_power(coeff, k + 1u);
        term = (x_power && coeff_power) ? expr_div(x_power, coeff_power) : NULL;
        term = simplify_owned(term);
        expr_retain(exp_expr);
        exp_clone = (expr_t *)exp_expr;
        term_product = (term && exp_clone) ? expr_mul(term, exp_clone) : NULL;
        term_product = simplify_owned(term_product);

        if (term_product) {
            sum = sum ? expr_integrate_add_terms_owned(sum, term_product) : term_product;
            term_product = NULL;
        }

        expr_free(term_product);
        expr_free(exp_clone);
        expr_free(term);
        expr_free(coeff_power);
        expr_free(x_power);
    }

    return simplify_owned(sum);
}

expr_t *integrate_symbolic_integer_power_times_exp(const expr_t *expr,
                                                          const expr_t *wrt)
{
    const expr_t *power_expr = NULL;
    const expr_t *exp_expr = NULL;
    expr_t *exponent_expr = NULL;
    expr_t *coeff = NULL;
    number_t exponent = num_new();
    expr_t *out = NULL;
    unsigned int degree = 0u;

    if (!match_power_exp_product(expr, wrt, &power_expr, &exp_expr))
        goto cleanup;
    if (!expr_integrate_match_wrt_power_factor_exponent(power_expr, wrt, &exponent_expr) ||
        !expr_match_const_value(exponent_expr, &exponent) ||
        !expr_integrate_number_matches_uint_at_most(exponent, 4u, &degree) ||
        !match_exp_proportional_wrt_coeff(exp_expr, wrt, &coeff))
        goto cleanup;

    out = build_symbolic_integer_power_exp_integral(degree, exp_expr, coeff, wrt);

cleanup:
    num_destroy(&exponent);
    expr_free(coeff);
    expr_free(exponent_expr);
    return out;
}

expr_t *integrate_symbolic_power_times_exp_gamma(const expr_t *expr,
                                                        const expr_t *wrt)
{
    const expr_t *power_expr = NULL;
    const expr_t *exp_expr = NULL;
    expr_t *exponent = NULL;
    expr_t *coeff = NULL;
    expr_t *next_exponent = NULL;
    expr_t *neg_coeff = NULL;
    expr_t *arg = NULL;
    expr_t *gamma = NULL;
    expr_t *log_neg_coeff = NULL;
    expr_t *denom_exponent = NULL;
    expr_t *denominator = NULL;
    expr_t *quotient = NULL;
    expr_t *out = NULL;
    number_t exponent_value = num_new();

    if (!match_power_exp_product(expr, wrt, &power_expr, &exp_expr) ||
        !expr_integrate_match_wrt_power_factor_exponent(power_expr, wrt, &exponent) ||
        !expr_match_const_value(exponent, &exponent_value) ||
        !match_exp_proportional_wrt_coeff(exp_expr, wrt, &coeff))
        goto cleanup;

    next_exponent = expr_add_num(exponent, &NUM_ONE);
    neg_coeff = coeff ? expr_neg(coeff) : NULL;
    arg = (neg_coeff && wrt) ? expr_mul(neg_coeff, wrt) : NULL;
    gamma = (next_exponent && arg) ? expr_gammainc_lower(next_exponent, arg) : NULL;
    log_neg_coeff = neg_coeff ? expr_log(neg_coeff) : NULL;
    denom_exponent = (next_exponent && log_neg_coeff)
        ? expr_mul(next_exponent, log_neg_coeff)
        : NULL;
    denominator = denom_exponent ? expr_exp(denom_exponent) : NULL;
    quotient = (gamma && denominator) ? expr_div(gamma, denominator) : NULL;
    out = simplify_owned(quotient);
    quotient = NULL;

cleanup:
    expr_free(quotient);
    expr_free(denominator);
    expr_free(denom_exponent);
    expr_free(log_neg_coeff);
    expr_free(gamma);
    expr_free(arg);
    expr_free(neg_coeff);
    expr_free(next_exponent);
    expr_free(coeff);
    expr_free(exponent);
    num_destroy(&exponent_value);
    return out;
}
