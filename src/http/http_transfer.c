/* http_transfer.c - bounded synchronous libcurl transport and streaming callbacks. */
#include <stdint.h>
#include <stdlib.h>
#define MARS_HTTP_INTERNAL_ACCESS
#include "http_internal.h"

typedef struct {
    http_client_t *client;
    http_response_t *response;
    http_body_fn sink;
    void *sink_data;
    file_t *upload;
    size_t upload_left;
    size_t header_bytes;
} transfer_t;

static size_t receive_body(char *data, size_t size, size_t count, void *context)
{
    transfer_t *transfer = context;
    http_client_t *client = transfer->client;
    if (size && count > SIZE_MAX / size) {
        http_fail(client, HTTP_ERROR_LIMIT, "Response fragment size overflow");
        return 0;
    }
    size_t length = size * count;
    if (length > client->limits.max_body_bytes - transfer->response->received) {
        http_fail(client, HTTP_ERROR_LIMIT, "Response body limit exceeded");
        return 0;
    }
    bool ok = transfer->sink ? transfer->sink(data, length, transfer->sink_data) :
                              array_append_carray(transfer->response->body, data, length);
    if (!ok) {
        if (client->error == HTTP_ERROR_NONE)
            http_fail(client, transfer->sink ? HTTP_ERROR_CANCELLED : HTTP_ERROR_MEMORY,
                      transfer->sink ? "Response sink stopped the transfer" : "Cannot buffer response body");
        return 0;
    }
    transfer->response->received += length;
    return length;
}

static size_t receive_header(char *data, size_t size, size_t count, void *context)
{
    transfer_t *transfer = context;
    http_client_t *client = transfer->client;
    if (size && count > SIZE_MAX / size)
        goto limit;
    size_t length = size * count;
    if (length > client->limits.max_header_bytes - transfer->header_bytes)
        goto limit;
    transfer->header_bytes += length;
    string_t *line = string_new(), *name = NULL, *value = NULL;
    string_cursor_t *cursor = NULL;
    bool ok = line && string_append_utf8_exact(line, data, length) == 0;
    if (!ok)
        goto done;
    cursor = string_cursor_new(line);
    if (!cursor) {
        ok = false;
        goto done;
    }
    if (string_cursor_consume(cursor, "HTTP/")) {
        unsigned char c = 0;
        while (string_cursor_peek_ascii(cursor, &c) && c != ' ')
            string_cursor_next(cursor);
        ok = string_cursor_consume(cursor, " ");
        long status = 0;
        for (size_t i = 0; ok && i < 3; ++i) {
            ok = string_cursor_peek_ascii(cursor, &c) && c >= '0' && c <= '9';
            if (ok) {
                status = status * 10 + c - '0';
                string_cursor_next(cursor);
            }
        }
        ok = ok && status >= 100 && status <= 599 && status != 101 &&
             string_cursor_peek_ascii(cursor, &c) && (c == ' ' || c == '\r' || c == '\n');
        if (ok) {
            dictionary_t *headers = http_headers_new();
            if (!headers)
                ok = false;
            else {
                http_headers_free(transfer->response->headers);
                transfer->response->headers = headers;
                transfer->response->status = status;
            }
        }
    } else if (string_cursor_match(cursor, "\r\n") || string_cursor_match(cursor, "\n")) {
        ok = true;
    } else {
        string_pos_t start = string_cursor_position(cursor);
        unsigned char c;
        while (!string_cursor_done(cursor) && !string_cursor_match(cursor, ":"))
            string_cursor_next(cursor);
        name = string_cursor_extract(start, cursor);
        ok = name && string_cursor_consume(cursor, ":");
        while (ok && string_cursor_peek_ascii(cursor, &c) && (c == ' ' || c == '\t'))
            string_cursor_next(cursor);
        start = string_cursor_position(cursor);
        string_pos_t end = start;
        while (ok && string_cursor_peek_ascii(cursor, &c) && c != '\r' && c != '\n') {
            string_cursor_next(cursor);
            if (c != ' ' && c != '\t')
                end = string_cursor_position(cursor);
        }
        string_view_t span = string_cursor_view_between(start, end, cursor);
        value = string_from_view(&span);
        ok = ok && (string_cursor_consume(cursor, "\r\n") || string_cursor_consume(cursor, "\n")) &&
             string_cursor_done(cursor) && value &&
             http_headers_store(transfer->response->headers, name, value, false);
    }
done:
    string_cursor_free(cursor);
    string_free(line); string_free(name); string_free(value);
    if (!ok) {
        http_fail(client, HTTP_ERROR_PROTOCOL, "Invalid or unrepresentable response header");
        return 0;
    }
    return length;
limit:
    http_fail(client, HTTP_ERROR_LIMIT, "Response header limit exceeded");
    return 0;
}

