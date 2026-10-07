/**
 * @file http_core.c
 * @brief HTTP client, request and response lifecycle.
 *
 * Initialises reusable libcurl clients, manages opaque request and response storage and exposes common
 * configuration and accessors. Protocol-specific operations share these handles rather than allocate independent
 * transport APIs.
 *
 * This is part of the synchronous http.h client. Keep transport limits, TLS policy and handle ownership consistent
 * with the shared client machinery; serving requests belongs to webserver.
 */

/* http_core.c - clients, requests and response accessors. */
#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define MARS_HTTP_INTERNAL_ACCESS
#include "http_internal.h"

static pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER;
static size_t global_users;

bool http_global_acquire(void)
{
    bool ok = true;
    pthread_mutex_lock(&global_lock);
    if (!global_users) {
        ok = curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
        if (ok) {
            const curl_version_info_data *info = curl_version_info(CURLVERSION_NOW);
            ok = info && info->version_num >= 0x075600 && (info->features & CURL_VERSION_THREADSAFE);
            if (!ok)
                curl_global_cleanup();
        }
    }
    if (ok)
        ++global_users;
    pthread_mutex_unlock(&global_lock);
    return ok;
}

void http_global_release(void)
{
    pthread_mutex_lock(&global_lock);
    if (global_users && !--global_users)
        curl_global_cleanup();
    pthread_mutex_unlock(&global_lock);
}

void http_fail(http_client_t *client, http_error_t code, const char *message)
{
    if (!client)
        return;
    client->error = code;
    string_free(client->message);
    client->message = message ? string_new_with(message) : NULL;
}

/* Create a client with a persistent connection cache and bounded defaults. */
http_client_t *http_client_new(void)
{
    if (!http_global_acquire())
        return NULL;
    http_client_t *client = calloc(1, sizeof(*client));
    if (!client || !(client->easy = curl_easy_init())) {
        free(client);
        http_global_release();
        return NULL;
    }
    http_limits_t defaults = {0};
    http_client_set_limits(client, &defaults);
    return client;
}

/* Release the idle client and its reference to the transport runtime. */
void http_client_free(http_client_t *client)
{
    if (!client)
        return;
    curl_easy_cleanup(client->easy);
    array_destroy(client->ca);
    array_destroy(client->certificate);
    http_secret_free(client->private_key);
    string_free(client->key_password);
    string_free(client->message);
    free(client);
    http_global_release();
}

/* Apply validated timeout and byte budgets atomically. */
bool http_client_set_limits(http_client_t *client, const http_limits_t *limits)
{
    if (!client || client->active || !limits || limits->connect_timeout_ms > LONG_MAX ||
        limits->total_timeout_ms > LONG_MAX)
        return false;
    client->limits = *limits;
    if (!client->limits.connect_timeout_ms) client->limits.connect_timeout_ms = 10000;
    if (!client->limits.total_timeout_ms) client->limits.total_timeout_ms = 30000;
    if (!client->limits.max_body_bytes) client->limits.max_body_bytes = 16u * 1024u * 1024u;
    if (!client->limits.max_header_bytes) client->limits.max_header_bytes = 64u * 1024u;
    if (!client->limits.max_upload_bytes) client->limits.max_upload_bytes = 16u * 1024u * 1024u;
    return true;
}

/* Read a bounded custom CA bundle through file_t, retaining verified HTTPS. */
bool http_client_set_ca_file(http_client_t *client, const string_t *path)
{
    if (!client || client->active)
        return false;
    if (!path) {
        CURL *fresh = curl_easy_init();
        if (!fresh)
            return false;
        curl_easy_cleanup(client->easy);
        client->easy = fresh;
        array_destroy(client->ca);
        client->ca = NULL;
        return true;
    }
    array_t *bytes = http_read_secret_file(path);
    if (!bytes)
        return false;
    /* Do not reuse a TLS connection established under a different trust bundle. */
    CURL *fresh = curl_easy_init();
    if (!fresh) {
        array_destroy(bytes);
        return false;
    }
    array_destroy(client->ca);
    client->ca = bytes;
    curl_easy_cleanup(client->easy);
    client->easy = fresh;
    return true;
}

