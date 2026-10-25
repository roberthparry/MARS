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
/* Keep the POSIX alternate signal-stack typedef distinct from the MARS stack API. */
#define stack_t posix_signal_stack_t
#include <signal.h>
#undef stack_t
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "array-t.h"
#include "file-t.h"
#include "globe-t.h"
#include "http-t.h"
#include "jurisdiction-t.h"
#include "sqlite-t.h"
#include "lab_server.h"
#include "lab_page.h"
#include "lab_wire.h"
#define MARS_SHARED_LAB_CALENDAR_INTERNAL_ACCESS
#include "internal/lab_calendar_internal.h"
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

static http_response_t *route_request_headers(webmethod_t method, const char *path, const char *body, const char *type,
                                              const char *host, const char *origin, const char *etag)
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
    json_t *object = body && type && !strcmp(type, "application/x-protobuf") ? test_lab_json(body) : NULL;
    array_t *encoded = object ? lab_wire_encode(object) : NULL;
    json_free(object);
    http_limits_t limits = {.connect_timeout_ms = 5000, .total_timeout_ms = 75000, .max_body_bytes = 16777216};
    bool ready =
        client && request && http_client_set_limits(client, &limits) &&
        (!host || request_header(request, "Host", host)) && (!origin || request_header(request, "Origin", origin)) &&
        (!etag || request_header(request, "If-None-Match", etag)) &&
        (!body || (payload &&
                   http_request_set_body(request, encoded ? array_get(encoded, 0) : (const void *)string_c_str(payload),
                                         encoded ? array_size(encoded) : string_byte_length(payload), content_type)));
    array_destroy(encoded);
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

static http_response_t *route_request(webmethod_t method, const char *path, const char *body, const char *type,
                                      const char *host, const char *origin)
{
    return route_request_headers(method, path, body, type, host, origin, NULL);
}

static bool response_header_equals(const http_response_t *response, const char *name, const char *expected)
{
    string_t *key = string_new_with(name);
    const string_t *value = key && response ? http_response_header_at(response, key, 0) : NULL;
    bool ok = value && string_view_equals_literal(string_view_all(value), expected);
    string_free(key);
    return ok;
}

static bool route_check_cache(const http_response_t *response, unsigned expected, bool content_ok,
                              const char *description, const char *cache_control)
{
    bool headers_ok = response_header_equals(response, "Cache-Control", cache_control) &&
                      response_header_equals(response, "X-Content-Type-Options", "nosniff");
    bool ok = response && http_response_status(response) == expected && content_ok && headers_ok;
    if (!ok) {
        json_t *error =
            response ? lab_wire_decode(http_response_body(response), http_response_body_size(response)) : NULL;
        string_fprintf(stderr, "Lab HTTP %s: expected=%u, received=%u, bytes=%zu, content=%d, headers=%d, error=%s\n",
                       description, expected, response ? http_response_status(response) : 0,
                       response ? http_response_body_size(response) : 0, content_ok, headers_ok,
                       test_lab_text(error, "error"));
        json_free(error);
    }
    return ok;
}

