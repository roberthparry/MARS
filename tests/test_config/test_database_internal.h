/**
 * @file test_database_internal.h
 * @brief Private raw-SQLite storage boundary for test management, profiles and reporting.
 *
 * Consumed only by tests/test_config/test_database*.c. Authorised fixtures use the narrower tests/internal
 * boundary; ordinary test suites and repository commands use test_database.h.
 * This code deliberately never includes sqlite-t.h or calls the MARS sqlite module. A connection is owned by
 * its opaque test-storage object and must be closed before invoking any case under test.
 */
#ifndef MARS_TEST_DATABASE_INTERNAL_H
#define MARS_TEST_DATABASE_INTERNAL_H

#if !defined(MARS_TEST_DATABASE_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_TEST_DATABASE_INTERNAL_ACCESS."
#endif

#include "test_database.h"
#define MARS_SHARED_TEST_DATABASE_INTERNAL_ACCESS
#include "internal/test_database_internal.h"

/** @brief Owned storage state, accessed only by the test-management implementation. */
struct _test_database_t {
    sqlite3 *connection; /**< Owned raw connection, closed on release. */
    string_t *error; /**< Owned latest diagnostic, or NULL. */
    sqlite3_int64 run_id; /**< Current transactional report identifier, or zero. */
    size_t ordinal; /**< Next per-run result index. */
    bool importing; /**< Whether catalogue updates must also record identities seen in this import. */
    bool memory_test; /**< Whether the current report is excluded from ordinary timing statistics. */
};

/** @brief Robust timing comparison made against prior accepted ordinary runs. */
typedef struct _test_timing_assessment_t {
    bool available; /**< Whether the configured minimum number of baseline samples exists. */
    bool outlier; /**< Whether this duration exceeds the robust, relative and absolute thresholds. */
    double median_ms; /**< Baseline median at the time of assessment; meaningful only when available. */
    double threshold_ms; /**< Slowdown threshold used for this result; meaningful only when available. */
} test_timing_assessment_t;

/**
 * @brief Store the latest diagnostic on a borrowed connection.
 * @param[in,out] database Borrowed connection; NULL is ignored.
 * @param[in] text Borrowed diagnostic copied before return; NULL uses SQLite's current error.
 * @return False, allowing direct propagation of an operation failure.
 */
bool test_database_fail(test_database_t *database, const char *text);

/**
 * @brief Merge the schema and automatically discovered per-module SQL files inside the caller's transaction.
 * @param[in,out] database Borrowed writable connection; caller commits or rolls back the transaction.
 * @param[in] root Borrowed repository root locating tests/test_database.sql; NULL means the current directory.
 * @return True on success; false on file, schema or SQL failure. Existing case definitions are never replaced.
 */
bool test_database_seed(test_database_t *database, const string_t *root);

/**
 * @brief Step an owned write statement once and finalise it on every path.
 * @param[in,out] database Borrowed connection.
 * @param[in,out] statement Owned statement consumed by the function; NULL returns false.
 * @return True for SQLITE_DONE, or false with a retained diagnostic.
 */
bool test_database_write(test_database_t *database, sqlite3_stmt *statement);

/**
 * @brief Insert or refresh one catalogue entry with bound parameter values.
 * @param[in,out] database Borrowed connection; any current transaction is retained.
 * @param[in] result Borrowed metadata; outcome and elapsed fields are ignored.
 * @return True on success, or false with a retained diagnostic.
 */
bool test_database_catalogue(test_database_t *database, const test_database_result_t *result);

/**
 * @brief Import module-row identities using the reviewable case-runner mapping.
 * @param[in,out] database Borrowed connection in an active catalogue-import transaction.
 * @return True on success; false on invalid module mappings or SQLite failure. No callback or test is executed.
 */
bool test_database_import_cases(test_database_t *database);

/**
 * @brief Add one executed case to lifetime statistics inside the caller's report transaction.
 * @param[in,out] database Borrowed writable connection in a transaction.
 * @param[in] result Borrowed completed result with a non-NULL UTC completion timestamp.
 * @param[in] memory_test True excludes this duration from ordinary statistics and records a memory-test date.
 * @param[in] timing_outlier True retains the execution but excludes its duration from the clean timing baseline.
 * @return True on success or for a skipped case; false with a retained diagnostic on SQL failure.
 */
bool test_database_history_record(test_database_t *database, const test_database_result_t *result, bool memory_test,
                                  bool timing_outlier);

/**
 * @brief Assess a duration against the prior accepted median and median absolute deviation.
 * @param[in,out] database Borrowed connection in the report transaction; assessment does not change samples.
 * @param[in] result Borrowed completed record being assessed.
 * @param[in] memory_test True bypasses ordinary timing classification.
 * @param[out] assessment Required destination, cleared before assessment. Insufficient history is not an error.
 * @return True on success, including insufficient history; false with a retained SQL diagnostic on failure.
 */
bool test_database_history_assess(test_database_t *database, const test_database_result_t *result, bool memory_test,
                                  test_timing_assessment_t *assessment);

/**
 * @brief Print suspected timing regressions after their report has committed.
 * @param[in,out] database Borrowed connection used only for reading the committed results.
 * @param[in] run_id Identifier of the completed report.
 * @return No value; diagnostic output never changes the completed test outcome or stored report.
 */
void test_database_history_warn(test_database_t *database, sqlite3_int64 run_id);

/**
 * @brief Upgrade old report columns and populate lifetime statistics exactly once.
 * @param[in,out] database Borrowed connection inside a schema-import transaction.
 * @return True on success; false on schema or history replay failure. The caller rolls back on failure.
 */
bool test_database_history_upgrade(test_database_t *database);

/**
 * @brief Consolidate a validated legacy report database into the default test database.
 * @param[in,out] database Borrowed default connection, with no active transaction.
 * @param[in] root Borrowed repository root; NULL selects the current directory.
 * @return True if no legacy file exists or migration succeeds; false leaves the source file available for retry.
 * @note Reports are fingerprinted for idempotence. The source is removed only after a successful commit.
 */
bool test_database_reports_migrate(test_database_t *database, const string_t *root);

#endif /* MARS_TEST_DATABASE_INTERNAL_H */
