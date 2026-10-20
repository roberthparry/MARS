/**
 * @file cfg_database_rivers.c
 * @brief Retire unsuitable river geometry during jurisdiction installation.
 *
 * The historical WDBII network was withdrawn because it omits town-scale meanders. Keep unrelated tables intact;
 * a replacement dataset must be geographically validated before packaging. No download is performed here.
 */
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
