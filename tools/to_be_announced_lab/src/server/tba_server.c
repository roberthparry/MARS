/**
 * @file tba_server.c
 * @brief Native forecasting HTTP routes, response policy and listener lifecycle.
 *
 * Routes dispatch directly through the webserver index. Every request is checked
 * against the private-network policy before asset reads, uploads or calculations.
 * Persistent state/results coordinate prefork workers through atomic files.
 */
#include <stdlib.h>
#include <string.h>

#include "lab_mobile.h"
#include "tba_forecast.h"
#include "tba_http.h"
#include "tba_page.h"
#include "tba_series.h"
#include "tba_server.h"

typedef bool (*tba_svr_handler_t)(const websrv_request_t *, websrv_response_t *, tba_server_t *);
struct tba_server {
    websrv_t *listener;
    tba_app_t *app;
    struct tba_svr_context *contexts;
};

static bool tba_svr_json(websrv_response_t *response, unsigned status, json_t *data)
{
    bool ok = data && websrv_response_status(response, status) && websrv_response_json(response, data);
    json_free(data);
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
    json_t *result = lab_mobile_details(tba_http_header(request, "Host"), tba_svr_port(server));
    const char *url = tba_json_text(result, "url");
    if (*url) {
        string_t *base = string_new_with(url);
        if (base && string_ends_with(base, "/")) {
            string_t *trimmed = string_substr(base, 0, string_byte_length(base) - 1);
            string_free(base);
            base = trimmed;
        }
        string_t *target = base ? string_sprintf("%S%s/", base, tba_app_base(server->app)) : NULL;
        string_t *qr = target ? lab_mobile_qr(target) : NULL;
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
    return tba_svr_json(response, 200, tba_app_state(server->app));
}

static bool tba_svr_state_post(const websrv_request_t *request, websrv_response_t *response, tba_server_t *server)
{
    json_t *update = websrv_request_json(request);
    if (json_type(update) != JSON_OBJECT) {
        json_free(update);
        return tba_svr_json(response, 400, tba_json_error("State payload must be an object."));
    }
    bool ok = tba_app_save(server->app, update);
    json_free(update);
    json_t *result = ok ? json_new_object() : tba_json_error("Could not save the worksheet state.");
    if (ok)
        tba_json_flag(result, "ok", true);
    return tba_svr_json(response, ok ? 200 : 500, result);
}

static bool tba_svr_columns(const websrv_request_t *request, websrv_response_t *response, tba_server_t *server)
{
    (void)server;
    const string_t *target = websrv_request_target(request);
    string_offset_t offset = string_find(target, "?");
    string_t *query = offset < 0 ? string_new() : string_substr(target, (size_t)offset + 1, string_byte_length(target));
    json_t *params = query ? tba_http_form(query) : NULL;
    tba_series_t *series = params ? tba_series_open(tba_json_text(params, "path"), tba_json_text(params, "date_column")) : NULL;
    json_t *result = series ? tba_series_details(series, tba_json_text(params, "value_column"),
                                                tba_json_text(params, "selected_columns")) : NULL;
    bool ok = result && tba_json_flag(result, "ok", true);
    if (!ok) {
        json_free(result);
        result = tba_json_error("CSV data, date column or selected values could not be read.");
    }
    tba_series_free(series);
    json_free(params);
    string_free(query);
    return tba_svr_json(response, ok ? 200 : 400, result);
}

static bool tba_svr_upload(const websrv_request_t *request, websrv_response_t *response, tba_server_t *server)
{
    string_t *filename = NULL, *text = tba_http_upload(request, &filename);
    tba_csv_t *csv = text ? tba_csv_parse(text) : NULL;
    string_t *path = csv ? tba_app_upload(server->app, text) : NULL;
    tba_series_t *series = path ? tba_series_open(string_c_str(path), "") : NULL;
    json_t *result = series ? tba_series_details(series, "", "") : NULL;
    bool ok = result && filename && tba_json_flag(result, "ok", true) &&
              tba_json_string(result, "path", string_c_str(path)) &&
              tba_json_string(result, "original_name", string_c_str(filename));
    if (!ok) {
        json_free(result);
        result = tba_json_error("Choose one non-empty, valid UTF-8 CSV file, up to 16 MiB.");
        file_t *file = path ? file_new(path) : NULL;
        if (file)
            file_delete(file);
        file_free(file);
    }
    tba_series_free(series);
    tba_csv_free(csv);
    string_free(filename);
    string_free(text);
    string_free(path);
    return tba_svr_json(response, ok ? 200 : 400, result);
}

static json_t *tba_svr_run(tba_server_t *server, const json_t *payload)
{
    if (json_type(payload) != JSON_OBJECT)
        return tba_json_error("Forecast settings must be an object.");
    if (!tba_app_save(server->app, payload))
        return tba_json_error("Could not save forecast settings.");
    return tba_forecast_run(server->app, payload);
}

static bool tba_svr_forecast(const websrv_request_t *request, websrv_response_t *response, tba_server_t *server)
{
    json_t *payload = websrv_request_json(request);
    bool valid = json_type(payload) == JSON_OBJECT;
    json_t *result = tba_svr_run(server, payload);
    json_free(payload);
    return tba_svr_json(response, !valid ? 400 : tba_json_bool(result, "ok") ? 200 : 422, result);
}

static bool tba_svr_forecast_form(const websrv_request_t *request, websrv_response_t *response, tba_server_t *server)
{
    string_t *text = websrv_request_text(request);
    json_t *payload = text ? tba_http_form(text) : NULL;
    json_t *result = tba_svr_run(server, payload);
    if (result)
        tba_json_string(result, "kind", "to-be-announced-forecast-result");
    string_t *data = result ? tba_text_script(result) : NULL;
    string_t *page = data ? string_sprintf("<!doctype html><html><body><script>"
        "window.parent.postMessage(%S,location.origin);</script></body></html>", data) : NULL;
    json_free(result);
    json_free(payload);
    string_free(data);
    string_free(text);
    return tba_svr_text(response, 200, "text/html; charset=utf-8", page);
}

static bool tba_svr_mobile_get(const websrv_request_t *request, websrv_response_t *response, tba_server_t *server)
{
    return tba_svr_json(response, 200, tba_svr_mobile(request, server));
}

static bool tba_svr_funnel(const websrv_request_t *request, websrv_response_t *response, tba_server_t *server)
{
    (void)request;
    (void)server;
    return tba_svr_json(response, 410, tba_json_error("Public access switching is disabled in To-Be-Announced Lab."));
}

static bool tba_svr_download(const websrv_request_t *request, websrv_response_t *response, tba_server_t *server)
{
    bool summary = string_ends_with(websrv_request_path(request), "/download-summary");
    json_t *result = tba_app_result(server->app);
    const char *data = tba_json_text(result, summary ? "summary_text" : "forecast_csv");
    bool ok;
    if (!*data)
        ok = tba_svr_json(response, 404, tba_json_error("No completed forecast is available yet."));
    else {
        ok = tba_http_header_set(response, "Content-Disposition", summary ?
            "attachment; filename=\"to-be-announced-summary.txt\"" :
            "attachment; filename=\"to-be-announced-forecast.csv\"") &&
            tba_svr_text(response, 200, summary ? "text/plain; charset=utf-8" : "text/csv; charset=utf-8", string_new_with(data));
    }
    json_free(result);
    return ok;
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
    return tba_svr_json(response, 200, manifest);
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
    {"/state", HTTP_GET, tba_svr_state_get, NULL, NULL}, {"/state", HTTP_POST, tba_svr_state_post, NULL, NULL},
    {"/target-columns", HTTP_GET, tba_svr_columns, NULL, NULL},
    {"/upload-target", HTTP_POST, tba_svr_upload, NULL, NULL}, {"/upload-xreg", HTTP_POST, tba_svr_upload, NULL, NULL},
    {"/forecast", HTTP_POST, tba_svr_forecast, NULL, NULL}, {"/forecast-form", HTTP_POST, tba_svr_forecast_form, NULL, NULL},
    {"/mobile-access", HTTP_GET, tba_svr_mobile_get, NULL, NULL}, {"/funnel-toggle", HTTP_POST, tba_svr_funnel, NULL, NULL},
    {"/download-summary", HTTP_GET, tba_svr_download, NULL, NULL}, {"/download-forecast", HTTP_GET, tba_svr_download, NULL, NULL},
    {"/manifest.webmanifest", HTTP_GET, tba_svr_manifest, NULL, NULL},
    {"/index.css", HTTP_GET, NULL, "index.css", "text/css; charset=utf-8"},
    {"/js/app.js", HTTP_GET, NULL, "js/app.js", "text/javascript; charset=utf-8"},
    {"/js/transport.js", HTTP_GET, NULL, "js/transport.js", "text/javascript; charset=utf-8"},
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
    if (!tba_http_permitted(request))
        return tba_svr_json(response, 403, tba_json_error("This forecasting Lab is private. Use this machine, WiFi or Tailscale."));
    if (route->handler)
        return route->handler(request, response, server);
    string_t *asset = route->asset ? tba_asset_path(route->asset) : tba_path("packaging/linux/to-be-announced-lab.svg");
    string_t *text = asset ? tba_file_read(string_c_str(asset), 4u * 1024u * 1024u) : NULL;
    string_free(asset);
    return text ? tba_svr_text(response, 200, route->type, text) :
                  tba_svr_json(response, 404, tba_json_error("Asset not found."));
}

/* Register the existing prefixed and root aliases on a bounded listener. */
tba_server_t *tba_svr_new(tba_app_t *app, const char *host, uint16_t port)
{
    if (!app)
        return NULL;
    tba_server_t *server = calloc(1, sizeof *server);
    string_t *address = string_new_with(host ? host : "127.0.0.1");
    websrv_limits_t limits = {.max_body_bytes = 17u * 1024u * 1024u, .timeout_ms = 420000};
    if (server) {
        server->app = app;
        server->listener = address ? websrv_new(address, port, &limits) : NULL;
        server->contexts = calloc(sizeof tba_svr_routes / sizeof *tba_svr_routes, sizeof *server->contexts);
    }
    string_free(address);
    bool ok = server && server->listener && server->contexts;
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
    return server ? websrv_serve_once(server->listener, 250) : -1;
}
