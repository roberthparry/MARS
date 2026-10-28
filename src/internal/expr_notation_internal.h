/**
 * @file expr_notation_internal.h
 * @brief Controlled expression-owned semantic notation preparation.
 *
 * Other mathematical modules use this interface to project expressions and
 * numeric values into TeX's internal semantic snapshot. It is not public API.
 */
#ifndef MARS_SHARED_EXPR_NOTATION_INTERNAL_H
#define MARS_SHARED_EXPR_NOTATION_INTERNAL_H
#if !defined(MARS_SHARED_EXPR_NOTATION_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_SHARED_EXPR_NOTATION_INTERNAL_ACCESS."
#endif
#include "expression-t.h"
#define MARS_SHARED_TeX_SEMANTIC_INTERNAL_ACCESS
#include "internal/TeX_semantic_internal.h"

/** @brief Initialise expression's shared immutable constants before legacy rendering. */
void expr_init_singletons(void);

/** @brief Report whether an expression contains a locally qualified distribution. */
bool expr_distribution_has_qualification(const expr_t *expression);

/** @brief Parse complete native mathematical input into owned notation. */
TeX_t *expr_notation_from_text(const string_t *text);

/** @brief Parse scalar input into owned notation without evaluating calculus. */
TeX_t *expr_notation_scalar(const string_t *text, bool formal);

/** @brief Create an owned expression snapshot, released with TeX_context_free. */
TeX_context_t *expr_TeX_context(const expr_t *value);

/**
 * @brief Create a snapshot with available integral-transform results resolved.
 * @param value Borrowed expression; it may be released after this call.
 * @return Owned context released with TeX_context_free, or NULL on failure or for a distribution
 *         requiring the legacy renderer.
 */
TeX_context_t *expr_TeX_result_context(const expr_t *value);

/**
 * @brief Snapshot bindings for the borrowed expression.
 * @param expression Borrowed expression.
 * @param context Receives an owned context, or NULL when there are no bindings.
 * @return True on success, including an empty list; false on failure.
 */
bool expr_notation_bindings_context(const expr_t *expression, TeX_context_t **context);

/**
 * @brief Snapshot a preserved surface binding when it represents the requested expression faithfully.
 * @param expression Borrowed expression; its data is copied into a successful snapshot.
 * @param preserve_root_symbol Keep a root variable symbolic even when it has a binding.
 * @return Owned context released with TeX_context_free, or NULL when unsuitable or on failure.
 */
TeX_context_t *expr_notation_surface_context(const expr_t *expression, bool preserve_root_symbol);

/** @brief Report whether the expression's authored or Cartesian form requires an unwrapped body. */
bool expr_notation_requires_unwrapped_body(const expr_t *expression);

/** @brief Owned ordered additive-term selection used by the legacy wrapping adapter. */
typedef struct _expr_notation_terms_t expr_notation_terms_t;

/** @brief Select additive terms in display order; release the result with expr_notation_terms_free. */
expr_notation_terms_t *expr_notation_terms_new(const expr_t *expression);

/** @brief Return the number of borrowed terms in a selection. */
size_t expr_notation_terms_count(const expr_notation_terms_t *terms);

/**
 * @brief Return one additive term and its sign.
 * @param terms Borrowed selection; its source expression must remain alive.
 * @param index Zero-based term index.
 * @param sign Receives 1 or -1 on success.
 * @return Borrowed term, or NULL for invalid arguments or an out-of-range index.
 */
const expr_t *expr_notation_terms_get(const expr_notation_terms_t *terms, size_t index, int *sign);

/** @brief Release a term selection without releasing its borrowed expression nodes. */
void expr_notation_terms_free(expr_notation_terms_t *terms);

/** @brief Select the scalar and additive operands of a product, borrowing both from the expression. */
bool expr_notation_scaled_sum(const expr_t *expression, const expr_t **factor, const expr_t **sum);

/**
 * @brief Prepare an optional local antiderivative abbreviation for a transform.
 * @param expression Borrowed input expression.
 * @param initial Receives an owned copy of the abbreviated integral, or NULL.
 * @return Owned display expression, or NULL if no abbreviation is applicable or allocation fails.
 * @note Release both returned expressions with expr_free.
 */
