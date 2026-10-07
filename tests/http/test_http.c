#include <arpa/inet.h>
#define stack_t posix_signal_stack_t
#include <signal.h>
#undef stack_t
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include "http.h"
#include "array.h"
#include "test_harness.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

/* Compile the actual README programs with distinct entry points. */
#define main http_get_example_main
#include "../../scratch/http_get.c"
#undef main
#define main http_post_json_example_main
#include "../../scratch/http_post_json.c"
#undef main
#define main http_post_xml_example_main
#include "../../scratch/http_post_xml.c"
#undef main

static unsigned plain_port, tls_port;
static pid_t plain_pid = -1, tls_pid = -1;

static pid_t start_fixture(bool tls, unsigned *port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in address = {.sin_family = AF_INET, .sin_addr.s_addr = htonl(INADDR_LOOPBACK)};
    socklen_t length = sizeof(address);
    if (fd < 0)
        return -1;
    if (bind(fd, (struct sockaddr *)&address, length) || listen(fd, 8) ||
        getsockname(fd, (struct sockaddr *)&address, &length)) {
        close(fd);
        return -1;
    }
    *port = ntohs(address.sin_port);
    string_t *descriptor = string_sprintf("%d", fd);
    pid_t pid = descriptor ? fork() : -1;
    if (pid == 0) {
        if (tls)
            execlp("python3", "python3", "tests/http/http_fixture.py", string_c_str(descriptor),
                   "tests/http/test-cert.pem", "tests/http/test-key.pem", (char *)NULL);
        else
            execlp("python3", "python3", "tests/http/http_fixture.py", string_c_str(descriptor), (char *)NULL);
        _exit(127);
    }
    string_free(descriptor);
    close(fd);
    return pid;
}

static void stop_fixture(pid_t pid)
{
    if (pid > 0) {
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
    }
}

static http_request_t *request_for(webmethod_t method, const char *path)
{
    string_t *url = string_sprintf("http://127.0.0.1:%u%s", plain_port, path);
    http_request_t *request = http_request_new(method, url);
    string_free(url);
    return request;
}

static bool header_equals(const http_response_t *response, const char *name, const char *expected)
{
    string_t *key = string_new_with(name);
    const string_t *value = http_response_header_at(response, key, 0);
    bool ok = value && string_view_equals_literal(string_view_all(value), expected);
    string_free(key);
    return ok;
}

static void test_http_validation_and_encoding(void)
{
    static const char *const bad[] = {
        "file:///etc/passwd", "ftp://localhost/", "http://user:pass@localhost/", "http://localhost/#fragment",
        "http://localhost/\r\nX: y", "http://localhost/a b", "http://localhost/\\escape", ""
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
        string_t *url = string_new_with(bad[i]);
        http_request_t *request = http_request_new(HTTP_GET, url);
        bool rejected = request == NULL;
        http_request_free(request);
        string_free(url);
        TEST_ASSERT_TRUE(rejected, bad[i]);
    }
    http_request_t *request = request_for(HTTP_GET, "/json");
    string_t *name = string_new_with("X-Test"), *value = string_new_with("bad\r\nInjected: yes");
    TEST_ASSERT_NOT_NULL(request);
    TEST_ASSERT_TRUE(!http_request_set_header(request, name, value), "header injection rejected");
    string_free(value);
    value = string_new_with("ok");
    TEST_ASSERT_TRUE(http_request_set_header(request, name, value), "valid header");
    string_free(name);
    name = string_new_with("Content-Length");
    TEST_ASSERT_TRUE(!http_request_set_header(request, name, value), "framing reserved");
    TEST_ASSERT_TRUE(!http_request_set_body(request, "a", 1, NULL), "GET body rejected");
    string_free(name); string_free(value);
    value = string_new_with("a b&é");
    string_t *encoded = http_url_encode(value);
    TEST_ASSERT_STR_EQ(string_c_str(encoded), "a%20b%26%C3%A9");
    string_free(value); string_free(encoded); http_request_free(request);
    TEST_ASSERT_TRUE(http_request_new(HTTP_GET, NULL) == NULL, "NULL URL");
    TEST_ASSERT_TRUE(http_client_send(NULL, NULL) == NULL, "NULL transfer");
    TEST_ASSERT_INT_EQ(http_response_status(NULL), 0);
    TEST_ASSERT_TRUE(!http_response_ok(NULL) && !http_response_body(NULL), "NULL accessors");
    TEST_ASSERT_INT_EQ(http_response_body_size(NULL), 0);
    TEST_ASSERT_INT_EQ(http_response_received_size(NULL), 0);
    TEST_ASSERT_TRUE(!http_response_text(NULL) && !http_response_json(NULL) && !http_response_xml(NULL), "NULL content");
    TEST_ASSERT_TRUE(!http_response_headers(NULL), "NULL headers");
    TEST_ASSERT_INT_EQ(http_response_header_count(NULL, NULL), 0);
    TEST_ASSERT_TRUE(!http_response_header_at(NULL, NULL, 0), "NULL header");
    TEST_ASSERT_INT_EQ(http_client_error(NULL), HTTP_ERROR_ARGUMENT);
    TEST_ASSERT_TRUE(!http_client_error_text(NULL), "NULL error");
    http_response_free(NULL); http_request_free(NULL); http_client_free(NULL);
}

