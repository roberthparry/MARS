/**
 * @file tba_http_upload.c
 * @brief Bounded multipart CSV upload decoding.
 *
 * Parses multipart upload headers using MARS string cursors.
 * Multipart requests require exactly one named file part and a closing boundary.
 * Uploaded filenames are display metadata only; storage generates its own names.
 */
#include <stdlib.h>
#include <string.h>

#include "tba_http.h"


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
