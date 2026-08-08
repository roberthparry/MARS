#include <stdbool.h>
#include <string.h>

#define MARS_DIFFEQUATION_PDE_INTERNAL_ACCESS
#include "diffequ_pde_internal.h"

static bool de_pde_find_pure_second_derivatives(
    const expr_t *expr,
    const expr_t *x,
    const expr_t *y,
    const expr_t **dependent_out,
    const expr_t **dxx_out,
    const expr_t **dyy_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;

    if (!expr)
        return true;
    if (expr_is_formal_derivative(expr)) {
        const expr_t *dependent;
        const expr_t *first_wrt;
        const expr_t *second_wrt;
        const expr_t **slot;

        if (expr_formal_derivative_order(expr) != 2u)
            return false;
        dependent = expr_formal_derivative_dependent(expr);
        first_wrt = expr_formal_derivative_wrt_at(expr, 0u);
        second_wrt = expr_formal_derivative_wrt_at(expr, 1u);
        if (!dependent || !first_wrt || !second_wrt ||
            !expr_struct_eq(first_wrt, second_wrt))
            return false;
        if (expr_struct_eq(first_wrt, x))
            slot = dxx_out;
        else if (expr_struct_eq(first_wrt, y))
            slot = dyy_out;
        else
            return false;
        if ((*dependent_out &&
             !expr_struct_eq(*dependent_out, dependent)) ||
            (*slot && !expr_struct_eq(*slot, expr)))
            return false;
        *dependent_out = dependent;
        *slot = expr;
        return true;
    }

    if (!expr_child_exprs(expr, &left, &right))
        return true;
    return de_pde_find_pure_second_derivatives(
               left, x, y, dependent_out, dxx_out, dyy_out) &&
           de_pde_find_pure_second_derivatives(
               right, x, y, dependent_out, dxx_out, dyy_out);
}

static bool de_pde_is_equal_nonzero_numeric_coefficient(
    const expr_t *left,
    const expr_t *right)
{
    number_t left_value = num_new();
    number_t right_value = num_new();
    bool matches = expr_match_const_value(left, &left_value) &&
        expr_match_const_value(right, &right_value) &&
        !num_is_zero(left_value) &&
        num_eq(left_value, right_value);

    num_destroy(&right_value);
    num_destroy(&left_value);
    return matches;
}

static bool de_pde_is_nonzero_numeric_coefficient(const expr_t *expr)
{
    number_t value = num_new();
    bool matches = expr_match_const_value(expr, &value) &&
        !num_is_zero(value);

    num_destroy(&value);
    return matches;
}

static bool de_pde_laplace_solution_verifies(
    const expr_t *right,
    const expr_t *x,
    const expr_t *y)
{
    expr_t *first_x = right ? expr_create_deriv(right, x) : NULL;
    expr_t *second_x = first_x ? expr_create_deriv(first_x, x) : NULL;
    expr_t *first_y = right ? expr_create_deriv(right, y) : NULL;
    expr_t *second_y = first_y ? expr_create_deriv(first_y, y) : NULL;
    expr_t *sum_raw = second_x && second_y
        ? expr_add(second_x, second_y)
        : NULL;
    expr_t *sum = sum_raw ? expr_simplify(sum_raw) : NULL;
    bool verifies = sum && expr_is_exact_zero(sum);

    expr_free(sum);
    expr_free(sum_raw);
    expr_free(second_y);
    expr_free(first_y);
    expr_free(second_x);
    expr_free(first_x);
    return verifies;
}

static expr_t *de_pde_laplace_general_family(
    const expr_t *x,
    const expr_t *y)
{
    expr_t *imaginary_unit = expr_new_named_const(NUM_I, "i");
    expr_t *imaginary_y = imaginary_unit
        ? expr_mul_simplify_owned(
              expr_clone(imaginary_unit), expr_clone(y))
        : NULL;
    expr_t *plus_argument = imaginary_y ? expr_add(x, imaginary_y) : NULL;
    expr_t *minus_argument = imaginary_y ? expr_sub(x, imaginary_y) : NULL;
    expr_t *first = plus_argument
        ? expr_new_arbitrary_function("F", plus_argument)
        : NULL;
    expr_t *second = minus_argument
        ? expr_new_arbitrary_function("G", minus_argument)
        : NULL;
    expr_t *right = NULL;

    if (first && second) {
        right = expr_add_simplify_owned(first, second);
        first = NULL;
        second = NULL;
    }
    expr_free(second);
    expr_free(first);
    expr_free(minus_argument);
    expr_free(plus_argument);
    expr_free(imaginary_y);
    expr_free(imaginary_unit);
    return right;
}

