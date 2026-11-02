/**
 * @file lab_photo_source.c
 * @brief Fixed photographic source-cell keys, gutters and exact crop mappings.
 *
 * Enumerates the bounded pixel grid of one installed photograph. Native arithmetic maps only intersecting
 * source cores to the destination; the browser decodes each source once and persists its reusable cells.
 */
#include <math.h>
#include <string.h>

#include "lab_dom.h"
#include "lab_photo_source.h"

static jsref rectangle(double x, double y, double width, double height)
{
    jsref value = lab_dom_object(LAB_VALUE_ARRAY);
    const double coordinates[] = {x, y, width, height};
    for (unsigned i = 0; i < 4; ++i) lab_dom_push(value, lab_dom_numeric(coordinates[i]));
    return value;
}

/* Describe storage and painting rectangles; no browser resource or handle is retained. */
void lab_photo_source(jsref photo)
{
    double width = lab_dom_to_number(lab_dom_get(photo, "width")), source[4], target[4];
    jsref s = lab_dom_get(photo, "source"), t = lab_dom_get(photo, "target");
    char url[64] = {0}, digest[65] = {0};
    int u = lab_dom_utf8_copy(lab_dom_get(photo, "url"), (unsigned char *)url, sizeof url - 1);
    int d = lab_dom_utf8_copy(lab_dom_get(photo, "sha256"), (unsigned char *)digest, sizeof digest - 1);
    if ((width != 1800 && width != 3600) || lab_dom_count(s) != 4 || lab_dom_count(t) != 4 ||
        u <= 0 || d != 64) { lab_dom_return(0); return; }
    for (unsigned i = 0; i < 4; ++i) {
        source[i] = lab_dom_to_number(lab_dom_item(s, i));
        target[i] = lab_dom_to_number(lab_dom_item(t, i));
        if (!isfinite(source[i]) || !isfinite(target[i])) { lab_dom_return(0); return; }
    }
    if (source[0] < 0 || source[1] < 0 || source[2] <= 0 || source[3] <= 0 || target[2] <= 0 || target[3] <= 0 ||
        source[0] + source[2] > width + .001 || source[1] + source[3] > width + .001) { lab_dom_return(0); return; }
    jsref parts = lab_dom_object(LAB_VALUE_ARRAY);
    unsigned count = (unsigned)ceil(width / 512);
    for (unsigned row = 0; row < count; ++row) {
        for (unsigned column = 0; column < count; ++column) {
            double x = column * 512, y = row * 512;
            double left = fmax(x, source[0]), top = fmax(y, source[1]);
            double right = fmin(fmin(width, x + 512), source[0] + source[2]);
            double bottom = fmin(fmin(width, y + 512), source[1] + source[3]);
            double rx = fmax(0, x - 1), ry = fmax(0, y - 1);
            double rw = fmin(width, x + 513) - rx, rh = fmin(width, y + 513) - ry;
            jsref part = lab_dom_object(LAB_VALUE_OBJECT);
            jsref key = lab_dom_join(lab_dom_get(photo, "url"), lab_dom_string(":"), "");
            key = lab_dom_join(key, lab_dom_get(photo, "sha256"), ":");
            key = lab_dom_join(key, lab_dom_format(row, "", ":"), "");
            key = lab_dom_join(key, lab_dom_format(column, "", ":"), "");
            lab_dom_set(part, "key", key);
            lab_dom_set(part, "read", rectangle(rx, ry, rw, rh));
            lab_dom_set(part, "write", rectangle(rx - x + 1, ry - y + 1, rw, rh));
            bool needed = right > left && bottom > top;
            lab_dom_set(part, "needed", lab_dom_numeric(needed));
            if (needed) {
                lab_dom_set(part, "source", rectangle(left - x + 1, top - y + 1, right - left, bottom - top));
                lab_dom_set(part, "target", rectangle(target[0] + (left - source[0]) * target[2] / source[2],
                    target[1] + (top - source[1]) * target[3] / source[3], (right - left) * target[2] / source[2],
                    (bottom - top) * target[3] / source[3]));
            }
            lab_dom_push(parts, part);
        }
    }
    lab_dom_return(parts);
}
