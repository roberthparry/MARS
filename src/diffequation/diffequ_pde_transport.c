#include <stdbool.h>

#define MARS_DIFFEQUATION_PDE_INTERNAL_ACCESS
#include "diffequ_pde_internal.h"

static bool de_transport_is_constant_coefficient(
    const expr_t *coefficient,
    const expr_t *x,
    const expr_t *y,
    const expr_t *dependent)
{
    return coefficient &&
        !expr_is_exact_zero(coefficient) &&
        !de_expr_uses(coefficient, x) &&
        !de_expr_uses(coefficient, y) &&
        !de_expr_uses(coefficient, dependent);
}

static expr_t *de_transport_characteristic_foot(
    const expr_t *along,
    const expr_t *transverse,
    const expr_t *fixed_transverse,
    const expr_t *along_coefficient,
    const expr_t *transverse_coefficient)
{
    expr_t *delta = expr_sub_simplify_owned(
        expr_clone(transverse), expr_clone(fixed_transverse));
    expr_t *ratio = expr_div_simplify_owned(
        expr_clone(along_coefficient),
        expr_clone(transverse_coefficient));
    expr_t *shift = NULL;
    expr_t *foot = NULL;

    if (delta && ratio) {
        shift = expr_mul_simplify_owned(ratio, delta);
        ratio = NULL;
        delta = NULL;
    }
    if (shift) {
        foot = expr_sub_simplify_owned(
            expr_clone(along), shift);
        shift = NULL;
    }
    expr_free(shift);
    expr_free(ratio);
    expr_free(delta);
    return foot;
}

static equation_t *de_transport_boundary_solution(
    const diffequ_t *de,
    const expr_t *dependent,
    const expr_t *x,
    const expr_t *y,
    const expr_t *x_coefficient,
    const expr_t *y_coefficient)
{
    for (size_t i = 0u; i < de->condition_count; ++i) {
        const equation_t *condition = de->conditions[i];
        const expr_t *left = condition ? equ_lhs(condition) : NULL;
        const expr_t *value = condition ? equ_rhs(condition) : NULL;
        const expr_t *first;
        const expr_t *second;
        const expr_t *along;
        const expr_t *transverse;
        const expr_t *fixed_transverse;
        const expr_t *along_coefficient;
        const expr_t *transverse_coefficient;
        expr_t *foot;
        expr_t *substituted;
        expr_t *solution_right;
        equation_t *solution;

        if (!left ||
            !value ||
            !expr_struct_eq(left, dependent) ||
            de->condition_point_counts[i] != 2u)
            continue;

        first = de->condition_points[i][0];
        second = de->condition_points[i][1];
        if (expr_struct_eq(first, x) &&
            !de_expr_uses(second, x) &&
            !de_expr_uses(second, y) &&
            !de_expr_uses(value, y) &&
            !de_expr_uses(value, dependent)) {
            along = x;
            transverse = y;
            fixed_transverse = second;
            along_coefficient = x_coefficient;
            transverse_coefficient = y_coefficient;
        } else if (expr_struct_eq(second, y) &&
                   !de_expr_uses(first, x) &&
                   !de_expr_uses(first, y) &&
                   !de_expr_uses(value, x) &&
                   !de_expr_uses(value, dependent)) {
            along = y;
            transverse = x;
            fixed_transverse = first;
            along_coefficient = y_coefficient;
            transverse_coefficient = x_coefficient;
        } else {
            continue;
        }

        foot = de_transport_characteristic_foot(
            along,
            transverse,
            fixed_transverse,
            along_coefficient,
            transverse_coefficient);
        substituted = foot
            ? expr_substitute(value, along, foot)
            : NULL;
        solution_right = substituted
            ? expr_simplify_owned(substituted)
            : NULL;
        expr_free(foot);
        if (!solution_right)
            return NULL;
        solution = equ_new(dependent, solution_right);
        expr_free(solution_right);
        return solution;
    }
    return NULL;
}

