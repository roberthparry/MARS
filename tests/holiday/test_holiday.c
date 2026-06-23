#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "array.h"
#include "datetime.h"
#include "holiday.h"
#include "test_harness.h"
#include "ustring.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

typedef struct holiday_case_row_t {
    int holiday_id;
    int rule_id;
    int event_year;
    bool derived_from_observance;
    datetime_t *holiday_date;
    char *holiday_name;
    char *holiday_class;
} holiday_case_row_t;

static bool holiday_suite_setup(void);
static void holiday_suite_cleanup(void);

TEST_SUITE_SETUP(holiday_suite_setup);
TEST_POST_SUMMARY(holiday_suite_cleanup);

static void holiday_case_row_clone(void *dst, const void *src)
{
    const holiday_case_row_t *from = src;
    holiday_case_row_t *to = dst;

    memset(to, 0, sizeof(*to));
    to->holiday_id = from->holiday_id;
    to->rule_id = from->rule_id;
    to->event_year = from->event_year;
    to->derived_from_observance = from->derived_from_observance;
    to->holiday_date = from->holiday_date
        ? datetime_init_copy(datetime_alloc(), from->holiday_date)
        : NULL;
    to->holiday_name = from->holiday_name ? strdup(from->holiday_name) : NULL;
    to->holiday_class = from->holiday_class ? strdup(from->holiday_class) : NULL;
}

static void holiday_case_row_destroy(void *elem)
{
    holiday_case_row_t *row = elem;

    datetime_dealloc(row->holiday_date);
    free(row->holiday_name);
    free(row->holiday_class);
}

static char *datetime_to_iso_date(const datetime_t *dttm)
{
    string_t *format = NULL;
    string_t *formatted = NULL;
    const char *text = NULL;
    char *out = NULL;

    if (!dttm)
        return NULL;
    format = string_new_with("%yyyy-%MM-%dd");
    formatted = format ? datetime_format_text(dttm, format) : NULL;
    text = formatted ? string_c_str(formatted) : NULL;
    if (text) {
        size_t len = strlen(text);

        out = malloc(len + 1u);
        if (out)
            memcpy(out, text, len + 1u);
    }
    string_free(formatted);
    string_free(format);
    return out;
}

static bool datetime_matches_iso_date(const datetime_t *dttm, const char *holiday_date)
{
    char *actual;
    bool matches;

    if (!dttm || !holiday_date)
        return false;
    actual = datetime_to_iso_date(dttm);
    matches = actual && strcmp(actual, holiday_date) == 0;
    free(actual);
    return matches;
}

static char *describe_events(const array_t *events)
{
    string_t *out = string_new();
    size_t i;

    if (!out)
        return NULL;
    if (!events || array_size(events) == 0u) {
        string_append_cstr(out, "(none)");
    } else {
        for (i = 0u; i < array_size(events); ++i) {
            const holiday_case_row_t *row = array_get(events, i);
            char *iso_date;

            if (!row)
                continue;
            if (i > 0u)
                string_append_cstr(out, "\n");
            string_append_cstr(out, "      - ");
            string_append_cstr(out, row->holiday_name ? row->holiday_name : "(unnamed)");
            string_append_cstr(out, " @ ");
            iso_date = datetime_to_iso_date(row->holiday_date);
            string_append_cstr(out, iso_date ? iso_date : "(no date)");
            free(iso_date);
        }
    }

    {
        const char *text = string_c_str(out);
        char *copy = text ? strdup(text) : NULL;

        string_free(out);
        return copy;
    }
}

static void print_events_summary(const char *label, const array_t *events)
{
    char *found = describe_events(events);

    printf("    %s (%zu)\n%s\n",
           label ? label : "events",
           events ? array_size(events) : 0u,
           found ? found : "(unavailable)");
    free(found);
}

