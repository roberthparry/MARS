#ifndef DIFFEQU_INTERNAL_H
#define DIFFEQU_INTERNAL_H

#include <stdbool.h>

#if !defined(MARS_DIFFEQUATION_INTERNAL_ACCESS) &&                                                                     \
    (!defined(__INTELLISENSE__) || (defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__ > 0))
#error "diffequ_internal.h is private to the diffequation module; include diffequation.h instead."
#endif

#include "diffequation.h"

struct diffequ_t {
    equation_t *equation;
    string_t *equation_text;
    string_t *differential_form_text;
    bool differential_form_input;
    bool partial_derivative_input;

    expr_t **independent_vars;
    size_t independent_count;

    expr_bindings_t *constants;

    equation_t **conditions;
    string_t **condition_texts;
    expr_t ***condition_points;
    size_t *condition_point_counts;
    size_t condition_count;

    string_t *independent_text;
    string_t *constant_text;

    size_t repeated_quadratic_power;
    expr_t *repeated_quadratic_square;
    char *repeated_quadratic_dependent;
};

struct diffequ_solve_result_t {
    de_solve_status_t status;
    de_solver_t solver;
    equation_t **solutions;
    size_t solution_count;
    char *diagnostic;
    char *steps;
    char *steps_tex;
    char *symmetry;
};

diffequ_t *de_new_owned(equation_t *equation);
diffequ_solve_result_t *de_solve_result_new(de_solve_status_t status, de_solver_t solver, const char *diagnostic);
int de_solve_result_append(diffequ_solve_result_t *result, equation_t *solution);
int de_solve_result_set_steps(diffequ_solve_result_t *result, const char *steps);
int de_solve_result_set_steps_tex(diffequ_solve_result_t *result, const char *steps_tex);
int de_solve_result_set_symmetry(diffequ_solve_result_t *result, const char *symmetry);
int de_solve_result_ensure_rule_steps(const diffequ_t *de, diffequ_solve_result_t *result);
bool de_linear_decompose(const expr_t *expr, const expr_t *needle, expr_t **coefficient_out, expr_t **constant_out);

#endif /* DIFFEQU_INTERNAL_H */
