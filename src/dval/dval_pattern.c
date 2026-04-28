#include <stdbool.h>
#include <stddef.h>

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

static bool dv_match_unary_affine(const dval_t *expr,
                                  dval_op_kind_t kind,
                                  size_t nvars,
                                  dval_t *const *vars,
                                  qfloat_t *constant_out,
                                  qfloat_t *coeffs_out)
{
    qfloat_t constant = QF_ZERO;

    if (!expr || !constant_out || !coeffs_out || (nvars > 0 && !vars))
        return false;

    if (!is_kind(expr, kind) || !expr->a)
        return false;

    for (size_t i = 0; i < nvars; ++i)
        coeffs_out[i] = QF_ZERO;

    if (!dv_match_affine_term(expr->a, nvars, vars, QF_ONE, &constant, coeffs_out))
        return false;

    *constant_out = constant;
    return true;
}

bool dv_match_exp_affine(const dval_t *expr,
                         size_t nvars,
                         dval_t *const *vars,
                         qfloat_t *constant_out,
                         qfloat_t *coeffs_out)
{
    return dv_match_unary_affine(expr, DV_KIND_EXP, nvars, vars, constant_out, coeffs_out);
}

#define DV_DEFINE_MATCH_UNARY_AFFINE(fn_name, op_sym)                         \
bool fn_name(const dval_t *expr,                                             \
             size_t nvars,                                                   \
             dval_t *const *vars,                                            \
             qfloat_t *constant_out,                                         \
             qfloat_t *coeffs_out)                                           \
{                                                                            \
    return dv_match_unary_affine(expr, DV_KIND_##op_sym, nvars, vars,       \
                                 constant_out, coeffs_out);                 \
}

DV_DEFINE_MATCH_UNARY_AFFINE(dv_match_sinh_affine, SINH)
DV_DEFINE_MATCH_UNARY_AFFINE(dv_match_cosh_affine, COSH)
DV_DEFINE_MATCH_UNARY_AFFINE(dv_match_sin_affine, SIN)
DV_DEFINE_MATCH_UNARY_AFFINE(dv_match_cos_affine, COS)

#undef DV_DEFINE_MATCH_UNARY_AFFINE

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
