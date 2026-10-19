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

#define MARS_SHARED_LAB_EVALUATE_INTERNAL_ACCESS
#include "internal/lab_evaluate_internal.h"
#include "lab_calendar.h"
#include "lab_evaluate.h"
#include "number-t.h"
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

static void test_lab_observing_conditions(void)
{
    json_t *options = test_lab_json(
        "{\"date\":\"2000-01-01\",\"time\":\"12:30:00\",\"lat\":\"50\",\"elevation\":\"2000\"}");
    json_t *weather = test_lab_json(
        "{\"forecast\":{\"forecastday\":[{\"hour\":["
        "{\"time_epoch\":946724400,\"temp_c\":40,\"pressure_mb\":1000},"
        "{\"time_epoch\":946728000,\"temp_c\":3,\"pressure_mb\":790}]}]}}");
    lab_cal_observing_conditions(options, weather);
    double temperature = NAN, pressure = NAN;
    bool ok = lab_cal_number(lab_cal_text(options, "temperature_c"), &temperature) && temperature == 3 &&
              lab_cal_number(lab_cal_text(options, "pressure_hpa"), &pressure) && pressure == 790 &&
              !strcmp(lab_cal_text(options, "atmosphere_note"),
                      "Temperature: 3.0 °C · Pressure: 790 hPa · Source: WeatherAPI");
    TEST_ASSERT_TRUE(ok, "UTC observation hour supplies both temperature and unmodified station pressure");
    json_free(weather);
    weather = test_lab_json("{\"forecast\":{\"forecastday\":[{\"hour\":["
                            "{\"time_epoch\":946728000,\"temp_c\":3,\"pressure_mb\":-1}]}]}}");
    lab_cal_observing_conditions(options, weather);
    ok = lab_cal_number(lab_cal_text(options, "temperature_c"), &temperature) && temperature == 3 &&
         lab_cal_number(lab_cal_text(options, "pressure_hpa"), &pressure) && pressure > 790 && pressure < 800 && ok;
    TEST_ASSERT_TRUE(ok, "missing pressure alone uses the elevation fallback");
    lab_cal_set(options, "time", "14:00:00");
    lab_cal_observing_conditions(options, weather);
    ok = strstr(lab_cal_text(options, "atmosphere_note"), "seasonal estimate") && ok;
    TEST_ASSERT_TRUE(ok, "weather for a different hour is not reused");
    json_free(weather);
    weather = test_lab_json("{\"forecast\":{\"forecastday\":[{\"hour\":["
                            "{\"time_epoch\":946728000,\"temp_c\":999,\"pressure_mb\":790}]}]}}");
    lab_cal_set(options, "time", "12:30:00");
    lab_cal_observing_conditions(options, weather);
    ok = lab_cal_number(lab_cal_text(options, "pressure_hpa"), &pressure) && pressure == 790 &&
         strstr(lab_cal_text(options, "atmosphere_note"), "seasonal estimate") && ok;
    TEST_ASSERT_TRUE(ok, "invalid temperature alone retains provider pressure");
    json_free(weather);
    lab_cal_set(options, "elevation", "0");
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
    (void)directory;
    time_t now = time(NULL);
    struct tm utc = {0};
    gmtime_r(&now, &utc);
    char date[16], clock[16];
    strftime(date, sizeof date, "%Y-%m-%d", &utc);
    strftime(clock, sizeof clock, "%H:%M:%S", &utc);
    json_t *options = test_lab_json("{\"lat\":\"52.7077\",\"lon\":\"-2.7541\",\"elevation\":\"50\"}");
    lab_cal_set(options, "date", date);
    lab_cal_set(options, "time", clock);
    string_t *source = string_sprintf("{\"current\":{\"last_updated_epoch\":%lld,\"temp_c\":8.3,\"pressure_mb\":1032}}",
                                      (long long)now);
    json_t *weather = json_from_text(source);
    string_free(source);
    lab_cal_observing_conditions(options, weather);
    bool ok = !strcmp(lab_cal_text(options, "weather_source"), "WeatherAPI") &&
              !strcmp(lab_cal_text(options, "temperature_c"), "8.300000");
    if (!ok) string_fprintf(stderr, "Current observation failed: %s\n", lab_cal_text(options, "weather_source"));
    lab_cal_weather_remember(options, weather, now);
    lab_cal_observing_conditions(options, NULL);
    ok = ok && !strcmp(lab_cal_text(options, "temperature_c"), "8.300000") &&
         !strcmp(lab_cal_text(options, "weather_source"), "Recent weather estimate");
    if (!ok) string_fprintf(stderr, "Cached observation failed: %s\n", lab_cal_text(options, "weather_source"));
    json_t *recent = lab_cal_weather_recent(options, now + 172799);
    ok = ok && recent && !strcmp(lab_cal_text(recent, "temp_c"), "8.3");
    if (!ok) string_fprintf(stderr, "Recent cache lookup failed\n");
    json_free(recent);
    recent = lab_cal_weather_recent(options, now + 172800);
    ok = ok && !recent;
    json_free(recent);
    recent = lab_cal_weather_recent(options, now - 1);
    ok = ok && !recent;
    json_free(recent);
    lab_cal_set(options, "lon", "-1.7541");
    recent = lab_cal_weather_recent(options, now);
    ok = ok && !recent;
    json_free(recent);
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
    json_free(weather);
    json_free(options);
    return ok;
}

static void test_lab_weather_recent_estimate(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(weather_cache_fixture),
                     "current UTC observations and location-matched estimates expire strictly at 48 hours");
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
}
