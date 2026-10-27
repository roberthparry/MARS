/**
 * @file test_cfg_textures.c
 * @brief Photographic database installation, indexed reads, corruption rejection and rollback tests.
 *
 * Uses the packaged PNGs and disposable plaintext/encrypted databases. Failed replacement fixtures never
 * modify source assets or installed databases. The documented metadata example runs after ordinary tests.
 */
#include <stdio.h>
#include <string.h>

#include "array-t.h"
#include "globe-t.h"
#include "string-t.h"
#define MARS_CFG_DATABASE_INTERNAL_ACCESS
#include "cfg_database_internal.h"
#include "test_harness.h"
#include "test_cfg_support.h"

static file_t *texture_file(const char *root, const char *relative)
{
    string_t *path = string_sprintf("%s/packaging/globe-db/textures/%s", root, relative);
    file_t *file = path ? file_new(path) : NULL;
    string_free(path);
    return file;
}

bool test_cfg_copy_textures(const char *directory)
{
    file_t *folder = texture_file(directory, "earth_tiles");
    bool ok = folder && file_create_directory(folder, 0700, true);
    file_free(folder);
    folder = texture_file(directory, "regional");
    ok = ok && folder && file_create_directory(folder, 0700, true);
    file_free(folder);
    for (unsigned i = 0; ok && i < 139; ++i) {
        string_t *name = i < 72 ? string_sprintf("earth_tiles/%02u-%02u.png", i % 12, i / 12)
                       : i >= 75 ? string_sprintf("regional/%02u.png", i - 75)
                       : string_new_with(i == 72 ? "earth_surface.png" : i == 73 ? "earth_surface_detail.png" : "manifest.json");
        file_t *source = name ? texture_file(MARS_CONFIG_ROOT_DIR, string_c_str(name)) : NULL;
        if (i >= 75 && source && !file_exists(source)) { file_free(source); string_free(name); break; }
        file_t *target = name ? texture_file(directory, string_c_str(name)) : NULL;
        ok = source && target && file_copy(source, target, false);
        file_free(source); file_free(target); string_free(name);
    }
    return ok;
}

static sqlite_t *texture_database(const char *key)
{
    string_t *path = string_new_with(":memory:"), *secret = string_new_with(key);
    sqlite_t *db = path && secret ? cfg_database_open(path, secret) : NULL;
    string_free(path); string_free(secret);
    return db;
}

static bool texture_region_found(unsigned slot, const double bounds[4], void *context)
{
    unsigned *count = context;
    ++*count;
    return slot < 64 && bounds[0] < bounds[2] && bounds[1] < bounds[3];
}

