/**
 * @file plot_surface_svg.c
 * @brief Rotatable SVG height surfaces with depth-ordered coloured mesh facets.
 *
 * Numerical ranges are normalised before conversion to drawing coordinates.
 * Missing vertices leave holes. Facets with a very large relative height jump
 * are omitted as a conservative discontinuity heuristic, not a continuity proof.
 * The camera determines projection, facet order and the exposed axes. Each render
 * is an independent bounded SVG, with no GPU or files.
 */
#include <errno.h>
#include <math.h>
#include <stdlib.h>

#define MARS_SHARED_COMPUTATION_INTERNAL_ACCESS
#include "internal/computation_internal.h"
#define MARS_PLOT_INTERNAL_ACCESS
#include "plot_internal.h"

typedef struct {
    double u, v, z, x, y, depth;
    bool valid;
} vertex_t;

typedef struct {
    size_t a, b, c;
    double depth;
} facet_t;

typedef struct {
    double ca, sa, ce, se, cr, sr, scale_x, scale_y, cx, cy;
} camera_t;

static camera_t camera(const plot_t *plot, double label_width)
{
    double radians = acos(-1) / 180;
    camera_t view = {.ca = cos(plot->azimuth * radians), .sa = sin(plot->azimuth * radians),
                     .ce = cos(plot->elevation * radians), .se = sin(plot->elevation * radians),
                     .cr = cos(plot->roll * radians), .sr = sin(plot->roll * radians),
                     .cx = plot->options.width * 0.5, .cy = plot->options.height * 0.52};
    double extent = fabs(view.ca) + fabs(view.sa);
    double margin = fmax(80, label_width + 20);
    margin = fmin(margin, (plot->options.width - 80) / 2.0);
    view.scale_x = (plot->options.width - 2 * margin) / extent;
    view.scale_y = (plot->options.height - 160) / (view.se * extent + view.ce);
    /* Rotate the original screen geometry, then shrink uniformly to keep it in view. */
    double horizontal[] = {view.scale_x * view.ca, -view.scale_x * view.sa, 0};
    double vertical[] = {-view.scale_y * view.se * view.sa, -view.scale_y * view.se * view.ca,
                          -view.scale_y * view.ce};
    double width = 0, height = 0;
    for (unsigned i = 0; i < 3; ++i) {
        width += fabs(view.cr * horizontal[i] - view.sr * vertical[i]);
        height += fabs(view.sr * horizontal[i] + view.cr * vertical[i]);
    }
    double fit = fmin(1, fmin((plot->options.width - 2 * margin) / width,
                              (plot->options.height - 160) / height));
    view.scale_x *= fit;
    view.scale_y *= fit;
    return view;
}

static bool finite_real(number_t value)
{
    return num_is_real(value) && num_is_finite(value);
}

static void project(const camera_t *view, vertex_t *vertex)
{
    double u = vertex->u - 0.5, v = vertex->v - 0.5, z = vertex->z - 0.5;
    double away = view->sa * u + view->ca * v;
    double x = view->scale_x * (view->ca * u - view->sa * v);
    double y = -view->scale_y * (view->se * away + view->ce * z);
    vertex->x = view->cx + view->cr * x - view->sr * y;
    vertex->y = view->cy + view->sr * x + view->cr * y;
    vertex->depth = view->ce * away - view->se * z;
}

static double fraction(number_t value, number_t lower, number_t span)
{
    NUM_SCOPE(numbers);
    return num_to_double(num_div(num_sub(value, lower), span));
}

static int far_first(const void *a, const void *b)
{
    const facet_t *first = a, *second = b;
    if (first->depth != second->depth)
        return first->depth > second->depth ? -1 : 1;
    return first->a < second->a ? -1 : first->a > second->a;
}

static unsigned colour(double height)
{
    static const unsigned palette[][3] = {{63, 107, 163}, {71, 181, 177}, {231, 197, 105}};
    double position = fmax(0, fmin(1, height)) * 2;
    unsigned band = position < 1 ? 0 : 1;
    double mix = position - band;
    unsigned rgb = 0;
    for (unsigned i = 0; i < 3; ++i)
        rgb = (rgb << 8) | (unsigned)lround(palette[band][i] * (1 - mix) + palette[band + 1][i] * mix);
    return rgb;
}

static bool stroke(string_t *svg, vertex_t a, vertex_t b)
{
    return string_append_format(svg, "<path d=\"M %.3f %.3f L %.3f %.3f\"/>\n", a.x, a.y, b.x, b.y) >= 0;
}

static vertex_t axis_vertex(const camera_t *view, unsigned axis, double t)
{
    vertex_t at = {.u = view->sa < 0, .v = view->ca < 0};
    if (axis == 0)
        at.u = t;
    else if (axis == 1)
        at.v = t;
    else {
        at.u = view->ca < 0;
        at.v = view->sa >= 0;
        at.z = t;
    }
    project(view, &at);
    return at;
}

