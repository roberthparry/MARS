#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "test_harness.h"

TEST_SUITE_CONFIG(TEST_CONFIG_LOCAL);

static int int_validity_equal(const void *actual, const void *expected, void *ctx);
static void int_validity_format(const void *value, char *buf, size_t buf_size, void *ctx);
static bool test_fixture_setup_impl(void);
static bool test_fixture_teardown_impl(void);

static const test_validity_contract_t int_contract =
    TEST_VALIDITY_CONTRACT("int-equality",
                           int_validity_equal,
                           int_validity_format,
                           NULL);

static int fixture_setup_calls = 0;
static int fixture_teardown_calls = 0;
static int fixture_balance = 0;
static int fixture_cookie = 0;
static int output_example_ran = 0;
static int output_example_disabled_ran = 0;
static char last_temp_dir[512];
static char last_temp_file[512];
static char last_stdout_capture_file[512];
static char last_stderr_capture_file[512];
static const char *test_env_name = "MARS_TEST_HARNESS_ENV_CASE";

static bool test_suite_setup_impl(void)
{
    setenv(test_env_name, "outer", 1);
    test_register_validity_checker("int-equality", &int_contract);
    return TEST_REQUIRE_VALIDITY_CHECKER("int-equality");
}

TEST_SUITE_SETUP(test_suite_setup_impl);
TEST_FIXTURE_SETUP(test_fixture_setup_impl);
TEST_FIXTURE_TEARDOWN(test_fixture_teardown_impl);

static void test_subtest_grouping(void);
static void test_sub_sub_function(void);
static void test_sub_sub_sub_function(void);
static void test_sub_sub_sub_sub_function(void);
static void test_fixture_is_active_for_case(void);
static void test_fixture_balance_after_nested_group(void);
static void test_temp_resources_are_available_in_case(void);
static void test_temp_resources_are_cleaned_after_case(void);
static void test_env_override_is_available_in_case(void);
static void test_env_override_is_restored_after_case(void);
static void test_stdout_capture_is_available_in_case(void);
static void test_stdout_capture_file_is_cleaned_after_case(void);
static void test_stderr_capture_is_available_in_case(void);
static void test_stderr_capture_file_is_cleaned_after_case(void);
static void test_output_example_runs(void);
static void test_output_example_disabled(void);
static void test_output_examples_respect_config(void);

static const char *test_local_config_path(void)
{
    return "tests/test_test_config/test_test_config.json";
}

static void seed_local_config_with_stale_entries(void)
{
    FILE *f = fopen(test_local_config_path(), "w");

    if (!f)
        return;

    fputs("{\n"
          "  \"test_top_level_default_true\": {\n"
          "    \"enabled\": true,\n"
          "    \"test_subtest_default_true\": true,\n"
          "    \"stale_nested\": true\n"
          "  },\n"
          "  \"test_parent_group\": {\n"
          "    \"enabled\": true,\n"
          "    \"test_subtest_grouping\": {\n"
          "      \"enabled\": true,\n"
          "      \"test_sub_sub_function\": {\n"
          "        \"enabled\": true,\n"
          "        \"stale_old_leaf\": true\n"
          "      }\n"
          "    },\n"
          "    \"stale_branch\": true\n"
          "  },\n"
          "  \"test_output_example_disabled\": false,\n"
          "  \"obsolete_top_level\": true\n"
          "}\n",
          f);
    fclose(f);
}

static bool file_contains_text(const char *path, const char *needle)
{
    FILE *f;
    long size;
    char *buf;
    size_t nread;
    bool found = false;

    f = fopen(path, "rb");
    if (!f)
        return false;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return false;
    }

    size = ftell(f);
    if (size < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return false;
    }

    buf = malloc((size_t)size + 1u);
    if (!buf) {
        fclose(f);
        return false;
    }

    nread = fread(buf, 1, (size_t)size, f);
    buf[nread] = '\0';
    fclose(f);

    found = strstr(buf, needle) != NULL;
    free(buf);
    return found;
}

static int int_validity_equal(const void *actual, const void *expected, void *ctx)
{
    (void)ctx;
    return *(const int *)actual == *(const int *)expected;
}

static void int_validity_format(const void *value, char *buf, size_t buf_size, void *ctx)
{
    (void)ctx;
    snprintf(buf, buf_size, "%d", *(const int *)value);
}

static bool test_fixture_setup_impl(void)
{
    fixture_setup_calls++;
    fixture_balance++;
    fixture_cookie = 0x51A7;
    return true;
}

