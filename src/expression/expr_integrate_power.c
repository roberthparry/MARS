#include <stdbool.h>
#include <string.h>

#include "expr_internal.h"
#include "expr_integrate_internal.h"
#include "internal/number_internal.h"

expr_t *integrate_power_of_wrt(const expr_t *base,
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

static expr_t *integrate_power_of_affine(const expr_t *base,
                                         number_t exponent,
                                         const expr_t *wrt)
{
    number_t constant = num_new();
    number_t coeff = num_new();
    number_t next_exponent;
    expr_t *power;
    expr_t *out;

    if (!match_nonconstant_affine_linear_expr(base, wrt, &constant, &coeff)) {
        num_destroy(&coeff);
        num_destroy(&constant);
        return NULL;
    }
    if (num_eq(exponent, NUM_NEG_ONE)) {
        expr_t *log_base = expr_log(base);

        num_destroy(&constant);
        return div_number_owned_consuming(log_base, &coeff);
    }

    next_exponent = num_add(exponent, NUM_ONE);
    if (num_eq(next_exponent, NUM_ZERO)) {
        num_destroy(&next_exponent);
        num_destroy(&coeff);
        num_destroy(&constant);
        return NULL;
    }

    power = expr_pow(base, &next_exponent);
    out = power ? div_number_owned_by_product(power, coeff, next_exponent) : NULL;

    num_destroy(&next_exponent);
    num_destroy(&coeff);
    num_destroy(&constant);
    return out;
}

bool is_wrt_symbolic_affine_leaf(const expr_t *expr, const expr_t *wrt)
{
    if (is_wrt(expr, wrt))
        return true;
    return expr &&
           wrt &&
           expr_is_var(expr) &&
           expr_is_var(wrt) &&
           expr->name &&
           wrt->name &&
           strcmp(expr->name, wrt->name) == 0;
}

bool is_negated_wrt_symbolic_affine_leaf(const expr_t *expr, const expr_t *wrt)
{
    return expr &&
           expr_is_neg(expr) &&
           is_wrt_symbolic_affine_leaf(expr->a, wrt);
}

static bool match_symbolic_unit_affine_base(const expr_t *base,
                                            const expr_t *wrt,
                                            number_t *coeff_out)
{
    number_t coeff;

    if (!base || !wrt || !coeff_out)
        return false;

    if (is_wrt_symbolic_affine_leaf(base, wrt)) {
        coeff = num_clone(NUM_ONE);
    } else if (is_negated_wrt_symbolic_affine_leaf(base, wrt)) {
        coeff = num_clone(NUM_NEG_ONE);
    } else if (expr_is_op(base, &ops_add)) {
        if (is_wrt_symbolic_affine_leaf(base->a, wrt) &&
            !depends_on_wrt(base->b, wrt)) {
            coeff = num_clone(NUM_ONE);
        } else if (is_wrt_symbolic_affine_leaf(base->b, wrt) &&
                   !depends_on_wrt(base->a, wrt)) {
            coeff = num_clone(NUM_ONE);
        } else if (is_negated_wrt_symbolic_affine_leaf(base->a, wrt) &&
                   !depends_on_wrt(base->b, wrt)) {
            coeff = num_clone(NUM_NEG_ONE);
        } else if (is_negated_wrt_symbolic_affine_leaf(base->b, wrt) &&
                   !depends_on_wrt(base->a, wrt)) {
            coeff = num_clone(NUM_NEG_ONE);
        } else {
            return false;
        }
    } else if (expr_is_op(base, &ops_sub)) {
        if (is_wrt_symbolic_affine_leaf(base->a, wrt) &&
            !depends_on_wrt(base->b, wrt)) {
            coeff = num_clone(NUM_ONE);
        } else if (is_wrt_symbolic_affine_leaf(base->b, wrt) &&
                   !depends_on_wrt(base->a, wrt)) {
            coeff = num_clone(NUM_NEG_ONE);
        } else {
            return false;
        }
    } else {
        return false;
    }

    num_destroy(coeff_out);
    *coeff_out = coeff;
    return true;
}

expr_t *match_symbolic_wrt_factor_coeff(const expr_t *expr,
                                        const expr_t *wrt)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;

    if (!expr || !wrt)
        return NULL;

    if (is_wrt_symbolic_affine_leaf(expr, wrt))
        return expr_new_const(NUM_ONE);
    if (is_negated_wrt_symbolic_affine_leaf(expr, wrt))
        return expr_new_const(NUM_NEG_ONE);

    if (expr_is_neg(expr))
        return expr_negate_owned(match_symbolic_wrt_factor_coeff(expr->a, wrt));

    if (expr_match_mul_expr(expr, &left, &right)) {
        if (is_wrt_symbolic_affine_leaf(left, wrt) &&
            !depends_on_wrt(right, wrt))
            return expr_clone(right);
        if (is_wrt_symbolic_affine_leaf(right, wrt) &&
            !depends_on_wrt(left, wrt))
            return expr_clone(left);
        if (is_negated_wrt_symbolic_affine_leaf(left, wrt) &&
            !depends_on_wrt(right, wrt))
            return expr_negate_owned(expr_clone(right));
        if (is_negated_wrt_symbolic_affine_leaf(right, wrt) &&
            !depends_on_wrt(left, wrt))
            return expr_negate_owned(expr_clone(left));
    }

    if (expr_is_op(expr, &ops_div) && expr->a && expr->b &&
        !depends_on_wrt(expr->b, wrt)) {
        expr_t *numer_coeff = match_symbolic_wrt_factor_coeff(expr->a, wrt);
        expr_t *denom = expr_clone(expr->b);
        expr_t *quotient = (numer_coeff && denom) ? expr_div(numer_coeff, denom) : NULL;

        expr_free(denom);
        expr_free(numer_coeff);
        return simplify_owned(quotient);
    }

    return NULL;
}

