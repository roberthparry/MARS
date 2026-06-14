#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#include <stdbool.h>
#include <stddef.h>

#include "test_config.h"
#include "ustring.h"

typedef void    (*test_fn)                  (void);

typedef bool    (*test_suite_setup_fn)      (void);

typedef bool    (*test_fixture_setup_fn)    (void);

typedef bool    (*test_fixture_teardown_fn) (void);

typedef void    (*test_post_summary_fn)     (void);

typedef int     (*test_validity_equal_fn)   (const void *actual, const void *expected, void *ctx);

typedef int     (*test_validity_format_fn)  (const void *value, string_t *out, void *ctx);

typedef struct {
    const char *name;
    test_validity_equal_fn equal;
    test_validity_format_fn format;
    void *ctx;
} test_validity_contract_t;

const test_validity_contract_t *test_validity_contract_int(void);
const test_validity_contract_t *test_validity_contract_long(void);
const test_validity_contract_t *test_validity_contract_cstr(void);

extern const test_config_mode_t test_suite_mode;
extern test_suite_setup_fn test_suite_setup_hook;
extern test_fixture_setup_fn test_fixture_setup_hook;
extern test_fixture_teardown_fn test_fixture_teardown_hook;
extern test_post_summary_fn test_post_summary_hook;

/*
 * Recommended suite shape:
 *
 * 1. Pick a config mode explicitly with TEST_SUITE_CONFIG(...).
 * 2. Use TEST_SUITE_SETUP(...) to register and require the suite's named
 *    validity checkers before any cases run.
 * 3. Keep semantic validity and presentation validity separate.
 *    Semantic checks should use TEST_ASSERT_VALID_NAMED(...) through
 *    suite-local wrappers such as ASSERT_NUMBER_EQ(...).
 *    Formatting and README/output checks should stay as explicit helpers or
 *    TEST_RUN_OUTPUT(...) cases rather than being hidden inside a validity
 *    contract.
 * 4. Expose a small suite-local assertion vocabulary in the suite header.
 *    A handful of well-named wrappers is better than forcing every test file
 *    to remember raw checker names or contract plumbing.
 * 5. Allow multiple semantic validity lanes when the domain needs them.
 *    A good suite often has more than one notion of "correct":
 *    exact value equality, tolerance-aware equality, mp-real equality,
 *    complex equality, string rendering, or prefix/presentation checks.
 *    Only semantic equality belongs in validity contracts. Presentation
 *    checks should stay explicit.
 * 6. Put README/demo/output code on the output lane with TEST_RUN_OUTPUT(...)
 *    or TEST_RUN_OUTPUT_TAGS(...). Output examples still participate in
 *    config, filtering, and reporting, but they are counted separately from
 *    correctness cases.
 * 7. Use fixtures for per-case resources and suite setup only for readiness
 *    checks such as validity registration.
 * 8. Prefer removing suite-local comparison engines once the harness-backed
 *    validity path is established. Helpers that remain should be thin wrappers
 *    for expected-value construction, labelling, or other domain-specific
 *    setup, not parallel assertion subsystems.
 */

#define TEST_SUITE_CONFIG(mode) \
    const test_config_mode_t test_suite_mode = (mode)

#define TEST_SUITE_SETUP(fn) \
    test_suite_setup_fn test_suite_setup_hook = (fn)

#define TEST_FIXTURE_SETUP(fn) \
    test_fixture_setup_fn test_fixture_setup_hook = (fn)

#define TEST_FIXTURE_TEARDOWN(fn) \
    test_fixture_teardown_fn test_fixture_teardown_hook = (fn)

#define TEST_POST_SUMMARY(fn) \
    test_post_summary_fn test_post_summary_hook = (fn)

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

/** Print a labelled section heading in test output. */
void test_section                   (const char *title);

/** Run one top-level test case if enabled by the active test config. */
void test_run_case                  (const char *file, int line, const char *name, const char *tags,
                                     test_fn fn);

/** Run one nested test case under the currently active group path. */
void test_run_subtest               (const char *file, int line, const char *name, const char *tags,
                                     test_fn fn);

/** Run one test case beneath an explicit parent group. */
void test_run_in_group              (const char *file, int line, const char *name, const char *parent, const char *tags,
                                     test_fn fn);

/** Run an output/demo case, counted separately from correctness cases. */
void test_run_output_case           (const char *file, int line, const char *tags, const char *name,
                                     test_fn fn);

/** Run an output/demo case beneath an explicit parent group. */
void test_run_output_in_group       (const char *file, int line, const char *name, const char *parent,
                                     const char *tags, test_fn fn);

/** Return the process exit code implied by the recorded test results. */
int  test_exit_code                 (void);

/** Record a failure unless @p expr is true. */
bool test_assert_true               (bool expr, const char *file, int line, const char *detail);

/** Record a failure unless @p expr is false. */
bool test_assert_false              (bool expr, const char *file, int line, const char *detail);

/** Record a failure unless two int values are equal. */
bool test_assert_int_eq             (int actual, int expected, const char *file, int line);

