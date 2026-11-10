/**
 * @file cfg_database_textures.c
 * @brief Transactional installation of packaged photographic surface imagery.
 *
 * Imports global PNGs and a bounded, spatially indexed regional catalogue into the independent globe store. A bounded
 * manifest supplies content digests and source attribution; each PNG must match its declared size, dimensions
 * and digest. Compatible updates retain content-identical, already validated PNG rows and replace changed photographs
 * without copying the whole catalogue. The complete manifest is retained alongside the indexed fields and provenance.
 * Savepoints preserve an existing image set on missing or corrupt input, including within migrations.
 * Live reporters cover blocking table replacement, validated image counts and transaction completion or rollback.
 * No image transcoding, network download or geometry replacement occurs here.
 */
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sodium.h>

#include "array-t.h"
#include "globe-t.h"
#include "image-t.h"
#include "json-t.h"
#include "string-t.h"
#include "mars_paths.h"
#include "cfg_progress.h"
#define MARS_CFG_DATABASE_INTERNAL_ACCESS
#include "cfg_database_internal.h"

typedef struct _texture_progress_t {
    cfg_progress_t *reporter;
    sqlite_stmt_t *existing;
    uint64_t imported;
    uint64_t retained;
    uint64_t total;
} texture_progress_t;

static const char texture_import_label[] = "Installing photographic globe textures";
static const char texture_import_unit[] = "tiles imported";

static const char *texture_unit(const texture_progress_t *progress)
{
    return progress->existing ? "tiles checked" : texture_import_unit;
}

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

/* Validate dimensions, supported PNG layout and chunk checksums without decoding pixels. */
static bool texture_dimensions(const unsigned char *data, size_t size, unsigned width, unsigned height)
{
    unsigned actual_width, actual_height;
    return image_png_header(data, size, &actual_width, &actual_height) &&
           actual_width == width && actual_height == height && image_png_validate(data, size);
}

/* A matching content address retains its previously validated image without reading the PNG body again. */
static int texture_retained(sqlite_stmt_t *query, unsigned level, unsigned column, unsigned row,
                            unsigned width, unsigned height, uint64_t bytes, const char *digest)
{
    if (!query) return 0;
    bool bound = sqlite_stmt_bind_int(query, 1, level) && sqlite_stmt_bind_int(query, 2, column) &&
                 sqlite_stmt_bind_int(query, 3, row);
    sqlite_step_result_t step = bound ? sqlite_stmt_step(query) : SQLITE_STEP_ERROR;
    int retained = step == SQLITE_STEP_ERROR ? -1 : step == SQLITE_STEP_ROW &&
        sqlite_stmt_column_int64(query, 0) == width && sqlite_stmt_column_int64(query, 1) == height &&
        sqlite_stmt_column_int64(query, 2) == (int64_t)bytes &&
        !strcmp(sqlite_stmt_column_text(query, 3), digest) && sqlite_stmt_column_int64(query, 4) == (int64_t)bytes;
    sqlite_stmt_reset(query);
    return retained;
}

