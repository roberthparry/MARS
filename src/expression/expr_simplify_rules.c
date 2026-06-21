#include "expr_bindings.h"
#include "expr_internal.h"

bool expr_simplify_is_plain_real_const(const expr_t *dv)
{
    if (!expr_is_unnamed_const(dv) || !num_is_real(dv->c))
        return false;
    if (!num_is_exact(dv->c) && num_constant_name(dv->c))
        return false;
    if (dv->binding_expr && dv->binding_expr->kind != EXPR_BINDING_EXPR_NUMBER)
        return false;
    return true;
}

bool expr_simplify_try_get_plain_real_const(const expr_t *dv, number_t *out)
{
    if (!expr_simplify_is_plain_real_const(dv))
        return false;
    *out = num_clone(dv->c);
    return true;
}

bool expr_simplify_is_simplifiable_const(const expr_t *dv)
{
    return expr_is_op(dv, &ops_const) &&
           (!dv->name || !*dv->name || !dv->binding_expr);
}

bool expr_simplify_allows_const_identity_fold(const expr_t *dv)
{
    if (!dv || !expr_is_op(dv, &ops_const))
        return false;
    if (!dv->binding_expr)
        return true;
    if (dv->name && *dv->name) {
        number_t value;
        bool is_default;
        int has_default;

        has_default = expr_get_default_constant_num(dv->name, &value);
        is_default = has_default && num_eq(dv->c, value);
        if (has_default)
            num_destroy(&value);
        return is_default;
    }
    return dv->binding_expr->kind == EXPR_BINDING_EXPR_NUMBER ||
           dv->binding_expr->kind == EXPR_BINDING_EXPR_CONST;
}

number_t expr_simplify_normalise_simple_rational_coeff(number_t coeff)
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

expr_t *expr_simplify_try_log10_power_of_ten(expr_t *arg)
{
    expr_t *inner;

    if (!expr_is_op(arg, &ops_pow) ||
        !expr_is_op(arg->a, &ops_const) ||
        !num_eq(arg->a->c, NUM_TEN))
        return NULL;

    inner = arg->b;
    expr_retain(inner);
    expr_free(arg);
    return inner;
}

expr_t *expr_simplify_try_floor_ceil_const(const expr_t *op, expr_t *arg)
{
    number_t folded;
    expr_t *out;

    if (!expr_is_op(arg, &ops_const) ||
        !expr_ops_is_floor_or_ceil(op ? op->ops : NULL))
        return NULL;

    folded = expr_is_op(op, &ops_floor) ? num_floor(arg->c) : num_ceil(arg->c);
    out = expr_new_const(folded);
    num_destroy(&folded);
    expr_free(arg);
    return out;
}

static expr_t *expr_new_pi_ratio_long(long numer, unsigned long denom)
{
    expr_t *base = NULL;
    expr_t *coeff = NULL;
    expr_t *scaled = NULL;
    expr_t *divisor = NULL;
    expr_t *quotient = NULL;
    expr_t *negative = NULL;
    expr_t *out = NULL;
    bool is_negative = false;

    if (denom == 0u)
        return NULL;
    if (numer == 0L)
        return expr_new_const(NUM_ZERO);
    if (numer < 0L) {
        is_negative = true;
        numer = -numer;
    }

    base = expr_new_named_const(NUM_PI, "@pi");
    if (!base)
        goto cleanup;

    if (numer != 1L) {
        number_t coeff_value = num_create_from_long(numer);

        coeff = expr_new_const(coeff_value);
        num_destroy(&coeff_value);
        if (!coeff)
            goto cleanup;
        scaled = expr_mul(base, coeff);
        if (!scaled)
            goto cleanup;
        expr_free(base);
        expr_free(coeff);
        base = expr_simplify_owned(scaled);
        scaled = NULL;
        coeff = NULL;
        if (!base)
            goto cleanup;
    }

    if (denom != 1u) {
        number_t denom_value = num_create_from_long((long)denom);

        divisor = expr_new_const(denom_value);
        num_destroy(&denom_value);
        if (!divisor)
            goto cleanup;
        quotient = expr_div(base, divisor);
        if (!quotient)
            goto cleanup;
        expr_free(base);
        expr_free(divisor);
        base = expr_simplify_owned(quotient);
        quotient = NULL;
        divisor = NULL;
        if (!base)
            goto cleanup;
    }

    if (!is_negative) {
        out = base;
        base = NULL;
        goto cleanup;
    }

    negative = expr_neg(base);
    if (!negative)
        goto cleanup;
    out = expr_simplify_owned(negative);
    negative = NULL;

cleanup:
    expr_free(negative);
    expr_free(quotient);
    expr_free(divisor);
    expr_free(scaled);
    expr_free(coeff);
    expr_free(base);
    return out;
}

