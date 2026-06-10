#ifndef EQUATION_INTERNAL_H
#define EQUATION_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "equation.h"
#include "number.h"

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
                                   equation_solve_result_t *result);

int equation_append_solution_expr(const expr_t *wrt,
                                  const expr_t *rhs,
                                  equation_solve_result_t *result);

int equation_try_solve_cubic(const equation_t *equation,
                             const expr_t *wrt,
                             equation_solve_result_t *result);

#endif
