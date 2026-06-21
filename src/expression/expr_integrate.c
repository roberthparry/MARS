#include <stdlib.h>

#include "expr_internal.h"
#include "expr_integrate_internal.h"

typedef expr_t *(*expr_integrate_rule_fn)(const expr_t *expr, const expr_t *wrt);

typedef struct expr_integrate_dispatch_rule {
    expr_integrate_rule_fn structural;
    expr_integrate_rule_fn primitive;
} expr_integrate_dispatch_rule_t;

static const expr_integrate_dispatch_rule_t integrate_dispatch_rules[EXPR_KIND_COUNT] = {
    [EXPR_KIND_CONST]         = { .structural = integrate_constant_rule },
    [EXPR_KIND_VAR]           = { .structural = integrate_var_rule },
    [EXPR_KIND_ADD]           = { .structural = integrate_add_rule },
    [EXPR_KIND_SUB]           = { .structural = integrate_sub_rule },
    [EXPR_KIND_NEG]           = { .structural = integrate_neg_rule },
    [EXPR_KIND_MUL]           = { .structural = integrate_mul_rule },
    [EXPR_KIND_DIV]           = { .structural = integrate_div_rule },
    [EXPR_KIND_POW]           = { .structural = integrate_pow_rule },
    [EXPR_KIND_POW_D]         = { .structural = integrate_pow_d_rule },
    [EXPR_KIND_SQRT]          = { .primitive  = integrate_sqrt_rule },
    [EXPR_KIND_LOG]           = { .primitive  = integrate_log_rule },
    [EXPR_KIND_LOG10]         = { .primitive  = integrate_log10_rule },
    [EXPR_KIND_EXP]           = { .primitive  = integrate_exp_rule },
    [EXPR_KIND_SIN]           = { .primitive  = integrate_sin_rule },
    [EXPR_KIND_COS]           = { .primitive  = integrate_cos_rule },
    [EXPR_KIND_TAN]           = { .primitive  = integrate_tan_rule },
    [EXPR_KIND_SEC]           = { .primitive  = integrate_sec_rule },
    [EXPR_KIND_COSEC]         = { .primitive  = integrate_cosec_rule },
    [EXPR_KIND_COT]           = { .primitive  = integrate_cot_rule },
    [EXPR_KIND_SINH]          = { .primitive  = integrate_sinh_rule },
    [EXPR_KIND_COSH]          = { .primitive  = integrate_cosh_rule },
    [EXPR_KIND_COSECH]        = { .primitive  = integrate_cosech_rule },
    [EXPR_KIND_TANH]          = { .primitive  = integrate_tanh_rule },
    [EXPR_KIND_SECH]          = { .primitive  = integrate_sech_rule },
    [EXPR_KIND_COTH]          = { .primitive  = integrate_coth_rule },
    [EXPR_KIND_ASIN]          = { .primitive  = integrate_asin_rule },
    [EXPR_KIND_ACOS]          = { .primitive  = integrate_acos_rule },
    [EXPR_KIND_ATAN]          = { .primitive  = integrate_atan_rule },
    [EXPR_KIND_ASEC]          = { .primitive  = integrate_asec_rule },
    [EXPR_KIND_ACOSEC]        = { .primitive  = integrate_acosec_rule },
    [EXPR_KIND_ACOT]          = { .primitive  = integrate_acot_rule },
    [EXPR_KIND_ASINH]         = { .primitive  = integrate_asinh_rule },
    [EXPR_KIND_ACOSH]         = { .primitive  = integrate_acosh_rule },
    [EXPR_KIND_ATANH]         = { .primitive  = integrate_atanh_rule },
    [EXPR_KIND_ASECH]         = { .primitive  = integrate_asech_rule },
    [EXPR_KIND_ACOSECH]       = { .primitive  = integrate_acosech_rule },
    [EXPR_KIND_ACOTH]         = { .primitive  = integrate_acoth_rule },
    [EXPR_KIND_ERF]           = { .primitive  = integrate_erf_rule },
    [EXPR_KIND_ERFC]          = { .primitive  = integrate_erfc_rule },
    [EXPR_KIND_NORMAL_PDF]    = { .primitive  = integrate_normal_pdf_rule },
    [EXPR_KIND_NORMAL_CDF]    = { .primitive  = integrate_normal_cdf_rule },
    [EXPR_KIND_NORMAL_LOGPDF] = { .primitive  = integrate_normal_logpdf_rule },
    [EXPR_KIND_EI]            = { .primitive  = integrate_ei_rule },
    [EXPR_KIND_E1]            = { .primitive  = integrate_e1_rule }
};

static const expr_integrate_dispatch_rule_t *integrate_dispatch_rule_for_kind(expr_op_kind_t kind)
{
    if ((unsigned)kind >= (unsigned)EXPR_KIND_COUNT)
        return NULL;
    return &integrate_dispatch_rules[kind];
}

static int expr_var_matches_local(const expr_t *left, const expr_t *right)
{
    if (!left || !right || !expr_is_var(left) || !expr_is_var(right))
        return 0;
    return left == right ||
           (left->var_id != 0u && left->var_id == right->var_id);
}

static int expr_is_bound_in_var_list_local(const expr_t *expr,
                                           size_t nvars,
                                           expr_t *const *vars)
{
    size_t i;

    if (!expr || !expr_is_var(expr) || !vars)
        return 0;

    for (i = 0u; i < nvars; ++i) {
        if (expr_var_matches_local(expr, vars[i]))
            return 1;
    }
    return 0;
}

