/**
 * @file almanac.c
 * @brief Configured astronomical engine lifecycle and persistence.
 *
 * Opens and closes the configured engine, manages diagnostics and database paths, and serialises its
 * configuration. Use this unit when changing resource ownership or restoring an engine, rather than numerical
 * event geometry.
 *
 * This is part of the catalogue-backed almanac implementation. Applications should use almanac.h and provide the
 * required configured data; this file is not a standalone astronomical program.
 */

/* Configured engine lifetime, diagnostics and serialisation. */
#include <ctype.h>
#include <stdio.h>
#include "file.h"
#include <stdlib.h>
#include <string.h>

#include "almanac_engine_internal.h"

void almanac_set_error(almanac_t *almanac, const char *message)
{
    if (!almanac || !almanac->error)
        return;
    string_clear(almanac->error);
    (void)string_append_cstr(almanac->error, message ? message : "almanac error");
}

void almanac_set_sqlite_error(almanac_t *almanac)
{
    const string_t *sqlite_error;

    if (!almanac)
        return;
    sqlite_error = almanac->db ? sqlite_last_error(almanac->db) : NULL;
    almanac_set_error(almanac, sqlite_error ? string_c_str(sqlite_error) : "almanac sqlite error");
}

static char *dup_c_string(const char *text)
{
    size_t len;
    char *out;

    if (!text)
        return NULL;
    len = strlen(text);
    out = malloc(len + 1u);
    if (!out)
        return NULL;
    memcpy(out, text, len + 1u);
    return out;
}

static char *dup_printf_path3(const char *a, const char *b, const char *c)
{
    size_t len_a;
    size_t len_b;
    size_t len_c;
    char *out;

    if (!a || !b || !c)
        return NULL;
    len_a = strlen(a);
    len_b = strlen(b);
    len_c = strlen(c);
    out = malloc(len_a + len_b + len_c + 1u);
    if (!out)
        return NULL;
    memcpy(out, a, len_a);
    memcpy(out + len_a, b, len_b);
    memcpy(out + len_a + len_b, c, len_c);
    out[len_a + len_b + len_c] = '\0';
    return out;
}

static char *mars_home_path(void)
{
    const char *mars_home = getenv("MARS_HOME");
    const char *home = getenv("HOME");

    if (mars_home && *mars_home)
        return dup_c_string(mars_home);
    if (home && *home)
        return dup_printf_path3(home, "/.mars", "");
    return NULL;
}

static char *almanac_default_db_path(void)
{
    char *mars_home = mars_home_path();
    char *path;

    if (!mars_home)
        return NULL;
    path = dup_printf_path3(mars_home, "/almanac/", "almanac.db");
    free(mars_home);
    return path;
}

static char *almanac_config_path(void)
{
    char *mars_home = mars_home_path();
    char *path;

    if (!mars_home)
        return NULL;
    path = dup_printf_path3(mars_home, "/config/", "almanac-db.env");
    free(mars_home);
    return path;
}

/* Parse complete configuration lines as MARS strings; this is not a shell interpreter. */
static char *almanac_config_lookup(const char *name)
{
    char *config_path = almanac_config_path();
    file_t *file = NULL;
    string_t *wanted = NULL;
    string_t *line = NULL;
    char *result = NULL;

    if (!config_path || !name || !*name)
        goto done;
    wanted = string_new_with(name);
    file = file_new_cstr(config_path);
    if (!wanted || !file || !file_open_follow(file, FILE_MODE_OPEN, FILE_ACCESS_READ))
        goto done;

    while (file_read_line(file, &line) && line) {
        string_t *key = NULL;
        string_t *value = NULL;
        string_trim(line);
        if (string_starts_with(line, "export ")) {
            string_t *body = string_substr(line, 7, string_byte_length(line) - 7);
            string_free(line);
            line = body;
            if (!line)
                break;
        }
        string_offset_t equals = string_find(line, "=");
        if (equals >= 0) {
            key = string_substr(line, 0, (size_t)equals);
            value = string_substr(line, (size_t)equals + 1, string_byte_length(line) - (size_t)equals - 1);
        }
        if (key)
            string_trim(key);
        bool matched = key && value && string_compare(key, wanted) == 0;
        if (matched) {
            string_trim(value);
            if (string_byte_length(value) >= 2 &&
                ((string_starts_with(value, "\"") && string_ends_with(value, "\"")) ||
                 (string_starts_with(value, "'") && string_ends_with(value, "'")))) {
                string_t *unquoted = string_substr(value, 1, string_byte_length(value) - 2);
                string_free(value);
                value = unquoted;
            }
            if (value)
                result = dup_c_string(string_c_str(value));
        }
        string_free(key);
        string_free(value);
        string_free(line);
        line = NULL;
        if (matched)
            break;
    }
    if (!file_close(file)) {
        free(result);
        result = NULL;
    }

done:
    string_free(line);
    string_free(wanted);
    file_free(file);
    free(config_path);
    return result;
}

