/**
 * @file cfg_database_coast.c
 * @brief Install the complete, validated shoreline artifact without altering its geometry.
 *
 * Reassembles bounded archive parts, verifies the accepted artifact's SHA-256 hashes,
 * and imports its lossless coordinates inside a savepoint. Offline preparation owns
 * all source composition, polar completion and repairs; normal installation performs
 * none of them. Unrelated database layers and an enclosing transaction are preserved.
 */
#include <stdio.h>
#include <string.h>

#include "globe-t.h"
#include "cfg_progress.h"
#define MARS_CFG_DATABASE_INTERNAL_ACCESS
#include "cfg_database_internal.h"

static bool cfg_coast_hash(file_t *file, const char *expected)
{
    string_t *hash = file_sha256(file);
    bool ok = hash && !strcmp(string_c_str(hash), expected);
    string_free(hash);
    return ok;
}

/* Seven sequential parts form one archive; they are not independent geographic sources. */
static bool cfg_coast_archive(const string_t *root, file_t *archive)
{
    bool ok = file_open(archive, FILE_MODE_CREATE_NEW, FILE_ACCESS_WRITE);
    for (unsigned i = 0; ok && i < 7; ++i) {
        string_t *path = string_sprintf("%s/packaging/globe-db/shoreline-20261023.cf64.zst.%02u", string_c_str(root), i);
        file_t *part = path ? file_new(path) : NULL;
        file_info_t *info = part ? file_get_info(part) : NULL;
        size_t expected = i < 6 ? 67108864 : 14916563;
        ok = info && file_info_size(info) == expected && file_open_read(part);
        file_info_free(info);
        unsigned char buffer[65536];
        while (ok && expected) {
            size_t read = 0, written = 0, wanted = expected < sizeof buffer ? expected : sizeof buffer;
            ok = file_read(part, buffer, wanted, &read) && read == wanted &&
                file_write(archive, buffer, read, &written) && written == read;
            if (ok) expected -= read;
        }
        if (part && file_is_open(part)) ok = file_close(part) && ok;
        file_free(part); string_free(path);
    }
    if (file_is_open(archive)) ok = file_close(archive) && ok;
    return ok && cfg_coast_hash(archive, "6296a43c066718fa65f2bfdea8a55a7660c9b69dfd3e906b45734a5c8c9a3297");
}

/* Import only the accepted complete artifact; never fall back to old data or repair at installation time. */
bool cfg_database_coast(sqlite_t *db, const string_t *root, const string_t *staging)
{
    string_t *packed_path = string_sprintf("%s/shoreline.zst", string_c_str(staging));
    string_t *plain_path = string_sprintf("%s/shoreline.bin", string_c_str(staging));
    file_t *packed = packed_path ? file_new(packed_path) : NULL;
    file_t *plain = plain_path ? file_new(plain_path) : NULL;
    cfg_progress_t *progress = cfg_progress_start("Indexing validated shorelines");
    bool ok = packed && plain && cfg_coast_archive(root, packed) &&
        file_decompress_zstd(packed, plain, 602563348, false) &&
        cfg_coast_hash(plain, "89b67b8d82bec49e6f28a3b55dc944e29d9c1a48e0beacb64550280a6c4f0d84") &&
        file_open_read(plain);
    bool transaction = ok && sqlite_exec_cstr(db, "savepoint globe_coast");
    ok = transaction && globe_import_shoreline(db, plain) && sqlite_exec_cstr(db,
        "update shoreline_dataset set source='OSM 2026-10-12 + GSHHG 2.3.7; complete 8m shoreline 2026-10-23 r2' "
        "where id=1") && sqlite_exec_cstr(db, "release globe_coast");
    if (transaction && !ok) sqlite_exec_cstr(db, "rollback to globe_coast; release globe_coast");
    if (!ok) fputs("Validated shoreline import failed; check archive parts, hashes and staging space.\n", stderr);
    if (plain && file_is_open(plain)) file_close(plain);
    if (plain && file_exists(plain) && !file_delete(plain)) ok = false;
    if (packed && file_exists(packed) && !file_delete(packed)) ok = false;
    cfg_progress_finish(progress, ok);
    file_free(packed); file_free(plain); string_free(packed_path); string_free(plain_path);
    return ok;
}
