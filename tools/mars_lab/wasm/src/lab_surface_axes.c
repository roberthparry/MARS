/**
 * @file lab_surface_axes.c
 * @brief Connected exterior axes and bounded label placement for retained 3D plots.
 *
 * Native SVG and this retained renderer share the same exterior-frame selector.
 * Its adjoining edges prefer the projected coordinate-box silhouette. Shaded
 * cylinders and cones use the same pinhole projection as the sampled surface;
 * tick positions, widths and upright labels scale with eye distance. Current
 * projected spacing selects strokes without pruning the retained tick catalogue.
 * Connected reference edges persist across camera frames until their exterior
 * placement or projected clearance becomes unsuitable.
 * Upright labels share a
 * conservative, at-most-forty-rectangle layout;
 * its bounded collision scan does not depend on mesh size or evaluate mathematics.
 */
#include <math.h>
#include <stdlib.h>

#define MARS_SHARED_PLOT_INTERNAL_ACCESS
#include "internal/plot_internal.h"
#include "lab_dom.h"
#include "lab_surface_volume.h"
#include "lab_surface_axes.h"

typedef struct {
    double rows[9], width, height, fit;
    plot_projection_t projection;
    plot_axis_frame_t frame;
    double boxes[40][4];
    unsigned count;
} axes_t;

static double numeric(jsref object, const char *key)
{
    return lab_dom_to_number(lab_dom_get(object, key));
}

static void project(const axes_t *axes, const double point[3], double out[2])
{
    double camera[3], screen[3];
    plot_projection_camera(&axes->projection, point, camera);
    plot_projection_point(&axes->projection, camera, screen);
    out[0] = screen[0]; out[1] = screen[1];
}

static void axis_point(const axes_t *axes, unsigned axis, double fraction, double out[2])
{
    double point[3];
    unsigned corner = axes->frame.corner[axis];
    for (unsigned i = 0; i < 3; ++i)
        point[i] = !!(corner & (1u << i));
    point[axis] = fraction;
    project(axes, point, out);
}

static void line(jsref paths, jsref widths, const double *values, unsigned count, double width)
{
    int path = lab_dom_object(LAB_VALUE_ARRAY);
    for (unsigned i = 0; i < count; ++i)
        lab_dom_push(path, lab_dom_numeric(values[i]));
    lab_dom_push(paths, path);
    lab_dom_push(widths, lab_dom_numeric(width));
}

static void text(axes_t *axes, jsref texts, jsref label, double x, double y, int anchor,
                 double dx, double dy, bool name, double scale)
{
    double font_size = (name ? 14 : 13) * scale;
    double width = lab_dom_length(label) * font_size * 0.8;
    if (!width || axes->count == 40)
        return;
    unsigned attempts = name ? 12 : dx || dy ? 4 : 1;
    for (unsigned attempt = 0; attempt < attempts; ++attempt) {
        double offset = (attempt % 4) * font_size * 1.5;
        double side = attempt < 4 ? 0 : attempt < 8 ? font_size * 2 : -font_size * 2;
        double at_x = x + dx * offset - dy * side, at_y = y + dy * offset + dx * side;
        if (name) {
            double before = anchor < 0 ? width : anchor == 0 ? width / 2 : 0;
            at_x = fmax(2 + before, fmin(axes->width - 2 - width + before, at_x));
            at_y = fmax(font_size + 2, fmin(axes->height - font_size * 0.3 - 2, at_y));
        }
        double left = at_x - (anchor < 0 ? width : anchor == 0 ? width / 2 : 0);
        double box[] = {left, at_y - font_size, left + width, at_y + font_size * 0.3};
        bool fits = box[0] >= 2 && box[1] >= 2 && box[2] <= axes->width - 2 && box[3] <= axes->height - 2;
        for (unsigned i = 0; fits && i < axes->count; ++i) {
            const double *other = axes->boxes[i];
            fits = box[2] + 4 <= other[0] || other[2] + 4 <= box[0] ||
                   box[3] + 4 <= other[1] || other[3] + 4 <= box[1];
        }
        if (!fits)
            continue;
        for (unsigned i = 0; i < 4; ++i)
            axes->boxes[axes->count][i] = box[i];
        ++axes->count;
        int item = lab_dom_object(LAB_VALUE_OBJECT);
        lab_dom_set(item, "text", label);
        lab_dom_set(item, "x", lab_dom_numeric(at_x));
        lab_dom_set(item, "y", lab_dom_numeric(at_y));
        lab_dom_set(item, "align", lab_dom_string(anchor < 0 ? "right" : anchor > 0 ? "left" : "center"));
        lab_dom_set(item, "size", lab_dom_numeric(font_size));
        lab_dom_set(item, "bold", lab_dom_scalar(LAB_VALUE_BOOLEAN, name));
        lab_dom_push(texts, item);
        return;
    }
}

