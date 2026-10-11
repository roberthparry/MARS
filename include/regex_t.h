/**
 * @file regex_t.h
 * @brief Opaque compiled regular expressions for MARS UTF-8 strings.
 *
 * regex_t owns a reusable POSIX extended expression. Search string_t values,
 * inspect whole-match and capture-group byte spans, or copy individual captures.
 * Matching uses a private C.UTF-8 locale without changing the process locale;
 * Unicode spelling is preserved rather than normalised. Options bound pattern,
 * subject and capture sizes, but do not impose an engine time or memory budget.
 *
 * Linux libc supplies the engine; no additional library is required. This is
 * not PCRE: lookaround, lazy quantifiers and named groups are not supported.
 * Use regex_t.h rather than the system regex.h. Their regex_t typedefs must not
 * appear in the same translation unit. Do not share a handle concurrently or
 * mutate a subject during a search. Ordinary literal searches belong to string_t.h.
 */
#ifndef MARS_REGEX_H
#define MARS_REGEX_H

#include <stdbool.h>
#include <stddef.h>

#include "string_t.h"

/** @brief Opaque compiled expression; release with regex_free(). */
typedef struct _regex_t regex_t;

/** @brief Search result; no match is not an error. */
typedef enum { REGEX_ERROR = -1, REGEX_NO_MATCH = 0, REGEX_MATCH = 1 } regex_status_t;

/** @brief Copied compilation policy; initialise with regex_options_default(). */
typedef struct {
    bool ignore_case;          /**< Case-insensitive matching; false by default. */
    bool multiline;            /**< Line-aware anchors, dot and negated classes; false by default. */
    size_t max_pattern_bytes;  /**< Positive pattern limit; default 64 KiB, at most INT_MAX. */
    size_t max_subject_bytes;  /**< Positive subject limit; default 64 MiB, at most INT_MAX. */
    size_t max_groups;         /**< Positive capture-group limit; default 1024, at most 65535. */
} regex_options_t;

/** @brief Byte interval [start,end); an unmatched optional group has matched=false and zero offsets. */
typedef struct {
    size_t start; /**< Inclusive UTF-8 byte offset in the original subject. */
    size_t end;   /**< Exclusive UTF-8 byte offset; equals start for an empty match. */
    bool matched; /**< Distinguishes an empty participating group from an absent group. */
} regex_span_t;

/** @brief Obtain the finite default policy. @return Options for regex_new(). */
regex_options_t regex_options_default(void);

/**
 * @brief Compile a POSIX extended pattern, including the valid empty pattern.
 * @param pattern Borrowed complete UTF-8 pattern without NUL; not retained.
 * @param options Borrowed policy copied by value, or NULL for defaults.
 * @param error Optional empty output slot; receives an owned diagnostic on failure, otherwise NULL.
 * Release any diagnostic with string_free() before reusing its output slot.
 * @return Caller-owned expression, or NULL with errno set; release with regex_free().
 */
regex_t *regex_new(const string_t *pattern, const regex_options_t *options, string_t **error);

/** @brief Release the compiled expression and private locale. @param regex Owned handle, or NULL. */
void regex_free(regex_t *regex);

/** @brief Count parenthesised capture groups, excluding group zero. @param regex Borrowed handle.
 * @return Group count, or zero for NULL. */
size_t regex_group_count(const regex_t *regex);

/**
 * @brief Find the leftmost longest match at or after a UTF-8 byte position.
 * @param regex Borrowed compiled expression.
 * @param subject Borrowed complete UTF-8 text without NUL, unchanged throughout the call.
 * @param start Byte offset, including EOF; must lie on a Unicode scalar boundary.
 * This does not create a new beginning of line. Anchors retain their original-subject meaning.
 * @param spans Optional caller array; index zero is the whole match, followed by numbered groups.
 * Valid arrays are cleared before searching, including on no match. Excess groups remain unmatched.
 * @param count Array capacity, at most max_groups+1; zero permits spans=NULL and discards captures.
 * @param error Optional empty diagnostic slot, as for regex_new().
 * @return REGEX_MATCH, REGEX_NO_MATCH, or REGEX_ERROR with errno set. Empty matches are valid;
 * advance by a complete Unicode scalar before repeating after an empty match, or stop at EOF.
 */
regex_status_t regex_search(const regex_t *regex, const string_t *subject, size_t start,
                            regex_span_t *spans, size_t count, string_t **error);

/** @brief Search from byte zero without captures.
 * @param regex Borrowed compiled expression. @param subject Borrowed UTF-8 text.
 * @param error Optional empty diagnostic slot, as for regex_new(). @return Search status. */
regex_status_t regex_test(const regex_t *regex, const string_t *subject, string_t **error);

/**
 * @brief Copy a capture without Unicode normalisation.
 * @param subject Borrowed original, unchanged subject used to obtain the span.
 * @param span Borrowed capture span; a participating empty span produces an empty string.
 * @return Owned string, or NULL for an unmatched group (errno=0) or invalid input/allocation failure
 * (errno set). Caller frees the result with string_free().
 */
string_t *regex_capture(const string_t *subject, const regex_span_t *span);

#endif
