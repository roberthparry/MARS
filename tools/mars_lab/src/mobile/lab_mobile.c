/**
 * @file lab_mobile.c
 * @brief Native private-network discovery, verified tailnet hosts and mobile metadata.
 *
 * Interface enumeration verifies numeric hosts without trusting DNS or Host text.
 * Tailscale status and Serve configuration are inspected through the bounded Lab
 * process runner. QR geometry is delegated to the independent lab_mobile_qr.c
 * encoder; this unit is responsible only for choosing and verifying access URLs.
 */
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <ctype.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <stdlib.h>
#include <string.h>

#include "lab_mobile.h"
#include "lab_process.h"

static const json_t *lab_mobile_member(const json_t *object, const char *name)
{
    string_t *key = string_new_with(name);
    const json_t *value = key ? json_object_get(object, key) : NULL;
    string_free(key);
    return value;
}

static bool lab_mobile_put(json_t *object, const char *name, const json_t *value)
{
    string_t *key = string_new_with(name);
    bool ok = key && value && json_object_set(object, key, value);
    string_free(key);
    return ok;
}

static bool lab_mobile_put_text(json_t *object, const char *name, const string_t *text)
{
    json_t *value = text ? json_new_string(text) : NULL;
    bool ok = value && lab_mobile_put(object, name, value);
    json_free(value);
    return ok;
}

static string_t *lab_mobile_host_name(const string_t *header)
{
    string_cursor_t *cursor = header ? string_cursor_new(header) : NULL;
    string_t *host = string_new();
    if (!cursor || !host)
        goto fail;
    bool bracketed = string_cursor_consume(cursor, "[");
    unsigned char ch;
    while (string_cursor_peek_ascii(cursor, &ch)) {
        bool allowed = bracketed ? isxdigit(ch) || ch == ':' || ch == '.' : isalnum(ch) || ch == '-' || ch == '.';
        if (!allowed)
            break;
        if (string_append_char(host, (char)tolower(ch)))
            goto fail;
        string_cursor_next(cursor);
    }
    if (!string_byte_length(host) || string_byte_length(host) > 253 ||
        (bracketed && !string_cursor_consume(cursor, "]")))
        goto fail;
    if (string_cursor_consume(cursor, ":")) {
        unsigned port = 0, digits = 0;
        while (string_cursor_peek_ascii(cursor, &ch) && ch >= '0' && ch <= '9') {
            if (++digits > 5)
                goto fail;
            port = port * 10 + ch - '0';
            string_cursor_next(cursor);
        }
        if (!digits || !port || port > 65535)
            goto fail;
    }
    if (!string_cursor_done(cursor))
        goto fail;
    if (!bracketed && string_ends_with(host, ".")) {
        string_view_t view = string_view(host, 0, string_byte_length(host) - 1);
        string_t *normalised = string_from_view(&view);
        string_free(host);
        host = normalised;
    }
    string_cursor_free(cursor);
    return host;
fail:
    string_cursor_free(cursor);
    string_free(host);
    return NULL;
}

static json_t *lab_mobile_tailscale_json(bool serve)
{
    const char *const status_args[] = {"tailscale", "status", "--json", NULL};
    const char *const serve_args[] = {"tailscale", "serve", "status", "--json", NULL};
    string_t *output = NULL;
    int status = -1;
    bool ok = lab_proc_run(serve ? serve_args : status_args, NULL, 1500, 1048576, &output, &status);
    json_t *result = ok && status == 0 && output ? json_from_text(output) : NULL;
    string_free(output);
    if (json_type(result) != JSON_OBJECT) {
        json_free(result);
        return NULL;
    }
    return result;
}

static bool lab_mobile_funnel_disabled(const json_t *value, unsigned depth)
{
    if (depth > 24)
        return false;
    if (json_type(value) == JSON_OBJECT) {
        const json_t *funnel = lab_mobile_member(value, "AllowFunnel");
        if (funnel) {
            if (json_type(funnel) != JSON_OBJECT)
                return false;
            for (size_t i = 0; i < json_object_size(funnel); ++i) {
                bool enabled;
                if (!json_bool_value(json_object_value_at(funnel, i), &enabled) || enabled)
                    return false;
            }
        }
        /* Inspect nested foreground/service configurations as well as the top-level map. */
        for (size_t i = 0; i < json_object_size(value); ++i)
            if (!lab_mobile_funnel_disabled(json_object_value_at(value, i), depth + 1))
                return false;
    } else if (json_type(value) == JSON_ARRAY) {
        for (size_t i = 0; i < json_array_size(value); ++i)
            if (!lab_mobile_funnel_disabled(json_array_get(value, i), depth + 1))
                return false;
    }
    return true;
}

