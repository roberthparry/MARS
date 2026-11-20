/**
 * @file plot_projection.c
 * @brief Shared pinhole camera, constant-radius axis cylinders and cone arrowheads.
 *
 * The eye is at camera (0, 0, -D). Geometry projects with f/(D+z), and readable
 * glyphs use the same denominator with a size referenced to the far sphere plane.
 * A fixed sixteen-sided mesh supplies opaque diffuse shading to native SVG and
 * retained Canvas/SVG renderers. Tick strokes use current projected separation;
 * their numerical catalogue remains intact. Camera fitting includes extended shafts,
 * cone tips and names before any surface vertices are projected. No absolute mathematical coordinates are read.
 */
#include <math.h>
#include <stddef.h>

#define MARS_PLOT_INTERNAL_ACCESS
#include "plot_projection.h"

/* Give intersecting coordinate axes stable perpendicular normals under the existing perspective camera. */
bool plot_projection_origin_frame(const plot_projection_t *projection, const double origin[3], plot_axis_frame_t *frame)
{
    if (!projection || !origin || !frame)
        return false;
    for (unsigned i = 0u; i < 3u; ++i)
        if (!isfinite(origin[i]))
            return false;
    plot_axis_frame_t result = {0};
    for (unsigned axis = 0u; axis < 3u; ++axis) {
        double lower[3], upper[3], a[3], b[3], camera[3];
        for (unsigned i = 0u; i < 3u; ++i)
            lower[i] = upper[i] = origin[i];
        lower[axis] = 0;
        upper[axis] = 1;
        plot_projection_camera(projection, lower, camera);
        plot_projection_point(projection, camera, a);
        plot_projection_camera(projection, upper, camera);
        plot_projection_point(projection, camera, b);
        double dx = b[0] - a[0], dy = b[1] - a[1], length = hypot(dx, dy);
        double nx = length > 1e-7 ? -dy / length : -1, ny = length > 1e-7 ? dx / length : 0;
        if (nx > 1e-9 || (fabs(nx) <= 1e-9 && ny < 0)) {
            nx = -nx;
            ny = -ny;
        }
        result.normal[axis][0] = nx;
        result.normal[axis][1] = ny;
    }
    *frame = result;
    return true;
}

/* Rotate and scale normalised display coordinates without applying perspective twice. */
void plot_projection_camera(const plot_projection_t *projection, const double point[3], double camera[3])
{
    double out[3] = {0};
    for (unsigned row = 0; row < 3; ++row)
        for (unsigned axis = 0; axis < 3; ++axis)
            out[row] += projection->rows[row * 3 + axis] * (point[axis] - 0.5);
    for (unsigned row = 0; row < 3; ++row) camera[row] = out[row];
}

/* A single homogeneous denominator drives positions, widths and readable text. */
void plot_projection_point(const plot_projection_t *projection, const double camera[3], double screen[3])
{
    double depth = camera[2], denominator = projection->distance + depth;
    double scale = projection->distance ? (denominator > 0 ? projection->distance / denominator : NAN) : 1;
    double x = projection->centre[0] + projection->fit * camera[0] * scale;
    double y = projection->centre[1] + projection->fit * camera[1] * scale;
    screen[0] = x; screen[1] = y; screen[2] = depth;
}

/* Reference the minimum readable size at the far enclosing-sphere plane. */
double plot_projection_size(const plot_projection_t *projection, double depth)
{
    if (!projection->distance) return 1;
    return projection->distance + depth > 0 ?
        (projection->distance + projection->radius) / (projection->distance + depth) : NAN;
}

/* Position the nearest text edge beside the tick tip rather than adding a fixed baseline offset. */
int plot_axis_tick_label_position(const double tip[2], const double normal[2], double font_size, double out[2])
{
    double gap = font_size * (1.5 / 13.0);
    out[0] = tip[0] + normal[0] * gap;
    out[1] = tip[1] + normal[1] * gap + font_size * (0.35 + 0.65 * normal[1]);
    return normal[0] < -1e-9 ? -1 : normal[0] > 1e-9;
}

