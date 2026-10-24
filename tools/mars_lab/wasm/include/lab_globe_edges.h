/**
 * @file lab_globe_edges.h
 * @brief Private geographic edge partitioning for the native shoreline projector.
 *
 * Indexes endpoints of axis-aligned edges so partially overlapping clipping joins
 * are projected through identical intervals. This is display geometry only;
 * callers outside the shoreline implementation should use its public host ABI.
 */
#if !defined(MARS_LAB_GLOBE_EDGES_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_LAB_GLOBE_EDGES_INTERNAL_ACCESS."
#endif
#ifndef LAB_GLOBE_EDGES_H
#define LAB_GLOBE_EDGES_H

#include <stdbool.h>
#include <stddef.h>

/** @brief Geographic coordinates retained in native display storage. */
typedef struct _lab_globe_vertex_t { double lon, lat; } lab_globe_vertex_t;

/** @brief Owned, sorted endpoint indices for horizontal and vertical joins. */
typedef struct _lab_globe_edges_t lab_globe_edges_t;

/** @brief Borrowed traversal state, valid until its endpoint index is freed. */
typedef struct _lab_globe_edge_cursor_t {
    const lab_globe_edges_t *edges;
    size_t at, end;
    unsigned axis;
    bool reverse;
    double previous;
} lab_globe_edge_cursor_t;

/** @brief Allocate empty endpoint indices; return NULL on allocation failure. */
lab_globe_edges_t *lab_globe_edges_new(void);

/** @brief Release both endpoint indices; NULL is accepted. */
void lab_globe_edges_free(lab_globe_edges_t *edges);

/** @brief Admit an axis-aligned edge's endpoints; other edges need no index entries. */
bool lab_globe_edges_add(lab_globe_edges_t *edges, lab_globe_vertex_t a, lab_globe_vertex_t b);

/** @brief Sort completed indices before starting any traversal. */
void lab_globe_edges_finish(lab_globe_edges_t *edges);

/** @brief Find strictly interior indexed endpoints in directed edge order by binary search. */
void lab_globe_edge_begin(const lab_globe_edges_t *edges, lab_globe_vertex_t a, lab_globe_vertex_t b,
                          lab_globe_edge_cursor_t *cursor);

/** @brief Return the next distinct interior endpoint, or false when traversal is complete. */
bool lab_globe_edge_next(lab_globe_edge_cursor_t *cursor, lab_globe_vertex_t *point);

#endif
