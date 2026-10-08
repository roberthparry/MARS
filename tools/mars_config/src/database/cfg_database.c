/**
 * @file cfg_database.c
 * @brief Native almanac and jurisdiction database installer orchestration.
 *
 * Resolves command-line, environment and saved settings; migrates the legacy
 * almanac default path; prompts for passwords and builds encrypted private
 * databases from repository SQL. Calendar configuration is delegated through
 * cfg_calendar.h. Imports and calendar work finish before publication, and
 * neither SQL text nor database error strings containing secrets are printed.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "cfg_calendar.h"
#include "cfg_database.h"
#include "cfg_database_internal.h"
#include "cfg_prompt.h"
#include "cfg_storage.h"

typedef struct cfg_database_options {
    string_t *home;
    string_t *configuration_directory;
    string_t *configuration_path;
    file_t *configuration;
    string_t *path;
    string_t *expanded_path;
    string_t *key;
    string_t *requested_location;
    string_t *saved_location;
    string_t *requested_language;
    string_t *saved_language;
} cfg_database_options;

static void cfg_database_options_free(cfg_database_options *options)
{
    string_free(options->home);
    string_free(options->configuration_directory);
    string_free(options->configuration_path);
    file_free(options->configuration);
    string_free(options->path);
    string_free(options->expanded_path);
    string_free(options->key);
    string_free(options->requested_location);
    string_free(options->saved_location);
    string_free(options->requested_language);
    string_free(options->saved_language);
}

static string_t *cfg_database_setting(file_t *configuration, const char *explicit_value, const char *name,
                                      const string_t *fallback)
{
    string_t *saved = cfg_storage_setting(configuration, name);
    string_t *value = cfg_storage_value(explicit_value);
    if (value && !string_byte_length(value)) {
        string_free(value);
        value = cfg_storage_value(getenv(name));
    }
    if (value && !string_byte_length(value)) {
        string_free(value);
        value = saved ? string_clone(string_byte_length(saved) || !fallback ? saved : fallback) : NULL;
    }
    if (!saved) {
        string_free(value);
        value = NULL;
    }
    string_free(saved);
    return value;
}

static string_t *cfg_database_argument(const char *argument, const char *environment)
{
    return cfg_storage_value(argument && *argument ? argument : getenv(environment));
}

static bool cfg_database_options_read(cfg_database_options *options, bool jurisdiction, const char *path,
                                      const char *key, const char *location, const char *language)
{
    options->home = cfg_storage_home();
    if (!options->home)
        return false;
    options->configuration_directory = string_sprintf("%s/config", string_c_str(options->home));
    options->configuration_path =
        string_sprintf("%s/config/%s-db.env", string_c_str(options->home), jurisdiction ? "jurisdiction" : "almanac");
    options->configuration = options->configuration_path ? file_new(options->configuration_path) : NULL;
    string_t *default_path =
        string_sprintf("%s/%s", string_c_str(options->home),
                       jurisdiction ? "jurisdiction/mars_jurisdiction_rules.db" : "almanac/almanac.db");
    if (!options->configuration_directory || !options->configuration || !default_path) {
        string_free(default_path);
        return false;
    }
    options->path =
        cfg_database_setting(options->configuration, path,
                             jurisdiction ? "MARS_JURISDICTION_DB_PATH" : "MARS_ALMANAC_DB_PATH", default_path);
    options->key = cfg_database_setting(options->configuration, key,
                                        jurisdiction ? "MARS_JURISDICTION_DB_KEY" : "MARS_ALMANAC_DB_KEY", NULL);
    options->expanded_path = options->path ? cfg_storage_expand_path(options->path) : NULL;
    string_t *legacy = string_sprintf("%s/almanac.db", string_c_str(options->home));
    bool ok = options->path && options->key && options->expanded_path && legacy;
    if (ok && !jurisdiction && !string_compare(options->expanded_path, legacy)) {
        string_free(options->path);
        string_free(options->expanded_path);
        options->path = string_clone(default_path);
        options->expanded_path = string_clone(default_path);
        ok = options->path && options->expanded_path;
    }
    string_free(legacy);
    string_free(default_path);
    if (ok && jurisdiction) {
        options->requested_location = cfg_database_argument(location, "MARS_CALENDAR_LOCATION_ARGUMENT");
        options->requested_language = cfg_database_argument(language, "MARS_CALENDAR_LANGUAGE_ARGUMENT");
        options->saved_location = cfg_database_setting(options->configuration, NULL, "MARS_CALENDAR_LOCATION", NULL);
        options->saved_language = cfg_database_setting(options->configuration, NULL, "MARS_CALENDAR_LANGUAGE", NULL);
        ok = options->requested_location && options->requested_language && options->saved_location &&
             options->saved_language;
    }
    return ok;
}

static string_t *cfg_database_password(const string_t *saved, bool jurisdiction)
{
    string_t *prompt = string_sprintf(
        "Choose a password to protect your private %s database%s: ", jurisdiction ? "jurisdiction" : "almanac",
        string_byte_length(saved) ? " [leave blank to keep current key]" : "");
    if (!prompt)
        return NULL;
    string_t *answer = NULL;
    for (;;) {
        answer = cfg_prompt_read(string_c_str(prompt), true);
        if (!answer)
            break;
        if (!string_byte_length(answer) && string_byte_length(saved)) {
            string_free(answer);
            answer = string_clone(saved);
            break;
        }
        if (!string_byte_length(answer)) {
            fputs("A database password is required.\n", stderr);
            string_free(answer);
            continue;
        }
        string_t *repeat = cfg_prompt_read("Confirm password: ", true);
        bool cancelled = !repeat;
        bool matches = repeat && !string_compare(answer, repeat);
        string_free(repeat);
        if (matches)
            break;
        string_free(answer);
        answer = NULL;
        if (cancelled)
            break;
        fputs("Passwords did not match. Please try again.\n", stderr);
    }
    string_free(prompt);
    return answer;
}

static bool cfg_database_export(string_t *body, const char *name, const string_t *value)
{
    string_t *quoted = cfg_storage_quote(value);
    bool ok = quoted && string_append_format(body, "export %s=", name) >= 0 &&
              !string_append_utf8_exact(body, string_c_str(quoted), string_byte_length(quoted)) &&
              !string_append_char(body, '\n');
    string_free(quoted);
    return ok;
}

static string_t *cfg_database_configuration(const cfg_database_options *options, bool jurisdiction,
                                            const string_t *location, const string_t *language)
{
    string_t *body = string_sprintf("# Generated by mars_config %s\n", jurisdiction ? "jurisdiction" : "almanac");
    bool ok =
        body &&
        cfg_database_export(body, jurisdiction ? "MARS_JURISDICTION_DB_PATH" : "MARS_ALMANAC_DB_PATH", options->path) &&
        cfg_database_export(body, jurisdiction ? "MARS_JURISDICTION_DB_KEY" : "MARS_ALMANAC_DB_KEY", options->key);
    if (ok && jurisdiction)
        ok = location && language && cfg_database_export(body, "MARS_CALENDAR_LOCATION", location) &&
             cfg_database_export(body, "MARS_CALENDAR_LANGUAGE", language);
    if (!ok) {
        string_free(body);
        body = NULL;
    }
    return body;
}

static bool cfg_database_source(sqlite_t *db, const string_t *root, const char *name, bool optional)
{
    string_t *path = string_new_with(name);
    string_t *full = string_sprintf("%s/%s", string_c_str(root), name);
    file_t *file = full ? file_new(full) : NULL;
    bool exists = file && file_exists(file);
    bool ok = path && file &&
              ((optional && !exists && !file_last_error(file)) || (exists && cfg_database_import(db, root, path)));
    file_free(file);
    string_free(full);
    string_free(path);
    return ok;
}

static bool cfg_database_build(cfg_database_options *options, bool jurisdiction, bool interactive)
{
    string_t *parent = cfg_storage_parent(options->expanded_path);
    bool ok = parent && cfg_storage_private_directory(options->home, true) &&
              cfg_storage_private_directory(parent, true) &&
              cfg_storage_private_directory(options->configuration_directory, true);
    string_t *staging = ok ? cfg_database_stage(parent) : NULL;
    string_t *temporary_path = staging ? string_sprintf("%s/database.db", string_c_str(staging)) : NULL;
    file_t *temporary = temporary_path ? file_new(temporary_path) : NULL;
    file_t *destination = file_new(options->expanded_path);
    string_t *root = cfg_storage_value(getenv("MARS_ROOT"));
    if (root && !string_byte_length(root)) {
        string_free(root);
        root = string_new_with(MARS_CONFIG_ROOT_DIR);
    }
    sqlite_t *db = temporary && root ? sqlite_open_encrypted(temporary_path, options->key) : NULL;
    ok = ok && db && destination && sqlite_exec_cstr(db, "PRAGMA foreign_keys = ON;");
    if (ok) {
        fprintf(stderr, "Building encrypted %s database...\n", jurisdiction ? "jurisdiction" : "almanac");
        ok = cfg_database_source(db, root,
                                 jurisdiction ? "packaging/jurisdiction-db/mars_holiday_rules.sql"
                                              : "packaging/almanac-db/mars_almanac.sql",
                                 false);
    }
    if (ok && !jurisdiction)
        ok = cfg_database_source(db, root, "packaging/almanac-db/mars_almanac_chebyshev.sql", true) &&
             cfg_database_source(db, root, "packaging/almanac-db/mars_almanac_frame_rotation.sql", true);
    /* Calendar's separate native reader must see committed rules. */
    if (ok)
        ok = sqlite_exec_cstr(db, "BEGIN; COMMIT;");
    string_t *chosen_location = NULL, *chosen_language = NULL;
    if (ok && jurisdiction)
        ok = cfg_calendar_populate(db, temporary_path, options->key, options->requested_location,
                                   options->saved_location, interactive, options->requested_language,
                                   options->saved_language, &chosen_location, &chosen_language);
    /* An unfinished source transaction must fail instead of being rolled back silently on close. */
    if (ok)
        ok = sqlite_exec_cstr(db, "BEGIN; COMMIT; PRAGMA wal_checkpoint(TRUNCATE); PRAGMA journal_mode=DELETE;");
    sqlite_close(db);
    string_t *body = ok ? cfg_database_configuration(options, jurisdiction, chosen_location, chosen_language) : NULL;
    ok = ok && body &&
         cfg_database_publish(temporary, destination, options->configuration, options->configuration_directory, staging,
                              body);
    if (ok) {
        printf("Built %s database at %s\nWrote %s database configuration to %s\n",
               jurisdiction ? "jurisdiction" : "almanac", string_c_str(options->path),
               jurisdiction ? "jurisdiction" : "almanac", string_c_str(options->configuration_path));
        if (jurisdiction)
            printf("calendar_local language: %s\ncalendar_local location: %s\n", string_c_str(chosen_language),
                   string_c_str(chosen_location));
    }
    string_free(body);
    string_free(chosen_location);
    string_free(chosen_language);
    file_free(destination);
    file_free(temporary);
    string_free(temporary_path);
    cfg_database_cleanup(staging);
    string_free(staging);
    string_free(parent);
    string_free(root);
    return ok;
}

