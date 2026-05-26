#include <stddef.h>
#include "dval_bindings.h"
#include "dval_internal.h"

/* ------------------------------------------------------------------------- */
/* EVALUATION FUNCTIONS                                                      */
/* ------------------------------------------------------------------------- */

static number_t eval_const(dval_t *dv)
{
    return num_clone(dv->c);
}

static number_t eval_var(dval_t *dv)
{
    if (dv && dv->binding_expr)
        return dv_binding_expr_eval(dv->binding_expr);
    return num_clone(dv->c);
}

static number_t eval_add(dval_t *dv)
{
    return num_add(dv_eval_num_internal(dv->a), dv_eval_num_internal(dv->b));
}

static number_t eval_sub(dval_t *dv)
{
    return num_sub(dv_eval_num_internal(dv->a), dv_eval_num_internal(dv->b));
}

static bool is_lambert_expr(const dval_t *dv)
{
    return dv_is_op(dv, &ops_lambert_w) ||
           dv_is_op(dv, &ops_lambert_w0) ||
           dv_is_op(dv, &ops_lambert_wm1);
}

static dval_t *lambert_product_inner(dval_t *a, dval_t *b)
{
    dval_t *w;
    dval_t *exp_term;

    if (is_lambert_expr(a) && dv_is_exp_expr(b)) {
        w = a;
        exp_term = b;
    } else if (is_lambert_expr(b) && dv_is_exp_expr(a)) {
        w = b;
        exp_term = a;
    } else {
        return NULL;
    }

    return dv_struct_eq(w, exp_term->a) ? w->a : NULL;
}

static number_t eval_mul(dval_t *dv)
{
    dval_t *inner = lambert_product_inner(dv->a, dv->b);

    if (inner)
        return num_clone(dv_eval_num_internal(inner));

    return num_mul(dv_eval_num_internal(dv->a), dv_eval_num_internal(dv->b));
}

static number_t eval_div(dval_t *dv)
{
    return num_div(dv_eval_num_internal(dv->a), dv_eval_num_internal(dv->b));
}

static number_t eval_neg(dval_t *dv)
{
    return num_neg(dv_eval_num_internal(dv->a));
}

static number_t eval_pow(dval_t *dv)
{
    return num_pow(dv_eval_num_internal(dv->a), dv_eval_num_internal(dv->b));
}

static number_t eval_pow_d(dval_t *dv)
{
    return num_pow(dv_eval_num_internal(dv->a), dv->c);
}

/* ------------------------------------------------------------------------- */
/* DERIVATIVE FUNCTIONS — lazy, stored in each node                          */
/* ------------------------------------------------------------------------- */

static dval_t *deriv_const(dval_t *dv)
{
    (void)dv;
    return dv_new_const(NUM_ZERO);
}

static dval_t *deriv_var(dval_t *dv)
{
    const dval_t *wrt = dv_current_wrt_internal();
    return dv_new_const((wrt == NULL || dv == wrt) ? NUM_ONE : NUM_ZERO);
}

static dval_t *deriv_add(dval_t *dv)
{
    dval_t *da  = dv_get_dx_internal(dv->a);
    dval_t *db  = dv_get_dx_internal(dv->b);
    dval_t *out = dv_add(da, db);
    dv_free(da);
    dv_free(db);
    return out;
}

static dval_t *deriv_sub(dval_t *dv)
{
    dval_t *da  = dv_get_dx_internal(dv->a);
    dval_t *db  = dv_get_dx_internal(dv->b);
    dval_t *out = dv_sub(da, db);
    dv_free(da);
    dv_free(db);
    return out;
}

static dval_t *deriv_exp_inverse_scaled_sqrt_product(dval_t *dv);
static dval_t *deriv_power_inverse_scaled_sqrt_product(dval_t *dv);
static dval_t *deriv_sqrt_affine_over_power(dval_t *dv);

static dval_t *deriv_mul(dval_t *dv)
{
    dval_t *special = deriv_exp_inverse_scaled_sqrt_product(dv);

    if (special)
        return special;
    special = deriv_power_inverse_scaled_sqrt_product(dv);
    if (special)
        return special;

    dval_t *da  = dv_get_dx_internal(dv->a);
    dval_t *db  = dv_get_dx_internal(dv->b);
    dval_t *t1  = dv_mul(da, dv->b);
    dval_t *t2  = dv_mul(dv->a, db);
    dval_t *out = dv_add(t1, t2);
    dv_free(da);
    dv_free(db);
    dv_free(t1);
    dv_free(t2);
    return out;
}

