/**
 * @file test_diffequ_bessel.h
 * @brief Register native scaled Bessel regressions and their runnable guide example.
 *
 * Used only by the sequential differential-equation suite; these declarations are
 * test fixtures, not installed library interfaces.
 */
#ifndef MARS_TEST_DIFFEQU_BESSEL_H
#define MARS_TEST_DIFFEQU_BESSEL_H

/**
 * @brief Run database-backed Bessel bases, condition and scope checks.
 * @return Nothing; reports failures through the harness and releases every fixture.
 * Takes no arguments, transfers no ownership and must run sequentially.
 */
void test_diffequ_bessel(void);

/**
 * @brief Print and verify the executable Bessel example from docs/diffequation.md.
 * @return Nothing; reports assertions through the harness and releases every fixture.
 * Takes no arguments and transfers no ownership. Run after ordinary checks.
 */
void example_diffequation_bessel(void);

#endif
