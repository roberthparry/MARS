/**
 * @file cfg_database_globe_copy.c
 * @brief Preserve non-photographic globe data in a fresh staged database.
 *
 * Copies tables through SQLite's public SQL interface from the attached previous database. Rowid tables use
 * bounded keyset batches; virtual and WITHOUT ROWID tables use native bulk insertion. Geometry and source
 * values are unchanged. Shadow tables are owned by their virtual-table modules and are never copied directly.
 * Deferred indexes, views, triggers and sequence state preserve unrelated user schema without replaying triggers.
 */
#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "cfg_progress.h"
#define MARS_CFG_DATABASE_INTERNAL_ACCESS
#include "cfg_database_internal.h"

static const char retained_tables[] =
    "SELECT s.name, s.sql, t.type, t.wr\n"
    "FROM previous.sqlite_schema AS s\n"
    "JOIN pragma_table_list AS t ON t.schema='previous' AND t.name=s.name\n"
    "WHERE s.type='table' AND t.type IN ('table','virtual')\n"
    "  AND s.name NOT GLOB 'sqlite_*'\n"
    "  AND s.name NOT IN ('globe_texture','globe_texture_dataset','globe_texture_manifest',\n"
    "                     'globe_texture_region','globe_texture_region_index')\n"
    "ORDER BY t.type, s.name";

static string_t *quoted_name(const char *name)
{
    string_t *escaped = name ? string_new_with(name) : NULL;
    bool ok = escaped && string_replace(escaped, "\"", "\"\"") >= 0;
    string_t *result = ok ? string_sprintf("\"%s\"", string_c_str(escaped)) : NULL;
    string_free(escaped);
    return result;
}

static bool scalar(sqlite_t *db, const char *sql, int64_t *value)
{
    sqlite_stmt_t *q = sqlite_stmt_prepare(db, sql);
    bool ok = q && sqlite_stmt_step(q) == SQLITE_STEP_ROW;
    if (ok) *value = sqlite_stmt_column_int64(q, 0);
    ok = ok && sqlite_stmt_step(q) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(q);
    return ok;
}

static string_t *column_list(sqlite_t *db, const char *name, bool *rowid)
{
    sqlite_stmt_t *q = sqlite_stmt_prepare(db,
        "SELECT group_concat('\"'||replace(name,'\"','\"\"')||'\"',',')\n"
        "FROM pragma_table_xinfo(?1,'previous') WHERE hidden=0");
    bool ok = q && sqlite_stmt_bind_text(q, 1, name) && sqlite_stmt_step(q) == SQLITE_STEP_ROW;
    const char *columns = ok ? sqlite_stmt_column_text(q, 0) : NULL;
    string_t *result = columns ? string_new_with(columns) : NULL;
    sqlite_stmt_finalize(q);
    if (!result || !*rowid) return result;
    q = sqlite_stmt_prepare(db,
        "SELECT alias FROM (SELECT '_rowid_' AS alias,0 AS priority UNION ALL SELECT 'rowid',1\n"
        "                   UNION ALL SELECT 'oid',2)\n"
        "WHERE NOT EXISTS (SELECT 1 FROM pragma_table_xinfo(?1,'previous') WHERE lower(name)=alias)\n"
        "ORDER BY priority LIMIT 1");
    bool bound = q && sqlite_stmt_bind_text(q, 1, name);
    sqlite_step_result_t step = bound ? sqlite_stmt_step(q) : SQLITE_STEP_ERROR;
    ok = bound && step == SQLITE_STEP_ROW;
    string_t *with_rowid = ok ? string_sprintf("\"%s\",%s", sqlite_stmt_column_text(q, 0),
                                               string_c_str(result)) : NULL;
    sqlite_stmt_finalize(q);
    /* With all aliases shadowed, the implicit rowid is inaccessible even to the owning application. */
    if (bound && step == SQLITE_STEP_DONE) {
        *rowid = false;
        return result;
    }
    string_free(result);
    return with_rowid;
}

