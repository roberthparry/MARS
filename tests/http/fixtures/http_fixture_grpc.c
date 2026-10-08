/**
 * @file http_fixture_grpc.c
 * @brief Minimal HTTP/2 peer for unary gRPC transport regressions.
 *
 * Consumes the connection preface, acknowledges SETTINGS/PING, collects bounded
 * DATA and emits literal HPACK headers and trailers. This deliberately small
 * peer preserves the original fixture's malformed gRPC cases without depending
 * on a second HTTP/2 implementation. It serves one stream per connection and
 * advertises GOAWAY before completing it, including when TLS negotiates h2.
 */
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>

#include "http_fixture.h"

static void fixture_u32(unsigned char *target, uint32_t value)
{
    target[0] = (unsigned char)(value >> 24);
    target[1] = (unsigned char)(value >> 16);
    target[2] = (unsigned char)(value >> 8);
    target[3] = (unsigned char)value;
}

static bool fixture_h2_frame(fixture_connection_t *connection, unsigned char kind, unsigned char flags,
                              uint32_t stream, const void *payload, size_t size)
{
    unsigned char head[9] = {(unsigned char)(size >> 16), (unsigned char)(size >> 8), (unsigned char)size, kind, flags};
    fixture_u32(head + 5, stream);
    return fixture_write(connection, head, sizeof(head)) && fixture_write(connection, payload, size);
}

static size_t fixture_hpack_literal(unsigned char *target, const char *name, const char *value)
{
    size_t name_size = strlen(name), value_size = strlen(value);
    /* All callers supply fixed ASCII literals shorter than HPACK's 127-byte prefix. */
    target[0] = 0;
    target[1] = (unsigned char)name_size;
    memcpy(target + 2, name, name_size);
    target[2 + name_size] = (unsigned char)value_size;
    memcpy(target + 3 + name_size, value, value_size);
    return 3 + name_size + value_size;
}

/* Exchange one unary call, retaining all seven deliberate framing/status faults. */
void fixture_grpc(fixture_connection_t *connection)
{
    unsigned char preface[24], head[9], data[65536], body[131072];
    if (!fixture_read(connection, preface, sizeof(preface)) ||
        memcmp(preface, "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n", sizeof(preface)) ||
        !fixture_h2_frame(connection, 4, 0, 0, NULL, 0))
        return;
    size_t used = 0;
    uint32_t stream;
    for (;;) {
        if (!fixture_read(connection, head, sizeof(head)))
            return;
        size_t size = ((size_t)head[0] << 16) | ((size_t)head[1] << 8) | head[2];
        unsigned kind = head[3], flags = head[4];
        stream = ((uint32_t)(head[5] & 127) << 24) | ((uint32_t)head[6] << 16) |
                 ((uint32_t)head[7] << 8) | head[8];
        if (size > sizeof(data) || !fixture_read(connection, data, size))
            return;
        if (kind == 4 && !(flags & 1)) {
            if (!fixture_h2_frame(connection, 4, 1, 0, NULL, 0))
                return;
        } else if (kind == 6 && !(flags & 1)) {
            if (!fixture_h2_frame(connection, 6, 1, 0, data, size))
                return;
        } else if (kind == 0) {
            if ((flags & 8) || size > 65536 - used)
                return;
            memcpy(body + used, data, size);
            used += size;
        }
        if (stream && (kind == 0 || kind == 1) && (flags & 1))
            break;
    }
    unsigned test_case = used == 7 && body[5] == 8 ? body[6] : 0;
    unsigned char goaway[8] = {0}, headers[256] = {0x88}, trailers[256];
    fixture_u32(goaway, stream);
    size_t header_size = 1 + fixture_hpack_literal(headers + 1, "content-type",
                                                  test_case == 6 ? "text/plain" : "application/grpc+proto");
    if (!fixture_h2_frame(connection, 7, 0, 0, goaway, sizeof(goaway)) ||
        !fixture_h2_frame(connection, 1, 4, stream, headers, header_size))
        return;
    const char *status = "0", *message = NULL;
    if (test_case == 1) {
        status = "7";
        used = 0;
        message = "Permission%20denied";
    } else if (test_case == 2)
        --used;
    else if (test_case == 3) {
        memcpy(body + used, body, used);
        used *= 2;
    } else if (test_case == 4)
        body[0] = 1;
    else if (test_case == 5)
        status = "17";
    if (used && !fixture_h2_frame(connection, 0, 0, stream, body, used))
        return;
    size_t trailer_size = test_case == 7 ? 0 : fixture_hpack_literal(trailers, "grpc-status", status);
    if (message)
        trailer_size += fixture_hpack_literal(trailers + trailer_size, "grpc-message", message);
    if (!fixture_h2_frame(connection, 1, 5, stream, trailers, trailer_size))
        return;
    shutdown(connection->fd, SHUT_WR);
    /* Drain acknowledgements for at most 200 ms, including continuously arriving bytes. */
    if (!fixture_limit_lifetime(connection, 200))
        return;
    unsigned char byte;
    while (fixture_read(connection, &byte, 1)) {
    }
}
