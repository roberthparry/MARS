/**
 * @file regex.c
 * @brief Public compiled-regex ownership, input policies and UTF-8 capture extraction.
 *
 * Validates MARS strings without copying or normalising them, enforces the
 * documented size limits and converts native offsets into public capture spans.
 * Linux-specific engine and locale details live behind regex_native.h.
 */
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

#include "regex_t.h"
#include "regex_native.h"

struct _regex_t {
    regex_native_t *native;
    regex_options_t options;
};

static regex_status_t regex_fail(int code, const char *message, string_t **error)
{
    if (error)
        *error = string_new_with(message);
    errno = code;
    return REGEX_ERROR;
}

static bool regex_text_valid(const string_t *text)
{
    if (!string_is_valid_utf8(text))
        return false;
    string_view_t view = string_view_all(text);
    for (size_t at = 0, next; at < string_view_length(view); at = next) {
        uint32_t rune;
        if (!string_view_peek_rune_value(view, at, &rune, &next) || !rune)
            return false;
    }
    return true;
}

static bool regex_boundary(const string_t *text, size_t position)
{
    size_t size = string_byte_length(text);
    if (position > size)
        return false;
    if (position == size)
        return true;
    const unsigned char *bytes = (const unsigned char *)string_c_str(text);
    return (bytes[position] & 0xc0) != 0x80;
}

/* Conservative input limits are not a timeout or an engine memory budget. */
regex_options_t regex_options_default(void)
{
    return (regex_options_t){.max_pattern_bytes = 64 * 1024, .max_subject_bytes = 64 * 1024 * 1024,
                             .max_groups = 1024};
}

/* Own compiled state only; pattern and options may be released immediately. */
regex_t *regex_new(const string_t *pattern, const regex_options_t *options, string_t **error)
{
    if (error)
        *error = NULL;
    regex_options_t policy = options ? *options : regex_options_default();
    if (!pattern || !policy.max_pattern_bytes || !policy.max_subject_bytes || !policy.max_groups ||
        policy.max_pattern_bytes > INT_MAX || policy.max_subject_bytes > INT_MAX || policy.max_groups > 65535) {
        regex_fail(EINVAL, "Invalid regular-expression policy or pattern", error);
        return NULL;
    }
    if (string_byte_length(pattern) > policy.max_pattern_bytes) {
        regex_fail(EOVERFLOW, "Regular-expression pattern limit exceeded", error);
        return NULL;
    }
    if (!regex_text_valid(pattern)) {
        regex_fail(EILSEQ, "Pattern must be complete UTF-8 without NUL", error);
        return NULL;
    }
    regex_t *regex = calloc(1, sizeof(*regex));
    if (!regex) {
        regex_fail(ENOMEM, "Allocating regular-expression handle failed", error);
        return NULL;
    }
    regex->options = policy;
    regex->native = regex_native_new(pattern, policy.ignore_case, policy.multiline, error);
    if (!regex->native) {
        free(regex);
        return NULL;
    }
    if (regex_native_groups(regex->native) > policy.max_groups) {
        regex_free(regex);
        regex_fail(EOVERFLOW, "Regular-expression capture-group limit exceeded", error);
        return NULL;
    }
    return regex;
}

/* Release both public and native state; NULL is harmless. */
void regex_free(regex_t *regex)
{
    if (!regex)
        return;
    regex_native_free(regex->native);
    free(regex);
}

/* Group zero is the whole match, not a parenthesised capture. */
size_t regex_group_count(const regex_t *regex)
{
    return regex ? regex_native_groups(regex->native) : 0;
}

/* Search without retaining subjects or using a process-global error buffer. */
regex_status_t regex_search(const regex_t *regex, const string_t *subject, size_t start,
                            regex_span_t *spans, size_t count, string_t **error)
{
    if (error)
        *error = NULL;
    if (!regex || !subject || (count && !spans) || count > regex->options.max_groups + 1)
        return regex_fail(EINVAL, "Invalid regular-expression search arguments", error);
    for (size_t i = 0; i < count; ++i)
        spans[i] = (regex_span_t){0};
    if (string_byte_length(subject) > regex->options.max_subject_bytes)
        return regex_fail(EOVERFLOW, "Regular-expression subject limit exceeded", error);
    if (!regex_text_valid(subject))
        return regex_fail(EILSEQ, "Subject must be complete UTF-8 without NUL", error);
    if (!regex_boundary(subject, start))
        return regex_fail(EINVAL, "Search position is not a UTF-8 boundary", error);
    size_t used = count < regex_group_count(regex) + 1 ? count : regex_group_count(regex) + 1;
    size_t local[32];
    size_t *offsets = used <= 16 ? local : malloc(2 * used * sizeof(*offsets));
    if (!offsets)
        return regex_fail(ENOMEM, "Allocating capture offsets failed", error);
    int status = regex_native_search(regex->native, subject, start, offsets, offsets + used, used, error);
    if (status == REGEX_MATCH) {
        for (size_t i = 0; i < used; ++i)
            if (offsets[i] != SIZE_MAX)
                spans[i] = (regex_span_t){.start = offsets[i], .end = offsets[used + i], .matched = true};
    }
    if (offsets != local)
        free(offsets);
    return (regex_status_t)status;
}

/* The common predicate path requests no capture array. */
regex_status_t regex_test(const regex_t *regex, const string_t *subject, string_t **error)
{
    return regex_search(regex, subject, 0, NULL, 0, error);
}

/* Copy only the selected byte range, preserving decomposed Unicode spelling. */
string_t *regex_capture(const string_t *subject, const regex_span_t *span)
{
    if (!subject || !span) {
        errno = EINVAL;
        return NULL;
    }
    if (!span->matched) {
        errno = 0;
        return NULL;
    }
    if (!regex_text_valid(subject) || span->end < span->start || !regex_boundary(subject, span->start) ||
        !regex_boundary(subject, span->end)) {
        errno = EINVAL;
        return NULL;
    }
    string_t *capture = string_new();
    if (!capture || string_append_bytes(capture, string_c_str(subject) + span->start, span->end - span->start) != 0) {
        string_free(capture);
        errno = ENOMEM;
        return NULL;
    }
    return capture;
}