/* Set a cooperative cancellation hook. */
void http_client_set_cancel(http_client_t *client, http_cancel_fn callback, void *user_data)
{
    if (client && !client->active) {
        client->cancel = callback;
        client->cancel_data = user_data;
    }
}

/* Read the most recent transport error. */
http_error_t http_client_error(const http_client_t *client)
{
    return client ? client->error : HTTP_ERROR_ARGUMENT;
}

/* Borrow a transport diagnostic without credentials. */
const string_t *http_client_error_text(const http_client_t *client) { return client ? client->message : NULL; }

static bool valid_url(const string_t *url)
{
    if (!http_ascii_text(url, false) || !string_byte_length(url) || string_find(url, "\\") >= 0 ||
        string_find(url, "#") >= 0 || !http_global_acquire())
        return false;
    CURLU *parsed = curl_url();
    char *scheme = NULL, *host = NULL, *user = NULL, *password = NULL;
    bool ok = parsed && curl_url_set(parsed, CURLUPART_URL, string_c_str(url), CURLU_DISALLOW_USER) == CURLUE_OK &&
              curl_url_get(parsed, CURLUPART_SCHEME, &scheme, 0) == CURLUE_OK &&
              curl_url_get(parsed, CURLUPART_HOST, &host, 0) == CURLUE_OK &&
              (!strcmp(scheme, "http") || !strcmp(scheme, "https")) && host && *host;
    if (ok) {
        ok = curl_url_get(parsed, CURLUPART_USER, &user, 0) != CURLUE_OK &&
             curl_url_get(parsed, CURLUPART_PASSWORD, &password, 0) != CURLUE_OK;
    }
    curl_free(scheme); curl_free(host); curl_free(user); curl_free(password);
    curl_url_cleanup(parsed);
    http_global_release();
    return ok;
}

/* Copy and validate an absolute transport URL. */
http_request_t *http_request_new(webmethod_t method, const string_t *url)
{
    if (method < HTTP_GET || method > HTTP_OPTIONS || !valid_url(url))
        return NULL;
    http_request_t *request = calloc(1, sizeof(*request));
    if (!request)
        return NULL;
    request->method = method;
    request->url = string_clone(url);
    request->headers = http_headers_new();
    if (!request->url || !request->headers) {
        http_request_free(request);
        return NULL;
    }
    return request;
}

/* Destroy a request and every copied input. */
void http_request_free(http_request_t *request)
{
    if (!request)
        return;
    string_free(request->url);
    http_headers_free(request->headers);
    free(request->body);
    string_free(request->body_path);
    free(request);
}

/* Validate a user header while protecting transport framing and proxy credentials. */
bool http_request_set_header(http_request_t *request, const string_t *name, const string_t *value)
{
    string_t *key = http_header_name(name);
    bool ok = request && key && http_header_value(value);
    /* Fixed protocol-reserved set; bounded independent of request size. */
    static const char *const reserved[] = {
        "host", "content-length", "transfer-encoding", "connection", "expect", "proxy-authorization",
        "proxy-connection", "te", "trailer", "upgrade"
    };
    for (size_t i = 0; ok && i < sizeof(reserved) / sizeof(reserved[0]); ++i)
        if (string_view_equals_literal(string_view_all(key), reserved[i]))
            ok = false;
    if (ok)
        ok = http_headers_store(request->headers, key, value, true);
    string_free(key);
    return ok;
}

/* Copy an explicit bearer token without enabling automatic authentication. */
bool http_request_set_bearer(http_request_t *request, const string_t *token)
{
    if (!http_ascii_text(token, false) || !string_byte_length(token))
        return false;
    string_t *key = string_new_with("Authorization"), *value = string_sprintf("Bearer %S", token);
    bool ok = key && value && http_request_set_header(request, key, value);
    string_free(key); string_free(value);
    return ok;
}

static bool set_content_type(http_request_t *request, const string_t *type)
{
    if (!type)
        return true;
    string_t *key = string_new_with("Content-Type");
    bool ok = key && http_request_set_header(request, key, type);
    string_free(key);
    return ok;
}

