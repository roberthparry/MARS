/**
 * @file cfg_database_textures.c
 * @brief Transactional installation of packaged photographic surface imagery.
 *
 * Imports two global PNGs, 72 NASA tiles and up to 64 regional photographs into the independent globe store. A bounded
 * manifest supplies content digests and source attribution; each PNG must match its declared size, dimensions
 * and digest. Savepoints preserve an existing image set on missing or corrupt input, including within migrations.
 * No image transcoding, network download or geometry replacement occurs here.
 */
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sodium.h>

#include "array-t.h"
#include "globe-t.h"
#include "json-t.h"
#include "string-t.h"
#include "mars_paths.h"
#include "cfg_progress.h"
#define MARS_CFG_DATABASE_INTERNAL_ACCESS
#include "cfg_database_internal.h"

static const json_t *texture_field(const json_t *value, const char *name)
{
    string_t *key = string_new_with(name);
    const json_t *field = key ? json_object_get(value, key) : NULL;
    string_free(key);
    return field;
}

static const char *texture_text(const json_t *value, const char *name, size_t maximum)
{
    const string_t *text = json_string_value(texture_field(value, name));
    return text && string_byte_length(text) && string_byte_length(text) <= maximum &&
           strlen(string_c_str(text)) == string_byte_length(text) ? string_c_str(text) : NULL;
}

static bool texture_number(const json_t *value, const char *name, uint64_t expected)
{
    const string_t *text = json_number_text(texture_field(value, name));
    if (!text) return false;
    const char *start = string_c_str(text);
    char *end = NULL;
    errno = 0;
    unsigned long long number = strtoull(start, &end, 10);
    return start[0] >= '0' && start[0] <= '9' && end && !*end && !errno && number == expected;
}

/* Validate chunk bounds and the RGB header without decoding the photograph. */
static unsigned texture_u32(const unsigned char *p)
{
    return ((unsigned)p[0] << 24) | ((unsigned)p[1] << 16) | ((unsigned)p[2] << 8) | p[3];
}

static bool texture_dimensions(const unsigned char *data, size_t size, unsigned width, unsigned height)
{
    if (size < 57 || memcmp(data, "\211PNG\r\n\032\n", 8)) return false;
    bool header = false, pixels = false;
    for (size_t at = 8; at + 12 <= size;) {
        unsigned length = texture_u32(data + at);
        if (length > size - at - 12) return false;
        const unsigned char *type = data + at + 4, *body = data + at + 8;
        if (!header) {
            if (memcmp(type, "IHDR", 4) || length != 13 || texture_u32(body) != width ||
                texture_u32(body + 4) != height || body[8] != 8 || body[9] != 2 ||
                body[10] || body[11] || body[12]) return false;
            header = true;
        } else if (!memcmp(type, "IHDR", 4)) return false;
        if (!memcmp(type, "IDAT", 4)) pixels = true;
        if (!memcmp(type, "IEND", 4)) return !length && pixels && at + 12 == size;
        at += (size_t)length + 12;
    }
    return false;
}

static bool texture_import(sqlite_stmt_t *insert, const string_t *directory, const json_t *assets,
                           unsigned level, unsigned column, unsigned row)
{
    static const unsigned widths[] = {2048, 8192, 1800, 1800}, heights[] = {1024, 4096, 1800, 1800};
    string_t *relative = level == 3 ? string_sprintf("regional/%02u.png", column) :
                        level == 2 ? string_sprintf("earth_tiles/%02u-%02u.png", column, row)
                                   : string_new_with(level ? "earth_surface_detail.png" : "earth_surface.png");
    const json_t *asset = relative ? json_object_get(assets, relative) : NULL;
    unsigned width = level == 3 && texture_number(asset, "width", 3600) ? 3600 : widths[level];
    unsigned height = level == 3 ? width : heights[level];
    const char *expected = texture_text(asset, "sha256", 64);
    string_t *path = relative ? string_sprintf("%s/%s", string_c_str(directory),
                                               string_c_str(relative)) : NULL;
    file_t *file = path ? file_new(path) : NULL;
    file_info_t *info = file ? file_get_info(file) : NULL;
    size_t maximum = level == 1 ? 134217728 : level == 3 ? 67108864 : 16777216;
    bool ok = info && file_info_type(info) == FILE_TYPE_REGULAR && file_info_size(info) <= maximum &&
              expected && strlen(expected) == 64 && texture_number(asset, "width", width) &&
              texture_number(asset, "height", height) && texture_number(asset, "bytes", file_info_size(info));
    array_t *bytes = ok ? file_read_all_bytes(file) : NULL;
    size_t size = array_size(bytes);
    const unsigned char *data = bytes && size ? array_get(bytes, 0) : NULL;
    unsigned char digest[crypto_hash_sha256_BYTES];
    char hex[65];
    ok = data && size <= maximum && size == file_info_size(info) &&
         texture_dimensions(data, size, width, height) &&
         crypto_hash_sha256(digest, data, size) == 0 && sodium_bin2hex(hex, sizeof hex, digest, sizeof digest) &&
         !strcmp(hex, expected) && sqlite_stmt_bind_int(insert, 1, level) &&
         sqlite_stmt_bind_int(insert, 2, column) && sqlite_stmt_bind_int(insert, 3, row) &&
         sqlite_stmt_bind_int(insert, 4, width) && sqlite_stmt_bind_int(insert, 5, height) &&
         sqlite_stmt_bind_int64(insert, 6, size) && sqlite_stmt_bind_text(insert, 7, hex) &&
         sqlite_stmt_bind_blob(insert, 8, data, size) && sqlite_stmt_step(insert) == SQLITE_STEP_DONE;
    sqlite_stmt_reset(insert);
    array_destroy(bytes); file_info_free(info); file_free(file); string_free(path); string_free(relative);
    return ok;
}

