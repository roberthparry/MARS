/**
 * @file test_shoreline.c
 * @brief Shoreline storage, spatial classification and upstream dataset regressions.
 *
 * Exercises opaque readers against synthetic nested rings and the packaged GSHHG data. Checks bounded chunk
 * decompression, transactional rollback, longitude wrapping and uncertain boundaries without an ephemeris engine.
 * Temporary files use the native file module; the caller's installed database is never modified.
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zstd.h>

#include "globe-t.h"
#include "almanac-t.h"
#include "string-t.h"
#include "test_harness.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

static file_t *temporary_file(file_t *directory, const char *name)
{
    string_t *path = string_sprintf("%s/%s", file_path(directory), name);
    file_t *file = file_new(path);
    string_free(path);
    return file;
}

static file_t *temporary_directory(void)
{
    /* Keep the full-resolution fixture off memory-backed /tmp on small machines. */
    string_t *parent = string_new_with("tests/build/release/shoreline");
    file_t *directory = file_create_temp_directory(parent);
    string_free(parent);
    return directory;
}

static sqlite_t *memory_database(void)
{
    string_t *path = string_new_with(":memory:"), *key = string_new_with("shoreline-test");
    sqlite_t *db = sqlite_open_encrypted(path, key);
    string_free(path);
    string_free(key);
    return db;
}

static void put_integer(unsigned char *bytes, int32_t value)
{
    uint32_t n = (uint32_t)value;
    bytes[0] = n >> 24;
    bytes[1] = n >> 16;
    bytes[2] = n >> 8;
    bytes[3] = n;
}

static bool write_square(file_t *file, int id, int level, int west, int east, int south, int north, bool dense)
{
    unsigned char bytes[44 + 1025 * 8] = {0};
    int steps = dense ? 256 : 1, points = steps * 4 + 1;
    put_integer(bytes, id);
    put_integer(bytes + 4, points);
    put_integer(bytes + 8, 15 * 256 + level);
    put_integer(bytes + 12, west * 1000000);
    put_integer(bytes + 16, east * 1000000);
    put_integer(bytes + 20, south * 1000000);
    put_integer(bytes + 24, north * 1000000);
    const int x[] = {west, east, east, west, west}, y[] = {south, south, north, north, south};
    for (int i = 0; i < points; ++i) {
        int edge = i == points - 1 ? 3 : i / steps, offset = i == points - 1 ? steps : i % steps;
        put_integer(bytes + 44 + i * 8, x[edge] * 1000000 +
                    (int)((int64_t)(x[edge + 1] - x[edge]) * 1000000 * offset / steps));
        put_integer(bytes + 48 + i * 8, y[edge] * 1000000 +
                    (int)((int64_t)(y[edge + 1] - y[edge]) * 1000000 * offset / steps));
    }
    size_t length = 44 + points * 8, written;
    return file_write(file, bytes, length, &written) && written == length;
}

typedef struct {
    double x, y, first_x, first_y, last_x, last_y;
    bool inside, land, valid, antarctic, saw_antarctic;
    unsigned closed, highest_level;
} coast_test_path_t;

static void path_crossing(coast_test_path_t *path, double x, double y)
{
    if ((path->last_y > path->y) != (y > path->y) &&
        path->x < path->last_x + (path->y - path->last_y) * (x - path->last_x) / (y - path->last_y))
        path->inside = !path->inside;
    path->last_x = x;
    path->last_y = y;
}

static bool check_path(globe_path_event_t event, double longitude, double latitude,
                       unsigned level, bool antarctic, void *context)
{
    coast_test_path_t *path = context;
    path->saw_antarctic |= antarctic;
    path->valid = path->valid && isfinite(longitude) && isfinite(latitude) && level >= 1 && level <= 4;
    if (event == GLOBE_PATH_MOVE) {
        path->first_x = path->last_x = longitude;
        path->first_y = path->last_y = latitude;
        path->inside = false;
    } else if (event == GLOBE_PATH_LINE) {
        path_crossing(path, longitude, latitude);
    } else {
        path_crossing(path, path->first_x, path->first_y);
        if (path->inside && level >= path->highest_level) {
            path->highest_level = level;
            path->land = level % 2;
            path->antarctic = antarctic;
        }
        ++path->closed;
    }
    return path->valid;
}

