#include <math.h>
#include <stddef.h>

#include "dval_internal.h"
#include "internal/number_internal.h"

static inline void dv_reverse_unary(number_t value, number_t *a_bar, number_t *b_bar)
{
    *a_bar = value;
    *b_bar = NUM_ZERO;
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

void dv_reverse_atan2(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t ax2 = num_sq_local(dv_eval_num_internal(dv->a));
    number_t bx2 = num_sq_local(dv_eval_num_internal(dv->b));
    number_t denom = num_add(ax2, bx2);
    number_t y_over_denom = num_div(dv_eval_num_internal(dv->b), denom);
    number_t x_over_denom = num_div(dv_eval_num_internal(dv->a), denom);
    number_t scaled_x = num_mul(*out_bar, x_over_denom);

    *a_bar = num_owned_mul_local(*out_bar, y_over_denom);
    *b_bar = num_owned_neg_local(scaled_x);
}

void dv_reverse_sin(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t cos_x = num_cos(dv_eval_num_internal(dv->a));
    number_t factor = num_mul(*out_bar, cos_x);

    dv_reverse_unary(num_owned_clone_local(factor), a_bar, b_bar);
}

void dv_reverse_cos(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t sin_x = num_sin(dv_eval_num_internal(dv->a));
    number_t product = num_mul(*out_bar, sin_x);

    dv_reverse_unary(num_owned_neg_local(product), a_bar, b_bar);
}

void dv_reverse_tan(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t cos_x = num_cos(dv_eval_num_internal(dv->a));
    number_t cos_sq = num_sq_local(cos_x);
    number_t inv = num_inverse_local(cos_sq);

    dv_reverse_unary(num_owned_mul_local(*out_bar, inv), a_bar, b_bar);
}

void dv_reverse_sinh(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t cosh_x = num_cosh(dv_eval_num_internal(dv->a));
    number_t factor = num_mul(*out_bar, cosh_x);

    dv_reverse_unary(num_owned_clone_local(factor), a_bar, b_bar);
}

void dv_reverse_cosh(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t sinh_x = num_sinh(dv_eval_num_internal(dv->a));
    number_t factor = num_mul(*out_bar, sinh_x);

    dv_reverse_unary(num_owned_clone_local(factor), a_bar, b_bar);
}

void dv_reverse_tanh(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t dv_sq = num_sq_local(dv_eval_num_internal(dv));
    number_t factor = num_sub(NUM_ONE, dv_sq);

    dv_reverse_unary(num_owned_mul_local(*out_bar, factor), a_bar, b_bar);
}

void dv_reverse_asin(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t x_sq = num_sq_local(dv_eval_num_internal(dv->a));
    number_t inner = num_sub(NUM_ONE, x_sq);
    number_t denom = num_sqrt(inner);

    dv_reverse_unary(num_owned_div_local(*out_bar, denom), a_bar, b_bar);
}

void dv_reverse_acos(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t x_sq = num_sq_local(dv_eval_num_internal(dv->a));
    number_t inner = num_sub(NUM_ONE, x_sq);
    number_t denom = num_sqrt(inner);
    number_t frac = num_div(*out_bar, denom);

    dv_reverse_unary(num_owned_neg_local(frac), a_bar, b_bar);
}

void dv_reverse_atan(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t x_sq = num_sq_local(dv_eval_num_internal(dv->a));
    number_t denom = num_add(NUM_ONE, x_sq);

    dv_reverse_unary(num_owned_div_local(*out_bar, denom), a_bar, b_bar);
}

void dv_reverse_asinh(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t x_sq = num_sq_local(dv_eval_num_internal(dv->a));
    number_t inner = num_add(x_sq, NUM_ONE);
    number_t denom = num_sqrt(inner);

    dv_reverse_unary(num_owned_div_local(*out_bar, denom), a_bar, b_bar);
}

void dv_reverse_acosh(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t xm1 = num_sub(dv_eval_num_internal(dv->a), NUM_ONE);
    number_t xp1 = num_add(dv_eval_num_internal(dv->a), NUM_ONE);
    number_t sqrt_xm1 = num_sqrt(xm1);
    number_t sqrt_xp1 = num_sqrt(xp1);
    number_t denom = num_mul(sqrt_xm1, sqrt_xp1);

    dv_reverse_unary(num_owned_div_local(*out_bar, denom), a_bar, b_bar);
}

void dv_reverse_atanh(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t x_sq = num_sq_local(dv_eval_num_internal(dv->a));
    number_t denom = num_sub(NUM_ONE, x_sq);

    dv_reverse_unary(num_owned_div_local(*out_bar, denom), a_bar, b_bar);
}

void dv_reverse_exp(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t factor = num_mul(*out_bar, dv_eval_num_internal(dv));

    dv_reverse_unary(num_owned_clone_local(factor), a_bar, b_bar);
}

void dv_reverse_log(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t factor = num_div(*out_bar, dv_eval_num_internal(dv->a));

    dv_reverse_unary(num_owned_clone_local(factor), a_bar, b_bar);
}

void dv_reverse_log10(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t denom = num_mul(dv_eval_num_internal(dv->a), NUM_LN10);

    dv_reverse_unary(num_owned_div_local(*out_bar, denom), a_bar, b_bar);
}

void dv_reverse_sqrt(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t denom = num_mul(NUM_TWO, dv_eval_num_internal(dv));

    dv_reverse_unary(num_owned_div_local(*out_bar, denom), a_bar, b_bar);
}

void dv_reverse_floor(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    (void)dv;
    (void)out_bar;
    dv_reverse_unary(NUM_ZERO, a_bar, b_bar);
}

void dv_reverse_ceil(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    (void)dv;
    (void)out_bar;
    dv_reverse_unary(NUM_ZERO, a_bar, b_bar);
}

void dv_reverse_abs(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    if (!num_is_real(dv_eval_num_internal(dv->a))) {
        *a_bar = NUM_NAN;
        *b_bar = NUM_ZERO;
        return;
    }
    switch (num_cmp(dv_eval_num_internal(dv->a), NUM_ZERO)) {
    case 1:
        *a_bar = num_owned_clone_local(*out_bar);
        break;
    case -1:
        *a_bar = num_owned_neg_local(*out_bar);
        break;
    default:
        *a_bar = NUM_ZERO;
        break;
    }
    *b_bar = NUM_ZERO;
}

void dv_reverse_hypot(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t ax = num_div(dv_eval_num_internal(dv->a), dv_eval_num_internal(dv));
    number_t bx = num_div(dv_eval_num_internal(dv->b), dv_eval_num_internal(dv));

    *a_bar = num_owned_mul_local(*out_bar, ax);
    *b_bar = num_owned_mul_local(*out_bar, bx);
}

void dv_reverse_erf(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t x_sq = num_sq_local(dv_eval_num_internal(dv->a));
    number_t neg_x_sq = num_neg(x_sq);
    number_t exp_term = num_exp(neg_x_sq);
    number_t factor = num_mul(NUM_2_SQRTPI, exp_term);

    dv_reverse_unary(num_owned_mul_local(*out_bar, factor), a_bar, b_bar);
}

void dv_reverse_erfc(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t x_sq = num_sq_local(dv_eval_num_internal(dv->a));
    number_t neg_x_sq = num_neg(x_sq);
    number_t exp_term = num_exp(neg_x_sq);
    number_t factor = num_mul(NUM_NEG_TWO_OVER_SQRT_PI, exp_term);

    dv_reverse_unary(num_owned_mul_local(*out_bar, factor), a_bar, b_bar);
}

void dv_reverse_erfinv(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t y_sq = num_sq_local(dv_eval_num_internal(dv));
    number_t exp_term = num_exp(y_sq);
    number_t scale = num_div(NUM_SQRT_PI, NUM_TWO);
    number_t factor = num_mul(scale, exp_term);

    dv_reverse_unary(num_owned_mul_local(*out_bar, factor), a_bar, b_bar);
}

void dv_reverse_erfcinv(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t y_sq = num_sq_local(dv_eval_num_internal(dv));
    number_t scale = num_div(NUM_SQRT_PI, NUM_TWO);
    number_t neg_scale = num_neg(scale);
    number_t exp_term = num_exp(y_sq);
    number_t factor = num_mul(neg_scale, exp_term);

    dv_reverse_unary(num_owned_mul_local(*out_bar, factor), a_bar, b_bar);
}

void dv_reverse_gamma(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t digamma_x = num_digamma(dv_eval_num_internal(dv->a));
    number_t factor = num_mul(dv_eval_num_internal(dv), digamma_x);

    dv_reverse_unary(num_owned_mul_local(*out_bar, factor), a_bar, b_bar);
}

void dv_reverse_lgamma(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    (void)dv;
    number_t digamma_x = num_digamma(dv_eval_num_internal(dv->a));
    number_t factor = num_mul(*out_bar, digamma_x);

    dv_reverse_unary(num_owned_clone_local(factor), a_bar, b_bar);
}

void dv_reverse_digamma(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    (void)dv;
    number_t trigamma_x = num_trigamma(dv_eval_num_internal(dv->a));
    number_t factor = num_mul(*out_bar, trigamma_x);

    dv_reverse_unary(num_owned_clone_local(factor), a_bar, b_bar);
}

void dv_reverse_trigamma(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    (void)dv;
    number_t tetragamma_x = num_tetragamma(dv_eval_num_internal(dv->a));
    number_t factor = num_mul(*out_bar, tetragamma_x);

    dv_reverse_unary(num_owned_clone_local(factor), a_bar, b_bar);
}

void dv_reverse_gammainv(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t y = dv_eval_num_internal(dv);
    number_t psi_y = num_digamma(y);
    number_t x_psi = num_mul(dv_eval_num_internal(dv->a), psi_y);
    number_t factor = num_inv(x_psi);

    dv_reverse_unary(num_owned_mul_local(*out_bar, factor), a_bar, b_bar);
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

void dv_reverse_lambert_w0(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t factor = num_lambert_reverse_factor(dv_eval_num_internal(dv->a), dv_eval_num_internal(dv));

    dv_reverse_unary(num_owned_mul_local(*out_bar, factor), a_bar, b_bar);
}

void dv_reverse_lambert_wm1(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t factor = num_lambert_reverse_factor(dv_eval_num_internal(dv->a), dv_eval_num_internal(dv));

    dv_reverse_unary(num_owned_mul_local(*out_bar, factor), a_bar, b_bar);
}

void dv_reverse_normal_pdf(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t x_pdf = num_mul(dv_eval_num_internal(dv->a), dv_eval_num_internal(dv));
    number_t factor = num_neg(x_pdf);

    dv_reverse_unary(num_owned_mul_local(*out_bar, factor), a_bar, b_bar);
}

void dv_reverse_normal_cdf(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t pdf_x = num_normal_pdf(dv_eval_num_internal(dv->a));
    number_t factor = num_mul(*out_bar, pdf_x);

    dv_reverse_unary(num_owned_clone_local(factor), a_bar, b_bar);
}

void dv_reverse_normal_logpdf(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t neg_x = num_neg(dv_eval_num_internal(dv->a));
    number_t factor = num_mul(*out_bar, neg_x);

    dv_reverse_unary(num_owned_clone_local(factor), a_bar, b_bar);
}

void dv_reverse_ei(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t exp_x = num_exp(dv_eval_num_internal(dv->a));
    number_t factor = num_div(exp_x, dv_eval_num_internal(dv->a));

    dv_reverse_unary(num_owned_mul_local(*out_bar, factor), a_bar, b_bar);
}

void dv_reverse_e1(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t neg_x = num_neg(dv_eval_num_internal(dv->a));
    number_t exp_neg_x = num_exp(neg_x);
    number_t factor = num_div(exp_neg_x, dv_eval_num_internal(dv->a));
    number_t neg_factor = num_neg(factor);

    dv_reverse_unary(num_owned_mul_local(*out_bar, neg_factor), a_bar, b_bar);
}

void dv_reverse_beta(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t a_plus_b = num_add(dv_eval_num_internal(dv->a), dv_eval_num_internal(dv->b));
    number_t digamma_a = num_digamma(dv_eval_num_internal(dv->a));
    number_t digamma_b = num_digamma(dv_eval_num_internal(dv->b));
    number_t psi_ab = num_digamma(a_plus_b);
    number_t psi_a_minus = num_sub(digamma_a, psi_ab);
    number_t psi_b_minus = num_sub(digamma_b, psi_ab);
    number_t scale_a = num_mul(dv_eval_num_internal(dv), psi_a_minus);
    number_t scale_b = num_mul(dv_eval_num_internal(dv), psi_b_minus);

    *a_bar = num_owned_mul_local(*out_bar, scale_a);
    *b_bar = num_owned_mul_local(*out_bar, scale_b);
}

void dv_reverse_logbeta(const dval_t *dv, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t a_plus_b = num_add(dv_eval_num_internal(dv->a), dv_eval_num_internal(dv->b));
    number_t digamma_a = num_digamma(dv_eval_num_internal(dv->a));
    number_t digamma_b = num_digamma(dv_eval_num_internal(dv->b));
    number_t psi_ab = num_digamma(a_plus_b);
    number_t scale_a = num_sub(digamma_a, psi_ab);
    number_t scale_b = num_sub(digamma_b, psi_ab);

    *a_bar = num_owned_mul_local(*out_bar, scale_a);
    *b_bar = num_owned_mul_local(*out_bar, scale_b);
}
