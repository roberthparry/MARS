/**
 * @file qcomplex_internal.h
 * @brief Private paired elementary evaluations for native complex algorithms.
 *
 * Shares sine/cosine and hyperbolic sine/cosine evaluations between complex arithmetic and reflection formulas.
 * Consumers within this module define MARS_QCOMPLEX_INTERNAL_ACCESS immediately before inclusion. Other callers
 * should use qcomplex-t.h; no representation details or configurable multiprecision facilities are exposed here.
 */
#ifndef MARS_QCOMPLEX_INTERNAL_H
#define MARS_QCOMPLEX_INTERNAL_H

#if !defined(MARS_QCOMPLEX_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_QCOMPLEX_INTERNAL_ACCESS."
#endif

#include "qcomplex-t.h"

/**
 * @brief Evaluate complex sine and cosine while sharing their real elementary factors.
 * @param[in] z Complex argument within the native elementary evaluation ranges, passed by value.
 * @param[out] sine Required caller-owned destination, distinct from cosine.
 * @param[out] cosine Required caller-owned destination, distinct from sine.
 * @return No value; scalar outputs follow qc_sin and qc_cos, including existing domain and branch conventions.
 * @note Allocates no storage and retains no pointers. Reentrant with separate caller-owned output storage.
 */
void qci_sin_cos(qcomplex_t z, qcomplex_t *sine, qcomplex_t *cosine);

/**
 * @brief Evaluate complex hyperbolic sine and cosine while sharing their real elementary factors.
 * @param[in] z Complex argument within the native elementary evaluation ranges, passed by value.
 * @param[out] sine Required caller-owned destination, distinct from cosine.
 * @param[out] cosine Required caller-owned destination, distinct from sine.
 * @return No value; scalar outputs follow qc_sinh and qc_cosh, including existing domain and branch conventions.
 * @note Allocates no storage and retains no pointers. Reentrant with separate caller-owned output storage.
 */
void qci_sinh_cosh(qcomplex_t z, qcomplex_t *sine, qcomplex_t *cosine);

#endif
