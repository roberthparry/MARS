#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include "expr_internal.h"
#include "expression.h"
#include "internal/expr_internal.h"
#include "internal/number_internal.h"

static bool is_kind(const expr_t *expr, expr_op_kind_t kind)
{
    return expr && expr->ops && expr->ops->kind == kind;
}

bool expr_is_exact_zero(const expr_t *dv)
{
    return is_kind(dv, EXPR_KIND_CONST) &&
           !dv->name &&
           num_eq(dv->c, NUM_ZERO);
}

bool expr_is_named_const(const expr_t *dv)
{
    return is_kind(dv, EXPR_KIND_CONST) && dv->name && *dv->name;
}

static bool expr_match_const_leaf(const expr_t *expr, number_t *value_out, const char **name_out)
{
    if (!is_kind(expr, EXPR_KIND_CONST))
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
    if (!is_kind(expr, EXPR_KIND_VAR))
        return false;
    if (value_out) {
        num_destroy(value_out);
        *value_out = num_clone(expr->c);
    }
    if (name_out)
        *name_out = (expr->name && *expr->name) ? expr->name : NULL;
    return true;
}

static bool expr_match_unnamed_const_leaf(const expr_t *expr, number_t *value_out)
{
    if (!value_out || !is_kind(expr, EXPR_KIND_CONST))
        return false;
    if (expr->name && *expr->name)
        return false;
    num_destroy(value_out);
    *value_out = num_clone(expr->c);
    return true;
}

static bool expr_match_unary_kind(const expr_t *expr, expr_op_kind_t kind, const expr_t **arg_out)
{
    if (!is_kind(expr, kind) || !expr->a)
        return false;
    if (arg_out)
        *arg_out = expr->a;
    return true;
}

static bool expr_match_binary_kind(const expr_t *expr,
                               expr_op_kind_t kind,
                               const expr_t **left_out,
                               const expr_t **right_out)
{
    if (!is_kind(expr, kind) || !expr->a || !expr->b)
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
        if (vars[i] == dv)
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

static void expr_zero_number_array(number_t *values, size_t n)
{
    for (size_t i = 0; i < n; ++i)
        values[i] = num_scope_detach(num_clone(NUM_ZERO));
}

static void expr_reset_number_array(number_t *values, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        num_destroy(&values[i]);
        values[i] = num_scope_detach(num_clone(NUM_ZERO));
    }
}

static void expr_clear_number_array(number_t *values, size_t n)
{
    if (!values)
        return;
    for (size_t i = 0; i < n; ++i)
        num_destroy(&values[i]);
}

static void expr_destroy_number_array(number_t *values, size_t n)
{
    if (!values)
        return;
    expr_clear_number_array(values, n);
    free(values);
}

static number_t *expr_clone_number_array(const number_t *values, size_t n)
{
    number_t *copy = malloc(n * sizeof(*copy));

    if ((n > 0) && !copy)
        return NULL;
    for (size_t i = 0; i < n; ++i)
        copy[i] = num_scope_detach(num_clone(values[i]));
    return copy;
}

static void expr_copy_number_array(number_t *dst, const number_t *src, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        num_destroy(&dst[i]);
        dst[i] = num_scope_detach(num_clone(src[i]));
    }
}

