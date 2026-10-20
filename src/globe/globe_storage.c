/**
 * @file globe_storage.c
 * @brief Resolve independent globe storage without creating databases or interpreting shell commands.
 *
 * The globe controller owns the returned encrypted SQLite connection. Environment overrides precede literal
 * assignments in globe-db.env. Missing or malformed configuration fails closed; no legacy database is consulted.
 */
#include <stdlib.h>

#include "string-t.h"
#define MARS_GLOBE_INTERNAL_ACCESS
#include "globe_layers.h"

static string_t *globe_setting(file_t *config, const char *name)
{
    const char *environment = getenv(name);
    if (environment && *environment) return string_new_with(environment);
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

/* Open only an existing, independently configured geometry database. */
sqlite_t *globe_configured_database(void)
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
    file_t *database = expanded ? file_new(expanded) : NULL;
    sqlite_t *db = database && file_exists(database) && key && string_byte_length(key)
                       ? sqlite_open_encrypted(expanded, key) : NULL;
    if (db && !sqlite_exec_cstr(db, "PRAGMA query_only=ON; PRAGMA cache_size=-512;")) {
        sqlite_close(db);
        db = NULL;
    }
    file_free(database); file_free(config);
    string_free(expanded); string_free(path); string_free(key);
    string_free(config_path); string_free(root); string_free(raw_home);
    return db;
}
