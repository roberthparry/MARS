#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#ifndef TEST_CONFIG_MODE
#error "You must #define TEST_CONFIG_MODE before including test_harness.h"
#endif

#include <stdio.h>
#include <math.h>
#include <float.h>
#include <string.h>

#include "test_config.h"

/* ------------------------------------------------------------------------- */
/* Colours                                                                   */
/* ------------------------------------------------------------------------- */

#define C_GREEN   "\x1b[32m"
#define C_RED     "\x1b[31m"
#define C_YELLOW  "\x1b[33m"
#define C_CYAN    "\x1b[36m"
#define C_RESET   "\x1b[0m"
#define C_BOLD    "\x1b[1m"

/* ------------------------------------------------------------------------- */
/* Global test state                                                         */
/* ------------------------------------------------------------------------- */

extern int tests_run;
extern int tests_failed;
extern int tests_skipped;

/* ------------------------------------------------------------------------- */
/* Assertion helpers                                                         */
/* ------------------------------------------------------------------------- */

#define ASSERT_TRUE(expr)                                               \
    do {                                                                \
        if (!(expr)) {                                                  \
            tests_failed++;                                             \
            printf(C_RED "    Assertion failed: %s\n" C_RESET, #expr);  \
            TEST_FAIL();                                                \
            continue;                                                   \
        }                                                               \
    } while (0)

#define ASSERT_EQ_INT(actual, expected)                         \
    do {                                                        \
        if ((actual) != (expected)) {                           \
            tests_failed++;                                     \
            printf(C_RED "    Expected %d, got %d\n" C_RESET,   \
                   (int)(expected), (int)(actual));             \
            TEST_FAIL();                                        \
            continue;                                           \
        }                                                       \
    } while (0)

#define ASSERT_EQ_LONG(actual, expected)                        \
    do {                                                        \
        if ((actual) != (expected)) {                           \
            tests_failed++;                                     \
            printf(C_RED "    Expected %ld, got %ld\n" C_RESET, \
                   (long)(expected), (long)(actual));           \
            TEST_FAIL();                                        \
            continue;                                           \
        }                                                       \
    } while (0)

#define ASSERT_EQ_DOUBLE(actual, expected, eps)                     \
    do {                                                            \
        if (fabs((actual) - (expected)) > (eps)) {                  \
            tests_failed++;                                         \
            printf(C_RED "    Expected %.12f, got %.12f\n" C_RESET, \
                   (double)(expected), (double)(actual));           \
            TEST_FAIL();                                            \
            continue;                                               \
        }                                                           \
    } while (0)

#define ASSERT_NOT_NULL(ptr)                                            \
    do {                                                                \
        if ((ptr) == NULL) {                                            \
            tests_failed++;                                             \
            printf(C_RED "    Expected non-null pointer\n" C_RESET);    \
            TEST_FAIL();                                                \
            continue;                                                   \
        }                                                               \
    } while (0)

#define ASSERT_NULL(ptr)                                            \
    do {                                                            \
        if ((ptr) != NULL) {                                        \
            tests_failed++;                                         \
            printf(C_RED "    Expected NULL pointer\n" C_RESET);    \
            TEST_FAIL();                                            \
            continue;                                               \
        }                                                           \
    } while (0)

/* ------------------------------------------------------------------------- */
/* RUN_TEST — SKIP, PASS/FAIL                                                */
/* ------------------------------------------------------------------------- */

#define RUN_TEST(func, parent)                                                    \
    do {                                                                          \
        /* Check enable/disable state */                                          \
        if (!test_enabled(__FILE__, #func, parent)) {                             \
            printf(C_YELLOW "SKIP: %s\n" C_RESET, #func);                         \
            tests_skipped++;                                                      \
            break;                                                                \
        }                                                                         \
                                                                                  \
        tests_run++;                                                              \
                                                                                  \
        int run_before     = tests_run;                                           \
        int failed_before  = tests_failed;                                        \
        int skipped_before = tests_skipped;                                       \
                                                                                  \
        func();                                                                   \
                                                                                  \
        if (tests_skipped > skipped_before) {                                     \
            int run_after     = tests_run;                                        \
            int failed_after  = tests_failed;                                     \
            int skipped_after = tests_skipped;                                    \
                                                                                  \
            int total   = (run_after - run_before);                               \
            int failed  = failed_after  - failed_before;                          \
            int skipped = skipped_after - skipped_before;                         \
            int passed  = total - failed;                                         \
                                                                                  \
            printf(C_CYAN "GROUP: %s"                                             \
                   " " C_RESET "(" C_GREEN "%d passed" C_RESET                    \
                   "," C_RED " %d failed" C_RESET                                 \
                   "," C_YELLOW " %d skipped" C_RESET ")\n" C_RESET,              \
                   #func, passed, failed, skipped);                               \
        } else if (tests_failed == failed_before) {                               \
            printf(C_BOLD C_GREEN "PASS: " C_RESET "%s\n", #func);                \
        } else {                                                                  \
            printf(C_BOLD C_RED "FAIL: " C_RESET "%s " C_RED "(%s:%d)\n" C_RESET, \
                   #func, __FILE__, __LINE__);                                    \
        }                                                                         \
    } while (0)

/* ------------------------------------------------------------------------- */
/* CALL TEST_FAIL() to mark test as failed                                   */
/* ------------------------------------------------------------------------- */

#define TEST_FAIL() do { tests_failed++; } while (0)

/* ------------------------------------------------------------------------- */
/* User must define this                                                     */
/* ------------------------------------------------------------------------- */

/// @brief The main test entry point. This function should be defined by the user in their test file and will be called by the test 
///        harness to execute the tests. It should return 0 on success or a non-zero value if there was an error during test execution.
/// @return 0 on success, non-zero on error.
/// @note There is no need to define a main() function in the test file, as the test harness provides its own main() that calls this 
///       tests_main() function. The test harness will automatically track the number of tests run, passed, failed, and skipped based 
///       on the assertions and the RUN_TEST macro used in this function. The user should not modify the global test state variables 
///       directly, but should use the provided macros and functions to ensure accurate tracking of test results. The tests_main() 
///       function should contain calls to RUN_TEST() for each test function defined in the test file, and can also include any necessary
///       setup or teardown code for the tests.
int tests_main(void);

/* ------------------------------------------------------------------------- */
/* Harness-owned main(). do not modify!                                      */
/* ------------------------------------------------------------------------- */

int tests_run = 0;
int tests_failed = 0;
int tests_skipped = 0;

int main(void) {

    /* Load config */
    test_config_set_mode(TEST_CONFIG_MODE);

    /* Run tests */
    int rc = tests_main();

    /* Save config */
    test_config_save();

    /* Release config resources */
    test_config_shutdown();

    /* Summary */
    int passed = tests_run - tests_failed;

    printf("\n" C_CYAN "SUMMARY: " C_RESET
           "%d run, " C_GREEN "%d passed" C_RESET ", "
           C_RED "%d failed" C_RESET ", "
           C_YELLOW "%d skipped" C_RESET "\n",
           tests_run, passed, tests_failed, tests_skipped);

    return rc;
}

#endif /* TEST_HARNESS_H */
