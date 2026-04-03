#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#ifndef TEST_CONFIG_MODE
#error "You must #define TEST_CONFIG_MODE before including test_harness.h"
#endif

#include "test_config.h"
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* Colours                                                                    */
/* ------------------------------------------------------------------------- */

#define C_GREEN   "\x1b[32m"
#define C_RED     "\x1b[31m"
#define C_YELLOW  "\x1b[33m"
#define C_CYAN    "\x1b[36m"
#define C_RESET   "\x1b[0m"

/* ------------------------------------------------------------------------- */
/* Global test state                                                          */
/* ------------------------------------------------------------------------- */

extern int tests_run;
extern int tests_failed;
extern int tests_skipped;

/* Group header state */
static int __group_should_print = 0;
static const char *__group_name = NULL;

/* ------------------------------------------------------------------------- */
/* Assertion helpers                                                          */
/* ------------------------------------------------------------------------- */

#define ASSERT_TRUE(expr) \
    do { \
        if (!(expr)) { \
            tests_failed++; \
            printf(C_RED "    Assertion failed: %s\n" C_RESET, #expr); \
            return; \
        } \
    } while (0)

#define ASSERT_EQ_INT(actual, expected) \
    do { \
        if ((actual) != (expected)) { \
            tests_failed++; \
            printf(C_RED "    Expected %d, got %d\n" C_RESET, \
                   (int)(expected), (int)(actual)); \
            return; \
        } \
    } while (0)

#define ASSERT_EQ_LONG(actual, expected) \
    do { \
        if ((actual) != (expected)) { \
            tests_failed++; \
            printf(C_RED "    Expected %ld, got %ld\n" C_RESET, \
                   (long)(expected), (long)(actual)); \
            return; \
        } \
    } while (0)

#define ASSERT_EQ_DOUBLE(actual, expected, eps) \
    do { \
        if (fabs((actual) - (expected)) > (eps)) { \
            tests_failed++; \
            printf(C_RED "    Expected %.12f, got %.12f\n" C_RESET, \
                   (double)(expected), (double)(actual)); \
            return; \
        } \
    } while (0)

#define ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            tests_failed++; \
            printf(C_RED "    Expected non-null pointer\n" C_RESET); \
            return; \
        } \
    } while (0)

#define ASSERT_NULL(ptr) \
    do { \
        if ((ptr) != NULL) { \
            tests_failed++; \
            printf(C_RED "    Expected NULL pointer\n" C_RESET); \
            return; \
        } \
    } while (0)

/* ------------------------------------------------------------------------- */
/* Group heading macro                                                        */
/* ------------------------------------------------------------------------- */

#define TEST_GROUP(name) \
    do { \
        __group_should_print = 1; \
        __group_name = (name); \
    } while (0)

/* ------------------------------------------------------------------------- */
/* RUN_TEST — SKIP, PASS/FAIL, group headers                                  */
/* ------------------------------------------------------------------------- */

#define RUN_TEST(func, parent) \
    do { \
        if (!test_enabled(__FILE__, #func, parent)) { \
            printf(C_YELLOW "SKIP: %s (%s:%d)\n" C_RESET, \
                   #func, __FILE__, __LINE__); \
            tests_skipped++; \
            break; \
        } \
        if (__group_should_print) { \
            printf("\n" C_CYAN "== %s ==\n" C_RESET, __group_name); \
            __group_should_print = 0; \
        } \
        int before = tests_failed; \
        tests_run++; \
        func(); \
        if (tests_failed == before) { \
            printf(C_GREEN "PASS: %s (%s:%d)\n" C_RESET, \
                   #func, __FILE__, __LINE__); \
        } else { \
            printf(C_RED "FAIL: %s (%s:%d)\n" C_RESET, \
                   #func, __FILE__, __LINE__); \
        } \
    } while (0)

/* ------------------------------------------------------------------------- */
/* User must define this                                                      */
/* ------------------------------------------------------------------------- */

int tests_main(void);

/* ------------------------------------------------------------------------- */
/* Harness-owned main()                                                       */
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
