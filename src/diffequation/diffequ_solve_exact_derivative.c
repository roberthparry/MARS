#include <stdlib.h>

#define MARS_DIFFEQUATION_SOLVE_INTERNAL_ACCESS
#include "diffequ_solve_internal.h"

static expr_t *de_exact_formal_derivative(const expr_t *dependent, const expr_t *independent, size_t order)
{
    expr_t **wrts;
    expr_t *derivative;

    if (!dependent || !independent || order == 0u)
        return NULL;
    wrts = calloc(order, sizeof(*wrts));
    if (!wrts)
        return NULL;
    for (size_t i = 0u; i < order; ++i)
        wrts[i] = (expr_t *)independent;
    derivative = expr_new_formal_derivative(dependent, order, wrts);
    free(wrts);
    return derivative;
}

static bool de_exact_contains_formal_derivative(const expr_t *expr)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;

    if (!expr)
        return false;
    if (expr_is_formal_derivative(expr))
        return true;
    if (!expr_child_exprs(expr, &left, &right))
        return false;
    return de_exact_contains_formal_derivative(left) || de_exact_contains_formal_derivative(right);
}

static bool de_exact_is_numeric_constant(const expr_t *expr)
{
    number_t value = num_new();

    if (!expr_match_const_value(expr, &value)) {
        num_destroy(&value);
        return false;
    }
    num_destroy(&value);
    return true;
}

static bool de_exact_is_numeric_one(const expr_t *expr)
{
    number_t value = num_new();
    bool is_one;

    if (!expr_match_const_value(expr, &value)) {
        num_destroy(&value);
        return false;
    }
    is_one = num_eq(value, NUM_ONE);
    num_destroy(&value);
    return is_one;
}

static expr_t *de_exact_integrated_right(const expr_t *forcing, const expr_t *independent)
{
    expr_t *integral = expr_integrate(forcing, independent);
    expr_t *negative;
    expr_t *constant;
    expr_t *right;

    if (!integral)
        integral = expr_integral(forcing, independent);
    negative = de_simplify_unary_owned(integral, expr_neg);
    constant = expr_new_named_const(NUM_NAN, "C₁");
    right = negative && constant ? expr_add_simplify_owned(negative, constant) : NULL;
    if (right) {
        negative = NULL;
        constant = NULL;
    }
    expr_free(constant);
    expr_free(negative);
    return right;
}

