/**
 * @file globe.c
 * @brief Own and dispatch geographic layers over borrowed SQLite connections.
 *
 * Separates availability from geography: missing or retired data never imply sea or an empty verified map.
 * Shoreline caches retain their existing ownership rules; regional river queries remain bounded and uncached.
 */
#include <stdlib.h>
#include <math.h>

#define MARS_GLOBE_INTERNAL_ACCESS
#include "globe_layers.h"

struct _globe_t {
    shoreline_t *shoreline;
    rivers_t *rivers;
    sqlite_t *details;
    sqlite_t *owned_geometry;
    bool towns;
};

static bool globe_table(sqlite_t *db, const char *name)
{
    if (!db) return false;
    sqlite_stmt_t *q = sqlite_stmt_prepare(db, "select 1 from sqlite_master where type in ('table','view') and name=?1");
    bool found = q && sqlite_stmt_bind_text(q, 1, name) && sqlite_stmt_step(q) == SQLITE_STEP_ROW;
    sqlite_stmt_finalize(q);
    return found;
}

/* Borrow connections and own only the available layer readers. */
globe_t *globe_open(sqlite_t *db, sqlite_t *detail_db)
{
    return globe_open_databases(db, detail_db, detail_db);
}

/* Keep geometry storage independent of the jurisdiction town catalogue. */
globe_t *globe_open_databases(sqlite_t *shore_db, sqlite_t *map_db, sqlite_t *town_db)
{
    if (!shore_db && !map_db && !town_db) return NULL;
    globe_t *g = calloc(1, sizeof *g);
    if (!g) return NULL;
    g->shoreline = shoreline_open(shore_db);
    g->details = town_db;
    g->towns = globe_table(town_db, "jurisdiction_town");
    if (globe_table(map_db, "rivers_dataset")) {
        sqlite_stmt_t *q = sqlite_stmt_prepare(map_db,
            "select 1 from rivers_dataset where id=1 and source not like 'WDBII%'");
        bool accepted = q && sqlite_stmt_step(q) == SQLITE_STEP_ROW;
        sqlite_stmt_finalize(q);
        if (accepted) g->rivers = rivers_open(map_db);
    }
    return g;
}

/* Own configured geometry while continuing to borrow the independent town connection. */
globe_t *globe_open_configured(sqlite_t *town_db)
{
    sqlite_t *db = globe_configured_database();
    if (!db) return NULL;
    globe_t *g = globe_open_databases(db, db, town_db);
    if (g) g->owned_geometry = db;
    else sqlite_close(db);
    return g;
}

/* Release readers before closing any owned geometry connection; borrowed databases remain open. */
void globe_close(globe_t *g)
{
    if (!g) return;
    shoreline_close(g->shoreline);
    rivers_close(g->rivers);
    sqlite_close(g->owned_geometry);
    free(g);
}

/* Use a bounded indexed capability table, including explicit unavailable future layers. */
bool globe_has_layer(const globe_t *g, globe_layer_t layer)
{
    if (!g || (unsigned)layer > GLOBE_LAYER_BUILDINGS) return false;
    const bool available[] = {g->shoreline != NULL, g->rivers != NULL, g->towns, false, false};
    return available[layer];
}

/* Preserve exact shoreline classification and uncertainty. */
globe_surface_t globe_hit_test(globe_t *g, double latitude, double longitude)
{
    return shoreline_hit_test(g ? g->shoreline : NULL, latitude, longitude);
}

/* Prepare the existing shared shoreline cache only when explicitly requested. */
bool globe_surface_prepare(globe_t *g)
{
    return g && shoreline_prepare(g->shoreline);
}

/* Delegate clipped display paths to the private shoreline engine. */
bool globe_surface_paths(globe_t *g, double west, double south, double east, double north,
                         double tolerance, globe_path_fn visit, void *context)
{
    return g && shoreline_paths(g->shoreline, west, south, east, north, tolerance, visit, context);
}

/* Stream regional rivers without retaining the full network. */
bool globe_river_segments(globe_t *g, double west, double south, double east, double north,
                          globe_river_segment_fn visit, void *context)
{
    return g && rivers_segments(g->rivers, west, south, east, north, visit, context);
}

