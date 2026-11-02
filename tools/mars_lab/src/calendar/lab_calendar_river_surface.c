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
    unsigned pixels;
    bool transparent;
    double longitude_shift;
} surface_regions_t;

static bool surface_region(unsigned slot, const double original[4], void *context)
{
    surface_regions_t *state = context;
    const double *b = state->bounds;
    const double region[] = {original[0] + state->longitude_shift, original[1],
                             original[2] + state->longitude_shift, original[3]};
    /* Broad views use regional overviews, not dozens of full-detail PNGs. */
    if (fmax(b[2] - b[0], b[3] - b[1]) > 1 && fmax(region[2] - region[0], region[3] - region[1]) <= 1)
        return true;
    /* Regions arrive from the spatial index, broad overviews first. Keep the
     * transport and decoded working set bounded even for a continental approach. */
    if (json_array_size(state->photos) >= 68) return true;
    globe_texture_info_t info;
    if (!globe_texture_metadata(state->globe, 3, slot, 0, &info)) return false;
    double left = fmax(b[0], region[0]), right = fmin(b[2], region[2]);
    double south = fmax(b[1], region[1]), north = fmin(b[3], region[3]);
    if (right <= left || north <= south) return true;
    double sx = info.width / (region[2] - region[0]), sy = info.height / (region[3] - region[1]);
    double source[] = {(left - region[0]) * sx, (region[3] - north) * sy,
                       (right - left) * sx, (north - south) * sy};
    double target[] = {(left - b[0]) * state->pixels / (b[2] - b[0]),
                       (b[3] - north) * state->pixels / (b[3] - b[1]),
                       (right - left) * state->pixels / (b[2] - b[0]),
                       (north - south) * state->pixels / (b[3] - b[1])};
    json_t *photo = json_new_object();
    string_t *url = string_sprintf("/earth_regions/%02u.png", slot);
    if (photo && url) {
        lab_cal_set(photo, "url", string_c_str(url));
        lab_cal_set(photo, "sha256", info.sha256);
        lab_cal_take(photo, "source", surface_numbers(source, 4));
        lab_cal_take(photo, "target", surface_numbers(target, 4));
        lab_cal_take(photo, "regional", json_new_bool(true));
        double scale = fmax(target[2] / source[2], target[3] / source[3]);
        /* Stable resolution tiers reuse decoded source photographs across nearby windows and wheel steps. */
        double decode = fmin(info.width, exp2(ceil(log2(fmax(64, info.width * scale)))));
        string_t *width = string_sprintf("%u", info.width), *decoded = string_sprintf("%.0f", decode);
        lab_cal_take(photo, "width", width ? json_new_number(width) : NULL);
        lab_cal_take(photo, "decode_size", decoded ? json_new_number(decoded) : NULL);
        string_free(width); string_free(decoded);
    }
    /* A finer source covering this entire cell supersedes broader regional photographs.
     * Transparent coastline pixels fall through to the globe's already resident world texture. */
    if (photo && url && state->transparent && left <= b[0] && south <= b[1] && right >= b[2] && north >= b[3]) {
        json_free(state->photos);
        state->photos = json_new_array();
    }
    bool ok = photo && url && state->photos && json_array_append(state->photos, photo);
    json_free(photo); string_free(url);
    return ok;
}

static bool surface_regions(surface_regions_t *state)
{
    const double *b = state->bounds;
    int first = (int)floor((b[0] + 180) / 360), last = (int)ceil((b[2] + 180) / 360) - 1;
    if (last - first > 1) return false;
    for (int wrap = first; wrap <= last; ++wrap) {
        state->longitude_shift = wrap * 360;
        double query[] = {fmax(-180, b[0] - state->longitude_shift), fmax(-90, b[1]),
                          fmin(180, b[2] - state->longitude_shift), fmin(90, b[3])};
        if (!globe_texture_regions(state->globe, query, surface_region, state)) return false;
    }
    return true;
}

