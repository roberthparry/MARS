/**
 * @file plot_occlusion.c
 * @brief Spatially indexed, perspective-correct clipping of visible plot-axis face fragments.
 *
 * A bounding-volume hierarchy avoids comparing each axis face with the entire
 * surface. Affine reciprocal-depth planes select nearer triangle portions;
 * convex subtraction preserves holes and handles intersecting surfaces without
 * a depth-order heuristic. Work buffers are retained between serial queries.
 */
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define MARS_PLOT_INTERNAL_ACCESS
#include "plot_occlusion.h"

typedef struct {
    double point[3][3], bounds[4];
} triangle_t;

typedef struct {
    double bounds[4];
    size_t begin, count, left, right;
} node_t;

typedef struct {
    plot_occlusion_polygon_t *items;
    size_t count, capacity;
} pieces_t;

struct _plot_occlusion_t {
    triangle_t *triangles;
    size_t count, capacity, nodes_used;
    node_t *nodes;
    pieces_t current, next;
    double plane[3], bounds[4];
    bool built;
};

/* Own all triangle and clipping storage in one explicitly released context. */
plot_occlusion_t *plot_occlusion_new(void)
{
    return calloc(1, sizeof(plot_occlusion_t));
}

static void bounds(const double points[][3], unsigned count, double out[4])
{
    out[0] = out[2] = INFINITY;
    out[1] = out[3] = -INFINITY;
    for (unsigned i = 0; i < count; ++i)
        for (unsigned axis = 0; axis < 2; ++axis) {
            out[axis * 2] = fmin(out[axis * 2], points[i][axis]);
            out[axis * 2 + 1] = fmax(out[axis * 2 + 1], points[i][axis]);
        }
}

/* Copy finite screen triangles before building the immutable hierarchy. */
bool plot_occlusion_add(plot_occlusion_t *index, const double points[3][3])
{
    if (!index || !points || index->built) return false;
    for (unsigned i = 0; i < 3; ++i)
        for (unsigned j = 0; j < 3; ++j)
            if (!isfinite(points[i][j])) return false;
    if (index->count == index->capacity) {
        size_t capacity = index->capacity ? index->capacity * 2 : 256;
        if (capacity < index->capacity || capacity > (size_t)-1 / sizeof(triangle_t)) return false;
        triangle_t *grown = realloc(index->triangles, capacity * sizeof(*grown));
        if (!grown) return false;
        index->triangles = grown;
        index->capacity = capacity;
    }
    triangle_t *triangle = &index->triangles[index->count++];
    memcpy(triangle->point, points, sizeof triangle->point);
    bounds(points, 3, triangle->bounds);
    return true;
}

static size_t build(plot_occlusion_t *index, size_t begin, size_t count, unsigned depth)
{
    size_t id = index->nodes_used++;
    node_t *node = &index->nodes[id];
    node->begin = begin; node->count = count;
    for (unsigned i = 0; i < 4; ++i) node->bounds[i] = i % 2 ? -INFINITY : INFINITY;
    for (size_t j = begin; j < begin + count; ++j)
        for (unsigned i = 0; i < 4; ++i)
            node->bounds[i] = i % 2 ? fmax(node->bounds[i], index->triangles[j].bounds[i]) :
                                     fmin(node->bounds[i], index->triangles[j].bounds[i]);
    if (count <= 8) return id;
    unsigned axis = node->bounds[3] - node->bounds[2] > node->bounds[1] - node->bounds[0] ? 2 : 0;
    double centre = (node->bounds[axis] + node->bounds[axis + 1]) / 2;
    size_t middle = begin;
    if (depth < 24)
        for (size_t j = begin; j < begin + count; ++j)
            if ((index->triangles[j].bounds[axis] + index->triangles[j].bounds[axis + 1]) / 2 < centre) {
                triangle_t swap = index->triangles[j];
                index->triangles[j] = index->triangles[middle];
                index->triangles[middle++] = swap;
            }
    if (middle == begin || middle == begin + count) middle = begin + count / 2;
    node->count = 0;
    node->left = build(index, begin, middle - begin, depth + 1);
    node->right = build(index, middle, begin + count - middle, depth + 1);
    return id;
}

/* Build bounded-depth spatial nodes once per projected surface. */
bool plot_occlusion_build(plot_occlusion_t *index)
{
    if (!index) return false;
    if (index->built) return true;
    if (index->count) {
        if (index->count > (size_t)-1 / (2 * sizeof(node_t))) return false;
        index->nodes = calloc(index->count * 2, sizeof(node_t));
        if (!index->nodes) return false;
        build(index, 0, index->count, 0);
    }
    index->built = true;
    return true;
}

static bool append(pieces_t *pieces, const plot_occlusion_polygon_t *polygon)
{
    if (polygon->count < 3) return true;
    double area = 0;
    for (unsigned i = 0; i < polygon->count; ++i) {
        const double *a = polygon->point[i], *b = polygon->point[(i + 1) % polygon->count];
        area += a[0] * b[1] - a[1] * b[0];
    }
    if (fabs(area) < 1e-10) return true;
    if (pieces->count == pieces->capacity) {
        size_t capacity = pieces->capacity ? pieces->capacity * 2 : 8;
        if (capacity > 4096) return false;
        plot_occlusion_polygon_t *grown = realloc(pieces->items, capacity * sizeof(*grown));
        if (!grown) return false;
        pieces->items = grown; pieces->capacity = capacity;
    }
    pieces->items[pieces->count++] = *polygon;
    return true;
}