static int dv_has_composite_preserved_binding_expr_node(const dval_t *dv)
{
    return dv_is_const(dv) && dv->binding_expr &&
        dv->binding_expr->kind != DV_BINDING_EXPR_NUMBER &&
        dv->binding_expr->kind != DV_BINDING_EXPR_CONST;
}

static int dv_binding_aware_search(const dval_t *dv,
                                   const dval_t *needle,
                                   int null_needle_matches_any,
                                   int match_composite_binding)
{
    dval_t *expanded;
    int depends;

    if (!dv)
        return 0;

    if (match_composite_binding &&
        dv_has_composite_preserved_binding_expr_node(dv))
        return 1;

    if (!needle) {
        if (null_needle_matches_any)
            return 1;
    } else if (dv == needle || dv_struct_eq(dv, needle)) {
        return 1;
    }

    if (dv_has_composite_preserved_binding_expr_node(dv)) {
        expanded = dv_binding_expr_eval_dval(dv->binding_expr);
        depends = expanded
            ? dv_binding_aware_search(expanded, needle,
                                      null_needle_matches_any,
                                      match_composite_binding)
            : 0;
        if (expanded)
            dv_free(expanded);
        return depends;
    }

    return dv_binding_aware_search(dv->a, needle,
                                   null_needle_matches_any,
                                   match_composite_binding) ||
           dv_binding_aware_search(dv->b, needle,
                                   null_needle_matches_any,
                                   match_composite_binding);
}

static int dv_depends_on_current_wrt(const dval_t *dv)
{
    return dv_binding_aware_search(dv, dv_current_wrt_internal(), 1, 0);
}

static int dv_depends_on_structural_node(const dval_t *dv, const dval_t *needle)
{
    if (!needle)
        return 0;
    return dv_binding_aware_search(dv, needle, 0, 0);
}

static int dv_has_composite_preserved_binding_expr(const dval_t *dv)
{
    return dv_binding_aware_search(dv, NULL, 0, 1);
}

static int dv_is_deriv_foldable_real_const(const dval_t *dv)
{
    return dv_is_unnamed_const(dv) &&
           (!dv->binding_expr || dv->binding_expr->kind == DV_BINDING_EXPR_NUMBER) &&
           num_is_real(dv->c);
}

static int dv_is_sqrt_like_expr(const dval_t *dv)
{
    return dv_is_sqrt_expr(dv) ||
           (dv_is_pow_d_expr(dv) && num_eq(dv->c, NUM_HALF));
}

static const dval_t *dv_sqrt_like_arg(const dval_t *dv)
{
    return dv->a;
}

static int split_scaled_sqrt_denominator(const dval_t *dv,
                                         number_t *scale_out,
                                         const dval_t **sqrt_out)
{
    if (dv_is_sqrt_like_expr(dv)) {
        num_destroy(scale_out);
        *scale_out = num_clone(NUM_ONE);
        *sqrt_out = dv;
        return 1;
    }

    if (!dv_is_op(dv, &ops_mul))
        return 0;

    if (dv_is_deriv_foldable_real_const(dv->a) && dv_is_sqrt_like_expr(dv->b)) {
        num_destroy(scale_out);
        *scale_out = num_clone(dv->a->c);
        *sqrt_out = dv->b;
        return 1;
    }

    if (dv_is_deriv_foldable_real_const(dv->b) && dv_is_sqrt_like_expr(dv->a)) {
        num_destroy(scale_out);
        *scale_out = num_clone(dv->b->c);
        *sqrt_out = dv->a;
        return 1;
    }

    return 0;
}

static int split_exp_numerator(const dval_t *dv,
                               const dval_t **factor_out,
                               const dval_t **exp_out)
{
    if (dv_is_exp_expr(dv)) {
        *factor_out = NULL;
        *exp_out = dv;
        return 1;
    }

    if (!dv_is_op(dv, &ops_mul))
        return 0;

    if (dv_is_exp_expr(dv->a) && !dv_depends_on_current_wrt(dv->b)) {
        *factor_out = dv->b;
        *exp_out = dv->a;
        return 1;
    }

    if (dv_is_exp_expr(dv->b) && !dv_depends_on_current_wrt(dv->a)) {
        *factor_out = dv->a;
        *exp_out = dv->b;
        return 1;
    }

    return 0;
}

