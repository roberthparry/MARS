/**
 * @file http_fixture_websocket.c
 * @brief RFC 6455 offline echo and malformed-message peer.
 *
 * Supplies SHA-1/base64 upgrade acceptance using OpenSSL, then handles masked
 * client frames and unmasked server frames. Special routes retain fragmented
 * UTF-8 with an interleaved ping, invalid text, oversized payloads, rejected
 * extensions, disconnects, timeouts and peer-initiated close handshakes.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/evp.h>

#include "http_fixture.h"

static bool fixture_ws_send(fixture_connection_t *connection, unsigned kind, const void *data, size_t size, bool final)
{
    unsigned char head[10] = {(unsigned char)((final ? 128 : 0) | kind)};
    size_t head_size = 2;
    if (size < 126)
        head[1] = (unsigned char)size;
    else if (size < 65536) {
        head[1] = 126;
        head[2] = (unsigned char)(size >> 8);
        head[3] = (unsigned char)size;
        head_size = 4;
    } else {
        head[1] = 127;
        uint64_t length = size;
        for (unsigned i = 0; i < 8; ++i)
            head[9 - i] = (unsigned char)(length >> (i * 8));
        head_size = 10;
    }
    return fixture_write(connection, head, head_size) && fixture_write(connection, data, size);
}

static unsigned char *fixture_ws_receive(fixture_connection_t *connection, unsigned *kind, size_t *size)
{
    unsigned char head[2], extended[8], mask[4];
    if (!fixture_read(connection, head, sizeof(head)) || !(head[1] & 128))
        return NULL;
    uint64_t length = head[1] & 127;
    if (length >= 126) {
        unsigned count = length == 126 ? 2 : 8;
        if (!fixture_read(connection, extended, count))
            return NULL;
        length = 0;
        for (unsigned i = 0; i < count; ++i)
            length = (length << 8) | extended[i];
    }
    if (length > 8388608 || !fixture_read(connection, mask, sizeof(mask)))
        return NULL;
    *size = (size_t)length;
    *kind = head[0] & 15;
    unsigned char *body = malloc(*size + 1);
    if (!body)
        return NULL;
    if (!fixture_read(connection, body, *size)) {
        free(body);
        return NULL;
    }
    for (size_t i = 0; i < *size; ++i)
        body[i] ^= mask[i % 4];
    return body;
}

static bool fixture_ws_upgrade(fixture_connection_t *connection, const fixture_request_t *request)
{
    if (!request->websocket_key)
        return false;
    EVP_MD_CTX *digest = EVP_MD_CTX_new();
    unsigned char hash[EVP_MAX_MD_SIZE], encoded[4 * ((EVP_MAX_MD_SIZE + 2) / 3) + 1];
    unsigned length = 0;
    const char *suffix = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    bool ok = digest && EVP_DigestInit_ex(digest, EVP_sha1(), NULL) == 1 &&
              EVP_DigestUpdate(digest, string_c_str(request->websocket_key), string_byte_length(request->websocket_key)) == 1 &&
              EVP_DigestUpdate(digest, suffix, strlen(suffix)) == 1 &&
              EVP_DigestFinal_ex(digest, hash, &length) == 1;
    EVP_MD_CTX_free(digest);
    if (!ok || EVP_EncodeBlock(encoded, hash, (int)length) <= 0)
        return false;
    string_t *response = string_sprintf(
             "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
             "Sec-WebSocket-Accept: %s\r\n%s\r\n", (const char *)encoded,
             fixture_equal(request->path, "/extension") ? "Sec-WebSocket-Extensions: permessage-deflate\r\n" : "");
    bool sent = response && fixture_string(connection, response);
    string_free(response);
    return sent;
}

static void fixture_ws_session(fixture_connection_t *connection, const fixture_request_t *request)
{
    const string_t *path = request->path;
    if (fixture_equal(path, "/reject")) {
        fixture_text(connection, "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n");
        return;
    }
    if (!fixture_ws_upgrade(connection, request))
        return;
    bool ok = true;
    if (fixture_equal(path, "/fragment"))
        ok = fixture_ws_send(connection, 1, "caf\xc3", 4, false) &&
             fixture_ws_send(connection, 9, "ping", 4, true) && fixture_ws_send(connection, 0, "\xa9", 1, true);
    else if (fixture_equal(path, "/invalid"))
        ok = fixture_ws_send(connection, 1, "\xff", 1, true);
    else if (fixture_equal(path, "/large")) {
        unsigned char data[16384];
        memset(data, 'x', sizeof(data));
        ok = fixture_ws_send(connection, 2, data, sizeof(data), true);
    } else if (fixture_equal(path, "/drop"))
        return;
    else if (fixture_equal(path, "/silent")) {
        fixture_delay(connection, 600);
        return;
    } else if (fixture_equal(path, "/peer-close"))
        ok = fixture_ws_send(connection, 8, "\x03\xe8", 2, true);
    while (ok) {
        unsigned kind;
        size_t size;
        unsigned char *body = fixture_ws_receive(connection, &kind, &size);
        if (!body)
            return;
        if (kind != 10)
            ok = fixture_ws_send(connection, kind, body, size, true);
        free(body);
        if (kind == 8)
            return;
    }
}

/* Upgrade and serve one WebSocket connection, retaining ownership of request text. */
void fixture_websocket(fixture_connection_t *connection)
{
    fixture_request_t request;
    if (!fixture_request(connection, &request))
        return;
    fixture_ws_session(connection, &request);
    fixture_request_free(&request);
}
