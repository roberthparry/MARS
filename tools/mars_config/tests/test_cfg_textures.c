/**
 * @file test_cfg_textures.c
 * @brief Photographic database installation, indexed reads, corruption rejection and rollback tests.
 *
 * Uses compact PNG fixtures and disposable plaintext/encrypted databases. The README case alone loads the real
 * packaged overview. Failed replacement fixtures never
 * modify source assets or installed databases. Captured progress verifies the full global and regional image
 * count. The documented metadata example runs after ordinary tests.
 */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
/* Keep the POSIX signal-stack typedef distinct from the MARS stack API. */
#define stack_t posix_signal_stack_t
#include <sys/wait.h>
#undef stack_t
#include <unistd.h>

#include "array-t.h"
#include "globe-t.h"
#include "json-t.h"
#include "string-t.h"
#include "cfg_database.h"
#define MARS_CFG_DATABASE_INTERNAL_ACCESS
#include "cfg_database_internal.h"
#include "test_harness.h"
#include "test_cfg_support.h"

static file_t *texture_file(const char *root, const char *relative)
{
    if (!root) return NULL;
    string_t *path = string_sprintf("%s/packaging/globe-db/textures/%s", root, relative);
    file_t *file = path ? file_new(path) : NULL;
    string_free(path);
    return file;
}

static string_t *texture_fixture_root(void)
{
    const char *root = test_cfg_texture_root();
    return root ? string_new_with(root) : NULL;
}

static bool texture_fixture_sources(void)
{
    const char *root = test_cfg_texture_root();
    string_t *sources = root ? string_sprintf("%s/packaging/globe-db", root) : NULL;
    bool ok = sources && !setenv("MARS_GLOBE_SOURCE_DIR", string_c_str(sources), 1);
    string_free(sources);
    return ok;
}

static size_t texture_source_size(const char *relative)
{
    file_t *file = texture_file(test_cfg_texture_root(), relative);
    file_info_t *info = file ? file_get_info(file) : NULL;
    size_t size = info ? file_info_size(info) : 0;
    file_info_free(info);
    file_free(file);
    return size;
}

static sqlite_t *texture_database(const char *key, file_t **directory)
{
    const char *temporary = getenv("TMPDIR");
    string_t *parent = temporary && *temporary ? string_new_with(temporary) :
                                               string_sprintf("%s/build", MARS_CONFIG_ROOT_DIR);
    *directory = file_create_temp_directory(parent);
    string_t *path = *directory ? string_sprintf("%s/textures.db", file_path(*directory)) : NULL;
    string_t *secret = string_new_with(key);
    sqlite_t *db = path && secret ? cfg_database_open(path, secret) : NULL;
    if (db && !sqlite_exec_cstr(db, "pragma cache_size=-8192;pragma temp_store=file")) {
        sqlite_close(db);
        db = NULL;
    }
    string_free(parent); string_free(path); string_free(secret);
    return db;
}

static void texture_database_close(sqlite_t *db, file_t *directory)
{
    sqlite_close(db);
    if (!directory) return;
    /* Only known files in this fixture's private directory; no shared database is touched. */
    static const char *suffixes[] = {"", "-journal", "-wal", "-shm"};
    bool ok = true;
    for (size_t i = 0; i < sizeof suffixes / sizeof *suffixes; ++i) {
        string_t *path = string_sprintf("%s/textures.db%s", file_path(directory), suffixes[i]);
        file_t *file = file_new(path);
        ok = file && (!file_exists(file) || file_delete(file)) && ok;
        file_free(file); string_free(path);
    }
    TEST_ASSERT_TRUE(ok && file_remove_directory(directory), "remove private disk-backed texture fixture");
    file_free(directory);
}

static bool texture_region_found(unsigned slot, const double bounds[4], void *context)
{
    unsigned *count = context;
    ++*count;
    return slot < GLOBE_TEXTURE_REGION_LIMIT && bounds[0] < bounds[2] && bounds[1] < bounds[3];
}

/* Capture only this disposable fixture's reporter; image insertion and migration still use production code. */
static bool texture_import_progress(sqlite_t *db, const string_t *root, const file_t *directory)
{
    string_t *path = directory ? string_sprintf("%s/import-progress.txt", file_path(directory)) : NULL;
    file_t *capture = path ? file_new(path) : NULL;
    bool ok = capture && file_open(capture, FILE_MODE_CREATE_NEW, FILE_ACCESS_WRITE) && file_close(capture);
    int descriptor = ok ? open(file_path(capture), O_WRONLY | O_NOFOLLOW) : -1;
    int saved = descriptor >= 0 ? dup(STDERR_FILENO) : -1;
    fflush(stderr);
    bool redirected = saved >= 0 && dup2(descriptor, STDERR_FILENO) >= 0;
    bool imported = redirected && cfg_database_textures(db, root);
    fflush(stderr);
    bool restored = !redirected || dup2(saved, STDERR_FILENO) >= 0;
    if (saved >= 0) close(saved);
    if (descriptor >= 0) close(descriptor);
    string_t *output = imported && restored ? file_read_all_text(capture) : NULL;
    ok = output && string_find(output, "0/202 tiles imported 0% — working") >= 0 &&
         string_find(output, "Finalising photographic globe textures 202/202 tiles imported — done") >= 0;
    if (!ok && output) string_fprintf(stderr, "Texture progress transcript: %S\n", output);
    ok = capture && (!file_exists(capture) || file_delete(capture)) && ok;
    file_free(capture); string_free(path); string_free(output);
    return ok;
}

