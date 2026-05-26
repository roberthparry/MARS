#include "dval_bindings.h"
#include "dval_internal.h"

bool dv_simplify_is_plain_real_const(const dval_t *dv)
{
    if (!dv_is_unnamed_const(dv) || !num_is_real(dv->c))
        return false;
    if (dv->binding_expr && dv->binding_expr->kind != DV_BINDING_EXPR_NUMBER)
        return false;
    return true;
}

bool dv_simplify_try_get_plain_real_const(const dval_t *dv, number_t *out)
{
    if (!dv_simplify_is_plain_real_const(dv))
        return false;
    *out = num_clone(dv->c);
    return true;
}

bool dv_simplify_is_simplifiable_const(const dval_t *dv)
{
    return dv_is_op(dv, &ops_const) &&
           (!dv->name || !*dv->name || !dv->binding_expr);
}

bool dv_simplify_allows_const_identity_fold(const dval_t *dv)
{
    if (!dv || !dv_is_op(dv, &ops_const))
        return false;
    if (!dv->binding_expr)
        return true;
    return dv->binding_expr->kind == DV_BINDING_EXPR_NUMBER ||
           dv->binding_expr->kind == DV_BINDING_EXPR_CONST;
}

number_t dv_simplify_normalise_simple_rational_coeff(number_t coeff)
{
    if (num_eq(coeff, NUM_HALF))
        return num_clone(NUM_HALF);
    if (num_eq(coeff, NUM_QUARTER))
        return num_clone(NUM_QUARTER);
    if (num_eq(coeff, NUM_ONE_EIGHTH))
        return num_clone(NUM_ONE_EIGHTH);
    if (num_is_real(coeff)) {
        double d = num_to_double(coeff);

        if (d == 0.5)
            return num_clone(NUM_HALF);
        if (d == 0.25)
            return num_clone(NUM_QUARTER);
        if (d == 0.125)
            return num_clone(NUM_ONE_EIGHTH);
        if (d == -0.5)
            return num_neg(NUM_HALF);
        if (d == -0.25)
            return num_neg(NUM_QUARTER);
        if (d == -0.125)
            return num_neg(NUM_ONE_EIGHTH);
    }
    return num_clone(coeff);
}

dval_t *dv_simplify_try_log10_power_of_ten(dval_t *arg)
{
    dval_t *inner;

    if (!dv_is_op(arg, &ops_pow) ||
        !dv_is_op(arg->a, &ops_const) ||
        !num_eq(arg->a->c, NUM_TEN))
        return NULL;

    inner = arg->b;
    dv_retain(inner);
    dv_free(arg);
    return inner;
}

dval_t *dv_simplify_try_floor_ceil_const(const dval_t *op, dval_t *arg)
{
    number_t folded;
    dval_t *out;

    if (!dv_is_op(arg, &ops_const) ||
        (!dv_is_op(op, &ops_floor) && !dv_is_op(op, &ops_ceil)))
        return NULL;

    folded = dv_is_op(op, &ops_floor) ? num_floor(arg->c) : num_ceil(arg->c);
    out = dv_new_const(folded);
    num_destroy(&folded);
    dv_free(arg);
    return out;
}

dval_t *dv_simplify_try_unary_const_fold(const dval_t *op, dval_t *arg)
{
    number_t folded;
    dval_t *out;

    if (!dv_simplify_allows_const_identity_fold(arg) ||
        !op->ops->fold_const_unary)
        return NULL;

    folded = num_new();
    if (!op->ops->fold_const_unary(&arg->c, &folded) ||
        !num_is_finite(folded)) {
        num_destroy(&folded);
        return NULL;
    }

    out = dv_new_const(folded);
    num_destroy(&folded);
    dv_free(arg);
    return out;
}

