/**
 * @file equation_internal.h
 * @brief Private algebraic solver construction and polynomial helpers.
 *
 * Declares equation-owned binding construction, coefficient matching and specialised solution paths. It extends
 * the controlled shared equation interface for implementation units inside the equation module.
 *
 * This header is an implementation detail under src/, not an installed public API. Keep its consumers within the
 * documented module boundary and preserve any explicit internal-access guards.
 */

#ifndef EQUATION_INTERNAL_H
#define EQUATION_INTERNAL_H

#if !defined(MARS_EQUATION_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_EQUATION_INTERNAL_ACCESS."
#endif

#include <stdbool.h>
#include <stddef.h>

#include "equation-t.h"
#include "number-t.h"

#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#define MARS_SHARED_EQUATION_INTERNAL_ACCESS
#include "internal/equation_internal.h"

/** @brief Mathematical relation tokens owned by the native equation input grammar. */
typedef enum {
    EQU_RELATION_EQUAL,       /**< Exact equality, written = or ==. */
    EQU_RELATION_NOT_EQUAL,   /**< Unequal operands, written != or its Unicode sign. */
    EQU_RELATION_LESS,        /**< Strict ordering, written <. */
    EQU_RELATION_LESS_EQUAL,  /**< Inclusive ordering, written <= or its Unicode sign. */
    EQU_RELATION_GREATER,     /**< Strict ordering, written >. */
    EQU_RELATION_GREATER_EQUAL, /**< Inclusive ordering, written >= or its Unicode sign. */
    EQU_RELATION_APPROXIMATE, /**< Approximate equality, written ~~= or its Unicode sign. */
    EQU_RELATION_CONGRUENT,   /**< Congruence, written ~== or its Unicode sign. */
    EQU_RELATION_COUNT        /**< Direct-dispatch table bound; not a valid relation. */
} equ_relation_kind_t;

/** @brief One relation token located in a borrowed native mathematical input view. */
typedef struct {
    size_t position; /**< Byte position of the relation's first rune in the input view. */
    size_t width; /**< Number of bytes occupied by the complete relation token. */
    equ_relation_kind_t kind; /**< Mathematical meaning; no TeX syntax is stored. */
} equ_input_relation_t;

/**
 * @brief Borrow an equation body's outer envelope and optional binding declarations.
 * @param text Borrowed source kept alive while either output view is used; never modified or retained.
 * @param[out] body Required output receiving the trimmed body, without outer braces or bindings.
 * @param[out] bindings Required output receiving a borrowed binding view, or an empty view if absent.
 * @param[out] has_bindings Required output distinguishing an explicit binding bar from an absent one.
 * @return True on success; false for invalid arguments, empty input or an invalid outer wrapper.
 * @note No ownership transfers and no expression or binding is evaluated. Outputs require no release.
 */
bool equ_input_parts(const string_t *text, string_view_t *body, string_view_t *bindings, bool *has_bindings);

/**
 * @brief Read the next top-level mathematical relation using longest-token matching.
 * @param body Borrowed source view, neither modified nor retained.
 * @param[in,out] start Required byte offset, initially zero or immediately after the previous relation.
 * On a match it advances beyond the token; at end it becomes the view length.
 * @param[out] relation Required output receiving a matched token; unchanged if no token is returned.
 * @return One for a match, zero for the end, or minus one with EINVAL for malformed input or arguments.
 * @note Parentheses, matrix brackets and transform braces hide their internal separators. Text is traversed
 * once between matches; dispatch on each token prefix has bounded cost. No allocation or ownership transfer occurs.
 */
int equ_input_next_relation(string_view_t body, size_t *start, equ_input_relation_t *relation);

equation_t *equ_new_with_owned_bindings(const expr_t *lhs, const expr_t *rhs, expr_bindings_t *bindings);

struct _expr_function_temporaries_t;

/** @brief Reserve and report one solver attempt, search seed or numerical iteration in the current scope. */
bool equ_computation_step(const char *stage);

/** @brief Poll the current scope before fallback or publishing solutions; no work is reserved. */
bool equ_computation_active(void);

