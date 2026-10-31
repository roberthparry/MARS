/**
 * @file TeX-t.h
 * @brief Owned mathematical notation independent of algebra and applications.
 *
 * TeX_builder_t constructs notation in reverse Polish order: push operands, then
 * apply fractions, roots, scripts, tensors, calculus, relations, equations or matrices.
 * The builder owns every intermediate fragment; finish transfers its one completed
 * TeX_t value to the caller. Failed operations preserve the stack and record a
 * persistent error, preventing accidental completion after an unchecked failure.
 * Literal text is escaped. Explicit composition performs no algebraic rewriting
 * and never runs a TeX engine. Immutable values may be read concurrently;
 * destruction and all access to a shared builder require external synchronisation.
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

/** @brief Opaque owner of an operand stack and its first uncleared construction error. */
typedef struct _TeX_builder_t TeX_builder_t;

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

/** @brief Integral-transform notation; constructing it does not evaluate the transform. */
typedef enum {
    TeX_TRANSFORM_LAPLACE,         /**< Forward Laplace transformation. */
    TeX_TRANSFORM_INVERSE_LAPLACE, /**< Inverse Laplace transformation. */
    TeX_TRANSFORM_FOURIER,         /**< Forward Fourier transformation. */
    TeX_TRANSFORM_INVERSE_FOURIER  /**< Inverse Fourier transformation. */
} TeX_transform_kind_t;

/** @brief Which bounds follow a sum, product or integral body on the operand stack. */
typedef enum {
    TeX_LIMIT_NONE,      /**< No bounds. */
    TeX_LIMIT_LOWER,     /**< Lower bound only. */
    TeX_LIMIT_UPPER,     /**< Upper bound only. */
    TeX_LIMIT_BOTH       /**< Lower bound followed by upper bound. */
} TeX_limits_t;

/** @brief Explicit mathematical spacing supplied without knowing TeX commands. */
typedef enum {
    TeX_SPACE_THIN, /**< Thin space between mathematical factors. */
    TeX_SPACE_QUAD  /**< Wide space before a caption. */
} TeX_space_t;

/**
 * @brief Create an empty reverse Polish construction stack.
 * @return Owned builder, released with TeX_builder_free(), or NULL with errno set to ENOMEM.
 * @note Pushes copy inputs. Every operation replaces its top operands only after successful construction.
 * Errors persist until cleared: EINVAL for invalid input, ERANGE for stack underflow,
 * EOVERFLOW for excessive dimensions, and ENOMEM for allocation failure.
 */
TeX_builder_t *TeX_builder_new(void);

/**
 * @brief Release a builder and every intermediate value still on its stack.
 * @param builder Owned builder to destroy, or NULL for no action.
 * @note A value already returned by TeX_builder_finish() remains independently owned.
 */
void TeX_builder_free(TeX_builder_t *builder);

/**
 * @brief Return the current number of operands, including after a construction error.
 * @param builder Borrowed builder, or NULL.
 * @return Stack depth, or zero for NULL; ownership is unchanged.
 */
size_t TeX_builder_count(const TeX_builder_t *builder);

/**
 * @brief Inspect the first construction error without changing the stack.
 * @param builder Borrowed builder, or NULL.
 * @return Zero when healthy, the saved errno value after failure, or EINVAL for NULL.
 */
int TeX_builder_error(const TeX_builder_t *builder);

/**
 * @brief Clear a saved error so construction can continue with the preserved operands.
 * @param builder Borrowed builder to update; NULL has no effect.
 * @note This frees no operands and does not change errno.
 */
void TeX_builder_clear_error(TeX_builder_t *builder);

/**
 * @brief Discard all operands and clear the construction error, ready for another formula.
 * @param builder Borrowed builder to reset; NULL has no effect.
 * @note Previously returned finished values remain valid; borrowed stack views become invalid.
 */
void TeX_builder_clear(TeX_builder_t *builder);

