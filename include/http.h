#ifndef MARS_HTTP_H
#define MARS_HTTP_H

#include <stdbool.h>
#include <stddef.h>
#include "webserver.h"
#include "dictionary.h"
#include "ustring.h"
#include "file.h"
#include "json.h"
#include "xml.h"

/**
 * @file http.h
 * @brief Synchronous C HTTP/HTTPS transport with opaque requests, responses and reusable clients.
 *
 * Uses libcurl >= 7.85.0 with verified TLS. Redirects, environment proxies, cookies and netrc credentials are disabled.
 * The module does not implement retries. Only HTTP and HTTPS are permitted. Callers must authorise destination URLs;
 * this is not an SSRF sandbox. All handles require external synchronisation; callbacks must not re-enter them.
 * HTTP 4xx/5xx responses are returned normally: check http_response_ok separately from transport success.
 */

/** @brief Opaque reusable connection owner. */
typedef struct _http_client_t http_client_t;

/** @brief Opaque request with owned URL, headers and body configuration. */
typedef struct _http_request_t http_request_t;

/** @brief Opaque completed HTTP response. */
typedef struct _http_response_t http_response_t;

/** @brief Transport failure categories, separate from HTTP status codes. */
typedef enum {
    HTTP_ERROR_NONE, HTTP_ERROR_ARGUMENT, HTTP_ERROR_MEMORY, HTTP_ERROR_TRANSPORT,
    HTTP_ERROR_TIMEOUT, HTTP_ERROR_LIMIT, HTTP_ERROR_CANCELLED, HTTP_ERROR_IO, HTTP_ERROR_PROTOCOL
} http_error_t;

/** @brief Transfer controls. Zero fields select the listed defaults, not unlimited values. */
typedef struct {
    size_t connect_timeout_ms; /**< Connection establishment timeout; default 10000 ms. */
    size_t total_timeout_ms;   /**< Whole transfer timeout; default 30000 ms. */
    size_t max_body_bytes;     /**< Download body limit, also in streaming mode; default 16 MiB. */
    size_t max_header_bytes;   /**< Cumulative response/user-request header budgets; default 64 KiB each. */
    size_t max_upload_bytes;   /**< Outgoing body limit; default 16 MiB. */
} http_limits_t;

/**
 * @brief Consume a response-body fragment.
 * @param data Borrowed binary bytes, valid only during this callback.
 * @param size Byte count; UTF-8 characters may be split across fragments.
 * @param user_data Caller-supplied context.
 * @return True if fully consumed, false to abort. Do not mutate or destroy active handles.
 */
typedef bool (*http_body_fn)(const void *data, size_t size, void *user_data);

/**
 * @brief Poll whether a transfer should be cancelled.
 * @param user_data Caller-supplied context.
 * @return True to cancel, false to continue. Called on the sending thread; do not re-enter active handles.
 */
typedef bool (*http_cancel_fn)(void *user_data);

/**
 * @brief Create a synchronous HTTP/HTTPS client with secure defaults.
 * @return Owned client, or NULL on allocation or libcurl initialisation failure; release with http_client_free.
 */
http_client_t *http_client_new(void);

/**
 * @brief Destroy a client and its connection cache.
 * @param client Owned idle client to destroy; NULL is harmless.
 */
void http_client_free(http_client_t *client);

/**
 * @brief Set transfer deadlines and resource limits.
 * @param client Idle client to modify.
 * @param limits Required controls; zero fields select defaults. Timeouts must fit a positive long.
 * @return True on success; false for invalid input. Previous settings remain on failure.
 */
bool http_client_set_limits(http_client_t *client, const http_limits_t *limits);

/**
 * @brief Load a custom PEM trust bundle through the file module.
 * @param client Idle client to modify; verification remains enabled.
 * @param path Borrowed regular-file path, or NULL to restore system trust. Symlinks are rejected; maximum 4 MiB.
 * @return True on success; false on invalid input, allocation or file I/O failure.
 */
bool http_client_set_ca_file(http_client_t *client, const string_t *path);

/**
 * @brief Install an optional cooperative cancellation callback.
 * @param client Idle client to modify; NULL is harmless.
 * @param callback Borrowed function pointer, or NULL to disable cancellation.
 * @param user_data Borrowed context retained until replaced or client destruction.
 */
