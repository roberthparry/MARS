#ifndef USTRING_H
#define USTRING_H

#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>

/**
 * @brief Opaque dynamic UTF‑8 string type.
 *
 * Instances are heap‑allocated and must be released with string_free().
 * All functions preserve UTF‑8 validity unless explicitly documented.
 */
typedef struct string_t string_t;

/* ------------------------------------------------------------------------- */
/* Creation / destruction                                                    */
/* ------------------------------------------------------------------------- */

/**
 * @brief Create an empty string.
 *
 * @return Newly allocated string, or NULL on allocation failure.
 */
string_t *string_new(void);

/**
 * @brief Create a new string initialized with a C string.
 *
 * @param init Null‑terminated UTF‑8 string to copy.
 * @return Newly allocated string, or NULL on allocation failure.
 */
string_t *string_new_with(const char *init);

/**
 * @brief Deep copy a string.
 *
 * @param src String to clone.
 * @return Newly allocated clone of src, or NULL on allocation failure.
 */
string_t *string_clone(const string_t *src);

/**
 * @brief Destroy a string and free all associated memory.
 *
 * @param s String to destroy (may be NULL).
 */
void string_free(string_t *s);

/* ------------------------------------------------------------------------- */
/* Access                                                                    */
/* ------------------------------------------------------------------------- */

/**
 * @brief Get a read‑only C string view of the contents.
 *
 * The returned pointer remains valid until the string is modified.
 *
 * @param s String to query.
 * @return Null‑terminated UTF‑8 buffer.
 */
const char *string_c_str(const string_t *s);

/**
 * @brief Get the number of bytes in the string (not codepoints).
 *
 * @param s String to query.
 * @return Byte length excluding the null terminator.
 */
size_t string_length(const string_t *s);

/* ------------------------------------------------------------------------- */
/* Modification                                                              */
/* ------------------------------------------------------------------------- */

/**
 * @brief Clear the string to empty.
 *
 * Capacity is preserved.
 *
 * @param s String to modify.
 */
void string_clear(string_t *s);

/**
 * @brief Append a UTF‑8 C string.
 *
 * @param s      Destination string.
 * @param suffix Null‑terminated UTF‑8 text to append.
 * @return 0 on success, non‑zero on allocation failure.
 */
int string_append_cstr(string_t *s, const char *suffix);

/**
 * @brief Append raw bytes to the string.
 *
 * Copies exactly @p size bytes from @p buffer into the end of the string.
 * The bytes are treated as UTF‑8 data but are not validated. A null
 * terminator is appended automatically after the copied bytes.
 *
 * Capacity is expanded as needed.
 *
 * @param s      Destination string.
 * @param buffer Pointer to the bytes to append (need not be null‑terminated).
 * @param size   Number of bytes to append.
 *
 * @return 0 on success, non‑zero on allocation failure.
 */
int string_append_chars(string_t *s, const char *buffer, size_t size);

/**
 * @brief Append a single ASCII character.
 *
 * @param s Destination string.
 * @param c Character to append.
 * @return 0 on success, non‑zero on allocation failure.
 */
int string_append_char(string_t *s, char c);

/**
 * @brief Insert UTF‑8 text at a byte position.
 *
 * @param s    Destination string.
 * @param pos  Byte offset at which to insert.
 * @param text Null‑terminated UTF‑8 text.
 * @return 0 on success, non‑zero on allocation failure or invalid position.
 */
int string_insert(string_t *s, size_t pos, const char *text);

/**
 * @brief Trim ASCII whitespace from both ends.
 *
 * @param s String to modify.
 */
void string_trim(string_t *s);

/* ------------------------------------------------------------------------- */
/* printf‑style append                                                       */
/* ------------------------------------------------------------------------- */

/**
 * @brief Append formatted text (printf‑style).
 *
 * @param s   Destination string.
 * @param fmt Format string.
 * @return Number of bytes appended, or negative on error.
 */
int string_printf(string_t *s, const char *fmt, ...);

/**
 * @brief Append formatted text using a va_list.
 *
 * @param s  Destination string.
 * @param fmt Format string.
 * @param ap  Argument list.
 * @return Number of bytes appended, or negative on error.
 */
