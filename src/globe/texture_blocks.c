/**
 * @file texture_blocks.c
 * @brief Transactional, content-addressed photographic source pyramids and direct block reads.
 *
 * Offline producers supply small PNGs; this module validates and publishes a complete source atomically.
 * Compact tuple metadata stays separate from deduplicated image payloads. Readers require completion markers
 * and verify content without decoding photographs. PNG validation and tier geometry belong to the image module;
 * no browser or graphics API is required here.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sodium.h>

#include "array-t.h"
#include "image-t.h"
#define MARS_GLOBE_INTERNAL_ACCESS
#include "globe_layers.h"

static bool digest_valid(const char *text)
{
    if (!text || strlen(text) != 64) return false;
    for (unsigned i = 0; i < 64; ++i)
        if (!((text[i] >= '0' && text[i] <= '9') || (text[i] >= 'a' && text[i] <= 'f'))) return false;
    return true;
}

static unsigned block_count(unsigned width)
{
    image_pyramid_t *pyramid = image_pyramid_new(width, width, 512, 1, 64);
    unsigned total = 0;
    for (unsigned i = 0; i < image_pyramid_levels(pyramid); ++i) {
        image_level_t level;
        if (image_pyramid_level(pyramid, i, &level)) total += level.rows * level.columns;
    }
    image_pyramid_free(pyramid);
    return total;
}

static bool has_table(sqlite_t *db, const char *name)
{
    sqlite_stmt_t *q = db ? sqlite_stmt_prepare(db,
        "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ?1") : NULL;
    bool found = q && sqlite_stmt_bind_text(q, 1, name) && sqlite_stmt_step(q) == SQLITE_STEP_ROW;
    sqlite_stmt_finalize(q);
    return found;
}

static bool schema(sqlite_t *db)
{
    return db && sqlite_exec_cstr(db,
        "CREATE TABLE IF NOT EXISTS globe_texture_block_source (\n"
        "    sha256 TEXT PRIMARY KEY, width INTEGER NOT NULL, format_version INTEGER NOT NULL,\n"
        "    block_count INTEGER NOT NULL\n"
        ") WITHOUT ROWID;\n"
        "CREATE TABLE IF NOT EXISTS globe_texture_block (\n"
        "    source_sha256 TEXT NOT NULL, tier INTEGER NOT NULL, row_index INTEGER NOT NULL,\n"
        "    column_index INTEGER NOT NULL, sha256 TEXT NOT NULL, byte_count INTEGER NOT NULL,\n"
        "    PRIMARY KEY (source_sha256, tier, row_index, column_index)\n"
        ") WITHOUT ROWID;\n"
        "CREATE TABLE IF NOT EXISTS globe_texture_block_payload (\n"
        "    sha256 TEXT PRIMARY KEY, png BLOB NOT NULL\n"
        ");\n"
        "CREATE TABLE IF NOT EXISTS globe_texture_generation (\n"
        "    id INTEGER PRIMARY KEY CHECK (id = 1), sha256 TEXT NOT NULL\n"
        ");");
}

static bool catalogue_identity(sqlite_t *db)
{
    if (!has_table(db, "globe_texture_manifest")) return true;
    sqlite_stmt_t *present = sqlite_stmt_prepare(db, "SELECT 1 FROM globe_texture_generation WHERE id = 1");
    bool exists = present && sqlite_stmt_step(present) == SQLITE_STEP_ROW;
    sqlite_stmt_finalize(present);
    if (exists) return true;
    sqlite_stmt_t *q = sqlite_stmt_prepare(db, "SELECT json FROM globe_texture_manifest WHERE id = 1");
    bool ok = q && sqlite_stmt_step(q) == SQLITE_STEP_ROW && sqlite_stmt_column_bytes(q, 0) > 0 &&
        sqlite_stmt_column_bytes(q, 0) <= GLOBE_TEXTURE_MANIFEST_LIMIT;
    unsigned char digest[crypto_hash_sha256_BYTES];
    char hex[65];
    if (ok) ok = crypto_hash_sha256(digest, sqlite_stmt_column_blob(q, 0), sqlite_stmt_column_bytes(q, 0)) == 0 &&
        sodium_bin2hex(hex, sizeof hex, digest, sizeof digest);
    sqlite_stmt_finalize(q);
    q = ok ? sqlite_stmt_prepare(db,
        "INSERT INTO globe_texture_generation VALUES (1, ?1)\n"
        "ON CONFLICT (id) DO UPDATE SET sha256 = excluded.sha256") : NULL;
    ok = q && sqlite_stmt_bind_text(q, 1, hex) && sqlite_stmt_step(q) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(q);
    return ok;
}

/* Read only immutable source metadata; a marker is written after all block validation succeeds. */
bool textures_pyramid_ready(sqlite_t *db, const char *sha256, unsigned width)
{
    if (!db || !digest_valid(sha256) || width < 64 || width > 8192 ||
        !has_table(db, "globe_texture_block_source")) return false;
    unsigned expected = block_count(width);
    if (!expected) return false;
    sqlite_stmt_t *q = sqlite_stmt_prepare(db,
        "SELECT 1 FROM globe_texture_block_source\n"
        "WHERE sha256 = ?1 AND width = ?2 AND format_version = 1 AND block_count = ?3");
    bool ready = q && sqlite_stmt_bind_text(q, 1, sha256) && sqlite_stmt_bind_int(q, 2, width) &&
        sqlite_stmt_bind_int(q, 3, expected) && sqlite_stmt_step(q) == SQLITE_STEP_ROW;
    sqlite_stmt_finalize(q);
    return ready;
}