static bool collect_event(const holiday_event_t *event, void *ctx)
{
    array_t *rows = ctx;
    holiday_case_row_t row;

    if (!event || !rows)
        return false;

    memset(&row, 0, sizeof(row));
    row.holiday_id = event->holiday_id;
    row.rule_id = event->rule_id;
    row.event_year = event->event_year;
    row.derived_from_observance = event->derived_from_observance;
    row.holiday_date = (datetime_t *)event->holiday_date;
    row.holiday_name = (char *)event->holiday_name;
    row.holiday_class = (char *)event->holiday_class;
    return array_add(rows, &row);
}

static array_t *load_events(const char *jurisdiction,
                            short start_year,
                            month_t start_month,
                            uint8_t start_day,
                            short end_year,
                            month_t end_month,
                            uint8_t end_day)
{
    holiday_t *holiday = holiday_open(jurisdiction);
    datetime_t *start = datetime_init_ymd(datetime_alloc(), start_year, start_month, start_day);
    datetime_t *end = datetime_init_ymd(datetime_alloc(), end_year, end_month, end_day);
    array_t *raw_events = NULL;
    array_t *rows = array_create(sizeof(holiday_case_row_t), holiday_case_row_clone, holiday_case_row_destroy);
    size_t i;

    if (!holiday || !start || !end || !rows) {
        array_destroy(rows);
        holiday_close(holiday);
        datetime_dealloc(end);
        datetime_dealloc(start);
        if (!holiday) {
            test_set_failure_detailf("holiday data unavailable; install MARS Lab to provision the holiday rule source");
        }
        return NULL;
    }

    raw_events = holiday_between(holiday, start, end);
    if (!raw_events) {
        test_set_failure_detailf("holiday query failed: %s",
                                 holiday_last_error(holiday) ? holiday_last_error(holiday) : "holiday error");
        array_destroy(rows);
        rows = NULL;
        goto done;
    }

    for (i = 0u; i < array_size(raw_events); ++i) {
        const holiday_event_t *event = array_get(raw_events, i);

        if (!collect_event(event, rows)) {
            test_set_failure_detailf("failed to collect holiday event");
            array_destroy(rows);
            rows = NULL;
            goto done;
        }
    }

done:
    array_destroy(raw_events);
    holiday_close(holiday);
    datetime_dealloc(end);
    datetime_dealloc(start);
    return rows;
}

static const holiday_case_row_t *find_event_by_name_and_date(const array_t *events,
                                                             const char *holiday_name,
                                                             const char *holiday_date)
{
    size_t i;

    if (!events || !holiday_name || !holiday_date)
        return NULL;

    for (i = 0u; i < array_size(events); ++i) {
        const holiday_case_row_t *row = array_get(events, i);

        if (row && row->holiday_name && row->holiday_date &&
            strcmp(row->holiday_name, holiday_name) == 0 &&
            datetime_matches_iso_date(row->holiday_date, holiday_date))
            return row;
    }

    return NULL;
}

static const holiday_case_row_t *find_event_by_id_and_date(const array_t *events,
                                                           int holiday_id,
                                                           const char *holiday_date)
{
    size_t i;

    if (!events || !holiday_date)
        return NULL;

    for (i = 0u; i < array_size(events); ++i) {
        const holiday_case_row_t *row = array_get(events, i);

        if (row && row->holiday_id == holiday_id && row->holiday_date &&
            datetime_matches_iso_date(row->holiday_date, holiday_date))
            return row;
    }

    return NULL;
}

static bool assert_event_present(const array_t *events,
                                 const char *holiday_name,
                                 const char *holiday_date)
{
    char *found;

    if (find_event_by_name_and_date(events, holiday_name, holiday_date))
        return true;
    found = describe_events(events);
    test_set_failure_detailf("expected %s on %s; found: %s",
                             holiday_name ? holiday_name : "(null)",
                             holiday_date ? holiday_date : "(null)",
                             found ? found : "(unavailable)");
    free(found);
    return false;
}