static bool expr_match_affine_term(const expr_t *dv,
                                 size_t nvars,
                                 expr_t *const *vars,
                                 number_t scale,
                                 number_t *constant_io,
                                 number_t *coeffs_io)
{
    NUM_SCOPE(scope);
    number_t constant = num_new();
    number_t inner_scale = num_new();
    const expr_t *base;
    const expr_t *left;
    const expr_t *right;
    bool is_sub;
    size_t idx;

    if (!dv)
        return false;

    if (expr_match_const_value(dv, &constant)) {
        number_t product;
        number_t sum;

        if (!num_is_real(constant) || !num_is_real(scale)) {
            return false;
        }
        product = num_mul(scale, constant);
        sum = num_add(*constant_io, product);
        num_destroy(constant_io);
        *constant_io = num_scope_detach(sum);
        return true;
    }

    if (expr_match_var_expr(dv, nvars, vars, &idx)) {
        if (!num_is_real(scale)) {
            return false;
        }
        {
            number_t sum = num_add(coeffs_io[idx], scale);

            num_destroy(&coeffs_io[idx]);
            coeffs_io[idx] = num_scope_detach(sum);
        }
        return true;
    }

    if (expr_match_add_sub_expr(dv, &left, &right, &is_sub)) {
        number_t right_scale = is_sub ? num_neg(scale) : num_clone(scale);
        number_t trial_constant = num_clone(*constant_io);
        number_t *trial_coeffs = expr_clone_number_array(coeffs_io, nvars);
        bool ok;

        if ((nvars > 0) && !trial_coeffs) {
            num_destroy(&trial_constant);
            return false;
        }

        ok = expr_match_affine_term(left, nvars, vars, scale, &trial_constant, trial_coeffs) &&
             expr_match_affine_term(right, nvars, vars, right_scale, &trial_constant, trial_coeffs);
        if (!ok) {
            num_destroy(&trial_constant);
            expr_destroy_number_array(trial_coeffs, nvars);
            return false;
        }

        num_destroy(constant_io);
        *constant_io = num_scope_detach(trial_constant);
        for (size_t i = 0; i < nvars; ++i) {
            num_destroy(&coeffs_io[i]);
            coeffs_io[i] = trial_coeffs[i];
            trial_coeffs[i] = number_invalid();
        }
        expr_destroy_number_array(trial_coeffs, nvars);
        return true;
    }

    if (expr_match_scaled_expr(dv, &inner_scale, &base)) {
        number_t product;
        number_t trial_constant;
        number_t *trial_coeffs;
        bool ok;

        if (!num_is_real(inner_scale) || !num_is_real(scale)) {
            num_destroy(&inner_scale);
            return false;
        }
        product = num_mul(scale, inner_scale);
        num_destroy(&inner_scale);
        trial_constant = num_clone(*constant_io);
        trial_coeffs = expr_clone_number_array(coeffs_io, nvars);
        if ((nvars > 0) && !trial_coeffs) {
            num_destroy(&trial_constant);
            return false;
        }

        ok = expr_match_affine_term(base, nvars, vars, product, &trial_constant, trial_coeffs);
        if (!ok) {
            num_destroy(&trial_constant);
            expr_destroy_number_array(trial_coeffs, nvars);
            return false;
        }

        num_destroy(constant_io);
        *constant_io = num_scope_detach(trial_constant);
        for (size_t i = 0; i < nvars; ++i) {
            num_destroy(&coeffs_io[i]);
            coeffs_io[i] = trial_coeffs[i];
            trial_coeffs[i] = number_invalid();
        }
        expr_destroy_number_array(trial_coeffs, nvars);
        return true;
    }

    return false;
}

static bool expr_pattern_unary_kind_to_op(expr_pattern_unary_affine_kind_t kind,
                                        expr_op_kind_t *op_kind_out)
{
    static const expr_op_kind_t unary_op_table[] = {
        EXPR_KIND_EXP,
        EXPR_KIND_LOG,
        EXPR_KIND_LOG10,
        EXPR_KIND_SIN,
        EXPR_KIND_COS,
        EXPR_KIND_TAN,
        EXPR_KIND_SEC,
        EXPR_KIND_COSEC,
        EXPR_KIND_COT,
        EXPR_KIND_SINH,
        EXPR_KIND_COSH,
        EXPR_KIND_COSECH,
        EXPR_KIND_TANH,
        EXPR_KIND_SECH,
        EXPR_KIND_COTH,
        EXPR_KIND_ASIN,
        EXPR_KIND_ACOS,
        EXPR_KIND_ATAN,
        EXPR_KIND_ASEC,
        EXPR_KIND_ACOSEC,
        EXPR_KIND_ACOT,
        EXPR_KIND_ASINH,
        EXPR_KIND_ACOSH,
        EXPR_KIND_ATANH,
        EXPR_KIND_ASECH,
        EXPR_KIND_ACOSECH,
        EXPR_KIND_ACOTH,
        EXPR_KIND_ERF,
        EXPR_KIND_ERFC,
        EXPR_KIND_NORMAL_PDF,
        EXPR_KIND_NORMAL_CDF,
        EXPR_KIND_NORMAL_LOGPDF,
        EXPR_KIND_EI,
        EXPR_KIND_E1
    };

    if (!op_kind_out)
        return false;

    if ((unsigned)kind >= sizeof(unary_op_table) / sizeof(unary_op_table[0]))
        return false;

    *op_kind_out = unary_op_table[kind];
    return true;
}

