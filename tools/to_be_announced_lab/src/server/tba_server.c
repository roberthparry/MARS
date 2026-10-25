/**
 * @file tba_server.c
 * @brief Private Lab routes and encrypted, transient request processing.
 *
 * Only authenticated encrypted envelopes reach computation. Retired storage and
 * file-upload routes are rejected. The actual socket peer must be loopback;
 * remote browsers use an existing trusted HTTPS reverse proxy.
 */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mars_paths.h"
#include "tba_mobile.h"
#include "tba_wire.h"
#include "tba_forecast.h"
#include "tba_http.h"
#include "tba_page.h"
#include "tba_series.h"
#include "tba_stream.h"
#include "tba_server.h"


typedef bool (*tba_svr_handler_t)(const websrv_request_t *, websrv_response_t *, tba_server_t *);
struct tba_server {
    websrv_t *listener;
    tba_app_t *app;
    tba_stream_t *stream;
    bool retire;
    struct tba_svr_context *contexts;
};

static bool tba_svr_protobuf(websrv_response_t *response, unsigned status, json_t *data)
{
    array_t *bytes = tba_wire_encode(data);
    if (!bytes) {
        json_t *error = tba_json_error("Response exceeds the Protobuf limits.");
        bytes = tba_wire_encode(error);
        json_free(error);
        status = 500;
    }
    bool ok = bytes && websrv_response_status(response, status) &&
              tba_http_header_set(response, "Content-Type", "application/x-protobuf") &&
              websrv_response_body(response, array_get(bytes, 0), array_size(bytes));
    array_destroy(bytes);
    json_free(data);
    return ok;
}

static json_t *tba_svr_payload(const websrv_request_t *request)
{
    const string_t *type = tba_http_header(request, "Content-Type");
    return type && string_view_equals_literal(string_view_all(type), "application/x-protobuf") ?
        tba_wire_decode(websrv_request_body(request), websrv_request_body_size(request)) : NULL;
}

static bool tba_svr_wasm(const websrv_request_t *request, websrv_response_t *response, tba_server_t *server)
{
    (void)server;
    const string_t *path = websrv_request_path(request);
    bool codec = string_ends_with(path, "/wasm/tba_codec.wasm");
    string_t *resource = mars_path_resource(MARS_PATH_PRIVATE_WASM, NULL, codec ? "tba_codec.wasm" : "tba_browser.wasm");
    file_t *file = resource ? file_new(resource) : NULL;
    string_free(resource);
    file_info_t *info = file ? file_get_info(file) : NULL;
    bool safe = info && file_info_type(info) == FILE_TYPE_REGULAR && file_info_size(info) <= 4u * 1024u * 1024u;
    array_t *bytes = safe ? file_read_all_bytes(file) : NULL;
    bool ok = bytes ? websrv_response_status(response, 200) &&
                          tba_http_header_set(response, "Content-Type", "application/wasm") &&
                          websrv_response_body(response, array_get(bytes, 0), array_size(bytes)) :
                      tba_svr_protobuf(response, 404, tba_json_error("Build the browser module first."));
    array_destroy(bytes);
    file_info_free(info);
    file_free(file);
    return ok;
}

static bool tba_svr_text(websrv_response_t *response, unsigned status, const char *type, string_t *text)
{
    bool ok = text && websrv_response_status(response, status) && tba_http_header_set(response, "Content-Type", type) &&
              websrv_response_body(response, string_c_str(text), string_byte_length(text));
    string_free(text);
    return ok;
}

static json_t *tba_svr_mobile(const websrv_request_t *request, tba_server_t *server)
{
    json_t *result = tba_mobile_details(tba_http_header(request, "Host"), tba_svr_port(server));
    const char *url = tba_json_text(result, "url");
    if (*url) {
        string_t *base = string_new_with(url);
        if (base && string_ends_with(base, "/")) {
            string_t *trimmed = string_substr(base, 0, string_byte_length(base) - 1);
            string_free(base);
            base = trimmed;
        }
        string_t *target = base ? string_sprintf("%S%s/", base, tba_app_base(server->app)) : NULL;
        string_t *qr = target ? tba_mobile_qr(target) : NULL;
        bool ok = target && qr && tba_json_string(result, "url", string_c_str(target)) &&
                  tba_json_string(result, "qr", string_c_str(qr));
        string_free(base);
        string_free(target);
        string_free(qr);
        if (!ok) {
            json_free(result);
            result = NULL;
        }
    }
    return result;
}

