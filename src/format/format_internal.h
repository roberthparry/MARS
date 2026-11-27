/**
 * @file format_internal.h
 * @brief Private compiled-template storage and typed-rendering contracts.
 *
 * Used only by the format implementation to share immutable field metadata, diagnostics and adapter rendering.
 * Callers outside this module use format-t.h. Define MARS_FORMAT_INTERNAL_ACCESS immediately before inclusion.
 */
#ifndef MARS_FORMAT_INTERNAL_H
#define MARS_FORMAT_INTERNAL_H

#if !defined(MARS_FORMAT_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_FORMAT_INTERNAL_ACCESS."
#endif

#include "format-t.h"

/** @brief Private stored argument category used for validation and direct rendering dispatch. */
typedef enum {
    FMT_KIND_NONE,        /**< Invalid or uninitialised descriptor. */
    FMT_KIND_SIGNED,      /**< Signed fundamental integer. */
    FMT_KIND_UNSIGNED,    /**< Unsigned fundamental integer. */
    FMT_KIND_BOOL,        /**< C truth value. */
    FMT_KIND_FLOAT,       /**< C float, retaining its default decimal precision. */
    FMT_KIND_DOUBLE,      /**< C double, retaining its default decimal precision. */
    FMT_KIND_LONG_DOUBLE, /**< C long double without narrowing. */
    FMT_KIND_CSTRING,     /**< Borrowed NUL-terminated UTF-8 text. */
    FMT_KIND_STRING,      /**< Borrowed native string. */
    FMT_KIND_VIEW,        /**< Borrowed native text view. */
    FMT_KIND_RUNE,        /**< Borrowed native user-visible rune. */
    FMT_KIND_POINTER,     /**< Address displayed without dereferencing. */
    FMT_KIND_ADAPTER,     /**< Copied opaque payload with immutable module-owned callbacks. */
    FMT_KIND_CUSTOM,      /**< Borrowed custom object with its renderer. */
    FMT_KIND_COUNT        /**< Number of categories; not a usable argument kind. */
} fmt_kind_t;

/** @brief Private decoded binding; referenced resources remain borrowed from the caller. */
typedef struct {
    const char *name; /**< Borrowed binding name, or NULL for an exclusively positional argument. */
    fmt_kind_t kind; /**< Active payload category. */
    union {
        intmax_t signed_value; /**< Copied signed integer. */
        uintmax_t unsigned_value; /**< Copied unsigned integer. */
        bool boolean; /**< Copied truth value. */
        long double real; /**< Copied ordinary floating-point value; kind retains its original precision. */
        const char *cstring; /**< Borrowed NUL-terminated UTF-8 text; NULL renders as (null). */
        const string_t *string; /**< Borrowed string; NULL renders as (null). */
        string_view_t view; /**< Borrowed view; its backing string must remain alive and unchanged. */
        rune_t rune; /**< Borrowed rune; its backing string must remain alive and unchanged. */
        const void *pointer; /**< Borrowed pointer rendered as an address, never dereferenced. */
        struct {
            const fmt_adapter_t *type; /**< Borrowed immutable adapter, also identifying the argument type. */
            unsigned char data[FMT_ADAPTER_CAPACITY]; /**< Copied opaque bytes decoded by the owning module. */
        } adapter; /**< Module-owned rendering and inline payload. */
        struct {
            const void *value; /**< Borrowed custom object passed to render. */
            fmt_custom_fn render; /**< Required unpadded-core renderer. */
        } custom; /**< Custom object and its renderer. */
    } value; /**< Tagged argument data; callers retain ownership of all referenced resources. */
} fmt_arg_impl_t;

/**
 * @brief Decode a public binding using a byte copy, without aliasing its storage.
 * @param argument Borrowed descriptor created by a constructor or zero-initialised; not retained or modified.
 * @return Private copy with the same borrowed resources; zero storage yields an invalid category. Cannot fail.
 */
fmt_arg_impl_t fmt_arg_unpack(const fmt_arg_t *argument);

/** @brief Owned copy of one argument's compile-time name and category. */
typedef struct {
    string_t *name; /**< Owned exact UTF-8 name, or NULL when unnamed. */
    fmt_kind_t kind; /**< Required render-time category. */
    const fmt_adapter_t *adapter; /**< Borrowed immutable adapter identity, or NULL for built-in categories. */
} fmt_layout_t;

/** @brief One literal span or directly indexed replacement field. */
typedef struct {
    size_t offset; /**< Byte offset of the literal or opening brace in the owned template. */
    size_t length; /**< Literal byte count; zero for fields. */
    size_t argument; /**< Argument index for fields, SIZE_MAX for literal spans. */
    fmt_options_t options; /**< Parsed options; presentation borrows the storage below. */
    string_t *presentation; /**< Owned presentation name for fields; NULL for literals. */
} fmt_part_t;

/** @brief Private immutable template representation after successful compilation. */
struct _fmt_t {
    string_t *text; /**< Owned exact UTF-8 template. */
    fmt_part_t *parts; /**< Owned contiguous literal/field sequence. */
    size_t count; /**< Number of initialised parts. */
    size_t capacity; /**< Allocated part capacity while compiling. */
    fmt_layout_t *layout; /**< Owned argument-name/type array. */
    size_t arguments; /**< Required number of render-time arguments. */
};

/**
 * @brief Clear the current thread's error and optionally copy the successful record to a caller.
 * @param[out] error Optional diagnostic destination; no pointers are retained. Reentrant across separate threads.
 */
void fmt_error_clear(fmt_error_t *error);

/**
 * @brief Publish a failure to thread-local storage and an optional caller destination.
 * @param[out] error Optional caller-owned diagnostic destination.
 * @param code Failure category.
 * @param offset Template byte offset, or SIZE_MAX.
 * @param argument Argument index, or SIZE_MAX.
 * @param message Static diagnostic, retained by reference and never freed.
 * @return False, allowing a checked operation to return its failure directly. Allocates no storage.
 */
bool fmt_error_set(fmt_error_t *error, fmt_code_t code, size_t offset, size_t argument, const char *message);

/**
 * @brief Copy exact UTF-8 from a C boundary without normalising its spelling.
 * @param text Borrowed NUL-terminated input, required.
 * @return Owned string released with string_free(), or NULL for invalid UTF-8, NULL input or allocation failure.
 * @note Failure publishes a thread-local diagnostic; successful copying does not replace an existing diagnostic.
 */
string_t *fmt_copy_text(const char *text);

/**
 * @brief Check presentation and option compatibility before any values are rendered.
 * @param layout Borrowed category and optional adapter identity captured at compilation.
 * @param options Borrowed parsed field options.
 * @param offset Template opening-brace offset for diagnostics.
 * @param argument Argument index for diagnostics.
 * @param[out] error Optional diagnostic destination.
 * @return True when compatible; false with a published diagnostic otherwise. Retains no inputs.
 */
bool fmt_validate(const fmt_layout_t *layout, const fmt_options_t *options, size_t offset, size_t argument, fmt_error_t *error);

/**
 * @brief Render and pad one field using directly indexed native dispatch.
 * @param argument Borrowed typed value, not retained or modified.
 * @param part Borrowed compiled field metadata.
 * @param[out] error Optional diagnostic destination.
 * @return Owned string released with string_free(), or NULL on type, value, range or allocation failure.
 */
string_t *fmt_render_field(const fmt_arg_impl_t *argument, const fmt_part_t *part, fmt_error_t *error);

#endif
