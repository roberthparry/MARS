#ifndef USTRING_H
#define USTRING_H

#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @file ustring.h
 * @brief Dynamic UTF-8 string type with grapheme-cluster, normalization,
 *        and Unicode-aware operations.
 *
 * ## Ownership model
 *
 * Every function that returns a @c string_t* allocates a new string on the
 * heap. The caller owns the result and must release it with string_free().
 * Functions that take a @c string_t* and modify it do so in place and do
 * not transfer ownership.
 *
 * ## UTF-8 validity
 *
 * All constructors validate their input. Invalid byte sequences are replaced
 * with U+FFFD (Unicode replacement character). Modification functions
 * preserve UTF-8 validity unless explicitly documented otherwise.
 *
 * ## Three levels of text operation
 *
 * This API distinguishes three levels of granularity:
 *
 *  - **Byte level**  — raw byte offsets and lengths (e.g. string_length(),
 *    string_substr()).  Fast, but not Unicode-aware.
 *  - **Codepoint level** — Unicode scalar values (e.g. string_utf8_length(),
 *    string_utf8_reverse()).  Correct for most Latin and CJK text.
 *  - **Grapheme level** — user-perceived characters as defined by Unicode
 *    UAX #29 (e.g. string_grapheme_count(), string_grapheme_at()).
 *    Required for emoji, Indic scripts, and flag sequences.
 */

/* =========================================================================
   Opaque type
   ========================================================================= */

/**
 * @brief Opaque, heap-allocated, dynamic UTF-8 string.
 *
 * Instances must be created with string_new() or string_new_with() and
 * released with string_free(). Do not allocate @c string_t on the stack
 * or embed it in another struct — use the pointer form exclusively.
 */
typedef struct string_t string_t;

/* =========================================================================
   Creation and destruction
   ========================================================================= */

/**
 * @brief Create a new, empty string.
 *
 * The string has zero length and a small initial capacity.
 *
 * @return Newly allocated empty string, or @c NULL on allocation failure.
 */
string_t *string_new(void);

/**
 * @brief Create a new string from a null-terminated C string.
 *
 * The contents of @p init are copied and validated as UTF-8. Invalid byte
 * sequences are replaced with U+FFFD.
 *
 * @param init  Null-terminated UTF-8 C string to copy. Must not be @c NULL.
 * @return      Newly allocated string, or @c NULL on allocation failure.
 */
string_t *string_new_with(const char *init);

/**
 * @brief Deep-copy a string.
 *
 * Creates an independent copy of @p src with its own heap storage. Modifying
 * the clone does not affect the original.
 *
 * @param src  String to clone. Must not be @c NULL.
 * @return     Newly allocated clone, or @c NULL on allocation failure.
 */
string_t *string_clone(const string_t *src);

/**
 * @brief Destroy a string and release all associated memory.
 *
 * After this call the pointer is invalid and must not be used. Passing
 * @c NULL is safe and has no effect.
 *
 * @param s  String to destroy, or @c NULL.
 */
void string_free(string_t *s);

/* =========================================================================
   Access
   ========================================================================= */

/**
 * @brief Return a read-only C string view of the contents.
 *
 * The returned pointer is null-terminated and points directly into the
 * string's internal buffer. It remains valid until the string is modified
 * or freed. Do not store the pointer across mutations.
 *
 * @param s  String to query. Must not be @c NULL.
 * @return   Null-terminated UTF-8 buffer. Never @c NULL.
 */
const char *string_c_str(const string_t *s);

/**
 * @brief Return the byte length of the string.
 *
 * This is the number of bytes in the UTF-8 encoding, not the number of
 * codepoints or grapheme clusters. For those, use string_utf8_length() or
 * string_grapheme_count() respectively.
 *
 * @param s  String to query. Must not be @c NULL.
 * @return   Number of bytes, excluding the null terminator.
 */
size_t string_length(const string_t *s);

/* =========================================================================
   Modification
   ========================================================================= */

/**
 * @brief Clear the string to empty, preserving allocated capacity.
 *
 * After this call string_length() returns 0 and string_c_str() returns an
 * empty string. No reallocation is performed.
 *
 * @param s  String to clear. Must not be @c NULL.
 */
void string_clear(string_t *s);