void http_client_set_cancel(http_client_t *client, http_cancel_fn callback, void *user_data);

/**
 * @brief Read the last transfer error category.
 * @param client Borrowed client.
 * @return Last error, or HTTP_ERROR_ARGUMENT for NULL. HTTP status errors are not transport errors.
 */
http_error_t http_client_error(const http_client_t *client);

/**
 * @brief Borrow the last transfer diagnostic.
 * @param client Borrowed client.
 * @return Borrowed text until the next transfer or client destruction, or NULL if unavailable.
 */
const string_t *http_client_error_text(const http_client_t *client);

/**
 * @brief Create a request with a copied absolute HTTP or HTTPS URL.
 * @param method Supported HTTP method.
 * @param url Borrowed ASCII URL; percent-encode non-ASCII bytes. User information, fragments and control characters
 *   are rejected.
 * @return Owned request, or NULL for invalid input or allocation failure; release with http_request_free.
 */
http_request_t *http_request_new(webmethod_t method, const string_t *url);

/**
 * @brief Release a request and its copied headers and body.
 * @param request Owned idle request; NULL is harmless.
 */
void http_request_free(http_request_t *request);

/**
 * @brief Copy or replace a request header using case-insensitive names.
 * @param request Idle request to modify.
 * @param name Borrowed ASCII token. Host, framing, connection, Expect and proxy-authorization headers are managed
 *   internally.
 * @param value Borrowed printable ASCII or tab value. CR, LF, NUL and other controls are rejected.
 * @return True on success; false for invalid input or allocation failure.
 */
bool http_request_set_header(http_request_t *request, const string_t *name, const string_t *value);

/**
 * @brief Set the Authorization header to a copied bearer token.
 * @param request Idle request to modify.
 * @param token Borrowed non-empty printable ASCII token without whitespace; never included in diagnostics.
 * @return True on success; false on invalid input or allocation failure.
 */
bool http_request_set_bearer(http_request_t *request, const string_t *token);

/**
 * @brief Copy a binary request body.
 * @param request Idle request; GET and HEAD do not accept bodies.
 * @param data Borrowed bytes; NULL is allowed only for zero size.
 * @param size Number of bytes, not characters.
 * @param content_type Optional borrowed Content-Type value; NULL leaves any existing header unchanged.
 * @return True on success; false on invalid input or allocation failure, leaving the old body unchanged.
 */
bool http_request_set_body(http_request_t *request, const void *data, size_t size, const string_t *content_type);

/**
 * @brief Copy a string's exact encoded bytes as the request body.
 * @param request Idle request accepting a body.
 * @param text Borrowed text, including any embedded NUL bytes.
 * @param content_type Optional borrowed Content-Type value.
 * @return True on success; false on invalid input or allocation failure.
 */
bool http_request_set_text(http_request_t *request, const string_t *text, const string_t *content_type);

/**
 * @brief Serialise a JSON body and set application/json.
 * @param request Idle request accepting a body.
 * @param json Borrowed JSON tree.
 * @return True on success; false on invalid input or serialisation/allocation failure.
 */
bool http_request_set_json(http_request_t *request, const json_t *json);

/**
 * @brief Serialise an XML body and set application/xml.
 * @param request Idle request accepting a body.
 * @param xml Borrowed XML document or root element.
 * @return True on success; false on invalid input or serialisation/allocation failure.
 */
bool http_request_set_xml(http_request_t *request, const xml_t *xml);

/**
 * @brief Select a file to upload incrementally through the file module.
 * @param request Idle request accepting a body.
 * @param path Borrowed regular-file path; copied now and opened at each transfer. Symlinks are rejected.
 * @param content_type Optional borrowed Content-Type value.
 * @return True on success; file errors are reported when sending. The previous body remains on setter failure.
 */
bool http_request_set_body_file(http_request_t *request, const string_t *path, const string_t *content_type);

/**
 * @brief Perform a request and buffer its binary response body.
 * @param client Idle client; used synchronously and not concurrently.
 * @param request Borrowed request, unchanged and reusable.
 * @return Owned complete response for any HTTP status; NULL on transport, limit, cancellation or allocation failure.
 */
http_response_t *http_client_send(http_client_t *client, const http_request_t *request);