static bool expr_match_unary_affine_op(const expr_t *expr,
                                     expr_op_kind_t op_kind,
                                     size_t nvars,
                                     expr_t *const *vars,
                                     number_t *constant_out,
                                     number_t *coeffs_out)
{
    NUM_SCOPE(scope);
    number_t constant = num_clone(NUM_ZERO);
    bool ok;

    if (!expr || !constant_out || !coeffs_out || (nvars > 0 && !vars))
        return false;

    if (!is_kind(expr, op_kind) || !expr->a)
        return false;

    expr_reset_number_array(coeffs_out, nvars);

    ok = expr_match_affine_term(expr->a, nvars, vars, NUM_ONE, &constant, coeffs_out);
    if (!ok) {
        num_destroy(&constant);
        expr_reset_number_array(coeffs_out, nvars);
        return false;
    }

    num_destroy(constant_out);
    *constant_out = num_scope_detach(constant);
    return true;
}

static bool expr_match_unary_affine_kind_num_local(const expr_t *expr,
                                                 expr_pattern_unary_affine_kind_t kind,
                                                 size_t nvars,
                                                 expr_t *const *vars,
                                                 number_t *constant_out,
                                                 number_t *coeffs_out)
{
    expr_op_kind_t op_kind;

    if (!expr_pattern_unary_kind_to_op(kind, &op_kind))
        return false;

    return expr_match_unary_affine_op(expr, op_kind, nvars, vars, constant_out, coeffs_out);
}

bool expr_match_unary_affine_kind(const expr_t *expr,
                                expr_pattern_unary_affine_kind_t kind,
                                size_t nvars,
                                expr_t *const *vars,
                                number_t *constant_out,
                                number_t *coeffs_out)
{
    if (!constant_out)
        return false;
    return expr_match_unary_affine_kind_num_local(expr, kind, nvars, vars, constant_out, coeffs_out);
}

static bool expr_match_affine_power_mul_deg2(const expr_t *expr, const expr_t **base_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;

    if (!expr_match_binary_kind(expr, EXPR_KIND_MUL, &left, &right))
        return false;
    if (!expr_struct_eq(left, right))
        return false;

    *base_out = left;
    return true;
}

static bool expr_match_affine_power_mul_deg3(const expr_t *expr, const expr_t **base_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    const expr_t *inner_left = NULL;
    const expr_t *inner_right = NULL;

    if (expr_match_binary_kind(expr, EXPR_KIND_MUL, &left, &right) &&
        expr_match_binary_kind(left, EXPR_KIND_MUL, &inner_left, &inner_right) &&
        expr_struct_eq(inner_left, inner_right) &&
        expr_struct_eq(inner_left, right)) {
        *base_out = inner_left;
        return true;
    }

    if (expr_match_binary_kind(expr, EXPR_KIND_MUL, &left, &right) &&
        expr_match_binary_kind(right, EXPR_KIND_MUL, &inner_left, &inner_right) &&
        expr_struct_eq(left, inner_left) &&
        expr_struct_eq(left, inner_right)) {
        *base_out = left;
        return true;
    }

    return false;
}

static bool expr_match_affine_power_mul_deg4(const expr_t *expr, const expr_t **base_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    const expr_t *ll = NULL;
    const expr_t *lr = NULL;
    const expr_t *rl = NULL;
    const expr_t *rr = NULL;

    if (!expr_match_binary_kind(expr, EXPR_KIND_MUL, &left, &right) ||
        !expr_match_binary_kind(left, EXPR_KIND_MUL, &ll, &lr) ||
        !expr_match_binary_kind(right, EXPR_KIND_MUL, &rl, &rr))
        return false;
    if (!expr_struct_eq(ll, lr) ||
        !expr_struct_eq(ll, rl) ||
        !expr_struct_eq(ll, rr))
        return false;

    *base_out = ll;
    return true;
}