static expr_t *expr_simplify_try_unary_symbolic_inverse_fold(const expr_t *op,
                                                             const number_t *value,
                                                             expr_t *arg)
{
    long numer;
    unsigned long denom;
    expr_t *out;

    if (!op || !value || !arg)
        return NULL;
    if (!expr_inverse_trig_exact_pi_ratio(op->ops, value, &numer, &denom))
        return NULL;

    out = expr_new_pi_ratio_long(numer, denom);
    if (!out)
        return NULL;
    expr_free(arg);
    return out;
}

expr_t *expr_simplify_try_unary_const_fold(const expr_t *op, expr_t *arg)
{
    number_t folded;
    expr_t *out;

    if (!expr_simplify_allows_const_identity_fold(arg) ||
        !op->ops->fold_const_unary)
        return NULL;

    folded = num_new();
    if (!op->ops->fold_const_unary(&arg->c, &folded) ||
        !num_is_finite(folded)) {
        num_destroy(&folded);
        return NULL;
    }

    out = expr_simplify_try_unary_symbolic_inverse_fold(op, &arg->c, arg);
    if (out) {
        num_destroy(&folded);
        return out;
    }

    out = expr_new_const(folded);
    num_destroy(&folded);
    expr_free(arg);
    return out;
}

static bool expr_contains_var_for_value_fold(const expr_t *dv)
{
    if (!dv)
        return false;
    if (expr_is_var(dv))
        return true;
    return expr_contains_var_for_value_fold(dv->a) ||
           expr_contains_var_for_value_fold(dv->b);
}

expr_t *expr_simplify_try_unary_const_value_fold(const expr_t *op, expr_t *arg)
{
    number_t value;
    number_t folded;
    expr_t *out = NULL;

    if (!arg || !op || !op->ops->fold_const_unary ||
        expr_contains_var_for_value_fold(arg))
        return NULL;

    value = expr_eval(arg);
    folded = num_new();
    if (num_is_finite(value) &&
        op->ops->fold_const_unary(&value, &folded) &&
        num_is_finite(folded)) {
        out = expr_simplify_try_unary_symbolic_inverse_fold(op, &value, arg);
        if (!out) {
            out = expr_new_const(folded);
            expr_free(arg);
        }
    }

    num_destroy(&folded);
    num_destroy(&value);
    return out;
}

