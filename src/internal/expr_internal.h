/**
 * @file expr_internal.h
 * @brief Controlled cross-module expression interface.
 *
 * Declares expression-owned transformations, numeric sampling and semantic snapshots used by equation, matrix,
 * differential-equation and TeX adapters. Native graphs, operation tables and binding encodings remain private to
 * expression. This is its single controlled cross-module interface; module-only notation helpers remain in
 * expression/expr_notation_internal.h. Define MARS_SHARED_EXPR_INTERNAL_ACCESS immediately before inclusion.
 * Unless a declaration states otherwise, inputs are borrowed: callers retain ownership and keep them alive
 * throughout the call. Release owned expressions with expr_free, MARS strings with string_free, C strings and
 * pointer arrays with free, numeric outputs with num_destroy, and rendering contexts with TeX_context_free.
 * Nodes returned by snapshot builders belong to the builder's model and are never freed individually.
 *
 * This header is an implementation detail under src/, not an installed public API. Keep its consumers within the
 * documented module boundary and preserve any explicit internal-access guards.
 */

#ifndef EXPR_SHARED_INTERNAL_H
#define EXPR_SHARED_INTERNAL_H

#if !defined(MARS_SHARED_EXPR_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_SHARED_EXPR_INTERNAL_ACCESS."
#endif

#include <stdbool.h>
#include <stddef.h>

#include "string-t.h"
#include "expr_context-t.h"
#include "expression-t.h"
#define MARS_SHARED_TeX_INTERNAL_ACCESS
#include "TeX_internal.h"

/**
 * @brief Receive one distribution impulse during native decomposition.
 * @param coordinate Borrowed support coordinate, valid during this callback.
 * @param weight Borrowed finite complex impulse weight, valid during this callback.
 * @param context Borrowed caller-owned plotting or accumulation state.
 * @return True to continue decomposition; false to stop it.
 * @note Clone numeric values with num_clone before retaining them beyond the callback; release clones with num_destroy.
 */
typedef bool (*expr_impulse_callback_t)(number_t coordinate, number_t weight, void *context);

/**
 * @brief Recognise distribution nodes, including unsupported nonlinear combinations, without evaluating them.
 * @param expression Borrowed source expression to inspect or transform without transferring ownership.
 * @return True when the documented structural property holds; false otherwise.
 */
bool expr_is_distribution(const expr_t *expression);

/**
 * @brief Return an owned regular part and emit impulses; preview reports a bounded infinite-sum preview.
 * @param expression Borrowed source expression to inspect or transform without transferring ownership.
 * @param variable Borrowed native variable identifying the coordinate of differentiation, integration, sampling or
 * solving.
 * @param callback Borrowed impulse callback invoked during decomposition; false requests that decomposition stop.
 * @param context Borrowed callback state; its owner keeps it valid throughout the call.
 * @param preview Optional output reporting whether decomposition produced a bounded preview of an infinite impulse
 * sum.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_distribution_decompose(const expr_t *expression, const expr_t *variable,
                                    expr_impulse_callback_t callback, void *context, bool *preview);

/** @brief Mutable expression-formatting buffer, initialised and released by sbuf_init and sbuf_free. */
typedef struct {
    string_t *text; /**< Buffer-owned text. */
} sbuf_t;

/** @brief Private entry owned by an automatic-name table. */
typedef struct _expr_autoname_entry_t autoname_entry_t;

/** @brief Temporary expression names restored by autoname_restore. */
typedef struct {
    autoname_entry_t *entries; /**< Expression-owned records. */
    size_t count;              /**< Number of records. */
    size_t cap;                /**< Allocated record capacity. */
} autoname_table_t;

/** @brief Borrowed expression symbols in an owned array. */
typedef struct {
    expr_t **vars; /**< Owned array of borrowed expression pointers; release the array with free. */
    size_t count;  /**< Number of symbols. */
    size_t cap;    /**< Allocated array capacity. */
} varlist_t;

/**
 * @brief Initialise an empty output buffer.
 * @param buffer Caller-owned buffer to initialise; release existing storage before reinitialisation.
 */
void sbuf_init(sbuf_t *buffer);

/**
 * @brief Release a buffer and its text.
 * @param buffer Caller-owned formatting buffer, initialised before use and released with sbuf_free.
 */
void sbuf_free(sbuf_t *buffer);

/**
 * @brief Append one character to a buffer.
 * @param buffer Caller-owned formatting buffer, initialised before use and released with sbuf_free.
 * @param value Character to append to the caller-owned buffer.
 */
void sbuf_putc(sbuf_t *buffer, char value);

/**
 * @brief Append a NUL-terminated string to a buffer.
 * @param buffer Caller-owned formatting buffer, initialised before use and released with sbuf_free.
 * @param text Borrowed UTF-8 input or already rendered fragment, as specified by the function's purpose.
 */
void sbuf_puts(sbuf_t *buffer, const char *text);

/**
 * @brief Copy a buffer's text into an independently owned string.
 * @param buffer Caller-owned formatting buffer, initialised before use and released with sbuf_free.
 * @return Owned text released with string_free, or NULL when no representation is available or allocation fails.
 */
string_t *sbuf_to_string(const sbuf_t *buffer);

/**
 * @brief Write a symbol name using native function syntax.
 * @param buffer Caller-owned formatting buffer, initialised before use and released with sbuf_free.
 * @param name Borrowed UTF-8 mathematical name; no text ownership transfers.
 */
void emit_name_func(sbuf_t *buffer, const char *name);

/**
 * @brief Render a function-style expression body into an independently allocated C string.
 * @param expression Borrowed source expression to inspect or transform without transferring ownership.
 * @return Owned C string released with free, or NULL when no representation is available or allocation fails.
 */
char *expr_function_display_body(const expr_t *expression);

/**
 * @brief Set thread-local scientific formatting and return its previous state.
 * @param scientific Whether native numeric formatting should use scientific notation in the calling thread.
 * @return The previous thread-local formatting setting, for restoration after the caller finishes.
 */
bool expr_set_number_scientific_local(bool scientific);

/**
 * @brief Set thread-local numeric precision and return its previous value.
 * @param precision Requested thread-local numeric formatting precision; the previous setting is returned for
 * restoration.
 * @return The previous thread-local formatting setting, for restoration after the caller finishes.
 */
int expr_set_number_precision_local(int precision);

/**
 * @brief Initialise a temporary naming table.
 * @param table Caller-owned naming table to initialise before assigning temporary names.
 * autoname_restore.
 */
void autoname_init(autoname_table_t *table);

/**
 * @brief Restore temporarily assigned names and release table storage.
 * @param table Caller-owned temporary-name table, initialised with autoname_init and restored with
 * autoname_restore.
 */
void autoname_restore(autoname_table_t *table);

/**
 * @brief Assign temporary names to unnamed variables in a borrowed expression.
 * @param expression Caller-owned expression whose binding or temporary formatting state may be updated.
 * @param table Caller-owned temporary-name table, initialised with autoname_init and restored with
 * autoname_restore.
 */
void assign_unnamed_vars_dfs(expr_t *expression, autoname_table_t *table);

/**
 * @brief Initialise an empty borrowed-symbol list.
 * @param list Caller-owned symbol-list storage; it owns its pointer array and borrows the collected expressions.
 */
void varlist_init(varlist_t *list);

/**
 * @brief Add variables found in the borrowed expression to the list.
 * @param expression Borrowed source expression to inspect or transform without transferring ownership.
 * @param list Caller-owned symbol-list storage; it owns its pointer array and borrows the collected expressions.
 */
void find_vars_dfs(const expr_t *expression, varlist_t *list);

/**
 * @brief Add named constants found in the borrowed expression to the list.
 * @param expression Borrowed source expression to inspect or transform without transferring ownership.
 * @param list Caller-owned symbol-list storage; it owns its pointer array and borrows the collected expressions.
 */
void find_named_consts_dfs(const expr_t *expression, varlist_t *list);

/**
 * @brief Add explicitly named constants found in the borrowed expression to the list.
 * @param expression Borrowed source expression to inspect or transform without transferring ownership.
 * @param list Caller-owned symbol-list storage; it owns its pointer array and borrows the collected expressions.
 */
void find_explicit_named_consts_dfs(const expr_t *expression, varlist_t *list);

/**
 * @brief Return the borrowed expression name, or the borrowed fallback if unnamed.
 * @param expression Borrowed source expression to inspect or transform without transferring ownership.
 * @param fallback Borrowed fallback spelling returned unchanged when the expression has no name.
 * @return Borrowed text as described above, or NULL when unavailable; do not modify or free it.
 */
const char *expr_name_or_default(const expr_t *expression, const char *fallback);

/** @brief Mathematical bound conventions used by iterated integration. */
typedef enum expr_integration_bound_kind {
    EXPR_INTEGRATION_BOUND_DEFINITE = 0,
    EXPR_INTEGRATION_BOUND_UPPER_ONLY,
    EXPR_INTEGRATION_BOUND_INDEFINITE
} expr_integration_bound_kind_t;

/**
 * @brief Return an owning Cartesian display form without factoring out a shared real scale.
 *
 * @param expr Expression to separate into real and imaginary terms.
 * @return Owning separated expression, or `NULL` when no Cartesian separation is available.
 */