int string_vprintf(string_t *s, const char *fmt, va_list ap);

/**
 * @brief Append formatted text (alias of string_printf()).
 *
 * @param s   Destination string.
 * @param fmt Format string.
 * @return Number of bytes appended, or negative on error.
 */
int string_append_format(string_t *s, const char *fmt, ...);

/* ------------------------------------------------------------------------- */
/* Search / compare                                                          */
/* ------------------------------------------------------------------------- */

/**
 * @brief Signed size type for search results.
 */
typedef long ssize_t;

/**
 * @brief Find the first occurrence of a UTF‑8 substring.
 *
 * @param s      String to search.
 * @param needle UTF‑8 substring to locate.
 * @return Byte index of the match, or -1 if not found.
 */
ssize_t string_find(const string_t *s, const char *needle);

/**
 * @brief Lexicographically compare two strings (UTF‑8 bytewise).
 *
 * @param a First string.
 * @param b Second string.
 * @return <0, 0, >0 as in strcmp().
 */
int string_compare(const string_t *a, const string_t *b);

/**
 * @brief Check whether the string begins with a prefix.
 *
 * @param s      String to test.
 * @param prefix UTF‑8 prefix.
 * @return true if s starts with prefix.
 */
bool string_starts_with(const string_t *s, const char *prefix);

/**
 * @brief Check whether the string ends with a suffix.
 *
 * @param s      String to test.
 * @param suffix UTF‑8 suffix.
 * @return true if s ends with suffix.
 */
bool string_ends_with(const string_t *s, const char *suffix);

/**
 * @brief Extract a substring by byte range.
 *
 * @param s   Source string.
 * @param pos Starting byte offset.
 * @param len Number of bytes to copy.
 * @return Newly allocated substring, or NULL on error.
 */
string_t *string_substr(const string_t *s, size_t pos, size_t len);

/**
 * @brief Reverse the string by bytes (not UTF‑8 aware).
 *
 * @param s String to modify.
 */
void string_reverse(string_t *s);

/* ------------------------------------------------------------------------- */
/* Replace                                                                   */
/* ------------------------------------------------------------------------- */

/**
 * @brief Replace all occurrences of a substring.
 *
 * @param s       String to modify.
 * @param search  UTF‑8 substring to replace.
 * @param replace Replacement text.
 * @return Number of replacements made, or negative on error.
 */
int string_replace(string_t *s, const char *search, const char *replace);

/* ------------------------------------------------------------------------- */
/* Split / join                                                              */
/* ------------------------------------------------------------------------- */

/**
 * @brief Split a string by a UTF‑8 delimiter.
 *
 * @param s         Source string.
 * @param delim     UTF‑8 delimiter.
 * @param out_count Output: number of elements.
 * @return Array of newly allocated strings; free with string_split_free().
 */
string_t **string_split(const string_t *s, const char *delim, size_t *out_count);

/**
 * @brief Free an array returned by string_split().
 *
 * @param arr   Array of strings.
 * @param count Number of elements.
 */
void string_split_free(string_t **arr, size_t count);

/**
 * @brief Join an array of strings with a separator.
 *
 * @param arr   Array of strings.
 * @param count Number of elements.
 * @param sep   UTF‑8 separator.
 * @return Newly allocated joined string.
 */
string_t *string_join(string_t **arr, size_t count, const char *sep);

/* ------------------------------------------------------------------------- */
/* Views                                                                     */
/* ------------------------------------------------------------------------- */

/**
 * @brief Non‑owning view into a UTF‑8 string.
 */
typedef struct {
    const char *data; /**< Pointer to the first byte. */
    size_t      len;  /**< Length in bytes. */
} string_view_t;

/**
 * @brief Create a byte‑range view into a string.
 *
 * @param s   Source string.
 * @param pos Starting byte offset.
 * @param len Number of bytes.
 * @return View referencing the original storage.
 */
string_view_t string_view(const string_t *s, size_t pos, size_t len);

/**
 * @brief Compare a view with a C string.
 *
 * @param v    View to compare.
 * @param cstr Null‑terminated UTF‑8 string.
 * @return true if equal.
 */
int string_view_equals(const string_view_t *v, const char *cstr);

