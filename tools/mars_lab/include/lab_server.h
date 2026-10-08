/**
 * @file lab_server.h
 * @brief Opaque native Lab server with owned listener and application routes.
 *
 * Owns the transport listener, browser routes and access policy. The application
 * controls process supervision without seeing or mutating webserver internals.
 * Create before fork; each single-threaded worker serves and frees its own copy.
 * Persistent state is synchronised separately by the state module.
 */
#ifndef MARS_LAB_SERVER_H
#define MARS_LAB_SERVER_H
#include <stdint.h>

#include "ustring.h"

/** @brief Opaque owner of a Lab listener and complete route set. */
typedef struct lab_server lab_server_t;

/**
 * @brief Create a fully routed native Lab server.
 * @param address Borrowed numeric IP; NULL selects IPv4 loopback.
 * @param port Port to bind; zero requests an ephemeral port.
 * @param timeout_ms Per-request deadline; zero selects 180 seconds.
 * @return Owned server, released with lab_svr_free, or NULL with errno set.
 */
lab_server_t *lab_svr_new(const string_t *address, uint16_t port, unsigned timeout_ms);

/**
 * @brief Release a server and this process's listener descriptor.
 * @param server Owned server to release; NULL is safe.
 */
void lab_svr_free(lab_server_t *server);

/**
 * @brief Get the actual listening port.
 * @param server Borrowed server, or NULL.
 * @return Listening port; zero for NULL.
 */
uint16_t lab_svr_port(const lab_server_t *server);

/**
 * @brief Accept and handle at most one request.
 * @param server Borrowed server used by one thread per process.
 * @param wait_ms Listener wait from zero (poll) through 600000 milliseconds.
 * @return One for a handled connection, zero for idle timeout, or -1 with errno set.
 */
int lab_svr_serve_once(lab_server_t *server, unsigned wait_ms);
#endif
