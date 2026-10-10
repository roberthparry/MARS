/**
 * @file lab_server.c
 * @brief Native MARS Lab HTTP routes, private-network access and bounded static assets.
 *
 * Adapts the existing browser contract to native evaluation, calendar and state helpers.
 * Uses the public webserver module and never interprets mathematical expressions.
 * Routes are registered before workers are forked; each worker owns its request state.
 */
#include <errno.h>
#include <stdlib.h>

#include "array.h"
#include "file.h"
#include "lab_calendar.h"
#include "lab_evaluate.h"
#include "lab_forms.h"
#include "lab_mobile.h"
#include "lab_page.h"
#include "lab_presentation.h"
#include "lab_state.h"
#include "lab_wire.h"
#include "lab_server.h"
#include "lab_server_internal.h"

#ifndef MARS_LAB_WASM_PATH
#define MARS_LAB_WASM_PATH "tools/mars_lab/build/release/wasm/lab_browser.wasm"
#endif

struct lab_server {
    websrv_t *listener;
};

static bool lab_svr_equal(const string_t *text, const char *literal)
{
    string_t *other = string_new_with(literal);
    bool result = text && other && string_compare(text, other) == 0;
    string_free(other);
    return result;
}

static bool lab_svr_header(websrv_response_t *response, const char *name, const char *value)
{
    string_t *key = string_new_with(name), *text = string_new_with(value);
    bool ok = key && text && websrv_response_header(response, key, text);
    string_free(key);
    string_free(text);
    return ok;
}

static const string_t *lab_svr_request_header(const websrv_request_t *request, const char *name)
{
    string_t *key = string_new_with(name);
    const string_t *value = key ? websrv_request_header(request, key) : NULL;
    string_free(key);
    return value;
}

static bool lab_svr_value(const websrv_request_t *request, websrv_response_t *response, const json_t *value)
{
    (void)request;
    array_t *bytes = lab_wire_encode(value);
    bool ok = bytes && lab_svr_header(response, "Content-Type", "application/x-protobuf") &&
              websrv_response_body(response, array_get(bytes, 0), array_size(bytes));
    array_destroy(bytes);
    return ok;
}

static bool lab_svr_message(const websrv_request_t *request, websrv_response_t *response, unsigned status,
                            const char *text)
{
    json_t *json = json_new_object(), *ok = json_new_bool(false);
    string_t *key = string_new_with("ok"), *error_key = string_new_with("error"), *error = string_new_with(text);
    json_t *value = error ? json_new_string(error) : NULL;
    bool result = json && ok && key && error_key && value && json_object_set(json, key, ok) &&
                  json_object_set(json, error_key, value) && websrv_response_status(response, status) &&
                  lab_svr_value(request, response, json);
    json_free(json);
    json_free(ok);
    json_free(value);
    string_free(key);
    string_free(error_key);
    string_free(error);
    return result;
}

static bool lab_svr_static_asset(const websrv_request_t *request, websrv_response_t *response,
                                 const char *path, const char *type)
{
    file_t *file = file_new_cstr(path);
    file_info_t *info = file ? file_get_info(file) : NULL;
    bool safe = info && file_info_type(info) == FILE_TYPE_REGULAR && file_info_size(info) <= 4194304;
    array_t *bytes = safe ? file_read_all_bytes(file) : NULL;
    bool ok =
        bytes ? lab_svr_header(response, "Content-Type", type) &&
                    websrv_response_body(response, array_size(bytes) ? array_get(bytes, 0) : NULL, array_size(bytes))
              : lab_svr_message(request, response, 404, "Asset not found");
    array_destroy(bytes);
    file_info_free(info);
    file_free(file);
    return ok;
}

struct lab_svr_asset {
    const char *route;
    const char *path;
    const char *type;
};

