/**
 * @file diffequ_pde_linear.c
 * @brief Parameter-dependent linear PDE reductions.
 *
 * Builds recognised linear solution forms with symbolic parameters and simplifies verified factors. It retains
 * unresolved parameters rather than substituting sample values into the solution.
 *
 * This is an implementation unit for diffequ_t. Specialised solving paths apply only to recognised
 * families; callers must inspect result status and any retained conditions.
 */

#include <stdlib.h>
#include <string.h>

#define MARS_DIFFEQUATION_PDE_INTERNAL_ACCESS
#include "diffequ_pde_internal.h"

#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_notation_internal.h"

static bool de_pde_collect_parameter_symbols(const expr_t *expression, const expr_t *independent,
                                             const expr_t *dependent, expr_t ***parameters, size_t *count,
                                             size_t *capacity)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    const char *name;

    if (!expression)
        return true;
    name = expr_symbol_name(expression);
    if (name && !expr_struct_eq(expression, independent) && !expr_struct_eq(expression, dependent)) {
        number_t value = expr_eval(expression);
        bool unknown = num_is_nan(value);

        num_destroy(&value);
        if (unknown) {
            for (size_t i = 0u; i < *count; ++i) {
                const char *existing = expr_symbol_name((*parameters)[i]);

                if (existing && strcmp(existing, name) == 0)
                    return true;
            }
            if (*count == *capacity) {
                size_t next_capacity = *capacity ? *capacity * 2u : 4u;
                expr_t **next = realloc(*parameters, next_capacity * sizeof(*next));

                if (!next)
                    return false;
                *parameters = next;
                *capacity = next_capacity;
            }
            (*parameters)[(*count)++] = (expr_t *)expression;
            return true;
        }
    }
    if (!expr_child_exprs(expression, &left, &right))
        return true;
    return de_pde_collect_parameter_symbols(left, independent, dependent, parameters, count, capacity) &&
           de_pde_collect_parameter_symbols(right, independent, dependent, parameters, count, capacity);
}

static expr_t **de_pde_parameter_symbols(const expr_t *expression, const expr_t *independent, const expr_t *dependent,
                                         size_t *count_out)
{
    expr_t **parameters = NULL;
    size_t count = 0u;
    size_t capacity = 0u;

    *count_out = 0u;
    if (!de_pde_collect_parameter_symbols(expression, independent, dependent, &parameters, &count, &capacity) ||
        count == 0u) {
        free(parameters);
        return NULL;
    }
    *count_out = count;
    return parameters;
}

static bool de_pde_split_parameter_factor(const expr_t *expression, const expr_t *wrt, expr_t **parameter_factor_out,
                                          expr_t **variable_factor_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    bool product;
    bool division = false;

    *parameter_factor_out = NULL;
    *variable_factor_out = NULL;
    if (!de_expr_uses(expression, wrt)) {
        *parameter_factor_out = expr_clone(expression);
        *variable_factor_out = expr_const_one();
        return *parameter_factor_out && *variable_factor_out;
    }
    if (expr_match_neg_expr(expression, &left)) {
        expr_t *parameter_factor = NULL;
        expr_t *variable_factor = NULL;

        if (!de_pde_split_parameter_factor(left, wrt, &parameter_factor, &variable_factor))
            return false;
        *parameter_factor_out = de_simplify_unary_owned(parameter_factor, expr_neg);
        *variable_factor_out = variable_factor;
        return *parameter_factor_out && *variable_factor_out;
    }
    product = expr_match_mul_expr(expression, &left, &right);
    if (!product) {
        left = NULL;
        right = NULL;
        division = expr_match_div_expr(expression, &left, &right);
    }
    if (product || division) {
        expr_t *left_parameter = NULL;
        expr_t *left_variable = NULL;
        expr_t *right_parameter = NULL;
        expr_t *right_variable = NULL;

        if (!de_pde_split_parameter_factor(left, wrt, &left_parameter, &left_variable) ||
            !de_pde_split_parameter_factor(right, wrt, &right_parameter, &right_variable)) {
            expr_free(right_variable);
            expr_free(right_parameter);
            expr_free(left_variable);
            expr_free(left_parameter);
            return false;
        }
        *parameter_factor_out = division ? expr_div_simplify_owned(left_parameter, right_parameter)
                                         : expr_mul_simplify_owned(left_parameter, right_parameter);
        *variable_factor_out = division ? expr_div_simplify_owned(left_variable, right_variable)
                                        : expr_mul_simplify_owned(left_variable, right_variable);
        return *parameter_factor_out && *variable_factor_out;
    }

    *parameter_factor_out = expr_const_one();
    *variable_factor_out = expr_clone(expression);
    return *parameter_factor_out && *variable_factor_out;
}

