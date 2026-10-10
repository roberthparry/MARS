/**
 * @file test_lab_forms.c
 * @brief Native Lab form parsing, identity and malformed-input regressions.
 *
 * Exercises the synchronous forms adapter without a server or browser. Checks
 * preserve authored symbolic bounds and parameter spelling, legacy empty town
 * fields, partial clock entry and bounded rejection contracts.
 */
#include <string.h>

#include "lab_forms.h"
#include "test_harness.h"
#include "test_lab_support.h"

static json_t *lab_forms_test_request(const char *literal, unsigned *status)
{
    json_t *payload = test_lab_json(literal);
    json_t *result = lab_forms_request(payload, status);
    json_free(payload);
    return result;
}

static bool lab_forms_test_flag(const json_t *object, const char *name, bool expected)
{
    bool value = !expected;
    return json_bool_value(test_lab_member(object, name), &value) && value == expected;
}

static void test_lab_forms_integrator_text(void)
{
    unsigned status = 0;
    json_t *result = lab_forms_test_request(
        "{\"action\":\"integrator\",\"text\":\" x = f(a,b) .. a-b\\n\\nfree: [α]\\ny: 1/3 \"}", &status);
    const json_t *rows = test_lab_member(result, "rows");
    bool ok = status == 200 && test_lab_ok(result, true) && json_array_size(rows) == 3 &&
              !strcmp(test_lab_text(json_array_get(rows, 0), "lo"), "f(a,b)") &&
              !strcmp(test_lab_text(json_array_get(rows, 0), "hi"), "a-b") &&
              !strcmp(test_lab_text(json_array_get(rows, 1), "name"), "[α]") &&
              !strcmp(test_lab_text(result, "text"), "x = f(a,b) .. a-b\nfree [α]\ny = 1/3");
    json_free(result);
    TEST_ASSERT_TRUE(ok, "native bounds preserve nested arguments, exact fractions and authored parameter names");
}

static void test_lab_forms_integrator_references(void)
{
    unsigned status = 0;
    json_t *result = lab_forms_test_request(
        "{\"action\":\"integrator\",\"text\":\"free x\\nfree y\\nfree z\","
        "\"expression\":\"{ x+y | z = 2 }\"}", &status);
    const json_t *rows = test_lab_member(result, "rows");
    bool ok = status == 200 && lab_forms_test_flag(result, "references_valid", true) &&
              lab_forms_test_flag(json_array_get(rows, 0), "referenced", true) &&
              lab_forms_test_flag(json_array_get(rows, 1), "referenced", true) &&
              lab_forms_test_flag(json_array_get(rows, 2), "referenced", false);
    json_free(result);
    result = lab_forms_test_request(
        "{\"action\":\"integrator\",\"text\":\"free alpha\\nfree z\",\"expression\":\"α+1\"}", &status);
    rows = test_lab_member(result, "rows");
    ok = ok && status == 200 && lab_forms_test_flag(result, "references_valid", true) &&
         !strcmp(test_lab_text(json_array_get(rows, 0), "name"), "alpha") &&
         lab_forms_test_flag(json_array_get(rows, 0), "referenced", true) &&
         lab_forms_test_flag(json_array_get(rows, 1), "referenced", false);
    json_free(result);
    result = lab_forms_test_request(
        "{\"action\":\"integrator\",\"text\":\"free x\",\"expression\":\"sin(\"}", &status);
    ok = ok && status == 200 && lab_forms_test_flag(result, "references_valid", false) &&
         lab_forms_test_flag(json_array_get(test_lab_member(result, "rows"), 0), "referenced", true);
    json_free(result);
    TEST_ASSERT_TRUE(ok, "native references ignore unused binding declarations and retain rows during incomplete edits");
}

static void test_lab_forms_integrator_structured(void)
{
    unsigned status = 0;
    json_t *result = lab_forms_test_request(
        "{\"action\":\"integrator\",\"rows\":[{\"kind\":\"BOUND\",\"name\":\" x \","
        "\"lo\":\"Blank  for none\",\"hi\":\" 1/3 \"}]}", &status);
    const json_t *row = json_array_get(test_lab_member(result, "rows"), 0);
    bool ok = status == 200 && !strcmp(test_lab_text(row, "lo"), "") &&
              !strcmp(test_lab_text(row, "name"), "x") && !strcmp(test_lab_text(result, "text"), "x = 1/3");
    json_free(result);
    result = lab_forms_test_request("{\"action\":\"integrator\",\"text\":\"\"}", &status);
    ok = ok && status == 200 && !strcmp(test_lab_text(result, "text"), "x = 0 .. 1");
    json_free(result);
    TEST_ASSERT_TRUE(ok, "structured rows normalise legacy placeholders and empty restoration keeps the default bound");
}

static void test_lab_forms_time(void)
{
    unsigned status = 0;
    json_t *result = lab_forms_test_request("{\"action\":\"time\",\"text\":\"12:34:56,789\"}", &status);
    bool ok = status == 200 && !strcmp(test_lab_text(result, "text"), "12:34:56.789");
    json_free(result);
    result = lab_forms_test_request("{\"action\":\"time\",\"text\":\"123\"}", &status);
    ok = ok && status == 200 && !strcmp(test_lab_text(result, "text"), "12:3");
    json_free(result);
    result = lab_forms_test_request("{\"action\":\"time\",\"text\":\"123456.\"}", &status);
    ok = ok && status == 200 && !strcmp(test_lab_text(result, "text"), "12:34:56.");
    json_free(result);
    TEST_ASSERT_TRUE(ok, "clock preparation retains partial typing and decimal entry without validating a finished time");
}

