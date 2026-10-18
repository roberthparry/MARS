/**
 * @file lab_surface_mesh.c
 * @brief Prepare retained primitives and GPU attributes from native SVG metadata.
 *
 * Validates normalised coordinates and units, expands RGB colours and triangulates
 * polygon fans. Browser code owns the resulting arrays and GPU upload. No handles
 * survive the synchronous call; per-record scopes bound temporary handle usage.
 * Processing is linear in the supplied display geometry and does not evaluate expressions.
 */
#include "lab_dom.h"
#include "lab_surface_mesh.h"

static int coordinates(int text, double *values, unsigned minimum, unsigned maximum)
{
    int parts = lab_dom_split(text, " ");
    unsigned count = lab_dom_count(parts);
    if (count < minimum || count > maximum || count % 3) return 0;
    for (unsigned i = 0; i < count; ++i) {
        values[i] = lab_dom_to_number(lab_dom_item(parts, i));
        if (!__builtin_isfinite(values[i])) return 0;
    }
    return (int)count;
}

static int vector(const double *values, unsigned count)
{
    int result = lab_dom_object(4);
    for (unsigned i = 0; i < count; ++i) lab_dom_push(result, lab_dom_numeric(values[i]));
    return result;
}

static void primitive(int primitives, int vertices, const double *points, unsigned count, int colour,
                      const double rgb[3])
{
    int entry = lab_dom_object(5), positions = lab_dom_object(4);
    for (unsigned i = 0; i < count; ++i) {
        lab_dom_push(positions, vector(points + i * 3, 3));
        for (unsigned axis = 0; axis < 3; ++axis)
            lab_dom_push(vertices, lab_dom_numeric(points[i * 3 + axis]));
        for (unsigned axis = 0; axis < 3; ++axis) lab_dom_push(vertices, lab_dom_numeric(rgb[axis]));
        for (unsigned axis = 0; axis < 3; ++axis)
            lab_dom_push(vertices, lab_dom_numeric(count == 2 || axis == i));
    }
    lab_dom_set(entry, "points", positions);
    lab_dom_set(entry, "colour", colour);
    lab_dom_set(entry, "depth", lab_dom_numeric(0));
    lab_dom_push(primitives, entry);
}

static int records(int chart, int primitives, int vertices, int segments)
{
    int paths = lab_dom_all(chart, segments ? "[data-segment]" : "[data-vertices]");
    unsigned count = lab_dom_count(paths), mark = lab_dom_mark();
    for (unsigned i = 0; i < count; ++i) {
        int path = lab_dom_item(paths, i);
        double points[15];
        unsigned used = coordinates(lab_dom_read(path, 3, segments ? "segment" : "vertices"),
                                    points, segments ? 6 : 9, segments ? 6 : 15);
        if (!used) return 0;
        const char *attribute = segments ? "stroke" : "fill";
        int colour = lab_dom_has(path, attribute) ? lab_dom_read(path, 2, attribute) : lab_dom_string("#000000");
        double parsed = lab_dom_parse_int(lab_dom_slice(colour, 1, (int)lab_dom_length(colour)), 16);
        if (!__builtin_isfinite(parsed) || parsed < 0 || parsed > 16777215) return 0;
        unsigned packed = (unsigned)parsed;
        double rgb[] = {(packed >> 16 & 255) / 255.0, (packed >> 8 & 255) / 255.0, (packed & 255) / 255.0};
        if (segments) {
            primitive(primitives, vertices, points, 2, colour, rgb);
        } else {
            for (unsigned next = 3; next + 3 < used; next += 3) {
                double triangle[9];
                for (unsigned axis = 0; axis < 3; ++axis) {
                    triangle[axis] = points[axis];
                    triangle[3 + axis] = points[next + axis];
                    triangle[6 + axis] = points[next + 3 + axis];
                }
                primitive(primitives, vertices, triangle, 3, colour, rgb);
            }
        }
        lab_dom_release(mark);
    }
    return 1;
}

/* Decode once at view creation; browser renderers consume the same prepared geometry. */
void lab_surface_mesh(int chart)
{
    double units[3];
    if (!coordinates(lab_dom_read(chart, 3, "units"), units, 3, 3) ||
        units[0] <= 0 || units[1] <= 0 || units[2] <= 0) {
        lab_dom_return(0);
        return;
    }
    int result = lab_dom_object(5), primitives = lab_dom_object(4), vertices = lab_dom_object(4);
    if (!records(chart, primitives, vertices, 1) || !records(chart, primitives, vertices, 0) ||
        (!lab_dom_count(primitives) && !lab_dom_equal(lab_dom_read(chart, 3, "empty"), lab_dom_string("true")))) {
        lab_dom_return(0);
        return;
    }
    lab_dom_set(result, "primitives", primitives);
    lab_dom_set(result, "vertices", vertices);
    lab_dom_set(result, "units", vector(units, 3));
    lab_dom_return(result);
}