static bool axes(string_t *svg, const plot_t *plot, const camera_t *view, number_t bounds[6])
{
    plot_tick_layout_t labels = {.width = plot->options.width, .height = plot->options.height, .font_size = 11};
    bool ok = string_append_format(svg, "<g data-plot-axes=\"true\" fill=\"none\" stroke=\"#%06x\" "
                                    "stroke-width=\"1.2\">\n", (unsigned)plot->options.axis_colour) >= 0;
    for (unsigned axis = 0; ok && axis < 3; ++axis) {
        vertex_t a = axis_vertex(view, axis, 0), b = axis_vertex(view, axis, 1);
        double length = hypot(b.x - a.x, b.y - a.y);
        double dx = (b.x - a.x) / length, dy = (b.y - a.y) / length;
        vertex_t tip = b;
        tip.x += dx * 12;
        tip.y += dy * 12;
        ok = stroke(svg, a, tip) && string_append_format(svg,
             "<path data-axis-arrow=\"%u\" d=\"M %.3f %.3f L %.3f %.3f L %.3f %.3f\"/>\n", axis,
             tip.x - dx * 7 - dy * 3.5, tip.y - dy * 7 + dx * 3.5, tip.x, tip.y,
             tip.x - dx * 7 + dy * 3.5, tip.y - dy * 7 - dx * 3.5) >= 0;
    }
    ok = ok && string_append_format(svg, "</g><g fill=\"#%06x\" font-family=\"sans-serif\" font-size=\"11\">\n",
                                     (unsigned)plot->options.label_colour) >= 0;
    const string_t *names[] = {plot->x_label, plot->y_label, plot->z_label};
    /* Endpoint names take priority; nearby numeric ticks may move or be thinned. */
    for (unsigned axis = 0; ok && axis < 3; ++axis) {
        vertex_t a = axis_vertex(view, axis, 0), b = axis_vertex(view, axis, 1);
        double length = hypot(b.x - a.x, b.y - a.y);
        double dx = (b.x - a.x) / length, dy = (b.y - a.y) / length;
        int anchor = dx < -0.1 ? -1 : dx > 0.1;
        ok = string_append_format(svg, "<g data-axis-name=\"%u\">", axis) >= 0 &&
             plot_tick_text(svg, &labels, names[axis], axis + 3, b.x + dx * 25, b.y + dy * 25 + 4,
                            anchor, dx, dy) && string_append_cstr(svg, "</g>") == 0;
    }
    for (unsigned axis = 0; ok && axis < 3; ++axis) {
        vertex_t a = axis_vertex(view, axis, 0), b = axis_vertex(view, axis, 1);
        vertex_t centre = {.u = 0.5, .v = 0.5};
        project(view, &centre);
        double nx = b.y - a.y, ny = a.x - b.x, length = hypot(nx, ny);
        nx /= length;
        ny /= length;
        if (nx * ((a.x + b.x) / 2 - centre.x) + ny * ((a.y + b.y) / 2 - centre.y) < 0) {
            nx = -nx;
            ny = -ny;
        }
        if (axis == 2) {
            nx = -view->cr;
            ny = -view->sr;
        }
        number_t ticks[12], span = num_sub(bounds[axis * 2 + 1], bounds[axis * 2]);
        size_t count = plot_ticks(bounds[axis * 2], bounds[axis * 2 + 1], ticks);
        for (size_t i = 0; i < count; ++i) {
            double t = fraction(ticks[i], bounds[axis * 2], span);
            vertex_t at = axis_vertex(view, axis, t), tip = at;
            tip.x += nx * 6;
            tip.y += ny * 6;
            int side = nx < -1e-9 ? -1 : nx > 1e-9;
            string_t *label = plot_tick_label(ticks[i]);
            ok = ok && label && string_append_format(svg, "<g data-plot-tick=\"%u\" stroke=\"#%06x\" "
                  "stroke-width=\"1.2\">", axis, (unsigned)plot->options.axis_colour) >= 0 && stroke(svg, at, tip) &&
                 string_append_cstr(svg, "</g>") == 0 &&
                 plot_tick_text(svg, &labels, label, axis, at.x + nx * 13 + side * 3,
                                at.y + ny * 13 + 4, side, nx, ny);
            string_free(label);
            num_destroy(&ticks[i]);
        }
        num_destroy(&span);
    }
    return ok && string_append_cstr(svg, "</g>\n") == 0;
}