static void test_cfg_textures_manifest(void)
{
    file_t *fixture = NULL;
    sqlite_t *db = texture_database("", &fixture);
    string_t *root = texture_fixture_root();
    file_t *source = texture_file(test_cfg_texture_root(), "manifest.json");
    string_t *expected = file_read_all_text(source);
    TEST_ASSERT_TRUE(db && expected && cfg_database_revision(db, "globe", 13) &&
                     sqlite_exec_cstr(db, "create table retained(value);insert into retained values(42)") &&
                     texture_import_progress(db, root, fixture),
                     "photographic installation counts all 202 global/regional images");
    sqlite_stmt_t *query = sqlite_stmt_prepare(db, "select json from globe_texture_manifest where id=1");
    TEST_ASSERT_TRUE(query && sqlite_stmt_step(query) == SQLITE_STEP_ROW &&
                     !strcmp(sqlite_stmt_column_text(query, 0), string_c_str(expected)),
                     "complete original manifest survives byte for byte, including names and processing provenance");
    sqlite_stmt_finalize(query);
    TEST_ASSERT_TRUE(sqlite_exec_cstr(db, "savepoint manifest_rollback;delete from globe_texture_manifest;"
                                        "rollback to manifest_rollback;release manifest_rollback"),
                     "manifest participates in database transactions");
    query = sqlite_stmt_prepare(db, "select value from retained");
    TEST_ASSERT_TRUE(query && sqlite_stmt_step(query) == SQLITE_STEP_ROW && sqlite_stmt_column_int(query, 0) == 42,
                     "manifest upgrade preserves unrelated tables");
    sqlite_stmt_finalize(query);
    file_free(source); string_free(expected); string_free(root); texture_database_close(db, fixture);
}

static void test_cfg_textures_capacity(void)
{
    file_t *directory = NULL;
    sqlite_t *db = texture_database("", &directory);
    string_t *root = texture_fixture_root();
    bool ok = db && root && cfg_database_textures(db, root);
    string_t *sql = string_sprintf(
        "INSERT INTO globe_texture_region SELECT %u,west,south,east,north,attribution,source_url "
        "FROM globe_texture_region WHERE slot=0;\n"
        "INSERT INTO globe_texture_region_index SELECT %u,west,east,south,north "
        "FROM globe_texture_region WHERE slot=0;\n"
        "INSERT INTO globe_texture SELECT level,%u,row_index,width,height,byte_count,sha256,png "
        "FROM globe_texture WHERE level=3 AND column_index=0 AND row_index=0;",
        (unsigned)GLOBE_TEXTURE_REGION_LIMIT - 1, (unsigned)GLOBE_TEXTURE_REGION_LIMIT - 1,
        (unsigned)GLOBE_TEXTURE_REGION_LIMIT - 1);
    ok = ok && sql && sqlite_exec(db, sql);
    globe_t *globe = ok ? globe_open(db, db) : NULL;
    globe_texture_info_t info;
    ok = ok && globe && globe_texture_metadata(globe, 3, GLOBE_TEXTURE_REGION_LIMIT - 1, 0, &info) &&
         !globe_texture_metadata(globe, 3, GLOBE_TEXTURE_REGION_LIMIT, 0, &info);
    string_t *invalid = string_sprintf(
        "INSERT INTO globe_texture_region SELECT %u,west,south,east,north,attribution,source_url "
        "FROM globe_texture_region WHERE slot=0", (unsigned)GLOBE_TEXTURE_REGION_LIMIT);
    ok = ok && invalid && !sqlite_exec(db, invalid);
    globe_close(globe); string_free(sql); string_free(invalid); string_free(root); texture_database_close(db, directory);
    TEST_ASSERT_TRUE(ok, "expanded regional capacity reaches its final slot and rejects the next address");
}

