/**
 * @file diagnostic-t.h
 * @brief Immutable, reference-counted structured diagnostics and cause chains.
 *
 * diagnostic_t copies a domain, numeric code, severity, UTF-8 message and optional
 * source location, and retains an optional cause. Use it for errors crossing
 * callback or thread boundaries without relying on another thread's errno.
 * Locations use zero-based byte offsets and lengths, not character indices;
 * optional line and column numbers are one-based. No source contents are stored.
 * Concurrent readers and reference operations are supported; protect each
 * owned reference's lifetime. Formatting returns independent string_t storage.
 * This is not a logging sink, parser or exception mechanism.
 */
#ifndef MARS_DIAGNOSTIC_H
#define MARS_DIAGNOSTIC_H

#include <stddef.h>
#include <stdint.h>

#include "string-t.h"

/** @brief Opaque immutable diagnostic; release an owned reference with diagnostic_free(). */
typedef struct _diagnostic_t diagnostic_t;

/** @brief Severity, independent of an application-defined numeric code. */
typedef enum {
    DIAGNOSTIC_INVALID = -1,
    DIAGNOSTIC_NOTE,
    DIAGNOSTIC_WARNING,
    DIAGNOSTIC_ERROR
} diagnostic_severity_t;

/** @brief Optional location; source may be NULL, and zero line/column means unknown. */
typedef struct {
    const char *source; /**< Source name, copied on construction and borrowed on inspection. */
    size_t offset; /**< Zero-based byte offset. */
    size_t length; /**< Byte length; zero permits a point location. */
    size_t line; /**< One-based line, or zero if unknown. */
    size_t column; /**< One-based column, or zero if unknown; requires a known line. */
} diagnostic_location_t;

/**
 * @brief Copy required non-empty domain/message and optional location, retaining cause.
 * Chains are limited to 64 entries (ELOOP); invalid severity/location gives EINVAL,
 * overflowing byte extent gives EOVERFLOW, allocation failure gives ENOMEM.
 * UTF-8 text follows string_t normalisation. No input ownership is transferred.
 */
diagnostic_t *diagnostic_new(diagnostic_severity_t severity, const char *domain, int64_t code,
                             const char *message, const diagnostic_location_t *location, diagnostic_t *cause);

/** @brief Retain the same immutable object; NULL gives EINVAL, reference overflow gives EOVERFLOW. */
diagnostic_t *diagnostic_retain(const diagnostic_t *diagnostic);

/** @brief Release a reference and any unreferenced causes, accepting NULL and preserving errno. */
void diagnostic_free(diagnostic_t *diagnostic);

/** @brief Return severity; NULL returns DIAGNOSTIC_INVALID with EINVAL. */
diagnostic_severity_t diagnostic_severity(const diagnostic_t *diagnostic);

/** @brief Borrow the immutable domain while retaining the diagnostic; NULL gives EINVAL. */
const char *diagnostic_domain(const diagnostic_t *diagnostic);

/** @brief Return the domain-specific code; NULL returns zero with EINVAL. */
int64_t diagnostic_code(const diagnostic_t *diagnostic);

/** @brief Borrow the immutable message while retaining the diagnostic; NULL gives EINVAL. */
const char *diagnostic_message(const diagnostic_t *diagnostic);

/** @brief Borrow a location, or NULL if absent; NULL diagnostic gives EINVAL. */
const diagnostic_location_t *diagnostic_location(const diagnostic_t *diagnostic);

/** @brief Borrow the immutable cause, or NULL if absent; retain separately to outlive the parent. */
const diagnostic_t *diagnostic_cause(const diagnostic_t *diagnostic);

/** @brief Return chain depth including this entry; NULL returns zero with EINVAL. */
size_t diagnostic_depth(const diagnostic_t *diagnostic);

/** @brief Format the complete chain as plain text, without a final newline; free the returned string_t. */
string_t *diagnostic_to_string(const diagnostic_t *diagnostic);

#endif
