#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include "http.h"
#include "test_harness.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

/* Run the exact README programs, overriding only their service URL. */
#define main http_get_example_main
#include "../../scratch/http_get.c"
#undef main
#define main http_post_json_example_main
#include "../../scratch/http_post_json.c"
#undef main
#define main http_post_xml_example_main
#include "../../scratch/http_post_xml.c"
#undef main

typedef struct {
    pid_t pid;
    uint16_t port;
} live_service_t;

static bool live_get(const websrv_request_t *request, websrv_response_t *response, void *context)
{
    (void)context;
    if (!string_view_equals_literal(string_view_all(websrv_request_target(request)), "/get?message=MARS"))
        return websrv_response_status(response, 400);
    string_t *text = string_new_with("{\"args\":{\"message\":\"MARS\"}}");
    json_t *json = text ? json_from_text(text) : NULL;
    bool ok = json && websrv_response_json(response, json);
    json_free(json);
    string_free(text);
    return ok;
}

static bool live_post(const websrv_request_t *request, websrv_response_t *response, void *context)
{
    (void)context;
    string_t *name = string_new_with("Content-Type");
    const string_t *type = websrv_request_header(request, name);
    bool is_json = type && string_view_equals_literal(string_view_all(type), "application/json");
    bool is_xml = type && string_view_equals_literal(string_view_all(type), "application/xml");
    string_free(name);
    if (!is_json && !is_xml) return websrv_response_status(response, 415);
    json_t *value = NULL;
    if (is_json) {
        value = websrv_request_json(request);
    } else {
        xml_t *xml = websrv_request_xml(request);
        string_t *text = xml ? xml_to_string(xml) : NULL;
        value = text ? json_new_string(text) : NULL;
        string_free(text);
        xml_free(xml);
    }
    if (!value) return websrv_response_status(response, 400);
    json_t *envelope = json_new_object();
    string_t *key = string_new_with(is_json ? "json" : "data");
    bool ok = envelope && key && json_object_set(envelope, key, value) &&
              websrv_response_json(response, envelope);
    string_free(key);
    json_free(envelope);
    json_free(value);
    return ok;
}

static bool live_redirect(const websrv_request_t *request, websrv_response_t *response, void *context)
{
    (void)request;
    (void)context;
    string_t *name = string_new_with("Location"), *value = string_new_with("/get?message=MARS");
    bool ok = name && value && websrv_response_status(response, 302) &&
              websrv_response_header(response, name, value);
    string_free(name);
    string_free(value);
    return ok;
}

static bool live_binary(const websrv_request_t *request, websrv_response_t *response, void *context)
{
    (void)request;
    (void)context;
    unsigned char bytes[256];
    for (size_t i = 0; i < sizeof(bytes); ++i) bytes[i] = (unsigned char)i;
    return websrv_response_body(response, bytes, sizeof(bytes));
}

static live_service_t start_service(unsigned requests)
{
    live_service_t service = {.pid = -1};
    websrv_limits_t limits = {.timeout_ms = 3000};
    websrv_t *server = websrv_new(NULL, 0, &limits);
    static const struct { const char *path; webmethod_t method; websrv_handler_fn handler; } routes[] = {
        {"/get", HTTP_GET, live_get},
        {"/post", HTTP_POST, live_post},
        {"/redirect", HTTP_GET, live_redirect},
        {"/bytes", HTTP_GET, live_binary}
    };
    bool ok = server != NULL;
    for (size_t i = 0; ok && i < sizeof(routes) / sizeof(routes[0]); ++i) {
        string_t *path = string_new_with(routes[i].path);
        ok = path && websrv_route(server, routes[i].method, path, routes[i].handler, NULL);
        string_free(path);
    }
    if (ok) {
        service.port = websrv_port(server);
        service.pid = fork();
        if (service.pid == 0) {
            for (unsigned i = 0; ok && i < requests; ++i)
                ok = websrv_serve_once(server, 3000) == 1;
            websrv_free(server);
            _exit(ok ? EXIT_SUCCESS : EXIT_FAILURE);
        }
    }
    websrv_free(server);
    return service;
}

static bool stop_service(live_service_t service)
{
    int status = 0;
    return service.pid > 0 && waitpid(service.pid, &status, 0) == service.pid &&
           WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS;
}

static string_t *service_url(live_service_t service, const char *path)
{
    return string_sprintf("http://127.0.0.1:%u%s", service.port, path);
}