static expr_t *de_pde_integrate_parameterised(const expr_t *integrand, const expr_t *wrt)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    bool subtraction = false;

    if (expr_match_add_sub_expr(integrand, &left, &right, &subtraction)) {
        expr_t *left_integral = de_pde_integrate_parameterised(left, wrt);
        expr_t *right_integral = de_pde_integrate_parameterised(right, wrt);

        if (!left_integral || !right_integral) {
            expr_free(right_integral);
            expr_free(left_integral);
            return NULL;
        }
        return subtraction ? expr_sub_simplify_owned(left_integral, right_integral)
                           : expr_add_simplify_owned(left_integral, right_integral);
    }

    {
        expr_t *parameter_factor = NULL;
        expr_t *variable_factor = NULL;
        expr_t *variable_integral = NULL;
        expr_t *result = NULL;

        if (de_pde_split_parameter_factor(integrand, wrt, &parameter_factor, &variable_factor))
            variable_integral = expr_integrate(variable_factor, wrt);
        if (parameter_factor && variable_integral)
            result = expr_mul_simplify_owned(parameter_factor, variable_integral);
        else {
            expr_free(variable_integral);
            expr_free(parameter_factor);
        }
        expr_free(variable_factor);
        return result ? result : de_integrate_or_formal(integrand, wrt);
    }
}

static bool de_pde_parameter_identity_is_zero(const expr_t *expression, expr_t *const *parameters,
                                              size_t parameter_count)
{
    expr_t *simplified = expression ? expr_simplify(expression) : NULL;
    expr_t *expanded = NULL;
    expr_t *expanded_simplified = NULL;
    bool is_zero = simplified && expr_is_exact_zero(simplified);

    if (!is_zero) {
        expanded = expr_display_expanded(expression);
        expanded_simplified = expanded ? expr_simplify(expanded) : NULL;
        is_zero = expanded_simplified && expr_is_exact_zero(expanded_simplified);
    }
    expr_free(expanded_simplified);
    expr_free(expanded);
    expr_free(simplified);
    if (is_zero)
        return true;
    if (!expression || !parameters || parameter_count == 0u)
        return false;

    for (size_t sample_index = 0u; sample_index < 2u * parameter_count + 5u; ++sample_index) {
        expr_t *sample = expr_clone(expression);

        for (size_t parameter_index = 0u; sample && parameter_index < parameter_count; ++parameter_index) {
            long sample_value = 1L + (long)sample_index + 2L * (long)parameter_index;
            expr_t *value = expr_const_long(sample_value);
            expr_t *substituted = value ? expr_substitute(sample, parameters[parameter_index], value) : NULL;

            expr_free(value);
            expr_free(sample);
            sample = substituted;
        }
        simplified = sample ? expr_simplify(sample) : NULL;
        if (!simplified || !expr_is_exact_zero(simplified)) {
            expr_free(simplified);
            expr_free(sample);
            return false;
        }
        expr_free(simplified);
        expr_free(sample);
    }
    return true;
}

static bool de_pde_is_exact_one(const expr_t *expression)
{
    number_t value = num_new();
    bool is_one = expr_match_const_value(expression, &value) && num_eq(value, NUM_ONE);

    num_destroy(&value);
    return is_one;
}

