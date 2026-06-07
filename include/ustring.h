#ifndef USTRING_H
#define USTRING_H

#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @file ustring.h
 * @brief Dynamic text string type with Unicode-aware operations.
 *
 * ## Ownership model
 *
 * Every function that returns a @c string_t* allocates a new string on the
 * heap. The caller owns the result and must release it with string_free().
 * Functions that take a @c string_t* and modify it do so in place and do
 * not transfer ownership.
 *
 * ## Text model
 *
 * @c string_t stores ordinary text that can be read or typed by a user. The
 * simple character functions (string_count(), string_at(),
 * string_substring(), string_each(), string_reverse()) treat combined text,
 * accents and emoji sequences as single characters. Low-level byte and UTF-8
 * details are handled inside the implementation.
 *
 * Constructors validate UTF-8 input. Invalid byte sequences are replaced with
 * U+FFFD (Unicode replacement character). Modification functions preserve
 * valid text unless explicitly documented otherwise.
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
typedef struct _string_t string_t;
typedef struct _string_cursor_t string_cursor_t;

/**
 * @brief Position inside a string cursor's source text.
 *
 * Values are produced and consumed by string cursor APIs. They should not be
 * interpreted as character indexes.
 */
typedef size_t string_pos_t;

/**
 * @brief Stack value for one user-visible text rune.
 *
 * A rune may contain more than one storage byte and may be more complex than
 * a C @c char. Treat this as a lightweight value returned by string_at().
 * It remains valid while the source string is alive and unmodified.
 */
typedef struct _rune_t {
    uintptr_t _opaque[3];
} rune_t;

/**
 * @brief Lightweight, non-owning view over part of a string.
 *
 * A view is a passive slice. It remains valid while the source string is alive
 * and unmodified.
 */
typedef struct string_view_t {
    uintptr_t _opaque[2];
} string_view_t;

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
 * @brief Export a transient, read-only C string view of the contents.
 *
 * This is an interoperability escape hatch for APIs that still require
 * NUL-terminated C strings. Callers must not infer anything about the
 * storage strategy from the returned pointer: it may be an internal buffer,
 * a cached materialisation, or another implementation detail. It remains
 * valid only until @p s is modified or freed.
 *
 * @param s  String to query. Must not be @c NULL.
 * @return   Null-terminated UTF-8 buffer. Never @c NULL.
 */
const char *string_c_str(const string_t *s);

/**
 * @brief Return the number of user-visible characters in the string.
 *
 * This is the ordinary character count users expect for readable text:
 * accented letters and emoji sequences are each counted as one character.
 *
 * @param s  String to query. Must not be @c NULL.
 * @return   Number of user-visible characters.
 */
size_t string_count(const string_t *s);

/**
 * @brief Return the storage length in bytes.
 *
 * Most callers should use string_count() for text. This low-level function is
 * useful when interoperating with byte-oriented APIs or allocating storage.
 *
 * @param s  String to query. Must not be @c NULL.
 * @return   Number of storage bytes, excluding the null terminator.
 */
size_t string_length(const string_t *s);

/* =========================================================================
   Modification
   ========================================================================= */

/**
 * @brief Clear the string to empty, preserving allocated capacity.
 *
 * After this call string_count() and string_length() return 0 and string_c_str() returns an
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
 * @brief Append another string.
 *
 * @param s       Destination string. Must not be @c NULL.
 * @param suffix  String to append. Must not be @c NULL.
 * @return        0 on success, non-zero on allocation failure.
 */
int string_append_string(string_t *s, const string_t *suffix);

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
 * @brief Insert text at a character index.
 *
 * Inserting at string_count() is equivalent to appending. Values beyond the
 * end are clamped to the end.
 *
 * @param s    Destination string. Must not be @c NULL.
 * @param pos  Character index at which to insert.
 * @param text Null-terminated UTF-8 text to insert. Must not be @c NULL.
 * @return     0 on success, non-zero on allocation failure.
 */
