/**
 * @file expr_progression_weighted.c
 * @brief Elementary and Lambert closed forms for polynomially weighted finite progressions.
 *
 * Recognises polynomial weights through degree six on sine, cosine, their hyperbolic
 * counterparts and exponential progressions. Differentiating the corresponding geometric
 * identity supplies short rational trigonometric moments without expanding the finite sum.
 * Source symbols remain linked; polynomial coefficients are collected symbolically.
 * Native presentation and sampling share this expression-module implementation.
 * Expanded tangent, cotangent and hyperbolic derivative polynomials are also recognised,
 * independently of how the summand was entered or obtained by calculus.
 * Rational arctangent derivative sums recover their digamma identities when entered directly.
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
static int polynomial_degree(const expr_t *expression, const expr_t *index, int limit)
{
    if (!expression)
        return -1;
    if (!uses_index(expression, index))
        return 0;
    if (same_index(expression, index))
        return 1;
    if (expr_is_neg(expression) || (expr_is_div(expression) && !uses_index(expression->b, index)))
        return polynomial_degree(expression->a, index, limit);
    if (expr_is_addsub(expression) || expr_is_mul(expression)) {
        int left = polynomial_degree(expression->a, index, limit), right = polynomial_degree(expression->b, index, limit);
        int degree = expr_is_mul(expression) ? left + right : (left > right ? left : right);
        return left < 0 || right < 0 || degree > limit ? -1 : degree;
    }
    if (expression->ops->kind == EXPR_KIND_POW || expression->ops->kind == EXPR_KIND_POW_D) {
        number_t exponent = expression->ops->kind == EXPR_KIND_POW_D ? expression->c :
                            expr_is_unnamed_const(expression->b) ? expression->b->c : NUM_NAN;
        if (num_is_real(exponent) && num_is_finite(exponent) && num_is_integer(exponent) &&
            num_ge(exponent, NUM_ZERO) && num_to_double(exponent) <= limit) {
            int base = polynomial_degree(expression->a, index, limit);
            int degree = base * (int)num_to_double(exponent);
            return base < 0 || degree > limit ? -1 : degree;
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
    int degree = step && !uses_index(step, index) ? polynomial_degree(weight, index, WEIGHTED_PROGRESSION_DEGREE) : -1;
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

/* A bounded arithmetic-tree walk finds the wave; polynomial rebuilding rejects every incompatible occurrence. */
static const expr_t *quotient_wave(const expr_t *expression, const expr_t *index)
{
    if (!expression || !uses_index(expression, index))
        return NULL;
    expr_op_kind_t kind = expression->ops->kind;
    if (kind == EXPR_KIND_TAN || kind == EXPR_KIND_COT || kind == EXPR_KIND_TANH || kind == EXPR_KIND_COTH)
        return expression;
    if (!expr_is_neg(expression) && !expr_is_addsub(expression) && !expr_is_mul(expression) &&
        !expr_is_div(expression) && kind != EXPR_KIND_POW && kind != EXPR_KIND_POW_D)
        return NULL;
    const expr_t *left = quotient_wave(expression->a, index);
    return left ? left : quotient_wave(expression->b, index);
}

/* Replace all structurally identical wave calls, retaining source symbols rather than their current values. */
static expr_t *quotient_polynomial(const expr_t *expression, const expr_t *index,
                                   const expr_t *wave, const expr_t *variable)
{
    if (!expression)
        return NULL;
    if (!uses_index(expression, index) || same_index(expression, index))
        return expr_retain_expr(expression);
    if (expression->ops->kind == wave->ops->kind && expr_struct_eq(expression->a, wave->a))
        return expr_retain_expr(variable);
    expr_op_kind_t kind = expression->ops->kind;
    if (!expr_is_neg(expression) && !expr_is_addsub(expression) && !expr_is_mul(expression) &&
        !expr_is_div(expression) && kind != EXPR_KIND_POW && kind != EXPR_KIND_POW_D)
        return NULL;
    expr_t *left = quotient_polynomial(expression->a, index, wave, variable), *out = NULL;
    if (expr_is_neg(expression))
        out = left ? expr_neg(left) : NULL;
    else if (kind == EXPR_KIND_POW_D)
        out = left ? expr_pow(left, &expression->c) : NULL;
    else {
        expr_t *right = quotient_polynomial(expression->b, index, wave, variable);
        out = left && right ? kind == EXPR_KIND_POW && expr_is_unnamed_const(right) ? expr_pow(left, &right->c) :
                              expression->ops->apply_binary(left, right) : NULL;
        expr_free(right);
    }
    expr_free(left);
    return out;
}