static json_t *lab_mobile_verified_serve(const string_t *host)
{
    if (!host || !string_ends_with(host, ".ts.net"))
        return NULL;
    json_t *status = lab_mobile_tailscale_json(false), *serve = NULL;
    const string_t *backend = json_string_value(lab_mobile_member(status, "BackendState"));
    const json_t *self = lab_mobile_member(status, "Self");
    const string_t *dns = json_string_value(lab_mobile_member(self, "DNSName"));
    string_t *own_host = dns ? lab_mobile_host_name(dns) : NULL;
    bool online = false;
    if (backend && string_view_equals_literal(string_view_all(backend), "Running") &&
        json_bool_value(lab_mobile_member(self, "Online"), &online) && online && own_host &&
        !string_compare(host, own_host)) {
        serve = lab_mobile_tailscale_json(true);
        if (serve && !lab_mobile_funnel_disabled(serve, 0)) {
            json_free(serve);
            serve = NULL;
        }
    }
    string_free(own_host);
    json_free(status);
    return serve;
}

/* Augment the server allowlist with only this node's verified private tailnet DNS name. */
bool lab_mobile_host_allowed(const string_t *host_header)
{
    string_t *host = lab_mobile_host_name(host_header);
    json_t *serve = lab_mobile_verified_serve(host);
    bool allowed = serve != NULL;
    json_free(serve);
    string_free(host);
    return allowed;
}

static bool lab_mobile_private_address(int family, const void *address, bool *tailnet)
{
    *tailnet = false;
    if (family == AF_INET) {
        uint32_t value = ntohl(((const struct in_addr *)address)->s_addr);
        *tailnet = (value & 0xffc00000u) == 0x64400000u;
        if (!*tailnet && (value & 0xff000000u) != 0x0a000000u && (value & 0xfff00000u) != 0xac100000u &&
            (value & 0xffff0000u) != 0xc0a80000u && (value & 0xffff0000u) != 0xa9fe0000u)
            return false;
    } else if (family == AF_INET6) {
        const struct in6_addr *address6 = address;
        if ((address6->s6_addr[0] & 0xfe) != 0xfc)
            return false;
        static const unsigned char tailnet_prefix[] = {0xfd, 0x7a, 0x11, 0x5c, 0xa1, 0xe0};
        *tailnet = !memcmp(address6->s6_addr, tailnet_prefix, sizeof tailnet_prefix);
    } else
        return false;
    return true;
}

static bool lab_mobile_local_private_address(const string_t *host, bool *tailnet, bool *ipv6)
{
    struct in_addr address4;
    struct in6_addr address6;
    int family = inet_pton(AF_INET, string_c_str(host), &address4) == 1    ? AF_INET
                 : inet_pton(AF_INET6, string_c_str(host), &address6) == 1 ? AF_INET6
                                                                           : 0;
    *ipv6 = family == AF_INET6;
    if (!lab_mobile_private_address(family, family == AF_INET ? (const void *)&address4 : &address6, tailnet))
        return false;
    struct ifaddrs *interfaces = NULL;
    if (getifaddrs(&interfaces))
        return false;
    bool found = false;
    /* Interface addresses are supplied as a linked list by the operating system. */
    for (struct ifaddrs *entry = interfaces; entry; entry = entry->ifa_next) {
        if (!entry->ifa_addr || !(entry->ifa_flags & IFF_UP) || entry->ifa_addr->sa_family != family)
            continue;
        if (family == AF_INET)
            found = !memcmp(&((struct sockaddr_in *)entry->ifa_addr)->sin_addr, &address4, sizeof address4);
        else
            found = !memcmp(&((struct sockaddr_in6 *)entry->ifa_addr)->sin6_addr, &address6, sizeof address6);
        if (found)
            break;
    }
    freeifaddrs(interfaces);
    return found;
}

static bool lab_mobile_wildcard_bind(const string_t *bind, bool *ipv6)
{
    string_view_t view = string_view_all(bind);
    *ipv6 = string_view_equals_literal(view, "::");
    return *ipv6 || string_view_equals_literal(view, "0.0.0.0");
}

