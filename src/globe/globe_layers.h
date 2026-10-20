/**
 * @file globe_layers.h
 * @brief Private shoreline and river engines owned by the globe controller.
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

/** @brief Private shoreline reader. */
typedef struct _shoreline_t shoreline_t;
/** @brief Open a shoreline reader on a borrowed connection. */
shoreline_t *shoreline_open(sqlite_t *db);
/** @brief Release a shoreline reader. */
void shoreline_close(shoreline_t *shoreline);
/** @brief Classify an exact coordinate, retaining coastal uncertainty. */
globe_surface_t shoreline_hit_test(shoreline_t *shoreline, double latitude, double longitude);
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
/** @brief Transactionally import legacy river geometry. */
bool rivers_import(sqlite_t *db, file_t *source);
#endif
