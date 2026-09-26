#include "expr_maths.h"

static bool uses(const expr_t *e, const expr_t *x)
{
    bool used = true;
    expr_t *variable = (expr_t *)x;
    return !expr_collect_var_usage(e, 1u, &variable, &used) || used;
}

static bool odd_integer(number_t value)
{
    number_t remainder = num_mod(value, NUM_TWO);
    bool odd = !num_is_zero(remainder);
    num_destroy(&remainder);
    return odd;
}

/* Evaluate the principal Bessel Y function through the number module. */
number_t eval_bessel_y(expr_t *e)
{
    number_t order = expr_eval(e->a), z = expr_eval(e->b);
    number_t result = num_bessel_y(order, z);
    num_destroy(&z); num_destroy(&order);
    return result;
}

static expr_t *argument_derivative(const expr_t *order, const expr_t *z)
{
    expr_t *n = expr_simplify(order);
    if (!n)
        return NULL;
    bool literal = expr_is_const(n) && !n->name && num_is_finite(n->c);
    if (literal && num_is_zero(n->c)) {
        /* Y0' = -Y1 preserves its genuine origin singularity without subtracting infinities. */
        expr_t *one = expr_const_one(), *y1 = expr_bessel_y(one, z), *result = expr_neg(y1);
        expr_free(y1); expr_free(one); expr_free(n);
        return result;
    }
    /* Keep both adjacent orders in the native Y backend, including negative half-integral orders.
     * At zero, the numeric backend's extension (or rejection) is inherited consistently. */
    expr_t *lower_order = expr_add_long(n, -1), *upper_order = expr_add_long(n, 1);
    expr_t *lower = expr_bessel_y(lower_order, z), *upper = expr_bessel_y(upper_order, z);
    expr_t *difference = expr_sub(lower, upper), *result = expr_div_num(difference, &NUM_TWO);
    expr_free(difference); expr_free(upper); expr_free(lower);
    expr_free(upper_order); expr_free(lower_order); expr_free(n);
    return result;
}

/* Differentiate in the argument; order-dependent derivatives remain formal. */
expr_t *deriv_bessel_y(expr_t *e)
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
    expr_t *local = argument_derivative(e->a, e->b), *chain = expr_create_deriv(e->b, x);
    expr_t *result = expr_mul(local, chain);
    expr_free(chain); expr_free(local);
    return result;
}

/* Reuse the guarded local derivative in the existing reverse-mode dispatch. */
void expr_reverse_bessel_y(const expr_t *e, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    number_t order_value = expr_eval(e->a);
    expr_t *order = expr_new_const(order_value);
    expr_t *local = num_is_finite(order_value) ? argument_derivative(order, e->b) : NULL;
    number_t factor = local ? expr_eval(local) : num_clone(NUM_NAN);
    *a_bar = NUM_NAN;
    *b_bar = num_mul(*out_bar, factor);
    num_destroy(&factor); num_destroy(&order_value);
    expr_free(local); expr_free(order);
}

static expr_t *bessel_y_simplify(const expr_t *e, expr_t *order, expr_t *z)
{
    if (order && z && expr_is_const(order) && !order->name && num_is_finite(order->c) &&
        num_is_real(order->c) && num_lt(order->c, NUM_ZERO) && num_is_integer(order->c)) {
        number_t positive = num_neg(order->c);
        bool negate = odd_integer(positive);
        expr_t *n = expr_new_const(positive), *result = expr_bessel_y(n, z);
        if (negate)
            result = expr_negate_owned(result);
        num_destroy(&positive);
        expr_free(n); expr_free(z); expr_free(order);
        return result;
    }
    return expr_simplify_binary_operator(e, order, z);
}

const expr_ops_t ops_bessel_y = {
    .eval = eval_bessel_y, .deriv = deriv_bessel_y, .reverse = expr_reverse_bessel_y,
    .kind = EXPR_KIND_BESSEL_Y, .arity = EXPR_OP_BINARY,
    .expression_name = "BesselY", .function_name = "bessely", .TeX_name = "Y",
    .apply_binary = expr_bessel_y, .integrate = expr_integrate_dispatch_primitive, .simplify = bessel_y_simplify,
};

/* Construct the principal Bessel function of the second kind, retaining its operands. */
expr_t *expr_bessel_y(const expr_t *order, const expr_t *argument)
{
    if (!order || !argument)
        return NULL;
    expr_retain(order);
    expr_retain(argument);
    return expr_new_binary_internal(&ops_bessel_y, order, argument);
}
