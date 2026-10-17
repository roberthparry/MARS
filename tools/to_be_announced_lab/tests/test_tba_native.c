/**
 * @file test_tba_native.c
 * @brief Sequential native CSV, date and loopback route smoke regressions.
 *
 * Uses a private temporary state location and ephemeral listener. Tests the real
 * C page/asset handlers without Python, public network services or saved user
 * settings. Each forked server is reaped before the next request.
 */
#include <errno.h>
#include <math.h>
/* Distinguish the POSIX signal-stack typedef from the MARS stack API. */
#define stack_t posix_signal_stack_t
#include <signal.h>
#undef stack_t
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "http-t.h"
#include "tba_wire.h"
#include "tba_chart.h"
#include "tba_csv.h"
#include "tba_dates.h"
#include "tba_forecast.h"
#include "tba_model.h"
#include "tba_page.h"
#include "tba_period.h"
#include "tba_server.h"
#include "tba_worker.h"
#include "test_harness.h"
#include "test_tba_support.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

static void test_tba_defaults(void)
{
    json_t *defaults = tba_app_defaults(), *copy = tba_app_defaults();
    bool ok = defaults && copy && json_object_size(defaults) == 23 &&
              !strcmp(tba_json_text(defaults, "models"), "sarimax") &&
              !strcmp(tba_json_text(defaults, "frequency"), "monthly") &&
              !strcmp(tba_json_text(defaults, "year_type"), "fiscal") &&
              !strcmp(tba_json_text(defaults, "level"), "0.95") &&
              json_type(tba_json_get(defaults, "level")) == JSON_STRING;
    static const char *const empty_fields[] = {
        "target_path", "target_display_name", "target_date_column", "target_value_column",
        "xreg_path", "xreg_display_name", "xreg_date_column", "xreg_columns"
    };
    for (size_t i = 0; i < sizeof empty_fields / sizeof *empty_fields; ++i)
        ok = json_type(tba_json_get(defaults, empty_fields[i])) == JSON_STRING &&
             !*tba_json_text(defaults, empty_fields[i]) && ok;
    static const char *const numeric[] = {"p", "d", "q", "P", "D", "Q", "season_period"};
    static const char *const expected[] = {"1", "1", "0", "1", "0", "0", "12"};
    for (size_t i = 0; i < sizeof numeric / sizeof *numeric; ++i)
        ok = json_type(tba_json_get(defaults, numeric[i])) == JSON_NUMBER &&
             !strcmp(tba_json_text(defaults, numeric[i]), expected[i]) && ok;
    ok = tba_json_string(copy, "models", "arima") && !strcmp(tba_json_text(defaults, "models"), "sarimax") && ok;
    json_free(copy);
    json_free(defaults);
    TEST_ASSERT_TRUE(ok, "compiled defaults start without datasets or columns and preserve independent ownership");
}

static void test_tba_rows(void)
{
    json_t *data = json_new_object(), *headers = json_new_array(), *rows = json_new_array(), *row = json_new_array();
    bool ok = data && headers && rows && row && tba_json_append(headers, "DATE") &&
              tba_json_append(headers, "value") && tba_json_append(row, "2026-01-01") &&
              tba_json_append(row, "42") && json_array_append(rows, row) &&
              tba_json_set(data, "headers", headers) && tba_json_set(data, "rows", rows);
    tba_csv_t *csv = ok ? tba_csv_from_rows(data) : NULL;
    ok = csv && tba_csv_rows(csv) == 1 && tba_csv_column(csv, "value") == 1 &&
         !strcmp(string_c_str(tba_csv_cell(csv, 0, 1)), "42") && ok;
    tba_csv_free(csv);
    /* Duplicate headings and ragged rows must fail before indexing. */
    json_t *duplicate = json_new_array();
    ok = duplicate && tba_json_append(duplicate, "DATE") && tba_json_append(duplicate, "DATE") &&
         tba_json_set(data, "headers", duplicate) && ok;
    csv = tba_csv_from_rows(data);
    ok = !csv && ok;
    tba_csv_free(csv);
    ok = tba_json_set(data, "headers", headers) && tba_json_append(row, "extra") && ok;
    json_t *ragged = json_new_array();
    ok = ragged && json_array_append(ragged, row) && tba_json_set(data, "rows", ragged) && ok;
    csv = tba_csv_from_rows(data);
    ok = !csv && ok;
    tba_csv_free(csv);
    json_t *empty = json_new_array();
    ok = empty && tba_json_set(data, "rows", empty) && ok;
    csv = tba_csv_from_rows(data);
    ok = !csv && ok;
    tba_csv_free(csv);
    json_free(empty);
    json_free(ragged);
    json_free(duplicate);
    json_free(row);
    json_free(rows);
    json_free(headers);
    json_free(data);
    TEST_ASSERT_TRUE(ok, "in-memory rows preserve cells and reject duplicates, ragged rows and empty input");
}

