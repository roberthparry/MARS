/**
 * @file test_diffequ_emden.h
 * @brief Register exact independent-coefficient modified-Emden checks and their executable guide example.
 *
 * Used by the differential-equation suite only. These functions run sequential harness cases and release
 * all owned mathematical fixtures; they are not installed library interfaces.
 */
#ifndef MARS_TEST_DIFFEQU_EMDEN_H
#define MARS_TEST_DIFFEQU_EMDEN_H

/**
 * @brief Run quadrature identities, exceptional branches and coefficient-matching regressions.
 * @return Nothing; individual assertions report through the test harness.
 * @note Takes no parameters and transfers no ownership. Run serially; numerical precision is restored.
 */
void test_diffequ_emden_quadratures(void);

/**
 * @brief Run the executable independent-coefficient quadrature example from docs/diffequation.md.
 * @return Nothing; prints the documented metadata and reports assertions through the README harness.
 * @note Takes no parameters and transfers no ownership. All fixtures are released; run after ordinary tests.
 */
void example_diffequation_emden_quadrature(void);

/**
 * @brief Run the symbolic point-linearisation example from docs/diffequation.md using Unicode derivatives.
 * @return Nothing; prints the documented input and rational solution, and reports failure through the harness.
 * @note Takes no parameters and transfers no ownership. Releases all fixtures; run serially after ordinary tests.
 */
void example_diffequation_symbolic_emden(void);

/**
 * @brief Run the numerical-trajectory README example and print its final sampled value.
 * @return Nothing; the harness verifies and prints the documented output after ordinary tests.
 * @note Takes no parameters or ownership; releases its problem, numbers and sampler. Run serially.
 */
void example_diffequation_trajectory(void);

#endif