/**
 * @brief Inspect a stack operand without transferring ownership.
 * @param builder Borrowed builder; may be inspected after failure.
 * @param level HP48-style stack level: one for the top operand, two for the next, and so on.
 * @return Borrowed notation, valid until that operand is consumed or the builder is cleared or freed.
 * NULL reports EINVAL for a NULL builder or ERANGE for zero or an unavailable level, without saving an error.
 */
const TeX_t *TeX_builder_peek(const TeX_builder_t *builder, size_t level);

/**
 * @brief Transfer the one completed formula out of a healthy builder.
 * @param builder Borrowed builder, left empty on success and reusable immediately.
 * @return Owned notation, released with TeX_free(), or NULL on a saved error or when depth is not exactly one.
 * @note Invalid depth saves EINVAL. Failure leaves every operand intact; the builder always remains caller-owned.
 */
TeX_t *TeX_builder_finish(TeX_builder_t *builder);

/**
 * @brief Push mathematical source written directly in TeX syntax.
 * @param builder Borrowed builder receiving an owned copy.
 * @param source Borrowed NUL-terminated trusted TeX body, authored directly or generated elsewhere.
 * The source is copied verbatim, neither retained nor syntax-checked; NULL is invalid.
 * @return True on success; false saves an error and leaves the stack unchanged.
 * @note No escaping, native mathematical parsing, simplification or evaluation occurs. Supply the mathematical
 * body without dollar signs or display-mode delimiters. Raw source can be combined with ordinary stack operations;
 * its unknown structure is conservatively grouped in arithmetic. Use TeX_push_text() to escape literal prose.
 * @note This function stores source; a TeX engine performs typesetting and diagnoses invalid TeX syntax.
 */
bool TeX_push_raw(TeX_builder_t *builder, const char *source);

/**
 * @brief Push one plain mathematical symbol without accepting operator or TeX command fragments.
 * @param builder Borrowed builder receiving owned notation.
 * @param name Borrowed NUL-terminated name, copied and not retained. ASCII names such as psi, mu, lambda,
 * Lambda, hbar, infinity, partial and nabla select symbols. Other identifier names and Unicode symbols are accepted too.
 * @return True on success; false saves an error and preserves the stack. Empty names and punctuation are invalid.
 * @note All standard Greek names are supported; an initial capital selects uppercase. No special keyboard is needed.
 */
bool TeX_push_symbol(TeX_builder_t *builder, const char *name);

/**
 * @brief Escape and push literal UTF-8 prose.
 * @param builder Borrowed builder receiving the escaped value.
 * @param text Borrowed string, neither modified nor retained; NULL and embedded NUL are invalid.
 * @return True on success; false saves an error and preserves the stack. Empty text is permitted.
 */
bool TeX_push_text(TeX_builder_t *builder, const string_t *text);

/**
 * @brief Parse and push native mathematical text using TeX_from_expression().
 * @param builder Borrowed builder receiving the result.
 * @param text Borrowed mathematical string, neither modified nor retained.
 * @return True on success; false saves an error and preserves the stack. Native parser diagnostics may appear.
 */
bool TeX_push_expression(TeX_builder_t *builder, const string_t *text);

/**
 * @brief Push native mathematical text supplied as a convenient C string.
 * @param builder Borrowed builder receiving the result.
 * @param text Borrowed NUL-terminated native mathematics, copied before parsing; NULL is invalid.
 * @return True on success; false saves an error and preserves the stack.
 * @note This is the C-string equivalent of TeX_push_expression(), with the same simplification conventions.
 */
bool TeX_push_math(TeX_builder_t *builder, const char *text);

/**
 * @brief Push a decimal integer without requiring source construction.
 * @param builder Borrowed builder receiving the notation.
 * @param value Integer to typeset; no evaluation is performed.
 * @return True on success; false saves an error and preserves the stack.
 * @note The literal value is retained for TeX_roll() controls, including after duplication or copying.
 */
bool TeX_push_integer(TeX_builder_t *builder, long value);

