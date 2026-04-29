#include <stddef.h>

#include "dval_math_internal.h"

static inline qcomplex_t dv_eval_unary_qc(dval_t *dv, qcomplex_t (*fn)(qcomplex_t))
{
    return fn(dv_eval_qc_internal(dv->a));
}

static inline qcomplex_t dv_eval_unary_real(dval_t *dv, qfloat_t (*fn)(qcomplex_t))
{
    return dv_qc_real_qf(fn(dv_eval_qc_internal(dv->a)));
}

qcomplex_t eval_sin(dval_t *dv) { return dv_eval_unary_qc(dv, qc_sin); }
qcomplex_t eval_cos(dval_t *dv) { return dv_eval_unary_qc(dv, qc_cos); }
qcomplex_t eval_tan(dval_t *dv) { return dv_eval_unary_qc(dv, qc_tan); }

qcomplex_t eval_sinh(dval_t *dv) { return dv_eval_unary_qc(dv, qc_sinh); }
qcomplex_t eval_cosh(dval_t *dv) { return dv_eval_unary_qc(dv, qc_cosh); }
qcomplex_t eval_tanh(dval_t *dv) { return dv_eval_unary_qc(dv, qc_tanh); }

qcomplex_t eval_asin(dval_t *dv) { return dv_eval_unary_qc(dv, qc_asin); }
qcomplex_t eval_acos(dval_t *dv) { return dv_eval_unary_qc(dv, qc_acos); }
qcomplex_t eval_atan(dval_t *dv) { return dv_eval_unary_qc(dv, qc_atan); }

qcomplex_t eval_asinh(dval_t *dv) { return dv_eval_unary_qc(dv, qc_asinh); }
qcomplex_t eval_acosh(dval_t *dv) { return dv_eval_unary_qc(dv, qc_acosh); }
qcomplex_t eval_atanh(dval_t *dv) { return dv_eval_unary_qc(dv, qc_atanh); }

qcomplex_t eval_exp(dval_t *dv) { return dv_eval_unary_qc(dv, qc_exp); }
qcomplex_t eval_log(dval_t *dv) { return dv_eval_unary_qc(dv, qc_log); }
qcomplex_t eval_sqrt(dval_t *dv) { return dv_eval_unary_qc(dv, qc_sqrt); }
qcomplex_t eval_abs(dval_t *dv) { return dv_eval_unary_real(dv, qc_abs); }
qcomplex_t eval_erf(dval_t *dv) { return dv_eval_unary_qc(dv, qc_erf); }
qcomplex_t eval_erfc(dval_t *dv) { return dv_eval_unary_qc(dv, qc_erfc); }
qcomplex_t eval_lgamma(dval_t *dv) { return dv_eval_unary_qc(dv, qc_lgamma); }
qcomplex_t eval_erfinv(dval_t *dv) { return dv_eval_unary_qc(dv, qc_erfinv); }
qcomplex_t eval_erfcinv(dval_t *dv) { return dv_eval_unary_qc(dv, qc_erfcinv); }
qcomplex_t eval_gamma(dval_t *dv) { return dv_eval_unary_qc(dv, qc_gamma); }
qcomplex_t eval_digamma(dval_t *dv) { return dv_eval_unary_qc(dv, qc_digamma); }
qcomplex_t eval_trigamma(dval_t *dv) { return dv_eval_unary_qc(dv, qc_trigamma); }
qcomplex_t eval_lambert_w0(dval_t *dv) { return dv_eval_unary_qc(dv, qc_productlog); }
qcomplex_t eval_lambert_wm1(dval_t *dv) { return dv_eval_unary_qc(dv, qc_lambert_wm1); }
qcomplex_t eval_normal_pdf(dval_t *dv) { return dv_eval_unary_qc(dv, qc_normal_pdf); }
qcomplex_t eval_normal_cdf(dval_t *dv) { return dv_eval_unary_qc(dv, qc_normal_cdf); }
qcomplex_t eval_normal_logpdf(dval_t *dv) { return dv_eval_unary_qc(dv, qc_normal_logpdf); }
qcomplex_t eval_ei(dval_t *dv) { return dv_eval_unary_qc(dv, qc_ei); }
qcomplex_t eval_e1(dval_t *dv) { return dv_eval_unary_qc(dv, qc_e1); }

