/**
 * @file http_websocket.c
 * @brief Verified WebSocket connection establishment.
 *
 * Performs the upgrade using the HTTP client's TLS, credential and header policies and owns connection lifetime.
 * Message framing and deadline-aware socket operations are implemented in the I/O companion.
 *
 * This is part of the synchronous http.h client. Keep transport limits, TLS policy and handle ownership consistent
 * with the shared client machinery; serving requests belongs to webserver.
 */

/* WebSocket handshake using the HTTP module's TLS and header policies. */
#include <stdlib.h>
#define MARS_HTTP_INTERNAL_ACCESS
#include "http_internal.h"

static size_t handshake_header(char *data, size_t size, size_t count, void *context)
{
    http_websocket_t *socket = context;
    if (size && count > SIZE_MAX / size)
        return 0;
    size_t bytes = size * count;
    if (bytes > socket->client->limits.max_header_bytes - socket->header_bytes) {
        http_fail(socket->client, HTTP_ERROR_LIMIT, "WebSocket handshake header limit exceeded");
        return 0;
    }
    socket->header_bytes += bytes;
    string_t *line = string_new();
    if (!line || string_append_utf8_exact(line, data, bytes)) {
        string_free(line);
        return 0;
    }
    string_offset_t colon = string_find(line, ":");
    string_t *name = colon > 0 ? string_substr(line, 0, colon) : NULL;
    string_t *key = name ? http_header_name(name) : NULL;
    bool unsupported = key && (string_view_equals_literal(string_view_all(key), "sec-websocket-extensions") ||
                               string_view_equals_literal(string_view_all(key), "sec-websocket-protocol"));
    string_free(line);
    string_free(name);
    string_free(key);
    if (unsupported) {
        http_fail(socket->client, HTTP_ERROR_PROTOCOL, "Unexpected WebSocket extension or subprotocol");
        return 0;
    }
    return bytes;
}

static size_t reject_body(char *data, size_t size, size_t count, void *context)
{
    (void)data;
    (void)size;
    (void)count;
    http_fail(context, HTTP_ERROR_PROTOCOL, "WebSocket upgrade rejected");
    return 0;
}

static int handshake_progress(void *context, curl_off_t a, curl_off_t b, curl_off_t c, curl_off_t d)
{
    (void)a;
    (void)b;
    (void)c;
    (void)d;
    http_client_t *client = context;
    if (!client->cancel || !client->cancel(client->cancel_data))
        return 0;
    http_fail(client, HTTP_ERROR_CANCELLED, "WebSocket connection cancelled");
    return 1;
}