static expr_t *de_transport_invariant(
    const expr_t *first,
    const expr_t *second,
    const expr_t *first_coefficient,
    const expr_t *second_coefficient)
{
    expr_t *ratio = expr_div_simplify_owned(
        expr_clone(second_coefficient),
        expr_clone(first_coefficient));
    expr_t *shift = ratio
        ? expr_mul_simplify_owned(ratio, expr_clone(first))
        : NULL;

    ratio = NULL;
    if (!shift)
        return NULL;
    return expr_sub_simplify_owned(expr_clone(second), shift);
}

static equation_t *de_transport_general_solution(
    const expr_t *dependent,
    const expr_t *first,
    const expr_t *second,
    const expr_t *first_coefficient,
    const expr_t *second_coefficient,
    const expr_t *remainder)
{
    expr_t *invariant = de_transport_invariant(
        first,
        second,
        first_coefficient,
        second_coefficient);
    expr_t *arbitrary = invariant
        ? expr_new_arbitrary_function("F", invariant)
        : NULL;
    expr_t *particular = NULL;
    expr_t *right = NULL;
    equation_t *solution = NULL;

    expr_free(invariant);
    if (!arbitrary)
        return NULL;
    if (expr_is_exact_zero(remainder)) {
        right = arbitrary;
        arbitrary = NULL;
    } else {
        expr_t *negative = expr_negate_owned(expr_clone(remainder));
        expr_t *rate = negative
            ? expr_div_simplify_owned(
                  negative, expr_clone(first_coefficient))
            : NULL;

        negative = NULL;
        particular = rate
            ? expr_mul_simplify_owned(rate, expr_clone(first))
            : NULL;
        rate = NULL;
        right = particular
            ? expr_add_simplify_owned(particular, arbitrary)
            : NULL;
        particular = NULL;
        arbitrary = NULL;
    }
    if (right)
        solution = equ_new(dependent, right);
    expr_free(right);
    expr_free(particular);
    expr_free(arbitrary);
    return solution;
}

de_attempt_t de_pde_attempt_constant_transport(
    const diffequ_t *de,
    const expr_t *residual,
    equation_t **solution_out,
    bool *recognized_out)
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
    de_attempt_t attempt = DE_ATTEMPT_NOT_MATCHED;

    *solution_out = NULL;
    *recognized_out = false;
    if (!de || !residual || de->independent_count != 2u)
        return DE_ATTEMPT_NOT_MATCHED;

    x = de->independent_vars[0];
    y = de->independent_vars[1];
    if (!de_pde_find_first_derivatives(
            residual, x, y, &dependent, &dx, &dy) ||
        !dependent ||
        !dx ||
        !dy ||
        !de_linear_decompose(
            residual, dx, &x_coefficient, &without_dx) ||
        !de_linear_decompose(
            without_dx, dy, &y_coefficient, &remainder) ||
        !de_transport_is_constant_coefficient(
            x_coefficient, x, y, dependent) ||
        !de_transport_is_constant_coefficient(
            y_coefficient, x, y, dependent) ||
        de_expr_uses(remainder, x) ||
        de_expr_uses(remainder, y) ||
        de_expr_uses(remainder, dependent))
        goto cleanup;

    *recognized_out = true;
    if (expr_is_exact_zero(remainder) && de->condition_count > 0u)
        *solution_out = de_transport_boundary_solution(
            de,
            dependent,
            x,
            y,
            x_coefficient,
            y_coefficient);
    else
        *solution_out = de_transport_general_solution(
            dependent,
            x,
            y,
            x_coefficient,
            y_coefficient,
            remainder);
    attempt = *solution_out
        ? DE_ATTEMPT_SOLVED
        : DE_ATTEMPT_NOT_MATCHED;

cleanup:
    expr_free(remainder);
    expr_free(y_coefficient);
    expr_free(without_dx);
    expr_free(x_coefficient);
    return attempt;
}
