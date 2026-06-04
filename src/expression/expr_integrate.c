#include <stdbool.h>

#include "expr_internal.h"

typedef expr_t *(*expr_integrate_rule_fn)(const expr_t *expr, const expr_t *wrt);

static expr_t *integrate_dispatch(const expr_t *expr, const expr_t *wrt);

static expr_t *simplify_owned(expr_t *expr)
{
    expr_t *simplified;

    if (!expr)
        return NULL;
    simplified = expr_simplify(expr);
    expr_free(expr);
    return simplified;
}

static bool depends_on_wrt(const expr_t *expr, const expr_t *wrt)
{
    expr_t *vars[1];
    bool used[1];

    vars[0] = (expr_t *)wrt;
    return expr_collect_var_usage(expr, 1u, vars, used) && used[0];
}

static bool is_wrt(const expr_t *expr, const expr_t *wrt)
{
    return expr && wrt && expr_is_var(expr) && expr == wrt;
}

static expr_t *mul_number_owned(expr_t *expr, number_t factor)
{
    expr_t *scaled;

    if (!expr)
        return NULL;
    scaled = expr_mul_num(expr, &factor);
    expr_free(expr);
    return simplify_owned(scaled);
}

static expr_t *div_number_owned(expr_t *expr, number_t denom)
{
    expr_t *scaled;

    if (!expr)
        return NULL;
    if (num_eq(denom, NUM_ZERO)) {
        expr_free(expr);
        return NULL;
    }
    scaled = expr_div_num(expr, &denom);
    expr_free(expr);
    return simplify_owned(scaled);
}

static expr_t *integrate_as_constant(const expr_t *expr, const expr_t *wrt)
{
    return simplify_owned(expr_mul(expr, wrt));
}

static expr_t *integrate_power_of_wrt(const expr_t *base,
                                      number_t exponent,
                                      const expr_t *wrt)
{
    number_t next_exponent;
    expr_t *power;
    expr_t *out;

    if (!is_wrt(base, wrt))
        return NULL;
    if (num_eq(exponent, NUM_NEG_ONE))
        return simplify_owned(expr_log(wrt));

    next_exponent = num_add(exponent, NUM_ONE);
    if (num_eq(next_exponent, NUM_ZERO)) {
        num_destroy(&next_exponent);
        return NULL;
    }

    power = expr_pow(wrt, &next_exponent);
    out = div_number_owned(power, next_exponent);
    num_destroy(&next_exponent);
    return out;
}

static expr_t *integrate_constant_rule(const expr_t *expr, const expr_t *wrt)
{
    if (!depends_on_wrt(expr, wrt))
        return integrate_as_constant(expr, wrt);
    return NULL;
}

static expr_t *integrate_var_rule(const expr_t *expr, const expr_t *wrt)
{
    if (is_wrt(expr, wrt))
        return integrate_power_of_wrt(expr, NUM_ONE, wrt);
    return integrate_as_constant(expr, wrt);
}

static expr_t *integrate_add_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *left = integrate_dispatch(expr->a, wrt);
    expr_t *right;
    expr_t *sum;

    if (!left)
        return NULL;
    right = integrate_dispatch(expr->b, wrt);
    if (!right) {
        expr_free(left);
        return NULL;
    }
    sum = expr_add(left, right);
    expr_free(right);
    expr_free(left);
    return simplify_owned(sum);
}

static expr_t *integrate_sub_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *left = integrate_dispatch(expr->a, wrt);
    expr_t *right;
    expr_t *diff;

    if (!left)
        return NULL;
    right = integrate_dispatch(expr->b, wrt);
    if (!right) {
        expr_free(left);
        return NULL;
    }
    diff = expr_sub(left, right);
    expr_free(right);
    expr_free(left);
    return simplify_owned(diff);
}

static expr_t *integrate_neg_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *inner = integrate_dispatch(expr->a, wrt);
    expr_t *negated;

    if (!inner)
        return NULL;
    negated = expr_neg(inner);
    expr_free(inner);
    return simplify_owned(negated);
}

static expr_t *integrate_scaled_rule(const expr_t *expr, const expr_t *wrt)
{
    number_t scale = num_new();
    const expr_t *base = NULL;
    expr_t *inner;
    expr_t *out;

    if (!expr_match_scaled_expr(expr, &scale, &base) || !base || base == expr) {
        num_destroy(&scale);
        return NULL;
    }

    inner = integrate_dispatch(base, wrt);
    if (!inner) {
        num_destroy(&scale);
        return NULL;
    }
    out = mul_number_owned(inner, scale);
    num_destroy(&scale);
    return out;
}