static bool test_fixture_teardown_impl(void)
{
    if (fixture_balance <= 0) {
        test_mark_failure(__FILE__, __LINE__, "fixture teardown saw non-positive balance");
        return false;
    }
    if (fixture_cookie != 0x51A7) {
        test_mark_failure(__FILE__, __LINE__, "fixture teardown saw invalid fixture cookie");
        return false;
    }

    fixture_balance--;
    fixture_teardown_calls++;
    if (fixture_balance == 0)
        fixture_cookie = 0;
    return true;
}

/* ------------------------------------------------------------------------- */
/* Test functions                                                            */
/* ------------------------------------------------------------------------- */

static void test_top_level_default_true(void)
{
    TEST_ASSERT_TRUE(TEST_ENABLED(NULL),
                     "top-level tests default to true");
    TEST_ASSERT_TRUE(TEST_CONFIG_HAS_KEY(NULL),
                     "top-level tests are materialized for regeneration");
}

static void test_subtest_default_true(void)
{
    TEST_ASSERT_TRUE(TEST_ENABLED("test_top_level_default_true"),
                     "subtests default to true");
    TEST_ASSERT_TRUE(TEST_CONFIG_HAS_KEY("test_top_level_default_true"),
                     "subtests are materialized under their parent");
}

static void test_repeat_lookup_same_value(void)
{
    bool a = TEST_ENABLED(NULL);
    bool b = TEST_ENABLED(NULL);

    TEST_ASSERT_TRUE(a == b, "repeat lookups return the same value");
    TEST_ASSERT_TRUE(TEST_CONFIG_HAS_KEY(NULL),
                     "top-level repeat lookup is materialized");
}

static void test_fixture_is_active_for_case(void)
{
    TEST_ASSERT_INT_EQ(fixture_cookie, 0x51A7);
    TEST_ASSERT_INT_EQ(fixture_setup_calls, fixture_teardown_calls + fixture_balance);
    TEST_ASSERT_TRUE(fixture_balance >= 1,
                     "fixture setup should have run before each test case");
}

static void test_subtest_grouping(void)
{
    TEST_ASSERT_TRUE(TEST_ENABLED("test_parent_group"),
                     "grouped subtests are enabled");
    TEST_RUN_SUBTEST(test_sub_sub_function, NULL);
}

static void test_sub_sub_function(void)
{
    TEST_ASSERT_TRUE(TEST_ENABLED("test_parent_group.test_subtest_grouping"),
                     "nested subtests inherit the full parent path");
    TEST_RUN_SUBTEST(test_sub_sub_sub_function, NULL);
}

static void test_sub_sub_sub_function(void)
{
    TEST_ASSERT_TRUE(TEST_ENABLED("test_parent_group.test_subtest_grouping.test_sub_sub_function"),
                     "deeply nested subtests stay enabled");
    TEST_RUN_SUBTEST(test_sub_sub_sub_sub_function, NULL);
}

static void test_sub_sub_sub_sub_function(void)
{
    TEST_ASSERT_TRUE(TEST_ENABLED("test_parent_group.test_subtest_grouping.test_sub_sub_function.test_sub_sub_sub_function"),
                     "deepest nested path is accepted");
}

static void test_missing_nested_path_is_materialized(void)
{
    const char *parent = "missing_parent_group";

    TEST_ASSERT_TRUE(TEST_ENABLED(parent),
                     "missing nested paths regenerate as enabled");
    TEST_ASSERT_TRUE(TEST_CONFIG_HAS_KEY(parent),
                     "missing nested paths are materialized");
}

static void test_fixture_balance_after_nested_group(void)
{
    TEST_ASSERT_INT_EQ(fixture_balance, 1);
    TEST_ASSERT_INT_EQ(fixture_setup_calls, fixture_teardown_calls + 1);
    TEST_ASSERT_INT_EQ(fixture_cookie, 0x51A7);
}

static void test_temp_resources_are_available_in_case(void)
{
    const char *dir = test_case_temp_dir();
    const char *path = test_case_temp_path("note.txt");
    FILE *f;

    TEST_ASSERT_NOT_NULL(dir);
    TEST_ASSERT_NOT_NULL(path);

    snprintf(last_temp_dir, sizeof(last_temp_dir), "%s", dir);
    snprintf(last_temp_file, sizeof(last_temp_file), "%s", path);

    f = fopen(path, "w");
    TEST_ASSERT_NOT_NULL(f);
    fputs("fixture-resource", f);
    fclose(f);

    TEST_ASSERT_INT_EQ(access(dir, F_OK), 0);
    TEST_ASSERT_INT_EQ(access(path, F_OK), 0);
}

