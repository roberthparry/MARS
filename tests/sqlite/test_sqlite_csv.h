/**
 * @file test_sqlite_csv.h
 * @brief CSV ingestion and export lanes for the SQLite regression suite.
 *
 * Only the SQLite test runner consumes these entry points. Ordinary cases run
 * before the README output examples; no test helpers enter the library API.
 */
#ifndef TEST_SQLITE_CSV_H
#define TEST_SQLITE_CSV_H

/** @brief Run CSV import/export regressions. */
void test_sqlite_csv_cases(void);

/** @brief Run complete documented CSV examples after ordinary tests. */
void test_sqlite_csv_examples(void);

#endif
