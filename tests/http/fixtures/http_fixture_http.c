/**
 * @file http_fixture_http.c
 * @brief HTTP/1.1 reply catalogue for offline transport and README tests.
 *
 * Echoes request bodies and selected headers, supplies JSON/XML and compressed
 * content, and deliberately emits malformed, interim and truncated replies.
 * Persistent connections and concurrent slow requests exercise client reuse and
 * cancellation. Wire construction bypasses the HTTP library being tested;
 * request text and JSON documents use the public string and JSON modules.
 */
#include <stdlib.h>
#include <string.h>

#include "http_fixture.h"
#include "json.h"

static bool fixture_header(fixture_connection_t *connection, const char *name, const char *value)
{
    return fixture_text(connection, name) && fixture_text(connection, ": ") && fixture_text(connection, value) &&
           fixture_text(connection, "\r\n");
}

static bool fixture_request_field(fixture_connection_t *connection, const char *name, const string_t *value)
{
    return fixture_text(connection, name) && fixture_text(connection, ": ") && fixture_string(connection, value) &&
           fixture_text(connection, "\r\n");
}

static string_t *fixture_post(const fixture_request_t *request, const unsigned char *body)
{
    string_t *text = string_new();
    bool ok = text && string_append_utf8_exact(text, (const char *)body, request->length) == 0;
    json_t *value = ok ? (fixture_equal(request->type, "application/json") ? json_from_text(text) : json_new_null()) : NULL;
    json_t *data = ok ? json_new_string(text) : NULL;
    json_t *reply = json_new_object();
    string_t *data_key = string_new_with("data"), *json_key = string_new_with("json");
    ok = value && data && reply && data_key && json_key && json_object_set(reply, data_key, data) &&
         json_object_set(reply, json_key, value);
    string_t *output = ok ? json_to_string(reply) : NULL;
    string_free(json_key);
    string_free(data_key);
    json_free(reply);
    json_free(data);
    json_free(value);
    string_free(text);
    return output;
}

