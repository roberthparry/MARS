/**
 * @file jurisdiction_holidays.c
 * @brief Holiday occurrences, exceptions and observance shifts.
 *
 * Builds and orders holiday events, applies occupancy and exception rules and exposes date-range queries. Returned
 * events reflect the configured jurisdiction data rather than a fixed worldwide holiday list.
 *
 * This is part of jurisdiction.h and uses configured rule data. Results depend on that data's coverage and
 * currency rather than hard-coded assumptions about the host machine.
 */

/* Assemble holiday occurrences, apply exceptions and observances, and expose queries. */
#include "jurisdiction_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct holiday_event_row_t {
    int holiday_id;
    int rule_id;
    int event_year;
    char *holiday_date;
    char *holiday_name;
    char *holiday_class;
    bool removed;
    bool derived_from_observance;
} holiday_event_row_t;

typedef struct holiday_collect_ctx_t {
    array_t *events;
} holiday_collect_ctx_t;

static int text_compare(const char *a, const char *b)
{
    if (!a && !b)
        return 0;
    if (!a)
        return -1;
    if (!b)
        return 1;
    return strcmp(a, b);
}

static int event_compare(const void *lhs, const void *rhs)
{
    const holiday_event_row_t *a = lhs;
    const holiday_event_row_t *b = rhs;
    int cmp = text_compare(a->holiday_date, b->holiday_date);

    if (cmp != 0)
        return cmp;
    cmp = text_compare(a->holiday_name, b->holiday_name);
    if (cmp != 0)
        return cmp;
    cmp = text_compare(a->holiday_class, b->holiday_class);
    if (cmp != 0)
        return cmp;
    if (a->derived_from_observance != b->derived_from_observance)
        return a->derived_from_observance ? 1 : -1;
    if (a->holiday_id != b->holiday_id)
        return (a->holiday_id < b->holiday_id) ? -1 : 1;
    if (a->rule_id != b->rule_id)
        return (a->rule_id < b->rule_id) ? -1 : 1;
    return 0;
}

static void free_events(jurisdiction_vec_t *rows)
{
    size_t i;
    holiday_event_row_t *items = rows ? rows->items : NULL;

    if (!items)
        return;
    for (i = 0; i < rows->count; ++i) {
        free(items[i].holiday_date);
        free(items[i].holiday_name);
        free(items[i].holiday_class);
    }
    jurisdiction_vec_free(rows);
}

static bool date_is_occupied(const holiday_event_row_t *events, size_t count, const char *holiday_date)
{
    size_t i;

    for (i = 0; i < count; ++i) {
        if (events[i].removed || !events[i].holiday_date)
            continue;
        if (strcmp(events[i].holiday_date, holiday_date) == 0)
            return true;
    }
    return false;
}

static char *next_observed_date(const char *base_date, const char *weekend_mask,
                                const holiday_event_row_t *occupied_events, size_t occupied_count, bool avoid_occupied,
                                bool force_monday)
{
    long delta;

    for (delta = 1; delta <= 14; ++delta) {
        char *candidate = jurisdiction_date_text_add_days(base_date, delta);
        int weekday = candidate ? jurisdiction_iso_weekday_from_date_text(candidate) : 0;
        bool is_weekend = jurisdiction_weekend_mask_contains(weekend_mask, weekday);
        bool is_monday = weekday == 1;
        bool occupied =
            candidate && avoid_occupied ? date_is_occupied(occupied_events, occupied_count, candidate) : false;

        if (candidate && !is_weekend && (!force_monday || is_monday) && !occupied)
            return candidate;
        free(candidate);
    }
    return NULL;
}

static char *previous_observed_date(const char *base_date, const char *weekend_mask,
                                    const holiday_event_row_t *occupied_events, size_t occupied_count,
                                    bool avoid_occupied)
{
    long delta;

    for (delta = 1; delta <= 14; ++delta) {
        char *candidate = jurisdiction_date_text_add_days(base_date, -delta);
        int weekday = candidate ? jurisdiction_iso_weekday_from_date_text(candidate) : 0;
        bool is_weekend = jurisdiction_weekend_mask_contains(weekend_mask, weekday);
        bool occupied =
            candidate && avoid_occupied ? date_is_occupied(occupied_events, occupied_count, candidate) : false;

        if (candidate && !is_weekend && !occupied)
            return candidate;
        free(candidate);
    }
    return NULL;
}