int string_insert(string_t *s, size_t pos, const char *text);

/**
 * @brief Insert another string at a character index.
 *
 * Inserting at string_count() is equivalent to appending. Values beyond the
 * end are clamped to the end.
 *
 * @param s    Destination string. Must not be @c NULL.
 * @param pos  Character index at which to insert.
 * @param text String to insert. Must not be @c NULL.
 * @return     0 on success, non-zero on allocation failure.
 */
int string_insert_string(string_t *s, size_t pos, const string_t *text);

/**
 * @brief Extract a substring by user-visible character range.
 *
 * @param s      Source string. Must not be @c NULL.
 * @param start  Zero-based starting character index.
 * @param count  Number of characters to extract.
 * @return       Newly allocated substring, or @c NULL on allocation failure.
 */
string_t *string_substring(const string_t *s, size_t start, size_t count);

/**
 * @brief Extract a single user-visible rune by index.
 *
 * @param s      Source string. Must not be @c NULL.
 * @param index  Zero-based character index.
 * @return       Rune value, or an empty rune for out-of-range @p index.
 */
rune_t string_at(const string_t *s, size_t index);

/**
 * @brief Test whether a rune value is empty.
 *
 * Empty values are returned for out-of-range access.
 *
 * @param rune  Rune value to inspect.
 * @return    @c true if empty, @c false otherwise.
 */
bool rune_is_empty(rune_t rune);

/**
 * @brief Create a stack rune from one ASCII character.
 *
 * This does not allocate and does not borrow from a string. Non-ASCII input
 * returns an empty rune; use string_at() or a string cursor for general text.
 *
 * @param c  ASCII character to wrap.
 * @return   Stack rune representing @p c, or an empty rune for non-ASCII.
 */
rune_t rune_from_ascii(char c);

/**
 * @brief Return the numeric value represented by a rune.
 *
 * This is useful for recognising typed mathematical symbols such as
 * @c √, @c π, superscripts, and other non-ASCII input without exposing string
 * storage. Empty runes return 0.
 *
 * @param rune  Rune to inspect.
 * @return      Numeric rune value, or 0 for an empty rune.
 */
uint32_t rune_value(rune_t rune);

/**
 * @brief Return a rune as one ASCII character when possible.
 *
 * This succeeds only when @p rune is exactly one ASCII character. It lets
 * callers recognise ordinary syntax characters without exposing string
 * storage.
 *
 * @param rune  Rune value to inspect.
 * @param out   Destination for the ASCII character. May be @c NULL.
 * @return      @c true if @p rune is a single ASCII character.
 */
bool rune_to_ascii(rune_t rune, char *out);

/**
 * @brief Test whether a rune is exactly one ASCII character.
 *
 * @param rune  Rune value to inspect.
 * @param ch    ASCII character to compare against.
 * @return      @c true when @p rune is exactly @p ch.
 */
bool rune_is_equal(const rune_t rune, char ch);

/**
 * @brief Test whether a rune is a decimal digit.
 *
 * Uses Unicode character properties when available; otherwise recognises
 * ASCII digits and a small set of common Unicode decimal digit ranges.
 *
 * @param rune  Rune value to inspect.
 * @return      @c true when @p rune is a decimal digit.
 */
bool rune_is_digit(rune_t rune);

/**
 * @brief Test whether a rune is alphabetic or a decimal digit.
 *
 * Uses Unicode character properties when available; otherwise recognises
 * ASCII letters/digits and common alphabetic ranges used in mathematical
 * input.
 *
 * @param rune  Rune value to inspect.
 * @return      @c true when @p rune is alphabetic or a decimal digit.
 */
bool rune_is_alpha_numeric(rune_t rune);

/**
 * @brief Copy a rune into a new string.
 *
 * @param rune  Rune value to copy.
 * @return    Newly allocated string, or @c NULL for empty/allocation failure.
 */
string_t *rune_to_string(rune_t rune);

