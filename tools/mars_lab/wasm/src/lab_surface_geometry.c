/**
 * @file lab_surface_geometry.c
 * @brief C projection, six-plane clipping and axis geometry for retained surfaces.
 *
 * Inputs are normalised vertices and a quaternion; a shared pinhole camera divides
 * every projected position by its homogeneous eye-distance denominator. No absolute plot coordinate
 * is converted here. Frame plans drive WebGL uniforms, Canvas and vector export.
 * Surface projected-box fitting preserves coordinate units; space curves use a
 * fixed coordinate box and enclosing-sphere fit for stable rotation and input-only
 * zoom previews. Narrower curve intervals fill the original box rather than shrinking its x axis.
 * Clipping has at most six planes and nine polygon vertices per input triangle.
 * Software and vector output share one native projection pass and stable depth
 * ordering. Temporary browser handles are released after each primitive or merge
 * step, so handle usage does not grow with the supplied geometry.
 */
#include <math.h>

#define MARS_SHARED_PLOT_INTERNAL_ACCESS
#include "internal/plot_internal.h"
#include "lab_dom.h"
#include "lab_plot_pointer.h"
#include "lab_surface_math.h"
#include "lab_surface_volume.h"
#include "lab_surface_axes.h"
#include "lab_surface_geometry.h"

typedef struct {
    double rows[9], scale[3], offset[3], width, height, fit, radius;
    plot_projection_t projection;
} frame_t;

static double numeric(jsref object, const char *key)
{
    return lab_dom_to_number(lab_dom_get(object, key));
}

static void read_vector(jsref value, double *out, unsigned count)
{
    for (unsigned i = 0; i < count; ++i)
        out[i] = lab_dom_to_number(lab_dom_item(value, i));
}

static jsref vector(const double *values, unsigned count)
{
    int out = lab_dom_object(LAB_VALUE_ARRAY);
    for (unsigned i = 0; i < count; ++i)
        lab_dom_push(out, lab_dom_numeric(values[i]));
    return out;
}

static void project(const frame_t *f, const double p[3], double out[3])
{
    double camera[3];
    plot_projection_camera(&f->projection, p, camera);
    plot_projection_point(&f->projection, camera, out);
}

static void read_frame(jsref handle, frame_t *frame)
{
    frame->width = numeric(handle, "width");
    frame->height = numeric(handle, "height");
    frame->fit = numeric(handle, "fit");
    read_vector(lab_dom_get(handle, "rows"), frame->rows, 9);
    read_vector(lab_dom_get(handle, "scaling"), frame->scale, 3);
    read_vector(lab_dom_get(handle, "offset"), frame->offset, 3);
    frame->projection = (plot_projection_t){.fit = frame->fit, .centre = {frame->width / 2, frame->height / 2}};
    for (unsigned i = 0; i < 9; ++i) frame->projection.rows[i] = frame->rows[i];
    double distance = numeric(handle, "distance");
    if (isfinite(distance) && distance > 0) {
        frame->projection.distance = distance;
        frame->projection.radius = numeric(handle, "radius");
        read_vector(lab_dom_get(handle, "centre"), frame->projection.centre, 2);
    }
}

/* A camera ray intersects the actual coordinate box, including perspective corners. */
bool lab_surface_contains(frame_jsref frame, double x, double y)
{
    if (!frame || !isfinite(x) || !isfinite(y))
        return false;
    frame_t f;
    read_frame(frame, &f);
    return plot_projection_contains(&f.projection, x, y);
}