expr_t *expr_simplify_try_sqrt_scaled_square_const(expr_t *arg)
{
    number_t coeff_root;
    number_t coeff_square;
    expr_t *raw;
    expr_t *simp;
    expr_t *out;

    if (!expr_is_op(arg, &ops_mul) ||
        !expr_simplify_is_plain_real_const(arg->a) ||
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

    expr_retain(arg->b);
    raw = expr_sqrt(arg->b);
    expr_free(arg->b);
    simp = expr_simplify(raw);
    expr_free(raw);
    expr_free(arg);
    out = expr_make_scaled(coeff_root, simp);
    num_destroy(&coeff_root);
    return out;
}

expr_t *expr_simplify_try_sqrt_quotient(expr_t *num, expr_t *den)
{
    expr_t *quotient;
    expr_t *simplified_quotient;
    expr_t *root;
    expr_t *out;

    if (!expr_is_sqrt_expr(num) || !expr_is_sqrt_expr(den) ||
        !num->a || !den->a ||
        !expr_is_const(den->a) ||
        !num_is_real(den->a->c) ||
        !num_gt(den->a->c, NUM_ZERO))
        return NULL;

    quotient = expr_div(num->a, den->a);
    simplified_quotient = quotient ? expr_simplify(quotient) : NULL;
    root = simplified_quotient ? expr_sqrt(simplified_quotient) : NULL;
    out = root ? expr_simplify(root) : NULL;

    expr_free(root);
    expr_free(simplified_quotient);
    expr_free(quotient);
    if (out) {
        expr_free(num);
        expr_free(den);
    }
    return out;
}

expr_t *expr_simplify_direct_inverse_pair(const expr_t *outer, expr_t *inner)
{
    expr_t *arg;

    if (!outer || !inner || inner->ops->arity != EXPR_OP_UNARY ||
        !expr_ops_are_direct_inverse_pair(outer->ops, inner->ops))
        return NULL;

    arg = inner->a;
    expr_retain(arg);
    expr_free(inner);
    return arg;
}

expr_t *expr_simplify_direct_inverse_pair_from_raw(const expr_t *outer,
                                                 const expr_t *raw_inner,
                                                 expr_t *simplified_inner)
{
    expr_t *arg;

    if (!outer || !raw_inner || raw_inner->ops->arity != EXPR_OP_UNARY ||
        !expr_ops_are_direct_inverse_pair(outer->ops, raw_inner->ops) ||
        !raw_inner->a)
        return NULL;

    arg = expr_simplify(raw_inner->a);
    if (simplified_inner)
        expr_free(simplified_inner);
    return arg;
}

typedef expr_t *(*expr_binary_simplify_rule_fn)(expr_t *a, expr_t *b);

typedef struct expr_binary_simplify_rule {
    expr_binary_simplify_rule_fn apply;
} expr_binary_simplify_rule_t;

static expr_t *expr_simplify_repeated_factor(expr_t *a, expr_t *b)
{
    if (!expr_struct_eq(a, b))
        return NULL;
    return expr_pow(a, &NUM_TWO);
}

static expr_t *expr_add_one_to_arg(const expr_t *arg)
{
    expr_t *one;
    expr_t *raw;
    expr_t *out;

    one = expr_new_const(NUM_ONE);
    raw = expr_add(arg, one);
    out = expr_simplify(raw);

    expr_free(raw);
    expr_free(one);
    return out;
}

static expr_t *expr_simplify_gamma_successor(expr_t *a, expr_t *b)
{
    const expr_t *gamma = NULL;
    const expr_t *factor = NULL;
    expr_t *successor_arg;
    expr_t *out;

    if (expr_is_op(a, &ops_gamma)) {
        gamma = a;
        factor = b;
    } else if (expr_is_op(b, &ops_gamma)) {
        gamma = b;
        factor = a;
    }

    if (!gamma || !factor || !expr_struct_eq(factor, gamma->a))
        return NULL;

    successor_arg = expr_add_one_to_arg(gamma->a);
    out = expr_gamma(successor_arg);
    expr_free(successor_arg);
    return out;
}

static expr_t *expr_simplify_lgamma_successor(expr_t *a, expr_t *b)
{
    const expr_t *log = NULL;
    const expr_t *lgamma = NULL;
    expr_t *successor_arg;
    expr_t *out;

    if (expr_is_op(a, &ops_log) && expr_is_op(b, &ops_lgamma)) {
        log = a;
        lgamma = b;
    } else if (expr_is_op(a, &ops_lgamma) && expr_is_op(b, &ops_log)) {
        lgamma = a;
        log = b;
    }

    if (!log || !lgamma || !expr_struct_eq(log->a, lgamma->a))
        return NULL;

    successor_arg = expr_add_one_to_arg(lgamma->a);
    out = expr_lgamma(successor_arg);
    expr_free(successor_arg);
    return out;
}

static const expr_binary_simplify_rule_t s_sum_rules[] = {
    { expr_simplify_lgamma_successor },
};

static const expr_binary_simplify_rule_t s_product_rules[] = {
    { expr_simplify_repeated_factor },
    { expr_simplify_gamma_successor },
};

expr_t *expr_simplify_try_basic_sum(expr_t *a, expr_t *b)
{
    size_t i;

    if (!a || !b)
        return NULL;

    for (i = 0; i < sizeof(s_sum_rules) / sizeof(s_sum_rules[0]); ++i) {
        expr_t *out = s_sum_rules[i].apply(a, b);

        if (out)
            return out;
    }
    return NULL;
}

expr_t *expr_simplify_try_basic_product(expr_t *a, expr_t *b)
{
    size_t i;

    if (!a || !b)
        return NULL;

    for (i = 0; i < sizeof(s_product_rules) / sizeof(s_product_rules[0]);
         ++i) {
        expr_t *out = s_product_rules[i].apply(a, b);

        if (out)
            return out;
    }
    return NULL;
}

static bool expr_inverse_unary_candidate_domain_ok(const expr_ops_t *ops,
                                                 const expr_t *candidate)
{
    number_t value;
    bool ok = true;

    if (!ops || !candidate)
        return false;

    value = expr_eval(candidate);
    ok = expr_inverse_unary_candidate_value_ok(ops, value);
    num_destroy(&value);
    return ok;
}

static expr_t *expr_try_simplify_vtable_inverse_candidate(
    const expr_t *outer,
    const expr_t *arg,
    const expr_t *candidate)
{
    expr_t *inverse;
    expr_t *simplified_inverse;
    expr_t *out = NULL;

    if (!outer || !outer->ops || !outer->ops->inverse_unary ||
        !expr_ops_has_inverse_unary_simplify_rule(outer->ops) ||
        !arg || !candidate ||
        !expr_inverse_unary_candidate_domain_ok(outer->ops, candidate))
        return NULL;

    inverse = outer->ops->inverse_unary(candidate);
    if (!inverse)
        return NULL;

    simplified_inverse = expr_simplify(inverse);
    expr_free(inverse);

    if (expr_struct_eq(simplified_inverse, arg)) {
        expr_retain((expr_t *)candidate);
        out = (expr_t *)candidate;
    }

    expr_free(simplified_inverse);
    return out;
}

static const expr_t *expr_extract_exp_product_argument(const expr_t *arg)
{
    if (!expr_is_op(arg, &ops_mul))
        return NULL;

    if (expr_is_exp_expr(arg->a) && expr_struct_eq(arg->a->a, arg->b))
        return arg->b;

    if (expr_is_exp_expr(arg->b) && expr_struct_eq(arg->b->a, arg->a))
        return arg->a;

    return NULL;
}

expr_t *expr_simplify_try_vtable_inverse_argument(const expr_t *outer,
                                                const expr_t *arg)
{
    expr_t *out;
    const expr_t *exp_product_arg;

    if (!arg)
        return NULL;

    if (outer && expr_ops_is_lambert(outer->ops)) {
        exp_product_arg = expr_extract_exp_product_argument(arg);
        out = expr_try_simplify_vtable_inverse_candidate(outer, arg,
                                                       exp_product_arg);
        if (out)
            return out;
    }

    return expr_try_simplify_vtable_inverse_candidate(outer, arg, arg);
}

static bool expr_is_trig_square_of(const expr_t *dv,
                                 const expr_ops_t *op,
                                 const expr_t **arg_out)
{
    if (!expr_is_pow_d_expr(dv) || !num_eq(dv->c, NUM_TWO))
        return false;
    if (!expr_is_op(dv->a, op))
        return false;

    *arg_out = dv->a->a;
    return true;
}

static expr_t *expr_double_arg_unary(const expr_t *arg, expr_apply_unary_fn builder)
{
    expr_t *two;
    expr_t *raw_double_arg;
    expr_t *double_arg;
    expr_t *out;

    two = expr_new_const(NUM_TWO);
    raw_double_arg = expr_mul(two, arg);
    double_arg = expr_simplify(raw_double_arg);
    out = builder(double_arg);

    expr_free(two);
    expr_free(raw_double_arg);
    expr_free(double_arg);
    return out;
}

bool expr_match_double_argument(const expr_t *expr, const expr_t *arg)
{
    const expr_t *factor = NULL;
    const expr_t *other = NULL;
    number_t factor_value = num_new();
    bool matched = false;

    if (!expr || !arg || !expr_match_mul_expr(expr, &factor, &other))
        goto cleanup;

    if (expr_match_const_value(factor, &factor_value) &&
        num_eq(factor_value, NUM_TWO) &&
        expr_struct_eq(other, arg)) {
        matched = true;
        goto cleanup;
    }

    num_destroy(&factor_value);
    factor_value = num_new();
    if (expr_match_const_value(other, &factor_value) &&
        num_eq(factor_value, NUM_TWO) &&
        expr_struct_eq(factor, arg)) {
        matched = true;
    }

cleanup:
    num_destroy(&factor_value);
    return matched;
}

expr_t *expr_try_trig_pythagorean_identity(const addend_t *terms, size_t n,
                                         number_t c_const,
                                         number_t common_coeff)
{
    const expr_t *sin_arg = NULL;
    const expr_t *cos_arg = NULL;
    const expr_t *sinh_arg = NULL;
    const expr_t *cosh_arg = NULL;
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
            expr_is_trig_square_of(terms[i].base, &ops_sin, &sin_arg)) {
            have_sin = true;
            continue;
        }
        if (num_is_one(terms[i].coeff) &&
            expr_is_trig_square_of(terms[i].base, &ops_cos, &cos_arg)) {
            have_cos = true;
            continue;
        }
        if (num_eq(terms[i].coeff, NUM_NEG_ONE) &&
            expr_is_trig_square_of(terms[i].base, &ops_sin, &sin_arg)) {
            have_neg_sin = true;
            continue;
        }
        if (num_is_one(terms[i].coeff) &&
            expr_is_trig_square_of(terms[i].base, &ops_cosh, &cosh_arg)) {
            have_cosh = true;
            continue;
        }
        if (num_is_one(terms[i].coeff) &&
            expr_is_trig_square_of(terms[i].base, &ops_sinh, &sinh_arg)) {
            have_sinh = true;
            continue;
        }
        if (num_eq(terms[i].coeff, NUM_NEG_ONE) &&
            expr_is_trig_square_of(terms[i].base, &ops_sinh, &sinh_arg)) {
            have_neg_sinh = true;
            continue;
        }
        return NULL;
    }

    if (nonzero_terms != 2)
        return NULL;
    if (have_sin && have_cos && sin_arg && cos_arg &&
        expr_struct_eq(sin_arg, cos_arg))
        return expr_new_const(common_coeff);
    if (have_neg_sin && have_cos && sin_arg && cos_arg &&
        expr_struct_eq(sin_arg, cos_arg))
        return expr_make_scaled(common_coeff, expr_double_arg_unary(cos_arg, expr_cos));
    if (have_sinh && have_cosh && sinh_arg && cosh_arg &&
        expr_struct_eq(sinh_arg, cosh_arg))
        return expr_make_scaled(common_coeff, expr_double_arg_unary(cosh_arg, expr_cosh));
    if (have_neg_sinh && have_cosh && sinh_arg && cosh_arg &&
        expr_struct_eq(sinh_arg, cosh_arg))
        return expr_new_const(common_coeff);

    return NULL;
}