/**
 * @brief Append one rune value to a string.
 *
 * @param s     Destination string. Must not be @c NULL.
 * @param rune  Rune value to append. Must not be empty.
 * @return      0 on success, non-zero on error.
 */
int string_append_rune(string_t *s, rune_t rune);

/**
 * @brief Callback used by string_each().
 *
 * Return 0 to continue iteration, or non-zero to stop. The @p rune is a
 * stack value valid for the duration of the callback.
 */
typedef int (*string_each_fn)(rune_t rune,
                              size_t index,
                              void *user);

/**
 * @brief Iterate over user-visible characters in order.
 *
 * Each callback receives one @c rune_t. Copy it with rune_to_string() if it
 * needs to keep the rune beyond the current source string lifetime.
 *
 * @param s     Source string. Must not be @c NULL.
 * @param fn    Callback to invoke for each character. Must not be @c NULL.
 * @param user  Caller data passed through to @p fn.
 * @return      0 after full iteration, callback return value if it stops,
 *              or -1 on bad arguments.
 */
int string_each(const string_t *s, string_each_fn fn, void *user);

/* =========================================================================
   Borrowed views
   ========================================================================= */

/**
 * @brief Borrow a view over part of a string.
 *
 * The returned view does not own storage. It remains valid while @p s is alive
 * and unmodified.
 *
 * @param s    Source string. Must not be @c NULL.
 * @param pos  Starting storage offset.
 * @param len  Maximum storage length to borrow.
 * @return     Borrowed view, or an empty view for invalid input.
 */
string_view_t string_view(const string_t *s, size_t pos, size_t len);

/**
 * @brief Borrow a view over a whole string.
 *
 * @param s  Source string. Must not be @c NULL.
 * @return   Borrowed view over @p s.
 */
string_view_t string_view_all(const string_t *s);

/**
 * @brief Return an empty borrowed view.
 *
 * @return Empty view value.
 */
string_view_t string_view_empty(void);

/**
 * @brief Borrow a sub-view from an existing view.
 *
 * @param view  Source view.
 * @param pos   Starting storage offset inside @p view.
 * @param len   Maximum storage length to borrow.
 * @return      Borrowed sub-view, or an empty view for invalid input.
 */
string_view_t string_view_slice(string_view_t view, size_t pos, size_t len);

/**
 * @brief Copy a borrowed view into a new string.
 *
 * @param v  View to copy. Must not be @c NULL.
 * @return   Newly allocated string, or @c NULL on allocation failure.
 */
string_t *string_from_view(const string_view_t *v);

/**
 * @brief Return the storage length of a borrowed view.
 *
 * @param view  View to inspect.
 * @return      Number of storage bytes in the view.
 */
size_t string_view_length(string_view_t view);

/**
 * @brief Test whether a borrowed view is empty.
 *
 * @param view  View to inspect.
 * @return      Non-zero when empty, zero otherwise.
 */
int string_view_is_empty(string_view_t view);

/**
 * @brief Compare two borrowed views for byte-for-byte equality.
 *
 * @param a  First view.
 * @param b  Second view.
 * @return   Non-zero when equal, zero otherwise.
 */
int string_view_equals_view(string_view_t a, string_view_t b);

/**
 * @brief Inspect one storage unit in a borrowed view.
 *
 * This is an interoperability helper for code that must hash or compare
 * legacy ASCII/UTF-8 spellings without exposing the view's backing pointer.
 * Most ordinary text code should prefer cursors and runes.
 *
 * @param view  View to inspect.
 * @param pos   Storage offset inside @p view.
 * @param out   Optional output storage unit.
 * @return      @c true when @p pos is valid, @c false otherwise.
 */
bool string_view_peek_storage(string_view_t view, size_t pos, unsigned char *out);

/**
 * @brief Decode the codepoint that begins at a view storage offset.
 *
 * @param view       View to inspect.
 * @param pos        Storage offset inside @p view.
 * @param out        Optional decoded Unicode codepoint.
 * @param width_out  Optional number of storage units consumed.
 * @return           @c true when a codepoint was decoded, @c false otherwise.
 */