static int exp_arg_is_scaled_sqrt(const dval_t *arg, const dval_t *sqrt_node)
{
    const dval_t *sqrt_arg = dv_sqrt_like_arg(sqrt_node);

    if (dv_struct_eq(arg, sqrt_node))
        return 1;
    if (dv_is_sqrt_like_expr(arg) && dv_struct_eq(dv_sqrt_like_arg(arg), sqrt_arg))
        return 1;

    if (!dv_is_op(arg, &ops_mul))
        return 0;

    if (((dv_struct_eq(arg->a, sqrt_node)) ||
         (dv_is_sqrt_like_expr(arg->a) &&
          dv_struct_eq(dv_sqrt_like_arg(arg->a), sqrt_arg))) &&
        !dv_depends_on_current_wrt(arg->b))
        return 1;
    if (((dv_struct_eq(arg->b, sqrt_node)) ||
         (dv_is_sqrt_like_expr(arg->b) &&
          dv_struct_eq(dv_sqrt_like_arg(arg->b), sqrt_arg))) &&
        !dv_depends_on_current_wrt(arg->a))
        return 1;

    return 0;
}

static dval_t *deriv_exp_over_scaled_sqrt(dval_t *dv)
{
    NUM_SCOPE(scope);
    const dval_t *sqrt_den;
    const dval_t *factor;
    const dval_t *exp_node;
    number_t den_scale = num_new();
    dval_t *factor_exp = NULL;
    dval_t *numerator = NULL;
    dval_t *bracket = NULL;
    dval_t *one = NULL;
    dval_t *x_pow = NULL;
    dval_t *quotient = NULL;
    dval_t *out = NULL;

    if (!split_scaled_sqrt_denominator(dv->b, &den_scale, &sqrt_den))
        return NULL;
    if (!split_exp_numerator(dv->a, &factor, &exp_node)) {
        num_destroy(&den_scale);
        return NULL;
    }
    if (!exp_arg_is_scaled_sqrt(exp_node->a, sqrt_den)) {
        num_destroy(&den_scale);
        return NULL;
    }
    if (factor && dv_depends_on_structural_node(factor, dv_sqrt_like_arg(sqrt_den))) {
        num_destroy(&den_scale);
        return NULL;
    }

    if (factor) {
        dv_retain(factor);
        dv_retain(exp_node);
        factor_exp = dv_mul(factor, exp_node);
        dv_free((dval_t *)factor);
        dv_free((dval_t *)exp_node);
    } else {
        dv_retain(exp_node);
        factor_exp = (dval_t *)exp_node;
    }

    dv_retain(exp_node->a);
    one = dv_new_const(NUM_ONE);
    bracket = dv_sub(exp_node->a, one);
    dv_free(exp_node->a);
    dv_free(one);

    numerator = dv_mul(factor_exp, bracket);
    dv_free(factor_exp);
    dv_free(bracket);

    {
        number_t three = num_create_from_long(3L);
        number_t three_halves = num_div(three, NUM_TWO);
        number_t two_den = num_mul(NUM_TWO, den_scale);
        number_t scalar = num_div(NUM_ONE, two_den);

        x_pow = dv_pow(dv_sqrt_like_arg(sqrt_den), &three_halves);
        quotient = dv_div(numerator, x_pow);
        out = dv_make_scaled(scalar, quotient);
        dv_free(x_pow);
        num_destroy(&three);
        num_destroy(&three_halves);
        num_destroy(&two_den);
        num_destroy(&scalar);
    }

    dv_free(numerator);

    num_destroy(&den_scale);
    return out;
}

static int split_inverse_scaled_sqrt(const dval_t *dv,
                                     number_t *num_scale_out,
                                     number_t *den_scale_out,
                                     const dval_t **sqrt_out)
{
    if (!dv_is_div(dv))
        return 0;
    if (!dv_is_deriv_foldable_real_const(dv->a))
        return 0;
    if (!split_scaled_sqrt_denominator(dv->b, den_scale_out, sqrt_out))
        return 0;

    num_destroy(num_scale_out);
    *num_scale_out = num_clone(dv->a->c);
    return 1;
}

static int split_symbolic_inverse_scaled_sqrt(const dval_t *dv,
                                              number_t *den_scale_out,
                                              const dval_t **factor_out,
                                              const dval_t **sqrt_out)
{
    if (!dv_is_div(dv))
        return 0;
    if (!split_scaled_sqrt_denominator(dv->b, den_scale_out, sqrt_out))
        return 0;
    if (dv_has_composite_preserved_binding_expr(dv->a))
        return 0;
    if (dv_depends_on_structural_node(dv->a, dv_sqrt_like_arg(*sqrt_out)))
        return 0;

    *factor_out = dv->a;
    return 1;
}

