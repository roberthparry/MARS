/**
 * @file lab_calendar_surface.c
 * @brief Native geographic classification and globe imagery for Almanac observers.
 *
 * Opens the local encrypted catalogue and delegates coordinate hit-testing to its opaque shoreline reader.
 * Town names never decide land/sea mode. This adapter does not fetch weather or change observer elevation.
 * Clipped shoreline paths produce transparent SVG outlines or a filled map at the requested zoom resolution.
 * No latitude-based snow or sea-ice colours are invented; the outlines never change geographic classification.
 * Selected-position country requests use the independent globe boundary layer without astronomical evaluation.
 * Town tooltips can request jurisdiction-owned artwork directly by their catalogue ISO country code.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sodium.h>

#include "almanac-t.h"
#include "globe-t.h"
#include "jurisdiction-t.h"
#define MARS_LAB_CALENDAR_INTERNAL_ACCESS
#include "lab_calendar_internal.h"

static void lab_country_catalogue(json_t *result, const char *code)
{
    lab_cal_set(result, "country", "");
    lab_cal_set(result, "country_code", "");
    lab_cal_set(result, "country_flag", "");
    string_t *name = NULL, *svg = NULL;
    jurisdiction_t *jurisdiction = code ? jurisdict_open(NULL) : NULL;
    if (code && jurisdict_country(jurisdiction, code, &name, &svg)) {
        lab_cal_set(result, "country", string_c_str(name));
        lab_cal_set(result, "country_code", code);
        size_t length = svg ? string_byte_length(svg) : 0;
        size_t capacity = sodium_base64_ENCODED_LEN(length, sodium_base64_VARIANT_ORIGINAL);
        char *encoded = length ? malloc(capacity) : NULL;
        if (encoded) {
            sodium_bin2base64(encoded, capacity, (const unsigned char *)string_c_str(svg), length,
                              sodium_base64_VARIANT_ORIGINAL);
            string_t *uri = string_sprintf("data:image/svg+xml;base64,%s", encoded);
            if (uri) lab_cal_set(result, "country_flag", string_c_str(uri));
            string_free(uri); free(encoded);
        }
    }
    jurisdict_close(jurisdiction); string_free(name); string_free(svg);
}

static void lab_country_metadata(json_t *result, globe_t *globe, double latitude, double longitude)
{
    string_t *code = globe_country_code_at(globe, latitude, longitude);
    lab_country_catalogue(result, code ? string_c_str(code) : NULL);
    string_free(code);
}

/* Apply the same geographic policy to named towns, custom locations and asynchronous weather refreshes. */
void lab_cal_surface(json_t *options)
{
    lab_cal_set(options, "country", "");
    lab_cal_set(options, "country_code", "");
    lab_cal_set(options, "country_flag", "");
    double latitude = 0, longitude = 0;
    globe_surface_t surface = GLOBE_UNKNOWN;
    if (lab_cal_number(lab_cal_text(options, "lat"), &latitude) &&
        lab_cal_number(lab_cal_text(options, "lon"), &longitude)) {
        globe_t *globe = globe_open_configured(NULL);
        surface = globe_hit_test(globe, latitude, longitude);
        lab_country_metadata(options, globe, latitude, longitude);
        globe_close(globe);
    }
    static const char *const names[] = {"Unknown", "Land", "Sea", "Inland water", "Coastal"};
    lab_cal_set(options, "observer_surface", names[surface]);
    lab_cal_set(options, "sea_horizon", surface == GLOBE_SEA ? "true" : "false");
}

/* Resolve geographic or catalogue-owned metadata without astronomical calculations or weather requests. */
json_t *lab_cal_country(const json_t *payload, unsigned *status)
{
    const char *code = lab_cal_text(payload, "country_code");
    if (*code) {
        size_t length = strlen(code);
        bool valid = (length == 2 || (length >= 4 && length <= 6 && code[2] == '-')) &&
                     code[0] >= 'A' && code[0] <= 'Z' && code[1] >= 'A' && code[1] <= 'Z';
        for (size_t i = 3; i < length && valid; ++i)
            valid = (code[i] >= 'A' && code[i] <= 'Z') || (code[i] >= '0' && code[i] <= '9');
        if (!valid)
            return lab_cal_error(status, 400, "Invalid country code");
        json_t *response = json_new_object();
        lab_cal_take(response, "ok", json_new_bool(true));
        lab_country_catalogue(response, code);
        return response;
    }
    double latitude, longitude;
    if (!lab_cal_number(lab_cal_text(payload, "lat"), &latitude) ||
        !lab_cal_number(lab_cal_text(payload, "lon"), &longitude) || fabs(latitude) > 90 || fabs(longitude) > 180)
        return lab_cal_error(status, 400, "Invalid country lookup coordinate");
    globe_t *globe = globe_open_configured(NULL);
    json_t *response = json_new_object();
    lab_cal_take(response, "ok", json_new_bool(true));
    lab_country_metadata(response, globe, latitude, longitude);
    globe_close(globe);
    return response;
}

