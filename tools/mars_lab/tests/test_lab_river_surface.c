/**
 * @file test_lab_river_surface.c
 * @brief Native geographic crop, river path and cache invalidation regressions.
 *
 * Uses a disposable plaintext globe with real photographic metadata and synthetic river geometry. Tests the
 * same format dispatch used by HTTP without accessing installed data or requiring a browser or network.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "globe-t.h"
#include "string-t.h"
#define MARS_SHARED_LAB_CALENDAR_INTERNAL_ACCESS
#include "internal/lab_calendar_internal.h"
#include "test_harness.h"
#include "test_lab_support.h"

static json_t *surface_request(const char *bounds, const char *format, const char *key, unsigned *status)
{
    string_t *text = string_sprintf("{%s,\"format\":\"%s\",\"key\":\"%s\"}", bounds, format, key ? key : "");
    json_t *payload = text ? test_lab_json(string_c_str(text)) : NULL;
    json_t *result = payload ? lab_cal_rivers(payload, status) : NULL;
    json_free(payload);
    string_free(text);
    return result;
}

static bool surface_crops(const json_t *plan, size_t expected)
{
    const json_t *photos = test_lab_member(plan, "photos");
    if (json_array_size(photos) != expected) return false;
    double area = 0;
    for (size_t i = 0; i < expected; ++i) {
        const json_t *photo = json_array_get(photos, i);
        double rectangle[2][4];
        for (unsigned part = 0; part < 2; ++part) {
            const json_t *values = test_lab_member(photo, part ? "target" : "source");
            if (json_array_size(values) != 4) return false;
            for (unsigned j = 0; j < 4; ++j) {
                const string_t *text = json_number_text(json_array_get(values, j));
                if (!text || !lab_cal_number(string_c_str(text), &rectangle[part][j])) return false;
            }
            double *r = rectangle[part], limit = part ? 2048 : 1800;
            if (r[0] < 0 || r[1] < 0 || r[2] <= 0 || r[3] <= 0 ||
                r[0] + r[2] > limit + 1e-8 || r[1] + r[3] > limit + 1e-8) return false;
        }
        area += rectangle[1][2] * rectangle[1][3];
    }
    return fabs(area - 2048.0 * 2048) < .01;
}

static bool surface_widths(const string_t *path, const char *bounds)
{
    sqlite_t *db = sqlite_open_plain(path);
    bool ok = db && sqlite_exec_cstr(db,
        "create table rivers_width(feature_id integer primary key,metres real);"
        "insert into rivers_width values(1,100);"
        "create table rivers_bank(id integer primary key,rings blob);"
        "create virtual table rivers_bank_bounds using rtree_i32(id,min_x,max_x,min_y,max_y)");
    /* A square river area with an island; outer and inner rings must remain distinct. */
    static const int32_t words[] = {2, 5, -500000,-500000, 1500000,-500000, 1500000,1500000,
        -500000,1500000, -500000,-500000, 5, 250000,250000, 250000,750000, 750000,750000,
        750000,250000, 250000,250000};
    unsigned char blob[sizeof words];
    for (size_t i = 0; i < sizeof words / sizeof *words; ++i) {
        uint32_t n = (uint32_t)words[i];
        blob[4*i] = n >> 24; blob[4*i+1] = n >> 16; blob[4*i+2] = n >> 8; blob[4*i+3] = n;
    }
    sqlite_stmt_t *insert = ok ? sqlite_stmt_prepare(db, "insert into rivers_bank values(1,?1)") : NULL;
    ok = insert && sqlite_stmt_bind_blob(insert, 1, blob, sizeof blob) && sqlite_stmt_step(insert) == SQLITE_STEP_DONE &&
         sqlite_exec_cstr(db, "insert into rivers_bank_bounds values(1,-500000,1500000,-500000,1500000)");
    sqlite_stmt_finalize(insert); sqlite_close(db);
    unsigned status = 0;
    for (unsigned i = 0; ok && i < 3; ++i) {
        json_t *plan = surface_request(bounds, "surface-plan", NULL, &status);
        json_t *data = surface_request(bounds, "surface-data", test_lab_text(plan, "key"), &status);
        const char *fill = test_lab_text(data, "fill");
        const json_t *banks = test_lab_member(data, "banks");
        const string_t *bank = json_string_value(json_array_get(banks, 0));
        const char *first = bank ? strchr(string_c_str(bank), 'M') : NULL;
        ok = status == 200 && json_array_size(banks) == 1 && first && strchr(first + 1, 'M') &&
             (i == 0 ? *fill && strstr(fill, "A") && !*test_lab_text(data, "path") :
              i == 1 ? !*fill && !*test_lab_text(data, "path") : !*fill && *test_lab_text(data, "path"));
        json_free(plan); json_free(data);
        data = surface_request(bounds, "", "", &status);
        ok = ok && status == 200 && strstr(test_lab_text(data, "image"), "fill-rule=\"nonzero\"");
        json_free(data);
        db = sqlite_open_plain(path);
        ok = ok && db && sqlite_exec_cstr(db, i == 0 ? "update rivers_width set metres=3" : "delete from rivers_width");
        sqlite_close(db);
    }
    db = sqlite_open_plain(path);
    /* A mapped one-metre pond is retained in storage and becomes visible only on a closer view. */
    static const int32_t pond_words[] = {1, 5, 100000,100000, 100010,100000, 100010,100010,
        100000,100010, 100000,100000};
    unsigned char pond_blob[sizeof pond_words];
    for (size_t i = 0; i < sizeof pond_words / sizeof *pond_words; ++i) {
        uint32_t n = (uint32_t)pond_words[i];
        pond_blob[4*i] = n >> 24; pond_blob[4*i+1] = n >> 16; pond_blob[4*i+2] = n >> 8; pond_blob[4*i+3] = n;
    }
    insert = db ? sqlite_stmt_prepare(db, "insert into rivers_bank values(2,?1)") : NULL;
    ok = ok && insert && sqlite_stmt_bind_blob(insert, 1, pond_blob, sizeof pond_blob) &&
         sqlite_stmt_step(insert) == SQLITE_STEP_DONE &&
         sqlite_exec_cstr(db, "insert into rivers_bank_bounds values(2,100000,100010,100000,100010)");
    sqlite_stmt_finalize(insert);
    globe_t *pond_globe = db ? globe_open_databases(NULL, db, NULL) : NULL;
    const double far_bounds[] = {-1, -1, 2, 2}, near_bounds[] = {.099, .099, .101, .101};
    json_t *far_banks = pond_globe ? lab_cal_river_banks(pond_globe, far_bounds) : NULL;
    json_t *near_banks = pond_globe ? lab_cal_river_banks(pond_globe, near_bounds) : NULL;
    ok = ok && json_array_size(far_banks) == 1 && json_array_size(near_banks) == 2;
    json_free(far_banks); json_free(near_banks); globe_close(pond_globe);
    ok = ok && db && sqlite_exec_cstr(db, "insert into rivers_width values(1,-2)");
    globe_t *g = db ? globe_open_databases(NULL, db, NULL) : NULL;
    double width;
    ok = ok && g && !globe_river_width_metres(g, 1, &width);
    ok = ok && !globe_river_water_complete(g, 0, 0, 1, 1);
    globe_close(g);
    ok = ok && sqlite_exec_cstr(db, "delete from rivers_width");
    ok = ok && sqlite_exec_cstr(db,
        "create virtual table rivers_water_coverage using rtree_i32(id,min_x,max_x,min_y,max_y);"
        "insert into rivers_water_coverage values(1,-1000000,2000000,-1000000,2000000)");
    g = db ? globe_open_databases(NULL, db, NULL) : NULL;
    ok = ok && g && globe_river_water_complete(g, 0, 0, 1, 1) &&
         !globe_river_water_complete(g, -2, 0, 1, 1) && !globe_river_water_complete(g, 0, 0, 3, 1) &&
         !globe_river_water_complete(g, 179, 0, 181, 1) && !globe_river_water_complete(g, NAN, 0, 1, 1);
    globe_close(g);
    sqlite_close(db);
    json_t *plan = surface_request(bounds, "surface-plan", NULL, &status);
    json_t *data = surface_request(bounds, "surface-data", test_lab_text(plan, "key"), &status);
    bool complete = false;
    ok = ok && status == 200 && json_bool_value(test_lab_member(data, "complete"), &complete) && complete &&
         !*test_lab_text(data, "path") && !*test_lab_text(data, "fill") &&
         json_array_size(test_lab_member(data, "banks")) == 1;
    json_free(data); json_free(plan);
    const char *overview = "\"west\":-10,\"south\":-10,\"east\":10,\"north\":10";
    plan = surface_request(overview, "surface-plan", NULL, &status);
    data = surface_request(overview, "surface-data", test_lab_text(plan, "key"), &status);
    ok = ok && status == 200 && json_array_size(test_lab_member(data, "banks")) == 1 &&
         !*test_lab_text(data, "path") && !*test_lab_text(data, "fill");
    json_free(data); json_free(plan);
    return ok;
}

