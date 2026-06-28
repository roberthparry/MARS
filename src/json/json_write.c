/* json_write.c - JSON serialisation and file saving */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json_internal.h"

typedef bool (*json_write_value_fn)(const json_t *json,
                                    string_t *out,
                                    int indent_size,
                                    int depth);

static bool json_write_null(const json_t *json, string_t *out, int indent_size, int depth);
static bool json_write_bool(const json_t *json, string_t *out, int indent_size, int depth);
static bool json_write_number(const json_t *json, string_t *out, int indent_size, int depth);
static bool json_write_string_value(const json_t *json, string_t *out, int indent_size, int depth);
static bool json_write_array(const json_t *json, string_t *out, int indent_size, int depth);
static bool json_write_object(const json_t *json, string_t *out, int indent_size, int depth);

static const json_write_value_fn json_write_ops[JSON_OBJECT + 1u] = {
    [JSON_NULL] = json_write_null,
    [JSON_BOOL] = json_write_bool,
    [JSON_NUMBER] = json_write_number,
    [JSON_STRING] = json_write_string_value,
    [JSON_ARRAY] = json_write_array,
    [JSON_OBJECT] = json_write_object
};

static bool json_append_indent(string_t *out, int spaces)
{
    for (int i = 0; i < spaces; ++i) {
        if (string_append_char(out, ' ') != 0)
            return false;
    }
    return true;
}

static bool json_append_escaped_rune(string_t *out, rune_t rune)
{
    uint32_t value;

    if (!out || rune_is_none(rune))
        return false;

    value = rune_value(rune);
    switch (value) {
        case '"':
            return string_append_cstr(out, "\\\"") == 0;
        case '\\':
            return string_append_cstr(out, "\\\\") == 0;
        case '\b':
            return string_append_cstr(out, "\\b") == 0;
        case '\f':
            return string_append_cstr(out, "\\f") == 0;
        case '\n':
            return string_append_cstr(out, "\\n") == 0;
        case '\r':
            return string_append_cstr(out, "\\r") == 0;
        case '\t':
            return string_append_cstr(out, "\\t") == 0;
        default:
            if (value == 0u || rune_is_control(rune))
                return string_append_format(out, "\\u%04X", (unsigned int)value) >= 0;
            return string_append_rune(out, rune) == 0;
    }
}

static bool json_append_escaped_string(string_t *out, const string_t *text)
{
    string_cursor_t *cursor;

    if (!out || !text || string_append_char(out, '"') != 0)
        return false;

    cursor = string_cursor_new(text);
    if (!cursor)
        return false;

    while (!string_cursor_done(cursor)) {
        rune_t rune = string_cursor_peek(cursor);

        if (!json_append_escaped_rune(out, rune) ||
            string_cursor_next(cursor) != 0) {
            string_cursor_free(cursor);
            return false;
        }
    }

    string_cursor_free(cursor);
    return string_append_char(out, '"') == 0;
}

static bool json_append_escaped_literal(string_t *out, const char *literal)
{
    string_t *text;
    bool ok;

    if (!out || !literal)
        return false;

    text = string_new_with(literal);
    if (!text)
        return false;

    ok = json_append_escaped_string(out, text);
    string_free(text);
    return ok;
}

static bool json_write_extended_number(const json_t *json, string_t *out)
{
    return string_append_char(out, '{') == 0 &&
           json_append_escaped_literal(out, "$mars.number") &&
           string_append_char(out, ':') == 0 &&
           json_append_escaped_string(out, json->u.number.text) &&
           string_append_char(out, '}') == 0;
}

static bool json_write_null(const json_t *json, string_t *out, int indent_size, int depth)
{
    (void)json;
    (void)indent_size;
    (void)depth;
    return string_append_cstr(out, "null") == 0;
}

static bool json_write_bool(const json_t *json, string_t *out, int indent_size, int depth)
{
    (void)indent_size;
    (void)depth;
    return string_append_cstr(out, json->u.boolean ? "true" : "false") == 0;
}

static bool json_write_number(const json_t *json, string_t *out, int indent_size, int depth)
{
    (void)indent_size;
    (void)depth;

    if (!json || !json->u.number.text)
        return false;

    if (json_number_scan(json->u.number.text, true))
        return string_append_string(out, json->u.number.text) == 0;

    return json_write_extended_number(json, out);
}

static bool json_write_string_value(const json_t *json,
                                    string_t *out,
                                    int indent_size,
                                    int depth)
{
    (void)indent_size;
    (void)depth;
    return json_append_escaped_string(out, json->u.text);
}

static bool json_write_value(const json_t *json,
                             string_t *out,
                             int indent_size,
                             int depth)
{
    if (!json || !json_type_valid(json->type))
        return false;

    return json_write_ops[json->type](json, out, indent_size, depth);
}