bool string_view_peek_codepoint(string_view_t view,
                                size_t pos,
                                uint32_t *out,
                                size_t *width_out);

/**
 * @brief Trim whitespace from both ends of a borrowed view.
 *
 * The returned view is borrowed from the same source as @p view.
 *
 * @param view  View to trim.
 * @return      Trimmed borrowed view.
 */
string_view_t string_view_trim(string_view_t view);

/**
 * @brief Compare a view with a null-terminated literal.
 *
 * @param view     View to compare.
 * @param literal  Null-terminated UTF-8 literal.
 * @return         @c true when the contents are equal.
 */
bool string_view_equals_literal(string_view_t view, const char *literal);

/**
 * @brief Test whether a view starts with a string.
 *
 * @param view              View to inspect.
 * @param literal           Prefix string to match.
 * @param case_insensitive  If true, compare ASCII letters case-insensitively.
 * @return                  @c true when @p literal matches at the start of
 *                          @p view.
 */
bool string_view_starts_with(string_view_t view,
                             const string_t *literal,
                             bool case_insensitive);

/**
 * @brief Test whether a view starts with another view.
 *
 * @param view              View to inspect.
 * @param literal           Prefix view to match.
 * @param case_insensitive  If true, compare ASCII letters case-insensitively.
 * @return                  @c true when @p literal matches at the start of
 *                          @p view.
 */
bool string_view_starts_with_view(string_view_t view,
                                  string_view_t literal,
                                  bool case_insensitive);

/* =========================================================================
   Cursor reading
   ========================================================================= */

/**
 * @brief Create a cursor for reading a string from left to right.
 *
 * The cursor borrows @p s. It remains valid while @p s is alive and
 * unmodified. The cursor hides storage details and lets callers work in terms
 * of runes, positions, literal matches, and slices.
 *
 * @param s  Source string. Must not be @c NULL.
 * @return   New cursor, or @c NULL on allocation failure.
 */
string_cursor_t *string_cursor_new(const string_t *s);

/**
 * @brief Create a cursor for reading a borrowed string view.
 *
 * The cursor owns an internal copy of @p view, so it remains valid until the
 * cursor is freed even if the original view source goes away.
 *
 * @param view  Borrowed view to read.
 * @return      New cursor, or @c NULL on allocation failure.
 */
string_cursor_t *string_cursor_new_view(string_view_t view);

/**
 * @brief Clone a cursor, preserving its current position.
 *
 * Useful for speculative parsing: clone, scan ahead, then either discard the
 * clone or copy its position back with string_cursor_seek().
 *
 * @param cursor  Cursor to clone. Must not be @c NULL.
 * @return        New cursor, or @c NULL on allocation failure.
 */
string_cursor_t *string_cursor_clone(const string_cursor_t *cursor);

/**
 * @brief Destroy a cursor.
 *
 * Passing @c NULL is safe.
 *
 * @param cursor  Cursor to destroy, or @c NULL.
 */
void string_cursor_free(string_cursor_t *cursor);

/**
 * @brief Test whether a cursor is at the end of its source text.
 *
 * @param cursor  Cursor to inspect. Must not be @c NULL.
 * @return        @c true at end, @c false otherwise.
 */
bool string_cursor_done(const string_cursor_t *cursor);

/**
 * @brief Return the cursor's current position.
 *
 * @param cursor  Cursor to inspect. Must not be @c NULL.
 * @return        Current cursor position.
 */
string_pos_t string_cursor_position(const string_cursor_t *cursor);

/**
 * @brief Return the end position of the cursor's source text.
 *
 * @param cursor  Cursor to inspect. Must not be @c NULL.
 * @return        End position for the cursor source.
 */
string_pos_t string_cursor_end_position(const string_cursor_t *cursor);

/**
 * @brief Move the cursor to a position in its source text.
 *
 * @param cursor  Cursor to move. Must not be @c NULL.
 * @param pos     Position previously returned by a cursor API.
 * @return        0 on success, non-zero if @p pos is invalid.
 */
