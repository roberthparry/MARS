# Web server

`webserver` provides a synchronous Linux HTTP listener using the `websrv_`
prefix and opaque `websrv_t`, `websrv_request_t` and `websrv_response_t`
handles. Include `webserver.h`. It uses Linux sockets and existing MARS
modules; there is no additional library dependency.

Use [http](http.md) to call services; use this module to implement them.

## Lifetime and routing

Create a server with `websrv_new()`, register routes with `websrv_route()`,
then call `websrv_serve_once()` repeatedly from your application loop.
Each call accepts at most one connection. It returns 1 after processing a
connection (including protocol rejection), 0 after an idle listener timeout,
or -1 on a local/I/O failure, with `errno` set. A peer deadline expires with
`ETIMEDOUT`. The listener remains usable after connection failures.

A NULL address binds **127.0.0.1 only**. Port zero selects an ephemeral port,
available through `websrv_port()`. Supply a numeric IPv4 or IPv6 address to change
the binding; `0.0.0.0` exposes all IPv4 interfaces and `::` enables a dual-stack
listener on all interfaces. `::1` selects IPv6 loopback. Hostname resolution and
scoped IPv6 address strings are not implemented. `websrv_request_peer()` gives
the socket peer address for access checks; it never trusts forwarded headers.

Routes match the method and exact path before the query string. Paths and
targets are not percent-decoded, normalised or mapped onto the filesystem.
Methods use the shared `webmethod_t` enumeration from `webserver.h`: GET, POST, PUT, PATCH,
DELETE, HEAD and OPTIONS. Register HEAD explicitly; its response advertises
the body length but sends no body. An unknown path returns 404; a known path
with an unregistered method returns 405 with an Allow header. Re-registering
the same method/path replaces the callback and context.

Callbacks receive borrowed handles valid only during the call. Copy anything
you wish to retain. They start with an empty status-200 response. Return false
if a response setter fails: the server discards the partial response and sends
an empty 500 response. Context pointers remain caller-owned.
Release the idle server with `websrv_free()`.

## Limits and protocol scope

`websrv_limits_t` contains:

| Field | Default | Accepted non-zero values |
| --- | --- | --- |
| `max_header_bytes` | 16 KiB | 512 bytes through 1 MiB |
| `max_body_bytes` | 1 MiB | 1 byte through 64 MiB |
| `timeout_ms` | 5000 ms | 1 through 600000 ms |

Header and body budgets apply separately to incoming and outgoing messages.
Responses reserve 192 header bytes for framing and Date. Bodies are buffered;
there is no file serving or streaming upload/download API in this first version.
Binary bodies preserve embedded NUL bytes. UTF-8 text decoding is strict and
does not normalise spelling; JSON/XML parsing delegates to the native modules.

The supported wire subset is HTTP/1.1 origin-form requests with CRLF line
endings, a non-empty Host header and either Content-Length framing or no body.
The parser uses `string_t` views and requires ASCII request lines and header
values. Header names are case-insensitive. For conservative handling, **all
duplicate request header names are rejected**, even where HTTP permits repeated
fields. Folded headers, invalid decimal lengths, fragments, invalid percent
escapes and framing ambiguities are rejected. Oversized headers return 431;
oversized bodies return 413. Transfer-Encoding is not supported (501, or 400
when combined with Content-Length); Expect returns 417 rather than waiting
for a body the client has not sent. Other HTTP versions return 505.

