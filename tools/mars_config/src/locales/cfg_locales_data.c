/**
 * @file cfg_locales_data.c
 * @brief Bounded pinned-input loading and exact UTF-8 container helpers.
 *
 * JSON syntax is ASCII only. Unicode values use single-key utf8_hex wrappers,
 * decoded after parsing so the general JSON parser cannot normalise CLDR labels.
 * Inputs are finite snapshots, not an arbitrary CLDR archive importer.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cfg_locales_internal.h"

/* Look up an ASCII field without exposing internal dictionary storage. */
const json_t *cfg_locales_get(const json_t *object, const char *key)
{
    string_t *name = string_new_with(key);
    const json_t *value = name ? json_object_get(object, name) : NULL;
    string_free(name);
    return value;
}

/* Return a borrowed text field. */
const string_t *cfg_locales_text(const json_t *object, const char *key)
{
    return json_string_value(cfg_locales_get(object, key));
}

/* Identifier equality does not alter either operand. */
bool cfg_locales_equal(const string_t *text, const char *literal)
{
    return text && string_view_equals_literal(string_view_all(text), literal);
}

/* Copy one string into an indexed map. */
bool cfg_locales_set(json_t *object, const string_t *key, const string_t *value)
{
    json_t *text = value ? json_new_string(value) : json_new_null();
    bool ok = key && text && json_object_set(object, key, text);
    json_free(text);
    return ok;
}

/* Copy one string into a row. */
bool cfg_locales_push(json_t *array, const string_t *text)
{
    json_t *value = text ? json_new_string(text) : json_new_null();
    bool ok = value && json_array_append(array, value);
    json_free(value);
    return ok;
}

/* Keep numeric SQL fields numeric in the intermediate tables. */
bool cfg_locales_number(json_t *array, size_t value)
{
    string_t *text = string_sprintf("%zu", value);
    json_t *number = text ? json_new_number(text) : NULL;
    bool ok = number && json_array_append(array, number);
    json_free(number);
    string_free(text);
    return ok;
}

static int cfg_locales_key_compare(const void *left, const void *right)
{
    return string_compare(*(const string_t *const *)left, *(const string_t *const *)right);
}

/* Sort borrowed ASCII keys; callers release only the returned vector. */
const string_t **cfg_locales_keys(const json_t *object)
{
    size_t count = json_object_size(object);
    const string_t **keys = calloc(count + 1, sizeof(*keys));
    if (!keys)
        return NULL;
    for (size_t i = 0; i < count; ++i)
        keys[i] = json_object_key_at(object, i);
    qsort(keys, count, sizeof(*keys), cfg_locales_key_compare);
    return keys;
}

/* Assemble source bytes once, avoiding chunk-boundary UTF-8 and normalisation changes. */
string_t *cfg_locales_read(const string_t *path)
{
    size_t limit = 4 * 1024 * 1024, used = 0, count = 0;
    char *bytes = malloc(limit + 1);
    file_t *file = path ? file_new(path) : NULL;
    bool ok = bytes && file && file_open_read(file);
    while (ok && used <= limit) {
        ok = file_read(file, bytes + used, limit + 1 - used, &count);
        used += count;
        if (!ok || !count || used > limit)
            break;
    }
    string_t *text = ok && used <= limit ? string_new() : NULL;
    if (text && string_append_utf8_exact(text, bytes, used)) {
        string_free(text);
        text = NULL;
    }
    if (file && !file_close(file)) {
        string_free(text);
        text = NULL;
    }
    file_free(file);
    free(bytes);
    return text;
}

/* Preserve exact directory bytes even when the path uses decomposed Unicode. */
string_t *cfg_locales_path(const string_t *directory, const char *suffix)
{
    string_t *path = directory ? string_clone(directory) : NULL;
    if (path && (string_append_utf8_exact(path, "/", 1) || string_append_utf8_exact(path, suffix, strlen(suffix)))) {
        string_free(path);
        path = NULL;
    }
    return path;
}

static int cfg_locales_hex(unsigned char byte)
{
    if (byte >= '0' && byte <= '9')
        return byte - '0';
    if (byte >= 'a' && byte <= 'f')
        return byte - 'a' + 10;
    return -1;
}