static expr_t *expr_apply_symbolic_bound_step_local(expr_t *anti,
                                                    expr_integration_bound_kind_t kind,
                                                    expr_t *var,
                                                    expr_t *lo,
                                                    expr_t *hi)
{
    expr_t *upper = NULL;
    expr_t *lower = NULL;
    expr_t *diff = NULL;
    expr_t *next = NULL;

    if (!anti || !var)
        return NULL;

    if (kind == EXPR_INTEGRATION_BOUND_DEFINITE) {
        if (!lo || !hi)
            return NULL;
        upper = expr_substitute(anti, var, hi);
        lower = expr_substitute(anti, var, lo);
        diff = (upper && lower) ? expr_sub(upper, lower) : NULL;
        next = diff ? expr_simplify(diff) : NULL;
    } else if (kind == EXPR_INTEGRATION_BOUND_UPPER_ONLY) {
        if (!hi)
            return NULL;
        upper = expr_substitute(anti, var, hi);
        next = upper ? expr_simplify(upper) : NULL;
    } else {
        next = expr_simplify(anti);
    }

    expr_free(diff);
    expr_free(lower);
    expr_free(upper);
    return next;
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

bool expr_has_unbound_parameters(const expr_t *expr,
                                 size_t nvars,
                                 expr_t *const *vars)
{
    if (!expr)
        return false;
    if ((expr_is_var(expr) || expr_is_const(expr)) &&
        !expr_is_bound_in_var_list_local(expr, nvars, vars) &&
        num_is_nan(expr->c)) {
        return true;
    }
    return expr_has_unbound_parameters(expr->a, nvars, vars) ||
           expr_has_unbound_parameters(expr->b, nvars, vars);
}

expr_t *expr_integrate_iterated(const expr_t *integrand,
                                size_t ndim,
                                expr_t *const *vars,
                                const expr_integration_bound_kind_t *kinds,
                                expr_t *const *lo,
                                expr_t *const *hi,
                                size_t max_steps,
                                size_t *completed_steps_out,
                                expr_t **first_antiderivative_out)
{
    expr_t *current = NULL;
    size_t completed_steps = 0u;

    if (!integrand || !vars || !kinds || !lo || !hi)
        return NULL;

    if (completed_steps_out)
        *completed_steps_out = 0u;
    if (first_antiderivative_out)
        *first_antiderivative_out = NULL;

    current = expr_simplify(integrand);
    if (!current)
        return NULL;

    for (size_t i = 0; i < ndim && i < max_steps; ++i) {
        expr_t *anti;
        expr_t *next;

        anti = expr_integrate(current, vars[i]);
        if (!anti) {
            expr_free(current);
            return NULL;
        }

        if (i == 0u && first_antiderivative_out) {
            expr_retain(anti);
            *first_antiderivative_out = anti;
        }

        next = expr_apply_symbolic_bound_step_local(anti, kinds[i], vars[i], lo[i], hi[i]);
        expr_free(anti);
        expr_free(current);

        if (!next) {
            if (first_antiderivative_out) {
                expr_free(*first_antiderivative_out);
                *first_antiderivative_out = NULL;
            }
            return NULL;
        }
        current = next;
        completed_steps = i + 1u;
    }

    if (completed_steps_out)
        *completed_steps_out = completed_steps;
    return current;
}

expr_t *expr_integrate_iterated_best_effort(
    const expr_t *integrand,
    size_t ndim,
    expr_t *const *vars,
    const expr_integration_bound_kind_t *kinds,
    expr_t *const *lo,
    expr_t *const *hi,
    size_t *completed_steps_out,
    size_t *remaining_ndim_out,
    expr_t **remaining_vars_out,
    number_t *remaining_lo_num_out,
    number_t *remaining_hi_num_out,
    const number_t *lo_num,
    const number_t *hi_num)
{
    expr_t *current = NULL;
    int *active = NULL;
    size_t remaining = ndim;
    size_t completed = 0u;

    if (!integrand || !vars || !kinds || !lo || !hi || !completed_steps_out ||
        !remaining_ndim_out || !remaining_vars_out ||
        (ndim > 0u && (!remaining_lo_num_out || !remaining_hi_num_out ||
                       !lo_num || !hi_num)))
        return NULL;

    *completed_steps_out = 0u;
    *remaining_ndim_out = 0u;

    current = expr_simplify(integrand);
    if (!current)
        return NULL;

    active = calloc(ndim, sizeof(*active));
    if (!active) {
        expr_free(current);
        return NULL;
    }

    for (size_t i = 0; i < ndim; ++i)
        active[i] = 1;

    while (remaining > 0u) {
        int progressed = 0;

        for (size_t i = ndim; i-- > 0u;) {
            expr_t *anti;
            expr_t *next;

            if (!active[i])
                continue;

            anti = expr_integrate(current, vars[i]);
            if (!anti)
                continue;

            next = expr_apply_symbolic_bound_step_local(anti, kinds[i], vars[i], lo[i], hi[i]);
            expr_free(anti);
            if (!next)
                continue;

            expr_free(current);
            current = next;
            active[i] = 0;
            remaining--;
            completed++;
            progressed = 1;
            break;
        }

        if (!progressed)
            break;
    }

    for (size_t i = 0, out = 0u; i < ndim; ++i) {
        if (!active[i])
            continue;
        remaining_vars_out[out] = vars[i];
        num_destroy(&remaining_lo_num_out[out]);
        remaining_lo_num_out[out] = num_clone(lo_num[i]);
        num_destroy(&remaining_hi_num_out[out]);
        remaining_hi_num_out[out] = num_clone(hi_num[i]);
        out++;
    }

    free(active);
    *completed_steps_out = completed;
    *remaining_ndim_out = remaining;
    return current;
}
