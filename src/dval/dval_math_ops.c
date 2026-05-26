#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "dval_math_internal.h"

static inline dval_t *dv_math_wrap_unary(const dval_ops_t *ops, const dval_t *a)
{
    if (!a)
        return NULL;
    dv_retain(a);
    return dv_new_unary_internal(ops, a);
}

static inline dval_t *dv_math_wrap_binary(const dval_ops_t *ops, const dval_t *a, const dval_t *b)
{
    if (!a || !b)
        return NULL;
    dv_retain(a);
    dv_retain(b);
    return dv_new_binary_internal(ops, a, b);
}

static dval_t *dv_inverse_log10_internal(const dval_t *a)
{
    dval_t *ten = dv_new_const(NUM_TEN);
    dval_t *out = dv_pow_dv(ten, a);

    dv_free(ten);
    return out;
}

static dval_t *dv_inverse_sqrt_internal(const dval_t *a)
{
    return dv_pow(a, &NUM_TWO);
}

static dval_t *dv_inverse_lambert_internal(const dval_t *a)
{
    dval_t *exp_a = dv_exp(a);
    dval_t *out = exp_a ? dv_mul(a, exp_a) : NULL;

    dv_free(exp_a);
    return out;
}

const dval_ops_t ops_atan2 = {
    .eval = eval_atan2, .deriv = deriv_atan2, .reverse = dv_reverse_atan2,
    .kind = DV_KIND_ATAN2, .arity = DV_OP_BINARY, .name = "atan2",
    .tex_name = "\\operatorname{atan2}",
    .apply_unary = NULL, .apply_binary = dv_atan2,
    .simplify = dv_simplify_binary_operator, .fold_const_unary = NULL
};