static bool route_check(const http_response_t *response, unsigned expected, bool content_ok, const char *description)
{
    return route_check_cache(response, expected, content_ok, description, "no-store");
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
    if (!test_lab_catalogue_database(directory) || !test_lab_offshore_database() || !test_lab_texture_database(directory))
        return false;
    if (setenv("NO_PROXY", "*", 1) || setenv("no_proxy", "*", 1))
        return false;
    http_response_t *response = route_request(HTTP_GET, "/", NULL, NULL, NULL, NULL);
    bool ok = route_check(response, 200, response_contains(response, "MARS Lab"), "GET / page");
    http_response_free(response);
    response = route_request(HTTP_POST, "/almanac-globe", "{\"west\":0,\"south\":0,\"east\":0,\"north\":1}",
                             "application/x-protobuf", NULL, NULL);
    ok = response && http_response_status(response) == 400 && ok;
    http_response_free(response);
    response = route_request(HTTP_POST, "/almanac-country", "{\"lat\":91,\"lon\":0}",
                             "application/x-protobuf", NULL, NULL);
    ok = response && http_response_status(response) == 400 && ok;
    http_response_free(response);
    response = route_request(HTTP_POST, "/almanac-country", "{\"country_code\":\"../GB\"}",
                             "application/x-protobuf", NULL, NULL);
    ok = response && http_response_status(response) == 400 && ok;
    http_response_free(response);
    response = route_request(HTTP_POST, "/almanac-country", "{\"country_code\":\"GB\"}",
                             "application/x-protobuf", NULL, NULL);
    ok = response && http_response_status(response) == 200 && ok;
    http_response_free(response);
    response = route_request(HTTP_POST, "/almanac-country", "{\"country_code\":\"GB-WLS\"}",
                             "application/x-protobuf", NULL, NULL);
    ok = response && http_response_status(response) == 200 && ok;
    http_response_free(response);
    response = route_request(HTTP_POST, "/almanac-borders", "{\"west\":0,\"south\":0,\"east\":361,\"north\":1}",
                             "application/x-protobuf", NULL, NULL);
    ok = response && http_response_status(response) == 400 && ok;
    http_response_free(response);
    response = route_request(HTTP_POST, "/almanac-borders",
                             "{\"west\":0,\"south\":0,\"east\":1,\"north\":1,\"projection_scale\":-1}",
                             "application/x-protobuf", NULL, NULL);
    ok = response && http_response_status(response) == 400 && ok;
    http_response_free(response);
    response = route_request(HTTP_POST, "/almanac-country", "{\"lat\":0,\"lon\":-30}",
                             "application/x-protobuf", NULL, NULL);
    ok = response && http_response_status(response) == 200 && ok;
    http_response_free(response);
    response = route_request(HTTP_GET, "/index.html", NULL, NULL, NULL, NULL);
    bool title_ok = response_contains(response, "<title>MARS Lab</title>");
    bool placeholders_ok = page_has_no_placeholders(response);
    bool catalogue_ok = response && !response_contains(response, "MARS_LAB_DATA");
    bool stylesheet_ok = response_contains(response, "<link rel=\"stylesheet\" href=\"/index.css\">") &&
                         response_contains(response, "html[data-starting]");
    bool scripts_ok = response_contains(response, "<script defer src=\"/js/app.js\"></script>") &&
                      !response_contains(response, "id=\"lab-config\"") &&
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
    response = route_request(HTTP_GET, "/js/calculus.js", NULL, NULL, NULL, NULL);
    ok = response && http_response_status(response) == 404 && ok;
    http_response_free(response);
    response = route_request(HTTP_GET, "/js/mobile.js", NULL, NULL, NULL, NULL);
    ok = response && http_response_status(response) == 404 && ok;
    http_response_free(response);
    response = route_request(HTTP_GET, "/js/date_picker.js", NULL, NULL, NULL, NULL);
    ok = response && http_response_status(response) == 404 && ok;
    http_response_free(response);
    response = route_request(HTTP_GET, "/js/result_text.js", NULL, NULL, NULL, NULL);
    ok = response && http_response_status(response) == 404 && ok;
    http_response_free(response);
    response = route_request(HTTP_GET, "/js/editor.js", NULL, NULL, NULL, NULL);
    ok = response && http_response_status(response) == 404 && ok;
    http_response_free(response);
    response = route_request(HTTP_GET, "/js/integrator.js", NULL, NULL, NULL, NULL);
    ok = response && http_response_status(response) == 404 && ok;
    http_response_free(response);
    response = route_request(HTTP_GET, "/js/almanac.js", NULL, NULL, NULL, NULL);
    ok = response && http_response_status(response) == 404 && ok;
    http_response_free(response);
    response = route_request(HTTP_GET, "/js/result_layout.js", NULL, NULL, NULL, NULL);
    ok = response && http_response_status(response) == 404 && ok;
    http_response_free(response);
    response = route_request(HTTP_GET, "/js/controls.js", NULL, NULL, NULL, NULL);
    ok = response && http_response_status(response) == 404 && ok;
    http_response_free(response);
    response = route_request(HTTP_GET, "/js/events.js", NULL, NULL, NULL, NULL);
    ok = response && http_response_status(response) == 404 && ok;
    http_response_free(response);
    response = route_request(HTTP_GET, "/js/worksheet.js", NULL, NULL, NULL, NULL);
    ok = response && http_response_status(response) == 404 && ok;
    http_response_free(response);
    response = route_request(HTTP_GET, "/js/evaluation.js", NULL, NULL, NULL, NULL);
    ok = response && http_response_status(response) == 404 && ok;
    http_response_free(response);
    static const char *const scripts[] = {"codec", "codec_types", "jobs", "codec_worker", "transport",
                                         "api", "app", "bindings", "bindings_controller", "locations", "globe", "globe_gpu", "globe_worker", "results",
                                         "results_services", "state", "surface", "surface_camera", "surface_export",
                                         "surface_hover", "surface_volume", "workspace", "workspace_native",
                                         "workspace_controllers"};
    for (size_t i = 0; i < sizeof scripts / sizeof *scripts; ++i) {
        string_t *url = string_sprintf("/js/%s.js", scripts[i]);
        string_t *path = string_sprintf("tools/mars_lab/build/typescript/%s.js", scripts[i]);
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
    response = route_request(HTTP_GET, "/catalogue", NULL, NULL, NULL, NULL);
    json_t *catalogue =
        response ? lab_wire_decode(http_response_body(response), http_response_body_size(response)) : NULL;
    ok = route_check(response, 200,
                     json_type(test_lab_member(catalogue, "defaults")) == JSON_OBJECT &&
                         !test_lab_member(catalogue, "towns") && !test_lab_member(catalogue, "locations") &&
                         !test_lab_member(catalogue, "options") &&
                         response_header_equals(response, "Content-Type", "application/x-protobuf"),
                     "GET /catalogue") &&
         ok;
    json_free(catalogue);
    http_response_free(response);
    response = route_request(HTTP_GET, "/js/missing.js", NULL, NULL, NULL, NULL);
    ok = response && http_response_status(response) == 404 && ok;
    http_response_free(response);
    response = route_request(HTTP_GET, "/js/native_adapters.js", NULL, NULL, NULL, NULL);
    ok = response && http_response_status(response) == 404 && ok;
    http_response_free(response);
    response = route_request(HTTP_GET, "/favicon.svg", NULL, NULL, NULL, NULL);
    ok = route_check(response, 200, response_contains(response, "<svg"), "GET /favicon.svg") && ok;
    http_response_free(response);
    response = route_request(HTTP_GET, "/earth_surface.jpg", NULL, NULL, NULL, NULL);
    const unsigned char *earth = response ? http_response_body(response) : NULL;
    ok = route_check_cache(response, 200, earth && http_response_body_size(response) > 100000 &&
                     earth[0] == 0xff && earth[1] == 0xd8 &&
                     response_header_equals(response, "Content-Type", "image/jpeg"), "GET /earth_surface.jpg",
                     "private, no-cache") && ok;
    http_response_free(response);
    response = route_request(HTTP_GET, "/earth_tiles/05-01.jpg", NULL, NULL, NULL, NULL);
    earth = response ? http_response_body(response) : NULL;
    ok = route_check_cache(response, 200, earth && http_response_body_size(response) > 1000 &&
                     earth[0] == 0xff && earth[1] == 0xd8 &&
                     response_header_equals(response, "Content-Type", "image/jpeg"), "local photographic tile",
                     "private, no-cache") && ok;
    http_response_free(response);
    response = route_request(HTTP_GET, "/earth_tiles/12-00.jpg", NULL, NULL, NULL, NULL);
    ok = response && http_response_status(response) == 404 && ok;
    http_response_free(response);
    response = route_request(HTTP_GET, "/offshore_flag.svg", NULL, NULL, NULL, NULL);
    ok = route_check(response, 200, response_contains(response, "<svg") && response_contains(response, "<path") &&
                     http_response_body_size(response) <= 4194304 &&
                     !response_contains(response, "<image") && !response_contains(response, "data:image/") &&
                     !response_contains(response, "<script") && !response_contains(response, "<foreignObject") &&
                     response_header_equals(response, "Content-Type", "image/svg+xml"), "GET /offshore_flag.svg") && ok;
    http_response_free(response);
    response = route_request(HTTP_POST, "/almanac-country", "{\"country_code\":\"ZZ-SEA\"}",
                             "application/x-protobuf", NULL, NULL);
    json_t *emblem = response ? lab_wire_decode(http_response_body(response), http_response_body_size(response)) : NULL;
    ok = route_check(response, 200, !strcmp(test_lab_text(emblem, "country_flag"), "/offshore_flag.svg") &&
                     !*test_lab_text(emblem, "country_code"), "offshore display code is not a jurisdiction") && ok;
    json_free(emblem); http_response_free(response);
    string_t *flag_path = string_new_with(getenv("MARS_JURISDICTION_DB_PATH"));
    string_t *flag_key = string_new_with(getenv("MARS_JURISDICTION_DB_KEY"));
    sqlite_t *flag_db = sqlite_open_encrypted(flag_path, flag_key);
    ok = ok && flag_db && sqlite_exec_cstr(flag_db,
        "update jurisdiction_country_flag set svg='<svg xmlns=\"http://www.w3.org/2000/svg\">"
        "<title>database-only artwork</title></svg>' where iso_country_code='ZZ-SEA'");
    sqlite_close(flag_db);
    response = route_request(HTTP_GET, "/offshore_flag.svg", NULL, NULL, NULL, NULL);
    ok = route_check(response, 200, response_contains(response, "database-only artwork"),
                     "offshore image is read from the database, never a packaged file") && ok;
    http_response_free(response);
    flag_db = sqlite_open_encrypted(flag_path, flag_key);
    ok = ok && flag_db && sqlite_exec_cstr(flag_db,
        "delete from jurisdiction_country_flag where iso_country_code='ZZ-SEA'");
    sqlite_close(flag_db); string_free(flag_path); string_free(flag_key);
    response = route_request(HTTP_GET, "/offshore_flag.svg", NULL, NULL, NULL, NULL);
    ok = response && http_response_status(response) == 404 && ok;
    http_response_free(response);
    ok = test_lab_offshore_database() && ok;
    response = route_request(HTTP_POST, "/state", "{\"expression\":\"x+8\",\"expression_updated_at\":500}",
                             "application/x-protobuf", NULL, NULL);
    json_t *state = response ? lab_wire_decode(http_response_body(response), http_response_body_size(response)) : NULL;
    const string_t *expression = json_string_value(test_lab_member(state, "expression"));
    ok = route_check(response, 200, expression && string_view_equals_literal(string_view_all(expression), "x+8"),
                     "POST /state saved expression") &&
         ok;
    json_free(state);
    http_response_free(response);
    response = route_request(HTTP_GET, "/state", NULL, NULL, NULL, NULL);
    state = response ? lab_wire_decode(http_response_body(response), http_response_body_size(response)) : NULL;
    expression = json_string_value(test_lab_member(state, "expression"));
    ok = route_check(response, 200, expression && string_view_equals_literal(string_view_all(expression), "x+8"),
                     "GET /state persisted expression") &&
         ok;
    json_free(state);
    http_response_free(response);
    response = route_request(HTTP_POST, "/eval", "{\"expression\":\"2+3\"}", "application/x-protobuf", NULL, NULL);
    json_t *result = response ? lab_wire_decode(http_response_body(response), http_response_body_size(response)) : NULL;
    const string_t *value = json_string_value(test_lab_member(result, "value"));
    ok = route_check(response, 200,
                     test_lab_ok(result, true) && value && string_view_equals_literal(string_view_all(value), "5"),
                     "POST /eval value=5") &&
         ok;
    json_free(result);
    http_response_free(response);
    response = route_request(HTTP_POST, "/eval",
        "{\"expression\":\"acos(sin(pi-x*y))\",\"action\":\"plot\",\"precision\":116,"
        "\"plot_lower\":\"-20\",\"plot_upper\":\"20\",\"plot_second_lower\":\"-20\","
        "\"plot_second_upper\":\"20\",\"plot_scale\":\"all\"}", "application/x-protobuf", NULL, NULL);
    result = response ? lab_wire_decode(http_response_body(response), http_response_body_size(response)) : NULL;
    const json_t *plot = test_lab_member(test_lab_member(test_lab_member(result, "presentation"), "calculus"), "plot");
    ok = route_check(response, 200, test_lab_ok(result, true) && strlen(test_lab_text(plot, "svg")) > 3000000 &&
                     !test_lab_member(result, "plot_svg") && !test_lab_member(result, "raw"),
                     "POST /eval refined mesh fits the complete Protobuf HTTP response") && ok;
    json_free(result);
    http_response_free(response);
    return ok;
}

/* Host is deliberately protected by the HTTP module; send hostile headers only through this bounded socket fixture. */
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
        /* Only the ASCII header is text; the response body is arbitrary Protobuf bytes. */
        for (ssize_t i = 0; ok && i < count && !string_ends_with(reply, "\r\n\r\n"); ++i)
            ok = string_append_char(reply, buffer[i]) == 0;
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
    ok = ok && eof && child > 0 && waited == child && WIFEXITED(status) && !WEXITSTATUS(status) &&
         string_starts_with(reply, "HTTP/1.1 403 ") &&
         string_find(reply, "content-type: application/x-protobuf\r\n") >= 0 &&
         string_find(reply, "cache-control: no-store\r\n") >= 0 &&
         string_find(reply, "x-content-type-options: nosniff\r\n") >= 0;
    if (!ok)
        string_fprintf(stderr, "Lab hostile request: host=%s, origin=%s, setup=%d, EOF=%d, status=%d, bytes=%zu\n",
                       host, origin ? origin : "none", ready, eof, status, reply ? string_byte_length(reply) : 0);
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
    } cases[] = {{"/eval", "{}", "application/json", NULL, NULL, 415},
                 {"/eval", "{}", "application/x-protobuf", NULL, NULL, 400},
                 {"/eval", "{", "application/x-protobuf", NULL, NULL, 400},
                 {"/eval", "[]", "application/x-protobuf", NULL, NULL, 400},
                 {"/eval", "{}", "text/plain", NULL, NULL, 415},
                 {"/eval", "{}", "application/x-protobuf", "example.invalid", NULL, 403},
                 {"/eval", "{}", "application/x-protobuf", "203.0.113.10", NULL, 403},
                 {"/eval", "{}", "application/x-protobuf", "[2001:db8::1]", NULL, 403},
                 {"/eval", "{}", "application/x-protobuf", "localhost", "https://example.invalid", 403},
                 {"/funnel-toggle", "{}", "application/x-protobuf", NULL, NULL, 410}};
    bool ok = true;
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); ++i) {
        if (cases[i].host) {
            ok = hostile_request(cases[i].host, cases[i].origin) && ok;
            continue;
        }
        http_response_t *response =
            route_request(HTTP_POST, cases[i].path, cases[i].body, cases[i].type, cases[i].host, cases[i].origin);
        json_t *result =
            response ? lab_wire_decode(http_response_body(response), http_response_body_size(response)) : NULL;
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

static bool catalogue_elevation(const jurisdict_place_t *place, void *context)
{
    size_t *count = context;
    const char *expected = !strcmp(place->name, "Shrewsbury") ? "-250" : "0";
    if (!place->elevation || strcmp(place->elevation, expected))
        return false;
    ++*count;
    return true;
}

static bool route_catalogue_elevation(const char *directory)
{
    if (!test_lab_catalogue_database(directory))
        return false;
    string_t *path = string_new_with(getenv("MARS_JURISDICTION_DB_PATH"));
    string_t *key = string_new_with(getenv("MARS_JURISDICTION_DB_KEY"));
    sqlite_t *db = path && key ? sqlite_open_encrypted(path, key) : NULL;
    bool ok = db && sqlite_exec_cstr(db,
        "UPDATE jurisdiction_town_elevation SET elevation_metres='-250' WHERE jurisdiction_town_id=1;"
        "UPDATE jurisdiction_town_elevation SET elevation_metres='-9999' WHERE jurisdiction_town_id=2;"
        "DELETE FROM jurisdiction_town_elevation WHERE jurisdiction_town_id=3;");
    sqlite_close(db);
    string_free(path);
    string_free(key);
    jurisdiction_t *engine = ok ? jurisdict_open(NULL) : NULL;
    size_t count = 0;
    int64_t next = 0;
    ok = engine && jurisdict_each_town(engine, catalogue_elevation, &count) && count == 3 && ok;
    count = 0;
    ok = jurisdict_each_town_batch(engine, 0, 32, catalogue_elevation, &count, &next) && count == 3 && ok;
    count = 0;
    ok = jurisdict_each_town_near_batch(engine, 53.3, -3.5, 0, 32, catalogue_elevation, &count, &next) &&
         count == 2 && ok;
    jurisdict_close(engine);
    static const struct { const char *mode, *latitude, *expected, *source; } heights[] = {
        {"legacy", "52.707700", "-250", "catalogue"},
        {"catalogue", "52.707700", "-250", "catalogue"},
        {"manual", "52.707700", "17", "manual"},
        {"legacy", "52.710000", "17", "retained"},
        {"manual", "52.710000", "17", "manual"},
    };
    for (size_t i = 0; i < sizeof heights / sizeof *heights; ++i) {
        json_t *observer = json_new_object();
        lab_cal_set(observer, "lat", heights[i].latitude);
        lab_cal_set(observer, "lon", "-2.754100");
        lab_cal_set(observer, "jurisdiction", "GB");
        lab_cal_set(observer, "elevation", "17");
        lab_cal_set(observer, "elevation_mode", heights[i].mode);
        lab_cal_resolve_observer(observer);
        ok = ok && !strcmp(lab_cal_text(observer, "elevation"), heights[i].expected) &&
             !strcmp(lab_cal_text(observer, "elevation_mode"), heights[i].source);
        if (i < 3) ok = ok && !strcmp(lab_cal_text(observer, "town"), "Shrewsbury|52.7077|-2.7541|-250") &&
                             !strcmp(lab_cal_text(observer, "jurisdiction"), "GB-ENG");
        else ok = ok && !*lab_cal_text(observer, "town");
        json_free(observer);
    }
    http_response_t *response = route_request(HTTP_GET, "/towns?after=0", NULL, NULL, NULL, NULL);
    json_t *data = response ? lab_wire_decode(http_response_body(response), http_response_body_size(response)) : NULL;
    const json_t *towns = test_lab_member(test_lab_member(data, "towns"), "GB-WLS");
    ok = route_check(response, 200,
        !strcmp(test_lab_text(json_array_get(towns, 0), "value"), "Rhyl|53.3190|-3.4916|0"),
        "missing DEM elevation is not sent to observer selection") && ok;
    json_free(data);
    http_response_free(response);
    return ok;
}

static void test_lab_catalogue_elevation(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(route_catalogue_elevation),
        "legacy no-data and absent elevations use zero; real below-sea-level elevations survive all catalogue paths");
}

