/**
 * @file test_lab_evaluation.c
 * @brief Native worker and browser response-contract integration regressions.
 *
 * Calls the public-to-tools evaluation and calendar adapters with real prebuilt
 * Lab workers. Mathematical modes must return native result fields and typed
 * binding arrays. Optional TeX rendering may fail without losing the result.
 * Validation cases avoid external weather requests and database dependencies.
 * Deadline policy and failure-response cases call pure private helpers, without
 * spawning a worker or waiting for a real timeout.
 */
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "almanac-t.h"
#include "file-t.h"
#include "number-t.h"
#include "sqlite-t.h"
#include "lab_calendar.h"
#include "lab_evaluate.h"
#include "lab_state.h"
#define MARS_SHARED_LAB_EVALUATE_INTERNAL_ACCESS
#include "internal/lab_evaluate_internal.h"
#define MARS_SHARED_LAB_CALENDAR_INTERNAL_ACCESS
#include "internal/lab_calendar_internal.h"
#include "test_harness.h"
#include "test_lab_support.h"

static json_t *evaluate(const char *path, const char *body, unsigned *status)
{
    string_t *route = string_new_with(path);
    json_t *payload = test_lab_json(body);
    json_t *response = route && payload ? lab_eval_request(route, payload, status) : NULL;
    json_free(payload);
    string_free(route);
    return response;
}

static void test_lab_math_modes(void)
{
    static const struct {
        const char *route;
        const char *payload;
        const char *mode;
        const char *field;
    } cases[] = {{"/eval", "{\"expression\":\"2+3\"}", "expression", "display_expression"},
                 {"/equation-eval", "{\"equation\":\"x=2\"}", "equation", "display_equation"},
                 {"/diffequation-eval", "{\"diffequation\":\"Dx(y)=y\"}", "diffequation", "display_TeX"},
                 {"/matrix-eval", "{\"matrix\":\"(1,2;3,4)\",\"operation\":\"det\"}", "matrix", "display_result"},
                 {"/integrator-eval", "{\"expression\":\"x\",\"bounds\":[{\"name\":\"x\",\"lo\":\"0\",\"hi\":\"1\"}]}",
                  "integrator", "value"},
                 {"/goal_seek", "{\"expression\":\"{x | x = 0}\",\"target\":\"2\",\"start\":{\"x\":\"0\"}}",
                  "goal_seek", "display_expression"}};
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); ++i) {
        unsigned status = 0;
        json_t *response = evaluate(cases[i].route, cases[i].payload, &status);
        bool ok = status == 200 && test_lab_ok(response, true) &&
                  !strcmp(test_lab_text(response, "mode"), cases[i].mode) && *test_lab_text(response, "input") &&
                  *test_lab_text(response, cases[i].field) &&
                  json_type(test_lab_member(response, "binding_values")) == JSON_ARRAY;
        if (!ok)
            string_printf("native Lab route %s returned status %u: %s\n", cases[i].route, status,
                          test_lab_text(response, "error"));
        json_free(response);
        TEST_ASSERT_TRUE(ok, "each mathematical mode returns the browser contract using its actual worker");
    }
}

static void test_lab_expression_contract(void)
{
    unsigned status = 0;
    json_t *response = evaluate("/eval", "{\"expression\":\"{x+1 | x=2}\"}", &status);
    bool ok =
        status == 200 && test_lab_ok(response, true) && !strcmp(test_lab_text(response, "value"), "3") &&
        string_find(json_string_value(test_lab_member(response, "display_expression")), "x") >= 0 &&
        string_find(json_string_value(test_lab_member(response, "display_function")), "x") >= 0 &&
        string_find(json_string_value(test_lab_member(response, "display_TeX")), "x") >= 0 &&
        !strcmp(test_lab_text(response, "display_expression"), test_lab_text(response, "full_display_expression"));
    const json_t *bindings = test_lab_member(response, "binding_values");
    ok = ok && json_array_size(bindings) == 1 && !strcmp(test_lab_text(json_array_get(bindings, 0), "name"), "x");
    json_free(response);
    TEST_ASSERT_TRUE(ok, "bindings evaluate only Value while symbolic cards and native binding arrays survive");
    response = evaluate("/eval", "{\"expression\":\"sin(x)^2+cos(x)^2\"}", &status);
    ok = status == 200 && test_lab_ok(response, true) && !strcmp(test_lab_text(response, "value"), "1");
    json_free(response);
    TEST_ASSERT_TRUE(ok, "binding-independent simplification exposes a numerical Value card");
}

static void test_lab_function_input(void)
{
    unsigned status = 0;
    json_t *response = evaluate("/function-run", "{\"source\":\"output(2+3).\",\"precision\":50}", &status);
    bool ok = status == 200 && test_lab_ok(response, true) && !strcmp(test_lab_text(response, "output"), "5\n") &&
              !*test_lab_text(response, "error");
    json_free(response);
    TEST_ASSERT_TRUE(ok, "Ophelia receives source on stdin and returns output/error fields");
}

