/**
 * @file textures.c
 * @brief Indexed photographic texture metadata and bounded, verified JPEG reads.
 *
 * The globe controller supplies its borrowed geometry connection. Metadata and payload reads are separate so
 * HTTP consumers can revalidate browser caches without reading or decrypting an image. No image codec, migration,
 * filesystem fallback or persistent pixel cache is part of this reader. Installers validate JPEG dimensions.
 */
#include <string.h>
#include <sodium.h>

#include "array-t.h"
#define MARS_GLOBE_INTERNAL_ACCESS
#include "globe_layers.h"

static bool texture_address(unsigned level, unsigned column, unsigned row)
{
    return level < 3 && (level == 2 ? column < 12 && row < 6 : column == 0 && row == 0);
}

static bool texture_digest(const char *text)
{
    if (!text || strlen(text) != 64) return false;
    for (unsigned i = 0; i < 64; ++i)
        if (!((text[i] >= '0' && text[i] <= '9') || (text[i] >= 'a' && text[i] <= 'f'))) return false;
    return true;
}

bool textures_metadata(sqlite_t *db, unsigned level, unsigned column, unsigned row, globe_texture_info_t *info)
{
    if (info) *info = (globe_texture_info_t){0};
    if (!db || !info || !texture_address(level, column, row)) return false;
    sqlite_stmt_t *q = sqlite_stmt_prepare(db,
        "select width,height,byte_count,sha256 from globe_texture "
        "where level=?1 and column_index=?2 and row_index=?3 and length(jpeg)=byte_count "
        "and exists(select 1 from globe_texture_dataset where id=1 and format_version=1)");
    bool ok = q && sqlite_stmt_bind_int(q, 1, level) && sqlite_stmt_bind_int(q, 2, column) &&
              sqlite_stmt_bind_int(q, 3, row) && sqlite_stmt_step(q) == SQLITE_STEP_ROW;
    static const int widths[] = {2048, 8192, 1800}, heights[] = {1024, 4096, 1800};
    int64_t size = ok ? sqlite_stmt_column_int64(q, 2) : 0;
    const char *digest = ok ? sqlite_stmt_column_text(q, 3) : NULL;
    ok = ok && sqlite_stmt_column_int(q, 0) == widths[level] && sqlite_stmt_column_int(q, 1) == heights[level] &&
         size >= 4 && size <= (level == 1 ? 16777216 : 4194304) && texture_digest(digest);
    if (ok) {
        info->width = widths[level]; info->height = heights[level]; info->byte_count = (size_t)size;
        memcpy(info->sha256, digest, sizeof info->sha256);
    }
    sqlite_stmt_finalize(q);
    return ok;
}

array_t *textures_jpeg(sqlite_t *db, unsigned level, unsigned column, unsigned row, const globe_texture_info_t *expected)
{
    globe_texture_info_t current;
    if (!expected || !memchr(expected->sha256, 0, sizeof expected->sha256) ||
        !textures_metadata(db, level, column, row, &current) ||
        current.width != expected->width || current.height != expected->height ||
        current.byte_count != expected->byte_count || strcmp(current.sha256, expected->sha256)) return NULL;
    sqlite_stmt_t *q = sqlite_stmt_prepare(db,
        "select jpeg from globe_texture where level=?1 and column_index=?2 and row_index=?3 and sha256=?4 "
        "and byte_count=?5 and length(jpeg)=?5");
    bool ok = q && sqlite_stmt_bind_int(q, 1, level) && sqlite_stmt_bind_int(q, 2, column) &&
              sqlite_stmt_bind_int(q, 3, row) && sqlite_stmt_bind_text(q, 4, expected->sha256) &&
              sqlite_stmt_bind_int64(q, 5, expected->byte_count) && sqlite_stmt_step(q) == SQLITE_STEP_ROW;
    size_t size = ok ? sqlite_stmt_column_bytes(q, 0) : 0;
    const unsigned char *data = ok ? sqlite_stmt_column_blob(q, 0) : NULL;
    ok = ok && data && size == expected->byte_count && size >= 4 && data[0] == 0xff && data[1] == 0xd8 &&
         data[size - 2] == 0xff && data[size - 1] == 0xd9;
    unsigned char digest[crypto_hash_sha256_BYTES];
    char hex[65];
    if (ok) {
        ok = crypto_hash_sha256(digest, data, size) == 0 &&
             sodium_bin2hex(hex, sizeof hex, digest, sizeof digest) && !strcmp(hex, expected->sha256);
    }
    array_t *bytes = ok ? array_create(1, NULL, NULL) : NULL;
    ok = bytes && array_append_carray(bytes, data, size);
    sqlite_stmt_finalize(q);
    if (!ok) { array_destroy(bytes); bytes = NULL; }
    return bytes;
}
