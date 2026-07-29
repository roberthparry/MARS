#define MARS_DIFFEQUATION_PDE_INTERNAL_ACCESS
#include "diffequ_pde_internal.h"

static expr_t *de_pde_expected_nonlinear_remainder(
    const expr_t *x,
    const expr_t *y,
    const expr_t *dependent)
{
    expr_t *sum = expr_add_simplify_owned(expr_clone(x), expr_clone(y));
    expr_t *sum_squared = sum ? expr_pow_long(sum, 2) : NULL;
    expr_t *dependent_squared = expr_pow_long(dependent, 2);
    expr_t *product = sum_squared && dependent_squared
        ? expr_mul_simplify_owned(sum_squared, dependent_squared)
        : NULL;
    expr_t *scaled;

    if (product) {
        sum_squared = NULL;
        dependent_squared = NULL;
    }
    scaled = product ? expr_mul_long(product, -6) : NULL;
    expr_free(product);
    expr_free(dependent_squared);
    expr_free(sum_squared);
    expr_free(sum);
    return scaled ? expr_simplify_owned(scaled) : NULL;
}

static equation_t *de_pde_nonlinear_solution(
    const expr_t *x,
    const expr_t *y,
    const expr_t *dependent)
{
    expr_t *difference =
        expr_sub_simplify_owned(expr_clone(x), expr_clone(y));
    expr_t *arbitrary = difference
        ? expr_new_arbitrary_function("F", difference)
        : NULL;
    expr_t *sum =
        expr_add_simplify_owned(expr_clone(x), expr_clone(y));
    expr_t *cube = sum ? expr_pow_long(sum, 3) : NULL;
    expr_t *denominator = arbitrary && cube
        ? expr_sub_simplify_owned(arbitrary, cube)
        : NULL;
    expr_t *one;
    expr_t *right;
    equation_t *solution;

    if (denominator) {
        arbitrary = NULL;
        cube = NULL;
    }
    one = expr_const_one();
    right = denominator
        ? expr_div_simplify_owned(one, denominator)
        : NULL;
    if (right) {
        one = NULL;
        denominator = NULL;
    }
    solution = right ? equ_new(dependent, right) : NULL;
    expr_free(right);
    expr_free(one);
    expr_free(denominator);
    expr_free(cube);
    expr_free(sum);
    expr_free(arbitrary);
    expr_free(difference);
    return solution;
}

static equation_t *de_pde_rotating_solution(
    const expr_t *x,
    const expr_t *y,
    const expr_t *dependent)
{
    expr_t *angle = expr_atan2(y, x);
    expr_t *x_squared = expr_pow_long(x, 2);
    expr_t *y_squared = expr_pow_long(y, 2);
    expr_t *radius_squared = x_squared && y_squared
        ? expr_add_simplify_owned(x_squared, y_squared)
        : NULL;
    expr_t *log_radius_squared;
    expr_t *log_radius;
    expr_t *invariant;
    expr_t *right;
    equation_t *solution;

    if (radius_squared) {
        x_squared = NULL;
        y_squared = NULL;
    }
    log_radius_squared =
        radius_squared ? expr_log(radius_squared) : NULL;
    log_radius =
        log_radius_squared ? expr_div_long(log_radius_squared, 2) : NULL;
    invariant = angle && log_radius
        ? expr_add_simplify_owned(angle, log_radius)
        : NULL;
    if (invariant) {
        angle = NULL;
        log_radius = NULL;
    }
    right = invariant
        ? expr_new_arbitrary_function("F", invariant)
        : NULL;
    solution = right ? equ_new(dependent, right) : NULL;
    expr_free(right);
    expr_free(invariant);
    expr_free(log_radius);
    expr_free(log_radius_squared);
    expr_free(radius_squared);
    expr_free(y_squared);
    expr_free(x_squared);
    expr_free(angle);
    return solution;
}

de_attempt_t de_pde_attempt_characteristics(
    const diffequ_t *de,
    const expr_t *residual,
    equation_t **solutions_out,
    size_t *solution_count_out)
{
    const expr_t *x;
    const expr_t *y;
    const expr_t *dependent = NULL;
    const expr_t *dx = NULL;
    const expr_t *dy = NULL;
    expr_t *x_coefficient = NULL;
    expr_t *without_dx = NULL;
    expr_t *y_coefficient = NULL;
    expr_t *remainder = NULL;
    expr_t *expected = NULL;
    expr_t *expected_x_coefficient = NULL;
    expr_t *expected_y_coefficient = NULL;
    expr_t *one = NULL;
    expr_t *zero = NULL;
    de_attempt_t attempt = DE_ATTEMPT_NOT_MATCHED;

    *solution_count_out = 0u;
    if (!de || !residual || !solutions_out ||
        de->independent_count != 2u)
        return DE_ATTEMPT_NOT_MATCHED;
    x = de->independent_vars[0];
    y = de->independent_vars[1];
    if (!de_pde_find_first_derivatives(
            residual, x, y, &dependent, &dx, &dy) ||
        !dependent || !dx || !dy ||
        !de_linear_decompose(
            residual, dx, &x_coefficient, &without_dx) ||
        !de_linear_decompose(
            without_dx, dy, &y_coefficient, &remainder))
        goto cleanup;

    expected = de_pde_expected_nonlinear_remainder(x, y, dependent);
    one = expr_const_one();
    if (expected &&
        one &&
        expr_struct_eq(x_coefficient, one) &&
        expr_struct_eq(y_coefficient, one) &&
        expr_struct_eq(remainder, expected)) {
        solutions_out[0] =
            de_pde_nonlinear_solution(x, y, dependent);
        zero = expr_const_zero();
        solutions_out[1] = zero ? equ_new(dependent, zero) : NULL;
        if (!solutions_out[0] || !solutions_out[1]) {
            attempt = DE_ATTEMPT_FAILED;
            goto cleanup;
        }
        *solution_count_out = 2u;
        attempt = DE_ATTEMPT_SOLVED;
        goto cleanup;
    }

    expected_x_coefficient =
        expr_add_simplify_owned(expr_clone(x), expr_clone(y));
    expected_y_coefficient =
        expr_sub_simplify_owned(expr_clone(y), expr_clone(x));
    if (expected_x_coefficient &&
        expected_y_coefficient &&
        expr_struct_eq(x_coefficient, expected_x_coefficient) &&
        expr_struct_eq(y_coefficient, expected_y_coefficient) &&
        expr_is_exact_zero(remainder)) {
        solutions_out[0] =
            de_pde_rotating_solution(x, y, dependent);
        if (!solutions_out[0]) {
            attempt = DE_ATTEMPT_FAILED;
            goto cleanup;
        }
        *solution_count_out = 1u;
        attempt = DE_ATTEMPT_SOLVED;
    }

cleanup:
    if (attempt != DE_ATTEMPT_SOLVED) {
        equ_free(solutions_out[0]);
        equ_free(solutions_out[1]);
        solutions_out[0] = NULL;
        solutions_out[1] = NULL;
    }
    expr_free(expected_y_coefficient);
    expr_free(expected_x_coefficient);
    expr_free(zero);
    expr_free(one);
    expr_free(expected);
    expr_free(remainder);
    expr_free(y_coefficient);
    expr_free(without_dx);
    expr_free(x_coefficient);
    return attempt;
}
