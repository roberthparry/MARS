/**
 * @file test_harness.h
 * @brief Sequential native test registration, assertions, fixtures and result reporting.
 *
 * The sequential core uses only C/POSIX facilities and builds without libmars. Optional adapters provide JSON
 * selection and database reporting. Suites supply mathematical comparisons through opaque callbacks; diagnostic
 * text is copied into a core-owned sink. tests_main remains the suite entry point, called by the core's main.
 */
#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Selection scope interpreted by an optional configuration adapter.
 * @details Without an adapter, all cases run and no configuration is read or written.
 */
typedef enum {
    TEST_CONFIG_NONE, /**< Ignore persistent selections. */
    TEST_CONFIG_GLOBAL, /**< Use the adapter's shared suite configuration. */
    TEST_CONFIG_LOCAL /**< Use the adapter's source-local configuration. */
} test_config_mode_t;

/** @brief Core-owned diagnostic sink; callbacks may append but must not retain or free it. */
typedef struct _test_text_t test_text_t;

/**
 * @brief Copy ordinary C text into a diagnostic sink without calling a module under test.
 * @param[in,out] out Core-owned sink valid only during the formatter callback; never retained or freed by callers.
 * @param[in] text Caller-owned NUL-terminated text, copied synchronously and not consumed.
 * @return Zero on success, -1 on invalid input, size overflow or allocation failure; previous contents survive.
 * @note Sequential use only; the core releases the sink after formatting the assertion.
 */
int test_text_append_cstr(test_text_t *out, const char *text);

/**
 * @brief Append standard printf-formatted diagnostic text.
 * @param[in,out] out Core-owned sink valid only during the formatter callback; not retained or consumed.
 * @param[in] fmt Caller-owned standard printf format; MARS-specific extensions are not supported.
 * @param[in] ... Arguments matching fmt, copied synchronously.
 * @return Zero on success or -1 on invalid input, formatting or allocation failure.
 * @note Sequential use only; the core owns and releases the sink, including its copied bytes.
 */
int test_text_append_format(test_text_t *out, const char *fmt, ...);

/** @brief Execute one selected case synchronously; assertions record failures, and no return value is required. */
typedef void (*test_fn)(void);

/** @brief Prepare a suite; return true on readiness, false on failure. Release owned resources through fixtures/hooks. */
typedef bool (*test_suite_setup_fn)(void);

/** @brief Prepare one case; true permits execution, false reports failure unless an assertion/skip already explains it. */
typedef bool (*test_fixture_setup_fn)(void);

/** @brief Release case resources synchronously; return true on success, false to record a teardown failure. */
typedef bool (*test_fixture_teardown_fn)(void);

/** @brief Run final suite-owned cleanup after reporting; all harness-owned case resources have already been released. */
typedef void (*test_post_summary_fn)(void);

/**
 * @brief Compare two opaque domain values without transferring ownership.
 * @param[in] got Observed caller-owned value, valid throughout the call.
 * @param[in] want Expected caller-owned value, valid throughout the call.
 * @param[in,out] ctx Optional contract-owner context, not released by the harness.
 * @return Non-zero for equal values, zero for a mismatch. The suite defines any domain-specific tolerance.
 * @note Synchronous use only; neither values nor context are retained by the core.
 */
typedef int (*test_validity_equal_fn)(const void *got, const void *want, void *ctx);

/**
 * @brief Describe an opaque value through the independent core's append-only sink.
 * @param[in] value Caller-owned domain value, valid during the call.
 * @param[in,out] out Core-owned diagnostic sink; append using test_text_append_cstr or test_text_append_format.
 * @param[in,out] ctx Optional contract-owner context; not consumed or retained by the core.
 * @return Zero on success, non-zero on failure; the harness then prints a generic pointer description.
 * @note Do not retain or free out. Domain-specific rendering and any intermediate allocations belong to the callback.
 */
typedef int (*test_validity_format_fn)(const void *value, test_text_t *out, void *ctx);

/** @brief Caller-owned domain comparison contract; registered contracts and their context must outlive suite execution. */
typedef struct _test_validity_contract_t {
    const char *name; /**< Caller-owned diagnostic label, valid while the contract is registered. */
    test_validity_equal_fn equal; /**< Required semantic comparison callback. */
    test_validity_format_fn format; /**< Optional domain formatter; NULL uses a pointer description. */
    void *ctx; /**< Optional caller-owned callback context, never released by the core. */
} test_validity_contract_t;

