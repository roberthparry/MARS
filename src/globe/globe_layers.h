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

/** @brief Read one bounded texture identity from the borrowed geometry store, without loading its PNG. */
bool textures_metadata(sqlite_t *db, unsigned level, unsigned column, unsigned row, globe_texture_info_t *info);

/** @brief Enumerate the bounded optional regional photographic catalogue. */
bool textures_regions(sqlite_t *db, const double bounds[4], globe_texture_region_fn callback, void *context);

/** @brief Return owned checksum-verified PNG bytes matching the caller's metadata, or NULL. */
array_t *textures_png(sqlite_t *db, unsigned level, unsigned column, unsigned row, const globe_texture_info_t *expected);

/**
 * @brief Check a committed source-pyramid completion marker without reading image payloads.
 * @param db Borrowed geometry connection; calls must be serialised.
 * @param sha256 Borrowed source content digest.
 * @param width Expected square source width in pixels.
 * @return True for a matching complete version-one pyramid; false for absent or invalid data.
 */
bool textures_pyramid_ready(sqlite_t *db, const char *sha256, unsigned width);

/**
 * @brief Read and verify one indexed block through the globe controller's borrowed connection.
 * @param db Borrowed geometry database; serialise calls.
 * @param sha256 Borrowed source digest, valid during the call.
 * @param tier Decoded square source width.
 * @param row Zero-based block row.
 * @param column Zero-based block column.
 * @param info Required metadata output, cleared on failure.
 * @return Owned PNG bytes released with array_destroy, or NULL on missing, corrupt or invalid data.
 */
array_t *textures_block(sqlite_t *db, const char *sha256, unsigned tier, unsigned row,
                        unsigned column, globe_texture_info_t *info);

/**
 * @brief Read the optional stable photographic catalogue identity.
 * @param db Borrowed geometry database; serialise calls.
 * @return Owned identity released with string_free, or NULL if unavailable or unreadable.
 */
string_t *textures_identity(sqlite_t *db);

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
/** @brief Read optional indexed width metadata; absent metadata gives zero metres. */
bool rivers_width_metres(rivers_t *rivers, int64_t id, double *width);
/** @brief Stream bounded, spatially indexed water polygons with their holes intact. */
bool rivers_banks(rivers_t *rivers, double west, double south, double east, double north,
                   globe_river_bank_fn visit, void *context);

/** @brief Check indexed complete water coverage without inferring coverage from polygon bounds. */
bool rivers_water_complete(rivers_t *rivers, double west, double south, double east, double north);

/**
 * @brief Stream spatially indexed survey rectangles clipped into the requested longitude frame.
 * @param[in] rivers Borrowed river reader; serialise calls and do not re-enter it through the visitor.
 * @param[in] west Western longitude in [-360,180] degrees.
 * @param[in] south Southern latitude in [-90,90] degrees.
 * @param[in] east Eastern longitude greater than west by at most 360 degrees.
 * @param[in] north Northern latitude greater than south and at most 90 degrees; all bounds must be finite.
 * @param[in] visit Required synchronous visitor; its bounds are borrowed only for the callback duration.
 * @param[in,out] context Optional caller-owned state, neither consumed nor retained.
 * @return True on completion or absent optional coverage; false on invalid or corrupt input, query failure,
 * cancellation or more than 4096 intersections. Discard partial output after false.
 * @note Only the globe controller calls this private layer interface. No argument ownership transfers.
 */
bool rivers_water_coverage(rivers_t *rivers, double west, double south, double east, double north,
                           globe_water_coverage_fn visit, void *context);
/** @brief Stream clipped river segments. */
bool rivers_segments(rivers_t *rivers, double west, double south, double east, double north,
                     rivers_segment_fn visit, void *context);
/** @brief Stream display segments with bounded deviation and fixed shared-node anchors. */
bool rivers_display_segments(rivers_t *rivers, double west, double south, double east, double north,
                             double tolerance, rivers_segment_fn visit, void *context);
/** @brief Transactionally import legacy river geometry. */
bool rivers_import(sqlite_t *db, file_t *source);
#endif
