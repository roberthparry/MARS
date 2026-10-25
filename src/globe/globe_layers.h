/**
 * @file globe_layers.h
 * @brief Private shoreline, country and river engines owned by the globe controller.
 *
 * Only globe implementation files use these layer interfaces. Applications include globe-t.h.
 * Each layer borrows its database and requires serialised access.
 */
#ifndef MARS_GLOBE_LAYERS_H
#define MARS_GLOBE_LAYERS_H
#if !defined(MARS_GLOBE_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_GLOBE_INTERNAL_ACCESS."
#endif

#include "globe-t.h"

/** @brief Open an existing configured geometry store; caller owns the connection, or NULL on failure. */
sqlite_t *globe_configured_database(void);

/** @brief Read one bounded texture identity from the borrowed geometry store, without loading its JPEG. */
bool textures_metadata(sqlite_t *db, unsigned level, unsigned column, unsigned row, globe_texture_info_t *info);

/** @brief Return owned checksum-verified JPEG bytes matching the caller's metadata, or NULL. */
array_t *textures_jpeg(sqlite_t *db, unsigned level, unsigned column, unsigned row, const globe_texture_info_t *expected);

/** @brief Resolve a country polygon through bounded, indexed ray intersections; caller owns the returned name. */
string_t *countries_at(sqlite_t *db, double latitude, double longitude);

/** @brief Resolve an owned country name or ISO code, without inferring codes for unrecognised territories. */
string_t *countries_lookup(sqlite_t *db, double latitude, double longitude, bool code);

/** @brief Return an owned cultural display code from the independent indexed region layer, or NULL. */
string_t *regions_lookup(sqlite_t *db, double latitude, double longitude);

/** @brief Stream bounded indexed shared borders, preserving the detailed shoreline independently. */
bool countries_borders(sqlite_t *db, double west, double south, double east, double north,
                        globe_border_segment_fn visitor, void *context);

/** @brief Private shoreline reader. */
typedef struct _shoreline_t shoreline_t;

/** @brief Resolve an owned country name/code, reconciling small unambiguous outline gaps only on confirmed land. */
string_t *countries_resolve(sqlite_t *db, shoreline_t *shoreline, double latitude, double longitude, bool code);

/** @brief Open a shoreline reader on a borrowed connection. */
shoreline_t *shoreline_open(sqlite_t *db);
/** @brief Release a shoreline reader. */
void shoreline_close(shoreline_t *shoreline);
/** @brief Classify an exact coordinate, retaining coastal uncertainty. */
globe_surface_t shoreline_hit_test(shoreline_t *shoreline, double latitude, double longitude);

/** @brief Classify using a finite proximity radius in metres from zero to 20,000, or return unknown on failure. */
globe_surface_t shoreline_hit_test_radius(shoreline_t *shoreline, double latitude, double longitude, double radius);

/** @brief Return a static geographic water name after exact containment, or NULL for unrecognised/uncertain points. */
const char *shoreline_water_name(shoreline_t *shoreline, double latitude, double longitude);
/** @brief Prepare shared geometry. */
bool shoreline_prepare(shoreline_t *shoreline);
/** @brief Stream clipped shoreline paths. */
bool shoreline_paths(shoreline_t *shoreline, double west, double south, double east, double north,
                     double tolerance, globe_path_fn visit, void *context);
/** @brief Transactionally import shoreline geometry. */
bool shoreline_import(sqlite_t *db, file_t *source);

/** @brief Private river reader. */
typedef struct _rivers_t rivers_t;
/** @brief Private river visitor. */
typedef globe_river_segment_fn rivers_segment_fn;
/** @brief Open a river reader. */
rivers_t *rivers_open(sqlite_t *db);
/** @brief Release a river reader. */
void rivers_close(rivers_t *rivers);
/** @brief Stream clipped river segments. */
bool rivers_segments(rivers_t *rivers, double west, double south, double east, double north,
                     rivers_segment_fn visit, void *context);
/** @brief Stream display segments with bounded deviation and fixed shared-node anchors. */
bool rivers_display_segments(rivers_t *rivers, double west, double south, double east, double north,
                             double tolerance, rivers_segment_fn visit, void *context);
/** @brief Transactionally import legacy river geometry. */
bool rivers_import(sqlite_t *db, file_t *source);
#endif