static bool expr_match_affine_power_exact(const expr_t *expr,
                                        size_t nvars,
                                        expr_t *const *vars,
                                        size_t degree,
                                        number_t *constant_out,
                                        number_t *coeffs_out)
{
    NUM_SCOPE(scope);
    const expr_t *arg = NULL;
    const expr_t *mul_base = NULL;
    number_t constant = num_clone(NUM_ZERO);
    number_t degree_num = num_create_from_long((long)degree);
    typedef bool (*expr_affine_power_mul_match_fn)(const expr_t *, const expr_t **);
    static const expr_affine_power_mul_match_fn mul_matchers[] = {
        NULL,
        NULL,
        expr_match_affine_power_mul_deg2,
        expr_match_affine_power_mul_deg3,
        expr_match_affine_power_mul_deg4
    };

    if (!expr || !constant_out || !coeffs_out || (nvars > 0 && !vars))
        return false;

    if (expr_match_unary_kind(expr, EXPR_KIND_POW_D, &arg) &&
        num_eq(expr->c, degree_num)) {
        expr_reset_number_array(coeffs_out, nvars);
        if (!expr_match_affine_term(arg, nvars, vars, NUM_ONE, &constant, coeffs_out)) {
            num_destroy(&constant);
            return false;
        }
        num_destroy(constant_out);
        *constant_out = num_scope_detach(constant);
        return true;
    }

    if (degree < (sizeof(mul_matchers) / sizeof(mul_matchers[0])) &&
        mul_matchers[degree] &&
        mul_matchers[degree](expr, &mul_base)) {
        expr_reset_number_array(coeffs_out, nvars);
        if (!expr_match_affine_term(mul_base, nvars, vars, NUM_ONE, &constant, coeffs_out)) {
            num_destroy(&constant);
            return false;
        }
        num_destroy(constant_out);
        *constant_out = num_scope_detach(constant);
        return true;
    }

    return false;
}

static bool expr_affine_equal(size_t nvars,
                            number_t constant_a,
                            const number_t *coeffs_a,
                            number_t constant_b,
                            const number_t *coeffs_b)
{
    if (!num_eq(constant_a, constant_b))
        return false;
    for (size_t i = 0; i < nvars; ++i)
        if (!num_eq(coeffs_a[i], coeffs_b[i]))
            return false;
    return true;
}

static bool expr_match_affine_power_deg4(const expr_t *expr,
                                       size_t nvars,
                                       expr_t *const *vars,
                                       size_t *degree_out,
                                       number_t *constant_out,
                                       number_t *coeffs_out)
{
    NUM_SCOPE(scope);
    number_t constant = num_clone(NUM_ZERO);

    if (!expr || !degree_out || !constant_out || !coeffs_out || (nvars > 0 && !vars))
        return false;

    if (expr_match_const_value(expr, &constant)) {
        *degree_out = 0;
        num_destroy(constant_out);
        *constant_out = num_scope_detach(num_clone(NUM_ZERO));
        expr_reset_number_array(coeffs_out, nvars);
        return true;
    }

    expr_reset_number_array(coeffs_out, nvars);

    if (expr_match_affine_term(expr, nvars, vars, NUM_ONE, &constant, coeffs_out)) {
        *degree_out = 1;
        num_destroy(constant_out);
        *constant_out = num_scope_detach(constant);
        return true;
    }
    num_destroy(&constant);

    if (expr_match_affine_power_exact(expr, nvars, vars, 2, constant_out, coeffs_out)) {
        *degree_out = 2;
        return true;
    }

    if (expr_match_affine_power_exact(expr, nvars, vars, 3, constant_out, coeffs_out)) {
        *degree_out = 3;
        return true;
    }

    if (expr_match_affine_power_exact(expr, nvars, vars, 4, constant_out, coeffs_out)) {
        *degree_out = 4;
        return true;
    }

    return false;
}

