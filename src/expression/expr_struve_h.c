#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"

static number_t struve_h_eval(expr_t *e)
{
    number_t order = expr_eval(e->a), z = expr_eval(e->b);
    number_t result = num_struve_h(order, z);
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

static bool nonpositive_integer(number_t value)
{
    return num_is_finite(value) && num_is_real(value) && num_is_integer(value) && num_le(value, NUM_ZERO);
}

static bool negative_half_integer(number_t order)
{
    number_t shifted = num_add(order, NUM_HALF);
    bool result = nonpositive_integer(shifted);
    num_destroy(&shifted);
    return result;
}

static expr_t *two_over_pi(void)
{
    expr_t *two = expr_const_long(2), *pi = expr_new_const(NUM_PI);
    expr_t *result = expr_div(two, pi);
    expr_free(pi);
    expr_free(two);
    return result;
}

/* DLMF 11.4.24: 2 H'_n = H_(n-1) - H_(n+1) + (z/2)^n/(sqrt(pi) Gamma(n+3/2)). */
static expr_t *literal_argument_derivative(number_t order, const expr_t *z)
{
    NUM_SCOPE(scope);
    if (num_is_zero(order)) {
        /* DLMF 11.4.32 avoids both division by z and 0^0 at the origin. */
        expr_t *one = expr_const_one(), *upper = expr_struve_h(one, z), *constant = two_over_pi();
        expr_t *result = expr_sub(constant, upper);
        expr_free(constant); expr_free(upper); expr_free(one);
        return result;
    }

    /* H_-1 = 2/pi - H_1: differentiate H_1 and reverse the sign to avoid cancelling poles at zero. */
    number_t effective = num_eq(order, NUM_NEG_ONE) ? NUM_ONE : order;
    number_t lower_value = num_add_long(effective, -1), upper_value = num_add_long(effective, 1);
    number_t gamma_value = num_add(upper_value, NUM_HALF);
    expr_t *lower_order = expr_new_const(lower_value), *upper_order = expr_new_const(upper_value);
    expr_t *lower = expr_struve_h(lower_order, z), *upper = expr_struve_h(upper_order, z);
    expr_t *sum = expr_sub(lower, upper), *correction = NULL;

    if (nonpositive_integer(gamma_value)) {
        /* Reciprocal gamma is exactly zero here; do not construct Gamma at its pole or 0*z^n. */
        correction = expr_const_zero();
    } else {
        expr_t *half_z = expr_div_num(z, &NUM_TWO), *power = expr_pow(half_z, &effective);
        expr_t *gamma_arg = expr_new_const(gamma_value), *gamma = expr_gamma(gamma_arg);
        expr_t *pi = expr_new_const(NUM_PI), *root_pi = expr_sqrt(pi), *denominator = expr_mul(root_pi, gamma);
        correction = expr_div(power, denominator);
        expr_free(denominator); expr_free(root_pi); expr_free(pi); expr_free(gamma); expr_free(gamma_arg);
        expr_free(power); expr_free(half_z);
    }
    expr_t *total = expr_add(sum, correction), *result = expr_div_num(total, &NUM_TWO);
    if (num_eq(order, NUM_NEG_ONE))
        result = expr_negate_owned(result);
    expr_free(total); expr_free(correction); expr_free(sum); expr_free(upper); expr_free(lower);
    expr_free(upper_order); expr_free(lower_order);
    return result;
}

static expr_t *struve_h_deriv(expr_t *e)
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

    expr_t *order = expr_simplify(e->a), *local = NULL;
    if (!order)
        return NULL;
    if (expr_is_const(order) && !order->name && num_is_finite(order->c)) {
        local = literal_argument_derivative(order->c, e->b);
    } else {
        /* DLMF 11.4.27 remains valid for symbolic fixed orders without evaluating gamma at unknown poles.
         * Its quotient is local to z != 0; specialise the order before differentiating at the origin. */
        expr_t *lower_order = expr_add_long(order, -1), *lower = expr_struve_h(lower_order, e->b);
        expr_t *scaled = expr_mul(order, e), *quotient = expr_div(scaled, e->b);
        local = expr_sub(lower, quotient);
        expr_free(quotient); expr_free(scaled); expr_free(lower); expr_free(lower_order);
    }
    expr_t *chain = expr_create_deriv(e->b, x), *result = expr_mul(local, chain);
    expr_free(chain); expr_free(local); expr_free(order);
    return result;
}