dval_t *dv_simplify_try_sqrt_scaled_square_const(dval_t *arg)
{
    number_t coeff_root;
    number_t coeff_square;
    dval_t *raw;
    dval_t *simp;
    dval_t *out;

    if (!dv_is_op(arg, &ops_mul) ||
        !dv_simplify_is_plain_real_const(arg->a) ||
        !num_gt(arg->a->c, NUM_ZERO))
        return NULL;

    coeff_root = num_sqrt(arg->a->c);
    coeff_square = num_mul(coeff_root, coeff_root);
    if (!num_eq(coeff_square, arg->a->c)) {
        num_destroy(&coeff_square);
        num_destroy(&coeff_root);
        return NULL;
    }
    num_destroy(&coeff_square);

    dv_retain(arg->b);
    raw = dv_sqrt(arg->b);
    dv_free(arg->b);
    simp = dv_simplify(raw);
    dv_free(raw);
    dv_free(arg);
    out = dv_make_scaled(coeff_root, simp);
    num_destroy(&coeff_root);
    return out;
}

dval_t *dv_simplify_direct_inverse_pair(const dval_t *outer, dval_t *inner)
{
    dval_t *arg;

    if (!outer || !inner || inner->ops->arity != DV_OP_UNARY ||
        outer->ops->direct_inverse != inner->ops)
        return NULL;

    arg = inner->a;
    dv_retain(arg);
    dv_free(inner);
    return arg;
}

dval_t *dv_simplify_direct_inverse_pair_from_raw(const dval_t *outer,
                                                 const dval_t *raw_inner,
                                                 dval_t *simplified_inner)
{
    dval_t *arg;

    if (!outer || !raw_inner || raw_inner->ops->arity != DV_OP_UNARY ||
        outer->ops->direct_inverse != raw_inner->ops || !raw_inner->a)
        return NULL;

    arg = dv_simplify(raw_inner->a);
    if (simplified_inner)
        dv_free(simplified_inner);
    return arg;
}

typedef bool (*dv_inverse_candidate_ok_fn)(number_t value);

typedef struct dv_inverse_unary_rule {
    const dval_ops_t *ops;
    dv_inverse_candidate_ok_fn candidate_ok;
} dv_inverse_unary_rule_t;

static bool dv_lambert_w_candidate_ok(number_t value)
{
    number_t imag;
    number_t neg_pi;
    bool ok;

    if (!num_is_finite(value))
        return false;

    if (num_is_real(value))
        return true;

    /* The branch-selecting W/productlog uses the principal complex strip. */
    imag = num_imag_part(value);
    neg_pi = num_neg(NUM_PI);
    ok = num_gt(imag, neg_pi) && num_lt(imag, NUM_PI);
    num_destroy(&neg_pi);
    num_destroy(&imag);
    return ok;
}

static bool dv_lambert_w0_candidate_ok(number_t value)
{
    return num_is_finite(value) &&
           num_is_real(value) &&
           num_ge(value, NUM_NEG_ONE);
}

static bool dv_lambert_wm1_candidate_ok(number_t value)
{
    return num_is_finite(value) &&
           num_is_real(value) &&
           num_le(value, NUM_NEG_ONE);
}

static const dv_inverse_unary_rule_t s_inverse_unary_rules[] = {
    { &ops_lambert_w,   dv_lambert_w_candidate_ok },
    { &ops_lambert_w0,  dv_lambert_w0_candidate_ok },
    { &ops_lambert_wm1, dv_lambert_wm1_candidate_ok },
    { &ops_log10,       NULL },
};

static const dv_inverse_unary_rule_t *
dv_inverse_unary_rule_for(const dval_ops_t *ops)
{
    size_t i;

    if (!ops)
        return NULL;

    for (i = 0; i < sizeof(s_inverse_unary_rules) /
                    sizeof(s_inverse_unary_rules[0]); ++i) {
        if (s_inverse_unary_rules[i].ops == ops)
            return &s_inverse_unary_rules[i];
    }
    return NULL;
}

static bool dv_inverse_unary_pattern_supported(const dval_ops_t *ops)
{
    return dv_inverse_unary_rule_for(ops) != NULL;
}