static void test_cfg_textures_install(void)
{
    string_t *root = string_new_with(MARS_CONFIG_ROOT_DIR);
    for (unsigned encrypted = 0; encrypted < 2; ++encrypted) {
        sqlite_t *db = texture_database(encrypted ? "texture-fixture" : "");
        TEST_ASSERT_TRUE(db && root && sqlite_exec_cstr(db, "create table preserved(id);insert into preserved values(42)") &&
                         cfg_database_textures(db, root) && cfg_database_textures_valid(db),
                         "all packaged images validate in either database storage mode");
        globe_t *globe = globe_open_databases(db, db, NULL);
        globe_texture_info_t info;
        TEST_ASSERT_TRUE(globe_texture_metadata(globe, 0, 0, 0, &info) && info.width == 2048 && info.height == 1024 &&
                         info.byte_count == 2623195, "overview dimensions and encoded length survive installation");
        array_t *png = globe_texture_png(globe, 0, 0, 0, &info);
        TEST_ASSERT_TRUE(png && array_size(png) == info.byte_count, "read unchanged, verified PNG bytes");
        array_destroy(png);
        TEST_ASSERT_TRUE(globe_texture_metadata(globe, 3, 0, 0, &info) && info.width == 1800 &&
                         info.byte_count == 4216742, "regional Sentinel photograph is installed");
        unsigned found = 0;
        const double inside[] = {-3.72, 52.31, -3.71, 52.32}, outside[] = {0, 0, 1, 1};
        TEST_ASSERT_TRUE(globe_texture_regions(globe, inside, texture_region_found, &found) && found >= 2,
                         "regional coverage includes detailed Welsh imagery and an overview");
        unsigned previous = found;
        TEST_ASSERT_TRUE(globe_texture_regions(globe, outside, texture_region_found, &found) && found == previous,
                         "regional coverage selects Wales and leaves other places unchanged");
        TEST_ASSERT_TRUE(globe_texture_metadata(globe, 3, 1, 0, &info) && info.width == 3600 && info.height == 3600,
                         "larger regional imagery retains its validated dimensions");
        static const double places[][2] = {
            {-4.63, 53.31}, {-5.27, 51.88}, {-3.18, 51.48}, {-2.71, 51.81}, {-4.08, 53.06},
        };
        for (size_t i = 0; i < sizeof places / sizeof *places; ++i) {
            double window[] = {places[i][0] - .001, places[i][1] - .001, places[i][0] + .001, places[i][1] + .001};
            found = 0;
            TEST_ASSERT_TRUE(globe_texture_regions(globe, window, texture_region_found, &found) && found >= 2,
                             "Anglesey, western, southern, eastern and mountain views have overview and detail coverage");
        }
        TEST_ASSERT_TRUE(!globe_texture_metadata(globe, 4, 0, 0, &info) && !info.width &&
                         !globe_texture_metadata(globe, 2, 12, 0, &info) &&
                         !globe_texture_metadata(globe, 2, 0, 6, &info) &&
                         !globe_texture_metadata(globe, 0, 1, 0, &info), "reject invalid texture addresses");
        TEST_ASSERT_TRUE(globe_texture_metadata(globe, 2, 11, 5, &info), "last tile is installed");
        globe_texture_info_t stale = info;
        stale.sha256[0] = stale.sha256[0] == '0' ? '1' : '0';
        TEST_ASSERT_TRUE(!globe_texture_png(globe, 2, 11, 5, &stale), "reject a stale requested identity");
        TEST_ASSERT_TRUE(sqlite_exec_cstr(db, "update globe_texture set png=zeroblob(byte_count) "
                                             "where level=2 and column_index=11 and row_index=5") &&
                         !globe_texture_png(globe, 2, 11, 5, &info) && !cfg_database_textures_valid(db),
                         "same-size damaged data cannot pass content validation");
        TEST_ASSERT_TRUE(cfg_database_textures(db, root) && cfg_database_textures_valid(db),
                         "reimport repairs only texture tables");
        sqlite_stmt_t *q = sqlite_stmt_prepare(db, "select id from preserved");
        TEST_ASSERT_TRUE(q && sqlite_stmt_step(q) == SQLITE_STEP_ROW && sqlite_stmt_column_int(q, 0) == 42,
                         "other database layers survive replacement");
        sqlite_stmt_finalize(q); globe_close(globe); sqlite_close(db);
    }
    string_free(root);
}

static bool texture_rollback_fixture(const char *directory)
{
    string_t *root = string_new_with(MARS_CONFIG_ROOT_DIR), *bad_root = string_new_with(directory);
    sqlite_t *db = texture_database("");
    bool ok = root && bad_root && db && cfg_database_textures(db, root) && test_cfg_copy_textures(directory);
    file_t *last = texture_file(directory, "earth_tiles/11-05.png");
    ok = ok && last && file_delete(last) && !cfg_database_textures(db, bad_root) && cfg_database_textures_valid(db);
    string_t *broken = string_new_with("not a PNG");
    ok = ok && broken && file_write_all_text(last, broken) && !cfg_database_textures(db, bad_root) &&
         cfg_database_textures_valid(db);
    file_free(last); string_free(broken); string_free(root); string_free(bad_root); sqlite_close(db);
    return ok;
}

static void test_cfg_textures_rollback(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(texture_rollback_fixture),
                     "missing or corrupt final tiles roll back the entire replacement");
}