static expr_t *match_symbolic_affine_base_coeff(const expr_t *base,
                                                const expr_t *wrt)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    bool is_sub = false;
    expr_t *coeff;

    coeff = match_symbolic_wrt_factor_coeff(base, wrt);
    if (coeff)
        return coeff;

    if (!expr_match_add_sub_expr(base, &left, &right, &is_sub))
        return NULL;

    coeff = match_symbolic_wrt_factor_coeff(left, wrt);
    if (coeff) {
        if (!depends_on_wrt(right, wrt))
            return coeff;
        expr_free(coeff);
    }

    coeff = match_symbolic_wrt_factor_coeff(right, wrt);
    if (coeff) {
        if (!depends_on_wrt(left, wrt))
            return is_sub ? expr_negate_owned(coeff) : coeff;
        expr_free(coeff);
    }

    return NULL;
}

static expr_t *integrate_power_of_symbolic_affine(const expr_t *base,
                                                  number_t exponent,
                                                  const expr_t *wrt)
{
    expr_t *coeff_expr = match_symbolic_affine_base_coeff(base, wrt);
    number_t next_exponent;
    expr_t *out;

    if (!coeff_expr)
        return NULL;

    if (num_eq(exponent, NUM_NEG_ONE)) {
        expr_t *log_base = expr_log(base);
        expr_t *quotient = (log_base && coeff_expr) ? expr_div(log_base, coeff_expr) : NULL;

        expr_free(log_base);
        expr_free(coeff_expr);
        return simplify_owned(quotient);
    }

    next_exponent = num_add(exponent, NUM_ONE);
    if (num_eq(next_exponent, NUM_ZERO)) {
        num_destroy(&next_exponent);
        expr_free(coeff_expr);
        return NULL;
    }

    {
        expr_t *next_exponent_expr = expr_new_const(next_exponent);
        expr_t *denominator = (coeff_expr && next_exponent_expr)
                                  ? expr_mul(coeff_expr, next_exponent_expr)
                                  : NULL;
        expr_t *power = expr_pow(base, &next_exponent);
        expr_t *quotient = (power && denominator) ? expr_div(power, denominator) : NULL;

        expr_free(power);
        expr_free(denominator);
        expr_free(next_exponent_expr);
        expr_free(coeff_expr);
        out = simplify_owned(quotient);
    }

    num_destroy(&next_exponent);
    return out;
}

static expr_t *integrate_power_of_symbolic_unit_affine(const expr_t *base,
                                                       number_t exponent,
                                                       const expr_t *wrt)
{
    number_t coeff = num_new();
    number_t next_exponent;
    expr_t *power;
    expr_t *out;

    if (!match_symbolic_unit_affine_base(base, wrt, &coeff)) {
        num_destroy(&coeff);
        return NULL;
    }
    if (num_eq(exponent, NUM_NEG_ONE)) {
        expr_t *log_base = expr_log(base);

        return div_number_owned_consuming(log_base, &coeff);
    }

    next_exponent = num_add(exponent, NUM_ONE);
    if (num_eq(next_exponent, NUM_ZERO)) {
        num_destroy(&next_exponent);
        num_destroy(&coeff);
        return NULL;
    }

    power = expr_pow(base, &next_exponent);
    out = power ? div_number_owned_by_product(power, coeff, next_exponent) : NULL;

    num_destroy(&next_exponent);
    num_destroy(&coeff);
    return out;
}