static bool test_tba_csv(void)
{
    string_t *text = string_new_with("DATE,\"value, quoted\"\r\n2026-01-01,\"a\"\"b\"\r\n2026-02-01,3\r\n");
    tba_csv_t *csv = tba_csv_parse(text);
    bool ok = csv && tba_csv_rows(csv) == 2 && tba_csv_columns(csv) == 2 && tba_csv_column(csv, "value, quoted") == 1 &&
              !strcmp(string_c_str(tba_csv_cell(csv, 0, 1)), "a\"b");
    string_t *serialised = csv ? tba_csv_text(csv) : NULL;
    tba_csv_t *copy = serialised ? tba_csv_parse(serialised) : NULL;
    ok = copy && tba_csv_rows(copy) == 2 && !strcmp(tba_csv_header(copy, 1), "value, quoted") && ok;
    tba_csv_free(copy);
    string_free(serialised);
    tba_csv_free(csv);
    string_free(text);
    return ok;
}

static bool test_tba_template(void)
{
    string_t *source = string_new_with("window.__callback(); __VALUE__"), *key = string_new_with("VALUE");
    string_t *value = string_new_with("__NOT_RECURSIVE__");
    json_t *values = json_new_object(), *item = value ? json_new_string(value) : NULL;
    bool ok = source && key && values && item && json_object_set(values, key, item);
    string_t *page = ok ? tba_page_expand(source, values) : NULL;
    ok = page && !strcmp(string_c_str(page), "window.__callback(); __NOT_RECURSIVE__") && ok;
    string_free(page);
    json_free(item);
    json_free(values);
    string_free(value);
    string_free(key);
    string_free(source);
    return ok;
}

static http_response_t *test_tba_request_outcome(tba_app_t *app, const char *path, const void *body, size_t length,
                                                const char *type, int expected_worker)
{
    tba_server_t *server = tba_svr_new(app, "127.0.0.1", 0);
    string_t *url = server ? string_sprintf("http://127.0.0.1:%u%s", tba_svr_port(server), path) : NULL;
    http_client_t *client = http_client_new();
    http_request_t *request = url ? http_request_new(type ? HTTP_POST : HTTP_GET, url) : NULL;
    string_t *mime = type ? string_new_with(type) : NULL;
    http_limits_t limits = {.connect_timeout_ms = 5000, .total_timeout_ms = 15000, .max_body_bytes = 4194304};
    bool ready = server && request && client && http_client_set_limits(client, &limits);
    if (type)
        ready = ready && mime && http_request_set_body(request, body, length, mime);
    string_free(mime);
    pid_t child = ready ? fork() : -1;
    if (!child) {
        http_request_free(request);
        http_client_free(client);
        string_free(url);
        int result = 0;
        for (unsigned i = 0; i < 40 && result == 0; ++i)
            result = tba_svr_once(server);
        tba_svr_free(server);
        tba_app_free(app);
        _exit(result == expected_worker ? 0 : 1);
    }
    tba_svr_free(server);
    http_response_t *response = child > 0 ? http_client_send(client, request) : NULL;
    int status = 0;
    pid_t waited = -1;
    if (child > 0) {
        do {
            waited = waitpid(child, &status, 0);
        } while (waited < 0 && errno == EINTR);
    }
    if (!(child > 0 && waited == child && WIFEXITED(status) && WEXITSTATUS(status) == 0)) {
        http_response_free(response);
        response = NULL;
    }
    http_request_free(request);
    http_client_free(client);
    string_free(url);
    return response;
}

