/* Load holiday policy rows and evaluate their calendar or SQL date rules. */
#include "jurisdiction_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool text_equals(const char *left, const char *right)
{
    return left && right && strcmp(left, right) == 0;
}

void jurisdiction_free_holiday_rule_rows(jurisdiction_vec_t *rows)
{
    size_t i;
    holiday_rule_row_t *items = rows ? rows->items : NULL;

    if (!items)
        return;
    for (i = 0; i < rows->count; ++i) {
        free(items[i].holiday_name);
        free(items[i].holiday_class);
        free(items[i].rule_kind);
        free(items[i].holiday_date);
        free(items[i].expression_language);
        free(items[i].expression_text);
    }
    jurisdiction_vec_free(rows);
}

void jurisdiction_free_observance_rule_rows(jurisdiction_vec_t *rows)
{
    size_t i;
    observance_rule_row_t *items = rows ? rows->items : NULL;

    if (!items)
        return;
    for (i = 0; i < rows->count; ++i) {
        free(items[i].observed_rule_kind);
        free(items[i].observed_name);
        free(items[i].weekend_mask);
    }
    jurisdiction_vec_free(rows);
}

void jurisdiction_free_exception_rows(jurisdiction_vec_t *rows)
{
    size_t i;
    holiday_exception_row_t *items = rows ? rows->items : NULL;

    if (!items)
        return;
    for (i = 0; i < rows->count; ++i) {
        free(items[i].holiday_date);
        free(items[i].action);
        free(items[i].name);
    }
    jurisdiction_vec_free(rows);
}

static char *date_text_from_ymd(short year, month_t month, uint8_t day)
{
    datetime_t *dttm = datetime_init_ymd(datetime_alloc(), year, month, day);
    char *out;

    if (!dttm)
        return NULL;
    out = jurisdiction_format_date(dttm);
    datetime_dealloc(dttm);
    return out;
}

static char *easter_related_text(int year, int offset_days, bool orthodox)
{
    datetime_t *dttm =
        orthodox ? datetime_init_orthodox_easter(datetime_alloc(), year) : datetime_init_easter(datetime_alloc(), year);
    char *out;

    if (!dttm)
        return NULL;
    datetime_add_days(dttm, offset_days);
    out = jurisdiction_format_date(dttm);
    datetime_dealloc(dttm);
    return out;
}

static char *nth_weekday_of_month_text(int year, int month, int weekday, int ordinal)
{
    datetime_t *dttm = datetime_init_ymd(datetime_alloc(), (short)year, (month_t)month, 1u);
    int current_weekday;
    int delta;

    if (!dttm || ordinal < 1)
        return NULL;
    current_weekday = jurisdiction_iso_weekday_from_datetime_weekday((int)datetime_weekday(dttm));
    delta = weekday - current_weekday;
    if (delta < 0)
        delta += 7;
    datetime_add_days(dttm, delta + (ordinal - 1) * 7);

    if ((int)datetime_month(dttm) != month) {
        datetime_dealloc(dttm);
        return NULL;
    }

    {
        char *out = jurisdiction_format_date(dttm);
        datetime_dealloc(dttm);
        return out;
    }
}

static char *last_weekday_of_month_text(int year, int month, int weekday)
{
    datetime_t *dttm;
    int current_weekday;
    int delta;

    if (month < 1 || month > 12)
        return NULL;
    if (month == 12)
        dttm = datetime_init_ymd(datetime_alloc(), (short)(year + 1), DT_January, 1u);
    else
        dttm = datetime_init_ymd(datetime_alloc(), (short)year, (month_t)(month + 1), 1u);
    if (!dttm)
        return NULL;

    datetime_add_days(dttm, -1);
    current_weekday = jurisdiction_iso_weekday_from_datetime_weekday((int)datetime_weekday(dttm));
    delta = current_weekday - weekday;
    if (delta < 0)
        delta += 7;
    datetime_add_days(dttm, -delta);

    {
        char *out = jurisdiction_format_date(dttm);
        datetime_dealloc(dttm);
        return out;
    }
}

static char *weekday_after_date_text(int year, int month, int day, int weekday)
{
    datetime_t *dttm = datetime_init_ymd(datetime_alloc(), (short)year, (month_t)month, (uint8_t)day);
    int current_weekday;
    int delta;

    if (!dttm)
        return NULL;
    current_weekday = jurisdiction_iso_weekday_from_datetime_weekday((int)datetime_weekday(dttm));
    delta = weekday - current_weekday;
    if (delta < 0)
        delta += 7;
    datetime_add_days(dttm, delta);

    {
        char *out = jurisdiction_format_date(dttm);
        datetime_dealloc(dttm);
        return out;
    }
}

