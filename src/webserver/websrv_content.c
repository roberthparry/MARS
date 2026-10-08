/**
 * @file websrv_content.c
 * @brief Web-server headers, bodies and document adapters.
 *
 * Validates dictionary-backed response fields and manages binary, text, JSON and XML bodies. This unit prepares
 * content independently of listener routing and socket framing.
 *
 * This is part of webserver.h's synchronous Linux listener. The HTTP client is a separate module; this
 * implementation does not supply a production worker pool or TLS terminator.
 */

/* String-backed header validation, binary bodies and native document adapters. */
#include <stdlib.h>
#include <string.h>

#include "websrv_internal.h"

static size_t key_hash(const void *key) { return string_hash(*(string_t *const *)key); }
static int key_compare(const void *a, const void *b)
{
    return string_compare(*(string_t *const *)a, *(string_t *const *)b);
}

dictionary_t *websrv_dict_new(size_t value_size)
{
    return dictionary_create(sizeof(string_t *), value_size, key_hash, key_compare, NULL, NULL, NULL, NULL, NULL);
}

void websrv_headers_free(dictionary_t *headers)
{
    for (size_t i = 0; i < dictionary_size(headers); ++i) {
        string_free(*(string_t *const *)dictionary_get_key(headers, i));
        string_free(*(string_t *const *)dictionary_get_value(headers, i));
    }
    dictionary_destroy(headers);
}

string_t *websrv_field_name(const string_t *name)
{
    if (!name || !string_byte_length(name)) return NULL;
    string_t *copy = string_new();
    if (!copy) return NULL;
    string_view_t v = string_view_all(name);
    for (size_t i = 0; i < string_view_length(v); ++i) {
        unsigned char c;
        if (!string_view_peek_ascii(v, i, &c)) goto fail;
        bool token = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '!' || c == '#' || c == '$' || c == '%' || c == '&' || c == '\'' || c == '*' ||
            c == '+' || c == '-' || c == '.' || c == '^' || c == '_' || c == '`' || c == '|' || c == '~';
        if (!token || string_append_char(copy, (char)(c >= 'A' && c <= 'Z' ? c + 32 : c))) goto fail;
    }
    return copy;
fail:
    string_free(copy);
    return NULL;
}

bool websrv_field_value(const string_t *value)
{
    if (!value) return false;
    string_view_t v = string_view_all(value);
    for (size_t i = 0; i < string_view_length(v); ++i) {
        unsigned char c;
        if (!string_view_peek_ascii(v, i, &c) || (c != '\t' && (c < 32 || c > 126))) return false;
    }
    return true;
}

