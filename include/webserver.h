/**
 * @file webserver.h
 * @brief Bounded synchronous Linux web server with opaque handles.
 *
 * Use websrv_t to implement a listening web service with method-and-path routing,
 * bounded requests and application callbacks. Opaque request and response handles
 * provide text, binary, JSON and XML body handling, while the caller controls the
 * synchronous serving loop.
 *
 * This module is suitable for local services, embedded endpoints and integration
 * tests. It supplies the shared webmethod_t type and HTTP_GET-style method constants.
 * Use http.h for client requests. Deployment requiring encrypted connections,
 * concurrent workers or persistent connections needs facilities beyond this server.
 *
 * No TLS, workers or persistent connections. Handles require external synchronisation.
 * Callbacks must not re-enter, modify routes on, or destroy their active server.
 */

#ifndef MARS_WEBSERVER_H
#define MARS_WEBSERVER_H
#include <stdint.h>
#include "dictionary.h"
#include "ustring.h"
#include "json.h"
#include "xml.h"

/** @brief HTTP request methods shared by the client and web server. */
typedef enum {
    HTTP_GET,     /**< Retrieve a representation. */
    HTTP_POST,    /**< Submit a representation for processing. */
    HTTP_PUT,     /**< Create or replace the target representation. */
    HTTP_PATCH,   /**< Apply a partial modification. */
    HTTP_DELETE,  /**< Delete the target resource. */
    HTTP_HEAD,    /**< Retrieve response metadata without a body. */
    HTTP_OPTIONS  /**< Request communication options. */
} webmethod_t;

/** @brief Opaque listening server and route owner. */
typedef struct _websrv_t websrv_t;

/** @brief Opaque request borrowed during a handler. */
typedef struct _websrv_request_t websrv_request_t;

/** @brief Opaque response builder borrowed during a handler. */
typedef struct _websrv_response_t websrv_response_t;

/** @brief Resource limits; zero fields select defaults. */
typedef struct {
    size_t max_header_bytes; /**< Each header budget; default 16 KiB, maximum 1 MiB, minimum 512 bytes. */
    size_t max_body_bytes;   /**< Each body budget; default 1 MiB, maximum 64 MiB. */
    unsigned timeout_ms;    /**< Absolute connection I/O deadline; default 5000 ms, maximum 600000 ms. */
} websrv_limits_t;

/**
 * @brief Build a response synchronously; initially status 200 and empty.
 * @param request Borrowed immutable request valid only during this callback.
 * @param response Borrowed builder valid only during this callback.
 * @param user_data Borrowed context registered with the route.
 * @return True to send the response; false to discard it and send empty status 500.
 */
typedef bool (*websrv_handler_fn)(const websrv_request_t *request, websrv_response_t *response, void *user_data);

/**
 * @brief Bind a synchronous Linux IPv4 server.
 * @param address Borrowed numeric IPv4 address; NULL selects 127.0.0.1; use 0.0.0.0 explicitly for public binding.
 * @param port TCP port; zero selects an ephemeral port.
 * @param limits Optional limits; NULL selects defaults.
 * @return Owned listener, or NULL with errno set; release with websrv_free().
 */
websrv_t *websrv_new(const string_t *address, uint16_t port, const websrv_limits_t *limits);

/**
 * @brief Close an idle listener and release routes, but not callback contexts.
 * @param server Owned idle server; NULL is harmless.
 */
void websrv_free(websrv_t *server);

/**
 * @brief Read the bound TCP port.
 * @param server Borrowed listener.
 * @return Bound port, or zero for NULL.
 */
uint16_t websrv_port(const websrv_t *server);

/**
 * @brief Register or replace an exact method/path route.
 * @param server Idle server to modify.
 * @param method Supported method; HEAD requires its own route.
 * @param path Borrowed ASCII path starting with /, without query or fragment; copied, not decoded.
 * @param handler Required synchronous callback.
 * @param user_data Borrowed context retained until replacement or destruction.
 * @return True on success; false with errno set on failure.
 */
bool websrv_route(websrv_t *server, webmethod_t method, const string_t *path,
                   websrv_handler_fn handler, void *user_data);

/**
 * @brief Process at most one connection, always closing it afterwards.
 * @param server Idle server; no concurrent use or callback re-entry.
 * @param wait_ms Listener wait from zero (poll) through 600000 milliseconds.
 * @return 1 for a processed connection, including rejection; 0 for idle timeout; -1 on local/I/O failure with errno set.
 * Peer timeouts set ETIMEDOUT. Handler execution cannot be pre-empted.
 */
