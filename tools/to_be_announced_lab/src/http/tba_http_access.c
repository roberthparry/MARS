/**
 * @file tba_http_access.c
 * @brief Private-network peer, Host and Origin checks for forecasting routes.
 *
 * Uses the same access policy as native MARS Lab. Forwarded addresses never
 * replace the actual socket peer. Tailscale checks are read-only and fail closed.
 */
#include <arpa/inet.h>
#include <unistd.h>

#include "lab_mobile.h"
#include "tba_http.h"

static bool tba_http_equal(const string_t *text, const char *literal)
{
    string_t *other = string_new_with(literal);
    bool result = text && other && string_compare(text, other) == 0;
    string_free(other);
    return result;
}

static const string_t *tba_http_request_header(const websrv_request_t *request, const char *name)
{
    string_t *key = string_new_with(name);
    const string_t *value = key ? websrv_request_header(request, key) : NULL;
    string_free(key);
    return value;
}

static bool tba_http_private_v4(const struct in_addr *address)
{
    uint32_t ip = ntohl(address->s_addr);
    return (ip >> 24) == 127 || (ip >> 24) == 10 || (ip >> 20) == 0xac1 || (ip >> 16) == 0xc0a8 ||
           (ip >> 16) == 0xa9fe || (ip >> 22) == 0x191;
}

static bool tba_http_private_address(const string_t *text)
{
    if (!text)
        return false;
    struct in_addr v4;
    struct in6_addr v6;
    if (inet_pton(AF_INET, string_c_str(text), &v4) == 1)
        return tba_http_private_v4(&v4);
    if (inet_pton(AF_INET6, string_c_str(text), &v6) != 1)
        return false;
    if (IN6_IS_ADDR_V4MAPPED(&v6)) {
        /* Copy the mapped address without alignment assumptions. */
        unsigned char *bytes = (unsigned char *)&v4;
        for (size_t i = 0; i < sizeof(v4); ++i)
            bytes[i] = v6.s6_addr[12 + i];
        return tba_http_private_v4(&v4);
    }
    return IN6_IS_ADDR_LOOPBACK(&v6) || IN6_IS_ADDR_LINKLOCAL(&v6) || (v6.s6_addr[0] & 0xfe) == 0xfc;
}

static bool tba_http_allowed_host(const string_t *host)
{
    if (!host)
        return false;
    string_t *name = NULL;
    if (string_find(host, "[") == 0) {
        string_offset_t end = string_find(host, "]");
        if (end > 0)
            name = string_substring(host, 1, (size_t)end - 1);
    } else {
        string_offset_t end = string_find(host, ":");
        name = string_substring(host, 0, end < 0 ? string_length(host) : (size_t)end);
    }
    char hostname[256] = {0};
    bool local = tba_http_equal(name, "localhost") || tba_http_private_address(name);
    if (!local && gethostname(hostname, sizeof(hostname) - 1) == 0) {
        string_t *mdns = string_sprintf("%s.local", hostname);
        local = tba_http_equal(name, hostname) || (mdns && name && string_compare(name, mdns) == 0);
        string_free(mdns);
    }
    if (!local)
        local = lab_mobile_host_allowed(host);
    string_free(name);
    return local;
}

/* Authorise transport identity before any state or calculation route runs. */
bool tba_http_permitted(const websrv_request_t *request)
{
    const string_t *host = tba_http_request_header(request, "Host");
    if (!tba_http_private_address(websrv_request_peer(request)) || !tba_http_allowed_host(host))
        return false;
    const string_t *origin = tba_http_request_header(request, "Origin");
    if (!origin)
        return true;
    string_t *http = string_sprintf("http://%S", host), *https = string_sprintf("https://%S", host);
    bool ok = http && https && (!string_compare(origin, http) || !string_compare(origin, https));
    string_free(http);
    string_free(https);
    return ok;
}
