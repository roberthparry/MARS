/**
 * @file cfg_database_almanac.c
 * @brief Almanac database settings and packaged ephemeris import policy.
 *
 * Owns legacy-path migration and the almanac SQL source sequence. The shared
 * database installer manages credentials, staging, transactions and publication.
 */
#define MARS_CFG_DATABASE_INTERNAL_ACCESS
#include "cfg_database_internal.h"

static bool cfg_almanac_options(cfg_database_options *options, const string_t *default_path,
                                const char *location, const char *language)
{
    (void)location;
    (void)language;
    string_t *legacy = string_sprintf("%s/almanac.db", string_c_str(options->home));
    bool ok = legacy != NULL;
    if (ok && !string_compare(options->expanded_path, legacy)) {
        string_free(options->path);
        string_free(options->expanded_path);
        options->path = string_clone(default_path);
        options->expanded_path = string_clone(default_path);
        ok = options->path && options->expanded_path;
    }
    string_free(legacy);
    return ok;
}

static bool cfg_almanac_sources(sqlite_t *db, const string_t *root, const string_t *staging)
{
    (void)staging;
    return cfg_database_source(db, root, "packaging/almanac-db/mars_almanac.sql", false) &&
           cfg_database_source(db, root, "packaging/almanac-db/mars_almanac_chebyshev.sql", true) &&
           cfg_database_source(db, root, "packaging/almanac-db/mars_almanac_frame_rotation.sql", true);
}

const cfg_database_profile cfg_database_almanac = {
    .name = "almanac",
    .title = "Almanac",
    .default_path = "almanac/almanac.db",
    .path_variable = "MARS_ALMANAC_DB_PATH",
    .key_variable = "MARS_ALMANAC_DB_KEY",
    .read_options = cfg_almanac_options,
    .import_sources = cfg_almanac_sources,
};
