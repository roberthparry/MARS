/**
 * @file integrator.h
 * @brief Adaptive number_t-based numerical integrators.
 *
 * All public integration entry points operate on number_t bounds, tolerances,
 * and results. The current runtime uses adaptive recursive subdivision with
 * midpoint/Simpson-style error control and bisects the subinterval with the
 * largest estimated error. Iteration stops when:
 *
 *      total_error <= max(abs_tol, rel_tol * |result|)
 *
 * or when the maximum subinterval count is reached.
 */

#ifndef INTEGRATOR_H
#define INTEGRATOR_H

#include <stddef.h>
#include "expression.h"

/** Opaque integrator handle. */
typedef struct _integrator_t integrator_t;

/**
 * @brief Create an integrator with default tolerances.
 *
 * Default absolute and relative tolerances are both 1e-27.
 * Default maximum subinterval count is 5000.
 *
 * @return New integrator, or NULL on allocation failure.
 */
integrator_t *intg_new(void);

/**
 * @brief Free an integrator.  Safe to call with NULL.
 */
void intg_free(integrator_t *ig);

/**
 * @brief Override convergence tolerances.
 *
 * Convergence is declared when total_error <= max(abs_tol, rel_tol * |result|).
 *
 * @param ig       Integrator handle.
 * @param abs_tol  Absolute tolerance.
 * @param rel_tol  Relative tolerance.
 */
void intg_set_tolerance(integrator_t *ig, number_t abs_tol, number_t rel_tol);

/**
 * @brief Override the maximum number of subintervals.
 *
 * @param ig             Integrator handle.
 * @param max_intervals  Upper bound on the subinterval count.
 */
void intg_set_interval_count_max(integrator_t *ig, size_t max_intervals);

/**
 * @brief Integrate an expr_t expression over [a, b].
 *
 * Uses the shared number_t adaptive engine and evaluates an
 * expr_t graph by rebinding @p x_var at each quadrature node.
 *
 * Example:
 * @code
 *   integrator_t *ig = intg_new();
 *   number_t x0  = num_create_from_double(0.0);
 *   expr_t *x    = expr_new_var(x0);
 *   expr_t *expr = expr_sin(x);
 *   number_t result = num_new();
 *   number_t err = num_new();
 *   intg_integral(ig, expr, x, num_create_from_double(0.0), NUM_PI, &result, &err);
 *   // result ≈ 2.0
 *   num_destroy(&err); num_destroy(&result);
 *   expr_free(expr); expr_free(x); num_destroy(&x0); intg_free(ig);
 * @endcode
 *
 * @param ig         Integrator handle.
 * @param expr       expr_t expression representing the integrand f(x).
 * @param x_var      Variable node in @p expr representing x.  Must have been
 *                   created with expr_new_var() or expr_new_named_var().
 * @param a          Lower bound.
 * @param b          Upper bound.
 * @param result     Receives the integral estimate.
 * @param error_est  If non-NULL, receives the final total error estimate.
 *
 * @return  0  Converged within tolerance.
 * @return  1  Maximum subintervals reached before convergence.
 * @return -1  Null argument or internal allocation failure.
 */
int intg_integral(integrator_t *ig, expr_t *expr, expr_t *x_var,
                       number_t a, number_t b,
                       number_t *result, number_t *error_est);

/**
 * @brief Number of subintervals used in the most recent integration call.
 */
size_t intg_get_interval_count_used(const integrator_t *ig);

/**
 * @brief Integrate an expr_t expression over [ax,bx] × [ay,by].
 *
 * Uses recursive applications of the shared number_t adaptive engine over a
 * rectangular 2-D domain. The outer integral adapts in y; the inner integral
 * reuses the same engine in x.
 *
 * @param ig         Integrator handle.
 * @param expr       expr_t expression representing f(x, y).
 * @param x_var      Variable node for x.
 * @param ax         Lower bound in x.
 * @param bx         Upper bound in x.
 * @param y_var      Variable node for y.
 * @param ay         Lower bound in y.
 * @param by         Upper bound in y.
 * @param result     Receives the integral estimate.
 * @param error_est  If non-NULL, receives the final total error estimate.
 *
 * @return  0  Converged within tolerance.
 * @return  1  Maximum subintervals reached before convergence.
 * @return -1  Null argument or internal allocation failure.
 */