static bool texture_import(sqlite_stmt_t *insert, const string_t *directory, const json_t *manifest,
                           unsigned level, unsigned column, unsigned row, texture_progress_t *progress)
{
    string_t *name = cfg_database_texture_name(manifest, level, column);
    cfg_progress_segment_name(progress->reporter, name);
    string_free(name);
    const json_t *assets = texture_field(manifest, "assets");
    static const unsigned widths[] = {2048, 8192, 1800, 1800, 1800}, heights[] = {1024, 4096, 1800, 1800, 1800};
    string_t *relative = level == 4 ? string_sprintf("earth_tiles_detail/%02u-%02u.png", column, row) :
                        level == 3 ? string_sprintf("regional/%02u.png", column) :
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
    int retained = ok ? texture_retained(progress->existing, level, column, row, width, height,
                                        file_info_size(info), expected) : 0;
    ok = ok && retained >= 0;
    array_t *bytes = ok && !retained ? file_read_all_bytes(file) : NULL;
    size_t size = array_size(bytes);
    const unsigned char *data = bytes && size ? array_get(bytes, 0) : NULL;
    unsigned char digest[crypto_hash_sha256_BYTES];
    char hex[65];
    if (ok && !retained) ok = data && size <= maximum && size == file_info_size(info) &&
         texture_dimensions(data, size, width, height) &&
         crypto_hash_sha256(digest, data, size) == 0 && sodium_bin2hex(hex, sizeof hex, digest, sizeof digest) &&
         !strcmp(hex, expected) && sqlite_stmt_bind_int(insert, 1, level) &&
         sqlite_stmt_bind_int(insert, 2, column) && sqlite_stmt_bind_int(insert, 3, row) &&
         sqlite_stmt_bind_int(insert, 4, width) && sqlite_stmt_bind_int(insert, 5, height) &&
         sqlite_stmt_bind_int64(insert, 6, size) && sqlite_stmt_bind_text(insert, 7, hex) &&
         sqlite_stmt_bind_blob(insert, 8, data, size) && sqlite_stmt_step(insert) == SQLITE_STEP_DONE;
    sqlite_stmt_reset(insert);
    array_destroy(bytes); file_info_free(info); file_free(file); string_free(path); string_free(relative);
    if (ok) {
        ++progress->imported;
        progress->retained += retained != 0;
        cfg_progress_update_items(progress->reporter, texture_import_label, progress->imported, progress->total,
                                  texture_unit(progress));
    }
    return ok;
}

/* Optional catalogue uses fixed numeric slots; it cannot introduce arbitrary file paths. */
static bool texture_regions(sqlite_t *db, sqlite_stmt_t *insert, const string_t *directory, const json_t *manifest,
                            texture_progress_t *progress)
{
    const json_t *regions = texture_field(manifest, "regions");
    size_t count = regions ? json_array_size(regions) : 0;
    if (regions && (json_type(regions) != JSON_ARRAY || count > GLOBE_TEXTURE_REGION_LIMIT)) return false;
    string_t *schema = string_sprintf(
        "DROP TABLE IF EXISTS globe_texture_region_index;\n"
        "DROP TABLE IF EXISTS globe_texture_region;\n"
        "CREATE TABLE globe_texture_region (\n"
        "    slot INTEGER PRIMARY KEY CHECK (slot >= 0 AND slot < %u),\n"
        "    west REAL, south REAL, east REAL, north REAL, attribution TEXT, source_url TEXT\n"
        ")", (unsigned)GLOBE_TEXTURE_REGION_LIMIT);
    bool prepared = schema && sqlite_exec(db, schema);
    string_free(schema);
    if (!prepared) return false;
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
             texture_import(insert, directory, manifest, 3, (unsigned)i, 0, progress) &&
             sqlite_stmt_bind_int(q, 1, (int)i);
        for (unsigned j = 0; ok && j < 4; ++j) ok = sqlite_stmt_bind_double(q, j + 2, b[j]);
        ok = ok && sqlite_stmt_bind_text(q, 6, credit) && sqlite_stmt_bind_text(q, 7, url) &&
             sqlite_stmt_step(q) == SQLITE_STEP_DONE;
        sqlite_stmt_reset(q);
    }
    sqlite_stmt_finalize(q);
    return ok && sqlite_exec_cstr(db,
        "create virtual table globe_texture_region_index using rtree(slot,west,east,south,north);"
        "insert into globe_texture_region_index select slot,west,east,south,north from globe_texture_region");
}

