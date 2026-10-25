/**
 * @file lab_server.c
 * @brief Native MARS Lab HTTP routes, private-network access and bounded static assets.
 *
 * Adapts the existing browser contract to native evaluation, calendar and state helpers.
 * Uses the public webserver module and never interprets mathematical expressions.
 * Routes are registered before workers are forked; each worker owns its request state.
 */
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "array-t.h"
#include "file-t.h"
#include "jurisdiction-t.h"
#include "lab_process_request.h"
#include "lab_calendar.h"
#include "lab_evaluate.h"
#include "lab_forms.h"
#include "lab_mobile.h"
#include "lab_page.h"
#include "lab_presentation.h"
#include "lab_state.h"
#include "lab_history.h"
#include "lab_surface_service.h"
#include "lab_wire.h"
#include "lab_server.h"
#define MARS_LAB_SERVER_INTERNAL_ACCESS
#include "lab_server_internal.h"

#ifndef MARS_LAB_WASM_PATH
#define MARS_LAB_WASM_PATH "tools/mars_lab/build/release/wasm/lab_browser.wasm"
#endif

#ifndef MARS_LAB_CODEC_PATH
#define MARS_LAB_CODEC_PATH "tools/mars_lab/build/release/wasm/lab_codec.wasm"
#endif

typedef struct lab_server {
    websrv_t *listener;
} lab_server_t;

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
    bool failed = false;
    array_t *bytes = lab_wire_encode_response(value, &failed);
    bool ok = bytes && (!failed || websrv_response_status(response, 500)) &&
              lab_svr_header(response, "Content-Type", "application/x-protobuf") &&
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
                                 const char *path, const char *type, size_t max_bytes)
{
    file_t *file = file_new_cstr(path);
    file_info_t *info = file ? file_get_info(file) : NULL;
    bool safe = info && file_info_type(info) == FILE_TYPE_REGULAR && file_info_size(info) <= max_bytes;
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
    size_t max_bytes;
};