static bool tba_svr_page(const websrv_request_t *request, websrv_response_t *response, tba_server_t *server)
{
    json_t *mobile = tba_svr_mobile(request, server);
    string_t *page = tba_page_render(server->app, mobile);
    json_free(mobile);
    return tba_svr_text(response, 200, "text/html; charset=utf-8", page);
}

static bool tba_svr_state_get(const websrv_request_t *request, websrv_response_t *response, tba_server_t *server)
{
    (void)request;
    return tba_svr_protobuf(response, 200, tba_app_state(server->app));
}

static bool tba_svr_retired(const websrv_request_t *request, websrv_response_t *response, tba_server_t *server)
{
    (void)request;
    (void)server;
    return tba_svr_protobuf(response, 410, tba_json_error("Use client-local files and encrypted streams."));
}

static bool tba_svr_stream_begin(const websrv_request_t *request, websrv_response_t *response, tba_server_t *server)
{
    json_t *body = tba_svr_payload(request);
    bool valid = json_type(body) == JSON_OBJECT && json_object_size(body) == 0;
    json_free(body);
    json_t *result = valid ? tba_stream_begin(server->stream) : tba_json_error("Invalid stream request.");
    return tba_svr_protobuf(response, valid ? tba_json_bool(result, "ok") ? 200 : 429 : 400, result);
}

static json_t *tba_svr_process(tba_server_t *server, const json_t *payload)
{
    const char *operation = tba_json_text(payload, "operation");
    if (!strcmp(operation, "run"))
        return tba_forecast_run(server->app, payload);
    if (strcmp(operation, "metadata"))
        return tba_json_error("Unsupported operation.");
    tba_series_t *series = tba_series_from_rows(tba_json_get(payload, "data"), tba_json_text(payload, "date_column"));
    json_t *result = series ? tba_series_details(series, tba_json_text(payload, "value_column"),
                                                 tba_json_text(payload, "selected_columns")) : NULL;
    tba_series_free(series);
    if (!result || !tba_json_flag(result, "ok", true)) {
        json_free(result);
        result = tba_json_error("Invalid local observations.");
    }
    return result;
}

static bool tba_svr_stream_chunk(const websrv_request_t *request, websrv_response_t *response, tba_server_t *server)
{
    json_t *frame = tba_svr_payload(request), *message = NULL;
    /* A failed final decode may still have allocated transient plaintext. */
    server->retire = tba_json_bool(frame, "final");
    tba_stream_reply_t *key = NULL;
    json_t *result = tba_stream_receive(server->stream, frame, &message, &key);
    json_free(frame);
    unsigned status = tba_json_bool(result, "ok") ? 200 : 400;
    if (message) {
        json_free(result);
        /* Request workers are isolated processes; the parent replaces a timed-out worker. */
        alarm(60);
        json_t *plain = tba_svr_process(server, message);
        result = tba_stream_seal(key, plain);
        json_free(plain);
        alarm(0);
        if (!result) {
            status = 500;
            result = tba_json_error("Encrypted response unavailable.");
        }
    }
    json_free(message);
    tba_stream_reply_free(key);
    return tba_svr_protobuf(response, status, result);
}

static bool tba_svr_mobile_get(const websrv_request_t *request, websrv_response_t *response, tba_server_t *server)
{
    return tba_svr_protobuf(response, 200, tba_svr_mobile(request, server));
}

static bool tba_svr_funnel(const websrv_request_t *request, websrv_response_t *response, tba_server_t *server)
{
    (void)request;
    (void)server;
    return tba_svr_protobuf(response, 410, tba_json_error("Public access switching is disabled in To-Be-Announced Lab."));
}