static bool json_write_array(const json_t *json, string_t *out, int indent_size, int depth)
{
    size_t count = array_size(json->u.array);

    if (string_append_char(out, '[') != 0)
        return false;
    if (count == 0u)
        return string_append_char(out, ']') == 0;

    for (size_t i = 0u; i < count; ++i) {
        json_t **value_ptr = array_get(json->u.array, i);

        if (!value_ptr)
            return false;
        if (i > 0u && string_append_char(out, ',') != 0)
            return false;

        if (indent_size > 0) {
            if (string_append_char(out, '\n') != 0 ||
                !json_append_indent(out, (depth + 1) * indent_size))
                return false;
        }

        if (!json_write_value(*value_ptr, out, indent_size, depth + 1))
            return false;
    }

    if (indent_size > 0) {
        if (string_append_char(out, '\n') != 0 ||
            !json_append_indent(out, depth * indent_size))
            return false;
    }

    return string_append_char(out, ']') == 0;
}

static bool json_write_object(const json_t *json, string_t *out, int indent_size, int depth)
{
    size_t count = dictionary_size(json->u.object);

    if (string_append_char(out, '{') != 0)
        return false;
    if (count == 0u)
        return string_append_char(out, '}') == 0;

    for (size_t i = 0u; i < count; ++i) {
        dictionary_entry_t *entry = NULL;
        const void *key_ptr;
        const void *value_ptr;
        string_t *key;

        if (!dictionary_get_entry_sorted(json->u.object,
                                         i,
                                         DICTIONARY_SORT_BY_KEY,
                                         &entry) ||
            !entry)
            return false;

        key_ptr = dictionary_entry_key(entry);
        value_ptr = dictionary_entry_value(entry);
        if (!key_ptr || !value_ptr)
            return false;
        if (i > 0u && string_append_char(out, ',') != 0)
            return false;

        if (indent_size > 0) {
            if (string_append_char(out, '\n') != 0 ||
                !json_append_indent(out, (depth + 1) * indent_size))
                return false;
        }

        key = *(string_t * const *)key_ptr;
        if (!json_append_escaped_string(out, key))
            return false;
        if (indent_size > 0) {
            if (string_append_cstr(out, ": ") != 0)
                return false;
        } else if (string_append_char(out, ':') != 0) {
            return false;
        }

        if (!json_write_value(*(json_t * const *)value_ptr, out, indent_size, depth + 1))
            return false;
    }

    if (indent_size > 0) {
        if (string_append_char(out, '\n') != 0 ||
            !json_append_indent(out, depth * indent_size))
            return false;
    }

    return string_append_char(out, '}') == 0;
}

string_t *json_to_string(const json_t *json)
{
    string_t *out = string_new();

    if (!out)
        return NULL;

    if (!json_write_value(json, out, 0, 0)) {
        string_free(out);
        return NULL;
    }

    return out;
}

string_t *json_to_string_pretty(const json_t *json, int indent_size)
{
    string_t *out = string_new();

    if (!out)
        return NULL;

    if (indent_size < 0)
        indent_size = 0;

    if (!json_write_value(json, out, indent_size, 0)) {
        string_free(out);
        return NULL;
    }

    return out;
}

static int json_write_file_text(const string_t *text, const string_t *path)
{
    FILE *file;
    int ok;

    if (!text || !path)
        return -1;

    file = fopen(string_c_str(path), "w");
    if (!file)
        return -1;

    ok = string_fprintf(file, "%S\n", text) >= 0 ? 0 : -1;
    if (fclose(file) != 0)
        ok = -1;

    return ok;
}

int json_to_file(const json_t *json, const string_t *path)
{
    string_t *text = json_to_string(json);
    int rc;

    if (!text)
        return -1;

    rc = json_write_file_text(text, path);
    string_free(text);
    return rc;
}

int json_to_file_pretty(const json_t *json, const string_t *path, int indent_size)
{
    string_t *text = json_to_string_pretty(json, indent_size);
    int rc;

    if (!text)
        return -1;

    rc = json_write_file_text(text, path);
    string_free(text);
    return rc;
}

bool json_serialize(const json_t *json,
                    string_t **out_type,
                    string_t **out_encoding,
                    void **out_data,
                    size_t *out_len)
{
    string_t *type = NULL;
    string_t *encoding = NULL;
    string_t *text = NULL;
    void *payload = NULL;

    if (!json || !out_type || !out_encoding || !out_data || !out_len)
        return false;

    text = json_to_string(json);
    if (!text)
        return false;

    payload = malloc(string_byte_length(text));
    if (!payload) {
        string_free(text);
        return false;
    }
    memcpy(payload, string_c_str(text), string_byte_length(text));

    type = string_new_with("json_t");
    encoding = string_new_with("application/json");
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

json_t *json_deserialise(const void *data,
                         size_t len,
                         const string_t *type,
                         const string_t *encoding)
{
    string_t *text;
    json_t *json;

    if (!data || !type || !encoding)
        return NULL;
    if (strcmp(string_c_str(type), "json_t") != 0 ||
        strcmp(string_c_str(encoding), "application/json") != 0)
        return NULL;

    text = string_new();
    if (!text)
        return NULL;
    if (string_append_chars(text, (const char *)data, len) != 0) {
        string_free(text);
        return NULL;
    }

    json = json_from_text(text);
    string_free(text);
    return json;
}