/* Enumerated once at registration, then resolved by the webserver's route index. */
static const struct lab_svr_asset lab_svr_assets[] = {
    {"/wasm/lab_browser.wasm", MARS_LAB_WASM_PATH, "application/wasm", 4194304},
    {"/wasm/lab_codec.wasm", MARS_LAB_CODEC_PATH, "application/wasm", 4194304},
    {"/js/codec.js", "tools/mars_lab/build/typescript/codec.js", "text/javascript; charset=utf-8", 4194304},
    {"/js/codec_types.js", "tools/mars_lab/build/typescript/codec_types.js", "text/javascript; charset=utf-8", 4194304},
    {"/js/jobs.js", "tools/mars_lab/build/typescript/jobs.js", "text/javascript; charset=utf-8", 4194304},
    {"/js/codec_worker.js", "tools/mars_lab/build/typescript/codec_worker.js", "text/javascript; charset=utf-8", 4194304},
    {"/js/transport.js", "tools/mars_lab/build/typescript/transport.js", "text/javascript; charset=utf-8", 4194304},
    {"/index.css", "tools/mars_lab/assets/index.css", "text/css; charset=utf-8", 4194304},
    {"/css/theme.css", "tools/mars_lab/assets/css/theme.css", "text/css; charset=utf-8", 4194304},
    {"/css/layout.css", "tools/mars_lab/assets/css/layout.css", "text/css; charset=utf-8", 4194304},
    {"/css/worksheets.css", "tools/mars_lab/assets/css/worksheets.css", "text/css; charset=utf-8", 4194304},
    {"/css/dates.css", "tools/mars_lab/assets/css/dates.css", "text/css; charset=utf-8", 4194304},
    {"/css/forms.css", "tools/mars_lab/assets/css/forms.css", "text/css; charset=utf-8", 4194304},
    {"/css/bindings.css", "tools/mars_lab/assets/css/bindings.css", "text/css; charset=utf-8", 4194304},
    {"/css/mobile.css", "tools/mars_lab/assets/css/mobile.css", "text/css; charset=utf-8", 4194304},
    {"/css/buttons.css", "tools/mars_lab/assets/css/buttons.css", "text/css; charset=utf-8", 4194304},
    {"/css/results.css", "tools/mars_lab/assets/css/results.css", "text/css; charset=utf-8", 4194304},
    {"/css/help.css", "tools/mars_lab/assets/css/help.css", "text/css; charset=utf-8", 4194304},
    {"/css/responsive.css", "tools/mars_lab/assets/css/responsive.css", "text/css; charset=utf-8", 4194304},
    {"/js/api.js", "tools/mars_lab/build/typescript/api.js", "text/javascript; charset=utf-8", 4194304},
    {"/js/app.js", "tools/mars_lab/build/typescript/app.js", "text/javascript; charset=utf-8", 4194304},
    {"/js/bindings.js", "tools/mars_lab/build/typescript/bindings.js", "text/javascript; charset=utf-8", 4194304},
    {"/js/bindings_controller.js", "tools/mars_lab/build/typescript/bindings_controller.js", "text/javascript; charset=utf-8", 4194304},
    {"/js/locations.js", "tools/mars_lab/build/typescript/locations.js", "text/javascript; charset=utf-8", 4194304},
    {"/js/globe.js", "tools/mars_lab/build/typescript/globe.js", "text/javascript; charset=utf-8", 4194304},
    {"/js/globe_gpu.js", "tools/mars_lab/build/typescript/globe_gpu.js", "text/javascript; charset=utf-8", 4194304},
    {"/js/globe_photo.js", "tools/mars_lab/build/typescript/globe_photo.js", "text/javascript; charset=utf-8", 4194304},
    {"/js/globe_worker.js", "tools/mars_lab/build/typescript/globe_worker.js", "text/javascript; charset=utf-8", 4194304},
    {"/earth_surface.jpg", "tools/mars_lab/assets/earth_surface.jpg", "image/jpeg", 4194304},
    {"/earth_surface_detail.jpg", "tools/mars_lab/assets/earth_surface_detail.jpg", "image/jpeg", 16777216},
    {"/earth_tiles/00-00.jpg", "tools/mars_lab/assets/earth_tiles/00-00.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/01-00.jpg", "tools/mars_lab/assets/earth_tiles/01-00.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/02-00.jpg", "tools/mars_lab/assets/earth_tiles/02-00.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/03-00.jpg", "tools/mars_lab/assets/earth_tiles/03-00.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/04-00.jpg", "tools/mars_lab/assets/earth_tiles/04-00.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/05-00.jpg", "tools/mars_lab/assets/earth_tiles/05-00.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/06-00.jpg", "tools/mars_lab/assets/earth_tiles/06-00.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/07-00.jpg", "tools/mars_lab/assets/earth_tiles/07-00.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/08-00.jpg", "tools/mars_lab/assets/earth_tiles/08-00.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/09-00.jpg", "tools/mars_lab/assets/earth_tiles/09-00.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/10-00.jpg", "tools/mars_lab/assets/earth_tiles/10-00.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/11-00.jpg", "tools/mars_lab/assets/earth_tiles/11-00.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/00-01.jpg", "tools/mars_lab/assets/earth_tiles/00-01.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/01-01.jpg", "tools/mars_lab/assets/earth_tiles/01-01.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/02-01.jpg", "tools/mars_lab/assets/earth_tiles/02-01.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/03-01.jpg", "tools/mars_lab/assets/earth_tiles/03-01.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/04-01.jpg", "tools/mars_lab/assets/earth_tiles/04-01.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/05-01.jpg", "tools/mars_lab/assets/earth_tiles/05-01.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/06-01.jpg", "tools/mars_lab/assets/earth_tiles/06-01.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/07-01.jpg", "tools/mars_lab/assets/earth_tiles/07-01.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/08-01.jpg", "tools/mars_lab/assets/earth_tiles/08-01.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/09-01.jpg", "tools/mars_lab/assets/earth_tiles/09-01.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/10-01.jpg", "tools/mars_lab/assets/earth_tiles/10-01.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/11-01.jpg", "tools/mars_lab/assets/earth_tiles/11-01.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/00-02.jpg", "tools/mars_lab/assets/earth_tiles/00-02.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/01-02.jpg", "tools/mars_lab/assets/earth_tiles/01-02.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/02-02.jpg", "tools/mars_lab/assets/earth_tiles/02-02.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/03-02.jpg", "tools/mars_lab/assets/earth_tiles/03-02.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/04-02.jpg", "tools/mars_lab/assets/earth_tiles/04-02.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/05-02.jpg", "tools/mars_lab/assets/earth_tiles/05-02.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/06-02.jpg", "tools/mars_lab/assets/earth_tiles/06-02.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/07-02.jpg", "tools/mars_lab/assets/earth_tiles/07-02.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/08-02.jpg", "tools/mars_lab/assets/earth_tiles/08-02.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/09-02.jpg", "tools/mars_lab/assets/earth_tiles/09-02.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/10-02.jpg", "tools/mars_lab/assets/earth_tiles/10-02.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/11-02.jpg", "tools/mars_lab/assets/earth_tiles/11-02.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/00-03.jpg", "tools/mars_lab/assets/earth_tiles/00-03.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/01-03.jpg", "tools/mars_lab/assets/earth_tiles/01-03.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/02-03.jpg", "tools/mars_lab/assets/earth_tiles/02-03.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/03-03.jpg", "tools/mars_lab/assets/earth_tiles/03-03.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/04-03.jpg", "tools/mars_lab/assets/earth_tiles/04-03.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/05-03.jpg", "tools/mars_lab/assets/earth_tiles/05-03.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/06-03.jpg", "tools/mars_lab/assets/earth_tiles/06-03.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/07-03.jpg", "tools/mars_lab/assets/earth_tiles/07-03.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/08-03.jpg", "tools/mars_lab/assets/earth_tiles/08-03.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/09-03.jpg", "tools/mars_lab/assets/earth_tiles/09-03.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/10-03.jpg", "tools/mars_lab/assets/earth_tiles/10-03.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/11-03.jpg", "tools/mars_lab/assets/earth_tiles/11-03.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/00-04.jpg", "tools/mars_lab/assets/earth_tiles/00-04.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/01-04.jpg", "tools/mars_lab/assets/earth_tiles/01-04.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/02-04.jpg", "tools/mars_lab/assets/earth_tiles/02-04.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/03-04.jpg", "tools/mars_lab/assets/earth_tiles/03-04.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/04-04.jpg", "tools/mars_lab/assets/earth_tiles/04-04.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/05-04.jpg", "tools/mars_lab/assets/earth_tiles/05-04.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/06-04.jpg", "tools/mars_lab/assets/earth_tiles/06-04.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/07-04.jpg", "tools/mars_lab/assets/earth_tiles/07-04.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/08-04.jpg", "tools/mars_lab/assets/earth_tiles/08-04.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/09-04.jpg", "tools/mars_lab/assets/earth_tiles/09-04.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/10-04.jpg", "tools/mars_lab/assets/earth_tiles/10-04.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/11-04.jpg", "tools/mars_lab/assets/earth_tiles/11-04.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/00-05.jpg", "tools/mars_lab/assets/earth_tiles/00-05.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/01-05.jpg", "tools/mars_lab/assets/earth_tiles/01-05.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/02-05.jpg", "tools/mars_lab/assets/earth_tiles/02-05.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/03-05.jpg", "tools/mars_lab/assets/earth_tiles/03-05.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/04-05.jpg", "tools/mars_lab/assets/earth_tiles/04-05.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/05-05.jpg", "tools/mars_lab/assets/earth_tiles/05-05.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/06-05.jpg", "tools/mars_lab/assets/earth_tiles/06-05.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/07-05.jpg", "tools/mars_lab/assets/earth_tiles/07-05.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/08-05.jpg", "tools/mars_lab/assets/earth_tiles/08-05.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/09-05.jpg", "tools/mars_lab/assets/earth_tiles/09-05.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/10-05.jpg", "tools/mars_lab/assets/earth_tiles/10-05.jpg", "image/jpeg", 4194304},
    {"/earth_tiles/11-05.jpg", "tools/mars_lab/assets/earth_tiles/11-05.jpg", "image/jpeg", 4194304},
    {"/js/results.js", "tools/mars_lab/build/typescript/results.js", "text/javascript; charset=utf-8", 4194304},
    {"/js/results_services.js", "tools/mars_lab/build/typescript/results_services.js", "text/javascript; charset=utf-8", 4194304},
    {"/js/surface.js", "tools/mars_lab/build/typescript/surface.js", "text/javascript; charset=utf-8", 4194304},
    {"/js/surface_camera.js", "tools/mars_lab/build/typescript/surface_camera.js", "text/javascript; charset=utf-8", 4194304},
    {"/js/surface_export.js", "tools/mars_lab/build/typescript/surface_export.js", "text/javascript; charset=utf-8", 4194304},
    {"/js/surface_hover.js", "tools/mars_lab/build/typescript/surface_hover.js", "text/javascript; charset=utf-8", 4194304},
    {"/js/surface_volume.js", "tools/mars_lab/build/typescript/surface_volume.js", "text/javascript; charset=utf-8", 4194304},
    {"/js/state.js", "tools/mars_lab/build/typescript/state.js", "text/javascript; charset=utf-8", 4194304},
    {"/js/history.js", "tools/mars_lab/build/typescript/history.js", "text/javascript; charset=utf-8", 4194304},
    {"/js/workspace.js", "tools/mars_lab/build/typescript/workspace.js", "text/javascript; charset=utf-8", 4194304},
    {"/js/workspace_controllers.js", "tools/mars_lab/build/typescript/workspace_controllers.js", "text/javascript; charset=utf-8", 4194304},
    {"/js/workspace_native.js", "tools/mars_lab/build/typescript/workspace_native.js", "text/javascript; charset=utf-8", 4194304},
};