static bool expr_match_scaled_affine_power_deg4(const expr_t *expr,
                                              size_t nvars,
                                              expr_t *const *vars,
                                              number_t *scale_out,
                                              size_t *degree_out,
                                              number_t *constant_out,
                                              number_t *coeffs_out)
{
    NUM_SCOPE(scope);
    number_t inner_scale = num_new();
    number_t const_value = num_new();
    const expr_t *base;

    if (!expr || !scale_out || !degree_out || !constant_out || !coeffs_out)
        return false;

    if (expr_match_const_value(expr, &const_value)) {
        if (!num_is_real(const_value))
            return false;
        num_destroy(scale_out);
        *scale_out = num_scope_detach(const_value);
        *degree_out = 0;
        num_destroy(constant_out);
        *constant_out = num_scope_detach(num_clone(NUM_ZERO));
        expr_reset_number_array(coeffs_out, nvars);
        return true;
    }

    if (expr_match_affine_power_deg4(expr, nvars, vars, degree_out, constant_out, coeffs_out)) {
        num_destroy(scale_out);
        *scale_out = num_scope_detach(num_clone(NUM_ONE));
        return true;
    }

    if (expr_match_scaled_expr(expr, &inner_scale, &base) &&
        num_is_real(inner_scale) &&
        expr_match_affine_power_deg4(base, nvars, vars, degree_out, constant_out, coeffs_out)) {
        num_destroy(scale_out);
        *scale_out = num_scope_detach(inner_scale);
        return true;
    }

    return false;
}

static bool expr_affine_is_zero(size_t nvars, number_t constant, const number_t *coeffs)
{
    if (!num_is_zero(constant))
        return false;
    for (size_t i = 0; i < nvars; ++i)
        if (!num_is_zero(coeffs[i]))
            return false;
    return true;
}

