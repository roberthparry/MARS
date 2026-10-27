/**
 * @file textures.c
 * @brief Indexed photographic texture metadata and bounded, verified PNG reads.
 *
 * The globe controller supplies its borrowed geometry connection. Metadata and payload reads are separate so
 * HTTP consumers can revalidate browser caches without reading or decrypting an image. No image codec, migration,
 * filesystem fallback or persistent pixel cache is part of this reader. Installers validate PNG dimensions.
 */
#include <math.h>
#include <string.h>
#include <sodium.h>

#include "array-t.h"
#define MARS_GLOBE_INTERNAL_ACCESS
#include "globe_layers.h"

static bool texture_address(unsigned level, unsigned column, unsigned row)
{
    return level == 3 ? column < 64 && row == 0 :
        level < 3 && (level == 2 ? column < 12 && row < 6 : column == 0 && row == 0);
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
        "where level=?1 and column_index=?2 and row_index=?3 and length(png)=byte_count "
        "and exists(select 1 from globe_texture_dataset where id=1 and format_version=2)");
    bool ok = q && sqlite_stmt_bind_int(q, 1, level) && sqlite_stmt_bind_int(q, 2, column) &&
              sqlite_stmt_bind_int(q, 3, row) && sqlite_stmt_step(q) == SQLITE_STEP_ROW;
    static const int widths[] = {2048, 8192, 1800, 1800}, heights[] = {1024, 4096, 1800, 1800};
    int width = ok && level == 3 && sqlite_stmt_column_int(q, 0) == 3600 ? 3600 : widths[level];
    int height = level == 3 ? width : heights[level];
    int64_t size = ok ? sqlite_stmt_column_int64(q, 2) : 0;
    const char *digest = ok ? sqlite_stmt_column_text(q, 3) : NULL;
    ok = ok && sqlite_stmt_column_int(q, 0) == width && sqlite_stmt_column_int(q, 1) == height &&
         size >= 4 && size <= (level == 1 ? 134217728 : level == 3 ? 67108864 : 16777216) && texture_digest(digest);
    if (ok) {
        info->width = width; info->height = height; info->byte_count = (size_t)size;
        memcpy(info->sha256, digest, sizeof info->sha256);
    }
    sqlite_stmt_finalize(q);
    return ok;
}

bool textures_regions(sqlite_t *db, const double bounds[4], globe_texture_region_fn callback, void *context)
{
    if (!db || !bounds || !callback) return false;
    for (unsigned i = 0; i < 4; ++i) if (!isfinite(bounds[i])) return false;
    if (bounds[0] >= bounds[2] || bounds[1] >= bounds[3]) return false;
    sqlite_stmt_t *exists = sqlite_stmt_prepare(db,
        "select 1 from sqlite_master where type='table' and name='globe_texture_region'");
    sqlite_step_result_t step = exists ? sqlite_stmt_step(exists) : SQLITE_STEP_ERROR;
    sqlite_stmt_finalize(exists);
    if (step != SQLITE_STEP_ROW) return step == SQLITE_STEP_DONE;
    /* The optional catalogue is capped at 64 slots, so this metadata-only scan has a fixed small bound. */
    sqlite_stmt_t *q = sqlite_stmt_prepare(db,
        "select slot,west,south,east,north from globe_texture_region "
        "where west<?1 and east>?2 and south<?3 and north>?4 order by slot limit 65");
    bool ok = q && sqlite_stmt_bind_double(q, 1, bounds[2]) && sqlite_stmt_bind_double(q, 2, bounds[0]) &&
              sqlite_stmt_bind_double(q, 3, bounds[3]) && sqlite_stmt_bind_double(q, 4, bounds[1]);
    unsigned count = 0;
    while (ok && (step = sqlite_stmt_step(q)) == SQLITE_STEP_ROW) {
        int slot = sqlite_stmt_column_int(q, 0);
        double b[4];
        for (unsigned i = 0; i < 4; ++i) {
            b[i] = sqlite_stmt_column_double(q, i + 1);
            ok = ok && isfinite(b[i]);
        }
        ok = ok && ++count <= 64 && slot >= 0 && slot < 64 && b[0] >= -180 && b[2] <= 180 &&
             b[1] >= -90 && b[3] <= 90 && b[0] < b[2] && b[1] < b[3] && callback((unsigned)slot, b, context);
    }
    ok = ok && step == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(q);
    return ok;
}

array_t *textures_png(sqlite_t *db, unsigned level, unsigned column, unsigned row, const globe_texture_info_t *expected)
{
    globe_texture_info_t current;
    if (!expected || !memchr(expected->sha256, 0, sizeof expected->sha256) ||
        !textures_metadata(db, level, column, row, &current) ||
        current.width != expected->width || current.height != expected->height ||
        current.byte_count != expected->byte_count || strcmp(current.sha256, expected->sha256)) return NULL;
    sqlite_stmt_t *q = sqlite_stmt_prepare(db,
        "select png from globe_texture where level=?1 and column_index=?2 and row_index=?3 and sha256=?4 "
        "and byte_count=?5 and length(png)=?5");
    bool ok = q && sqlite_stmt_bind_int(q, 1, level) && sqlite_stmt_bind_int(q, 2, column) &&
              sqlite_stmt_bind_int(q, 3, row) && sqlite_stmt_bind_text(q, 4, expected->sha256) &&
              sqlite_stmt_bind_int64(q, 5, expected->byte_count) && sqlite_stmt_step(q) == SQLITE_STEP_ROW;
    size_t size = ok ? sqlite_stmt_column_bytes(q, 0) : 0;
    const unsigned char *data = ok ? sqlite_stmt_column_blob(q, 0) : NULL;
    ok = ok && data && size == expected->byte_count && size >= 57 && !memcmp(data, "\211PNG\r\n\032\n", 8);
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
