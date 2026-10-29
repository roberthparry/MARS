/**
 * @file test_cfg_rivers.c
 * @brief Installer-owned OSM import, junction-preserving display and rollback regressions.
 *
 * Synthetic GeoJSON sequences are imported into disposable unencrypted databases and queried through the
 * public globe API. No installed data or user credentials are accessed; the runtime has no OSM parser dependency.
 */
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "globe-t.h"
#include "string-t.h"
#include "cfg_database.h"
#define MARS_CFG_DATABASE_INTERNAL_ACCESS
#include "cfg_database_internal.h"
#include "test_harness.h"
#include "test_cfg_support.h"

static sqlite_t *test_database(void)
{
    string_t *path = string_new_with(":memory:");
    sqlite_t *db = sqlite_open_plain(path);
    string_free(path);
    return db;
}

static file_t *temporary_directory(void);

static void test_cfg_rivers_main_selection(void)
{
    file_t *directory = temporary_directory();
    string_t *path = directory ? string_sprintf("%s/main.db", file_path(directory)) : NULL;
    sqlite_t *source = test_database();
    bool ok = source && path && sqlite_exec_cstr(source,
        "create table rivers_dataset(id,format_version,source,point_count,feature_count,source_point_count,"
        "storage_tolerance_metres);"
        "insert into rivers_dataset values(1,2,'OpenStreetMap centreline export / ODbL',11,5,11,8);"
        "create table rivers_chunk(id integer primary key,feature_id,kind,vertices,anchors);"
        "insert into rivers_chunk values"
        "(1,10,1,x'0000000000000000000f424000000000',x'0101'),"
        "(2,10,1,x'000f424000000000001e848000000000',x'0101'),"
        "(3,20,1,x'000f424000000000000f4240000f4240',x'0101'),"
        "(4,30,2,x'001e848000000000001e8480000f4240',x'0101'),"
        "(5,40,3,x'001e848000000000001e8480fff0bdc0',x'0101'),"
        "(6,50,4,x'000000000000000000000000000f4240',x'0101');"
        "create virtual table rivers_bounds using rtree_i32(id,min_x,max_x,min_y,max_y);"
        "insert into rivers_bounds values(1,0,1000000,0,0),(2,1000000,2000000,0,0),"
        "(3,1000000,1000000,0,1000000),(4,2000000,2000000,0,1000000),"
        "(5,2000000,2000000,-1000000,0),(6,0,0,0,1000000)") &&
        cfg_database_select_main_rivers(source, path);
    sqlite_t *selected = ok ? sqlite_open_plain(path) : NULL;
    sqlite_stmt_t *meta = selected ? sqlite_stmt_prepare(selected,
        "select point_count,feature_count,source_point_count from rivers_dataset") : NULL;
    ok = ok && meta && sqlite_stmt_step(meta) == SQLITE_STEP_ROW &&
        sqlite_stmt_column_int64(meta, 0) == 5 && sqlite_stmt_column_int64(meta, 1) == 2 &&
        sqlite_stmt_column_int64(meta, 2) == 11;
    sqlite_stmt_finalize(meta);
    sqlite_stmt_t *a = sqlite_stmt_prepare(source,
        "select id,feature_id,kind,hex(vertices),hex(anchors) from rivers_chunk where kind=1 order by id");
    sqlite_stmt_t *b = selected ? sqlite_stmt_prepare(selected,
        "select id,feature_id,kind,hex(vertices),hex(anchors) from rivers_chunk order by id") : NULL;
    ok = ok && a && b;
    size_t rows = 0;
    sqlite_step_result_t left = SQLITE_STEP_DONE, right = SQLITE_STEP_DONE;
    while (ok && a && b && (left = sqlite_stmt_step(a)) == SQLITE_STEP_ROW) {
        right = sqlite_stmt_step(b);
        ok = right == SQLITE_STEP_ROW;
        for (unsigned column = 0; ok && column < 5; ++column)
            ok = !strcmp(sqlite_stmt_column_text(a, column), sqlite_stmt_column_text(b, column));
        ++rows;
    }
    ok = ok && left == SQLITE_STEP_DONE && sqlite_stmt_step(b) == SQLITE_STEP_DONE && rows == 3;
    sqlite_stmt_finalize(a); sqlite_stmt_finalize(b);
    ok = ok && !cfg_database_select_main_rivers(source, path);
    meta = sqlite_stmt_prepare(source, "select count(*) from rivers_chunk");
    ok = ok && meta && sqlite_stmt_step(meta) == SQLITE_STEP_ROW && sqlite_stmt_column_int(meta, 0) == 6;
    sqlite_stmt_finalize(meta);
    sqlite_close(selected); sqlite_close(source);
    file_t *file = path ? file_new(path) : NULL;
    if (file && file_exists(file)) file_delete(file);
    file_free(file); string_free(path);
    if (directory) file_remove_directory(directory);
    file_free(directory);
    TEST_ASSERT_TRUE(ok, "main-channel selection removes minor classes and retains complete tributaries and anchors");
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

typedef struct { unsigned count; double west, south, east, north; bool valid; } hits_t;

static bool hit(int64_t id, unsigned kind, double ax, double ay, double bx, double by, void *context)
{
    hits_t *h = context;
    ++h->count;
    h->valid = h->valid && id >= 0 && kind <= 255 && fmin(ax, bx) >= h->west - 1e-9 &&
               fmax(ax, bx) <= h->east + 1e-9 && fmin(ay, by) >= h->south - 1e-9 && fmax(ay, by) <= h->north + 1e-9;
    return true;
}


static void test_cfg_rivers_osm_import_and_display(void)
{
    static const char source_text[] =
        "\036{\"type\":\"Feature\",\"geometry\":{\"type\":\"LineString\",\"coordinates\":[[0,0],[1,0.01],[2,0],[3,0.01],[4,0]]},"
        "\"properties\":{\"@type\":\"way\",\"@id\":5000000001,\"@way_nodes\":[1,2,3,4,5],\"waterway\":\"river\"}}\n"
        "\036{\"type\":\"Feature\",\"geometry\":{\"type\":\"LineString\",\"coordinates\":[[2,0],[2,1]]},"
        "\"properties\":{\"@type\":\"way\",\"@id\":5000000002,\"@way_nodes\":[3,6],\"waterway\":\"stream\"}}\n";
    file_t *directory = temporary_directory(), *source = child(directory, "rivers.geojsonseq");
    sqlite_t *db = test_database();
    size_t written;
    TEST_ASSERT_TRUE(db && sqlite_exec_cstr(db, "create table shoreline_sentinel(id integer);insert into shoreline_sentinel values(7)"),
                     "retain unrelated shoreline tables");
    TEST_ASSERT_TRUE(file_open_write(source) && file_write(source, source_text, sizeof source_text - 1, &written) &&
                     written == sizeof source_text - 1, "write OSM export fixture");
    file_close(source);
    TEST_ASSERT_TRUE(file_open_read(source) && cfg_database_import_osm_rivers(db, source), "import OSM centrelines and anchors");
    file_close(source);
    globe_t *g = globe_open_databases(NULL, db, NULL);
    hits_t h = {.west = -1, .south = -1, .east = 5, .north = 2, .valid = true};
    TEST_ASSERT_TRUE(g && globe_has_layer(g, GLOBE_LAYER_RIVERS), "OSM source is available");
    TEST_ASSERT_TRUE(globe_river_segments(g, -1, -1, 5, 2, hit, &h) && h.count == 5 && h.valid,
                     "exact geometry survives import");
    h.count = 0;
    TEST_ASSERT_TRUE(globe_river_display_segments(g, -1, -1, 5, 2, .05, hit, &h) && h.count == 3 && h.valid,
                     "display reduction retains the shared confluence");
    TEST_ASSERT_TRUE(!globe_river_display_segments(g, -1, -1, 5, 2, NAN, hit, &h), "reject invalid tolerance");
    globe_close(g);
    sqlite_stmt_t *q = sqlite_stmt_prepare(db,
        "select feature_id,length(anchors) from rivers_chunk where id=1");
    TEST_ASSERT_TRUE(q && sqlite_stmt_step(q) == SQLITE_STEP_ROW &&
                     sqlite_stmt_column_int64(q, 0) == 5000000001LL && sqlite_stmt_column_int(q, 1) == 5,
                     "retain 64-bit way identity and bounded anchor array");
    sqlite_stmt_finalize(q);
    sqlite_t *installed = test_database();
    TEST_ASSERT_TRUE(installed && sqlite_exec_cstr(installed,
        "create table shoreline_sentinel(id integer);insert into shoreline_sentinel values(42)"), "create installed fixture");
    TEST_ASSERT_TRUE(cfg_database_publish_rivers(installed, db), "publish only validated river tables");
    TEST_ASSERT_TRUE(!cfg_database_publish_rivers(db, db), "reject self-publication");
    TEST_ASSERT_TRUE(sqlite_exec_cstr(db, "update rivers_dataset set point_count=point_count+1"), "damage metadata count");
    TEST_ASSERT_TRUE(!cfg_database_publish_rivers(installed, db), "metadata mismatch rolls back publication");
    TEST_ASSERT_TRUE(sqlite_exec_cstr(db, "update rivers_dataset set point_count=point_count-1"), "restore source count");
    TEST_ASSERT_TRUE(sqlite_exec_cstr(db, "update rivers_chunk set anchors=x'00' where id=1"), "damage source anchors");
    TEST_ASSERT_TRUE(!cfg_database_publish_rivers(installed, db), "bad anchors roll back publication");
    TEST_ASSERT_TRUE(sqlite_exec_cstr(db, "update rivers_chunk set anchors=x'0100010001' where id=1"), "restore source anchors");
    g = globe_open_databases(NULL, installed, NULL);
    h.count = 0;
    TEST_ASSERT_TRUE(g && globe_river_segments(g, -1, -1, 5, 2, hit, &h) && h.count == 5,
                     "failed publications preserve installed river geometry");
    globe_close(g);
    q = sqlite_stmt_prepare(installed, "select id from shoreline_sentinel");
    TEST_ASSERT_TRUE(q && sqlite_stmt_step(q) == SQLITE_STEP_ROW && sqlite_stmt_column_int(q, 0) == 42,
                     "publication leaves installed shoreline untouched");
    sqlite_stmt_finalize(q);
    sqlite_close(installed);
    TEST_ASSERT_TRUE(file_open_write(source) && file_truncate(source, 0) &&
                     file_write(source, source_text, sizeof source_text - 2, &written), "write truncated sequence");
    file_close(source);
    TEST_ASSERT_TRUE(file_open_read(source) && !cfg_database_import_osm_rivers(db, source), "truncated import rolls back");
    file_close(source);
    g = globe_open_databases(NULL, db, NULL);
    h.count = 0;
    TEST_ASSERT_TRUE(globe_river_segments(g, -1, -1, 5, 2, hit, &h) && h.count == 5, "failed replacement retains rivers");
    globe_close(g);
    q = sqlite_stmt_prepare(db, "select id from shoreline_sentinel");
    TEST_ASSERT_TRUE(q && sqlite_stmt_step(q) == SQLITE_STEP_ROW && sqlite_stmt_column_int(q, 0) == 7,
                     "shoreline survives successful and failed imports");
    sqlite_stmt_finalize(q);
    sqlite_close(db);
    TEST_ASSERT_TRUE(file_delete(source) && file_remove_directory(directory), "clean OSM fixture");
    file_free(source); file_free(directory);
}

static void test_cfg_rivers_storage_detail(void)
{
    static const char source_text[] =
        "{\"type\":\"Feature\",\"geometry\":{\"type\":\"LineString\",\"coordinates\":[[0,0],[0.001,0.00001],[0.002,0],[0.003,0.00001],[0.004,0]]},"
        "\"properties\":{\"@type\":\"way\",\"@id\":101,\"@way_nodes\":[1,2,3,4,5],\"waterway\":\"river\"}}\n"
        "{\"type\":\"Feature\",\"geometry\":{\"type\":\"LineString\",\"coordinates\":[[0.002,0],[0.002,0.001]]},"
        "\"properties\":{\"@type\":\"way\",\"@id\":102,\"@way_nodes\":[3,6],\"waterway\":\"stream\"}}\n"
        "{\"type\":\"Feature\",\"geometry\":{\"type\":\"LineString\",\"coordinates\":[[1,0],[1.001,0.0002],[1.002,0]]},"
        "\"properties\":{\"@type\":\"way\",\"@id\":103,\"@way_nodes\":[11,12,13],\"waterway\":\"river\"}}\n"
        "{\"type\":\"Feature\",\"geometry\":{\"type\":\"LineString\",\"coordinates\":[[2,0],[2.00001,0],[2.00001,0.00001],[2,0.00001],[2,0]]},"
        "\"properties\":{\"@type\":\"way\",\"@id\":104,\"@way_nodes\":[21,22,23,24,21],\"waterway\":\"stream\"}}\n"
        "{\"type\":\"Feature\",\"geometry\":{\"type\":\"LineString\",\"coordinates\":[[3,0],[3.001,0.00005],[3.002,0]]},"
        "\"properties\":{\"@type\":\"way\",\"@id\":105,\"@way_nodes\":[31,32,33],\"waterway\":\"canal\"}}\n"
        "{\"type\":\"Feature\",\"geometry\":{\"type\":\"LineString\",\"coordinates\":[[3.001,-0.00005],[3.001,0.00002]]},"
        "\"properties\":{\"@type\":\"way\",\"@id\":106,\"@way_nodes\":[34,35],\"waterway\":\"stream\"}}\n"
        "{\"type\":\"Feature\",\"geometry\":{\"type\":\"LineString\",\"coordinates\":[[179.999,2],[180,2.00001],[-179.999,2]]},"
        "\"properties\":{\"@type\":\"way\",\"@id\":107,\"@way_nodes\":[41,42,43],\"waterway\":\"tidal_channel\"}}\n"
        "{\"type\":\"Feature\",\"geometry\":{\"type\":\"LineString\",\"coordinates\":[[179.999,3],[180,3.00005],[-179.999,3]]},"
        "\"properties\":{\"@type\":\"way\",\"@id\":108,\"@way_nodes\":[51,52,53],\"waterway\":\"river\"}}\n"
        "{\"type\":\"Feature\",\"geometry\":{\"type\":\"LineString\",\"coordinates\":[[-180,2.99995],[-180,3.00002]]},"
        "\"properties\":{\"@type\":\"way\",\"@id\":109,\"@way_nodes\":[54,55],\"waterway\":\"stream\"}}\n";
    file_t *directory = temporary_directory(), *source = child(directory, "detail.geojsonseq");
    sqlite_t *db = test_database();
    string_t *text = string_new_with(source_text);
    TEST_ASSERT_TRUE(source && text && file_write_all_text(source, text) && file_open_read(source) &&
                     cfg_database_import_osm_rivers(db, source), "stage shoreline-detail rivers, including narrow streams");
    file_close(source);
    string_free(text);
    sqlite_stmt_t *q = sqlite_stmt_prepare(db,
        "select source_point_count,point_count,feature_count,storage_tolerance_metres,"
        "(select sum(length(vertices)) from rivers_chunk) from rivers_dataset");
    TEST_ASSERT_TRUE(q && sqlite_stmt_step(q) == SQLITE_STEP_ROW &&
                     sqlite_stmt_column_int64(q, 0) == 28 && sqlite_stmt_column_int64(q, 1) == 25 &&
                     sqlite_stmt_column_int(q, 2) == 9 && sqlite_stmt_column_double(q, 3) == 8 &&
                     sqlite_stmt_column_int(q, 4) == 200, "storage shrinks while all nine waterways remain");
    sqlite_stmt_finalize(q);
    static const int expected[] = {3, 2, 3, 5, 3, 2, 2, 3, 2};
    q = sqlite_stmt_prepare(db, "select feature_id,length(anchors) from rivers_chunk order by feature_id");
    for (size_t i = 0; i < sizeof expected / sizeof *expected; ++i)
        TEST_ASSERT_TRUE(q && sqlite_stmt_step(q) == SQLITE_STEP_ROW &&
                         sqlite_stmt_column_int64(q, 0) == (int64_t)(101 + i) &&
                         sqlite_stmt_column_int(q, 1) == expected[i],
                         "preserve confluences, bends, small loops and neighbours, including across the date line");
    sqlite_stmt_finalize(q);
    q = sqlite_stmt_prepare(db,
        "select hex(anchors),hex(substr(vertices,1,8)),hex(substr(vertices,-8)) from rivers_chunk where feature_id=101");
    TEST_ASSERT_TRUE(q && sqlite_stmt_step(q) == SQLITE_STEP_ROW &&
                     !strcmp(sqlite_stmt_column_text(q, 0), "010101") &&
                     !strcmp(sqlite_stmt_column_text(q, 1), "0000000000000000") &&
                     !strcmp(sqlite_stmt_column_text(q, 2), "00000FA000000000"),
                     "junction and mouth coordinates remain exact");
    sqlite_stmt_finalize(q);
    sqlite_t *installed = test_database();
    TEST_ASSERT_TRUE(cfg_database_publish_rivers(installed, db), "publish compact candidate with detail metadata");
    TEST_ASSERT_TRUE(sqlite_exec_cstr(db, "update rivers_dataset set storage_tolerance_metres=100"),
                     "construct over-coarse candidate");
    TEST_ASSERT_TRUE(!cfg_database_publish_rivers(installed, db), "reject an over-coarse replacement");
    q = sqlite_stmt_prepare(installed, "select point_count,storage_tolerance_metres from rivers_dataset");
    TEST_ASSERT_TRUE(q && sqlite_stmt_step(q) == SQLITE_STEP_ROW && sqlite_stmt_column_int(q, 0) == 25 &&
                     sqlite_stmt_column_double(q, 1) == 8, "rejection retains compact installed candidate");
    sqlite_stmt_finalize(q);
    TEST_ASSERT_TRUE(sqlite_exec_cstr(db,
        "update rivers_dataset set storage_tolerance_metres=8;"
        "create table candidate_padding(payload blob);insert into candidate_padding values(zeroblob(3800000));"),
        "construct an oversized candidate without changing its valid geometry");
    TEST_ASSERT_TRUE(!cfg_database_publish_rivers_limited(installed, db, 3800000),
                     "exercise source budget rejection with a small regression limit");
    q = sqlite_stmt_prepare(installed, "select point_count from rivers_dataset");
    TEST_ASSERT_TRUE(q && sqlite_stmt_step(q) == SQLITE_STEP_ROW && sqlite_stmt_column_int(q, 0) == 25,
                     "budget rejection preserves the previously installed river layer");
    sqlite_stmt_finalize(q);
    TEST_ASSERT_TRUE(cfg_database_publish_rivers(installed, db),
                     "the approved 7 GB limit accepts a valid candidate above the former 3.8 MB limit");
    sqlite_close(installed); sqlite_close(db);
    TEST_ASSERT_TRUE(file_delete(source) && file_remove_directory(directory), "clean storage-detail fixture");
    file_free(source); file_free(directory);
}

static void test_cfg_rivers_neighbourhoods(void)
{
    static const unsigned counts[] = {0, 128, 129};
    file_t *directory = temporary_directory(), *source = child(directory, "neighbours.geojsonseq");
    for (size_t c = 0; c < sizeof counts / sizeof *counts; ++c) {
        for (unsigned crossing = 0; crossing < 2; ++crossing) {
            string_t *text = string_new_with(
                "{\"type\":\"Feature\",\"geometry\":{\"type\":\"LineString\",\"coordinates\":"
                "[[0,0],[0.0005,0.00001],[0.001,0],[0.001,0.001],[0.001,0.002]]},"
                "\"properties\":{\"@type\":\"way\",\"@id\":1,\"@way_nodes\":[1,2,3,4,5],\"waterway\":\"river\"}}\n");
            /* These neighbours fill the chunk envelope but touch neither shortcut. */
            for (unsigned i = 0; i < counts[c]; ++i)
                TEST_ASSERT_TRUE(string_append_format(text,
                    "{\"type\":\"Feature\",\"geometry\":{\"type\":\"LineString\",\"coordinates\":"
                    "[[0.0001,0.0005],[0.0009,0.0005]]},\"properties\":{\"@type\":\"way\",\"@id\":%u,"
                    "\"@way_nodes\":[%u,%u],\"waterway\":\"river\"}}\n", i + 10, i * 2 + 10, i * 2 + 11) >= 0,
                    "append neighbours outside the proposed shortcuts");
            if (crossing)
                TEST_ASSERT_TRUE(string_append_cstr(text,
                    "{\"type\":\"Feature\",\"geometry\":{\"type\":\"LineString\",\"coordinates\":"
                    "[[0.0005,-0.0001],[0.0005,0.000005]]},\"properties\":{\"@type\":\"way\",\"@id\":1000,"
                    "\"@way_nodes\":[2000,2001],\"waterway\":\"river\"}}\n") >= 0,
                    "append a neighbour crossed only by a shortcut");
            sqlite_t *db = test_database();
            TEST_ASSERT_TRUE(db && text && file_write_all_text(source, text) && file_open_read(source) &&
                             cfg_database_import_osm_rivers(db, source), "import bounded and crowded neighbourhoods");
            file_close(source);
            sqlite_stmt_t *q = sqlite_stmt_prepare(db, "select length(vertices)/8 from rivers_chunk where feature_id=1");
            TEST_ASSERT_TRUE(q && sqlite_stmt_step(q) == SQLITE_STEP_ROW &&
                             sqlite_stmt_column_int(q, 0) == (crossing ? 5 : 3),
                             "crowded envelopes still simplify safe shortcuts and preserve crossing geometry");
            sqlite_stmt_finalize(q);
            sqlite_close(db);
            string_free(text);
        }
    }
    TEST_ASSERT_TRUE(file_delete(source) && file_remove_directory(directory), "remove neighbourhood fixtures");
    file_free(source); file_free(directory);
}

static void test_cfg_rivers_storage_chunks(void)
{
    string_t *text = string_new_with("{\"type\":\"Feature\",\"geometry\":{\"type\":\"LineString\",\"coordinates\":[");
    for (unsigned i = 0; text && i < 260; ++i)
        TEST_ASSERT_TRUE(string_append_format(text, "%s[0.%06u,0]", i ? "," : "", i * 10) >= 0, "append source point");
    TEST_ASSERT_TRUE(string_append_cstr(text,
        "]},\"properties\":{\"@type\":\"way\",\"@id\":1,\"waterway\":\"river\",\"@way_nodes\":[") >= 0,
        "append source node identifiers");
    for (unsigned i = 0; i < 260; ++i)
        TEST_ASSERT_TRUE(string_append_format(text, "%s%u", i ? "," : "", i + 1) >= 0, "append source node");
    TEST_ASSERT_TRUE(string_append_cstr(text, "]}}\n") >= 0, "finish sequence");
    file_t *directory = temporary_directory(), *source = child(directory, "chunks.geojsonseq");
    sqlite_t *db = test_database();
    TEST_ASSERT_TRUE(db && file_write_all_text(source, text) && file_open_read(source) &&
                     cfg_database_import_osm_rivers(db, source), "simplify both sides of a chunk boundary");
    file_close(source); string_free(text);
    sqlite_stmt_t *q = sqlite_stmt_prepare(db,
        "select source_point_count,point_count,(select count(*) from rivers_chunk),"
        "(select sum(length(vertices)) from rivers_chunk) from rivers_dataset");
    TEST_ASSERT_TRUE(q && sqlite_stmt_step(q) == SQLITE_STEP_ROW && sqlite_stmt_column_int(q, 0) == 260 &&
                     sqlite_stmt_column_int(q, 1) == 3 && sqlite_stmt_column_int(q, 2) == 2 &&
                     sqlite_stmt_column_int(q, 3) == 32, "260 source points reduce to three with a shared boundary");
    sqlite_stmt_finalize(q);
    sqlite_t *installed = test_database();
    TEST_ASSERT_TRUE(cfg_database_publish_rivers(installed, db), "recount shared boundary correctly when publishing");
    sqlite_close(installed); sqlite_close(db);
    TEST_ASSERT_TRUE(file_delete(source) && file_remove_directory(directory), "clean chunk fixture");
    file_free(source); file_free(directory);
}

static void test_cfg_rivers_section_widths(void)
{
    static const struct { const char *properties; bool water; } areas[] = {
        {"{\"natural\":\"water\",\"water\":\"lake\"}", true},
        {"{\"natural\":\"water\",\"water\":\"lagoon\"}", true},
        {"{\"natural\":\"water\",\"water\":\"reservoir\"}", true},
        {"{\"natural\":\"water\"}", true},
        {"{\"waterway\":\"riverbank\"}", true},
        {"{\"water\":\"river\"}", true},
        {"{\"water\":\"stream\"}", true},
        {"{\"water\":\"canal\"}", true},
        {"{\"natural\":\"wetland\",\"wetland\":\"marsh\"}", false},
        {"{\"natural\":\"wood\"}", false},
        {"{}", false}
    };
    for (size_t i = 0; i < sizeof areas / sizeof *areas; ++i) {
        string_t *text = string_new_with(areas[i].properties);
        json_t *properties = text ? json_from_text(text) : NULL;
        TEST_ASSERT_TRUE(properties && cfg_database_river_open_water(properties) == areas[i].water,
                         "include mapped lakes, lagoons and reservoirs without flooding marshland");
        json_free(properties); string_free(text);
    }
    static const struct { const char *properties; const char *kind; bool retained; } cases[] = {
        {",\"width\":\"20\"", "river", true},
        {",\"width\":\"2.999\"", "river", false},
        {",\"width\":\"3\"", "river", true},
        {",\"width\":\"299 cm\"", "canal", false},
        {",\"width\":\"3000 mm\"", "stream", true},
        {",\"width\":\"0.00299 km\"", "tidal_channel", false},
        {",\"width\":3", "river", true},
        {",\"width\":\"0.5\"", "river", false},
        {",\"width\":\"1 ft\"", "river", false},
        {",\"width\":\"0'6\\\"\"", "river", false},
        {",\"est_width\":\"0.01\"", "river", true},
        {",\"width\":\"unknown\",\"est_width\":\"0.01\"", "river", true},
        {",\"width\":\"5\"", "river", true},
        {",\"width\":\"25\"", "river", true},
        {",\"width\":\"16 m\"", "river", true},
        {",\"width\":\"15.999\"", "river", true},
        {",\"width\":16", "river", true},
        {",\"width\":\"10\"", "canal", true},
        {",\"width\":\"50\"", "canal", true},
        {",\"width\":\"30\"", "stream", true},
        {",\"width\":\"8\"", "tidal_channel", true},
        {"", "river", true},
        {",\"width\":\"narrow\"", "river", true},
        {",\"width\":\"5;25\"", "river", true},
        {",\"width\":\"5-25\"", "river", true},
        {",\"width\":\"0\"", "river", true},
        {",\"width\":\"-5\"", "river", true},
        {",\"width\":\"NaN\"", "river", true},
        {",\"width\":\"5 furlongs\"", "river", true},
        {",\"width\":\"5,5\"", "river", true},
        {",\"width\":\"0x5\"", "river", true},
        {",\"width\":\"5e-3\"", "river", true},
        {",\"width\":\"5\\u0000;25\"", "river", true},
        {",\"est_width\":\"8\"", "river", true},
        {",\"width\":\"20\",\"est_width\":\"5\"", "river", true},
        {",\"width\":\"5\",\"est_width\":\"20\"", "river", true},
        {",\"width\":\"unknown\",\"est_width\":\"5\"", "river", true},
        {",\"maxwidth\":\"2\"", "canal", true},
        {",\"width\":\"15 m extra\"", "river", true},
        {",\"width\":\"1500 cm\"", "river", true},
        {",\"width\":\"0.015 km\"", "river", true},
        {",\"width\":\"16000 mm\"", "river", true},
        {",\"width\":\"0.01 mi\"", "river", true},
        {",\"width\":\"17 yd\"", "river", true},
        {",\"width\":\"600 in\"", "river", true},
        {",\"width\":\"50 ft\"", "river", true},
        {",\"width\":\"53 ft\"", "river", true},
        {",\"width\":\"52'5\\\"\"", "river", true},
        {",\"width\":\"52'6\\\"\"", "river", true},
        {",\"width\":\"5'12\\\"\"", "river", true},
        {",\"width\":\"5'6\"", "river", true},
        {",\"width\":\" 15.5 m \\t\"", "river", true},
        {",\"width\":null", "river", true},
        {",\"width\":[5,25]", "river", true}
    };
    string_t *text = string_new();
    size_t retained = 0;
    for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
        TEST_ASSERT_TRUE(string_append_format(text,
            "{\"type\":\"Feature\",\"geometry\":{\"type\":\"LineString\",\"coordinates\":[[0.%03zu,0],[0.%03zu,0]]},"
            "\"properties\":{\"@type\":\"way\",\"@id\":%zu,\"@way_nodes\":[%zu,%zu],"
            "\"name\":\"Example River\",\"waterway\":\"%s\"%s}}\n",
            i, i + 1, i + 1000, i + 1, i + 2, cases[i].kind, cases[i].properties) >= 0,
            "write adjacent sections sharing a name and endpoint nodes");
        retained += cases[i].retained;
    }
    file_t *directory = temporary_directory(), *source = child(directory, "widths.geojsonseq");
    sqlite_t *db = test_database(), *installed = test_database();
    TEST_ASSERT_TRUE(text && source && db && installed && file_write_all_text(source, text) &&
                     file_open_read(source) && cfg_database_import_osm_rivers(db, source),
                     "filter individual sections without discarding a river or canal by name or class");
    file_close(source); string_free(text);
    sqlite_stmt_t *q = sqlite_stmt_prepare(db, "select count(*) from rivers_chunk where feature_id=?1");
    for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
        TEST_ASSERT_TRUE(q && sqlite_stmt_bind_int64(q, 1, (int64_t)i + 1000) &&
                         sqlite_stmt_step(q) == SQLITE_STEP_ROW &&
                         sqlite_stmt_column_int(q, 0) == (int)cases[i].retained,
                         "omit only known sub-threshold sections and retain unknown widths");
        sqlite_stmt_reset(q);
    }
    sqlite_stmt_finalize(q);
    TEST_ASSERT_TRUE(cfg_database_publish_rivers(installed, db), "publish width-selected geometry with correct counts");
    globe_t *g = globe_open_databases(NULL, installed, NULL);
    hits_t h = {.west = -1, .south = -1, .east = 1, .north = 1, .valid = true};
    TEST_ASSERT_TRUE(g && globe_river_segments(g, -1, -1, 1, 1, hit, &h) && h.valid && h.count == retained,
                     "wider sections on both sides survive publication and display");
    h = (hits_t){.west = 0.0011, .south = -1, .east = 0.0019, .north = 1, .valid = true};
    TEST_ASSERT_TRUE(globe_river_display_segments(g, h.west, h.south, h.east, h.north, .01, hit, &h) && !h.count,
                     "display simplification does not bridge the omitted narrow section");
    globe_close(g);
    sqlite_close(db); sqlite_close(installed);
    TEST_ASSERT_TRUE(file_delete(source) && file_remove_directory(directory), "clean section-width fixture");
    file_free(source); file_free(directory);
}