qcomplex_t eval_hypot(dval_t *dv)
{
    return qc_hypot(dv_eval_qc_internal(dv->a), dv_eval_qc_internal(dv->b));
}

qcomplex_t eval_beta(dval_t *dv)
{
    return qc_beta(dv_eval_qc_internal(dv->a), dv_eval_qc_internal(dv->b));
}

qcomplex_t eval_logbeta(dval_t *dv)
{
    return qc_logbeta(dv_eval_qc_internal(dv->a), dv_eval_qc_internal(dv->b));
}

qcomplex_t eval_atan2(dval_t *dv)
{
    qcomplex_t fy = dv_eval_qc_internal(dv->a);
    qcomplex_t gx = dv_eval_qc_internal(dv->b);
    return qc_atan2(fy, gx);
}

dval_t *deriv_sin(dval_t *dv)
{
    dval_t *da  = dv_get_dx_internal(dv->a);
    dval_t *ca  = dv_cos(dv->a);
    dval_t *out = dv_mul(ca, da);
    dv_free(ca);
    dv_free(da);
    return out;
}

dval_t *deriv_cos(dval_t *dv)
{
    dval_t *da      = dv_get_dx_internal(dv->a);
    dval_t *sin_a   = dv_sin(dv->a);
    dval_t *neg_sin = dv_neg(sin_a);
    dv_free(sin_a);
    dval_t *out     = dv_mul(neg_sin, da);
    dv_free(da);
    dv_free(neg_sin);
    return out;
}

dval_t *deriv_tan(dval_t *dv)
{
    dval_t *da  = dv_get_dx_internal(dv->a);
    dval_t *t   = dv_tan(dv->a);
    dval_t *t2  = dv_pow_d(t, 2.0);
    dval_t *one = dv_new_const_d(1.0);
    dval_t *fac = dv_add(one, t2);
    dval_t *out = dv_mul(fac, da);
    dv_free(da);
    dv_free(t);
    dv_free(t2);
    dv_free(one);
    dv_free(fac);
    return out;
}

dval_t *deriv_sinh(dval_t *dv)
{
    dval_t *da   = dv_get_dx_internal(dv->a);
    dval_t *ca   = dv_cosh(dv->a);
    dval_t *out  = dv_mul(ca, da);
    dv_free(ca);
    dv_free(da);
    return out;
}

dval_t *deriv_cosh(dval_t *dv)
{
    dval_t *da   = dv_get_dx_internal(dv->a);
    dval_t *sa   = dv_sinh(dv->a);
    dval_t *out  = dv_mul(sa, da);
    dv_free(sa);
    dv_free(da);
    return out;
}

dval_t *deriv_tanh(dval_t *dv)
{
    dval_t *da  = dv_get_dx_internal(dv->a);
    dval_t *t   = dv_tanh(dv->a);
    dval_t *t2  = dv_pow_d(t, 2.0);
    dval_t *one = dv_new_const_d(1.0);
    dval_t *fac = dv_sub(one, t2);
    dval_t *out = dv_mul(fac, da);
    dv_free(da);
    dv_free(t);
    dv_free(t2);
    dv_free(one);
    dv_free(fac);
    return out;
}

dval_t *deriv_exp(dval_t *dv)
{
    dval_t *da   = dv_get_dx_internal(dv->a);
    dval_t *ea   = dv_exp(dv->a);
    dval_t *out  = dv_mul(ea, da);
    dv_free(ea);
    dv_free(da);
    return out;
}

dval_t *deriv_log(dval_t *dv)
{
    dval_t *da  = dv_get_dx_internal(dv->a);
    dval_t *out = dv_div(da, dv->a);
    dv_free(da);
    return out;
}

