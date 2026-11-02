/**
 * @file diffequ_solve_riccati.c
 * @brief Lift Riccati equations to linear equations and construct analytic solution families.
 *
 * For y' = a*y^2 + b*y + c, set r = (b + a'/a)/2 and y = -(v'/v + r)/a.
 * The auxiliary equation is v'' + (a*c + r' - r^2)*v = 0. Monomial normal potentials
 * have entire hypergeometric fundamental solutions, avoiding artificial square-root singularities at zero.
 * A single value condition fixes the projective coefficients; without one, both projective charts are retained.
 * These are local solutions on intervals where a and the selected auxiliary solution do not vanish.
 */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MARS_SHARED_EQUATION_INTERNAL_ACCESS
#include "internal/equation_internal.h"
#define MARS_DIFFEQUATION_SOLVE_INTERNAL_ACCESS
#include "diffequ_solve_internal.h"
#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_notation_internal.h"

static void de_riccati_coefficients_free(expr_t **coefficients, size_t degree)
{
    if (coefficients)
        for (size_t i = 0u; i <= degree; ++i)
            expr_free(coefficients[i]);
    free(coefficients);
}

/* Polynomial coefficient visits are bounded by the equation collector's degree limit. */
static bool de_riccati_basis(const expr_t *potential, const expr_t *x, expr_t **first, expr_t **second)
{
    number_t *coefficients = NULL;
    size_t degree = 0u;
    expr_t *argument = NULL, *lower = NULL, *hypergeometric = NULL;
    bool valid = false;

    *first = NULL;
    *second = NULL;
    if (!equ_match_polynomial_alloc(potential, x, &coefficients, &degree))
        return false;
    if (degree > (size_t)(LONG_MAX - 3L) || !num_is_real(coefficients[degree]) ||
        !num_is_finite(coefficients[degree]))
        goto cleanup;
    for (size_t i = 0u; i < degree; ++i)
        if (!num_is_zero(coefficients[i]))
            goto cleanup;

    long power = (long)degree + 2L;
    expr_t *exponent = expr_const_long(power);
    expr_t *x_power = exponent ? expr_pow_xp(x, exponent) : NULL;
    expr_free(exponent);
    argument = expr_div_simplify_owned(
        expr_mul_simplify_owned(expr_negate_owned(expr_new_const(coefficients[degree])), x_power),
        expr_mul_simplify_owned(expr_const_long(power), expr_const_long(power)));
    lower = expr_div_simplify_owned(expr_const_long(power - 1L), expr_const_long(power));
    *first = argument && lower ? expr_hypergeometric_pFq(0u, NULL, 1u, (const expr_t *const *)&lower, argument) : NULL;
    expr_free(lower);
    lower = expr_div_simplify_owned(expr_const_long(power + 1L), expr_const_long(power));
    hypergeometric = argument && lower
        ? expr_hypergeometric_pFq(0u, NULL, 1u, (const expr_t *const *)&lower, argument) : NULL;
    *second = hypergeometric ? expr_mul_simplify_owned(expr_clone(x), hypergeometric) : NULL;
    hypergeometric = NULL;
    valid = *first && *second;

cleanup:
    expr_free(hypergeometric);
    expr_free(lower);
    expr_free(argument);
    for (size_t i = 0u; i <= degree; ++i)
        num_destroy(&coefficients[i]);
    free(coefficients);
    if (!valid) {
        expr_free(*second); expr_free(*first);
        *first = NULL; *second = NULL;
    }
    return valid;
}

static expr_t *de_riccati_at(const expr_t *expression, const expr_t *x, const expr_t *point)
{
    expr_t *substituted = expression ? expr_substitute(expression, x, point) : NULL;
    expr_t *simplified = substituted ? expr_simplify(substituted) : NULL;
    expr_free(substituted);
    return simplified;
}

static expr_t *de_riccati_constant(const diffequ_t *de, const expr_t *x, const expr_t *y)
{
    char name[64] = "C";
    const char *x_name = expr_symbol_name(x), *y_name = expr_symbol_name(y);
    for (size_t suffix = 1u; de_constant(de, name) || (x_name && !strcmp(x_name, name)) ||
                           (y_name && !strcmp(y_name, name)); ++suffix)
        snprintf(name, sizeof name, "C_%zu", suffix);
    return expr_new_named_const(NUM_NAN, name);
}