static void test_cfg_rivers_storage_budget(void)
{
    sqlite_t *source = test_database(), *installed = test_database();
    TEST_ASSERT_TRUE(cfg_database_river_storage_budget == INT64_C(7000000000) &&
                     cfg_database_river_storage_budget > INT32_MAX, "production budget is 7 decimal GB with 64-bit arithmetic");
    TEST_ASSERT_TRUE(!cfg_database_publish_rivers_limited(installed, source, 0) &&
                     !cfg_database_publish_rivers_limited(installed, source, -1) &&
                     !cfg_database_publish_rivers_limited(installed, source, cfg_database_river_storage_budget + 1),
                     "reject invalid limits or attempts to exceed the production budget");
    TEST_ASSERT_TRUE(source && installed && sqlite_exec_cstr(source,
        "pragma page_size=65536;"
        "create table rivers_dataset(id integer primary key,format_version integer,source text,point_count integer,"
        "feature_count integer,source_point_count integer,storage_tolerance_metres real);"
        "insert into rivers_dataset values(1,2,'OpenStreetMap centreline export / ODbL',257,1,257,8);"
        "create table rivers_chunk(id integer primary key,feature_id integer,kind integer,vertices blob,anchors blob);"),
        "prepare a compact source with a different page size from the destination");
    unsigned char vertices[2056] = {0}, anchors[257];
    memset(anchors, 1, sizeof anchors);
    for (size_t i = 0; i < sizeof anchors; ++i) {
        vertices[i * 8 + 2] = (unsigned char)(i >> 8);
        vertices[i * 8 + 3] = (unsigned char)i;
    }
    sqlite_stmt_t *insert = sqlite_stmt_prepare(source, "insert into rivers_chunk values(?1,?1,1,?2,?3)");
    TEST_ASSERT_TRUE(insert && sqlite_stmt_bind_int64(insert, 1, 1) &&
                     sqlite_stmt_bind_blob(insert, 2, vertices, sizeof vertices) &&
                     sqlite_stmt_bind_blob(insert, 3, anchors, sizeof anchors) &&
                     sqlite_stmt_step(insert) == SQLITE_STEP_DONE, "create the initial valid river layer");
    sqlite_stmt_reset(insert);
    TEST_ASSERT_TRUE(sqlite_exec_cstr(installed,
        "pragma page_size=4096;create table shoreline_sentinel(id integer,payload blob);"
        "insert into shoreline_sentinel values(42,zeroblob(4000000));"
        "create table free_space(payload blob);insert into free_space values(zeroblob(6000000));drop table free_space;") &&
        cfg_database_publish_rivers(installed, source), "unrelated shoreline and free pages do not count as river storage");
    for (int64_t i = 2; i <= 1200; ++i) {
        TEST_ASSERT_TRUE(sqlite_stmt_bind_int64(insert, 1, i) &&
                         sqlite_stmt_step(insert) == SQLITE_STEP_DONE, "append a valid full-sized river chunk");
        sqlite_stmt_reset(insert);
    }
    sqlite_stmt_finalize(insert);
    TEST_ASSERT_TRUE(sqlite_exec_cstr(source,
        "update rivers_dataset set point_count=308400,source_point_count=308400,feature_count=1200"),
        "record all source points before testing destination growth");
    sqlite_stmt_t *q = sqlite_stmt_prepare(source, "select page_count*page_size from pragma_page_count,pragma_page_size");
    TEST_ASSERT_TRUE(q && sqlite_stmt_step(q) == SQLITE_STEP_ROW && sqlite_stmt_column_int64(q, 0) <= 3800000,
                     "candidate is below the cap, so the destination accounting must reject this case");
    sqlite_stmt_finalize(q);
    TEST_ASSERT_TRUE(!cfg_database_publish_rivers_limited(installed, source, 3800000),
                     "exercise destination budget rejection on reused free pages with a small regression limit");
    q = sqlite_stmt_prepare(installed,
        "select point_count,feature_count,(select count(*) from rivers_chunk),"
        "(select id from shoreline_sentinel),(select length(payload) from shoreline_sentinel) from rivers_dataset");
    TEST_ASSERT_TRUE(q && sqlite_stmt_step(q) == SQLITE_STEP_ROW && sqlite_stmt_column_int(q, 0) == 257 &&
                     sqlite_stmt_column_int(q, 1) == 1 && sqlite_stmt_column_int(q, 2) == 1 &&
                     sqlite_stmt_column_int(q, 3) == 42 && sqlite_stmt_column_int(q, 4) == 4000000,
                     "destination budget failure restores its river layer and preserves shoreline bytes");
    sqlite_stmt_finalize(q);
    TEST_ASSERT_TRUE(cfg_database_publish_rivers(installed, source),
                     "production publication no longer rejects river layers above 3.8 MB");
    q = sqlite_stmt_prepare(installed, "select point_count,feature_count from rivers_dataset");
    TEST_ASSERT_TRUE(q && sqlite_stmt_step(q) == SQLITE_STEP_ROW && sqlite_stmt_column_int(q, 0) == 308400 &&
                     sqlite_stmt_column_int(q, 1) == 1200, "all sections survive production publication");
    sqlite_stmt_finalize(q);
    sqlite_close(source); sqlite_close(installed);
}

