#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#include "expr_maths.h"

/* Domain proofs must not depend on the current value of a mutable variable. */
static bool clausen_sum_is_fixed(const expr_t *expr)
{
    return !expr || (!expr_is_var(expr) && clausen_sum_is_fixed(expr->a) && clausen_sum_is_fixed(expr->b));
}

static bool clausen_sum_uses_index(const expr_t *expr, const expr_t *index)
{
    expr_t *variables[] = {(expr_t *)index};
    bool used = true;

    return !expr_collect_var_usage(expr, 1u, variables, &used) || used;
}

/* Remove one index factor while retaining every untouched subtree, including live angle variables. */
static expr_t *clausen_sum_index_quotient(const expr_t *expr, const expr_t *index)
{
    expr_t *left = NULL;
    expr_t *right = NULL;
    expr_t *result = NULL;

    if (!expr)
        return NULL;
    if (expr_struct_eq(expr, index))
        return expr_new_const(NUM_ONE);
    if (expr_is_mul(expr)) {
        left = clausen_sum_index_quotient(expr->a, index);
        if (left) {
            result = expr_mul(left, expr->b);
        } else {
            right = clausen_sum_index_quotient(expr->b, index);
            result = right ? expr_mul(expr->a, right) : NULL;
        }
    } else if (expr_is_div(expr) && !clausen_sum_uses_index(expr->b, index)) {
        left = clausen_sum_index_quotient(expr->a, index);
        result = left ? expr_div(left, expr->b) : NULL;
    } else if (expr_is_op(expr, &ops_neg)) {
        left = clausen_sum_index_quotient(expr->a, index);
        result = left ? expr_neg(left) : NULL;
    } else if (expr_is_op(expr, &ops_add) || expr_is_op(expr, &ops_sub)) {
        left = clausen_sum_index_quotient(expr->a, index);
        right = clausen_sum_index_quotient(expr->b, index);
        if (left && right)
            result = expr_is_op(expr, &ops_add) ? expr_add(left, right) : expr_sub(left, right);
    }
    expr_free(right);
    expr_free(left);
    if (result) {
        expr_t *simplified = expr_simplify(result);

        if (simplified) {
            expr_free(result);
            result = simplified;
        }
    }
    return result;
}

static bool clausen_sum_integer(number_t number, long *value)
{
    string_t *text;
    char *end = NULL;
    bool matched;

    if (!num_is_exact(number) || !num_is_real(number) || !num_is_finite(number) || !num_is_integer(number))
        return false;
    text = num_to_string(number);
    if (!text)
        return false;
    errno = 0;
    *value = strtol(string_c_str(text), &end, 10);
    matched = errno != ERANGE && end != string_c_str(text) && *end == '\0';
    string_free(text);
    return matched;
}

static bool clausen_sum_fixed_integer(const expr_t *expr, long *value)
{
    number_t number;
    bool matched;

    if (!expr || !clausen_sum_is_fixed(expr))
        return false;
    number = expr_eval((expr_t *)expr);
    matched = clausen_sum_integer(number, value);
    num_destroy(&number);
    return matched;
}

/* These operations preserve real angles without imposing assumptions on free variables. */
static bool clausen_sum_real_angle(const expr_t *angle)
{
    if (!angle)
        return false;
    if (clausen_sum_is_fixed(angle)) {
        number_t value = expr_eval((expr_t *)angle);
        bool real = num_is_real(value) && num_is_finite(value);

        num_destroy(&value);
        return real;
    }
    if (expr_is_op(angle, &ops_abs))
        return true;
    if (expr_is_op(angle, &ops_neg) || expr_is_op(angle, &ops_sin) || expr_is_op(angle, &ops_cos))
        return clausen_sum_real_angle(angle->a);
    if (expr_is_op(angle, &ops_add) || expr_is_op(angle, &ops_sub) || expr_is_op(angle, &ops_mul))
        return clausen_sum_real_angle(angle->a) && clausen_sum_real_angle(angle->b);
    if (expr_is_div(angle) && clausen_sum_real_angle(angle->a) && clausen_sum_is_fixed(angle->b)) {
        number_t denominator = expr_eval(angle->b);
        bool real = num_is_real(denominator) && num_is_finite(denominator) && !num_is_zero(denominator);

        num_destroy(&denominator);
        return real;
    }
    return false;
}

