#ifndef DIFFEQUATION_H
#define DIFFEQUATION_H

#include <stddef.h>

#include "equation.h"

/**
 * @file diffequation.h
 * @brief Construction, parsing, inspection, and formatting of differential
 *        equations.
 *
 * A problem consists of a base equation, its independent variables, constant
 * bindings, and optional initial or boundary conditions. The first symbolic
 * solvers cover exact differential forms, separable, linear, homogeneous,
 * affine-substitution, linear
 * changes of variables, quadratic Bernoulli, and autonomous
 * derivative-quadratic first-order ordinary differential equations, together
 * with arbitrary-order constant-coefficient linear ODEs and second-order
 * equations that can be completed through Sturm-Liouville normalization.
 * Two-variable constant-coefficient homogeneous transport PDEs are solved
 * from explicit axis-aligned boundary data.
 */

/**
 * @brief An opaque differential-equation problem.
 */
typedef struct diffequ_t diffequ_t;
typedef struct diffequ_solve_result_t diffequ_solve_result_t;

/**
 * @brief Outcome of a differential-equation solve attempt.
 */
typedef enum {
    DE_SOLVE_STATUS_SOLVED,
    DE_SOLVE_STATUS_UNSUPPORTED,
    DE_SOLVE_STATUS_INVALID,
    DE_SOLVE_STATUS_FAILED
} de_solve_status_t;

/**
 * @brief Solver family selected for a differential equation.
 */
typedef enum {
    DE_SOLVER_NONE,
    DE_SOLVER_SEPARABLE,
    DE_SOLVER_LINEAR,
    DE_SOLVER_BERNOULLI,
    DE_SOLVER_HOMOGENEOUS,
    DE_SOLVER_LINEAR_SUBSTITUTION,
    DE_SOLVER_LINEAR_TRANSFORMATION,
    DE_SOLVER_STURM_LIOUVILLE,
    DE_SOLVER_CONSTANT_COEFFICIENT_LINEAR,
    DE_SOLVER_DERIVATIVE_QUADRATIC,
    DE_SOLVER_EXACT_DERIVATIVE_LINEARIZATION,
    DE_SOLVER_CONSTANT_COEFFICIENT_TRANSPORT,
    DE_SOLVER_CHARACTERISTICS,
    DE_SOLVER_PARAMETER_LINEAR_PDE,
    DE_SOLVER_HYDROGEN_MATRIX,
    DE_SOLVER_EXACT_FIRST_ORDER
} de_solver_t;

/**
 * @brief Construct a differential-equation problem from a base equation.
 *
 * The base expressions are retained by a new equation wrapper. The returned
 * problem initially has no independent-variable declarations, constants, or
 * conditions.
 *
 * @param equation Base equation to retain. Must not be `NULL`.
 * @return A newly allocated differential-equation problem, or `NULL` on
 *         invalid input or allocation failure.
 */
diffequ_t *de_new(const equation_t *equation);

/**
 * @brief Parse a differential-equation problem from canonical text.
 *
 * @param text Null-terminated input text. Must not be `NULL`.
 * @return A newly allocated differential-equation problem, or `NULL` when the
 *         text is invalid or allocation fails.
 */
diffequ_t *de_from_string(const char *text);

/**
 * @brief Parse a differential-equation problem from MARS string text.
 *
 * @param text Input string. Must not be `NULL`.
 * @return A newly allocated differential-equation problem, or `NULL` when the
 *         text is invalid or allocation fails.
 */
diffequ_t *de_from_text(const string_t *text);

/**
 * @brief Destroy a differential-equation problem.
 *
 * @param de Owning problem handle. May be `NULL`.
 */
void de_free(diffequ_t *de);

/**
 * @brief Access the base differential equation.
 *
 * @param de Problem to inspect.
 * @return A borrowed equation, or `NULL` when @p de is `NULL`.
 *
 * The returned equation remains owned by @p de and must not be freed.
 */
const equation_t *de_equation(const diffequ_t *de);

