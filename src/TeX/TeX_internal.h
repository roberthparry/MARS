/**
 * @file TeX_internal.h
 * @brief TeX-only context ownership, symbol spelling and fragment layout helpers.
 *
 * TeX implementations and authorised white-box tests use this private interface. Native mathematical modules
 * use internal/TeX_internal.h instead, which exposes only their shared semantic and layout contract. Define
 * MARS_TeX_INTERNAL_ACCESS immediately before including this header. Static spelling results remain immutable;
 * allocated notation, strings, contexts and models retain their documented release functions.
 */
#ifndef MARS_TeX_INTERNAL_H
#define MARS_TeX_INTERNAL_H
#if !defined(MARS_TeX_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_TeX_INTERNAL_ACCESS."
#endif

#include "string-t.h"
#include "TeX-t.h"

#define MARS_SHARED_TeX_INTERNAL_ACCESS
#include "internal/TeX_internal.h"

/** @brief Private equation layouts used by semantic rendering and the native equation adapter. */
typedef enum {
    TeX_EQUATION_INLINE, /**< Separate the sides by an equals sign. */
    TeX_EQUATION_ALIGNED /**< Top-aligned equation, preserving an already wrapped right side. */
} TeX_equation_style_t;

/** @brief Forward declaration of a provider's read-only callback table. */
typedef struct _TeX_provider_t TeX_provider_t;

/**
 * @brief Borrowed node and its interpretation context.
 *
 * TeX never dereferences node or context. Both and the provider table must remain
 * valid throughout rendering. Children may use different providers.
 */
typedef struct {
    const TeX_provider_t *provider; /**< Borrowed callback table; its address identifies this interpretation. */
    const void *context; /**< Borrowed provider state; NULL is allowed when the callbacks support it. */
    const void *node; /**< Borrowed non-NULL node identity, dereferenced only by its provider. */
} TeX_node_ref_t;

/**
 * @brief Callbacks exposing semantic structure without transferring ownership.
 *
 * Return false on failure. Describe must fill a complete view; child supplies
 * the indexed child without modifying the graph. Callbacks must not return TeX
 * syntax, free borrowed nodes or retain TeX's output pointers. No callback is
 * invoked concurrently within one rendering call. Re-entry is permitted.
 */
struct _TeX_provider_t {
    /**
     * @brief Fill a complete view for a borrowed node; return false to reject it or report failure.
     * @param context Borrowed callback state; its owner keeps it valid throughout the call.
     * @param node Borrowed semantic node identity interpreted only by this provider.
     * @param view Caller-owned output receiving a copied descriptor of the requested semantic node.
     * @return True when the requested operation succeeds; false when unavailable, invalid or unsuccessful.
     */
    bool (*describe)(const void *context, const void *node, TeX_node_view_t *view);

    /**
     * @brief Supply a borrowed zero-based child reference; return false for an unavailable child or failure.
     * @param context Borrowed callback state; its owner keeps it valid throughout the call.
     * @param node Borrowed semantic node identity interpreted only by this provider.
     * @param index Zero-based position of the requested operand, coordinate or term.
     * @param child Required output receiving a borrowed semantic child reference for the requested index.
     * @return True when the requested operation succeeds; false when unavailable, invalid or unsuccessful.
     */
    bool (*child)(const void *context, const void *node, size_t index, TeX_node_ref_t *child);
};

/**
 * @brief Resolve a Greek Unicode code point to its standard mathematical TeX spelling.
 * @param codepoint Unicode scalar value to look up directly.
 * @return Immutable static spelling, or NULL for an unknown symbol; never free or modify it.
 */
const char *TeX_semantic_greek(unsigned codepoint);

/**
 * @brief Resolve a mathematical function name through the private perfect-hash spelling table.
 * @param name Borrowed plain UTF-8 function name.
 * @return Immutable static spelling, or NULL for a user-defined or unknown name; never free or modify it.
 */
const char *TeX_semantic_function(const string_t *name);

/**
 * @brief Resolve a plain operation name to its established TeX spelling.
 * @param name Borrowed NUL-terminated operation name, valid throughout this call.
 * @return Immutable static spelling, or NULL for an unknown name; never free or modify it.
 */
