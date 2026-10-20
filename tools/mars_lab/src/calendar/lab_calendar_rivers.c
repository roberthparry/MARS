/**
 * @file lab_calendar_rivers.c
 * @brief Bounded asynchronous globe river imagery from the jurisdiction catalogue.
 *
 * Converts spatially selected river segments into a transparent SVG overlay. Requests are restricted to close
 * views and bounded output; missing data leave the existing globe usable. Rivers never alter observer corrections.
 */
#include <math.h>

#include "jurisdiction-t.h"
#define MARS_LAB_CALENDAR_INTERNAL_ACCESS
#include "lab_calendar_internal.h"

typedef struct {
    string_t *path;
    double west, north, scale_x, scale_y;
    size_t segments;
} lab_river_paths_t;

static bool lab_river_segment(int64_t id, unsigned kind, double ax, double ay, double bx, double by, void *context)
{
    (void)id;
    (void)kind;
    lab_river_paths_t *p = context;
    return ++p->segments <= 100000 && string_append_format(p->path, "M%.3f %.3fL%.3f %.3f",
        (ax - p->west) * p->scale_x, (p->north - ay) * p->scale_y,
        (bx - p->west) * p->scale_x, (p->north - by) * p->scale_y) >= 0;
}

/* Render an independently requested overlay, never blocking the initial globe response. */
json_t *lab_cal_rivers(const json_t *payload, unsigned *status)
{
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
    jurisdiction_t *j = jurisdict_open(NULL);
    globe_t *r = jurisdict_globe_open(j);
    bool ok = p.path && globe_river_segments(r, b[0], b[1], b[2], b[3], lab_river_segment, &p);
    globe_close(r);
    jurisdict_close(j);
    string_t *image = ok ? string_sprintf(
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"2048\" height=\"2048\" viewBox=\"0 0 2048 2048\">"
        "<path fill=\"none\" stroke=\"#367fb0\" stroke-width=\"2\" stroke-linecap=\"round\" d=\"%s\"/></svg>",
        string_c_str(p.path)) : NULL;
    string_free(p.path);
    if (!image) return lab_cal_error(status, 503, "River data unavailable; install the jurisdiction database");
    json_t *response = json_new_object();
    lab_cal_take(response, "ok", json_new_bool(true));
    lab_cal_set(response, "image", string_c_str(image));
    string_free(image);
    if (status) *status = 200;
    return response;
}