static http_response_t *test_tba_request(tba_app_t *app, const char *path, const void *body, size_t length,
                                        const char *type)
{
    return test_tba_request_outcome(app, path, body, length, type, 1);
}

static bool test_tba_route(tba_app_t *app, const char *path, unsigned expected)
{
    http_response_t *response = test_tba_request(app, path, NULL, 0, NULL);
    bool ok = response && http_response_status(response) == expected &&
              (expected == 404 || http_response_body_size(response) > 0);
    if (!ok)
        string_fprintf(stderr, "route %s: expected %u, got %u\n", path, expected,
                       response ? http_response_status(response) : 0);
    http_response_free(response);
    return ok;
}

static bool test_tba_wire(tba_app_t *app)
{
    json_t *update = json_new_object();
    bool ok = update && tba_json_string(update, "target_display_name", "Unicode £ – local");
    array_t *bytes = ok ? tba_wire_encode(update) : NULL;
    http_response_t *response = bytes ? test_tba_request(app, "/to-be-announced/state", array_get(bytes, 0),
                                                        array_size(bytes), "application/x-protobuf") : NULL;
    json_t *reply = response ? tba_wire_decode(http_response_body(response), http_response_body_size(response)) : NULL;
    ok = response && http_response_status(response) == 410 && !tba_json_bool(reply, "ok") && ok;
    string_t *header = string_new_with("Content-Type");
    const string_t *type = response && header ? http_response_header_at(response, header, 0) : NULL;
    ok = type && string_view_equals_literal(string_view_all(type), "application/x-protobuf") && ok;
    string_free(header);
    if (!ok)
        string_fprintf(stderr, "Protobuf state POST: HTTP %u, error %s\n", http_response_status(response),
                       tba_json_text(reply, "error"));
    json_free(reply);
    http_response_free(response);
    file_t *saved = file_new_cstr(getenv("TBA_LAB_STATE_FILE"));
    file_info_t *info = saved ? file_get_info(saved) : NULL;
    ok = !info && ok;
    file_info_free(info);
    file_free(saved);
    response = test_tba_request(app, "/to-be-announced/state", NULL, 0, NULL);
    reply = response ? tba_wire_decode(http_response_body(response), http_response_body_size(response)) : NULL;
    ok = !*tba_json_text(reply, "target_display_name") && ok;
    if (*tba_json_text(reply, "target_display_name"))
        string_fprintf(stderr, "Protobuf state GET: HTTP %u, name %s\n", http_response_status(response),
                       tba_json_text(reply, "target_display_name"));
    json_free(reply);
    http_response_free(response);
    static const struct { const char *type; const char *body; unsigned status; } rejected[] = {
        {"application/json", "{}", 415}, {"application/x-protobuf", "{}", 400},
        {"application/x-protobuf", "", 400}
    };
    for (size_t i = 0; i < sizeof rejected / sizeof *rejected; ++i) {
        response = test_tba_request(app, "/stream/begin", rejected[i].body, strlen(rejected[i].body), rejected[i].type);
        reply = response ? tba_wire_decode(http_response_body(response), http_response_body_size(response)) : NULL;
        if (!response || http_response_status(response) != rejected[i].status || !*tba_json_text(reply, "error"))
            string_fprintf(stderr, "Protobuf rejection %zu: HTTP %u, expected %u, error %s\n", i,
                           http_response_status(response), rejected[i].status, tba_json_text(reply, "error"));
        ok = response && http_response_status(response) == rejected[i].status && *tba_json_text(reply, "error") && ok;
        json_free(reply);
        http_response_free(response);
    }
    for (unsigned final = 0; final <= 1; ++final) {
        json_t *frame = json_new_object();
        bool ready = frame && tba_json_flag(frame, "final", final != 0);
        array_t *encoded = ready ? tba_wire_encode(frame) : NULL;
        response = encoded ? test_tba_request_outcome(app, "/stream/chunk", array_get(encoded, 0), array_size(encoded),
                                                     "application/x-protobuf", final ? 2 : 1) : NULL;
        ok = response && http_response_status(response) == 400 && ok;
        http_response_free(response);
        array_destroy(encoded);
        json_free(frame);
    }
    response = test_tba_request(app, "/wasm/tba_browser.wasm", NULL, 0, NULL);
    if (!response || http_response_status(response) != 200)
        string_fprintf(stderr, "WASM asset: HTTP %u\n", http_response_status(response));
    ok = response && http_response_status(response) == 200 && http_response_body_size(response) > 8 &&
         !memcmp(http_response_body(response), "\0asm\1\0\0\0", 8) && ok;
    http_response_free(response);
    response = bytes ? test_tba_request(app, "/to-be-announced/forecast-form", array_get(bytes, 0),
                                       array_size(bytes), "application/x-protobuf") : NULL;
    ok = response && http_response_status(response) == 404 && ok;
    http_response_free(response);
    array_destroy(bytes);
    json_free(update);
    return ok;
}

