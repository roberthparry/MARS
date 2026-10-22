/**
 * @file shoreline_internal.h
 * @brief Private reader ownership and on-disk codecs shared by shoreline queries and import.
 *
 * The reader borrows its connection and owns its prepared hit-test and any anonymous-database cache reference.
 * File-backed geometry is shared independently of connections. Codecs describe coordinate chunks, not BLOB pages.
 * Only shoreline implementation
 * units may include this header; applications use the opaque reader in globe-t.h.
 */
#ifndef MARS_SHORELINE_INTERNAL_H
#define MARS_SHORELINE_INTERNAL_H

#include <stdint.h>

#if !defined(MARS_SHORELINE_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_SHORELINE_INTERNAL_ACCESS."
#endif

/** @brief Exact stored coordinate; boundary marks the preceding edge as physical rather than a polar closure. */
typedef struct { int32_t x, y; } coast_vertex_t;

/** @brief Immutable polygon metadata and four display-only index ranges. */
typedef struct {
    uint32_t first, count, lod_first[4], lod_count[4];
    int32_t west, south, east, north;
    unsigned level;
    bool antarctic;
} coast_ring_t;

/** @brief Immutable cached geometry borrowed for the duration of a retained reference. */
typedef struct _coast_geometry_t {
    coast_vertex_t *vertices;
    unsigned char *boundary;
    coast_ring_t *rings;
    uint32_t *lod;
    struct _coast_node_t *nodes;
    size_t vertex_count, ring_count, lod_count, node_count;
    unsigned references;
    string_t *key;
} coast_geometry_t;

/**
 * @brief Visit one spatially selected ring.
 * @param geometry Borrowed retained geometry containing the ring's coordinate and display arrays.
 * @param ring Borrowed immutable ring metadata.
 * @param context Borrowed caller state.
 * @return True to continue traversal, false to stop.
 */
typedef bool (*coast_ring_fn)(const coast_geometry_t *geometry, const coast_ring_t *ring, void *context);

/**
 * @brief Retain current geometry, loading it only when requested.
 * @param shoreline Borrowed serialised reader.
 * @param load Permit loading the complete dataset when not already cached.
 * @return Retained immutable geometry, or NULL on failure or a cold non-loading lookup.
 */
coast_geometry_t *coast_geometry_acquire(shoreline_t *shoreline, bool load);

/**
 * @brief Release a retained geometry reference.
 * @param geometry Owned reference, or NULL.
 */
void coast_geometry_release(coast_geometry_t *geometry);

/**
 * @brief Visit spatially intersecting rings through a bounded-leaf bounding-volume tree.
 * @param geometry Borrowed immutable geometry.
 * @param bounds West, south, east and north in integer microdegree units represented as doubles.
 * @param span Minimum display extent; zero retains all intersecting rings.
 * @param visit Required synchronous visitor.
 * @param context Borrowed visitor state.
 * @return True on completion, false when the visitor stops.
 */
bool coast_geometry_visit(const coast_geometry_t *geometry, const double bounds[4], double span,
                          coast_ring_fn visit, void *context);

/**
 * @brief Choose a cached display resolution, retaining original points at close scales.
 * @param tolerance Requested display tolerance in degrees.
 * @return Index 0..3, or -1 for full-resolution vertices.
 */
int coast_geometry_lod(double tolerance);

/** @brief Reader-owned query and borrowed catalogue connection. */
typedef struct _shoreline_t {
    sqlite_stmt_t *query;
    sqlite_t *db;
    coast_geometry_t *local_geometry;
} shoreline_t;

/** @brief Stable stored encoding for an independently readable coordinate chunk. */
typedef enum _shoreline_codec_t {
    SHORELINE_CODEC_RAW = 0, /**< Big-endian signed 32-bit coordinate pairs. */
    SHORELINE_CODEC_ZSTD     /**< One complete Zstandard frame of those coordinate pairs. */
} shoreline_codec_t;

#endif
