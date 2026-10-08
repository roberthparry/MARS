/**
 * @file http_fixture_transport.c
 * @brief Deadline-bounded socket and TLS transport for native protocol fixtures.
 *
 * Non-blocking I/O and monotonic poll deadlines bound handshakes, wire transfers,
 * deliberate delays and TLS shutdown. Every accepted connection has a 15-second
 * total lifetime; each transfer also has a five-second limit. Protocol handlers
 * may shorten the lifetime, notably for the final HTTP/2 acknowledgement drain.
 */
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <openssl/err.h>
#include <sys/socket.h>

#include "http_fixture.h"

typedef enum { FIXTURE_READ, FIXTURE_WRITE, FIXTURE_ACCEPT, FIXTURE_SHUTDOWN } fixture_operation_t;

static struct timespec fixture_after(struct timespec now, unsigned milliseconds)
{
    now.tv_sec += milliseconds / 1000;
    now.tv_nsec += (long)(milliseconds % 1000) * 1000000L;
    if (now.tv_nsec >= 1000000000L) {
        ++now.tv_sec;
        now.tv_nsec -= 1000000000L;
    }
    return now;
}

static struct timespec fixture_earlier(struct timespec left, struct timespec right)
{
    return left.tv_sec < right.tv_sec || (left.tv_sec == right.tv_sec && left.tv_nsec < right.tv_nsec) ? left : right;
}

static int fixture_remaining(struct timespec deadline)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now))
        return 0;
    time_t seconds = deadline.tv_sec - now.tv_sec;
    long nanoseconds = deadline.tv_nsec - now.tv_nsec;
    if (nanoseconds < 0) {
        --seconds;
        nanoseconds += 1000000000L;
    }
    if (seconds < 0 || (!seconds && !nanoseconds)) {
        errno = ETIMEDOUT;
        return 0;
    }
    return (int)(seconds * 1000 + (nanoseconds + 999999L) / 1000000L);
}

static bool fixture_wait(fixture_connection_t *connection, short events, struct timespec deadline)
{
    struct pollfd descriptor = {.fd = connection->fd, .events = events};
    for (;;) {
        int remaining = fixture_remaining(deadline);
        if (!remaining)
            return false;
        int result = poll(&descriptor, 1, remaining);
        if (result > 0)
            return !(descriptor.revents & POLLNVAL);
        if (result < 0 && errno != EINTR)
            return false;
        /* Recheck the absolute clock after signals and poll timeouts. */
    }
}

static bool fixture_operation_deadline(fixture_connection_t *connection, struct timespec *deadline)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now))
        return false;
    *deadline = fixture_earlier(connection->deadline, fixture_after(now, 5000));
    return fixture_remaining(*deadline) > 0;
}

static int fixture_transfer(fixture_connection_t *connection, fixture_operation_t operation,
                             void *data, int size, struct timespec deadline)
{
    while (fixture_remaining(deadline)) {
        int result;
        short events = operation == FIXTURE_READ ? POLLIN : POLLOUT;
        if (connection->tls) {
            ERR_clear_error();
            errno = 0;
            if (operation == FIXTURE_READ)
                result = SSL_read(connection->tls, data, size);
            else if (operation == FIXTURE_WRITE)
                result = SSL_write(connection->tls, data, size);
            else if (operation == FIXTURE_ACCEPT)
                result = SSL_accept(connection->tls);
            else {
                result = SSL_shutdown(connection->tls);
                if (!result)
                    return 1; /* The close notification was sent; do not await its peer. */
            }
            if (result > 0)
                return result;
            int error = SSL_get_error(connection->tls, result);
            if (error == SSL_ERROR_WANT_READ)
                events = POLLIN;
            else if (error == SSL_ERROR_WANT_WRITE)
                events = POLLOUT;
            else if (error == SSL_ERROR_SYSCALL && errno == EINTR)
                continue;
            else
                return -1;
        } else {
            result = operation == FIXTURE_READ ? (int)recv(connection->fd, data, (size_t)size, 0) :
                                                (int)send(connection->fd, data, (size_t)size, 0);
            if (result > 0)
                return result;
            if (!result)
                return -1;
            if (errno == EINTR)
                continue;
            if (errno != EAGAIN && errno != EWOULDBLOCK)
                return -1;
        }
        if (!fixture_wait(connection, events, deadline))
            return -1;
    }
    return -1;
}

/* Start the total lifetime before allocating TLS state or starting a worker. */
bool fixture_connection_init(fixture_connection_t *connection, int fd)
{
    *connection = (fixture_connection_t){.fd = fd};
    struct timespec now;
    int flags = fcntl(fd, F_GETFL);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) || clock_gettime(CLOCK_MONOTONIC, &now))
        return false;
    connection->deadline = fixture_after(now, 15000);
    return true;
}

/* Shortening a drain deadline can never renew the original lifetime. */
bool fixture_limit_lifetime(fixture_connection_t *connection, unsigned milliseconds)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now)) {
        connection->deadline = (struct timespec){0};
        return false;
    }
    connection->deadline = fixture_earlier(connection->deadline, fixture_after(now, milliseconds));
    return true;
}

/* Read the complete field within both the transfer and connection deadlines. */
bool fixture_read(fixture_connection_t *connection, void *data, size_t size)
{
    struct timespec deadline;
    if (!fixture_operation_deadline(connection, &deadline))
        return false;
    unsigned char *bytes = data;
    while (size) {
        int count = size > INT_MAX ? INT_MAX : (int)size;
        int result = fixture_transfer(connection, FIXTURE_READ, bytes, count, deadline);
        if (result <= 0)
            return false;
        bytes += result;
        size -= (size_t)result;
    }
    return true;
}

/* Keep the same buffer and size while OpenSSL requests a retry. */
bool fixture_write(fixture_connection_t *connection, const void *data, size_t size)
{
    struct timespec deadline;
    if (!fixture_operation_deadline(connection, &deadline))
        return false;
    const unsigned char *bytes = data;
    while (size) {
        int count = size > INT_MAX ? INT_MAX : (int)size;
        int result = fixture_transfer(connection, FIXTURE_WRITE, (void *)bytes, count, deadline);
        if (result <= 0)
            return false;
        bytes += result;
        size -= (size_t)result;
    }
    return true;
}

/* The handshake consumes the same total lifetime as protocol traffic. */
bool fixture_tls_accept(fixture_connection_t *connection)
{
    struct timespec deadline;
    return fixture_operation_deadline(connection, &deadline) &&
           fixture_transfer(connection, FIXTURE_ACCEPT, NULL, 0, deadline) > 0;
}

/* Do not extend connection lifetime to send a TLS close notification. */
void fixture_tls_shutdown(fixture_connection_t *connection)
{
    struct timespec deadline;
    if (fixture_operation_deadline(connection, &deadline))
        fixture_transfer(connection, FIXTURE_SHUTDOWN, NULL, 0, deadline);
}

/* Bound deliberate slow responses by the remaining total connection lifetime. */
void fixture_delay(fixture_connection_t *connection, unsigned milliseconds)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now))
        return;
    struct timespec deadline = fixture_earlier(connection->deadline, fixture_after(now, milliseconds));
    int remaining;
    while ((remaining = fixture_remaining(deadline)) > 0) {
        if (poll(NULL, 0, remaining) < 0 && errno != EINTR)
            return;
    }
}