expr_t *expr_separate_cartesian_for_display(const expr_t *expr);

/**
 * @brief Split an expression into owning real and imaginary Cartesian coefficients for display.
 *
 * @param expr Expression to inspect.
 * @param real_out Owning real coefficient on success.
 * @param imaginary_out Owning imaginary coefficient on success.
 * @param has_imaginary_out Whether the imaginary coefficient is structurally present.
 * @return `true` when the expression can be split.
 */
bool expr_cartesian_parts_for_display(const expr_t *expr, expr_t **real_out, expr_t **imaginary_out,
                                      bool *has_imaginary_out);

/**
 * @brief Expand supported unary functions of Cartesian arguments recursively for display.
 *
 * @param expr Expression whose supported unary nodes should be expanded.
 * @return Owning Cartesian expression, or `NULL` when no display expansion applies.
 */
expr_t *expr_complex_unary_cartesian_for_display(const expr_t *expr);

/**
 * @brief Rotate Cartesian expressions by exact imaginary factors recursively for display.
 *
 * @param expr Expression whose Cartesian product should be rotated.
 * @return Owning rotated expression, or `NULL` when the rewrite does not apply.
 */
expr_t *expr_beautify_imaginary_cartesian_product_for_display(const expr_t *expr);

/**
 * @brief Move a named additive constant last inside any outer domain restrictions, without resimplifying.
 *
 * @param expr Expression whose named addend should be reordered.
 * @param name Name of the additive constant to move.
 * @return Owning reordered expression, or `NULL` when the named addend is absent.
 */
expr_t *expr_move_named_addend_last_for_display(const expr_t *expr, const char *name);

/**
 * @brief Match a primitive plus its named integration constant through outer domain restrictions.
 * @param expr Borrowed antiderivative family returned by expr_integrate_family().
 * @param primitive_out Receives the borrowed primitive, or NULL on failure.
 * @param constant_out Receives the borrowed integration constant, or NULL on failure.
 * @return Whether the family has the expected additive structure.
 */
bool expr_match_antiderivative_family(const expr_t *expr, const expr_t **primitive_out, const expr_t **constant_out);

/**
 * @brief Place the imaginary unit last in Cartesian coefficients for display.
 *
 * @param expr Cartesian expression whose imaginary coefficients should be reordered.
 * @return Owning reordered expression, or `NULL` when no reorder is required.
 */
expr_t *expr_move_imaginary_unit_last_for_display(const expr_t *expr);

/**
 * @brief Write a symbolic reciprocal square root in Cartesian surd form for display.
 *
 * @param expr Expression to rewrite.
 * @return Owning Cartesian reciprocal-root expression, or `NULL` when the rewrite does not apply.
 */
expr_t *expr_beautify_symbolic_complex_square_root_reciprocal_for_display(const expr_t *expr);

/**
 * @brief Expand preserved binding expressions into their expression trees for display processing.
 *
 * @param expr Expression whose preserved nodes should be expanded.
 * @return Owning expanded expression, or `NULL` on failure.
 */
expr_t *expr_expand_preserved_for_display(const expr_t *expr);

/** @brief Unary mathematical families recognised in affine-expression matching. */
typedef enum {
    EXPR_PATTERN_UNARY_EXP,
    EXPR_PATTERN_UNARY_LOG,
    EXPR_PATTERN_UNARY_LOG10,
    EXPR_PATTERN_UNARY_SIN,
    EXPR_PATTERN_UNARY_COS,
    EXPR_PATTERN_UNARY_TAN,
    EXPR_PATTERN_UNARY_SEC,
    EXPR_PATTERN_UNARY_COSEC,
    EXPR_PATTERN_UNARY_COT,
    EXPR_PATTERN_UNARY_SINH,
    EXPR_PATTERN_UNARY_COSH,
    EXPR_PATTERN_UNARY_COSECH,
    EXPR_PATTERN_UNARY_TANH,
    EXPR_PATTERN_UNARY_SECH,
    EXPR_PATTERN_UNARY_COTH,
    EXPR_PATTERN_UNARY_ASIN,
    EXPR_PATTERN_UNARY_ACOS,
    EXPR_PATTERN_UNARY_ATAN,
    EXPR_PATTERN_UNARY_ASEC,
    EXPR_PATTERN_UNARY_ACOSEC,
    EXPR_PATTERN_UNARY_ACOT,
    EXPR_PATTERN_UNARY_ASINH,
    EXPR_PATTERN_UNARY_ACOSH,
    EXPR_PATTERN_UNARY_ATANH,
    EXPR_PATTERN_UNARY_ASECH,
    EXPR_PATTERN_UNARY_ACOSECH,
    EXPR_PATTERN_UNARY_ACOTH,
    EXPR_PATTERN_UNARY_ERF,
    EXPR_PATTERN_UNARY_ERFC,
    EXPR_PATTERN_UNARY_NORMAL_PDF,
    EXPR_PATTERN_UNARY_NORMAL_CDF,
    EXPR_PATTERN_UNARY_NORMAL_LOGPDF,
    EXPR_PATTERN_UNARY_EI,
    EXPR_PATTERN_UNARY_E1,
    EXPR_PATTERN_UNARY_LI,
    EXPR_PATTERN_UNARY_COUNT
} expr_pattern_unary_affine_kind_t;

/**
 * @brief Report whether a borrowed expression is structurally an exact numeric zero.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @return True when the documented structural property holds; false otherwise.
 */
bool expr_is_exact_zero(const expr_t *expr);

/**
 * @brief Report whether a borrowed expression is a named constant.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @return True when the documented structural property holds; false otherwise.
 */
bool expr_is_named_const(const expr_t *expr);

/**
 * @brief Resolve a default constant into an initialised, owned numeric output; return nonzero on success.
 * @param name Borrowed UTF-8 mathematical name; no text ownership transfers.
 * @param value_out Initialised numeric output replaced on success; release the resulting value with num_destroy.
 * @return Non-zero when the name resolves to a supported constant; zero otherwise.
 */
int expr_get_default_constant_num_text(const string_t *name, number_t *value_out);

/**
 * @brief Create an owned expression representing exact zero.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_const_zero(void);

/**
 * @brief Create an owned expression representing exact one.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_const_one(void);

/**
 * @brief Create an owned expression representing an exact signed integer.
 * @param value Exact signed integer used by the constant or arithmetic operation.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_const_long(long value);

/**
 * @brief Retain a borrowed expression; release the returned reference with expr_free.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_retain_expr(const expr_t *expr);

/**
 * @brief Parse formal mathematics using the supplied borrowed symbol identities; return an owned expression.
 * @param expr Borrowed native mathematical input text; this call does not modify or retain it.
 * @param names Borrowed ordered array of symbol names, corresponding to the supplied values or native symbols.
 * @param symbols Borrowed nsymbols-element array of native symbols paired with names.
 * @param nsymbols Number of entries in the borrowed names and symbols arrays.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_from_expression_text_formal(const string_t *expr, const string_t *const *names, expr_t *const *symbols,
                                         size_t nsymbols);

/**
 * @brief Parse formal mathematics with borrowed symbols while retaining authored term order; return an owned
 * expression.
 * @param expr Borrowed native mathematical input text; this call does not modify or retain it.
 * @param names Borrowed ordered array of symbol names, corresponding to the supplied values or native symbols.
 * @param symbols Borrowed nsymbols-element array of native symbols paired with names.
 * @param nsymbols Number of entries in the borrowed names and symbols arrays.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_from_expression_text_formal_ordered(const string_t *expr, const string_t *const *names,
                                                 expr_t *const *symbols, size_t nsymbols);

/**
 * @brief Parse differential input with implicit multiplication and symbolic function calls; return an owned expression
 * and bindings.
 * @param text Borrowed UTF-8 input or already rendered fragment, as specified by the function's purpose.
 * @param bindings Optional output receiving owned bindings; release a returned collection with expr_bindings_free.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_from_differential_text_internal(const string_t *text, expr_bindings_t **bindings);

/**
 * @brief Clone borrowed bindings, optionally selecting constants only; release the owned result with
 * expr_bindings_free.
 * @param bindings Borrowed read-only binding collection; no bindings are transferred.
 * @param constants_only Whether to select constant bindings while omitting variable bindings.
 * @return Owned binding collection released with expr_bindings_free, or NULL on failure.
 */
expr_bindings_t *expr_bindings_clone_internal(const expr_bindings_t *bindings, bool constants_only);

/**
 * @brief Promote the uniquely inferred constant within the supplied mutable bindings.
 * @param bindings Borrowed mutable binding collection to update in place.
 * @return True when the requested operation succeeds; false when unavailable, invalid or unsuccessful.
 */
bool expr_bindings_promote_single_constant_internal(expr_bindings_t *bindings);

/**
 * @brief Copy the bindings attached to a borrowed expression; release the owned result with expr_bindings_free.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @return Owned binding collection released with expr_bindings_free, or NULL on failure.
 */
expr_bindings_t *expr_bindings_from_expr_internal(const expr_t *expr);

/**
 * @brief Merge two borrowed binding collections into an independently owned collection.
 * @param bindings Borrowed read-only binding collection; no bindings are transferred.
 * @param additional_bindings Borrowed read-only binding collection; no bindings are transferred.
 * @return Owned binding collection released with expr_bindings_free, or NULL on failure.
 */