/**
 * @brief Obtain the built-in int comparison contract.
 * @return Immutable process-lifetime contract owned by the core; never free it. Cannot fail.
 */
const test_validity_contract_t *test_validity_contract_int(void);
/**
 * @brief Obtain the built-in long comparison contract.
 * @return Immutable process-lifetime contract owned by the core; never free it. Cannot fail.
 */
const test_validity_contract_t *test_validity_contract_long(void);
/**
 * @brief Obtain the built-in cstr comparison contract.
 * @return Immutable process-lifetime contract owned by the core; never free it. Cannot fail.
 */
const test_validity_contract_t *test_validity_contract_cstr(void);

/** @brief Immutable configuration mode defined once by the suite's primary source. */
extern const test_config_mode_t test_suite_mode;

/** @brief Static, caller-independent primary suite source, supplied by TEST_SUITE_CONFIG for module planning. */
extern const char *const test_suite_source;

/**
 * @brief Report whether this suite may load shared database selections and case data before execution.
 * @details The weak default returns true. Override to return false in SQLite and test-storage suites,
 * so shared storage cannot control their selection or initialise engine state before their cases run.
 * Those suites use JSON selections and isolated case data. Results may still be saved after execution;
 * use --test-database off to disable reporting as well. Explicit fixture operations are unaffected.
 * @return True permits shared selections and case data; false requires isolated selection and fixtures.
 */
bool test_suite_database_selection_enabled(void);

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
 *    for want-value construction, labelling, or other domain-specific
 *    setup, not parallel assertion subsystems.
 */

/**
 * @brief Define the suite's configuration mode and primary source for progress planning.
 * @param mode Constant test_config_mode_t selection, normally TEST_CONFIG_GLOBAL.
 * @details Invoke once at file scope in the suite's primary translation unit. Both definitions have static
 * storage duration and need no release; the primary source identifies helper files belonging to the same module.
 */
#define TEST_SUITE_CONFIG(mode) \
    const test_config_mode_t test_suite_mode = (mode); \
    const char *const test_suite_source = __FILE__

#define TEST_SUITE_SETUP(fn) test_suite_setup_fn test_suite_setup_hook = (fn)

#define TEST_FIXTURE_SETUP(fn) test_fixture_setup_fn test_fixture_setup_hook = (fn)

#define TEST_FIXTURE_TEARDOWN(fn) test_fixture_teardown_fn test_fixture_teardown_hook = (fn)

#define TEST_POST_SUMMARY(fn) test_post_summary_fn test_post_summary_hook = (fn)

#define C_GREEN "\x1b[32m"
#define C_RED "\x1b[31m"
#define C_YELLOW "\x1b[33m"
#define C_CYAN "\x1b[36m"
#define C_RESET "\x1b[0m"
#define C_BOLD "\x1b[1m"
#define C_DIM "\x1b[2m"
#define C_WHITE "\x1b[97m"
#define C_GREY "\x1b[90m"
#define C_MAGENTA "\x1b[95m"

/**
 * @brief Optional executable-owned dispatch before the shared harness parses test-selection arguments.
 * @return No value. The default implementation does nothing; a suite may supply one strong definition.
 * @details A suite that also serves native subprocess fixtures can inspect its private startup state and exit
 * after handling a fixture command. Returning continues ordinary argument parsing, setup and test reporting.
 * No test database has been opened at this point. Run only single-threaded startup work here; fixture handlers
 * own and release any resources they acquire. Ordinary test registration remains in tests_main.
 */
void test_program_setup(void);

/**
 * @brief Print a labelled section heading in test output.
 * @param[in] title Caller-owned NUL-terminated heading, copied during this call.
 * @return No value; failures are recorded or reported by the sequential harness.
 * @note Synchronous, sequential use only. Caller-owned inputs are not retained unless explicitly documented.
 */
void test_section(const char *title);

