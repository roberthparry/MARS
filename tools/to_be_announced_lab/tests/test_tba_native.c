/**
 * @file test_tba_native.c
 * @brief Sequential native CSV, date and loopback route smoke regressions.
 *
 * Uses a private temporary state location and ephemeral listener. Tests the real
 * C page/asset handlers without Python, public network services or saved user
 * settings. Each forked server is reaped before the next request.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "http.h"
#include "tba_csv.h"
#include "tba_dates.h"
#include "tba_page.h"
#include "tba_server.h"
#include "test_harness.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

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

static bool test_tba_route(tba_app_t *app, const char *path, unsigned expected)
{
    tba_server_t *server = tba_svr_new(app, "127.0.0.1", 0);
    string_t *url = server ? string_sprintf("http://127.0.0.1:%u%s", tba_svr_port(server), path) : NULL;
    http_client_t *client = http_client_new();
    http_request_t *request = url ? http_request_new(HTTP_GET, url) : NULL;
    http_limits_t limits = {.connect_timeout_ms = 5000, .total_timeout_ms = 15000, .max_body_bytes = 4194304};
    bool ready = server && request && client && http_client_set_limits(client, &limits);
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
        _exit(result == 1 ? 0 : 1);
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
    bool ok = child > 0 && waited == child && WIFEXITED(status) && WEXITSTATUS(status) == 0 && response &&
              http_response_status(response) == expected && (expected == 404 || http_response_body_size(response) > 0);
    if (!ok)
        string_fprintf(stderr, "route %s: expected %u, got %u\n", path, expected,
                       response ? http_response_status(response) : 0);
    http_response_free(response);
    http_request_free(request);
    http_client_free(client);
    string_free(url);
    return ok;
}

/* Standalone smoke suite: all requests are sequential and no user state is written. */
static void test_tba_native_routes(void)
{
    char directory[] = "/tmp/mars-tba-test-XXXXXX";
    TEST_ASSERT_TRUE(mkdtemp(directory) != NULL, "private fixture directory exists");
    string_t *state = string_sprintf("%s/state.json", directory);
    bool ok = state && setenv("MARS_LAB_STATE_FILE", string_c_str(state), 1) == 0 &&
              setenv("TBA_LAB_UPLOAD_DIR", directory, 1) == 0;
    tba_app_t *app = ok ? tba_app_new(NULL, NULL) : NULL;
    ok = app && test_tba_csv() && test_tba_template() && tba_date_cstr("29/02/2024") == 20240229 &&
         tba_date_cstr("29/02/2023") == 0 && tba_date_next(20261231, "daily") == 20270101 && ok;
    const char *paths[] = {"/to-be-announced/", "/to-be-announced/index.css", "/to-be-announced/js/app.js",
                           "/to-be-announced/js/transport.js", "/to-be-announced/state"};
    for (size_t i = 0; app && i < sizeof paths / sizeof *paths; ++i)
        ok = test_tba_route(app, paths[i], 200) && ok;
    if (app)
        ok = test_tba_route(app, "/to-be-announced/not-found", 404) && ok;
    tba_app_free(app);
    string_free(state);
    file_t *root = file_new_cstr(directory);
    ok = root && file_remove_directory(root) && ok;
    file_free(root);
    string_printf("Native CSV, dates and loopback routes: %s\n", ok ? "PASS" : "FAIL");
    TEST_ASSERT_TRUE(ok, "native CSV, dates and loopback routes work without Python");
}

/* Register the fixture with the shared sequential test configuration. */
int tests_main(void)
{
    TEST_RUN_IN_GROUP(test_tba_native_routes, tests, NULL);
    return TEST_EXIT_CODE();
}
