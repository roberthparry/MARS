#include <stdlib.h>

#define MARS_SHARED_EQUATION_INTERNAL_ACCESS
#include "internal/equation_internal.h"
#define MARS_DIFFEQUATION_SOLVE_INTERNAL_ACCESS
#include "diffequ_solve_internal.h"

typedef struct {
    expr_t *leading;
    expr_t *first;
    expr_t *dependent;
    expr_t *forcing;
} de_linear_second_order_t;

static void de_linear_second_order_clear(de_linear_second_order_t *form)
{
    expr_free(form->forcing);
    expr_free(form->dependent);
    expr_free(form->first);
    expr_free(form->leading);
}

static bool de_decompose_second_order(
    const expr_t *residual,
    const expr_t *second_derivative,
    const expr_t *first_derivative,
    const expr_t *dependent,
    de_linear_second_order_t *form)
{
    expr_t *after_second = NULL;
    expr_t *after_first = NULL;
    expr_t *constant = NULL;

    if (!de_linear_decompose(
            residual,
            second_derivative,
            &form->leading,
            &after_second))
        goto fail;

    if (first_derivative) {
        if (!de_linear_decompose(
                after_second,
                first_derivative,
                &form->first,
                &after_first))
            goto fail;
    } else {
        form->first = expr_const_zero();
        after_first = expr_clone(after_second);
        if (!form->first || !after_first)
            goto fail;
    }

    if (!de_linear_decompose(
            after_first,
            dependent,
            &form->dependent,
            &constant))
        goto fail;

    form->forcing = de_simplify_unary_owned(constant, expr_neg);
    constant = NULL;
    expr_free(after_first);
    expr_free(after_second);
    return form->leading &&
           form->first &&
           form->dependent &&
           form->forcing;

fail:
    expr_free(constant);
    expr_free(after_first);
    expr_free(after_second);
    de_linear_second_order_clear(form);
    return false;
}

static bool de_build_self_adjoint_form(
    const expr_t *independent,
    const de_linear_second_order_t *form)
{
    expr_t *leading_derivative = NULL;
    expr_t *multiplier_numerator = NULL;
    expr_t *log_multiplier_derivative = NULL;
    expr_t *log_multiplier = NULL;
    expr_t *multiplier = NULL;
    expr_t *p = NULL;
    expr_t *q = NULL;
    expr_t *weighted_forcing = NULL;
    bool ok = false;

    leading_derivative =
        expr_create_deriv(form->leading, independent);
    multiplier_numerator = leading_derivative
        ? expr_sub_simplify_owned(
              expr_clone(form->first), leading_derivative)
        : NULL;
    if (leading_derivative)
        leading_derivative = NULL;
    log_multiplier_derivative = multiplier_numerator
        ? expr_div_simplify_owned(
              multiplier_numerator, expr_clone(form->leading))
        : NULL;
    if (multiplier_numerator)
        multiplier_numerator = NULL;
    log_multiplier = de_integrate_or_formal(
        log_multiplier_derivative, independent);
    multiplier = log_multiplier ? expr_exp(log_multiplier) : NULL;
    p = multiplier
        ? expr_mul_simplify_owned(
              expr_clone(multiplier), expr_clone(form->leading))
        : NULL;
    q = multiplier
        ? expr_mul_simplify_owned(
              expr_clone(multiplier),
              de_simplify_unary_owned(
                  expr_clone(form->dependent), expr_neg))
        : NULL;
    weighted_forcing = multiplier
        ? expr_mul_simplify_owned(
              expr_clone(multiplier), expr_clone(form->forcing))
        : NULL;
    ok = p && q && weighted_forcing;

    expr_free(weighted_forcing);
    expr_free(q);
    expr_free(p);
    expr_free(multiplier);
    expr_free(log_multiplier);
    expr_free(log_multiplier_derivative);
    expr_free(multiplier_numerator);
    expr_free(leading_derivative);
    return ok;
}

static expr_t *de_named_constant(const char *name)
{
    return expr_new_named_const(NUM_NAN, name);
}

static expr_t *de_sturm_series_coefficient(const expr_t *index)
{
    return expr_new_indexed_symbol("c", index);
}

static equation_t *de_sturm_series_coefficient_value(
    long index,
    const expr_t *value)
{
    expr_t *index_expr = expr_const_long(index);
    expr_t *coefficient = index_expr
        ? de_sturm_series_coefficient(index_expr)
        : NULL;
    equation_t *equation = coefficient && value
        ? equ_new(coefficient, value)
        : NULL;

    expr_free(coefficient);
    expr_free(index_expr);
    return equation;
}

