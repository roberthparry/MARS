/**
 * @file test_webserver.c
 * @brief Native web-server protocol and lifecycle regressions.
 *
 * Checks routes, binary and document bodies, malformed requests, size limits, deadlines and response handling. The
 * runnable greeting example is executed after ordinary server assertions.
 *
 * Used by the project test harness for regression verification. Select cases through tests/test_config.json and
 * run suites sequentially; this source is not part of the installed library.
 */

#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include "webserver.h"
#include "test_harness.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

#define main webserver_example_main
#include "../../scratch/webserver_hello.c"
#undef main

static bool contains(const string_t *text, const char *part)
{
    string_t *needle = string_new_with(part);
    string_view_t v = string_view_all(text), n = string_view_all(needle);
    bool found = false;
    for (size_t i = 0; needle && i + string_view_length(n) <= string_view_length(v); ++i) {
        if (string_view_equals_view(string_view_slice(v, i, string_view_length(n)), n)) {
            found = true;
            break;
        }
    }
    string_free(needle);
    return found;
}

static bool echo(const websrv_request_t *request, websrv_response_t *response, void *context)
{
    (void)context;
    return websrv_response_body(response, websrv_request_body(request), websrv_request_body_size(request));
}

static bool inspect(const websrv_request_t *request, websrv_response_t *response, void *context)
{
    (void)context;
    string_t *name = string_new_with("X-TEST");
    const string_t *value = websrv_request_header(request, name);
    string_t *body = string_sprintf("%d %S %S %S", websrv_request_method(request),
        websrv_request_path(request), websrv_request_target(request), value);
    bool ok = body && websrv_response_text(response, body);
    string_free(name);
    string_free(body);
    return ok;
}

static bool document(const websrv_request_t *request, websrv_response_t *response, void *context)
{
    if (context) {
        xml_t *xml = websrv_request_xml(request);
        bool ok = xml && websrv_response_xml(response, xml);
        xml_free(xml);
        return ok;
    }
    json_t *json = websrv_request_json(request);
    bool ok = json && websrv_response_json(response, json);
    json_free(json);
    return ok;
}

static bool special(const websrv_request_t *request, websrv_response_t *response, void *context)
{
    (void)request;
    unsigned status = *(unsigned *)context;
    if (!status) {
        websrv_response_body(response, "secret", 6);
        return false;
    }
    return websrv_response_status(response, status) && websrv_response_body(response, "body", 4);
}

static websrv_t *fixture(websrv_handler_fn handler, void *context, const websrv_limits_t *limits)
{
    websrv_t *server = websrv_new(NULL, 0, limits);
    string_t *path = string_new_with("/echo");
    bool ok = server && path;
    for (unsigned i = 0; ok && i <= HTTP_OPTIONS; ++i)
        ok = websrv_route(server, (webmethod_t)i, path, handler, context);
    string_free(path);
    if (ok) return server;
    websrv_free(server);
    return NULL;
}

/* Fork only the server; all test cases and clients run strictly sequentially. */
static string_t *exchange(websrv_t *server, const void *data, size_t size, bool shutdown_write, int expected)
{
    if (!server) return NULL;
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = htons(websrv_port(server)),
                              .sin_addr.s_addr = htonl(INADDR_LOOPBACK)};
    struct timeval timeout = {.tv_sec = 3};
    if (fd < 0) return NULL;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr))) { close(fd); return NULL; }
    pid_t pid = fork();
    if (pid == 0) {
        close(fd);
        int rc = websrv_serve_once(server, 1000);
        bool ok = rc == expected;
        if (expected == -1 && !shutdown_write) ok = ok && errno == ETIMEDOUT;
        websrv_free(server);
        _exit(ok ? 0 : 1);
    }
    if (pid < 0) { close(fd); return NULL; }
    bool ok = true;
    size_t sent = 0;
    while (sent < size) {
        ssize_t n = send(fd, (const char *)data + sent, size - sent, MSG_NOSIGNAL);
        if (n <= 0) { ok = false; break; }
        sent += (size_t)n;
    }
    if (shutdown_write) shutdown(fd, SHUT_WR);
    string_t *reply = string_new();
    char bytes[1024];
    for (;;) {
        ssize_t n = recv(fd, bytes, sizeof(bytes), 0);
        if (n <= 0) break;
        if (!reply || string_append_chars(reply, bytes, (size_t)n)) { ok = false; break; }
    }
    close(fd);
    int status = 0;
    ok = waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0 && ok;
    if (!ok) { string_free(reply); reply = NULL; }
    return reply;
}

