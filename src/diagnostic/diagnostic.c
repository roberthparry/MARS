/**
 * @file diagnostic.c
 * @brief Owned diagnostic records with bounded immutable causes and plain-text presentation.
 *
 * Construction copies text before publishing it. Atomic reference counts allow
 * native workers to hand records to observers without sharing mutable strings.
 * Cause traversal is bounded and destruction is iterative, not recursive.
 */
#include <errno.h>
#include <stdatomic.h>
#include <stdlib.h>

#include "diagnostic-t.h"

typedef struct _diagnostic_t {
    atomic_size_t references;
    diagnostic_severity_t severity;
    int64_t code;
    string_t *domain;
    string_t *message;
    string_t *source;
    diagnostic_location_t location;
    bool located;
    diagnostic_t *cause;
    size_t depth;
} diagnostic_t;

/* Publish only complete, independently owned records. */
diagnostic_t *diagnostic_new(diagnostic_severity_t severity, const char *domain, int64_t code,
                             const char *message, const diagnostic_location_t *location, diagnostic_t *cause)
{
    if (severity < DIAGNOSTIC_NOTE || severity > DIAGNOSTIC_ERROR || !domain || !*domain || !message || !*message ||
        (location && location->column && !location->line)) {
        errno = EINVAL;
        return NULL;
    }
    if (location && location->length > SIZE_MAX - location->offset) {
        errno = EOVERFLOW;
        return NULL;
    }
    if (cause && cause->depth >= 64) {
        errno = ELOOP;
        return NULL;
    }
    diagnostic_t *diagnostic = calloc(1, sizeof(*diagnostic));
    if (!diagnostic)
        return NULL;
    atomic_init(&diagnostic->references, 1);
    diagnostic->severity = severity;
    diagnostic->code = code;
    diagnostic->depth = cause ? cause->depth + 1 : 1;
    diagnostic->domain = string_new_with(domain);
    diagnostic->message = string_new_with(message);
    if (location) {
        diagnostic->located = true;
        diagnostic->location = *location;
        if (location->source)
            diagnostic->source = string_new_with(location->source);
        diagnostic->location.source = diagnostic->source ? string_c_str(diagnostic->source) : NULL;
    }
    if (cause)
        diagnostic->cause = diagnostic_retain(cause);
    if (!diagnostic->domain || !diagnostic->message || (location && location->source && !diagnostic->source) ||
        (cause && !diagnostic->cause)) {
        diagnostic_free(diagnostic);
        return NULL;
    }
    return diagnostic;
}

/* Retaining shares immutable data rather than copying it. */
diagnostic_t *diagnostic_retain(const diagnostic_t *diagnostic)
{
    if (!diagnostic) {
        errno = EINVAL;
        return NULL;
    }
    diagnostic_t *shared = (diagnostic_t *)diagnostic;
    size_t references = atomic_load(&shared->references);
    do {
        if (references == SIZE_MAX) {
            errno = EOVERFLOW;
            return NULL;
        }
    } while (!atomic_compare_exchange_weak(&shared->references, &references, references + 1));
    return shared;
}

/* Release chains iteratively and leave the caller's error intact. */
void diagnostic_free(diagnostic_t *diagnostic)
{
    int saved = errno;
    while (diagnostic && atomic_fetch_sub(&diagnostic->references, 1) == 1) {
        diagnostic_t *cause = diagnostic->cause;
        string_free(diagnostic->domain);
        string_free(diagnostic->message);
        string_free(diagnostic->source);
        free(diagnostic);
        diagnostic = cause;
    }
    errno = saved;
}

/* Read immutable severity. */
diagnostic_severity_t diagnostic_severity(const diagnostic_t *diagnostic)
{
    if (!diagnostic)
        errno = EINVAL;
    return diagnostic ? diagnostic->severity : DIAGNOSTIC_INVALID;
}

/* Borrow the domain without allocating. */
const char *diagnostic_domain(const diagnostic_t *diagnostic)
{
    if (!diagnostic)
        errno = EINVAL;
    return diagnostic ? string_c_str(diagnostic->domain) : NULL;
}

/* Read the domain-specific code. */
int64_t diagnostic_code(const diagnostic_t *diagnostic)
{
    if (!diagnostic)
        errno = EINVAL;
    return diagnostic ? diagnostic->code : 0;
}

/* Borrow copied message storage. */
const char *diagnostic_message(const diagnostic_t *diagnostic)
{
    if (!diagnostic)
        errno = EINVAL;
    return diagnostic ? string_c_str(diagnostic->message) : NULL;
}

/* Preserve the distinction between an absent location and byte offset zero. */
const diagnostic_location_t *diagnostic_location(const diagnostic_t *diagnostic)
{
    if (!diagnostic)
        errno = EINVAL;
    return diagnostic && diagnostic->located ? &diagnostic->location : NULL;
}

/* A borrowed cause stays alive through its parent. */
const diagnostic_t *diagnostic_cause(const diagnostic_t *diagnostic)
{
    if (!diagnostic)
        errno = EINVAL;
    return diagnostic ? diagnostic->cause : NULL;
}

/* Depth is cached at construction, not found by a scan. */
size_t diagnostic_depth(const diagnostic_t *diagnostic)
{
    if (!diagnostic)
        errno = EINVAL;
    return diagnostic ? diagnostic->depth : 0;
}

/* Render each bounded cause once, with direct severity lookup. */
string_t *diagnostic_to_string(const diagnostic_t *diagnostic)
{
    static const char *const labels[] = {"note", "warning", "error"};
    if (!diagnostic) {
        errno = EINVAL;
        return NULL;
    }
    string_t *text = string_new();
    if (!text)
        return NULL;
    for (const diagnostic_t *entry = diagnostic; entry; entry = entry->cause) {
        if ((entry != diagnostic && string_append_cstr(text, "\nCaused by: ") != 0) ||
            string_append_format(text, "%s [%s:%lld]: %s", labels[entry->severity],
                                 string_c_str(entry->domain), (long long)entry->code,
                                 string_c_str(entry->message)) < 0)
            goto failed;
        const diagnostic_location_t *location = diagnostic_location(entry);
        if (!location)
            continue;
        if (string_append_format(text, " (%s", location->source ? location->source : "source") < 0)
            goto failed;
        if (location->line && string_append_format(text, ":%zu", location->line) < 0)
            goto failed;
        if (location->column && string_append_format(text, ":%zu", location->column) < 0)
            goto failed;
        if (string_append_format(text, "; bytes %zu+%zu)", location->offset, location->length) < 0)
            goto failed;
    }
    return text;
failed:
    string_free(text);
    return NULL;
}