static expr_t *integrate_power_of_wrt_symbolic_exponent(const expr_t *base,
                                                        const expr_t *exponent,
                                                        const expr_t *wrt)
{
    expr_t *next_exponent;
    expr_t *power;
    expr_t *quotient;
    expr_t *out;

    if (!is_wrt(base, wrt) || depends_on_wrt(exponent, wrt))
        return NULL;

    next_exponent = expr_add_num(exponent, &NUM_ONE);
    power = next_exponent ? expr_pow_xp(wrt, next_exponent) : NULL;
    quotient = (power && next_exponent) ? expr_div(power, next_exponent) : NULL;

    expr_free(power);
    expr_free(next_exponent);

    out = simplify_owned(quotient);
    return out;
}

static expr_t *integrate_power_of_affine_symbolic_exponent(const expr_t *base,
                                                           const expr_t *exponent,
                                                           const expr_t *wrt)
{
    number_t constant = num_new();
    number_t coeff = num_new();
    expr_t *coeff_expr = NULL;
    expr_t *next_exponent;
    expr_t *denominator;
    expr_t *power;
    expr_t *quotient;
    expr_t *out;
    bool matched;

    if (!base || !exponent || depends_on_wrt(exponent, wrt)) {
        num_destroy(&coeff);
        num_destroy(&constant);
        return NULL;
    }

    matched = match_nonconstant_affine_linear_expr(base, wrt, &constant, &coeff);
    if (matched) {
        coeff_expr = expr_new_const(coeff);
    } else {
        coeff_expr = match_symbolic_affine_base_coeff(base, wrt);
        matched = coeff_expr != NULL;
    }
    num_destroy(&constant);
    num_destroy(&coeff);
    if (!matched) {
        return NULL;
    }

    next_exponent = expr_add_num(exponent, &NUM_ONE);
    denominator = (next_exponent && coeff_expr) ? expr_mul(coeff_expr, next_exponent) : NULL;
    power = next_exponent ? expr_pow_xp(base, next_exponent) : NULL;
    quotient = (power && denominator) ? expr_div(power, denominator) : NULL;

    expr_free(power);
    expr_free(denominator);
    expr_free(next_exponent);
    expr_free(coeff_expr);

    out = simplify_owned(quotient);
    return out;
}

static bool number_is_supported_sqrt_power_lift_exponent_local(number_t value)
{
    long numerator = 0;
    long denominator = 0;

    return num_get_small_rational(value, &numerator, &denominator) &&
           denominator == 1 &&
           numerator > 0 &&
           ((numerator % 2) != 0 || (numerator % 4) == 0);
}

expr_t *integrate_pow_rule(const expr_t *expr, const expr_t *wrt)
{
    number_t exponent = num_new();
    expr_t *out;

    if (!expr || !expr->a || !expr->b) {
        num_destroy(&exponent);
        return NULL;
    }

    if (!expr_match_const_value(expr->b, &exponent)) {
        out = integrate_power_of_wrt_symbolic_exponent(expr->a, expr->b, wrt);
        if (!out)
            out = integrate_power_of_affine_symbolic_exponent(expr->a, expr->b, wrt);
        num_destroy(&exponent);
        return out;
    }

    if (num_eq(exponent, NUM_TWO)) {
        out = integrate_symbolic_squared_hyperbolic(expr, wrt);
        if (out) {
            num_destroy(&exponent);
            return out;
        }
        out = integrate_matching_squared_unary_affine(expr, wrt);
        if (out) {
            num_destroy(&exponent);
            return out;
        }
    }
    number_t three = num_create_from_long(3);
    if (num_eq(exponent, three)) {
        out = integrate_matching_cubed_unary_affine(expr, wrt);
        if (out) {
            num_destroy(&three);
            num_destroy(&exponent);
            return out;
        }
    }
    num_destroy(&three);

    out = integrate_power_of_wrt(expr->a, exponent, wrt);
    if (out) {
        num_destroy(&exponent);
        return out;
    }

    out = integrate_power_of_affine(expr->a, exponent, wrt);
    if (!out)
        out = integrate_power_of_symbolic_unit_affine(expr->a, exponent, wrt);
    if (!out)
        out = integrate_power_of_symbolic_affine(expr->a, exponent, wrt);
    num_destroy(&exponent);
    return out;
}

