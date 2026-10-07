/**
 * @file http_redirect.c
 * @brief Bounded same-origin HTTP redirects.
 *
 * Resolves redirect targets and compares origins before replaying permitted requests. This policy deliberately
 * excludes unsafe method replay and unrestricted cross-origin credential forwarding.
 *
 * This is part of the synchronous http.h client. Keep transport limits, TLS policy and handle ownership consistent
 * with the shared client machinery; serving requests belongs to webserver.
 */

/* Bounded same-origin redirects; unsafe method replay is deliberately excluded. */
#include <string.h>
#include <strings.h>
#include <time.h>
#define MARS_HTTP_INTERNAL_ACCESS
#include "http_internal.h"

static uint64_t milliseconds(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now))
        return 0;
    return (uint64_t)now.tv_sec * 1000 + (uint64_t)now.tv_nsec / 1000000;
}

static bool same_part(CURLU *a, CURLU *b, CURLUPart part, unsigned flags)
{
    char *left = NULL, *right = NULL;
    bool ok = !curl_url_get(a, part, &left, flags) && !curl_url_get(b, part, &right, flags) && !strcasecmp(left, right);
    curl_free(left);
    curl_free(right);
    return ok;
}

static string_t *resolve(const string_t *base, const string_t *location)
{
    if (!http_ascii_text(location, false) || !string_byte_length(location) || string_find(location, "\\") >= 0)
        return NULL;
    CURLU *a = curl_url(), *b = curl_url();
    char *url = NULL;
    bool ok = a && b && !curl_url_set(a, CURLUPART_URL, string_c_str(base), 0) &&
              !curl_url_set(b, CURLUPART_URL, string_c_str(base), 0) &&
              !curl_url_set(b, CURLUPART_URL, string_c_str(location), 0) && same_part(a, b, CURLUPART_SCHEME, 0) &&
              same_part(a, b, CURLUPART_HOST, 0) && same_part(a, b, CURLUPART_PORT, CURLU_DEFAULT_PORT) &&
              !curl_url_set(b, CURLUPART_FRAGMENT, NULL, 0) && !curl_url_get(b, CURLUPART_URL, &url, 0);
    string_t *result = ok ? string_new_with(url) : NULL;
    /* The ordinary constructor also rejects embedded user information. */
    http_request_t *probe = result ? http_request_new(HTTP_GET, result) : NULL;
    if (!probe) {
        string_free(result);
        result = NULL;
    }
    http_request_free(probe);
    curl_free(url);
    curl_url_cleanup(a);
    curl_url_cleanup(b);
    return result;
}

static bool redirect(long status)
{
    return status == 301 || status == 302 || status == 303 || status == 307 || status == 308;
}

http_response_t *http_follow_redirects(http_client_t *client, const http_request_t *request)
{
    if (!client || !request || client->active || !client->redirects ||
        (request->method != HTTP_GET && request->method != HTTP_HEAD))
        return http_perform_once(client, request, NULL, NULL);
    http_limits_t original = client->limits;
    uint64_t start = milliseconds();
    http_request_t current = *request;
    string_t *url = string_clone(request->url), *location_name = string_new_with("Location");
    http_response_t *response = NULL;
    size_t body_left = original.max_body_bytes, headers_left = original.max_header_bytes;
    if (!url || !location_name) {
        http_fail(client, HTTP_ERROR_MEMORY, "Cannot prepare redirects");
        goto done;
    }
    for (unsigned hop = 0;; ++hop) {
        uint64_t elapsed = milliseconds() - start;
        if (elapsed >= original.total_timeout_ms) {
            http_fail(client, HTTP_ERROR_TIMEOUT, "Redirect deadline exceeded");
            break;
        }
        client->limits.total_timeout_ms = original.total_timeout_ms - elapsed;
        client->limits.max_body_bytes = body_left;
        client->limits.max_header_bytes = headers_left;
        current.url = url;
        response = http_perform_once(client, &current, NULL, NULL);
        if (!response || !redirect(response->status))
            break;
        size_t count = http_response_header_count(response, location_name);
        if (!count)
            break;
        string_t *next = count == 1 ? resolve(url, http_response_header_at(response, location_name, 0)) : NULL;
        body_left -= response->received;
        headers_left -= response->header_bytes;
        http_response_free(response);
        response = NULL;
        if (!next) {
            http_fail(client, HTTP_ERROR_PROTOCOL, "Invalid or cross-origin redirect");
            break;
        }
        if (hop == client->redirects) {
            string_free(next);
            http_fail(client, HTTP_ERROR_LIMIT, "Redirect hop limit exceeded");
            break;
        }
        string_free(url);
        url = next;
    }
done:
    client->limits = original;
    string_free(url);
    string_free(location_name);
    return response;
}