/* Apply the caller's shoreline-scale tolerance while preserving river junctions. */
bool globe_river_display_segments(globe_t *g, double west, double south, double east, double north,
                                  double tolerance, globe_river_segment_fn visit, void *context)
{
    return g && rivers_display_segments(g->rivers, west, south, east, north, tolerance, visit, context);
}

/* Preserve transactional shoreline import without changing catalogue ownership. */
bool globe_import_shoreline(sqlite_t *db, file_t *source)
{
    return shoreline_import(db, source);
}

/* Retain the binary importer for legacy-format fixtures, not production display. */
bool globe_import_rivers(sqlite_t *db, file_t *source)
{
    return rivers_import(db, source);
}

/* Page through indexed town identifiers; preserve existing catalogue ordering and geographic priority semantics. */
bool globe_towns_batch(globe_t *globe, int64_t after_id, size_t limit, bool nearby, double latitude, double longitude,
                       globe_place_visit_fn visitor, void *context, int64_t *next_id)
{
    if (next_id)
        *next_id = 0;
    if (!globe || !globe->towns || !visitor || !next_id || after_id < 0 || !limit || limit > 1024 ||
        (nearby && (!isfinite(latitude) || !isfinite(longitude) || fabs(latitude) > 90 || fabs(longitude) > 180)))
        return false;
    const char *sql =
        "select town.jurisdiction_id,town.town_name,lat.latitude,lon.longitude,"
        "coalesce(elev.elevation_metres,'0'),code.timezone_name,"
        "case when def.jurisdiction_id is null then 0 else 1 end,town.jurisdiction_town_id "
        "from jurisdiction_town town join jurisdiction_town_latitude lat using(jurisdiction_town_id) "
        "join jurisdiction_town_longitude lon using(jurisdiction_town_id) "
        "join jurisdiction_town_timezone tz using(jurisdiction_town_id) "
        "join timezone_code code on code.timezone_code=tz.timezone_code "
        "left join jurisdiction_town_elevation elev using(jurisdiction_town_id) "
        "left join jurisdiction_default_town def on def.jurisdiction_town_id=town.jurisdiction_town_id "
        "and def.jurisdiction_id=town.jurisdiction_id ";
    const char *filter = nearby ?
        "where cast(lat.latitude as real) between ?3-5 and ?3+5 "
        "and (abs(cast(lon.longitude as real)-?4)<=5 or abs(cast(lon.longitude as real)-?4)>=355) "
        "and town.jurisdiction_town_id>?1 order by town.jurisdiction_town_id limit ?2" :
        "where town.jurisdiction_town_id>?1 order by town.jurisdiction_town_id limit ?2";
    string_t *query = string_sprintf("%s%s", sql, filter);
    sqlite_stmt_t *stmt = query ? sqlite_stmt_prepare(globe->details, string_c_str(query)) : NULL;
    string_free(query);
    bool ok = stmt && sqlite_stmt_bind_int64(stmt, 1, after_id) && sqlite_stmt_bind_int64(stmt, 2, (int64_t)limit) &&
              (!nearby || (sqlite_stmt_bind_double(stmt, 3, latitude) && sqlite_stmt_bind_double(stmt, 4, longitude)));
    sqlite_step_result_t step = SQLITE_STEP_DONE;
    bool stopped = false;
    while (ok && (step = sqlite_stmt_step(stmt)) == SQLITE_STEP_ROW) {
        globe_place_t place = {.jurisdiction_code = sqlite_stmt_column_text(stmt, 0),
                                   .name = sqlite_stmt_column_text(stmt, 1),
                                   .latitude = sqlite_stmt_column_text(stmt, 2),
                                   .longitude = sqlite_stmt_column_text(stmt, 3),
                                   .elevation = sqlite_stmt_column_text(stmt, 4),
                                   .timezone = sqlite_stmt_column_text(stmt, 5),
                                   .is_default = sqlite_stmt_column_int(stmt, 6) != 0};
        *next_id = sqlite_stmt_column_int64(stmt, 7);
        if (!visitor(&place, context)) {
            stopped = true;
            break;
        }
    }
    ok = ok && (stopped || step == SQLITE_STEP_DONE);
    sqlite_stmt_finalize(stmt);
    return ok;
}