bool dv_lambert_candidate_on_selected_branch(const dval_ops_t *ops,
                                             number_t value)
{
    const dv_inverse_unary_rule_t *rule = dv_inverse_unary_rule_for(ops);

    if (!rule || !rule->candidate_ok)
        return false;

    return rule->candidate_ok(value);
}

typedef dval_t *(*dv_binary_simplify_rule_fn)(dval_t *a, dval_t *b);

typedef struct dv_binary_simplify_rule {
    dv_binary_simplify_rule_fn apply;
} dv_binary_simplify_rule_t;

static dval_t *dv_simplify_repeated_factor(dval_t *a, dval_t *b)
{
    if (!dv_struct_eq(a, b))
        return NULL;
    return dv_pow(a, &NUM_TWO);
}

static dval_t *dv_add_one_to_arg(const dval_t *arg)
{
    dval_t *one;
    dval_t *raw;
    dval_t *out;

    one = dv_new_const(NUM_ONE);
    raw = dv_add(arg, one);
    out = dv_simplify(raw);

    dv_free(raw);
    dv_free(one);
    return out;
}

static dval_t *dv_simplify_gamma_successor(dval_t *a, dval_t *b)
{
    const dval_t *gamma = NULL;
    const dval_t *factor = NULL;
    dval_t *successor_arg;
    dval_t *out;

    if (dv_is_op(a, &ops_gamma)) {
        gamma = a;
        factor = b;
    } else if (dv_is_op(b, &ops_gamma)) {
        gamma = b;
        factor = a;
    }

    if (!gamma || !factor || !dv_struct_eq(factor, gamma->a))
        return NULL;

    successor_arg = dv_add_one_to_arg(gamma->a);
    out = dv_gamma(successor_arg);
    dv_free(successor_arg);
    return out;
}

static dval_t *dv_simplify_lgamma_successor(dval_t *a, dval_t *b)
{
    const dval_t *log = NULL;
    const dval_t *lgamma = NULL;
    dval_t *successor_arg;
    dval_t *out;

    if (dv_is_op(a, &ops_log) && dv_is_op(b, &ops_lgamma)) {
        log = a;
        lgamma = b;
    } else if (dv_is_op(a, &ops_lgamma) && dv_is_op(b, &ops_log)) {
        lgamma = a;
        log = b;
    }

    if (!log || !lgamma || !dv_struct_eq(log->a, lgamma->a))
        return NULL;

    successor_arg = dv_add_one_to_arg(lgamma->a);
    out = dv_lgamma(successor_arg);
    dv_free(successor_arg);
    return out;
}

static const dv_binary_simplify_rule_t s_sum_rules[] = {
    { dv_simplify_lgamma_successor },
};

static const dv_binary_simplify_rule_t s_product_rules[] = {
    { dv_simplify_repeated_factor },
    { dv_simplify_gamma_successor },
};

dval_t *dv_simplify_try_basic_sum(dval_t *a, dval_t *b)
{
    size_t i;

    if (!a || !b)
        return NULL;

    for (i = 0; i < sizeof(s_sum_rules) / sizeof(s_sum_rules[0]); ++i) {
        dval_t *out = s_sum_rules[i].apply(a, b);

        if (out)
            return out;
    }
    return NULL;
}

dval_t *dv_simplify_try_basic_product(dval_t *a, dval_t *b)
{
    size_t i;

    if (!a || !b)
        return NULL;

    for (i = 0; i < sizeof(s_product_rules) / sizeof(s_product_rules[0]);
         ++i) {
        dval_t *out = s_product_rules[i].apply(a, b);

        if (out)
            return out;
    }
    return NULL;
}

static bool dv_inverse_unary_candidate_domain_ok(const dval_ops_t *ops,
                                                 const dval_t *candidate)
{
    const dv_inverse_unary_rule_t *rule;
    number_t value;
    bool ok = true;

    if (!ops || !candidate)
        return false;

    rule = dv_inverse_unary_rule_for(ops);
    if (!rule)
        return false;
    if (!rule->candidate_ok)
        return true;

    value = dv_eval(candidate);
    ok = rule->candidate_ok(value);
    num_destroy(&value);
    return ok;
}