int intg_double_integral(integrator_t *ig, expr_t *expr,
                       expr_t *x_var, number_t ax, number_t bx,
                       expr_t *y_var, number_t ay, number_t by,
                       number_t *result, number_t *error_est);

/**
 * @brief Integrate an expr_t expression over [ax,bx] × [ay,by] × [az,bz].
 *
 * Extends intg_double_integral() to 3-D rectangular domains by recursively
 * applying the same number_t adaptive engine.
 *
 * @param ig         Integrator handle.
 * @param expr       expr_t expression representing f(x, y, z).
 * @param x_var      Variable node for x.
 * @param ax         Lower bound in x.
 * @param bx         Upper bound in x.
 * @param y_var      Variable node for y.
 * @param ay         Lower bound in y.
 * @param by         Upper bound in y.
 * @param z_var      Variable node for z.
 * @param az         Lower bound in z.
 * @param bz         Upper bound in z.
 * @param result     Receives the integral estimate.
 * @param error_est  If non-NULL, receives the final total error estimate.
 *
 * @return  0  Converged within tolerance.
 * @return  1  Maximum subintervals reached before convergence.
 * @return -1  Null argument or internal allocation failure.
 */
int intg_triple_integral(integrator_t *ig, expr_t *expr,
                       expr_t *x_var, number_t ax, number_t bx,
                       expr_t *y_var, number_t ay, number_t by,
                       expr_t *z_var, number_t az, number_t bz,
                       number_t *result, number_t *error_est);

/**
 * @brief Integrate a expr_t expression over an N-dimensional rectangular domain.
 *
 * Generalises intg_integral() to arbitrary dimension N using recursive
 * applications of the same number_t adaptive engine. The outermost variable
 * (vars[ndim-1]) is adapted by bisection; all inner variables use fixed bounds.
 *
 * Variable ordering: vars[0] is the innermost integration variable, vars[ndim-1]
 * the outermost.  lo[i] and hi[i] are the bounds for vars[i].
 *
 * Example — ∫₀¹ ∫₀¹ (x+y) dx dy = 1:
 * @code
 *   integrator_t *ig = intg_new();
 *   number_t x0 = num_create_from_double(0.0);
 *   number_t y0 = num_create_from_double(0.0);
 *   expr_t *x = expr_new_var(x0);
 *   expr_t *y = expr_new_var(y0);
 *   expr_t *expr = expr_add(x, y);
 *   expr_t *vars[2] = { x, y };
 *   number_t lo[2] = { num_create_from_double(0.0), num_create_from_double(0.0) };
 *   number_t hi[2] = { num_create_from_double(1.0), num_create_from_double(1.0) };
 *   number_t result = num_new();
 *   number_t err = num_new();
 *   intg_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);
 *   // result ≈ 1.0
 *   expr_free(expr); expr_free(y); expr_free(x);
 *   num_destroy(&err); num_destroy(&result);
 *   num_destroy(&hi[1]); num_destroy(&hi[0]);
 *   num_destroy(&lo[1]); num_destroy(&lo[0]);
 *   num_destroy(&y0); num_destroy(&x0); intg_free(ig);
 * @endcode
 *
 * @param ig         Integrator handle.
 * @param expr       expr_t expression representing the integrand.
 * @param ndim       Number of integration dimensions (≥ 1).
 * @param vars       Array of ndim variable nodes, vars[0] innermost.
 * @param lo         Lower bounds; lo[i] is the lower bound for vars[i].
 * @param hi         Upper bounds; hi[i] is the upper bound for vars[i].
 * @param result     Receives the integral estimate.
 * @param error_est  If non-NULL, receives the final total error estimate.
 *
 * @return  0  Converged within tolerance.
 * @return  1  Maximum subintervals reached before convergence.
 * @return -1  Null argument, ndim == 0, or internal allocation failure.
 */
int intg_integral_multi(integrator_t *ig, expr_t *expr,
                      size_t ndim, expr_t * const *vars,
                      const number_t *lo, const number_t *hi,
                      number_t *result, number_t *error_est);

#endif /* INTEGRATOR_H */
