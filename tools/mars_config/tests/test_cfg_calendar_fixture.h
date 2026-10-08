/**
 * @file test_cfg_calendar_fixture.h
 * @brief Shared immutable calendar seed with disposable per-case working copies.
 *
 * Calendar and terminal regressions use these test-only helpers. The sequential
 * runner owns one read-only encrypted seed, built lazily outside test children.
 * Each case receives an independent copied database and private saved settings.
 * No database connection, mutable settings or process environment is shared.
 */
#ifndef MARS_TEST_CFG_CALENDAR_FIXTURE_H
#define MARS_TEST_CFG_CALENDAR_FIXTURE_H

#include "sqlite.h"

/** Run a callback through test_cfg_isolated after preparing the process-owned immutable seed once. */
bool test_cfg_calendar_isolated(bool (*callback)(const char *directory));

/** Copy the seed into an isolated home, create its saved settings and return an owned connection, path and key. */
sqlite_t *test_cfg_calendar_database(const char *directory, string_t **path, string_t **key);

/** Remove the runner's seed after all ordinary and README groups, recording any cleanup failure in the harness. */
void test_cfg_calendar_fixture_finish(void);

#endif