static char *weekday_before_date_text(int year, int month, int day, int weekday)
{
    datetime_t *dttm = datetime_init_ymd(datetime_alloc(), (short)year, (month_t)month, (uint8_t)day);
    int current_weekday;
    int delta;

    if (!dttm)
        return NULL;
    current_weekday = jurisdiction_iso_weekday_from_datetime_weekday((int)datetime_weekday(dttm));
    delta = current_weekday - weekday;
    if (delta < 0)
        delta += 7;
    datetime_add_days(dttm, -delta);

    {
        char *out = jurisdiction_format_date(dttm);
        datetime_dealloc(dttm);
        return out;
    }
}

static char *evaluate_sql_rule_date(sqlite_t *db, const holiday_rule_row_t *rule, int year, const char *jurisdiction)
{
    sqlite_stmt_t *stmt = NULL;
    char *sql = NULL;
    const char *date_text;
    char *out = NULL;
    sqlite_step_result_t rc;
    size_t sql_len;
    static const char prefix[] =
        "with holiday_rule_context(rule_year, jurisdiction_id, holiday_id, holiday_rule_id) as ("
        " select ?1, ?2, ?3, ?4"
        ") ";

    if (!db || !rule || !rule->expression_language || !rule->expression_text || !jurisdiction)
        return NULL;
    if (strcmp(rule->expression_language, "mars_sql") != 0)
        return NULL;

    sql_len = strlen(prefix) + strlen(rule->expression_text) + 1u;
    sql = malloc(sql_len);
    if (!sql)
        return NULL;
    snprintf(sql, sql_len, "%s%s", prefix, rule->expression_text);

    stmt = sqlite_stmt_prepare(db, sql);
    free(sql);
    if (!stmt)
        return NULL;
    if (!sqlite_stmt_bind_int(stmt, 1, year) || !sqlite_stmt_bind_text(stmt, 2, jurisdiction) ||
        !sqlite_stmt_bind_int(stmt, 3, rule->holiday_id) || !sqlite_stmt_bind_int(stmt, 4, rule->rule_id)) {
        sqlite_stmt_finalize(stmt);
        return NULL;
    }

    rc = sqlite_stmt_step(stmt);
    if (rc == SQLITE_STEP_ROW) {
        date_text = sqlite_stmt_column_text(stmt, 0);
        if (date_text)
            out = jurisdiction_dup_c_string(date_text);
    }
    sqlite_stmt_finalize(stmt);
    return out;
}

