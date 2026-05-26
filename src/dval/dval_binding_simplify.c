#include "dval_binding_simplify.h"

/* Binding rules consume and return the owned node. A rule that does not match
 * must return the same node untouched. */
typedef dv_binding_expr_t *(*binding_simplify_rule_fn)(dv_binding_expr_t *expr);

typedef struct {
    binding_simplify_rule_fn apply;
} binding_simplify_rule_t;

static dv_binding_expr_t *binding_apply_kind_rules(
    dv_binding_expr_t *expr,
    dv_binding_expr_kind_t kind,
    const binding_simplify_rule_t *rules,
    size_t n)
{
    size_t i;

    if (!expr || expr->kind != kind)
        return expr;

    for (i = 0u; i < n; ++i) {
        expr = rules[i].apply(expr);
        if (!expr || expr->kind != kind)
            return expr;
    }

    return expr;
}

static dv_binding_expr_t *binding_apply_addsub_rules(
    dv_binding_expr_t *expr,
    const binding_simplify_rule_t *rules,
    size_t n)
{
    size_t i;

    if (!expr || (expr->kind != DV_BINDING_EXPR_ADD &&
                  expr->kind != DV_BINDING_EXPR_SUB))
        return expr;

    for (i = 0u; i < n; ++i) {
        expr = rules[i].apply(expr);
        if (!expr || (expr->kind != DV_BINDING_EXPR_ADD &&
                      expr->kind != DV_BINDING_EXPR_SUB))
            return expr;
    }

    return expr;
}

static void binding_simplify_binary_children(dv_binding_expr_t *expr)
{
    if (!expr)
        return;
    expr->u.binary.left = dv_binding_expr_simplify(expr->u.binary.left);
    expr->u.binary.right = dv_binding_expr_simplify(expr->u.binary.right);
}

static void binding_simplify_binary_op_children(dv_binding_expr_t *expr)
{
    if (!expr)
        return;
    expr->u.binary_op.left = dv_binding_expr_simplify(expr->u.binary_op.left);
    expr->u.binary_op.right = dv_binding_expr_simplify(expr->u.binary_op.right);
}

static const binding_simplify_rule_t s_binding_addsub_rules[] = {
    { binding_expr_try_fold_exact_complex_owned },
    { binding_expr_try_fold_number_owned },
    { binding_expr_try_simplify_basic_sum },
    { binding_expr_try_simplify_trig_sum }
};

static const binding_simplify_rule_t s_binding_neg_rules[] = {
    { binding_expr_try_fold_number_owned },
    { binding_expr_try_fold_neg_leading_number }
};

static const binding_simplify_rule_t s_binding_mul_rules[] = {
    { binding_expr_try_fold_exact_complex_owned },
    { binding_expr_try_fold_number_owned },
    { binding_expr_try_fold_mul_leading_numbers },
    { binding_expr_try_simplify_basic_product },
    { binding_expr_try_simplify_i_unit_product },
    { binding_expr_try_simplify_lambert_product },
    { binding_expr_try_simplify_trig_product },
    { binding_expr_try_combine_mul_powers }
};

static const binding_simplify_rule_t s_binding_div_rules[] = {
    { binding_expr_try_fold_exact_complex_owned },
    { binding_expr_try_fold_number_owned },
    { binding_expr_try_simplify_basic_quotient },
    { binding_expr_try_fold_div_leading_number }
};

static const binding_simplify_rule_t s_binding_unary_exact_rules[] = {
    { binding_expr_try_simplify_direct_inverse },
    { binding_expr_try_simplify_lambert_inverse },
    { binding_expr_try_simplify_complex_floor_ceil },
    { binding_expr_try_simplify_log_e },
    { binding_expr_try_simplify_log10_power },
    { binding_expr_try_simplify_imag_trig_bridge },
    { binding_expr_try_simplify_trig_exact }
};

static const binding_simplify_rule_t s_binding_binary_op_rules[] = {
    { binding_expr_try_simplify_logbeta_integers }
};