expr_bindings_t *expr_bindings_merge_internal(const expr_bindings_t *bindings,
                                              const expr_bindings_t *additional_bindings);

/**
 * @brief Consume an owned expression and return its owned simplification, or NULL on failure.
 * @param expr Owned expression consumed by this call, including failure; do not release the input again.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_simplify_owned(expr_t *expr);

/**
 * @brief Beautify an already simplified, borrowed expression; return an independently owned result.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_beautify_presimplified(const expr_t *expr);

/**
 * @brief Factor common terms after calculus processing; return an owned expression.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_factor_common_post_calculus(const expr_t *expr);

/**
 * @brief Report whether a borrowed graph contains a symbolic power scaled by one half.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @return True when the documented structural property holds; false otherwise.
 */
bool expr_contains_half_scaled_symbolic_power(const expr_t *expr);

/**
 * @brief Consume an owned expression and return its simplified, owned negation.
 * @param expr Owned expression consumed by this call, including failure; do not release the input again.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_negate_owned(expr_t *expr);

/**
 * @brief Multiply a borrowed expression by an exact signed integer; return an owned result.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param value Exact signed integer used by the constant or arithmetic operation.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_mul_long(const expr_t *expr, long value);

/**
 * @brief Divide a borrowed expression by an exact signed integer; return an owned result.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param value Exact signed integer used by the constant or arithmetic operation.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_div_long(const expr_t *expr, long value);

/**
 * @brief Raise a borrowed expression to an exact signed-integer power; return an owned result.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param exponent Signed integer exponent; no ownership transfers.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_pow_long(const expr_t *expr, long exponent);

/**
 * @brief Add an exact signed integer to a borrowed expression; return an owned result.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param value Exact signed integer used by the constant or arithmetic operation.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_add_long(const expr_t *expr, long value);

/**
 * @brief Create an owned indexed symbol from a borrowed name and index expression.
 * @param name Borrowed UTF-8 mathematical name; no text ownership transfers.
 * @param index Borrowed symbol or expression used as the mathematical index.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_new_indexed_symbol(const char *name, const expr_t *index);

/**
 * @brief Create an owned symbolic summation from a borrowed term and index.
 * @param term Borrowed summand or product term whose index interpretation is retained.
 * @param index Borrowed symbol or expression used as the mathematical index.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_new_summation(const expr_t *term, const expr_t *index);

/**
 * @brief Create an owned finite summation from borrowed term, index and upper-bound expressions.
 * @param term Borrowed summand or product term whose index interpretation is retained.
 * @param index Borrowed symbol or expression used as the mathematical index.
 * @param upper Borrowed mathematical integration or summation bound; no ownership transfers.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_new_finite_summation(const expr_t *term, const expr_t *index, const expr_t *upper);

/**
 * @brief Return an owned, simplified sum of two borrowed operands.
 * @param left Borrowed mathematical operand; no input ownership transfers.
 * @param right Borrowed mathematical operand; no input ownership transfers.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_add_simplify_owned(const expr_t *left, const expr_t *right);

/**
 * @brief Return an owned, simplified difference of two borrowed operands.
 * @param left Borrowed mathematical operand; no input ownership transfers.
 * @param right Borrowed mathematical operand; no input ownership transfers.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_sub_simplify_owned(const expr_t *left, const expr_t *right);

/**
 * @brief Return an owned, simplified product of two borrowed operands.
 * @param left Borrowed mathematical operand; no input ownership transfers.
 * @param right Borrowed mathematical operand; no input ownership transfers.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_mul_simplify_owned(const expr_t *left, const expr_t *right);

/**
 * @brief Return an owned, simplified quotient of two borrowed operands.
 * @param left Borrowed mathematical operand; no input ownership transfers.
 * @param right Borrowed mathematical operand; no input ownership transfers.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_div_simplify_owned(const expr_t *left, const expr_t *right);

/**
 * @brief Compare borrowed expressions structurally, respecting symbol identity and equivalent product orderings.
 * @param u Borrowed expression graph to compare structurally, without evaluating or modifying it.
 * @param v Borrowed expression graph to compare structurally, without evaluating or modifying it.
 * @return Non-zero when both expressions have the same native structure; zero otherwise.
 */
int expr_struct_eq(const expr_t *u, const expr_t *v);

/**
 * @brief Borrow available child expressions; return false when the input has no children.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param left_out Required caller-owned output receiving the borrowed matched operand; keep the source expression
 * alive.
 * @param right_out Required caller-owned output receiving the borrowed matched operand; keep the source expression
 * alive.
 * @return True when the requested operation succeeds; false when unavailable, invalid or unsuccessful.
 */
bool expr_child_exprs(const expr_t *expr, const expr_t **left_out, const expr_t **right_out);

/**
 * @brief Check that native numerical evaluation is not blocked by an unresolved transform or undefined function.
 * Resolves supported transforms and inspects their results without assigning variables or sampling. A true result
 * does not guarantee finite values, suitable bounds or convergence; numerical integrals and sums remain eligible.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @return True when the documented structural property holds; false otherwise.
 */
bool expr_has_numeric_form(const expr_t *expr);

/**
 * @brief Report whether a borrowed expression represents an unevaluated formal derivative.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @return True when the documented structural property holds; false otherwise.
 */
bool expr_is_formal_derivative(const expr_t *expr);

/**
 * @brief Create an owned formal derivative from a borrowed dependent expression and ordered coordinate array.
 * @param dependent Borrowed dependent-variable expression whose ordered derivative is constructed.
 * @param wrt_count Number of ordered differentiation variables in wrts.
 * @param wrts Borrowed wrt_count-element array preserving differentiation order, including repeated coordinates.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_new_formal_derivative(const expr_t *dependent, size_t wrt_count, expr_t *const *wrts);

/**
 * @brief Create an owned symbolic function call from a borrowed name and argument.
 * @param name Borrowed UTF-8 mathematical name; no text ownership transfers.
 * @param argument Borrowed mathematical argument, retained or copied by the constructed expression as required.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_new_arbitrary_function(const char *name, const expr_t *argument);

/**
 * @brief Create an owned symbolic function call from a borrowed name and ordered argument array.
 * @param name Borrowed UTF-8 mathematical name; no text ownership transfers.
 * @param argument_count Number of borrowed argument expressions in arguments.
 * @param arguments Borrowed argument_count-element array of mathematical operands; the array remains caller-owned.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_new_arbitrary_function_n(const char *name, size_t argument_count, expr_t *const *arguments);

/**
 * @brief Borrow the dependent expression of a formal derivative, or return NULL for another expression kind.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @return Borrowed expression owned by the source graph, or NULL when absent; do not free the returned pointer.
 */
const expr_t *expr_formal_derivative_dependent(const expr_t *expr);

/**
 * @brief Return the order of a borrowed formal derivative, or zero for another expression kind.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @return Requested zero-based collection size or derivative order; zero when no such data is available.
 */
size_t expr_formal_derivative_order(const expr_t *expr);

/**
 * @brief Borrow one derivative coordinate by zero-based index; return NULL when unavailable.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param index Zero-based position of the requested operand, coordinate or term.
 * @return Borrowed expression owned by the source graph, or NULL when absent; do not free the returned pointer.
 */
const expr_t *expr_formal_derivative_wrt_at(const expr_t *expr, size_t index);

/**
 * @brief Borrow the operand of an explicit negation; return false for another expression kind.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param arg_out Required caller-owned output receiving the borrowed matched operand; keep the source expression
 * alive.
 * @return True when the requested pattern matches; false when it does not match or matching fails.
 */
bool expr_match_neg_expr(const expr_t *expr, const expr_t **arg_out);

/**
 * Return whether an expression is a formal summation node.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @return True when the documented structural property holds; false otherwise.
 */
bool expr_is_summation(const expr_t *expr);

/**
 * Return whether a finite summation exceeds the native direct-evaluation limit.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @return True when the documented structural property holds; false otherwise.
 */
bool expr_finite_summation_exceeds_direct_limit(const expr_t *expr);

/**
 * @brief Return the closed form supplied by the summand function for a recognised finite progression.
 *
 * @param expr Expression to inspect.
 * @return Owning closed-form expression, or `NULL` when no identity applies.
 */
expr_t *expr_finite_progression_closed_form(const expr_t *expr);

/**
 * @brief Return whether a progression reduction depends on the supplied value of its step.
 *
 * @param expr Expression to inspect.
 * @return `true` when the native result cards must retain the bound algebraic specialisation.
 */
bool expr_finite_progression_requires_bound_step(const expr_t *expr);

/**
 * @brief Return whether an expression is a recognised finite inverse-function progression.
 * @param expr Expression to inspect.
 * @return `true` for a recognised finite inverse-function progression.
 */
bool expr_is_finite_inverse_progression(const expr_t *expr);

/**
 * @brief Return an owned Lerch representation of a recognised weighted finite hyperbolic-sine sum.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_finite_weighted_sinh_lerch_form(const expr_t *expr);

/**
 * @brief Recover an owned weighted finite hyperbolic-sine sum from a recognised Lerch representation.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_finite_weighted_sinh_from_lerch_form(const expr_t *expr);

/**
 * @brief Report whether a borrowed expression is a recognised weighted hyperbolic-sine Lerch form.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @return True when the documented structural property holds; false otherwise.
 */