static void test_lab_evaluation_validation(void)
{
    static const struct {
        const char *route;
        const char *payload;
    } cases[] = {{"/eval", "{}"},
                 {"/eval", "[]"},
                 {"/eval", "{\"expression\":\"x\",\"precision\":16}"},
                 {"/eval", "{\"expression\":\"x\",\"precision\":78915}"},
                 {"/eval", "{\"expression\":\"x\\u0000y\"}"},
                 {"/eval", "{\"expression\":\"x\",\"action\":\"unknown\"}"},
                 {"/matrix-eval", "{\"matrix\":\"[1]\",\"operation\":\"unknown\"}"},
                 {"/integrator-eval", "{\"expression\":\"x\",\"bounds\":\"invalid\"}"},
                 {"/function-run", "{\"source\":\"\"}"}};
    bool ok = true;
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); ++i) {
        unsigned status = 0;
        json_t *response = evaluate(cases[i].route, cases[i].payload, &status);
        ok = status == 400 && test_lab_ok(response, false) && *test_lab_text(response, "error") && ok;
        json_free(response);
    }
    unsigned status = 201;
    json_t *response = evaluate("/not-a-route", "{}", &status);
    ok = !response && status == 201 && ok;
    json_free(response);
    response = evaluate("/eval", "{\"expression\":\"sin(\"}", &status);
    ok = status == 422 && test_lab_ok(response, false) && *test_lab_text(response, "error") && ok;
    json_free(response);
    response = evaluate("/render_TeX", "{\"tex\":\"\\\\input{/etc/passwd}\"}", &status);
    ok = status == 422 && test_lab_ok(response, false) && ok;
    json_free(response);
    TEST_ASSERT_TRUE(ok, "invalid inputs, native parse errors, unsafe TeX and unknown routes have distinct contracts");
}

static void test_lab_calendar_validation(void)
{
    const char *routes[] = {"/datetime-eval", "/datetime-weather", "/datetime-jurisdiction-location", "/almanac-eval", "/almanac-weather",
                            "/almanac-land-totality"};
    json_t *payload = test_lab_json("{\"date\":\"not-a-date\"}");
    bool ok = payload != NULL;
    for (size_t i = 0; i < sizeof(routes) / sizeof(*routes); ++i) {
        string_t *route = string_new_with(routes[i]);
        unsigned status = 0;
        json_t *response = lab_cal_request(route, payload, &status);
        ok = status == 400 && test_lab_ok(response, false) && *test_lab_text(response, "error") && ok;
        json_free(response);
        string_free(route);
    }
    json_free(payload);
    TEST_ASSERT_TRUE(ok, "calendar routes reject malformed dates before network or database access");
    unsigned status = 0;
    string_t *route = string_new_with("/almanac-eval");
    payload = test_lab_json("{\"date\":\"2000-01-01\",\"town\":\"Fixture\",\"elevation\":\"12000\"}");
    json_t *response = lab_cal_request(route, payload, &status);
    TEST_ASSERT_TRUE(status == 400 && strstr(test_lab_text(response, "error"), "elevation"),
                     "out-of-range observer elevation is rejected before requesting weather");
    json_free(response);
    json_free(payload);
    payload = test_lab_json("{\"date\":\"2000-01-01\",\"town\":\"\",\"latitude\":\"0\","
                            "\"longitude\":\"0\",\"elevation\":\"-10\"}");
    response = lab_cal_request(route, payload, &status);
    TEST_ASSERT_TRUE(status == 400 && strstr(test_lab_text(response, "error"), "height of eye"),
                     "confirmed ocean coordinates reject a negative height of eye");
    json_free(response);
    json_free(payload);
    string_free(route);
}

static void check_weather_local_day(void)
{
    const struct { const char *date, *clock; int first, last, shift; } cases[] = {
        {"2026-10-23", "23:06:19", -3600, 79200, 1},
        {"2026-10-23", "00:06:19", 25200, 108000, -1},
        {"2026-10-23", "23:06:19", 25200, 108000, 0},
        {"2026-10-23", "12:06:19", -3600, 79200, 0},
        {"2026-10-31", "23:06:19", -50400, 32400, 1},
        {"2027-01-01", "00:06:19", 43200, 126000, -1},
        {"2026-10-23", "23:59:59", 0, 82800, 0},
        {"2026-10-23", "00:00:00", 0, 82800, 0},
        {"2026-10-23", "23:06:19", -20700, 62100, 1},
        {"2026-10-25", "23:06:19", -3600, 82800, 0},
        {"2026-03-29", "22:30:00", 0, 79200, 0},
        {"2026-03-29", "23:30:00", 0, 79200, 1}
    };
    for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
        json_t *options = test_lab_json("{\"observing_conditions\":\"yes\",\"zone\":\"-12\"}");
        lab_cal_set(options, "date", cases[i].date); lab_cal_set(options, "time", cases[i].clock);
        double midnight = floor(lab_cal_weather_epoch(options) / 86400) * 86400;
        string_t *source = string_sprintf("{\"forecast\":{\"forecastday\":[{\"date\":\"%s\",\"hour\":["
            "{\"time_epoch\":%.0f},{\"time_epoch\":%.0f}]}]}}", cases[i].date,
            midnight + cases[i].first, midnight + cases[i].last);
        json_t *weather = json_from_text(source);
        TEST_ASSERT_INT_EQ(lab_cal_weather_day_shift(options, weather), cases[i].shift);
        lab_cal_set(options, "observing_conditions", "");
        TEST_ASSERT_INT_EQ(lab_cal_weather_day_shift(options, weather), 0);
        json_free(weather); string_free(source); json_free(options);
    }
    json_t *options = test_lab_json("{\"date\":\"2026-10-23\",\"time\":\"23:06:19\","
                                    "\"observing_conditions\":\"yes\",\"lat\":\"51.5\",\"elevation\":\"39\"}");
    json_t *weather = test_lab_json("{\"forecast\":{\"forecastday\":[{\"date\":\"2026-10-23\",\"hour\":["
        "{\"time_epoch\":1792710000},{\"time_epoch\":1792792800}]},{\"date\":\"2026-10-24\",\"hour\":["
        "{\"time_epoch\":1792796400,\"temp_c\":10,\"pressure_mb\":1008},"
        "{\"time_epoch\":1792800000,\"temp_c\":12,\"pressure_mb\":1009}]}]}}");
    TEST_ASSERT_INT_EQ(lab_cal_weather_day_shift(options, weather), 0);
    lab_cal_observing_conditions(options, weather);
    double pressure = 0;
    TEST_ASSERT_TRUE(strstr(lab_cal_text(options, "weather_source"), "hourly interpolation") &&
                     lab_cal_number(lab_cal_text(options, "pressure_hpa"), &pressure) &&
                     pressure > 1003.3 && pressure < 1003.5 &&
                     !strcmp(lab_cal_text(options, "date"), "2026-10-23") &&
                     !strcmp(lab_cal_text(options, "time"), "23:06:19"),
                     "next local day supplies conditions without changing the selected GMT moment");
    json_free(weather);
    weather = test_lab_json("{\"forecast\":{\"forecastday\":[{\"date\":\"2026-10-23\",\"hour\":["
        "{\"time_epoch\":1792789200},{\"time_epoch\":1792800000}]}]}}");
    TEST_ASSERT_INT_EQ(lab_cal_weather_day_shift(options, weather), 0);
    json_free(weather);
    weather = test_lab_json("{\"error\":{\"code\":2007}}");
    TEST_ASSERT_INT_EQ(lab_cal_weather_day_shift(options, weather), 0);
    TEST_ASSERT_INT_EQ(lab_cal_weather_day_shift(options, NULL), 0);
    json_free(weather); json_free(options);
}

