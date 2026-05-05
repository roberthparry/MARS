#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

/**
 * @file test_harness.h
 * @brief Lightweight test runner with per-test enable/disable and group support.
 *
 * Usage:
 *   1. In exactly one test translation unit, define:
 *        #define TEST_CONFIG_MODE TEST_CONFIG_GLOBAL   // shared config file
 *        #define TEST_CONFIG_MODE TEST_CONFIG_LOCAL    // per-test-file config
 *        #define TEST_CONFIG_MAIN
 *      before including this header.
 *   2. Define the entry point expected by main():
 *        int tests_main(void) {
 *            TEST_SECTION("Arithmetic");
 *            RUN_TEST_CASE(test_addition);
 *            return TESTS_EXIT_CODE();
 *        }
 *   3. Any additional helper translation units may include this header
 *      without defining TEST_CONFIG_MAIN.
 *   4. Use the helper macros for the common cases:
 *        RUN_TEST_CASE(test_addition);     // top-level JSON entry
 *        RUN_SUBTEST(test_parse_basic);    // child of the current function
 *        RUN_TEST(test_group_math, NULL);  // explicit parent when needed
 *
 * RUN_TEST behaviour:
 *   • Consults test_config to decide whether to SKIP the test.
 *   • After calling func(), if sub-tests were skipped it prints a GROUP summary;
 *     otherwise it prints PASS or FAIL for the individual test.
 *   • Assertion macros (ASSERT_*) call TEST_FAIL() and continue to the next
 *     iteration of the enclosing for/while loop — tests are conventionally
 *     wrapped in a for(;;){ ... break; } body or use a do/while(0) pattern.
 *
 * The harness tracks both:
 *   • assertion-level counters (`tests_failed`)
 *   • case-level counters for summary reporting
 * and owns them; do not modify them directly.
 */

#include <stdio.h>
#include <math.h>
#include <float.h>
#include <string.h>
#include <time.h>

#include "test_config.h"

/* Colours */

#define C_GREEN   "\x1b[32m"
#define C_RED     "\x1b[31m"
#define C_YELLOW  "\x1b[33m"
#define C_CYAN    "\x1b[36m"
#define C_RESET   "\x1b[0m"
#define C_BOLD    "\x1b[1m"
#define C_DIM     "\x1b[2m"
#define C_WHITE   "\x1b[97m"
#define C_GREY    "\x1b[90m"
#define C_MAGENTA "\x1b[95m"

/* Global test state */

extern int    tests_run;
extern int    tests_failed;
extern int    tests_skipped;
extern int    tests_passed_cases;
extern int    tests_failed_cases;
extern double tests_total_ms;
extern int    tests_rts;      /* RUN_TEST starts — harness-internal, for group detection */
extern const char *tests_last_fail_file;
extern int         tests_last_fail_line;

/* Timing helpers */

static inline double th_elapsed_ms(struct timespec t0, struct timespec t1) {
    return (t1.tv_sec - t0.tv_sec) * 1000.0
         + (t1.tv_nsec - t0.tv_nsec) / 1e6;
}

static inline void th_print_time(double ms) {
    printf(C_GREY "  [");
    if (ms < 0.001) {
        long ns = (long)(ms * 1000000.0 + 0.5);
        if (ns < 1) printf("< 1 ns");
        else        printf("%ld ns", ns);
    }
    else if (ms < 1.0)
        printf("%.1f µs", ms * 1000.0);
    else if (ms < 1000.0)
        printf("%.1f ms", ms);
    else
        printf("%.2f s", ms / 1000.0);
    printf("]" C_RESET);
}

static inline void th_print_section(const char *title)
{
    if (!title)
        title = "";
    printf(C_BOLD C_CYAN "=== %s ===\n" C_RESET, title);
}

/* Assertion helpers */

