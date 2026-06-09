#include <stdbool.h>

#include "expr_integrate_internal.h"

bool match_trig_proportional_wrt_coeff(const expr_t *expr,
                                       const expr_t *wrt,
                                       bool *is_sin_out,
                                       expr_t **coeff_out)
{
    expr_t *constant = NULL;
    expr_t *coeff = NULL;
    bool ok = false;

    if (!expr || !wrt || !is_sin_out || !coeff_out || !expr->a)
        return false;

    if (expr_is_op(expr, &ops_sin)) {
        *is_sin_out = true;
    } else if (expr_is_op(expr, &ops_cos)) {
        *is_sin_out = false;
    } else {
        return false;
    }

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

static bool match_power_trig_product(const expr_t *expr,
                                     const expr_t *wrt,
                                     const expr_t **power_out,
                                     const expr_t **trig_out,
                                     bool *is_sin_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    expr_t *exponent = NULL;
    expr_t *coeff = NULL;

    if (!expr || !wrt || !power_out || !trig_out || !is_sin_out ||
        !expr_match_mul_expr(expr, &left, &right))
        return false;

    if (expr_integrate_match_wrt_power_factor_exponent(left, wrt, &exponent) &&
        match_trig_proportional_wrt_coeff(right, wrt, is_sin_out, &coeff)) {
        expr_free(coeff);
        expr_free(exponent);
        *power_out = left;
        *trig_out = right;
        return true;
    }
    expr_free(coeff);
    expr_free(exponent);
    coeff = NULL;
    exponent = NULL;

    if (expr_integrate_match_wrt_power_factor_exponent(right, wrt, &exponent) &&
        match_trig_proportional_wrt_coeff(left, wrt, is_sin_out, &coeff)) {
        expr_free(coeff);
        expr_free(exponent);
        *power_out = right;
        *trig_out = left;
        return true;
    }

    expr_free(coeff);
    expr_free(exponent);
    return false;
}

static expr_t *build_symbolic_quadratic_power_trig_integral(bool integrand_is_sin,
                                                            const expr_t *trig_expr,
                                                            const expr_t *coeff,
                                                            const expr_t *wrt)
{
    expr_t *sin_v = NULL;
    expr_t *cos_v = NULL;
    expr_t *x_sq = NULL;
    expr_t *coeff_sq = NULL;
    expr_t *coeff_cubed = NULL;
    expr_t *a2x2 = NULL;
    expr_t *two = NULL;
    expr_t *bracket = NULL;
    expr_t *lead_product = NULL;
    expr_t *term1 = NULL;
    expr_t *x_trig = NULL;
    expr_t *two_x_trig = NULL;
    expr_t *term2 = NULL;
    expr_t *sum = NULL;

    if (!trig_expr || !trig_expr->a || !coeff || !wrt)
        return NULL;

    sin_v = expr_sin(trig_expr->a);
    cos_v = expr_cos(trig_expr->a);
    x_sq = expr_integrate_build_unsigned_expr_power(wrt, 2u);
    coeff_sq = expr_integrate_build_unsigned_expr_power(coeff, 2u);
    coeff_cubed = expr_integrate_build_unsigned_expr_power(coeff, 3u);
    a2x2 = (coeff_sq && x_sq) ? expr_mul(coeff_sq, x_sq) : NULL;
    two = expr_new_const(NUM_TWO);
    bracket = integrand_is_sin
        ? ((two && a2x2) ? expr_sub(two, a2x2) : NULL)
        : ((a2x2 && two) ? expr_sub(a2x2, two) : NULL);
    lead_product = (bracket && (integrand_is_sin ? cos_v : sin_v))
        ? expr_mul(bracket, integrand_is_sin ? cos_v : sin_v)
        : NULL;
    term1 = (lead_product && coeff_cubed) ? expr_div(lead_product, coeff_cubed) : NULL;

    x_trig = (wrt && (integrand_is_sin ? sin_v : cos_v))
        ? expr_mul(wrt, integrand_is_sin ? sin_v : cos_v)
        : NULL;
    two_x_trig = x_trig ? expr_mul_num(x_trig, &NUM_TWO) : NULL;
    term2 = (two_x_trig && coeff_sq) ? expr_div(two_x_trig, coeff_sq) : NULL;
    sum = expr_integrate_add_terms_owned(term1, term2);
    term1 = NULL;
    term2 = NULL;

    expr_free(term2);
    expr_free(two_x_trig);
    expr_free(x_trig);
    expr_free(term1);
    expr_free(lead_product);
    expr_free(bracket);
    expr_free(two);
    expr_free(a2x2);
    expr_free(coeff_cubed);
    expr_free(coeff_sq);
    expr_free(x_sq);
    expr_free(cos_v);
    expr_free(sin_v);
    return simplify_owned(sum);
}

static expr_t *build_symbolic_integer_power_trig_integral(unsigned int degree,
                                                          bool integrand_is_sin,
                                                          const expr_t *trig_expr,
                                                          const expr_t *coeff,
                                                          const expr_t *wrt)
{
    expr_t *x_power = NULL;
    expr_t *trig_part = NULL;
    expr_t *product = NULL;
    expr_t *term1 = NULL;
    expr_t *inner = NULL;
    expr_t *scaled_inner = NULL;
    expr_t *term2 = NULL;
    expr_t *sum = NULL;

    if (!trig_expr || !trig_expr->a || !coeff || !wrt)
        return NULL;

    if (degree == 2u) {
        expr_t *quadratic = build_symbolic_quadratic_power_trig_integral(integrand_is_sin,
                                                                         trig_expr,
                                                                         coeff,
                                                                         wrt);

        if (quadratic)
            return quadratic;
    }

    x_power = expr_integrate_build_unsigned_expr_power(wrt, degree);
    trig_part = integrand_is_sin ? expr_cos(trig_expr->a) : expr_sin(trig_expr->a);
    product = (x_power && trig_part) ? expr_mul(x_power, trig_part) : NULL;
    term1 = (product && coeff) ? expr_div(product, coeff) : NULL;
    if (integrand_is_sin)
        term1 = expr_integrate_negate_owned(term1);
    term1 = simplify_owned(term1);

    if (degree == 0u) {
        expr_free(product);
        expr_free(trig_part);
        expr_free(x_power);
        return term1;
    }

    inner = build_symbolic_integer_power_trig_integral(degree - 1u,
                                                       !integrand_is_sin,
                                                       trig_expr,
                                                       coeff,
                                                       wrt);
    if (inner) {
        number_t scale = num_create_from_long((long)degree);

        scaled_inner = expr_mul_num(inner, &scale);
        num_destroy(&scale);
    }
    term2 = (scaled_inner && coeff) ? expr_div(scaled_inner, coeff) : NULL;
    if (!integrand_is_sin)
        term2 = expr_integrate_negate_owned(term2);
    term2 = simplify_owned(term2);
    sum = expr_integrate_add_terms_owned(term1, term2);
    term1 = NULL;
    term2 = NULL;

    expr_free(term2);
    expr_free(scaled_inner);
    expr_free(inner);
    expr_free(term1);
    expr_free(product);
    expr_free(trig_part);
    expr_free(x_power);
    return simplify_owned(sum);
}

expr_t *integrate_symbolic_integer_power_times_trig(const expr_t *expr,
                                                           const expr_t *wrt)
{
    const expr_t *power_expr = NULL;
    const expr_t *trig_expr = NULL;
    expr_t *exponent_expr = NULL;
    expr_t *coeff = NULL;
    number_t exponent = num_new();
    expr_t *out = NULL;
    unsigned int degree = 0u;
    bool is_sin = false;

    if (!match_power_trig_product(expr, wrt, &power_expr, &trig_expr, &is_sin))
        goto cleanup;
    if (!expr_integrate_match_wrt_power_factor_exponent(power_expr, wrt, &exponent_expr) ||
        !expr_match_const_value(exponent_expr, &exponent) ||
        !expr_integrate_number_matches_uint_at_most(exponent, 4u, &degree) ||
        !match_trig_proportional_wrt_coeff(trig_expr, wrt, &is_sin, &coeff))
        goto cleanup;

    out = build_symbolic_integer_power_trig_integral(degree, is_sin, trig_expr, coeff, wrt);

cleanup:
    num_destroy(&exponent);
    expr_free(coeff);
    expr_free(exponent_expr);
    return out;
}

static bool match_exp_trig_product(const expr_t *expr,
                                   const expr_t *wrt,
                                   const expr_t **exp_out,
                                   const expr_t **trig_out,
                                   bool *is_sin_out,
                                   expr_t **exp_coeff_out,
                                   expr_t **trig_coeff_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    expr_t *exp_coeff = NULL;
    expr_t *trig_coeff = NULL;

    if (!expr || !wrt || !exp_out || !trig_out || !is_sin_out ||
        !exp_coeff_out || !trig_coeff_out ||
        !expr_match_mul_expr(expr, &left, &right))
        return false;

    if (match_exp_proportional_wrt_coeff(left, wrt, &exp_coeff) &&
        match_trig_proportional_wrt_coeff(right, wrt, is_sin_out, &trig_coeff)) {
        *exp_out = left;
        *trig_out = right;
        *exp_coeff_out = exp_coeff;
        *trig_coeff_out = trig_coeff;
        return true;
    }
    expr_free(exp_coeff);
    expr_free(trig_coeff);
    exp_coeff = NULL;
    trig_coeff = NULL;

    if (match_exp_proportional_wrt_coeff(right, wrt, &exp_coeff) &&
        match_trig_proportional_wrt_coeff(left, wrt, is_sin_out, &trig_coeff)) {
        *exp_out = right;
        *trig_out = left;
        *exp_coeff_out = exp_coeff;
        *trig_coeff_out = trig_coeff;
        return true;
    }

    expr_free(exp_coeff);
    expr_free(trig_coeff);
    return false;
}

expr_t *integrate_symbolic_exp_times_trig(const expr_t *expr, const expr_t *wrt)
{
    const expr_t *exp_expr = NULL;
    const expr_t *trig_expr = NULL;
    expr_t *exp_coeff = NULL;
    expr_t *trig_coeff = NULL;
    expr_t *sin_v = NULL;
    expr_t *cos_v = NULL;
    expr_t *left = NULL;
    expr_t *right = NULL;
    expr_t *sum = NULL;
    expr_t *product = NULL;
    expr_t *exp_coeff_sq = NULL;
    expr_t *trig_coeff_sq = NULL;
    expr_t *denom = NULL;
    expr_t *quotient = NULL;
    expr_t *out = NULL;
    bool is_sin = false;

    if (!match_exp_trig_product(expr, wrt, &exp_expr, &trig_expr, &is_sin,
                                &exp_coeff, &trig_coeff))
        goto cleanup;

    sin_v = trig_expr && trig_expr->a ? expr_sin(trig_expr->a) : NULL;
    cos_v = trig_expr && trig_expr->a ? expr_cos(trig_expr->a) : NULL;
    if (is_sin) {
        left = (exp_coeff && sin_v) ? expr_mul(exp_coeff, sin_v) : NULL;
        right = (trig_coeff && cos_v) ? expr_mul(trig_coeff, cos_v) : NULL;
        sum = (left && right) ? expr_sub(left, right) : NULL;
    } else {
        left = (trig_coeff && sin_v) ? expr_mul(trig_coeff, sin_v) : NULL;
        right = (exp_coeff && cos_v) ? expr_mul(exp_coeff, cos_v) : NULL;
        sum = (left && right) ? expr_add(left, right) : NULL;
    }

    product = (exp_expr && sum) ? expr_mul(exp_expr, sum) : NULL;
    exp_coeff_sq = exp_coeff ? expr_pow(exp_coeff, &NUM_TWO) : NULL;
    trig_coeff_sq = trig_coeff ? expr_pow(trig_coeff, &NUM_TWO) : NULL;
    denom = (exp_coeff_sq && trig_coeff_sq) ? expr_add(exp_coeff_sq, trig_coeff_sq) : NULL;
    quotient = (product && denom) ? expr_div(product, denom) : NULL;
    out = simplify_owned(quotient);
    quotient = NULL;

cleanup:
    expr_free(quotient);
    expr_free(denom);
    expr_free(trig_coeff_sq);
    expr_free(exp_coeff_sq);
    expr_free(product);
    expr_free(sum);
    expr_free(right);
    expr_free(left);
    expr_free(cos_v);
    expr_free(sin_v);
    expr_free(trig_coeff);
    expr_free(exp_coeff);
    return out;
}

typedef struct {
    bool has_wrt;
    bool has_exp;
    bool has_trig;
    bool trig_is_sin;
} exact_wrt_exp_trig_match_t;

static bool match_wrt_exp_trig_exact_rec(const expr_t *expr,
                                         const expr_t *wrt,
                                         exact_wrt_exp_trig_match_t *match)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;

    if (!expr || !wrt || !match)
        return false;

    if (expr_match_mul_expr(expr, &left, &right))
        return match_wrt_exp_trig_exact_rec(left, wrt, match) &&
               match_wrt_exp_trig_exact_rec(right, wrt, match);

    if (is_wrt(expr, wrt)) {
        if (match->has_wrt)
            return false;
        match->has_wrt = true;
        return true;
    }

    if (expr_is_op(expr, &ops_exp) && expr->a && is_wrt(expr->a, wrt)) {
        if (match->has_exp)
            return false;
        match->has_exp = true;
        return true;
    }

    if ((expr_is_op(expr, &ops_sin) || expr_is_op(expr, &ops_cos)) &&
        expr->a && is_wrt(expr->a, wrt)) {
        if (match->has_trig)
            return false;
        match->has_trig = true;
        match->trig_is_sin = expr_is_op(expr, &ops_sin);
        return true;
    }

    return false;
}

