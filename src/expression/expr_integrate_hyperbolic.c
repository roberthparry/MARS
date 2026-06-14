#include <stdbool.h>

#include "expr_integrate_internal.h"

static bool match_hyperbolic_proportional_wrt_coeff(const expr_t *expr,
                                                    const expr_t *wrt,
                                                    bool *is_sinh_out,
                                                    expr_t **coeff_out)
{
    expr_t *constant = NULL;
    expr_t *coeff = NULL;
    bool ok = false;

    if (!expr || !wrt || !is_sinh_out || !coeff_out || !expr->a)
        return false;

    if (expr_is_op(expr, &ops_sinh)) {
        *is_sinh_out = true;
    } else if (expr_is_op(expr, &ops_cosh)) {
        *is_sinh_out = false;
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

static bool match_exp_hyperbolic_product(const expr_t *expr,
                                         const expr_t *wrt,
                                         const expr_t **exp_out,
                                         const expr_t **hyper_out,
                                         bool *is_sinh_out,
                                         expr_t **exp_coeff_out,
                                         expr_t **hyper_coeff_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    expr_t *exp_coeff = NULL;
    expr_t *hyper_coeff = NULL;

    if (!expr || !wrt || !exp_out || !hyper_out || !is_sinh_out ||
        !exp_coeff_out || !hyper_coeff_out ||
        !expr_match_mul_expr(expr, &left, &right))
        return false;

    if (match_exp_proportional_wrt_coeff(left, wrt, &exp_coeff) &&
        match_hyperbolic_proportional_wrt_coeff(right, wrt, is_sinh_out,
                                                &hyper_coeff)) {
        *exp_out = left;
        *hyper_out = right;
        *exp_coeff_out = exp_coeff;
        *hyper_coeff_out = hyper_coeff;
        return true;
    }
    expr_free(exp_coeff);
    expr_free(hyper_coeff);
    exp_coeff = NULL;
    hyper_coeff = NULL;

    if (match_exp_proportional_wrt_coeff(right, wrt, &exp_coeff) &&
        match_hyperbolic_proportional_wrt_coeff(left, wrt, is_sinh_out,
                                                &hyper_coeff)) {
        *exp_out = right;
        *hyper_out = left;
        *exp_coeff_out = exp_coeff;
        *hyper_coeff_out = hyper_coeff;
        return true;
    }

    expr_free(exp_coeff);
    expr_free(hyper_coeff);
    return false;
}

expr_t *integrate_symbolic_exp_times_hyperbolic(const expr_t *expr,
                                                       const expr_t *wrt)
{
    const expr_t *exp_expr = NULL;
    const expr_t *hyper_expr = NULL;
    expr_t *exp_coeff = NULL;
    expr_t *hyper_coeff = NULL;
    expr_t *sinh_v = NULL;
    expr_t *cosh_v = NULL;
    expr_t *left = NULL;
    expr_t *right = NULL;
    expr_t *bracket = NULL;
    expr_t *product = NULL;
    expr_t *exp_coeff_sq = NULL;
    expr_t *hyper_coeff_sq = NULL;
    expr_t *denom = NULL;
    expr_t *quotient = NULL;
    expr_t *out = NULL;
    bool is_sinh = false;

    if (!match_exp_hyperbolic_product(expr, wrt, &exp_expr, &hyper_expr,
                                      &is_sinh, &exp_coeff, &hyper_coeff))
        goto cleanup;

    sinh_v = hyper_expr && hyper_expr->a ? expr_sinh(hyper_expr->a) : NULL;
    cosh_v = hyper_expr && hyper_expr->a ? expr_cosh(hyper_expr->a) : NULL;
    if (is_sinh) {
        left = (exp_coeff && sinh_v) ? expr_mul(exp_coeff, sinh_v) : NULL;
        right = (hyper_coeff && cosh_v) ? expr_mul(hyper_coeff, cosh_v) : NULL;
    } else {
        left = (exp_coeff && cosh_v) ? expr_mul(exp_coeff, cosh_v) : NULL;
        right = (hyper_coeff && sinh_v) ? expr_mul(hyper_coeff, sinh_v) : NULL;
    }
    bracket = (left && right) ? expr_sub(left, right) : NULL;
    product = (exp_expr && bracket) ? expr_mul(exp_expr, bracket) : NULL;
    exp_coeff_sq = exp_coeff ? expr_pow(exp_coeff, &NUM_TWO) : NULL;
    hyper_coeff_sq = hyper_coeff ? expr_pow(hyper_coeff, &NUM_TWO) : NULL;
    denom = (exp_coeff_sq && hyper_coeff_sq) ? expr_sub(exp_coeff_sq, hyper_coeff_sq) : NULL;
    denom = simplify_owned(denom);
    if (!denom || expr_is_exact_zero(denom))
        goto cleanup;
    quotient = (product && denom) ? expr_div(product, denom) : NULL;
    out = simplify_owned(quotient);
    quotient = NULL;

cleanup:
    expr_free(quotient);
    expr_free(denom);
    expr_free(hyper_coeff_sq);
    expr_free(exp_coeff_sq);
    expr_free(product);
    expr_free(bracket);
    expr_free(right);
    expr_free(left);
    expr_free(cosh_v);
    expr_free(sinh_v);
    expr_free(hyper_coeff);
    expr_free(exp_coeff);
    return out;
}

static bool match_hyperbolic_product(const expr_t *expr,
                                     const expr_t *wrt,
                                     const expr_t **first_out,
                                     const expr_t **second_out,
                                     bool *first_is_sinh_out,
                                     bool *second_is_sinh_out,
                                     expr_t **first_coeff_out,
                                     expr_t **second_coeff_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    expr_t *first_coeff = NULL;
    expr_t *second_coeff = NULL;

    if (!expr || !wrt || !first_out || !second_out ||
        !first_is_sinh_out || !second_is_sinh_out ||
        !first_coeff_out || !second_coeff_out ||
        !expr_match_mul_expr(expr, &left, &right))
        return false;

    if (match_hyperbolic_proportional_wrt_coeff(left, wrt,
                                                first_is_sinh_out,
                                                &first_coeff) &&
        match_hyperbolic_proportional_wrt_coeff(right, wrt,
                                                second_is_sinh_out,
                                                &second_coeff)) {
        *first_out = left;
        *second_out = right;
        *first_coeff_out = first_coeff;
        *second_coeff_out = second_coeff;
        return true;
    }

    expr_free(first_coeff);
    expr_free(second_coeff);
    return false;
}

static expr_t *build_symbolic_same_hyperbolic_product_integral(const expr_t *arg,
                                                               const expr_t *coeff,
                                                               bool is_sinh_square,
                                                               bool is_cosh_square,
                                                               bool is_sinh_cosh)
{
    expr_t *two_arg = NULL;
    expr_t *sinh_two_arg = NULL;
    expr_t *denom = NULL;
    expr_t *term1 = NULL;
    expr_t *term2 = NULL;
    expr_t *sinh_arg = NULL;
    expr_t *sinh_sq = NULL;
    expr_t *quotient = NULL;
    expr_t *out = NULL;
    number_t four = num_create_from_long(4);

    if (!arg || !coeff)
        goto cleanup;

    if (is_sinh_cosh) {
        sinh_arg = expr_sinh(arg);
        sinh_sq = sinh_arg ? expr_pow(sinh_arg, &NUM_TWO) : NULL;
        denom = expr_mul_num(coeff, &NUM_TWO);
        quotient = (sinh_sq && denom) ? expr_div(sinh_sq, denom) : NULL;
        out = simplify_owned(quotient);
        quotient = NULL;
        goto cleanup;
    }

    two_arg = expr_mul_num(arg, &NUM_TWO);
    sinh_two_arg = two_arg ? expr_sinh(two_arg) : NULL;
    denom = expr_mul_num(coeff, &four);
    term1 = (sinh_two_arg && denom) ? expr_div(sinh_two_arg, denom) : NULL;
    term2 = expr_mul_num(arg, &NUM_HALF);
    if (term2 && coeff) {
        expr_t *scaled = expr_div(term2, coeff);

        expr_free(term2);
        term2 = scaled;
    }

    if (is_sinh_square)
        out = (term1 && term2) ? expr_sub(term1, term2) : NULL;
    else if (is_cosh_square)
        out = (term1 && term2) ? expr_add(term1, term2) : NULL;
    out = simplify_owned(out);

cleanup:
    num_destroy(&four);
    expr_free(quotient);
    expr_free(sinh_sq);
    expr_free(sinh_arg);
    expr_free(term2);
    expr_free(term1);
    expr_free(denom);
    expr_free(sinh_two_arg);
    expr_free(two_arg);
    return out;
}

expr_t *integrate_symbolic_hyperbolic_product(const expr_t *expr,
                                                     const expr_t *wrt)
{
    const expr_t *first_expr = NULL;
    const expr_t *second_expr = NULL;
    const expr_t *sinh_expr = NULL;
    const expr_t *cosh_expr = NULL;
    expr_t *first_coeff = NULL;
    expr_t *second_coeff = NULL;
    expr_t *sinh_coeff = NULL;
    expr_t *cosh_coeff = NULL;
    expr_t *sinh_first = NULL;
    expr_t *cosh_first = NULL;
    expr_t *sinh_second = NULL;
    expr_t *cosh_second = NULL;
    expr_t *first_factor = NULL;
    expr_t *second_factor = NULL;
    expr_t *left = NULL;
    expr_t *right = NULL;
    expr_t *bracket = NULL;
    expr_t *first_coeff_sq = NULL;
    expr_t *second_coeff_sq = NULL;
    expr_t *denom = NULL;
    expr_t *quotient = NULL;
    expr_t *out = NULL;
    bool first_is_sinh = false;
    bool second_is_sinh = false;

    if (!match_hyperbolic_product(expr, wrt, &first_expr, &second_expr,
                                  &first_is_sinh, &second_is_sinh,
                                  &first_coeff, &second_coeff))
        goto cleanup;

    if (expr_equal_exact_local(first_expr->a, second_expr->a)) {
        out = build_symbolic_same_hyperbolic_product_integral(
            first_expr->a,
            first_coeff,
            first_is_sinh && second_is_sinh,
            !first_is_sinh && !second_is_sinh,
            first_is_sinh != second_is_sinh);
        goto cleanup;
    }

    sinh_first = first_expr && first_expr->a ? expr_sinh(first_expr->a) : NULL;
    cosh_first = first_expr && first_expr->a ? expr_cosh(first_expr->a) : NULL;
    sinh_second = second_expr && second_expr->a ? expr_sinh(second_expr->a) : NULL;
    cosh_second = second_expr && second_expr->a ? expr_cosh(second_expr->a) : NULL;

    if (first_is_sinh && second_is_sinh) {
        first_factor = (cosh_second && sinh_first) ? expr_mul(cosh_second, sinh_first) : NULL;
        second_factor = (cosh_first && sinh_second) ? expr_mul(cosh_first, sinh_second) : NULL;
        left = (second_coeff && first_factor) ? expr_mul(second_coeff, first_factor) : NULL;
        right = (first_coeff && second_factor) ? expr_mul(first_coeff, second_factor) : NULL;
        first_coeff_sq = first_coeff ? expr_pow(first_coeff, &NUM_TWO) : NULL;
        second_coeff_sq = second_coeff ? expr_pow(second_coeff, &NUM_TWO) : NULL;
        denom = (second_coeff_sq && first_coeff_sq) ? expr_sub(second_coeff_sq, first_coeff_sq) : NULL;
    } else if (!first_is_sinh && !second_is_sinh) {
        first_factor = (sinh_first && cosh_second) ? expr_mul(sinh_first, cosh_second) : NULL;
        second_factor = (cosh_first && sinh_second) ? expr_mul(cosh_first, sinh_second) : NULL;
        left = (first_coeff && first_factor) ? expr_mul(first_coeff, first_factor) : NULL;
        right = (second_coeff && second_factor) ? expr_mul(second_coeff, second_factor) : NULL;
        first_coeff_sq = first_coeff ? expr_pow(first_coeff, &NUM_TWO) : NULL;
        second_coeff_sq = second_coeff ? expr_pow(second_coeff, &NUM_TWO) : NULL;
        denom = (first_coeff_sq && second_coeff_sq) ? expr_sub(first_coeff_sq, second_coeff_sq) : NULL;
    } else {
        sinh_expr = first_is_sinh ? first_expr : second_expr;
        cosh_expr = first_is_sinh ? second_expr : first_expr;
        sinh_coeff = expr_clone(first_is_sinh ? first_coeff : second_coeff);
        cosh_coeff = expr_clone(first_is_sinh ? second_coeff : first_coeff);
        expr_free(sinh_first);
        expr_free(cosh_first);
        expr_free(sinh_second);
        expr_free(cosh_second);
        sinh_first = sinh_expr && sinh_expr->a ? expr_sinh(sinh_expr->a) : NULL;
        cosh_first = sinh_expr && sinh_expr->a ? expr_cosh(sinh_expr->a) : NULL;
        sinh_second = cosh_expr && cosh_expr->a ? expr_sinh(cosh_expr->a) : NULL;
        cosh_second = cosh_expr && cosh_expr->a ? expr_cosh(cosh_expr->a) : NULL;
        first_factor = (cosh_first && cosh_second) ? expr_mul(cosh_first, cosh_second) : NULL;
        second_factor = (sinh_first && sinh_second) ? expr_mul(sinh_first, sinh_second) : NULL;
        left = (sinh_coeff && first_factor) ? expr_mul(sinh_coeff, first_factor) : NULL;
        right = (cosh_coeff && second_factor) ? expr_mul(cosh_coeff, second_factor) : NULL;
        first_coeff_sq = sinh_coeff ? expr_pow(sinh_coeff, &NUM_TWO) : NULL;
        second_coeff_sq = cosh_coeff ? expr_pow(cosh_coeff, &NUM_TWO) : NULL;
        denom = (first_coeff_sq && second_coeff_sq) ? expr_sub(first_coeff_sq, second_coeff_sq) : NULL;
    }

    bracket = (left && right) ? expr_sub(left, right) : NULL;
    quotient = (bracket && denom) ? expr_div(bracket, denom) : NULL;
    out = simplify_owned(quotient);
    quotient = NULL;

cleanup:
    expr_free(quotient);
    expr_free(denom);
    expr_free(second_coeff_sq);
    expr_free(first_coeff_sq);
    expr_free(bracket);
    expr_free(right);
    expr_free(left);
    expr_free(second_factor);
    expr_free(first_factor);
    expr_free(cosh_second);
    expr_free(sinh_second);
    expr_free(cosh_first);
    expr_free(sinh_first);
    expr_free(cosh_coeff);
    expr_free(sinh_coeff);
    expr_free(second_coeff);
    expr_free(first_coeff);
    return out;
}

static bool match_trig_hyperbolic_product(const expr_t *expr,
                                          const expr_t *wrt,
                                          const expr_t **trig_out,
                                          const expr_t **hyper_out,
                                          bool *trig_is_sin_out,
                                          bool *hyper_is_sinh_out,
                                          expr_t **trig_coeff_out,
                                          expr_t **hyper_coeff_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    expr_t *trig_coeff = NULL;
    expr_t *hyper_coeff = NULL;

    if (!expr || !wrt || !trig_out || !hyper_out ||
        !trig_is_sin_out || !hyper_is_sinh_out ||
        !trig_coeff_out || !hyper_coeff_out ||
        !expr_match_mul_expr(expr, &left, &right))
        return false;

    if (match_trig_proportional_wrt_coeff(left, wrt, trig_is_sin_out,
                                          &trig_coeff) &&
        match_hyperbolic_proportional_wrt_coeff(right, wrt,
                                                hyper_is_sinh_out,
                                                &hyper_coeff)) {
        *trig_out = left;
        *hyper_out = right;
        *trig_coeff_out = trig_coeff;
        *hyper_coeff_out = hyper_coeff;
        return true;
    }
    expr_free(trig_coeff);
    expr_free(hyper_coeff);
    trig_coeff = NULL;
    hyper_coeff = NULL;

    if (match_trig_proportional_wrt_coeff(right, wrt, trig_is_sin_out,
                                          &trig_coeff) &&
        match_hyperbolic_proportional_wrt_coeff(left, wrt,
                                                hyper_is_sinh_out,
                                                &hyper_coeff)) {
        *trig_out = right;
        *hyper_out = left;
        *trig_coeff_out = trig_coeff;
        *hyper_coeff_out = hyper_coeff;
        return true;
    }

    expr_free(trig_coeff);
    expr_free(hyper_coeff);
    return false;
}

expr_t *integrate_symbolic_trig_times_hyperbolic(const expr_t *expr,
                                                        const expr_t *wrt)
{
    const expr_t *trig_expr = NULL;
    const expr_t *hyper_expr = NULL;
    expr_t *trig_coeff = NULL;
    expr_t *hyper_coeff = NULL;
    expr_t *sin_u = NULL;
    expr_t *cos_u = NULL;
    expr_t *sinh_v = NULL;
    expr_t *cosh_v = NULL;
    expr_t *first_factor = NULL;
    expr_t *second_factor = NULL;
    expr_t *left = NULL;
    expr_t *right = NULL;
    expr_t *bracket = NULL;
    expr_t *trig_coeff_sq = NULL;
    expr_t *hyper_coeff_sq = NULL;
    expr_t *denom = NULL;
    expr_t *quotient = NULL;
    expr_t *out = NULL;
    bool trig_is_sin = false;
    bool hyper_is_sinh = false;
    bool subtract = false;

    if (!match_trig_hyperbolic_product(expr, wrt, &trig_expr, &hyper_expr,
                                       &trig_is_sin, &hyper_is_sinh,
                                       &trig_coeff, &hyper_coeff))
        goto cleanup;

    sin_u = trig_expr && trig_expr->a ? expr_sin(trig_expr->a) : NULL;
    cos_u = trig_expr && trig_expr->a ? expr_cos(trig_expr->a) : NULL;
    sinh_v = hyper_expr && hyper_expr->a ? expr_sinh(hyper_expr->a) : NULL;
    cosh_v = hyper_expr && hyper_expr->a ? expr_cosh(hyper_expr->a) : NULL;

    if (!trig_is_sin && !hyper_is_sinh) {
        first_factor = (sin_u && cosh_v) ? expr_mul(sin_u, cosh_v) : NULL;
        second_factor = (cos_u && sinh_v) ? expr_mul(cos_u, sinh_v) : NULL;
        left = (trig_coeff && first_factor) ? expr_mul(trig_coeff, first_factor) : NULL;
        right = (hyper_coeff && second_factor) ? expr_mul(hyper_coeff, second_factor) : NULL;
    } else if (!trig_is_sin && hyper_is_sinh) {
        first_factor = (cos_u && cosh_v) ? expr_mul(cos_u, cosh_v) : NULL;
        second_factor = (sin_u && sinh_v) ? expr_mul(sin_u, sinh_v) : NULL;
        left = (hyper_coeff && first_factor) ? expr_mul(hyper_coeff, first_factor) : NULL;
        right = (trig_coeff && second_factor) ? expr_mul(trig_coeff, second_factor) : NULL;
    } else if (trig_is_sin && !hyper_is_sinh) {
        first_factor = (sin_u && sinh_v) ? expr_mul(sin_u, sinh_v) : NULL;
        second_factor = (cos_u && cosh_v) ? expr_mul(cos_u, cosh_v) : NULL;
        left = (hyper_coeff && first_factor) ? expr_mul(hyper_coeff, first_factor) : NULL;
        right = (trig_coeff && second_factor) ? expr_mul(trig_coeff, second_factor) : NULL;
        subtract = true;
    } else {
        first_factor = (sin_u && cosh_v) ? expr_mul(sin_u, cosh_v) : NULL;
        second_factor = (cos_u && sinh_v) ? expr_mul(cos_u, sinh_v) : NULL;
        left = (hyper_coeff && first_factor) ? expr_mul(hyper_coeff, first_factor) : NULL;
        right = (trig_coeff && second_factor) ? expr_mul(trig_coeff, second_factor) : NULL;
        subtract = true;
    }

    bracket = (left && right) ? (subtract ? expr_sub(left, right) : expr_add(left, right))
                              : NULL;
    trig_coeff_sq = trig_coeff ? expr_pow(trig_coeff, &NUM_TWO) : NULL;
    hyper_coeff_sq = hyper_coeff ? expr_pow(hyper_coeff, &NUM_TWO) : NULL;
    denom = (trig_coeff_sq && hyper_coeff_sq) ? expr_add(trig_coeff_sq, hyper_coeff_sq) : NULL;
    quotient = (bracket && denom) ? expr_div(bracket, denom) : NULL;
    out = simplify_owned(quotient);
    quotient = NULL;

cleanup:
    expr_free(quotient);
    expr_free(denom);
    expr_free(hyper_coeff_sq);
    expr_free(trig_coeff_sq);
    expr_free(bracket);
    expr_free(right);
    expr_free(left);
    expr_free(second_factor);
    expr_free(first_factor);
    expr_free(cosh_v);
    expr_free(sinh_v);
    expr_free(cos_u);
    expr_free(sin_u);
    expr_free(hyper_coeff);
    expr_free(trig_coeff);
    return out;
}

expr_t *integrate_symbolic_squared_hyperbolic(const expr_t *expr,
                                                     const expr_t *wrt)
{
    expr_t *coeff = NULL;
    expr_t *two_arg = NULL;
    expr_t *sinh_two_arg = NULL;
    expr_t *denom = NULL;
    expr_t *term1 = NULL;
    expr_t *term2 = NULL;
    expr_t *out = NULL;
    const expr_t *base = NULL;
    number_t exponent = num_new();
    number_t four = num_create_from_long(4);
    bool is_sinh = false;

    if (!expr || !wrt || !expr->a)
        goto cleanup;

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW_D) {
        base = expr->a;
        num_destroy(&exponent);
        exponent = num_clone(expr->c);
    } else if (expr->ops && expr->ops->kind == EXPR_KIND_POW &&
               expr->a && expr->b) {
        base = expr->a;
        if (!expr_match_const_value(expr->b, &exponent))
            goto cleanup;
    } else {
        goto cleanup;
    }

    if (!base ||
        !num_eq(exponent, NUM_TWO) ||
        !match_hyperbolic_proportional_wrt_coeff(base, wrt, &is_sinh, &coeff))
        goto cleanup;

    two_arg = expr_mul_num(base->a, &NUM_TWO);
    sinh_two_arg = two_arg ? expr_sinh(two_arg) : NULL;
    denom = coeff ? expr_mul_num(coeff, &four) : NULL;
    term1 = (sinh_two_arg && denom) ? expr_div(sinh_two_arg, denom) : NULL;
    term2 = wrt ? expr_mul_num(wrt, &NUM_HALF) : NULL;
    out = is_sinh ? expr_sub(term1, term2) : expr_add(term1, term2);
    out = simplify_owned(out);

cleanup:
    num_destroy(&four);
    num_destroy(&exponent);
    expr_free(term2);
    expr_free(term1);
    expr_free(denom);
    expr_free(sinh_two_arg);
    expr_free(two_arg);
    expr_free(coeff);
    return out;
}

expr_t *integrate_exp_tanh_exact(const expr_t *expr, const expr_t *wrt)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    const expr_t *exp_expr = NULL;
    const expr_t *tanh_expr = NULL;
    expr_t *atan_exp = NULL;
    expr_t *twice_atan = NULL;
    expr_t *out = NULL;

    if (!expr || !wrt || !expr_match_mul_expr(expr, &left, &right))
        return NULL;

    if (expr_is_op(left, &ops_exp) && left->a && is_wrt(left->a, wrt) &&
        expr_is_op(right, &ops_tanh) && right->a && is_wrt(right->a, wrt)) {
        exp_expr = left;
        tanh_expr = right;
    } else if (expr_is_op(right, &ops_exp) && right->a && is_wrt(right->a, wrt) &&
               expr_is_op(left, &ops_tanh) && left->a && is_wrt(left->a, wrt)) {
        exp_expr = right;
        tanh_expr = left;
    }
    if (!exp_expr || !tanh_expr)
        return NULL;

    atan_exp = expr_atan(exp_expr);
    twice_atan = atan_exp ? expr_mul_num(atan_exp, &NUM_TWO) : NULL;
    out = (exp_expr && twice_atan) ? expr_sub(exp_expr, twice_atan) : NULL;
    out = simplify_owned(out);

    expr_free(twice_atan);
    expr_free(atan_exp);
    return out;
}
