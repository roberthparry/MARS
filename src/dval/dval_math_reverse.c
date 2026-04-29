#include <stddef.h>

#include "dval_internal.h"

static inline void dv_reverse_unary(qfloat_t value, qfloat_t *a_bar, qfloat_t *b_bar)
{
    *a_bar = value;
    *b_bar = QF_ZERO;
}

static inline qfloat_t qf_from_int_local(int x)
{
    return qf_from_double((double)x);
}

static inline qfloat_t qf_sq_local(qfloat_t x)
{
    return qf_mul(x, x);
}

void dv_reverse_atan2(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    qfloat_t denom = qf_add(qf_sq_local(dv->a->x.re), qf_sq_local(dv->b->x.re));
    *a_bar = qf_mul(out_bar, qf_div(dv->b->x.re, denom));
    *b_bar = qf_neg(qf_mul(out_bar, qf_div(dv->a->x.re, denom)));
}

void dv_reverse_sin(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    dv_reverse_unary(qf_mul(out_bar, qf_cos(dv->a->x.re)), a_bar, b_bar);
}

void dv_reverse_cos(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    dv_reverse_unary(qf_neg(qf_mul(out_bar, qf_sin(dv->a->x.re))), a_bar, b_bar);
}

void dv_reverse_tan(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    qfloat_t cos_x = qf_cos(dv->a->x.re);
    *a_bar = qf_mul(out_bar, qf_div(QF_ONE, qf_sq_local(cos_x)));
    *b_bar = QF_ZERO;
}

void dv_reverse_sinh(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    dv_reverse_unary(qf_mul(out_bar, qf_cosh(dv->a->x.re)), a_bar, b_bar);
}

void dv_reverse_cosh(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    dv_reverse_unary(qf_mul(out_bar, qf_sinh(dv->a->x.re)), a_bar, b_bar);
}

void dv_reverse_tanh(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    dv_reverse_unary(qf_mul(out_bar, qf_sub(QF_ONE, qf_sq_local(dv->x.re))), a_bar, b_bar);
}

void dv_reverse_asin(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    dv_reverse_unary(qf_div(out_bar, qf_sqrt(qf_sub(QF_ONE, qf_sq_local(dv->a->x.re)))), a_bar, b_bar);
}

void dv_reverse_acos(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    dv_reverse_unary(qf_neg(qf_div(out_bar, qf_sqrt(qf_sub(QF_ONE, qf_sq_local(dv->a->x.re))))), a_bar, b_bar);
}

void dv_reverse_atan(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    dv_reverse_unary(qf_div(out_bar, qf_add(QF_ONE, qf_sq_local(dv->a->x.re))), a_bar, b_bar);
}

void dv_reverse_asinh(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    dv_reverse_unary(qf_div(out_bar, qf_sqrt(qf_add(qf_sq_local(dv->a->x.re), QF_ONE))), a_bar, b_bar);
}

void dv_reverse_acosh(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    qfloat_t denom = qf_mul(qf_sqrt(qf_sub(dv->a->x.re, QF_ONE)),
                            qf_sqrt(qf_add(dv->a->x.re, QF_ONE)));
    *a_bar = qf_div(out_bar, denom);
    *b_bar = QF_ZERO;
}

void dv_reverse_atanh(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    dv_reverse_unary(qf_div(out_bar, qf_sub(QF_ONE, qf_sq_local(dv->a->x.re))), a_bar, b_bar);
}

void dv_reverse_exp(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    dv_reverse_unary(qf_mul(out_bar, dv->x.re), a_bar, b_bar);
}

void dv_reverse_log(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    dv_reverse_unary(qf_div(out_bar, dv->a->x.re), a_bar, b_bar);
}

void dv_reverse_sqrt(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    dv_reverse_unary(qf_div(out_bar, qf_mul(qf_from_int_local(2), dv->x.re)), a_bar, b_bar);
}

void dv_reverse_abs(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    int cmp = qf_cmp(dv->a->x.re, QF_ZERO);
    if (cmp > 0)
        *a_bar = out_bar;
    else if (cmp < 0)
        *a_bar = qf_neg(out_bar);
    else
        *a_bar = QF_ZERO;
    *b_bar = QF_ZERO;
}

void dv_reverse_hypot(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    *a_bar = qf_mul(out_bar, qf_div(dv->a->x.re, dv->x.re));
    *b_bar = qf_mul(out_bar, qf_div(dv->b->x.re, dv->x.re));
}