/* Enumerated once at registration, then resolved by the webserver's route index. */
static const struct lab_svr_asset lab_svr_assets[] = {
    {"/wasm/lab_browser.wasm", MARS_LAB_WASM_PATH, "application/wasm"},
    {"/js/transport.js", "tools/mars_lab/assets/js/transport.js", "text/javascript; charset=utf-8"},
    {"/index.css", "tools/mars_lab/assets/index.css", "text/css; charset=utf-8"},
    {"/css/theme.css", "tools/mars_lab/assets/css/theme.css", "text/css; charset=utf-8"},
    {"/css/layout.css", "tools/mars_lab/assets/css/layout.css", "text/css; charset=utf-8"},
    {"/css/worksheets.css", "tools/mars_lab/assets/css/worksheets.css", "text/css; charset=utf-8"},
    {"/css/dates.css", "tools/mars_lab/assets/css/dates.css", "text/css; charset=utf-8"},
    {"/css/forms.css", "tools/mars_lab/assets/css/forms.css", "text/css; charset=utf-8"},
    {"/css/bindings.css", "tools/mars_lab/assets/css/bindings.css", "text/css; charset=utf-8"},
    {"/css/mobile.css", "tools/mars_lab/assets/css/mobile.css", "text/css; charset=utf-8"},
    {"/css/buttons.css", "tools/mars_lab/assets/css/buttons.css", "text/css; charset=utf-8"},
    {"/css/results.css", "tools/mars_lab/assets/css/results.css", "text/css; charset=utf-8"},
    {"/css/help.css", "tools/mars_lab/assets/css/help.css", "text/css; charset=utf-8"},
    {"/css/responsive.css", "tools/mars_lab/assets/css/responsive.css", "text/css; charset=utf-8"},
    {"/js/api.js", "tools/mars_lab/assets/js/api.js", "text/javascript; charset=utf-8"},
    {"/js/app.js", "tools/mars_lab/assets/js/app.js", "text/javascript; charset=utf-8"},
    {"/js/bindings.js", "tools/mars_lab/assets/js/bindings.js", "text/javascript; charset=utf-8"},
    {"/js/locations.js", "tools/mars_lab/assets/js/locations.js", "text/javascript; charset=utf-8"},
    {"/js/results.js", "tools/mars_lab/assets/js/results.js", "text/javascript; charset=utf-8"},
    {"/js/state.js", "tools/mars_lab/assets/js/state.js", "text/javascript; charset=utf-8"},
    {"/js/workspace.js", "tools/mars_lab/assets/js/workspace.js", "text/javascript; charset=utf-8"},
};

static bool lab_svr_asset_request(const websrv_request_t *request, websrv_response_t *response, void *context)
{
    const struct lab_svr_asset *asset = context;
    if (!lab_svr_header(response, "Cache-Control", "no-store") ||
        !lab_svr_header(response, "X-Content-Type-Options", "nosniff"))
        return false;
    if (!lab_svr_request_permitted(request))
        return lab_svr_message(request, response, 403, "MARS Lab is private; use this machine or a private network.");
    return lab_svr_static_asset(request, response, asset->path, asset->type);
}

