/**
 * @file diffequ_solve_emden.c
 * @brief Modified-Emden linearisation and initial-value solutions.
 *
 * Recognises the scaled logarithmic-derivative image of a quadratic auxiliary
 * function. Builds exact rational solutions and native derivation text, with
 * value and slope conditions imposed at a shared ordinary point. This private
 * solver strategy is dispatched by the differential-equation module.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MARS_DIFFEQUATION_SOLVE_INTERNAL_ACCESS
#include "diffequ_solve_internal.h"
#define MARS_SHARED_EQUATION_INTERNAL_ACCESS
#include "internal/equation_internal.h"

char *de_emden_symbolic_text(const expr_t *expression)
{
    const char *name = expr_symbol_name(expression);
    expr_t *symbol = name && (expr_is_variable(expression) || expr_is_named_const(expression))
                         ? expr_new_named_var(NUM_NAN, name) : NULL;
    char *text = expr_to_string(symbol ? symbol : expression, style_UNBOUND);
    expr_free(symbol);
    return text;
}

/* Describe the native logarithmic-derivative substitution without browser rewriting. */

/*
 * The modified Emden equation
 *
 *     y'' + 3 y y' + y^3 = 0
 *
 * is the logarithmic-derivative image of u''' = 0: setting y = u'/u
 * makes the left hand side u'''/u.  Thus u is quadratic and y is its
 * logarithmic derivative.  The two essential ratios among the three
 * coefficients of u are represented below by C₁ and C₂.
 */
static bool de_emden_initial_quotient(const expr_t *independent, const expr_t *scale, const expr_t *point,
                                      const expr_t *value, const expr_t *slope, expr_t **numerator_out,
                                      expr_t **denominator_out)
{
    expr_t *a = value ? expr_clone(value) : expr_new_named_const(NUM_NAN, "C1");
    expr_t *v = slope ? expr_clone(slope) : expr_new_named_const(NUM_NAN, "C2");
    expr_t *t = expr_sub_simplify_owned(expr_clone(independent), expr_clone(point));
    expr_t *b = expr_add_simplify_owned(v, expr_mul_simplify_owned(expr_clone(scale), expr_pow_long(a, 2)));
    *numerator_out = expr_add_simplify_owned(expr_clone(a), expr_mul_simplify_owned(expr_clone(b), expr_clone(t)));
    expr_t *linear = expr_mul_simplify_owned(expr_clone(scale), expr_mul_simplify_owned(expr_clone(a), expr_clone(t)));
    expr_t *quadratic = expr_div_simplify_owned(
        expr_mul_simplify_owned(expr_clone(scale), expr_mul_simplify_owned(expr_clone(b), expr_pow_long(t, 2))),
        expr_const_long(2));
    *denominator_out = expr_add_simplify_owned(expr_add_simplify_owned(expr_const_one(), linear), quadratic);
    expr_free(b);
    expr_free(t);
    expr_free(a);
    return *numerator_out && *denominator_out;
}

