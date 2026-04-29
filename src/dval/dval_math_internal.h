#ifndef DVAL_MATH_INTERNAL_H
#define DVAL_MATH_INTERNAL_H

#include "dval_internal.h"

qcomplex_t eval_sin(dval_t *dv);
qcomplex_t eval_cos(dval_t *dv);
qcomplex_t eval_tan(dval_t *dv);
qcomplex_t eval_sinh(dval_t *dv);
qcomplex_t eval_cosh(dval_t *dv);
qcomplex_t eval_tanh(dval_t *dv);
qcomplex_t eval_asin(dval_t *dv);
qcomplex_t eval_acos(dval_t *dv);
qcomplex_t eval_atan(dval_t *dv);
qcomplex_t eval_asinh(dval_t *dv);
qcomplex_t eval_acosh(dval_t *dv);
qcomplex_t eval_atanh(dval_t *dv);
qcomplex_t eval_exp(dval_t *dv);
qcomplex_t eval_log(dval_t *dv);
qcomplex_t eval_sqrt(dval_t *dv);
qcomplex_t eval_abs(dval_t *dv);
qcomplex_t eval_erf(dval_t *dv);
qcomplex_t eval_erfc(dval_t *dv);
qcomplex_t eval_lgamma(dval_t *dv);
qcomplex_t eval_erfinv(dval_t *dv);
qcomplex_t eval_erfcinv(dval_t *dv);
qcomplex_t eval_gamma(dval_t *dv);
qcomplex_t eval_digamma(dval_t *dv);
qcomplex_t eval_trigamma(dval_t *dv);
qcomplex_t eval_lambert_w0(dval_t *dv);
qcomplex_t eval_lambert_wm1(dval_t *dv);
qcomplex_t eval_normal_pdf(dval_t *dv);
qcomplex_t eval_normal_cdf(dval_t *dv);
qcomplex_t eval_normal_logpdf(dval_t *dv);
qcomplex_t eval_ei(dval_t *dv);
qcomplex_t eval_e1(dval_t *dv);
qcomplex_t eval_hypot(dval_t *dv);
qcomplex_t eval_beta(dval_t *dv);
qcomplex_t eval_logbeta(dval_t *dv);
qcomplex_t eval_atan2(dval_t *dv);

dval_t *deriv_sin(dval_t *dv);
dval_t *deriv_cos(dval_t *dv);
dval_t *deriv_tan(dval_t *dv);
dval_t *deriv_sinh(dval_t *dv);
dval_t *deriv_cosh(dval_t *dv);
dval_t *deriv_tanh(dval_t *dv);
dval_t *deriv_asin(dval_t *dv);
dval_t *deriv_acos(dval_t *dv);
dval_t *deriv_atan(dval_t *dv);
dval_t *deriv_asinh(dval_t *dv);
dval_t *deriv_acosh(dval_t *dv);
dval_t *deriv_atanh(dval_t *dv);
dval_t *deriv_exp(dval_t *dv);
dval_t *deriv_log(dval_t *dv);
dval_t *deriv_sqrt(dval_t *dv);
dval_t *deriv_abs(dval_t *dv);
dval_t *deriv_erf(dval_t *dv);
dval_t *deriv_erfc(dval_t *dv);
dval_t *deriv_lgamma(dval_t *dv);
dval_t *deriv_erfinv(dval_t *dv);
dval_t *deriv_erfcinv(dval_t *dv);
dval_t *deriv_gamma(dval_t *dv);
dval_t *deriv_digamma(dval_t *dv);
dval_t *deriv_trigamma(dval_t *dv);
dval_t *deriv_lambert_w0(dval_t *dv);
dval_t *deriv_lambert_wm1(dval_t *dv);
dval_t *deriv_normal_pdf(dval_t *dv);
dval_t *deriv_normal_cdf(dval_t *dv);
dval_t *deriv_normal_logpdf(dval_t *dv);
dval_t *deriv_ei(dval_t *dv);
dval_t *deriv_e1(dval_t *dv);
dval_t *deriv_hypot(dval_t *dv);
dval_t *deriv_beta(dval_t *dv);
dval_t *deriv_logbeta(dval_t *dv);
dval_t *deriv_atan2(dval_t *dv);

#endif