/* Build drawing plans only; exact coordinate arithmetic belongs to the native plot module. */
void lab_surface_axes(view_jsref view, frame_jsref frame)
{
    axes_t axes = {.width = numeric(frame, "width"), .height = numeric(frame, "height"), .fit = numeric(frame, "fit")};
    int paths = lab_dom_object(LAB_VALUE_ARRAY), texts = lab_dom_object(LAB_VALUE_ARRAY);
    int widths = lab_dom_object(LAB_VALUE_ARRAY);
    int rows = lab_dom_get(frame, "rows");
    int volume = lab_dom_get(view, "volume"), ticks = lab_dom_get(view, "ticks");
    for (unsigned i = 0; i < 9; ++i)
        axes.rows[i] = lab_dom_to_number(lab_dom_item(rows, i));
    axes.projection = (plot_projection_t){.fit = axes.fit, .distance = numeric(frame, "distance"),
        .radius = numeric(frame, "radius")};
    int centre = lab_dom_get(frame, "centre");
    for (unsigned i = 0; i < 2; ++i)
        axes.projection.centre[i] = lab_dom_to_number(lab_dom_item(centre, i));
    for (unsigned i = 0; i < 9; ++i) axes.projection.rows[i] = axes.rows[i];
    double corners[8][2];
    for (unsigned i = 0; i < 8; ++i) {
        double point[] = {!!(i & 1), !!(i & 2), !!(i & 4)};
        project(&axes, point, corners[i]);
    }
    plot_axis_frame_t previous = {0};
    int saved = lab_dom_get(view, "axisCorners");
    if (lab_dom_count(saved) != 3)
        saved = lab_dom_split(lab_dom_read(lab_dom_get(view, "chart"), LAB_DOM_DATASET, "axisCorners"), " ");
    bool valid = lab_dom_count(saved) == 3;
    for (unsigned axis = 0; valid && axis < 3; ++axis) {
        double value = lab_dom_to_number(lab_dom_item(saved, axis));
        valid = isfinite(value) && value >= 0 && value < 8 && value == floor(value);
        if (valid) previous.corner[axis] = (unsigned)value;
    }
    if (!plot_axis_frame(corners, valid ? &previous : NULL, &axes.frame)) {
        lab_dom_set(frame, "paths", paths);
        lab_dom_set(frame, "texts", texts);
        return;
    }
    int selected = lab_dom_object(LAB_VALUE_ARRAY);
    for (unsigned i = 0; i < 3; ++i)
        lab_dom_push(selected, lab_dom_numeric(axes.frame.corner[i]));
    lab_dom_set(frame, "axisCorners", selected);
    lab_dom_set(view, "axisCorners", selected);
    plot_axis_face_t faces[PLOT_AXIS_FACES * 3];
    unsigned face_count = 0;
    for (unsigned axis = 0; axis < 3; ++axis) {
        double start[2], end[2];
        axis_point(&axes, axis, 0, start);
        axis_point(&axes, axis, 1, end);
        unsigned corner = axes.frame.corner[axis];
        double lower[] = {!!(corner & 1), !!(corner & 2), !!(corner & 4)}, upper[3];
        for (unsigned i = 0; i < 3; ++i) upper[i] = lower[i];
        upper[axis] = 1;
        plot_axis_solid_t solid;
        if (!plot_axis_solid(&axes.projection, lower, upper, axis, &solid))
            continue;
        for (unsigned i = 0; i < solid.count; ++i) faces[face_count++] = solid.face[i];
        plot_axis_arrow_t arrow = solid.arrow;
        double scale = solid.name_scale;
        double dx = arrow.direction[0], dy = arrow.direction[1];
        double shaft[] = {start[0], start[1], arrow.tip[0], arrow.tip[1]};
        double head[] = {arrow.wing[0][0], arrow.wing[0][1], arrow.tip[0], arrow.tip[1],
                         arrow.wing[1][0], arrow.wing[1][1]};
        line(paths, widths, shaft, 4, 1.4);
        line(paths, widths, head, 6, 1.4);
        text(&axes, texts, lab_dom_item(lab_dom_get(view, "names"), axis),
             arrow.name[0] + axes.frame.normal[axis][0] * 10 * scale,
             arrow.name[1] + axes.frame.normal[axis][1] * 10 * scale + 14 * scale * 0.3,
             dx < -0.1 ? -1 : dx > 0.1, dx, dy, true, scale);
    }
    lab_dom_set(frame, "axisPathCount", lab_dom_numeric(lab_dom_count(paths)));
    qsort(faces, face_count, sizeof *faces, plot_axis_face_order);
    int solids = lab_dom_object(LAB_VALUE_ARRAY);
    for (unsigned i = 0; i < face_count; ++i) {
        unsigned mark = lab_dom_mark();
        const plot_axis_face_t *face = &faces[i];
        int item = lab_dom_object(LAB_VALUE_OBJECT), points = lab_dom_object(LAB_VALUE_ARRAY);
        for (unsigned j = 0; j < face->count; ++j) {
            int point = lab_dom_object(LAB_VALUE_ARRAY);
            lab_dom_push(point, lab_dom_numeric(face->point[j][0]));
            lab_dom_push(point, lab_dom_numeric(face->point[j][1]));
            lab_dom_push(points, point);
        }
        lab_dom_set(item, "points", points);
        lab_dom_set(item, "shade", lab_dom_numeric(face->shade));
        lab_dom_set(item, "axis", lab_dom_numeric(face->axis));
        lab_dom_set(item, "cone", lab_dom_scalar(LAB_VALUE_BOOLEAN, face->cone));
        lab_dom_push(solids, item);
        lab_dom_release(mark);
    }
    lab_dom_set(frame, "solids", solids);
    for (unsigned axis = 0; axis < 3; ++axis) {
        double start[2], end[2];
        axis_point(&axes, axis, 0, start);
        axis_point(&axes, axis, 1, end);
        double length = hypot(end[0] - start[0], end[1] - start[1]);
        if (length < 1e-7)
            continue;
        double nx = axes.frame.normal[axis][0], ny = axes.frame.normal[axis][1];
        double positions[PLOT_AXIS_MAX_TICKS][2], scales[PLOT_AXIS_MAX_TICKS];
        jsref entries[PLOT_AXIS_MAX_TICKS];
        unsigned count = 0, zero = PLOT_AXIS_MAX_TICKS;
        /* The native tick catalogue has at most twelve entries per axis. */
        for (unsigned i = 0; i < lab_dom_count(ticks) && count < PLOT_AXIS_MAX_TICKS; ++i) {
            int tick = lab_dom_item(ticks, i);
            if ((unsigned)numeric(tick, "axis") != axis)
                continue;
            double fraction = numeric(tick, "fraction");
            axis_point(&axes, axis, fraction, positions[count]);
            unsigned corner = axes.frame.corner[axis];
            double point[] = {!!(corner & 1), !!(corner & 2), !!(corner & 4)}, camera[3];
            point[axis] = fraction;
            plot_projection_camera(&axes.projection, point, camera);
            scales[count] = plot_projection_size(&axes.projection, camera[2]);
            entries[count] = tick;
            if (lab_dom_truth(lab_dom_get(tick, "zero"))) zero = count;
            ++count;
        }
        unsigned visible = plot_axis_tick_mask(positions, scales, count, zero);
        for (unsigned i = 0; i < count; ++i) {
            if (!(visible & (1u << i))) continue;
            const double *at = positions[i];
            double scale = scales[i];
            double mark[] = {at[0], at[1], at[0] + nx * 6 * scale, at[1] + ny * 6 * scale};
            line(paths, widths, mark, 4, 1.2 * scale);
            double endpoint[] = {mark[2], mark[3]}, normal[] = {nx, ny}, position[2];
            int side = plot_axis_tick_label_position(endpoint, normal, 13 * scale, position);
            text(&axes, texts, lab_surface_tick_label(volume, entries[i]), position[0], position[1], side,
                 length < 96 ? nx : 0, length < 96 ? ny : 0, false, scale);
        }
    }
    if (lab_dom_truth(lab_dom_get(volume, "dirty")))
        text(&axes, texts, lab_dom_string("Preview — refining the selected volume"),
             axes.width / 2, axes.height - 12, 0, 0, 0, false, 1);
    lab_dom_set(frame, "paths", paths);
    lab_dom_set(frame, "texts", texts);
    lab_dom_set(frame, "pathWidths", widths);
}
