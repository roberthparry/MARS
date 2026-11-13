/**
 * @file test_harness_text.c
 * @brief C-allocated diagnostic text for the independent test harness.
 *
 * The small append-only public sink lets domain adapters format their own values without requiring the
 * harness to link any MARS module. Allocation, checked growth and printf formatting use only the C library.
 */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_harness.h"
#define MARS_TEST_HARNESS_INTERNAL_ACCESS
#include "test_harness_internal.h"

struct _test_text_t {
    char *bytes;
    size_t length;
    size_t capacity;
};

test_text_t *test_text_new(void)
{
    return calloc(1, sizeof(test_text_t));
}

test_text_t *test_text_new_with(const char *text)
{
    test_text_t *out = test_text_new();
    if (out && test_text_append_cstr(out, text ? text : "")) {
        test_text_free(out);
        return NULL;
    }
    return out;
}

test_text_t *test_text_clone(const test_text_t *source)
{
    return test_text_new_with(test_text_c_str(source));
}

void test_text_free(test_text_t *text)
{
    if (text) free(text->bytes);
    free(text);
}

const char *test_text_c_str(const test_text_t *text)
{
    return text && text->bytes ? text->bytes : "";
}

size_t test_text_length(const test_text_t *text)
{
    return text ? text->length : 0;
}

void test_text_clear(test_text_t *text)
{
    if (!text) return;
    text->length = 0;
    if (text->bytes) text->bytes[0] = '\0';
}

int test_text_compare(const test_text_t *left, const test_text_t *right)
{
    return strcmp(test_text_c_str(left), test_text_c_str(right));
}

/* Append a domain formatter's ordinary C text without invoking the module being tested. */
int test_text_append_cstr(test_text_t *out, const char *text)
{
    if (!out || !text) return -1;
    size_t length = strlen(text);
    if (length > SIZE_MAX - out->length - 1) return -1;
    size_t required = out->length + length + 1;
    if (required > out->capacity) {
        size_t capacity = out->capacity > SIZE_MAX / 2 ? required : out->capacity * 2;
        if (capacity < required) capacity = required;
        if (capacity < 64) capacity = 64;
        char *grown = realloc(out->bytes, capacity);
        if (!grown) return -1;
        out->bytes = grown;
        out->capacity = capacity;
    }
    memcpy(out->bytes + out->length, text, length + 1);
    out->length += length;
    return 0;
}

test_text_t *test_text_vsprintf(const char *fmt, va_list ap)
{
    if (!fmt) return NULL;
    va_list copy;
    va_copy(copy, ap);
    int length = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (length < 0) return NULL;
    test_text_t *out = test_text_new();
    if (!out) return NULL;
    out->capacity = (size_t)length + 1;
    out->bytes = malloc(out->capacity);
    if (!out->bytes || vsnprintf(out->bytes, out->capacity, fmt, ap) != length) {
        test_text_free(out);
        return NULL;
    }
    out->length = (size_t)length;
    return out;
}

test_text_t *test_text_sprintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    test_text_t *out = test_text_vsprintf(fmt, ap);
    va_end(ap);
    return out;
}

/* Format a domain value with standard printf arguments into the caller-owned diagnostic sink. */
int test_text_append_format(test_text_t *out, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    test_text_t *text = test_text_vsprintf(fmt, ap);
    va_end(ap);
    int status = text ? test_text_append_cstr(out, test_text_c_str(text)) : -1;
    test_text_free(text);
    return status;
}