static bool make_legacy_chunk(sqlite_t *db)
{
    sqlite_stmt_t *query = sqlite_stmt_prepare(db,
        "SELECT substr(b.vertices,c.byte_offset+1,c.byte_count) FROM shoreline_chunk c "
        "JOIN shoreline_blob b ON b.id=c.blob_id WHERE c.id=1");
    bool ok = query && sqlite_stmt_step(query) == SQLITE_STEP_ROW;
    unsigned char packed[4096];
    size_t length = ok ? ZSTD_compress(packed, sizeof packed, sqlite_stmt_column_blob(query, 0),
                                      sqlite_stmt_column_bytes(query, 0), 3) : 0;
    sqlite_stmt_finalize(query);
    if (!ok || !length || ZSTD_isError(length)) return false;
    query = sqlite_stmt_prepare(db, "INSERT INTO shoreline_blob VALUES(1000000,?1)");
    ok = query && sqlite_stmt_bind_blob(query, 1, packed, length) && sqlite_stmt_step(query) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(query);
    query = sqlite_stmt_prepare(db,
        "UPDATE shoreline_chunk SET blob_id=1000000,byte_offset=0,byte_count=?1,codec=1 WHERE id=1");
    ok = ok && query && sqlite_stmt_bind_int(query, 1, (int)length) && sqlite_stmt_step(query) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(query);
    return ok;
}

