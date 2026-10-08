/**
 * @file file_sqlite.c
 * @brief SQLCipher file-content import and export.
 *
 * Stores file contents and selected metadata using manifests and chunks, then reconstructs files through verified
 * output handling. Persistent records represent data, never live file handles or descriptors.
 *
 * This belongs to the Linux-only file.h implementation. Filesystem operations and transforms must retain the
 * public error, ownership and output-publication contracts.
 */

#define MARS_FILE_INTERNAL_ACCESS
#include <errno.h>
#include <fcntl.h>
#include <sodium.h>
#include <string.h>
#include <unistd.h>

#include "file_internal.h"
#include "sqlite.h"

#define FILE_MANIFEST_SIZE 64u
static const unsigned char manifest_magic[8] = {'M', 'A', 'R', 'S', 'F', 'I', 'L', 1};

static bool store_arguments(file_t *file, sqlite_t *db, const string_t *name)
{
    if (!file_require_closed(file))
        return false;
    if (!db || !name || !string_byte_length(name) ||
        strlen(string_c_str(name)) != string_byte_length(name))
        return file_fail(file, EINVAL);
    const char *database = sqlite_database_path(db);
    struct stat a, b;
    if (database && *database && stat(database, &a) == 0 && lstat(file->path, &b) == 0 &&
        a.st_dev == b.st_dev && a.st_ino == b.st_ino)
        return file_fail(file, EINVAL);
    return true;
}

static void rollback(sqlite_t *db)
{
    sqlite_exec_cstr(db, "rollback to mars_file_transfer");
    sqlite_exec_cstr(db, "release mars_file_transfer");
}

static bool same_snapshot(const struct stat *a, const struct stat *b)
{
    return a->st_dev == b->st_dev && a->st_ino == b->st_ino && a->st_size == b->st_size &&
        a->st_mtim.tv_sec == b->st_mtim.tv_sec && a->st_mtim.tv_nsec == b->st_mtim.tv_nsec &&
        a->st_ctim.tv_sec == b->st_ctim.tv_sec && a->st_ctim.tv_nsec == b->st_ctim.tv_nsec;
}

static bool store_chunks(file_t *source, sqlite_t *db, const string_t *name, uint64_t size,
                         unsigned char manifest[FILE_MANIFEST_SIZE])
{
    sqlite_stmt_t *stmt = sqlite_stmt_prepare(db,
        "insert into mars_file_chunk(name, ordinal, value) values(?1, ?2, ?3)");
    unsigned char buffer[FILE_STREAM_BLOCK];
    crypto_hash_sha256_state hash;
    crypto_hash_sha256_init(&hash);
    uint64_t total = 0;
    int64_t ordinal = 0;
    bool ok = stmt && sqlite_stmt_bind_text(stmt, 1, string_c_str(name));
    while (ok) {
        size_t count;
        if (!file_read(source, buffer, sizeof(buffer), &count)) {
            ok = false;
            break;
        }
        if (!count)
            break;
        if (count > size - total) {
            ok = file_fail(source, ESTALE);
            break;
        }
        ok = sqlite_stmt_bind_int64(stmt, 2, ordinal++) && sqlite_stmt_bind_blob(stmt, 3, buffer, count) &&
            sqlite_stmt_step(stmt) == SQLITE_STEP_DONE;
        if (ok) {
            total += count;
            crypto_hash_sha256_update(&hash, buffer, count);
            sqlite_stmt_reset(stmt);
        }
    }
    if (ok && total != size)
        ok = file_fail(source, ESTALE);
    if (ok)
        crypto_hash_sha256_final(&hash, manifest + 32);
    sodium_memzero(&hash, sizeof(hash));
    sodium_memzero(buffer, sizeof(buffer));
    sqlite_stmt_finalize(stmt);
    return ok;
}

