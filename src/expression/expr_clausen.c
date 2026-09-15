#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#define MARS_EXPR_INTEGRATE_INTERNAL_ACCESS
#include "expr_integrate_internal.h"

static bool clausen_order(number_t value, unsigned long *order)
{
    if (!order || !num_is_real(value) || !num_is_integer(value) || !num_gt(value, NUM_ZERO))
        return false;
    string_t *text = num_to_string(value);
    char *end = NULL;
    bool valid = false;

    if (text) {
        errno = 0;
        *order = strtoul(string_c_str(text), &end, 10);
        valid = errno != ERANGE && end && *end == '\0' && *order != 0u;
    }
    string_free(text);
    return valid;
}

static bool clausen_parts(const expr_t *expr, unsigned long *order, const expr_t **argument)
{
    if (expr_is_op(expr, &ops_clausen2)) {
        *order = 2u;
        *argument = expr->a;
        return true;
    }
    if (!expr_is_op(expr, &ops_clausen) || !clausen_order(expr_eval_num_internal(expr->a), order))
        return false;
    *argument = expr->b;
    return true;
}

static number_t eval_clausen(expr_t *expr)
{
    unsigned long order;
    const expr_t *argument;

    return clausen_parts(expr, &order, &argument) ? num_clausen(order, expr_eval_num_internal(argument)) : NUM_NAN;
}

static expr_t *clausen_derivative_factor(unsigned long order, const expr_t *argument)
{
    if (order == 1u) {
        expr_t *half = expr_div_long(argument, 2);
        expr_t *cotangent = half ? expr_cot(half) : NULL;
        expr_t *factor = cotangent ? expr_div_long(cotangent, -2) : NULL;

        expr_free(cotangent);
        expr_free(half);
        return factor;
    }
    expr_t *previous = expr_clausen(order - 1u, argument);

    return order % 2u ? expr_negate_owned(previous) : previous;
}

static expr_t *deriv_clausen(expr_t *expr)
{
    unsigned long order;
    const expr_t *argument;

    if (!clausen_parts(expr, &order, &argument))
        return expr_new_const(NUM_NAN);
    if (expr_is_op(expr, &ops_clausen)) {
        expr_t *order_derivative = expr_get_dx_internal(expr->a);
        bool constant_order = order_derivative && expr_const_is_zero(order_derivative);

        expr_free(order_derivative);
        if (!constant_order)
            return expr_new_const(NUM_NAN);
    }
    expr_t *factor = clausen_derivative_factor(order, argument);
    expr_t *derivative = expr_get_dx_internal(argument);
    expr_t *out = factor && derivative ? expr_mul(factor, derivative) : NULL;

    expr_free(derivative);
    expr_free(factor);
    return out;
}

static void reverse_clausen(const expr_t *expr, const number_t *out_bar, number_t *a_bar, number_t *b_bar)
{
    unsigned long order;
    const expr_t *argument;
    number_t derivative = NUM_NAN;

    if (clausen_parts(expr, &order, &argument)) {
        number_t value = expr_eval_num_internal(argument);

        if (order == 1u) {
            number_t half = num_div(value, NUM_TWO);
            number_t cotangent = num_cot(half);
            number_t scaled = num_div(cotangent, NUM_TWO);

            derivative = num_neg(scaled);
            num_destroy(&scaled);
            num_destroy(&cotangent);
            num_destroy(&half);
        } else {
            derivative = num_clausen(order - 1u, value);
            if (order % 2u) {
                number_t negative = num_neg(derivative);

                num_destroy(&derivative);
                derivative = negative;
            }
        }
    }
    number_t result = num_mul(*out_bar, derivative);

    num_destroy(&derivative);
    *a_bar = expr_is_op(expr, &ops_clausen2) ? result : NUM_ZERO;
    *b_bar = expr_is_op(expr, &ops_clausen2) ? NUM_ZERO : result;
}