const char *TeX_operation_name(const char *name);

/** @brief Presentation of a mathematical qualification. */
typedef enum {
    TeX_QUALIFICATION_INLINE, /**< An underbrace caption attached to the operand. */
    TeX_QUALIFICATION_CONDITION, /**< A caption following a condition separator. */
    TeX_QUALIFICATION_TRAILING, /**< A parenthesised caption after the expression. */
    TeX_QUALIFICATION_FINITE_PART /**< A finite-part operator around the operand. */
} TeX_qualification_style_t;

/**
 * @brief Create a rendering context which takes responsibility for releasing provider state.
 * @param root Root reference; node and provider remain subject to their usual lifetime rules.
 * @param release Callback invoked once with root.context when the context is freed; NULL means no release.
 * @return Owned context, or NULL on failure. Ownership of root.context transfers only on success.
 * @note The callback is responsible for any graph storage it owns; TeX never frees borrowed node pointers.
 */
TeX_context_t *TeX_context_adopt(TeX_node_ref_t root, void (*release)(const void *));

/**
 * @brief Borrow the root reference without retaining its provider state or graph.
 * @param context Borrowed rendering context whose provider state and graph remain valid throughout the call.
 * @return Root reference valid while its context and graph remain alive; a zeroed reference for NULL.
 */
TeX_node_ref_t TeX_context_root(const TeX_context_t *context);

/**
 * @brief Format an operand and plain-text qualification without interpreting their meaning.
 * @param operand Borrowed rendered mathematical operand; it remains caller-owned.
 * @param caption Borrowed plain-text caption, or NULL for TeX_QUALIFICATION_FINITE_PART.
 * @param style Presentation style selecting the requested native or TeX representation.
 * @return Owned text released with string_free, or NULL when no representation is available or allocation fails.
 */
string_t *TeX_qualification(const string_t *operand, const char *caption, TeX_qualification_style_t style);

/**
 * @brief Render a plain mathematical identifier; return owned notation released with TeX_free.
 * @param name Borrowed UTF-8 mathematical name; no text ownership transfers.
 * @return Owned notation released with TeX_free, or NULL when input is unsupported or rendering fails.
 */
TeX_t *TeX_identifier(const string_t *name);

/**
 * @brief Record presentation structure on newly owned notation without changing its source.
 * @param notation Owned result to classify, or NULL; ownership stays with the caller.
 * @param kind Presentation node kind; TeX_NODE_COUNT denotes trusted source with unknown structure.
 * @return The same pointer, or NULL for NULL input; no allocation is performed.
 * @note Use only before sharing the result; publicly visible notation remains immutable.
 */
TeX_t *TeX_classify(TeX_t *notation, TeX_node_kind_t kind);

/**
 * @brief Inspect presentation structure without parsing generated source.
 * @param notation Borrowed notation, or NULL.
 * @return Its stored kind, or TeX_NODE_COUNT for unknown structure or NULL; ownership is unchanged.
 */
TeX_node_kind_t TeX_kind(const TeX_t *notation);

/**
 * @brief Copy notation together with its presentation structure.
 * @param notation Borrowed non-NULL notation, neither modified nor retained.
 * @return Owned copy released with TeX_free(), or NULL for NULL input or allocation failure.
 */
TeX_t *TeX_copy(const TeX_t *notation);

/**
 * @brief Create immutable decimal notation retaining its exact literal integer value.
 * @param value Literal integer to store; no arithmetic evaluation is performed.
 * @return Owned notation released with TeX_free(), or NULL on allocation failure.
 * @note Used by the RPN builder so stack controls never require parsing generated TeX.
 */
TeX_t *TeX_compose_integer(long value);

/**
 * @brief Inspect literal integer metadata independently of the rendered source.
 * @param notation Borrowed notation to inspect, neither modified nor retained; NULL is rejected.
 * @param[out] value Receives the literal integer on success; NULL is rejected and failure leaves it unchanged.
 * @return True only for an explicitly stored integer, including its copies. No ownership transfers or allocation occurs.
 */
