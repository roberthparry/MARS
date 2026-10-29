/**
 * @file equation_internal.h
 * @brief Controlled cross-module equation interface.
 *
 * Exposes the limited equation parsing, construction and solution facilities required by other native MARS
 * modules. Use equation_t first; this guarded interface is for implementation collaboration, not application or
 * installed public use.
 *
 * This header is an implementation detail under src/, not an installed public API. Keep its consumers within the
 * documented module boundary and preserve any explicit internal-access guards.
 */

#ifndef EQUATION_SHARED_INTERNAL_H
#define EQUATION_SHARED_INTERNAL_H

#if !defined(MARS_SHARED_EQUATION_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_SHARED_EQUATION_INTERNAL_ACCESS."
#endif

#include <stdbool.h>
#include <stddef.h>

#include "equation-t.h"
#include "number-t.h"
#include "TeX-t.h"

/**
 * @brief Interpret one borrowed native operand using caller-owned parser state.
 * @param context Borrowed callback state; its owner keeps it valid throughout the call.
 * @param text Borrowed UTF-8 input or already rendered fragment, as specified by the function's purpose.
 * @return Owned notation released by the equation adapter with TeX_free, or NULL on failure.
 * @note Neither context nor text transfers ownership; do not retain the adapter's temporary text pointer.
 */
typedef TeX_t *(*notation_read_fn)(void *context, const string_t *text);

/**
 * @brief Parse native equation input and delegate its operands through a borrowed reader.
 * @param text Borrowed input containing the sides and optional native bindings.
 * @param read Required operand reader returning owned notation for each side.
 * @param context Borrowed reader state passed unchanged to read.
 * @param recognised Required caller-owned flag initialised to false; set true when this parser claims input.
 * @return Owned notation released with TeX_free, or NULL for unrecognised input or failure.
 * @note A true recognised flag with NULL output indicates a failed claimed input, not permission to try
 * another grammar. Operands are read sequentially and all intermediate notation is released internally.
 */
TeX_t *equ_notation_input(const string_t *text, notation_read_fn read, void *context, bool *recognised);

/**
 * @brief Parse native differential text, treating undeclared calls as symbolic functions.
 * @param text Borrowed UTF-8 input or already rendered fragment, as specified by the function's purpose.
 * @return Owned equation released with equ_free, or NULL on invalid input or failure.
 */
equation_t *equ_from_differential_text_internal(const string_t *text);

/**
 * @brief Construct an owned equation, retaining sides and adopting bindings on success.
 * @param lhs Borrowed mathematical side or copied display notation, according to the declared parameter type.
 * @param rhs Borrowed mathematical side or copied display notation, according to the declared parameter type.
 * @param bindings Owned binding collection adopted on success; the caller retains it on failure.
 * @return Owned equation released with equ_free, or NULL on invalid input or failure.
 */
equation_t *equ_new_with_owned_bindings(const expr_t *lhs, const expr_t *rhs, expr_bindings_t *bindings);

/**
 * @brief Match a polynomial with owned symbolic coefficients; release each with expr_free and the array with free.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param wrt Borrowed native variable identifying the coordinate of differentiation, integration, sampling or
 * solving.
 * @param coefficients_out Required output receiving owned symbolic coefficients; release each with expr_free and
 * the array with free.
 * @param degree_out Required output receiving the degree of the returned polynomial coefficient array.
 * @return True when the requested pattern matches; false when it does not match or matching fails.
 */
bool equ_match_symbolic_polynomial_alloc(const expr_t *expr, const expr_t *wrt, expr_t ***coefficients_out,
                                         size_t *degree_out);

/**
 * @brief Check whether every borrowed coefficient of a degree-order polynomial is real.
 * @param coeffs Borrowed numeric polynomial coefficient array in ascending-power order.
 * @param degree Polynomial degree; coefficient arrays contain degree plus one entries in ascending-power order.
 * @return True when the requested operation succeeds; false when unavailable, invalid or unsuccessful.
 */
bool equ_polynomial_coefficients_real(const number_t *coeffs, size_t degree);