/* Cl_1 has logarithmic poles at every multiple of 2*pi. Prove a strict principal interval. */
static bool clausen_sum_first_order_angle(const expr_t *angle)
{
    number_t value;
    number_t pi;
    number_t period;
    bool regular;

    if (!angle || !clausen_sum_is_fixed(angle))
        return false;
    value = expr_eval((expr_t *)angle);
    pi = num_clone(NUM_PI);
    period = num_mul_long(pi, 2L);
    regular = num_is_real(value) && num_is_finite(value) && num_gt(value, NUM_ZERO) && num_lt(value, period);
    num_destroy(&period);
    num_destroy(&pi);
    num_destroy(&value);
    return regular;
}

static expr_t *clausen_sum_make(long order, const expr_t *angle)
{
    return order == 2L ? expr_clausen2(angle) : expr_clausen((unsigned long)order, angle);
}

/* Match both k^n and the compact constant-exponent representation produced by simplification. */
static bool clausen_sum_power(const expr_t *power, const expr_t *index, long *order)
{
    const expr_t *base = NULL;
    const expr_t *exponent = NULL;
    number_t value = NUM_NAN;
    bool matched;

    if (expr_struct_eq(power, index)) {
        *order = 1L;
        return true;
    }
    if (expr_match_pow_expr(power, &base, &exponent))
        return expr_struct_eq(base, index) && clausen_sum_fixed_integer(exponent, order);
    matched = expr_match_pow_const(power, &base, &value) && expr_struct_eq(base, index) &&
              clausen_sum_integer(value, order);
    num_destroy(&value);
    return matched;
}

static expr_t *clausen_sum_fourier(const expr_t *term, const expr_t *index)
{
    const expr_t *circular = NULL;
    expr_t *angle;
    expr_t *result = NULL;
    long order;

    if (expr_is_div(term) && clausen_sum_power(term->b, index, &order)) {
        circular = term->a;
    } else if (expr_is_mul(term)) {
        if (clausen_sum_power(term->a, index, &order))
            circular = term->b;
        else if (clausen_sum_power(term->b, index, &order))
            circular = term->a;
        if (!circular || order >= 0L || order == LONG_MIN)
            return NULL;
        order = -order;
    } else {
        return NULL;
    }
    if (order < 1L || !expr_is_op(circular, order % 2L == 0L ? &ops_sin : &ops_cos))
        return NULL;
    angle = clausen_sum_index_quotient(circular->a, index);
    if (angle && !clausen_sum_uses_index(angle, index) && clausen_sum_real_angle(angle) &&
        (order != 1L || clausen_sum_first_order_angle(angle)))
        result = clausen_sum_make(order, angle);
    expr_free(angle);
    return result;
}

