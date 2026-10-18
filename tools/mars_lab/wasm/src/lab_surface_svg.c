/**
 * @file lab_surface_svg.c
 * @brief Generate SVG commands and adjacent-colour groups from projected plot primitives.
 *
 * Consumes native projection output without changing geometry or painter order.
 * Independent segments always start new subpaths, preserving discontinuities.
 * Command pieces accumulate in browser-owned arrays and are joined once per path
 * and colour group, avoiding repeated copying of a growing curve string.
 * Scoped handles bound temporary storage; browser code owns SVG element creation.
 */
#include <math.h>

#include "lab_dom.h"
#include "lab_surface_svg.h"

static int coordinate(int value, double *number)
{
    if (lab_dom_type(value) != LAB_VALUE_NUMBER)
        return 0;
    *number = lab_dom_to_number(value);
    return isfinite(*number);
}

static int commands(int points, int flat, int close)
{
    if (lab_dom_type(points) != LAB_VALUE_ARRAY)
        return 0;
    unsigned count = lab_dom_count(points);
    if (flat) {
        if (count % 2)
            return 0;
        count /= 2;
    }
    if (count < 2 || (!flat && count > 12))
        return 0;
    int pieces = lab_dom_object(LAB_VALUE_ARRAY);
    unsigned mark = lab_dom_mark();
    for (unsigned i = 0; i < count; ++i) {
        int point = flat ? points : lab_dom_item(points, i);
        if (!flat && (lab_dom_type(point) != LAB_VALUE_ARRAY || lab_dom_count(point) != 2)) {
            lab_dom_release(mark);
            return 0;
        }
        unsigned index = flat ? i * 2 : 0;
        double x, y;
        if (!coordinate(lab_dom_item(point, index), &x) || !coordinate(lab_dom_item(point, index + 1), &y)) {
            lab_dom_release(mark);
            return 0;
        }
        lab_dom_push(pieces, lab_dom_format(x, i ? "L" : "M", " "));
        lab_dom_push(pieces, lab_dom_format(y, "", ""));
        lab_dom_release(mark);
    }
    if (close)
        lab_dom_push(pieces, lab_dom_string("Z"));
    return lab_dom_join_array(pieces, "");
}

static void collect(int polygons, int groups, lab_surface_primitive_t kind)
{
    unsigned count = lab_dom_type(polygons) == LAB_VALUE_ARRAY ? lab_dom_count(polygons) : 0;
    unsigned mark = lab_dom_mark();
    int interrupted = 1;
    for (unsigned i = 0; i < count; ++i) {
        int polygon = lab_dom_item(polygons, i);
        int colour = lab_dom_get(polygon, "colour");
        int path = lab_dom_type(colour) == LAB_VALUE_STRING
                       ? commands(lab_dom_get(polygon, "points"), 0, kind == LAB_SURFACE_POLYGONS) : 0;
        if (!path) {
            interrupted = 1;
            lab_dom_release(mark);
            continue;
        }
        unsigned used = lab_dom_count(groups);
        int group = used ? lab_dom_item(groups, used - 1) : 0;
        if (kind == LAB_SURFACE_POLYGONS || interrupted || !group ||
            !lab_dom_equal(colour, lab_dom_get(group, "colour"))) {
            group = lab_dom_object(LAB_VALUE_OBJECT);
            lab_dom_set(group, "colour", colour);
            lab_dom_set(group, "pieces", lab_dom_object(LAB_VALUE_ARRAY));
            lab_dom_push(groups, group);
        }
        lab_dom_push(lab_dom_get(group, "pieces"), path);
        interrupted = 0;
        lab_dom_release(mark);
    }
}

/* Return browser-owned path strings; no SVG nodes or host handles are retained natively. */
void lab_surface_svg_plan(int polygons, int paths, lab_surface_primitive_t kind)
{
    int result = lab_dom_object(LAB_VALUE_OBJECT), series = lab_dom_object(LAB_VALUE_ARRAY),
        axes = lab_dom_object(LAB_VALUE_ARRAY);
    int groups = lab_dom_object(LAB_VALUE_ARRAY);
    collect(polygons, groups, kind);
    unsigned mark = lab_dom_mark();
    unsigned count = lab_dom_count(groups);
    for (unsigned i = 0; i < count; ++i) {
        int group = lab_dom_item(groups, i), entry = lab_dom_object(LAB_VALUE_OBJECT);
        lab_dom_set(entry, "colour", lab_dom_get(group, "colour"));
        lab_dom_set(entry, "path", lab_dom_join_array(lab_dom_get(group, "pieces"), ""));
        lab_dom_push(series, entry);
        lab_dom_release(mark);
    }
    count = lab_dom_type(paths) == LAB_VALUE_ARRAY ? lab_dom_count(paths) : 0;
    for (unsigned i = 0; i < count; ++i) {
        int path = commands(lab_dom_item(paths, i), 1, 0);
        if (path)
            lab_dom_push(axes, path);
        lab_dom_release(mark);
    }
    lab_dom_set(result, "series", series);
    lab_dom_set(result, "axes", axes);
    lab_dom_return(result);
}
