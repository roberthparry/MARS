#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_bindings.h"
#include "expr_internal.h"
#include "expression.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"

static bool expr_is_op_kind(const expr_t *expr, expr_op_kind_t kind)
{
    return expr && expr->ops && expr->ops->kind == kind;
}

bool expr_is_exact_zero(const expr_t *dv)
{
    return expr_is_op_kind(dv, EXPR_KIND_CONST) && !dv->name && num_eq(dv->c, NUM_ZERO);
}

bool expr_is_named_const(const expr_t *dv)
{
    return expr_is_op_kind(dv, EXPR_KIND_CONST) && dv->name && *dv->name;
}

/* Report whether an expression is a formal summation. */
bool expr_is_summation(const expr_t *expr)
{
    return expr_is_op_kind(expr, EXPR_KIND_SUMMATION);
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
    return expr && (expr_is_var(expr) || expr_is_const(expr)) && expr->name && *expr->name;
}

static bool expr_has_composite_preserved_binding_leaf(const expr_t *expr)
{
    return expr_is_const(expr) && expr->binding_expr && expr->binding_expr->kind != EXPR_BINDING_EXPR_NUMBER &&
           expr->binding_expr->kind != EXPR_BINDING_EXPR_CONST && !expr_binding_expr_is_array(expr->binding_expr);
}