static bool tba_svr_manifest(const websrv_request_t *request, websrv_response_t *response, tba_server_t *server)
{
    (void)request;
    json_t *manifest = json_new_object();
    string_t *url = string_sprintf("%s/", tba_app_base(server->app));
    bool ok = manifest && url && tba_json_string(manifest, "name", "To-Be-Announced Lab") &&
              tba_json_string(manifest, "short_name", "TBA") && tba_json_string(manifest, "start_url", string_c_str(url)) &&
              tba_json_string(manifest, "scope", string_c_str(url)) && tba_json_string(manifest, "display", "standalone");
    string_free(url);
    if (!ok) {
        json_free(manifest);
        return false;
    }
    bool sent = websrv_response_status(response, 200) && websrv_response_json(response, manifest) &&
                tba_http_header_set(response, "Content-Type", "application/manifest+json");
    json_free(manifest);
    return sent;
}

typedef struct {
    const char *path;
    webmethod_t method;
    tba_svr_handler_t handler;
    const char *asset, *type;
} tba_svr_route_t;

struct tba_svr_context {
    tba_server_t *server;
    const tba_svr_route_t *route;
};

static const tba_svr_route_t tba_svr_routes[] = {
    {"/", HTTP_GET, tba_svr_page, NULL, NULL}, {"/index.html", HTTP_GET, tba_svr_page, NULL, NULL},
    {"/state", HTTP_GET, tba_svr_state_get, NULL, NULL}, {"/state", HTTP_POST, tba_svr_retired, NULL, NULL},
    {"/target-columns", HTTP_POST, tba_svr_retired, NULL, NULL},
    {"/upload-target", HTTP_POST, tba_svr_retired, NULL, NULL}, {"/upload-xreg", HTTP_POST, tba_svr_retired, NULL, NULL},
    {"/forecast", HTTP_POST, tba_svr_retired, NULL, NULL},
    {"/stream/begin", HTTP_POST, tba_svr_stream_begin, NULL, NULL},
    {"/stream/chunk", HTTP_POST, tba_svr_stream_chunk, NULL, NULL},
    {"/mobile-access", HTTP_GET, tba_svr_mobile_get, NULL, NULL}, {"/funnel-toggle", HTTP_POST, tba_svr_funnel, NULL, NULL},
    {"/download-summary", HTTP_GET, tba_svr_retired, NULL, NULL}, {"/fetch-forecast", HTTP_GET, tba_svr_retired, NULL, NULL},
    {"/manifest.webmanifest", HTTP_GET, tba_svr_manifest, NULL, NULL},
    {"/wasm/tba_browser.wasm", HTTP_GET, tba_svr_wasm, NULL, NULL},
    {"/wasm/tba_codec.wasm", HTTP_GET, tba_svr_wasm, NULL, NULL},
    {"/js/codec.js", HTTP_GET, NULL, "/@private_scripts/codec.js", "text/javascript; charset=utf-8"},
    {"/js/codec_types.js", HTTP_GET, NULL, "/@private_scripts/codec_types.js",
     "text/javascript; charset=utf-8"},
    {"/js/jobs.js", HTTP_GET, NULL, "/@private_scripts/jobs.js", "text/javascript; charset=utf-8"},
    {"/js/codec_worker.js", HTTP_GET, NULL, "/@private_scripts/codec_worker.js",
     "text/javascript; charset=utf-8"},
    {"/index.css", HTTP_GET, NULL, "index.css", "text/css; charset=utf-8"},
    {"/js/transport.js", HTTP_GET, NULL,
     "/@private_scripts/transport.js",
     "text/javascript; charset=utf-8"},
    {"/favicon.svg", HTTP_GET, NULL, NULL, "image/svg+xml"}
};