static dval_t *dv_try_simplify_vtable_inverse_candidate(
    const dval_t *outer,
    const dval_t *arg,
    const dval_t *candidate)
{
    dval_t *inverse;
    dval_t *simplified_inverse;
    dval_t *out = NULL;

    if (!outer || !outer->ops || !outer->ops->inverse_unary ||
        !dv_inverse_unary_pattern_supported(outer->ops) ||
        !arg || !candidate ||
        !dv_inverse_unary_candidate_domain_ok(outer->ops, candidate))
        return NULL;

    inverse = outer->ops->inverse_unary(candidate);
    if (!inverse)
        return NULL;

    simplified_inverse = dv_simplify(inverse);
    dv_free(inverse);

    if (dv_struct_eq(simplified_inverse, arg)) {
        dv_retain((dval_t *)candidate);
        out = (dval_t *)candidate;
    }

    dv_free(simplified_inverse);
    return out;
}

static const dval_t *dv_extract_exp_product_argument(const dval_t *arg)
{
    if (!dv_is_op(arg, &ops_mul))
        return NULL;

    if (dv_is_exp_expr(arg->a) && dv_struct_eq(arg->a->a, arg->b))
        return arg->b;

    if (dv_is_exp_expr(arg->b) && dv_struct_eq(arg->b->a, arg->a))
        return arg->a;

    return NULL;
}

dval_t *dv_simplify_try_vtable_inverse_argument(const dval_t *outer,
                                                const dval_t *arg)
{
    dval_t *out;
    const dval_t *exp_product_arg;

    if (!arg)
        return NULL;

    if (outer && (outer->ops == &ops_lambert_w ||
                  outer->ops == &ops_lambert_w0 ||
                  outer->ops == &ops_lambert_wm1)) {
        exp_product_arg = dv_extract_exp_product_argument(arg);
        out = dv_try_simplify_vtable_inverse_candidate(outer, arg,
                                                       exp_product_arg);
        if (out)
            return out;
    }

    return dv_try_simplify_vtable_inverse_candidate(outer, arg, arg);
}

static bool dv_is_trig_square_of(const dval_t *dv,
                                 const dval_ops_t *op,
                                 const dval_t **arg_out)
{
    if (!dv_is_pow_d_expr(dv) || !num_eq(dv->c, NUM_TWO))
        return false;
    if (!dv_is_op(dv->a, op))
        return false;

    *arg_out = dv->a->a;
    return true;
}

static dval_t *dv_double_arg_unary(const dval_t *arg, dval_apply_unary_fn builder)
{
    dval_t *two;
    dval_t *raw_double_arg;
    dval_t *double_arg;
    dval_t *out;

    two = dv_new_const(NUM_TWO);
    raw_double_arg = dv_mul(two, arg);
    double_arg = dv_simplify(raw_double_arg);
    out = builder(double_arg);

    dv_free(two);
    dv_free(raw_double_arg);
    dv_free(double_arg);
    return out;
}