/**
 * @brief Append a null-terminated UTF-8 C string.
 *
 * @param s       Destination string. Must not be @c NULL.
 * @param suffix  Null-terminated UTF-8 text to append. Must not be @c NULL.
 * @return        0 on success, non-zero on allocation failure.
 */
int string_append_cstr(string_t *s, const char *suffix);

/**
 * @brief Append exactly @p size raw bytes from @p buffer.
 *
 * The bytes are copied verbatim without UTF-8 validation. A null terminator
 * is written after the appended bytes automatically. Capacity is expanded
 * as needed.
 *
 * @param s       Destination string. Must not be @c NULL.
 * @param buffer  Pointer to the bytes to append. Need not be null-terminated.
 * @param size    Number of bytes to append.
 * @return        0 on success, non-zero on allocation failure.
 */
int string_append_chars(string_t *s, const char *buffer, size_t size);

/**
 * @brief Append a single ASCII character.
 *
 * @param s  Destination string. Must not be @c NULL.
 * @param c  ASCII character to append.
 * @return   0 on success, non-zero on allocation failure.
 */
int string_append_char(string_t *s, char c);

/**
 * @brief Insert UTF-8 text at a byte offset.
 *
 * @p pos must lie on a valid UTF-8 codepoint boundary. Inserting at
 * string_length() is equivalent to appending.
 *
 * @param s    Destination string. Must not be @c NULL.
 * @param pos  Byte offset at which to insert. Must be a codepoint boundary.
 * @param text Null-terminated UTF-8 text to insert. Must not be @c NULL.
 * @return     0 on success, non-zero on allocation failure or invalid @p pos.
 */
int string_insert(string_t *s, size_t pos, const char *text);

/**
 * @brief Trim ASCII whitespace from both ends of the string in place.
 *
 * Removes leading and trailing space, tab, newline, carriage return,
 * form feed, and vertical tab characters. Non-ASCII whitespace is not
 * removed.
 *
 * @param s  String to modify. Must not be @c NULL.
 */
void string_trim(string_t *s);

/* =========================================================================
   printf-style append
   ========================================================================= */

/**
 * @brief Append formatted text using printf-style format string.
 *
 * Equivalent to @c sprintf into a temporary buffer followed by
 * string_append_cstr(), but handles sizing automatically.
 *
 * @param s    Destination string. Must not be @c NULL.
 * @param fmt  printf-style format string. Must not be @c NULL.
 * @param ...  Format arguments.
 * @return     Number of bytes appended, or negative on error.
 */
int string_printf(string_t *s, const char *fmt, ...);

/**
 * @brief Append formatted text using a @c va_list.
 *
 * The @c va_list variant of string_printf(). Useful when implementing
 * wrapper functions.
 *
 * @param s    Destination string. Must not be @c NULL.
 * @param fmt  printf-style format string. Must not be @c NULL.
 * @param ap   Argument list (consumed by this call).
 * @return     Number of bytes appended, or negative on error.
 */
int string_vprintf(string_t *s, const char *fmt, va_list ap);

/**
 * @brief Alias for string_printf().
 *
 * @param s    Destination string. Must not be @c NULL.
 * @param fmt  printf-style format string. Must not be @c NULL.
 * @param ...  Format arguments.
 * @return     Number of bytes appended, or negative on error.
 */
int string_append_format(string_t *s, const char *fmt, ...);

/* =========================================================================
   Search and comparison
   ========================================================================= */

/**
 * @brief Signed result type for byte-offset search functions.
 *
 * Used instead of @c ssize_t to avoid a POSIX dependency.
 */
typedef long string_offset_t;

/**
 * @brief Find the first occurrence of a UTF-8 substring.
 *
 * Searches @p s for the first occurrence of @p needle using a
 * bytewise comparison.
 *
 * @param s       String to search. Must not be @c NULL.
 * @param needle  Null-terminated UTF-8 substring to find. Must not be @c NULL.
 * @return        Byte offset of the first match, or -1 if not found.
 */
string_offset_t string_find(const string_t *s, const char *needle);

/**
 * @brief Lexicographically compare two strings by UTF-8 byte value.
 *
 * The comparison is bytewise (not locale-aware or Unicode collation order).
 * For locale-aware comparison, normalize both strings to NFC first.
 *
 * @param a  First string. Must not be @c NULL.
 * @param b  Second string. Must not be @c NULL.
 * @return   Negative if a < b, zero if a == b, positive if a > b.
 */
