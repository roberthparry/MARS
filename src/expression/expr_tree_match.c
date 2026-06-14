#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "expr_internal.h"
#include "expr_bindings.h"
#include "expression.h"
#include "internal/expr_internal.h"
#include "internal/number_internal.h"

static bool expr_is_op_kind(const expr_t *expr, expr_op_kind_t kind)
{
    return expr && expr->ops && expr->ops->kind == kind;
}

bool expr_is_exact_zero(const expr_t *dv)
{
    return expr_is_op_kind(dv, EXPR_KIND_CONST) &&
           !dv->name &&
           num_eq(dv->c, NUM_ZERO);
}

bool expr_is_named_const(const expr_t *dv)
{
    return expr_is_op_kind(dv, EXPR_KIND_CONST) && dv->name && *dv->name;
}

static bool expr_match_const_leaf(const expr_t *expr, number_t *value_out, const char **name_out)
{
    if (!expr_is_op_kind(expr, EXPR_KIND_CONST))
        return false;
    if (value_out) {
        num_destroy(value_out);
        *value_out = num_clone(expr->c);
    }
    if (name_out)
        *name_out = (expr->name && *expr->name) ? expr->name : NULL;
    return true;
}

static bool expr_match_var_leaf(const expr_t *expr, number_t *value_out, const char **name_out)
{
    if (!expr_is_op_kind(expr, EXPR_KIND_VAR))
        return false;
    if (value_out) {
        num_destroy(value_out);
        *value_out = num_clone(expr->c);
    }
    if (name_out)
        *name_out = (expr->name && *expr->name) ? expr->name : NULL;
    return true;
}

static bool expr_is_named_leaf(const expr_t *expr)
{
    return expr &&
           (expr_is_var(expr) || expr_is_const(expr)) &&
           expr->name &&
           *expr->name;
}

static bool expr_has_composite_preserved_binding_leaf(const expr_t *expr)
{
    return expr_is_const(expr) &&
           expr->binding_expr &&
           expr->binding_expr->kind != EXPR_BINDING_EXPR_NUMBER &&
           expr->binding_expr->kind != EXPR_BINDING_EXPR_CONST;
}

static bool expr_is_same_named_leaf_for_substitution(const expr_t *expr,
                                                     const expr_t *needle)
{
    return expr &&
           needle &&
           expr != needle &&
           expr_is_var(needle) &&
           expr_is_named_leaf(expr) &&
           needle->name &&
           *needle->name &&
           strcmp(expr->name, needle->name) == 0;
}

static bool expr_match_unnamed_const_leaf(const expr_t *expr, number_t *value_out)
{
    if (!value_out || !expr_is_op_kind(expr, EXPR_KIND_CONST))
        return false;
    if (expr->name && *expr->name)
        return false;
    num_destroy(value_out);
    *value_out = num_clone(expr->c);
    return true;
}

bool expr_match_unary_op(const expr_t *expr, expr_op_kind_t kind, const expr_t **arg_out)
{
    if (!expr_is_op_kind(expr, kind) || !expr->a)
        return false;
    if (arg_out)
        *arg_out = expr->a;
    return true;
}

bool expr_match_binary_op(const expr_t *expr,
                        expr_op_kind_t kind,
                        const expr_t **left_out,
                        const expr_t **right_out)
{
    if (!expr_is_op_kind(expr, kind) || !expr->a || !expr->b)
        return false;
    if (left_out)
        *left_out = expr->a;
    if (right_out)
        *right_out = expr->b;
    return true;
}

static bool expr_match_scaled_inner(const expr_t *factor,
                                  const expr_t *other,
                                  number_t *scale_out,
                                  const expr_t **base_out)
{
    NUM_SCOPE(scope);
    number_t factor_value = num_new();
    number_t inner_scale = num_new();
    const expr_t *inner_base;

    if (!expr_is_unnamed_const(factor) || !expr_match_const_value(factor, &factor_value))
        return false;
    if (expr_match_scaled_expr(other, &inner_scale, &inner_base)) {
        num_destroy(scale_out);
        *scale_out = num_scope_detach(num_mul(factor_value, inner_scale));
        *base_out = inner_base;
        return true;
    }
    num_destroy(scale_out);
    *scale_out = num_scope_detach(factor_value);
    *base_out = other;
    return true;
}

static int expr_match_var_index(size_t nvars, expr_t *const *vars, const expr_t *dv)
{
    for (size_t i = 0; i < nvars; ++i)
        if (vars[i] == dv ||
            (expr_is_var(vars[i]) && expr_is_var(dv) &&
             vars[i]->var_id != 0 && vars[i]->var_id == dv->var_id))
            return (int)i;
    return -1;
}

bool expr_match_var_expr(const expr_t *expr,
                       size_t nvars,
                       expr_t *const *vars,
                       size_t *index_out)
{
    int idx;

    if (!expr || !vars || !index_out)
        return false;

    idx = expr_match_var_index(nvars, vars, expr);
    if (idx < 0)
        return false;

    *index_out = (size_t)idx;
    return true;
}

bool expr_match_const_value(const expr_t *expr, number_t *value_out)
{
    return expr_match_unnamed_const_leaf(expr, value_out);
}