/* Replace changed PNG rows in compatible stores; older formats retain the complete validated rebuild. */
bool cfg_database_textures(sqlite_t *db, const string_t *root)
{
    if (!db || !root) return false;
    string_t *directory = mars_path_resource(MARS_PATH_TEXTURES, root, "");
    string_t *path = directory ? string_sprintf("%s/manifest.json", string_c_str(directory)) : NULL;
    file_t *file = path ? file_new(path) : NULL;
    file_info_t *info = file ? file_get_info(file) : NULL;
    string_t *text = info && file_info_size(info) <= GLOBE_TEXTURE_MANIFEST_LIMIT ? file_read_all_text(file) : NULL;
    json_t *manifest = text ? json_from_text(text) : NULL;
    const json_t *assets = texture_field(manifest, "assets");
    const json_t *regions = texture_field(manifest, "regions");
    size_t region_count = json_array_size(regions);
    const json_t *detail = texture_field(manifest, "global_detail");
    size_t detail_count = detail ? 1152 : 0;
    const char *name = texture_text(manifest, "name", 256), *url = texture_text(manifest, "source_url", 2048);
    const char *source_digest = texture_text(manifest, "source_sha256", 64);
    const char *attribution = texture_text(manifest, "attribution", 4096);
    const char *month = texture_text(manifest, "reference_month", 7);
    bool ok = texture_number(manifest, "format_version", 2) && json_type(assets) == JSON_OBJECT &&
              (!regions || (json_type(regions) == JSON_ARRAY && region_count <= GLOBE_TEXTURE_REGION_LIMIT)) &&
              (!detail || (texture_number(detail, "width", 86400) && texture_number(detail, "height", 43200) &&
                  texture_number(detail, "columns", 48) && texture_number(detail, "rows", 24) &&
                  texture_number(detail, "tile_size", 1800))) &&
              json_object_size(assets) == 74 + region_count + detail_count &&
              name && url && source_digest && strlen(source_digest) == 64 &&
              attribution && month && !strcmp(month, "2004-07");
    int64_t revision = 0;
    ok = ok && cfg_database_current_revision(db, "globe", &revision);
    sqlite_stmt_t *tables = ok ? sqlite_stmt_prepare(db,
        "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = 'globe_texture'") : NULL;
    sqlite_step_result_t found = tables ? sqlite_stmt_step(tables) : SQLITE_STEP_ERROR;
    bool incremental = revision >= CFG_GLOBE_TEXTURE_PATCH_BASE && found == SQLITE_STEP_ROW;
    ok = ok && found != SQLITE_STEP_ERROR;
    sqlite_stmt_finalize(tables);
    cfg_progress_t *progress = ok ? cfg_progress_start("Preparing photographic texture tables") : NULL;
    cfg_progress_explain(progress,
        incremental ? "Keeping validated images; only changed PNG rows will be read and replaced." :
        "Releasing the previous image tables. Large catalogues can take several minutes; this step has no tile count.");
    bool transaction = ok && sqlite_exec_cstr(db, "savepoint globe_texture_import");
    ok = transaction && cfg_database_public_ddl(db, incremental ?
        "delete from globe_texture_manifest;delete from globe_texture_dataset;" :
        "drop table if exists globe_texture;drop table if exists globe_texture_dataset;"
        "drop table if exists globe_texture_manifest;"
        "create table globe_texture_manifest(id integer primary key check(id=1),json text not null);"
        "create table globe_texture_dataset(id integer primary key,format_version integer not null,name text not null,"
        "source_url text not null,source_sha256 text not null,attribution text not null,reference_month text not null);"
        "create table globe_texture(level integer not null,column_index integer not null,row_index integer not null,"
        "width integer not null,height integer not null,byte_count integer not null,sha256 text not null,png blob not null,"
        "primary key(level,column_index,row_index));");
    cfg_progress_finish(progress, ok);
    progress = ok ? cfg_progress_start(texture_import_label) : NULL;
    sqlite_stmt_t *existing = ok && incremental ? sqlite_stmt_prepare(db,
        "SELECT width, height, byte_count, sha256, length(png)\n"
        "FROM globe_texture\n"
        "WHERE level = ?1 AND column_index = ?2 AND row_index = ?3") : NULL;
    ok = ok && (!incremental || existing);
    texture_progress_t imported = {.reporter = progress, .existing = existing,
                                  .total = 74 + region_count + detail_count};
    cfg_progress_update_items(progress, texture_import_label, 0, imported.total, texture_unit(&imported));
    cfg_progress_explain(progress,
        incremental ? "Checking content addresses. Unchanged images stay in place; changed images are validated and imported." :
        "Importing local images. Each tile is counted after validation and insertion; individual tiles vary in size.");
    sqlite_stmt_t *meta = ok ? sqlite_stmt_prepare(db, "insert into globe_texture_dataset values(1,2,?1,?2,?3,?4,?5)") : NULL;
    ok = meta && sqlite_stmt_bind_text(meta, 1, name) && sqlite_stmt_bind_text(meta, 2, url) &&
         sqlite_stmt_bind_text(meta, 3, source_digest) && sqlite_stmt_bind_text(meta, 4, attribution) &&
         sqlite_stmt_bind_text(meta, 5, month) && sqlite_stmt_step(meta) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(meta);
    sqlite_stmt_t *snapshot = ok ? sqlite_stmt_prepare(db,
        "insert into globe_texture_manifest(id,json) values(1,?1)") : NULL;
    ok = snapshot && sqlite_stmt_bind_text(snapshot, 1, string_c_str(text)) &&
         sqlite_stmt_step(snapshot) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(snapshot);
    unsigned char catalogue_digest[crypto_hash_sha256_BYTES];
    char catalogue_hex[65];
    ok = ok && crypto_hash_sha256(catalogue_digest, (const unsigned char *)string_c_str(text),
                                 string_byte_length(text)) == 0 &&
         sodium_bin2hex(catalogue_hex, sizeof catalogue_hex, catalogue_digest, sizeof catalogue_digest) &&
         sqlite_exec_cstr(db, "CREATE TABLE IF NOT EXISTS globe_texture_generation ("
                             "id INTEGER PRIMARY KEY CHECK(id=1),sha256 TEXT NOT NULL)");
    snapshot = ok ? sqlite_stmt_prepare(db,
        "INSERT INTO globe_texture_generation VALUES(1,?1)\n"
        "ON CONFLICT(id) DO UPDATE SET sha256=excluded.sha256") : NULL;
    ok = snapshot && sqlite_stmt_bind_text(snapshot, 1, catalogue_hex) &&
         sqlite_stmt_step(snapshot) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(snapshot);
    sqlite_stmt_t *insert = ok ? sqlite_stmt_prepare(db,
        "INSERT INTO globe_texture VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)\n"
        "ON CONFLICT (level, column_index, row_index) DO UPDATE SET\n"
        "    width = excluded.width, height = excluded.height, byte_count = excluded.byte_count,\n"
        "    sha256 = excluded.sha256, png = excluded.png") : NULL;
    ok = ok && insert;
    for (unsigned level = 0; ok && level < 3; ++level)
        for (unsigned row = 0; ok && row < (level == 2 ? 6u : 1u); ++row)
            for (unsigned column = 0; ok && column < (level == 2 ? 12u : 1u); ++column)
                ok = texture_import(insert, directory, manifest, level, column, row, &imported);
    for (unsigned row = 0; ok && detail && row < 24; ++row)
        for (unsigned column = 0; ok && column < 48; ++column)
            ok = texture_import(insert, directory, manifest, 4, column, row, &imported);
    if (ok) ok = texture_regions(db, insert, directory, manifest, &imported);
    sqlite_stmt_finalize(insert);
    sqlite_stmt_finalize(existing);
    if (ok && incremental) {
        sqlite_stmt_t *obsolete = sqlite_stmt_prepare(db,
            "DELETE FROM globe_texture WHERE level = 3 AND column_index >= ?1");
        ok = obsolete && sqlite_stmt_bind_int64(obsolete, 1, region_count) &&
             sqlite_stmt_step(obsolete) == SQLITE_STEP_DONE;
        sqlite_stmt_finalize(obsolete);
        if (ok && !detail) ok = sqlite_exec_cstr(db, "DELETE FROM globe_texture WHERE level = 4");
    }
    if (ok) {
        cfg_progress_update_items(progress, "Finalising photographic globe textures", imported.imported, imported.total,
                                  texture_unit(&imported));
        string_t *summary = incremental ? string_sprintf("%llu images replaced; %llu unchanged images retained.",
            (unsigned long long)(imported.imported - imported.retained), (unsigned long long)imported.retained) : NULL;
        cfg_progress_explain(progress, summary ? string_c_str(summary) :
            "All images are inserted. Completing this texture stage can still take time.");
        string_free(summary);
        ok = sqlite_exec_cstr(db, "release globe_texture_import");
    }
    cfg_progress_finish(progress, ok);
    if (transaction && !ok) {
        progress = cfg_progress_start("Restoring previous photographic textures");
        bool restored = sqlite_exec_cstr(db, "rollback to globe_texture_import;release globe_texture_import");
        cfg_progress_finish(progress, restored);
    }
    json_free(manifest); string_free(text); file_info_free(info); file_free(file); string_free(path); string_free(directory);
    return ok;
}

