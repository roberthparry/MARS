/**
 * @file diffequ_context.c
 * @brief Shared assumption-aware simplification of differential-equation results.
 *
 * Retains solver status, derivation steps and parameter constraints, while creating
 * independent symbolic snapshots. Context assumptions never change solver selection.
 */
#include "expr_context-t.h"
#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_internal.h"

static bool de_context_expression(expr_t **slot, const expr_context_t *context)
{
    if (!*slot)
        return true;
    expr_context_t *captured = context ? NULL : expr_context_from_expression(*slot, NULL);
    expr_t *simplified = expr_context_refine(context ? context : captured, *slot);
    expr_context_free(captured);
    if (!simplified)
        return false;
    expr_free(*slot);
    *slot = simplified;
    return true;
}

static bool de_context_equation(equation_t **slot, const expr_context_t *context)
{
    if (!*slot)
        return true;
    equation_t *simplified = equ_simplify_with_context(*slot, context);
    if (!simplified)
        return false;
    equ_free(*slot);
    *slot = simplified;
    return true;
}

/* Solve first, then share the expression context's proof rules across the result. */
diffequ_solve_result_t *de_solve_with_context(const diffequ_t *de, unsigned int options,
                                           const expr_context_t *context)
{
    diffequ_solve_result_t *result = de_solve_with_options(de, options);
    if (!result)
        return NULL;
    for (size_t i = 0; i < result->solution_count; ++i)
        if (!de_context_equation(&result->solutions[i], context))
            goto failure;
    if (!de_context_equation(&result->parameter_constraint, context) ||
        !de_context_expression(&result->series_centre, context))
        goto failure;
    if (result->series_coefficients)
        for (size_t i = 0; i <= result->series_degree; ++i)
            if (!de_context_expression(&result->series_coefficients[i], context))
                goto failure;
    return result;

failure:
    de_solve_result_free(result);
    return NULL;
}
