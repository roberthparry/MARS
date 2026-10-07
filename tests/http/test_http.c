/**
 * @file test_http.c
 * @brief HTTP and web-service protocol regression suite.
 *
 * Exercises bounded transport, TLS, documents, forms, authentication, sessions, SOAP, events, WebSocket and unary
 * gRPC against local fixtures. Complete README programs run after the ordinary protocol tests.
 *
 * Used by the project test harness for regression verification. Select cases through tests/test_config.json and
 * run suites sequentially; this source is not part of the installed library.
 */

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
#include "protobuf.h"
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
#define main http_services_example_main
#include "../../scratch/http_services.c"
#undef main
#define main http_grpc_example_main
#include "../../scratch/http_grpc.c"
#undef main
#define main http_websocket_example_main
#include "../../scratch/http_websocket.c"
#undef main

static unsigned plain_port, tls_port;
static pid_t plain_pid = -1, tls_pid = -1;

static pid_t start_fixture(bool tls, bool identity, unsigned *port)
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
        if (identity)
            execlp("python3", "python3", "tests/http/http_fixture.py", string_c_str(descriptor),
                   "tests/http/test-cert.pem", "tests/http/test-key.pem", "identity", (char *)NULL);
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

static void test_http_forms_auth_and_multipart(void)
{
    http_client_t *client = http_client_new();
    http_form_t *form = http_form_new();
    string_t *name = string_new_with("message"), *value = string_new_with("hello + & café");
    TEST_ASSERT_TRUE(http_form_add_text(form, name, value), "copy text field");
    TEST_ASSERT_TRUE(http_form_add_text(form, name, value), "retain repeated field");
    string_t *encoded = http_form_encode(form);
    TEST_ASSERT_STR_EQ(string_c_str(encoded), "message=hello+%2B+%26+caf%C3%A9&message=hello+%2B+%26+caf%C3%A9");
    http_request_t *request = request_for(HTTP_POST, "/inspect");
    TEST_ASSERT_TRUE(http_request_set_form(request, form), "URL-encoded body");
    string_t *user = string_new_with("Aladdin"), *password = string_new_with("open sesame");
    TEST_ASSERT_TRUE(http_request_set_basic(request, user, password), "Basic credentials");
    http_response_t *response = http_client_send(client, request);
    TEST_ASSERT_TRUE(header_equals(response, "X-Authorization", "Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ=="), "Basic encoding");
    TEST_ASSERT_TRUE(header_equals(response, "X-Type", "application/x-www-form-urlencoded"), "form media type");
    string_t *text = http_response_text(response);
    TEST_ASSERT_STR_EQ(string_c_str(text), string_c_str(encoded));
    string_free(text); http_response_free(response);
    TEST_ASSERT_TRUE(http_request_set_multipart(request, form, 4096), "multipart text fields");
    response = http_client_send(client, request);
    text = http_response_text(response);
    TEST_ASSERT_TRUE(text && string_find(text, "name=\"message\"") >= 0 &&
                     string_find(text, "hello + & café") >= 0, "multipart content");
    string_free(text); http_response_free(response);
    TEST_ASSERT_TRUE(!http_request_set_multipart(request, form, 10), "bounded multipart failure");
    string_t *path = string_new_with("tests/http/test-cert.pem");
    string_t *filename = string_new_with("certificate.pem"), *type = string_new_with("application/x-pem-file");
    TEST_ASSERT_TRUE(http_form_add_file(form, name, filename, type, path), "file part");
    TEST_ASSERT_TRUE(!http_form_encode(form), "file parts cannot be URL encoded");
    TEST_ASSERT_TRUE(http_request_set_multipart(request, form, 16384), "file module upload snapshot");
    response = http_client_send(client, request);
    text = http_response_text(response);
    TEST_ASSERT_TRUE(text && string_find(text, "BEGIN CERTIFICATE") >= 0 &&
                     string_find(text, "filename=\"certificate.pem\"") >= 0, "file contents and metadata");
    string_free(text); http_response_free(response);
    string_t *bad = string_new_with("bad:user");
    TEST_ASSERT_TRUE(!http_request_set_basic(request, bad, password), "colon in username rejected");
    string_free(bad);
    string_t *message = string_new_with("The quick brown fox jumps over the lazy dog");
    string_t *signature = http_hmac_sha256(message, "key", 3);
    TEST_ASSERT_STR_EQ(string_c_str(signature), "97yD9DBThCSxMpjmqm+xQ+9NWaFJRhdZl0edvC0aPNg=");
    TEST_ASSERT_TRUE(!http_hmac_sha256(message, NULL, 0), "missing signing key");
    string_free(signature); string_free(message);
    string_free(path); string_free(filename); string_free(type);
    string_free(user); string_free(password); string_free(name); string_free(value); string_free(encoded);
    http_form_free(form); http_request_free(request); http_client_free(client);
    form = http_form_new();
    name = string_new_with("symbols");
    value = string_new_with("*~ ");
    TEST_ASSERT_TRUE(http_form_add_text(form, name, value), "HTML form safe-character set");
    encoded = http_form_encode(form);
    TEST_ASSERT_STR_EQ(string_c_str(encoded), "symbols=*%7E+");
    string_free(encoded); string_free(name); string_free(value); http_form_free(form);
}