int de_sturm_liouville_cubic_basis(
    const expr_t *independent,
    const expr_t *parameter,
    const expr_t *dependent,
    equation_t **solutions_out,
    size_t *solution_count_out)
{
    expr_t *index = expr_new_named_var(NUM_NAN, "n");
    expr_t *coefficient = index
        ? de_sturm_series_coefficient(index)
        : NULL;
    expr_t *power = index
        ? expr_pow_xp(independent, index)
        : NULL;
    expr_t *term = coefficient && power
        ? expr_mul_simplify_owned(coefficient, power)
        : NULL;
    expr_t *series = term && index
        ? expr_new_summation(term, index)
        : NULL;
    expr_t *next_index = index
        ? expr_add_long(index, 2L)
        : NULL;
    expr_t *previous_index = index
        ? expr_add_long(index, -3L)
        : NULL;
    expr_t *next_coefficient = next_index
        ? de_sturm_series_coefficient(next_index)
        : NULL;
    expr_t *current_coefficient = index
        ? de_sturm_series_coefficient(index)
        : NULL;
    expr_t *previous_coefficient = previous_index
        ? de_sturm_series_coefficient(previous_index)
        : NULL;
    expr_t *parameter_term = parameter && current_coefficient
        ? expr_mul(parameter, current_coefficient)
        : NULL;
    expr_t *numerator = parameter_term && previous_coefficient
        ? expr_add(parameter_term, previous_coefficient)
        : NULL;
    expr_t *index_plus_2 = index ? expr_add_long(index, 2L) : NULL;
    expr_t *index_plus_1 = index ? expr_add_long(index, 1L) : NULL;
    expr_t *index_product = index_plus_2 && index_plus_1
        ? expr_mul(index_plus_2, index_plus_1)
        : NULL;
    expr_t *two = expr_const_long(2L);
    expr_t *denominator = two && index_product
        ? expr_mul(two, index_product)
        : NULL;
    expr_t *recurrence = numerator && denominator
        ? expr_div(numerator, denominator)
        : NULL;
    expr_t *constant_2 = de_named_constant("C₂");
    expr_t *constant_3 = de_named_constant("C₃");
    expr_t *zero = expr_new_const(NUM_ZERO);
    int ok = 0;

    if (term) {
        coefficient = NULL;
        power = NULL;
    }
    if (!independent || !parameter || !dependent || !solutions_out ||
        !solution_count_out || !series || !next_coefficient ||
        !recurrence || !constant_2 || !constant_3 || !zero)
        goto cleanup;
    *solution_count_out = 0u;

    solutions_out[0] = equ_new(dependent, series);
    solutions_out[1] =
        de_sturm_series_coefficient_value(0L, constant_2);
    solutions_out[2] =
        de_sturm_series_coefficient_value(1L, constant_3);
    solutions_out[3] =
        de_sturm_series_coefficient_value(-1L, zero);
    solutions_out[4] =
        de_sturm_series_coefficient_value(-2L, zero);
    solutions_out[5] =
        de_sturm_series_coefficient_value(-3L, zero);
    solutions_out[6] = equ_new(next_coefficient, recurrence);

    ok = 1;
    for (size_t i = 0u; i < 7u; ++i) {
        if (!solutions_out[i]) {
            ok = 0;
            break;
        }
    }
    if (ok)
        *solution_count_out = 7u;

cleanup:
    if (!ok && solutions_out) {
        for (size_t i = 0u; i < 7u; ++i) {
            equ_free(solutions_out[i]);
            solutions_out[i] = NULL;
        }
    }
    expr_free(zero);
    expr_free(constant_3);
    expr_free(constant_2);
    expr_free(recurrence);
    expr_free(denominator);
    expr_free(two);
    expr_free(index_product);
    expr_free(index_plus_1);
    expr_free(index_plus_2);
    expr_free(numerator);
    expr_free(parameter_term);
    expr_free(previous_coefficient);
    expr_free(current_coefficient);
    expr_free(next_coefficient);
    expr_free(previous_index);
    expr_free(next_index);
    expr_free(series);
    expr_free(term);
    expr_free(power);
    expr_free(coefficient);
    expr_free(index);
    return ok ? 0 : -1;
}

