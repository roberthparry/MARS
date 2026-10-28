/**
 * @file TeX-t.h
 * @brief Owned mathematical notation independent of algebra and applications.
 *
 * TeX_t composes immutable TeX fragments, escaping literal text and supplying
 * fraction, root, script, tensor, calculus, equation and matrix syntax. Constructors copy their inputs;
 * children may therefore be released immediately after composition. Objects
 * may be read concurrently, but destruction requires external synchronisation.
 * Structured constructors perform no algebraic rewriting and do not run a TeX engine.
 * TeX_from_expression uses the native mathematical modules for parsing and simplification.
 * The TeX module owns typesetting for expressions, matrices, equations and differential equations;
 * those modules retain their mathematical models and delegate notation to this module.
 * Trusted fragments support composition; never use them for untrusted text.
 */
#ifndef MARS_TEX_H
#define MARS_TEX_H

#include <stdbool.h>
#include <stddef.h>

#include "string-t.h"

/**
 * @brief Opaque, immutable mathematical notation owned by the caller.
 * @see TeX_free
 */
typedef struct _TeX_t TeX_t;

/** @brief Equation presentation without changing either mathematical side. */
typedef enum {
    TeX_EQUATION_INLINE, /**< Separate the sides by an equals sign. */
    TeX_EQUATION_ALIGNED /**< Top-aligned equation, preserving an already wrapped right side. */
} TeX_equation_style_t;

/** @brief Matrix delimiter styles for entries containing mathematical notation. */
typedef enum {
    TeX_MATRIX_BRACKETS,    /**< Square brackets using the bmatrix environment. */
    TeX_MATRIX_PARENTHESES, /**< Parentheses using the pmatrix environment. */
    TeX_MATRIX_PLAIN        /**< No delimiters, using the matrix environment. */
} TeX_matrix_style_t;

/** @brief Vertical position of a tensor index slot. */
typedef enum {
    TeX_INDEX_LOWER, /**< Covariant index slot. */
    TeX_INDEX_UPPER  /**< Contravariant index slot. */
} TeX_index_position_t;

/** @brief One borrowed tensor index; slots retain their supplied left-to-right order. */
typedef struct {
    const TeX_t *value;          /**< Non-NULL index notation, copied during construction. */
    TeX_index_position_t position; /**< Lower or upper placement. */
} TeX_index_t;

/** @brief Automatically sized mathematical delimiters. */
typedef enum {
    TeX_FENCE_PARENTHESES, /**< Round parentheses. */
    TeX_FENCE_BRACKETS,    /**< Square brackets. */
    TeX_FENCE_ABSOLUTE,    /**< Absolute-value bars. */
    TeX_FENCE_FLOOR,       /**< Greatest-integer floor delimiters. */
    TeX_FENCE_CEILING      /**< Least-integer ceiling delimiters. */
} TeX_fence_t;

/** @brief Derivative notation, without any differentiation or evaluation. */
typedef enum {
    TeX_DERIVATIVE_ORDINARY, /**< Upright differential d. */
    TeX_DERIVATIVE_PARTIAL   /**< Partial differential symbol. */
} TeX_derivative_style_t;

/**
 * @brief Copy trusted TeX syntax without escaping or validating commands.
 * @param source Borrowed, null-terminated TeX source. Must not be NULL or contain untrusted text.
 * @return Independently owned notation, released with TeX_free(), or NULL on invalid input or allocation failure.
 * @note The source is copied and is not retained.
 */
TeX_t *TeX_raw(const char *source);

/**
 * @brief Escape literal UTF-8 text and enclose it in a TeX text command.
 * @param text Borrowed text, neither modified nor retained. Empty strings are permitted.
 * @return Independently owned notation, released with TeX_free(), or NULL on invalid input or allocation failure.
 * @note NULL input or an embedded NUL is rejected with errno set to EINVAL.
 */
TeX_t *TeX_text(const string_t *text);

