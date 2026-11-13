/**
 * @file test_cfg_calendar_fixture.h
 * @brief Shared immutable calendar seed with disposable per-case working copies.
 *
 * Calendar and terminal regressions use these test-only helpers. The sequential
 * runner owns one read-only plaintext seed, built lazily outside test children.
 * Each case receives an independent copied database and private saved settings.
 * No database connection, mutable settings or process environment is shared.
 */
#ifndef MARS_TEST_CFG_CALENDAR_FIXTURE_H
#define MARS_TEST_CFG_CALENDAR_FIXTURE_H

#include "sqlite-t.h"

/**
 * @brief Run a calendar case with its own home after preparing the immutable catalogue once.
 * @param callback Function called in an isolated child with its temporary directory; must not be NULL.
 * @return True if preparation and the callback succeed; false on allocation, installation or child failure.
 * @note The sequential runner owns the seed until test_cfg_calendar_fixture_finish(). No connection is shared.
 */
bool test_cfg_calendar_isolated(bool (*callback)(const char *directory));

/**
 * @brief Copy the catalogue into the current isolated child and save its private configuration.
 * @param directory Borrowed child-owned temporary directory, valid for this call.
 * @param[out] path Receives an owned pathname; release with string_free().
 * @param[out] key Receives an owned empty key selecting plaintext storage; release with string_free().
 * @return An owned connection, released with sqlite_close(), or NULL with both outputs NULL on failure.
 * @note Call only through test_cfg_calendar_isolated(), sequentially. Output pointers must be distinct.
 */
sqlite_t *test_cfg_calendar_database(const char *directory, string_t **path, string_t **key);

/**
 * @brief Remove the runner-owned seed after all ordinary and README cases.
 * @return Nothing; any cleanup failure is recorded as a harness assertion.
 * @note Call in the parent runner after all isolated children have exited. Safe when no seed was prepared.
 */
void test_cfg_calendar_fixture_finish(void);

/**
 * @brief Prepare real schema, town and locale sources for isolated selection and publication checks.
 * @param directory Borrowed temporary home; owns and removes the resulting source directory after the case.
 * @return True after copying the sources and setting MARS_ROOT, or false on allocation, file or environment failure.
 * @note Retains core town locations and the complete locale data. Bulk worldwide town and holiday-rule imports
 * are covered by the shared complete calendar seed. Run only in an isolated child before starting worker threads.
 */
bool test_cfg_calendar_sources(const char *directory);

#endif