expr_t *integrate_wrt_exp_times_trig_exact(const expr_t *expr, const expr_t *wrt)
{
    exact_wrt_exp_trig_match_t match = { false, false, false, false };
    expr_t *exp_x = NULL;
    expr_t *sin_x = NULL;
    expr_t *cos_x = NULL;
    expr_t *x_sin = NULL;
    expr_t *x_cos = NULL;
    expr_t *first = NULL;
    expr_t *bracket = NULL;
    expr_t *product = NULL;
    expr_t *out = NULL;

    if (!match_wrt_exp_trig_exact_rec(expr, wrt, &match) ||
        !match.has_wrt || !match.has_exp || !match.has_trig)
        goto cleanup;

    exp_x = expr_exp(wrt);
    sin_x = expr_sin(wrt);
    cos_x = expr_cos(wrt);
    x_sin = (wrt && sin_x) ? expr_mul(wrt, sin_x) : NULL;
    x_cos = (wrt && cos_x) ? expr_mul(wrt, cos_x) : NULL;
    if (match.trig_is_sin) {
        first = (cos_x && x_cos) ? expr_sub(cos_x, x_cos) : NULL;
        bracket = (first && x_sin) ? expr_add(first, x_sin) : NULL;
    } else {
        first = (x_cos && sin_x) ? expr_sub(x_cos, sin_x) : NULL;
        bracket = (first && x_sin) ? expr_add(first, x_sin) : NULL;
    }
    product = (exp_x && bracket) ? expr_mul(exp_x, bracket) : NULL;
    out = product ? mul_number_owned(product, NUM_HALF) : NULL;
    product = NULL;
    out = simplify_owned(out);

cleanup:
    expr_free(product);
    expr_free(bracket);
    expr_free(first);
    expr_free(x_cos);
    expr_free(x_sin);
    expr_free(cos_x);
    expr_free(sin_x);
    expr_free(exp_x);
    return out;
}