static string_t *string_new_from_cstr(const char *text)
{
    if (!text || *text == '\0')
        return NULL;
    return string_new_with(text);
}

/* Open the configured almanac engine. */
almanac_t *almanac_open(void)
{
    almanac_t *almanac;
    char *path_text = NULL;
    char *key_text = NULL;
    char *configured = NULL;
    string_t *path = NULL;
    string_t *key = NULL;

    almanac = calloc(1u, sizeof(*almanac));
    if (!almanac)
        return NULL;
    almanac->error = string_new();
    if (!almanac->error) {
        free(almanac);
        return NULL;
    }

    configured = getenv("MARS_ALMANAC_DB_PATH") ? dup_c_string(getenv("MARS_ALMANAC_DB_PATH")) : NULL;
    if (!configured)
        configured = almanac_config_lookup("MARS_ALMANAC_DB_PATH");
    path_text = configured ? configured : almanac_default_db_path();

    configured = getenv("MARS_ALMANAC_DB_KEY") ? dup_c_string(getenv("MARS_ALMANAC_DB_KEY")) : NULL;
    if (!configured)
        configured = almanac_config_lookup("MARS_ALMANAC_DB_KEY");
    key_text = configured;

    path = string_new_from_cstr(path_text);
    key = string_new_from_cstr(key_text);
    if (!path || !key) {
        almanac_set_error(almanac,
                          "almanac configuration is incomplete; provide MARS_ALMANAC_DB_KEY and an almanac database");
        string_free(path);
        string_free(key);
        free(path_text);
        free(key_text);
        return almanac;
    }

    almanac->db = sqlite_open_encrypted(path, key);
    if (!almanac->db)
        almanac_set_error(almanac, "failed to open encrypted almanac database");

    string_free(path);
    string_free(key);
    free(path_text);
    free(key_text);
    return almanac;
}

/* Destroy an almanac engine. */
void almanac_close(almanac_t *almanac)
{
    if (!almanac)
        return;
    free(almanac->nutation_terms);
    sqlite_close(almanac->db);
    string_free(almanac->error);
    free(almanac);
}

/* Return the last error message recorded by the almanac engine. */
const char *almanac_last_error(const almanac_t *almanac)
{
    return (almanac && almanac->error) ? string_c_str(almanac->error) : NULL;
}

/* Serialise an almanac engine into a SQLite-ready payload. */
bool almanac_serialize(const almanac_t *almanac, string_t **out_type, string_t **out_encoding, void **out_data,
    size_t *out_len)
{
    static const char sentinel[] = "default";
    void *payload;

    if (!almanac || !almanac->db || !out_type || !out_encoding || !out_data || !out_len)
        return false;

    *out_type = string_new_with("almanac_t");
    *out_encoding = string_new_with("mars/configured-engine-v1");
    if (!*out_type || !*out_encoding) {
        string_free(*out_type);
        string_free(*out_encoding);
        *out_type = NULL;
        *out_encoding = NULL;
        return false;
    }

    payload = malloc(sizeof(sentinel) - 1u);
    if (!payload) {
        string_free(*out_type);
        string_free(*out_encoding);
        *out_type = NULL;
        *out_encoding = NULL;
        return false;
    }

    memcpy(payload, sentinel, sizeof(sentinel) - 1u);
    *out_data = payload;
    *out_len = sizeof(sentinel) - 1u;
    return true;
}

/* Reconstruct an almanac engine from a serialised payload. */
almanac_t *almanac_deserialise(const void *data, size_t len, const string_t *type, const string_t *encoding)
{
    static const char sentinel[] = "default";

    if (!data || !type || !encoding)
        return NULL;
    if (strcmp(string_c_str(type), "almanac_t") != 0)
        return NULL;
    if (strcmp(string_c_str(encoding), "mars/configured-engine-v1") != 0)
        return NULL;
    if (len != sizeof(sentinel) - 1u)
        return NULL;
    if (memcmp(data, sentinel, sizeof(sentinel) - 1u) != 0)
        return NULL;

    return almanac_open();
}
