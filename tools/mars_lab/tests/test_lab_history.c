/**
 * @file test_lab_history.c
 * @brief Isolated persistence, revision and quota regressions for worksheet history.
 *
 * Uses only temporary private stores. Exercises independent browsers and modes,
 * stale writers, malformed snapshots, payload limits, eviction and database caps.
 */
#include <stdio.h>
#include <string.h>

#include "array-t.h"
#include "file-t.h"
#include "sqlite-t.h"
#include "lab_history.h"
#include "lab_state.h"
#include "lab_wire.h"
#include "test_harness.h"
#include "test_lab_support.h"

static bool history_test_set(json_t *object, const char *name, json_t *value)
{
    string_t *key = string_new_with(name);
    bool ok = key && value && json_object_set(object, key, value);
    string_free(key); json_free(value);
    return ok;
}

static json_t *history_test_payload(const char *mode, size_t padding)
{
    string_t *text = string_sprintf("{\"state\":{\"mode\":\"%s\",\"text\":\"", mode);
    char chunk[1024]; memset(chunk, 'x', sizeof chunk);
    while (text && padding) {
        size_t size = padding < sizeof chunk ? padding : sizeof chunk;
        string_append_bytes(text, chunk, size); padding -= size;
    }
    if (text) string_append_cstr(text, "\"}}");
    json_t *state = text ? test_lab_json(string_c_str(text)) : NULL;
    array_t *bytes = state ? lab_wire_encode(state) : NULL;
    json_t *payload = bytes ? lab_wire_binary(array_get(bytes, 0), array_size(bytes)) : NULL;
    string_free(text); json_free(state); array_destroy(bytes);
    return payload;
}

static json_t *history_test_request(unsigned client, unsigned session, unsigned mode, unsigned revision,
                                     unsigned epoch, size_t padding, unsigned count)
{
    string_t *text = string_sprintf("{\"action\":\"save\",\"client\":\"%032x\",\"session\":\"%032x\","
        "\"mode\":%u,\"revision\":%u,\"epoch\":\"%032x\",\"history\":{\"version\":1,\"back\":[],\"forward\":[]}}",
        client, session, mode, revision, epoch);
    json_t *request = text ? test_lab_json(string_c_str(text)) : NULL;
    json_t *history = (json_t *)test_lab_member(request, "history");
    const char *name = mode == 6 ? "almanac" : "expression";
    json_t *payload = history_test_payload(name, padding);
    json_t *back = (json_t *)test_lab_member(history, "back");
    bool ok = request && payload;
    for (unsigned i = 0; ok && i < count; ++i) ok = json_array_append(back, payload);
    ok = history_test_set(history, "current", payload) && ok;
    string_free(text);
    if (!ok) { json_free(request); return NULL; }
    return request;
}

static bool history_test_status(json_t *request, unsigned expected)
{
    unsigned status = 0;
    json_t *reply = request ? lab_history_request(request, &status) : NULL;
    bool ok = reply && status == expected && test_lab_ok(reply, expected == 200);
    if (!ok) string_fprintf(stderr, "History status: expected %u, actual %u\n", expected, status);
    json_free(reply); json_free(request);
    return ok;
}

static json_t *history_test_load(unsigned client)
{
    string_t *text = string_sprintf("{\"action\":\"load\",\"client\":\"%032x\",\"session\":\"%032x\"}", client, 99);
    json_t *request = text ? test_lab_json(string_c_str(text)) : NULL;
    unsigned status = 0;
    json_t *result = request ? lab_history_request(request, &status) : NULL;
    string_free(text); json_free(request);
    if (status != 200) { json_free(result); return NULL; }
    return result;
}