/* Known highland and coastal cases distinguish sea-level input from the pressure used by refraction. */
static void check_weather_observer_pressure(void)
{
    json_t *options = test_lab_json(
        "{\"date\":\"2000-01-01\",\"time\":\"12:30:00\",\"lat\":\"-26.2023\",\"elevation\":\"1767\","
        "\"lon\":\"28.0436\",\"zone\":\"2\",\"visibility\":\"all\",\"defer_events\":\"true\"}");
    json_t *weather = test_lab_json(
        "{\"current\":{\"last_updated_epoch\":946729800,\"temp_c\":30.4,\"pressure_mb\":850},"
        "\"forecast\":{\"forecastday\":[{\"hour\":["
        "{\"time_epoch\":946728000,\"temp_c\":30,\"pressure_mb\":1012},"
        "{\"time_epoch\":946731600,\"temp_c\":30.8,\"pressure_mb\":1014}]}]}}");
    double pressure = 0;
    lab_cal_observing_conditions(options, weather);
    TEST_ASSERT_TRUE(lab_cal_number(lab_cal_text(options, "pressure_hpa"), &pressure) &&
                     pressure > 818.0 && pressure < 818.1 &&
                     !strcmp(lab_cal_text(options, "temperature_c"), "30.400000"),
                     "Johannesburg uses interpolated sea-level pressure and current temperature at 1767 metres");
    double refraction = almanac_refraction_degrees(5, 30.4, pressure);
    double sea_level_refraction = almanac_refraction_degrees(5, 30.4, 1013);
    TEST_ASSERT_TRUE(refraction > 0 && refraction < sea_level_refraction * 0.82 &&
                     refraction > sea_level_refraction * 0.80,
                     "observer pressure reduces Johannesburg refraction by about nineteen per cent");
    json_t *fields = test_lab_json("{\"snapshot\":\"MOON|Moon|moon|0|0|0|0|0|1|0.5|0|0|0|0|YES|0|0|0|10|20|0.5\"}");
    unsigned status = 200;
    json_t *response = lab_cal_almanac(fields, options, &status);
    const char *pressures = "818 hPa (observer)\n1013 hPa (sea level)";
    const json_t *cell = json_array_get(lab_cal_get(response, "observer_summary"), 7);
    TEST_ASSERT_TRUE(status == 200 && !strcmp(lab_cal_text(cell, "value"), pressures),
                     "summary distinguishes estimated observer pressure from sea-level pressure");
    const char *variants[] = {"all", "visible"};
    for (unsigned i = 0; i < sizeof variants / sizeof *variants; ++i) {
        const json_t *rendered = lab_cal_get(lab_cal_get(response, "almanac_presentation"), variants[i]);
        TEST_ASSERT_TRUE(strstr(lab_cal_text(rendered, "html"), pressures) &&
                         strstr(lab_cal_text(rendered, "copy_text"), pressures),
                         "both body-list variants and copied worksheet retain both pressure references");
    }
    json_free(response);
    json_free(fields);
    lab_cal_observing_conditions(options, weather);
    TEST_ASSERT_TRUE(lab_cal_number(lab_cal_text(options, "pressure_hpa"), &pressure) &&
                     pressure > 818.0 && pressure < 818.1,
                     "repeated evaluations do not apply elevation twice");
    lab_cal_set(options, "elevation", "0");
    lab_cal_observing_conditions(options, weather);
    TEST_ASSERT_TRUE(!strcmp(lab_cal_text(options, "pressure_hpa"), "1013.000000"),
                     "sea-level observer retains interpolated hourly pressure, not nearby station pressure");
    lab_cal_set(options, "elevation", "-400");
    lab_cal_observing_conditions(options, weather);
    TEST_ASSERT_TRUE(lab_cal_number(lab_cal_text(options, "pressure_hpa"), &pressure) && pressure > 1013,
                     "below-sea-level observer pressure increases");
    lab_cal_set(options, "elevation", "1767");
    lab_cal_set(options, "time", "15:00:00");
    lab_cal_observing_conditions(options, weather);
    TEST_ASSERT_TRUE(lab_cal_number(lab_cal_text(options, "pressure_hpa"), &pressure) &&
                     pressure > 818 && pressure < 819 &&
                     !*lab_cal_text(options, "pressure_msl_hpa") &&
                     strstr(lab_cal_text(options, "weather_source"), "standard atmosphere"),
                     "out-of-hour pressure falls back to the standard atmosphere at observer elevation");
    json_free(weather);
    json_free(options);
}

