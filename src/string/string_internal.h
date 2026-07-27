#ifndef STRING_INTERNAL_H
#define STRING_INTERNAL_H

#if !defined(MARS_STRING_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || \
     (defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__ > 0))
#error "string_internal.h is private to the string module; include ustring.h instead."
#endif

/**
 * @file string_internal.h
 * @brief Internal representation and helpers for the string_t type.
 *
 * Not for public use. External code should include only ustring.h.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "ustring.h"
#include "string_view.h"

/**
 * @brief Full internal definition of a dynamic UTF-8 string.
 *
 * Invariants:
 *   • data[len] == '\0' at all times (NUL terminator always present).
 *   • cap > len  (capacity always exceeds the occupied byte count).
 *   • All bytes in data[0..len) are valid UTF-8 (enforced at mutation sites).
 *
 * Fields:
 *   data        — heap-allocated byte buffer; NUL-terminated.
 *   len         — number of content bytes, not counting the NUL terminator.
 *   cap         — number of bytes allocated (always at least len + 1).
 *   scratch     — optional transient buffer reserved for internal helpers
 *                 that need short-lived materialised storage.
 *   scratch_cap — number of bytes allocated for scratch.
 */
struct _string_t {
    char  *data;
    size_t len;
    size_t cap;
    char  *scratch;
    size_t scratch_cap;
};

/**
 * @brief Internal cursor over a string.
 *
 * The cursor normally borrows @c source and advances by encoded positions
 * that are managed through the public cursor API. When constructed from a
 * string view, @c owned_source holds the materialised view so cursor movement
 * is still backed by a valid string object.
 *
 * Fields:
 *   source       — borrowed string being traversed.
 *   owned_source — optional owned backing string for view-derived cursors.
 *   pos          — current encoded byte position within source.
 */
struct _string_cursor_t {
    const string_t *source;
    string_t       *owned_source;
    size_t          pos;
};

/**
 * @brief Ensure @p s has room for at least @p needed bytes (including the NUL).
 *
 * Doubles capacity until the requirement is met, then reallocates. No-op if
 * the current capacity is already sufficient.
 * Returns 0 on success, -1 on allocation failure.
 */
int string_reserve(string_t *s, size_t needed);

/**
 * @brief Decode one UTF-8 codepoint from @p s[0..len).
 *
 * On success, stores the decoded codepoint in @p *adv and returns the
 * number of bytes consumed (1–4).
 * On invalid or truncated input, returns 1 and stores U+FFFD (replacement
 * character) so the caller can advance past the bad byte and continue.
 */
uint32_t utf8_decode(const char *s, size_t len, size_t *adv);

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

size_t utf8_next(const char *s, size_t len, size_t i);
size_t string_utf8_prev(const char *s, size_t len, size_t i);
size_t string_utf8_length(const string_t *s);
void string_utf8_reverse(string_t *s);
void string_utf8_to_upper(string_t *s);
void string_utf8_to_lower(string_t *s);

grapheme_class_t string_grapheme_class(uint32_t cp);
size_t string_grapheme_next(const char *s, size_t len, size_t i);
size_t string_grapheme_prev(const char *s, size_t len, size_t i);
size_t string_grapheme_count(const string_t *s);
void string_grapheme_reverse(string_t *s);
string_t *string_grapheme_substr(const string_t *s, size_t gpos, size_t glen);
string_t *string_grapheme_at(const string_t *s, size_t index);
bool rune_is_empty(rune_t rune);

typedef enum {
    STRING_NORM_NFC,
    STRING_NORM_NFD,
    STRING_NORM_NFKC,
    STRING_NORM_NFKD,
    STRING_NORM_COUNT
} string_norm_form_t;

int string_normalise(string_t *s, string_norm_form_t form);
int string_normalise_storage(string_t *s);
size_t string_encoded_len(const string_t *s);
size_t string_character_byte_offset(const string_t *s, size_t index);

#endif