static expr_t *expr_simplify_try_double_angle_product(expr_t *a,
                                                    expr_t *b,
                                                    const expr_ops_t *left_op,
                                                    const expr_ops_t *right_op,
                                                    expr_apply_unary_fn builder)
{
    const expr_t *arg = NULL;
    expr_t *double_angle;

    if (expr_is_op(a, left_op) && expr_is_op(b, right_op) &&
        expr_struct_eq(a->a, b->a)) {
        arg = a->a;
    } else if (expr_is_op(a, right_op) && expr_is_op(b, left_op) &&
               expr_struct_eq(a->a, b->a)) {
        arg = a->a;
    }

    if (!arg)
        return NULL;

    double_angle = expr_double_arg_unary(arg, builder);

    return expr_make_scaled(NUM_HALF, double_angle);
}

expr_t *expr_simplify_try_trig_product(expr_t *a, expr_t *b)
{
    const expr_t *arg = NULL;
    expr_apply_unary_fn builder = NULL;
    expr_t *double_angle = expr_simplify_try_double_angle_product(
        a, b, &ops_sin, &ops_cos, expr_sin);

    if (double_angle)
        return double_angle;

    double_angle = expr_simplify_try_double_angle_product(
        a, b, &ops_sinh, &ops_cosh, expr_sinh);
    if (double_angle)
        return double_angle;

    if (expr_is_op(a, &ops_cos) && expr_is_op(b, &ops_tan) &&
        expr_struct_eq(a->a, b->a)) {
        arg = a->a;
        builder = expr_sin;
    } else if (expr_is_op(a, &ops_tan) && expr_is_op(b, &ops_cos) &&
               expr_struct_eq(a->a, b->a)) {
        arg = a->a;
        builder = expr_sin;
    } else if (expr_is_op(a, &ops_cosh) && expr_is_op(b, &ops_tanh) &&
               expr_struct_eq(a->a, b->a)) {
        arg = a->a;
        builder = expr_sinh;
    } else if (expr_is_op(a, &ops_tanh) && expr_is_op(b, &ops_cosh) &&
               expr_struct_eq(a->a, b->a)) {
        arg = a->a;
        builder = expr_sinh;
    }

    if (!arg || !builder)
        return NULL;

    return builder(arg);
}

