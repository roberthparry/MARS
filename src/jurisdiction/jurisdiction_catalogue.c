/**
 * @file jurisdiction_catalogue.c
 * @brief Read-only enumeration of jurisdiction choices, default locations and towns.
 *
 * Projects the installed database through borrowed callback records. SQL joins use
 * the normalised catalogue keys; no packaged C or JSON copy supplies these rows.
 * Visitors may stop early. Statements are always finalised, including on errors.
 * Town name sorting is bounded to one indexed jurisdiction at a time.
 */
#include <math.h>
#include <string.h>

#define MARS_JURISDICTION_INTERNAL_ACCESS
#include "jurisdiction_internal.h"


/* Read the singleton setup origin without inferring a place from language or a worksheet selection. */
bool jurisdict_setup_origin(jurisdiction_t *j, jurisdict_place_visit_fn visitor, void *context)
{
    if (!j || !visitor) return false;
    sqlite_stmt_t *q = sqlite_stmt_prepare(j->db,
        "SELECT jurisdiction,location,CAST(latitude AS TEXT),CAST(longitude AS TEXT),timezone "
        "FROM calendar_local_settings WHERE singleton=1");
    bool ok = q && sqlite_stmt_step(q) == SQLITE_STEP_ROW;
    if (ok) {
        jurisdict_place_t place = {.jurisdiction_code = sqlite_stmt_column_text(q, 0),
            .name = sqlite_stmt_column_text(q, 1), .latitude = sqlite_stmt_column_text(q, 2),
            .longitude = sqlite_stmt_column_text(q, 3), .timezone = sqlite_stmt_column_text(q, 4),
            .elevation = "0", .is_default = true};
        ok = place.jurisdiction_code && *place.jurisdiction_code && place.name && *place.name &&
             place.latitude && place.longitude && place.timezone && *place.timezone;
        if (ok) visitor(&place, context);
    }
    sqlite_stmt_finalize(q);
    return ok;
}

static sqlite_stmt_t *jurisdiction_catalogue_prepare(jurisdiction_t *jurisdiction, const char *sql, bool visitor)
{
    if (!jurisdiction)
        return NULL;
    jurisdiction_set_error(jurisdiction, "");
    if (!visitor) {
        jurisdiction_set_error(jurisdiction, "A catalogue visitor is required");
        return NULL;
    }
    sqlite_stmt_t *stmt = sqlite_stmt_prepare(jurisdiction->db, sql);
    if (!stmt)
        jurisdiction_set_error(jurisdiction, "Cannot read the configured jurisdiction catalogue");
    return stmt;
}

static bool jurisdiction_catalogue_rows(sqlite_stmt_t *stmt, jurisdict_place_visit_fn visitor,
                                        void *context, bool *stopped)
{
    sqlite_step_result_t step;
    while ((step = sqlite_stmt_step(stmt)) == SQLITE_STEP_ROW) {
        jurisdict_place_t place = {.jurisdiction_code = sqlite_stmt_column_text(stmt, 0),
                                   .name = sqlite_stmt_column_text(stmt, 1),
                                   .latitude = sqlite_stmt_column_text(stmt, 2),
                                   .longitude = sqlite_stmt_column_text(stmt, 3),
                                   .elevation = sqlite_stmt_column_text(stmt, 4),
                                   .timezone = sqlite_stmt_column_text(stmt, 5),
                                   .is_default = sqlite_stmt_column_int(stmt, 6) != 0};
        if (!visitor(&place, context)) {
            *stopped = true;
            break;
        }
    }
    return *stopped || step == SQLITE_STEP_DONE;
}

/* Seek by the indexed town ID without rescanning preceding pages. */
static bool jurisdiction_town_batch(jurisdiction_t *jurisdiction, int64_t after_id, size_t limit,
                                    jurisdict_place_visit_fn visitor, void *context, int64_t *next_id,
                                    bool nearby, double latitude, double longitude)
{
    globe_t *globe = jurisdict_globe_open(jurisdiction);
    bool ok = globe_towns_batch(globe, after_id, limit, nearby, latitude, longitude, visitor, context, next_id);
    globe_close(globe);
    if (!ok && jurisdiction)
        jurisdiction_set_error(jurisdiction, "Cannot read a town catalogue batch");
    return ok;
}

/* Enumerate only one jurisdiction through the globe controller. */
bool jurisdict_each_town_jurisdiction_batch(jurisdiction_t *jurisdiction, const char *code, int64_t after_id, size_t limit,
                                            jurisdict_place_visit_fn visitor, void *context, int64_t *next_id)
{
    globe_t *globe = jurisdict_globe_open(jurisdiction);
    bool ok = globe_towns_jurisdiction_batch(globe, code, after_id, limit, visitor, context, next_id);
    globe_close(globe);
    return ok;
}

/* Enumerate a bounded ID page of the complete catalogue. */
bool jurisdict_each_town_batch(jurisdiction_t *jurisdiction, int64_t after_id, size_t limit,
                               jurisdict_place_visit_fn visitor, void *context, int64_t *next_id)
{
    return jurisdiction_town_batch(jurisdiction, after_id, limit, visitor, context, next_id, false, 0, 0);
}

/* Prioritise a small geographical window, wrapping longitude at the date line. */
bool jurisdict_each_town_near_batch(jurisdiction_t *jurisdiction, double latitude, double longitude,
                                    int64_t after_id, size_t limit, jurisdict_place_visit_fn visitor,
                                    void *context, int64_t *next_id)
{
    if (next_id)
        *next_id = 0;
    if (!isfinite(latitude) || !isfinite(longitude) || fabs(latitude) > 90 || fabs(longitude) > 180)
        return false;
    return jurisdiction_town_batch(jurisdiction, after_id, limit, visitor, context, next_id, true, latitude, longitude);
}

