#ifndef DIFFEQUATION_H
#define DIFFEQUATION_H

#include <stddef.h>

#include "equation.h"
#include "matrix.h"

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
 * from explicit axis-aligned boundary data. The two-dimensional Laplace
 * equation is solved in Cartesian and polar coordinates as a general
 * harmonic family when no boundary data are supplied.
 */

/**
 * @brief An opaque differential-equation problem.
 */
typedef struct diffequ_t diffequ_t;
typedef struct diffequ_solve_result_t diffequ_solve_result_t;

/** @brief Lie point-symmetry analysis of a scalar second-order ODE. */
typedef struct de_lie_t de_lie_t;

/**
 * @brief Normalise a scalar second-order ODE to y'' = f(x,y,p), where p = y'.
 * @param de Problem to analyse; conditions are not imposed on the symmetry algebra.
 * @return An owning analysis, or NULL for an unsupported normal form or allocation failure.
 * The equation must be linear in its highest derivative. Analysis is local where
 * its leading coefficient is non-zero. Release the result with de_lie_free().
 */
de_lie_t *de_lie_new(const diffequ_t *de);

/** @brief Release an analysis and its owned expressions; NULL is harmless. */
void de_lie_free(de_lie_t *lie);

/**
 * @brief Borrow a jet coordinate: index 0 is x, 1 is y and 2 is p = y'.
 * @return A borrowed expression, or NULL for a null analysis or invalid index.
 * The velocity symbol is chosen to avoid collisions with the original equation.
 */
const expr_t *de_lie_coordinate(const de_lie_t *lie, size_t index);

/** @brief Borrow the normalised right-hand side f, or NULL for a null analysis. */
const expr_t *de_lie_rhs(const de_lie_t *lie);

/**
 * @brief Construct the first or second prolongation coefficient on y'' = f.
 * @param lie Analysis specifying the jet coordinates and right-hand side.
 * @param xi Independent-coordinate component of a point generator.
 * @param eta Dependent-coordinate component; xi and eta must not depend on p.
 * @param order Prolongation order, either 1 or 2.
 * @return An owning expression, or NULL for invalid input or allocation failure.
 */
expr_t *de_lie_prolongation(const de_lie_t *lie, const expr_t *xi, const expr_t *eta, size_t order);

/**
 * @brief Construct the infinitesimal invariance residual for a supplied point generator.
 * @return An owning expression eta^(2)-xi*f_x-eta*f_y-eta^(1)*f_p, or NULL on error.
 * An identically zero residual verifies the generator; a non-zero expression
 * is not a symmetry certificate. Release the expression with expr_free().
 */
expr_t *de_lie_residual(const de_lie_t *lie, const expr_t *xi, const expr_t *eta);

/**
 * @brief Construct the determining equation using arbitrary functions xi(x,y) and eta(x,y).
 * @return An owning equation whose left side is the invariance residual and whose right side is zero,
 * or NULL on error. This constructs, but does not generally solve, the determining PDE.
 */
equation_t *de_lie_determining_equation(const de_lie_t *lie);

/**
 * @brief Compute a Lie-Tresse relative invariant of the normalised equation.
 * @param lie Analysis to inspect.
 * @param index Zero selects f_pppp; one selects the second Lie-Tresse invariant.
 * @return An owning expression, or NULL for invalid input or allocation failure.
 * Both invariants vanishing identically is the local point-linearisability criterion.
 * Evaluating them at a single point is not sufficient.
 */
expr_t *de_lie_invariant(const de_lie_t *lie, size_t index);

/**
 * @brief Search for polynomial point generators by exact coefficient matching.
 * @param lie Analysis to inspect.
 * @param degree Maximum total degree of xi and eta, from zero through four.
 * @return An owning expression matrix with two rows (xi, eta) and one column per generator,
 * or NULL when the search is unsupported or fails. A zero-column matrix means
 * no generator was found within the requested polynomial space, not that no symmetry exists.
 * Residuals must be polynomials of degree at most 15 in each jet coordinate
 * with finite real numeric coefficients. Every returned generator is verified symbolically.
 */
matrix_t *de_lie_polynomial_generators(const de_lie_t *lie, size_t degree);

/**
 * @brief Compute the Lie bracket of two columns of a two-row point-generator matrix.
 * @return An owning two-row, one-column expression matrix, or NULL for invalid input or failure.
 * The convention is [G_i,G_j] = G_i G_j - G_j G_i.
 */
matrix_t *de_lie_bracket(const de_lie_t *lie, const matrix_t *generators, size_t i, size_t j);

/**
 * @brief Compute and verify structure constants for a polynomial generator basis.
 * @return An owning expression matrix, or NULL for a dependent, non-closed, non-polynomial
 * or invalid basis, or allocation failure. Row i*n+j contains C_ij^k in column k.
 * Coefficients are finite real constants and bracket reconstruction is checked exactly.
 * An empty basis returns a zero-row, zero-column matrix.
 * The basis may have at most 30 columns. The same degree-15 coefficient bound as the polynomial search applies.
 */
