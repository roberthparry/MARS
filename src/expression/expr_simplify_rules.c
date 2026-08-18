#include "expr_bindings.h"
#define MARS_EXPR_INTERNAL_ACCESS
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
    return expr_is_op(dv, &ops_const) && (!dv->name || !*dv->name || !dv->binding_expr);
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
    return dv->binding_expr->kind == EXPR_BINDING_EXPR_NUMBER || dv->binding_expr->kind == EXPR_BINDING_EXPR_CONST;
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

    if (!expr_is_op(arg, &ops_pow) || !expr_is_op(arg->a, &ops_const) || !num_eq(arg->a->c, NUM_TEN))
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

    if (!expr_is_op(arg, &ops_const) || !expr_ops_is_floor_or_ceil(op ? op->ops : NULL))
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

static expr_t *expr_simplify_try_unary_symbolic_inverse_fold(const expr_t *op, const number_t *value, expr_t *arg)
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

    if (!expr_simplify_allows_const_identity_fold(arg) || !op->ops->fold_const_unary)
        return NULL;

    folded = num_new();
    if (!op->ops->fold_const_unary(&arg->c, &folded) || !num_is_finite(folded)) {
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
    return expr_contains_var_for_value_fold(dv->a) || expr_contains_var_for_value_fold(dv->b);
}

