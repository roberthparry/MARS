/**
 * @file jurisdiction_dst.c
 * @brief Daylight-saving transition occurrence queries.
 *
 * Collects named timezone-rule transitions and materialises their local display dates and times. This unit reports
 * transitions while the timezone unit resolves effective offsets.
 *
 * This is part of jurisdiction.h and uses configured rule data. Results depend on that data's coverage and
 * currency rather than hard-coded assumptions about the host machine.
 */

/* Collect daylight-saving transitions and materialise their local display times. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>

#include "jurisdiction_internal.h"

typedef struct timezone_transition_occurrence_t {
    short year;
    month_t month;
    uint8_t day;
    int seconds;
    char at_suffix;
    int gmtoff_minutes;
} timezone_transition_occurrence_t;

typedef struct timezone_named_rule_ref_t {
    char *rule_name;
    int gmtoff_minutes;
} timezone_named_rule_ref_t;

static void free_timezone_named_rule_refs(jurisdiction_vec_t *rows)
{
    size_t i;
    timezone_named_rule_ref_t *items = rows ? rows->items : NULL;

    if (!items)
        return;
    for (i = 0u; i < rows->count; ++i)
        free(items[i].rule_name);
    jurisdiction_vec_free(rows);
}

static int transition_occurrence_compare(const void *lhs, const void *rhs)
{
    const timezone_transition_occurrence_t *left = lhs;
    const timezone_transition_occurrence_t *right = rhs;

    return jurisdiction_compare_boundary_to_boundary(left->year, left->month, left->day, left->seconds, right->year, right->month,
                                        right->day, right->seconds);
}

static bool materialise_transition_display_datetime(const timezone_transition_occurrence_t *occurrence,
                                                    double previous_offset_hours, datetime_t **out)
{
    short year;
    month_t month;
    uint8_t day;
    int seconds;
    double standard_offset_hours;
    double previous_save_hours;

    if (out)
        *out = NULL;
    if (!occurrence || !out)
        return false;

    year = occurrence->year;
    month = occurrence->month;
    day = occurrence->day;
    seconds = occurrence->seconds;
    standard_offset_hours = (double)occurrence->gmtoff_minutes / 60.0;
    previous_save_hours = previous_offset_hours - standard_offset_hours;

    switch (occurrence->at_suffix) {
        case 'u':
        case 'g':
        case 'z':
            seconds += (int)lround(previous_offset_hours * 3600.0);
            break;
        case 's':
            seconds += (int)lround(previous_save_hours * 3600.0);
            break;
        case 'w':
        default:
            break;
    }

    if (!jurisdiction_normalise_day_time(&year, &month, &day, &seconds))
        return false;
    *out = datetime_init_ymdt(datetime_alloc(), year, month, day, (uint8_t)(seconds / 3600),
                              (uint8_t)((seconds % 3600) / 60), (double)(seconds % 60));
    return *out != NULL;
}

static bool timezone_load_named_rule_names_for_year(sqlite_t *db, const char *timezone_name, int year,
                                                    jurisdiction_vec_t *rule_names)
{
    jurisdiction_vec_t era_rows = {0};
    datetime_t *jan1 = NULL;
    datetime_t *dec31 = NULL;
    const timezone_era_row_t *jan_era;
    const timezone_era_row_t *dec_era;
    timezone_named_rule_ref_t ref;
    size_t i;
    bool ok = false;

    if (!db || !timezone_name || !*timezone_name || !rule_names || year < 1 || year > 9999)
        return false;

    era_rows.item_size = sizeof(timezone_era_row_t);
    jan1 = datetime_init_ymd(datetime_alloc(), (short)year, DT_January, 1u);
    dec31 = datetime_init_ymd(datetime_alloc(), (short)year, DT_December, 31u);
    if (!jan1 || !dec31)
        goto done;
    if (!jurisdiction_load_timezone_eras(db, timezone_name, &era_rows))
        goto done;
    jan_era = jurisdiction_select_timezone_era_for_date(&era_rows, jan1);
    dec_era = jurisdiction_select_timezone_era_for_date(&era_rows, dec31);

    if (jan_era && jan_era->rules_kind && strcmp(jan_era->rules_kind, "named") == 0 && jan_era->rule_name &&
        *jan_era->rule_name) {
        memset(&ref, 0, sizeof(ref));
        ref.rule_name = jurisdiction_dup_c_string(jan_era->rule_name);
        ref.gmtoff_minutes = jan_era->gmtoff_minutes;
        if (!ref.rule_name || !jurisdiction_vec_push(rule_names, &ref)) {
            free(ref.rule_name);
            goto done;
        }
    }

    if (dec_era && dec_era->rules_kind && strcmp(dec_era->rules_kind, "named") == 0 && dec_era->rule_name &&
        *dec_era->rule_name) {
        bool seen = false;
        const timezone_named_rule_ref_t *names = rule_names->items;

        for (i = 0u; names && i < rule_names->count; ++i) {
            if (names[i].rule_name && strcmp(names[i].rule_name, dec_era->rule_name) == 0) {
                seen = true;
                break;
            }
        }
        if (!seen) {
            memset(&ref, 0, sizeof(ref));
            ref.rule_name = jurisdiction_dup_c_string(dec_era->rule_name);
            ref.gmtoff_minutes = dec_era->gmtoff_minutes;
            if (!ref.rule_name || !jurisdiction_vec_push(rule_names, &ref)) {
                free(ref.rule_name);
                goto done;
            }
        }
    }

    ok = true;

done:
    datetime_dealloc(dec31);
    datetime_dealloc(jan1);
    jurisdiction_free_timezone_era_rows(&era_rows);
    return ok;
}

static bool timezone_collect_transition_occurrences(sqlite_t *db, const timezone_named_rule_ref_t *rule_ref, int year,
                                                    jurisdiction_vec_t *occurrences)
{
    jurisdiction_vec_t rule_rows = {0};
    const timezone_transition_rule_row_t *items;
    size_t i;
    bool ok = false;

    if (!db || !rule_ref || !rule_ref->rule_name || !*rule_ref->rule_name || !occurrences || year < 1 || year > 9999)
        return false;

    rule_rows.item_size = sizeof(timezone_transition_rule_row_t);
    if (!jurisdiction_load_timezone_transition_rules(db, rule_ref->rule_name, &rule_rows))
        return false;
    items = rule_rows.items;

    for (i = 0u; i < rule_rows.count; ++i) {
        timezone_transition_occurrence_t occurrence;

        memset(&occurrence, 0, sizeof(occurrence));
        if (!jurisdiction_compute_transition_occurrence(&items[i], year, &occurrence.year, &occurrence.month, &occurrence.day,
                                           &occurrence.seconds))
            continue;
        occurrence.at_suffix = items[i].at_suffix;
        occurrence.gmtoff_minutes = rule_ref->gmtoff_minutes;
        if (!jurisdiction_vec_push(occurrences, &occurrence))
            goto done;
    }

    ok = true;

done:
    jurisdiction_free_timezone_transition_rule_rows(&rule_rows);
    return ok;
}

/* Resolve local DST transitions and their before/after GMT offsets. */
bool jurisdict_dst_transition_details(jurisdiction_t *holiday, int year, datetime_t **clocks_forward,
                                      double *forward_from_offset_hours, double *forward_to_offset_hours,
                                      datetime_t **clocks_back, double *back_from_offset_hours,
                                      double *back_to_offset_hours)
{
    sqlite_t *db = holiday ? holiday->db : NULL;
    const char *jurisdiction = holiday ? holiday->jurisdiction : NULL;
    jurisdiction_vec_t lineage_rows = {0};
    char timezone_name[128];
    jurisdiction_vec_t rule_names = {0};
    jurisdiction_vec_t occurrences = {0};
    const timezone_named_rule_ref_t *rule_name_items = NULL;
    timezone_transition_occurrence_t *occurrence_items = NULL;
    size_t i;
    bool ok = false;

    if (clocks_forward)
        *clocks_forward = NULL;
    if (clocks_back)
        *clocks_back = NULL;
    if (forward_from_offset_hours)
        *forward_from_offset_hours = DBL_MAX;
    if (forward_to_offset_hours)
        *forward_to_offset_hours = DBL_MAX;
    if (back_from_offset_hours)
        *back_from_offset_hours = DBL_MAX;
    if (back_to_offset_hours)
        *back_to_offset_hours = DBL_MAX;

    lineage_rows.item_size = sizeof(lineage_row_t);
    rule_names.item_size = sizeof(timezone_named_rule_ref_t);
    occurrences.item_size = sizeof(timezone_transition_occurrence_t);

    if (!holiday || !db || !jurisdiction || *jurisdiction == '\0' || year < 1 || year > 9999) {
        jurisdiction_set_error(holiday, "invalid daylight-saving query");
        return false;
    }
    if (!jurisdiction_load_lineage(db, jurisdiction, &lineage_rows)) {
        jurisdiction_set_error(holiday, "failed to load jurisdiction lineage");
        goto done;
    }
    if (!jurisdiction_load_default_timezone_name(db, &lineage_rows, timezone_name, sizeof(timezone_name))) {
        jurisdiction_set_error(holiday, "default jurisdiction timezone unavailable");
        goto done;
    }
    if (!timezone_load_named_rule_names_for_year(db, timezone_name, year, &rule_names)) {
        jurisdiction_set_error(holiday, "failed to load daylight-saving rules");
        goto done;
    }

    rule_name_items = rule_names.items;
    for (i = 0u; rule_name_items && i < rule_names.count; ++i) {
        if (!timezone_collect_transition_occurrences(db, &rule_name_items[i], year, &occurrences)) {
            jurisdiction_set_error(holiday, "failed to collect daylight-saving transitions");
            goto done;
        }
    }

    occurrence_items = occurrences.items;
    if (occurrence_items && occurrences.count > 1u)
        qsort(occurrence_items, occurrences.count, sizeof(*occurrence_items), transition_occurrence_compare);

    for (i = 0u; occurrence_items && i < occurrences.count; ++i) {
        datetime_t *transition_date = NULL;
        datetime_t *previous_date = NULL;
        datetime_t *transition_probe = NULL;
        double previous_offset = 0.0;
        double current_offset = 0.0;
        double delta;

        previous_date = datetime_init_ymd(datetime_alloc(), occurrence_items[i].year, occurrence_items[i].month,
                                          occurrence_items[i].day);
        transition_probe = datetime_init_ymd(datetime_alloc(), occurrence_items[i].year, occurrence_items[i].month,
                                             occurrence_items[i].day);
        if (!previous_date || !transition_probe) {
            datetime_dealloc(transition_probe);
            datetime_dealloc(previous_date);
            jurisdiction_set_error(holiday, "failed to materialise daylight-saving transition date");
            goto done;
        }
        if (!datetime_add_days(previous_date, -1) ||
            !jurisdiction_timezone_offset_for_name_on_date(db, timezone_name, previous_date, &previous_offset) ||
            !jurisdiction_timezone_offset_for_name_on_date(db, timezone_name, transition_probe, &current_offset)) {
            datetime_dealloc(transition_probe);
            datetime_dealloc(previous_date);
            jurisdiction_set_error(holiday, "failed to evaluate daylight-saving transition");
            goto done;
        }
        datetime_dealloc(transition_probe);
        transition_probe = NULL;
        datetime_dealloc(previous_date);
        previous_date = NULL;

        if (!materialise_transition_display_datetime(&occurrence_items[i], previous_offset, &transition_date)) {
            jurisdiction_set_error(holiday, "failed to materialise daylight-saving transition time");
            goto done;
        }

        delta = current_offset - previous_offset;
        if (delta > 0.0) {
            if (clocks_forward && !*clocks_forward) {
                *clocks_forward = transition_date;
                if (forward_from_offset_hours)
                    *forward_from_offset_hours = previous_offset;
                if (forward_to_offset_hours)
                    *forward_to_offset_hours = current_offset;
            } else
                datetime_dealloc(transition_date);
        } else if (delta < 0.0) {
            if (clocks_back && !*clocks_back) {
                *clocks_back = transition_date;
                if (back_from_offset_hours)
                    *back_from_offset_hours = previous_offset;
                if (back_to_offset_hours)
                    *back_to_offset_hours = current_offset;
            } else
                datetime_dealloc(transition_date);
        } else {
            datetime_dealloc(transition_date);
        }
    }

    ok = true;

done:
    if (!ok) {
        if (clocks_forward) {
            datetime_dealloc(*clocks_forward);
            *clocks_forward = NULL;
        }
        if (clocks_back) {
            datetime_dealloc(*clocks_back);
            *clocks_back = NULL;
        }
        if (forward_from_offset_hours)
            *forward_from_offset_hours = DBL_MAX;
        if (forward_to_offset_hours)
            *forward_to_offset_hours = DBL_MAX;
        if (back_from_offset_hours)
            *back_from_offset_hours = DBL_MAX;
        if (back_to_offset_hours)
            *back_to_offset_hours = DBL_MAX;
    }
    free_timezone_named_rule_refs(&rule_names);
    jurisdiction_vec_free(&occurrences);
    jurisdiction_vec_free(&lineage_rows);
    return ok;
}

/* Return local DST transition moments without offset details. */
bool jurisdict_dst_transition_datetimes(jurisdiction_t *holiday, int year, datetime_t **clocks_forward,
                                        datetime_t **clocks_back)
{
    return jurisdict_dst_transition_details(holiday, year, clocks_forward, NULL, NULL, clocks_back, NULL, NULL);
}