static bool lab_svr_asset_request(const websrv_request_t *request, websrv_response_t *response, void *context)
{
    const struct lab_svr_asset *asset = context;
    if (!lab_svr_header(response, "Cache-Control", "no-store") ||
        !lab_svr_header(response, "X-Content-Type-Options", "nosniff"))
        return false;
    if (!lab_svr_request_permitted(request))
        return lab_svr_message(request, response, 403, "MARS Lab is private; use this machine or a private network.");
    return lab_svr_static_asset(request, response, asset->path, asset->type, asset->max_bytes);
}

/* Browser requests stay open while awaiting their result; an observed input close cancels native work. */
static bool lab_svr_calculation_cancelled(void *argument)
{
    return websrv_request_input_closed(argument);
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
        if (lab_svr_equal(path, "/offshore_flag.svg")) {
            jurisdiction_t *catalogue = jurisdict_open(NULL);
            string_t *name = NULL, *svg = NULL;
            bool found = jurisdict_country(catalogue, "ZZ-SEA", &name, &svg) && svg;
            bool ok = found ? lab_svr_header(response, "Content-Type", "image/svg+xml") &&
                              websrv_response_text(response, svg)
                            : lab_svr_message(request, response, 404, "Offshore artwork is not installed");
            string_free(name); string_free(svg); jurisdict_close(catalogue);
            return ok;
        }
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
            json_t *catalogue = lab_page_jurisdiction_index();
            bool ok = catalogue && lab_svr_value(request, response, catalogue);
            json_free(catalogue);
            return ok;
        }
        if (lab_svr_equal(path, "/towns")) {
            const char *target = string_c_str(websrv_request_target(request));
            int64_t cursor = 0;
            double latitude = 0, longitude = 0;
            bool nearby = false;
            const char *jurisdiction = NULL;
            bool valid = !strcmp(target, "/towns");
            if (!strncmp(target, "/towns?after=", 13)) {
                const char *digit = target + 13;
                valid = *digit >= '0' && *digit <= '9';
                for (; valid && *digit && *digit != '&'; ++digit) {
                    valid = *digit >= '0' && *digit <= '9' && cursor <= (INT64_MAX - (*digit - '0')) / 10;
                    if (valid)
                        cursor = cursor * 10 + *digit - '0';
                }
                if (valid && !strncmp(digit, "&jurisdiction=", 14)) {
                    jurisdiction = digit + 14;
                    size_t length = strlen(jurisdiction);
                    valid = length >= 2 && length <= 32;
                    for (size_t i = 0; valid && i < length; ++i)
                        valid = (jurisdiction[i] >= 'A' && jurisdiction[i] <= 'Z') ||
                                (jurisdiction[i] >= '0' && jurisdiction[i] <= '9') || jurisdiction[i] == '-';
                } else if (valid && *digit) {
                    char *end = NULL;
                    valid = !strncmp(digit, "&lat=", 5);
                    if (valid) {
                        latitude = strtod(digit + 5, &end);
                        valid = end != digit + 5 && !strncmp(end, "&lon=", 5);
                    }
                    if (valid) {
                        const char *start = end + 5;
                        longitude = strtod(start, &end);
                        valid = end != start && !*end && isfinite(latitude) && isfinite(longitude) &&
                                fabs(latitude) <= 90 && fabs(longitude) <= 180;
                    }
                    nearby = valid;
                }
            }
            if (!valid)
                return lab_svr_message(request, response, 400, "Invalid town catalogue cursor");
            json_t *batch = jurisdiction ? lab_page_town_jurisdiction_batch(cursor, jurisdiction) :
                           nearby ? lab_page_town_near_batch(cursor, latitude, longitude) : lab_page_town_batch(cursor);
            bool ok = batch && lab_svr_value(request, response, batch);
            json_free(batch);
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
            return lab_svr_static_asset(request, response, "packaging/linux/mars-lab.svg", "image/svg+xml", 4194304);
        if (lab_svr_equal(path, "/apple-touch-icon.png"))
            return lab_svr_static_asset(request, response, "packaging/linux/icon-concepts/wizard-prism-180.png", "image/png", 4194304);
        if (lab_svr_equal(path, "/icon-192.png"))
            return lab_svr_static_asset(request, response, "packaging/linux/icon-concepts/wizard-prism-192.png", "image/png", 4194304);
        if (lab_svr_equal(path, "/icon-512.png"))
            return lab_svr_static_asset(request, response, "packaging/linux/icon-concepts/wizard-prism-512.png", "image/png", 4194304);
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
    if (websrv_request_body_size(request) > 8388608)
        return lab_svr_message(request, response, 413, "Calculation payload exceeds the 8 MiB limit");
    json_t *payload = lab_wire_decode(websrv_request_body(request), websrv_request_body_size(request));
    if (!payload || json_type(payload) != JSON_OBJECT) {
        json_free(payload);
        return lab_svr_message(request, response, 400, "Expected a valid versioned Protobuf request object");
    }
    if (lab_svr_equal(path, "/history")) {
        unsigned status = 400;
        json_t *result = lab_history_request(payload, &status);
        json_free(payload);
        bool ok = result && websrv_response_status(response, status) && lab_svr_value(request, response, result);
        json_free(result);
        return ok;
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
    lab_proc_set_cancel_check(lab_svr_calculation_cancelled, (void *)request);
    json_t *result = presentation ? lab_presentation_request(payload, &status)
                                 : forms ? lab_forms_request(payload, &status) : lab_eval_request(path, payload, &status);
    bool surface = lab_svr_equal(path, "/almanac-globe");
    if (!result)
        result = surface ? lab_surface_service_request(payload, &status) : lab_cal_request(path, payload, &status);
    lab_proc_set_cancel_check(NULL, NULL);
    if (result && !presentation && !forms && !lab_presentation_adapt(result)) {
        json_free(result);
        result = NULL;
        json_free(payload);
        return lab_svr_message(request, response, 500, "Could not prepare native presentation metadata");
    }
    bool ok = result ? websrv_response_status(response, status) && lab_svr_value(request, response, result)
                     : lab_svr_message(request, response, surface ? 503 : 404,
                                       surface ? "Shoreline service unavailable" : "Unknown Lab operation");
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
                                             "/towns",
                                             "/mobile-access",
                                             "/favicon.svg",
                                             "/offshore_flag.svg",
                                             "/apple-touch-icon.png",
                                             "/icon-192.png",
                                             "/icon-512.png",
                                             "/manifest.webmanifest"};
    static const char *const post_routes[] = {"/state",
                                              "/history",
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
                                              "/home-sky",
                                              "/datetime-weather",
                                              "/datetime-jurisdiction-location",
                                              "/almanac-eval",
                                              "/almanac-weather",
                                              "/almanac-globe",
                                              "/almanac-country",
                                              "/almanac-borders",
                                              "/almanac-rivers",
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
        .max_header_bytes = 16384, .max_body_bytes = 33554432, .timeout_ms = timeout_ms ? timeout_ms : 180000};
    server->listener = websrv_new(address, port, &limits);
    if (!server->listener || !lab_svr_register_routes(server)) {
        int error = errno;
        lab_svr_free(server);
        errno = error;
        return NULL;
    }
    /* Serve the shell immediately. Globe requests lazily prepare their native geometry;
     * opening a listener must not decode the world shoreline before serving HTML. */
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
