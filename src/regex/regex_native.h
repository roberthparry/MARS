/**
 * @file regex_native.h
 * @brief Private Linux regex adapter boundary, independent of either regex_t typedef.
 *
 * Used only by the regex module. The native translation unit includes the system
 * regex.h, while the public-facing unit includes regex-t.h; neither needs to rename
 * or expose the other's type. Public callers must use regex-t.h instead.
 */
#ifndef MARS_REGEX_NATIVE_H
#define MARS_REGEX_NATIVE_H

#include <stdbool.h>
#include <stddef.h>

#include "string-t.h"

/** @brief Private compiled libc pattern and owned locale. */
typedef struct _regex_native_t regex_native_t;

/** @brief Compile validated UTF-8. @param pattern Borrowed pattern. @param ignore_case Case policy.
 * @param multiline Line policy. @param error Owned diagnostic output. @return Owned adapter or NULL. */
regex_native_t *regex_native_new(const string_t *pattern, bool ignore_case, bool multiline, string_t **error);

/** @brief Release adapter state. @param native Owned adapter, or NULL. */
void regex_native_free(regex_native_t *native);

/** @brief Count groups. @param native Valid borrowed adapter. @return Parenthesised group count. */
size_t regex_native_groups(const regex_native_t *native);

/** @brief Search validated text. @param native Valid adapter. @param subject Borrowed UTF-8 text.
 * @param start Valid byte offset. @param starts Output starts; SIZE_MAX denotes an absent group.
 * @param ends Output ends. @param count Both array lengths, or zero. @param error Owned diagnostic output.
 * @return 1 for match, 0 for no match, -1 for failure with errno set. */
int regex_native_search(const regex_native_t *native, const string_t *subject, size_t start,
                        size_t *starts, size_t *ends, size_t count, string_t **error);

#endif