static int split_power_like(const dval_t *dv,
                            const dval_t **base_out,
                            number_t *exponent_out)
{
    if (dv_is_sqrt_like_expr(dv)) {
        *base_out = dv_sqrt_like_arg(dv);
        num_destroy(exponent_out);
        *exponent_out = num_clone(NUM_HALF);
        return 1;
    }
    if (dv_is_pow_d_expr(dv)) {
        *base_out = dv->a;
        num_destroy(exponent_out);
        *exponent_out = num_clone(dv->c);
        return 1;
    }
    return 0;
}

static int split_scaled_sqrt_factor_owned(const dval_t *term,
                                          const dval_t *base,
                                          dval_t **factor_out)
{
    if (!term || !base)
        return 0;
    if (dv_is_sqrt_like_expr(term) &&
        dv_struct_eq(dv_sqrt_like_arg(term), base)) {
        *factor_out = dv_new_const(NUM_ONE);
        return 1;
    }
    if (dv_is_neg(term)) {
        dval_t *inner_factor = NULL;

        if (!split_scaled_sqrt_factor_owned(term->a, base, &inner_factor))
            return 0;
        *factor_out = dv_neg(inner_factor);
        dv_free(inner_factor);
        return 1;
    }
    if (dv_is_mul(term)) {
        if (dv_is_sqrt_like_expr(term->a) &&
            dv_struct_eq(dv_sqrt_like_arg(term->a), base) &&
            !dv_depends_on_structural_node(term->b, base)) {
            dv_retain(term->b);
            *factor_out = term->b;
            return 1;
        }
        if (dv_is_sqrt_like_expr(term->b) &&
            dv_struct_eq(dv_sqrt_like_arg(term->b), base) &&
            !dv_depends_on_structural_node(term->a, base)) {
            dv_retain(term->a);
            *factor_out = term->a;
            return 1;
        }
    }
    return 0;
}

static int split_sqrt_affine_numerator_owned(const dval_t *num,
                                             const dval_t *base,
                                             dval_t **factor_out,
                                             dval_t **constant_out)
{
    dval_t *factor = NULL;

    if (split_scaled_sqrt_factor_owned(num, base, &factor)) {
        *factor_out = factor;
        *constant_out = dv_new_const(NUM_ZERO);
        return 1;
    }

    if (!dv_is_addsub(num))
        return 0;

    if (split_scaled_sqrt_factor_owned(num->a, base, &factor) &&
        !dv_depends_on_structural_node(num->b, base)) {
        *factor_out = factor;
        if (dv_is_op(num, &ops_sub)) {
            dval_t *neg_const;

            dv_retain(num->b);
            neg_const = dv_neg(num->b);
            dv_free(num->b);
            *constant_out = neg_const;
        } else {
            dv_retain(num->b);
            *constant_out = num->b;
        }
        return 1;
    }
    dv_free(factor);
    factor = NULL;

    if (dv_is_op(num, &ops_add) &&
        split_scaled_sqrt_factor_owned(num->b, base, &factor) &&
        !dv_depends_on_structural_node(num->a, base)) {
        dv_retain(num->a);
        *factor_out = factor;
        *constant_out = num->a;
        return 1;
    }

    if (dv_is_op(num, &ops_sub) &&
        split_scaled_sqrt_factor_owned(num->b, base, &factor) &&
        !dv_depends_on_structural_node(num->a, base)) {
        dval_t *neg_factor = dv_neg(factor);

        dv_free(factor);
        dv_retain(num->a);
        *factor_out = neg_factor;
        *constant_out = num->a;
        return 1;
    }
    dv_free(factor);
    return 0;
}