static json_t *surface_photographs(globe_t *globe, const double b[4], unsigned pixels, bool transparent)
{
    json_t *photos = json_new_array();
    surface_regions_t state = {.globe = globe, .photos = photos, .bounds = b, .pixels = pixels,
                               .transparent = transparent};
    if (transparent) {
        bool ready = photos && surface_regions(&state);
        photos = state.photos;
        if (!ready) { json_free(photos); return NULL; }
        if (json_array_size(photos)) return photos;
    }
    int first = (int)floor((b[0] + 180) / 30), last = (int)ceil((b[2] + 180) / 30) - 1;
    int top = (int)fmax(0, floor((90 - b[3]) / 30)), bottom = (int)fmin(5, ceil((90 - b[1]) / 30) - 1);
    bool ok = photos && first <= last && last - first <= 2 && top >= 0 && bottom < 6 && bottom - top <= 2;
    for (int row = top; ok && row <= bottom; ++row) {
        for (int column = first; ok && column <= last; ++column) {
            unsigned wrapped = (unsigned)((column % 12 + 12) % 12);
            globe_texture_info_t info;
            ok = globe_texture_metadata(globe, 2, wrapped, (unsigned)row, &info);
            double west = column * 30 - 180, north = 90 - row * 30;
            double left = fmax(b[0], west), right = fmin(b[2], west + 30);
            double south = fmax(b[1], north - 30), upper = fmin(b[3], north);
            double source[] = {(left - west) * 60, (north - upper) * 60, (right - left) * 60, (upper - south) * 60};
            double target[] = {(left - b[0]) * pixels / (b[2] - b[0]), (b[3] - upper) * pixels / (b[3] - b[1]),
                               (right - left) * pixels / (b[2] - b[0]), (upper - south) * pixels / (b[3] - b[1])};
            json_t *photo = ok ? json_new_object() : NULL;
            string_t *url = ok ? string_sprintf("/earth_tiles/%02u-%02u.png", wrapped, (unsigned)row) : NULL;
            string_t *width = string_new_with("1800");
            if (photo && url) {
                lab_cal_set(photo, "url", string_c_str(url));
                lab_cal_set(photo, "sha256", info.sha256);
                lab_cal_take(photo, "width", width ? json_new_number(width) : NULL);
                lab_cal_take(photo, "source", surface_numbers(source, 4));
                lab_cal_take(photo, "target", surface_numbers(target, 4));
            }
            ok = photo && url && json_array_append(photos, photo);
            json_free(photo); string_free(url); string_free(width);
        }
    }
    if (ok && !transparent) ok = surface_regions(&state);
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
    bool photograph = !strcmp(lab_cal_text(payload, "format"), "photo-plan");
    bool mask = !strcmp(lab_cal_text(payload, "format"), "mask-plan");
    globe_reader_t *reader = globe_reader_new();
    string_t *identity = globe_reader_identity(reader);
    globe_t *globe = identity ? globe_reader_get(reader) : NULL;
    string_t *key = identity ? string_sprintf("%s:%s:%.12g:%.12g:%.12g:%.12g",
        photograph ? "photo-cell-3" : "river-surface-7", string_c_str(identity), b[0], b[1], b[2], b[3]) : NULL;
    bool ok = key && globe;
    bool rivers = data && ok && globe_has_layer(globe, GLOBE_LAYER_RIVERS);
    bool complete = data && ok && globe_river_water_complete(globe, b[0], b[1], b[2], b[3]);
    bool changed = data && ok && strcmp(lab_cal_text(payload, "key"), string_c_str(key));
    /* A 510-square core has a one-pixel geographic gutter on each side in the existing 512-square GPU slot. */
    double xg = photograph ? (b[2] - b[0]) / 510 : 0, yg = photograph ? (b[3] - b[1]) / 510 : 0;
    const double image_bounds[] = {b[0] - xg, b[1] - yg, b[2] + xg, b[3] + yg};
    json_t *photos = ok && !data && !mask ?
        surface_photographs(globe, image_bounds, photograph ? 512 : 2048, photograph) : NULL;
    river_surface_t surface = {.path = data ? string_new() : NULL, .west = b[0], .north = b[3],
                               .fill = data ? string_new() : NULL, .globe = globe,
                               .sx = 2048 / (b[2] - b[0]), .sy = 2048 / (b[3] - b[1])};
    bool regional = fmax(b[2] - b[0], b[3] - b[1]) <= 6;
    if (data) ok = ok && !changed && surface.path && surface.fill && (!rivers || complete || !regional ||
        globe_river_display_segments(globe, b[0], b[1], b[2], b[3],
            fmin(b[2] - b[0], b[3] - b[1]) / 2048, surface_segment, &surface));
    else ok = ok && (mask || photos);
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
        else if (!mask) { lab_cal_take(result, "photos", photos); photos = NULL; }
        if (status) *status = 200;
    }
    json_free(photos); json_free(banks); string_free(surface.path); string_free(surface.fill);
    string_free(key); string_free(identity); string_free(after);
    globe_reader_free(reader);
    return result ? result : lab_cal_error(status, changed ? 409 : 503,
        changed ? "Globe changed during river preparation" : "River surface data unavailable");
}