static void test_lab_observing_conditions(void)
{
    check_weather_local_day();
    check_weather_observer_pressure();
    json_t *options = test_lab_json(
        "{\"date\":\"2000-01-01\",\"time\":\"12:30:00\",\"lat\":\"50\",\"elevation\":\"2000\"}");
    json_t *weather = test_lab_json(
        "{\"forecast\":{\"forecastday\":[{\"hour\":["
        "{\"time_epoch\":946724400,\"temp_c\":40,\"pressure_mb\":1000},"
        "{\"time_epoch\":946728000,\"temp_c\":3,\"pressure_mb\":1000}]}]}}");
    lab_cal_observing_conditions(options, weather);
    double temperature = NAN, pressure = NAN;
    bool ok = lab_cal_number(lab_cal_text(options, "temperature_c"), &temperature) && temperature == 3 &&
              lab_cal_number(lab_cal_text(options, "pressure_hpa"), &pressure) &&
              pressure > 784.5 && pressure < 784.6 &&
              !strcmp(lab_cal_text(options, "atmosphere_note"),
                      "Temperature: 3.0 °C · Pressure: 785 hPa (observer)\n1000 hPa (sea level) · "
                      "Source: WeatherAPI hourly value\n"
                      "estimated pressure at observer altitude");
    TEST_ASSERT_TRUE(ok, "UTC hourly sea-level pressure is converted before display and refraction");
    json_free(weather);
    weather = test_lab_json("{\"forecast\":{\"forecastday\":[{\"hour\":["
                            "{\"time_epoch\":946728000,\"temp_c\":3,\"pressure_mb\":-1}]}]}}");
    lab_cal_observing_conditions(options, weather);
    ok = lab_cal_number(lab_cal_text(options, "temperature_c"), &temperature) && temperature == 3 &&
         lab_cal_number(lab_cal_text(options, "pressure_hpa"), &pressure) && pressure > 790 && pressure < 800 && ok;
    TEST_ASSERT_TRUE(ok, "missing pressure alone uses the elevation fallback");
    lab_cal_set(options, "time", "14:00:00");
    lab_cal_observing_conditions(options, weather);
    ok = strstr(lab_cal_text(options, "atmosphere_note"), "Rough seasonal assumption") && ok;
    TEST_ASSERT_TRUE(ok, "weather for a different hour is not reused");
    json_free(weather);
    weather = test_lab_json("{\"forecast\":{\"forecastday\":[{\"hour\":["
                            "{\"time_epoch\":946728000,\"temp_c\":999,\"pressure_mb\":1000}]}]}}");
    lab_cal_set(options, "time", "12:30:00");
    lab_cal_observing_conditions(options, weather);
    ok = lab_cal_number(lab_cal_text(options, "pressure_hpa"), &pressure) && pressure > 700 && pressure < 800 &&
         strstr(lab_cal_text(options, "weather_source"), "estimated pressure at observer altitude") &&
         strstr(lab_cal_text(options, "atmosphere_note"), "Rough seasonal assumption") && ok;
    TEST_ASSERT_TRUE(ok, "invalid temperature does not prevent the lapse-rate conversion of valid sea-level pressure");
    json_free(weather);
    weather = test_lab_json("{\"forecast\":{\"forecastday\":[{\"hour\":["
                            "{\"time_epoch\":946728000,\"temp_c\":10,\"pressure_mb\":1000}]},"
                            "{\"hour\":[{\"time_epoch\":946731600,\"temp_c\":20}]}]}}");
    lab_cal_observing_conditions(options, weather);
    TEST_ASSERT_TRUE(!strcmp(lab_cal_text(options, "temperature_c"), "15.000000") &&
                     strstr(lab_cal_text(options, "weather_source"), "hourly interpolation"),
                     "adjacent hourly epochs interpolate the selected half hour across provider day arrays");
    lab_cal_set(options, "time", "12:15:00");
    lab_cal_set(options, "zone", "10");
    lab_cal_observing_conditions(options, weather);
    TEST_ASSERT_TRUE(!strcmp(lab_cal_text(options, "temperature_c"), "12.500000"),
                     "selected GMT minutes matter and display zone does not shift weather epochs");
    lab_cal_set(options, "time", "15:00:00");
    lab_cal_observing_conditions(options, weather);
    TEST_ASSERT_TRUE(strstr(lab_cal_text(options, "weather_source"), "Rough seasonal assumption"),
                     "hourly temperatures are not extrapolated beyond available coverage");
    json_free(weather);
    lab_cal_set(options, "elevation", "0");
    weather = test_lab_json("{\"forecast\":{\"forecastday\":[{\"hour\":["
                            "{\"time_epoch\":946767600,\"temp_c\":10}]},{\"hour\":["
                            "{\"time_epoch\":946771200,\"temp_c\":14}]}]}}");
    lab_cal_set(options, "time", "23:30:00");
    lab_cal_observing_conditions(options, weather);
    TEST_ASSERT_TRUE(!strcmp(lab_cal_text(options, "temperature_c"), "12.000000"),
                     "temperature interpolates across actual UTC midnight");
    json_free(weather);
    weather = test_lab_json("{\"forecast\":{\"forecastday\":[{\"day\":{\"mintemp_c\":0,\"maxtemp_c\":30},"
                            "\"hour\":[{\"time_epoch\":946764000,\"temp_c\":10},"
                            "{\"time_epoch\":946771200,\"temp_c\":14}]}]}}");
    lab_cal_observing_conditions(options, weather);
    TEST_ASSERT_TRUE(strstr(lab_cal_text(options, "weather_source"), "Rough seasonal assumption"),
                     "missing hours and daily extrema do not invent an hourly temperature curve");
    json_free(weather);
    lab_cal_observing_conditions(options, NULL);
    double winter = 0, summer = 0, south_summer = 0;
    ok = lab_cal_number(lab_cal_text(options, "temperature_c"), &winter) && ok;
    lab_cal_set(options, "date", "2000-07-01");
    lab_cal_observing_conditions(options, NULL);
    ok = lab_cal_number(lab_cal_text(options, "temperature_c"), &summer) && summer > winter && ok;
    lab_cal_set(options, "date", "2000-01-01");
    lab_cal_set(options, "lat", "-50");
    /* This date is outside provider coverage, so this also checks the no-network fallback path. */
    lab_cal_atmosphere(options);
    ok = lab_cal_number(lab_cal_text(options, "temperature_c"), &south_summer) && south_summer > winter && ok;
    json_free(options);
    TEST_ASSERT_TRUE(ok, "hourly conditions match UTC; pressure is not corrected twice; fallbacks are independent");
}