static void test_http_session_redirects_oauth(void)
{
    http_client_t *client = http_client_new();
    TEST_ASSERT_TRUE(http_client_set_cookies(client, true), "enable cookie session");
    http_request_t *request = request_for(HTTP_GET, "/headers");
    http_response_t *response = http_client_send(client, request);
    TEST_ASSERT_NOT_NULL(response);
    http_response_free(response); http_request_free(request);
    request = request_for(HTTP_GET, "/inspect");
    response = http_client_send(client, request);
    string_t *cookie_key = string_new_with("X-Cookie");
    const string_t *cookie = http_response_header_at(response, cookie_key, 0);
    TEST_ASSERT_TRUE(cookie && string_find(cookie, "one=1") >= 0 && string_find(cookie, "two=2") >= 0, "scoped cookies");
    http_response_free(response);
    TEST_ASSERT_TRUE(http_client_clear_cookies(client), "clear jar");
    response = http_client_send(client, request);
    TEST_ASSERT_TRUE(header_equals(response, "X-Cookie", ""), "cleared cookies");
    http_response_free(response); http_request_free(request); string_free(cookie_key);
    TEST_ASSERT_TRUE(http_client_set_cookies(client, false), "disable cookie engine");
    TEST_ASSERT_TRUE(http_client_set_redirects(client, 2), "same-origin redirects");
    request = request_for(HTTP_GET, "/redirect");
    response = http_client_send(client, request);
    TEST_ASSERT_INT_EQ(http_response_status(response), 200);
    http_response_free(response); http_request_free(request);
    request = request_for(HTTP_GET, "/redirect-loop");
    response = http_client_send(client, request);
    TEST_ASSERT_TRUE(!response && http_client_error(client) == HTTP_ERROR_LIMIT, "bounded redirect loop");
    http_request_free(request);
    request = request_for(HTTP_GET, "/redirect-away");
    response = http_client_send(client, request);
    TEST_ASSERT_TRUE(!response && http_client_error(client) == HTTP_ERROR_PROTOCOL, "cross-origin blocked");
    http_request_free(request);
    TEST_ASSERT_TRUE(!http_client_set_redirects(client, 17), "hop maximum");
    string_t *url = string_sprintf("http://127.0.0.1:%u/inspect", plain_port);
    string_t *id = string_new_with("client id"), *secret = string_new_with("a:b");
    string_t *scope = string_new_with("read write"), *refresh = string_new_with("old token");
    response = http_oauth2_client_credentials(client, url, id, secret, scope);
    string_t *text = http_response_text(response);
    TEST_ASSERT_STR_EQ(string_c_str(text), "grant_type=client_credentials&scope=read+write");
    TEST_ASSERT_TRUE(header_equals(response, "X-Authorization", "Basic Y2xpZW50K2lkOmElM0Fi"), "OAuth Basic form encoding");
    string_free(text); http_response_free(response);
    response = http_oauth2_refresh(client, url, id, secret, refresh, NULL);
    text = http_response_text(response);
    TEST_ASSERT_STR_EQ(string_c_str(text), "grant_type=refresh_token&refresh_token=old+token");
    string_free(text); http_response_free(response);
    string_t *unsafe = string_new_with("http://example.invalid/token");
    TEST_ASSERT_TRUE(!http_oauth2_client_credentials(client, unsafe, id, secret, NULL), "credentials require TLS");
    TEST_ASSERT_TRUE(!http_oauth2_refresh(client, url, id, secret, NULL, NULL), "refresh token required");
    string_free(unsafe); string_free(url); string_free(id); string_free(secret); string_free(scope); string_free(refresh);
    http_client_free(client);
}

