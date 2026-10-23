/**
 * @file lab_globe_markers.c
 * @brief Native bounded marker decluttering, clipping and geographic coordinate formatting.
 *
 * Uses a fixed direct-indexed bit grid instead of browser string-key sets. The private singleton
 * serialises each operation; its frame is owned by the single browser renderer. Storage is fixed
 * at approximately 44 KiB for viewports up to 16384 CSS pixels. Selected labels retain precedence.
 * This module changes display policy only, not geographic or astronomical source data.
 */
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "lab_dom.h"
#include "lab_serial.h"
#include "lab_globe_markers.h"

typedef struct _marker_store_t {
    lab_serial_t serial;
    double size;
    uint64_t occupied[(516 * 686 + 63) / 64];
} marker_store_t;

static marker_store_t *marker_store(void)
{
    static marker_store_t instance = {.serial = LAB_SERIAL_INITIALIZER};
    return &instance;
}

static void marker_begin(void *context)
{
    marker_store_t *state = marker_store();
    double size = *(double *)context;
    state->size = isfinite(size) && size >= 28 && size <= 16384 ? size : 0;
    memset(state->occupied, 0, sizeof state->occupied);
}

/* Reset the current frame under the native owner's dispatch gate. */
void lab_globe_markers_begin(double size)
{
    lab_serial_dispatch(&marker_store()->serial, marker_begin, &size);
}

static void marker_pixels(double x, double y)
{
    jsref result = lab_dom_object(LAB_VALUE_ARRAY);
    lab_dom_push(result, lab_dom_numeric(x));
    lab_dom_push(result, lab_dom_numeric(y));
    lab_dom_return(result);
}

typedef struct _marker_request_t {
    double x, y, width;
    bool town, selected;
} marker_request_t;

static void marker_place(void *context)
{
    marker_request_t *request = context;
    marker_store_t *state = marker_store();
    double x = request->x, y = request->y, width = request->width, size = state->size;
    lab_dom_return(0);
    if (!size || !isfinite(x) || !isfinite(y) || !isfinite(width) || width < 0 || width > size ||
        x < 0 || x > size || y < 0 || y > size) return;
    if (request->selected) {
        x = fmax(4, fmin(size - width, x + 14));
        y = fmax(14, fmin(size - 14, y));
    }
    if (request->town && (x + width > size || y < 14 || y > size - 14)) return;
    int left = (int)floor((x - 8) / 32) + 1, right = (int)floor((x + width) / 32) + 1;
    int top = (int)floor((y - 15) / 24) + 1, bottom = (int)floor((y + 15) / 24) + 1;
    if (left < 0 || right >= 516 || top < 0 || bottom >= 686) return;
    if (request->town && !request->selected)
        for (int row = top; row <= bottom; ++row)
            for (int col = left; col <= right; ++col) {
                unsigned index = (unsigned)(row * 516 + col);
                if (state->occupied[index / 64] & (UINT64_C(1) << (index % 64))) return;
            }
    for (int row = top; row <= bottom; ++row)
        for (int col = left; col <= right; ++col) {
            unsigned index = (unsigned)(row * 516 + col);
            state->occupied[index / 64] |= UINT64_C(1) << (index % 64);
        }
    marker_pixels(x, y);
}

/* Admit one projected candidate without retaining browser references. */
void lab_globe_markers_place(double x, double y, double width, bool town, bool selected)
{
    marker_request_t request = {x, y, width, town, selected};
    if (!lab_serial_dispatch(&marker_store()->serial, marker_place, &request)) lab_dom_return(0);
}

/* Complete orthographic viewport projection in native code. */
void lab_globe_marker_pixels(jsref point, double zoom, double size)
{
    lab_dom_return(0);
    if (!isfinite(zoom) || zoom < 1 || zoom > 4096 || !isfinite(size) || size <= 0 || size > 16384 ||
        lab_dom_type(point) != LAB_VALUE_ARRAY || lab_dom_count(point) != 3) return;
    double x = lab_dom_to_number(lab_dom_item(point, 0)), y = lab_dom_to_number(lab_dom_item(point, 1));
    double depth = lab_dom_to_number(lab_dom_item(point, 2));
    x = size * (0.5 + x * zoom / 2.24);
    y = size * (0.5 + y * zoom / 2.24);
    if (isfinite(x) && isfinite(y) && isfinite(depth) && depth > 0 && x >= 0 && x <= size && y >= 0 && y <= size)
        marker_pixels(x, y);
}

/* Format rounded minutes without host arithmetic or a WASI formatting dependency. */
void lab_globe_coordinate(double value, jsref positive, jsref negative)
{
    if (!isfinite(value) || fabs(value) > 540) { lab_dom_return(lab_dom_string("")); return; }
    unsigned hundredths = (unsigned)floor(fabs(value) * 6000 + 0.5), minutes = hundredths % 6000;
    jsref text = lab_dom_format(hundredths / 6000, "", "°");
    if (minutes) {
        char fraction[] = {'.', '0' + minutes / 10 % 10, '0' + minutes % 10, 0};
        text = lab_dom_join(text, lab_dom_format(minutes / 100, " ", fraction), "′");
    }
    if (value) text = lab_dom_join(lab_dom_join(text, 0, " "), value < 0 ? negative : positive, "");
    lab_dom_return(text);
}
