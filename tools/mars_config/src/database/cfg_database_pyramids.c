/**
 * @file cfg_database_pyramids.c
 * @brief Resumable installation of prepared photographic blocks alongside the original catalogue.
 *
 * Resolves preparation files through the configured texture resource directory. Each source imports through
 * the globe module's independently committed savepoint; a later failure preserves completed sources and
 * leaves unconverted photographs usable. Original image payloads are never copied by this stage.
 */
#include <string.h>

#include "array-t.h"
#include "file-t.h"
#include "globe-t.h"
#include "mars_paths.h"
#include "cfg_progress.h"
#define MARS_CFG_DATABASE_INTERNAL_ACCESS
#include "cfg_database_internal.h"

typedef struct { unsigned level, column, row; } source_address_t;

/* Import only single-source shards matching an installed photograph's immutable content address. */
bool cfg_database_pyramids(sqlite_t *db, const string_t *root)
{
    if (!db || !root || !sqlite_exec_cstr(db, "PRAGMA busy_timeout=30000")) return false;
    string_t *directory = mars_path_resource(MARS_PATH_TEXTURES, root, "pyramid");
    file_t *folder = directory ? file_new(directory) : NULL;
    if (!folder) { string_free(directory); return false; }
    if (!file_exists(folder)) { file_free(folder); string_free(directory); return true; }
    array_t *addresses = array_create(sizeof(source_address_t), NULL, NULL);
    sqlite_stmt_t *sources = addresses ? sqlite_stmt_prepare(db,
        "SELECT level, column_index, row_index\n"
        "FROM globe_texture WHERE level >= 2 ORDER BY level, row_index, column_index") : NULL;
    sqlite_step_result_t step = SQLITE_STEP_ERROR;
    bool ok = sources != NULL;
    while (ok && (step = sqlite_stmt_step(sources)) == SQLITE_STEP_ROW) {
        source_address_t address = {sqlite_stmt_column_int(sources, 0), sqlite_stmt_column_int(sources, 1),
                                    sqlite_stmt_column_int(sources, 2)};
        ok = array_size(addresses) < GLOBE_TEXTURE_REGION_LIMIT + 1226 &&
            array_append_carray(addresses, &address, 1);
    }
    ok = ok && step == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(sources);
    /* Do not keep a read cursor open while upgrading to a source's independent writer transaction. */
    globe_t *globe = ok ? globe_open_databases(db, db, NULL) : NULL;
    uint64_t total = array_size(addresses), imported = 0;
    cfg_progress_t *progress = globe ? cfg_progress_start("Installing photographic source blocks") : NULL;
    ok = ok && globe;
    for (size_t i = 0; ok && i < total; ++i) {
        const source_address_t *address = array_get(addresses, i);
        globe_texture_info_t info;
        ok = globe_texture_metadata(globe, address->level, address->column, address->row, &info);
        string_t *path = ok && !info.pyramid ? string_sprintf("%s/%s.db", string_c_str(directory), info.sha256) : NULL;
        file_t *source = path ? file_new(path) : NULL;
        if (source && file_exists(source)) {
            sqlite_t *input = sqlite_open_plain(path);
            sqlite_stmt_t *check = input ? sqlite_stmt_prepare(input,
                "SELECT count(*), min(sha256), min(width) FROM globe_texture_block_source") : NULL;
            const char *digest = check && sqlite_stmt_step(check) == SQLITE_STEP_ROW ?
                sqlite_stmt_column_text(check, 1) : NULL;
            ok = digest && sqlite_stmt_column_int(check, 0) == 1 && !strcmp(digest, info.sha256) &&
                sqlite_stmt_column_int(check, 2) == (int)info.width;
            sqlite_stmt_finalize(check); sqlite_close(input);
            bool transaction = ok && sqlite_exec_cstr(db, "BEGIN IMMEDIATE");
            ok = transaction && globe_texture_pyramid_import(db, source);
            if (ok) ok = sqlite_exec_cstr(db, "COMMIT");
            if (!ok && transaction) sqlite_exec_cstr(db, "ROLLBACK");
            if (ok) ++imported;
        }
        file_free(source); string_free(path);
        cfg_progress_update_items(progress, "Installing photographic source blocks", i + 1, total,
                                  "sources checked");
    }
    if (ok) {
        string_t *summary = string_sprintf("%llu prepared sources added; existing blocks and originals retained.",
                                         (unsigned long long)imported);
        if (summary) cfg_progress_explain(progress, string_c_str(summary));
        string_free(summary);
    }
    cfg_progress_finish(progress, ok);
    array_destroy(addresses); globe_close(globe); file_free(folder); string_free(directory);
    return ok;
}