static bool assert_event_absent(const array_t *events,
                                const char *holiday_name,
                                const char *holiday_date)
{
    char *found;

    if (!find_event_by_name_and_date(events, holiday_name, holiday_date))
        return true;
    found = describe_events(events);
    test_set_failure_detailf("did not expect %s on %s; found: %s",
                             holiday_name ? holiday_name : "(null)",
                             holiday_date ? holiday_date : "(null)",
                             found ? found : "(unavailable)");
    free(found);
    return false;
}

static void test_holiday_database_builds_and_opens(void)
{
    holiday_t *holiday;

    holiday = holiday_open(NULL);
    ASSERT_NOT_NULL(holiday);
    holiday_close(holiday);
}

static void test_england_christmas_pair_substitutions_in_2021(void)
{
    array_t *events = load_events("GB-ENG", 2021, DT_December, 25, 2021, DT_December, 31);

    ASSERT_NOT_NULL(events);
    print_events_summary("England 2021 Christmas window", events);
    ASSERT_EQ_LONG((long)array_size(events), 4L);
    ASSERT_TRUE(assert_event_present(events, "Bank Holiday in Lieu of Christmas Day", "2021-12-27"));
    ASSERT_TRUE(assert_event_present(events, "Bank Holiday in Lieu of Boxing Day", "2021-12-28"));
    ASSERT_TRUE(assert_event_present(events, "Christmas Day", "2021-12-25"));
    ASSERT_TRUE(assert_event_present(events, "Boxing Day", "2021-12-26"));

    array_destroy(events);
}

static void test_england_exception_holidays_land_on_correct_dates_in_2022(void)
{
    array_t *events = load_events("GB-ENG", 2022, DT_January, 1, 2022, DT_December, 31);

    ASSERT_NOT_NULL(events);
    print_events_summary("England 2022 exceptions", events);
    ASSERT_TRUE(assert_event_present(events, "Spring Bank Holiday", "2022-06-02"));
    ASSERT_TRUE(assert_event_present(events, "Platinum Jubilee Bank Holiday", "2022-06-03"));
    ASSERT_TRUE(assert_event_present(events, "State Funeral of Queen Elizabeth II", "2022-09-19"));

    array_destroy(events);
}

static void test_south_africa_observes_sunday_only_not_saturday(void)
{
    array_t *observed_events = load_events("ZA", 2023, DT_January, 1, 2023, DT_January, 2);
    array_t *saturday_events = load_events("ZA", 2026, DT_December, 25, 2026, DT_December, 28);

    ASSERT_NOT_NULL(observed_events);
    print_events_summary("South Africa 2023 New Year window", observed_events);
    ASSERT_TRUE(assert_event_present(observed_events, "New Year's Day", "2023-01-01"));
    ASSERT_TRUE(assert_event_present(observed_events, "New Year's Day (observed)", "2023-01-02"));

    ASSERT_NOT_NULL(saturday_events);
    print_events_summary("South Africa 2026 Christmas window", saturday_events);
    ASSERT_TRUE(assert_event_present(saturday_events, "Christmas Day", "2026-12-25"));
    ASSERT_TRUE(assert_event_present(saturday_events, "Day of Goodwill", "2026-12-26"));
    ASSERT_TRUE(assert_event_absent(saturday_events, "Day of Goodwill (observed)", "2026-12-28"));

    array_destroy(saturday_events);
    array_destroy(observed_events);
}

static void test_netherlands_kings_day_moves_to_previous_weekday_in_2025(void)
{
    array_t *events = load_events("NL", 2025, DT_April, 26, 2025, DT_April, 27);

    ASSERT_NOT_NULL(events);
    print_events_summary("Netherlands 2025 King's Day window", events);
    ASSERT_TRUE(assert_event_present(events, "Koningsdag", "2025-04-26"));
    ASSERT_TRUE(assert_event_absent(events, "Koningsdag", "2025-04-27"));

    array_destroy(events);
}