int string_compare(const string_t *a, const string_t *b);

/**
 * @brief Test whether the string begins with a given prefix.
 *
 * @param s       String to test. Must not be @c NULL.
 * @param prefix  Null-terminated UTF-8 prefix. Must not be @c NULL.
 * @return        @c true if @p s starts with @p prefix, @c false otherwise.
 */
bool string_starts_with(const string_t *s, const char *prefix);

/**
 * @brief Test whether the string ends with a given suffix.
 *
 * @param s       String to test. Must not be @c NULL.
 * @param suffix  Null-terminated UTF-8 suffix. Must not be @c NULL.
 * @return        @c true if @p s ends with @p suffix, @c false otherwise.
 */
bool string_ends_with(const string_t *s, const char *suffix);

/**
 * @brief Extract a substring by byte range.
 *
 * @p pos and @p pos + @p len should lie on codepoint boundaries to avoid
 * splitting a multibyte sequence.
 *
 * @param s    Source string. Must not be @c NULL.
 * @param pos  Starting byte offset.
 * @param len  Number of bytes to extract.
 * @return     Newly allocated substring, or @c NULL on error.
 */
string_t *string_substr(const string_t *s, size_t pos, size_t len);

/**
 * @brief Reverse the string bytewise.
 *
 * @warning This operation corrupts any multibyte UTF-8 sequence. It is only
 * safe for pure ASCII strings. For Unicode-correct reversal use
 * string_utf8_reverse() (codepoint level) or string_grapheme_reverse()
 * (grapheme level).
 *
 * @param s  String to modify. Must not be @c NULL.
 */
void string_reverse(string_t *s);

/* =========================================================================
   Replace
   ========================================================================= */

/**
 * @brief Replace all non-overlapping occurrences of a substring.
 *
 * Replacements are applied left-to-right. The replacement string may be
 * longer, shorter, or the same length as the search string.
 *
 * @param s        String to modify. Must not be @c NULL.
 * @param search   Null-terminated UTF-8 substring to find. Must not be @c NULL.
 * @param replace  Null-terminated UTF-8 replacement text. Must not be @c NULL.
 * @return         Number of replacements made, or negative on allocation failure.
 */
int string_replace(string_t *s, const char *search, const char *replace);

/* =========================================================================
   Split and join
   ========================================================================= */

/**
 * @brief Split a string by a UTF-8 delimiter.
 *
 * Empty tokens (from leading, trailing, or consecutive delimiters) are
 * omitted from the result. The returned array and all strings within it
 * are heap-allocated and must be freed with string_split_free().
 *
 * @param s          Source string. Must not be @c NULL.
 * @param delim      Null-terminated UTF-8 delimiter. Must not be @c NULL.
 * @param out_count  Output: number of elements in the returned array.
 * @return           Array of newly allocated strings, or @c NULL on failure.
 *
 * @see string_split_free()
 */
string_t **string_split(const string_t *s, const char *delim, size_t *out_count);

/**
 * @brief Free an array returned by string_split().
 *
 * Frees each string in @p arr and then the array itself. Passing @c NULL
 * for @p arr or zero for @p count is safe.
 *
 * @param arr    Array of strings returned by string_split().
 * @param count  Number of elements in @p arr.
 */
void string_split_free(string_t **arr, size_t count);

/**
 * @brief Join an array of strings with a separator.
 *
 * Produces a single string formed by concatenating all elements of @p arr
 * with @p sep inserted between consecutive elements.
 *
 * @param arr    Array of strings to join. Must not be @c NULL.
 * @param count  Number of elements in @p arr.
 * @param sep    Null-terminated UTF-8 separator. Must not be @c NULL.
 * @return       Newly allocated joined string, or @c NULL on failure.
 */
string_t *string_join(string_t **arr, size_t count, const char *sep);

/* =========================================================================
   Views
   ========================================================================= */

/**
 * @brief Non-owning, read-only view into a UTF-8 string's storage.
 *
 * A view is valid only as long as the source string is alive and unmodified.
 * Do not store views across mutations of the source string.
 */
typedef struct {
    const char *data; /**< Pointer to the first byte of the view. */
    size_t      len;  /**< Length of the view in bytes. */
} string_view_t;