dval_t *deriv_sqrt(dval_t *dv)
{
    dval_t *da   = dv_get_dx_internal(dv->a);
    dval_t *two  = dv_new_const_d(2.0);
    dval_t *sqra = dv_sqrt(dv->a);
    dval_t *den  = dv_mul(two, sqra);
    dv_free(sqra);
    dval_t *out  = dv_div(da, den);
    dv_free(da);
    dv_free(two);
    dv_free(den);
    return out;
}

dval_t *deriv_asin(dval_t *dv)
{
    dval_t *da   = dv_get_dx_internal(dv->a);
    dval_t *a2   = dv_pow_d(dv->a, 2.0);
    dval_t *one  = dv_new_const_d(1.0);
    dval_t *sub  = dv_sub(one, a2);
    dval_t *den  = dv_sqrt(sub);
    dv_free(sub);
    dval_t *out  = dv_div(da, den);
    dv_free(da);
    dv_free(a2);
    dv_free(one);
    dv_free(den);
    return out;
}

dval_t *deriv_acos(dval_t *dv)
{
    dval_t *da   = dv_get_dx_internal(dv->a);
    dval_t *a2   = dv_pow_d(dv->a, 2.0);
    dval_t *one  = dv_new_const_d(1.0);
    dval_t *sub  = dv_sub(one, a2);
    dval_t *den  = dv_sqrt(sub);
    dv_free(sub);
    dval_t *num  = dv_neg(da);
    dval_t *out  = dv_div(num, den);
    dv_free(da);
    dv_free(a2);
    dv_free(one);
    dv_free(den);
    dv_free(num);
    return out;
}

dval_t *deriv_atan(dval_t *dv)
{
    dval_t *da  = dv_get_dx_internal(dv->a);
    dval_t *a2  = dv_pow_d(dv->a, 2.0);
    dval_t *one = dv_new_const_d(1.0);
    dval_t *den = dv_add(one, a2);
    dval_t *out = dv_div(da, den);
    dv_free(da);
    dv_free(a2);
    dv_free(one);
    dv_free(den);
    return out;
}

dval_t *deriv_atan2(dval_t *dv)
{
    dval_t *y  = dv->a;
    dval_t *x  = dv->b;
    dval_t *dy = dv_get_dx_internal(y);
    dval_t *dx = dv_get_dx_internal(x);
    dval_t *x_dy = dv_mul(x, dy);
    dval_t *y_dx = dv_mul(y, dx);
    dval_t *num  = dv_sub(x_dy, y_dx);
    dval_t *x2  = dv_mul(x, x);
    dval_t *y2  = dv_mul(y, y);
    dval_t *den = dv_add(x2, y2);
    dval_t *out = dv_div(num, den);
    dv_free(dy);
    dv_free(dx);
    dv_free(x_dy);
    dv_free(y_dx);
    dv_free(num);
    dv_free(x2);
    dv_free(y2);
    dv_free(den);
    return out;
}

dval_t *deriv_asinh(dval_t *dv)
{
    dval_t *da   = dv_get_dx_internal(dv->a);
    dval_t *a2   = dv_pow_d(dv->a, 2.0);
    dval_t *one  = dv_new_const_d(1.0);
    dval_t *sum  = dv_add(one, a2);
    dval_t *den  = dv_sqrt(sum);
    dv_free(sum);
    dval_t *out  = dv_div(da, den);
    dv_free(da);
    dv_free(a2);
    dv_free(one);
    dv_free(den);
    return out;
}

dval_t *deriv_acosh(dval_t *dv)
{
    dval_t *da  = dv_get_dx_internal(dv->a);
    dval_t *one = dv_new_const_d(1.0);
    dval_t *am1 = dv_sub(dv->a, one);
    dval_t *ap1 = dv_add(dv->a, one);
    dval_t *s1  = dv_sqrt(am1);
    dval_t *s2  = dv_sqrt(ap1);
    dval_t *den = dv_mul(s1, s2);
    dval_t *out = dv_div(da, den);
    dv_free(da);
    dv_free(one);
    dv_free(am1);
    dv_free(ap1);
    dv_free(s1);
    dv_free(s2);
    dv_free(den);
    return out;
}

