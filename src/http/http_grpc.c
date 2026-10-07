/* Unary gRPC framing over verified HTTP/2; streaming and message compression are explicit non-features. */
#include <stdlib.h>
#define MARS_HTTP_INTERNAL_ACCESS
#include "http_internal.h"

struct _http_grpc_t {
    int status;
    string_t *message;
    protobuf_t *payload;
};

static bool header(http_request_t *request, const char *name, const char *value)
{
    string_t *key = string_new_with(name), *text = string_new_with(value);
    bool ok = key && text && http_headers_store(request->headers, key, text, true);
    string_free(key);
    string_free(text);
    return ok;
}

/* Serialise one length-prefixed Protocol Buffers message and require HTTP/2 transport. */
bool http_request_set_grpc(http_request_t *request, const protobuf_t *message)
{
    if (!request || request->method != HTTP_POST || !message)
        return false;
    array_t *body = protobuf_encode(message);
    array_t *framed = array_create(1, NULL, NULL);
    size_t size = array_size(body);
    unsigned char prefix[5] = {0, (unsigned char)(size >> 24), (unsigned char)(size >> 16), (unsigned char)(size >> 8),
                               (unsigned char)size};
    string_t *type = string_new_with("application/grpc+proto");
    bool ok = body && framed && type && size <= UINT32_MAX && array_append_carray(framed, prefix, 5) &&
              (!size || array_append_carray(framed, array_get(body, 0), size)) && header(request, "te", "trailers") &&
              header(request, "grpc-accept-encoding", "identity") && header(request, "grpc-encoding", "identity") &&
              http_request_set_body(request, array_get(framed, 0), array_size(framed), type);
    if (ok)
        request->http2 = true;
    array_destroy(body);
    array_destroy(framed);
    string_free(type);
    return ok;
}

static const string_t *single(const http_response_t *response, const char *name, size_t *count)
{
    string_t *key = string_new_with(name);
    *count = key ? http_response_header_count(response, key) : SIZE_MAX;
    const string_t *value = *count == 1 ? http_response_header_at(response, key, 0) : NULL;
    string_free(key);
    return value;
}

static bool media_type(const string_t *type)
{
    string_cursor_t *cursor = type ? string_cursor_new(type) : NULL;
    if (!cursor)
        return false;
    bool ok = string_cursor_consume(cursor, "application/grpc");
    if (ok)
        string_cursor_consume(cursor, "+proto");
    unsigned char ch;
    ok = ok && (string_cursor_done(cursor) ||
                (string_cursor_peek_ascii(cursor, &ch) && (ch == ';' || ch == ' ' || ch == '\t')));
    string_cursor_free(cursor);
    return ok;
}

static int status_code(const string_t *text)
{
    string_cursor_t *cursor = text ? string_cursor_new(text) : NULL;
    if (!cursor || string_cursor_done(cursor)) {
        string_cursor_free(cursor);
        return -1;
    }
    int value = 0;
    bool ok = true;
    while (ok && !string_cursor_done(cursor)) {
        unsigned char ch;
        ok = string_cursor_peek_ascii(cursor, &ch) && ch >= '0' && ch <= '9' && value <= 16;
        if (ok)
            value = value * 10 + ch - '0';
        string_cursor_next(cursor);
    }
    string_cursor_free(cursor);
    return ok && value <= 16 ? value : -1;
}

static int hex(unsigned char ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    return -1;
}

static string_t *decode_message(const string_t *text)
{
    if (!text)
        return string_new();
    string_cursor_t *cursor = string_cursor_new(text);
    array_t *bytes = array_create(1, NULL, NULL);
    bool ok = cursor && bytes;
    while (ok && !string_cursor_done(cursor)) {
        unsigned char ch, hi, lo;
        ok = string_cursor_peek_ascii(cursor, &ch);
        string_cursor_next(cursor);
        if (ok && ch == '%') {
            ok = string_cursor_peek_ascii(cursor, &hi);
            string_cursor_next(cursor);
            ok = ok && string_cursor_peek_ascii(cursor, &lo);
            string_cursor_next(cursor);
            ok = ok && hex(hi) >= 0 && hex(lo) >= 0;
            if (ok)
                ch = hex(hi) * 16 + hex(lo);
        }
        if (ok)
            ok = array_add(bytes, &ch);
    }
    string_t *result = ok ? string_new() : NULL;
    if (result && string_append_utf8_exact(result, array_get(bytes, 0), array_size(bytes))) {
        string_free(result);
        result = NULL;
    }
    array_destroy(bytes);
    string_cursor_free(cursor);
    return result;
}

/* Distinguish protocol errors from a completed unary call with a non-zero application status. */
http_grpc_t *http_response_grpc(const http_response_t *response, size_t max_fields)
{
    if (!response || response->streamed || !response->http2 || response->status != 200)
        return NULL;
    size_t count;
    const string_t *type = single(response, "content-type", &count);
    if (count != 1 || !media_type(type))
        return NULL;
    int status = status_code(single(response, "grpc-status", &count));
    if (count != 1 || status < 0)
        return NULL;
    const string_t *encoding = single(response, "grpc-encoding", &count);
    if (count > 1 || (encoding && !string_view_equals_literal(string_view_all(encoding), "identity")))
        return NULL;
    const string_t *message = single(response, "grpc-message", &count);
    if (count > 1)
        return NULL;
    http_grpc_t *result = calloc(1, sizeof(*result));
    if (!result)
        return NULL;
    result->status = status;
    result->message = decode_message(message);
    if (!result->message)
        goto fail;
    size_t size = http_response_body_size(response);
    const unsigned char *body = http_response_body(response);
    if (!size && status)
        return result;
    if (size < 5 || body[0])
        goto fail;
    uint32_t length = ((uint32_t)body[1] << 24) | ((uint32_t)body[2] << 16) | ((uint32_t)body[3] << 8) | body[4];
    if (length != size - 5)
        goto fail;
    /* A unary success requires exactly one message; errors never expose an application payload. */
    if (!status) {
        result->payload = protobuf_decode(body + 5, length, 67108864, max_fields);
        if (!result->payload)
            goto fail;
    }
    return result;
fail:
    http_grpc_free(result);
    return NULL;
}

/* Release the decoded status text and wire message. */
void http_grpc_free(http_grpc_t *result)
{
    if (result) {
        string_free(result->message);
        protobuf_free(result->payload);
        free(result);
    }
}

/* Return the protocol's application status. */
int http_grpc_status(const http_grpc_t *result)
{
    return result ? result->status : -1;
}

/* Borrow the UTF-8 status message. */
const string_t *http_grpc_message(const http_grpc_t *result)
{
    return result ? result->message : NULL;
}

/* Borrow the successful unary response message. */
const protobuf_t *http_grpc_payload(const http_grpc_t *result)
{
    return result ? result->payload : NULL;
}