static bool texture_global_fixture(const char *directory)
{
    (void)directory;
    string_t *root = texture_fixture_root();
    file_t *manifest_file = texture_file(test_cfg_texture_root(), "manifest.json");
    string_t *original = manifest_file ? file_read_all_text(manifest_file) : NULL;
    json_t *manifest = original ? json_from_text(original) : NULL;
    string_t *assets_key = string_new_with("assets"), *detail_key = string_new_with("global_detail");
    json_t *assets = assets_key ? json_clone(json_object_get(manifest, assets_key)) : NULL;
    string_t *layout = string_new_with("{\"width\":86400,\"height\":43200,\"columns\":48,\"rows\":24,\"tile_size\":1800}");
    json_t *detail = json_from_text(layout);
    file_t *pattern = texture_file(test_cfg_texture_root(), "pattern-1800.png");
    file_t *folder = texture_file(test_cfg_texture_root(), "earth_tiles_detail");
    string_t *pattern_key = string_new_with("earth_tiles/00-00.png");
    const json_t *description = pattern_key ? json_object_get(assets, pattern_key) : NULL;
    file_t *database_directory = NULL;
    sqlite_t *db = texture_database("", &database_directory);
    bool ok = root && original && manifest && assets && detail && pattern && folder && description && db &&
              file_create_directory(folder, 0700, true) && cfg_database_textures(db, root) &&
              cfg_database_textures_current(db, root) && cfg_database_revision(db, "globe", CFG_GLOBE_TEXTURE_PATCH_BASE);
    for (unsigned row = 0; ok && row < 24; ++row) {
        for (unsigned column = 0; ok && column < 48; ++column) {
            string_t *relative = string_sprintf("earth_tiles_detail/%02u-%02u.png", column, row);
            file_t *target = relative ? texture_file(test_cfg_texture_root(), string_c_str(relative)) : NULL;
            ok = target && file_create_hard_link(pattern, target) && json_object_set(assets, relative, description);
            file_free(target); string_free(relative);
        }
    }
    ok = ok && json_object_set(manifest, assets_key, assets) && json_object_set(manifest, detail_key, detail);
    string_t *updated = ok ? json_to_string_pretty(manifest, 2) : NULL;
    ok = updated && file_write_all_text(manifest_file, updated) && !cfg_database_textures_current(db, root) &&
         cfg_database_textures(db, root) && cfg_database_textures_current(db, root) && cfg_database_textures_valid(db);
    globe_t *globe = ok ? globe_open_databases(db, db, NULL) : NULL;
    globe_texture_info_t info;
    array_t *bytes = globe && globe_texture_metadata(globe, 4, 47, 23, &info) ?
        globe_texture_png(globe, 4, 47, 23, &info) : NULL;
    ok = ok && bytes && info.width == 1800 && info.height == 1800 &&
         !globe_texture_metadata(globe, 4, 48, 0, &info) && !globe_texture_metadata(globe, 4, 0, 24, &info);
    array_destroy(bytes); globe_close(globe);
    file_t *last = texture_file(test_cfg_texture_root(), "earth_tiles_detail/47-23.png");
    ok = ok && last && file_delete(last) && !cfg_database_textures(db, root) && cfg_database_textures_valid(db);
    bool restored = original && file_write_all_text(manifest_file, original);
    ok = restored && cfg_database_textures(db, root) && cfg_database_textures_valid(db) && ok;
    globe = globe_open_databases(db, db, NULL);
    ok = !globe_texture_metadata(globe, 4, 0, 0, &info) && ok;
    globe_close(globe);
    for (unsigned row = 0; row < 24; ++row) {
        for (unsigned column = 0; column < 48; ++column) {
            string_t *relative = string_sprintf("earth_tiles_detail/%02u-%02u.png", column, row);
            file_t *target = relative ? texture_file(test_cfg_texture_root(), string_c_str(relative)) : NULL;
            ok = target && (!file_exists(target) || file_delete(target)) && ok;
            file_free(target); string_free(relative);
        }
    }
    ok = folder && file_remove_directory(folder) && ok;
    texture_database_close(db, database_directory);
    file_free(last); file_free(pattern); file_free(folder); file_free(manifest_file);
    string_free(root); string_free(original); string_free(updated); string_free(layout);
    string_free(assets_key); string_free(detail_key); string_free(pattern_key);
    json_free(manifest); json_free(assets); json_free(detail);
    return ok;
}

static void test_cfg_textures_global_detail(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(texture_global_fixture),
                     "full-resolution global grid imports all 1152 images, validates final addresses and rolls back missing input");
}

static void test_cfg_textures_install(void)
{
    string_t *root = texture_fixture_root();
    for (unsigned encrypted = 0; encrypted < 2; ++encrypted) {
        file_t *directory = NULL;
        sqlite_t *db = texture_database(encrypted ? "texture-fixture" : "", &directory);
        TEST_ASSERT_TRUE(db && root && sqlite_exec_cstr(db, "create table preserved(id);insert into preserved values(42)") &&
                         cfg_database_textures(db, root) && cfg_database_textures_valid(db),
                         "all packaged images validate in either database storage mode");
        globe_t *globe = globe_open_databases(db, db, NULL);
        globe_texture_info_t info;
        TEST_ASSERT_TRUE(globe_texture_metadata(globe, 0, 0, 0, &info) && info.width == 2048 && info.height == 1024 &&
                         info.byte_count == texture_source_size("earth_surface.png"),
                         "overview dimensions and encoded length survive installation");
        array_t *png = globe_texture_png(globe, 0, 0, 0, &info);
        TEST_ASSERT_TRUE(png && array_size(png) == info.byte_count, "read unchanged, verified PNG bytes");
        array_destroy(png);
        TEST_ASSERT_TRUE(globe_texture_metadata(globe, 3, 0, 0, &info) && info.width == 1800 &&
                         info.byte_count == texture_source_size("regional/00.png"),
                         "regional image dimensions and encoded length survive installation");
        TEST_ASSERT_TRUE(globe_texture_metadata(globe, 3, 127, 0, &info) && info.width == 3600,
                         "a real installed PNG is addressed by a three-digit regional slot");
        unsigned found = 0;
        const double inside[] = {-3.72, 52.31, -3.71, 52.32}, outside[] = {0, 0, 1, 1};
        TEST_ASSERT_TRUE(globe_texture_regions(globe, inside, texture_region_found, &found) && found >= 2,
                         "regional coverage includes detailed Welsh imagery and an overview");
        unsigned previous = found;
        TEST_ASSERT_TRUE(globe_texture_regions(globe, outside, texture_region_found, &found) && found == previous,
                         "regional coverage selects Wales and leaves other places unchanged");
        TEST_ASSERT_TRUE(globe_texture_metadata(globe, 3, 1, 0, &info) && info.width == 3600 && info.height == 3600,
                         "larger regional imagery retains its validated dimensions");
        TEST_ASSERT_TRUE(sqlite_exec_cstr(db,
            "insert into globe_texture_region(slot,west,south,east,north) values(4095,10,10,11,11);"
            "insert into globe_texture_region_index(slot,west,east,south,north) values(4095,10,11,10,11)"),
            "spatial catalogue supports slots beyond the former 64-region limit");
        const double remote[] = {10.2, 10.2, 10.3, 10.3};
        found = 0;
        TEST_ASSERT_TRUE(globe_texture_regions(globe, remote, texture_region_found, &found) && found == 1,
                         "indexed query finds a high-numbered slot without returning unrelated regions");
        TEST_ASSERT_TRUE(sqlite_exec_cstr(db,
            "delete from globe_texture_region_index where slot=4095;"
            "delete from globe_texture_region where slot=4095"), "remove synthetic region");
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
        sqlite_stmt_finalize(q); globe_close(globe); texture_database_close(db, directory);
    }
    string_free(root);
}