/* Solve the recognised family, retaining an arbitrary datum when only one is supplied. */
de_attempt_t de_attempt_modified_emden_linearization(const diffequ_t *de, const expr_t *independent,
                                                            const expr_t *dependent, const expr_t *first_derivative,
                                                            const expr_t *second_derivative, const expr_t *residual,
                                                            equation_t **solutions_out, size_t *solution_count_out,
                                                            expr_t **scale_out)
{
    expr_t *leading = NULL;
    expr_t *remainder = NULL;
    expr_t *mixed_monomial = NULL;
    expr_t *mixed_coefficient = NULL;
    expr_t *after_mixed = NULL;
    expr_t *cubic_coefficient = NULL;
    expr_t *constant_remainder = NULL;
    expr_t *three = NULL;
    expr_t *y_cubed = NULL;
    expr_t *three_leading = NULL;
    expr_t *scale = NULL;
    expr_t *scale_squared = NULL;
    expr_t *expected_cubic = NULL;
    expr_t *coefficient_difference = NULL;
    expr_t *constant_1 = NULL;
    expr_t *constant_2 = NULL;
    expr_t *two = NULL;
    expr_t *two_x = NULL;
    expr_t *numerator = NULL;
    expr_t *x_squared = NULL;
    expr_t *constant_1_x = NULL;
    expr_t *denominator_sum = NULL;
    expr_t *denominator = NULL;
    expr_t *scaled_denominator = NULL;
    expr_t *solution_denominator = NULL;
    expr_t *right = NULL;
    const expr_t *value_point = NULL, *slope_point = NULL, *initial_value = NULL, *initial_slope = NULL;
    de_attempt_t attempt = DE_ATTEMPT_NOT_MATCHED;

    if (!de || !solutions_out || !solution_count_out || !scale_out || !first_derivative ||
        !de_linear_decompose(residual, second_derivative, &leading, &remainder))
        goto cleanup;
    *solution_count_out = 0u;
    *scale_out = NULL;
    bool has_value = de_find_initial_condition(de, dependent, &value_point, &initial_value);
    bool has_slope = de_find_derivative_condition(de, dependent, independent, 1, &slope_point, &initial_slope);
    if (de->condition_count != (size_t)has_value + (size_t)has_slope)
        goto cleanup;
    if (has_value && has_slope) {
        expr_t *difference = expr_sub_simplify_owned(expr_clone(value_point), expr_clone(slope_point));
        bool same_point = difference && expr_is_exact_zero(difference);
        expr_free(difference);
        if (!same_point)
            goto cleanup;
    }

    mixed_monomial = expr_mul(dependent, first_derivative);
    y_cubed = expr_pow_long(dependent, 3L);
    if (!mixed_monomial || !y_cubed ||
        !de_linear_decompose(remainder, mixed_monomial, &mixed_coefficient, &after_mixed) ||
        !de_linear_decompose(after_mixed, y_cubed, &cubic_coefficient, &constant_remainder) ||
        !expr_is_exact_zero(constant_remainder) || expr_is_exact_zero(leading))
        goto cleanup;

    three = expr_const_long(3L);
    three_leading = three ? expr_mul_simplify_owned(expr_clone(three), expr_clone(leading)) : NULL;
    scale = three_leading ? expr_div_simplify_owned(expr_clone(mixed_coefficient), three_leading) : NULL;
    three_leading = NULL;
    scale_squared = scale ? expr_mul(scale, scale) : NULL;
    expected_cubic = scale_squared ? expr_mul_simplify_owned(scale_squared, expr_clone(leading)) : NULL;
    scale_squared = NULL;
    coefficient_difference =
        expected_cubic ? expr_sub_simplify_owned(expr_clone(cubic_coefficient), expected_cubic) : NULL;
    expected_cubic = NULL;
    if (!scale || expr_is_exact_zero(scale) || de_expr_uses(scale, independent) || de_expr_uses(scale, dependent) ||
        !coefficient_difference || !expr_is_exact_zero(coefficient_difference))
        goto cleanup;

    attempt = DE_ATTEMPT_FAILED;
    if (de->condition_count) {
        if (!de_emden_initial_quotient(independent, scale, has_value ? value_point : slope_point,
                                       initial_value, initial_slope, &numerator, &solution_denominator))
            goto cleanup;
    } else {
        constant_1 = expr_new_named_const(NUM_NAN, "C1");
        constant_2 = expr_new_named_const(NUM_NAN, "C2");
        two = expr_const_long(2L);
        two_x = two ? expr_mul(two, independent) : NULL;
        numerator = two_x ? expr_add(two_x, constant_1) : NULL;
        x_squared = expr_mul(independent, independent);
        constant_1_x = constant_1 ? expr_mul(constant_1, independent) : NULL;
        denominator_sum = x_squared && constant_1_x ? expr_add(x_squared, constant_1_x) : NULL;
        denominator = denominator_sum && constant_2 ? expr_add(denominator_sum, constant_2) : NULL;
        scaled_denominator = denominator ? expr_mul_simplify_owned(expr_clone(scale), denominator) : NULL;
        denominator = NULL;
        solution_denominator = scaled_denominator ? expr_clone(scaled_denominator) : NULL;
    }
    right = numerator && solution_denominator ? expr_div(numerator, solution_denominator) : NULL;
    solutions_out[0] = right ? equ_new(dependent, right) : NULL;
    if (solutions_out[0]) {
        /* Preserve the quotient presentation even when simplification stores a reciprocal product. */
        char *numerator_text = de_emden_symbolic_text(numerator);
        char *denominator_text = de_emden_symbolic_text(solution_denominator);
        string_t *quotient = string_new();
        bool displayed = numerator_text && denominator_text && quotient &&
                         string_append_format(quotient, "(%s)/(%s)", numerator_text, denominator_text) >= 0 &&
                         equ_set_display_unbound(solutions_out[0], NULL, quotient) == 0;
        string_free(quotient);
        free(denominator_text);
        free(numerator_text);
        if (!displayed)
            goto cleanup;
        *solution_count_out = 1u;
        *scale_out = expr_clone(scale);
        attempt = DE_ATTEMPT_SOLVED;
    }

cleanup:
    if (attempt != DE_ATTEMPT_SOLVED) {
        equ_free(solutions_out ? solutions_out[0] : NULL);
        if (solutions_out)
            solutions_out[0] = NULL;
    }
    expr_free(right);
    expr_free(solution_denominator);
    expr_free(scaled_denominator);
    expr_free(denominator);
    expr_free(denominator_sum);
    expr_free(constant_1_x);
    expr_free(x_squared);
    expr_free(numerator);
    expr_free(two_x);
    expr_free(two);
    expr_free(constant_2);
    expr_free(constant_1);
    expr_free(coefficient_difference);
    expr_free(expected_cubic);
    expr_free(scale_squared);
    expr_free(scale);
    expr_free(three_leading);
    expr_free(y_cubed);
    expr_free(three);
    expr_free(constant_remainder);
    expr_free(cubic_coefficient);
    expr_free(after_mixed);
    expr_free(mixed_coefficient);
    expr_free(mixed_monomial);
    expr_free(remainder);
    expr_free(leading);
    return attempt;
}