static bool copy_rows(sqlite_t *db, const char *name, bool rowid, bool batch, cfg_progress_t *progress)
{
    string_t *table = quoted_name(name), *columns = column_list(db, name, &rowid);
    batch = batch && rowid;
    string_t *sql = table ? string_sprintf("SELECT count(*) FROM previous.%s", string_c_str(table)) : NULL;
    int64_t total = 0, copied = 0;
    bool ok = sql && columns && scalar(db, string_c_str(sql), &total) && total >= 0;
    string_free(sql);
    static const char label[] = "Preserving globe data";
    if (ok) cfg_progress_update_items(progress, label, 0, total, "rows copied");
    if (ok && batch) {
        /* The first column is a verified unshadowed rowid alias. Preserve even INT64_MIN and INT64_MAX. */
        string_offset_t end = string_find(columns, ",");
        string_t *identity = end > 0 ? string_substr(columns, 0, (size_t)end) : NULL;
        string_t *bounds_sql = identity ? string_sprintf(
            "SELECT max(id),count(*) FROM (SELECT %s AS id FROM previous.%s\n"
            "WHERE %s>=?1 ORDER BY %s LIMIT 4096)", string_c_str(identity), string_c_str(table),
            string_c_str(identity), string_c_str(identity)) : NULL;
        string_t *copy_sql = identity ? string_sprintf(
            "INSERT INTO main.%s(%s) SELECT %s FROM previous.%s\n"
            "WHERE %s>=?1 AND %s<=?2 ORDER BY %s", string_c_str(table), string_c_str(columns),
            string_c_str(columns), string_c_str(table), string_c_str(identity), string_c_str(identity),
            string_c_str(identity)) : NULL;
        sqlite_stmt_t *bounds = bounds_sql ? sqlite_stmt_prepare(db, string_c_str(bounds_sql)) : NULL;
        sqlite_stmt_t *write = copy_sql ? sqlite_stmt_prepare(db, string_c_str(copy_sql)) : NULL;
        ok = bounds && write;
        int64_t first = INT64_MIN;
        while (ok && copied < total) {
            ok = sqlite_stmt_bind_int64(bounds, 1, first) && sqlite_stmt_step(bounds) == SQLITE_STEP_ROW;
            int64_t last = ok ? sqlite_stmt_column_int64(bounds, 0) : 0;
            int64_t count = ok ? sqlite_stmt_column_int64(bounds, 1) : 0;
            ok = ok && count > 0 && count <= total - copied;
            sqlite_stmt_reset(bounds);
            ok = ok && sqlite_stmt_bind_int64(write, 1, first) && sqlite_stmt_bind_int64(write, 2, last) &&
                 sqlite_stmt_step(write) == SQLITE_STEP_DONE;
            sqlite_stmt_reset(write);
            if (ok) {
                copied += count;
                cfg_progress_update_items(progress, label, copied, total, "rows copied");
            }
            if (last == INT64_MAX) break;
            first = last + 1;
        }
        sqlite_stmt_finalize(write); sqlite_stmt_finalize(bounds);
        string_free(copy_sql); string_free(bounds_sql); string_free(identity);
    } else if (ok) {
        sql = string_sprintf("INSERT INTO main.%s(%s) SELECT %s FROM previous.%s", string_c_str(table),
                             string_c_str(columns), string_c_str(columns), string_c_str(table));
        ok = sql && sqlite_exec(db, sql);
        if (ok) copied = total;
        string_free(sql);
    }
    int64_t actual = 0;
    sql = table ? string_sprintf("SELECT count(*) FROM main.%s", string_c_str(table)) : NULL;
    ok = ok && copied == total && sql && scalar(db, string_c_str(sql), &actual) && actual == total;
    if (ok) cfg_progress_update_items(progress, label, copied, total, "rows copied");
    string_free(sql); string_free(columns); string_free(table);
    return ok;
}

/* Copy installed non-image tables using an already attached, writer-protected source snapshot. */
bool cfg_database_copy_globe(sqlite_t *db)
{
    cfg_progress_t *progress = cfg_progress_start("Preserving installed globe layers");
    cfg_progress_explain(progress,
        "Preserving coastlines, rivers, boundaries and other installed tables before adding the new photographs.");
    sqlite_stmt_t *tables = sqlite_stmt_prepare(db, retained_tables);
    bool ok = tables != NULL;
    sqlite_step_result_t step = SQLITE_STEP_DONE;
    while (ok && (step = sqlite_stmt_step(tables)) == SQLITE_STEP_ROW) {
        const char *name = sqlite_stmt_column_text(tables, 0), *schema = sqlite_stmt_column_text(tables, 1);
        const char *type = sqlite_stmt_column_text(tables, 2);
        bool rowid = !sqlite_stmt_column_int(tables, 3);
        bool batch = rowid && type && !strcmp(type, "table");
        ok = name && schema && sqlite_exec_cstr(db, schema) && copy_rows(db, name, rowid, batch, progress);
    }
    sqlite_stmt_finalize(tables);
    ok = ok && step == SQLITE_STEP_DONE;
    cfg_progress_finish(progress, ok);
    return ok;
}

/* Recreate auxiliary schema only after copying values, so stored triggers cannot replay during the copy. */
bool cfg_database_copy_globe_schema(sqlite_t *db)
{
    sqlite_stmt_t *q = sqlite_stmt_prepare(db,
        "SELECT s.sql FROM previous.sqlite_schema AS s\n"
        "WHERE s.type IN ('index','view','trigger') AND s.sql IS NOT NULL\n"
        "  AND NOT EXISTS (SELECT 1 FROM main.sqlite_schema AS n WHERE n.name=s.name)\n"
        "  AND tbl_name NOT IN ('globe_texture','globe_texture_dataset','globe_texture_manifest',\n"
        "                       'globe_texture_region','globe_texture_region_index')\n"
        "ORDER BY CASE type WHEN 'index' THEN 0 WHEN 'view' THEN 1 ELSE 2 END,name");
    bool ok = q != NULL;
    sqlite_step_result_t step = SQLITE_STEP_DONE;
    while (ok && (step = sqlite_stmt_step(q)) == SQLITE_STEP_ROW) {
        const char *sql = sqlite_stmt_column_text(q, 0);
        ok = sql && sqlite_exec_cstr(db, sql);
    }
    sqlite_stmt_finalize(q);
    int64_t sequences = 0;
    ok = ok && step == SQLITE_STEP_DONE && scalar(db,
        "SELECT count(*) FROM previous.sqlite_schema WHERE name='sqlite_sequence'", &sequences);
    if (ok && sequences) {
        int64_t present = 0;
        ok = scalar(db, "SELECT count(*) FROM main.sqlite_schema WHERE name='sqlite_sequence'", &present);
        if (ok && present) ok = sqlite_exec_cstr(db,
            "DELETE FROM main.sqlite_sequence;"
            "INSERT INTO main.sqlite_sequence SELECT * FROM previous.sqlite_sequence;");
    }
    int64_t application = 0, version = 0;
    ok = ok && scalar(db, "PRAGMA previous.application_id", &application) &&
         scalar(db, "PRAGMA previous.user_version", &version);
    string_t *pragmas = ok ? string_sprintf("PRAGMA main.application_id=%lld; PRAGMA main.user_version=%lld;",
                                           (long long)application, (long long)version) : NULL;
    ok = ok && pragmas && sqlite_exec(db, pragmas);
    string_free(pragmas);
    return ok;
}
