/* Public expression conversion, formatting and serialisation entry points.
 * Body rendering lives in the expr_stringout_* implementation units; style wrappers
 * retain binding envelopes and executable declarations. Rendering preserves the DAG.
 */

#include <ctype.h>
#include <gmp.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expr_bindings.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"
#include "expr_stringout.h"
#define MARS_EXPR_STRINGOUT_INTERNAL_ACCESS
#include "expr_stringout_internal.h"
#include "expression.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"
#include "ustring.h"

static int expr_append_padding(string_t *out, int count)
{
    for (int i = 0; i < count; ++i) {
        if (string_append_char(out, ' ') != 0)
            return -1;
    }
    return 0;
}

static style_t expr_format_style(const string_format_spec_t *spec, string_format_result_t *result)
{
    if (!spec || !result)
        return style_EXPRESSION;

    switch (spec->trailing_modifier) {
        case 'u':
        case 'U':
            *result = STRING_FORMAT_HANDLED_WITH_TRAILING_MODIFIER;
            return style_UNBOUND;
        case 't':
        case 'T':
            *result = STRING_FORMAT_HANDLED_WITH_TRAILING_MODIFIER;
            return style_LATEX;
        case 'f':
        case 'F':
            *result = STRING_FORMAT_HANDLED_WITH_TRAILING_MODIFIER;
            return style_FUNCTION;
        default:
            return style_EXPRESSION;
    }
}

static string_format_result_t expr_format_callback(string_t *out, const string_format_spec_t *spec, va_list ap,
                                                   void *user)
{
    bool left;
    bool old_scientific;
    bool scientific;
    int width;
    int old_precision;
    int precision;
    int pad;
    size_t text_len;
    const expr_t *expr;
    string_t *text;
    style_t style;
    string_format_result_t result = STRING_FORMAT_HANDLED;

    (void)user;

    if (!out || !spec || (spec->conversion != 'n' && spec->conversion != 'N'))
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
    precision = spec->precision;
    if (spec->precision_from_argument) {
        precision = va_arg(ap, int);
        if (precision < 0)
            precision = -1;
    }

    style = expr_format_style(spec, &result);
    scientific = spec->conversion == 'N';
    expr = va_arg(ap, const expr_t *);
    old_scientific = expr_set_number_scientific_local(scientific);
    old_precision = expr_set_number_precision_local(precision);
    text = expr_to_text(expr, style);
    expr_set_number_precision_local(old_precision);
    expr_set_number_scientific_local(old_scientific);
    if (!text)
        return STRING_FORMAT_ERROR;

    text_len = string_length(text);
    pad = width > (int)text_len ? width - (int)text_len : 0;

    if (!left && expr_append_padding(out, pad) != 0)
        goto fail;
    if (string_append_string(out, text) != 0)
        goto fail;
    if (left && expr_append_padding(out, pad) != 0)
        goto fail;

    string_free(text);
    return result;

fail:
    string_free(text);
    return STRING_FORMAT_ERROR;
}


static string_t *expr_to_TeX_text(const expr_t *expr)
{
    char *expression_text = NULL;
    char *bindings = NULL;
    sbuf_t b;
    string_t *out;

    if (expr_to_TeX_parts(expr, &expression_text, &bindings) != 0)
        return expr_to_text_expr(expr);

    sbuf_init(&b);
    if (bindings && *bindings) {
        sbuf_puts(&b, "\\left\\{ ");
        sbuf_puts(&b, expression_text);
        sbuf_puts(&b, " \\;\\middle|\\; ");
        sbuf_puts(&b, bindings);
        sbuf_puts(&b, " \\right\\}");
    } else {
        sbuf_puts(&b, expression_text);
    }

    free(expression_text);
    free(bindings);
    out = sbuf_to_string(&b);
    sbuf_free(&b);
    return out;
}

static bool expr_is_trailing_display_space(rune_t rune)
{
    return rune_is_equal(rune, '\n') || rune_is_equal(rune, '\r') || rune_is_equal(rune, ' ') ||
           rune_is_equal(rune, '\t');
}

