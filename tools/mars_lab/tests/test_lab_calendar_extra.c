/**
 * @file test_lab_calendar_extra.c
 * @brief Native Lab named-town offsets and DateTime card regressions.
 *
 * Standard sequential native Lab harness registrations exercise
 * New York and Los Angeles in summer and winter, the spring DST boundary,
 * invalid-zone fallback, and exact restoration of unset, empty and populated
 * TZ values. DateTime card and location fixtures exercise the browser contract
 * without requiring an installed jurisdiction database. Environment-changing
 * checks use the suite's isolated fixture and never modify the parent process.
 */
#include <stdlib.h>
#include <string.h>

#include "datetime.h"
#include "internal/lab_calendar_internal.h"
#include "lab_process.h"
#include "sqlite.h"
#include "test_harness.h"
#include "test_lab_support.h"

static bool same_environment(const char *expected)
{
    const char *actual = getenv("TZ");
    return expected ? actual && !strcmp(actual, expected) : actual == NULL;
}

static bool check_zone(const char *name, const char *date_text, double expected, int hour, const char *environment)
{
    string_t *zone = string_new_with(name), *date = string_new_with(date_text);
    datetime_t *moment = datetime_from_string(date_text);
    double jd = datetime_jdn(moment); /* Noon UTC on the selected civil date. */
    datetime_dealloc(moment);
    struct tm local;
    double event_offset = 123, noon_offset = 123;
    bool result = lab_cal_timezone_at(zone, jd, &local, &event_offset) && event_offset == expected &&
                  local.tm_hour == hour && same_environment(environment) &&
                  lab_cal_timezone_noon(zone, date, &noon_offset) && noon_offset == expected &&
                  same_environment(environment);
    string_free(zone);
    string_free(date);
    return result;
}

static bool check_transition(void)
{
    string_t *zone = string_new_with("America/New_York");
    double midnight = datetime_ymd_to_jdn(2026, (month_t)3, 8) - 0.5;
    struct tm before, after;
    double before_offset, after_offset;
    bool result = lab_cal_timezone_at(zone, midnight + (6 * 3600 + 59 * 60) / 86400.0, &before, &before_offset) &&
                  lab_cal_timezone_at(zone, midnight + 7 / 24.0, &after, &after_offset) && before_offset == -5 &&
                  before.tm_hour == 1 && before.tm_min == 59 && after_offset == -4 && after.tm_hour == 3 &&
                  after.tm_min == 0;
    string_free(zone);
    return result;
}

static bool check_invalid(const char *name, const char *environment)
{
    string_t *zone = string_new_with(name), *date = string_new_with("2026-07-15");
    struct tm local = {.tm_hour = 17};
    double fallback = 5.5;
    double jd = datetime_ymd_to_jdn(2026, (month_t)7, 15);
    bool result = !lab_cal_timezone_at(zone, jd, &local, &fallback) && fallback == 5.5 && local.tm_hour == 17 &&
                  !lab_cal_timezone_noon(zone, date, &fallback) && fallback == 5.5 && same_environment(environment);
    string_free(zone);
    string_free(date);
    return result;
}

static bool timezone_fixture(const char *directory)
{
    (void)directory;
    const char *previous = getenv("TZ");
    bool was_set = previous != NULL;
    string_t *saved = previous ? string_new_with(previous) : NULL;
    if (was_set && !saved)
        return false;
    static const char *environments[] = {NULL, "", "Europe/London"};
    static const char *invalid[] = {
        "", "America/Does_Not_Exist", "../etc/passwd", "/etc/passwd", "America//New_York", "UTC0", "right/UTC"};
    bool passed = true;
    for (size_t i = 0; i < sizeof(environments) / sizeof(*environments) && passed; ++i) {
        const char *environment = environments[i];
        if ((environment ? setenv("TZ", environment, 1) : unsetenv("TZ")) != 0) {
            passed = false;
            break;
        }
        tzset();
        passed = check_zone("America/New_York", "2026-01-15", -5, 7, environment) &&
                 check_zone("America/New_York", "2026-07-15", -4, 8, environment) &&
                 check_zone("America/Los_Angeles", "2026-01-15", -8, 4, environment) &&
                 check_zone("America/Los_Angeles", "2026-07-15", -7, 5, environment) && check_transition() &&
                 same_environment(environment);
        for (size_t j = 0; j < sizeof(invalid) / sizeof(*invalid) && passed; ++j)
            passed = check_invalid(invalid[j], environment);
    }
    int restored = was_set ? setenv("TZ", string_c_str(saved), 1) : unsetenv("TZ");
    string_free(saved);
    if (restored)
        abort();
    tzset();
    return passed;
}