/** Record a failure unless two long values are equal. */
bool test_assert_long_eq            (long actual, long expected, const char *file, int line);

/** Record a failure unless two double values differ by no more than @p eps. */
bool test_assert_double_eq          (double actual, double expected, double eps, const char *file, int line);

/** Record a failure unless @p ptr is non-null. */
bool test_assert_not_null           (const void *ptr, const char *file, int line);

/** Record a failure unless @p ptr is null. */
bool test_assert_null               (const void *ptr, const char *file, int line);

/** Record a failure unless @p actual and @p expected satisfy @p contract. */
bool test_assert_validity           (const test_validity_contract_t *contract, const void *actual, const void *expected, const char *file, int line);

/** Record a validity failure using a checker registered by name. */
bool test_assert_validity_named     (const char *name, const void *actual, const void *expected, const char *file, int line);

/** Record a failure unless two C-string boundary values are equal. */
bool test_assert_cstr_eq            (const char *actual, const char *expected, const char *file, int line);

/** Register a named semantic validity checker for later assertions. */
void test_register_validity_checker (const char *name, const test_validity_contract_t *contract);

/** Find a previously registered semantic validity checker. */
const test_validity_contract_t *test_find_validity_checker(const char *name);

/** Require that a named validity checker exists before a suite runs. */
bool test_require_validity_checker  (const char *name, const char *file, int line);

/** Query whether a case is enabled by the active test config. */
bool test_harness_config_is_enabled (const char *file, const char *func, const char *parent);

/** Query whether a case has an explicit key in the active test config. */
bool test_harness_config_has_key    (const char *file, const char *func, const char *parent);

/** Store formatted failure detail for the current case. */
void test_set_failure_detailf       (const char *fmt, ...);

/** Clear any recorded failure or skip detail for the current case. */
void test_clear_failure_detail      (void);

/** Mark the current case as failed at @p file:@p line. */
void test_mark_failure              (const char *file, int line, const char *detail);

/** Mark the current case as explicitly failed at @p file:@p line. */
void test_fail                      (const char *file, int line);

/** Mark the current case as skipped at @p file:@p line. */
void test_mark_skip                 (const char *file, int line, const char *detail);

/** Mark the current case as skipped and return false for assertion-style macros. */
bool test_request_skip              (const char *file, int line, const char *detail);

/** Return the temporary directory allocated for the current case. */
const char *test_case_temp_dir      (void);

/** Return a temporary file path beneath the current case directory. */
const char *test_case_temp_path     (const char *leafname);

/** Redirect stdout to a temporary capture file for the current case. */
int test_case_begin_stdout_capture  (const char *leafname, const char **path_out);

/** Restore stdout after a capture started by test_case_begin_stdout_capture(). */
bool test_case_end_stdout_capture   (int saved_stdout);

/** Redirect stderr to a temporary capture file for the current case. */
int test_case_begin_stderr_capture  (const char *leafname, const char **path_out);

/** Restore stderr after a capture started by test_case_begin_stderr_capture(). */
bool test_case_end_stderr_capture   (int saved_stderr);

/** Run the registered test suite. */
int tests_main(void);

#define TEST_SECTION(title) \
    test_section((title))

#define TEST_ENABLED(parent) \
    test_harness_config_is_enabled(__FILE__, __func__, (parent))

#define TEST_CONFIG_HAS_KEY(parent) \
    test_harness_config_has_key(__FILE__, __func__, (parent))