static void test_temp_resources_are_cleaned_after_case(void)
{
    TEST_ASSERT_TRUE(last_temp_dir[0] != '\0',
                     "previous temp dir path should have been captured");
    TEST_ASSERT_TRUE(last_temp_file[0] != '\0',
                     "previous temp file path should have been captured");
    TEST_ASSERT_TRUE(access(last_temp_file, F_OK) != 0,
                     "temp file should be removed after its case finishes");
    TEST_ASSERT_TRUE(access(last_temp_dir, F_OK) != 0,
                     "temp directory should be removed after its case finishes");
}

static void test_env_override_is_available_in_case(void)
{
    const char *before = getenv(test_env_name);
    const char *after;

    TEST_ASSERT_NOT_NULL(before);
    TEST_ASSERT_STR_EQ(before, "outer");
    TEST_ASSERT_TRUE(test_case_setenv(test_env_name, "inner"),
                     "test case env override should succeed");
    after = getenv(test_env_name);
    TEST_ASSERT_NOT_NULL(after);
    TEST_ASSERT_STR_EQ(after, "inner");
}

static void test_env_override_is_restored_after_case(void)
{
    const char *value = getenv(test_env_name);

    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_STR_EQ(value, "outer");
}

static void test_stdout_capture_is_available_in_case(void)
{
    const char *path = NULL;
    char buf[128];
    FILE *f;
    char *line;
    int saved_stdout = test_case_begin_stdout_capture("stdout-capture.txt", &path);

    TEST_ASSERT_TRUE(saved_stdout >= 0,
                     "stdout capture should start successfully");
    TEST_ASSERT_NOT_NULL(path);

    snprintf(last_stdout_capture_file, sizeof(last_stdout_capture_file), "%s", path);

    printf("captured-line\n");
    TEST_ASSERT_TRUE(test_case_end_stdout_capture(saved_stdout),
                     "stdout capture should restore stdout successfully");

    f = fopen(path, "r");
    TEST_ASSERT_NOT_NULL(f);
    line = fgets(buf, sizeof(buf), f);
    fclose(f);

    TEST_ASSERT_NOT_NULL(line);
    TEST_ASSERT_STR_EQ(buf, "captured-line\n");
}

static void test_stdout_capture_file_is_cleaned_after_case(void)
{
    TEST_ASSERT_TRUE(last_stdout_capture_file[0] != '\0',
                     "stdout capture path should have been recorded");
    TEST_ASSERT_TRUE(access(last_stdout_capture_file, F_OK) != 0,
                     "captured stdout file should be removed after its case finishes");
}

static void test_stderr_capture_is_available_in_case(void)
{
    const char *path = NULL;
    char buf[128];
    FILE *f;
    char *line;
    int saved_stderr = test_case_begin_stderr_capture("stderr-capture.txt", &path);

    TEST_ASSERT_TRUE(saved_stderr >= 0,
                     "stderr capture should start successfully");
    TEST_ASSERT_NOT_NULL(path);

    snprintf(last_stderr_capture_file, sizeof(last_stderr_capture_file), "%s", path);

    fprintf(stderr, "captured-error\n");
    TEST_ASSERT_TRUE(test_case_end_stderr_capture(saved_stderr),
                     "stderr capture should restore stderr successfully");

    f = fopen(path, "r");
    TEST_ASSERT_NOT_NULL(f);
    line = fgets(buf, sizeof(buf), f);
    fclose(f);

    TEST_ASSERT_NOT_NULL(line);
    TEST_ASSERT_STR_EQ(buf, "captured-error\n");
}

static void test_stderr_capture_file_is_cleaned_after_case(void)
{
    TEST_ASSERT_TRUE(last_stderr_capture_file[0] != '\0',
                     "stderr capture path should have been recorded");
    TEST_ASSERT_TRUE(access(last_stderr_capture_file, F_OK) != 0,
                     "captured stderr file should be removed after its case finishes");
}

static void test_output_example_runs(void)
{
    TEST_ASSERT_TRUE(TEST_ENABLED(NULL),
                     "output examples default to enabled");
    TEST_ASSERT_TRUE(TEST_CONFIG_HAS_KEY(NULL),
                     "output examples are materialized for regeneration");
    output_example_ran = 1;
}

static void test_output_example_disabled(void)
{
    output_example_disabled_ran = 1;
}

