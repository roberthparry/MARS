/**
 * @file plot_surface_svg.c
 * @brief Rotatable SVG grids, meshes and space curves with a shared equal-unit camera.
 *
 * Numerical ranges are normalised before conversion to drawing coordinates.
 * Missing vertices leave holes. Finite heights are joined regardless of slope;
 * callers with an evaluator must sample domain gaps before supplying the mesh.
 * The equal-unit camera determines projection, facet order and the exposed axes
 * for both height grids and meshes. Space curves use a fixed enclosing-sphere
 * fit centred on their normalised box, matching the retained interactive camera.
 * Each render
 * is an independent bounded SVG, with no GPU or files.
 */
#include <errno.h>
#include <math.h>
#include <stdlib.h>

#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"
#define MARS_SHARED_COMPUTATION_INTERNAL_ACCESS
#include "internal/computation_internal.h"
#define MARS_PLOT_INTERNAL_ACCESS
#include "plot_internal.h"

typedef struct {
    double u, v, z, x, y, depth;
    number_t height; /* Borrowed sample or viewport boundary, never a temporary result. */
    bool valid;
} vertex_t;

typedef struct {
    size_t a, b, c;
    double depth;
} facet_t;

typedef struct {
    double ca, sa, ce, se, cr, sr, scale_x, scale_y, cx, cy;
    double units[3];
} camera_t;

static camera_t camera(const plot_t *plot, double label_width, const number_t bounds[6])
{
    double radians = acos(-1) / 180;
    camera_t view = {.ca = cos(plot->azimuth * radians), .sa = sin(plot->azimuth * radians),
                     .ce = cos(plot->elevation * radians), .se = sin(plot->elevation * radians),
                     .cr = cos(plot->roll * radians), .sr = sin(plot->roll * radians),
                     .cx = plot->options.width * 0.5, .cy = plot->options.height * 0.52, .units = {1, 1, 1}};
    double margin = fmax(80, label_width + 20);
    margin = fmin(margin, (plot->options.width - 80) / 2.0);
    /* Height grids and implicit meshes share one physical scale on every axis. */
    number_t spans[] = {num_sub(bounds[1], bounds[0]), num_sub(bounds[3], bounds[2]),
                        num_sub(bounds[5], bounds[4])};
    number_t longest = spans[0];
    for (unsigned i = 1; i < 3; ++i)
        if (num_gt(spans[i], longest))
            longest = spans[i];
    for (unsigned i = 0; i < 3; ++i)
        view.units[i] = num_to_double(num_div(spans[i], longest));
    double horizontal[] = {view.ca * view.units[0], -view.sa * view.units[1], 0};
    double vertical[] = {-view.se * view.sa * view.units[0], -view.se * view.ca * view.units[1],
                          -view.ce * view.units[2]};
    double width = 0, height = 0;
    for (unsigned i = 0; i < 3; ++i) {
        width += fabs(view.cr * horizontal[i] - view.sr * vertical[i]);
        height += fabs(view.sr * horizontal[i] + view.cr * vertical[i]);
    }
    view.scale_x = view.scale_y = fmin((plot->options.width - 2 * margin) / width,
                                      (plot->options.height - 160) / height);
    return view;
}

/* Invert a projected triangle to all three normalised viewport coordinates for pointer zoom. */
static bool pointer_map(string_t *svg, const vertex_t p[3])
{
    double x1 = p[1].x - p[0].x, y1 = p[1].y - p[0].y;
    double x2 = p[2].x - p[0].x, y2 = p[2].y - p[0].y;
    double determinant = x1 * y2 - x2 * y1;
    if (fabs(determinant) < 1e-12)
        return true;
    double ux = ((p[1].u - p[0].u) * y2 - (p[2].u - p[0].u) * y1) / determinant;
    double uy = (x1 * (p[2].u - p[0].u) - x2 * (p[1].u - p[0].u)) / determinant;
    double vx = ((p[1].v - p[0].v) * y2 - (p[2].v - p[0].v) * y1) / determinant;
    double vy = (x1 * (p[2].v - p[0].v) - x2 * (p[1].v - p[0].v)) / determinant;
    double zx = ((p[1].z - p[0].z) * y2 - (p[2].z - p[0].z) * y1) / determinant;
    double zy = (x1 * (p[2].z - p[0].z) - x2 * (p[1].z - p[0].z)) / determinant;
    return string_append_format(svg, " data-uv=\"%.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g\"", ux, uy,
        p[0].u - ux * p[0].x - uy * p[0].y, vx, vy, p[0].v - vx * p[0].x - vy * p[0].y,
        zx, zy, p[0].z - zx * p[0].x - zy * p[0].y) >= 0;
}