static void test_lab_calendar_named_timezones(void)
{
    bool passed = test_lab_isolated(timezone_fixture);
    TEST_ASSERT_TRUE(passed,
                     "New York and Los Angeles seasonal offsets, DST transition, invalid zones and TZ restoration");
}

static bool location_fixture(const char *directory)
{
    (void)directory;
    json_t *fields = test_lab_json("{\"jurisdiction_latitude\":\"40.712800\","
                                   "\"jurisdiction_longitude\":\"-74.006000\",\"jurisdiction_gmt_offset\":\"5.5\"}");
    static const struct {
        const char *zone;
        const char *date;
        const char *offset;
    } cases[] = {{"America/New_York", "2026-01-15", "-5"},
                 {"America/New_York", "2026-07-15", "-4"},
                 {"America/Los_Angeles", "2026-01-15", "-8"},
                 {"America/Los_Angeles", "2026-07-15", "-7"},
                 {"America/Does_Not_Exist", "2026-07-15", "5.5"}};
    bool passed = fields != NULL;
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); ++i) {
        json_t *options = json_new_object();
        lab_cal_set(options, "jurisdiction", "US");
        lab_cal_set(options, "timezone", cases[i].zone);
        lab_cal_set(options, "date", cases[i].date);
        json_t *response = lab_cal_location_fields(fields, options);
        passed = passed && test_lab_ok(response, true) && !strcmp(test_lab_text(response, "jurisdiction"), "US") &&
                 !strcmp(test_lab_text(response, "latitude"), "40.712800") &&
                 !strcmp(test_lab_text(response, "longitude"), "-74.006000") &&
                 !strcmp(test_lab_text(response, "gmt_offset"), cases[i].offset) &&
                 !strcmp(test_lab_text(response, "source"), "holiday_lab");
        json_free(response);
        json_free(options);
    }
    json_free(fields);
    return passed;
}

static void test_lab_calendar_location_timezone(void)
{
    bool passed = test_lab_isolated(location_fixture);
    TEST_ASSERT_TRUE(passed,
                     "location response uses town seasonal offset and preserves native offset for invalid zones");
}

static const json_t *section_rows(const json_t *response, const char *key, size_t index)
{
    return test_lab_member(json_array_get(test_lab_member(response, key), index), "rows");
}

static bool row_matches(const json_t *rows, size_t index, const char *label, const char *value)
{
    const json_t *row = json_array_get(rows, index);
    return !strcmp(test_lab_text(row, "label"), label) && !strcmp(test_lab_text(row, "value"), value);
}