static expr_t *de_exp_owned(expr_t *argument)
{
    return de_simplify_unary_owned(argument, expr_exp);
}

static expr_t *de_root(const de_linear_second_order_t *form,
                       const expr_t *sqrt_discriminant,
                       bool plus)
{
    expr_t *negative_first =
        de_simplify_unary_owned(expr_clone(form->first), expr_neg);
    expr_t *numerator = plus
        ? expr_add_simplify_owned(
              negative_first, expr_clone(sqrt_discriminant))
        : expr_sub_simplify_owned(
              negative_first, expr_clone(sqrt_discriminant));
    expr_t *denominator =
        expr_mul_simplify_owned(
            expr_const_long(2L), expr_clone(form->leading));

    return numerator && denominator
        ? expr_div_simplify_owned(numerator, denominator)
        : NULL;
}

static expr_t *de_shifted_independent(const expr_t *independent,
                                      const expr_t *point)
{
    return point
        ? expr_sub_simplify_owned(
              expr_clone(independent), expr_clone(point))
        : expr_clone(independent);
}

static expr_t *de_distinct_root_solution(
    const diffequ_t *de,
    const expr_t *independent,
    const expr_t *dependent,
    expr_t *first_root,
    expr_t *second_root)
{
    const expr_t *value_point = NULL;
    const expr_t *value = NULL;
    const expr_t *slope_point = NULL;
    const expr_t *slope = NULL;
    expr_t *shift = NULL;
    expr_t *first_exponential = NULL;
    expr_t *second_exponential = NULL;
    expr_t *first_coefficient = NULL;
    expr_t *second_coefficient = NULL;
    expr_t *denominator = NULL;
    expr_t *first_term = NULL;
    expr_t *second_term = NULL;
    expr_t *solution = NULL;
    bool has_value;
    bool has_slope;

    has_value = de_find_initial_condition(
        de, dependent, &value_point, &value);
    has_slope = de_find_derivative_condition(
        de,
        dependent,
        independent,
        1u,
        &slope_point,
        &slope);
    if (has_value != has_slope ||
        (has_value && !expr_struct_eq(value_point, slope_point)))
        goto cleanup;

    shift = de_shifted_independent(
        independent, has_value ? value_point : NULL);
    first_exponential = shift
        ? de_exp_owned(expr_mul_simplify_owned(
              expr_clone(first_root), expr_clone(shift)))
        : NULL;
    second_exponential = shift
        ? de_exp_owned(expr_mul_simplify_owned(
              expr_clone(second_root), expr_clone(shift)))
        : NULL;
    if (!first_exponential || !second_exponential)
        goto cleanup;

    if (has_value) {
        denominator = expr_sub_simplify_owned(
            expr_clone(first_root), expr_clone(second_root));
        first_coefficient = denominator
            ? expr_div_simplify_owned(
                  expr_sub_simplify_owned(
                      expr_clone(slope),
                      expr_mul_simplify_owned(
                          expr_clone(second_root), expr_clone(value))),
                  expr_clone(denominator))
            : NULL;
        second_coefficient = denominator
            ? expr_div_simplify_owned(
                  expr_sub_simplify_owned(
                      expr_mul_simplify_owned(
                          expr_clone(first_root), expr_clone(value)),
                      expr_clone(slope)),
                  denominator)
            : NULL;
        if (denominator)
            denominator = NULL;
    } else {
        first_coefficient = de_named_constant("C1");
        second_coefficient = de_named_constant("C2");
    }

    first_term = first_coefficient
        ? expr_mul_simplify_owned(
              first_coefficient, first_exponential)
        : NULL;
    if (first_coefficient)
        first_coefficient = NULL;
    if (first_exponential)
        first_exponential = NULL;
    second_term = second_coefficient
        ? expr_mul_simplify_owned(
              second_coefficient, second_exponential)
        : NULL;
    if (second_coefficient)
        second_coefficient = NULL;
    if (second_exponential)
        second_exponential = NULL;
    solution = first_term && second_term
        ? expr_add_simplify_owned(first_term, second_term)
        : NULL;
    if (first_term && second_term) {
        first_term = NULL;
        second_term = NULL;
    }

cleanup:
    expr_free(second_term);
    expr_free(first_term);
    expr_free(denominator);
    expr_free(second_coefficient);
    expr_free(first_coefficient);
    expr_free(second_exponential);
    expr_free(first_exponential);
    expr_free(shift);
    expr_free(second_root);
    expr_free(first_root);
    return solution;
}