expr_t *expr_simplify_try_unary_const_value_fold(const expr_t *op, expr_t *arg)
{
    number_t value;
    number_t folded;
    expr_t *out = NULL;

    if (!arg || !op || !op->ops->fold_const_unary || expr_contains_var_for_value_fold(arg))
        return NULL;

    value = expr_eval(arg);
    folded = num_new();
    if (num_is_finite(value) && op->ops->fold_const_unary(&value, &folded) && num_is_finite(folded)) {
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

    if (!expr_is_op(arg, &ops_mul) || !expr_simplify_is_plain_real_const(arg->a) || !num_gt(arg->a->c, NUM_ZERO))
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

static expr_t *simplify_scaled_radical_quotient_term(const expr_t *term, const expr_t *den,
                                                      const expr_t *den_root)
{
    const expr_t *coefficient;
    const expr_t *remainder;
    expr_t *quotient;
    expr_t *out;

    if (!expr_is_op(term, &ops_mul))
        return NULL;
    if (expr_is_addsub(term->a)) {
        coefficient = term->a;
        remainder = term->b;
    } else if (expr_is_addsub(term->b)) {
        coefficient = term->b;
        remainder = term->a;
    } else {
        return NULL;
    }
    if (!expr_struct_eq(coefficient->a, den_root) || !expr_simplify_is_plain_real_const(coefficient->b))
        return NULL;

    quotient = expr_div_simplify_owned(expr_clone(coefficient), expr_clone(den));
    out = quotient ? expr_mul_simplify_owned(quotient, expr_clone(remainder)) : NULL;
    return out;
}

expr_t *expr_simplify_try_sqrt_quotient(expr_t *num, expr_t *den)
{
    bool pure_square_root;
    bool scaled_square_root;
    const expr_t *scalar = NULL;
    const expr_t *rest = NULL;
    expr_t *quotient;
    expr_t *simplified_quotient;
    expr_t *root;
    expr_t *out;

    pure_square_root = expr_is_sqrt_expr(den) && den->a && expr_is_const(den->a) && num_is_real(den->a->c) &&
                       num_gt(den->a->c, NUM_ZERO);
    scaled_square_root = expr_is_op(den, &ops_mul) &&
                         ((expr_simplify_is_plain_real_const(den->a) && expr_is_sqrt_expr(den->b)) ||
                          (expr_is_sqrt_expr(den->a) && expr_simplify_is_plain_real_const(den->b)));
    if (!pure_square_root && !scaled_square_root)
        return NULL;

    if (scaled_square_root && expr_is_addsub(num)) {
        const expr_t *den_root = expr_simplify_is_plain_real_const(den->a) ? den->b : den->a;
        expr_t *left = simplify_scaled_radical_quotient_term(num->a, den, den_root);
        expr_t *right = simplify_scaled_radical_quotient_term(num->b, den, den_root);

        if (left && right) {
            out = expr_is_op(num, &ops_add) ? expr_add_simplify_owned(left, right)
                                            : expr_sub_simplify_owned(left, right);
            expr_free(num);
            expr_free(den);
            return out;
        }
        expr_free(right);
        expr_free(left);
    }

    if (scaled_square_root && (expr_is_op(num, &ops_add) || expr_is_op(num, &ops_sub))) {
        const expr_t *scale = expr_simplify_is_plain_real_const(den->a) ? den->a : den->b;
        const expr_t *den_root = scale == den->a ? den->b : den->a;

        if (expr_struct_eq(num->a, den_root) && expr_simplify_is_plain_real_const(num->b)) {
            expr_t *ratio = expr_div_simplify_owned(expr_clone(num->b), expr_clone(den_root));
            expr_t *one = expr_new_const(NUM_ONE);
            expr_t *inside = expr_is_op(num, &ops_add) ? expr_add_simplify_owned(one, ratio)
                                                       : expr_sub_simplify_owned(one, ratio);

            out = expr_div_simplify_owned(inside, expr_clone(scale));
            if (out) {
                expr_free(num);
                expr_free(den);
            }
            return out;
        }
    }

    if ((expr_is_op(num, &ops_add) || expr_is_op(num, &ops_sub)) &&
        (pure_square_root || (expr_is_div(num->a) && expr_is_div(num->b)))) {
        expr_t *left = expr_div_simplify_owned(expr_clone(num->a), expr_clone(den));
        expr_t *right = expr_div_simplify_owned(expr_clone(num->b), expr_clone(den));

        out = expr_is_op(num, &ops_add) ? expr_add_simplify_owned(left, right) : expr_sub_simplify_owned(left, right);
        if (out) {
            expr_free(num);
            expr_free(den);
        }
        return out;
    }
    if (!pure_square_root)
        return NULL;

    if (expr_simplify_is_plain_real_const(num)) {
        scalar = num;
    } else if (expr_is_op(num, &ops_mul) && expr_simplify_is_plain_real_const(num->a)) {
        scalar = num->a;
        rest = num->b;
    }
    if (scalar) {
        number_t square = num_mul(scalar->c, scalar->c);
        number_t common_factor;
        number_t radicand;
        expr_t *radicand_expr;
        expr_t *raw_root;
        expr_t *simplified_root;
        expr_t *signed_root;

        if (!num_is_integer(square) || !num_is_integer(den->a->c)) {
            num_destroy(&square);
            return NULL;
        }
        common_factor = num_gcd(square, den->a->c);
        if (!num_gt(common_factor, NUM_ONE)) {
            num_destroy(&common_factor);
            num_destroy(&square);
            return NULL;
        }
        num_destroy(&common_factor);
        radicand = num_div(square, den->a->c);
        radicand_expr = expr_new_const(radicand);
        raw_root = radicand_expr ? expr_sqrt(radicand_expr) : NULL;
        simplified_root = raw_root ? expr_simplify(raw_root) : NULL;
        signed_root = simplified_root;
        num_destroy(&radicand);
        num_destroy(&square);
        expr_free(raw_root);
        expr_free(radicand_expr);
        if (!simplified_root)
            return NULL;
        if (num_lt(scalar->c, NUM_ZERO)) {
            signed_root = expr_neg(simplified_root);
            expr_free(simplified_root);
            if (!signed_root)
                return NULL;
        }
        if (rest) {
            expr_t *raw_product = expr_mul(signed_root, rest);

            out = raw_product ? expr_simplify(raw_product) : NULL;
            expr_free(raw_product);
        } else {
            out = signed_root;
            signed_root = NULL;
        }
        expr_free(signed_root);
        if (out) {
            expr_free(num);
            expr_free(den);
        }
        return out;
    }
    if (!expr_is_sqrt_expr(num) || !num->a)
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

expr_t *expr_simplify_direct_inverse_pair_from_raw(const expr_t *outer, const expr_t *raw_inner,
                                                   expr_t *simplified_inner)
{
    expr_t *arg;

    if (!outer || !raw_inner || raw_inner->ops->arity != EXPR_OP_UNARY ||
        !expr_ops_are_direct_inverse_pair(outer->ops, raw_inner->ops) || !raw_inner->a)
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

static expr_t *expr_simplify_tangent_addition_product(expr_t *a, expr_t *b)
{
    const expr_t *reciprocal = NULL;
    const expr_t *numerator = NULL;

    if (expr_is_div(a) && expr_const_is_one(a->a)) {
        reciprocal = a;
        numerator = b;
    } else if (expr_is_div(b) && expr_const_is_one(b->a)) {
        reciprocal = b;
        numerator = a;
    }

    return reciprocal ? expr_simplify_try_tangent_addition_quotient(numerator, reciprocal->b) : NULL;
}

static const expr_binary_simplify_rule_t s_sum_rules[] = {
    {expr_simplify_lgamma_successor},
};

static const expr_binary_simplify_rule_t s_product_rules[] = {
    {expr_simplify_tangent_addition_product},
    {expr_simplify_repeated_factor},
    {expr_simplify_gamma_successor},
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

    for (i = 0; i < sizeof(s_product_rules) / sizeof(s_product_rules[0]); ++i) {
        expr_t *out = s_product_rules[i].apply(a, b);

        if (out)
            return out;
    }
    return NULL;
}

static bool expr_inverse_unary_candidate_domain_ok(const expr_ops_t *ops, const expr_t *candidate)
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

static expr_t *expr_try_simplify_vtable_inverse_candidate(const expr_t *outer, const expr_t *arg,
                                                          const expr_t *candidate)
{
    expr_t *inverse;
    expr_t *simplified_inverse;
    expr_t *out = NULL;

    if (!outer || !outer->ops || !outer->ops->inverse_unary || !expr_ops_has_inverse_unary_simplify_rule(outer->ops) ||
        !arg || !candidate || !expr_inverse_unary_candidate_domain_ok(outer->ops, candidate))
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

expr_t *expr_simplify_try_vtable_inverse_argument(const expr_t *outer, const expr_t *arg)
{
    expr_t *out;
    const expr_t *exp_product_arg;

    if (!arg)
        return NULL;

    if (outer && expr_ops_is_lambert(outer->ops)) {
        exp_product_arg = expr_extract_exp_product_argument(arg);
        out = expr_try_simplify_vtable_inverse_candidate(outer, arg, exp_product_arg);
        if (out)
            return out;
    }

    return expr_try_simplify_vtable_inverse_candidate(outer, arg, arg);
}

static bool expr_is_trig_square_of(const expr_t *dv, const expr_ops_t *op, const expr_t **arg_out)
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

    if (expr_match_const_value(factor, &factor_value) && num_eq(factor_value, NUM_TWO) && expr_struct_eq(other, arg)) {
        matched = true;
        goto cleanup;
    }

    num_destroy(&factor_value);
    factor_value = num_new();
    if (expr_match_const_value(other, &factor_value) && num_eq(factor_value, NUM_TWO) && expr_struct_eq(factor, arg)) {
        matched = true;
    }

cleanup:
    num_destroy(&factor_value);
    return matched;
}

expr_t *expr_try_trig_pythagorean_identity(const addend_t *terms, size_t n, number_t c_const, number_t common_coeff)
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

        if (num_is_one(terms[i].coeff) && expr_is_trig_square_of(terms[i].base, &ops_sin, &sin_arg)) {
            have_sin = true;
            continue;
        }
        if (num_is_one(terms[i].coeff) && expr_is_trig_square_of(terms[i].base, &ops_cos, &cos_arg)) {
            have_cos = true;
            continue;
        }
        if (num_eq(terms[i].coeff, NUM_NEG_ONE) && expr_is_trig_square_of(terms[i].base, &ops_sin, &sin_arg)) {
            have_neg_sin = true;
            continue;
        }
        if (num_is_one(terms[i].coeff) && expr_is_trig_square_of(terms[i].base, &ops_cosh, &cosh_arg)) {
            have_cosh = true;
            continue;
        }
        if (num_is_one(terms[i].coeff) && expr_is_trig_square_of(terms[i].base, &ops_sinh, &sinh_arg)) {
            have_sinh = true;
            continue;
        }
        if (num_eq(terms[i].coeff, NUM_NEG_ONE) && expr_is_trig_square_of(terms[i].base, &ops_sinh, &sinh_arg)) {
            have_neg_sinh = true;
            continue;
        }
        return NULL;
    }

    if (nonzero_terms != 2)
        return NULL;
    if (have_sin && have_cos && sin_arg && cos_arg && expr_struct_eq(sin_arg, cos_arg))
        return expr_new_const(common_coeff);
    if (have_neg_sin && have_cos && sin_arg && cos_arg && expr_struct_eq(sin_arg, cos_arg))
        return expr_make_scaled(common_coeff, expr_double_arg_unary(cos_arg, expr_cos));
    if (have_sinh && have_cosh && sinh_arg && cosh_arg && expr_struct_eq(sinh_arg, cosh_arg))
        return expr_make_scaled(common_coeff, expr_double_arg_unary(cosh_arg, expr_cosh));
    if (have_neg_sinh && have_cosh && sinh_arg && cosh_arg && expr_struct_eq(sinh_arg, cosh_arg))
        return expr_new_const(common_coeff);

    return NULL;
}

static const expr_t *expr_find_trig_square_factor_local(const expr_t *expr, const expr_ops_t *op,
                                                        const expr_t *argument)
{
    const expr_t *found_argument = NULL;
    const expr_t *found;

    if (!expr)
        return NULL;
    if (expr_is_trig_square_of(expr, op, &found_argument) && (!argument || expr_struct_eq(found_argument, argument)))
        return expr;
    if (!expr_is_op(expr, &ops_mul))
        return NULL;
    found = expr_find_trig_square_factor_local(expr->a, op, argument);
    return found ? found : expr_find_trig_square_factor_local(expr->b, op, argument);
}

bool expr_combine_trig_pythagorean_addends(addend_t *terms, size_t n)
{
    bool combined = false;

    if (!terms)
        return false;

    for (size_t i = 0u; i < n; ++i) {
        const expr_t *sin_argument = NULL;
        const expr_t *sin_square;
        bool matched = false;

        if (!terms[i].base || num_is_zero(terms[i].coeff))
            continue;
        sin_square = expr_find_trig_square_factor_local(terms[i].base, &ops_sin, NULL);
        if (!sin_square || !expr_is_trig_square_of(sin_square, &ops_sin, &sin_argument))
            continue;

        for (size_t j = 0u; j < n; ++j) {
            const expr_t *cos_square;
            expr_t *sin_quotient;
            expr_t *cos_quotient;

            if (i == j || !terms[j].base || num_is_zero(terms[j].coeff) || !num_eq(terms[i].coeff, terms[j].coeff))
                continue;
            cos_square = expr_find_trig_square_factor_local(terms[j].base, &ops_cos, sin_argument);
            if (!cos_square)
                continue;

            sin_quotient = expr_simplify_extract_exact_factor_quotient(terms[i].base, sin_square);
            cos_quotient = expr_simplify_extract_exact_factor_quotient(terms[j].base, cos_square);
            if (sin_quotient && cos_quotient && expr_struct_eq(sin_quotient, cos_quotient)) {
                expr_free(terms[i].base);
                terms[i].base = sin_quotient;
                sin_quotient = NULL;
                num_destroy(&terms[j].coeff);
                terms[j].coeff = num_clone(NUM_ZERO);
                combined = true;
                matched = true;
            }
            expr_free(cos_quotient);
            expr_free(sin_quotient);
            if (matched)
                break;
        }
    }
    return combined;
}

static bool expr_match_trig_weighted_term_local(const expr_t *expr, const expr_ops_t **trig_op_out,
                                                const expr_t **trig_out, const expr_t **argument_out,
                                                expr_t **common_out, expr_t **remainder_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    const expr_t *trig = NULL;
    const expr_t *sum = NULL;
    const expr_t *first = NULL;
    const expr_t *second = NULL;
    bool is_subtraction = false;
    expr_t *common = NULL;
    expr_t *remainder = NULL;

    *trig_op_out = NULL;
    *trig_out = NULL;
    *argument_out = NULL;
    *common_out = NULL;
    *remainder_out = NULL;

    if (!expr_match_mul_expr(expr, &left, &right))
        return false;
    if (expr_is_op(left, &ops_sin) || expr_is_op(left, &ops_cos)) {
        trig = left;
        sum = right;
    } else if (expr_is_op(right, &ops_sin) || expr_is_op(right, &ops_cos)) {
        trig = right;
        sum = left;
    } else {
        return false;
    }
    if (!expr_match_add_sub_expr(sum, &first, &second, &is_subtraction))
        return false;

    common = expr_simplify_extract_common_factor_quotient(first, trig);
    if (common) {
        remainder = is_subtraction ? expr_negate_owned(expr_clone(second)) : expr_clone(second);
    } else if (!is_subtraction) {
        common = expr_simplify_extract_common_factor_quotient(second, trig);
        remainder = common ? expr_clone(first) : NULL;
    }
    if (!common || !remainder) {
        expr_free(remainder);
        expr_free(common);
        return false;
    }

    *trig_op_out = trig->ops;
    *trig_out = trig;
    *argument_out = trig->a;
    *common_out = common;
    *remainder_out = remainder;
    return true;
}

expr_t *expr_simplify_try_trig_weighted_sum(const expr_t *a, const expr_t *b)
{
    const expr_ops_t *first_op = NULL;
    const expr_ops_t *second_op = NULL;
    const expr_t *first_trig = NULL;
    const expr_t *second_trig = NULL;
    const expr_t *first_argument = NULL;
    const expr_t *second_argument = NULL;
    expr_t *first_common = NULL;
    expr_t *second_common = NULL;
    expr_t *first_remainder = NULL;
    expr_t *second_remainder = NULL;
    expr_t *first_tail = NULL;
    expr_t *second_tail = NULL;
    expr_t *out = NULL;

    if (!expr_match_trig_weighted_term_local(a, &first_op, &first_trig, &first_argument, &first_common,
                                             &first_remainder) ||
        !expr_match_trig_weighted_term_local(b, &second_op, &second_trig, &second_argument, &second_common,
                                             &second_remainder) ||
        first_op == second_op ||
        !((first_op == &ops_sin && second_op == &ops_cos) || (first_op == &ops_cos && second_op == &ops_sin)) ||
        !expr_struct_eq(first_argument, second_argument) || !expr_struct_eq(first_common, second_common))
        goto cleanup;

    first_tail = expr_mul_simplify_owned(expr_clone(first_trig), first_remainder);
    first_remainder = NULL;
    second_tail = expr_mul_simplify_owned(expr_clone(second_trig), second_remainder);
    second_remainder = NULL;
    out = (first_tail && second_tail) ? expr_add_simplify_owned(first_tail, second_tail) : NULL;
    first_tail = NULL;
    second_tail = NULL;
    out = out ? expr_add_simplify_owned(first_common, out) : NULL;
    first_common = NULL;

cleanup:
    expr_free(second_tail);
    expr_free(first_tail);
    expr_free(second_remainder);
    expr_free(first_remainder);
    expr_free(second_common);
    expr_free(first_common);
    return out;
}

static expr_t *expr_simplify_try_double_angle_product(expr_t *a, expr_t *b, const expr_ops_t *left_op,
                                                      const expr_ops_t *right_op, expr_apply_unary_fn builder)
{
    const expr_t *arg = NULL;
    expr_t *double_angle;

    if (expr_is_op(a, left_op) && expr_is_op(b, right_op) && expr_struct_eq(a->a, b->a)) {
        arg = a->a;
    } else if (expr_is_op(a, right_op) && expr_is_op(b, left_op) && expr_struct_eq(a->a, b->a)) {
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
    expr_t *double_angle = expr_simplify_try_double_angle_product(a, b, &ops_sin, &ops_cos, expr_sin);

    if (double_angle)
        return double_angle;

    double_angle = expr_simplify_try_double_angle_product(a, b, &ops_sinh, &ops_cosh, expr_sinh);
    if (double_angle)
        return double_angle;

    if (expr_is_op(a, &ops_cos) && expr_is_op(b, &ops_tan) && expr_struct_eq(a->a, b->a)) {
        arg = a->a;
        builder = expr_sin;
    } else if (expr_is_op(a, &ops_tan) && expr_is_op(b, &ops_cos) && expr_struct_eq(a->a, b->a)) {
        arg = a->a;
        builder = expr_sin;
    } else if (expr_is_op(a, &ops_cosh) && expr_is_op(b, &ops_tanh) && expr_struct_eq(a->a, b->a)) {
        arg = a->a;
        builder = expr_sinh;
    } else if (expr_is_op(a, &ops_tanh) && expr_is_op(b, &ops_cosh) && expr_struct_eq(a->a, b->a)) {
        arg = a->a;
        builder = expr_sinh;
    }

    if (!arg || !builder)
        return NULL;

    return builder(arg);
}

static bool expr_simplify_match_tangent_product(const expr_t *expr, const expr_t *first_argument,
                                                const expr_t *second_argument)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;

    if (!expr_match_mul_expr(expr, &left, &right) || !expr_is_op(left, &ops_tan) ||
        !expr_is_op(right, &ops_tan))
        return false;

    return (expr_struct_eq(left->a, first_argument) && expr_struct_eq(right->a, second_argument)) ||
           (expr_struct_eq(left->a, second_argument) && expr_struct_eq(right->a, first_argument));
}

expr_t *expr_simplify_try_tangent_addition_quotient(const expr_t *numerator, const expr_t *denominator)
{
    const expr_t *numerator_left = NULL;
    const expr_t *numerator_right = NULL;
    const expr_t *denominator_left = NULL;
    const expr_t *denominator_right = NULL;
    const expr_t *product = NULL;
    bool numerator_is_subtraction = false;
    bool denominator_is_subtraction = false;
    expr_t *raw_argument;
    expr_t *argument;
    expr_t *out;

    if (!expr_match_add_sub_expr(numerator, &numerator_left, &numerator_right, &numerator_is_subtraction) ||
        !expr_is_op(numerator_left, &ops_tan))
        return NULL;

    if (!numerator_is_subtraction && expr_is_neg(numerator_right)) {
        numerator_right = numerator_right->a;
        numerator_is_subtraction = true;
    }
    if (!expr_is_op(numerator_right, &ops_tan) ||
        !expr_match_add_sub_expr(denominator, &denominator_left, &denominator_right,
                                 &denominator_is_subtraction))
        return NULL;

    if (denominator_is_subtraction) {
        if (!expr_const_is_one(denominator_left))
            return NULL;
        product = denominator_right;
    } else if (expr_const_is_one(denominator_left)) {
        product = denominator_right;
    } else if (expr_const_is_one(denominator_right)) {
        product = denominator_left;
    } else {
        return NULL;
    }

    if (!denominator_is_subtraction && expr_is_neg(product)) {
        product = product->a;
        denominator_is_subtraction = true;
    }

    if (denominator_is_subtraction == numerator_is_subtraction)
        return NULL;

    if (!expr_simplify_match_tangent_product(product, numerator_left->a, numerator_right->a))
        return NULL;

    raw_argument = numerator_is_subtraction ? expr_sub(numerator_left->a, numerator_right->a)
                                            : expr_add(numerator_left->a, numerator_right->a);
    argument = raw_argument ? expr_simplify(raw_argument) : NULL;
    out = argument ? expr_tan(argument) : NULL;
    expr_free(argument);
    expr_free(raw_argument);
    return out;
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
    const expr_t *lambert_arg = expr_lambert_arg(arg);

    if (expr_is_const(lambert_arg) && num_is_real(lambert_arg->c) && num_eq(lambert_arg->c, NUM_ZERO)) {
        num_destroy(&inner_value);
        return expr_new_const(NUM_ONE);
    }
    if (expr_simplify_try_get_plain_real_const(lambert_arg, &inner_value) && num_eq(inner_value, NUM_ZERO)) {
        num_destroy(&inner_value);
        return expr_new_const(NUM_ONE);
    }
    num_destroy(&inner_value);

    return expr_div(lambert_arg, arg);
}

static bool expr_is_e_power_of_local(const expr_t *power, const expr_t *exponent)
{
    if (!power || !exponent)
        return false;
    if (expr_is_exp_expr(power))
        return expr_struct_eq(power->a, exponent);
    if (expr_is_pow_d_expr(power) && power->a && expr_is_const(power->a) && num_eq(power->a->c, NUM_E) &&
        expr_is_const(exponent))
        return num_eq(power->c, exponent->c);
    return expr_is_op(power, &ops_pow) && power->a && power->b && expr_is_const(power->a) &&
           num_eq(power->a->c, NUM_E) && expr_struct_eq(power->b, exponent);
}

static expr_t *expr_lambert_principal_argument_factor_local(const expr_t *argument)
{
    const expr_t *factor = NULL;

    if (!argument || !expr_is_op(argument, &ops_mul))
        return NULL;
    if (expr_is_e_power_of_local(argument->b, argument->a))
        factor = argument->a;
    else if (expr_is_e_power_of_local(argument->a, argument->b))
        factor = argument->b;
    if (!factor || !expr_is_const(factor) || !num_is_real(factor->c) || num_lt(factor->c, NUM_NEG_ONE))
        return NULL;

    expr_retain((expr_t *)factor);
    return (expr_t *)factor;
}

expr_t *expr_simplify_try_lambert_argument(expr_t *arg)
{
    expr_t *factor;
    expr_t *expanded;

    factor = expr_lambert_principal_argument_factor_local(arg);
    if (factor)
        return factor;
    if (!expr_is_unnamed_const(arg) || !arg->binding_expr || expr_binding_expr_is_array(arg->binding_expr))
        return NULL;

    expanded = expr_binding_expr_eval_expr(arg->binding_expr);
    factor = expr_lambert_principal_argument_factor_local(expanded);
    expr_free(expanded);
    return factor;
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

    inner = (expr_t *)expr_lambert_arg(w);

    if (!expr_current_wrt_internal() && expr_is_var(inner) && inner->binding_expr &&
        !expr_binding_expr_is_array(inner->binding_expr))
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

    if (expr_is_op(dv, &ops_neg) && expr_i_unit_sign(dv->a, &child_sign)) {
        *sign_out = -child_sign;
        return true;
    }

    return false;
}

static bool expr_extract_i_unit_factor(const expr_t *dv, int *sign_out, const expr_t **rest_out)
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

static expr_t *expr_extract_i_unit_factor_owned(const expr_t *expr, int *sign_out)
{
    expr_t *coefficient = NULL;
    expr_t *other = NULL;
    expr_t *combined = NULL;
    int child_sign;

    if (!expr || !sign_out)
        return NULL;
    if (expr_i_unit_sign(expr, sign_out))
        return expr_const_one();
    if (expr_is_op(expr, &ops_neg)) {
        coefficient = expr_extract_i_unit_factor_owned(expr->a, &child_sign);
        if (coefficient)
            *sign_out = -child_sign;
        return coefficient;
    }
    if (!expr_is_op(expr, &ops_mul))
        return NULL;

    coefficient = expr_extract_i_unit_factor_owned(expr->a, &child_sign);
    other = coefficient ? expr_clone(expr->b) : NULL;
    if (!coefficient) {
        coefficient = expr_extract_i_unit_factor_owned(expr->b, &child_sign);
        other = coefficient ? expr_clone(expr->a) : NULL;
    }
    if (!coefficient || !other) {
        expr_free(other);
        expr_free(coefficient);
        return NULL;
    }
    combined = expr_mul_simplify_owned(coefficient, other);
    *sign_out = child_sign;
    return combined;
}

static bool expr_contains_i_unit_factor(const expr_t *expr)
{
    int sign;

    if (!expr)
        return false;
    if (expr_i_unit_sign(expr, &sign))
        return true;
    return expr_contains_i_unit_factor(expr->a) || expr_contains_i_unit_factor(expr->b);
}

static bool expr_cartesian_sum_parts_owned(const expr_t *expr, const expr_t **real_out, expr_t **imaginary_out,
                                           int *imaginary_sign_out)
{
    const expr_t *left;
    const expr_t *right;
    expr_t *imaginary;
    int sign;
    bool subtract;

    if (!expr || !real_out || !imaginary_out || !imaginary_sign_out ||
        (!expr_is_op(expr, &ops_add) && !expr_is_op(expr, &ops_sub)))
        return false;
    left = expr->a;
    right = expr->b;
    subtract = expr_is_op(expr, &ops_sub);
    imaginary = expr_extract_i_unit_factor_owned(right, &sign);
    if (imaginary) {
        *real_out = left;
        *imaginary_out = imaginary;
        *imaginary_sign_out = subtract ? -sign : sign;
        return true;
    }
    if (!subtract) {
        imaginary = expr_extract_i_unit_factor_owned(left, &sign);
        if (imaginary) {
            *real_out = right;
            *imaginary_out = imaginary;
            *imaginary_sign_out = sign;
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

    if (!expr_extract_i_unit_factor(a, &a_sign, &a_rest) || !expr_extract_i_unit_factor(b, &b_sign, &b_rest))
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

static expr_t *expr_unary_product(expr_apply_unary_fn left_fn, const expr_t *left_arg, expr_apply_unary_fn right_fn,
                                  const expr_t *right_arg)
{
    expr_t *left = left_fn(left_arg);
    expr_t *right = right_fn(right_arg);

    if (!left || !right) {
        expr_free(left);
        expr_free(right);
        return NULL;
    }
    return expr_mul_simplify_owned(left, right);
}

static expr_t *expr_double_arg(const expr_t *arg)
{
    return expr_mul_simplify_owned(expr_const_long(2L), expr_clone(arg));
}

static expr_t *expr_cartesian_tangent_part(const expr_t *real, const expr_t *imaginary, bool hyperbolic,
                                           bool imaginary_part)
{
    expr_t *double_real = expr_double_arg(real);
    expr_t *double_imaginary = expr_double_arg(imaginary);
    expr_t *numerator;
    expr_t *denominator_left;
    expr_t *denominator_right;
    expr_t *denominator;

    if (!double_real || !double_imaginary) {
        expr_free(double_real);
        expr_free(double_imaginary);
        return NULL;
    }

    if (hyperbolic) {
        numerator = imaginary_part ? expr_sin(double_imaginary) : expr_sinh(double_real);
        denominator_left = expr_cosh(double_real);
        denominator_right = expr_cos(double_imaginary);
    } else {
        numerator = imaginary_part ? expr_sinh(double_imaginary) : expr_sin(double_real);
        denominator_left = expr_cos(double_real);
        denominator_right = expr_cosh(double_imaginary);
    }
    expr_free(double_real);
    expr_free(double_imaginary);

    if (!denominator_left || !denominator_right) {
        expr_free(numerator);
        expr_free(denominator_left);
        expr_free(denominator_right);
        return NULL;
    }
    denominator = expr_add_simplify_owned(denominator_left, denominator_right);
    if (!numerator || !denominator) {
        expr_free(numerator);
        expr_free(denominator);
        return NULL;
    }
    return expr_div_simplify_owned(numerator, denominator);
}

static expr_t *expr_twice_unary_product(expr_apply_unary_fn left_fn, const expr_t *left_arg,
                                        expr_apply_unary_fn right_fn, const expr_t *right_arg)
{
    expr_t *product = expr_unary_product(left_fn, left_arg, right_fn, right_arg);

    return product ? expr_mul_simplify_owned(expr_const_long(2L), product) : NULL;
}

static bool expr_cartesian_reciprocal_unary_parts(const expr_t *op, const expr_t *real, const expr_t *imaginary,
                                                  expr_t **real_out, expr_t **imaginary_out,
                                                  int *imaginary_sign_out)
{
    expr_t *double_real = NULL;
    expr_t *double_imaginary = NULL;
    expr_t *denominator_left = NULL;
    expr_t *denominator_right = NULL;
    expr_t *denominator = NULL;
    expr_t *real_numerator = NULL;
    expr_t *imaginary_numerator = NULL;
    expr_t *real_part = NULL;
    expr_t *imaginary_part = NULL;
    bool secant = expr_is_op(op, &ops_sec);
    bool cosecant = expr_is_op(op, &ops_cosec);
    bool cotangent = expr_is_op(op, &ops_cot);
    bool hyperbolic_secant = expr_is_op(op, &ops_sech);
    bool hyperbolic_cosecant = expr_is_op(op, &ops_cosech);
    bool hyperbolic_cotangent = expr_is_op(op, &ops_coth);
    bool ok = false;

    if ((!secant && !cosecant && !cotangent && !hyperbolic_secant && !hyperbolic_cosecant &&
         !hyperbolic_cotangent) ||
        !real || !imaginary || !real_out || !imaginary_out || !imaginary_sign_out)
        goto cleanup;

    double_real = expr_double_arg(real);
    double_imaginary = expr_double_arg(imaginary);
    if (!double_real || !double_imaginary)
        goto cleanup;

    if (secant) {
        denominator_left = expr_cos(double_real);
        denominator_right = expr_cosh(double_imaginary);
        real_numerator = expr_twice_unary_product(expr_cos, real, expr_cosh, imaginary);
        imaginary_numerator = expr_twice_unary_product(expr_sin, real, expr_sinh, imaginary);
    } else if (cosecant || cotangent) {
        denominator_left = expr_cosh(double_imaginary);
        denominator_right = expr_cos(double_real);
        real_numerator = cotangent ? expr_sin(double_real)
                                   : expr_twice_unary_product(expr_sin, real, expr_cosh, imaginary);
        imaginary_numerator = cotangent ? expr_sinh(double_imaginary)
                                        : expr_twice_unary_product(expr_cos, real, expr_sinh, imaginary);
        *imaginary_sign_out = -*imaginary_sign_out;
    } else if (hyperbolic_secant) {
        denominator_left = expr_cosh(double_real);
        denominator_right = expr_cos(double_imaginary);
        real_numerator = expr_twice_unary_product(expr_cosh, real, expr_cos, imaginary);
        imaginary_numerator = expr_twice_unary_product(expr_sinh, real, expr_sin, imaginary);
        *imaginary_sign_out = -*imaginary_sign_out;
    } else {
        denominator_left = expr_cosh(double_real);
        denominator_right = expr_cos(double_imaginary);
        real_numerator = hyperbolic_cotangent
                             ? expr_sinh(double_real)
                             : expr_twice_unary_product(expr_sinh, real, expr_cos, imaginary);
        imaginary_numerator = hyperbolic_cotangent
                                  ? expr_sin(double_imaginary)
                                  : expr_twice_unary_product(expr_cosh, real, expr_sin, imaginary);
        *imaginary_sign_out = -*imaginary_sign_out;
    }
    denominator = (cosecant || cotangent || hyperbolic_cosecant || hyperbolic_cotangent)
                      ? expr_sub_simplify_owned(denominator_left, denominator_right)
                      : expr_add_simplify_owned(denominator_left, denominator_right);
    denominator_left = NULL;
    denominator_right = NULL;
    real_part = real_numerator && denominator
                    ? expr_div_simplify_owned(real_numerator, expr_clone(denominator))
                    : NULL;
    if (real_numerator && denominator)
        real_numerator = NULL;
    imaginary_part = imaginary_numerator && denominator
                         ? expr_div_simplify_owned(imaginary_numerator, expr_clone(denominator))
                         : NULL;
    if (imaginary_numerator && denominator)
        imaginary_numerator = NULL;
    if (!real_part || !imaginary_part)
        goto cleanup;
    *real_out = real_part;
    *imaginary_out = imaginary_part;
    real_part = NULL;
    imaginary_part = NULL;
    ok = true;

cleanup:
    expr_free(imaginary_part);
    expr_free(real_part);
    expr_free(imaginary_numerator);
    expr_free(real_numerator);
    expr_free(denominator);
    expr_free(denominator_right);
    expr_free(denominator_left);
    expr_free(double_imaginary);
    expr_free(double_real);
    return ok;
}

static expr_t *expr_cartesian_shifted_radius(const expr_t *real, long real_shift, const expr_t *imaginary,
                                             long imaginary_shift)
{
    expr_t *shifted_real = real_shift ? expr_add_long(real, real_shift) : expr_clone(real);
    expr_t *shifted_imaginary = imaginary_shift ? expr_add_long(imaginary, imaginary_shift) : expr_clone(imaginary);
    expr_t *real_squared = shifted_real ? expr_pow(shifted_real, &NUM_TWO) : NULL;
    expr_t *imaginary_squared = shifted_imaginary ? expr_pow(shifted_imaginary, &NUM_TWO) : NULL;
    expr_t *radius_squared = NULL;
    expr_t *radius = NULL;

    if (real_squared && imaginary_squared) {
        radius_squared = expr_add_simplify_owned(real_squared, imaginary_squared);
        real_squared = NULL;
        imaginary_squared = NULL;
    }
    radius = radius_squared ? expr_sqrt(radius_squared) : NULL;
    expr_free(radius_squared);
    expr_free(imaginary_squared);
    expr_free(real_squared);
    expr_free(shifted_imaginary);
    expr_free(shifted_real);
    return radius;
}

static bool expr_cartesian_inverse_elliptic_parts(expr_op_kind_t kind, const expr_t *real, const expr_t *imaginary,
                                                  const expr_t *imaginary_orientation_source, expr_t **real_out,
                                                  expr_t **imaginary_out, int *imaginary_sign_out)
{
    expr_t *upper_radius = NULL;
    expr_t *lower_radius = NULL;
    expr_t *radius_difference = NULL;
    expr_t *radius_sum = NULL;
    expr_t *projection = NULL;
    expr_t *height = NULL;
    expr_t *real_part = NULL;
    expr_t *imaginary_part = NULL;
    expr_t *imaginary_absolute = NULL;
    expr_t *imaginary_orientation = NULL;
    expr_t *oriented_imaginary_part = NULL;
    bool inverse_sine = kind == EXPR_KIND_ASIN;
    bool inverse_cosine = kind == EXPR_KIND_ACOS;
    bool inverse_hyperbolic_cosine = kind == EXPR_KIND_ACOSH;
    bool ok = false;

    if ((!inverse_sine && !inverse_cosine && !inverse_hyperbolic_cosine) || !real || !imaginary || !real_out ||
        !imaginary_orientation_source || !imaginary_out || !imaginary_sign_out)
        goto cleanup;
    upper_radius = expr_cartesian_shifted_radius(real, 1L, imaginary, 0L);
    lower_radius = expr_cartesian_shifted_radius(real, -1L, imaginary, 0L);
    radius_difference = upper_radius && lower_radius ? expr_sub(upper_radius, lower_radius) : NULL;
    radius_sum = upper_radius && lower_radius ? expr_add(upper_radius, lower_radius) : NULL;
    projection = radius_difference ? expr_div_long(radius_difference, 2L) : NULL;
    height = radius_sum ? expr_div_long(radius_sum, 2L) : NULL;
    if (inverse_sine) {
        real_part = projection ? expr_asin(projection) : NULL;
        imaginary_part = height ? expr_acosh(height) : NULL;
    } else if (inverse_cosine) {
        real_part = projection ? expr_acos(projection) : NULL;
        imaginary_part = height ? expr_acosh(height) : NULL;
        *imaginary_sign_out = -*imaginary_sign_out;
    } else {
        real_part = height ? expr_acosh(height) : NULL;
        imaginary_part = projection ? expr_acos(projection) : NULL;
    }
    imaginary_absolute = expr_abs(imaginary_orientation_source);
    imaginary_orientation = imaginary_absolute ? expr_div(imaginary_orientation_source, imaginary_absolute) : NULL;
    oriented_imaginary_part = imaginary_part && imaginary_orientation
                                  ? expr_mul(imaginary_orientation, imaginary_part)
                                  : NULL;
    expr_free(imaginary_part);
    imaginary_part = oriented_imaginary_part;
    oriented_imaginary_part = NULL;
    if (!real_part || !imaginary_part)
        goto cleanup;
    *real_out = real_part;
    *imaginary_out = imaginary_part;
    real_part = NULL;
    imaginary_part = NULL;
    ok = true;

cleanup:
    expr_free(oriented_imaginary_part);
    expr_free(imaginary_orientation);
    expr_free(imaginary_absolute);
    expr_free(imaginary_part);
    expr_free(real_part);
    expr_free(height);
    expr_free(projection);
    expr_free(radius_sum);
    expr_free(radius_difference);
    expr_free(lower_radius);
    expr_free(upper_radius);
    return ok;
}

static expr_t *expr_cartesian_square(const expr_t *expr)
{
    if (!expr)
        return NULL;
    if (expr->ops && expr->ops->kind == EXPR_KIND_NEG)
        return expr_cartesian_square(expr->a);
    if (expr->ops && expr->ops->kind == EXPR_KIND_MUL) {
        expr_t *left = expr_cartesian_square(expr->a);
        expr_t *right = expr_cartesian_square(expr->b);

        if (left && right)
            return expr_mul_simplify_owned(left, right);
        expr_free(right);
        expr_free(left);
        return NULL;
    }
    if (expr->ops && expr->ops->kind == EXPR_KIND_DIV) {
        expr_t *numerator = expr_cartesian_square(expr->a);
        expr_t *denominator = expr_cartesian_square(expr->b);

        if (numerator && denominator)
            return expr_div_simplify_owned(numerator, denominator);
        expr_free(denominator);
        expr_free(numerator);
        return NULL;
    }
    return expr_pow(expr, &NUM_TWO);
}

static bool expr_cartesian_inverse_hyperbolic_sine_parts(const expr_t *real, const expr_t *imaginary,
                                                         const expr_t *real_orientation_source, expr_t **real_out,
                                                         expr_t **imaginary_out)
{
    expr_t *upper_radius = NULL;
    expr_t *lower_radius = NULL;
    expr_t *radius_sum = NULL;
    expr_t *radius_difference = NULL;
    expr_t *height = NULL;
    expr_t *projection = NULL;
    expr_t *real_magnitude = NULL;
    expr_t *real_absolute = NULL;
    expr_t *real_orientation = NULL;
    expr_t *real_part = NULL;
    expr_t *imaginary_part = NULL;
    bool ok = false;

    if (!real || !imaginary || !real_orientation_source || !real_out || !imaginary_out)
        goto cleanup;
    upper_radius = expr_cartesian_shifted_radius(real, 0L, imaginary, 1L);
    lower_radius = expr_cartesian_shifted_radius(real, 0L, imaginary, -1L);
    radius_sum = upper_radius && lower_radius ? expr_add(upper_radius, lower_radius) : NULL;
    radius_difference = upper_radius && lower_radius ? expr_sub(upper_radius, lower_radius) : NULL;
    height = radius_sum ? expr_div_long(radius_sum, 2L) : NULL;
    projection = radius_difference ? expr_div_long(radius_difference, 2L) : NULL;
    real_magnitude = height ? expr_acosh(height) : NULL;
    real_absolute = expr_abs(real_orientation_source);
    real_orientation = real_absolute ? expr_div(real_orientation_source, real_absolute) : NULL;
    real_part = real_magnitude && real_orientation ? expr_mul(real_orientation, real_magnitude) : NULL;
    imaginary_part = projection ? expr_asin(projection) : NULL;
    if (!real_part || !imaginary_part)
        goto cleanup;
    *real_out = real_part;
    *imaginary_out = imaginary_part;
    real_part = NULL;
    imaginary_part = NULL;
    ok = true;

cleanup:
    expr_free(imaginary_part);
    expr_free(real_part);
    expr_free(real_orientation);
    expr_free(real_absolute);
    expr_free(real_magnitude);
    expr_free(projection);
    expr_free(height);
    expr_free(radius_difference);
    expr_free(radius_sum);
    expr_free(lower_radius);
    expr_free(upper_radius);
    return ok;
}

static bool expr_cartesian_inverse_hyperbolic_tangent_parts(const expr_t *real, const expr_t *imaginary,
                                                            expr_t **real_out, expr_t **imaginary_out)
{
    expr_t *real_plus_one = NULL;
    expr_t *real_minus_one = NULL;
    expr_t *upper_real_square = NULL;
    expr_t *lower_real_square = NULL;
    expr_t *upper_imaginary_square = NULL;
    expr_t *lower_imaginary_square = NULL;
    expr_t *upper = NULL;
    expr_t *lower = NULL;
    expr_t *ratio = NULL;
    expr_t *ratio_log = NULL;
    expr_t *two_imaginary = NULL;
    expr_t *real_square = NULL;
    expr_t *imaginary_square = NULL;
    expr_t *angle_denominator = NULL;
    expr_t *angle = NULL;
    expr_t *real_part = NULL;
    expr_t *imaginary_part = NULL;
    bool ok = false;

    if (!real || !imaginary || !real_out || !imaginary_out)
        goto cleanup;
    real_plus_one = expr_add_long(real, 1L);
    real_minus_one = expr_add_long(real, -1L);
    upper_real_square = expr_cartesian_square(real_plus_one);
    lower_real_square = expr_cartesian_square(real_minus_one);
    upper_imaginary_square = expr_cartesian_square(imaginary);
    lower_imaginary_square = expr_cartesian_square(imaginary);
    upper = upper_real_square && upper_imaginary_square
                ? expr_add_simplify_owned(upper_real_square, upper_imaginary_square)
                : NULL;
    if (upper_real_square && upper_imaginary_square) {
        upper_real_square = NULL;
        upper_imaginary_square = NULL;
    }
    lower = lower_real_square && lower_imaginary_square
                ? expr_add_simplify_owned(lower_real_square, lower_imaginary_square)
                : NULL;
    if (lower_real_square && lower_imaginary_square) {
        lower_real_square = NULL;
        lower_imaginary_square = NULL;
    }
    ratio = upper && lower ? expr_div(upper, lower) : NULL;
    ratio_log = ratio ? expr_log(ratio) : NULL;
    real_part = ratio_log ? expr_mul_simplify_owned(expr_new_const(NUM_QUARTER), ratio_log) : NULL;
    if (ratio_log)
        ratio_log = NULL;

    two_imaginary = expr_double_arg(imaginary);
    real_square = expr_cartesian_square(real);
    imaginary_square = expr_cartesian_square(imaginary);
    if (real_square && imaginary_square) {
        expr_t *real_difference = expr_sub_simplify_owned(expr_const_one(), real_square);

        real_square = NULL;
        if (real_difference) {
            angle_denominator = expr_sub_simplify_owned(real_difference, imaginary_square);
            imaginary_square = NULL;
        }
    }
    angle = two_imaginary && angle_denominator ? expr_atan2(two_imaginary, angle_denominator) : NULL;
    imaginary_part = angle ? expr_mul_simplify_owned(expr_new_const(NUM_HALF), angle) : NULL;
    if (angle)
        angle = NULL;
    if (!real_part || !imaginary_part)
        goto cleanup;
    *real_out = real_part;
    *imaginary_out = imaginary_part;
    real_part = NULL;
    imaginary_part = NULL;
    ok = true;

cleanup:
    expr_free(imaginary_part);
    expr_free(real_part);
    expr_free(angle);
    expr_free(angle_denominator);
    expr_free(imaginary_square);
    expr_free(real_square);
    expr_free(two_imaginary);
    expr_free(ratio_log);
    expr_free(ratio);
    expr_free(lower);
    expr_free(upper);
    expr_free(lower_imaginary_square);
    expr_free(upper_imaginary_square);
    expr_free(lower_real_square);
    expr_free(upper_real_square);
    expr_free(real_minus_one);
    expr_free(real_plus_one);
    return ok;
}

static bool expr_cartesian_reciprocal_argument(const expr_t *real, const expr_t *imaginary, expr_t **real_out,
                                               expr_t **imaginary_out)
{
    expr_t *real_squared = expr_cartesian_square(real);
    expr_t *imaginary_squared = expr_cartesian_square(imaginary);
    expr_t *norm = NULL;
    expr_t *reciprocal_real = NULL;
    expr_t *reciprocal_imaginary = NULL;
    bool ok = false;

    if (real_squared && imaginary_squared) {
        norm = expr_add_simplify_owned(real_squared, imaginary_squared);
        real_squared = NULL;
        imaginary_squared = NULL;
    }
    reciprocal_real = norm ? expr_div(real, norm) : NULL;
    reciprocal_imaginary = norm ? expr_div(imaginary, norm) : NULL;
    if (!reciprocal_real || !reciprocal_imaginary)
        goto cleanup;
    *real_out = reciprocal_real;
    *imaginary_out = reciprocal_imaginary;
    reciprocal_real = NULL;
    reciprocal_imaginary = NULL;
    ok = true;

cleanup:
    expr_free(reciprocal_imaginary);
    expr_free(reciprocal_real);
    expr_free(norm);
    expr_free(imaginary_squared);
    expr_free(real_squared);
    return ok;
}

expr_t *expr_simplify_try_complex_unary_cartesian(const expr_t *op, expr_t *arg_node)
{
    const expr_t *left;
    const expr_t *right;
    const expr_t *real;
    const expr_t *imaginary = NULL;
    const expr_t *real_orientation_source = NULL;
    const expr_t *imaginary_orientation_source = NULL;
    expr_t *owned_real = NULL;
    expr_t *owned_imaginary = NULL;
    expr_t *owned_reciprocal_real = NULL;
    expr_t *owned_reciprocal_imaginary = NULL;
    expr_t *real_part = NULL;
    expr_t *imaginary_coefficient = NULL;
    expr_t *imaginary_unit;
    expr_t *imaginary_part;
    expr_t *out;
    expr_t *simplified;
    int imaginary_sign;
    bool subtract;
    bool reciprocal = expr_is_op(op, &ops_sec) || expr_is_op(op, &ops_cosec) || expr_is_op(op, &ops_cot) ||
                      expr_is_op(op, &ops_sech) || expr_is_op(op, &ops_cosech) || expr_is_op(op, &ops_coth);
    bool cosine = expr_is_op(op, &ops_cos) || expr_is_op(op, &ops_sec);
    bool exponential = expr_is_op(op, &ops_exp);
    bool hyperbolic_cosine = expr_is_op(op, &ops_cosh) || expr_is_op(op, &ops_sech);
    bool hyperbolic_sine = expr_is_op(op, &ops_sinh) || expr_is_op(op, &ops_cosech);
    bool hyperbolic_tangent = expr_is_op(op, &ops_tanh) || expr_is_op(op, &ops_coth);
    bool inverse_sine = expr_is_op(op, &ops_asin) || expr_is_op(op, &ops_acosec);
    bool inverse_cosine = expr_is_op(op, &ops_acos) || expr_is_op(op, &ops_asec);
    bool inverse_tangent = expr_is_op(op, &ops_atan) || expr_is_op(op, &ops_acot);
    bool inverse_cotangent = expr_is_op(op, &ops_acot);
    bool inverse_hyperbolic_sine = expr_is_op(op, &ops_asinh) || expr_is_op(op, &ops_acosech);
    bool inverse_hyperbolic_cosine = expr_is_op(op, &ops_acosh) || expr_is_op(op, &ops_asech);
    bool inverse_hyperbolic_tangent = expr_is_op(op, &ops_atanh) || expr_is_op(op, &ops_acoth);
    bool reciprocal_inverse = expr_is_op(op, &ops_asec) || expr_is_op(op, &ops_acosec) ||
                              expr_is_op(op, &ops_asech) ||
                              expr_is_op(op, &ops_acosech) || expr_is_op(op, &ops_acoth);
    expr_op_kind_t inverse_kind = inverse_sine        ? EXPR_KIND_ASIN
                                  : inverse_cosine    ? EXPR_KIND_ACOS
                                  : inverse_tangent   ? EXPR_KIND_ATAN
                                  : inverse_hyperbolic_sine
                                      ? EXPR_KIND_ASINH
                                  : inverse_hyperbolic_cosine ? EXPR_KIND_ACOSH
                                                               : EXPR_KIND_ATANH;
    bool logarithm = expr_is_op(op, &ops_log);
    bool common_logarithm = expr_is_op(op, &ops_log10);
    bool sine = expr_is_op(op, &ops_sin) || expr_is_op(op, &ops_cosec);
    bool tangent = expr_is_op(op, &ops_tan) || expr_is_op(op, &ops_cot);
    bool scaled_cartesian = false;
    bool parsed_has_imaginary = false;
    bool legacy_direct_cartesian = false;

    if ((!sine && !cosine && !tangent && !hyperbolic_sine && !hyperbolic_cosine && !hyperbolic_tangent &&
         !exponential && !logarithm && !common_logarithm && !inverse_sine && !inverse_cosine && !inverse_tangent &&
         !inverse_hyperbolic_sine && !inverse_hyperbolic_cosine && !inverse_hyperbolic_tangent) ||
        !arg_node)
        return NULL;

    if (expr_is_op(arg_node, &ops_add) || expr_is_op(arg_node, &ops_sub)) {
        const expr_t *probe_real = NULL;
        expr_t *probe_imaginary = NULL;
        int probe_sign = 1;

        legacy_direct_cartesian =
            expr_cartesian_sum_parts_owned(arg_node, &probe_real, &probe_imaginary, &probe_sign);
        expr_free(probe_imaginary);
    }

    if (!legacy_direct_cartesian &&
        expr_cartesian_parts_for_display(arg_node, &owned_real, &owned_imaginary, &parsed_has_imaginary) &&
        parsed_has_imaginary) {
        real = owned_real;
        imaginary = owned_imaginary;
        imaginary_sign = 1;
        scaled_cartesian = true;
    } else {
        expr_free(owned_imaginary);
        expr_free(owned_real);
        owned_imaginary = NULL;
        owned_real = NULL;
    }

    if (!scaled_cartesian && expr_is_op(arg_node, &ops_mul)) {
        const expr_t *cartesian_real = NULL;
        const expr_t *scale = NULL;
        expr_t *cartesian_imaginary = NULL;

        if (expr_cartesian_sum_parts_owned(arg_node->a, &cartesian_real, &cartesian_imaginary, &imaginary_sign) &&
            !expr_contains_i_unit_factor(arg_node->b)) {
            scale = arg_node->b;
        } else {
            expr_free(cartesian_imaginary);
            cartesian_imaginary = NULL;
            if (expr_cartesian_sum_parts_owned(arg_node->b, &cartesian_real, &cartesian_imaginary, &imaginary_sign) &&
                !expr_contains_i_unit_factor(arg_node->a))
                scale = arg_node->a;
        }
        owned_real = scale ? expr_mul_simplify_owned(expr_clone(scale), expr_clone(cartesian_real)) : NULL;
        owned_imaginary = scale ? expr_mul_simplify_owned(expr_clone(scale), cartesian_imaginary) : NULL;
        if (scale)
            cartesian_imaginary = NULL;
        expr_free(cartesian_imaginary);
        scaled_cartesian = owned_real && owned_imaginary;
        if (scaled_cartesian)
            real = owned_real;
    } else if (!scaled_cartesian && expr_is_op(arg_node, &ops_div) && !expr_contains_i_unit_factor(arg_node->b)) {
        const expr_t *cartesian_real = NULL;
        expr_t *cartesian_imaginary = NULL;

        if (expr_cartesian_sum_parts_owned(arg_node->a, &cartesian_real, &cartesian_imaginary, &imaginary_sign)) {
            owned_real = expr_div_simplify_owned(expr_clone(cartesian_real), expr_clone(arg_node->b));
            owned_imaginary = expr_div_simplify_owned(cartesian_imaginary, expr_clone(arg_node->b));
            cartesian_imaginary = NULL;
            scaled_cartesian = owned_real && owned_imaginary;
            if (scaled_cartesian)
                real = owned_real;
        }
        expr_free(cartesian_imaginary);
    }

    if (scaled_cartesian) {
        imaginary = owned_imaginary;
    } else if (expr_is_op(arg_node, &ops_add) || expr_is_op(arg_node, &ops_sub)) {
        left = arg_node->a;
        right = arg_node->b;
        subtract = expr_is_op(arg_node, &ops_sub);

        owned_imaginary = expr_extract_i_unit_factor_owned(right, &imaginary_sign);
        if (owned_imaginary) {
            real = left;
            imaginary_sign = subtract ? -imaginary_sign : imaginary_sign;
        } else if (!subtract && (owned_imaginary = expr_extract_i_unit_factor_owned(left, &imaginary_sign))) {
            real = right;
        } else {
            return NULL;
        }
    } else {
        owned_imaginary = expr_extract_i_unit_factor_owned(arg_node, &imaginary_sign);
        owned_real = owned_imaginary ? expr_const_zero() : NULL;
        if (!owned_imaginary || !owned_real) {
            expr_free(owned_imaginary);
            expr_free(owned_real);
            return NULL;
        }
        real = owned_real;
    }

    imaginary = owned_imaginary;
    real_orientation_source = real;
    imaginary_orientation_source = imaginary;

    if (reciprocal_inverse) {
        if (!expr_cartesian_reciprocal_argument(real, imaginary, &owned_reciprocal_real,
                                                &owned_reciprocal_imaginary))
            goto cleanup;
        real = owned_reciprocal_real;
        imaginary = owned_reciprocal_imaginary;
        imaginary_sign = -imaginary_sign;
    }

    if (reciprocal) {
        if (!expr_cartesian_reciprocal_unary_parts(op, real, imaginary, &real_part, &imaginary_coefficient,
                                                   &imaginary_sign)) {
            real_part = NULL;
            imaginary_coefficient = NULL;
        }
    } else if (inverse_sine || inverse_cosine || inverse_hyperbolic_cosine) {
        if (!expr_cartesian_inverse_elliptic_parts(inverse_kind, real, imaginary, imaginary_orientation_source,
                                                   &real_part, &imaginary_coefficient, &imaginary_sign)) {
            real_part = NULL;
            imaginary_coefficient = NULL;
        }
    } else if (inverse_hyperbolic_sine) {
        if (!expr_cartesian_inverse_hyperbolic_sine_parts(real, imaginary, real_orientation_source, &real_part,
                                                          &imaginary_coefficient)) {
            real_part = NULL;
            imaginary_coefficient = NULL;
        }
    } else if (inverse_hyperbolic_tangent) {
        if (!expr_cartesian_inverse_hyperbolic_tangent_parts(real, imaginary, &real_part,
                                                             &imaginary_coefficient)) {
            real_part = NULL;
            imaginary_coefficient = NULL;
        }
    } else if (inverse_tangent) {
        expr_t *real_squared = expr_cartesian_square(real);
        expr_t *imaginary_squared = expr_cartesian_square(imaginary);
        expr_t *two_real = expr_mul_simplify_owned(expr_const_long(2L), expr_clone(real));
        expr_t *real_denominator = real_squared && imaginary_squared
                                           ? expr_sub_simplify_owned(
                                                 expr_sub_simplify_owned(expr_const_one(), real_squared),
                                                 imaginary_squared)
                                           : NULL;
        expr_t *real_angle = two_real && real_denominator ? expr_atan2(two_real, real_denominator) : NULL;
        expr_t *imaginary_plus_one = expr_add_simplify_owned(expr_clone(imaginary), expr_const_one());
        expr_t *imaginary_minus_one = expr_sub_simplify_owned(expr_clone(imaginary), expr_const_one());
        expr_t *upper = expr_cartesian_square(imaginary_plus_one);
        expr_t *lower = expr_cartesian_square(imaginary_minus_one);
        expr_t *real_square_for_upper = expr_cartesian_square(real);
        expr_t *real_square_for_lower = expr_cartesian_square(real);
        expr_t *ratio;
        expr_t *ratio_log;

        expr_free(imaginary_minus_one);
        expr_free(imaginary_plus_one);
        expr_free(real_denominator);
        expr_free(two_real);
        upper = upper && real_square_for_upper ? expr_add_simplify_owned(real_square_for_upper, upper) : NULL;
        lower = lower && real_square_for_lower ? expr_add_simplify_owned(real_square_for_lower, lower) : NULL;
        ratio = upper && lower ? expr_div_simplify_owned(upper, lower) : NULL;
        ratio_log = ratio ? expr_log(ratio) : NULL;
        expr_free(ratio);
        real_part = real_angle ? expr_mul_simplify_owned(expr_new_const(NUM_HALF), real_angle) : NULL;
        imaginary_coefficient = ratio_log ? expr_mul_simplify_owned(expr_new_const(NUM_QUARTER), ratio_log) : NULL;
        if (inverse_cotangent && real_part) {
            real_part = expr_sub_simplify_owned(expr_new_pi_ratio_long(1L, 2u), real_part);
            imaginary_sign = -imaginary_sign;
        }
    } else if (logarithm || common_logarithm) {
        expr_t *real_squared = expr_cartesian_square(real);
        expr_t *imaginary_squared = expr_cartesian_square(imaginary);
        expr_t *modulus_squared =
            real_squared && imaginary_squared ? expr_add_simplify_owned(real_squared, imaginary_squared) : NULL;
        expr_t *modulus_log = modulus_squared ? expr_log(modulus_squared) : NULL;
        expr_t *half = expr_new_const(NUM_HALF);

        expr_free(modulus_squared);
        real_part = modulus_log && half ? expr_mul_simplify_owned(half, modulus_log) : NULL;
        if (!real_part) {
            expr_free(half);
            expr_free(modulus_log);
        }
        imaginary_coefficient = expr_atan2(imaginary, real);
        if (common_logarithm) {
            expr_t *ten = expr_const_long(10L);
            expr_t *log_ten = ten ? expr_log(ten) : NULL;

            expr_free(ten);
            if (log_ten) {
                real_part = expr_div_simplify_owned(real_part, expr_clone(log_ten));
                imaginary_coefficient = expr_div_simplify_owned(imaginary_coefficient, log_ten);
            } else {
                expr_free(real_part);
                expr_free(imaginary_coefficient);
                real_part = NULL;
                imaginary_coefficient = NULL;
            }
        }
    } else if (sine) {
        real_part = expr_unary_product(expr_sin, real, expr_cosh, imaginary);
        imaginary_coefficient = expr_unary_product(expr_cos, real, expr_sinh, imaginary);
    } else if (cosine) {
        real_part = expr_unary_product(expr_cos, real, expr_cosh, imaginary);
        imaginary_coefficient = expr_unary_product(expr_sin, real, expr_sinh, imaginary);
        imaginary_sign = -imaginary_sign;
    } else if (hyperbolic_sine) {
        real_part = expr_unary_product(expr_sinh, real, expr_cos, imaginary);
        imaginary_coefficient = expr_unary_product(expr_cosh, real, expr_sin, imaginary);
    } else if (hyperbolic_cosine) {
        real_part = expr_unary_product(expr_cosh, real, expr_cos, imaginary);
        imaginary_coefficient = expr_unary_product(expr_sinh, real, expr_sin, imaginary);
    } else if (exponential) {
        real_part = expr_unary_product(expr_exp, real, expr_cos, imaginary);
        imaginary_coefficient = expr_unary_product(expr_exp, real, expr_sin, imaginary);
    } else {
        real_part = expr_cartesian_tangent_part(real, imaginary, hyperbolic_tangent, false);
        imaginary_coefficient = expr_cartesian_tangent_part(real, imaginary, hyperbolic_tangent, true);
    }
    imaginary_unit = expr_new_named_const(NUM_I, "i");
    imaginary_part = imaginary_unit && imaginary_coefficient ? expr_mul(imaginary_unit, imaginary_coefficient) : NULL;
    out = real_part && imaginary_part
              ? expr_new_binary_internal(imaginary_sign < 0 ? &ops_sub : &ops_add, real_part, imaginary_part)
              : NULL;
    if (out) {
        real_part = NULL;
        imaginary_part = NULL;
    }
    simplified = out;

    expr_free(imaginary_part);
    expr_free(imaginary_unit);
    expr_free(imaginary_coefficient);
    expr_free(real_part);
    expr_free(owned_reciprocal_imaginary);
    expr_free(owned_reciprocal_real);
    expr_free(owned_imaginary);
    expr_free(owned_real);
    if (simplified)
        expr_free(arg_node);
    return simplified;

cleanup:
    expr_free(owned_reciprocal_imaginary);
    expr_free(owned_reciprocal_real);
    expr_free(owned_imaginary);
    expr_free(owned_real);
    return NULL;
}

static expr_t *expr_complex_unary_cartesian_for_display_recursive(const expr_t *expr, bool *changed)
{
    expr_t *left = NULL;
    expr_t *right = NULL;
    expr_t *rebuilt = NULL;
    expr_t *argument = NULL;
    expr_t *expanded = NULL;

    if (!expr)
        return NULL;
    if (!expr->ops || expr->ops->arity == EXPR_OP_ATOM)
        return expr_clone(expr);

    left = expr_complex_unary_cartesian_for_display_recursive(expr->a, changed);
    if (!left)
        goto cleanup;
    if (expr_is_pow_d_expr(expr)) {
        rebuilt = expr_new_pow_const_internal(left, expr->c);
    } else if (expr->ops->arity == EXPR_OP_BINARY) {
        right = expr_complex_unary_cartesian_for_display_recursive(expr->b, changed);
        if (!right)
            goto cleanup;
        rebuilt = expr_new_binary_internal(expr->ops, left, right);
    } else {
        rebuilt = expr_new_unary_internal(expr->ops, left);
    }
    if (rebuilt) {
        left = NULL;
        right = NULL;
    }
    if (rebuilt && rebuilt->ops->arity == EXPR_OP_UNARY && rebuilt->a) {
        argument = expr_clone(rebuilt->a);
        expanded = argument ? expr_simplify_try_complex_unary_cartesian(rebuilt, argument) : NULL;
        if (expanded) {
            expr_free(rebuilt);
            rebuilt = expanded;
            expanded = NULL;
            *changed = true;
        } else {
            expr_free(argument);
        }
        argument = NULL;
    }

cleanup:
    expr_free(expanded);
    expr_free(argument);
    expr_free(right);
    expr_free(left);
    return rebuilt;
}

/* Expand supported complex unary functions recursively for presentation. */
expr_t *expr_complex_unary_cartesian_for_display(const expr_t *expr)
{
    bool changed = false;
    expr_t *expanded = expr_complex_unary_cartesian_for_display_recursive(expr, &changed);

    if (!changed) {
        expr_free(expanded);
        return NULL;
    }
    return expanded;
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

    if (expr_is_op(op, &ops_tan) || expr_is_op(op, &ops_tanh)) {
        expr_t *i = expr_new_named_const(NUM_I, "i");

        inner = expr_is_op(op, &ops_tan) ? expr_tanh(arg) : expr_tan(arg);
        out = i && inner ? expr_mul(i, inner) : NULL;
        expr_free(i);
        expr_free(inner);
        expr_free(arg);
        if (!out)
            return NULL;
        expr_free(arg_node);
        return out;
    }

    if (expr_is_op(op, &ops_sec) || expr_is_op(op, &ops_sech)) {
        inner = expr_is_op(op, &ops_sec) ? expr_sech(arg) : expr_sec(arg);
        expr_free(arg);
        if (!inner)
            return NULL;
        expr_free(arg_node);
        return inner;
    }

    if (expr_is_op(op, &ops_cosec) || expr_is_op(op, &ops_cot) || expr_is_op(op, &ops_cosech) ||
        expr_is_op(op, &ops_coth)) {
        expr_t *negative_i = expr_new_named_const(NUM_NEG_I, "-i");

        if (expr_is_op(op, &ops_cosec))
            inner = expr_cosech(arg);
        else if (expr_is_op(op, &ops_cot))
            inner = expr_coth(arg);
        else if (expr_is_op(op, &ops_cosech))
            inner = expr_cosec(arg);
        else
            inner = expr_cot(arg);
        out = negative_i && inner ? expr_mul(negative_i, inner) : NULL;
        expr_free(negative_i);
        expr_free(inner);
        expr_free(arg);
        if (!out)
            return NULL;
        expr_free(arg_node);
        return out;
    }

    if (expr_is_op(op, &ops_asin) || expr_is_op(op, &ops_atan) || expr_is_op(op, &ops_asinh) ||
        expr_is_op(op, &ops_atanh)) {
        expr_t *i = expr_new_named_const(NUM_I, "i");

        if (expr_is_op(op, &ops_asin))
            inner = expr_asinh(arg);
        else if (expr_is_op(op, &ops_atan))
            inner = expr_atanh(arg);
        else if (expr_is_op(op, &ops_asinh))
            inner = expr_asin(arg);
        else
            inner = expr_atan(arg);
        out = i && inner ? expr_mul(i, inner) : NULL;
        expr_free(i);
        expr_free(inner);
        expr_free(arg);
        if (!out)
            return NULL;
        expr_free(arg_node);
        return out;
    }

    if (expr_is_op(op, &ops_log) || expr_is_op(op, &ops_log10)) {
        expr_t *square = expr_pow(arg, &NUM_TWO);
        expr_t *modulus_log = square ? expr_log(square) : NULL;
        expr_t *real_part = modulus_log ? expr_mul_simplify_owned(expr_new_const(NUM_HALF), modulus_log) : NULL;
        expr_t *zero = expr_const_zero();
        expr_t *phase = zero ? expr_atan2(arg, zero) : NULL;
        expr_t *i = expr_new_named_const(NUM_I, "i");
        expr_t *imaginary = i && phase ? expr_mul(i, phase) : NULL;

        expr_free(i);
        expr_free(phase);
        expr_free(zero);
        if (expr_is_op(op, &ops_log10)) {
            expr_t *ten = expr_const_long(10L);
            expr_t *log_ten = ten ? expr_log(ten) : NULL;

            expr_free(ten);
            real_part = log_ten ? expr_div_simplify_owned(real_part, expr_clone(log_ten)) : NULL;
            imaginary = log_ten ? expr_div_simplify_owned(imaginary, log_ten) : NULL;
        }
        out = real_part && imaginary ? expr_add(real_part, imaginary) : NULL;
        expr_free(imaginary);
        expr_free(real_part);
        expr_free(square);
        expr_free(arg);
        if (!out)
            return NULL;
        expr_free(arg_node);
        simp = expr_simplify(out);
        expr_free(out);
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