expr_t *integrate_constant_over_power_denominator(const expr_t *numerator,
                                                  const expr_t *denominator,
                                                  const expr_t *wrt)
{
    const expr_t *base = NULL;
    number_t exponent = num_new();
    number_t neg_exponent;
    expr_t *inner = NULL;
    expr_t *scaled;
    expr_t *out;

    if (!numerator || !denominator || depends_on_wrt(numerator, wrt)) {
        num_destroy(&exponent);
        return NULL;
    }

    if (expr_is_pow_d_expr(denominator)) {
        base = denominator->a;
        num_destroy(&exponent);
        exponent = num_clone(denominator->c);
    } else if (denominator->ops && denominator->ops->kind == EXPR_KIND_POW &&
               denominator->b &&
               expr_match_const_value(denominator->b, &exponent)) {
        base = denominator->a;
    } else if (denominator->ops && denominator->ops->kind == EXPR_KIND_SQRT &&
               denominator->a) {
        const expr_t *sqrt_arg = denominator->a;
        number_t inner_exponent = num_new();
        bool lifted_power = false;

        if (expr_is_pow_d_expr(sqrt_arg) &&
            number_is_supported_sqrt_power_lift_exponent_local(sqrt_arg->c)) {
            base = sqrt_arg->a;
            num_destroy(&exponent);
            exponent = num_mul(sqrt_arg->c, NUM_HALF);
            lifted_power = true;
        } else if (sqrt_arg->ops &&
                   sqrt_arg->ops->kind == EXPR_KIND_POW &&
                   sqrt_arg->b &&
                   expr_match_const_value(sqrt_arg->b, &inner_exponent) &&
                   number_is_supported_sqrt_power_lift_exponent_local(inner_exponent)) {
            base = sqrt_arg->a;
            num_destroy(&exponent);
            exponent = num_mul(inner_exponent, NUM_HALF);
            lifted_power = true;
        }

        if (!lifted_power) {
            base = sqrt_arg;
            num_destroy(&exponent);
            exponent = num_clone(NUM_HALF);
        }
        num_destroy(&inner_exponent);
    } else {
        num_destroy(&exponent);
        return NULL;
    }

    neg_exponent = num_neg(exponent);
    inner = integrate_power_of_wrt(base, neg_exponent, wrt);
    if (!inner)
        inner = integrate_power_of_affine(base, neg_exponent, wrt);
    if (!inner)
        inner = integrate_power_of_symbolic_unit_affine(base, neg_exponent, wrt);
    if (!inner)
        inner = integrate_power_of_symbolic_affine(base, neg_exponent, wrt);
    num_destroy(&neg_exponent);
    num_destroy(&exponent);
    if (!inner)
        return NULL;

    scaled = expr_mul(numerator, inner);
    expr_free(inner);
    out = simplify_owned(scaled);
    return out;
}

expr_t *integrate_pow_d_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *out = integrate_matching_squared_unary_affine(expr, wrt);

    if (out)
        return out;
    out = integrate_symbolic_squared_hyperbolic(expr, wrt);
    if (out)
        return out;

    number_t three = num_create_from_long(3);
    if (num_eq(expr->c, three)) {
        out = integrate_matching_cubed_unary_affine(expr, wrt);
        if (out) {
            num_destroy(&three);
            return out;
        }
    }
    num_destroy(&three);

    out = integrate_power_of_wrt(expr->a, expr->c, wrt);

    if (out)
        return out;
    out = integrate_power_of_affine(expr->a, expr->c, wrt);
    if (out)
        return out;
    out = integrate_power_of_symbolic_unit_affine(expr->a, expr->c, wrt);
    if (out)
        return out;
    return integrate_power_of_symbolic_affine(expr->a, expr->c, wrt);
}

expr_t *integrate_sqrt_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *out = expr && expr->a ? integrate_sqrt_one_plus_minus_affine_square(expr->a, wrt)
                                  : NULL;

    if (out)
        return out;
    out = expr && expr->a ? integrate_sqrt_wrt_over_symbolic_unit_affine(expr->a, wrt) : NULL;
    if (out)
        return out;
    out = expr && expr->a ? integrate_symbolic_square_family_root(expr->a, wrt) : NULL;
    if (out)
        return out;
    out = expr && expr->a ? integrate_centered_quadratic_root(expr->a, wrt) : NULL;
    if (out)
        return out;
    out = expr && expr->a ? integrate_symbolic_general_quadratic_root(expr->a, wrt) : NULL;
    if (out)
        return out;
    out = integrate_power_of_wrt(expr->a, NUM_HALF, wrt);
    if (out)
        return out;
    out = integrate_power_of_affine(expr->a, NUM_HALF, wrt);
    if (out)
        return out;
    out = integrate_power_of_symbolic_unit_affine(expr->a, NUM_HALF, wrt);
    if (out)
        return out;
    return integrate_power_of_symbolic_affine(expr->a, NUM_HALF, wrt);
}