/* Optional catalogue uses fixed numeric slots; it cannot introduce arbitrary file paths. */
static bool texture_regions(sqlite_t *db, sqlite_stmt_t *insert, const string_t *directory, const json_t *manifest)
{
    const json_t *regions = texture_field(manifest, "regions");
    size_t count = regions ? json_array_size(regions) : 0;
    if (regions && (json_type(regions) != JSON_ARRAY || count > 64)) return false;
    if (!sqlite_exec_cstr(db, "drop table if exists globe_texture_region;"
        "create table globe_texture_region(slot integer primary key check(slot>=0 and slot<64),"
        "west real,south real,east real,north real,attribution text,source_url text)")) return false;
    sqlite_stmt_t *q = sqlite_stmt_prepare(db, "insert into globe_texture_region values(?1,?2,?3,?4,?5,?6,?7)");
    bool ok = q != NULL;
    for (size_t i = 0; ok && i < count; ++i) {
        const json_t *region = json_array_get(regions, i), *bounds = texture_field(region, "bounds");
        const char *credit = texture_text(region, "attribution", 4096), *url = texture_text(region, "source_url", 2048);
        double b[4] = {0};
        ok = credit && url && json_type(bounds) == JSON_ARRAY && json_array_size(bounds) == 4;
        for (unsigned j = 0; ok && j < 4; ++j) {
            const string_t *number = json_number_text(json_array_get(bounds, j));
            char *end = NULL;
            b[j] = number ? strtod(string_c_str(number), &end) : NAN;
            ok = end && !*end && isfinite(b[j]);
        }
        ok = ok && b[0] >= -180 && b[2] <= 180 && b[1] >= -90 && b[3] <= 90 &&
             b[0] < b[2] && b[1] < b[3] &&
             texture_import(insert, directory, texture_field(manifest, "assets"), 3, (unsigned)i, 0) &&
             sqlite_stmt_bind_int(q, 1, (int)i);
        for (unsigned j = 0; ok && j < 4; ++j) ok = sqlite_stmt_bind_double(q, j + 2, b[j]);
        ok = ok && sqlite_stmt_bind_text(q, 6, credit) && sqlite_stmt_bind_text(q, 7, url) &&
             sqlite_stmt_step(q) == SQLITE_STEP_DONE;
        sqlite_stmt_reset(q);
    }
    sqlite_stmt_finalize(q);
    return ok;
}