bool jurisdiction_load_holiday_rules(sqlite_t *db, const jurisdiction_vec_t *lineage_rows, int start_year, int end_year,
                               jurisdiction_vec_t *rules)
{
    static const char sql[] = "select hd.holiday_id, hr.holiday_rule_id, hd.jurisdiction_id, "
                              "       coalesce(hn.localized_name, hd.default_name), hd.holiday_class, "
                              "       hr.rule_kind, hr.month, hr.day, hr.weekday, hr.ordinal, hr.offset_days, "
                              "       hr.holiday_date, hr.expression_language, hr.expression_text, "
                              "       hr.valid_from_year, hr.valid_to_year "
                              "from holiday_definition hd "
                              "join holiday_rule hr on hr.holiday_id = hd.holiday_id "
                              "left join holiday_name hn on hn.holiday_id = hd.holiday_id and hn.is_primary = 'Y' "
                              "where hd.jurisdiction_id = ?1 "
                              "  and (hr.valid_from_year is null or hr.valid_from_year <= ?2) "
                              "  and (hr.valid_to_year is null or hr.valid_to_year >= ?3) "
                              "order by hd.holiday_id, hr.priority, hr.sequence_no;";
    sqlite_stmt_t *stmt = NULL;
    const lineage_row_t *lineage = lineage_rows ? lineage_rows->items : NULL;
    size_t i;

    if (!lineage)
        return false;
    stmt = sqlite_stmt_prepare(db, sql);
    if (!stmt)
        return false;

    for (i = 0; i < lineage_rows->count; ++i) {
        sqlite_step_result_t rc;

        sqlite_stmt_reset(stmt);
        sqlite_stmt_clear_bindings(stmt);
        if (!sqlite_stmt_bind_text(stmt, 1, lineage[i].jurisdiction_id) || !sqlite_stmt_bind_int(stmt, 2, end_year) ||
            !sqlite_stmt_bind_int(stmt, 3, start_year))
            goto fail;
        while ((rc = sqlite_stmt_step(stmt)) == SQLITE_STEP_ROW) {
            holiday_rule_row_t row;

            memset(&row, 0, sizeof(row));
            row.holiday_id = sqlite_stmt_column_int(stmt, 0);
            row.rule_id = sqlite_stmt_column_int(stmt, 1);
            snprintf(row.jurisdiction_id, sizeof(row.jurisdiction_id), "%s", sqlite_stmt_column_text(stmt, 2));
            row.holiday_name = jurisdiction_dup_c_string(sqlite_stmt_column_text(stmt, 3));
            row.holiday_class = jurisdiction_dup_c_string(sqlite_stmt_column_text(stmt, 4));
            row.rule_kind = jurisdiction_dup_c_string(sqlite_stmt_column_text(stmt, 5));
            row.month = sqlite_stmt_column_is_null(stmt, 6) ? 0 : sqlite_stmt_column_int(stmt, 6);
            row.day = sqlite_stmt_column_is_null(stmt, 7) ? 0 : sqlite_stmt_column_int(stmt, 7);
            row.weekday = sqlite_stmt_column_is_null(stmt, 8) ? 0 : sqlite_stmt_column_int(stmt, 8);
            row.ordinal = sqlite_stmt_column_is_null(stmt, 9) ? 0 : sqlite_stmt_column_int(stmt, 9);
            row.offset_days = sqlite_stmt_column_is_null(stmt, 10) ? 0 : sqlite_stmt_column_int(stmt, 10);
            row.holiday_date = jurisdiction_dup_c_string(sqlite_stmt_column_text(stmt, 11));
            row.expression_language = jurisdiction_dup_c_string(sqlite_stmt_column_text(stmt, 12));
            row.expression_text = jurisdiction_dup_c_string(sqlite_stmt_column_text(stmt, 13));
            row.valid_from_year = sqlite_stmt_column_is_null(stmt, 14) ? 0 : sqlite_stmt_column_int(stmt, 14);
            row.valid_to_year = sqlite_stmt_column_is_null(stmt, 15) ? 0 : sqlite_stmt_column_int(stmt, 15);
            if (!row.holiday_name || !row.holiday_class || !row.rule_kind || !jurisdiction_vec_push(rules, &row))
                goto fail;
        }
        if (rc != SQLITE_STEP_DONE)
            goto fail;
    }

    sqlite_stmt_finalize(stmt);
    return true;

fail:
    sqlite_stmt_finalize(stmt);
    return false;
}

bool jurisdiction_load_observance_rules(sqlite_t *db, const jurisdiction_vec_t *lineage_rows, jurisdiction_vec_t *observances)
{
    static const char sql[] = "select hor.holiday_id, hor.holiday_rule_id, hor.observed_rule_kind, hor.observed_name, "
                              "       hor.weekend_mask, hor.suppress_original, hor.valid_from_year, hor.valid_to_year "
                              "from holiday_observance_rule hor "
                              "join holiday_definition hd on hd.holiday_id = hor.holiday_id "
                              "where hd.jurisdiction_id = ?1 "
                              "order by hor.holiday_id, hor.priority;";
    sqlite_stmt_t *stmt = NULL;
    const lineage_row_t *lineage = lineage_rows ? lineage_rows->items : NULL;
    size_t i;

    if (!lineage)
        return false;
    stmt = sqlite_stmt_prepare(db, sql);
    if (!stmt)
        return false;

    for (i = 0; i < lineage_rows->count; ++i) {
        sqlite_step_result_t rc;

        sqlite_stmt_reset(stmt);
        sqlite_stmt_clear_bindings(stmt);
        if (!sqlite_stmt_bind_text(stmt, 1, lineage[i].jurisdiction_id))
            goto fail;
        while ((rc = sqlite_stmt_step(stmt)) == SQLITE_STEP_ROW) {
            observance_rule_row_t row;

            memset(&row, 0, sizeof(row));
            row.holiday_id = sqlite_stmt_column_int(stmt, 0);
            row.applies_to_rule_id = sqlite_stmt_column_is_null(stmt, 1) ? 0 : sqlite_stmt_column_int(stmt, 1);
            row.observed_rule_kind = jurisdiction_dup_c_string(sqlite_stmt_column_text(stmt, 2));
            row.observed_name = jurisdiction_dup_c_string(sqlite_stmt_column_text(stmt, 3));
            row.weekend_mask = jurisdiction_normalise_weekend_mask(sqlite_stmt_column_text(stmt, 4));
            row.suppress_original = text_equals(sqlite_stmt_column_text(stmt, 5), "Y");
            row.valid_from_year = sqlite_stmt_column_is_null(stmt, 6) ? 0 : sqlite_stmt_column_int(stmt, 6);
            row.valid_to_year = sqlite_stmt_column_is_null(stmt, 7) ? 0 : sqlite_stmt_column_int(stmt, 7);
            if (!row.observed_rule_kind || !row.weekend_mask || !jurisdiction_vec_push(observances, &row))
                goto fail;
        }
        if (rc != SQLITE_STEP_DONE)
            goto fail;
    }

    sqlite_stmt_finalize(stmt);
    return true;

fail:
    sqlite_stmt_finalize(stmt);
    return false;
}