static bool expr_is_lambert_expr(const expr_t *dv)
{
    return dv && expr_ops_is_lambert(dv->ops);
}

expr_t *expr_simplify_try_lambert_exp(expr_t *arg)
{
    number_t inner_value = NUM_ZERO;

    if (!expr_is_lambert_expr(arg))
        return NULL;
    if (expr_is_const(arg->a) && num_is_real(arg->a->c) &&
        num_eq(arg->a->c, NUM_ZERO)) {
        num_destroy(&inner_value);
        return expr_new_const(NUM_ONE);
    }
    if (expr_simplify_try_get_plain_real_const(arg->a, &inner_value) &&
        num_eq(inner_value, NUM_ZERO)) {
        num_destroy(&inner_value);
        return expr_new_const(NUM_ONE);
    }
    num_destroy(&inner_value);

    return expr_div(arg->a, arg);
}

expr_t *expr_simplify_try_lambert_product(expr_t *a, expr_t *b)
{
    expr_t *w;
    expr_t *exp_term;
    expr_t *inner;

    if (expr_is_lambert_expr(a) && expr_is_exp_expr(b)) {
        w = a;
        exp_term = b;
    } else if (expr_is_lambert_expr(b) && expr_is_exp_expr(a)) {
        w = b;
        exp_term = a;
    } else {
        return NULL;
    }

    if (!expr_struct_eq(w, exp_term->a))
        return NULL;

    inner = w->a;

    if (!expr_current_wrt_internal() && expr_is_var(inner) && inner->binding_expr)
        return expr_binding_expr_eval_expr(inner->binding_expr);

    expr_retain(inner);
    return inner;
}