static http_soap_t *parse_soap(const char *source)
{
    string_t *text = string_new_with(source);
    xml_t *document = xml_from_text(text);
    http_soap_t *soap = http_soap_parse(document);
    xml_free(document); string_free(text);
    return soap;
}

static void test_http_soap_envelopes_and_faults(void)
{
    string_t *source = string_new_with("<m:answer xmlns:m=\"urn:mars\">42</m:answer>");
    xml_t *document = xml_from_text(source);
    const xml_t *payload = xml_child_at(document, 0);
    http_client_t *client = http_client_new();
    http_request_t *request = request_for(HTTP_POST, "/inspect");
    string_t *action = string_new_with("urn:mars:answer");
    TEST_ASSERT_TRUE(http_request_set_soap(request, HTTP_SOAP_11, action, payload, NULL), "SOAP 1.1 request");
    http_response_t *response = http_client_send(client, request);
    TEST_ASSERT_TRUE(header_equals(response, "X-Type", "text/xml; charset=utf-8"), "SOAP 1.1 media type");
    TEST_ASSERT_TRUE(header_equals(response, "X-SOAPAction", "\"urn:mars:answer\""), "SOAP 1.1 action");
    xml_t *reply = http_response_xml(response);
    http_soap_t *soap = http_soap_parse(reply);
    TEST_ASSERT_NOT_NULL(soap);
    TEST_ASSERT_TRUE(!http_soap_is_fault(soap) && http_soap_body(soap), "normal SOAP payload");
    http_soap_free(soap); xml_free(reply); http_response_free(response);
    TEST_ASSERT_TRUE(http_request_set_soap(request, HTTP_SOAP_12, action, payload, NULL), "SOAP 1.2 request");
    response = http_client_send(client, request);
    TEST_ASSERT_TRUE(header_equals(response, "X-Type", "application/soap+xml; charset=utf-8; action=\"urn:mars:answer\""),
                     "SOAP 1.2 action parameter");
    TEST_ASSERT_TRUE(header_equals(response, "X-SOAPAction", ""), "stale SOAPAction removed");
    http_response_free(response);
    soap = parse_soap("<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\"><s:Body>"
                      "<s:Fault><faultcode>s:Server</faultcode><faultstring>Unavailable</faultstring></s:Fault>"
                      "</s:Body></s:Envelope>");
    TEST_ASSERT_TRUE(http_soap_is_fault(soap), "SOAP 1.1 fault");
    TEST_ASSERT_STR_EQ(string_c_str(http_soap_fault_code(soap)), "s:Server");
    TEST_ASSERT_STR_EQ(string_c_str(http_soap_fault_reason(soap)), "Unavailable");
    http_soap_free(soap);
    soap = parse_soap("<Envelope xmlns=\"http://www.w3.org/2003/05/soap-envelope\"><Body><Fault>"
                      "<Code><Value>Receiver</Value></Code><Reason><Text xml:lang=\"en\">Try later</Text></Reason>"
                      "</Fault></Body></Envelope>");
    TEST_ASSERT_TRUE(http_soap_is_fault(soap), "SOAP 1.2 default namespace");
    TEST_ASSERT_STR_EQ(string_c_str(http_soap_fault_reason(soap)), "Try later");
    http_soap_free(soap);
    TEST_ASSERT_TRUE(!parse_soap("<Envelope xmlns=\"urn:wrong\"><Body/></Envelope>"), "wrong namespace rejected");
    TEST_ASSERT_TRUE(!parse_soap("<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\">"
                                "<s:Body/><s:Body/></s:Envelope>"), "duplicate body rejected");
    TEST_ASSERT_TRUE(!parse_soap("<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\">"
                                "<s:Header><required/></s:Header><s:Body/></s:Envelope>"), "unhandled headers rejected");
    string_free(source); string_free(action); xml_free(document);
    http_request_free(request); http_client_free(client);
}