static bool weather_cache_fixture(const char *directory)
{
    time_t now = time(NULL);
    struct tm utc = {0};
    gmtime_r(&now, &utc);
    char date[16], clock[16];
    strftime(date, sizeof date, "%Y-%m-%d", &utc);
    strftime(clock, sizeof clock, "%H:%M:%S", &utc);
    json_t *options = test_lab_json("{\"lat\":\"52.7077\",\"lon\":\"-2.7541\",\"elevation\":\"50\"}");
    lab_cal_set(options, "date", date);
    lab_cal_set(options, "time", clock);
    /* Preserve an observation from the former files before accepting any new weather. */
    string_t *location = string_new_with("52.708,-2.754");
    string_t *legacy_directory = string_sprintf("%s/lab/weather-cache", directory);
    string_t *legacy_path = string_sprintf("%S/%02lu.json", legacy_directory, string_hash(location) % 64);
    file_t *legacy_dir = file_new(legacy_directory), *legacy_file = file_new(legacy_path);
    string_t *legacy = string_sprintf("{\"location\":\"52.708,-2.754\",\"retrieved\":\"%lld\","
                                      "\"observed\":\"%lld\",\"temp_c\":\"7.2\"}", (long long)now, (long long)now);
    bool migrated = file_create_directory(legacy_dir, 0700, true) && file_write_all_text(legacy_file, legacy);
    string_t *blocked_path = lab_state_private_path("weather_cache.db");
    sqlite_t *blocked = sqlite_open_plain(blocked_path);
    migrated = migrated && blocked && sqlite_exec_cstr(blocked,
        "CREATE TABLE weather_cache(slot INTEGER PRIMARY KEY,record TEXT NOT NULL)");
    sqlite_stmt_t *legacy_insert = blocked ? sqlite_stmt_prepare(blocked,
        "INSERT INTO weather_cache VALUES(7,?1)") : NULL;
    migrated = migrated && legacy_insert && sqlite_stmt_bind_text(legacy_insert, 1, string_c_str(legacy)) &&
               sqlite_stmt_step(legacy_insert) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(legacy_insert);
    migrated = migrated && blocked && sqlite_exec_cstr(blocked, "BEGIN IMMEDIATE");
    json_t *unavailable = lab_cal_weather_recent(options, now);
    migrated = migrated && !unavailable && file_exists(legacy_file);
    json_free(unavailable);
    if (blocked) sqlite_exec_cstr(blocked, "ROLLBACK");
    sqlite_close(blocked);
    string_free(blocked_path);
    json_t *imported = migrated ? lab_cal_weather_recent(options, now) : NULL;
    migrated = migrated && imported && !strcmp(lab_cal_text(imported, "temp_c"), "7.2") &&
               !file_exists(legacy_dir);
    json_free(imported);
    file_free(legacy_file);
    file_free(legacy_dir);
    string_free(legacy);
    string_free(legacy_path);
    string_free(legacy_directory);
    string_free(location);
    string_t *source = string_sprintf("{\"current\":{\"last_updated_epoch\":%lld,\"temp_c\":8.3,\"pressure_mb\":1032}}",
                                      (long long)now);
    json_t *weather = json_from_text(source);
    string_free(source);
    lab_cal_observing_conditions(options, weather);
    bool ok = migrated && !strcmp(lab_cal_text(options, "weather_source"),
                      "WeatherAPI\nestimated pressure (standard atmosphere)") &&
              !strcmp(lab_cal_text(options, "temperature_c"), "8.300000");
    if (!ok) string_fprintf(stderr, "Current observation failed: %s\n", lab_cal_text(options, "weather_source"));
    lab_cal_weather_remember(options, weather, now);
    lab_cal_observing_conditions(options, NULL);
    ok = ok && !strcmp(lab_cal_text(options, "temperature_c"), "8.300000") &&
         strstr(lab_cal_text(options, "weather_source"), "Recent weather estimate (") &&
         strstr(lab_cal_text(options, "weather_source"), "min from selected time)");
    if (!ok) string_fprintf(stderr, "Cached observation failed: %s\n", lab_cal_text(options, "weather_source"));
    json_t *recent = lab_cal_weather_recent(options, now + 3599);
    ok = ok && recent && !strcmp(lab_cal_text(recent, "temp_c"), "8.3");
    if (!ok) string_fprintf(stderr, "Recent cache lookup failed\n");
    json_free(recent);
    string_t *cache_path = lab_state_private_path("weather_cache.db");
    sqlite_t *cache_db = sqlite_open_plain(cache_path);
    ok = ok && cache_db && sqlite_exec_cstr(cache_db, "BEGIN IMMEDIATE");
    /* Another worker holding the cache must cause an immediate miss, not a blocked evaluation. */
    recent = lab_cal_weather_recent(options, now);
    ok = ok && !recent;
    json_free(recent);
    if (cache_db) sqlite_exec_cstr(cache_db, "ROLLBACK");
    sqlite_stmt_t *limit = cache_db ? sqlite_stmt_prepare(cache_db, "SELECT count(*) FROM weather_records") : NULL;
    ok = ok && limit && sqlite_stmt_step(limit) == SQLITE_STEP_ROW && sqlite_stmt_column_int(limit, 0) == 1;
    sqlite_stmt_finalize(limit);
    ok = ok && !sqlite_stmt_prepare(cache_db, "SELECT record FROM weather_cache") &&
         !sqlite_exec_cstr(cache_db, "UPDATE weather_records SET latitude=100") &&
         !sqlite_exec_cstr(cache_db, "UPDATE weather_records SET source=zeroblob(257)");
    limit = sqlite_stmt_prepare(cache_db,
        "SELECT typeof(latitude),typeof(temperature_c),valid_utc FROM weather_readable");
    ok = ok && limit && sqlite_stmt_step(limit) == SQLITE_STEP_ROW &&
         !strcmp(sqlite_stmt_column_text(limit, 0), "real") &&
         !strcmp(sqlite_stmt_column_text(limit, 1), "real") && strlen(sqlite_stmt_column_text(limit, 2)) == 19;
    sqlite_stmt_finalize(limit);
    sqlite_close(cache_db);
    string_free(cache_path);
    recent = lab_cal_weather_recent(options, now + 3601);
    ok = ok && !recent;
    json_free(recent);
    recent = lab_cal_weather_recent(options, now - 1);
    ok = ok && !recent;
    json_free(recent);
    time_t later = now + 7200;
    struct tm later_utc = {0};
    char later_date[16], later_clock[16];
    gmtime_r(&later, &later_utc);
    strftime(later_date, sizeof later_date, "%Y-%m-%d", &later_utc);
    strftime(later_clock, sizeof later_clock, "%H:%M:%S", &later_utc);
    lab_cal_set(options, "date", later_date);
    lab_cal_set(options, "time", later_clock);
    recent = lab_cal_weather_recent(options, now);
    ok = ok && !recent;
    json_free(recent);
    lab_cal_set(options, "date", date);
    lab_cal_set(options, "time", clock);
    lab_cal_set(options, "lon", "-1.7541");
    recent = lab_cal_weather_recent(options, now);
    double distance = 0;
    ok = ok && recent && lab_cal_number(lab_cal_text(recent, "distance_km"), &distance) && distance > 60 && distance < 80;
    json_free(recent);
    lab_cal_set(options, "lat", "20");
    recent = lab_cal_weather_recent(options, now);
    ok = ok && !recent;
    json_free(recent);
    lab_cal_set(options, "lat", "52.7077");
    lab_cal_set(options, "lon", "-2.7541");
    lab_cal_set(options, "date", "2000-01-01");
    recent = lab_cal_weather_recent(options, now);
    ok = ok && !recent;
    json_free(recent);
    lab_cal_set(options, "date", date);
    json_t *invalid = test_lab_json("{\"current\":{\"last_updated_epoch\":0,\"temp_c\":999}}");
    lab_cal_weather_remember(options, invalid, now);
    json_free(invalid);
    recent = lab_cal_weather_recent(options, now);
    ok = ok && recent && !strcmp(lab_cal_text(recent, "temp_c"), "8.3");
    json_free(recent);
    /* The closest fresh observation wins, rather than always using the latest observation. */
    source = string_sprintf("{\"current\":{\"last_updated_epoch\":%lld,\"temp_c\":12.4}}", (long long)now - 1800);
    json_t *earlier = json_from_text(source);
    string_free(source);
    lab_cal_weather_remember(options, earlier, now);
    time_t selected = now - 1700;
    gmtime_r(&selected, &utc);
    strftime(date, sizeof date, "%Y-%m-%d", &utc);
    strftime(clock, sizeof clock, "%H:%M:%S", &utc);
    lab_cal_set(options, "date", date);
    lab_cal_set(options, "time", clock);
    recent = lab_cal_weather_recent(options, now);
    ok = ok && recent && !strcmp(lab_cal_text(recent, "temp_c"), "12.4");
    json_free(recent);
    /* More than 64 locations coexist, including keys that previously collided in the same slot. */
    for (unsigned i = 0; i < 70; ++i) {
        string_t *longitude = string_sprintf("%u", i);
        lab_cal_set(options, "lon", string_c_str(longitude));
        string_free(longitude);
        lab_cal_weather_remember(options, earlier, now);
    }
    lab_cal_set(options, "lon", "-2.7541");
    recent = lab_cal_weather_recent(options, now);
    ok = ok && recent && !strcmp(lab_cal_text(recent, "temp_c"), "12.4");
    json_free(recent);
    json_free(earlier);
    cache_path = lab_state_private_path("weather_cache.db");
    cache_db = sqlite_open_plain(cache_path);
    ok = ok && sqlite_exec_cstr(cache_db, "DELETE FROM weather_records");
    sqlite_close(cache_db);
    string_free(cache_path);
    lab_cal_set(options, "lat", "48.8566");
    lab_cal_set(options, "lon", "2.3522");
    lab_cal_weather_remember(options, weather, now);
    lab_cal_set(options, "lat", "51.5074");
    lab_cal_set(options, "lon", "-0.1278");
    recent = lab_cal_weather_recent(options, now);
    ok = ok && recent && lab_cal_number(lab_cal_text(recent, "distance_km"), &distance) &&
         distance > 340 && distance < 350;
    json_free(recent);
    lab_cal_weather_remember(options, weather, now);
    recent = lab_cal_weather_recent(options, now);
    ok = ok && recent && lab_cal_number(lab_cal_text(recent, "distance_km"), &distance) && distance < 0.1;
    json_free(recent);
    lab_cal_set(options, "lat", "0");
    lab_cal_set(options, "lon", "179.5");
    lab_cal_weather_remember(options, weather, now);
    lab_cal_set(options, "lon", "-179.5");
    recent = lab_cal_weather_recent(options, now);
    ok = ok && recent && lab_cal_number(lab_cal_text(recent, "distance_km"), &distance) &&
         distance > 110 && distance < 112;
    json_free(recent);
    lab_cal_observing_conditions(options, NULL);
    ok = ok && strstr(lab_cal_text(options, "weather_source"), "from 111 km away");
    lab_cal_set(options, "lat", "-45");
    for (unsigned i = 0; i < 520; ++i) {
        string_t *longitude = string_sprintf("%.1f", -130 + i * 0.5);
        lab_cal_set(options, "lon", string_c_str(longitude));
        string_free(longitude);
        lab_cal_weather_remember(options, weather, now);
    }
    cache_path = lab_state_private_path("weather_cache.db");
    cache_db = sqlite_open_plain(cache_path);
    limit = sqlite_stmt_prepare(cache_db, "SELECT count(*) FROM weather_records");
    ok = ok && limit && sqlite_stmt_step(limit) == SQLITE_STEP_ROW && sqlite_stmt_column_int(limit, 0) == 512;
    sqlite_stmt_finalize(limit);
    limit = sqlite_stmt_prepare(cache_db, "PRAGMA page_count");
    ok = ok && limit && sqlite_stmt_step(limit) == SQLITE_STEP_ROW && sqlite_stmt_column_int(limit, 0) <= 256;
    sqlite_stmt_finalize(limit);
    sqlite_close(cache_db);
    string_free(cache_path);
    recent = lab_cal_weather_recent(options, now);
    ok = ok && recent;
    json_free(recent);
    json_free(weather);
    json_free(options);
    return ok;
}

