#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include "dval_internal.h"
#include "dval.h"
#include "internal/dval_internal.h"

static bool is_kind(const dval_t *expr, dval_op_kind_t kind)
{
    return expr && expr->ops && expr->ops->kind == kind;
}

bool dv_is_exact_zero(const dval_t *dv)
{
    return is_kind(dv, DV_KIND_CONST) &&
           !dv->name &&
           num_eq(dv->c, NUM_ZERO);
}

bool dv_is_named_const(const dval_t *dv)
{
    return is_kind(dv, DV_KIND_CONST) && dv->name && *dv->name;
}

static bool dv_match_const_leaf(const dval_t *expr, number_t *value_out, const char **name_out)
{
    if (!is_kind(expr, DV_KIND_CONST))
        return false;
    if (value_out)
        *value_out = num_clone(expr->c);
    if (name_out)
        *name_out = (expr->name && *expr->name) ? expr->name : NULL;
    return true;
}

static bool dv_match_var_leaf(const dval_t *expr, number_t *value_out, const char **name_out)
{
    if (!is_kind(expr, DV_KIND_VAR))
        return false;
    if (value_out)
        *value_out = num_clone(expr->c);
    if (name_out)
        *name_out = (expr->name && *expr->name) ? expr->name : NULL;
    return true;
}

static bool dv_match_unnamed_const_leaf(const dval_t *expr, number_t *value_out)
{
    const char *name;
    return value_out && dv_match_const_leaf(expr, value_out, &name) && !name;
}

static bool dv_match_unary_kind(const dval_t *expr, dval_op_kind_t kind, const dval_t **arg_out)
{
    if (!is_kind(expr, kind) || !expr->a)
        return false;
    if (arg_out)
        *arg_out = expr->a;
    return true;
}

static bool dv_match_binary_kind(const dval_t *expr,
                               dval_op_kind_t kind,
                               const dval_t **left_out,
                               const dval_t **right_out)
{
    if (!is_kind(expr, kind) || !expr->a || !expr->b)
        return false;
    if (left_out)
        *left_out = expr->a;
    if (right_out)
        *right_out = expr->b;
    return true;
}

static bool dv_match_scaled_inner(const dval_t *factor,
                                  const dval_t *other,
                                  number_t *scale_out,
                                  const dval_t **base_out)
{
    number_t factor_value = num_new();
    number_t inner_scale = num_new();
    const dval_t *inner_base;

    if (!dv_is_unnamed_const(factor) || !dv_match_const_value(factor, &factor_value)) {
        num_destroy(&factor_value);
        num_destroy(&inner_scale);
        return false;
    }
    if (dv_match_scaled_expr(other, &inner_scale, &inner_base)) {
        *scale_out = num_mul(factor_value, inner_scale);
        num_destroy(&factor_value);
        num_destroy(&inner_scale);
        *base_out = inner_base;
        return true;
    }
    *scale_out = factor_value;
    num_destroy(&inner_scale);
    *base_out = other;
    return true;
}

static int dv_match_var_index(size_t nvars, dval_t *const *vars, const dval_t *dv)
{
    for (size_t i = 0; i < nvars; ++i)
        if (vars[i] == dv)
            return (int)i;
    return -1;
}

bool dv_match_var_expr(const dval_t *expr,
                       size_t nvars,
                       dval_t *const *vars,
                       size_t *index_out)
{
    int idx;

    if (!expr || !vars || !index_out)
        return false;

    idx = dv_match_var_index(nvars, vars, expr);
    if (idx < 0)
        return false;

    *index_out = (size_t)idx;
    return true;
}

static void dv_zero_number_array(number_t *values, size_t n)
{
    for (size_t i = 0; i < n; ++i)
        values[i] = num_clone(NUM_ZERO);
}

static void dv_reset_number_array(number_t *values, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        num_destroy(&values[i]);
        values[i] = num_clone(NUM_ZERO);
    }
}

static void dv_clear_number_array(number_t *values, size_t n)
{
    if (!values)
        return;
    for (size_t i = 0; i < n; ++i)
        num_destroy(&values[i]);
}

static void dv_destroy_number_array(number_t *values, size_t n)
{
    if (!values)
        return;
    dv_clear_number_array(values, n);
    free(values);
}