static bool lab_svr_dispatch(const websrv_request_t *request, websrv_response_t *response, void *context)
{
    lab_server_t *server = context;
    if (!lab_svr_header(response, "Cache-Control", "no-store") ||
        !lab_svr_header(response, "X-Content-Type-Options", "nosniff"))
        return false;
    if (!lab_svr_request_permitted(request))
        return lab_svr_message(request, response, 403, "MARS Lab is private; use this machine or a private network.");
    const string_t *path = websrv_request_path(request);
    if (websrv_request_method(request) == HTTP_GET) {
        if (lab_svr_equal(path, "/") || lab_svr_equal(path, "/index.html")) {
            json_t *state = lab_state_load();
            json_t *mobile = lab_mobile_details(lab_svr_request_header(request, "Host"), lab_svr_port(server));
            string_t *key = string_new_with("mobile");
            if (state && mobile && key)
                json_object_set(state, key, mobile);
            json_free(mobile);
            string_free(key);
            string_t *page = state ? lab_page_render(state) : NULL;
            bool ok = page && lab_svr_header(response, "Content-Type", "text/html; charset=utf-8") &&
                      websrv_response_text(response, page);
            string_free(page);
            json_free(state);
            return ok;
        }
        if (lab_svr_equal(path, "/bootstrap")) {
            json_t *state = lab_state_load();
            json_t *config = state ? lab_page_bootstrap(state) : NULL;
            bool ok = config ? lab_svr_value(request, response, config)
                             : lab_svr_message(request, response, 500, "Could not load Lab configuration");
            json_free(config);
            json_free(state);
            return ok;
        }
        if (lab_svr_equal(path, "/state")) {
            json_t *state = lab_state_load();
            bool ok = state && lab_svr_value(request, response, state);
            json_free(state);
            return ok;
        }
        if (lab_svr_equal(path, "/jurisdictions")) {
            json_t *catalogue = lab_page_jurisdictions();
            bool ok = catalogue && lab_svr_value(request, response, catalogue);
            json_free(catalogue);
            return ok;
        }
        if (lab_svr_equal(path, "/catalogue")) {
            json_t *catalogue = lab_page_catalogue();
            bool ok = catalogue && lab_svr_value(request, response, catalogue);
            json_free(catalogue);
            return ok;
        }
        if (lab_svr_equal(path, "/mobile-access")) {
            json_t *mobile = lab_mobile_details(lab_svr_request_header(request, "Host"), lab_svr_port(server));
            bool ok = mobile && lab_svr_value(request, response, mobile);
            json_free(mobile);
            return ok;
        }
        if (lab_svr_equal(path, "/favicon.svg"))
            return lab_svr_static_asset(request, response, "packaging/linux/mars-lab.svg", "image/svg+xml");
        if (lab_svr_equal(path, "/apple-touch-icon.png"))
            return lab_svr_static_asset(request, response, "packaging/linux/icon-concepts/wizard-prism-180.png", "image/png");
        if (lab_svr_equal(path, "/icon-192.png"))
            return lab_svr_static_asset(request, response, "packaging/linux/icon-concepts/wizard-prism-192.png", "image/png");
        if (lab_svr_equal(path, "/icon-512.png"))
            return lab_svr_static_asset(request, response, "packaging/linux/icon-concepts/wizard-prism-512.png", "image/png");
        /* Browser installation manifests have a standard JSON format, unlike our API messages. */
        const char *body =
            "{\"name\":\"MARS Lab\",\"short_name\":\"MARS\",\"start_url\":\"/\",\"display\":\"standalone\","
            "\"icons\":[{\"src\":\"/icon-192.png\",\"sizes\":\"192x192\",\"type\":\"image/png\"}]}";
        string_t *text = string_new_with(body);
        bool ok = text && lab_svr_header(response, "Content-Type", "application/manifest+json") &&
                  websrv_response_text(response, text);
        string_free(text);
        return ok;
    }
    if (lab_svr_equal(path, "/funnel-toggle"))
        return lab_svr_message(request, response, 410, "Public access switching is disabled in MARS Lab.");
    const string_t *type = lab_svr_request_header(request, "Content-Type");
    string_offset_t separator = type ? string_find(type, ";") : -1;
    string_t *media = type ? string_substring(type, 0, separator < 0 ? string_length(type) : (size_t)separator) : NULL;
    if (media)
        string_trim(media);
    bool is_wire = lab_svr_equal(media, "application/x-protobuf");
    string_free(media);
    if (!is_wire)
        return lab_svr_message(request, response, 415, "Expected application/x-protobuf");
    json_t *payload = lab_wire_decode(websrv_request_body(request), websrv_request_body_size(request));
    if (!payload || json_type(payload) != JSON_OBJECT) {
        json_free(payload);
        return lab_svr_message(request, response, 400, "Expected a valid versioned Protobuf request object");
    }
    if (lab_svr_equal(path, "/state")) {
        bool saved = lab_state_save(payload);
        json_free(payload);
        if (!saved)
            return lab_svr_message(request, response, 500, "Could not save Lab state");
        json_t *state = lab_state_load();
        bool ok = state && lab_svr_value(request, response, state);
        json_free(state);
        return ok;
    }
    unsigned status = 200;
    bool presentation = lab_svr_equal(path, "/presentation"), forms = lab_svr_equal(path, "/forms");
    json_t *result = presentation ? lab_presentation_request(payload, &status)
                                 : forms ? lab_forms_request(payload, &status) : lab_eval_request(path, payload, &status);
    if (!result)
        result = lab_cal_request(path, payload, &status);
    if (result && !presentation && !forms && !lab_presentation_adapt(result)) {
        json_free(result);
        result = NULL;
        json_free(payload);
        return lab_svr_message(request, response, 500, "Could not prepare native presentation metadata");
    }
    bool ok = result ? websrv_response_status(response, status) && lab_svr_value(request, response, result)
                     : lab_svr_message(request, response, 404, "Unknown Lab operation");
    json_free(result);
    json_free(payload);
    return ok;
}