static bool lab_mobile_bound_address(const string_t *bind, const string_t *host)
{
    bool ipv6;
    if (lab_mobile_wildcard_bind(bind, &ipv6))
        return ipv6 || string_find(host, ":") < 0;
    struct in_addr bound4, host4;
    struct in6_addr bound6, host6;
    if (inet_pton(AF_INET, string_c_str(bind), &bound4) == 1 && inet_pton(AF_INET, string_c_str(host), &host4) == 1)
        return !memcmp(&bound4, &host4, sizeof bound4);
    if (inet_pton(AF_INET6, string_c_str(bind), &bound6) == 1 && inet_pton(AF_INET6, string_c_str(host), &host6) == 1)
        return !memcmp(&bound6, &host6, sizeof bound6);
    return false;
}

static bool lab_mobile_desktop_host(const string_t *host)
{
    string_view_t view = string_view_all(host);
    if (string_view_equals_literal(view, "localhost") || string_view_equals_literal(view, "0.0.0.0") ||
        string_view_equals_literal(view, "::"))
        return true;
    struct in_addr address4;
    struct in6_addr address6;
    if (inet_pton(AF_INET, string_c_str(host), &address4) == 1)
        return (ntohl(address4.s_addr) & 0xff000000u) == 0x7f000000u;
    if (inet_pton(AF_INET6, string_c_str(host), &address6) == 1)
        return IN6_IS_ADDR_LOOPBACK(&address6);
    return false;
}

static string_t *lab_mobile_discover_private_host(const string_t *bind, bool *tailnet, bool *ipv6)
{
    bool allow_ipv6;
    if (!lab_mobile_wildcard_bind(bind, &allow_ipv6))
        return NULL;
    struct ifaddrs *interfaces = NULL;
    if (getifaddrs(&interfaces))
        return NULL;
    string_t *selected = NULL;
    unsigned best_rank = 5;
    /* Inspect each OS-provided interface once; prefer LAN IPv4, then ULA, then link-local or tailnet. */
    for (struct ifaddrs *entry = interfaces; entry; entry = entry->ifa_next) {
        if (!entry->ifa_addr || !(entry->ifa_flags & IFF_UP) || !(entry->ifa_flags & IFF_RUNNING) ||
            (entry->ifa_flags & IFF_LOOPBACK))
            continue;
        int family = entry->ifa_addr->sa_family;
        const void *address;
        unsigned rank;
        if (family == AF_INET) {
            address = &((const struct sockaddr_in *)entry->ifa_addr)->sin_addr;
            uint32_t value = ntohl(((const struct in_addr *)address)->s_addr);
            rank = (value & 0xffff0000u) == 0xa9fe0000u ? 2 : 0;
        } else if (family == AF_INET6 && allow_ipv6) {
            address = &((const struct sockaddr_in6 *)entry->ifa_addr)->sin6_addr;
            rank = 1;
        } else
            continue;
        bool candidate_tailnet;
        if (!lab_mobile_private_address(family, address, &candidate_tailnet))
            continue;
        if (candidate_tailnet)
            rank = 3;
        if (rank >= best_rank)
            continue;
        char numeric[INET6_ADDRSTRLEN];
        if (!inet_ntop(family, address, numeric, sizeof numeric))
            continue;
        string_t *candidate = string_new_with(numeric);
        if (!candidate)
            break;
        string_free(selected);
        selected = candidate;
        best_rank = rank;
        *tailnet = candidate_tailnet;
        *ipv6 = family == AF_INET6;
    }
    freeifaddrs(interfaces);
    return selected;
}