/**
 * @brief Create a new string from a view.
 *
 * @param v View to copy.
 * @return Newly allocated string.
 */
string_t *string_from_view(const string_view_t *v);

/**
 * @brief Split into non‑owning views.
 *
 * @param s         Source string.
 * @param delim     UTF‑8 delimiter.
 * @param out_count Output count.
 * @return Array of views; free with string_split_view_free().
 */
string_view_t *string_split_view(const string_t *s, const char *delim, size_t *out_count);

/**
 * @brief Free an array returned by string_split_view().
 *
 * @param views Array of views.
 */
void string_split_view_free(string_view_t *views);

/* ------------------------------------------------------------------------- */
/* UTF‑8 codepoint utilities                                                 */
/* ------------------------------------------------------------------------- */

/**
 * @brief Advance to the next UTF‑8 codepoint boundary.
 *
 * @param s   Byte buffer.
 * @param len Total buffer length.
 * @param i   Current byte index.
 * @return Index of the next codepoint boundary.
 */
size_t utf8_next(const char *s, size_t len, size_t i);

/**
 * @brief Move to the previous UTF‑8 codepoint boundary.
 */
size_t string_utf8_prev(const char *s, size_t len, size_t i);

/**
 * @brief Count UTF‑8 codepoints in a string.
 *
 * @param s String to query.
 * @return Number of Unicode scalar values.
 */
size_t string_utf8_length(const string_t *s);

/**
 * @brief Reverse the string by Unicode codepoints.
 *
 * @param s String to modify.
 */
void string_utf8_reverse(string_t *s);

/**
 * @brief Convert all codepoints to uppercase (Unicode aware).
 */
void string_utf8_to_upper(string_t *s);

/**
 * @brief Convert all codepoints to lowercase (Unicode aware).
 */
void string_utf8_to_lower(string_t *s);

/* ------------------------------------------------------------------------- */
/* Grapheme cluster utilities                                                */
/* ------------------------------------------------------------------------- */

/**
 * @brief Unicode grapheme break classes.
 */
typedef enum {
    GB_Other,
    GB_CR,
    GB_LF,
    GB_Control,
    GB_Extend,
    GB_ZWJ,
    GB_Regional_Indicator,
    GB_Extended_Pictographic,
    GB_SpacingMark
} grapheme_class_t;

/**
 * @brief Advance to the next grapheme cluster boundary.
 */
size_t utf8_grapheme_next(const char *s, size_t len, size_t i);

/**
 * @brief Move to the previous grapheme cluster boundary.
 */
size_t utf8_grapheme_prev(const char *s, size_t len, size_t i);

/**
 * @brief Count grapheme clusters in a string.
 */
size_t utf8_grapheme_count(const string_t *s);

/**
 * @brief Reverse the string by grapheme clusters.
 */
void string_utf8_grapheme_reverse(string_t *s);

/**
 * @brief Extract a substring by grapheme cluster range.
 *
 * @param s    Source string.
 * @param gpos Starting grapheme index.
 * @param glen Number of graphemes.
 * @return Newly allocated substring.
 */
string_t *string_utf8_grapheme_substr(const string_t *s, size_t gpos, size_t glen);

/* ------------------------------------------------------------------------- */
/* ASCII case conversion                                                     */
/* ------------------------------------------------------------------------- */

/**
 * @brief Convert ASCII letters to uppercase.
 */
void string_to_upper(string_t *s);

/**
 * @brief Convert ASCII letters to lowercase.
 */
void string_to_lower(string_t *s);

/* ------------------------------------------------------------------------- */
/* Hashing                                                                   */
/* ------------------------------------------------------------------------- */

/**
 * @brief Compute a hash of the UTF‑8 contents.
 *
 * @param s String to hash.
 * @return Hash value.
 */
unsigned long string_hash(const string_t *s);

/* ------------------------------------------------------------------------- */
/* Fixed‑capacity buffer                                                     */
/* ------------------------------------------------------------------------- */

/**
 * @brief Fixed‑capacity string buffer (no dynamic allocation).
 */
typedef struct {
    char  *data; /**< Storage buffer. */
    size_t len;  /**< Current length. */
    size_t cap;  /**< Total capacity. */
} string_buffer_t;

