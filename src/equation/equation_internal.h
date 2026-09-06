#ifndef EQUATION_INTERNAL_H
#define EQUATION_INTERNAL_H

#if !defined(MARS_EQUATION_INTERNAL_ACCESS) &&                                                                         \
    (!defined(__INTELLISENSE__) || (defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__ > 0))
#error "equation_internal.h is private to the equation module; include equation.h instead."
#endif

#include <stdbool.h>
#include <stddef.h>

#include "equation.h"
#include "number.h"

#define MARS_SHARED_EQUATION_INTERNAL_ACCESS
#include "internal/equation_internal.h"

equation_t *equ_new_with_owned_bindings(const expr_t *lhs, const expr_t *rhs, expr_bindings_t *bindings);

int equ_set_display_TeX(equation_t *equation, const string_t *lhs, const string_t *rhs);
const string_t *equ_lhs_display_TeX(const equation_t *equation);
const string_t *equ_rhs_display_TeX(const equation_t *equation);
void equ_set_power_series_domain(equation_t *equation, bool power_series);

bool equ_match_quadratic_expr(const expr_t *expr, const expr_t *wrt, number_t *constant_out, number_t *linear_out,
                              number_t *quadratic_out);

bool equ_match_symbolic_linear_expr(const expr_t *expr, const expr_t *wrt, expr_t **constant_out, expr_t **linear_out);

bool equ_match_symbolic_cubic_expr(const expr_t *expr, const expr_t *wrt, expr_t **constant_out, expr_t **linear_out,
                                   expr_t **quadratic_out, expr_t **cubic_out);

bool equ_match_symbolic_quartic_expr(const expr_t *expr, const expr_t *wrt, expr_t **constant_out, expr_t **linear_out,
                                     expr_t **quadratic_out, expr_t **cubic_out, expr_t **quartic_out);

bool equ_match_symbolic_polynomial_alloc(const expr_t *expr, const expr_t *wrt, expr_t ***coefficients_out,
                                         size_t *degree_out);

bool equ_match_polynomial_expr(const expr_t *expr, const expr_t *wrt, size_t max_degree, number_t *coeffs_out);

bool equ_polynomial_coefficients_real(const number_t *coeffs, size_t degree);

bool equ_polynomial_root_effectively_real(number_t root, number_t tolerance);

void equ_polynomial_deflate_conjugate_pair(const number_t *coeffs, size_t degree, number_t root, number_t *deflated);

int equ_append_solution_value(const expr_t *wrt, number_t value, equation_solutions_t *solutions);

int equ_append_solution_expr(const expr_t *wrt, const expr_t *rhs, equation_solutions_t *solutions);

int equ_solve_quadratic_coefficients(const number_t *coeffs, const expr_t *wrt, equation_solutions_t *solutions);

int equ_try_solve_cubic(const equation_t *equation, const expr_t *wrt, equation_solutions_t *solutions);

int equ_solve_cubic_coefficients(const number_t *coeffs, const expr_t *wrt, equation_solutions_t *solutions);

int equ_try_solve_quartic(const equation_t *equation, const expr_t *wrt, equation_solutions_t *solutions);

int equ_solve_quartic_coefficients(const number_t *coeffs, const expr_t *wrt, equation_solutions_t *solutions);

int equ_try_solve_quintic(const equation_t *equation, const expr_t *wrt, equation_solutions_t *solutions);

int equ_try_solve_general_polynomial(const equation_t *equation, const expr_t *wrt, equation_solutions_t *solutions);

int equ_solve_numeric_into(const equation_t *equation, expr_bindings_t *bindings,
                           const expr_goal_seek_options_t *options, equation_solutions_t *solutions);

expr_bindings_t *equ_bindings_borrow(const equation_t *equation);

/* Return one when inapplicable, zero after a bounded search, or minus one on allocation failure. */
int equ_try_search_zeta_roots(const equation_t *equation, equation_solutions_t *solutions);

#endif
