/**
 * @file jurisdiction_timezone.c
 * @brief Timezone eras and effective GMT offsets.
 *
 * Loads timezone histories and named transition rules and resolves offsets for civil dates. This supports default
 * local-time policy and the transition-reporting routines.
 *
 * This is part of jurisdiction.h and uses configured rule data. Results depend on that data's coverage and
 * currency rather than hard-coded assumptions about the host machine.
 */

/* Load timezone eras and named rules, and resolve civil-date GMT offsets. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jurisdiction_internal.h"

void jurisdiction_free_timezone_era_rows(jurisdiction_vec_t *rows)
{
    size_t i;
    timezone_era_row_t *items = rows ? rows->items : NULL;

    if (!items)
        return;
    for (i = 0u; i < rows->count; ++i) {
        free(items[i].rules_kind);
        free(items[i].rule_name);
        free(items[i].until_day_kind);
    }
    jurisdiction_vec_free(rows);
}

void jurisdiction_free_timezone_transition_rule_rows(jurisdiction_vec_t *rows)
{
    size_t i;
    timezone_transition_rule_row_t *items = rows ? rows->items : NULL;

    if (!items)
        return;
    for (i = 0u; i < rows->count; ++i) {
        free(items[i].rule_name);
        free(items[i].on_kind);
    }
    jurisdiction_vec_free(rows);
}

bool jurisdiction_load_default_timezone_name(sqlite_t *db, const jurisdiction_vec_t *lineage_rows, char *timezone_name,
                                       size_t timezone_name_size)
{
    static const char sql[] = "select code.timezone_name "
                              "from jurisdiction_location_default as loc "
                              "join jurisdiction_location_default_timezone as tz "
                              "  on tz.jurisdiction_id = loc.jurisdiction_id "
                              "join timezone_code as code "
                              "  on code.timezone_code = tz.timezone_code "
                              "where loc.jurisdiction_id = ?1;";
    sqlite_stmt_t *stmt = NULL;
    const lineage_row_t *lineage = lineage_rows ? lineage_rows->items : NULL;
    size_t i;
    bool found = false;

    if (!lineage || !timezone_name || timezone_name_size == 0u)
        return false;
    stmt = sqlite_stmt_prepare(db, sql);
    if (!stmt)
        return false;

    timezone_name[0] = '\0';
    for (i = 0; i < lineage_rows->count; ++i) {
        const char *tz_text;
        sqlite_step_result_t rc;

        sqlite_stmt_reset(stmt);
        sqlite_stmt_clear_bindings(stmt);
        if (!sqlite_stmt_bind_text(stmt, 1, lineage[i].jurisdiction_id))
            goto done;
        rc = sqlite_stmt_step(stmt);
        if (rc != SQLITE_STEP_ROW)
            continue;
        tz_text = sqlite_stmt_column_text(stmt, 0);
        if (!tz_text || *tz_text == '\0')
            goto done;
        snprintf(timezone_name, timezone_name_size, "%s", tz_text);
        found = true;
        break;
    }

done:
    sqlite_stmt_finalize(stmt);
    return found;
}

bool jurisdiction_load_timezone_eras(sqlite_t *db, const char *timezone_name, jurisdiction_vec_t *rows)
{
    static const char canonical_sql[] = "select canonical_timezone_name "
                                        "from timezone_canonical where timezone_name = ?1;";
    static const char sql[] = "select sequence_no, gmtoff_minutes, rules_kind, fixed_save_minutes, rule_name, "
                              "       until_year, until_month, until_day_kind, until_day_value, "
                              "       until_weekday, until_seconds, until_suffix "
                              "from timezone_era where timezone_name = ?1 order by sequence_no;";
    sqlite_stmt_t *stmt = NULL;
    sqlite_stmt_t *canonical_stmt = NULL;
    const char *query_timezone_name = timezone_name;
    char canonical_timezone_name[128];
    sqlite_step_result_t rc = SQLITE_STEP_DONE;
    bool retried_with_canonical = false;

    if (!db || !timezone_name || !*timezone_name || !rows)
        return false;
    canonical_timezone_name[0] = '\0';

retry:
    stmt = sqlite_stmt_prepare(db, sql);
    if (!stmt)
        return false;
    if (!sqlite_stmt_bind_text(stmt, 1, query_timezone_name))
        goto fail;

    while ((rc = sqlite_stmt_step(stmt)) == SQLITE_STEP_ROW) {
        timezone_era_row_t row;

        memset(&row, 0, sizeof(row));
        row.sequence_no = sqlite_stmt_column_int(stmt, 0);
        row.gmtoff_minutes = sqlite_stmt_column_int(stmt, 1);
        row.rules_kind = jurisdiction_dup_c_string(sqlite_stmt_column_text(stmt, 2));
        row.fixed_save_minutes = sqlite_stmt_column_is_null(stmt, 3) ? 0 : sqlite_stmt_column_int(stmt, 3);
        row.rule_name = jurisdiction_dup_c_string(sqlite_stmt_column_text(stmt, 4));
        row.until_year = sqlite_stmt_column_is_null(stmt, 5) ? 0 : sqlite_stmt_column_int(stmt, 5);
        row.until_month = sqlite_stmt_column_is_null(stmt, 6) ? 0 : sqlite_stmt_column_int(stmt, 6);
        row.until_day_kind = jurisdiction_dup_c_string(sqlite_stmt_column_text(stmt, 7));
        row.until_day_value = sqlite_stmt_column_is_null(stmt, 8) ? 0 : sqlite_stmt_column_int(stmt, 8);
        row.until_weekday = sqlite_stmt_column_is_null(stmt, 9) ? 0 : sqlite_stmt_column_int(stmt, 9);
        row.until_seconds = sqlite_stmt_column_is_null(stmt, 10) ? 0 : sqlite_stmt_column_int(stmt, 10);
        row.until_suffix = sqlite_stmt_column_is_null(stmt, 11) ? '\0' : sqlite_stmt_column_text(stmt, 11)[0];
        if (!row.rules_kind || !jurisdiction_vec_push(rows, &row)) {
            free(row.rules_kind);
            free(row.rule_name);
            free(row.until_day_kind);
            goto fail;
        }
    }

    sqlite_stmt_finalize(stmt);
    stmt = NULL;
    if (rows->count == 0u && !retried_with_canonical) {
        canonical_stmt = sqlite_stmt_prepare(db, canonical_sql);
        if (!canonical_stmt)
            return false;
        if (!sqlite_stmt_bind_text(canonical_stmt, 1, timezone_name))
            goto fail;
        if (sqlite_stmt_step(canonical_stmt) == SQLITE_STEP_ROW) {
            const char *canonical_text = sqlite_stmt_column_text(canonical_stmt, 0);

            if (canonical_text && *canonical_text) {
                snprintf(canonical_timezone_name, sizeof(canonical_timezone_name), "%s", canonical_text);
                if (strcmp(canonical_timezone_name, timezone_name) != 0) {
                    query_timezone_name = canonical_timezone_name;
                    retried_with_canonical = true;
                }
            }
        }
        sqlite_stmt_finalize(canonical_stmt);
        canonical_stmt = NULL;
        if (retried_with_canonical)
            goto retry;
    }
    return rc == SQLITE_STEP_DONE;

fail:
    sqlite_stmt_finalize(stmt);
    sqlite_stmt_finalize(canonical_stmt);
    return false;
}

bool jurisdiction_load_timezone_transition_rules(sqlite_t *db, const char *rule_name, jurisdiction_vec_t *rows)
{
    static const char sql[] = "select rule_name, from_year, to_year, in_month, on_kind, on_day, on_weekday, "
                              "       at_seconds, at_suffix, save_minutes "
                              "from timezone_transition_rule where rule_name = ?1 "
                              "order by coalesce(from_year, -999999), coalesce(to_year, 999999), in_month, on_day;";
    sqlite_stmt_t *stmt = NULL;
    sqlite_step_result_t rc;

    if (!db || !rule_name || !*rule_name || !rows)
        return false;
    stmt = sqlite_stmt_prepare(db, sql);
    if (!stmt)
        return false;
    if (!sqlite_stmt_bind_text(stmt, 1, rule_name))
        goto fail;

    while ((rc = sqlite_stmt_step(stmt)) == SQLITE_STEP_ROW) {
        timezone_transition_rule_row_t row;

        memset(&row, 0, sizeof(row));
        row.rule_name = jurisdiction_dup_c_string(sqlite_stmt_column_text(stmt, 0));
        row.from_year = sqlite_stmt_column_is_null(stmt, 1) ? 0 : sqlite_stmt_column_int(stmt, 1);
        row.to_year = sqlite_stmt_column_is_null(stmt, 2) ? 0 : sqlite_stmt_column_int(stmt, 2);
        row.in_month = sqlite_stmt_column_int(stmt, 3);
        row.on_kind = jurisdiction_dup_c_string(sqlite_stmt_column_text(stmt, 4));
        row.on_day = sqlite_stmt_column_int(stmt, 5);
        row.on_weekday = sqlite_stmt_column_is_null(stmt, 6) ? 0 : sqlite_stmt_column_int(stmt, 6);
        row.at_seconds = sqlite_stmt_column_int(stmt, 7);
        row.at_suffix = sqlite_stmt_column_is_null(stmt, 8) ? 'w' : sqlite_stmt_column_text(stmt, 8)[0];
        row.save_minutes = sqlite_stmt_column_int(stmt, 9);
        if (!row.rule_name || !row.on_kind || !jurisdiction_vec_push(rows, &row)) {
            free(row.rule_name);
            free(row.on_kind);
            goto fail;
        }
    }

    sqlite_stmt_finalize(stmt);
    return rc == SQLITE_STEP_DONE;

fail:
    sqlite_stmt_finalize(stmt);
    return false;
}

static bool resolve_month_day(short year, month_t month, const char *day_kind, int day_value, int weekday,
                              uint8_t *resolved_day)
{
    datetime_t *probe = NULL;
    int target_day = day_value;
    weekday_t current_weekday;
    int delta;
    bool ok = false;

    if (!resolved_day || !datetime_valid_ymd(year, month, 1))
        return false;

    if (!day_kind || strcmp(day_kind, "day_of_month") == 0) {
        target_day = day_value;
    } else if (strcmp(day_kind, "last_weekday") == 0) {
        target_day = (int)datetime_days_in_month(year, month);
        probe = datetime_init_ymd(datetime_alloc(), year, month, (uint8_t)target_day);
        if (!probe)
            goto done;
        current_weekday = datetime_weekday(probe);
        delta = ((int)current_weekday - weekday + 7) % 7;
        target_day -= delta;
    } else if (strcmp(day_kind, "weekday_on_or_after") == 0) {
        target_day = day_value;
        if (!datetime_valid_ymd(year, month, (uint8_t)target_day) ||
            !(probe = datetime_init_ymd(datetime_alloc(), year, month, (uint8_t)target_day)))
            goto done;
        current_weekday = datetime_weekday(probe);
        delta = (weekday - (int)current_weekday + 7) % 7;
        target_day += delta;
    } else if (strcmp(day_kind, "weekday_on_or_before") == 0) {
        target_day = day_value;
        if (!datetime_valid_ymd(year, month, (uint8_t)target_day) ||
            !(probe = datetime_init_ymd(datetime_alloc(), year, month, (uint8_t)target_day)))
            goto done;
        current_weekday = datetime_weekday(probe);
        delta = ((int)current_weekday - weekday + 7) % 7;
        target_day -= delta;
    } else {
        goto done;
    }

    if (!datetime_valid_ymd(year, month, (uint8_t)target_day))
        goto done;
    *resolved_day = (uint8_t)target_day;
    ok = true;

done:
    datetime_dealloc(probe);
    return ok;
}

bool jurisdiction_normalise_day_time(short *year, month_t *month, uint8_t *day, int *seconds)
{
    datetime_t *date = NULL;
    long shift_days = 0;
    bool ok = false;

    if (!year || !month || !day || !seconds)
        return false;
    date = datetime_init_ymd(datetime_alloc(), *year, *month, *day);
    if (!date)
        goto done;

    while (*seconds < 0) {
        *seconds += 86400;
        shift_days -= 1;
    }
    while (*seconds >= 86400) {
        *seconds -= 86400;
        shift_days += 1;
    }
    if (shift_days != 0) {
        if (!datetime_add_days(date, shift_days))
            goto done;
        *year = datetime_year(date);
        *month = datetime_month(date);
        *day = datetime_day(date);
    }
    ok = true;

done:
    datetime_dealloc(date);
    return ok;
}

static int compare_local_noon_to_boundary(const datetime_t *date, short boundary_year, month_t boundary_month,
                                          uint8_t boundary_day, int boundary_seconds)
{
    datetime_t *boundary = NULL;
    int cmp;

    if (!date)
        return 0;
    boundary = datetime_init_ymd(datetime_alloc(), boundary_year, boundary_month, boundary_day);
    if (!boundary)
        return 0;
    cmp = datetime_compare(date, boundary);
    datetime_dealloc(boundary);
    if (cmp != 0)
        return cmp;
    if (12 * 3600 < boundary_seconds)
        return -1;
    if (12 * 3600 > boundary_seconds)
        return 1;
    return 0;
}

const timezone_era_row_t *jurisdiction_select_timezone_era_for_date(const jurisdiction_vec_t *eras, const datetime_t *date)
{
    const timezone_era_row_t *items = eras ? eras->items : NULL;
    size_t i;

    if (!items || !date)
        return NULL;
    for (i = 0u; i < eras->count; ++i) {
        short boundary_year;
        month_t boundary_month;
        uint8_t boundary_day;
        int cmp;

        if (items[i].until_year == 0)
            return &items[i];
        boundary_year = (short)items[i].until_year;
        boundary_month = (month_t)(items[i].until_month ? items[i].until_month : 1);
        if (!resolve_month_day(
                boundary_year, boundary_month, items[i].until_day_kind ? items[i].until_day_kind : "day_of_month",
                items[i].until_day_value ? items[i].until_day_value : 1, items[i].until_weekday, &boundary_day))
            return NULL;
        cmp = compare_local_noon_to_boundary(date, boundary_year, boundary_month, boundary_day, items[i].until_seconds);
        if (cmp < 0)
            return &items[i];
    }
    return eras->count ? &items[eras->count - 1u] : NULL;
}

bool jurisdiction_compute_transition_occurrence(const timezone_transition_rule_row_t *rule, int year, short *out_year,
                                          month_t *out_month, uint8_t *out_day, int *out_seconds)
{
    uint8_t day;
    short rule_year;
    month_t rule_month;
    int rule_seconds;

    if (!rule || !out_year || !out_month || !out_day || !out_seconds)
        return false;
    if (!jurisdiction_year_in_range(year, rule->from_year, rule->to_year))
        return false;

    rule_year = (short)year;
    rule_month = (month_t)rule->in_month;
    if (!resolve_month_day(rule_year, rule_month, rule->on_kind ? rule->on_kind : "day_of_month", rule->on_day,
                           rule->on_weekday, &day))
        return false;

    rule_seconds = rule->at_seconds;
    if (!jurisdiction_normalise_day_time(&rule_year, &rule_month, &day, &rule_seconds))
        return false;

    *out_year = rule_year;
    *out_month = rule_month;
    *out_day = day;
    *out_seconds = rule_seconds;
    return true;
}

int jurisdiction_compare_boundary_to_boundary(short left_year, month_t left_month, uint8_t left_day, int left_seconds,
                                        short right_year, month_t right_month, uint8_t right_day, int right_seconds)
{
    datetime_t *left = NULL;
    datetime_t *right = NULL;
    int cmp;

    left = datetime_init_ymd(datetime_alloc(), left_year, left_month, left_day);
    right = datetime_init_ymd(datetime_alloc(), right_year, right_month, right_day);
    if (!left || !right) {
        datetime_dealloc(right);
        datetime_dealloc(left);
        return 0;
    }
    cmp = datetime_compare(left, right);
    datetime_dealloc(right);
    datetime_dealloc(left);
    if (cmp != 0)
        return cmp;
    if (left_seconds < right_seconds)
        return -1;
    if (left_seconds > right_seconds)
        return 1;
    return 0;
}

static bool resolve_named_rule_save_minutes(sqlite_t *db, const char *rule_name, const datetime_t *date,
                                            int *save_minutes)
{
    jurisdiction_vec_t rule_rows = {0};
    const timezone_transition_rule_row_t *items;
    int probe_years[2];
    size_t i;
    bool found = false;
    short best_year = 0;
    month_t best_month = 0;
    uint8_t best_day = 0;
    int best_seconds = 0;
    int best_save = 0;

    if (!db || !rule_name || !*rule_name || !date || !save_minutes)
        return false;

    rule_rows.item_size = sizeof(timezone_transition_rule_row_t);
    if (!jurisdiction_load_timezone_transition_rules(db, rule_name, &rule_rows))
        return false;
    items = rule_rows.items;
    probe_years[0] = (int)datetime_year(date) - 1;
    probe_years[1] = (int)datetime_year(date);

    for (i = 0u; i < rule_rows.count; ++i) {
        size_t year_index;

        for (year_index = 0u; year_index < 2u; ++year_index) {
            short occurrence_year;
            month_t occurrence_month;
            uint8_t occurrence_day;
            int occurrence_seconds;
            int candidate_cmp;

            if (!jurisdiction_compute_transition_occurrence(&items[i], probe_years[year_index], &occurrence_year,
                                                           &occurrence_month, &occurrence_day, &occurrence_seconds))
                continue;

            candidate_cmp = compare_local_noon_to_boundary(date, occurrence_year, occurrence_month, occurrence_day,
                                                           occurrence_seconds);
            if (candidate_cmp < 0)
                continue;

            if (!found ||
                jurisdiction_compare_boundary_to_boundary(occurrence_year, occurrence_month, occurrence_day, occurrence_seconds,
                                             best_year, best_month, best_day, best_seconds) > 0) {
                found = true;
                best_year = occurrence_year;
                best_month = occurrence_month;
                best_day = occurrence_day;
                best_seconds = occurrence_seconds;
                best_save = items[i].save_minutes;
            }
        }
    }

    jurisdiction_free_timezone_transition_rule_rows(&rule_rows);
    *save_minutes = found ? best_save : 0;
    return true;
}

bool jurisdiction_timezone_offset_for_name_on_date(sqlite_t *db, const char *timezone_name, const datetime_t *date,
                                             double *offset_hours)
{
    jurisdiction_vec_t era_rows = {0};
    const timezone_era_row_t *era;
    int save_minutes = 0;

    if (!db || !timezone_name || !*timezone_name || !date || !offset_hours)
        return false;

    era_rows.item_size = sizeof(timezone_era_row_t);
    if (!jurisdiction_load_timezone_eras(db, timezone_name, &era_rows))
        return false;
    era = jurisdiction_select_timezone_era_for_date(&era_rows, date);
    if (!era) {
        jurisdiction_free_timezone_era_rows(&era_rows);
        return false;
    }

    if (strcmp(era->rules_kind, "fixed") == 0) {
        save_minutes = era->fixed_save_minutes;
    } else if (strcmp(era->rules_kind, "named") == 0) {
        if (!resolve_named_rule_save_minutes(db, era->rule_name, date, &save_minutes)) {
            jurisdiction_free_timezone_era_rows(&era_rows);
            return false;
        }
    } else {
        save_minutes = 0;
    }

    *offset_hours = (double)(era->gmtoff_minutes + save_minutes) / 60.0;
    jurisdiction_free_timezone_era_rows(&era_rows);
    return true;
}

/* Resolve the jurisdiction's GMT offset for the supplied civil date. */
bool jurisdict_default_gmt_offset(jurisdiction_t *holiday, const datetime_t *date, double *offset_hours)
{
    sqlite_t *db = holiday ? holiday->db : NULL;
    const char *jurisdiction = holiday ? holiday->jurisdiction : NULL;
    jurisdiction_vec_t lineage_rows = {0};
    char timezone_name[128];
    bool ok;

    lineage_rows.item_size = sizeof(lineage_row_t);

    if (!holiday || !db || !jurisdiction || *jurisdiction == '\0' || !date || !offset_hours) {
        jurisdiction_set_error(holiday, "invalid holiday query");
        return false;
    }
    if (!jurisdiction_load_lineage(db, jurisdiction, &lineage_rows)) {
        jurisdiction_set_error(holiday, "failed to load jurisdiction lineage");
        jurisdiction_vec_free(&lineage_rows);
        return false;
    }
    ok = jurisdiction_load_default_timezone_name(db, &lineage_rows, timezone_name, sizeof(timezone_name));
    jurisdiction_vec_free(&lineage_rows);
    if (!ok) {
        jurisdiction_set_error(holiday, "default jurisdiction timezone unavailable");
        return false;
    }
    if (!jurisdiction_timezone_offset_for_name_on_date(db, timezone_name, date, offset_hours)) {
        jurisdiction_set_error(holiday, "failed to resolve jurisdiction timezone offset");
        return false;
    }
    return true;
}