bool TeX_integer_value(const TeX_t *notation, long *value);

/** @brief Structural stack-object tags; formula fragments have no collection payload. */
typedef enum {
    TeX_OBJECT_FORMULA, /**< Ordinary immutable notation. */
    TeX_OBJECT_LIST,    /**< Ordered, possibly nested list. */
    TeX_OBJECT_LOWER,   /**< One lower index label. */
    TeX_OBJECT_UPPER,   /**< One upper index label. */
    TeX_OBJECT_MATRIX,  /**< Matrix retaining its row lists. */
    TeX_OBJECT_RELATION, /**< Left side, literal relation selector and right side. */
    TeX_OBJECT_LINES,   /**< Ordered mathematical rows with common left alignment. */
    TeX_OBJECT_ALIGNED, /**< Ordered rows aligned at stored relation signs or explicit cells. */
    TeX_OBJECT_INDENT,  /**< Body followed by literal indentation in points. */
    TeX_OBJECT_COMMA,   /**< Partial-derivative index label or group. */
    TeX_OBJECT_SEMICOLON /**< Covariant-derivative index label or group. */
} TeX_object_kind_t;

/**
 * @brief Inspect a stack object's stored structural tag without parsing notation.
 * @param notation Borrowed object, or NULL.
 * @return Its tag; NULL reports TeX_OBJECT_FORMULA. No ownership changes.
 */
TeX_object_kind_t TeX_object_kind(const TeX_t *notation);

/**
 * @brief Borrow directly indexed collection members for the lifetime of their owner.
 * @param notation Borrowed object, or NULL.
 * @param[out] count Required output receiving the member count, zero for NULL.
 * @return Borrowed member array, possibly NULL for an empty collection. Never release its entries.
 */
const TeX_t *const *TeX_object_items(const TeX_t *notation, size_t *count);

/**
 * @brief Copy members into an immutable structural stack object, bounded to 128 nested levels.
 * @param kind Stored collection tag.
 * @param items Borrowed member array, NULL permitted for zero count; no ownership transfers.
 * @param count Number of members to copy in display order.
 * @return Owned object released with TeX_free(), or NULL with errno set on failure.
 */
TeX_t *TeX_object_new(TeX_object_kind_t kind, const TeX_t *const *items, size_t count);

/**
 * @brief Attach copied structural members to independently owned rendered notation.
 * @param kind Structural object tag.
 * @param items Borrowed members, copied without transferring their ownership.
 * @param count Number of members, bounded as for TeX_object_new().
 * @param rendered Owned notation, consumed on every path, including failure; NULL reports failure.
 * @return Owned structural value released with TeX_free(), or NULL with errno set.
 */
TeX_t *TeX_object_rendered(TeX_object_kind_t kind, const TeX_t *const *items, size_t count, TeX_t *rendered);

/**
 * @brief Construct a relation retaining its sides and operator for later alignment.
 * @param left Borrowed left side, unchanged and not retained.
 * @param right Borrowed right side, unchanged and not retained.
 * @param relation Valid relation selector, stored as structural integer data.
 * @return Owned notation released with TeX_free(), or NULL with errno set; input ownership is unchanged.
 */
TeX_t *TeX_object_relation(const TeX_t *left, const TeX_t *right, TeX_relation_t relation);

/**
 * @brief Render a collection of rows without parsing any mathematical source.
 * @param rows Borrowed row array, copied into the resulting block.
 * @param count Positive row count; each row may be notation, a relation or a list of column cells.
 * @param align True to align stored relation signs or explicit cell boundaries; false for common left alignment.
 * @return Owned block released with TeX_free(), or NULL with errno set; all input ownership is unchanged.
 * @note Explicit cell lists use a left-aligned array whose width is the widest row. Other aligned blocks
 * retain relation alignment. Empty cell lists are invalid; shorter rows leave their trailing cells empty.
 */
TeX_t *TeX_object_lines(const TeX_t *const *rows, size_t count, bool align);