/**
 * @brief Get the number of declared independent variables.
 *
 * @param de Problem to inspect.
 * @return The independent-variable count, or zero when @p de is `NULL`.
 */
size_t de_independent_count(const diffequ_t *de);

/**
 * @brief Access a declared independent variable by position.
 *
 * @param de Problem to inspect.
 * @param index Zero-based variable index.
 * @return A borrowed expression, or `NULL` when @p de is `NULL` or @p index is
 *         out of range.
 *
 * The returned expression remains owned by @p de and must not be freed.
 */
const expr_t *de_independent_at(const diffequ_t *de, size_t index);

/**
 * @brief Access all constant bindings associated with a problem.
 *
 * @param de Problem to inspect.
 * @return Borrowed bindings, or `NULL` when @p de is `NULL` or has no binding
 *         collection.
 *
 * The returned bindings remain owned by @p de and must not be freed.
 */
expr_bindings_t *de_constants(const diffequ_t *de);

/**
 * @brief Look up a named problem constant.
 *
 * @param de Problem to inspect.
 * @param name Null-terminated constant name.
 * @return A borrowed constant expression, or `NULL` when no such constant
 *         exists or either argument is `NULL`.
 *
 * The returned expression remains owned by @p de and must not be freed.
 */
expr_t *de_constant(const diffequ_t *de, const char *name);

/**
 * @brief Get the number of initial or boundary conditions.
 *
 * @param de Problem to inspect.
 * @return The condition count, or zero when @p de is `NULL`.
 */
size_t de_condition_count(const diffequ_t *de);

/**
 * @brief Access an initial or boundary condition by position.
 *
 * @param de Problem to inspect.
 * @param index Zero-based condition index.
 * @return A borrowed equation, or `NULL` when @p de is `NULL` or @p index is
 *         out of range.
 *
 * The returned equation remains owned by @p de and must not be freed.
 */
const equation_t *de_condition_at(const diffequ_t *de, size_t index);

/**
 * @brief Get the number of local arguments on a condition.
 *
 * For example, `u(x, 0) = f(x)` has two arguments while the ODE condition
 * `y(0) = 1` has one.
 *
 * @param de Problem to inspect.
 * @param condition_index Zero-based condition index.
 * @return The argument count, or zero when the condition has no local
 *         arguments or the indices are invalid.
 */
size_t de_condition_argument_count(const diffequ_t *de,
                                   size_t condition_index);

/**
 * @brief Access one local condition argument.
 *
 * @param de Problem to inspect.
 * @param condition_index Zero-based condition index.
 * @param argument_index Zero-based local argument index.
 * @return A borrowed expression, or `NULL` when either index is invalid.
 *
 * The returned expression remains owned by @p de and must not be freed.
 */
const expr_t *de_condition_argument_at(const diffequ_t *de,
                                       size_t condition_index,
                                       size_t argument_index);

/**
 * @brief Format a differential-equation problem as an MARS string.
 *
 * `style_EXPRESSION` uses conventional derivative fractions: ordinary
 * derivatives are written as `dy/dx` and partial derivatives as `∂u/∂x`.
 * The resulting notation is accepted by de_from_string().
 *
 * @param de Problem to format. Must not be `NULL`.
 * @param style Output style.
 * @return A newly allocated string, or `NULL` on invalid input, unsupported
 *         style, or allocation failure. The caller owns the returned string
 *         and must release it with string_free().
 */
string_t *de_to_text(const diffequ_t *de, style_t style);

/**
 * @brief Format a differential-equation problem as a C string.
 *
 * @param de Problem to format. Must not be `NULL`.
 * @param style Output style.
 * @return A newly allocated null-terminated string, or `NULL` on invalid input,
 *         unsupported style, or allocation failure. The caller owns the
 *         returned string and must release it with free().
 */
char *de_to_string(const diffequ_t *de, style_t style);

