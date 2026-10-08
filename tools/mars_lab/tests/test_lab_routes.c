/**
 * @file test_lab_routes.c
 * @brief Loopback HTTP integration tests for native Lab routes and response contracts.
 *
 * Registers real Lab handlers on an ephemeral listener and forks one server child
 * per request. Every child is reaped before the next request. Private state fixtures
 * cover page, asset and worksheet routes; malformed payloads and hostile Host/Origin
 * headers exercise boundary validation without external network services.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "file.h"
#include "http.h"
#include "lab_server.h"
#include "sqlite.h"
#include "test_harness.h"
#include "test_lab_support.h"

static bool request_header(http_request_t *request, const char *name, const char *value)
{
    string_t *key = string_new_with(name), *text = string_new_with(value);
    bool ok = key && text && http_request_set_header(request, key, text);
    string_free(key);
    string_free(text);
    return ok;
}

static http_response_t *route_request(webmethod_t method, const char *path, const char *body, const char *type,
                                      const char *host, const char *origin)
{
    /* Page generation and real maths share the server's absolute I/O deadline.
     * Allow CPU-limited DEBUG builds while preserving a finite per-request bound. */
    lab_server_t *server = lab_svr_new(NULL, 0, 60000);
    if (!server) {
        string_fprintf(stderr, "Lab HTTP %s: listener/route setup failed, errno=%d\n", path, errno);
        lab_svr_free(server);
        return NULL;
    }
    string_t *url = string_sprintf("http://127.0.0.1:%u%s", lab_svr_port(server), path);
    http_client_t *client = http_client_new();
    http_request_t *request = url ? http_request_new(method, url) : NULL;
    string_t *payload = body ? string_new_with(body) : NULL;
    string_t *content_type = type ? string_new_with(type) : NULL;
    http_limits_t limits = {.connect_timeout_ms = 5000, .total_timeout_ms = 75000, .max_body_bytes = 4194304};
    bool ready =
        client && request && http_client_set_limits(client, &limits) &&
        (!host || request_header(request, "Host", host)) && (!origin || request_header(request, "Origin", origin)) &&
        (!body ||
         (payload && http_request_set_body(request, string_c_str(payload), string_byte_length(payload), content_type)));
    struct timespec start = {0}, finish = {0};
    clock_gettime(CLOCK_MONOTONIC, &start);
    pid_t child = ready ? fork() : -1;
    if (!child) {
        /* The forked server owns copies of the client fixtures too. */
        http_request_free(request);
        http_client_free(client);
        string_free(content_type);
        string_free(payload);
        string_free(url);
        int result = lab_svr_serve_once(server, 10000);
        int error = errno;
        if (result != 1)
            string_fprintf(stderr, "Lab HTTP %s: serve_once=%d, errno=%d (ETIMEDOUT=%d)\n", path, result, error,
                           ETIMEDOUT);
        lab_svr_free(server);
        _exit(result == 1 ? 0 : 1);
    }
    lab_svr_free(server);
    http_response_t *response = child > 0 ? http_client_send(client, request) : NULL;
    if (!response) {
        clock_gettime(CLOCK_MONOTONIC, &finish);
        long long elapsed =
            (long long)(finish.tv_sec - start.tv_sec) * 1000 + (finish.tv_nsec - start.tv_nsec) / 1000000;
        const string_t *diagnostic = http_client_error_text(client);
        string_fprintf(stderr, "Lab HTTP %s: ready=%d, child=%ld, transport=%d, elapsed=%lld ms: %s\n", path, ready,
                       (long)child, http_client_error(client), elapsed,
                       diagnostic ? string_c_str(diagnostic) : "no transport diagnostic");
    }
    int status = 0;
    pid_t waited = -1;
    if (child > 0) {
        do {
            waited = waitpid(child, &status, 0);
        } while (waited < 0 && errno == EINTR);
    }
    if (waited != child || child <= 0 || !WIFEXITED(status) || WEXITSTATUS(status)) {
        string_fprintf(stderr, "Lab HTTP %s: server wait=%ld, child=%ld, wait status=%d\n", path, (long)waited,
                       (long)child, status);
        http_response_free(response);
        response = NULL;
    }
    http_request_free(request);
    http_client_free(client);
    string_free(content_type);
    string_free(payload);
    string_free(url);
    return response;
}

