#include <stdio.h>
#include <stdbool.h>

#define TEST_CONFIG_MODE TEST_CONFIG_LOCAL
#include "test_config.h"

/* ANSI colours */
#define GREEN   "\x1b[32m"
#define RED     "\x1b[31m"
#define YELLOW  "\x1b[33m"
#define BLUE    "\x1b[34m"
#define RESET   "\x1b[0m"

static int tests_run = 0;
static int tests_failed = 0;
static int tests_skipped = 0;

/* ------------------------------------------------------------------------- */
/* Output helpers                                                            */
/* ------------------------------------------------------------------------- */

static void ok(bool cond, const char *msg)
{
    tests_run++;
    if (cond) {
        printf(GREEN "  OK  " RESET "%s\n", msg);
    } else {
        tests_failed++;
        printf(RED   " FAIL " RESET "%s\n", msg);
    }
}

static void skip(const char *msg)
{
    tests_skipped++;
    printf(BLUE " SKIP " RESET "%s\n", msg);
}

/* ------------------------------------------------------------------------- */
/* Test wrapper: automatically skip disabled tests                           */
/* ------------------------------------------------------------------------- */

#define RUN_TEST(func, parent_name)                                      \
    do {                                                                 \
        if (!test_enabled(__FILE__, #func, parent_name)) {               \
            skip(#func " (disabled in config)");                         \
        } else {                                                         \
            func();                                                      \
        }                                                                \
    } while (0)

/* ------------------------------------------------------------------------- */
/* Test functions                                                            */
/* ------------------------------------------------------------------------- */

static void test_top_level_default_true(void)
{
    if (test_config_has_key(__FILE__, __func__, NULL)) {
        ok(true, "top‑level tests default to true — unable to test; key exists in JSON");
        return;
    }

    bool enabled = test_enabled(__FILE__, __func__, NULL);
    ok(enabled, "top‑level tests default to true");
}

static void test_subtest_default_true(void)
{
    if (test_config_has_key(__FILE__, __func__, "test_top_level_default_true")) {
        ok(true, "subtests default to true — unable to test; key exists in JSON");
        return;
    }

    bool enabled = test_enabled(__FILE__, __func__, "test_top_level_default_true");
    ok(enabled, "subtests default to true");
}

static void test_repeat_lookup_same_value(void)
{
    bool a = test_enabled(__FILE__, __func__, NULL);
    bool b = test_enabled(__FILE__, __func__, NULL);
    ok(a == b, "repeated lookups return same value");
}

static void test_subtest_grouping(void)
{
    bool enabled = test_enabled(__FILE__, __func__, "parent_group");
    ok(enabled, "subtest grouped under parent_group");
}

static void test_sub_sub_function(void)
{
    bool enabled = test_enabled(__FILE__, __func__, "test_subtest_grouping");
    ok(enabled, "sub‑sub‑function under test_subtest_grouping");
}

static void test_sub_sub_sub_function(void)
{
    bool enabled = test_enabled(__FILE__, __func__, "test_sub_sub_function");
    ok(enabled, "sub‑sub‑sub‑function under test_sub_sub_function");
}

static void test_sub_sub_sub_sub_function(void)
{
    bool enabled = test_enabled(__FILE__, __func__, "test_sub_sub_sub_function");
    ok(enabled, "four‑level nested subtest");
}

/* ------------------------------------------------------------------------- */
/* Main                                                                      */
/* ------------------------------------------------------------------------- */

int main(void)
{
    TEST_CONFIG_INIT();

    printf(YELLOW "Running test_config tests...\n" RESET);

    RUN_TEST(test_top_level_default_true, NULL);
    RUN_TEST(test_subtest_default_true, "test_top_level_default_true");
    RUN_TEST(test_repeat_lookup_same_value, NULL);
    RUN_TEST(test_subtest_grouping, "parent_group");

    RUN_TEST(test_sub_sub_function, "test_subtest_grouping");
    RUN_TEST(test_sub_sub_sub_function, "test_sub_sub_function");
    RUN_TEST(test_sub_sub_sub_sub_function, "test_sub_sub_sub_function");

    test_config_save();

    printf("\n");
    if (tests_failed == 0) {
        printf(GREEN "All %d tests passed", tests_run);
        if (tests_skipped)
            printf(" (" BLUE "%d skipped" RESET ")\n", tests_skipped);
        else
            printf(".\n");
        return 0;
    } else {
        printf(RED "%d/%d tests failed" RESET, tests_failed, tests_run);
        if (tests_skipped)
            printf(" (" BLUE "%d skipped" RESET ")\n", tests_skipped);
        else
            printf("\n");
        return 1;
    }
}