static void test_lab_calendar_datetime_cards(void)
{
    json_t *fields = test_lab_json(
        "{\"date\":\"2026-07-15\",\"weekday\":\"Wednesday\",\"julian_day_number\":\"2461237\","
        "\"gmt_offset\":\"-4\",\"moon_phase\":\"new moon\",\"sunrise\":\"05:40\",\"sunset\":\"20:25\","
        "\"moonrise\":\"unavailable\",\"moonrise_status\":\"moon remains below the horizon\",\"moonset\":\"21:30\","
        "\"dst_forward\":\"2026-03-08 02:00\",\"dst_forward_from_offset\":\"-5\",\"dst_forward_to_offset\":\"-4\","
        "\"dst_back\":\"2026-11-01 02:00\",\"dst_back_from_offset\":\"-4\",\"dst_back_to_offset\":\"-5\","
        "\"dst_status\":\"ok\",\"start\":\"2026-07-01\",\"end\":\"2026-07-15\",\"days_between\":\"14\","
        "\"days_between_abs\":\"14\",\"duration_years\":\"0\",\"duration_months\":\"0\",\"duration_days\":\"14\","
        "\"calendar_section_christian\":\"current date\\t2026-07-15\\nEaster Sunday\\t2026-04-05\\n"
        "Christmas Day\\t2026-12-25\",\"bank_holiday\":\"Independence Day: 2026-07-04\","
        "\"latitude\":\"40.7128\",\"longitude\":\"-74.006\",\"elevation_metres\":\"10\","
        "\"solar_declination\":\"21.5\",\"solar_inclination\":\"19.2\",\"solar_max_altitude\":\"70.8\"}");
    json_t *response = lab_cal_datetime(fields);
    const json_t *sun = section_rows(response, "overview_sections", 1);
    const json_t *calendar = section_rows(response, "calendar_sections", 0);
    bool passed =
        test_lab_ok(response, true) && !strcmp(test_lab_text(response, "mode"), "datetime") &&
        row_matches(section_rows(response, "overview_sections", 0), 0, "Date", "2026-07-15") &&
        row_matches(section_rows(response, "overview_sections", 0), 2, "Time basis", "GMT offset -4") &&
        row_matches(sun, 0, "Sunrise", "05:40") && row_matches(sun, 2, "Moonrise", "moon remains below the horizon") &&
        row_matches(sun, 5, "Clocks forward", "Sunday 8th March 2026 at 02:00, from GMT-5 to GMT-4") &&
        row_matches(sun, 6, "Clocks back", "Sunday 1st November 2026 at 02:00, from GMT-4 to GMT-5") &&
        row_matches(section_rows(response, "range_sections", 0), 4, "Calendar span", "0 years, 0 months, 14 days") &&
        json_array_size(test_lab_member(response, "calendar_sections")) == 10 &&
        row_matches(calendar, 0, "Current date", "2026-07-15") &&
        row_matches(calendar, 1, "Easter Sunday", "2026-04-05") &&
        row_matches(calendar, 2, "Christmas Day", "2026-12-25") &&
        row_matches(section_rows(response, "local_sections", 0), 0, "Independence Day", "2026-07-04") &&
        row_matches(section_rows(response, "solar_sections", 0), 2, "Elevation", "10 m") &&
        row_matches(section_rows(response, "solar_sections", 1), 0, "Solar declination", "21.5°") &&
        !strcmp(test_lab_text(test_lab_member(response, "fields"), "julian_day_number"), "2461237");
    static const char *cards[] = {"overview", "range", "calendar", "local", "solar"};
    for (size_t i = 0; i < sizeof(cards) / sizeof(*cards); ++i)
        passed = passed && *test_lab_text(response, cards[i]);
    json_free(response);
    lab_cal_set(fields, "bank_holiday", "");
    lab_cal_set(fields, "holiday_notice", "Jurisdiction database unavailable");
    lab_cal_set(fields, "dst_status", "none");
    response = lab_cal_datetime(fields);
    passed = passed &&
             row_matches(section_rows(response, "local_sections", 0), 0, "Holiday database",
                         "Jurisdiction database unavailable") &&
             row_matches(section_rows(response, "overview_sections", 1), 5, "Daylight saving",
                         "No daylight saving changes this year");
    json_free(response);
    json_free(fields);
    TEST_ASSERT_TRUE(passed,
                     "DateTime cards retain native values, observance order, DST text and unavailable-data notices");
}

