/**
 * @file file_hash.c
 * @brief Bounded-memory SHA-256 content identification for regular Linux files.
 *
 * Streams source bytes through libsodium and returns an owned hexadecimal digest.
 * Open and close semantics match the file module's whole-file operations. Metadata
 * checks detect ordinary concurrent edits without promising a hostile-writer snapshot.
 */
#define MARS_FILE_INTERNAL_ACCESS
#include <errno.h>
#include <sodium.h>
#include <unistd.h>

#include "file_internal.h"

/* Hash a closed regular file without changing its contents or retaining a stream. */
string_t *file_sha256(file_t *source)
{
    if (!file_require_closed(source) || !file_open_read(source))
        return NULL;
    struct stat before, after;
    bool ok = fstat(fileno(source->stream), &before) == 0;
    if (!ok)
        file_fail(source, errno);
    crypto_hash_sha256_state state;
    crypto_hash_sha256_init(&state);
    unsigned char buffer[FILE_STREAM_BLOCK], digest[crypto_hash_sha256_BYTES];
    while (ok) {
        size_t count = 0;
        ok = file_read(source, buffer, sizeof(buffer), &count);
        if (!ok || !count)
            break;
        crypto_hash_sha256_update(&state, buffer, count);
    }
    if (ok && fstat(fileno(source->stream), &after) != 0)
        ok = file_fail(source, errno);
    if (ok && (before.st_size != after.st_size || before.st_mtim.tv_sec != after.st_mtim.tv_sec ||
               before.st_mtim.tv_nsec != after.st_mtim.tv_nsec || before.st_ctim.tv_sec != after.st_ctim.tv_sec ||
               before.st_ctim.tv_nsec != after.st_ctim.tv_nsec))
        ok = file_fail(source, ESTALE);
    ok = file_finish(source, ok);
    string_t *result = NULL;
    if (ok) {
        crypto_hash_sha256_final(&state, digest);
        char hex[crypto_hash_sha256_BYTES * 2 + 1];
        sodium_bin2hex(hex, sizeof(hex), digest, sizeof(digest));
        result = string_new_with(hex);
        if (!result)
            file_fail(source, ENOMEM);
    }
    sodium_memzero(&state, sizeof(state));
    return result;
}