/**
 * @brief Run one top-level test case if enabled by the active test config.
 * @param[in] file Source identity, copied for reporting; caller retains ownership.
 * @param[in] line Registration or assertion source line.
 * @param[in] name Caller-owned stable case or checker name.
 * @param[in] tags Optional caller-owned comma-separated tags.
 * @param[in] fn Caller-owned synchronous callback, invoked only when selected.
 * @return No value; failures are recorded or reported by the sequential harness.
 * @note Synchronous, sequential use only. Caller-owned inputs are not retained unless explicitly documented.
 */
void test_run_case(const char *file, int line, const char *name, const char *tags, test_fn fn);

/**
 * @brief Run one nested test case under the currently active group path.
 * @param[in] file Source identity, copied for reporting; caller retains ownership.
 * @param[in] line Registration or assertion source line.
 * @param[in] name Caller-owned stable case or checker name.
 * @param[in] tags Optional caller-owned comma-separated tags.
 * @param[in] fn Caller-owned synchronous callback, invoked only when selected.
 * @return No value; failures are recorded or reported by the sequential harness.
 * @note Synchronous, sequential use only. Caller-owned inputs are not retained unless explicitly documented.
 */
void test_run_subtest(const char *file, int line, const char *name, const char *tags, test_fn fn);

/**
 * @brief Run one test case beneath an explicit parent group.
 * @param[in] file Source identity, copied for reporting; caller retains ownership.
 * @param[in] line Registration or assertion source line.
 * @param[in] name Caller-owned stable case or checker name.
 * @param[in] parent Optional caller-owned dotted parent path; NULL selects the top level.
 * @param[in] tags Optional caller-owned comma-separated tags.
 * @param[in] fn Caller-owned synchronous callback, invoked only when selected.
 * @return No value; failures are recorded or reported by the sequential harness.
 * @note Synchronous, sequential use only. Caller-owned inputs are not retained unless explicitly documented.
 */
void test_run_in_group(const char *file, int line, const char *name, const char *parent, const char *tags, test_fn fn);

/**
 * @brief Run a named data row through a synchronous module-owned checking callback.
 * @param[in] file Borrowed source-file identity, normally __FILE__.
 * @param[in] line Declaration line, normally __LINE__.
 * @param[in] name Borrowed stable row name used in selection and reporting.
 * @param[in] parent Borrowed containing group name, or NULL for a top-level row.
 * @param[in] tags Borrowed comma-separated tags, or NULL.
 * @param[in] data Borrowed case data valid throughout the call; not consumed.
 * @param[in] check Borrowed callback invoked when selected, using the ordinary fixtures and assertions.
 * @return No value. Failures and skips use the normal harness accounting. Nested data callbacks restore prior context.
 * @note The caller retains data ownership; this sequential interface is not safe for concurrent use.
 */
void test_run_data_case(const char *file, int line, const char *name, const char *parent, const char *tags,
                         const void *data, void (*check)(const void *data));

/**
 * @brief Load enabled module-table rows, close storage and register each as a separate native case.
 * @param[in] file Borrowed source identity for the module runner.
 * @param[in] line Runner registration line.
 * @param[in] module Borrowed registry key selecting a module case table and optional operation group.
 * @param[in] parent Borrowed parent group identifying this runner; NULL uses the currently executing case path.
 * @param[in] check Borrowed checking callback receiving one test_database_case_t as opaque data.
 * @return No value. Storage errors fail the containing group; an empty enabled table skips it.
 * @note With automatic storage disabled, rows come from a temporary in-memory schema; no test database file is created.
 */
void test_run_database_cases(const char *file, int line, const char *module, const char *parent,
                              void (*check)(const void *data));

/**
 * @brief Run an output/demo case, counted separately from correctness cases.
 * @param[in] file Source identity, copied for reporting; caller retains ownership.
 * @param[in] line Registration or assertion source line.
 * @param[in] tags Optional caller-owned comma-separated tags.
 * @param[in] name Caller-owned stable case or checker name.
 * @param[in] fn Caller-owned synchronous callback, invoked only when selected.
 * @return No value; failures are recorded or reported by the sequential harness.
 * @note Synchronous, sequential use only. Caller-owned inputs are not retained unless explicitly documented.
 */
void test_run_output_case(const char *file, int line, const char *tags, const char *name, test_fn fn);