static void test_lab_weather_recent_estimate(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(weather_cache_fixture),
                     "SQLite migration preserves observations, removes old files and skips contention; estimates expire in one hour");
}

static bool weather_conditions_fixture(const char *directory)
{
    time_t now = time(NULL);
    json_t *options = test_lab_json("{\"lat\":\"58.30183\",\"lon\":\"-134.41972\",\"elevation\":\"18\","
                                    "\"date\":\"2026-10-25\",\"time\":\"10:13:46\",\"temperature_c\":\"-0.4\","
                                    "\"pressure_hpa\":\"1025\",\"pressure_msl_hpa\":\"1027\","
                                    "\"weather_source\":\"WeatherAPI hourly interpolation\","
                                    "\"atmosphere_note\":\"Provider conditions\"}");
    /* A legacy entry must be rejected even when fresh and otherwise matched exactly. */
    string_t *legacy_key = string_sprintf("%.17g|%.17g|%.17g|%.17g", 58.30183, -134.41972, 18.0,
                                          lab_cal_weather_epoch(options));
    string_t *cache_path = string_sprintf("%s/lab/weather-cache", directory);
    file_t *cache = file_new(cache_path);
    bool ok = file_create_directory(cache, 0700, true);
    string_t *legacy_path = string_sprintf("%s/conditions-%02lu.json", string_c_str(cache_path),
                                           string_hash(legacy_key) % 64);
    file_t *legacy_file = file_new(legacy_path);
    lab_cal_set(options, "key", string_c_str(legacy_key));
    string_t *retrieved = string_sprintf("%lld", (long long)now);
    lab_cal_set(options, "retrieved", string_c_str(retrieved));
    string_t *legacy_record = json_to_string(options);
    ok = ok && file_write_all_text(legacy_file, legacy_record) &&
         !lab_cal_weather_conditions_restore(options, now);
    file_free(legacy_file);
    file_free(cache);
    string_free(legacy_record);
    string_free(retrieved);
    string_free(legacy_path);
    string_free(cache_path);
    string_free(legacy_key);
    lab_cal_weather_conditions_remember(options, now);
    lab_cal_observing_conditions(options, NULL);
    ok = ok && lab_cal_weather_conditions_restore(options, now + 899) &&
              !strcmp(lab_cal_text(options, "temperature_c"), "-0.4") &&
              !strcmp(lab_cal_text(options, "pressure_hpa"), "1025") &&
              !strcmp(lab_cal_text(options, "pressure_msl_hpa"), "1027") &&
              !strcmp(lab_cal_text(options, "weather_source"), "WeatherAPI hourly interpolation");
    ok = ok && !lab_cal_weather_conditions_restore(options, now + 900) &&
         !lab_cal_weather_conditions_restore(options, now - 1);
    static const char *const keys[] = {"lat", "lon", "elevation", "date", "time"};
    static const char *const changed[] = {"58.30184", "-134.41973", "19", "2026-10-24", "10:13:47"};
    for (unsigned i = 0; i < sizeof keys / sizeof *keys; ++i) {
        string_t *saved = string_new_with(lab_cal_text(options, keys[i]));
        lab_cal_set(options, keys[i], changed[i]);
        ok = ok && !lab_cal_weather_conditions_restore(options, now);
        lab_cal_set(options, keys[i], string_c_str(saved));
        string_free(saved);
    }
    lab_cal_set(options, "weather_source", "Rough seasonal assumption");
    lab_cal_set(options, "temperature_c", "7");
    lab_cal_weather_conditions_remember(options, now + 100);
    ok = ok && lab_cal_weather_conditions_restore(options, now + 101) &&
         !strcmp(lab_cal_text(options, "temperature_c"), "-0.4") &&
         !lab_cal_weather_conditions_restore(options, now + 900);
    json_free(options);
    return ok;
}

