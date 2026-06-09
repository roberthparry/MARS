#include <limits.h>

#include "expr_maths.h"

static inline void expr_reverse_unary(number_t value, number_t *a_bar, number_t *b_bar)
{
    *a_bar = value;
    *b_bar = NUM_ZERO;
}

static inline void expr_reverse_binary(number_t a_value, number_t b_value,
                                     number_t *a_bar, number_t *b_bar)
{
    *a_bar = a_value;
    *b_bar = b_value;
}

static inline number_t num_sq_local(const number_t value)
{
    return num_mul(value, value);
}

static inline number_t num_inverse_local(const number_t value)
{
    return num_div(NUM_ONE, value);
}

static inline number_t num_owned_clone_local(const number_t value)
{
    return num_clone(value);
}

static inline number_t num_owned_neg_local(const number_t value)
{
    return num_neg(value);
}

static inline number_t num_owned_mul_local(const number_t a, const number_t b)
{
    return num_mul(a, b);
}

static inline number_t num_owned_div_local(const number_t a, const number_t b)
{
    return num_div(a, b);
}

void expr_reverse_atan2(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t ax2 = num_sq_local(expr_eval_num_internal(dv->a));
    number_t bx2 = num_sq_local(expr_eval_num_internal(dv->b));
    number_t denom = num_add(ax2, bx2);
    number_t y_over_denom = num_div(expr_eval_num_internal(dv->b), denom);
    number_t x_over_denom = num_div(expr_eval_num_internal(dv->a), denom);
    number_t scaled_x = num_mul(*out_bar, x_over_denom);

    *a_bar = num_owned_mul_local(*out_bar, y_over_denom);
    *b_bar = num_owned_neg_local(scaled_x);
}

void expr_reverse_sin(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t cos_x = num_cos(expr_eval_num_internal(dv->a));
    number_t factor = num_mul(*out_bar, cos_x);

    expr_reverse_unary(num_owned_clone_local(factor), a_bar, b_bar);
}

void expr_reverse_cos(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t sin_x = num_sin(expr_eval_num_internal(dv->a));
    number_t product = num_mul(*out_bar, sin_x);

    expr_reverse_unary(num_owned_neg_local(product), a_bar, b_bar);
}

void expr_reverse_tan(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t cos_x = num_cos(expr_eval_num_internal(dv->a));
    number_t cos_sq = num_sq_local(cos_x);
    number_t inv = num_inverse_local(cos_sq);

    expr_reverse_unary(num_owned_mul_local(*out_bar, inv), a_bar, b_bar);
}