bool expr_is_finite_weighted_sinh_lerch_form(const expr_t *expr);

/**
 * @brief Evaluate a recognised weighted hyperbolic-sine Lerch form into an initialised, owned numeric output.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param value_out Initialised numeric output replaced on success; release the resulting value with num_destroy.
 * @return True when the requested operation succeeds; false when unavailable, invalid or unsuccessful.
 */
bool expr_finite_weighted_sinh_lerch_value(const expr_t *expr, number_t *value_out);

/**
 * @brief Return an owned Lerch representation of a recognised weighted finite hyperbolic-cosine sum.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_finite_weighted_cosh_lerch_form(const expr_t *expr);

/**
 * @brief Recover an owned weighted finite hyperbolic-cosine sum from a recognised Lerch representation.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_finite_weighted_cosh_from_lerch_form(const expr_t *expr);

/**
 * @brief Report whether a borrowed expression is a recognised weighted hyperbolic-cosine Lerch form.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @return True when the documented structural property holds; false otherwise.
 */
bool expr_is_finite_weighted_cosh_lerch_form(const expr_t *expr);

/**
 * @brief Evaluate a recognised weighted hyperbolic-cosine Lerch form into an initialised, owned numeric output.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param value_out Initialised numeric output replaced on success; release the resulting value with num_destroy.
 * @return True when the requested operation succeeds; false when unavailable, invalid or unsuccessful.
 */
bool expr_finite_weighted_cosh_lerch_value(const expr_t *expr, number_t *value_out);

/**
 * @brief Return an owned Lerch representation of a recognised weighted finite sine sum.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_finite_weighted_sin_lerch_form(const expr_t *expr);

/**
 * @brief Evaluate a recognised weighted sine Lerch form into an initialised, owned numeric output.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param value_out Initialised numeric output replaced on success; release the resulting value with num_destroy.
 * @return True when the requested operation succeeds; false when unavailable, invalid or unsuccessful.
 */
bool expr_finite_weighted_sin_lerch_value(const expr_t *expr, number_t *value_out);

/**
 * @brief Return an owned Lerch representation of a recognised weighted finite cosine sum.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_finite_weighted_cos_lerch_form(const expr_t *expr);

/**
 * @brief Evaluate a recognised weighted cosine Lerch form into an initialised, owned numeric output.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param value_out Initialised numeric output replaced on success; release the resulting value with num_destroy.
 * @return True when the requested operation succeeds; false when unavailable, invalid or unsuccessful.
 */
bool expr_finite_weighted_cos_lerch_value(const expr_t *expr, number_t *value_out);

/**
 * @brief Recover an owned finite progression from its recognised q-digamma representation.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_finite_progression_from_qdigamma_form(const expr_t *expr);

/**
 * @brief Evaluate a recognised q-digamma progression into an initialised, owned numeric output.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param value_out Initialised numeric output replaced on success; release the resulting value with num_destroy.
 * @return True when the requested operation succeeds; false when unavailable, invalid or unsuccessful.
 */
bool expr_finite_qdigamma_progression_value(const expr_t *expr, number_t *value_out);

/**
 * @brief Return an owned derivative of a recognised finite arctangent progression with respect to a borrowed variable.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param wrt Borrowed native variable identifying the coordinate of differentiation, integration, sampling or
 * solving.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_finite_atan_progression_derivative_form(const expr_t *expr, const expr_t *wrt);

/**
 * @brief Render a recognised finite progression as a summation followed by its closed form.
 *
 * @param expr Expression to inspect.
 * @return Newly allocated TeX text, or `NULL` when the expression is not a recognised progression.
 */
char *expr_finite_progression_identity_TeX(const expr_t *expr);

/**
 * @brief Borrow the argument of a unary expression; return false for another expression kind.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param arg_out Required caller-owned output receiving the borrowed matched operand; keep the source expression
 * alive.
 * @return True when the requested pattern matches; false when it does not match or matching fails.
 */
bool expr_match_unary_expr(const expr_t *expr, const expr_t **arg_out);

/**
 * @brief Borrow an argument when its unary function matches the supplied borrowed prototype.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param prototype Borrowed expression identifying the unary function family to match.
 * @param arg_out Required caller-owned output receiving the borrowed matched operand; keep the source expression
 * alive.
 * @return True when the requested pattern matches; false when it does not match or matching fails.
 */
bool expr_match_same_reapplicable_unary_function(const expr_t *expr, const expr_t *prototype,
                                                 const expr_t **arg_out);

/**
 * @brief Borrow the argument of an exponential; return false for another expression kind.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param arg_out Required caller-owned output receiving the borrowed matched operand; keep the source expression
 * alive.
 * @return True when the requested pattern matches; false when it does not match or matching fails.
 */
bool expr_match_exp_expr(const expr_t *expr, const expr_t **arg_out);

/**
 * @brief Borrow the argument of a natural logarithm; return false for another expression kind.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param arg_out Required caller-owned output receiving the borrowed matched operand; keep the source expression
 * alive.
 * @return True when the requested pattern matches; false when it does not match or matching fails.
 */
bool expr_match_log_expr(const expr_t *expr, const expr_t **arg_out);

/**
 * @brief Inspect a principal square root without exposing its expression node.
 * @param expr Borrowed expression to inspect.
 * @param arg_out Receives the borrowed radicand on success.
 * @return True for a square-root node; false otherwise.
 */
bool expr_match_sqrt_expr(const expr_t *expr, const expr_t **arg_out);

/**
 * @brief Borrow the argument of a sine; return false for another expression kind.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param arg_out Required caller-owned output receiving the borrowed matched operand; keep the source expression
 * alive.
 * @return True when the requested pattern matches; false when it does not match or matching fails.
 */
bool expr_match_sin_expr(const expr_t *expr, const expr_t **arg_out);

/**
 * @brief Borrow the argument of a cosine; return false for another expression kind.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param arg_out Required caller-owned output receiving the borrowed matched operand; keep the source expression
 * alive.
 * @return True when the requested pattern matches; false when it does not match or matching fails.
 */
bool expr_match_cos_expr(const expr_t *expr, const expr_t **arg_out);

/**
 * @brief Borrow the argument of a tangent; return false for another expression kind.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param arg_out Required caller-owned output receiving the borrowed matched operand; keep the source expression
 * alive.
 * @return True when the requested pattern matches; false when it does not match or matching fails.
 */
bool expr_match_tan_expr(const expr_t *expr, const expr_t **arg_out);

/**
 * @brief Borrow the argument of a cotangent; return false for another expression kind.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param arg_out Required caller-owned output receiving the borrowed matched operand; keep the source expression
 * alive.
 * @return True when the requested pattern matches; false when it does not match or matching fails.
 */
bool expr_match_cot_expr(const expr_t *expr, const expr_t **arg_out);

/**
 * @brief Borrow a power base and copy its exponent into an initialised, owned numeric output.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param base_out Required caller-owned output receiving the borrowed matched operand; keep the source expression
 * alive.
 * @param exponent_out Initialised numeric output replaced on success; release the resulting value with num_destroy.
 * @return True when the requested pattern matches; false when it does not match or matching fails.
 */
bool expr_match_pow_const(const expr_t *expr, const expr_t **base_out, number_t *exponent_out);

/**
 * @brief Borrow the base and exponent of an expression-valued power.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param base_out Required caller-owned output receiving the borrowed matched operand; keep the source expression
 * alive.
 * @param exponent_out Required caller-owned output receiving the borrowed matched operand; keep the source
 * expression alive.
 * @return True when the requested pattern matches; false when it does not match or matching fails.
 */
bool expr_match_pow_expr(const expr_t *expr, const expr_t **base_out, const expr_t **exponent_out);

/**
 * @brief Borrow the integrand and domain encoding of an integral without exposing graph fields.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param integrand_out Required caller-owned output receiving the borrowed matched operand; keep the source
 * expression alive.
 * @param domain_out Required caller-owned output receiving the borrowed matched operand; keep the source expression
 * alive.
 * @return True when the requested pattern matches; false when it does not match or matching fails.
 */
bool expr_match_integral_expr(const expr_t *expr, const expr_t **integrand_out, const expr_t **domain_out);

