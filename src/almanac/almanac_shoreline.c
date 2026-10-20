/**
 * @file almanac_shoreline.c
 * @brief Connect the almanac catalogue to the independent shoreline reader.
 *
 * Lazily owns one opaque shoreline reader per astronomical engine. Geography, compression and spatial indexing
 * belong to the globe module. Geometry comes exclusively from its independent configured database.
 */
#define MARS_ALMANAC_INTERNAL_ACCESS
#include "almanac_engine_internal.h"

/* Prepare shared immutable geometry from independent globe storage. */
bool almanac_surface_prepare(almanac_t *almanac)
{
    if (!almanac) return false;
    if (!almanac->shoreline) almanac->shoreline = globe_open_configured(NULL);
    return globe_surface_prepare(almanac->shoreline);
}

/* Classify through the independently owned globe connection. */
globe_surface_t almanac_surface_at(almanac_t *almanac, double latitude_degrees, double longitude_degrees)
{
    if (!almanac) return GLOBE_UNKNOWN;
    if (!almanac->shoreline)
        almanac->shoreline = globe_open_configured(NULL);
    return globe_hit_test(almanac->shoreline, latitude_degrees, longitude_degrees);
}

/* Supply bounded display imagery without exposing the engine's database. */
bool almanac_surface_paths(almanac_t *almanac, double west, double south, double east, double north,
                          double tolerance, globe_path_fn visit, void *context)
{
    if (!almanac) return false;
    if (!almanac->shoreline) almanac->shoreline = globe_open_configured(NULL);
    return globe_surface_paths(almanac->shoreline, west, south, east, north, tolerance, visit, context);
}