static expr_t *de_repeated_root_solution(
    const diffequ_t *de,
    const expr_t *independent,
    const expr_t *dependent,
    expr_t *root)
{
    const expr_t *value_point = NULL;
    const expr_t *value = NULL;
    const expr_t *slope_point = NULL;
    const expr_t *slope = NULL;
    expr_t *shift = NULL;
    expr_t *first_coefficient = NULL;
    expr_t *second_coefficient = NULL;
    expr_t *linear_term = NULL;
    expr_t *exponential = NULL;
    expr_t *solution = NULL;
    bool has_value;
    bool has_slope;

    has_value = de_find_initial_condition(
        de, dependent, &value_point, &value);
    has_slope = de_find_derivative_condition(
        de,
        dependent,
        independent,
        1u,
        &slope_point,
        &slope);
    if (has_value != has_slope ||
        (has_value && !expr_struct_eq(value_point, slope_point)))
        goto cleanup;

    shift = de_shifted_independent(
        independent, has_value ? value_point : NULL);
    if (has_value) {
        first_coefficient = expr_clone(value);
        second_coefficient = expr_sub_simplify_owned(
            expr_clone(slope),
            expr_mul_simplify_owned(
                expr_clone(root), expr_clone(value)));
    } else {
        first_coefficient = de_named_constant("C1");
        second_coefficient = de_named_constant("C2");
    }
    linear_term = second_coefficient && shift
        ? expr_add_simplify_owned(
              first_coefficient,
              expr_mul_simplify_owned(
                  second_coefficient, expr_clone(shift)))
        : NULL;
    if (second_coefficient && first_coefficient) {
        second_coefficient = NULL;
        first_coefficient = NULL;
    }
    exponential = shift
        ? de_exp_owned(expr_mul_simplify_owned(root, shift))
        : NULL;
    if (shift)
        shift = NULL;
    if (root)
        root = NULL;
    solution = linear_term && exponential
        ? expr_mul_simplify_owned(linear_term, exponential)
        : NULL;
    if (linear_term && exponential) {
        linear_term = NULL;
        exponential = NULL;
    }

cleanup:
    expr_free(exponential);
    expr_free(linear_term);
    expr_free(second_coefficient);
    expr_free(first_coefficient);
    expr_free(shift);
    expr_free(root);
    return solution;
}

static expr_t *de_oscillatory_solution(
    const diffequ_t *de,
    const expr_t *independent,
    const expr_t *dependent,
    expr_t *alpha,
    expr_t *beta)
{
    const expr_t *value_point = NULL;
    const expr_t *value = NULL;
    const expr_t *slope_point = NULL;
    const expr_t *slope = NULL;
    expr_t *shift = NULL;
    expr_t *phase = NULL;
    expr_t *cosine = NULL;
    expr_t *sine = NULL;
    expr_t *cosine_coefficient = NULL;
    expr_t *sine_coefficient = NULL;
    expr_t *cosine_term = NULL;
    expr_t *sine_term = NULL;
    expr_t *oscillation = NULL;
    expr_t *envelope = NULL;
    expr_t *solution = NULL;
    bool has_value;
    bool has_slope;

    has_value = de_find_initial_condition(
        de, dependent, &value_point, &value);
    has_slope = de_find_derivative_condition(
        de,
        dependent,
        independent,
        1u,
        &slope_point,
        &slope);
    if (has_value != has_slope ||
        (has_value && !expr_struct_eq(value_point, slope_point)))
        goto cleanup;

    shift = de_shifted_independent(
        independent, has_value ? value_point : NULL);
    phase = shift
        ? expr_mul_simplify_owned(
              expr_clone(beta), expr_clone(shift))
        : NULL;
    cosine = phase ? expr_cos(phase) : NULL;
    sine = phase ? expr_sin(phase) : NULL;
    if (!cosine || !sine)
        goto cleanup;

    if (has_value) {
        cosine_coefficient = expr_clone(value);
        sine_coefficient = expr_div_simplify_owned(
            expr_sub_simplify_owned(
                expr_clone(slope),
                expr_mul_simplify_owned(
                    expr_clone(alpha), expr_clone(value))),
            expr_clone(beta));
    } else {
        cosine_coefficient = de_named_constant("C1");
        sine_coefficient = de_named_constant("C2");
    }
    cosine_term = cosine_coefficient
        ? expr_mul_simplify_owned(cosine_coefficient, cosine)
        : NULL;
    if (cosine_coefficient)
        cosine_coefficient = NULL;
    if (cosine)
        cosine = NULL;
    sine_term = sine_coefficient
        ? expr_mul_simplify_owned(sine_coefficient, sine)
        : NULL;
    if (sine_coefficient)
        sine_coefficient = NULL;
    if (sine)
        sine = NULL;
    oscillation = cosine_term && sine_term
        ? expr_add_simplify_owned(cosine_term, sine_term)
        : NULL;
    if (cosine_term && sine_term) {
        cosine_term = NULL;
        sine_term = NULL;
    }
    envelope = shift
        ? de_exp_owned(expr_mul_simplify_owned(alpha, shift))
        : NULL;
    if (alpha)
        alpha = NULL;
    if (shift)
        shift = NULL;
    solution = envelope && oscillation
        ? expr_mul_simplify_owned(envelope, oscillation)
        : NULL;
    if (envelope && oscillation) {
        envelope = NULL;
        oscillation = NULL;
    }

cleanup:
    expr_free(envelope);
    expr_free(oscillation);
    expr_free(sine_term);
    expr_free(cosine_term);
    expr_free(sine_coefficient);
    expr_free(cosine_coefficient);
    expr_free(sine);
    expr_free(cosine);
    expr_free(phase);
    expr_free(shift);
    expr_free(beta);
    expr_free(alpha);
    return solution;
}