static number_t *dv_clone_number_array(const number_t *values, size_t n)
{
    number_t *copy = malloc(n * sizeof(*copy));

    if ((n > 0) && !copy)
        return NULL;
    for (size_t i = 0; i < n; ++i)
        copy[i] = num_clone(values[i]);
    return copy;
}

static void dv_copy_number_array(number_t *dst, const number_t *src, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        num_destroy(&dst[i]);
        dst[i] = num_clone(src[i]);
    }
}

static bool dv_match_affine_term(const dval_t *dv,
                                 size_t nvars,
                                 dval_t *const *vars,
                                 number_t scale,
                                 number_t *constant_io,
                                 number_t *coeffs_io)
{
    number_t constant = num_new();
    number_t inner_scale = num_new();
    const dval_t *base;
    const dval_t *left;
    const dval_t *right;
    bool is_sub;
    size_t idx;

    if (!dv)
        return false;

    if (dv_match_const_value(dv, &constant)) {
        number_t product;
        number_t sum;

        if (!num_is_real(constant) || !num_is_real(scale)) {
            num_destroy(&constant);
            num_destroy(&inner_scale);
            return false;
        }
        product = num_mul(scale, constant);
        sum = num_add(*constant_io, product);
        num_destroy(&product);
        num_destroy(constant_io);
        *constant_io = sum;
        num_destroy(&constant);
        num_destroy(&inner_scale);
        return true;
    }

    if (dv_match_var_expr(dv, nvars, vars, &idx)) {
        if (!num_is_real(scale)) {
            num_destroy(&constant);
            num_destroy(&inner_scale);
            return false;
        }
        {
            number_t sum = num_add(coeffs_io[idx], scale);

            num_destroy(&coeffs_io[idx]);
            coeffs_io[idx] = sum;
        }
        num_destroy(&constant);
        num_destroy(&inner_scale);
        return true;
    }

    if (dv_match_add_sub_expr(dv, &left, &right, &is_sub)) {
        number_t right_scale = is_sub ? num_neg(scale) : num_clone(scale);
        bool ok = dv_match_affine_term(left, nvars, vars, scale, constant_io, coeffs_io) &&
                  dv_match_affine_term(right, nvars, vars, right_scale, constant_io, coeffs_io);

        num_destroy(&constant);
        num_destroy(&inner_scale);
        num_destroy(&right_scale);
        return ok;
    }

    if (dv_match_scaled_expr(dv, &inner_scale, &base)) {
        number_t product;
        bool ok;

        if (!num_is_real(inner_scale) || !num_is_real(scale)) {
            num_destroy(&constant);
            num_destroy(&inner_scale);
            return false;
        }
        product = num_mul(scale, inner_scale);
        ok = dv_match_affine_term(base, nvars, vars, product, constant_io, coeffs_io);
        num_destroy(&constant);
        num_destroy(&inner_scale);
        num_destroy(&product);
        return ok;
    }

    num_destroy(&constant);
    num_destroy(&inner_scale);
    return false;
}

static bool dv_pattern_unary_kind_to_op(dv_pattern_unary_affine_kind_t kind,
                                        dval_op_kind_t *op_kind_out)
{
    static const dval_op_kind_t unary_op_table[] = {
        DV_KIND_EXP,
        DV_KIND_SIN,
        DV_KIND_COS,
        DV_KIND_SINH,
        DV_KIND_COSH
    };

    if (!op_kind_out)
        return false;

    if ((unsigned)kind >= sizeof(unary_op_table) / sizeof(unary_op_table[0]))
        return false;

    *op_kind_out = unary_op_table[kind];
    return true;
}

static bool dv_match_unary_affine_op(const dval_t *expr,
                                     dval_op_kind_t op_kind,
                                     size_t nvars,
                                     dval_t *const *vars,
                                     number_t *constant_out,
                                     number_t *coeffs_out)
{
    number_t constant = num_clone(NUM_ZERO);
    bool ok;

    if (!expr || !constant_out || !coeffs_out || (nvars > 0 && !vars))
        return false;

    if (!is_kind(expr, op_kind) || !expr->a)
        return false;

    dv_zero_number_array(coeffs_out, nvars);

    ok = dv_match_affine_term(expr->a, nvars, vars, NUM_ONE, &constant, coeffs_out);
    if (!ok) {
        num_destroy(&constant);
        return false;
    }

    num_destroy(constant_out);
    *constant_out = constant;
    return true;
}

