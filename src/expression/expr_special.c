#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#define MARS_EXPR_INTEGRATE_INTERNAL_ACCESS
#include "expr_integrate_internal.h"
#include "expr_maths.h"

/* Shared special-function dependency checks. */

static bool special_uses_variable(const expr_t *expr, const expr_t *wrt)
{
    expr_t *variable = (expr_t *)wrt;
    bool used = true;

    return !expr_collect_var_usage(expr, 1u, &variable, &used) || used;
}

/* Clausen evaluation, differentiation, integration and simplification. */

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

/* Clausen Fourier sums and finite-period identities. */

/* Domain proofs must not depend on the current value of a mutable variable. */
static bool clausen_sum_is_fixed(const expr_t *expr)
{
    return !expr || (!expr_is_var(expr) && clausen_sum_is_fixed(expr->a) && clausen_sum_is_fixed(expr->b));
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
    } else if (expr_is_div(expr) && !special_uses_variable(expr->b, index)) {
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
    if (angle && !special_uses_variable(angle, index) && clausen_sum_real_angle(angle) &&
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
    if (special_uses_variable(offset, index)) {
        offset = argument->b;
        indexed_term = argument->a;
    }
    if (special_uses_variable(offset, index) || !clausen_sum_real_angle(offset))
        goto cleanup;
    increment = clausen_sum_index_quotient(indexed_term, index);
    if (!increment || special_uses_variable(increment, index))
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

/* Orthogonal-polynomial evaluation and calculus. */

typedef number_t (*orthopoly_numeric_fn)(number_t, number_t);
static const orthopoly_numeric_fn s_orthopoly_numeric[EXPR_KIND_COUNT] = {
    [EXPR_KIND_CHEBYSHEV_T] = num_chebyshev_t,
    [EXPR_KIND_CHEBYSHEV_U] = num_chebyshev_u,
    [EXPR_KIND_HERMITE_H]   = num_hermite_h,
};

static number_t orthopoly_eval(expr_t *e)
{
    number_t n = expr_eval(e->a), x = expr_eval(e->b);
    number_t out = s_orthopoly_numeric[e->ops->kind](n, x);
    num_destroy(&n);
    num_destroy(&x);
    return out;
}

static bool orthopoly_degree(const expr_t *e, long *n)
{
    if (!expr_is_const(e->a) || e->a->name || !num_is_integer(e->a->c) || !num_is_real(e->a->c))
        return false;
    double value = num_to_double(e->a->c);
    if (value < 0 || value > 64)
        return false;
    *n = (long)value;
    return true;
}

/* Expand bounded integral degrees for polynomial calculus, without changing display nodes. */
expr_t *expr_orthopoly_expand(const expr_t *e)
{
    long n;
    if (!e || !s_orthopoly_numeric[e->ops->kind] || !orthopoly_degree(e, &n))
        return NULL;
    expr_t *previous = expr_const_one(), *two = expr_const_long(2);
    expr_t *twice_x = expr_mul(two, e->b);
    expr_t *current = expr_clone(e->ops == &ops_chebyshev_t ? e->b : twice_x);
    for (long k = 1; k < n; ++k) {
        expr_t *factor = expr_const_long(e->ops == &ops_hermite_h ? 2 * k : 1);
        expr_t *front = expr_mul(twice_x, current), *back = expr_mul(factor, previous);
        expr_t *raw = expr_sub(front, back), *next = expr_simplify(raw);
        expr_free(factor);
        expr_free(front);
        expr_free(back);
        expr_free(raw);
        expr_free(previous);
        previous = current;
        current = next;
    }
    expr_t *out = expr_clone(n == 0 ? previous : current);
    expr_free(previous);
    expr_free(current);
    expr_free(two);
    expr_free(twice_x);
    return out;
}

static expr_t *orthopoly_deriv(expr_t *e)
{
    const expr_t *wrt = expr_current_wrt_internal();
    if (!wrt)
        return NULL;
    if (special_uses_variable(e->a, wrt)) {
        expr_t *variable = (expr_t *)wrt;
        return expr_new_formal_derivative(e, 1u, &variable);
    }
    if (expr_const_is_zero(e->a))
        return expr_const_zero();
    if (e->ops == &ops_chebyshev_u) {
        expr_t *expanded = expr_orthopoly_expand(e);
        if (expanded) {
            expr_t *out = expr_create_deriv(expanded, wrt);
            expr_free(expanded);
            return out;
        }
        /* A finite polynomial sum avoids removable singularities at x = ±1. */
        expr_bindings_t *bindings = expr_bindings_from_expr_internal(e);
        char name[64] = "j";
        for (unsigned suffix = 1u; expr_bindings_get(bindings, name); ++suffix)
            snprintf(name, sizeof(name), "j%u", suffix);
        expr_t *index = expr_new_named_var(NUM_NAN, name);
        expr_bindings_free(bindings);
        expr_t *two = expr_const_long(2), *zero = expr_const_zero();
        expr_t *twice_index = expr_mul(two, index), *order = expr_sub(e->a, twice_index);
        expr_t *previous = expr_add_long(order, -1);
        expr_t *poly = expr_chebyshev_u(previous, e->b), *weighted = expr_mul(order, poly);
        expr_t *term = expr_mul(two, weighted), *last = expr_add_long(e->a, -1);
        expr_t *half = expr_div(last, two), *upper = expr_floor(half);
        expr_t *sum = expr_new_finite_summation_range(term, index, zero, upper);
        expr_t *chain = expr_create_deriv(e->b, wrt), *out = chain ? expr_mul(sum, chain) : NULL;
        expr_free(index);
        expr_free(two);
        expr_free(zero);
        expr_free(twice_index);
        expr_free(order);
        expr_free(previous);
        expr_free(poly);
        expr_free(weighted);
        expr_free(term);
        expr_free(last);
        expr_free(half);
        expr_free(upper);
        expr_free(sum);
        expr_free(chain);
        return out;
    }
    expr_t *previous = expr_add_long(e->a, -1);
    expr_t *poly = e->ops == &ops_chebyshev_t ? expr_chebyshev_u(previous, e->b)
                                            : expr_hermite_h(previous, e->b);
    expr_t *factor = e->ops == &ops_chebyshev_t ? expr_clone(e->a) : expr_add(e->a, e->a);
    expr_t *local = expr_mul(factor, poly), *chain = expr_create_deriv(e->b, wrt);
    expr_t *out = chain ? expr_mul(local, chain) : NULL;
    expr_free(previous);
    expr_free(poly);
    expr_free(factor);
    expr_free(local);
    expr_free(chain);
    return out;
}

static expr_t *orthopoly_integrate(const expr_t *e, const expr_t *wrt)
{
    if (special_uses_variable(e->a, wrt))
        return NULL;
    if (e->ops == &ops_chebyshev_t) {
        expr_t *expanded = expr_orthopoly_expand(e);
        expr_t *out = expanded ? expr_integrate(expanded, wrt) : NULL;
        expr_free(expanded);
        return out;
    }
    expr_t *rate = expr_create_deriv(e->b, wrt);
    if (!rate || special_uses_variable(rate, wrt) || expr_const_is_zero(rate)) {
        expr_free(rate);
        return NULL;
    }
    expr_t *next = expr_add_long(e->a, 1);
    expr_t *poly = e->ops == &ops_hermite_h ? expr_hermite_h(next, e->b) : expr_chebyshev_t(next, e->b);
    expr_t *twice = e->ops == &ops_hermite_h ? expr_add(next, next) : expr_clone(next);
    expr_t *denominator = expr_mul(twice, rate), *out = expr_div(poly, denominator);
    expr_free(next);
    expr_free(poly);
    expr_free(twice);
    expr_free(denominator);
    expr_free(rate);
    return out;
}

static expr_t *orthopoly_simplify(const expr_t *e, expr_t *a, expr_t *b)
{
    if (expr_const_is_zero(a)) {
        expr_free(a);
        expr_free(b);
        return expr_const_one();
    }
    return expr_simplify_binary_operator(e, a, b);
}

const expr_ops_t ops_chebyshev_t = {
    .eval = orthopoly_eval,
    .deriv = orthopoly_deriv,
    .kind = EXPR_KIND_CHEBYSHEV_T,
    .arity = EXPR_OP_BINARY,
    .expression_name = "Tn",
    .function_name = "chebyshev_t",
    .TeX_name = "T",
    .apply_binary = expr_chebyshev_t,
    .simplify = orthopoly_simplify,
    .integrate = orthopoly_integrate
};

const expr_ops_t ops_chebyshev_u = {
    .eval = orthopoly_eval,
    .deriv = orthopoly_deriv,
    .kind = EXPR_KIND_CHEBYSHEV_U,
    .arity = EXPR_OP_BINARY,
    .expression_name = "Un",
    .function_name = "chebyshev_u",
    .TeX_name = "U",
    .apply_binary = expr_chebyshev_u,
    .simplify = orthopoly_simplify,
    .integrate = orthopoly_integrate
};

const expr_ops_t ops_hermite_h = {
    .eval = orthopoly_eval,
    .deriv = orthopoly_deriv,
    .kind = EXPR_KIND_HERMITE_H,
    .arity = EXPR_OP_BINARY,
    .expression_name = "ℋ",
    .function_name = "hermite_h",
    .TeX_name = "\\mathcal{H}",
    .apply_binary = expr_hermite_h,
    .simplify = orthopoly_simplify,
    .integrate = orthopoly_integrate
};

static expr_t *orthopoly_new(const expr_ops_t *ops, const expr_t *n, const expr_t *x)
{
    if (!n || !x)
        return NULL;
    expr_retain(n);
    expr_retain(x);
    return expr_new_binary_internal(ops, n, x);
}

/* Construct a first-kind Chebyshev polynomial. */
expr_t *expr_chebyshev_t(const expr_t *n, const expr_t *x)
{
    return orthopoly_new(&ops_chebyshev_t, n, x);
}
/* Construct a second-kind Chebyshev polynomial. */
expr_t *expr_chebyshev_u(const expr_t *n, const expr_t *x)
{
    return orthopoly_new(&ops_chebyshev_u, n, x);
}
/* Construct a physicists' Hermite polynomial without reusing the harmonic Hn alias. */
expr_t *expr_hermite_h(const expr_t *n, const expr_t *x)
{
    return orthopoly_new(&ops_hermite_h, n, x);
}
