/**
 * @file test_lab_diagnostics.c
 * @brief Sequential process, request and Protobuf diagnostic regressions.
 *
 * Uses bounded local child fixtures and isolated executable overrides. Checks
 * domain-specific codes, retained lifetimes, unchanged legacy fields and full
 * cause chains without copying command arguments or inputs into diagnostics.
 */
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "lab_process.h"
#define MARS_SHARED_LAB_EVALUATE_INTERNAL_ACCESS
#include "internal/lab_evaluate_internal.h"
#include "test_harness.h"
#include "test_lab_support.h"

static bool diagnostic_matches(const diagnostic_t *error, const char *domain, int code)
{
    return error && !strcmp(diagnostic_domain(error), domain) && diagnostic_code(error) == code;
}

static void test_lab_process_diagnostics(void)
{
    static const struct {
        const char *command;
        unsigned timeout;
        size_t limit;
        int error;
        int status;
    } cases[] = {
        {"printf ok", 1000, 16, 0, 0},
        {"printf private-output; exit 23", 1000, 64, 0, 23},
        {"printf too-long", 1000, 2, EFBIG, -1},
        {"printf '\\377'", 1000, 16, EILSEQ, -1},
        {"sleep 2", 30, 16, ETIMEDOUT, -1},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); ++i) {
        const char *argv[] = {"/bin/sh", "-c", cases[i].command, NULL};
        string_t *output = NULL;
        int status = -1;
        bool ran = lab_proc_run(argv, NULL, cases[i].timeout, cases[i].limit, &output, &status);
        int saved = errno;
        diagnostic_t *error = lab_proc_diagnostic(ran, saved, status, cases[i].timeout);
        bool ok = errno == saved && (cases[i].error ? !ran && saved == cases[i].error : ran && status == cases[i].status);
        string_free(output);
        if (cases[i].error)
            ok = ok && diagnostic_matches(error, "lab.process", cases[i].error) && diagnostic_cause(error) &&
                 diagnostic_matches(diagnostic_cause(error), "lab.worker", status);
        else if (cases[i].status)
            ok = ok && diagnostic_matches(error, "lab.worker", cases[i].status) &&
                 !strstr(diagnostic_message(error), "private-output");
        else
            ok = ok && !error;
        diagnostic_free(error);
        TEST_ASSERT_TRUE(ok, "process metadata survives output destruction without disclosing output or changing errno");
    }
    errno = ERANGE;
    diagnostic_t *error = lab_proc_diagnostic(false, ENOENT, -1, 10);
    bool ok = errno == ERANGE && diagnostic_matches(error, "lab.process", ENOENT) && !diagnostic_cause(error);
    diagnostic_free(error);
    TEST_ASSERT_TRUE(ok, "pre-spawn failures have no invented child status");
}

static bool response_diagnostic(const json_t *response, const char *domain, int code)
{
    const json_t *error = test_lab_member(response, "failure_diagnostic");
    string_t *expected = string_sprintf("%d", code);
    const string_t *actual = json_number_text(test_lab_member(error, "code"));
    bool ok = error && expected && actual && !string_compare(expected, actual) &&
              !strcmp(test_lab_text(error, "domain"), domain);
    string_free(expected);
    return ok;
}

static json_t *diagnostic_request(const char *path, const char *input, unsigned *status)
{
    string_t *route = string_new_with(path);
    json_t *payload = test_lab_json(input);
    json_t *response = lab_eval_request(route, payload, status);
    json_free(payload);
    string_free(route);
    return response;
}

static bool request_diagnostics(const char *directory)
{
    (void)directory;
    unsigned status = 0;
    json_t *response = diagnostic_request("/eval", "{\"expression\":3}", &status);
    bool ok = status == 400 && response_diagnostic(response, "lab.request", 400) && *test_lab_text(response, "error");
    json_free(response);
    ok = setenv("MARS_LAB_BINARY", "/nonexistent-mars-diagnostic-worker", 1) == 0 && ok;
    response = diagnostic_request("/eval", "{\"expression\":\"x\"}", &status);
    ok = status == 422 && response_diagnostic(response, "lab.process", ENOENT) && ok;
    json_free(response);
    ok = setenv("MARS_LAB_BINARY", "/bin/false", 1) == 0 && ok;
    response = diagnostic_request("/eval", "{\"expression\":\"x\"}", &status);
    ok = status == 422 && response_diagnostic(response, "lab.worker", 1) && *test_lab_text(response, "error") && ok;
    json_free(response);
    ok = setenv("MARS_LAB_OPHELIA_BINARY", "/bin/false", 1) == 0 && ok;
    response = diagnostic_request("/function-run", "{\"source\":\"output(1).\"}", &status);
    ok = status == 422 && response_diagnostic(response, "lab.worker", 1) && *test_lab_text(response, "error") && ok;
    json_free(response);
    ok = setenv("MARS_LAB_BINARY", "/bin/true", 1) == 0 && ok;
    response = diagnostic_request("/eval", "{\"expression\":\"x\"}", &status);
    ok = status == 502 && response_diagnostic(response, "lab.request", 502) && ok;
    json_free(response);
    return ok;
}

static void test_lab_request_diagnostics(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(request_diagnostics), "validation, spawning, child exits and empty replies remain distinct");
}

/* Run these fixtures sequentially with the normal native Lab suite. */
void test_lab_diagnostic_cases(void)
{
    TEST_RUN_IN_GROUP(test_lab_process_diagnostics, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_request_diagnostics, tests, NULL);
}