static bool texture_rollback_fixture(const char *directory)
{
    string_t *root = texture_fixture_root(), *bad_root = string_new_with(directory);
    file_t *fixture = NULL;
    sqlite_t *db = texture_database("", &fixture);
    bool ok = root && bad_root && db && cfg_database_textures(db, root) && test_cfg_copy_textures(directory);
    file_t *last = texture_file(directory, "earth_tiles/11-05.png");
    ok = ok && last && file_delete(last) && !cfg_database_textures(db, bad_root) && cfg_database_textures_valid(db);
    string_t *broken = string_new_with("not a PNG");
    ok = ok && broken && file_write_all_text(last, broken) && !cfg_database_textures(db, bad_root) &&
         cfg_database_textures_valid(db);
    ok = ok && cfg_database_revision(db, "globe", CFG_GLOBE_TEXTURE_PATCH_BASE) &&
         !cfg_database_textures(db, bad_root) && cfg_database_textures_valid(db);
    file_free(last); string_free(broken); string_free(root); string_free(bad_root); texture_database_close(db, fixture);
    return ok;
}

static void test_cfg_textures_rollback(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(texture_rollback_fixture),
                     "missing or corrupt final tiles roll back the entire replacement");
}

static bool texture_migration_fixture(const char *directory)
{
    string_t *textures = string_sprintf("%s/packaging/globe-db/textures", test_cfg_texture_root());
    bool configured = textures && !setenv("MARS_GLOBE_TEXTURE_DIR", string_c_str(textures), 1) &&
                      texture_fixture_sources();
    string_free(textures);
    file_t *fixture = NULL;
    sqlite_t *db = texture_database("", &fixture);
    string_t *root = string_new_with(MARS_CONFIG_ROOT_DIR), *staging = string_new_with(directory);
    /* Match the installer's transaction boundary; individual fixture writes must not fsync every row. */
    bool ok = configured && db && root && staging && sqlite_exec_cstr(db,
        "BEGIN;"
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
    q = sqlite_stmt_prepare(db, "SELECT count(*) FROM rivers_bank WHERE length(rings)=48");
    ok = ok && q && sqlite_stmt_step(q) == SQLITE_STEP_ROW && sqlite_stmt_column_int(q, 0) == 1;
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
    /* The coastline transparency update also replaces photographs in the previous complete catalogue. */
    ok = ok && cfg_database_revision(db, "globe", 14) &&
         sqlite_exec_cstr(db, "update globe_texture set png=zeroblob(byte_count) "
                             "where level=3 and column_index=1") &&
         !cfg_database_textures_valid(db) && cfg_database_globe_refresh(db, root, staging) &&
         cfg_database_textures_valid(db);
    /* The expanded catalogue must also replace photographs installed by revision fifteen. */
    ok = ok && cfg_database_revision(db, "globe", 15) &&
         sqlite_exec_cstr(db, "delete from globe_texture where level=3 and column_index=1") &&
         !cfg_database_textures_valid(db) && cfg_database_globe_refresh(db, root, staging) &&
         cfg_database_textures_valid(db);
    /* Compatible migrations keep the image table, import changed rows once and retain all matching PNG bodies. */
    ok = ok && cfg_database_revision(db, "globe", CFG_GLOBE_TEXTURE_PATCH_BASE) && sqlite_exec_cstr(db,
        "UPDATE globe_texture SET width = 1800, height = 1800 WHERE level = 3 AND column_index = 1;\n"
        "CREATE TABLE texture_update_audit (changed INTEGER NOT NULL);\n"
        "INSERT INTO texture_update_audit VALUES (0);\n"
        "CREATE TRIGGER texture_update_probe AFTER UPDATE ON globe_texture BEGIN\n"
        "    UPDATE texture_update_audit SET changed = changed + 1;\n"
        "END;\n"
        "INSERT INTO globe_texture SELECT level, 8191, row_index, width, height, byte_count, sha256, png\n"
        "FROM globe_texture WHERE level = 3 AND column_index = 0;") &&
        cfg_database_globe_refresh(db, root, staging) && cfg_database_textures_valid(db) &&
        cfg_database_globe_refresh(db, root, staging) && cfg_database_textures_valid(db);
    q = sqlite_stmt_prepare(db, "SELECT changed FROM texture_update_audit");
    ok = ok && q && sqlite_stmt_step(q) == SQLITE_STEP_ROW && sqlite_stmt_column_int(q, 0) == 1;
    sqlite_stmt_finalize(q);
    q = sqlite_stmt_prepare(db, "SELECT count(*) FROM sqlite_master WHERE name = 'texture_update_probe'");
    ok = ok && q && sqlite_stmt_step(q) == SQLITE_STEP_ROW && sqlite_stmt_column_int(q, 0) == 1;
    sqlite_stmt_finalize(q);
    string_t *installed_path = fixture ? string_sprintf("%s/textures.db", file_path(fixture)) : NULL;
    ok = ok && installed_path && !setenv("MARS_GLOBE_DB_PATH", string_c_str(installed_path), 1) &&
         !setenv("MARS_GLOBE_DB_KEY", "", 1) && sqlite_exec_cstr(db, "COMMIT");
    sqlite_close(db);
    db = NULL;
    ok = ok && cfg_databases_update() == 0;
    db = installed_path ? sqlite_open_plain(installed_path) : NULL;
    /* An independently prepared catalogue must be imported even when its managed-data revision is unchanged. */
    ok = ok && db && cfg_database_textures_current(db, root) &&
         sqlite_exec_cstr(db, "UPDATE globe_texture_manifest SET json = '{}' WHERE id = 1") &&
         !cfg_database_textures_current(db, root);
    sqlite_close(db);
    db = NULL;
    ok = ok && cfg_databases_update() == 0 && cfg_databases_update() == 0;
    db = installed_path ? sqlite_open_plain(installed_path) : NULL;
    ok = ok && cfg_database_textures_current(db, root);
    string_free(installed_path);
    q = db ? sqlite_stmt_prepare(db,
        "SELECT changed FROM texture_update_audit WHERE EXISTS (\n"
        "    SELECT 1 FROM sqlite_master WHERE name = 'texture_update_probe'\n"
        ") AND EXISTS (SELECT 1 FROM mars_database_revision WHERE name = 'globe' AND revision = 19)") : NULL;
    ok = ok && q && sqlite_stmt_step(q) == SQLITE_STEP_ROW && sqlite_stmt_column_int(q, 0) == 1;
    sqlite_stmt_finalize(q);
    q = sqlite_stmt_prepare(db, "select source from rivers_dataset");
    ok = ok && q && sqlite_stmt_step(q) == SQLITE_STEP_ROW && !strcmp(sqlite_stmt_column_text(q, 0), "retained");
    sqlite_stmt_finalize(q);
    globe_t *globe = ok ? globe_open_databases(db, db, NULL) : NULL;
    globe_texture_info_t info;
    ok = ok && globe_texture_metadata(globe, 3, 1, 0, &info) && info.width == 3600;
    globe_close(globe); string_free(staging); string_free(root); texture_database_close(db, fixture);
    return ok;
}

static void test_cfg_textures_migration(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(texture_migration_fixture),
                     "photograph migrations preserve geography, replace changed rows once and retain matching PNGs");
}

