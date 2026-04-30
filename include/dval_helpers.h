#ifndef DVAL_HELPERS_H
#define DVAL_HELPERS_H

#include <stdbool.h>
#include <stddef.h>

#include "dval.h"

/**
 * @file dval_helpers.h
 * @brief General public helper utilities for dval expression DAGs.
 *
 * These helpers are shared by sibling modules without exposing dval internals.
 * Heavier structural matchers used by the symbolic integrator live under
 * `include/internal/`.
 */

/**
 * @brief Return true when @p dv is exactly the unnamed constant zero.
 */
bool dv_is_exact_zero(const dval_t *dv);

/**
 * @brief Return true when @p dv is a named constant node.
 */
bool dv_is_named_const(const dval_t *dv);

/**
 * @brief Return a copy of @p expr with every instance of @p needle replaced.
 *
 * The returned expression is a new owning handle. Shared subexpressions are
 * preserved structurally where possible.
 */
dval_t *dv_substitute(const dval_t *expr,
                      const dval_t *needle,
                      dval_t *replacement);

#endif
