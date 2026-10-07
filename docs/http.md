# HTTP and HTTPS

The client and [web server](webserver.md) share `webmethod_t` from `webserver.h`:
`HTTP_GET`, `HTTP_POST`, `HTTP_PUT`, `HTTP_PATCH`, `HTTP_DELETE`, `HTTP_HEAD` and
`HTTP_OPTIONS`. Including `http.h` makes these types available.

The HTTP module consumes web services from C using opaque `http_client_t`,
`http_request_t` and `http_response_t` handles. Include `http.h`.
Transport uses system libcurl 7.85.0 or newer, with thread-safe global
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

Redirects, environment proxies, automatic cookies and netrc credentials are
disabled. The module does not implement retries. Bearer credentials and other
headers are sent only to the explicitly requested URL; use HTTPS for secrets.
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

This is a synchronous client, not an HTTP server, browser, OAuth workflow,
WebSocket client or asynchronous scheduler. Multipart forms, proxy
configuration and redirect policies are not currently exposed.

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