/**
 * @brief Parse ordinary MARS mathematical text and arrange it as readable TeX.
 *
 * Uses the expression module's presentation parser and renders the expression body without a binding wrapper.
 * Accepts native style_EXPRESSION output, including braces, bindings, Unicode notation and domain conditions.
 * Binding declarations are omitted from the notation; ordinary bound variables remain symbolic.
 * If the producer has already evaluated an operation, its authored form cannot be recovered from the result.
 * Ordinary algebra is simplified and beautified; calculus operations are retained instead of evaluated.
 * Uses all function names and aliases registered by the expression parser, with their native TeX renderings.
 * Supports implicit multiplication, integral transforms, sums, products and parenthesised derivative operands.
 * In that grammar log is base ten, ln is natural logarithm, and 1/2(x) means (1/2)*x.
 * @F, @Finv, @L, @Linv, @S, @Z, @P, Dx and aliases such as d/dx retain their operators. This is not a TeX parser.
 * Accepts whole equations and scalar indices such as x_n or x_(n+1). Undeclared calls on equation sides remain
 * symbolic functions; use explicit multiplication to distinguish P*x from P(x). Tensor syntax is not supported.
 * Also accepts a parenthesised matrix literal containing an explicit column comma or row semicolon,
 * optionally followed by a scalar power or multiplied explicitly by a scalar on either side.
 * Entries use the native matrix parser; compound powers must be parenthesised.
 * Matrix powers are displayed, not evaluated; outer scalar bindings are parsed without numerical substitution.
 * Differential-equation problem text uses its native parser and displays initial/boundary conditions without solving.
 * Matrix sums and matrix–matrix products are not accepted here. Captions belong in separate literal text.
 *
 * @param text Borrowed mathematical source; neither modified nor retained. Must not be NULL or contain NUL.
 * @return Independently owned notation, released with TeX_free(), or NULL on parse, formatting or allocation failure.
 * @note Invalid source follows the expression parser's diagnostics, including messages on stderr.
 * @note Simplification follows the expression module's algebra and domain conventions; this is not verbatim typesetting.
 */
TeX_t *TeX_from_expression(const string_t *text);

/**
 * @brief Compose a fraction from two mathematical fragments.
 * @param numerator Borrowed, non-NULL numerator notation.
 * @param denominator Borrowed, non-NULL denominator notation.
 * @return Independently owned notation, released with TeX_free(), or NULL on invalid input or allocation failure.
 * @note Both fragments are copied. No algebraic validation or simplification is performed.
 */
TeX_t *TeX_fraction(const TeX_t *numerator, const TeX_t *denominator);

/**
 * @brief Compose a square or indexed radical.
 * @param radicand Borrowed, non-NULL notation beneath the radical.
 * @param index Borrowed root index, or NULL for a square root.
 * @return Independently owned notation, released with TeX_free(), or NULL on invalid input or allocation failure.
 * @note Supplied fragments are copied and are not retained.
 */
TeX_t *TeX_root(const TeX_t *radicand, const TeX_t *index);

/**
 * @brief Group the complete base and attach optional lower and upper scripts.
 * @param base Borrowed, non-NULL base notation.
 * @param lower Borrowed subscript notation, or NULL to omit the subscript.
 * @param upper Borrowed superscript notation, or NULL to omit the superscript.
 * @return Independently owned notation, released with TeX_free(), or NULL on invalid input or allocation failure.
 * @note Supplied fragments are copied. With both scripts omitted, the base is still grouped.
 */
TeX_t *TeX_scripts(const TeX_t *base, const TeX_t *lower, const TeX_t *upper);

/**
 * @brief Concatenate mathematical fragments in their supplied order without inserting separators.
 * @param parts Borrowed array of non-NULL fragments; may be NULL only when count is zero.
 * @param count Number of fragments. Zero produces empty notation.
 * @return Independently owned notation, released with TeX_free(), or NULL on invalid input or allocation failure.
 * @note Fragments are copied; neither the array nor its entries are retained.
 */
TeX_t *TeX_sequence(const TeX_t *const *parts, size_t count);

/**
 * @brief Compose an equation from two trusted mathematical fragments.
 * @param left Borrowed, non-NULL left side.
 * @param right Borrowed, non-NULL right side.
 * @param style Inline or top-aligned display; no automatic line breaking is performed.
 * @return Independently owned notation; release with TeX_free(). NULL on invalid input or allocation failure.
 * @note Copies both sides. Aligned mode incorporates a native top-aligned right-side wrapper rather than nesting it.
 */
TeX_t *TeX_equation(const TeX_t *left, const TeX_t *right, TeX_equation_style_t style);

/**
 * @brief Enclose notation in automatically sized delimiters.
 * @param body Borrowed, non-NULL mathematical notation.
 * @param fence Delimiter pair selected from TeX_fence_t.
 * @return Independently owned notation, released with TeX_free(), or NULL on invalid input or allocation failure.
 */
TeX_t *TeX_fenced(const TeX_t *body, TeX_fence_t fence);

/**
 * @brief Attach ordered tensor index slots without aligning differently positioned slots on top of each other.
 * @param base Borrowed, non-NULL tensor symbol.
 * @param indices Borrowed array of non-NULL index values and their positions; may be NULL when count is zero.
 * @param count Number of index slots. Zero produces the grouped base alone.
 * @return Independently owned notation, released with TeX_free(), or NULL on invalid input or allocation failure.
 * @note All inputs are copied. This formats indices; it does not contract tensors or check their dimensions.
 */
TeX_t *TeX_tensor(const TeX_t *base, const TeX_index_t *indices, size_t count);

/**
 * @brief Compose summation notation with optional bounds.
 * @param body Borrowed, non-NULL summand notation.
 * @param lower Borrowed lower annotation such as n = 0, or NULL to omit it.
 * @param upper Borrowed upper annotation, or NULL to omit it.
 * @return Independently owned notation, released with TeX_free(), or NULL on invalid input or allocation failure.
 * @note Inputs are copied. No summation is evaluated.
 */
