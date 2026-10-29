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

#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"

/**
 * @brief Report whether native authored or Cartesian semantics require a single unwrapped body.
 * @param expression Borrowed source expression to inspect or transform without transferring ownership.
 * @return True when the requested operation succeeds; false when unavailable, invalid or unsuccessful.
 */
bool expr_notation_requires_unwrapped_body(const expr_t *expression);

/**
 * @brief Append an optional native antiderivative definition; consumes body and returns owned notation.
 * @param body Owned notation consumed by this call; release only the returned value with TeX_free.
 * @param initial Borrowed antiderivative expression whose local defining notation is copied.
 * @return Owned notation released with TeX_free, or NULL when input is unsupported or rendering fails.
 */
TeX_t *expr_notation_append_primitive(TeX_t *body, const expr_t *initial);

/**
 * @brief Borrow the scalar and additive operands of a product without changing its mathematics.
 * @param expression Borrowed source expression to inspect or transform without transferring ownership.
 * @param factor Required output receiving a borrowed product operand; keep the source expression alive.
 * @param sum Required output receiving a borrowed product operand; keep the source expression alive.
 * @return True when the requested operation succeeds; false when unavailable, invalid or unsuccessful.
 */
bool expr_notation_scaled_sum(const expr_t *expression, const expr_t **factor, const expr_t **sum);

/**
 * @brief Select evaluated-result semantics for nodes subsequently added to this builder.
 * @param build Borrowed semantic snapshot builder; it owns all nodes added by this call.
 * @param enabled Whether to enable the documented snapshot interpretation for subsequently added nodes.
 */
void notation_build_result_mode(notation_build_t *build, bool enabled);

/**
 * @brief Report whether a borrowed expression graph contains a formal derivative.
 * @param expression Borrowed source expression to inspect or transform without transferring ownership.
 * @return True when the requested operation succeeds; false when unavailable, invalid or unsuccessful.
 */
bool expr_notation_has_formal_derivative(const expr_t *expression);

/**
 * @brief Report whether a formal derivative has one coordinate and contains an integral transform.
 * @param expression Borrowed source expression to inspect or transform without transferring ownership.
 * @return True when the requested operation succeeds; false when unavailable, invalid or unsuccessful.
 */
bool expr_notation_is_transform_derivative(const expr_t *expression);

/**
 * @brief Report whether a borrowed expression contains the given symbol name.
 * @param expression Borrowed source expression to inspect or transform without transferring ownership.
 * @param name Borrowed UTF-8 mathematical name; no text ownership transfers.
 * @return True when the requested operation succeeds; false when unavailable, invalid or unsuccessful.
 */
bool expr_stringout_tree_has_symbol_name(const expr_t *expression, const char *name);

/**
 * @brief Report whether the native operation supports an analytic Cartesian series.
 * @param kind Requested native mathematical family used to recognise the unary operation.
 * @return True when the requested operation succeeds; false when unavailable, invalid or unsuccessful.
 */
bool expr_kind_has_analytic_cartesian_series(expr_op_kind_t kind);

/**
 * @brief Report whether this snapshot represents resolved mathematical results.
 * @param build Borrowed semantic snapshot builder; it owns all nodes added by this call.
 * @return True when the requested operation succeeds; false when unavailable, invalid or unsuccessful.
 */
bool expr_notation_results_enabled(const notation_build_t *build);

/**
 * @brief Project an expression's domain restrictions into model-owned semantic nodes.
 * @param build Borrowed semantic snapshot builder; it owns all nodes added by this call.
 * @param expression Borrowed source expression to inspect or transform without transferring ownership.
 * @return Model-owned semantic node, or NULL on invalid input or allocation failure; do not free it individually.
 */
const TeX_model_node_t *expr_notation_domain(notation_build_t *build, const expr_t *expression);

