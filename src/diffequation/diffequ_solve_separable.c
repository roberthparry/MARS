#include <stdbool.h>

#define MARS_DIFFEQUATION_SOLVE_INTERNAL_ACCESS
#include "diffequ_solve_internal.h"

static expr_t *de_separable_div_owned(expr_t *numerator, expr_t *denominator)
{
    number_t value;
    expr_t *quotient;

    if (!numerator || !denominator) {
        expr_free(denominator);
        expr_free(numerator);
        return NULL;
    }
    if (expr_match_const_value(denominator, &value) && num_eq(value, NUM_ONE)) {
        expr_free(denominator);
        return numerator;
    }
    quotient = expr_div(numerator, denominator);
    expr_free(denominator);
    expr_free(numerator);
    return quotient;
}

bool de_split_separable(const expr_t *expr, const expr_t *independent, const expr_t *dependent,
                        expr_t **independent_factor_out, expr_t **dependent_factor_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    bool uses_independent = de_expr_uses(expr, independent);
    bool uses_dependent = de_expr_uses(expr, dependent);

    *independent_factor_out = NULL;
    *dependent_factor_out = NULL;

    if (!uses_dependent) {
        *independent_factor_out = expr_clone(expr);
        *dependent_factor_out = expr_const_one();
        return *independent_factor_out && *dependent_factor_out;
    }
    if (!uses_independent) {
        *independent_factor_out = expr_const_one();
        *dependent_factor_out = expr_clone(expr);
        return *independent_factor_out && *dependent_factor_out;
    }

    if (expr_match_neg_expr(expr, &left)) {
        expr_t *independent_factor = NULL;
        expr_t *dependent_factor = NULL;

        if (!de_split_separable(left, independent, dependent, &independent_factor, &dependent_factor))
            return false;
        *independent_factor_out = de_simplify_unary_owned(independent_factor, expr_neg);
        *dependent_factor_out = dependent_factor;
        return *independent_factor_out && *dependent_factor_out;
    }

    {
        bool division = false;
        bool product = expr_match_mul_expr(expr, &left, &right);

        if (!product)
            division = expr_match_div_expr(expr, &left, &right);
        if (product || division) {
            expr_t *left_independent = NULL;
            expr_t *left_dependent = NULL;
            expr_t *right_independent = NULL;
            expr_t *right_dependent = NULL;

            if (!de_split_separable(left, independent, dependent, &left_independent, &left_dependent) ||
                !de_split_separable(right, independent, dependent, &right_independent, &right_dependent))
                goto factor_fail;

            *independent_factor_out = division ? de_separable_div_owned(left_independent, right_independent)
                                               : expr_mul_simplify_owned(left_independent, right_independent);
            left_independent = NULL;
            right_independent = NULL;
            *dependent_factor_out = division ? de_separable_div_owned(left_dependent, right_dependent)
                                             : expr_mul_simplify_owned(left_dependent, right_dependent);
            left_dependent = NULL;
            right_dependent = NULL;

        factor_fail:
            expr_free(right_dependent);
            expr_free(right_independent);
            expr_free(left_dependent);
            expr_free(left_independent);
            if (*independent_factor_out && *dependent_factor_out)
                return true;
            expr_free(*dependent_factor_out);
            expr_free(*independent_factor_out);
            *dependent_factor_out = NULL;
            *independent_factor_out = NULL;
        }
    }

    return false;
}

static expr_t *de_separable_constant(const diffequ_t *de, const expr_t *dependent, const expr_t *independent,
                                     const expr_t *dependent_integral, const expr_t *independent_integral)
{
    const expr_t *point = NULL;
    const expr_t *value = NULL;
    expr_t *dependent_at_value;
    expr_t *independent_at_point;
    expr_t *constant;

    if (!de_find_initial_condition(de, dependent, &point, &value))
        return de_arbitrary_constant();

    dependent_at_value = expr_substitute(dependent_integral, dependent, value);
    independent_at_point = expr_substitute(independent_integral, independent, point);
    constant = dependent_at_value && independent_at_point
                   ? expr_sub_simplify_owned(dependent_at_value, independent_at_point)
                   : NULL;
    if (dependent_at_value && independent_at_point) {
        dependent_at_value = NULL;
        independent_at_point = NULL;
    }
    expr_free(independent_at_point);
    expr_free(dependent_at_value);
    return constant;
}

