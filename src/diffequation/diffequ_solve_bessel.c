/**
 * @file diffequ_solve_bessel.c
 * @brief Recognise scaled Bessel equations and construct real solution bases.
 *
 * Normalises homogeneous second-order coefficient ratios x*B/A and x^2*C/A
 * independently of variable names, term order and a common multiplier.
 * Finite real coefficients give ordinary or modified Bessel bases, elementary
 * half-order bases, or the Euler limit. Absolute coordinates describe real local
 * branches on either side of the singular origin. Point conditions must lie in
 * one regular half-line and are imposed by the shared native linear-basis solver.
 * This private strategy is called by the Sturm--Liouville solver; it never changes
 * the authored problem or implements mathematical rewriting in a Lab client.
 */
#include <stdlib.h>

#include "number-t.h"
#define MARS_SHARED_EQUATION_INTERNAL_ACCESS
#include "internal/equation_internal.h"
#define MARS_DIFFEQUATION_SOLVE_INTERNAL_ACCESS
#include "diffequ_solve_internal.h"

static expr_t *bessel_power(const expr_t *coordinate, const expr_t *power)
{
    expr_t *result = power ? expr_pow_xp(coordinate, power) : NULL;
    return result ? expr_simplify_owned(result) : NULL;
}

static expr_t *bessel_root(const expr_t *value)
{
    return de_simplify_unary_owned(expr_clone(value), expr_sqrt);
}

static expr_t *bessel_normalise(const expr_t *coefficient, const expr_t *factor, const expr_t *leading)
{
    expr_t *numerator = expr_mul(coefficient, factor);
    expr_t *quotient = numerator ? expr_simplify_extract_common_factor_quotient(numerator, leading) : NULL;
    if (!quotient && numerator)
        quotient = expr_div_simplify_owned(expr_clone(numerator), expr_clone(leading));
    expr_free(numerator);
    expr_t *expanded = quotient ? expr_expand_products_internal(quotient) : NULL;
    expr_free(quotient);
    return expanded ? expr_simplify_owned(expanded) : NULL;
}

static bool bessel_regular_conditions(const diffequ_t *de, const expr_t *coordinate, const expr_t *leading)
{
    int side = 0;
    for (size_t i = 0u; i < de->condition_count; ++i) {
        if (de->condition_point_counts[i] != 1u)
            return false;
        const expr_t *point = de->condition_points[i][0];
        number_t value = expr_eval(point);
        if (!num_is_finite(value) || !num_is_real(value) || num_is_zero(value))
            return false;
        int sign = num_get_sign(value);
        if (side && sign != side)
            return false;
        side = sign;
        expr_t *at = expr_substitute(leading, coordinate, point);
        number_t coefficient = at ? expr_eval(at) : NUM_NAN;
        expr_free(at);
        if (!num_is_finite(coefficient) || num_is_zero(coefficient))
            return false;
    }
    return true;
}

static expr_t *bessel_restrict(const diffequ_t *de, const expr_t *body, const expr_t *coordinate,
                               const expr_t *leading)
{
    expr_t *zero = expr_const_zero(), *real = expr_real_parameter(coordinate);
    expr_t *absolute = expr_abs(coordinate), *regular = expr_abs(leading), *side = NULL;
    expr_t *square = expr_pow_long(coordinate, 2L);
    expr_t *factor = square ? expr_simplify_extract_common_factor_quotient(leading, square) : NULL;
    number_t value = factor ? expr_eval(factor) : num_clone(NUM_NAN);
    bool monomial = factor && !de_expr_uses(factor, coordinate) && num_is_finite(value) && !num_is_zero(value);
    if (de->condition_count) {
        number_t point = expr_eval(de->condition_points[0][0]);
        side = num_get_sign(point) > 0 ? expr_clone(coordinate) : expr_neg(coordinate);
        num_destroy(&point);
    }
    expr_t *arguments[9] = {(expr_t *)body, real, zero, absolute, zero};
    size_t count = 5u;
    if (!monomial) {
        arguments[count++] = regular;
        arguments[count++] = zero;
    }
    if (side) {
        arguments[count++] = side;
        arguments[count++] = zero;
    }
    expr_t *result = zero && real && absolute && regular && (!de->condition_count || side) ?
        expr_real_domain_from_args(count, arguments) : NULL;
    num_destroy(&value); expr_free(factor); expr_free(square);
    expr_free(side); expr_free(regular); expr_free(absolute); expr_free(real); expr_free(zero);
    return result;
}

