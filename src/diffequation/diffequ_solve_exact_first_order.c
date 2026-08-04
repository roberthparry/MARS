#define MARS_DIFFEQUATION_SOLVE_INTERNAL_ACCESS
#include "diffequ_solve_internal.h"
#define MARS_SHARED_EQUATION_INTERNAL_ACCESS
#include "internal/equation_internal.h"

static expr_t *de_exact_normalize_isolated_rhs(const expr_t *rhs)
{
    const expr_t *negated = NULL;
    const expr_t *numerator = NULL;
    const expr_t *denominator = NULL;
    const expr_t *scaled_base = NULL;
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    number_t denominator_scale = num_new();
    expr_t *normalized_numerator = NULL;
    expr_t *normalized_denominator = NULL;
    expr_t *out = NULL;
    bool is_sub = false;
    bool negate_quotient = false;

    if (!rhs)
        goto cleanup;

    if (expr_match_neg_expr(rhs, &negated) &&
        expr_match_div_expr(negated, &numerator, &denominator)) {
        negate_quotient = true;
    } else if (expr_match_div_expr(rhs, &numerator, &denominator) &&
               expr_match_neg_expr(numerator, &negated)) {
        numerator = negated;
        negate_quotient = true;
    } else if (expr_match_div_expr(rhs, &numerator, &denominator) &&
               expr_match_neg_expr(denominator, &negated)) {
        denominator = negated;
        negate_quotient = true;
    } else if (expr_match_div_expr(rhs, &numerator, &denominator) &&
               expr_match_scaled_expr(
                   denominator, &denominator_scale, &scaled_base) &&
               num_sign(denominator_scale) < 0) {
        normalized_denominator = expr_negate_owned(
            expr_clone(denominator));
        negate_quotient = true;
    } else {
        out = expr_clone(rhs);
        goto cleanup;
    }

    if (!negate_quotient)
        goto cleanup;
    if (expr_match_add_sub_expr(numerator, &left, &right, &is_sub)) {
        normalized_numerator = is_sub
            ? expr_sub_simplify_owned(expr_clone(right), expr_clone(left))
            : expr_add_simplify_owned(
                  expr_negate_owned(expr_clone(left)),
                  expr_negate_owned(expr_clone(right)));
    } else {
        normalized_numerator = expr_negate_owned(expr_clone(numerator));
    }
    if (!normalized_denominator)
        normalized_denominator = expr_clone(denominator);
    if (normalized_numerator && normalized_denominator) {
        out = expr_div_simplify_owned(
            normalized_numerator, normalized_denominator);
        normalized_numerator = NULL;
        normalized_denominator = NULL;
    }

cleanup:
    expr_free(normalized_denominator);
    expr_free(normalized_numerator);
    num_destroy(&denominator_scale);
    return out;
}