These conservative restrictions address the message framing concerns described
in [RFC 9112](https://www.rfc-editor.org/rfc/rfc9112.html); this is not a complete
HTTP/1.1 implementation. Every response says Connection: close. No pipelined
request is dispatched after the first, and there is no keep-alive, HTTP/2,
WebSocket upgrade, CONNECT tunnel, multipart interpretation or chunked coding.

The connection deadline covers receiving and sending together, not just each
socket operation. Handler execution cannot be pre-empted; keep callbacks
bounded. The server is single-threaded and handles one connection at a time.
Do not re-enter, modify routes on, or free an active server.

There is no TLS or authentication policy. Use a configured reverse proxy for
HTTPS and public deployment, and implement authorisation in your handlers.
Do not trust forwarded headers automatically. A slow client occupies the sole
worker until its deadline; this is intended for small local or proxied services,
not as a hardened, high-concurrency internet-facing server.

Applications may register routes before forking a fixed worker pool. Each child
then serves the inherited listener independently; no server handle is shared
between threads. Applications remain responsible for process supervision,
shutdown and interprocess locking of persistent state.

## Public API

### `websrv_request_peer`

Borrow the numeric socket peer address during a handler. The `request` remains
caller-owned. Returns NULL for NULL input. IPv4-mapped IPv6 peers retain mapped
notation; applications checking network ranges must account for that form.

### `websrv_new`

Bind a synchronous Linux IPv4 or IPv6 server.

`address`: Borrowed numeric IP address; NULL selects 127.0.0.1. Use 0.0.0.0 or :: explicitly for all interfaces.

`port`: TCP port; zero selects an ephemeral port.

`limits`: Optional limits; NULL selects defaults.

Returns: Owned listener, or NULL with errno set; release with websrv_free().

### `websrv_free`

Close an idle listener and release routes, but not callback contexts.

`server`: Owned idle server; NULL is harmless.

### `websrv_port`

Read the bound TCP port.

`server`: Borrowed listener.

Returns: Bound port, or zero for NULL.

### `websrv_route`

Register or replace an exact method/path route.

`server`: Idle server to modify.

`method`: Supported method; HEAD requires its own route.

`path`: Borrowed ASCII path starting with /, without query or fragment; copied, not decoded.

`handler`: Required synchronous callback.

`user_data`: Borrowed context retained until replacement or destruction.

Returns: True on success; false with errno set on failure.

### `websrv_serve_once`

Process at most one connection, always closing it afterwards.

`server`: Idle server; no concurrent use or callback re-entry.

`wait_ms`: Listener wait from zero (poll) through 600000 milliseconds.

Returns: 1 for a processed connection, including rejection; 0 for idle timeout; -1 on local/I/O failure with errno set. Peer timeouts set ETIMEDOUT. Handler execution cannot be pre-empted.

### `websrv_request_method`

Read the request method.

`request`: Borrowed request.

Returns: Method, or HTTP_GET for NULL.

### `websrv_request_target`

Borrow the raw origin-form target including query; no URL decoding.

`request`: Borrowed request.

Returns: Borrowed string valid only during the handler, or NULL.

### `websrv_request_path`

Borrow the raw routing path without query; no URL decoding.

`request`: Borrowed request.

Returns: Borrowed string valid only during the handler, or NULL.

### `websrv_request_header`

Borrow a header value using a case-insensitive name.

`request`: Borrowed request; duplicate request headers are rejected before dispatch.

`name`: Borrowed ASCII header name.

Returns: Borrowed value during the handler, or NULL if absent or invalid.

### `websrv_request_body`

Borrow binary request bytes, preserving embedded NUL bytes.

`request`: Borrowed request.

Returns: Borrowed bytes during the handler, or NULL for an empty body or NULL input.

### `websrv_request_body_size`

Read the request body byte count.

`request`: Borrowed request.

Returns: Byte count, or zero for NULL.

### `websrv_request_text`

Decode the request body as exact UTF-8 without normalisation.

`request`: Borrowed request; Content-Type is not interpreted.

Returns: Owned text, released with string_free(), or NULL on invalid input or failure.

### `websrv_request_json`

Decode the request body as JSON.

`request`: Borrowed request; Content-Type is not interpreted.

Returns: Owned json, released with json_free(), or NULL on invalid input or failure.

### `websrv_request_xml`

Decode the request body as XML.

`request`: Borrowed request; Content-Type is not interpreted.

Returns: Owned xml, released with xml_free(), or NULL on invalid input or failure.

### `websrv_response_status`

Set the final response status.

`response`: Borrowed active response builder.

`status`: Status 200 through 599; 204, 205 and 304 suppress the body.

Returns: True on success; false on invalid input.

### `websrv_response_header`

Copy or replace a response header.

`response`: Borrowed active response builder.

`name`: Borrowed ASCII token. Framing, connection, upgrade, trailer and Date headers are reserved.

`value`: Borrowed printable ASCII or tab value; NUL bytes and newlines are rejected.

Returns: True on success; false on invalid input, budget exhaustion or allocation failure.

### `websrv_response_body`

Copy a binary response body within the configured budget.

`response`: Borrowed active response builder.

`data`: Borrowed bytes; NULL allowed only for zero size.

`size`: Byte count.

Returns: True on success; false leaves the old body unchanged.

### `websrv_response_text`

Copy encoded text bytes; set Content-Type separately.

`response`: Borrowed active response builder.

`text`: Borrowed text.

Returns: True on success; false on failure. Return false from the handler if any setter fails.

### `websrv_response_json`

Copy serialised JSON and set application/json.

`response`: Borrowed active response builder.

`json`: Borrowed json.

Returns: True on success; false on failure. Return false from the handler if any setter fails.

### `websrv_response_xml`

Copy serialised XML and set application/xml.

`response`: Borrowed active response builder.

`xml`: Borrowed xml.

Returns: True on success; false on failure. Return false from the handler if any setter fails.

## Runnable example: create and call a service

This complete program uses an ephemeral loopback port, forks a single-request
server and calls it with the MARS client. It needs no internet connection.
The child handles the request; the parent checks the response and waits for
the child. For a long-running service, keep calling `websrv_serve_once()`
in your application's loop instead of stopping after one connection.

The source is also available as `scratch/webserver_hello.c`.

```c
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include "webserver.h"
#include "http.h"

static bool hello(const websrv_request_t *request, websrv_response_t *response, void *context)
{
    (void)request;
    (void)context;
    string_t *text = string_new_with("{\"message\":\"Hello from MARS\"}");
    json_t *json = text ? json_from_text(text) : NULL;
    bool ok = json && websrv_response_json(response, json);
    json_free(json);
    string_free(text);
    return ok;
}

int main(void)
{
    websrv_t *server = websrv_new(NULL, 0, NULL);
    string_t *path = string_new_with("/hello");
    bool ready = server && path && websrv_route(server, HTTP_GET, path, hello, NULL);
    string_free(path);
    if (!ready) {
        websrv_free(server);
        return EXIT_FAILURE;
    }
    uint16_t port = websrv_port(server);
    pid_t child = fork();
    if (child == 0) {
        int served = websrv_serve_once(server, 3000);
        websrv_free(server);
        _exit(served == 1 ? EXIT_SUCCESS : EXIT_FAILURE);
    }
    websrv_free(server);
    if (child < 0) return EXIT_FAILURE;
    string_t *url = string_sprintf("http://127.0.0.1:%u/hello", port);
    http_client_t *client = http_client_new();
    http_request_t *request = http_request_new(HTTP_GET, url);
    http_response_t *response = client && request ? http_client_send(client, request) : NULL;
    string_t *text = http_response_ok(response) ? http_response_text(response) : NULL;
    bool ok = text != NULL;
    if (ok) string_printf("status=%ld\n%S\n", http_response_status(response), text);
    string_free(text);
    http_response_free(response);
    http_request_free(request);
    http_client_free(client);
    string_free(url);
    int status = 0;
    ok = waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0 && ok;
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
```

Build and run:

```sh
make -j1 build/release/scratch/webserver_hello
build/release/scratch/webserver_hello
```

Output:

```text
status=200
{"message":"Hello from MARS"}
```

## Tests

Run `make -j1 test_webserver`. Tests use ephemeral loopback sockets only,
run sequentially, and require no public service. Ordinary checks cover
lifetime and validation, route replacement, method dispatch, headers,
binary bodies, JSON/XML, HEAD and bodyless statuses, callback failure,
malformed framing, budgets, truncated input, deadlines and recovery.
The complete README program runs last. `make -j1 test-readme-examples`
also compiles and runs the exact documentation program and verifies its output.
No memory tests are implied by these commands.

`make -j1 test-http-live` additionally runs end-to-end client/server tests
against this module, including the HTTP guide's GET, JSON and XML programs.
It starts bounded child servers on ephemeral loopback ports and needs no
internet connection. These integration tests also run in ordinary `make test`.