/* Keep the catalogue intact while suppressing only strokes that overlap in the current projection. */
unsigned plot_axis_tick_mask(const double positions[][2], const double scales[], unsigned count, unsigned zero)
{
    if (!positions || !scales || !count || count > PLOT_AXIS_MAX_TICKS) return 0;
    for (unsigned i = 0; i < count; ++i)
        if (!isfinite(positions[i][0]) || !isfinite(positions[i][1]) ||
            !isfinite(scales[i]) || !(scales[i] > 0)) return 0;
    unsigned mask = zero < count ? 1u << zero : 0;
    /* Bounded label geometry, independent of mesh size; each of the at most twelve entries is visited once. */
    for (unsigned pass = 0; pass < count; ++pass) {
        unsigned i = !pass ? 0 : pass == 1 ? count - 1 : pass - 1;
        if (mask & (1u << i)) continue;
        bool clear = true;
        for (unsigned j = 0; clear && j < count; ++j) {
            if (!(mask & (1u << j))) continue;
            double dx = positions[i][0] - positions[j][0], dy = positions[i][1] - positions[j][1];
            double gap = 4 * (scales[i] + scales[j]);
            clear = dx * dx + dy * dy >= gap * gap;
        }
        if (clear) mask |= 1u << i;
    }
    return mask;
}

/* Fit all projected corners, or the exact pinhole silhouette of the rotation-stable sphere. */
bool plot_projection_init(const double rows[9], double width, double height, double label_width,
                          bool sphere, plot_projection_t *projection)
{
    if (!rows || !projection || !(width > 80) || !(height > 120) || !isfinite(width) ||
        !isfinite(height) || !isfinite(label_width) || label_width < 0) return false;
    plot_projection_t p = {.fit = 1};
    double squared = 0;
    for (unsigned i = 0; i < 9; ++i) {
        if (!isfinite(rows[i])) return false;
        p.rows[i] = rows[i]; squared += rows[i] * rows[i];
    }
    p.radius = sqrt(squared) / 2;
    p.distance = 4 * p.radius;
    if (!(p.radius > 0) || !isfinite(p.radius) || !isfinite(p.distance)) return false;
    double low[2] = {INFINITY, INFINITY}, high[2] = {-INFINITY, -INFINITY};
    for (unsigned corner = 0; corner < 8; ++corner) {
        double point[] = {!!(corner & 1), !!(corner & 2), !!(corner & 4)}, camera[3], screen[3];
        plot_projection_camera(&p, point, camera);
        plot_projection_point(&p, camera, screen);
        for (unsigned i = 0; i < 2; ++i) {
            low[i] = fmin(low[i], screen[i]); high[i] = fmax(high[i], screen[i]);
        }
    }
    double margin = fmin(fmax(48, label_width * 5 / 3 + 16), (width - 80) / 2);
    /* Leave more space above the volume for its title and positive arrowheads. */
    double heading = fmin(48, fmax(0, height - 240) / 4), centre_y = (height + heading) / 2;
    double available[] = {width - 2 * margin, height - 96 - heading};
    if (sphere) {
        double diameter = 2 * p.radius * p.distance / sqrt(p.distance * p.distance - p.radius * p.radius);
        p.fit = fmin(available[0], available[1]) / diameter;
        p.centre[0] = width / 2; p.centre[1] = centre_y;
    } else {
        p.fit = fmin(available[0] / (high[0] - low[0]), available[1] / (high[1] - low[1]));
        p.centre[0] = width / 2 - p.fit * (low[0] + high[0]) / 2;
        p.centre[1] = centre_y - p.fit * (low[1] + high[1]) / 2;
    }
    if (!(p.fit > 0) || !isfinite(p.fit)) return false;
    *projection = p;
    return true;
}

/* Undo the orthogonal scaled columns and intersect a fixed three-slab box. */
bool plot_projection_contains(const plot_projection_t *p, double x, double y)
{
    if (!p || !(p->fit > 0) || !isfinite(x) || !isfinite(y)) return false;
    double origin[3] = {0, 0, -p->distance}, ray[] = {0, 0, 1};
    if (p->distance) {
        ray[0] = (x - p->centre[0]) / (p->fit * p->distance);
        ray[1] = (y - p->centre[1]) / (p->fit * p->distance);
    } else {
        origin[0] = (x - p->centre[0]) / p->fit;
        origin[1] = (y - p->centre[1]) / p->fit;
    }
    double near = p->distance ? 0 : -INFINITY, far = INFINITY;
    for (unsigned axis = 0; axis < 3; ++axis) {
        double norm = 0, start = 0, direction = 0;
        for (unsigned row = 0; row < 3; ++row) {
            double value = p->rows[row * 3 + axis];
            norm += value * value; start += value * origin[row]; direction += value * ray[row];
        }
        if (!(norm > 0) || !isfinite(norm)) return false;
        start /= norm; direction /= norm;
        if (!direction) {
            if (fabs(start) > 0.5 + 1e-10) return false;
        } else {
            double a = (-0.5 - 1e-10 - start) / direction, b = (0.5 + 1e-10 - start) / direction;
            near = fmax(near, fmin(a, b)); far = fmin(far, fmax(a, b));
            if (near > far) return false;
        }
    }
    return true;
}