int string_cursor_seek(string_cursor_t *cursor, string_pos_t pos);

/**
 * @brief Peek at the current user-visible rune without advancing.
 *
 * @param cursor  Cursor to inspect. Must not be @c NULL.
 * @return        Current rune, or an empty rune at end.
 */
rune_t string_cursor_peek(const string_cursor_t *cursor);

/**
 * @brief Advance by one user-visible rune.
 *
 * @param cursor  Cursor to move. Must not be @c NULL.
 * @return        0 on success, non-zero at end or on bad arguments.
 */
int string_cursor_next(string_cursor_t *cursor);

/**
 * @brief Test whether the cursor currently matches literal text.
 *
 * This is a string-module boundary helper: callers may pass ordinary typed
 * text, while the string module performs the storage comparison.
 *
 * @param cursor   Cursor to inspect. Must not be @c NULL.
 * @param literal  Null-terminated UTF-8 literal. Must not be @c NULL.
 * @return         @c true if @p literal matches at the cursor.
 */
bool string_cursor_match(const string_cursor_t *cursor, const char *literal);

/**
 * @brief Consume literal text if it matches at the cursor.
 *
 * @param cursor   Cursor to move. Must not be @c NULL.
 * @param literal  Null-terminated UTF-8 literal. Must not be @c NULL.
 * @return         @c true if consumed, @c false if it did not match.
 */
bool string_cursor_consume(string_cursor_t *cursor, const char *literal);

/**
 * @brief Peek at the current rune as ASCII.
 *
 * @param cursor  Cursor to inspect. Must not be @c NULL.
 * @param out     Optional output character.
 * @return        @c true when the current rune is one ASCII character.
 */
bool string_cursor_peek_ascii(const string_cursor_t *cursor, unsigned char *out);

/**
 * @brief Peek at a saved cursor position as ASCII.
 *
 * @param cursor  Cursor whose source owns @p pos.
 * @param pos     Position to inspect.
 * @param out     Optional output character.
 * @return        @c true when the rune at @p pos is one ASCII character.
 */
bool string_cursor_peek_ascii_at(const string_cursor_t *cursor,
                                 string_pos_t pos,
                                 unsigned char *out);

/**
 * @brief Advance over a known storage span.
 *
 * Use this after matching a literal or borrowed view span when the cursor
 * needs to move by that exact matched length.
 *
 * @param cursor  Cursor to move.
 * @param length  Storage length to skip.
 * @return        @c true on success, @c false if the span is invalid.
 */
bool string_cursor_skip(string_cursor_t *cursor, size_t length);

/**
 * @brief Skip whitespace runes at the cursor.
 *
 * Handles ordinary ASCII whitespace and common Unicode space codepoints.
 *
 * @param cursor  Cursor to move.
 */
void string_cursor_skip_spaces(string_cursor_t *cursor);

/**
 * @brief Copy text between two cursor positions into a new string.
 *
 * @param cursor  Cursor whose source text owns the positions.
 * @param start   Start position.
 * @param end     End position.
 * @return        New string, or @c NULL on invalid positions/allocation failure.
 */
string_t *string_cursor_slice(const string_cursor_t *cursor,
                              string_pos_t start,
                              string_pos_t end);

/**
 * @brief Append text between two cursor positions to an existing string.
 *
 * @param out     Destination string.
 * @param cursor  Cursor whose source text owns the positions.
 * @param start   Start position.
 * @param end     End position.
 * @return        0 on success, non-zero on error.
 */
int string_cursor_append_slice(string_t *out,
                               const string_cursor_t *cursor,
                               string_pos_t start,
                               string_pos_t end);

/**
 * @brief Borrow a view between two cursor positions.
 *
 * The returned view remains valid while the cursor and its source remain alive
 * and unmodified.
 *
 * @param cursor  Cursor whose source text owns the positions.
 * @param start   Start position.
 * @param end     End position.
 * @return        Borrowed view, or an empty view for invalid positions.
 */
