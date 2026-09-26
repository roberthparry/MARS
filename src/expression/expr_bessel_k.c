#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"

static number_t bessel_k_eval(expr_t *e)
{
    number_t n = expr_eval(e->a), z = expr_eval(e->b);
    number_t result = num_bessel_k(n, z);
    num_destroy(&n);
    num_destroy(&z);
    return result;
}

static bool uses(const expr_t *e, const expr_t *x)
{
    bool used = true;
    expr_t *variable = (expr_t *)x;
    return !expr_collect_var_usage(e, 1u, &variable, &used) || used;
}

static expr_t *bessel_k_deriv(expr_t *e)
{
    const expr_t *x = expr_current_wrt_internal();
    if (!x)
        return NULL;
    if (uses(e->a, x)) {
        expr_t *variable = (expr_t *)x;
        return expr_new_formal_derivative(e, 1u, &variable);
    }
    expr_t *lower_n = expr_add_long(e->a, -1), *upper_n = expr_add_long(e->a, 1);
    expr_t *lower = expr_bessel_k(lower_n, e->b), *upper = expr_bessel_k(upper_n, e->b);
    number_t coefficient = num_neg(NUM_HALF);
    expr_t *sum = expr_add(lower, upper), *half = expr_new_const(coefficient);
    num_destroy(&coefficient);
    expr_t *local = expr_mul(half, sum), *chain = expr_create_deriv(e->b, x);
    expr_t *result = chain ? expr_mul(local, chain) : NULL;
    expr_free(chain); expr_free(local); expr_free(half); expr_free(sum);
    expr_free(upper); expr_free(lower); expr_free(upper_n); expr_free(lower_n);
    return result;
}

/* The K0 primitive uses the native modified Struve functions. */
static expr_t *zero_primitive(const expr_t *z)
{
    expr_t *one = expr_const_one(), *zero = expr_const_zero(), *minus_one = expr_const_long(-1);
    expr_t *l0 = expr_struve_l(zero, z), *lm1 = expr_struve_l(minus_one, z);
    expr_t *k0 = expr_bessel_k(zero, z), *k1 = expr_bessel_k(one, z);
    expr_t *left = expr_mul(k0, lm1), *right = expr_mul(k1, l0), *sum = expr_add(left, right);
    expr_t *pi = expr_new_named_const(NUM_PI, "@pi"), *pi_z = expr_mul(pi, z);
    expr_t *scale = expr_div_num(pi_z, &NUM_TWO), *result = expr_mul(scale, sum);
    expr_free(scale); expr_free(pi_z); expr_free(pi); expr_free(sum); expr_free(right); expr_free(left);
    expr_free(k1); expr_free(k0); expr_free(lm1); expr_free(l0);
    expr_free(minus_one); expr_free(zero); expr_free(one);
    return result;
}

static expr_t *bessel_k_integrate(const expr_t *e, const expr_t *x)
{
    if (uses(e->a, x) || !expr_is_const(e->a) || !num_is_real(e->a->c) || !num_is_integer(e->a->c))
        return NULL;
    double degree = fabs(num_to_double(e->a->c));
    if (degree > 64)
        return NULL;
    expr_t *rate = expr_create_deriv(e->b, x);
    if (!rate || uses(rate, x) || expr_const_is_zero(rate)) {
        expr_free(rate);
        return NULL;
    }
    long n = (long)degree;
    expr_t *zero = expr_const_zero(), *k0 = expr_bessel_k(zero, e->b);
    expr_t *primitive = n % 2 ? expr_neg(k0) : zero_primitive(e->b);
    for (long order = n % 2 + 2; order <= n; order += 2) {
        expr_t *index = expr_const_long(order-1), *k = expr_bessel_k(index, e->b);
        expr_t *minus_two = expr_const_long(-2), *term = expr_mul(minus_two, k);
        expr_t *next = expr_sub(term, primitive);
        expr_free(term); expr_free(minus_two); expr_free(k); expr_free(index); expr_free(primitive);
        primitive = next;
    }
    expr_t *result = expr_div(primitive, rate);
    expr_free(primitive); expr_free(k0); expr_free(zero); expr_free(rate);
    return result;
}

static expr_t *bessel_k_simplify(const expr_t *e, expr_t *n, expr_t *z)
{
    expr_t *positive = expr_simplify_positive_part_if_negative(n);
    if (positive) {
        expr_free(n);
        n = positive;
    }
    return expr_simplify_binary_operator(e, n, z);
}

const expr_ops_t ops_bessel_k = {
    .eval = bessel_k_eval, .deriv = bessel_k_deriv, .kind = EXPR_KIND_BESSEL_K, .arity = EXPR_OP_BINARY,
    .expression_name = "BesselK", .function_name = "besselk", .TeX_name = "K",
    .apply_binary = expr_bessel_k, .simplify = bessel_k_simplify, .integrate = bessel_k_integrate,
};

/* Construct an order-argument node, preserving the scalar function for all output styles. */
expr_t *expr_bessel_k(const expr_t *order, const expr_t *argument)
{
    if (!order || !argument)
        return NULL;
    expr_retain(order);
    expr_retain(argument);
    return expr_new_binary_internal(&ops_bessel_k, order, argument);
}
