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
#include <string.h>

#include "internal/lab_evaluate_internal.h"
#include "lab_calendar.h"
#include "lab_evaluate.h"
#include "number.h"
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
    const char *routes[] = {"/datetime-eval", "/datetime-weather", "/datetime-jurisdiction-location", "/almanac-eval",
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
        {315653u, 500u, 120000u},
        {315653u, 5000u, 120000u},
        {315653u, 20000u, 120000u},
        {315653u, 100000u, 120000u},
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

    const int errors[] = {ENOENT, EOVERFLOW, ECANCELED};
    for (size_t i = 0u; i < sizeof(errors) / sizeof(*errors); ++i) {
        response = lab_eval_worker_failure(&status, errors[i], 80000u);
        ok = response && status == 422u && test_lab_ok(response, false) &&
             !test_lab_member(response, "error_code") && !test_lab_member(response, "timeout_ms") &&
             !strcmp(test_lab_text(response, "error"),
                     "Native worker could not complete (unavailable or output limit)");
        json_free(response);
        TEST_ASSERT_TRUE(ok, "non-timeout process failures are not misreported as deadline expiry");
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
}