static unsigned clip(double polygon[12][3], unsigned count)
{
    if (count == 2) {
        double start = 0, end = 1, delta[3];
        for (unsigned axis = 0; axis < 3; ++axis) {
            double a = polygon[0][axis];
            delta[axis] = polygon[1][axis] - a;
            if (!delta[axis]) {
                if (a < 0 || a > 1)
                    return 0;
                continue;
            }
            double first = -a / delta[axis], last = (1 - a) / delta[axis];
            double lo = first < last ? first : last, hi = first > last ? first : last;
            if (lo > start)
                start = lo;
            if (hi < end)
                end = hi;
        }
        if (start > end)
            return 0;
        for (unsigned axis = 0; axis < 3; ++axis) {
            polygon[1][axis] = polygon[0][axis] + end * delta[axis];
            polygon[0][axis] += start * delta[axis];
        }
        return 2;
    }
    for (unsigned axis = 0; axis < 3; ++axis)
        for (unsigned bound = 0; bound < 2; ++bound) {
            double output[12][3];
            unsigned used = 0;
            for (unsigned i = 0; i < count; ++i) {
                double *a = polygon[i], *b = polygon[(i + 1) % count];
                int inside_a = bound ? a[axis] <= 1 : a[axis] >= 0;
                int inside_b = bound ? b[axis] <= 1 : b[axis] >= 0;
                if (inside_a) {
                    if (used >= 12)
                        return 0;
                    for (unsigned j = 0; j < 3; ++j)
                        output[used][j] = a[j];
                    ++used;
                }
                if (inside_a != inside_b) {
                    if (used >= 12)
                        return 0;
                    double t = (bound - a[axis]) / (b[axis] - a[axis]);
                    for (unsigned j = 0; j < 3; ++j)
                        output[used][j] = a[j] + t * (b[j] - a[j]);
                    ++used;
                }
            }
            count = used;
            for (unsigned i = 0; i < count; ++i)
                for (unsigned j = 0; j < 3; ++j)
                    polygon[i][j] = output[i][j];
        }
    return count;
}

/* A bounded clipping primitive also supports the browser regression fixture. */
void lab_surface_clip(jsref points)
{
    unsigned count = lab_dom_count(points);
    int result = lab_dom_object(LAB_VALUE_ARRAY);
    if (count <= 3) {
        double polygon[12][3];
        for (unsigned i = 0; i < count; ++i)
            read_vector(lab_dom_item(points, i), polygon[i], 3);
        count = clip(polygon, count);
        for (unsigned i = 0; i < count; ++i)
            lab_dom_push(result, vector(polygon[i], 3));
    }
    lab_dom_return(result);
}

/* Host code transports the projected numbers without recalculating them. */
void lab_surface_project(frame_jsref frame, jsref point)
{
    frame_t f;
    double p[3], out[3];
    read_frame(frame, &f);
    read_vector(point, p, 3);
    project(&f, p, out);
    lab_dom_return(vector(out, 2));
}

static jsref projected_polygon(const frame_t *f, jsref points)
{
    double polygon[12][3], depth = 0;
    unsigned count = lab_dom_count(points);
    if (count > 3)
        count = 0;
    for (unsigned i = 0; i < count; ++i) {
        read_vector(lab_dom_item(points, i), polygon[i], 3);
        for (unsigned j = 0; j < 3; ++j)
            polygon[i][j] = (polygon[i][j] - 0.5) * f->scale[j] + f->offset[j] + 0.5;
    }
    count = clip(polygon, count);
    int out = lab_dom_object(LAB_VALUE_OBJECT), vertices = lab_dom_object(LAB_VALUE_ARRAY);
    for (unsigned i = 0; i < count; ++i) {
        double p[3];
        project(f, polygon[i], p);
        depth += p[2];
        lab_dom_push(vertices, vector(p, 2));
    }
    lab_dom_set(out, "points", vertices);
    lab_dom_set(out, "depth", lab_dom_numeric(count ? depth / count : 0));
    return out;
}

/* Shared geometry for Canvas and on-demand SVG; GPU clipping uses the same transform. */
void lab_surface_polygon(frame_jsref frame, jsref points)
{
    frame_t f;
    read_frame(frame, &f);
    lab_dom_return(projected_polygon(&f, points));
}