static void test_http_live_status_and_redirect(void)
{
    live_service_t service = start_service(2);
    http_client_t *client = http_client_new();
    static const char *const paths[] = {"/missing", "/redirect"};
    static const long expected[] = {404, 302};
    bool ok = service.pid > 0 && client;
    for (size_t i = 0; ok && i < 2; ++i) {
        string_t *url = service_url(service, paths[i]);
        http_request_t *request = http_request_new(HTTP_GET, url);
        http_response_t *response = request ? http_client_send(client, request) : NULL;
        ok = response && http_response_status(response) == expected[i];
        if (ok && i == 1) {
            string_t *name = string_new_with("Location");
            const string_t *location = http_response_header_at(response, name, 0);
            ok = location && string_view_equals_literal(string_view_all(location), "/get?message=MARS");
            string_free(name);
        }
        http_response_free(response);
        http_request_free(request);
        string_free(url);
    }
    http_client_free(client);
    bool stopped = stop_service(service);
    TEST_ASSERT_TRUE(ok && stopped, "local service returns 404 and unfollowed 302 with Location");
}

static void test_http_live_binary(void)
{
    live_service_t service = start_service(1);
    string_t *url = service_url(service, "/bytes");
    http_client_t *client = http_client_new();
    http_request_t *request = http_request_new(HTTP_GET, url);
    http_response_t *response = service.pid > 0 && client && request ? http_client_send(client, request) : NULL;
    bool ok = response && http_response_status(response) == 200 && http_response_body_size(response) == 256;
    const unsigned char *bytes = http_response_body(response);
    for (size_t i = 0; ok && i < 256; ++i) ok = bytes[i] == (unsigned char)i;
    http_response_free(response);
    http_request_free(request);
    http_client_free(client);
    string_free(url);
    bool stopped = stop_service(service);
    TEST_ASSERT_TRUE(ok && stopped, "all octets traverse the native server and HTTP client");
}

static void test_http_live_rejects_bad_documents(void)
{
    live_service_t service = start_service(2);
    http_client_t *client = http_client_new();
    string_t *url = service_url(service, "/post"), *type = string_new_with("application/json");
    http_request_t *request = http_request_new(HTTP_POST, url);
    bool ok = service.pid > 0 && client && request && type;
    for (unsigned i = 0; ok && i < 2; ++i) {
        const char *body = i ? "not-json" : "{}";
        string_t *text = string_new_with(body);
        ok = text && http_request_set_text(request, text, i ? type : NULL);
        http_response_t *response = ok ? http_client_send(client, request) : NULL;
        ok = response && http_response_status(response) == (i ? 400 : 415);
        http_response_free(response);
        string_free(text);
    }
    http_request_free(request);
    http_client_free(client);
    string_free(type);
    string_free(url);
    bool stopped = stop_service(service);
    TEST_ASSERT_TRUE(ok && stopped, "service validates content types and JSON payloads");
}

static int run_example(int (*program)(int, char **), const char *path)
{
    live_service_t service = start_service(1);
    string_t *url = service_url(service, path);
    char *argv[] = {"http_example", url ? (char *)string_c_str(url) : NULL, NULL};
    int rc = service.pid > 0 && url ? program(2, argv) : EXIT_FAILURE;
    string_free(url);
    return stop_service(service) && rc == EXIT_SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* README example: complete documented GET program against our native service. */
static void example_http_live_get(void)
{
    TEST_ASSERT_INT_EQ(run_example(http_get_example_main, "/get?message=MARS"), EXIT_SUCCESS);
}

/* README example: complete documented JSON POST program against our native service. */
static void example_http_live_post(void)
{
    TEST_ASSERT_INT_EQ(run_example(http_post_json_example_main, "/post"), EXIT_SUCCESS);
}

/* README example: complete documented XML POST program against our native service. */
static void example_http_live_xml(void)
{
    TEST_ASSERT_INT_EQ(run_example(http_post_xml_example_main, "/post"), EXIT_SUCCESS);
}

int tests_main(void)
{
    TEST_SECTION("HTTP client and native web server integration");
    TEST_RUN_IN_GROUP(test_http_live_status_and_redirect, tests, NULL);
    TEST_RUN_IN_GROUP(test_http_live_binary, tests, NULL);
    TEST_RUN_IN_GROUP(test_http_live_rejects_bad_documents, tests, NULL);
    TEST_SECTION("README Output Examples");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_http_live_get, readme_examples, "http,webserver,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_http_live_post, readme_examples, "http,webserver,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_http_live_xml, readme_examples, "http,webserver,readme,output");
    return TEST_EXIT_CODE();
}