/* Taylor coefficients are exact symbolic expressions; no numerical bindings enter recognition. */
static bool polynomial_coefficients(const expr_t *polynomial, const expr_t *variable, int degree, expr_t **coefficients)
{
    NUM_SCOPE(scope);
    expr_t *current = expr_retain_expr(polynomial);
    number_t factorial = num_clone(NUM_ONE);
    bool ok = current != NULL;
    for (int j = 0; ok && j <= degree; ++j) {
        expr_t *zero = expr_substitute(current, variable, EXPR_ZERO), *divisor = expr_new_const(factorial);
        coefficients[j] = expr_simplify_owned(zero && divisor ? expr_div(zero, divisor) : NULL);
        expr_free(divisor);
        expr_free(zero);
        ok = coefficients[j] && !uses_index(coefficients[j], variable);
        if (ok && j < degree) {
            expr_t *next = expr_create_deriv(current, variable);
            expr_free(current);
            current = next;
            factorial = num_mul_long(factorial, j + 1);
            ok = current != NULL;
        }
    }
    expr_free(current);
    return ok;
}

/* Re-entered arctangent derivative sums retain their identity without relying on action history. */
expr_t *expr_finite_rational_progression_closed_form(const expr_t *expression)
{
    if (expr_is_neg(expression)) {
        expr_t *inner = expr_finite_rational_progression_closed_form(expression->a);
        expr_t *out = inner ? expr_neg(inner) : NULL;
        expr_free(inner);
        return out;
    }
    if (!expr_is_op(expression, &ops_summation) || !expr_is_op(expression->b, &ops_argument_list) ||
        !expr_is_div(expression->a))
        return NULL;
    const expr_t *index = expression->b->a, *bounds = expression->b->b, *term = expression->a;
    if (!expr_is_var(index) || !expr_is_op(bounds, &ops_argument_list) ||
        !expr_is_unnamed_const(bounds->a) || !num_is_one(bounds->a->c) || uses_index(bounds->b, index) ||
        polynomial_degree(term->a, index, 1) != 1 || polynomial_degree(term->b, index, 2) != 2)
        return NULL;
    expr_t *numerator[2] = {0}, *denominator[3] = {0};
    bool ok = polynomial_coefficients(term->a, index, 1, numerator) &&
              polynomial_coefficients(term->b, index, 2, denominator) &&
              expr_is_exact_zero(numerator[0]) && expr_is_unnamed_const(denominator[0]) &&
              num_is_one(denominator[0]->c) &&
              expr_is_exact_zero(denominator[1]);
    const expr_t *step = NULL;
    number_t exponent = NUM_NAN;
    ok = ok && expr_match_pow_const(denominator[2], &step, &exponent) && num_eq(exponent, NUM_TWO);
    expr_t *out = ok ? expr_finite_atan_moment_form(bounds->b, step, numerator[1]) : NULL;
    expr_t *linked = out ? expr_clone_linked_symbols(out, expression) : NULL;
    num_destroy(&exponent);
    expr_free(out);
    for (size_t i = 0; i < 2u; ++i)
        expr_free(numerator[i]);
    for (size_t i = 0; i < 3u; ++i)
        expr_free(denominator[i]);
    return linked;
}

/* G_0(t)=t and G_(n+1)(t)=(constant+quadratic*t^2)*G_n'(t), through supported Lambert order 32. */
static void quotient_derivative_coefficients(int order, long constant, long quadratic, number_t coefficients[34])
{
    for (int j = 0; j < 34; ++j)
        coefficients[j] = j == 1 ? NUM_ONE : NUM_ZERO;
    for (int n = 0; n < order; ++n) {
        number_t next[34];
        for (int j = 0; j < 34; ++j) {
            number_t lower = j + 1 < 34 ? num_mul_long(coefficients[j + 1], constant * (j + 1)) : NUM_ZERO;
            number_t higher = j > 0 ? num_mul_long(coefficients[j - 1], quadratic * (j - 1)) : NUM_ZERO;
            next[j] = num_add(lower, higher);
        }
        for (int j = 0; j < 34; ++j)
            coefficients[j] = next[j];
    }
}

