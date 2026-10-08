/**
 * @file jurisdiction_calendar.c
 * @brief Weekend inheritance and working-day queries.
 *
 * Supplies civil-date helpers and combines weekend policy with holiday occurrences to classify and count working
 * days. Calendar arithmetic alone cannot determine these jurisdiction-dependent results.
 *
 * This is part of jurisdiction.h and uses configured rule data. Results depend on that data's coverage and
 * currency rather than hard-coded assumptions about the host machine.
 */

/* Civil-date helpers, inherited weekend policy and working-day queries. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jurisdiction_internal.h"

bool jurisdiction_parse_date_text(const char *text, short *year, month_t *month, uint8_t *day)
{
    int y;
    int m;
    int d;
    char tail;

    if (!text || !year || !month || !day)
        return false;
    if (sscanf(text, "%d-%d-%d%c", &y, &m, &d, &tail) != 3)
        return false;
    if (y < 1 || y > 9999 || m < 1 || m > 12 || d < 1 || d > 31)
        return false;
    if (!datetime_valid_ymd((short)y, (month_t)m, (uint8_t)d))
        return false;

    *year = (short)y;
    *month = (month_t)m;
    *day = (uint8_t)d;
    return true;
}

char *jurisdiction_format_date(const datetime_t *dttm)
{
    string_t *format = string_new_with("%yyyy-%MM-%dd");
    string_t *text = format ? datetime_format_text(dttm, format) : NULL;
    const char *c_text = text ? string_c_str(text) : NULL;
    char *out = NULL;

    if (c_text) {
        size_t len = strlen(c_text);
        out = malloc(len + 1u);
        if (out)
            memcpy(out, c_text, len + 1u);
    }

    string_free(text);
    string_free(format);
    return out;
}

int jurisdiction_iso_weekday_from_datetime_weekday(int weekday)
{
    switch (weekday) {
        case DT_Monday:
            return 1;
        case DT_Tuesday:
            return 2;
        case DT_Wednesday:
            return 3;
        case DT_Thursday:
            return 4;
        case DT_Friday:
            return 5;
        case DT_Saturday:
            return 6;
        case DT_Sunday:
            return 7;
        default:
            return 0;
    }
}

int jurisdiction_iso_weekday_from_date_text(const char *text)
{
    short year;
    month_t month;
    uint8_t day;
    datetime_t *dttm;
    int weekday;

    if (!jurisdiction_parse_date_text(text, &year, &month, &day))
        return 0;
    dttm = datetime_init_ymd(datetime_alloc(), year, month, day);
    if (!dttm)
        return 0;
    weekday = jurisdiction_iso_weekday_from_datetime_weekday((int)datetime_weekday(dttm));
    datetime_dealloc(dttm);
    return weekday;
}

char *jurisdiction_date_text_add_days(const char *text, long days)
{
    short year;
    month_t month;
    uint8_t day;
    datetime_t *dttm;
    char *out;

    if (!jurisdiction_parse_date_text(text, &year, &month, &day))
        return NULL;
    dttm = datetime_init_ymd(datetime_alloc(), year, month, day);
    if (!dttm)
        return NULL;
    datetime_add_days(dttm, days);
    out = jurisdiction_format_date(dttm);
    datetime_dealloc(dttm);
    return out;
}

char *jurisdiction_normalise_weekend_mask(const char *mask)
{
    size_t i;
    size_t out_len = 0u;
    char *out = malloc(16u);

    if (!out)
        return NULL;
    if (!mask || !*mask) {
        strcpy(out, "6,7");
        return out;
    }
    for (i = 0u; mask[i] != '\0'; ++i) {
        if ((mask[i] >= '1' && mask[i] <= '7') || mask[i] == ',')
            out[out_len++] = mask[i];
    }
    if (out_len == 0u) {
        strcpy(out, "6,7");
        return out;
    }
    out[out_len] = '\0';
    return out;
}

bool jurisdiction_weekend_mask_contains(const char *mask, int weekday)
{
    char token[2];

    if (!mask || weekday < 1 || weekday > 7)
        return false;
    token[0] = (char)('0' + weekday);
    token[1] = '\0';
    return strstr(mask, token) != NULL;
}

const char *jurisdiction_effective_weekend_mask_for_year(const weekend_rule_t *rules, size_t count, int year)
{
    size_t i;

    for (i = 0; i < count; ++i) {
        if (jurisdiction_year_in_range(year, rules[i].valid_from_year, rules[i].valid_to_year))
            return rules[i].weekend_mask;
    }
    return "6,7";
}

bool jurisdiction_load_weekend_rules(sqlite_t *db, const jurisdiction_vec_t *lineage_rows, jurisdiction_vec_t *weekend_rules)
{
    static const char sql[] = "select jurisdiction_id, weekend_mask, valid_from_year, valid_to_year "
                              "from jurisdiction_weekend_rule where jurisdiction_id = ?1 "
                              "order by coalesce(valid_from_year, -999999), coalesce(valid_to_year, 999999);";
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
            weekend_rule_t row;

            memset(&row, 0, sizeof(row));
            snprintf(row.jurisdiction_id, sizeof(row.jurisdiction_id), "%s", lineage[i].jurisdiction_id);
            row.depth = lineage[i].depth;
            row.weekend_mask = jurisdiction_normalise_weekend_mask(sqlite_stmt_column_text(stmt, 1));
            row.valid_from_year = sqlite_stmt_column_is_null(stmt, 2) ? 0 : sqlite_stmt_column_int(stmt, 2);
            row.valid_to_year = sqlite_stmt_column_is_null(stmt, 3) ? 0 : sqlite_stmt_column_int(stmt, 3);
            if (!row.weekend_mask || !jurisdiction_vec_push(weekend_rules, &row))
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

void jurisdiction_free_weekend_rules(jurisdiction_vec_t *rows)
{
    size_t i;
    weekend_rule_t *items = rows ? rows->items : NULL;

    if (!items)
        return;
    for (i = 0; i < rows->count; ++i)
        free(items[i].weekend_mask);
    jurisdiction_vec_free(rows);
}

static bool holiday_date_matches(const datetime_t *lhs, const datetime_t *rhs)
{
    return lhs && rhs && datetime_compare(lhs, rhs) == 0;
}

static bool holiday_has_event_on_date(const array_t *events, const datetime_t *date)
{
    size_t i;

    if (!events || !date)
        return false;
    for (i = 0u; i < array_size(events); ++i) {
        const holiday_event_t *event = array_get(events, i);

        if (event && holiday_date_matches(event->holiday_date, date))
            return true;
    }
    return false;
}

/* Apply the inherited weekend policy for the supplied year. */
bool jurisdict_is_weekend(jurisdiction_t *holiday, const datetime_t *date)
{
    sqlite_t *db = holiday ? holiday->db : NULL;
    const char *jurisdiction = holiday ? holiday->jurisdiction : NULL;
    jurisdiction_vec_t lineage_rows = {0};
    jurisdiction_vec_t weekend_rules = {0};
    weekend_rule_t *weekend_items;
    const char *weekend_mask;
    int iso_weekday;
    bool is_weekend = false;

    lineage_rows.item_size = sizeof(lineage_row_t);
    weekend_rules.item_size = sizeof(weekend_rule_t);

    if (!holiday || !db || !jurisdiction || *jurisdiction == '\0' || !date) {
        jurisdiction_set_error(holiday, "invalid holiday query");
        return false;
    }
    if (!jurisdiction_load_lineage(db, jurisdiction, &lineage_rows) ||
        !jurisdiction_load_weekend_rules(db, &lineage_rows, &weekend_rules)) {
        jurisdiction_set_error(holiday, "failed to load weekend rules");
        goto done;
    }

    weekend_items = weekend_rules.items;
    weekend_mask = jurisdiction_effective_weekend_mask_for_year(weekend_items, weekend_rules.count, (int)datetime_year(date));
    iso_weekday = jurisdiction_iso_weekday_from_datetime_weekday((int)datetime_weekday(date));
    is_weekend = jurisdiction_weekend_mask_contains(weekend_mask, iso_weekday);

done:
    jurisdiction_free_weekend_rules(&weekend_rules);
    jurisdiction_vec_free(&lineage_rows);
    return is_weekend;
}

/* Count inclusive dates that are neither holidays nor weekend days. */
long jurisdict_working_days_between(jurisdiction_t *holiday, const datetime_t *start, const datetime_t *end)
{
    array_t *events;
    datetime_t *cursor = NULL;
    long working_days = 0;

    if (!holiday || !start || !end || datetime_compare(start, end) > 0) {
        jurisdiction_set_error(holiday, "invalid holiday query");
        return -1;
    }

    events = jurisdict_holidays_between(holiday, start, end);
    if (!events)
        return -1;

    cursor = datetime_init_copy(datetime_alloc(), start);
    if (!cursor) {
        array_destroy(events);
        jurisdiction_set_error(holiday, "failed to initialise working day cursor");
        return -1;
    }

    while (datetime_compare(cursor, end) <= 0) {
        if (!jurisdict_is_weekend(holiday, cursor) && !holiday_has_event_on_date(events, cursor)) {
            working_days++;
        }
        if (!datetime_add_days(cursor, 1)) {
            working_days = -1;
            jurisdiction_set_error(holiday, "failed to advance working day cursor");
            break;
        }
    }

    datetime_dealloc(cursor);
    array_destroy(events);
    return working_days;
}