static dv_binding_expr_t *binding_try_simplify_unary_exact(dv_binding_expr_t *expr)
{
    return binding_apply_kind_rules(
        expr,
        DV_BINDING_EXPR_UNARY_OP,
        s_binding_unary_exact_rules,
        sizeof(s_binding_unary_exact_rules) / sizeof(s_binding_unary_exact_rules[0]));
}

dv_binding_expr_t *dv_binding_simplify_atom(dv_binding_expr_t *expr)
{
    return expr;
}

dv_binding_expr_t *dv_binding_simplify_neg(dv_binding_expr_t *expr)
{
    if (!expr)
        return NULL;

    expr->u.unary.child = dv_binding_expr_simplify(expr->u.unary.child);
    return binding_apply_kind_rules(
        expr,
        DV_BINDING_EXPR_NEG,
        s_binding_neg_rules,
        sizeof(s_binding_neg_rules) / sizeof(s_binding_neg_rules[0]));
}

dv_binding_expr_t *dv_binding_simplify_addsub(dv_binding_expr_t *expr)
{
    binding_simplify_binary_children(expr);
    return binding_apply_addsub_rules(
        expr,
        s_binding_addsub_rules,
        sizeof(s_binding_addsub_rules) / sizeof(s_binding_addsub_rules[0]));
}

dv_binding_expr_t *dv_binding_simplify_mul(dv_binding_expr_t *expr)
{
    if (!expr)
        return NULL;

    binding_simplify_binary_children(expr);
    return binding_apply_kind_rules(
        expr,
        DV_BINDING_EXPR_MUL,
        s_binding_mul_rules,
        sizeof(s_binding_mul_rules) / sizeof(s_binding_mul_rules[0]));
}

dv_binding_expr_t *dv_binding_simplify_div(dv_binding_expr_t *expr)
{
    if (!expr)
        return NULL;

    binding_simplify_binary_children(expr);
    return binding_apply_kind_rules(
        expr,
        DV_BINDING_EXPR_DIV,
        s_binding_div_rules,
        sizeof(s_binding_div_rules) / sizeof(s_binding_div_rules[0]));
}

dv_binding_expr_t *dv_binding_simplify_powi(dv_binding_expr_t *expr)
{
    if (!expr)
        return NULL;

    expr->u.powi.base = dv_binding_expr_simplify(expr->u.powi.base);
    if (expr->u.powi.exponent == 0)
        return binding_expr_fold_to_number_owned(expr, num_clone(NUM_ONE));
    if (expr->u.powi.exponent == 1) {
        dv_binding_expr_t *base = expr->u.powi.base;

        expr->u.powi.base = NULL;
        dv_binding_expr_free(expr);
        return base;
    }
    if (expr->u.powi.exponent == 2 &&
        binding_expr_is_const_id(expr->u.powi.base, DV_BINDING_CONST_I))
        return binding_expr_fold_to_number_owned(expr, num_clone(NUM_NEG_ONE));
    return binding_expr_try_fold_number_owned(expr);
}

dv_binding_expr_t *dv_binding_simplify_unary_op(dv_binding_expr_t *expr)
{
    if (!expr)
        return NULL;

    expr = binding_expr_try_simplify_principal_inverse(expr);
    if (!expr || expr->kind != DV_BINDING_EXPR_UNARY_OP)
        return expr;
    expr->u.unary_op.child = dv_binding_expr_simplify(expr->u.unary_op.child);
    return binding_try_simplify_unary_exact(expr);
}

dv_binding_expr_t *dv_binding_simplify_binary_op(dv_binding_expr_t *expr)
{
    binding_simplify_binary_op_children(expr);
    return binding_apply_kind_rules(
        expr,
        DV_BINDING_EXPR_BINARY_OP,
        s_binding_binary_op_rules,
        sizeof(s_binding_binary_op_rules) / sizeof(s_binding_binary_op_rules[0]));
}
