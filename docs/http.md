# HTTP and HTTPS

The client and [web server](webserver.md) share `webmethod_t` from `webserver.h`:
`HTTP_GET`, `HTTP_POST`, `HTTP_PUT`, `HTTP_PATCH`, `HTTP_DELETE`, `HTTP_HEAD` and
`HTTP_OPTIONS`. Including `http.h` makes these types available.

The HTTP module consumes web services from C using opaque `http_client_t`,
`http_request_t` and `http_response_t` handles. Include `http.h`.
Transport uses system libcurl 7.86.0 or newer, with thread-safe global
initialisation; JSON and XML interpretation stays in the existing
[JSON](json.md) and native [XML](xml.md) modules.

## Build and security

Install the Debian/Ubuntu development package `libcurl4-openssl-dev`.
The Makefile discovers `libcurl` through pkg-config, falls back to `-lcurl`,
and checks availability with `make check-deps`. Static consumers must link
libcurl and its dependencies as well as MARS. See [building](building.md).

HTTPS always verifies the certificate chain and hostname, with TLS 1.2 as the
minimum. System trust is used by default. `http_client_set_ca_file` loads a
custom PEM bundle through the file module; passing NULL restores system trust.
Changing trust discards cached connections. There is no insecure verification
switch. Custom bundles require a libcurl TLS backend supporting CAINFO_BLOB;
the recommended OpenSSL build supports it.

Only absolute HTTP/HTTPS URLs are accepted. User information, fragments,
backslashes, spaces, control characters and raw non-ASCII URL bytes are
rejected. Use `http_url_encode` for individual path/query components, not a
whole URL. It percent-encodes UTF-8 bytes, including separators, and uses
`%20` rather than `+` for spaces.

Redirects and cookies are disabled by default and can be explicitly enabled
with the restrictions below. Environment proxies and netrc credentials remain
disabled. The module does not implement retries. Use HTTPS for secrets.
A plain HTTP URL does not protect credentials. Callers must authorise
destinations: this is **not** an SSRF sandbox, and localhost/private networks
are not blocked.

## Ownership and response handling

Create a reusable client, create a request, set its headers/body, and send it.
Requests copy URLs, headers, tokens and in-memory bodies. File uploads copy
the path and reopen the file on each send. Inputs and requests can be released
after a synchronous call returns. Free every returned response independently.

A non-NULL response means a completed HTTP exchange, **not** a successful
application request. Inspect `http_response_status` or `http_response_ok`
(the latter accepts 200–299). A 404 or 500 still has inspectable headers and
body. NULL indicates a transport, allocation, I/O, cancellation or limit
failure; inspect `http_client_error` and `http_client_error_text`.
Diagnostics do not include the supplied URL or bearer token.

Handles require external synchronisation. Different clients can be used on
different threads. Callbacks execute synchronously on the sending thread;
do not mutate, free or re-enter active client/request handles. Do not call
libcurl global cleanup while MARS clients exist. The module balances its own
global initialisation references.

GET, POST, PUT, PATCH, DELETE, HEAD and OPTIONS are supported. Bodies are
rejected for GET and HEAD. Header names are case-insensitive ASCII tokens;
values accept printable ASCII and horizontal tabs, never CR/LF or NUL.
Transport framing headers, Host, connection/upgrade and proxy authentication
headers are reserved. Setting a request header replaces its previous value.

Response headers are exposed read-only as a `dictionary_t`:
lower-case `string_t *` keys map to `array_t *` values containing
`string_t *` entries. Repeated fields remain separate; use
`http_response_header_count` and `http_response_header_at` for convenient
case-insensitive access. Interim-response fields are discarded; final
response trailers are included. Do not mutate or free borrowed dictionary
contents. Non-ASCII field values are rejected. Obsolete header folding may be
normalised by libcurl before delivery; any raw continuation that reaches the
MARS parser is rejected.

## Limits and streaming

Zero-initialise `http_limits_t`; zero fields select defaults, not infinity:

| Field | Default | Meaning |
| --- | --- | --- |
| `connect_timeout_ms` | 10,000 | Connection establishment deadline |
| `total_timeout_ms` | 30,000 | Whole exchange deadline |
| `max_body_bytes` | 16 MiB | Decoded response bytes, including streamed output |
| `max_header_bytes` | 64 KiB | Separate budgets for user request headers and cumulative response headers |
| `max_upload_bytes` | 16 MiB | Outgoing body bytes |

Header limits include interim responses and trailers. The outgoing budget
excludes transport-generated headers. Supported HTTP content encodings are
decoded by libcurl; the body limit applies **after decompression**.
Memory use includes libcurl, headers, copied request data and allocation
overhead; the byte budgets are not a process-memory cap. Body setters copy
before a client is chosen, so application code must also bound its input.
Timeouts depend on the installed resolver; a blocking system resolver may
not be interrupted promptly with libcurl's no-signal mode.

`http_client_send` retains a bounded binary body. `http_response_text`
strictly decodes UTF-8 without normalising Unicode; invalid UTF-8 fails.
JSON/XML helpers parse the buffered text explicitly without assuming a MIME
type or HTTP success. Binary getters use byte lengths, not NUL termination.

`http_client_stream` delivers fragments to `http_body_fn`, retaining no body.
Fragments may split UTF-8 characters; an XML reader can consume each fragment
with `xml_reader_feed` and finish only after successful transfer completion.
Return false to abort. `http_client_set_cancel` installs a cooperative
cancellation callback (true means cancel).

`http_client_download` writes at the current position in an already-open,
caller-owned `file_t`; it does not close or truncate the file.
Use `file_create` for a fresh/truncated destination. Failed downloads may
leave partial output. For atomic publication, write to a temporary file and
publish it only after checking transport success and the HTTP status.
Streaming sinks receive error-response bodies too.

`http_request_set_body_file` uses file-module reads and a fixed initial
length; the source must not change during the transfer. A shortened source
fails, while later appended bytes are not uploaded. Paths and CA bundles
use the file module's regular-file, no-follow opening policy. A failed
upload may already have changed server state; do not blindly retry it.

This is a synchronous client, not a web server, browser or asynchronous
scheduler. The service helpers below add SOAP, forms and multipart uploads,
authentication, session cookies, bounded redirects, event streams, WebSocket
messages and uncompressed unary gRPC. OAuth helpers cover client credentials
and refresh tokens, not an interactive browser authorisation workflow.
Proxy configuration is not exposed. Use `webserver.h` to implement a service.

## Complete examples using a real web service