/**
 * @brief Combine two rows or existing blocks, flattening their retained row collections.
 * @param first Borrowed first row or block; copied rather than consumed.
 * @param second Borrowed following row or block; copied rather than consumed.
 * @return Owned block released with TeX_free(), or NULL with errno set; aligned blocks retain alignment.
 */
TeX_t *TeX_object_newline(const TeX_t *first, const TeX_t *second);

/**
 * @brief Indent notation while retaining the body for structural alignment.
 * @param body Borrowed body, unchanged and not retained.
 * @param spacing Non-negative indentation in TeX points.
 * @return Owned notation released with TeX_free(), or NULL with errno set.
 */
TeX_t *TeX_object_indent(const TeX_t *body, unsigned spacing);

/**
 * @brief Look up a relation's typesetting operator by direct enumeration indexing.
 * @param relation Relation selector to inspect.
 * @return Borrowed process-lifetime syntax, or NULL for an invalid selector; never release it.
 */
const char *TeX_relation_operator(TeX_relation_t relation);

/**
 * @brief Copy one label into a variance-marked index operand.
 * @param label Borrowed notation; an existing variance mark is replaced rather than nested.
 * @param upper True for an upper index, false for a lower index.
 * @return Owned object released with TeX_free(), or NULL on failure; input ownership is unchanged.
 */
TeX_t *TeX_object_index(const TeX_t *label, bool upper);

/**
 * @brief Mark a copied index label or non-empty list as a partial or covariant derivative group.
 * @param label Borrowed label or group, unchanged and not retained; NULL and empty lists are rejected.
 * @param covariant True for a semicolon, false for a comma; an existing punctuation mark is replaced.
 * @return Owned marked notation released with TeX_free(), or NULL with errno set; input ownership is unchanged.
 */
TeX_t *TeX_object_derivative_index(const TeX_t *label, bool covariant);

/**
 * @brief Compose ordered component and derivative indices from their structural list objects.
 * @param base Borrowed non-NULL base notation, unchanged and not retained.
 * @param list Borrowed index list; labels, variance marks and derivative groups are copied during composition.
 * @return Owned notation released with TeX_free(), or NULL with errno set. Empty component lists are permitted;
 * empty derivative groups are rejected. No index is reordered or interpreted as a mathematical operation.
 */
TeX_t *TeX_compose_tensor_indices(const TeX_t *base, const TeX_t *list);

/**
 * @brief Compose a rectangular matrix from an immutable list of non-empty row lists.
 * @param rows Borrowed row collection; no ownership transfers.
 * @param style Valid matrix delimiter selection.
 * @param spacing Additional row spacing in points, zero for the default.
 * @return Owned matrix retaining copied rows, released with TeX_free(), or NULL with errno set.
 */
TeX_t *TeX_object_matrix(const TeX_t *rows, TeX_matrix_style_t style, unsigned spacing);

/**
 * @brief Reformat retained matrix data without parsing generated TeX.
 * @param matrix Borrowed structural matrix, unchanged on every path.
 * @param style New delimiter style when change_style is true.
 * @param spacing New row spacing when change_style is false.
 * @param change_style Select which presentation setting to change.
 * @return Owned replacement released with TeX_free(), or NULL with errno set; no ownership transfers.
 */
TeX_t *TeX_object_matrix_format(const TeX_t *matrix, TeX_matrix_style_t style, unsigned spacing,
                              bool change_style);

/**
 * @brief Decide whether a semantic child requires visible grouping within an operation.
 * @param parent Presentation kind of the enclosing operation.
 * @param child Presentation kind of the operand.
 * @param index Zero-based operand position.
 * @return True when visible mathematical delimiters are required; no ownership changes.
 */
bool TeX_needs_fence(TeX_node_kind_t parent, TeX_node_kind_t child, size_t index);

/**
 * @brief Typeset a validated, plain UTF-8 mathematical symbol without accepting source syntax.
 * @param name Borrowed NUL-terminated symbol name, not retained; punctuation and commands are rejected.
 * @return Owned notation released with TeX_free(), or NULL with errno on invalid input or allocation failure.
 */
TeX_t *TeX_compose_symbol(const char *name);