/* Recover a weighted derivative polynomial, rather than depending on calculus-action provenance. */
expr_t *expr_finite_quotient_derivative_closed_form(const expr_t *expression, const expr_t **upper_out)
{
    NUM_SCOPE(scope);
    if (upper_out)
        *upper_out = NULL;
    if (expr_is_neg(expression)) {
        expr_t *inner = expr_finite_quotient_derivative_closed_form(expression->a, upper_out);
        expr_t *out = inner ? expr_neg(inner) : NULL;
        expr_free(inner);
        return out;
    }
    if (!expression || !expr_is_op(expression, &ops_summation) || !expr_is_op(expression->b, &ops_argument_list))
        return NULL;
    const expr_t *index = expression->b->a, *bounds = expression->b->b;
    if (!expr_is_var(index) || !expr_is_op(bounds, &ops_argument_list) ||
        !expr_is_unnamed_const(bounds->a) || !num_is_one(bounds->a->c) || uses_index(bounds->b, index))
        return NULL;
    const expr_t *wave = quotient_wave(expression->a, index);
    if (!wave || wave == expression->a)
        return NULL;
    expr_t *step = expr_simplify_extract_common_factor_quotient(wave->a, index);
    expr_t *variable = expr_new_var(NUM_NAN);
    expr_t *polynomial = step && !uses_index(step, index) ?
                         quotient_polynomial(expression->a, index, wave, variable) : NULL;
    int degree = polynomial_degree(polynomial, variable, 33);
    if (degree < 2) {
        expr_free(polynomial);
        expr_free(variable);
        expr_free(step);
        return NULL;
    }
    expr_t *coefficients[34] = {0}, *weight = NULL, *result = NULL;
    bool ok = polynomial_coefficients(polynomial, variable, degree, coefficients);
    expr_op_kind_t kind = wave->ops->kind;
    bool hyperbolic = kind == EXPR_KIND_TANH || kind == EXPR_KIND_COTH;
    number_t basis[34];
    if (ok) {
        quotient_derivative_coefficients(degree - 1, kind == EXPR_KIND_COT ? -1 : 1, hyperbolic ? -1 :
                                            kind == EXPR_KIND_COT ? -1 : 1, basis);
        expr_t *leading = expr_new_const(basis[degree]);
        weight = expr_simplify_owned(leading ? expr_div(coefficients[degree], leading) : NULL);
        expr_free(leading);
        ok = weight != NULL;
    }
    for (int j = 0; ok && j <= degree; ++j) {
        expr_t *factor = expr_new_const(basis[j]), *term = factor ? expr_mul(weight, factor) : NULL;
        expr_t *difference = expr_simplify_owned(term ? expr_sub(coefficients[j], term) : NULL);
        ok = difference && expr_is_exact_zero(difference);
        expr_free(difference);
        expr_free(term);
        expr_free(factor);
    }
    int weight_degree = ok ? polynomial_degree(weight, index, 64) : -1;
    expr_t *weights[65] = {0};
    ok = weight_degree >= 0 && polynomial_coefficients(weight, index, weight_degree, weights);
    expr_t *phase = expr_new_const(hyperbolic ? num_create_from_long(-2) : num_mul_long(NUM_I, 2));
    expr_t *scaled = step && phase ? expr_mul(step, phase) : NULL, *q = scaled ? expr_exp(scaled) : NULL;
    expr_t *a = expr_const_long(kind == EXPR_KIND_TAN || kind == EXPR_KIND_TANH ? -1 : 1);
    expr_t *one = expr_const_one(), *order = expr_const_long(1 - degree);
    expr_t *power = phase ? expr_pow_long(phase, degree - 2) : NULL;
    expr_t *factor = power ? expr_mul_long(power, kind == EXPR_KIND_COT ? 4 : -4) : NULL;
    for (int j = 0; ok && j <= weight_degree; ++j) {
        if (expr_is_exact_zero(weights[j]))
            continue;
        expr_t *r = expr_const_long(j), *sum = expr_lambert_S(a, q, one, r, order, bounds->b);
        expr_t *coefficient = factor ? expr_mul(factor, weights[j]) : NULL;
        expr_t *term = coefficient && sum ? expr_mul(coefficient, sum) : NULL;
        expr_t *next = term ? result ? expr_add(result, term) : expr_retain_expr(term) : NULL;
        expr_free(result);
        result = next;
        ok = result != NULL;
        expr_free(term); expr_free(coefficient); expr_free(sum); expr_free(r);
    }
    expr_t *linked = ok && result ? expr_clone_linked_symbols(result, expression) : NULL;
    if (linked && upper_out)
        *upper_out = bounds->b;
    for (int j = 0; j < 65; ++j)
        expr_free(weights[j]);
    for (int j = 0; j < 34; ++j)
        expr_free(coefficients[j]);
    expr_free(factor); expr_free(power); expr_free(order); expr_free(one); expr_free(a);
    expr_free(q); expr_free(scaled); expr_free(phase); expr_free(result);
    expr_free(weight); expr_free(polynomial); expr_free(variable); expr_free(step);
    return expr_simplify_owned(linked);
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
    int degree = polynomial_degree(weight, index, WEIGHTED_PROGRESSION_DEGREE);
    expr_t *step = degree > 0 ? expr_simplify_extract_common_factor_quotient(function->a, index) : NULL;
    expr_t *half = step ? expr_div_long(step, 2) : NULL;
    bool circular = function && (function->ops->kind == EXPR_KIND_SIN || function->ops->kind == EXPR_KIND_COS);
    expr_t *guard = half ? circular ? expr_sin(half) : expr_sinh(half) : NULL;
    expr_t *linked = guard ? expr_clone_linked_symbols(guard, expression) : NULL;
    expr_free(guard); expr_free(half); expr_free(step); expr_free(weight);
    return linked;
}