/* Reuse the same guarded argument derivative for the generic reverse-mode vtable dispatch. */
static void struve_h_reverse(const expr_t *e, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t order = expr_eval(e->a);
    expr_t *local = num_is_finite(order) ? literal_argument_derivative(order, e->b) : NULL;
    number_t factor = local ? expr_eval(local) : num_clone(NUM_NAN);
    *a_bar = NUM_NAN;
    *b_bar = num_mul(*out_bar, factor);
    num_destroy(&factor);
    num_destroy(&order);
    expr_free(local);
}

/* Expand representable orders without crossing a gamma or hypergeometric denominator pole. */
expr_t *expr_struve_h_hypergeometric(const expr_t *order, const expr_t *argument)
{
    NUM_SCOPE(scope);
    if (!order || !argument)
        return NULL;
    expr_t *n = expr_simplify(order);
    if (!n || !expr_is_const(n) || n->name || !num_is_finite(n->c)) {
        expr_free(n);
        return NULL;
    }
    number_t exponent = num_add_long(n->c, 1), lower_value = num_add(exponent, NUM_HALF);
    if (!num_is_finite(exponent) || !num_is_finite(lower_value) || nonpositive_integer(lower_value)) {
        expr_free(n);
        return NULL;
    }
    number_t three_halves = num_add(NUM_ONE, NUM_HALF);
    expr_t *one = expr_const_one(), *b = expr_new_const(three_halves), *c = expr_new_const(lower_value);
    expr_t *square = expr_pow_long(argument, 2), *minus_four = expr_const_long(-4);
    expr_t *q = expr_div(square, minus_four);
    const expr_t *upper[] = {one}, *lower[] = {b, c};
    expr_t *hyper = one && b && c && q ? expr_hypergeometric_pFq(1u, upper, 2u, lower, q) : NULL;
    expr_t *coefficient = NULL;
    if (num_is_zero(n->c)) {
        /* Preserve the exact legacy form (2*z/pi) 1F2(1;3/2,3/2;-z^2/4). */
        expr_t *twice_z = expr_mul_num(argument, &NUM_TWO), *pi = expr_new_const(NUM_PI);
        coefficient = expr_div(twice_z, pi);
        expr_free(pi); expr_free(twice_z);
    } else {
        expr_t *half_z = expr_div_num(argument, &NUM_TWO);
        expr_t *power = num_is_zero(exponent) ? expr_const_one() : expr_pow(half_z, &exponent);
        expr_t *numerator = expr_mul_num(power, &NUM_TWO), *gamma = expr_gamma(c);
        expr_t *pi = expr_new_const(NUM_PI), *root_pi = expr_sqrt(pi), *denominator = expr_mul(root_pi, gamma);
        coefficient = expr_div(numerator, denominator);
        expr_free(denominator); expr_free(root_pi); expr_free(pi); expr_free(gamma);
        expr_free(numerator); expr_free(power); expr_free(half_z);
    }
    expr_t *result = expr_mul(coefficient, hyper);
    expr_free(coefficient); expr_free(hyper); expr_free(q); expr_free(minus_four); expr_free(square);
    expr_free(c); expr_free(b); expr_free(one); expr_free(n);
    return result;
}

/* Integrate the alternating series (DLMF 11.2.1), retaining the branch in powers of z/2.
 * Ordinary orders use 4 (z/2)^(n+2) 2F3(1,(n+2)/2;3/2,n+3/2,(n+4)/2;-z^2/4)
 * divided by (n+2) sqrt(pi) Gamma(n+3/2).
 * DLMF 11.4.3 gives H_-a = (-1)^(a-1/2) J_a for positive half-integral a. Integrating the
 * shifted J series gives 2 (-1)^(a-1/2) (z/2)^(a+1) 1F2((a+1)/2;a+1,(a+3)/2;-z^2/4)
 * divided by (a+1) Gamma(a+1); no gamma-pole cancellation is required. */