/**
 * @brief Run an output/demo case beneath an explicit parent group.
 * @param[in] file Source identity, copied for reporting; caller retains ownership.
 * @param[in] line Registration or assertion source line.
 * @param[in] name Caller-owned stable case or checker name.
 * @param[in] parent Optional caller-owned dotted parent path; NULL selects the top level.
 * @param[in] tags Optional caller-owned comma-separated tags.
 * @param[in] fn Caller-owned synchronous callback, invoked only when selected.
 * @return No value; failures are recorded or reported by the sequential harness.
 * @note Synchronous, sequential use only. Caller-owned inputs are not retained unless explicitly documented.
 */
void test_run_output_in_group(const char *file, int line, const char *name, const char *parent, const char *tags,
                              test_fn fn);

/**
 * @brief Return the process exit code implied by the recorded test results.
 * @return Zero if no assertion/setup failure was recorded, otherwise one.
 * @note Synchronous, sequential use only. Caller-owned inputs are not retained unless explicitly documented.
 */
int test_exit_code(void);

/**
 * @brief Record a failure unless @p expr is true.
 * @param[in] expr Boolean condition to check.
 * @param[in] file Source identity, copied for reporting; caller retains ownership.
 * @param[in] line Registration or assertion source line.
 * @param[in] detail Optional caller-owned explanation, copied when recorded.
 * @return True on success or when the condition holds, otherwise false. Assertion failures are recorded.
 * @note Synchronous, sequential use only. Caller-owned inputs are not retained unless explicitly documented.
 */
bool test_assert_true(bool expr, const char *file, int line, const char *detail);

/**
 * @brief Record a failure unless @p expr is false.
 * @param[in] expr Boolean condition to check.
 * @param[in] file Source identity, copied for reporting; caller retains ownership.
 * @param[in] line Registration or assertion source line.
 * @param[in] detail Optional caller-owned explanation, copied when recorded.
 * @return True on success or when the condition holds, otherwise false. Assertion failures are recorded.
 * @note Synchronous, sequential use only. Caller-owned inputs are not retained unless explicitly documented.
 */
bool test_assert_false(bool expr, const char *file, int line, const char *detail);

/**
 * @brief Record a failure unless two int values are equal.
 * @param[in] got Observed value; not consumed.
 * @param[in] want Expected value; not consumed.
 * @param[in] file Source identity, copied for reporting; caller retains ownership.
 * @param[in] line Registration or assertion source line.
 * @return True on success or when the condition holds, otherwise false. Assertion failures are recorded.
 * @note Synchronous, sequential use only. Caller-owned inputs are not retained unless explicitly documented.
 */
bool test_assert_int_eq(int got, int want, const char *file, int line);

/**
 * @brief Record a failure unless two long values are equal.
 * @param[in] got Observed value; not consumed.
 * @param[in] want Expected value; not consumed.
 * @param[in] file Source identity, copied for reporting; caller retains ownership.
 * @param[in] line Registration or assertion source line.
 * @return True on success or when the condition holds, otherwise false. Assertion failures are recorded.
 * @note Synchronous, sequential use only. Caller-owned inputs are not retained unless explicitly documented.
 */
bool test_assert_long_eq(long got, long want, const char *file, int line);

/**
 * @brief Record a failure unless two double values differ by no more than @p eps.
 * @param[in] got Observed value; not consumed.
 * @param[in] want Expected value; not consumed.
 * @param[in] eps Non-negative absolute error tolerance.
 * @param[in] file Source identity, copied for reporting; caller retains ownership.
 * @param[in] line Registration or assertion source line.
 * @return True on success or when the condition holds, otherwise false. Assertion failures are recorded.
 * @note Synchronous, sequential use only. Caller-owned inputs are not retained unless explicitly documented.
 */
bool test_assert_double_eq(double got, double want, double eps, const char *file, int line);

/**
 * @brief Record a failure unless @p ptr is non-null.
 * @param[in] ptr Pointer to check without dereferencing or retaining it.
 * @param[in] file Source identity, copied for reporting; caller retains ownership.
 * @param[in] line Registration or assertion source line.
 * @return True on success or when the condition holds, otherwise false. Assertion failures are recorded.
 * @note Synchronous, sequential use only. Caller-owned inputs are not retained unless explicitly documented.
 */
bool test_assert_not_null(const void *ptr, const char *file, int line);