matrix_t *de_lie_structure_constants(const de_lie_t *lie, const matrix_t *generators);

/**
 * @brief Reduce an autonomous equation using its independent-coordinate translation symmetry.
 * @return An owning first-order equation p*dp/dy = f(y,p), or NULL if f depends on x or on error.
 * Here p is regarded as a function of y. This local reduction uses y as a coordinate
 * where p is non-zero; equilibrium solutions must be checked in the original ODE.
 */
equation_t *de_lie_autonomous_reduction(const de_lie_t *lie);

/**
 * @brief Optional work requested from a differential-equation solve.
 *
 * Options may be combined with bitwise OR and passed to
 * de_solve_with_options().
 */
typedef enum de_solve_option_t {
    DE_SOLVE_OPTION_NONE = 0u,
    /** Construct plain-text and TeX derivations for presentation. */
    DE_SOLVE_OPTION_STEPS = 1u << 0
} de_solve_option_t;

/**
 * @brief Outcome of a differential-equation solve attempt.
 */
typedef enum {
    DE_SOLVE_STATUS_SOLVED,
    DE_SOLVE_STATUS_UNSUPPORTED,
    DE_SOLVE_STATUS_INVALID,
    DE_SOLVE_STATUS_FAILED,
    /** A local Taylor expansion with an explicit remainder, not an exact finite solution. */
    DE_SOLVE_STATUS_SERIES
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
    DE_SOLVER_STATIONARY_EIGENFUNCTION,
    /* Backwards-compatible name retained for existing callers. */
    DE_SOLVER_HYDROGEN_MATRIX = DE_SOLVER_STATIONARY_EIGENFUNCTION,
    DE_SOLVER_EXACT_FIRST_ORDER,
    DE_SOLVER_LAPLACE,
    DE_SOLVER_POWER_LAW_BESSEL,
    DE_SOLVER_TAYLOR_SERIES,
    /** Three-dimensional constant-speed wave equation, represented by Kirchhoff spherical means. */
    DE_SOLVER_KIRCHHOFF,
    /** Forced one-dimensional constant-speed wave IVP, using d'Alembert and Duhamel integrals. */
    DE_SOLVER_DALEMBERT_DUHAMEL,
    /** Whole-line constant-coefficient dissipative evolution, represented by a Fourier kernel. */
    DE_SOLVER_FOURIER_EVOLUTION,
    /** Zero-background KdV solitary waves: a particular family, not the general solution. */
    DE_SOLVER_KDV_SOLITARY_WAVE,
    /** Positive-integer-power generalised KdV travelling waves, with explicit branch and domain restrictions. */
    DE_SOLVER_GKDV_TRAVELLING_WAVE,
    /** Heat-reaction equation on the right half-line with zero initial data and a Dirichlet boundary history. */
    DE_SOLVER_HALF_LINE_HEAT
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
 * In addition to applied derivatives such as `Dx(y)` and `Dxx(y)`, the parser
 * accepts polynomial differential operators, for example
 * `(Dx^2 + 4Dx + 20)^2(y) = 0` and `(D^2 + @omega^2)x = 0`, and expands them
 * before constructing the problem. Literal positive integer powers through
 * 64 are supported, including repeated quadratic factors such as
 * `(D^2 + @omega^2)^3x = 0`. A bare `D` consistently defaults to `Dt`
 * only for dependent `x`, and to `Dx` for every other dependent variable,
 * including in direct forms such as `D^2(y)` and `D^2(x)`. An explicit suffix
 * takes precedence.
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
size_t de_condition_argument_count(const diffequ_t *de, size_t condition_index);

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
const expr_t *de_condition_argument_at(const diffequ_t *de, size_t condition_index, size_t argument_index);

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
 * Before rejecting an unsolved second-order ODE, a polynomial normal form
 * may produce a degree-six local Taylor expansion with status
 * ::DE_SOLVE_STATUS_SERIES. See de_solve_series() for its scope and limitations.
 * This default entry point does not construct presentation derivations; use
 * de_solve_with_options() with ::DE_SOLVE_OPTION_STEPS when they are needed.
 *
 * @param de Differential-equation problem to solve.
 * @return An owning solve result, or `NULL` only when the result object itself
 *         cannot be allocated. Release it with de_solve_result_free().
 */
diffequ_solve_result_t *de_solve(const diffequ_t *de);

/**
 * @brief Attempt to solve a differential equation with optional output.
 *
 * This performs the same symbolic solve as de_solve(). Pass
 * ::DE_SOLVE_OPTION_STEPS when a presenting client also needs the derivation
 * returned by de_solve_result_steps() and de_solve_result_steps_TeX().
 * Omitting that option avoids constructing the derivation.
 *
 * @param de Differential-equation problem to solve.
 * @param options Bitwise OR of ::de_solve_option_t values.
 * @return An owning solve result, or `NULL` only when the result object itself
 *         cannot be allocated. Release it with de_solve_result_free().
 */
diffequ_solve_result_t *de_solve_with_options(const diffequ_t *de, unsigned int options);

/**
 * @brief Construct a local Taylor solution of a polynomial second-order ODE.
 *
 * Requires a finite non-zero numeric coefficient of the highest derivative and
 * a normal form y'' = f(x,y,y') polynomial in its three coordinates (degree at
 * most 15 in each coordinate). Other symbols denote finite constant parameters.
 * Conditions may specify y and/or y' at one common point; missing data remain
 * arbitrary constants. With no conditions the expansion point is zero.
 * Boundary data at different points and other condition forms are unsupported.
 * The returned equation includes O((x-x0)^(degree+1)); its status is
 * ::DE_SOLVE_STATUS_SERIES. The infinite Taylor series converges locally, but
 * no convergence radius or numerical truncation-error bound is supplied.
 *
 * @param de Problem to expand. Must not be NULL.
 * @param degree Highest retained power, from 2 through 8 inclusive.
 * @param options Optional ::DE_SOLVE_OPTION_STEPS presentation work.
 * @return An owning result, or NULL on result-allocation failure.
 */
diffequ_solve_result_t *de_solve_series(const diffequ_t *de, size_t degree, unsigned int options);

/**
 * @brief Borrow the expansion point of a local series result.
 * @return A borrowed expression, or NULL for a non-series or null result.
 */
const expr_t *de_solve_result_series_centre(const diffequ_solve_result_t *result);

/**
 * @brief Return the highest retained power of a local Taylor expansion.
 * @return The degree, or zero for a non-series or null result.
 */
size_t de_solve_result_series_degree(const diffequ_solve_result_t *result);

/**
 * @brief Borrow a Taylor coefficient multiplying (x-x0)^index.
 * @return A borrowed expression, or NULL for an unavailable or out-of-range coefficient.
 */
const expr_t *de_solve_result_series_coefficient(const diffequ_solve_result_t *result, size_t index);

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
de_solve_status_t de_solve_result_status(const diffequ_solve_result_t *result);

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
const char *de_solve_result_diagnostic(const diffequ_solve_result_t *result);

/**
 * @brief Borrow the mathematical derivation produced by the selected solver.
 *
 * The text uses conventional Unicode mathematical notation and is intended
 * for presentation by thin clients. It is constructed only when the solve
 * was requested with ::DE_SOLVE_OPTION_STEPS. The derivation names the
 * parameterised rule selected from the parsed expression tree; it is not a
 * stored explanation for one literal input equation.
 *
 * @param result Result to inspect.
 * An unsolved second-order ODE may provide Lie analysis and an order reduction
 * without claiming a closed-form solution.
 * @return Borrowed multiline UTF-8 text, or `NULL` when derivations were not
 *         requested or no derivation is available.
 */
const char *de_solve_result_steps(const diffequ_solve_result_t *result);
const char *de_solve_result_steps_TeX(const diffequ_solve_result_t *result);

/**
 * @brief Borrow the symmetry group identified by the selected solver.
 *
 * @param result Result to inspect.
 * @return Borrowed UTF-8 group name, or `NULL` when none was identified.
 */
const char *de_solve_result_symmetry(const diffequ_solve_result_t *result);

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
 * A solution may be a particular family rather than a general solution.
 * KdV solitary waves retain their scope and parameter restrictions in native
 * presentation, even without optional derivation steps; inspect the diagnostic.
 *
 * When de_solve_result_parameter_constraint() is non-NULL, the first family
 * must be solved jointly with that constraint. Later families are separate
 * alternatives; for a gradient-envelope result, the second is the affine
 * complete integral. Native unbound and TeX renderings include these labels
 * and conditions; other consumers must inspect the constraint explicitly.
 *
 * @param result Result to inspect.
 * @param index Zero-based solution index.
 * @return A borrowed equation, or `NULL` when @p index is out of range.
 */
const equation_t *de_solve_result_at(const diffequ_solve_result_t *result, size_t index);

/**
 * @brief Borrow the auxiliary parameter of a local envelope solution.
 *
 * The parameter is determined jointly with the first solution equation by
 * de_solve_result_parameter_constraint(), not a free constant in that family.
 * @param result Result to inspect.
 * @return Borrowed parameter, or `NULL` for a result without an envelope.
 */
const expr_t *de_solve_result_parameter(const diffequ_solve_result_t *result);

/**
 * @brief Borrow the constraint determining the first family's envelope parameter.
 *
 * Regular local branches require a nonzero derivative of the constraint
 * residual with respect to de_solve_result_parameter(). Native output also
 * states the coefficient-domain restrictions. The condition is retained even
 * when derivation steps were not requested.
 * @param result Result to inspect.
 * @return Borrowed constraint equation, or `NULL` when none is required.
 */
const equation_t *de_solve_result_parameter_constraint(const diffequ_solve_result_t *result);

#endif /* DIFFEQUATION_H */
