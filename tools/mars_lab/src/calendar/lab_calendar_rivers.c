/**
 * @file lab_calendar_rivers.c
 * @brief Bounded asynchronous river and administrative-border imagery from the independent globe database.
 *
 * Converts spatially selected river segments into a transparent SVG overlay. Requests are restricted to close
 * views and bounded output; missing data leave the existing globe usable. Rivers never alter observer corrections.
 * Shared country borders use their own overview endpoint and never draw generalised administrative coastlines.
 * Vector requests return bounded decimal geographic segments for native browser projection; the legacy SVG
 * response remains available for callers without a vector layer.
 */
#include <math.h>
#include <string.h>

#include "jurisdiction-t.h"
#define MARS_LAB_CALENDAR_INTERNAL_ACCESS
#include "lab_calendar_internal.h"

typedef struct {
    string_t *path;
    string_t *fill;
    globe_t *globe;
    double west, north, scale_x, scale_y;
    size_t segments;
    size_t limit;
    bool vectors;
} lab_river_paths_t;

static bool lab_river_segment(int64_t id, unsigned kind, double ax, double ay, double bx, double by, void *context)
{
    (void)id;
    (void)kind;
    lab_river_paths_t *p = context;
    if (p->globe) {
        double width;
        if (!globe_river_width_metres(p->globe, id, &width)) return false;
        if (width > 0)
            return ++p->segments <= 100000 && lab_cal_river_ribbon(p->fill, width, ax, ay, bx, by,
                p->west, p->north, p->scale_x, p->scale_y);
    }
    return ++p->segments <= (p->limit ? p->limit : 100000) && string_append_format(p->path, "M%.3f %.3fL%.3f %.3f",
        (ax - p->west) * p->scale_x, (p->north - ay) * p->scale_y,
        (bx - p->west) * p->scale_x, (p->north - by) * p->scale_y) >= 0;
}

static bool lab_border_segment(double ax, double ay, double bx, double by, void *context)
{
    lab_river_paths_t *p = context;
    if (p->vectors)
        return ++p->segments <= p->limit &&
            string_append_format(p->path, "%.6f %.6f %.6f %.6f\n", ax, ay, bx, by) >= 0;
    return lab_river_segment(0, 0, ax, ay, bx, by, context);
}

/* Shared administrative land borders are independent of shoreline and river imagery. */
json_t *lab_cal_borders(const json_t *payload, unsigned *status)
{
    double b[4];
    const char *keys[] = {"west", "south", "east", "north"};
    for (unsigned i = 0; i < 4; ++i)
        if (!lab_cal_number(lab_cal_text(payload, keys[i]), &b[i]) || !isfinite(b[i]))
            return lab_cal_error(status, 400, "Invalid border bounds");
    if (b[0] < -360 || b[0] > 180 || b[2] - b[0] < 0.000001 || b[2] - b[0] > 360 ||
        b[1] < -90 || b[3] > 90 || b[3] - b[1] < 0.000001)
        return lab_cal_error(status, 400, "Invalid border bounds");
    double projection_scale = 0;
    const char *scale_text = lab_cal_text(payload, "projection_scale");
    if (*scale_text && (!lab_cal_number(scale_text, &projection_scale) || !isfinite(projection_scale) ||
                       projection_scale <= 0 || projection_scale > 1000000))
        return lab_cal_error(status, 400, "Invalid border display scale");
    /* Bound the projected stroke to one CSS pixel; projection can only shorten it towards the limb. */
    double stroke = projection_scale > 0 ?
        fmin(1.2, 1024 / (projection_scale * fmax(b[2] - b[0], b[3] - b[1]) * 0.017453292519943295)) : 1.2;
    lab_river_paths_t p = {.path = string_new(), .west = b[0], .north = b[3], .limit = 250000,
                           .vectors = !strcmp(lab_cal_text(payload, "format"), "segments"),
                           .scale_x = 1024 / (b[2] - b[0]), .scale_y = 1024 / (b[3] - b[1])};
    globe_t *globe = globe_open_configured(NULL);
    bool ok = p.path && globe_border_segments(globe, b[0], b[1], b[2], b[3], lab_border_segment, &p);
    globe_close(globe);
    if (p.vectors) {
        json_t *response = ok ? json_new_object() : NULL;
        if (response) {
            lab_cal_take(response, "ok", json_new_bool(true));
            lab_cal_set(response, "segments", string_c_str(p.path));
            if (status) *status = 200;
        }
        string_free(p.path);
        return response ? response : lab_cal_error(status, 503, "Country borders unavailable; update the globe database");
    }
    string_t *image = ok ? string_sprintf(
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"2048\" height=\"2048\" viewBox=\"0 0 1024 1024\">"
        "<path fill=\"none\" stroke=\"#dcc7a0\" stroke-width=\"%.6f\" stroke-linecap=\"round\" d=\"%s\"/></svg>",
        stroke, string_c_str(p.path)) : NULL;
    string_free(p.path);
    if (!image) return lab_cal_error(status, 503, "Country borders unavailable; update the globe database");
    json_t *response = json_new_object();
    lab_cal_take(response, "ok", json_new_bool(true));
    lab_cal_set(response, "image", string_c_str(image));
    string_free(image);
    if (status) *status = 200;
    return response;
}