TeX_t *TeX_sum(const TeX_t *body, const TeX_t *lower, const TeX_t *upper);

/**
 * @brief Compose an integral with optional bounds and an upright differential.
 * @param body Borrowed, non-NULL integrand notation.
 * @param variable Borrowed, non-NULL integration variable.
 * @param lower Borrowed lower bound, or NULL to omit it.
 * @param upper Borrowed upper bound, or NULL to omit it.
 * @return Independently owned notation, released with TeX_free(), or NULL on invalid input or allocation failure.
 * @note Inputs are copied. Nest this constructor for iterated integrals; no integration is evaluated.
 */
TeX_t *TeX_integral(const TeX_t *body, const TeX_t *variable, const TeX_t *lower, const TeX_t *upper);

/**
 * @brief Compose an ordinary or partial derivative fraction of positive integer order.
 * @param body Borrowed, non-NULL differentiated expression.
 * @param variable Borrowed, non-NULL differentiation variable.
 * @param order Positive derivative order; zero is invalid.
 * @param style Ordinary or partial notation selected from TeX_derivative_style_t.
 * @return Independently owned notation, released with TeX_free(), or NULL on invalid input or allocation failure.
 * @note Inputs are copied. This emits notation only; coefficients should be composed beside this fraction.
 */
TeX_t *TeX_derivative(const TeX_t *body, const TeX_t *variable, unsigned order, TeX_derivative_style_t style);

/**
 * @brief Compose a matrix from row-major mathematical cells.
 * @param cells Borrowed, non-NULL array of rows times columns entries. NULL entries denote empty cells.
 * @param rows Number of rows; must be non-zero.
 * @param columns Number of columns; must be non-zero, and rows times columns must fit size_t.
 * @param style Delimiter style, selected from TeX_matrix_style_t.
 * @param row_spacing Extra row spacing in points. Zero uses the environment's default spacing.
 * @return Independently owned notation, released with TeX_free(), or NULL on invalid input or allocation failure.
 * @note Cells are copied; neither the array nor its entries are retained.
 */
TeX_t *TeX_matrix(const TeX_t *const *cells, size_t rows, size_t columns,
                  TeX_matrix_style_t style, unsigned row_spacing);

/**
 * @brief Wrap a mathematical body and optional binding list in braces.
 * @param body Borrowed body; must not be NULL.
 * @param bindings Borrowed bindings, or NULL to omit the binding bar.
 * @return Independently owned notation, released with TeX_free(), or NULL on failure.
 */
TeX_t *TeX_bound(const TeX_t *body, const TeX_t *bindings);

/**
 * @brief Place a scalar factor beside a mathematical body with compact spacing.
 * @param factor Borrowed factor; must not be NULL.
 * @param body Borrowed body; must not be NULL.
 * @return Independently owned notation, released with TeX_free(), or NULL on failure.
 */
TeX_t *TeX_scaled(const TeX_t *factor, const TeX_t *body);

/**
 * @brief Compose aligned real and imaginary components on separate rows.
 * @param real Borrowed real component; must not be NULL.
 * @param imaginary Borrowed imaginary coefficient; must not be NULL.
 * @return Independently owned notation, released with TeX_free(), or NULL on failure.
 */
TeX_t *TeX_cartesian_sum(const TeX_t *real, const TeX_t *imaginary);

/**
 * @brief Compose a symmetric pair with an optional common denominator.
 * @param centre Borrowed common centre; must not be NULL.
 * @param offset Borrowed displacement; must not be NULL.
 * @param denominator Borrowed common denominator, or NULL for none.
 * @param group_offset Whether to enclose the displacement in parentheses.
 * @return Independently owned notation, released with TeX_free(), or NULL on failure.
 */
TeX_t *TeX_symmetric_pair(const TeX_t *centre, const TeX_t *offset,
                          const TeX_t *denominator, bool group_offset);

/**
 * @brief Obtain the immutable TeX source without allocating a copy.
 * @param notation Borrowed notation, or NULL.
 * @return Borrowed, null-terminated source valid until notation is destroyed, or NULL for NULL input.
 * @note The returned pointer must not be modified or freed by the caller.
 */
const char *TeX_source(const TeX_t *notation);

/**
 * @brief Copy the TeX source into an independently owned string.
 * @param notation Borrowed notation to render. Must not be NULL.
 * @return A new string, released with string_free(), or NULL for NULL input or allocation failure.
 */
string_t *TeX_render(const TeX_t *notation);

/**
 * @brief Release notation and its owned source storage.
 * @param notation Notation to destroy, or NULL for no action.
 * @note Any pointer previously returned by TeX_source() for this value becomes invalid.
 */
void TeX_free(TeX_t *notation);

#endif