/**
 * @brief Push an independently owned copy of existing notation.
 * @param builder Borrowed builder receiving a copy.
 * @param notation Borrowed non-NULL notation, which may belong to this builder and is never retained.
 * @return True on success; false saves an error and preserves the stack.
 */
bool TeX_push_notation(TeX_builder_t *builder, const TeX_t *notation);

/**
 * @brief Push explicit mathematical spacing.
 * @param builder Borrowed builder receiving the spacing fragment.
 * @param space Thin or quad spacing; other values are invalid.
 * @return True on success; false saves an error and preserves the stack.
 */
bool TeX_push_space(TeX_builder_t *builder, TeX_space_t space);

/**
 * @brief Duplicate the top operand, following the HP48 DUP operation.
 * @param builder Borrowed builder receiving an independent copy.
 * @return True on success; false saves an error and preserves the stack, including ERANGE for an empty stack.
 * @note Bottom-to-top stack effect: a becomes a a. The copy retains presentation and literal integer metadata.
 */
bool TeX_dup(TeX_builder_t *builder);

/**
 * @brief Remove and release the top operand, following the HP48 DROP operation.
 * @param[in,out] builder Borrowed builder requiring at least one operand; ownership remains with the caller.
 * @return True on success; false saves ERANGE for an empty stack or reports EINVAL for NULL.
 * An existing construction error is preserved and prevents any change to the stack.
 * @note Bottom-to-top stack effect: a b becomes a. The builder releases the removed notation;
 * callers must not free it, and any borrowed view of it becomes invalid. Lower operands and
 * their borrowed views remain valid. Removing the tail neither allocates nor shifts surviving
 * operands. Access to a shared builder requires external synchronisation.
 */
bool TeX_drop(TeX_builder_t *builder);

/**
 * @brief Exchange the top two operands, following the HP48 SWAP operation.
 * @param[in,out] builder Borrowed builder requiring at least two operands; ownership remains with the caller.
 * @return True on success; false saves ERANGE for insufficient operands or reports EINVAL for NULL.
 * An existing construction error is preserved and prevents any change to the stack.
 * @note Bottom-to-top stack effect: a b becomes b a. This exchanges two pointers in constant
 * time without allocating, copying or freeing notation. Lower operands, borrowed views and
 * literal integer metadata remain valid. Access to a shared builder requires external synchronisation.
 */
bool TeX_swap(TeX_builder_t *builder);

/**
 * @brief Consume a level number and move that operand to the top, following the HP48 ROLL operation.
 * @param builder Borrowed builder with a literal integer level on top, pushed with TeX_push_integer() or copied.
 * @return True on success; false preserves every operand, including the control, and saves EINVAL for a non-literal
 * control or ERANGE for a level outside one through the remaining stack depth. Existing errors remain unchanged.
 * @note Levels count from one after excluding the control. Thus a b c 3 becomes b c a, while a b 2 becomes b a.
 * A level of one only consumes its control. Notation is moved without copying or allocation; only the selected
 * pointers shift. Arithmetic notation is not evaluated to obtain a level, and generated TeX is never parsed.
 * Borrowed operand views remain valid; the consumed control view does not. Shared builders need external locking.
 */
bool TeX_roll(TeX_builder_t *builder);

/**
 * @brief Rotate the top three operands, following the HP48 ROT operation, equivalent to pushing three then ROLL.
 * @param builder Borrowed builder requiring at least three operands.
 * @return True on success; false saves an error, including ERANGE for underflow, without changing the stack.
 * @note Bottom-to-top stack effect: a b c becomes b c a. Lower operands and borrowed views remain unchanged;
 * the operation moves three pointers in constant time without allocating or copying notation.
 */
bool TeX_rot(TeX_builder_t *builder);

/**
 * @brief Replace left then right operand by their displayed sum.
 * @param builder Borrowed builder; the right operand must be on top.
 * @return True on success; false saves an error and preserves both operands. No algebra is evaluated or reordered.
 * @note Operator syntax and visible grouping are supplied by TeX.
 */