static bool texture_scalar_is(sqlite_t *db, const char *sql, int64_t expected)
{
    sqlite_stmt_t *q = sqlite_stmt_prepare(db, sql);
    bool ok = q && sqlite_stmt_step(q) == SQLITE_STEP_ROW && sqlite_stmt_column_int64(q, 0) == expected &&
              sqlite_stmt_step(q) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(q);
    return ok;
}

static unsigned texture_failed_updates;

static bool texture_failed_update(sqlite_t *db, const string_t *root, const string_t *staging)
{
    (void)staging;
    ++texture_failed_updates;
    /* Fail after real image insertion, exercising rollback of a fully populated replacement. */
    (void)cfg_database_textures(db, root);
    return false;
}

static bool texture_invalid_update(sqlite_t *db, const string_t *root, const string_t *staging)
{
    (void)staging;
    return cfg_database_textures(db, root) && sqlite_exec_cstr(db,
        "UPDATE globe_texture SET byte_count=byte_count+1 WHERE level=0 AND column_index=0 AND row_index=0");
}

static bool texture_staged_fixture(const char *directory)
{
    (void)directory;
    file_t *fixture = NULL;
    sqlite_t *db = texture_database("", &fixture);
    string_t *root = texture_fixture_root();
    string_t *path = fixture ? string_sprintf("%s/textures.db", file_path(fixture)) : NULL;
    string_t *parent = fixture ? string_new_with(file_path(fixture)) : NULL;
    string_t *staging = parent ? cfg_database_stage(parent) : NULL;
    file_t *installed = path ? file_new(path) : NULL;
    bool ok = db && root && installed && staging &&
        texture_fixture_sources() &&
        cfg_database_textures(db, root) &&
        cfg_database_revision(db, "globe", 15) && sqlite_exec_cstr(db,
            "BEGIN; PRAGMA application_id=1296126547; PRAGMA user_version=27;\n"
            "CREATE TABLE country_name(id INTEGER PRIMARY KEY,name TEXT,iso_country_code TEXT);\n"
            "CREATE TABLE country_chunk(id INTEGER PRIMARY KEY,country INTEGER);\n"
            "CREATE VIRTUAL TABLE country_bounds USING rtree_i32(id,west,east,south,north);\n"
            "CREATE TABLE retained(value BLOB, label TEXT, optional INTEGER);\n"
            "WITH RECURSIVE n(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM n WHERE x<5001)\n"
            "INSERT INTO retained(rowid,value,label) SELECT x,cast(x AS BLOB),'kept' FROM n;\n"
            "INSERT INTO retained VALUES(x'00ff','edge',NULL);\n"
            "INSERT INTO retained(rowid,label) VALUES(-9223372036854775808,'minimum'),\n"
            "  (9223372036854775807,'maximum');\n"
            "CREATE TABLE keyed(k TEXT PRIMARY KEY,v BLOB) WITHOUT ROWID;\n"
            "INSERT INTO keyed VALUES('a',x'00ff');\n"
            "CREATE TABLE numbered(id INTEGER PRIMARY KEY AUTOINCREMENT,v TEXT);\n"
            "INSERT INTO numbered VALUES(17,'seventeen'),(99,'deleted');DELETE FROM numbered WHERE id=99;\n"
            "CREATE TABLE generated(a INTEGER,b INTEGER GENERATED ALWAYS AS(a*2) STORED);\n"
            "INSERT INTO generated(a) VALUES(21);\n"
            "CREATE TABLE shadowed(_rowid_ TEXT,rowid TEXT,oid TEXT);\n"
            "INSERT INTO shadowed VALUES('one','two','three');\n"
            "CREATE TABLE sqlitekeeper(value INTEGER);INSERT INTO sqlitekeeper VALUES(42);\n"
            "CREATE TABLE \"quoted\"\"name\"(\"quoted\"\"column\" TEXT);\n"
            "INSERT INTO \"quoted\"\"name\" VALUES('unchanged');\n"
            "CREATE VIRTUAL TABLE river_bounds USING rtree(id,min_x,max_x,min_y,max_y);\n"
            "INSERT INTO river_bounds VALUES(7,1,2,3,4),(8,-5,-4,-3,-2);\n"
            "CREATE INDEX retained_label ON retained(label);\n"
            "CREATE VIEW retained_view AS SELECT label FROM retained;\n"
            "CREATE TABLE audit(value TEXT);INSERT INTO audit VALUES('original');\n"
            "CREATE TRIGGER retained_audit AFTER INSERT ON retained BEGIN\n"
            "  INSERT INTO audit VALUES(new.label);END; COMMIT;");
    if (ok) ok = cfg_database_rebuild_globe(&db, path, root, staging, cfg_database_globe_refresh,
                                           CFG_GLOBE_DATA_REVISION, false) && !db;
    cfg_database_cleanup(staging); string_free(staging);
    db = ok ? sqlite_open_plain(path) : db;
    ok = ok && db && cfg_database_textures_valid(db) && texture_scalar_is(db, "PRAGMA page_size", 65536) &&
         texture_scalar_is(db, "PRAGMA application_id", 1296126547) &&
         texture_scalar_is(db, "PRAGMA user_version", 27) &&
         texture_scalar_is(db, "SELECT count(*) FROM retained", 5004) &&
         texture_scalar_is(db, "SELECT count(*) FROM retained WHERE rowid=-9223372036854775808", 1) &&
         texture_scalar_is(db, "SELECT count(*) FROM retained WHERE rowid=9223372036854775807", 1) &&
         texture_scalar_is(db, "SELECT count(*) FROM keyed WHERE hex(v)='00FF'", 1) &&
         texture_scalar_is(db, "SELECT b FROM generated", 42) &&
         texture_scalar_is(db, "SELECT count(*) FROM shadowed WHERE oid='three'", 1) &&
         texture_scalar_is(db, "SELECT value FROM sqlitekeeper", 42) &&
         texture_scalar_is(db, "SELECT count(*) FROM \"quoted\"\"name\" WHERE \"quoted\"\"column\"='unchanged'", 1) &&
         texture_scalar_is(db, "SELECT count(*) FROM river_bounds WHERE min_x<0", 1) &&
         texture_scalar_is(db, "SELECT count(*) FROM retained_view", 5004) &&
         texture_scalar_is(db, "SELECT count(*) FROM audit", 1) &&
         texture_scalar_is(db, "SELECT seq FROM sqlite_sequence WHERE name='numbered'", 99) &&
         texture_scalar_is(db, "SELECT revision FROM mars_database_revision WHERE name='globe'", CFG_GLOBE_DATA_REVISION) &&
         sqlite_exec_cstr(db, "INSERT INTO retained(label) VALUES('after');INSERT INTO numbered(v) VALUES('next')") &&
         texture_scalar_is(db, "SELECT count(*) FROM audit WHERE value='after'", 1) &&
         texture_scalar_is(db, "SELECT id FROM numbered WHERE v='next'", 100);
    /* Exercise the release dispatch, not just its staging helper, using the same miniature installed store. */
    ok = ok && cfg_database_revision(db, "globe", 15);
    sqlite_close(db); db = NULL;
    ok = ok && !setenv("MARS_GLOBE_DB_PATH", string_c_str(path), 1) && !setenv("MARS_GLOBE_DB_KEY", "", 1) &&
         !setenv("MARS_ROOT", string_c_str(root), 1) && cfg_databases_update() == 0;
    db = ok ? sqlite_open_plain(path) : NULL;
    ok = ok && db && texture_scalar_is(db, "SELECT revision FROM mars_database_revision WHERE name='globe'",
                                     CFG_GLOBE_DATA_REVISION) &&
         texture_scalar_is(db, "SELECT count(*) FROM audit", 2);
    string_t *before = ok ? file_sha256(installed) : NULL;
    staging = ok ? cfg_database_stage(parent) : NULL;
    texture_failed_updates = 0;
    ok = ok && before && staging &&
         cfg_database_rebuild_globe(&db, path, root, staging, texture_failed_update, CFG_GLOBE_DATA_REVISION, false) &&
         !db && !texture_failed_updates;
    cfg_database_cleanup(staging); string_free(staging);
    staging = ok ? cfg_database_stage(parent) : NULL;
    ok = ok && staging && !cfg_database_rebuild_globe(&db, path, root, staging, texture_failed_update,
                                                   CFG_GLOBE_DATA_REVISION, true) &&
         !db && texture_failed_updates == 1;
    string_t *after = ok ? file_sha256(installed) : NULL;
    ok = ok && after && !strcmp(string_c_str(before), string_c_str(after));
    cfg_database_cleanup(staging); string_free(staging); string_free(after);
    staging = ok ? cfg_database_stage(parent) : NULL;
    ok = ok && staging && !cfg_database_rebuild_globe(&db, path, root, staging, texture_invalid_update,
                                                   CFG_GLOBE_DATA_REVISION, true) && !db;
    after = ok ? file_sha256(installed) : NULL;
    ok = ok && after && !strcmp(string_c_str(before), string_c_str(after));
    cfg_database_cleanup(staging); string_free(staging); string_free(after);
    sqlite_t *writer = ok ? sqlite_open_plain(path) : NULL;
    ok = ok && writer && sqlite_exec_cstr(writer, "BEGIN IMMEDIATE");
    staging = ok ? cfg_database_stage(parent) : NULL;
    ok = ok && staging && !cfg_database_rebuild_globe(&db, path, root, staging, texture_failed_update,
                                                   CFG_GLOBE_DATA_REVISION, true) &&
         !db && texture_failed_updates == 1;
    if (writer) ok = sqlite_exec_cstr(writer, "ROLLBACK") && ok;
    sqlite_close(writer);
    after = ok ? file_sha256(installed) : NULL;
    ok = ok && after && !strcmp(string_c_str(before), string_c_str(after));
    cfg_database_cleanup(staging); string_free(staging);
    string_free(after); string_free(before); file_free(installed);
    string_free(path); string_free(parent); string_free(root); texture_database_close(db, fixture);
    return ok;
}

