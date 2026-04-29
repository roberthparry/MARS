#include <stddef.h>

#include "dval_math_internal.h"

static inline dval_t *dv_math_wrap_unary(const dval_ops_t *ops, dval_t *a)
{
    if (!a)
        return NULL;
    dv_retain(a);
    return dv_new_unary_internal(ops, a);
}

static inline dval_t *dv_math_wrap_binary(const dval_ops_t *ops, dval_t *a, dval_t *b)
{
    if (!a || !b)
        return NULL;
    dv_retain(a);
    dv_retain(b);
    return dv_new_binary_internal(ops, a, b);
}

const dval_ops_t ops_atan2 = {
    .eval = eval_atan2, .deriv = deriv_atan2, .reverse = dv_reverse_atan2,
    .kind = DV_KIND_ATAN2, .arity = DV_OP_BINARY, .name = "atan2",
    .apply_unary = NULL, .apply_binary = dv_atan2,
    .simplify = dv_simplify_binary_operator, .fold_const_unary = NULL
};

const dval_ops_t ops_sin = {
    .eval = eval_sin, .deriv = deriv_sin, .reverse = dv_reverse_sin,
    .kind = DV_KIND_SIN, .arity = DV_OP_UNARY, .name = "sin",
    .apply_unary = dv_sin, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = dv_fold_zero_to_zero
};
const dval_ops_t ops_cos = {
    .eval = eval_cos, .deriv = deriv_cos, .reverse = dv_reverse_cos,
    .kind = DV_KIND_COS, .arity = DV_OP_UNARY, .name = "cos",
    .apply_unary = dv_cos, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = dv_fold_cos_const
};
const dval_ops_t ops_tan = {
    .eval = eval_tan, .deriv = deriv_tan, .reverse = dv_reverse_tan,
    .kind = DV_KIND_TAN, .arity = DV_OP_UNARY, .name = "tan",
    .apply_unary = dv_tan, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = dv_fold_zero_to_zero
};
const dval_ops_t ops_sinh = {
    .eval = eval_sinh, .deriv = deriv_sinh, .reverse = dv_reverse_sinh,
    .kind = DV_KIND_SINH, .arity = DV_OP_UNARY, .name = "sinh",
    .apply_unary = dv_sinh, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_cosh = {
    .eval = eval_cosh, .deriv = deriv_cosh, .reverse = dv_reverse_cosh,
    .kind = DV_KIND_COSH, .arity = DV_OP_UNARY, .name = "cosh",
    .apply_unary = dv_cosh, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_tanh = {
    .eval = eval_tanh, .deriv = deriv_tanh, .reverse = dv_reverse_tanh,
    .kind = DV_KIND_TANH, .arity = DV_OP_UNARY, .name = "tanh",
    .apply_unary = dv_tanh, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_asin = {
    .eval = eval_asin, .deriv = deriv_asin, .reverse = dv_reverse_asin,
    .kind = DV_KIND_ASIN, .arity = DV_OP_UNARY, .name = "asin",
    .apply_unary = dv_asin, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_acos = {
    .eval = eval_acos, .deriv = deriv_acos, .reverse = dv_reverse_acos,
    .kind = DV_KIND_ACOS, .arity = DV_OP_UNARY, .name = "acos",
    .apply_unary = dv_acos, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_atan = {
    .eval = eval_atan, .deriv = deriv_atan, .reverse = dv_reverse_atan,
    .kind = DV_KIND_ATAN, .arity = DV_OP_UNARY, .name = "atan",
    .apply_unary = dv_atan, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_asinh = {
    .eval = eval_asinh, .deriv = deriv_asinh, .reverse = dv_reverse_asinh,
    .kind = DV_KIND_ASINH, .arity = DV_OP_UNARY, .name = "asinh",
    .apply_unary = dv_asinh, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_acosh = {
    .eval = eval_acosh, .deriv = deriv_acosh, .reverse = dv_reverse_acosh,
    .kind = DV_KIND_ACOSH, .arity = DV_OP_UNARY, .name = "acosh",
    .apply_unary = dv_acosh, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_atanh = {
    .eval = eval_atanh, .deriv = deriv_atanh, .reverse = dv_reverse_atanh,
    .kind = DV_KIND_ATANH, .arity = DV_OP_UNARY, .name = "atanh",
    .apply_unary = dv_atanh, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_exp = {
    .eval = eval_exp, .deriv = deriv_exp, .reverse = dv_reverse_exp,
    .kind = DV_KIND_EXP, .arity = DV_OP_UNARY, .name = "exp",
    .apply_unary = dv_exp, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = dv_fold_exp_const
};
const dval_ops_t ops_log = {
    .eval = eval_log, .deriv = deriv_log, .reverse = dv_reverse_log,
    .kind = DV_KIND_LOG, .arity = DV_OP_UNARY, .name = "log",
    .apply_unary = dv_log, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = dv_fold_log_const
};
const dval_ops_t ops_sqrt = {
    .eval = eval_sqrt, .deriv = deriv_sqrt, .reverse = dv_reverse_sqrt,
    .kind = DV_KIND_SQRT, .arity = DV_OP_UNARY, .name = "sqrt",
    .apply_unary = dv_sqrt, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = dv_fold_sqrt_const
};
const dval_ops_t ops_abs = {
    .eval = eval_abs, .deriv = deriv_abs, .reverse = dv_reverse_abs,
    .kind = DV_KIND_ABS, .arity = DV_OP_UNARY, .name = "abs",
    .apply_unary = dv_abs, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_erf = {
    .eval = eval_erf, .deriv = deriv_erf, .reverse = dv_reverse_erf,
    .kind = DV_KIND_ERF, .arity = DV_OP_UNARY, .name = "erf",
    .apply_unary = dv_erf, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_erfc = {
    .eval = eval_erfc, .deriv = deriv_erfc, .reverse = dv_reverse_erfc,
    .kind = DV_KIND_ERFC, .arity = DV_OP_UNARY, .name = "erfc",
    .apply_unary = dv_erfc, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_lgamma = {
    .eval = eval_lgamma, .deriv = deriv_lgamma, .reverse = dv_reverse_lgamma,
    .kind = DV_KIND_LGAMMA, .arity = DV_OP_UNARY, .name = "lgamma",
    .apply_unary = dv_lgamma, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_hypot = {
    .eval = eval_hypot, .deriv = deriv_hypot, .reverse = dv_reverse_hypot,
    .kind = DV_KIND_HYPOT, .arity = DV_OP_BINARY, .name = "hypot",
    .apply_unary = NULL, .apply_binary = dv_hypot,
    .simplify = dv_simplify_hypot_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_erfinv = {
    .eval = eval_erfinv, .deriv = deriv_erfinv, .reverse = dv_reverse_erfinv,
    .kind = DV_KIND_ERFINV, .arity = DV_OP_UNARY, .name = "erfinv",
    .apply_unary = dv_erfinv, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_erfcinv = {
    .eval = eval_erfcinv, .deriv = deriv_erfcinv, .reverse = dv_reverse_erfcinv,
    .kind = DV_KIND_ERFCINV, .arity = DV_OP_UNARY, .name = "erfcinv",
    .apply_unary = dv_erfcinv, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_gamma = {
    .eval = eval_gamma, .deriv = deriv_gamma, .reverse = dv_reverse_gamma,
    .kind = DV_KIND_GAMMA, .arity = DV_OP_UNARY, .name = "gamma",
    .apply_unary = dv_gamma, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_digamma = {
    .eval = eval_digamma, .deriv = deriv_digamma, .reverse = dv_reverse_digamma,
    .kind = DV_KIND_DIGAMMA, .arity = DV_OP_UNARY, .name = "digamma",
    .apply_unary = dv_digamma, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_trigamma = {
    .eval = eval_trigamma, .deriv = deriv_trigamma, .reverse = dv_reverse_trigamma,
    .kind = DV_KIND_TRIGAMMA, .arity = DV_OP_UNARY, .name = "trigamma",
    .apply_unary = dv_trigamma, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_lambert_w0 = {
    .eval = eval_lambert_w0, .deriv = deriv_lambert_w0, .reverse = dv_reverse_lambert_w0,
    .kind = DV_KIND_LAMBERT_W0, .arity = DV_OP_UNARY, .name = "lambert_w0",
    .apply_unary = dv_lambert_w0, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_lambert_wm1 = {
    .eval = eval_lambert_wm1, .deriv = deriv_lambert_wm1, .reverse = dv_reverse_lambert_wm1,
    .kind = DV_KIND_LAMBERT_WM1, .arity = DV_OP_UNARY, .name = "lambert_wm1",
    .apply_unary = dv_lambert_wm1, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_normal_pdf = {
    .eval = eval_normal_pdf, .deriv = deriv_normal_pdf, .reverse = dv_reverse_normal_pdf,
    .kind = DV_KIND_NORMAL_PDF, .arity = DV_OP_UNARY, .name = "normal_pdf",
    .apply_unary = dv_normal_pdf, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_normal_cdf = {
    .eval = eval_normal_cdf, .deriv = deriv_normal_cdf, .reverse = dv_reverse_normal_cdf,
    .kind = DV_KIND_NORMAL_CDF, .arity = DV_OP_UNARY, .name = "normal_cdf",
    .apply_unary = dv_normal_cdf, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_normal_logpdf = {
    .eval = eval_normal_logpdf, .deriv = deriv_normal_logpdf, .reverse = dv_reverse_normal_logpdf,
    .kind = DV_KIND_NORMAL_LOGPDF, .arity = DV_OP_UNARY, .name = "normal_logpdf",
    .apply_unary = dv_normal_logpdf, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_ei = {
    .eval = eval_ei, .deriv = deriv_ei, .reverse = dv_reverse_ei,
    .kind = DV_KIND_EI, .arity = DV_OP_UNARY, .name = "Ei",
    .apply_unary = dv_ei, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_e1 = {
    .eval = eval_e1, .deriv = deriv_e1, .reverse = dv_reverse_e1,
    .kind = DV_KIND_E1, .arity = DV_OP_UNARY, .name = "E1",
    .apply_unary = dv_e1, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_beta = {
    .eval = eval_beta, .deriv = deriv_beta, .reverse = dv_reverse_beta,
    .kind = DV_KIND_BETA, .arity = DV_OP_BINARY, .name = "beta",
    .apply_unary = NULL, .apply_binary = dv_beta,
    .simplify = dv_simplify_binary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_logbeta = {
    .eval = eval_logbeta, .deriv = deriv_logbeta, .reverse = dv_reverse_logbeta,
    .kind = DV_KIND_LOGBETA, .arity = DV_OP_BINARY, .name = "logbeta",
    .apply_unary = NULL, .apply_binary = dv_logbeta,
    .simplify = dv_simplify_binary_operator, .fold_const_unary = NULL
};

dval_t *dv_sqrt(dval_t *a) { return dv_math_wrap_unary(&ops_sqrt, a); }
dval_t *dv_exp(dval_t *a) { return dv_math_wrap_unary(&ops_exp, a); }
dval_t *dv_log(dval_t *a) { return dv_math_wrap_unary(&ops_log, a); }
dval_t *dv_sin(dval_t *a) { return dv_math_wrap_unary(&ops_sin, a); }
dval_t *dv_cos(dval_t *a) { return dv_math_wrap_unary(&ops_cos, a); }
dval_t *dv_tan(dval_t *a) { return dv_math_wrap_unary(&ops_tan, a); }
dval_t *dv_sinh(dval_t *a) { return dv_math_wrap_unary(&ops_sinh, a); }
dval_t *dv_cosh(dval_t *a) { return dv_math_wrap_unary(&ops_cosh, a); }
dval_t *dv_tanh(dval_t *a) { return dv_math_wrap_unary(&ops_tanh, a); }
dval_t *dv_asin(dval_t *a) { return dv_math_wrap_unary(&ops_asin, a); }
dval_t *dv_acos(dval_t *a) { return dv_math_wrap_unary(&ops_acos, a); }
dval_t *dv_atan(dval_t *a) { return dv_math_wrap_unary(&ops_atan, a); }
dval_t *dv_atan2(dval_t *a, dval_t *b) { return dv_math_wrap_binary(&ops_atan2, a, b); }
dval_t *dv_asinh(dval_t *a) { return dv_math_wrap_unary(&ops_asinh, a); }
dval_t *dv_acosh(dval_t *a) { return dv_math_wrap_unary(&ops_acosh, a); }
dval_t *dv_atanh(dval_t *a) { return dv_math_wrap_unary(&ops_atanh, a); }
dval_t *dv_abs(dval_t *a) { return dv_math_wrap_unary(&ops_abs, a); }
dval_t *dv_erf(dval_t *a) { return dv_math_wrap_unary(&ops_erf, a); }
dval_t *dv_erfc(dval_t *a) { return dv_math_wrap_unary(&ops_erfc, a); }
dval_t *dv_lgamma(dval_t *a) { return dv_math_wrap_unary(&ops_lgamma, a); }
dval_t *dv_hypot(dval_t *a, dval_t *b) { return dv_math_wrap_binary(&ops_hypot, a, b); }
dval_t *dv_erfinv(dval_t *a) { return dv_math_wrap_unary(&ops_erfinv, a); }
dval_t *dv_erfcinv(dval_t *a) { return dv_math_wrap_unary(&ops_erfcinv, a); }
dval_t *dv_gamma(dval_t *a) { return dv_math_wrap_unary(&ops_gamma, a); }
dval_t *dv_digamma(dval_t *a) { return dv_math_wrap_unary(&ops_digamma, a); }
dval_t *dv_trigamma(dval_t *a) { return dv_math_wrap_unary(&ops_trigamma, a); }
dval_t *dv_lambert_w0(dval_t *a) { return dv_math_wrap_unary(&ops_lambert_w0, a); }
dval_t *dv_lambert_wm1(dval_t *a) { return dv_math_wrap_unary(&ops_lambert_wm1, a); }
dval_t *dv_normal_pdf(dval_t *a) { return dv_math_wrap_unary(&ops_normal_pdf, a); }
dval_t *dv_normal_cdf(dval_t *a) { return dv_math_wrap_unary(&ops_normal_cdf, a); }
dval_t *dv_normal_logpdf(dval_t *a) { return dv_math_wrap_unary(&ops_normal_logpdf, a); }
dval_t *dv_ei(dval_t *a) { return dv_math_wrap_unary(&ops_ei, a); }
dval_t *dv_e1(dval_t *a) { return dv_math_wrap_unary(&ops_e1, a); }
dval_t *dv_beta(dval_t *a, dval_t *b) { return dv_math_wrap_binary(&ops_beta, a, b); }
dval_t *dv_logbeta(dval_t *a, dval_t *b) { return dv_math_wrap_binary(&ops_logbeta, a, b); }