static void test_cfg_rivers_compressed_input(void)
{
    string_t *text = string_new_with(
        "\036{\"type\":\"Feature\",\"geometry\":{\"type\":\"LineString\",\"coordinates\":[[1,1],[2,2]]},"
        "\"properties\":{\"@type\":\"way\",\"@id\":1,\"@way_nodes\":[1,2],\"waterway\":\"river\",\"note\":\"");
    char padding[65536];
    uint32_t random = 42;
    for (unsigned block = 0; block < 3; ++block) {
        for (size_t i = 0; i < sizeof padding; ++i) {
            random ^= random << 13;
            random ^= random >> 17;
            random ^= random << 5;
            padding[i] = (char)('a' + random % 26);
        }
        TEST_ASSERT_TRUE(text && string_append_bytes(text, padding, sizeof padding) == 0,
                         "force compressed input and decoded records across multiple buffers");
    }
    TEST_ASSERT_TRUE(string_append_cstr(text, "\"}}\n") >= 0, "finish the long valid record");
    file_t *directory = temporary_directory(), *plain = child(directory, "input.geojsonseq");
    file_t *packed = child(directory, "input.geojsonseq.zst");
    sqlite_t *db = test_database();
    TEST_ASSERT_TRUE(db && file_write_all_text(plain, text) && file_compress_zstd(plain, packed, false) &&
                     file_open_read(packed) && cfg_database_import_osm_rivers(db, packed),
                     "stream a complete checksummed compressed sequence directly into the candidate");
    file_close(packed);
    string_free(text);
    file_info_t *info = file_get_info(packed);
    size_t bytes = info ? (size_t)file_info_size(info) : 0;
    TEST_ASSERT_TRUE(bytes > 65536 && bytes < 262144, "exercise multiple compressed input buffers with a bounded fixture");
    file_info_free(info);
    unsigned char *saved = malloc(bytes);
    size_t read = 0, written = 0;
    TEST_ASSERT_TRUE(saved && file_open_read(packed) && file_read(packed, saved, bytes, &read) && read == bytes,
                     "save the bounded frame for malformed and concatenated input checks");
    file_close(packed);
    for (unsigned damage = 0; damage < 4; ++damage) {
        TEST_ASSERT_TRUE(file_open_write(packed) && file_write(packed, saved, bytes, &written) && written == bytes,
                         "restore the original compressed fixture");
        if (!damage) {
            TEST_ASSERT_TRUE(file_truncate(packed, (int64_t)bytes - 1), "truncate the final frame checksum");
        } else if (damage == 1) {
            unsigned char changed = saved[bytes - 1] ^ 1;
            TEST_ASSERT_TRUE(file_seek(packed, -1, FILE_SEEK_END) && file_write(packed, &changed, 1, &written),
                             "damage the final checksum after valid records have been decoded");
        } else if (damage == 2) {
            TEST_ASSERT_TRUE(file_write(packed, "x", 1, &written), "append trailing junk");
        } else {
            TEST_ASSERT_TRUE(file_write(packed, saved, bytes, &written) && written == bytes, "append a second complete frame");
        }
        file_close(packed);
        TEST_ASSERT_TRUE(file_open_read(packed) && !cfg_database_import_osm_rivers(db, packed),
                         "reject truncation, checksum corruption, trailing bytes and concatenated frames");
        file_close(packed);
        sqlite_stmt_t *q = sqlite_stmt_prepare(db, "select point_count,feature_count from rivers_dataset");
        TEST_ASSERT_TRUE(q && sqlite_stmt_step(q) == SQLITE_STEP_ROW && sqlite_stmt_column_int(q, 0) == 2 &&
                         sqlite_stmt_column_int(q, 1) == 1, "late compressed-input failure rolls back the entire replacement");
        sqlite_stmt_finalize(q);
    }
    free(saved);
    sqlite_close(db);
    TEST_ASSERT_TRUE(file_delete(plain) && file_delete(packed) && file_remove_directory(directory),
                     "clean only the disposable compression fixtures");
    file_free(plain); file_free(packed); file_free(directory);
}

