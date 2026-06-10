#ifndef EQUATION_H
#define EQUATION_H

#include <stdbool.h>
#include <stddef.h>

#include "expression.h"

typedef struct equation_t equation_t;

/**
 * @brief Result status for equation_solve_for().
 */
typedef enum equation_solve_status {
    EQUATION_SOLVE_INVALID,
    EQUATION_SOLVE_UNSOLVED,
    EQUATION_SOLVE_SOLVED
} equation_solve_status_t;

/**
 * @brief Owning result returned by equation_solve_for().
 *
 * @p solutions is an owning array of owning equation handles. Each solution is
 * expected to be in isolated form, e.g. @c x = expression. Release the result
 * with equation_solve_result_clear().
 */
typedef struct equation_solve_result {
    equation_t **solutions;
    size_t count;
    equation_solve_status_t status;
} equation_solve_result_t;

/**
 * @brief Create an equation from a left- and right-hand expression.
 *
 * The expressions are retained, not consumed. The caller owns the returned
 * equation and must release it with equation_free().
 */
equation_t *equation_new(const expr_t *lhs, const expr_t *rhs);

/**
 * @brief Release an owning equation handle.
 */
void equation_free(equation_t *equation);

/**
 * @brief Borrow the left-hand side expression from an equation.
 */
const expr_t *equation_lhs(const equation_t *equation);

/**
 * @brief Borrow the right-hand side expression from an equation.
 */
const expr_t *equation_rhs(const equation_t *equation);

/**
 * @brief Build the owning residual expression @c lhs - rhs, simplified.
 */
expr_t *equation_residual(const equation_t *equation);

/**
 * @brief Return true when the equation is already isolated as @c wrt = f(...)
 *        and the right-hand side does not contain @p wrt.
 */
bool equation_is_solved_for(const equation_t *equation, const expr_t *wrt);

/**
 * @brief Try to isolate @p wrt on the left-hand side.
 *
 * This first solver slice handles already-isolated equations and non-constant
 * affine equations. Later symbolic solvers can append additional symbolic
 * solution equations to the same result shape.
 */
int equation_solve_for(const equation_t *equation,
                       const expr_t *wrt,
                       equation_solve_result_t *result);

/**
 * @brief Numerically seek one point satisfying @p equation.
 *
 * This is the equation-level counterpart to expr_goal_seek(). It solves the
 * residual @c lhs-rhs = 0 by moving every non-constant variable in @p bindings,
 * then returns one isolated equation per moved variable, e.g. @c x = 3 and
 * @c y = 2. The supplied binding nodes are updated in place.
 *
 * @p bindings should be the binding set that was produced while parsing this
 * equation, or another binding set whose variables are the same nodes used by
 * @p equation.
 */
int equation_solve_numeric(const equation_t *equation,
                           expr_bindings_t *bindings,
                           const expr_goal_seek_options_t *options,
                           equation_solve_result_t *result);

/**
 * @brief Release owning fields in an equation solve result.
 */
void equation_solve_result_clear(equation_solve_result_t *result);

/**
 * @brief Serialise @p equation to newly allocated text.
 *
 * The expression style produces a parseable equation wrapper:
 *
 *   { lhs = rhs }
 *
 * Use equation_from_text() or equation_from_string() to parse it back.
 */
string_t *equation_to_text(const equation_t *equation, style_t style);

/**
 * @brief Print the expression-style string representation of @p equation.
 */
void equation_print(const equation_t *equation);

/**
 * @brief Construct an equation from equation-style text.
 *
 * Accepted forms are:
 *
 *   lhs = rhs
 *   { lhs = rhs }
 *   { lhs = rhs | x = val, ...; [name] = val, ... }
 *
 * The left and right sides share one symbol table, so a variable named on both
 * sides resolves to the same expr_t node. If @p bnd_out is non-NULL, symbolic
 * bindings can be queried with expr_bindings_get() and must later be released
 * with expr_bindings_free().
 */
equation_t *equation_from_string(const char *s, expr_bindings_t **bnd_out);

/**
 * @brief Construct an equation from text stored in a string.
 */
equation_t *equation_from_text(const string_t *text, expr_bindings_t **bnd_out);

#endif