/**
 * @brief Record a failure unless @p ptr is null.
 * @param[in] ptr Pointer to check without dereferencing or retaining it.
 * @param[in] file Source identity, copied for reporting; caller retains ownership.
 * @param[in] line Registration or assertion source line.
 * @return True on success or when the condition holds, otherwise false. Assertion failures are recorded.
 * @note Synchronous, sequential use only. Caller-owned inputs are not retained unless explicitly documented.
 */
bool test_assert_null(const void *ptr, const char *file, int line);

/**
 * @brief Record a failure unless @p got and @p want satisfy @p contract.
 * @param[in] contract Caller-owned comparison contract, valid during this call.
 * @param[in] got Observed value; not consumed.
 * @param[in] want Expected value; not consumed.
 * @param[in] file Source identity, copied for reporting; caller retains ownership.
 * @param[in] line Registration or assertion source line.
 * @return True on success or when the condition holds, otherwise false. Assertion failures are recorded.
 * @note Synchronous, sequential use only. Caller-owned inputs are not retained unless explicitly documented.
 */
bool test_assert_validity(const test_validity_contract_t *contract, const void *got, const void *want,
                          const char *file, int line);

/**
 * @brief Record a validity failure using a checker registered by name.
 * @param[in] name Caller-owned stable case or checker name.
 * @param[in] got Observed value; not consumed.
 * @param[in] want Expected value; not consumed.
 * @param[in] file Source identity, copied for reporting; caller retains ownership.
 * @param[in] line Registration or assertion source line.
 * @return True on success or when the condition holds, otherwise false. Assertion failures are recorded.
 * @note Synchronous, sequential use only. Caller-owned inputs are not retained unless explicitly documented.
 */
bool test_assert_validity_named(const char *name, const void *got, const void *want, const char *file, int line);

/**
 * @brief Record a failure unless two C-string boundary values are equal.
 * @param[in] got Observed value; not consumed.
 * @param[in] want Expected value; not consumed.
 * @param[in] file Source identity, copied for reporting; caller retains ownership.
 * @param[in] line Registration or assertion source line.
 * @return True on success or when the condition holds, otherwise false. Assertion failures are recorded.
 * @note Synchronous, sequential use only. Caller-owned inputs are not retained unless explicitly documented.
 */
bool test_assert_cstr_eq(const char *got, const char *want, const char *file, int line);

/**
 * @brief Register a named semantic validity checker for later assertions.
 * @param[in] name Caller-owned name, copied into the registry; duplicate registration replaces the contract.
 * @param[in] contract Caller-owned contract and context; both must remain valid until the suite finishes.
 * @return No value; invalid inputs or allocation failure leave the registry unchanged.
 * @note Sequential use only. The registry copies the name and retains the contract pointer; it never frees the contract.
 */
void test_register_validity_checker(const char *name, const test_validity_contract_t *contract);

/**
 * @brief Find a previously registered semantic validity checker.
 * @param[in] name Caller-owned stable case or checker name.
 * @return Caller-owned registered contract valid until its owner releases it, or NULL when absent.
 * @note Synchronous, sequential use only. Caller-owned inputs are not retained unless explicitly documented.
 */
const test_validity_contract_t *test_find_validity_checker(const char *name);

/**
 * @brief Require that a named validity checker exists before a suite runs.
 * @param[in] name Caller-owned stable case or checker name.
 * @param[in] file Source identity, copied for reporting; caller retains ownership.
 * @param[in] line Registration or assertion source line.
 * @return True on success or when the condition holds, otherwise false. Assertion failures are recorded.
 * @note Synchronous, sequential use only. Caller-owned inputs are not retained unless explicitly documented.
 */
bool test_require_validity_checker(const char *name, const char *file, int line);

/**
 * @brief Query whether a case is enabled by the active test config.
 * @param[in] file Source identity, copied for reporting; caller retains ownership.
 * @param[in] func Caller-owned local case name.
 * @param[in] parent Optional caller-owned dotted parent path; NULL selects the top level.
 * @return Effective enabled state, defaulting to true when no optional configuration adapter is linked.
 * @note Synchronous, sequential use only. Caller-owned inputs are not retained unless explicitly documented.
 */
bool test_harness_config_is_enabled(const char *file, const char *func, const char *parent);