static expr_t *de_pde_cancel_verified_unit_exponentials(const expr_t *expression, expr_t *const *parameters,
                                                        size_t parameter_count)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    bool subtraction = false;
    expr_t *left_result = NULL;
    expr_t *right_result = NULL;

    if (!expression)
        return NULL;
    if (expr_match_exp_expr(expression, &left) &&
        de_pde_parameter_identity_is_zero(left, parameters, parameter_count)) {
        return expr_const_one();
    }
    if (expr_match_add_sub_expr(expression, &left, &right, &subtraction)) {
        left_result = de_pde_cancel_verified_unit_exponentials(left, parameters, parameter_count);
        right_result = de_pde_cancel_verified_unit_exponentials(right, parameters, parameter_count);
        if (!left_result || !right_result) {
            expr_free(right_result);
            expr_free(left_result);
            return NULL;
        }
        return subtraction ? expr_sub_simplify_owned(left_result, right_result)
                           : expr_add_simplify_owned(left_result, right_result);
    }
    if (expr_match_mul_expr(expression, &left, &right)) {
        left_result = de_pde_cancel_verified_unit_exponentials(left, parameters, parameter_count);
        right_result = de_pde_cancel_verified_unit_exponentials(right, parameters, parameter_count);
        if (!left_result || !right_result) {
            expr_free(right_result);
            expr_free(left_result);
            return NULL;
        }
        if (de_pde_is_exact_one(left_result)) {
            expr_free(left_result);
            return right_result;
        }
        if (de_pde_is_exact_one(right_result)) {
            expr_free(right_result);
            return left_result;
        }
        return expr_mul_simplify_owned(left_result, right_result);
    }
    if (expr_match_div_expr(expression, &left, &right)) {
        left_result = de_pde_cancel_verified_unit_exponentials(left, parameters, parameter_count);
        right_result = de_pde_cancel_verified_unit_exponentials(right, parameters, parameter_count);
        if (!left_result || !right_result) {
            expr_free(right_result);
            expr_free(left_result);
            return NULL;
        }
        if (de_pde_is_exact_one(right_result)) {
            expr_free(right_result);
            return left_result;
        }
        return expr_div_simplify_owned(left_result, right_result);
    }
    if (expr_match_neg_expr(expression, &left)) {
        expr_t *negated = NULL;

        left_result = de_pde_cancel_verified_unit_exponentials(left, parameters, parameter_count);
        if (left_result)
            negated = expr_neg(left_result);
        expr_free(left_result);
        return negated;
    }
    return expr_clone(expression);
}

static bool de_pde_uses_any_parameter(const expr_t *expression, expr_t *const *parameters, size_t parameter_count)
{
    for (size_t i = 0u; i < parameter_count; ++i) {
        if (de_expr_uses(expression, parameters[i]))
            return true;
    }
    return false;
}