bool TeX_add(TeX_builder_t *builder);

/**
 * @brief Replace minuend then subtrahend by their displayed difference.
 * @param builder Borrowed builder; the subtrahend must be on top.
 * @return True on success; false saves an error and preserves both operands.
 * @note A compound subtrahend is grouped, preserving the order of nested subtraction.
 */
bool TeX_sub(TeX_builder_t *builder);

/**
 * @brief Negate the complete top operand without evaluating it.
 * @param builder Borrowed builder requiring one operand.
 * @return True on success; false saves an error and preserves the operand. Compound operands retain grouping.
 */
bool TeX_neg(TeX_builder_t *builder);

/**
 * @brief Replace left then right factor by their product, retaining mathematical grouping.
 * @param builder Borrowed builder; the right factor must be on top.
 * @return True on success; false saves an error and preserves both factors. Exactly two factors are consumed.
 */
bool TeX_mul(TeX_builder_t *builder);

/**
 * @brief Apply a plain named function to its top arguments in push order.
 * @param builder Borrowed builder receiving the call notation.
 * @param name Borrowed NUL-terminated UTF-8 function name, validated like TeX_push_symbol() and not retained.
 * @param count Number of arguments to consume; zero constructs an empty argument list.
 * @return True on success; false saves an error and preserves the arguments. No function is evaluated.
 * @note Standard function spelling is selected internally; callers supply names such as sin, P or psi.
 */
bool TeX_function(TeX_builder_t *builder, const char *name, size_t count);

/**
 * @brief Apply a function already on the stack to its following arguments.
 * @param builder Borrowed builder; push the function first, then arguments in display order.
 * @param count Number of arguments following the function; zero constructs an empty argument list.
 * @return True on success; false saves an error and preserves every operand. No function is evaluated.
 * @note Supports indexed function symbols built with TeX_subscript().
 */
bool TeX_call(TeX_builder_t *builder, size_t count);

/**
 * @brief Replace numerator then denominator by their fraction.
 * @param builder Borrowed builder; the denominator must be on top.
 * @return True on success; false saves an error and preserves both operands.
 */
bool TeX_frac(TeX_builder_t *builder);

/**
 * @brief Replace the top radicand by its square root.
 * @param builder Borrowed builder requiring one operand.
 * @return True on success; false saves an error and preserves the radicand.
 */
bool TeX_sqrt(TeX_builder_t *builder);

/**
 * @brief Replace radicand then index by an indexed root.
 * @param builder Borrowed builder; the root index must be on top.
 * @return True on success; false saves an error and preserves both operands.
 */
bool TeX_root(TeX_builder_t *builder);

/**
 * @brief Replace base then exponent by a grouped base with a superscript.
 * @param builder Borrowed builder; the exponent must be on top.
 * @return True on success; false saves an error and preserves both operands.
 * @note Compound bases retain visible mathematical grouping from their stored construction structure.
 */
bool TeX_pow(TeX_builder_t *builder);

/**
 * @brief Replace the top operand by derivative prime notation.
 * @param builder Borrowed builder requiring one operand, retained unchanged on failure.
 * @param order Positive derivative order; one through three use primes and larger orders use a parenthesised superscript.
 * @return True on success; false records EINVAL for zero order, ERANGE for missing operands, or an allocation error.
 * The builder owns the result; no notation ownership transfers to the caller.
 * @note This constructs notation without differentiation. Compound operands retain visible grouping;
 * use TeX_call() afterwards to show an evaluation point such as y'(0).
 */
bool TeX_prime(TeX_builder_t *builder, unsigned order);

/**
 * @brief Replace base then subscript by indexed notation.
 * @param builder Borrowed builder; the subscript must be on top.
 * @return True on success; false saves an error and preserves both operands.
 */
bool TeX_subscript(TeX_builder_t *builder);

