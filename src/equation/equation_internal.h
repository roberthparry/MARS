#ifndef EQUATION_INTERNAL_H
#define EQUATION_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "equation.h"
#include "number.h"

typedef enum equation_solve_status {
    EQUATION_SOLVE_INVALID,
    EQUATION_SOLVE_UNSOLVED,
    EQUATION_SOLVE_SOLVED
} equation_solve_status_t;

struct equation_solutions {
    equation_t **solutions;
    size_t count;
    equation_solve_status_t status;
};

bool equation_match_affine_linear_expr(const expr_t *expr,
                                       const expr_t *wrt,
                                       bool require_nonzero_coeff,
                                       number_t *constant_out,
                                       number_t *coeff_out);

bool equation_match_quadratic_expr(const expr_t *expr,
                                   const expr_t *wrt,
                                   number_t *constant_out,
                                   number_t *linear_out,
                                   number_t *quadratic_out);

bool equation_match_symbolic_linear_expr(const expr_t *expr,
                                         const expr_t *wrt,
                                         expr_t **constant_out,
                                         expr_t **linear_out);

bool equation_match_symbolic_quadratic_expr(const expr_t *expr,
                                            const expr_t *wrt,
                                            expr_t **constant_out,
                                            expr_t **linear_out,
                                            expr_t **quadratic_out);

bool equation_match_symbolic_cubic_expr(const expr_t *expr,
                                        const expr_t *wrt,
                                        expr_t **constant_out,
                                        expr_t **linear_out,
                                        expr_t **quadratic_out,
                                        expr_t **cubic_out);

bool equation_match_polynomial_expr(const expr_t *expr,
                                    const expr_t *wrt,
                                    size_t max_degree,
                                    number_t *coeffs_out);

int equation_append_solution_value(const expr_t *wrt,
                                   number_t value,
                                   equation_solutions_t *solutions);

int equation_append_solution_expr(const expr_t *wrt,
                                  const expr_t *rhs,
                                  equation_solutions_t *solutions);

int equation_try_solve_cubic(const equation_t *equation,
                             const expr_t *wrt,
                             equation_solutions_t *solutions);

int equation_solve_for_into(const equation_t *equation,
                            const expr_t *wrt,
                            equation_solutions_t *solutions);

int equation_solve_numeric_into(const equation_t *equation,
                                expr_bindings_t *bindings,
                                const expr_goal_seek_options_t *options,
                                equation_solutions_t *solutions);

void equation_solutions_clear(equation_solutions_t *solutions);

#endif
