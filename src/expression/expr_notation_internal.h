/**
 * @file expr_notation_internal.h
 * @brief Private semantic preparation helpers for expression notation.
 *
 * These helpers interpret expression-owned encodings and prepare generic nodes.
 * They are consumed only by the expression adapters, not by the TeX renderer.
 * Callers request output through the public expression formatting API.
 */
#ifndef MARS_EXPR_NOTATION_INTERNAL_H
#define MARS_EXPR_NOTATION_INTERNAL_H
#if !defined(MARS_EXPR_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_EXPR_INTERNAL_ACCESS."
#endif

#define MARS_SHARED_EXPR_NOTATION_INTERNAL_ACCESS
#include "internal/expr_notation_internal.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"

/** @brief Report whether this snapshot represents resolved mathematical results. */
bool expr_notation_results_enabled(const notation_build_t *build);

/** @brief Project an expression's domain restrictions into model-owned semantic nodes. */
const TeX_model_node_t *expr_notation_domain(notation_build_t *build, const expr_t *expression);

/** @brief Borrow the uniquely detachable qualified distribution, or NULL if its scope is ambiguous. */
const expr_t *expr_distribution_detachable(const expr_t *expression);

/** @brief Borrow a distribution detached into this root domain, or NULL for other domains. */
const expr_t *expr_notation_domain_distribution(const notation_build_t *build, const expr_t *expression);

/** @brief Separate calculus factors from a scalar quotient, or return NULL when not applicable. */
const TeX_model_node_t *expr_notation_calculus_quotient(notation_build_t *build, const expr_t *expression);

/** @brief Combine a positive rational numerator factor with an outer denominator when applicable. */
const TeX_model_node_t *expr_notation_flat_fraction(notation_build_t *build, const expr_t *expression);

/** @brief Rewrite a reciprocal exponential containing calculus as a negative exponential. */
const TeX_model_node_t *expr_notation_inverse_exponential(notation_build_t *build, const expr_t *expression);

/** @brief Order scalar factors before calculus factors without changing their mathematical meaning. */
const TeX_model_node_t *expr_notation_product(notation_build_t *build, const expr_t *expression);

/** @brief Project a complete conjugate sum, or return NULL when it does not apply. */
const TeX_model_node_t *expr_notation_conjugate_sum(notation_build_t *build, const expr_t *expression);

/** @brief Project a recognised polynomial or series in its native display order. */
const TeX_model_node_t *expr_notation_ordered_sum(notation_build_t *build, const expr_t *expression);

/** @brief Project an exact binding value without evaluating away its symbolic structure. */
const TeX_model_node_t *expr_notation_binding(notation_build_t *build, const expr_binding_expr_t *binding);

/** @brief Project a native symbol name, including trailing numeric subscripts. */
const TeX_model_node_t *expr_notation_name(notation_build_t *build, const char *name);

/** @brief Unpack a special function into independent semantic parameter groups and indices. */
const TeX_model_node_t *expr_notation_packed_function(notation_build_t *build, const expr_t *expression);

/** @brief Interpret a scalar operation over already prepared semantic operands. */
const TeX_model_node_t *expr_notation_operation(notation_build_t *build, const expr_ops_t *operation,
                                               const TeX_model_node_t *left, const TeX_model_node_t *right);

#endif
