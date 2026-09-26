#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"

static number_t bessel_i_eval(expr_t *e)
{
    number_t order = expr_eval(e->a), z = expr_eval(e->b);
    number_t result = num_bessel_i(order, z);
    num_destroy(&order);
    num_destroy(&z);
    return result;
}

static bool uses(const expr_t *e, const expr_t *x)
{
    bool used = true;
    expr_t *variable = (expr_t *)x;
    return !expr_collect_var_usage(e, 1u, &variable, &used) || used;
}

static bool negative_integer(number_t value)
{
    return num_is_finite(value) && num_is_real(value) && num_is_integer(value) && num_lt(value, NUM_ZERO);
}

static bool nonpositive_integer(number_t value)
{
    return num_is_zero(value) || negative_integer(value);
}

/* DLMF 10.27.1 applies only to integral orders; non-integral negative orders retain their branch. */
static expr_t *reflect_literal_order(expr_t *order)
{
    if (order && expr_is_const(order) && !order->name && negative_integer(order->c)) {
        number_t positive = num_neg(order->c);
        expr_t *replacement = expr_new_const(positive);
        num_destroy(&positive);
        expr_free(order);
        return replacement;
    }
    return order;
}

/* DLMF 10.29.1: the symmetric derivative introduces no removable division by z. */
static expr_t *bessel_i_deriv(expr_t *e)
{
    const expr_t *x = expr_current_wrt_internal();
    if (!x)
        return NULL;
    if (uses(e->a, x)) {
        expr_t *variable = (expr_t *)x;
        return expr_new_formal_derivative(e, 1u, &variable);
    }
    if (!uses(e->b, x))
        return expr_const_zero();

    expr_t *order = reflect_literal_order(expr_simplify(e->a));
    expr_t *lower_order = expr_add_long(order, -1), *upper_order = expr_add_long(order, 1);
    expr_t *lower = expr_bessel_i(lower_order, e->b), *upper = expr_bessel_i(upper_order, e->b);
    expr_t *sum = expr_add(lower, upper), *local = expr_div_num(sum, &NUM_TWO);
    expr_t *chain = expr_create_deriv(e->b, x), *result = expr_mul(local, chain);
    expr_free(chain); expr_free(local); expr_free(sum); expr_free(upper); expr_free(lower);
    expr_free(upper_order); expr_free(lower_order); expr_free(order);
    return result;
}

static void bessel_i_reverse(const expr_t *e, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t order = expr_eval(e->a), z = expr_eval(e->b);
    if (negative_integer(order)) {
        number_t positive = num_neg(order);
        num_destroy(&order);
        order = positive;
    }
    number_t lower_order = num_add_long(order, -1), upper_order = num_add_long(order, 1);
    number_t lower = num_bessel_i(lower_order, z), upper = num_bessel_i(upper_order, z);
    number_t sum = num_add(lower, upper), factor = num_div(sum, NUM_TWO);
    *a_bar = NUM_NAN;
    *b_bar = num_mul(*out_bar, factor);
    num_destroy(&factor); num_destroy(&sum); num_destroy(&upper); num_destroy(&lower);
    num_destroy(&upper_order); num_destroy(&lower_order); num_destroy(&z); num_destroy(&order);
}

/* DLMF 10.25.2 gives (z/2)^n 0F1(;n+1;z^2/4)/Gamma(n+1).
 * Termwise integration gives 2 (z/2)^(n+1) 1F2((n+1)/2;n+1,(n+3)/2;z^2/4)
 * divided by (n+1) Gamma(n+1). Reflection precedes parameter construction so that negative
 * integral orders never generate spurious gamma poles or terminating hypergeometric quotients. */