static bool block_png(const unsigned char *p, size_t size)
{
    unsigned width, height;
    return size <= 2 * 1024 * 1024 && image_png_header(p, size, &width, &height) &&
           width == 514 && height == 514 && image_png_validate(p, size);
}

static bool store_block(sqlite_t *db, const char *source, unsigned tier, unsigned row, unsigned column,
                        const array_t *bytes)
{
    const unsigned char *data = array_size(bytes) ? array_get(bytes, 0) : NULL;
    size_t count = array_size(bytes);
    unsigned char digest[crypto_hash_sha256_BYTES];
    char hex[65];
    bool ok = block_png(data, count) && crypto_hash_sha256(digest, data, count) == 0 &&
        sodium_bin2hex(hex, sizeof hex, digest, sizeof digest);
    sqlite_stmt_t *q = ok ? sqlite_stmt_prepare(db,
        "INSERT OR IGNORE INTO globe_texture_block_payload (sha256, png) VALUES (?1, ?2)") : NULL;
    ok = q && sqlite_stmt_bind_text(q, 1, hex) && sqlite_stmt_bind_blob(q, 2, data, count) &&
        sqlite_stmt_step(q) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(q);
    q = ok ? sqlite_stmt_prepare(db,
        "INSERT INTO globe_texture_block\n"
        "    (source_sha256, tier, row_index, column_index, sha256, byte_count)\n"
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6)") : NULL;
    ok = q && sqlite_stmt_bind_text(q, 1, source) && sqlite_stmt_bind_int(q, 2, tier) &&
        sqlite_stmt_bind_int(q, 3, row) && sqlite_stmt_bind_int(q, 4, column) &&
        sqlite_stmt_bind_text(q, 5, hex) && sqlite_stmt_bind_int64(q, 6, count) &&
        sqlite_stmt_step(q) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(q);
    return ok;
}