static string_t *send_text(websrv_t *server, const char *text)
{
    string_t *input = string_new_with(text);
    string_t *reply = input ? exchange(server, string_c_str(input), string_byte_length(input), true, 1) : NULL;
    string_free(input);
    return reply;
}

static void test_websrv_lifetime(void)
{
    websrv_free(NULL);
    TEST_ASSERT_INT_EQ(websrv_port(NULL), 0);
    TEST_ASSERT_INT_EQ(websrv_serve_once(NULL, 0), -1);
    websrv_limits_t bad = {.max_header_bytes = 1};
    TEST_ASSERT_TRUE(!websrv_new(NULL, 0, &bad), "reject unusable limits");
    string_t *address = string_new_with("localhost");
    websrv_t *invalid = websrv_new(address, 0, NULL);
    string_free(address);
    TEST_ASSERT_TRUE(!invalid, "numeric IPv4 only");
    websrv_t *server = websrv_new(NULL, 0, NULL);
    TEST_ASSERT_NOT_NULL(server);
    bool ok = websrv_port(server) != 0 && websrv_serve_once(server, 0) == 0;
    string_t *path = string_new_with("/bad?query");
    ok = ok && !websrv_route(server, HTTP_GET, path, echo, NULL);
    string_free(path);
    path = string_new_with("/echo");
    ok = ok && !websrv_route(server, (webmethod_t)99, path, echo, NULL) &&
         websrv_route(server, HTTP_GET, path, echo, NULL) &&
         websrv_route(server, HTTP_GET, path, inspect, NULL);
    string_free(path);
    websrv_free(server);
    TEST_ASSERT_TRUE(ok, "listener lifecycle, invalid routes and replacement");
}

static void test_websrv_routing(void)
{
    websrv_t *server = websrv_new(NULL, 0, NULL);
    string_t *path = string_new_with("/echo");
    bool ok = server && websrv_route(server, HTTP_GET, path, inspect, NULL);
    string_free(path);
    string_t *reply = ok ? send_text(server, "GET /echo?q=1 HTTP/1.1\r\nHost: localhost\r\nX-Test: yes\r\n\r\n") : NULL;
    ok = reply && contains(reply, "200 Response") && contains(reply, "0 /echo /echo?q=1 yes");
    string_free(reply);
    reply = send_text(server, "POST /echo HTTP/1.1\r\nHost: localhost\r\n\r\n");
    ok = ok && reply && contains(reply, "405 Response") && contains(reply, "allow: GET\r\n");
    string_free(reply);
    reply = send_text(server, "GET /other HTTP/1.1\r\nHost: localhost\r\n\r\n");
    ok = ok && reply && contains(reply, "404 Response");
    string_free(reply);
    websrv_free(server);
    TEST_ASSERT_TRUE(ok, "method dispatch, path/query separation, 404 and 405");
}

static void test_websrv_binary_and_methods(void)
{
    static const char *const methods[] = {"GET", "POST", "PUT", "PATCH", "DELETE", "HEAD", "OPTIONS"};
    websrv_t *server = fixture(echo, NULL, NULL);
    bool ok = server != NULL;
    for (unsigned i = 0; ok && i <= HTTP_OPTIONS; ++i) {
        string_t *input = string_sprintf("%s /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 3\r\n\r\n", methods[i]);
        const char bytes[] = {'a', 0, (char)255};
        string_append_chars(input, bytes, sizeof(bytes));
        string_t *reply = exchange(server, string_c_str(input), string_byte_length(input), true, 1);
        ok = reply && contains(reply, "Content-Length: 3\r\n");
        if (ok && i != HTTP_HEAD) {
            string_view_t v = string_view(reply, string_byte_length(reply) - 3, 3);
            string_view_t sent = string_view(input, string_byte_length(input) - 3, 3);
            ok = string_view_equals_view(v, sent);
        }
        if (ok && i == HTTP_HEAD) {
            string_view_t end = string_view(reply, string_byte_length(reply) - 4, 4);
            ok = string_view_equals_literal(end, "\r\n\r\n");
        }
        string_free(input);
        string_free(reply);
    }
    websrv_free(server);
    TEST_ASSERT_TRUE(ok, "binary round trip, all methods and HEAD suppression");
}

