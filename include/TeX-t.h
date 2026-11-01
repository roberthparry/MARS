/**
 * @file TeX-t.h
 * @brief Owned mathematical notation independent of algebra and applications.
 *
 * TeX_builder_t is the preferred authoring interface. Construct notation in reverse Polish order: enter operands, then
 * apply fractions, roots, scripts, tensors, calculus, relations, equations or matrices.
 * The builder owns every intermediate fragment; finish transfers its one completed
 * TeX_t value to the caller. Failed operations preserve the stack and record a
 * persistent error, preventing accidental completion after an unchecked failure.
 * Operand-entry names imply a push: TeX_symbol(), TeX_integer(), TeX_math() and
 * the other entry functions append one value without consuming existing operands.
 * Formula operators such as TeX_add(), TeX_frac() and TeX_tensor() apply to values
 * already on the stack. Stack controls and notation access have their own documented effects.
 * Lists are immutable stack objects: push members, push their count, then TeX_list().
 * Collections, matrix dimensions and derivative orders are operands rather than C arrays
 * or operation arguments. Nested lists are limited to 128 levels. Presentation selectors
 * choose notation only; no operation evaluates the supplied mathematics.
 * Literal text is escaped. Explicit composition performs no algebraic rewriting
 * and never runs a TeX engine. Immutable values may be read concurrently;
 * destruction and all access to a shared builder require external synchronisation.
 * Optional native-text entry uses the native mathematical modules for parsing and simplification.
 * TeX_from_expression is a standalone conversion convenience, not a fragment-construction API.
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

/** @brief Matrix delimiter styles for entries containing mathematical notation. */
typedef enum {
    TeX_MATRIX_BRACKETS,    /**< Square brackets using the bmatrix environment. */
    TeX_MATRIX_PARENTHESES, /**< Parentheses using the pmatrix environment. */
    TeX_MATRIX_PLAIN        /**< No delimiters, using the matrix environment. */
} TeX_matrix_style_t;

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
 * @name Operand entry (push)
 * Each entry function has the stack effect S -> S value: one owned operand is
 * added and no existing operand is consumed. The builder copies borrowed input;
 * failure leaves the stack unchanged. The push is implied by the operation name.
 * TeX_depth() separately enters the current depth as an integer stack control.
 * @{
 */

/**
 * @brief Push one plain mathematical symbol without accepting operator or TeX command fragments.
 * @param builder Borrowed builder receiving owned notation.
 * @param name Borrowed NUL-terminated name, copied and not retained. ASCII names such as psi, mu, lambda,
 * Lambda, hbar, infinity, partial and nabla select symbols. Other identifier names and Unicode symbols are accepted too.
 * @return True on success; false saves an error and preserves the stack. Empty names and punctuation are invalid.
 * @note All standard Greek names are supported; an initial capital selects uppercase. No special keyboard is needed.
 */
bool TeX_symbol(TeX_builder_t *builder, const char *name);

/**
 * @brief Escape and push literal UTF-8 prose.
 * @param builder Borrowed builder receiving the escaped value.
 * @param text Borrowed string, neither modified nor retained; NULL and embedded NUL are invalid.
 * @return True on success; false saves an error and preserves the stack. Empty text is permitted.
 */
bool TeX_text(TeX_builder_t *builder, const string_t *text);

/**
 * @brief Parse and push native mathematical text using TeX_from_expression().
 * @param builder Borrowed builder receiving the result.
 * @param text Borrowed mathematical string, neither modified nor retained.
 * @return True on success; false saves an error and preserves the stack. Native parser diagnostics may appear.
 */
bool TeX_expression(TeX_builder_t *builder, const string_t *text);

/**
 * @brief Push native mathematical text supplied as a convenient C string.
 * @param builder Borrowed builder receiving the result.
 * @param text Borrowed NUL-terminated native mathematics, copied before parsing; NULL is invalid.
 * @return True on success; false saves an error and preserves the stack.
 * @note This is the C-string equivalent of TeX_expression(), with the same simplification conventions.
 */
bool TeX_math(TeX_builder_t *builder, const char *text);

/**
 * @brief Push a decimal integer without requiring source construction.
 * @param builder Borrowed builder receiving the notation.
 * @param value Integer to typeset; no evaluation is performed.
 * @return True on success; false saves an error and preserves the stack.
 * @note The literal value is retained for TeX_roll() controls, including after duplication or copying.
 */
bool TeX_integer(TeX_builder_t *builder, long value);

/**
 * @brief Push an independently owned copy of existing notation.
 * @param builder Borrowed builder receiving a copy.
 * @param notation Borrowed non-NULL notation, which may belong to this builder and is never retained.
 * @return True on success; false saves an error and preserves the stack.
 */
bool TeX_notation(TeX_builder_t *builder, const TeX_t *notation);

/**
 * @brief Push explicit mathematical spacing.
 * @param builder Borrowed builder receiving the spacing fragment.
 * @param space Thin or quad spacing; other values are invalid.
 * @return True on success; false saves an error and preserves the stack.
 */
bool TeX_space(TeX_builder_t *builder, TeX_space_t space);

/**
 * @brief Push mathematical source written directly in TeX syntax.
 * @param builder Borrowed builder receiving an owned copy.
 * @param source Borrowed NUL-terminated trusted TeX body, authored directly or generated elsewhere.
 * The source is copied verbatim, neither retained nor syntax-checked; NULL is invalid.
 * @return True on success; false saves an error and leaves the stack unchanged.
 * @note No escaping, native mathematical parsing, simplification or evaluation occurs. Supply the mathematical
 * body without dollar signs or display-mode delimiters. Raw source can be combined with ordinary stack operations;
 * its unknown structure is conservatively grouped in arithmetic. Use TeX_text() to escape literal prose.
 * @note This function stores source; a TeX engine performs typesetting and diagnoses invalid TeX syntax.
 */
bool TeX_raw(TeX_builder_t *builder, const char *source);

/** @} */

/**
 * @name Stack manipulation and index annotations
 * These functions have their individual HP48-style or unary annotation effects.
 * They preserve existing operand ownership rules and the saved-error contract.
 * @{
 */

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
 * @param builder Borrowed builder with a literal integer level on top, pushed with TeX_integer() or copied.
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
 * @brief Assemble an HP48-style list from preceding objects and a count on top.
 * @param builder Borrowed builder; push members in order, then their non-negative literal integer count.
 * @return True on success; false saves EINVAL for a non-integer or negative count, ERANGE for missing
 * members, EOVERFLOW for excessive nesting or size, or ENOMEM. All inputs survive failure unchanged.
 * @note Zero constructs an empty list. Lists own independent copies and may contain other lists,
 * matrices and marked indices. DUP, DROP, SWAP, ROT and ROLL treat each list as one operand.
 */
bool TeX_list(TeX_builder_t *builder);

/**
 * @brief Expand the top list into its members followed by their literal integer count.
 * @param builder Borrowed builder requiring a list on top.
 * @return True on success; false saves an error and preserves the complete stack.
 * @note Matches HP48 LIST->. Members are independent copies; the consumed list's views expire,
 * whilst lower operands retain their identities. No ownership transfers to the caller.
 */
bool TeX_unlist(TeX_builder_t *builder);

/**
 * @brief Push the current stack depth as a literal integer, as with HP48 DEPTH.
 * @param builder Borrowed builder; all existing operands are preserved.
 * @return True on success; false saves an error and leaves the stack unchanged.
 * @note TeX_depth(), TeX_list() gathers the entire stack into one list without a C-side count.
 */
bool TeX_depth(TeX_builder_t *builder);

/**
 * @brief Mark the top label as an upper index operand for a later index list.
 * @param builder Borrowed builder requiring one label.
 * @return True on success; false saves an error and preserves the label.
 * @note Replaces any existing variance mark; the underlying label is copied unchanged.
 * The same mark selects an upper bound in a calculus bound list.
 */
bool TeX_upper(TeX_builder_t *builder);

/**
 * @brief Mark the top label as a lower index operand for a later index list.
 * @param builder Borrowed builder requiring one label.
 * @return True on success; false saves an error and preserves the label.
 * @note Unmarked labels in a tensor index list also occupy lower slots.
 */
bool TeX_lower(TeX_builder_t *builder);

/**
 * @brief Mark the top index label or index list with a comma for partial-derivative notation.
 * @param builder Borrowed builder requiring one label or non-empty list of derivative labels.
 * @return True on success; false saves an error and preserves the operand. The builder owns the marked copy.
 * @note Include this object after the component's own indices in the list consumed by TeX_tensor().
 * Labels default to lower placement; TeX_upper() or TeX_lower() may explicitly select placement.
 * A marked list supplies ordered higher derivatives. No differentiation, evaluation or coordinate change occurs.
 */
bool TeX_comma(TeX_builder_t *builder);

/**
 * @brief Mark the top index label or index list with a semicolon for covariant-derivative notation.
 * @param builder Borrowed builder requiring one label or non-empty list of derivative labels.
 * @return True on success; false saves an error and preserves the operand. The builder owns the marked copy.
 * @note Use in a tensor index list as with TeX_comma(). Component and derivative indices retain their order;
 * a following lower derivative index shares the existing lower script, giving forms such as V_{i;j}.
 * No connection is chosen or calculated, and covariant derivatives are never commuted or evaluated.
 */
bool TeX_semicolon(TeX_builder_t *builder);

/** @} */

/**
 * @name Formula construction (apply)
 * Operators consume their documented top operands and replace them with the
 * constructed notation. Counts, dimensions, orders and collections are stack
 * operands. Function-name and presentation arguments select the operation.
 * For example, TeX_function() applies a name to an existing argument list;
 * TeX_math() instead enters a new formula supplied as native mathematical text.
 * @{
 */

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
 * @brief Apply a plain named function to the top argument list.
 * @param builder Borrowed builder requiring one list; member order is argument order.
 * @param name Borrowed NUL-terminated UTF-8 function name, validated like TeX_symbol() and not retained.
 * @return True on success; false saves an error and preserves the arguments. No function is evaluated.
 * @note Standard function spelling is selected internally; callers supply names such as sin, P or psi.
 */
bool TeX_function(TeX_builder_t *builder, const char *name);

/**
 * @brief Apply a function already on the stack to its following arguments.
 * @param builder Borrowed builder; push the function first, then one argument list in display order.
 * @return True on success; false saves an error and preserves every operand. No function is evaluated.
 * @note Supports indexed function symbols built with TeX_subscript().
 */
bool TeX_call(TeX_builder_t *builder);

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
 * @brief Replace an operand and its literal integer order by prime notation.
 * @param builder Borrowed builder; push the body followed by its positive order, at most UINT_MAX.
 * @return True on success; false records EINVAL for a non-integer or non-positive order,
 * EOVERFLOW for an excessive order, ERANGE for missing operands, or an allocation error.
 * The builder owns the result; no notation ownership transfers to the caller.
 * @note This constructs notation without differentiation. Compound operands retain visible grouping;
 * use TeX_call() afterwards to show an evaluation point such as y'(0).
 * To prime a tensor index, apply this operation to its label before TeX_upper() or TeX_lower(), then
 * assemble the index list for TeX_tensor(). Such a prime labels a coordinate chart, not a derivative.
 * Orders one through three use primes; larger orders use a parenthesised superscript.
 */
bool TeX_prime(TeX_builder_t *builder);

/**
 * @brief Replace the top operand by the same notation with an overbar.
 * @param builder Borrowed builder requiring one operand; the builder is neither consumed nor retained.
 * @return True on success; false records EINVAL for a NULL builder, ERANGE for an empty stack, or an
 * allocation error. A saved error blocks the operation. Failure preserves the stack and its existing operands.
 * @note The bar covers the complete operand without assigning a mathematical meaning or evaluating it.
 * To construct barred coordinates, apply it to the base symbol before attaching tensor indices.
 * Applying it after tensor assembly instead bars the complete indexed notation. The builder owns the
 * replacement; finish transfers the completed formula, which the caller releases with TeX_free().
 */
bool TeX_bar(TeX_builder_t *builder);

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
 * @brief Concatenate members of the top list in their original push order.
 * @param builder Borrowed builder requiring one list; an empty list gives empty notation.
 * @return True on success; false saves an error and preserves the stack.
 */
bool TeX_sequence(TeX_builder_t *builder);

/**
 * @brief Combine two mathematical rows or blocks with a visible line break.
 * @param builder Borrowed builder; push the first row followed by the next row.
 * @return True on success; false saves an error and preserves both operands.
 * @note Repeated calls append rows to one block without nesting environments. Rows initially share
 * a left edge; TeX_align() aligns their stored relation signs. Adding to an aligned block retains alignment.
 * No mathematical source is parsed. The builder owns the resulting block and all copied row data.
 */
bool TeX_newline(TeX_builder_t *builder);

/**
 * @brief Indent the top mathematical body by a spacing value supplied on the stack.
 * @param builder Borrowed builder; push the body then a non-negative literal integer in TeX points, at most UINT_MAX.
 * @return True on success; false saves an error and preserves body and spacing. Zero adds no visible indentation.
 * @note Indentation may be applied to a row or a complete block. Retained row structure permits later alignment.
 */
bool TeX_indent(TeX_builder_t *builder);

/**
 * @brief Align a list of rows or an existing multiline block without interpreting its mathematics.
 * @param builder Borrowed builder requiring a non-empty row list or a block made with TeX_newline().
 * @return True on success; false saves an error and preserves the input object, including invalid or empty lists.
 * @note Rows constructed with TeX_eq() and the other RPN relation operations align at their outermost
 * relation sign. Row lists supply explicit left-aligned cells with visible spacing between columns. Other rows,
 * including raw TeX and native text, remain whole left-aligned cells; their rendered source is never parsed.
 * A new line appended after alignment retains the block's alignment. All row copies belong to the builder.
 */
bool TeX_align(TeX_builder_t *builder);

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
 * @param builder Borrowed builder; push the base, then one list of index labels in left-to-right order.
 * Mark upper slots with TeX_upper(); lower slots may be unmarked or marked with TeX_lower().
 * Index labels may be primed with TeX_prime() before selecting their placement and assembling the list.
 * Mark derivative labels or label lists with TeX_comma() or TeX_semicolon() before outer list assembly.
 * @return True on success; false saves an error and preserves both operands. An empty list groups the base alone.
 * @note There is no dimension, metric signature or Cartesian-coordinate restriction. All-lower, all-upper and
 * mixed slots retain their authored order. Repeated labels are displayed without performing contractions.
 * Adjacent labels of the same placement share one script, including comma and semicolon derivative groups.
 * Changes between upper and lower placement retain distinct horizontal slots and the supplied index order.
 * The same placement operation can format indexed operators or connection coefficients, including Christoffel
 * symbols; it does not assert that those objects transform as tensors or calculate a covariant derivative.
 */
bool TeX_tensor(TeX_builder_t *builder);

/**
 * @brief Replace a summand and a bound list by summation notation.
 * @param builder Borrowed builder; push the body followed by a list of zero, one or two bounds.
 * @return True on success; false saves an error and preserves all operands. No sum is evaluated.
 * @note An empty list means no bounds; one unmarked member is lower, two are lower then upper.
 * TeX_upper() marks a lone upper bound; TeX_lower() can explicitly mark a lower bound.
 * Duplicate upper or lower bounds are rejected.
 */
bool TeX_sum(TeX_builder_t *builder);

/**
 * @brief Replace a product body then selected bounds by indexed-product notation.
 * @param builder Borrowed builder; push the body followed by one bound list, as documented for TeX_sum().
 * @return True on success; false saves an error and preserves all operands. The builder retains ownership.
 * @note This constructs a large product sign with optional bounds. Each lower annotation may be an equation
 * constructed with TeX_eq(). No multiplication or evaluation is performed.
 */
bool TeX_prod(TeX_builder_t *builder);

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
 * @param builder Borrowed builder; push the body, variable, then a bound list as documented for TeX_sum().
 * An empty list gives an indefinite integral.
 * @return True on success; false saves an error and preserves all operands. No integration is performed.
 */
bool TeX_integral(TeX_builder_t *builder);

/**
 * @brief Replace body, variable then literal integer order by a derivative.
 * @param builder Borrowed builder; the positive derivative order, at most UINT_MAX, must be on top.
 * @param style Ordinary or partial differential notation.
 * @return True on success; false saves an error and preserves all three operands. No differentiation is performed.
 */
bool TeX_derivative(TeX_builder_t *builder, TeX_derivative_style_t style);

/**
 * @brief Replace a body and ordered coordinate variables by partial-derivative operator notation.
 * @param builder Borrowed builder; push the body first, then a non-empty list of variables in application order.
 * @return True on success; false saves an error and preserves all operands. An empty list saves EINVAL, an excessive
 * order saves EOVERFLOW, and unavailable operands save ERANGE. No notation ownership escapes the builder.
 * @note Variables x then y produce a denominator with partial y then partial x. Adjacent repeated variables
 * combine into a power; non-adjacent repetitions retain their order. One variable gives a first derivative.
 * The differentiated body follows the operator in brackets. This typesets mathematics without differentiating it.
 */
bool TeX_partial(TeX_builder_t *builder);

/**
 * @brief Assemble a matrix from row lists or HP48-style dimensions supplied on the stack.
 * @param builder Borrowed builder; supply either one list of equally sized, non-empty row lists,
 * or row-major cell operands followed by a two-member list containing positive literal row and column counts.
 * @return True on success; false saves an error and preserves all cells and dimension objects.
 * @note The result uses square brackets and default row spacing. TeX_matrix_style() changes its
 * delimiters; TeX_matrix_spacing() consumes an explicit spacing operand. Rows remain structural data
 * through DUP and copying. Ragged, empty or excessive dimensions are rejected. Nothing is evaluated.
 */
bool TeX_matrix(TeX_builder_t *builder);

/**
 * @brief Change the top structural matrix's delimiter style.
 * @param builder Borrowed builder requiring a matrix constructed with TeX_matrix().
 * @param style Square brackets, parentheses or plain presentation; no mathematical operands are supplied here.
 * @return True on success; false saves an error and preserves the matrix. Its cells and row spacing are unchanged.
 */
bool TeX_matrix_style(TeX_builder_t *builder, TeX_matrix_style_t style);

/**
 * @brief Apply extra row spacing from a literal integer on the stack.
 * @param builder Borrowed builder; push a structural matrix then a non-negative spacing in points, at most UINT_MAX.
 * @return True on success; false saves an error and preserves both operands. Zero restores default spacing.
 */
bool TeX_matrix_spacing(TeX_builder_t *builder);

/**
 * @brief Wrap a body and a binding list in native bound notation.
 * @param builder Borrowed builder; push the body followed by a list of presentation fragments.
 * Members are concatenated as supplied; an empty list means no bindings.
 * @return True on success; false saves an error and preserves all operands.
 */
bool TeX_bound(TeX_builder_t *builder);

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
 * @brief Replace centre, offset and a denominator list by a symmetric pair.
 * @param builder Borrowed builder; push centre, offset, then an empty or one-member denominator list.
 * @param group_offset Whether the offset requires visible parentheses.
 * @return True on success; false saves an error and preserves every operand.
 */
bool TeX_symmetric_pair(TeX_builder_t *builder, bool group_offset);

/** @} */

/**
 * @brief Convert existing MARS mathematical text into readable TeX as a standalone convenience.
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
 * @note Prefer TeX_builder_t for authoring formulas. TeX_math() and TeX_expression() optionally enter native text
 * within that same stack; use this standalone converter when a complete native mathematical string already exists.
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