typedef struct { unsigned count; bool accept; } event_state_t;
static bool check_event(const string_t *event, const string_t *data, const string_t *id,
                        bool has_retry, uint64_t retry, void *context)
{
    event_state_t *state = context;
    ++state->count;
    if (!state->accept) return false;
    return string_view_equals_literal(string_view_all(event), "update") &&
           string_view_equals_literal(string_view_all(data), "café\n42") &&
           string_view_equals_literal(string_view_all(id), "7") && has_retry && retry == 1500;
}

static void test_http_event_stream(void)
{
    const char *input = "\xEF\xBB\xBF: comment\r\nid: 7\rretry: 1500\nevent: update\ndata: café\r\ndata: 42\n\n";
    event_state_t state = {.accept = true};
    http_event_reader_t *reader = http_event_reader_new(1024, check_event, &state);
    TEST_ASSERT_NOT_NULL(reader);
    for (size_t i = 0; i < strlen(input); ++i)
        TEST_ASSERT_TRUE(http_event_reader_feed(reader, input + i, 1), "split bytes, UTF-8, CRLF and BOM");
    TEST_ASSERT_INT_EQ(state.count, 1);
    TEST_ASSERT_TRUE(http_event_reader_feed(reader, "data: discarded", 15), "unterminated event");
    TEST_ASSERT_TRUE(http_event_reader_finish(reader), "valid EOF");
    TEST_ASSERT_INT_EQ(state.count, 1);
    TEST_ASSERT_TRUE(!http_event_reader_feed(reader, "", 0), "finished reader");
    http_event_reader_free(reader);
    reader = http_event_reader_new(4, check_event, &state);
    TEST_ASSERT_TRUE(!http_event_reader_feed(reader, "12345", 5), "line budget");
    TEST_ASSERT_TRUE(!http_event_reader_finish(reader), "sticky limit failure");
    http_event_reader_free(reader);
    reader = http_event_reader_new(0, check_event, &state);
    TEST_ASSERT_TRUE(!http_event_reader_feed(reader, "\xff\n", 2), "invalid UTF-8");
    http_event_reader_free(reader);
    state.accept = false;
    reader = http_event_reader_new(0, check_event, &state);
    TEST_ASSERT_TRUE(!http_event_reader_feed(reader, "data: x\n\n", 9), "callback cancellation");
    http_event_reader_free(reader);
    reader = http_event_reader_new(1024, check_event, &state);
    uint64_t retry = 99;
    TEST_ASSERT_TRUE(!http_event_reader_retry(reader, &retry) && retry == 99, "retry output unchanged");
    const char metadata[] = "id: 7\nretry: 0\n";
    TEST_ASSERT_TRUE(http_event_reader_feed(reader, metadata, sizeof(metadata) - 1), "metadata without event");
    TEST_ASSERT_STR_EQ(string_c_str(http_event_reader_id(reader)), "7");
    TEST_ASSERT_TRUE(http_event_reader_retry(reader, &retry) && retry == 0, "zero retry preserved");
    http_event_reader_free(reader);
}

static void test_http_client_identity(void)
{
    unsigned port;
    pid_t pid = start_fixture(true, true, &port);
    TEST_ASSERT_TRUE(pid > 0, "start mutual TLS fixture");
    http_client_t *client = http_client_new();
    string_t *cert = string_new_with("tests/http/test-cert.pem"), *key = string_new_with("tests/http/test-key.pem");
    string_t *url = string_sprintf("https://localhost:%u/json", port);
    http_request_t *request = http_request_new(HTTP_GET, url);
    TEST_ASSERT_TRUE(http_client_set_ca_file(client, cert), "trust test CA");
    http_response_t *response = http_client_send(client, request);
    bool rejected = !response;
    http_response_free(response);
    TEST_ASSERT_TRUE(rejected, "server requires client certificate");
    TEST_ASSERT_TRUE(http_client_set_identity_files(client, cert, key, NULL), "load client identity");
    response = http_client_send(client, request);
    bool accepted = http_response_ok(response);
    http_response_free(response);
    TEST_ASSERT_TRUE(accepted, "verified mutual TLS exchange");
    TEST_ASSERT_TRUE(!http_client_set_identity_files(client, cert, NULL, NULL), "incomplete identity rejected");
    TEST_ASSERT_TRUE(http_client_set_identity_files(client, NULL, NULL, NULL), "clear identity and connection cache");
    response = http_client_send(client, request);
    rejected = !response;
    http_response_free(response);
    http_request_free(request); http_client_free(client);
    string_free(cert); string_free(key); string_free(url); stop_fixture(pid);
    TEST_ASSERT_TRUE(rejected, "old authenticated connection not reused");
}

