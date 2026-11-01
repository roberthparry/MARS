/**
 * @file jurisdiction_search.c
 * @brief Bounded worldwide searches through Unicode town, jurisdiction and geographic alias indexes.
 *
 * Treats input as literal text, not FTS syntax. Unicode tokenisation supplies case- and accent-insensitive
 * name-prefix matching. Country and region matches select their default town or first indexed local town.
 * Only the best 256 indexed candidates reach coordinate joins and duplicate elimination;
 * returned records retain their actual regional membership, reference height and time zone.
 * Missing indexes report failure without scanning the world catalogue or writing to the database.
 */
#include <string.h>

#define MARS_JURISDICTION_INTERNAL_ACCESS
#include "jurisdiction_internal.h"

/* Search the installed name index without requiring a selected country or a resident world catalogue. */
bool jurisdict_search_towns(jurisdiction_t *engine, const string_t *text, size_t limit,
                            jurisdict_place_visit_fn visitor, void *context)
{
    if (engine) jurisdiction_set_error(engine, "");
    if (!engine || !text || !visitor || !limit || limit > 64 || string_byte_length(text) > 256 ||
        !string_is_valid_utf8(text)) return false;
    string_t *name = string_clone(text);
    if (name) string_trim(name);
    bool valid = name && string_length(name) >= 2 && !memchr(string_c_str(name), 0, string_byte_length(name));
    string_t *quoted = valid ? string_clone(name) : NULL;
    valid = valid && quoted && string_replace(quoted, "\"", "\"\"") >= 0;
    string_t *match = valid ? string_sprintf("\"%s\"*", string_c_str(quoted)) : NULL;
    static const char sql[] =
        "WITH town_matches AS (\n"
        "    SELECT rowid AS id, rank AS relevance,\n"
        "           CASE WHEN lower(town_name)=lower(?2) THEN 0 ELSE 2 END AS priority\n"
        "    FROM jurisdiction_town_search\n"
        "    WHERE jurisdiction_town_search MATCH ?1\n"
        "    ORDER BY rank, rowid LIMIT 256\n"
        "), area_matches AS (\n"
        "    SELECT jurisdiction_id, name, rank AS relevance\n"
        "    FROM jurisdiction_area_search\n"
        "    WHERE jurisdiction_area_search MATCH ?1\n"
        "    ORDER BY rank, rowid LIMIT 64\n"
        "), area_towns AS (\n"
        "    SELECT COALESCE(\n"
        "        (SELECT jurisdiction_town_id FROM jurisdiction_default_town\n"
        "         WHERE jurisdiction_id=area.jurisdiction_id),\n"
        "        (SELECT jurisdiction_town_id FROM jurisdiction_town_jurisdiction_id\n"
        "         WHERE jurisdiction_id=area.jurisdiction_id ORDER BY jurisdiction_town_id LIMIT 1)\n"
        "    ) AS id, relevance, name\n"
        "    FROM area_matches AS area\n"
        "), alias_matches AS (\n"
        "    SELECT jurisdiction_town_id AS id, name, rank AS relevance,\n"
        "           CASE WHEN lower(name)=lower(?2) THEN 0 ELSE 1 END AS priority\n"
        "    FROM jurisdiction_place_search\n"
        "    WHERE jurisdiction_place_search MATCH ?1\n"
        "    ORDER BY rank, rowid LIMIT 64\n"
        "), references_found AS (\n"
        "        SELECT id, priority, relevance, NULL AS matched_name FROM town_matches\n"
        "        UNION ALL\n"
        "        SELECT id, CASE WHEN lower(name)=lower(?2) THEN 1 ELSE 3 END, relevance, name\n"
        "        FROM area_towns WHERE id IS NOT NULL\n"
        "        UNION ALL\n"
        "        SELECT id, priority, relevance, name FROM alias_matches\n"
        "), candidates AS (\n"
        "    SELECT id, priority, relevance, matched_name\n"
        "    FROM (\n"
        "        SELECT *, ROW_NUMBER() OVER (\n"
        "            PARTITION BY id ORDER BY priority, relevance, matched_name\n"
        "        ) AS choice FROM references_found\n"
        "    ) WHERE choice=1 ORDER BY priority, relevance, id LIMIT 256\n"
        "), places AS (\n"
        "    SELECT town.jurisdiction_id, town.town_name, lat.latitude, lon.longitude,\n"
        "           COALESCE(NULLIF(elev.elevation_metres, '-9999'), '0') AS elevation,\n"
        "           zone.timezone_name, def.jurisdiction_id IS NOT NULL AS is_default,\n"
        "           candidates.priority, candidates.relevance, candidates.matched_name,\n"
        "           town.jurisdiction_town_id AS id,\n"
        "           ROW_NUMBER() OVER (\n"
        "               PARTITION BY substr(town.jurisdiction_id,1,2), town.town_name,\n"
        "                            CAST(lat.latitude AS REAL), CAST(lon.longitude AS REAL)\n"
        "               ORDER BY length(town.jurisdiction_id) DESC, town.jurisdiction_town_id\n"
        "           ) AS occurrence\n"
        "    FROM candidates\n"
        "    JOIN jurisdiction_town AS town ON town.jurisdiction_town_id=candidates.id\n"
        "    JOIN jurisdiction_town_latitude AS lat USING (jurisdiction_town_id)\n"
        "    JOIN jurisdiction_town_longitude AS lon USING (jurisdiction_town_id)\n"
        "    JOIN jurisdiction_town_timezone AS tz USING (jurisdiction_town_id)\n"
        "    JOIN timezone_code AS zone ON zone.timezone_code=tz.timezone_code\n"
        "    LEFT JOIN jurisdiction_town_elevation AS elev USING (jurisdiction_town_id)\n"
        "    LEFT JOIN jurisdiction_default_town AS def\n"
        "           ON def.jurisdiction_town_id=town.jurisdiction_town_id\n"
        "          AND def.jurisdiction_id=town.jurisdiction_id\n"
        ")\n"
        "SELECT jurisdiction_id, town_name, latitude, longitude, elevation, timezone_name, is_default, matched_name\n"
        "FROM places WHERE occurrence=1\n"
        "ORDER BY priority, lower(town_name)<>lower(?2), is_default DESC, relevance,\n"
        "         town_name COLLATE NOCASE, jurisdiction_id, id\n"
        "LIMIT ?3";
    sqlite_stmt_t *query = match ? sqlite_stmt_prepare(engine->db, sql) : NULL;
    bool ok = query && sqlite_stmt_bind_text(query, 1, string_c_str(match)) &&
              sqlite_stmt_bind_text(query, 2, string_c_str(name)) && sqlite_stmt_bind_int64(query, 3, (int64_t)limit);
    sqlite_step_result_t step = SQLITE_STEP_DONE;
    bool stopped = false;
    while (ok && (step = sqlite_stmt_step(query)) == SQLITE_STEP_ROW) {
        jurisdict_place_t place = {
            .jurisdiction_code = sqlite_stmt_column_text(query, 0), .name = sqlite_stmt_column_text(query, 1),
            .latitude = sqlite_stmt_column_text(query, 2), .longitude = sqlite_stmt_column_text(query, 3),
            .elevation = sqlite_stmt_column_text(query, 4), .timezone = sqlite_stmt_column_text(query, 5),
            .is_default = sqlite_stmt_column_int(query, 6) != 0,
            .matched_name = sqlite_stmt_column_text(query, 7),
        };
        if (!visitor(&place, context)) { stopped = true; break; }
    }
    ok = ok && (stopped || step == SQLITE_STEP_DONE);
    if (!ok) jurisdiction_set_error(engine, "Cannot search the installed town index; update the jurisdiction database");
    sqlite_stmt_finalize(query);
    string_free(match); string_free(quoted); string_free(name);
    return ok;
}
