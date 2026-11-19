/**
 * @file plot_surface_svg.c
 * @brief Rotatable SVG grids, meshes and space curves with a shared equal-unit camera.
 *
 * Numerical ranges are normalised before conversion to drawing coordinates.
 * Missing vertices leave holes. Finite heights are joined regardless of slope;
 * callers with an evaluator must sample domain gaps before supplying the mesh.
 * The equal-unit camera determines projection, facet order and a connected exterior axis frame
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
#define MARS_PLOT_INTERNAL_ACCESS
#include "plot_axis_frame.h"
#define MARS_PLOT_INTERNAL_ACCESS
#include "plot_projection.h"

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
    double units[3];
    plot_projection_t projection;
    plot_axis_frame_t frame;
} camera_t;

static camera_t camera(const plot_t *plot, double label_width, const number_t bounds[6])
{
    NUM_SCOPE(numbers);
    double radians = acos(-1) / 180;
    camera_t view = {.units = {1, 1, 1}};
    double ca = cos(plot->azimuth * radians), sa = sin(plot->azimuth * radians);
    double ce = cos(plot->elevation * radians), se = sin(plot->elevation * radians);
    double cr = cos(plot->roll * radians), sr = sin(plot->roll * radians);
    /* Height grids and implicit meshes share one physical scale on every axis. */
    number_t spans[] = {num_sub(bounds[1], bounds[0]), num_sub(bounds[3], bounds[2]),
                        num_sub(bounds[5], bounds[4])};
    number_t longest = spans[0];
    for (unsigned i = 1; i < 3; ++i)
        if (num_gt(spans[i], longest))
            longest = spans[i];
    for (unsigned i = 0; i < 3; ++i)
        view.units[i] = num_to_double(num_div(spans[i], longest));
    double horizontal[] = {ca * view.units[0], -sa * view.units[1], 0};
    double vertical[] = {-se * sa * view.units[0], -se * ca * view.units[1], -ce * view.units[2]};
    double rows[9];
    for (unsigned i = 0; i < 3; ++i) {
        rows[i] = cr * horizontal[i] - sr * vertical[i];
        rows[3 + i] = sr * horizontal[i] + cr * vertical[i];
    }
    rows[6] = ce * sa * view.units[0]; rows[7] = ce * ca * view.units[1]; rows[8] = -se * view.units[2];
    plot_projection_init(rows, plot->options.width, plot->options.height, label_width,
                         plot->segments, &view.projection);
    return view;
}

/* Perspective-correct barycentric coordinates use three affine numerators and one shared denominator. */
static bool pointer_map(string_t *svg, const vertex_t p[3], double distance)
{
    double x1 = p[1].x - p[0].x, y1 = p[1].y - p[0].y;
    double x2 = p[2].x - p[0].x, y2 = p[2].y - p[0].y;
    double determinant = x1 * y2 - x2 * y1;
    if (fabs(determinant) < 1e-12)
        return true;
    double coefficients[12];
    for (unsigned axis = 0; axis < 4; ++axis) {
        double values[3];
        for (unsigned i = 0; i < 3; ++i) {
            double attributes[] = {p[i].u, p[i].v, p[i].z, 1};
            values[i] = attributes[axis] / (1 + p[i].depth / distance);
        }
        double a = ((values[1] - values[0]) * y2 - (values[2] - values[0]) * y1) / determinant;
        double b = (x1 * (values[2] - values[0]) - x2 * (values[1] - values[0])) / determinant;
        coefficients[axis * 3] = a; coefficients[axis * 3 + 1] = b;
        coefficients[axis * 3 + 2] = values[0] - a * p[0].x - b * p[0].y;
    }
    bool ok = string_append_cstr(svg, " data-uv=\"") == 0;
    for (unsigned i = 0; ok && i < 9; ++i)
        ok = string_append_format(svg, "%s%.17g", i ? " " : "", coefficients[i]) >= 0;
    return ok && string_append_format(svg, "\" data-uv-denominator=\"%.17g %.17g %.17g\"",
                                      coefficients[9], coefficients[10], coefficients[11]) >= 0;
}

static bool finite_real(number_t value)
{
    return num_is_real(value) && num_is_finite(value);
}