string_view_t string_cursor_view(const string_cursor_t *cursor,
                                 string_pos_t start,
                                 string_pos_t end);

/**
 * @brief Copy text from a position up to the cursor's current position.
 *
 * @param cursor  Cursor whose source text owns the position.
 * @param start   Start position.
 * @return        New string, or @c NULL on invalid position/allocation failure.
 */
string_t *string_cursor_slice_since(const string_cursor_t *cursor,
                                    string_pos_t start);

/**
 * @brief Borrow a view from a position to the cursor's current position.
 *
 * @param cursor  Cursor whose source text owns the position.
 * @param start   Start position.
 * @return        Borrowed view, or an empty view for invalid positions.
 */
string_view_t string_cursor_view_since(const string_cursor_t *cursor,
                                       string_pos_t start);

/**
 * @brief Peek at a rune at a saved cursor position without moving the cursor.
 *
 * @param cursor  Cursor whose source text owns the position.
 * @param pos     Position to inspect.
 * @return        Rune at @p pos, or an empty rune for invalid/end positions.
 */
rune_t string_cursor_peek_at(const string_cursor_t *cursor,
                             string_pos_t pos);

/**
 * @brief Test whether literal text matches at a saved cursor position.
 *
 * @param cursor   Cursor whose source text owns the position.
 * @param pos      Position at which to test.
 * @param literal  Null-terminated UTF-8 literal. Must not be @c NULL.
 * @return         @c true if @p literal matches at @p pos.
 */
bool string_cursor_match_at(const string_cursor_t *cursor,
                            string_pos_t pos,
                            const char *literal);

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
   printf-style formatting
   ========================================================================= */

/**
 * @brief Append formatted text using a @c va_list.
 *
 * The string module owns @c %S for `const string_t *`, @c %W for
 * `string_view_t`, and @c %R for `rune_t`.
 *
 * @param s    Destination string. Must not be @c NULL.
 * @param fmt  printf-style format string. Must not be @c NULL.
 * @param ap   Argument list (consumed by this call).
 * @return     Number of bytes appended, or negative on error.
 */
int string_append_vformat(string_t *s, const char *fmt, va_list ap);

/**
 * @brief Append formatted text using a printf-style format string.
 *
 * In addition to ordinary printf conversions, the string module owns:
 * @c %S for `const string_t *`, @c %W for `string_view_t`, and @c %R for
 * `rune_t`.
 *
 * @code
 * string_append_format(out, "name=%S first=%R", name, string_at(name, 0));
 * @endcode
 *
 * Use @c %% for a literal percent sign.
 *
 * @param s    Destination string. Must not be @c NULL.
 * @param fmt  printf-style format string. Must not be @c NULL.
 * @param ...  Format arguments.
 * @return     Number of bytes appended, or negative on error.
 */
int string_append_format(string_t *s, const char *fmt, ...);

/**
 * @brief Create a new string from a @c va_list format operation.
 *
 * Uses the same conversion extensions as string_append_format():
 * @c %S for `const string_t *`, @c %W for `string_view_t`, and @c %R for
 * `rune_t`.
 *
 * @param fmt  printf-style format string. Must not be @c NULL.
 * @param ap   Argument list (consumed by this call).
 * @return     Newly allocated formatted string, or @c NULL on error.
 */
string_t *string_vsprintf(const char *fmt, va_list ap);

/**
 * @brief Create a new formatted string.
 *
 * Uses the same conversion extensions as string_append_format():
 * @c %S for `const string_t *`, @c %W for `string_view_t`, and @c %R for
 * `rune_t`.
 *
 * @param fmt  printf-style format string. Must not be @c NULL.
 * @param ...  Format arguments.
 * @return     Newly allocated formatted string, or @c NULL on error.
 */
string_t *string_sprintf(const char *fmt, ...);