static bool fixture_http_reply(fixture_connection_t *connection, const fixture_request_t *request,
                                const unsigned char *input)
{
    const string_t *path = request->path;
    const unsigned char *body = input;
    size_t size = request->length;
    const char *status = "200 OK", *location = NULL, *type = NULL;
    unsigned char large[4096];
    string_t *owned = NULL;
    memset(large, 'x', sizeof(large));
    /* The bounded, fixed route vocabulary is intentionally explicit: each route
     * defines a distinct wire fault or response, rather than a growing API registry. */
    if (fixture_equal(path, "/truncated")) {
        fixture_text(connection, "HTTP/1.1 200 OK\r\nContent-Length: 100\r\n\r\nshort");
        return false;
    }
    if (fixture_equal(path, "/chunks"))
        return fixture_text(connection, "HTTP/1.1 103 Early Hints\r\nX-Early: discarded\r\n\r\n"
                                        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n"
                                        "Trailer: X-End\r\n\r\n3\r\none\r\n3\r\ntwo\r\n"
                                        "0\r\nX-End: yes\r\n\r\n");
    if (fixture_equal(path, "/badheader"))
        return fixture_text(connection, "HTTP/1.1 200 OK\r\nX-Invalid: \xff\r\nContent-Length: 0\r\n\r\n");
    if (fixture_equal(path, "/folded"))
        return fixture_text(connection, "HTTP/1.1 200 OK\r\nX-Fold: one\r\n two\r\nContent-Length: 0\r\n\r\n");
    if (fixture_equal(path, "/switch"))
        return fixture_text(connection, "HTTP/1.1 101 Switching Protocols\r\nConnection: Upgrade\r\n\r\n");
    if (fixture_equal(path, "/redirect-loop")) {
        status = "302 Found";
        body = (const unsigned char *)"loop";
        size = 4;
        location = "/redirect-loop";
    } else if (fixture_equal(path, "/redirect-away")) {
        status = "302 Found";
        body = (const unsigned char *)"blocked";
        size = 7;
        location = "http://example.invalid/";
    } else if (fixture_equal(path, "/get?message=MARS")) {
        body = (const unsigned char *)"{\"args\":{\"message\":\"MARS\"}}";
        size = strlen((const char *)body);
        type = "application/json";
    } else if (fixture_equal(path, "/post")) {
        owned = fixture_post(request, input);
        if (!owned)
            return false;
        body = (const unsigned char *)string_c_str(owned);
        size = string_byte_length(owned);
        type = "application/json";
    } else if (fixture_equal(path, "/json")) {
        body = (const unsigned char *)"{\"answer\":42}";
        size = 13;
        type = "application/json";
    } else if (fixture_equal(path, "/xml")) {
        body = (const unsigned char *)"<answer>42</answer>";
        size = 19;
        type = "application/xml";
    } else if (fixture_equal(path, "/binary")) {
        body = (const unsigned char *)"\x00\xff\xfe" "A";
        size = 4;
    } else if (fixture_equal(path, "/headers")) {
        body = (const unsigned char *)"headers";
        size = 7;
    } else if (fixture_equal(path, "/redirect")) {
        status = "302 Found";
        body = (const unsigned char *)"not followed";
        size = 12;
        location = "/json";
    } else if (fixture_equal(path, "/error")) {
        status = "404 Not Found";
        body = (const unsigned char *)"{\"error\":\"missing\"}";
        size = strlen((const char *)body);
    } else if (fixture_equal(path, "/large")) {
        body = large;
        size = sizeof(large);
    } else if (fixture_equal(path, "/gzip")) {
        /* Deterministic gzip member containing 4096 'x' bytes. Keep the wire
         * payload small so this route tests expansion limits, not download size. */
        static const unsigned char compressed[] = {
            0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xed, 0xc1, 0x01,
            0x0d, 0x00, 0x00, 0x00, 0xc2, 0xa0, 0xda, 0x8f, 0x6f, 0x0f, 0x07, 0x14, 0x00,
            0x00, 0x00, 0xf0, 0x6e, 0xc1, 0x77, 0x10, 0x3e, 0x00, 0x10, 0x00, 0x00
        };
        body = compressed;
        size = sizeof(compressed);
    } else if (fixture_equal(path, "/slow")) {
        fixture_delay(connection, 300);
        body = (const unsigned char *)"late";
        size = 4;
    } else if (!fixture_equal(path, "/inspect") && !fixture_equal(path, "/echo") && !fixture_equal(path, "/bigheader")) {
        status = "404 Not Found";
        body = (const unsigned char *)"unknown";
        size = 7;
    }
    string_t *head = string_sprintf("HTTP/1.1 %s\r\nContent-Length: %zu\r\n", status, size);
    bool ok = head && fixture_string(connection, head);
    string_free(head);
    if (location)
        ok = ok && fixture_header(connection, "Location", location);
    if (type)
        ok = ok && fixture_header(connection, "Content-Type", type);
    if (request->close)
        ok = ok && fixture_header(connection, "Connection", "close");
    if (fixture_equal(path, "/inspect"))
        ok = ok && fixture_request_field(connection, "X-Authorization", request->authorization) &&
             fixture_request_field(connection, "X-Cookie", request->cookie) &&
             fixture_request_field(connection, "X-Type", request->type) &&
             fixture_request_field(connection, "X-SOAPAction", request->soap_action);
    if (fixture_equal(path, "/echo"))
        ok = ok && fixture_request_field(connection, "X-Method", request->method) &&
             fixture_request_field(connection, "X-Type", request->type) &&
             fixture_request_field(connection, "X-Custom", request->custom) &&
             fixture_request_field(connection, "X-Auth", request->authorization);
    if (fixture_equal(path, "/headers"))
        ok = ok && fixture_header(connection, "Set-Cookie", "one=1") &&
             fixture_header(connection, "set-cookie", "two=2");
    if (fixture_equal(path, "/gzip"))
        ok = ok && fixture_header(connection, "Content-Encoding", "gzip");
    if (fixture_equal(path, "/bigheader"))
        ok = ok && fixture_text(connection, "X-Large: ") && fixture_write(connection, large, sizeof(large)) &&
             fixture_text(connection, "\r\n");
    ok = ok && fixture_text(connection, "\r\n");
    if (!fixture_equal(request->method, "HEAD"))
        ok = ok && fixture_write(connection, body, size);
    string_free(owned);
    return ok;
}

/* Serve successive requests until a close, deliberate truncation or I/O failure. */
void fixture_http(fixture_connection_t *connection)
{
    fixture_request_t request;
    while (fixture_request(connection, &request)) {
        unsigned char *body = malloc(request.length + 1);
        if (!body) {
            fixture_request_free(&request);
            return;
        }
        bool ok = fixture_read(connection, body, request.length) && fixture_http_reply(connection, &request, body);
        free(body);
        bool close = request.close;
        fixture_request_free(&request);
        if (!ok || close)
            return;
    }
}