static void project(const camera_t *view, vertex_t *vertex)
{
    double point[] = {vertex->u, vertex->v, vertex->z}, camera[3], screen[3];
    plot_projection_camera(&view->projection, point, camera);
    plot_projection_point(&view->projection, camera, screen);
    vertex->x = screen[0]; vertex->y = screen[1]; vertex->depth = screen[2];
}

static bool frame_axes(camera_t *view)
{
    double corners[8][2];
    for (unsigned i = 0; i < 8; ++i) {
        vertex_t at = {.u = !!(i & 1), .v = !!(i & 2), .z = !!(i & 4)};
        project(view, &at);
        corners[i][0] = at.x;
        corners[i][1] = at.y;
    }
    return plot_axis_frame(corners, NULL, &view->frame);
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
    unsigned corner = view->frame.corner[axis];
    vertex_t at = {.u = !!(corner & 1), .v = !!(corner & 2), .z = !!(corner & 4)};
    if (axis == 0)
        at.u = t;
    else if (axis == 1)
        at.v = t;
    else
        at.z = t;
    project(view, &at);
    return at;
}

static bool solid_axis(const camera_t *view, unsigned axis, plot_axis_solid_t *solid)
{
    unsigned corner = view->frame.corner[axis];
    double lower[] = {!!(corner & 1), !!(corner & 2), !!(corner & 4)}, upper[3];
    for (unsigned i = 0; i < 3; ++i) upper[i] = lower[i];
    upper[axis] = 1;
    return plot_axis_solid(&view->projection, lower, upper, axis, solid);
}

static unsigned axis_colour(unsigned colour, double shade)
{
    unsigned out = 0;
    for (unsigned i = 0; i < 3; ++i) {
        unsigned shift = (2 - i) * 8;
        out |= (unsigned)lround(((colour >> shift) & 255) * shade) << shift;
    }
    return out;
}