static expr_t *integrate_mul_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *scaled = integrate_scaled_rule(expr, wrt);
    bool left_depends;
    bool right_depends;
    expr_t *inner;
    expr_t *product;

    if (scaled)
        return scaled;

    left_depends = depends_on_wrt(expr->a, wrt);
    right_depends = depends_on_wrt(expr->b, wrt);
    if (left_depends && right_depends)
        return NULL;

    if (!left_depends) {
        inner = integrate_dispatch(expr->b, wrt);
        product = inner ? expr_mul(expr->a, inner) : NULL;
    } else {
        inner = integrate_dispatch(expr->a, wrt);
        product = inner ? expr_mul(inner, expr->b) : NULL;
    }
    expr_free(inner);
    return simplify_owned(product);
}

static expr_t *integrate_div_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *scaled = integrate_scaled_rule(expr, wrt);
    expr_t *inner;
    expr_t *quotient;

    if (scaled)
        return scaled;

    if (!depends_on_wrt(expr->b, wrt)) {
        inner = integrate_dispatch(expr->a, wrt);
        quotient = inner ? expr_div(inner, expr->b) : NULL;
        expr_free(inner);
        return simplify_owned(quotient);
    }

    if (is_wrt(expr->b, wrt) && !depends_on_wrt(expr->a, wrt)) {
        expr_t *log_x = expr_log(wrt);

        quotient = log_x ? expr_mul(expr->a, log_x) : NULL;
        expr_free(log_x);
        return simplify_owned(quotient);
    }

    return NULL;
}

static expr_t *integrate_pow_d_rule(const expr_t *expr, const expr_t *wrt)
{
    return integrate_power_of_wrt(expr->a, expr->c, wrt);
}

static expr_t *integrate_sqrt_rule(const expr_t *expr, const expr_t *wrt)
{
    return integrate_power_of_wrt(expr->a, NUM_HALF, wrt);
}

static expr_t *integrate_log_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *x_log_x;
    expr_t *raw;

    if (!is_wrt(expr->a, wrt))
        return NULL;

    x_log_x = expr_mul(wrt, expr);
    raw = x_log_x ? expr_sub(x_log_x, wrt) : NULL;
    expr_free(x_log_x);
    return simplify_owned(raw);
}

static expr_t *integrate_affine_unary_kind(const expr_t *expr,
                                           const expr_t *wrt,
                                           expr_pattern_unary_affine_kind_t kind,
                                           expr_apply_unary_fn antiderivative_fn,
                                           number_t sign)
{
    expr_t *vars[1];
    number_t constant = num_new();
    number_t coeffs[1];
    expr_t *anti;
    expr_t *out;

    vars[0] = (expr_t *)wrt;
    coeffs[0] = num_new();
    if (!expr_match_unary_affine_kind(expr, kind, 1u, vars, &constant, coeffs) ||
        num_eq(coeffs[0], NUM_ZERO)) {
        num_destroy(&coeffs[0]);
        num_destroy(&constant);
        return NULL;
    }

    anti = antiderivative_fn(expr->a);
    if (num_eq(sign, NUM_NEG_ONE)) {
        expr_t *negated = anti ? expr_neg(anti) : NULL;

        expr_free(anti);
        anti = negated;
    }

    out = div_number_owned(anti, coeffs[0]);
    num_destroy(&coeffs[0]);
    num_destroy(&constant);
    return out;
}

static expr_t *integrate_exp_rule(const expr_t *expr, const expr_t *wrt)
{
    return integrate_affine_unary_kind(expr, wrt, EXPR_PATTERN_UNARY_EXP,
                                      expr_exp, NUM_ONE);
}

static expr_t *integrate_sin_rule(const expr_t *expr, const expr_t *wrt)
{
    return integrate_affine_unary_kind(expr, wrt, EXPR_PATTERN_UNARY_SIN,
                                      expr_cos, NUM_NEG_ONE);
}

static expr_t *integrate_cos_rule(const expr_t *expr, const expr_t *wrt)
{
    return integrate_affine_unary_kind(expr, wrt, EXPR_PATTERN_UNARY_COS,
                                      expr_sin, NUM_ONE);
}