/**
 * @brief Initialize a fixed‑capacity buffer.
 *
 * @param b        Buffer to initialize.
 * @param storage  Backing storage.
 * @param capacity Size of storage in bytes.
 */
void string_buffer_init(string_buffer_t *b, char *storage, size_t capacity);

/**
 * @brief Append text to a fixed buffer.
 *
 * @return 0 on success, non‑zero if capacity exceeded.
 */
int string_buffer_append(string_buffer_t *b, const char *text);

/**
 * @brief Append a single character to a fixed buffer.
 *
 * @return 0 on success, non‑zero if capacity exceeded.
 */
int string_buffer_append_char(string_buffer_t *b, char c);

/* ------------------------------------------------------------------------- */
/* Builder API                                                               */
/* ------------------------------------------------------------------------- */

/**
 * @brief Alias for string_t used as a builder.
 */
typedef string_t string_builder_t;

/** @brief Create a new builder. */
static inline string_builder_t *string_builder_new(void) { return string_new(); }

/** @brief Free a builder. */
static inline void string_builder_free(string_builder_t *b) { string_free(b); }

/** @brief Append text to a builder. */
static inline int string_builder_append(string_builder_t *b, const char *s) { return string_append_cstr(b, s); }

/** @brief Append a character to a builder. */
static inline int string_builder_append_char(string_builder_t *b, char c) { return string_append_char(b, c); }

/**
 * @brief Append formatted text to a builder.
 */
static inline int string_builder_format(string_builder_t *b, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int r = string_vprintf(b, fmt, ap);
    va_end(ap);
    return r;
}

/* ------------------------------------------------------------------------- */
/* Normalization                                                             */
/* ------------------------------------------------------------------------- */

/**
 * @brief Unicode normalization forms as defined in UAX #15.
 *
 * Unicode text can be represented in multiple canonically equivalent ways.
 * Normalization transforms text into a predictable, stable representation
 * so that string comparison, searching, and storage behave consistently.
 *
 * The four standard forms fall into two axes:
 *
 *   - Canonical vs. Compatibility
 *   - Decomposition vs. Composition
 *
 * Canonical forms preserve meaning exactly. Compatibility forms may fold
 * distinctions that Unicode considers stylistic (e.g., superscripts,
 * circled letters, ligatures).
 *
 * Decomposition breaks characters into their constituent parts (e.g.,
 * "é" → "e" + "◌́"), while composition recombines them into the preferred
 * composed form when possible.
 */
typedef enum {
    /**
     * @brief Canonical Composition (NFC).
     *
     * Produces the preferred composed form of canonically equivalent text.
     * This is the most widely used form for storage, interchange, and
     * comparison. For example:
     *
     *   - "e" + "◌́" → "é"
     *
     * NFC preserves meaning exactly and avoids compatibility folding.
     */
    STRING_NORM_NFC,

    /**
     * @brief Canonical Decomposition (NFD).
     *
     * Expands characters into their canonical decomposed sequences.
     * This is useful for low‑level text processing, accent stripping,
     * and algorithms that operate on combining marks.
     *
     *   - "é" → "e" + "◌́"
     */
    STRING_NORM_NFD,

    /**
     * @brief Compatibility Composition (NFKC).
     *
     * Applies compatibility decomposition (which may remove stylistic
     * distinctions) and then recomposes where possible. This is often
     * used for security, search, and user input normalization.
     *
     * Examples of compatibility folding:
     *   - "①" (circled 1) → "1"
     *   - "ﬀ" (ligature) → "ff"
     */
    STRING_NORM_NFKC,

    /**
     * @brief Compatibility Decomposition (NFKD).
     *
     * Fully decomposes characters using compatibility mappings.
     * This form exposes the underlying structure of text and is useful
     * for tokenization, indexing, and text analysis.
     *
     *   - "①" → "1"
     *   - "é" → "e" + "◌́"
     *   - "ﬀ" → "f" + "f"
     */
    STRING_NORM_NFKD
} string_norm_form_t;


/**
 * @brief Normalize the string in place.
 *
 * @param s    String to modify.
 * @param form Normalization form.
 * @return 0 on success, non‑zero on error or unsupported form.
 */
int string_normalize(string_t *s, string_norm_form_t form);

#endif