static bool route_catalogue_country(const char *directory)
{
    if (!test_lab_catalogue_database(directory)) return false;
    string_t *path = string_sprintf("%s/geometry.db", directory);
    sqlite_t *geometry = sqlite_open_plain(path);
    file_t *sources[2] = {NULL, NULL};
    const char *extensions[] = {"shp", "dbf"};
    bool ok = geometry != NULL;
    for (unsigned i = 0; ok && i < 2; ++i) {
        string_t *packed_path = string_sprintf("packaging/globe-db/map-units-5.1.1.%s.zst", extensions[i]);
        string_t *plain_path = string_sprintf("%s/map.%s", directory, extensions[i]);
        file_t *packed = file_new(packed_path);
        sources[i] = file_new(plain_path);
        ok = packed && sources[i] && file_decompress_zstd(packed, sources[i], 16 * 1048576, false) &&
             file_open_read(sources[i]);
        file_free(packed); string_free(packed_path); string_free(plain_path);
    }
    ok = ok && globe_import_countries(geometry, sources[0], sources[1]);
    file_t *regions = file_new_cstr("packaging/globe-db/flag_regions.geojson");
    ok = ok && regions && globe_import_regions(geometry, regions);
    file_free(regions);
    file_free(sources[0]); file_free(sources[1]); sqlite_close(geometry);
    ok = ok && !setenv("MARS_GLOBE_DB_PATH", string_c_str(path), 1) && !setenv("MARS_GLOBE_DB_KEY", "", 1);
    string_free(path);
    path = string_new_with(getenv("MARS_JURISDICTION_DB_PATH"));
    string_t *key = string_new_with(getenv("MARS_JURISDICTION_DB_KEY"));
    sqlite_t *db = sqlite_open_encrypted(path, key);
    ok = ok && db && sqlite_exec_cstr(db,
        "UPDATE jurisdiction_town SET jurisdiction_id='GB';"
        "UPDATE jurisdiction_town SET town_name='London' WHERE jurisdiction_town_id=1;"
        "UPDATE jurisdiction_town_latitude SET latitude='51.5074' WHERE jurisdiction_town_id=1;"
        "UPDATE jurisdiction_town_longitude SET longitude='-0.1278' WHERE jurisdiction_town_id=1;"
        "UPDATE jurisdiction_town SET town_name='Cardiff' WHERE jurisdiction_town_id=2;"
        "UPDATE jurisdiction_town_latitude SET latitude='51.4816' WHERE jurisdiction_town_id=2;"
        "UPDATE jurisdiction_town_longitude SET longitude='-3.1791' WHERE jurisdiction_town_id=2;"
        "UPDATE jurisdiction_town SET town_name='Edinburgh' WHERE jurisdiction_town_id=3;"
        "UPDATE jurisdiction_town_latitude SET latitude='55.9533' WHERE jurisdiction_town_id=3;"
        "UPDATE jurisdiction_town_longitude SET longitude='-3.1883' WHERE jurisdiction_town_id=3;"
        "CREATE TABLE jurisdiction_flag_region(code text primary key,name text,parent_code text,artwork_code text);"
        "INSERT INTO jurisdiction_flag_region VALUES('GB-CON','Cornwall, England','GB-ENG','GB-CON');"
        "CREATE TABLE jurisdiction_country_flag(iso_country_code text primary key,svg text);"
        "INSERT INTO jurisdiction_country_flag VALUES('GB-CON','<svg xmlns=\"http://www.w3.org/2000/svg\" "
        "viewBox=\"0 0 10 10\"><path d=\"M0 0h10v10H0z\"/></svg>'),"
        "('GB-ENG','<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 10 10\">"
        "<path fill=\"red\" d=\"M0 0h10v10H0z\"/></svg>');");
    sqlite_close(db); string_free(path); string_free(key);
    json_t *catalogue = ok ? lab_page_town_jurisdiction_batch(0, "GB") : NULL;
    const json_t *towns = test_lab_member(test_lab_member(catalogue, "towns"), "GB");
    if (!ok || json_array_size(towns) != 3) {
        string_t *diagnostic = catalogue ? json_to_string(catalogue) : NULL;
        string_fprintf(stderr, "Country fixture ready=%d catalogue=%s\n", ok,
                       diagnostic ? string_c_str(diagnostic) : "null");
        string_free(diagnostic);
    }
    ok = ok && json_array_size(towns) == 3;
    for (size_t i = 0; ok && i < 3; ++i) {
        const json_t *row = json_array_get(towns, i);
        const char *name = test_lab_text(row, "name");
        const char *expected = !strcmp(name, "London") ? "GB-ENG" : !strcmp(name, "Cardiff") ? "GB-WLS" : "GB-SCT";
        ok = !strcmp(test_lab_text(row, "country_code"), expected);
        if (!ok) string_fprintf(stderr, "Country fixture %s expected=%s actual=%s\n", name, expected,
                               test_lab_text(row, "country_code"));
    }
    json_free(catalogue);
    http_response_t *response = route_request(HTTP_POST, "/almanac-country", "{\"lat\":50.2632,\"lon\":-5.051}",
                                              "application/x-protobuf", NULL, NULL);
    json_t *display = response ? lab_wire_decode(http_response_body(response), http_response_body_size(response)) : NULL;
    ok = ok && response && http_response_status(response) == 200 &&
         !strcmp(test_lab_text(display, "country_code"), "GB-ENG") &&
         !strcmp(test_lab_text(display, "flag_code"), "GB-CON") &&
         !strcmp(test_lab_text(display, "country"), "Cornwall, England") &&
         !strncmp(test_lab_text(display, "country_flag"), "data:image/svg+xml;base64,", 26);
    json_t *fields = test_lab_json("{\"snapshot\":\"MOON|Moon|moon|0|0|0|0|0|1|0.5|0|0|0|0|YES|0|0|0|10|20|0.5\"}");
    json_t *options = display ? json_clone(display) : NULL;
    lab_cal_set(options, "date", "2026-10-24"); lab_cal_set(options, "time", "12:00:00");
    lab_cal_set(options, "zone", "1"); lab_cal_set(options, "elevation", "0");
    lab_cal_set(options, "town", "Truro|50.2632|-5.0510|0");
    lab_cal_set(options, "jurisdiction", "GB-ENG");
    lab_cal_set(options, "lat", "50.2632"); lab_cal_set(options, "lon", "-5.0510");
    lab_cal_set(options, "observer_surface", "Land"); lab_cal_set(options, "defer_events", "true");
    unsigned status = 200;
    json_t *card = ok ? lab_cal_almanac(fields, options, &status) : NULL;
    const json_t *cell = json_array_get(test_lab_member(card, "observer_summary"), 1);
    const json_t *rendered = test_lab_member(test_lab_member(card, "almanac_presentation"), "all");
    ok = ok && status == 200 && !strcmp(test_lab_text(cell, "value"), "Truro, Cornwall, England") &&
         !strcmp(test_lab_text(cell, "flag"), test_lab_text(display, "country_flag")) &&
         strstr(test_lab_text(rendered, "html"), test_lab_text(display, "country_flag")) &&
         !strcmp(lab_cal_observer_jurisdiction(options), "GB-ENG");
    json_free(card); json_free(options); json_free(fields);
    json_free(display); http_response_free(response);
    path = string_new_with(getenv("MARS_JURISDICTION_DB_PATH"));
    key = string_new_with(getenv("MARS_JURISDICTION_DB_KEY"));
    db = sqlite_open_encrypted(path, key);
    ok = ok && db && sqlite_exec_cstr(db,
        "UPDATE jurisdiction_town SET town_name='Truro' WHERE jurisdiction_town_id=1;"
        "UPDATE jurisdiction_town_latitude SET latitude='50.2632' WHERE jurisdiction_town_id=1;"
        "UPDATE jurisdiction_town_longitude SET longitude='-5.0510' WHERE jurisdiction_town_id=1;");
    sqlite_close(db); string_free(path); string_free(key);
    catalogue = ok ? lab_page_town_jurisdiction_batch(0, "GB") : NULL;
    towns = test_lab_member(test_lab_member(catalogue, "towns"), "GB");
    bool cornish = false;
    for (size_t i = 0; i < json_array_size(towns); ++i) {
        const json_t *row = json_array_get(towns, i);
        if (!strcmp(test_lab_text(row, "name"), "Truro"))
            cornish = !strcmp(test_lab_text(row, "country_code"), "GB-CON");
    }
    ok = ok && cornish;
    json_free(catalogue);
    return ok;
}