void dv_reverse_erf(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    qfloat_t scale = qf_div(qf_from_int_local(2), qf_sqrt(QF_PI));
    *a_bar = qf_mul(out_bar, qf_mul(scale, qf_exp(qf_neg(qf_sq_local(dv->a->x.re)))));
    *b_bar = QF_ZERO;
}

void dv_reverse_erfc(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    qfloat_t scale = qf_div(qf_from_int_local(2), qf_sqrt(QF_PI));
    *a_bar = qf_neg(qf_mul(out_bar, qf_mul(scale, qf_exp(qf_neg(qf_sq_local(dv->a->x.re))))));
    *b_bar = QF_ZERO;
}

void dv_reverse_erfinv(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    qfloat_t scale = qf_div(qf_sqrt(QF_PI), qf_from_int_local(2));
    *a_bar = qf_mul(out_bar, qf_mul(scale, qf_exp(qf_sq_local(dv->x.re))));
    *b_bar = QF_ZERO;
}

void dv_reverse_erfcinv(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    qfloat_t scale = qf_div(qf_sqrt(QF_PI), qf_from_int_local(2));
    *a_bar = qf_neg(qf_mul(out_bar, qf_mul(scale, qf_exp(qf_sq_local(dv->x.re)))));
    *b_bar = QF_ZERO;
}

void dv_reverse_gamma(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    dv_reverse_unary(qf_mul(out_bar, qf_mul(dv->x.re, qf_digamma(dv->a->x.re))), a_bar, b_bar);
}

void dv_reverse_lgamma(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    (void)dv;
    dv_reverse_unary(qf_mul(out_bar, qf_digamma(dv->a->x.re)), a_bar, b_bar);
}

void dv_reverse_digamma(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    (void)dv;
    dv_reverse_unary(qf_mul(out_bar, qf_trigamma(dv->a->x.re)), a_bar, b_bar);
}

void dv_reverse_trigamma(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    (void)dv;
    dv_reverse_unary(qf_mul(out_bar, qf_tetragamma(dv->a->x.re)), a_bar, b_bar);
}

static qfloat_t qf_lambert_reverse_factor(qfloat_t z, qfloat_t w)
{
    if (qf_eq(z, QF_ZERO))
        return QF_ONE;
    return qf_div(w, qf_mul(z, qf_add(QF_ONE, w)));
}

void dv_reverse_lambert_w0(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    dv_reverse_unary(qf_mul(out_bar, qf_lambert_reverse_factor(dv->a->x.re, dv->x.re)), a_bar, b_bar);
}

void dv_reverse_lambert_wm1(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    dv_reverse_unary(qf_mul(out_bar, qf_lambert_reverse_factor(dv->a->x.re, dv->x.re)), a_bar, b_bar);
}

void dv_reverse_normal_pdf(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    dv_reverse_unary(qf_neg(qf_mul(out_bar, qf_mul(dv->a->x.re, dv->x.re))), a_bar, b_bar);
}

void dv_reverse_normal_cdf(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    dv_reverse_unary(qf_mul(out_bar, qf_normal_pdf(dv->a->x.re)), a_bar, b_bar);
}

void dv_reverse_normal_logpdf(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    dv_reverse_unary(qf_neg(qf_mul(out_bar, dv->a->x.re)), a_bar, b_bar);
}

void dv_reverse_ei(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    dv_reverse_unary(qf_mul(out_bar, qf_div(qf_exp(dv->a->x.re), dv->a->x.re)), a_bar, b_bar);
}

void dv_reverse_e1(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    dv_reverse_unary(qf_neg(qf_mul(out_bar, qf_div(qf_exp(qf_neg(dv->a->x.re)), dv->a->x.re))), a_bar, b_bar);
}

void dv_reverse_beta(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    qfloat_t psi_ab = qf_digamma(qf_add(dv->a->x.re, dv->b->x.re));
    *a_bar = qf_mul(out_bar, qf_mul(dv->x.re, qf_sub(qf_digamma(dv->a->x.re), psi_ab)));
    *b_bar = qf_mul(out_bar, qf_mul(dv->x.re, qf_sub(qf_digamma(dv->b->x.re), psi_ab)));
}

void dv_reverse_logbeta(const dval_t *dv, qfloat_t out_bar, qfloat_t *a_bar, qfloat_t *b_bar)
{
    qfloat_t psi_ab = qf_digamma(qf_add(dv->a->x.re, dv->b->x.re));
    (void)dv;
    *a_bar = qf_mul(out_bar, qf_sub(qf_digamma(dv->a->x.re), psi_ab));
    *b_bar = qf_mul(out_bar, qf_sub(qf_digamma(dv->b->x.re), psi_ab));
}
