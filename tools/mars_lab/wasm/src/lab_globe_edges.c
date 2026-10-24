/**
 * @file lab_globe_edges.c
 * @brief Indexed common partitions for coincident geographic clipping edges.
 *
 * A clipped concave ring can retrace part of a longer closing edge. Projecting
 * those spans independently makes different curved approximations and a false
 * sliver of fill. Sorted endpoint indices split every overlapping edge into the
 * same intervals in either direction, without scanning unrelated ring vertices.
 * The shoreline owner constructs these indices once per admitted region.
 */
#include <stdlib.h>

#include "array-t.h"
#define MARS_LAB_GLOBE_EDGES_INTERNAL_ACCESS
#include "lab_globe_edges.h"

typedef struct _edge_knot_t { double fixed, variable; } edge_knot_t;
struct _lab_globe_edges_t { array_t *axes[2]; };

static int edge_compare(const void *left, const void *right)
{
    const edge_knot_t *a = left, *b = right;
    if (a->fixed != b->fixed) return (a->fixed > b->fixed) - (a->fixed < b->fixed);
    return (a->variable > b->variable) - (a->variable < b->variable);
}

/* Allocate the two independent coordinate-order indices. */
lab_globe_edges_t *lab_globe_edges_new(void)
{
    lab_globe_edges_t *edges = calloc(1, sizeof *edges);
    if (!edges) return NULL;
    for (unsigned i = 0; i < 2; ++i) edges->axes[i] = array_create(sizeof(edge_knot_t), NULL, NULL);
    if (!edges->axes[0] || !edges->axes[1]) { lab_globe_edges_free(edges); return NULL; }
    return edges;
}

/* Release region-owned endpoint storage. */
void lab_globe_edges_free(lab_globe_edges_t *edges)
{
    if (!edges) return;
    for (unsigned i = 0; i < 2; ++i) array_destroy(edges->axes[i]);
    free(edges);
}

/* Only equal-coordinate edges can contain an overlapping clipping join. */
bool lab_globe_edges_add(lab_globe_edges_t *edges, lab_globe_vertex_t a, lab_globe_vertex_t b)
{
    if (a.lon == b.lon && a.lat == b.lat) return true;
    if (a.lon != b.lon && a.lat != b.lat) return true;
    unsigned axis = a.lon != b.lon;
    edge_knot_t knots[2] = {{axis ? a.lat : a.lon, axis ? a.lon : a.lat},
                            {axis ? b.lat : b.lon, axis ? b.lon : b.lat}};
    return array_size(edges->axes[axis]) <= 1000000 - 2 && array_append_carray(edges->axes[axis], knots, 2);
}

/* Sorting is paid once on admission, not on each camera movement. */
void lab_globe_edges_finish(lab_globe_edges_t *edges)
{
    for (unsigned i = 0; i < 2; ++i) array_sort(edges->axes[i], edge_compare);
}

static size_t edge_bound(const array_t *knots, edge_knot_t key, bool upper)
{
    size_t lo = 0, hi = array_size(knots);
    while (lo < hi) {
        size_t middle = lo + (hi - lo) / 2;
        int order = edge_compare(array_get(knots, middle), &key);
        if (order < 0 || (upper && !order)) lo = middle + 1;
        else hi = middle;
    }
    return lo;
}

/* Restrict traversal to knots strictly inside this directed edge. */
void lab_globe_edge_begin(const lab_globe_edges_t *edges, lab_globe_vertex_t a, lab_globe_vertex_t b,
                          lab_globe_edge_cursor_t *cursor)
{
    *cursor = (lab_globe_edge_cursor_t){.edges = edges};
    if ((a.lon != b.lon && a.lat != b.lat) || (a.lon == b.lon && a.lat == b.lat)) return;
    unsigned axis = a.lon != b.lon;
    double start = axis ? a.lon : a.lat, end = axis ? b.lon : b.lat, fixed = axis ? a.lat : a.lon;
    bool reverse = start > end;
    size_t first = edge_bound(edges->axes[axis], (edge_knot_t){fixed, reverse ? end : start}, true);
    size_t last = edge_bound(edges->axes[axis], (edge_knot_t){fixed, reverse ? start : end}, false);
    *cursor = (lab_globe_edge_cursor_t){edges, reverse ? last : first, reverse ? first : last, axis, reverse, start};
}

/* Duplicated endpoints are harmless in storage and emitted only once per edge. */
bool lab_globe_edge_next(lab_globe_edge_cursor_t *cursor, lab_globe_vertex_t *point)
{
    while (cursor->at != cursor->end) {
        size_t index = cursor->reverse ? --cursor->at : cursor->at++;
        const edge_knot_t *knot = array_get(cursor->edges->axes[cursor->axis], index);
        if (knot->variable == cursor->previous) continue;
        cursor->previous = knot->variable;
        *point = (lab_globe_vertex_t){cursor->axis ? knot->variable : knot->fixed,
                                      cursor->axis ? knot->fixed : knot->variable};
        return true;
    }
    return false;
}