/**
 * @brief Resolve an ASCII symbol name through a compact perfect hash.
 * @param name Borrowed plain name; NULL, empty strings, embedded NUL and unknown names are rejected.
 * @return Immutable static TeX spelling, or NULL when the name is not recognised; never free or modify it.
 * @note Standard Greek names use initial capitals for uppercase letters, plus common mathematical aliases.
 */
const char *TeX_symbol_name(const string_t *name);

/**
 * @brief Resolve a Unicode Greek or supported mathematical symbol by direct code-point lookup.
 * @param codepoint Unicode scalar value to look up.
 * @return Immutable static spelling, or NULL for an unknown symbol; never free or modify it.
 */
const char *TeX_symbol_codepoint(unsigned codepoint);

/**
 * @brief Compose an addition or subtraction while preserving operand grouping.
 * @param left Borrowed left operand, neither modified nor retained.
 * @param right Borrowed right operand, neither modified nor retained.
 * @param subtract Select subtraction rather than addition.
 * @return Owned notation released with TeX_free(), or NULL on invalid input or allocation failure.
 */
TeX_t *TeX_arithmetic_binary(const TeX_t *left, const TeX_t *right, bool subtract);

/**
 * @brief Negate one displayed operand, retaining visible grouping when required.
 * @param body Borrowed non-NULL operand, neither modified nor retained.
 * @return Owned notation released with TeX_free(), or NULL on invalid input or allocation failure.
 */
TeX_t *TeX_arithmetic_negative(const TeX_t *body);

/**
 * @brief Multiply exactly two factors with mathematical grouping and unambiguous numeric multiplication.
 * @param left Borrowed non-NULL left factor, neither modified nor retained.
 * @param right Borrowed non-NULL right factor, neither modified nor retained.
 * @return Owned notation released with TeX_free(), or NULL on invalid input or allocation failure.
 */
TeX_t *TeX_arithmetic_multiply(const TeX_t *left, const TeX_t *right);

/**
 * @brief Attach scripts to a visibly grouped base when its structure requires delimiters.
 * @param base Borrowed non-NULL base, neither modified nor retained.
 * @param lower Borrowed lower script, or NULL to omit it.
 * @param upper Borrowed upper script, or NULL to omit it.
 * @return Owned notation released with TeX_free(), or NULL on invalid input or allocation failure.
 */
TeX_t *TeX_arithmetic_scripts(const TeX_t *base, const TeX_t *lower, const TeX_t *upper);

/**
 * @brief Attach primes to an operand or index label while preserving its visible scope.
 * @param body Borrowed non-NULL notation, neither modified nor retained.
 * @param order Positive prime count, or parenthesised order above three; zero is invalid.
 * @return Independently owned notation released with TeX_free(), or NULL for invalid input or allocation failure.
 * @note No mathematical differentiation or evaluation occurs.
 */
TeX_t *TeX_arithmetic_prime(const TeX_t *body, unsigned order);

/**
 * @brief Reuse semantic prime notation for an already constructed operand.
 * @param body Borrowed non-NULL notation, neither modified nor retained; grouping is supplied by the caller.
 * @param order Positive prime count, or parenthesised order above three; zero is invalid.
 * @return Owned notation released with TeX_free(), or NULL with EINVAL for invalid input or on allocation failure.
 */
TeX_t *TeX_compose_prime(const TeX_t *body, unsigned order);

/**
 * @brief Place an overbar over one complete mathematical operand.
 * @param body Borrowed non-NULL notation; neither modified nor retained.
 * @return Independently owned notation released with TeX_free(), or NULL with EINVAL for NULL input
 * or an allocation error. No mathematical interpretation, simplification or evaluation is performed.
 * @note The overbar delimits its complete body, so subsequent scripts can treat it as a grouped operand.
 */
TeX_t *TeX_compose_bar(const TeX_t *body);

/**
 * @brief Apply established function spelling to already selected mathematical arguments.
 * @param name Borrowed plain UTF-8 function name, valid throughout this call.
 * @param arguments Borrowed array of count non-NULL argument fragments, or NULL when count is zero.
 * @param count Number of arguments in display order.
 * @return Owned notation released with TeX_free(), or NULL on invalid input or allocation failure.
 */
