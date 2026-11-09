/**
 * @file test_database.h
 * @brief Test-only catalogue, selection profiles, bounded run history and disposable SQLite fixtures.
 *
 * Compiled test functions remain in C. The versioned tests/tests.db holds cases, catalogue, shared profiles,
 * lifetime statistics and bounded detailed reports. It survives make clean. The SQLite C API is used independently
 * of the MARS sqlite module being tested. Nothing opens
 * a database merely by linking this support. Normal builds, installation and applications never initialise it.
 * Callers run sequentially; close all management handles before running test cases. SQLite and storage suites use
 * isolated selection and fixtures; optional reports are saved afterwards without affecting their console outcome.
 */
#ifndef MARS_TEST_DATABASE_H
#define MARS_TEST_DATABASE_H

#include <stdbool.h>
#include <stddef.h>

#include "string-t.h"

/** @brief Opaque owned test-storage connection; never the connection under test. */
typedef struct _test_database_t test_database_t;

/** @brief Opaque owned, indexed profile snapshot with no live SQLite connection. */
typedef struct _test_profile_t test_profile_t;

/** @brief Owned case data loaded from one module-specific table; release the complete array together. */
typedef struct _test_database_case_t {
    string_t *name; /**< Owned stable case identifier. */
    string_t *operation; /**< Owned identifier of the checking operation supplied by the C module runner. */
    string_t *input; /**< Owned native mathematical input or fixture SQL. */
    string_t *expected; /**< Owned expected value or module-defined assertion data. */
    unsigned precision; /**< Requested decimal precision, validated between 1 and 10000. */
} test_database_case_t;

/**
 * @brief Load enabled case rows and close the storage connection before returning.
 * @param[in] root Borrowed repository root; NULL selects the current directory.
 * @param[in] path Optional borrowed storage pathname; NULL selects tests/tests.db, :memory: uses a disposable schema.
 * @param[in] module Borrowed registry key identifying a module table and optional operation group.
 * @param[out] count Required destination for the number of returned cases; initially set to zero.
 * @param[out] error Optional owned failure diagnostic released with string_free().
 * @return Owned case array released with test_database_cases_free(), including a non-NULL empty array;
 * NULL on invalid module names, storage errors or allocation failure.
 * @note Tables contain inputs and expectations, never compiled C or executable callback addresses.
 */
test_database_case_t *test_database_cases_load(const string_t *root, const string_t *path, const char *module,
                                               size_t *count, string_t **error);

/**
 * @brief Release a complete loaded case array and its owned text; NULL is accepted.
 * @param[in,out] cases Owned array returned by test_database_cases_load(), invalid after return.
 * @param[in] count Number of populated entries returned by that function.
 */
void test_database_cases_free(test_database_case_t *cases, size_t count);

/** @brief Borrowed metadata for one completed, failed or skipped harness case. */
typedef struct _test_database_result_t {
    const char *source; /**< Repository-relative test source; valid during the call. */
    const char *path; /**< Complete dotted group and case path. */
    const char *name; /**< Registered C function or README identifier. */
    const char *parent; /**< Dotted parent group, or an empty string. */
    const char *tags; /**< Authored comma-separated tags, or an empty string. */
    const char *detail; /**< Failure or skip explanation; storage retains at most 4096 characters. */
    const char *outcome; /**< One of pass, fail or skip. */
    int declaration_line; /**< Declaration line, or zero when imported from configuration. */
    bool enabled; /**< Whether selection allowed execution. */
    bool is_group; /**< Whether this record represents a containing group. */
    bool is_output; /**< Whether this is a README/output example. */
    double elapsed_ms; /**< Non-negative elapsed milliseconds. */
    const char *finished_utc; /**< Optional ISO 8601 UTC completion timestamp; NULL records the current time. */
} test_database_result_t;

/**
 * @brief Open validated test storage, initialising its schema only when the file is empty.
 * @param[in] root Borrowed repository root locating the shared schema and module SQL sources; NULL means the current directory.
 * @param[in] path Borrowed database pathname; NULL selects root/tests/tests.db.
 * Explicit paths require an existing parent directory; :memory: selects disposable in-memory storage.
 * @param[in] create True permits creation; false requires an existing compatible database.
 * @param[out] error Optional destination for an owned diagnostic released with string_free(); initially set to NULL.
 * @return Owned connection released with test_database_close(), or NULL on I/O, schema or SQLite failure.
 * @note Existing unrelated databases are rejected. Connections have a one-second busy timeout and a 64 MiB page cap.
 * Compatible older schemas are upgraded when create is true. New module case definitions otherwise require explicit sync.
 * Opening the default database for writing imports and removes a validated legacy test_results.db after commit.
 */
test_database_t *test_database_open(const string_t *root, const string_t *path, bool create, string_t **error);

/**
 * @brief Close a connection and release its resources; NULL is accepted.
 * @param[in,out] database Owned connection, invalid after return; unfinished transactions are rolled back.
 */
void test_database_close(test_database_t *database);

/**
 * @brief Borrow the latest connection diagnostic without transferring ownership.
 * @param[in] database Borrowed connection, or NULL.
 * @return Borrowed UTF-8 diagnostic valid until the next operation or close; a static message for NULL.
 */
const char *test_database_error(const test_database_t *database);