static bool de_pde_find_polar_laplace_derivatives(
    const expr_t *expr,
    const expr_t *radius,
    const expr_t *angle,
    const expr_t **dependent_out,
    const expr_t **radial_first_out,
    const expr_t **radial_second_out,
    const expr_t **angular_second_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;

    if (!expr)
        return true;
    if (expr_is_formal_derivative(expr)) {
        const expr_t *dependent =
            expr_formal_derivative_dependent(expr);
        const expr_t *first_wrt =
            expr_formal_derivative_wrt_at(expr, 0u);
        size_t order = expr_formal_derivative_order(expr);
        const expr_t **slot;

        if (!dependent || !first_wrt ||
            (order != 1u && order != 2u))
            return false;
        if (order == 1u && expr_struct_eq(first_wrt, radius)) {
            slot = radial_first_out;
        } else if (order == 2u &&
                   expr_struct_eq(
                       first_wrt,
                       expr_formal_derivative_wrt_at(expr, 1u))) {
            if (expr_struct_eq(first_wrt, radius))
                slot = radial_second_out;
            else if (expr_struct_eq(first_wrt, angle))
                slot = angular_second_out;
            else
                return false;
        } else {
            return false;
        }
        if ((*dependent_out &&
             !expr_struct_eq(*dependent_out, dependent)) ||
            (*slot && !expr_struct_eq(*slot, expr)))
            return false;
        *dependent_out = dependent;
        *slot = expr;
        return true;
    }

    if (!expr_child_exprs(expr, &left, &right))
        return true;
    return de_pde_find_polar_laplace_derivatives(
               left,
               radius,
               angle,
               dependent_out,
               radial_first_out,
               radial_second_out,
               angular_second_out) &&
           de_pde_find_polar_laplace_derivatives(
               right,
               radius,
               angle,
               dependent_out,
               radial_first_out,
               radial_second_out,
               angular_second_out);
}

static expr_t *de_pde_polar_coordinate(
    const expr_t *radius,
    const expr_t *angle,
    bool negative)
{
    expr_t *imaginary_unit = expr_new_named_const(NUM_I, "i");
    expr_t *imaginary_angle = imaginary_unit
        ? expr_mul_simplify_owned(
              expr_clone(imaginary_unit), expr_clone(angle))
        : NULL;
    expr_t *exponent = negative && imaginary_angle
        ? expr_negate_owned(expr_clone(imaginary_angle))
        : expr_clone(imaginary_angle);
    expr_t *exponential = exponent ? expr_exp(exponent) : NULL;
    expr_t *coordinate = exponential
        ? expr_mul_simplify_owned(
              expr_clone(radius), expr_clone(exponential))
        : NULL;

    expr_free(exponential);
    expr_free(exponent);
    expr_free(imaginary_angle);
    expr_free(imaginary_unit);
    return coordinate;
}

static expr_t *de_pde_polar_laplace_general_family(
    const expr_t *radius,
    const expr_t *angle)
{
    expr_t *plus_argument =
        de_pde_polar_coordinate(radius, angle, false);
    expr_t *minus_argument =
        de_pde_polar_coordinate(radius, angle, true);
    expr_t *first = plus_argument
        ? expr_new_arbitrary_function("F", plus_argument)
        : NULL;
    expr_t *second = minus_argument
        ? expr_new_arbitrary_function("G", minus_argument)
        : NULL;
    expr_t *right = NULL;

    if (first && second) {
        right = expr_add_simplify_owned(first, second);
        first = NULL;
        second = NULL;
    }
    expr_free(second);
    expr_free(first);
    expr_free(minus_argument);
    expr_free(plus_argument);
    return right;
}