TeX_t *TeX_named_call(const string_t *name, const TeX_t *const *arguments, size_t count);

/**
 * @brief Apply already constructed function notation to selected arguments.
 * @param function Borrowed non-NULL function notation, neither modified nor retained.
 * @param arguments Borrowed array of count non-NULL arguments, or NULL when count is zero.
 * @param count Number of arguments in display order.
 * @return Owned notation released with TeX_free(), or NULL on invalid input or allocation failure.
 */
TeX_t *TeX_applied(const TeX_t *function, const TeX_t *const *arguments, size_t count);

/**
 * @brief Convert Unicode mathematical notation into a newly allocated TeX string.
 * @param text Borrowed UTF-8 mathematical text; NULL is treated as an empty string.
 * @return Owned C string released with free, or NULL when no representation is available or allocation fails.
 */
char *TeX_from_unicode_math(const char *text);

/**
 * @brief Bind a borrowed semantic root to a new owned rendering context.
 * @param root Borrowed root node reference whose provider and storage obey the documented context lifetime.
 * @return Owned context, released with TeX_context_free, or NULL for invalid arguments or allocation failure.
 * @note The referenced graph, provider tables and provider contexts must outlive every use of this context.
 */
TeX_context_t *TeX_context_new(TeX_node_ref_t root);

/** @brief Private vertical placement used only when composing already selected index labels. */
typedef enum {
    TeX_INDEX_LOWER, /**< Lower index slot. */
    TeX_INDEX_UPPER  /**< Upper index slot. */
} TeX_index_position_t;

/** @brief One borrowed tensor index; slots retain their supplied left-to-right order. */
typedef struct {
    const TeX_t *value;          /**< Non-NULL index notation, copied during construction. */
    TeX_index_position_t position; /**< Lower or upper placement. */
} TeX_index_t;

/**
 * @brief Copy trusted TeX syntax without escaping or validating commands.
 * @param source Borrowed, null-terminated TeX source. Must not be NULL or contain untrusted text.
 * @return Independently owned notation, released with TeX_free(), or NULL on invalid input or allocation failure.
 * @note The source is copied and is not retained.
 */
TeX_t *TeX_compose_raw(const char *source);

/**
 * @brief Escape literal UTF-8 text and enclose it in a TeX text command.
 * @param text Borrowed text, neither modified nor retained. Empty strings are permitted.
 * @return Independently owned notation, released with TeX_free(), or NULL on invalid input or allocation failure.
 * @note NULL input or an embedded NUL is rejected with errno set to EINVAL.
 */
TeX_t *TeX_compose_text(const string_t *text);

/**
 * @brief Compose a fraction from two mathematical fragments.
 * @param numerator Borrowed, non-NULL numerator notation.
 * @param denominator Borrowed, non-NULL denominator notation.
 * @return Independently owned notation, released with TeX_free(), or NULL on invalid input or allocation failure.
 * @note Both fragments are copied. No algebraic validation or simplification is performed.
 */
TeX_t *TeX_compose_fraction(const TeX_t *numerator, const TeX_t *denominator);

/**
 * @brief Compose a square or indexed radical.
 * @param radicand Borrowed, non-NULL notation beneath the radical.
 * @param index Borrowed root index, or NULL for a square root.
 * @return Independently owned notation, released with TeX_free(), or NULL on invalid input or allocation failure.
 * @note Supplied fragments are copied and are not retained.
 */
TeX_t *TeX_compose_root(const TeX_t *radicand, const TeX_t *index);

/**
 * @brief Group the complete base and attach optional lower and upper scripts.
 * @param base Borrowed, non-NULL base notation.
 * @param lower Borrowed subscript notation, or NULL to omit the subscript.
 * @param upper Borrowed superscript notation, or NULL to omit the superscript.
 * @return Independently owned notation, released with TeX_free(), or NULL on invalid input or allocation failure.
 * @note Supplied fragments are copied. With both scripts omitted, the base is still grouped.
 */
