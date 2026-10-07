/* http_headers.c - validated dictionary-backed HTTP fields. */
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
