/**
 * @file test_diffequ_interval.h
 * @brief Registration of native coordinate-interval and documented usage checks.
 *
 * Used only by the differential-equation test suite. These entry points register
 * cases with the shared harness; they are not library or application APIs.
 */
#ifndef MARS_TEST_DIFFEQU_INTERVAL_H
#define MARS_TEST_DIFFEQU_INTERVAL_H

/**
 * @brief Run coordinate and parameter restriction parsing, serialisation and solution-domain checks.
 * @return No value; individual assertions report failures through the sequential test harness.
 * No arguments or ownership transfers; all fixtures are released before returning. Not a concurrent runner.
 */
void test_diffequ_intervals(void);

/**
 * @brief Run the executable interval example from docs/diffequation.md after ordinary tests.
 * @return No value; printed output and assertions follow the README-example harness conventions.
 * No arguments or ownership transfers; all fixtures are released before returning. Run sequentially.
 */
void example_diffequation_interval(void);

/**
 * @brief Run the parameter-restriction example from docs/diffequation.md after ordinary tests.
 * @return No value; printed output and assertions follow the README-example harness conventions.
 * No arguments or ownership transfers; all fixtures are released before returning. Run sequentially.
 */
void example_diffequation_parameter_restrictions(void);

#endif
