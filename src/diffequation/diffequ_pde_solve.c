/**
 * @file diffequ_pde_solve.c
 * @brief Partial differential equation solver dispatch.
 *
 * Coordinates two-variable and multivariable solving strategies and recognised stationary or harmonic cases.
 * Family-specific solvers return matched results or leave the problem unresolved.
 *
 * This is an implementation unit for diffequ_t. Specialised solving paths apply only to recognised
 * families; callers must inspect result status and any retained conditions.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MARS_SHARED_COMPUTATION_INTERNAL_ACCESS
#include "internal/computation_internal.h"

#define MARS_DIFFEQUATION_PDE_INTERNAL_ACCESS
#include "diffequ_pde_internal.h"

#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_notation_internal.h"

static bool de_pde_exact_small_rational(number_t value, number_t *exact_out)
{
    double approximation;

    if (!exact_out || !num_is_real(value) || !num_is_finite(value))
        return false;
    approximation = num_to_double(value);
    for (long denominator = 1L; denominator <= 16L; ++denominator) {
        long numerator = lround(approximation * (double)denominator);
        number_t candidate;

        if (numerator < -64L || numerator > 64L)
            continue;
        candidate = num_create_from_frac(numerator, denominator);
        if (num_eq(value, candidate)) {
            num_destroy(exact_out);
            *exact_out = candidate;
            return true;
        }
        num_destroy(&candidate);
    }
    return false;
}

static expr_t *de_pde_exact_sample_constant(const expr_t *expr)
{
    number_t value;
    number_t real;
    number_t imaginary;
    number_t exact_real = num_new();
    number_t exact_imaginary = num_new();
    number_t exact_value;
    number_t imaginary_term;
    expr_t *constant = NULL;

    if (!expr)
        return NULL;
    value = expr_eval(expr);
    if (num_is_exact(value)) {
        constant = expr_new_const(value);
        goto cleanup_value;
    }

    real = num_real_part(value);
    imaginary = num_imag_part(value);
    if (de_pde_exact_small_rational(real, &exact_real) &&
        de_pde_exact_small_rational(imaginary, &exact_imaginary)) {
        imaginary_term = num_mul(exact_imaginary, NUM_I);
        exact_value = num_add(exact_real, imaginary_term);
        if (num_eq(value, exact_value))
            constant = expr_new_const(exact_value);
        num_destroy(&exact_value);
        num_destroy(&imaginary_term);
    }
    num_destroy(&imaginary);
    num_destroy(&real);

cleanup_value:
    num_destroy(&exact_imaginary);
    num_destroy(&exact_real);
    num_destroy(&value);
    return constant;
}

static bool de_pde_collect_dependent_derivatives(const expr_t *expression, const expr_t *dependent,
                                                 const expr_t **derivatives, size_t *count, size_t capacity)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;

    if (!expression)
        return true;
    if (expr_is_formal_derivative(expression) &&
        de_pde_same_symbolic_form(expr_formal_derivative_dependent(expression), dependent)) {
        for (size_t i = 0u; i < *count; ++i)
            if (expr_struct_eq(derivatives[i], expression))
                return true;
        if (*count >= capacity)
            return false;
        derivatives[(*count)++] = expression;
        return true;
    }
    if (!expr_child_exprs(expression, &left, &right))
        return true;
    return de_pde_collect_dependent_derivatives(left, dependent, derivatives, count, capacity) &&
           de_pde_collect_dependent_derivatives(right, dependent, derivatives, count, capacity);
}

static expr_t *de_pde_apply_spatial_operator(const expr_t *operator_expression, const expr_t *dependent,
                                             const expr_t *initial)
{
    expr_t *applied = expr_substitute(operator_expression, dependent, initial);

    if (applied) {
        expr_t *simplified = expr_simplify(applied);

        expr_free(applied);
        applied = simplified;
    }
    return applied;
}

static bool de_pde_stationary_rate_matches_at_point(const expr_t *rate_expression, const diffequ_t *de,
                                                    size_t time_index, const expr_t *expected_rate, size_t sample_index)
{
    expr_t *sample = expr_clone(rate_expression);
    expr_t *difference_raw = NULL;
    expr_t *difference = NULL;
    bool matches = false;

    for (size_t i = 0u; sample && i < de->independent_count; ++i) {
        expr_t *point;
        expr_t *substituted;
        long coordinate;

        if (i == time_index)
            continue;
        coordinate = 1L + (long)((sample_index + 2u * i) % 3u);
        point = expr_const_long(coordinate);
        substituted = point ? expr_substitute(sample, de->independent_vars[i], point) : NULL;
        expr_free(point);
        expr_free(sample);
        sample = substituted;
    }
    if (sample) {
        expr_t *simplified = expr_simplify(sample);

        expr_free(sample);
        sample = simplified;
    }
    difference_raw = sample ? expr_sub(sample, expected_rate) : NULL;
    difference = difference_raw ? expr_simplify(difference_raw) : NULL;
    matches = difference && expr_is_exact_zero(difference);
    if (!matches && difference) {
        number_t numerical_difference = expr_eval(difference);
        number_t magnitude = num_abs(numerical_difference);

        matches = num_is_finite(magnitude) && num_to_double(magnitude) < 1.0e-20;
        num_destroy(&magnitude);
        num_destroy(&numerical_difference);
    }

    expr_free(difference);
    expr_free(difference_raw);
    expr_free(sample);
    return matches;
}

static const expr_t *de_pde_initial_time_point(const diffequ_t *de, size_t time_index)
{
    const expr_t *time_point = NULL;

    for (size_t point_index = 0u; point_index < de->condition_point_counts[0]; ++point_index) {
        const expr_t *point = de->condition_points[0][point_index];
        bool is_free_spatial_coordinate = false;

        for (size_t independent_index = 0u; independent_index < de->independent_count; ++independent_index) {
            if (independent_index != time_index &&
                de_pde_same_symbolic_form(point, de->independent_vars[independent_index])) {
                is_free_spatial_coordinate = true;
                break;
            }
        }
        if (is_free_spatial_coordinate)
            continue;
        if (time_point)
            return NULL;
        time_point = point;
    }
    return time_point;
}

static bool de_pde_verify_stationary_rate(const expr_t *rate_expression, const diffequ_t *de, size_t time_index,
                                          const expr_t *expected_rate)
{
    size_t sample_count = 2u * de->independent_count + 3u;

    if (!rate_expression || !de || !expected_rate)
        return false;
    for (size_t sample = 0u; sample < sample_count; ++sample)
        if (!de_pde_stationary_rate_matches_at_point(rate_expression, de, time_index, expected_rate, sample))
            return false;
    return true;
}

static diffequ_solve_result_t *de_pde_solve_stationary_eigenfunction(const diffequ_t *de, const expr_t *residual,
                                                                     bool include_steps)
{
    const expr_t *dependent = NULL;
    const expr_t *time_derivative = NULL;
    const expr_t *initial;
    const expr_t *initial_time;
    size_t time_index = 0u;
    expr_t *time_coefficient = NULL;
    expr_t *spatial_operator = NULL;
    expr_t *applied_operator = NULL;
    expr_t *denominator = NULL;
    expr_t *rate_raw = NULL;
    expr_t *rate = NULL;
    expr_t *rate_sample = NULL;
    expr_t *time_offset = NULL;
    expr_t *phase_exponent = NULL;
    expr_t *phase = NULL;
    expr_t *right = NULL;
    equation_t *solution = NULL;
    diffequ_solve_result_t *result = NULL;

    if (!de || !residual || de->condition_count != 1u || de->condition_point_counts[0] != de->independent_count)
        goto cleanup;
    for (size_t i = 0u; i < de->independent_count; ++i) {
        const expr_t *candidate = NULL;
        size_t derivative_count = 0u;
        const expr_t *derivatives[64];

        if (!de_pde_collect_dependent_derivatives(residual, equ_lhs(de->conditions[0]), derivatives, &derivative_count,
                                                  64u))
            goto cleanup;
        for (size_t j = 0u; j < derivative_count; ++j)
            if (expr_formal_derivative_order(derivatives[j]) == 1u &&
                expr_struct_eq(expr_formal_derivative_wrt_at(derivatives[j], 0u), de->independent_vars[i])) {
                candidate = derivatives[j];
                break;
            }
        if (candidate) {
            dependent = expr_formal_derivative_dependent(candidate);
            time_derivative = candidate;
            time_index = i;
            break;
        }
    }
    if (!dependent || !time_derivative || !de_pde_same_symbolic_form(equ_lhs(de->conditions[0]), dependent) ||
        !de_linear_decompose(residual, time_derivative, &time_coefficient, &spatial_operator) ||
        expr_is_exact_zero(time_coefficient))
        goto cleanup;
    initial = equ_rhs(de->conditions[0]);
    initial_time = de_pde_initial_time_point(de, time_index);
    if (!initial_time)
        goto cleanup;
    applied_operator = de_pde_apply_spatial_operator(spatial_operator, dependent, initial);
    denominator = expr_mul_simplify_owned(expr_clone(time_coefficient), expr_clone(initial));
    rate_raw = applied_operator && denominator
                   ? expr_div_simplify_owned(expr_negate_owned(applied_operator), denominator)
                   : NULL;
    applied_operator = NULL;
    denominator = NULL;
    rate_sample = expr_clone(rate_raw);
    for (size_t i = 0u; rate_sample && i < de->independent_count; ++i) {
        expr_t *point;
        expr_t *substituted;

        if (i == time_index)
            continue;
        point = expr_const_long(i == 0u || (time_index == 0u && i == 1u) ? 1L : 0L);
        substituted = point ? expr_substitute(rate_sample, de->independent_vars[i], point) : NULL;
        expr_free(point);
        expr_free(rate_sample);
        rate_sample = substituted;
    }
    rate = rate_sample ? expr_simplify(rate_sample) : NULL;
    if (!rate || !de_pde_verify_stationary_rate(rate_raw, de, time_index, rate))
        goto cleanup;
    for (size_t i = 0u; i < de->independent_count; ++i)
        if (de_expr_uses(rate, de->independent_vars[i]))
            goto cleanup;
    {
        expr_t *exact_rate = de_pde_exact_sample_constant(rate);

        if (exact_rate) {
            expr_free(rate);
            rate = exact_rate;
        }
    }
    time_offset = expr_sub_simplify_owned(expr_clone(de->independent_vars[time_index]), expr_clone(initial_time));
    phase_exponent = time_offset ? expr_mul_simplify_owned(expr_clone(rate), time_offset) : NULL;
    time_offset = NULL;
    phase = phase_exponent ? expr_exp(phase_exponent) : NULL;
    right = phase ? expr_mul_simplify_owned(expr_clone(initial), phase) : NULL;
    phase = NULL;
    solution = right ? equ_new(dependent, right) : NULL;
    result = solution ? de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_STATIONARY_EIGENFUNCTION,
                                            "solved by the stationary-eigenfunction evolution rule")
                      : NULL;
    if (!result || de_solve_result_append(result, solution) != 0) {
        de_solve_result_free(result);
        result = NULL;
        goto cleanup;
    }
    solution = NULL;
    if (include_steps &&
        !de_pde_set_stationary_steps(result, dependent, de->independent_vars[time_index], initial_time,
                                     time_coefficient, spatial_operator, initial, rate, result->solutions[0])) {
        de_solve_result_free(result);
        result = NULL;
        goto cleanup;
    }

cleanup:
    equ_free(solution);
    expr_free(right);
    expr_free(phase);
    expr_free(phase_exponent);
    expr_free(time_offset);
    expr_free(rate);
    expr_free(rate_sample);
    expr_free(rate_raw);
    expr_free(denominator);
    expr_free(applied_operator);
    expr_free(spatial_operator);
    expr_free(time_coefficient);
    return result;
}

diffequ_solve_result_t *de_pde_solve_two_variable(const diffequ_t *de, const expr_t *residual, bool include_steps)
{
    equation_t *laplace_solution = NULL;
    equation_t *polar_laplace_solution = NULL;
    equation_t *transport_solution = NULL;
    equation_t *characteristic_solutions[2] = {NULL, NULL};
    size_t characteristic_solution_count = 0u;
    bool transport_recognized = false;
    de_attempt_t transport;
    de_attempt_t characteristics;
    de_attempt_t laplace;
    de_attempt_t polar_laplace;
    diffequ_solve_result_t *result = NULL;
    char *laplace_steps = NULL;
    char *laplace_steps_TeX = NULL;
    diffequ_solve_result_t *stationary = NULL;

    if (!computation_step_current("diffequation.pde_method"))
        goto cleanup;
    result = de_pde_solve_wave_ivp(de, residual, include_steps);
    if (result)
        goto cleanup;

    if (!computation_step_current("diffequation.pde_method"))
        goto cleanup;
    result = de_pde_solve_half_line_heat(de, residual, include_steps);
    if (result)
        goto cleanup;

    polar_laplace = residual ? de_pde_attempt_polar_laplace(de, residual, &polar_laplace_solution) : DE_ATTEMPT_FAILED;
    if (polar_laplace == DE_ATTEMPT_SOLVED) {
        result = de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_LAPLACE,
                                     "solved as the polar form of the two-dimensional "
                                     "Laplace equation");
        if (!result || (include_steps && !de_polar_laplace_set_steps(result)) ||
            de_solve_result_append(result, polar_laplace_solution) != 0) {
            de_solve_result_free(result);
            result = NULL;
            goto cleanup;
        }
        polar_laplace_solution = NULL;
        goto cleanup;
    }

    laplace = residual ? de_pde_attempt_laplace(de, residual, &laplace_solution) : DE_ATTEMPT_FAILED;
    if (laplace == DE_ATTEMPT_SOLVED) {
        if (include_steps &&
            !de_pde_cartesian_laplace_steps(de, laplace_solution, &laplace_steps, &laplace_steps_TeX)) {
            result = NULL;
            goto cleanup;
        }
        result = de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_LAPLACE,
                                     "solved as the two-dimensional Laplace equation");
        if (!result || (include_steps && de_solve_result_set_steps(result, laplace_steps) != 0) ||
            (include_steps && de_solve_result_set_steps_TeX(result, laplace_steps_TeX) != 0) ||
            de_solve_result_append(result, laplace_solution) != 0) {
            de_solve_result_free(result);
            result = NULL;
            goto cleanup;
        }
        laplace_solution = NULL;
        goto cleanup;
    }

    if (!computation_step_current("diffequation.pde_method"))
        goto cleanup;
    result = de_pde_solve_second_order_constant(de, residual, include_steps);
    if (result)
        goto cleanup;

    if (!computation_step_current("diffequation.pde_method"))
        goto cleanup;
    result = de_pde_solve_radial_euler(de, residual, include_steps);
    if (result)
        goto cleanup;

    if (!computation_step_current("diffequation.pde_method"))
        goto cleanup;
    result = de_pde_solve_autonomous_transport(de, residual, include_steps);
    if (result)
        goto cleanup;

    transport = residual ? de_pde_attempt_constant_transport(de, residual, &transport_solution, &transport_recognized)
                         : DE_ATTEMPT_FAILED;
    if (transport == DE_ATTEMPT_SOLVED) {
        result = de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_CONSTANT_COEFFICIENT_TRANSPORT,
                                     "solved by the method of characteristics");
        if (!result || de_solve_result_append(result, transport_solution) != 0) {
            de_solve_result_free(result);
            result = NULL;
            goto cleanup;
        }
        transport_solution = NULL;
        goto cleanup;
    }

    characteristics = residual ? de_pde_attempt_characteristics(de, residual, characteristic_solutions,
                                                                &characteristic_solution_count)
                               : DE_ATTEMPT_FAILED;
    if (characteristics == DE_ATTEMPT_SOLVED) {
        result = de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_CHARACTERISTICS,
                                     "solved by the method of characteristics");
        if (!result)
            goto cleanup;
        for (size_t i = 0u; i < characteristic_solution_count; ++i) {
            if (de_solve_result_append(result, characteristic_solutions[i]) != 0) {
                de_solve_result_free(result);
                result = NULL;
                goto cleanup;
            }
            characteristic_solutions[i] = NULL;
        }
        goto cleanup;
    }

    if (!computation_step_current("diffequation.pde_method"))
        goto cleanup;
    result = de_pde_solve_affine_transport(de, residual, include_steps);
    if (result)
        goto cleanup;
    if (!computation_step_current("diffequation.pde_method"))
        goto cleanup;
    result = de_pde_solve_triangular_transport(de, residual, include_steps);
    if (result)
        goto cleanup;
    if (!computation_step_current("diffequation.pde_method"))
        goto cleanup;
    result = de_pde_solve_gradient_envelope(de, residual, include_steps);
    if (result)
        goto cleanup;
    stationary = de_pde_solve_stationary_eigenfunction(de, residual, include_steps);
    if (stationary) {
        result = stationary;
        goto cleanup;
    }

    if (!computation_step_current("diffequation.pde_method"))
        goto cleanup;
    result = de_pde_solve_kdv(de, residual, include_steps);
    if (result)
        goto cleanup;

    if (!computation_step_current("diffequation.pde_method"))
        goto cleanup;
    result = de_pde_solve_fourier_evolution(de, residual, include_steps);
    if (result)
        goto cleanup;

    result = de_solve_result_new(polar_laplace == DE_ATTEMPT_FAILED || laplace == DE_ATTEMPT_FAILED ||
                                         transport == DE_ATTEMPT_FAILED || characteristics == DE_ATTEMPT_FAILED
                                     ? DE_SOLVE_STATUS_FAILED
                                     : DE_SOLVE_STATUS_UNSUPPORTED,
                                 DE_SOLVER_NONE,
                                 polar_laplace == DE_ATTEMPT_FAILED
                                     ? "failed to complete the polar Laplace-equation solution"
                                 : laplace == DE_ATTEMPT_FAILED ? "failed to complete the Laplace-equation solution"
                                 : transport == DE_ATTEMPT_FAILED || characteristics == DE_ATTEMPT_FAILED
                                     ? "failed to complete the PDE characteristic solution"
                                 : transport_recognized ? "the transport equation boundary data is unsupported"
                                                        : "no available symbolic PDE solver matched the equation");

cleanup:
    free(laplace_steps_TeX);
    free(laplace_steps);
    for (size_t i = 0u; i < 2u; ++i)
        equ_free(characteristic_solutions[i]);
    equ_free(transport_solution);
    equ_free(laplace_solution);
    equ_free(polar_laplace_solution);
    if (!computation_poll_current()) {
        de_solve_result_free(result);
        return NULL;
    }
    return result;
}

diffequ_solve_result_t *de_pde_solve_multi_variable(const diffequ_t *de, const expr_t *residual, bool include_steps)
{
    diffequ_solve_result_t *wave = de_pde_solve_wave(de, residual, include_steps);
    if (wave)
        return wave;
    diffequ_solve_result_t *stationary = NULL;
    equation_t *solution = NULL;
    bool recognized = false;
    de_attempt_t attempt =
        residual ? de_pde_attempt_constant_transport_n(de, residual, &solution, &recognized) : DE_ATTEMPT_FAILED;
    diffequ_solve_result_t *result;

    if (attempt == DE_ATTEMPT_SOLVED) {
        result = de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_CONSTANT_COEFFICIENT_TRANSPORT,
                                     "solved by the method of characteristics");
        if (!result || de_solve_result_append(result, solution) != 0) {
            de_solve_result_free(result);
            equ_free(solution);
            return NULL;
        }
        return result;
    }

    equ_free(solution);
    result = de_pde_solve_affine_transport(de, residual, include_steps);
    if (result)
        return result;
    stationary = de_pde_solve_stationary_eigenfunction(de, residual, include_steps);
    if (stationary)
        return stationary;
    return de_solve_result_new(
        attempt == DE_ATTEMPT_FAILED ? DE_SOLVE_STATUS_FAILED : DE_SOLVE_STATUS_UNSUPPORTED, DE_SOLVER_NONE,
        attempt == DE_ATTEMPT_FAILED ? "failed to complete the multidimensional transport solution"
        : recognized                 ? "the multidimensional transport data is unsupported"
                                     : "no available symbolic multidimensional PDE solver "
                                       "matched the equation");
}