/* Sum a complete set of m equally spaced angles: m^(1-n) Cl_n(m*x), for n >= 2. */
static expr_t *clausen_sum_period(const expr_t *term, const expr_t *index, const expr_t *lower,
                                const expr_t *upper)
{
    const expr_t *argument;
    const expr_t *offset;
    const expr_t *indexed_term;
    expr_t *expanded_argument = NULL;
    expr_t *increment = NULL;
    expr_t *count = NULL;
    expr_t *pi_expr = NULL;
    expr_t *period = NULL;
    expr_t *expected_step = NULL;
    expr_t *difference = NULL;
    expr_t *simplified_difference = NULL;
    expr_t *scaled_angle = NULL;
    expr_t *clausen = NULL;
    expr_t *factor = NULL;
    expr_t *result = NULL;
    number_t pi = NUM_NAN;
    long lower_value;
    long upper_value;
    long order = 2L;

    if (expr_is_op(term, &ops_clausen2))
        argument = term->a;
    else if (expr_is_op(term, &ops_clausen) && clausen_sum_fixed_integer(term->a, &order))
        argument = term->b;
    else
        return NULL;
    /* Fixed bounds avoid silently imposing integer or positivity assumptions on a live binding. */
    if (order < 2L || !clausen_sum_fixed_integer(lower, &lower_value) || lower_value != 0L ||
        !clausen_sum_fixed_integer(upper, &upper_value) || upper_value < 1L || upper_value == LONG_MAX)
        return NULL;
    expanded_argument = expr_display_expanded(argument);
    if (expanded_argument)
        argument = expanded_argument;
    if (!expr_is_op(argument, &ops_add))
        goto cleanup;
    offset = argument->a;
    indexed_term = argument->b;
    if (clausen_sum_uses_index(offset, index)) {
        offset = argument->b;
        indexed_term = argument->a;
    }
    if (clausen_sum_uses_index(offset, index) || !clausen_sum_real_angle(offset))
        goto cleanup;
    increment = clausen_sum_index_quotient(indexed_term, index);
    if (!increment || clausen_sum_uses_index(increment, index))
        goto cleanup;
    count = expr_add_long(upper, 1L);
    pi = num_clone(NUM_PI);
    pi_expr = expr_new_named_const(pi, "@pi");
    period = pi_expr ? expr_mul_long(pi_expr, 2L) : NULL;
    expected_step = period && count ? expr_div(period, count) : NULL;
    difference = expected_step ? expr_sub(increment, expected_step) : NULL;
    simplified_difference = difference ? expr_simplify(difference) : NULL;
    if (!simplified_difference || !expr_is_exact_zero(simplified_difference))
        goto cleanup;
    scaled_angle = expr_mul(count, offset);
    clausen = scaled_angle ? clausen_sum_make(order, scaled_angle) : NULL;
    factor = expr_pow_long(count, 1L - order);
    result = clausen && factor ? expr_mul(factor, clausen) : NULL;
    if (result) {
        expr_t *linked = expr_clone_linked_symbols(result, term);

        expr_free(result);
        result = linked;
    }

cleanup:
    expr_free(expanded_argument);
    expr_free(factor);
    expr_free(clausen);
    expr_free(scaled_angle);
    expr_free(simplified_difference);
    expr_free(difference);
    expr_free(expected_step);
    expr_free(period);
    expr_free(pi_expr);
    expr_free(count);
    expr_free(increment);
    num_destroy(&pi);
    return result;
}

/* Reduce real Clausen Fourier sums and complete finite periods without losing convergence conditions. */
expr_t *expr_clausen_sum_closed_form(const expr_t *expr)
{
    const expr_t *index;
    const expr_t *range;
    const expr_t *lower;
    const expr_t *upper;
    number_t endpoint;
    expr_t *result = NULL;
    long lower_value;
    bool infinite;

    if (!expr || !expr->a || !expr_is_op(expr, &ops_summation) || !expr_is_op(expr->b, &ops_argument_list))
        return NULL;
    index = expr->b->a;
    range = expr->b->b;
    if (!expr_is_var(index) || !expr_is_op(range, &ops_argument_list))
        return NULL;
    lower = range->a;
    upper = range->b;
    if (!upper || !clausen_sum_is_fixed(upper))
        return NULL;
    endpoint = expr_eval((expr_t *)upper);
    infinite = num_is_real(endpoint) && num_is_inf(endpoint) && num_get_sign(endpoint) > 0;
    num_destroy(&endpoint);
    if (infinite) {
        if (clausen_sum_fixed_integer(lower, &lower_value) && lower_value == 1L)
            result = clausen_sum_fourier(expr->a, index);
    } else {
        result = clausen_sum_period(expr->a, index, lower, upper);
    }
    if (result) {
        expr_t *simplified = expr_simplify(result);

        if (simplified) {
            expr_free(result);
            result = simplified;
        }
    }
    return result;
}