/**
 * @brief Attempt to solve a differential-equation problem symbolically.
 *
 * The implementation recognises first-order separable, first-order linear,
 * homogeneous, substitution, quadratic Bernoulli, and autonomous
 * derivative-quadratic ordinary differential equations. Exact third-order
 * forms `a*y''' + k*y'*y'' = f(x)` are integrated once and logarithmically
 * linearized. The solver also normalizes regular second-order linear equations
 * to Sturm-Liouville form and solves arbitrary-order constant-coefficient
 * linear ODEs, including nonhomogeneous forcing.
 * First-order linear equations use an integrating factor.
 * Homogeneous equations use the substitution `y = u*x`. Equations depending
 * on an affine combination use `u = a*x + b*y + c`; ratios of two
 * non-parallel affine combinations are translated to homogeneous form.
 * Nonsingular linear changes `Y = a*x + b*y`, `X = c*x + d*y` are accepted
 * when they make the transformed equation separable. When symbolic
 * integration has no closed form, an exact unevaluated integral is retained
 * in the solution.
 * Two-variable equations `a*Dx(u) + b*Dy(u) = 0` are solved by
 * characteristics when `a` and `b` are nonzero constants and explicit data
 * is supplied on a constant-`x` or constant-`y` boundary.
 * Unsupported but well-formed problems return a result with
 * ::DE_SOLVE_STATUS_UNSUPPORTED rather than returning `NULL`.
 *
 * @param de Differential-equation problem to solve.
 * @return An owning solve result, or `NULL` only when the result object itself
 *         cannot be allocated. Release it with de_solve_result_free().
 */
diffequ_solve_result_t *de_solve(const diffequ_t *de);

/**
 * @brief Destroy a differential-equation solve result.
 *
 * @param result Owning result handle. May be `NULL`.
 */
void de_solve_result_free(diffequ_solve_result_t *result);

/**
 * @brief Return the solve status.
 *
 * @param result Result to inspect.
 * @return Its status, or ::DE_SOLVE_STATUS_INVALID when @p result is `NULL`.
 */
de_solve_status_t de_solve_result_status(
    const diffequ_solve_result_t *result);

/**
 * @brief Return the solver family used for a successful result.
 *
 * @param result Result to inspect.
 * @return Its solver family, or ::DE_SOLVER_NONE when unavailable.
 */
de_solver_t de_solve_result_solver(const diffequ_solve_result_t *result);

/**
 * @brief Borrow a diagnostic message describing the result.
 *
 * @param result Result to inspect.
 * @return A borrowed null-terminated message, or `NULL`.
 */
const char *de_solve_result_diagnostic(
    const diffequ_solve_result_t *result);

/**
 * @brief Borrow the mathematical derivation produced by the selected solver.
 *
 * The text uses conventional Unicode mathematical notation and is intended
 * for presentation by thin clients. Not every solver currently supplies a
 * derivation.
 *
 * @param result Result to inspect.
 * @return Borrowed multiline UTF-8 text, or `NULL` when unavailable.
 */
const char *de_solve_result_steps(
    const diffequ_solve_result_t *result);
const char *de_solve_result_steps_tex(
    const diffequ_solve_result_t *result);

/**
 * @brief Borrow the symmetry group identified by the selected solver.
 *
 * @param result Result to inspect.
 * @return Borrowed UTF-8 group name, or `NULL` when none was identified.
 */
const char *de_solve_result_symmetry(
    const diffequ_solve_result_t *result);

/**
 * @brief Return the number of symbolic solution families.
 *
 * @param result Result to inspect.
 * @return The number of solutions, or zero when unavailable.
 */
size_t de_solve_result_count(const diffequ_solve_result_t *result);

/**
 * @brief Borrow one symbolic solution equation.
 *
 * @param result Result to inspect.
 * @param index Zero-based solution index.
 * @return A borrowed equation, or `NULL` when @p index is out of range.
 */
const equation_t *de_solve_result_at(
    const diffequ_solve_result_t *result,
    size_t index);

#endif /* DIFFEQUATION_H */
