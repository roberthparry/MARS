/**
 * @file test_TeX_builder.h
 * @brief Register public reverse Polish construction checks with the TeX test suite.
 *
 * Shared only by the TeX suite entry point and its builder regression implementation.
 * This test-support interface is not installed with the library.
 */
#ifndef TESTS_TeX_BUILDER_H
#define TESTS_TeX_BUILDER_H

/**
 * @brief Run the public builder's ownership, operand-order and failure-recovery cases sequentially.
 * @note Uses the shared harness and tests/test_config.json; acquires no caller-owned resources.
 */
void run_TeX_builder_tests(void);

#endif
