/**
 * @file cfg_database_publish.c
 * @brief Private staging and recoverable publication of completed databases.
 *
 * Database and configuration staging remain on their respective destination
 * filesystems. A hard-link backup preserves the old database until configuration
 * publication succeeds. Each rename is atomic; the pair is not a crash-atomic
 * filesystem transaction. A failed rollback retains the backup for recovery.
 * Only known installer-owned staging filenames are removed during cleanup.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cfg_database_internal.h"
#include "cfg_storage.h"

static file_t *cfg_database_stage_file(const string_t *directory, const char *name)
{
    string_t *path = string_sprintf("%s/%s", string_c_str(directory), name);
    file_t *file = path ? file_new(path) : NULL;
    string_free(path);
    return file;
}

/* mkdtemp provides exclusive directory creation; all content I/O uses file.h. */
string_t *cfg_database_stage(const string_t *directory)
{
    string_t *pattern = string_sprintf("%s/.mars-database-XXXXXX", string_c_str(directory));
    char *buffer = pattern ? strdup(string_c_str(pattern)) : NULL;
    bool created = buffer && mkdtemp(buffer);
    string_t *result = created ? string_new_with(buffer) : NULL;
    if (created && !result) {
        file_t *folder = file_new_cstr(buffer);
        if (folder)
            file_remove_directory(folder);
        file_free(folder);
    }
    free(buffer);
    string_free(pattern);
    return result;
}

/* Do not delete backup.db: its presence means rollback needs manual recovery. */
void cfg_database_cleanup(const string_t *staging)
{
    if (!staging)
        return;
    static const char *const names[] = {"database.db", "database.db-journal", "database.db-wal", "database.db-shm",
                                        "configuration.env"};
    for (size_t i = 0; i < sizeof(names) / sizeof(*names); ++i) {
        file_t *file = cfg_database_stage_file(staging, names[i]);
        if (file)
            file_delete(file);
        file_free(file);
    }
    file_t *folder = file_new(staging);
    if (folder)
        file_remove_directory(folder);
    file_free(folder);
}

static bool cfg_database_no_sidecars(file_t *destination)
{
    static const char *const suffixes[] = {"-journal", "-wal", "-shm"};
    bool ok = true;
    for (size_t i = 0; i < sizeof(suffixes) / sizeof(*suffixes) && ok; ++i) {
        string_t *path = string_sprintf("%s%s", file_path(destination), suffixes[i]);
        file_t *sidecar = path ? file_new(path) : NULL;
        bool exists = false;
        ok = sidecar && cfg_storage_safe_regular(sidecar, &exists) && !exists;
        file_free(sidecar);
        string_free(path);
    }
    return ok;
}

static string_t *cfg_database_canonical_destination(file_t *file)
{
    string_t *path = string_new_with(file_path(file));
    string_t *parent = path ? cfg_storage_parent(path) : NULL;
    file_t *folder = parent ? file_new(parent) : NULL;
    file_t *resolved = folder ? file_resolve(folder) : NULL;
    string_cursor_t *cursor = path ? string_cursor_new(path) : NULL;
    string_pos_t start = 0;
    while (cursor && !string_cursor_done(cursor)) {
        if (string_cursor_consume(cursor, "/"))
            start = string_cursor_position(cursor);
        else
            string_cursor_next(cursor);
    }
    string_t *leaf = cursor ? string_cursor_slice_between(start, string_cursor_position(cursor), cursor) : NULL;
    string_t *result = resolved && leaf ? string_sprintf("%s/%s", file_path(resolved), string_c_str(leaf)) : NULL;
    string_free(leaf);
    string_cursor_free(cursor);
    file_free(resolved);
    file_free(folder);
    string_free(parent);
    string_free(path);
    return result;
}

/* Complete both outputs before replacing either destination; recover on rename failure. */
bool cfg_database_publish(file_t *database, file_t *destination, file_t *configuration,
                          const string_t *configuration_directory, const string_t *staging, const string_t *body)
{
    string_t *config_stage = cfg_database_stage(configuration_directory);
    file_t *config_file = config_stage ? cfg_database_stage_file(config_stage, "configuration.env") : NULL;
    file_t *backup = cfg_database_stage_file(staging, "backup.db");
    string_t *database_path = cfg_database_canonical_destination(destination);
    string_t *configuration_path = cfg_database_canonical_destination(configuration);
    bool existed = false, configuration_existed = false;
    bool ok = config_file && backup && database_path && configuration_path &&
              string_compare(database_path, configuration_path) && cfg_storage_safe_regular(destination, &existed) &&
              cfg_storage_safe_regular(configuration, &configuration_existed) &&
              cfg_database_no_sidecars(destination) && cfg_storage_publish(config_file, config_stage, body) &&
              file_chmod(database, 0600) && file_open_read(database) && file_sync(database, false);
    if (database && file_is_open(database))
        ok = file_close(database) && ok;
    bool backed_up = ok && existed && file_create_hard_link(destination, backup);
    ok = ok && (!existed || backed_up);
    bool replaced = ok && file_move(database, destination, true);
    ok = replaced && file_move(config_file, configuration, true);
    bool recovered = true;
    if (replaced && !ok)
        recovered = existed ? file_move(backup, destination, true) : file_delete(destination);
    if (backed_up && recovered)
        file_delete(backup);
    if (!recovered)
        fputs("Database rollback failed; the previous database is retained in its private staging backup.\n", stderr);
    cfg_database_cleanup(config_stage);
    file_free(backup);
    file_free(config_file);
    string_free(database_path);
    string_free(configuration_path);
    string_free(config_stage);
    return ok;
}