/** @brief Outcome of native equation solving, independent of rendered notation. */
typedef enum equation_solve_status {
    EQUATION_SOLVE_INVALID,
    EQUATION_SOLVE_UNSOLVED,
    EQUATION_SOLVE_NO_SOLUTIONS,
    EQUATION_SOLVE_SOLVED
} equation_solve_status_t;

/** @brief Optional numerical-search interpretation retained by equation solution sets. */
typedef enum equation_search_kind {
    EQUATION_SEARCH_NONE,
    EQUATION_SEARCH_ZETA_VALUE,
    EQUATION_SEARCH_ZETA_SERIES_EMPTY,
    EQUATION_SEARCH_ZETA_ZEROS
} equation_search_kind_t;

/** @brief Owned solution storage shared with differential-equation analysis; clear with equ_solutions_clear. */
typedef struct _equation_solutions_t {
    equation_t **solutions; /**< Owned array of owned equations, released by equ_solutions_clear. */
    size_t count; /**< Number of equations in solutions. */
    equation_solve_status_t status; /**< Native solver outcome. */
    equation_search_kind_t search_kind; /**< Optional numerical-search interpretation. */
} equation_solutions_t;

/**
 * @brief Match native affine coefficients into initialised owned numeric outputs; release with num_destroy.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param wrt Borrowed native variable identifying the coordinate of differentiation, integration, sampling or
 * solving.
 * @param require_nonzero_coeff Whether to reject an affine match whose variable coefficient is zero.
 * @param constant_out Initialised numeric output replaced on success; release the resulting value with num_destroy.
 * @param coeff_out Initialised numeric output replaced on success; release the resulting value with num_destroy.
 * @return True when the requested pattern matches; false when it does not match or matching fails.
 */
bool equ_match_affine_linear_expr(const expr_t *expr, const expr_t *wrt, bool require_nonzero_coeff,
                                  number_t *constant_out, number_t *coeff_out);

/**
 * @brief Collect owned numeric polynomial coefficients; release each with num_destroy and the array with free.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param wrt Borrowed native variable identifying the coordinate of differentiation, integration, sampling or
 * solving.
 * @param coeffs_out Required caller-owned output receiving owned numeric coefficients in ascending-power order;
 * destroy entries and free the array.
 * @param degree_out Required output receiving the degree of the returned polynomial coefficient array.
 * @return True when the requested pattern matches; false when it does not match or matching fails.
 */
bool equ_match_polynomial_alloc(const expr_t *expr, const expr_t *wrt, number_t **coeffs_out, size_t *degree_out);

/**
 * @brief Collect owned symbolic polynomial coefficients for differential analysis; free entries and array.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param wrt Borrowed native variable identifying the coordinate of differentiation, integration, sampling or
 * solving.
 * @param coefficients_out Required output receiving owned symbolic coefficients; release each with expr_free and
 * the array with free.
 * @param degree_out Required output receiving the degree of the returned polynomial coefficient array.
 * @return True when the requested operation succeeds; false when unavailable, invalid or unsuccessful.
 */
bool equ_collect_symbolic_polynomial_alloc(const expr_t *expr, const expr_t *wrt, expr_t ***coefficients_out,
                                           size_t *degree_out);

/**
 * @brief Match three independently owned symbolic coefficients; release each successful output with expr_free.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param wrt Borrowed native variable identifying the coordinate of differentiation, integration, sampling or
 * solving.
 * @param constant_out Required caller-owned output receiving an owned expression; release a successful result with
 * expr_free.
 * @param linear_out Required caller-owned output receiving an owned expression; release a successful result with
 * expr_free.
 * @param quadratic_out Required caller-owned output receiving an owned expression; release a successful result with
 * expr_free.
 * @return True when the requested pattern matches; false when it does not match or matching fails.
 */
bool equ_match_symbolic_quadratic_expr(const expr_t *expr, const expr_t *wrt, expr_t **constant_out,
                                       expr_t **linear_out, expr_t **quadratic_out);

