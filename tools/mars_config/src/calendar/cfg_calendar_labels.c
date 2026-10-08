/**
 * @file cfg_calendar_labels.c
 * @brief Indexed holiday-name translations with inherited and exact-event precedence.
 *
 * Definition names, original primary names and known qualification suffixes are
 * expanded once. Exact event translations override those entries. Unrecognised
 * event names remain untouched when the snapshot collector performs its lookup.
 */
#include "cfg_calendar_internal.h"

static bool cfg_calendar_label_query(sqlite_t *db, const char *sql, const string_t *jurisdiction,
                                     const string_t *locale, const string_t *language)
{
    sqlite_stmt_t *stmt = sqlite_stmt_prepare(db, sql);
    bool ok = stmt && sqlite_stmt_bind_text(stmt, 1, string_c_str(jurisdiction)) &&
              sqlite_stmt_bind_text(stmt, 2, string_c_str(locale)) &&
              sqlite_stmt_bind_text(stmt, 3, string_c_str(language)) && sqlite_stmt_step(stmt) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(stmt);
    return ok;
}

/* Build connection-local indexes; precedence is determined before event expansion. */
bool cfg_calendar_labels(sqlite_t *db, const string_t *jurisdiction, const string_t *locale)
{
    bool ok = sqlite_exec_cstr(
        db, "DROP TABLE IF EXISTS temp.cfg_calendar_labels; DROP TABLE IF EXISTS temp.cfg_calendar_label_rows;"
            "DROP TABLE IF EXISTS temp.cfg_calendar_suffixes; DROP TABLE IF EXISTS temp.cfg_calendar_label_base;"
            "CREATE TEMP TABLE cfg_calendar_labels(holiday INTEGER,source TEXT,translated TEXT,"
            "PRIMARY KEY(holiday,source));");
    if (!ok || !locale || !string_byte_length(locale))
        return ok;
    string_t *normal = cfg_calendar_key(locale, true);
    string_cursor_t *cursor = normal ? string_cursor_new(normal) : NULL;
    string_pos_t start = cursor ? string_cursor_position(cursor) : (string_pos_t){0};
    while (cursor && !string_cursor_done(cursor) && rune_value(string_cursor_peek(cursor)) != '_')
        string_cursor_next(cursor);
    string_t *language = cursor ? string_cursor_extract(start, cursor) : NULL;
    string_cursor_free(cursor);
    ok = normal && language &&
         sqlite_exec_cstr(
             db, "CREATE TEMP TABLE cfg_calendar_label_rows(holiday INTEGER,default_name TEXT,source_name TEXT,"
                 "translated TEXT); CREATE TEMP TABLE cfg_calendar_suffixes(source TEXT PRIMARY KEY,translated TEXT);");
    if (ok)
        ok = cfg_calendar_label_query(
            db,
            "WITH RECURSIVE lineage(jurisdiction_id) AS (SELECT ?1 UNION "
            "SELECT p.parent_jurisdiction_id FROM jurisdiction_parent_jurisdiction_id p "
            "JOIN lineage l ON l.jurisdiction_id=p.jurisdiction_id) "
            "INSERT INTO cfg_calendar_label_rows "
            "SELECT h.holiday_id,h.default_name,o.localized_name,n.localized_name FROM lineage "
            "JOIN holiday_definition h USING(jurisdiction_id) JOIN holiday_name n USING(holiday_id) "
            "LEFT JOIN holiday_name o ON o.holiday_id=h.holiday_id AND o.is_primary='Y' "
            "WHERE lower(replace(n.locale,'-','_')) IN(?2,?3) "
            "ORDER BY CASE WHEN lower(replace(n.locale,'-','_'))=?2 THEN 0 ELSE 1 END,"
            "n.is_primary DESC,n.holiday_name_id",
            jurisdiction, normal, language);
    if (ok)
        ok =
            sqlite_exec_cstr(db, "INSERT OR IGNORE INTO cfg_calendar_labels "
                                 "SELECT holiday,original,translated FROM ("
                                 "SELECT rowid AS priority,0 AS spelling,holiday,default_name AS original,translated "
                                 "FROM cfg_calendar_label_rows UNION ALL SELECT rowid,1,holiday,source_name,translated "
                                 "FROM cfg_calendar_label_rows) WHERE original IS NOT NULL AND original<>'' "
                                 "AND translated IS NOT NULL AND translated<>'' ORDER BY priority,spelling;");
    if (ok)
        ok = cfg_calendar_label_query(
            db,
            "INSERT OR IGNORE INTO cfg_calendar_suffixes SELECT source_suffix,localized_suffix "
            "FROM holiday_name_qualifier WHERE lower(replace(locale,'-','_')) IN(?2,?3) AND ?1 IS NOT NULL "
            "ORDER BY CASE WHEN lower(replace(locale,'-','_'))=?2 THEN 0 ELSE 1 END,source_suffix",
            jurisdiction, normal, language);
    if (ok)
        ok = sqlite_exec_cstr(
            db, "CREATE TEMP TABLE cfg_calendar_label_base AS SELECT * FROM cfg_calendar_labels;"
                "INSERT OR IGNORE INTO cfg_calendar_labels "
                "SELECT b.holiday,b.source||s.source,b.translated||s.translated "
                "FROM cfg_calendar_label_base b CROSS JOIN cfg_calendar_suffixes s ORDER BY b.rowid,s.rowid;");
    if (ok)
        ok = cfg_calendar_label_query(
            db,
            "WITH RECURSIVE lineage(jurisdiction_id) AS (SELECT ?1 UNION "
            "SELECT p.parent_jurisdiction_id FROM jurisdiction_parent_jurisdiction_id p "
            "JOIN lineage l ON l.jurisdiction_id=p.jurisdiction_id) "
            "INSERT OR REPLACE INTO cfg_calendar_labels SELECT n.holiday_id,n.source_name,n.localized_name "
            "FROM lineage JOIN holiday_definition USING(jurisdiction_id) "
            "JOIN holiday_event_localized_name n USING(holiday_id) "
            "WHERE lower(replace(n.locale,'-','_')) IN(?2,?3) "
            "ORDER BY CASE WHEN lower(replace(n.locale,'-','_'))=?2 THEN 1 ELSE 0 END",
            jurisdiction, normal, language);
    string_free(normal);
    string_free(language);
    return ok;
}
