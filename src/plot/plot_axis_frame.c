/**
 * @file plot_axis_frame.c
 * @brief Shared connected exterior scales and positive-direction arrows for three-dimensional plots.
 *
 * A fixed cube has only 48 paths using each coordinate direction once. Supporting
 * line checks retain at least two silhouette edges. Projected lengths and the
 * clearance between adjoining rays reject short, overlapping depth scales before
 * minimising intrusion and preferring the lower-left frame. Cardinal views retain
 * collinear edges when they continue away from their shared corner.
 * A caller-supplied previous frame remains in place while its scales stay exterior
 * and readable, preventing small camera movements from repeatedly exchanging edges.
 * Arrowheads extend beyond the upper coordinate endpoint, clear of shared joins.
 * Native SVG and WebAssembly use exactly the same geometry and no sampled values
 * or expressions are inspected. There are no allocations or mutable globals.
 */
#include <math.h>

#define MARS_PLOT_INTERNAL_ACCESS
#include "plot_axis_frame.h"

/* Keep increasing coordinate direction independent of the selected frame's traversal. */
bool plot_axis_arrow(const double lower[2], const double upper[2], plot_axis_arrow_t *arrow)
{
    if (!lower || !upper || !arrow)
        return false;
    for (unsigned i = 0; i < 2; ++i)
        if (!isfinite(lower[i]) || !isfinite(upper[i]))
            return false;
    double dx = upper[0] - lower[0], dy = upper[1] - lower[1], length = hypot(dx, dy);
    if (!isfinite(length) || length < 1e-7)
        return false;
    plot_axis_arrow_t chosen = {.direction = {dx / length, dy / length}};
    double normal[] = {-chosen.direction[1], chosen.direction[0]};
    for (unsigned i = 0; i < 2; ++i) {
        chosen.tip[i] = upper[i] + chosen.direction[i] * 22;
        chosen.wing[0][i] = upper[i] + chosen.direction[i] * 13 + normal[i] * 4;
        chosen.wing[1][i] = upper[i] + chosen.direction[i] * 13 - normal[i] * 4;
        chosen.name[i] = upper[i] + chosen.direction[i] * 25;
    }
    *arrow = chosen;
    return true;
}

typedef struct {
    double length, intrusion, normal[2];
} edge_t;

static edge_t projected_edge(const double corners[8][2], unsigned first, unsigned last, double tolerance)
{
    double dx = corners[last][0] - corners[first][0], dy = corners[last][1] - corners[first][1];
    double length = hypot(dx, dy);
    edge_t edge = {.length = length, .normal = {-1, 0}};
    if (length <= tolerance)
        return edge;
    double low = 0, high = 0;
    for (unsigned i = 0; i < 8; ++i) {
        double distance = (dx * (corners[i][1] - corners[first][1]) -
                           dy * (corners[i][0] - corners[first][0])) / length;
        low = fmin(low, distance);
        high = fmax(high, distance);
    }
    double sign = high >= -low ? 1 : -1;
    edge.normal[0] = sign * dy / length;
    edge.normal[1] = -sign * dx / length;
    edge.intrusion = low < -tolerance && high > tolerance ? fmin(-low, high) : 0;
    return edge;
}

