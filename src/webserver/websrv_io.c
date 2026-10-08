/**
 * @file websrv_io.c
 * @brief Deadline-aware web-server socket I/O.
 *
 * Waits on non-blocking sockets and writes one framed response per connection. It centralises deadline and
 * partial-write handling without introducing persistent-connection scheduling.
 *
 * This is part of webserver.h's synchronous Linux listener. The HTTP client is a separate module; this
 * implementation does not supply a production worker pool or TLS terminator.
 */

/* Deadline-based non-blocking socket I/O and single-response framing. */
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <time.h>

#include "websrv_internal.h"

int64_t websrv_now(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now)) return 0;
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

int websrv_wait(int fd, short events, int64_t deadline)
{
    for (;;) {
        int64_t remaining = deadline - websrv_now();
        struct pollfd p = {.fd = fd, .events = events};
        int rc = poll(&p, 1, remaining > 0 ? (int)remaining : 0);
        if (rc < 0 && errno == EINTR) {
            if (websrv_now() >= deadline) return 0;
            continue;
        }
        return rc;
    }
}

bool websrv_send(int fd, const void *data, size_t size, int64_t deadline)
{
    const unsigned char *bytes = data;
    while (size) {
        if (websrv_now() >= deadline || websrv_wait(fd, POLLOUT, deadline) == 0) {
            errno = ETIMEDOUT;
            return false;
        }
        ssize_t sent = send(fd, bytes, size, MSG_NOSIGNAL);
        if (sent < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        if (sent <= 0) return false;
        bytes += sent;
        size -= (size_t)sent;
    }
    return true;
}

bool websrv_write_response(int fd, const websrv_response_t *r, bool head, int64_t deadline)
{
    bool bodyless = r->status == 204 || r->status == 205 || r->status == 304;
    size_t size = bodyless ? 0 : r->body_size;
    string_t *wire = string_sprintf("HTTP/1.1 %u Response\r\nConnection: close\r\n", r->status);
    if (!wire) return false;
    bool ok = true;
    if (r->status != 204 && r->status != 304)
        ok = string_append_format(wire, "Content-Length: %zu\r\n", size) >= 0;
    time_t now = time(NULL);
    struct tm utc;
    static const char *const days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char *const months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                         "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    if (ok && gmtime_r(&now, &utc))
        ok = string_append_format(wire, "Date: %s, %02d %s %04d %02d:%02d:%02d GMT\r\n",
             days[utc.tm_wday], utc.tm_mday, months[utc.tm_mon], utc.tm_year + 1900,
             utc.tm_hour, utc.tm_min, utc.tm_sec) >= 0;
    for (size_t i = 0; ok && i < dictionary_size(r->headers); ++i) {
        const string_t *key = *(string_t *const *)dictionary_get_key(r->headers, i);
        const string_t *value = *(string_t *const *)dictionary_get_value(r->headers, i);
        ok = string_append_format(wire, "%S: %S\r\n", key, value) >= 0;
    }
    ok = ok && !string_append_cstr(wire, "\r\n") &&
         websrv_send(fd, string_c_str(wire), string_byte_length(wire), deadline);
    if (ok && !head && size) ok = websrv_send(fd, r->body, size, deadline);
    string_free(wire);
    return ok;
}
