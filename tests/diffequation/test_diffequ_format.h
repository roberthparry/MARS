/**
 * @file test_diffequ_format.h
 * @brief Register differential-equation notation checks and their runnable guide example.
 *
 * Private to the sequential differential-equation suite; no fixture is an installed library interface.
 */
#ifndef TEST_DIFFEQU_FORMAT_H
#define TEST_DIFFEQU_FORMAT_H

/**
 * @brief Check that integer powers retain their native notation and parsing semantics.
 * @return Nothing; releases its fixtures and reports assertions through the harness.
 * Takes no arguments or ownership; call sequentially.
 */
void test_diffequ_expression_integer_powers(void);

/**
 * @brief Check database-backed explicit solution notation without changing the calculable equation sides.
 * @return Nothing; releases its fixtures and reports assertions through the harness.
 * Takes no arguments or ownership; call sequentially.
 */
void test_diffequ_solution_notation(void);

/**
 * @brief Run the Unicode-power example from docs/diffequation.md after ordinary checks.
 * @return Nothing; prints the documented output and reports assertions through the harness.
 * Takes no arguments or ownership; releases all fixtures before returning. Call sequentially.
 */
void example_diffequation_unicode_powers(void);

#endif
