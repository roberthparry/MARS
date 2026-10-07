/* Bounded synchronous WebSocket message I/O with deadline-aware non-blocking transport. */
#include <errno.h>
#include <poll.h>
#include <time.h>
#define MARS_HTTP_INTERNAL_ACCESS
#include "http_internal.h"

static uint64_t now_ms(void)
{
    struct timespec time;
    if (clock_gettime(CLOCK_MONOTONIC, &time))
        return 0;
    return (uint64_t)time.tv_sec * 1000 + (uint64_t)time.tv_nsec / 1000000;
}

static bool fail(http_websocket_t *socket, http_error_t error, const char *message)
{
    socket->open = false;
    http_fail(socket->client, error, message);
    return false;
}

static bool check_deadline(http_websocket_t *socket, uint64_t start)
{
    http_client_t *client = socket->client;
    if (client->cancel && client->cancel(client->cancel_data))
        return fail(socket, HTTP_ERROR_CANCELLED, "WebSocket operation cancelled");
    if (now_ms() - start >= client->limits.total_timeout_ms)
        return fail(socket, HTTP_ERROR_TIMEOUT, "WebSocket operation deadline exceeded");
    return true;
}

static bool wait_socket(http_websocket_t *socket, short events, uint64_t start)
{
    curl_socket_t descriptor;
    if (curl_easy_getinfo(socket->easy, CURLINFO_ACTIVESOCKET, &descriptor) != CURLE_OK ||
        descriptor == CURL_SOCKET_BAD)
        return fail(socket, HTTP_ERROR_TRANSPORT, "WebSocket has no active socket");
    while (check_deadline(socket, start)) {
        uint64_t elapsed = now_ms() - start;
        if (elapsed >= socket->client->limits.total_timeout_ms)
            return fail(socket, HTTP_ERROR_TIMEOUT, "WebSocket operation deadline exceeded");
        uint64_t left = socket->client->limits.total_timeout_ms - elapsed;
        struct pollfd item = {.fd = descriptor, .events = events};
        int result = poll(&item, 1, left > 100 ? 100 : (int)left);
        if (result > 0) {
            if (item.revents & events)
                return true;
            return fail(socket, HTTP_ERROR_TRANSPORT, "WebSocket transport closed unexpectedly");
        }
        if (result < 0 && errno != EINTR)
            return fail(socket, HTTP_ERROR_IO, "Cannot poll WebSocket transport");
    }
    return false;
}

static bool valid_utf8(const void *data, size_t size)
{
    string_t *text = string_new();
    bool ok = text && !string_append_utf8_exact(text, data, size);
    string_free(text);
    return ok;
}

static bool send_frame(http_websocket_t *socket, unsigned flags, const void *data, size_t size, uint64_t start)
{
    const unsigned char *bytes = data ? data : (const unsigned char *)"";
    size_t offset = 0;
    do {
        if (!check_deadline(socket, start))
            return false;
        size_t sent = 0;
        CURLcode code = curl_ws_send(socket->easy, bytes + offset, size - offset, &sent, 0, flags);
        if (sent > size - offset)
            return fail(socket, HTTP_ERROR_PROTOCOL, "Invalid WebSocket send count");
        offset += sent;
        if (code != CURLE_OK && code != CURLE_AGAIN)
            return fail(socket, HTTP_ERROR_TRANSPORT, curl_easy_strerror(code));
        if (code == CURLE_OK && offset == size)
            return true;
        if (!wait_socket(socket, POLLOUT, start))
            return false;
    } while (true);
}

/* Send a single complete text/binary frame, preserving embedded NUL bytes. */
bool http_websocket_send(http_websocket_t *socket, bool text, const void *data, size_t size)
{
    if (!socket || !socket->open || socket->busy)
        return false;
    http_fail(socket->client, HTTP_ERROR_NONE, NULL);
    if ((!data && size) || size > INT64_MAX || size > socket->client->limits.max_upload_bytes) {
        http_fail(socket->client, HTTP_ERROR_ARGUMENT, "Invalid or oversized WebSocket message");
        return false;
    }
    if (text && !valid_utf8(data, size)) {
        http_fail(socket->client, HTTP_ERROR_ARGUMENT, "WebSocket text must be valid UTF-8");
        return false;
    }
    socket->busy = true;
    bool ok = send_frame(socket, text ? CURLWS_TEXT : CURLWS_BINARY, data, size, now_ms());
    socket->busy = false;
    return ok;
}

static bool close_code(uint16_t code)
{
    return (code >= 1000 && code <= 1014 && code != 1004 && code != 1005 && code != 1006) ||
           (code >= 3000 && code <= 4999);
}

