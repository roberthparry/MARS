/**
 * @file cfg_database_jurisdiction.c
 * @brief Jurisdiction database settings, rule and flag imports, and calendar generation.
 *
 * Owns location and language selection, packaged jurisdiction rules, retirement of
 * withdrawn legacy river geometry and calendar population. The shared database
 * manager owns encryption, staging and publication.
 * Optional offline country artwork and its licence remain in the jurisdiction catalogue, separate from geometry.
 */
#include <stdlib.h>
#include <string.h>

#include "cfg_storage.h"
#include "cfg_calendar.h"
#define MARS_CFG_DATABASE_INTERNAL_ACCESS
#include "cfg_database_internal.h"

/* Remove only the withdrawn dataset; preserve future replacement sources. */
bool cfg_database_rivers(sqlite_t *db, const string_t *root, const string_t *staging)
{
    (void)root;
    (void)staging;
    if (!db) return false;
    sqlite_stmt_t *table = sqlite_stmt_prepare(db,
        "select 1 from sqlite_master where type='table' and name='rivers_dataset'");
    bool ok = table != NULL;
    sqlite_step_result_t step = table ? sqlite_stmt_step(table) : SQLITE_STEP_DONE;
    sqlite_stmt_finalize(table);
    if (!ok || step != SQLITE_STEP_ROW) return ok && step == SQLITE_STEP_DONE;
    sqlite_stmt_t *source = sqlite_stmt_prepare(db,
        "select 1 from rivers_dataset where id=1 and source like 'WDBII%'");
    step = source ? sqlite_stmt_step(source) : SQLITE_STEP_DONE;
    ok = source != NULL;
    sqlite_stmt_finalize(source);
    if (!ok || step != SQLITE_STEP_ROW) return ok && step == SQLITE_STEP_DONE;
    ok = sqlite_exec_cstr(db, "begin immediate");
    if (!ok) return false;
    ok = sqlite_exec_cstr(db,
        "drop table if exists rivers_bounds;drop table if exists rivers_chunk;drop table rivers_dataset;commit");
    if (!ok) sqlite_exec_cstr(db, "rollback");
    return ok;
}

static string_t *cfg_jurisdiction_argument(const char *argument, const char *environment)
{
    return cfg_storage_value(argument && *argument ? argument : getenv(environment));
}

static bool cfg_jurisdiction_options(cfg_database_options *options, const string_t *default_path,
                                     const char *location, const char *language)
{
    (void)default_path;
    options->requested_location = cfg_jurisdiction_argument(location, "MARS_CALENDAR_LOCATION_ARGUMENT");
    options->requested_language = cfg_jurisdiction_argument(language, "MARS_CALENDAR_LANGUAGE_ARGUMENT");
    options->saved_location = cfg_database_setting(options->configuration, NULL, "MARS_CALENDAR_LOCATION", NULL);
    options->saved_language = cfg_database_setting(options->configuration, NULL, "MARS_CALENDAR_LANGUAGE", NULL);
    return options->requested_location && options->requested_language && options->saved_location &&
           options->saved_language;
}

static bool cfg_jurisdiction_sources(sqlite_t *db, const string_t *root, const string_t *staging)
{
    return cfg_database_source(db, root, "packaging/jurisdiction-db/mars_holiday_rules.sql", false) &&
           cfg_database_rivers(db, root, staging);
}