static void test_shoreline_nested_rings_and_rollback(void)
{
    file_t *directory = temporary_directory();
    TEST_ASSERT_NOT_NULL(directory);
    file_t *source = temporary_file(directory, "rings.bin");
    sqlite_t *db = memory_database();
    TEST_ASSERT_NOT_NULL(db);
    globe_t *empty = globe_open(db, NULL);
    TEST_ASSERT_TRUE(empty && !globe_has_layer(empty, GLOBE_LAYER_SHORELINE) &&
                     globe_hit_test(empty, 0, 0) == GLOBE_UNKNOWN, "missing data are not sea");
    TEST_ASSERT_TRUE(!globe_has_layer(empty, GLOBE_LAYER_ROADS) &&
                     !globe_has_layer(empty, GLOBE_LAYER_BUILDINGS), "uninstalled detail is unavailable");
    globe_close(empty);
    TEST_ASSERT_TRUE(file_open_write(source) &&
                     write_square(source, 0, 1, -20, 20, -20, 20, true) &&
                     write_square(source, 1, 2, -10, 10, -10, 10, false) &&
                     write_square(source, 2, 3, -5, 5, -5, 5, false) &&
                     write_square(source, 3, 4, -1, 1, -1, 1, false) &&
                     write_square(source, 4, 1, 170, 190, -10, 10, false) && file_close(source), "fixture rings");
    TEST_ASSERT_TRUE(file_open_read(source) && globe_import_shoreline(db, source) && file_close(source), "import rings");
    TEST_ASSERT_TRUE(sqlite_exec_cstr(db, "BEGIN IMMEDIATE; CREATE TABLE retained(value); INSERT INTO retained VALUES(7)") &&
                     file_open_read(source) && globe_import_shoreline(db, source) && file_close(source) &&
                     sqlite_exec_cstr(db, "ROLLBACK"), "shoreline import participates in caller-owned transaction");
    sqlite_stmt_t *retained = sqlite_stmt_prepare(db, "SELECT count(*) FROM sqlite_master WHERE name='retained'");
    TEST_ASSERT_TRUE(retained && sqlite_stmt_step(retained) == SQLITE_STEP_ROW &&
                     sqlite_stmt_column_int(retained, 0) == 0, "nested import does not commit caller changes");
    sqlite_stmt_finalize(retained);
    globe_t *shoreline = globe_open(db, NULL);
    TEST_ASSERT_NOT_NULL(shoreline);
    TEST_ASSERT_INT_EQ(globe_hit_test(shoreline, 15, 0), GLOBE_LAND);
    TEST_ASSERT_INT_EQ(globe_hit_test(shoreline, 7, 0), GLOBE_INLAND_WATER);
    TEST_ASSERT_INT_EQ(globe_hit_test(shoreline, 3, 0), GLOBE_LAND);
    TEST_ASSERT_INT_EQ(globe_hit_test(shoreline, 0, 0), GLOBE_INLAND_WATER);
    TEST_ASSERT_INT_EQ(globe_hit_test(shoreline, 30, 0), GLOBE_SEA);
    TEST_ASSERT_INT_EQ(globe_hit_test(shoreline, 20, 0), GLOBE_COAST);
    TEST_ASSERT_INT_EQ(globe_hit_test(shoreline, 20.003, 0), GLOBE_COAST);
    TEST_ASSERT_INT_EQ(globe_hit_test(shoreline, 20.01, 0), GLOBE_SEA);
    TEST_ASSERT_INT_EQ(globe_hit_test(shoreline, 0, -179), GLOBE_LAND);
    TEST_ASSERT_INT_EQ(globe_hit_test(shoreline, 0, 179), GLOBE_LAND);
    TEST_ASSERT_INT_EQ(globe_hit_test(shoreline, 0, -180), GLOBE_LAND);
    TEST_ASSERT_INT_EQ(globe_hit_test(shoreline, NAN, 0), GLOBE_UNKNOWN);
    TEST_ASSERT_INT_EQ(globe_hit_test(shoreline, 91, 0), GLOBE_UNKNOWN);
    const double samples[][2] = {{0, 15}, {0, 7}, {0, 3}, {0, 0}, {0, 30}};
    TEST_ASSERT_TRUE(sqlite_exec_cstr(db, "PRAGMA cache_size=-1024"), "caller cache budget");
    for (unsigned i = 0; i < sizeof samples / sizeof *samples; ++i) {
        coast_test_path_t path = {.x = samples[i][0], .y = samples[i][1], .valid = true};
        TEST_ASSERT_TRUE(globe_surface_paths(shoreline, -32, -32, 32, 32, 0, check_path, &path), "clipped vector rings");
        TEST_ASSERT_TRUE(path.valid && path.closed == 4, "bounded closed polygons");
        TEST_ASSERT_INT_EQ(path.land, globe_hit_test(shoreline, path.y, path.x) == GLOBE_LAND);
    }
    coast_test_path_t path = {.x = 0, .y = 15, .valid = true};
    TEST_ASSERT_TRUE(globe_surface_paths(shoreline, -1, 14, 1, 16, 0, check_path, &path), "enclosing polygon retained");
    TEST_ASSERT_TRUE(path.land, "viewport wholly inside land remains green");
    path = (coast_test_path_t){.x = 180, .y = 0, .valid = true};
    TEST_ASSERT_TRUE(globe_surface_paths(shoreline, 160, -16, 200, 16, 0, check_path, &path), "date-line vectors");
    TEST_ASSERT_TRUE(path.land, "date-line island filled");
    TEST_ASSERT_TRUE(globe_surface_prepare(shoreline), "reusable resident geometry");
    TEST_ASSERT_INT_EQ(globe_hit_test(shoreline, 20.003, 0), GLOBE_COAST);
    TEST_ASSERT_INT_EQ(globe_hit_test(shoreline, 0, -179), GLOBE_LAND);
    TEST_ASSERT_TRUE(make_legacy_chunk(db), "legacy compressed chunk fixture");
    TEST_ASSERT_TRUE(globe_surface_prepare(shoreline), "anonymous cache refreshes after mutation and reads legacy chunks");
    TEST_ASSERT_INT_EQ(globe_hit_test(shoreline, 15, 0), GLOBE_LAND);
    sqlite_stmt_t *cache = sqlite_stmt_prepare(db, "PRAGMA cache_size");
    TEST_ASSERT_TRUE(cache && sqlite_stmt_step(cache) == SQLITE_STEP_ROW, "read restored cache budget");
    TEST_ASSERT_INT_EQ(sqlite_stmt_column_int(cache, 0), -1024);
    sqlite_stmt_finalize(cache);
    TEST_ASSERT_TRUE(!globe_surface_paths(shoreline, 0, 0, 0, 1, 0, check_path, &path), "reject empty extent");
    TEST_ASSERT_TRUE(!globe_surface_paths(shoreline, NAN, 0, 1, 1, 0, check_path, &path), "reject non-finite extent");
    globe_close(shoreline);
    size_t written;
    TEST_ASSERT_TRUE(file_open_write(source) && file_write(source, "broken", 6, &written) && file_close(source),
                     "truncated replacement fixture");
    TEST_ASSERT_TRUE(sqlite_exec_cstr(db, "BEGIN IMMEDIATE; CREATE TABLE retained(value); INSERT INTO retained VALUES(7)") &&
                     file_open_read(source) && !globe_import_shoreline(db, source) && file_close(source) &&
                     sqlite_exec_cstr(db, "COMMIT"),
                     "bad replacement rolls back");
    retained = sqlite_stmt_prepare(db, "SELECT value FROM retained");
    TEST_ASSERT_TRUE(retained && sqlite_stmt_step(retained) == SQLITE_STEP_ROW &&
                     sqlite_stmt_column_int(retained, 0) == 7, "failed import preserves caller transaction");
    sqlite_stmt_finalize(retained);
    shoreline = globe_open(db, NULL);
    TEST_ASSERT_NOT_NULL(shoreline);
    TEST_ASSERT_INT_EQ(globe_hit_test(shoreline, 15, 0), GLOBE_LAND);
    globe_close(shoreline);
    TEST_ASSERT_TRUE(sqlite_exec_cstr(db, "UPDATE shoreline_chunk SET codec=1; UPDATE shoreline_blob SET vertices=x'00'"),
                     "corrupt payload fixture");
    shoreline = globe_open(db, NULL);
    TEST_ASSERT_INT_EQ(globe_hit_test(shoreline, 15, 0), GLOBE_UNKNOWN);
    TEST_ASSERT_TRUE(!globe_surface_paths(shoreline, -32, -32, 32, 32, 0, check_path, &path), "reject corrupt vector source");
    globe_close(shoreline);
    sqlite_close(db);
    TEST_ASSERT_TRUE(file_delete(source) && file_remove_directory(directory), "remove owned fixtures");
    file_free(source);
    file_free(directory);
}