static void test_tba_browser_policy(void)
{
    TEST_ASSERT_TRUE(tba_browser_abi_version() == 3, "application WASM ABI is versioned");
    TEST_ASSERT_TRUE(tba_model_season(2, 0, 0) == 12 && tba_model_season(3, 0, 0) == 4 &&
                     tba_model_season(2, 1, 0) == 0 && tba_model_season(2, 2, 0) == 1 &&
                     tba_model_season(2, 4, 6) == 6, "seasonality overrides and frequency defaults");
    TEST_ASSERT_TRUE(tba_model_order(0, 5, 0) == 0 && tba_model_order(5, 4, 0) == 2 &&
                     tba_model_order(3, 4, 3) == 1 && tba_model_order(3, 2, 3) == 0 &&
                     tba_model_order(99, 0, 0) == -1, "table-driven model suggestions reject invalid indices");
    TEST_ASSERT_TRUE(tba_model_option_count(2) == 6 && tba_model_option_value(2, 5) == 12 &&
                     !strcmp(tba_model_option_label(3, 2), "Half-yearly") &&
                     tba_model_option_value(2, 99) == -1, "season options preserve labels and values");
    TEST_ASSERT_TRUE(tba_period_end(20240115, 3, 1) == 20240331 && tba_period_end(20240401, 4, 1) == 20250331 &&
                     tba_period_end(20240215, 2, 0) == 20240229, "fiscal quarter, year and leap-month boundaries");
    TEST_ASSERT_TRUE(tba_period_add(20240228, 1, 1, 0) == 20240229 &&
                     tba_period_add(20240228, 1, 2, 0) == 20240301 &&
                     tba_period_add(20231231, 2, 2, 0) == 20240229 &&
                     tba_period_add(20241231, 3, 1, 1) == 20250331 &&
                     tba_period_add(20240331, 4, 1, 1) == 20250331, "horizons cross month, year and fiscal boundaries");
    TEST_ASSERT_TRUE(!tba_period_end(20230229, 2, 0) && !tba_period_end(0, 2, 0) &&
                     !tba_period_add(99991231, 1, 1, 0) && !tba_period_add(20260101, 1, 10001, 0),
                     "invalid and overflowing dates are rejected");
    TEST_ASSERT_TRUE(tba_period_options(20251231, 20260331, 2, 0) == 3 &&
                     tba_period_option(0) == 20260131 && tba_period_option(2) == 20260331 &&
                     !tba_period_option(3), "forecast choices honour driver coverage");
    TEST_ASSERT_TRUE(tba_period_options(20200101, 20300101, 1, 0) == 1200 &&
                     !tba_period_options(20200101, 0, 0, 0), "horizons are bounded and reject unknown frequency");
    TEST_ASSERT_TRUE(tba_model_preferred(10, 4) == 3 && tba_model_preferred(10, 1) == 1 &&
                     tba_model_warning(8, 1, 0, 12, 1, 0, 0) == 1 &&
                     tba_model_warning(8, 4, 12, 12, 1, 0, 0) == 6 &&
                     tba_model_warning(2, 4, 12, 12, 1, 0, 0) == 7 &&
                     !tba_model_replace(0, 0, 1, 0, 0) && tba_model_replace(1, 0, 1, 0, 0),
                     "native settings policy preserves edits and detects seasonal mismatches");
}