/**
 * @brief Create a byte-range view into a string.
 *
 * The view points directly into @p s's internal storage and becomes invalid
 * if @p s is modified or freed.
 *
 * @param s    Source string. Must not be @c NULL.
 * @param pos  Starting byte offset.
 * @param len  Number of bytes in the view.
 * @return     View referencing the original storage.
 */
string_view_t string_view(const string_t *s, size_t pos, size_t len);

/**
 * @brief Test whether a view's contents equal a C string.
 *
 * @param v     View to compare. Must not be @c NULL.
 * @param cstr  Null-terminated UTF-8 string to compare against.
 * @return      Non-zero if equal, zero if not.
 */
int string_view_equals(const string_view_t *v, const char *cstr);

/**
 * @brief Create a new string from a view.
 *
 * Copies the bytes referenced by @p v into a newly allocated string.
 *
 * @param v  View to copy. Must not be @c NULL.
 * @return   Newly allocated string, or @c NULL on allocation failure.
 */
string_t *string_from_view(const string_view_t *v);

/**
 * @brief Split a string into non-owning views.
 *
 * Equivalent to string_split() but returns views into the original storage
 * rather than allocating new strings. The views become invalid if @p s is
 * modified or freed. Free the returned array with string_split_view_free().
 *
 * @param s          Source string. Must not be @c NULL.
 * @param delim      Null-terminated UTF-8 delimiter. Must not be @c NULL.
 * @param out_count  Output: number of elements in the returned array.
 * @return           Heap-allocated array of views, or @c NULL on failure.
 *
 * @see string_split_view_free()
 */
string_view_t *string_split_view(const string_t *s, const char *delim, size_t *out_count);

/**
 * @brief Free an array returned by string_split_view().
 *
 * Frees the array itself. The views do not own their data so no string
 * memory is freed. Passing @c NULL is safe.
 *
 * @param views  Array returned by string_split_view().
 */
void string_split_view_free(string_view_t *views);

/* =========================================================================
   UTF-8 codepoint utilities
   ========================================================================= */

/**
 * @brief Advance to the start of the next UTF-8 codepoint.
 *
 * If @p i already points past the end of the buffer, @p len is returned.
 *
 * @param s    Byte buffer. Must not be @c NULL.
 * @param len  Total buffer length in bytes.
 * @param i    Current byte index.
 * @return     Byte index of the next codepoint boundary.
 */
size_t utf8_next(const char *s, size_t len, size_t i);

/**
 * @brief Move to the start of the previous UTF-8 codepoint.
 *
 * @param s    Byte buffer. Must not be @c NULL.
 * @param len  Total buffer length in bytes.
 * @param i    Current byte index (must be > 0).
 * @return     Byte index of the previous codepoint boundary.
 */
size_t string_utf8_prev(const char *s, size_t len, size_t i);

/**
 * @brief Count the number of Unicode codepoints in a string.
 *
 * Counts Unicode scalar values, not bytes or grapheme clusters. A single
 * user-perceived character (grapheme) may comprise multiple codepoints.
 * For grapheme counting use string_grapheme_count().
 *
 * @param s  String to query. Must not be @c NULL.
 * @return   Number of Unicode codepoints.
 */
size_t string_utf8_length(const string_t *s);

/**
 * @brief Reverse the string by Unicode codepoints.
 *
 * Each codepoint is treated as an atomic unit. This is correct for most
 * Latin and CJK text but will produce visually wrong results for strings
 * containing combining marks or emoji sequences. For those, use
 * string_grapheme_reverse().
 *
 * @param s  String to modify. Must not be @c NULL.
 */
void string_utf8_reverse(string_t *s);

/**
 * @brief Convert all codepoints to their Unicode uppercase equivalents.
 *
 * @param s  String to modify. Must not be @c NULL.
 */
void string_utf8_to_upper(string_t *s);

/**
 * @brief Convert all codepoints to their Unicode lowercase equivalents.
 *
 * @param s  String to modify. Must not be @c NULL.
 */
void string_utf8_to_lower(string_t *s);

/* =========================================================================
   Grapheme cluster utilities
   ========================================================================= */

/**
 * @brief Unicode grapheme cluster break classes (UAX #29).
 *
 * Used internally by the grapheme iterator and exposed for callers that
 * need to inspect the break class of individual codepoints.
 */
