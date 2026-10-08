/**
 * @file jurisdiction_catalogue.c
 * @brief Read-only enumeration of jurisdiction choices, default locations and towns.
 *
 * Projects the installed database through borrowed callback records. SQL joins use
 * the normalised catalogue keys; no packaged C or JSON copy supplies these rows.
 * Visitors may stop early. Statements are always finalised, including on errors.
 */
#include "jurisdiction_internal.h"

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

static bool jurisdiction_catalogue_places(jurisdiction_t *jurisdiction, const char *sql,
                                          jurisdict_place_visit_fn visitor, void *context)
{
    sqlite_stmt_t *stmt = jurisdiction_catalogue_prepare(jurisdiction, sql, visitor != NULL);
    if (!stmt)
        return false;
    sqlite_step_result_t step;
    bool stopped = false;
    while ((step = sqlite_stmt_step(stmt)) == SQLITE_STEP_ROW) {
        jurisdict_place_t place = {.jurisdiction_code = sqlite_stmt_column_text(stmt, 0),
                                   .name = sqlite_stmt_column_text(stmt, 1),
                                   .latitude = sqlite_stmt_column_text(stmt, 2),
                                   .longitude = sqlite_stmt_column_text(stmt, 3),
                                   .elevation = sqlite_stmt_column_text(stmt, 4),
                                   .timezone = sqlite_stmt_column_text(stmt, 5),
                                   .is_default = sqlite_stmt_column_int(stmt, 6) != 0};
        if (!visitor(&place, context)) {
            stopped = true;
            break;
        }
    }
    bool ok = stopped || step == SQLITE_STEP_DONE;
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
        "coalesce(elev.elevation_metres,'0'), "
        "code.timezone_name, case when def.jurisdiction_id is null then 0 else 1 end as is_default "
        "from jurisdiction_town town join jurisdiction_town_latitude lat using(jurisdiction_town_id) "
        "join jurisdiction_town_longitude lon using(jurisdiction_town_id) "
        "join jurisdiction_town_timezone tz using(jurisdiction_town_id) "
        "join timezone_code code on code.timezone_code = tz.timezone_code "
        "left join jurisdiction_town_elevation elev using(jurisdiction_town_id) "
        "left join jurisdiction_default_town def on def.jurisdiction_town_id = town.jurisdiction_town_id "
        "and def.jurisdiction_id = town.jurisdiction_id "
        "order by town.jurisdiction_id, is_default desc, town.town_name collate nocase, town.jurisdiction_town_id;";
    return jurisdiction_catalogue_places(jurisdiction, sql, visitor, context);
}
