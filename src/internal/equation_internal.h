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

/* Binding probe for differential equations: undeclared calls are symbolic functions, not scalar products. */
equation_t *equ_from_differential_text_internal(const string_t *text);
/* Shared construction and operator-polynomial recognition for the differential-equation parser. */
equation_t *equ_new_with_owned_bindings(const expr_t *lhs, const expr_t *rhs, expr_bindings_t *bindings);
bool equ_match_symbolic_polynomial_alloc(const expr_t *expr, const expr_t *wrt, expr_t ***coefficients_out,
                                         size_t *degree_out);
bool equ_polynomial_coefficients_real(const number_t *coeffs, size_t degree);

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

/* Share the native symbolic coefficient collector with differential-equation analysis. */
bool equ_collect_symbolic_polynomial_alloc(const expr_t *expr, const expr_t *wrt, expr_t ***coefficients_out,
                                           size_t *degree_out);

bool equ_match_symbolic_quadratic_expr(const expr_t *expr, const expr_t *wrt, expr_t **constant_out,
                                       expr_t **linear_out, expr_t **quadratic_out);

int equ_solve_for_into(const equation_t *equation, const expr_t *wrt, equation_solutions_t *solutions);

void equ_solutions_clear(equation_solutions_t *solutions);

/* Attach faithful native mathematical notation to an equation's underlying expression trees. */
int equ_set_display_TeX(equation_t *equation, const string_t *lhs, const string_t *rhs);

/* Attach display-only plain notation; expression and function styles retain the underlying algebra. */
int equ_set_display_unbound(equation_t *equation, const string_t *lhs, const string_t *rhs);

#endif
