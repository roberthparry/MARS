#include "expr_integrate_internal.h"

expr_t *integrate_exp_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *out = integrate_affine_unary_kind(expr, wrt, EXPR_PATTERN_UNARY_EXP,
                                             expr_exp, NUM_ONE);

    if (out)
        return out;
    return integrate_exp_of_negative_quadratic(expr, wrt);
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
    return integrate_affine_unary_kind(expr, wrt, EXPR_PATTERN_UNARY_SINH,
                                      expr_cosh, NUM_ONE);
}

expr_t *integrate_cosh_rule(const expr_t *expr, const expr_t *wrt)
{
    return integrate_affine_unary_kind(expr, wrt, EXPR_PATTERN_UNARY_COSH,
                                      expr_sinh, NUM_ONE);
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
        return NULL;
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

expr_t *integrate_asec_rule(const expr_t *expr, const expr_t *wrt)
{
    number_t constant = num_new();
    number_t coeff = num_new();
    expr_t *u_asec_u;
    expr_t *acosh_u;
    expr_t *raw;

    if (!match_affine_unary(expr, wrt, EXPR_PATTERN_UNARY_ASEC,
                            &constant, &coeff)) {
        num_destroy(&coeff);
        num_destroy(&constant);
        return NULL;
    }

    u_asec_u = expr_mul(expr->a, expr);
    acosh_u = expr_acosh(expr->a);
    raw = (u_asec_u && acosh_u) ? expr_sub(u_asec_u, acosh_u) : NULL;

    expr_free(acosh_u);
    expr_free(u_asec_u);
    num_destroy(&constant);
    return div_number_owned_consuming(raw, &coeff);
}

expr_t *integrate_acosec_rule(const expr_t *expr, const expr_t *wrt)
{
    number_t constant = num_new();
    number_t coeff = num_new();
    expr_t *u_acosec_u;
    expr_t *acosh_u;
    expr_t *raw;

    if (!match_affine_unary(expr, wrt, EXPR_PATTERN_UNARY_ACOSEC,
                            &constant, &coeff)) {
        num_destroy(&coeff);
        num_destroy(&constant);
        return NULL;
    }

    u_acosec_u = expr_mul(expr->a, expr);
    acosh_u = expr_acosh(expr->a);
    raw = (u_acosec_u && acosh_u) ? expr_add(u_acosec_u, acosh_u) : NULL;

    expr_free(acosh_u);
    expr_free(u_acosec_u);
    num_destroy(&constant);
    return div_number_owned_consuming(raw, &coeff);
}

expr_t *integrate_acot_rule(const expr_t *expr, const expr_t *wrt)
{
    number_t constant = num_new();
    number_t coeff = num_new();
    expr_t *u_acot_u;
    expr_t *u_sq;
    expr_t *one_plus_u_sq;
    expr_t *log_term;
    expr_t *half_log_term;
    expr_t *raw;

    if (!match_affine_unary(expr, wrt, EXPR_PATTERN_UNARY_ACOT,
                            &constant, &coeff)) {
        num_destroy(&coeff);
        num_destroy(&constant);
        return NULL;
    }

    u_acot_u = expr_mul(expr->a, expr);
    u_sq = expr_pow(expr->a, &NUM_TWO);
    one_plus_u_sq = u_sq ? expr_add_num(u_sq, &NUM_ONE) : NULL;
    log_term = one_plus_u_sq ? expr_log(one_plus_u_sq) : NULL;
    half_log_term = log_term ? expr_mul_num(log_term, &NUM_HALF) : NULL;
    raw = (u_acot_u && half_log_term) ? expr_add(u_acot_u, half_log_term) : NULL;

    expr_free(half_log_term);
    expr_free(log_term);
    expr_free(one_plus_u_sq);
    expr_free(u_sq);
    expr_free(u_acot_u);
    num_destroy(&constant);
    return div_number_owned_consuming(raw, &coeff);
}

expr_t *integrate_asin_rule(const expr_t *expr, const expr_t *wrt)
{
    number_t constant = num_new();
    number_t coeff = num_new();
    expr_t *u_asin_u;
    expr_t *u_sq;
    expr_t *one_minus_u_sq;
    expr_t *root;
    expr_t *raw;

    if (!match_affine_unary(expr, wrt, EXPR_PATTERN_UNARY_ASIN,
                            &constant, &coeff)) {
        num_destroy(&coeff);
        num_destroy(&constant);
        return NULL;
    }

    u_asin_u = expr_mul(expr->a, expr);
    u_sq = expr_pow(expr->a, &NUM_TWO);
    one_minus_u_sq = u_sq ? expr_sub_num(u_sq, &NUM_ONE) : NULL;
    root = one_minus_u_sq ? expr_neg(one_minus_u_sq) : NULL;
    if (root) {
        expr_t *tmp = expr_sqrt(root);
        expr_free(root);
        root = tmp;
    }
    raw = (u_asin_u && root) ? expr_add(u_asin_u, root) : NULL;

    expr_free(root);
    expr_free(one_minus_u_sq);
    expr_free(u_sq);
    expr_free(u_asin_u);
    num_destroy(&constant);
    return div_number_owned_consuming(raw, &coeff);
}

expr_t *integrate_acos_rule(const expr_t *expr, const expr_t *wrt)
{
    number_t constant = num_new();
    number_t coeff = num_new();
    expr_t *u_acos_u;
    expr_t *u_sq;
    expr_t *one_minus_u_sq;
    expr_t *root;
    expr_t *raw;

    if (!match_affine_unary(expr, wrt, EXPR_PATTERN_UNARY_ACOS,
                            &constant, &coeff)) {
        num_destroy(&coeff);
        num_destroy(&constant);
        return NULL;
    }

    u_acos_u = expr_mul(expr->a, expr);
    u_sq = expr_pow(expr->a, &NUM_TWO);
    one_minus_u_sq = u_sq ? expr_sub_num(u_sq, &NUM_ONE) : NULL;
    root = one_minus_u_sq ? expr_neg(one_minus_u_sq) : NULL;
    if (root) {
        expr_t *tmp = expr_sqrt(root);
        expr_free(root);
        root = tmp;
    }
    raw = (u_acos_u && root) ? expr_sub(u_acos_u, root) : NULL;

    expr_free(root);
    expr_free(one_minus_u_sq);
    expr_free(u_sq);
    expr_free(u_acos_u);
    num_destroy(&constant);
    return div_number_owned_consuming(raw, &coeff);
}

expr_t *integrate_atan_rule(const expr_t *expr, const expr_t *wrt)
{
    number_t constant = num_new();
    number_t coeff = num_new();
    expr_t *u_atan_u;
    expr_t *u_sq;
    expr_t *one_plus_u_sq;
    expr_t *log_term;
    expr_t *half_log_term;
    expr_t *raw;

    if (!match_affine_unary(expr, wrt, EXPR_PATTERN_UNARY_ATAN,
                            &constant, &coeff)) {
        num_destroy(&coeff);
        num_destroy(&constant);
        return NULL;
    }

    u_atan_u = expr_mul(expr->a, expr);
    u_sq = expr_pow(expr->a, &NUM_TWO);
    one_plus_u_sq = u_sq ? expr_add_num(u_sq, &NUM_ONE) : NULL;
    log_term = one_plus_u_sq ? expr_log(one_plus_u_sq) : NULL;
    half_log_term = log_term ? expr_mul_num(log_term, &NUM_HALF) : NULL;
    raw = (u_atan_u && half_log_term) ? expr_sub(u_atan_u, half_log_term) : NULL;

    expr_free(half_log_term);
    expr_free(log_term);
    expr_free(one_plus_u_sq);
    expr_free(u_sq);
    expr_free(u_atan_u);
    num_destroy(&constant);
    return div_number_owned_consuming(raw, &coeff);
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

expr_t *integrate_asinh_rule(const expr_t *expr, const expr_t *wrt)
{
    number_t constant = num_new();
    number_t coeff = num_new();
    expr_t *u_asinh_u;
    expr_t *u_sq;
    expr_t *one_plus_u_sq;
    expr_t *root;
    expr_t *raw;

    if (!match_affine_unary(expr, wrt, EXPR_PATTERN_UNARY_ASINH,
                            &constant, &coeff)) {
        num_destroy(&coeff);
        num_destroy(&constant);
        return NULL;
    }

    u_asinh_u = expr_mul(expr->a, expr);
    u_sq = expr_pow(expr->a, &NUM_TWO);
    one_plus_u_sq = u_sq ? expr_add_num(u_sq, &NUM_ONE) : NULL;
    root = one_plus_u_sq ? expr_sqrt(one_plus_u_sq) : NULL;
    raw = (u_asinh_u && root) ? expr_sub(u_asinh_u, root) : NULL;

    expr_free(root);
    expr_free(one_plus_u_sq);
    expr_free(u_sq);
    expr_free(u_asinh_u);
    num_destroy(&constant);
    return div_number_owned_consuming(raw, &coeff);
}

expr_t *integrate_acosh_rule(const expr_t *expr, const expr_t *wrt)
{
    number_t constant = num_new();
    number_t coeff = num_new();
    expr_t *u_acosh_u;
    expr_t *u_minus_one;
    expr_t *u_plus_one;
    expr_t *sqrt1;
    expr_t *sqrt2;
    expr_t *root_product;
    expr_t *raw;

    if (!match_affine_unary(expr, wrt, EXPR_PATTERN_UNARY_ACOSH,
                            &constant, &coeff)) {
        num_destroy(&coeff);
        num_destroy(&constant);
        return NULL;
    }

    u_acosh_u = expr_mul(expr->a, expr);
    u_minus_one = expr_sub_num(expr->a, &NUM_ONE);
    u_plus_one = expr_add_num(expr->a, &NUM_ONE);
    sqrt1 = u_minus_one ? expr_sqrt(u_minus_one) : NULL;
    sqrt2 = u_plus_one ? expr_sqrt(u_plus_one) : NULL;
    root_product = (sqrt1 && sqrt2) ? expr_mul(sqrt1, sqrt2) : NULL;
    raw = (u_acosh_u && root_product) ? expr_sub(u_acosh_u, root_product) : NULL;

    expr_free(root_product);
    expr_free(sqrt2);
    expr_free(sqrt1);
    expr_free(u_plus_one);
    expr_free(u_minus_one);
    expr_free(u_acosh_u);
    num_destroy(&constant);
    return div_number_owned_consuming(raw, &coeff);
}

expr_t *integrate_atanh_rule(const expr_t *expr, const expr_t *wrt)
{
    number_t constant = num_new();
    number_t coeff = num_new();
    expr_t *u_atanh_u;
    expr_t *u_sq;
    expr_t *one_minus_u_sq;
    expr_t *log_term;
    expr_t *half_log_term;
    expr_t *raw;

    if (!match_affine_unary(expr, wrt, EXPR_PATTERN_UNARY_ATANH,
                            &constant, &coeff)) {
        num_destroy(&coeff);
        num_destroy(&constant);
        return NULL;
    }

    u_atanh_u = expr_mul(expr->a, expr);
    u_sq = expr_pow(expr->a, &NUM_TWO);
    one_minus_u_sq = u_sq ? expr_sub_num(u_sq, &NUM_ONE) : NULL;
    log_term = one_minus_u_sq ? expr_neg(one_minus_u_sq) : NULL;
    if (log_term) {
        expr_t *tmp = expr_log(log_term);
        expr_free(log_term);
        log_term = tmp;
    }
    half_log_term = log_term ? expr_mul_num(log_term, &NUM_HALF) : NULL;
    raw = (u_atanh_u && half_log_term) ? expr_add(u_atanh_u, half_log_term) : NULL;

    expr_free(half_log_term);
    expr_free(log_term);
    expr_free(one_minus_u_sq);
    expr_free(u_sq);
    expr_free(u_atanh_u);
    num_destroy(&constant);
    return div_number_owned_consuming(raw, &coeff);
}

expr_t *integrate_asech_rule(const expr_t *expr, const expr_t *wrt)
{
    number_t constant = num_new();
    number_t coeff = num_new();
    expr_t *u_asech_u;
    expr_t *asin_u;
    expr_t *raw;

    if (!match_affine_unary(expr, wrt, EXPR_PATTERN_UNARY_ASECH,
                            &constant, &coeff)) {
        num_destroy(&coeff);
        num_destroy(&constant);
        return NULL;
    }

    u_asech_u = expr_mul(expr->a, expr);
    asin_u = expr_asin(expr->a);
    raw = (u_asech_u && asin_u) ? expr_add(u_asech_u, asin_u) : NULL;

    expr_free(asin_u);
    expr_free(u_asech_u);
    num_destroy(&constant);
    return div_number_owned_consuming(raw, &coeff);
}

expr_t *integrate_acosech_rule(const expr_t *expr, const expr_t *wrt)
{
    number_t constant = num_new();
    number_t coeff = num_new();
    expr_t *u_acosech_u;
    expr_t *asinh_u;
    expr_t *raw;

    if (!match_affine_unary(expr, wrt, EXPR_PATTERN_UNARY_ACOSECH,
                            &constant, &coeff)) {
        num_destroy(&coeff);
        num_destroy(&constant);
        return NULL;
    }

    u_acosech_u = expr_mul(expr->a, expr);
    asinh_u = expr_asinh(expr->a);
    raw = (u_acosech_u && asinh_u) ? expr_add(u_acosech_u, asinh_u) : NULL;

    expr_free(asinh_u);
    expr_free(u_acosech_u);
    num_destroy(&constant);
    return div_number_owned_consuming(raw, &coeff);
}

expr_t *integrate_acoth_rule(const expr_t *expr, const expr_t *wrt)
{
    number_t constant = num_new();
    number_t coeff = num_new();
    expr_t *u_acoth_u;
    expr_t *u_sq;
    expr_t *one_minus_u_sq;
    expr_t *log_term;
    expr_t *half_log_term;
    expr_t *raw;

    if (!match_affine_unary(expr, wrt, EXPR_PATTERN_UNARY_ACOTH,
                            &constant, &coeff)) {
        num_destroy(&coeff);
        num_destroy(&constant);
        return NULL;
    }

    u_acoth_u = expr_mul(expr->a, expr);
    u_sq = expr_pow(expr->a, &NUM_TWO);
    one_minus_u_sq = u_sq ? expr_sub_num(u_sq, &NUM_ONE) : NULL;
    log_term = one_minus_u_sq ? expr_neg(one_minus_u_sq) : NULL;
    if (log_term) {
        expr_t *tmp = expr_log(log_term);
        expr_free(log_term);
        log_term = tmp;
    }
    half_log_term = log_term ? expr_mul_num(log_term, &NUM_HALF) : NULL;
    raw = (u_acoth_u && half_log_term) ? expr_add(u_acoth_u, half_log_term) : NULL;

    expr_free(half_log_term);
    expr_free(log_term);
    expr_free(one_minus_u_sq);
    expr_free(u_sq);
    expr_free(u_acoth_u);
    num_destroy(&constant);
    return div_number_owned_consuming(raw, &coeff);
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