dval_t *deriv_atanh(dval_t *dv)
{
    dval_t *da  = dv_get_dx_internal(dv->a);
    dval_t *a2  = dv_pow_d(dv->a, 2.0);
    dval_t *one = dv_new_const_d(1.0);
    dval_t *den = dv_sub(one, a2);
    dval_t *out = dv_div(da, den);
    dv_free(da);
    dv_free(a2);
    dv_free(one);
    dv_free(den);
    return out;
}

dval_t *deriv_abs(dval_t *dv)
{
    dval_t *da   = dv_get_dx_internal(dv->a);
    dval_t *absa = dv_abs(dv->a);
    dval_t *sign = dv_div(dv->a, absa);
    dval_t *out  = dv_mul(sign, da);
    dv_free(da);
    dv_free(absa);
    dv_free(sign);
    return out;
}

static qfloat_t two_over_sqrtpi(void)
{
    qfloat_t pi = qf_mul(qf_from_double(4.0), qf_atan(qf_from_double(1.0)));
    return qf_div(qf_from_double(2.0), qf_sqrt(pi));
}

dval_t *deriv_erf(dval_t *dv)
{
    dval_t *da     = dv_get_dx_internal(dv->a);
    dval_t *c      = dv_new_const(two_over_sqrtpi());
    dval_t *a2     = dv_pow_d(dv->a, 2.0);
    dval_t *neg_a2 = dv_neg(a2);
    dval_t *ea2    = dv_exp(neg_a2);
    dval_t *fac    = dv_mul(c, ea2);
    dval_t *out    = dv_mul(fac, da);
    dv_free(da);
    dv_free(c);
    dv_free(a2);
    dv_free(neg_a2);
    dv_free(ea2);
    dv_free(fac);
    return out;
}

dval_t *deriv_erfc(dval_t *dv)
{
    dval_t *da     = dv_get_dx_internal(dv->a);
    dval_t *c      = dv_new_const(qf_neg(two_over_sqrtpi()));
    dval_t *a2     = dv_pow_d(dv->a, 2.0);
    dval_t *neg_a2 = dv_neg(a2);
    dval_t *ea2    = dv_exp(neg_a2);
    dval_t *fac    = dv_mul(c, ea2);
    dval_t *out    = dv_mul(fac, da);
    dv_free(da);
    dv_free(c);
    dv_free(a2);
    dv_free(neg_a2);
    dv_free(ea2);
    dv_free(fac);
    return out;
}

dval_t *deriv_lgamma(dval_t *dv)
{
    dval_t *da  = dv_get_dx_internal(dv->a);
    dval_t *dg  = dv_digamma(dv->a);
    dval_t *out = dv_mul(dg, da);
    dv_free(da);
    dv_free(dg);
    return out;
}

dval_t *deriv_hypot(dval_t *dv)
{
    dval_t *a    = dv->a;
    dval_t *b    = dv->b;
    dval_t *da   = dv_get_dx_internal(a);
    dval_t *db   = dv_get_dx_internal(b);
    dval_t *a_da = dv_mul(a, da);
    dval_t *b_db = dv_mul(b, db);
    dval_t *num  = dv_add(a_da, b_db);
    dval_t *h    = dv_hypot(a, b);
    dval_t *out  = dv_div(num, h);
    dv_free(da);
    dv_free(db);
    dv_free(a_da);
    dv_free(b_db);
    dv_free(num);
    dv_free(h);
    return out;
}

static qfloat_t sqrtpi_over_2(void)
{
    qfloat_t pi = qf_mul(qf_from_double(4.0), qf_atan(qf_from_double(1.0)));
    return qf_div(qf_sqrt(pi), qf_from_double(2.0));
}