static void test_cfg_rivers_numeric_decoder(void)
{
    file_t *directory = temporary_directory(), *source = child(directory, "numeric.geojsonseq");
    sqlite_t *db = test_database();
    string_t *text = string_new_with(
        "{\"properties\":{\"@type\":\"way\",\"@id\":9,\"waterway\":\"river\","
        "\"name\":\"ေပါင်း​ေလာင်းမြစ်\",\"@way_\\u006eodes\":[9223372036854775806,9223372036854775807]},"
        "\"geometry\":null,\"geome\\u0074ry\":{\"type\":\"LineString\",\"coordinates\":null,"
        "\"coordina\\u0074es\":[[1e0,-2.5e-1],[2e0,3.5e-1]]},\"type\":\"Feature\","
        "\"decoy\":{\"geometry\":{\"coordinates\":[[90,90]]},\"@way_nodes\":[0]}}\n");
    TEST_ASSERT_TRUE(db && text && file_write_all_text(source, text) && file_open_read(source) &&
                     cfg_database_import_osm_rivers(db, source),
                     "decode reordered, escaped and duplicate keys, exponents and exact 64-bit node IDs");
    file_close(source);
    string_free(text);
    globe_t *g = globe_open_databases(NULL, db, NULL);
    hits_t h = {.west = 1, .south = -.25, .east = 2, .north = .35, .valid = true};
    TEST_ASSERT_TRUE(g && globe_river_segments(g, 0, -1, 3, 1, hit, &h) && h.count == 1 && h.valid,
                     "numeric decoding preserves coordinates and ignores unrelated nested fields");
    globe_close(g);
    static const struct { const char *coordinates, *nodes; } invalid[] = {
        {"[[01,0],[2,0]]", "[1,2]"},
        {"[[+1,0],[2,0]]", "[1,2]"},
        {"[[1e999,0],[2,0]]", "[1,2]"},
        {"[[181,0],[2,0]]", "[1,2]"},
        {"[[1,91],[2,0]]", "[1,2]"},
        {"[[1,0,3],[2,0]]", "[1,2]"},
        {"[[1,0],[2,0],]", "[1,2]"},
        {"[[1,0][2,0]]", "[1,2]"},
        {"[[1,0],[2,0]]", "[1]"},
        {"[[1,0],[2,0]]", "[1,2,3]"},
        {"[[1,0],[2,0]]", "[1,9223372036854775808]"},
        {"[[1,0],[2,0]]", "[1,2.5]"},
        {"[[1,0],[2,0]]", "[1,2e0]"},
        {"[[1,0],[2,0]]", "[1,0]"},
        {"[[1,0],[2,0]]", "[1,-2]"}
    };
    for (size_t i = 0; i < sizeof invalid / sizeof *invalid; ++i) {
        text = string_sprintf(
            "{\"type\":\"Feature\",\"geometry\":{\"type\":\"LineString\",\"coordinates\":%s},"
            "\"properties\":{\"@type\":\"way\",\"@id\":1,\"waterway\":\"river\",\"@way_nodes\":%s}}\n",
            invalid[i].coordinates, invalid[i].nodes);
        TEST_ASSERT_TRUE(text && file_write_all_text(source, text) && file_open_read(source) &&
                         !cfg_database_import_osm_rivers(db, source), "reject malformed or out-of-range numeric arrays");
        file_close(source);
        string_free(text);
        sqlite_stmt_t *q = sqlite_stmt_prepare(db, "select feature_id from rivers_chunk");
        TEST_ASSERT_TRUE(q && sqlite_stmt_step(q) == SQLITE_STEP_ROW && sqlite_stmt_column_int64(q, 0) == 9 &&
                         sqlite_stmt_step(q) == SQLITE_STEP_DONE, "failed decoding preserves the previous candidate");
        sqlite_stmt_finalize(q);
    }
    sqlite_close(db);
    TEST_ASSERT_TRUE(file_delete(source) && file_remove_directory(directory), "remove disposable decoder fixtures");
    file_free(source); file_free(directory);
}