static bool expr_match_affine_poly_deg4_rec(const expr_t *expr,
                                          size_t nvars,
                                          expr_t *const *vars,
                                          number_t *poly_coeffs_out,
                                          number_t *constant_io,
                                          number_t *coeffs_io,
                                          bool *have_basis_io)
{
    NUM_SCOPE(scope);
    number_t subtree_scale = num_new();
    const expr_t *scaled_base = NULL;
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    bool is_sub = false;
    number_t scale = num_new();
    size_t degree;
    number_t term_constant = num_clone(NUM_ZERO);
    number_t *term_coeffs = NULL;
    number_t poly_left[5];
    number_t poly_right[5];
    number_t left_constant = num_clone(*constant_io);
    number_t right_constant = num_clone(*constant_io);
    number_t *left_coeffs = NULL;
    number_t *right_coeffs = NULL;
    bool have_left = *have_basis_io;
    bool have_right = *have_basis_io;
    bool ok = false;

    term_coeffs = malloc(nvars * sizeof(*term_coeffs));
    if ((nvars > 0) && !term_coeffs) {
        num_destroy(&subtree_scale);
        num_destroy(&scale);
        num_destroy(&term_constant);
        num_destroy(&left_constant);
        num_destroy(&right_constant);
        return false;
    }
    for (size_t i = 0; i < nvars; ++i)
        term_coeffs[i] = number_invalid();
    expr_zero_number_array(poly_left, 5);
    expr_zero_number_array(poly_right, 5);

    ok = expr_match_scaled_affine_power_deg4(expr, nvars, vars, &scale, &degree,
                                           &term_constant, term_coeffs);
    if (ok) {
        expr_reset_number_array(poly_coeffs_out, 5);
        if (!num_is_real(scale)) {
            expr_destroy_number_array(term_coeffs, nvars);
            term_coeffs = NULL;
            expr_clear_number_array(poly_left, 5);
            expr_clear_number_array(poly_right, 5);
            num_destroy(&subtree_scale);
            num_destroy(&scale);
            num_destroy(&term_constant);
            num_destroy(&left_constant);
            num_destroy(&right_constant);
            return false;
        }
        num_destroy(&poly_coeffs_out[degree]);
        poly_coeffs_out[degree] = num_scope_detach(num_clone(scale));

        if (degree > 0) {
            if (*have_basis_io &&
                !expr_affine_equal(nvars, *constant_io, coeffs_io, term_constant, term_coeffs)) {
                expr_destroy_number_array(term_coeffs, nvars);
                term_coeffs = NULL;
                expr_clear_number_array(poly_left, 5);
                expr_clear_number_array(poly_right, 5);
                num_destroy(&subtree_scale);
                num_destroy(&scale);
                num_destroy(&term_constant);
                num_destroy(&left_constant);
                num_destroy(&right_constant);
                return false;
            }
            num_destroy(constant_io);
            *constant_io = num_scope_detach(num_clone(term_constant));
            expr_copy_number_array(coeffs_io, term_coeffs, nvars);
            *have_basis_io = true;
        }

        expr_destroy_number_array(term_coeffs, nvars);
        term_coeffs = NULL;
        expr_clear_number_array(poly_left, 5);
        expr_clear_number_array(poly_right, 5);
        num_destroy(&subtree_scale);
        num_destroy(&scale);
        num_destroy(&term_constant);
        num_destroy(&left_constant);
        num_destroy(&right_constant);
        return true;
    }

    expr_destroy_number_array(term_coeffs, nvars);
    term_coeffs = NULL;

    if (expr_match_scaled_expr(expr, &subtree_scale, &scaled_base) &&
        num_is_real(subtree_scale) &&
        expr_match_affine_poly_deg4_rec(scaled_base, nvars, vars, poly_left,
                                      constant_io, coeffs_io, have_basis_io)) {
        for (size_t i = 0; i < 5; ++i) {
            num_destroy(&poly_coeffs_out[i]);
            poly_coeffs_out[i] = num_scope_detach(num_mul(subtree_scale, poly_left[i]));
        }
        expr_clear_number_array(poly_left, 5);
        expr_clear_number_array(poly_right, 5);
        num_destroy(&subtree_scale);
        num_destroy(&scale);
        num_destroy(&term_constant);
        num_destroy(&left_constant);
        num_destroy(&right_constant);
        return true;
    }

    if (expr_match_add_sub_expr(expr, &left, &right, &is_sub)) {
        left_coeffs = expr_clone_number_array(coeffs_io, nvars);
        right_coeffs = expr_clone_number_array(coeffs_io, nvars);
        if ((nvars > 0) && (!left_coeffs || !right_coeffs)) {
            expr_destroy_number_array(left_coeffs, nvars);
            expr_destroy_number_array(right_coeffs, nvars);
            expr_clear_number_array(poly_left, 5);
            expr_clear_number_array(poly_right, 5);
            num_destroy(&subtree_scale);
            num_destroy(&scale);
            num_destroy(&term_constant);
            num_destroy(&left_constant);
            num_destroy(&right_constant);
            return false;
        }
        if (!expr_match_affine_poly_deg4_rec(left, nvars, vars, poly_left,
                                           &left_constant, left_coeffs, &have_left) ||
            !expr_match_affine_poly_deg4_rec(right, nvars, vars, poly_right,
                                           &right_constant, right_coeffs, &have_right)) {
            expr_destroy_number_array(left_coeffs, nvars);
            expr_destroy_number_array(right_coeffs, nvars);
            expr_clear_number_array(poly_left, 5);
            expr_clear_number_array(poly_right, 5);
            num_destroy(&subtree_scale);
            num_destroy(&scale);
            num_destroy(&term_constant);
            num_destroy(&left_constant);
            num_destroy(&right_constant);
            return false;
        }

        if (have_left && have_right &&
            !expr_affine_equal(nvars, left_constant, left_coeffs, right_constant, right_coeffs)) {
            expr_destroy_number_array(left_coeffs, nvars);
            expr_destroy_number_array(right_coeffs, nvars);
            expr_clear_number_array(poly_left, 5);
            expr_clear_number_array(poly_right, 5);
            num_destroy(&subtree_scale);
            num_destroy(&scale);
            num_destroy(&term_constant);
            num_destroy(&left_constant);
            num_destroy(&right_constant);
            return false;
        }

        for (size_t i = 0; i < 5; ++i) {
            num_destroy(&poly_coeffs_out[i]);
            poly_coeffs_out[i] = num_scope_detach(
                is_sub ? num_sub(poly_left[i], poly_right[i])
                       : num_add(poly_left[i], poly_right[i]));
        }

        if (have_left) {
            num_destroy(constant_io);
            *constant_io = num_scope_detach(num_clone(left_constant));
            expr_copy_number_array(coeffs_io, left_coeffs, nvars);
            *have_basis_io = true;
        } else if (have_right) {
            num_destroy(constant_io);
            *constant_io = num_scope_detach(num_clone(right_constant));
            expr_copy_number_array(coeffs_io, right_coeffs, nvars);
            *have_basis_io = true;
        } else {
            num_destroy(constant_io);
            *constant_io = num_scope_detach(num_clone(NUM_ZERO));
            expr_reset_number_array(coeffs_io, nvars);
            *have_basis_io = false;
        }

        expr_destroy_number_array(left_coeffs, nvars);
        expr_destroy_number_array(right_coeffs, nvars);
        expr_clear_number_array(poly_left, 5);
        expr_clear_number_array(poly_right, 5);
        num_destroy(&subtree_scale);
        num_destroy(&scale);
        num_destroy(&term_constant);
        num_destroy(&left_constant);
        num_destroy(&right_constant);
        return true;
    }

    expr_clear_number_array(poly_left, 5);
    expr_clear_number_array(poly_right, 5);
    num_destroy(&subtree_scale);
    num_destroy(&scale);
    num_destroy(&term_constant);
    num_destroy(&left_constant);
    num_destroy(&right_constant);
    return false;
}

