/**
 * @file expr_progression_weighted.c
 * @brief Elementary closed forms for polynomially weighted finite progressions.
 *
 * Recognises polynomial weights through degree six on sine, cosine, their hyperbolic
 * counterparts and exponential progressions. Differentiating the corresponding geometric
 * identity supplies short rational trigonometric moments without expanding the finite sum.
 * Source symbols remain linked; polynomial coefficients are collected symbolically.
 * Native presentation and sampling share this expression-module implementation.
 */
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"

enum { WEIGHTED_PROGRESSION_DEGREE = 6 };

static bool same_index(const expr_t *expression, const expr_t *index)
{
    return expression == index || (expr_is_var(expression) && expr_is_var(index) && index->var_id &&
                                  expression->var_id == index->var_id);
}

static bool uses_index(const expr_t *expression, const expr_t *index)
{
    return expression && (same_index(expression, index) || uses_index(expression->a, index) ||
                          uses_index(expression->b, index));
}

static bool entire_function(const expr_t *expression)
{
    if (!expression || !expression->ops)
        return false;
    expr_op_kind_t kind = expression->ops->kind;
    return kind == EXPR_KIND_SIN || kind == EXPR_KIND_COS || kind == EXPR_KIND_SINH ||
           kind == EXPR_KIND_COSH || kind == EXPR_KIND_EXP;
}

/* Follow multiplicative factors only; a second indexed function invalidates the polynomial weight. */
static const expr_t *weighted_function(const expr_t *expression, const expr_t *index)
{
    if (!expression || !uses_index(expression, index))
        return NULL;
    if (entire_function(expression))
        return expression;
    if (expr_is_neg(expression) || (expr_is_div(expression) && !uses_index(expression->b, index)))
        return weighted_function(expression->a, index);
    if (expr_is_mul(expression)) {
        const expr_t *left = weighted_function(expression->a, index);
        const expr_t *right = weighted_function(expression->b, index);
        return left && right ? NULL : left ? left : right;
    }
    return NULL;
}

/* Validate structure before collecting symbolic coefficients by differentiation. */
static int polynomial_degree(const expr_t *expression, const expr_t *index)
{
    if (!expression)
        return -1;
    if (!uses_index(expression, index))
        return 0;
    if (same_index(expression, index))
        return 1;
    if (expr_is_neg(expression) || (expr_is_div(expression) && !uses_index(expression->b, index)))
        return polynomial_degree(expression->a, index);
    if (expr_is_addsub(expression) || expr_is_mul(expression)) {
        int left = polynomial_degree(expression->a, index), right = polynomial_degree(expression->b, index);
        int degree = expr_is_mul(expression) ? left + right : (left > right ? left : right);
        return left < 0 || right < 0 || degree > WEIGHTED_PROGRESSION_DEGREE ? -1 : degree;
    }
    if (expression->ops->kind == EXPR_KIND_POW || expression->ops->kind == EXPR_KIND_POW_D) {
        number_t exponent = expression->ops->kind == EXPR_KIND_POW_D ? expression->c :
                            expr_is_unnamed_const(expression->b) ? expression->b->c : NUM_NAN;
        if (num_is_real(exponent) && num_is_finite(exponent) && num_is_integer(exponent) &&
            num_ge(exponent, NUM_ZERO) && num_to_double(exponent) <= WEIGHTED_PROGRESSION_DEGREE) {
            int base = polynomial_degree(expression->a, index);
            int degree = base * (int)num_to_double(exponent);
            return base < 0 || degree > WEIGHTED_PROGRESSION_DEGREE ? -1 : degree;
        }
    }
    return -1;
}

/* Rotate the geometric numerator after factoring the denominator's half-angle exponential. */
static const expr_ops_t *moment_function(const expr_ops_t *function, unsigned power, bool *negative)
{
    bool odd = (power & 1u) != 0;
    *negative = false;
    if (function->kind == EXPR_KIND_SIN || function->kind == EXPR_KIND_COS) {
        bool sine = function->kind == EXPR_KIND_SIN;
        *negative = ((power + (sine ? 0u : 1u)) & 2u) != 0;
        return sine != odd ? &ops_sin : &ops_cos;
    }
    *negative = odd;
    if (function->kind == EXPR_KIND_SINH || function->kind == EXPR_KIND_COSH)
        return (function->kind == EXPR_KIND_SINH) != odd ? &ops_sinh : &ops_cosh;
    return function;
}