static void test_tba_chart(void)
{
    double *dates = tba_chart_buffer(0), *values = tba_chart_buffer(1);
    dates[0] = 10; dates[1] = 20; dates[2] = 50;
    values[0] = 5; values[1] = NAN; values[2] = 5;
    bool ok = tba_chart_begin(3, 60);
    tba_chart_include(3);
    ok = tba_chart_finish() && tba_chart_time_axis() && tba_chart_x(0) == 68 &&
         fabs(tba_chart_x(2) - 823.2) < 0.001 && tba_chart_y_tick(0) == 6 &&
         tba_chart_y_tick(4) == 4 && tba_chart_tick(1, 3) == 1 && ok;
    double *xs = tba_chart_buffer(2), *ys = tba_chart_buffer(3);
    xs[0] = 0; xs[1] = 10; ys[0] = ys[1] = 5;
    ok = tba_chart_nearest(2, 9, 5) == 1 && tba_chart_nearest(0, 0, 0) == -1 &&
         !tba_chart_begin(32769, 0) && !tba_chart_buffer(4) && ok;
    dates[0] = NAN;
    tba_chart_begin(1, 0);
    values[0] = NAN;
    tba_chart_include(1);
    ok = !tba_chart_finish() && !tba_chart_time_axis() && ok;
    TEST_ASSERT_TRUE(ok, "WASM chart geometry handles missing values, constant ranges, ticks and capacity bounds");
}

static void test_tba_forecast_view(void)
{
    json_t *result = json_new_object();
    bool ok = result && tba_json_flag(result, "ok", true) &&
              tba_json_string(result, "model", "ARIMA") &&
              tba_json_string(result, "forecast_end_date", "2026-02-28") &&
              tba_json_string(result, "forecast_csv",
                  "date,actual,mean,stderr,lower,upper\n01/01/2026,10,11,,9,13\n01/02/2026,<tag>,12,,,\n") &&
              tba_forecast_view(result);
    const json_t *chart = tba_json_get(result, "chart"), *actual = tba_json_get(chart, "actual");
    string_t *body = string_new_with(tba_json_text(result, "table_body"));
    ok = json_array_size(actual) == 2 && json_type(json_array_get(actual, 0)) == JSON_NUMBER &&
         json_type(json_array_get(actual, 1)) == JSON_NULL &&
         json_array_size(tba_json_get(chart, "series")) == 1 &&
         body && string_find(body, "&lt;tag&gt;") >= 0 && string_find(body, "<tag>") < 0 && ok;
    string_free(body);
    json_free(result);
    TEST_ASSERT_TRUE(ok, "native result tables escape cells and charts carry typed numbers or null");
}

static void test_tba_worker(void)
{
    string_t *y = string_new_with("DATE,value\n"), *x = string_new_with("DATE,driver\n");
    bool ok = y && x;
    for (unsigned i = 0; ok && i < 42; ++i) {
        double value = 20 + i + sin(i * 0.7);
        ok = string_append_format(x, "%u-%02u-01,%.8f\n", 2023 + i / 12, i % 12 + 1, value) >= 0;
        if (i < 36)
            ok = string_append_format(y, "%u-%02u-01,%.8f\n", 2023 + i / 12, i % 12 + 1,
                                      2 + 3 * value + cos(i * 0.4)) >= 0 && ok;
    }
    static const char *const models[] = {"regression", "arima", "arimax", "sarima", "sarimax", "auto-arima"};
    const char *args[] = {"worker",
                    "--target-date-column", "DATE", "--target-value-column", "value",
                    "--xreg-date-column", "DATE",
                    "--xreg-cols", "driver", "--frequency", "monthly", "--year-type", "calendar",
                    "--horizon", "3", "--p", "0", "--d", "0", "--q", "0",
                    "--P", "0", "--D", "0", "--Q", "0", "--season-period", "0", "--model", NULL};
    size_t model_index = sizeof args / sizeof *args - 1;
    for (size_t i = 0; ok && i < sizeof models / sizeof *models; ++i) {
        args[model_index] = models[i];
        json_t *result = tba_worker_memory((int)(sizeof args / sizeof *args), args, y, x);
        bool fitted = tba_json_bool(result, "ok");
        if (!fitted)
            string_fprintf(stderr, "%s: %s\n", models[i], tba_json_text(result, "error"));
        ok = fitted && *tba_json_text(result, "forecast_csv") && ok;
        json_t *memory = tba_worker_memory((int)(sizeof args / sizeof *args), args, NULL, x);
        ok = memory && !tba_json_bool(memory, "ok") && ok;
        json_free(memory);
        json_free(result);
    }
    args[model_index] = "invalid-model";
    json_t *invalid = tba_worker_memory((int)(sizeof args / sizeof *args), args, y, x);
    ok = invalid && !tba_json_bool(invalid, "ok") && *tba_json_text(invalid, "error") && ok;
    json_free(invalid);
    string_free(y);
    string_free(x);
    TEST_ASSERT_TRUE(ok, "all six native modes operate in memory and reject invalid or missing inputs");
}