/* Import contents and selected metadata atomically into the existing object store. */
bool file_import_sqlite(file_t *source, sqlite_t *db, const string_t *name, uint64_t max_bytes)
{
    if (!store_arguments(source, db, name) || !file_open_read(source))
        return false;
    struct stat before, after;
    bool ok = fstat(fileno(source->stream), &before) == 0;
    if (!ok)
        file_fail(source, errno);
    if (ok && (before.st_size < 0 || (uint64_t)before.st_size > max_bytes))
        ok = file_fail(source, EFBIG);
    bool transaction = ok && sqlite_exec_cstr(db, "savepoint mars_file_transfer");
    ok = ok && transaction && sqlite_init_object_store(db);
    unsigned char manifest[FILE_MANIFEST_SIZE] = {0};
    string_t *type = NULL, *encoding = NULL;
    if (ok) {
        memcpy(manifest, manifest_magic, sizeof(manifest_magic));
        file_encode_u64(manifest + 8, (uint64_t)before.st_size, 8);
        file_encode_u64(manifest + 16, before.st_mode & 0777, 4);
        file_encode_u64(manifest + 20, (uint64_t)before.st_mtim.tv_sec, 8);
        file_encode_u64(manifest + 28, before.st_mtim.tv_nsec, 4);
        type = string_new_with("file");
        encoding = string_new_with("mars/file-chunks-v1");
        if (!type || !encoding)
            ok = file_fail(source, ENOMEM);
        else
            ok = sqlite_store_object(db, name, type, encoding, manifest, sizeof(manifest)) &&
                store_chunks(source, db, name, (uint64_t)before.st_size, manifest);
    }
    if (ok && fstat(fileno(source->stream), &after) != 0)
        ok = file_fail(source, errno);
    if (ok && !same_snapshot(&before, &after))
        ok = file_fail(source, ESTALE);
    if (!ok && !file_last_error(source))
        file_fail(source, EIO);
    ok = file_finish(source, ok);
    sqlite_stmt_t *stmt = NULL;
    if (ok) {
        stmt = sqlite_stmt_prepare(db, "update mars_object set value = ?1 where name = ?2");
        ok = stmt && sqlite_stmt_bind_blob(stmt, 1, manifest, sizeof(manifest)) &&
            sqlite_stmt_bind_text(stmt, 2, string_c_str(name)) && sqlite_stmt_step(stmt) == SQLITE_STEP_DONE;
    }
    sqlite_stmt_finalize(stmt);
    string_free(type);
    string_free(encoding);
    if (ok)
        ok = sqlite_exec_cstr(db, "release mars_file_transfer");
    int error = file_last_error(source);
    if (!ok && transaction)
        rollback(db);
    return ok ? file_succeed(source) : file_fail(source, error ? error : EIO);
}

static bool load_manifest(sqlite_t *db, const string_t *name, unsigned char manifest[FILE_MANIFEST_SIZE])
{
    sqlite_stmt_t *stmt = sqlite_stmt_prepare(db,
        "select type, encoding, length(value), case when length(value)=64 then value end "
        "from mars_object where name=?1");
    bool ok = stmt && sqlite_stmt_bind_text(stmt, 1, string_c_str(name)) &&
        sqlite_stmt_step(stmt) == SQLITE_STEP_ROW;
    if (ok) {
        const char *type = sqlite_stmt_column_text(stmt, 0);
        const char *encoding = sqlite_stmt_column_text(stmt, 1);
        const void *value = sqlite_stmt_column_blob(stmt, 3);
        ok = type && encoding && !strcmp(type, "file") && !strcmp(encoding, "mars/file-chunks-v1") &&
            sqlite_stmt_column_int64(stmt, 2) == FILE_MANIFEST_SIZE && value &&
            sqlite_stmt_column_bytes(stmt, 3) == FILE_MANIFEST_SIZE;
        if (ok)
            memcpy(manifest, value, FILE_MANIFEST_SIZE);
    }
    sqlite_stmt_finalize(stmt);
    return ok && !memcmp(manifest, manifest_magic, sizeof(manifest_magic)) &&
        file_decode_u64(manifest + 16, 4) <= 0777 && file_decode_u64(manifest + 28, 4) < 1000000000;
}