/**
 * @brief Replace base, lower script then upper script by notation bearing both scripts.
 * @param builder Borrowed builder; the upper script must be on top.
 * @return True on success; false saves an error and preserves all three operands.
 */
bool TeX_scripts(TeX_builder_t *builder);

/**
 * @brief Concatenate the top count operands in their original push order.
 * @param builder Borrowed builder receiving the combined fragment.
 * @param count Number to consume; zero pushes empty notation.
 * @return True on success; false saves an error and preserves the stack.
 */
bool TeX_sequence(TeX_builder_t *builder, size_t count);

/**
 * @brief Replace left then right side by an equation.
 * @param builder Borrowed builder; the right side must be on top.
 * @param style Inline or top-aligned equation presentation.
 * @return True on success; false saves an error and preserves both sides.
 */
bool TeX_equation(TeX_builder_t *builder, TeX_equation_style_t style);

/**
 * @brief Replace left then right operand by an equality, or extend an existing relation chain.
 * @param builder Borrowed builder requiring two operands, with the right operand on top.
 * @return True on success; false records an error and preserves both operands. The builder owns the result.
 * @note No equality is tested. Repeated relation operations concatenate a chain in stack order.
 */
bool TeX_eq(TeX_builder_t *builder);

/**
 * @brief Replace left then right operand by a not-equal relation.
 * @param builder Borrowed builder requiring two operands, with the right operand on top.
 * @return True on success; false records an error and preserves both operands. The builder owns the result.
 * @note No comparison is performed; a relation already on either side is retained as part of the chain.
 */
bool TeX_ne(TeX_builder_t *builder);

/**
 * @brief Replace left then right operand by a strictly-less-than relation.
 * @param builder Borrowed builder requiring two operands, with the right operand on top.
 * @return True on success; false records an error and preserves both operands. The builder owns the result.
 * @note No comparison is performed; repeated relation operations preserve the authored chain.
 */
bool TeX_lt(TeX_builder_t *builder);

/**
 * @brief Replace left then right operand by a less-than-or-equal relation.
 * @param builder Borrowed builder requiring two operands, with the right operand on top.
 * @return True on success; false records an error and preserves both operands. The builder owns the result.
 * @note No comparison is performed; repeated relation operations preserve the authored chain.
 */
bool TeX_le(TeX_builder_t *builder);

/**
 * @brief Replace left then right operand by a strictly-greater-than relation.
 * @param builder Borrowed builder requiring two operands, with the right operand on top.
 * @return True on success; false records an error and preserves both operands. The builder owns the result.
 * @note No comparison is performed; repeated relation operations preserve the authored chain.
 */
bool TeX_gt(TeX_builder_t *builder);

/**
 * @brief Replace left then right operand by a greater-than-or-equal relation.
 * @param builder Borrowed builder requiring two operands, with the right operand on top.
 * @return True on success; false records an error and preserves both operands. The builder owns the result.
 * @note No comparison is performed; repeated relation operations preserve the authored chain.
 */
bool TeX_ge(TeX_builder_t *builder);

/**
 * @brief Replace left then right operand by approximate equality, shown with an approximately-equal sign.
 * @param builder Borrowed builder requiring two operands, with the right operand on top.
 * @return True on success; false records an error and preserves both operands. The builder owns the result.
 * @note This constructs notation without comparing values or selecting an absolute or relative tolerance.
 */
bool TeX_approx(TeX_builder_t *builder);

/**
 * @brief Replace left then right operand by congruence, shown with a tilde above an equals sign.
 * @param builder Borrowed builder requiring two operands, with the right operand on top.
 * @return True on success; false records an error and preserves both operands. The builder owns the result.
 * @note This constructs notation without testing geometric congruence or algebraic equivalence.
 */
bool TeX_cong(TeX_builder_t *builder);

/**
 * @brief Enclose the top operand in automatically sized delimiters.
 * @param builder Borrowed builder requiring one operand.
 * @param fence Delimiter pair selected from TeX_fence_t.
 * @return True on success; false saves an error and preserves the operand.
 */