/* Standalone smoke suite: all requests are sequential and no user state is written. */
static void test_tba_native_routes(void)
{
    char directory[] = "/tmp/mars-tba-test-XXXXXX";
    TEST_ASSERT_TRUE(mkdtemp(directory) != NULL, "private fixture directory exists");
    string_t *state = string_sprintf("%s/state.pb", directory);
    bool ok = state && setenv("TBA_LAB_STATE_FILE", string_c_str(state), 1) == 0 &&
              setenv("TBA_LAB_UPLOAD_DIR", directory, 1) == 0;
    tba_app_t *app = ok ? tba_app_new(NULL) : NULL;
    ok = app && test_tba_csv() && test_tba_template() && tba_date_cstr("29/02/2024") == 20240229 &&
         tba_date_cstr("29/02/2023") == 0 && tba_date_next(20261231, "daily") == 20270101 && ok;
    const char *paths[] = {"/to-be-announced/", "/to-be-announced/index.css", "/to-be-announced/js/transport.js",
                          "/to-be-announced/js/codec.js", "/to-be-announced/js/codec_types.js",
                          "/to-be-announced/js/jobs.js", "/to-be-announced/js/codec_worker.js",
                          "/to-be-announced/wasm/tba_codec.wasm",
                          "/to-be-announced/state"};
    for (size_t i = 0; app && i < sizeof paths / sizeof *paths; ++i)
        ok = test_tba_route(app, paths[i], 200) && ok;
    if (app)
        ok = test_tba_route(app, "/to-be-announced/not-found", 404) && ok;
    if (app)
        ok = test_tba_wire(app) && ok;

    tba_app_free(app);
    string_free(state);
    file_t *root = file_new_cstr(directory);
    ok = root && test_tba_cleanup(root) && ok;
    file_free(root);
    string_printf("Native CSV, dates and loopback routes: %s\n", ok ? "PASS" : "FAIL");
    TEST_ASSERT_TRUE(ok, "native CSV, dates and loopback routes work without Python");
}

static void test_tba_stream(void)
{
    TEST_ASSERT_TRUE(test_tba_stream_checks(), "encrypted envelopes, one-use replies, replay, quota, cancellation and expiry");
}

/* Register the fixture with the shared sequential test configuration. */
int tests_main(void)
{
    TEST_RUN_IN_GROUP(test_tba_defaults, tests, NULL);
    TEST_RUN_IN_GROUP(test_tba_rows, tests, NULL);
    TEST_RUN_IN_GROUP(test_tba_browser_policy, tests, NULL);
    TEST_RUN_IN_GROUP(test_tba_chart, tests, NULL);
    TEST_RUN_IN_GROUP(test_tba_forecast_view, tests, NULL);
    TEST_RUN_IN_GROUP(test_tba_worker, tests, NULL);
    TEST_RUN_IN_GROUP(test_tba_stream, tests, NULL);
    TEST_RUN_IN_GROUP(test_tba_native_routes, tests, NULL);
    return TEST_EXIT_CODE();
}