#define TEST_RUN_CASE(fn, tags) \
    test_run_case(__FILE__, __LINE__, #fn, (tags), (fn))

#define TEST_RUN_SUBTEST(fn, tags) \
    test_run_subtest(__FILE__, __LINE__, #fn, (tags), (fn))

#define TEST_RUN_IN_GROUP(fn, parent, tags) \
    test_run_in_group(__FILE__, __LINE__, #fn, #parent, (tags), (fn))

#define TEST_RUN_OUTPUT(fn) \
    test_run_output_case(__FILE__, __LINE__, NULL, #fn, (fn))

#define TEST_RUN_OUTPUT_TAGS(fn, tags) \
    test_run_output_case(__FILE__, __LINE__, (tags), #fn, (fn))

#define TEST_RUN_OUTPUT_IN_GROUP(fn, parent) \
    test_run_output_in_group(__FILE__, __LINE__, #fn, #parent, NULL, (fn))

#define TEST_RUN_OUTPUT_IN_GROUP_TAGS(fn, parent, tags) \
    test_run_output_in_group(__FILE__, __LINE__, #fn, #parent, (tags), (fn))

#define TEST_EXIT_CODE() \
    test_exit_code()

#define TEST_HARNESS_RETURN_UNLESS(ok_expr) \
    do { \
        if (!(ok_expr)) \
            return; \
    } while (0)

#define TEST_ASSERT_TRUE(expr, detail) \
    TEST_HARNESS_RETURN_UNLESS(test_assert_true((expr), __FILE__, __LINE__, (detail)))

#define TEST_ASSERT_FALSE(expr, detail) \
    TEST_HARNESS_RETURN_UNLESS(test_assert_false((expr), __FILE__, __LINE__, (detail)))

#define TEST_ASSERT_INT_EQ(actual, expected) \
    TEST_HARNESS_RETURN_UNLESS(test_assert_int_eq((actual), (expected), __FILE__, __LINE__))

#define TEST_ASSERT_LONG_EQ(actual, expected) \
    TEST_HARNESS_RETURN_UNLESS(test_assert_long_eq((actual), (expected), __FILE__, __LINE__))

#define TEST_ASSERT_DOUBLE_EQ(actual, expected, eps) \
    TEST_HARNESS_RETURN_UNLESS(test_assert_double_eq((actual), (expected), (eps), __FILE__, __LINE__))

#define TEST_ASSERT_NOT_NULL(ptr) \
    TEST_HARNESS_RETURN_UNLESS(test_assert_not_null((ptr), __FILE__, __LINE__))

#define TEST_ASSERT_NULL(ptr) \
    TEST_HARNESS_RETURN_UNLESS(test_assert_null((ptr), __FILE__, __LINE__))

#define TEST_VALIDITY_CONTRACT(name, equal_fn, format_fn, ctx_ptr) \
    ((test_validity_contract_t){ (name), (equal_fn), (format_fn), (ctx_ptr) })

#define TEST_VALID_INT() \
    test_validity_contract_int()

#define TEST_VALID_LONG() \
    test_validity_contract_long()

#define TEST_VALID_CSTR() \
    test_validity_contract_cstr()

#define TEST_ASSERT_VALID(contract_ptr, actual_ptr, expected_ptr) \
    TEST_HARNESS_RETURN_UNLESS(test_assert_validity((contract_ptr), (actual_ptr), (expected_ptr), __FILE__, __LINE__))

#define TEST_ASSERT_VALID_NAMED(name, actual_ptr, expected_ptr) \
    TEST_HARNESS_RETURN_UNLESS(test_assert_validity_named((name), (actual_ptr), (expected_ptr), __FILE__, __LINE__))

#define TEST_ASSERT_STR_EQ(actual_cstr, expected_cstr) \
    TEST_HARNESS_RETURN_UNLESS(test_assert_cstr_eq((actual_cstr), (expected_cstr), __FILE__, __LINE__))

#define TEST_FAIL() \
    test_fail(__FILE__, __LINE__)

#define TEST_FAIL_AT(file, line) \
    test_fail((file), (line))

#define TEST_SKIP(reason) \
    TEST_HARNESS_RETURN_UNLESS(test_request_skip(__FILE__, __LINE__, (reason)))

#define TEST_REQUIRE_VALIDITY_CHECKER(name) \
    test_require_validity_checker((name), __FILE__, __LINE__)

/* Compatibility aliases for older test files. */
#define TESTS_EXIT_CODE() \
    TEST_EXIT_CODE()

#define RUN_TEST_CASE(fn) \
    TEST_RUN_CASE(fn, NULL)

#define RUN_TEST_CASE_TAGS(fn, tags) \
    TEST_RUN_CASE(fn, tags)

#define RUN_SUBTEST(fn) \
    TEST_RUN_SUBTEST(fn, NULL)

#define RUN_SUBTEST_TAGS(fn, tags) \
    TEST_RUN_SUBTEST(fn, tags)

#define RUN_TEST_IN_GROUP(fn, parent) \
    TEST_RUN_IN_GROUP(fn, parent, NULL)

#define RUN_TEST_IN_GROUP_TAGS(fn, parent, tags) \
    TEST_RUN_IN_GROUP(fn, parent, tags)

#define RUN_OUTPUT(fn) \
    TEST_RUN_OUTPUT(fn)

#define RUN_OUTPUT_TAGS(fn, tags) \
    TEST_RUN_OUTPUT_TAGS(fn, tags)

#define RUN_OUTPUT_IN_GROUP(fn, parent) \
    TEST_RUN_OUTPUT_IN_GROUP(fn, parent)

#define RUN_OUTPUT_IN_GROUP_TAGS(fn, parent, tags) \
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(fn, parent, tags)

#define ASSERT_TRUE(expr) \
    TEST_ASSERT_TRUE((expr), #expr)

#define ASSERT_FALSE(expr) \
    TEST_ASSERT_FALSE((expr), #expr)

#define ASSERT_EQ_INT(actual, expected) \
    TEST_ASSERT_INT_EQ((actual), (expected))

#define ASSERT_EQ_LONG(actual, expected) \
    TEST_ASSERT_LONG_EQ((actual), (expected))

#define ASSERT_EQ_DOUBLE(actual, expected, eps) \
    TEST_ASSERT_DOUBLE_EQ((actual), (expected), (eps))

#define ASSERT_NOT_NULL(ptr) \
    TEST_ASSERT_NOT_NULL((ptr))

#define ASSERT_NULL(ptr) \
    TEST_ASSERT_NULL((ptr))

#endif
