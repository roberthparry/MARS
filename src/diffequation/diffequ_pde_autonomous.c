/**
 * @file diffequ_pde_autonomous.c
 * @brief Autonomous transport partial differential equations.
 *
 * Recognises supported autonomous characteristic fields and constructs invariant solution forms. The general PDE
 * dispatcher calls this specialised path when the coefficient structure matches.
 *
 * This is an implementation unit for diffequ_t. Specialised solving paths apply only to recognised
 * families; callers must inspect result status and any retained conditions.
 */

#include <stdlib.h>

#define MARS_DIFFEQUATION_PDE_INTERNAL_ACCESS
#include "diffequ_pde_internal.h"

/* The normalising coefficient is constant in both coordinates and in the dependent variable. */
#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_notation_internal.h"

static bool de_autonomous_constant(const expr_t *coefficient, const expr_t *dependent, bool *assumption)
{
    if (!coefficient || de_expr_uses(coefficient, dependent) || expr_is_exact_zero(coefficient))
        return false;
    number_t value = expr_eval(coefficient);
    *assumption = num_is_nan(value);
    bool valid = *assumption || (num_is_finite(value) && !num_is_zero(value));
    num_destroy(&value);
    return valid;
}

/* Partial derivatives hold the dependent symbol fixed: this verifies the implicit characteristic relation. */
static bool de_autonomous_invariant(const expr_t *relation, const expr_t *x, const expr_t *y,
                                    const expr_t *a, const expr_t *b)
{
    expr_t *dx = expr_create_deriv(relation, x);
    expr_t *dy = expr_create_deriv(relation, y);
    expr_t *first = dx ? expr_mul(a, dx) : NULL;
    expr_t *second = dy ? expr_mul(b, dy) : NULL;
    expr_t *residual = first && second ? expr_add(first, second) : NULL;
    bool valid = residual && de_pde_is_symbolically_zero(residual);
    expr_free(residual);
    expr_free(second);
    expr_free(first);
    expr_free(dy);
    expr_free(dx);
    return valid;
}

/* Solve c*u_t + a(u)*u_x = 0 as an implicit family, independently of coordinate names and input order. */
diffequ_solve_result_t *de_pde_solve_autonomous_transport(const diffequ_t *de, const expr_t *residual,
                                                        bool include_steps)
{
    const expr_t *dependent = NULL, *dx = NULL, *dy = NULL;
    expr_t *a = NULL, *b = NULL, *without_dx = NULL, *remainder = NULL;
    expr_t *speed = NULL, *phase = NULL, *right = NULL, *relation = NULL;
    equation_t *solution = NULL;
    diffequ_solve_result_t *result = NULL;
    bool assumption = false;

    if (!de || !residual || de->independent_count != 2u || de->condition_count != 0u)
        return NULL;
    const expr_t *x = de->independent_vars[0], *y = de->independent_vars[1];
    if (!de_pde_find_first_derivatives(residual, x, y, &dependent, &dx, &dy) || !dependent || !dx || !dy ||
        !de_linear_decompose(residual, dx, &a, &without_dx) ||
        !de_linear_decompose(without_dx, dy, &b, &remainder) || !de_pde_is_symbolically_zero(remainder) ||
        de_expr_uses(a, x) || de_expr_uses(a, y) || de_expr_uses(b, x) || de_expr_uses(b, y))
        goto cleanup;

    const expr_t *along = NULL, *across = NULL, *normaliser = NULL, *transport = NULL;
    if (de_autonomous_constant(a, dependent, &assumption) && de_expr_uses(b, dependent)) {
        along = x;
        across = y;
        normaliser = a;
        transport = b;
    } else if (de_autonomous_constant(b, dependent, &assumption) && de_expr_uses(a, dependent)) {
        along = y;
        across = x;
        normaliser = b;
        transport = a;
    } else {
        goto cleanup;
    }
    speed = expr_div_simplify_owned(expr_clone(transport), expr_clone(normaliser));
    phase = speed ? expr_sub_simplify_owned(expr_clone(across), expr_mul(speed, along)) : NULL;
    right = phase ? expr_new_arbitrary_function("F", phase) : NULL;
    relation = right ? expr_sub(dependent, right) : NULL;
    if (!relation || !de_autonomous_invariant(phase, x, y, a, b) ||
        !de_autonomous_invariant(relation, x, y, a, b))
        goto cleanup;
    /* Reuse the same dependent symbol on both sides of the implicit equation. */
    solution = equ_new(dependent, right);
    result = solution ? de_solve_result_new(DE_SOLVE_STATUS_SOLVED, DE_SOLVER_CHARACTERISTICS,
        "implicit local characteristic family; coefficients defined, normaliser nonzero, and H_u nonzero") : NULL;
    if (!result || (include_steps && !de_autonomous_steps(result, dependent, along, across, normaliser,
                                                         speed, phase, relation, assumption)) ||
        de_solve_result_append(result, solution) != 0) {
        de_solve_result_free(result);
        result = NULL;
        goto cleanup;
    }
    solution = NULL;

cleanup:
    equ_free(solution);
    expr_free(relation);
    expr_free(right);
    expr_free(phase);
    expr_free(speed);
    expr_free(remainder);
    expr_free(without_dx);
    expr_free(b);
    expr_free(a);
    return result;
}