/* Register a bounded, fixed set of UI endpoints before forking workers. */
static bool lab_svr_register_routes(lab_server_t *server)
{
    static const char *const get_routes[] = {"/",
                                             "/index.html",
                                             "/bootstrap",
                                             "/catalogue",
                                             "/state",
                                             "/jurisdictions",
                                             "/mobile-access",
                                             "/favicon.svg",
                                             "/apple-touch-icon.png",
                                             "/icon-192.png",
                                             "/icon-512.png",
                                             "/manifest.webmanifest"};
    static const char *const post_routes[] = {"/state",
                                              "/presentation",
                                              "/forms",
                                              "/eval",
                                              "/goal_seek",
                                              "/function-run",
                                              "/render_TeX",
                                              "/equation-eval",
                                              "/diffequation-eval",
                                              "/matrix-eval",
                                              "/integrator-eval",
                                              "/datetime-eval",
                                              "/datetime-weather",
                                              "/datetime-jurisdiction-location",
                                              "/almanac-eval",
                                              "/almanac-land-totality",
                                              "/funnel-toggle"};
    for (size_t i = 0; i < sizeof lab_svr_assets / sizeof *lab_svr_assets; ++i) {
        string_t *path = string_new_with(lab_svr_assets[i].route);
        bool ok =
            path && websrv_route(server->listener, HTTP_GET, path, lab_svr_asset_request, (void *)&lab_svr_assets[i]);
        string_free(path);
        if (!ok)
            return false;
    }
    for (size_t i = 0; i < sizeof(get_routes) / sizeof(*get_routes); ++i) {
        string_t *path = string_new_with(get_routes[i]);
        bool ok = path && websrv_route(server->listener, HTTP_GET, path, lab_svr_dispatch, server);
        string_free(path);
        if (!ok)
            return false;
    }
    for (size_t i = 0; i < sizeof(post_routes) / sizeof(*post_routes); ++i) {
        string_t *path = string_new_with(post_routes[i]);
        bool ok = path && websrv_route(server->listener, HTTP_POST, path, lab_svr_dispatch, server);
        string_free(path);
        if (!ok)
            return false;
    }
    return true;
}

/* Own the listener and its complete route set as one application resource. */
lab_server_t *lab_svr_new(const string_t *address, uint16_t port, unsigned timeout_ms)
{
    lab_server_t *server = calloc(1, sizeof(*server));
    if (!server)
        return NULL;
    websrv_limits_t limits = {
        .max_header_bytes = 16384, .max_body_bytes = 8388608, .timeout_ms = timeout_ms ? timeout_ms : 180000};
    server->listener = websrv_new(address, port, &limits);
    if (!server->listener || !lab_svr_register_routes(server)) {
        int error = errno;
        lab_svr_free(server);
        errno = error;
        return NULL;
    }
    return server;
}

/* Release this process's copy of the listener and routing state. */
void lab_svr_free(lab_server_t *server)
{
    if (server) {
        websrv_free(server->listener);
        free(server);
    }
}

/* Report the actual port, including ephemeral-port selection. */
uint16_t lab_svr_port(const lab_server_t *server)
{
    return server ? websrv_port(server->listener) : 0;
}

/* Keep transport internals behind the Lab's serving interface. */
int lab_svr_serve_once(lab_server_t *server, unsigned wait_ms)
{
    if (!server) {
        errno = EINVAL;
        return -1;
    }
    return websrv_serve_once(server->listener, wait_ms);
}
