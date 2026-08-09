#define MARS_DIFFEQUATION_SOLVE_INTERNAL_ACCESS
#include "diffequ_solve_internal.h"

static expr_t *de_bernoulli_arbitrary_constant(const expr_t *forcing_integral)
{
    const expr_t *unscaled = NULL;
    number_t scale = num_new();
    number_t magnitude = num_new();
    expr_t *constant = de_arbitrary_constant();

    if (!constant || !expr_match_scaled_expr(forcing_integral, &scale, &unscaled) || !num_is_finite(scale) ||
        num_eq(scale, NUM_ZERO))
        goto cleanup;

    num_destroy(&magnitude);
    magnitude = num_abs(scale);
    if (!num_eq(magnitude, NUM_ONE)) {
        expr_t *scaled = expr_mul_num(constant, &magnitude);

        if (scaled) {
            expr_free(constant);
            constant = expr_simplify_owned(scaled);
        }
    }

cleanup:
    num_destroy(&magnitude);
    num_destroy(&scale);
    return constant;
}

static const expr_t *de_find_dependent_square(const expr_t *expr, const expr_t *dependent)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    const expr_t *base = NULL;
    number_t exponent;
    const expr_t *found = NULL;

    if (!expr)
        return NULL;
    exponent = num_new();
    if (expr_match_pow_const(expr, &base, &exponent) && expr_struct_eq(base, dependent) && num_eq(exponent, NUM_TWO)) {
        found = expr;
        goto cleanup;
    }
    if (expr_is_formal_derivative(expr) || !expr_child_exprs(expr, &left, &right))
        goto cleanup;

    found = de_find_dependent_square(left, dependent);
    if (!found)
        found = de_find_dependent_square(right, dependent);

cleanup:
    num_destroy(&exponent);
    return found;
}

static expr_t *de_bernoulli_constant(const diffequ_t *de, const expr_t *dependent, const expr_t *independent,
                                     const expr_t *factor_exponent, const expr_t *integrating_factor,
                                     const expr_t *integral)
{
    const expr_t *point = NULL;
    const expr_t *value = NULL;
    expr_t *reciprocal_value = NULL;
    expr_t *factor_at_point = NULL;
    expr_t *integral_at_point = NULL;
    expr_t *scaled_value = NULL;
    expr_t *constant = NULL;

    if (!de_find_initial_condition(de, dependent, &point, &value))
        return de_bernoulli_arbitrary_constant(integral);

    reciprocal_value = expr_div_simplify_owned(expr_const_one(), expr_clone(value));
    factor_at_point = expr_is_exact_zero(point) && expr_match_integral_expr(factor_exponent, NULL, NULL)
                          ? expr_const_one()
                          : expr_substitute(integrating_factor, independent, point);
    integral_at_point = expr_is_exact_zero(point) && expr_match_integral_expr(integral, NULL, NULL)
                            ? expr_const_zero()
                            : expr_substitute(integral, independent, point);
    scaled_value =
        reciprocal_value && factor_at_point ? expr_mul_simplify_owned(reciprocal_value, factor_at_point) : NULL;
    if (reciprocal_value && factor_at_point) {
        reciprocal_value = NULL;
        factor_at_point = NULL;
    }
    constant = scaled_value && integral_at_point ? expr_sub_simplify_owned(scaled_value, integral_at_point) : NULL;
    if (scaled_value && integral_at_point) {
        scaled_value = NULL;
        integral_at_point = NULL;
    }

    expr_free(scaled_value);
    expr_free(integral_at_point);
    expr_free(factor_at_point);
    expr_free(reciprocal_value);
    return constant;
}

de_attempt_t de_attempt_bernoulli_square(const diffequ_t *de, const expr_t *independent, const expr_t *dependent,
                                         const expr_t *derivative_right, equation_t **solution_out)
{
    const expr_t *dependent_square = de_find_dependent_square(derivative_right, dependent);
    expr_t *square_coefficient = NULL;
    expr_t *remaining = NULL;
    expr_t *linear_coefficient = NULL;
    expr_t *constant_term = NULL;
    expr_t *coefficient_integral = NULL;
    expr_t *integrating_factor = NULL;
    expr_t *negative_square_coefficient = NULL;
    expr_t *weighted_forcing = NULL;
    expr_t *forcing_integral = NULL;
    expr_t *constant = NULL;
    expr_t *transformed_solution = NULL;
    expr_t *solved_right = NULL;
    de_attempt_t attempt = DE_ATTEMPT_NOT_MATCHED;

    if (!dependent_square ||
        !de_linear_decompose(derivative_right, dependent_square, &square_coefficient, &remaining) ||
        !de_linear_decompose(remaining, dependent, &linear_coefficient, &constant_term))
        goto cleanup;
    if (!expr_is_exact_zero(constant_term) || de_expr_uses(square_coefficient, dependent) ||
        de_expr_uses(linear_coefficient, dependent))
        goto cleanup;
    attempt = DE_ATTEMPT_FAILED;
    if (de_has_zero_initial_condition(de, dependent)) {
        expr_t *zero = expr_const_zero();

        *solution_out = zero ? equ_new(dependent, zero) : NULL;
        expr_free(zero);
        if (*solution_out)
            attempt = DE_ATTEMPT_SOLVED;
        goto cleanup;
    }

    coefficient_integral = de_integrate_or_formal(linear_coefficient, independent);
    integrating_factor = coefficient_integral ? expr_exp(coefficient_integral) : NULL;
    negative_square_coefficient = de_simplify_unary_owned(square_coefficient, expr_neg);
    square_coefficient = NULL;
    weighted_forcing = integrating_factor
                           ? expr_mul_simplify_owned(expr_clone(integrating_factor), negative_square_coefficient)
                           : NULL;
    if (integrating_factor)
        negative_square_coefficient = NULL;
    forcing_integral = de_integrate_or_formal(weighted_forcing, independent);
    if (!integrating_factor || !forcing_integral)
        goto cleanup;

    constant =
        de_bernoulli_constant(de, dependent, independent, coefficient_integral, integrating_factor, forcing_integral);
    transformed_solution = constant ? expr_add_simplify_owned(forcing_integral, constant) : NULL;
    if (constant) {
        forcing_integral = NULL;
        constant = NULL;
    }
    transformed_solution =
        transformed_solution ? expr_div_simplify_owned(transformed_solution, integrating_factor) : NULL;
    integrating_factor = NULL;
    solved_right = transformed_solution ? expr_div_simplify_owned(expr_const_one(), transformed_solution) : NULL;
    transformed_solution = NULL;
    if (!solved_right)
        goto cleanup;

    *solution_out = equ_new(dependent, solved_right);
    if (*solution_out)
        attempt = DE_ATTEMPT_SOLVED;

cleanup:
    expr_free(solved_right);
    expr_free(transformed_solution);
    expr_free(constant);
    expr_free(forcing_integral);
    expr_free(weighted_forcing);
    expr_free(negative_square_coefficient);
    expr_free(integrating_factor);
    expr_free(coefficient_integral);
    expr_free(constant_term);
    expr_free(linear_coefficient);
    expr_free(remaining);
    expr_free(square_coefficient);
    return attempt;
}