/**
 * @brief Print formatted text to stdout.
 *
 * Uses the same conversion extensions as string_append_format():
 * @c %S for `const string_t *`, @c %W for `string_view_t`, and @c %R for
 * `rune_t`.
 *
 * @param fmt  printf-style format string. Must not be @c NULL.
 * @param ...  Format arguments.
 * @return     Number of bytes printed, or negative on error.
 */
int string_printf(const char *fmt, ...);

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
 * @brief Find the first occurrence of a string.
 *
 * Searches @p s for the first occurrence of @p needle using a
 * bytewise comparison.
 *
 * @param s       String to search. Must not be @c NULL.
 * @param needle  String to find. Must not be @c NULL.
 * @return        Byte offset of the first match, or -1 if not found.
 */
string_offset_t string_find_string(const string_t *s, const string_t *needle);

/**
 * @brief Lexicographically compare two strings by UTF-8 byte value.
 *
 * The comparison is bytewise (not locale-aware or Unicode collation order).
 * Strings are stored in the class's canonical form before comparison.
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
 * @brief Test whether the string begins with a given prefix string.
 *
 * @param s       String to test. Must not be @c NULL.
 * @param prefix  Prefix string. Must not be @c NULL.
 * @return        @c true if @p s starts with @p prefix, @c false otherwise.
 */
bool string_starts_with_string(const string_t *s, const string_t *prefix);

/**
 * @brief Test whether the string ends with a given suffix.
 *
 * @param s       String to test. Must not be @c NULL.
 * @param suffix  Null-terminated UTF-8 suffix. Must not be @c NULL.
 * @return        @c true if @p s ends with @p suffix, @c false otherwise.
 */
bool string_ends_with(const string_t *s, const char *suffix);

/**
 * @brief Test whether the string ends with a given suffix string.
 *
 * @param s       String to test. Must not be @c NULL.
 * @param suffix  Suffix string. Must not be @c NULL.
 * @return        @c true if @p s ends with @p suffix, @c false otherwise.
 */
bool string_ends_with_string(const string_t *s, const string_t *suffix);

/**
 * @brief Extract a substring by storage byte range.
 *
 * Prefer string_substring() for ordinary text. This low-level function is for
 * byte-oriented code; @p pos and @p pos + @p len should lie on valid character
 * boundaries to avoid splitting encoded text.
 *
 * @param s    Source string. Must not be @c NULL.
 * @param pos  Starting byte offset.
 * @param len  Number of bytes to extract.
 * @return     Newly allocated substring, or @c NULL on error.
 */
string_t *string_substr(const string_t *s, size_t pos, size_t len);

/**
 * @brief Reverse the string by user-visible characters.
 *
 * Combined characters, accents and emoji sequences are preserved as units.
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

/**
 * @brief Replace all non-overlapping occurrences of one string with another.
 *
 * @param s        String to modify. Must not be @c NULL.
 * @param search   String to find. Must not be @c NULL.
 * @param replace  Replacement string. Must not be @c NULL.
 * @return         Number of replacements made, or negative on allocation failure.
 */
int string_replace_string(string_t *s,
                          const string_t *search,
                          const string_t *replace);

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
 * @brief Split a string by another string delimiter.
 *
 * Empty tokens (from leading, trailing, or consecutive delimiters) are
 * preserved.
 *
 * @param s          Source string. Must not be @c NULL.
 * @param delim      Delimiter string. Must not be @c NULL.
 * @param out_count  Output: number of elements in the returned array.
 * @return           Array of newly allocated strings, or @c NULL on failure.
 */
string_t **string_split_string(const string_t *s,
                               const string_t *delim,
                               size_t *out_count);

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

/**
 * @brief Join an array of strings with a separator string.
 *
 * @param arr    Array of strings to join. Must not be @c NULL.
 * @param count  Number of elements in @p arr.
 * @param sep    Separator string. Must not be @c NULL.
 * @return       Newly allocated joined string, or @c NULL on failure.
 */