static void test_ireland_historic_whit_monday_exists_in_1960(void)
{
    array_t *events = load_events("IE", 1960, DT_January, 1, 1960, DT_December, 31);
    datetime_t *easter = datetime_init_easter(datetime_alloc(), 1960);
    char expected_whit_monday[16];

    ASSERT_NOT_NULL(events);
    ASSERT_NOT_NULL(easter);
    print_events_summary("Ireland 1960 holidays", events);
    datetime_add_days(easter, 50);
    snprintf(expected_whit_monday,
             sizeof(expected_whit_monday),
             "%04d-%02d-%02d",
             datetime_year(easter),
             (int)datetime_month(easter),
             (int)datetime_day(easter));

    ASSERT_TRUE(assert_event_present(events, "Whit Monday", expected_whit_monday));
    ASSERT_TRUE(assert_event_absent(events, "June Bank Holiday", expected_whit_monday));

    datetime_dealloc(easter);
    array_destroy(events);
}

static void test_ukraine_future_rule_windows_cover_2030(void)
{
    array_t *events = load_events("UA", 2030, DT_January, 1, 2030, DT_December, 31);

    ASSERT_NOT_NULL(events);
    print_events_summary("Ukraine 2030 holidays", events);
    ASSERT_NOT_NULL(find_event_by_id_and_date(events, 2000003, "2030-12-25"));
    ASSERT_NOT_NULL(find_event_by_id_and_date(events, 2000014, "2030-07-15"));
    ASSERT_NOT_NULL(find_event_by_id_and_date(events, 2000016, "2030-08-24"));
    ASSERT_NOT_NULL(find_event_by_id_and_date(events, 2000019, "2030-10-01"));

    array_destroy(events);
}

static void test_holiday_queries_weekend_and_holiday_status(void)
{
    holiday_t *holiday = holiday_open("ZA");
    datetime_t *new_year = datetime_init_ymd(datetime_alloc(), 2023, DT_January, 1);
    datetime_t *working_tuesday = datetime_init_ymd(datetime_alloc(), 2023, DT_January, 3);

    ASSERT_NOT_NULL(holiday);
    ASSERT_NOT_NULL(new_year);
    ASSERT_NOT_NULL(working_tuesday);
    printf("    South Africa 2023-01-01: weekend=%s national_holiday=%s\n",
           holiday_is_weekend(holiday, new_year) ? "true" : "false",
           holiday_is_national_holiday(holiday, new_year) ? "true" : "false");
    printf("    South Africa 2023-01-03: weekend=%s national_holiday=%s\n",
           holiday_is_weekend(holiday, working_tuesday) ? "true" : "false",
           holiday_is_national_holiday(holiday, working_tuesday) ? "true" : "false");
    ASSERT_TRUE(holiday_is_weekend(holiday, new_year));
    ASSERT_TRUE(holiday_is_national_holiday(holiday, new_year));
    ASSERT_TRUE(!holiday_is_weekend(holiday, working_tuesday));
    ASSERT_TRUE(!holiday_is_national_holiday(holiday, working_tuesday));

    datetime_dealloc(working_tuesday);
    datetime_dealloc(new_year);
    holiday_close(holiday);
}

static void test_holiday_working_days_between_counts_business_days(void)
{
    holiday_t *holiday = holiday_open("GB-ENG");
    datetime_t *start = datetime_init_ymd(datetime_alloc(), 2021, DT_December, 24);
    datetime_t *end = datetime_init_ymd(datetime_alloc(), 2021, DT_December, 31);
    long working_days;

    ASSERT_NOT_NULL(holiday);
    ASSERT_NOT_NULL(start);
    ASSERT_NOT_NULL(end);
    working_days = holiday_working_days_between(holiday, start, end);
    printf("    England working days from 2021-12-24 to 2021-12-31: %ld\n", working_days);
    ASSERT_EQ_LONG(working_days, 4L);

    datetime_dealloc(end);
    datetime_dealloc(start);
    holiday_close(holiday);
}

