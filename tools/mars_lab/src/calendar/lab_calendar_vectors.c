/**
 * @file lab_calendar_vectors.c
 * @brief Bounded geographic polygon transport for the live SVG globe surface.
 *
 * Native shoreline selection and clipping retain the nesting order of land, lakes
 * and islands. Small geographic patches make hemisphere clipping unambiguous in
 * the WebAssembly projector, including date-line and polar views. This module
 * exports display geometry only; it never modifies the installed shoreline.
 */
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "globe-t.h"
#include "lab_wire.h"
#define MARS_LAB_CALENDAR_INTERNAL_ACCESS
#include "lab_calendar_internal.h"

typedef struct _lab_vector_output_t {
    string_t *layers[4];
    size_t points;
    size_t bytes;
} lab_vector_output_t;

static globe_reader_t *lab_vector_reader;

/* The service creates this session after fork and uses it on one serial request loop. */
bool lab_cal_surface_open(void)
{
    if (!lab_vector_reader) lab_vector_reader = globe_reader_new();
    return lab_vector_reader != NULL;
}

/* Explicitly release the service's encrypted connection. */
void lab_cal_surface_close(void)
{
    globe_reader_free(lab_vector_reader);
    lab_vector_reader = NULL;
}

static bool lab_vector_point(globe_path_event_t event, double longitude, double latitude,
                             unsigned level, bool antarctic, void *context)
{
    lab_vector_output_t *output = context;
    if (level < 1 || level > 4 || ++output->points > 500000) return false;
    string_t *layer = output->layers[level - 1];
    unsigned char record[17] = {0};
    record[0] = event == GLOBE_PATH_CLOSE ? 0 : event == GLOBE_PATH_MOVE ? level | (antarctic ? 8 : 0) : 16;
    double coordinates[] = {longitude, latitude};
    for (unsigned axis = 0; event != GLOBE_PATH_CLOSE && axis < 2; ++axis) {
        uint64_t bits;
        memcpy(&bits, &coordinates[axis], sizeof bits);
        for (unsigned byte = 0; byte < 8; ++byte) record[1 + axis * 8 + byte] = (unsigned char)(bits >> (byte * 8));
    }
    output->bytes += sizeof record;
    return output->bytes <= 8 * 1048576 - 1024 &&
        string_append_bytes(layer, (const char *)record, sizeof record) == 0;
}

/* Supply geographically clipped polygons, leaving projection and SVG construction to WASM. */
json_t *lab_cal_globe_vectors(const double bounds[4], unsigned *status)
{
    lab_vector_output_t output = {0};
    bool ok = true;
    for (unsigned i = 0; i < 4; ++i) {
        output.layers[i] = string_new();
        ok = ok && output.layers[i];
    }
    globe_t *globe = lab_vector_reader ? globe_reader_get(lab_vector_reader) : globe_open_configured(NULL);
    /* World views are split into small projection patches below; prepare once
     * rather than rereading entire continental rings for every cold patch. */
    if (bounds[2] - bounds[0] > 90 || bounds[3] - bounds[1] > 90)
        ok = ok && globe_surface_prepare(globe);
    /* A regional patch remains far smaller than a hemisphere. The WASM
     * projector subdivides its edges before horizon clipping; ten-degree
     * cells unnecessarily reread the same continental rings nine times. */
    unsigned columns = (unsigned)ceil((bounds[2] - bounds[0]) / 30);
    unsigned rows = (unsigned)ceil((bounds[3] - bounds[1]) / 30);
    double dx = (bounds[2] - bounds[0]) / columns, dy = (bounds[3] - bounds[1]) / rows;
    double tolerance = fmin(bounds[2] - bounds[0], bounds[3] - bounds[1]) / 4096;
    for (unsigned y = 0; ok && y < rows; ++y) {
        for (unsigned x = 0; ok && x < columns; ++x) {
            double west = bounds[0] + x * dx, south = bounds[1] + y * dy;
            ok = globe_surface_paths(globe, west, south, west + dx, south + dy,
                                     tolerance, lab_vector_point, &output);
        }
    }
    if (!lab_vector_reader) globe_close(globe);
    string_t *text = string_new_with("MSH1");
    ok = ok && text;
    for (unsigned i = 0; i < 4; ++i) {
        if (ok) ok = string_append_bytes(text, string_c_str(output.layers[i]), string_byte_length(output.layers[i])) == 0;
        string_free(output.layers[i]);
    }
    json_t *response = ok ? json_new_object() : lab_cal_error(status, 503, "Shoreline vectors unavailable or over budget");
    if (ok) {
        lab_cal_take(response, "ok", json_new_bool(true));
        lab_cal_take(response, "vectors", lab_wire_binary(string_c_str(text), string_byte_length(text)));
    }
    string_free(text);
    return response;
}
