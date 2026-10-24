/**
 * @file globe_storage.c
 * @brief Resolve independent globe storage without creating databases or interpreting shell commands.
 *
 * The globe controller owns the returned SQLite connection. A non-empty configured key selects encryption;
 * an empty or absent key selects plaintext. Environment overrides precede literal
 * assignments in globe-db.env. Missing or malformed configuration fails closed; no legacy database is consulted.
 */
#include <stdlib.h>

#include "string-t.h"
#define MARS_GLOBE_INTERNAL_ACCESS
#include "globe_layers.h"

static string_t *globe_setting(file_t *config, const char *name)
{
    const char *environment = getenv(name);
    if (environment) return string_new_with(environment);
    string_t *result = string_new(), *line = NULL;
    if (!config || !file_exists(config)) return result;
    if (!file_open_read(config)) {
        string_free(result);
        return NULL;
    }
    bool ok = result != NULL;
    size_t bytes = 0;
    while (ok) {
        ok = file_read_line(config, &line);
        if (!ok || !line) break;
        bytes += string_byte_length(line);
        if (bytes > 1048576) { ok = false; break; }
        string_trim(line);
        if (string_starts_with(line, "export ")) {
            string_t *body = string_substr(line, 7, string_byte_length(line) - 7);
            string_free(line);
            line = body;
            if (!line) { ok = false; break; }
        }
        string_offset_t equals = string_find(line, "=");
        if (equals >= 0) {
            string_t *key = string_substr(line, 0, (size_t)equals);
            if (key) string_trim(key);
            if (key && string_view_equals_literal(string_view_all(key), name)) {
                string_t *value = string_substr(line, (size_t)equals + 1,
                                               string_byte_length(line) - (size_t)equals - 1);
                if (value) string_trim(value);
                string_t *decoded = value ? string_unquote_shell(value) : NULL;
                ok = decoded && (!string_byte_length(result) || !string_compare(result, decoded));
                string_free(value);
                string_free(result);
                result = decoded;
            }
            string_free(key);
        }
        string_free(line);
        line = NULL;
    }
    string_free(line);
    ok = file_close(config) && ok;
    if (!ok) { string_free(result); result = NULL; }
    return result;
}

static string_t *globe_expand(const string_t *path)
{
    const char *home = getenv("HOME");
    if (string_starts_with(path, "~/"))
        return home ? string_sprintf("%s/%s", home, string_c_str(path) + 2) : NULL;
    return string_clone(path);
}

static bool globe_configuration(string_t **database_path, string_t **database_key)
{
    const char *configured_home = getenv("MARS_HOME"), *home = getenv("HOME");
    string_t *raw_home = configured_home && *configured_home ? string_new_with(configured_home)
                         : home ? string_sprintf("%s/.mars", home) : NULL;
    string_t *root = raw_home ? globe_expand(raw_home) : NULL;
    string_t *config_path = root ? string_sprintf("%s/config/globe-db.env", string_c_str(root)) : NULL;
    file_t *config = config_path ? file_new(config_path) : NULL;
    string_t *path = config ? globe_setting(config, "MARS_GLOBE_DB_PATH") : NULL;
    string_t *key = config ? globe_setting(config, "MARS_GLOBE_DB_KEY") : NULL;
    if (path && !string_byte_length(path)) {
        string_free(path);
        path = string_sprintf("%s/globe/globe.db", string_c_str(root));
    }
    string_t *expanded = path ? globe_expand(path) : NULL;
    *database_path = expanded;
    *database_key = key;
    bool ok = expanded && key;
    file_free(config);
    string_free(path);
    string_free(config_path); string_free(root); string_free(raw_home);
    return ok;
}

static sqlite_t *globe_database_open(const string_t *path, const string_t *key)
{
    file_t *database = path ? file_new(path) : NULL;
    sqlite_t *db = database && file_exists(database) && key
                       ? (string_byte_length(key) ? sqlite_open_encrypted(path, key) : sqlite_open_plain(path)) : NULL;
    if (db && !sqlite_exec_cstr(db, "PRAGMA query_only=ON; PRAGMA cache_size=-512;")) {
        sqlite_close(db);
        db = NULL;
    }
    file_free(database);
    return db;
}

/* Open only an existing, independently configured geometry database. */
sqlite_t *globe_configured_database(void)
{
    string_t *path = NULL, *key = NULL;
    sqlite_t *db = globe_configuration(&path, &key) ? globe_database_open(path, key) : NULL;
    string_free(path); string_free(key);
    return db;
}

struct _globe_reader_t {
    sqlite_t *database;
    globe_t *globe;
    string_t *path, *key, *stamp;
};

static string_t *globe_storage_stamp(const string_t *path)
{
    string_t *stamp = string_new();
    const char *suffixes[] = {"", "-wal"};
    bool ok = stamp && path;
    for (unsigned i = 0; ok && i < 2; ++i) {
        string_t *name = string_sprintf("%s%s", string_c_str(path), suffixes[i]);
        file_t *file = name ? file_new(name) : NULL;
        file_info_t *info = file ? file_get_info(file) : NULL;
        int64_t seconds = 0;
        long nanos = 0;
        ok = file && (info ? file_info_status_change_time(info, &seconds, &nanos) : i && !file_exists(file));
        if (ok) ok = string_append_format(stamp, ":%lld:%ld:%llu", (long long)seconds, nanos,
                                           (unsigned long long)file_info_size(info)) >= 0;
        file_info_free(info); file_free(file); string_free(name);
    }
    if (ok) return stamp;
    string_free(stamp);
    return NULL;
}

static void globe_reader_reset(globe_reader_t *reader)
{
    globe_close(reader->globe);
    sqlite_close(reader->database);
    string_free(reader->path); string_free(reader->key); string_free(reader->stamp);
    *reader = (globe_reader_t){0};
}

/* Allocate lazily so a server can create its session after forking. */
globe_reader_t *globe_reader_new(void)
{
    return calloc(1, sizeof(globe_reader_t));
}

/* Refresh configuration and file identity without repeating encrypted database initialisation. */
globe_t *globe_reader_get(globe_reader_t *reader)
{
    if (!reader) return NULL;
    string_t *path = NULL, *key = NULL;
    bool configured = globe_configuration(&path, &key);
    string_t *stamp = configured ? globe_storage_stamp(path) : NULL;
    bool same = stamp && reader->globe && !string_compare(path, reader->path) &&
                !string_compare(key, reader->key) && !string_compare(stamp, reader->stamp);
    if (!same) {
        globe_reader_reset(reader);
        reader->database = stamp ? globe_database_open(path, key) : NULL;
        reader->globe = reader->database ? globe_open_databases(reader->database, reader->database, NULL) : NULL;
        if (reader->globe) {
            reader->path = path; reader->key = key;
            /* Opening SQLite may change journal metadata; record the post-open identity. */
            reader->stamp = globe_storage_stamp(path);
            path = key = NULL;
            if (!reader->stamp) globe_reader_reset(reader);
        } else globe_reader_reset(reader);
    }
    string_free(path); string_free(key); string_free(stamp);
    return reader->globe;
}

/* Close readers before their borrowed database and discard configuration secrets. */
void globe_reader_free(globe_reader_t *reader)
{
    if (!reader) return;
    globe_reader_reset(reader);
    free(reader);
}
