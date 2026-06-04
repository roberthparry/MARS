#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "expr_maths.h"

static inline expr_t *expr_math_wrap_unary(const expr_ops_t *ops, const expr_t *a)
{
    if (!a)
        return NULL;
    expr_retain(a);
    return expr_new_unary_internal(ops, a);
}

static inline expr_t *expr_math_wrap_binary(const expr_ops_t *ops, const expr_t *a, const expr_t *b)
{
    if (!a || !b)
        return NULL;
    expr_retain(a);
    expr_retain(b);
    return expr_new_binary_internal(ops, a, b);
}

static expr_t *expr_inverse_log10_internal(const expr_t *a)
{
    expr_t *ten = expr_new_const(NUM_TEN);
    expr_t *out = expr_pow_xp(ten, a);

    expr_free(ten);
    return out;
}

static expr_t *expr_inverse_sqrt_internal(const expr_t *a)
{
    return expr_pow(a, &NUM_TWO);
}

static expr_t *expr_inverse_lambert_internal(const expr_t *a)
{
    expr_t *exp_a = expr_exp(a);
    expr_t *out = exp_a ? expr_mul(a, exp_a) : NULL;

    expr_free(exp_a);
    return out;
}

const expr_ops_t ops_atan2 = {
    .eval = eval_atan2, .deriv = deriv_atan2, .reverse = expr_reverse_atan2,
    .kind = EXPR_KIND_ATAN2, .arity = EXPR_OP_BINARY, .name = "atan2",
    .tex_name = "\\operatorname{atan2}",
    .apply_unary = NULL, .apply_binary = expr_atan2,
    .simplify = expr_simplify_binary_operator, .fold_const_unary = NULL
};