TeX_t *TeX_compose_scripts(const TeX_t *base, const TeX_t *lower, const TeX_t *upper);

/**
 * @brief Concatenate mathematical fragments in their supplied order without inserting separators.
 * @param parts Borrowed array of non-NULL fragments; may be NULL only when count is zero.
 * @param count Number of fragments. Zero produces empty notation.
 * @return Independently owned notation, released with TeX_free(), or NULL on invalid input or allocation failure.
 * @note Fragments are copied; neither the array nor its entries are retained.
 */
TeX_t *TeX_compose_sequence(const TeX_t *const *parts, size_t count);

/**
 * @brief Compose an equation from two trusted mathematical fragments.
 * @param left Borrowed, non-NULL left side.
 * @param right Borrowed, non-NULL right side.
 * @param style Inline or top-aligned display; no automatic line breaking is performed.
 * @return Independently owned notation; release with TeX_free(). NULL on invalid input or allocation failure.
 * @note Copies both sides. Aligned mode incorporates a native top-aligned right-side wrapper rather than nesting it.
 */
TeX_t *TeX_compose_equation(const TeX_t *left, const TeX_t *right, TeX_equation_style_t style);

/**
 * @brief Compose two mathematical operands with a TeX-owned relation symbol.
 * @param left Borrowed non-NULL left operand, neither modified nor retained.
 * @param right Borrowed non-NULL right operand, neither modified nor retained.
 * @param relation Mathematical relation selecting the symbol; invalid values are rejected.
 * @return Independently owned notation released with TeX_free(), or NULL with EINVAL for invalid input
 * or on allocation failure. Inputs remain caller-owned and no relation is evaluated.
 */
TeX_t *TeX_compose_relation(const TeX_t *left, const TeX_t *right, TeX_relation_t relation);

/**
 * @brief Enclose notation in automatically sized delimiters.
 * @param body Borrowed, non-NULL mathematical notation.
 * @param fence Delimiter pair selected from TeX_fence_t.
 * @return Independently owned notation, released with TeX_free(), or NULL on invalid input or allocation failure.
 */
TeX_t *TeX_compose_fenced(const TeX_t *body, TeX_fence_t fence);

/**
 * @brief Attach ordered tensor index slots without aligning differently positioned slots on top of each other.
 * @param base Borrowed, non-NULL tensor symbol.
 * @param indices Borrowed array of non-NULL index values and their positions; may be NULL when count is zero.
 * @param count Number of index slots. Zero produces the grouped base alone.
 * @return Independently owned notation, released with TeX_free(), or NULL on invalid input or allocation failure.
 * @note All inputs are copied. This formats indices; it does not contract tensors or check their dimensions.
 */
TeX_t *TeX_compose_tensor(const TeX_t *base, const TeX_index_t *indices, size_t count);

/**
 * @brief Compose summation notation with optional bounds.
 * @param body Borrowed, non-NULL summand notation.
 * @param lower Borrowed lower annotation such as n = 0, or NULL to omit it.
 * @param upper Borrowed upper annotation, or NULL to omit it.
 * @return Independently owned notation, released with TeX_free(), or NULL on invalid input or allocation failure.
 * @note Inputs are copied. No summation is evaluated.
 */
TeX_t *TeX_compose_sum(const TeX_t *body, const TeX_t *lower, const TeX_t *upper);

/**
 * @brief Compose indexed-product notation with optional lower and upper annotations.
 * @param body Borrowed non-NULL product body, neither modified nor retained.
 * @param lower Borrowed lower annotation, or NULL to omit it.
 * @param upper Borrowed upper annotation, or NULL to omit it.
 * @return Independently owned notation released with TeX_free(), or NULL for invalid input or allocation failure.
 * @note Inputs are copied; no product is evaluated.
 */
TeX_t *TeX_compose_product(const TeX_t *body, const TeX_t *lower, const TeX_t *upper);

/**
 * @brief Reuse semantic transform layout for already constructed notation.
 * @param operands Borrowed array of three non-NULL values: body, source variable, target variable.
 * @param kind Forward or inverse Fourier or Laplace notation; other values are rejected.
 * @return Owned notation released with TeX_free(), or NULL with EINVAL for invalid arguments or on allocation failure.
 * @note No input is modified or retained; the result copies its inputs and no transform is evaluated.
 */