static void test_http_headers_status_and_reuse(void)
{
    http_client_t *client = http_client_new();
    static const char *const paths[] = {"/headers", "/redirect", "/error", "/json", "/chunks"};
    static const long statuses[] = {200, 302, 404, 200, 200};
    for (size_t i = 0; i < 5; ++i) {
        http_request_t *request = request_for(HTTP_GET, paths[i]);
        http_response_t *response = http_client_send(client, request);
        TEST_ASSERT_NOT_NULL(response);
        TEST_ASSERT_INT_EQ(http_response_status(response), statuses[i]);
        TEST_ASSERT_INT_EQ(http_response_ok(response), statuses[i] == 200);
        TEST_ASSERT_INT_EQ(http_client_error(client), HTTP_ERROR_NONE);
        if (!i) {
            string_t *key = string_new_with("SET-COOKIE");
            TEST_ASSERT_INT_EQ(http_response_header_count(response, key), 2);
            TEST_ASSERT_STR_EQ(string_c_str(http_response_header_at(response, key, 1)), "two=2");
            TEST_ASSERT_TRUE(!http_response_header_at(response, key, 2), "header bounds");
            TEST_ASSERT_NOT_NULL(http_response_headers(response));
            string_free(key);
        }
        if (i == 4) {
            TEST_ASSERT_TRUE(header_equals(response, "X-End", "yes"), "chunk trailer");
            TEST_ASSERT_TRUE(!header_equals(response, "X-Early", "discarded"), "interim headers discarded");
            TEST_ASSERT_INT_EQ(http_response_body_size(response), 6);
            TEST_ASSERT_TRUE(!memcmp(http_response_body(response), "onetwo", 6), "dechunked body");
        }
        http_response_free(response); http_request_free(request);
    }
    http_client_free(client);
}

static void test_http_methods_and_binary(void)
{
    http_client_t *client = http_client_new();
    static const unsigned char body[] = {0, 255, 254, 65};
    static const char *const methods[] = {"GET", "POST", "PUT", "PATCH", "DELETE", "HEAD", "OPTIONS"};
    for (webmethod_t method = HTTP_GET; method <= HTTP_OPTIONS; ++method) {
        http_request_t *request = request_for(method, "/echo");
        string_t *name = string_new_with("X-Custom"), *old = string_new_with("old");
        string_t *value = string_new_with("new"), *token = string_new_with("test-token");
        TEST_ASSERT_TRUE(http_request_set_header(request, name, old), "initial header");
        TEST_ASSERT_TRUE(http_request_set_header(request, name, value), "replacement header");
        TEST_ASSERT_TRUE(http_request_set_bearer(request, token), "bearer");
        bool body_allowed = method != HTTP_GET && method != HTTP_HEAD;
        if (body_allowed)
            TEST_ASSERT_TRUE(http_request_set_body(request, body, sizeof(body), NULL), "binary body");
        http_response_t *response = http_client_send(client, request);
        TEST_ASSERT_NOT_NULL(response);
        TEST_ASSERT_TRUE(header_equals(response, "X-Method", methods[method]), "method");
        TEST_ASSERT_TRUE(header_equals(response, "X-Custom", "new"), "replaced header");
        TEST_ASSERT_TRUE(header_equals(response, "X-Auth", "Bearer test-token"), "auth");
        TEST_ASSERT_INT_EQ(http_response_body_size(response), body_allowed ? sizeof(body) : 0);
        if (body_allowed) {
            TEST_ASSERT_TRUE(!memcmp(http_response_body(response), body, sizeof(body)), "binary round trip");
            TEST_ASSERT_TRUE(!http_response_text(response), "invalid UTF-8 rejected");
        }
        http_response_free(response); http_request_free(request);
        string_free(name); string_free(old); string_free(value); string_free(token);
    }
    http_client_free(client);
}