static json_t *cfg_locales_unhex(const string_t *hex)
{
    size_t length = hex ? string_byte_length(hex) : 0;
    if (!hex || length % 2 || length > 32768)
        return NULL;
    char *bytes = malloc(length / 2 + 1);
    bool ok = bytes != NULL;
    string_view_t view = string_view_all(hex);
    for (size_t i = 0; i < length && ok; i += 2) {
        unsigned char a = 0, b = 0;
        ok = string_view_peek_ascii(view, i, &a) && string_view_peek_ascii(view, i + 1, &b);
        int high = cfg_locales_hex(a), low = cfg_locales_hex(b);
        ok = ok && high >= 0 && low >= 0 && (high || low);
        if (ok)
            bytes[i / 2] = (char)(high * 16 + low);
    }
    string_t *text = ok ? string_new() : NULL;
    ok = text && !string_append_utf8_exact(text, bytes, length / 2);
    json_t *result = ok ? json_new_string(text) : NULL;
    string_free(text);
    free(bytes);
    return result;
}

static json_t *cfg_locales_decode(const json_t *source, unsigned depth)
{
    if (!source || depth > 16)
        return NULL;
    json_type_t type = json_type(source);
    if (type == JSON_OBJECT && cfg_locales_get(source, "utf8_hex"))
        return json_object_size(source) == 1 ? cfg_locales_unhex(cfg_locales_text(source, "utf8_hex")) : NULL;
    if (type != JSON_ARRAY && type != JSON_OBJECT) {
        const string_t *text = json_string_value(source);
        if (text && string_byte_length(text) > 16384)
            return NULL;
        for (size_t i = 0; text && i < string_byte_length(text); ++i) {
            unsigned char byte = 0;
            if (!string_view_peek_ascii(string_view_all(text), i, &byte) || !byte)
                return NULL;
        }
        return json_clone(source);
    }
    size_t count = type == JSON_ARRAY ? json_array_size(source) : json_object_size(source);
    if (count > 20000)
        return NULL;
    json_t *result = type == JSON_ARRAY ? json_new_array() : json_new_object();
    bool ok = result != NULL;
    for (size_t i = 0; i < count && ok; ++i) {
        if (type == JSON_OBJECT) {
            const string_t *key = json_object_key_at(source, i);
            ok = key && string_byte_length(key) <= 16384;
            for (size_t j = 0; ok && j < string_byte_length(key); ++j) {
                unsigned char byte = 0;
                ok = string_view_peek_ascii(string_view_all(key), j, &byte) && byte != 0;
            }
        }
        const json_t *entry = type == JSON_ARRAY ? json_array_get(source, i) : json_object_value_at(source, i);
        json_t *decoded = ok ? cfg_locales_decode(entry, depth + 1) : NULL;
        ok = decoded && (type == JSON_ARRAY ? json_array_append(result, decoded)
                                            : json_object_set(result, json_object_key_at(source, i), decoded));
        json_free(decoded);
    }
    if (!ok) {
        json_free(result);
        result = NULL;
    }
    return result;
}

static bool cfg_locales_preflight(const string_t *text)
{
    if (!text)
        return false;
    bool quoted = false;
    unsigned depth = 0;
    size_t string_length = 0;
    string_view_t view = string_view_all(text);
    for (size_t i = 0; i < string_byte_length(text); ++i) {
        unsigned char byte = 0;
        if (!string_view_peek_ascii(view, i, &byte) || !byte)
            return false;
        if (quoted) {
            if (++string_length > 32768)
                return false;
            if (byte == '"')
                quoted = false;
            else if (byte == '\\') {
                if (!string_view_peek_ascii(view, ++i, &byte) || !byte)
                    return false;
                if (byte == 'u') {
                    unsigned scalar = 0;
                    for (unsigned j = 0; j < 4; ++j) {
                        if (!string_view_peek_ascii(view, ++i, &byte))
                            return false;
                        if (byte >= 'A' && byte <= 'F')
                            byte += 'a' - 'A';
                        int digit = cfg_locales_hex(byte);
                        if (digit < 0)
                            return false;
                        scalar = scalar * 16 + (unsigned)digit;
                    }
                    /* Unicode data must use the explicit exact-byte wrapper, including keys. */
                    if (!scalar || scalar > 127)
                        return false;
                }
            }
        } else if (byte == '"') {
            quoted = true;
            string_length = 0;
        } else if (byte == '[' || byte == '{') {
            if (++depth > 16)
                return false;
        } else if (byte == ']' || byte == '}') {
            if (!depth)
                return false;
            --depth;
        }
    }
    return !quoted && !depth;
}

