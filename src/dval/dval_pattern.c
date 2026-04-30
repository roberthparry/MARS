#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include "qcomplex.h"
#include "dval_internal.h"
#include "dval.h"
#include "dval_pattern.h"

static bool is_kind(const dval_t *expr, dval_op_kind_t kind)
{
    return expr && expr->ops && expr->ops->kind == kind;
}

bool dv_is_exact_zero(const dval_t *dv)
{
    return is_kind(dv, DV_KIND_CONST) &&
           !dv->name &&
           qc_eq(dv->c, QC_ZERO);
}

bool dv_is_named_const(const dval_t *dv)
{
    return is_kind(dv, DV_KIND_CONST) && dv->name && *dv->name;
}

static bool dv_match_const_leaf(const dval_t *expr, qfloat_t *value_out, const char **name_out)
{
    if (!is_kind(expr, DV_KIND_CONST))
        return false;
    if (value_out)
        *value_out = expr->c.re;
    if (name_out)
        *name_out = (expr->name && *expr->name) ? expr->name : NULL;
    return true;
}

static bool dv_match_var_leaf(const dval_t *expr, qfloat_t *value_out, const char **name_out)
{
    if (!is_kind(expr, DV_KIND_VAR))
        return false;
    if (value_out)
        *value_out = expr->c.re;
    if (name_out)
        *name_out = (expr->name && *expr->name) ? expr->name : NULL;
    return true;
}