dval_t *dv_try_trig_pythagorean_identity(const addend_t *terms, size_t n,
                                         number_t c_const,
                                         number_t common_coeff)
{
    const dval_t *sin_arg = NULL;
    const dval_t *cos_arg = NULL;
    const dval_t *sinh_arg = NULL;
    const dval_t *cosh_arg = NULL;
    size_t nonzero_terms = 0;
    bool have_sin = false;
    bool have_cos = false;
    bool have_neg_sin = false;
    bool have_sinh = false;
    bool have_neg_sinh = false;
    bool have_cosh = false;

    if (!num_is_zero(c_const))
        return NULL;

    for (size_t i = 0; i < n; ++i) {
        if (!terms[i].base || num_is_zero(terms[i].coeff))
            continue;
        nonzero_terms++;
        if (nonzero_terms > 2)
            return NULL;

        if (num_is_one(terms[i].coeff) &&
            dv_is_trig_square_of(terms[i].base, &ops_sin, &sin_arg)) {
            have_sin = true;
            continue;
        }
        if (num_is_one(terms[i].coeff) &&
            dv_is_trig_square_of(terms[i].base, &ops_cos, &cos_arg)) {
            have_cos = true;
            continue;
        }
        if (num_eq(terms[i].coeff, NUM_NEG_ONE) &&
            dv_is_trig_square_of(terms[i].base, &ops_sin, &sin_arg)) {
            have_neg_sin = true;
            continue;
        }
        if (num_is_one(terms[i].coeff) &&
            dv_is_trig_square_of(terms[i].base, &ops_cosh, &cosh_arg)) {
            have_cosh = true;
            continue;
        }
        if (num_is_one(terms[i].coeff) &&
            dv_is_trig_square_of(terms[i].base, &ops_sinh, &sinh_arg)) {
            have_sinh = true;
            continue;
        }
        if (num_eq(terms[i].coeff, NUM_NEG_ONE) &&
            dv_is_trig_square_of(terms[i].base, &ops_sinh, &sinh_arg)) {
            have_neg_sinh = true;
            continue;
        }
        return NULL;
    }

    if (nonzero_terms != 2)
        return NULL;
    if (have_sin && have_cos && sin_arg && cos_arg &&
        dv_struct_eq(sin_arg, cos_arg))
        return dv_new_const(common_coeff);
    if (have_neg_sin && have_cos && sin_arg && cos_arg &&
        dv_struct_eq(sin_arg, cos_arg))
        return dv_make_scaled(common_coeff, dv_double_arg_unary(cos_arg, dv_cos));
    if (have_sinh && have_cosh && sinh_arg && cosh_arg &&
        dv_struct_eq(sinh_arg, cosh_arg))
        return dv_make_scaled(common_coeff, dv_double_arg_unary(cosh_arg, dv_cosh));
    if (have_neg_sinh && have_cosh && sinh_arg && cosh_arg &&
        dv_struct_eq(sinh_arg, cosh_arg))
        return dv_new_const(common_coeff);

    return NULL;
}

static dval_t *dv_simplify_try_double_angle_product(dval_t *a,
                                                    dval_t *b,
                                                    const dval_ops_t *left_op,
                                                    const dval_ops_t *right_op,
                                                    dval_apply_unary_fn builder)
{
    const dval_t *arg = NULL;
    dval_t *double_angle;

    if (dv_is_op(a, left_op) && dv_is_op(b, right_op) &&
        dv_struct_eq(a->a, b->a)) {
        arg = a->a;
    } else if (dv_is_op(a, right_op) && dv_is_op(b, left_op) &&
               dv_struct_eq(a->a, b->a)) {
        arg = a->a;
    }

    if (!arg)
        return NULL;

    double_angle = dv_double_arg_unary(arg, builder);

    return dv_make_scaled(NUM_HALF, double_angle);
}

dval_t *dv_simplify_try_trig_product(dval_t *a, dval_t *b)
{
    const dval_t *arg = NULL;
    dval_apply_unary_fn builder = NULL;
    dval_t *double_angle = dv_simplify_try_double_angle_product(
        a, b, &ops_sin, &ops_cos, dv_sin);

    if (double_angle)
        return double_angle;

    double_angle = dv_simplify_try_double_angle_product(
        a, b, &ops_sinh, &ops_cosh, dv_sinh);
    if (double_angle)
        return double_angle;

    if (dv_is_op(a, &ops_cos) && dv_is_op(b, &ops_tan) &&
        dv_struct_eq(a->a, b->a)) {
        arg = a->a;
        builder = dv_sin;
    } else if (dv_is_op(a, &ops_tan) && dv_is_op(b, &ops_cos) &&
               dv_struct_eq(a->a, b->a)) {
        arg = a->a;
        builder = dv_sin;
    } else if (dv_is_op(a, &ops_cosh) && dv_is_op(b, &ops_tanh) &&
               dv_struct_eq(a->a, b->a)) {
        arg = a->a;
        builder = dv_sinh;
    } else if (dv_is_op(a, &ops_tanh) && dv_is_op(b, &ops_cosh) &&
               dv_struct_eq(a->a, b->a)) {
        arg = a->a;
        builder = dv_sinh;
    }

    if (!arg || !builder)
        return NULL;

    return builder(arg);
}

