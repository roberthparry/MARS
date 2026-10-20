/**
 * @file test_rivers.c
 * @brief River import, indexed clipping, dateline and rollback regressions.
 *
 * Synthetic open lines establish exact segment counts and rollback behaviour without depending on withdrawn
 * geographic data or accessing the user's installed jurisdiction database.
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "globe-t.h"
#include "string-t.h"
#include "test_harness.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

static sqlite_t *test_database(void)
{
    string_t *path = string_new_with(":memory:"), *key = string_new_with("rivers-test");
    sqlite_t *db = sqlite_open_encrypted(path, key);
    string_free(path);
    string_free(key);
    return db;
}

static file_t *temporary_directory(void)
{
    string_t *parent = string_new_with("tests/build/release/rivers");
    file_t *directory = file_create_temp_directory(parent);
    string_free(parent);
    return directory;
}

static file_t *child(file_t *directory, const char *name)
{
    string_t *path = string_sprintf("%s/%s", file_path(directory), name);
    file_t *f = file_new(path);
    string_free(path);
    return f;
}

static void put(unsigned char *p, int32_t value)
{
    uint32_t n = (uint32_t)value;
    p[0] = n >> 24; p[1] = n >> 16; p[2] = n >> 8; p[3] = n;
}

static bool line(file_t *file, int id, int west, int east)
{
    unsigned char b[44 + 300 * 8] = {0};
    put(b, id); put(b + 4, 300); put(b + 8, 15 * 256 + 1);
    put(b + 12, west * 1000000); put(b + 16, east * 1000000);
    for (int i = 0; i < 300; ++i) put(b + 44 + i * 8, west * 1000000 + (east - west) * 1000000LL * i / 299);
    size_t written;
    return file_write(file, b, sizeof b, &written) && written == sizeof b;
}

typedef struct { unsigned count; double west, south, east, north; bool valid; } hits_t;

static bool hit(int64_t id, unsigned kind, double ax, double ay, double bx, double by, void *context)
{
    hits_t *h = context;
    ++h->count;
    h->valid = h->valid && id >= 0 && kind <= 255 && fmin(ax, bx) >= h->west - 1e-9 &&
               fmax(ax, bx) <= h->east + 1e-9 && fmin(ay, by) >= h->south - 1e-9 && fmax(ay, by) <= h->north + 1e-9;
    return true;
}

static bool cancel(int64_t id, unsigned kind, double ax, double ay, double bx, double by, void *context)
{
    (void)id; (void)kind; (void)ax; (void)ay; (void)bx; (void)by; (void)context;
    return false;
}

static void test_rivers_index_and_rollback(void)
{
    file_t *directory = temporary_directory(), *source = child(directory, "rivers.bin");
    sqlite_t *db = test_database();
    TEST_ASSERT_NOT_NULL(db);
    TEST_ASSERT_TRUE(file_open_write(source) && line(source, 1, -2, 2) && line(source, 2, 179, 181), "write open lines");
    file_close(source);
    TEST_ASSERT_TRUE(file_open_read(source) && globe_import_rivers(db, source), "import bounded chunks");
    file_close(source);
    globe_t *retired = globe_open(NULL, db);
    TEST_ASSERT_TRUE(retired && !globe_has_layer(retired, GLOBE_LAYER_RIVERS), "withdrawn WDBII is unavailable");
    globe_close(retired);
    TEST_ASSERT_TRUE(sqlite_exec_cstr(db, "update rivers_dataset set source='synthetic fixture'"), "mark synthetic data");
    sqlite_t *town_db = test_database();
    TEST_ASSERT_NOT_NULL(town_db);
    TEST_ASSERT_TRUE(sqlite_exec_cstr(town_db, "create table jurisdiction_town(id integer primary key)"), "town store");
    globe_t *split = globe_open_databases(NULL, db, town_db);
    TEST_ASSERT_TRUE(split && globe_has_layer(split, GLOBE_LAYER_RIVERS) &&
                     globe_has_layer(split, GLOBE_LAYER_TOWNS), "separate map and town stores");
    hits_t split_hits = {.west = -3, .south = -1, .east = 3, .north = 1, .valid = true};
    TEST_ASSERT_TRUE(globe_river_segments(split, -3, -1, 3, 1, hit, &split_hits) && split_hits.count == 299,
                     "river query uses map store");
    globe_close(split);
    split = globe_open_databases(NULL, town_db, db);
    TEST_ASSERT_TRUE(split && !globe_has_layer(split, GLOBE_LAYER_RIVERS) &&
                     !globe_has_layer(split, GLOBE_LAYER_TOWNS), "do not search wrong stores for layers");
    globe_close(split);
    TEST_ASSERT_TRUE(sqlite_exec_cstr(town_db, "insert into jurisdiction_town values(1)"), "borrowed store stays open");
    sqlite_close(town_db);
    TEST_ASSERT_TRUE(!globe_open_databases(NULL, NULL, NULL), "connections required");
    globe_t *r = globe_open(NULL, db);
    TEST_ASSERT_NOT_NULL(r);
    hits_t h = {.west = -3, .south = -1, .east = 3, .north = 1, .valid = true};
    TEST_ASSERT_TRUE(globe_river_segments(r, -3, -1, 3, 1, hit, &h), "open polyline");
    TEST_ASSERT_TRUE(h.valid && h.count == 299, "no invented closing edge or duplicated chunk edge");
    h = (hits_t){.west = -181, .south = -1, .east = -179, .north = 1, .valid = true};
    TEST_ASSERT_TRUE(globe_river_segments(r, -181, -1, -179, 1, hit, &h), "wrapped dateline");
    TEST_ASSERT_TRUE(h.valid && h.count == 299, "dateline geometry retained");
    h = (hits_t){.west = -.5, .south = -1, .east = .5, .north = 1, .valid = true};
    TEST_ASSERT_TRUE(globe_river_segments(r, -.5, -1, .5, 1, hit, &h) && h.valid && h.count > 0, "clipped output");
    TEST_ASSERT_TRUE(!globe_river_segments(r, -3, -1, 3, 1, cancel, NULL), "visitor cancellation");
    TEST_ASSERT_TRUE(!globe_river_segments(r, NAN, -1, 3, 1, hit, &h), "invalid bounds");
    globe_close(r);
    TEST_ASSERT_TRUE(file_open_write(source) && file_truncate(source, 0), "truncate input");
    file_close(source);
    TEST_ASSERT_TRUE(file_open_read(source) && !globe_import_rivers(db, source), "empty replacement rolls back");
    file_close(source);
    r = globe_open(NULL, db);
    h = (hits_t){.west = -3, .south = -1, .east = 3, .north = 1, .valid = true};
    TEST_ASSERT_TRUE(r && globe_river_segments(r, -3, -1, 3, 1, hit, &h) && h.count == 299, "old dataset preserved");
    globe_close(r);
    sqlite_close(db);
    TEST_ASSERT_TRUE(file_delete(source) && file_remove_directory(directory), "clean fixture");
    file_free(source); file_free(directory);
}


int tests_main(void)
{
    TEST_SECTION("Indexed river network");
    TEST_RUN_IN_GROUP(test_rivers_index_and_rollback, tests, NULL);
    return TEST_EXIT_CODE();
}