static void test_cfg_textures_staged(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(texture_staged_fixture),
                     "fresh image replacement preserves unrelated schema, rowids and data; failures retain the original");
}

static bool texture_complete_reimport(sqlite_t *db, const string_t *root, const string_t *staging)
{
    (void)staging;
    return cfg_database_textures(db, root) &&
           sqlite_exec_cstr(db,
               "INSERT INTO retained VALUES('during import'); DROP TABLE retained_parent;\n"
               "CREATE TABLE retained_parent(id INTEGER PRIMARY KEY); INSERT INTO retained_parent VALUES(17);");
}

static bool texture_foreign_failure(sqlite_t *db, const string_t *root, const string_t *staging)
{
    return texture_complete_reimport(db, root, staging) &&
           sqlite_exec_cstr(db, "INSERT INTO retained_child VALUES(999)");
}

static bool texture_interrupted_reimport(sqlite_t *db, const string_t *root, const string_t *staging)
{
    if (!texture_complete_reimport(db, root, staging)) _exit(9);
    /* Force dirty pages to disk, then exit without closing SQLite or committing the outer transaction. */
    if (!sqlite_exec_cstr(db, "PRAGMA cache_size=1; UPDATE retained SET value='uncommitted'")) _exit(10);
    _exit(0);
}