static bool calendar_worker_fixture(const char *directory)
{
    json_t *options = test_lab_json("{\"date\":\"2026-10-05\",\"year\":\"2026\",\"lat\":\"52.7077\","
                                    "\"lon\":\"-2.7541\",\"gmt_offset\":\"1\",\"elevation\":\"0\","
                                    "\"start\":\"2026-01-01\",\"end\":\"2026-12-31\"}");
    string_t *path = string_sprintf("%s/calendar-cache.db", directory);
    string_t *key = string_new_with("calendar-regression-fixture");
    string_t *name = string_new_with(
        "mars_lab/datetime_lab_output_v4/date=2026-10-05/jdn=none:0/start=2026-01-01/end=2026-12-31/year=2026/"
        "jurisdiction=/lat=52.707700000/lon=-2.754100000/elevation=0.000000000/gmt_offset=1.000000000");
    string_t *legacy = string_new_with("date 2026-10-05\njulian_day_number 2461319\n"
                                       "christian_calendar_date Gregorian 2026-10-05\neaster 2026-04-05\n");
    sqlite_t *db = path && key ? sqlite_open_encrypted(path, key) : NULL;
    bool prepared = db && sqlite_init_object_store(db) && sqlite_store_string(db, name, legacy) &&
                    !setenv("MARS_LAB_OBJECT_STORE_PATH", string_c_str(path), 1) &&
                    !setenv("MARS_LAB_OBJECT_STORE_KEY", string_c_str(key), 1) && !unsetenv("MARS_LAB_DATETIME_BINARY");
    sqlite_close(db);
    string_t *diagnostic = NULL;
    json_t *fields = prepared ? lab_cal_run("datetime_lab", options, 10000, &diagnostic) : NULL;
    json_t *response = fields ? lab_cal_datetime(fields) : NULL;
    static const size_t expected[] = {5, 2, 4, 4, 7, 5, 5, 3, 3, 6};
    bool ok = fields && json_array_size(test_lab_member(response, "calendar_sections")) == 10;
    for (size_t i = 0; i < sizeof(expected) / sizeof(*expected); ++i) {
        size_t count = json_array_size(section_rows(response, "calendar_sections", i));
        if (count != expected[i]) {
            string_printf("Calendar section %zu: expected %zu rows, received %zu\n", i, expected[i], count);
            ok = false;
        }
    }
    if (!ok) {
        string_t *text = fields ? json_to_string(fields) : string_clone(diagnostic);
        string_printf("%S\n", text);
        string_free(text);
    }
    /* Preserve the old record, and verify complete native output survives a new-cache round trip. */
    db = prepared ? sqlite_open_encrypted(path, key) : NULL;
    string_t *retained = NULL;
    ok = db && sqlite_load_string(db, name, &retained) && string_compare(retained, legacy) == 0 && ok;
    string_free(retained);
    sqlite_close(db);
    string_t *binary = lab_proc_worker_path("datetime_lab");
    const char *argv[] = {binary ? string_c_str(binary) : "",
                          "date=2026-10-05",
                          "year=2026",
                          "lat=52.7077",
                          "lon=-2.7541",
                          "gmt_offset=1",
                          "elevation=0",
                          "start=2026-01-01",
                          "end=2026-12-31",
                          "cache_put=1",
                          NULL};
    string_t *output = NULL;
    int status = -1;
    bool stored = ok && binary && lab_proc_run_input(argv, NULL, diagnostic, 10000, 4096, &output, &status) && !status;
    string_free(output);
    string_free(binary);
    lab_cal_set(options, "cache_only", "1");
    json_t *cached = stored ? lab_cal_run("datetime_lab", options, 10000, NULL) : NULL;
    string_t *original_text = fields ? json_to_string(fields) : NULL;
    string_t *cached_text = cached ? json_to_string(cached) : NULL;
    ok = original_text && cached_text && string_compare(original_text, cached_text) == 0 && ok;
    string_free(cached_text);
    string_free(original_text);
    json_free(cached);
    string_free(legacy);
    string_free(name);
    string_free(key);
    string_free(path);
    json_free(response);
    json_free(fields);
    json_free(options);
    string_free(diagnostic);
    return ok;
}

static void test_lab_calendar_worker_observances(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(calendar_worker_fixture),
                     "real datetime worker retains every calendar observance and sunset-start row");
}

/* Register only with the standard sequential native Lab suite. */
void test_lab_calendar_extra_cases(void)
{
    TEST_RUN_IN_GROUP(test_lab_calendar_named_timezones, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_calendar_location_timezone, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_calendar_datetime_cards, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_calendar_worker_observances, tests, NULL);
}