static expr_t *integrate_clausen(const expr_t *expr, const expr_t *wrt)
{
    unsigned long order;
    const expr_t *argument;
    expr_t *constant = NULL;
    expr_t *coefficient = NULL;
    expr_t *out = NULL;

    if (!clausen_parts(expr, &order, &argument) || order == ULONG_MAX ||
        (expr_is_op(expr, &ops_clausen) && depends_on_wrt(expr->a, wrt)) ||
        !match_symbolic_affine_constant_and_coeff(argument, wrt, &constant, &coefficient) ||
        expr_const_is_zero(coefficient))
        goto cleanup;
    expr_t *next = expr_clausen(order + 1u, argument);
    expr_t *signed_next = order % 2u ? next : expr_negate_owned(next);

    out = signed_next ? simplify_owned(expr_div(signed_next, coefficient)) : NULL;
    expr_free(signed_next);

cleanup:
    expr_free(coefficient);
    expr_free(constant);
    return out;
}

static expr_t *simplify_clausen(const expr_t *expr, expr_t *a, expr_t *b)
{
    bool unary = expr_is_op(expr, &ops_clausen2);
    expr_t *argument = unary ? a : b;
    unsigned long order = 2u;
    expr_t *out = NULL;

    if (!argument || (!unary && (!expr_simplify_allows_const_identity_fold(a) || !clausen_order(a->c, &order))))
        return unary ? expr_simplify_unary_operator(expr, a, b) : expr_simplify_binary_operator(expr, a, b);
    if (expr_simplify_allows_const_identity_fold(argument)) {
        bool zero = num_is_zero(argument->c);
        bool pi = num_eq(argument->c, NUM_PI);

        if (!(order % 2u) && (zero || pi))
            out = expr_const_zero();
        else if (order == 1u && pi) {
            expr_t *two = expr_new_const(NUM_TWO);

            out = expr_negate_owned(expr_log(two));
            expr_free(two);
        } else if (order == 1u && zero) {
            out = expr_new_const(NUM_INF);
        }
    }
    if (!out && expr_is_neg(argument)) {
        out = expr_clausen(order, argument->a);
        if (!(order % 2u))
            out = expr_negate_owned(out);
    }
    if (!out)
        out = unary || order == 2u ? expr_clausen2(argument) : expr_clausen_xp(a, argument);
    expr_free(b);
    expr_free(a);
    return out;
}

const expr_ops_t ops_clausen2 = {
    .eval = eval_clausen,
    .deriv = deriv_clausen,
    .reverse = reverse_clausen,
    .kind = EXPR_KIND_CLAUSEN2,
    .arity = EXPR_OP_UNARY,
    .expression_name = "Cl₂",
    .function_name = "clausen2",
    .TeX_name = "\\operatorname{Cl}_{2}",
    .apply_unary = expr_clausen2,
    .integrate = integrate_clausen,
    .simplify = simplify_clausen
};

const expr_ops_t ops_clausen = {
    .eval = eval_clausen,
    .deriv = deriv_clausen,
    .reverse = reverse_clausen,
    .kind = EXPR_KIND_CLAUSEN,
    .arity = EXPR_OP_BINARY,
    .expression_name = "Cl",
    .function_name = "cl",
    .TeX_name = "\\operatorname{Cl}",
    .apply_binary = expr_clausen_xp,
    .integrate = integrate_clausen,
    .simplify = simplify_clausen
};

/* Construct the order-two Clausen integral without evaluating its argument. */
expr_t *expr_clausen2(const expr_t *argument)
{
    if (!argument)
        return NULL;
    expr_retain(argument);
    return expr_new_unary_internal(&ops_clausen2, argument);
}

/* Retain an explicit order expression for the parser's general Clausen notation. */
expr_t *expr_clausen_xp(const expr_t *order, const expr_t *argument)
{
    if (!order || !argument)
        return NULL;
    expr_retain(order);
    expr_retain(argument);
    return expr_new_binary_internal(&ops_clausen, order, argument);
}

/* Construct a fixed positive integer-order Clausen expression. */
expr_t *expr_clausen(unsigned long order, const expr_t *argument)
{
    if (order == 0u || !argument)
        return NULL;
    if (order == 2u)
        return expr_clausen2(argument);
    char digits[3u * sizeof(order) + 1u];

    snprintf(digits, sizeof(digits), "%lu", order);
    number_t value = num_create_from_string(digits);
    expr_t *degree = expr_new_const(value);
    expr_t *out = degree ? expr_clausen_xp(degree, argument) : NULL;

    expr_free(degree);
    num_destroy(&value);
    return out;
}