static void test_lab_catalogue_country(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(route_catalogue_country),
        "UK catalogue towns retain their group but display their geographic constituent-country flags");
}

static bool route_catalogue(const char *directory)
{
    if (!test_lab_catalogue_database(directory))
        return false;
    http_response_t *response = route_request(HTTP_GET, "/jurisdictions", NULL, NULL, NULL, NULL);
    json_t *data = response ? lab_wire_decode(http_response_body(response), http_response_body_size(response)) : NULL;
    bool index_ok = data && !json_object_size(test_lab_member(data, "towns")) &&
                    json_array_size(test_lab_member(data, "options")) == 4 &&
                    json_array_size(test_lab_member(test_lab_member(data, "locations"), "GB-WLS")) == 4;
    json_free(data);
    http_response_free(response);
    response = route_request(HTTP_GET, "/towns?after=0", NULL, NULL, NULL, NULL);
    data = response ? lab_wire_decode(http_response_body(response), http_response_body_size(response)) : NULL;
    bool available = false;
    const json_t *towns = test_lab_member(test_lab_member(data, "towns"), "GB-WLS");
    bool ok = index_ok && data && json_bool_value(test_lab_member(data, "available"), &available) && available &&
              json_array_size(towns) == 1 &&
              !strcmp(test_lab_text(json_array_get(towns, 0), "name"), "Rhyl") &&
              !strcmp(test_lab_text(json_array_get(towns, 0), "value"), "Rhyl|53.3190|-3.4916|5") &&
              !strcmp(test_lab_text(json_array_get(towns, 0), "detail"), "+53.3190  -003.4916");
    ok = route_check(response, 200, ok, "GET /jurisdictions database rows") && ok;
    json_free(data);
    http_response_free(response);

    string_t *path = string_new_with(getenv("MARS_JURISDICTION_DB_PATH"));
    string_t *key = string_new_with(getenv("MARS_JURISDICTION_DB_KEY"));
    sqlite_t *db = path && key ? sqlite_open_encrypted(path, key) : NULL;
    string_t *sql = string_new_with("update jurisdiction_town set town_name='Changed database town' "
                                    "where jurisdiction_town_id=2;");
    ok = db && sql && sqlite_exec(db, sql) && ok;
    ok = ok && sqlite_exec_cstr(db,
        "PRAGMA cache_size=-512;"
        "BEGIN; WITH RECURSIVE ids(id) AS (VALUES(4) UNION ALL SELECT id+1 FROM ids WHERE id<50000) "
        "INSERT INTO jurisdiction_town SELECT id,'US','Town '||id FROM ids;"
        "INSERT INTO jurisdiction_town_latitude SELECT jurisdiction_town_id,'40' FROM jurisdiction_town WHERE jurisdiction_town_id>3;"
        "INSERT INTO jurisdiction_town_longitude SELECT jurisdiction_town_id,'-74' FROM jurisdiction_town WHERE jurisdiction_town_id>3;"
        "INSERT INTO jurisdiction_town_timezone SELECT jurisdiction_town_id,2 FROM jurisdiction_town WHERE jurisdiction_town_id>3; COMMIT;"
        "ALTER TABLE jurisdiction_town RENAME TO town_storage;"
        "CREATE VIEW jurisdiction_town AS SELECT * FROM town_storage;");
    string_free(sql);
    sqlite_close(db);
    string_free(path);
    response = route_request(HTTP_GET, "/towns", NULL, NULL, NULL, NULL);
    data = response ? lab_wire_decode(http_response_body(response), http_response_body_size(response)) : NULL;
    towns = test_lab_member(test_lab_member(data, "towns"), "GB-WLS");
    ok = route_check(response, 200, !strcmp(test_lab_text(json_array_get(towns, 0), "name"), "Changed database town"),
                     "GET /jurisdictions reflects database change") &&
         ok;
    json_free(data);
    http_response_free(response);

    response = route_request(HTTP_GET, "/towns?after=0&lat=53.3&lon=-3.5", NULL, NULL, NULL, NULL);
    data = response ? lab_wire_decode(http_response_body(response), http_response_body_size(response)) : NULL;
    ok = route_check(response, 200,
                     json_array_size(test_lab_member(test_lab_member(data, "towns"), "GB-WLS")) == 1 &&
                     json_array_size(test_lab_member(test_lab_member(data, "towns"), "US")) == 0,
                     "local priority page excludes distant towns") && ok;
    json_free(data);
    http_response_free(response);
    response = route_request(HTTP_GET, "/towns?after=0&lat=nan&lon=0", NULL, NULL, NULL, NULL);
    ok = route_check(response, 400, true, "reject non-finite town centre") && ok;
    http_response_free(response);
    response = route_request(HTTP_GET, "/towns?after=&lat=0&lon=0", NULL, NULL, NULL, NULL);
    ok = route_check(response, 400, true, "reject missing town cursor") && ok;
    http_response_free(response);

    /* The complete fixture exceeds the value-node budget; every HTTP page must still round-trip. */
    int64_t cursor = 0;
    size_t total = 0;
    for (unsigned page = 0; ok && page < 100; ++page) {
        string_t *url = string_sprintf("/towns?after=%lld", (long long)cursor);
        response = url ? route_request(HTTP_GET, string_c_str(url), NULL, NULL, NULL, NULL) : NULL;
        data = response ? lab_wire_decode(http_response_body(response), http_response_body_size(response)) : NULL;
        const json_t *groups = test_lab_member(data, "towns");
        size_t count = 0;
        for (size_t i = 0; i < json_object_size(groups); ++i)
            count += json_array_size(json_object_value_at(groups, i));
        int64_t next = strtoll(test_lab_text(data, "next"), NULL, 10);
        ok = route_check(response, 200, data && count <= 512 &&
                         ((count && next > cursor) || (!count && next == 0)), "bounded town page") && ok;
        total += count;
        cursor = next;
        string_free(url);
        json_free(data);
        http_response_free(response);
        if (!cursor)
            break;
    }
    ok = ok && total == 50000 && cursor == 0;
    path = string_new_with(getenv("MARS_JURISDICTION_DB_PATH"));
    db = path && key ? sqlite_open_encrypted(path, key) : NULL;
    ok = ok && db && sqlite_exec_cstr(db,
        "UPDATE jurisdiction_town_latitude SET latitude='0' WHERE jurisdiction_town_id=50000;"
        "UPDATE jurisdiction_town_longitude SET longitude='179.9' WHERE jurisdiction_town_id=50000;");
    sqlite_close(db);
    string_free(path);
    response = route_request(HTTP_GET, "/towns?after=0&lat=0&lon=-179.9", NULL, NULL, NULL, NULL);
    data = response ? lab_wire_decode(http_response_body(response), http_response_body_size(response)) : NULL;
    ok = route_check(response, 200,
                     json_array_size(test_lab_member(test_lab_member(data, "towns"), "US")) == 1,
                     "local priority window wraps at date line") && ok;
    json_free(data);
    http_response_free(response);
    response = route_request(HTTP_GET, "/towns?after=-1", NULL, NULL, NULL, NULL);
    ok = route_check(response, 400, true, "reject invalid town cursor") && ok;
    http_response_free(response);

    ok = !setenv("MARS_JURISDICTION_DB_KEY", "wrong-fixture-key", 1) && ok;
    response = route_request(HTTP_GET, "/jurisdictions", NULL, NULL, NULL, NULL);
    data = response ? lab_wire_decode(http_response_body(response), http_response_body_size(response)) : NULL;
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
static bool route_rivers(const char *directory)
{
    if (!test_lab_catalogue_database(directory)) return false;
    string_t *globe_path = string_sprintf("%s/globe.db", directory);
    bool configured = globe_path && !setenv("MARS_GLOBE_DB_PATH", string_c_str(globe_path), 1) &&
                      !setenv("MARS_GLOBE_DB_KEY", "river-globe-test", 1);
    string_free(globe_path);
    if (!configured) return false;
    string_t *config_directory = string_sprintf("%s/config", directory);
    string_t *config_path = string_sprintf("%s/config/globe-db.env", directory);
    string_t *config_text = string_sprintf("MARS_GLOBE_DB_PATH=%s\nMARS_GLOBE_DB_KEY=river-globe-test\n",
                                           getenv("MARS_GLOBE_DB_PATH"));
    file_t *config_folder = config_directory ? file_new(config_directory) : NULL;
    file_t *config_file = config_path ? file_new(config_path) : NULL;
    configured = config_folder && config_file && config_text && file_create_directory(config_folder, 0700, false) &&
                 file_write_all_text(config_file, config_text) && !setenv("MARS_HOME", directory, 1);
    file_free(config_folder); file_free(config_file);
    string_free(config_directory); string_free(config_path); string_free(config_text);
    if (!configured) return false;
    if (setenv("NO_PROXY", "*", 1) || setenv("no_proxy", "*", 1)) return false;
    const char *bounds = "{\"west\":-1,\"south\":-1,\"east\":2,\"north\":2}";
    http_response_t *response = route_request(HTTP_POST, "/almanac-rivers", bounds,
                                             "application/x-protobuf", NULL, NULL);
    bool ok = response && http_response_status(response) == 503;
    http_response_free(response);
    response = route_request(HTTP_POST, "/almanac-rivers", "{\"west\":-180,\"south\":-90,\"east\":180,\"north\":90}",
                             "application/x-protobuf", NULL, NULL);
    ok = response && http_response_status(response) == 400 && ok;
    http_response_free(response);
    string_t *path = string_new_with(getenv("MARS_GLOBE_DB_PATH"));
    string_t *key = string_new_with(getenv("MARS_GLOBE_DB_KEY"));
    sqlite_t *db = sqlite_open_encrypted(path, key);
    string_free(path); string_free(key);
    ok = db && sqlite_exec_cstr(db,
        "create table rivers_dataset(id integer,format_version integer,point_count integer,source text);"
        "insert into rivers_dataset values(1,1,2,'synthetic fixture');"
        "create table rivers_chunk(id integer primary key,feature_id integer,kind integer,vertices blob);"
        "insert into rivers_chunk values(1,1,1,x'0000000000000000000f4240000f4240');"
        "create virtual table rivers_bounds using rtree_i32(id,min_x,max_x,min_y,max_y);"
        "insert into rivers_bounds values(1,0,1000000,0,1000000);"
        "create table country_border_chunk(id integer primary key,points blob not null);"
        "insert into country_border_chunk values(1,x'000000000000000040420f0040420f00');"
        "create virtual table country_border_bounds using rtree_i32(id,west,east,south,north);"
        "insert into country_border_bounds values(1,0,1000000,0,1000000);") && ok;
    sqlite_close(db);
    response = route_request(HTTP_POST, "/almanac-rivers", bounds, "application/x-protobuf", NULL, NULL);
    json_t *data = response ? lab_wire_decode(http_response_body(response), http_response_body_size(response)) : NULL;
    const char *image = test_lab_text(data, "image");
    ok = route_check(response, 200, image && strstr(image, "<svg") && strstr(image, "M682.667"),
                     "bounded river overlay") && ok;
    json_free(data);
    http_response_free(response);
    response = route_request(HTTP_POST, "/almanac-borders",
        "{\"west\":-1,\"south\":-1,\"east\":2,\"north\":2,\"format\":\"segments\"}",
        "application/x-protobuf", NULL, NULL);
    data = response ? lab_wire_decode(http_response_body(response), http_response_body_size(response)) : NULL;
    ok = route_check(response, 200,
        !strcmp(test_lab_text(data, "segments"), "0.000000 0.000000 1.000000 1.000000\n") &&
        !*test_lab_text(data, "image"), "geographic border vectors without raster imagery") && ok;
    json_free(data);
    http_response_free(response);
    path = string_new_with(getenv("MARS_GLOBE_DB_PATH"));
    key = string_new_with(getenv("MARS_GLOBE_DB_KEY"));
    db = sqlite_open_encrypted(path, key);
    string_free(path); string_free(key);
    ok = db && sqlite_exec_cstr(db, "update rivers_dataset set source='WDBII / GSHHG 2.3.7'") && ok;
    sqlite_close(db);
    response = route_request(HTTP_POST, "/almanac-rivers", bounds, "application/x-protobuf", NULL, NULL);
    ok = response && http_response_status(response) == 503 && ok;
    http_response_free(response);
    return ok;
}

static bool route_textures(const char *directory)
{
    if (!test_lab_texture_database(directory)) return false;
    file_t *source = file_new_cstr("packaging/globe-db/textures/earth_surface.jpg");
    array_t *original = source ? file_read_all_bytes(source) : NULL;
    string_t *hash = source ? file_sha256(source) : NULL;
    string_t *etag = hash ? string_sprintf("\"sha256-%s\"", string_c_str(hash)) : NULL;
    http_response_t *response = route_request(HTTP_GET, "/earth_surface.jpg", NULL, NULL, NULL, NULL);
    bool ok = original && etag && response && http_response_status(response) == 200 &&
              http_response_body_size(response) == array_size(original) &&
              !memcmp(http_response_body(response), array_get(original, 0), array_size(original)) &&
              response_header_equals(response, "ETag", string_c_str(etag)) &&
              response_header_equals(response, "Cache-Control", "private, no-cache");
    http_response_free(response);
    string_t *weak = etag ? string_sprintf("\"outdated\", W/%s", string_c_str(etag)) : NULL;
    const char *validators[] = {etag ? string_c_str(etag) : "", weak ? string_c_str(weak) : "", "*"};
    for (unsigned i = 0; ok && i < 3; ++i) {
        response = route_request_headers(HTTP_GET, "/earth_surface.jpg", NULL, NULL, NULL, NULL, validators[i]);
        ok = response && http_response_status(response) == 304 && !http_response_body_size(response) &&
             response_header_equals(response, "ETag", string_c_str(etag));
        http_response_free(response);
    }
    response = route_request_headers(HTTP_GET, "/earth_surface.jpg", NULL, NULL, NULL, "https://public.invalid",
                                     etag ? string_c_str(etag) : "*");
    ok = response && http_response_status(response) == 403 &&
         response_header_equals(response, "Cache-Control", "no-store") && ok;
    http_response_free(response);
    response = route_request_headers(HTTP_GET, "/earth_surface.jpg", NULL, NULL, NULL, NULL, "\"outdated\"");
    ok = response && http_response_status(response) == 200 && http_response_body_size(response) == array_size(original) && ok;
    http_response_free(response);
    response = route_request(HTTP_GET, "/earth_surface_detail.jpg", NULL, NULL, NULL, NULL);
    ok = response && http_response_status(response) == 200 && http_response_body_size(response) == 9361205 && ok;
    http_response_free(response);
    string_t *path = string_new_with(getenv("MARS_GLOBE_DB_PATH"));
    sqlite_t *db = path ? sqlite_open_plain(path) : NULL;
    ok = db && sqlite_exec_cstr(db, "update globe_texture set jpeg=zeroblob(byte_count) where level=0") && ok;
    sqlite_close(db);
    response = route_request(HTTP_GET, "/earth_surface.jpg", NULL, NULL, NULL, NULL);
    ok = response && http_response_status(response) == 500 && response_header_equals(response, "Cache-Control", "no-store") && ok;
    http_response_free(response);
    db = path ? sqlite_open_plain(path) : NULL;
    ok = db && sqlite_exec_cstr(db, "drop table globe_texture") && ok;
    sqlite_close(db);
    response = route_request(HTTP_GET, "/earth_surface.jpg", NULL, NULL, NULL, NULL);
    ok = response && http_response_status(response) == 404 && ok;
    http_response_free(response);
    string_free(path); string_free(weak); string_free(etag); string_free(hash); array_destroy(original); file_free(source);
    return ok;
}

static void test_lab_http_textures(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(route_textures), "database image bytes, conditional caching, corruption and missing-layer responses");
}

static void test_lab_http_rivers(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(route_rivers), "river overlay uses independent globe data and rejects wide requests");
}

void test_lab_route_cases(void)
{
    TEST_RUN_IN_GROUP(test_lab_server_lifetime, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_http_contracts, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_http_catalogue, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_catalogue_elevation, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_catalogue_country, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_http_rejections, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_http_rivers, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_http_textures, tests, NULL);
}
