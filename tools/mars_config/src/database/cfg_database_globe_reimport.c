/**
 * @file cfg_database_globe_reimport.c
 * @brief Full transactional reimport of packaged globe layers without copying retained image blocks.
 *
 * Recreates every packaged photograph and geographic layer while leaving unrelated tables on their existing
 * pages. A durable SQLite rollback journal protects the complete operation. Auxiliary schema is saved before
 * import and restored afterwards; triggers cannot replay during the import. No settings or user paths change.
 */
#include "array-t.h"
#include "cfg_progress.h"
#define MARS_CFG_DATABASE_INTERNAL_ACCESS
#include "cfg_database_internal.h"

typedef struct _saved_schema_t {
    string_t *name, *create, *drop;
} saved_schema_t;

static void schema_free(void *value)
{
    saved_schema_t *entry = value;
    string_free(entry->name); string_free(entry->create); string_free(entry->drop);
}

static array_t *schema_save(sqlite_t *db)
{
    array_t *saved = array_create(sizeof(saved_schema_t), NULL, schema_free);
    sqlite_stmt_t *q = saved ? sqlite_stmt_prepare(db,
        "SELECT name, sql,\n"
        "       CASE WHEN type IN ('view', 'trigger')\n"
        "            THEN 'DROP ' || upper(type) || ' main.\"' || replace(name, '\"', '\"\"') || '\"' END\n"
        "FROM main.sqlite_schema\n"
        "WHERE type IN ('index', 'view', 'trigger') AND sql IS NOT NULL\n"
        "  AND tbl_name NOT IN ('globe_texture', 'globe_texture_dataset', 'globe_texture_manifest',\n"
        "                       'globe_texture_region', 'globe_texture_region_index')\n"
        "ORDER BY CASE type WHEN 'index' THEN 0 WHEN 'view' THEN 1 ELSE 2 END, name") : NULL;
    bool ok = q != NULL;
    sqlite_step_result_t step = SQLITE_STEP_DONE;
    while (ok && (step = sqlite_stmt_step(q)) == SQLITE_STEP_ROW) {
        const char *name = sqlite_stmt_column_text(q, 0), *sql = sqlite_stmt_column_text(q, 1);
        const char *drop = sqlite_stmt_column_text(q, 2);
        saved_schema_t entry = {.name = name ? string_new_with(name) : NULL,
                                .create = sql ? string_new_with(sql) : NULL,
                                .drop = drop ? string_new_with(drop) : NULL};
        ok = entry.name && entry.create && (!drop || entry.drop) && array_add(saved, &entry);
        if (!ok) schema_free(&entry);
    }
    sqlite_stmt_finalize(q);
    ok = ok && step == SQLITE_STEP_DONE;
    /* Schema objects are bounded control metadata; finish reading them before executing any schema changes. */
    for (size_t i = 0; ok && i < array_size(saved); ++i) {
        const saved_schema_t *entry = array_get(saved, i);
        if (entry->drop) ok = sqlite_exec(db, entry->drop);
    }
    if (!ok) { array_destroy(saved); saved = NULL; }
    return saved;
}

static bool schema_restore(sqlite_t *db, const array_t *saved)
{
    sqlite_stmt_t *q = sqlite_stmt_prepare(db, "SELECT 1 FROM main.sqlite_schema WHERE name = ?1");
    bool ok = q != NULL;
    for (size_t i = 0; ok && i < array_size(saved); ++i) {
        const saved_schema_t *entry = array_get(saved, i);
        ok = sqlite_stmt_bind_text(q, 1, string_c_str(entry->name));
        sqlite_step_result_t step = ok ? sqlite_stmt_step(q) : SQLITE_STEP_ERROR;
        sqlite_stmt_reset(q);
        ok = step == SQLITE_STEP_ROW || (step == SQLITE_STEP_DONE && sqlite_exec(db, entry->create));
    }
    sqlite_stmt_finalize(q);
    return ok;
}

/* Replace all packaged content atomically; retained BLOB tables are never selected, copied or rewritten. */
bool cfg_database_reimport_globe(sqlite_t *db, const string_t *root, const string_t *staging,
                                bool (*apply)(sqlite_t *, const string_t *, const string_t *), int revision)
{
    if (!db || !root || !staging || !apply || revision < 1) return false;
    sqlite_stmt_t *foreign = sqlite_stmt_prepare(db, "PRAGMA foreign_keys");
    int enforcement = foreign && sqlite_stmt_step(foreign) == SQLITE_STEP_ROW ?
        sqlite_stmt_column_int(foreign, 0) : -1;
    sqlite_stmt_finalize(foreign);
    bool configured = enforcement >= 0 && sqlite_exec_cstr(db,
        "PRAGMA synchronous=FULL; PRAGMA cache_size=-65536; PRAGMA temp.cache_size=-512; PRAGMA foreign_keys=OFF;");
    bool transaction = configured && sqlite_exec_cstr(db, "BEGIN IMMEDIATE");
    int64_t installed = 0;
    bool ok = transaction && cfg_database_current_revision(db, "globe", &installed) && installed <= revision;
    cfg_progress_t *progress = ok ? cfg_progress_start("Preparing full globe reimport") : NULL;
    cfg_progress_explain(progress,
        "Keeping river and prepared image-block tables in place. Every packaged photograph and geographic layer "
        "will be imported afresh; SQLite retains rollback data until commit.");
    array_t *saved = ok ? schema_save(db) : NULL;
    static const struct { const char *name, *sql; } tables[] = {
        {"globe_texture_region_index", "DROP TABLE IF EXISTS globe_texture_region_index"},
        {"globe_texture_region", "DROP TABLE IF EXISTS globe_texture_region"},
        {"globe_texture", "DROP TABLE IF EXISTS globe_texture"},
        {"globe_texture_dataset", "DROP TABLE IF EXISTS globe_texture_dataset"},
        {"globe_texture_manifest", "DROP TABLE IF EXISTS globe_texture_manifest"}
    };
    size_t count = sizeof tables / sizeof *tables;
    ok = ok && saved;
    for (size_t i = 0; ok && i < count; ++i) {
        cfg_progress_update_items(progress, "Preparing full globe reimport", i, count, "tables cleared");
        string_t *name = string_new_with(tables[i].name);
        cfg_progress_segment_name(progress, name);
        string_free(name);
        ok = cfg_database_public_ddl(db, tables[i].sql);
        if (ok) cfg_progress_update_items(progress, "Preparing full globe reimport", i + 1, count, "tables cleared");
    }
    cfg_progress_finish(progress, ok);
    ok = ok && apply(db, root, staging) && schema_restore(db, saved) &&
         cfg_database_globe_replacement_valid(db) && cfg_database_revision(db, "globe", revision);
    array_destroy(saved);
    if (transaction) ok = cfg_database_finish_transaction(db, ok);
    /* DROP TABLE must not cascade through custom foreign keys; check every relationship before commit instead. */
    bool restored = enforcement >= 0 && sqlite_exec_cstr(db,
        enforcement ? "PRAGMA foreign_keys=ON" : "PRAGMA foreign_keys=OFF");
    return ok && restored;
}
