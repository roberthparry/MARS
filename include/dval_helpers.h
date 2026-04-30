#ifndef DVAL_HELPERS_H
#define DVAL_HELPERS_H

#include <stdbool.h>
#include <stddef.h>

#include "dval.h"

/**
 * @file dval_helpers.h
 * @brief General public helper utilities for dval expression DAGs.
 *
 * These helpers are shared by sibling modules such as matrix code and the
 * integrator without exposing dval internals.
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
 * @brief Detect whether @p expr is exactly one of the nominated variables.
 *
 * On success, writes the matching variable index to @p index_out.
 */
bool dv_match_var_expr(const dval_t *expr,
                       size_t nvars,
                       dval_t *const *vars,
                       size_t *index_out);

/**
 * @brief Detect whether @p expr is an unnamed constant leaf.
 */
bool dv_match_const_value(const dval_t *expr, qfloat_t *value_out);

/**
 * @brief Detect whether @p expr is a scaled subexpression.
 *
 * Recognises negation, multiplication by a constant, and division by a
 * constant. On success returns the overall scalar in @p scale_out and a
 * borrowed pointer to the unscaled base expression in @p base_out.
 */
bool dv_match_scaled_expr(const dval_t *expr,
                          qfloat_t *scale_out,
                          const dval_t **base_out);

/**
 * @brief Detect whether @p expr is a binary sum or difference.
 *
 * On success, returns borrowed pointers to the left and right operands and
 * sets @p is_sub_out to true for subtraction, false for addition.
 */
bool dv_match_add_sub_expr(const dval_t *expr,
                           const dval_t **left_out,
                           const dval_t **right_out,
                           bool *is_sub_out);

/**
 * @brief Detect whether @p expr is a binary product.
 *
 * On success, returns borrowed pointers to the left and right operands.
 */
bool dv_match_mul_expr(const dval_t *expr,
                       const dval_t **left_out,
                       const dval_t **right_out);

/**
 * @brief Collect variable usage of @p vars across @p expr.
 *
 * On success, writes `used_out[i] = true` when `vars[i]` appears anywhere in
 * the expression tree. Returns false if any argument is invalid.
 */
bool dv_collect_var_usage(const dval_t *expr,
                          size_t nvars,
                          dval_t *const *vars,
                          bool *used_out);

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
