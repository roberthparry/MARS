#ifndef DVAL_PATTERN_H
#define DVAL_PATTERN_H

#include <stdbool.h>
#include <stddef.h>
#include "dval.h"

/**
 * @file dval_pattern.h
 * @brief Advanced structural matching helpers for dval expression DAGs.
 *
 * These helpers expose expression-shape queries used by higher-level library
 * components such as the integrator. They are public so sibling modules can
 * use them without depending on dval internals, but they are not intended as
 * part of the common end-user arithmetic workflow.
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
 * @brief Detect whether @p expr is exp(affine(vars)).
 *
 * Recognised affine forms are built from constants and the nominated
 * variables using addition, subtraction, negation, multiplication by
 * constants, and division by constants.
 *
 * On success, writes the constant term to @p constant_out and the variable
 * coefficients to @p coeffs_out and returns true. Variables absent from the
 * affine form receive coefficient zero.
 */
bool dv_match_exp_affine(const dval_t *expr,
                         size_t nvars,
                         dval_t *const *vars,
                         qfloat_t *constant_out,
                         qfloat_t *coeffs_out);

/**
 * @brief Detect whether @p expr is sinh(affine(vars)).
 *
 * Uses the same affine grammar as dv_match_exp_affine().
 */
bool dv_match_sinh_affine(const dval_t *expr,
                          size_t nvars,
                          dval_t *const *vars,
                          qfloat_t *constant_out,
                          qfloat_t *coeffs_out);

/**
 * @brief Detect whether @p expr is cosh(affine(vars)).
 *
 * Uses the same affine grammar as dv_match_exp_affine().
 */
bool dv_match_cosh_affine(const dval_t *expr,
                          size_t nvars,
                          dval_t *const *vars,
                          qfloat_t *constant_out,
                          qfloat_t *coeffs_out);

/**
 * @brief Detect whether @p expr is sin(affine(vars)).
 *
 * Uses the same affine grammar as dv_match_exp_affine().
 */
bool dv_match_sin_affine(const dval_t *expr,
                         size_t nvars,
                         dval_t *const *vars,
                         qfloat_t *constant_out,
                         qfloat_t *coeffs_out);

/**
 * @brief Detect whether @p expr is cos(affine(vars)).
 *
 * Uses the same affine grammar as dv_match_exp_affine().
 */
bool dv_match_cos_affine(const dval_t *expr,
                         size_t nvars,
                         dval_t *const *vars,
                         qfloat_t *constant_out,
                         qfloat_t *coeffs_out);

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
 * @brief Detect whether @p expr is one of the nominated variable nodes.
 *
 * On success writes the matching variable index to @p index_out.
 */
bool dv_match_var_expr(const dval_t *expr,
                       size_t nvars,
                       dval_t *const *vars,
                       size_t *index_out);

/**
 * @brief Mark which nominated variables appear in @p expr.
 *
 * The output array has length @p nvars and is written with true for each
 * variable node that occurs anywhere in the expression DAG.
 */
bool dv_collect_var_usage(const dval_t *expr,
                          size_t nvars,
                          dval_t *const *vars,
                          bool *used_out);

/**
 * @brief Deep-copy @p expr while replacing every occurrence of @p needle
 * with @p replacement.
 *
 * The returned expression is newly allocated and owned by the caller.
 * Returns NULL on allocation or reconstruction failure.
 */
dval_t *dv_substitute(const dval_t *expr,
                      const dval_t *needle,
                      dval_t *replacement);

#endif
