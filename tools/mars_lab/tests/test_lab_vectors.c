/**
 * @file test_lab_vectors.c
 * @brief Isolated native shoreline-vector transport regressions.
 *
 * Imports synthetic nested rings into a private encrypted globe database, then
 * exercises world and regional vector selection and near-shore observer policy without astronomy or network access.
 */
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "file-t.h"
#include "globe-t.h"
#include "sqlite-t.h"
#include "webgl-t.h"
#include "lab_surface_service.h"
#define MARS_SHARED_LAB_CALENDAR_INTERNAL_ACCESS
#include "internal/lab_calendar_internal.h"
#include "test_harness.h"
#include "test_lab_support.h"

static void vector_integer(unsigned char *bytes, int32_t value)
{
    uint32_t n = (uint32_t)value;
    bytes[0] = n >> 24; bytes[1] = n >> 16; bytes[2] = n >> 8; bytes[3] = n;
}

static bool vector_square(file_t *file, int level, int half)
{
    unsigned char bytes[84] = {0};
    vector_integer(bytes, level); vector_integer(bytes + 4, 5); vector_integer(bytes + 8, 15 * 256 + level);
    vector_integer(bytes + 12, -half); vector_integer(bytes + 16, half);
    vector_integer(bytes + 20, -half); vector_integer(bytes + 24, half);
    const int x[] = {-half, half, half, -half, -half}, y[] = {-half, -half, half, half, -half};
    for (unsigned i = 0; i < 5; ++i) {
        vector_integer(bytes + 44 + i * 8, x[i]); vector_integer(bytes + 48 + i * 8, y[i]);
    }
    size_t written = 0;
    return file_write(file, bytes, sizeof bytes, &written) && written == sizeof bytes;
}

static bool vector_response(const json_t *response, bool single)
{
    string_t *key = string_new_with("vectors"), *raw_key = string_new_with("$lab_binary");
    const json_t *wrapper = json_object_get(response, key);
    const json_t *value = wrapper ? json_object_get(wrapper, raw_key) : NULL;
    const string_t *raw = value ? json_string_value(value) : NULL;
    size_t length = raw ? string_byte_length(raw) : 0;
    const unsigned char *bytes = raw ? (const unsigned char *)string_c_str(raw) : NULL;
    bool ok = length >= 4 && !memcmp(bytes, "MSH1", 4) && (length - 4) % 17 == 0;
    unsigned counts[4] = {0};
    for (size_t i = 4; ok && i < length; i += 17) {
        unsigned tag = bytes[i];
        if (tag >= 1 && tag <= 3) ++counts[tag];
        else if (tag != 0 && tag != 16) ok = false;
    }
    ok = ok && counts[1] && counts[2] && counts[3] && (!single || counts[1] == 1);
    string_free(key); string_free(raw_key);
    return ok;
}

static bool vector_surface_policy(void)
{
    const struct { const char *latitude, *surface, *horizon, *location; } cases[] = {
        {"5.01", "Coastal water", "false", "Custom — Coastal water"},
        {"5.1", "Sea", "true", "Custom — Offshore"},
        {"5.001", "Coastal", "false", "Custom — Coastal"},
        {"4", "Land", "false", "Custom — Land"},
        {"2", "Inland water", "false", "Custom — Inland water"}
    };
    bool ok = true;
    for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
        json_t *options = test_lab_json("{\"lon\":\"0\",\"elevation\":\"39\",\"jurisdiction\":\"RO\"}");
        lab_cal_set(options, "lat", cases[i].latitude);
        lab_cal_surface(options);
        string_t *location = lab_cal_observer_location(options, "", !strcmp(cases[i].horizon, "true"));
        ok = ok && !strcmp(lab_cal_text(options, "observer_surface"), cases[i].surface) &&
             !strcmp(lab_cal_text(options, "sea_horizon"), cases[i].horizon) &&
             !strcmp(lab_cal_text(options, "elevation"), "39") &&
             !*lab_cal_observer_jurisdiction(options) && location &&
             !strcmp(string_c_str(location), cases[i].location);
        string_free(location);
        json_free(options);
    }
    return ok;
}