/* Publish only complete, checksum-verified pyramids and keep an existing immutable source unchanged. */
bool globe_texture_pyramid_build(sqlite_t *db, const char *sha256, unsigned width,
                                 globe_texture_block_producer_fn produce, void *context)
{
    if (!db || !digest_valid(sha256) || width < 64 || width > 8192 || !produce) return false;
    if (textures_pyramid_ready(db, sha256, width)) return true;
    image_pyramid_t *pyramid = image_pyramid_new(width, width, 512, 1, 64);
    if (!pyramid) return false;
    unsigned expected = 0;
    bool transaction = sqlite_exec_cstr(db, "SAVEPOINT globe_texture_pyramid");
    bool ok = transaction && schema(db);
    sqlite_stmt_t *q = ok ? sqlite_stmt_prepare(db,
        "DELETE FROM globe_texture_block WHERE source_sha256 = ?1") : NULL;
    ok = q && sqlite_stmt_bind_text(q, 1, sha256) && sqlite_stmt_step(q) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(q);
    for (unsigned i = 0; ok && i < image_pyramid_levels(pyramid); ++i) {
        image_level_t level;
        ok = image_pyramid_level(pyramid, i, &level);
        if (ok) expected += level.rows * level.columns;
        for (unsigned row = 0; ok && row < level.rows; ++row) {
            for (unsigned column = 0; ok && column < level.columns; ++column) {
                array_t *bytes = produce(level.edge, row, column, context);
                ok = bytes && store_block(db, sha256, level.edge, row, column, bytes);
                array_destroy(bytes);
            }
        }
    }
    image_pyramid_free(pyramid);
    q = ok ? sqlite_stmt_prepare(db,
        "INSERT INTO globe_texture_block_source VALUES (?1, ?2, 1, ?3)\n"
        "ON CONFLICT (sha256) DO UPDATE SET width = excluded.width,\n"
        "    format_version = 1, block_count = excluded.block_count") : NULL;
    ok = q && sqlite_stmt_bind_text(q, 1, sha256) && sqlite_stmt_bind_int(q, 2, width) &&
        sqlite_stmt_bind_int(q, 3, expected) && sqlite_stmt_step(q) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(q);
    ok = ok && catalogue_identity(db);
    if (ok) ok = sqlite_exec_cstr(db, "RELEASE globe_texture_pyramid");
    if (!ok && transaction) sqlite_exec_cstr(db,
        "ROLLBACK TO globe_texture_pyramid; RELEASE globe_texture_pyramid");
    return ok;
}

typedef struct {
    sqlite_t *database;
    const char *source;
} block_import_t;

static array_t *import_block(unsigned tier, unsigned row, unsigned column, void *context)
{
    block_import_t *state = context;
    sqlite_stmt_t *q = sqlite_stmt_prepare(state->database,
        "SELECT p.png, i.byte_count, i.sha256\n"
        "FROM globe_texture_block AS i\n"
        "JOIN globe_texture_block_payload AS p ON p.sha256 = i.sha256\n"
        "WHERE i.source_sha256 = ?1 AND i.tier = ?2 AND i.row_index = ?3 AND i.column_index = ?4");
    bool ok = q && sqlite_stmt_bind_text(q, 1, state->source) && sqlite_stmt_bind_int(q, 2, tier) &&
        sqlite_stmt_bind_int(q, 3, row) && sqlite_stmt_bind_int(q, 4, column) &&
        sqlite_stmt_step(q) == SQLITE_STEP_ROW;
    size_t size = ok ? sqlite_stmt_column_bytes(q, 0) : 0;
    const unsigned char *data = ok ? sqlite_stmt_column_blob(q, 0) : NULL;
    const char *expected = ok ? sqlite_stmt_column_text(q, 2) : NULL;
    unsigned char digest[crypto_hash_sha256_BYTES];
    char hex[65];
    ok = size >= 57 && size <= 2 * 1024 * 1024 && sqlite_stmt_column_int64(q, 1) == (int64_t)size &&
        digest_valid(expected) && data && crypto_hash_sha256(digest, data, size) == 0 &&
        sodium_bin2hex(hex, sizeof hex, digest, sizeof digest) && !strcmp(hex, expected);
    array_t *bytes = ok ? array_create(1, NULL, NULL) : NULL;
    if (!bytes || !array_append_carray(bytes, data, size)) { array_destroy(bytes); bytes = NULL; }
    sqlite_stmt_finalize(q);
    return bytes;
}