static void cross(const double a[3], const double b[3], double out[3])
{
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

static double light(const double normal[3])
{
    double length = hypot(hypot(normal[0], normal[1]), normal[2]);
    double diffuse = (-0.35 * normal[0] - 0.45 * normal[1] - 0.8215838363 * normal[2]) / length;
    return 0.32 + 0.68 * fmax(0, diffuse);
}

static void face(const plot_projection_t *p, plot_axis_solid_t *solid, unsigned axis, bool cone,
                 const double points[][3], unsigned count, const double normal[3])
{
    double centre[3] = {0};
    for (unsigned i = 0; i < count; ++i)
        for (unsigned component = 0; component < 3; ++component) centre[component] += points[i][component] / count;
    double facing = p->distance ? -normal[0] * centre[0] - normal[1] * centre[1] -
                                  normal[2] * (p->distance + centre[2]) : -normal[2];
    if (facing <= 0) return;
    plot_axis_face_t *out = &solid->face[solid->count++];
    *out = (plot_axis_face_t){.count = count, .axis = axis, .cone = cone, .shade = light(normal)};
    for (unsigned i = 0; i < count; ++i) {
        double screen[3];
        plot_projection_point(p, points[i], screen);
        out->point[i][0] = screen[0]; out->point[i][1] = screen[1];
        out->depths[i] = screen[2];
        out->depth += screen[2] / count;
    }
}

typedef struct {
    double lower[3], upper[3], direction[3], pixel;
} axis_geometry_t;

static bool axis_camera_geometry(const plot_projection_t *p, const double lower[3], const double upper[3],
                                  bool extend, axis_geometry_t *geometry)
{
    double *a = geometry->lower, *b = geometry->upper, *delta = geometry->direction, length = 0;
    plot_projection_camera(p, lower, a); plot_projection_camera(p, upper, b);
    for (unsigned i = 0; i < 3; ++i) { delta[i] = b[i] - a[i]; length += delta[i] * delta[i]; }
    length = sqrt(length);
    if (!(length > 1e-10)) return false;
    for (unsigned i = 0; i < 3; ++i) delta[i] /= length;
    if (extend) {
        for (unsigned i = 0; i < 3; ++i) {
            a[i] -= delta[i] * length * 0.08;
            b[i] += delta[i] * length * 0.08;
        }
    }
    geometry->pixel = (p->distance ? (p->distance + p->radius) / p->distance : 1) / p->fit;
    return true;
}

/* Generate physical circles before projection; apparent taper is entirely projective. */
bool plot_axis_solid(const plot_projection_t *p, const double lower[3], const double upper[3],
                     unsigned axis, bool extend, plot_axis_solid_t *solid)
{
    if (!p || !solid || !lower || !upper || axis > 2 || !(p->fit > 0)) return false;
    axis_geometry_t geometry;
    if (!axis_camera_geometry(p, lower, upper, extend, &geometry)) return false;
    double *a = geometry.lower, *b = geometry.upper, *delta = geometry.direction;
    double seed[] = {fabs(delta[0]) < 0.8, fabs(delta[0]) >= 0.8, 0}, u[3], v[3];
    cross(delta, seed, u);
    double norm = hypot(hypot(u[0], u[1]), u[2]);
    for (unsigned i = 0; i < 3; ++i) u[i] /= norm;
    cross(delta, u, v);
    double pixel = geometry.pixel;
    double small[PLOT_AXIS_SIDES][3], large[PLOT_AXIS_SIDES][3], bottom[PLOT_AXIS_SIDES][3];
    double tip[3], name[3], rear[3], base[3];
    for (unsigned i = 0; i < 3; ++i) {
        tip[i] = b[i] + delta[i] * pixel * 27;
        name[i] = b[i] + delta[i] * pixel * 34;
        base[i] = b[i] + delta[i] * pixel * 13;
        rear[i] = -delta[i];
    }
    plot_axis_solid_t out = {0};
    double projected_a[3], projected_b[3], projected_tip[3], projected_name[3];
    plot_projection_point(p, a, projected_a); plot_projection_point(p, b, projected_b);
    plot_projection_point(p, tip, projected_tip); plot_projection_point(p, name, projected_name);
    if (!isfinite(projected_tip[0]) || !isfinite(projected_tip[1]) ||
        !isfinite(projected_name[0]) || !isfinite(projected_name[1])) return false;
    if (!plot_axis_arrow(projected_a, projected_b, &out.arrow)) return false;
    out.start[0] = projected_a[0]; out.start[1] = projected_a[1];
    out.arrow.tip[0] = projected_tip[0]; out.arrow.tip[1] = projected_tip[1];
    out.arrow.name[0] = projected_name[0]; out.arrow.name[1] = projected_name[1];
    out.name_scale = plot_projection_size(p, b[2]);
    double wing_low = INFINITY, wing_high = -INFINITY;
    for (unsigned side = 0; side < PLOT_AXIS_SIDES; ++side) {
        double angle = 6.283185307179586 * side / PLOT_AXIS_SIDES;
        for (unsigned i = 0; i < 3; ++i) {
            double radial = cos(angle) * u[i] + sin(angle) * v[i];
            bottom[side][i] = a[i] + radial * pixel * 1.4;
            small[side][i] = base[i] + radial * pixel * 1.4;
            large[side][i] = base[i] + radial * pixel * 5.2;
        }
        if (p->distance && (p->distance + bottom[side][2] <= 0 ||
            p->distance + small[side][2] <= 0 || p->distance + large[side][2] <= 0)) return false;
        double screen[3]; plot_projection_point(p, large[side], screen);
        double wing = -out.arrow.direction[1] * screen[0] + out.arrow.direction[0] * screen[1];
        if (wing < wing_low) { out.arrow.wing[0][0] = screen[0]; out.arrow.wing[0][1] = screen[1]; }
        if (wing > wing_high) { out.arrow.wing[1][0] = screen[0]; out.arrow.wing[1][1] = screen[1]; }
        if (wing < wing_low) wing_low = wing;
        if (wing > wing_high) wing_high = wing;
    }
    face(p, &out, axis, false, bottom, PLOT_AXIS_SIDES, rear);
    face(p, &out, axis, true, large, PLOT_AXIS_SIDES, rear);
    for (unsigned side = 0; side < PLOT_AXIS_SIDES; ++side) {
        unsigned next = (side + 1) % PLOT_AXIS_SIDES;
        double cylinder[4][3], cone[3][3], radial[3], slope[3];
        double angle = 6.283185307179586 * (side + 0.5) / PLOT_AXIS_SIDES;
        for (unsigned i = 0; i < 3; ++i) {
            cylinder[0][i] = bottom[side][i]; cylinder[1][i] = bottom[next][i];
            cylinder[2][i] = small[next][i]; cylinder[3][i] = small[side][i];
            cone[0][i] = large[side][i]; cone[1][i] = large[next][i]; cone[2][i] = tip[i];
            radial[i] = cos(angle) * u[i] + sin(angle) * v[i];
            slope[i] = 14 * radial[i] + 5.2 * delta[i];
        }
        face(p, &out, axis, false, cylinder, 4, radial);
        face(p, &out, axis, true, cone, 3, slope);
    }
    *solid = out;
    return true;
}

/* Positive camera depth is farther from the eye. */
int plot_axis_face_order(const void *first, const void *second)
{
    const plot_axis_face_t *a = first, *b = second;
    return a->depth > b->depth ? -1 : a->depth < b->depth;
}

/* Bound a camera-space circular section without generating or shading its sixteen-sided mesh. */
static bool axis_ring_bounds(const plot_projection_t *p, const double centre[3], const double normal[3],
                              double radius, double low[2], double high[2])
{
    double depth_radius = radius * sqrt(fmax(0, 1 - normal[2] * normal[2]));
    double near = p->distance + centre[2] - depth_radius, far = p->distance + centre[2] + depth_radius;
    if (p->distance && !(near > 0)) return false;
    for (unsigned i = 0; i < 2; ++i) {
        double extent = radius * sqrt(fmax(0, 1 - normal[i] * normal[i]));
        for (unsigned side = 0; side < 2; ++side) {
            double value = centre[i] + (side ? extent : -extent);
            for (unsigned depth = 0; depth < (p->distance ? 2u : 1u); ++depth) {
                double projected = p->centre[i] + p->fit * value * (p->distance ? p->distance / (depth ? far : near) : 1);
                if (!isfinite(projected)) return false;
                low[i] = fmin(low[i], projected); high[i] = fmax(high[i], projected);
            }
        }
    }
    return true;
}

/* Reserve actual projected arrow and name extents, rather than shrinking every view by a fixed allowance. */
bool plot_projection_fit_axes(plot_projection_t *projection, double width, double height,
                              const plot_axis_frame_t *frame, const double origin[3], const double name_widths[3])
{
    if (!projection || !frame || !name_widths || !isfinite(width) || !isfinite(height) ||
        !(width > 80) || !(height > 120)) return false;
    for (unsigned axis = 0; axis < 3; ++axis)
        if (!isfinite(name_widths[axis]) || name_widths[axis] < 0 ||
            (origin && !isfinite(origin[axis]))) return false;
    plot_projection_t fitted = *projection;
    double heading = fmin(48, fmax(0, height - 240) / 4);
    double centre[] = {width / 2, (height + heading) / 2};
    double minimum[] = {16, 16 + heading}, maximum[] = {width - 16, height - 16};
    for (unsigned attempt = 0; attempt < 8; ++attempt) {
        double low[] = {centre[0], centre[1]}, high[] = {centre[0], centre[1]};
        for (unsigned axis = 0; axis < 3; ++axis) {
            double lower[3], upper[3];
            bool intersects = true;
            for (unsigned i = 0; i < 3; ++i) {
                lower[i] = upper[i] = origin ? origin[i] : !!(frame->corner[axis] & (1u << i));
                if (origin && i != axis && (origin[i] < 0 || origin[i] > 1)) intersects = false;
            }
            if (!intersects) continue;
            lower[axis] = 0; upper[axis] = 1;
            axis_geometry_t geometry;
            if (!axis_camera_geometry(&fitted, lower, upper, origin != NULL, &geometry)) continue;
            double base[3], tip[3], name[3], start_screen[3], end_screen[3], tip_screen[3], name_screen[3];
            for (unsigned i = 0; i < 3; ++i) {
                base[i] = geometry.upper[i] + geometry.direction[i] * geometry.pixel * 13;
                tip[i] = geometry.upper[i] + geometry.direction[i] * geometry.pixel * 27;
                name[i] = geometry.upper[i] + geometry.direction[i] * geometry.pixel * 34;
            }
            if (!axis_ring_bounds(&fitted, geometry.lower, geometry.direction, geometry.pixel * 1.4, low, high) ||
                !axis_ring_bounds(&fitted, base, geometry.direction, geometry.pixel * 5.2, low, high)) return false;
            plot_projection_point(&fitted, geometry.lower, start_screen);
            plot_projection_point(&fitted, geometry.upper, end_screen);
            plot_projection_point(&fitted, tip, tip_screen);
            plot_projection_point(&fitted, name, name_screen);
            plot_axis_arrow_t arrow;
            if (!plot_axis_arrow(start_screen, end_screen, &arrow)) continue;
            for (unsigned i = 0; i < 2; ++i) {
                if (!isfinite(tip_screen[i]) || !isfinite(name_screen[i])) return false;
                low[i] = fmin(low[i], tip_screen[i]); high[i] = fmax(high[i], tip_screen[i]);
            }
            if (!name_widths[axis]) continue;
            double scale = plot_projection_size(&fitted, geometry.upper[2]);
            double font = 14 * scale, name_width = name_widths[axis] * scale, dx = arrow.direction[0];
            double x = name_screen[0] + frame->normal[axis][0] * 10 * scale;
            double y = name_screen[1] + frame->normal[axis][1] * 10 * scale + font * 0.3;
            double left = x - (dx < -0.1 ? name_width : dx > 0.1 ? 0 : name_width / 2);
            low[0] = fmin(low[0], left); high[0] = fmax(high[0], left + name_width);
            low[1] = fmin(low[1], y - font); high[1] = fmax(high[1], y + font * 0.3);
        }
        double ratio = 1;
        for (unsigned i = 0; i < 2; ++i) {
            if (low[i] < minimum[i])
                ratio = fmin(ratio, (centre[i] - minimum[i]) / (centre[i] - low[i]));
            if (high[i] > maximum[i])
                ratio = fmin(ratio, (maximum[i] - centre[i]) / (high[i] - centre[i]));
        }
        if (ratio >= 1) { *projection = fitted; return true; }
        if (!(ratio > 0) || !isfinite(ratio)) return false;
        /* A small convergence allowance handles the arrow's fixed-pixel size under perspective. */
        ratio *= 0.995;
        fitted.fit *= ratio;
        for (unsigned i = 0; i < 2; ++i)
            fitted.centre[i] = centre[i] + (fitted.centre[i] - centre[i]) * ratio;
    }
    return false;
}