/* Configure only the selected private database, using literal settings and native SQLCipher. */
int cfg_database_run(bool jurisdiction, const char *path, const char *key, const char *location, const char *language,
                     bool interactive)
{
    cfg_database_options options = {0};
    bool ok = cfg_database_options_read(&options, jurisdiction, path, key, location, language);
    interactive = interactive && isatty(STDIN_FILENO);
    if (ok && interactive) {
        printf("%s database setup\nConfig file: %s\n", jurisdiction ? "Jurisdiction" : "Almanac",
               string_c_str(options.configuration_path));
        string_t *chosen = cfg_database_password(options.key, jurisdiction);
        string_free(options.key);
        options.key = chosen;
        ok = chosen != NULL;
    }
    string_t *quoted_key = ok ? cfg_storage_quote(options.key) : NULL;
    string_t *quoted_path = ok ? cfg_storage_quote(options.path) : NULL;
    ok = ok && quoted_key && quoted_path && string_byte_length(options.key) && string_byte_length(options.path);
    string_free(quoted_key);
    string_free(quoted_path);
    if (ok)
        ok = cfg_database_build(&options, jurisdiction, interactive);
    if (!ok)
        fprintf(stderr, "Failed to configure the %s database; check settings, private storage and packaged SQL.\n",
                jurisdiction ? "jurisdiction" : "almanac");
    cfg_database_options_free(&options);
    return ok ? 0 : 1;
}
