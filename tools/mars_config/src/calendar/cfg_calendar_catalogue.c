/**
 * @file cfg_calendar_catalogue.c
 * @brief Indexed town resolution and explicit substring/fuzzy discovery.
 *
 * The installed catalogue is normalised once into connection-local temporary
 * tables. Ordinary lookups use an index; only requested discovery scans names.
 * Country duplicates are suppressed only when a matching subdivision describes
 * the same physical town. All returned locations own their string fields.
 */
#include <stdlib.h>

#include "cfg_calendar_internal.h"

/* Release one location, including partially constructed records. */
void cfg_calendar_place_clear(void *value)
{
    cfg_calendar_place_t *place = value;
    string_free(place->name);
    string_free(place->jurisdiction);
    string_free(place->timezone);
    *place = (cfg_calendar_place_t){0};
}

/* Materialise normalised names with indexes for name and default resolution. */
bool cfg_calendar_catalogue(sqlite_t *db)
{
    bool ok = sqlite_exec_cstr(
        db, "DROP TABLE IF EXISTS temp.cfg_calendar_places;"
            "CREATE TEMP TABLE cfg_calendar_places(normalized TEXT NOT NULL,name TEXT NOT NULL,"
            "jurisdiction TEXT NOT NULL,latitude REAL NOT NULL,longitude REAL NOT NULL,timezone TEXT NOT NULL,"
            "default_for TEXT); CREATE INDEX temp.cfg_calendar_place_name ON cfg_calendar_places(normalized);"
            "CREATE INDEX temp.cfg_calendar_place_default ON cfg_calendar_places(default_for);");
    sqlite_stmt_t *read =
        ok ? sqlite_stmt_prepare(
                 db,
                 "SELECT t.town_name,t.jurisdiction_id,lat.latitude,lon.longitude,zone.timezone_name,d.jurisdiction_id "
                 "FROM jurisdiction_town t JOIN jurisdiction_town_latitude lat USING(jurisdiction_town_id) "
                 "JOIN jurisdiction_town_longitude lon USING(jurisdiction_town_id) "
                 "JOIN jurisdiction_town_timezone tz USING(jurisdiction_town_id) "
                 "JOIN timezone_code zone USING(timezone_code) "
                 "LEFT JOIN jurisdiction_default_town d USING(jurisdiction_town_id)")
           : NULL;
    sqlite_stmt_t *write =
        read ? sqlite_stmt_prepare(db, "INSERT INTO cfg_calendar_places VALUES(?1,?2,?3,?4,?5,?6,?7)") : NULL;
    ok = ok && read && write;
    sqlite_step_result_t step = SQLITE_STEP_DONE;
    while (ok && (step = sqlite_stmt_step(read)) == SQLITE_STEP_ROW) {
        const char *name = sqlite_stmt_column_text(read, 0);
        string_t *text = name ? string_new_with(name) : NULL;
        string_t *key = text ? cfg_calendar_key(text, false) : NULL;
        sqlite_stmt_reset(write);
        ok = key && sqlite_stmt_bind_text(write, 1, string_c_str(key));
        for (int column = 0; column < 6 && ok; ++column) {
            if (column == 2 || column == 3)
                ok = !sqlite_stmt_column_is_null(read, column) &&
                     sqlite_stmt_bind_double(write, column + 2, sqlite_stmt_column_double(read, column));
            else if (column == 5 && sqlite_stmt_column_is_null(read, column))
                ok = sqlite_stmt_bind_null(write, column + 2);
            else
                ok = !sqlite_stmt_column_is_null(read, column) &&
                     sqlite_stmt_bind_text(write, column + 2, sqlite_stmt_column_text(read, column));
        }
        ok = ok && sqlite_stmt_step(write) == SQLITE_STEP_DONE;
        string_free(key);
        string_free(text);
    }
    sqlite_stmt_finalize(write);
    sqlite_stmt_finalize(read);
    return ok && step == SQLITE_STEP_DONE;
}