/* Send protocol closure without waiting indefinitely for a peer acknowledgement. */
bool http_websocket_close(http_websocket_t *socket, uint16_t code, const string_t *reason)
{
    if (!socket || !socket->open || socket->busy)
        return false;
    size_t size = reason ? string_byte_length(reason) : 0;
    if (!close_code(code) || size > 123) {
        http_fail(socket->client, HTTP_ERROR_ARGUMENT, "Invalid WebSocket close code or reason");
        return false;
    }
    unsigned char payload[125] = {(unsigned char)(code >> 8), (unsigned char)code};
    if (size) {
        const unsigned char *bytes = (const unsigned char *)string_c_str(reason);
        for (size_t i = 0; i < size; ++i)
            payload[i + 2] = bytes[i];
    }
    http_fail(socket->client, HTTP_ERROR_NONE, NULL);
    socket->busy = true;
    bool ok = send_frame(socket, CURLWS_CLOSE, payload, size + 2, now_ms());
    socket->open = socket->busy = false;
    return ok;
}

/* Reassemble fragmented messages while allowing bounded interleaved control frames. */
array_t *http_websocket_receive(http_websocket_t *socket, bool *text)
{
    if (!socket || !socket->open || socket->busy || !text)
        return NULL;
    http_fail(socket->client, HTTP_ERROR_NONE, NULL);
    socket->busy = true;
    uint64_t start = now_ms();
    array_t *message = array_create(1, NULL, NULL);
    unsigned char buffer[8192], control[125];
    size_t frame_offset = 0, control_size = 0;
    int message_type = 0, frame_flags = 0;
    bool complete = false;
    if (!message) {
        fail(socket, HTTP_ERROR_MEMORY, "Cannot allocate WebSocket message");
        goto done;
    }
    while (check_deadline(socket, start)) {
        size_t count = 0;
        const struct curl_ws_frame *meta = NULL;
        CURLcode code = curl_ws_recv(socket->easy, buffer, sizeof(buffer), &count, &meta);
        if (code == CURLE_AGAIN) {
            if (!wait_socket(socket, POLLIN, start))
                break;
            continue;
        }
        if (code != CURLE_OK) {
            fail(socket, HTTP_ERROR_TRANSPORT, curl_easy_strerror(code));
            break;
        }
        if (!meta || meta->offset < 0 || meta->bytesleft < 0 || (uint64_t)meta->offset != frame_offset) {
            fail(socket, HTTP_ERROR_PROTOCOL, "Invalid WebSocket frame offset");
            break;
        }
        int flags = meta->flags;
        size_t left = (uint64_t)meta->bytesleft > SIZE_MAX ? SIZE_MAX : (size_t)meta->bytesleft;
        if (!frame_offset)
            frame_flags = flags;
        if (frame_flags != flags) {
            fail(socket, HTTP_ERROR_PROTOCOL, "WebSocket frame type changed");
            break;
        }
        int controls = flags & (CURLWS_CLOSE | CURLWS_PING | CURLWS_PONG);
        if (controls) {
            if ((flags & (CURLWS_TEXT | CURLWS_BINARY | CURLWS_CONT)) || (controls & (controls - 1)) ||
                count > sizeof(control) - control_size || left > sizeof(control) - control_size - count) {
                fail(socket, HTTP_ERROR_PROTOCOL, "Invalid WebSocket control frame");
                break;
            }
            for (size_t i = 0; i < count; ++i)
                control[control_size++] = buffer[i];
        } else {
            int kind = flags & (CURLWS_TEXT | CURLWS_BINARY);
            if ((kind != CURLWS_TEXT && kind != CURLWS_BINARY) || (message_type && kind != message_type)) {
                fail(socket, HTTP_ERROR_PROTOCOL, "Invalid WebSocket message continuation");
                break;
            }
            message_type = kind;
            size_t used = array_size(message), limit = socket->client->limits.max_body_bytes;
            if (count > limit - used || left > limit - used - count) {
                fail(socket, HTTP_ERROR_LIMIT, "WebSocket message limit exceeded");
                break;
            }
            if (count && !array_append_carray(message, buffer, count)) {
                fail(socket, HTTP_ERROR_MEMORY, "Cannot grow WebSocket message");
                break;
            }
        }
        frame_offset += count;
        if (left)
            continue;
        frame_offset = 0;
        if (controls) {
            if (controls == CURLWS_CLOSE) {
                if (control_size == 1 || (control_size >= 2 && (!close_code(((uint16_t)control[0] << 8) | control[1]) ||
                                                                !valid_utf8(control + 2, control_size - 2)))) {
                    fail(socket, HTTP_ERROR_PROTOCOL, "Invalid WebSocket close payload");
                } else {
                    send_frame(socket, CURLWS_CLOSE, control, control_size, start);
                    socket->open = false;
                }
                break;
            }
            /* libcurl automatically answers PING; PONG needs no application action. */
            control_size = 0;
            continue;
        }
        if (!(flags & CURLWS_CONT)) {
            if (message_type == CURLWS_TEXT && !valid_utf8(array_get(message, 0), array_size(message))) {
                fail(socket, HTTP_ERROR_PROTOCOL, "Invalid UTF-8 WebSocket text");
                break;
            }
            *text = message_type == CURLWS_TEXT;
            complete = true;
            break;
        }
    }
done:
    socket->busy = false;
    if (!complete) {
        array_destroy(message);
        return NULL;
    }
    return message;
}