/* Euler differentiation gives P_m(q)/(1-q)^(m+1), with only m+1 tail coefficients. */
static bool moment_coefficients(const expr_t *upper, unsigned order, long *head, expr_t **tail)
{
    head[1] = 1;
    tail[0] = expr_new_const(NUM_ONE);
    for (unsigned level = 0; tail[0] && level < order; ++level) {
        long next_head[WEIGHTED_PROGRESSION_DEGREE + 2] = {0};
        expr_t *next_tail[WEIGHTED_PROGRESSION_DEGREE + 1] = {0};
        bool ok = true;
        for (unsigned j = 0; j <= level + 1; ++j) {
            next_head[j] = (long)j * head[j] + (j ? (long)(level + 2 - j) * head[j - 1] : 0);
            expr_t *a = j <= level ? expr_add_long(upper, 1L + (long)j) : NULL;
            expr_t *left = a ? expr_mul(a, tail[j]) : NULL;
            expr_t *constant = j ? expr_const_long((long)level + 1L - (long)j) : NULL;
            expr_t *b = constant ? expr_sub(constant, upper) : NULL;
            expr_t *right = b ? expr_mul(b, tail[j - 1]) : NULL;
            next_tail[j] = expr_simplify_owned(left && right ? expr_add(left, right) :
                                          left ? expr_retain_expr(left) : expr_retain_expr(right));
            ok = ok && next_tail[j];
            expr_free(right); expr_free(b); expr_free(constant); expr_free(left); expr_free(a);
        }
        for (unsigned j = 0; j <= WEIGHTED_PROGRESSION_DEGREE; ++j) {
            expr_free(tail[j]);
            tail[j] = next_tail[j];
            head[j] = next_head[j];
        }
        head[WEIGHTED_PROGRESSION_DEGREE + 1] = next_head[WEIGHTED_PROGRESSION_DEGREE + 1];
        if (!ok)
            return false;
    }
    return tail[0] != NULL;
}

static expr_t *moment_wave(const expr_ops_t *wave, const expr_t *step, const expr_t *upper,
                           long offset, const expr_t *coefficient)
{
    bool reflected = !upper && offset < 0 && wave->kind != EXPR_KIND_EXP;
    if (reflected)
        offset = -offset;
    expr_t *twice_upper = upper ? expr_mul_long(upper, 2) : NULL;
    expr_t *frequency = upper ? expr_add_long(twice_upper, offset) : expr_const_long(offset);
    expr_t *product = frequency ? expr_mul(frequency, step) : NULL;
    expr_t *angle = product ? expr_div_long(product, 2) : NULL;
    expr_t *value = angle ? wave->apply_unary(angle) : NULL;
    expr_t *result = value ? expr_mul(coefficient, value) : NULL;
    if (reflected && (wave->kind == EXPR_KIND_SIN || wave->kind == EXPR_KIND_SINH))
        result = expr_negate_owned(result);
    expr_free(value); expr_free(angle); expr_free(product); expr_free(frequency); expr_free(twice_upper);
    return result;
}

static expr_t *moment(const expr_ops_t *function, const expr_t *upper, const expr_t *step, unsigned order)
{
    if (!order)
        return function->finite_progression ? function->finite_progression(upper, step) : NULL;
    bool negative;
    unsigned power = order + 1;
    const expr_ops_t *wave = moment_function(function, power, &negative);
    long head[WEIGHTED_PROGRESSION_DEGREE + 2] = {0};
    expr_t *tail[WEIGHTED_PROGRESSION_DEGREE + 1] = {0};
    expr_t *numerator = NULL;
    bool ok = moment_coefficients(upper, order, head, tail);
    for (unsigned j = 0; ok && j <= order; ++j) {
        expr_t *coefficient = head[j] ? expr_const_long(head[j]) : NULL;
        expr_t *a = coefficient ? moment_wave(wave, step, NULL, 2L * j - power, coefficient) : NULL;
        expr_t *b = moment_wave(wave, step, upper, 2L + 2L * j - power, tail[j]);
        expr_t *term = b ? a ? expr_sub(a, b) : expr_neg(b) : NULL;
        expr_t *next = term ? numerator ? expr_add(numerator, term) : expr_retain_expr(term) : NULL;
        expr_free(numerator);
        numerator = next;
        ok = numerator != NULL;
        expr_free(term); expr_free(b); expr_free(a); expr_free(coefficient);
    }
    expr_t *half = expr_div_long(step, 2);
    bool circular = function->kind == EXPR_KIND_SIN || function->kind == EXPR_KIND_COS;
    expr_t *sine = half ? circular ? expr_sin(half) : expr_sinh(half) : NULL;
    expr_t *denominator_power = sine ? expr_pow_long(sine, (long)power) : NULL;
    expr_t *denominator = denominator_power ? expr_mul_long(denominator_power, 1L << power) : NULL;
    expr_t *signed_numerator = ok ? negative ? expr_distribute_negative_for_display(numerator) :
                                               expr_retain_expr(numerator) : NULL;
    expr_t *result = expr_simplify_owned(signed_numerator && denominator ?
                                           expr_div(signed_numerator, denominator) : NULL);
    expr_free(signed_numerator);
    for (unsigned j = 0; j <= WEIGHTED_PROGRESSION_DEGREE; ++j)
        expr_free(tail[j]);
    expr_free(denominator); expr_free(denominator_power); expr_free(sine); expr_free(half); expr_free(numerator);
    return result;
}

