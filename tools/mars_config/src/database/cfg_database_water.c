/**
 * @file cfg_database_water.c
 * @brief Publish reviewed mapped-water geometry as a required portable globe asset.
 *
 * Fresh installations and managed migrations import the same water outlines, islands, recorded widths and
 * explicit survey coverage. A savepoint preserves existing geography when the packaged source is missing or
 * invalid. Resource resolution and bounded SQL streaming use the ordinary installer facilities.
 */
#define MARS_CFG_DATABASE_INTERNAL_ACCESS
#include "cfg_database_internal.h"

bool cfg_database_mapped_water(sqlite_t *db, const string_t *root)
{
    bool transaction = db && root && sqlite_exec_cstr(db, "SAVEPOINT globe_mapped_water");
    bool ok = transaction && cfg_database_source(db, root, "packaging/globe-db/mapped_water.sql", false) &&
              sqlite_exec_cstr(db, "RELEASE globe_mapped_water");
    if (transaction && !ok)
        sqlite_exec_cstr(db, "ROLLBACK TO globe_mapped_water; RELEASE globe_mapped_water");
    return ok;
}