static dval_t *deriv_sqrt_affine_over_power(dval_t *dv)
{
    NUM_SCOPE(scope);
    const dval_t *base = NULL;
    number_t exponent = num_new();
    number_t half_minus_exponent = num_new();
    number_t neg_exponent = num_new();
    number_t den_exponent = num_new();
    dval_t *factor = NULL;
    dval_t *constant = NULL;
    dval_t *sqrt_base = NULL;
    dval_t *sqrt_term = NULL;
    dval_t *term1 = NULL;
    dval_t *term2 = NULL;
    dval_t *sum = NULL;
    dval_t *den = NULL;
    dval_t *out = NULL;

    if (!split_power_like(dv->b, &base, &exponent))
        goto cleanup;
    if (!split_sqrt_affine_numerator_owned(dv->a, base, &factor, &constant))
        goto cleanup;

    half_minus_exponent = num_sub(NUM_HALF, exponent);
    neg_exponent = num_neg(exponent);
    den_exponent = num_add(exponent, NUM_ONE);

    sqrt_base = dv_sqrt(base);
    sqrt_term = dv_mul(factor, sqrt_base);
    term1 = dv_make_scaled(half_minus_exponent, sqrt_term);
    sqrt_term = NULL;
    term2 = dv_make_scaled(neg_exponent, constant);
    constant = NULL;
    sum = dv_add(term1, term2);
    den = dv_pow(base, &den_exponent);
    out = dv_div(sum, den);

cleanup:
    dv_free(factor);
    dv_free(constant);
    dv_free(sqrt_base);
    dv_free(sqrt_term);
    dv_free(term1);
    dv_free(term2);
    dv_free(sum);
    dv_free(den);
    num_destroy(&exponent);
    num_destroy(&half_minus_exponent);
    num_destroy(&neg_exponent);
    num_destroy(&den_exponent);
    return out;
}

static dval_t *deriv_power_inverse_scaled_sqrt_product(dval_t *dv)
{
    NUM_SCOPE(scope);
    const dval_t *factor = NULL;
    const dval_t *sqrt_den = NULL;
    const dval_t *base = NULL;
    number_t den_scale = num_new();
    number_t exponent = num_new();
    number_t coeff = num_new();
    number_t out_exponent = num_new();
    number_t three = num_create_from_long(3L);
    number_t three_halves = num_div(three, NUM_TWO);
    dval_t *factor_scaled = NULL;
    dval_t *power_base = NULL;
    dval_t *pow_term = NULL;
    dval_t *base_dx = NULL;
    dval_t *tmp = NULL;
    dval_t *out = NULL;
    int matched;

    matched = (split_power_like(dv->a, &base, &exponent) &&
               split_symbolic_inverse_scaled_sqrt(dv->b, &den_scale,
                                                  &factor, &sqrt_den)) ||
              (split_power_like(dv->b, &base, &exponent) &&
               split_symbolic_inverse_scaled_sqrt(dv->a, &den_scale,
                                                  &factor, &sqrt_den));
    if (!matched)
        goto cleanup;
    if (!dv_struct_eq(base, dv_sqrt_like_arg(sqrt_den)))
        goto cleanup;

    coeff = num_sub(exponent, NUM_HALF);
    if (num_is_zero(coeff)) {
        out = dv_new_const(NUM_ZERO);
        goto cleanup;
    }
    coeff = num_div(coeff, den_scale);
    out_exponent = num_sub(exponent, three_halves);

    dv_retain(factor);
    factor_scaled = dv_make_scaled(coeff, (dval_t *)factor);
    dv_retain(base);
    power_base = (dval_t *)base;
    pow_term = dv_make_pow_like(power_base, out_exponent);
    base_dx = dv_get_dx_internal((dval_t *)base);

    tmp = dv_mul(factor_scaled, pow_term);
    out = dv_mul(tmp, base_dx);

cleanup:
    dv_free(factor_scaled);
    dv_free(pow_term);
    dv_free(base_dx);
    dv_free(tmp);
    num_destroy(&den_scale);
    num_destroy(&exponent);
    num_destroy(&coeff);
    num_destroy(&out_exponent);
    num_destroy(&three);
    num_destroy(&three_halves);
    return out;
}

