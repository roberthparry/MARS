/**
 * @file lab_calendar_river_surface.c
 * @brief Native preparation of bounded photographic river surface tiles.
 *
 * Supplies exact image crops and centreline paths for asynchronous browser rasterisation. A metadata identity
 * invalidates cached tiles when globe storage changes. Plans never scan river geometry; paths are requested only
 * on cache misses. No observer, history, jurisdiction or source database is modified.
 */
#include <math.h>
#include <string.h>

#include "globe-t.h"
#define MARS_LAB_CALENDAR_INTERNAL_ACCESS
#include "lab_calendar_internal.h"

typedef struct _river_surface_t {
    string_t *path;
    string_t *fill;
    globe_t *globe;
    double west, north, sx, sy;
    size_t segments;
} river_surface_t;

static json_t *surface_numbers(const double *values, size_t count)
{
    json_t *array = json_new_array();
    for (size_t i = 0; array && i < count; ++i) {
        string_t *text = string_sprintf("%.9f", values[i]);
        json_t *number = text ? json_new_number(text) : NULL;
        if (!number || !json_array_append(array, number)) { json_free(array); array = NULL; }
        json_free(number); string_free(text);
    }
    return array;
}

static bool surface_segment(int64_t id, unsigned kind, double ax, double ay, double bx, double by, void *context)
{
    (void)kind;
    river_surface_t *surface = context;
    double width;
    if (!globe_river_width_metres(surface->globe, id, &width)) return false;
    if (width > 0)
        return ++surface->segments <= 100000 && lab_cal_river_ribbon(surface->fill, width, ax, ay, bx, by,
            surface->west, surface->north, surface->sx, surface->sy);
    return ++surface->segments <= 100000 && string_append_format(surface->path, "M%.3f %.3fL%.3f %.3f",
        (ax - surface->west) * surface->sx, (surface->north - ay) * surface->sy,
        (bx - surface->west) * surface->sx, (surface->north - by) * surface->sy) >= 0;
}

typedef struct {
    globe_t *globe;
    json_t *photos;
    const double *bounds;
} surface_regions_t;

static bool surface_region(unsigned slot, const double region[4], void *context)
{
    surface_regions_t *state = context;
    const double *b = state->bounds;
    /* Broad views use regional overviews, not dozens of full-detail PNGs. */
    if (fmax(b[2] - b[0], b[3] - b[1]) > 1 && fmax(region[2] - region[0], region[3] - region[1]) <= 1)
        return true;
    globe_texture_info_t info;
    if (!globe_texture_metadata(state->globe, 3, slot, 0, &info)) return false;
    double left = fmax(b[0], region[0]), right = fmin(b[2], region[2]);
    double south = fmax(b[1], region[1]), north = fmin(b[3], region[3]);
    double sx = info.width / (region[2] - region[0]), sy = info.height / (region[3] - region[1]);
    double source[] = {(left - region[0]) * sx, (region[3] - north) * sy,
                       (right - left) * sx, (north - south) * sy};
    double target[] = {(left - b[0]) * 2048 / (b[2] - b[0]), (b[3] - north) * 2048 / (b[3] - b[1]),
                       (right - left) * 2048 / (b[2] - b[0]), (north - south) * 2048 / (b[3] - b[1])};
    json_t *photo = json_new_object();
    string_t *url = string_sprintf("/earth_regions/%02u.jpg", slot);
    if (photo && url) {
        lab_cal_set(photo, "url", string_c_str(url));
        lab_cal_set(photo, "sha256", info.sha256);
        lab_cal_take(photo, "source", surface_numbers(source, 4));
        lab_cal_take(photo, "target", surface_numbers(target, 4));
        lab_cal_take(photo, "regional", json_new_bool(true));
    }
    bool ok = photo && url && json_array_append(state->photos, photo);
    json_free(photo); string_free(url);
    return ok;
}

