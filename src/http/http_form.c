/**
 * @file http_form.c
 * @brief URL-encoded forms and multipart request bodies.
 *
 * Retains ordered form fields and constructs bounded multipart snapshots, including file parts. Request
 * preparation owns the resulting bytes so transfer does not depend on caller buffers remaining mutable.
 *
 * This is part of the synchronous http.h client. Keep transport limits, TLS policy and handle ownership consistent
 * with the shared client machinery; serving requests belongs to webserver.
 */

/* Ordered forms and bounded multipart snapshots. */
#include <sodium.h>
#include <stdlib.h>
#define MARS_HTTP_INTERNAL_ACCESS
#include "http_internal.h"

typedef struct {
    string_t *name, *value, *filename, *type, *path;
} form_part_t;
struct _http_form_t {
    array_t *parts;
};

static void part_free(void *value)
{
    form_part_t *p = value;
    string_free(p->name);
    string_free(p->value);
    string_free(p->filename);
    string_free(p->type);
    string_free(p->path);
}

/* Create an ordered form with independently owned parts. */
http_form_t *http_form_new(void)
{
    http_form_t *f = calloc(1, sizeof(*f));
    if (f)
        f->parts = array_create(sizeof(form_part_t), NULL, part_free);
    if (f && !f->parts) {
        free(f);
        return NULL;
    }
    return f;
}

/* Release every copied field and path. */
void http_form_free(http_form_t *f)
{
    if (f) {
        array_destroy(f->parts);
        free(f);
    }
}

static bool quoted(const string_t *text)
{
    return http_ascii_text(text, true) && string_find(text, "\"") < 0 && string_find(text, "\\") < 0;
}

/* Copy a text field without interpreting its spelling. */
bool http_form_add_text(http_form_t *f, const string_t *name, const string_t *value)
{
    if (!f || !name || !value)
        return false;
    form_part_t p = {.name = string_clone(name), .value = string_clone(value)};
    if (p.name && p.value && array_add(f->parts, &p))
        return true;
    part_free(&p);
    return false;
}

/* Retain safe multipart metadata and a copied file path. */
bool http_form_add_file(http_form_t *f, const string_t *name, const string_t *filename, const string_t *type,
                        const string_t *path)
{
    if (!f || !quoted(name) || !quoted(filename) || !http_ascii_text(type, true) || !path)
        return false;
    file_t *probe = file_new(path);
    if (!probe)
        return false;
    file_free(probe);
    form_part_t p = {.name = string_clone(name),
                     .filename = string_clone(filename),
                     .type = string_clone(type),
                     .path = string_clone(path)};
    if (p.name && p.filename && p.type && p.path && array_add(f->parts, &p))
        return true;
    part_free(&p);
    return false;
}

string_t *http_form_component(const string_t *text)
{
    string_t *encoded = http_url_encode(text), *out = string_new();
    string_cursor_t *c = encoded ? string_cursor_new(encoded) : NULL;
    bool ok = out && c;
    while (ok && !string_cursor_done(c)) {
        if (string_cursor_consume(c, "%20"))
            ok = string_append_char(out, '+') == 0;
        else if (string_cursor_consume(c, "%2A"))
            ok = string_append_char(out, '*') == 0;
        else if (string_cursor_consume(c, "~"))
            ok = string_append_format(out, "%%7E") >= 0;
        else {
            unsigned char ch;
            ok = string_cursor_peek_ascii(c, &ch) && string_append_char(out, (char)ch) == 0;
            string_cursor_next(c);
        }
    }
    string_cursor_free(c);
    string_free(encoded);
    if (!ok) {
        string_free(out);
        return NULL;
    }
    return out;
}

/* Encode ordered repeated fields using HTML form spelling. */
string_t *http_form_encode(const http_form_t *f)
{
    if (!f)
        return NULL;
    string_t *out = string_new();
    bool ok = out != NULL;
    for (size_t i = 0; ok && i < array_size(f->parts); ++i) {
        const form_part_t *p = array_get(f->parts, i);
        string_t *name = p->path ? NULL : http_form_component(p->name);
        string_t *value = p->value ? http_form_component(p->value) : NULL;
        ok = name && value && (!i || !string_append_char(out, '&')) &&
             string_append_format(out, "%S=%S", name, value) >= 0;
        string_free(name);
        string_free(value);
    }
    if (!ok) {
        string_free(out);
        return NULL;
    }
    return out;
}

/* Install a copied form body. */
bool http_request_set_form(http_request_t *r, const http_form_t *f)
{
    string_t *body = http_form_encode(f), *type = string_new_with("application/x-www-form-urlencoded");
    bool ok = body && type && http_request_set_text(r, body, type);
    string_free(body);
    string_free(type);
    return ok;
}

static bool append(array_t *out, const void *data, size_t n, size_t limit)
{
    return n <= limit - array_size(out) && (!n || array_append_carray(out, data, n));
}

static bool append_text(array_t *out, const string_t *text, size_t limit)
{
    return text && append(out, string_c_str(text), string_byte_length(text), limit);
}

/* Snapshot multipart bytes with an unpredictable delimiter and a strict total budget. */
bool http_request_set_multipart(http_request_t *r, const http_form_t *f, size_t limit)
{
    if (!r || !f || limit > 67108864 || sodium_init() < 0)
        return false;
    if (!limit)
        limit = 16777216;
    unsigned char random[24];
    char hex[49];
    randombytes_buf(random, sizeof(random));
    sodium_bin2hex(hex, sizeof(hex), random, sizeof(random));
    string_t *boundary = string_sprintf("mars-%s", hex);
    string_t *type = boundary ? string_sprintf("multipart/form-data; boundary=%S", boundary) : NULL;
    array_t *out = array_create(1, NULL, NULL);
    bool ok = out && type;
    for (size_t i = 0; ok && i < array_size(f->parts); ++i) {
        const form_part_t *p = array_get(f->parts, i);
        if (!quoted(p->name)) {
            ok = false;
            break;
        }
        string_t *head =
            p->path ? string_sprintf("--%S\r\nContent-Disposition: form-data; name=\"%S\"; filename=\"%S\"\r\n"
                                     "Content-Type: %S\r\n\r\n",
                                     boundary, p->name, p->filename, p->type)
                    : string_sprintf("--%S\r\nContent-Disposition: form-data; name=\"%S\"\r\n\r\n", boundary, p->name);
        ok = append_text(out, head, limit);
        string_free(head);
        if (ok && p->path) {
            file_t *file = file_new(p->path);
            ok = file && file_open_read(file);
            unsigned char buffer[8192];
            while (ok) {
                size_t n = 0;
                ok = file_read(file, buffer, sizeof(buffer), &n);
                if (!ok || !n)
                    break;
                ok = append(out, buffer, n, limit);
            }
            if (file && file_is_open(file) && !file_close(file))
                ok = false;
            file_free(file);
        } else if (ok) {
            ok = append_text(out, p->value, limit);
        }
        ok = ok && append(out, "\r\n", 2, limit);
    }
    string_t *end = boundary ? string_sprintf("--%S--\r\n", boundary) : NULL;
    ok = ok && append_text(out, end, limit) && http_request_set_body(r, array_get(out, 0), array_size(out), type);
    string_free(end);
    string_free(type);
    string_free(boundary);
    array_destroy(out);
    return ok;
}