static void test_invalid_parent_name_is_rejected(void)
{
    TEST_ASSERT_FALSE(TEST_ENABLED("missing..deep.parent.path"),
                      "invalid parent paths must be rejected");
    TEST_ASSERT_FALSE(TEST_CONFIG_HAS_KEY("missing..deep.parent.path"),
                      "invalid parent paths must not be materialized");
}

static void test_validity_contract_success(void)
{
    int expected = 42;
    int actual = 42;

    TEST_ASSERT_VALID_NAMED("int-equality", &actual, &expected);
}

static void test_builtin_string_validity_contract_success(void)
{
    TEST_ASSERT_STR_EQ("mars", "mars");
}

static void test_explicit_skip_example(void)
{
    TEST_SKIP("intentional self-test skip");
}

static void test_prunes_stale_keys_and_old_nesting(void)
{
    test_config_save();

    TEST_ASSERT_FALSE(file_contains_text(test_local_config_path(), "\"obsolete_top_level\""),
                      "obsolete top-level keys should be pruned");
    TEST_ASSERT_FALSE(file_contains_text(test_local_config_path(), "\"stale_branch\""),
                      "stale group children should be pruned");
    TEST_ASSERT_FALSE(file_contains_text(test_local_config_path(), "\"stale_old_leaf\""),
                      "stale deep nesting should be pruned");
    TEST_ASSERT_FALSE(file_contains_text(test_local_config_path(), "\"stale_nested\""),
                      "old nesting beneath active groups should be pruned");
    TEST_ASSERT_TRUE(file_contains_text(test_local_config_path(), "\"test_parent_group\""),
                     "active groups should remain in the regenerated config");
    TEST_ASSERT_TRUE(file_contains_text(test_local_config_path(), "\"test_output_example_runs\""),
                     "output examples should be regenerated into config");
}

static void test_output_examples_respect_config(void)
{
    TEST_ASSERT_INT_EQ(output_example_ran, 1);
    TEST_ASSERT_INT_EQ(output_example_disabled_ran, 0);
}

static void test_parent_group(void)
{
    TEST_RUN_SUBTEST(test_subtest_grouping, NULL);
}

/* ------------------------------------------------------------------------- */
/* Main                                                                      */
/* ------------------------------------------------------------------------- */

int tests_main(void)
{
    seed_local_config_with_stale_entries();

    printf("Running test_config tests...\n");
    TEST_SECTION("Configuration");

    TEST_RUN_CASE(test_top_level_default_true, "config,defaults");
    TEST_RUN_IN_GROUP(test_subtest_default_true,
                      test_top_level_default_true,
                      "config,defaults,group");
    TEST_RUN_CASE(test_repeat_lookup_same_value, "config,regeneration");
    TEST_RUN_CASE(test_fixture_is_active_for_case, "config,fixture");
    TEST_RUN_CASE(test_parent_group, "config,nesting");
    TEST_RUN_CASE(test_fixture_balance_after_nested_group, "config,fixture");
    TEST_RUN_CASE(test_temp_resources_are_available_in_case, "config,resource");
    TEST_RUN_CASE(test_temp_resources_are_cleaned_after_case, "config,resource");
    TEST_RUN_CASE(test_env_override_is_available_in_case, "config,resource");
    TEST_RUN_CASE(test_env_override_is_restored_after_case, "config,resource");
    TEST_RUN_CASE(test_stdout_capture_is_available_in_case, "config,resource");
    TEST_RUN_CASE(test_stdout_capture_file_is_cleaned_after_case, "config,resource");
    TEST_RUN_CASE(test_stderr_capture_is_available_in_case, "config,resource");
    TEST_RUN_CASE(test_stderr_capture_file_is_cleaned_after_case, "config,resource");
    TEST_RUN_OUTPUT_TAGS(test_output_example_runs, "config,output");
    TEST_RUN_OUTPUT_TAGS(test_output_example_disabled, "config,output");
    TEST_RUN_CASE(test_output_examples_respect_config, "config,output");
    TEST_RUN_CASE(test_missing_nested_path_is_materialized, "config,regeneration");
    TEST_RUN_CASE(test_invalid_parent_name_is_rejected, "config,validation");
    TEST_RUN_CASE(test_validity_contract_success, "config,validation");
    TEST_RUN_CASE(test_builtin_string_validity_contract_success, "config,validation");
    TEST_RUN_CASE(test_explicit_skip_example, "config,skip");
    TEST_RUN_CASE(test_prunes_stale_keys_and_old_nesting, "config,regeneration");
    return TEST_EXIT_CODE();
}