/**
 * @brief Build an unevaluated integral with an explicitly chosen bound variable.
 *
 * All inputs are borrowed. The caller must choose a dummy that cannot capture free symbols;
 * the returned expression is owned by the caller and is numerically integrated from zero.
 * @param integrand Borrowed expression to integrate or compare with a proposed antiderivative.
 * @param upper Borrowed mathematical integration or summation bound; no ownership transfers.
 * @param dummy Borrowed integration variable chosen by the caller to avoid capturing free symbols.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_integral_with_dummy_internal(const expr_t *integrand, const expr_t *upper, const expr_t *dummy);

/**
 * @brief Create an owned unevaluated integral from borrowed bounds, integrand and a capture-safe dummy variable.
 * @param integrand Borrowed expression to integrate or compare with a proposed antiderivative.
 * @param lower Borrowed mathematical integration or summation bound; no ownership transfers.
 * @param upper Borrowed mathematical integration or summation bound; no ownership transfers.
 * @param dummy Borrowed integration variable chosen by the caller to avoid capturing free symbols.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_integral_with_bounds_internal(const expr_t *integrand, const expr_t *lower, const expr_t *upper,
                                           const expr_t *dummy);

/**
 * @brief Verify an antiderivative symbolically on the real domain.
 *
 * The caller must ensure that variable arguments are real. Algebraic expansion and
 * real logarithmic identities are used when direct simplification does not suffice.
 * All inputs are borrowed; an unproved identity returns false.
 * @param primitive Borrowed proposed antiderivative to verify against the supplied integrand.
 * @param integrand Borrowed expression to integrate or compare with a proposed antiderivative.
 * @param wrt Borrowed native variable identifying the coordinate of differentiation, integration, sampling or
 * solving.
 * @return True when the requested operation succeeds; false when unavailable, invalid or unsuccessful.
 */
bool expr_verify_antiderivative_real_internal(const expr_t *primitive, const expr_t *integrand, const expr_t *wrt);

/**
 * @brief Assign an exact pi-scaled integer-family binding to a mutable expression.
 * @param expr Caller-owned expression whose binding or temporary formatting state may be updated.
 * @param denominator Required caller-owned output receiving the exact signed-long denominator on success.
 * @param n_coeff Exact signed coefficient of the integer-family index in the pi-scaled binding.
 * @param offset Exact signed constant offset in the pi-scaled integer-family binding.
 */
void expr_set_binding_pi_linear_family(expr_t *expr, long denominator, long n_coeff, long offset);

/**
 * @brief Set a scalar binding from authored text, retaining its exact algebra; return false on invalid input.
 * @param expr Caller-owned expression whose binding or temporary formatting state may be updated.
 * @param text Borrowed UTF-8 input or already rendered fragment, as specified by the function's purpose.
 * @return True when the requested operation succeeds; false when unavailable, invalid or unsuccessful.
 */
bool expr_set_binding_value_text(expr_t *expr, const string_t *text);

/**
 * @brief Copy a scalar binding's value and exact metadata without changing the destination's identity.
 * @param destination Caller-owned destination expression whose binding value and exact metadata are updated.
 * @param source Borrowed source expression to inspect or transform without transferring ownership.
 * @return True when the requested operation succeeds; false when unavailable, invalid or unsuccessful.
 */
bool expr_copy_binding_value(expr_t *destination, const expr_t *source);

/**
 * @brief Return an owning Function-style binding value, retaining exact algebra, or NULL on failure.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @return Owned C string released with free, or NULL when no representation is available or allocation fails.
 */
char *expr_binding_to_function_value(const expr_t *expr);

/**
 * @brief Recover an exact complex-root seed into an initialised, owned numeric output and report its order.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param seed_out Initialised numeric output replaced on success; release the resulting value with num_destroy.
 * @param order_out Required caller-owned output receiving the recognised mathematical order.
 * @return True when the requested pattern matches; false when it does not match or matching fails.
 */
bool expr_exact_complex_root_seed(const expr_t *expr, number_t *seed_out, long *order_out);

/**
 * @brief Return an owned copy of an authored reciprocal-power base and report its root order.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param order_out Required caller-owned output receiving the recognised mathematical order.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_explicit_root_base(const expr_t *expr, long *order_out);

/**
 * @brief Copy an unnamed constant into an initialised, owned numeric output; named constants do not match.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param value_out Initialised numeric output replaced on success; release the resulting value with num_destroy.
 * @return True when the requested pattern matches; false when it does not match or matching fails.
 */
bool expr_match_const_value(const expr_t *expr, number_t *value_out);

/**
 * @brief Locate a borrowed variable by identity in the supplied variable array and report its zero-based index.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param nvars Number of independent variables and corresponding coefficient or usage entries.
 * @param vars Borrowed ordered variable array; its length is given by nvars or ndim as documented by the operation.
 * @param index_out Required output receiving the zero-based position of a matched variable in vars.
 * @return True when the requested pattern matches; false when it does not match or matching fails.
 */
bool expr_match_var_expr(const expr_t *expr, size_t nvars, expr_t *const *vars, size_t *index_out);

/**
 * @brief Separate an owned numeric scale into an initialised output and borrow its remaining expression.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param scale_out Initialised numeric output replaced on success; release the resulting value with num_destroy.
 * @param base_out Required caller-owned output receiving the borrowed matched operand; keep the source expression
 * alive.
 * @return True when the requested pattern matches; false when it does not match or matching fails.
 */
bool expr_match_scaled_expr(const expr_t *expr, number_t *scale_out, const expr_t **base_out);

/**
 * @brief Borrow additive operands and report whether the operation is subtraction.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param left_out Required caller-owned output receiving the borrowed matched operand; keep the source expression
 * alive.
 * @param right_out Required caller-owned output receiving the borrowed matched operand; keep the source expression
 * alive.
 * @param is_sub_out Required output reporting whether the matched operation is subtraction rather than addition.
 * @return True when the requested pattern matches; false when it does not match or matching fails.
 */
bool expr_match_add_sub_expr(const expr_t *expr, const expr_t **left_out, const expr_t **right_out, bool *is_sub_out);

/**
 * @brief Borrow the first expression child, or return NULL when absent.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @return Borrowed expression owned by the source graph, or NULL when absent; do not free the returned pointer.
 */
const expr_t *expr_first_child(const expr_t *expr);

/**
 * @brief Borrow the second expression child, or return NULL when absent.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @return Borrowed expression owned by the source graph, or NULL when absent; do not free the returned pointer.
 */
const expr_t *expr_second_child(const expr_t *expr);

/**
 * @brief Report whether borrowed expressions denote the same factor under structural and exact symbolic checks.
 * @param left Borrowed mathematical operand; no input ownership transfers.
 * @param right Borrowed mathematical operand; no input ownership transfers.
 * @return True when the requested operation succeeds; false when unavailable, invalid or unsuccessful.
 */
bool expr_simplify_same_factor(const expr_t *left, const expr_t *right);

/**
 * @brief Borrow both operands of a product; return false for another expression kind.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param left_out Required caller-owned output receiving the borrowed matched operand; keep the source expression
 * alive.
 * @param right_out Required caller-owned output receiving the borrowed matched operand; keep the source expression
 * alive.
 * @return True when the requested pattern matches; false when it does not match or matching fails.
 */
bool expr_match_mul_expr(const expr_t *expr, const expr_t **left_out, const expr_t **right_out);

/**
 * @brief Return an owned simplified quotient after removing a borrowed common factor, or NULL when unproved.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param factor Borrowed candidate common mathematical factor to inspect or divide out.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_simplify_extract_common_factor_quotient(const expr_t *expr, const expr_t *factor);

/**
 * @brief Find an owned common factor across a borrowed expression array, or return NULL when none is found.
 * @param expressions Borrowed count-element array of expression roots; no array or expression ownership transfers.
 * @param count Number of entries in the supplied ordered array or collection.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_simplify_find_common_factor(expr_t *const *expressions, size_t count);

/**
 * @brief Borrow the numerator and denominator of a quotient; return false for another expression kind.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param left_out Required caller-owned output receiving the borrowed matched operand; keep the source expression
 * alive.
 * @param right_out Required caller-owned output receiving the borrowed matched operand; keep the source expression
 * alive.
 * @return True when the requested pattern matches; false when it does not match or matching fails.
 */
bool expr_match_div_expr(const expr_t *expr, const expr_t **left_out, const expr_t **right_out);

/**
 * @brief Mark uses of borrowed variables in the caller-supplied Boolean array without changing their bindings.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param nvars Number of independent variables and corresponding coefficient or usage entries.
 * @param vars Borrowed ordered variable array; its length is given by nvars or ndim as documented by the operation.
 * @param used_out Caller-owned nvars-element usage array populated from the supplied expression's variable
 * references.
 * @return True when the requested operation succeeds; false when unavailable, invalid or unsuccessful.
 */
bool expr_collect_var_usage(const expr_t *expr, size_t nvars, expr_t *const *vars, bool *used_out);

/**
 * @brief Report whether a borrowed expression has unset parameters outside the supplied active-variable array.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param nvars Number of independent variables and corresponding coefficient or usage entries.
 * @param vars Borrowed ordered variable array; its length is given by nvars or ndim as documented by the operation.
 * @return True when the documented structural property holds; false otherwise.
 */
bool expr_has_unbound_parameters(const expr_t *expr, size_t nvars, expr_t *const *vars);

/**
 * @brief Expand products in a borrowed expression; return an independently owned result.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_expand_products_internal(const expr_t *expr);

/**
 * @brief Distribute explicit signs for mathematical display; return an independently owned result.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_distribute_negative_for_display(const expr_t *expr);

/**
 * @brief Return an owned canonical name for a recognised default constant, or NULL when unrecognised.
 * @param name Borrowed UTF-8 mathematical name; no text ownership transfers.
 * @return Owned text released with string_free, or NULL when no representation is available or allocation fails.
 */
string_t *expr_default_constant_canonical_name_text(const string_t *name);

