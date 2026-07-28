#ifndef DIFFEQU_INTERNAL_H
#define DIFFEQU_INTERNAL_H

#if !defined(MARS_DIFFEQUATION_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || \
     (defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__ > 0))
#error "diffequ_internal.h is private to the diffequation module; include diffequation.h instead."
#endif

#include "diffequation.h"

struct diffequ_t {
    equation_t *equation;
    string_t *equation_text;

    expr_t **independent_vars;
    size_t independent_count;

    expr_bindings_t *constants;

    equation_t **conditions;
    string_t **condition_texts;
    expr_t **condition_points;
    size_t condition_count;

    string_t *independent_text;
    string_t *constant_text;
};

struct diffequ_solve_result_t {
    de_solve_status_t status;
    de_solver_t solver;
    equation_t **solutions;
    size_t solution_count;
    char *diagnostic;
};

diffequ_t *de_new_owned(equation_t *equation);
diffequ_solve_result_t *de_solve_result_new(
    de_solve_status_t status,
    de_solver_t solver,
    const char *diagnostic);
int de_solve_result_append(
    diffequ_solve_result_t *result,
    equation_t *solution);

#endif /* DIFFEQU_INTERNAL_H */