static size_t send_file(char *data, size_t size, size_t count, void *context)
{
    transfer_t *transfer = context;
    if (size && count > SIZE_MAX / size)
        return CURL_READFUNC_ABORT;
    size_t capacity = size * count;
    if (capacity > transfer->upload_left)
        capacity = transfer->upload_left;
    if (!capacity)
        return 0;
    size_t got = 0;
    if (!file_read(transfer->upload, data, capacity, &got) || !got) {
        http_fail(transfer->client, HTTP_ERROR_IO, "Cannot read the complete upload file");
        return CURL_READFUNC_ABORT;
    }
    transfer->upload_left -= got;
    return got;
}

static int progress(void *context, curl_off_t download_total, curl_off_t downloaded,
                    curl_off_t upload_total, curl_off_t uploaded)
{
    transfer_t *transfer = context;
    http_client_t *client = transfer->client;
    (void)download_total; (void)downloaded; (void)upload_total; (void)uploaded;
    if (client->cancel && client->cancel(client->cancel_data)) {
        http_fail(client, HTTP_ERROR_CANCELLED, "Request cancelled");
        return 1;
    }
    return 0;
}

static bool add_header_line(struct curl_slist **list, const char *text)
{
    struct curl_slist *next = curl_slist_append(*list, text);
    if (!next)
        return false;
    *list = next;
    return true;
}

static bool outgoing_headers(http_client_t *client, const http_request_t *request, struct curl_slist **list)
{
    size_t bytes = 0;
    for (size_t i = 0; i < dictionary_size(request->headers); ++i) {
        string_t *key = *(string_t *const *)dictionary_get_key(request->headers, i);
        array_t *values = *(array_t *const *)dictionary_get_value(request->headers, i);
        string_t *value = *(string_t *const *)array_get(values, 0);
        size_t key_size = string_byte_length(key), value_size = string_byte_length(value);
        if (key_size > client->limits.max_header_bytes - bytes)
            goto limit;
        bytes += key_size;
        if (value_size > client->limits.max_header_bytes - bytes)
            goto limit;
        bytes += value_size;
        if (4 > client->limits.max_header_bytes - bytes)
            goto limit;
        bytes += 4;
        string_t *line = string_byte_length(value) ? string_sprintf("%S: %S", key, value) : string_sprintf("%S;", key);
        bool ok = line && add_header_line(list, string_c_str(line));
        string_free(line);
        if (!ok) {
            http_fail(client, HTTP_ERROR_MEMORY, "Cannot prepare request headers");
            return false;
        }
    }
    if (!add_header_line(list, "Expect:")) {
        http_fail(client, HTTP_ERROR_MEMORY, "Cannot prepare request headers");
        return false;
    }
    return true;
limit:
    http_fail(client, HTTP_ERROR_LIMIT, "Request header limit exceeded");
    return false;
}

