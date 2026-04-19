/**
 * @file integrator.h
 * @brief Adaptive numerical integrators at qfloat_t precision.
 *
 * Two adaptive quadrature rules are provided:
 *
 *   ig_integral()         — G7K15 (Gauss-Kronrod), callback-based, degree 29.
 *                           Works for any integrand; tops out at ~21 digits.
 *
 *   ig_single_integral()  — Turán T15/T4, dval_t expression-based, degree 31.
 *   ig_double_integral()  — 2-D Turán, adapts in the outer variable.
 *   ig_triple_integral()  — 3-D Turán, adapts in the outermost variable.
 *
 * All rules apply adaptive bisection of the subinterval with the largest
 * error estimate.  Iteration stops when:
 *
 *      total_error <= max(abs_tol, rel_tol * |result|)
 *
 * or when the maximum subinterval count is reached.
 */

#ifndef INTEGRATOR_H
#define INTEGRATOR_H

#include <stddef.h>
#include "qfloat.h"
#include "dval.h"

/**
 * @brief Integrand callback for ig_integral().
 *
 * @param x   Evaluation point.
 * @param ctx User context pointer (may be NULL).
 * @return    f(x).
 */
typedef qfloat_t (*integrand_fn)(qfloat_t x, void *ctx);

/** Opaque integrator handle. */
typedef struct _integrator_t integrator_t;

/**
 * @brief Create an integrator with default tolerances.
 *
 * Default absolute and relative tolerances are both 1e-27.
 * Default maximum subinterval count is 500.
 *
 * @return New integrator, or NULL on allocation failure.
 */
integrator_t *ig_new(void);

/**
 * @brief Free an integrator.  Safe to call with NULL.
 */
void ig_free(integrator_t *ig);

/**
 * @brief Override convergence tolerances.
 *
 * Convergence is declared when total_error <= max(abs_tol, rel_tol * |result|).
 *
 * @param ig       Integrator handle.
 * @param abs_tol  Absolute tolerance.
 * @param rel_tol  Relative tolerance.
 */
void ig_set_tolerance(integrator_t *ig, qfloat_t abs_tol, qfloat_t rel_tol);

/**
 * @brief Override the maximum number of subintervals.
 *
 * @param ig             Integrator handle.
 * @param max_intervals  Upper bound on the subinterval count.
 */
void ig_set_interval_count_max(integrator_t *ig, size_t max_intervals);

/**
 * @brief Integrate f over [a, b] using the G7K15 rule (lower precision).
 *
 * Uses adaptive Gauss-Kronrod G7K15 with a callback integrand.
 * Degree-29 polynomial exactness; practical accuracy tops out near 21 digits.
 * For full qfloat_t precision use ig_single_integral() with a dval_t expression.
 *
 * @param ig         Integrator handle.
 * @param f          Integrand callback.
 * @param ctx        User context forwarded to f (may be NULL).
 * @param a          Lower bound.
 * @param b          Upper bound.
 * @param result     Receives the integral estimate.
 * @param error_est  If non-NULL, receives the final total error estimate.
 *
 * @return  0  Converged within tolerance.
 * @return  1  Maximum subintervals reached before convergence.
 * @return -1  Internal allocation failure.
 */
int ig_integral(integrator_t *ig, integrand_fn f, void *ctx,
                qfloat_t a, qfloat_t b, qfloat_t *result, qfloat_t *error_est);

/**
 * @brief Number of subintervals used in the most recent integration call.
 */
size_t ig_get_interval_count_used(const integrator_t *ig);

/**
 * @brief Integrate a dval_t expression over [a, b] using adaptive Turán T15/T4.
 *
 * Applies an adaptive Turán quadrature rule that uses both f(x) and f''(x) at
 * 8 symmetric node positions on each subinterval, achieving degree-31 polynomial
 * exactness (versus degree 29 for G7K15).  The second derivative is computed
 * automatically via the dval_t expression graph — no user-supplied derivative is
 * needed.
 *
 * The nested T4 rule (4 of the 8 positions, degree 15) provides the error
 * estimate; the adaptive bisection strategy is identical to ig_integral().
 *
 * @p x_var must be the variable node in @p expr that represents the integration
 * variable.  Both @p expr and any derivative graph built from it are invalidated
 * and re-evaluated at each quadrature node; the caller's graph is not modified
 * permanently.
 *
 * Example:
 * @code
 *   integrator_t *ig = ig_new();
 *   dval_t *x    = dv_new_var(qf_from_double(0.0));
 *   dval_t *expr = dv_sin(x);
 *   qfloat_t result, err;
 *   ig_single_integral(ig, expr, x, qf_from_double(0.0), QF_PI, &result, &err);
 *   // result ≈ 2.0
 *   dv_free(expr); dv_free(x); ig_free(ig);
 * @endcode
 *
 * @param ig         Integrator handle.
 * @param expr       dval_t expression representing the integrand f(x).
 * @param x_var      Variable node in @p expr representing x.  Must have been
 *                   created with dv_new_var() or dv_new_named_var().
 * @param a          Lower bound.
 * @param b          Upper bound.
 * @param result     Receives the integral estimate.
 * @param error_est  If non-NULL, receives the final total error estimate.
 *
 * @return  0  Converged within tolerance.
 * @return  1  Maximum subintervals reached before convergence.
 * @return -1  Null argument or internal allocation failure.
 */