static bool dv_match_unnamed_const_leaf(const dval_t *expr, qfloat_t *value_out)
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
                                  qfloat_t *scale_out,
                                  const dval_t **base_out)
{
    qfloat_t factor_value;
    qfloat_t inner_scale;
    const dval_t *inner_base;

    if (!dv_match_unnamed_const_leaf(factor, &factor_value))
        return false;
    if (dv_match_scaled_expr(other, &inner_scale, &inner_base)) {
        *scale_out = qf_mul(factor_value, inner_scale);
        *base_out = inner_base;
        return true;
    }
    *scale_out = factor_value;
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

static bool dv_match_affine_term(const dval_t *dv,
                                 size_t nvars,
                                 dval_t *const *vars,
                                 qfloat_t scale,
                                 qfloat_t *constant_io,
                                 qfloat_t *coeffs_io)
{
    qfloat_t constant;
    qfloat_t inner_scale;
    const dval_t *base;
    const dval_t *left;
    const dval_t *right;
    bool is_sub;
    size_t idx;

    if (!dv)
        return false;

    if (dv_match_const_value(dv, &constant)) {
        *constant_io = qf_add(*constant_io, qf_mul(scale, constant));
        return true;
    }

    if (dv_match_var_expr(dv, nvars, vars, &idx)) {
        coeffs_io[idx] = qf_add(coeffs_io[idx], scale);
        return true;
    }

    if (dv_match_add_sub_expr(dv, &left, &right, &is_sub))
        return dv_match_affine_term(left, nvars, vars, scale, constant_io, coeffs_io) &&
               dv_match_affine_term(right, nvars, vars, is_sub ? qf_neg(scale) : scale,
                                    constant_io, coeffs_io);

    if (dv_match_scaled_expr(dv, &inner_scale, &base))
        return dv_match_affine_term(base, nvars, vars, qf_mul(scale, inner_scale),
                                    constant_io, coeffs_io);

    return false;
}

static bool dv_pattern_unary_kind_to_op(dv_pattern_unary_affine_kind_t kind,
                                        dval_op_kind_t *op_kind_out)
{
    if (!op_kind_out)
        return false;

    switch (kind) {
    case DV_PATTERN_UNARY_EXP:
        *op_kind_out = DV_KIND_EXP;
        return true;
    case DV_PATTERN_UNARY_SIN:
        *op_kind_out = DV_KIND_SIN;
        return true;
    case DV_PATTERN_UNARY_COS:
        *op_kind_out = DV_KIND_COS;
        return true;
    case DV_PATTERN_UNARY_SINH:
        *op_kind_out = DV_KIND_SINH;
        return true;
    case DV_PATTERN_UNARY_COSH:
        *op_kind_out = DV_KIND_COSH;
        return true;
    default:
        return false;
    }
}

static bool dv_match_unary_affine_op(const dval_t *expr,
                                     dval_op_kind_t op_kind,
                                     size_t nvars,
                                     dval_t *const *vars,
                                     qfloat_t *constant_out,
                                     qfloat_t *coeffs_out)
{
    qfloat_t constant = QF_ZERO;

    if (!expr || !constant_out || !coeffs_out || (nvars > 0 && !vars))
        return false;

    if (!is_kind(expr, op_kind) || !expr->a)
        return false;

    for (size_t i = 0; i < nvars; ++i)
        coeffs_out[i] = QF_ZERO;

    if (!dv_match_affine_term(expr->a, nvars, vars, QF_ONE, &constant, coeffs_out))
        return false;

    *constant_out = constant;
    return true;
}

bool dv_match_unary_affine_kind(const dval_t *expr,
                                dv_pattern_unary_affine_kind_t kind,
                                size_t nvars,
                                dval_t *const *vars,
                                qfloat_t *constant_out,
                                qfloat_t *coeffs_out)
{
    dval_op_kind_t op_kind;

    if (!dv_pattern_unary_kind_to_op(kind, &op_kind))
        return false;

    return dv_match_unary_affine_op(expr, op_kind, nvars, vars, constant_out, coeffs_out);
}

static bool dv_match_affine_power_exact(const dval_t *expr,
                                        size_t nvars,
                                        dval_t *const *vars,
                                        size_t degree,
                                        qfloat_t *constant_out,
                                        qfloat_t *coeffs_out)
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
    qfloat_t constant = QF_ZERO;

    if (!expr || !constant_out || !coeffs_out || (nvars > 0 && !vars))
        return false;

    if (dv_match_unary_kind(expr, DV_KIND_POW_D, &arg) &&
        qf_eq(expr->c.re, qf_from_double((double) degree))) {
        for (size_t i = 0; i < nvars; ++i)
            coeffs_out[i] = QF_ZERO;
        if (!dv_match_affine_term(arg, nvars, vars, QF_ONE, &constant, coeffs_out))
            return false;
        *constant_out = constant;
        return true;
    }

    switch (degree) {
    case 2:
        if (dv_match_binary_kind(expr, DV_KIND_MUL, &left, &right) &&
            dv_struct_eq(left, right)) {
            for (size_t i = 0; i < nvars; ++i)
                coeffs_out[i] = QF_ZERO;
            if (!dv_match_affine_term(left, nvars, vars, QF_ONE, &constant, coeffs_out))
                return false;
            *constant_out = constant;
            return true;
        }
        break;
    case 3:
        if (dv_match_binary_kind(expr, DV_KIND_MUL, &left, &right) &&
            dv_match_binary_kind(left, DV_KIND_MUL, &inner_left, &inner_right) &&
            dv_struct_eq(inner_left, inner_right) &&
            dv_struct_eq(inner_left, right)) {
            for (size_t i = 0; i < nvars; ++i)
                coeffs_out[i] = QF_ZERO;
            if (!dv_match_affine_term(inner_left, nvars, vars, QF_ONE, &constant, coeffs_out))
                return false;
            *constant_out = constant;
            return true;
        }

        if (dv_match_binary_kind(expr, DV_KIND_MUL, &left, &right) &&
            dv_match_binary_kind(right, DV_KIND_MUL, &inner_left, &inner_right) &&
            dv_struct_eq(left, inner_left) &&
            dv_struct_eq(left, inner_right)) {
            for (size_t i = 0; i < nvars; ++i)
                coeffs_out[i] = QF_ZERO;
            if (!dv_match_affine_term(left, nvars, vars, QF_ONE, &constant, coeffs_out))
                return false;
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
            for (size_t i = 0; i < nvars; ++i)
                coeffs_out[i] = QF_ZERO;
            if (!dv_match_affine_term(ll, nvars, vars, QF_ONE, &constant, coeffs_out))
                return false;
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
                            qfloat_t constant_a,
                            const qfloat_t *coeffs_a,
                            qfloat_t constant_b,
                            const qfloat_t *coeffs_b)
{
    if (!qf_eq(constant_a, constant_b))
        return false;
    for (size_t i = 0; i < nvars; ++i)
        if (!qf_eq(coeffs_a[i], coeffs_b[i]))
            return false;
    return true;
}

static bool dv_match_affine_power_deg4(const dval_t *expr,
                                       size_t nvars,
                                       dval_t *const *vars,
                                       size_t *degree_out,
                                       qfloat_t *constant_out,
                                       qfloat_t *coeffs_out)
{
    qfloat_t constant = QF_ZERO;

    if (!expr || !degree_out || !constant_out || !coeffs_out || (nvars > 0 && !vars))
        return false;

    if (dv_match_const_value(expr, &constant)) {
        *degree_out = 0;
        *constant_out = QF_ZERO;
        for (size_t i = 0; i < nvars; ++i)
            coeffs_out[i] = QF_ZERO;
        return true;
    }

    for (size_t i = 0; i < nvars; ++i)
        coeffs_out[i] = QF_ZERO;

    if (dv_match_affine_term(expr, nvars, vars, QF_ONE, &constant, coeffs_out)) {
        *degree_out = 1;
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

    return false;
}

static bool dv_match_scaled_affine_power_deg4(const dval_t *expr,
                                              size_t nvars,
                                              dval_t *const *vars,
                                              qfloat_t *scale_out,
                                              size_t *degree_out,
                                              qfloat_t *constant_out,
                                              qfloat_t *coeffs_out)
{
    qfloat_t inner_scale;
    qfloat_t const_value;
    const dval_t *base;

    if (!expr || !scale_out || !degree_out || !constant_out || !coeffs_out)
        return false;

    if (dv_match_const_value(expr, &const_value)) {
        *scale_out = const_value;
        *degree_out = 0;
        *constant_out = QF_ZERO;
        for (size_t i = 0; i < nvars; ++i)
            coeffs_out[i] = QF_ZERO;
        return true;
    }

    if (dv_match_affine_power_deg4(expr, nvars, vars, degree_out, constant_out, coeffs_out)) {
        *scale_out = QF_ONE;
        return true;
    }

    if (dv_match_scaled_expr(expr, &inner_scale, &base) &&
        dv_match_affine_power_deg4(base, nvars, vars, degree_out, constant_out, coeffs_out)) {
        *scale_out = inner_scale;
        return true;
    }

    return false;
}

static bool dv_affine_is_zero(size_t nvars, qfloat_t constant, const qfloat_t *coeffs)
{
    if (!qf_eq(constant, QF_ZERO))
        return false;
    for (size_t i = 0; i < nvars; ++i)
        if (!qf_eq(coeffs[i], QF_ZERO))
            return false;
    return true;
}

static bool dv_match_affine_poly_deg4_rec(const dval_t *expr,
                                          size_t nvars,
                                          dval_t *const *vars,
                                          qfloat_t *poly_coeffs_out,
                                          qfloat_t *constant_io,
                                          qfloat_t *coeffs_io,
                                          bool *have_basis_io)
{
    qfloat_t subtree_scale;
    const dval_t *scaled_base = NULL;
    const dval_t *left = NULL;
    const dval_t *right = NULL;
    bool is_sub = false;
    qfloat_t scale;
    size_t degree;
    qfloat_t term_constant = QF_ZERO;
    qfloat_t *term_coeffs = NULL;
    qfloat_t poly_left[5] = { QF_ZERO, QF_ZERO, QF_ZERO, QF_ZERO, QF_ZERO };
    qfloat_t poly_right[5] = { QF_ZERO, QF_ZERO, QF_ZERO, QF_ZERO, QF_ZERO };
    qfloat_t left_constant = *constant_io;
    qfloat_t right_constant = *constant_io;
    qfloat_t *left_coeffs = NULL;
    qfloat_t *right_coeffs = NULL;
    bool have_left = *have_basis_io;
    bool have_right = *have_basis_io;
    bool ok = false;

    term_coeffs = malloc(nvars * sizeof(*term_coeffs));
    if ((nvars > 0) && !term_coeffs)
        return false;

    ok = dv_match_scaled_affine_power_deg4(expr, nvars, vars, &scale, &degree,
                                           &term_constant, term_coeffs);
    if (ok) {
        for (size_t i = 0; i < 5; ++i)
            poly_coeffs_out[i] = QF_ZERO;
        poly_coeffs_out[degree] = scale;

        if (degree > 0) {
            if (*have_basis_io &&
                !dv_affine_equal(nvars, *constant_io, coeffs_io, term_constant, term_coeffs)) {
                free(term_coeffs);
                return false;
            }
            *constant_io = term_constant;
            for (size_t i = 0; i < nvars; ++i)
                coeffs_io[i] = term_coeffs[i];
            *have_basis_io = true;
        }

        free(term_coeffs);
        return true;
    }

    free(term_coeffs);

    if (dv_match_scaled_expr(expr, &subtree_scale, &scaled_base) &&
        dv_match_affine_poly_deg4_rec(scaled_base, nvars, vars, poly_left,
                                      constant_io, coeffs_io, have_basis_io)) {
        for (size_t i = 0; i < 5; ++i)
            poly_coeffs_out[i] = qf_mul(subtree_scale, poly_left[i]);
        return true;
    }

    if (dv_match_add_sub_expr(expr, &left, &right, &is_sub)) {
        left_coeffs = malloc(nvars * sizeof(*left_coeffs));
        right_coeffs = malloc(nvars * sizeof(*right_coeffs));
        if ((nvars > 0) && (!left_coeffs || !right_coeffs)) {
            free(left_coeffs);
            free(right_coeffs);
            return false;
        }

        for (size_t i = 0; i < nvars; ++i) {
            left_coeffs[i] = coeffs_io[i];
            right_coeffs[i] = coeffs_io[i];
        }

        if (!dv_match_affine_poly_deg4_rec(left, nvars, vars, poly_left,
                                           &left_constant, left_coeffs, &have_left) ||
            !dv_match_affine_poly_deg4_rec(right, nvars, vars, poly_right,
                                           &right_constant, right_coeffs, &have_right)) {
            free(left_coeffs);
            free(right_coeffs);
            return false;
        }

        if (have_left && have_right &&
            !dv_affine_equal(nvars, left_constant, left_coeffs, right_constant, right_coeffs)) {
            free(left_coeffs);
            free(right_coeffs);
            return false;
        }

        for (size_t i = 0; i < 5; ++i)
            poly_coeffs_out[i] = is_sub ? qf_sub(poly_left[i], poly_right[i])
                                        : qf_add(poly_left[i], poly_right[i]);

        if (have_left) {
            *constant_io = left_constant;
            for (size_t i = 0; i < nvars; ++i)
                coeffs_io[i] = left_coeffs[i];
            *have_basis_io = true;
        } else if (have_right) {
            *constant_io = right_constant;
            for (size_t i = 0; i < nvars; ++i)
                coeffs_io[i] = right_coeffs[i];
            *have_basis_io = true;
        } else {
            *constant_io = QF_ZERO;
            for (size_t i = 0; i < nvars; ++i)
                coeffs_io[i] = QF_ZERO;
            *have_basis_io = false;
        }

        free(left_coeffs);
        free(right_coeffs);
        return true;
    }

    return false;
}

bool dv_match_affine_poly_deg4(const dval_t *expr,
                               size_t nvars,
                               dval_t *const *vars,
                               qfloat_t *poly_coeffs_out,
                               qfloat_t *constant_out,
                               qfloat_t *coeffs_out)
{
    bool have_basis = false;
    qfloat_t constant = QF_ZERO;

    if (!expr || !poly_coeffs_out || !constant_out || !coeffs_out || (nvars > 0 && !vars))
        return false;

    for (size_t i = 0; i < nvars; ++i)
        coeffs_out[i] = QF_ZERO;
    for (size_t i = 0; i < 5; ++i)
        poly_coeffs_out[i] = QF_ZERO;

    if (!dv_match_affine_poly_deg4_rec(expr, nvars, vars, poly_coeffs_out,
                                       &constant, coeffs_out, &have_basis))
        return false;

    *constant_out = have_basis ? constant : QF_ZERO;
    return true;
}

static bool dv_match_affine_poly_deg4_times_unary_affine_op(const dval_t *expr,
                                                            dval_op_kind_t kind,
                                                            size_t nvars,
                                                            dval_t *const *vars,
                                                            qfloat_t *poly_coeffs_out,
                                                            qfloat_t *constant_out,
                                                            qfloat_t *coeffs_out)
{
    const dval_t *left = NULL;
    const dval_t *right = NULL;
    qfloat_t unary_constant = QF_ZERO;
    qfloat_t poly_constant = QF_ZERO;
    qfloat_t *unary_coeffs = NULL;
    qfloat_t *poly_coeffs = NULL;
    qfloat_t poly_terms[5];
    bool matched = false;

    if (!expr || !poly_coeffs_out || !constant_out || !coeffs_out || (nvars > 0 && !vars))
        return false;
    if (!dv_match_binary_kind(expr, DV_KIND_MUL, &left, &right))
        return false;

    unary_coeffs = malloc(nvars * sizeof(*unary_coeffs));
    poly_coeffs = malloc(nvars * sizeof(*poly_coeffs));
    if ((nvars > 0) && (!unary_coeffs || !poly_coeffs)) {
        free(unary_coeffs);
        free(poly_coeffs);
        return false;
    }

    if (dv_match_unary_affine_op(right, kind, nvars, vars, &unary_constant, unary_coeffs) &&
        dv_match_affine_poly_deg4(left, nvars, vars, poly_terms, &poly_constant, poly_coeffs) &&
        (dv_affine_is_zero(nvars, poly_constant, poly_coeffs) ||
         dv_affine_equal(nvars, poly_constant, poly_coeffs, unary_constant, unary_coeffs))) {
        matched = true;
    }

    if (!matched &&
        dv_match_unary_affine_op(left, kind, nvars, vars, &unary_constant, unary_coeffs) &&
        dv_match_affine_poly_deg4(right, nvars, vars, poly_terms, &poly_constant, poly_coeffs) &&
        (dv_affine_is_zero(nvars, poly_constant, poly_coeffs) ||
         dv_affine_equal(nvars, poly_constant, poly_coeffs, unary_constant, unary_coeffs))) {
        matched = true;
    }

    if (matched) {
        *constant_out = unary_constant;
        for (size_t i = 0; i < nvars; ++i)
            coeffs_out[i] = unary_coeffs[i];
        for (size_t i = 0; i < 5; ++i)
            poly_coeffs_out[i] = poly_terms[i];
    }

    free(unary_coeffs);
    free(poly_coeffs);
    return matched;
}

bool dv_match_affine_poly_deg4_times_unary_affine_kind(const dval_t *expr,
                                                       dv_pattern_unary_affine_kind_t kind,
                                                       size_t nvars,
                                                       dval_t *const *vars,
                                                       qfloat_t *poly_coeffs_out,
                                                       qfloat_t *constant_out,
                                                       qfloat_t *coeffs_out)
{
    dval_op_kind_t op_kind;

    if (!dv_pattern_unary_kind_to_op(kind, &op_kind))
        return false;

    return dv_match_affine_poly_deg4_times_unary_affine_op(expr, op_kind, nvars, vars,
                                                           poly_coeffs_out, constant_out,
                                                           coeffs_out);
}

bool dv_match_const_value(const dval_t *expr, qfloat_t *value_out)
{
    return dv_match_unnamed_const_leaf(expr, value_out);
}

bool dv_match_scaled_expr(const dval_t *expr,
                          qfloat_t *scale_out,
                          const dval_t **base_out)
{
    qfloat_t inner_scale;
    qfloat_t const_value;
    const dval_t *inner_base;
    const dval_t *left;
    const dval_t *right;
    const dval_t *arg;

    if (!expr || !scale_out || !base_out)
        return false;

    if (dv_match_unary_kind(expr, DV_KIND_NEG, &arg)) {
        if (dv_match_scaled_expr(arg, &inner_scale, &inner_base)) {
            *scale_out = qf_neg(inner_scale);
            *base_out = inner_base;
            return true;
        }
        *scale_out = qf_from_double(-1.0);
        *base_out = arg;
        return true;
    }

    if (dv_match_binary_kind(expr, DV_KIND_MUL, &left, &right)) {
        if (dv_match_scaled_inner(left, right, scale_out, base_out) ||
            dv_match_scaled_inner(right, left, scale_out, base_out))
            return true;
    }

    if (dv_match_binary_kind(expr, DV_KIND_DIV, &left, &right) &&
        dv_match_unnamed_const_leaf(right, &const_value) &&
        !qf_eq(const_value, QF_ZERO)) {
        if (dv_match_scaled_expr(left, &inner_scale, &inner_base)) {
            *scale_out = qf_div(inner_scale, const_value);
            *base_out = inner_base;
            return true;
        }
        *scale_out = qf_div(QF_ONE, const_value);
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
    qfloat_t value;
    const char *name;
    const dval_t *arg;

    if (!expr)
        return NULL;

    if (expr == needle) {
        dv_retain(replacement);
        return replacement;
    }

    if (dv_match_const_leaf(expr, &value, &name)) {
        if (name)
            return dv_new_named_const(value, name);
        return dv_new_const(value);
    }

    if (dv_match_var_leaf(expr, &value, &name)) {
        if (name)
            return dv_new_named_var(value, name);
        return dv_new_var(value);
    }

    if (dv_match_unary_kind(expr, DV_KIND_POW_D, &arg)) {
        left = dv_substitute(arg, needle, replacement);
        if (!left)
            return NULL;
        out = dv_pow_qc(left, expr->c);
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