[httpbin](https://httpbin.org/) is a public HTTP request/response test service.
Its [API specification](https://httpbin.org/spec.json) documents the echo,
status, redirect and binary endpoints used here. No API key is needed.

Each example below is a **complete C program**, including `main()`, response
validation, error handling and cleanup. The source files are in `scratch/`.
With no arguments they contact the real service over verified HTTPS.
An optional first argument overrides the URL for an authorised compatible
endpoint; the ordinary tests use this to run the exact same programs against
local fixtures. An unexpected response or transport failure returns
`EXIT_FAILURE`; successful output is shown below.

The README tests compile these actual source files with renamed entry points
and execute their `main()` functions. This keeps the documented programs and
the tests together. README examples run **last** in both the offline
`tests/http/test_http.c` and live `tests/http_live/test_http_live.c` suites.

### HTTPS GET: read an echoed query parameter

Save as `scratch/http_get.c` (already supplied):

```c
#include <stdlib.h>
#include "http.h"

/* Run the documented example; an optional URL allows deterministic offline tests. */
int main(int argc, char **argv)
{
    if (argc > 2) {
        string_printf("Usage: %s [URL]\n", argv[0]);
        return EXIT_FAILURE;
    }
    string_t *url = string_new_with(argc == 2 ? argv[1] : "https://httpbin.org/get?message=MARS");
    string_t *args_key = string_new_with("args"), *message_key = string_new_with("message");
    http_client_t *client = http_client_new();
    http_request_t *request = http_request_new(HTTP_GET, url);
    http_response_t *response = http_client_send(client, request);
    json_t *json = http_response_ok(response) ? http_response_json(response) : NULL;
    const json_t *args = json_object_get(json, args_key);
    const string_t *message = json_string_value(json_object_get(args, message_key));
    bool valid = http_response_status(response) == 200 && message &&
                 string_view_equals_literal(string_view_all(message), "MARS");
    if (valid)
        string_printf("status=%ld\nmessage=%S\n", http_response_status(response), message);
    else
        string_printf("request failed: status=%ld error=%d\n",
                      http_response_status(response), (int)http_client_error(client));
    json_free(json); string_free(args_key); string_free(message_key);
    http_response_free(response); http_request_free(request); http_client_free(client); string_free(url);
    return valid ? EXIT_SUCCESS : EXIT_FAILURE;
}
```

Build and run from the repository root:

```sh
make -j1 build/release/scratch/http_get
build/release/scratch/http_get
```

Program output (Make may also print compiler commands):

```text
status=200
message=MARS
```

### JSON POST: read the parsed JSON echo

Source: `scratch/http_post_json.c`.

```c
#include <stdlib.h>
#include "http.h"

/* Run the documented example; an optional URL allows deterministic offline tests. */
int main(int argc, char **argv)
{
    if (argc > 2) {
        string_printf("Usage: %s [URL]\n", argv[0]);
        return EXIT_FAILURE;
    }
    string_t *url = string_new_with(argc == 2 ? argv[1] : "https://httpbin.org/post");
    string_t *text = string_new_with("{\"answer\":42}");
    string_t *json_key = string_new_with("json"), *answer_key = string_new_with("answer");
    json_t *payload = json_from_text(text);
    http_client_t *client = http_client_new();
    http_request_t *request = http_request_new(HTTP_POST, url);
    bool ready = http_request_set_json(request, payload);
    http_response_t *response = ready ? http_client_send(client, request) : NULL;
    json_t *reply = http_response_ok(response) ? http_response_json(response) : NULL;
    const json_t *echo = json_object_get(reply, json_key);
    const string_t *answer = json_number_text(json_object_get(echo, answer_key));
    bool valid = http_response_status(response) == 200 && answer &&
                 string_view_equals_literal(string_view_all(answer), "42");
    if (valid)
        string_printf("status=%ld\nanswer=%S\n", http_response_status(response), answer);
    else
        string_printf("request failed: status=%ld error=%d\n",
                      http_response_status(response), (int)http_client_error(client));
    json_free(reply); json_free(payload);
    string_free(text); string_free(json_key); string_free(answer_key);
    http_response_free(response); http_request_free(request); http_client_free(client); string_free(url);
    return valid ? EXIT_SUCCESS : EXIT_FAILURE;
}
```

Build and run:

```sh
make -j1 build/release/scratch/http_post_json
build/release/scratch/http_post_json
```

Program output:

```text
status=200
answer=42
```

### XML POST: recover XML from the service's JSON envelope

httpbin's `/post` response is JSON even when the request is XML. Its `data`
field contains the original XML text. Decode that envelope first, then parse
the XML; do not call `http_response_xml` on a JSON response.

Source: `scratch/http_post_xml.c`.

```c
#include <stdlib.h>
#include "http.h"

/* Run the documented example; an optional URL allows deterministic offline tests. */
int main(int argc, char **argv)
{
    if (argc > 2) {
        string_printf("Usage: %s [URL]\n", argv[0]);
        return EXIT_FAILURE;
    }
    string_t *url = string_new_with(argc == 2 ? argv[1] : "https://httpbin.org/post");
    string_t *text = string_new_with("<answer>42</answer>");
    string_t *data_key = string_new_with("data");
    xml_t *document = xml_from_text(text);
    http_client_t *client = http_client_new();
    http_request_t *request = http_request_new(HTTP_POST, url);
    bool ready = http_request_set_xml(request, document);
    http_response_t *response = ready ? http_client_send(client, request) : NULL;
    /* httpbin wraps the echoed XML body in the JSON field "data". */
    json_t *envelope = http_response_ok(response) ? http_response_json(response) : NULL;
    const string_t *echo = json_string_value(json_object_get(envelope, data_key));
    xml_t *reply = echo ? xml_from_text(echo) : NULL;
    string_t *output = reply ? xml_to_string(reply) : NULL;
    bool valid = http_response_status(response) == 200 && output &&
                 string_view_equals_literal(string_view_all(output), "<answer>42</answer>");
    if (valid)
        string_printf("status=%ld\n%S\n", http_response_status(response), output);
    else
        string_printf("request failed: status=%ld error=%d\n",
                      http_response_status(response), (int)http_client_error(client));
    string_free(output); xml_free(reply); xml_free(document); json_free(envelope);
    string_free(text); string_free(data_key);
    http_response_free(response); http_request_free(request); http_client_free(client); string_free(url);
    return valid ? EXIT_SUCCESS : EXIT_FAILURE;
}
```

Build and run:

```sh
make -j1 build/release/scratch/http_post_xml
build/release/scratch/http_post_xml
```

Program output:

```text
status=200
<answer>42</answer>
```

For creating a service rather than calling one, see [Web server](webserver.md).

### Running the live tests

The live suite starts the native MARS `webserver` on ephemeral loopback ports
and calls it through the HTTP client. It checks status codes, disabled
redirects, every binary octet, content types and malformed JSON, then runs all
three README programs above with local URL arguments. Each child server is
reaped before the next case starts.

These tests run during ordinary `make test`; there is no opt-in flag and no
external-service dependency. Run them separately with:

```sh
make -j1 test-http-live
```

Successful README example output (the harness also prints test results):

```text
status=200
message=MARS
status=200
answer=42
status=200
<answer>42</answer>
```

The suite needs local sockets and process creation, but no internet access,
DNS, Python fixture or external CA store. It uses plain HTTP because the
native web server does not implement TLS. The separate HTTP suite retains
local HTTPS certificate and hostname-verification tests.

Running the standalone programs above without a URL argument still contacts
httpbin.org and therefore requires internet access and a working system CA
store. That public service sees the fixed example payload and connection
metadata, including your public IP address. Never send personal data or
production credentials to a public echo service.

## Public API

All declarations, parameters, return values and ownership rules are documented
in [http.h](../include/http.h). The complete function reference follows.

### `http_client_new`

Create a synchronous HTTP/HTTPS client with secure defaults.

`http_client_t *http_client_new(void);`


Returns: Owned client, or NULL on allocation or libcurl initialisation failure; release with http_client_free.

### `http_client_free`

Destroy a client and its connection cache.

`void http_client_free(http_client_t *client);`

- `client`: Owned idle client to destroy; NULL is harmless.

### `http_client_set_limits`

Set transfer deadlines and resource limits.

`bool http_client_set_limits(http_client_t *client, const http_limits_t *limits);`

- `client`: Idle client to modify.
- `limits`: Required controls; zero fields select defaults. Timeouts must fit a positive long.

Returns: True on success; false for invalid input. Previous settings remain on failure.

### `http_client_set_ca_file`

Load a custom PEM trust bundle through the file module.

`bool http_client_set_ca_file(http_client_t *client, const string_t *path);`

- `client`: Idle client to modify; verification remains enabled.
- `path`: Borrowed regular-file path, or NULL to restore system trust. Symlinks are rejected; maximum 4 MiB.

Returns: True on success; false on invalid input, allocation or file I/O failure.

### `http_client_set_cancel`

Install an optional cooperative cancellation callback.

`void http_client_set_cancel(http_client_t *client, http_cancel_fn callback, void *user_data);`

- `client`: Idle client to modify; NULL is harmless.
- `callback`: Borrowed function pointer, or NULL to disable cancellation.
- `user_data`: Borrowed context retained until replaced or client destruction.

### `http_client_error`

Read the last transfer error category.

`http_error_t http_client_error(const http_client_t *client);`

- `client`: Borrowed client.

Returns: Last error, or HTTP_ERROR_ARGUMENT for NULL. HTTP status errors are not transport errors.

### `http_client_error_text`

Borrow the last transfer diagnostic.

`const string_t *http_client_error_text(const http_client_t *client);`

- `client`: Borrowed client.

Returns: Borrowed text until the next transfer or client destruction, or NULL if unavailable.

### `http_request_new`

Create a request with a copied absolute HTTP or HTTPS URL.

`http_request_t *http_request_new(webmethod_t method, const string_t *url);`

- `method`: Supported HTTP method.
- `url`: Borrowed ASCII URL; percent-encode non-ASCII bytes. User information, fragments and control characters are rejected.

Returns: Owned request, or NULL for invalid input or allocation failure; release with http_request_free.

### `http_request_free`

Release a request and its copied headers and body.

`void http_request_free(http_request_t *request);`

- `request`: Owned idle request; NULL is harmless.

### `http_request_set_header`

Copy or replace a request header using case-insensitive names.

`bool http_request_set_header(http_request_t *request, const string_t *name, const string_t *value);`

- `request`: Idle request to modify.
- `name`: Borrowed ASCII token. Host, framing, connection, Expect and proxy-authorization headers are managed internally.
- `value`: Borrowed printable ASCII or tab value. CR, LF, NUL and other controls are rejected.

Returns: True on success; false for invalid input or allocation failure.

### `http_request_set_bearer`

Set the Authorization header to a copied bearer token.

`bool http_request_set_bearer(http_request_t *request, const string_t *token);`

- `request`: Idle request to modify.
- `token`: Borrowed non-empty printable ASCII token without whitespace; never included in diagnostics.

Returns: True on success; false on invalid input or allocation failure.

### `http_request_set_body`

Copy a binary request body.

`bool http_request_set_body(http_request_t *request, const void *data, size_t size, const string_t *content_type);`

- `request`: Idle request; GET and HEAD do not accept bodies.
- `data`: Borrowed bytes; NULL is allowed only for zero size.
- `size`: Number of bytes, not characters.
- `content_type`: Optional borrowed Content-Type value; NULL leaves any existing header unchanged.

Returns: True on success; false on invalid input or allocation failure, leaving the old body unchanged.

### `http_request_set_text`

Copy a string's exact encoded bytes as the request body.

`bool http_request_set_text(http_request_t *request, const string_t *text, const string_t *content_type);`

- `request`: Idle request accepting a body.
- `text`: Borrowed text, including any embedded NUL bytes.
- `content_type`: Optional borrowed Content-Type value.

Returns: True on success; false on invalid input or allocation failure.

### `http_request_set_json`

Serialise a JSON body and set application/json.

`bool http_request_set_json(http_request_t *request, const json_t *json);`

- `request`: Idle request accepting a body.
- `json`: Borrowed JSON tree.

Returns: True on success; false on invalid input or serialisation/allocation failure.

### `http_request_set_xml`

Serialise an XML body and set application/xml.

`bool http_request_set_xml(http_request_t *request, const xml_t *xml);`

- `request`: Idle request accepting a body.
- `xml`: Borrowed XML document or root element.

Returns: True on success; false on invalid input or serialisation/allocation failure.

### `http_request_set_body_file`

Select a file to upload incrementally through the file module.

`bool http_request_set_body_file(http_request_t *request, const string_t *path, const string_t *content_type);`

- `request`: Idle request accepting a body.
- `path`: Borrowed regular-file path; copied now and opened at each transfer. Symlinks are rejected.
- `content_type`: Optional borrowed Content-Type value.

Returns: True on success; file errors are reported when sending. The previous body remains on setter failure.

### `http_client_send`

Perform a request and buffer its binary response body.

`http_response_t *http_client_send(http_client_t *client, const http_request_t *request);`

- `client`: Idle client; used synchronously and not concurrently.
- `request`: Borrowed request, unchanged and reusable.

Returns: Owned complete response for any HTTP status; NULL on transport, limit, cancellation or allocation failure.

### `http_client_stream`

Stream a response body without retaining it.

`http_response_t *http_client_stream(http_client_t *client, const http_request_t *request, http_body_fn callback, void *user_data);`

- `client`: Idle client; callbacks must not re-enter, modify or destroy this client or request.
- `request`: Borrowed request.
- `callback`: Required sink accepting borrowed bytes; false aborts the transfer.
- `user_data`: Borrowed sink context, used only during this call.

Returns: Owned response metadata, or NULL on failure. The sink may have consumed partial data, including HTTP error bodies.

### `http_client_download`

Stream a response to a caller-opened file.

`http_response_t *http_client_download(http_client_t *client, const http_request_t *request, file_t *destination);`

- `client`: Idle client.
- `request`: Borrowed request.
- `destination`: Borrowed open writable file; written at its current position and neither closed nor deleted.

Returns: Owned response metadata, or NULL on failure. Partial data and HTTP error bodies can remain in the destination.

### `http_response_free`

Release response headers and any buffered body.

`void http_response_free(http_response_t *response);`

- `response`: Owned response; NULL is harmless.

### `http_response_status`

Read the final HTTP status.

`long http_response_status(const http_response_t *response);`

- `response`: Borrowed response.

Returns: HTTP status code, or zero for NULL.

### `http_response_ok`

Test for an HTTP success status.

`bool http_response_ok(const http_response_t *response);`

- `response`: Borrowed response.

Returns: True for status 200 through 299; false otherwise.

### `http_response_body`

Borrow buffered body bytes without assuming text encoding.

`const void *http_response_body(const http_response_t *response);`

- `response`: Borrowed response.

Returns: Borrowed bytes until response destruction, or NULL for an empty or streamed body.

### `http_response_body_size`

Read the number of buffered body bytes.

`size_t http_response_body_size(const http_response_t *response);`

- `response`: Borrowed response.

Returns: Buffered byte count; zero for NULL or streamed responses.

### `http_response_received_size`

Read the number of body bytes delivered.

`size_t http_response_received_size(const http_response_t *response);`

- `response`: Borrowed response.

Returns: Received byte count, including streamed bytes; zero for NULL.

### `http_response_text`

Copy buffered body bytes as exact UTF-8, without normalisation.

`string_t *http_response_text(const http_response_t *response);`

- `response`: Borrowed buffered response; no HTTP-status or Content-Type interpretation is performed.

Returns: Owned text released with string_free, or NULL for streaming mode, malformed UTF-8 or allocation failure.

### `http_response_json`

Parse a buffered UTF-8 body with the JSON module.

`json_t *http_response_json(const http_response_t *response);`

- `response`: Borrowed response; check status and Content-Type separately.

Returns: Owned JSON tree released with json_free, or NULL on decoding/parsing failure.

### `http_response_xml`

Parse a buffered body with the native XML module.

`xml_t *http_response_xml(const http_response_t *response);`

- `response`: Borrowed response; check status and Content-Type separately.

Returns: Owned XML document released with xml_free, or NULL on decoding/parsing failure.

### `http_response_headers`

Borrow final response headers, including trailers.

`const dictionary_t *http_response_headers(const http_response_t *response);`

- `response`: Borrowed response.

Returns: Read-only dictionary of lower-case string_t * keys and array_t * values containing string_t * entries, or NULL.

### `http_response_header_count`

Count repeated values for one case-insensitive header name.

`size_t http_response_header_count(const http_response_t *response, const string_t *name);`

- `response`: Borrowed response.
- `name`: Borrowed ASCII header name.

Returns: Number of values, or zero if absent or invalid.

### `http_response_header_at`

Borrow one header value without combining repeated fields.

`const string_t *http_response_header_at(const http_response_t *response, const string_t *name, size_t index);`

- `response`: Borrowed response.
- `name`: Borrowed case-insensitive header name.
- `index`: Zero-based value position.

Returns: Borrowed value until response destruction, or NULL if absent or out of range.

### `http_url_encode`

Percent-encode one URL query value or path segment.

`string_t *http_url_encode(const string_t *text);`

- `text`: Borrowed text; its encoded bytes are escaped, retaining only ASCII unreserved bytes. Spaces become %20.

Returns: Owned ASCII text released with string_free, or NULL on invalid input or allocation failure.

## Forms, SOAP and event streams

XML and JSON represent data; they do not themselves implement SOAP, an
authentication flow or an event-stream framing protocol. These helpers remain
in the HTTP module.

- Forms: create `http_form_new`, append copied text fields with
  `http_form_add_text`, and use `http_request_set_form`. Duplicate fields retain
  their order. `http_form_encode` escapes UTF-8 bytes and writes spaces as `+`.
  Release the builder with `http_form_free`.
- Uploads: `http_form_add_file` records an explicit transmitted filename, media
  type and file-module path. `http_request_set_multipart` reads files through
  `file_t` into a bounded snapshot: 16 MiB by default, at most 64 MiB.
  The snapshot is buffered, not streamed. Metadata must be printable ASCII
  without quotes/backslashes in names and filenames; arbitrary Unicode field
  values are supported. Symlink opening follows the file module's restrictions.
- SOAP: `http_soap_envelope` copies an XML payload into a SOAP 1.1 or 1.2
  envelope. `http_request_set_soap` prepares a POST atomically, selecting
  `text/xml` plus `SOAPAction` for 1.1, or `application/soap+xml` with an
  action parameter for 1.2. Reusing a request removes stale SOAPAction headers.
  Read the response with `http_response_xml`, including on HTTP 500, then
  `http_soap_parse`. Check `http_soap_is_fault`, `http_soap_fault_code` and
  `http_soap_fault_reason`; `http_soap_body` exposes the borrowed Body tree,
  including application-specific detail. Release with `http_soap_free`.
  Namespace aliases and default namespaces are recognised. Incoming non-empty
  SOAP headers are rejected rather than silently ignoring required processing.
  WSDL generation, SOAP encoding, WS-Security, attachments and header handlers
  are not implemented.
- Server-Sent Events: `http_event_reader_new`, `http_event_reader_feed`,
  `http_event_reader_finish` and `http_event_reader_free` provide a bounded
  incremental reader. Feed the bytes from an `http_client_stream` callback.
  The reader handles split UTF-8, an initial BOM, CR/LF/CRLF, comments,
  multiline data, event types, persistent identifiers and retry fields.
  Invalid UTF-8 is rejected, rather than replaced. EOF discards an event
  without its terminating blank line. The application checks the service's
  media type, manages reconnection and sends Last-Event-ID; the reader does
  not open connections. The byte budget applies separately to a line and
  accumulated data/type/identifier; storage overhead is additional.

### Authentication and sessions

`http_request_set_basic` sets an explicit Basic header; use HTTPS.
`http_oauth2_client_credentials` and `http_oauth2_refresh` implement those two
OAuth token exchanges, including form-encoding credentials before Basic
authentication. HTTPS is required except for numeric loopback HTTP test
endpoints. Responses, including OAuth errors, are returned normally.
Validate the token JSON and HTTP status before calling `http_request_set_bearer`.
Consent, authorisation-code/PKCE flows, secure token storage, expiry scheduling
and automatic refresh/retry remain application responsibilities.

`http_hmac_sha256` returns a Base64 HMAC of caller-defined canonical bytes.
It does **not** invent a signing profile: the application must implement the
service's canonicalisation, timestamp, nonce and header rules. This is not
automatic AWS Signature V4, HTTP Message Signatures or WS-Security support.
Credential-bearing strings and request headers are ordinary process memory,
not locked secure-memory containers.

`http_client_set_identity_files` loads PEM client certificates and private
keys through the file module (4 MiB each). An optional password unlocks an
encrypted key. TLS chain and hostname verification remain enabled. Replacing
or clearing an identity discards cached connections and cookies. Passing NULL
for both paths clears it. The OpenSSL libcurl backend is recommended.

`http_client_set_cookies` opts into an in-memory jar, disabled by default;
`http_client_clear_cookies` empties it. No cookie files are read or written.
Domain/path/secure-cookie rules are enforced by libcurl. A retained jar
exceeding the header budget is discarded and the request fails. Trust or
identity changes discard the jar.

`http_client_set_redirects` enables up to 16 same-origin hops for buffered
GET/HEAD requests. The default is zero. Scheme, host and effective port must
match; cross-origin destinations are rejected before a follow-up request.
Timeout and response byte budgets cover the entire chain. Streaming and
non-GET/HEAD requests still return redirects without replaying them.
Redirects do not weaken TLS checks or enable ambient proxies.

### Runnable encoding example

This complete offline program builds a form and SOAP envelope and consumes an
event stream. It is also `scratch/http_services.c`; its README test runs after
the ordinary HTTP tests.

```c
#include "http.h"
#include <stdio.h>
#include <stdlib.h>

static bool show_event(const string_t *event, const string_t *data, const string_t *id, bool has_retry,
                       uint64_t retry_ms, void *context)
{
    (void)id;
    (void)has_retry;
    (void)retry_ms;
    unsigned *count = context;
    ++*count;
    string_printf("%S: %S\n", event, data);
    return true;
}

int main(void)
{
    http_form_t *form = http_form_new();
    string_t *name = string_new_with("message"), *value = string_new_with("hello & goodbye");
    bool ok = form && name && value && http_form_add_text(form, name, value);
    string_t *encoded = ok ? http_form_encode(form) : NULL;
    if (encoded)
        string_printf("%S\n", encoded);
    else
        ok = false;

    string_t *text = string_new_with("<answer>42</answer>");
    xml_t *document = text ? xml_from_text(text) : NULL;
    xml_t *envelope = document ? http_soap_envelope(HTTP_SOAP_12, xml_child_at(document, 0), NULL) : NULL;
    http_soap_t *soap = envelope ? http_soap_parse(envelope) : NULL;
    if (soap)
        printf("SOAP body children: %zu\n", xml_child_count(http_soap_body(soap)));
    else
        ok = false;

    unsigned events = 0;
    http_event_reader_t *reader = http_event_reader_new(1024, show_event, &events);
    const char event[] = "event: answer\ndata: 42\n\n";
    ok = reader && http_event_reader_feed(reader, event, sizeof(event) - 1) && http_event_reader_finish(reader) &&
         events == 1 && ok;
    http_event_reader_free(reader);
    http_soap_free(soap);
    xml_free(envelope);
    xml_free(document);
    string_free(text);
    string_free(encoded);
    string_free(name);
    string_free(value);
    http_form_free(form);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
```

Output:

```text
message=hello+%26+goodbye
SOAP body children: 1
answer: 42
```

The formats follow [SOAP 1.1](https://www.w3.org/TR/2000/NOTE-SOAP-20000508/),
[SOAP 1.2](https://www.w3.org/TR/soap12-part2/),
[OAuth 2.0](https://www.rfc-editor.org/rfc/rfc6749.html) and
[Server-Sent Events](https://html.spec.whatwg.org/multipage/server-sent-events.html).

## Unary gRPC over HTTP/2

Include `protobuf.h` as well as `http.h` to construct or inspect wire messages.

`http_request_set_grpc` prepares a POST using an owned, copied
[Protocol Buffers](protobuf.md) message. The request URL identifies the service
method, conventionally `/package.Service/Method`. Add bearer/Basic credentials
or application metadata through the ordinary HTTP request setters before
sending. This helper requires HTTP/2; cleartext endpoints use HTTP/2 prior
knowledge, while HTTPS endpoints negotiate HTTP/2 through TLS. A libcurl build
without HTTP/2 support fails explicitly rather than silently using HTTP/1.

Parse a completed response with `http_response_grpc`, then inspect
`http_grpc_status`, `http_grpc_message` and `http_grpc_payload`. A gRPC
application error is not the same as an HTTP error: HTTP 200 can carry a
non-zero gRPC status. Status messages are percent-decoded and checked as
UTF-8. `http_grpc_free` releases the parsed result independently of the HTTP
response. Ordinary HTTP accessors retain response metadata and trailers.

Only **uncompressed unary calls** are supported: one request message and,
on success, exactly one response message. Multiple response frames, compressed
messages, malformed lengths/statuses and non-Protocol-Buffers media types are
rejected. Streaming RPCs, gRPC-Web, generated stubs, schema reflection and
automatic retry are not implemented. The HTTP timeout bounds the operation;
server-side deadline metadata, if wanted, must be set explicitly by the
application. The wire format follows the
[gRPC HTTP/2 specification](https://github.com/grpc/grpc/blob/master/doc/PROTOCOL-HTTP2.md).

### Runnable unary example

This is `scratch/http_grpc.c`. Run it with the HTTP/HTTPS URL of a compatible
unary echo method accepting an unsigned integer field 1. The test runner
supplies an offline HTTP/2 fixture.

```c
#include "http.h"
#include "protobuf.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc != 2)
        return EXIT_FAILURE;
    string_t *url = string_new_with(argv[1]);
    http_client_t *client = http_client_new();
    http_request_t *request = http_request_new(HTTP_POST, url);
    protobuf_t *message = protobuf_new(1024, 16);
    bool ready = client && request && message && protobuf_add_integer(message, 1, PROTOBUF_VARINT, 150) &&
                 http_request_set_grpc(request, message);
    http_response_t *response = ready ? http_client_send(client, request) : NULL;
    http_grpc_t *result = response ? http_response_grpc(response, 16) : NULL;
    uint64_t value = 0;
    bool ok = http_grpc_status(result) == 0 && protobuf_integer(http_grpc_payload(result), 0, &value);
    if (ok)
        printf("gRPC status: %d\nReply: %" PRIu64 "\n", http_grpc_status(result), value);
    http_grpc_free(result);
    http_response_free(response);
    protobuf_free(message);
    http_request_free(request);
    http_client_free(client);
    string_free(url);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
```

Output from the echo method:

```text
gRPC status: 0
Reply: 150
```

## WebSocket messages

Include `array.h` when inspecting or freeing returned byte arrays.

`http_websocket_open` upgrades an HTTP/HTTPS GET request, mapping its scheme
internally to ws/wss. It uses the client's verified TLS trust, optional client
certificate, timeouts, cancellation callback and byte limits, plus explicit
request headers such as Authorization or Origin. Redirects are never followed.
The libcurl build must support WebSockets; absence is reported as an error.

The connection reserves its client until `http_websocket_free`; do not free,
reconfigure or send other requests through that client while it is reserved.
The request can be freed after opening. Cookie jars are **not** shared with the
WebSocket handshake; set an explicit Cookie header if the application needs
one. WebSocket subprotocol negotiation and extensions/compression are not
implemented, and unsolicited extension/subprotocol responses are rejected.

Use `http_websocket_send` for a complete text or binary message and
`http_websocket_receive` for an owned byte array containing the next complete
message. UTF-8 text is validated; binary payloads can contain any bytes.
Fragmented messages are reassembled under the body limit, with control frames
handled between fragments. libcurl answers pings automatically.

Each send/receive has its own total-timeout budget. A transport, timeout or
protocol failure stops messaging; inspect the client's error and
`http_websocket_is_open`. A peer close returns NULL from receive with no error
when its close frame is valid. `http_websocket_close` sends a validated close
code/reason and stops messaging, but does not wait for acknowledgement.
Always call `http_websocket_free` afterwards to release the transport.

### Runnable WebSocket example

This is `scratch/http_websocket.c`. Supply an HTTP/HTTPS URL whose endpoint
supports a WebSocket echo upgrade; use HTTPS for credentials or private data.
The README test runs against the local WebSocket fixture.

```c
#include "http.h"
#include "array.h"
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc != 2)
        return EXIT_FAILURE;
    string_t *url = string_new_with(argv[1]);
    http_client_t *client = http_client_new();
    http_request_t *request = http_request_new(HTTP_GET, url);
    http_websocket_t *socket = client && request ? http_websocket_open(client, request) : NULL;
    bool text = false;
    bool sent = socket && http_websocket_send(socket, true, "MARS", 4);
    array_t *reply = sent ? http_websocket_receive(socket, &text) : NULL;
    string_t *output = string_new();
    bool ok = reply && text && output && !string_append_utf8_exact(output, array_get(reply, 0), array_size(reply));
    if (ok)
        string_printf("WebSocket echo: %S\n", output);
    if (http_websocket_is_open(socket))
        ok = http_websocket_close(socket, 1000, NULL) && ok;
    string_free(output);
    array_destroy(reply);
    http_websocket_free(socket);
    http_request_free(request);
    http_client_free(client);
    string_free(url);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
```

Output:

```text
WebSocket echo: MARS
```

For SSE reconnection state even when no data event is dispatched, use
`http_event_reader_id` and `http_event_reader_retry`. These accessors do not
open connections or schedule retries.

## Extended API reference

### `http_form_new`

Create an empty reusable form.

`http_form_t *http_form_new(void);`


Returns: Owned form, or NULL; release with http_form_free().

### `http_form_free`

Release a form and copied parts.

`void http_form_free(http_form_t *form);`

- `form`: Owned form; NULL is harmless.

### `http_form_add_text`

Copy a text form field.

`bool http_form_add_text(http_form_t *form, const string_t *name, const string_t *value);`

- `form`: Form to modify.
- `name`: Borrowed field name.
- `value`: Borrowed text, copied without normalisation.

Returns: True on success; false on invalid input or allocation failure.

### `http_form_add_file`

Add a file part, copying metadata and deferring reading until multipart preparation.

`bool http_form_add_file(http_form_t *form, const string_t *name, const string_t *filename, const string_t *content_type, const string_t *path);`

- `form`: Form to modify.
- `name`: Borrowed printable ASCII field name without quotes or backslashes.
- `filename`: Borrowed printable ASCII transmitted filename without quotes or backslashes; never inferred from path.
- `content_type`: Borrowed printable ASCII media type.
- `path`: Borrowed path; file module rejects symlinks when opened.

Returns: True on success, false on invalid input or allocation failure.

### `http_form_encode`

Encode text fields as application/x-www-form-urlencoded; spaces become +.

`string_t *http_form_encode(const http_form_t *form);`

- `form`: Borrowed text-only form; file parts are rejected.

Returns: Owned encoded string, or NULL; release with string_free().

### `http_request_set_form`

Set a copied URL-encoded form body and Content-Type.

`bool http_request_set_form(http_request_t *request, const http_form_t *form);`

- `request`: Request accepting a body.
- `form`: Borrowed text-only form.

Returns: True on success, false on failure.

### `http_request_set_multipart`

Build a bounded multipart/form-data snapshot, reading file parts through file_t.

`bool http_request_set_multipart(http_request_t *request, const http_form_t *form, size_t max_bytes);`

- `request`: Request accepting a body.
- `form`: Borrowed form; all contents are copied now, not streamed during sending.
- `max_bytes`: Encoded body budget; zero selects 16 MiB; maximum 64 MiB.

Returns: True on success; false leaves the old request body unchanged.

### `http_request_set_basic`

Set an explicit Basic Authorization header.

`bool http_request_set_basic(http_request_t *request, const string_t *username, const string_t *password);`

- `request`: Request to modify; use HTTPS for credentials.
- `username`: Borrowed UTF-8 username without colon or NUL.
- `password`: Borrowed UTF-8 password without NUL.

Returns: True on success; false on invalid input or allocation failure.

### `http_hmac_sha256`

Sign caller-defined canonical bytes using HMAC-SHA256 and standard Base64.

`string_t *http_hmac_sha256(const string_t *canonical, const void *key, size_t key_size);`

- `canonical`: Borrowed exact message bytes; this helper does not define a service-specific signing scheme.
- `key`: Borrowed secret bytes.
- `key_size`: Secret byte count, greater than zero.

Returns: Owned Base64 signature, or NULL; release with string_free().

### `http_client_set_cookies`

Enable an in-memory cookie session, or disable it and discard all cookies.

`bool http_client_set_cookies(http_client_t *client, bool enabled);`

- `client`: Idle client to modify; no cookie files are read or written.
- `enabled`: True to enable; disabled by default. Cookies are scoped by libcurl domain/path rules.

Returns: True on success, false on failure.

### `http_client_clear_cookies`

Discard all session cookies.

`bool http_client_clear_cookies(http_client_t *client);`

- `client`: Idle client.

Returns: True on success, false on failure.

### `http_client_set_redirects`

Follow bounded same-origin redirects for buffered GET/HEAD requests only.

`bool http_client_set_redirects(http_client_t *client, unsigned max_hops);`

- `client`: Idle client.
- `max_hops`: Zero disables redirects (default); maximum 16. Streaming and other methods return redirects unchanged.

Returns: True on success, false on invalid input. Cross-origin redirects fail without sending credentials.

### `http_client_set_identity_files`

Load a PEM TLS client identity through the file module and reset cached connections.

`bool http_client_set_identity_files(http_client_t *client, const string_t *certificate, const string_t *private_key, const string_t *password);`

- `client`: Idle client.
- `certificate`: Borrowed PEM certificate path, or NULL together with private_key to clear identity.
- `private_key`: Borrowed PEM key path; each file is limited to 4 MiB and symlinks are rejected.
- `password`: Optional borrowed key password without NUL; copied.

Returns: True on success; false leaves the old identity unchanged. TLS verification remains enabled.

### `http_oauth2_client_credentials`

Request an OAuth2 client-credentials token using form-encoded Basic client authentication.

`http_response_t *http_oauth2_client_credentials(http_client_t *client, const string_t *url, const string_t *client_id, const string_t *secret, const string_t *scope);`

- `client`: Idle HTTP client.
- `url`: Borrowed HTTPS token endpoint; HTTP is allowed only for numeric loopback testing.
- `client_id`: Borrowed OAuth client identifier.
- `secret`: Borrowed client secret.
- `scope`: Optional borrowed space-separated scope.

Returns: Owned HTTP response, including OAuth error responses; NULL on failure. Parse and validate token JSON.

### `http_oauth2_refresh`

Exchange a refresh token; token storage, expiry scheduling and consent remain application responsibilities.

`http_response_t *http_oauth2_refresh(http_client_t *client, const string_t *url, const string_t *client_id, const string_t *secret, const string_t *refresh_token, const string_t *scope);`

- `client`: Idle HTTP client.
- `url`: Borrowed HTTPS token endpoint; numeric loopback HTTP is allowed for tests.
- `client_id`: Borrowed client identifier.
- `secret`: Borrowed client secret.
- `refresh_token`: Borrowed refresh token.
- `scope`: Optional scope.

Returns: Owned HTTP response including OAuth errors, or NULL on failure.

### `http_soap_envelope`

Build a SOAP envelope using the native XML module.

`xml_t *http_soap_envelope(http_soap_version_t version, const xml_t *payload, const xml_t *header);`

- `version`: SOAP 1.1 or 1.2.
- `payload`: Borrowed single body element, deep-copied.
- `header`: Optional borrowed single header block, deep-copied.

Returns: Owned envelope element, or NULL; release with xml_free.

### `http_request_set_soap`

Prepare a POST with the appropriate SOAP media type and action.

`bool http_request_set_soap(http_request_t *request, http_soap_version_t version, const string_t *action, const xml_t *payload, const xml_t *header);`

- `request`: POST request to modify atomically.
- `version`: SOAP version.
- `action`: Borrowed printable ASCII action without quotes or backslashes; NULL selects an empty action.
- `payload`: Borrowed body element.
- `header`: Optional borrowed header block.

Returns: True on success; false leaves the request unchanged.

### `http_soap_parse`

Validate and copy an envelope, recognising namespace aliases and SOAP faults.

`http_soap_t *http_soap_parse(const xml_t *document);`

- `document`: Borrowed XML document or envelope. Header blocks are rejected because no mustUnderstand handler is registered.

Returns: Owned result, or NULL for malformed or unsupported envelopes; release with http_soap_free.

### `http_soap_free`

Release an envelope and extracted fault text.

`void http_soap_free(http_soap_t *soap);`

- `soap`: Owned envelope; NULL is harmless.

### `http_soap_body`

Borrow the validated Body element, including its ordered payload children.

`const xml_t *http_soap_body(const http_soap_t *soap);`

- `soap`: Borrowed envelope.

Returns: Borrowed Body until soap is freed, or NULL.

### `http_soap_is_fault`

Report whether the body contains a recognised SOAP Fault.

`bool http_soap_is_fault(const http_soap_t *soap);`

- `soap`: Borrowed envelope.

Returns: True for a SOAP Fault; false for normal or NULL envelopes.

### `http_soap_fault_code`

Borrow the SOAP fault code text without interpreting application-specific codes.

`const string_t *http_soap_fault_code(const http_soap_t *soap);`

- `soap`: Borrowed envelope.

Returns: Borrowed code until soap is freed, or NULL when absent.

### `http_soap_fault_reason`

Borrow the first SOAP fault reason, preserving its text.

`const string_t *http_soap_fault_reason(const http_soap_t *soap);`

- `soap`: Borrowed envelope.

Returns: Borrowed reason until soap is freed, or NULL when absent.

### `http_event_reader_new`

Create a bounded SSE reader without automatic reconnection.

`http_event_reader_t *http_event_reader_new(size_t max_bytes, http_event_fn callback, void *user_data);`

- `max_bytes`: Maximum bytes per line and accumulated event; zero selects 1 MiB; maximum 64 MiB.
- `callback`: Required event consumer.
- `user_data`: Borrowed callback context.

Returns: Owned reader, or NULL; release with http_event_reader_free.

### `http_event_reader_free`

Release reader state.

`void http_event_reader_free(http_event_reader_t *reader);`

- `reader`: Owned idle reader; NULL is harmless.

### `http_event_reader_feed`

Consume a byte fragment, handling split UTF-8, BOM, CRLF and multi-line events.

`bool http_event_reader_feed(http_event_reader_t *reader, const void *data, size_t size);`

- `reader`: Reader to update.
- `data`: Borrowed bytes; NULL only when size is zero.
- `size`: Byte count.

Returns: True on success; false on invalid UTF-8, limits, allocation failure or callback rejection; failure is sticky.

### `http_event_reader_finish`

Finish input, discarding an event not terminated by a blank line.

`bool http_event_reader_finish(http_event_reader_t *reader);`

- `reader`: Reader to finish; no subsequent feed calls are allowed.

Returns: True on valid EOF; false for previous errors or invalid trailing UTF-8.

### `http_request_set_grpc`

Prepare an uncompressed unary gRPC POST and require HTTP/2.

`bool http_request_set_grpc(http_request_t *request, const protobuf_t *message);`

- `request`: POST request to modify; add authentication and metadata with ordinary HTTP setters.
- `message`: Borrowed Protocol Buffers message, serialised and copied.

Returns: True on success, false on invalid input or allocation failure; do not send on failure.

### `http_response_grpc`

Validate a buffered unary gRPC response, including status and framing.

`http_grpc_t *http_response_grpc(const http_response_t *response, size_t max_fields);`

- `response`: Borrowed response from HTTP/2 with application/grpc media type.
- `max_fields`: Protocol Buffers field limit; zero selects 4096.

Returns: Owned result including non-zero gRPC statuses, or NULL on malformed/unsupported responses.

### `http_grpc_free`

Release a parsed gRPC result.

`void http_grpc_free(http_grpc_t *result);`

- `result`: Owned result; NULL is harmless.

### `http_grpc_status`

Read the gRPC application status independently of HTTP status.

`int http_grpc_status(const http_grpc_t *result);`

- `result`: Borrowed result.

Returns: Status 0 to 16, or -1 for NULL.

### `http_grpc_message`

Borrow the percent-decoded UTF-8 grpc-message.

`const string_t *http_grpc_message(const http_grpc_t *result);`

- `result`: Borrowed result.

Returns: Borrowed message, possibly empty, or NULL for NULL result.

### `http_grpc_payload`

Borrow the unary Protocol Buffers response.

`const protobuf_t *http_grpc_payload(const http_grpc_t *result);`

- `result`: Borrowed result.

Returns: Borrowed message on status zero, or NULL on error; valid until result destruction.

### `http_websocket_open`

Upgrade a GET request to a WebSocket connection without following redirects.

`http_websocket_t *http_websocket_open(http_client_t *client, const http_request_t *request);`

- `client`: Borrowed idle client; remains exclusively reserved until http_websocket_free. Do not free it first.
- `request`: Borrowed GET request with an HTTP/HTTPS URL, mapped internally to ws/wss; copied handshake headers.

Returns: Owned connection, or NULL with client error set. Compression and subprotocol negotiation are unsupported.

### `http_websocket_free`

Close the transport and release the client's reservation.

`void http_websocket_free(http_websocket_t *socket);`

- `socket`: Owned connection; NULL is harmless. Send an explicit close first for a graceful protocol shutdown.

### `http_websocket_send`

Send one complete bounded text or binary message, handling partial writes.

`bool http_websocket_send(http_websocket_t *socket, bool text, const void *data, size_t size);`

- `socket`: Open idle connection.
- `text`: True for strict UTF-8 text; false for binary.
- `data`: Borrowed bytes; NULL only for size zero.
- `size`: Byte count, bounded by the client's upload limit.

Returns: True when the entire message was sent; false sets the client error and closes the connection on I/O failure.

### `http_websocket_receive`

Receive and reassemble one bounded message, handling interleaved control frames.

`array_t *http_websocket_receive(http_websocket_t *socket, bool *text);`

- `socket`: Open idle connection.
- `text`: Required output: true for text, false for binary; unchanged on failure.

Returns: Owned byte array, or NULL on error or a peer close; inspect client error and http_websocket_is_open.

### `http_websocket_close`

Send a close frame and stop further messaging; does not wait for the peer's acknowledgement.

`bool http_websocket_close(http_websocket_t *socket, uint16_t code, const string_t *reason);`

- `socket`: Open idle connection.
- `code`: Valid wire close code: 1000-1014 excluding 1004-1006, or 3000-4999.
- `reason`: Optional borrowed UTF-8 reason, at most 123 encoded bytes.

Returns: True if the close frame was sent; release the transport with http_websocket_free.

### `http_websocket_is_open`

Check whether data messaging is still permitted.

`bool http_websocket_is_open(const http_websocket_t *socket);`

- `socket`: Borrowed connection.

Returns: True while open; false after close, transport/protocol failure, or for NULL.

### `http_event_reader_id`

Borrow the latest event identifier, including updates without dispatched data.

`const string_t *http_event_reader_id(const http_event_reader_t *reader);`

- `reader`: Borrowed reader.

Returns: Borrowed identifier until subsequent feed or destruction, or NULL for NULL reader.

### `http_event_reader_retry`

Read the latest valid retry field, including updates without dispatched data.

`bool http_event_reader_retry(const http_event_reader_t *reader, uint64_t *milliseconds);`

- `reader`: Borrowed reader.
- `milliseconds`: Required output delay, unchanged when no valid retry field exists.

Returns: True if a valid retry delay is available; false otherwise.

## Tests

Run `make -j1 test_http` from the repository root. Python 3's standard library
provides offline loopback HTTP/HTTPS fixtures on ephemeral ports; no public
service or network credentials are needed. Ordinary tests cover URL/header
validation, all methods, repeated headers and trailers, response status,
binary content, JSON/XML, file upload/download, cancellation, deadlines,
truncation, malformed headers, streamed XML, body/header/upload limits and
TLS trust/hostname verification.
README output examples run last.

The PEM certificate and private key under `tests/http/` are public, test-only
fixtures for localhost, valid until October 2036. Never use them in production;
renew them before expiry. Transfer files use the test harness's per-case
temporary directory and are removed after successful tests. This suite is functional testing, not a claim of exhaustive fault
injection or memory-test coverage.