static void de_number_coefficients_free(number_t *coefficients,
                                        size_t count)
{
    if (!coefficients)
        return;
    for (size_t i = 0u; i < count; ++i)
        num_destroy(&coefficients[i]);
    free(coefficients);
}

static bool de_affine_riccati_basis(
    const expr_t *potential,
    const expr_t *independent,
    expr_t **primary_out,
    expr_t **second_factor_out)
{
    number_t *coefficients = NULL;
    size_t degree = 0u;
    number_t slope = num_new();
    number_t twice_slope = num_new();
    number_t intercept = num_new();
    number_t intercept_squared = num_new();
    number_t expected_constant = num_new();
    number_t half_slope = num_new();
    number_t shift = num_new();
    number_t square_root_slope = num_new();
    expr_t *independent_squared = NULL;
    expr_t *quadratic_exponent = NULL;
    expr_t *linear_exponent = NULL;
    expr_t *exponent = NULL;
    expr_t *primary = NULL;
    expr_t *shifted = NULL;
    expr_t *error_argument = NULL;
    expr_t *second_factor = NULL;
    bool matched = false;

    *primary_out = NULL;
    *second_factor_out = NULL;
    if (!equ_match_polynomial_alloc(
            potential, independent, &coefficients, &degree) ||
        degree != 2u ||
        !num_is_real(coefficients[0]) ||
        !num_is_real(coefficients[1]) ||
        !num_is_real(coefficients[2]) ||
        !num_gt(coefficients[2], NUM_ZERO))
        goto cleanup;

    num_destroy(&slope);
    slope = num_sqrt(coefficients[2]);
    if (!num_is_real(slope) || !num_gt(slope, NUM_ZERO))
        goto cleanup;
    num_destroy(&twice_slope);
    twice_slope = num_mul(NUM_TWO, slope);
    num_destroy(&intercept);
    intercept = num_div(coefficients[1], twice_slope);
    num_destroy(&intercept_squared);
    intercept_squared = num_mul(intercept, intercept);
    num_destroy(&expected_constant);
    expected_constant = num_add(intercept_squared, slope);
    if (!num_eq(expected_constant, coefficients[0]))
        goto cleanup;

    num_destroy(&half_slope);
    half_slope = num_div(slope, NUM_TWO);
    num_destroy(&shift);
    shift = num_div(intercept, slope);
    num_destroy(&square_root_slope);
    square_root_slope = num_sqrt(slope);

    independent_squared = expr_pow_long(independent, 2L);
    quadratic_exponent = independent_squared
        ? expr_mul_simplify_owned(
              expr_new_const(half_slope), independent_squared)
        : NULL;
    if (independent_squared)
        independent_squared = NULL;
    linear_exponent = expr_mul_simplify_owned(
        expr_new_const(intercept), expr_clone(independent));
    exponent = quadratic_exponent && linear_exponent
        ? expr_add_simplify_owned(
              quadratic_exponent, linear_exponent)
        : NULL;
    if (quadratic_exponent && linear_exponent) {
        quadratic_exponent = NULL;
        linear_exponent = NULL;
    }
    primary = exponent ? de_exp_owned(exponent) : NULL;
    if (exponent)
        exponent = NULL;

    shifted = expr_add_simplify_owned(
        expr_clone(independent), expr_new_const(shift));
    error_argument = shifted
        ? expr_mul_simplify_owned(
              expr_new_const(square_root_slope), shifted)
        : NULL;
    if (shifted)
        shifted = NULL;
    second_factor = error_argument
        ? de_simplify_unary_owned(error_argument, expr_erf)
        : NULL;
    if (error_argument)
        error_argument = NULL;
    if (!primary || !second_factor)
        goto cleanup;

    *primary_out = primary;
    *second_factor_out = second_factor;
    primary = NULL;
    second_factor = NULL;
    matched = true;

cleanup:
    expr_free(second_factor);
    expr_free(error_argument);
    expr_free(shifted);
    expr_free(primary);
    expr_free(exponent);
    expr_free(linear_exponent);
    expr_free(quadratic_exponent);
    expr_free(independent_squared);
    num_destroy(&square_root_slope);
    num_destroy(&shift);
    num_destroy(&half_slope);
    num_destroy(&expected_constant);
    num_destroy(&intercept_squared);
    num_destroy(&intercept);
    num_destroy(&twice_slope);
    num_destroy(&slope);
    de_number_coefficients_free(coefficients, degree + 1u);
    return matched;
}

