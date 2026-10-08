/**
 * @file tba_http_form.c
 * @brief Bounded URL-encoded form and multipart CSV upload decoding.
 *
 * Parses using MARS string cursors and preserves repeated checkbox selections.
 * Multipart requests require exactly one named file part and a closing boundary.
 * Uploaded filenames are display metadata only; storage generates its own names.
 */
#include <stdlib.h>
#include <string.h>

#include "tba_http.h"

static int tba_http_hex(unsigned char ch)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

static string_t *tba_http_decode(const string_t *source)
{
    size_t size = string_byte_length(source), used = 0;
    char *bytes = malloc(size + 1);
    string_cursor_t *cursor = string_cursor_new(source);
    bool ok = bytes && cursor;
    while (ok && !string_cursor_done(cursor)) {
        unsigned char ch;
        ok = string_cursor_peek_ascii(cursor, &ch) && ch != 0;
        if (!ok)
            break;
        string_cursor_next(cursor);
        if (ch == '%') {
            unsigned char a, b;
            ok = string_cursor_peek_ascii(cursor, &a);
            string_cursor_next(cursor);
            ok = ok && string_cursor_peek_ascii(cursor, &b);
            string_cursor_next(cursor);
            ok = ok && tba_http_hex(a) >= 0 && tba_http_hex(b) >= 0;
            if (!ok)
                break;
            ch = (unsigned char)(tba_http_hex(a) * 16 + tba_http_hex(b));
        } else if (ch == '+')
            ch = ' ';
        ok = ch != 0;
        bytes[used++] = (char)ch;
    }
    string_t *result = ok ? string_new() : NULL;
    if (result && string_append_utf8_exact(result, bytes, used)) {
        string_free(result);
        result = NULL;
    }
    string_cursor_free(cursor);
    free(bytes);
    return result;
}

/* Decode form fields, including repeated model/driver/outlier checkbox values. */
json_t *tba_http_form(const string_t *text)
{
    if (!text || string_byte_length(text) > 1024 * 1024)
        return NULL;
    size_t count = 0;
    string_t **fields = string_split(text, "&", &count);
    json_t *result = count <= 1024 ? json_new_object() : NULL;
    for (size_t i = 0; result && i < count; ++i) {
        string_offset_t equal = string_find(fields[i], "=");
        string_t *raw_key = string_substr(fields[i], 0, equal < 0 ? string_byte_length(fields[i]) : (size_t)equal);
        string_t *raw_value = equal < 0 ? string_new() : string_substr(fields[i], (size_t)equal + 1, string_byte_length(fields[i]));
        string_t *key = raw_key ? tba_http_decode(raw_key) : NULL, *value = raw_value ? tba_http_decode(raw_value) : NULL;
        bool ok = key && value;
        const char *name = key ? string_c_str(key) : "";
        const char *group = !strcmp(name, "xreg-column-choice") ? "xreg_columns" :
                            !strcmp(name, "outlier-choice") ? "outlier_dates" :
                            !strcmp(name, "model-choice") ? "models" : NULL;
        string_t *combined = NULL;
        if (group) {
            const char *previous = tba_json_text(result, group);
            combined = string_sprintf("%s%s%S", previous, *previous ? ", " : "", value);
            ok = ok && combined && tba_json_string(result, group, string_c_str(combined));
        } else if (*name)
            ok = ok && tba_json_string(result, name, string_c_str(value));
        string_free(combined);
        string_free(raw_key);
        string_free(raw_value);
        string_free(key);
        string_free(value);
        if (!ok) {
            json_free(result);
            result = NULL;
        }
    }
    string_split_free(fields, count);
    return result;
}

static string_t *tba_http_parameter(const string_t *header, const char *wanted)
{
    string_cursor_t *cursor = header ? string_cursor_new(header) : NULL;
    string_t *result = NULL;
    while (cursor && !string_cursor_done(cursor)) {
        if (!string_cursor_consume(cursor, ";")) {
            string_cursor_next(cursor);
            continue;
        }
        string_cursor_skip_spaces(cursor);
        bool match = string_cursor_consume(cursor, wanted);
        if (!match || !string_cursor_consume(cursor, "="))
            continue;
        bool quoted = string_cursor_consume(cursor, "\"");
        string_pos_t start = string_cursor_position(cursor);
        unsigned char ch;
        while (!string_cursor_done(cursor)) {
            if (string_cursor_peek_ascii(cursor, &ch) && (quoted ? ch == '"' : ch == ';' || ch == ' '))
                break;
            string_cursor_next(cursor);
        }
        result = string_cursor_extract(start, cursor);
        if (quoted && !string_cursor_consume(cursor, "\"")) {
            string_free(result);
            result = NULL;
        }
        break;
    }
    string_cursor_free(cursor);
    return result;
}

/* Extract one CSV upload with explicit framing and size checks. */
string_t *tba_http_upload(const websrv_request_t *request, string_t **filename)
{
    *filename = NULL;
    const string_t *type = tba_http_header(request, "Content-Type");
    if (!type || !string_starts_with(type, "multipart/form-data;") ||
        websrv_request_body_size(request) > 17u * 1024u * 1024u)
        return NULL;
    string_t *boundary = tba_http_parameter(type, "boundary");
    if (!boundary || !string_byte_length(boundary) || string_byte_length(boundary) > 70 ||
        string_find(boundary, "\r") >= 0 || string_find(boundary, "\n") >= 0) {
        string_free(boundary);
        return NULL;
    }
    string_t *body = websrv_request_text(request);
    string_t *opening = string_sprintf("--%S\r\n", boundary);
    string_t *closing = string_sprintf("\r\n--%S--", boundary);
    string_t *result = NULL;
    if (body && opening && closing && string_starts_with_string(body, opening)) {
        string_offset_t split = string_find(body, "\r\n\r\n");
        string_offset_t end = string_find(body, string_c_str(closing));
        if (split > 0 && split < 16384 && end > split + 4 && (size_t)(end - split - 4) <= 16u * 1024u * 1024u) {
            string_t *headers = string_substr(body, string_byte_length(opening), (size_t)split - string_byte_length(opening));
            string_t *name = tba_http_parameter(headers, "name");
            string_t *original = tba_http_parameter(headers, "filename");
            if (name && original && string_view_equals_literal(string_view_all(name), "file")) {
                result = string_substr(body, (size_t)split + 4, (size_t)(end - split - 4));
                *filename = original;
                original = NULL;
            }
            string_free(name);
            string_free(original);
            string_free(headers);
        }
    }
    string_free(body);
    string_free(opening);
    string_free(closing);
    string_free(boundary);
    return result;
}

/* Borrow a request header through a transient key. */
const string_t *tba_http_header(const websrv_request_t *request, const char *name)
{
    string_t *key = string_new_with(name);
    const string_t *value = key ? websrv_request_header(request, key) : NULL;
    string_free(key);
    return value;
}

/* Copy a response header and release transient string arguments. */
bool tba_http_header_set(websrv_response_t *response, const char *name, const char *value)
{
    string_t *key = string_new_with(name), *text = string_new_with(value);
    bool ok = key && text && websrv_response_header(response, key, text);
    string_free(key);
    string_free(text);
    return ok;
}