static bool collect_body(const void *data, size_t size, void *context)
{
    return array_append_carray(context, data, size);
}

static bool stop_body(const void *data, size_t size, void *context)
{
    (void)data; (void)size; (void)context;
    return false;
}

static bool cancelled(void *context)
{
    (void)context;
    return true;
}

static void test_http_stream_and_cancellation(void)
{
    http_client_t *client = http_client_new();
    http_request_t *request = request_for(HTTP_GET, "/binary");
    array_t *bytes = array_create(1, NULL, NULL);
    http_response_t *response = http_client_stream(client, request, collect_body, bytes);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_INT_EQ(array_size(bytes), 4);
    TEST_ASSERT_INT_EQ(http_response_received_size(response), 4);
    TEST_ASSERT_INT_EQ(http_response_body_size(response), 0);
    TEST_ASSERT_TRUE(!http_response_text(response), "stream is not retained");
    http_response_free(response);
    TEST_ASSERT_TRUE(!http_client_stream(client, request, stop_body, NULL), "sink stops");
    TEST_ASSERT_INT_EQ(http_client_error(client), HTTP_ERROR_CANCELLED);
    http_client_set_cancel(client, cancelled, NULL);
    TEST_ASSERT_TRUE(!http_client_send(client, request), "cancel before connection");
    TEST_ASSERT_INT_EQ(http_client_error(client), HTTP_ERROR_CANCELLED);
    http_client_set_cancel(client, NULL, NULL);
    TEST_ASSERT_TRUE(!http_client_stream(client, request, NULL, NULL), "missing sink");
    TEST_ASSERT_INT_EQ(http_client_error(client), HTTP_ERROR_ARGUMENT);
    http_request_free(request); http_client_free(client); array_destroy(bytes);
}

static void test_http_limits_timeout_and_truncation(void)
{
    http_client_t *client = http_client_new();
    static const char *const paths[] = {"/large", "/gzip", "/bigheader", "/slow", "/truncated"};
    for (size_t i = 0; i < 5; ++i) {
        http_limits_t limits = {.max_body_bytes = 128, .max_header_bytes = 1024,
                                .total_timeout_ms = i == 3 ? 50 : 3000};
        TEST_ASSERT_TRUE(http_client_set_limits(client, &limits), "limits");
        http_request_t *request = request_for(HTTP_GET, paths[i]);
        http_response_t *response = http_client_send(client, request);
        TEST_ASSERT_TRUE(!response, "failed transfer");
        TEST_ASSERT_INT_EQ(http_client_error(client), i < 3 ? HTTP_ERROR_LIMIT :
                                                      i == 3 ? HTTP_ERROR_TIMEOUT : HTTP_ERROR_TRANSPORT);
        TEST_ASSERT_NOT_NULL(http_client_error_text(client));
        http_request_free(request);
    }
    http_limits_t limits = {.max_upload_bytes = 2};
    TEST_ASSERT_TRUE(http_client_set_limits(client, &limits), "upload limit");
    http_request_t *request = request_for(HTTP_POST, "/echo");
    TEST_ASSERT_TRUE(http_request_set_body(request, "long", 4, NULL), "copy body");
    TEST_ASSERT_TRUE(!http_client_send(client, request), "oversized upload");
    TEST_ASSERT_INT_EQ(http_client_error(client), HTTP_ERROR_LIMIT);
    http_request_free(request);
    limits = (http_limits_t){.max_header_bytes = 5};
    TEST_ASSERT_TRUE(http_client_set_limits(client, &limits), "header limit");
    request = request_for(HTTP_GET, "/json");
    string_t *key = string_new_with("X-Long"), *value = string_new_with("value");
    TEST_ASSERT_TRUE(http_request_set_header(request, key, value), "custom header");
    TEST_ASSERT_TRUE(!http_client_send(client, request), "outgoing header budget");
    TEST_ASSERT_INT_EQ(http_client_error(client), HTTP_ERROR_LIMIT);
    string_free(key); string_free(value); http_request_free(request); http_client_free(client);
}

