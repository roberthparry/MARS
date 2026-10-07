/**
 * @file http_auth.c
 * @brief Explicit credentials, signatures and OAuth token helpers.
 *
 * Prepares supported authentication data and token requests without taking over application token policy.
 * Client-credentials and refresh-token exchanges are supported; interactive browser authorisation remains outside
 * this unit.
 *
 * This is part of the synchronous http.h client. Keep transport limits, TLS policy and handle ownership consistent
 * with the shared client machinery; serving requests belongs to webserver.
 */

/* Explicit HTTP authentication; token lifecycle belongs to the application. */
#include <arpa/inet.h>
#include <sodium.h>
#include <stdlib.h>
#include <string.h>
#define MARS_HTTP_INTERNAL_ACCESS
#include "http_internal.h"

static bool no_nul(const string_t *text)
{
    return text && !memchr(string_c_str(text), 0, string_byte_length(text));
}

static string_t *base64(const void *bytes, size_t size)
{
    if (size > (SIZE_MAX - 16) / 4)
        return NULL;
    size_t capacity = sodium_base64_ENCODED_LEN(size, sodium_base64_VARIANT_ORIGINAL);
    char *buffer = malloc(capacity);
    if (!buffer)
        return NULL;
    sodium_bin2base64(buffer, capacity, bytes, size, sodium_base64_VARIANT_ORIGINAL);
    string_t *result = string_new_with(buffer);
    sodium_memzero(buffer, capacity);
    free(buffer);
    return result;
}

/* Copy an explicit Basic header, avoiding hidden credential negotiation. */
bool http_request_set_basic(http_request_t *request, const string_t *username, const string_t *password)
{
    if (!request || !no_nul(username) || !no_nul(password) || string_find(username, ":") >= 0)
        return false;
    size_t user_size = string_byte_length(username), password_size = string_byte_length(password);
    if (password_size >= SIZE_MAX - user_size)
        return false;
    size_t size = user_size + password_size + 1;
    unsigned char *plain = malloc(size);
    if (!plain)
        return false;
    memcpy(plain, string_c_str(username), user_size);
    plain[user_size] = ':';
    memcpy(plain + user_size + 1, string_c_str(password), password_size);
    string_t *encoded = base64(plain, size);
    string_t *value = encoded ? string_sprintf("Basic %S", encoded) : NULL;
    string_t *name = string_new_with("Authorization");
    bool ok = value && name && http_request_set_header(request, name, value);
    sodium_memzero(plain, size);
    free(plain);
    string_free(encoded);
    string_free(value);
    string_free(name);
    return ok;
}

/* Sign exact caller-supplied canonical bytes, not a guessed request representation. */
string_t *http_hmac_sha256(const string_t *canonical, const void *key, size_t key_size)
{
    if (!canonical || !key || !key_size || sodium_init() < 0)
        return NULL;
    crypto_auth_hmacsha256_state state;
    unsigned char digest[crypto_auth_hmacsha256_BYTES];
    crypto_auth_hmacsha256_init(&state, key, key_size);
    crypto_auth_hmacsha256_update(&state, (const unsigned char *)string_c_str(canonical),
                                  string_byte_length(canonical));
    crypto_auth_hmacsha256_final(&state, digest);
    string_t *result = base64(digest, sizeof(digest));
    sodium_memzero(&state, sizeof(state));
    sodium_memzero(digest, sizeof(digest));
    return result;
}

bool http_secure_endpoint(const string_t *url)
{
    http_request_t *probe = http_request_new(HTTP_POST, url);
    if (!probe)
        return false;
    CURLU *parsed = curl_url();
    char *scheme = NULL, *host = NULL;
    bool ok = parsed && !curl_url_set(parsed, CURLUPART_URL, string_c_str(url), 0) &&
              !curl_url_get(parsed, CURLUPART_SCHEME, &scheme, 0) && !curl_url_get(parsed, CURLUPART_HOST, &host, 0);
    if (ok && strcmp(scheme, "https")) {
        struct in_addr ipv4;
        ok = !strcmp(scheme, "http") &&
             ((!strcmp(host, "[::1]")) || (inet_pton(AF_INET, host, &ipv4) == 1 && (ntohl(ipv4.s_addr) >> 24) == 127));
    }
    curl_free(scheme);
    curl_free(host);
    curl_url_cleanup(parsed);
    http_request_free(probe);
    return ok;
}

static bool field(http_form_t *form, const char *name, const string_t *value)
{
    string_t *key = string_new_with(name);
    bool ok = key && value && http_form_add_text(form, key, value);
    string_free(key);
    return ok;
}

/* OAuth Basic credentials require form encoding before joining with a colon. */
static string_t *credential(const string_t *value)
{
    return no_nul(value) ? http_form_component(value) : NULL;
}

static http_response_t *token(http_client_t *client, const string_t *url, const string_t *id, const string_t *secret,
                              const string_t *refresh, const string_t *scope)
{
    if (!client || client->active)
        return NULL;
    if (!http_secure_endpoint(url) || !no_nul(id) || !no_nul(secret) || (scope && !no_nul(scope)) ||
        (refresh && !no_nul(refresh))) {
        http_fail(client, HTTP_ERROR_ARGUMENT, "Invalid OAuth credentials or insecure endpoint");
        return NULL;
    }
    http_request_t *request = http_request_new(HTTP_POST, url);
    http_form_t *form = http_form_new();
    string_t *grant = string_new_with(refresh ? "refresh_token" : "client_credentials");
    string_t *user = credential(id), *password = credential(secret);
    bool ok = request && form && grant && user && password && field(form, "grant_type", grant) &&
              (!refresh || field(form, "refresh_token", refresh)) && (!scope || field(form, "scope", scope)) &&
              http_request_set_basic(request, user, password) && http_request_set_form(request, form);
    if (!ok)
        http_fail(client, HTTP_ERROR_MEMORY, "Cannot prepare OAuth request");
    http_response_t *response = ok ? http_client_send(client, request) : NULL;
    string_free(grant);
    string_free(user);
    string_free(password);
    http_form_free(form);
    http_request_free(request);
    return response;
}

/* Obtain a token response without persisting credentials or scheduling renewals. */
http_response_t *http_oauth2_client_credentials(http_client_t *client, const string_t *url, const string_t *client_id,
                                                const string_t *secret, const string_t *scope)
{
    return token(client, url, client_id, secret, NULL, scope);
}

/* Exchange an explicitly supplied refresh token. */
http_response_t *http_oauth2_refresh(http_client_t *client, const string_t *url, const string_t *client_id,
                                     const string_t *secret, const string_t *refresh_token, const string_t *scope)
{
    if (!refresh_token) {
        if (client && !client->active)
            http_fail(client, HTTP_ERROR_ARGUMENT, "Missing refresh token");
        return NULL;
    }
    return token(client, url, client_id, secret, refresh_token, scope);
}