bool expr_match_scaled_expr(const expr_t *expr,
                          number_t *scale_out,
                          const expr_t **base_out)
{
    NUM_SCOPE(scope);
    number_t inner_scale;
    number_t const_value = num_new();
    const expr_t *inner_base;
    const expr_t *left;
    const expr_t *right;
    const expr_t *arg;

    if (!expr || !scale_out || !base_out)
        return false;

    if (expr_match_unary_op(expr, EXPR_KIND_NEG, &arg)) {
        inner_scale = num_new();
        if (expr_match_scaled_expr(arg, &inner_scale, &inner_base)) {
            num_destroy(scale_out);
            *scale_out = num_scope_detach(num_neg(inner_scale));
            *base_out = inner_base;
            return true;
        }
        num_destroy(scale_out);
        *scale_out = num_scope_detach(num_clone(NUM_NEG_ONE));
        *base_out = arg;
        return true;
    }

    if (expr_match_binary_op(expr, EXPR_KIND_MUL, &left, &right)) {
        if (expr_match_scaled_inner(left, right, scale_out, base_out) ||
            expr_match_scaled_inner(right, left, scale_out, base_out))
            return true;
    }

    if (expr_match_binary_op(expr, EXPR_KIND_DIV, &left, &right) &&
        expr_match_unnamed_const_leaf(right, &const_value)) {
        if (num_eq(const_value, NUM_ZERO))
            return false;
        inner_scale = num_new();
        if (expr_match_scaled_expr(left, &inner_scale, &inner_base)) {
            num_destroy(scale_out);
            *scale_out = num_scope_detach(num_div(inner_scale, const_value));
            *base_out = inner_base;
            return true;
        }
        num_destroy(scale_out);
        *scale_out = num_scope_detach(num_div(NUM_ONE, const_value));
        *base_out = left;
        return true;
    }

    return false;
}

bool expr_match_add_sub_expr(const expr_t *expr,
                           const expr_t **left_out,
                           const expr_t **right_out,
                           bool *is_sub_out)
{
    if (!expr || !left_out || !right_out || !is_sub_out)
        return false;
    if (expr_match_binary_op(expr, EXPR_KIND_ADD, left_out, right_out)) {
        *is_sub_out = false;
        return true;
    }
    if (expr_match_binary_op(expr, EXPR_KIND_SUB, left_out, right_out)) {
        *is_sub_out = true;
        return true;
    }
    return false;
}

bool expr_match_mul_expr(const expr_t *expr,
                       const expr_t **left_out,
                       const expr_t **right_out)
{
    if (!expr || !left_out || !right_out)
        return false;
    if (!expr_match_binary_op(expr, EXPR_KIND_MUL, left_out, right_out))
        return false;
    return true;
}

static bool expr_collect_var_usage_impl(const expr_t *expr,
                                      size_t nvars,
                                      expr_t *const *vars,
                                      bool *used_out)
{
    size_t idx;

    if (!expr)
        return true;

    if (expr_match_var_expr(expr, nvars, vars, &idx)) {
        used_out[idx] = true;
        return true;
    }

    if (expr->a && !expr_collect_var_usage_impl(expr->a, nvars, vars, used_out))
        return false;
    if (expr->b && !expr_collect_var_usage_impl(expr->b, nvars, vars, used_out))
        return false;
    return true;
}

bool expr_collect_var_usage(const expr_t *expr,
                          size_t nvars,
                          expr_t *const *vars,
                          bool *used_out)
{
    if (!used_out || (nvars > 0 && !vars))
        return false;
    for (size_t i = 0; i < nvars; ++i)
        used_out[i] = false;
    return expr_collect_var_usage_impl(expr, nvars, vars, used_out);
}

expr_t *expr_substitute(const expr_t *expr,
                      const expr_t *needle,
                      expr_t *replacement)
{
    expr_t *left;
    expr_t *right;
    expr_t *out;
    const char *name;
    const expr_t *arg;

    if (!expr)
        return NULL;

    if (expr == needle || expr_is_same_named_leaf_for_substitution(expr, needle)) {
        expr_retain(replacement);
        return replacement;
    }

    if (expr_has_composite_preserved_binding_leaf(expr)) {
        expr_t *expanded = expr_binding_expr_eval_expr(expr->binding_expr);

        if (!expanded)
            return NULL;
        out = expr_substitute(expanded, needle, replacement);
        expr_free(expanded);
        return out;
    }

    if (expr_match_const_leaf(expr, NULL, &name)) {
        (void)name;
        return expr_clone(expr);
    }

    if (expr_match_var_leaf(expr, NULL, &name)) {
        (void)name;
        return expr_clone(expr);
    }

    if (expr_match_unary_op(expr, EXPR_KIND_POW_D, &arg)) {
        left = expr_substitute(arg, needle, replacement);
        if (!left)
            return NULL;
        out = expr_pow(left, &expr->c);
        expr_free(left);
        return out;
    }

    if (expr->ops->arity == EXPR_OP_UNARY && expr->ops->apply_unary) {
        left = expr_substitute(expr->a, needle, replacement);
        if (!left)
            return NULL;
        out = expr->ops->apply_unary(left);
        expr_free(left);
        return out;
    }

    if (expr->ops->arity == EXPR_OP_BINARY && expr->ops->apply_binary) {
        left = expr_substitute(expr->a, needle, replacement);
        right = expr_substitute(expr->b, needle, replacement);
        if (!left || !right) {
            expr_free(left);
            expr_free(right);
            return NULL;
        }
        out = expr->ops->apply_binary(left, right);
        expr_free(left);
        expr_free(right);
        return out;
    }

    return NULL;
}