#define ASSERT_TRUE(expr)                                               \
    do {                                                                \
        if (!(expr)) {                                                  \
            printf(C_RED "    Assertion failed at %s:%d: %s\n" C_RESET, \
                   __FILE__, __LINE__, #expr);                          \
            TEST_FAIL_AT(__FILE__, __LINE__);                           \
            continue;                                                   \
        }                                                               \
    } while (0)

#define ASSERT_EQ_INT(actual, expected)                         \
    do {                                                        \
        if ((actual) != (expected)) {                           \
            printf(C_RED "    Assertion failed at %s:%d: "      \
                   "expected %d, got %d\n" C_RESET,             \
                   __FILE__, __LINE__,                           \
                   (int)(expected), (int)(actual));             \
            TEST_FAIL_AT(__FILE__, __LINE__);                   \
            continue;                                           \
        }                                                       \
    } while (0)

#define ASSERT_EQ_LONG(actual, expected)                        \
    do {                                                        \
        if ((actual) != (expected)) {                           \
            printf(C_RED "    Assertion failed at %s:%d: "      \
                   "expected %ld, got %ld\n" C_RESET,           \
                   __FILE__, __LINE__,                           \
                   (long)(expected), (long)(actual));           \
            TEST_FAIL_AT(__FILE__, __LINE__);                   \
            continue;                                           \
        }                                                       \
    } while (0)

#define ASSERT_EQ_DOUBLE(actual, expected, eps)                     \
    do {                                                            \
        if (fabs((actual) - (expected)) > (eps)) {                  \
            printf(C_RED "    Assertion failed at %s:%d: "          \
                   "expected %.12f, got %.12f\n" C_RESET,           \
                   __FILE__, __LINE__,                               \
                   (double)(expected), (double)(actual));           \
            TEST_FAIL_AT(__FILE__, __LINE__);                       \
            continue;                                               \
        }                                                           \
    } while (0)

#define ASSERT_NOT_NULL(ptr)                                            \
    do {                                                                \
        if ((ptr) == NULL) {                                            \
            printf(C_RED "    Assertion failed at %s:%d: "              \
                   "expected non-null pointer\n" C_RESET,               \
                   __FILE__, __LINE__);                                  \
            TEST_FAIL_AT(__FILE__, __LINE__);                           \
            continue;                                                   \
        }                                                               \
    } while (0)

#define ASSERT_NULL(ptr)                                            \
    do {                                                            \
        if ((ptr) != NULL) {                                        \
            printf(C_RED "    Assertion failed at %s:%d: "          \
                   "expected NULL pointer\n" C_RESET,               \
                   __FILE__, __LINE__);                              \
            TEST_FAIL_AT(__FILE__, __LINE__);                       \
            continue;                                               \
        }                                                           \
    } while (0)

/* RUN_TEST — SKIP, PASS/FAIL */

#define RUN_TEST(func, parent)                                                    \
    do {                                                                          \
        tests_rts++;                                                              \
        /* Check enable/disable state */                                          \
        if (!test_enabled(__FILE__, #func, parent)) {                             \
            const char *__th_parent = (parent);                                   \
            if (__th_parent)                                                      \
                printf(C_YELLOW "SKIP: %s (in %s)" C_RESET "\n",                  \
                       #func, __th_parent);                                       \
            else                                                                  \
                printf(C_YELLOW "SKIP: %s" C_RESET "\n", #func);                  \
            tests_skipped++;                                                      \
            break;                                                                \
        }                                                                         \
                                                                                  \
        tests_run++;                                                              \
                                                                                  \
        int failed_before  = tests_failed;                                        \
        int skipped_before = tests_skipped;                                       \
        int passed_cases_before = tests_passed_cases;                             \
        int failed_cases_before = tests_failed_cases;                             \
        int rts_before     = tests_rts;                                           \
                                                                                  \
        double __total_ms_before = tests_total_ms;                                \
        struct timespec __th_t0, __th_t1;                                         \
        clock_gettime(CLOCK_MONOTONIC, &__th_t0);                                 \
        func();                                                                   \
        clock_gettime(CLOCK_MONOTONIC, &__th_t1);                                 \
        double __th_ms = th_elapsed_ms(__th_t0, __th_t1);                         \
                                                                                  \
        int __is_group = (tests_rts > rts_before);                                \
        double __disp_ms = __is_group                                             \
                         ? (tests_total_ms - __total_ms_before)                   \
                         : __th_ms;                                               \
        if (!__is_group)                                                          \
            tests_total_ms += __th_ms;                                            \
                                                                                  \
        if (tests_skipped > skipped_before) {                                     \
            int skipped_after = tests_skipped;                                    \
            int passed_cases_after = tests_passed_cases;                          \
            int failed_cases_after = tests_failed_cases;                          \
                                                                                  \
            int failed  = failed_cases_after - failed_cases_before;               \
            int skipped = skipped_after - skipped_before;                         \
            int passed  = passed_cases_after - passed_cases_before;               \
                                                                                  \
            printf("\r" C_CYAN "GROUP: %s"                                        \
                   " " C_RESET "(" C_GREEN "%d passed" C_RESET                    \
                   "," C_RED " %d failed" C_RESET                                 \
                   "," C_YELLOW " %d skipped" C_RESET ")",                        \
                   #func, passed, failed, skipped);                               \
        } else if (tests_failed == failed_before) {                               \
            tests_passed_cases++;                                                 \
            printf(C_BOLD C_GREEN "PASS: " C_RESET "%s", #func);                  \
        } else {                                                                  \
            tests_failed_cases++;                                                 \
            printf(C_BOLD C_RED "FAIL: " C_RESET "%s " C_RED "(%s:%d)" C_RESET,   \
                   #func,                                                         \
                   tests_last_fail_file ? tests_last_fail_file : __FILE__,        \
                   tests_last_fail_file ? tests_last_fail_line : __LINE__);       \
        }                                                                         \
        th_print_time(__disp_ms);                                                 \
        putchar('\n');                                                            \
    } while (0)

#define RUN_TEST_CASE(func) RUN_TEST(func, NULL)

#define RUN_SUBTEST(func) RUN_TEST(func, __func__)

#define TEST_SECTION(title)        \
    do {                           \
        th_print_section(title);   \
    } while (0)

#define TESTS_EXIT_CODE() (tests_failed ? 1 : 0)


#define TEST_FAIL_AT(file, line)          \
    do {                                  \
        tests_failed++;                   \
        tests_last_fail_file = (file);    \
        tests_last_fail_line = (line);    \
    } while (0)

#define TEST_FAIL() TEST_FAIL_AT(__FILE__, __LINE__)

/* Define this in your test file. Call RUN_TEST() for each test. */
int tests_main(void);

#ifdef TEST_CONFIG_MAIN

#ifndef TEST_CONFIG_MODE
#error "You must #define TEST_CONFIG_MODE before defining TEST_CONFIG_MAIN"
#endif

/* Harness-owned — do not modify */

int    tests_run      = 0;
int    tests_failed   = 0;
int    tests_skipped  = 0;
int    tests_passed_cases = 0;
int    tests_failed_cases = 0;
double tests_total_ms = 0.0;
int    tests_rts      = 0;
const char *tests_last_fail_file = NULL;
int         tests_last_fail_line = 0;

int main(void) {
    test_config_set_mode(TEST_CONFIG_MODE);

    int rc = tests_main();

    test_config_save();
    test_config_shutdown();

    printf("\n" C_CYAN "SUMMARY: " C_RESET
           "%d run, " C_GREEN "%d passed" C_RESET ", "
           C_RED "%d failed" C_RESET ", "
           C_YELLOW "%d skipped" C_RESET,
           tests_run, tests_passed_cases, tests_failed_cases, tests_skipped);
    th_print_time(tests_total_ms);
    putchar('\n');

    return rc;
}

#endif

#endif /* TEST_HARNESS_H */