static bool cfg_calendar_query_parts(const string_t *text, string_t **name, string_t **jurisdiction)
{
    string_cursor_t *cursor = string_cursor_new(text);
    if (!cursor)
        return false;
    string_pos_t start = string_cursor_position(cursor), last = start, after = start;
    bool comma = false;
    while (!string_cursor_done(cursor)) {
        if (rune_value(string_cursor_peek(cursor)) == ',') {
            comma = true;
            last = string_cursor_position(cursor);
            string_cursor_next(cursor);
            after = string_cursor_position(cursor);
        } else
            string_cursor_next(cursor);
    }
    string_t *part = comma ? string_cursor_slice_between(start, last, cursor) : string_clone(text);
    *name = part ? cfg_calendar_key(part, false) : NULL;
    *jurisdiction = comma ? string_cursor_extract(after, cursor) : string_new();
    if (*jurisdiction) {
        string_trim(*jurisdiction);
        string_to_upper(*jurisdiction);
    }
    string_free(part);
    string_cursor_free(cursor);
    return *name && *jurisdiction;
}

static array_t *cfg_calendar_rows(sqlite_stmt_t *stmt)
{
    array_t *rows = stmt ? array_create(sizeof(cfg_calendar_place_t), NULL, cfg_calendar_place_clear) : NULL;
    sqlite_step_result_t step = SQLITE_STEP_DONE;
    bool ok = rows != NULL;
    while (ok && (step = sqlite_stmt_step(stmt)) == SQLITE_STEP_ROW) {
        cfg_calendar_place_t place = {.name = string_new_with(sqlite_stmt_column_text(stmt, 0)),
                                      .jurisdiction = string_new_with(sqlite_stmt_column_text(stmt, 1)),
                                      .latitude = sqlite_stmt_column_double(stmt, 2),
                                      .longitude = sqlite_stmt_column_double(stmt, 3),
                                      .timezone = string_new_with(sqlite_stmt_column_text(stmt, 4))};
        ok = place.name && place.jurisdiction && place.timezone && array_add(rows, &place);
        if (!ok)
            cfg_calendar_place_clear(&place);
    }
    if (!ok || step != SQLITE_STEP_DONE) {
        array_destroy(rows);
        rows = NULL;
    }
    return rows;
}

static array_t *cfg_calendar_selected(sqlite_t *db, const string_t *jurisdiction, bool multiple, const string_t *name)
{
    const char *exact =
        "SELECT DISTINCT p.name,p.jurisdiction,p.latitude,p.longitude,p.timezone FROM cfg_calendar_places p "
        "WHERE p.normalized=?1 AND (?2='' OR p.jurisdiction=?2) AND (?2<>'' OR NOT EXISTS("
        "SELECT 1 FROM cfg_calendar_places q WHERE q.normalized=p.normalized "
        "AND substr(q.jurisdiction,1,length(p.jurisdiction)+1)=p.jurisdiction||'-' "
        "AND q.latitude=p.latitude AND q.longitude=p.longitude AND q.timezone=p.timezone)) "
        "ORDER BY p.normalized,p.jurisdiction";
    const char *many =
        "SELECT DISTINCT p.name,p.jurisdiction,p.latitude,p.longitude,p.timezone FROM cfg_calendar_places p "
        "JOIN cfg_calendar_discovery k ON k.name=p.normalized "
        "WHERE (?2='' OR p.jurisdiction=?2) AND (?2<>'' OR NOT EXISTS("
        "SELECT 1 FROM cfg_calendar_places q WHERE q.normalized=p.normalized "
        "AND substr(q.jurisdiction,1,length(p.jurisdiction)+1)=p.jurisdiction||'-' "
        "AND q.latitude=p.latitude AND q.longitude=p.longitude AND q.timezone=p.timezone)) "
        "ORDER BY p.normalized,p.jurisdiction";
    sqlite_stmt_t *stmt = sqlite_stmt_prepare(db, multiple ? many : exact);
    bool ok = stmt && (multiple || sqlite_stmt_bind_text(stmt, 1, string_c_str(name))) &&
              sqlite_stmt_bind_text(stmt, 2, string_c_str(jurisdiction));
    array_t *rows = ok ? cfg_calendar_rows(stmt) : NULL;
    sqlite_stmt_finalize(stmt);
    return rows;
}

