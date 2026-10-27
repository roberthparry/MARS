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

typedef struct _globe_t {
    shoreline_t *shoreline;
    rivers_t *rivers;
    sqlite_t *details;
    sqlite_t *geometry;
    sqlite_t *owned_geometry;
    bool towns;
    bool countries;
    bool regions;
} globe_t;

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
    g->geometry = shore_db;
    g->countries = globe_table(shore_db, "country_bounds") && globe_table(shore_db, "country_chunk") &&
                   globe_table(shore_db, "country_name");
    g->towns = globe_table(town_db, "jurisdiction_town");
    g->regions = globe_table(shore_db, "region_bounds") && globe_table(shore_db, "region_chunk") &&
                 globe_table(shore_db, "region_name");
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

/* Read only the indexed image identity until an application actually needs the PNG. */
bool globe_texture_metadata(globe_t *g, unsigned level, unsigned column, unsigned row, globe_texture_info_t *info)
{
    return textures_metadata(g ? g->geometry : NULL, level, column, row, info);
}

/* Enumerate regional imagery without reading image payloads. */
bool globe_texture_regions(globe_t *g, const double bounds[4], globe_texture_region_fn callback, void *context)
{
    return textures_regions(g ? g->geometry : NULL, bounds, callback, context);
}

/* Keep payload ownership and corruption detection within the geographic data API. */
array_t *globe_texture_png(globe_t *g, unsigned level, unsigned column, unsigned row,
                            const globe_texture_info_t *expected)
{
    return textures_png(g ? g->geometry : NULL, level, column, row, expected);
}

/* Use a bounded indexed capability table, including explicit unavailable future layers. */
bool globe_has_layer(const globe_t *g, globe_layer_t layer)
{
    if (!g || (unsigned)layer > GLOBE_LAYER_COUNTRIES) return false;
    const bool available[] = {g->shoreline != NULL, g->rivers != NULL, g->towns, false, false, g->countries};
    return available[layer];
}

/* Preserve exact shoreline classification and uncertainty. */
globe_surface_t globe_hit_test(globe_t *g, double latitude, double longitude)
{
    return shoreline_hit_test(g ? g->shoreline : NULL, latitude, longitude);
}

/* Query near-shore context without altering default classification or country ownership. */
globe_surface_t globe_hit_test_radius(globe_t *g, double latitude, double longitude, double radius_metres)
{
    return shoreline_hit_test_radius(g ? g->shoreline : NULL, latitude, longitude, radius_metres);
}

/* Geographic water names do not imply administrative ownership. */
string_t *globe_water_name_at(globe_t *g, double latitude, double longitude)
{
    const char *name = shoreline_water_name(g ? g->shoreline : NULL, latitude, longitude);
    return name ? string_new_with(name) : NULL;
}

/* Administrative coverage is independent of land/water; never infer it from a nearby town. */
string_t *globe_country_at(globe_t *g, double latitude, double longitude)
{
    if (!g || !g->countries || !isfinite(latitude) || !isfinite(longitude) || fabs(latitude) > 90 || fabs(longitude) > 180)
        return NULL;
    return countries_resolve(g->geometry, g->shoreline, latitude, longitude, false);
}

/* Geographic codes link to jurisdiction metadata without using the selected form jurisdiction. */
string_t *globe_country_code_at(globe_t *g, double latitude, double longitude)
{
    if (!g || !g->countries || !isfinite(latitude) || !isfinite(longitude) || fabs(latitude) > 90 || fabs(longitude) > 180)
        return NULL;
    string_t *code = countries_resolve(g->geometry, g->shoreline, latitude, longitude, true);
    if (code && string_byte_length(code) != 2 && string_byte_length(code) != 6) { string_free(code); return NULL; }
    return code;
}

/* Resolve an independent cultural display overlay without replacing country ownership. */
string_t *globe_region_code_at(globe_t *g, double latitude, double longitude)
{
    return g && g->regions ? regions_lookup(g->geometry, latitude, longitude) : NULL;
}