/**
 * @brief Query whether a case has an explicit key in the active test config.
 * @param[in] file Source identity, copied for reporting; caller retains ownership.
 * @param[in] func Caller-owned local case name.
 * @param[in] parent Optional caller-owned dotted parent path; NULL selects the top level.
 * @return Adapter-reported key presence. Without an adapter returns true to suppress configuration-discovery warnings.
 * @note Synchronous, sequential use only. Caller-owned inputs are not retained unless explicitly documented.
 */
bool test_harness_config_has_key(const char *file, const char *func, const char *parent);

/**
 * @brief Store formatted failure detail for the current case.
 * @param[in] fmt Caller-owned standard printf format; MARS-specific extensions are unsupported.
 * @param[in] ... Arguments matching fmt; copied synchronously.
 * @return No value; failures are recorded or reported by the sequential harness.
 * @note Synchronous, sequential use only. Caller-owned inputs are not retained unless explicitly documented.
 */
void test_set_failure_detailf(const char *fmt, ...);

/**
 * @brief Clear any recorded failure or skip detail for the current case.
 * @return No value; failures are recorded or reported by the sequential harness.
 * @note Synchronous, sequential use only. Caller-owned inputs are not retained unless explicitly documented.
 */
void test_clear_failure_detail(void);

/**
 * @brief Mark the current case as failed at @p file:@p line.
 * @param[in] file Source identity, copied for reporting; caller retains ownership.
 * @param[in] line Registration or assertion source line.
 * @param[in] detail Optional caller-owned explanation, copied when recorded.
 * @return No value; failures are recorded or reported by the sequential harness.
 * @note Synchronous, sequential use only. Caller-owned inputs are not retained unless explicitly documented.
 */
void test_mark_failure(const char *file, int line, const char *detail);

/**
 * @brief Mark the current case as explicitly failed at @p file:@p line.
 * @param[in] file Source identity, copied for reporting; caller retains ownership.
 * @param[in] line Registration or assertion source line.
 * @return No value; failures are recorded or reported by the sequential harness.
 * @note Synchronous, sequential use only. Caller-owned inputs are not retained unless explicitly documented.
 */
void test_fail(const char *file, int line);

/**
 * @brief Mark the current case as skipped at @p file:@p line.
 * @param[in] file Source identity, copied for reporting; caller retains ownership.
 * @param[in] line Registration or assertion source line.
 * @param[in] detail Optional caller-owned explanation, copied when recorded.
 * @return No value; failures are recorded or reported by the sequential harness.
 * @note Synchronous, sequential use only. Caller-owned inputs are not retained unless explicitly documented.
 */
void test_mark_skip(const char *file, int line, const char *detail);

/**
 * @brief Mark the current case as skipped and return false for assertion-style macros.
 * @param[in] file Source identity, copied for reporting; caller retains ownership.
 * @param[in] line Registration or assertion source line.
 * @param[in] detail Optional caller-owned explanation, copied when recorded.
 * @return True on success or when the condition holds, otherwise false. Assertion failures are recorded.
 * @note Synchronous, sequential use only. Caller-owned inputs are not retained unless explicitly documented.
 */
bool test_request_skip(const char *file, int line, const char *detail);

/**
 * @brief Return the temporary directory allocated for the current case.
 * @return Core-owned directory path, valid until case cleanup, or NULL on allocation or filesystem failure.
 * @note Synchronous, sequential use only. Caller-owned inputs are not retained unless explicitly documented.
 */
const char *test_case_temp_dir(void);

/**
 * @brief Return a temporary file path beneath the current case directory.
 * @param[in] leafname Optional caller-owned file basename; NULL or empty uses tmp.
 * @return Core-owned path, valid until case cleanup, or NULL on allocation or filesystem failure; no file is created.
 * @note Synchronous, sequential use only. Caller-owned inputs are not retained unless explicitly documented.
 */
const char *test_case_temp_path(const char *leafname);

/**
 * @brief Redirect stdout to a temporary capture file for the current case.
 * @param[in] leafname Optional caller-owned file basename; NULL or empty uses tmp.
 * @param[out] path_out Optional output receiving a core-owned path, valid until case cleanup; do not free it.
 * @return Owned saved descriptor on success, or -1 on failure; pass it to test_case_end_stdout_capture.
 * @note Synchronous, sequential use only. Caller-owned inputs are not retained unless explicitly documented.
 */