bool TeX_fence(TeX_builder_t *builder, TeX_fence_t fence);

/**
 * @brief Replace a base and ordered indices by arbitrary-rank indexed notation in any coordinate basis.
 * @param builder Borrowed builder; push the base first, then index values in left-to-right order.
 * @param positions Borrowed array of count lower/upper positions; may be NULL only when count is zero.
 * @param count Number of indices to consume after the base; zero groups the base alone.
 * @return True on success; false saves an error and preserves every operand. The positions are not retained.
 * @note There is no dimension, metric signature or Cartesian-coordinate restriction. All-lower, all-upper and
 * mixed slots retain their authored order. Repeated labels are displayed without performing contractions.
 * The same placement operation can format indexed operators or connection coefficients, including Christoffel
 * symbols; it does not assert that those objects transform as tensors or calculate a covariant derivative.
 */
bool TeX_tensor(TeX_builder_t *builder, const TeX_index_position_t *positions, size_t count);

/**
 * @brief Replace summand then selected bounds by summation notation.
 * @param builder Borrowed builder; push the body first, then lower and/or upper bounds as selected.
 * @param limits Which bounds are present; when both are present the upper bound is on top.
 * @return True on success; false saves an error and preserves all operands. No sum is evaluated.
 */
bool TeX_sum(TeX_builder_t *builder, TeX_limits_t limits);

/**
 * @brief Replace a product body then selected bounds by indexed-product notation.
 * @param builder Borrowed builder; push the body first, then lower and/or upper annotations as selected.
 * @param limits Which annotations are present; with both present the upper annotation must be on top.
 * @return True on success; false saves an error and preserves all operands. The builder retains ownership.
 * @note This constructs a large product sign with optional bounds. Each lower annotation may be an equation
 * constructed with TeX_equation(). No multiplication or evaluation is performed.
 */
bool TeX_prod(TeX_builder_t *builder, TeX_limits_t limits);

/**
 * @brief Replace a body, source variable and target variable by integral-transform notation.
 * @param builder Borrowed builder requiring those three operands in push order, with the target on top.
 * @param kind Forward or inverse Fourier or Laplace notation selected from TeX_transform_kind_t.
 * @return True on success; false saves an error and preserves every operand, including for an invalid kind.
 * @note Variables are explicit mathematical notation, for example x then k or t then s. Inputs are consumed
 * only after successful construction; no transform is evaluated and no rendering context is required.
 */
bool TeX_transform(TeX_builder_t *builder, TeX_transform_kind_t kind);

/**
 * @brief Replace integrand, variable then selected bounds by integral notation.
 * @param builder Borrowed builder; push the body, variable, then lower and/or upper bounds in that order.
 * @param limits Which bounds are present; none gives an indefinite integral.
 * @return True on success; false saves an error and preserves all operands. No integration is performed.
 */
bool TeX_integral(TeX_builder_t *builder, TeX_limits_t limits);

/**
 * @brief Replace body then variable by an ordinary or partial derivative.
 * @param builder Borrowed builder; the differentiation variable must be on top.
 * @param order Positive derivative order.
 * @param style Ordinary or partial differential notation.
 * @return True on success; false saves an error and preserves both operands. No differentiation is performed.
 */
bool TeX_derivative(TeX_builder_t *builder, unsigned order, TeX_derivative_style_t style);

/**
 * @brief Replace a body and ordered coordinate variables by partial-derivative operator notation.
 * @param builder Borrowed builder; push the body first, then the variables in differentiation application order.
 * @param count Positive number of variable operands to consume after the body; it is the total derivative order.
 * @return True on success; false saves an error and preserves all operands. Zero saves EINVAL, an excessive
 * order saves EOVERFLOW, and unavailable operands save ERANGE. No notation ownership escapes the builder.
 * @note Variables x then y produce a denominator with partial y then partial x. Adjacent repeated variables
 * combine into a power; non-adjacent repetitions retain their order. One variable gives a first derivative.
 * The differentiated body follows the operator in brackets. This typesets mathematics without differentiating it.
 */