static expr_t *integrate_tan_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *vars[1];
    number_t constant = num_new();
    number_t coeffs[1];
    expr_t *cos_arg;
    expr_t *log_cos;
    expr_t *negated;
    expr_t *out;

    vars[0] = (expr_t *)wrt;
    coeffs[0] = num_new();
    if (!expr_match_unary_affine_kind(expr, EXPR_PATTERN_UNARY_TAN, 1u, vars,
                                      &constant, coeffs) ||
        num_eq(coeffs[0], NUM_ZERO)) {
        num_destroy(&coeffs[0]);
        num_destroy(&constant);
        return NULL;
    }

    cos_arg = expr_cos(expr->a);
    log_cos = cos_arg ? expr_log(cos_arg) : NULL;
    negated = log_cos ? expr_neg(log_cos) : NULL;
    out = div_number_owned(negated, coeffs[0]);

    expr_free(log_cos);
    expr_free(cos_arg);
    num_destroy(&coeffs[0]);
    num_destroy(&constant);
    return out;
}

static expr_t *integrate_sinh_rule(const expr_t *expr, const expr_t *wrt)
{
    return integrate_affine_unary_kind(expr, wrt, EXPR_PATTERN_UNARY_SINH,
                                      expr_cosh, NUM_ONE);
}

static expr_t *integrate_cosh_rule(const expr_t *expr, const expr_t *wrt)
{
    return integrate_affine_unary_kind(expr, wrt, EXPR_PATTERN_UNARY_COSH,
                                      expr_sinh, NUM_ONE);
}

static expr_t *integrate_tanh_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *vars[1];
    number_t constant = num_new();
    number_t coeffs[1];
    expr_t *cosh_arg;
    expr_t *log_cosh;
    expr_t *out;

    vars[0] = (expr_t *)wrt;
    coeffs[0] = num_new();
    if (!expr_match_unary_affine_kind(expr, EXPR_PATTERN_UNARY_TANH, 1u, vars,
                                      &constant, coeffs) ||
        num_eq(coeffs[0], NUM_ZERO)) {
        num_destroy(&coeffs[0]);
        num_destroy(&constant);
        return NULL;
    }

    cosh_arg = expr_cosh(expr->a);
    log_cosh = cosh_arg ? expr_log(cosh_arg) : NULL;
    out = div_number_owned(log_cosh, coeffs[0]);

    expr_free(cosh_arg);
    num_destroy(&coeffs[0]);
    num_destroy(&constant);
    return out;
}

typedef struct expr_integrate_rule {
    expr_op_kind_t kind;
    expr_integrate_rule_fn integrate;
} expr_integrate_rule_t;

static const expr_integrate_rule_t rules[] = {
    { EXPR_KIND_CONST, integrate_constant_rule },
    { EXPR_KIND_VAR,   integrate_var_rule },
    { EXPR_KIND_ADD,   integrate_add_rule },
    { EXPR_KIND_SUB,   integrate_sub_rule },
    { EXPR_KIND_NEG,   integrate_neg_rule },
    { EXPR_KIND_MUL,   integrate_mul_rule },
    { EXPR_KIND_DIV,   integrate_div_rule },
    { EXPR_KIND_POW_D, integrate_pow_d_rule },
    { EXPR_KIND_SQRT,  integrate_sqrt_rule },
    { EXPR_KIND_LOG,   integrate_log_rule },
    { EXPR_KIND_EXP,   integrate_exp_rule },
    { EXPR_KIND_SIN,   integrate_sin_rule },
    { EXPR_KIND_COS,   integrate_cos_rule },
    { EXPR_KIND_TAN,   integrate_tan_rule },
    { EXPR_KIND_SINH,  integrate_sinh_rule },
    { EXPR_KIND_COSH,  integrate_cosh_rule },
    { EXPR_KIND_TANH,  integrate_tanh_rule }
};

static expr_t *integrate_dispatch(const expr_t *expr, const expr_t *wrt)
{
    if (!expr || !wrt)
        return NULL;

    if (!depends_on_wrt(expr, wrt))
        return integrate_as_constant(expr, wrt);

    for (size_t i = 0; i < sizeof(rules) / sizeof(rules[0]); ++i)
        if (expr->ops->kind == rules[i].kind)
            return rules[i].integrate(expr, wrt);

    return NULL;
}

expr_t *expr_integrate(const expr_t *expr, const expr_t *wrt)
{
    expr_t *simplified;
    expr_t *raw;

    if (!expr || !wrt || !expr_is_var(wrt))
        return NULL;

    simplified = expr_simplify(expr);
    if (!simplified)
        return NULL;

    raw = integrate_dispatch(simplified, wrt);
    expr_free(simplified);
    return simplify_owned(raw);
}