static void test_lab_weather_conditions_cache(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(weather_conditions_fixture),
                     "exact provider conditions survive history revisits and failed refreshes, but expire after 15 minutes");
}

static void test_lab_integrator_deadline_policy(void)
{
    static const struct {
        unsigned precision;
        unsigned cap;
        unsigned milliseconds;
    } cases[] = {
        {0u, 0u, 30000u},
        {17u, 500u, 31000u},
        {17u, 5000u, 40000u},
        {96u, 5000u, 40000u},
        {97u, 5000u, 50000u},
        {96u, 20000u, 70000u},
        {384u, 5000u, 70000u},
        {385u, 5000u, 80000u},
        {386u, 500u, 71000u},
        {386u, 5000u, 80000u},
        {386u, 20000u, 110000u},
        {386u, 25000u, 120000u},
        {386u, 50000u, 120000u},
        {96u, 100000u, 120000u},
        {78914u, 500u, 120000u},
        {78914u, 5000u, 120000u},
        {78914u, 20000u, 120000u},
        {78914u, 100000u, 120000u},
        {UINT_MAX, 0u, 120000u},
        {0u, UINT_MAX, 120000u},
        {UINT_MAX, UINT_MAX, 120000u},
    };
    for (size_t i = 0u; i < sizeof(cases) / sizeof(*cases); ++i) {
        unsigned actual = lab_eval_integrator_timeout(cases[i].precision, cases[i].cap);
        if (actual != cases[i].milliseconds)
            string_printf("integrator deadline: digits=%u cap=%u expected=%u ms actual=%u ms\n",
                          cases[i].precision, cases[i].cap, cases[i].milliseconds, actual);
        TEST_ASSERT_TRUE(actual == cases[i].milliseconds && actual >= 30000u && actual <= 120000u,
                         "deadline preserves base and precision allowances and saturates without overflow");
    }
}

