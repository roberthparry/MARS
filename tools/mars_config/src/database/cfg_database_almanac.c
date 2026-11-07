/**
 * @file cfg_database_almanac.c
 * @brief Almanac database settings and packaged ephemeris import policy.
 *
 * Owns the almanac SQL source sequence and option policy. The shared
 * database installer manages credentials, staging, transactions and publication.
 */
#define MARS_CFG_DATABASE_INTERNAL_ACCESS
#include "cfg_database_internal.h"

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
    .import_sources = cfg_almanac_sources,
};