/* Honour complete comma-containing names before interpreting a jurisdiction suffix. */
array_t *cfg_calendar_matches(sqlite_t *db, const string_t *text)
{
    string_t *full = cfg_calendar_key(text, false), *name = NULL, *jurisdiction = NULL;
    sqlite_stmt_t *exists =
        full ? sqlite_stmt_prepare(db, "SELECT 1 FROM cfg_calendar_places WHERE normalized=?1 LIMIT 1") : NULL;
    bool ok = exists && sqlite_stmt_bind_text(exists, 1, string_c_str(full));
    sqlite_step_result_t step = ok ? sqlite_stmt_step(exists) : SQLITE_STEP_ERROR;
    if (step == SQLITE_STEP_ROW) {
        name = string_clone(full);
        jurisdiction = string_new();
        ok = name && jurisdiction;
    } else if (step == SQLITE_STEP_DONE)
        ok = cfg_calendar_query_parts(text, &name, &jurisdiction);
    else
        ok = false;
    sqlite_stmt_finalize(exists);
    if (ok && cfg_calendar_equal(name, "quebec city")) {
        string_free(name);
        name = string_new_with("quebec");
        ok = name != NULL;
    }
    array_t *rows = ok ? cfg_calendar_selected(db, jurisdiction, false, name) : NULL;
    string_free(full);
    string_free(name);
    string_free(jurisdiction);
    return rows;
}

typedef struct {
    size_t a, b, length;
} cfg_calendar_block_t;

static array_t *cfg_calendar_runes(const string_t *text)
{
    array_t *runes = array_create(sizeof(uint32_t), NULL, NULL);
    string_view_t view = string_view_all(text);
    string_pos_t position = 0, next = 0;
    bool ok = runes != NULL;
    while (ok && position < string_byte_length(text)) {
        uint32_t value = 0;
        ok = string_view_peek_rune_value(view, position, &value, &next) && next > position && array_add(runes, &value);
        position = next;
    }
    if (!ok) {
        array_destroy(runes);
        runes = NULL;
    }
    return runes;
}

/* SequenceMatcher's earliest longest block, followed by its two disjoint regions. */
static bool cfg_calendar_matching(const array_t *a, size_t alo, size_t ahi, const array_t *b, size_t blo, size_t bhi,
                                  const bool *popular, size_t *total)
{
    size_t width = bhi - blo;
    size_t *previous = calloc(width + 1, sizeof(size_t)), *current = calloc(width + 1, sizeof(size_t));
    if (!previous || !current) {
        free(previous);
        free(current);
        return false;
    }
    cfg_calendar_block_t best = {alo, blo, 0};
    for (size_t i = alo; i < ahi; ++i) {
        for (size_t j = blo; j < bhi; ++j) {
            bool equal = *(uint32_t *)array_get(a, i) == *(uint32_t *)array_get(b, j);
            size_t length = equal && !popular[j] ? previous[j - blo] + 1 : 0;
            current[j - blo + 1] = length;
            if (length > best.length)
                best = (cfg_calendar_block_t){i + 1 - length, j + 1 - length, length};
        }
        size_t *swap = previous;
        previous = current;
        current = swap;
    }
    free(previous);
    free(current);
    while (best.a > alo && best.b > blo &&
           *(uint32_t *)array_get(a, best.a - 1) == *(uint32_t *)array_get(b, best.b - 1)) {
        --best.a;
        --best.b;
        ++best.length;
    }
    while (best.a + best.length < ahi && best.b + best.length < bhi &&
           *(uint32_t *)array_get(a, best.a + best.length) == *(uint32_t *)array_get(b, best.b + best.length))
        ++best.length;
    if (!best.length)
        return true;
    *total += best.length;
    return (!(best.a > alo && best.b > blo) || cfg_calendar_matching(a, alo, best.a, b, blo, best.b, popular, total)) &&
           (!(best.a + best.length < ahi && best.b + best.length < bhi) ||
            cfg_calendar_matching(a, best.a + best.length, ahi, b, best.b + best.length, bhi, popular, total));
}