static bool expr_is_same_named_leaf_for_substitution(const expr_t *expr, const expr_t *needle)
{
    return expr && needle && expr != needle && expr_is_var(needle) && expr_is_named_leaf(expr) && needle->name &&
           *needle->name && strcmp(expr->name, needle->name) == 0;
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

bool expr_match_binary_op(const expr_t *expr, expr_op_kind_t kind, const expr_t **left_out, const expr_t **right_out)
{
    if (!expr_is_op_kind(expr, kind) || !expr->a || !expr->b)
        return false;
    if (left_out)
        *left_out = expr->a;
    if (right_out)
        *right_out = expr->b;
    return true;
}

bool expr_match_neg_expr(const expr_t *expr, const expr_t **arg_out)
{
    return expr_match_unary_op(expr, EXPR_KIND_NEG, arg_out);
}

bool expr_match_unary_expr(const expr_t *expr, const expr_t **arg_out)
{
    if (!expr || !expr->ops || expr->ops->arity != EXPR_OP_UNARY || !expr->a)
        return false;
    if (arg_out)
        *arg_out = expr->a;
    return true;
}

bool expr_match_reapplicable_unary_function(const expr_t *expr, const expr_t **arg_out)
{
    if (!expr_match_unary_expr(expr, arg_out) || expr->ops == &ops_neg || !expr->ops->apply_unary)
        return false;
    return true;
}

bool expr_match_same_reapplicable_unary_function(const expr_t *expr, const expr_t *prototype,
                                                 const expr_t **arg_out)
{
    return expr_match_reapplicable_unary_function(expr, arg_out) && prototype && prototype->ops == expr->ops;
}

expr_t *expr_apply_reapplicable_unary_function(const expr_t *prototype, const expr_t *argument)
{
    if (!prototype || !prototype->ops || !prototype->ops->apply_unary || prototype->ops == &ops_neg || !argument)
        return NULL;
    return prototype->ops->apply_unary(argument);
}

bool expr_match_exp_expr(const expr_t *expr, const expr_t **arg_out)
{
    return expr_match_unary_op(expr, EXPR_KIND_EXP, arg_out);
}

bool expr_match_log_expr(const expr_t *expr, const expr_t **arg_out)
{
    return expr_match_unary_op(expr, EXPR_KIND_LOG, arg_out);
}

bool expr_match_sin_expr(const expr_t *expr, const expr_t **arg_out)
{
    return expr_match_unary_op(expr, EXPR_KIND_SIN, arg_out);
}

bool expr_match_cos_expr(const expr_t *expr, const expr_t **arg_out)
{
    return expr_match_unary_op(expr, EXPR_KIND_COS, arg_out);
}

bool expr_match_tan_expr(const expr_t *expr, const expr_t **arg_out)
{
    return expr_match_unary_op(expr, EXPR_KIND_TAN, arg_out);
}

bool expr_match_cot_expr(const expr_t *expr, const expr_t **arg_out)
{
    return expr_match_unary_op(expr, EXPR_KIND_COT, arg_out);
}

bool expr_match_add_expr(const expr_t *expr, const expr_t **left_out, const expr_t **right_out)
{
    return expr_match_binary_op(expr, EXPR_KIND_ADD, left_out, right_out);
}

bool expr_match_sub_expr(const expr_t *expr, const expr_t **left_out, const expr_t **right_out)
{
    return expr_match_binary_op(expr, EXPR_KIND_SUB, left_out, right_out);
}

bool expr_match_pow_const(const expr_t *expr, const expr_t **base_out, number_t *exponent_out)
{
    if (!expr_is_op_kind(expr, EXPR_KIND_POW_D) || !expr->a)
        return false;
    if (base_out)
        *base_out = expr->a;
    if (exponent_out) {
        num_destroy(exponent_out);
        *exponent_out = num_clone(expr->c);
    }
    return true;
}

bool expr_match_pow_expr(const expr_t *expr, const expr_t **base_out, const expr_t **exponent_out)
{
    if (!expr_is_op_kind(expr, EXPR_KIND_POW) || !expr->a || !expr->b)
        return false;
    if (base_out)
        *base_out = expr->a;
    if (exponent_out)
        *exponent_out = expr->b;
    return true;
}

bool expr_match_integral_expr(const expr_t *expr, const expr_t **integrand_out, const expr_t **domain_out)
{
    if (!expr_is_op_kind(expr, EXPR_KIND_INTEGRAL) || !expr->a || !expr->b)
        return false;
    if (integrand_out)
        *integrand_out = expr->a;
    if (domain_out)
        *domain_out = expr->b;
    return true;
}

bool expr_child_exprs(const expr_t *expr, const expr_t **left_out, const expr_t **right_out)
{
    if (!expr)
        return false;
    if (left_out)
        *left_out = expr->a;
    if (right_out)
        *right_out = expr->b;
    return expr->a || expr->b;
}

const expr_t *expr_integral_dummy_expr(const expr_t *integral)
{
    if (!integral || !integral->b || !expr_is_integral_meta(integral->b))
        return NULL;
    return integral->b->b;
}

const expr_t *expr_integral_upper_bound_expr(const expr_t *integral)
{
    const expr_t *domain;

    if (!integral || !integral->b)
        return NULL;
    if (!expr_is_integral_meta(integral->b))
        return expr_is_integral_bounds(integral->b) ? integral->b->b : integral->b;

    domain = integral->b->a;
    if (!domain)
        return NULL;
    return expr_is_integral_bounds(domain) ? domain->b : domain;
}

const expr_t *expr_integral_lower_bound_expr(const expr_t *integral)
{
    const expr_t *domain;

    if (!integral || !integral->b)
        return NULL;
    if (expr_is_integral_bounds(integral->b))
        return integral->b->a;
    if (!expr_is_integral_meta(integral->b))
        return NULL;

    domain = integral->b->a;
    if (!domain || !expr_is_integral_bounds(domain))
        return NULL;
    return domain->a;
}

bool expr_contains_integral_operation(const expr_t *expr)
{
    if (!expr)
        return false;
    if (expr_is_op(expr, &ops_integral))
        return true;
    return expr_contains_integral_operation(expr->a) || expr_contains_integral_operation(expr->b);
}

const char *expr_symbol_name(const expr_t *expr)
{
    return (expr && expr->name && *expr->name) ? expr->name : NULL;
}

bool expr_is_variable(const expr_t *expr)
{
    return expr_is_var(expr);
}

static bool expr_match_scaled_inner(const expr_t *factor, const expr_t *other, number_t *scale_out,
                                    const expr_t **base_out)
{
    NUM_SCOPE(scope);
    number_t factor_value = num_new();
    number_t inner_scale = num_new();
    const expr_t *inner_base;

    if (!expr_is_unnamed_const(factor) || !expr_match_const_value(factor, &factor_value))
        return false;
    if (expr_match_scaled_expr(other, &inner_scale, &inner_base)) {
        number_t scale = num_mul(factor_value, inner_scale);

        num_destroy(&inner_scale);
        num_destroy(scale_out);
        *scale_out = num_scope_detach(scale);
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
            (expr_is_var(vars[i]) && expr_is_var(dv) && vars[i]->var_id != 0 && vars[i]->var_id == dv->var_id))
            return (int)i;
    return -1;
}

bool expr_match_var_expr(const expr_t *expr, size_t nvars, expr_t *const *vars, size_t *index_out)
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

bool expr_match_scaled_expr(const expr_t *expr, number_t *scale_out, const expr_t **base_out)
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
            number_t scale = num_neg(inner_scale);

            num_destroy(&inner_scale);
            num_destroy(scale_out);
            *scale_out = num_scope_detach(scale);
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
            number_t scale = num_div(inner_scale, const_value);

            num_destroy(&inner_scale);
            num_destroy(scale_out);
            *scale_out = num_scope_detach(scale);
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

bool expr_match_add_sub_expr(const expr_t *expr, const expr_t **left_out, const expr_t **right_out, bool *is_sub_out)
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

bool expr_match_mul_expr(const expr_t *expr, const expr_t **left_out, const expr_t **right_out)
{
    if (!expr || !left_out || !right_out)
        return false;
    if (!expr_match_binary_op(expr, EXPR_KIND_MUL, left_out, right_out))
        return false;
    return true;
}

bool expr_match_div_expr(const expr_t *expr, const expr_t **left_out, const expr_t **right_out)
{
    if (!expr || !left_out || !right_out)
        return false;
    return expr_match_binary_op(expr, EXPR_KIND_DIV, left_out, right_out);
}

static bool expr_collect_var_usage_impl(const expr_t *expr, size_t nvars, expr_t *const *vars, bool *used_out)
{
    size_t idx;

    if (!expr)
        return true;

    if (expr_match_var_expr(expr, nvars, vars, &idx)) {
        used_out[idx] = true;
        return true;
    }

    if (expr_is_op(expr, &ops_integral)) {
        const expr_t *dummy = expr_integral_dummy_expr(expr);
        const expr_t *lower = expr_integral_lower_bound_expr(expr);
        const expr_t *upper = expr_integral_upper_bound_expr(expr);

        if (lower && !expr_collect_var_usage_impl(lower, nvars, vars, used_out))
            return false;
        if (upper && !expr_collect_var_usage_impl(upper, nvars, vars, used_out))
            return false;
        if (expr->a) {
            expr_t *const *filtered_vars = vars;
            size_t filtered_nvars = nvars;

            if (dummy && nvars > 0u) {
                size_t out = 0u;
                expr_t *stack_vars[16];
                expr_t **filtered_storage = NULL;

                filtered_storage = (nvars <= 16u) ? stack_vars : calloc(nvars, sizeof(*filtered_storage));
                if (!filtered_storage)
                    return false;
                filtered_vars = filtered_storage;

                for (size_t i = 0; i < nvars; ++i) {
                    int same_dummy = vars[i] == dummy || (expr_is_var(vars[i]) && expr_is_var(dummy) &&
                                                          vars[i]->var_id != 0 && vars[i]->var_id == dummy->var_id);
                    if (!same_dummy)
                        filtered_storage[out++] = vars[i];
                }
                filtered_nvars = out;
                if (!expr_collect_var_usage_impl(expr->a, filtered_nvars, filtered_vars, used_out)) {
                    if (filtered_storage != stack_vars)
                        free(filtered_storage);
                    return false;
                }
                if (filtered_storage != stack_vars)
                    free(filtered_storage);
                return true;
            }

            return expr_collect_var_usage_impl(expr->a, filtered_nvars, filtered_vars, used_out);
        }
        return true;
    }

    if (expr_is_op(expr, &ops_summation) || expr_is_op(expr, &ops_product)) {
        const expr_t *index = expr->b;
        const expr_t *lower = NULL;
        const expr_t *upper = NULL;

        if (expr_is_op(expr->b, &ops_argument_list)) {
            index = expr->b->a;
            upper = expr->b->b;
            if (expr_is_op(upper, &ops_argument_list)) {
                lower = upper->a;
                upper = upper->b;
            }
        }
        if (lower && !expr_collect_var_usage_impl(lower, nvars, vars, used_out))
            return false;
        if (upper && !expr_collect_var_usage_impl(upper, nvars, vars, used_out))
            return false;
        if (expr->a) {
            expr_t *const *filtered_vars = vars;
            size_t filtered_nvars = nvars;

            if (index && nvars > 0u) {
                size_t out = 0u;
                expr_t *stack_vars[16];
                expr_t **filtered_storage = nvars <= 16u ? stack_vars : calloc(nvars, sizeof(*filtered_storage));

                if (!filtered_storage)
                    return false;
                filtered_vars = filtered_storage;
                for (size_t i = 0u; i < nvars; ++i) {
                    const int same_index = vars[i] == index ||
                                           (expr_is_var(vars[i]) && expr_is_var(index) && vars[i]->var_id != 0u &&
                                            vars[i]->var_id == index->var_id);

                    if (!same_index)
                        filtered_storage[out++] = vars[i];
                }
                filtered_nvars = out;
                if (!expr_collect_var_usage_impl(expr->a, filtered_nvars, filtered_vars, used_out)) {
                    if (filtered_storage != stack_vars)
                        free(filtered_storage);
                    return false;
                }
                if (filtered_storage != stack_vars)
                    free(filtered_storage);
                return true;
            }
            return expr_collect_var_usage_impl(expr->a, filtered_nvars, filtered_vars, used_out);
        }
        return true;
    }

    if (expr->a && !expr_collect_var_usage_impl(expr->a, nvars, vars, used_out))
        return false;
    if (expr->b && !expr_collect_var_usage_impl(expr->b, nvars, vars, used_out))
        return false;
    return true;
}

bool expr_collect_var_usage(const expr_t *expr, size_t nvars, expr_t *const *vars, bool *used_out)
{
    if (!used_out || (nvars > 0 && !vars))
        return false;
    for (size_t i = 0; i < nvars; ++i)
        used_out[i] = false;
    return expr_collect_var_usage_impl(expr, nvars, vars, used_out);
}

expr_t *expr_substitute(const expr_t *expr, const expr_t *needle, const expr_t *replacement)
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
        return (expr_t *)replacement;
    }

    /* Substitution may change a finite operator's bounds, but must not capture a shadowing index. */
    if (expr_is_op(expr, &ops_summation) || expr_is_op(expr, &ops_product)) {
        const expr_t *index = expr_is_op(expr->b, &ops_argument_list) ? expr->b->a : expr->b;
        const expr_t *limits = expr_is_op(expr->b, &ops_argument_list) ? expr->b->b : NULL;
        const expr_t *lower = expr_is_op(limits, &ops_argument_list) ? limits->a : NULL;
        const expr_t *upper = lower ? limits->b : limits;
        const bool shadowed = index == needle || expr_is_same_named_leaf_for_substitution(index, needle);
        expr_t *term = shadowed ? expr_clone(expr->a) : expr_substitute(expr->a, needle, replacement);
        expr_t *lower_copy = lower ? expr_substitute(lower, needle, replacement) : NULL;
        expr_t *upper_copy = upper ? expr_substitute(upper, needle, replacement) : NULL;
        const bool sum = expr_is_op(expr, &ops_summation);

        out = NULL;
        if (term && (!lower || lower_copy) && (!upper || upper_copy)) {
            if (lower)
                out = sum ? expr_new_finite_summation_range(term, index, lower_copy, upper_copy)
                          : expr_new_finite_product_range(term, index, lower_copy, upper_copy);
            else if (upper)
                out = sum ? expr_new_finite_summation(term, index, upper_copy)
                          : expr_new_finite_product_range(term, index, EXPR_ZERO, upper_copy);
            else
                out = sum ? expr_new_summation(term, index) : expr_new_product(term, index);
        }
        expr_free(upper_copy);
        expr_free(lower_copy);
        expr_free(term);
        return out;
    }

    if (expr_is_formal_derivative(expr)) {
        const expr_t *dependent = expr_formal_derivative_dependent(expr);

        if (dependent == needle || expr_is_same_named_leaf_for_substitution(dependent, needle)) {
            expr_t *value = expr_clone(replacement);

            for (size_t i = 0u; value && i < expr_formal_derivative_order(expr); ++i) {
                expr_t *next = expr_create_deriv(value, expr_formal_derivative_wrt_at(expr, i));

                expr_free(value);
                value = next;
            }
            return value;
        }
        return expr_clone(expr);
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

    if (expr_is_arbitrary_function(expr)) {
        left = expr_substitute(expr->a, needle, replacement);
        if (!left)
            return NULL;
        out = expr_new_arbitrary_function(expr->name, left);
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

    if (expr_is_op(expr, &ops_integral)) {
        const expr_t *dummy = expr_integral_dummy_expr(expr);
        const expr_t *lower = expr_integral_lower_bound_expr(expr);
        const expr_t *upper = expr_integral_upper_bound_expr(expr);
        bool shadowed = false;

        if (dummy) {
            shadowed =
                dummy == needle ||
                (expr_is_var(dummy) && expr_is_var(needle) && dummy->var_id != 0 && dummy->var_id == needle->var_id) ||
                expr_is_same_named_leaf_for_substitution(dummy, needle);
        }

        left = shadowed ? expr_clone(expr->a) : expr_substitute(expr->a, needle, replacement);
        right = upper ? expr_substitute(upper, needle, replacement) : NULL;
        if (!left || !right) {
            expr_free(right);
            expr_free(left);
            return NULL;
        }

        if (lower) {
            expr_t *lower_copy = expr_substitute(lower, needle, replacement);

            if (!lower_copy) {
                expr_free(right);
                expr_free(left);
                return NULL;
            }
            out = dummy ? expr_integral_with_bounds_internal(left, lower_copy, right, dummy)
                        : expr_integral_with_bounds_internal(left, lower_copy, right, right);
            expr_free(lower_copy);
        } else {
            out = dummy ? expr_integral_with_dummy_internal(left, right, dummy)
                        : expr_integral_with_dummy_internal(left, right, right);
        }

        expr_free(right);
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

static expr_t *expr_display_expanded_expr_mode(const expr_t *expr, bool expand_sum_products);

static expr_t *expr_display_expanded_product(const expr_t *left, const expr_t *right, bool expand_sum_products)
{
    expr_t *left_expr;
    expr_t *right_expr;
    expr_t *out;
    const expr_t *child_left = NULL;
    const expr_t *child_right = NULL;
    bool is_sub = false;

    if (!left || !right)
        return NULL;

    if (expr_is_op(left, &ops_neg) && left->a) {
        expr_t *inner = expr_display_expanded_product(left->a, right, expand_sum_products);
        expr_t *negated = inner ? expr_neg(inner) : NULL;

        expr_free(inner);
        return negated;
    }
    if (expr_is_op(right, &ops_neg) && right->a)
        return expr_display_expanded_product(right, left, expand_sum_products);

    if (!expand_sum_products && expr_match_add_sub_expr(left, &child_left, &child_right, &is_sub) &&
        expr_match_add_sub_expr(right, &child_left, &child_right, &is_sub)) {
        left_expr = expr_display_expanded_expr_mode(left, false);
        right_expr = expr_display_expanded_expr_mode(right, false);
        if (!left_expr || !right_expr) {
            expr_free(left_expr);
            expr_free(right_expr);
            return NULL;
        }

        out = expr_mul(left_expr, right_expr);
        expr_free(left_expr);
        expr_free(right_expr);
        return out;
    }

    if (expr_match_add_expr(left, &child_left, &child_right)) {
        expr_t *first = expr_display_expanded_product(child_left, right, expand_sum_products);
        expr_t *second = expr_display_expanded_product(child_right, right, expand_sum_products);

        if (!first || !second) {
            expr_free(first);
            expr_free(second);
            return NULL;
        }

        out = expr_add(first, second);
        expr_free(first);
        expr_free(second);
        return out;
    }

    if (expr_match_sub_expr(left, &child_left, &child_right)) {
        expr_t *first = expr_display_expanded_product(child_left, right, expand_sum_products);
        expr_t *second = expr_display_expanded_product(child_right, right, expand_sum_products);

        if (!first || !second) {
            expr_free(first);
            expr_free(second);
            return NULL;
        }

        out = expr_sub(first, second);
        expr_free(first);
        expr_free(second);
        return out;
    }

    if (expr_match_add_sub_expr(right, &child_left, &child_right, &is_sub))
        return expr_display_expanded_product(right, left, expand_sum_products);

    left_expr = expr_display_expanded_expr_mode(left, expand_sum_products);
    right_expr = expr_display_expanded_expr_mode(right, expand_sum_products);
    if (!left_expr || !right_expr) {
        expr_free(left_expr);
        expr_free(right_expr);
        return NULL;
    }

    if (expand_sum_products && (expr_is_addsub(left_expr) || expr_is_addsub(right_expr))) {
        out = expr_display_expanded_product(left_expr, right_expr, expand_sum_products);
    } else {
        out = expr_mul(left_expr, right_expr);
    }
    expr_free(left_expr);
    expr_free(right_expr);
    return out;
}

static expr_t *expr_display_expanded_expr_mode(const expr_t *expr, bool expand_sum_products)
{
    expr_t *left;
    expr_t *right;
    expr_t *out;
    const expr_t *child_left = NULL;
    const expr_t *child_right = NULL;
    const expr_t *scaled_base = NULL;
    number_t scale = num_new();

    if (!expr) {
        num_destroy(&scale);
        return NULL;
    }

    if (expand_sum_products && expr_match_scaled_expr(expr, &scale, &scaled_base) && scaled_base &&
        scaled_base != expr && expr_is_addsub(scaled_base)) {
        expr_t *scale_expr = expr_new_const(scale);

        out = scale_expr ? expr_display_expanded_product(scale_expr, scaled_base, expand_sum_products) : NULL;
        expr_free(scale_expr);
        num_destroy(&scale);
        return out;
    }
    num_destroy(&scale);

    if (expr_match_add_expr(expr, &child_left, &child_right)) {
        left = expr_display_expanded_expr_mode(child_left, expand_sum_products);
        right = expr_display_expanded_expr_mode(child_right, expand_sum_products);
        if (!left || !right) {
            expr_free(left);
            expr_free(right);
            return NULL;
        }
        out = expr_add(left, right);
        expr_free(left);
        expr_free(right);
        return out;
    }

    if (expr_match_sub_expr(expr, &child_left, &child_right)) {
        left = expr_display_expanded_expr_mode(child_left, expand_sum_products);
        right = expr_display_expanded_expr_mode(child_right, expand_sum_products);
        if (!left || !right) {
            expr_free(left);
            expr_free(right);
            return NULL;
        }
        out = expr_sub(left, right);
        expr_free(left);
        expr_free(right);
        return out;
    }

    if (expr_is_op(expr, &ops_neg) && expr->a) {
        left = expr_display_expanded_expr_mode(expr->a, expand_sum_products);
        out = left ? expr_neg(left) : NULL;
        expr_free(left);
        return out;
    }

    if (expr_match_mul_expr(expr, &child_left, &child_right))
        return expr_display_expanded_product(child_left, child_right, expand_sum_products);

    expr_retain(expr);
    return (expr_t *)expr;
}

static bool expr_display_integral_is_indefinite(const expr_t *upper, const expr_t *dummy)
{
    return upper && dummy && expr_is_var(upper) && expr_is_var(dummy) &&
           (upper == dummy || (upper->var_id != 0 && dummy->var_id != 0 && upper->var_id == dummy->var_id));
}

static expr_t *expr_display_add_integration_constant(expr_t *anti)
{
    expr_t *constant;
    expr_t *out;

    if (!anti)
        return NULL;

    constant = expr_new_integration_constant(anti, NULL, anti);
    if (!constant)
        return anti;

    out = expr_add(anti, constant);
    expr_free(constant);
    if (!out)
        return anti;
    expr_free(anti);
    return out;
}

static expr_t *expr_display_simplified_integral(const expr_t *expr)
{
    const expr_t *dummy;
    const expr_t *lower;
    const expr_t *upper;
    expr_t *local_var = NULL;
    expr_t *family_var = NULL;
    expr_t *local_integrand = NULL;
    expr_t *anti = NULL;
    expr_t *upper_eval = NULL;
    expr_t *lower_eval = NULL;
    expr_t *diff = NULL;
    expr_t *simplified = NULL;
    expr_t *out = NULL;
    bool indefinite;

    if (!expr_is_op(expr, &ops_integral) || !expr->a)
        return NULL;

    dummy = expr_integral_dummy_expr(expr);
    upper = expr_integral_upper_bound_expr(expr);
    lower = expr_integral_lower_bound_expr(expr);
    if (!dummy || !upper)
        return NULL;
    indefinite = !lower && expr_display_integral_is_indefinite(upper, dummy);

    local_var = expr_clone(dummy);
    local_integrand = local_var ? expr_substitute(expr->a, dummy, local_var) : NULL;
    anti = local_integrand ? expr_integrate(local_integrand, local_var) : NULL;
    if (!anti)
        goto cleanup;

    if (indefinite) {
        family_var = expr_new_named_var(NUM_NAN, expr_symbol_name(dummy) ? expr_symbol_name(dummy) : "x");
    }
    upper_eval = expr_substitute(anti, local_var, family_var ? family_var : upper);
    if (!upper_eval)
        goto cleanup;

    if (lower) {
        lower_eval = expr_substitute(anti, local_var, lower);
        diff = lower_eval ? expr_sub(upper_eval, lower_eval) : NULL;
        simplified = diff ? expr_simplify(diff) : NULL;
        if (simplified) {
            out = simplified;
            simplified = NULL;
        } else {
            out = diff;
            diff = NULL;
        }
    } else {
        /*
         * Display-only: a missing lower bound mirrors the Integrator's
         * upper-bound antiderivative output. Numeric evaluation still keeps
         * the existing expression semantics.
         */
        simplified = expr_simplify(upper_eval);
        if (simplified) {
            out = simplified;
            simplified = NULL;
        } else {
            out = upper_eval;
            upper_eval = NULL;
        }
        if (indefinite)
            out = expr_display_add_integration_constant(out);
    }

cleanup:
    expr_free(simplified);
    expr_free(diff);
    expr_free(lower_eval);
    expr_free(upper_eval);
    expr_free(anti);
    expr_free(local_integrand);
    expr_free(family_var);
    expr_free(local_var);
    return out;
}

static expr_t *expr_display_clone_integral(const expr_t *expr)
{
    const expr_t *dummy;
    const expr_t *lower;
    const expr_t *upper;

    if (!expr_is_op(expr, &ops_integral) || !expr->a)
        return NULL;

    dummy = expr_integral_dummy_expr(expr);
    upper = expr_integral_upper_bound_expr(expr);
    lower = expr_integral_lower_bound_expr(expr);
    if (!dummy || !upper)
        return NULL;

    return lower ? expr_integral_with_bounds_internal(expr->a, lower, upper, dummy)
                 : expr_integral_with_dummy_internal(expr->a, upper, dummy);
}

expr_t *expr_display_simplified(const expr_t *expr)
{
    expr_t *expanded;
    expr_t *simplified;

    if (!expr)
        return NULL;

    simplified = expr_display_simplified_integral(expr);
    if (simplified)
        return simplified;
    if (expr_is_op(expr, &ops_integral))
        return expr_display_clone_integral(expr);

    expanded = expr_display_expanded_expr_mode(expr, false);
    if (!expanded)
        return expr_simplify(expr);

    simplified = expr_simplify(expanded);
    if (!simplified)
        return expanded;

    expr_free(expanded);
    return simplified;
}

static expr_t *expr_display_simplify_expanded_terms_local(const expr_t *expr)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    expr_t *left_display;
    expr_t *right_display;
    expr_t *out;

    if (!expr)
        return NULL;

    if (!expr_match_add_expr(expr, &left, &right) && !expr_match_sub_expr(expr, &left, &right))
        return expr_simplify(expr);

    left_display = expr_display_simplify_expanded_terms_local(left);
    right_display = expr_display_simplify_expanded_terms_local(right);
    if (!left_display || !right_display) {
        expr_free(right_display);
        expr_free(left_display);
        return NULL;
    }

    out = expr_is_op(expr, &ops_sub) ? expr_sub(left_display, right_display) : expr_add(left_display, right_display);
    expr_free(right_display);
    expr_free(left_display);
    return out;
}

static bool expr_display_is_proper_fraction_scaled_sum_local(const expr_t *expr)
{
    number_t scale = num_new();
    number_t magnitude;
    const expr_t *base = NULL;
    bool matched;

    matched = expr_match_scaled_expr(expr, &scale, &base) && base && base != expr && expr_is_addsub(base);
    if (!matched) {
        num_destroy(&scale);
        return false;
    }

    magnitude = num_abs(scale);
    matched = num_gt(magnitude, NUM_ZERO) && num_lt(magnitude, NUM_ONE);
    num_destroy(&magnitude);
    num_destroy(&scale);
    return matched;
}

expr_t *expr_display_expanded(const expr_t *expr)
{
    expr_t *expanded;
    expr_t *termwise;
    expr_t *simplified;

    if (!expr)
        return NULL;

    expanded = expr_display_expanded_expr_mode(expr, true);
    if (!expanded)
        return expr_simplify(expr);

    termwise = expr_display_simplify_expanded_terms_local(expanded);
    simplified = expr_simplify(expanded);
    if (termwise && expr_is_addsub(termwise) && simplified &&
        expr_display_is_proper_fraction_scaled_sum_local(simplified)) {
        expr_free(simplified);
        expr_free(expanded);
        return termwise;
    }
    expr_free(termwise);
    if (!simplified)
        return expanded;

    expr_free(expanded);
    return simplified;
}

expr_t *expr_expand_products_internal(const expr_t *expr)
{
    return expr_display_expanded_expr_mode(expr, true);
}

expr_t *expr_canonicalize_known_radicals_internal(const expr_t *expr)
{
    expr_t *left;
    expr_t *right;
    expr_t *out;

    if (!expr)
        return NULL;

    if (expr_is_op(expr, &ops_const) && (!expr->name || !*expr->name)) {
        if (num_eq(expr->c, NUM_SQRT2))
            return expr_new_const(NUM_SQRT2);
        if (num_eq(expr->c, NUM_SQRT3))
            return expr_new_const(NUM_SQRT3);
        if (num_eq(expr->c, NUM_SQRT_HALF))
            return expr_new_const(NUM_SQRT_HALF);
        if (num_eq(expr->c, NUM_SQRT2_OVER_TWO))
            return expr_new_const(NUM_SQRT2_OVER_TWO);
        if (num_eq(expr->c, NUM_SQRT3_OVER_TWO))
            return expr_new_const(NUM_SQRT3_OVER_TWO);
    }

    if (expr_is_op(expr, &ops_sqrt) && expr->a && expr_is_op(expr->a, &ops_const) &&
        (!expr->a->name || !*expr->a->name)) {
        number_t three = num_create_from_long(3L);

        if (num_eq(expr->a->c, NUM_TWO)) {
            num_destroy(&three);
            return expr_new_const(NUM_SQRT2);
        }
        if (num_eq(expr->a->c, three)) {
            num_destroy(&three);
            return expr_new_const(NUM_SQRT3);
        }
        num_destroy(&three);
        if (num_eq(expr->a->c, NUM_HALF))
            return expr_new_const(NUM_SQRT_HALF);
    }

    if (expr->ops && expr->ops->arity == EXPR_OP_UNARY && expr->ops->apply_unary && expr->a) {
        left = expr_canonicalize_known_radicals_internal(expr->a);
        out = left ? expr->ops->apply_unary(left) : NULL;
        expr_free(left);
        return out;
    }

    if (expr->ops && expr->ops->arity == EXPR_OP_BINARY && expr->ops->apply_binary && expr->a && expr->b) {
        left = expr_canonicalize_known_radicals_internal(expr->a);
        right = expr_canonicalize_known_radicals_internal(expr->b);
        out = (left && right) ? expr->ops->apply_binary(left, right) : NULL;
        expr_free(right);
        expr_free(left);
        return out;
    }

    expr_retain(expr);
    return (expr_t *)expr;
}

static char *expr_note_text_dup(const expr_t *expr, style_t style)
{
    string_t *text = expr_to_text(expr, style);
    char *copy = text ? strdup(string_c_str(text)) : NULL;

    string_free(text);
    return copy;
}

static bool expr_note_value_is_defined_at(const expr_t *integrand, const expr_t *var, number_t point)
{
    expr_t *point_const;
    expr_t *eval_expr;
    number_t value;
    bool ok;

    if (!integrand || !var)
        return false;

    point_const = expr_new_const(point);
    eval_expr = point_const ? expr_substitute(integrand, var, point_const) : NULL;
    if (!point_const || !eval_expr) {
        expr_free(eval_expr);
        expr_free(point_const);
        return false;
    }

    value = expr_eval(eval_expr);
    ok = num_is_real(value) && num_is_finite(value);
    num_destroy(&value);
    expr_free(eval_expr);
    expr_free(point_const);
    return ok;
}

static bool expr_note_integrand_uses_dummy(const expr_t *integrand, const expr_t *dummy)
{
    expr_t *vars[1];
    bool used[1] = {false};

    if (!integrand || !dummy)
        return false;
    vars[0] = (expr_t *)dummy;
    return expr_collect_var_usage(integrand, 1u, vars, used) && used[0];
}

static const expr_t *expr_note_first_free_var_other_than(const expr_t *expr, const expr_t *dummy)
{
    const expr_t *nested_dummy;
    const expr_t *left;

    if (!expr)
        return NULL;

    if (expr_is_var(expr)) {
        if (expr_display_integral_is_indefinite(expr, dummy))
            return NULL;
        return expr;
    }

    if (expr_is_op(expr, &ops_integral)) {
        nested_dummy = expr_integral_dummy_expr(expr);
        left = expr_note_first_free_var_other_than(expr_integral_lower_bound_expr(expr), dummy);
        if (left)
            return left;
        left = expr_note_first_free_var_other_than(expr_integral_upper_bound_expr(expr), dummy);
        if (left)
            return left;
        return expr_note_first_free_var_other_than(expr->a, nested_dummy);
    }

    left = expr_note_first_free_var_other_than(expr->a, dummy);
    if (left)
        return left;
    return expr_note_first_free_var_other_than(expr->b, dummy);
}

static bool expr_note_symbolic_integral_value_is_finite(const expr_t *expr)
{
    expr_t *display;
    number_t value;
    bool ok;

    display = expr_display_simplified_integral(expr);
    if (!display)
        return false;

    value = expr_eval(display);
    ok = num_is_real(value) && num_is_finite(value);
    num_destroy(&value);
    expr_free(display);
    return ok;
}

bool expr_integral_value_note(const expr_t *expr, char *out, size_t out_size)
{
    const expr_t *lower_expr;
    const expr_t *upper_expr;
    const expr_t *dummy_expr;
    expr_t *upper_const = NULL;
    expr_t *lower_const = NULL;
    expr_t *upper_integrand = NULL;
    expr_t *lower_integrand = NULL;
    number_t upper = number_invalid();
    number_t lower = number_invalid();
    char *upper_text = NULL;
    char *lower_text = NULL;
    char *integrand_text = NULL;
    const expr_t *integrand = NULL;
    const expr_t *child_left = NULL;
    const expr_t *child_right = NULL;
    const expr_t *parameter_var = NULL;
    const char *dummy_name;
    const char *parameter_name;
    bool found = false;

    if (!expr || !out || out_size == 0u)
        return false;

    if (expr_match_integral_expr(expr, &integrand, NULL)) {
        lower_expr = expr_integral_lower_bound_expr(expr);
        upper_expr = expr_integral_upper_bound_expr(expr);
        dummy_expr = expr_integral_dummy_expr(expr);
        dummy_name = expr_symbol_name(dummy_expr);

        if (dummy_expr && !expr_note_integrand_uses_dummy(integrand, dummy_expr)) {
            parameter_var = expr_note_first_free_var_other_than(integrand, dummy_expr);
            parameter_name = expr_symbol_name(parameter_var);
            if (parameter_name) {
                snprintf(out, out_size,
                         "The differential is d%s, so %s is treated as a parameter. Use d%s if you intended to "
                         "integrate with respect to %s.",
                         dummy_name ? dummy_name : "t", parameter_name, parameter_name, parameter_name);
                found = true;
            }
        }

        num_destroy(&upper);
        upper = upper_expr ? expr_eval(upper_expr) : num_clone(NUM_NAN);
        num_destroy(&lower);
        lower = lower_expr ? expr_eval(lower_expr) : num_clone(NUM_ZERO);
        if (!found && upper_expr && dummy_expr && num_is_real(lower) && num_is_finite(lower) && num_is_real(upper) &&
            num_is_finite(upper) && !expr_note_symbolic_integral_value_is_finite(expr)) {
            upper_const = expr_new_const(upper);
            lower_const = expr_new_const(lower);
            upper_integrand = upper_const ? expr_substitute(integrand, dummy_expr, upper_const) : NULL;
            lower_integrand = lower_const ? expr_substitute(integrand, dummy_expr, lower_const) : NULL;
            if (upper_const && lower_const && upper_integrand && lower_integrand) {
                upper_text = expr_note_text_dup(upper_expr, style_UNBOUND);
                lower_text = lower_expr ? expr_note_text_dup(lower_expr, style_UNBOUND) : NULL;
                integrand_text = expr_note_text_dup(integrand, style_UNBOUND);

                if (!lower_expr && !expr_note_value_is_defined_at(integrand, dummy_expr, NUM_ZERO)) {
                    snprintf(out, out_size,
                             "Here ∫^%s means ∫₀^%s. The integrand %s is not finite at %s = 0; evaluation "
                             "requires an improper-integral limit that could not be determined.",
                             upper_text ? upper_text : "?", upper_text ? upper_text : "?",
                             integrand_text ? integrand_text : "f(t)", dummy_name ? dummy_name : "t");
                    found = true;
                } else if (!expr_note_value_is_defined_at(integrand, dummy_expr, lower)) {
                    snprintf(out, out_size,
                             "The integrand %s is not finite at %s = %s; the improper-integral limit at the lower "
                             "bound could not be determined.",
                             integrand_text ? integrand_text : "f(t)", dummy_name ? dummy_name : "t",
                             lower_text ? lower_text : "0");
                    found = true;
                } else if (!expr_note_value_is_defined_at(integrand, dummy_expr, upper)) {
                    snprintf(out, out_size,
                             "The integrand %s is not finite at %s = %s; the improper-integral limit at the upper "
                             "bound could not be determined.",
                             integrand_text ? integrand_text : "f(t)", dummy_name ? dummy_name : "t",
                             upper_text ? upper_text : "?");
                    found = true;
                }
            }
        }
    }

    free(integrand_text);
    free(lower_text);
    free(upper_text);
    expr_free(lower_integrand);
    expr_free(upper_integrand);
    expr_free(lower_const);
    expr_free(upper_const);
    num_destroy(&lower);
    num_destroy(&upper);

    if (found)
        return true;

    if (!expr_child_exprs(expr, &child_left, &child_right))
        return false;
    return expr_integral_value_note(child_left, out, out_size) || expr_integral_value_note(child_right, out, out_size);
}