void expr_reverse_sec(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t x = expr_eval_num_internal(dv->a);
    number_t factor = num_mul(num_sec(x), num_tan(x));

    expr_reverse_unary(num_owned_mul_local(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_cosec(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t x = expr_eval_num_internal(dv->a);
    number_t factor = num_mul(num_cosec(x), num_cot(x));
    number_t product = num_mul(*out_bar, factor);

    expr_reverse_unary(num_owned_neg_local(product), a_bar, b_bar);
}

void expr_reverse_cot(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t cosec_x = num_cosec(expr_eval_num_internal(dv->a));
    number_t factor = num_sq_local(cosec_x);
    number_t product = num_mul(*out_bar, factor);

    expr_reverse_unary(num_owned_neg_local(product), a_bar, b_bar);
}

void expr_reverse_sinh(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t cosh_x = num_cosh(expr_eval_num_internal(dv->a));
    number_t factor = num_mul(*out_bar, cosh_x);

    expr_reverse_unary(num_owned_clone_local(factor), a_bar, b_bar);
}

void expr_reverse_cosh(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t sinh_x = num_sinh(expr_eval_num_internal(dv->a));
    number_t factor = num_mul(*out_bar, sinh_x);

    expr_reverse_unary(num_owned_clone_local(factor), a_bar, b_bar);
}

void expr_reverse_tanh(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t expr_sq = num_sq_local(expr_eval_num_internal(dv));
    number_t factor = num_sub(NUM_ONE, expr_sq);

    expr_reverse_unary(num_owned_mul_local(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_sech(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t x = expr_eval_num_internal(dv->a);
    number_t factor = num_mul(num_sech(x), num_tanh(x));
    number_t product = num_mul(*out_bar, factor);

    expr_reverse_unary(num_owned_neg_local(product), a_bar, b_bar);
}

void expr_reverse_cosech(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t x = expr_eval_num_internal(dv->a);
    number_t factor = num_mul(num_cosech(x), num_coth(x));
    number_t product = num_mul(*out_bar, factor);

    expr_reverse_unary(num_owned_neg_local(product), a_bar, b_bar);
}

void expr_reverse_coth(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t cosech_x = num_cosech(expr_eval_num_internal(dv->a));
    number_t factor = num_sq_local(cosech_x);
    number_t product = num_mul(*out_bar, factor);

    expr_reverse_unary(num_owned_neg_local(product), a_bar, b_bar);
}

void expr_reverse_asin(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t x_sq = num_sq_local(expr_eval_num_internal(dv->a));
    number_t inner = num_sub(NUM_ONE, x_sq);
    number_t denom = num_sqrt(inner);

    expr_reverse_unary(num_owned_div_local(*out_bar, denom), a_bar, b_bar);
}

void expr_reverse_acos(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t x_sq = num_sq_local(expr_eval_num_internal(dv->a));
    number_t inner = num_sub(NUM_ONE, x_sq);
    number_t denom = num_sqrt(inner);
    number_t frac = num_div(*out_bar, denom);

    expr_reverse_unary(num_owned_neg_local(frac), a_bar, b_bar);
}

void expr_reverse_atan(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t x_sq = num_sq_local(expr_eval_num_internal(dv->a));
    number_t denom = num_add(NUM_ONE, x_sq);

    expr_reverse_unary(num_owned_div_local(*out_bar, denom), a_bar, b_bar);
}

static number_t expr_reverse_inverse_reciprocal_factor(const expr_t *dv,
                                                     number_t (*inner_factor)(const expr_t *))
{
    number_t x = expr_eval_num_internal(dv->a);
    number_t x_sq = num_sq_local(x);
    number_t factor = inner_factor(dv);

    return num_div(num_neg(factor), x_sq);
}

static number_t expr_reverse_asec_inner(const expr_t *dv)
{
    number_t x = expr_eval_num_internal(dv->a);
    number_t x_sq = num_sq_local(x);
    number_t inv_x_sq = num_inverse_local(x_sq);
    number_t inner = num_sub(NUM_ONE, inv_x_sq);

    return num_neg(num_inverse_local(num_sqrt(inner)));
}

static number_t expr_reverse_acosec_inner(const expr_t *dv)
{
    number_t x = expr_eval_num_internal(dv->a);
    number_t x_sq = num_sq_local(x);
    number_t inv_x_sq = num_inverse_local(x_sq);
    number_t inner = num_sub(NUM_ONE, inv_x_sq);

    return num_inverse_local(num_sqrt(inner));
}

static number_t expr_reverse_acot_inner(const expr_t *dv)
{
    number_t x = expr_eval_num_internal(dv->a);
    number_t x_sq = num_sq_local(x);
    number_t inv_x_sq = num_inverse_local(x_sq);
    number_t denom = num_add(NUM_ONE, inv_x_sq);

    return num_inverse_local(denom);
}

void expr_reverse_asec(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t factor = expr_reverse_inverse_reciprocal_factor(dv, expr_reverse_asec_inner);

    expr_reverse_unary(num_owned_mul_local(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_acosec(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t factor = expr_reverse_inverse_reciprocal_factor(dv, expr_reverse_acosec_inner);

    expr_reverse_unary(num_owned_mul_local(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_acot(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t factor = expr_reverse_inverse_reciprocal_factor(dv, expr_reverse_acot_inner);

    expr_reverse_unary(num_owned_mul_local(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_asinh(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t x_sq = num_sq_local(expr_eval_num_internal(dv->a));
    number_t inner = num_add(x_sq, NUM_ONE);
    number_t denom = num_sqrt(inner);

    expr_reverse_unary(num_owned_div_local(*out_bar, denom), a_bar, b_bar);
}

void expr_reverse_acosh(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t xm1 = num_sub(expr_eval_num_internal(dv->a), NUM_ONE);
    number_t xp1 = num_add(expr_eval_num_internal(dv->a), NUM_ONE);
    number_t sqrt_xm1 = num_sqrt(xm1);
    number_t sqrt_xp1 = num_sqrt(xp1);
    number_t denom = num_mul(sqrt_xm1, sqrt_xp1);

    expr_reverse_unary(num_owned_div_local(*out_bar, denom), a_bar, b_bar);
}

void expr_reverse_atanh(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t x_sq = num_sq_local(expr_eval_num_internal(dv->a));
    number_t denom = num_sub(NUM_ONE, x_sq);

    expr_reverse_unary(num_owned_div_local(*out_bar, denom), a_bar, b_bar);
}

static number_t expr_reverse_asech_inner(const expr_t *dv)
{
    number_t x = expr_eval_num_internal(dv->a);
    number_t inv_x = num_inverse_local(x);
    number_t im1 = num_sub(inv_x, NUM_ONE);
    number_t ip1 = num_add(inv_x, NUM_ONE);
    number_t denom = num_mul(num_sqrt(im1), num_sqrt(ip1));

    return num_inverse_local(denom);
}

static number_t expr_reverse_acosech_inner(const expr_t *dv)
{
    number_t x = expr_eval_num_internal(dv->a);
    number_t x_sq = num_sq_local(x);
    number_t inv_x_sq = num_inverse_local(x_sq);
    number_t inner = num_add(NUM_ONE, inv_x_sq);

    return num_inverse_local(num_sqrt(inner));
}

static number_t expr_reverse_acoth_inner(const expr_t *dv)
{
    number_t x = expr_eval_num_internal(dv->a);
    number_t x_sq = num_sq_local(x);
    number_t inv_x_sq = num_inverse_local(x_sq);
    number_t denom = num_sub(NUM_ONE, inv_x_sq);

    return num_inverse_local(denom);
}

void expr_reverse_asech(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t factor = expr_reverse_inverse_reciprocal_factor(dv, expr_reverse_asech_inner);

    expr_reverse_unary(num_owned_mul_local(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_acosech(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t factor = expr_reverse_inverse_reciprocal_factor(dv, expr_reverse_acosech_inner);

    expr_reverse_unary(num_owned_mul_local(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_acoth(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t factor = expr_reverse_inverse_reciprocal_factor(dv, expr_reverse_acoth_inner);

    expr_reverse_unary(num_owned_mul_local(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_exp(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t factor = num_mul(*out_bar, expr_eval_num_internal(dv));

    expr_reverse_unary(num_owned_clone_local(factor), a_bar, b_bar);
}

void expr_reverse_log(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t factor = num_div(*out_bar, expr_eval_num_internal(dv->a));

    expr_reverse_unary(num_owned_clone_local(factor), a_bar, b_bar);
}

void expr_reverse_log10(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t denom = num_mul(expr_eval_num_internal(dv->a), NUM_LN10);

    expr_reverse_unary(num_owned_div_local(*out_bar, denom), a_bar, b_bar);
}

void expr_reverse_sqrt(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t denom = num_mul(NUM_TWO, expr_eval_num_internal(dv));

    expr_reverse_unary(num_owned_div_local(*out_bar, denom), a_bar, b_bar);
}

void expr_reverse_floor(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    (void)dv;
    (void)out_bar;
    expr_reverse_unary(NUM_ZERO, a_bar, b_bar);
}

void expr_reverse_ceil(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    (void)dv;
    (void)out_bar;
    expr_reverse_unary(NUM_ZERO, a_bar, b_bar);
}

void expr_reverse_abs(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    if (!num_is_real(expr_eval_num_internal(dv->a))) {
        *a_bar = NUM_NAN;
        *b_bar = NUM_ZERO;
        return;
    }
    switch (num_cmp(expr_eval_num_internal(dv->a), NUM_ZERO)) {
        case  1: *a_bar = num_owned_clone_local(*out_bar); break;
        case -1: *a_bar = num_owned_neg_local  (*out_bar); break;
        default: *a_bar = NUM_ZERO;                        break;
    }
    *b_bar = NUM_ZERO;
}

void expr_reverse_hypot(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t ax = num_div(expr_eval_num_internal(dv->a), expr_eval_num_internal(dv));
    number_t bx = num_div(expr_eval_num_internal(dv->b), expr_eval_num_internal(dv));

    *a_bar = num_owned_mul_local(*out_bar, ax);
    *b_bar = num_owned_mul_local(*out_bar, bx);
}

void expr_reverse_erf(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t x_sq = num_sq_local(expr_eval_num_internal(dv->a));
    number_t neg_x_sq = num_neg(x_sq);
    number_t exp_term = num_exp(neg_x_sq);
    number_t factor = num_mul(NUM_2_SQRTPI, exp_term);

    expr_reverse_unary(num_owned_mul_local(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_erfc(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t x_sq = num_sq_local(expr_eval_num_internal(dv->a));
    number_t neg_x_sq = num_neg(x_sq);
    number_t exp_term = num_exp(neg_x_sq);
    number_t factor = num_mul(NUM_NEG_TWO_OVER_SQRT_PI, exp_term);

    expr_reverse_unary(num_owned_mul_local(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_erfinv(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t y_sq = num_sq_local(expr_eval_num_internal(dv));
    number_t exp_term = num_exp(y_sq);
    number_t scale = num_div(NUM_SQRT_PI, NUM_TWO);
    number_t factor = num_mul(scale, exp_term);

    expr_reverse_unary(num_owned_mul_local(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_erfcinv(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t y_sq = num_sq_local(expr_eval_num_internal(dv));
    number_t scale = num_div(NUM_SQRT_PI, NUM_TWO);
    number_t neg_scale = num_neg(scale);
    number_t exp_term = num_exp(y_sq);
    number_t factor = num_mul(neg_scale, exp_term);

    expr_reverse_unary(num_owned_mul_local(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_gamma(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t digamma_x = num_digamma(expr_eval_num_internal(dv->a));
    number_t factor = num_mul(expr_eval_num_internal(dv), digamma_x);

    expr_reverse_unary(num_owned_mul_local(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_lgamma(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    (void)dv;
    number_t digamma_x = num_digamma(expr_eval_num_internal(dv->a));
    number_t factor = num_mul(*out_bar, digamma_x);

    expr_reverse_unary(num_owned_clone_local(factor), a_bar, b_bar);
}

void expr_reverse_digamma(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    (void)dv;
    number_t trigamma_x = num_trigamma(expr_eval_num_internal(dv->a));
    number_t factor = num_mul(*out_bar, trigamma_x);

    expr_reverse_unary(num_owned_clone_local(factor), a_bar, b_bar);
}

void expr_reverse_trigamma(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    (void)dv;
    number_t tetragamma_x = num_tetragamma(expr_eval_num_internal(dv->a));
    number_t factor = num_mul(*out_bar, tetragamma_x);

    expr_reverse_unary(num_owned_clone_local(factor), a_bar, b_bar);
}

void expr_reverse_polygamma(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t order_value = expr_eval_num_internal(dv->a);
    number_t next;
    number_t factor;
    unsigned int order;

    if (!expr_number_to_polygamma_order(order_value, &order) ||
        order == UINT_MAX) {
        expr_reverse_binary(NUM_ZERO, NUM_ZERO, a_bar, b_bar);
        return;
    }

    next = num_polygamma(order + 1u, expr_eval_num_internal(dv->b));
    factor = num_mul(*out_bar, next);
    expr_reverse_binary(NUM_ZERO, num_owned_clone_local(factor), a_bar, b_bar);
}

void expr_reverse_gammainv(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t y = expr_eval_num_internal(dv);
    number_t psi_y = num_digamma(y);
    number_t x_psi = num_mul(expr_eval_num_internal(dv->a), psi_y);
    number_t factor = num_inv(x_psi);

    expr_reverse_unary(num_owned_mul_local(*out_bar, factor), a_bar, b_bar);
}

static number_t num_lambert_reverse_factor(const number_t z, const number_t w)
{
    if (num_eq(z, NUM_ZERO))
        return NUM_ONE;
    number_t one_plus_w = num_add(NUM_ONE, w);
    {
        number_t denom = num_mul(z, one_plus_w);
        return num_div(w, denom);
    }
}

void expr_reverse_lambert_w0(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t factor = num_lambert_reverse_factor(expr_eval_num_internal(dv->a), expr_eval_num_internal(dv));

    expr_reverse_unary(num_owned_mul_local(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_lambert_w(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t factor = num_lambert_reverse_factor(expr_eval_num_internal(dv->a), expr_eval_num_internal(dv));

    expr_reverse_unary(num_owned_mul_local(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_lambert_wm1(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t factor = num_lambert_reverse_factor(expr_eval_num_internal(dv->a), expr_eval_num_internal(dv));

    expr_reverse_unary(num_owned_mul_local(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_normal_pdf(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t x_pdf = num_mul(expr_eval_num_internal(dv->a), expr_eval_num_internal(dv));
    number_t factor = num_neg(x_pdf);

    expr_reverse_unary(num_owned_mul_local(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_normal_cdf(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t pdf_x = num_normal_pdf(expr_eval_num_internal(dv->a));
    number_t factor = num_mul(*out_bar, pdf_x);

    expr_reverse_unary(num_owned_clone_local(factor), a_bar, b_bar);
}

void expr_reverse_normal_logpdf(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t neg_x = num_neg(expr_eval_num_internal(dv->a));
    number_t factor = num_mul(*out_bar, neg_x);

    expr_reverse_unary(num_owned_clone_local(factor), a_bar, b_bar);
}

void expr_reverse_ei(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t exp_x = num_exp(expr_eval_num_internal(dv->a));
    number_t factor = num_div(exp_x, expr_eval_num_internal(dv->a));

    expr_reverse_unary(num_owned_mul_local(*out_bar, factor), a_bar, b_bar);
}

void expr_reverse_e1(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t neg_x = num_neg(expr_eval_num_internal(dv->a));
    number_t exp_neg_x = num_exp(neg_x);
    number_t factor = num_div(exp_neg_x, expr_eval_num_internal(dv->a));
    number_t neg_factor = num_neg(factor);

    expr_reverse_unary(num_owned_mul_local(*out_bar, neg_factor), a_bar, b_bar);
}

void expr_reverse_beta(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t a_plus_b = num_add(expr_eval_num_internal(dv->a), expr_eval_num_internal(dv->b));
    number_t digamma_a = num_digamma(expr_eval_num_internal(dv->a));
    number_t digamma_b = num_digamma(expr_eval_num_internal(dv->b));
    number_t psi_ab = num_digamma(a_plus_b);
    number_t psi_a_minus = num_sub(digamma_a, psi_ab);
    number_t psi_b_minus = num_sub(digamma_b, psi_ab);
    number_t scale_a = num_mul(expr_eval_num_internal(dv), psi_a_minus);
    number_t scale_b = num_mul(expr_eval_num_internal(dv), psi_b_minus);

    *a_bar = num_owned_mul_local(*out_bar, scale_a);
    *b_bar = num_owned_mul_local(*out_bar, scale_b);
}

void expr_reverse_logbeta(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t a_plus_b = num_add(expr_eval_num_internal(dv->a), expr_eval_num_internal(dv->b));
    number_t digamma_a = num_digamma(expr_eval_num_internal(dv->a));
    number_t digamma_b = num_digamma(expr_eval_num_internal(dv->b));
    number_t psi_ab = num_digamma(a_plus_b);
    number_t scale_a = num_sub(digamma_a, psi_ab);
    number_t scale_b = num_sub(digamma_b, psi_ab);

    *a_bar = num_owned_mul_local(*out_bar, scale_a);
    *b_bar = num_owned_mul_local(*out_bar, scale_b);
}

static number_t gammainc_x_density_num(const expr_t *dv)
{
    number_t s_minus_one = num_sub(expr_eval_num_internal(dv->a), NUM_ONE);
    number_t x_pow = num_pow(expr_eval_num_internal(dv->b), s_minus_one);
    number_t neg_x = num_neg(expr_eval_num_internal(dv->b));
    number_t exp_neg_x = num_exp(neg_x);

    return num_mul(x_pow, exp_neg_x);
}

static void expr_reverse_gammainc_x_only(const expr_t *dv,
                                       const number_t *out_bar,
                                       number_t *a_bar,
                                       number_t *b_bar,
                                       int sign,
                                       int regularised)
{
    number_t factor = gammainc_x_density_num(dv);

    if (regularised) {
        number_t gamma_s = num_gamma(expr_eval_num_internal(dv->a));
        factor = num_div(factor, gamma_s);
    }
    if (sign < 0)
        factor = num_neg(factor);

    *a_bar = NUM_NAN;
    *b_bar = num_owned_mul_local(*out_bar, factor);
}

void expr_reverse_gammainc_lower(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    expr_reverse_gammainc_x_only(dv, out_bar, a_bar, b_bar, 1, 0);
}

void expr_reverse_gammainc_upper(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    expr_reverse_gammainc_x_only(dv, out_bar, a_bar, b_bar, -1, 0);
}

void expr_reverse_gammainc_P(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    expr_reverse_gammainc_x_only(dv, out_bar, a_bar, b_bar, 1, 1);
}

void expr_reverse_gammainc_Q(const expr_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    expr_reverse_gammainc_x_only(dv, out_bar, a_bar, b_bar, -1, 1);
}