/**
 * @brief Return an owned iterated integral, reporting completed steps and the first owned antiderivative when
 * requested.
 * @param integrand Borrowed expression to integrate or compare with a proposed antiderivative.
 * @param ndim Number of integration dimensions and entries in the corresponding bound and variable arrays.
 * @param vars Borrowed ordered variable array; its length is given by nvars or ndim as documented by the operation.
 * @param kinds Borrowed ndim-element array specifying definite, upper-only or indefinite integration at each step.
 * @param lo Borrowed ndim-element array of lower integration-bound expressions, interpreted according to kinds.
 * @param hi Borrowed ndim-element array of upper integration-bound expressions, interpreted according to kinds.
 * @param max_steps Maximum number of symbolic integration steps to attempt.
 * @param completed_steps_out Optional output receiving the number of integration steps completed.
 * @param first_antiderivative_out Required caller-owned output receiving an owned expression; release a successful
 * result with expr_free.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_integrate_iterated(const expr_t *integrand, size_t ndim, expr_t *const *vars,
                                const expr_integration_bound_kind_t *kinds, expr_t *const *lo, expr_t *const *hi,
                                size_t max_steps, size_t *completed_steps_out, expr_t **first_antiderivative_out);

/**
 * @brief Return an owned partial integration result and report the remaining dimensions, variables and bounds.
 * @param integrand Borrowed expression to integrate or compare with a proposed antiderivative.
 * @param ndim Number of integration dimensions and entries in the corresponding bound and variable arrays.
 * @param vars Borrowed ordered variable array; its length is given by nvars or ndim as documented by the operation.
 * @param kinds Borrowed ndim-element array specifying definite, upper-only or indefinite integration at each step.
 * @param lo Borrowed ndim-element array of lower integration-bound expressions, interpreted according to kinds.
 * @param hi Borrowed ndim-element array of upper integration-bound expressions, interpreted according to kinds.
 * @param completed_steps_out Optional output receiving the number of integration steps completed.
 * @param remaining_ndim_out Optional output receiving the number of dimensions left for numerical integration.
 * @param remaining_vars_out Caller-owned output array receiving the remaining integration-variable references.
 * @param remaining_lo_num_out Caller-owned numeric output array for the remaining bounds; release populated numeric
 * values with num_destroy.
 * @param remaining_hi_num_out Caller-owned numeric output array for the remaining bounds; release populated numeric
 * values with num_destroy.
 * @param lo_num Borrowed ndim-element array of numerical lower bounds used by the fallback integration plan.
 * @param hi_num Borrowed ndim-element array of numerical upper bounds used by the fallback integration plan.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_integrate_iterated_best_effort(const expr_t *integrand, size_t ndim, expr_t *const *vars,
                                            const expr_integration_bound_kind_t *kinds, expr_t *const *lo,
                                            expr_t *const *hi, size_t *completed_steps_out, size_t *remaining_ndim_out,
                                            expr_t **remaining_vars_out, number_t *remaining_lo_num_out,
                                            number_t *remaining_hi_num_out, const number_t *lo_num,
                                            const number_t *hi_num);

/**
 * @brief Recognise a quartic polynomial multiplied by an affine form using initialised numeric output arrays.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param nvars Number of independent variables and corresponding coefficient or usage entries.
 * @param vars Borrowed ordered variable array; its length is given by nvars or ndim as documented by the operation.
 * @param poly_coeffs_out Initialised caller-owned numeric coefficient array; release populated values with
 * num_destroy.
 * @param constant_out Initialised numeric output replaced on success; release the resulting value with num_destroy.
 * @param coeffs_out Initialised caller-owned numeric coefficient array; release populated values with num_destroy.
 * @return True when the requested pattern matches; false when it does not match or matching fails.
 */
bool expr_match_affine_poly_deg4(const expr_t *expr, size_t nvars, expr_t *const *vars, number_t *poly_coeffs_out,
                                 number_t *constant_out, number_t *coeffs_out);

/**
 * @brief Collect coefficients of a polynomial of degree at most four into the caller-supplied numeric array.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param var Borrowed native variable identifying the coordinate of differentiation, integration, sampling or
 * solving.
 * @param coeffs_out Initialised caller-owned numeric coefficient array; release populated values with num_destroy.
 * @return True when the requested operation succeeds; false when unavailable, invalid or unsuccessful.
 */
bool expr_collect_poly_deg4(const expr_t *expr, const expr_t *var, number_t *coeffs_out);

/**
 * @brief Recognise a selected unary function of an affine argument using initialised numeric outputs.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param kind Requested native mathematical family used to recognise the unary operation.
 * @param nvars Number of independent variables and corresponding coefficient or usage entries.
 * @param vars Borrowed ordered variable array; its length is given by nvars or ndim as documented by the operation.
 * @param constant_out Initialised numeric output replaced on success; release the resulting value with num_destroy.
 * @param coeffs_out Initialised caller-owned numeric coefficient array; release populated values with num_destroy.
 * @return True when the requested pattern matches; false when it does not match or matching fails.
 */
bool expr_match_unary_affine_kind(const expr_t *expr, expr_pattern_unary_affine_kind_t kind, size_t nvars,
                                  expr_t *const *vars, number_t *constant_out, number_t *coeffs_out);

/**
 * @brief Test whether an expression is headed by a selected unary function.
 *
 * @param expr Expression to inspect.
 * @param kind Unary function kind to match.
 * @return `true` when the expression has the requested unary head.
 */
bool expr_is_unary_pattern_kind(const expr_t *expr, expr_pattern_unary_affine_kind_t kind);

/**
 * @brief Recognise a quartic polynomial times a selected affine unary function using initialised numeric output arrays.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param unary_kind Requested native mathematical family used to recognise the unary operation.
 * @param nvars Number of independent variables and corresponding coefficient or usage entries.
 * @param vars Borrowed ordered variable array; its length is given by nvars or ndim as documented by the operation.
 * @param poly_coeffs_out Initialised caller-owned numeric coefficient array; release populated values with
 * num_destroy.
 * @param constant_out Initialised numeric output replaced on success; release the resulting value with num_destroy.
 * @param coeffs_out Initialised caller-owned numeric coefficient array; release populated values with num_destroy.
 * @return True when the requested pattern matches; false when it does not match or matching fails.
 */
bool expr_match_affine_poly_deg4_times_unary_affine_kind(const expr_t *expr,
                                                         expr_pattern_unary_affine_kind_t unary_kind, size_t nvars,
                                                         expr_t *const *vars, number_t *poly_coeffs_out,
                                                         number_t *constant_out, number_t *coeffs_out);

/**
 * @brief Return an owned canonical spelling of a borrowed mathematical name.
 * @param name Borrowed UTF-8 mathematical name; no text ownership transfers.
 * @return Owned text released with string_free, or NULL when no representation is available or allocation fails.
 */
string_t *expr_normalise_name_text(const string_t *name);

/**
 * @brief Return an owned canonical Greek spelling for a borrowed alias, or NULL when unrecognised.
 * @param alias Borrowed ASCII or Unicode alias to canonicalise.
 * @return Owned text released with string_free, or NULL when no representation is available or allocation fails.
 */
string_t *expr_normalise_greek_alias_text(const string_t *alias);

/**
 * @brief Return an owned canonical spelling of a borrowed binding name.
 * @param name Borrowed UTF-8 mathematical name; no text ownership transfers.
 * @return Owned text released with string_free, or NULL when no representation is available or allocation fails.
 */
string_t *expr_normalise_binding_name_text(const string_t *name);

/**
 * @brief Report whether a borrowed name denotes a registered default mathematical constant.
 * @param name Borrowed UTF-8 mathematical name; no text ownership transfers.
 * @return Non-zero when the name identifies a registered default constant; zero otherwise.
 */
int expr_is_default_constant_name_text(const string_t *name);

/**
 * @brief Render expression text with bindings, retaining a bound root variable symbolically.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @return Owned text released with string_free, or NULL when no representation is available or allocation fails.
 */
string_t *expr_to_text_symbolic(const expr_t *expr);

/**
 * @brief Return an owned zeta closed form and owned order expression for a convergent infinite power sum.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param order_out Required caller-owned output receiving an owned expression; release a successful result with
 * expr_free.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_infinite_power_sum_closed_form(const expr_t *expr, expr_t **order_out);

/**
 * @brief Return owned display-only conditioned cases, or NULL when no case split is recognised.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @return Owned C string released with free, or NULL when no representation is available or allocation fails.
 */
char *expr_conditioned_cases_to_string(const expr_t *expr);

/**
 * @brief Specialise supplied constant parameters in transform results; retain free variables and domain checks.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_transform_specialise_constants(const expr_t *expr);

/**
 * @brief Render an evaluated integral-transform identity, specialising only supplied constants.
 * @param source Borrowed source expression to inspect or transform without transferring ownership.
 * @param result Borrowed already computed mathematical result or rendered result text, according to its declared
 * type.
 * @return An owned TeX string to release with free(), or NULL for other expressions.
 */
char *expr_transform_identity_TeX(const expr_t *source, const expr_t *result);

/**
 * @brief Render an unresolved transform's authored operations without claiming an evaluated identity.
 * @param source Borrowed source expression to inspect or transform without transferring ownership.
 * @param result Borrowed already computed mathematical result or rendered result text, according to its declared
 * type.
 * @return An owned TeX string to release with free(), or NULL for other expressions.
 */