static bool expr_match_affine_poly_deg4_num_local(const expr_t *expr,
                                                size_t nvars,
                                                expr_t *const *vars,
                                                number_t *poly_coeffs_out,
                                                number_t *constant_out,
                                                number_t *coeffs_out)
{
    NUM_SCOPE(scope);
    bool have_basis = false;
    number_t constant = num_clone(NUM_ZERO);

    if (!expr || !poly_coeffs_out || !constant_out || !coeffs_out || (nvars > 0 && !vars))
        return false;

    expr_reset_number_array(coeffs_out, nvars);
    expr_reset_number_array(poly_coeffs_out, 5);

    if (!expr_match_affine_poly_deg4_rec(expr, nvars, vars, poly_coeffs_out,
                                       &constant, coeffs_out, &have_basis)) {
        num_destroy(&constant);
        return false;
    }

    num_destroy(constant_out);
    *constant_out = have_basis ? num_scope_detach(constant)
                               : num_scope_detach(num_clone(NUM_ZERO));
    return true;
}

bool expr_match_affine_poly_deg4(const expr_t *expr,
                               size_t nvars,
                               expr_t *const *vars,
                               number_t *poly_coeffs_out,
                               number_t *constant_out,
                               number_t *coeffs_out)
{
    if (!constant_out)
        return false;
    return expr_match_affine_poly_deg4_num_local(expr, nvars, vars,
                                               poly_coeffs_out, constant_out, coeffs_out);
}