static bool response_header_equals(const http_response_t *response, const char *name, const char *expected)
{
    string_t *key = string_new_with(name);
    const string_t *value = key && response ? http_response_header_at(response, key, 0) : NULL;
    bool ok = value && string_view_equals_literal(string_view_all(value), expected);
    string_free(key);
    return ok;
}

static bool route_check(const http_response_t *response, unsigned expected, bool content_ok, const char *description)
{
    bool headers_ok = response_header_equals(response, "Cache-Control", "no-store") &&
                      response_header_equals(response, "X-Content-Type-Options", "nosniff");
    bool ok = response && http_response_status(response) == expected && content_ok && headers_ok;
    if (!ok) {
        json_t *error = response ? http_response_json(response) : NULL;
        string_fprintf(stderr, "Lab HTTP %s: expected=%u, received=%u, bytes=%zu, content=%d, headers=%d, error=%s\n",
                       description, expected, response ? http_response_status(response) : 0,
                       response ? http_response_body_size(response) : 0, content_ok, headers_ok,
                       test_lab_text(error, "error"));
        json_free(error);
    }
    return ok;
}

static bool response_contains(const http_response_t *response, const char *text)
{
    string_t *body = string_new();
    bool ok = response && body &&
              string_append_utf8_exact(body, http_response_body(response), http_response_body_size(response)) == 0 &&
              string_find(body, text) >= 0;
    string_free(body);
    return ok;
}

/* Scan the bounded response once for template tokens, allowing ordinary JS __property names. */
static bool page_has_no_placeholders(const http_response_t *response)
{
    string_t *body = string_new();
    bool ok = response && body &&
              string_append_utf8_exact(body, http_response_body(response), http_response_body_size(response)) == 0;
    string_view_t view = string_view_all(body);
    size_t length = string_view_length(view);
    for (size_t i = 0; ok && i + 2 < length; ++i) {
        unsigned char a = 0, b = 0, first = 0;
        if (!string_view_peek_ascii(view, i, &a) || a != '_' || !string_view_peek_ascii(view, i + 1, &b) || b != '_' ||
            !string_view_peek_ascii(view, i + 2, &first) || first < 'A' || first > 'Z')
            continue;
        i += 2;
        while (i < length && string_view_peek_ascii(view, i, &a)) {
            if (a == '_' && i + 1 < length && string_view_peek_ascii(view, i + 1, &b) && b == '_') {
                ok = false;
                break;
            }
            if (a != '_' && !(a >= 'A' && a <= 'Z') && !(a >= '0' && a <= '9'))
                break;
            ++i;
        }
    }
    string_free(body);
    return ok;
}