static bool tba_svr_dispatch(const websrv_request_t *request, websrv_response_t *response, void *context)
{
    const struct tba_svr_context *binding = context;
    tba_server_t *server = binding->server;
    const tba_svr_route_t *route = binding->route;
    if (!tba_http_header_set(response, "Cache-Control", "no-store") ||
        !tba_http_header_set(response, "X-Content-Type-Options", "nosniff") ||
        !tba_http_header_set(response, "X-Frame-Options", "SAMEORIGIN"))
        return false;
    if (!tba_http_permitted(request) || !tba_http_loopback(request))
        return tba_svr_protobuf(response, 403, tba_json_error("Access requires local loopback or a trusted HTTPS reverse proxy."));
    if (route->method == HTTP_POST && route->handler != tba_svr_retired) {
        const string_t *type = tba_http_header(request, "Content-Type");
        if (!type || !string_view_equals_literal(string_view_all(type), "application/x-protobuf"))
            return tba_svr_protobuf(response, 415, tba_json_error("Use application/x-protobuf."));
    }
    if (route->method == HTTP_POST && websrv_request_body_size(request) > 65536)
        return tba_svr_protobuf(response, 413, tba_json_error("Stream envelope exceeds 64 KiB."));
    if (route->handler)
        return route->handler(request, response, server);
    /* Leading slash marks a fixed repository-relative asset, never a user-supplied path. */
    string_t *asset = route->asset ? (route->asset[0] == '/' ? tba_path(route->asset + 1) : tba_asset_path(route->asset))
                                 : tba_path("@icons/to-be-announced-lab.svg");
    string_t *text = asset ? tba_file_read(string_c_str(asset), 4u * 1024u * 1024u) : NULL;
    string_free(asset);
    return text ? tba_svr_text(response, 200, route->type, text) :
                  tba_svr_protobuf(response, 404, tba_json_error("Asset not found."));
}

/* Register the existing prefixed and root aliases on a bounded listener. */
tba_server_t *tba_svr_new(tba_app_t *app, const char *host, uint16_t port)
{
    if (!app)
        return NULL;
    tba_server_t *server = calloc(1, sizeof *server);
    string_t *address = string_new_with(host ? host : "127.0.0.1");
    websrv_limits_t limits = {.max_body_bytes = 4u * 1024u * 1024u, .timeout_ms = 30000};
    if (server) {
        server->app = app;
        server->stream = tba_stream_new();
        server->listener = address ? websrv_new(address, port, &limits) : NULL;
        server->contexts = calloc(sizeof tba_svr_routes / sizeof *tba_svr_routes, sizeof *server->contexts);
    }
    string_free(address);
    bool ok = server && server->listener && server->contexts && server->stream;
    for (size_t i = 0; ok && i < sizeof tba_svr_routes / sizeof *tba_svr_routes; ++i) {
        const tba_svr_route_t *route = &tba_svr_routes[i];
        string_t *path = string_new_with(route->path);
        string_t *prefixed = string_sprintf("%s%s", tba_app_base(app), route->path);
        server->contexts[i] = (struct tba_svr_context){server, route};
        ok = path && prefixed && websrv_route(server->listener, route->method, path, tba_svr_dispatch, &server->contexts[i]) &&
             websrv_route(server->listener, route->method, prefixed, tba_svr_dispatch, &server->contexts[i]);
        string_free(path);
        string_free(prefixed);
    }
    if (!ok) {
        tba_svr_free(server);
        server = NULL;
    }
    return server;
}

/* Release this process's listener copy. */
void tba_svr_free(tba_server_t *server)
{
    if (server) {
        websrv_free(server->listener);
        tba_stream_free(server->stream);
        free(server->contexts);
        free(server);
    }
}

/* Report the actual bound port. */
uint16_t tba_svr_port(const tba_server_t *server)
{
    return server ? websrv_port(server->listener) : 0;
}

/* Serve one request in the current single-threaded worker. */
int tba_svr_once(tba_server_t *server)
{
    if (!server)
        return -1;
    tba_stream_expire(server->stream);
    int result = websrv_serve_once(server->listener, 250);
    return server->retire ? 2 : result;
}