static bool surface_fixture(const char *directory)
{
    if (!test_lab_texture_database(directory)) return false;
    unsigned initial_status = 0;
    const char *welsh_bounds = "\"west\":-3.74,\"south\":52.30,\"east\":-3.70,\"north\":52.32";
    json_t *initial = surface_request(welsh_bounds, "surface-plan", NULL, &initial_status);
    bool imagery_only = initial_status == 200 && json_array_size(test_lab_member(initial, "photos")) == 2;
    json_t *initial_data = surface_request(welsh_bounds, "surface-data", test_lab_text(initial, "key"), &initial_status);
    imagery_only = imagery_only && initial_status == 200 && !*test_lab_text(initial_data, "path") &&
                   json_array_size(test_lab_member(initial_data, "banks")) == 0;
    json_free(initial_data); json_free(initial);
    string_t *path = string_new_with(getenv("MARS_GLOBE_DB_PATH"));
    sqlite_t *db = path ? sqlite_open_plain(path) : NULL;
    bool ok = imagery_only && db && sqlite_exec_cstr(db,
        "create table rivers_dataset(id,format_version,point_count,source);"
        "insert into rivers_dataset values(1,1,2,'surface fixture');"
        "create table rivers_chunk(id integer primary key,feature_id,kind,vertices);"
        "insert into rivers_chunk values(1,1,1,x'0000000000000000000f4240000f4240');"
        "create virtual table rivers_bounds using rtree_i32(id,min_x,max_x,min_y,max_y);"
        "insert into rivers_bounds values(1,0,1000000,0,1000000);");
    sqlite_close(db);
    static const char *const bounds[] = {
        "\"west\":-1,\"south\":-1,\"east\":2,\"north\":2",
        "\"west\":179,\"south\":-1,\"east\":181,\"north\":1",
        "\"west\":-181,\"south\":-1,\"east\":-179,\"north\":1",
        "\"west\":0,\"south\":60,\"east\":30,\"north\":90"
    };
    unsigned status = 0;
    json_t *regional = surface_request("\"west\":-3.74,\"south\":52.30,\"east\":-3.70,\"north\":52.32",
                                      "surface-plan", NULL, &status);
    const json_t *regional_photos = test_lab_member(regional, "photos");
    ok = ok && status == 200 && json_array_size(regional_photos) == 2 &&
         !strcmp(test_lab_text(json_array_get(regional_photos, 1), "url"), "/earth_regions/00.jpg");
    json_free(regional);
    db = sqlite_open_plain(path);
    ok = ok && db && sqlite_exec_cstr(db,
        "insert into globe_texture_region values(1,-5.51,51.24,-2.61,53.493333333333)");
    sqlite_close(db);
    regional = surface_request("\"west\":-5.5,\"south\":51.3,\"east\":-2.7,\"north\":53.4",
                               "surface-plan", NULL, &status);
    regional_photos = test_lab_member(regional, "photos");
    ok = ok && status == 200 && json_array_size(regional_photos) == 2 &&
         !strcmp(test_lab_text(json_array_get(regional_photos, 1), "url"), "/earth_regions/01.jpg");
    json_free(regional);
    regional = surface_request(welsh_bounds, "surface-plan", NULL, &status);
    ok = ok && status == 200 && json_array_size(test_lab_member(regional, "photos")) == 3;
    json_free(regional);
    string_t *key = NULL;
    for (unsigned i = 0; ok && i < 4; ++i) {
        json_t *plan = surface_request(bounds[i], "surface-plan", NULL, &status);
        ok = status == 200 && test_lab_ok(plan, true) && surface_crops(plan, i == 3 ? 1 : 4);
        if (!i) key = string_new_with(test_lab_text(plan, "key"));
        if (i == 1 || i == 2) {
            const json_t *photos = test_lab_member(plan, "photos");
            ok = ok && !strcmp(test_lab_text(json_array_get(photos, 0), "url"), "/earth_tiles/11-02.jpg") &&
                 !strcmp(test_lab_text(json_array_get(photos, 1), "url"), "/earth_tiles/00-02.jpg");
        }
        json_free(plan);
    }
    json_t *data = surface_request(bounds[0], "surface-data", key ? string_c_str(key) : "", &status);
    ok = ok && status == 200 && strstr(test_lab_text(data, "path"), "M682.667 1365.333L1365.333 682.667");
    json_free(data);
    data = surface_request(bounds[0], "surface-data", "obsolete", &status);
    ok = status == 409 && ok;
    json_free(data);
    data = surface_request("\"west\":0,\"south\":0,\"east\":31,\"north\":1", "surface-plan", NULL, &status);
    ok = status == 400 && ok;
    json_free(data);
    ok = surface_widths(path, bounds[0]) && ok;
    globe_reader_t *reader = globe_reader_new();
    string_t *before = globe_reader_identity(reader), *same = globe_reader_identity(reader);
    ok = before && same && string_byte_length(before) == 64 && !string_compare(before, same) && ok;
    db = sqlite_open_plain(path);
    ok = db && sqlite_exec_cstr(db, "update rivers_chunk set vertices=x'00'") && ok;
    sqlite_close(db);
    string_t *after = globe_reader_identity(reader);
    ok = before && after && string_compare(before, after) && ok;
    data = surface_request(bounds[0], "surface-data", key ? string_c_str(key) : "", &status);
    ok = status == 409 && ok;
    json_free(data);
    data = surface_request(bounds[0], "surface-plan", NULL, &status);
    ok = status == 200 && surface_crops(data, 4) && ok; /* Planning never decodes river vertices. */
    json_free(data);
    setenv("MARS_GLOBE_DB_PATH", "/nonexistent/mars-surface-fixture.db", 1);
    string_t *missing = globe_reader_identity(reader);
    ok = !missing && ok;
    string_free(missing); string_free(before); string_free(same); string_free(after); string_free(key); string_free(path);
    globe_reader_free(reader);
    return ok;
}

static void test_lab_river_surface(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(surface_fixture),
                     "photographic river plans wrap at the date line and invalidate cached geometry after database changes");
}

/* Register native regional surface preparation independently of browser rasterisation. */
void test_lab_river_surface_cases(void)
{
    TEST_RUN_IN_GROUP(test_lab_river_surface, tests, NULL);
}

static bool surface_readme_fixture(const char *directory)
{
    if (!test_lab_texture_database(directory)) return false;
    /* README example: obtain an owned cache identity without reading the full database. */
    globe_reader_t *reader = globe_reader_new();
    string_t *identity = globe_reader_identity(reader);
    printf("Cache identity available: %s\n", identity ? "yes" : "no");
    bool ok = identity && string_byte_length(identity) == 64;
    string_free(identity);
    globe_reader_free(reader);
    return ok;
}

static void example_globe_reader_identity(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(surface_readme_fixture), "README cache identity output uses a readable globe");
}

/* Run the documented ownership example after ordinary regressions. */
void test_lab_river_surface_readme_cases(void)
{
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_globe_reader_identity, readme_examples, "globe,readme,output");
}