static pid_t start_protocol_fixture(const char *script, bool tls, unsigned *port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in address = {.sin_family = AF_INET, .sin_addr.s_addr = htonl(INADDR_LOOPBACK)};
    socklen_t length = sizeof(address);
    if (fd < 0) return -1;
    if (bind(fd, (struct sockaddr *)&address, length) || listen(fd, 8) ||
        getsockname(fd, (struct sockaddr *)&address, &length)) { close(fd); return -1; }
    *port = ntohs(address.sin_port);
    string_t *descriptor = string_sprintf("%d", fd);
    pid_t pid = descriptor ? fork() : -1;
    if (pid == 0) {
        if (tls)
            execlp("python3", "python3", script, string_c_str(descriptor),
                   "tests/http/test-cert.pem", "tests/http/test-key.pem", (char *)NULL);
        else
            execlp("python3", "python3", script, string_c_str(descriptor), (char *)NULL);
        _exit(127);
    }
    string_free(descriptor); close(fd);
    return pid;
}

static void test_http_grpc_unary(void)
{
    unsigned port = 0;
    pid_t pid = start_protocol_fixture("tests/http/grpc_fixture.py", false, &port);
    TEST_ASSERT_TRUE(pid > 0, "start local HTTP/2 peer");
    http_client_t *client = http_client_new();
    string_t *url = string_sprintf("http://127.0.0.1:%u/mars.Test/Echo", port);
    bool passed = true;
    for (unsigned mode = 0; mode < 8; ++mode) {
        protobuf_t *message = protobuf_new(1024, 10);
        http_request_t *request = http_request_new(HTTP_POST, url);
        bool ready = message && request && protobuf_add_integer(message, 1, PROTOBUF_VARINT, mode) &&
                     http_request_set_grpc(request, message);
        http_response_t *response = ready ? http_client_send(client, request) : NULL;
        http_grpc_t *result = response ? http_response_grpc(response, 10) : NULL;
        if (!response) passed = false;
        if (!mode) {
            uint64_t value = UINT64_MAX;
            passed = passed && http_grpc_status(result) == 0 &&
                     protobuf_integer(http_grpc_payload(result), 0, &value) && value == 0;
        } else if (mode == 1) {
            passed = passed && http_grpc_status(result) == 7 && !http_grpc_payload(result) &&
                     string_view_equals_literal(string_view_all(http_grpc_message(result)), "Permission denied");
        } else passed = passed && !result;
        http_grpc_free(result); http_response_free(response); http_request_free(request); protobuf_free(message);
    }
    string_free(url); http_client_free(client); stop_fixture(pid);
    TEST_ASSERT_TRUE(passed, "HTTP/2 unary success, status errors, truncation, streaming and compression rejection");
}

static void test_http_grpc_verified_tls(void)
{
    unsigned port = 0;
    pid_t pid = start_protocol_fixture("tests/http/grpc_fixture.py", true, &port);
    http_client_t *client = http_client_new();
    string_t *url = string_sprintf("https://localhost:%u/mars.Test/Echo", port);
    string_t *ca = string_new_with("tests/http/test-cert.pem");
    http_request_t *request = http_request_new(HTTP_POST, url);
    protobuf_t *message = protobuf_new(1024, 16);
    bool ready = pid > 0 && client && request && message &&
                 protobuf_add_integer(message, 1, PROTOBUF_VARINT, 150) && http_request_set_grpc(request, message);
    http_response_t *response = ready ? http_client_send(client, request) : NULL;
    bool rejected = ready && !response;
    http_response_free(response);
    bool trusted = http_client_set_ca_file(client, ca);
    response = ready && trusted ? http_client_send(client, request) : NULL;
    http_grpc_t *result = response ? http_response_grpc(response, 16) : NULL;
    uint64_t value = 0;
    bool accepted = http_grpc_status(result) == 0 && protobuf_integer(http_grpc_payload(result), 0, &value) && value == 150;
    http_grpc_free(result); http_response_free(response); protobuf_free(message);
    http_request_free(request); http_client_free(client); string_free(url); string_free(ca); stop_fixture(pid);
    TEST_ASSERT_TRUE(rejected && accepted, "HTTP/2 via ALPN with verified TLS, rejecting untrusted certificates");
}

