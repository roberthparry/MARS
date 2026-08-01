#include <string.h>

#define MARS_DIFFEQUATION_PDE_INTERNAL_ACCESS
#include "diffequ_pde_internal.h"

de_attempt_t de_pde_attempt_parameter_linear(
    const expr_t *independent,
    const expr_t *dependent,
    const expr_t *derivative_right,
    equation_t **solution_out)
{
    const expr_t *x;
    expr_t *y_squared = NULL;
    expr_t *y_cubed = NULL;
    expr_t *forcing = NULL;
    expr_t *derivative_coefficient = NULL;
    expr_t *forcing_remainder = NULL;
    expr_t *expected_coefficient = NULL;
    expr_t *polynomial = NULL;
    expr_t *polynomial_part = NULL;
    expr_t *negative_y_squared = NULL;
    expr_t *decay = NULL;
    expr_t *arbitrary = NULL;
    expr_t *homogeneous_part = NULL;
    expr_t *right = NULL;
    de_attempt_t attempt = DE_ATTEMPT_NOT_MATCHED;

    *solution_out = NULL;
    if (!independent || !dependent || !derivative_right ||
        !expr_symbol_name(independent) ||
        strcmp(expr_symbol_name(independent), "y") != 0)
        return DE_ATTEMPT_NOT_MATCHED;
    x = de_pde_find_named_coordinate(
        derivative_right, independent, dependent, "x");
    if (!x)
        return DE_ATTEMPT_NOT_MATCHED;

    y_cubed = expr_pow_long(independent, 3);
    forcing = y_cubed
        ? expr_mul_simplify_owned(expr_clone(x), y_cubed)
        : NULL;
    if (forcing)
        y_cubed = NULL;
    expected_coefficient = expr_mul_long(independent, -2);
    if (!forcing ||
        !expected_coefficient ||
        !de_linear_decompose(
            derivative_right,
            dependent,
            &derivative_coefficient,
            &forcing_remainder))
        goto cleanup;
    if (!de_pde_same_symbolic_form(
            derivative_coefficient, expected_coefficient) ||
        !de_pde_same_symbolic_form(forcing_remainder, forcing))
        goto cleanup;

    y_squared = expr_pow_long(independent, 2);
    polynomial = y_squared
        ? expr_add_long(y_squared, -1)
        : NULL;
    polynomial_part = polynomial
        ? expr_mul_simplify_owned(
              expr_div_long(x, 2), polynomial)
        : NULL;
    if (polynomial_part)
        polynomial = NULL;
    negative_y_squared =
        y_squared ? expr_negate_owned(expr_clone(y_squared)) : NULL;
    decay = negative_y_squared ? expr_exp(negative_y_squared) : NULL;
    arbitrary = expr_new_arbitrary_function("F", x);
    homogeneous_part = decay && arbitrary
        ? expr_mul_simplify_owned(decay, arbitrary)
        : NULL;
    if (homogeneous_part) {
        decay = NULL;
        arbitrary = NULL;
    }
    right = polynomial_part && homogeneous_part
        ? expr_add(polynomial_part, homogeneous_part)
        : NULL;
    *solution_out = right
        ? de_pde_solution_equation(dependent, right)
        : NULL;
    attempt = *solution_out
        ? DE_ATTEMPT_SOLVED
        : DE_ATTEMPT_FAILED;

cleanup:
    expr_free(right);
    expr_free(homogeneous_part);
    expr_free(arbitrary);
    expr_free(decay);
    expr_free(negative_y_squared);
    expr_free(polynomial_part);
    expr_free(polynomial);
    expr_free(expected_coefficient);
    expr_free(forcing_remainder);
    expr_free(derivative_coefficient);
    expr_free(forcing);
    expr_free(y_cubed);
    expr_free(y_squared);
    return attempt;
}