int ig_single_integral(integrator_t *ig, dval_t *expr, dval_t *x_var,
                       qfloat_t a, qfloat_t b,
                       qfloat_t *result, qfloat_t *error_est);

/**
 * @brief Integrate a dval_t expression over [ax,bx] × [ay,by] using Turán T15/T4.
 *
 * Applies the same adaptive Turán T15/T4 strategy as ig_single_integral(), but
 * over a 2-D rectangular domain.  The outer integral adapts in y; the inner
 * integral is evaluated at fixed precision in x.  Both second derivatives
 * ∂²f/∂x² and ∂²f/∂y² are computed automatically.
 *
 * @param ig         Integrator handle.
 * @param expr       dval_t expression representing f(x, y).
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
int ig_double_integral(integrator_t *ig, dval_t *expr,
                       dval_t *x_var, qfloat_t ax, qfloat_t bx,
                       dval_t *y_var, qfloat_t ay, qfloat_t by,
                       qfloat_t *result, qfloat_t *error_est);

/**
 * @brief Integrate a dval_t expression over [ax,bx] × [ay,by] × [az,bz].
 *
 * Extends ig_double_integral() to 3-D rectangular domains.  The outermost
 * integral adapts in z; the inner 2-D integral is evaluated at fixed precision.
 * All required second derivatives are computed automatically.
 *
 * @param ig         Integrator handle.
 * @param expr       dval_t expression representing f(x, y, z).
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
int ig_triple_integral(integrator_t *ig, dval_t *expr,
                       dval_t *x_var, qfloat_t ax, qfloat_t bx,
                       dval_t *y_var, qfloat_t ay, qfloat_t by,
                       dval_t *z_var, qfloat_t az, qfloat_t bz,
                       qfloat_t *result, qfloat_t *error_est);

/**
 * @brief Integrate a dval_t expression over an N-dimensional rectangular domain.
 *
 * Generalises ig_single_integral() to arbitrary dimension N using the same
 * adaptive Turán T15/T4 strategy.  All 2^N mixed second-derivative expressions
 * are built automatically.  The outermost variable (vars[ndim-1]) is adapted
 * by bisection; all inner variables use fixed bounds.
 *
 * Variable ordering: vars[0] is the innermost integration variable, vars[ndim-1]
 * the outermost.  lo[i] and hi[i] are the bounds for vars[i].
 *
 * Example — ∫₀¹ ∫₀¹ (x+y) dx dy = 1:
 * @code
 *   integrator_t *ig = ig_new();
 *   dval_t *x = dv_new_var(qf_from_double(0.0));
 *   dval_t *y = dv_new_var(qf_from_double(0.0));
 *   dval_t *expr = dv_add(x, y);
 *   dval_t *vars[2] = { x, y };
 *   qfloat_t lo[2] = { qf_from_double(0.0), qf_from_double(0.0) };
 *   qfloat_t hi[2] = { qf_from_double(1.0), qf_from_double(1.0) };
 *   qfloat_t result, err;
 *   ig_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);
 *   // result ≈ 1.0
 *   dv_free(expr); dv_free(y); dv_free(x); ig_free(ig);
 * @endcode
 *
 * @param ig         Integrator handle.
 * @param expr       dval_t expression representing the integrand.
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
int ig_integral_multi(integrator_t *ig, dval_t *expr,
                      size_t ndim, dval_t * const *vars,
                      const qfloat_t *lo, const qfloat_t *hi,
                      qfloat_t *result, qfloat_t *error_est);

#endif /* INTEGRATOR_H */
