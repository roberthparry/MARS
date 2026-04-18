/**
 * @file integrator.h
 * @brief Adaptive Gauss-Kronrod G7K15 numerical integrator at qfloat_t precision.
 *
 * Integrates a user-supplied function over a finite interval [a, b] using
 * an adaptive G7K15 rule.  The 15-point Kronrod estimate is used as the
 * result; the difference |K15 - G7| is the per-subinterval error estimate.
 *
 * At each adaptive step the subinterval with the largest error is bisected.
 * Iteration stops when the total error estimate satisfies:
 *
 *      total_error <= max(abs_tol, rel_tol * |result|)
 *
 * or when `max_intervals` subintervals have been created.
 */

#ifndef INTEGRATOR_H
#define INTEGRATOR_H

#include <stddef.h>
#include "qfloat.h"

/**
 * @brief Integrand callback.
 *
 * @param x   Evaluation point.
 * @param ctx User context pointer (may be NULL).
 * @return    f(x).
 */
typedef qfloat_t (*integrand_fn)(qfloat_t x, void *ctx);

/** Opaque integrator handle. */
typedef struct integrator_t integrator_t;

/**
 * @brief Create an integrator with default tolerances.
 *
 * Default absolute and relative tolerances are both 1e-21.
 * Default maximum subinterval count is 200.
 *
 * @return New integrator, or NULL on allocation failure.
 */
integrator_t *integrator_create(void);

/**
 * @brief Free an integrator.  Safe to call with NULL.
 */
void integrator_destroy(integrator_t *ig);

/**
 * @brief Override convergence tolerances.
 *
 * Convergence is declared when total_error <= max(abs_tol, rel_tol * |result|).
 *
 * @param ig       Integrator handle.
 * @param abs_tol  Absolute tolerance.
 * @param rel_tol  Relative tolerance.
 */
void integrator_set_tol(integrator_t *ig, qfloat_t abs_tol, qfloat_t rel_tol);

/**
 * @brief Override the maximum number of subintervals.
 *
 * @param ig             Integrator handle.
 * @param max_intervals  Upper bound on the subinterval count.
 */
void integrator_set_max_intervals(integrator_t *ig, size_t max_intervals);

/**
 * @brief Integrate f over [a, b].
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
int integrator_eval(integrator_t *ig, integrand_fn f, void *ctx,
                    qfloat_t a, qfloat_t b, qfloat_t *result, qfloat_t *error_est);

/**
 * @brief Number of subintervals used in the most recent call to integrator_eval.
 */
size_t integrator_last_intervals(const integrator_t *ig);

#endif /* INTEGRATOR_H */