static void test_websrv_documents(void)
{
    bool ok = true;
    for (unsigned i = 0; ok && i < 2; ++i) {
        websrv_t *server = fixture(document, i ? &ok : NULL, NULL);
        const char *body = i ? "<answer>42</answer>" : "{\"answer\":42}";
        string_t *text = string_new_with(body);
        string_t *input = string_sprintf("POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: %zu\r\n\r\n%S",
                                        string_byte_length(text), text);
        string_t *reply = exchange(server, string_c_str(input), string_byte_length(input), true, 1);
        ok = reply && contains(reply, "200 Response") && contains(reply, body) &&
             contains(reply, i ? "content-type: application/xml" : "content-type: application/json");
        string_free(text);
        string_free(input);
        string_free(reply);
        reply = send_text(server, "POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 1\r\n\r\n!");
        ok = ok && reply && contains(reply, "500 Response");
        string_free(reply);
        websrv_free(server);
    }
    TEST_ASSERT_TRUE(ok, "native JSON/XML round trip and invalid document failure");
}

static void test_websrv_malformed(void)
{
    static const struct { const char *input; const char *status; } cases[] = {
        {"GET /echo HTTP/1.1\r\n\r\n", "400"},
        {"GET /echo HTTP/1.1\r\nHost: a\r\nHOST: b\r\n\r\n", "400"},
        {"GET /echo HTTP/1.1\r\nHost : a\r\n\r\n", "400"},
        {"GET /echo HTTP/1.1\r\nHost: a\r\n x: y\r\n\r\n", "400"},
        {"GET /echo HTTP/1.1\r\nHost: a b\r\n\r\n", "400"},
        {"GET /echo HTTP/1.1\r\nHost: a\r\nContent-Length: 1\r\nContent-Length: 1\r\n\r\n", "400"},
        {"POST /echo HTTP/1.1\r\nHost: a\r\nContent-Length: -1\r\n\r\n", "400"},
        {"POST /echo HTTP/1.1\r\nHost: a\r\nContent-Length: 1, 1\r\n\r\n", "400"},
        {"POST /echo HTTP/1.1\r\nHost: a\r\nContent-Length: 99999999999999999999999\r\n\r\n", "413"},
        {"POST /echo HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\n", "501"},
        {"POST /echo HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\nContent-Length: 0\r\n\r\n", "400"},
        {"POST /echo HTTP/1.1\r\nHost: a\r\nExpect: 100-continue\r\n\r\n", "417"},
        {"GET /echo HTTP/1.1\r\nHost: a\r\nUpgrade: websocket\r\n\r\n", "400"},
        {"CONNECT /echo HTTP/1.1\r\nHost: a\r\n\r\n", "501"},
        {"GET /echo HTTP/1.0\r\nHost: a\r\n\r\n", "505"},
        {"GET /bad%zz HTTP/1.1\r\nHost: a\r\n\r\n", "400"},
        {"GET /bad#fragment HTTP/1.1\r\nHost: a\r\n\r\n", "400"},
        {"GET http://a/echo HTTP/1.1\r\nHost: a\r\n\r\n", "400"},
        {"GET /echo HTTP/1.1\r\nHost: a\r\nX: one\ntwo\r\n\r\n", "400"}
    };
    websrv_t *server = fixture(echo, NULL, NULL);
    bool ok = server != NULL;
    for (size_t i = 0; ok && i < sizeof(cases) / sizeof(cases[0]); ++i) {
        string_t *reply = send_text(server, cases[i].input);
        string_t *expected = string_sprintf("HTTP/1.1 %s ", cases[i].status);
        ok = reply && contains(reply, string_c_str(expected));
        if (!ok) string_printf("rejection case %zu failed\n", i);
        string_free(expected);
        string_free(reply);
    }
    websrv_free(server);
    TEST_ASSERT_TRUE(ok, "reject unsafe or unsupported framing before dispatch");
}