int test_case_begin_stdout_capture(const char *leafname, const char **path_out);

/**
 * @brief Restore stdout after a capture started by test_case_begin_stdout_capture().
 * @param[in] saved_stdout Original descriptor returned by test_case_begin_stdout_capture; consumed and closed.
 * @return True on success or when the condition holds, otherwise false. Assertion failures are recorded.
 * @note Synchronous, sequential use only. Caller-owned inputs are not retained unless explicitly documented.
 */
bool test_case_end_stdout_capture(int saved_stdout);

/**
 * @brief Redirect stderr to a temporary capture file for the current case.
 * @param[in] leafname Optional caller-owned file basename; NULL or empty uses tmp.
 * @param[out] path_out Optional output receiving a core-owned path, valid until case cleanup; do not free it.
 * @return Owned saved descriptor on success, or -1 on failure; pass it to test_case_end_stderr_capture.
 * @note Synchronous, sequential use only. Caller-owned inputs are not retained unless explicitly documented.
 */
int test_case_begin_stderr_capture(const char *leafname, const char **path_out);

/**
 * @brief Restore stderr after a capture started by test_case_begin_stderr_capture().
 * @param[in] saved_stderr Original descriptor returned by test_case_begin_stderr_capture; consumed and closed.
 * @return True on success or when the condition holds, otherwise false. Assertion failures are recorded.
 * @note Synchronous, sequential use only. Caller-owned inputs are not retained unless explicitly documented.
 */
bool test_case_end_stderr_capture(int saved_stderr);

/**
 * @brief Run the registered test suite.
 * @return Suite-supplied status; zero defers to assertion results, non-zero becomes the executable's exit status.
 * @note Synchronous, sequential use only. Caller-owned inputs are not retained unless explicitly documented.
 */
int tests_main(void);

#define TEST_SECTION(title) test_section((title))

#define TEST_ENABLED(parent) test_harness_config_is_enabled(__FILE__, __func__, (parent))

#define TEST_CONFIG_HAS_KEY(parent) test_harness_config_has_key(__FILE__, __func__, (parent))

#define TEST_RUN_CASE(fn, tags) test_run_case(__FILE__, __LINE__, #fn, (tags), (fn))

#define TEST_RUN_SUBTEST(fn, tags) test_run_subtest(__FILE__, __LINE__, #fn, (tags), (fn))

#define TEST_RUN_IN_GROUP(fn, parent, tags) test_run_in_group(__FILE__, __LINE__, #fn, #parent, (tags), (fn))

#define TEST_RUN_OUTPUT(fn) test_run_output_case(__FILE__, __LINE__, NULL, #fn, (fn))

#define TEST_RUN_OUTPUT_TAGS(fn, tags) test_run_output_case(__FILE__, __LINE__, (tags), #fn, (fn))

#define TEST_RUN_OUTPUT_IN_GROUP(fn, parent) test_run_output_in_group(__FILE__, __LINE__, #fn, #parent, NULL, (fn))