static bool add_event(jurisdiction_vec_t *events, int holiday_id, int rule_id, int event_year, const char *holiday_date,
                      const char *holiday_name, const char *holiday_class, bool derived_from_observance)
{
    holiday_event_row_t event;

    memset(&event, 0, sizeof(event));
    event.holiday_id = holiday_id;
    event.rule_id = rule_id;
    event.event_year = event_year;
    event.holiday_date = jurisdiction_dup_c_string(holiday_date);
    event.holiday_name = jurisdiction_dup_c_string(holiday_name);
    event.holiday_class = jurisdiction_dup_c_string(holiday_class ? holiday_class : "public");
    event.derived_from_observance = derived_from_observance;

    if (!event.holiday_date || !event.holiday_name || !event.holiday_class || !jurisdiction_vec_push(events, &event)) {
        free(event.holiday_date);
        free(event.holiday_name);
        free(event.holiday_class);
        return false;
    }
    return true;
}

static void filter_events_to_range(jurisdiction_vec_t *events, const char *start_text, const char *end_text)
{
    holiday_event_row_t *items = events ? events->items : NULL;
    size_t i;

    if (!items || !start_text || !end_text)
        return;

    for (i = 0u; i < events->count; ++i) {
        if (items[i].removed || !items[i].holiday_date)
            continue;
        if (strcmp(items[i].holiday_date, start_text) < 0 || strcmp(items[i].holiday_date, end_text) > 0)
            items[i].removed = true;
    }
}

static bool holiday_events_equivalent(const holiday_event_row_t *lhs, const holiday_event_row_t *rhs)
{
    if (!lhs || !rhs)
        return false;
    return text_compare(lhs->holiday_date, rhs->holiday_date) == 0 &&
           text_compare(lhs->holiday_name, rhs->holiday_name) == 0 &&
           text_compare(lhs->holiday_class, rhs->holiday_class) == 0;
}

static void dedupe_sorted_events(jurisdiction_vec_t *events)
{
    holiday_event_row_t *items = events ? events->items : NULL;
    holiday_event_row_t *previous = NULL;
    size_t i;

    if (!items)
        return;

    for (i = 0u; i < events->count; ++i) {
        if (items[i].removed)
            continue;
        if (previous && holiday_events_equivalent(previous, &items[i])) {
            items[i].removed = true;
            continue;
        }
        previous = &items[i];
    }
}

static void apply_exceptions(jurisdiction_vec_t *events, const jurisdiction_vec_t *exceptions)
{
    holiday_event_row_t *event_items = events ? events->items : NULL;
    const holiday_exception_row_t *exception_items = exceptions ? exceptions->items : NULL;
    size_t i;
    size_t j;

    if (!event_items || !exception_items)
        return;

    for (i = 0; i < exceptions->count; ++i) {
        int exception_year = atoi(exception_items[i].holiday_date);

        if (!jurisdiction_year_in_range(exception_year, exception_items[i].valid_from_year, exception_items[i].valid_to_year))
            continue;

        if (strcmp(exception_items[i].action, "add") == 0 || strcmp(exception_items[i].action, "replace") == 0) {
            const char *name = exception_items[i].name;
            const char *holiday_class = "public";

            for (j = 0; j < events->count; ++j) {
                if (event_items[j].removed)
                    continue;
                if (event_items[j].holiday_id == exception_items[i].holiday_id && event_items[j].holiday_class) {
                    holiday_class = event_items[j].holiday_class;
                    if (!name)
                        name = event_items[j].holiday_name;
                    break;
                }
            }
            if (strcmp(exception_items[i].action, "replace") == 0) {
                for (j = 0; j < events->count; ++j) {
                    if (event_items[j].removed || event_items[j].event_year != exception_year)
                        continue;
                    if ((exception_items[i].target_rule_id != 0 &&
                         event_items[j].rule_id == exception_items[i].target_rule_id) ||
                        (exception_items[i].target_rule_id == 0 &&
                         event_items[j].holiday_id == exception_items[i].holiday_id))
                        event_items[j].removed = true;
                }
            }
            add_event(events, exception_items[i].holiday_id, exception_items[i].target_rule_id, exception_year,
                      exception_items[i].holiday_date, name ? name : "Holiday", holiday_class, false);
            event_items = events->items;
        } else if (strcmp(exception_items[i].action, "suppress") == 0) {
            for (j = 0; j < events->count; ++j) {
                if (event_items[j].removed)
                    continue;
                if (strcmp(event_items[j].holiday_date, exception_items[i].holiday_date) == 0 &&
                    (exception_items[i].target_rule_id == 0 ||
                     event_items[j].rule_id == exception_items[i].target_rule_id) &&
                    (exception_items[i].holiday_id == 0 || event_items[j].holiday_id == exception_items[i].holiday_id))
                    event_items[j].removed = true;
            }
        } else if (strcmp(exception_items[i].action, "rename") == 0 && exception_items[i].name) {
            for (j = 0; j < events->count; ++j) {
                if (event_items[j].removed)
                    continue;
                if (strcmp(event_items[j].holiday_date, exception_items[i].holiday_date) == 0 &&
                    (exception_items[i].target_rule_id == 0 ||
                     event_items[j].rule_id == exception_items[i].target_rule_id) &&
                    (exception_items[i].holiday_id == 0 ||
                     event_items[j].holiday_id == exception_items[i].holiday_id)) {
                    free(event_items[j].holiday_name);
                    event_items[j].holiday_name = jurisdiction_dup_c_string(exception_items[i].name);
                }
            }
        }
    }
}