static expr_t *literal_primitive(number_t order, const expr_t *z)
{
    NUM_SCOPE(scope);
    bool shifted = negative_half_integer(order);
    number_t exponent = shifted ? num_add_long(num_neg(order), 1) : num_add_long(order, 2);
    number_t gamma_parameter = shifted ? num_clone(exponent) : num_add(num_add_long(order, 1), NUM_HALF);
    number_t upper_parameter = num_div(exponent, NUM_TWO), last_lower = num_add_long(upper_parameter, 1);
    number_t three_halves = num_add(NUM_ONE, NUM_HALF);

    /* Even negative integral orders contain a z^-1 term. A terminating numerator cannot cancel
     * the lower-parameter pole to manufacture their missing logarithmic primitive. */
    if (!num_is_finite(exponent) || !num_is_finite(gamma_parameter) || !num_is_finite(last_lower) ||
        num_is_zero(exponent) || nonpositive_integer(gamma_parameter) || nonpositive_integer(last_lower))
        return NULL;

    expr_t *half_z = expr_div_num(z, &NUM_TWO), *square = expr_pow_long(half_z, 2), *q = expr_neg(square);
    expr_t *one = expr_const_one(), *a = expr_new_const(upper_parameter), *b = expr_new_const(gamma_parameter);
    expr_t *c = expr_new_const(last_lower), *three_halves_expr = expr_new_const(three_halves);
    const expr_t *upper[] = {one, a}, *lower[] = {three_halves_expr, b, c};
    expr_t *hyper = one && a && b && c && three_halves_expr && q
                        ? expr_hypergeometric_pFq(shifted ? 1u : 2u, shifted ? upper + 1 : upper,
                                                shifted ? 2u : 3u, shifted ? lower + 1 : lower, q)
                        : NULL;
    number_t shifted_index = shifted ? num_sub(num_neg(order), NUM_HALF) : NUM_ZERO;
    number_t parity = shifted ? num_mod(shifted_index, NUM_TWO) : NUM_ZERO;
    long scale_value = shifted ? (num_is_zero(parity) ? 2 : -2) : 4;
    expr_t *power = expr_pow(half_z, &exponent), *scale = expr_const_long(scale_value);
    expr_t *scaled = expr_mul(scale, power), *numerator = expr_mul(scaled, hyper);
    expr_t *gamma = expr_gamma(b), *exponent_expr = expr_new_const(exponent);
    expr_t *gamma_factor = NULL;
    if (shifted) {
        gamma_factor = expr_clone(gamma);
    } else {
        expr_t *pi = expr_new_const(NUM_PI), *root_pi = expr_sqrt(pi);
        gamma_factor = expr_mul(root_pi, gamma);
        expr_free(root_pi); expr_free(pi);
    }
    expr_t *denominator = expr_mul(exponent_expr, gamma_factor), *result = expr_div(numerator, denominator);
    expr_free(denominator); expr_free(gamma_factor); expr_free(exponent_expr); expr_free(gamma);
    expr_free(numerator); expr_free(scaled); expr_free(scale); expr_free(power); expr_free(hyper);
    expr_free(three_halves_expr); expr_free(c); expr_free(b); expr_free(a); expr_free(one);
    expr_free(q); expr_free(square); expr_free(half_z);
    return result;
}

static expr_t *struve_h_integrate(const expr_t *e, const expr_t *x)
{
    if (uses(e->a, x))
        return NULL;
    expr_t *order = expr_simplify(e->a);
    if (!order || !expr_is_const(order) || order->name || !num_is_finite(order->c)) {
        expr_free(order);
        return NULL;
    }
    expr_t *rate = expr_simplify_owned(expr_create_deriv(e->b, x));
    /* An unproved symbolic slope might vanish; do not silently divide by it. */
    if (!rate || uses(rate, x) || !expr_is_const(rate) || rate->name || !num_is_finite(rate->c) ||
        num_is_zero(rate->c)) {
        expr_free(rate); expr_free(order);
        return NULL;
    }
    expr_t *primitive = literal_primitive(order->c, e->b), *result = expr_div(primitive, rate);
    expr_free(primitive); expr_free(rate); expr_free(order);
    return result;
}

static expr_t *struve_h_simplify(const expr_t *e, expr_t *order, expr_t *z)
{
    if (order && z && expr_is_const(order) && !order->name && num_is_finite(order->c) &&
        num_is_real(order->c) && !z->name && expr_const_is_zero(z)) {
        expr_t *result = NULL;
        if (num_gt(order->c, NUM_NEG_ONE) || negative_half_integer(order->c))
            result = expr_const_zero();
        else if (num_eq(order->c, NUM_NEG_ONE))
            result = two_over_pi();
        if (result) {
            expr_free(z); expr_free(order);
            return result;
        }
    }
    return expr_simplify_binary_operator(e, order, z);
}

const expr_ops_t ops_struve_h = {
    .eval = struve_h_eval, .deriv = struve_h_deriv, .kind = EXPR_KIND_STRUVE_H, .arity = EXPR_OP_BINARY,
    .expression_name = "StruveH", .function_name = "struveh", .TeX_name = "\\mathbf{H}",
    .apply_binary = expr_struve_h, .simplify = struve_h_simplify, .integrate = struve_h_integrate,
    .reverse = struve_h_reverse,
};

/* Construct the principal ordinary Struve function with its order and argument retained. */
expr_t *expr_struve_h(const expr_t *order, const expr_t *argument)
{
    if (!order || !argument)
        return NULL;
    expr_retain(order);
    expr_retain(argument);
    return expr_new_binary_internal(&ops_struve_h, order, argument);
}
