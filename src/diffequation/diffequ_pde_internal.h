#ifndef DIFFEQU_PDE_INTERNAL_H
#define DIFFEQU_PDE_INTERNAL_H

#if !defined(MARS_DIFFEQUATION_PDE_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || \
     (defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__ > 0))
#error "diffequ_pde_internal.h is private to the PDE solver."
#endif

#define MARS_DIFFEQUATION_SOLVE_INTERNAL_ACCESS
#include "diffequ_solve_internal.h"

bool de_pde_find_first_derivatives(
    const expr_t *expr,
    const expr_t *x,
    const expr_t *y,
    const expr_t **dependent_out,
    const expr_t **dx_out,
    const expr_t **dy_out);
const expr_t *de_pde_find_named_coordinate(
    const expr_t *expr,
    const expr_t *independent,
    const expr_t *dependent,
    const char *name);
bool de_pde_same_symbolic_form(
    const expr_t *left,
    const expr_t *right);

de_attempt_t de_pde_attempt_constant_transport(
    const diffequ_t *de,
    const expr_t *residual,
    equation_t **solution_out,
    bool *recognized_out);
de_attempt_t de_pde_attempt_characteristics(
    const diffequ_t *de,
    const expr_t *residual,
    equation_t **solutions_out,
    size_t *solution_count_out);
de_attempt_t de_pde_attempt_parameter_linear(
    const expr_t *independent,
    const expr_t *dependent,
    const expr_t *derivative_right,
    equation_t **solution_out);

diffequ_solve_result_t *de_pde_solve_two_variable(
    const diffequ_t *de,
    const expr_t *residual);

#endif
