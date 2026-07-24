#include "expr_integrate_internal.h"

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
    expr_t *constant = NULL;
    expr_t *coeff = NULL;
    expr_t *u_atan_u;
    expr_t *u_sq;
    expr_t *one_plus_u_sq;
    expr_t *log_term;
    expr_t *half_log_term;
    expr_t *raw;
    expr_t *out;

    if (!expr_is_op(expr, &ops_atan) || !expr->a ||
        !match_symbolic_affine_constant_and_coeff(expr->a, wrt,
                                                  &constant, &coeff) ||
        expr_const_is_zero(coeff)) {
        expr_free(coeff);
        expr_free(constant);
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
    out = raw ? expr_div(raw, coeff) : NULL;
    expr_free(raw);
    expr_free(coeff);
    expr_free(constant);
    return simplify_owned(out);
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