const dval_ops_t ops_sin = {
    .eval = eval_sin, .deriv = deriv_sin, .reverse = dv_reverse_sin,
    .kind = DV_KIND_SIN, .arity = DV_OP_UNARY, .name = "sin",
    .tex_name = "\\sin",
    .direct_inverse = &ops_asin,
    .inverse_unary = dv_asin,
    .apply_unary = dv_sin, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = dv_fold_zero_to_zero
};
const dval_ops_t ops_cos = {
    .eval = eval_cos, .deriv = deriv_cos, .reverse = dv_reverse_cos,
    .kind = DV_KIND_COS, .arity = DV_OP_UNARY, .name = "cos",
    .tex_name = "\\cos",
    .direct_inverse = &ops_acos,
    .inverse_unary = dv_acos,
    .apply_unary = dv_cos, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = dv_fold_cos_const
};
const dval_ops_t ops_tan = {
    .eval = eval_tan, .deriv = deriv_tan, .reverse = dv_reverse_tan,
    .kind = DV_KIND_TAN, .arity = DV_OP_UNARY, .name = "tan",
    .tex_name = "\\tan",
    .direct_inverse = &ops_atan,
    .inverse_unary = dv_atan,
    .apply_unary = dv_tan, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = dv_fold_zero_to_zero
};
const dval_ops_t ops_sinh = {
    .eval = eval_sinh, .deriv = deriv_sinh, .reverse = dv_reverse_sinh,
    .kind = DV_KIND_SINH, .arity = DV_OP_UNARY, .name = "sinh",
    .tex_name = "\\sinh",
    .direct_inverse = &ops_asinh,
    .inverse_unary = dv_asinh,
    .apply_unary = dv_sinh, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_cosh = {
    .eval = eval_cosh, .deriv = deriv_cosh, .reverse = dv_reverse_cosh,
    .kind = DV_KIND_COSH, .arity = DV_OP_UNARY, .name = "cosh",
    .tex_name = "\\cosh",
    .direct_inverse = &ops_acosh,
    .inverse_unary = dv_acosh,
    .apply_unary = dv_cosh, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_tanh = {
    .eval = eval_tanh, .deriv = deriv_tanh, .reverse = dv_reverse_tanh,
    .kind = DV_KIND_TANH, .arity = DV_OP_UNARY, .name = "tanh",
    .tex_name = "\\tanh",
    .direct_inverse = &ops_atanh,
    .inverse_unary = dv_atanh,
    .apply_unary = dv_tanh, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_asin = {
    .eval = eval_asin, .deriv = deriv_asin, .reverse = dv_reverse_asin,
    .kind = DV_KIND_ASIN, .arity = DV_OP_UNARY, .name = "asin",
    .tex_name = "\\arcsin",
    .inverse_unary = dv_sin,
    .apply_unary = dv_asin, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_acos = {
    .eval = eval_acos, .deriv = deriv_acos, .reverse = dv_reverse_acos,
    .kind = DV_KIND_ACOS, .arity = DV_OP_UNARY, .name = "acos",
    .tex_name = "\\arccos",
    .inverse_unary = dv_cos,
    .apply_unary = dv_acos, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_atan = {
    .eval = eval_atan, .deriv = deriv_atan, .reverse = dv_reverse_atan,
    .kind = DV_KIND_ATAN, .arity = DV_OP_UNARY, .name = "atan",
    .tex_name = "\\arctan",
    .inverse_unary = dv_tan,
    .apply_unary = dv_atan, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_asinh = {
    .eval = eval_asinh, .deriv = deriv_asinh, .reverse = dv_reverse_asinh,
    .kind = DV_KIND_ASINH, .arity = DV_OP_UNARY, .name = "asinh",
    .tex_name = "\\operatorname{asinh}",
    .inverse_unary = dv_sinh,
    .apply_unary = dv_asinh, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_acosh = {
    .eval = eval_acosh, .deriv = deriv_acosh, .reverse = dv_reverse_acosh,
    .kind = DV_KIND_ACOSH, .arity = DV_OP_UNARY, .name = "acosh",
    .tex_name = "\\operatorname{acosh}",
    .inverse_unary = dv_cosh,
    .apply_unary = dv_acosh, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_atanh = {
    .eval = eval_atanh, .deriv = deriv_atanh, .reverse = dv_reverse_atanh,
    .kind = DV_KIND_ATANH, .arity = DV_OP_UNARY, .name = "atanh",
    .tex_name = "\\operatorname{atanh}",
    .inverse_unary = dv_tanh,
    .apply_unary = dv_atanh, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_exp = {
    .eval = eval_exp, .deriv = deriv_exp, .reverse = dv_reverse_exp,
    .kind = DV_KIND_EXP, .arity = DV_OP_UNARY, .name = "exp",
    .tex_name = "\\exp",
    .direct_inverse = &ops_log,
    .inverse_unary = dv_log,
    .apply_unary = dv_exp, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = dv_fold_exp_const
};
const dval_ops_t ops_log = {
    .eval = eval_log, .deriv = deriv_log, .reverse = dv_reverse_log,
    .kind = DV_KIND_LOG, .arity = DV_OP_UNARY, .name = "ln",
    .tex_name = "\\ln",
    .direct_inverse = &ops_exp,
    .inverse_unary = dv_exp,
    .apply_unary = dv_log, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = dv_fold_log_const
};
const dval_ops_t ops_log10 = {
    .eval = eval_log10, .deriv = deriv_log10, .reverse = dv_reverse_log10,
    .kind = DV_KIND_LOG10, .arity = DV_OP_UNARY, .name = "log",
    .tex_name = "\\log",
    .inverse_unary = dv_inverse_log10_internal,
    .apply_unary = dv_log10, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_sqrt = {
    .eval = eval_sqrt, .deriv = deriv_sqrt, .reverse = dv_reverse_sqrt,
    .kind = DV_KIND_SQRT, .arity = DV_OP_UNARY, .name = "sqrt",
    .tex_name = "\\sqrt",
    .inverse_unary = dv_inverse_sqrt_internal,
    .apply_unary = dv_sqrt, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = dv_fold_sqrt_const
};
const dval_ops_t ops_floor = {
    .eval = eval_floor, .deriv = deriv_floor, .reverse = dv_reverse_floor,
    .kind = DV_KIND_FLOOR, .arity = DV_OP_UNARY, .name = "floor",
    .tex_name = "\\lfloor",
    .apply_unary = dv_floor, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = dv_fold_floor_const
};
const dval_ops_t ops_ceil = {
    .eval = eval_ceil, .deriv = deriv_ceil, .reverse = dv_reverse_ceil,
    .kind = DV_KIND_CEIL, .arity = DV_OP_UNARY, .name = "ceil",
    .tex_name = "\\lceil",
    .apply_unary = dv_ceil, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_abs = {
    .eval = eval_abs, .deriv = deriv_abs, .reverse = dv_reverse_abs,
    .kind = DV_KIND_ABS, .arity = DV_OP_UNARY, .name = "abs",
    .tex_name = NULL,
    .apply_unary = dv_abs, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_erf = {
    .eval = eval_erf, .deriv = deriv_erf, .reverse = dv_reverse_erf,
    .kind = DV_KIND_ERF, .arity = DV_OP_UNARY, .name = "erf",
    .tex_name = "\\operatorname{erf}",
    .direct_inverse = &ops_erfinv,
    .inverse_unary = dv_erfinv,
    .apply_unary = dv_erf, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_erfc = {
    .eval = eval_erfc, .deriv = deriv_erfc, .reverse = dv_reverse_erfc,
    .kind = DV_KIND_ERFC, .arity = DV_OP_UNARY, .name = "erfc",
    .tex_name = "\\operatorname{erfc}",
    .direct_inverse = &ops_erfcinv,
    .inverse_unary = dv_erfcinv,
    .apply_unary = dv_erfc, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_lgamma = {
    .eval = eval_lgamma, .deriv = deriv_lgamma, .reverse = dv_reverse_lgamma,
    .kind = DV_KIND_LGAMMA, .arity = DV_OP_UNARY, .name = "lgamma",
    .tex_name = "\\log\\Gamma",
    .apply_unary = dv_lgamma, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_hypot = {
    .eval = eval_hypot, .deriv = deriv_hypot, .reverse = dv_reverse_hypot,
    .kind = DV_KIND_HYPOT, .arity = DV_OP_BINARY, .name = "hypot",
    .tex_name = "\\operatorname{hypot}",
    .apply_unary = NULL, .apply_binary = dv_hypot,
    .simplify = dv_simplify_hypot_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_erfinv = {
    .eval = eval_erfinv, .deriv = deriv_erfinv, .reverse = dv_reverse_erfinv,
    .kind = DV_KIND_ERFINV, .arity = DV_OP_UNARY, .name = "erfinv",
    .tex_name = "\\operatorname{erf}^{-1}",
    .inverse_unary = dv_erf,
    .apply_unary = dv_erfinv, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_erfcinv = {
    .eval = eval_erfcinv, .deriv = deriv_erfcinv, .reverse = dv_reverse_erfcinv,
    .kind = DV_KIND_ERFCINV, .arity = DV_OP_UNARY, .name = "erfcinv",
    .tex_name = "\\operatorname{erfc}^{-1}",
    .inverse_unary = dv_erfc,
    .apply_unary = dv_erfcinv, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_gamma = {
    .eval = eval_gamma, .deriv = deriv_gamma, .reverse = dv_reverse_gamma,
    .kind = DV_KIND_GAMMA, .arity = DV_OP_UNARY, .name = "gamma",
    .tex_name = "\\Gamma",
    .direct_inverse = &ops_gammainv,
    .inverse_unary = dv_gammainv,
    .apply_unary = dv_gamma, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_digamma = {
    .eval = eval_digamma, .deriv = deriv_digamma, .reverse = dv_reverse_digamma,
    .kind = DV_KIND_DIGAMMA, .arity = DV_OP_UNARY, .name = "digamma",
    .tex_name = "\\psi^{(0)}",
    .apply_unary = dv_digamma, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_trigamma = {
    .eval = eval_trigamma, .deriv = deriv_trigamma, .reverse = dv_reverse_trigamma,
    .kind = DV_KIND_TRIGAMMA, .arity = DV_OP_UNARY, .name = "trigamma",
    .tex_name = "\\psi^{(1)}",
    .apply_unary = dv_trigamma, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_polygamma = {
    .eval = eval_polygamma, .deriv = deriv_polygamma, .reverse = dv_reverse_polygamma,
    .kind = DV_KIND_POLYGAMMA, .arity = DV_OP_BINARY, .name = "polygamma",
    .tex_name = "\\psi",
    .apply_unary = NULL, .apply_binary = dv_polygamma_dv,
    .simplify = dv_simplify_binary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_gammainv = {
    .eval = eval_gammainv, .deriv = deriv_gammainv, .reverse = dv_reverse_gammainv,
    .kind = DV_KIND_GAMMAINV, .arity = DV_OP_UNARY, .name = "gammainv",
    .tex_name = "\\Gamma^{-1}",
    .inverse_unary = dv_gamma,
    .apply_unary = dv_gammainv, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_lambert_w = {
    .eval = eval_lambert_w, .deriv = deriv_lambert_w, .reverse = dv_reverse_lambert_w,
    .kind = DV_KIND_LAMBERT_W, .arity = DV_OP_UNARY, .name = "W",
    .tex_name = "W",
    .inverse_unary = dv_inverse_lambert_internal,
    .apply_unary = dv_lambert_w, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_lambert_w0 = {
    .eval = eval_lambert_w0, .deriv = deriv_lambert_w0, .reverse = dv_reverse_lambert_w0,
    .kind = DV_KIND_LAMBERT_W0, .arity = DV_OP_UNARY, .name = "W₀",
    .tex_name = "W_{0}",
    .inverse_unary = dv_inverse_lambert_internal,
    .apply_unary = dv_lambert_w0, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_lambert_wm1 = {
    .eval = eval_lambert_wm1, .deriv = deriv_lambert_wm1, .reverse = dv_reverse_lambert_wm1,
    .kind = DV_KIND_LAMBERT_WM1, .arity = DV_OP_UNARY, .name = "W₋₁",
    .tex_name = "W_{-1}",
    .inverse_unary = dv_inverse_lambert_internal,
    .apply_unary = dv_lambert_wm1, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_normal_pdf = {
    .eval = eval_normal_pdf, .deriv = deriv_normal_pdf, .reverse = dv_reverse_normal_pdf,
    .kind = DV_KIND_NORMAL_PDF, .arity = DV_OP_UNARY, .name = "normal_pdf",
    .tex_name = "\\operatorname{normal\\_pdf}",
    .apply_unary = dv_normal_pdf, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_normal_cdf = {
    .eval = eval_normal_cdf, .deriv = deriv_normal_cdf, .reverse = dv_reverse_normal_cdf,
    .kind = DV_KIND_NORMAL_CDF, .arity = DV_OP_UNARY, .name = "normal_cdf",
    .tex_name = "\\operatorname{normal\\_cdf}",
    .apply_unary = dv_normal_cdf, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_normal_logpdf = {
    .eval = eval_normal_logpdf, .deriv = deriv_normal_logpdf, .reverse = dv_reverse_normal_logpdf,
    .kind = DV_KIND_NORMAL_LOGPDF, .arity = DV_OP_UNARY, .name = "normal_logpdf",
    .tex_name = "\\operatorname{normal\\_logpdf}",
    .apply_unary = dv_normal_logpdf, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_ei = {
    .eval = eval_ei, .deriv = deriv_ei, .reverse = dv_reverse_ei,
    .kind = DV_KIND_EI, .arity = DV_OP_UNARY, .name = "Ei",
    .tex_name = "\\operatorname{Ei}",
    .apply_unary = dv_ei, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_e1 = {
    .eval = eval_e1, .deriv = deriv_e1, .reverse = dv_reverse_e1,
    .kind = DV_KIND_E1, .arity = DV_OP_UNARY, .name = "E1",
    .tex_name = "\\operatorname{E1}",
    .apply_unary = dv_e1, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_beta = {
    .eval = eval_beta, .deriv = deriv_beta, .reverse = dv_reverse_beta,
    .kind = DV_KIND_BETA, .arity = DV_OP_BINARY, .name = "beta",
    .tex_name = "\\operatorname{beta}",
    .apply_unary = NULL, .apply_binary = dv_beta,
    .simplify = dv_simplify_binary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_logbeta = {
    .eval = eval_logbeta, .deriv = deriv_logbeta, .reverse = dv_reverse_logbeta,
    .kind = DV_KIND_LOGBETA, .arity = DV_OP_BINARY, .name = "logbeta",
    .tex_name = "\\operatorname{logbeta}",
    .apply_unary = NULL, .apply_binary = dv_logbeta,
    .simplify = dv_simplify_binary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_gammainc_lower = {
    .eval = eval_gammainc_lower, .deriv = deriv_gammainc_lower, .reverse = dv_reverse_gammainc_lower,
    .kind = DV_KIND_GAMMAINC_LOWER, .arity = DV_OP_BINARY, .name = "gammainc_lower",
    .tex_name = "\\gamma",
    .apply_unary = NULL, .apply_binary = dv_gammainc_lower,
    .simplify = dv_simplify_binary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_gammainc_upper = {
    .eval = eval_gammainc_upper, .deriv = deriv_gammainc_upper, .reverse = dv_reverse_gammainc_upper,
    .kind = DV_KIND_GAMMAINC_UPPER, .arity = DV_OP_BINARY, .name = "gammainc_upper",
    .tex_name = "\\Gamma",
    .apply_unary = NULL, .apply_binary = dv_gammainc_upper,
    .simplify = dv_simplify_binary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_gammainc_P = {
    .eval = eval_gammainc_P, .deriv = deriv_gammainc_P, .reverse = dv_reverse_gammainc_P,
    .kind = DV_KIND_GAMMAINC_P, .arity = DV_OP_BINARY, .name = "gammainc_P",
    .tex_name = "\\operatorname{P}",
    .apply_unary = NULL, .apply_binary = dv_gammainc_P,
    .simplify = dv_simplify_binary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_gammainc_Q = {
    .eval = eval_gammainc_Q, .deriv = deriv_gammainc_Q, .reverse = dv_reverse_gammainc_Q,
    .kind = DV_KIND_GAMMAINC_Q, .arity = DV_OP_BINARY, .name = "gammainc_Q",
    .tex_name = "\\operatorname{Q}",
    .apply_unary = NULL, .apply_binary = dv_gammainc_Q,
    .simplify = dv_simplify_binary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_factorial = {
    .eval = eval_factorial, .deriv = deriv_not_differentiable,
    .reverse = dv_reverse_not_differentiable,
    .kind = DV_KIND_FACTORIAL, .arity = DV_OP_UNARY, .diff_kind = DV_DIFF_NONE,
    .name = "factorial", .tex_name = "\\operatorname{factorial}",
    .apply_unary = dv_factorial, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_fibonacci = {
    .eval = eval_fibonacci, .deriv = deriv_not_differentiable,
    .reverse = dv_reverse_not_differentiable,
    .kind = DV_KIND_FIBONACCI, .arity = DV_OP_UNARY, .diff_kind = DV_DIFF_NONE,
    .name = "fibonacci", .tex_name = "\\operatorname{fibonacci}",
    .apply_unary = dv_fibonacci, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_partition = {
    .eval = eval_partition, .deriv = deriv_not_differentiable,
    .reverse = dv_reverse_not_differentiable,
    .kind = DV_KIND_PARTITION, .arity = DV_OP_UNARY, .diff_kind = DV_DIFF_NONE,
    .name = "partition", .tex_name = "\\operatorname{partition}",
    .apply_unary = dv_partition, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_isqrt = {
    .eval = eval_isqrt, .deriv = deriv_not_differentiable,
    .reverse = dv_reverse_not_differentiable,
    .kind = DV_KIND_ISQRT, .arity = DV_OP_UNARY, .diff_kind = DV_DIFF_NONE,
    .name = "isqrt", .tex_name = "\\operatorname{isqrt}",
    .apply_unary = dv_isqrt, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_gcd = {
    .eval = eval_gcd, .deriv = deriv_not_differentiable,
    .reverse = dv_reverse_not_differentiable,
    .kind = DV_KIND_GCD, .arity = DV_OP_BINARY, .diff_kind = DV_DIFF_NONE,
    .name = "gcd", .tex_name = "\\gcd",
    .apply_unary = NULL, .apply_binary = dv_gcd,
    .simplify = dv_simplify_binary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_lcm = {
    .eval = eval_lcm, .deriv = deriv_not_differentiable,
    .reverse = dv_reverse_not_differentiable,
    .kind = DV_KIND_LCM, .arity = DV_OP_BINARY, .diff_kind = DV_DIFF_NONE,
    .name = "lcm", .tex_name = "\\operatorname{lcm}",
    .apply_unary = NULL, .apply_binary = dv_lcm,
    .simplify = dv_simplify_binary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_mod = {
    .eval = eval_mod, .deriv = deriv_not_differentiable,
    .reverse = dv_reverse_not_differentiable,
    .kind = DV_KIND_MOD, .arity = DV_OP_BINARY, .diff_kind = DV_DIFF_NONE,
    .name = "mod", .tex_name = "\\operatorname{mod}",
    .apply_unary = NULL, .apply_binary = dv_mod,
    .simplify = dv_simplify_binary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_modinv = {
    .eval = eval_modinv, .deriv = deriv_not_differentiable,
    .reverse = dv_reverse_not_differentiable,
    .kind = DV_KIND_MODINV, .arity = DV_OP_BINARY, .diff_kind = DV_DIFF_NONE,
    .name = "modinv", .tex_name = "\\operatorname{modinv}",
    .apply_unary = NULL, .apply_binary = dv_modinv,
    .simplify = dv_simplify_binary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_is_prime = {
    .eval = eval_is_prime, .deriv = deriv_not_differentiable,
    .reverse = dv_reverse_not_differentiable,
    .kind = DV_KIND_IS_PRIME, .arity = DV_OP_UNARY, .diff_kind = DV_DIFF_NONE,
    .name = "is_prime", .tex_name = "\\operatorname{is\\_prime}",
    .apply_unary = dv_is_prime, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_next_prime = {
    .eval = eval_next_prime, .deriv = deriv_not_differentiable,
    .reverse = dv_reverse_not_differentiable,
    .kind = DV_KIND_NEXT_PRIME, .arity = DV_OP_UNARY, .diff_kind = DV_DIFF_NONE,
    .name = "next_prime", .tex_name = "\\operatorname{next\\_prime}",
    .apply_unary = dv_next_prime, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_prev_prime = {
    .eval = eval_prev_prime, .deriv = deriv_not_differentiable,
    .reverse = dv_reverse_not_differentiable,
    .kind = DV_KIND_PREV_PRIME, .arity = DV_OP_UNARY, .diff_kind = DV_DIFF_NONE,
    .name = "prev_prime", .tex_name = "\\operatorname{prev\\_prime}",
    .apply_unary = dv_prev_prime, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_bit_and = {
    .eval = eval_bit_and, .deriv = deriv_not_differentiable,
    .reverse = dv_reverse_not_differentiable,
    .kind = DV_KIND_BIT_AND, .arity = DV_OP_BINARY, .diff_kind = DV_DIFF_NONE,
    .name = "AND", .tex_name = "\\operatorname{AND}",
    .apply_unary = NULL, .apply_binary = dv_bit_and,
    .simplify = dv_simplify_binary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_bit_or = {
    .eval = eval_bit_or, .deriv = deriv_not_differentiable,
    .reverse = dv_reverse_not_differentiable,
    .kind = DV_KIND_BIT_OR, .arity = DV_OP_BINARY, .diff_kind = DV_DIFF_NONE,
    .name = "OR", .tex_name = "\\operatorname{OR}",
    .apply_unary = NULL, .apply_binary = dv_bit_or,
    .simplify = dv_simplify_binary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_bit_xor = {
    .eval = eval_bit_xor, .deriv = deriv_not_differentiable,
    .reverse = dv_reverse_not_differentiable,
    .kind = DV_KIND_BIT_XOR, .arity = DV_OP_BINARY, .diff_kind = DV_DIFF_NONE,
    .name = "XOR", .tex_name = "\\operatorname{XOR}",
    .apply_unary = NULL, .apply_binary = dv_bit_xor,
    .simplify = dv_simplify_binary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_bit_not = {
    .eval = eval_bit_not, .deriv = deriv_not_differentiable,
    .reverse = dv_reverse_not_differentiable,
    .kind = DV_KIND_BIT_NOT, .arity = DV_OP_UNARY, .diff_kind = DV_DIFF_NONE,
    .name = "NOT", .tex_name = "\\operatorname{NOT}",
    .apply_unary = dv_bit_not, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_shl = {
    .eval = eval_shl, .deriv = deriv_not_differentiable,
    .reverse = dv_reverse_not_differentiable,
    .kind = DV_KIND_SHL, .arity = DV_OP_BINARY, .diff_kind = DV_DIFF_NONE,
    .name = "SHL", .tex_name = "\\operatorname{SHL}",
    .apply_unary = NULL, .apply_binary = dv_shl,
    .simplify = dv_simplify_binary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_shr = {
    .eval = eval_shr, .deriv = deriv_not_differentiable,
    .reverse = dv_reverse_not_differentiable,
    .kind = DV_KIND_SHR, .arity = DV_OP_BINARY, .diff_kind = DV_DIFF_NONE,
    .name = "SHR", .tex_name = "\\operatorname{SHR}",
    .apply_unary = NULL, .apply_binary = dv_shr,
    .simplify = dv_simplify_binary_operator, .fold_const_unary = NULL
};
const dval_ops_t ops_factors = {
    .eval = eval_factors, .deriv = deriv_not_differentiable,
    .reverse = dv_reverse_not_differentiable,
    .kind = DV_KIND_FACTORS, .arity = DV_OP_UNARY, .diff_kind = DV_DIFF_NONE,
    .name = "factors", .tex_name = "\\operatorname{factors}",
    .apply_unary = dv_factors, .apply_binary = NULL,
    .simplify = dv_simplify_unary_operator, .fold_const_unary = NULL
};

dval_t *dv_sqrt(const dval_t *a) { return dv_math_wrap_unary(&ops_sqrt, a); }
dval_t *dv_exp(const dval_t *a) { return dv_math_wrap_unary(&ops_exp, a); }
dval_t *dv_log(const dval_t *a) { return dv_math_wrap_unary(&ops_log, a); }
dval_t *dv_log10(const dval_t *a) { return dv_math_wrap_unary(&ops_log10, a); }
dval_t *dv_floor(const dval_t *a) { return dv_math_wrap_unary(&ops_floor, a); }
dval_t *dv_ceil(const dval_t *a) { return dv_math_wrap_unary(&ops_ceil, a); }
dval_t *dv_sin(const dval_t *a) { return dv_math_wrap_unary(&ops_sin, a); }
dval_t *dv_cos(const dval_t *a) { return dv_math_wrap_unary(&ops_cos, a); }
dval_t *dv_tan(const dval_t *a) { return dv_math_wrap_unary(&ops_tan, a); }
dval_t *dv_sinh(const dval_t *a) { return dv_math_wrap_unary(&ops_sinh, a); }
dval_t *dv_cosh(const dval_t *a) { return dv_math_wrap_unary(&ops_cosh, a); }
dval_t *dv_tanh(const dval_t *a) { return dv_math_wrap_unary(&ops_tanh, a); }
dval_t *dv_asin(const dval_t *a) { return dv_math_wrap_unary(&ops_asin, a); }
dval_t *dv_acos(const dval_t *a) { return dv_math_wrap_unary(&ops_acos, a); }
dval_t *dv_atan(const dval_t *a) { return dv_math_wrap_unary(&ops_atan, a); }
dval_t *dv_atan2(const dval_t *a, const dval_t *b) { return dv_math_wrap_binary(&ops_atan2, a, b); }
dval_t *dv_asinh(const dval_t *a) { return dv_math_wrap_unary(&ops_asinh, a); }
dval_t *dv_acosh(const dval_t *a) { return dv_math_wrap_unary(&ops_acosh, a); }
dval_t *dv_atanh(const dval_t *a) { return dv_math_wrap_unary(&ops_atanh, a); }
dval_t *dv_abs(const dval_t *a) { return dv_math_wrap_unary(&ops_abs, a); }
dval_t *dv_erf(const dval_t *a) { return dv_math_wrap_unary(&ops_erf, a); }
dval_t *dv_erfc(const dval_t *a) { return dv_math_wrap_unary(&ops_erfc, a); }
dval_t *dv_lgamma(const dval_t *a) { return dv_math_wrap_unary(&ops_lgamma, a); }
dval_t *dv_hypot(const dval_t *a, const dval_t *b) { return dv_math_wrap_binary(&ops_hypot, a, b); }
dval_t *dv_erfinv(const dval_t *a) { return dv_math_wrap_unary(&ops_erfinv, a); }
dval_t *dv_erfcinv(const dval_t *a) { return dv_math_wrap_unary(&ops_erfcinv, a); }
dval_t *dv_gamma(const dval_t *a) { return dv_math_wrap_unary(&ops_gamma, a); }
dval_t *dv_digamma(const dval_t *a) { return dv_math_wrap_unary(&ops_digamma, a); }
dval_t *dv_trigamma(const dval_t *a) { return dv_math_wrap_unary(&ops_trigamma, a); }
dval_t *dv_polygamma_dv(const dval_t *order, const dval_t *arg) { return dv_math_wrap_binary(&ops_polygamma, order, arg); }
dval_t *dv_polygamma(unsigned int order, const dval_t *a)
{
    NUM_SCOPE(scope);
    number_t order_value = num_create_from_long((long)order);
    dval_t *order_dv = dv_new_const(order_value);
    dval_t *out = dv_polygamma_dv(order_dv, a);

    dv_free(order_dv);
    return out;
}
dval_t *dv_gammainv(const dval_t *a) { return dv_math_wrap_unary(&ops_gammainv, a); }
dval_t *dv_lambert_w(const dval_t *a) { return dv_math_wrap_unary(&ops_lambert_w, a); }
dval_t *dv_lambert_w0(const dval_t *a) { return dv_math_wrap_unary(&ops_lambert_w0, a); }
dval_t *dv_lambert_wm1(const dval_t *a) { return dv_math_wrap_unary(&ops_lambert_wm1, a); }
dval_t *dv_normal_pdf(const dval_t *a) { return dv_math_wrap_unary(&ops_normal_pdf, a); }
dval_t *dv_normal_cdf(const dval_t *a) { return dv_math_wrap_unary(&ops_normal_cdf, a); }
dval_t *dv_normal_logpdf(const dval_t *a) { return dv_math_wrap_unary(&ops_normal_logpdf, a); }
dval_t *dv_ei(const dval_t *a) { return dv_math_wrap_unary(&ops_ei, a); }
dval_t *dv_e1(const dval_t *a) { return dv_math_wrap_unary(&ops_e1, a); }
dval_t *dv_beta(const dval_t *a, const dval_t *b) { return dv_math_wrap_binary(&ops_beta, a, b); }
dval_t *dv_logbeta(const dval_t *a, const dval_t *b) { return dv_math_wrap_binary(&ops_logbeta, a, b); }
dval_t *dv_gammainc_lower(const dval_t *a, const dval_t *b) { return dv_math_wrap_binary(&ops_gammainc_lower, a, b); }
dval_t *dv_gammainc_upper(const dval_t *a, const dval_t *b) { return dv_math_wrap_binary(&ops_gammainc_upper, a, b); }
dval_t *dv_gammainc_P(const dval_t *a, const dval_t *b) { return dv_math_wrap_binary(&ops_gammainc_P, a, b); }
dval_t *dv_gammainc_Q(const dval_t *a, const dval_t *b) { return dv_math_wrap_binary(&ops_gammainc_Q, a, b); }
dval_t *dv_factorial(const dval_t *a) { return dv_math_wrap_unary(&ops_factorial, a); }
dval_t *dv_fibonacci(const dval_t *a) { return dv_math_wrap_unary(&ops_fibonacci, a); }
dval_t *dv_partition(const dval_t *a) { return dv_math_wrap_unary(&ops_partition, a); }
dval_t *dv_isqrt(const dval_t *a) { return dv_math_wrap_unary(&ops_isqrt, a); }
dval_t *dv_gcd(const dval_t *a, const dval_t *b) { return dv_math_wrap_binary(&ops_gcd, a, b); }
dval_t *dv_lcm(const dval_t *a, const dval_t *b) { return dv_math_wrap_binary(&ops_lcm, a, b); }
dval_t *dv_mod(const dval_t *a, const dval_t *b) { return dv_math_wrap_binary(&ops_mod, a, b); }
dval_t *dv_modinv(const dval_t *a, const dval_t *b) { return dv_math_wrap_binary(&ops_modinv, a, b); }
dval_t *dv_is_prime(const dval_t *a) { return dv_math_wrap_unary(&ops_is_prime, a); }
dval_t *dv_next_prime(const dval_t *a) { return dv_math_wrap_unary(&ops_next_prime, a); }
dval_t *dv_prev_prime(const dval_t *a) { return dv_math_wrap_unary(&ops_prev_prime, a); }
dval_t *dv_bit_and(const dval_t *a, const dval_t *b) { return dv_math_wrap_binary(&ops_bit_and, a, b); }
dval_t *dv_bit_or(const dval_t *a, const dval_t *b) { return dv_math_wrap_binary(&ops_bit_or, a, b); }
dval_t *dv_bit_xor(const dval_t *a, const dval_t *b) { return dv_math_wrap_binary(&ops_bit_xor, a, b); }
dval_t *dv_bit_not(const dval_t *a) { return dv_math_wrap_unary(&ops_bit_not, a); }
dval_t *dv_shl(const dval_t *a, const dval_t *b) { return dv_math_wrap_binary(&ops_shl, a, b); }
dval_t *dv_shr(const dval_t *a, const dval_t *b) { return dv_math_wrap_binary(&ops_shr, a, b); }

static dval_t *dv_factor_product_from_number(number_t value)
{
    enum { FACTOR_MAX_BITS = 1024 };
    number_factors_t *factors;
    dval_t *out = NULL;

    if (!num_is_integer(value) || num_get_sign(value) <= 0 ||
        num_bit_length(value) > FACTOR_MAX_BITS)
        return NULL;

    factors = num_factors(value);
    if (!factors)
        return NULL;

    if (factors->count == 0u) {
        num_factors_free(factors);
        return dv_new_const(NUM_ONE);
    }

    for (size_t i = 0u; i < factors->count; ++i) {
        char name[32];
        dval_t *base;
        dval_t *factor;

        snprintf(name, sizeof(name), "a%zu", i);
        base = dv_new_named_const(factors->items[i].prime, name);
        {
            char *prime_text = num_to_string(factors->items[i].prime);

            base->binding_expr =
                dv_binding_expr_new_number_text(prime_text ? prime_text : "NAN");
            free(prime_text);
        }
        if (factors->items[i].exponent > 1u) {
            number_t exponent = num_create_from_long((long)factors->items[i].exponent);

            factor = dv_pow(base, &exponent);
            num_destroy(&exponent);
            dv_free(base);
        } else {
            factor = base;
        }

        if (out) {
            dval_t *next = dv_mul(out, factor);

            dv_free(out);
            dv_free(factor);
            out = next;
        } else {
            out = factor;
        }
    }

    num_factors_free(factors);
    return out;
}

dval_t *dv_factors(const dval_t *a)
{
    dval_t *product;

    if (!a)
        return NULL;

    product = dv_factor_product_from_number(dv_eval_num_internal(a));
    if (product)
        return product;

    return dv_math_wrap_unary(&ops_factors, a);
}

dval_t *dv_logbeta_pdf(const dval_t *x, const dval_t *a, const dval_t *b)
{
    dval_t *one;
    dval_t *a_minus_one;
    dval_t *b_minus_one;
    dval_t *log_x;
    dval_t *one_minus_x;
    dval_t *log_one_minus_x;
    dval_t *left;
    dval_t *right;
    dval_t *sum;
    dval_t *log_beta;
    dval_t *out;

    if (!x || !a || !b)
        return NULL;

    one = dv_new_const(NUM_ONE);
    a_minus_one = dv_sub(a, one);
    b_minus_one = dv_sub(b, one);
    log_x = dv_log(x);
    one_minus_x = dv_sub(one, x);
    log_one_minus_x = dv_log(one_minus_x);
    left = dv_mul(a_minus_one, log_x);
    right = dv_mul(b_minus_one, log_one_minus_x);
    sum = dv_add(left, right);
    log_beta = dv_logbeta(a, b);
    out = dv_sub(sum, log_beta);

    dv_free(log_beta);
    dv_free(sum);
    dv_free(right);
    dv_free(left);
    dv_free(log_one_minus_x);
    dv_free(one_minus_x);
    dv_free(log_x);
    dv_free(b_minus_one);
    dv_free(a_minus_one);
    dv_free(one);
    return out;
}

dval_t *dv_beta_pdf(const dval_t *x, const dval_t *a, const dval_t *b)
{
    dval_t *log_pdf = dv_logbeta_pdf(x, a, b);
    dval_t *out = log_pdf ? dv_exp(log_pdf) : NULL;

    dv_free(log_pdf);
    return out;
}

dval_t *dv_binomial(const dval_t *n, const dval_t *k)
{
    dval_t *one;
    dval_t *n_plus_one;
    dval_t *k_plus_one;
    dval_t *n_minus_k;
    dval_t *n_minus_k_plus_one;
    dval_t *gamma_n;
    dval_t *gamma_k;
    dval_t *gamma_n_minus_k;
    dval_t *denominator;
    dval_t *out;

    if (!n || !k)
        return NULL;

    one = dv_new_const(NUM_ONE);
    n_plus_one = dv_add(n, one);
    k_plus_one = dv_add(k, one);
    n_minus_k = dv_sub(n, k);
    n_minus_k_plus_one = dv_add(n_minus_k, one);
    gamma_n = dv_gamma(n_plus_one);
    gamma_k = dv_gamma(k_plus_one);
    gamma_n_minus_k = dv_gamma(n_minus_k_plus_one);
    denominator = dv_mul(gamma_k, gamma_n_minus_k);
    out = dv_div(gamma_n, denominator);

    dv_free(denominator);
    dv_free(gamma_n_minus_k);
    dv_free(gamma_k);
    dv_free(gamma_n);
    dv_free(n_minus_k_plus_one);
    dv_free(n_minus_k);
    dv_free(k_plus_one);
    dv_free(n_plus_one);
    dv_free(one);
    return out;
}