static bool history_round_trip(const char *directory)
{
    (void)directory;
    bool ok = history_test_status(history_test_request(1, 11, 0, 0, 0, 20, 2), 200) &&
              history_test_status(history_test_request(2, 22, 0, 0, 0, 30, 1), 200) &&
              history_test_status(history_test_request(1, 11, 6, 0, 0, 40, 0), 200);
    json_t *a = history_test_load(1), *b = history_test_load(2);
    ok = a && b && json_array_size(test_lab_member(a, "records")) == 2 &&
         json_array_size(test_lab_member(b, "records")) == 1 && ok;
    const json_t *record = json_array_get(test_lab_member(a, "records"), 0);
    ok = json_array_size(test_lab_member(test_lab_member(record, "history"), "back")) == 2 && ok;
    json_free(a); json_free(b);
    ok = history_test_status(history_test_request(1, 33, 0, 1, 11, 50, 1), 200) && ok;
    ok = history_test_status(history_test_request(1, 11, 0, 1, 11, 60, 1), 409) && ok;
    ok = history_test_status(history_test_request(1, 44, 0, 0, 0, 70, 1), 409) && ok;
    ok = history_test_status(history_test_request(1, 44, 0, 2, 11, 262144, 0), 400) && ok;
    ok = history_test_status(history_test_request(1, 44, 0, 2, 11, 10, 65), 400) && ok;
    ok = history_test_status(history_test_request(1, 44, 0, 2, 11, 220000, 4), 413) && ok;
    json_t *invalid = history_test_request(1, 44, 0, 2, 11, 10, 1);
    history_test_set((json_t *)test_lab_member(invalid, "history"), "current", history_test_payload("almanac", 2));
    ok = history_test_status(invalid, 400) && ok;
    string_t *path = lab_state_private_path("mars_lab_history.db");
    file_t *file = path ? file_new(path) : NULL;
    file_info_t *info = file ? file_get_info(file) : NULL;
    sqlite_t *db = path ? sqlite_open_plain(path) : NULL;
    sqlite_stmt_t *query = db ? sqlite_stmt_prepare(db,
        "SELECT revision FROM history_v1 WHERE client='00000000000000000000000000000001' AND mode=0") : NULL;
    ok = info && (file_info_permissions(info) & 0777) == 0600 && query &&
         sqlite_stmt_step(query) == SQLITE_STEP_ROW && sqlite_stmt_column_int(query, 0) == 2 && ok;
    sqlite_stmt_finalize(query); sqlite_close(db); file_info_free(info); file_free(file); string_free(path);
    return ok;
}

static bool history_eviction(const char *directory)
{
    (void)directory;
    bool ok = true;
    for (unsigned client = 1; ok && client <= 20; ++client)
        ok = history_test_status(history_test_request(client, client + 100, 0, 0, 0, 195000, 4), 200);
    json_t *old = history_test_load(1), *latest = history_test_load(20);
    ok = old && latest && json_array_size(test_lab_member(old, "records")) == 0 &&
         json_array_size(test_lab_member(latest, "records")) == 1 && ok;
    json_free(old); json_free(latest);
    ok = history_test_status(history_test_request(1, 333, 0, 0, 0, 10, 1), 200) && ok;
    ok = history_test_status(history_test_request(1, 101, 0, 1, 101, 10, 1), 409) && ok;
    string_t *path = lab_state_private_path("mars_lab_history.db");
    sqlite_t *db = path ? sqlite_open_plain(path) : NULL;
    sqlite_stmt_t *query = db ? sqlite_stmt_prepare(db, "SELECT sum(length(payload)),count(*) FROM history_v1") : NULL;
    ok = query && sqlite_stmt_step(query) == SQLITE_STEP_ROW && sqlite_stmt_column_int64(query, 0) <= 16777216 &&
         sqlite_stmt_column_int(query, 1) <= 448 && ok;
    sqlite_stmt_finalize(query);
    query = db ? sqlite_stmt_prepare(db, "PRAGMA page_count") : NULL;
    ok = query && sqlite_stmt_step(query) == SQLITE_STEP_ROW && sqlite_stmt_column_int(query, 0) <= 8192 && ok;
    sqlite_stmt_finalize(query); sqlite_close(db); string_free(path);
    return ok;
}

static void test_lab_history_round_trip(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(history_round_trip), "browser separation, revisions, limits and private storage");
}

static void test_lab_history_eviction(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(history_eviction), "bounded database and eviction-safe revisions");
}

/* Register history checks alongside ordinary state persistence regressions. */
void test_lab_history_cases(void)
{
    TEST_RUN_IN_GROUP(test_lab_history_round_trip, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_history_eviction, tests, NULL);
}