static bool finite_real(number_t value)
{
    return num_is_real(value) && num_is_finite(value);
}

static void project(const camera_t *view, vertex_t *vertex)
{
    double u = (vertex->u - 0.5) * view->units[0], v = (vertex->v - 0.5) * view->units[1];
    double z = (vertex->z - 0.5) * view->units[2];
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

static vertex_t intersection(vertex_t a, vertex_t b, number_t boundary)
{
    NUM_SCOPE(numbers);
    number_t da = num_sub(boundary, a.height), db = num_sub(boundary, b.height);
    if (num_gt(num_abs(da), num_abs(db))) {
        vertex_t swap = a;
        a = b;
        b = swap;
    }
    double t = fraction(boundary, a.height, num_sub(b.height, a.height));
    return (vertex_t){.u = a.u + t * (b.u - a.u), .v = a.v + t * (b.v - a.v), .height = boundary};
}

static size_t clip_plane(const vertex_t *input, size_t count, vertex_t *output, number_t boundary, bool upper)
{
    size_t used = 0;
    if (!count)
        return 0;
    vertex_t previous = input[count - 1];
    bool inside = upper ? num_le(previous.height, boundary) : num_ge(previous.height, boundary);
    for (size_t i = 0; i < count; ++i) {
        vertex_t next = input[i];
        bool keep = upper ? num_le(next.height, boundary) : num_ge(next.height, boundary);
        if (keep != inside)
            output[used++] = intersection(previous, next, boundary);
        if (keep)
            output[used++] = next;
        previous = next;
        inside = keep;
    }
    return used;
}

/* Parallel height planes cut a triangle into at most five vertices; no plateau is added. */
static int clip_facet(const vertex_t *vertices, const facet_t *facet, const camera_t *view,
                      const number_t bounds[2], number_t span, vertex_t output[5], double *depth)
{
    vertex_t triangle[] = {vertices[facet->a], vertices[facet->b], vertices[facet->c]}, lower[4];
    size_t count = clip_plane(triangle, 3, lower, bounds[0], false);
    count = clip_plane(lower, count, output, bounds[1], true);
    *depth = 0;
    for (size_t i = 0; i < count; ++i) {
        output[i].z = fraction(output[i].height, bounds[0], span);
        project(view, &output[i]);
        if (!isfinite(output[i].x) || !isfinite(output[i].y) || !isfinite(output[i].depth)) {
            errno = ERANGE;
            return -1;
        }
        *depth += output[i].depth;
    }
    if (count)
        *depth /= count;
    return (int)count;
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

static void include_extent(double extent[4], vertex_t at)
{
    extent[0] = fmin(extent[0], at.x);
    extent[1] = fmax(extent[1], at.x);
    extent[2] = fmin(extent[2], at.y);
    extent[3] = fmax(extent[3], at.y);
}

static void fit_camera(const plot_t *plot, camera_t *view, double extent[4], double label_width)
{
    if (plot->segments) {
        /* Match the retained segment camera without fitting visible branches or axes.
         * The fixed sphere keeps the baseline scale invariant under rotation. */
        double margin = fmin(fmax(45, label_width + 25), (plot->options.width - 80) / 2.0);
        double diameter = sqrt(view->units[0] * view->units[0] + view->units[1] * view->units[1] +
                               view->units[2] * view->units[2]);
        view->scale_x = view->scale_y =
            fmin(plot->options.width - 2 * margin, plot->options.height - 100) / diameter;
        view->cx = plot->options.width * 0.5;
        view->cy = plot->options.height * 0.5;
        return;
    }
    for (unsigned axis = 0; axis < 3; ++axis) {
        include_extent(extent, axis_vertex(view, axis, 0));
        include_extent(extent, axis_vertex(view, axis, 1));
    }
    double margin = fmin(fmax(45, label_width + 25), (plot->options.width - 80) / 2.0);
    double width = extent[1] - extent[0], height = extent[3] - extent[2];
    if (!(width > 0) || !(height > 0))
        return;
    /* Fit visible facets and axes, not the empty corners of a projected cuboid. */
    double scale = fmin((plot->options.width - 2 * margin) / width,
                       (plot->options.height - 100) / height);
    view->cx = plot->options.width * 0.5 + scale * (view->cx - (extent[0] + extent[1]) / 2);
    view->cy = plot->options.height * 0.5 + scale * (view->cy - (extent[2] + extent[3]) / 2);
    view->scale_x *= scale;
    view->scale_y *= scale;
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
             plot_tick_text(svg, &labels, names[axis], axis + 3,
                            b.x + dx * (plot->segments ? 26 : 25), b.y + dy * (plot->segments ? 26 : 25) + 4,
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
            string_t *label = plot_tick_label(ticks[i], span);
            ok = ok && label && string_append_format(svg, "<g data-plot-tick=\"%u\" data-tick-fraction=\"%.17g\" "
                  "stroke=\"#%06x\" stroke-width=\"1.2\">", axis, t,
                  (unsigned)plot->options.axis_colour) >= 0 && stroke(svg, at, tip) &&
                 string_append_cstr(svg, "</g>") == 0 &&
                 plot_tick_text(svg, &labels, label, axis, at.x + nx * 13 + side * 3,
                                at.y + ny * 13 + 4, side, nx, ny);
            string_free(label);
            num_destroy(&ticks[i]);
        }
        num_destroy(&span);
    }
    ok = ok && string_append_cstr(svg, "</g>\n") == 0;
    /* Keep axis grab areas above facets and labels. Non-scaling strokes preserve
     * a finger-sized target when a mobile client fits the SVG into a narrow card. */
    for (unsigned axis = 0; ok && axis < 3; ++axis) {
        vertex_t a = axis_vertex(view, axis, 0), b = axis_vertex(view, axis, 1);
        double length = hypot(b.x - a.x, b.y - a.y);
        b.x += (b.x - a.x) / length * 25;
        b.y += (b.y - a.y) / length * 25;
        ok = string_append_format(svg,
            "<path data-axis-handle=\"%u\" d=\"M %.3f %.3f L %.3f %.3f\" "
            "fill=\"none\" stroke=\"transparent\" stroke-width=\"44\" stroke-linecap=\"round\" "
            "vector-effect=\"non-scaling-stroke\" pointer-events=\"stroke\"/>\n",
            axis, a.x, a.y, b.x, b.y) >= 0;
    }
    return ok;
}

/* Preserve native coordinate precision across browser gestures and resampling. */
static bool coordinate_metadata(string_t *svg, const number_t bounds[6])
{
    static const char *const names[] = {"x-min", "x-max", "y-min", "y-max", "z-min", "z-max"};
    for (unsigned i = 0; i < 6; ++i) {
        size_t bits = num_get_effective_prec_bits(bounds[i]);
        int digits = (int)(bits * 30103 / 100000 + 3);
        number_t decimal = num_as_inexact_real_prec(bounds[i], bits);
        string_t *exact = num_sprintf_text("%.*n", digits, decimal);
        number_t span = num_sub(bounds[(i / 2) * 2 + 1], bounds[(i / 2) * 2]);
        number_t magnitude = num_abs(decimal);
        long difference = num_is_zero(magnitude) ? 0 : num_get_exponent2(magnitude) - num_get_exponent2(span);
        int shown = difference > 0 ? (int)(difference * 30103 / 100000 + 5) : 4;
        string_t *label = num_sprintf_text("%.*n", shown, decimal);
        bool ok = exact && label && string_append_format(svg, " data-exact-%s=\"", names[i]) >= 0 &&
                  plot_escape(svg, exact) && string_append_format(svg, "\" data-label-%s=\"", names[i]) >= 0 &&
                  plot_escape(svg, label) && string_append_cstr(svg, "\"") == 0;
        string_free(exact);
        string_free(label);
        num_destroy(&magnitude);
        num_destroy(&span);
        num_destroy(&decimal);
        if (!ok) return false;
    }
    return true;
}

/* Clip a normalised space-curve segment to the viewing volume before projection. */
static bool segment(const vertex_t *vertices, size_t index, const camera_t *view,
                     const number_t bounds[2], number_t span, vertex_t out[2])
{
    double points[2][3];
    for (unsigned i = 0; i < 2; ++i) {
        const vertex_t *v = vertices + index + i;
        if (!v->valid)
            return false;
        points[i][0] = v->u;
        points[i][1] = v->v;
        points[i][2] = fraction(v->height, bounds[0], span);
    }
    double start = 0, end = 1;
    for (unsigned axis = 0; axis < 3; ++axis) {
        double a = points[0][axis], delta = points[1][axis] - a;
        if (!delta) {
            if (a < 0 || a > 1)
                return false;
            continue;
        }
        double first = -a / delta, last = (1 - a) / delta;
        start = fmax(start, fmin(first, last));
        end = fmin(end, fmax(first, last));
    }
    if (start > end)
        return false;
    for (unsigned i = 0; i < 2; ++i) {
        double t = i ? end : start;
        out[i] = (vertex_t){.u = points[0][0] + t * (points[1][0] - points[0][0]),
                            .v = points[0][1] + t * (points[1][1] - points[0][1]),
                            .z = points[0][2] + t * (points[1][2] - points[0][2])};
        project(view, &out[i]);
    }
    return true;
}

/* Render native three-dimensional geometry into an independently owned SVG string. */
string_t *plot_surface_svg(const plot_t *plot)
{
    NUM_SCOPE(numbers);
    size_t columns = plot->surface_columns, rows = plot->surface_rows, count = columns * rows;
    number_t height_bounds[] = {NUM_NAN, NUM_NAN}, full[] = {NUM_NAN, NUM_NAN};
    if (!plot_surface_range(plot, height_bounds, full))
        return NULL;
    number_t lo = height_bounds[0], hi = height_bounds[1], full_span = num_sub(full[1], full[0]);
    if (num_is_zero(full_span))
        full_span = num_clone(NUM_ONE);
    number_t bounds[] = {count ? plot->surface_x[0] : plot->bounds[0],
                         count ? plot->surface_x[columns - 1] : plot->bounds[1],
                         count ? plot->surface_y[0] : plot->bounds[2],
                         count ? plot->surface_y[rows - 1] : plot->bounds[3], lo, hi};
    if (plot->mesh) {
        bounds[1] = bounds[0];
        bounds[3] = bounds[2];
        const number_t *coordinates[] = {plot->surface_x, plot->surface_y};
        for (unsigned axis = 0; axis < 2; ++axis) {
            for (size_t i = 0; i < count; ++i) {
                if (num_lt(coordinates[axis][i], bounds[2 * axis]))
                    bounds[2 * axis] = coordinates[axis][i];
                if (num_gt(coordinates[axis][i], bounds[2 * axis + 1]))
                    bounds[2 * axis + 1] = coordinates[axis][i];
            }
            if (num_eq(bounds[2 * axis], bounds[2 * axis + 1])) {
                bounds[2 * axis] = num_sub(bounds[2 * axis], NUM_ONE);
                bounds[2 * axis + 1] = num_add(bounds[2 * axis + 1], NUM_ONE);
            }
        }
        if (plot->fixed)
            for (unsigned i = 0; i < 4; ++i)
                bounds[i] = plot->bounds[i];
    }
    double label_width = 0;
    for (unsigned axis = 0; axis < 3; ++axis) {
        double width = plot_tick_width(bounds[axis * 2], bounds[axis * 2 + 1], 11);
        if (width < 0)
            return NULL;
        label_width = fmax(label_width, width);
    }
    camera_t view = camera(plot, label_width, bounds);
    bool faceted = plot->mesh || plot->refined_count;
    const number_t *x = plot->refined_count ? plot->refined_x : plot->surface_x;
    const number_t *y = plot->refined_count ? plot->refined_y : plot->surface_y;
    const number_t *z = plot->refined_count ? plot->refined_z : plot->surface_z;
    if (plot->refined_count) {
        columns = count = plot->refined_count;
        rows = 1;
    }
    number_t spans[] = {num_sub(bounds[1], bounds[0]), num_sub(bounds[3], bounds[2]), num_sub(hi, lo)};
    if (count > SIZE_MAX / (2 * sizeof(facet_t))) {
        errno = E2BIG;
        return NULL;
    }
    vertex_t *vertices = calloc(count ? count : 1, sizeof(*vertices));
    facet_t *facets = calloc(count ? count * 2 : 1, sizeof(*facets));
    string_t *svg = string_new();
    bool ok = vertices && facets && svg;
    for (size_t r = 0; ok && r < rows; ++r) {
        for (size_t c = 0; ok && c < columns; ++c) {
            ok = computation_step_current("plot.surface.project");
            vertex_t *at = &vertices[r * columns + c];
            number_t value = z[r * columns + c];
            if (!finite_real(value))
                continue;
            at->u = fraction(x[c], bounds[0], spans[0]);
            at->v = fraction(y[faceted ? c : r], bounds[2], spans[1]);
            at->height = value;
            at->z = fraction(value, full[0], full_span);
            at->valid = isfinite(at->u) && isfinite(at->v) && isfinite(at->z);
        }
    }
    size_t faces = 0;
    double extent[] = {INFINITY, -INFINITY, INFINITY, -INFINITY};
    for (size_t i = 0; ok && faceted && !plot->segments && i + 2 < count; i += 3) {
        facet_t facet = {.a = i, .b = i + 1, .c = i + 2};
        vertex_t polygon[5];
        int clipped = clip_facet(vertices, &facet, &view, height_bounds, spans[2], polygon, &facet.depth);
        ok = clipped >= 0;
        if (clipped >= 3) {
            facets[faces++] = facet;
            for (int j = 0; j < clipped; ++j)
                include_extent(extent, polygon[j]);
        }
    }
    for (size_t r = 0; ok && r + 1 < rows; ++r) {
        for (size_t c = 0; ok && c + 1 < columns; ++c) {
            ok = computation_step_current("plot.surface.mesh");
            size_t indices[] = {r * columns + c, r * columns + c + 1,
                                (r + 1) * columns + c + 1, (r + 1) * columns + c};
            bool valid = true;
            for (unsigned j = 0; j < 4; ++j) {
                const vertex_t *v = &vertices[indices[j]];
                valid = valid && v->valid;
            }
            if (!valid)
                continue;
            for (unsigned j = 1; ok && j < 3; ++j) {
                size_t a = indices[0], b = indices[j], d = indices[j + 1];
                facet_t facet = {.a = a, .b = b, .c = d};
                vertex_t polygon[5];
                int clipped = clip_facet(vertices, &facet, &view, height_bounds, spans[2], polygon, &facet.depth);
                ok = clipped >= 0;
                if (clipped >= 3) {
                    facets[faces++] = facet;
                    for (int k = 0; k < clipped; ++k)
                        include_extent(extent, polygon[k]);
                }
            }
        }
    }
    size_t lines = 0;
    for (size_t i = 0; ok && plot->segments && i + 1 < count; i += 2) {
        vertex_t ends[2];
        ok = computation_step_current("plot.segment.project");
        if (segment(vertices, i, &view, height_bounds, spans[2], ends)) {
            ++lines;
            include_extent(extent, ends[0]);
            include_extent(extent, ends[1]);
        }
    }
    if (ok && !faces && !lines && !plot->height_fixed) {
        errno = ENODATA;
        ok = false;
    }
    if (ok) {
        fit_camera(plot, &view, extent, label_width);
        qsort(facets, faces, sizeof(*facets), far_first);
        ok = string_append_format(svg, "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%u\" height=\"%u\" "
                 "viewBox=\"0 0 %u %u\" role=\"img\" data-surface=\"true\" "
                 "data-azimuth=\"%.3f\" data-elevation=\"%.3f\" data-roll=\"%.3f\" "
                 "data-x-min=\"%.17g\" data-x-max=\"%.17g\" data-y-min=\"%.17g\" data-y-max=\"%.17g\" "
                 "data-z-min=\"%.17g\" data-z-max=\"%.17g\" data-mesh=\"%s\" "
                 "data-units=\"%.17g %.17g %.17g\" data-label-width=\"%.17g\"",
                 plot->options.width, plot->options.height, plot->options.width, plot->options.height,
                 plot->azimuth, plot->elevation, plot->roll, num_to_double(bounds[0]), num_to_double(bounds[1]),
                 num_to_double(bounds[2]), num_to_double(bounds[3]), num_to_double(lo), num_to_double(hi),
                 plot->mesh ? "true" : "false", view.units[0], view.units[1], view.units[2], label_width) >= 0;
        number_t coordinates[] = {bounds[0], bounds[1], bounds[2], bounds[3], lo, hi};
        if (ok) ok = coordinate_metadata(svg, coordinates);
        if (ok && plot->segments)
            ok = string_append_cstr(svg, " data-segments=\"true\"") == 0;
        if (ok && !faces && !lines)
            ok = string_append_cstr(svg, " data-empty=\"true\"") == 0;
        if (ok && count && isfinite(num_to_double(full[0])) && isfinite(num_to_double(full[1])))
            ok = string_append_format(svg, " data-sample-z-min=\"%.17g\" data-sample-z-max=\"%.17g\"",
                                       num_to_double(full[0]), num_to_double(full[1])) >= 0;
        ok = ok && string_append_cstr(svg, "><title>") == 0 &&
             plot_escape(svg, plot->title) && string_append_format(svg, "</title><text x=\"%u\" y=\"24\" "
                 "text-anchor=\"middle\" fill=\"#%06x\" font-family=\"sans-serif\" font-size=\"14\">",
                 plot->options.width / 2, (unsigned)plot->options.label_colour) >= 0 && plot_escape(svg, plot->title) &&
             string_append_cstr(svg, "</text><g data-series=\"0\" stroke=\"#193d46\" "
                 "stroke-opacity=\"0.35\" stroke-width=\"0.45\" stroke-linejoin=\"round\">\n") == 0;
    }
    for (size_t i = 0; ok && i < faces; ++i) {
        const facet_t *f = &facets[i];
        vertex_t polygon[5];
        double depth, height = 0;
        int clipped = clip_facet(vertices, f, &view, height_bounds, spans[2], polygon, &depth);
        ok = computation_step_current("plot.surface.draw") && clipped >= 3;
        for (int j = 0; j < clipped; ++j)
            height += polygon[j].z;
        if (ok)
            ok = string_append_format(svg, "<path fill=\"#%06x\" d=\"M %.3f %.3f", colour(height / clipped),
                                       polygon[0].x, polygon[0].y) >= 0;
        for (int j = 1; ok && j < clipped; ++j)
            ok = string_append_format(svg, " L %.3f %.3f", polygon[j].x, polygon[j].y) >= 0;
        ok = ok && string_append_cstr(svg, " Z\" data-vertices=\"") == 0;
        for (int j = 0; ok && j < clipped; ++j)
            ok = string_append_format(svg, "%s%.9g %.9g %.9g", j ? " " : "",
                                      polygon[j].u, polygon[j].v, polygon[j].z) >= 0;
        ok = ok && string_append_cstr(svg, "\"") == 0 && pointer_map(svg, polygon) &&
             string_append_cstr(svg, "/>\n") == 0;
    }
    for (size_t i = 0; ok && plot->segments && i + 1 < count; i += 2) {
        vertex_t ends[2];
        if (!segment(vertices, i, &view, height_bounds, spans[2], ends))
            continue;
        ok = computation_step_current("plot.segment.draw") && string_append_format(svg,
            "<path fill=\"none\" stroke=\"#78d8cb\" stroke-opacity=\"1\" stroke-width=\"2\" "
            "d=\"M %.3f %.3f L %.3f %.3f\" data-segment=\"%.9g %.9g %.9g %.9g %.9g %.9g\"/>\n",
            ends[0].x, ends[0].y, ends[1].x, ends[1].y,
            ends[0].u, ends[0].v, ends[0].z, ends[1].u, ends[1].v, ends[1].z) >= 0;
    }
    if (ok && !faces && !lines)
        ok = string_append_format(svg, "<text x=\"%u\" y=\"%u\" text-anchor=\"middle\" stroke=\"none\" "
                    "fill=\"#%06x\" font-family=\"sans-serif\" font-size=\"14\">"
                    "No surface found in this view</text>", plot->options.width / 2, plot->options.height / 2,
                    (unsigned)plot->options.label_colour) >= 0;
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