static bool plane(const plot_occlusion_polygon_t *polygon, double out[3])
{
    const double *a = polygon->point[0];
    for (unsigned i = 1; i + 1 < polygon->count; ++i) {
        const double *b = polygon->point[i], *c = polygon->point[i + 1];
        double x = b[0] - a[0], y = b[1] - a[1], u = c[0] - a[0], v = c[1] - a[1];
        double determinant = x * v - y * u;
        if (fabs(determinant) < 1e-12) continue;
        out[0] = ((b[2] - a[2]) * v - (c[2] - a[2]) * y) / determinant;
        out[1] = (x * (c[2] - a[2]) - u * (b[2] - a[2])) / determinant;
        out[2] = a[2] - out[0] * a[0] - out[1] * a[1];
        return true;
    }
    return false;
}

static bool clip(const plot_occlusion_polygon_t *input, const double edge[3], bool inside,
                  plot_occlusion_polygon_t *out)
{
    out->count = 0;
    if (!input->count) return true;
    const double *previous = input->point[input->count - 1];
    double before = edge[0] * previous[0] + edge[1] * previous[1] + edge[2];
    if (!inside) before = -before;
    for (unsigned i = 0; i < input->count; ++i) {
        const double *point = input->point[i];
        double value = edge[0] * point[0] + edge[1] * point[1] + edge[2];
        if (!inside) value = -value;
        if ((value >= 0) != (before >= 0)) {
            if (out->count == PLOT_OCCLUSION_POINTS) return false;
            double fraction = before / (before - value);
            for (unsigned j = 0; j < 3; ++j)
                out->point[out->count][j] = previous[j] + fraction * (point[j] - previous[j]);
            ++out->count;
        }
        if (value >= 0) {
            if (out->count == PLOT_OCCLUSION_POINTS) return false;
            memcpy(out->point[out->count++], point, 3 * sizeof(double));
        }
        previous = point; before = value;
    }
    return true;
}

static bool overlaps(const double a[4], const double b[4])
{
    return a[0] <= b[1] && b[0] <= a[1] && a[2] <= b[3] && b[2] <= a[3];
}

static bool subtract(plot_occlusion_t *index, const triangle_t *triangle)
{
    if (!overlaps(index->bounds, triangle->bounds)) return true;
    plot_occlusion_polygon_t source = {.count = 3}, cover;
    memcpy(source.point, triangle->point, sizeof triangle->point);
    double near[3], difference[3];
    if (!plane(&source, near)) return true;
    for (unsigned i = 0; i < 3; ++i) difference[i] = near[i] - index->plane[i];
    double tolerance = fmax(fabs(source.point[0][2]), fmax(fabs(source.point[1][2]), fabs(source.point[2][2]))) * 1e-10;
    difference[2] -= tolerance;
    if (!clip(&source, difference, true, &cover)) return false;
    if (cover.count < 3) return true;
    double area = 0;
    for (unsigned i = 0; i < cover.count; ++i) {
        const double *a = cover.point[i], *b = cover.point[(i + 1) % cover.count];
        area += a[0] * b[1] - a[1] * b[0];
    }
    if (fabs(area) < 1e-10) return true;
    index->next.count = 0;
    for (size_t piece = 0; piece < index->current.count; ++piece) {
        plot_occlusion_polygon_t remaining = index->current.items[piece];
        double extent[4];
        bounds(remaining.point, remaining.count, extent);
        if (!overlaps(extent, triangle->bounds)) {
            if (!append(&index->next, &remaining)) return false;
            continue;
        }
        for (unsigned i = 0; i < cover.count && remaining.count >= 3; ++i) {
            const double *a = cover.point[i], *b = cover.point[(i + 1) % cover.count];
            double sign = area > 0 ? 1 : -1;
            double edge[] = {sign * (a[1] - b[1]), sign * (b[0] - a[0]),
                             sign * (b[1] * a[0] - b[0] * a[1])};
            plot_occlusion_polygon_t outside, inside;
            if (!clip(&remaining, edge, false, &outside) || !clip(&remaining, edge, true, &inside) ||
                !append(&index->next, &outside)) return false;
            remaining = inside;
        }
    }
    pieces_t swap = index->current;
    index->current = index->next; index->next = swap;
    return true;
}

static bool visit(plot_occlusion_t *index, size_t id)
{
    if (!index->current.count) return true;
    const node_t *node = &index->nodes[id];
    if (!overlaps(index->bounds, node->bounds)) return true;
    if (!node->count) return visit(index, node->left) && visit(index, node->right);
    for (size_t i = node->begin; i < node->begin + node->count && index->current.count; ++i)
        if (!subtract(index, &index->triangles[i])) return false;
    return true;
}

/* Keep visible fragments exact in screen space; callers paint covered sections with a light fade. */
bool plot_occlusion_visible(plot_occlusion_t *index, const plot_occlusion_polygon_t *polygon,
                            plot_occlusion_emit_t emit, void *context)
{
    if (!index || !index->built || !polygon || !emit || polygon->count < 3 ||
        polygon->count > PLOT_OCCLUSION_POINTS) return false;
    for (unsigned i = 0; i < polygon->count; ++i)
        for (unsigned j = 0; j < 3; ++j)
            if (!isfinite(polygon->point[i][j])) return false;
    if (!plane(polygon, index->plane)) return true;
    bounds(polygon->point, polygon->count, index->bounds);
    index->current.count = 0;
    if (!append(&index->current, polygon) || (index->count && !visit(index, 0))) return false;
    for (size_t i = 0; i < index->current.count; ++i)
        if (!emit(&index->current.items[i], context)) return false;
    return true;
}

/* Release every allocation owned by the display-only visibility index. */
void plot_occlusion_free(plot_occlusion_t *index)
{
    if (!index) return;
    free(index->triangles); free(index->nodes);
    free(index->current.items); free(index->next.items);
    free(index);
}