static bool lab_mobile_https_proxy(const json_t *serve, const string_t *host, uint16_t port, unsigned depth)
{
    if (depth > 24 || json_type(serve) != JSON_OBJECT)
        return false;
    string_t *authority = string_sprintf("%S:443", host);
    const json_t *web = lab_mobile_member(serve, "Web");
    const json_t *config = authority ? json_object_get(web, authority) : NULL;
    const string_t *proxy =
        json_string_value(lab_mobile_member(lab_mobile_member(lab_mobile_member(config, "Handlers"), "/"), "Proxy"));
    string_t *expected4 = string_sprintf("http://127.0.0.1:%u", (unsigned)port);
    string_t *expected6 = string_sprintf("http://[::1]:%u", (unsigned)port);
    string_t *expected_local = string_sprintf("http://localhost:%u", (unsigned)port);
    bool https = false;
    json_bool_value(lab_mobile_member(lab_mobile_member(lab_mobile_member(serve, "TCP"), "443"), "HTTPS"), &https);
    bool matches = https && proxy && expected4 && expected6 && expected_local &&
                   (!string_compare(proxy, expected4) || !string_compare(proxy, expected6) ||
                    !string_compare(proxy, expected_local));
    string_free(authority);
    string_free(expected4);
    string_free(expected6);
    string_free(expected_local);
    const json_t *foreground = lab_mobile_member(serve, "Foreground");
    for (size_t i = 0; !matches && i < json_object_size(foreground); ++i)
        matches = lab_mobile_https_proxy(json_object_value_at(foreground, i), host, port, depth + 1);
    return matches;
}

/* Report private sharing already reachable through the supplied, verified local host. */
json_t *lab_mobile_details(const string_t *host_header, uint16_t port)
{
    string_t *host = lab_mobile_host_name(host_header), *url = string_new();
    const char *configured_bind = getenv("MARS_LAB_BIND_HOST");
    string_t *bind = string_new_with(configured_bind ? configured_bind : "127.0.0.1");
    bool tailnet = false, ipv6 = false, reachable = false;
    int error = 0;
    json_t *serve = host && port ? lab_mobile_verified_serve(host) : NULL;
    bool proxy = serve && lab_mobile_https_proxy(serve, host, port, 0), bind_ipv6 = false, bind_tailnet = false;
    bool wildcard = bind && lab_mobile_wildcard_bind(bind, &bind_ipv6);
    bool direct_tailnet = bind && lab_mobile_local_private_address(bind, &bind_tailnet, &bind_ipv6) && bind_tailnet;
    if (serve && (proxy || wildcard || direct_tailnet)) {
        tailnet = true;
        error = !url    ? -1
                : proxy ? string_append_format(url, "https://%S/", host)
                        : string_append_format(url, "http://%S:%u/", host, (unsigned)port);
        reachable = error >= 0;
    } else if (host && bind && port && lab_mobile_bound_address(bind, host) &&
               lab_mobile_local_private_address(host, &tailnet, &ipv6)) {
        error = url ? string_append_format(url, ipv6 ? "http://[%S]:%u/" : "http://%S:%u/", host, (unsigned)port) : -1;
        reachable = error >= 0;
    } else if (host && wildcard && port && lab_mobile_desktop_host(host)) {
        string_t *discovered = lab_mobile_discover_private_host(bind, &tailnet, &ipv6);
        if (discovered) {
            error =
                url ? string_append_format(url, ipv6 ? "http://[%S]:%u/" : "http://%S:%u/", discovered, (unsigned)port)
                    : -1;
            reachable = error >= 0;
        }
        string_free(discovered);
    }
    const char *title = reachable ? (tailnet ? "Tailscale access" : "WiFi access") : "Local access";
    const char *hint = reachable ? (tailnet ? "Scan from a device connected to Tailscale."
                                            : "Scan from a phone on the same private network.")
                                 : "No verified private mobile URL is available for this host.";
    string_t *title_text = string_new_with(title), *hint_text = string_new_with(hint);
    string_t *qr = url ? lab_mobile_qr(url) : NULL;
    json_t *result = json_new_object(), *disabled = json_new_bool(false),
           *tailscale = json_new_bool(reachable && tailnet);
    bool ok = error >= 0 && bind && result && disabled && tailscale && title_text && hint_text && url && qr &&
              lab_mobile_put_text(result, "title", title_text) && lab_mobile_put_text(result, "hint", hint_text) &&
              lab_mobile_put_text(result, "url", url) && lab_mobile_put_text(result, "qr", qr) &&
              lab_mobile_put(result, "control", disabled) && lab_mobile_put(result, "funnel", disabled) &&
              lab_mobile_put(result, "tailscale", tailscale);
    json_free(tailscale);
    json_free(disabled);
    json_free(serve);
    string_free(qr);
    string_free(url);
    string_free(host);
    string_free(bind);
    string_free(title_text);
    string_free(hint_text);
    if (!ok) {
        json_free(result);
        result = NULL;
    }
    return result;
}