static bool dv_match_unary_affine_kind_num_local(const dval_t *expr,
                                                 dv_pattern_unary_affine_kind_t kind,
                                                 size_t nvars,
                                                 dval_t *const *vars,
                                                 number_t *constant_out,
                                                 number_t *coeffs_out)
{
    dval_op_kind_t op_kind;

    if (!dv_pattern_unary_kind_to_op(kind, &op_kind))
        return false;

    return dv_match_unary_affine_op(expr, op_kind, nvars, vars, constant_out, coeffs_out);
}

bool dv_match_unary_affine_kind(const dval_t *expr,
                                dv_pattern_unary_affine_kind_t kind,
                                size_t nvars,
                                dval_t *const *vars,
                                number_t *constant_out,
                                number_t *coeffs_out)
{
    if (!constant_out)
        return false;
    return dv_match_unary_affine_kind_num_local(expr, kind, nvars, vars, constant_out, coeffs_out);
}

static bool dv_match_affine_power_exact(const dval_t *expr,
                                        size_t nvars,
                                        dval_t *const *vars,
                                        size_t degree,
                                        number_t *constant_out,
                                        number_t *coeffs_out)
{
    const dval_t *arg = NULL;
    const dval_t *left = NULL;
    const dval_t *right = NULL;
    const dval_t *inner_left = NULL;
    const dval_t *inner_right = NULL;
    const dval_t *ll = NULL;
    const dval_t *lr = NULL;
    const dval_t *rl = NULL;
    const dval_t *rr = NULL;
    number_t constant = num_clone(NUM_ZERO);
    number_t degree_num = num_create_from_long((long)degree);

    if (!expr || !constant_out || !coeffs_out || (nvars > 0 && !vars))
        return false;

    if (dv_match_unary_kind(expr, DV_KIND_POW_D, &arg) &&
        num_eq(expr->c, degree_num)) {
        dv_zero_number_array(coeffs_out, nvars);
        if (!dv_match_affine_term(arg, nvars, vars, NUM_ONE, &constant, coeffs_out)) {
            num_destroy(&degree_num);
            num_destroy(&constant);
            return false;
        }
        num_destroy(constant_out);
        *constant_out = constant;
        num_destroy(&degree_num);
        return true;
    }
    num_destroy(&degree_num);

    switch (degree) {
    case 2:
        if (dv_match_binary_kind(expr, DV_KIND_MUL, &left, &right) &&
            dv_struct_eq(left, right)) {
            dv_zero_number_array(coeffs_out, nvars);
            if (!dv_match_affine_term(left, nvars, vars, NUM_ONE, &constant, coeffs_out)) {
                num_destroy(&constant);
                return false;
            }
            num_destroy(constant_out);
            *constant_out = constant;
            return true;
        }
        break;
    case 3:
        if (dv_match_binary_kind(expr, DV_KIND_MUL, &left, &right) &&
            dv_match_binary_kind(left, DV_KIND_MUL, &inner_left, &inner_right) &&
            dv_struct_eq(inner_left, inner_right) &&
            dv_struct_eq(inner_left, right)) {
            dv_zero_number_array(coeffs_out, nvars);
            if (!dv_match_affine_term(inner_left, nvars, vars, NUM_ONE, &constant, coeffs_out)) {
                num_destroy(&constant);
                return false;
            }
            num_destroy(constant_out);
            *constant_out = constant;
            return true;
        }

        if (dv_match_binary_kind(expr, DV_KIND_MUL, &left, &right) &&
            dv_match_binary_kind(right, DV_KIND_MUL, &inner_left, &inner_right) &&
            dv_struct_eq(left, inner_left) &&
            dv_struct_eq(left, inner_right)) {
            dv_zero_number_array(coeffs_out, nvars);
            if (!dv_match_affine_term(left, nvars, vars, NUM_ONE, &constant, coeffs_out)) {
                num_destroy(&constant);
                return false;
            }
            num_destroy(constant_out);
            *constant_out = constant;
            return true;
        }
        break;
    case 4:
        if (dv_match_binary_kind(expr, DV_KIND_MUL, &left, &right) &&
            dv_match_binary_kind(left, DV_KIND_MUL, &ll, &lr) &&
            dv_match_binary_kind(right, DV_KIND_MUL, &rl, &rr) &&
            dv_struct_eq(ll, lr) &&
            dv_struct_eq(ll, rl) &&
            dv_struct_eq(ll, rr)) {
            dv_zero_number_array(coeffs_out, nvars);
            if (!dv_match_affine_term(ll, nvars, vars, NUM_ONE, &constant, coeffs_out)) {
                num_destroy(&constant);
                return false;
            }
            num_destroy(constant_out);
            *constant_out = constant;
            return true;
        }
        break;
    default:
        break;
    }

    return false;
}