static bool expr_match_affine_poly_deg4_times_unary_affine_op(const expr_t *expr,
                                                            expr_op_kind_t kind,
                                                            size_t nvars,
                                                            expr_t *const *vars,
                                                            number_t *poly_coeffs_out,
                                                            number_t *constant_out,
                                                            number_t *coeffs_out)
{
    NUM_SCOPE(scope);
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    number_t unary_constant = num_clone(NUM_ZERO);
    number_t poly_constant = num_clone(NUM_ZERO);
    number_t *unary_coeffs = NULL;
    number_t *poly_coeffs = NULL;
    number_t poly_terms[5];
    bool matched = false;

    if (!expr || !poly_coeffs_out || !constant_out || !coeffs_out || (nvars > 0 && !vars))
        return false;
    if (!expr_match_binary_kind(expr, EXPR_KIND_MUL, &left, &right))
        return false;

    unary_coeffs = malloc(nvars * sizeof(*unary_coeffs));
    poly_coeffs = malloc(nvars * sizeof(*poly_coeffs));
    if ((nvars > 0) && (!unary_coeffs || !poly_coeffs)) {
        free(unary_coeffs);
        free(poly_coeffs);
        return false;
    }
    for (size_t i = 0; i < nvars; ++i) {
        unary_coeffs[i] = number_invalid();
        poly_coeffs[i] = number_invalid();
    }
    expr_zero_number_array(poly_terms, 5);

    if (expr_match_unary_affine_op(right, kind, nvars, vars, &unary_constant, unary_coeffs) &&
        expr_match_affine_poly_deg4_num_local(left, nvars, vars, poly_terms, &poly_constant, poly_coeffs) &&
        (expr_affine_is_zero(nvars, poly_constant, poly_coeffs) ||
         expr_affine_equal(nvars, poly_constant, poly_coeffs, unary_constant, unary_coeffs))) {
        matched = true;
    }

    if (!matched &&
        expr_match_unary_affine_op(left, kind, nvars, vars, &unary_constant, unary_coeffs) &&
        expr_match_affine_poly_deg4_num_local(right, nvars, vars, poly_terms, &poly_constant, poly_coeffs) &&
        (expr_affine_is_zero(nvars, poly_constant, poly_coeffs) ||
         expr_affine_equal(nvars, poly_constant, poly_coeffs, unary_constant, unary_coeffs))) {
        matched = true;
    }

    if (matched) {
        num_destroy(constant_out);
        *constant_out = num_scope_detach(num_clone(unary_constant));
        expr_copy_number_array(coeffs_out, unary_coeffs, nvars);
        expr_copy_number_array(poly_coeffs_out, poly_terms, 5);
    }

    expr_destroy_number_array(unary_coeffs, nvars);
    expr_destroy_number_array(poly_coeffs, nvars);
    expr_clear_number_array(poly_terms, 5);
    num_destroy(&unary_constant);
    num_destroy(&poly_constant);
    return matched;
}

static bool expr_match_affine_poly_deg4_times_unary_affine_kind_num_local(const expr_t *expr,
                                                                        expr_pattern_unary_affine_kind_t kind,
                                                                        size_t nvars,
                                                                        expr_t *const *vars,
                                                                        number_t *poly_coeffs_out,
                                                                        number_t *constant_out,
                                                                        number_t *coeffs_out)
{
    expr_op_kind_t op_kind;

    if (!expr_pattern_unary_kind_to_op(kind, &op_kind))
        return false;

    return expr_match_affine_poly_deg4_times_unary_affine_op(expr, op_kind, nvars, vars,
                                                           poly_coeffs_out, constant_out,
                                                           coeffs_out);
}

bool expr_match_affine_poly_deg4_times_unary_affine_kind(const expr_t *expr,
                                                       expr_pattern_unary_affine_kind_t kind,
                                                       size_t nvars,
                                                       expr_t *const *vars,
                                                       number_t *poly_coeffs_out,
                                                       number_t *constant_out,
                                                       number_t *coeffs_out)
{
    if (!constant_out)
        return false;
    return expr_match_affine_poly_deg4_times_unary_affine_kind_num_local(
        expr, kind, nvars, vars, poly_coeffs_out, constant_out, coeffs_out);
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

    if (expr_match_unary_kind(expr, EXPR_KIND_NEG, &arg)) {
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

    if (expr_match_binary_kind(expr, EXPR_KIND_MUL, &left, &right)) {
        if (expr_match_scaled_inner(left, right, scale_out, base_out) ||
            expr_match_scaled_inner(right, left, scale_out, base_out))
            return true;
    }

    if (expr_match_binary_kind(expr, EXPR_KIND_DIV, &left, &right) &&
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
    if (expr_match_binary_kind(expr, EXPR_KIND_ADD, left_out, right_out)) {
        *is_sub_out = false;
        return true;
    }
    if (expr_match_binary_kind(expr, EXPR_KIND_SUB, left_out, right_out)) {
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
    if (!expr_match_binary_kind(expr, EXPR_KIND_MUL, left_out, right_out))
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

    if (expr == needle) {
        expr_retain(replacement);
        return replacement;
    }

    if (expr_match_const_leaf(expr, NULL, &name)) {
        if (name)
            return expr_new_named_const(expr->c, name);
        return expr_new_const(expr->c);
    }

    if (expr_match_var_leaf(expr, NULL, &name)) {
        if (name)
            return expr_new_named_var(expr->x, name);
        return expr_new_var(expr->x);
    }

    if (expr_match_unary_kind(expr, EXPR_KIND_POW_D, &arg)) {
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