char *expr_formal_transform_TeX(const expr_t *source, const expr_t *result);

/**
 * @brief Return a borrowed convergence, unsupported-transform or distribution explanation, or NULL.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @return Borrowed text as described above, or NULL when unavailable; do not modify or free it.
 */
const char *expr_transform_value_note(const expr_t *expr);

/** @brief Optional numerical region restricting one Newton iteration attempt. */
typedef struct _expr_newton_region_t {
    number_t real_min; /**< Borrowed inclusive lower bound on the real coordinate. */
    number_t real_max; /**< Borrowed inclusive upper bound on the real coordinate. */
    number_t imaginary_magnitude_max; /**< Borrowed maximum permitted imaginary magnitude. */
} expr_newton_region_t;

/**
 * @brief Run one Newton attempt, rejecting excursions outside an optional borrowed numeric safety region.
 * @param expr Caller-owned expression whose binding or temporary formatting state may be updated.
 * @param variable Borrowed native variable identifying the coordinate of differentiation, integration, sampling or
 * solving.
 * @param target Borrowed numeric target for goal seeking; its storage remains caller-owned.
 * @param options Borrowed goal-seek options controlling convergence and iteration limits.
 * @param region Borrowed optional numerical safety region; NULL leaves the iteration unrestricted by this region.
 * @return Native solver status, preserving the equation or goal-seek API status convention.
 */
int expr_goal_seek_newton_one(expr_t *expr, expr_t *variable, number_t target,
                              const expr_goal_seek_options_t *options, const expr_newton_region_t *region);

/** @brief Owned temporary-name plan retained across related expression renderings. */
typedef struct _expr_function_temporaries_t expr_function_temporaries_t;

/**
 * @brief Build an owned temporary-name plan for borrowed roots; release it with expr_function_temporaries_free.
 * @param roots Borrowed count-element array of related expression roots used to choose shared temporary names.
 * @param count Number of entries in the supplied ordered array or collection.
 * @return Owned plan released with expr_function_temporaries_free, or NULL on allocation failure.
 * @note Keep the borrowed roots alive until the plan is freed: cleanup restores temporary names on their
 * graph nodes. Do not concurrently rename or free those graphs while this plan is active.
 */
expr_function_temporaries_t *expr_function_temporaries_new(const expr_t *const *roots, size_t count);

/**
 * @brief Build an owned mathematical plan with up to 26 collision-free single-letter definition names.
 * @param roots Borrowed count-element array of related expression roots used to choose shared temporary names.
 * @param count Number of entries in the supplied ordered array or collection.
 * @return Owned plan released with expr_function_temporaries_free, or NULL on allocation failure.
 * @note Keep the borrowed roots alive until the plan is freed: cleanup restores temporary names on their
 * graph nodes. Do not concurrently rename or free those graphs while this plan is active.
 */
expr_function_temporaries_t *expr_mathematical_temporaries_new(const expr_t *const *roots, size_t count);

/**
 * @brief Return owned Function-style declarations selected by a borrowed temporary-name plan.
 * @param plan Borrowed temporary-name plan used across related renderings.
 * @return Owned text released with string_free, or NULL when no representation is available or allocation fails.
 */
string_t *expr_function_temporaries_declarations_text(const expr_function_temporaries_t *plan);

/**
 * @brief Return owned Function-style text for a borrowed expression using a shared temporary-name plan.
 * @param plan Borrowed temporary-name plan used across related renderings.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @return Owned text released with string_free, or NULL when no representation is available or allocation fails.
 */
string_t *expr_function_temporaries_expression_text(const expr_function_temporaries_t *plan, const expr_t *expr);

/**
 * @brief Return owned mathematical definitions selected by a borrowed temporary-name plan and output style.
 * @param plan Borrowed temporary-name plan used across related renderings.
 * @param style Presentation style selecting the requested native or TeX representation.
 * @return Owned text released with string_free, or NULL when no representation is available or allocation fails.
 */
string_t *expr_temporaries_math_definitions(const expr_function_temporaries_t *plan, style_t style);

/**
 * @brief Return owned mathematical text using a shared temporary-name plan without changing its algebra.
 * @param plan Borrowed temporary-name plan used across related renderings.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @param style Presentation style selecting the requested native or TeX representation.
 * @return Owned text released with string_free, or NULL when no representation is available or allocation fails.
 */
string_t *expr_temporaries_math_expression(const expr_function_temporaries_t *plan, const expr_t *expr, style_t style);

/**
 * @brief Release an owned temporary-name plan and its retained data; NULL is accepted.
 * @param plan Owned plan to release; NULL is accepted.
 */
void expr_function_temporaries_free(expr_function_temporaries_t *plan);

/**
 * @brief Return owned Function-style text with explicit Cartesian component assignments.
 * @param expr Borrowed source expression to inspect or transform without transferring ownership.
 * @return Owned text released with string_free, or NULL when no representation is available or allocation fails.
 */
string_t *expr_to_text_function_cartesian(const expr_t *expr);

/**
 * @brief Resolve a series parameter through caller-owned native binding state.
 * @param context Borrowed lookup state, valid throughout the expansion call.
 * @param name Borrowed UTF-8 binding name; do not retain its temporary storage.
 * @param value_out Initialised numeric output receiving an owned copy on success; release with num_destroy.
 * @return True when the binding resolves; false when unavailable. No input ownership transfers.
 */
typedef bool (*expr_series_binding_lookup_fn)(void *context, const char *name, number_t *value_out);

/**
 * @brief Expand native series into owned expression text and optional notation, preserving binding lookup and domain
 * flags.
 * @param source Borrowed native mathematical input text; this call does not modify or retain it.
 * @param display_TeX_out Optional output receiving an owned rendering, or NULL when unavailable; release it with
 * string_free.
 * @param lookup_binding Optional borrowed callback resolving native series bindings during this call.
 * @param lookup_context Borrowed state passed unchanged to lookup_binding; the caller retains ownership.
 * @param domain_specialised_out Optional output reporting whether supplied constants specialised the mathematical
 * domain.
 * @param power_series_out Optional output reporting whether a power-series input was recognised.
 * @return Owned text released with string_free, or NULL when no representation is available or allocation fails.
 */
string_t *expr_expand_series_text(string_view_t source, string_t **display_TeX_out,
                                  expr_series_binding_lookup_fn lookup_binding, void *lookup_context,
                                  bool *domain_specialised_out, bool *power_series_out);

/**
 * @brief Parse native input into an owned expression and optional owned bindings and derivation notation.
 * @param s Borrowed native mathematical input text; the caller retains the string.
 * @param bnd_out Optional output receiving owned bindings; release a returned collection with expr_bindings_free.
 * @param derivation_TeX_out Optional output receiving an owned rendering, or NULL when unavailable; release it with
 * string_free.
 * @param domain_specialised_out Optional output reporting whether supplied constants specialised the mathematical
 * domain.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_from_string_with_derivation_TeX_internal(const char *s, expr_bindings_t **bnd_out,
                                                      string_t **derivation_TeX_out,
                                                      bool *domain_specialised_out);

/**
 * @brief Parse authored calculus operations without replacing them with their results.
 * @param source Borrowed native mathematical input text; this call does not modify or retain it.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_from_string_preserving_calculus_internal(const char *source);

/**
 * @brief Return owning source TeX, preferring the recognised sigma notation over a series' closed form.
 * @param source Borrowed native mathematical input text; this call does not modify or retain it.
 * @return Owned text released with string_free, or NULL when no representation is available or allocation fails.
 */
string_t *expr_source_TeX_internal(const char *source);

/**
 * @brief Describe a context-eligible symbol: variable identity or borrowed named-constant name.
 * @param symbol Borrowed native symbol whose identity or binding is copied without transferring ownership.
 * @param identity Required caller-owned output receiving the stable symbol identity used by the expression context.
 * @param name Borrowed name output identifying the matched symbol; the source symbol owns its storage.
 * @return True when the requested operation succeeds; false when unavailable, invalid or unsuccessful.
 */
bool expr_context_symbol_identity(const expr_t *symbol, uint64_t *identity, const char **name);

/**
 * @brief Capture non-scoped symbols into an empty context, preserving the active variable's freedom.
 * @param context Borrowed expression fact context used to inspect or capture symbol assumptions.
 * @param source Borrowed source expression to inspect or transform without transferring ownership.
 * @param wrt Borrowed native variable identifying the coordinate of differentiation, integration, sampling or
 * solving.
 * @return True when the requested operation succeeds; false when unavailable, invalid or unsuccessful.
 */
bool expr_context_capture_symbols(expr_context_t *context, const expr_t *source, const expr_t *wrt);

/**
 * @brief Clone, bind and refine non-scoped symbols; optionally beautify even when no proof changes the algebra.
 * @param context Borrowed expression fact context used to inspect or capture symbol assumptions.
 * @param source Borrowed source expression to inspect or transform without transferring ownership.
 * @param beautify_unchanged Whether to beautify the owned snapshot even when captured facts do not change the
 * expression.
 * @return Owned expression reference released with expr_free, or NULL when the requested form is unavailable or the
 * operation fails.
 */