/* Initial data must lie at an ordinary real point, before any projective coefficients are claimed. */
static bool de_riccati_regular_initial(const expr_t *x, const expr_t *point, const expr_t *value,
                                      expr_t *const *coefficients, const expr_t *shift)
{
    const struct {
        const expr_t *expression;
        bool at_point;
        bool nonzero;
    } inputs[] = {
        {point,           false, false},
        {value,           false, false},
        {coefficients[0], true,  false},
        {coefficients[1], true,  false},
        {coefficients[2], true,  true},
        {shift,           true,  false}
    };
    /* Exactly six scalar checks; none depends on an unbounded symbol catalogue. */
    for (size_t i = 0u; i < sizeof inputs / sizeof *inputs; ++i) {
        expr_t *at = inputs[i].at_point ? de_riccati_at(inputs[i].expression, x, point)
                                       : expr_clone(inputs[i].expression);
        number_t number = at ? expr_eval(at) : num_clone(NUM_NAN);
        bool valid = num_is_real(number) && num_is_finite(number) && (!inputs[i].nonzero || !num_is_zero(number));
        num_destroy(&number);
        expr_free(at);
        if (!valid)
            return false;
    }
    return true;
}

/* The two entire solutions have Wronskian one, so no singular basis inversion is needed. */
static expr_t *de_riccati_initial_combination(const expr_t *x, const expr_t *a, const expr_t *shift,
                                             const expr_t *first, const expr_t *second,
                                             const expr_t *point, const expr_t *value)
{
    expr_t *a_at = de_riccati_at(a, x, point);
    expr_t *shift_at = de_riccati_at(shift, x, point);
    if (expr_is_exact_zero(point)) {
        expr_t *slope = expr_negate_owned(expr_add_simplify_owned(
            expr_mul_simplify_owned(a_at, expr_clone(value)), shift_at));
        return slope ? expr_add_simplify_owned(expr_clone(first), expr_mul_simplify_owned(slope, expr_clone(second)))
                     : NULL;
    }
    expr_t *first_at = de_riccati_at(first, x, point), *second_at = de_riccati_at(second, x, point);
    expr_t *first_derivative = expr_create_deriv(first, x), *second_derivative = expr_create_deriv(second, x);
    expr_t *first_slope = de_riccati_at(first_derivative, x, point);
    expr_t *second_slope = de_riccati_at(second_derivative, x, point);
    expr_t *slope = expr_negate_owned(expr_add_simplify_owned(expr_mul_simplify_owned(a_at, expr_clone(value)),
                                                             shift_at));
    expr_t *first_weight = slope && second_at && second_slope
        ? expr_sub_simplify_owned(expr_clone(second_slope), expr_mul(slope, second_at)) : NULL;
    expr_t *second_weight = slope && first_at && first_slope
        ? expr_sub_simplify_owned(expr_mul(slope, first_at), expr_clone(first_slope)) : NULL;
    expr_t *combination = first_weight && second_weight
        ? expr_add_simplify_owned(expr_mul(first_weight, first), expr_mul(second_weight, second)) : NULL;

    expr_free(second_weight); expr_free(first_weight);
    expr_free(slope);
    expr_free(second_slope); expr_free(first_slope);
    expr_free(second_derivative); expr_free(first_derivative);
    expr_free(second_at); expr_free(first_at);
    return combination;
}

static equation_t *de_riccati_solution(const expr_t *x, const expr_t *y, const expr_t *a,
                                      const expr_t *shift, const expr_t *auxiliary)
{
    expr_t *derivative = expr_create_deriv(auxiliary, x);
    expr_t *ratio = derivative ? expr_div_simplify_owned(derivative, expr_clone(auxiliary)) : NULL;
    expr_t *right = ratio ? expr_div_simplify_owned(
        expr_negate_owned(expr_add_simplify_owned(ratio, expr_clone(shift))), expr_clone(a)) : NULL;
    equation_t *solution = right ? equ_new(y, right) : NULL;
    expr_free(right);
    return solution;
}