static dval_t *deriv_exp_inverse_scaled_sqrt_product(dval_t *dv)
{
    NUM_SCOPE(scope);
    const dval_t *factor = NULL;
    const dval_t *exp_node = NULL;
    const dval_t *sqrt_den = NULL;
    number_t num_scale = num_new();
    number_t den_scale = num_new();
    dval_t *factor_exp = NULL;
    dval_t *bracket = NULL;
    dval_t *one = NULL;
    dval_t *numerator = NULL;
    dval_t *x_pow = NULL;
    dval_t *quotient = NULL;
    dval_t *out = NULL;
    int matched;

    matched = (split_exp_numerator(dv->a, &factor, &exp_node) &&
               split_inverse_scaled_sqrt(dv->b, &num_scale, &den_scale, &sqrt_den)) ||
              (split_exp_numerator(dv->b, &factor, &exp_node) &&
               split_inverse_scaled_sqrt(dv->a, &num_scale, &den_scale, &sqrt_den));
    if (!matched) {
        num_destroy(&num_scale);
        num_destroy(&den_scale);
        return NULL;
    }

    if (!exp_arg_is_scaled_sqrt(exp_node->a, sqrt_den)) {
        num_destroy(&num_scale);
        num_destroy(&den_scale);
        return NULL;
    }
    if (factor && dv_depends_on_structural_node(factor, dv_sqrt_like_arg(sqrt_den))) {
        num_destroy(&num_scale);
        num_destroy(&den_scale);
        return NULL;
    }

    if (factor) {
        dv_retain(factor);
        dv_retain(exp_node);
        factor_exp = dv_mul(factor, exp_node);
        dv_free((dval_t *)factor);
        dv_free((dval_t *)exp_node);
    } else {
        dv_retain(exp_node);
        factor_exp = (dval_t *)exp_node;
    }

    dv_retain(exp_node->a);
    one = dv_new_const(NUM_ONE);
    bracket = dv_sub(exp_node->a, one);
    dv_free(exp_node->a);
    dv_free(one);

    numerator = dv_mul(factor_exp, bracket);
    dv_free(factor_exp);
    dv_free(bracket);

    {
        number_t three = num_create_from_long(3L);
        number_t three_halves = num_div(three, NUM_TWO);
        number_t two_den = num_mul(NUM_TWO, den_scale);
        number_t scalar = num_div(num_scale, two_den);

        x_pow = dv_pow(dv_sqrt_like_arg(sqrt_den), &three_halves);
        quotient = dv_div(numerator, x_pow);
        out = dv_make_scaled(scalar, quotient);
        dv_free(x_pow);
        num_destroy(&three);
        num_destroy(&three_halves);
        num_destroy(&two_den);
        num_destroy(&scalar);
    }

    dv_free(numerator);

    num_destroy(&num_scale);
    num_destroy(&den_scale);
    return out;
}

static dval_t *deriv_div(dval_t *dv)
{
    NUM_SCOPE(scope);
    dval_t *special = deriv_exp_over_scaled_sqrt(dv);

    if (special)
        return special;
    special = deriv_sqrt_affine_over_power(dv);
    if (special)
        return special;

    dval_t *da   = dv_get_dx_internal(dv->a);
    dval_t *db   = dv_get_dx_internal(dv->b);
    dval_t *num1 = dv_mul(da, dv->b);
    dval_t *num2 = dv_mul(dv->a, db);
    dval_t *num  = dv_sub(num1, num2);
    number_t two = num_create_from_long(2);
    dval_t *den  = dv_pow(dv->b, &two);
    dval_t *out  = dv_div(num, den);
    dv_free(da);
    dv_free(db);
    dv_free(num1);
    dv_free(num2);
    dv_free(num);
    dv_free(den);
    return out;
}

static dval_t *deriv_neg(dval_t *dv)
{
    dval_t *da  = dv_get_dx_internal(dv->a);
    dval_t *out = dv_neg(da);
    dv_free(da);
    return out;
}

static dval_t *dv_unary_constexpr_from_preserved_arg(const dval_ops_t *ops,
                                                     const dval_t *arg)
{
    dv_binding_expr_t *expr;
    number_t value;
    dval_t *node;

    if (!arg || !dv_is_const(arg) || !arg->binding_expr)
        return NULL;

    expr = dv_binding_expr_new_unary_op(ops,
                                        dv_binding_expr_clone(arg->binding_expr));
    expr = dv_binding_expr_simplify(expr);
    value = dv_binding_expr_eval(expr);
    node = dv_new_const(value);
    num_destroy(&value);
    node->binding_expr = expr;
    return node;
}

static dval_t *dv_log_preserving_constexpr(const dval_t *arg)
{
    dval_t *preserved = dv_unary_constexpr_from_preserved_arg(&ops_log, arg);

    return preserved ? preserved : dv_log(arg);
}

static dval_t *deriv_pow(dval_t *dv)
{
    dval_t *a  = dv->a;
    dval_t *b  = dv->b;
    dval_t *da = dv_get_dx_internal(a);
    dval_t *db = dv_get_dx_internal(b);

    dval_t *loga    = dv_log_preserving_constexpr(a);
    dval_t *da_on_a = dv_div(da, a);
    dval_t *term1   = dv_mul(db, loga);
    dval_t *term2   = dv_mul(b, da_on_a);
    dval_t *sum     = dv_add(term1, term2);
    dval_t *powab   = dv_pow_dv(a, b);
    dval_t *out     = dv_mul(sum, powab);

    dv_free(da);
    dv_free(db);
    dv_free(loga);
    dv_free(da_on_a);
    dv_free(term1);
    dv_free(term2);
    dv_free(sum);
    dv_free(powab);

    return out;
}

