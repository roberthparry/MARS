/**
 * @file expr_distribution_internal.h
 * @brief Controlled native decomposition of distributions for plotting and presentation.
 *
 * Lab plotting receives a regular expression and borrowed impulse coordinates
 * and weights. Infinite sums are explicitly bounded previews, never numerical
 * approximations to a Dirac delta. Unsupported nonlinear distributions fail.
 * Native transform presentation may separate ordinary curves with excluded poles
 * from impulse expressions while retaining the full symbolic distribution.
 */
#ifndef MARS_EXPR_DISTRIBUTION_INTERNAL_H
#define MARS_EXPR_DISTRIBUTION_INTERNAL_H
#if !defined(MARS_SHARED_EXPR_DISTRIBUTION_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_SHARED_EXPR_DISTRIBUTION_INTERNAL_ACCESS."
#endif

#include "expression-t.h"

/** Receive a borrowed real support coordinate and finite complex impulse weight. */
typedef bool (*expr_impulse_callback_t)(number_t coordinate, number_t weight, void *context);

/** @brief Recognise distribution nodes, including unsupported nonlinear combinations, without evaluating them. */
bool expr_is_distribution(const expr_t *expression);

/** @brief Return owned TeX separating a supported ordinary part from impulses, or NULL without changing semantics.
 * Only linear combinations with affine principal-value poles are presented this way; the full expression is retained.
 */
char *expr_distribution_curve_TeX(const expr_t *expression, const expr_t *variable);

/** Return an owned regular part, or NULL for an absent or unsupported distribution; preview flags bounded sums. */
expr_t *expr_distribution_decompose(const expr_t *expression, const expr_t *variable,
                                    expr_impulse_callback_t callback, void *context, bool *preview);
#endif