static json_t *cfg_locales_input(const string_t *directory, const char *name, size_t *total_bytes)
{
    string_t *path = cfg_locales_path(directory, name);
    string_t *text = path ? cfg_locales_read(path) : NULL;
    size_t length = text ? string_byte_length(text) : 0;
    bool ok = text && *total_bytes <= CFG_LOCALES_INPUT_BYTES && length <= CFG_LOCALES_INPUT_BYTES - *total_bytes;
    if (ok)
        *total_bytes += length;
    ok = ok && cfg_locales_preflight(text);
    json_t *encoded = ok ? json_from_text(text) : NULL;
    json_t *decoded = encoded ? cfg_locales_decode(encoded, 0) : NULL;
    if (!decoded)
        fprintf(stderr, "Cannot load pinned locale input: %s\n", name);
    json_free(encoded);
    string_free(text);
    string_free(path);
    return decoded;
}

static bool cfg_locales_pairs_valid(const json_t *pairs, size_t count)
{
    if (!pairs || json_type(pairs) != JSON_ARRAY || json_array_size(pairs) != count)
        return false;
    for (size_t i = 0; i < count; ++i) {
        const json_t *pair = json_array_get(pairs, i);
        if (json_type(pair) != JSON_ARRAY || json_array_size(pair) != 2)
            return false;
        for (size_t j = 0; j < 2; ++j) {
            const string_t *text = json_string_value(json_array_get(pair, j));
            if (!text || !string_byte_length(text))
                return false;
        }
    }
    return true;
}

static bool cfg_locales_record_valid(const json_t *record, bool supplement)
{
    const char *fields[] = {supplement ? "english_name" : "english", supplement ? "native_name" : "native",
                            supplement ? "date_pattern" : "pattern"};
    if (!record || json_type(record) != JSON_OBJECT)
        return false;
    for (size_t i = 0; i < 3; ++i) {
        const json_t *value = cfg_locales_get(record, fields[i]);
        /* CLDR can lack a display name; selection applies its documented fallback. */
        if (!value || (json_type(value) != JSON_STRING && (supplement || i == 2 || json_type(value) != JSON_NULL)))
            return false;
    }
    if (string_byte_length(cfg_locales_text(record, fields[2])) > CFG_LOCALES_PATTERN_BYTES)
        return false;
    const string_t *era = cfg_locales_text(record, "era");
    if (era && string_byte_length(era) > CFG_LOCALES_AFFIX_BYTES)
        return false;
    return cfg_locales_pairs_valid(cfg_locales_get(record, "months"), 12) &&
           cfg_locales_pairs_valid(cfg_locales_get(record, "weekdays"), 7) &&
           (supplement || (cfg_locales_text(record, "language") && cfg_locales_text(record, "era") &&
                           cfg_locales_pairs_valid(cfg_locales_get(record, "standalone"), 12)));
}

static bool cfg_locales_shape(const cfg_locales_state *state)
{
    const char *raw_fields[] = {"resolutions", "territories", "english"};
    const char *supp_fields[] = {
        "options",  "towns",           "locales",       "patterns", "weekday_prefixes", "first_day_suffixes",
        "defaults", "forced_official", "extra_official"};
    for (size_t i = 0; i < sizeof(raw_fields) / sizeof(*raw_fields); ++i) {
        const json_t *value = cfg_locales_get(state->raw, raw_fields[i]);
        if (!value || json_type(value) != JSON_OBJECT || !json_object_size(value))
            return false;
    }
    for (size_t i = 0; i < sizeof(supp_fields) / sizeof(*supp_fields); ++i) {
        const json_t *value = cfg_locales_get(state->supp, supp_fields[i]);
        if (!value || json_type(value) != JSON_OBJECT)
            return false;
    }
    const char *bounded_fields[] = {"patterns", "weekday_prefixes", "first_day_suffixes"};
    for (size_t i = 0; i < sizeof(bounded_fields) / sizeof(*bounded_fields); ++i) {
        const json_t *values = cfg_locales_get(state->supp, bounded_fields[i]);
        size_t limit = i == 0 ? CFG_LOCALES_PATTERN_BYTES : CFG_LOCALES_AFFIX_BYTES;
        for (size_t j = 0; j < json_object_size(values); ++j) {
            const string_t *value = json_string_value(json_object_value_at(values, j));
            if (!value || string_byte_length(value) > limit)
                return false;
        }
    }
    const json_t *supplements = cfg_locales_get(state->supp, "locales");
    for (size_t i = 0; i < json_object_size(supplements); ++i)
        if (!cfg_locales_record_valid(json_object_value_at(supplements, i), true))
            return false;
    return true;
}