static void test_lab_forms_town(void)
{
    unsigned status = 0;
    json_t *result = lab_forms_test_request("{\"action\":\"town\",\"value\":\"Name||-3|5\"}", &status);
    const json_t *town = test_lab_member(result, "town");
    bool ok = status == 200 && !strcmp(test_lab_text(town, "latitude"), "") &&
              !strcmp(test_lab_text(town, "longitude"), "-3") && !strcmp(test_lab_text(town, "elevation"), "5") &&
              !strcmp(test_lab_text(result, "detail"), "");
    json_free(result);
    result = lab_forms_test_request(
        "{\"action\":\"town\",\"value\":\"Name|51.5|-3|\","
        "\"candidates\":[\"Name|51.5000001|-3|5\",\"Other|51.5|-3|\"]}", &status);
    number_t index;
    ok = ok && status == 200 && json_number_value(test_lab_member(result, "match_index"), &index);
    if (ok) {
        ok = num_to_double(index) == 0 && !strcmp(test_lab_text(result, "detail"), "+51.5000  -003.0000");
        num_destroy(&index);
    }
    json_free(result);
    result = lab_forms_test_request(
        "{\"action\":\"town\",\"value\":\"Name|51.5|-3|7\",\"candidates\":[\"Name|51.5|-3|5\"]}", &status);
    bool has_index = json_number_value(test_lab_member(result, "match_index"), &index);
    ok = ok && status == 200 && has_index && num_to_double(index) == -1;
    if (has_index)
        num_destroy(&index);
    json_free(result);
    TEST_ASSERT_TRUE(ok, "legacy town fields retain positions; matching tolerates coordinates but respects elevation");
}

static void test_lab_forms_rejections(void)
{
    static const char *const invalid[] = {
        "{\"action\":\"unknown\"}",
        "{\"action\":\"time\",\"text\":42}",
        "{\"action\":\"time\",\"text\":\"12\\u000034\"}",
        "{\"action\":\"integrator\",\"rows\":[42]}",
        "{\"action\":\"integrator\",\"text\":\"x = 0 .. 1 .. 2\"}",
        "{\"action\":\"integrator\",\"text\":\"x = f(0 .. 1\"}",
        "{\"action\":\"town\",\"value\":\"\",\"candidates\":[42]}"
    };
    bool ok = true;
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        unsigned status = 0;
        json_t *result = lab_forms_test_request(invalid[i], &status);
        ok = ok && status == 400 && test_lab_ok(result, false);
        json_free(result);
    }
    TEST_ASSERT_TRUE(ok, "malformed syntax, embedded NUL, wrong types and unknown actions are rejected atomically");
}

static void test_lab_forms_limits(void)
{
    json_t *payload = test_lab_json("{\"action\":\"integrator\",\"rows\":[]}");
    json_t *rows = (json_t *)test_lab_member(payload, "rows");
    json_t *row = test_lab_json("{\"name\":\"x\"}");
    bool ok = payload && row;
    for (size_t i = 0; ok && i < 257; ++i)
        ok = json_array_append(rows, row);
    unsigned status = 0;
    json_t *result = lab_forms_request(payload, &status);
    ok = ok && status == 400 && test_lab_ok(result, false);
    json_free(result);
    json_free(row);
    json_free(payload);
    payload = test_lab_json("{\"action\":\"time\"}");
    string_t *key = string_new_with("text"), *text = string_new();
    for (size_t i = 0; ok && i < 65537; ++i)
        ok = string_append_char(text, '1') == 0;
    json_t *value = json_new_string(text);
    ok = ok && json_object_set(payload, key, value);
    result = lab_forms_request(payload, &status);
    ok = ok && status == 400 && test_lab_ok(result, false);
    json_free(result);
    json_free(value);
    json_free(payload);
    string_free(key);
    string_free(text);
    TEST_ASSERT_TRUE(ok, "form row count and aggregate text budgets reject oversized requests");
}

static void test_lab_forms_town_presentation(void)
{
    json_t *town = test_lab_json("{\"name\":\" Rhyl \",\"latitude\":\"53.3190\","
                                 "\"longitude\":\"-3.4916\",\"elevation\":\"5\"}");
    bool ok = lab_forms_town_presentation(town) &&
              !strcmp(test_lab_text(town, "value"), "Rhyl|53.3190|-3.4916|5") &&
              !strcmp(test_lab_text(town, "detail"), "+53.3190  -003.4916") &&
              !strcmp(test_lab_text(town, "name"), " Rhyl ");
    json_free(town);
    town = test_lab_json("{\"name\":\"東京\"}");
    ok = ok && lab_forms_town_presentation(town) && !strcmp(test_lab_text(town, "value"), "東京|||") &&
         !strcmp(test_lab_text(town, "detail"), "");
    json_free(town);
    town = test_lab_json("{\"latitude\":\"invalid\",\"longitude\":\"0\"}");
    ok = ok && lab_forms_town_presentation(town) && !strcmp(test_lab_text(town, "detail"), "");
    json_free(town);
    town = test_lab_json("{\"latitude\":7}");
    ok = ok && !lab_forms_town_presentation(town) && !lab_forms_town_presentation(NULL);
    json_free(town);
    TEST_ASSERT_TRUE(ok, "catalogue keys and coordinates use native formatting without changing source fields");
}

/* Register these ordinary checks before the suite's README examples. */
void test_lab_forms_cases(void)
{
    TEST_RUN_IN_GROUP(test_lab_forms_integrator_text, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_forms_integrator_references, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_forms_integrator_structured, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_forms_time, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_forms_town, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_forms_town_presentation, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_forms_rejections, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_forms_limits, tests, NULL);
}