static bool dv_affine_equal(size_t nvars,
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

static bool dv_match_affine_power_deg4(const dval_t *expr,
                                       size_t nvars,
                                       dval_t *const *vars,
                                       size_t *degree_out,
                                       number_t *constant_out,
                                       number_t *coeffs_out)
{
    number_t constant = num_clone(NUM_ZERO);

    if (!expr || !degree_out || !constant_out || !coeffs_out || (nvars > 0 && !vars))
        return false;

    if (dv_match_const_value(expr, &constant)) {
        *degree_out = 0;
        num_destroy(constant_out);
        *constant_out = num_clone(NUM_ZERO);
        num_destroy(&constant);
        dv_zero_number_array(coeffs_out, nvars);
        return true;
    }

    dv_zero_number_array(coeffs_out, nvars);

    if (dv_match_affine_term(expr, nvars, vars, NUM_ONE, &constant, coeffs_out)) {
        *degree_out = 1;
        num_destroy(constant_out);
        *constant_out = constant;
        return true;
    }

    if (dv_match_affine_power_exact(expr, nvars, vars, 2, constant_out, coeffs_out)) {
        *degree_out = 2;
        return true;
    }

    if (dv_match_affine_power_exact(expr, nvars, vars, 3, constant_out, coeffs_out)) {
        *degree_out = 3;
        return true;
    }

    if (dv_match_affine_power_exact(expr, nvars, vars, 4, constant_out, coeffs_out)) {
        *degree_out = 4;
        return true;
    }

    num_destroy(&constant);
    return false;
}

static bool dv_match_scaled_affine_power_deg4(const dval_t *expr,
                                              size_t nvars,
                                              dval_t *const *vars,
                                              number_t *scale_out,
                                              size_t *degree_out,
                                              number_t *constant_out,
                                              number_t *coeffs_out)
{
    number_t inner_scale = num_new();
    number_t const_value = num_new();
    const dval_t *base;

    if (!expr || !scale_out || !degree_out || !constant_out || !coeffs_out)
        return false;

    if (dv_match_const_value(expr, &const_value)) {
        if (!num_is_real(const_value)) {
            num_destroy(&inner_scale);
            num_destroy(&const_value);
            return false;
        }
        num_destroy(scale_out);
        *scale_out = const_value;
        *degree_out = 0;
        num_destroy(constant_out);
        *constant_out = num_clone(NUM_ZERO);
        dv_zero_number_array(coeffs_out, nvars);
        num_destroy(&inner_scale);
        return true;
    }

    if (dv_match_affine_power_deg4(expr, nvars, vars, degree_out, constant_out, coeffs_out)) {
        num_destroy(scale_out);
        *scale_out = num_clone(NUM_ONE);
        num_destroy(&inner_scale);
        num_destroy(&const_value);
        return true;
    }

    if (dv_match_scaled_expr(expr, &inner_scale, &base) &&
        num_is_real(inner_scale) &&
        dv_match_affine_power_deg4(base, nvars, vars, degree_out, constant_out, coeffs_out)) {
        num_destroy(scale_out);
        *scale_out = inner_scale;
        num_destroy(&const_value);
        return true;
    }

    num_destroy(&inner_scale);
    num_destroy(&const_value);
    return false;
}

static bool dv_affine_is_zero(size_t nvars, number_t constant, const number_t *coeffs)
{
    if (!num_is_zero(constant))
        return false;
    for (size_t i = 0; i < nvars; ++i)
        if (!num_is_zero(coeffs[i]))
            return false;
    return true;
}

static bool dv_match_affine_poly_deg4_rec(const dval_t *expr,
                                          size_t nvars,
                                          dval_t *const *vars,
                                          number_t *poly_coeffs_out,
                                          number_t *constant_io,
                                          number_t *coeffs_io,
                                          bool *have_basis_io)
{
    number_t subtree_scale = num_new();
    const dval_t *scaled_base = NULL;
    const dval_t *left = NULL;
    const dval_t *right = NULL;
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
    dv_zero_number_array(poly_left, 5);
    dv_zero_number_array(poly_right, 5);

    ok = dv_match_scaled_affine_power_deg4(expr, nvars, vars, &scale, &degree,
                                           &term_constant, term_coeffs);
    if (ok) {
        dv_reset_number_array(poly_coeffs_out, 5);
        if (!num_is_real(scale)) {
            free(term_coeffs);
            dv_clear_number_array(poly_left, 5);
            dv_clear_number_array(poly_right, 5);
            num_destroy(&subtree_scale);
            num_destroy(&scale);
            num_destroy(&term_constant);
            num_destroy(&left_constant);
            num_destroy(&right_constant);
            return false;
        }
        num_destroy(&poly_coeffs_out[degree]);
        poly_coeffs_out[degree] = num_clone(scale);

        if (degree > 0) {
            if (*have_basis_io &&
                !dv_affine_equal(nvars, *constant_io, coeffs_io, term_constant, term_coeffs)) {
                free(term_coeffs);
                dv_clear_number_array(poly_left, 5);
                dv_clear_number_array(poly_right, 5);
                num_destroy(&subtree_scale);
                num_destroy(&scale);
                num_destroy(&term_constant);
                num_destroy(&left_constant);
                num_destroy(&right_constant);
                return false;
            }
            num_destroy(constant_io);
            *constant_io = num_clone(term_constant);
            dv_copy_number_array(coeffs_io, term_coeffs, nvars);
            *have_basis_io = true;
        }

        dv_destroy_number_array(term_coeffs, nvars);
        dv_clear_number_array(poly_left, 5);
        dv_clear_number_array(poly_right, 5);
        num_destroy(&subtree_scale);
        num_destroy(&scale);
        num_destroy(&term_constant);
        num_destroy(&left_constant);
        num_destroy(&right_constant);
        return true;
    }

    free(term_coeffs);

    if (dv_match_scaled_expr(expr, &subtree_scale, &scaled_base) &&
        num_is_real(subtree_scale) &&
        dv_match_affine_poly_deg4_rec(scaled_base, nvars, vars, poly_left,
                                      constant_io, coeffs_io, have_basis_io)) {
        for (size_t i = 0; i < 5; ++i) {
            num_destroy(&poly_coeffs_out[i]);
            poly_coeffs_out[i] = num_mul(subtree_scale, poly_left[i]);
        }
        dv_clear_number_array(poly_left, 5);
        dv_clear_number_array(poly_right, 5);
        num_destroy(&subtree_scale);
        num_destroy(&scale);
        num_destroy(&term_constant);
        num_destroy(&left_constant);
        num_destroy(&right_constant);
        return true;
    }

    if (dv_match_add_sub_expr(expr, &left, &right, &is_sub)) {
        left_coeffs = dv_clone_number_array(coeffs_io, nvars);
        right_coeffs = dv_clone_number_array(coeffs_io, nvars);
        if ((nvars > 0) && (!left_coeffs || !right_coeffs)) {
            dv_destroy_number_array(left_coeffs, nvars);
            dv_destroy_number_array(right_coeffs, nvars);
            dv_clear_number_array(poly_left, 5);
            dv_clear_number_array(poly_right, 5);
            num_destroy(&subtree_scale);
            num_destroy(&scale);
            num_destroy(&term_constant);
            num_destroy(&left_constant);
            num_destroy(&right_constant);
            return false;
        }
        if (!dv_match_affine_poly_deg4_rec(left, nvars, vars, poly_left,
                                           &left_constant, left_coeffs, &have_left) ||
            !dv_match_affine_poly_deg4_rec(right, nvars, vars, poly_right,
                                           &right_constant, right_coeffs, &have_right)) {
            dv_destroy_number_array(left_coeffs, nvars);
            dv_destroy_number_array(right_coeffs, nvars);
            dv_clear_number_array(poly_left, 5);
            dv_clear_number_array(poly_right, 5);
            num_destroy(&subtree_scale);
            num_destroy(&scale);
            num_destroy(&term_constant);
            num_destroy(&left_constant);
            num_destroy(&right_constant);
            return false;
        }

        if (have_left && have_right &&
            !dv_affine_equal(nvars, left_constant, left_coeffs, right_constant, right_coeffs)) {
            dv_destroy_number_array(left_coeffs, nvars);
            dv_destroy_number_array(right_coeffs, nvars);
            dv_clear_number_array(poly_left, 5);
            dv_clear_number_array(poly_right, 5);
            num_destroy(&subtree_scale);
            num_destroy(&scale);
            num_destroy(&term_constant);
            num_destroy(&left_constant);
            num_destroy(&right_constant);
            return false;
        }

        for (size_t i = 0; i < 5; ++i) {
            num_destroy(&poly_coeffs_out[i]);
            poly_coeffs_out[i] = is_sub ? num_sub(poly_left[i], poly_right[i])
                                        : num_add(poly_left[i], poly_right[i]);
        }

        if (have_left) {
            num_destroy(constant_io);
            *constant_io = num_clone(left_constant);
            dv_copy_number_array(coeffs_io, left_coeffs, nvars);
            *have_basis_io = true;
        } else if (have_right) {
            num_destroy(constant_io);
            *constant_io = num_clone(right_constant);
            dv_copy_number_array(coeffs_io, right_coeffs, nvars);
            *have_basis_io = true;
        } else {
            num_destroy(constant_io);
            *constant_io = num_clone(NUM_ZERO);
            dv_reset_number_array(coeffs_io, nvars);
            *have_basis_io = false;
        }

        dv_destroy_number_array(left_coeffs, nvars);
        dv_destroy_number_array(right_coeffs, nvars);
        dv_clear_number_array(poly_left, 5);
        dv_clear_number_array(poly_right, 5);
        num_destroy(&subtree_scale);
        num_destroy(&scale);
        num_destroy(&term_constant);
        num_destroy(&left_constant);
        num_destroy(&right_constant);
        return true;
    }

    dv_clear_number_array(poly_left, 5);
    dv_clear_number_array(poly_right, 5);
    num_destroy(&subtree_scale);
    num_destroy(&scale);
    num_destroy(&term_constant);
    num_destroy(&left_constant);
    num_destroy(&right_constant);
    return false;
}

static bool dv_match_affine_poly_deg4_num_local(const dval_t *expr,
                                                size_t nvars,
                                                dval_t *const *vars,
                                                number_t *poly_coeffs_out,
                                                number_t *constant_out,
                                                number_t *coeffs_out)
{
    bool have_basis = false;
    number_t constant = num_clone(NUM_ZERO);

    if (!expr || !poly_coeffs_out || !constant_out || !coeffs_out || (nvars > 0 && !vars))
        return false;

    dv_zero_number_array(coeffs_out, nvars);
    dv_zero_number_array(poly_coeffs_out, 5);

    if (!dv_match_affine_poly_deg4_rec(expr, nvars, vars, poly_coeffs_out,
                                       &constant, coeffs_out, &have_basis))
        return false;

    num_destroy(constant_out);
    *constant_out = have_basis ? constant : num_clone(NUM_ZERO);
    return true;
}

bool dv_match_affine_poly_deg4(const dval_t *expr,
                               size_t nvars,
                               dval_t *const *vars,
                               number_t *poly_coeffs_out,
                               number_t *constant_out,
                               number_t *coeffs_out)
{
    if (!constant_out)
        return false;
    return dv_match_affine_poly_deg4_num_local(expr, nvars, vars,
                                               poly_coeffs_out, constant_out, coeffs_out);
}

static bool dv_match_affine_poly_deg4_times_unary_affine_op(const dval_t *expr,
                                                            dval_op_kind_t kind,
                                                            size_t nvars,
                                                            dval_t *const *vars,
                                                            number_t *poly_coeffs_out,
                                                            number_t *constant_out,
                                                            number_t *coeffs_out)
{
    const dval_t *left = NULL;
    const dval_t *right = NULL;
    number_t unary_constant = num_clone(NUM_ZERO);
    number_t poly_constant = num_clone(NUM_ZERO);
    number_t *unary_coeffs = NULL;
    number_t *poly_coeffs = NULL;
    number_t poly_terms[5];
    bool matched = false;

    if (!expr || !poly_coeffs_out || !constant_out || !coeffs_out || (nvars > 0 && !vars))
        return false;
    if (!dv_match_binary_kind(expr, DV_KIND_MUL, &left, &right))
        return false;

    unary_coeffs = malloc(nvars * sizeof(*unary_coeffs));
    poly_coeffs = malloc(nvars * sizeof(*poly_coeffs));
    if ((nvars > 0) && (!unary_coeffs || !poly_coeffs)) {
        dv_destroy_number_array(unary_coeffs, nvars);
        dv_destroy_number_array(poly_coeffs, nvars);
        return false;
    }
    dv_zero_number_array(poly_terms, 5);

    if (dv_match_unary_affine_op(right, kind, nvars, vars, &unary_constant, unary_coeffs) &&
        dv_match_affine_poly_deg4_num_local(left, nvars, vars, poly_terms, &poly_constant, poly_coeffs) &&
        (dv_affine_is_zero(nvars, poly_constant, poly_coeffs) ||
         dv_affine_equal(nvars, poly_constant, poly_coeffs, unary_constant, unary_coeffs))) {
        matched = true;
    }

    if (!matched &&
        dv_match_unary_affine_op(left, kind, nvars, vars, &unary_constant, unary_coeffs) &&
        dv_match_affine_poly_deg4_num_local(right, nvars, vars, poly_terms, &poly_constant, poly_coeffs) &&
        (dv_affine_is_zero(nvars, poly_constant, poly_coeffs) ||
         dv_affine_equal(nvars, poly_constant, poly_coeffs, unary_constant, unary_coeffs))) {
        matched = true;
    }

    if (matched) {
        num_destroy(constant_out);
        *constant_out = num_clone(unary_constant);
        dv_copy_number_array(coeffs_out, unary_coeffs, nvars);
        dv_copy_number_array(poly_coeffs_out, poly_terms, 5);
    }

    num_destroy(&unary_constant);
    num_destroy(&poly_constant);
    dv_destroy_number_array(unary_coeffs, nvars);
    dv_destroy_number_array(poly_coeffs, nvars);
    dv_clear_number_array(poly_terms, 5);
    return matched;
}

static bool dv_match_affine_poly_deg4_times_unary_affine_kind_num_local(const dval_t *expr,
                                                                        dv_pattern_unary_affine_kind_t kind,
                                                                        size_t nvars,
                                                                        dval_t *const *vars,
                                                                        number_t *poly_coeffs_out,
                                                                        number_t *constant_out,
                                                                        number_t *coeffs_out)
{
    dval_op_kind_t op_kind;

    if (!dv_pattern_unary_kind_to_op(kind, &op_kind))
        return false;

    return dv_match_affine_poly_deg4_times_unary_affine_op(expr, op_kind, nvars, vars,
                                                           poly_coeffs_out, constant_out,
                                                           coeffs_out);
}

bool dv_match_affine_poly_deg4_times_unary_affine_kind(const dval_t *expr,
                                                       dv_pattern_unary_affine_kind_t kind,
                                                       size_t nvars,
                                                       dval_t *const *vars,
                                                       number_t *poly_coeffs_out,
                                                       number_t *constant_out,
                                                       number_t *coeffs_out)
{
    if (!constant_out)
        return false;
    return dv_match_affine_poly_deg4_times_unary_affine_kind_num_local(
        expr, kind, nvars, vars, poly_coeffs_out, constant_out, coeffs_out);
}

bool dv_match_const_value(const dval_t *expr, number_t *value_out)
{
    return dv_match_unnamed_const_leaf(expr, value_out);
}

bool dv_match_scaled_expr(const dval_t *expr,
                          number_t *scale_out,
                          const dval_t **base_out)
{
    number_t inner_scale;
    number_t const_value = num_new();
    const dval_t *inner_base;
    const dval_t *left;
    const dval_t *right;
    const dval_t *arg;

    if (!expr || !scale_out || !base_out)
        return false;

    if (dv_match_unary_kind(expr, DV_KIND_NEG, &arg)) {
        inner_scale = num_new();
        if (dv_match_scaled_expr(arg, &inner_scale, &inner_base)) {
            *scale_out = num_neg(inner_scale);
            num_destroy(&inner_scale);
            *base_out = inner_base;
            return true;
        }
        num_destroy(&inner_scale);
        *scale_out = NUM_NEG_ONE;
        *base_out = arg;
        return true;
    }

    if (dv_match_binary_kind(expr, DV_KIND_MUL, &left, &right)) {
        if (dv_match_scaled_inner(left, right, scale_out, base_out) ||
            dv_match_scaled_inner(right, left, scale_out, base_out))
            return true;
    }

    if (dv_match_binary_kind(expr, DV_KIND_DIV, &left, &right) &&
        dv_match_unnamed_const_leaf(right, &const_value)) {
        if (num_eq(const_value, NUM_ZERO)) {
            num_destroy(&const_value);
            return false;
        }
        inner_scale = num_new();
        if (dv_match_scaled_expr(left, &inner_scale, &inner_base)) {
            *scale_out = num_div(inner_scale, const_value);
            num_destroy(&inner_scale);
            num_destroy(&const_value);
            *base_out = inner_base;
            return true;
        }
        num_destroy(&inner_scale);
        *scale_out = num_div(NUM_ONE, const_value);
        num_destroy(&const_value);
        *base_out = left;
        return true;
    }

    return false;
}

bool dv_match_add_sub_expr(const dval_t *expr,
                           const dval_t **left_out,
                           const dval_t **right_out,
                           bool *is_sub_out)
{
    if (!expr || !left_out || !right_out || !is_sub_out)
        return false;
    if (dv_match_binary_kind(expr, DV_KIND_ADD, left_out, right_out)) {
        *is_sub_out = false;
        return true;
    }
    if (dv_match_binary_kind(expr, DV_KIND_SUB, left_out, right_out)) {
        *is_sub_out = true;
        return true;
    }
    return false;
}

bool dv_match_mul_expr(const dval_t *expr,
                       const dval_t **left_out,
                       const dval_t **right_out)
{
    if (!expr || !left_out || !right_out)
        return false;
    if (!dv_match_binary_kind(expr, DV_KIND_MUL, left_out, right_out))
        return false;
    return true;
}

static bool dv_collect_var_usage_impl(const dval_t *expr,
                                      size_t nvars,
                                      dval_t *const *vars,
                                      bool *used_out)
{
    size_t idx;

    if (!expr)
        return true;

    if (dv_match_var_expr(expr, nvars, vars, &idx)) {
        used_out[idx] = true;
        return true;
    }

    if (expr->a && !dv_collect_var_usage_impl(expr->a, nvars, vars, used_out))
        return false;
    if (expr->b && !dv_collect_var_usage_impl(expr->b, nvars, vars, used_out))
        return false;
    return true;
}

bool dv_collect_var_usage(const dval_t *expr,
                          size_t nvars,
                          dval_t *const *vars,
                          bool *used_out)
{
    if (!used_out || (nvars > 0 && !vars))
        return false;
    for (size_t i = 0; i < nvars; ++i)
        used_out[i] = false;
    return dv_collect_var_usage_impl(expr, nvars, vars, used_out);
}

dval_t *dv_substitute(const dval_t *expr,
                      const dval_t *needle,
                      dval_t *replacement)
{
    dval_t *left;
    dval_t *right;
    dval_t *out;
    const char *name;
    const dval_t *arg;

    if (!expr)
        return NULL;

    if (expr == needle) {
        dv_retain(replacement);
        return replacement;
    }

    if (dv_match_const_leaf(expr, NULL, &name)) {
        if (name)
            return dv_new_named_const(expr->c, name);
        return dv_new_const(expr->c);
    }

    if (dv_match_var_leaf(expr, NULL, &name)) {
        if (name)
            return dv_new_named_var(expr->x, name);
        return dv_new_var(expr->x);
    }

    if (dv_match_unary_kind(expr, DV_KIND_POW_D, &arg)) {
        left = dv_substitute(arg, needle, replacement);
        if (!left)
            return NULL;
        out = dv_pow(left, &expr->c);
        dv_free(left);
        return out;
    }

    if (expr->ops->arity == DV_OP_UNARY && expr->ops->apply_unary) {
        left = dv_substitute(expr->a, needle, replacement);
        if (!left)
            return NULL;
        out = expr->ops->apply_unary(left);
        dv_free(left);
        return out;
    }

    if (expr->ops->arity == DV_OP_BINARY && expr->ops->apply_binary) {
        left = dv_substitute(expr->a, needle, replacement);
        right = dv_substitute(expr->b, needle, replacement);
        if (!left || !right) {
            dv_free(left);
            dv_free(right);
            return NULL;
        }
        out = expr->ops->apply_binary(left, right);
        dv_free(left);
        dv_free(right);
        return out;
    }

    return NULL;
}
