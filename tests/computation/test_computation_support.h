/**
 * @file test_computation_support.h
 * @brief Registration interface for scoped expression-control tests and README output.
 *
 * Used only by the computation test suite, with examples registered last.
 */
#ifndef MARS_TEST_COMPUTATION_SUPPORT_H
#define MARS_TEST_COMPUTATION_SUPPORT_H

/** @brief Register ordinary expression scope and recovery tests. */
void test_computation_expression_cases(void);

/** @brief Register the documented scoped evaluation example after ordinary tests. */
void test_computation_expression_readme(void);

/** @brief Register cancellation and rollback regressions across native modules. */
void test_computation_module_cases(void);

/** @brief Run the native module guides' computation examples after ordinary cases. */
void test_computation_module_readme(void);

#endif