de_attempt_t de_attempt_exact_derivative_linearization(const diffequ_t *de, const expr_t *independent,
                                                       const expr_t *dependent, const expr_t *first_derivative,
                                                       const expr_t *second_derivative, const expr_t *residual,
                                                       equation_t **solutions_out, size_t *solution_count_out)
{
    expr_t *third_derivative = NULL;
    expr_t *leading = NULL;
    expr_t *after_third = NULL;
    expr_t *mixed_coefficient = NULL;
    expr_t *forcing = NULL;
    expr_t *mixed_scale = NULL;
    expr_t *mixed_offset = NULL;
    expr_t *integrated_right = NULL;
    expr_t *auxiliary = NULL;
    expr_t *auxiliary_second = NULL;
    expr_t *absolute_auxiliary = NULL;
    expr_t *log_auxiliary = NULL;
    expr_t *dependent_scale = NULL;
    expr_t *dependent_right = NULL;
    expr_t *leading_square = NULL;
    expr_t *linear_scale_denominator = NULL;
    expr_t *linear_scale = NULL;
    expr_t *linear_right = NULL;
    expr_t *canonical_parameter = NULL;
    expr_t *cubic_term = NULL;
    expr_t *cubic_coefficient = NULL;
    expr_t *cubic_remainder = NULL;
    expr_t *scaled_cubic_coefficient = NULL;
    expr_t *transformed_solution = NULL;
    equation_t *dependent_solution = NULL;
    de_attempt_t attempt = DE_ATTEMPT_NOT_MATCHED;

    if (!de || !independent || !dependent || !first_derivative || !second_derivative || !residual || !solutions_out ||
        !solution_count_out || de->condition_count != 0u)
        return DE_ATTEMPT_NOT_MATCHED;
    *solution_count_out = 0u;

    third_derivative = de_exact_formal_derivative(dependent, independent, 3u);
    if (!third_derivative || !de_linear_decompose(residual, third_derivative, &leading, &after_third) || !leading ||
        !de_exact_is_numeric_constant(leading) || expr_is_exact_zero(leading) ||
        !de_linear_decompose(after_third, second_derivative, &mixed_coefficient, &forcing) ||
        !de_linear_decompose(mixed_coefficient, first_derivative, &mixed_scale, &mixed_offset) || !mixed_scale ||
        !de_exact_is_numeric_constant(mixed_scale) || expr_is_exact_zero(mixed_scale) || !mixed_offset ||
        !expr_is_exact_zero(mixed_offset) || de_expr_uses(forcing, dependent) ||
        de_exact_contains_formal_derivative(forcing))
        goto cleanup;

    attempt = DE_ATTEMPT_FAILED;
    integrated_right = de_exact_integrated_right(forcing, independent);
    auxiliary = expr_new_named_var(NUM_NAN, "u");
    auxiliary_second = de_exact_formal_derivative(auxiliary, independent, 2u);
    absolute_auxiliary = auxiliary ? expr_abs(auxiliary) : NULL;
    log_auxiliary = absolute_auxiliary ? expr_log(absolute_auxiliary) : NULL;
    dependent_scale = expr_div_simplify_owned(expr_mul_long(leading, 2L), expr_clone(mixed_scale));
    dependent_right = dependent_scale && log_auxiliary ? expr_mul_simplify_owned(dependent_scale, log_auxiliary) : NULL;
    if (dependent_right) {
        dependent_scale = NULL;
        log_auxiliary = NULL;
    }

    leading_square = expr_pow_long(leading, 2L);
    linear_scale_denominator = leading_square ? expr_mul_long(leading_square, 2L) : NULL;
    linear_scale =
        linear_scale_denominator ? expr_div_simplify_owned(expr_clone(mixed_scale), linear_scale_denominator) : NULL;
    if (linear_scale)
        linear_scale_denominator = NULL;
    cubic_term = expr_pow_long(independent, 3L);
    if (!cubic_term || !de_linear_decompose(integrated_right, cubic_term, &cubic_coefficient, &cubic_remainder))
        goto cleanup;
    scaled_cubic_coefficient =
        linear_scale && cubic_coefficient
            ? expr_mul_simplify_owned(expr_new_const(NUM_TWO),
                                      expr_mul_simplify_owned(expr_clone(linear_scale), expr_clone(cubic_coefficient)))
            : NULL;
    canonical_parameter =
        linear_scale && cubic_remainder
            ? expr_mul_simplify_owned(expr_new_const(NUM_TWO),
                                      expr_mul_simplify_owned(expr_clone(linear_scale), expr_clone(cubic_remainder)))
            : NULL;
    linear_right = linear_scale && integrated_right && auxiliary
                       ? expr_mul_simplify_owned(expr_mul_simplify_owned(linear_scale, expr_clone(integrated_right)),
                                                 expr_clone(auxiliary))
                       : NULL;
    if (linear_right)
        linear_scale = NULL;

    if (!integrated_right || !auxiliary || !auxiliary_second || !dependent_right || !linear_right ||
        !canonical_parameter || !scaled_cubic_coefficient || !de_exact_is_numeric_one(scaled_cubic_coefficient) ||
        de_expr_uses(canonical_parameter, independent))
        goto cleanup;

    /*
     * Hand the reduced self-adjoint equation and its canonical initial
     * conditions to the Sturm-Liouville subsystem.  It returns the exact
     * fundamental basis and the equations that define that basis.
     */
    if (de_sturm_liouville_cubic_basis(independent, canonical_parameter, auxiliary, solutions_out,
                                       solution_count_out) != 0)
        goto cleanup;
    transformed_solution = expr_substitute(dependent_right, auxiliary, equ_rhs(solutions_out[0]));
    dependent_solution = transformed_solution ? equ_new(dependent, transformed_solution) : NULL;
    if (!dependent_solution)
        goto cleanup;
    equ_free(solutions_out[0]);
    solutions_out[0] = dependent_solution;
    dependent_solution = NULL;
    attempt = DE_ATTEMPT_SOLVED;

cleanup:
    equ_free(dependent_solution);
    expr_free(transformed_solution);
    expr_free(scaled_cubic_coefficient);
    expr_free(cubic_remainder);
    expr_free(cubic_coefficient);
    expr_free(cubic_term);
    expr_free(canonical_parameter);
    expr_free(linear_right);
    expr_free(linear_scale);
    expr_free(linear_scale_denominator);
    expr_free(leading_square);
    expr_free(dependent_right);
    expr_free(dependent_scale);
    expr_free(log_auxiliary);
    expr_free(absolute_auxiliary);
    expr_free(auxiliary_second);
    expr_free(auxiliary);
    expr_free(integrated_right);
    expr_free(mixed_offset);
    expr_free(mixed_scale);
    expr_free(forcing);
    expr_free(mixed_coefficient);
    expr_free(after_third);
    expr_free(leading);
    expr_free(third_derivative);
    return attempt;
}
