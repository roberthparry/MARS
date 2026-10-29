/**
 * @file cfg_database_globe_rebuild.c
 * @brief Build and publish a globe replacement without rewriting the installed photographs.
 *
 * Keeps the installed plaintext globe writer-locked and unchanged while a fresh database is built beside it.
 * The replacement uses 64 KiB pages, bounded caches, ordinary rollback journalling and full synchronisation.
 * Non-image data and auxiliary schema are preserved, photographs are verified by their importer, and only a
 * completed, closed and synchronised replacement is renamed into place. No configuration or credentials change.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>

#include "cfg_progress.h"
#define MARS_CFG_DATABASE_INTERNAL_ACCESS
#include "cfg_database_internal.h"

static bool same_file(const struct stat *before, const struct stat *after)
{
    return S_ISREG(after->st_mode) && before->st_dev == after->st_dev && before->st_ino == after->st_ino &&
           before->st_size == after->st_size && before->st_mtim.tv_sec == after->st_mtim.tv_sec &&
           before->st_mtim.tv_nsec == after->st_mtim.tv_nsec;
}

static bool valid_replacement(sqlite_t *db)
{
    sqlite_stmt_t *q = sqlite_stmt_prepare(db, "PRAGMA main.foreign_key_check");
    bool ok = q && sqlite_stmt_step(q) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(q);
    q = ok ? sqlite_stmt_prepare(db,
        "SELECT 1 FROM globe_texture_dataset WHERE id=1 AND format_version=2\n"
        "  AND (SELECT count(*) FROM globe_texture)>=74\n"
        "  AND NOT EXISTS (SELECT 1 FROM globe_texture WHERE byte_count<>length(png))") : NULL;
    ok = q && sqlite_stmt_step(q) == SQLITE_STEP_ROW;
    sqlite_stmt_finalize(q);
    return ok;
}

static bool attach_previous(sqlite_t *db, const string_t *path)
{
    sqlite_stmt_t *q = sqlite_stmt_prepare(db, "ATTACH DATABASE ?1 AS previous KEY ''");
    bool ok = q && sqlite_stmt_bind_text(q, 1, string_c_str(path)) && sqlite_stmt_step(q) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(q);
    return ok && sqlite_exec_cstr(db, "PRAGMA previous.cache_size=-8192");
}

/* Replace the connection as well as the file: callers must never keep using the old inode after publication. */
bool cfg_database_rebuild_globe(sqlite_t **source, const string_t *path, const string_t *root, const string_t *staging,
                               bool (*apply)(sqlite_t *, const string_t *, const string_t *), int revision, bool force)
{
    if (!source) return false;
    sqlite_close(*source);
    *source = NULL;
    if (!path || !root || !staging || !apply || revision < 1) return false;
    file_t *destination = file_new(path);
    struct stat before = {0}, current = {0};
    bool ok = destination && lstat(file_path(destination), &before) == 0 && S_ISREG(before.st_mode);
    sqlite_t *guard = ok ? sqlite_open_plain(path) : NULL;
    bool locked = guard && sqlite_exec_cstr(guard, "BEGIN IMMEDIATE");
    int64_t installed = 0;
    ok = locked && lstat(file_path(destination), &current) == 0 && same_file(&before, &current) &&
         cfg_database_no_sidecars(destination) && cfg_database_current_revision(guard, "globe", &installed) &&
         installed <= revision;
    if (ok && installed == revision && !force) {
        ok = sqlite_exec_cstr(guard, "ROLLBACK");
        sqlite_close(guard); file_free(destination);
        return ok;
    }
    string_t *temporary_path = ok ? string_sprintf("%s/database.db", string_c_str(staging)) : NULL;
    file_t *temporary = temporary_path ? file_new(temporary_path) : NULL;
    ok = ok && temporary && file_open(temporary, FILE_MODE_CREATE_NEW, FILE_ACCESS_WRITE) &&
         file_close(temporary) && file_chmod(temporary, 0600);
    sqlite_t *db = ok ? sqlite_open_plain(temporary_path) : NULL;
    ok = ok && db && sqlite_exec_cstr(db,
        "PRAGMA page_size=65536; PRAGMA cache_size=-65536; PRAGMA temp.cache_size=-512;\n"
        "PRAGMA journal_mode=DELETE; PRAGMA synchronous=FULL; PRAGMA foreign_keys=OFF;");
    bool attached = ok && attach_previous(db, path);
    ok = ok && attached;
    bool transaction = ok && sqlite_exec_cstr(db, "BEGIN");
    ok = transaction && cfg_database_copy_globe(db);
    if (transaction) ok = cfg_database_finish_transaction(db, ok);
    if (attached && sqlite_exec_cstr(db, "DETACH DATABASE previous")) attached = false;
    else if (attached) ok = false;
    /* Unqualified migration DDL must never resolve a missing main table against the installed source. */
    transaction = ok && !attached && sqlite_exec_cstr(db, "BEGIN");
    ok = transaction && apply(db, root, staging);
    if (ok) attached = attach_previous(db, path);
    ok = ok && attached && cfg_database_copy_globe_schema(db) && cfg_database_revision(db, "globe", revision);
    cfg_progress_t *progress = ok ? cfg_progress_start("Validating replacement globe") : NULL;
    if (ok) ok = valid_replacement(db);
    cfg_progress_finish(progress, ok);
    if (transaction) ok = cfg_database_finish_transaction(db, ok);
    if (attached && !sqlite_exec_cstr(db, "DETACH DATABASE previous")) ok = false;
    sqlite_close(db);
    ok = ok && lstat(file_path(destination), &current) == 0 && same_file(&before, &current) &&
         cfg_database_no_sidecars(destination) && file_open_read(temporary) && file_sync(temporary, false);
    if (temporary && file_is_open(temporary)) ok = file_close(temporary) && ok;
    /* This rename publishes one complete file while the old inode is still protected by the writer guard. */
    if (ok) ok = file_replace(temporary, destination, NULL);
    if (locked && !sqlite_exec_cstr(guard, "ROLLBACK")) {
        if (ok) fputs("Globe published; closing the previous writer guard failed.\n", stderr);
        else fputs("Cannot release the previous globe writer guard.\n", stderr);
    }
    sqlite_close(guard);
    file_free(temporary); string_free(temporary_path); file_free(destination);
    return ok;
}
