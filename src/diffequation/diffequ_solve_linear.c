#define MARS_DIFFEQUATION_SOLVE_INTERNAL_ACCESS
#include "diffequ_solve_internal.h"

static expr_t *de_linear_constant(const diffequ_t *de,
                                  const expr_t *dependent,
                                  const expr_t *independent,
                                  const expr_t *factor_exponent,
                                  const expr_t *integrating_factor,
                                  const expr_t *integral)
{
    const expr_t *point = NULL;
    const expr_t *value = NULL;
    expr_t *factor_at_point;
    expr_t *integral_at_point;
    expr_t *scaled_value;
    expr_t *constant;

    if (!de_find_initial_condition(
            de, dependent, &point, &value))
        return de_arbitrary_constant();

    factor_at_point =
        expr_is_exact_zero(point) &&
        expr_match_integral_expr(factor_exponent, NULL, NULL)
        ? expr_const_one()
        : expr_substitute(integrating_factor, independent, point);
    integral_at_point =
        expr_is_exact_zero(point) &&
        expr_match_integral_expr(integral, NULL, NULL)
        ? expr_const_zero()
        : expr_substitute(integral, independent, point);
    scaled_value = factor_at_point
        ? expr_mul_simplify_owned(expr_clone(value), factor_at_point)
        : NULL;
    if (factor_at_point)
        factor_at_point = NULL;
    constant = scaled_value && integral_at_point
        ? expr_sub_simplify_owned(
              scaled_value, integral_at_point)
        : NULL;
    if (scaled_value && integral_at_point) {
        scaled_value = NULL;
        integral_at_point = NULL;
    }

    expr_free(scaled_value);
    expr_free(integral_at_point);
    expr_free(factor_at_point);
    return constant;
}

de_attempt_t de_attempt_linear(
    const diffequ_t *de,
    const expr_t *independent,
    const expr_t *dependent,
    const expr_t *derivative_right,
    equation_t **solution_out)
{
    expr_t *dependent_coefficient = NULL;
    expr_t *forcing = NULL;
    expr_t *coefficient = NULL;
    expr_t *coefficient_integral = NULL;
    expr_t *integrating_factor = NULL;
    expr_t *weighted_forcing = NULL;
    expr_t *forcing_integral = NULL;
    expr_t *constant = NULL;
    expr_t *numerator = NULL;
    expr_t *solved_right = NULL;
    de_attempt_t attempt = DE_ATTEMPT_NOT_MATCHED;

    if (!de_linear_decompose(
            derivative_right,
            dependent,
            &dependent_coefficient,
            &forcing))
        goto cleanup;
    if (de_expr_uses(dependent_coefficient, dependent) ||
        de_expr_uses(forcing, dependent))
        goto cleanup;
    attempt = DE_ATTEMPT_FAILED;

    coefficient =
        de_simplify_unary_owned(dependent_coefficient, expr_neg);
    dependent_coefficient = NULL;
    coefficient_integral =
        de_integrate_or_formal(coefficient, independent);
    integrating_factor = coefficient_integral
        ? expr_exp(coefficient_integral)
        : NULL;
    weighted_forcing = integrating_factor
        ? expr_mul_simplify_owned(
              expr_clone(integrating_factor), forcing)
        : NULL;
    if (integrating_factor)
        forcing = NULL;
    forcing_integral =
        de_integrate_or_formal(weighted_forcing, independent);
    if (!integrating_factor || !forcing_integral)
        goto cleanup;

    constant = de_linear_constant(
        de,
        dependent,
        independent,
        coefficient_integral,
        integrating_factor,
        forcing_integral);
    numerator = constant
        ? expr_add_simplify_owned(forcing_integral, constant)
        : NULL;
    if (constant) {
        forcing_integral = NULL;
        constant = NULL;
    }
    solved_right = numerator
        ? expr_div_simplify_owned(numerator, integrating_factor)
        : NULL;
    if (numerator) {
        numerator = NULL;
        integrating_factor = NULL;
    }
    if (!solved_right)
        goto cleanup;

    *solution_out = equ_new(dependent, solved_right);
    if (*solution_out)
        attempt = DE_ATTEMPT_SOLVED;

cleanup:
    expr_free(solved_right);
    expr_free(numerator);
    expr_free(constant);
    expr_free(forcing_integral);
    expr_free(weighted_forcing);
    expr_free(integrating_factor);
    expr_free(coefficient_integral);
    expr_free(coefficient);
    expr_free(forcing);
    expr_free(dependent_coefficient);
    return attempt;
}
