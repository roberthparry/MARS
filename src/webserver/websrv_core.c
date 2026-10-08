/**
 * @file websrv_core.c
 * @brief Linux web listener lifecycle and exact route dispatch.
 *
 * Creates the listener, registers method-and-path handlers and serves individual connections. The application owns
 * the synchronous serving loop; worker pools and TLS termination are not implemented here.
 *
 * This is part of webserver.h's synchronous Linux listener. The HTTP client is a separate module; this
 * implementation does not supply a production worker pool or TLS terminator.
 */

/* Linux listener lifetime and exact dictionary-backed route dispatch. */
#include <arpa/inet.h>
#include <errno.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "websrv_internal.h"

/* Bind a bounded IP listener; exposure beyond loopback is explicit. */
websrv_t *websrv_new(const string_t *address, uint16_t port, const websrv_limits_t *limits)
{
    websrv_limits_t l = limits ? *limits : (websrv_limits_t){0};
    if (!l.max_header_bytes) l.max_header_bytes = 16384;
    if (!l.max_body_bytes) l.max_body_bytes = 1048576;
    if (!l.timeout_ms) l.timeout_ms = 5000;
    if (l.max_header_bytes < 512 || l.max_header_bytes > 1048576 ||
        l.max_body_bytes > 67108864 || l.timeout_ms > 600000) {
        errno = EINVAL;
        return NULL;
    }
    struct sockaddr_storage addr = {0};
    struct sockaddr_in *v4 = (struct sockaddr_in *)&addr;
    struct sockaddr_in6 *v6 = (struct sockaddr_in6 *)&addr;
    socklen_t length;
    if (!address || (websrv_field_value(address) &&
                     inet_pton(AF_INET, string_c_str(address), &v4->sin_addr) == 1)) {
        v4->sin_family = AF_INET;
        v4->sin_port = htons(port);
        if (!address) v4->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        length = sizeof(*v4);
    } else if (websrv_field_value(address) &&
               inet_pton(AF_INET6, string_c_str(address), &v6->sin6_addr) == 1) {
        v6->sin6_family = AF_INET6;
        v6->sin6_port = htons(port);
        length = sizeof(*v6);
    } else {
        errno = EINVAL;
        return NULL;
    }
    websrv_t *s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->fd = -1;
    s->limits = l;
    s->routes = websrv_dict_new(sizeof(websrv_routes_t));
    s->fd = socket(addr.ss_family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    int one = 1;
    int zero = 0;
    if (!s->routes || s->fd < 0 || setsockopt(s->fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) ||
        (addr.ss_family == AF_INET6 && setsockopt(s->fd, IPPROTO_IPV6, IPV6_V6ONLY, &zero, sizeof(zero))) ||
        bind(s->fd, (struct sockaddr *)&addr, length) || listen(s->fd, 32) ||
        getsockname(s->fd, (struct sockaddr *)&addr, &length)) {
        int saved = errno;
        websrv_free(s);
        errno = saved;
        return NULL;
    }
    s->port = ntohs(addr.ss_family == AF_INET ? v4->sin_port : v6->sin6_port);
    return s;
}

/* Release an idle server without taking ownership of callback contexts. */
void websrv_free(websrv_t *s)
{
    if (!s) return;
    if (s->active) { errno = EBUSY; return; }
    if (s->fd >= 0) close(s->fd);
    for (size_t i = 0; i < dictionary_size(s->routes); ++i)
        string_free(*(string_t *const *)dictionary_get_key(s->routes, i));
    dictionary_destroy(s->routes);
    free(s);
}

/* Return the actual port after ephemeral binding. */
uint16_t websrv_port(const websrv_t *s) { return s ? s->port : 0; }

/* Copy an exact route into a path hash table with directly indexed methods. */
bool websrv_route(websrv_t *s, webmethod_t method, const string_t *path,
                   websrv_handler_fn handler, void *user_data)
{
    if (!s || s->active || (unsigned)method > HTTP_OPTIONS || !handler || !websrv_target_valid(path, true)) {
        errno = s && s->active ? EBUSY : EINVAL;
        return false;
    }
    dictionary_entry_t *entry = NULL;
    websrv_routes_t route = {0};
    if (dictionary_get_entry(s->routes, &path, &entry))
        route = *(const websrv_routes_t *)dictionary_entry_value(entry);
    route.handler[method] = handler;
    route.context[method] = user_data;
    if (entry) return dictionary_set_entry(s->routes, entry, &route);
    string_t *key = string_clone(path);
    if (key && dictionary_set(s->routes, &key, &route)) return true;
    string_free(key);
    errno = ENOMEM;
    return false;
}

static bool allow_header(websrv_response_t *response, const websrv_routes_t *route)
{
    static const char *const methods[] = {"GET", "POST", "PUT", "PATCH", "DELETE", "HEAD", "OPTIONS"};
    string_t *name = string_new_with("Allow"), *value = string_new();
    bool ok = name && value;
    for (unsigned i = 0; ok && i <= HTTP_OPTIONS; ++i) {
        if (!route->handler[i]) continue;
        if (string_byte_length(value)) ok = string_append_cstr(value, ", ") == 0;
        if (ok) ok = string_append_cstr(value, methods[i]) == 0;
    }
    ok = ok && websrv_response_header(response, name, value);
    string_free(name);
    string_free(value);
    return ok;
}

/* Accept one bounded connection and dispatch only a fully parsed request. */
int websrv_serve_once(websrv_t *s, unsigned wait_ms)
{
    if (!s || s->active || wait_ms > 600000) {
        errno = s && s->active ? EBUSY : EINVAL;
        return -1;
    }
    int ready = websrv_wait(s->fd, POLLIN, websrv_now() + wait_ms);
    if (ready <= 0) return ready;
    struct sockaddr_storage peer = {0};
    socklen_t peer_size = sizeof(peer);
    int fd = accept4(s->fd, (struct sockaddr *)&peer, &peer_size, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (fd < 0) return errno == EAGAIN || errno == EWOULDBLOCK ? 0 : -1;
    s->active = true;
    int64_t deadline = websrv_now() + s->limits.timeout_ms;
    websrv_request_t request = {0};
    websrv_response_t response = {
        .status = 200, .max_body = s->limits.max_body_bytes, .max_headers = s->limits.max_header_bytes,
        .headers = websrv_dict_new(sizeof(string_t *)), .header_bytes = 192
    };
    int result = -1;
    char peer_text[INET6_ADDRSTRLEN];
    const void *peer_address = peer.ss_family == AF_INET
        ? (const void *)&((struct sockaddr_in *)&peer)->sin_addr
        : (const void *)&((struct sockaddr_in6 *)&peer)->sin6_addr;
    if (!response.headers || !inet_ntop(peer.ss_family, peer_address, peer_text, sizeof(peer_text)) ||
        !(request.peer = string_new_with(peer_text))) goto done;
    int status = websrv_read_request(fd, &s->limits, deadline, &request);
    if (status < 0) goto done;
    if (status) {
        response.status = (unsigned)status;
    } else {
        dictionary_entry_t *entry = NULL;
        if (!dictionary_get_entry(s->routes, &request.path, &entry)) {
            response.status = 404;
        } else {
            const websrv_routes_t *route = dictionary_entry_value(entry);
            bool ok = true;
            if (!route->handler[request.method]) {
                response.status = 405;
                ok = allow_header(&response, route);
            } else {
                ok = route->handler[request.method](&request, &response, route->context[request.method]);
            }
            if (!ok) {
                websrv_response_clear(&response);
                response.status = 500;
            }
        }
    }
    result = websrv_write_response(fd, &response, request.method == HTTP_HEAD, deadline) ? 1 : -1;
done:;
    int saved = errno;
    websrv_request_clear(&request);
    websrv_response_clear(&response);
    close(fd);
    s->active = false;
    errno = saved;
    return result;
}