bool jurisdiction_load_exceptions(sqlite_t *db, const jurisdiction_vec_t *lineage_rows, jurisdiction_vec_t *exceptions)
{
    static const char sql[] =
        "select holiday_id, holiday_rule_id, holiday_date, action, name, valid_from_year, valid_to_year "
        "from holiday_exception where jurisdiction_id = ?1 order by holiday_date, priority;";
    sqlite_stmt_t *stmt = NULL;
    const lineage_row_t *lineage = lineage_rows ? lineage_rows->items : NULL;
    size_t i;

    if (!lineage)
        return false;
    stmt = sqlite_stmt_prepare(db, sql);
    if (!stmt)
        return false;

    for (i = 0; i < lineage_rows->count; ++i) {
        sqlite_step_result_t rc;

        sqlite_stmt_reset(stmt);
        sqlite_stmt_clear_bindings(stmt);
        if (!sqlite_stmt_bind_text(stmt, 1, lineage[i].jurisdiction_id))
            goto fail;
        while ((rc = sqlite_stmt_step(stmt)) == SQLITE_STEP_ROW) {
            holiday_exception_row_t row;

            memset(&row, 0, sizeof(row));
            row.holiday_id = sqlite_stmt_column_is_null(stmt, 0) ? 0 : sqlite_stmt_column_int(stmt, 0);
            row.target_rule_id = sqlite_stmt_column_is_null(stmt, 1) ? 0 : sqlite_stmt_column_int(stmt, 1);
            row.holiday_date = jurisdiction_dup_c_string(sqlite_stmt_column_text(stmt, 2));
            row.action = jurisdiction_dup_c_string(sqlite_stmt_column_text(stmt, 3));
            row.name = jurisdiction_dup_c_string(sqlite_stmt_column_text(stmt, 4));
            row.valid_from_year = sqlite_stmt_column_is_null(stmt, 5) ? 0 : sqlite_stmt_column_int(stmt, 5);
            row.valid_to_year = sqlite_stmt_column_is_null(stmt, 6) ? 0 : sqlite_stmt_column_int(stmt, 6);
            if (!row.holiday_date || !row.action || !jurisdiction_vec_push(exceptions, &row))
                goto fail;
        }
        if (rc != SQLITE_STEP_DONE)
            goto fail;
    }

    sqlite_stmt_finalize(stmt);
    return true;

fail:
    sqlite_stmt_finalize(stmt);
    return false;
}

char *jurisdiction_evaluate_rule_date(sqlite_t *db, const holiday_rule_row_t *rule, int year, const char *jurisdiction)
{
    if (!rule || !rule->rule_kind || !jurisdiction_year_in_range(year, rule->valid_from_year, rule->valid_to_year))
        return NULL;

    if (strcmp(rule->rule_kind, "fixed_date") == 0)
        return date_text_from_ymd((short)year, (month_t)rule->month, (uint8_t)rule->day);
    if (strcmp(rule->rule_kind, "one_off") == 0) {
        if (rule->holiday_date && atoi(rule->holiday_date) == year)
            return jurisdiction_dup_c_string(rule->holiday_date);
        return NULL;
    }
    if (strcmp(rule->rule_kind, "easter_offset") == 0)
        return easter_related_text(year, rule->offset_days, false);
    if (strcmp(rule->rule_kind, "orthodox_easter_offset") == 0)
        return easter_related_text(year, rule->offset_days, true);
    if (strcmp(rule->rule_kind, "nth_weekday") == 0 && rule->ordinal > 0)
        return nth_weekday_of_month_text(year, rule->month, rule->weekday, rule->ordinal);
    if (strcmp(rule->rule_kind, "last_weekday") == 0)
        return last_weekday_of_month_text(year, rule->month, rule->weekday);
    if (strcmp(rule->rule_kind, "weekday_after_date") == 0)
        return weekday_after_date_text(year, rule->month, rule->day, rule->weekday);
    if (strcmp(rule->rule_kind, "weekday_before_date") == 0)
        return weekday_before_date_text(year, rule->month, rule->day, rule->weekday);
    if (strcmp(rule->rule_kind, "algorithmic") == 0 || strcmp(rule->rule_kind, "rrule") == 0)
        return evaluate_sql_rule_date(db, rule, year, jurisdiction);

    return NULL;
}
