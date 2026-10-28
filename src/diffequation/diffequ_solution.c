/**
 * @file diffequ_solution.c
 * @brief Differential-equation result ownership and access.
 *
 * Stores solver status, families, constraints, steps and series metadata and exposes result queries. Solver
 * implementations use this common representation so callers can inspect outcomes uniformly.
 *
 * This is an implementation unit for diffequ_t. Specialised solving paths apply only to recognised
 * families; callers must inspect result status and any retained conditions.
 */

#include <stdlib.h>
#include <string.h>

#include "diffequation-t.h"

#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_internal.h"

diffequ_solve_result_t *de_solve_result_new(de_solve_status_t status, de_solver_t solver, const char *diagnostic)
{
    diffequ_solve_result_t *result = calloc(1u, sizeof(*result));

    if (!result)
        return NULL;
    result->status = status;
    result->solver = solver;
    if (diagnostic) {
        result->diagnostic = strdup(diagnostic);
        if (!result->diagnostic) {
            free(result);
            return NULL;
        }
    }
    return result;
}

int de_solve_result_append(diffequ_solve_result_t *result, equation_t *solution)
{
    equation_t **solutions;

    if (!result || !solution)
        return -1;

    solutions = realloc(result->solutions, (result->solution_count + 1u) * sizeof(*solutions));
    if (!solutions)
        return -1;

    result->solutions = solutions;
    result->solutions[result->solution_count++] = solution;
    return 0;
}

int de_solve_result_set_steps(diffequ_solve_result_t *result, const char *steps)
{
    char *copy;

    if (!result || !steps)
        return -1;
    copy = strdup(steps);
    if (!copy)
        return -1;
    free(result->steps);
    result->steps = copy;
    return 0;
}

int de_solve_result_set_symmetry(diffequ_solve_result_t *result, const char *symmetry)
{
    char *copy;

    if (!result || !symmetry)
        return -1;
    copy = strdup(symmetry);
    if (!copy)
        return -1;
    free(result->symmetry);
    result->symmetry = copy;
    return 0;
}

int de_solve_result_set_steps_TeX(diffequ_solve_result_t *result, const char *steps_TeX)
{
    char *copy;

    if (!result || !steps_TeX)
        return -1;
    copy = strdup(steps_TeX);
    if (!copy)
        return -1;
    free(result->steps_TeX);
    result->steps_TeX = copy;
    return 0;
}

void de_solve_result_free(diffequ_solve_result_t *result)
{
    if (!result)
        return;

    for (size_t i = 0u; i < result->solution_count; ++i)
        equ_free(result->solutions[i]);
    free(result->solutions);
    expr_free(result->envelope_parameter);
    equ_free(result->parameter_constraint);
    if (result->series_coefficients) {
        for (size_t i = 0u; i <= result->series_degree; ++i)
            expr_free(result->series_coefficients[i]);
    }
    free(result->series_coefficients);
    expr_free(result->series_centre);
    free(result->symmetry);
    free(result->steps);
    free(result->steps_TeX);
    free(result->diagnostic);
    free(result);
}

de_solve_status_t de_solve_result_status(const diffequ_solve_result_t *result)
{
    return result ? result->status : DE_SOLVE_STATUS_INVALID;
}

de_solver_t de_solve_result_solver(const diffequ_solve_result_t *result)
{
    return result ? result->solver : DE_SOLVER_NONE;
}

const char *de_solve_result_diagnostic(const diffequ_solve_result_t *result)
{
    return result ? result->diagnostic : NULL;
}

const char *de_solve_result_steps(const diffequ_solve_result_t *result)
{
    return result ? result->steps : NULL;
}

const char *de_solve_result_symmetry(const diffequ_solve_result_t *result)
{
    return result ? result->symmetry : NULL;
}

const char *de_solve_result_steps_TeX(const diffequ_solve_result_t *result)
{
    return result ? result->steps_TeX : NULL;
}

size_t de_solve_result_count(const diffequ_solve_result_t *result)
{
    return result ? result->solution_count : 0u;
}

const equation_t *de_solve_result_at(const diffequ_solve_result_t *result, size_t index)
{
    if (!result || index >= result->solution_count)
        return NULL;
    return result->solutions[index];
}

/* Borrow the auxiliary variable used in the first, constrained envelope family. */
const expr_t *de_solve_result_parameter(const diffequ_solve_result_t *result)
{
    return result ? result->envelope_parameter : NULL;
}

/* Borrow the equation that determines the first solution family's auxiliary parameter. */
const equation_t *de_solve_result_parameter_constraint(const diffequ_solve_result_t *result)
{
    return result ? result->parameter_constraint : NULL;
}

/* Borrow the centre without transferring ownership. */
const expr_t *de_solve_result_series_centre(const diffequ_solve_result_t *result)
{
    return result ? result->series_centre : NULL;
}

/* Report the retained degree, not the number of coefficients. */
size_t de_solve_result_series_degree(const diffequ_solve_result_t *result)
{
    return result ? result->series_degree : 0u;
}

/* Borrow one exact symbolic Taylor coefficient. */
const expr_t *de_solve_result_series_coefficient(const diffequ_solve_result_t *result, size_t index)
{
    return result && result->series_coefficients && index <= result->series_degree
               ? result->series_coefficients[index] : NULL;
}