/* Recognise the quadratic field, retaining an honest linear reduction when its potential is unsupported. */
diffequ_solve_result_t *de_solve_riccati(const diffequ_t *de, const expr_t *x, const expr_t *y,
                                       const expr_t *right, bool include_steps)
{
    expr_t **coefficients = NULL;
    size_t degree = 0u;
    expr_t *shift = NULL, *potential = NULL, *first = NULL, *second = NULL, *combination = NULL;
    expr_t *constant = NULL;
    de_riccati_bessel_t *bessel = NULL;
    diffequ_solve_result_t *result = NULL;
    const expr_t *point = NULL, *value = NULL;

    if (!equ_collect_symbolic_polynomial_alloc(right, y, &coefficients, &degree)) {
        de_riccati_coefficients_free(coefficients, degree);
        coefficients = calloc(3u, sizeof *coefficients);
        degree = 2u;
        expr_t *square = expr_pow(y, &NUM_TWO), *remainder = NULL;
        bool matched = coefficients && square &&
                       de_linear_decompose(right, square, &coefficients[2], &remainder) &&
                       de_linear_decompose(remainder, y, &coefficients[1], &coefficients[0]);
        expr_free(remainder); expr_free(square);
        if (!matched)
            goto cleanup;
    }
    if (degree != 2u || expr_is_exact_zero(coefficients[2]))
        goto cleanup;
    const expr_t *a = coefficients[2], *b = coefficients[1], *c = coefficients[0];
    if (de_expr_uses(a, y) || de_expr_uses(b, y) || de_expr_uses(c, y))
        goto cleanup;
    expr_t *a_derivative = expr_create_deriv(a, x);
    shift = a_derivative ? expr_div_simplify_owned(
        expr_add_simplify_owned(expr_clone(b), expr_div_simplify_owned(a_derivative, expr_clone(a))),
        expr_const_long(2L)) : NULL;
    potential = shift ? expr_sub_simplify_owned(
        expr_add_simplify_owned(expr_mul(a, c), expr_create_deriv(shift, x)), expr_mul(shift, shift)) : NULL;
    if (!potential)
        goto cleanup;

    bool conditioned = de_find_initial_condition(de, y, &point, &value);
    bool conditions_valid = de->condition_count == (size_t)conditioned &&
                            (!conditioned || (!de_expr_uses(point, x) && !de_expr_uses(value, x) &&
                                               !de_expr_uses(value, y)));
    if (conditioned && conditions_valid)
        conditions_valid = de_riccati_regular_initial(x, point, value, coefficients, shift);
    bool solved = conditions_valid && de_riccati_basis(potential, x, &first, &second);
    if (solved)
        bessel = de_riccati_bessel_new(potential, x);
    result = de_solve_result_new(solved ? DE_SOLVE_STATUS_SOLVED : DE_SOLVE_STATUS_UNSUPPORTED, DE_SOLVER_RICCATI,
        solved ? "solved by Riccati linearisation; auxiliary denominators must be non-zero"
               : conditions_valid ? "Riccati linearisation found; the auxiliary linear equation is not yet supported"
                                  : "Riccati linearisation found; the supplied conditions are not supported");
    if (!result || de_solve_result_set_symmetry(result, "SL(2, ℝ) Lie system") != 0)
        goto fail;
    if (solved) {
        if (conditioned)
            combination = de_riccati_initial_combination(x, a, shift, first, second, point, value);
        else {
            constant = de_riccati_constant(de, x, y);
            expr_t *coefficient = bessel
                ? expr_mul_simplify_owned(expr_clone(de_riccati_bessel_scale(bessel)), expr_clone(constant))
                : expr_clone(constant);
            combination = expr_add_simplify_owned(expr_clone(first),
                                                   expr_mul_simplify_owned(coefficient, expr_clone(second)));
        }
        if (!combination)
            goto fail;
        equation_t *solution = de_riccati_solution(x, y, a, shift, combination);
        if (!solution || (bessel && !de_riccati_bessel_present(bessel, solution, a, shift, constant, point, value)) ||
            de_solve_result_append(result, solution) != 0) {
            equ_free(solution);
            goto fail;
        }
        if (!conditioned) {
            solution = de_riccati_solution(x, y, a, shift, second);
            if (!solution || (bessel && !de_riccati_bessel_present(bessel, solution, a, shift, NULL, NULL, NULL)) ||
                de_solve_result_append(result, solution) != 0) {
                equ_free(solution);
                goto fail;
            }
        }
    }
    const expr_t *shown_first = first, *shown_second = second;
    if (bessel)
        de_riccati_bessel_basis(bessel, &shown_first, &shown_second);
    if (include_steps && !de_riccati_steps(result, x, y, a, shift, potential, shown_first, shown_second, bessel != NULL))
        goto fail;
    goto cleanup;

fail:
    de_solve_result_free(result);
    result = NULL;
cleanup:
    de_riccati_bessel_free(bessel);
    expr_free(constant);
    expr_free(combination);
    expr_free(second); expr_free(first);
    expr_free(potential); expr_free(shift);
    de_riccati_coefficients_free(coefficients, degree);
    return result;
}
