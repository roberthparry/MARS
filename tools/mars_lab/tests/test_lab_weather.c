/**
 * @file test_lab_weather.c
 * @brief Database-backed weather failure and credential-redaction regressions.
 *
 * Exercises native transport/provider diagnostics with synthetic metadata, without external requests or
 * access to an account key. Isolated calendar fixtures verify that unavailable DateTime cards and Almanac
 * observing conditions preserve the same actionable status without reading the user's weather cache.
 */
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "http-t.h"
#include "lab_calendar.h"
#include "test_harness.h"
#include "test_database.h"
#include "test_lab_support.h"
#define MARS_SHARED_LAB_CALENDAR_INTERNAL_ACCESS
#include "internal/lab_calendar_internal.h"

static void weather_diagnostic_row(const void *data)
{
    const test_database_case_t *row = data;
    json_t *input = json_from_text(row->input);
    bool client_ready = false;
    unsigned error = 0, status = 0;
    bool valid = input && json_bool_value(lab_cal_get(input, "client"), &client_ready) &&
                 lab_cal_integer(lab_cal_text(input, "error"), UINT_MAX, &error) &&
                 lab_cal_integer(lab_cal_text(input, "status"), 599, &status);
    string_t *detail = string_new_with(lab_cal_text(input, "detail"));
    string_t *diagnostic = valid ? lab_cal_weather_failure(client_ready, (http_error_t)error, detail, status,
                                                          lab_cal_get(input, "payload")) : NULL;
    const char *actual = diagnostic ? string_c_str(diagnostic) : "";
    bool ok = valid && !strcmp(actual, string_c_str(row->expected)) &&
              !strstr(actual, "fixture-weather-key") && !strstr(actual, "https://");
    if (!ok)
        printf("%s: got '%s'; expected '%s'\n", string_c_str(row->name), actual, string_c_str(row->expected));
    string_free(diagnostic);
    string_free(detail);
    json_free(input);
    TEST_ASSERT_TRUE(ok, "weather failures explain the cause without copying credentials or raw diagnostics");
}

static void test_lab_weather_diagnostics(void)
{
    test_run_database_cases(__FILE__, __LINE__, "mars_lab.weather_diagnostics", NULL, weather_diagnostic_row);
}

static bool weather_status_fixture(const char *directory)
{
    (void)directory;
    json_t *options = test_lab_json("{\"date\":\"2000-01-01\",\"time\":\"12:00:00\",\"lat\":\"52.7\","
                                    "\"lon\":\"-2.75\",\"elevation\":\"75\",\"zone\":\"0\"}");
    json_t *weather = lab_cal_weather(options);
    const json_t *fields = lab_cal_get(weather, "fields");
    const json_t *section = json_array_get(lab_cal_get(weather, "overview_sections"), 0);
    const json_t *row = json_array_get(lab_cal_get(section, "rows"), 0);
    const char *expected = "The selected date is outside the weather provider's supported range.";
    bool available = true;
    bool ok = json_bool_value(lab_cal_get(weather, "available"), &available) && !available &&
              !strcmp(lab_cal_text(fields, "weather_status"), expected) &&
              !strcmp(lab_cal_text(row, "value"), expected);
    lab_cal_atmosphere(options);
    ok = ok && !strcmp(lab_cal_text(options, "weather_status"), expected) &&
         *lab_cal_text(options, "weather_source");
    json_free(weather);
    json_free(options);
    return ok;
}

static void test_lab_weather_status_cards(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(weather_status_fixture),
                     "DateTime status rows and Almanac fallback conditions retain the native weather failure");
}

/* Register offline weather regressions with the sequential native suite. */
void test_lab_weather_cases(void)
{
    TEST_RUN_IN_GROUP(test_lab_weather_diagnostics, tests, "weather,diagnostics");
    TEST_RUN_IN_GROUP(test_lab_weather_status_cards, tests, "weather,diagnostics");
}