static void test_websrv_limits_and_timeout(void)
{
    websrv_limits_t limits = {.max_header_bytes = 512, .max_body_bytes = 3, .timeout_ms = 80};
    websrv_t *server = fixture(echo, NULL, &limits);
    string_t *reply = send_text(server, "POST /echo HTTP/1.1\r\nHost: a\r\nContent-Length: 4\r\n\r\n");
    bool ok = reply && contains(reply, "413 Response");
    string_free(reply);
    string_t *large = string_new_with("GET /echo HTTP/1.1\r\nHost: a\r\nX: ");
    for (size_t i = 0; i < 520; ++i) string_append_char(large, 'a');
    string_append_cstr(large, "\r\n\r\n");
    reply = send_text(server, string_c_str(large));
    ok = ok && reply && contains(reply, "431 Response");
    string_free(large);
    string_free(reply);
    const char partial[] = "POST /echo HTTP/1.1\r\nHost: a\r\nContent-Length: 3\r\n\r\nx";
    reply = exchange(server, partial, sizeof(partial) - 1, false, -1);
    ok = ok && reply && !string_byte_length(reply);
    string_free(reply);
    reply = exchange(server, partial, sizeof(partial) - 1, true, -1);
    ok = ok && reply && !string_byte_length(reply);
    string_free(reply);
    reply = send_text(server, "GET /echo HTTP/1.1\r\nHost: a\r\n\r\n");
    ok = ok && reply && contains(reply, "200 Response");
    string_free(reply);
    websrv_free(server);
    TEST_ASSERT_TRUE(ok, "header/body bounds, absolute deadline, truncated input and recovery");
}

static void test_websrv_bodyless_and_failure(void)
{
    static const unsigned statuses[] = {0, 204, 205, 304};
    bool ok = true;
    for (size_t i = 0; ok && i < sizeof(statuses) / sizeof(statuses[0]); ++i) {
        unsigned status = statuses[i];
        websrv_t *server = fixture(special, &status, NULL);
        string_t *reply = send_text(server, "GET /echo HTTP/1.1\r\nHost: a\r\n\r\n");
        string_t *expected = string_sprintf("HTTP/1.1 %u ", status ? status : 500);
        ok = reply && contains(reply, string_c_str(expected)) && !contains(reply, "secret") && !contains(reply, "body");
        if (status == 204 || status == 304) ok = ok && !contains(reply, "Content-Length:");
        string_free(expected);
        string_free(reply);
        websrv_free(server);
    }
    TEST_ASSERT_TRUE(ok, "bodyless statuses and handler failure discard");
}

static bool setter_checks(const websrv_request_t *request, websrv_response_t *response, void *context)
{
    websrv_t *server = context;
    bool ok = websrv_serve_once(server, 0) == -1 && errno == EBUSY;
    string_t *name = string_new_with("Content-Length"), *value = string_new_with("2");
    ok = ok && !websrv_response_header(response, name, value);
    string_free(name);
    name = string_new_with("X-Test");
    ok = ok && websrv_response_header(response, name, value);
    string_free(value);
    value = string_new_with("bad\r\nInjected: yes");
    ok = ok && !websrv_response_header(response, name, value);
    string_free(value);
    value = string_new_with("replaced");
    ok = ok && websrv_response_header(response, name, value) &&
         !websrv_response_status(response, 100) && !websrv_response_body(response, NULL, 1) &&
         websrv_response_body(response, "ok", 2);
    char bytes[1024] = {0};
    ok = ok && !websrv_response_body(response, bytes, sizeof(bytes));
    string_t *invalid = websrv_request_text(request);
    ok = ok && !invalid;
    string_free(invalid);
    string_free(name);
    string_free(value);
    return ok;
}

static void test_websrv_setters(void)
{
    websrv_limits_t limits = {.max_body_bytes = 16};
    websrv_t *server = fixture(setter_checks, NULL, &limits);
    string_t *path = string_new_with("/echo");
    bool ok = server && websrv_route(server, HTTP_POST, path, setter_checks, server);
    string_free(path);
    string_t *request = string_new_with("POST /echo HTTP/1.1\r\nHost: a\r\nContent-Length: 1\r\n\r\n");
    string_append_char(request, (char)255);
    string_t *reply = ok ? exchange(server, string_c_str(request), string_byte_length(request), true, 1) : NULL;
    ok = reply && contains(reply, "200 Response") && contains(reply, "x-test: replaced") &&
         contains(reply, "\r\n\r\nok");
    string_free(request);
    string_free(reply);
    websrv_free(server);
    TEST_ASSERT_TRUE(ok, "setter validation, replacement, budgets, re-entry and strict UTF-8");
}