static void test_http_websocket_messages(void)
{
    unsigned port = 0;
    pid_t pid = start_protocol_fixture("tests/http/websocket_fixture.py", false, &port);
    TEST_ASSERT_TRUE(pid > 0, "start WebSocket peer");
    http_client_t *client = http_client_new();
    string_t *url = string_sprintf("http://127.0.0.1:%u/echo", port);
    http_request_t *request = http_request_new(HTTP_GET, url);
    http_websocket_t *socket = http_websocket_open(client, request);
    bool passed = socket != NULL;
    unsigned char *data = malloc(131072);
    if (!data) passed = false;
    for (size_t i = 0; data && i < 131072; ++i) data[i] = (unsigned char)i;
    bool text = true;
    bool sent = socket && data && http_websocket_send(socket, false, data, 131072);
    array_t *reply = sent ? http_websocket_receive(socket, &text) : NULL;
    passed = passed && reply && !text && array_size(reply) == 131072 &&
             !memcmp(array_get(reply, 0), data, 131072);
    array_destroy(reply); free(data);
    sent = socket && http_websocket_send(socket, true, "", 0);
    reply = sent ? http_websocket_receive(socket, &text) : NULL;
    passed = passed && reply && text && !array_size(reply);
    array_destroy(reply);
    passed = passed && !http_websocket_send(socket, true, "\xff", 1) && http_websocket_is_open(socket);
    passed = passed && !http_client_set_cookies(client, true);
    passed = passed && !http_websocket_close(socket, 1005, NULL) && http_websocket_is_open(socket);
    passed = passed && http_websocket_close(socket, 1000, NULL) && !http_websocket_is_open(socket);
    http_websocket_free(socket); http_request_free(request); string_free(url);
    url = string_sprintf("http://127.0.0.1:%u/fragment", port);
    request = http_request_new(HTTP_GET, url);
    socket = http_websocket_open(client, request);
    reply = socket ? http_websocket_receive(socket, &text) : NULL;
    passed = passed && reply && text && array_size(reply) == 5 && !memcmp(array_get(reply, 0), "café", 5);
    array_destroy(reply);
    if (socket) http_websocket_close(socket, 1000, NULL);
    http_websocket_free(socket); http_request_free(request); string_free(url);
    http_client_free(client); stop_fixture(pid);
    TEST_ASSERT_TRUE(passed, "large binary, empty text, fragmented UTF-8, interleaved ping and close handling");
}

static void test_http_websocket_failures_and_tls(void)
{
    unsigned port = 0;
    pid_t pid = start_protocol_fixture("tests/http/websocket_fixture.py", false, &port);
    TEST_ASSERT_TRUE(pid > 0, "start rejection peer");
    http_client_t *client = http_client_new();
    http_limits_t limits = {.total_timeout_ms = 300, .max_body_bytes = 1024};
    bool passed = client && http_client_set_limits(client, &limits);
    const char *paths[] = {"/invalid", "/large", "/drop", "/silent", "/peer-close", "/extension", "/reject"};
    const http_error_t errors[] = {HTTP_ERROR_PROTOCOL, HTTP_ERROR_LIMIT, HTTP_ERROR_TRANSPORT,
                                  HTTP_ERROR_TIMEOUT, HTTP_ERROR_NONE, HTTP_ERROR_PROTOCOL, HTTP_ERROR_PROTOCOL};
    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        string_t *url = string_sprintf("http://127.0.0.1:%u%s", port, paths[i]);
        http_request_t *request = http_request_new(HTTP_GET, url);
        http_websocket_t *socket = http_websocket_open(client, request);
        bool text;
        array_t *reply = socket ? http_websocket_receive(socket, &text) : NULL;
        bool expected = !reply && !http_websocket_is_open(socket) &&
                        (http_client_error(client) == errors[i] ||
                         (i == 6 && http_client_error(client) == HTTP_ERROR_TRANSPORT));
        passed = passed && expected;
        array_destroy(reply); http_websocket_free(socket); http_request_free(request); string_free(url);
        /* The deliberately silent peer sleeps; restart it before subsequent cases. */
        if (i == 3) {
            stop_fixture(pid);
            pid = start_protocol_fixture("tests/http/websocket_fixture.py", false, &port);
            if (pid <= 0) { passed = false; break; }
        }
    }
    stop_fixture(pid);
    pid = start_protocol_fixture("tests/http/websocket_fixture.py", true, &port);
    http_limits_t normal = {0};
    passed = passed && pid > 0 && http_client_set_limits(client, &normal);
    string_t *url = string_sprintf("https://localhost:%u/echo", port);
    http_request_t *request = http_request_new(HTTP_GET, url);
    http_websocket_t *socket = http_websocket_open(client, request);
    passed = passed && !socket;
    http_websocket_free(socket);
    string_t *ca = string_new_with("tests/http/test-cert.pem");
    bool trusted = http_client_set_ca_file(client, ca);
    socket = trusted ? http_websocket_open(client, request) : NULL;
    bool text = false, sent = socket && http_websocket_send(socket, true, "secure", 6);
    array_t *reply = sent ? http_websocket_receive(socket, &text) : NULL;
    passed = passed && reply && text && array_size(reply) == 6 && !memcmp(array_get(reply, 0), "secure", 6);
    array_destroy(reply);
    if (socket) http_websocket_close(socket, 1000, NULL);
    http_websocket_free(socket); http_request_free(request); string_free(url); string_free(ca);
    http_client_free(client); stop_fixture(pid);
    TEST_ASSERT_TRUE(passed, "protocol rejection, limits, timeout, closure and verified WSS");
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

