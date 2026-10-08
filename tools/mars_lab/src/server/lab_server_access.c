/**
 * @file lab_server_access.c
 * @brief Native Lab socket-peer, Host and browser-Origin access checks.
 *
 * Private to server/. Forwarded addresses never override the socket peer.
 * Only verified local or tailnet hosts pass; routing and responses live in lab_server.c.
 */
#include <arpa/inet.h>
#include <unistd.h>

#include "lab_mobile.h"
#include "lab_server_internal.h"

static bool lab_svr_equal(const string_t *text, const char *literal)
{
    string_t *other = string_new_with(literal);
    bool result = text && other && string_compare(text, other) == 0;
    string_free(other);
    return result;
}

static const string_t *lab_svr_request_header(const websrv_request_t *request, const char *name)
{
    string_t *key = string_new_with(name);
    const string_t *value = key ? websrv_request_header(request, key) : NULL;
    string_free(key);
    return value;
}

static bool lab_svr_private_v4(const struct in_addr *address)
{
    uint32_t ip = ntohl(address->s_addr);
    return (ip >> 24) == 127 || (ip >> 24) == 10 || (ip >> 20) == 0xac1 || (ip >> 16) == 0xc0a8 ||
           (ip >> 16) == 0xa9fe || (ip >> 22) == 0x191;
}

static bool lab_svr_private_address(const string_t *text)
{
    if (!text)
        return false;
    struct in_addr v4;
    struct in6_addr v6;
    if (inet_pton(AF_INET, string_c_str(text), &v4) == 1)
        return lab_svr_private_v4(&v4);
    if (inet_pton(AF_INET6, string_c_str(text), &v6) != 1)
        return false;
    if (IN6_IS_ADDR_V4MAPPED(&v6)) {
        /* Copy the mapped address without alignment assumptions. */
        unsigned char *bytes = (unsigned char *)&v4;
        for (size_t i = 0; i < sizeof(v4); ++i)
            bytes[i] = v6.s6_addr[12 + i];
        return lab_svr_private_v4(&v4);
    }
    return IN6_IS_ADDR_LOOPBACK(&v6) || IN6_IS_ADDR_LINKLOCAL(&v6) || (v6.s6_addr[0] & 0xfe) == 0xfc;
}

static bool lab_svr_allowed_host(const string_t *host)
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
    bool local = lab_svr_equal(name, "localhost") || lab_svr_private_address(name);
    if (!local && gethostname(hostname, sizeof(hostname) - 1) == 0) {
        string_t *mdns = string_sprintf("%s.local", hostname);
        local = lab_svr_equal(name, hostname) || (mdns && name && string_compare(name, mdns) == 0);
        string_free(mdns);
    }
    if (!local)
        local = lab_mobile_host_allowed(host);
    string_free(name);
    return local;
}

/* Authorise transport identity before any state or calculation route runs. */
bool lab_svr_request_permitted(const websrv_request_t *request)
{
    const string_t *host = lab_svr_request_header(request, "Host");
    if (!lab_svr_private_address(websrv_request_peer(request)) || !lab_svr_allowed_host(host))
        return false;
    const string_t *origin = lab_svr_request_header(request, "Origin");
    if (!origin)
        return true;
    string_t *http = string_sprintf("http://%S", host), *https = string_sprintf("https://%S", host);
    bool ok = http && https && (!string_compare(origin, http) || !string_compare(origin, https));
    string_free(http);
    string_free(https);
    return ok;
}