static bool texture_reimport_fixture(const char *directory)
{
    (void)directory;
    file_t *fixture = NULL;
    sqlite_t *db = texture_database("", &fixture);
    string_t *root = texture_fixture_root();
    string_t *path = fixture ? string_sprintf("%s/textures.db", file_path(fixture)) : NULL;
    string_t *parent = fixture ? string_new_with(file_path(fixture)) : NULL;
    string_t *staging = parent ? cfg_database_stage(parent) : NULL;
    file_t *installed = path ? file_new(path) : NULL;
    struct stat before = {0}, after = {0};
    bool ok = db && root && staging && installed && cfg_database_textures(db, root) &&
        cfg_database_revision(db, "globe", CFG_GLOBE_DATA_REVISION) && sqlite_exec_cstr(db,
            "BEGIN; CREATE TABLE retained(value TEXT); INSERT INTO retained VALUES('original');\n"
            "CREATE TABLE retained_payload(id INTEGER PRIMARY KEY,bytes BLOB);\n"
            "INSERT INTO retained_payload VALUES(17,zeroblob(1048576));\n"
            "CREATE TABLE retained_parent(id INTEGER PRIMARY KEY); INSERT INTO retained_parent VALUES(17);\n"
            "CREATE TABLE retained_child(parent INTEGER REFERENCES retained_parent(id) ON DELETE CASCADE);\n"
            "INSERT INTO retained_child VALUES(17);\n"
            "CREATE TABLE audit(value TEXT); CREATE INDEX retained_value ON retained(value);\n"
            "CREATE VIEW \"retained\"\"view\" AS SELECT value FROM retained;\n"
            "CREATE TRIGGER retained_audit AFTER INSERT ON retained BEGIN\n"
            "  INSERT INTO audit VALUES(new.value); END;\n"
            "UPDATE globe_texture SET png=zeroblob(byte_count) WHERE level=0;\n"
            "INSERT INTO globe_texture VALUES(99,0,0,1,1,1,'obsolete',x'00'); COMMIT;") &&
        stat(file_path(installed), &before) == 0;
    sqlite_stmt_t *page = ok ? sqlite_stmt_prepare(db,
        "SELECT rootpage FROM main.sqlite_schema WHERE name='retained_payload'") : NULL;
    ok = ok && page && sqlite_stmt_step(page) == SQLITE_STEP_ROW;
    int64_t retained_page = ok ? sqlite_stmt_column_int64(page, 0) : 0;
    sqlite_stmt_finalize(page);
    ok = ok && cfg_database_reimport_globe(db, root, staging, texture_complete_reimport, CFG_GLOBE_DATA_REVISION) &&
         cfg_database_textures_valid(db) && stat(file_path(installed), &after) == 0 &&
         before.st_dev == after.st_dev && before.st_ino == after.st_ino &&
         texture_scalar_is(db, "SELECT rootpage FROM main.sqlite_schema WHERE name='retained_payload'", retained_page) &&
         texture_scalar_is(db, "SELECT length(bytes) FROM retained_payload WHERE id=17", 1048576) &&
         texture_scalar_is(db, "SELECT count(*) FROM globe_texture WHERE level=99", 0) &&
         texture_scalar_is(db, "SELECT count(*) FROM retained_child WHERE parent=17", 1) &&
         texture_scalar_is(db, "PRAGMA foreign_keys", 1) &&
         texture_scalar_is(db, "SELECT count(*) FROM \"retained\"\"view\"", 2) &&
         texture_scalar_is(db, "SELECT count(*) FROM audit", 0) &&
         sqlite_exec_cstr(db, "INSERT INTO retained VALUES('after import')") &&
         texture_scalar_is(db, "SELECT count(*) FROM audit WHERE value='after import'", 1);
    string_t *original = ok ? file_sha256(installed) : NULL;
    ok = ok && original && !cfg_database_reimport_globe(db, root, staging, texture_invalid_update,
                                                       CFG_GLOBE_DATA_REVISION);
    string_t *unchanged = ok ? file_sha256(installed) : NULL;
    ok = ok && unchanged && !strcmp(string_c_str(original), string_c_str(unchanged));
    string_free(unchanged);
    ok = ok && !cfg_database_reimport_globe(db, root, staging, texture_foreign_failure,
                                           CFG_GLOBE_DATA_REVISION) && texture_scalar_is(db, "PRAGMA foreign_keys", 1);
    unchanged = ok ? file_sha256(installed) : NULL;
    ok = ok && unchanged && !strcmp(string_c_str(original), string_c_str(unchanged));
    string_free(unchanged);
    sqlite_t *writer = ok ? sqlite_open_plain(path) : NULL;
    ok = ok && writer && sqlite_exec_cstr(writer, "BEGIN IMMEDIATE") &&
         !cfg_database_reimport_globe(db, root, staging, texture_complete_reimport, CFG_GLOBE_DATA_REVISION) &&
         texture_scalar_is(db, "PRAGMA foreign_keys", 1);
    if (writer) ok = sqlite_exec_cstr(writer, "ROLLBACK") && ok;
    sqlite_close(writer);
    sqlite_close(db); db = NULL;
    pid_t child = ok ? fork() : -1;
    if (child == 0) {
        sqlite_t *candidate = sqlite_open_plain(path);
        bool completed = candidate && cfg_database_reimport_globe(candidate, root, staging,
            texture_interrupted_reimport, CFG_GLOBE_DATA_REVISION);
        sqlite_close(candidate);
        _exit(completed ? 11 : 12);
    }
    int status = 0;
    ok = ok && child > 0 && waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    db = ok ? sqlite_open_plain(path) : NULL;
    unchanged = db ? file_sha256(installed) : NULL;
    ok = ok && db && unchanged && !strcmp(string_c_str(original), string_c_str(unchanged)) &&
         cfg_database_textures_valid(db) && texture_scalar_is(db, "SELECT count(*) FROM retained", 3) &&
         texture_scalar_is(db, "SELECT count(*) FROM audit", 1);
    string_free(unchanged); string_free(original);
    cfg_database_cleanup(staging); string_free(staging);
    file_free(installed); string_free(path); string_free(parent); string_free(root);
    texture_database_close(db, fixture);
    return ok;
}