/* Select one connected half of the projected box outline without moving any coordinate edge. */
bool plot_axis_frame(const double corners[8][2], const plot_axis_frame_t *previous, plot_axis_frame_t *frame)
{
    if (!corners || !frame)
        return false;
    double bounds[] = {INFINITY, -INFINITY, INFINITY, -INFINITY};
    for (unsigned i = 0; i < 8; ++i) {
        if (!isfinite(corners[i][0]) || !isfinite(corners[i][1]))
            return false;
        bounds[0] = fmin(bounds[0], corners[i][0]);
        bounds[1] = fmax(bounds[1], corners[i][0]);
        bounds[2] = fmin(bounds[2], corners[i][1]);
        bounds[3] = fmax(bounds[3], corners[i][1]);
    }
    double scale = fmax(bounds[1] - bounds[0], bounds[3] - bounds[2]);
    if (!(scale > 0))
        return false;
    double tolerance = scale * 1e-9, best = -INFINITY, least_intrusion = INFINITY, legibility = -1;
    double clearance = fmin(96, scale * 0.18);
    bool exterior = false;
    edge_t edges[3][4];
    for (unsigned axis = 0; axis < 3; ++axis) {
        /* Removing the varying coordinate bit packs its four parallel edges without empty slots. */
        unsigned mask = (1u << axis) - 1;
        for (unsigned slot = 0; slot < 4; ++slot) {
            unsigned first = (slot & mask) | ((slot & ~mask) << 1);
            edges[axis][slot] = projected_edge(corners, first, first | (1u << axis), tolerance);
        }
    }
    plot_axis_frame_t chosen = {0}, retained = {0};
    double retained_readable = -1, retained_intrusion = INFINITY;
    unsigned retained_supported = 0;
    /* Fixed topology enumeration, not a search over mesh points or a dispatch lookup. */
    for (unsigned start = 0; start < 8; ++start) {
        for (unsigned first = 0; first < 3; ++first) {
            for (unsigned second = 0; second < 3; ++second) {
                if (first == second)
                    continue;
                unsigned order[] = {first, second, 3 - first - second}, at = start;
                plot_axis_frame_t candidate = {0};
                double score = 0, intrusion = 0, readable = clearance, previous_direction[2] = {0}, previous_length = 0;
                unsigned supported = 0;
                for (unsigned step = 0; step < 3; ++step) {
                    unsigned axis = order[step], next = at ^ (1u << axis);
                    candidate.corner[axis] = at & ~(1u << axis);
                    unsigned mask = (1u << axis) - 1, corner = candidate.corner[axis];
                    unsigned slot = (corner & mask) | ((corner >> (axis + 1)) << axis);
                    const edge_t *edge = &edges[axis][slot];
                    intrusion += edge->intrusion;
                    supported += edge->intrusion <= tolerance;
                    for (unsigned i = 0; i < 2; ++i) candidate.normal[axis][i] = edge->normal[i];
                    readable = fmin(readable, edge->length);
                    double direction[] = {corners[next][0] - corners[at][0], corners[next][1] - corners[at][1]};
                    if (edge->length > tolerance) {
                        for (unsigned i = 0; i < 2; ++i) direction[i] /= edge->length;
                        if (step) {
                            /* Each scale needs perpendicular clearance, even when two rays point opposite ways. */
                            double separation = fabs(previous_direction[0] * direction[1] -
                                                     previous_direction[1] * direction[0]);
                            readable = fmin(readable, fmin(previous_length, edge->length) * separation);
                        }
                    }
                    for (unsigned i = 0; i < 2; ++i) previous_direction[i] = direction[i];
                    previous_length = edge->length;
                    double x = ((corners[at][0] + corners[next][0]) / 2 - bounds[0]) / scale;
                    double y = ((corners[at][1] + corners[next][1]) / 2 - bounds[2]) / scale;
                    score += y - (axis == 2 ? 1.5 : 0.35) * x;
                    at = next;
                }
                if (previous && candidate.corner[0] == previous->corner[0] &&
                    candidate.corner[1] == previous->corner[1] && candidate.corner[2] == previous->corner[2]) {
                    retained = candidate;
                    retained_readable = readable;
                    retained_intrusion = intrusion;
                    retained_supported = supported;
                }
                bool outside = supported >= 2, tied = fabs(readable - legibility) <= tolerance;
                if ((outside && !exterior) || (outside == exterior &&
                    (readable > legibility + tolerance || (tied &&
                     (intrusion < least_intrusion - tolerance ||
                      (fabs(intrusion - least_intrusion) <= tolerance && score > best)))))) {
                    exterior = outside;
                    legibility = readable;
                    least_intrusion = intrusion;
                    best = score;
                    chosen = candidate;
                }
            }
        }
    }
    if (!isfinite(best))
        return false;
    /* Only a substantial loss of legibility or silhouette support changes the reference edges. */
    bool keep = retained_supported >= 2 && retained_readable + tolerance >= legibility * 0.75 &&
                retained_intrusion <= least_intrusion + scale * 0.025;
    *frame = keep ? retained : chosen;
    return true;
}