static bool cfg_calendar_similarity(const string_t *candidate, const array_t *query, const bool *popular, double *ratio)
{
    array_t *a = cfg_calendar_runes(candidate);
    if (!a)
        return false;
    size_t total = 0, size = array_size(a) + array_size(query);
    bool ok = cfg_calendar_matching(a, 0, array_size(a), query, 0, array_size(query), popular, &total);
    *ratio = size ? 2.0 * total / size : 1.0;
    array_destroy(a);
    return ok;
}

/* Substring first, then at most six difflib-compatible scored name suggestions. */
array_t *cfg_calendar_search(sqlite_t *db, const string_t *text)
{
    array_t *exact = cfg_calendar_matches(db, text);
    if (!exact || array_size(exact))
        return exact;
    array_destroy(exact);
    string_t *name = NULL, *jurisdiction = NULL;
    bool ok = cfg_calendar_query_parts(text, &name, &jurisdiction) &&
              sqlite_exec_cstr(db, "DROP TABLE IF EXISTS temp.cfg_calendar_discovery;"
                                   "CREATE TEMP TABLE cfg_calendar_discovery(name TEXT PRIMARY KEY,score REAL);");
    sqlite_stmt_t *substring =
        ok ? sqlite_stmt_prepare(
                 db, "INSERT INTO cfg_calendar_discovery SELECT DISTINCT normalized,1 FROM cfg_calendar_places "
                     "WHERE instr(normalized,?1)>0")
           : NULL;
    ok = substring && sqlite_stmt_bind_text(substring, 1, string_c_str(name)) &&
         sqlite_stmt_step(substring) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(substring);
    sqlite_stmt_t *count = ok ? sqlite_stmt_prepare(db, "SELECT count(*) FROM cfg_calendar_discovery") : NULL;
    ok = count && sqlite_stmt_step(count) == SQLITE_STEP_ROW;
    bool fuzzy = ok && sqlite_stmt_column_int(count, 0) == 0 && string_byte_length(name);
    sqlite_stmt_finalize(count);
    if (fuzzy) {
        array_t *query = cfg_calendar_runes(name);
        size_t length = query ? array_size(query) : 0;
        bool *popular = calloc(length ? length : 1, sizeof(bool));
        /* Python's autojunk threshold applies only to long search strings. */
        if (query && popular && length >= 200) {
            for (size_t i = 0; i < length; ++i) {
                size_t occurrences = 0;
                for (size_t j = 0; j < length; ++j)
                    occurrences += *(uint32_t *)array_get(query, i) == *(uint32_t *)array_get(query, j);
                popular[i] = occurrences > length / 100 + 1;
            }
        }
        sqlite_stmt_t *read =
            query && popular
                ? sqlite_stmt_prepare(db, "SELECT DISTINCT normalized FROM cfg_calendar_places ORDER BY normalized")
                : NULL;
        sqlite_stmt_t *write =
            read ? sqlite_stmt_prepare(db, "INSERT INTO cfg_calendar_discovery VALUES(?1,?2)") : NULL;
        sqlite_step_result_t step = SQLITE_STEP_DONE;
        ok = read && write;
        while (ok && (step = sqlite_stmt_step(read)) == SQLITE_STEP_ROW) {
            string_t *candidate = string_new_with(sqlite_stmt_column_text(read, 0));
            double ratio = 0;
            ok = candidate && cfg_calendar_similarity(candidate, query, popular, &ratio);
            if (ok && ratio >= 0.6) {
                sqlite_stmt_reset(write);
                ok = sqlite_stmt_bind_text(write, 1, string_c_str(candidate)) &&
                     sqlite_stmt_bind_double(write, 2, ratio) && sqlite_stmt_step(write) == SQLITE_STEP_DONE;
            }
            string_free(candidate);
        }
        ok = ok && step == SQLITE_STEP_DONE;
        sqlite_stmt_finalize(write);
        sqlite_stmt_finalize(read);
        array_destroy(query);
        free(popular);
        ok = ok &&
             sqlite_exec_cstr(db, "DELETE FROM cfg_calendar_discovery WHERE name NOT IN "
                                  "(SELECT name FROM cfg_calendar_discovery ORDER BY score DESC,name DESC LIMIT 6)");
    }
    array_t *rows = ok ? cfg_calendar_selected(db, jurisdiction, true, NULL) : NULL;
    string_free(name);
    string_free(jurisdiction);
    return rows;
}