static bool jurisdiction_catalogue_places(jurisdiction_t *jurisdiction, const char *sql,
                                          jurisdict_place_visit_fn visitor, void *context)
{
    sqlite_stmt_t *stmt = jurisdiction_catalogue_prepare(jurisdiction, sql, visitor != NULL);
    if (!stmt)
        return false;
    bool stopped = false;
    bool ok = jurisdiction_catalogue_rows(stmt, visitor, context, &stopped);
    if (!ok)
        jurisdiction_set_error(jurisdiction, "Cannot enumerate the configured jurisdiction catalogue");
    sqlite_stmt_finalize(stmt);
    return ok;
}

/* Enumerate UI-independent, database-derived jurisdiction labels. */
bool jurisdict_each_choice(jurisdiction_t *jurisdiction, jurisdict_choice_visit_fn visitor, void *context)
{
    static const char sql[] =
        "select j.jurisdiction_id, case when p.name is not null then p.name || ' - ' || j.name else j.name end as "
        "label "
        "from jurisdiction j left join jurisdiction p on p.jurisdiction_id = j.parent_jurisdiction_id "
        "where j.jurisdiction_type in ('country','subdivision') and j.name is not null "
        "order by label collate nocase, j.jurisdiction_id;";
    sqlite_stmt_t *stmt = jurisdiction_catalogue_prepare(jurisdiction, sql, visitor != NULL);
    if (!stmt)
        return false;
    sqlite_step_result_t step;
    bool stopped = false;
    while ((step = sqlite_stmt_step(stmt)) == SQLITE_STEP_ROW) {
        if (!visitor(sqlite_stmt_column_text(stmt, 0), sqlite_stmt_column_text(stmt, 1), context)) {
            stopped = true;
            break;
        }
    }
    bool ok = stopped || step == SQLITE_STEP_DONE;
    if (!ok)
        jurisdiction_set_error(jurisdiction, "Cannot enumerate jurisdiction choices");
    sqlite_stmt_finalize(stmt);
    return ok;
}

/* Enumerate complete representative locations without synthesising country records. */
bool jurisdict_each_location(jurisdiction_t *jurisdiction, jurisdict_place_visit_fn visitor, void *context)
{
    static const char sql[] =
        "select loc.jurisdiction_id, coalesce(name.locality_name,loc.jurisdiction_id), lat.latitude, lon.longitude, "
        "'0', code.timezone_name, 1 from jurisdiction_location_default loc "
        "join jurisdiction_location_default_latitude lat using(jurisdiction_id) "
        "join jurisdiction_location_default_longitude lon using(jurisdiction_id) "
        "join jurisdiction_location_default_timezone tz using(jurisdiction_id) "
        "join timezone_code code on code.timezone_code = tz.timezone_code "
        "left join jurisdiction_location_default_locality name using(jurisdiction_id) order by loc.jurisdiction_id;";
    return jurisdiction_catalogue_places(jurisdiction, sql, visitor, context);
}

/* Enumerate installed towns with their explicit default marker and stored precision. */
bool jurisdict_each_town(jurisdiction_t *jurisdiction, jurisdict_place_visit_fn visitor, void *context)
{
    static const char sql[] =
        "select town.jurisdiction_id, town.town_name, lat.latitude, lon.longitude, "
        "coalesce(nullif(elev.elevation_metres,'-9999'),'0'), "
        "code.timezone_name, case when def.jurisdiction_id is null then 0 else 1 end as is_default "
        "from jurisdiction_town town join jurisdiction_town_latitude lat using(jurisdiction_town_id) "
        "join jurisdiction_town_longitude lon using(jurisdiction_town_id) "
        "join jurisdiction_town_timezone tz using(jurisdiction_town_id) "
        "join timezone_code code on code.timezone_code = tz.timezone_code "
        "left join jurisdiction_town_elevation elev using(jurisdiction_town_id) "
        "left join jurisdiction_default_town def on def.jurisdiction_town_id = town.jurisdiction_town_id "
        "and def.jurisdiction_id = town.jurisdiction_id "
        "where town.jurisdiction_id=?1 "
        "order by is_default desc, town.town_name collate nocase, town.jurisdiction_town_id;";
    /* Keep the sorter bounded to one indexed jurisdiction, not the entire world catalogue. */
    sqlite_stmt_t *groups = jurisdiction_catalogue_prepare(jurisdiction,
        "SELECT DISTINCT jurisdiction_id FROM jurisdiction_town_jurisdiction_id ORDER BY jurisdiction_id",
        visitor != NULL);
    sqlite_stmt_t *towns = groups ? jurisdiction_catalogue_prepare(jurisdiction, sql, visitor != NULL) : NULL;
    bool ok = groups && towns, stopped = false;
    sqlite_step_result_t step = SQLITE_STEP_DONE;
    while (ok && !stopped && (step = sqlite_stmt_step(groups)) == SQLITE_STEP_ROW) {
        sqlite_stmt_reset(towns);
        ok = sqlite_stmt_bind_text(towns, 1, sqlite_stmt_column_text(groups, 0)) &&
             jurisdiction_catalogue_rows(towns, visitor, context, &stopped);
    }
    ok = ok && (stopped || step == SQLITE_STEP_DONE);
    if (!ok && jurisdiction)
        jurisdiction_set_error(jurisdiction, "Cannot enumerate the configured jurisdiction catalogue");
    sqlite_stmt_finalize(towns);
    sqlite_stmt_finalize(groups);
    return ok;
}
