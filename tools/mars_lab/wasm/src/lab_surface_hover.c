/**
 * @file lab_surface_hover.c
 * @brief Pick retained curve segments in screen pixels without interpreting expressions.
 *
 * Each endpoint pair is independent, preserving discontinuities. Work is bounded
 * to 65536 retained primitives per bridge call. Per-primitive handle scopes keep
 * transient storage constant even when every primitive is rejected.
 * Interpolation uses the sampled bounds, not the camera's selected volume.
 * Native inverse projection admits pointers and markers only inside the chart.
 */
#include <math.h>

#include "lab_dom.h"
#include "lab_surface_hover.h"

static double number(browser_handle object, const char *key)
{
    return lab_dom_to_number(lab_dom_get(object, key));
}

static double item(browser_handle array, unsigned index)
{
    return lab_dom_to_number(lab_dom_item(array, index));
}

static int inside_chart(const double transform[6], double determinant, double width, double height,
                        double x, double y)
{
    double local_x = ((x - transform[4]) * transform[3] - (y - transform[5]) * transform[2]) / determinant;
    double local_y = ((y - transform[5]) * transform[0] - (x - transform[4]) * transform[1]) / determinant;
    return isfinite(local_x) && isfinite(local_y) &&
           local_x >= 0 && local_x <= width && local_y >= 0 && local_y <= height;
}