static void test_http_json_and_xml(void)
{
    http_client_t *client = http_client_new();
    string_t *text = string_new_with("{\"answer\":42}");
    json_t *json = json_from_text(text);
    http_request_t *request = request_for(HTTP_POST, "/echo");
    TEST_ASSERT_TRUE(http_request_set_json(request, json), "JSON request");
    http_response_t *response = http_client_send(client, request);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_TRUE(header_equals(response, "X-Type", "application/json"), "JSON media type");
    json_t *round = http_response_json(response);
    TEST_ASSERT_NOT_NULL(round);
    json_free(round); json_free(json); string_free(text);
    http_response_free(response); http_request_free(request);
    text = string_new_with("<answer>42</answer>");
    xml_t *xml = xml_from_text(text);
    request = request_for(HTTP_PUT, "/echo");
    TEST_ASSERT_TRUE(http_request_set_xml(request, xml), "XML request");
    response = http_client_send(client, request);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_TRUE(header_equals(response, "X-Type", "application/xml"), "XML media type");
    xml_t *copy = http_response_xml(response);
    string_t *out = xml_to_string(copy);
    TEST_ASSERT_STR_EQ(string_c_str(out), "<answer>42</answer>");
    string_free(out); xml_free(copy); xml_free(xml); string_free(text);
    http_response_free(response); http_request_free(request); http_client_free(client);
}

static void test_http_file_upload_download(void)
{
    string_t *path = string_new_with(test_case_temp_path("transfer.bin"));
    file_t *file = file_new(path);
    const unsigned char data[] = {0, 255, 254, 65};
    TEST_ASSERT_TRUE(file_write_all_bytes(file, data, sizeof(data)), "prepare upload");
    http_client_t *client = http_client_new();
    http_request_t *request = request_for(HTTP_POST, "/echo");
    TEST_ASSERT_TRUE(http_request_set_body_file(request, path, NULL), "file upload source");
    http_response_t *response = http_client_send(client, request);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_INT_EQ(http_response_body_size(response), sizeof(data));
    TEST_ASSERT_TRUE(!memcmp(http_response_body(response), data, sizeof(data)), "uploaded bytes");
    http_response_free(response); http_request_free(request);
    request = request_for(HTTP_GET, "/binary");
    TEST_ASSERT_TRUE(file_create(file), "open truncated download destination");
    response = http_client_download(client, request, file);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_TRUE(file_is_open(file), "borrowed destination remains open");
    TEST_ASSERT_TRUE(file_close(file), "close destination");
    array_t *bytes = file_read_all_bytes(file);
    TEST_ASSERT_INT_EQ(array_size(bytes), sizeof(data));
    TEST_ASSERT_TRUE(!memcmp(array_get(bytes, 0), data, sizeof(data)), "downloaded bytes");
    array_destroy(bytes); http_response_free(response); http_request_free(request);
    request = request_for(HTTP_GET, "/binary");
    TEST_ASSERT_TRUE(!http_client_download(client, request, file), "closed destination rejected");
    TEST_ASSERT_INT_EQ(http_client_error(client), HTTP_ERROR_ARGUMENT);
    TEST_ASSERT_TRUE(file_open_read(file), "open read-only destination");
    TEST_ASSERT_TRUE(!http_client_download(client, request, file), "read-only destination fails");
    TEST_ASSERT_INT_EQ(http_client_error(client), HTTP_ERROR_IO);
    TEST_ASSERT_TRUE(file_close(file), "close read-only destination");
    http_request_free(request);
    TEST_ASSERT_TRUE(file_delete(file), "remove temporary file");
    request = request_for(HTTP_PUT, "/echo");
    TEST_ASSERT_TRUE(http_request_set_body_file(request, path, NULL), "deferred file opening");
    TEST_ASSERT_TRUE(!http_client_send(client, request), "missing upload source");
    TEST_ASSERT_INT_EQ(http_client_error(client), HTTP_ERROR_IO);
    http_request_free(request); http_client_free(client); file_free(file); string_free(path);
}