static expr_t *de_simplified_at(const expr_t *expression,
                                const expr_t *independent,
                                const expr_t *point)
{
    expr_t *substituted = expr_substitute(
        expression, independent, point);
    expr_t *simplified = substituted
        ? expr_simplify(substituted)
        : NULL;

    expr_free(substituted);
    return simplified;
}

static expr_t *de_factorized_basis_solution(
    const diffequ_t *de,
    const expr_t *independent,
    const expr_t *dependent,
    const expr_t *primary,
    const expr_t *second_factor)
{
    const expr_t *value_point = NULL;
    const expr_t *value = NULL;
    const expr_t *slope_point = NULL;
    const expr_t *slope = NULL;
    expr_t *secondary = NULL;
    expr_t *primary_derivative = NULL;
    expr_t *secondary_derivative = NULL;
    expr_t *primary_at_point = NULL;
    expr_t *secondary_at_point = NULL;
    expr_t *primary_derivative_at_point = NULL;
    expr_t *secondary_derivative_at_point = NULL;
    expr_t *determinant = NULL;
    expr_t *first_coefficient = NULL;
    expr_t *second_coefficient = NULL;
    expr_t *second_term = NULL;
    expr_t *combination = NULL;
    expr_t *solution = NULL;
    bool has_value = de_find_initial_condition(
        de, dependent, &value_point, &value);
    bool has_slope = de_find_derivative_condition(
        de,
        dependent,
        independent,
        1u,
        &slope_point,
        &slope);

    if (has_value != has_slope ||
        (has_value && !expr_struct_eq(value_point, slope_point)))
        goto cleanup;

    if (!has_value) {
        first_coefficient = de_named_constant("C1");
        second_coefficient = de_named_constant("C2");
        goto combine;
    }

    secondary = expr_mul_simplify_owned(
        expr_clone(primary), expr_clone(second_factor));
    primary_derivative = expr_create_deriv(primary, independent);
    secondary_derivative = secondary
        ? expr_create_deriv(secondary, independent)
        : NULL;
    primary_at_point = de_simplified_at(
        primary, independent, value_point);
    secondary_at_point = secondary
        ? de_simplified_at(secondary, independent, value_point)
        : NULL;
    primary_derivative_at_point = primary_derivative
        ? de_simplified_at(
              primary_derivative, independent, value_point)
        : NULL;
    secondary_derivative_at_point = secondary_derivative
        ? de_simplified_at(
              secondary_derivative, independent, value_point)
        : NULL;
    if (!primary_at_point || !secondary_at_point ||
        !primary_derivative_at_point ||
        !secondary_derivative_at_point)
        goto cleanup;

    determinant = expr_sub_simplify_owned(
        expr_mul_simplify_owned(
            expr_clone(primary_at_point),
            expr_clone(secondary_derivative_at_point)),
        expr_mul_simplify_owned(
            expr_clone(secondary_at_point),
            expr_clone(primary_derivative_at_point)));
    first_coefficient = determinant
        ? expr_div_simplify_owned(
              expr_sub_simplify_owned(
                  expr_mul_simplify_owned(
                      expr_clone(value),
                      expr_clone(secondary_derivative_at_point)),
                  expr_mul_simplify_owned(
                      expr_clone(slope),
                      expr_clone(secondary_at_point))),
              expr_clone(determinant))
        : NULL;
    second_coefficient = determinant
        ? expr_div_simplify_owned(
              expr_sub_simplify_owned(
                  expr_mul_simplify_owned(
                      expr_clone(slope),
                      expr_clone(primary_at_point)),
                  expr_mul_simplify_owned(
                      expr_clone(value),
                      expr_clone(primary_derivative_at_point))),
              determinant)
        : NULL;
    if (determinant)
        determinant = NULL;

combine:
    second_term = second_coefficient
        ? expr_mul_simplify_owned(
              second_coefficient, expr_clone(second_factor))
        : NULL;
    if (second_coefficient)
        second_coefficient = NULL;
    combination = first_coefficient && second_term
        ? expr_add_simplify_owned(first_coefficient, second_term)
        : NULL;
    if (first_coefficient && second_term) {
        first_coefficient = NULL;
        second_term = NULL;
    }
    solution = combination
        ? expr_mul_simplify_owned(
              combination, expr_clone(primary))
        : NULL;
    if (combination)
        combination = NULL;

cleanup:
    expr_free(combination);
    expr_free(second_term);
    expr_free(second_coefficient);
    expr_free(first_coefficient);
    expr_free(determinant);
    expr_free(secondary_derivative_at_point);
    expr_free(primary_derivative_at_point);
    expr_free(secondary_at_point);
    expr_free(primary_at_point);
    expr_free(secondary_derivative);
    expr_free(primary_derivative);
    expr_free(secondary);
    return solution;
}