static jsref depth_order(jsref source)
{
    unsigned count = lab_dom_count(source);
    int target = lab_dom_object(LAB_VALUE_ARRAY);
    unsigned mark = lab_dom_mark();
    /* Stable bottom-up merge sort: equal-depth primitives retain their input order. */
    for (unsigned width = 1; width < count;) {
        for (unsigned begin = 0; begin < count;) {
            unsigned middle = count - begin < width ? count : begin + width;
            unsigned end = count - middle < width ? count : middle + width;
            unsigned left = begin, right = middle;
            for (unsigned next = begin; next < end; ++next) {
                int take_left = right == end;
                if (left < middle && right < end)
                    take_left = numeric(lab_dom_item(source, left), "depth") >=
                                numeric(lab_dom_item(source, right), "depth");
                unsigned selected = take_left ? left++ : right++;
                lab_dom_key_set(target, lab_dom_format(next, "", ""), lab_dom_item(source, selected));
                lab_dom_release(mark);
            }
            begin = end;
        }
        int swap = source;
        source = target;
        target = swap;
        if (width > count / 2)
            break;
        width *= 2;
    }
    return source;
}

/* Read the camera once, project every primitive and return a shared painter-order plan. */
void lab_surface_polygons(frame_jsref frame, jsref primitives, lab_surface_primitive_t kind)
{
    int result = lab_dom_object(LAB_VALUE_ARRAY);
    if (!lab_dom_truth(lab_dom_get(frame, "drawMesh"))) {
        lab_dom_return(result);
        return;
    }
    frame_t f;
    read_frame(frame, &f);
    unsigned count = lab_dom_count(primitives), minimum = kind != LAB_SURFACE_POLYGONS ? 2 : 3;
    unsigned mark = lab_dom_mark();
    /* Visiting each primitive is necessary for clipping and projection; sorting is O(n log n). */
    for (unsigned i = 0; i < count; ++i) {
        int primitive = lab_dom_item(primitives, i);
        int polygon = projected_polygon(&f, lab_dom_get(primitive, "points"));
        if (lab_dom_count(lab_dom_get(polygon, "points")) >= minimum) {
            lab_dom_set(polygon, "colour", lab_dom_get(primitive, "colour"));
            lab_dom_push(result, polygon);
        }
        lab_dom_release(mark);
    }
    lab_dom_return(depth_order(result));
}

/* Finite segments need not reach the requested endpoints, particularly beside singularities or domain gaps. */
static void curve_input_transform(jsref volume, double *scale, double *offset)
{
    if (!lab_dom_truth(lab_dom_get(volume, "dirty")))
        return;
    int bounds = lab_dom_get(volume, "bounds"), sampled = lab_dom_get(volume, "sampled");
    double low = lab_dom_to_number(lab_dom_item(bounds, 0)), high = lab_dom_to_number(lab_dom_item(bounds, 1));
    double first = lab_dom_to_number(lab_dom_item(sampled, 0)), last = lab_dom_to_number(lab_dom_item(sampled, 1));
    double span = high - low, sampled_span = last - first;
    double magnitude = __builtin_fabs(low) + __builtin_fabs(high) + __builtin_fabs(first) + __builtin_fabs(last);
    /* At microscopic scales retain the exact exponent transform, not rounded absolute coordinates. */
    if (!isfinite(span) || !isfinite(sampled_span) ||
        !(span > magnitude * 1e-12) || !(sampled_span > magnitude * 1e-12))
        return;
    *scale = sampled_span / span;
    *offset = ((first - low) / 2 + (last - high) / 2) / span;
}