/**
 * @brief Import every configured test/group into the catalogue and optionally snapshot its selection.
 * @param[in,out] database Borrowed writable connection.
 * @param[in] root Borrowed repository root containing tests/test_config.json.
 * @param[in] profile Optional profile name to replace atomically, with missing entries enabled; NULL imports metadata only.
 * @param[in] review_missing True marks unseen identities unconfirmed for review, without deleting or disabling tests.
 * @return True on success; false on invalid JSON, allocation or SQLite failure, with no partial import committed.
 * @note No synchronisation deletes catalogue entries, case rows, saved selections or results. An incomplete
 * configuration may leave entries unconfirmed; this status never controls test execution. Renames retain both keys.
 */
bool test_database_import(test_database_t *database, const string_t *root, const char *profile, bool review_missing);

/**
 * @brief Create an empty profile, preserving an existing profile with the same name.
 * @param[in,out] database Borrowed writable connection.
 * @param[in] name Borrowed non-empty profile name of at most 128 bytes.
 * @param[in] default_enabled Selection applied when no rule or selected descendant matches.
 * @return True on success; false for invalid input or SQLite failure.
 */
bool test_database_profile_create(test_database_t *database, const char *name, bool default_enabled);

/**
 * @brief Set an exact test or group rule in an existing profile.
 * @param[in,out] database Borrowed writable connection.
 * @param[in] profile Borrowed profile name.
 * @param[in] source Borrowed repository-relative source path.
 * @param[in] path Borrowed complete dotted case/group path.
 * @param[in] enabled True enables the path; false excludes it and its descendants.
 * @return True on success; false if the profile is absent or the operation fails.
 */
bool test_database_profile_set(test_database_t *database, const char *profile, const char *source,
                               const char *path, bool enabled);

/**
 * @brief Read a profile into indexed in-memory rules without retaining a database connection.
 * @param[in,out] database Borrowed connection; remains caller-owned and may be closed immediately afterwards.
 * @param[in] name Borrowed profile name.
 * @return Owned snapshot released with test_profile_free(), or NULL for an absent/invalid profile or read failure.
 */
test_profile_t *test_database_profile_load(test_database_t *database, const char *name);

/**
 * @brief Release a profile snapshot; NULL is accepted.
 * @param[in,out] profile Owned snapshot, invalid after return.
 */
void test_profile_free(test_profile_t *profile);

/**
 * @brief Resolve indexed case and ancestor rules, allowing traversal to explicitly selected descendants.
 * @param[in] profile Borrowed immutable snapshot.
 * @param[in] source Borrowed repository-relative source path.
 * @param[in] path Borrowed dotted test/group path, with at most 128 components.
 * @return True when enabled; false for exclusions, invalid inputs or allocation failure. Disabled ancestors win.
 */
bool test_profile_enabled(const test_profile_t *profile, const char *source, const char *path);

/**
 * @brief Begin one transactional run report after test execution has finished.
 * @param[in,out] database Borrowed connection; no other transaction may be active.
 * @param[in] executable Borrowed executable identity.
 * @param[in] profile Borrowed selected profile name, or NULL for JSON selection.
 * @param[in] memory_test True marks memory-tool execution, excluded from every ordinary timing statistic.
 * @return True on success; false on failure. Close rolls back an unfinished report.
 */
bool test_database_run_begin(test_database_t *database, const char *executable, const char *profile, bool memory_test);

/**
 * @brief Record a case result and update its catalogue metadata in the current run transaction.
 * @details Significant ordinary slowdowns are retained as timing outliers. Their assertion outcomes are preserved,
 * while their durations are excluded from future baseline means, medians and regression assessments.
 * @param[in,out] database Borrowed connection with an active report.
 * @param[in] result Borrowed case record; strings are copied before return.
 * @return True on success; false on invalid data or SQLite failure. Caller must roll back after failure.
 */
bool test_database_run_record(test_database_t *database, const test_database_result_t *result);

/**
 * @brief Commit a completed report and prune older history within its fixed limits.
 * @param[in,out] database Borrowed connection with an active report.
 * @param[in] exit_code Actual process outcome, including setup failures.
 * @param[in] elapsed_ms Non-negative measured run duration.
 * @return True on success; false on failure. Retains at most 200 runs and 50000 detailed results.
 * Lifetime counts, extrema, means, exact median distributions and memory-test dates are preserved.
 */
bool test_database_run_finish(test_database_t *database, int exit_code, double elapsed_ms);

/**
 * @brief Initialise an empty, disposable schema-compatible fixture at an explicit path.
 * @param[in] root Borrowed repository root locating the schema.
 * @param[in] destination Borrowed fixture pathname owned and cleaned up by the test harness.
 * @param[out] error Optional owned diagnostic released with string_free().
 * @return True on success; false if destination exists, creation fails or schema validation fails.
 * @note This never copies or opens the shared catalogue. Encryption/corruption tests use their own fixtures.
 */
bool test_database_fixture(const string_t *root, const string_t *destination, string_t **error);

/**
 * @brief Execute an explicitly requested test-database administration command.
 * @param[in] root Borrowed repository root.
 * @param[in] argc Number of command arguments, excluding the tool and test-db subcommand.
 * @param[in] argv Borrowed argument array; accepted commands are documented by test-db help.
 * @return Zero on success, one on an operation failure, or two for invalid arguments.
 * @note Called only by an explicit command; linking this function does not create any files.
 */
int test_database_command(const string_t *root, int argc, char **argv);

#endif /* MARS_TEST_DATABASE_H */