/* Upgrade HTTP(S) to WS(S), reserving the client until the connection is released. */
http_websocket_t *http_websocket_open(http_client_t *client, const http_request_t *request)
{
    if (!client || client->active)
        return NULL;
    http_fail(client, HTTP_ERROR_NONE, NULL);
    if (!request || request->method != HTTP_GET || request->has_body) {
        http_fail(client, HTTP_ERROR_ARGUMENT, "WebSocket requires a GET request without a body");
        return NULL;
    }
    for (size_t i = 0; i < dictionary_size(request->headers); ++i) {
        const string_t *key = *(string_t *const *)dictionary_get_key(request->headers, i);
        if (string_find(key, "sec-websocket-") == 0) {
            http_fail(client, HTTP_ERROR_ARGUMENT, "WebSocket handshake fields are reserved");
            return NULL;
        }
    }
    http_websocket_t *socket = calloc(1, sizeof(*socket));
    CURLU *url = curl_url();
    char *scheme = NULL, *address = NULL;
    bool ok = false;
    if (!socket || !url)
        goto memory;
    socket->client = client;
    socket->easy = curl_easy_init();
    if (!socket->easy)
        goto memory;
    if (curl_url_set(url, CURLUPART_URL, string_c_str(request->url), 0) ||
        curl_url_get(url, CURLUPART_SCHEME, &scheme, 0) ||
        curl_url_set(url, CURLUPART_SCHEME, scheme[4] == 's' ? "wss" : "ws", 0) ||
        curl_url_get(url, CURLUPART_URL, &address, 0)) {
        http_fail(client, HTTP_ERROR_PROTOCOL, "WebSocket protocol is unavailable");
        goto done;
    }
    if (!http_outgoing_headers(client, request, &socket->headers))
        goto done;
    client->active = true;
    CURLcode code = CURLE_OK;
#define SET(option, value)                                                                                             \
    do {                                                                                                               \
        code = curl_easy_setopt(socket->easy, option, value);                                                          \
        if (code)                                                                                                      \
            goto curl_fail;                                                                                            \
    } while (0)
    SET(CURLOPT_URL, address);
    SET(CURLOPT_PROTOCOLS_STR, "ws,wss");
    SET(CURLOPT_CONNECT_ONLY, 2L);
    SET(CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_1_1);
    SET(CURLOPT_FOLLOWLOCATION, 0L);
    SET(CURLOPT_PROXY, "");
    SET(CURLOPT_NOPROXY, "*");
    SET(CURLOPT_NETRC, (long)CURL_NETRC_IGNORED);
    SET(CURLOPT_NOSIGNAL, 1L);
    SET(CURLOPT_CONNECTTIMEOUT_MS, (long)client->limits.connect_timeout_ms);
    SET(CURLOPT_TIMEOUT_MS, (long)client->limits.total_timeout_ms);
    SET(CURLOPT_SSL_VERIFYPEER, 1L);
    SET(CURLOPT_SSL_VERIFYHOST, 2L);
    SET(CURLOPT_SSLVERSION, (long)CURL_SSLVERSION_TLSv1_2);
    SET(CURLOPT_HTTPHEADER, socket->headers);
    SET(CURLOPT_HEADERFUNCTION, handshake_header);
    SET(CURLOPT_HEADERDATA, socket);
    SET(CURLOPT_WRITEFUNCTION, reject_body);
    SET(CURLOPT_WRITEDATA, client);
    SET(CURLOPT_NOPROGRESS, 0L);
    SET(CURLOPT_XFERINFOFUNCTION, handshake_progress);
    SET(CURLOPT_XFERINFODATA, client);
    if (client->ca) {
        struct curl_blob ca = {
            .data = array_get(client->ca, 0), .len = array_size(client->ca), .flags = CURL_BLOB_COPY};
        SET(CURLOPT_CAINFO_BLOB, &ca);
        SET(CURLOPT_CAPATH, NULL);
    }
    http_client_t settings = *client;
    settings.easy = socket->easy;
    settings.cookies = false;
    if (!http_apply_session(&settings)) {
        http_fail(client, HTTP_ERROR_TRANSPORT, "Cannot configure WebSocket TLS identity");
        goto done;
    }
    code = curl_easy_perform(socket->easy);
    if (code)
        goto curl_fail;
    long status = 0;
    code = curl_easy_getinfo(socket->easy, CURLINFO_RESPONSE_CODE, &status);
    if (code)
        goto curl_fail;
    if (status != 101) {
        http_fail(client, HTTP_ERROR_PROTOCOL, "WebSocket upgrade was not accepted");
        goto done;
    }
    socket->open = ok = true;
    goto done;
curl_fail:
    if (client->error == HTTP_ERROR_NONE)
        http_fail(client, code == CURLE_OPERATION_TIMEDOUT ? HTTP_ERROR_TIMEOUT : HTTP_ERROR_TRANSPORT,
                  curl_easy_strerror(code));
    goto done;
memory:
    http_fail(client, HTTP_ERROR_MEMORY, "Cannot allocate WebSocket handshake");
done:
    curl_free(scheme);
    curl_free(address);
    curl_url_cleanup(url);
    if (!ok) {
        if (socket) {
            curl_easy_cleanup(socket->easy);
            curl_slist_free_all(socket->headers);
            free(socket);
        }
        client->active = false;
        return NULL;
    }
    return socket;
#undef SET
}

/* Release the transport and the borrowed client's exclusive reservation. */
void http_websocket_free(http_websocket_t *socket)
{
    if (!socket || socket->busy)
        return;
    curl_easy_cleanup(socket->easy);
    curl_slist_free_all(socket->headers);
    socket->client->active = false;
    free(socket);
}

/* Query whether message exchange is still permitted. */
bool http_websocket_is_open(const http_websocket_t *socket)
{
    return socket && socket->open;
}
