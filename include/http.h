/**
 * @file http.h
 * @brief Synchronous C HTTP/HTTPS transport with opaque requests, responses and reusable clients.
 *
 * Use this module to consume web services through reusable clients with configured
 * timeouts, size limits and authentication. Requests and responses support text,
 * binary data, JSON, XML, SOAP, URL-encoded forms and multipart uploads. Additional
 * helpers cover cookies, redirects, client certificates, event streams, WebSocket
 * messages and unary gRPC exchanges.
 *
 * JSON, XML and Protocol Buffers provide payload representations, not transport.
 * Include their respective headers when manipulating those documents or messages.
 * SOAP support does not provide automatic WSDL client generation, and gRPC support
 * is limited to uncompressed unary calls. Use webserver.h to implement a listening
 * service rather than consume one.
 *
 * Uses libcurl >= 7.86.0 with verified TLS. Redirects and cookies are opt-in; environment proxies and netrc are
 * disabled. The module does not implement retries. Only HTTP and HTTPS are permitted. Callers must authorise
 * destination URLs; this is not an SSRF sandbox. All handles require external synchronisation; callbacks must not
 * re-enter them. HTTP 4xx/5xx responses are returned normally: check http_response_ok separately from transport
 * success.
 */

#ifndef MARS_HTTP_H
#define MARS_HTTP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dictionary.h"
#include "file.h"
#include "json.h"
#include "ustring.h"
#include "webserver.h"
#include "xml.h"

/** @brief Opaque Protocol Buffers message; include protobuf.h to construct or inspect it. */
typedef struct _protobuf_t protobuf_t;

/** @brief Opaque byte-array result; include array.h for array access and destruction. */
typedef struct _array_t array_t;

/** @brief Opaque reusable connection owner. */
typedef struct _http_client_t http_client_t;

/** @brief Opaque request with owned URL, headers and body configuration. */
typedef struct _http_request_t http_request_t;

/** @brief Opaque completed HTTP response. */
typedef struct _http_response_t http_response_t;