static bool load_chunks(sqlite_t *db, const string_t *name, file_t *output,
                        const unsigned char manifest[FILE_MANIFEST_SIZE])
{
    sqlite_stmt_t *stmt = sqlite_stmt_prepare(db,
        "select ordinal, length(value), case when length(value) between 1 and 65536 then value end "
        "from mars_file_chunk where name=?1 order by ordinal");
    bool ok = stmt && sqlite_stmt_bind_text(stmt, 1, string_c_str(name));
    uint64_t total = 0, size = file_decode_u64(manifest + 8, 8);
    int64_t ordinal = 0;
    crypto_hash_sha256_state hash;
    unsigned char digest[crypto_hash_sha256_BYTES];
    crypto_hash_sha256_init(&hash);
    while (ok) {
        sqlite_step_result_t result = sqlite_stmt_step(stmt);
        if (result == SQLITE_STEP_DONE)
            break;
        if (result != SQLITE_STEP_ROW) {
            ok = false;
            break;
        }
        int64_t length = sqlite_stmt_column_int64(stmt, 1);
        const void *data = sqlite_stmt_column_blob(stmt, 2);
        size_t expected = size - total > FILE_STREAM_BLOCK ? FILE_STREAM_BLOCK : (size_t)(size - total);
        if (sqlite_stmt_column_int64(stmt, 0) != ordinal++ || length <= 0 || (uint64_t)length != expected ||
            !data || sqlite_stmt_column_bytes(stmt, 2) != expected) {
            ok = false;
            break;
        }
        size_t written;
        ok = file_write(output, data, expected, &written);
        if (ok) {
            total += expected;
            crypto_hash_sha256_update(&hash, data, expected);
        }
    }
    crypto_hash_sha256_final(&hash, digest);
    ok = ok && total == size && sodium_memcmp(digest, manifest + 32, sizeof(digest)) == 0;
    sodium_memzero(&hash, sizeof(hash));
    sodium_memzero(digest, sizeof(digest));
    sqlite_stmt_finalize(stmt);
    return ok;
}

static bool restore_file_metadata(file_t *output, const unsigned char manifest[FILE_MANIFEST_SIZE])
{
    uint64_t raw_seconds = file_decode_u64(manifest + 20, 8);
    int64_t seconds = raw_seconds <= INT64_MAX ? (int64_t)raw_seconds : -1 - (int64_t)(UINT64_MAX - raw_seconds);
    time_t stamp = (time_t)seconds;
    if ((int64_t)stamp != seconds)
        return file_fail(output, EOVERFLOW);
    struct timespec times[2] = {{.tv_nsec = UTIME_OMIT},
        {.tv_sec = stamp, .tv_nsec = (long)file_decode_u64(manifest + 28, 4)}};
    if (!file_flush(output))
        return false;
    if (fchmod(fileno(output->stream), (mode_t)file_decode_u64(manifest + 16, 4)) != 0 ||
        futimens(fileno(output->stream), times) != 0)
        return file_fail(output, errno);
    return true;
}

/* Export verified contents through a private staged file; optionally restore safe metadata. */
bool file_export_sqlite(sqlite_t *db, const string_t *name, file_t *destination, uint64_t max_bytes,
                        bool overwrite, bool restore_metadata)
{
    if (!store_arguments(destination, db, name))
        return false;
    if (!sqlite_exec_cstr(db, "savepoint mars_file_transfer"))
        return file_fail(destination, EIO);
    unsigned char manifest[FILE_MANIFEST_SIZE] = {0};
    bool ok = load_manifest(db, name, manifest);
    int error = ok ? 0 : EBADMSG;
    if (ok && file_decode_u64(manifest + 8, 8) > max_bytes) {
        ok = false;
        error = EFBIG;
    }
    file_stage_t stage = {0};
    if (ok && !file_stage_begin(&stage, destination, overwrite)) {
        ok = false;
        error = file_last_error(destination);
    }
    if (ok && !load_chunks(db, name, stage.temporary, manifest)) {
        ok = false;
        error = file_last_error(stage.temporary);
        if (!error)
            error = EBADMSG;
    }
    if (ok && restore_metadata && !restore_file_metadata(stage.temporary, manifest)) {
        ok = false;
        error = file_last_error(stage.temporary);
    }
    if (ok && !sqlite_exec_cstr(db, "release mars_file_transfer")) {
        ok = false;
        error = EIO;
    }
    if (!ok)
        rollback(db);
    if (ok && !file_stage_publish(&stage)) {
        ok = false;
        error = file_last_error(destination);
    }
    file_stage_discard(&stage);
    return ok ? file_succeed(destination) : file_fail(destination, error);
}