typedef enum {
    GB_Other,                /**< Any codepoint not in another class.        */
    GB_CR,                   /**< Carriage return (U+000D).                  */
    GB_LF,                   /**< Line feed (U+000A).                        */
    GB_Control,              /**< Other control characters.                  */
    GB_Extend,               /**< Combining marks and enclosing marks.       */
    GB_ZWJ,                  /**< Zero width joiner (U+200D).                */
    GB_Regional_Indicator,   /**< Regional indicator symbols (flags).        */
    GB_Extended_Pictographic,/**< Emoji and pictographic symbols.            */
    GB_SpacingMark           /**< Spacing combining marks (Indic scripts).   */
} grapheme_class_t;

/**
 * @brief Classify a Unicode codepoint into its grapheme cluster break class.
 *
 * Uses binary search over sorted Unicode range tables. The classification
 * follows Unicode Standard Annex #29.
 *
 * @param cp  Unicode codepoint to classify.
 * @return    Grapheme break class of @p cp.
 */
grapheme_class_t string_grapheme_class(uint32_t cp);

/**
 * @brief Advance to the start of the next grapheme cluster.
 *
 * Implements the grapheme cluster boundary rules from Unicode UAX #29,
 * including CR+LF pairs, Extend and SpacingMark sequences (GB9/GB9a),
 * ZWJ emoji sequences (GB11), and regional indicator pairs (GB12/GB13).
 *
 * @param s    Byte buffer. Must not be @c NULL.
 * @param len  Total buffer length in bytes.
 * @param i    Current byte index. Must be a grapheme cluster boundary.
 * @return     Byte index of the next grapheme cluster boundary.
 */
size_t string_grapheme_next(const char *s, size_t len, size_t i);

/**
 * @brief Move to the start of the previous grapheme cluster.
 *
 * Uses forward scanning from a safe anchor point to correctly handle
 * ZWJ sequences and regional indicator pairs — sequences that cannot be
 * reliably decomposed by walking backwards alone.
 *
 * @param s    Byte buffer. Must not be @c NULL.
 * @param len  Total buffer length in bytes.
 * @param i    Current byte index (must be > 0).
 * @return     Byte index of the previous grapheme cluster boundary.
 */
size_t string_grapheme_prev(const char *s, size_t len, size_t i);

/**
 * @brief Count the number of grapheme clusters in a string.
 *
 * A grapheme cluster is the smallest unit of text that a user perceives as
 * a single character. This count may be less than string_utf8_length() when
 * the string contains combining marks, emoji modifier sequences, or ZWJ
 * sequences.
 *
 * @param s  String to query. Must not be @c NULL.
 * @return   Number of grapheme clusters.
 */
size_t string_grapheme_count(const string_t *s);

/**
 * @brief Reverse the string by grapheme clusters.
 *
 * Each grapheme cluster is treated as an atomic unit, preserving the visual
 * integrity of emoji, flag sequences, and combining character sequences.
 * This is the Unicode-correct way to reverse a string for display.
 *
 * @param s  String to modify. Must not be @c NULL.
 */
void string_grapheme_reverse(string_t *s);

/**
 * @brief Extract a substring by grapheme cluster range.
 *
 * Extracts @p glen grapheme clusters starting at grapheme index @p gpos.
 * If @p gpos is beyond the end of the string an empty string is returned.
 * If @p glen extends beyond the end, the available graphemes are returned.
 *
 * @param s     Source string. Must not be @c NULL.
 * @param gpos  Zero-based starting grapheme index.
 * @param glen  Number of grapheme clusters to extract.
 * @return      Newly allocated substring, or @c NULL on allocation failure.
 */
string_t *string_grapheme_substr(const string_t *s, size_t gpos, size_t glen);

/**
 * @brief Extract a single grapheme cluster by index.
 *
 * Convenience wrapper around string_grapheme_substr() for the common case
 * of extracting exactly one grapheme.
 *
 * @param s      Source string. Must not be @c NULL.
 * @param index  Zero-based grapheme cluster index.
 * @return       Newly allocated string containing the grapheme cluster,
 *               or @c NULL on allocation failure or out-of-range @p index.
 */
string_t *string_grapheme_at(const string_t *s, size_t index);