static dval_t *deriv_pow_d(dval_t *dv)
{
    NUM_SCOPE(scope);
    number_t exponent = num_clone(dv->c);
    number_t exponent_minus_one = num_sub(exponent, NUM_ONE);
    dval_t *da   = dv_get_dx_internal(dv->a);
    dval_t *p    = dv_pow(dv->a, &exponent_minus_one);
    dval_t *coef = dv_new_const(exponent);
    dval_t *cp   = dv_mul(coef, p);
    dval_t *out  = dv_mul(cp, da);
    dv_free(da);
    dv_free(p);
    dv_free(coef);
    dv_free(cp);
    return out;
}

/* ------------------------------------------------------------------------- */
/* Operator vtable instances                                                 */
/* ------------------------------------------------------------------------- */

const dval_ops_t ops_const = {
    .eval = eval_const,
    .deriv = deriv_const,
    .reverse = dv_reverse_atom,
    .kind = DV_KIND_CONST,
    .arity = DV_OP_ATOM,
    .name = "const",
    .tex_name = NULL,
    .apply_unary = NULL,
    .apply_binary = NULL,
    .simplify = dv_simplify_passthrough,
    .fold_const_unary = NULL
};

const dval_ops_t ops_var = {
    .eval = eval_var,
    .deriv = deriv_var,
    .reverse = dv_reverse_atom,
    .kind = DV_KIND_VAR,
    .arity = DV_OP_ATOM,
    .name = "var",
    .tex_name = NULL,
    .apply_unary = NULL,
    .apply_binary = NULL,
    .simplify = dv_simplify_passthrough,
    .fold_const_unary = NULL
};

const dval_ops_t ops_add = {
    .eval = eval_add,
    .deriv = deriv_add,
    .reverse = dv_reverse_add,
    .kind = DV_KIND_ADD,
    .arity = DV_OP_BINARY,
    .name = "+",
    .tex_name = "+",
    .apply_unary = NULL,
    .apply_binary = dv_add,
    .simplify = dv_simplify_add_sub_operator,
    .fold_const_unary = NULL
};

const dval_ops_t ops_sub = {
    .eval = eval_sub,
    .deriv = deriv_sub,
    .reverse = dv_reverse_sub,
    .kind = DV_KIND_SUB,
    .arity = DV_OP_BINARY,
    .name = "-",
    .tex_name = "-",
    .apply_unary = NULL,
    .apply_binary = dv_sub,
    .simplify = dv_simplify_add_sub_operator,
    .fold_const_unary = NULL
};

const dval_ops_t ops_mul = {
    .eval = eval_mul,
    .deriv = deriv_mul,
    .reverse = dv_reverse_mul,
    .kind = DV_KIND_MUL,
    .arity = DV_OP_BINARY,
    .name = "*",
    .tex_name = "\\cdot",
    .apply_unary = NULL,
    .apply_binary = dv_mul,
    .simplify = dv_simplify_mul_operator,
    .fold_const_unary = NULL
};

const dval_ops_t ops_div = {
    .eval = eval_div,
    .deriv = deriv_div,
    .reverse = dv_reverse_div,
    .kind = DV_KIND_DIV,
    .arity = DV_OP_BINARY,
    .name = "/",
    .tex_name = "/",
    .apply_unary = NULL,
    .apply_binary = dv_div,
    .simplify = dv_simplify_div_operator,
    .fold_const_unary = NULL
};

const dval_ops_t ops_pow = {
    .eval = eval_pow,
    .deriv = deriv_pow,
    .reverse = dv_reverse_pow,
    .kind = DV_KIND_POW,
    .arity = DV_OP_BINARY,
    .name = "^",
    .tex_name = "^",
    .apply_unary = NULL,
    .apply_binary = dv_pow_dv,
    .simplify = dv_simplify_pow_operator,
    .fold_const_unary = NULL
};

