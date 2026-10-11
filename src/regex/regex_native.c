/**
 * @file regex_native.c
 * @brief Linux libc compilation, matching and per-handle UTF-8 locale ownership.
 *
 * This is the only MARS translation unit that uses POSIX regex_t directly.
 * Native errors and match offsets cross the private adapter as MARS strings
 * and size_t values; the public opaque type is never included here.
 */
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <regex.h>
#include <stdint.h>
#include <stdlib.h>

#include "regex_native.h"

struct _regex_native_t {
    regex_t expression;
    locale_t locale;
    bool multiline;
};

static int regex_native_fail(int code, const char *message, string_t **error)
{
    if (error)
        *error = string_new_with(message);
    errno = code;
    return -1;
}

static int regex_native_error(int status, string_t **error)
{
    if (error) {
        size_t size = regerror(status, NULL, NULL, 0);
        char *message = malloc(size);
        if (message) {
            regerror(status, NULL, message, size);
            *error = string_new_with(message);
            free(message);
        }
    }
    errno = status == REG_ESPACE ? ENOMEM : EINVAL;
    return -1;
}

/* Compile and execute under the same thread-local locale, without setlocale(). */
regex_native_t *regex_native_new(const string_t *pattern, bool ignore_case, bool multiline, string_t **error)
{
    regex_native_t *native = calloc(1, sizeof(*native));
    if (!native) {
        regex_native_fail(ENOMEM, "Allocating regular expression failed", error);
        return NULL;
    }
    native->locale = newlocale(LC_CTYPE_MASK | LC_COLLATE_MASK, "C.UTF-8", (locale_t)0);
    if (!native->locale) {
        int code = errno;
        free(native);
        regex_native_fail(code, "C.UTF-8 locale is unavailable", error);
        return NULL;
    }
    locale_t previous = uselocale(native->locale);
    if (!previous) {
        int code = errno;
        freelocale(native->locale);
        free(native);
        regex_native_fail(code, "Selecting the regular-expression locale failed", error);
        return NULL;
    }
    int flags = REG_EXTENDED | (ignore_case ? REG_ICASE : 0) | (multiline ? REG_NEWLINE : 0);
    int status = regcomp(&native->expression, string_c_str(pattern), flags);
    if (status)
        regex_native_error(status, error);
    int code = errno;
    uselocale(previous);
    if (status) {
        freelocale(native->locale);
        free(native);
        errno = code;
        return NULL;
    }
    native->multiline = multiline;
    return native;
}

/* The expression owns its compiled storage and locale, not any input strings. */
void regex_native_free(regex_native_t *native)
{
    if (!native)
        return;
    regfree(&native->expression);
    freelocale(native->locale);
    free(native);
}

/* POSIX records this count during successful compilation. */
size_t regex_native_groups(const regex_native_t *native)
{
    return native->expression.re_nsub;
}

/* REG_STARTEND keeps all returned offsets relative to the original subject. */
int regex_native_search(const regex_native_t *native, const string_t *subject, size_t start,
                        size_t *starts, size_t *ends, size_t count, string_t **error)
{
    size_t length = string_byte_length(subject);
    if (length > INT_MAX)
        return regex_native_fail(EOVERFLOW, "Subject exceeds native regex offset range", error);
    regmatch_t local[16];
    size_t slots = count ? count : 1;
    regmatch_t *matches = slots <= 16 ? local : malloc(slots * sizeof(*matches));
    if (!matches)
        return regex_native_fail(ENOMEM, "Allocating match offsets failed", error);
    matches[0] = (regmatch_t){.rm_so = (regoff_t)start, .rm_eo = (regoff_t)length};
    unsigned char previous_byte = 0;
    bool after_newline = start && string_view_peek_ascii(string_view_all(subject), start - 1, &previous_byte) &&
                         previous_byte == '\n';
    int flags = REG_STARTEND | (start && !(native->multiline && after_newline) ? REG_NOTBOL : 0);
    locale_t previous = uselocale(native->locale);
    int status = -1;
    if (!previous)
        regex_native_fail(errno, "Selecting the regular-expression locale failed", error);
    else {
        int result = regexec(&native->expression, string_c_str(subject), slots, matches, flags);
        status = result == 0 ? 1 : result == REG_NOMATCH ? 0 : regex_native_error(result, error);
        int code = errno;
        uselocale(previous);
        errno = code;
    }
    if (status == 1) {
        for (size_t i = 0; i < count; ++i) {
            starts[i] = matches[i].rm_so < 0 ? SIZE_MAX : (size_t)matches[i].rm_so;
            ends[i] = matches[i].rm_eo < 0 ? SIZE_MAX : (size_t)matches[i].rm_eo;
        }
    }
    if (matches != local)
        free(matches);
    return status;
}