static bool test_cfg_rivers_plain_install(const char *directory, const string_t *candidate)
{
    string_t *path = string_sprintf("%s/installed.db", directory);
    file_t *file = path ? file_new(path) : NULL;
    sqlite_t *db = path ? sqlite_open_plain(path) : NULL;
    bool ok = file && db && sqlite_exec_cstr(db,
        "create table shoreline_dataset(id,format_version,point_count);"
        "insert into shoreline_dataset values(1,2,7);"
        "create table shoreline_blob(id,vertices);"
        "create table shoreline_chunk(id,level,codec,point_count,byte_offset,byte_count,boundary,blob_id);"
        "create virtual table shoreline_bounds using rtree(id,min_x,max_x,min_y,max_y);");
    sqlite_close(db);
    ok = ok && file_chmod(file, 0600) && !setenv("MARS_GLOBE_DB_PATH", string_c_str(path), 1) &&
         !setenv("MARS_GLOBE_DB_KEY", "ignored-stale-key", 1) && !cfg_globe_river_install(string_c_str(candidate));
    unsigned char header[16];
    size_t count = 0;
    ok = ok && file_open_read(file) && file_read(file, header, sizeof header, &count) &&
         count == sizeof header && !memcmp(header, "SQLite format 3", sizeof header);
    if (file && file_is_open(file)) ok = file_close(file) && ok;
    db = ok ? sqlite_open_plain(path) : NULL;
    globe_t *globe = db ? globe_open_databases(db, db, NULL) : NULL;
    sqlite_stmt_t *check = db ? sqlite_stmt_prepare(db, "select point_count from shoreline_dataset where id=1") : NULL;
    ok = ok && globe && globe_has_layer(globe, GLOBE_LAYER_RIVERS) && globe_has_layer(globe, GLOBE_LAYER_SHORELINE) &&
         check && sqlite_stmt_step(check) == SQLITE_STEP_ROW && sqlite_stmt_column_int(check, 0) == 7;
    sqlite_stmt_finalize(check);
    globe_close(globe);
    sqlite_close(db);
    file_free(file);
    string_free(path);
    return ok;
}