static void test_http_verified_tls_and_trust_reset(void)
{
    http_client_t *client = http_client_new();
    string_t *url = string_sprintf("https://localhost:%u/json", tls_port);
    string_t *ca = string_new_with("tests/http/test-cert.pem");
    http_request_t *request = http_request_new(HTTP_GET, url);
    TEST_ASSERT_TRUE(!http_client_send(client, request), "untrusted certificate rejected");
    TEST_ASSERT_INT_EQ(http_client_error(client), HTTP_ERROR_TRANSPORT);
    TEST_ASSERT_TRUE(http_client_set_ca_file(client, ca), "explicit trust");
    http_response_t *response = http_client_send(client, request);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_INT_EQ(http_response_status(response), 200);
    http_response_free(response);
    string_t *missing = string_new_with(test_case_temp_path("missing-ca.pem"));
    TEST_ASSERT_TRUE(!http_client_set_ca_file(client, missing), "failed trust update");
    response = http_client_send(client, request);
    TEST_ASSERT_NOT_NULL(response);
    http_response_free(response); string_free(missing);
    string_t *wrong = string_sprintf("https://127.0.0.1:%u/json", tls_port);
    http_request_t *mismatch = http_request_new(HTTP_GET, wrong);
    TEST_ASSERT_TRUE(!http_client_send(client, mismatch), "hostname mismatch rejected");
    TEST_ASSERT_TRUE(http_client_set_ca_file(client, NULL), "restore system trust");
    TEST_ASSERT_TRUE(!http_client_send(client, request), "cached trusted connection discarded");
    http_request_free(mismatch); string_free(wrong); string_free(ca); string_free(url);
    http_request_free(request); http_client_free(client);
}

static void test_http_malformed_response_headers(void)
{
    http_client_t *client = http_client_new();
    static const char *const paths[] = {"/badheader", "/switch"};
    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        http_request_t *request = request_for(HTTP_GET, paths[i]);
        http_response_t *response = http_client_send(client, request);
        TEST_ASSERT_TRUE(!response, paths[i]);
        TEST_ASSERT_INT_EQ(http_client_error(client), HTTP_ERROR_PROTOCOL);
        http_request_free(request);
    }
    http_request_t *request = request_for(HTTP_GET, "/folded");
    http_response_t *response = http_client_send(client, request);
    /* libcurl versions may reject folding or normalise it before our callback. */
    TEST_ASSERT_TRUE(!response || header_equals(response, "X-Fold", "one two"), "safe unfolded header");
    http_response_free(response); http_request_free(request);
    http_client_free(client);
}

static bool xml_fragment(const void *data, size_t size, void *context)
{
    return xml_reader_feed(context, data, size);
}