static void test_cfg_textures_reimport(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(texture_reimport_fixture),
        "full reimport replaces matching and obsolete images, retains payload pages and schema, and recovers interrupted work");
}

/* Keep the documentation's real image and exact byte count out of repeated migration/corruption fixtures. */
static bool texture_documented_overview(sqlite_t *db)
{
    file_t *source = texture_file(MARS_CONFIG_ROOT_DIR, "earth_surface.png");
    array_t *bytes = source ? file_read_all_bytes(source) : NULL;
    string_t *digest = source ? file_sha256(source) : NULL;
    sqlite_stmt_t *replace = db ? sqlite_stmt_prepare(db,
        "UPDATE globe_texture SET byte_count=?1, sha256=?2, png=?3\n"
        "WHERE level=0 AND column_index=0 AND row_index=0") : NULL;
    bool ok = bytes && digest && replace && sqlite_stmt_bind_int64(replace, 1, array_size(bytes)) &&
              sqlite_stmt_bind_text(replace, 2, string_c_str(digest)) &&
              sqlite_stmt_bind_blob(replace, 3, array_get(bytes, 0), array_size(bytes)) &&
              sqlite_stmt_step(replace) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(replace);
    string_free(digest); array_destroy(bytes); file_free(source);
    return ok;
}

static void example_globe_texture_metadata(void)
{
    /* README example: query one image and release its owned bytes after use. */
    file_t *fixture = NULL;
    sqlite_t *db = texture_database("", &fixture);
    string_t *root = texture_fixture_root();
    TEST_ASSERT_TRUE(db && root && cfg_database_textures(db, root) && texture_documented_overview(db),
                     "prepare documented image fixture");
    globe_t *globe = globe_open_databases(db, db, NULL);
    globe_texture_info_t info;
    TEST_ASSERT_TRUE(globe_texture_metadata(globe, 0, 0, 0, &info), "README texture metadata");
    array_t *png = globe_texture_png(globe, 0, 0, 0, &info);
    TEST_ASSERT_TRUE(png && info.width == 2048 && info.height == 1024 && array_size(png) == 2623195,
                     "README output matches the packaged overview");
    printf("%u x %u; %zu bytes\n", info.width, info.height, array_size(png));
    array_destroy(png); globe_close(globe); string_free(root); texture_database_close(db, fixture);
}

void test_cfg_textures_cases(void)
{
    (void)test_cfg_texture_root();
    TEST_RUN_IN_GROUP(test_cfg_textures_manifest, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_textures_capacity, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_textures_global_detail, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_textures_install, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_textures_rollback, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_textures_migration, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_textures_staged, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_textures_reimport, tests, NULL);
}

void test_cfg_textures_readme_cases(void)
{
    TEST_RUN_IN_GROUP(example_globe_texture_metadata, readme_examples, NULL);
}