static expr_t *de_invert_separable_integral(const expr_t *integral, const expr_t *dependent, const expr_t *right)
{
    const expr_t *argument = NULL;

    if (expr_struct_eq(integral, dependent))
        return expr_clone(right);
    if (expr_match_log_expr(integral, &argument) && expr_struct_eq(argument, dependent))
        return expr_exp(right);
    return NULL;
}

static bool de_is_logarithmic_dependent_integral(const expr_t *integral, const expr_t *dependent)
{
    const expr_t *argument = NULL;

    return expr_match_log_expr(integral, &argument) && expr_struct_eq(argument, dependent);
}

static bool de_is_dependent_square(const expr_t *expr, const expr_t *dependent)
{
    const expr_t *base = NULL;
    number_t exponent = num_new();
    bool matched =
        expr_match_pow_const(expr, &base, &exponent) && expr_struct_eq(base, dependent) && num_eq(exponent, NUM_TWO);

    num_destroy(&exponent);
    return matched;
}

de_attempt_t de_attempt_separable(const diffequ_t *de, const expr_t *independent, const expr_t *dependent,
                                  const expr_t *derivative_right, equation_t **solution_out)
{
    expr_t *independent_factor = NULL;
    expr_t *dependent_factor = NULL;
    expr_t *reciprocal = NULL;
    expr_t *dependent_integral = NULL;
    expr_t *independent_integral = NULL;
    expr_t *constant = NULL;
    expr_t *right = NULL;
    expr_t *solved_right = NULL;
    const expr_t *condition_point = NULL;
    const expr_t *condition_value = NULL;
    bool has_initial_condition;
    bool quadratic_dependent_factor;
    de_attempt_t attempt = DE_ATTEMPT_NOT_MATCHED;

    if (!de_split_separable(derivative_right, independent, dependent, &independent_factor, &dependent_factor))
        goto cleanup;
    attempt = DE_ATTEMPT_FAILED;
    quadratic_dependent_factor = de_is_dependent_square(dependent_factor, dependent);
    if (quadratic_dependent_factor && de_has_zero_initial_condition(de, dependent)) {
        expr_t *zero = expr_const_zero();

        *solution_out = zero ? equ_new(dependent, zero) : NULL;
        expr_free(zero);
        if (*solution_out)
            attempt = DE_ATTEMPT_SOLVED;
        goto cleanup;
    }

    reciprocal = expr_div_simplify_owned(expr_const_one(), dependent_factor);
    dependent_factor = NULL;
    dependent_integral = reciprocal ? expr_integrate(reciprocal, dependent) : NULL;
    independent_integral = expr_integrate(independent_factor, independent);
    if (!dependent_integral || !independent_integral)
        goto cleanup;

    constant = de_separable_constant(de, dependent, independent, dependent_integral, independent_integral);
    has_initial_condition = de_find_initial_condition(de, dependent, &condition_point, &condition_value);
    if (!has_initial_condition && de_is_logarithmic_dependent_integral(dependent_integral, dependent)) {
        expr_t *exponential = expr_exp(independent_integral);

        solved_right = expr_mul_simplify_owned(constant, exponential);
        constant = NULL;
        if (!solved_right)
            goto cleanup;
        goto construct_solution;
    }

    right = constant ? expr_add_simplify_owned(independent_integral, constant) : NULL;
    if (constant) {
        independent_integral = NULL;
        constant = NULL;
    }
    if (right && quadratic_dependent_factor) {
        expr_t *negative_right = de_simplify_unary_owned(expr_clone(right), expr_neg);

        solved_right = negative_right ? expr_div_simplify_owned(expr_const_one(), negative_right) : NULL;
        if (!solved_right)
            goto cleanup;
        goto construct_solution;
    }
    solved_right = right ? de_invert_separable_integral(dependent_integral, dependent, right) : NULL;
    if (!solved_right)
        goto cleanup;

construct_solution:
    *solution_out = equ_new(dependent, solved_right);
    if (*solution_out)
        attempt = DE_ATTEMPT_SOLVED;

cleanup:
    expr_free(solved_right);
    expr_free(right);
    expr_free(constant);
    expr_free(independent_integral);
    expr_free(dependent_integral);
    expr_free(reciprocal);
    expr_free(dependent_factor);
    expr_free(independent_factor);
    return attempt;
}