static bool vector_fixture(const char *directory)
{
    string_t *path = string_sprintf("%s/globe.db", directory), *input_path = string_sprintf("%s/rings.bin", directory);
    string_t *key = string_new_with("vector-fixture");
    file_t *input = file_new(input_path);
    sqlite_t *db = sqlite_open_encrypted(path, key);
    bool ok = db && input && file_open(input, FILE_MODE_CREATE_NEW, FILE_ACCESS_WRITE) &&
        vector_square(input, 1, 5000000) && vector_square(input, 2, 3000000) && vector_square(input, 3, 1000000) &&
        file_close(input) && file_open_read(input) && globe_import_shoreline(db, input);
    file_close(input); sqlite_close(db);
    ok = ok && !setenv("MARS_GLOBE_DB_PATH", string_c_str(path), 1) && !setenv("MARS_GLOBE_DB_KEY", string_c_str(key), 1);
    globe_reader_t *reader = globe_reader_new();
    globe_t *first = globe_reader_get(reader);
    ok = ok && first && globe_reader_get(reader) == first;
    setenv("MARS_GLOBE_DB_KEY", "wrong-fixture-key", 1);
    ok = !globe_reader_get(reader) && ok;
    setenv("MARS_GLOBE_DB_KEY", string_c_str(key), 1);
    ok = globe_reader_get(reader) && ok;
    setenv("MARS_GLOBE_DB_PATH", "/nonexistent/mars-globe-fixture.db", 1);
    ok = !globe_reader_get(reader) && ok;
    setenv("MARS_GLOBE_DB_PATH", string_c_str(path), 1);
    ok = globe_reader_get(reader) && ok;
    globe_reader_free(reader);
    ok = vector_surface_policy() && ok;
    ok = lab_cal_surface_open() && ok;
    const char *requests[] = {
        "{\"west\":-6,\"south\":-6,\"east\":6,\"north\":6,\"format\":\"vectors\"}",
        "{\"west\":-38,\"south\":-22,\"east\":36,\"north\":22,\"format\":\"vectors\"}",
        "{\"west\":-180,\"south\":-90,\"east\":180,\"north\":90,\"format\":\"vectors\"}"
    };
    for (unsigned i = 0; ok && i < sizeof requests / sizeof *requests; ++i) {
        json_t *payload = test_lab_json(requests[i]);
        unsigned status = 200;
        json_t *response = lab_cal_globe(payload, &status);
        ok = response && status == 200 && vector_response(response, i == 0);
        json_free(response); json_free(payload);
        if (ok && i == 0) {
            db = sqlite_open_encrypted(path, key);
            ok = db && sqlite_exec_cstr(db, "update shoreline_chunk set codec=999 where polygon_id=1");
            sqlite_close(db);
            payload = test_lab_json(requests[0]);
            status = 200;
            response = lab_cal_globe(payload, &status);
            ok = ok && response && status == 503;
            json_free(response); json_free(payload);
            db = sqlite_open_encrypted(path, key);
            ok = db && sqlite_exec_cstr(db, "update shoreline_chunk set codec=0 where polygon_id=1") && ok;
            sqlite_close(db);
        }
    }
    lab_cal_surface_close();
    ok = vector_surface_policy() && ok;
    pid_t owner = ok ? lab_surface_service_start(2) : -1;
    ok = ok && owner > 0;
    for (unsigned i = 0; ok && i < sizeof requests / sizeof *requests; ++i) {
        pid_t client = fork();
        if (!client) {
            lab_surface_service_select(i % 2);
            json_t *payload = test_lab_json(requests[i]);
            unsigned status = 200;
            json_t *response = lab_surface_service_request(payload, &status);
            bool valid = response && status == 200 && vector_response(response, i == 0);
            json_free(response); json_free(payload);
            payload = test_lab_json("{\"west\":999}");
            response = lab_surface_service_request(payload, &status);
            valid = valid && response && status == 400;
            json_free(response); json_free(payload);
            _exit(valid ? 0 : 1);
        }
        int status = 0;
        ok = client > 0 && waitpid(client, &status, 0) == client && WIFEXITED(status) && !WEXITSTATUS(status);
    }
    lab_surface_service_select(8);
    if (owner > 0) {
        int status = 0;
        ok = waitpid(owner, &status, 0) == owner && WIFEXITED(status) && !WEXITSTATUS(status) && ok;
    }
    file_free(input); string_free(input_path); string_free(path); string_free(key);
    return ok;
}

static void test_lab_shoreline_vectors(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(vector_fixture), "native vector transport preserves nested rings without imagery");
}

static void test_lab_native_mask_plan(void)
{
    webgl_mask_t *mask = webgl_mask_new(4096);
    bool ok = mask != NULL;
    /* Both winding directions must overlap as a union, not cancel or overflow stencil. */
    for (unsigned i = 0; ok && i < 256; ++i) {
        double s = i % 2 ? 1 : -1;
        ok = webgl_mask_begin(mask, 1) && webgl_mask_point(mask, -1, -s) &&
             webgl_mask_point(mask, 1, -s) && webgl_mask_point(mask, 1, s) &&
             webgl_mask_point(mask, -1, s) && webgl_mask_end(mask);
    }
    size_t count = 0, batches = 0;
    const float *vertices = webgl_mask_vertices(mask, &count);
    const webgl_mask_batch_t *commands = webgl_mask_batches(mask, &batches);
    ok = ok && count == 1536 && batches == 3 && commands && commands[0].count == 127 * 6;
    for (size_t i = 0; ok && i < count; i += 3) {
        const float *p = vertices + i * 2;
        ok = (p[2] - p[0]) * (p[5] - p[1]) - (p[3] - p[1]) * (p[4] - p[0]) > 0;
    }
    ok = ok && !webgl_mask_point(mask, 0, 0) && !webgl_mask_vertices(mask, &count) && !count;
    webgl_mask_reset(mask);
    ok = webgl_mask_begin(mask, 4) && !webgl_mask_point(mask, NAN, 0) && ok;
    webgl_mask_reset(mask);
    ok = webgl_mask_begin(mask, 2) && webgl_mask_end(mask) && !webgl_mask_begin(mask, 1) && ok;
    webgl_mask_free(mask);
    mask = webgl_mask_new(3);
    ok = mask && webgl_mask_begin(mask, 1) && webgl_mask_point(mask, 0, 0) &&
         webgl_mask_point(mask, 1, 0) && webgl_mask_point(mask, 1, 1) &&
         !webgl_mask_point(mask, 0, 1) && !webgl_mask_batches(mask, &batches) && ok;
    webgl_mask_free(mask);
    TEST_ASSERT_TRUE(ok, "native GPU plans bound memory and stencil winding without coordinate formatting");
}

/* Register native transport coverage; browser tests verify projection and live SVG. */
void test_lab_vector_cases(void)
{
    TEST_RUN_IN_GROUP(test_lab_shoreline_vectors, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_native_mask_plan, tests, NULL);
}