static expr_t *de_affine_factorized_solution(
    const diffequ_t *de,
    const expr_t *independent,
    const expr_t *dependent,
    const de_linear_second_order_t *form)
{
    expr_t *potential = NULL;
    expr_t *primary = NULL;
    expr_t *second_factor = NULL;
    expr_t *solution = NULL;

    if (!expr_is_exact_zero(form->first) ||
        !expr_is_exact_zero(form->forcing) ||
        de_expr_uses(form->leading, independent))
        return NULL;

    potential = expr_div_simplify_owned(
        de_simplify_unary_owned(
            expr_clone(form->dependent), expr_neg),
        expr_clone(form->leading));
    if (!potential ||
        !de_affine_riccati_basis(
            potential,
            independent,
            &primary,
            &second_factor))
        goto cleanup;

    solution = de_factorized_basis_solution(
        de,
        independent,
        dependent,
        primary,
        second_factor);

cleanup:
    expr_free(second_factor);
    expr_free(primary);
    expr_free(potential);
    return solution;
}

de_attempt_t de_attempt_sturm_liouville(
    const diffequ_t *de,
    const expr_t *independent,
    const expr_t *dependent,
    const expr_t *second_derivative,
    const expr_t *first_derivative,
    const expr_t *residual,
    equation_t **solution_out)
{
    de_linear_second_order_t form = { 0 };
    expr_t *first_squared = NULL;
    expr_t *leading_dependent = NULL;
    expr_t *four_leading_dependent = NULL;
    expr_t *discriminant = NULL;
    expr_t *sqrt_discriminant = NULL;
    expr_t *first_root = NULL;
    expr_t *second_root = NULL;
    expr_t *solution = NULL;
    de_attempt_t attempt = DE_ATTEMPT_NOT_MATCHED;

    if (!de_decompose_second_order(
            residual,
            second_derivative,
            first_derivative,
            dependent,
            &form))
        goto cleanup;
    if (expr_is_exact_zero(form.leading) ||
        de_expr_uses(form.leading, dependent) ||
        de_expr_uses(form.first, dependent) ||
        de_expr_uses(form.dependent, dependent) ||
        de_expr_uses(form.forcing, dependent))
        goto cleanup;

    attempt = DE_ATTEMPT_FAILED;
    if (!de_build_self_adjoint_form(independent, &form))
        goto cleanup;

    solution = de_affine_factorized_solution(
        de, independent, dependent, &form);
    if (solution)
        goto make_solution;

    if (de_expr_uses(form.leading, independent) ||
        de_expr_uses(form.first, independent) ||
        de_expr_uses(form.dependent, independent) ||
        de_expr_uses(form.forcing, independent) ||
        !expr_is_exact_zero(form.forcing)) {
        attempt = DE_ATTEMPT_NOT_MATCHED;
        goto cleanup;
    }

    first_squared = expr_pow_long(form.first, 2L);
    leading_dependent = expr_mul_simplify_owned(
        expr_clone(form.leading), expr_clone(form.dependent));
    four_leading_dependent = leading_dependent
        ? expr_mul_simplify_owned(
              expr_const_long(4L), leading_dependent)
        : NULL;
    if (leading_dependent)
        leading_dependent = NULL;
    discriminant = first_squared && four_leading_dependent
        ? expr_sub_simplify_owned(
              first_squared, four_leading_dependent)
        : NULL;
    if (first_squared && four_leading_dependent) {
        first_squared = NULL;
        four_leading_dependent = NULL;
    }
    if (!discriminant)
        goto cleanup;

    if (expr_is_exact_zero(discriminant)) {
        expr_t *denominator = expr_mul_simplify_owned(
            expr_const_long(2L), expr_clone(form.leading));
        expr_t *root = denominator
            ? expr_div_simplify_owned(
                  de_simplify_unary_owned(
                      expr_clone(form.first), expr_neg),
                  denominator)
            : NULL;

        solution = root
            ? de_repeated_root_solution(
                  de, independent, dependent, root)
            : NULL;
        root = NULL;
    } else {
        number_t discriminant_value = num_new();
        bool numeric_discriminant = expr_match_const_value(
            discriminant, &discriminant_value);

        if (numeric_discriminant &&
            num_is_real(discriminant_value) &&
            num_lt(discriminant_value, NUM_ZERO)) {
            number_t positive_value =
                num_neg(discriminant_value);
            number_t root_value = num_sqrt(positive_value);
            expr_t *positive_root = expr_new_const(root_value);
            expr_t *denominator = expr_mul_simplify_owned(
                expr_const_long(2L), expr_clone(form.leading));
            expr_t *alpha_denominator =
                expr_clone(denominator);
            expr_t *alpha = alpha_denominator
                ? expr_div_simplify_owned(
                      de_simplify_unary_owned(
                          expr_clone(form.first), expr_neg),
                      alpha_denominator)
                : NULL;
            expr_t *beta =
                positive_root && denominator
                ? expr_div_simplify_owned(
                      positive_root, denominator)
                : NULL;

            if (positive_root && denominator) {
                positive_root = NULL;
                denominator = NULL;
            }
            solution = alpha && beta
                ? de_oscillatory_solution(
                      de,
                      independent,
                      dependent,
                      alpha,
                      beta)
                : NULL;
            if (alpha && beta) {
                alpha = NULL;
                beta = NULL;
            }
            expr_free(beta);
            expr_free(alpha);
            expr_free(denominator);
            expr_free(positive_root);
            num_destroy(&root_value);
            num_destroy(&positive_value);
        } else if (numeric_discriminant &&
                   num_is_real(discriminant_value) &&
                   num_ge(discriminant_value, NUM_ZERO)) {
            number_t root_value = num_sqrt(discriminant_value);

            sqrt_discriminant = expr_new_const(root_value);
            num_destroy(&root_value);
        } else {
            sqrt_discriminant = de_simplify_unary_owned(
                expr_clone(discriminant), expr_sqrt);
        }
        num_destroy(&discriminant_value);
        if (!solution) {
            first_root = sqrt_discriminant
                ? de_root(&form, sqrt_discriminant, true)
                : NULL;
            second_root = sqrt_discriminant
                ? de_root(&form, sqrt_discriminant, false)
                : NULL;
            solution = first_root && second_root
                ? de_distinct_root_solution(
                      de,
                      independent,
                      dependent,
                      first_root,
                      second_root)
                : NULL;
            if (first_root && second_root) {
                first_root = NULL;
                second_root = NULL;
            }
        }
    }

make_solution:
    if (!solution)
        goto cleanup;
    *solution_out = equ_new(dependent, solution);
    if (*solution_out)
        attempt = DE_ATTEMPT_SOLVED;

cleanup:
    expr_free(solution);
    expr_free(second_root);
    expr_free(first_root);
    expr_free(sqrt_discriminant);
    expr_free(discriminant);
    expr_free(four_leading_dependent);
    expr_free(leading_dependent);
    expr_free(first_squared);
    de_linear_second_order_clear(&form);
    return attempt;
}