/* Verify preparation records as they stream into independent source savepoints. */
bool globe_texture_pyramid_import(sqlite_t *db, file_t *source)
{
    const char *name = source ? file_path(source) : NULL;
    string_t *path = name ? string_new_with(name) : NULL;
    sqlite_t *input = path && file_exists(source) ? sqlite_open_plain(path) : NULL;
    bool ok = input && sqlite_set_cache_budget(input, NULL, 512) &&
        sqlite_exec_cstr(input, "PRAGMA query_only=ON;");
    sqlite_stmt_t *q = ok ? sqlite_stmt_prepare(input,
        "SELECT sha256, width, format_version, block_count FROM globe_texture_block_source ORDER BY sha256") : NULL;
    sqlite_step_result_t step = SQLITE_STEP_ERROR;
    unsigned count = 0;
    while (q && ok && (step = sqlite_stmt_step(q)) == SQLITE_STEP_ROW) {
        const char *sha256 = sqlite_stmt_column_text(q, 0);
        int width = sqlite_stmt_column_int(q, 1);
        ok = ++count <= GLOBE_TEXTURE_REGION_LIMIT + 1226 && digest_valid(sha256) && width >= 64 && width <= 8192 &&
            sqlite_stmt_column_int(q, 2) == 1 && sqlite_stmt_column_int(q, 3) == (int)block_count((unsigned)width);
        block_import_t state = {.database = input, .source = sha256};
        if (ok) ok = globe_texture_pyramid_build(db, sha256, (unsigned)width, import_block, &state);
    }
    ok = ok && q && step == SQLITE_STEP_DONE && count > 0;
    sqlite_stmt_finalize(q); sqlite_close(input); string_free(path);
    return ok;
}

/* A single compound-key lookup resolves both the compact metadata and its immutable PNG payload. */
array_t *textures_block(sqlite_t *db, const char *source, unsigned tier, unsigned row, unsigned column,
                        globe_texture_info_t *info)
{
    if (info) *info = (globe_texture_info_t){0};
    if (!db || !info || !digest_valid(source) || tier < 64 || tier > 8192 ||
        row >= (tier + 511) / 512 || column >= (tier + 511) / 512) return NULL;
    sqlite_stmt_t *q = sqlite_stmt_prepare(db,
        "SELECT p.png, i.byte_count, i.sha256\n"
        "FROM globe_texture_block AS i\n"
        "JOIN globe_texture_block_payload AS p ON p.sha256 = i.sha256\n"
        "JOIN globe_texture_block_source AS s ON s.sha256 = i.source_sha256 AND s.format_version = 1\n"
        "WHERE i.source_sha256 = ?1 AND i.tier = ?2 AND i.row_index = ?3 AND i.column_index = ?4");
    bool ok = q && sqlite_stmt_bind_text(q, 1, source) && sqlite_stmt_bind_int(q, 2, tier) &&
        sqlite_stmt_bind_int(q, 3, row) && sqlite_stmt_bind_int(q, 4, column) &&
        sqlite_stmt_step(q) == SQLITE_STEP_ROW;
    size_t size = ok ? sqlite_stmt_column_bytes(q, 0) : 0;
    const unsigned char *data = ok ? sqlite_stmt_column_blob(q, 0) : NULL;
    const char *expected = ok ? sqlite_stmt_column_text(q, 2) : NULL;
    unsigned char digest[crypto_hash_sha256_BYTES];
    char hex[65];
    ok = size >= 57 && size <= 2 * 1024 * 1024 && sqlite_stmt_column_int64(q, 1) == (int64_t)size &&
        digest_valid(expected) && block_png(data, size) && crypto_hash_sha256(digest, data, size) == 0 &&
        sodium_bin2hex(hex, sizeof hex, digest, sizeof digest) && !strcmp(hex, expected);
    array_t *bytes = ok ? array_create(1, NULL, NULL) : NULL;
    ok = bytes && array_append_carray(bytes, data, size);
    if (ok) {
        info->width = info->height = 514; info->byte_count = size; info->pyramid = true;
        memcpy(info->sha256, hex, sizeof info->sha256);
    }
    sqlite_stmt_finalize(q);
    if (!ok) { array_destroy(bytes); bytes = NULL; }
    return bytes;
}

/* Catalogue identities exclude mutable preparation progress and unrelated geographic layers. */
string_t *textures_identity(sqlite_t *db)
{
    if (!has_table(db, "globe_texture_generation")) return NULL;
    sqlite_stmt_t *q = sqlite_stmt_prepare(db, "SELECT sha256 FROM globe_texture_generation WHERE id = 1");
    const char *text = q && sqlite_stmt_step(q) == SQLITE_STEP_ROW ? sqlite_stmt_column_text(q, 0) : NULL;
    string_t *identity = digest_valid(text) ? string_new_with(text) : NULL;
    sqlite_stmt_finalize(q);
    return identity;
}