static void apply_observances(jurisdiction_vec_t *events, const jurisdiction_vec_t *observances,
                              const weekend_rule_t *weekend_rules, size_t weekend_rule_count)
{
    holiday_event_row_t *event_items = events ? events->items : NULL;
    const observance_rule_row_t *observance_items = observances ? observances->items : NULL;
    size_t i;
    size_t j;

    if (!event_items || !observance_items)
        return;

    for (i = 0; i < observances->count; ++i) {
        for (j = 0; j < events->count; ++j) {
            const char *weekend_mask;
            int weekday;
            char *observed_date = NULL;

            if (event_items[j].removed || event_items[j].derived_from_observance)
                continue;
            if (event_items[j].holiday_id != observance_items[i].holiday_id)
                continue;
            if (observance_items[i].applies_to_rule_id != 0 &&
                event_items[j].rule_id != observance_items[i].applies_to_rule_id)
                continue;
            if (!jurisdiction_year_in_range(event_items[j].event_year, observance_items[i].valid_from_year,
                               observance_items[i].valid_to_year))
                continue;

            weekend_mask =
                observance_items[i].weekend_mask && *observance_items[i].weekend_mask
                    ? observance_items[i].weekend_mask
                    : jurisdiction_effective_weekend_mask_for_year(weekend_rules, weekend_rule_count, event_items[j].event_year);
            weekday = jurisdiction_iso_weekday_from_date_text(event_items[j].holiday_date);
            if (!jurisdiction_weekend_mask_contains(weekend_mask, weekday))
                continue;

            if (strcmp(observance_items[i].observed_rule_kind, "next_weekday") == 0)
                observed_date = next_observed_date(event_items[j].holiday_date, weekend_mask, event_items,
                                                   events->count, false, false);
            else if (strcmp(observance_items[i].observed_rule_kind, "next_monday") == 0)
                observed_date = next_observed_date(event_items[j].holiday_date, weekend_mask, event_items,
                                                   events->count, false, true);
            else if (strcmp(observance_items[i].observed_rule_kind, "next_non_holiday") == 0)
                observed_date = next_observed_date(event_items[j].holiday_date, weekend_mask, event_items,
                                                   events->count, true, false);
            else if (strcmp(observance_items[i].observed_rule_kind, "previous_weekday") == 0)
                observed_date = previous_observed_date(event_items[j].holiday_date, weekend_mask, event_items,
                                                       events->count, false);
            else if (strcmp(observance_items[i].observed_rule_kind, "christmas_pair") == 0)
                observed_date = next_observed_date(event_items[j].holiday_date, weekend_mask, event_items,
                                                   events->count, true, false);

            if (!observed_date)
                continue;
            if (observance_items[i].suppress_original)
                event_items[j].removed = true;
            add_event(
                events, event_items[j].holiday_id, event_items[j].rule_id, event_items[j].event_year, observed_date,
                observance_items[i].observed_name ? observance_items[i].observed_name : event_items[j].holiday_name,
                event_items[j].holiday_class, true);
            free(observed_date);
            event_items = events->items;
        }
    }
}