/* Return the nearest visible segment within ten CSS pixels, or null. */
void lab_surface_hover(view_handle view, frame_handle frame, browser_handle screen,
                       double mouse_x, double mouse_y, browser_handle previous)
{
    int primitives = lab_dom_get(view, "primitives"), rows = lab_dom_get(frame, "rows");
    unsigned count = lab_dom_count(primitives);
    if (count > 65536) {
        lab_dom_return(0);
        return;
    }
    int scaling = lab_dom_get(frame, "scaling"), offset = lab_dom_get(frame, "offset");
    double rotation[6], scale[3], shift[3], transform[6];
    static const char *const matrix_keys[] = {"a", "b", "c", "d", "e", "f"};
    for (unsigned i = 0; i < 6; ++i) {
        rotation[i] = item(rows, i);
        transform[i] = number(screen, matrix_keys[i]);
        if (!isfinite(transform[i])) {
            lab_dom_return(0);
            return;
        }
    }
    for (unsigned i = 0; i < 3; ++i) {
        scale[i] = item(scaling, i);
        shift[i] = item(offset, i);
    }
    double width = number(frame, "width"), height = number(frame, "height"), fit = number(frame, "fit");
    double determinant = transform[0] * transform[3] - transform[1] * transform[2];
    if (!isfinite(determinant) || !determinant ||
        !isfinite(width) || !isfinite(height) || width <= 0 || height <= 0 ||
        !isfinite(mouse_x) || !isfinite(mouse_y) ||
        !inside_chart(transform, determinant, width, height, mouse_x, mouse_y)) {
        lab_dom_return(0);
        return;
    }
    int result = previous, found = 0;
    double best = previous ? number(previous, "distance") : 100;
    double chosen[3] = {0}, chosen_x = 0, chosen_y = 0;
    if (!lab_dom_truth(lab_dom_get(frame, "drawMesh"))) {
        lab_dom_return(previous);
        return;
    }
    /* Picking requires examining every independent pair; the list has a fixed admission limit. */
    for (unsigned i = 0; i < count; ++i) {
        unsigned mark = lab_dom_mark();
        int points = lab_dom_get(lab_dom_item(primitives, i), "points");
        if (lab_dom_count(points) != 2) {
            lab_dom_release(mark);
            continue;
        }
        double raw[2][3], p[2][3], start = 0, end = 1;
        int valid = 1;
        for (unsigned axis = 0; axis < 3; ++axis) {
            for (unsigned j = 0; j < 2; ++j) {
                raw[j][axis] = item(lab_dom_item(points, j), axis);
                p[j][axis] = (raw[j][axis] - 0.5) * scale[axis] + shift[axis] + 0.5;
                if (!isfinite(p[j][axis]))
                    valid = 0;
            }
            double delta = p[1][axis] - p[0][axis];
            if (!delta) {
                if (p[0][axis] < 0 || p[0][axis] > 1)
                    valid = 0;
            } else {
                double a = -p[0][axis] / delta, b = (1 - p[0][axis]) / delta;
                double lo = a < b ? a : b, hi = a > b ? a : b;
                if (lo > start)
                    start = lo;
                if (hi < end)
                    end = hi;
            }
        }
        /* All geometry is now on the C stack; no per-primitive handle escapes this scope. */
        lab_dom_release(mark);
        if (!valid || start > end)
            continue;
        double pixel[2][2];
        for (unsigned j = 0; j < 2; ++j) {
            double q[2] = {0, 0}, t = j ? end : start;
            for (unsigned row = 0; row < 2; ++row)
                for (unsigned axis = 0; axis < 3; ++axis)
                    q[row] += rotation[row * 3 + axis] * (p[0][axis] + t * (p[1][axis] - p[0][axis]) - 0.5);
            double x = width / 2 + fit * q[0];
            double y = height / 2 + fit * q[1];
            pixel[j][0] = transform[0] * x + transform[2] * y + transform[4];
            pixel[j][1] = transform[1] * x + transform[3] * y + transform[5];
        }
        double dx = pixel[1][0] - pixel[0][0], dy = pixel[1][1] - pixel[0][1];
        double length = dx * dx + dy * dy;
        double t = length ? ((mouse_x - pixel[0][0]) * dx + (mouse_y - pixel[0][1]) * dy) / length : 0;
        if (t < 0)
            t = 0;
        if (t > 1)
            t = 1;
        double x = pixel[0][0] + t * dx, y = pixel[0][1] + t * dy;
        double distance = (x - mouse_x) * (x - mouse_x) + (y - mouse_y) * (y - mouse_y);
        if (!isfinite(distance) || distance > best)
            continue;
        if (!inside_chart(transform, determinant, width, height, x, y))
            continue;
        best = distance;
        double fraction = start + t * (end - start);
        for (unsigned axis = 0; axis < 3; ++axis)
            chosen[axis] = raw[0][axis] + fraction * (raw[1][axis] - raw[0][axis]);
        chosen_x = x;
        chosen_y = y;
        found = 1;
    }
    if (found) {
        int chart = lab_dom_get(view, "chart"), text = lab_dom_string("Approximate sample");
        int valid = 1;
        static const char *const bounds[] = {"xMin", "xMax", "yMin", "yMax", "zMin", "zMax"};
        static const char *const labels[] = {"\nx ≈ ", "\nRe ≈ ", "\nIm ≈ "};
        for (unsigned axis = 0; axis < 3; ++axis) {
            double lo = lab_dom_to_number(lab_dom_read(chart, LAB_DOM_DATASET, bounds[axis * 2]));
            double hi = lab_dom_to_number(lab_dom_read(chart, LAB_DOM_DATASET, bounds[axis * 2 + 1]));
            double value = lo + chosen[axis] * (hi - lo);
            if (!isfinite(value)) {
                valid = 0;
                break;
            }
            text = lab_dom_join(text, lab_dom_join(lab_dom_string(labels[axis]), lab_dom_precision(value, 7), ""), "");
        }
        if (!valid) {
            lab_dom_return(previous);
            return;
        }
        result = lab_dom_object(LAB_VALUE_OBJECT);
        lab_dom_set(result, "x", lab_dom_numeric(chosen_x));
        lab_dom_set(result, "y", lab_dom_numeric(chosen_y));
        lab_dom_set(result, "distance", lab_dom_numeric(best));
        lab_dom_set(result, "text", text);
    }
    lab_dom_return(result);
}
