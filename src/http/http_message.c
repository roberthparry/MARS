/**
 * @file http_message.c
 * @brief HTTP message headers and text or document body adapters.
 *
 * Validates and stores dictionary-backed header fields and converts request or response bodies through string,
 * JSON and XML APIs. These helpers implement the message representation independently of libcurl execution.
 * Transport status, header validity and document parsing remain separate checks.
 */

#include <stdint.h>
#include <stdlib.h>
#define MARS_HTTP_INTERNAL_ACCESS
#include "http_internal.h"

static size_t key_hash(const void *key) { return string_hash(*(string_t *const *)key); }
static int key_compare(const void *a, const void *b)
{
    return string_compare(*(string_t *const *)a, *(string_t *const *)b);
}
static void value_free(void *value) { string_free(*(string_t **)value); }

dictionary_t *http_headers_new(void)
{
    /* Own entries explicitly: dictionary insertion rollback can destroy shallow copies. */
    return dictionary_create(sizeof(string_t *), sizeof(array_t *), key_hash, key_compare, NULL, NULL, NULL, NULL, NULL);
}

void http_headers_free(dictionary_t *headers)
{
    for (size_t i = 0; i < dictionary_size(headers); ++i) {
        string_free(*(string_t *const *)dictionary_get_key(headers, i));
        array_destroy(*(array_t *const *)dictionary_get_value(headers, i));
    }
    dictionary_destroy(headers);
}

bool http_ascii_text(const string_t *text, bool spaces)
{
    if (!text)
        return false;
    string_view_t view = string_view_all(text);
    for (string_pos_t pos = 0; pos < string_view_length(view); ++pos) {
        unsigned char c;
        if (!string_view_peek_ascii(view, pos, &c) || c > 126 || c < (spaces ? 32 : 33))
            return false;
    }
    return true;
}

bool http_header_value(const string_t *value)
{
    if (!value)
        return false;
    string_view_t view = string_view_all(value);
    for (string_pos_t pos = 0; pos < string_view_length(view); ++pos) {
        unsigned char c;
        if (!string_view_peek_ascii(view, pos, &c) || (c != 9 && (c < 32 || c > 126)))
            return false;
    }
    return true;
}

string_t *http_header_name(const string_t *name)
{
    if (!name || !string_byte_length(name))
        return NULL;
    string_t *out = string_new();
    if (!out)
        return NULL;
    string_view_t view = string_view_all(name);
    for (string_pos_t pos = 0; pos < string_view_length(view); ++pos) {
        unsigned char c;
        if (!string_view_peek_ascii(view, pos, &c))
            goto fail;
        bool token = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                     c == '!' || c == '#' || c == '$' || c == '%' || c == '&' || c == '\'' || c == '*' ||
                     c == '+' || c == '-' || c == '.' || c == '^' || c == '_' || c == '`' || c == '|' || c == '~';
        if (!token || string_append_char(out, (char)(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c)))
            goto fail;
    }
    return out;
fail:
    string_free(out);
    return NULL;
}

const array_t *http_headers_get(const dictionary_t *headers, const string_t *name)
{
    string_t *key = http_header_name(name);
    dictionary_entry_t *entry = NULL;
    const array_t *values = NULL;
    if (key && dictionary_get_entry(headers, &key, &entry))
        values = *(array_t *const *)dictionary_entry_value(entry);
    string_free(key);
    return values;
}

bool http_headers_store(dictionary_t *headers, const string_t *name, const string_t *value, bool replace)
{
    string_t *key = http_header_name(name), *copy = NULL;
    dictionary_entry_t *entry = NULL;
    array_t *values = NULL;
    if (!headers || !key || !http_header_value(value))
        goto fail;
    copy = string_clone(value);
    if (!copy)
        goto fail;
    bool exists = dictionary_get_entry(headers, &key, &entry);
    if (exists && !replace) {
        values = *(array_t *const *)dictionary_entry_value(entry);
        if (!array_add(values, &copy))
            goto fail;
        string_free(key);
        return true;
    }
    values = array_create(sizeof(string_t *), NULL, value_free);
    if (!values || !array_add(values, &copy)) {
        array_destroy(values);
        goto fail;
    }
    copy = NULL;
    if (exists) {
        array_t *old = *(array_t *const *)dictionary_entry_value(entry);
        if (dictionary_set_entry(headers, entry, &values)) {
            array_destroy(old);
            string_free(key);
            return true;
        }
    } else if (dictionary_set(headers, &key, &values)) {
        return true;
    }
    array_destroy(values);
fail:
    string_free(key);
    string_free(copy);
    return false;
}

static bool set_document(http_request_t *request, string_t *text, const char *mime)
{
    string_t *type = string_new_with(mime);
    bool ok = text && type && http_request_set_text(request, text, type);
    string_free(type);
    string_free(text);
    return ok;
}

/* Serialise using the JSON module, retaining the caller's tree. */
bool http_request_set_json(http_request_t *request, const json_t *json)
{
    return json && set_document(request, json_to_string(json), "application/json");
}

/* Serialise using the native XML module, retaining the caller's tree. */
bool http_request_set_xml(http_request_t *request, const xml_t *xml)
{
    return xml && set_document(request, xml_to_string(xml), "application/xml");
}

/* Decode buffered UTF-8 strictly without changing Unicode spelling. */
string_t *http_response_text(const http_response_t *response)
{
    if (!response || response->streamed)
        return NULL;
    string_t *text = string_new();
    if (!text)
        return NULL;
    if (string_append_utf8_exact(text, http_response_body(response), http_response_body_size(response)) != 0) {
        string_free(text);
        return NULL;
    }
    return text;
}

/* Parse a buffered UTF-8 response as JSON without assuming HTTP success. */
json_t *http_response_json(const http_response_t *response)
{
    string_t *text = http_response_text(response);
    json_t *json = text ? json_from_text(text) : NULL;
    string_free(text);
    return json;
}

/* Parse a buffered UTF-8 response as XML without assuming HTTP success. */
xml_t *http_response_xml(const http_response_t *response)
{
    string_t *text = http_response_text(response);
    xml_t *xml = text ? xml_from_text(text) : NULL;
    string_free(text);
    return xml;
}