/**
 * @brief Stream a response body without retaining it.
 * @param client Idle client; callbacks must not re-enter, modify or destroy this client or request.
 * @param request Borrowed request.
 * @param callback Required sink accepting borrowed bytes; false aborts the transfer.
 * @param user_data Borrowed sink context, used only during this call.
 * @return Owned response metadata, or NULL on failure. The sink may have consumed partial data, including HTTP error
 *   bodies.
 */
http_response_t *http_client_stream(http_client_t *client, const http_request_t *request, http_body_fn callback, void *user_data);

/**
 * @brief Stream a response to a caller-opened file.
 * @param client Idle client.
 * @param request Borrowed request.
 * @param destination Borrowed open writable file; written at its current position and neither closed nor deleted.
 * @return Owned response metadata, or NULL on failure. Partial data and HTTP error bodies can remain in the
 *   destination.
 */
http_response_t *http_client_download(http_client_t *client, const http_request_t *request, file_t *destination);

/**
 * @brief Release response headers and any buffered body.
 * @param response Owned response; NULL is harmless.
 */
void http_response_free(http_response_t *response);

/**
 * @brief Read the final HTTP status.
 * @param response Borrowed response.
 * @return HTTP status code, or zero for NULL.
 */
long http_response_status(const http_response_t *response);

/**
 * @brief Test for an HTTP success status.
 * @param response Borrowed response.
 * @return True for status 200 through 299; false otherwise.
 */
bool http_response_ok(const http_response_t *response);

/**
 * @brief Borrow buffered body bytes without assuming text encoding.
 * @param response Borrowed response.
 * @return Borrowed bytes until response destruction, or NULL for an empty or streamed body.
 */
const void *http_response_body(const http_response_t *response);

/**
 * @brief Read the number of buffered body bytes.
 * @param response Borrowed response.
 * @return Buffered byte count; zero for NULL or streamed responses.
 */
size_t http_response_body_size(const http_response_t *response);

/**
 * @brief Read the number of body bytes delivered.
 * @param response Borrowed response.
 * @return Received byte count, including streamed bytes; zero for NULL.
 */
size_t http_response_received_size(const http_response_t *response);

/**
 * @brief Copy buffered body bytes as exact UTF-8, without normalisation.
 * @param response Borrowed buffered response; no HTTP-status or Content-Type interpretation is performed.
 * @return Owned text released with string_free, or NULL for streaming mode, malformed UTF-8 or allocation failure.
 */
string_t *http_response_text(const http_response_t *response);

/**
 * @brief Parse a buffered UTF-8 body with the JSON module.
 * @param response Borrowed response; check status and Content-Type separately.
 * @return Owned JSON tree released with json_free, or NULL on decoding/parsing failure.
 */
json_t *http_response_json(const http_response_t *response);

/**
 * @brief Parse a buffered body with the native XML module.
 * @param response Borrowed response; check status and Content-Type separately.
 * @return Owned XML document released with xml_free, or NULL on decoding/parsing failure.
 */
xml_t *http_response_xml(const http_response_t *response);

/**
 * @brief Borrow final response headers, including trailers.
 * @param response Borrowed response.
 * @return Read-only dictionary of lower-case string_t * keys and array_t * values containing string_t * entries, or
 *   NULL.
 */
const dictionary_t *http_response_headers(const http_response_t *response);

/**
 * @brief Count repeated values for one case-insensitive header name.
 * @param response Borrowed response.
 * @param name Borrowed ASCII header name.
 * @return Number of values, or zero if absent or invalid.
 */
size_t http_response_header_count(const http_response_t *response, const string_t *name);

/**
 * @brief Borrow one header value without combining repeated fields.
 * @param response Borrowed response.
 * @param name Borrowed case-insensitive header name.
 * @param index Zero-based value position.
 * @return Borrowed value until response destruction, or NULL if absent or out of range.
 */
const string_t *http_response_header_at(const http_response_t *response, const string_t *name, size_t index);

/**
 * @brief Percent-encode one URL query value or path segment.
 * @param text Borrowed text; its encoded bytes are escaped, retaining only ASCII unreserved bytes. Spaces become %20.
 * @return Owned ASCII text released with string_free, or NULL on invalid input or allocation failure.
 */
string_t *http_url_encode(const string_t *text);

#endif /* MARS_HTTP_H */