/* Replace a copied binary body, preserving the old body if preparation fails. */
bool http_request_set_body(http_request_t *request, const void *data, size_t size, const string_t *content_type)
{
    if (!request || request->method == HTTP_GET || request->method == HTTP_HEAD || (!data && size))
        return false;
    unsigned char *copy = size ? malloc(size) : NULL;
    if (size && !copy)
        return false;
    if (size)
        memcpy(copy, data, size);
    if (!set_content_type(request, content_type)) {
        free(copy);
        return false;
    }
    free(request->body);
    string_free(request->body_path);
    request->body = copy;
    request->body_size = size;
    request->body_path = NULL;
    request->has_body = true;
    return true;
}

/* Copy the exact encoded string bytes. */
bool http_request_set_text(http_request_t *request, const string_t *text, const string_t *content_type)
{
    return text && http_request_set_body(request, string_c_str(text), string_byte_length(text), content_type);
}

/* Select an incremental file upload, opening the path only during a transfer. */
bool http_request_set_body_file(http_request_t *request, const string_t *path, const string_t *content_type)
{
    if (!request || request->method == HTTP_GET || request->method == HTTP_HEAD || !path)
        return false;
    file_t *probe = file_new(path);
    string_t *copy = probe ? string_clone(path) : NULL;
    file_free(probe);
    if (!copy)
        return false;
    if (!set_content_type(request, content_type)) {
        string_free(copy);
        return false;
    }
    free(request->body);
    string_free(request->body_path);
    request->body_path = copy;
    request->body = NULL;
    request->body_size = 0;
    request->has_body = true;
    return true;
}

/* Destroy a complete response independently of its originating client. */
void http_response_free(http_response_t *response)
{
    if (!response)
        return;
    http_headers_free(response->headers);
    array_destroy(response->body);
    free(response);
}

/* Read the final HTTP code. */
long http_response_status(const http_response_t *response) { return response ? response->status : 0; }
/* Classify HTTP status, not transport completion. */
bool http_response_ok(const http_response_t *response)
{
    return response && response->status >= 200 && response->status < 300;
}
/* Borrow buffered binary bytes. */
const void *http_response_body(const http_response_t *response)
{
    return response && response->body ? array_get(response->body, 0) : NULL;
}
/* Count buffered binary bytes. */
size_t http_response_body_size(const http_response_t *response)
{
    return response && response->body ? array_size(response->body) : 0;
}
/* Count bytes delivered to a buffer or sink. */
size_t http_response_received_size(const http_response_t *response) { return response ? response->received : 0; }
/* Expose a read-only, duplicate-preserving header dictionary. */
const dictionary_t *http_response_headers(const http_response_t *response) { return response ? response->headers : NULL; }
/* Count one case-insensitive field's values. */
size_t http_response_header_count(const http_response_t *response, const string_t *name)
{
    const array_t *values = response ? http_headers_get(response->headers, name) : NULL;
    return values ? array_size(values) : 0;
}
/* Borrow one repeated field value. */
const string_t *http_response_header_at(const http_response_t *response, const string_t *name, size_t index)
{
    const array_t *values = response ? http_headers_get(response->headers, name) : NULL;
    string_t *const *value = values ? array_get(values, index) : NULL;
    return value ? *value : NULL;
}

/* Encode one URL component; slash and other separators are escaped as data. */
string_t *http_url_encode(const string_t *text)
{
    if (!text)
        return NULL;
    string_t *out = string_new();
    const unsigned char *bytes = (const unsigned char *)string_c_str(text);
    static const char hex[] = "0123456789ABCDEF";
    if (!out)
        return NULL;
    for (size_t i = 0; i < string_byte_length(text); ++i) {
        unsigned char c = bytes[i];
        bool plain = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                     c == '-' || c == '.' || c == '_' || c == '~';
        char escaped[3] = {'%', hex[c >> 4], hex[c & 15]};
        if (plain ? string_append_char(out, (char)c) != 0 : string_append_utf8_exact(out, escaped, 3) != 0) {
            string_free(out);
            return NULL;
        }
    }
    return out;
}