de_attempt_t de_pde_attempt_parameter_linear(const diffequ_t *de, const expr_t *independent, const expr_t *dependent,
                                             const expr_t *derivative_right, bool include_steps,
                                             equation_t **solution_out, char **steps_out, char **steps_TeX_out)
{
    expr_t **parameters = NULL;
    size_t parameter_count = 0u;
    expr_t *dependent_coefficient = NULL;
    expr_t *forcing = NULL;
    expr_t *coefficient = NULL;
    expr_t *coefficient_integral = NULL;
    expr_t *integrating_factor = NULL;
    expr_t *weighted_forcing = NULL;
    expr_t *forcing_integral = NULL;
    expr_t *arbitrary = NULL;
    expr_t *negative_coefficient_integral = NULL;
    expr_t *inverse_integrating_factor = NULL;
    expr_t *particular = NULL;
    expr_t *homogeneous = NULL;
    expr_t *solved_right = NULL;
    expr_t *coefficient_derivative = NULL;
    expr_t *coefficient_check_raw = NULL;
    expr_t *forcing_derivative = NULL;
    expr_t *forcing_check_raw = NULL;
    de_attempt_t attempt = DE_ATTEMPT_NOT_MATCHED;

    *solution_out = NULL;
    *steps_out = NULL;
    *steps_TeX_out = NULL;
    if (!de || !de->partial_derivative_input || de->condition_count != 0u || !independent || !dependent ||
        !derivative_right || !expr_symbol_name(independent))
        return DE_ATTEMPT_NOT_MATCHED;

    parameters = de_pde_parameter_symbols(derivative_right, independent, dependent, &parameter_count);
    if (!parameters || parameter_count == 0u)
        goto cleanup;
    if (!de_linear_decompose(derivative_right, dependent, &dependent_coefficient, &forcing) ||
        de_expr_uses(dependent_coefficient, dependent) || de_expr_uses(forcing, dependent))
        goto cleanup;
    attempt = DE_ATTEMPT_FAILED;

    coefficient = de_simplify_unary_owned(dependent_coefficient, expr_neg);
    dependent_coefficient = NULL;
    coefficient_integral = de_pde_integrate_parameterised(coefficient, independent);
    integrating_factor = coefficient_integral ? expr_exp(coefficient_integral) : NULL;
    weighted_forcing =
        integrating_factor ? expr_mul_simplify_owned(expr_clone(integrating_factor), expr_clone(forcing)) : NULL;
    forcing_integral = weighted_forcing ? de_pde_integrate_parameterised(weighted_forcing, independent) : NULL;
    arbitrary = parameter_count == 1u ? expr_new_arbitrary_function("F", parameters[0])
                                      : expr_new_arbitrary_function_n("F", parameter_count, parameters);
    negative_coefficient_integral =
        coefficient_integral ? de_simplify_unary_owned(expr_clone(coefficient_integral), expr_neg) : NULL;
    inverse_integrating_factor = negative_coefficient_integral ? expr_exp(negative_coefficient_integral) : NULL;
    particular = forcing_integral && inverse_integrating_factor
                     ? expr_mul_simplify_owned(expr_clone(forcing_integral), expr_clone(inverse_integrating_factor))
                     : NULL;
    if (particular && de_pde_uses_any_parameter(coefficient, parameters, parameter_count)) {
        expr_t *expanded_particular = expr_display_expanded(particular);
        expr_t *simplified_particular =
            expanded_particular
                ? de_pde_cancel_verified_unit_exponentials(expanded_particular, parameters, parameter_count)
                : NULL;
        expr_t *final_particular = simplified_particular ? expr_display_expanded(simplified_particular) : NULL;

        if (final_particular) {
            expr_free(particular);
            particular = final_particular;
            final_particular = NULL;
        }
        expr_free(final_particular);
        expr_free(simplified_particular);
        expr_free(expanded_particular);
    }
    homogeneous = arbitrary && inverse_integrating_factor
                      ? expr_mul_simplify_owned(expr_clone(arbitrary), expr_clone(inverse_integrating_factor))
                      : NULL;
    solved_right = particular && homogeneous ? expr_add(particular, homogeneous) : NULL;
    if (!coefficient || !coefficient_integral || !integrating_factor || !weighted_forcing || !forcing_integral ||
        !arbitrary || !solved_right)
        goto cleanup;

    coefficient_derivative = expr_create_deriv(coefficient_integral, independent);
    coefficient_check_raw = coefficient_derivative ? expr_sub(coefficient_derivative, coefficient) : NULL;
    forcing_derivative = expr_create_deriv(forcing_integral, independent);
    forcing_check_raw = forcing_derivative ? expr_sub(forcing_derivative, weighted_forcing) : NULL;
    if (!de_pde_parameter_identity_is_zero(coefficient_check_raw, parameters, parameter_count) ||
        !de_pde_parameter_identity_is_zero(forcing_check_raw, parameters, parameter_count))
        goto cleanup;

    *solution_out = de_pde_solution_equation(dependent, solved_right);
    if (!*solution_out || (include_steps && !de_pde_parameter_linear_steps(
                                                independent, dependent, parameters, parameter_count, coefficient,
                                                forcing, coefficient_integral, integrating_factor, weighted_forcing,
                                                forcing_integral, arbitrary, solved_right, steps_out, steps_TeX_out)))
        goto cleanup;
    attempt = DE_ATTEMPT_SOLVED;

cleanup:
    if (attempt != DE_ATTEMPT_SOLVED) {
        equ_free(*solution_out);
        *solution_out = NULL;
        free(*steps_TeX_out);
        free(*steps_out);
        *steps_TeX_out = NULL;
        *steps_out = NULL;
    }
    expr_free(forcing_check_raw);
    expr_free(forcing_derivative);
    expr_free(coefficient_check_raw);
    expr_free(coefficient_derivative);
    expr_free(solved_right);
    expr_free(homogeneous);
    expr_free(particular);
    expr_free(inverse_integrating_factor);
    expr_free(negative_coefficient_integral);
    expr_free(arbitrary);
    expr_free(forcing_integral);
    expr_free(weighted_forcing);
    expr_free(integrating_factor);
    expr_free(coefficient_integral);
    expr_free(coefficient);
    expr_free(forcing);
    expr_free(dependent_coefficient);
    free(parameters);
    return attempt;
}