static void holiday_event_destroy_owned(holiday_event_t *event)
{
    if (!event)
        return;
    datetime_dealloc((datetime_t *)event->holiday_date);
    free((char *)event->holiday_name);
    free((char *)event->holiday_class);
    memset(event, 0, sizeof(*event));
}

static bool holiday_collect_event(const holiday_event_t *event, void *ctx)
{
    holiday_collect_ctx_t *collect_ctx = ctx;
    holiday_event_t owned;

    if (!event || !collect_ctx || !collect_ctx->events)
        return false;

    memset(&owned, 0, sizeof(owned));
    owned.holiday_id = event->holiday_id;
    owned.rule_id = event->rule_id;
    owned.event_year = event->event_year;
    owned.holiday_date = event->holiday_date ? datetime_init_copy(datetime_alloc(), event->holiday_date) : NULL;
    owned.holiday_name = event->holiday_name ? jurisdiction_dup_c_string(event->holiday_name) : NULL;
    owned.holiday_class = event->holiday_class ? jurisdiction_dup_c_string(event->holiday_class) : NULL;
    owned.derived_from_observance = event->derived_from_observance;

    if ((event->holiday_date && !owned.holiday_date) || (event->holiday_name && !owned.holiday_name) ||
        (event->holiday_class && !owned.holiday_class) || !array_add(collect_ctx->events, &owned)) {
        holiday_event_destroy_owned(&owned);
        return false;
    }
    return true;
}

/* Collect an owned array of holiday occurrences in the inclusive range. */
array_t *jurisdict_holidays_between(jurisdiction_t *holiday, const datetime_t *start, const datetime_t *end)
{
    holiday_collect_ctx_t ctx;
    array_t *events;

    events = array_create(sizeof(holiday_event_t), NULL, (array_destroy_fn)holiday_event_destroy_owned);
    if (!events)
        return NULL;

    memset(&ctx, 0, sizeof(ctx));
    ctx.events = events;
    if (!jurisdict_each_holiday_between(holiday, start, end, holiday_collect_event, &ctx)) {
        array_destroy(events);
        return NULL;
    }
    return events;
}

/* Test whether the supplied date has a holiday occurrence. */
bool jurisdict_is_national_holiday(jurisdiction_t *holiday, const datetime_t *date)
{
    array_t *events;
    bool is_holiday;

    if (!holiday || !date) {
        jurisdiction_set_error(holiday, "invalid holiday query");
        return false;
    }
    events = jurisdict_holidays_between(holiday, date, date);
    if (!events)
        return false;
    is_holiday = array_size(events) > 0u;
    array_destroy(events);
    return is_holiday;
}