static bool route_contracts(const char *directory)
{
    (void)directory;
    if (setenv("NO_PROXY", "*", 1) || setenv("no_proxy", "*", 1))
        return false;
    http_response_t *response = route_request(HTTP_GET, "/", NULL, NULL, NULL, NULL);
    bool ok = route_check(response, 200, response_contains(response, "MARS Lab"), "GET / page");
    http_response_free(response);
    response = route_request(HTTP_GET, "/index.html", NULL, NULL, NULL, NULL);
    bool title_ok = response_contains(response, "<title>MARS Lab</title>");
    bool placeholders_ok = page_has_no_placeholders(response);
    bool catalogue_ok = response && !response_contains(response, "MARS_LAB_DATA");
    bool stylesheet_ok = response_contains(response, "<link rel=\"stylesheet\" href=\"/index.css\">") &&
                         !response_contains(response, "<style>");
    bool scripts_ok = response_contains(response, "<script defer src=\"/js/app.js\"></script>") &&
                      response_contains(response, "id=\"lab-config\" type=\"application/json\"") &&
                      !response_contains(response, "function evaluateExpression") &&
                      !response_contains(response, "JURISDICTION_TOWN_OPTIONS");
    bool type_ok = response_header_equals(response, "Content-Type", "text/html; charset=utf-8");
    if (!title_ok || !placeholders_ok || !catalogue_ok || !type_ok)
        string_fprintf(stderr, "Lab index page: title=%d, placeholders=%d, catalogue=%d, HTML type=%d\n", title_ok,
                       placeholders_ok, catalogue_ok, type_ok);
    ok = route_check(response, 200,
                     title_ok && placeholders_ok && catalogue_ok && type_ok && stylesheet_ok && scripts_ok,
                     "GET /index.html") &&
         ok;
    http_response_free(response);
    response = route_request(HTTP_GET, "/index.css", NULL, NULL, NULL, NULL);
    file_t *stylesheet = file_new_cstr("tools/mars_lab/assets/index.css");
    string_t *expected_css = stylesheet ? file_read_all_text(stylesheet) : NULL;
    string_t *actual_css = response ? http_response_text(response) : NULL;
    bool css_ok = expected_css && actual_css && string_compare(expected_css, actual_css) == 0 &&
                  response_contains(response, "@import") &&
                  response_header_equals(response, "Content-Type", "text/css; charset=utf-8");
    ok = route_check(response, 200, css_ok, "GET /index.css") && ok;
    string_free(actual_css);
    string_free(expected_css);
    file_free(stylesheet);
    http_response_free(response);
    static const char *const styles[] = {"theme",  "layout",  "worksheets", "dates", "forms",     "bindings",
                                         "mobile", "buttons", "results",    "help",  "responsive"};
    for (size_t i = 0; i < sizeof styles / sizeof *styles; ++i) {
        string_t *url = string_sprintf("/css/%s.css", styles[i]);
        string_t *path = string_sprintf("tools/mars_lab/assets/css/%s.css", styles[i]);
        file_t *file = path ? file_new(path) : NULL;
        string_t *expected = file ? file_read_all_text(file) : NULL;
        response = url ? route_request(HTTP_GET, string_c_str(url), NULL, NULL, NULL, NULL) : NULL;
        string_t *actual = response ? http_response_text(response) : NULL;
        bool match = expected && actual && string_compare(expected, actual) == 0 &&
                     response_header_equals(response, "Content-Type", "text/css; charset=utf-8") &&
                     response_header_equals(response, "Cache-Control", "no-store") &&
                     response_header_equals(response, "X-Content-Type-Options", "nosniff");
        ok = route_check(response, 200, match, styles[i]) && ok;
        string_free(actual);
        string_free(expected);
        file_free(file);
        string_free(path);
        string_free(url);
        http_response_free(response);
    }
    response = route_request(HTTP_GET, "/css/missing.css", NULL, NULL, NULL, NULL);
    ok = response && http_response_status(response) == 404 && ok;
    http_response_free(response);
    static const char *const scripts[] = {"almanac",    "api",         "app",       "bindings",      "calculus",
                                          "controls",   "date_picker", "editor",    "evaluation",    "events",
                                          "integrator", "locations",   "mobile",    "result_layout", "result_text",
                                          "results",    "state",       "worksheet", "workspace"};
    for (size_t i = 0; i < sizeof scripts / sizeof *scripts; ++i) {
        string_t *url = string_sprintf("/js/%s.js", scripts[i]);
        string_t *path = string_sprintf("tools/mars_lab/assets/js/%s.js", scripts[i]);
        file_t *file = path ? file_new(path) : NULL;
        string_t *expected = file ? file_read_all_text(file) : NULL;
        response = url ? route_request(HTTP_GET, string_c_str(url), NULL, NULL, NULL, NULL) : NULL;
        string_t *actual = response ? http_response_text(response) : NULL;
        bool match = expected && actual && string_compare(expected, actual) == 0 &&
                     response_header_equals(response, "Content-Type", "text/javascript; charset=utf-8");
        ok = route_check(response, 200, match, scripts[i]) && ok;
        string_free(actual);
        string_free(expected);
        file_free(file);
        string_free(path);
        string_free(url);
        http_response_free(response);
    }
    response = route_request(HTTP_GET, "/catalogue.json", NULL, NULL, NULL, NULL);
    json_t *catalogue = response ? http_response_json(response) : NULL;
    ok = route_check(response, 200,
                     json_type(test_lab_member(catalogue, "defaults")) == JSON_OBJECT &&
                         !test_lab_member(catalogue, "towns") && !test_lab_member(catalogue, "locations") &&
                         !test_lab_member(catalogue, "options") &&
                         response_header_equals(response, "Content-Type", "application/json"),
                     "GET /catalogue.json") &&
         ok;
    json_free(catalogue);
    http_response_free(response);
    response = route_request(HTTP_GET, "/js/missing.js", NULL, NULL, NULL, NULL);
    ok = response && http_response_status(response) == 404 && ok;
    http_response_free(response);
    response = route_request(HTTP_GET, "/favicon.svg", NULL, NULL, NULL, NULL);
    ok = route_check(response, 200, response_contains(response, "<svg"), "GET /favicon.svg") && ok;
    http_response_free(response);
    response = route_request(HTTP_POST, "/state", "{\"expression\":\"x+8\",\"expression_updated_at\":500}",
                             "application/json", NULL, NULL);
    json_t *state = response ? http_response_json(response) : NULL;
    const string_t *expression = json_string_value(test_lab_member(state, "expression"));
    ok = route_check(response, 200, expression && string_view_equals_literal(string_view_all(expression), "x+8"),
                     "POST /state saved expression") &&
         ok;
    json_free(state);
    http_response_free(response);
    response = route_request(HTTP_GET, "/state", NULL, NULL, NULL, NULL);
    state = response ? http_response_json(response) : NULL;
    expression = json_string_value(test_lab_member(state, "expression"));
    ok = route_check(response, 200, expression && string_view_equals_literal(string_view_all(expression), "x+8"),
                     "GET /state persisted expression") &&
         ok;
    json_free(state);
    http_response_free(response);
    response = route_request(HTTP_POST, "/eval", "{\"expression\":\"2+3\"}", "application/json", NULL, NULL);
    json_t *result = response ? http_response_json(response) : NULL;
    const string_t *value = json_string_value(test_lab_member(result, "value"));
    ok = route_check(response, 200,
                     test_lab_ok(result, true) && value && string_view_equals_literal(string_view_all(value), "5"),
                     "POST /eval value=5") &&
         ok;
    json_free(result);
    http_response_free(response);
    return ok;
}