de_attempt_t de_attempt_exact_first_order(
    const diffequ_t *de,
    const expr_t *independent,
    const expr_t *dependent,
    const expr_t *first_derivative,
    const expr_t *residual,
    equation_t **solutions_out,
    size_t *solution_count_out)
{
    expr_t *dependent_coefficient = NULL;
    expr_t *independent_coefficient = NULL;
    expr_t *dependent_coefficient_dx = NULL;
    expr_t *independent_coefficient_dy = NULL;
    expr_t *exactness_difference = NULL;
    expr_t *potential = NULL;
    expr_t *potential_dx = NULL;
    expr_t *correction_integrand = NULL;
    expr_t *correction = NULL;
    expr_t *complete_potential = NULL;
    expr_t *constant = NULL;
    expr_t *polynomial_constant = NULL;
    expr_t *polynomial_linear = NULL;
    expr_t *polynomial_quadratic = NULL;
    expr_t *normalized_constant = NULL;
    equation_t *implicit_solution = NULL;
    equation_t *isolation_solution = NULL;
    const equation_t *equation_to_isolate = NULL;
    equation_solutions_t isolated = { 0 };
    de_attempt_t attempt = DE_ATTEMPT_NOT_MATCHED;

    if (!de || !independent || !dependent || !first_derivative ||
        !residual || !solutions_out || !solution_count_out ||
        de->condition_count != 0u)
        goto cleanup;
    *solution_count_out = 0u;
    solutions_out[0] = NULL;
    solutions_out[1] = NULL;

    if (!de_linear_decompose(
            residual,
            first_derivative,
            &dependent_coefficient,
            &independent_coefficient))
        goto cleanup;

    dependent_coefficient_dx = expr_create_deriv(
        dependent_coefficient, independent);
    independent_coefficient_dy = expr_create_deriv(
        independent_coefficient, dependent);
    exactness_difference = dependent_coefficient_dx &&
        independent_coefficient_dy
        ? expr_sub_simplify_owned(
              dependent_coefficient_dx,
              independent_coefficient_dy)
        : NULL;
    if (dependent_coefficient_dx && independent_coefficient_dy) {
        dependent_coefficient_dx = NULL;
        independent_coefficient_dy = NULL;
    }
    if (!exactness_difference ||
        !expr_is_exact_zero(exactness_difference))
        goto cleanup;

    attempt = DE_ATTEMPT_FAILED;
    potential = expr_integrate(dependent_coefficient, dependent);
    potential_dx = potential
        ? expr_create_deriv(potential, independent)
        : NULL;
    correction_integrand = potential_dx
        ? expr_sub_simplify_owned(
              expr_clone(independent_coefficient),
              potential_dx)
        : NULL;
    if (potential_dx)
        potential_dx = NULL;
    if (!potential || !correction_integrand ||
        de_expr_uses(correction_integrand, dependent))
        goto cleanup;

    correction = expr_is_exact_zero(correction_integrand)
        ? expr_const_zero()
        : expr_integrate(correction_integrand, independent);
    complete_potential = correction
        ? expr_add_simplify_owned(potential, correction)
        : NULL;
    if (correction) {
        potential = NULL;
        correction = NULL;
    }
    constant = de_arbitrary_constant();
    implicit_solution = complete_potential && constant
        ? equ_new(complete_potential, constant)
        : NULL;
    if (!implicit_solution)
        goto cleanup;

    equation_to_isolate = implicit_solution;
    if (equ_match_symbolic_quadratic_expr(
            complete_potential,
            dependent,
            &polynomial_constant,
            &polynomial_linear,
            &polynomial_quadratic)) {
        /*
         * A quadratic formula introduces 4aC in its discriminant.  Since C
         * is arbitrary, solve the equivalent equation F = C/4 so that the
         * displayed explicit family absorbs that numerical factor into C.
         */
        normalized_constant = expr_div_long(constant, 4L);
        isolation_solution = normalized_constant
            ? equ_new(complete_potential, normalized_constant)
            : NULL;
        if (isolation_solution)
            equation_to_isolate = isolation_solution;
    }

    if (equ_solve_for_into(
            equation_to_isolate, dependent, &isolated) == 0 &&
        equ_solutions_count(&isolated) > 0u &&
        equ_solutions_count(&isolated) <= 2u) {
        for (size_t i = 0u; i < equ_solutions_count(&isolated); ++i) {
            const equation_t *item = equ_solutions_at(&isolated, i);
            expr_t *normalized_rhs = item
                ? de_exact_normalize_isolated_rhs(equ_rhs(item))
                : NULL;

            solutions_out[i] = item && normalized_rhs
                ? equ_new(equ_lhs(item), normalized_rhs)
                : NULL;
            expr_free(normalized_rhs);
            if (!solutions_out[i])
                goto cleanup;
            (*solution_count_out)++;
        }
    } else {
        solutions_out[0] = implicit_solution;
        implicit_solution = NULL;
        *solution_count_out = 1u;
    }
    attempt = DE_ATTEMPT_SOLVED;

cleanup:
    if (attempt != DE_ATTEMPT_SOLVED) {
        if (solutions_out) {
            equ_free(solutions_out[1]);
            equ_free(solutions_out[0]);
            solutions_out[1] = NULL;
            solutions_out[0] = NULL;
        }
        if (solution_count_out)
            *solution_count_out = 0u;
    }
    equ_solutions_clear(&isolated);
    equ_free(isolation_solution);
    equ_free(implicit_solution);
    expr_free(normalized_constant);
    expr_free(polynomial_quadratic);
    expr_free(polynomial_linear);
    expr_free(polynomial_constant);
    expr_free(constant);
    expr_free(complete_potential);
    expr_free(correction);
    expr_free(correction_integrand);
    expr_free(potential_dx);
    expr_free(potential);
    expr_free(exactness_difference);
    expr_free(independent_coefficient_dy);
    expr_free(dependent_coefficient_dx);
    expr_free(independent_coefficient);
    expr_free(dependent_coefficient);
    return attempt;
}