/* Ratios of zoom exponents are safe even when absolute endpoints coincide as doubles. */
void lab_surface_frame(view_jsref view, jsref override_bounds)
{
    frame_t f = {0};
    int volume = lab_dom_get(view, "volume"), out = lab_dom_object(LAB_VALUE_OBJECT);
    int segments = lab_dom_equal(lab_dom_read(lab_dom_get(view, "chart"), LAB_DOM_DATASET, "segments"), lab_dom_string("true"));
    f.width = numeric(view, "width");
    f.height = numeric(view, "height");
    double units[3];
    read_vector(lab_dom_get(segments ? volume : view, "units"), units, 3);
    lab_surface_rotation(lab_dom_get(view, "orientation"), f.rows);
    double relative = numeric(volume, "exponent") - numeric(volume, "sampleExponent");
    int auto_height = lab_dom_truth(lab_dom_get(volume, "autoHeight"));
    double limited = relative > 60 ? 60 : relative < -60 ? -60 : relative;
    for (unsigned i = 0; i < 3; ++i)
        f.scale[i] = (segments ? i != 0 : auto_height && i == 2) ? 1 : lab_surface_exp2(limited);
    if (segments)
        curve_input_transform(volume, &f.scale[0], &f.offset[0]);
    if (!segments && auto_height) {
        /* Auto-height surfaces retain physical units; curves instead fill their stable coordinate box. */
        double factor = 1 / f.scale[0];
        units[0] *= factor;
        units[1] *= factor;
        double longest = units[0] > units[1] ? units[0] : units[1];
        if (units[2] > longest)
            longest = units[2];
        for (unsigned i = 0; i < 3; ++i)
            units[i] /= longest;
    }
    lab_dom_set(out, "drawMesh", lab_dom_scalar(LAB_VALUE_BOOLEAN, override_bounds || (relative >= -60 && relative <= 60)));
    for (unsigned i = 0; i < 3; ++i) {
        f.radius += units[i] * units[i];
        for (unsigned row = 0; row < 3; ++row)
            f.rows[row * 3 + i] *= units[i];
    }
    /* Explicit normalised-frame overrides are diagnostic only, never sampling bounds. */
    if (override_bounds) {
        double target[6], sampled[6];
        read_vector(override_bounds, target, 6);
        read_vector(lab_dom_get(volume, "sampled"), sampled, 6);
        for (unsigned i = 0; i < 3; ++i) {
            double span = target[i * 2 + 1] - target[i * 2];
            if (!(span > 0)) {
                lab_dom_return(lab_dom_scalar(LAB_VALUE_NULL, 0));
                return;
            }
            f.scale[i] = (sampled[i * 2 + 1] - sampled[i * 2]) / span;
            f.offset[i] = ((sampled[i * 2] - target[i * 2]) + (sampled[i * 2 + 1] - target[i * 2 + 1])) / (2 * span);
        }
    }
    f.radius = __builtin_sqrt(f.radius) / 2;
    if (!plot_projection_init(f.rows, f.width, f.height, numeric(volume, "labelWidth"), segments, &f.projection)) {
        lab_dom_return(lab_dom_scalar(LAB_VALUE_NULL, 0));
        return;
    }
    f.fit = f.projection.fit;
    double matrix[16] = {0};
    double shift_x = 2 * f.projection.centre[0] / f.width - 1;
    double shift_y = 1 - 2 * f.projection.centre[1] / f.height;
    for (unsigned i = 0; i < 3; ++i) {
        matrix[i * 4] = 2 * f.fit * f.rows[i] / f.width + shift_x * f.rows[6 + i] / f.projection.distance;
        matrix[i * 4 + 1] = -2 * f.fit * f.rows[3 + i] / f.height + shift_y * f.rows[6 + i] / f.projection.distance;
        matrix[i * 4 + 2] = f.rows[6 + i] / (2 * f.radius);
        matrix[i * 4 + 3] = f.rows[6 + i] / f.projection.distance;
    }
    matrix[12] = shift_x; matrix[13] = shift_y;
    matrix[15] = 1;
    lab_dom_set(out, "matrix", vector(matrix, 16));
    lab_dom_set(out, "rows", vector(f.rows, 9));
    lab_dom_set(out, "scaling", vector(f.scale, 3));
    lab_dom_set(out, "offset", vector(f.offset, 3));
    lab_dom_set(out, "width", lab_dom_numeric(f.width));
    lab_dom_set(out, "height", lab_dom_numeric(f.height));
    lab_dom_set(out, "fit", lab_dom_numeric(f.fit));
    lab_dom_set(out, "distance", lab_dom_numeric(f.projection.distance));
    lab_dom_set(out, "radius", lab_dom_numeric(f.projection.radius));
    lab_dom_set(out, "centre", vector(f.projection.centre, 2));
    lab_surface_axes(view, out);
    lab_dom_return(out);
}