static bool test_cfg_rivers_stage_fixture(const char *directory)
{
    string_t *input_path = string_sprintf("%s/river.geojsonseq", directory);
    string_t *output_path = string_sprintf("%s/candidate.db", directory);
    string_t *text = string_new_with(
        "{\"type\":\"Feature\",\"geometry\":{\"type\":\"LineString\",\"coordinates\":[[1,1],[2,2]]},"
        "\"properties\":{\"@type\":\"way\",\"@id\":1,\"@way_nodes\":[1,2],\"waterway\":\"river\"}}\n");
    file_t *input = input_path ? file_new(input_path) : NULL;
    file_t *output = output_path ? file_new(output_path) : NULL;
    bool ok = input && output && text && !setenv("MARS_GLOBE_DB_KEY", "", 1) &&
              !setenv("MARS_GLOBE_RIVER_DB_KEY", "river-stage-fixture", 1) &&
              file_write_all_text(input, text) && !cfg_globe_river_stage(string_c_str(input_path), string_c_str(output_path));
    file_info_t *info = output ? file_get_info(output) : NULL;
    ok = ok && info && file_info_permissions(info) == 0600 &&
         cfg_globe_river_stage(string_c_str(input_path), string_c_str(output_path)) != 0;
    unsigned char header[16];
    size_t header_bytes = 0;
    ok = ok && file_open_read(output) && file_read(output, header, sizeof header, &header_bytes) &&
         header_bytes == sizeof header && !memcmp(header, "SQLite format 3", sizeof header);
    if (file_is_open(output)) ok = file_close(output) && ok;
    sqlite_t *db = ok ? sqlite_open_plain(output_path) : NULL;
    globe_t *g = db ? globe_open_databases(NULL, db, NULL) : NULL;
    ok = ok && g && globe_has_layer(g, GLOBE_LAYER_RIVERS);
    globe_close(g); sqlite_close(db);
    ok = ok && test_cfg_rivers_plain_install(directory, output_path);
    ok = ok && !cfg_globe_river_resume(string_c_str(input_path), string_c_str(output_path));
    /* Recreate an interrupted junction phase from the independently known source fixture. */
    db = ok ? sqlite_open_plain(output_path) : NULL;
    ok = db && sqlite_exec_cstr(db,
        "delete from rivers_dataset;"
        "create table rivers_node_use(id integer primary key,uses integer not null);"
        "insert into rivers_node_use values(1,1),(2,1);"
        "update rivers_chunk set anchors=NULL,node_ids=x'00000000000000010000000000000002';"
        "update rivers_stage set phase=1,last_chunk=0");
    sqlite_close(db);
    ok = ok && !cfg_globe_river_resume(string_c_str(input_path), string_c_str(output_path));
    db = ok ? sqlite_open_plain(output_path) : NULL;
    ok = db && sqlite_exec_cstr(db,
        "delete from rivers_dataset;create table rivers_node_use(id integer primary key,uses integer not null);"
        "update rivers_stage set phase=2,last_chunk=0");
    sqlite_close(db);
    ok = ok && !cfg_globe_river_resume(string_c_str(input_path), string_c_str(output_path));
    /* Never mix earlier selection policies with a new checkpoint. */
    db = ok ? sqlite_open_plain(output_path) : NULL;
    ok = db && sqlite_exec_cstr(db, "update rivers_stage set version=1");
    sqlite_close(db);
    ok = ok && cfg_globe_river_resume(string_c_str(input_path), string_c_str(output_path)) != 0;
    db = ok ? sqlite_open_plain(output_path) : NULL;
    ok = db && sqlite_exec_cstr(db, "update rivers_stage set version=2");
    sqlite_close(db);
    ok = ok && cfg_globe_river_resume(string_c_str(input_path), string_c_str(output_path)) != 0;
    db = ok ? sqlite_open_plain(output_path) : NULL;
    ok = db && sqlite_exec_cstr(db, "update rivers_stage set version=3");
    sqlite_close(db);
    /* Resume rejects a different source even if it is itself valid GeoJSON. */
    string_t *changed = string_clone(text);
    ok = ok && changed && string_append_cstr(changed, "\n") == 0 && file_write_all_text(input, changed) &&
         cfg_globe_river_resume(string_c_str(input_path), string_c_str(output_path)) != 0 &&
         file_write_all_text(input, text);
    string_free(changed);
    string_t *packed_path = string_sprintf("%s/river.geojsonseq.zst", directory);
    string_t *second_path = string_sprintf("%s/compressed-candidate.db", directory);
    file_t *packed = packed_path ? file_new(packed_path) : NULL;
    ok = ok && packed && second_path && file_compress_zstd(input, packed, false) &&
         !cfg_globe_river_stage(string_c_str(packed_path), string_c_str(second_path));
    file_free(packed); string_free(packed_path); string_free(second_path);
    file_info_free(info); file_free(input); file_free(output);
    string_free(input_path); string_free(output_path); string_free(text);
    return ok;
}