int websrv_serve_once(websrv_t *server, unsigned wait_ms);

/**
 * @brief Read the request method.
 * @param request Borrowed request.
 * @return Method, or HTTP_GET for NULL.
 */
webmethod_t websrv_request_method(const websrv_request_t *request);

/**
 * @brief Borrow the raw origin-form target including query; no URL decoding.
 * @param request Borrowed request.
 * @return Borrowed string valid only during the handler, or NULL.
 */
const string_t *websrv_request_target(const websrv_request_t *request);

/**
 * @brief Borrow the raw routing path without query; no URL decoding.
 * @param request Borrowed request.
 * @return Borrowed string valid only during the handler, or NULL.
 */
const string_t *websrv_request_path(const websrv_request_t *request);

/**
 * @brief Borrow a header value using a case-insensitive name.
 * @param request Borrowed request; duplicate request headers are rejected before dispatch.
 * @param name Borrowed ASCII header name.
 * @return Borrowed value during the handler, or NULL if absent or invalid.
 */
const string_t *websrv_request_header(const websrv_request_t *request, const string_t *name);

/**
 * @brief Borrow binary request bytes, preserving embedded NUL bytes.
 * @param request Borrowed request.
 * @return Borrowed bytes during the handler, or NULL for an empty body or NULL input.
 */
const void *websrv_request_body(const websrv_request_t *request);

/**
 * @brief Read the request body byte count.
 * @param request Borrowed request.
 * @return Byte count, or zero for NULL.
 */
size_t websrv_request_body_size(const websrv_request_t *request);

/**
 * @brief Decode the request body as exact UTF-8 without normalisation.
 * @param request Borrowed request; Content-Type is not interpreted.
 * @return Owned text, released with string_free(), or NULL on invalid input or failure.
 */
string_t *websrv_request_text(const websrv_request_t *request);

/**
 * @brief Decode the request body as JSON.
 * @param request Borrowed request; Content-Type is not interpreted.
 * @return Owned json, released with json_free(), or NULL on invalid input or failure.
 */
json_t *websrv_request_json(const websrv_request_t *request);

/**
 * @brief Decode the request body as XML.
 * @param request Borrowed request; Content-Type is not interpreted.
 * @return Owned xml, released with xml_free(), or NULL on invalid input or failure.
 */
xml_t *websrv_request_xml(const websrv_request_t *request);

/**
 * @brief Set the final response status.
 * @param response Borrowed active response builder.
 * @param status Status 200 through 599; 204, 205 and 304 suppress the body.
 * @return True on success; false on invalid input.
 */
bool websrv_response_status(websrv_response_t *response, unsigned status);

/**
 * @brief Copy or replace a response header.
 * @param response Borrowed active response builder.
 * @param name Borrowed ASCII token. Framing, connection, upgrade, trailer and Date headers are reserved.
 * @param value Borrowed printable ASCII or tab value; NUL bytes and newlines are rejected.
 * @return True on success; false on invalid input, budget exhaustion or allocation failure.
 */
bool websrv_response_header(websrv_response_t *response, const string_t *name, const string_t *value);

/**
 * @brief Copy a binary response body within the configured budget.
 * @param response Borrowed active response builder.
 * @param data Borrowed bytes; NULL allowed only for zero size.
 * @param size Byte count.
 * @return True on success; false leaves the old body unchanged.
 */
bool websrv_response_body(websrv_response_t *response, const void *data, size_t size);

/**
 * @brief Copy encoded text bytes; set Content-Type separately.
 * @param response Borrowed active response builder.
 * @param text Borrowed text.
 * @return True on success; false on failure. Return false from the handler if any setter fails.
 */
bool websrv_response_text(websrv_response_t *response, const string_t *text);

/**
 * @brief Copy serialised JSON and set application/json.
 * @param response Borrowed active response builder.
 * @param json Borrowed json.
 * @return True on success; false on failure. Return false from the handler if any setter fails.
 */
bool websrv_response_json(websrv_response_t *response, const json_t *json);

/**
 * @brief Copy serialised XML and set application/xml.
 * @param response Borrowed active response builder.
 * @param xml Borrowed xml.
 * @return True on success; false on failure. Return false from the handler if any setter fails.
 */
bool websrv_response_xml(websrv_response_t *response, const xml_t *xml);

#endif
