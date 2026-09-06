#ifndef EQUATION_SHARED_INTERNAL_H
#define EQUATION_SHARED_INTERNAL_H

#if !defined(MARS_SHARED_EQUATION_INTERNAL_ACCESS) &&                                                                  \
    (!defined(__INTELLISENSE__) || (defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__ > 0))
#error "internal/equation_internal.h is private to the MARS implementation; include equation.h instead."
#endif

#include <stdbool.h>
#include <stddef.h>

#include "equation.h"
#include "number.h"

typedef enum equation_solve_status {
    EQUATION_SOLVE_INVALID,
    EQUATION_SOLVE_UNSOLVED,
    EQUATION_SOLVE_NO_SOLUTIONS,
    EQUATION_SOLVE_SOLVED
} equation_solve_status_t;

typedef enum equation_search_kind {
    EQUATION_SEARCH_NONE,
    EQUATION_SEARCH_ZETA_VALUE,
    EQUATION_SEARCH_ZETA_SERIES_EMPTY,
    EQUATION_SEARCH_ZETA_ZEROS
} equation_search_kind_t;

struct equation_solutions {
    equation_t **solutions;
    size_t count;
    equation_solve_status_t status;
    equation_search_kind_t search_kind;
};

bool equ_match_affine_linear_expr(const expr_t *expr, const expr_t *wrt, bool require_nonzero_coeff,
                                  number_t *constant_out, number_t *coeff_out);

bool equ_match_polynomial_alloc(const expr_t *expr, const expr_t *wrt, number_t **coeffs_out, size_t *degree_out);

bool equ_match_symbolic_quadratic_expr(const expr_t *expr, const expr_t *wrt, expr_t **constant_out,
                                       expr_t **linear_out, expr_t **quadratic_out);

int equ_solve_for_into(const equation_t *equation, const expr_t *wrt, equation_solutions_t *solutions);

void equ_solutions_clear(equation_solutions_t *solutions);

#endif