bool cfg_database_textures(sqlite_t *db, const string_t *root)
{
    if (!db || !root) return false;
    string_t *directory = mars_path_resource(MARS_PATH_TEXTURES, root, "");
    string_t *path = directory ? string_sprintf("%s/manifest.json", string_c_str(directory)) : NULL;
    file_t *file = path ? file_new(path) : NULL;
    file_info_t *info = file ? file_get_info(file) : NULL;
    string_t *text = info && file_info_size(info) <= 1048576 ? file_read_all_text(file) : NULL;
    json_t *manifest = text ? json_from_text(text) : NULL;
    const json_t *assets = texture_field(manifest, "assets");
    const char *name = texture_text(manifest, "name", 256), *url = texture_text(manifest, "source_url", 2048);
    const char *source_digest = texture_text(manifest, "source_sha256", 64);
    const char *attribution = texture_text(manifest, "attribution", 4096);
    const char *month = texture_text(manifest, "reference_month", 7);
    bool ok = texture_number(manifest, "format_version", 2) && json_type(assets) == JSON_OBJECT &&
              json_object_size(assets) == 74 + json_array_size(texture_field(manifest, "regions")) &&
              name && url && source_digest && strlen(source_digest) == 64 &&
              attribution && month && !strcmp(month, "2004-07");
    bool transaction = ok && sqlite_exec_cstr(db, "savepoint globe_texture_import");
    ok = transaction && sqlite_exec_cstr(db,
        "drop table if exists globe_texture;drop table if exists globe_texture_dataset;"
        "create table globe_texture_dataset(id integer primary key,format_version integer not null,name text not null,"
        "source_url text not null,source_sha256 text not null,attribution text not null,reference_month text not null);"
        "create table globe_texture(level integer not null,column_index integer not null,row_index integer not null,"
        "width integer not null,height integer not null,byte_count integer not null,sha256 text not null,png blob not null,"
        "primary key(level,column_index,row_index));");
    sqlite_stmt_t *meta = ok ? sqlite_stmt_prepare(db, "insert into globe_texture_dataset values(1,2,?1,?2,?3,?4,?5)") : NULL;
    ok = meta && sqlite_stmt_bind_text(meta, 1, name) && sqlite_stmt_bind_text(meta, 2, url) &&
         sqlite_stmt_bind_text(meta, 3, source_digest) && sqlite_stmt_bind_text(meta, 4, attribution) &&
         sqlite_stmt_bind_text(meta, 5, month) && sqlite_stmt_step(meta) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(meta);
    sqlite_stmt_t *insert = ok ? sqlite_stmt_prepare(db, "insert into globe_texture values(?1,?2,?3,?4,?5,?6,?7,?8)") : NULL;
    cfg_progress_t *progress = cfg_progress_start("Installing photographic globe textures");
    ok = ok && insert;
    for (unsigned level = 0; ok && level < 3; ++level)
        for (unsigned row = 0; ok && row < (level == 2 ? 6u : 1u); ++row)
            for (unsigned column = 0; ok && column < (level == 2 ? 12u : 1u); ++column)
                ok = texture_import(insert, directory, assets, level, column, row);
    if (ok) ok = texture_regions(db, insert, directory, manifest);
    sqlite_stmt_finalize(insert);
    if (ok) ok = sqlite_exec_cstr(db, "release globe_texture_import");
    if (transaction && !ok) sqlite_exec_cstr(db, "rollback to globe_texture_import;release globe_texture_import");
    cfg_progress_finish(progress, ok);
    json_free(manifest); string_free(text); file_info_free(info); file_free(file); string_free(path); string_free(directory);
    return ok;
}

bool cfg_database_textures_valid(sqlite_t *db)
{
    globe_t *globe = db ? globe_open_databases(db, db, NULL) : NULL;
    sqlite_stmt_t *count = db ? sqlite_stmt_prepare(db, "select count(*) from globe_texture") : NULL;
    int total = count && sqlite_stmt_step(count) == SQLITE_STEP_ROW ? sqlite_stmt_column_int(count, 0) : 0;
    bool ok = globe && total >= 74 && total <= 138;
    sqlite_stmt_finalize(count);
    for (unsigned level = 0; ok && level < 3; ++level) {
        for (unsigned row = 0; ok && row < (level == 2 ? 6u : 1u); ++row) {
            for (unsigned column = 0; ok && column < (level == 2 ? 12u : 1u); ++column) {
                globe_texture_info_t info;
                array_t *png = globe_texture_metadata(globe, level, column, row, &info)
                                    ? globe_texture_png(globe, level, column, row, &info) : NULL;
                ok = png != NULL;
                array_destroy(png);
            }
        }
    }
    for (int slot = 0; ok && slot < total - 74; ++slot) {
        globe_texture_info_t info;
        array_t *png = globe_texture_metadata(globe, 3, (unsigned)slot, 0, &info)
            ? globe_texture_png(globe, 3, (unsigned)slot, 0, &info) : NULL;
        ok = png != NULL;
        array_destroy(png);
    }
    globe_close(globe);
    return ok;
}
