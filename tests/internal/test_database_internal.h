/**
 * @file test_database_internal.h
 * @brief Controlled access to test-storage fixtures from other test modules.
 *
 * Storage regression tests may execute trusted fixture SQL, prepare reference statements and copy authored SQL
 * sources. This boundary exposes no connection representation or implementation header. Ordinary suites use
 * tests/include/test_database.h and the test harness instead. Callers must explicitly grant shared access.
 */
#ifndef MARS_SHARED_TEST_DATABASE_INTERNAL_H
#define MARS_SHARED_TEST_DATABASE_INTERNAL_H

#if !defined(MARS_SHARED_TEST_DATABASE_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private test-storage access requires MARS_SHARED_TEST_DATABASE_INTERNAL_ACCESS."
#endif

#if defined(__has_include)
#if __has_include(<sqlite3.h>)
#include <sqlite3.h>
#else
#include <sqlcipher/sqlite3.h>
#endif
#else
#include <sqlite3.h>
#endif

#include "test_database.h"

/**
 * @brief Commit one completed run and print any preserved timing-outlier warnings.
 * @param[in,out] database Caller-owned storage with an active reporting transaction.
 * @param exit_code Completed executable status.
 * @param elapsed_ms Total non-negative elapsed milliseconds.
 * @return True after committing and assessing warnings, false on storage failure; close the connection to roll back.
 * @note Inputs are not consumed; call sequentially. Historical outliers remain recorded and excluded from baselines.
 */
bool test_database_report_complete(test_database_t *database, int exit_code, double elapsed_ms);

/**
 * @brief Execute trusted SQL on a test-owned storage connection.
 * @param[in,out] database Borrowed open connection; the caller owns its transaction and lifetime.
 * @param[in] sql Borrowed, trusted SQL text valid until the call returns.
 * @return True on success; false with a retained diagnostic available through test_database_error().
 * @note External values must be bound through a prepared statement rather than interpolated into SQL.
 */
bool test_database_exec(test_database_t *database, const char *sql);

/**
 * @brief Prepare a reference statement for an authorised storage regression fixture.
 * @param[in,out] database Borrowed open connection, which must outlive the returned statement.
 * @param[in] sql Borrowed trusted SQL text, copied by SQLite where necessary.
 * @return Owned statement released with sqlite3_finalize(), or NULL with a retained diagnostic on failure.
 * @note Statements and connections are used sequentially; never pass a live handle to the module under test.
 */
sqlite3_stmt *test_database_prepare(test_database_t *database, const char *sql);

/**
 * @brief Copy the shared schema and discovered module SQL files into an isolated test checkout.
 * @param[in] source_root Borrowed repository root; NULL selects the current directory.
 * @param[in] destination_root Borrowed fixture root; its tests directory and module directories are created.
 * @return True on success; false on allocation, directory or copy failure. Existing files are never overwritten.
 * @note The fixture owns its copied files and removes them through the harness cleanup facilities.
 */
bool test_database_sources_copy(const string_t *source_root, const string_t *destination_root);

#endif /* MARS_SHARED_TEST_DATABASE_INTERNAL_H */
