/**
 * @file cfg_database_plain.c
 * @brief Explicit, non-destructive conversion of installed catalogues to plaintext SQLite.
 *
 * The configuration command exports each keyed database to a new private file, validates it and then publishes
 * its saved path and empty key atomically. The original database and configuration backup remain recoverable.
 * Callers must stop readers and writers first; the source writer lock protects the export, not other processes'
 * cached configuration. No database is rebuilt and no source tables are removed.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mars_settings.h"
#include "cfg_storage.h"
#include "cfg_database.h"
#define MARS_CFG_DATABASE_INTERNAL_ACCESS
#include "cfg_database_internal.h"

static bool cfg_plain_integrity(sqlite_t *db)
{
    sqlite_stmt_t *stmt = sqlite_stmt_prepare(db, "PRAGMA integrity_check");
    bool ok = stmt && sqlite_stmt_step(stmt) == SQLITE_STEP_ROW &&
              !strcmp(sqlite_stmt_column_text(stmt, 0), "ok") && sqlite_stmt_step(stmt) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(stmt);
    return ok;
}

static bool cfg_plain_pragma(sqlite_t *db, const char *name)
{
    string_t *query = string_sprintf("PRAGMA main.%s", name);
    sqlite_stmt_t *stmt = query ? sqlite_stmt_prepare(db, string_c_str(query)) : NULL;
    bool ok = stmt && sqlite_stmt_step(stmt) == SQLITE_STEP_ROW;
    int64_t value = ok ? sqlite_stmt_column_int64(stmt, 0) : 0;
    sqlite_stmt_finalize(stmt);
    string_free(query);
    query = ok ? string_sprintf("PRAGMA plain.%s=%lld", name, (long long)value) : NULL;
    ok = query && sqlite_exec(db, query);
    string_free(query);
    return ok;
}

static bool cfg_plain_export(sqlite_t *db, const string_t *path)
{
    sqlite_stmt_t *attach = sqlite_stmt_prepare(db, "ATTACH DATABASE ?1 AS plain KEY ''");
    bool attached = attach && sqlite_stmt_bind_text(attach, 1, string_c_str(path)) &&
                    sqlite_stmt_step(attach) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(attach);
    bool transaction = attached && sqlite_exec_cstr(db, "BEGIN IMMEDIATE");
    bool ok = transaction && sqlite_exec_cstr(db, "SELECT sqlcipher_export('plain')") &&
              cfg_plain_pragma(db, "user_version") && cfg_plain_pragma(db, "application_id");
    if (ok) ok = sqlite_exec_cstr(db, "COMMIT");
    if (!ok && transaction) sqlite_exec_cstr(db, "ROLLBACK");
    if (attached) ok = sqlite_exec_cstr(db, "DETACH DATABASE plain") && ok;
    return ok;
}

static string_t *cfg_plain_settings(file_t *config, const char *path_name, const char *key_name,
                                     const string_t *path)
{
    string_t *body = string_new(), *line = NULL, *empty = string_new();
    bool ok = body && empty && file_open_read(config);
    while (ok) {
        ok = file_read_line(config, &line);
        if (!ok || !line) break;
        string_t *copy = string_clone(line);
        if (copy) string_trim(copy);
        size_t start = copy && string_starts_with(copy, "export ") ? 7 : 0;
        string_offset_t equals = copy ? string_find(copy, "=") : -1;
        string_t *name = equals >= (string_offset_t)start ? string_substr(copy, start, (size_t)equals - start) : NULL;
        if (name) string_trim(name);
        bool replace = name && (string_view_equals_literal(string_view_all(name), path_name) ||
                                 string_view_equals_literal(string_view_all(name), key_name) ||
                                 (!strcmp(key_name, "MARS_GLOBE_DB_KEY") &&
                                  string_view_equals_literal(string_view_all(name), "MARS_GLOBE_RIVER_DB_KEY")));
        ok = copy && (replace || (string_append_cstr(body, string_c_str(line)) >= 0 && !string_append_char(body, '\n')));
        string_free(name); string_free(copy); string_free(line); line = NULL;
    }
    if (file_is_open(config)) ok = file_close(config) && ok;
    ok = ok && cfg_database_export(body, path_name, path) && cfg_database_export(body, key_name, empty);
    string_free(line); string_free(empty);
    if (ok) return body;
    string_free(body);
    return NULL;
}

static bool cfg_plain_one(const char *name, const char *prefix)
{
    string_t *home = cfg_storage_home();
    string_t *directory = home ? string_sprintf("%s/config", string_c_str(home)) : NULL;
    string_t *config_path = directory ? string_sprintf("%s/%s-db.env", string_c_str(directory), name) : NULL;
    string_t *path_name = string_sprintf("MARS_%s_DB_PATH", prefix);
    string_t *key_name = string_sprintf("MARS_%s_DB_KEY", prefix);
    file_t *config = config_path ? file_new(config_path) : NULL;
    bool exists = false;
    bool ok = config && path_name && key_name && !getenv(string_c_str(path_name)) && !getenv(string_c_str(key_name)) &&
              cfg_storage_safe_regular(config, &exists) && exists;
    string_t *raw = ok ? cfg_storage_setting(config, string_c_str(path_name)) : NULL;
    string_t *path = raw && string_byte_length(raw) ? cfg_storage_expand_path(raw) : NULL;
    string_t *key = ok ? cfg_storage_setting(config, string_c_str(key_name)) : NULL;
    file_t *source = path ? file_new(path) : NULL;
    ok = ok && source && key && cfg_storage_safe_regular(source, &exists) && exists;
    sqlite_t *db = ok ? cfg_database_open(path, key) : NULL;
    bool plain = key && !string_byte_length(key);
    ok = ok && db;
    string_t *destination_path = ok && !plain ? string_sprintf("%s.plain.db", string_c_str(path)) : NULL;
    file_t *destination = destination_path ? file_new(destination_path) : NULL;
    string_t *backup_path = config_path ? string_sprintf("%s.encrypted-backup", string_c_str(config_path)) : NULL;
    file_t *backup = backup_path ? file_new(backup_path) : NULL;
    string_t *parent = path ? cfg_storage_parent(path) : NULL;
    string_t *stage = ok && !plain && parent ? cfg_database_stage(parent) : NULL;
    string_t *temporary_path = stage ? string_sprintf("%s/database.db", string_c_str(stage)) : NULL;
    file_t *temporary = temporary_path ? file_new(temporary_path) : NULL;
    if (ok && !plain) {
        ok = destination && backup && temporary && cfg_storage_safe_regular(destination, &exists) && !exists &&
             cfg_storage_safe_regular(backup, &exists) && !exists;
        if (ok) {
            printf("Exporting %s to plaintext; retaining the encrypted original...\n", name);
            fflush(stdout);
            ok = cfg_plain_export(db, temporary_path);
        }
    }
    sqlite_close(db);
    db = ok ? sqlite_open_plain(plain ? path : temporary_path) : NULL;
    ok = ok && db && cfg_plain_integrity(db);
    sqlite_close(db);
    if (ok && !plain) {
        string_t *body = cfg_plain_settings(config, string_c_str(path_name), string_c_str(key_name), destination_path);
        ok = body && file_chmod(temporary, 0600) && file_open_read(temporary);
        if (ok) { ok = file_sync(temporary, false); ok = file_close(temporary) && ok; }
        ok = ok && file_copy(config, backup, false) && file_chmod(backup, 0600) &&
             file_move(temporary, destination, false) && cfg_storage_publish(config, directory, body) && mars_settings_sync(body);
        string_free(body);
    }
    if (ok) printf("%s database: plaintext verified%s.\n", name, plain ? " (already configured)" : " and selected");
    else fprintf(stderr, "%s plaintext conversion failed; original database retained. Check overrides and output paths.\n", name);
    cfg_database_cleanup(stage);
    file_free(temporary); file_free(backup); file_free(destination); file_free(source); file_free(config);
    string_free(temporary_path); string_free(stage); string_free(parent); string_free(backup_path);
    string_free(destination_path); string_free(key); string_free(path); string_free(raw);
    string_free(key_name); string_free(path_name); string_free(config_path); string_free(directory); string_free(home);
    return ok;
}

/* Convert only explicitly named installed catalogues, stopping before later stores on failure. */
int cfg_databases_plain(void)
{
    static const char *const names[][2] = {{"globe", "GLOBE"}, {"jurisdiction", "JURISDICTION"}, {"almanac", "ALMANAC"}};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
        if (!cfg_plain_one(names[i][0], names[i][1])) return 1;
    return 0;
}
