/**
 * @file test_diffequ_energy.h
 * @brief Register autonomous-energy regression checks and their runnable guide example.
 *
 * Used only by the sequential differential-equation test suite. No declarations
 * are installed or exposed as library interfaces.
 */
#ifndef MARS_TEST_DIFFEQU_ENERGY_H
#define MARS_TEST_DIFFEQU_ENERGY_H

/**
 * @brief Run native energy-family, initial-value and rejection regressions.
 * @return Nothing; failures are reported through the test harness.
 * Takes no arguments, transfers no ownership and releases all fixtures before returning. Run sequentially.
 */
void test_diffequ_autonomous_energy(void);

/**
 * @brief Run the executable autonomous-force example from docs/diffequation.md after ordinary checks.
 * @return Nothing; prints the documented solution families and reports assertions through the harness.
 * Takes no arguments, transfers no ownership and releases all fixtures before returning. Run sequentially.
 */
void example_diffequation_autonomous_energy(void);

#endif