/** @brief Transport failure categories, separate from HTTP status codes. */
typedef enum {
    HTTP_ERROR_NONE,
    HTTP_ERROR_ARGUMENT,
    HTTP_ERROR_MEMORY,
    HTTP_ERROR_TRANSPORT,
    HTTP_ERROR_TIMEOUT,
    HTTP_ERROR_LIMIT,
    HTTP_ERROR_CANCELLED,
    HTTP_ERROR_IO,
    HTTP_ERROR_PROTOCOL
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
http_response_t *http_client_stream(http_client_t *client, const http_request_t *request, http_body_fn callback,
                                    void *user_data);

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

/**
 * @brief Opaque ordered form preserving duplicate field names.
 */
typedef struct _http_form_t http_form_t;

/**
 * @brief Create an empty reusable form.
 * @return Owned form, or NULL; release with http_form_free().
 */
http_form_t *http_form_new(void);

/**
 * @brief Release a form and copied parts.
 * @param form Owned form; NULL is harmless.
 */
void http_form_free(http_form_t *form);

/**
 * @brief Copy a text form field.
 * @param form Form to modify.
 * @param name Borrowed field name.
 * @param value Borrowed text, copied without normalisation.
 * @return True on success; false on invalid input or allocation failure.
 */
bool http_form_add_text(http_form_t *form, const string_t *name, const string_t *value);

/**
 * @brief Add a file part, copying metadata and deferring reading until multipart preparation.
 * @param form Form to modify.
 * @param name Borrowed printable ASCII field name without quotes or backslashes.
 * @param filename Borrowed printable ASCII transmitted filename without quotes or backslashes; never inferred from
 * path.
 * @param content_type Borrowed printable ASCII media type.
 * @param path Borrowed path; file module rejects symlinks when opened.
 * @return True on success, false on invalid input or allocation failure.
 */
bool http_form_add_file(http_form_t *form, const string_t *name, const string_t *filename, const string_t *content_type,
                        const string_t *path);

/**
 * @brief Encode text fields as application/x-www-form-urlencoded; spaces become +.
 * @param form Borrowed text-only form; file parts are rejected.
 * @return Owned encoded string, or NULL; release with string_free().
 */
string_t *http_form_encode(const http_form_t *form);

/**
 * @brief Set a copied URL-encoded form body and Content-Type.
 * @param request Request accepting a body.
 * @param form Borrowed text-only form.
 * @return True on success, false on failure.
 */
bool http_request_set_form(http_request_t *request, const http_form_t *form);

/**
 * @brief Build a bounded multipart/form-data snapshot, reading file parts through file_t.
 * @param request Request accepting a body.
 * @param form Borrowed form; all contents are copied now, not streamed during sending.
 * @param max_bytes Encoded body budget; zero selects 16 MiB; maximum 64 MiB.
 * @return True on success; false leaves the old request body unchanged.
 */
bool http_request_set_multipart(http_request_t *request, const http_form_t *form, size_t max_bytes);

/**
 * @brief Set an explicit Basic Authorization header.
 * @param request Request to modify; use HTTPS for credentials.
 * @param username Borrowed UTF-8 username without colon or NUL.
 * @param password Borrowed UTF-8 password without NUL.
 * @return True on success; false on invalid input or allocation failure.
 */
bool http_request_set_basic(http_request_t *request, const string_t *username, const string_t *password);

/**
 * @brief Sign caller-defined canonical bytes using HMAC-SHA256 and standard Base64.
 * @param canonical Borrowed exact message bytes; this helper does not define a service-specific signing scheme.
 * @param key Borrowed secret bytes.
 * @param key_size Secret byte count, greater than zero.
 * @return Owned Base64 signature, or NULL; release with string_free().
 */
string_t *http_hmac_sha256(const string_t *canonical, const void *key, size_t key_size);

/**
 * @brief Enable an in-memory cookie session, or disable it and discard all cookies.
 * @param client Idle client to modify; no cookie files are read or written.
 * @param enabled True to enable; disabled by default. Cookies are scoped by libcurl domain/path rules.
 * @return True on success, false on failure.
 */
bool http_client_set_cookies(http_client_t *client, bool enabled);

/**
 * @brief Discard all session cookies.
 * @param client Idle client.
 * @return True on success, false on failure.
 */
bool http_client_clear_cookies(http_client_t *client);

/**
 * @brief Follow bounded same-origin redirects for buffered GET/HEAD requests only.
 * @param client Idle client.
 * @param max_hops Zero disables redirects (default); maximum 16. Streaming and other methods return redirects
 * unchanged.
 * @return True on success, false on invalid input. Cross-origin redirects fail without sending credentials.
 */
bool http_client_set_redirects(http_client_t *client, unsigned max_hops);

/**
 * @brief Load a PEM TLS client identity through the file module and reset cached connections.
 * @param client Idle client.
 * @param certificate Borrowed PEM certificate path, or NULL together with private_key to clear identity.
 * @param private_key Borrowed PEM key path; each file is limited to 4 MiB and symlinks are rejected.
 * @param password Optional borrowed key password without NUL; copied.
 * @return True on success; false leaves the old identity unchanged. TLS verification remains enabled.
 */
bool http_client_set_identity_files(http_client_t *client, const string_t *certificate, const string_t *private_key,
                                    const string_t *password);

/**
 * @brief Request an OAuth2 client-credentials token using form-encoded Basic client authentication.
 * @param client Idle HTTP client.
 * @param url Borrowed HTTPS token endpoint; HTTP is allowed only for numeric loopback testing.
 * @param client_id Borrowed OAuth client identifier.
 * @param secret Borrowed client secret.
 * @param scope Optional borrowed space-separated scope.
 * @return Owned HTTP response, including OAuth error responses; NULL on failure. Parse and validate token JSON.
 */
http_response_t *http_oauth2_client_credentials(http_client_t *client, const string_t *url, const string_t *client_id,
                                                const string_t *secret, const string_t *scope);

/**
 * @brief Exchange a refresh token; token storage, expiry scheduling and consent remain application responsibilities.
 * @param client Idle HTTP client.
 * @param url Borrowed HTTPS token endpoint; numeric loopback HTTP is allowed for tests.
 * @param client_id Borrowed client identifier.
 * @param secret Borrowed client secret.
 * @param refresh_token Borrowed refresh token.
 * @param scope Optional scope.
 * @return Owned HTTP response including OAuth errors, or NULL on failure.
 */
http_response_t *http_oauth2_refresh(http_client_t *client, const string_t *url, const string_t *client_id,
                                     const string_t *secret, const string_t *refresh_token, const string_t *scope);

/**
 * @brief Supported SOAP envelope versions.
 */
typedef enum { HTTP_SOAP_11, HTTP_SOAP_12 } http_soap_version_t;

/**
 * @brief Opaque validated SOAP envelope with borrowed body and fault accessors.
 */
typedef struct _http_soap_t http_soap_t;

/**
 * @brief Build a SOAP envelope using the native XML module.
 * @param version SOAP 1.1 or 1.2.
 * @param payload Borrowed single body element, deep-copied.
 * @param header Optional borrowed single header block, deep-copied.
 * @return Owned envelope element, or NULL; release with xml_free.
 */
xml_t *http_soap_envelope(http_soap_version_t version, const xml_t *payload, const xml_t *header);

/**
 * @brief Prepare a POST with the appropriate SOAP media type and action.
 * @param request POST request to modify atomically.
 * @param version SOAP version.
 * @param action Borrowed printable ASCII action without quotes or backslashes; NULL selects an empty action.
 * @param payload Borrowed body element.
 * @param header Optional borrowed header block.
 * @return True on success; false leaves the request unchanged.
 */
bool http_request_set_soap(http_request_t *request, http_soap_version_t version, const string_t *action,
                           const xml_t *payload, const xml_t *header);

/**
 * @brief Validate and copy an envelope, recognising namespace aliases and SOAP faults.
 * @param document Borrowed XML document or envelope. Header blocks are rejected because no mustUnderstand handler is
 * registered.
 * @return Owned result, or NULL for malformed or unsupported envelopes; release with http_soap_free.
 */
http_soap_t *http_soap_parse(const xml_t *document);

/**
 * @brief Release an envelope and extracted fault text.
 * @param soap Owned envelope; NULL is harmless.
 */
void http_soap_free(http_soap_t *soap);

/**
 * @brief Borrow the validated Body element, including its ordered payload children.
 * @param soap Borrowed envelope.
 * @return Borrowed Body until soap is freed, or NULL.
 */
const xml_t *http_soap_body(const http_soap_t *soap);

/**
 * @brief Report whether the body contains a recognised SOAP Fault.
 * @param soap Borrowed envelope.
 * @return True for a SOAP Fault; false for normal or NULL envelopes.
 */
bool http_soap_is_fault(const http_soap_t *soap);

/**
 * @brief Borrow the SOAP fault code text without interpreting application-specific codes.
 * @param soap Borrowed envelope.
 * @return Borrowed code until soap is freed, or NULL when absent.
 */
const string_t *http_soap_fault_code(const http_soap_t *soap);

/**
 * @brief Borrow the first SOAP fault reason, preserving its text.
 * @param soap Borrowed envelope.
 * @return Borrowed reason until soap is freed, or NULL when absent.
 */
const string_t *http_soap_fault_reason(const http_soap_t *soap);

/**
 * @brief Opaque incremental UTF-8 Server-Sent Events reader.
 */
typedef struct _http_event_reader_t http_event_reader_t;

/**
 * @brief Consume one complete event; all strings are borrowed only during the callback.
 * @param event Event type, defaulting to message.
 * @param data Joined data lines without the final newline.
 * @param id Persistent last-event identifier, possibly empty.
 * @param has_retry Whether a valid retry field has been received.
 * @param retry_ms Latest retry delay in milliseconds; meaningful only when has_retry is true.
 * @param user_data Borrowed context supplied at construction.
 * @return True to continue, false to abort parsing. Do not re-enter the reader.
 */
typedef bool (*http_event_fn)(const string_t *event, const string_t *data, const string_t *id, bool has_retry,
                              uint64_t retry_ms, void *user_data);

/**
 * @brief Create a bounded SSE reader without automatic reconnection.
 * @param max_bytes Maximum bytes per line and accumulated event; zero selects 1 MiB; maximum 64 MiB.
 * @param callback Required event consumer.
 * @param user_data Borrowed callback context.
 * @return Owned reader, or NULL; release with http_event_reader_free.
 */
http_event_reader_t *http_event_reader_new(size_t max_bytes, http_event_fn callback, void *user_data);

/**
 * @brief Release reader state.
 * @param reader Owned idle reader; NULL is harmless.
 */
void http_event_reader_free(http_event_reader_t *reader);

/**
 * @brief Consume a byte fragment, handling split UTF-8, BOM, CRLF and multi-line events.
 * @param reader Reader to update.
 * @param data Borrowed bytes; NULL only when size is zero.
 * @param size Byte count.
 * @return True on success; false on invalid UTF-8, limits, allocation failure or callback rejection; failure is sticky.
 */
bool http_event_reader_feed(http_event_reader_t *reader, const void *data, size_t size);

/**
 * @brief Finish input, discarding an event not terminated by a blank line.
 * @param reader Reader to finish; no subsequent feed calls are allowed.
 * @return True on valid EOF; false for previous errors or invalid trailing UTF-8.
 */
bool http_event_reader_finish(http_event_reader_t *reader);

/**
 * @brief Opaque parsed unary gRPC result with an application status and optional Protocol Buffers payload.
 */
typedef struct _http_grpc_t http_grpc_t;

/**
 * @brief Prepare an uncompressed unary gRPC POST and require HTTP/2.
 * @param request POST request to modify; add authentication and metadata with ordinary HTTP setters.
 * @param message Borrowed Protocol Buffers message, serialised and copied.
 * @return True on success, false on invalid input or allocation failure; do not send on failure.
 */
bool http_request_set_grpc(http_request_t *request, const protobuf_t *message);

/**
 * @brief Validate a buffered unary gRPC response, including status and framing.
 * @param response Borrowed response from HTTP/2 with application/grpc media type.
 * @param max_fields Protocol Buffers field limit; zero selects 4096.
 * @return Owned result including non-zero gRPC statuses, or NULL on malformed/unsupported responses.
 */
http_grpc_t *http_response_grpc(const http_response_t *response, size_t max_fields);

/**
 * @brief Release a parsed gRPC result.
 * @param result Owned result; NULL is harmless.
 */
void http_grpc_free(http_grpc_t *result);

/**
 * @brief Read the gRPC application status independently of HTTP status.
 * @param result Borrowed result.
 * @return Status 0 to 16, or -1 for NULL.
 */
int http_grpc_status(const http_grpc_t *result);

/**
 * @brief Borrow the percent-decoded UTF-8 grpc-message.
 * @param result Borrowed result.
 * @return Borrowed message, possibly empty, or NULL for NULL result.
 */
const string_t *http_grpc_message(const http_grpc_t *result);

/**
 * @brief Borrow the unary Protocol Buffers response.
 * @param result Borrowed result.
 * @return Borrowed message on status zero, or NULL on error; valid until result destruction.
 */
const protobuf_t *http_grpc_payload(const http_grpc_t *result);

/**
 * @brief Opaque synchronous WebSocket connection using the HTTP client's TLS settings and limits.
 */
typedef struct _http_websocket_t http_websocket_t;

/**
 * @brief Upgrade a GET request to a WebSocket connection without following redirects.
 * @param client Borrowed idle client; remains exclusively reserved until http_websocket_free. Do not free it first.
 * @param request Borrowed GET request with an HTTP/HTTPS URL, mapped internally to ws/wss; copied handshake headers.
 * @return Owned connection, or NULL with client error set. Compression and subprotocol negotiation are unsupported.
 */
http_websocket_t *http_websocket_open(http_client_t *client, const http_request_t *request);

/**
 * @brief Close the transport and release the client's reservation.
 * @param socket Owned connection; NULL is harmless. Send an explicit close first for a graceful protocol shutdown.
 */
void http_websocket_free(http_websocket_t *socket);

/**
 * @brief Send one complete bounded text or binary message, handling partial writes.
 * @param socket Open idle connection.
 * @param text True for strict UTF-8 text; false for binary.
 * @param data Borrowed bytes; NULL only for size zero.
 * @param size Byte count, bounded by the client's upload limit.
 * @return True when the entire message was sent; false sets the client error and closes the connection on I/O failure.
 */
bool http_websocket_send(http_websocket_t *socket, bool text, const void *data, size_t size);

/**
 * @brief Receive and reassemble one bounded message, handling interleaved control frames.
 * @param socket Open idle connection.
 * @param text Required output: true for text, false for binary; unchanged on failure.
 * @return Owned byte array, or NULL on error or a peer close; inspect client error and http_websocket_is_open.
 */
array_t *http_websocket_receive(http_websocket_t *socket, bool *text);

/**
 * @brief Send a close frame and stop further messaging; does not wait for the peer's acknowledgement.
 * @param socket Open idle connection.
 * @param code Valid wire close code: 1000-1014 excluding 1004-1006, or 3000-4999.
 * @param reason Optional borrowed UTF-8 reason, at most 123 encoded bytes.
 * @return True if the close frame was sent; release the transport with http_websocket_free.
 */
bool http_websocket_close(http_websocket_t *socket, uint16_t code, const string_t *reason);

/**
 * @brief Check whether data messaging is still permitted.
 * @param socket Borrowed connection.
 * @return True while open; false after close, transport/protocol failure, or for NULL.
 */
bool http_websocket_is_open(const http_websocket_t *socket);

/**
 * @brief Borrow the latest event identifier, including updates without dispatched data.
 * @param reader Borrowed reader.
 * @return Borrowed identifier until subsequent feed or destruction, or NULL for NULL reader.
 */
const string_t *http_event_reader_id(const http_event_reader_t *reader);

/**
 * @brief Read the latest valid retry field, including updates without dispatched data.
 * @param reader Borrowed reader.
 * @param milliseconds Required output delay, unchanged when no valid retry field exists.
 * @return True if a valid retry delay is available; false otherwise.
 */
bool http_event_reader_retry(const http_event_reader_t *reader, uint64_t *milliseconds);

#endif /* MARS_HTTP_H */