/* Render an independently requested overlay, never blocking the initial globe response. */
json_t *lab_cal_rivers(const json_t *payload, unsigned *status)
{
    const char *format = lab_cal_text(payload, "format");
    if (!strcmp(format, "surface-plan") || !strcmp(format, "surface-data"))
        return lab_cal_river_surface(payload, status);
    double b[4];
    const char *keys[] = {"west", "south", "east", "north"};
    for (unsigned i = 0; i < 4; ++i)
        if (!lab_cal_number(lab_cal_text(payload, keys[i]), &b[i]) || !isfinite(b[i]))
            return lab_cal_error(status, 400, "Invalid river bounds");
    if (b[0] < -360 || b[0] > 180 || b[2] - b[0] < 0.000001 || b[2] - b[0] > 30 ||
        b[1] < -90 || b[3] > 90 || b[3] - b[1] < 0.000001 || b[3] - b[1] > 30)
        return lab_cal_error(status, 400, "River overlays require a close view");
    lab_river_paths_t p = {.path = string_new(), .west = b[0], .north = b[3],
                           .scale_x = 2048 / (b[2] - b[0]), .scale_y = 2048 / (b[3] - b[1])};
    globe_t *r = globe_open_configured(NULL);
    p.globe = r;
    p.fill = string_new();
    double tolerance = fmin(b[2] - b[0], b[3] - b[1]) / 2048;
    bool regional = fmax(b[2] - b[0], b[3] - b[1]) <= 6;
    bool ok = p.path && p.fill && (!regional ||
        globe_river_display_segments(r, b[0], b[1], b[2], b[3], tolerance, lab_river_segment, &p));
    json_t *banks = ok ? lab_cal_river_banks(r, b) : NULL;
    string_t *bank_svg = string_new();
    ok = ok && banks && bank_svg;
    for (size_t i = 0; ok && i < json_array_size(banks); ++i) {
        const string_t *path = json_string_value(json_array_get(banks, i));
        ok = path && string_append_string(bank_svg, path) == 0;
    }
    globe_close(r);
    string_t *image = ok ? string_sprintf(
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"2048\" height=\"2048\" viewBox=\"0 0 2048 2048\">"
        "<path fill=\"none\" stroke=\"#367fb0\" stroke-width=\"2\" stroke-linecap=\"round\" d=\"%s\"/>"
        "<path fill=\"#367fb0\" d=\"%s\"/><path fill=\"#367fb0\" fill-rule=\"nonzero\" d=\"%s\"/></svg>", string_c_str(p.path), string_c_str(p.fill),
        string_c_str(bank_svg)) : NULL;
    string_free(p.path);
    string_free(p.fill);
    string_free(bank_svg); json_free(banks);
    if (!image) return lab_cal_error(status, 503, "River data unavailable; import rivers into the globe database");
    json_t *response = json_new_object();
    lab_cal_take(response, "ok", json_new_bool(true));
    lab_cal_set(response, "image", string_c_str(image));
    string_free(image);
    if (status) *status = 200;
    return response;
}
