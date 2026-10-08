/**
 * @file test_checks.h
 * @brief Private registration and temporary-tree support for native check tests.
 *
 * Only this sequential test executable consumes these helpers. Every registered
 * case uses the shared global configuration harness. Fixtures own disposable
 * trees and never modify the real Git index or repository configuration.
 */
#ifndef MARS_TEST_CHECKS_H
#define MARS_TEST_CHECKS_H

#include "checks_support.h"

/**
 * @brief Create an owned private temporary root.
 * @return Owned path, released with test_checks_finish; allocation failures terminate.
 */
string_t *test_checks_root(void);

/**
 * @brief Write fixture text, creating its parent directories.
 * @param root Borrowed fixture root.
 * @param path Relative fixture path.
 * @param content UTF-8 fixture contents.
 * @return True when the complete file was written.
 */
bool test_checks_write(const string_t *root, const char *path, const char *content);

/**
 * @brief Remove and release a private test root.
 * @param root Owned root created by test_checks_root.
 * @return True when the tree was removed.
 */
bool test_checks_finish(string_t *root);

/**
 * @brief Register parser, quoting and output comparison correctness cases.
 * @return No value; results are recorded by the shared harness.
 */
void test_checks_parser_cases(void);

/**
 * @brief Register policy, filesystem and process correctness cases.
 * @return No value; results are recorded by the shared harness.
 */
void test_checks_control_cases(void);

/**
 * @brief Register all migrated Python Markdown coverage and layout cases.
 * @return No value; results are recorded by the shared harness.
 */
void test_checks_documentation_cases(void);

/**
 * @brief Register README runner integration cases after ordinary correctness tests.
 * @return No value; results are recorded by the shared harness.
 */
void test_checks_readme_cases(void);

/** @brief Register native coverage-report regressions. @return No value. */
void test_checks_coverage_cases(void);

/** @brief Register native release-evidence regressions. @return No value. */
void test_checks_evidence_cases(void);

/** @brief Register native C source-policy regressions. @return No value. */
void test_checks_policy_cases(void);

/** @brief Run the documented parser benchmark after ordinary tests. @return No value. */
void test_checks_policy_readme_cases(void);

#endif