/* Reconstruct the finite identity while preserving all authored parameter bindings. */
expr_t *expr_finite_weighted_progression_closed_form(const expr_t *expression, const expr_t **upper_out)
{
    if (upper_out)
        *upper_out = NULL;
    if (expr_is_neg(expression)) {
        expr_t *inner = expr_finite_weighted_progression_closed_form(expression->a, upper_out);
        expr_t *result = inner ? expr_neg(inner) : NULL;
        expr_free(inner);
        return result;
    }
    if (!expression || expression->ops->kind != EXPR_KIND_SUMMATION ||
        !expr_is_op(expression->b, &ops_argument_list))
        return NULL;
    const expr_t *index = expression->b->a, *bounds = expression->b->b;
    if (!expr_is_var(index) || !expr_is_op(bounds, &ops_argument_list) ||
        !expr_is_unnamed_const(bounds->a) || !num_is_one(bounds->a->c) || uses_index(bounds->b, index))
        return NULL;
    const expr_t *function = weighted_function(expression->a, index);
    if (!function)
        return NULL;
    expr_t *step = expr_simplify_extract_common_factor_quotient(function->a, index);
    expr_t *weight = expr_simplify_extract_common_factor_quotient(expression->a, function);
    int degree = step && !uses_index(step, index) ? polynomial_degree(weight, index) : -1;
    expr_t *current = degree >= 0 ? expr_clone_linked_symbols(weight, expression) : NULL;
    expr_t *result = NULL;
    long factorial = 1;
    bool ok = current != NULL;
    for (int order = 0; ok && order <= degree; ++order) {
        expr_t *at_zero = expr_substitute(current, index, EXPR_ZERO);
        expr_t *coefficient = expr_simplify_owned(at_zero ? expr_div_long(at_zero, factorial) : NULL);
        expr_free(at_zero);
        ok = coefficient && !uses_index(coefficient, index);
        if (ok && !expr_is_exact_zero(coefficient)) {
            expr_t *basis = moment(function->ops, bounds->b, step, (unsigned)order);
            expr_t *term = basis ? expr_mul(coefficient, basis) : NULL;
            expr_t *next = term ? result ? expr_add(result, term) : expr_retain_expr(term) : NULL;
            expr_free(basis);
            expr_free(term);
            expr_free(result);
            result = next;
            ok = result != NULL;
        }
        expr_free(coefficient);
        if (ok && order < degree) {
            expr_t *next = expr_create_deriv(current, index);
            expr_free(current);
            current = next;
            factorial *= order + 1;
            ok = current != NULL;
        }
    }
    if (ok && !result)
        result = expr_new_const(NUM_ZERO);
    expr_t *linked = ok && result ? expr_clone_linked_symbols(result, expression) : NULL;
    if (linked && upper_out)
        *upper_out = bounds->b;
    expr_free(result);
    expr_free(current);
    expr_free(weight);
    expr_free(step);
    return linked;
}

/* Give sampling a cheap, live distance from the removable denominator zeros. */
expr_t *expr_finite_progression_sample_guard(const expr_t *expression)
{
    while (expr_is_neg(expression))
        expression = expression->a;
    if (!expr_is_op(expression, &ops_summation) || !expr_is_op(expression->b, &ops_argument_list))
        return NULL;
    const expr_t *index = expression->b->a;
    const expr_t *function = weighted_function(expression->a, index);
    expr_t *weight = function ? expr_simplify_extract_common_factor_quotient(expression->a, function) : NULL;
    int degree = polynomial_degree(weight, index);
    expr_t *step = degree > 0 ? expr_simplify_extract_common_factor_quotient(function->a, index) : NULL;
    expr_t *half = step ? expr_div_long(step, 2) : NULL;
    bool circular = function && (function->ops->kind == EXPR_KIND_SIN || function->ops->kind == EXPR_KIND_COS);
    expr_t *guard = half ? circular ? expr_sin(half) : expr_sinh(half) : NULL;
    expr_t *linked = guard ? expr_clone_linked_symbols(guard, expression) : NULL;
    expr_free(guard); expr_free(half); expr_free(step); expr_free(weight);
    return linked;
}