/**
 * @brief Render solution centres and offsets with structural symmetry and shared denominators.
 * @param plan Required borrowed temporary-binding plan.
 * @param rows Borrowed centre/offset pairs, preceded by a lone root when count is odd.
 * @param count Number of entries in rows, at most sixteen.
 * @param style Unbound expression or LaTeX output style.
 * @param rendered Zero-initialised output array; caller frees every populated string, even on failure.
 * @return True on success; false on allocation failure or invalid count.
 */
bool equ_solution_pairs_text(const struct _expr_function_temporaries_t *plan, const expr_t *const *rows,
                             size_t count, style_t style, string_t **rendered);

int equ_set_display_TeX(equation_t *equation, const string_t *lhs, const string_t *rhs);
const string_t *equ_lhs_display_TeX(const equation_t *equation);
const string_t *equ_rhs_display_TeX(const equation_t *equation);
const string_t *equ_lhs_display_unbound(const equation_t *equation);
const string_t *equ_rhs_display_unbound(const equation_t *equation);
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
bool equ_collect_symbolic_polynomial_alloc(const expr_t *expr, const expr_t *wrt, expr_t ***coefficients_out,
                                           size_t *degree_out);

bool equ_match_polynomial_expr(const expr_t *expr, const expr_t *wrt, size_t max_degree, number_t *coeffs_out);

bool equ_polynomial_coefficients_real(const number_t *coeffs, size_t degree);

bool equ_polynomial_root_effectively_real(number_t root, number_t tolerance);

void equ_polynomial_deflate_conjugate_pair(const number_t *coeffs, size_t degree, number_t root, number_t *deflated);

int equ_append_solution_value(const expr_t *wrt, number_t value, equation_solutions_t *solutions);

int equ_append_solution_expr(const expr_t *wrt, const expr_t *rhs, equation_solutions_t *solutions);

/* Propose a rational approximation without claiming that it is an exact root or coefficient. */
number_t equ_polynomial_rational_candidate(number_t value);

/**
 * @brief Invert a principal square root, exponential or logarithm on either isolated equation side.
 * @param equation Borrowed source equation; its sides and bindings are not modified.
 * @param wrt Borrowed variable to isolate.
 * @param solutions Borrowed collection receiving reduced solutions or a proved-empty status.
 * @return Zero after a handled reduction, one when inapplicable, or minus one on allocation or computation failure.
 * @note Square-root targets must have a known principal-root range. Constant-base exponential inversion requires
 * a finite positive real base different from one; it returns the principal logarithmic inverse.
 */
int equ_try_solve_unary_inverse(const equation_t *equation, const expr_t *wrt, equation_solutions_t *solutions);

int equ_try_solve_cubic(const equation_t *equation, const expr_t *wrt, equation_solutions_t *solutions);

int equ_try_solve_quartic(const equation_t *equation, const expr_t *wrt, equation_solutions_t *solutions);

int equ_try_quartic_radicals(const number_t *coeffs, const expr_t *wrt, equation_solutions_t *solutions);

int equ_try_solve_quintic(const equation_t *equation, const expr_t *wrt, equation_solutions_t *solutions);

int equ_try_solve_general_polynomial(const equation_t *equation, const expr_t *wrt, equation_solutions_t *solutions);

int equ_solve_numeric_into(const equation_t *equation, expr_bindings_t *bindings,
                           const expr_goal_seek_options_t *options, equation_solutions_t *solutions);

expr_bindings_t *equ_bindings_borrow(const equation_t *equation);

/* Return one when inapplicable, zero after a bounded search, or minus one on allocation failure. */
int equ_try_search_zeta_roots(const equation_t *equation, equation_solutions_t *solutions);

/**
 * @brief Create an owned equation notation snapshot, released with TeX_context_free.
 * @param value Borrowed matrix or equation to copy into the semantic snapshot; the source remains unchanged.
 * @return Owned rendering context released with TeX_context_free, or NULL on invalid input or failure.
 */
TeX_context_t *equ_TeX_context(const equation_t *value);

/**
 * @brief Snapshot both equation sides in a borrowed builder.
 * @param build Borrowed semantic snapshot builder; it owns all nodes added by this call.
 * @param value Borrowed matrix or equation to copy into the semantic snapshot; the source remains unchanged.
 * @return Model-owned semantic node, or NULL on invalid input or allocation failure; do not free it individually.
 */
const TeX_model_node_t *notation_build_equation(notation_build_t *build, const equation_t *value);

#endif
