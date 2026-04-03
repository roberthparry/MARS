#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#ifndef TEST_CONFIG_MODE
#error "You must #define TEST_CONFIG_MODE before including test_harness.h"
#endif

#include "test_config.h"
#include <stdio.h>

/* ------------------------------------------------------------------------- */
/* Colours (optional, purely cosmetic)                                       */
/* ------------------------------------------------------------------------- */

#define C_GREEN   "\x1b[32m"
#define C_RED     "\x1b[31m"
#define C_YELLOW  "\x1b[33m"
#define C_CYAN    "\x1b[36m"
#define C_RESET   "\x1b[0m"

/* ------------------------------------------------------------------------- */
/* Global test state (shared across all test files)                          */
/* ------------------------------------------------------------------------- */

extern int tests_run;
extern int tests_failed;

/* ------------------------------------------------------------------------- */
/* Optional RUN_TEST() wrapper                                               */
/* ------------------------------------------------------------------------- */

#define RUN_TEST(func, parent)                                     \
    do {                                                           \
        if (!test_enabled(__FILE__, #func, parent)) {              \
            printf(C_YELLOW "SKIP: %s (disabled)\n" C_RESET,       \
                   #func);                                         \
        } else {                                                   \
            tests_run++;                                           \
            func();                                                \
        }                                                          \
    } while (0)

/* ------------------------------------------------------------------------- */
/* User must define this in their test file                                  */
/* ------------------------------------------------------------------------- */

int tests_main(void);

/* ------------------------------------------------------------------------- */
/* Harness-owned main()                                                       */
/* ------------------------------------------------------------------------- */

int tests_run = 0;
int tests_failed = 0;

int main(void) {
    /* reference counters so GCC never warns about them being unused */
    tests_run = tests_run;
    tests_failed = tests_failed;

    /* load test configuration */
    test_config_set_mode(TEST_CONFIG_MODE);

    /* run user tests */
    int rc = tests_main();

    /* save configuration */
    test_config_save();

    /* summary */
    int passed = tests_run - tests_failed;
    printf(C_CYAN "SUMMARY: " C_RESET
           "%d run, " C_GREEN "%d passed" C_RESET ", "
           C_RED "%d failed" C_RESET "\n",
           tests_run, passed, tests_failed);

    return rc;
}

#endif /* TEST_HARNESS_H */
