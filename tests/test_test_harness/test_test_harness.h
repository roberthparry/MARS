/**
 * @file test_test_harness.h
 * @brief Private registration boundary within the harness regression suite.
 *
 * Shares only the database-case registration callback between this suite's sources. The implementation under
 * test remains in tests/test_harness and is accessed through tests/include or the controlled tests/internal API.
 */
#ifndef MARS_TEST_TEST_HARNESS_INTERNAL_H
#define MARS_TEST_TEST_HARNESS_INTERNAL_H
#if !defined(MARS_TEST_TEST_HARNESS_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private regression access requires MARS_TEST_TEST_HARNESS_INTERNAL_ACCESS."
#endif

/**
 * @brief Run storage regressions, followed by the testing guide's explicit runnable examples.
 * @return No value. Fixtures own and release disposable storage; assertions report failures through the harness.
 * @note Called once, sequentially after configuration regressions. No connection survives a case callback.
 */
void test_harness_database_cases(void);

#endif