dval_t *deriv_erfinv(dval_t *dv)
{
    dval_t *da  = dv_get_dx_internal(dv->a);
    dval_t *w   = dv_erfinv(dv->a);
    dval_t *w2  = dv_pow_d(w, 2.0);
    dval_t *ew2 = dv_exp(w2);
    dval_t *c   = dv_new_const(sqrtpi_over_2());
    dval_t *fac = dv_mul(c, ew2);
    dval_t *out = dv_mul(fac, da);
    dv_free(da); dv_free(w); dv_free(w2); dv_free(ew2); dv_free(c); dv_free(fac);
    return out;
}

dval_t *deriv_erfcinv(dval_t *dv)
{
    dval_t *da  = dv_get_dx_internal(dv->a);
    dval_t *w   = dv_erfcinv(dv->a);
    dval_t *w2  = dv_pow_d(w, 2.0);
    dval_t *ew2 = dv_exp(w2);
    dval_t *c   = dv_new_const(qf_neg(sqrtpi_over_2()));
    dval_t *fac = dv_mul(c, ew2);
    dval_t *out = dv_mul(fac, da);
    dv_free(da); dv_free(w); dv_free(w2); dv_free(ew2); dv_free(c); dv_free(fac);
    return out;
}

dval_t *deriv_gamma(dval_t *dv)
{
    dval_t *da  = dv_get_dx_internal(dv->a);
    dval_t *g   = dv_gamma(dv->a);
    dval_t *dg  = dv_digamma(dv->a);
    dval_t *gdg = dv_mul(g, dg);
    dval_t *out = dv_mul(gdg, da);
    dv_free(da); dv_free(g); dv_free(dg); dv_free(gdg);
    return out;
}

dval_t *deriv_digamma(dval_t *dv)
{
    dval_t *da  = dv_get_dx_internal(dv->a);
    dval_t *tg  = dv_trigamma(dv->a);
    dval_t *out = dv_mul(tg, da);
    dv_free(da); dv_free(tg);
    return out;
}

dval_t *deriv_trigamma(dval_t *dv)
{
    qfloat_t t2     = qf_tetragamma(dv_eval_qf(dv->a));
    dval_t *da    = dv_get_dx_internal(dv->a);
    dval_t *coeff = dv_new_const(t2);
    dval_t *out   = dv_mul(coeff, da);
    dv_free(da); dv_free(coeff);
    return out;
}

dval_t *deriv_lambert_w0(dval_t *dv)
{
    dval_t *da  = dv_get_dx_internal(dv->a);
    dval_t *w   = dv_lambert_w0(dv->a);
    dval_t *wp1 = dv_add_d(w, 1.0);
    dval_t *den = dv_mul(dv->a, wp1);
    dval_t *fac = dv_div(w, den);
    dval_t *out = dv_mul(fac, da);
    dv_free(da); dv_free(w); dv_free(wp1); dv_free(den); dv_free(fac);
    return out;
}

dval_t *deriv_lambert_wm1(dval_t *dv)
{
    dval_t *da  = dv_get_dx_internal(dv->a);
    dval_t *w   = dv_lambert_wm1(dv->a);
    dval_t *wp1 = dv_add_d(w, 1.0);
    dval_t *den = dv_mul(dv->a, wp1);
    dval_t *fac = dv_div(w, den);
    dval_t *out = dv_mul(fac, da);
    dv_free(da); dv_free(w); dv_free(wp1); dv_free(den); dv_free(fac);
    return out;
}

dval_t *deriv_normal_pdf(dval_t *dv)
{
    dval_t *da   = dv_get_dx_internal(dv->a);
    dval_t *neg_a = dv_neg(dv->a);
    dval_t *phi  = dv_normal_pdf(dv->a);
    dval_t *fac  = dv_mul(neg_a, phi);
    dval_t *out  = dv_mul(fac, da);
    dv_free(da); dv_free(neg_a); dv_free(phi); dv_free(fac);
    return out;
}

