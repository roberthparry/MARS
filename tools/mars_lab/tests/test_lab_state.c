/**
 * @file test_lab_state.c
 * @brief Native Lab state merging, corruption preservation and page escaping tests.
 *
 * Uses private state fixtures to check timestamp ordering, per-mode precision
 * merging and private file permissions. Page assertions cover HTML and script
 * escaping, literal template-like user input and rejection of malformed assets.
 * The user's state and environment are never changed by these tests.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file.h"
#include "lab_page.h"
#include "lab_state.h"
#include "test_harness.h"
#include "test_lab_support.h"

static bool state_round_trip(const char *directory)
{
    (void)directory;
    json_t *state = lab_state_load();
    bool ok = state && *test_lab_text(state, "expression") &&
              json_type(test_lab_member(state, "precision_bits")) == JSON_OBJECT;
    json_free(state);
    json_t *updates = test_lab_json("{\"expression\":\"  x+7  \",\"expression_updated_at\":200,"
                                    "\"equation\":\"x=7\",\"equation_updated_at\":200,\"lab_mode\":\"matrix\","
                                    "\"precision_bits\":{\"expression\":256},\"unknown_fixture_key\":\"discard\"}");
    ok = updates && lab_state_save(updates) && ok;
    json_free(updates);
    updates = test_lab_json("{\"expression\":\"stale\",\"expression_updated_at\":100,"
                            "\"equation\":\"stale\",\"equation_updated_at\":100,\"lab_mode\":\"invalid-mode\","
                            "\"precision_bits\":{\"matrix\":512}}");
    ok = updates && lab_state_save(updates) && ok;
    json_free(updates);
    state = lab_state_load();
    const json_t *precision = test_lab_member(state, "precision_bits");
    const string_t *expression_bits = json_number_text(test_lab_member(precision, "expression"));
    const string_t *matrix_bits = json_number_text(test_lab_member(precision, "matrix"));
    ok = state && !strcmp(test_lab_text(state, "expression"), "x+7") &&
         !strcmp(test_lab_text(state, "equation"), "x=7") && !strcmp(test_lab_text(state, "lab_mode"), "matrix") &&
         !test_lab_member(state, "unknown_fixture_key") && expression_bits && matrix_bits &&
         string_view_equals_literal(string_view_all(expression_bits), "256") &&
         string_view_equals_literal(string_view_all(matrix_bits), "512") && ok;
    json_free(state);
    file_t *file = file_new_cstr(getenv("MARS_LAB_STATE_FILE"));
    file_info_t *info = file ? file_get_info(file) : NULL;
    ok = info && (file_info_permissions(info) & 0777) == 0600 && ok;
    file_info_free(info);
    file_free(file);
    return ok;
}

static bool corrupt_state(const char *directory)
{
    (void)directory;
    const char damaged[] = "{broken state";
    file_t *file = file_new_cstr(getenv("MARS_LAB_STATE_FILE"));
    bool written = file && file_write_all_bytes(file, damaged, sizeof(damaged) - 1);
    int write_error = errno;
    json_t *state = lab_state_load();
    bool rejected_load = !state;
    json_free(state);
    json_t *update = test_lab_json("{\"expression\":\"2\"}");
    bool rejected_save = update && !lab_state_save(update);
    json_free(update);
    string_t *preserved = file ? file_read_all_text(file) : NULL;
    bool unchanged = preserved && string_view_equals_literal(string_view_all(preserved), damaged);
    bool ok = written && rejected_load && rejected_save && unchanged;
    if (!ok)
        string_fprintf(stderr,
                       "Lab corrupt-state fixture: written=%d, write errno=%d, load rejected=%d, "
                       "save rejected=%d, bytes preserved=%d\n",
                       written, write_error, rejected_load, rejected_save, unchanged);
    string_free(preserved);
    file_free(file);
    return ok;
}

static bool page_escaping(const char *directory)
{
    json_t *state = test_lab_json("{\"expression\":\"</textarea><script>alert(1)</script>&\\\" __LAB_NAME__\","
                                  "\"control_token\":\"</script>&\\u2028\\u2029__LAB_NAME__\"}");
    bool ok = state && !setenv("MARS_LAB_APP_NAME", "<Lab & \"test\">", 1);
    string_t *page = ok ? lab_page_render(state) : NULL;
    ok = page && string_find(page, "&lt;/textarea&gt;&lt;script&gt;alert(1)&lt;/script&gt;&amp;&quot;") >= 0 &&
         string_find(page, "&lt;Lab &amp; &quot;test&quot;&gt;") >= 0 &&
         string_find(page, "</textarea><script>alert(1)</script>") < 0 &&
         string_find(page, "\\u003c/script\\u003e\\u0026\\u2028\\u2029__LAB_NAME__") >= 0 &&
         string_find(page, "__LAB_NAME__") >= 0 && string_find(page, "MARS_LAB_DATA") < 0;
    string_free(page);
    json_free(state);
    string_t *path = string_sprintf("%s/bad.html", directory);
    file_t *file = path ? file_new(path) : NULL;
    static const char invalid[] = "<html>missing asset catalogue</html>";
    bool prepared = file && file_write_all_bytes(file, invalid, sizeof(invalid) - 1) &&
                    !setenv("MARS_LAB_ASSET_FILE", string_c_str(path), 1);
    page = prepared ? lab_page_render(NULL) : NULL;
    json_t *defaults = prepared ? lab_page_defaults() : NULL;
    ok = prepared && !page && !defaults && ok;
    string_free(page);
    json_free(defaults);
    file_free(file);
    string_free(path);
    return ok;
}

static void test_lab_state_round_trip(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(state_round_trip), "state merges timestamps and precision into private files");
}

static void test_lab_state_corruption(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(corrupt_state), "corrupt saved state is rejected and preserved verbatim");
}

static void test_lab_page_escaping(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(page_escaping), "HTML and script substitutions are escaped exactly once");
}

static bool packaged_defaults(const char *directory)
{
    (void)directory;
    json_t *catalogue = lab_page_catalogue();
    json_t *defaults = lab_page_defaults();
    string_t *expected = json_to_string(test_lab_member(catalogue, "defaults"));
    string_t *actual = defaults ? json_to_string(defaults) : NULL;
    bool ok = expected && actual && !string_compare(expected, actual);
    string_free(actual);
    string_free(expected);
    json_free(defaults);
    json_free(catalogue);
    return ok;
}

static void test_lab_packaged_defaults(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(packaged_defaults), "explicit packaged dates, locations and worksheets survive");
}

/* Keep the native state and page assertions together in the suite registration. */
void test_lab_state_cases(void)
{
    TEST_RUN_IN_GROUP(test_lab_state_round_trip, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_state_corruption, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_page_escaping, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_packaged_defaults, tests, NULL);
}
