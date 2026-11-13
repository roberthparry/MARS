/**
 * @file test_harness_internal.h
 * @brief Private C diagnostic-buffer facilities for the independent sequential harness.
 *
 * Only the harness implementation may include this header. Buffers use the C allocator and standard printf
 * formatting; they never call a library module under test. Suites use test_harness.h and its append-only sink.
 */
#ifndef MARS_TEST_HARNESS_INTERNAL_H
#define MARS_TEST_HARNESS_INTERNAL_H
#if !defined(MARS_TEST_HARNESS_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private harness access requires MARS_TEST_HARNESS_INTERNAL_ACCESS."
#endif

#include <stdarg.h>

#include "test_harness.h"

/**
 * @brief Allocate an empty owned buffer; return NULL on allocation failure. Release with test_text_free.
 * @return Owned buffer released with test_text_free, or NULL on failure.
 * @note Private sequential core facility; never calls a MARS module.
 */
test_text_t *test_text_new(void);

/**
 * @brief Copy text, treating NULL as empty; return an owned buffer or NULL on allocation failure.
 * @param[in] text Buffer or C text as described above; ownership changes only for test_text_free.
 * @return Owned buffer released with test_text_free, or NULL on failure.
 * @note Private sequential core facility; never calls a MARS module.
 */
test_text_t *test_text_new_with(const char *text);

/**
 * @brief Copy source without consuming it; return an owned buffer or NULL on allocation failure.
 * @param[in] source Caller-owned source buffer, valid during this call.
 * @return Owned buffer released with test_text_free, or NULL on failure.
 * @note Private sequential core facility; never calls a MARS module.
 */
test_text_t *test_text_clone(const test_text_t *source);

/**
 * @brief Release owned text and its bytes; NULL is accepted. No other thread may access it during release.
 * @param[in] text Buffer or C text as described above; ownership changes only for test_text_free.
 * @return No value.
 * @note Private sequential core facility; never calls a MARS module.
 */
void test_text_free(test_text_t *text);

/**
 * @brief Return text's owned, NUL-terminated bytes, valid until mutation/release; NULL yields an empty string.
 * @param[in] text Buffer or C text as described above; ownership changes only for test_text_free.
 * @return Buffer-owned text, valid until mutation/release; NULL input yields an empty string.
 * @note Private sequential core facility; never calls a MARS module.
 */
const char *test_text_c_str(const test_text_t *text);

/**
 * @brief Return the byte length of text, or zero for NULL; no ownership changes.
 * @param[in] text Buffer or C text as described above; ownership changes only for test_text_free.
 * @return Byte count, or zero for NULL.
 * @note Private sequential core facility; never calls a MARS module.
 */
size_t test_text_length(const test_text_t *text);

/**
 * @brief Empty text without releasing capacity; NULL is accepted.
 * @param[in] text Buffer or C text as described above; ownership changes only for test_text_free.
 * @return No value.
 * @note Private sequential core facility; never calls a MARS module.
 */
void test_text_clear(test_text_t *text);

/**
 * @brief Compare left and right lexicographically, treating NULL as empty; return a strcmp-style result.
 * @param[in] left Caller-owned left buffer; NULL is treated as empty.
 * @param[in] right Caller-owned right buffer; NULL is treated as empty.
 * @return Negative, zero or positive according to strcmp ordering.
 * @note Private sequential core facility; never calls a MARS module.
 */
int test_text_compare(const test_text_t *left, const test_text_t *right);

/**
 * @brief Format fmt/ap with standard printf syntax; return an owned buffer or NULL on formatting/allocation failure.
 * @param[in] fmt Caller-owned standard printf format, valid during this call.
 * @param[in] ap Printf arguments, consumed as by vsnprintf; the caller must call va_end.
 * @return Owned buffer released with test_text_free, or NULL on failure.
 * @note Private sequential core facility; never calls a MARS module.
 */
test_text_t *test_text_vsprintf(const char *fmt, va_list ap);

/**
 * @brief Format fmt/arguments with standard printf syntax; return an owned buffer or NULL on failure.
 * @param[in] fmt Caller-owned standard printf format, valid during this call.
 * @param[in] undefined Arguments matching fmt, copied during this call.
 * @return Owned buffer released with test_text_free, or NULL on failure.
 * @note Private sequential core facility; never calls a MARS module.
 */
test_text_t *test_text_sprintf(const char *fmt, ...);

#endif