static expr_t *bessel_i_series(const expr_t *order, const expr_t *z, bool primitive)
{
    NUM_SCOPE(scope);
    if (!order || !z)
        return NULL;
    expr_t *n = reflect_literal_order(expr_simplify(order));
    if (!n || !expr_is_const(n) || n->name || !num_is_finite(n->c)) {
        expr_free(n);
        return NULL;
    }
    number_t gamma_parameter = num_add_long(n->c, 1);
    number_t upper_parameter = num_div(gamma_parameter, NUM_TWO);
    number_t last_lower = num_add_long(upper_parameter, 1);
    if (!num_is_finite(gamma_parameter) || nonpositive_integer(gamma_parameter) ||
        (primitive && (!num_is_finite(last_lower) || nonpositive_integer(last_lower)))) {
        expr_free(n);
        return NULL;
    }

    expr_t *square = expr_pow_long(z, 2), *four = expr_const_long(4), *q = expr_div(square, four);
    expr_t *a = expr_new_const(upper_parameter), *b = expr_new_const(gamma_parameter);
    expr_t *c = expr_new_const(last_lower);
    const expr_t *upper[] = {a}, *lower[] = {b, c};
    expr_t *hyper = a && b && c && q
                        ? expr_hypergeometric_pFq(primitive ? 1u : 0u, primitive ? upper : NULL,
                                                primitive ? 2u : 1u, lower, q)
                        : NULL;
    expr_t *result = NULL;
    if (!primitive && num_is_zero(n->c)) {
        /* Keep I0 exactly as 0F1(;1;z^2/4), without a 0^0 prefactor at zero. */
        result = hyper;
        hyper = NULL;
    } else {
        number_t exponent = primitive ? gamma_parameter : n->c;
        expr_t *half_z = expr_div_num(z, &NUM_TWO), *power = expr_pow(half_z, &exponent);
        expr_t *gamma = expr_gamma(b), *denominator = NULL, *scaled = NULL;
        if (primitive) {
            scaled = expr_mul_num(power, &NUM_TWO);
            denominator = expr_mul(b, gamma);
        } else {
            expr_retain(power);
            scaled = power;
            expr_retain(gamma);
            denominator = gamma;
        }
        expr_t *numerator = expr_mul(scaled, hyper);
        result = expr_div(numerator, denominator);
        expr_free(numerator); expr_free(denominator); expr_free(scaled); expr_free(gamma);
        expr_free(power); expr_free(half_z);
    }
    expr_free(hyper); expr_free(c); expr_free(b); expr_free(a);
    expr_free(q); expr_free(four); expr_free(square); expr_free(n);
    return result;
}

/* Expand numerical orders for symbolic verification, with the exact entire 0F1 form at order zero. */
expr_t *expr_bessel_i_hypergeometric(const expr_t *order, const expr_t *argument)
{
    return bessel_i_series(order, argument, false);
}

static expr_t *bessel_i_integrate(const expr_t *e, const expr_t *x)
{
    if (uses(e->a, x))
        return NULL;
    expr_t *rate = expr_simplify_owned(expr_create_deriv(e->b, x));
    /* A symbolic slope may vanish; specialise it before applying the affine rule. */
    if (!rate || uses(rate, x) || !expr_is_const(rate) || rate->name || !num_is_finite(rate->c) ||
        num_is_zero(rate->c)) {
        expr_free(rate);
        return NULL;
    }
    expr_t *primitive = bessel_i_series(e->a, e->b, true), *result = expr_div(primitive, rate);
    expr_free(primitive); expr_free(rate);
    return result;
}

static expr_t *bessel_i_simplify(const expr_t *e, expr_t *order, expr_t *z)
{
    order = reflect_literal_order(order);
    if (order && z && expr_is_const(order) && !order->name && num_is_finite(order->c) &&
        num_is_real(order->c) && !z->name && expr_const_is_zero(z)) {
        expr_t *result = NULL;
        if (num_is_zero(order->c))
            result = expr_const_one();
        else if (num_gt(order->c, NUM_ZERO))
            result = expr_const_zero();
        if (result) {
            expr_free(z); expr_free(order);
            return result;
        }
    }
    return expr_simplify_binary_operator(e, order, z);
}

const expr_ops_t ops_bessel_i = {
    .eval = bessel_i_eval, .deriv = bessel_i_deriv, .kind = EXPR_KIND_BESSEL_I, .arity = EXPR_OP_BINARY,
    .expression_name = "BesselI", .function_name = "besseli", .TeX_name = "I",
    .apply_binary = expr_bessel_i, .simplify = bessel_i_simplify, .integrate = bessel_i_integrate,
    .reverse = bessel_i_reverse,
};

/* Construct the principal modified Bessel function with its order and argument retained. */
expr_t *expr_bessel_i(const expr_t *order, const expr_t *argument)
{
    if (!order || !argument)
        return NULL;
    expr_retain(order);
    expr_retain(argument);
    return expr_new_binary_internal(&ops_bessel_i, order, argument);
}