dval_t *deriv_normal_cdf(dval_t *dv)
{
    dval_t *da  = dv_get_dx_internal(dv->a);
    dval_t *phi = dv_normal_pdf(dv->a);
    dval_t *out = dv_mul(phi, da);
    dv_free(da); dv_free(phi);
    return out;
}

dval_t *deriv_normal_logpdf(dval_t *dv)
{
    dval_t *da    = dv_get_dx_internal(dv->a);
    dval_t *neg_a = dv_neg(dv->a);
    dval_t *out   = dv_mul(neg_a, da);
    dv_free(da); dv_free(neg_a);
    return out;
}

dval_t *deriv_ei(dval_t *dv)
{
    dval_t *da  = dv_get_dx_internal(dv->a);
    dval_t *ea  = dv_exp(dv->a);
    dval_t *fac = dv_div(ea, dv->a);
    dval_t *out = dv_mul(fac, da);
    dv_free(da); dv_free(ea); dv_free(fac);
    return out;
}

dval_t *deriv_e1(dval_t *dv)
{
    dval_t *da    = dv_get_dx_internal(dv->a);
    dval_t *neg_a = dv_neg(dv->a);
    dval_t *en_a  = dv_exp(neg_a);
    dval_t *neg_en = dv_neg(en_a);
    dval_t *fac   = dv_div(neg_en, dv->a);
    dval_t *out   = dv_mul(fac, da);
    dv_free(da); dv_free(neg_a); dv_free(en_a); dv_free(neg_en); dv_free(fac);
    return out;
}

dval_t *deriv_beta(dval_t *dv)
{
    dval_t *da    = dv_get_dx_internal(dv->a);
    dval_t *db    = dv_get_dx_internal(dv->b);
    dval_t *apb   = dv_add(dv->a, dv->b);
    dval_t *dg_a  = dv_digamma(dv->a);
    dval_t *dg_b  = dv_digamma(dv->b);
    dval_t *dg_ab = dv_digamma(apb);
    dval_t *diff_a = dv_sub(dg_a, dg_ab);
    dval_t *diff_b = dv_sub(dg_b, dg_ab);
    dval_t *beta_n = dv_beta(dv->a, dv->b);
    dval_t *ca    = dv_mul(beta_n, diff_a);
    dval_t *cb    = dv_mul(beta_n, diff_b);
    dval_t *ta    = dv_mul(ca, da);
    dval_t *tb    = dv_mul(cb, db);
    dval_t *out   = dv_add(ta, tb);
    dv_free(da); dv_free(db); dv_free(apb);
    dv_free(dg_a); dv_free(dg_b); dv_free(dg_ab);
    dv_free(diff_a); dv_free(diff_b); dv_free(beta_n);
    dv_free(ca); dv_free(cb); dv_free(ta); dv_free(tb);
    return out;
}

dval_t *deriv_logbeta(dval_t *dv)
{
    dval_t *da    = dv_get_dx_internal(dv->a);
    dval_t *db    = dv_get_dx_internal(dv->b);
    dval_t *apb   = dv_add(dv->a, dv->b);
    dval_t *dg_a  = dv_digamma(dv->a);
    dval_t *dg_b  = dv_digamma(dv->b);
    dval_t *dg_ab = dv_digamma(apb);
    dval_t *diff_a = dv_sub(dg_a, dg_ab);
    dval_t *diff_b = dv_sub(dg_b, dg_ab);
    dval_t *ta    = dv_mul(diff_a, da);
    dval_t *tb    = dv_mul(diff_b, db);
    dval_t *out   = dv_add(ta, tb);
    dv_free(da); dv_free(db); dv_free(apb);
    dv_free(dg_a); dv_free(dg_b); dv_free(dg_ab);
    dv_free(diff_a); dv_free(diff_b); dv_free(ta); dv_free(tb);
    return out;
}