const dval_ops_t ops_pow_d = {
    .eval = eval_pow_d,
    .deriv = deriv_pow_d,
    .reverse = dv_reverse_pow_d,
    .kind = DV_KIND_POW_D,
    .arity = DV_OP_BINARY,
    .name = "^",
    .tex_name = "^",
    .apply_unary = NULL,
    .apply_binary = NULL,
    .simplify = dv_simplify_pow_d_operator,
    .fold_const_unary = NULL
};

const dval_ops_t ops_neg = {
    .eval = eval_neg,
    .deriv = deriv_neg,
    .reverse = dv_reverse_neg,
    .kind = DV_KIND_NEG,
    .arity = DV_OP_UNARY,
    .name = "-",
    .tex_name = "-",
    .apply_unary = dv_neg,
    .apply_binary = NULL,
    .simplify = dv_simplify_neg_operator,
    .fold_const_unary = NULL
};

/* ------------------------------------------------------------------------- */
/* Arithmetic constructors (retain children)                                 */
/* ------------------------------------------------------------------------- */

dval_t *dv_neg(const dval_t *dv)
{
    if (!dv)
        return NULL;
    dv_retain(dv);
    return dv_new_unary_internal(&ops_neg, dv);
}

dval_t *dv_add(const dval_t *dv1, const dval_t *dv2)
{
    if (!dv1 || !dv2)
        return NULL;
    dv_retain(dv1);
    dv_retain(dv2);
    return dv_new_binary_internal(&ops_add, dv1, dv2);
}

dval_t *dv_sub(const dval_t *dv1, const dval_t *dv2)
{
    if (!dv1 || !dv2)
        return NULL;
    dv_retain(dv1);
    dv_retain(dv2);
    return dv_new_binary_internal(&ops_sub, dv1, dv2);
}

dval_t *dv_mul(const dval_t *dv1, const dval_t *dv2)
{
    if (!dv1 || !dv2)
        return NULL;
    dv_retain(dv1);
    dv_retain(dv2);
    return dv_new_binary_internal(&ops_mul, dv1, dv2);
}

dval_t *dv_div(const dval_t *dv1, const dval_t *dv2)
{
    if (!dv1 || !dv2)
        return NULL;
    dv_retain(dv1);
    dv_retain(dv2);
    return dv_new_binary_internal(&ops_div, dv1, dv2);
}

dval_t *dv_pow_dv(const dval_t *a, const dval_t *b)
{
    if (!a || !b)
        return NULL;
    dv_retain(a);
    dv_retain(b);
    return dv_new_binary_internal(&ops_pow, a, b);
}

dval_t *dv_pow(const dval_t *dv, const number_t *exponent)
{
    if (!dv || !exponent)
        return NULL;
    dv_retain(dv);
    return dv_new_pow_const_internal(dv, *exponent);
}

dval_t *dv_add_num(const dval_t *dv, const number_t *value)
{
    dval_t *c;
    dval_t *r;

    if (!value)
        return NULL;
    c = dv_new_const(*value);
    if (!c)
        return NULL;
    r = dv_add(dv, c);
    dv_free(c);
    return r;
}

dval_t *dv_sub_num(const dval_t *dv, const number_t *value)
{
    dval_t *c;
    dval_t *r;

    if (!value)
        return NULL;
    c = dv_new_const(*value);
    if (!c)
        return NULL;
    r = dv_sub(dv, c);
    dv_free(c);
    return r;
}

dval_t *dv_num_sub(const number_t *value, const dval_t *dv)
{
    dval_t *c;
    dval_t *r;

    if (!value)
        return NULL;
    c = dv_new_const(*value);
    if (!c)
        return NULL;
    r = dv_sub(c, dv);
    dv_free(c);
    return r;
}

dval_t *dv_mul_num(const dval_t *dv, const number_t *value)
{
    dval_t *c;
    dval_t *r;

    if (!value)
        return NULL;
    c = dv_new_const(*value);
    if (!c)
        return NULL;
    r = dv_mul(dv, c);
    dv_free(c);
    return r;
}

dval_t *dv_div_num(const dval_t *dv, const number_t *value)
{
    dval_t *c;
    dval_t *r;

    if (!value)
        return NULL;
    c = dv_new_const(*value);
    if (!c)
        return NULL;
    r = dv_div(dv, c);
    dv_free(c);
    return r;
}

dval_t *dv_num_div(const number_t *value, const dval_t *dv)
{
    dval_t *c;
    dval_t *r;

    if (!value)
        return NULL;
    c = dv_new_const(*value);
    if (!c)
        return NULL;
    r = dv_div(c, dv);
    dv_free(c);
    return r;
}
