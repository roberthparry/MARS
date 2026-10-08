/**
 * @file websrv_parse.c
 * @brief Bounded HTTP request parsing for the web server.
 *
 * Reads origin-form requests and validates start lines, headers and host information through string views. Strict
 * framing and size limits protect the boundary before application route handlers run.
 *
 * This is part of webserver.h's synchronous Linux listener. The HTTP client is a separate module; this
 * implementation does not supply a production worker pool or TLS terminator.
 */

/* Strict, bounded HTTP/1.1 origin-form parsing through string_t views. */
#include <errno.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "websrv_internal.h"

static unsigned char at(string_view_t v, size_t pos)
{
    unsigned char c = 0;
    string_view_peek_ascii(v, pos, &c);
    return c;
}

static size_t find(string_view_t v, size_t start, unsigned char c)
{
    size_t n = string_view_length(v);
    for (size_t i = start; i < n; ++i)
        if (at(v, i) == c) return i;
    return n;
}

static string_t *copy_slice(string_view_t v, size_t begin, size_t count)
{
    string_view_t part = string_view_slice(v, begin, count);
    return string_from_view(&part);
}

static int request_line(string_view_t line, websrv_request_t *r)
{
    static const char *const methods[] = {"GET", "POST", "PUT", "PATCH", "DELETE", "HEAD", "OPTIONS"};
    size_t n = string_view_length(line), a = find(line, 0, ' '), b = find(line, a + 1, ' ');
    if (!a || a >= n || b >= n || b == a + 1) return 400;
    string_view_t method = string_view_slice(line, 0, a);
    unsigned i;
    /* Seven fixed protocol tokens: a bounded scan, not a growing dispatch table. */
    for (i = 0; i <= HTTP_OPTIONS; ++i)
        if (string_view_equals_literal(method, methods[i])) break;
    if (i > HTTP_OPTIONS) return 501;
    r->method = (webmethod_t)i;
    if (!string_view_equals_literal(string_view_slice(line, b + 1, n - b - 1), "HTTP/1.1")) return 505;
    r->target = copy_slice(line, a + 1, b - a - 1);
    if (!r->target) return -1;
    if (!websrv_target_valid(r->target, false)) return 400;
    string_view_t target = string_view_all(r->target);
    r->path = copy_slice(target, 0, find(target, 0, '?'));
    return r->path ? 0 : -1;
}

static int header_line(string_view_t line, dictionary_t *headers)
{
    size_t n = string_view_length(line), colon = find(line, 0, ':');
    if (!colon || colon == n) return 400;
    string_t *name = copy_slice(line, 0, colon);
    string_t *key = websrv_field_name(name);
    string_free(name);
    if (!key) return 400;
    size_t start = colon + 1, end = n;
    while (start < end && (at(line, start) == ' ' || at(line, start) == '\t')) ++start;
    while (end > start && (at(line, end - 1) == ' ' || at(line, end - 1) == '\t')) --end;
    string_t *value = copy_slice(line, start, end - start);
    dictionary_entry_t *entry = NULL;
    int rc = 400;
    if (value && websrv_field_value(value) && !dictionary_get_entry(headers, &key, &entry)) {
        if (websrv_header_put(headers, key, value)) return 0;
        rc = -1;
    }
    string_free(key);
    string_free(value);
    return rc;
}

static bool valid_host(const string_t *host)
{
    if (!host || !string_byte_length(host)) return false;
    string_view_t v = string_view_all(host);
    for (size_t i = 0; i < string_view_length(v); ++i) {
        unsigned char c = at(v, i);
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '.' || c == '-' || c == ':' || c == '[' || c == ']')) return false;
    }
    return true;
}

static int parse_headers(const string_t *wire, size_t end, websrv_request_t *r, size_t *body_size,
                         const websrv_limits_t *limits)
{
    string_view_t v = string_view(wire, 0, end);
    size_t begin = 0;
    bool first = true;
    while (begin < end - 2) {
        size_t cr = find(v, begin, '\r');
        if (cr + 1 >= end || at(v, cr + 1) != '\n') return 400;
        string_view_t line = string_view_slice(v, begin, cr - begin);
        int status = first ? request_line(line, r) : header_line(line, r->headers);
        if (status) return status;
        first = false;
        begin = cr + 2;
    }
    if (!valid_host(websrv_header_get(r->headers, "host"))) return 400;
    const string_t *length = websrv_header_get(r->headers, "content-length");
    if (websrv_header_get(r->headers, "transfer-encoding")) return length ? 400 : 501;
    if (websrv_header_get(r->headers, "expect")) return 417;
    if (websrv_header_get(r->headers, "upgrade")) return 400;
    *body_size = 0;
    if (length) {
        string_view_t digits = string_view_all(length);
        if (!string_view_length(digits)) return 400;
        for (size_t i = 0; i < string_view_length(digits); ++i) {
            unsigned char c = at(digits, i);
            if (c < '0' || c > '9') return 400;
            if (*body_size > (SIZE_MAX - (c - '0')) / 10) return 413;
            *body_size = *body_size * 10 + (c - '0');
        }
        if (*body_size > limits->max_body_bytes) return 413;
    }
    return 0;
}

static ssize_t receive(int fd, void *buffer, size_t size, int64_t deadline)
{
    for (;;) {
        if (websrv_now() >= deadline) { errno = ETIMEDOUT; return -1; }
        int ready = websrv_wait(fd, POLLIN, deadline);
        if (ready <= 0) { if (!ready) errno = ETIMEDOUT; return -1; }
        ssize_t n = recv(fd, buffer, size, 0);
        if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        if (!n) errno = ECONNRESET;
        return n ? n : -1;
    }
}

int websrv_read_request(int fd, const websrv_limits_t *limits, int64_t deadline, websrv_request_t *r)
{
    r->headers = websrv_dict_new(sizeof(string_t *));
    string_t *wire = string_new();
    int status = -1;
    if (!r->headers || !wire) goto done;
    size_t end = 0, tail = 0, received = 0;
    char buffer[4096];
    while (!end) {
        size_t size = string_byte_length(wire);
        if (size == limits->max_header_bytes) { status = 431; goto done; }
        size_t room = limits->max_header_bytes - size;
        ssize_t n = receive(fd, buffer, room < sizeof(buffer) ? room : sizeof(buffer), deadline);
        if (n < 0) goto done;
        received = (size_t)n;
        for (tail = 0; tail < received; ++tail) {
            unsigned char c = (unsigned char)buffer[tail];
            if (!c || c > 127) { status = 400; goto done; }
            if (string_append_char(wire, (char)c)) goto done;
            size_t count = string_byte_length(wire);
            if (count >= 4 &&
                string_view_equals_literal(string_view(wire, count - 4, 4), "\r\n\r\n")) {
                end = count;
                ++tail;
                break;
            }
        }
    }
    size_t body_size;
    status = parse_headers(wire, end, r, &body_size, limits);
    if (status) goto done;
    size_t available = received - tail;
    if (available > body_size) available = body_size; /* Ignore pipelined bytes; connection will close. */
    r->body = body_size ? malloc(body_size) : NULL;
    if (body_size && !r->body) { status = -1; goto done; }
    r->body_size = body_size;
    if (available) memcpy(r->body, buffer + tail, available);
    while (available < body_size) {
        size_t remaining = body_size - available;
        ssize_t n = receive(fd, r->body + available, remaining, deadline);
        if (n < 0) { status = -1; goto done; }
        available += (size_t)n;
    }
done:
    string_free(wire);
    return status;
}