static http_response_t *perform(http_client_t *client, const http_request_t *request,
                                http_body_fn sink, void *sink_data)
{
    if (!client || !request || client->active || !client->easy) {
        if (client && !client->active)
            http_fail(client, HTTP_ERROR_ARGUMENT, "Invalid client or request");
        return NULL;
    }
    client->active = true;
    http_fail(client, HTTP_ERROR_NONE, NULL);
    http_response_t *response = calloc(1, sizeof(*response));
    transfer_t transfer = {.client = client, .response = response, .sink = sink, .sink_data = sink_data};
    struct curl_slist *headers = NULL;
    bool ok = false;
    if (!response)
        goto memory;
    response->streamed = sink != NULL;
    response->headers = http_headers_new();
    if (!sink)
        response->body = array_create(1, NULL, NULL);
    if (!response->headers || (!sink && !response->body))
        goto memory;
    if (progress(&transfer, 0, 0, 0, 0))
        goto done;
    size_t upload_size = request->body_size;
    if (request->body_path) {
        int64_t length = 0;
        transfer.upload = file_new(request->body_path);
        if (!transfer.upload || !file_open_read(transfer.upload) ||
            !file_seek(transfer.upload, 0, FILE_SEEK_END) || !file_tell(transfer.upload, &length) ||
            length < 0 || (uint64_t)length > SIZE_MAX || !file_seek(transfer.upload, 0, FILE_SEEK_BEGIN)) {
            http_fail(client, HTTP_ERROR_IO, "Cannot open or size upload file");
            goto done;
        }
        upload_size = (size_t)length;
        transfer.upload_left = upload_size;
    }
    if (upload_size > client->limits.max_upload_bytes || upload_size > INT64_MAX) {
        http_fail(client, HTTP_ERROR_LIMIT, "Request body limit exceeded");
        goto done;
    }
    if (!outgoing_headers(client, request, &headers))
        goto done;
    curl_easy_reset(client->easy);
    CURLcode code = CURLE_OK;
#define SET(option, value) do { \
    code = curl_easy_setopt(client->easy, option, value); \
    if (code != CURLE_OK) goto curl_fail; \
} while (0)
    SET(CURLOPT_URL, string_c_str(request->url));
    SET(CURLOPT_PROTOCOLS_STR, "http,https");
    SET(CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
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
    SET(CURLOPT_HTTPHEADER, headers);
    SET(CURLOPT_ACCEPT_ENCODING, "");
    SET(CURLOPT_USERAGENT, "MARS-http/1");
    SET(CURLOPT_WRITEFUNCTION, receive_body);
    SET(CURLOPT_WRITEDATA, &transfer);
    SET(CURLOPT_HEADERFUNCTION, receive_header);
    SET(CURLOPT_HEADERDATA, &transfer);
    SET(CURLOPT_NOPROGRESS, 0L);
    SET(CURLOPT_XFERINFOFUNCTION, progress);
    SET(CURLOPT_XFERINFODATA, &transfer);
    if (client->ca) {
        struct curl_blob ca = {.data = array_get(client->ca, 0), .len = array_size(client->ca), .flags = CURL_BLOB_COPY};
        SET(CURLOPT_CAINFO_BLOB, &ca);
        SET(CURLOPT_CAPATH, NULL);
    }
    if (transfer.upload) {
        SET(CURLOPT_UPLOAD, 1L);
        SET(CURLOPT_INFILESIZE_LARGE, (curl_off_t)upload_size);
        SET(CURLOPT_READFUNCTION, send_file);
        SET(CURLOPT_READDATA, &transfer);
    } else if (request->has_body || request->method == HTTP_POST) {
        SET(CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)upload_size);
        SET(CURLOPT_POSTFIELDS, request->body ? (const char *)request->body : "");
    }
    if (request->method == HTTP_HEAD)
        SET(CURLOPT_NOBODY, 1L);
    static const char *const methods[] = {
        [HTTP_GET]     = "GET",
        [HTTP_POST]    = "POST",
        [HTTP_PUT]     = "PUT",
        [HTTP_PATCH]   = "PATCH",
        [HTTP_DELETE]  = "DELETE",
        [HTTP_HEAD]    = "HEAD",
        [HTTP_OPTIONS] = "OPTIONS"
    };
    SET(CURLOPT_CUSTOMREQUEST, methods[request->method]);
    code = curl_easy_perform(client->easy);
    if (code != CURLE_OK)
        goto curl_fail;
    code = curl_easy_getinfo(client->easy, CURLINFO_RESPONSE_CODE, &response->status);
    if (code != CURLE_OK)
        goto curl_fail;
    if (response->status < 200 || response->status > 599) {
        http_fail(client, HTTP_ERROR_PROTOCOL, "No final HTTP response");
        goto done;
    }
    ok = true;
    goto done;
curl_fail:
    if (client->error == HTTP_ERROR_NONE)
        http_fail(client, code == CURLE_OPERATION_TIMEDOUT ? HTTP_ERROR_TIMEOUT :
                          code == CURLE_OUT_OF_MEMORY ? HTTP_ERROR_MEMORY : HTTP_ERROR_TRANSPORT,
                  curl_easy_strerror(code));
    goto done;
memory:
    http_fail(client, HTTP_ERROR_MEMORY, "Cannot allocate response");
done:
    if (transfer.upload && file_is_open(transfer.upload) && !file_close(transfer.upload)) {
        http_fail(client, HTTP_ERROR_IO, "Cannot close upload file");
        ok = false;
    }
    file_free(transfer.upload);
    /* Clear borrowed callback pointers before releasing stack state and request headers. */
    curl_easy_reset(client->easy);
    curl_slist_free_all(headers);
    client->active = false;
    if (!ok) {
        http_response_free(response);
        return NULL;
    }
    return response;
#undef SET
}

/* Send synchronously, retaining the bounded response body. */
http_response_t *http_client_send(http_client_t *client, const http_request_t *request)
{
    return perform(client, request, NULL, NULL);
}

/* Send synchronously to a borrowed callback sink. */
http_response_t *http_client_stream(http_client_t *client, const http_request_t *request,
                                    http_body_fn callback, void *user_data)
{
    if (!callback) {
        if (client && !client->active)
            http_fail(client, HTTP_ERROR_ARGUMENT, "Missing response sink");
        return NULL;
    }
    return perform(client, request, callback, user_data);
}

typedef struct { http_client_t *client; file_t *file; } file_sink_t;
static bool download_sink(const void *data, size_t size, void *context)
{
    file_sink_t *sink = context;
    size_t written = 0;
    if (file_write(sink->file, data, size, &written) && written == size)
        return true;
    http_fail(sink->client, HTTP_ERROR_IO, "Cannot write download file");
    return false;
}

/* Stream to an already-open file without taking ownership or closing it. */
http_response_t *http_client_download(http_client_t *client, const http_request_t *request, file_t *destination)
{
    if (!destination || !file_is_open(destination)) {
        if (client && !client->active)
            http_fail(client, HTTP_ERROR_ARGUMENT, "Destination file is not open");
        return NULL;
    }
    file_sink_t sink = {client, destination};
    return http_client_stream(client, request, download_sink, &sink);
}