bool TeX_partial(TeX_builder_t *builder, size_t count);

/**
 * @brief Replace row-major cells by a matrix without interpreting their contents.
 * @param builder Borrowed builder; push the first row first and the final cell last.
 * @param rows Positive number of rows.
 * @param columns Positive number of columns; rows times columns must fit size_t.
 * @param style Square brackets, parentheses or plain matrix presentation.
 * @param row_spacing Extra row spacing in points, or zero for the environment default.
 * @return True on success; false saves an error and preserves every cell.
 * @note Push an empty raw fragment for an intentionally empty cell.
 */
bool TeX_matrix(TeX_builder_t *builder, size_t rows, size_t columns,
                      TeX_matrix_style_t style, unsigned row_spacing);

/**
 * @brief Wrap the top body, optionally followed by bindings, in native bound notation.
 * @param builder Borrowed builder; bindings, when present, must be on top.
 * @param bindings Whether to consume a binding fragment after the body.
 * @return True on success; false saves an error and preserves all operands.
 */
bool TeX_bound(TeX_builder_t *builder, bool bindings);

/**
 * @brief Place a scalar factor beside the following mathematical body with compact spacing.
 * @param builder Borrowed builder; push the scalar first and the body last.
 * @return True on success; false saves an error and preserves both operands.
 */
bool TeX_scaled(TeX_builder_t *builder);

/**
 * @brief Replace real then imaginary component by a two-row Cartesian presentation.
 * @param builder Borrowed builder; the imaginary coefficient must be on top.
 * @return True on success; false saves an error and preserves both operands.
 */
bool TeX_cartesian_sum(TeX_builder_t *builder);

/**
 * @brief Replace centre, offset and optional denominator by a symmetric pair.
 * @param builder Borrowed builder; push centre, offset, then denominator when present.
 * @param denominator Whether a denominator operand is present.
 * @param group_offset Whether the offset requires visible parentheses.
 * @return True on success; false saves an error and preserves every operand.
 */
bool TeX_symmetric_pair(TeX_builder_t *builder, bool denominator, bool group_offset);

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
 * Accepts whole equations and relation chains with <, <=, >=, >, !=, ==, ~~= and ~==. A single = remains
 * an equality alias. Approximate equality (~~=) and congruence (~==) are typeset without numerical comparisons.
 * Unicode relation signs are also accepted. Relations are presentation input, not requests to solve inequalities.
 * Accepts scalar indices such as x_n or x_(n+1). Undeclared calls on equation sides remain
 * symbolic functions; use explicit multiplication to distinguish P*x from P(x). Tensor syntax is not supported.
 * Also accepts a parenthesised matrix literal containing an explicit column comma or row semicolon,
 * optionally followed by a scalar power or multiplied explicitly by a scalar on either side.
 * Entries use the native matrix parser; compound powers must be parenthesised.
 * Matrix powers are displayed, not evaluated; outer scalar bindings are parsed without numerical substitution.
 * Differential-equation problem text uses its native parser and displays initial/boundary conditions without solving.
 * Authored prime derivatives in a single-coordinate ODE retain primes and function-style initial conditions.
 * Explicit coordinate derivatives and partial differential equations retain their differential-fraction notation.
 * Matrix sums and matrix–matrix products are not accepted here. Captions belong in separate literal text.
 *
 * @param text Borrowed mathematical source; neither modified nor retained. Must not be NULL or contain NUL.
 * @return Independently owned notation, released with TeX_free(), or NULL on parse, formatting or allocation failure.
 * @note Invalid source follows the expression parser's diagnostics, including messages on stderr.
 * @note Simplification follows the expression module's algebra and domain conventions; this is not verbatim typesetting.
 */
TeX_t *TeX_from_expression(const string_t *text);

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
