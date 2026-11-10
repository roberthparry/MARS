/**
 * @file cfg_database_pyramids.c
 * @brief Resumable installation of prepared photographic blocks alongside the original catalogue.
 *
 * Resolves preparation files through the configured texture resource directory. Each source imports through
 * the globe module's independently committed savepoint; a later failure preserves completed sources and
 * leaves unconverted photographs usable. Checking completion markers is read-only; an installation stage
 * starts only when matching, not-yet-installed preparation files exist. Original image payloads are never copied.
 */
#include <stdio.h>
#include <string.h>

#include "array-t.h"
#include "file-t.h"
#include "globe-t.h"
#include "set-t.h"
#include "mars_paths.h"
#include "cfg_progress.h"
#define MARS_CFG_DATABASE_INTERNAL_ACCESS
#include "cfg_database_internal.h"

typedef struct _source_address_t { unsigned level, column, row; } source_address_t;

typedef struct {
    globe_texture_info_t info;
    source_address_t address;
} pending_source_t;

static size_t pyramid_hash(const void *value)
{
    const pending_source_t *source = value;
    const globe_texture_info_t *info = &source->info;
    size_t hash = 2166136261u;
    for (unsigned i = 0; i < 64; ++i) hash = (hash ^ (unsigned char)info->sha256[i]) * 16777619u;
    return hash ^ info->width;
}

static int pyramid_compare(const void *left, const void *right)
{
    const pending_source_t *first = left, *second = right;
    const globe_texture_info_t *a = &first->info, *b = &second->info;
    int order = strcmp(a->sha256, b->sha256);
    return order ? order : (a->width > b->width) - (a->width < b->width);
}

static bool pyramid_import(sqlite_t *db, const string_t *directory, const globe_texture_info_t *info)
{
    string_t *path = string_sprintf("%s/%s.db", string_c_str(directory), info->sha256);
    file_t *source = path ? file_new(path) : NULL;
    sqlite_t *input = source && file_exists(source) ? sqlite_open_plain(path) : NULL;
    sqlite_stmt_t *check = input ? sqlite_stmt_prepare(input,
        "SELECT count(*), min(sha256), min(width)\n"
        "FROM globe_texture_block_source") : NULL;
    const char *digest = check && sqlite_stmt_step(check) == SQLITE_STEP_ROW ? sqlite_stmt_column_text(check, 1) : NULL;
    bool ok = digest && sqlite_stmt_column_int(check, 0) == 1 && !strcmp(digest, info->sha256) &&
              sqlite_stmt_column_int(check, 2) == (int)info->width;
    sqlite_stmt_finalize(check);
    sqlite_close(input);
    bool transaction = ok && sqlite_exec_cstr(db, "BEGIN IMMEDIATE");
    ok = transaction && globe_texture_pyramid_import(db, source);
    if (ok) ok = sqlite_exec_cstr(db, "COMMIT");
    if (!ok && transaction) sqlite_exec_cstr(db, "ROLLBACK");
    file_free(source);
    string_free(path);
    return ok;
}

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
    set_t *pending = globe ? set_create(sizeof(pending_source_t), pyramid_hash, pyramid_compare, NULL, NULL) : NULL;
    json_t *catalogue = pending ? cfg_database_texture_catalogue(db) : NULL;
    uint64_t total = array_size(addresses), retained = 0, unavailable = 0;
    static const char checking[] = "Checking photographic source blocks";
    cfg_progress_t *progress = pending ? cfg_progress_start(checking) : NULL;
    cfg_progress_update_items(progress, checking, 0, total, "sources checked");
    ok = ok && pending;
    for (size_t i = 0; ok && i < total; ++i) {
        const source_address_t *address = array_get(addresses, i);
        string_t *name = cfg_database_texture_name(catalogue, address->level, address->column);
        cfg_progress_segment_name(progress, name);
        string_free(name);
        globe_texture_info_t info;
        ok = globe_texture_metadata(globe, address->level, address->column, address->row, &info);
        if (ok && info.pyramid) {
            ++retained;
        } else if (ok) {
            string_t *path = string_sprintf("%s/%s.db", string_c_str(directory), info.sha256);
            file_t *source = path ? file_new(path) : NULL;
            ok = source != NULL;
            pending_source_t candidate = {.info = info, .address = *address};
            if (ok && file_exists(source)) ok = set_contains(pending, &candidate) || set_add(pending, &candidate);
            else if (ok) ++unavailable;
            file_free(source);
            string_free(path);
        }
        if (ok) cfg_progress_update_items(progress, checking, i + 1, total, "sources checked");
    }
    if (ok) {
        string_t *summary = string_sprintf("%llu already installed; %llu without prepared files; %llu to import.",
                                         (unsigned long long)retained, (unsigned long long)unavailable,
                                         (unsigned long long)set_get_size(pending));
        if (summary) cfg_progress_explain(progress, string_c_str(summary));
        string_free(summary);
    }
    cfg_progress_finish(progress, ok);
    total = set_get_size(pending);
    static const char installing[] = "Installing photographic source blocks";
    progress = ok && total ? cfg_progress_start(installing) : NULL;
    cfg_progress_update_items(progress, installing, 0, total, "sources imported");
    for (size_t i = 0; ok && i < total; ++i) {
        const pending_source_t *source = set_get(pending, i);
        string_t *name = cfg_database_texture_name(catalogue, source->address.level, source->address.column);
        cfg_progress_segment_name(progress, name);
        string_free(name);
        ok = pyramid_import(db, directory, &source->info);
        if (ok) cfg_progress_update_items(progress, installing, i + 1, total, "sources imported");
    }
    cfg_progress_finish(progress, ok);
    if (ok && !total) fputs("Photographic source blocks: up to date; nothing to import.\n", stderr);
    set_destroy(pending); array_destroy(addresses);
    json_free(catalogue);
    globe_close(globe); file_free(folder); string_free(directory);
    return ok;
}
