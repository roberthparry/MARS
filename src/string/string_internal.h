#ifndef STRING_INTERNAL_H
#define STRING_INTERNAL_H

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

/**
 * @brief Full internal definition of a dynamic UTF-8 string.
 *
 * Invariants:
 *   • data[len] == '\0' at all times (NUL terminator always present).
 *   • cap > len  (capacity always exceeds the occupied byte count).
 *   • All bytes in data[0..len) are valid UTF-8 (enforced at mutation sites).
 *
 * Fields:
 *   data — heap-allocated byte buffer; NUL-terminated.
 *   len  — number of content bytes, not counting the NUL terminator.
 *   cap  — number of bytes allocated (always at least len + 1).
 */
struct string_t {
    char  *data;
    size_t len;
    size_t cap;
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

#endif