/* Evaluate holiday rules and visit the resulting borrowed event views. */
bool jurisdict_each_holiday_between(jurisdiction_t *holiday, const datetime_t *start, const datetime_t *end,
                                    jurisdict_visit_fn visitor, void *ctx)
{
    sqlite_t *db = holiday ? holiday->db : NULL;
    const char *jurisdiction = holiday ? holiday->jurisdiction : NULL;
    char *start_text = NULL;
    char *end_text = NULL;
    jurisdiction_vec_t lineage_rows = {0};
    jurisdiction_vec_t weekend_rules = {0};
    jurisdiction_vec_t rules = {0};
    jurisdiction_vec_t observances = {0};
    jurisdiction_vec_t exceptions = {0};
    jurisdiction_vec_t events = {0};
    holiday_rule_row_t *rule_items;
    holiday_event_row_t *event_items;
    weekend_rule_t *weekend_items;
    int year;
    size_t i;
    bool ok = false;

    lineage_rows.item_size = sizeof(lineage_row_t);
    weekend_rules.item_size = sizeof(weekend_rule_t);
    rules.item_size = sizeof(holiday_rule_row_t);
    observances.item_size = sizeof(observance_rule_row_t);
    exceptions.item_size = sizeof(holiday_exception_row_t);
    events.item_size = sizeof(holiday_event_row_t);

    if (!holiday || !db || !jurisdiction || *jurisdiction == '\0' || !start || !end || !visitor) {
        jurisdiction_set_error(holiday, "invalid holiday query");
        return false;
    }

    start_text = jurisdiction_format_date(start);
    end_text = jurisdiction_format_date(end);
    if (!start_text || !end_text) {
        jurisdiction_set_error(holiday, "failed to format date range");
        goto done;
    }

    if (!jurisdiction_load_lineage(db, jurisdiction, &lineage_rows) ||
        !jurisdiction_load_weekend_rules(db, &lineage_rows, &weekend_rules) ||
        !jurisdiction_load_holiday_rules(db, &lineage_rows, datetime_year(start), datetime_year(end), &rules) ||
        !jurisdiction_load_observance_rules(db, &lineage_rows, &observances) ||
        !jurisdiction_load_exceptions(db, &lineage_rows, &exceptions)) {
        jurisdiction_set_error(holiday, "failed to load holiday rules");
        goto done;
    }

    rule_items = rules.items;
    weekend_items = weekend_rules.items;
    for (year = datetime_year(start); year <= datetime_year(end); ++year) {
        (void)jurisdiction_effective_weekend_mask_for_year(weekend_items, weekend_rules.count, year);
        for (i = 0; i < rules.count; ++i) {
            char *holiday_date = jurisdiction_evaluate_rule_date(db, &rule_items[i], year, jurisdiction);

            if (!holiday_date)
                continue;
            if (strcmp(holiday_date, start_text) >= 0 && strcmp(holiday_date, end_text) <= 0) {
                if (!add_event(&events, rule_items[i].holiday_id, rule_items[i].rule_id, year, holiday_date,
                               rule_items[i].holiday_name, rule_items[i].holiday_class, false)) {
                    free(holiday_date);
                    jurisdiction_set_error(holiday, "failed to collect holiday event");
                    goto done;
                }
            }
            free(holiday_date);
        }
    }

    apply_exceptions(&events, &exceptions);
    apply_observances(&events, &observances, weekend_items, weekend_rules.count);
    filter_events_to_range(&events, start_text, end_text);

    event_items = events.items;
    if (events.count > 1u)
        qsort(event_items, events.count, sizeof(*event_items), event_compare);
    dedupe_sorted_events(&events);
    for (i = 0; i < events.count; ++i) {
        holiday_event_t public_event;
        datetime_t *public_date = NULL;
        short year_value;
        month_t month_value;
        uint8_t day_value;

        if (event_items[i].removed)
            continue;
        if (!jurisdiction_parse_date_text(event_items[i].holiday_date, &year_value, &month_value, &day_value)) {
            jurisdiction_set_error(holiday, "failed to parse holiday date");
            goto done;
        }
        public_date = datetime_init_ymd(datetime_alloc(), year_value, month_value, day_value);
        if (!public_date) {
            jurisdiction_set_error(holiday, "failed to materialise holiday date");
            goto done;
        }
        memset(&public_event, 0, sizeof(public_event));
        public_event.holiday_id = event_items[i].holiday_id;
        public_event.rule_id = event_items[i].rule_id;
        public_event.event_year = event_items[i].event_year;
        public_event.holiday_date = public_date;
        public_event.holiday_name = event_items[i].holiday_name;
        public_event.holiday_class = event_items[i].holiday_class;
        public_event.derived_from_observance = event_items[i].derived_from_observance;
        if (!visitor(&public_event, ctx)) {
            datetime_dealloc(public_date);
            ok = true;
            goto done;
        }
        datetime_dealloc(public_date);
    }

    ok = true;

done:
    free_events(&events);
    jurisdiction_free_exception_rows(&exceptions);
    jurisdiction_free_observance_rule_rows(&observances);
    jurisdiction_free_holiday_rule_rows(&rules);
    jurisdiction_free_weekend_rules(&weekend_rules);
    jurisdiction_vec_free(&lineage_rows);
    free(start_text);
    free(end_text);
    return ok;
}
