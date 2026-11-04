/**
 * @file lab_photo_source.c
 * @brief Fixed photographic source-cell keys, gutters and exact crop mappings.
 *
 * Chooses a stable decoded resolution from the destination's pixel demand, then enumerates that photograph's
 * bounded source grid. An optional native cache provider reuses adequate higher-resolution pixels across zoom
 * changes. Resolution-specific keys still prevent a coarse cached source from replacing finer detail.
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

/* Check installed dimensions in native code before the browser decodes directly to the selected tier. */
bool lab_photo_source_header(jsref bytes, unsigned width)
{
    static const unsigned char prefix[] = {137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82};
    unsigned char header[24];
    if ((width != 1800 && width != 3600) || lab_dom_bytes_copy(bytes, header, sizeof header) != (int)sizeof header ||
        memcmp(header, prefix, sizeof prefix)) return false;
    for (unsigned i = 0; i < 4; ++i) {
        unsigned char expected = (unsigned char)(width >> (24 - 8 * i));
        if (header[16 + i] != expected || header[20 + i] != expected) return false;
    }
    return true;
}

static jsref source_key(jsref photo, double width)
{
    jsref key = lab_dom_join(lab_dom_get(photo, "url"), lab_dom_get(photo, "sha256"), ":");
    return lab_dom_join(key, lab_dom_format(width, "", ":"), ":");
}

static jsref block_key(jsref source, unsigned row, unsigned column)
{
    jsref key = lab_dom_join(source, lab_dom_format(row, "", ":"), "");
    return lab_dom_join(key, lab_dom_format(column, "", ":"), "");
}

static bool cached_key(jsref key, lab_photo_source_cached_fn cached, void *context)
{
    char text[256] = {0};
    int size = lab_dom_utf8_copy(key, (unsigned char *)text, sizeof text - 1);
    return size > 0 && size < (int)sizeof text && cached(text, context);
}

/* Probe explicit content addresses; a missing block rejects that tier immediately. */
static bool cached_source(jsref photo, double installed, double decoded, const double source[4],
                          lab_photo_source_cached_fn cached, void *context)
{
    unsigned mark = lab_dom_mark();
    jsref key = source_key(photo, decoded);
    bool available = cached_key(key, cached, context);
    if (!available) {
        unsigned left = (unsigned)floor(source[0] * decoded / (installed * 512));
        unsigned top = (unsigned)floor(source[1] * decoded / (installed * 512));
        unsigned right = (unsigned)ceil((source[0] + source[2]) * decoded / (installed * 512));
        unsigned bottom = (unsigned)ceil((source[1] + source[3]) * decoded / (installed * 512));
        available = right > left && bottom > top;
        for (unsigned row = top; available && row < bottom; ++row) {
            for (unsigned column = left; available && column < right; ++column) {
                unsigned part_mark = lab_dom_mark();
                available = cached_key(block_key(key, row, column), cached, context);
                lab_dom_release(part_mark);
            }
        }
    }
    lab_dom_release(mark);
    return available;
}

/* Describe storage and painting rectangles through an optional native cache provider. */
jsref lab_photo_source_plan(jsref photo, lab_photo_source_cached_fn cached, void *context)
{
    double width = lab_dom_to_number(lab_dom_get(photo, "width")), source[4], target[4];
    jsref s = lab_dom_get(photo, "source"), t = lab_dom_get(photo, "target");
    char url[64] = {0}, digest[65] = {0};
    int u = lab_dom_utf8_copy(lab_dom_get(photo, "url"), (unsigned char *)url, sizeof url - 1);
    int d = lab_dom_utf8_copy(lab_dom_get(photo, "sha256"), (unsigned char *)digest, sizeof digest - 1);
    if ((width != 1800 && width != 3600) || lab_dom_count(s) != 4 || lab_dom_count(t) != 4 ||
        u <= 0 || d != 64) return 0;
    for (unsigned i = 0; i < 4; ++i) {
        source[i] = lab_dom_to_number(lab_dom_item(s, i));
        target[i] = lab_dom_to_number(lab_dom_item(t, i));
        if (!isfinite(source[i]) || !isfinite(target[i])) return 0;
    }
    if (source[0] < 0 || source[1] < 0 || source[2] <= 0 || source[3] <= 0 || target[2] <= 0 || target[3] <= 0 ||
        source[0] + source[2] > width + .001 || source[1] + source[3] > width + .001) return 0;
    double demand = width * fmax(target[2] / source[2], target[3] / source[3]);
    double decoded = fmin(width, exp2(ceil(log2(fmax(64, demand)))));
    if (!isfinite(demand) || !isfinite(decoded)) return 0;
    bool reused = false;
    for (double candidate = decoded; cached; candidate = fmin(width, candidate * 2)) {
        if (cached_source(photo, width, candidate, source, cached, context)) {
            decoded = candidate; reused = true; break;
        }
        if (candidate == width) break;
    }
    for (unsigned i = 0; i < 4; ++i) source[i] *= decoded / width;
    jsref plan = lab_dom_object(LAB_VALUE_OBJECT);
    jsref key = source_key(photo, decoded);
    lab_dom_set(plan, "key", key);
    lab_dom_set(plan, "width", lab_dom_numeric(decoded));
    lab_dom_set(plan, "cached", lab_dom_scalar(LAB_VALUE_BOOLEAN, reused));
    lab_dom_set(plan, "prefix", lab_dom_join(lab_dom_get(photo, "url"), lab_dom_get(photo, "sha256"), ":"));
    /* One large coastal overview must not evict an entire neighbourhood of smaller decoded photographs.
     * Its intersecting 514-square blocks remain reusable in the shared 64 MiB working cache instead. */
    lab_dom_set(plan, "retain", lab_dom_scalar(LAB_VALUE_BOOLEAN, decoded <= 2048));
    lab_dom_set(plan, "source", rectangle(source[0], source[1], source[2], source[3]));
    width = decoded;
    jsref parts = lab_dom_object(LAB_VALUE_ARRAY);
    unsigned count = (unsigned)ceil(width / 512);
    unsigned first_column = (unsigned)floor(source[0] / 512), first_row = (unsigned)floor(source[1] / 512);
    unsigned last_column = (unsigned)fmin(count, ceil((source[0] + source[2]) / 512));
    unsigned last_row = (unsigned)fmin(count, ceil((source[1] + source[3]) / 512));
    for (unsigned row = first_row; row < last_row; ++row) {
        for (unsigned column = first_column; column < last_column; ++column) {
            double x = column * 512, y = row * 512;
            double left = fmax(x, source[0]), top = fmax(y, source[1]);
            double right = fmin(fmin(width, x + 512), source[0] + source[2]);
            double bottom = fmin(fmin(width, y + 512), source[1] + source[3]);
            double rx = fmax(0, x - 1), ry = fmax(0, y - 1);
            double rw = fmin(width, x + 513) - rx, rh = fmin(width, y + 513) - ry;
            jsref part = lab_dom_object(LAB_VALUE_OBJECT);
            jsref part_key = block_key(key, row, column);
            lab_dom_set(part, "key", part_key);
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
    lab_dom_set(plan, "parts", parts);
    return plan;
}

/* The standalone planner has no cache ownership or retained browser state. */
void lab_photo_source(jsref photo)
{
    lab_dom_return(lab_photo_source_plan(photo, NULL, NULL));
}