static void example_holiday_readme_queries(void)
{
    holiday_t *holiday = holiday_open("GB-ENG");
    array_t *events = NULL;
    datetime_t *bank_holiday = datetime_init_ymd(datetime_alloc(), 2021, DT_December, 25);
    datetime_t *range_start = datetime_init_ymd(datetime_alloc(), 2021, DT_December, 24);
    datetime_t *range_end = datetime_init_ymd(datetime_alloc(), 2021, DT_December, 31);
    long working_days;
    size_t i;

    ASSERT_NOT_NULL(holiday);
    ASSERT_NOT_NULL(bank_holiday);
    ASSERT_NOT_NULL(range_start);
    ASSERT_NOT_NULL(range_end);
    events = holiday_between(holiday, range_start, range_end);
    ASSERT_NOT_NULL(events);
    ASSERT_EQ_LONG((long)array_size(events), 4L);

    printf("Holidays between 2021-12-24 and 2021-12-31:\n");
    for (i = 0u; i < array_size(events); ++i) {
        holiday_event_t *event = array_get(events, i);
        char *date_text;

        ASSERT_NOT_NULL(event);
        date_text = datetime_format(event->holiday_date, "%yyyy-%MM-%dd");
        printf("- %s: %s\n",
               event->holiday_name ? event->holiday_name : "(unavailable)",
               date_text ? date_text : "(unavailable)");
        free(date_text);
    }

    printf("2021-12-25 weekend: %s\n",
           holiday_is_weekend(holiday, bank_holiday) ? "yes" : "no");
    printf("2021-12-25 national holiday: %s\n",
           holiday_is_national_holiday(holiday, bank_holiday) ? "yes" : "no");

    working_days = holiday_working_days_between(holiday, range_start, range_end);
    ASSERT_EQ_LONG(working_days, 4L);
    printf("Working days between 2021-12-24 and 2021-12-31: %ld\n", working_days);

    array_destroy(events);
    datetime_dealloc(range_end);
    datetime_dealloc(range_start);
    datetime_dealloc(bank_holiday);
    holiday_close(holiday);
}

static bool holiday_suite_setup(void)
{
    holiday_t *holiday = holiday_open("GB-ENG");

    if (holiday) {
        holiday_close(holiday);
        return true;
    }

    fprintf(stderr,
            "Holiday tests require the configured holiday rule source.\n"
            "Install MARS Lab first, for example with `make install-mars-lab`.\n");
    return false;
}

static void holiday_suite_cleanup(void)
{
}

int tests_main(void)
{
    TEST_SECTION("Holiday Rules");
    TEST_RUN_IN_GROUP(test_holiday_database_builds_and_opens, tests, NULL);
    TEST_RUN_IN_GROUP(test_england_christmas_pair_substitutions_in_2021, tests, NULL);
    TEST_RUN_IN_GROUP(test_england_exception_holidays_land_on_correct_dates_in_2022, tests, NULL);
    TEST_RUN_IN_GROUP(test_south_africa_observes_sunday_only_not_saturday, tests, NULL);
    TEST_RUN_IN_GROUP(test_netherlands_kings_day_moves_to_previous_weekday_in_2025, tests, NULL);
    TEST_RUN_IN_GROUP(test_ireland_historic_whit_monday_exists_in_1960, tests, NULL);
    TEST_RUN_IN_GROUP(test_ukraine_future_rule_windows_cover_2030, tests, NULL);
    TEST_RUN_IN_GROUP(test_holiday_queries_weekend_and_holiday_status, tests, NULL);
    TEST_RUN_IN_GROUP(test_holiday_working_days_between_counts_business_days, tests, NULL);

    TEST_SECTION("README Output Examples");
    printf(C_BOLD C_YELLOW "Running README examples...\n" C_RESET);
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_holiday_readme_queries, readme_examples,
                                  "holiday,readme,output");
    return TEST_EXIT_CODE();
}
