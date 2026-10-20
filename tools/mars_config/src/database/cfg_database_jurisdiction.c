/**
 * @file cfg_database_jurisdiction.c
 * @brief Jurisdiction database settings, rule imports and calendar generation.
 *
 * Owns location and language selection, packaged jurisdiction rules, retirement of
 * withdrawn legacy river geometry and calendar population. The shared database
 * manager owns encryption, staging and publication.
 */
#include <stdlib.h>

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

static bool cfg_jurisdiction_populate(sqlite_t *db, const string_t *path, const cfg_database_options *options,
                                      bool interactive, string_t **location, string_t **language)
{
    return cfg_calendar_populate(db, path, options->key, options->requested_location,
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