static void test_http_xml_stream_and_body_replacement(void)
{
    http_client_t *client = http_client_new();
    http_request_t *request = request_for(HTTP_GET, "/xml");
    xml_reader_t *reader = xml_reader_new(NULL, NULL, NULL);
    http_response_t *response = http_client_stream(client, request, xml_fragment, reader);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_TRUE(xml_reader_finish(reader), "finish streamed document");
    xml_t *document = xml_reader_take_document(reader);
    string_t *text = xml_to_string(document);
    TEST_ASSERT_STR_EQ(string_c_str(text), "<answer>42</answer>");
    string_free(text); xml_free(document); xml_reader_free(reader);
    http_response_free(response); http_request_free(request);
    request = request_for(HTTP_POST, "/echo");
    text = string_new_with("retained");
    string_t *type = string_new_with("text/plain");
    string_t *bad_type = string_new_with("text/plain\r\nInjected: yes");
    TEST_ASSERT_TRUE(http_request_set_text(request, text, type), "text body");
    TEST_ASSERT_TRUE(!http_request_set_body(request, "lost", 4, bad_type), "invalid type rejected");
    TEST_ASSERT_TRUE(!http_request_set_body(request, NULL, 1, NULL), "invalid binary body");
    response = http_client_send(client, request);
    string_t *body = http_response_text(response);
    TEST_ASSERT_STR_EQ(string_c_str(body), "retained");
    TEST_ASSERT_TRUE(header_equals(response, "X-Type", "text/plain"), "type retained");
    string_free(body); http_response_free(response);
    TEST_ASSERT_TRUE(http_request_set_body(request, NULL, 0, NULL), "empty replacement");
    response = http_client_send(client, request);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_INT_EQ(http_response_body_size(response), 0);
    body = http_response_text(response);
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_INT_EQ(string_byte_length(body), 0);
    string_free(body); http_response_free(response); http_request_free(request);
    string_free(text); string_free(type); string_free(bad_type); http_client_free(client);
}

/* README examples execute the documented main functions against local fixtures. */
static void example_http_json(void)
{
    string_t *url = string_sprintf("http://127.0.0.1:%u/get?message=MARS", plain_port);
    char *argv[] = {"http_get", (char *)string_c_str(url), NULL};
    int status = http_get_example_main(2, argv);
    string_free(url);
    TEST_ASSERT_INT_EQ(status, EXIT_SUCCESS);
}

static void example_http_post_json(void)
{
    string_t *url = string_sprintf("http://127.0.0.1:%u/post", plain_port);
    char *argv[] = {"http_post_json", (char *)string_c_str(url), NULL};
    int status = http_post_json_example_main(2, argv);
    string_free(url);
    TEST_ASSERT_INT_EQ(status, EXIT_SUCCESS);
}

static void example_http_xml(void)
{
    string_t *url = string_sprintf("http://127.0.0.1:%u/post", plain_port);
    char *argv[] = {"http_post_xml", (char *)string_c_str(url), NULL};
    int status = http_post_xml_example_main(2, argv);
    string_free(url);
    TEST_ASSERT_INT_EQ(status, EXIT_SUCCESS);
}

int tests_main(void)
{
    plain_pid = start_fixture(false, &plain_port);
    tls_pid = start_fixture(true, &tls_port);
    if (plain_pid <= 0 || tls_pid <= 0) {
        stop_fixture(plain_pid); stop_fixture(tls_pid);
        return 1;
    }
    TEST_SECTION("HTTP");
    TEST_RUN_IN_GROUP(test_http_validation_and_encoding, tests, NULL);
    TEST_RUN_IN_GROUP(test_http_headers_status_and_reuse, tests, NULL);
    TEST_RUN_IN_GROUP(test_http_methods_and_binary, tests, NULL);
    TEST_RUN_IN_GROUP(test_http_stream_and_cancellation, tests, NULL);
    TEST_RUN_IN_GROUP(test_http_limits_timeout_and_truncation, tests, NULL);
    TEST_RUN_IN_GROUP(test_http_json_and_xml, tests, NULL);
    TEST_RUN_IN_GROUP(test_http_file_upload_download, tests, NULL);
    TEST_RUN_IN_GROUP(test_http_verified_tls_and_trust_reset, tests, NULL);
    TEST_RUN_IN_GROUP(test_http_malformed_response_headers, tests, NULL);
    TEST_RUN_IN_GROUP(test_http_xml_stream_and_body_replacement, tests, NULL);
    TEST_SECTION("README Output Examples");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_http_json, readme_examples, "http,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_http_post_json, readme_examples, "http,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_http_xml, readme_examples, "http,readme,output");
    stop_fixture(plain_pid); stop_fixture(tls_pid);
    return TEST_EXIT_CODE();
}