static string_t *expr_trim_trailing_display_space(string_t *text)
{
    size_t len;
    string_t *trimmed;

    if (!text)
        return NULL;

    len = string_length(text);
    while (len > 0u && expr_is_trailing_display_space(string_at(text, len - 1u)))
        len--;

    if (len == string_length(text))
        return text;

    trimmed = string_substring(text, 0u, len);
    string_free(text);
    return trimmed;
}

string_t *expr_to_text(const expr_t *expr, style_t style)
{
    string_t *text;

    expr_init_singletons();
    if (!expr)
        return string_new_with("NULL");

    if (style == style_LATEX) {
        text = expr_to_TeX_text(expr);
    } else if (style == style_FUNCTION) {
        text = expr_to_text_function(expr);
    } else if (style == style_UNBOUND) {
        text = expr_to_text_unbound(expr);
    } else {
        text = expr_to_text_expr(expr);
    }

    return expr_trim_trailing_display_space(text);
}

char *expr_to_string(const expr_t *expr, style_t style)
{
    string_t *text = expr_to_text(expr, style);
    const char *src;
    size_t len;
    char *out;

    if (!text)
        return NULL;

    src = string_c_str(text);
    len = strlen(src) + 1u;
    out = malloc(len);
    if (out)
        memcpy(out, src, len);
    string_free(text);
    return out;
}

string_t *expr_vsprintf_text(const char *fmt, va_list ap)
{
    return string_vsprintf_with_callback(fmt, ap, expr_format_callback, NULL);
}

string_t *expr_sprintf_text(const char *fmt, ...)
{
    va_list ap;
    string_t *text;

    va_start(ap, fmt);
    text = expr_vsprintf_text(fmt, ap);
    va_end(ap);
    return text;
}

int expr_sprintf(char *out, size_t out_size, const char *fmt, ...)
{
    int n;
    va_list ap;
    string_t *text;
    size_t len;

    va_start(ap, fmt);
    text = expr_vsprintf_text(fmt, ap);
    va_end(ap);
    if (!text)
        return -1;

    len = string_length(text);
    if (out && out_size > 0u) {
        size_t copy_len = len < out_size - 1u ? len : out_size - 1u;

        memcpy(out, string_c_str(text), copy_len);
        out[copy_len] = '\0';
    }

    n = len <= (size_t)INT_MAX ? (int)len : -1;
    string_free(text);
    return n;
}

int expr_printf(const char *fmt, ...)
{
    va_list ap;
    int written;
    string_t *text;

    va_start(ap, fmt);
    text = expr_vsprintf_text(fmt, ap);
    va_end(ap);
    if (!text)
        return -1;

    written = string_printf("%S", text);
    string_free(text);
    return written;
}

void expr_print(const expr_t *expr)
{
    if (expr_printf("%n\n", expr) < 0)
        string_printf("NULL\n");
}

bool expr_serialize(const expr_t *expr, string_t **out_type, string_t **out_encoding, void **out_data, size_t *out_len)
{
    string_t *type = NULL;
    string_t *encoding = NULL;
    string_t *text = NULL;
    void *payload = NULL;

    if (!expr || !out_type || !out_encoding || !out_data || !out_len)
        return false;

    text = expr_to_text(expr, style_EXPRESSION);
    if (!text)
        return false;

    payload = malloc(string_byte_length(text));
    if (!payload) {
        string_free(text);
        return false;
    }
    memcpy(payload, string_c_str(text), string_byte_length(text));

    type = string_new_with("expr_t");
    encoding = string_new_with("mars/expression");
    if (!type || !encoding) {
        free(payload);
        string_free(text);
        string_free(type);
        string_free(encoding);
        return false;
    }

    *out_type = type;
    *out_encoding = encoding;
    *out_data = payload;
    *out_len = string_byte_length(text);
    string_free(text);
    return true;
}

expr_t *expr_deserialise(const void *data, size_t len, const string_t *type, const string_t *encoding)
{
    string_t *text;
    expr_t *expr;

    if (!data || !type || !encoding)
        return NULL;
    if (strcmp(string_c_str(type), "expr_t") != 0 || strcmp(string_c_str(encoding), "mars/expression") != 0)
        return NULL;

    text = string_new();
    if (!text)
        return NULL;
    if (string_append_chars(text, (const char *)data, len) != 0) {
        string_free(text);
        return NULL;
    }
    expr = expr_from_text(text, NULL);
    string_free(text);
    return expr;
}