static bool hex_digit(unsigned char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

bool websrv_target_valid(const string_t *target, bool route)
{
    if (!target) return false;
    string_view_t v = string_view_all(target);
    unsigned char c;
    if (!string_view_peek_ascii(v, 0, &c) || c != '/') return false;
    for (size_t i = 0; i < string_view_length(v); ++i) {
        if (!string_view_peek_ascii(v, i, &c) || c < 33 || c > 126 || c == '#' || c == '\\' ||
            (route && c == '?')) return false;
        if (c == '%') {
            unsigned char a, b;
            if (!string_view_peek_ascii(v, i + 1, &a) || !string_view_peek_ascii(v, i + 2, &b) ||
                !hex_digit(a) || !hex_digit(b)) return false;
            i += 2;
        }
    }
    return true;
}

/* On success the dictionary owns both inputs; on failure it owns neither. */
bool websrv_header_put(dictionary_t *headers, string_t *key, string_t *value)
{
    dictionary_entry_t *entry = NULL;
    if (dictionary_get_entry(headers, &key, &entry)) {
        string_t *old = *(string_t **)dictionary_entry_value(entry);
        if (!dictionary_set_entry(headers, entry, &value)) return false;
        string_free(old);
        string_free(key);
        return true;
    }
    return dictionary_set(headers, &key, &value);
}

const string_t *websrv_header_get(const dictionary_t *headers, const char *name)
{
    string_t *key = string_new_with(name);
    dictionary_entry_t *entry = NULL;
    const string_t *value = key && dictionary_get_entry(headers, &key, &entry)
        ? *(string_t *const *)dictionary_entry_value(entry) : NULL;
    string_free(key);
    return value;
}

void websrv_request_clear(websrv_request_t *r)
{
    string_free(r->target);
    string_free(r->path);
    string_free(r->peer);
    free(r->body);
    websrv_headers_free(r->headers);
}

void websrv_response_clear(websrv_response_t *r)
{
    free(r->body);
    websrv_headers_free(r->headers);
    r->body = NULL;
    r->body_size = 0;
    r->headers = NULL;
}

/* Read the parsed method without transferring ownership. */
webmethod_t websrv_request_method(const websrv_request_t *r) { return r ? r->method : HTTP_GET; }

/* Borrow the verified socket peer independently of request headers. */
const string_t *websrv_request_peer(const websrv_request_t *r) { return r ? r->peer : NULL; }

/* Borrow the undecoded request target. */
const string_t *websrv_request_target(const websrv_request_t *r) { return r ? r->target : NULL; }

/* Borrow the undecoded routing path. */
const string_t *websrv_request_path(const websrv_request_t *r) { return r ? r->path : NULL; }

/* Borrow a case-insensitively matched header. */
const string_t *websrv_request_header(const websrv_request_t *r, const string_t *name)
{
    string_t *key = websrv_field_name(name);
    dictionary_entry_t *entry = NULL;
    const string_t *value = r && key && dictionary_get_entry(r->headers, &key, &entry)
        ? *(string_t *const *)dictionary_entry_value(entry) : NULL;
    string_free(key);
    return value;
}

/* Borrow exact binary body bytes. */
const void *websrv_request_body(const websrv_request_t *r) { return r ? r->body : NULL; }

/* Read the binary body byte count. */
size_t websrv_request_body_size(const websrv_request_t *r) { return r ? r->body_size : 0; }

/* Decode UTF-8 without normalising the payload. */
string_t *websrv_request_text(const websrv_request_t *r)
{
    if (!r) return NULL;
    string_t *text = string_new();
    if (text && !string_append_utf8_exact(text, websrv_request_body(r), websrv_request_body_size(r))) return text;
    string_free(text);
    return NULL;
}

/* Parse a body through the native JSON module. */
json_t *websrv_request_json(const websrv_request_t *r)
{
    string_t *text = websrv_request_text(r);
    json_t *json = text ? json_from_text(text) : NULL;
    string_free(text);
    return json;
}

/* Parse a body through the native XML module. */
xml_t *websrv_request_xml(const websrv_request_t *r)
{
    string_t *text = websrv_request_text(r);
    xml_t *xml = text ? xml_from_text(text) : NULL;
    string_free(text);
    return xml;
}

/* Set a final status; wire framing handles bodyless statuses. */
bool websrv_response_status(websrv_response_t *r, unsigned status)
{
    if (!r || status < 200 || status > 599) return false;
    r->status = status;
    return true;
}

/* Validate and copy a response field without permitting framing overrides. */
bool websrv_response_header(websrv_response_t *r, const string_t *name, const string_t *value)
{
    string_t *key = websrv_field_name(name), *copy = NULL;
    if (!r || !key || !websrv_field_value(value)) goto fail;
    string_view_t v = string_view_all(key);
    if (string_view_equals_literal(v, "content-length") || string_view_equals_literal(v, "transfer-encoding") ||
        string_view_equals_literal(v, "connection") || string_view_equals_literal(v, "proxy-connection") ||
        string_view_equals_literal(v, "keep-alive") || string_view_equals_literal(v, "trailer") ||
        string_view_equals_literal(v, "upgrade") || string_view_equals_literal(v, "date")) goto fail;
    size_t size = string_byte_length(key) + string_byte_length(value) + 4;
    dictionary_entry_t *entry = NULL;
    size_t old = dictionary_get_entry(r->headers, &key, &entry)
        ? string_byte_length(key) + string_byte_length(*(string_t **)dictionary_entry_value(entry)) + 4 : 0;
    if (size > r->max_headers || r->header_bytes - old > r->max_headers - size) goto fail;
    copy = string_clone(value);
    if (!copy || !websrv_header_put(r->headers, key, copy)) goto fail;
    r->header_bytes = r->header_bytes - old + size;
    return true;
fail:
    string_free(key);
    string_free(copy);
    return false;
}

/* Replace a bounded binary body atomically. */
bool websrv_response_body(websrv_response_t *r, const void *data, size_t size)
{
    if (!r || (!data && size) || size > r->max_body) return false;
    unsigned char *body = size ? malloc(size) : NULL;
    if (size && !body) return false;
    if (size) memcpy(body, data, size);
    free(r->body);
    r->body = body;
    r->body_size = size;
    return true;
}

/* Copy encoded text bytes without changing Unicode spelling. */
bool websrv_response_text(websrv_response_t *r, const string_t *text)
{
    return text && websrv_response_body(r, string_c_str(text), string_byte_length(text));
}

static bool response_document(websrv_response_t *r, string_t *text, const char *mime)
{
    string_t *name = string_new_with("Content-Type"), *value = string_new_with(mime);
    bool ok = text && name && value && websrv_response_text(r, text) && websrv_response_header(r, name, value);
    string_free(name);
    string_free(value);
    string_free(text);
    return ok;
}

/* Serialise a borrowed JSON document. */
bool websrv_response_json(websrv_response_t *r, const json_t *json)
{
    return json && response_document(r, json_to_string(json), "application/json");
}

/* Serialise a borrowed XML document. */
bool websrv_response_xml(websrv_response_t *r, const xml_t *xml)
{
    return xml && response_document(r, xml_to_string(xml), "application/xml");
}