expr_t *expr_context_snapshot(const expr_context_t *context, const expr_t *source, bool beautify_unchanged);

/**
 * @brief Infer structural context facts without numerical sampling of compound expressions.
 * @param context Borrowed expression fact context used to inspect or capture symbol assumptions.
 * @param source Borrowed source expression to inspect or transform without transferring ownership.
 * @return Bit mask of symbol facts established by the supplied context, without numerical sampling.
 */
unsigned expr_context_proven_facts(const expr_context_t *context, const expr_t *source);

/** @brief Private, bounded real sampling plan; unsupported or unstable evaluations use the ordinary evaluator. */
typedef struct _expr_real_sample_t expr_real_sample_t;

/**
 * @brief Compile up to three real coordinate inputs and snapshot other finite real atoms; NULL means unavailable.
 * @param expression Borrowed source expression to inspect or transform without transferring ownership.
 * @param axes Borrowed count-element array of real coordinate expressions; at most three coordinates are supported.
 * @param count Number of entries in the supplied ordered array or collection.
 * @return Owned sampling plan released with expr_real_sample_free, or NULL when the expression is unsupported or
 * preparation fails.
 */
expr_real_sample_t *expr_real_sample_new(const expr_t *expression, const expr_t *const *axes, size_t count);

/**
 * @brief Evaluate a guarded double approximation; false requests ordinary evaluation, including cancellation checks.
 * @param sample Borrowed compiled real-sampling plan used without modifying its stored snapshot.
 * @param coordinates Borrowed real coordinates, in the same order as the sampling plan's axes.
 * @param value Required caller-owned output receiving the evaluated real approximation.
 * @return True when the requested operation succeeds; false when unavailable, invalid or unsuccessful.
 */
bool expr_real_sample_eval(const expr_real_sample_t *sample, const double *coordinates, double *value);

/**
 * @brief Release a sampling plan; NULL is harmless.
 * @param sample Owned plan to release; NULL is accepted.
 */
void expr_real_sample_free(expr_real_sample_t *sample);

/**
 *  @name Semantic snapshots and native mathematical input
 * @{
 */

/**
 * @brief Parse complete native mathematical input into owned notation released with TeX_free.
 * @param text Borrowed UTF-8 input or already rendered fragment, as specified by the function's purpose.
 * @return Owned notation released with TeX_free, or NULL when input is unsupported or rendering fails.
 */
TeX_t *expr_notation_from_text(const string_t *text);

/**
 * @brief Parse scalar input into owned notation; formal mode preserves unevaluated calculus.
 * @param text Borrowed UTF-8 input or already rendered fragment, as specified by the function's purpose.
 * @param formal Whether to retain formal calculus operations instead of requesting their evaluated results.
 * @return Owned notation released with TeX_free, or NULL when input is unsupported or rendering fails.
 */
TeX_t *expr_notation_scalar(const string_t *text, bool formal);

/**
 * @brief Snapshot a borrowed expression; release the independently owned result with TeX_context_free.
 * @param value Borrowed expression to copy into the semantic snapshot.
 * @return Owned rendering context released with TeX_context_free, or NULL on invalid input or failure.
 */
TeX_context_t *expr_TeX_context(const expr_t *value);

/** @brief Owned expression preparation state and shared-expression dictionary. */
typedef struct _notation_build_t notation_build_t;

/**
 * @brief Copy a native symbol and its exact binding value into a builder-owned assignment node.
 * @param build Borrowed builder owning the returned node.
 * @param symbol Borrowed native symbol; no binding values are evaluated or substituted into the body.
 * @param unset Optional caller-owned output reporting whether this symbol has an unset binding.
 * @return Builder-owned node, or NULL on failure; no ownership transfers from symbol.
 */
const TeX_model_node_t *notation_build_assignment(notation_build_t *build, const expr_t *symbol, bool *unset);

/**
 * @brief Allocate a semantic snapshot builder; release it with notation_build_free.
 * @return Owned builder released with notation_build_free, or NULL on allocation failure.
 */
notation_build_t *notation_build_new(void);

/**
 * @brief Select ordinary or partial differentiation before adding expression nodes.
 * @param build Borrowed semantic snapshot builder; it owns all nodes added by this call.
 * @param partial Whether subsequent derivative snapshots use partial rather than ordinary differential notation.
 */
void notation_build_derivatives(notation_build_t *build, bool partial);

/**
 * @brief Request prime notation for eligible ordinary dependent-variable derivatives in a semantic snapshot.
 * @param build Borrowed snapshot builder; neither modified nodes nor ownership escape this call. NULL is ignored.
 * @param enabled Whether subsequent single-coordinate dependent-variable derivative nodes prefer primes.
 * @note Partial or mixed-coordinate derivatives retain explicit differential notation. Existing snapshots are
 * unchanged; the choice participates in memoisation keys so shared nodes keep their requested presentation.
 */
void expr_notation_prime_derivatives(notation_build_t *build, bool enabled);

/**
 * @brief Select the dependent-variable interpretation used by differential equations.
 * @param build Borrowed semantic snapshot builder; it owns all nodes added by this call.
 * @param enabled Whether to enable the documented snapshot interpretation for subsequently added nodes.
 */
void notation_build_dependent_variables(notation_build_t *build, bool enabled);

/**
 * @brief Preserve authored term order in subsequently prepared expressions.
 * @param build Borrowed semantic snapshot builder; it owns all nodes added by this call.
 * @param enabled Whether to enable the documented snapshot interpretation for subsequently added nodes.
 */
void notation_build_source_order(notation_build_t *build, bool enabled);

/**
 * @brief Release preparation state and any snapshot not transferred to a context; NULL is accepted.
 * @param build Owned snapshot builder to release; NULL is accepted.
 */
void notation_build_free(notation_build_t *build);

/**
 * @brief Copy semantic node data; return a builder-owned node, or NULL on failure.
 * @param build Borrowed semantic snapshot builder; it owns all nodes added by this call.
 * @param view Semantic descriptor copied into builder-owned storage; optional text is copied rather than retained.
 * @param children Borrowed ordered child-reference array; entries must belong to the supplied builder or model.
 * @return Model-owned semantic node, or NULL on invalid input or allocation failure; do not free it individually.
 */
const TeX_model_node_t *notation_build_node(notation_build_t *build, TeX_node_view_t view,
                                            const TeX_model_node_t *const *children);

/**
 * @brief Copy plain mathematical text into a builder-owned semantic atom, never typesetting source.
 * @param build Borrowed semantic snapshot builder; it owns all nodes added by this call.
 * @param kind Mathematical node kind identifying the snapshot atom or operation.
 * @param text Borrowed UTF-8 input or already rendered fragment, as specified by the function's purpose.
 * @return Model-owned semantic node, or NULL on invalid input or allocation failure; do not free it individually.
 */
const TeX_model_node_t *notation_build_literal(notation_build_t *build, TeX_node_kind_t kind, const char *text);

/**
 * @brief Render a completed snapshot into an independently owned TeX string.
 * @param build Borrowed builder which must still be released with notation_build_free.
 * @param root Borrowed node belonging to build.
 * @return Owned string released with string_free, or NULL on failure.
 * @note Rendering first attempts to transfer the model into a context. A successful transfer consumes that
 * model even if rendering later fails; do not use build or root to add or render more nodes afterwards.
 */
string_t *notation_build_render(notation_build_t *build, const TeX_model_node_t *root);

/**
 * @brief Snapshot borrowed native expression meaning; return a builder-owned node.
 * @param build Borrowed semantic snapshot builder; it owns all nodes added by this call.
 * @param expression Borrowed source expression to inspect or transform without transferring ownership.
 * @return Model-owned semantic node, or NULL on invalid input or allocation failure; do not free it individually.
 */
const TeX_model_node_t *notation_build_expr(notation_build_t *build, const expr_t *expression);

/**
 * @brief Interpret native mathematical text and return a builder-owned semantic node.
 * @param build Borrowed semantic snapshot builder; it owns all nodes added by this call.
 * @param source Borrowed native mathematical input text; this call does not modify or retain it.
 * @return Model-owned semantic node, or NULL on invalid input or allocation failure; do not free it individually.
 */
const TeX_model_node_t *notation_build_formula(notation_build_t *build, const char *source);

/**
 * @brief Snapshot a borrowed numeric value; return a builder-owned semantic node.
 * @param build Borrowed semantic snapshot builder; it owns all nodes added by this call.
 * @param value Borrowed numeric input; passing it by value does not transfer its storage.
 * @return Model-owned semantic node, or NULL on invalid input or allocation failure; do not free it individually.
 */
const TeX_model_node_t *notation_build_number(notation_build_t *build, const number_t value);

/**
 * @brief Transfer a completed snapshot into an independently owned rendering context.
 * @param build Borrowed builder; after success its transferred model cannot accept more nodes.
 * @param root Borrowed root belonging to build.
 * @return Owned context released with TeX_context_free, or NULL on failure, retaining the model in build.
 * @note Always release the builder with notation_build_free. Native source objects and builder preparation
 * state may be freed after successful transfer; the resulting context owns its copied semantic data.
 */
TeX_context_t *notation_build_context(notation_build_t *build, const TeX_model_node_t *root);

/** @} */

#endif /* EXPR_SHARED_INTERNAL_H */
