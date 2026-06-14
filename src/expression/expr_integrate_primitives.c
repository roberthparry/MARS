#include "expr_integrate_internal.h"

static bool is_wrt_square_power_primitives(const expr_t *expr, const expr_t *wrt)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    number_t exponent = num_new();
    bool ok = false;

    if (!expr || !wrt)
        goto cleanup;

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW_D &&
        expr->a && is_wrt(expr->a, wrt)) {
        ok = num_eq(expr->c, NUM_TWO);
        goto cleanup;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW &&
        expr->a && expr->b && is_wrt(expr->a, wrt) &&
        expr_match_const_value(expr->b, &exponent)) {
        ok = num_eq(exponent, NUM_TWO);
        goto cleanup;
    }

    if (expr_match_mul_expr(expr, &left, &right) &&
        is_wrt(left, wrt) && is_wrt(right, wrt)) {
        ok = true;
        goto cleanup;
    }

cleanup:
    num_destroy(&exponent);
    return ok;
}

static expr_t *match_symbolic_wrt_factor_coeff_primitives(const expr_t *expr,
                                                          const expr_t *wrt)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;

    if (!expr || !wrt)
        return NULL;

    if (is_wrt(expr, wrt))
        return expr_new_const(NUM_ONE);

    if (expr_is_neg(expr))
        return expr_negate_owned(
            match_symbolic_wrt_factor_coeff_primitives(expr->a, wrt));

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

    if (expr_is_op(expr, &ops_div) && expr->a && expr->b &&
        !depends_on_wrt(expr->b, wrt)) {
        expr_t *numer_coeff = match_symbolic_wrt_factor_coeff_primitives(expr->a, wrt);
        expr_t *denom = expr_retain_expr(expr->b);
        expr_t *quotient = (numer_coeff && denom) ? expr_div(numer_coeff, denom) : NULL;

        expr_free(denom);
        expr_free(numer_coeff);
        return simplify_owned(quotient);
    }

    return NULL;
}

static expr_t *match_symbolic_wrt_square_coeff_primitives(const expr_t *expr,
                                                          const expr_t *wrt)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;

    if (!expr || !wrt)
        return NULL;

    if (is_wrt_square_power_primitives(expr, wrt))
        return expr_new_const(NUM_ONE);

    if (expr_is_neg(expr))
        return expr_negate_owned(
            match_symbolic_wrt_square_coeff_primitives(expr->a, wrt));

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

    if (expr_is_op(expr, &ops_div) && expr->a && expr->b &&
        !depends_on_wrt(expr->b, wrt)) {
        expr_t *numer_coeff = match_symbolic_wrt_square_coeff_primitives(expr->a, wrt);
        expr_t *denom = expr_retain_expr(expr->b);
        expr_t *quotient = (numer_coeff && denom) ? expr_div(numer_coeff, denom) : NULL;

        expr_free(denom);
        expr_free(numer_coeff);
        return simplify_owned(quotient);
    }

    return NULL;
}

static expr_t *integrate_exp_symbolic_proportional_wrt(const expr_t *expr,
                                                       const expr_t *wrt)
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

static expr_t *integrate_exp_symbolic_square_wrt(const expr_t *expr,
                                                 const expr_t *wrt)
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

static expr_t *integrate_tanh_symbolic_proportional_wrt(const expr_t *expr,
                                                        const expr_t *wrt)
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

static expr_t *integrate_sinh_symbolic_proportional_wrt(const expr_t *expr,
                                                        const expr_t *wrt)
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

static expr_t *integrate_cosh_symbolic_proportional_wrt(const expr_t *expr,
                                                        const expr_t *wrt)
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
    expr_t *out = integrate_affine_unary_kind(expr, wrt, EXPR_PATTERN_UNARY_EXP,
                                             expr_exp, NUM_ONE);

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
    return integrate_affine_unary_kind(expr, wrt, EXPR_PATTERN_UNARY_SIN,
                                      expr_cos, NUM_NEG_ONE);
}

expr_t *integrate_cos_rule(const expr_t *expr, const expr_t *wrt)
{
    return integrate_affine_unary_kind(expr, wrt, EXPR_PATTERN_UNARY_COS,
                                      expr_sin, NUM_ONE);
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
    if (!expr_match_unary_affine_kind(expr, EXPR_PATTERN_UNARY_TAN, 1u, vars,
                                      &constant, coeffs) ||
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
    if (!expr_match_unary_affine_kind(expr, EXPR_PATTERN_UNARY_SEC, 1u, vars,
                                      &constant, coeffs) ||
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
    if (!expr_match_unary_affine_kind(expr, EXPR_PATTERN_UNARY_COSEC, 1u, vars,
                                      &constant, coeffs) ||
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
    if (!expr_match_unary_affine_kind(expr, EXPR_PATTERN_UNARY_COT, 1u, vars,
                                      &constant, coeffs) ||
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
    expr_t *out = integrate_affine_unary_kind(expr, wrt, EXPR_PATTERN_UNARY_SINH,
                                             expr_cosh, NUM_ONE);

    if (out)
        return out;
    return integrate_sinh_symbolic_proportional_wrt(expr, wrt);
}

expr_t *integrate_cosh_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *out = integrate_affine_unary_kind(expr, wrt, EXPR_PATTERN_UNARY_COSH,
                                             expr_sinh, NUM_ONE);

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
    if (!expr_match_unary_affine_kind(expr, EXPR_PATTERN_UNARY_COSECH, 1u, vars,
                                      &constant, coeffs) ||
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
    if (!expr_match_unary_affine_kind(expr, EXPR_PATTERN_UNARY_TANH, 1u, vars,
                                      &constant, coeffs) ||
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
    if (!expr_match_unary_affine_kind(expr, EXPR_PATTERN_UNARY_SECH, 1u, vars,
                                      &constant, coeffs) ||
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
    if (!expr_match_unary_affine_kind(expr, EXPR_PATTERN_UNARY_COTH, 1u, vars,
                                      &constant, coeffs) ||
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

    if (!match_affine_unary(expr, wrt, EXPR_PATTERN_UNARY_ERF,
                            &constant, &coeff)) {
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

    if (!match_affine_unary(expr, wrt, EXPR_PATTERN_UNARY_ERFC,
                            &constant, &coeff)) {
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
    return integrate_affine_unary_kind(expr, wrt, EXPR_PATTERN_UNARY_NORMAL_PDF,
                                      expr_normal_cdf, NUM_ONE);
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
    if (!expr_match_unary_affine_kind(expr, EXPR_PATTERN_UNARY_NORMAL_CDF, 1u,
                                      vars, &constant, coeffs) ||
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

    if (!match_affine_unary(expr, wrt, EXPR_PATTERN_UNARY_NORMAL_LOGPDF,
                            &constant, &coeff)) {
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

    if (!match_affine_unary(expr, wrt, EXPR_PATTERN_UNARY_EI,
                            &constant, &coeff)) {
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

    if (!match_affine_unary(expr, wrt, EXPR_PATTERN_UNARY_E1,
                            &constant, &coeff)) {
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