/* README: execute the complete offline forms/SOAP/events program. */
static void example_http_services(void)
{
    TEST_ASSERT_INT_EQ(http_services_example_main(), EXIT_SUCCESS);
}

/* README: unary gRPC against the offline HTTP/2 echo fixture. */
static void example_http_grpc(void)
{
    unsigned port = 0;
    pid_t pid = start_protocol_fixture("tests/http/grpc_fixture.py", false, &port);
    string_t *url = string_sprintf("http://127.0.0.1:%u/mars.Test/Echo", port);
    char *argv[] = {"http_grpc", (char *)string_c_str(url), NULL};
    int status = pid > 0 && url ? http_grpc_example_main(2, argv) : EXIT_FAILURE;
    stop_fixture(pid); string_free(url);
    TEST_ASSERT_INT_EQ(status, EXIT_SUCCESS);
}

/* README: text messaging against the offline WebSocket echo fixture. */
static void example_http_websocket(void)
{
    unsigned port = 0;
    pid_t pid = start_protocol_fixture("tests/http/websocket_fixture.py", false, &port);
    string_t *url = string_sprintf("http://127.0.0.1:%u/echo", port);
    char *argv[] = {"http_websocket", (char *)string_c_str(url), NULL};
    int status = pid > 0 && url ? http_websocket_example_main(2, argv) : EXIT_FAILURE;
    stop_fixture(pid); string_free(url);
    TEST_ASSERT_INT_EQ(status, EXIT_SUCCESS);
}

int tests_main(void)
{
    plain_pid = start_fixture(false, false, &plain_port);
    tls_pid = start_fixture(true, false, &tls_port);
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
    TEST_RUN_IN_GROUP(test_http_forms_auth_and_multipart, tests, NULL);
    TEST_RUN_IN_GROUP(test_http_session_redirects_oauth, tests, NULL);
    TEST_RUN_IN_GROUP(test_http_soap_envelopes_and_faults, tests, NULL);
    TEST_RUN_IN_GROUP(test_http_event_stream, tests, NULL);
    TEST_RUN_IN_GROUP(test_http_client_identity, tests, NULL);
    TEST_RUN_IN_GROUP(test_http_grpc_unary, tests, NULL);
    TEST_RUN_IN_GROUP(test_http_grpc_verified_tls, tests, NULL);
    TEST_RUN_IN_GROUP(test_http_websocket_messages, tests, NULL);
    TEST_RUN_IN_GROUP(test_http_websocket_failures_and_tls, tests, NULL);
    TEST_SECTION("README Output Examples");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_http_json, readme_examples, "http,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_http_post_json, readme_examples, "http,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_http_xml, readme_examples, "http,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_http_services, readme_examples, "http,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_http_grpc, readme_examples, "http,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_http_websocket, readme_examples, "http,readme,output");
    stop_fixture(plain_pid); stop_fixture(tls_pid);
    return TEST_EXIT_CODE();
}