static void test_shoreline_packaged_world(void)
{
    file_t *directory = temporary_directory();
    TEST_ASSERT_NOT_NULL(directory);
    file_t *source = temporary_file(directory, "world.bin");
    string_t *path = string_new_with("packaging/almanac-db/gshhs_f.b.zst");
    file_t *packed = file_new(path);
    string_free(path);
    file_t *database = temporary_file(directory, "world.db");
    string_t *database_path = string_new_with(file_path(database)), *key = string_new_with("shoreline-test");
    sqlite_t *db = sqlite_open_encrypted(database_path, key);
    string_free(database_path);
    string_free(key);
    TEST_ASSERT_NOT_NULL(db);
    bool imported = file_decompress_zstd(packed, source, 128 * 1024 * 1024, false) &&
                    file_open_read(source) && globe_import_shoreline(db, source) && file_close(source);
    if (!imported)
        fprintf(stderr, "Shoreline import: file error %d, database error %s\n", file_last_error(source),
                string_c_str(sqlite_last_error(db)));
    TEST_ASSERT_TRUE(imported, "import GSHHG");
    globe_t *shoreline = globe_open(db, NULL);
    TEST_ASSERT_NOT_NULL(shoreline);
    const struct { double latitude, longitude; globe_surface_t surface; } sites[] = {
        {52.7077, -2.7541, GLOBE_LAND},
        {36.7202, -4.4203, GLOBE_COAST},
        {36.7300, -4.4300, GLOBE_LAND},
        {51, 0, GLOBE_LAND},
        {0, 0, GLOBE_SEA},
        {40, -30, GLOBE_SEA},
        {-25, 135, GLOBE_LAND},
        {40, -100, GLOBE_LAND},
        {0, 180, GLOBE_SEA},
        {0, -180, GLOBE_SEA},
        {-89, 0, GLOBE_LAND},
        {-89, 180, GLOBE_LAND},
        {-89, -180, GLOBE_LAND},
        {-90, 0, GLOBE_LAND},
        {90, 0, GLOBE_SEA},
        {47.7, -87.5, GLOBE_INLAND_WATER},
        {-17.7, 178.1, GLOBE_LAND},
    };
    for (size_t i = 0; i < sizeof sites / sizeof *sites; ++i) {
        printf("Classify %.4f, %.4f\n", sites[i].latitude, sites[i].longitude);
        TEST_ASSERT_INT_EQ(globe_hit_test(shoreline, sites[i].latitude, sites[i].longitude), sites[i].surface);
    }
    sqlite_stmt_t *query = sqlite_stmt_prepare(db, "SELECT point_count,polygon_count FROM shoreline_dataset");
    TEST_ASSERT_TRUE(query && sqlite_stmt_step(query) == SQLITE_STEP_ROW, "dataset metadata");
    TEST_ASSERT_INT_EQ(sqlite_stmt_column_int(query, 0), 10587854);
    TEST_ASSERT_INT_EQ(sqlite_stmt_column_int(query, 1), 188268);
    sqlite_stmt_finalize(query);
    coast_test_path_t world = {.x = -2.7541, .y = 52.7077, .valid = true};
    TEST_ASSERT_TRUE(globe_surface_paths(shoreline, -180, -90, 180, 90, 0.08, check_path, &world), "world vector overview");
    TEST_ASSERT_TRUE(world.valid && world.land && world.closed > 1000, "world vectors include inland observer");
    TEST_ASSERT_TRUE(world.saw_antarctic && !world.antarctic, "Antarctica distinguished from ordinary land");
    TEST_ASSERT_TRUE(globe_surface_prepare(shoreline), "world cache remains resident");
    for (size_t i = 0; i < sizeof sites / sizeof *sites; ++i)
        TEST_ASSERT_INT_EQ(globe_hit_test(shoreline, sites[i].latitude, sites[i].longitude), sites[i].surface);
    coast_test_path_t polar = {.x = 0, .y = -85, .valid = true};
    TEST_ASSERT_TRUE(globe_surface_paths(shoreline, -5, -86, 5, -84, 0, check_path, &polar), "polar vector detail");
    TEST_ASSERT_TRUE(polar.valid && polar.land && polar.antarctic, "clipped Antarctic interior retains ice-front identity");
    coast_test_path_t lake = {.x = -87.5, .y = 47.7, .valid = true};
    TEST_ASSERT_TRUE(globe_surface_paths(shoreline, -88, 47, -87, 48, 0, check_path, &lake), "lake vector detail");
    TEST_ASSERT_TRUE(lake.valid && !lake.land, "inland water remains water in vector detail");
    const char *names[] = {"MARS_GLOBE_DB_PATH", "MARS_GLOBE_DB_KEY", "MARS_ALMANAC_DB_PATH", "MARS_ALMANAC_DB_KEY"};
    string_t *saved[4] = {0};
    for (size_t i = 0; i < 4; ++i)
        if (getenv(names[i])) saved[i] = string_new_with(getenv(names[i]));
    bool configured = !setenv(names[0], file_path(database), 1) && !setenv(names[1], "shoreline-test", 1) &&
                      !setenv(names[2], ":memory:", 1) && !setenv(names[3], "empty-almanac-test", 1);
    globe_t *independent = configured ? globe_open_configured(NULL) : NULL;
    almanac_t *empty_almanac = configured ? almanac_open() : NULL;
    bool separate = independent && empty_almanac &&
                    globe_hit_test(independent, 52.7077, -2.7541) == GLOBE_LAND &&
                    almanac_surface_at(empty_almanac, 52.7077, -2.7541) == GLOBE_LAND;
    globe_close(independent);
    almanac_close(empty_almanac);
    bool missing = !setenv(names[0], "/nonexistent/mars-globe-test.db", 1);
    independent = globe_open_configured(NULL);
    missing = missing && !independent;
    globe_close(independent);
    for (size_t i = 0; i < 4; ++i) {
        if (saved[i]) setenv(names[i], string_c_str(saved[i]), 1);
        else unsetenv(names[i]);
        string_free(saved[i]);
    }
    TEST_ASSERT_TRUE(separate, "empty astronomical database uses independent configured globe geometry");
    TEST_ASSERT_TRUE(missing, "missing globe database is not created or replaced by legacy geometry");
    query = sqlite_stmt_prepare(db, "SELECT count(*),sum(length(vertices)) FROM shoreline_blob");
    TEST_ASSERT_TRUE(query && sqlite_stmt_step(query) == SQLITE_STEP_ROW, "BLOB totals");
    printf("Shoreline BLOB pages: %d; uncompressed bytes: %lld\n", sqlite_stmt_column_int(query, 0),
           (long long)sqlite_stmt_column_int64(query, 1));
    sqlite_stmt_finalize(query);
    query = sqlite_stmt_prepare(db, "SELECT count(*) FROM shoreline_chunk WHERE codec<>0");
    TEST_ASSERT_TRUE(query && sqlite_stmt_step(query) == SQLITE_STEP_ROW, "read stored codecs");
    TEST_ASSERT_INT_EQ(sqlite_stmt_column_int(query, 0), 0);
    sqlite_stmt_finalize(query);
    TEST_ASSERT_TRUE(sqlite_exec_cstr(db, "UPDATE shoreline_blob SET vertices=x'00' WHERE id=1"), "replace cached data");
    TEST_ASSERT_TRUE(!globe_surface_prepare(shoreline), "file-backed cache rejects changed corrupt data");
    globe_close(shoreline);
    sqlite_close(db);
    file_info_t *info = file_get_info(database);
    TEST_ASSERT_NOT_NULL(info);
    printf("Encrypted shoreline database, including indexes: %llu bytes\n",
           (unsigned long long)file_info_size(info));
    file_info_free(info);
    TEST_ASSERT_TRUE(file_delete(source) && file_delete(database) && file_remove_directory(directory),
                     "remove world fixture");
    file_free(database);
    file_free(packed);
    file_free(source);
    file_free(directory);
}

int tests_main(void)
{
    TEST_SECTION("Indexed shorelines");
    TEST_RUN_IN_GROUP(test_shoreline_nested_rings_and_rollback, tests, NULL);
    TEST_RUN_IN_GROUP(test_shoreline_packaged_world, tests, NULL);
    return TEST_EXIT_CODE();
}