/* Compare the small manifest snapshot; never load image bodies just to decide whether an update is needed. */
bool cfg_database_textures_current(sqlite_t *db, const string_t *root)
{
    string_t *directory = root ? mars_path_resource(MARS_PATH_TEXTURES, root, "") : NULL;
    string_t *path = directory ? string_sprintf("%s/manifest.json", string_c_str(directory)) : NULL;
    file_t *file = path ? file_new(path) : NULL;
    file_info_t *info = file ? file_get_info(file) : NULL;
    string_t *text = info && file_info_size(info) <= GLOBE_TEXTURE_MANIFEST_LIMIT ? file_read_all_text(file) : NULL;
    sqlite_stmt_t *query = db && text ? sqlite_stmt_prepare(db,
        "SELECT json FROM globe_texture_manifest WHERE id = 1") : NULL;
    bool current = query && sqlite_stmt_step(query) == SQLITE_STEP_ROW &&
        sqlite_stmt_column_bytes(query, 0) == string_byte_length(text) &&
        !memcmp(sqlite_stmt_column_text(query, 0), string_c_str(text), string_byte_length(text));
    sqlite_stmt_finalize(query); string_free(text); file_info_free(info); file_free(file);
    string_free(path); string_free(directory);
    return current;
}

bool cfg_database_textures_valid(sqlite_t *db)
{
    globe_t *globe = db ? globe_open_databases(db, db, NULL) : NULL;
    sqlite_stmt_t *count = db ? sqlite_stmt_prepare(db, "select count(*) from globe_texture") : NULL;
    int total = count && sqlite_stmt_step(count) == SQLITE_STEP_ROW ? sqlite_stmt_column_int(count, 0) : 0;
    sqlite_stmt_t *detail = db ? sqlite_stmt_prepare(db, "SELECT count(*) FROM globe_texture WHERE level = 4") : NULL;
    int detail_count = detail && sqlite_stmt_step(detail) == SQLITE_STEP_ROW ? sqlite_stmt_column_int(detail, 0) : -1;
    bool ok = globe && (detail_count == 0 || detail_count == 1152) && total >= 74 + detail_count &&
              total <= 74 + detail_count + GLOBE_TEXTURE_REGION_LIMIT;
    sqlite_stmt_finalize(detail);
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
    for (unsigned row = 0; ok && detail_count && row < 24; ++row) {
        for (unsigned column = 0; ok && column < 48; ++column) {
            globe_texture_info_t info;
            array_t *png = globe_texture_metadata(globe, 4, column, row, &info)
                ? globe_texture_png(globe, 4, column, row, &info) : NULL;
            ok = png != NULL;
            array_destroy(png);
        }
    }
    for (int slot = 0; ok && slot < total - 74 - detail_count; ++slot) {
        globe_texture_info_t info;
        array_t *png = globe_texture_metadata(globe, 3, (unsigned)slot, 0, &info)
            ? globe_texture_png(globe, 3, (unsigned)slot, 0, &info) : NULL;
        ok = png != NULL;
        array_destroy(png);
    }
    globe_close(globe);
    return ok;
}