static bool dv_is_lambert_expr(const dval_t *dv)
{
    return dv_is_op(dv, &ops_lambert_w) ||
           dv_is_op(dv, &ops_lambert_w0) ||
           dv_is_op(dv, &ops_lambert_wm1);
}

dval_t *dv_simplify_try_lambert_product(dval_t *a, dval_t *b)
{
    dval_t *w;
    dval_t *exp_term;
    dval_t *inner;

    if (dv_is_lambert_expr(a) && dv_is_exp_expr(b)) {
        w = a;
        exp_term = b;
    } else if (dv_is_lambert_expr(b) && dv_is_exp_expr(a)) {
        w = b;
        exp_term = a;
    } else {
        return NULL;
    }

    if (!dv_struct_eq(w, exp_term->a))
        return NULL;

    inner = w->a;

    if (!dv_current_wrt_internal() && dv_is_var(inner) && inner->binding_expr)
        return dv_binding_expr_eval_dval(inner->binding_expr);

    dv_retain(inner);
    return inner;
}

static bool dv_is_i_const(const dval_t *dv)
{
    return dv && dv_is_op(dv, &ops_const) && num_eq(dv->c, NUM_I);
}

static bool dv_i_unit_sign(const dval_t *dv, int *sign_out)
{
    int child_sign;

    if (!dv || !sign_out)
        return false;

    if (dv_is_op(dv, &ops_const)) {
        if (num_eq(dv->c, NUM_I)) {
            *sign_out = 1;
            return true;
        }
        if (num_eq(dv->c, NUM_NEG_I)) {
            *sign_out = -1;
            return true;
        }
    }

    if (dv_is_op(dv, &ops_neg) &&
        dv_i_unit_sign(dv->a, &child_sign)) {
        *sign_out = -child_sign;
        return true;
    }

    return false;
}

static bool dv_extract_i_unit_factor(const dval_t *dv,
                                     int *sign_out,
                                     const dval_t **rest_out)
{
    if (!dv || !sign_out || !rest_out)
        return false;

    if (dv_i_unit_sign(dv, sign_out)) {
        *rest_out = NULL;
        return true;
    }

    if (dv_is_op(dv, &ops_mul)) {
        if (dv_i_unit_sign(dv->a, sign_out)) {
            *rest_out = dv->b;
            return true;
        }
        if (dv_i_unit_sign(dv->b, sign_out)) {
            *rest_out = dv->a;
            return true;
        }
    }

    return false;
}

dval_t *dv_simplify_try_i_unit_product(dval_t *a, dval_t *b)
{
    const dval_t *a_rest = NULL;
    const dval_t *b_rest = NULL;
    dval_t *base = NULL;
    dval_t *simp = NULL;
    int a_sign;
    int b_sign;
    int coeff_sign;

    if (!dv_extract_i_unit_factor(a, &a_sign, &a_rest) ||
        !dv_extract_i_unit_factor(b, &b_sign, &b_rest))
        return NULL;

    coeff_sign = -(a_sign * b_sign);

    if (a_rest && b_rest) {
        dv_retain(a_rest);
        dv_retain(b_rest);
        base = dv_mul(a_rest, b_rest);
        simp = dv_simplify(base);
        dv_free(base);
        base = simp;
    } else if (a_rest) {
        dv_retain(a_rest);
        base = (dval_t *)a_rest;
    } else if (b_rest) {
        dv_retain(b_rest);
        base = (dval_t *)b_rest;
    } else {
        base = dv_new_const(NUM_ONE);
    }

    if (coeff_sign < 0)
        return dv_make_scaled(NUM_NEG_ONE, base);
    return base;
}