/* Borders are a separate display layer; availability never changes land/sea classification. */
bool globe_border_segments(globe_t *g, double west, double south, double east, double north,
                           globe_border_segment_fn visitor, void *context)
{
    return g && countries_borders(g->geometry, west, south, east, north, visitor, context);
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

/* Read recorded river width independently of the centreline geometry. */
bool globe_river_width_metres(globe_t *g, int64_t id, double *width)
{
    return g && rivers_width_metres(g->rivers, id, width);
}

/* Stream bank geometry independently of centreline width metadata. */
bool globe_river_banks(globe_t *g, double west, double south, double east, double north,
                       globe_river_bank_fn visit, void *context)
{
    return g && rivers_banks(g->rivers, west, south, east, north, visit, context);
}

/* Complete survey coverage permits local water polygons to supersede a coarse lake mask. */
bool globe_river_water_complete(globe_t *g, double west, double south, double east, double north)
{
    return g && rivers_water_complete(g->rivers, west, south, east, north);
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
static bool globe_town_page(globe_t *globe, int64_t after_id, size_t limit, bool nearby, double latitude, double longitude,
                            const char *jurisdiction, globe_place_visit_fn visitor, void *context, int64_t *next_id)
{
    if (next_id)
        *next_id = 0;
    if (!globe || !globe->towns || !visitor || !next_id || after_id < 0 || !limit || limit > 1024 ||
        (nearby && (!isfinite(latitude) || !isfinite(longitude) || fabs(latitude) > 90 || fabs(longitude) > 180)))
        return false;
    const char *sql =
        "select town.jurisdiction_id,town.town_name,lat.latitude,lon.longitude,"
        "coalesce(nullif(elev.elevation_metres,'-9999'),'0'),code.timezone_name,"
        "case when def.jurisdiction_id is null then 0 else 1 end,town.jurisdiction_town_id "
        "from jurisdiction_town town join jurisdiction_town_latitude lat using(jurisdiction_town_id) "
        "join jurisdiction_town_longitude lon using(jurisdiction_town_id) "
        "join jurisdiction_town_timezone tz using(jurisdiction_town_id) "
        "join timezone_code code on code.timezone_code=tz.timezone_code "
        "left join jurisdiction_town_elevation elev using(jurisdiction_town_id) "
        "left join jurisdiction_default_town def on def.jurisdiction_town_id=town.jurisdiction_town_id "
        "and def.jurisdiction_id=town.jurisdiction_id ";
    const char *filter = jurisdiction ?
        "where town.jurisdiction_id=?3 and town.jurisdiction_town_id>?1 "
        "order by town.jurisdiction_town_id limit ?2" : nearby ?
        "where cast(lat.latitude as real) between ?3-5 and ?3+5 "
        "and (abs(cast(lon.longitude as real)-?4)<=5 or abs(cast(lon.longitude as real)-?4)>=355) "
        "and town.jurisdiction_town_id>?1 order by town.jurisdiction_town_id limit ?2" :
        "where town.jurisdiction_town_id>?1 order by town.jurisdiction_town_id limit ?2";
    string_t *query = string_sprintf("%s%s", sql, filter);
    sqlite_stmt_t *stmt = query ? sqlite_stmt_prepare(globe->details, string_c_str(query)) : NULL;
    string_free(query);
    bool ok = stmt && sqlite_stmt_bind_int64(stmt, 1, after_id) && sqlite_stmt_bind_int64(stmt, 2, (int64_t)limit) &&
              (!jurisdiction || sqlite_stmt_bind_text(stmt, 3, jurisdiction)) &&
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

/* Page through the world or a bounded geographic window. */
bool globe_towns_batch(globe_t *globe, int64_t after_id, size_t limit, bool nearby, double latitude, double longitude,
                       globe_place_visit_fn visitor, void *context, int64_t *next_id)
{
    return globe_town_page(globe, after_id, limit, nearby, latitude, longitude, NULL, visitor, context, next_id);
}

/* Load only the requested jurisdiction through its indexed catalogue membership. */
bool globe_towns_jurisdiction_batch(globe_t *globe, const char *jurisdiction, int64_t after_id, size_t limit,
                                    globe_place_visit_fn visitor, void *context, int64_t *next_id)
{
    if (!jurisdiction || !*jurisdiction) {
        if (next_id) *next_id = 0;
        return false;
    }
    return globe_town_page(globe, after_id, limit, false, 0, 0, jurisdiction, visitor, context, next_id);
}
