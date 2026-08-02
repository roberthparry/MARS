#include <stdlib.h>
#include <string.h>

#include "diffequation.h"
#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_internal.h"

diffequ_solve_result_t *de_solve_result_new(
    de_solve_status_t status,
    de_solver_t solver,
    const char *diagnostic)
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

int de_solve_result_append(
    diffequ_solve_result_t *result,
    equation_t *solution)
{
    equation_t **solutions;

    if (!result || !solution)
        return -1;

    solutions = realloc(
        result->solutions,
        (result->solution_count + 1u) * sizeof(*solutions));
    if (!solutions)
        return -1;

    result->solutions = solutions;
    result->solutions[result->solution_count++] = solution;
    return 0;
}

int de_solve_result_set_steps(
    diffequ_solve_result_t *result,
    const char *steps)
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

int de_solve_result_set_symmetry(
    diffequ_solve_result_t *result,
    const char *symmetry)
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

int de_solve_result_set_steps_tex(
    diffequ_solve_result_t *result,
    const char *steps_tex)
{
    char *copy;

    if (!result || !steps_tex)
        return -1;
    copy = strdup(steps_tex);
    if (!copy)
        return -1;
    free(result->steps_tex);
    result->steps_tex = copy;
    return 0;
}

void de_solve_result_free(diffequ_solve_result_t *result)
{
    if (!result)
        return;

    for (size_t i = 0u; i < result->solution_count; ++i)
        equ_free(result->solutions[i]);
    free(result->solutions);
    free(result->symmetry);
    free(result->steps);
    free(result->steps_tex);
    free(result->diagnostic);
    free(result);
}

de_solve_status_t de_solve_result_status(
    const diffequ_solve_result_t *result)
{
    return result ? result->status : DE_SOLVE_STATUS_INVALID;
}

de_solver_t de_solve_result_solver(const diffequ_solve_result_t *result)
{
    return result ? result->solver : DE_SOLVER_NONE;
}

const char *de_solve_result_diagnostic(
    const diffequ_solve_result_t *result)
{
    return result ? result->diagnostic : NULL;
}

const char *de_solve_result_steps(
    const diffequ_solve_result_t *result)
{
    return result ? result->steps : NULL;
}

const char *de_solve_result_symmetry(
    const diffequ_solve_result_t *result)
{
    return result ? result->symmetry : NULL;
}

const char *de_solve_result_steps_tex(
    const diffequ_solve_result_t *result)
{
    return result ? result->steps_tex : NULL;
}

size_t de_solve_result_count(const diffequ_solve_result_t *result)
{
    return result ? result->solution_count : 0u;
}

const equation_t *de_solve_result_at(
    const diffequ_solve_result_t *result,
    size_t index)
{
    if (!result || index >= result->solution_count)
        return NULL;
    return result->solutions[index];
}
