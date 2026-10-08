/**
 * @file tba_support.c
 * @brief JSON ownership helpers and string-based scalar/markup conversion.
 *
 * Shared by native forecasting modules. Decimal parsing consumes string cursors
 * and rejects partial/non-finite input; markup escaping never treats data as HTML.
 */
#include <math.h>
#include <string.h>

#include "tba_support.h"

/* Borrow a keyed JSON member. */
const json_t *tba_json_get(const json_t *object, const char *key)
{
    string_t *name = string_new_with(key);
    const json_t *value = name ? json_object_get(object, name) : NULL;
    string_free(name);
    return value;
}

/* Borrow textual scalar storage. */
const char *tba_json_text(const json_t *object, const char *key)
{
    const json_t *value = tba_json_get(object, key);
    const string_t *text = json_type(value) == JSON_NUMBER ? json_number_text(value) : json_string_value(value);
    return text ? string_c_str(text) : "";
}

/* Read a boolean without coercion. */
bool tba_json_bool(const json_t *object, const char *key)
{
    bool value = false;
    json_bool_value(tba_json_get(object, key), &value);
    return value;
}

/* Copy a member while retaining caller ownership. */
bool tba_json_set(json_t *object, const char *key, const json_t *value)
{
    string_t *name = string_new_with(key);
    bool ok = name && value && json_object_set(object, name, value);
    string_free(name);
    return ok;
}

/* Copy a string member. */
bool tba_json_string(json_t *object, const char *key, const char *text)
{
    string_t *s = string_new_with(text ? text : "");
    json_t *value = s ? json_new_string(s) : NULL;
    bool ok = tba_json_set(object, key, value);
    json_free(value);
    string_free(s);
    return ok;
}

/* Store a boolean member. */
bool tba_json_flag(json_t *object, const char *key, bool flag)
{
    json_t *value = json_new_bool(flag);
    bool ok = tba_json_set(object, key, value);
    json_free(value);
    return ok;
}

/* Store a finite decimal without locale-dependent parsing. */
bool tba_json_number(json_t *object, const char *key, double number)
{
    string_t *text = isfinite(number) ? string_sprintf("%.17g", number) : NULL;
    json_t *value = text ? json_new_number(text) : NULL;
    bool ok = tba_json_set(object, key, value);
    json_free(value);
    string_free(text);
    return ok;
}

/* Append an owned copy of text. */
bool tba_json_append(json_t *array, const char *text)
{
    string_t *s = string_new_with(text ? text : "");
    json_t *value = s ? json_new_string(s) : NULL;
    bool ok = value && json_array_append(array, value);
    json_free(value);
    string_free(s);
    return ok;
}

/* Construct the shared error response shape. */
json_t *tba_json_error(const char *message)
{
    json_t *result = json_new_object();
    if (!result || !tba_json_flag(result, "ok", false) || !tba_json_string(result, "error", message)) {
        json_free(result);
        return NULL;
    }
    return result;
}

/* Parse decimal syntax using a bounded string cursor. */
bool tba_text_number(const string_t *text, double *value)
{
    if (!text || !value || string_byte_length(text) > 128)
        return false;
    string_cursor_t *cursor = string_cursor_new(text);
    if (!cursor)
        return false;
    string_cursor_skip_spaces(cursor);
    bool negative = string_cursor_consume(cursor, "-");
    if (!negative)
        string_cursor_consume(cursor, "+");
    long double number = 0, scale = 1;
    bool digits = false, point = false;
    unsigned char ch;
    while (string_cursor_peek_ascii(cursor, &ch)) {
        if (ch == '.' && !point) {
            point = true;
        } else if (ch >= '0' && ch <= '9') {
            digits = true;
            number = number * 10 + ch - '0';
            if (point)
                scale *= 10;
        } else
            break;
        string_cursor_next(cursor);
    }
    int exponent = 0;
    bool exponent_ok = true;
    if (string_cursor_consume(cursor, "e") || string_cursor_consume(cursor, "E")) {
        bool minus = string_cursor_consume(cursor, "-");
        if (!minus)
            string_cursor_consume(cursor, "+");
        exponent_ok = false;
        while (string_cursor_peek_ascii(cursor, &ch) && ch >= '0' && ch <= '9' && exponent < 10000) {
            exponent = exponent * 10 + ch - '0';
            exponent_ok = true;
            string_cursor_next(cursor);
        }
        if (minus)
            exponent = -exponent;
    }
    string_cursor_skip_spaces(cursor);
    bool ok = digits && exponent_ok && string_cursor_done(cursor);
    double parsed = (double)((negative ? -number : number) / scale * powl(10, exponent));
    string_cursor_free(cursor);
    if (ok && isfinite(parsed))
        *value = parsed;
    return ok && isfinite(parsed);
}

/* Escape text in the order which prevents double escaping. */
string_t *tba_text_html(const char *text)
{
    string_t *result = string_new_with(text ? text : "");
    if (result && (string_replace(result, "&", "&amp;") < 0 || string_replace(result, "<", "&lt;") < 0 ||
                   string_replace(result, ">", "&gt;") < 0 || string_replace(result, "\"", "&quot;") < 0 ||
                   string_replace(result, "'", "&#39;") < 0)) {
        string_free(result);
        return NULL;
    }
    return result;
}

/* Protect HTML script delimiters while preserving valid JSON. */
string_t *tba_text_script(const json_t *value)
{
    string_t *text = json_to_string(value);
    if (text && (string_replace(text, "<", "\\u003c") < 0 || string_replace(text, ">", "\\u003e") < 0 ||
                 string_replace(text, "&", "\\u0026") < 0)) {
        string_free(text);
        return NULL;
    }
    return text;
}
