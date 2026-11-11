/**
 * @file test_lab_numeric.c
 * @brief Database-backed native Lab numerical callable regressions.
 *
 * Exercises expression evaluation through the actual request adapter. Public exact integer operations must
 * reach the Value card without parser splitting, operand rounding or JavaScript reinterpretation.
 */
#include <stdio.h>
#include <string.h>

#include "number-t.h"
#include "lab_evaluate.h"
#include "lab_wire.h"
#define MARS_SHARED_LAB_EVALUATE_INTERNAL_ACCESS
#include "internal/lab_evaluate_internal.h"
#include "test_harness.h"
#include "test_database.h"
#include "test_lab_support.h"

static void numeric_request_row(const void *data)
{
    const test_database_case_t *row = data;
    json_t *payload = json_new_object();
    lab_eval_set(payload, "expression", string_c_str(row->input));
    string_t *digits = string_new_with("78");
    lab_eval_put(payload, "precision", json_new_number(digits));
    string_free(digits);
    string_t *path = string_new_with("/eval");
    unsigned status = 0;
    json_t *result = lab_eval_request(path, payload, &status);
    const char *value = lab_eval_text(result, "value");
    number_t got = num_create_from_string(value);
    number_t want = num_create_from_text(row->expected);
    bool ok = status == 200 && test_lab_ok(result, true) && num_eq(got, want);
    if (!ok)
        printf("%s: HTTP %u; value %s; error %s\n", string_c_str(row->name), status, value,
               lab_eval_text(result, "error"));
    num_destroy(&want);
    num_destroy(&got);
    json_free(result);
    string_free(path);
    json_free(payload);
    TEST_ASSERT_TRUE(ok, "native Lab request exposes the numerical callable and its exact result");
}

static void test_lab_numeric_callables(void)
{
    test_run_database_cases(__FILE__, __LINE__, "mars_lab.numeric_callables", NULL, numeric_request_row);
}

/* Register request cases with the sequential native suite. */
void test_lab_numeric_cases(void)
{
    TEST_RUN_IN_GROUP(test_lab_numeric_callables, tests, "expression,numeric-callables");
}
