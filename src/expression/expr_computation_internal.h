/**
 * @file expr_computation_internal.h
 * @brief Expression-owned checkpoint helpers without exposing the graph layout.
 *
 * Used by expression algorithms including text-series recognition. External
 * consumers establish a scope through computation_run() instead of calling
 * these implementation helpers.
 */
#ifndef MARS_EXPR_COMPUTATION_INTERNAL_H
#define MARS_EXPR_COMPUTATION_INTERNAL_H
#if !defined(MARS_EXPR_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_EXPR_INTERNAL_ACCESS."
#endif

#include <stdbool.h>

/** @brief Reserve a recursive node or iteration under the current native computation scope. */
bool expr_computation_step(const char *stage);

/** @brief Poll scoped stopping before publishing results or cache entries. */
bool expr_computation_active(void);

#endif