expr_t *expr_notation_primitive_display(const expr_t *expression, expr_t **initial);

/**
 * @brief Snapshot the variable and integrand defining a selected antiderivative.
 * @param initial Borrowed integral expression, which may be released after this call.
 * @return Owned context released with TeX_context_free, or NULL for invalid input or failure.
 */
TeX_context_t *expr_notation_primitive_context(const expr_t *initial);

/** @brief Report whether the expression graph contains a formal derivative. */
bool expr_notation_has_formal_derivative(const expr_t *expression);

/** @brief Report whether a formal derivative has one coordinate and contains an integral transform. */
bool expr_notation_is_transform_derivative(const expr_t *expression);

/** @brief Expression-owned description of a unary mathematical operation. */
typedef struct {
    const char *name; /**< Borrowed canonical function name, or NULL for a non-function operation. */
    const char *atomic_argument_name; /**< Borrowed named atomic argument, or NULL. */
    bool signal_function; /**< Whether the operation is a signal or distribution function. */
} expr_notation_unary_t;

/**
 * @brief Describe a unary expression without exposing its operation table or graph layout.
 * @param expression Borrowed expression; borrowed strings remain valid while it remains alive.
 * @param result Receives the operation description on success.
 * @return True for a unary operation and false for invalid arguments or other expression kinds.
 */
bool expr_notation_unary(const expr_t *expression, expr_notation_unary_t *result);

/** @brief Report whether a borrowed name denotes one Greek symbol or its supported alias. */
bool expr_notation_is_greek_symbol(const char *name);

/** @brief Borrowed mathematical names identifying an expression operation. */
typedef struct {
    const char *canonical_name; /**< Function-language operation name, or NULL. */
    const char *expression_name; /**< Native expression-language spelling, or NULL. */
} expr_notation_operation_names_t;

/**
 * @brief Describe an operation using names owned by expression, without presentation syntax.
 * @param expression Borrowed expression; returned names remain valid while it remains alive.
 * @param result Receives borrowed operation names on success.
 * @return True for an expression with an operation; false for invalid arguments.
 */
bool expr_notation_operation_names(const expr_t *expression, expr_notation_operation_names_t *result);

/** @brief Owned native preparation state and shared-expression dictionary. */
typedef struct _notation_build_t notation_build_t;

/** @brief Allocate a native semantic snapshot builder. */
notation_build_t *notation_build_new(void);

/** @brief Set derivative style before adding any expression nodes to this builder. */
void notation_build_derivatives(notation_build_t *build, bool partial);

/** @brief Record that formal derivatives denote dependent variables in a differential equation. */
void notation_build_dependent_variables(notation_build_t *build, bool enabled);

/** @brief Preserve the author's term order in subsequently prepared expressions. */
void notation_build_source_order(notation_build_t *build, bool enabled);

/** Select evaluated-result semantics for subsequently constructed nodes. */
void notation_build_result_mode(notation_build_t *build, bool enabled);

/** @brief Release native preparation state and any untransferred semantic model. */
void notation_build_free(notation_build_t *build);

/** @brief Copy semantic node data into this builder; return a model-owned node. */
const TeX_model_node_t *notation_build_node(notation_build_t *build, TeX_node_view_t view,
                                            const TeX_model_node_t *const *children);

/** @brief Copy plain text into a semantic atom; text is not typesetting source. */
const TeX_model_node_t *notation_build_literal(notation_build_t *build, TeX_node_kind_t kind, const char *text);

/** @brief Render a completed semantic root to an owned string; consume its model on success. */
string_t *notation_build_render(notation_build_t *build, const TeX_model_node_t *root);

/** @brief Snapshot native expression meaning; input remains owned by its caller. */
const TeX_model_node_t *notation_build_expr(notation_build_t *build, const expr_t *expression);

/** @brief Interpret native mathematical text and snapshot it without producing typesetting source. */
const TeX_model_node_t *notation_build_formula(notation_build_t *build, const char *source);

/** @brief Snapshot a numeric value without transferring its ownership. */
const TeX_model_node_t *notation_build_number(notation_build_t *build, const number_t value);

/** @brief Transfer the semantic snapshot to a new context on success; builder must still be freed. */
TeX_context_t *notation_build_context(notation_build_t *build, const TeX_model_node_t *root);

#endif