/* Import catalogue-owned artwork independently of geometry and observer selection. */
bool cfg_database_flags(sqlite_t *db, const string_t *root)
{
    string_t *license_path = string_sprintf("%s/packaging/jurisdiction-db/flags/LICENSE", string_c_str(root));
    file_t *license = license_path ? file_new(license_path) : NULL;
    bool present = license && file_exists(license);
    string_t *terms = present ? file_read_all_text(license) : NULL;
    file_free(license); string_free(license_path);
    if (!present) { string_free(terms); return true; } /* Older/custom packages need no artwork. */
    if (!terms) return false;
    bool transaction = sqlite_exec_cstr(db, "savepoint country_flags");
    bool ok = transaction && sqlite_exec_cstr(db,
        "create table if not exists jurisdiction_country_flag(iso_country_code text primary key,svg text not null);"
        "create table if not exists jurisdiction_country_flag_source(id integer primary key,version text,source text,licence text)");
    sqlite_stmt_t *provenance = ok ? sqlite_stmt_prepare(db,
        "insert or replace into jurisdiction_country_flag_source values(1,'7.3.2','https://github.com/lipis/flag-icons',?1)") : NULL;
    ok = ok && provenance && sqlite_stmt_bind_text(provenance, 1, string_c_str(terms)) &&
         sqlite_stmt_step(provenance) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(provenance); string_free(terms);
    sqlite_stmt_t *countries = ok ? sqlite_stmt_prepare(db,
        "select jurisdiction_id from jurisdiction where jurisdiction_type in ('country','subdivision') "
        "order by jurisdiction_id") : NULL;
    sqlite_stmt_t *insert = ok ? sqlite_stmt_prepare(db,
        "insert into jurisdiction_country_flag values(?1,?2) on conflict(iso_country_code) do update set svg=excluded.svg") : NULL;
    ok = ok && countries && insert;
    sqlite_step_result_t step = SQLITE_STEP_ERROR;
    while (ok && (step = sqlite_stmt_step(countries)) == SQLITE_STEP_ROW) {
        const char *code = sqlite_stmt_column_text(countries, 0);
        size_t length = code ? strlen(code) : 0;
        if (!code || (length != 2 && (length < 4 || length > 6 || code[2] != '-')) ||
            code[0] < 'A' || code[0] > 'Z' || code[1] < 'A' || code[1] > 'Z') continue;
        char filename[7] = {0};
        bool valid = true;
        for (size_t i = 0; i < length; ++i) {
            bool letter = code[i] >= 'A' && code[i] <= 'Z';
            valid = valid && (letter || (code[i] >= '0' && code[i] <= '9') || (i == 2 && code[i] == '-'));
            filename[i] = letter ? code[i] + 'a' - 'A' : code[i];
        }
        if (!valid) continue;
        string_t *path = string_sprintf("%s/packaging/jurisdiction-db/flags/%s.svg", string_c_str(root), filename);
        file_t *source = path ? file_new(path) : NULL;
        file_info_t *info = source ? file_get_info(source) : NULL;
        string_t *svg = info && file_info_size(info) > 0 && file_info_size(info) <= 1024 * 1024 ?
                        file_read_all_text(source) : NULL;
        if (svg) {
            ok = sqlite_stmt_bind_text(insert, 1, code) && sqlite_stmt_bind_text(insert, 2, string_c_str(svg)) &&
                 sqlite_stmt_step(insert) == SQLITE_STEP_DONE;
            sqlite_stmt_reset(insert);
        }
        string_free(svg); file_info_free(info); file_free(source); string_free(path);
    }
    ok = ok && step == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(countries); sqlite_stmt_finalize(insert);
    if (ok) ok = sqlite_exec_cstr(db, "release country_flags");
    if (!ok && transaction) sqlite_exec_cstr(db, "rollback to country_flags; release country_flags");
    return ok;
}

static bool cfg_jurisdiction_populate(sqlite_t *db, const string_t *path, const cfg_database_options *options,
                                      bool interactive, string_t **location, string_t **language)
{
    const char *configured_root = getenv("MARS_ROOT");
    string_t *root = string_new_with(configured_root && *configured_root ? configured_root : MARS_CONFIG_ROOT_DIR);
    bool flags = root && cfg_database_flags(db, root);
    string_free(root);
    return flags && cfg_calendar_populate(db, path, options->key, options->requested_location,
                                 options->saved_location, interactive, options->requested_language,
                                 options->saved_language, location, language);
}

static bool cfg_jurisdiction_export(string_t *body, const string_t *location, const string_t *language)
{
    return location && language && cfg_database_export(body, "MARS_CALENDAR_LOCATION", location) &&
           cfg_database_export(body, "MARS_CALENDAR_LANGUAGE", language);
}

const cfg_database_profile cfg_database_jurisdiction = {
    .name = "jurisdiction",
    .title = "Jurisdiction",
    .default_path = "jurisdiction/mars_jurisdiction_rules.db",
    .path_variable = "MARS_JURISDICTION_DB_PATH",
    .key_variable = "MARS_JURISDICTION_DB_KEY",
    .read_options = cfg_jurisdiction_options,
    .import_sources = cfg_jurisdiction_sources,
    .populate = cfg_jurisdiction_populate,
    .export_settings = cfg_jurisdiction_export,
};