string_t *string_join_string(string_t **arr,
                             size_t count,
                             const string_t *sep);

/* =========================================================================
   Case conversion
   ========================================================================= */

/**
 * @brief Convert text to uppercase in place.
 *
 * Uses Unicode case mapping when built with libunistring; otherwise falls
 * back to ASCII letters.
 *
 * @param s  String to modify. Must not be @c NULL.
 */
void string_to_upper(string_t *s);

/**
 * @brief Convert text to lowercase in place.
 *
 * Uses Unicode case mapping when built with libunistring; otherwise falls
 * back to ASCII letters.
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
 * printf("%s\n", string_buffer_c_str(&buf));   // "Hello!"
 * @endcode
 */
typedef struct {
    uintptr_t _opaque_storage[3]; /**< Implementation storage; use string_buffer_* accessors. */
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
 * @brief Append a string to a fixed-capacity buffer.
 *
 * If the text does not fit, as much as possible is appended up to a valid
 * UTF-8 codepoint boundary and the buffer is NUL-terminated.
 *
 * @param b     Destination buffer. Must not be @c NULL.
 * @param text  String to append. Must not be @c NULL.
 * @return      0 if the full text was appended, non-zero if truncated.
 */
int string_buffer_append_string(string_buffer_t *b, const string_t *text);

/**
 * @brief Append a single ASCII character to a fixed-capacity buffer.
 *
 * @param b  Destination buffer. Must not be @c NULL.
 * @param c  Character to append.
 * @return   0 on success, non-zero if the buffer is full.
 */
int string_buffer_append_char(string_buffer_t *b, char c);

/**
 * @brief Return the current NUL-terminated contents of a fixed-capacity buffer.
 *
 * @param b  Buffer to inspect. May be @c NULL.
 * @return   NUL-terminated contents, or an empty string for @c NULL.
 */
const char *string_buffer_c_str(const string_buffer_t *b);

/**
 * @brief Return the current byte length of a fixed-capacity buffer.
 *
 * @param b  Buffer to inspect. May be @c NULL.
 * @return   Number of bytes written, excluding the NUL terminator.
 */
size_t string_buffer_length(const string_buffer_t *b);

/**
 * @brief Return the total byte capacity of a fixed-capacity buffer.
 *
 * @param b  Buffer to inspect. May be @c NULL.
 * @return   Capacity in bytes, including space for the NUL terminator.
 */
size_t string_buffer_capacity(const string_buffer_t *b);

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
string_builder_t *string_builder_new(void);

/**
 * @brief Free a builder and all associated memory.
 * @param b  Builder to free, or @c NULL.
 */
void string_builder_free(string_builder_t *b);

/**
 * @brief Append a null-terminated C string to the builder.
 * @param b  Builder. Must not be @c NULL.
 * @param s  Text to append. Must not be @c NULL.
 * @return   0 on success, non-zero on allocation failure.
 */
int string_builder_append(string_builder_t *b, const char *s);

/**
 * @brief Append a string to the builder.
 * @param b  Builder. Must not be @c NULL.
 * @param s  String to append. Must not be @c NULL.
 * @return   0 on success, non-zero on allocation failure.
 */
int string_builder_append_string(string_builder_t *b, const string_t *s);

/**
 * @brief Append a single ASCII character to the builder.
 * @param b  Builder. Must not be @c NULL.
 * @param c  Character to append.
 * @return   0 on success, non-zero on allocation failure.
 */
int string_builder_append_char(string_builder_t *b, char c);

/**
 * @brief Append formatted text to the builder (printf-style).
 *
 * Use @c %S for `const string_t *`, @c %W for `string_view_t`, and
 * @c %R for `rune_t`.
 *
 * @param b    Builder. Must not be @c NULL.
 * @param fmt  printf-style format string. Must not be @c NULL.
 * @param ...  Format arguments.
 * @return     Number of bytes appended, or negative on error.
 */
int string_builder_format(string_builder_t *b, const char *fmt, ...);

#endif /* USTRING_H */