static bool de_pde_polar_coordinate_verifies(
    const expr_t *coordinate,
    const expr_t *radius,
    const expr_t *angle)
{
    expr_t *radial_first = coordinate
        ? expr_create_deriv(coordinate, radius)
        : NULL;
    expr_t *radial_second = radial_first
        ? expr_create_deriv(radial_first, radius)
        : NULL;
    expr_t *angular_first = coordinate
        ? expr_create_deriv(coordinate, angle)
        : NULL;
    expr_t *angular_second = angular_first
        ? expr_create_deriv(angular_first, angle)
        : NULL;
    expr_t *radius_squared = expr_pow_long(radius, 2);
    expr_t *radial_first_squared = radial_first
        ? expr_pow_long(radial_first, 2)
        : NULL;
    expr_t *angular_first_squared = angular_first
        ? expr_pow_long(angular_first, 2)
        : NULL;
    expr_t *first_identity_radial =
        radius_squared && radial_first_squared
        ? expr_mul(radius_squared, radial_first_squared)
        : NULL;
    expr_t *first_identity_raw =
        first_identity_radial && angular_first_squared
        ? expr_add(first_identity_radial, angular_first_squared)
        : NULL;
    expr_t *first_identity = first_identity_raw
        ? expr_simplify(first_identity_raw)
        : NULL;
    expr_t *second_identity_radial =
        radius_squared && radial_second
        ? expr_mul(radius_squared, radial_second)
        : NULL;
    expr_t *second_identity_first = radial_first
        ? expr_mul(radius, radial_first)
        : NULL;
    expr_t *second_identity_partial =
        second_identity_radial && second_identity_first
        ? expr_add(
              second_identity_radial, second_identity_first)
        : NULL;
    expr_t *second_identity_raw =
        second_identity_partial && angular_second
        ? expr_add(second_identity_partial, angular_second)
        : NULL;
    expr_t *second_identity = second_identity_raw
        ? expr_simplify(second_identity_raw)
        : NULL;
    bool verifies = first_identity && second_identity &&
        expr_is_exact_zero(first_identity) &&
        expr_is_exact_zero(second_identity);

    expr_free(second_identity);
    expr_free(second_identity_raw);
    expr_free(second_identity_partial);
    expr_free(second_identity_first);
    expr_free(second_identity_radial);
    expr_free(first_identity);
    expr_free(first_identity_raw);
    expr_free(first_identity_radial);
    expr_free(angular_first_squared);
    expr_free(radial_first_squared);
    expr_free(radius_squared);
    expr_free(angular_second);
    expr_free(angular_first);
    expr_free(radial_second);
    expr_free(radial_first);
    return verifies;
}

static bool de_pde_polar_laplace_solution_verifies(
    const expr_t *radius,
    const expr_t *angle)
{
    expr_t *positive =
        de_pde_polar_coordinate(radius, angle, false);
    expr_t *negative =
        de_pde_polar_coordinate(radius, angle, true);
    bool verifies = positive && negative &&
        de_pde_polar_coordinate_verifies(positive, radius, angle) &&
        de_pde_polar_coordinate_verifies(negative, radius, angle);

    expr_free(negative);
    expr_free(positive);
    return verifies;
}

de_attempt_t de_pde_attempt_laplace(
    const diffequ_t *de,
    const expr_t *residual,
    equation_t **solution_out)
{
    const expr_t *x;
    const expr_t *y;
    const expr_t *dependent = NULL;
    const expr_t *dxx = NULL;
    const expr_t *dyy = NULL;
    expr_t *x_coefficient = NULL;
    expr_t *after_x = NULL;
    expr_t *y_coefficient = NULL;
    expr_t *remainder = NULL;
    expr_t *right = NULL;
    de_attempt_t attempt = DE_ATTEMPT_NOT_MATCHED;

    if (solution_out)
        *solution_out = NULL;
    if (!de || !residual || !solution_out ||
        de->independent_count != 2u || de->condition_count != 0u)
        return DE_ATTEMPT_NOT_MATCHED;
    x = de->independent_vars[0];
    y = de->independent_vars[1];
    if (!x || !y ||
        !de_pde_find_pure_second_derivatives(
            residual, x, y, &dependent, &dxx, &dyy) ||
        !dependent || !dxx || !dyy)
        return DE_ATTEMPT_NOT_MATCHED;
    if (!de_linear_decompose(
            residual, dxx, &x_coefficient, &after_x) ||
        !de_linear_decompose(
            after_x, dyy, &y_coefficient, &remainder)) {
        attempt = DE_ATTEMPT_FAILED;
        goto cleanup;
    }
    if (!expr_is_exact_zero(remainder) ||
        !de_pde_is_equal_nonzero_numeric_coefficient(
            x_coefficient, y_coefficient))
        goto cleanup;

    right = de_pde_laplace_general_family(x, y);
    if (!right || !de_pde_laplace_solution_verifies(right, x, y)) {
        attempt = DE_ATTEMPT_FAILED;
        goto cleanup;
    }
    *solution_out = de_pde_solution_equation(dependent, right);
    attempt = *solution_out ? DE_ATTEMPT_SOLVED : DE_ATTEMPT_FAILED;

cleanup:
    expr_free(right);
    expr_free(remainder);
    expr_free(y_coefficient);
    expr_free(after_x);
    expr_free(x_coefficient);
    return attempt;
}