/* Load versioned source files and reject duplicate locale identifiers. */
bool cfg_locales_load(cfg_locales_state *state, const string_t *directory)
{
    size_t total_bytes = 0;
    state->raw = cfg_locales_input(directory, "cldr46.json", &total_bytes);
    state->supp = cfg_locales_input(directory, "supplements.json", &total_bytes);
    state->records = json_new_object();
    bool ok = state->records && cfg_locales_equal(cfg_locales_text(state->raw, "format"), "mars-cldr-resolved-1") &&
              cfg_locales_equal(cfg_locales_text(state->raw, "babel"), "2.17.0") &&
              cfg_locales_equal(cfg_locales_text(state->raw, "cldr"), "46") &&
              cfg_locales_equal(cfg_locales_text(state->supp, "format"), "mars-calendar-supplements-1");
    const json_t *files = cfg_locales_get(state->raw, "locale_files");
    size_t count = json_array_size(files);
    ok = ok && count && count <= 32 && cfg_locales_shape(state);
    if (!ok)
        fputs("Pinned locale manifest or supplement schema is invalid.\n", stderr);
    for (size_t i = 0; i < count && ok; ++i) {
        const string_t *name = json_string_value(json_array_get(files, i));
        string_view_t view = string_view_all(name);
        ok = name && string_byte_length(name) && string_byte_length(name) <= 80;
        for (size_t j = 0; ok && j < string_byte_length(name); ++j) {
            unsigned char ch = 0;
            ok = string_view_peek_ascii(view, j, &ch) &&
                 ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '.');
        }
        ok = ok && !cfg_locales_equal(name, ".") && !cfg_locales_equal(name, "..");
        json_t *records = ok ? cfg_locales_input(directory, string_c_str(name), &total_bytes) : NULL;
        ok = records && json_type(records) == JSON_OBJECT && json_object_size(records);
        for (size_t j = 0; ok && j < json_object_size(records); ++j) {
            const string_t *key = json_object_key_at(records, j);
            ok = cfg_locales_record_valid(json_object_value_at(records, j), false) &&
                 !json_object_get(state->records, key) &&
                 json_object_set(state->records, key, json_object_value_at(records, j));
            if (!ok)
                string_fprintf(stderr, "Invalid or duplicate locale record %S in %S.\n", key, name);
        }
        json_free(records);
    }
    return ok;
}

/* Never NFC-normalise accumulated output. */
bool cfg_locales_append(string_t *out, const string_t *text)
{
    return out && text && !string_append_utf8_exact(out, string_c_str(text), string_byte_length(text));
}

/* Hex framing preserves byte identity even where string comparison normalises Unicode. */
string_t *cfg_locales_signature(const json_t *pairs)
{
    string_t *signature = string_new();
    bool ok = signature != NULL;
    const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < json_array_size(pairs) && ok; ++i) {
        const json_t *pair = json_array_get(pairs, i);
        ok = json_array_size(pair) == 2;
        for (size_t j = 0; j < 2 && ok; ++j) {
            const string_t *text = json_string_value(json_array_get(pair, j));
            ok = text != NULL;
            /* This is byte encoding, not text parsing. */
            for (size_t k = 0; ok && k < string_byte_length(text); ++k) {
                unsigned char byte = (unsigned char)string_c_str(text)[k];
                char encoded[2] = {digits[byte >> 4], digits[byte & 15]};
                ok = !string_append_utf8_exact(signature, encoded, 2);
            }
            ok = ok && !string_append_utf8_exact(signature, ":", 1);
        }
        ok = ok && !string_append_utf8_exact(signature, ";", 1);
    }
    if (!ok) {
        string_free(signature);
        signature = NULL;
    }
    return signature;
}
