/**
 * @file diffequ_internal.h
 * @brief Private differential-equation problem and result structures.
 *
 * Stores the parsed equation, display forms, independent variables and conditions shared by construction, solving
 * and formatting code. Public callers inspect these through diffequ_t instead of accessing the layout.
 *
 * This header is an implementation detail under src/, not an installed public API. Keep its consumers within the
 * documented module boundary and preserve any explicit internal-access guards.
 */

#ifndef DIFFEQU_INTERNAL_H
#define DIFFEQU_INTERNAL_H

#include <stdbool.h>

#if !defined(MARS_DIFFEQUATION_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_DIFFEQUATION_INTERNAL_ACCESS."
#endif

#include "diffequation-t.h"

typedef struct _diffequ_t {
    equation_t *equation;
    expr_t *display_lhs;
    expr_t *display_rhs;
    string_t *equation_text;
    string_t *differential_form_text;
    bool differential_form_input;
    bool partial_derivative_input;
    bool prime_derivative_input; /**< The authored governing equation contains ordinary derivative primes. */

    expr_t **independent_vars;
    size_t independent_count;
    expr_t **interval_lower; /**< Optional owning strict lower bounds, indexed by independent variable. */
    expr_t **interval_upper; /**< Optional owning strict upper bounds, indexed by independent variable. */

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
} diffequ_t;

typedef struct _diffequ_solve_result_t {
    de_solve_status_t status;
    de_solver_t solver;
    equation_t **solutions;
    size_t solution_count;
    expr_t *envelope_parameter;
    equation_t *parameter_constraint;
    char *diagnostic;
    char *steps;
    char *steps_TeX;
    char *symmetry;
    expr_t *series_centre;
    expr_t **series_coefficients;
    size_t series_degree;
    bool intervals_applied; /**< Authored intervals already restrict every returned solution equation. */
} diffequ_solve_result_t;

/**
 * @brief Parse one strict coordinate interval using the shared native relation scanner.
 * @param de Borrowed mutable problem with independent coordinates already installed.
 * @param text Borrowed condition source; neither changed nor retained.
 * @param names Borrowed symbol-name array used by the mathematical expression parser.
 * @param symbols Borrowed corresponding native expression nodes, retained by parsed bounds as needed.
 * @param symbol_count Length of both symbol arrays.
 * @return One for an installed interval, zero for an ordinary equality condition, or minus one for invalid input.
 * Finite real, coordinate-independent bounds are intersected; an empty intersection is rejected.
 * Partial mutation on failure is discarded by the owning problem parser. No ownership transfers from inputs.
 */
int de_parse_interval(diffequ_t *de, const string_t *text, const string_t *const *names,
                      expr_t *const *symbols, size_t symbol_count);

/**
 * @brief Restrict an expression to the problem's authored strict coordinate intervals.
 * @param de Borrowed problem, not changed or retained.
 * @param expression Borrowed formula; its nodes are retained in the returned expression.
 * @return Owned graph released with expr_free, or NULL for invalid input or allocation failure.
 * Without intervals, returns a retained reference. Bounds and live bindings remain owned by the returned graph.
 */
expr_t *de_restrict_intervals(const diffequ_t *de, const expr_t *expression);

/**
 * @brief Render coordinate intervals as parseable native inequalities, without typesetting syntax.
 * @param de Borrowed problem, neither modified nor retained.
 * @return Owned string released with string_free, empty when no intervals exist, or NULL on invalid input or failure.
 */
string_t *de_intervals_to_text(const diffequ_t *de);

/**
 * @brief Apply authored intervals once to every solution in a solved or series result.
 * @param de Borrowed source problem; neither changed nor retained.
 * @param result Borrowed mutable result whose equations are replaced on success.
 * @return True on success or when no intervals need applying; false for invalid inputs or allocation failure.
 * Failure leaves the result unchanged. Replacement equations own their interval predicates; no input ownership
 * transfers. As with other mutable result operations, callers must serialise access.
 */
bool de_result_apply_intervals(const diffequ_t *de, diffequ_solve_result_t *result);

diffequ_t *de_new_owned(equation_t *equation);
diffequ_solve_result_t *de_solve_result_new(de_solve_status_t status, de_solver_t solver, const char *diagnostic);
int de_solve_result_append(diffequ_solve_result_t *result, equation_t *solution);
int de_solve_result_set_steps(diffequ_solve_result_t *result, const char *steps);
int de_solve_result_set_steps_TeX(diffequ_solve_result_t *result, const char *steps_TeX);
int de_solve_result_set_symmetry(diffequ_solve_result_t *result, const char *symmetry);
int de_solve_result_ensure_rule_steps(const diffequ_t *de, diffequ_solve_result_t *result);
bool de_linear_decompose(const expr_t *expr, const expr_t *needle, expr_t **coefficient_out, expr_t **constant_out);
/* Scan an optional positive derivative order; return 1, 0 (absent), or -1 (invalid). */
int de_parse_derivative_order(const char *text, size_t *position, size_t *order);

#endif /* DIFFEQU_INTERNAL_H */
