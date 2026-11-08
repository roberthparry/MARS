/**
 * @file integrator_internal.h
 * @brief Private adaptive integration state and dispatch helpers.
 *
 * Defines tolerances, interval limits and retained exact-result state, with interfaces to numerical and recognised
 * symbolic paths. Public users configure opaque integrator_t handles through the public API in integrator-t.h.
 *
 * This header is an implementation detail under src/, not an installed public API. Keep its consumers within the
 * documented module boundary and preserve any explicit internal-access guards.
 */

#ifndef INTEGRATOR_INTERNAL_H
#define INTEGRATOR_INTERNAL_H

#if !defined(MARS_INTEGRATOR_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_INTEGRATOR_INTERNAL_ACCESS."
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "expression-t.h"
#include "integrator-t.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

typedef struct _integrator_t {
    number_t abs_tol;
    number_t rel_tol;
    size_t max_intervals;
    size_t last_intervals;
    expr_t *last_exact_result;
    computation_t *computation;
} integrator_t;

/** @brief Reserve shared work or poll cancellation; absent control permits work. */
bool intg_checkpoint(integrator_t *ig, uint64_t work);

/** @brief Publish a stage with unknown total and poll for observer cancellation. */
bool intg_report(integrator_t *ig, const char *stage, uint64_t completed);

void intg_clear_exact_result(integrator_t *ig);
void intg_set_exact_result_owned(integrator_t *ig, expr_t *expr);

/**
 * @brief Recognise a logarithmic Bose moment with positive integer power on the positive half-line.
 * @param ig Borrowed mutable integrator receiving an owned exact expression on a match.
 * @param expression Borrowed integrand; not modified or retained.
 * @param coordinate Borrowed integration coordinate; not modified or retained.
 * @param lower Borrowed lower bound, zero or positive infinity for a reversed integral.
 * @param upper Borrowed upper bound, positive infinity or zero for a reversed integral.
 * @param[out] result Required output receiving an owned number on success; release it with num_destroy.
 * @param[out] error Optional output receiving owned zero on success; release it with num_destroy.
 * @return One on success, zero when unsupported, or minus one on matched evaluation/allocation failure.
 * Outputs are unchanged without success. Supports finite real scale, positive exponential rate and powers 1 to 32.
 * The exact expression belongs to ig until its next calculation or intg_free. Serialise mutable integrator access.
 */
int intg_try_logarithmic_bose(integrator_t *ig, const expr_t *expression, const expr_t *coordinate,
                               number_t lower, number_t upper, number_t *result, number_t *error);

int try_integral_multi_special_affine(integrator_t *ig, expr_t *expr, size_t ndim, expr_t *const *vars,
                                      const number_t *lo, const number_t *hi, number_t *result, number_t *error_est);

int intg_integral_multi_num(integrator_t *ig, expr_t *expr, size_t ndim, expr_t *const *vars, const number_t *lo,
                            const number_t *hi, number_t *result, number_t *error_est);

#endif