typedef struct {
    string_t *image, *ring, *layers[4];
    double west, north, scale_x, scale_y;
    unsigned vertices;
    size_t total;
    bool map;
} lab_globe_paths_t;

static bool lab_globe_path(globe_path_event_t event, double longitude, double latitude,
                           unsigned level, bool antarctic, void *context)
{
    lab_globe_paths_t *paths = context;
    if (event == GLOBE_PATH_MOVE) {
        string_free(paths->ring);
        paths->ring = string_new();
        if (!paths->ring) return false;
        paths->vertices = 0;
    }
    if (event == GLOBE_PATH_CLOSE) {
        if (paths->vertices >= 3) {
            string_t *layer = paths->layers[level - 1];
            if (string_append_format(layer,
                "<path fill=\"%s\" stroke=\"%s\" stroke-opacity=\"0.45\" stroke-width=\"0.65\" d=\"",
                paths->map ? (antarctic ? "#f3f5f2" : level % 2 ? "#465522" : "#071440") : "none",
                antarctic ? "#e5eeef" : paths->map ? "#74816a" : "#b8d1c7") < 0 ||
                string_append_bytes(layer, string_c_str(paths->ring), string_byte_length(paths->ring)) != 0 ||
                string_append_bytes(layer, "Z\"/>", 4) != 0) return false;
        }
        size_t bytes = 0;
        for (unsigned i = 0; i < 4; ++i) bytes += string_byte_length(paths->layers[i]);
        return bytes <= 8 * 1024 * 1024;
    }
    if (++paths->total > 500000) return false;
    char point[80];
    int length = snprintf(point, sizeof point, "%c%.3f %.3f", paths->vertices++ ? 'L' : 'M',
                          (longitude - paths->west) * paths->scale_x, (paths->north - latitude) * paths->scale_y);
    return length > 0 && (size_t)length < sizeof point &&
           string_append_bytes(paths->ring, point, (size_t)length) == 0;
}

/* Return actual clipped vector polygons; the browser samples their SVG image on the GPU globe. */
json_t *lab_cal_globe(const json_t *payload, unsigned *status)
{
    const char *keys[] = {"west", "south", "east", "north"};
    double bounds[4];
    for (unsigned i = 0; i < 4; ++i)
        if (!lab_cal_number(lab_cal_text(payload, keys[i]), &bounds[i]))
            return lab_cal_error(status, 400, "Invalid globe bounds");
    if (bounds[0] < -360 || bounds[0] > 180 || bounds[2] - bounds[0] < 0.0000001 || bounds[2] - bounds[0] > 360 ||
        bounds[1] < -90 || bounds[3] > 90 || bounds[3] - bounds[1] < 0.0000001)
        return lab_cal_error(status, 400, "Invalid globe bounds");
    lab_globe_paths_t paths = {
        .image = string_new_with("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"1024\" height=\"1024\" "
                                 "viewBox=\"0 0 1024 1024\">"),
        .west = bounds[0], .north = bounds[3],
        .map = !strcmp(lab_cal_text(payload, "view"), "map"),
        .scale_x = 1024 / (bounds[2] - bounds[0]), .scale_y = 1024 / (bounds[3] - bounds[1])
    };
    almanac_t *almanac = almanac_open();
    bool allocated = paths.image != NULL;
    if (allocated && paths.map)
        allocated = string_append_cstr(paths.image, "<path fill=\"#071440\" d=\"M0 0H1024V1024H0Z\"/>") == 0;
    for (unsigned i = 0; i < 4; ++i) {
        paths.layers[i] = string_new();
        allocated = allocated && paths.layers[i] != NULL;
    }
    double tolerance = fmin(bounds[2] - bounds[0], bounds[3] - bounds[1]) / 2048;
    bool ok = allocated && almanac_surface_paths(almanac, bounds[0], bounds[1], bounds[2], bounds[3],
                                                   tolerance, lab_globe_path, &paths);
    almanac_close(almanac);
    string_free(paths.ring);
    for (unsigned i = 0; i < 4; ++i) {
        if (ok) string_append_cstr(paths.image, string_c_str(paths.layers[i]));
        string_free(paths.layers[i]);
    }
    if (!ok) {
        string_free(paths.image);
        return lab_cal_error(status, 503, "Shoreline imagery unavailable; install the Almanac database");
    }
    string_append_cstr(paths.image, "</svg>");
    json_t *response = json_new_object();
    lab_cal_take(response, "ok", json_new_bool(true));
    lab_cal_set(response, "image", string_c_str(paths.image));
    string_free(paths.image);
    return response;
}