/**
 * @brief Borrow the uniquely detachable qualified distribution, or NULL if its scope is ambiguous.
 * @param expression Borrowed source expression to inspect or transform without transferring ownership.
 * @return Borrowed expression owned by the source graph, or NULL when absent; do not free the returned pointer.
 */
const expr_t *expr_distribution_detachable(const expr_t *expression);

/**
 * @brief Borrow a distribution detached into this root domain, or NULL for other domains.
 * @param build Borrowed semantic snapshot builder; it owns all nodes added by this call.
 * @param expression Borrowed source expression to inspect or transform without transferring ownership.
 * @return Borrowed expression owned by the source graph, or NULL when absent; do not free the returned pointer.
 */
const expr_t *expr_notation_domain_distribution(const notation_build_t *build, const expr_t *expression);

/**
 * @brief Separate calculus factors from a scalar quotient, or return NULL when not applicable.
 * @param build Borrowed semantic snapshot builder; it owns all nodes added by this call.
 * @param expression Borrowed source expression to inspect or transform without transferring ownership.
 * @return Model-owned semantic node, or NULL on invalid input or allocation failure; do not free it individually.
 */
const TeX_model_node_t *expr_notation_calculus_quotient(notation_build_t *build, const expr_t *expression);

/**
 * @brief Combine a positive rational numerator factor with an outer denominator when applicable.
 * @param build Borrowed semantic snapshot builder; it owns all nodes added by this call.
 * @param expression Borrowed source expression to inspect or transform without transferring ownership.
 * @return Model-owned semantic node, or NULL on invalid input or allocation failure; do not free it individually.
 */
const TeX_model_node_t *expr_notation_flat_fraction(notation_build_t *build, const expr_t *expression);

/**
 * @brief Rewrite a reciprocal exponential containing calculus as a negative exponential.
 * @param build Borrowed semantic snapshot builder; it owns all nodes added by this call.
 * @param expression Borrowed source expression to inspect or transform without transferring ownership.
 * @return Model-owned semantic node, or NULL on invalid input or allocation failure; do not free it individually.
 */
const TeX_model_node_t *expr_notation_inverse_exponential(notation_build_t *build, const expr_t *expression);

/**
 * @brief Order scalar factors before calculus factors without changing their mathematical meaning.
 * @param build Borrowed semantic snapshot builder; it owns all nodes added by this call.
 * @param expression Borrowed source expression to inspect or transform without transferring ownership.
 * @return Model-owned semantic node, or NULL on invalid input or allocation failure; do not free it individually.
 */
const TeX_model_node_t *expr_notation_product(notation_build_t *build, const expr_t *expression);

/**
 * @brief Describe an exact reciprocal of a calculus-free sum as a fraction when suitable.
 * @param build Borrowed semantic snapshot builder; it owns all nodes added by this call.
 * @param expression Borrowed source expression to inspect or transform without transferring ownership.
 * @return Model-owned semantic node, or NULL on invalid input or allocation failure; do not free it individually.
 */
const TeX_model_node_t *expr_notation_reciprocal_sum(notation_build_t *build, const expr_t *expression);

/**
 * @brief Project a complete conjugate sum, or return NULL when it does not apply.
 * @param build Borrowed semantic snapshot builder; it owns all nodes added by this call.
 * @param expression Borrowed source expression to inspect or transform without transferring ownership.
 * @return Model-owned semantic node, or NULL on invalid input or allocation failure; do not free it individually.
 */
const TeX_model_node_t *expr_notation_conjugate_sum(notation_build_t *build, const expr_t *expression);

/**
 * @brief Project a recognised polynomial or series in its native display order.
 * @param build Borrowed semantic snapshot builder; it owns all nodes added by this call.
 * @param expression Borrowed source expression to inspect or transform without transferring ownership.
 * @return Model-owned semantic node, or NULL on invalid input or allocation failure; do not free it individually.
 */
const TeX_model_node_t *expr_notation_ordered_sum(notation_build_t *build, const expr_t *expression);

