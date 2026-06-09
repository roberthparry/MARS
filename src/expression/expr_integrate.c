#include "expr_internal.h"
#include "expr_integrate_internal.h"

typedef expr_t *(*expr_integrate_rule_fn)(const expr_t *expr, const expr_t *wrt);

typedef struct expr_integrate_dispatch_rule {
    expr_integrate_rule_fn structural;
    expr_integrate_rule_fn primitive;
} expr_integrate_dispatch_rule_t;

static const expr_integrate_dispatch_rule_t integrate_dispatch_rules[EXPR_KIND_COUNT] = {
    [EXPR_KIND_CONST] = { .structural = integrate_constant_rule },
    [EXPR_KIND_VAR] = { .structural = integrate_var_rule },
    [EXPR_KIND_ADD] = { .structural = integrate_add_rule },
    [EXPR_KIND_SUB] = { .structural = integrate_sub_rule },
    [EXPR_KIND_NEG] = { .structural = integrate_neg_rule },
    [EXPR_KIND_MUL] = { .structural = integrate_mul_rule },
    [EXPR_KIND_DIV] = { .structural = integrate_div_rule },
    [EXPR_KIND_POW] = { .structural = integrate_pow_rule },
    [EXPR_KIND_POW_D] = { .structural = integrate_pow_d_rule },
    [EXPR_KIND_SQRT] = { .primitive = integrate_sqrt_rule },
    [EXPR_KIND_LOG] = { .primitive = integrate_log_rule },
    [EXPR_KIND_LOG10] = { .primitive = integrate_log10_rule },
    [EXPR_KIND_EXP] = { .primitive = integrate_exp_rule },
    [EXPR_KIND_SIN] = { .primitive = integrate_sin_rule },
    [EXPR_KIND_COS] = { .primitive = integrate_cos_rule },
    [EXPR_KIND_TAN] = { .primitive = integrate_tan_rule },
    [EXPR_KIND_SEC] = { .primitive = integrate_sec_rule },
    [EXPR_KIND_COSEC] = { .primitive = integrate_cosec_rule },
    [EXPR_KIND_COT] = { .primitive = integrate_cot_rule },
    [EXPR_KIND_SINH] = { .primitive = integrate_sinh_rule },
    [EXPR_KIND_COSH] = { .primitive = integrate_cosh_rule },
    [EXPR_KIND_COSECH] = { .primitive = integrate_cosech_rule },
    [EXPR_KIND_TANH] = { .primitive = integrate_tanh_rule },
    [EXPR_KIND_SECH] = { .primitive = integrate_sech_rule },
    [EXPR_KIND_COTH] = { .primitive = integrate_coth_rule },
    [EXPR_KIND_ASIN] = { .primitive = integrate_asin_rule },
    [EXPR_KIND_ACOS] = { .primitive = integrate_acos_rule },
    [EXPR_KIND_ATAN] = { .primitive = integrate_atan_rule },
    [EXPR_KIND_ASEC] = { .primitive = integrate_asec_rule },
    [EXPR_KIND_ACOSEC] = { .primitive = integrate_acosec_rule },
    [EXPR_KIND_ACOT] = { .primitive = integrate_acot_rule },
    [EXPR_KIND_ASINH] = { .primitive = integrate_asinh_rule },
    [EXPR_KIND_ACOSH] = { .primitive = integrate_acosh_rule },
    [EXPR_KIND_ATANH] = { .primitive = integrate_atanh_rule },
    [EXPR_KIND_ASECH] = { .primitive = integrate_asech_rule },
    [EXPR_KIND_ACOSECH] = { .primitive = integrate_acosech_rule },
    [EXPR_KIND_ACOTH] = { .primitive = integrate_acoth_rule },
    [EXPR_KIND_ERF] = { .primitive = integrate_erf_rule },
    [EXPR_KIND_ERFC] = { .primitive = integrate_erfc_rule },
    [EXPR_KIND_NORMAL_PDF] = { .primitive = integrate_normal_pdf_rule },
    [EXPR_KIND_NORMAL_CDF] = { .primitive = integrate_normal_cdf_rule },
    [EXPR_KIND_NORMAL_LOGPDF] = { .primitive = integrate_normal_logpdf_rule },
    [EXPR_KIND_EI] = { .primitive = integrate_ei_rule },
    [EXPR_KIND_E1] = { .primitive = integrate_e1_rule }
};

static const expr_integrate_dispatch_rule_t *integrate_dispatch_rule_for_kind(expr_op_kind_t kind)
{
    if ((unsigned)kind >= (unsigned)EXPR_KIND_COUNT)
        return NULL;
    return &integrate_dispatch_rules[kind];
}

expr_t *expr_integrate_dispatch_primitive(const expr_t *expr, const expr_t *wrt)
{
    const expr_integrate_dispatch_rule_t *rule;

    if (!expr || !expr->ops)
        return NULL;

    rule = integrate_dispatch_rule_for_kind(expr->ops->kind);
    return (rule && rule->primitive) ? rule->primitive(expr, wrt) : NULL;
}

expr_t *expr_integrate_dispatch(const expr_t *expr, const expr_t *wrt)
{
    const expr_integrate_dispatch_rule_t *rule;

    if (!expr || !wrt)
        return NULL;

    if (!depends_on_wrt(expr, wrt))
        return expr_integrate_as_constant(expr, wrt);

    rule = integrate_dispatch_rule_for_kind(expr->ops->kind);
    if (rule && rule->structural)
        return rule->structural(expr, wrt);

    if (expr->ops->integrate)
        return expr->ops->integrate(expr, wrt);

    /*
     * Exact subtree u-substitution needs stronger factor extraction and
     * equivalence checking before it is safe to enable as a general fallback.
     */
    return NULL;
}

expr_t *expr_integrate(const expr_t *expr, const expr_t *wrt)
{
    expr_t *simplified;
    expr_t *raw;

    if (!expr || !wrt || !expr_is_var(wrt))
        return NULL;

    simplified = expr_simplify(expr);
    if (!simplified)
        return NULL;

    raw = expr_integrate_dispatch(simplified, wrt);
    expr_free(simplified);
    return simplify_owned(raw);
}