/* =========================================================================
   ASCII case conversion
   ========================================================================= */

/**
 * @brief Convert ASCII letters to uppercase in place.
 *
 * Only the 26 ASCII letters A–Z are affected. Non-ASCII codepoints are
 * left unchanged. For Unicode-aware case conversion use string_utf8_to_upper().
 *
 * @param s  String to modify. Must not be @c NULL.
 */
void string_to_upper(string_t *s);

/**
 * @brief Convert ASCII letters to lowercase in place.
 *
 * Only the 26 ASCII letters a–z are affected. Non-ASCII codepoints are
 * left unchanged. For Unicode-aware case conversion use string_utf8_to_lower().
 *
 * @param s  String to modify. Must not be @c NULL.
 */
void string_to_lower(string_t *s);

/* =========================================================================
   Hashing
   ========================================================================= */

/**
 * @brief Compute a hash of the UTF-8 byte contents.
 *
 * The hash is consistent within a single process run but is not guaranteed
 * to be stable across library versions or platforms. Suitable for use in
 * hash tables but not for persistent storage or cross-process comparison.
 *
 * @param s  String to hash. Must not be @c NULL.
 * @return   Hash value.
 */
unsigned long string_hash(const string_t *s);

/* =========================================================================
   Fixed-capacity buffer
   ========================================================================= */

/**
 * @brief Fixed-capacity string buffer backed by caller-supplied storage.
 *
 * Useful for formatting short strings on the stack without heap allocation.
 * Appends that would exceed @c cap are silently truncated at a valid UTF-8
 * codepoint boundary.
 *
 * ### Example
 * @code
 * char storage[64];
 * string_buffer_t buf;
 * string_buffer_init(&buf, storage, sizeof(storage));
 * string_buffer_append(&buf, "Hello");
 * string_buffer_append_char(&buf, '!');
 * printf("%s\n", buf.data);   // "Hello!"
 * @endcode
 */
typedef struct {
    char  *data; /**< Caller-supplied storage buffer. */
    size_t len;  /**< Current number of bytes written (excluding NUL). */
    size_t cap;  /**< Total capacity of @c data in bytes (including NUL). */
} string_buffer_t;

/**
 * @brief Initialise a fixed-capacity buffer over caller-supplied storage.
 *
 * @p storage must remain valid for the lifetime of @p b. The buffer is
 * initialised to empty (zero length, NUL-terminated).
 *
 * @param b         Buffer to initialise. Must not be @c NULL.
 * @param storage   Backing storage array. Must not be @c NULL.
 * @param capacity  Size of @p storage in bytes (must be >= 1).
 */
void string_buffer_init(string_buffer_t *b, char *storage, size_t capacity);

/**
 * @brief Append a null-terminated string to a fixed-capacity buffer.
 *
 * If the text does not fit, as much as possible is appended up to a valid
 * UTF-8 codepoint boundary and the buffer is NUL-terminated.
 *
 * @param b     Destination buffer. Must not be @c NULL.
 * @param text  Null-terminated text to append. Must not be @c NULL.
 * @return      0 if the full text was appended, non-zero if truncated.
 */
int string_buffer_append(string_buffer_t *b, const char *text);

/**
 * @brief Append a single ASCII character to a fixed-capacity buffer.
 *
 * @param b  Destination buffer. Must not be @c NULL.
 * @param c  Character to append.
 * @return   0 on success, non-zero if the buffer is full.
 */
int string_buffer_append_char(string_buffer_t *b, char c);

/* =========================================================================
   Builder API
   ========================================================================= */

/**
 * @brief Dynamic string builder (alias for string_t).
 *
 * @c string_builder_t is a @c typedef for @c string_t. It is provided as a
 * semantic marker to indicate that a string is being used in an incremental
 * construction pattern. A builder and a @c string_t are interchangeable —
 * the builder @e is the finished string; there is no separate "finish" step.
 *
 * ### Example
 * @code
 * string_builder_t *b = string_builder_new();
 * string_builder_append(b, "Hello");
 * string_builder_append(b, ", ");
 * string_builder_format(b, "%s!", "world");
 * printf("%s\n", string_c_str(b));  // "Hello, world!"
 * string_builder_free(b);
 * @endcode
 */
typedef string_t string_builder_t;

/**
 * @brief Create a new, empty builder.
 * @return Newly allocated builder, or @c NULL on allocation failure.
 */
