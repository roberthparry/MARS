/**
 * @file test_diffequ_riccati.h
 * @brief Register native Riccati regressions and the matching differential-equation README example.
 *
 * Only the differential-equation suite includes this test interface. Tests own and release their fixtures;
 * they run sequentially through the ordinary harness and require no application or private solver layout.
 */
#ifndef TEST_DIFFEQU_RICCATI_H
#define TEST_DIFFEQU_RICCATI_H

/**
 * @brief Check equivalent Riccati forms, both projective branches and numerical ODE residuals.
 * @return Nothing; failures are recorded in the active test harness. All fixtures are released locally.
 */
void test_diffequ_riccati_families(void);

/**
 * @brief Verify value conditions at zero and away from zero, including a variable quadratic coefficient.
 * @return Nothing; failures are recorded in the active test harness. No fixture ownership transfers.
 */
void test_diffequ_riccati_initial_values(void);

/**
 * @brief Verify honest unsupported reductions and rejection of non-Riccati or unsuitable condition data.
 * @return Nothing; failures are recorded in the active test harness. All fixtures are released locally.
 */
void test_diffequ_riccati_scope(void);

/**
 * @brief Run the Riccati README example after ordinary tests and check its displayed output.
 * @return Nothing; failures are recorded in the active test harness. Printed notation remains solver-owned.
 */
void example_diffequation_riccati(void);

#endif
