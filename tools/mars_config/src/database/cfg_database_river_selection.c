/**
 * @file cfg_database_river_selection.c
 * @brief Prepare a smaller river-class candidate while preserving complete retained waterways.
 *
 * Copies indexed OSM river chunks into a new plaintext database. Selection uses the recorded waterway class,
 * rather than a reach's length, name or missing width, so main tributaries and narrow connecting reaches remain
 * intact. Coordinates, anchors and spatial bounds are copied exactly. The source and installed globe are unchanged.
 */
#include <string.h>

#include "cfg_progress.h"
#define MARS_CFG_DATABASE_INTERNAL_ACCESS
#include "cfg_database_internal.h"

static bool selected_integrity(sqlite_t *db)
{
    sqlite_stmt_t *q = sqlite_stmt_prepare(db, "pragma mars_selected.quick_check");
    bool ok = q && sqlite_stmt_step(q) == SQLITE_STEP_ROW;
    const char *text = ok ? sqlite_stmt_column_text(q, 0) : NULL;
    ok = text && !strcmp(text, "ok") && sqlite_stmt_step(q) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(q);
    return ok;
}

/* Produce an independent plaintext candidate; keep every byte of each retained river's geometry. */
bool cfg_database_select_main_rivers(sqlite_t *source, const string_t *output)
{
    if (!source || !output) return false;
    sqlite_stmt_t *q = sqlite_stmt_prepare(source,
        "select 1 from rivers_dataset where id=1 and format_version=2 "
        "and source='OpenStreetMap centreline export / ODbL' and point_count>=2 and feature_count>0");
    bool ok = q && sqlite_stmt_step(q) == SQLITE_STEP_ROW;
    sqlite_stmt_finalize(q);
    file_t *file = ok ? file_new(output) : NULL;
    bool created = file && file_open(file, FILE_MODE_CREATE_NEW, FILE_ACCESS_WRITE);
    ok = created && file_close(file) && file_chmod(file, 0600);
    q = ok ? sqlite_stmt_prepare(source, "attach database ?1 as mars_selected key ''") : NULL;
    bool attached = q && sqlite_stmt_bind_text(q, 1, string_c_str(output)) &&
                    sqlite_stmt_step(q) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(q);
    cfg_progress_t *progress = attached ? cfg_progress_start("Selecting main river channels") : NULL;
    ok = ok && attached && sqlite_exec_cstr(source,
        "pragma mars_selected.cache_size=-16384;pragma mars_selected.synchronous=full;");
    bool transaction = ok && sqlite_exec_cstr(source, "begin");
    ok = transaction && sqlite_exec_cstr(source,
        "create table mars_selected.rivers_chunk("
        "id integer primary key,feature_id integer,kind integer,vertices blob,anchors blob);"
        "insert into mars_selected.rivers_chunk "
        "select id,feature_id,kind,vertices,anchors from main.rivers_chunk where kind=1;"
        "create index mars_selected.rivers_feature_chunks on rivers_chunk(feature_id,id);");
    cfg_progress_finish(progress, ok);
    progress = ok ? cfg_progress_start("Indexing retained river channels") : NULL;
    ok = ok && sqlite_exec_cstr(source,
        "create virtual table mars_selected.rivers_bounds using rtree_i32(id,min_x,max_x,min_y,max_y);"
        "insert into mars_selected.rivers_bounds "
        "select b.id,b.min_x,b.max_x,b.min_y,b.max_y "
        "from mars_selected.rivers_chunk c cross join main.rivers_bounds b where b.id=c.id;"
        "create table mars_selected.rivers_dataset(id integer primary key,format_version integer,source text,"
        "point_count integer,feature_count integer,source_point_count integer,storage_tolerance_metres real);"
        "insert into mars_selected.rivers_dataset "
        "select d.id,d.format_version,d.source,t.points-t.chunks+t.features,t.features,"
        "d.source_point_count,d.storage_tolerance_metres "
        "from main.rivers_dataset d cross join "
        "(select sum(length(vertices)/8) as points,count(*) as chunks,count(distinct feature_id) as features "
        "from mars_selected.rivers_chunk) t where d.id=1;"
        "create table mars_selected.rivers_selection("
        "id integer primary key,policy text,source_features integer,retained_features integer);"
        "insert into mars_selected.rivers_selection "
        "select 1,'OSM waterway=river; complete retained ways',a.feature_count,b.feature_count "
        "from main.rivers_dataset a cross join mars_selected.rivers_dataset b where a.id=1 and b.id=1;");
    cfg_progress_finish(progress, ok);
    progress = ok ? cfg_progress_start("Validating retained river channels") : NULL;
    q = ok ? sqlite_stmt_prepare(source,
        "select 1 from mars_selected.rivers_dataset d where d.id=1 and d.point_count>=2 and d.feature_count>0 "
        "and d.source_point_count>=d.point_count "
        "and (select count(*) from mars_selected.rivers_chunk)="
        "(select count(*) from mars_selected.rivers_bounds) "
        "and not exists(select 1 from mars_selected.rivers_chunk where kind<>1 "
        "or length(vertices)<16 or length(vertices)>2056 or length(vertices)%8<>0 "
        "or length(anchors)<>length(vertices)/8)") : NULL;
    ok = q && sqlite_stmt_step(q) == SQLITE_STEP_ROW;
    sqlite_stmt_finalize(q);
    ok = ok && selected_integrity(source);
    cfg_progress_finish(progress, ok);
    if (transaction) ok = cfg_database_finish_transaction(source, ok);
    bool detached = !attached || sqlite_exec_cstr(source, "detach database mars_selected");
    if (!ok && created && detached && file_exists(file)) file_delete(file);
    file_free(file);
    return ok && detached;
}