static void test_lab_integrator_high_precision_deadline(void)
{
    unsigned status = 0u;
    json_t *response = evaluate("/integrator-eval",
                                "{\"expression\":\"exp(Li(x))\",\"precision\":386,\"max_intervals\":5000,"
                                "\"bounds\":[{\"name\":\"x\",\"lo\":\"0\",\"hi\":\"1\"}]}", &status);
    const char *prefix = "0.62432998854355087099";
    /* A successful integrator's error field is its numerical estimate, not a failure diagnostic. */
    number_t estimate = num_create_from_text(json_string_value(test_lab_member(response, "error")));
    bool ok = response && status == 200u && test_lab_ok(response, true) &&
              !strcmp(test_lab_text(response, "mode"), "integrator") &&
              !strcmp(test_lab_text(response, "status"), "converged") &&
              !strncmp(test_lab_text(response, "value"), prefix, strlen(prefix)) &&
              *test_lab_text(response, "work_units") && strcmp(test_lab_text(response, "work_units"), "0") &&
              !strcmp(test_lab_text(response, "work_cap"), "5000") &&
              !strcmp(test_lab_text(response, "max_intervals"), "5000") &&
              !test_lab_member(response, "error_code") && !*test_lab_text(response, "raw_error") &&
              num_is_real(estimate) && num_is_finite(estimate) && !num_is_nan(estimate) && num_sign(estimate) >= 0;
    if (!ok)
        string_printf("386-digit exp(Li(x)) route: HTTP=%u status=%s value=%s work=%s/%s error=%s\n",
                      status, test_lab_text(response, "status"), test_lab_text(response, "value"),
                      test_lab_text(response, "work_units"), test_lab_text(response, "work_cap"),
                      test_lab_text(response, "error"));
    num_destroy(&estimate);
    json_free(response);
    TEST_ASSERT_TRUE(ok, "386-digit exp(Li(x)) on [0,1] converges through the native route with a 5000 work cap");
}

static void test_lab_worker_deadline_diagnostic(void)
{
    unsigned status = 0u;
    json_t *response = lab_eval_worker_failure(&status, ETIMEDOUT, 80000u);
    const string_t *deadline = json_number_text(test_lab_member(response, "timeout_ms"));
    bool ok = response && status == 422u && test_lab_ok(response, false) &&
              !strcmp(test_lab_text(response, "error_code"), "ETIMEDOUT") && deadline &&
              !strcmp(string_c_str(deadline), "80000") &&
              !strcmp(test_lab_text(response, "error"), "Native worker timed out after 80000 ms (ETIMEDOUT)");
    json_free(response);
    TEST_ASSERT_TRUE(ok, "saved ETIMEDOUT produces a precise failure code, deadline and message without waiting");

    const int errors[] = {ENOENT, EOVERFLOW};
    for (size_t i = 0u; i < sizeof(errors) / sizeof(*errors); ++i) {
        response = lab_eval_worker_failure(&status, errors[i], 80000u);
        ok = response && status == 422u && test_lab_ok(response, false) &&
             !test_lab_member(response, "error_code") && !test_lab_member(response, "timeout_ms") &&
             !strcmp(test_lab_text(response, "error"),
                     "Native worker could not complete (unavailable or output limit)");
        json_free(response);
        TEST_ASSERT_TRUE(ok, "non-timeout process failures are not misreported as deadline expiry");
    }
    const int stops[] = {ECANCELED, E2BIG, EIO};
    const char *codes[] = {"ECANCELED", "E2BIG", "EIO"};
    for (size_t i = 0; i < sizeof(stops) / sizeof(*stops); ++i) {
        response = lab_eval_worker_failure(&status, stops[i], 80000u);
        ok = response && status == 422u && test_lab_ok(response, false) &&
             !strcmp(test_lab_text(response, "error_code"), codes[i]) && !test_lab_member(response, "timeout_ms");
        json_free(response);
        TEST_ASSERT_TRUE(ok, "control stops retain their precise reason without a false timeout");
    }
}

/* Run real workers sequentially through the native adapters. */
void test_lab_evaluation_cases(void)
{
    TEST_RUN_IN_GROUP(test_lab_integrator_deadline_policy, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_worker_deadline_diagnostic, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_integrator_high_precision_deadline, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_math_modes, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_expression_contract, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_function_input, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_evaluation_validation, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_calendar_validation, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_observing_conditions, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_weather_recent_estimate, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_weather_conditions_cache, tests, NULL);
}
