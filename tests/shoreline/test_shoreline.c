/**
 * @file test_shoreline.c
 * @brief Shoreline and country storage, spatial classification and upstream dataset regressions.
 *
 * Exercises opaque readers against synthetic nested rings and the packaged GSHHG data. Checks bounded chunk
 * decompression, transactional rollback, longitude wrapping and uncertain boundaries without an ephemeris engine.
 * Temporary files use the native file module; the caller's installed database is never modified.
 * Administrative tests cover indexed country queries, enclaves, islands and transactional import failures.
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
#define MARS_GLOBE_INTERNAL_ACCESS
#include "globe/globe_layers.h"
#define MARS_SHORELINE_INTERNAL_ACCESS
#include "globe/shoreline_internal.h"
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

static bool make_compressed_chunk(sqlite_t *db, int codec)
{
    sqlite_stmt_t *query = sqlite_stmt_prepare(db,
        "SELECT substr(b.vertices,c.byte_offset+1,c.byte_count) FROM shoreline_chunk c "
        "JOIN shoreline_blob b ON b.id=c.blob_id WHERE c.id=1");
    bool ok = query && sqlite_stmt_step(query) == SQLITE_STEP_ROW;
    unsigned char packed[8192];
    size_t length = ok ? ZSTD_compress(packed, sizeof packed, sqlite_stmt_column_blob(query, 0),
                                      sqlite_stmt_column_bytes(query, 0), 3) : 0;
    sqlite_stmt_finalize(query);
    if (!ok || !length || ZSTD_isError(length)) return false;
    query = sqlite_stmt_prepare(db, "INSERT INTO shoreline_blob VALUES(1000000,?1)");
    ok = query && sqlite_stmt_bind_blob(query, 1, packed, length) && sqlite_stmt_step(query) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(query);
    query = sqlite_stmt_prepare(db,
        "UPDATE shoreline_chunk SET blob_id=1000000,byte_offset=0,byte_count=?1,codec=?2 WHERE id=1");
    ok = ok && query && sqlite_stmt_bind_int(query, 1, (int)length) && sqlite_stmt_bind_int(query, 2, codec) &&
         sqlite_stmt_step(query) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(query);
    return ok;
}

static bool write_bytes(file_t *file, const void *bytes, size_t size)
{
    size_t written;
    return file_write(file, bytes, size, &written) && written == size;
}

static bool write_precise_ring(file_t *file, int id, const double (*vertices)[2], size_t count)
{
    unsigned char header[44] = {0}, point[16];
    double west = INFINITY, east = -INFINITY, south = INFINITY, north = -INFINITY;
    for (size_t i = 0; i < count; ++i) {
        west = fmin(west, vertices[i][0]); east = fmax(east, vertices[i][0]);
        south = fmin(south, vertices[i][1]); north = fmax(north, vertices[i][1]);
    }
    put_integer(header, id); put_integer(header + 4, (int)count); put_integer(header + 8, 15 * 256 + 1);
    put_integer(header + 12, (int32_t)floor(west * 1000000));
    put_integer(header + 16, (int32_t)ceil(east * 1000000));
    put_integer(header + 20, (int32_t)floor(south * 1000000));
    put_integer(header + 24, (int32_t)ceil(north * 1000000));
    bool ok = write_bytes(file, header, sizeof header);
    for (size_t i = 0; ok && i < count; ++i) {
        coast_encode_double(point, vertices[i][0]); coast_encode_double(point + 8, vertices[i][1]);
        ok = write_bytes(file, point, sizeof point);
    }
    return ok;
}

typedef struct _coast_exact_probe_t {
    const double (*vertices)[2];
    size_t count, seen;
} coast_exact_probe_t;

static bool check_exact_path(globe_path_event_t event, double x, double y, unsigned level, bool antarctic, void *context)
{
    coast_exact_probe_t *probe = context;
    if (level != 1 || antarctic) return false;
    if (event == GLOBE_PATH_CLOSE) return true;
    if (probe->seen >= probe->count) return false;
    const double *expected = probe->vertices[probe->seen++];
    return x == expected[0] && y == expected[1];
}

static void test_shoreline_lossless_coordinates(void)
{
    file_t *directory = temporary_directory();
    TEST_ASSERT_NOT_NULL(directory);
    file_t *source = temporary_file(directory, "lossless.bin");
    sqlite_t *db = memory_database();
    TEST_ASSERT_NOT_NULL(db);
    const double corners[][2] = {{-20, -20}, {20, -20}, {20, 20}, {-20, 20}, {-20, -20}};
    double square[1025][2];
    for (unsigned i = 0; i < 1025; ++i) {
        unsigned edge = i == 1024 ? 3 : i / 256, offset = i == 1024 ? 256 : i % 256;
        for (unsigned axis = 0; axis < 2; ++axis)
            square[i][axis] = corners[edge][axis] + (corners[edge + 1][axis] - corners[edge][axis]) * offset / 256;
    }
    const double tiny[][2] = {{100.5934961, 2.9196721}, {100.5934962, 2.9196721},
                             {100.5934962, 2.9196722}, {100.5934961, 2.9196721}};
    TEST_ASSERT_TRUE(file_open_write(source) && write_bytes(source, "MARSCF64", 8) &&
        write_precise_ring(source, 1, square, 1025) && write_precise_ring(source, 2, tiny, 4) &&
        file_close(source) && file_open_read(source) && globe_import_shoreline(db, source), "lossless import");
    file_close(source);
    globe_t *globe = globe_open(db, NULL);
    TEST_ASSERT_NOT_NULL(globe);
    TEST_ASSERT_INT_EQ(globe_hit_test(globe, 0, 0), GLOBE_LAND);
    TEST_ASSERT_INT_EQ(globe_hit_test(globe, 30, 0), GLOBE_SEA);
    TEST_ASSERT_INT_EQ(globe_hit_test(globe, tiny[0][1], tiny[0][0]), GLOBE_COAST);
    coast_exact_probe_t probe = {.vertices = tiny, .count = 4};
    TEST_ASSERT_TRUE(globe_surface_paths(globe, 100.59, 2.91, 100.60, 2.93, 0, check_exact_path, &probe) &&
                     probe.seen >= 3, "collapsed-on-microdegree-grid ring survives with exact original vertices");
    TEST_ASSERT_INT_EQ(globe_hit_test(globe, 0, 0), GLOBE_LAND);
    TEST_ASSERT_TRUE(make_compressed_chunk(db, SHORELINE_CODEC_F64_ZSTD) && globe_surface_prepare(globe),
                     "lossless compressed resident coordinates");
    globe_close(globe);
    globe = globe_open(db, NULL);
    TEST_ASSERT_INT_EQ(globe_hit_test(globe, 0, 0), GLOBE_LAND);
    globe_close(globe);
    const double invalid[][2] = {{NAN, 0}, {1, 1}, {2, 0}, {NAN, 0}};
    TEST_ASSERT_TRUE(file_open_write(source) && write_bytes(source, "MARSCF64", 8) &&
        write_precise_ring(source, 3, invalid, 4) && file_close(source) && file_open_read(source) &&
        !globe_import_shoreline(db, source), "non-finite lossless input rolls back");
    file_close(source);
    globe = globe_open(db, NULL);
    TEST_ASSERT_INT_EQ(globe_hit_test(globe, 0, 0), GLOBE_LAND);
    globe_close(globe);
    sqlite_close(db);
    file_delete(source); file_free(source); file_remove_directory(directory); file_free(directory);
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
    TEST_ASSERT_TRUE(make_compressed_chunk(db, SHORELINE_CODEC_ZSTD), "legacy compressed chunk fixture");
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

static bool country_test_border(double ax, double ay, double bx, double by, void *context)
{
    size_t *count = context;
    ++*count;
    return isfinite(ax) && isfinite(ay) && isfinite(bx) && isfinite(by);
}

static void test_country_packaged_boundaries(void)
{
    file_t *directory = temporary_directory();
    TEST_ASSERT_NOT_NULL(directory);
    const char *extensions[] = {"shp", "dbf"};
    file_t *source[2] = {NULL, NULL};
    for (unsigned i = 0; i < 2; ++i) {
        string_t *path = string_sprintf("packaging/globe-db/map-units-5.1.1.%s.zst", extensions[i]);
        file_t *packed = file_new(path);
        string_free(path);
        source[i] = temporary_file(directory, extensions[i]);
        TEST_ASSERT_TRUE(file_decompress_zstd(packed, source[i], 16 * 1024 * 1024, false) && file_open_read(source[i]),
                         "bounded country source decompression");
        file_free(packed);
    }
    /* Exercise the installed, disk-backed layout rather than locking the entire world catalogue in RAM. */
    file_t *database = temporary_file(directory, "countries.db");
    string_t *path = string_new_with(file_path(database)), *key = string_new_with("country-import-test");
    sqlite_t *db = sqlite_open_encrypted(path, key);
    string_free(path); string_free(key);
    TEST_ASSERT_NOT_NULL(db);
    TEST_ASSERT_TRUE(sqlite_exec_cstr(db, "PRAGMA cache_size=-512;PRAGMA temp.cache_size=-512;"
                                         "create table retained(value);insert into retained values(42)"), "other layer");
    TEST_ASSERT_TRUE(globe_import_countries(db, source[0], source[1]), "packaged country polygons import");
    string_t *iso = countries_lookup(db, 52.7077, -2.7541, true);
    TEST_ASSERT_TRUE(iso && !strcmp(string_c_str(iso), "GB-ENG"), "constituent-country code joins the jurisdiction catalogue");
    string_free(iso);
    size_t borders = 0;
    TEST_ASSERT_TRUE(countries_borders(db, -180, -90, 180, 90, country_test_border, &borders) && borders > 68617,
                     "map units retain international borders and add constituent-country boundaries");
    printf("Shared international border segments: %zu\n", borders);
    borders = 0;
    TEST_ASSERT_TRUE(countries_borders(db, -5, 50, 2, 51, country_test_border, &borders) && borders == 0,
                     "English coastline is not duplicated as an administrative border");
    TEST_ASSERT_TRUE(countries_borders(db, -3.3, 51.5, -2.3, 53.3, country_test_border, &borders) && borders > 0,
                     "Wales has a land border with England");
    borders = 0;
    TEST_ASSERT_TRUE(countries_borders(db, -4, 54.8, -1.5, 56, country_test_border, &borders) && borders > 0,
                     "Scotland has a land border with England");
    borders = 0;
    TEST_ASSERT_TRUE(countries_borders(db, -4.9, 54, -4, 54.5, country_test_border, &borders) && borders == 0,
                     "Isle of Man has no invented shared land or maritime boundary");
    TEST_ASSERT_TRUE(countries_borders(db, 6, 48, 9, 50, country_test_border, &borders) && borders > 0,
                     "Franco-German land border is present");
    TEST_ASSERT_TRUE(!countries_borders(db, 0, 0, 361, 1, country_test_border, &borders), "invalid viewport rejected");
    static const struct { double lat, lon; const char *name; } sites[] = {
        {52.7077, -2.7541, "England"}, {48.8566, 2.3522, "France"},
        {51.4816, -3.1791, "Wales"}, {55.9533, -3.1883, "Scotland"}, {54.23, -4.52, "Isle of Man"},
        {-29.31, 27.48, "Lesotho"}, {-25.75, 28.19, "South Africa"},
        {-17.77, 177.42, "Fiji"}, {55.70, 12.50, "Denmark"}, {52.37, 4.90, "Netherlands"},
        {38.72, -9.14, "Portugal"}, {0, -30, NULL}, {NAN, 0, NULL}, {91, 0, NULL}
    };
    for (size_t i = 0; i < sizeof sites / sizeof *sites; ++i) {
        string_t *name = countries_at(db, sites[i].lat, sites[i].lon);
        printf("Country %.5f %.5f: %s\n", sites[i].lat, sites[i].lon, name ? string_c_str(name) : "unavailable");
        if (sites[i].name) TEST_ASSERT_TRUE(name && !strcmp(string_c_str(name), sites[i].name), sites[i].name);
        else TEST_ASSERT_TRUE(!name, "sea or invalid position has no country");
        string_free(name);
    }
    globe_t *globe = globe_open_databases(db, db, NULL);
    TEST_ASSERT_TRUE(globe_has_layer(globe, GLOBE_LAYER_COUNTRIES), "country layer reports availability");
    TEST_ASSERT_TRUE(!globe_country_at(globe, 52.7077, -2.7541), "country polygons cannot replace missing shoreline");
    globe_close(globe);
    TEST_ASSERT_TRUE(!globe_import_countries(db, source[0], source[1]), "truncated source rejected");
    file_t *broken = temporary_file(directory, "broken.shp");
    unsigned char header[100];
    size_t count = 0;
    TEST_ASSERT_TRUE(file_seek(source[0], 0, FILE_SEEK_BEGIN) &&
                     file_read(source[0], header, sizeof header, &count) && count == sizeof header &&
                     file_open_write(broken) && file_write(broken, header, sizeof header, &count) &&
                     count == sizeof header && file_close(broken) && file_open_read(broken) &&
                     file_seek(source[1], 0, FILE_SEEK_BEGIN), "header-only shape fixture");
    TEST_ASSERT_TRUE(!globe_import_countries(db, broken, source[1]), "failure after replacement begins rolls back");
    file_close(broken); file_delete(broken); file_free(broken);
    string_t *name = countries_at(db, 52.7077, -2.7541);
    TEST_ASSERT_TRUE(name && !strcmp(string_c_str(name), "England"), "failed import preserves boundaries");
    string_free(name);
    sqlite_stmt_t *q = sqlite_stmt_prepare(db, "select page_count*page_size from pragma_page_count,pragma_page_size");
    TEST_ASSERT_TRUE(q && sqlite_stmt_step(q) == SQLITE_STEP_ROW, "measure country storage");
    printf("Country database including spatial index: %lld bytes\n", (long long)sqlite_stmt_column_int64(q, 0));
    TEST_ASSERT_TRUE(sqlite_stmt_column_int64(q, 0) < 15 * 1024 * 1024, "country storage budget");
    sqlite_stmt_finalize(q);
    q = sqlite_stmt_prepare(db, "select value from retained");
    TEST_ASSERT_TRUE(q && sqlite_stmt_step(q) == SQLITE_STEP_ROW && sqlite_stmt_column_int(q, 0) == 42,
                     "unrelated data preserved");
    sqlite_stmt_finalize(q);
    TEST_ASSERT_TRUE(sqlite_exec_cstr(db,
        "insert into country_name values(1000,'Overlapping fixture','');"
        "insert into country_chunk select id+100000,1000,points from country_chunk where country="
        "(select id from country_name where name='England');"
        "insert into country_bounds select b.id+100000,b.west,b.east,b.south,b.north from country_bounds b "
        "join country_chunk c on c.id=b.id where c.country=(select id from country_name where name='England')"),
        "overlapping administrative fixture");
    name = countries_at(db, 52.7077, -2.7541);
    TEST_ASSERT_TRUE(!name, "overlapping countries produce no guessed name");
    string_free(name);
    sqlite_close(db);
    file_delete(database); file_free(database);
    for (unsigned i = 0; i < 2; ++i) {
        file_close(source[i]); file_delete(source[i]); file_free(source[i]);
    }
    file_remove_directory(directory); file_free(directory);
}

int tests_main(void)
{
    TEST_SECTION("Indexed shorelines");
    TEST_RUN_IN_GROUP(test_country_packaged_boundaries, tests, NULL);
    TEST_RUN_IN_GROUP(test_shoreline_lossless_coordinates, tests, NULL);
    TEST_RUN_IN_GROUP(test_shoreline_nested_rings_and_rollback, tests, NULL);
    TEST_RUN_IN_GROUP(test_shoreline_packaged_world, tests, NULL);
    return TEST_EXIT_CODE();
}