/**
 * @brief Project an exact binding value without evaluating away its symbolic structure.
 * @param build Borrowed semantic snapshot builder; it owns all nodes added by this call.
 * @param binding Borrowed native binding expression whose exact mathematical structure is copied.
 * @return Model-owned semantic node, or NULL on invalid input or allocation failure; do not free it individually.
 */
const TeX_model_node_t *expr_notation_binding(notation_build_t *build, const expr_binding_expr_t *binding);

/**
 * @brief Project a native symbol name, including trailing numeric subscripts.
 * @param build Borrowed semantic snapshot builder; it owns all nodes added by this call.
 * @param name Borrowed UTF-8 mathematical name; no text ownership transfers.
 * @return Model-owned semantic node, or NULL on invalid input or allocation failure; do not free it individually.
 */
const TeX_model_node_t *expr_notation_name(notation_build_t *build, const char *name);

/**
 * @brief Unpack a special function into independent semantic parameter groups and indices.
 * @param build Borrowed semantic snapshot builder; it owns all nodes added by this call.
 * @param expression Borrowed source expression to inspect or transform without transferring ownership.
 * @return Model-owned semantic node, or NULL on invalid input or allocation failure; do not free it individually.
 */
const TeX_model_node_t *expr_notation_packed_function(notation_build_t *build, const expr_t *expression);

/**
 * @brief Interpret a scalar operation over already prepared semantic operands.
 * @param build Borrowed semantic snapshot builder; it owns all nodes added by this call.
 * @param operation Borrowed native operation descriptor; its semantics remain owned by expression.
 * @param left Borrowed mathematical operand; no input ownership transfers.
 * @param right Borrowed mathematical operand; no input ownership transfers.
 * @return Model-owned semantic node, or NULL on invalid input or allocation failure; do not free it individually.
 */
const TeX_model_node_t *expr_notation_operation(notation_build_t *build, const expr_ops_t *operation,
                                               const TeX_model_node_t *left, const TeX_model_node_t *right);

/**
 * @brief Prepare an optional local antiderivative abbreviation for a transform.
 * @param expression Borrowed input expression.
 * @param initial Receives an owned copy of the abbreviated integral, or NULL.
 * @return Owned display expression, or NULL when inapplicable or on allocation failure.
 * @note Release both returned expressions with expr_free.
 */
expr_t *expr_notation_primitive_display(const expr_t *expression, expr_t **initial);

/**
 * @brief Snapshot a borrowed expression, resolving available integral-transform results.
 * @param value Borrowed expression to copy into the semantic snapshot.
 * @return Independently owned context released with TeX_context_free, or NULL on failure.
 */
TeX_context_t *expr_TeX_result_context(const expr_t *value);

/**
 * @brief Snapshot the bindings belonging to a borrowed expression.
 * @param expression Borrowed expression whose bindings are copied.
 * @param context Receives an owned context, or NULL when there are no bindings.
 * @return True on success, including an empty list; false on failure.
 */
bool expr_notation_bindings_context(const expr_t *expression, TeX_context_t **context);

/**
 * @brief Snapshot faithful authored surface arithmetic without substituting variable bindings.
 * @param expression Borrowed source, which may be freed after a successful call.
 * @param preserve_root_symbol Keep a root variable symbolic even when it has a binding.
 * @return Owned context released with TeX_context_free, or NULL when unsuitable or on failure.
 */
TeX_context_t *expr_notation_surface_context(const expr_t *expression, bool preserve_root_symbol);

/**
 * @brief Snapshot an antiderivative definition; release the independently owned result with TeX_context_free.
 * @param initial Borrowed antiderivative expression whose local defining notation is copied.
 * @return Owned rendering context released with TeX_context_free, or NULL on invalid input or failure.
 */
TeX_context_t *expr_notation_primitive_context(const expr_t *initial);

#endif