/* Render a normalised height field into an independently owned SVG string. */
string_t *plot_surface_svg(const plot_t *plot)
{
    NUM_SCOPE(numbers);
    size_t columns = plot->surface_columns, rows = plot->surface_rows, count = columns * rows;
    number_t lo = NUM_NAN, hi = NUM_NAN;
    for (size_t i = 0; i < count; ++i) {
        if (!computation_step_current("plot.surface.range"))
            return NULL;
        number_t value = plot->surface_z[i];
        if (finite_real(value)) {
            if (num_is_nan(lo) || num_lt(value, lo))
                lo = num_clone(value);
            if (num_is_nan(hi) || num_gt(value, hi))
                hi = num_clone(value);
        }
    }
    if (num_is_nan(lo)) {
        errno = ENODATA;
        return NULL;
    }
    if (num_eq(lo, hi)) {
        number_t padding = num_div(num_abs(lo), num_create_from_long(20));
        if (num_lt(padding, NUM_ONE))
            padding = num_clone(NUM_ONE);
        lo = num_sub(lo, padding);
        hi = num_add(hi, padding);
    }
    number_t bounds[] = {plot->surface_x[0], plot->surface_x[columns - 1],
                         plot->surface_y[0], plot->surface_y[rows - 1], lo, hi};
    double label_width = 0;
    for (unsigned axis = 0; axis < 3; ++axis) {
        double width = plot_tick_width(bounds[axis * 2], bounds[axis * 2 + 1], 11);
        if (width < 0)
            return NULL;
        label_width = fmax(label_width, width);
    }
    camera_t view = camera(plot, label_width);
    number_t spans[] = {num_sub(bounds[1], bounds[0]), num_sub(bounds[3], bounds[2]), num_sub(hi, lo)};
    if (count > SIZE_MAX / (2 * sizeof(facet_t))) {
        errno = E2BIG;
        return NULL;
    }
    vertex_t *vertices = calloc(count, sizeof(*vertices));
    facet_t *facets = calloc(count * 2, sizeof(*facets));
    string_t *svg = string_new();
    bool ok = vertices && facets && svg;
    for (size_t r = 0; ok && r < rows; ++r) {
        for (size_t c = 0; ok && c < columns; ++c) {
            ok = computation_step_current("plot.surface.project");
            vertex_t *at = &vertices[r * columns + c];
            number_t value = plot->surface_z[r * columns + c];
            if (!finite_real(value))
                continue;
            at->u = fraction(plot->surface_x[c], bounds[0], spans[0]);
            at->v = fraction(plot->surface_y[r], bounds[2], spans[1]);
            at->z = fraction(value, lo, spans[2]);
            at->valid = isfinite(at->u) && isfinite(at->v) && isfinite(at->z);
            project(&view, at);
        }
    }
    size_t faces = 0;
    for (size_t r = 0; ok && r + 1 < rows; ++r) {
        for (size_t c = 0; ok && c + 1 < columns; ++c) {
            ok = computation_step_current("plot.surface.mesh");
            size_t indices[] = {r * columns + c, r * columns + c + 1,
                                (r + 1) * columns + c + 1, (r + 1) * columns + c};
            bool valid = true;
            double low = 1, high = 0;
            for (unsigned j = 0; j < 4; ++j) {
                const vertex_t *v = &vertices[indices[j]];
                valid = valid && v->valid;
                low = fmin(low, v->z);
                high = fmax(high, v->z);
            }
            if (!valid || (columns >= 17 && rows >= 17 && high - low > 0.6))
                continue;
            for (unsigned j = 1; j < 3; ++j) {
                size_t a = indices[0], b = indices[j], d = indices[j + 1];
                double depth = vertices[a].depth + vertices[b].depth + vertices[d].depth;
                facets[faces++] = (facet_t){a, b, d, depth};
            }
        }
    }
    if (ok && !faces) {
        errno = ENODATA;
        ok = false;
    }
    if (ok) {
        qsort(facets, faces, sizeof(*facets), far_first);
        ok = string_append_format(svg, "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%u\" height=\"%u\" "
                 "viewBox=\"0 0 %u %u\" role=\"img\" data-surface=\"true\" "
                 "data-azimuth=\"%.3f\" data-elevation=\"%.3f\" data-roll=\"%.3f\"><title>",
                 plot->options.width, plot->options.height, plot->options.width, plot->options.height,
                 plot->azimuth, plot->elevation, plot->roll) >= 0 &&
             plot_escape(svg, plot->title) && string_append_format(svg, "</title><text x=\"%u\" y=\"24\" "
                 "text-anchor=\"middle\" fill=\"#%06x\" font-family=\"sans-serif\" font-size=\"14\">",
                 plot->options.width / 2, (unsigned)plot->options.label_colour) >= 0 && plot_escape(svg, plot->title) &&
             string_append_cstr(svg, "</text><g data-series=\"0\" stroke=\"#193d46\" "
                 "stroke-opacity=\"0.35\" stroke-width=\"0.45\" stroke-linejoin=\"round\">\n") == 0;
    }
    for (size_t i = 0; ok && i < faces; ++i) {
        const facet_t *f = &facets[i];
        const vertex_t *a = &vertices[f->a], *b = &vertices[f->b], *c = &vertices[f->c];
        ok = computation_step_current("plot.surface.draw") && string_append_format(svg,
            "<path fill=\"#%06x\" d=\"M %.3f %.3f L %.3f %.3f L %.3f %.3f Z\"/>\n",
            colour((a->z + b->z + c->z) / 3), a->x, a->y, b->x, b->y, c->x, c->y) >= 0;
    }
    ok = ok && string_append_cstr(svg, "</g>\n") == 0 && axes(svg, plot, &view, bounds) &&
         string_append_cstr(svg, "</svg>\n") == 0;
    free(facets);
    free(vertices);
    if (!ok) {
        string_free(svg);
        svg = NULL;
    }
    return svg;
}