static bool expr_is_i_const(const expr_t *dv)
{
    return dv && expr_is_op(dv, &ops_const) && num_eq(dv->c, NUM_I);
}

static bool expr_i_unit_sign(const expr_t *dv, int *sign_out)
{
    int child_sign;

    if (!dv || !sign_out)
        return false;

    if (expr_is_op(dv, &ops_const)) {
        if (num_eq(dv->c, NUM_I)) {
            *sign_out = 1;
            return true;
        }
        if (num_eq(dv->c, NUM_NEG_I)) {
            *sign_out = -1;
            return true;
        }
    }

    if (expr_is_op(dv, &ops_neg) &&
        expr_i_unit_sign(dv->a, &child_sign)) {
        *sign_out = -child_sign;
        return true;
    }

    return false;
}

static bool expr_extract_i_unit_factor(const expr_t *dv,
                                     int *sign_out,
                                     const expr_t **rest_out)
{
    if (!dv || !sign_out || !rest_out)
        return false;

    if (expr_i_unit_sign(dv, sign_out)) {
        *rest_out = NULL;
        return true;
    }

    if (expr_is_op(dv, &ops_mul)) {
        if (expr_i_unit_sign(dv->a, sign_out)) {
            *rest_out = dv->b;
            return true;
        }
        if (expr_i_unit_sign(dv->b, sign_out)) {
            *rest_out = dv->a;
            return true;
        }
    }

    return false;
}

expr_t *expr_simplify_try_i_unit_product(expr_t *a, expr_t *b)
{
    const expr_t *a_rest = NULL;
    const expr_t *b_rest = NULL;
    expr_t *base = NULL;
    expr_t *simp = NULL;
    int a_sign;
    int b_sign;
    int coeff_sign;

    if (!expr_extract_i_unit_factor(a, &a_sign, &a_rest) ||
        !expr_extract_i_unit_factor(b, &b_sign, &b_rest))
        return NULL;

    coeff_sign = -(a_sign * b_sign);

    if (a_rest && b_rest) {
        expr_retain(a_rest);
        expr_retain(b_rest);
        base = expr_mul(a_rest, b_rest);
        simp = expr_simplify(base);
        expr_free(base);
        base = simp;
    } else if (a_rest) {
        expr_retain(a_rest);
        base = (expr_t *)a_rest;
    } else if (b_rest) {
        expr_retain(b_rest);
        base = (expr_t *)b_rest;
    } else {
        base = expr_new_const(NUM_ONE);
    }

    if (coeff_sign < 0)
        return expr_make_scaled(NUM_NEG_ONE, base);
    return base;
}

