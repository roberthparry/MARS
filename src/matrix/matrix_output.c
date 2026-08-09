#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "matrix.h"
#include "ustring.h"

static mat_string_style_t mo_matrix_style(int scientific, int layout)
{
    if (scientific)
        return layout ? MAT_STRING_LAYOUT_SCIENTIFIC : MAT_STRING_INLINE_SCIENTIFIC;
    return layout ? MAT_STRING_LAYOUT_PRETTY : MAT_STRING_INLINE_PRETTY;
}

static int mo_append_padding(string_t *out, int count)
{
    for (int i = 0; i < count; ++i) {
        if (string_append_char(out, ' ') != 0)
            return -1;
    }
    return 0;
}

static string_format_result_t mo_format_callback(string_t *out, const string_format_spec_t *spec, va_list ap,
                                                 void *user)
{
    bool scientific;
    bool layout;
    bool left;
    int width;
    const matrix_t *A;
    string_t *text;
    size_t text_len;
    int pad;
    string_format_result_t result = STRING_FORMAT_HANDLED;

    (void)user;

    if (!out || !spec || (spec->conversion != 'm' && spec->conversion != 'M'))
        return STRING_FORMAT_UNHANDLED;

    width = spec->width;
    left = spec->flag_left;
    if (spec->width_from_argument) {
        width = va_arg(ap, int);
        if (width < 0) {
            left = true;
            width = -width;
        }
    }
    if (spec->precision_from_argument)
        (void)va_arg(ap, int);

    scientific = spec->conversion == 'M';
    layout = spec->trailing_modifier == 'l' || spec->trailing_modifier == 'L';
    if (layout)
        result = STRING_FORMAT_HANDLED_WITH_TRAILING_MODIFIER;

    A = va_arg(ap, const matrix_t *);
    text = mat_to_text(A, mo_matrix_style(scientific, layout));
    if (!text)
        return STRING_FORMAT_ERROR;

    text_len = string_length(text);
    pad = width > (int)text_len ? width - (int)text_len : 0;

    if (!left && mo_append_padding(out, pad) != 0)
        goto fail;
    if (string_append_string(out, text) != 0)
        goto fail;
    if (left && mo_append_padding(out, pad) != 0)
        goto fail;

    string_free(text);
    return result;

fail:
    string_free(text);
    return STRING_FORMAT_ERROR;
}

string_t *mat_vsprintf_text(const char *fmt, va_list ap)
{
    return string_vsprintf_with_callback(fmt, ap, mo_format_callback, NULL);
}

int mat_sprintf(char *out, size_t out_size, const char *fmt, ...)
{
    int n;
    va_list ap;
    string_t *text;
    size_t len;

    va_start(ap, fmt);
    text = mat_vsprintf_text(fmt, ap);
    va_end(ap);
    if (!text)
        return -1;

    len = string_view_length(string_view_all(text));
    if (out && out_size > 0u) {
        size_t copy_len = len < out_size - 1u ? len : out_size - 1u;

        memcpy(out, string_c_str(text), copy_len);
        out[copy_len] = '\0';
    }

    n = len <= (size_t)INT_MAX ? (int)len : -1;
    string_free(text);
    return n;
}

string_t *mat_sprintf_text(const char *fmt, ...)
{
    va_list ap;
    string_t *text;

    va_start(ap, fmt);
    text = mat_vsprintf_text(fmt, ap);
    va_end(ap);
    return text;
}

int mat_printf(const char *fmt, ...)
{
    va_list ap;
    int written;
    string_t *text;

    va_start(ap, fmt);
    text = mat_vsprintf_text(fmt, ap);
    va_end(ap);
    if (!text)
        return -1;

    written = string_printf("%S", text);
    string_free(text);
    return written;
}

void mat_print(const matrix_t *A)
{
    if (mat_printf("%ml\n", A) < 0)
        string_printf("(null)\n");
}