de_attempt_t de_pde_attempt_polar_laplace(
    const diffequ_t *de,
    const expr_t *residual,
    equation_t **solution_out)
{
    const expr_t *radius = NULL;
    const expr_t *angle = NULL;
    const expr_t *dependent = NULL;
    const expr_t *radial_first = NULL;
    const expr_t *radial_second = NULL;
    const expr_t *angular_second = NULL;
    expr_t *radial_second_coefficient = NULL;
    expr_t *after_radial_second = NULL;
    expr_t *radial_first_coefficient = NULL;
    expr_t *after_radial_first = NULL;
    expr_t *angular_second_coefficient = NULL;
    expr_t *remainder = NULL;
    expr_t *expected_radial_first = NULL;
    expr_t *expected_angular_second = NULL;
    expr_t *radius_squared = NULL;
    expr_t *right = NULL;
    de_attempt_t attempt = DE_ATTEMPT_NOT_MATCHED;

    if (solution_out)
        *solution_out = NULL;
    if (!de || !residual || !solution_out ||
        de->independent_count != 2u || de->condition_count != 0u)
        return DE_ATTEMPT_NOT_MATCHED;
    for (size_t i = 0u; i < de->independent_count; ++i) {
        const char *name = expr_symbol_name(de->independent_vars[i]);

        if (name && strcmp(name, "r") == 0)
            radius = de->independent_vars[i];
        else if (name && strcmp(name, "θ") == 0)
            angle = de->independent_vars[i];
    }
    if (!radius || !angle ||
        !de_pde_find_polar_laplace_derivatives(
            residual,
            radius,
            angle,
            &dependent,
            &radial_first,
            &radial_second,
            &angular_second) ||
        !dependent || !radial_first ||
        !radial_second || !angular_second)
        return DE_ATTEMPT_NOT_MATCHED;
    if (!de_linear_decompose(
            residual,
            radial_second,
            &radial_second_coefficient,
            &after_radial_second) ||
        !de_linear_decompose(
            after_radial_second,
            radial_first,
            &radial_first_coefficient,
            &after_radial_first) ||
        !de_linear_decompose(
            after_radial_first,
            angular_second,
            &angular_second_coefficient,
            &remainder)) {
        attempt = DE_ATTEMPT_FAILED;
        goto cleanup;
    }
    radius_squared = expr_pow_long(radius, 2);
    expected_radial_first =
        radial_second_coefficient && radius
        ? expr_div_simplify_owned(
              expr_clone(radial_second_coefficient),
              expr_clone(radius))
        : NULL;
    expected_angular_second =
        radial_second_coefficient && radius_squared
        ? expr_div_simplify_owned(
              expr_clone(radial_second_coefficient),
              expr_clone(radius_squared))
        : NULL;
    if (!expr_is_exact_zero(remainder) ||
        !de_pde_is_nonzero_numeric_coefficient(
            radial_second_coefficient) ||
        !de_pde_same_symbolic_form(
            radial_first_coefficient, expected_radial_first) ||
        !de_pde_same_symbolic_form(
            angular_second_coefficient, expected_angular_second)) {
        goto cleanup;
    }

    right = de_pde_polar_laplace_general_family(radius, angle);
    if (!right ||
        !de_pde_polar_laplace_solution_verifies(radius, angle)) {
        attempt = DE_ATTEMPT_FAILED;
        goto cleanup;
    }
    *solution_out = de_pde_solution_equation(dependent, right);
    attempt = *solution_out ? DE_ATTEMPT_SOLVED : DE_ATTEMPT_FAILED;

cleanup:
    expr_free(right);
    expr_free(radius_squared);
    expr_free(expected_angular_second);
    expr_free(expected_radial_first);
    expr_free(remainder);
    expr_free(angular_second_coefficient);
    expr_free(after_radial_first);
    expr_free(radial_first_coefficient);
    expr_free(after_radial_second);
    expr_free(radial_second_coefficient);
    return attempt;
}