static bool test_cfg_rivers_checkpoint_fixture(const char *directory)
{
    string_t *input_path = string_sprintf("%s/checkpoint.geojsonseq", directory);
    string_t *output_path = string_sprintf("%s/checkpoint.db", directory);
    string_t *key = string_new_with("river-stage-fixture"), *text = string_new();
    file_t *input = input_path ? file_new(input_path) : NULL;
    bool ok = input && output_path && key && text && !setenv("MARS_GLOBE_DB_KEY", string_c_str(key), 1);
    for (unsigned i = 0; ok && i < 10002; ++i)
        ok = string_append_cstr(text,
            "{\"type\":\"Feature\",\"geometry\":{\"type\":\"LineString\",\"coordinates\":[[1,1],[2,2]]},"
            "\"properties\":{\"@type\":\"way\",\"@id\":1,\"@way_nodes\":[1,2],\"waterway\":\"river\"}}\n") == 0;
    ok = ok && file_write_all_text(input, text) &&
         !cfg_globe_river_stage(string_c_str(input_path), string_c_str(output_path));
    sqlite_t *db = ok ? sqlite_open_plain(output_path) : NULL;
    ok = db && sqlite_exec_cstr(db,
        "delete from rivers_dataset;create table rivers_node_use(id integer primary key,uses integer not null);"
        "insert into rivers_node_use values(1,2),(2,2);"
        "update rivers_chunk set anchors=NULL,node_ids=x'00000000000000010000000000000002';"
        "update rivers_stage set phase=1,last_chunk=0;"
        "create trigger checkpoint_stop before update of anchors on rivers_chunk when new.id=1001 "
        "begin select raise(abort,'fixture interruption'); end");
    sqlite_close(db);
    ok = ok && cfg_globe_river_resume(string_c_str(input_path), string_c_str(output_path)) != 0;
    db = ok ? sqlite_open_plain(output_path) : NULL;
    sqlite_stmt_t *check = db ? sqlite_stmt_prepare(db,
        "select phase,last_chunk,(select count(*) from rivers_dataset) from rivers_stage") : NULL;
    ok = check && sqlite_stmt_step(check) == SQLITE_STEP_ROW && sqlite_stmt_column_int(check, 0) == 1 &&
         sqlite_stmt_column_int(check, 1) == 1000 && sqlite_stmt_column_int(check, 2) == 0;
    sqlite_stmt_finalize(check);
    ok = ok && sqlite_exec_cstr(db, "drop trigger checkpoint_stop");
    sqlite_close(db);
    ok = ok && !cfg_globe_river_resume(string_c_str(input_path), string_c_str(output_path));
    db = ok ? sqlite_open_plain(output_path) : NULL;
    ok = db && sqlite_exec_cstr(db,
        "delete from rivers_dataset;delete from rivers_chunk where id>10000;delete from rivers_bounds where id>10000;"
        "create table rivers_node_use(id integer primary key,uses integer not null);"
        "insert into rivers_node_use values(1,2),(2,2);"
        "update rivers_chunk set anchors=NULL,node_ids=x'00000000000000010000000000000002';"
        "update rivers_stage set phase=0,last_chunk=0,records=10000,chunks=10000,features=10000,points=20000,"
        "source_points=0,unknown_width_sections=10000");
    sqlite_close(db);
    ok = ok && !cfg_globe_river_resume(string_c_str(input_path), string_c_str(output_path));
    db = ok ? sqlite_open_plain(output_path) : NULL;
    check = db ? sqlite_stmt_prepare(db,
        "select point_count,feature_count,(select count(*) from rivers_chunk) from rivers_dataset") : NULL;
    ok = check && sqlite_stmt_step(check) == SQLITE_STEP_ROW && sqlite_stmt_column_int(check, 0) == 20004 &&
         sqlite_stmt_column_int(check, 1) == 10002 && sqlite_stmt_column_int(check, 2) == 10002;
    sqlite_stmt_finalize(check); sqlite_close(db);
    file_free(input); string_free(input_path); string_free(output_path); string_free(key); string_free(text);
    return ok;
}

static void test_cfg_rivers_stage(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(test_cfg_rivers_stage_fixture),
                     "staging, resume and installation ignore stale keys and preserve plaintext storage and shorelines");
    TEST_ASSERT_TRUE(test_cfg_isolated(test_cfg_rivers_checkpoint_fixture),
                     "batch checkpoints survive failure and input replay does not duplicate records");
}

/* Register installer regressions with the sequential configuration runner. */
void test_cfg_rivers_cases(void)
{
    TEST_RUN_IN_GROUP(test_cfg_rivers_main_selection, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_rivers_osm_import_and_display, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_rivers_storage_detail, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_rivers_neighbourhoods, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_rivers_storage_chunks, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_rivers_storage_budget, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_rivers_section_widths, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_rivers_compressed_input, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_rivers_stage, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_rivers_numeric_decoder, tests, NULL);
}