static bool expr_extract_i_product_arg(const expr_t *dv, const expr_t **arg_out)
{
    if (!dv || !arg_out || !expr_is_op(dv, &ops_mul))
        return false;

    if (expr_is_i_const(dv->a)) {
        *arg_out = dv->b;
        return true;
    }
    if (expr_is_i_const(dv->b)) {
        *arg_out = dv->a;
        return true;
    }

    return false;
}

expr_t *expr_simplify_try_imag_trig_bridge(const expr_t *op, expr_t *arg_node)
{
    const expr_t *arg_borrowed = NULL;
    expr_t *arg;
    expr_t *inner;
    expr_t *out;
    expr_t *simp;

    if (!expr_extract_i_product_arg(arg_node, &arg_borrowed))
        return NULL;

    expr_retain(arg_borrowed);
    arg = (expr_t *)arg_borrowed;

    if (expr_is_op(op, &ops_cosh)) {
        inner = expr_cos(arg);
        expr_free(arg);
        expr_free(arg_node);
        simp = expr_simplify(inner);
        expr_free(inner);
        return simp;
    }

    if (expr_is_op(op, &ops_cos)) {
        inner = expr_cosh(arg);
        expr_free(arg);
        expr_free(arg_node);
        simp = expr_simplify(inner);
        expr_free(inner);
        return simp;
    }

    if (expr_is_op(op, &ops_sinh) || expr_is_op(op, &ops_sin)) {
        expr_t *i = expr_new_named_const(NUM_I, "i");

        inner = expr_is_op(op, &ops_sinh) ? expr_sin(arg) : expr_sinh(arg);
        out = expr_mul(i, inner);
        expr_free(i);
        expr_free(inner);
        expr_free(arg);
        expr_free(arg_node);
        simp = expr_simplify(out);
        expr_free(out);
        return simp;
    }

    expr_free(arg);
    return NULL;
}

expr_t *expr_simplify_positive_part_if_negative(expr_t *dv)
{
    if (!dv)
        return NULL;

    if (expr_is_neg(dv)) {
        expr_retain(dv->a);
        return dv->a;
    }

    if (expr_is_op(dv, &ops_sub) && expr_const_is_zero(dv->a)) {
        expr_retain(dv->b);
        return dv->b;
    }

    if (expr_simplify_is_plain_real_const(dv) && num_lt(dv->c, NUM_ZERO)) {
        number_t positive = num_neg(dv->c);
        expr_t *out = expr_new_const(positive);

        num_destroy(&positive);
        return out;
    }

    if (expr_is_mul(dv)) {
        expr_t *positive_left = expr_simplify_positive_part_if_negative(dv->a);
        expr_t *positive_right = expr_simplify_positive_part_if_negative(dv->b);

        if (positive_left && positive_right) {
            expr_free(positive_left);
            expr_free(positive_right);
            return NULL;
        }
        if (positive_left) {
            expr_t *right = dv->b;
            expr_t *out;

            expr_retain(right);
            out = expr_mul(positive_left, right);
            expr_free(positive_left);
            expr_free(right);
            return out;
        }
        if (positive_right) {
            expr_t *left = dv->a;
            expr_t *out;

            expr_retain(left);
            out = expr_mul(left, positive_right);
            expr_free(left);
            expr_free(positive_right);
            return out;
        }
    }

    if (expr_is_div(dv)) {
        expr_t *positive_num = expr_simplify_positive_part_if_negative(dv->a);
        expr_t *positive_den = expr_simplify_positive_part_if_negative(dv->b);

        if (positive_num && positive_den) {
            expr_free(positive_num);
            expr_free(positive_den);
            return NULL;
        }
        if (positive_num) {
            expr_t *den = dv->b;
            expr_t *out;

            expr_retain(den);
            out = expr_div(positive_num, den);
            expr_free(positive_num);
            expr_free(den);
            return out;
        }
        if (positive_den) {
            expr_t *num = dv->a;
            expr_t *out;

            expr_retain(num);
            out = expr_div(num, positive_den);
            expr_free(num);
            expr_free(positive_den);
            return out;
        }
    }

    return NULL;
}