static json_t *surface_photographs(globe_t *globe, const double b[4])
{
    json_t *photos = json_new_array();
    int first = (int)floor((b[0] + 180) / 30), last = (int)ceil((b[2] + 180) / 30) - 1;
    int top = (int)floor((90 - b[3]) / 30), bottom = (int)ceil((90 - b[1]) / 30) - 1;
    bool ok = photos && first <= last && last - first <= 1 && top >= 0 && bottom < 6 && bottom - top <= 1;
    for (int row = top; ok && row <= bottom; ++row) {
        for (int column = first; ok && column <= last; ++column) {
            unsigned wrapped = (unsigned)((column % 12 + 12) % 12);
            globe_texture_info_t info;
            ok = globe_texture_metadata(globe, 2, wrapped, (unsigned)row, &info);
            double west = column * 30 - 180, north = 90 - row * 30;
            double left = fmax(b[0], west), right = fmin(b[2], west + 30);
            double south = fmax(b[1], north - 30), upper = fmin(b[3], north);
            double source[] = {(left - west) * 60, (north - upper) * 60, (right - left) * 60, (upper - south) * 60};
            double target[] = {(left - b[0]) * 2048 / (b[2] - b[0]), (b[3] - upper) * 2048 / (b[3] - b[1]),
                               (right - left) * 2048 / (b[2] - b[0]), (upper - south) * 2048 / (b[3] - b[1])};
            json_t *photo = ok ? json_new_object() : NULL;
            string_t *url = ok ? string_sprintf("/earth_tiles/%02u-%02u.jpg", wrapped, (unsigned)row) : NULL;
            if (photo && url) {
                lab_cal_set(photo, "url", string_c_str(url));
                lab_cal_set(photo, "sha256", info.sha256);
                lab_cal_take(photo, "source", surface_numbers(source, 4));
                lab_cal_take(photo, "target", surface_numbers(target, 4));
            }
            ok = photo && url && json_array_append(photos, photo);
            json_free(photo); string_free(url);
        }
    }
    surface_regions_t state = {globe, photos, b};
    if (ok && fmax(b[2] - b[0], b[3] - b[1]) <= 4)
        ok = globe_texture_regions(globe, b, surface_region, &state);
    if (ok) return photos;
    json_free(photos);
    return NULL;
}

/* Plan first, and query geometry only when the browser lacks the corresponding completed surface tile. */
json_t *lab_cal_river_surface(const json_t *payload, unsigned *status)
{
    static const char *const names[] = {"west", "south", "east", "north"};
    double b[4];
    for (unsigned i = 0; i < 4; ++i)
        if (!lab_cal_number(lab_cal_text(payload, names[i]), &b[i]) || !isfinite(b[i]))
            return lab_cal_error(status, 400, "Invalid river surface bounds");
    if (b[0] < -360 || b[0] > 180 || b[2] - b[0] < .000001 || b[2] - b[0] > 30 ||
        b[1] < -90 || b[3] > 90 || b[3] - b[1] < .000001 || b[3] - b[1] > 30)
        return lab_cal_error(status, 400, "River surface tiles require a close view");
    bool data = !strcmp(lab_cal_text(payload, "format"), "surface-data");
    globe_reader_t *reader = globe_reader_new();
    string_t *identity = globe_reader_identity(reader);
    globe_t *globe = identity ? globe_reader_get(reader) : NULL;
    string_t *key = identity ? string_sprintf("river-surface-5:%s:%.12g:%.12g:%.12g:%.12g",
        string_c_str(identity), b[0], b[1], b[2], b[3]) : NULL;
    bool ok = key && globe;
    bool rivers = ok && globe_has_layer(globe, GLOBE_LAYER_RIVERS);
    bool complete = ok && globe_river_water_complete(globe, b[0], b[1], b[2], b[3]);
    bool changed = data && ok && strcmp(lab_cal_text(payload, "key"), string_c_str(key));
    json_t *photos = ok && !data ? surface_photographs(globe, b) : NULL;
    river_surface_t surface = {.path = data ? string_new() : NULL, .west = b[0], .north = b[3],
                               .fill = data ? string_new() : NULL, .globe = globe,
                               .sx = 2048 / (b[2] - b[0]), .sy = 2048 / (b[3] - b[1])};
    bool regional = fmax(b[2] - b[0], b[3] - b[1]) <= 6;
    if (data) ok = ok && !changed && surface.path && surface.fill && (!rivers || complete || !regional ||
        globe_river_display_segments(globe, b[0], b[1], b[2], b[3],
            fmin(b[2] - b[0], b[3] - b[1]) / 2048, surface_segment, &surface));
    else ok = ok && photos;
    json_t *banks = ok && data ? (rivers ? lab_cal_river_banks(globe, b) : json_new_array()) : NULL;
    if (data) ok = ok && banks;
    string_t *after = ok ? globe_reader_identity(reader) : NULL;
    changed = changed || (after && string_compare(identity, after));
    ok = ok && after && !changed;
    json_t *result = ok ? json_new_object() : NULL;
    if (result) {
        lab_cal_take(result, "ok", json_new_bool(true));
        lab_cal_set(result, "key", string_c_str(key));
        lab_cal_take(result, "bounds", surface_numbers(b, 4));
        if (data) {
            lab_cal_take(result, "complete", json_new_bool(complete));
            lab_cal_set(result, "path", string_c_str(surface.path));
            lab_cal_set(result, "fill", string_c_str(surface.fill));
            lab_cal_take(result, "banks", banks); banks = NULL;
        }
        else { lab_cal_take(result, "photos", photos); photos = NULL; }
        if (status) *status = 200;
    }
    json_free(photos); json_free(banks); string_free(surface.path); string_free(surface.fill);
    string_free(key); string_free(identity); string_free(after);
    globe_reader_free(reader);
    return result ? result : lab_cal_error(status, changed ? 409 : 503,
        changed ? "Globe changed during river preparation" : "River surface data unavailable");
}
