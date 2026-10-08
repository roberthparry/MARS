/**
 * @file http_fixture.h
 * @brief Private transport contract for the native offline protocol peers.
 *
 * Shares bounded socket/TLS operations and request metadata between the fixture
 * executable and its HTTP, HTTP/2 and WebSocket handlers. Uses public string
 * facilities for text, while raw wire output bypasses the HTTP library so
 * malformed replies reach the client intact.
 */
#ifndef MARS_HTTP_FIXTURE_H
#define MARS_HTTP_FIXTURE_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>
#include <openssl/ssl.h>

#include "ustring.h"

typedef struct {
    int fd;
    SSL *tls;
    struct timespec deadline;
} fixture_connection_t;

typedef struct {
    string_t *method;
    string_t *path;
    string_t *authorization;
    string_t *cookie;
    string_t *type;
    string_t *soap_action;
    string_t *custom;
    string_t *websocket_key;
    size_t length;
    bool close;
} fixture_request_t;

/**
 * @brief Initialise a non-blocking connection with a 15-second total lifetime.
 * @param connection Receives transport state; the caller retains descriptor ownership.
 * @param fd Accepted socket descriptor.
 * @return False if the clock or non-blocking setup fails.
 */
bool fixture_connection_init(fixture_connection_t *connection, int fd);

/**
 * @brief Shorten the remaining connection lifetime without extending its deadline.
 * @param connection Borrowed connection.
 * @param milliseconds Maximum remaining lifetime, measured with the monotonic clock.
 * @return False on clock failure; subsequent I/O then fails closed.
 */
bool fixture_limit_lifetime(fixture_connection_t *connection, unsigned milliseconds);

/**
 * @brief Perform a server TLS handshake within the transport deadlines.
 * @param connection Borrowed connection with an attached TLS session.
 * @return Whether the handshake completed.
 */
bool fixture_tls_accept(fixture_connection_t *connection);

/**
 * @brief Send TLS close notification within the remaining transport deadline.
 * @param connection Borrowed connection with an attached TLS session.
 * @return No value; the caller releases TLS state and closes the socket.
 */
void fixture_tls_shutdown(fixture_connection_t *connection);

/**
 * @brief Read a complete wire field.
 * @param connection Borrowed connected socket and optional TLS session.
 * @param data Destination buffer of at least size bytes.
 * @param size Exact byte count to receive.
 * @return False on disconnect or timeout.
 */
bool fixture_read(fixture_connection_t *connection, void *data, size_t size);

/**
 * @brief Send a complete wire field.
 * @param connection Borrowed connected socket and optional TLS session.
 * @param data Borrowed bytes; NULL is allowed for an empty field.
 * @param size Byte count to transmit.
 * @return False on disconnect or timeout.
 */
bool fixture_write(fixture_connection_t *connection, const void *data, size_t size);

/**
 * @brief Send a literal protocol fragment, excluding its terminator.
 * @param connection Borrowed connection.
 * @param text Borrowed NUL-terminated protocol literal.
 * @return Whether every byte was sent.
 */
bool fixture_text(fixture_connection_t *connection, const char *text);

/**
 * @brief Send owned text through the socket interoperability boundary.
 * @param connection Borrowed connection.
 * @param text Borrowed MARS string.
 * @return Whether every encoded byte was sent.
 */
bool fixture_string(fixture_connection_t *connection, const string_t *text);

/**
 * @brief Compare request text with a fixed protocol literal.
 * @param text Borrowed string; NULL compares equal only to an empty literal.
 * @param literal Borrowed comparison literal.
 * @return Whether the whole string matches.
 */
bool fixture_equal(const string_t *text, const char *literal);

/**
 * @brief Parse bounded unsigned decimal text with a string cursor.
 * @param text Borrowed decimal string.
 * @param maximum Largest accepted value.
 * @param value Receives the result on success.
 * @return False for empty, non-decimal or out-of-range input.
 */
bool fixture_decimal(const string_t *text, size_t maximum, size_t *value);

/**
 * @brief Parse a bounded HTTP request head into owned strings and body length.
 * @param connection Borrowed connection.
 * @param request Receives owned fields on success; cleared on failure.
 * @return Whether a complete supported request head was parsed.
 */
bool fixture_request(fixture_connection_t *connection, fixture_request_t *request);

/**
 * @brief Release all owned request fields.
 * @param request Initialised request, cleared before returning.
 * @return No value.
 */
void fixture_request_free(fixture_request_t *request);

/**
 * @brief Delay a deliberate slow response.
 * @param connection Borrowed connection whose total deadline bounds the delay.
 * @param milliseconds Required delay in milliseconds.
 * @return No value.
 */
void fixture_delay(fixture_connection_t *connection, unsigned milliseconds);

/**
 * @brief Serve persistent HTTP/1.1 and deliberately invalid replies.
 * @param connection Borrowed connection with a bounded I/O deadline.
 * @return No value; the caller closes the connection.
 */
void fixture_http(fixture_connection_t *connection);

/**
 * @brief Serve the single-stream HTTP/2 gRPC framing fixture.
 * @param connection Borrowed connection with a bounded I/O deadline.
 * @return No value; the caller closes the connection.
 */
void fixture_grpc(fixture_connection_t *connection);

/**
 * @brief Serve WebSocket handshakes, echo frames and intentional faults.
 * @param connection Borrowed connection with a bounded I/O deadline.
 * @return No value; the caller closes the connection.
 */
void fixture_websocket(fixture_connection_t *connection);

#endif
