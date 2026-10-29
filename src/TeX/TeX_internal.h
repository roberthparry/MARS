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

#endif
