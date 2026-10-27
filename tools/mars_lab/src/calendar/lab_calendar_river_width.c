/**
 * @file lab_calendar_river_width.c
 * @brief Metric width projection shared by photographic river tiles and SVG fallback overlays.
 *
 * Uses local east/north distances to construct river ribbons, including round endpoint caps. Known widths
 * below half a texture pixel are omitted rather than exaggerated. This does not infer missing source widths,
 * replace mapped bank polygons, or change observer geography. Coordinates are already unwrapped and clipped.
 */
#include <math.h>

#include "globe-t.h"
#define MARS_LAB_CALENDAR_INTERNAL_ACCESS
#include "lab_calendar_internal.h"

static bool cap(string_t *path, double x, double y, double rx, double ry)
{
    return string_append_format(path,
        "M%.3f %.3fA%.3f %.3f 0 1 1 %.3f %.3fA%.3f %.3f 0 1 1 %.3f %.3fZ",
        x - rx, y, rx, ry, x + rx, y, rx, ry, x - rx, y) >= 0;
}

typedef struct _bank_paths_t {
    json_t *paths;
    const double *bounds;
    size_t bytes;
} bank_paths_t;

static bool bank_path(const double *coordinates, const size_t *sizes, size_t rings, void *context)
{
    bank_paths_t *state = context;
    const double *b = state->bounds;
    double sx = 2048 / (b[2] - b[0]), sy = 2048 / (b[3] - b[1]);
    double outer_area = 0;
    for (size_t i = 1; i < sizes[0]; ++i) {
        outer_area += (coordinates[2 * (i - 1)] - coordinates[0]) * (coordinates[2 * i + 1] - coordinates[1]) -
                      (coordinates[2 * i] - coordinates[0]) * (coordinates[2 * (i - 1) + 1] - coordinates[1]);
    }
    /* Below a quarter texture pixel, retain the mapped feature for a closer view. */
    if (fabs(outer_area) * sx * sy < 0.5) return true;
    string_t *path = string_new();
    bool ok = path != NULL;
    size_t offset = 0;
    for (size_t ring = 0; ok && ring < rings; ++ring) {
        double area = 0;
        for (size_t i = 1; i < sizes[ring]; ++i) {
            size_t a = offset + i - 1, c = offset + i;
            area += (coordinates[2 * a] - coordinates[2 * offset]) * (coordinates[2 * c + 1] - coordinates[2 * offset + 1]) -
                    (coordinates[2 * c] - coordinates[2 * offset]) * (coordinates[2 * a + 1] - coordinates[2 * offset + 1]);
        }
        bool reverse = ring ? area < 0 : area > 0;
        for (size_t i = 0; ok && i < sizes[ring]; ++i) {
            size_t point = offset + (reverse ? sizes[ring] - 1 - i : i);
            ok = string_byte_length(path) + state->bytes < 6 * 1024 * 1024 && string_append_format(path,
                "%c%.3f %.3f", i ? 'L' : 'M', (coordinates[2 * point] - b[0]) * sx,
                (b[3] - coordinates[2 * point + 1]) * sy) >= 0;
        }
        offset += sizes[ring];
        ok = ok && string_append_cstr(path, "Z") == 0;
    }
    if (path) state->bytes += string_byte_length(path);
    json_t *value = ok ? json_new_string(path) : NULL;
    ok = value && json_array_append(state->paths, value);
    json_free(value); string_free(path);
    return ok;
}

/* Normalise ring winding so one nonzero fill preserves holes and avoids seams between adjoining tiles. */
json_t *lab_cal_river_banks(globe_t *globe, const double bounds[4])
{
    bank_paths_t state = {.paths = json_new_array(), .bounds = bounds};
    if (state.paths && globe_river_banks(globe, bounds[0], bounds[1], bounds[2], bounds[3], bank_path, &state))
        return state.paths;
    json_free(state.paths);
    return NULL;
}

/* Use the same physical width irrespective of segment direction and latitude. */
bool lab_cal_river_ribbon(string_t *path, double width, double ax, double ay, double bx, double by,
                          double west, double north, double sx, double sy)
{
    if (!path || !isfinite(width) || width <= 0 || width > 100000) return false;
    const double metres = 111195.0802335329;
    double cosine = fmax(0.000001, cos((ay + by) * 0.5 * 0.017453292519943295));
    double east = (bx - ax) * cosine, up = by - ay, length = hypot(east, up);
    if (length == 0) return true;
    double rx = width * 0.5 * sx / (metres * cosine), ry = width * 0.5 * sy / metres;
    double nx = -up / length * rx, ny = -east / length * ry;
    if (2 * hypot(nx, ny) < 0.5) return true;
    double x0 = (ax - west) * sx, y0 = (north - ay) * sy;
    double x1 = (bx - west) * sx, y1 = (north - by) * sy;
    /* Clockwise winding matches the circular caps, so overlaps remain filled with the nonzero rule. */
    return string_byte_length(path) < 6 * 1024 * 1024 && string_append_format(path,
        "M%.3f %.3fL%.3f %.3fL%.3f %.3fL%.3f %.3fZ",
        x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny) >= 0 &&
        cap(path, x0, y0, rx, ry) && cap(path, x1, y1, rx, ry);
}
