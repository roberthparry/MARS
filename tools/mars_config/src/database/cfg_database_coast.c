/**
 * @file cfg_database_coast.c
 * @brief Restore and import the packaged shoreline data during native almanac installation.
 *
 * Uses a bounded temporary file beneath the installer's private staging directory. No network service or
 * interpreter is needed. Older source packages may omit coastlines; geographical queries then report unknown.
 */
#include <stdio.h>

#include "globe-t.h"
#include "cfg_progress.h"
#define MARS_CFG_DATABASE_INTERNAL_ACCESS
#include "cfg_database_internal.h"

/* Keep the optional geographic import separate from SQL statement ingestion. */
bool cfg_database_coast(sqlite_t *db, const string_t *root, const string_t *staging)
{
    string_t *packed_path = string_sprintf("%s/packaging/almanac-db/gshhs_f.b.zst", string_c_str(root));
    string_t *plain_path = string_sprintf("%s/shoreline.bin", string_c_str(staging));
    file_t *packed = packed_path ? file_new(packed_path) : NULL;
    file_t *plain = plain_path ? file_new(plain_path) : NULL;
    bool exists = packed && file_exists(packed);
    bool ok = packed && plain && !file_last_error(packed);
    if (ok && exists) {
        cfg_progress_t *progress = cfg_progress_start("Indexing shorelines");
        ok = file_decompress_zstd(packed, plain, 128 * 1024 * 1024, false) &&
             file_open_read(plain) && globe_import_shoreline(db, plain);
        if (!ok)
            fprintf(stderr, "Shoreline import failed (source error %d, staging error %d): %s\n",
                    file_last_error(packed), file_last_error(plain), string_c_str(sqlite_last_error(db)));
        file_close(plain);
        if (file_exists(plain) && !file_delete(plain)) ok = false;
        cfg_progress_finish(progress, ok);
    }
    file_free(packed);
    file_free(plain);
    string_free(packed_path);
    string_free(plain_path);
    return ok;
}