const expr_ops_t ops_sin = {
    .eval = eval_sin, .deriv = deriv_sin, .reverse = expr_reverse_sin,
    .kind = EXPR_KIND_SIN, .arity = EXPR_OP_UNARY, .name = "sin",
    .tex_name = "\\sin",
    .direct_inverse = &ops_asin,
    .inverse_unary = expr_asin,
    .apply_unary = expr_sin, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = expr_fold_zero_to_zero
};
const expr_ops_t ops_cos = {
    .eval = eval_cos, .deriv = deriv_cos, .reverse = expr_reverse_cos,
    .kind = EXPR_KIND_COS, .arity = EXPR_OP_UNARY, .name = "cos",
    .tex_name = "\\cos",
    .direct_inverse = &ops_acos,
    .inverse_unary = expr_acos,
    .apply_unary = expr_cos, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = expr_fold_cos_const
};
const expr_ops_t ops_tan = {
    .eval = eval_tan, .deriv = deriv_tan, .reverse = expr_reverse_tan,
    .kind = EXPR_KIND_TAN, .arity = EXPR_OP_UNARY, .name = "tan",
    .tex_name = "\\tan",
    .direct_inverse = &ops_atan,
    .inverse_unary = expr_atan,
    .apply_unary = expr_tan, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = expr_fold_zero_to_zero
};
const expr_ops_t ops_sec = {
    .eval = eval_sec, .deriv = deriv_sec, .reverse = expr_reverse_sec,
    .kind = EXPR_KIND_SEC, .arity = EXPR_OP_UNARY, .name = "sec",
    .tex_name = "\\sec",
    .direct_inverse = &ops_asec,
    .inverse_unary = expr_asec,
    .apply_unary = expr_sec, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_cosec = {
    .eval = eval_cosec, .deriv = deriv_cosec, .reverse = expr_reverse_cosec,
    .kind = EXPR_KIND_COSEC, .arity = EXPR_OP_UNARY, .name = "cosec",
    .tex_name = "\\operatorname{cosec}",
    .direct_inverse = &ops_acosec,
    .inverse_unary = expr_acosec,
    .apply_unary = expr_cosec, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_cot = {
    .eval = eval_cot, .deriv = deriv_cot, .reverse = expr_reverse_cot,
    .kind = EXPR_KIND_COT, .arity = EXPR_OP_UNARY, .name = "cot",
    .tex_name = "\\cot",
    .direct_inverse = &ops_acot,
    .inverse_unary = expr_acot,
    .apply_unary = expr_cot, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_sinh = {
    .eval = eval_sinh, .deriv = deriv_sinh, .reverse = expr_reverse_sinh,
    .kind = EXPR_KIND_SINH, .arity = EXPR_OP_UNARY, .name = "sinh",
    .tex_name = "\\sinh",
    .direct_inverse = &ops_asinh,
    .inverse_unary = expr_asinh,
    .apply_unary = expr_sinh, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_cosh = {
    .eval = eval_cosh, .deriv = deriv_cosh, .reverse = expr_reverse_cosh,
    .kind = EXPR_KIND_COSH, .arity = EXPR_OP_UNARY, .name = "cosh",
    .tex_name = "\\cosh",
    .direct_inverse = &ops_acosh,
    .inverse_unary = expr_acosh,
    .apply_unary = expr_cosh, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_tanh = {
    .eval = eval_tanh, .deriv = deriv_tanh, .reverse = expr_reverse_tanh,
    .kind = EXPR_KIND_TANH, .arity = EXPR_OP_UNARY, .name = "tanh",
    .tex_name = "\\tanh",
    .direct_inverse = &ops_atanh,
    .inverse_unary = expr_atanh,
    .apply_unary = expr_tanh, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_sech = {
    .eval = eval_sech, .deriv = deriv_sech, .reverse = expr_reverse_sech,
    .kind = EXPR_KIND_SECH, .arity = EXPR_OP_UNARY, .name = "sech",
    .tex_name = "\\operatorname{sech}",
    .direct_inverse = &ops_asech,
    .inverse_unary = expr_asech,
    .apply_unary = expr_sech, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_cosech = {
    .eval = eval_cosech, .deriv = deriv_cosech, .reverse = expr_reverse_cosech,
    .kind = EXPR_KIND_COSECH, .arity = EXPR_OP_UNARY, .name = "cosech",
    .tex_name = "\\operatorname{cosech}",
    .direct_inverse = &ops_acosech,
    .inverse_unary = expr_acosech,
    .apply_unary = expr_cosech, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_coth = {
    .eval = eval_coth, .deriv = deriv_coth, .reverse = expr_reverse_coth,
    .kind = EXPR_KIND_COTH, .arity = EXPR_OP_UNARY, .name = "coth",
    .tex_name = "\\coth",
    .direct_inverse = &ops_acoth,
    .inverse_unary = expr_acoth,
    .apply_unary = expr_coth, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_asin = {
    .eval = eval_asin, .deriv = deriv_asin, .reverse = expr_reverse_asin,
    .kind = EXPR_KIND_ASIN, .arity = EXPR_OP_UNARY, .name = "asin",
    .tex_name = "\\arcsin",
    .inverse_unary = expr_sin,
    .apply_unary = expr_asin, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_acos = {
    .eval = eval_acos, .deriv = deriv_acos, .reverse = expr_reverse_acos,
    .kind = EXPR_KIND_ACOS, .arity = EXPR_OP_UNARY, .name = "acos",
    .tex_name = "\\arccos",
    .inverse_unary = expr_cos,
    .apply_unary = expr_acos, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_atan = {
    .eval = eval_atan, .deriv = deriv_atan, .reverse = expr_reverse_atan,
    .kind = EXPR_KIND_ATAN, .arity = EXPR_OP_UNARY, .name = "atan",
    .tex_name = "\\arctan",
    .inverse_unary = expr_tan,
    .apply_unary = expr_atan, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_asec = {
    .eval = eval_asec, .deriv = deriv_asec, .reverse = expr_reverse_asec,
    .kind = EXPR_KIND_ASEC, .arity = EXPR_OP_UNARY, .name = "asec",
    .tex_name = "\\operatorname{arcsec}",
    .inverse_unary = expr_sec,
    .apply_unary = expr_asec, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_acosec = {
    .eval = eval_acosec, .deriv = deriv_acosec, .reverse = expr_reverse_acosec,
    .kind = EXPR_KIND_ACOSEC, .arity = EXPR_OP_UNARY, .name = "acosec",
    .tex_name = "\\operatorname{arccosec}",
    .inverse_unary = expr_cosec,
    .apply_unary = expr_acosec, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_acot = {
    .eval = eval_acot, .deriv = deriv_acot, .reverse = expr_reverse_acot,
    .kind = EXPR_KIND_ACOT, .arity = EXPR_OP_UNARY, .name = "acot",
    .tex_name = "\\operatorname{arccot}",
    .inverse_unary = expr_cot,
    .apply_unary = expr_acot, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_asinh = {
    .eval = eval_asinh, .deriv = deriv_asinh, .reverse = expr_reverse_asinh,
    .kind = EXPR_KIND_ASINH, .arity = EXPR_OP_UNARY, .name = "asinh",
    .tex_name = "\\operatorname{asinh}",
    .inverse_unary = expr_sinh,
    .apply_unary = expr_asinh, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_acosh = {
    .eval = eval_acosh, .deriv = deriv_acosh, .reverse = expr_reverse_acosh,
    .kind = EXPR_KIND_ACOSH, .arity = EXPR_OP_UNARY, .name = "acosh",
    .tex_name = "\\operatorname{acosh}",
    .inverse_unary = expr_cosh,
    .apply_unary = expr_acosh, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_atanh = {
    .eval = eval_atanh, .deriv = deriv_atanh, .reverse = expr_reverse_atanh,
    .kind = EXPR_KIND_ATANH, .arity = EXPR_OP_UNARY, .name = "atanh",
    .tex_name = "\\operatorname{atanh}",
    .inverse_unary = expr_tanh,
    .apply_unary = expr_atanh, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_asech = {
    .eval = eval_asech, .deriv = deriv_asech, .reverse = expr_reverse_asech,
    .kind = EXPR_KIND_ASECH, .arity = EXPR_OP_UNARY, .name = "asech",
    .tex_name = "\\operatorname{arsech}",
    .inverse_unary = expr_sech,
    .apply_unary = expr_asech, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_acosech = {
    .eval = eval_acosech, .deriv = deriv_acosech, .reverse = expr_reverse_acosech,
    .kind = EXPR_KIND_ACOSECH, .arity = EXPR_OP_UNARY, .name = "acosech",
    .tex_name = "\\operatorname{arcosech}",
    .inverse_unary = expr_cosech,
    .apply_unary = expr_acosech, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_acoth = {
    .eval = eval_acoth, .deriv = deriv_acoth, .reverse = expr_reverse_acoth,
    .kind = EXPR_KIND_ACOTH, .arity = EXPR_OP_UNARY, .name = "acoth",
    .tex_name = "\\operatorname{arcoth}",
    .inverse_unary = expr_coth,
    .apply_unary = expr_acoth, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_exp = {
    .eval = eval_exp, .deriv = deriv_exp, .reverse = expr_reverse_exp,
    .kind = EXPR_KIND_EXP, .arity = EXPR_OP_UNARY, .name = "exp",
    .tex_name = "\\exp",
    .direct_inverse = &ops_log,
    .inverse_unary = expr_log,
    .apply_unary = expr_exp, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = expr_fold_exp_const
};
const expr_ops_t ops_log = {
    .eval = eval_log, .deriv = deriv_log, .reverse = expr_reverse_log,
    .kind = EXPR_KIND_LOG, .arity = EXPR_OP_UNARY, .name = "ln",
    .tex_name = "\\ln",
    .direct_inverse = &ops_exp,
    .inverse_unary = expr_exp,
    .apply_unary = expr_log, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = expr_fold_log_const
};
const expr_ops_t ops_log10 = {
    .eval = eval_log10, .deriv = deriv_log10, .reverse = expr_reverse_log10,
    .kind = EXPR_KIND_LOG10, .arity = EXPR_OP_UNARY, .name = "log",
    .tex_name = "\\log",
    .inverse_unary = expr_inverse_log10_internal,
    .apply_unary = expr_log10, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_sqrt = {
    .eval = eval_sqrt, .deriv = deriv_sqrt, .reverse = expr_reverse_sqrt,
    .kind = EXPR_KIND_SQRT, .arity = EXPR_OP_UNARY, .name = "sqrt",
    .tex_name = "\\sqrt",
    .inverse_unary = expr_inverse_sqrt_internal,
    .apply_unary = expr_sqrt, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = expr_fold_sqrt_const
};
const expr_ops_t ops_floor = {
    .eval = eval_floor, .deriv = deriv_floor, .reverse = expr_reverse_floor,
    .kind = EXPR_KIND_FLOOR, .arity = EXPR_OP_UNARY, .name = "floor",
    .tex_name = "\\lfloor",
    .apply_unary = expr_floor, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = expr_fold_floor_const
};
const expr_ops_t ops_ceil = {
    .eval = eval_ceil, .deriv = deriv_ceil, .reverse = expr_reverse_ceil,
    .kind = EXPR_KIND_CEIL, .arity = EXPR_OP_UNARY, .name = "ceil",
    .tex_name = "\\lceil",
    .apply_unary = expr_ceil, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_abs = {
    .eval = eval_abs, .deriv = deriv_abs, .reverse = expr_reverse_abs,
    .kind = EXPR_KIND_ABS, .arity = EXPR_OP_UNARY, .name = "abs",
    .tex_name = NULL,
    .apply_unary = expr_abs, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_erf = {
    .eval = eval_erf, .deriv = deriv_erf, .reverse = expr_reverse_erf,
    .kind = EXPR_KIND_ERF, .arity = EXPR_OP_UNARY, .name = "erf",
    .tex_name = "\\operatorname{erf}",
    .direct_inverse = &ops_erfinv,
    .inverse_unary = expr_erfinv,
    .apply_unary = expr_erf, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_erfc = {
    .eval = eval_erfc, .deriv = deriv_erfc, .reverse = expr_reverse_erfc,
    .kind = EXPR_KIND_ERFC, .arity = EXPR_OP_UNARY, .name = "erfc",
    .tex_name = "\\operatorname{erfc}",
    .direct_inverse = &ops_erfcinv,
    .inverse_unary = expr_erfcinv,
    .apply_unary = expr_erfc, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_lgamma = {
    .eval = eval_lgamma, .deriv = deriv_lgamma, .reverse = expr_reverse_lgamma,
    .kind = EXPR_KIND_LGAMMA, .arity = EXPR_OP_UNARY, .name = "lgamma",
    .tex_name = "\\log\\Gamma",
    .apply_unary = expr_lgamma, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_hypot = {
    .eval = eval_hypot, .deriv = deriv_hypot, .reverse = expr_reverse_hypot,
    .kind = EXPR_KIND_HYPOT, .arity = EXPR_OP_BINARY, .name = "hypot",
    .tex_name = "\\operatorname{hypot}",
    .apply_unary = NULL, .apply_binary = expr_hypot,
    .simplify = expr_simplify_hypot_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_erfinv = {
    .eval = eval_erfinv, .deriv = deriv_erfinv, .reverse = expr_reverse_erfinv,
    .kind = EXPR_KIND_ERFINV, .arity = EXPR_OP_UNARY, .name = "erfinv",
    .tex_name = "\\operatorname{erf}^{-1}",
    .inverse_unary = expr_erf,
    .apply_unary = expr_erfinv, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_erfcinv = {
    .eval = eval_erfcinv, .deriv = deriv_erfcinv, .reverse = expr_reverse_erfcinv,
    .kind = EXPR_KIND_ERFCINV, .arity = EXPR_OP_UNARY, .name = "erfcinv",
    .tex_name = "\\operatorname{erfc}^{-1}",
    .inverse_unary = expr_erfc,
    .apply_unary = expr_erfcinv, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_gamma = {
    .eval = eval_gamma, .deriv = deriv_gamma, .reverse = expr_reverse_gamma,
    .kind = EXPR_KIND_GAMMA, .arity = EXPR_OP_UNARY, .name = "gamma",
    .tex_name = "\\Gamma",
    .direct_inverse = &ops_gammainv,
    .inverse_unary = expr_gammainv,
    .apply_unary = expr_gamma, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_digamma = {
    .eval = eval_digamma, .deriv = deriv_digamma, .reverse = expr_reverse_digamma,
    .kind = EXPR_KIND_DIGAMMA, .arity = EXPR_OP_UNARY, .name = "digamma",
    .tex_name = "\\psi^{(0)}",
    .apply_unary = expr_digamma, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_trigamma = {
    .eval = eval_trigamma, .deriv = deriv_trigamma, .reverse = expr_reverse_trigamma,
    .kind = EXPR_KIND_TRIGAMMA, .arity = EXPR_OP_UNARY, .name = "trigamma",
    .tex_name = "\\psi^{(1)}",
    .apply_unary = expr_trigamma, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_polygamma = {
    .eval = eval_polygamma, .deriv = deriv_polygamma, .reverse = expr_reverse_polygamma,
    .kind = EXPR_KIND_POLYGAMMA, .arity = EXPR_OP_BINARY, .name = "polygamma",
    .tex_name = "\\psi",
    .apply_unary = NULL, .apply_binary = expr_polygamma_xp,
    .simplify = expr_simplify_binary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_gammainv = {
    .eval = eval_gammainv, .deriv = deriv_gammainv, .reverse = expr_reverse_gammainv,
    .kind = EXPR_KIND_GAMMAINV, .arity = EXPR_OP_UNARY, .name = "gammainv",
    .tex_name = "\\Gamma^{-1}",
    .inverse_unary = expr_gamma,
    .apply_unary = expr_gammainv, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_lambert_w = {
    .eval = eval_lambert_w, .deriv = deriv_lambert_w, .reverse = expr_reverse_lambert_w,
    .kind = EXPR_KIND_LAMBERT_W, .arity = EXPR_OP_UNARY, .name = "W",
    .tex_name = "W",
    .inverse_unary = expr_inverse_lambert_internal,
    .apply_unary = expr_lambert_w, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_lambert_w0 = {
    .eval = eval_lambert_w0, .deriv = deriv_lambert_w0, .reverse = expr_reverse_lambert_w0,
    .kind = EXPR_KIND_LAMBERT_W0, .arity = EXPR_OP_UNARY, .name = "W₀",
    .tex_name = "W_{0}",
    .inverse_unary = expr_inverse_lambert_internal,
    .apply_unary = expr_lambert_w0, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_lambert_wm1 = {
    .eval = eval_lambert_wm1, .deriv = deriv_lambert_wm1, .reverse = expr_reverse_lambert_wm1,
    .kind = EXPR_KIND_LAMBERT_WM1, .arity = EXPR_OP_UNARY, .name = "W₋₁",
    .tex_name = "W_{-1}",
    .inverse_unary = expr_inverse_lambert_internal,
    .apply_unary = expr_lambert_wm1, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_normal_pdf = {
    .eval = eval_normal_pdf, .deriv = deriv_normal_pdf, .reverse = expr_reverse_normal_pdf,
    .kind = EXPR_KIND_NORMAL_PDF, .arity = EXPR_OP_UNARY, .name = "normal_pdf",
    .tex_name = "\\operatorname{normal\\_pdf}",
    .apply_unary = expr_normal_pdf, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_normal_cdf = {
    .eval = eval_normal_cdf, .deriv = deriv_normal_cdf, .reverse = expr_reverse_normal_cdf,
    .kind = EXPR_KIND_NORMAL_CDF, .arity = EXPR_OP_UNARY, .name = "normal_cdf",
    .tex_name = "\\operatorname{normal\\_cdf}",
    .apply_unary = expr_normal_cdf, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_normal_logpdf = {
    .eval = eval_normal_logpdf, .deriv = deriv_normal_logpdf, .reverse = expr_reverse_normal_logpdf,
    .kind = EXPR_KIND_NORMAL_LOGPDF, .arity = EXPR_OP_UNARY, .name = "normal_logpdf",
    .tex_name = "\\operatorname{normal\\_logpdf}",
    .apply_unary = expr_normal_logpdf, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_pdf = {
    .eval = eval_normal_pdf, .deriv = deriv_pdf, .reverse = expr_reverse_normal_pdf,
    .kind = EXPR_KIND_NORMAL_PDF, .arity = EXPR_OP_UNARY, .name = "pdf",
    .tex_name = "\\operatorname{pdf}",
    .apply_unary = expr_pdf, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_cdf = {
    .eval = eval_normal_cdf, .deriv = deriv_cdf, .reverse = expr_reverse_normal_cdf,
    .kind = EXPR_KIND_NORMAL_CDF, .arity = EXPR_OP_UNARY, .name = "cdf",
    .tex_name = "\\operatorname{cdf}",
    .apply_unary = expr_cdf, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_logpdf = {
    .eval = eval_normal_logpdf, .deriv = deriv_logpdf, .reverse = expr_reverse_normal_logpdf,
    .kind = EXPR_KIND_NORMAL_LOGPDF, .arity = EXPR_OP_UNARY, .name = "logpdf",
    .tex_name = "\\operatorname{logpdf}",
    .apply_unary = expr_logpdf, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_ei = {
    .eval = eval_ei, .deriv = deriv_ei, .reverse = expr_reverse_ei,
    .kind = EXPR_KIND_EI, .arity = EXPR_OP_UNARY, .name = "Ei",
    .tex_name = "\\operatorname{Ei}",
    .apply_unary = expr_ei, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_e1 = {
    .eval = eval_e1, .deriv = deriv_e1, .reverse = expr_reverse_e1,
    .kind = EXPR_KIND_E1, .arity = EXPR_OP_UNARY, .name = "E1",
    .tex_name = "\\operatorname{E1}",
    .apply_unary = expr_e1, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_beta = {
    .eval = eval_beta, .deriv = deriv_beta, .reverse = expr_reverse_beta,
    .kind = EXPR_KIND_BETA, .arity = EXPR_OP_BINARY, .name = "beta",
    .tex_name = "\\operatorname{beta}",
    .apply_unary = NULL, .apply_binary = expr_beta,
    .simplify = expr_simplify_binary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_logbeta = {
    .eval = eval_logbeta, .deriv = deriv_logbeta, .reverse = expr_reverse_logbeta,
    .kind = EXPR_KIND_LOGBETA, .arity = EXPR_OP_BINARY, .name = "logbeta",
    .tex_name = "\\operatorname{logbeta}",
    .apply_unary = NULL, .apply_binary = expr_logbeta,
    .simplify = expr_simplify_binary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_gammainc_lower = {
    .eval = eval_gammainc_lower, .deriv = deriv_gammainc_lower, .reverse = expr_reverse_gammainc_lower,
    .kind = EXPR_KIND_GAMMAINC_LOWER, .arity = EXPR_OP_BINARY, .name = "gammainc_lower",
    .tex_name = "\\gamma",
    .apply_unary = NULL, .apply_binary = expr_gammainc_lower,
    .simplify = expr_simplify_binary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_gammainc_upper = {
    .eval = eval_gammainc_upper, .deriv = deriv_gammainc_upper, .reverse = expr_reverse_gammainc_upper,
    .kind = EXPR_KIND_GAMMAINC_UPPER, .arity = EXPR_OP_BINARY, .name = "gammainc_upper",
    .tex_name = "\\Gamma",
    .apply_unary = NULL, .apply_binary = expr_gammainc_upper,
    .simplify = expr_simplify_binary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_gammainc_P = {
    .eval = eval_gammainc_P, .deriv = deriv_gammainc_P, .reverse = expr_reverse_gammainc_P,
    .kind = EXPR_KIND_GAMMAINC_P, .arity = EXPR_OP_BINARY, .name = "gammainc_P",
    .tex_name = "\\operatorname{P}",
    .apply_unary = NULL, .apply_binary = expr_gammainc_P,
    .simplify = expr_simplify_binary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_gammainc_Q = {
    .eval = eval_gammainc_Q, .deriv = deriv_gammainc_Q, .reverse = expr_reverse_gammainc_Q,
    .kind = EXPR_KIND_GAMMAINC_Q, .arity = EXPR_OP_BINARY, .name = "gammainc_Q",
    .tex_name = "\\operatorname{Q}",
    .apply_unary = NULL, .apply_binary = expr_gammainc_Q,
    .simplify = expr_simplify_binary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_factorial = {
    .eval = eval_factorial, .deriv = deriv_not_differentiable,
    .reverse = expr_reverse_not_differentiable,
    .kind = EXPR_KIND_FACTORIAL, .arity = EXPR_OP_UNARY, .diff_kind = EXPR_DIFF_NONE,
    .name = "factorial", .tex_name = "\\operatorname{factorial}",
    .apply_unary = expr_factorial, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_fibonacci = {
    .eval = eval_fibonacci, .deriv = deriv_not_differentiable,
    .reverse = expr_reverse_not_differentiable,
    .kind = EXPR_KIND_FIBONACCI, .arity = EXPR_OP_UNARY, .diff_kind = EXPR_DIFF_NONE,
    .name = "fibonacci", .tex_name = "\\operatorname{fibonacci}",
    .apply_unary = expr_fibonacci, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_partition = {
    .eval = eval_partition, .deriv = deriv_not_differentiable,
    .reverse = expr_reverse_not_differentiable,
    .kind = EXPR_KIND_PARTITION, .arity = EXPR_OP_UNARY, .diff_kind = EXPR_DIFF_NONE,
    .name = "partition", .tex_name = "\\operatorname{partition}",
    .apply_unary = expr_partition, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_isqrt = {
    .eval = eval_isqrt, .deriv = deriv_not_differentiable,
    .reverse = expr_reverse_not_differentiable,
    .kind = EXPR_KIND_ISQRT, .arity = EXPR_OP_UNARY, .diff_kind = EXPR_DIFF_NONE,
    .name = "isqrt", .tex_name = "\\operatorname{isqrt}",
    .apply_unary = expr_isqrt, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_gcd = {
    .eval = eval_gcd, .deriv = deriv_not_differentiable,
    .reverse = expr_reverse_not_differentiable,
    .kind = EXPR_KIND_GCD, .arity = EXPR_OP_BINARY, .diff_kind = EXPR_DIFF_NONE,
    .name = "gcd", .tex_name = "\\gcd",
    .apply_unary = NULL, .apply_binary = expr_gcd,
    .simplify = expr_simplify_binary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_lcm = {
    .eval = eval_lcm, .deriv = deriv_not_differentiable,
    .reverse = expr_reverse_not_differentiable,
    .kind = EXPR_KIND_LCM, .arity = EXPR_OP_BINARY, .diff_kind = EXPR_DIFF_NONE,
    .name = "lcm", .tex_name = "\\operatorname{lcm}",
    .apply_unary = NULL, .apply_binary = expr_lcm,
    .simplify = expr_simplify_binary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_mod = {
    .eval = eval_mod, .deriv = deriv_not_differentiable,
    .reverse = expr_reverse_not_differentiable,
    .kind = EXPR_KIND_MOD, .arity = EXPR_OP_BINARY, .diff_kind = EXPR_DIFF_NONE,
    .name = "mod", .tex_name = "\\operatorname{mod}",
    .apply_unary = NULL, .apply_binary = expr_mod,
    .simplify = expr_simplify_binary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_modinv = {
    .eval = eval_modinv, .deriv = deriv_not_differentiable,
    .reverse = expr_reverse_not_differentiable,
    .kind = EXPR_KIND_MODINV, .arity = EXPR_OP_BINARY, .diff_kind = EXPR_DIFF_NONE,
    .name = "modinv", .tex_name = "\\operatorname{modinv}",
    .apply_unary = NULL, .apply_binary = expr_modinv,
    .simplify = expr_simplify_binary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_is_prime = {
    .eval = eval_is_prime, .deriv = deriv_not_differentiable,
    .reverse = expr_reverse_not_differentiable,
    .kind = EXPR_KIND_IS_PRIME, .arity = EXPR_OP_UNARY, .diff_kind = EXPR_DIFF_NONE,
    .name = "is_prime", .tex_name = "\\operatorname{is\\_prime}",
    .apply_unary = expr_is_prime, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_next_prime = {
    .eval = eval_next_prime, .deriv = deriv_not_differentiable,
    .reverse = expr_reverse_not_differentiable,
    .kind = EXPR_KIND_NEXT_PRIME, .arity = EXPR_OP_UNARY, .diff_kind = EXPR_DIFF_NONE,
    .name = "next_prime", .tex_name = "\\operatorname{next\\_prime}",
    .apply_unary = expr_next_prime, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_prev_prime = {
    .eval = eval_prev_prime, .deriv = deriv_not_differentiable,
    .reverse = expr_reverse_not_differentiable,
    .kind = EXPR_KIND_PREV_PRIME, .arity = EXPR_OP_UNARY, .diff_kind = EXPR_DIFF_NONE,
    .name = "prev_prime", .tex_name = "\\operatorname{prev\\_prime}",
    .apply_unary = expr_prev_prime, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_bit_and = {
    .eval = eval_bit_and, .deriv = deriv_not_differentiable,
    .reverse = expr_reverse_not_differentiable,
    .kind = EXPR_KIND_BIT_AND, .arity = EXPR_OP_BINARY, .diff_kind = EXPR_DIFF_NONE,
    .name = "AND", .tex_name = "\\operatorname{AND}",
    .apply_unary = NULL, .apply_binary = expr_bit_and,
    .simplify = expr_simplify_binary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_bit_or = {
    .eval = eval_bit_or, .deriv = deriv_not_differentiable,
    .reverse = expr_reverse_not_differentiable,
    .kind = EXPR_KIND_BIT_OR, .arity = EXPR_OP_BINARY, .diff_kind = EXPR_DIFF_NONE,
    .name = "OR", .tex_name = "\\operatorname{OR}",
    .apply_unary = NULL, .apply_binary = expr_bit_or,
    .simplify = expr_simplify_binary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_bit_xor = {
    .eval = eval_bit_xor, .deriv = deriv_not_differentiable,
    .reverse = expr_reverse_not_differentiable,
    .kind = EXPR_KIND_BIT_XOR, .arity = EXPR_OP_BINARY, .diff_kind = EXPR_DIFF_NONE,
    .name = "XOR", .tex_name = "\\operatorname{XOR}",
    .apply_unary = NULL, .apply_binary = expr_bit_xor,
    .simplify = expr_simplify_binary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_bit_not = {
    .eval = eval_bit_not, .deriv = deriv_not_differentiable,
    .reverse = expr_reverse_not_differentiable,
    .kind = EXPR_KIND_BIT_NOT, .arity = EXPR_OP_UNARY, .diff_kind = EXPR_DIFF_NONE,
    .name = "NOT", .tex_name = "\\operatorname{NOT}",
    .apply_unary = expr_bit_not, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_shl = {
    .eval = eval_shl, .deriv = deriv_not_differentiable,
    .reverse = expr_reverse_not_differentiable,
    .kind = EXPR_KIND_SHL, .arity = EXPR_OP_BINARY, .diff_kind = EXPR_DIFF_NONE,
    .name = "SHL", .tex_name = "\\operatorname{SHL}",
    .apply_unary = NULL, .apply_binary = expr_shl,
    .simplify = expr_simplify_binary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_shr = {
    .eval = eval_shr, .deriv = deriv_not_differentiable,
    .reverse = expr_reverse_not_differentiable,
    .kind = EXPR_KIND_SHR, .arity = EXPR_OP_BINARY, .diff_kind = EXPR_DIFF_NONE,
    .name = "SHR", .tex_name = "\\operatorname{SHR}",
    .apply_unary = NULL, .apply_binary = expr_shr,
    .simplify = expr_simplify_binary_operator, .fold_const_unary = NULL
};
const expr_ops_t ops_factors = {
    .eval = eval_factors, .deriv = deriv_not_differentiable,
    .reverse = expr_reverse_not_differentiable,
    .kind = EXPR_KIND_FACTORS, .arity = EXPR_OP_UNARY, .diff_kind = EXPR_DIFF_NONE,
    .name = "factors", .tex_name = "\\operatorname{factors}",
    .apply_unary = expr_factors, .apply_binary = NULL,
    .simplify = expr_simplify_unary_operator, .fold_const_unary = NULL
};

expr_t *expr_sqrt(const expr_t *a) { return expr_math_wrap_unary(&ops_sqrt, a); }
expr_t *expr_exp(const expr_t *a) { return expr_math_wrap_unary(&ops_exp, a); }
expr_t *expr_log(const expr_t *a) { return expr_math_wrap_unary(&ops_log, a); }
expr_t *expr_log10(const expr_t *a) { return expr_math_wrap_unary(&ops_log10, a); }
expr_t *expr_floor(const expr_t *a) { return expr_math_wrap_unary(&ops_floor, a); }
expr_t *expr_ceil(const expr_t *a) { return expr_math_wrap_unary(&ops_ceil, a); }
expr_t *expr_sin(const expr_t *a) { return expr_math_wrap_unary(&ops_sin, a); }
expr_t *expr_cos(const expr_t *a) { return expr_math_wrap_unary(&ops_cos, a); }
expr_t *expr_tan(const expr_t *a) { return expr_math_wrap_unary(&ops_tan, a); }
expr_t *expr_sec(const expr_t *a) { return expr_math_wrap_unary(&ops_sec, a); }
expr_t *expr_cosec(const expr_t *a) { return expr_math_wrap_unary(&ops_cosec, a); }
expr_t *expr_cot(const expr_t *a) { return expr_math_wrap_unary(&ops_cot, a); }
expr_t *expr_sinh(const expr_t *a) { return expr_math_wrap_unary(&ops_sinh, a); }
expr_t *expr_cosh(const expr_t *a) { return expr_math_wrap_unary(&ops_cosh, a); }
expr_t *expr_tanh(const expr_t *a) { return expr_math_wrap_unary(&ops_tanh, a); }
expr_t *expr_sech(const expr_t *a) { return expr_math_wrap_unary(&ops_sech, a); }
expr_t *expr_cosech(const expr_t *a) { return expr_math_wrap_unary(&ops_cosech, a); }
expr_t *expr_coth(const expr_t *a) { return expr_math_wrap_unary(&ops_coth, a); }
expr_t *expr_asin(const expr_t *a) { return expr_math_wrap_unary(&ops_asin, a); }
expr_t *expr_acos(const expr_t *a) { return expr_math_wrap_unary(&ops_acos, a); }
expr_t *expr_atan(const expr_t *a) { return expr_math_wrap_unary(&ops_atan, a); }
expr_t *expr_asec(const expr_t *a) { return expr_math_wrap_unary(&ops_asec, a); }
expr_t *expr_acosec(const expr_t *a) { return expr_math_wrap_unary(&ops_acosec, a); }
expr_t *expr_acot(const expr_t *a) { return expr_math_wrap_unary(&ops_acot, a); }
expr_t *expr_atan2(const expr_t *a, const expr_t *b) { return expr_math_wrap_binary(&ops_atan2, a, b); }
expr_t *expr_asinh(const expr_t *a) { return expr_math_wrap_unary(&ops_asinh, a); }
expr_t *expr_acosh(const expr_t *a) { return expr_math_wrap_unary(&ops_acosh, a); }
expr_t *expr_atanh(const expr_t *a) { return expr_math_wrap_unary(&ops_atanh, a); }
expr_t *expr_asech(const expr_t *a) { return expr_math_wrap_unary(&ops_asech, a); }
expr_t *expr_acosech(const expr_t *a) { return expr_math_wrap_unary(&ops_acosech, a); }
expr_t *expr_acoth(const expr_t *a) { return expr_math_wrap_unary(&ops_acoth, a); }
expr_t *expr_abs(const expr_t *a) { return expr_math_wrap_unary(&ops_abs, a); }
expr_t *expr_erf(const expr_t *a) { return expr_math_wrap_unary(&ops_erf, a); }
expr_t *expr_erfc(const expr_t *a) { return expr_math_wrap_unary(&ops_erfc, a); }
expr_t *expr_lgamma(const expr_t *a) { return expr_math_wrap_unary(&ops_lgamma, a); }
expr_t *expr_hypot(const expr_t *a, const expr_t *b) { return expr_math_wrap_binary(&ops_hypot, a, b); }
expr_t *expr_erfinv(const expr_t *a) { return expr_math_wrap_unary(&ops_erfinv, a); }
expr_t *expr_erfcinv(const expr_t *a) { return expr_math_wrap_unary(&ops_erfcinv, a); }
expr_t *expr_gamma(const expr_t *a) { return expr_math_wrap_unary(&ops_gamma, a); }
expr_t *expr_digamma(const expr_t *a) { return expr_math_wrap_unary(&ops_digamma, a); }
expr_t *expr_trigamma(const expr_t *a) { return expr_math_wrap_unary(&ops_trigamma, a); }
expr_t *expr_polygamma_xp(const expr_t *order, const expr_t *arg) { return expr_math_wrap_binary(&ops_polygamma, order, arg); }
expr_t *expr_polygamma(unsigned int order, const expr_t *a)
{
    NUM_SCOPE(scope);
    number_t order_value = num_create_from_long((long)order);
    expr_t *order_xp = expr_new_const(order_value);
    expr_t *out = expr_polygamma_xp(order_xp, a);

    expr_free(order_xp);
    return out;
}
expr_t *expr_gammainv(const expr_t *a) { return expr_math_wrap_unary(&ops_gammainv, a); }
expr_t *expr_lambert_w(const expr_t *a) { return expr_math_wrap_unary(&ops_lambert_w, a); }
expr_t *expr_lambert_w0(const expr_t *a) { return expr_math_wrap_unary(&ops_lambert_w0, a); }
expr_t *expr_lambert_wm1(const expr_t *a) { return expr_math_wrap_unary(&ops_lambert_wm1, a); }
expr_t *expr_normal_pdf(const expr_t *a) { return expr_math_wrap_unary(&ops_normal_pdf, a); }
expr_t *expr_normal_cdf(const expr_t *a) { return expr_math_wrap_unary(&ops_normal_cdf, a); }
expr_t *expr_normal_logpdf(const expr_t *a) { return expr_math_wrap_unary(&ops_normal_logpdf, a); }
expr_t *expr_pdf(const expr_t *a) { return expr_math_wrap_unary(&ops_pdf, a); }
expr_t *expr_cdf(const expr_t *a) { return expr_math_wrap_unary(&ops_cdf, a); }
expr_t *expr_logpdf(const expr_t *a) { return expr_math_wrap_unary(&ops_logpdf, a); }
expr_t *expr_ei(const expr_t *a) { return expr_math_wrap_unary(&ops_ei, a); }
expr_t *expr_e1(const expr_t *a) { return expr_math_wrap_unary(&ops_e1, a); }
expr_t *expr_beta(const expr_t *a, const expr_t *b) { return expr_math_wrap_binary(&ops_beta, a, b); }
expr_t *expr_logbeta(const expr_t *a, const expr_t *b) { return expr_math_wrap_binary(&ops_logbeta, a, b); }
expr_t *expr_gammainc_lower(const expr_t *a, const expr_t *b) { return expr_math_wrap_binary(&ops_gammainc_lower, a, b); }
expr_t *expr_gammainc_upper(const expr_t *a, const expr_t *b) { return expr_math_wrap_binary(&ops_gammainc_upper, a, b); }
expr_t *expr_gammainc_P(const expr_t *a, const expr_t *b) { return expr_math_wrap_binary(&ops_gammainc_P, a, b); }
expr_t *expr_gammainc_Q(const expr_t *a, const expr_t *b) { return expr_math_wrap_binary(&ops_gammainc_Q, a, b); }
expr_t *expr_factorial(const expr_t *a) { return expr_math_wrap_unary(&ops_factorial, a); }
expr_t *expr_fibonacci(const expr_t *a) { return expr_math_wrap_unary(&ops_fibonacci, a); }
expr_t *expr_partition(const expr_t *a) { return expr_math_wrap_unary(&ops_partition, a); }
expr_t *expr_isqrt(const expr_t *a) { return expr_math_wrap_unary(&ops_isqrt, a); }
expr_t *expr_gcd(const expr_t *a, const expr_t *b) { return expr_math_wrap_binary(&ops_gcd, a, b); }
expr_t *expr_lcm(const expr_t *a, const expr_t *b) { return expr_math_wrap_binary(&ops_lcm, a, b); }
expr_t *expr_mod(const expr_t *a, const expr_t *b) { return expr_math_wrap_binary(&ops_mod, a, b); }
expr_t *expr_modinv(const expr_t *a, const expr_t *b) { return expr_math_wrap_binary(&ops_modinv, a, b); }
expr_t *expr_is_prime(const expr_t *a) { return expr_math_wrap_unary(&ops_is_prime, a); }
expr_t *expr_next_prime(const expr_t *a) { return expr_math_wrap_unary(&ops_next_prime, a); }
expr_t *expr_prev_prime(const expr_t *a) { return expr_math_wrap_unary(&ops_prev_prime, a); }
expr_t *expr_bit_and(const expr_t *a, const expr_t *b) { return expr_math_wrap_binary(&ops_bit_and, a, b); }
expr_t *expr_bit_or(const expr_t *a, const expr_t *b) { return expr_math_wrap_binary(&ops_bit_or, a, b); }
expr_t *expr_bit_xor(const expr_t *a, const expr_t *b) { return expr_math_wrap_binary(&ops_bit_xor, a, b); }
expr_t *expr_bit_not(const expr_t *a) { return expr_math_wrap_unary(&ops_bit_not, a); }
expr_t *expr_shl(const expr_t *a, const expr_t *b) { return expr_math_wrap_binary(&ops_shl, a, b); }
expr_t *expr_shr(const expr_t *a, const expr_t *b) { return expr_math_wrap_binary(&ops_shr, a, b); }

static expr_t *expr_factor_product_from_number(number_t value)
{
    enum { FACTOR_MAX_BITS = 1024 };
    number_factors_t *factors;
    expr_t *out = NULL;

    if (!num_is_integer(value) || num_get_sign(value) <= 0 ||
        num_bit_length(value) > FACTOR_MAX_BITS)
        return NULL;

    factors = num_factors(value);
    if (!factors)
        return NULL;

    if (factors->count == 0u) {
        num_factors_free(factors);
        return expr_new_const(NUM_ONE);
    }

    for (size_t i = 0u; i < factors->count; ++i) {
        char name[32];
        expr_t *base;
        expr_t *factor;

        snprintf(name, sizeof(name), "a%zu", i);
        base = expr_new_named_const(factors->items[i].prime, name);
        {
            char *prime_text = num_to_string(factors->items[i].prime);

            base->binding_expr =
                expr_binding_expr_new_number_text(prime_text ? prime_text : "NAN");
            free(prime_text);
        }
        if (factors->items[i].exponent > 1u) {
            number_t exponent = num_create_from_long((long)factors->items[i].exponent);

            factor = expr_pow(base, &exponent);
            num_destroy(&exponent);
            expr_free(base);
        } else {
            factor = base;
        }

        if (out) {
            expr_t *next = expr_mul(out, factor);

            expr_free(out);
            expr_free(factor);
            out = next;
        } else {
            out = factor;
        }
    }

    num_factors_free(factors);
    return out;
}

expr_t *expr_factors(const expr_t *a)
{
    expr_t *product;

    if (!a)
        return NULL;

    product = expr_factor_product_from_number(expr_eval_num_internal(a));
    if (product)
        return product;

    return expr_math_wrap_unary(&ops_factors, a);
}

expr_t *expr_logbeta_pdf(const expr_t *x, const expr_t *a, const expr_t *b)
{
    expr_t *one;
    expr_t *a_minus_one;
    expr_t *b_minus_one;
    expr_t *log_x;
    expr_t *one_minus_x;
    expr_t *log_one_minus_x;
    expr_t *left;
    expr_t *right;
    expr_t *sum;
    expr_t *log_beta;
    expr_t *out;

    if (!x || !a || !b)
        return NULL;

    one = expr_new_const(NUM_ONE);
    a_minus_one = expr_sub(a, one);
    b_minus_one = expr_sub(b, one);
    log_x = expr_log(x);
    one_minus_x = expr_sub(one, x);
    log_one_minus_x = expr_log(one_minus_x);
    left = expr_mul(a_minus_one, log_x);
    right = expr_mul(b_minus_one, log_one_minus_x);
    sum = expr_add(left, right);
    log_beta = expr_logbeta(a, b);
    out = expr_sub(sum, log_beta);

    expr_free(log_beta);
    expr_free(sum);
    expr_free(right);
    expr_free(left);
    expr_free(log_one_minus_x);
    expr_free(one_minus_x);
    expr_free(log_x);
    expr_free(b_minus_one);
    expr_free(a_minus_one);
    expr_free(one);
    return out;
}

expr_t *expr_beta_pdf(const expr_t *x, const expr_t *a, const expr_t *b)
{
    expr_t *log_pdf = expr_logbeta_pdf(x, a, b);
    expr_t *out = log_pdf ? expr_exp(log_pdf) : NULL;

    expr_free(log_pdf);
    return out;
}

expr_t *expr_binomial(const expr_t *n, const expr_t *k)
{
    expr_t *one;
    expr_t *n_plus_one;
    expr_t *k_plus_one;
    expr_t *n_minus_k;
    expr_t *n_minus_k_plus_one;
    expr_t *gamma_n;
    expr_t *gamma_k;
    expr_t *gamma_n_minus_k;
    expr_t *denominator;
    expr_t *out;

    if (!n || !k)
        return NULL;

    one = expr_new_const(NUM_ONE);
    n_plus_one = expr_add(n, one);
    k_plus_one = expr_add(k, one);
    n_minus_k = expr_sub(n, k);
    n_minus_k_plus_one = expr_add(n_minus_k, one);
    gamma_n = expr_gamma(n_plus_one);
    gamma_k = expr_gamma(k_plus_one);
    gamma_n_minus_k = expr_gamma(n_minus_k_plus_one);
    denominator = expr_mul(gamma_k, gamma_n_minus_k);
    out = expr_div(gamma_n, denominator);

    expr_free(denominator);
    expr_free(gamma_n_minus_k);
    expr_free(gamma_k);
    expr_free(gamma_n);
    expr_free(n_minus_k_plus_one);
    expr_free(n_minus_k);
    expr_free(k_plus_one);
    expr_free(n_plus_one);
    expr_free(one);
    return out;
}