static void test_websrv_exact_bytes(void)
{
    /* Include decomposed UTF-8 and every possible octet, without text conversion. */
    unsigned char bytes[8192];
    for (size_t i = 0; i < sizeof(bytes); ++i) bytes[i] = (unsigned char)i;
    bytes[0] = 'e'; bytes[1] = 0xcc; bytes[2] = 0x81;
    websrv_t *server = fixture(echo, NULL, NULL);
    TEST_ASSERT_NOT_NULL(server);
    uint16_t port = websrv_port(server);
    pid_t child = fork();
    if (child == 0) {
        int rc = websrv_serve_once(server, 1000);
        websrv_free(server);
        _exit(rc == 1 ? 0 : 1);
    }
    websrv_free(server);
    TEST_ASSERT_TRUE(child > 0, "fork server");
    string_t *url = string_sprintf("http://127.0.0.1:%u/echo", port);
    http_client_t *client = http_client_new();
    http_request_t *request = http_request_new(HTTP_POST, url);
    bool ready = client && request && http_request_set_body(request, bytes, sizeof(bytes), NULL);
    http_response_t *response = ready ? http_client_send(client, request) : NULL;
    bool ok = response && http_response_status(response) == 200 && http_response_body_size(response) == sizeof(bytes);
    const unsigned char *received = http_response_body(response);
    for (size_t i = 0; ok && i < sizeof(bytes); ++i) ok = received[i] == bytes[i];
    http_response_free(response);
    http_request_free(request);
    http_client_free(client);
    string_free(url);
    int status = 0;
    ok = waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0 && ok;
    TEST_ASSERT_TRUE(ok, "all octets and decomposed UTF-8 survive a multi-buffer client/server round trip");
}

static void test_websrv_header_boundary(void)
{
    websrv_t *server = fixture(echo, NULL, NULL);
    string_t *request = string_new_with("POST /echo HTTP/1.1\r\nHost: a\r\nX-Padding: ");
    for (size_t i = 0; i < 6000; ++i) string_append_char(request, 'a');
    string_append_cstr(request, "\r\nContent-Length: 2\r\n\r\nokGET /other HTTP/1.1\r\nHost: a\r\n\r\n");
    string_t *reply = exchange(server, string_c_str(request), string_byte_length(request), true, 1);
    bool ok = reply && contains(reply, "200 Response") && contains(reply, "\r\n\r\nok") &&
              !contains(reply, "404 Response");
    string_free(request);
    string_free(reply);
    websrv_free(server);
    TEST_ASSERT_TRUE(ok, "headers cross receive boundaries and pipelined requests are not dispatched");
}

/* README example: execute the complete documented program last. */
static void example_webserver_hello(void)
{
    TEST_ASSERT_INT_EQ(webserver_example_main(), EXIT_SUCCESS);
}

int tests_main(void)
{
    TEST_SECTION("Web server");
    TEST_RUN_IN_GROUP(test_websrv_lifetime, tests, NULL);
    TEST_RUN_IN_GROUP(test_websrv_routing, tests, NULL);
    TEST_RUN_IN_GROUP(test_websrv_binary_and_methods, tests, NULL);
    TEST_RUN_IN_GROUP(test_websrv_documents, tests, NULL);
    TEST_RUN_IN_GROUP(test_websrv_malformed, tests, NULL);
    TEST_RUN_IN_GROUP(test_websrv_limits_and_timeout, tests, NULL);
    TEST_RUN_IN_GROUP(test_websrv_bodyless_and_failure, tests, NULL);
    TEST_RUN_IN_GROUP(test_websrv_setters, tests, NULL);
    TEST_RUN_IN_GROUP(test_websrv_exact_bytes, tests, NULL);
    TEST_RUN_IN_GROUP(test_websrv_header_boundary, tests, NULL);
    TEST_SECTION("README Output Examples");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_webserver_hello, readme_examples, "webserver,readme,output");
    return TEST_EXIT_CODE();
}
