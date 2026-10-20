/**
 * @file cfg_database_globe.c
 * @brief Independent encrypted shoreline database installation and verification.
 *
 * Publishes only validated shoreline storage, without altering jurisdiction or almanac files. The first installer
 * validates and preserves existing databases; regional updates and recoverable removal are reserved
 * commands until their migration and live-reader coordination are implemented. No network download is performed.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "globe-t.h"
#include "cfg_database.h"
#include "cfg_prompt.h"
#include "cfg_storage.h"
#define MARS_CFG_DATABASE_INTERNAL_ACCESS
#include "cfg_database_internal.h"

static string_t *cfg_globe_setting(file_t *config, const char *name, const string_t *fallback)
{
    const char *environment = getenv(name);
    string_t *value = environment && *environment ? cfg_storage_value(environment) : cfg_storage_setting(config, name);
    if (value && !string_byte_length(value) && fallback) {
        string_free(value);
        value = string_clone(fallback);
    }
    return value;
}

static bool cfg_globe_valid(sqlite_t *db)
{
    sqlite_stmt_t *q = db ? sqlite_stmt_prepare(db, "PRAGMA integrity_check") : NULL;
    const char *result = q && sqlite_stmt_step(q) == SQLITE_STEP_ROW ? sqlite_stmt_column_text(q, 0) : NULL;
    bool ok = result && !strcmp(result, "ok");
    sqlite_stmt_finalize(q);
    globe_t *globe = ok ? globe_open_databases(db, db, NULL) : NULL;
    ok = ok && globe && globe_has_layer(globe, GLOBE_LAYER_SHORELINE);
    globe_close(globe);
    return ok;
}

/* Keep globe setup independent; unsupported operations fail without changing storage. */
static int cfg_globe_execute(const char *action, const char *explicit_key)
{
    bool install = action && !strcmp(action, "globe-install");
    bool check = action && !strcmp(action, "globe-check");
    if (!install && !check) {
        fputs("Globe update/removal is not implemented yet; no files changed.\n", stderr);
        return 2;
    }
    string_t *home = cfg_storage_home();
    if (!home) return 1;
    string_t *config_dir = string_sprintf("%s/config", string_c_str(home));
    string_t *config_path = string_sprintf("%s/config/globe-db.env", string_c_str(home));
    string_t *default_path = string_sprintf("%s/globe/globe.db", string_c_str(home));
    file_t *config = config_path ? file_new(config_path) : NULL;
    string_t *path = config ? cfg_globe_setting(config, "MARS_GLOBE_DB_PATH", default_path) : NULL;
    string_t *key = explicit_key ? string_new_with(explicit_key) :
                    config ? cfg_globe_setting(config, "MARS_GLOBE_DB_KEY", NULL) : NULL;
    if (install && !explicit_key && key && !string_byte_length(key) && isatty(STDIN_FILENO)) {
        string_free(key);
        key = cfg_prompt_read("Password for the new globe database: ", true);
        string_t *repeat = key ? cfg_prompt_read("Confirm password: ", true) : NULL;
        if (!repeat || string_compare(key, repeat)) {
            string_free(key);
            key = NULL;
        }
        string_free(repeat);
    }
    string_t *expanded = path ? cfg_storage_expand_path(path) : NULL;
    string_t *parent = expanded ? cfg_storage_parent(expanded) : NULL;
    file_t *destination = expanded ? file_new(expanded) : NULL;
    bool exists = false;
    bool ok = config_dir && config && parent && destination && key && string_byte_length(key) &&
              cfg_storage_safe_regular(destination, &exists);
    string_t *staging = NULL, *temporary_path = NULL, *root = NULL, *body = NULL;
    file_t *temporary = NULL;
    sqlite_t *db = NULL;
    if (ok && (check || exists)) {
        db = exists ? sqlite_open_encrypted(expanded, key) : NULL;
        ok = db && sqlite_exec_cstr(db, "PRAGMA query_only=ON; PRAGMA cache_size=-512;") && cfg_globe_valid(db);
        sqlite_close(db);
    } else if (ok && install) {
        ok = !exists && cfg_storage_private_directory(home, true) && cfg_storage_private_directory(parent, true) &&
             cfg_storage_private_directory(config_dir, true);
        staging = ok ? cfg_database_stage(parent) : NULL;
        temporary_path = staging ? string_sprintf("%s/database.db", string_c_str(staging)) : NULL;
        temporary = temporary_path ? file_new(temporary_path) : NULL;
        root = cfg_storage_value(getenv("MARS_ROOT"));
        if (root && !string_byte_length(root)) {
            string_free(root);
            root = string_new_with(MARS_CONFIG_ROOT_DIR);
        }
        db = temporary && root ? sqlite_open_encrypted(temporary_path, key) : NULL;
        ok = ok && db && sqlite_exec_cstr(db, "PRAGMA cache_size=-512; PRAGMA temp.cache_size=-512;") &&
             cfg_database_coast(db, root, staging) && cfg_globe_valid(db);
        sqlite_close(db);
        string_t *quoted_path = path ? cfg_storage_quote(path) : NULL;
        string_t *quoted_key = key ? cfg_storage_quote(key) : NULL;
        body = ok && quoted_path && quoted_key ? string_sprintf(
            "# Generated by mars_config globe-install\nexport MARS_GLOBE_DB_PATH=%s\nexport MARS_GLOBE_DB_KEY=%s\n",
            string_c_str(quoted_path), string_c_str(quoted_key)) : NULL;
        string_free(quoted_path);
        string_free(quoted_key);
        ok = ok && body && cfg_database_publish(temporary, destination, config, config_dir, staging, body);
    }
    if (ok) puts(install ? "Globe database ready: existing data preserved; shoreline available."
                        : "Globe database integrity and shoreline availability verified.");
    else fputs("Globe database operation failed; check its path, key and packaged shoreline source.\n", stderr);
    file_free(temporary);
    cfg_database_cleanup(staging);
    string_free(staging); string_free(temporary_path); string_free(root); string_free(body);
    file_free(destination); file_free(config);
    string_free(expanded); string_free(parent); string_free(path); string_free(key);
    string_free(default_path); string_free(config_path); string_free(config_dir); string_free(home);
    return ok ? 0 : 1;
}

/* Dispatch standalone globe operations using their saved settings. */
int cfg_globe_run(const char *action)
{
    return cfg_globe_execute(action, NULL);
}

/* The umbrella installer owns password selection; never prompt again here. */
int cfg_globe_install_key(const char *key)
{
    return key && *key ? cfg_globe_execute("globe-install", key) : 1;
}