static bool dv_extract_i_product_arg(const dval_t *dv, const dval_t **arg_out)
{
    if (!dv || !arg_out || !dv_is_op(dv, &ops_mul))
        return false;

    if (dv_is_i_const(dv->a)) {
        *arg_out = dv->b;
        return true;
    }
    if (dv_is_i_const(dv->b)) {
        *arg_out = dv->a;
        return true;
    }

    return false;
}

dval_t *dv_simplify_try_imag_trig_bridge(const dval_t *op, dval_t *arg_node)
{
    const dval_t *arg_borrowed = NULL;
    dval_t *arg;
    dval_t *inner;
    dval_t *out;
    dval_t *simp;

    if (!dv_extract_i_product_arg(arg_node, &arg_borrowed))
        return NULL;

    dv_retain(arg_borrowed);
    arg = (dval_t *)arg_borrowed;

    if (dv_is_op(op, &ops_cosh)) {
        inner = dv_cos(arg);
        dv_free(arg);
        dv_free(arg_node);
        simp = dv_simplify(inner);
        dv_free(inner);
        return simp;
    }

    if (dv_is_op(op, &ops_cos)) {
        inner = dv_cosh(arg);
        dv_free(arg);
        dv_free(arg_node);
        simp = dv_simplify(inner);
        dv_free(inner);
        return simp;
    }

    if (dv_is_op(op, &ops_sinh) || dv_is_op(op, &ops_sin)) {
        dval_t *i = dv_new_named_const(NUM_I, "i");

        inner = dv_is_op(op, &ops_sinh) ? dv_sin(arg) : dv_sinh(arg);
        out = dv_mul(i, inner);
        dv_free(i);
        dv_free(inner);
        dv_free(arg);
        dv_free(arg_node);
        simp = dv_simplify(out);
        dv_free(out);
        return simp;
    }

    dv_free(arg);
    return NULL;
}

dval_t *dv_simplify_positive_part_if_negative(dval_t *dv)
{
    if (!dv)
        return NULL;

    if (dv_is_neg(dv)) {
        dv_retain(dv->a);
        return dv->a;
    }

    if (dv_is_op(dv, &ops_sub) && dv_const_is_zero(dv->a)) {
        dv_retain(dv->b);
        return dv->b;
    }

    if (dv_simplify_is_plain_real_const(dv) && num_lt(dv->c, NUM_ZERO)) {
        number_t positive = num_neg(dv->c);
        dval_t *out = dv_new_const(positive);

        num_destroy(&positive);
        return out;
    }

    if (dv_is_mul(dv)) {
        dval_t *positive_left = dv_simplify_positive_part_if_negative(dv->a);
        dval_t *positive_right = dv_simplify_positive_part_if_negative(dv->b);

        if (positive_left && positive_right) {
            dv_free(positive_left);
            dv_free(positive_right);
            return NULL;
        }
        if (positive_left) {
            dval_t *right = dv->b;
            dval_t *out;

            dv_retain(right);
            out = dv_mul(positive_left, right);
            dv_free(positive_left);
            dv_free(right);
            return out;
        }
        if (positive_right) {
            dval_t *left = dv->a;
            dval_t *out;

            dv_retain(left);
            out = dv_mul(left, positive_right);
            dv_free(left);
            dv_free(positive_right);
            return out;
        }
    }

    if (dv_is_div(dv)) {
        dval_t *positive_num = dv_simplify_positive_part_if_negative(dv->a);
        dval_t *positive_den = dv_simplify_positive_part_if_negative(dv->b);

        if (positive_num && positive_den) {
            dv_free(positive_num);
            dv_free(positive_den);
            return NULL;
        }
        if (positive_num) {
            dval_t *den = dv->b;
            dval_t *out;

            dv_retain(den);
            out = dv_div(positive_num, den);
            dv_free(positive_num);
            dv_free(den);
            return out;
        }
        if (positive_den) {
            dval_t *num = dv->a;
            dval_t *out;

            dv_retain(num);
            out = dv_div(num, positive_den);
            dv_free(num);
            dv_free(positive_den);
            return out;
        }
    }

    return NULL;
}
