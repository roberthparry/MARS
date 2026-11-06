/**
 * @file test_cfg_pyramids.c
 * @brief Transactional source-block import, content verification and immutable cache-identity regressions.
 *
 * Generates small synthetic PNGs in memory and exercises the production globe API against disposable stores.
 * No installed data is opened. Failure injection verifies that incomplete sources cannot become visible.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "array-t.h"
#include "file-t.h"
#include "globe-t.h"
#include "sqlite-t.h"
#include "string-t.h"
#define MARS_CFG_DATABASE_INTERNAL_ACCESS
#include "cfg_database_internal.h"
#include "test_harness.h"
#include "test_cfg_support.h"

static void u32(unsigned char *out, unsigned value)
{
    out[0] = value >> 24; out[1] = value >> 16; out[2] = value >> 8; out[3] = value;
}

static bool chunk(array_t *out, const char *kind, const void *data, size_t size)
{
    unsigned char prefix[8], checksum[4];
    u32(prefix, (unsigned)size); memcpy(prefix + 4, kind, 4);
    uLong crc = crc32(0, (const Bytef *)kind, 4);
    if (size) crc = crc32(crc, data, (uInt)size);
    u32(checksum, (unsigned)crc);
    return array_append_carray(out, prefix, 8) && (!size || array_append_carray(out, data, size)) &&
        array_append_carray(out, checksum, 4);
}

static array_t *produce(unsigned tier, unsigned row, unsigned column, void *context)
{
    unsigned *remaining = context;
    if (!*remaining) return NULL;
    --*remaining;
    size_t size = 514 * (514 * 4 + 1);
    unsigned char *pixels = calloc(size, 1);
    uLongf count = compressBound(size);
    unsigned char *encoded = malloc(count), header[13] = {0};
    for (unsigned y = 0; pixels && y < 514; ++y)
        memset(pixels + y * (514 * 4 + 1) + 1, (tier + row + column) & 255, 514 * 4);
    bool ok = pixels && encoded && compress2(encoded, &count, pixels, size, 1) == Z_OK;
    array_t *out = ok ? array_create(1, NULL, NULL) : NULL;
    u32(header, 514); u32(header + 4, 514); header[8] = 8; header[9] = 6;
    ok = out && array_append_carray(out, "\211PNG\r\n\032\n", 8) && chunk(out, "IHDR", header, 13) &&
        chunk(out, "IDAT", encoded, count) && chunk(out, "IEND", NULL, 0);
    free(pixels); free(encoded);
    if (!ok) { array_destroy(out); out = NULL; }
    return out;
}

static void test_cfg_pyramid_atomic(void)
{
    string_t *path = string_new_with(":memory:");
    sqlite_t *db = sqlite_open_plain(path);
    const char *digest = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    unsigned remaining = 2;
    TEST_ASSERT_TRUE(db && sqlite_exec_cstr(db,
        "CREATE TABLE globe_texture_manifest(id INTEGER PRIMARY KEY,json TEXT);"
        "INSERT INTO globe_texture_manifest VALUES(1,'catalogue fixture')") &&
        !globe_texture_pyramid_build(db, digest, 1024, produce, &remaining),
                     "an interrupted source does not publish a completion marker");
    remaining = 100;
    TEST_ASSERT_TRUE(globe_texture_pyramid_build(db, digest, 1024, produce, &remaining),
                     "a failed source resumes by rebuilding its atomic source transaction");
    globe_t *globe = globe_open_databases(db, db, NULL);
    string_t *before = globe_texture_identity(globe);
    TEST_ASSERT_TRUE(before && sqlite_exec_cstr(db, "CREATE TABLE unrelated(value); INSERT INTO unrelated VALUES(42)"),
                     "the source catalogue supplies a stable image identity independently of file timestamps");
    string_t *after = globe_texture_identity(globe);
    TEST_ASSERT_TRUE(before && after && !string_compare(before, after),
                     "unrelated database writes do not invalidate photographic cells");
    string_free(before); string_free(after);
    globe_texture_info_t info;
    array_t *bytes = globe_texture_block(globe, digest, 1024, 1, 1, &info);
    TEST_ASSERT_TRUE(bytes && info.width == 514 && info.height == 514 && info.pyramid && info.byte_count > 0,
                     "a compound address returns a verified small PNG including its gutter");
    array_destroy(bytes);
    TEST_ASSERT_TRUE(!globe_texture_block(globe, digest, 1024, 2, 0, &info) &&
                     !globe_texture_block(globe, "bad", 1024, 0, 0, &info),
                     "invalid source identities and out-of-range block coordinates are rejected");
    remaining = 0;
    TEST_ASSERT_TRUE(globe_texture_pyramid_build(db, digest, 1024, produce, &remaining),
                     "an already complete immutable source never calls the producer again");
    TEST_ASSERT_TRUE(sqlite_exec_cstr(db, "UPDATE globe_texture_block_payload SET png=zeroblob(length(png))"),
                     "inject corrupt stored payloads");
    TEST_ASSERT_TRUE(!globe_texture_block(globe, digest, 1024, 0, 0, &info),
                     "corrupt blocks are rejected before reaching a browser decoder");
    globe_close(globe); sqlite_close(db); string_free(path);
}

static bool import_fixture(const char *directory)
{
    string_t *path = string_sprintf("%s/blocks.db", directory), *memory = string_new_with(":memory:");
    file_t *file = file_new(path);
    sqlite_t *source = sqlite_open_plain(path), *target = sqlite_open_plain(memory);
    const char *digest = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
    unsigned remaining = 100;
    bool ok = source && target && globe_texture_pyramid_build(source, digest, 1024, produce, &remaining);
    sqlite_close(source);
    ok = ok && globe_texture_pyramid_import(target, file);
    globe_t *globe = globe_open_databases(target, target, NULL);
    globe_texture_info_t info;
    array_t *bytes = ok ? globe_texture_block(globe, digest, 64, 0, 0, &info) : NULL;
    ok = ok && bytes && info.width == 514;
    array_destroy(bytes); globe_close(globe); sqlite_close(target);
    source = sqlite_open_plain(path); target = sqlite_open_plain(memory);
    ok = ok && sqlite_exec_cstr(source, "DELETE FROM globe_texture_block WHERE tier=64");
    sqlite_close(source);
    ok = ok && !globe_texture_pyramid_import(target, file);
    globe = globe_open_databases(target, target, NULL);
    bytes = globe_texture_block(globe, digest, 1024, 0, 0, &info);
    ok = ok && !bytes;
    array_destroy(bytes); globe_close(globe); sqlite_close(target);
    ok = file_delete(file) && ok;
    file_free(file); string_free(path); string_free(memory);
    return ok;
}

static void test_cfg_pyramid_import(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(import_fixture),
                     "complete prepared sources import; missing final blocks roll back every earlier block");
}

static bool installer_fixture(const char *directory)
{
    const char *digest = "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc";
    string_t *folder_path = string_sprintf("%s/pyramid", directory), *root = string_new();
    file_t *folder = file_new(folder_path);
    string_t *path = string_sprintf("%s/%s.db", string_c_str(folder_path), digest);
    string_t *memory = string_new_with(":memory:");
    file_t *file = file_new(path);
    sqlite_t *source = folder && file_create_directory(folder, 0700, true) ? sqlite_open_plain(path) : NULL;
    sqlite_t *target = sqlite_open_plain(memory);
    unsigned remaining = 100;
    bool ok = source && target && globe_texture_pyramid_build(source, digest, 1800, produce, &remaining);
    sqlite_close(source);
    const char *old = getenv("MARS_GLOBE_TEXTURE_DIR");
    string_t *previous = old ? string_new_with(old) : NULL;
    ok = ok && (!old || previous) && !setenv("MARS_GLOBE_TEXTURE_DIR", directory, 1) && sqlite_exec_cstr(target,
        "CREATE TABLE globe_texture_dataset(id,format_version);"
        "INSERT INTO globe_texture_dataset VALUES(1,2);"
        "CREATE TABLE globe_texture(level,column_index,row_index,width,height,byte_count,sha256,png,"
        "PRIMARY KEY(level,column_index,row_index));"
        "INSERT INTO globe_texture VALUES(3,0,0,1800,1800,4,"
        "'cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc',zeroblob(4));"
        "INSERT INTO globe_texture VALUES(3,1,0,1800,1800,4,"
        "'dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd',zeroblob(4));") &&
        cfg_database_pyramids(target, root) && cfg_database_pyramids(target, root);
    globe_t *globe = globe_open_databases(target, target, NULL);
    globe_texture_info_t installed, missing;
    ok = ok && globe_texture_metadata(globe, 3, 0, 0, &installed) && installed.pyramid &&
        globe_texture_metadata(globe, 3, 1, 0, &missing) && !missing.pyramid;
    globe_close(globe); sqlite_close(target);
    ok = (previous ? !setenv("MARS_GLOBE_TEXTURE_DIR", string_c_str(previous), 1) :
                     !unsetenv("MARS_GLOBE_TEXTURE_DIR")) && ok;
    ok = file_delete(file) && file_remove_directory(folder) && ok;
    string_free(previous); file_free(file); file_free(folder); string_free(root);
    string_free(folder_path); string_free(path); string_free(memory);
    return ok;
}

static void test_cfg_pyramid_installer(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(installer_fixture),
                     "release imports matching shards with per-source writer locks, resumes and retains missing originals");
}

/* Register the production storage regressions in the ordinary sequential installer suite. */
void test_cfg_pyramids_cases(void)
{
    TEST_RUN_IN_GROUP(test_cfg_pyramid_atomic, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_pyramid_import, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_pyramid_installer, tests, NULL);
}
