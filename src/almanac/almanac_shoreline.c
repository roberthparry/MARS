/**
 * @file almanac_shoreline.c
 * @brief Connect the almanac catalogue to the independent shoreline reader.
 *
 * Lazily owns one opaque shoreline reader per astronomical engine. Geography, compression and spatial indexing
 * belong to the shoreline module; this adapter only supplies the existing encrypted database connection.
 */
#define MARS_ALMANAC_INTERNAL_ACCESS
#include "almanac_engine_internal.h"

/* Prepare shared immutable geometry while the caller still owns its database connection. */
bool almanac_surface_prepare(almanac_t *almanac)
{
    if (!almanac) return false;
    if (!almanac->shoreline) almanac->shoreline = shoreline_open(almanac->db);
    return shoreline_prepare(almanac->shoreline);
}

/* Reuse the shared catalogue connection without exposing it to callers. */
shoreline_surface_t almanac_surface_at(almanac_t *almanac, double latitude_degrees, double longitude_degrees)
{
    if (!almanac) return SHORELINE_UNKNOWN;
    if (!almanac->shoreline)
        almanac->shoreline = shoreline_open(almanac->db);
    return shoreline_hit_test(almanac->shoreline, latitude_degrees, longitude_degrees);
}

/* Supply bounded display imagery without exposing the engine's database. */
bool almanac_surface_paths(almanac_t *almanac, double west, double south, double east, double north,
                          double tolerance, shoreline_path_fn visit, void *context)
{
    if (!almanac) return false;
    if (!almanac->shoreline) almanac->shoreline = shoreline_open(almanac->db);
    return shoreline_paths(almanac->shoreline, west, south, east, north, tolerance, visit, context);
}