TeX_t *TeX_compose_transform(const TeX_t *const *operands, TeX_transform_kind_t kind);

/**
 * @brief Reuse semantic ordered partial-derivative layout for already constructed notation.
 * @param operands Borrowed non-NULL array containing the body followed by count non-NULL variable values.
 * @param count Positive variable count, no greater than UINT_MAX, with room for the body in size_t.
 * @return Owned notation released with TeX_free(), or NULL with EINVAL for an invalid array or count,
 * or on allocation failure. All values are copied; neither the array nor its members are retained.
 * @note Variables arrive in application order. Layout reverses the denominator order and groups adjacent repetitions.
 */
TeX_t *TeX_compose_partial(const TeX_t *const *operands, size_t count);

/**
 * @brief Compose an integral with optional bounds and an upright differential.
 * @param body Borrowed, non-NULL integrand notation.
 * @param variable Borrowed, non-NULL integration variable.
 * @param lower Borrowed lower bound, or NULL to omit it.
 * @param upper Borrowed upper bound, or NULL to omit it.
 * @return Independently owned notation, released with TeX_free(), or NULL on invalid input or allocation failure.
 * @note Inputs are copied. Nest this constructor for iterated integrals; no integration is evaluated.
 */
TeX_t *TeX_compose_integral(const TeX_t *body, const TeX_t *variable, const TeX_t *lower, const TeX_t *upper);

/**
 * @brief Compose an ordinary or partial derivative fraction of positive integer order.
 * @param body Borrowed, non-NULL differentiated expression.
 * @param variable Borrowed, non-NULL differentiation variable.
 * @param order Positive derivative order; zero is invalid.
 * @param style Ordinary or partial notation selected from TeX_derivative_style_t.
 * @return Independently owned notation, released with TeX_free(), or NULL on invalid input or allocation failure.
 * @note Inputs are copied. This emits notation only; coefficients should be composed beside this fraction.
 */
TeX_t *TeX_compose_derivative(const TeX_t *body, const TeX_t *variable, unsigned order, TeX_derivative_style_t style);

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
TeX_t *TeX_compose_matrix(const TeX_t *const *cells, size_t rows, size_t columns,
                  TeX_matrix_style_t style, unsigned row_spacing);

/**
 * @brief Wrap a mathematical body and optional binding list in braces.
 * @param body Borrowed body; must not be NULL.
 * @param bindings Borrowed bindings, or NULL to omit the binding bar.
 * @return Independently owned notation, released with TeX_free(), or NULL on failure.
 */
TeX_t *TeX_compose_bound(const TeX_t *body, const TeX_t *bindings);

/**
 * @brief Place a scalar factor beside a mathematical body with compact spacing.
 * @param factor Borrowed factor; must not be NULL.
 * @param body Borrowed body; must not be NULL.
 * @return Independently owned notation, released with TeX_free(), or NULL on failure.
 */
TeX_t *TeX_compose_scaled(const TeX_t *factor, const TeX_t *body);

/**
 * @brief Compose aligned real and imaginary components on separate rows.
 * @param real Borrowed real component; must not be NULL.
 * @param imaginary Borrowed imaginary coefficient; must not be NULL.
 * @return Independently owned notation, released with TeX_free(), or NULL on failure.
 */
TeX_t *TeX_compose_cartesian_sum(const TeX_t *real, const TeX_t *imaginary);

/**
 * @brief Compose a symmetric pair with an optional common denominator.
 * @param centre Borrowed common centre; must not be NULL.
 * @param offset Borrowed displacement; must not be NULL.
 * @param denominator Borrowed common denominator, or NULL for none.
 * @param group_offset Whether to enclose the displacement in parentheses.
 * @return Independently owned notation, released with TeX_free(), or NULL on failure.
 */
TeX_t *TeX_compose_symmetric_pair(const TeX_t *centre, const TeX_t *offset,
                          const TeX_t *denominator, bool group_offset);

#endif