static bool axes(string_t *svg, const plot_t *plot, const camera_t *view, number_t bounds[6])
{
    plot_axis_solid_t solids[3] = {0};
    plot_axis_face_t faces[PLOT_AXIS_FACES * 3];
    unsigned count = 0, base_colour = plot->options.axis_colour;
    for (unsigned axis = 0; axis < 3; ++axis) {
        if (!solid_axis(view, axis, &solids[axis])) continue;
        for (unsigned i = 0; i < solids[axis].count; ++i) faces[count++] = solids[axis].face[i];
    }
    qsort(faces, count, sizeof *faces, plot_axis_face_order);
    bool ok = string_append_cstr(svg, "<g data-axis-solids=\"true\" stroke-linejoin=\"round\">\n") == 0;
    for (unsigned i = 0; ok && i < count; ++i) {
        const plot_axis_face_t *face = &faces[i];
        unsigned colour = axis_colour(base_colour, face->shade);
        ok = string_append_format(svg, "<path data-axis-%s=\"%u\" data-axis-colour=\"#%06x\" "
            "data-axis-shade=\"%.6f\" fill=\"#%06x\" stroke=\"#%06x\" stroke-width=\"0.35\" d=\"",
            face->cone ? "cone" : "cylinder", face->axis, base_colour, face->shade, colour, colour) >= 0;
        for (unsigned j = 0; ok && j < face->count; ++j)
            ok = string_append_format(svg, "%s %.3f %.3f", j ? " L" : "M",
                                       face->point[j][0], face->point[j][1]) >= 0;
        ok = ok && string_append_cstr(svg, " Z\"/>\n") == 0;
    }
    /* Retain centreline metadata for native inspection and drag targets, without drawing wire arrows. */
    ok = ok && string_append_cstr(svg, "</g><g data-plot-axes=\"true\" opacity=\"0\">\n") == 0;
    for (unsigned axis = 0; ok && axis < 3; ++axis) {
        if (!solids[axis].count) continue;
        vertex_t start = axis_vertex(view, axis, 0);
        const plot_axis_arrow_t *arrow = &solids[axis].arrow;
        ok = string_append_format(svg, "<path data-axis-line=\"%u\" d=\"M %.3f %.3f L %.3f %.3f\"/>\n",
            axis, start.x, start.y, arrow->tip[0], arrow->tip[1]) >= 0 && string_append_format(svg,
            "<path data-axis-arrow=\"%u\" d=\"M %.3f %.3f L %.3f %.3f L %.3f %.3f\"/>\n", axis,
            arrow->wing[0][0], arrow->wing[0][1], arrow->tip[0], arrow->tip[1],
            arrow->wing[1][0], arrow->wing[1][1]) >= 0;
    }
    ok = ok && string_append_format(svg, "</g><g fill=\"#%06x\" font-family=\"sans-serif\" "
        "stroke=\"#10161d\" stroke-width=\"2.8\" stroke-linejoin=\"round\" paint-order=\"stroke\">\n",
        (unsigned)plot->options.label_colour) >= 0;
    plot_tick_layout_t labels = {.width = plot->options.width, .height = plot->options.height, .sized = true};
    const string_t *names[] = {plot->x_label, plot->y_label, plot->z_label};
    for (unsigned axis = 0; ok && axis < 3; ++axis) {
        if (!solids[axis].count) continue;
        const plot_axis_arrow_t *arrow = &solids[axis].arrow;
        double scale = solids[axis].name_scale, dx = arrow->direction[0], dy = arrow->direction[1];
        labels.font_size = 14 * scale;
        int anchor = dx < -0.1 ? -1 : dx > 0.1;
        ok = string_append_format(svg, "<g data-axis-name=\"%u\" font-weight=\"600\">", axis) >= 0 &&
             plot_tick_text(svg, &labels, names[axis], axis + 3,
                 arrow->name[0] + view->frame.normal[axis][0] * 10 * scale,
                 arrow->name[1] + view->frame.normal[axis][1] * 10 * scale + labels.font_size * 0.3,
                 anchor, dx, dy) && string_append_cstr(svg, "</g>") == 0;
    }
    for (unsigned axis = 0; ok && axis < 3; ++axis) {
        if (!solids[axis].count) continue;
        vertex_t a = axis_vertex(view, axis, 0), b = axis_vertex(view, axis, 1);
        double length = hypot(b.x - a.x, b.y - a.y);
        double nx = view->frame.normal[axis][0], ny = view->frame.normal[axis][1];
        number_t ticks[12], span = num_sub(bounds[axis * 2 + 1], bounds[axis * 2]);
        size_t tick_count = plot_ticks(bounds[axis * 2], bounds[axis * 2 + 1], ticks);
        double positions[PLOT_AXIS_MAX_TICKS][2], scales[PLOT_AXIS_MAX_TICKS], fractions[PLOT_AXIS_MAX_TICKS];
        vertex_t points[PLOT_AXIS_MAX_TICKS];
        unsigned zero_tick = (unsigned)tick_count;
        for (size_t i = 0; i < tick_count; ++i) {
            fractions[i] = fraction(ticks[i], bounds[axis * 2], span);
            points[i] = axis_vertex(view, axis, fractions[i]);
            positions[i][0] = points[i].x; positions[i][1] = points[i].y;
            scales[i] = plot_projection_size(&view->projection, points[i].depth);
            if (num_is_zero(ticks[i])) zero_tick = (unsigned)i;
        }
        unsigned visible = plot_axis_tick_mask(positions, scales, (unsigned)tick_count, zero_tick);
        for (size_t i = 0; i < tick_count; ++i) {
            double t = fractions[i];
            vertex_t at = points[i], tip = at;
            double scale = scales[i];
            bool shown = (visible & (1u << i)) != 0;
            tip.x += nx * 6 * scale; tip.y += ny * 6 * scale;
            string_t *label = plot_tick_label(ticks[i], span);
            bool zero = zero_tick == i;
            labels.font_size = 13 * scale;
            double endpoint[] = {tip.x, tip.y}, normal[] = {nx, ny}, position[2];
            int side = plot_axis_tick_label_position(endpoint, normal, labels.font_size, position);
            ok = ok && label && string_append_format(svg, "<g data-plot-tick=\"%u\" data-tick-fraction=\"%.17g\" "
                "data-tick-label=\"", axis, t) >= 0 && plot_escape(svg, label) && string_append_format(svg,
                "\" data-tick-zero=\"%s\" data-tick-visible=\"%s\"%s stroke=\"#%06x\" stroke-width=\"%.3f\">",
                zero ? "true" : "false", shown ? "true" : "false", shown ? "" : " display=\"none\"",
                base_colour, 1.2 * scale) >= 0 && stroke(svg, at, tip) &&
                string_append_cstr(svg, "</g>") == 0 && (!shown || plot_tick_text(svg, &labels, label, axis,
                    position[0], position[1],
                    side, length < 96 ? nx : 0, length < 96 ? ny : 0));
            string_free(label); num_destroy(&ticks[i]);
        }
        num_destroy(&span);
    }
    ok = ok && string_append_cstr(svg, "</g>\n") == 0;
    for (unsigned axis = 0; ok && axis < 3; ++axis) {
        if (!solids[axis].count) continue;
        vertex_t start = axis_vertex(view, axis, 0);
        const plot_axis_arrow_t *arrow = &solids[axis].arrow;
        ok = string_append_format(svg, "<path data-axis-handle=\"%u\" d=\"M %.3f %.3f L %.3f %.3f\" "
            "fill=\"none\" stroke=\"transparent\" stroke-width=\"44\" stroke-linecap=\"round\" "
            "vector-effect=\"non-scaling-stroke\" pointer-events=\"stroke\"/>\n",
            axis, start.x, start.y, arrow->tip[0], arrow->tip[1]) >= 0;
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
        double width = plot_tick_width(bounds[axis * 2], bounds[axis * 2 + 1], 13);
        if (width < 0)
            return NULL;
        label_width = fmax(label_width, width);
    }
    camera_t view = camera(plot, label_width, bounds);
    if (!frame_axes(&view)) {
        errno = ERANGE;
        return NULL;
    }
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
    for (size_t i = 0; ok && faceted && !plot->segments && i + 2 < count; i += 3) {
        facet_t facet = {.a = i, .b = i + 1, .c = i + 2};
        vertex_t polygon[5];
        int clipped = clip_facet(vertices, &facet, &view, height_bounds, spans[2], polygon, &facet.depth);
        ok = clipped >= 0;
        if (clipped >= 3) {
            facets[faces++] = facet;
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
        }
    }
    if (ok && !faces && !lines && !plot->height_fixed) {
        errno = ENODATA;
        ok = false;
    }
    if (ok) {
        qsort(facets, faces, sizeof(*facets), far_first);
        ok = string_append_format(svg, "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%u\" height=\"%u\" "
                 "viewBox=\"0 0 %u %u\" role=\"img\" data-surface=\"true\" "
                 "data-azimuth=\"%.3f\" data-elevation=\"%.3f\" data-roll=\"%.3f\" "
                 "data-x-min=\"%.17g\" data-x-max=\"%.17g\" data-y-min=\"%.17g\" data-y-max=\"%.17g\" "
                 "data-z-min=\"%.17g\" data-z-max=\"%.17g\" data-mesh=\"%s\" "
                 "data-units=\"%.17g %.17g %.17g\" data-label-width=\"%.17g\" "
                 "data-axis-corners=\"%u %u %u\"",
                 plot->options.width, plot->options.height, plot->options.width, plot->options.height,
                 plot->azimuth, plot->elevation, plot->roll, num_to_double(bounds[0]), num_to_double(bounds[1]),
                 num_to_double(bounds[2]), num_to_double(bounds[3]), num_to_double(lo), num_to_double(hi),
                 plot->mesh ? "true" : "false", view.units[0], view.units[1], view.units[2], label_width,
                 view.frame.corner[0], view.frame.corner[1], view.frame.corner[2]) >= 0;
        number_t coordinates[] = {bounds[0], bounds[1], bounds[2], bounds[3], lo, hi};
        if (ok) ok = coordinate_metadata(svg, coordinates);
        if (ok) ok = string_append_format(svg, " data-camera-distance=\"%.17g\" data-camera-fit=\"%.17g\" "
            "data-camera-centre=\"%.17g %.17g\"", view.projection.distance, view.projection.fit,
            view.projection.centre[0], view.projection.centre[1]) >= 0;
        if (ok) ok = string_append_cstr(svg, " data-camera-rows=\"") == 0;
        for (unsigned i = 0; ok && i < 9; ++i)
            ok = string_append_format(svg, "%s%.17g", i ? " " : "", view.projection.rows[i]) >= 0;
        if (ok) ok = string_append_cstr(svg, "\"") == 0;
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
        ok = ok && string_append_cstr(svg, "\"") == 0 && pointer_map(svg, polygon, view.projection.distance) &&
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
