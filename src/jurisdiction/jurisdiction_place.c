/**
 * @file jurisdiction_place.c
 * @brief Resolve a rounded catalogue reference position through bounded SQLite queries.
 *
 * Visits one town using the numeric latitude index, falling back to the small representative-location
 * catalogue only when no town matches. This avoids constructing an application-wide town index for a single
 * location. Records borrow statement storage for the callback duration; the engine retains no result cache.
 */
#include <math.h>

#define MARS_JURISDICTION_INTERNAL_ACCESS
#include "jurisdiction_internal.h"

static bool jurisdiction_place_query(jurisdiction_t *engine, const char *sql, double latitude, double longitude,
                                     jurisdict_place_visit_fn visitor, void *context, bool *found)
{
    sqlite_stmt_t *query = sqlite_stmt_prepare(engine->db, sql);
    bool ok = query && sqlite_stmt_bind_double(query, 1, latitude - 0.0000005) &&
              sqlite_stmt_bind_double(query, 2, latitude + 0.0000005) &&
              sqlite_stmt_bind_double(query, 3, longitude - 0.0000005) &&
              sqlite_stmt_bind_double(query, 4, longitude + 0.0000005);
    sqlite_step_result_t step = ok ? sqlite_stmt_step(query) : SQLITE_STEP_ERROR;
    if (step == SQLITE_STEP_ROW) {
        jurisdict_place_t place = {
            .jurisdiction_code = sqlite_stmt_column_text(query, 0), .name = sqlite_stmt_column_text(query, 1),
            .latitude = sqlite_stmt_column_text(query, 2), .longitude = sqlite_stmt_column_text(query, 3),
            .elevation = sqlite_stmt_column_text(query, 4), .timezone = sqlite_stmt_column_text(query, 5),
            .is_default = sqlite_stmt_column_int(query, 6) != 0,
        };
        *found = true;
        visitor(&place, context);
    }
    sqlite_stmt_finalize(query);
    return ok && (step == SQLITE_STEP_ROW || step == SQLITE_STEP_DONE);
}

/* Look up one reference position without loading unrelated towns or geographic layers. */
bool jurisdict_place_at(jurisdiction_t *engine, double latitude, double longitude,
                       jurisdict_place_visit_fn visitor, void *context)
{
    if (!engine || !visitor || !isfinite(latitude) || !isfinite(longitude) ||
        fabs(latitude) > 90 || fabs(longitude) > 180) return false;
    static const char town_sql[] =
        "SELECT town.jurisdiction_id, town.town_name, lat.latitude, lon.longitude,\n"
        "       COALESCE(NULLIF(elev.elevation_metres, '-9999'), ''), code.timezone_name,\n"
        "       CASE WHEN def.jurisdiction_id IS NULL THEN 0 ELSE 1 END\n"
        "FROM jurisdiction_town_latitude AS lat\n"
        "JOIN jurisdiction_town AS town USING (jurisdiction_town_id)\n"
        "JOIN jurisdiction_town_longitude AS lon USING (jurisdiction_town_id)\n"
        "JOIN jurisdiction_town_timezone AS tz USING (jurisdiction_town_id)\n"
        "JOIN timezone_code AS code ON code.timezone_code = tz.timezone_code\n"
        "LEFT JOIN jurisdiction_town_elevation AS elev USING (jurisdiction_town_id)\n"
        "LEFT JOIN jurisdiction_default_town AS def\n"
        "       ON def.jurisdiction_town_id = town.jurisdiction_town_id\n"
        "      AND def.jurisdiction_id = town.jurisdiction_id\n"
        "WHERE CAST(lat.latitude AS REAL) BETWEEN ?1 AND ?2\n"
        "  AND CAST(lon.longitude AS REAL) BETWEEN ?3 AND ?4\n"
        "ORDER BY town.jurisdiction_id, town.jurisdiction_town_id\n"
        "LIMIT 1";
    static const char location_sql[] =
        "SELECT loc.jurisdiction_id, COALESCE(name.locality_name, loc.jurisdiction_id),\n"
        "       lat.latitude, lon.longitude, '0', code.timezone_name, 1\n"
        "FROM jurisdiction_location_default AS loc\n"
        "JOIN jurisdiction_location_default_latitude AS lat USING (jurisdiction_id)\n"
        "JOIN jurisdiction_location_default_longitude AS lon USING (jurisdiction_id)\n"
        "JOIN jurisdiction_location_default_timezone AS tz USING (jurisdiction_id)\n"
        "JOIN timezone_code AS code ON code.timezone_code = tz.timezone_code\n"
        "LEFT JOIN jurisdiction_location_default_locality AS name USING (jurisdiction_id)\n"
        "WHERE CAST(lat.latitude AS REAL) BETWEEN ?1 AND ?2\n"
        "  AND CAST(lon.longitude AS REAL) BETWEEN ?3 AND ?4\n"
        "ORDER BY loc.jurisdiction_id\n"
        "LIMIT 1";
    jurisdiction_set_error(engine, "");
    bool found = false;
    bool ok = jurisdiction_place_query(engine, town_sql, latitude, longitude, visitor, context, &found);
    if (ok && !found)
        ok = jurisdiction_place_query(engine, location_sql, latitude, longitude, visitor, context, &found);
    if (!ok) jurisdiction_set_error(engine, "Cannot resolve the catalogue reference position");
    return ok;
}