static inline string_builder_t *string_builder_new(void) { return string_new(); }

/**
 * @brief Free a builder and all associated memory.
 * @param b  Builder to free, or @c NULL.
 */
static inline void string_builder_free(string_builder_t *b) { string_free(b); }

/**
 * @brief Append a null-terminated C string to the builder.
 * @param b  Builder. Must not be @c NULL.
 * @param s  Text to append. Must not be @c NULL.
 * @return   0 on success, non-zero on allocation failure.
 */
static inline int string_builder_append(string_builder_t *b, const char *s) { return string_append_cstr(b, s); }

/**
 * @brief Append a single ASCII character to the builder.
 * @param b  Builder. Must not be @c NULL.
 * @param c  Character to append.
 * @return   0 on success, non-zero on allocation failure.
 */
static inline int string_builder_append_char(string_builder_t *b, char c) { return string_append_char(b, c); }

/**
 * @brief Append formatted text to the builder (printf-style).
 * @param b    Builder. Must not be @c NULL.
 * @param fmt  printf-style format string. Must not be @c NULL.
 * @param ...  Format arguments.
 * @return     Number of bytes appended, or negative on error.
 */
static inline int string_builder_format(string_builder_t *b, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = string_vprintf(b, fmt, ap);
    va_end(ap);
    return r;
}

/* =========================================================================
   Unicode normalisation
   ========================================================================= */

/**
 * @brief Unicode normalisation forms (UAX #15).
 *
 * Unicode text can be represented in multiple canonically equivalent ways.
 * Normalisation transforms text into a predictable, stable form so that
 * comparison, searching, and storage behave consistently.
 *
 * The four standard forms combine two axes:
 *
 *  | Form  | Decomposition  | Composition |
 *  |-------|----------------|-------------|
 *  | NFD   | Canonical      | No          |
 *  | NFC   | Canonical      | Yes         |
 *  | NFKD  | Compatibility  | No          |
 *  | NFKC  | Compatibility  | Yes         |
 *
 * **Canonical** forms preserve meaning exactly. **Compatibility** forms fold
 * stylistic distinctions (e.g. superscripts, circled digits, ligatures).
 * **Decomposition** expands characters into constituent parts; **Composition**
 * recombines them into the preferred composed form.
 *
 * NFC is the recommended form for storage and interchange.
 */
typedef enum {
    /**
     * @brief Canonical Decomposition followed by Canonical Composition.
     *
     * The most widely used normalisation form. Produces the preferred
     * composed representation of canonically equivalent text.
     *
     * Example: "e" + combining acute → "é"
     */
    STRING_NORM_NFC,

    /**
     * @brief Canonical Decomposition only.
     *
     * Expands characters into their canonical decomposed sequences.
     * Useful for accent stripping and low-level text processing.
     *
     * Example: "é" → "e" + combining acute
     */
    STRING_NORM_NFD,

    /**
     * @brief Compatibility Decomposition followed by Canonical Composition.
     *
     * Folds stylistic and compatibility variants before composing. Often
     * used for security normalisation and search.
     *
     * Examples: "①" → "1",  "ﬀ" → "ff"
     */
    STRING_NORM_NFKC,

    /**
     * @brief Compatibility Decomposition only.
     *
     * Fully decomposes text using compatibility mappings. Useful for
     * indexing and text analysis where stylistic distinctions should
     * be ignored.
     *
     * Examples: "①" → "1",  "é" → "e" + combining acute,  "ﬀ" → "f" + "f"
     */
    STRING_NORM_NFKD

} string_norm_form_t;

/**
 * @brief Normalise a string in place to the specified Unicode form.
 *
 * The string is modified in place. If normalisation produces a longer
 * byte sequence (e.g. NFD decomposition), the internal buffer is grown
 * automatically.
 *
 * When built without @c HAVE_UNISTRING, normalisation is a no-op that
 * returns 0 (success) for NFC/NFD/NFKC/NFKD and -1 for unrecognised forms.
 *
 * @param s     String to normalise. Must not be @c NULL.
 * @param form  Target normalisation form.
 * @return      0 on success, non-zero on error or unrecognised @p form.
 */
int string_normalize(string_t *s, string_norm_form_t form);

#endif /* USTRING_H */