/**
 * @brief Solve for a borrowed variable into caller-owned solution storage, returning the native solver status.
 * @param equation Borrowed equation whose native metadata or solution set is inspected or updated as documented.
 * @param wrt Borrowed native variable identifying the coordinate of differentiation, integration, sampling or
 * solving.
 * @param solutions Caller-owned solution storage; successful solution equations are released by
 * equ_solutions_clear.
 * @return Native solver status, preserving the equation or goal-seek API status convention.
 */
int equ_solve_for_into(const equation_t *equation, const expr_t *wrt, equation_solutions_t *solutions);

/**
 * @brief Solve a quadratic coefficient array for native equation and ODE analysis.
 * @param coeffs Borrowed three coefficients in ascending power order.
 * @param wrt Borrowed variable whose solutions are constructed.
 * @param solutions Borrowed destination receiving owned solution equations.
 * @return Solver status; negative indicates failure.
 */
int equ_solve_quadratic_coefficients(const number_t *coeffs, const expr_t *wrt, equation_solutions_t *solutions);

/**
 * @brief Solve a cubic coefficient array for native equation and ODE analysis.
 * @param coeffs Borrowed four coefficients in ascending power order.
 * @param wrt Borrowed variable whose solutions are constructed.
 * @param solutions Borrowed destination receiving owned solution equations.
 * @return Solver status; negative indicates failure.
 */
int equ_solve_cubic_coefficients(const number_t *coeffs, const expr_t *wrt, equation_solutions_t *solutions);

/**
 * @brief Solve a quartic coefficient array for native equation and ODE analysis.
 * @param coeffs Borrowed five coefficients in ascending power order.
 * @param wrt Borrowed variable whose solutions are constructed.
 * @param solutions Borrowed destination receiving owned solution equations.
 * @return Solver status; negative indicates failure.
 */
int equ_solve_quartic_coefficients(const number_t *coeffs, const expr_t *wrt, equation_solutions_t *solutions);

/**
 * @brief Release solution equations and reset caller-owned storage without freeing the container.
 * @param solutions Caller-owned solution storage; successful solution equations are released by
 * equ_solutions_clear.
 */
void equ_solutions_clear(equation_solutions_t *solutions);

/**
 * @brief Render long finite root sets with shared exact definitions; NULL retains the ordinary output.
 * Optional order indexes the solutions. Expression style places definitions after a binding bar.
 * @param solutions Caller-owned solution storage; successful solution equations are released by
 * equ_solutions_clear.
 * @param order Borrowed optional solution-order index array; NULL preserves the native solution order.
 * @param style Presentation style selecting the requested native or TeX representation.
 * @return Owned text released with string_free, or NULL when no representation is available or allocation fails.
 */
string_t *equ_solutions_compact_text(const equation_solutions_t *solutions, const size_t *order, style_t style);

/**
 * @brief Render structurally opposite displacements about a shared centre, retaining any common denominator.
 * @param first Borrowed solution equation in the structurally paired result set.
 * @param second Borrowed solution equation in the structurally paired result set.
 * @param style Presentation style selecting the requested native or TeX representation.
 * @return Owned text released with string_free, or NULL when no representation is available or allocation fails.
 */
string_t *equ_solution_pair_rhs_text(const equation_t *first, const equation_t *second, style_t style);

/**
 * @brief Attach faithful native mathematical notation to an equation's underlying expression trees.
 * @param equation Borrowed equation whose native metadata or solution set is inspected or updated as documented.
 * @param lhs Borrowed mathematical side or copied display notation, according to the declared parameter type.
 * @param rhs Borrowed mathematical side or copied display notation, according to the declared parameter type.
 * @return Zero on success; a non-zero error status on failure.
 */
int equ_set_display_TeX(equation_t *equation, const string_t *lhs, const string_t *rhs);

/**
 * @brief Attach display-only plain notation; expression and function styles retain the underlying algebra.
 * @param equation Borrowed equation whose native metadata or solution set is inspected or updated as documented.
 * @param lhs Borrowed mathematical side or copied display notation, according to the declared parameter type.
 * @param rhs Borrowed mathematical side or copied display notation, according to the declared parameter type.
 * @return Zero on success; a non-zero error status on failure.
 */
int equ_set_display_unbound(equation_t *equation, const string_t *lhs, const string_t *rhs);

#endif