static bool texture_migration_fixture(const char *directory)
{
    sqlite_t *db = texture_database("");
    string_t *root = string_new_with(MARS_CONFIG_ROOT_DIR), *staging = string_new_with(directory);
    bool ok = db && root && staging && sqlite_exec_cstr(db,
        "create table shoreline_dataset(id,format_version,point_count);insert into shoreline_dataset values(1,2,7);"
        "create table shoreline_blob(id,vertices);"
        "create table shoreline_chunk(id,level,codec,point_count,byte_offset,byte_count,boundary,blob_id);"
        "create virtual table shoreline_bounds using rtree(id,min_x,max_x,min_y,max_y);"
        "create table rivers_dataset(id,source);insert into rivers_dataset values(1,'retained');") &&
        cfg_database_revision(db, "globe", 5) && cfg_database_globe_refresh(db, root, staging) &&
        cfg_database_textures_valid(db);
    sqlite_stmt_t *q = sqlite_stmt_prepare(db, "select source from rivers_dataset");
    ok = ok && q && sqlite_stmt_step(q) == SQLITE_STEP_ROW && !strcmp(sqlite_stmt_column_text(q, 0), "retained");
    sqlite_stmt_finalize(q);
    ok = ok && cfg_database_revision(db, "globe", 9) &&
         sqlite_exec_cstr(db, "delete from globe_texture where level=3;drop table globe_texture_region") &&
         cfg_database_globe_refresh(db, root, staging) && cfg_database_textures_valid(db);
    q = sqlite_stmt_prepare(db, "select source from rivers_dataset");
    ok = ok && q && sqlite_stmt_step(q) == SQLITE_STEP_ROW && !strcmp(sqlite_stmt_column_text(q, 0), "retained");
    sqlite_stmt_finalize(q);
    ok = ok && cfg_database_revision(db, "globe", 10) &&
         sqlite_exec_cstr(db, "delete from globe_texture where level=3 and column_index>0") &&
         cfg_database_globe_refresh(db, root, staging) && cfg_database_textures_valid(db);
    ok = ok && cfg_database_revision(db, "globe", 11) &&
         sqlite_exec_cstr(db, "alter table globe_texture rename column png to jpeg;"
                             "update globe_texture_dataset set format_version=1") &&
         cfg_database_globe_refresh(db, root, staging) && cfg_database_textures_valid(db);
    q = sqlite_stmt_prepare(db, "select source from rivers_dataset");
    ok = ok && q && sqlite_stmt_step(q) == SQLITE_STEP_ROW && !strcmp(sqlite_stmt_column_text(q, 0), "retained");
    sqlite_stmt_finalize(q);
    globe_t *globe = ok ? globe_open_databases(db, db, NULL) : NULL;
    globe_texture_info_t info;
    ok = ok && globe_texture_metadata(globe, 3, 1, 0, &info) && info.width == 3600;
    globe_close(globe); string_free(staging); string_free(root); sqlite_close(db);
    return ok;
}

static void test_cfg_textures_migration(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(texture_migration_fixture),
                     "revision-five migration updates countries and textures while preserving shoreline and rivers");
}

static void example_globe_texture_metadata(void)
{
    /* README example: query one image and release its owned bytes after use. */
    sqlite_t *db = texture_database("");
    string_t *root = string_new_with(MARS_CONFIG_ROOT_DIR);
    TEST_ASSERT_TRUE(db && root && cfg_database_textures(db, root), "prepare documented image fixture");
    globe_t *globe = globe_open_databases(db, db, NULL);
    globe_texture_info_t info;
    TEST_ASSERT_TRUE(globe_texture_metadata(globe, 0, 0, 0, &info), "README texture metadata");
    array_t *png = globe_texture_png(globe, 0, 0, 0, &info);
    TEST_ASSERT_TRUE(png && info.width == 2048 && info.height == 1024 && array_size(png) == 2623195,
                     "README output matches the packaged overview");
    printf("%u x %u; %zu bytes\n", info.width, info.height, array_size(png));
    array_destroy(png); globe_close(globe); string_free(root); sqlite_close(db);
}

void test_cfg_textures_cases(void)
{
    TEST_RUN_IN_GROUP(test_cfg_textures_install, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_textures_rollback, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_textures_migration, tests, NULL);
}

void test_cfg_textures_readme_cases(void)
{
    TEST_RUN_IN_GROUP(example_globe_texture_metadata, readme_examples, NULL);
}