#define TEST_RUN_OUTPUT_IN_GROUP_TAGS(fn, parent, tags)                                                                \
    test_run_output_in_group(__FILE__, __LINE__, #fn, #parent, (tags), (fn))

#define TEST_EXIT_CODE() test_exit_code()

#define TEST_HARNESS_RETURN_UNLESS(ok_expr)                                                                            \
    do {                                                                                                               \
        if (!(ok_expr))                                                                                                \
            return;                                                                                                    \
    } while (0)

#define TEST_ASSERT_TRUE(expr, detail)                                                                                 \
    TEST_HARNESS_RETURN_UNLESS(test_assert_true((expr), __FILE__, __LINE__, (detail)))

#define TEST_ASSERT_FALSE(expr, detail)                                                                                \
    TEST_HARNESS_RETURN_UNLESS(test_assert_false((expr), __FILE__, __LINE__, (detail)))

#define TEST_ASSERT_INT_EQ(got, want)                                                                           \
    TEST_HARNESS_RETURN_UNLESS(test_assert_int_eq((got), (want), __FILE__, __LINE__))

#define TEST_ASSERT_LONG_EQ(got, want)                                                                          \
    TEST_HARNESS_RETURN_UNLESS(test_assert_long_eq((got), (want), __FILE__, __LINE__))

#define TEST_ASSERT_DOUBLE_EQ(got, want, eps)                                                                   \
    TEST_HARNESS_RETURN_UNLESS(test_assert_double_eq((got), (want), (eps), __FILE__, __LINE__))

#define TEST_ASSERT_NOT_NULL(ptr) TEST_HARNESS_RETURN_UNLESS(test_assert_not_null((ptr), __FILE__, __LINE__))

#define TEST_ASSERT_NULL(ptr) TEST_HARNESS_RETURN_UNLESS(test_assert_null((ptr), __FILE__, __LINE__))

#define TEST_VALIDITY_CONTRACT(name, equal_fn, format_fn, ctx_ptr)                                                     \
    ((test_validity_contract_t){(name), (equal_fn), (format_fn), (ctx_ptr)})

#define TEST_VALID_INT() test_validity_contract_int()

#define TEST_VALID_LONG() test_validity_contract_long()

#define TEST_VALID_CSTR() test_validity_contract_cstr()

#define TEST_ASSERT_VALID(contract_ptr, got_ptr, want_ptr)                                                      \
    TEST_HARNESS_RETURN_UNLESS(test_assert_validity((contract_ptr), (got_ptr), (want_ptr), __FILE__, __LINE__))

#define TEST_ASSERT_VALID_NAMED(name, got_ptr, want_ptr)                                                        \
    TEST_HARNESS_RETURN_UNLESS(test_assert_validity_named((name), (got_ptr), (want_ptr), __FILE__, __LINE__))

#define TEST_ASSERT_STR_EQ(got_cstr, want_cstr)                                                                 \
    TEST_HARNESS_RETURN_UNLESS(test_assert_cstr_eq((got_cstr), (want_cstr), __FILE__, __LINE__))

#define TEST_FAIL() test_fail(__FILE__, __LINE__)

#define TEST_FAIL_AT(file, line) test_fail((file), (line))

#define TEST_SKIP(reason) TEST_HARNESS_RETURN_UNLESS(test_request_skip(__FILE__, __LINE__, (reason)))

#define TEST_REQUIRE_VALIDITY_CHECKER(name) test_require_validity_checker((name), __FILE__, __LINE__)

/* Compatibility aliases for older test files. */
#define TESTS_EXIT_CODE() TEST_EXIT_CODE()

#define RUN_TEST_CASE(fn) TEST_RUN_CASE(fn, NULL)

#define RUN_TEST_CASE_TAGS(fn, tags) TEST_RUN_CASE(fn, tags)

#define RUN_SUBTEST(fn) TEST_RUN_SUBTEST(fn, NULL)

#define RUN_SUBTEST_TAGS(fn, tags) TEST_RUN_SUBTEST(fn, tags)

#define RUN_TEST_IN_GROUP(fn, parent) TEST_RUN_IN_GROUP(fn, parent, NULL)

#define RUN_TEST_IN_GROUP_TAGS(fn, parent, tags) TEST_RUN_IN_GROUP(fn, parent, tags)

#define RUN_OUTPUT(fn) TEST_RUN_OUTPUT(fn)

#define RUN_OUTPUT_TAGS(fn, tags) TEST_RUN_OUTPUT_TAGS(fn, tags)

#define RUN_OUTPUT_IN_GROUP(fn, parent) TEST_RUN_OUTPUT_IN_GROUP(fn, parent)

#define RUN_OUTPUT_IN_GROUP_TAGS(fn, parent, tags) TEST_RUN_OUTPUT_IN_GROUP_TAGS(fn, parent, tags)

#define ASSERT_TRUE(expr) TEST_ASSERT_TRUE((expr), #expr)

#define ASSERT_FALSE(expr) TEST_ASSERT_FALSE((expr), #expr)

#define ASSERT_EQ_INT(got, want) TEST_ASSERT_INT_EQ((got), (want))

#define ASSERT_EQ_LONG(got, want) TEST_ASSERT_LONG_EQ((got), (want))

#define ASSERT_EQ_DOUBLE(got, want, eps) TEST_ASSERT_DOUBLE_EQ((got), (want), (eps))

#define ASSERT_NOT_NULL(ptr) TEST_ASSERT_NOT_NULL((ptr))

#define ASSERT_NULL(ptr) TEST_ASSERT_NULL((ptr))

#endif