/* Host is deliberately protected by http.h; send hostile headers only through this bounded socket fixture. */
static bool hostile_request(const char *host, const char *origin)
{
    lab_server_t *server = lab_svr_new(NULL, 0, 5000);
    string_t *wire =
        origin ? string_sprintf("POST /eval HTTP/1.1\r\nHost: %s\r\nOrigin: %s\r\nContent-Type: application/json\r\n"
                                "Content-Length: 2\r\nConnection: close\r\n\r\n{}",
                                host, origin)
               : string_sprintf("POST /eval HTTP/1.1\r\nHost: %s\r\nContent-Type: application/json\r\n"
                                "Content-Length: 2\r\nConnection: close\r\n\r\n{}",
                                host);
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    struct sockaddr_in address = {
        .sin_family = AF_INET, .sin_port = htons(lab_svr_port(server)), .sin_addr.s_addr = htonl(INADDR_LOOPBACK)};
    struct timeval timeout = {.tv_sec = 10};
    bool ready = server && wire && fd >= 0 && !setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) &&
                 !setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) &&
                 !connect(fd, (struct sockaddr *)&address, sizeof(address));
    pid_t child = ready ? fork() : -1;
    if (!child) {
        close(fd);
        string_free(wire);
        int result = lab_svr_serve_once(server, 5000);
        lab_svr_free(server);
        _exit(result == 1 ? 0 : 1);
    }
    lab_svr_free(server);
    bool ok = child > 0;
    size_t sent = 0;
    while (ok && sent < string_byte_length(wire)) {
        ssize_t count = send(fd, string_c_str(wire) + sent, string_byte_length(wire) - sent, MSG_NOSIGNAL);
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0) {
            ok = false;
            break;
        }
        sent += (size_t)count;
    }
    string_t *reply = string_new();
    ok = reply && ok;
    bool eof = false;
    /* The fixed error response is small; reject unexpectedly large responses, never truncate to a pass. */
    while (ok && string_byte_length(reply) < 16384) {
        char buffer[1024];
        ssize_t count = recv(fd, buffer, sizeof(buffer), 0);
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0) {
            eof = count == 0;
            break;
        }
        ok = string_append_utf8_exact(reply, buffer, (size_t)count) == 0;
    }
    if (fd >= 0)
        close(fd);
    int status = 0;
    pid_t waited = -1;
    if (child > 0) {
        do {
            waited = waitpid(child, &status, 0);
        } while (waited < 0 && errno == EINTR);
    }
    string_offset_t separator = reply ? string_find(reply, "\r\n\r\n") : -1;
    string_t *body =
        separator >= 0 ? string_substr(reply, (size_t)separator + 4, string_byte_length(reply) - (size_t)separator - 4)
                       : NULL;
    json_t *result = body ? json_from_text(body) : NULL;
    ok = ok && eof && child > 0 && waited == child && WIFEXITED(status) && !WEXITSTATUS(status) &&
         string_starts_with(reply, "HTTP/1.1 403 ") && test_lab_ok(result, false) && *test_lab_text(result, "error") &&
         string_find(reply, "cache-control: no-store\r\n") >= 0 &&
         string_find(reply, "x-content-type-options: nosniff\r\n") >= 0;
    if (!ok)
        string_fprintf(stderr, "Lab hostile request: host=%s, origin=%s, setup=%d, EOF=%d, status=%d, bytes=%zu\n",
                       host, origin ? origin : "none", ready, eof, status, reply ? string_byte_length(reply) : 0);
    json_free(result);
    string_free(body);
    string_free(reply);
    string_free(wire);
    return ok;
}