/* Build the native basis once, preserving any unconstrained integration constants. */
expr_t *de_bessel_solution(const diffequ_t *de, const expr_t *coordinate, const expr_t *dependent,
                            const expr_t *leading, const expr_t *first, const expr_t *potential,
                            const expr_t *forcing)
{
    NUM_SCOPE(scope);
    expr_t *normal_first = NULL, *normal_potential = NULL, *square = NULL, *absolute = NULL;
    expr_t *envelope = NULL, *scale = NULL, *argument = NULL, *order = NULL;
    expr_t *basis[2] = {NULL, NULL}, *weighted[2] = {NULL, NULL};
    expr_t *combination = NULL, *solution = NULL, *zero = NULL;
    expr_t *wave_expression = NULL, *shift_expression = NULL, *alpha = NULL, *order_squared_expression = NULL;
    expr_t *half_power = NULL, *quarter_difference = NULL;
    expr_t **coefficients = NULL;
    size_t degree = 0u;
    if (!scope || !de || !coordinate || !dependent || !leading || !first || !potential || !forcing ||
        (!de_expr_uses(leading, coordinate) && !de_expr_uses(first, coordinate) && !de_expr_uses(potential, coordinate)) ||
        !expr_is_exact_zero(forcing))
        goto cleanup;
    normal_first = bessel_normalise(first, coordinate, leading);
    square = expr_pow_long(coordinate, 2L);
    normal_potential = square ? bessel_normalise(potential, square, leading) : NULL;
    if (!normal_first || de_expr_uses(normal_first, coordinate) || !normal_potential ||
        !equ_collect_symbolic_polynomial_alloc(normal_potential, coordinate, &coefficients, &degree) || degree > 2u ||
        (degree >= 1u && !expr_is_exact_zero(coefficients[1])))
        goto cleanup;
    wave_expression = degree == 2u ? expr_clone(coefficients[2]) : expr_const_zero();
    shift_expression = expr_clone(coefficients[0]);
    if (!wave_expression || !shift_expression)
        goto cleanup;
    number_t p = expr_eval(normal_first), q = expr_eval(shift_expression), wave = expr_eval(wave_expression);
    if (!num_is_finite(p) || !num_is_real(p) || !num_is_finite(q) || !num_is_real(q) ||
        !num_is_finite(wave) || !num_is_real(wave) || !bessel_regular_conditions(de, coordinate, leading))
        goto cleanup;
    alpha = expr_div_simplify_owned(expr_sub_simplify_owned(expr_const_one(), expr_clone(normal_first)),
                                   expr_const_long(2L));
    order_squared_expression = alpha ? expr_sub_simplify_owned(expr_mul(alpha, alpha), expr_clone(shift_expression)) : NULL;
    half_power = expr_div_simplify_owned(de_simplify_unary_owned(expr_clone(normal_first), expr_neg), expr_const_long(2L));
    quarter_difference = order_squared_expression ? expr_sub_simplify_owned(expr_clone(order_squared_expression),
        expr_new_const(num_create_from_frac(1, 4))) : NULL;
    number_t order_squared = order_squared_expression ? expr_eval(order_squared_expression) : NUM_NAN;
    bool euler = num_is_zero(wave), modified = num_get_sign(wave) < 0;
    bool half_order = quarter_difference && expr_is_exact_zero(quarter_difference);
    if (!alpha || !half_power || !num_is_finite(order_squared) || !num_is_real(order_squared) ||
        (!euler && num_get_sign(order_squared) < 0))
        goto cleanup;
    absolute = de_simplify_unary_owned(expr_clone(coordinate), expr_abs);
    if (!absolute)
        goto cleanup;
    if (euler) {
        if (num_get_sign(order_squared) > 0) {
            order = bessel_root(order_squared_expression);
            expr_t *upper = order ? expr_add(alpha, order) : NULL;
            expr_t *lower = order ? expr_sub(alpha, order) : NULL;
            basis[0] = upper ? expr_pow_xp(absolute, upper) : NULL;
            basis[1] = lower ? expr_pow_xp(absolute, lower) : NULL;
            expr_free(lower); expr_free(upper);
            envelope = expr_const_one();
        } else {
            envelope = bessel_power(absolute, alpha);
            argument = expr_ln(absolute);
            if (num_is_zero(order_squared)) {
                basis[0] = expr_const_one();
                basis[1] = expr_clone(argument);
            } else {
                expr_t *positive_order = expr_neg(order_squared_expression);
                scale = positive_order ? bessel_root(positive_order) : NULL;
                expr_free(positive_order);
                expr_t *phase = argument && scale ? expr_mul_simplify_owned(expr_clone(argument), expr_clone(scale)) : NULL;
                basis[0] = phase ? expr_cos(phase) : NULL;
                basis[1] = phase ? expr_sin(phase) : NULL;
                expr_free(phase);
            }
        }
    } else {
        expr_t *positive_wave = modified ? expr_neg(wave_expression) : expr_clone(wave_expression);
        scale = positive_wave ? bessel_root(positive_wave) : NULL;
        expr_free(positive_wave);
        argument = scale ? expr_mul_simplify_owned(expr_clone(scale), expr_clone(half_order ? coordinate : absolute)) : NULL;
        envelope = bessel_power(absolute, half_order ? half_power : alpha);
        if (argument && half_order) {
            basis[0] = modified ? expr_cosh(argument) : expr_cos(argument);
            basis[1] = modified ? expr_sinh(argument) : expr_sin(argument);
        } else if (argument) {
            order = bessel_root(order_squared_expression);
            basis[0] = order ? (modified ? expr_bessel_I(order, argument) : expr_bessel_J(order, argument)) : NULL;
            basis[1] = order ? (modified ? expr_bessel_K(order, argument) : expr_bessel_Y(order, argument)) : NULL;
        }
    }
    if (!basis[0] || !basis[1] || !envelope)
        goto cleanup;
    if (de->condition_count) {
        weighted[0] = expr_mul_simplify_owned(expr_clone(envelope), expr_clone(basis[0]));
        weighted[1] = expr_mul_simplify_owned(expr_clone(envelope), expr_clone(basis[1]));
        zero = expr_const_zero();
        if (weighted[0] && weighted[1] && zero)
            solution = de_apply_basis_conditions(de, coordinate, dependent, weighted, 2u, zero);
    } else {
        expr_t *constant_one = expr_new_named_const(NUM_NAN, "C1"), *constant_two = expr_new_named_const(NUM_NAN, "C2");
        expr_t *first_term = constant_one ? expr_mul(constant_one, basis[0]) : NULL;
        expr_t *second_term = constant_two ? expr_mul(constant_two, basis[1]) : NULL;
        combination = first_term && second_term ? expr_add(first_term, second_term) : NULL;
        solution = combination ? expr_mul_simplify_owned(expr_clone(envelope), expr_clone(combination)) : NULL;
        expr_free(second_term); expr_free(first_term); expr_free(constant_two); expr_free(constant_one);
    }
    if (solution) {
        expr_t *restricted = bessel_restrict(de, solution, coordinate, leading);
        expr_free(solution);
        solution = restricted;
    }
cleanup:
    for (size_t i = 0u; coefficients && i <= degree; ++i)
        expr_free(coefficients[i]);
    free(coefficients);
    expr_free(quarter_difference); expr_free(half_power); expr_free(order_squared_expression); expr_free(alpha);
    expr_free(shift_expression); expr_free(wave_expression);
    expr_free(zero); expr_free(combination);
    expr_free(weighted[1]); expr_free(weighted[0]); expr_free(basis[1]); expr_free(basis[0]);
    expr_free(order); expr_free(argument); expr_free(scale); expr_free(envelope); expr_free(absolute);
    expr_free(normal_potential); expr_free(square); expr_free(normal_first);
    return solution;
}