static bool route_rejections(const char *directory)
{
    (void)directory;
    if (setenv("NO_PROXY", "*", 1) || setenv("no_proxy", "*", 1))
        return false;
    static const struct {
        const char *path;
        const char *body;
        const char *type;
        const char *host;
        const char *origin;
        unsigned status;
    } cases[] = {{"/eval", "{}", "application/json", NULL, NULL, 400},
                 {"/eval", "{", "application/json", NULL, NULL, 400},
                 {"/eval", "[]", "application/json", NULL, NULL, 400},
                 {"/eval", "{}", "text/plain", NULL, NULL, 415},
                 {"/eval", "{}", "application/json", "example.invalid", NULL, 403},
                 {"/eval", "{}", "application/json", "203.0.113.10", NULL, 403},
                 {"/eval", "{}", "application/json", "[2001:db8::1]", NULL, 403},
                 {"/eval", "{}", "application/json", "localhost", "https://example.invalid", 403},
                 {"/funnel-toggle", "{}", "application/json", NULL, NULL, 410}};
    bool ok = true;
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); ++i) {
        if (cases[i].host) {
            ok = hostile_request(cases[i].host, cases[i].origin) && ok;
            continue;
        }
        http_response_t *response =
            route_request(HTTP_POST, cases[i].path, cases[i].body, cases[i].type, cases[i].host, cases[i].origin);
        json_t *result = response ? http_response_json(response) : NULL;
        bool passed = route_check(response, cases[i].status,
                                  test_lab_ok(result, false) && *test_lab_text(result, "error"), cases[i].path);
        if (!passed)
            string_fprintf(stderr, "Lab rejection case %zu: type=%s, host=%s, origin=%s\n", i, cases[i].type,
                           cases[i].host ? cases[i].host : "default", cases[i].origin ? cases[i].origin : "none");
        ok = passed && ok;
        json_free(result);
        http_response_free(response);
    }
    return ok;
}

static void test_lab_http_contracts(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(route_contracts),
                     "real HTTP routes serve assets, persist state and evaluate maths");
}

static bool route_catalogue(const char *directory)
{
    if (!test_lab_catalogue_database(directory))
        return false;
    http_response_t *response = route_request(HTTP_GET, "/jurisdictions", NULL, NULL, NULL, NULL);
    json_t *data = response ? http_response_json(response) : NULL;
    bool available = false;
    const json_t *towns = test_lab_member(test_lab_member(data, "towns"), "GB-WLS");
    const json_t *location = test_lab_member(test_lab_member(data, "locations"), "GB-WLS");
    bool ok = data && json_bool_value(test_lab_member(data, "available"), &available) && available &&
              json_array_size(test_lab_member(data, "options")) == 4 && json_array_size(towns) == 1 &&
              !strcmp(test_lab_text(json_array_get(towns, 0), "name"), "Rhyl") && json_array_size(location) == 4;
    ok = route_check(response, 200, ok, "GET /jurisdictions database rows") && ok;
    json_free(data);
    http_response_free(response);

    string_t *path = string_new_with(getenv("MARS_JURISDICTION_DB_PATH"));
    string_t *key = string_new_with(getenv("MARS_JURISDICTION_DB_KEY"));
    sqlite_t *db = path && key ? sqlite_open_encrypted(path, key) : NULL;
    string_t *sql = string_new_with("update jurisdiction_town set town_name='Changed database town' "
                                    "where jurisdiction_town_id=2;");
    ok = db && sql && sqlite_exec(db, sql) && ok;
    string_free(sql);
    sqlite_close(db);
    string_free(path);
    response = route_request(HTTP_GET, "/jurisdictions", NULL, NULL, NULL, NULL);
    data = response ? http_response_json(response) : NULL;
    towns = test_lab_member(test_lab_member(data, "towns"), "GB-WLS");
    ok = route_check(response, 200, !strcmp(test_lab_text(json_array_get(towns, 0), "name"), "Changed database town"),
                     "GET /jurisdictions reflects database change") &&
         ok;
    json_free(data);
    http_response_free(response);

    ok = !setenv("MARS_JURISDICTION_DB_KEY", "wrong-fixture-key", 1) && ok;
    response = route_request(HTTP_GET, "/jurisdictions", NULL, NULL, NULL, NULL);
    data = response ? http_response_json(response) : NULL;
    available = true;
    bool unavailable = data && json_bool_value(test_lab_member(data, "available"), &available) && !available &&
                       !json_array_size(test_lab_member(data, "options")) &&
                       !json_object_size(test_lab_member(data, "towns")) && *test_lab_text(data, "error");
    ok = route_check(response, 200, unavailable, "GET /jurisdictions wrong key") && ok;
    json_free(data);
    http_response_free(response);
    response = route_request(HTTP_GET, "/", NULL, NULL, NULL, NULL);
    ok =
        route_check(response, 200, response_contains(response, "MARS Lab"), "page without jurisdiction database") && ok;
    http_response_free(response);
    ok = key && !setenv("MARS_JURISDICTION_DB_KEY", string_c_str(key), 1) && ok;
    string_free(key);
    return ok;
}

static void test_lab_http_catalogue(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(route_catalogue),
                     "catalogue comes from SQLCipher and reports unavailable storage");
}

static void test_lab_http_rejections(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(route_rejections),
                     "HTTP errors preserve JSON contracts and enforce Host/Origin");
}

static void test_lab_server_lifetime(void)
{
    lab_svr_free(NULL);
    TEST_ASSERT_TRUE(lab_svr_port(NULL) == 0, "null servers have no port");
    errno = 0;
    TEST_ASSERT_TRUE(lab_svr_serve_once(NULL, 0) == -1 && errno == EINVAL,
                     "null servers reject serving without dereferencing transport state");
    string_t *invalid = string_new_with("not-an-ip-address");
    lab_server_t *server = lab_svr_new(invalid, 0, 100);
    string_free(invalid);
    bool rejected = !server;
    lab_svr_free(server);
    TEST_ASSERT_TRUE(rejected, "invalid listener addresses fail atomically");
    server = lab_svr_new(NULL, 0, 100);
    bool ok = server && lab_svr_port(server) != 0 && lab_svr_serve_once(server, 0) == 0;
    lab_svr_free(server);
    TEST_ASSERT_TRUE(ok, "opaque server owns an ephemeral listener and handles idle timeouts");
}

/* Run one real server subprocess at a time through the suite's normal harness. */
void test_lab_route_cases(void)
{
    TEST_RUN_IN_GROUP(test_lab_server_lifetime, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_http_contracts, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_http_catalogue, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_http_rejections, tests, NULL);
}
