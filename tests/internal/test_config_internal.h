/**
 * @file test_config_internal.h
 * @brief Controlled source-path access for configuration regression tests.
 *
 * Exposes the path normaliser without including the configuration module's implementation headers. Ordinary
 * test suites use test_config.h and the harness; this interface is for verifying catalogue source identities.
 */
#ifndef MARS_SHARED_TEST_CONFIG_INTERNAL_H
#define MARS_SHARED_TEST_CONFIG_INTERNAL_H

#if !defined(MARS_SHARED_TEST_CONFIG_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private test-configuration access requires MARS_SHARED_TEST_CONFIG_INTERNAL_ACCESS."
#endif

#include "string-t.h"
#include "test_config.h"

/**
 * @brief Count configured leaves for the active module without executing or saving tests.
 * @param[in] suite_source Caller-owned primary source identifying the module directory.
 * @param[in] selection Optional caller-owned synchronous override; NULL uses JSON alone.
 * @param[out] count Required destination receiving the count, initially zero.
 * @return True for a complete known count, including zero; false for discovery, invalid input or allocation failure.
 * @note Owns no caller input. Serialise with configuration mutations; storage is not opened.
 */
bool test_config_count_current(const string_t *suite_source, test_config_selection_fn selection, size_t *count);

/**
 * @brief Normalise a test source into its stable repository-relative identity.
 * @param[in] file Borrowed source path; not modified or retained and valid for this call. NULL means an empty path.
 * @return Owned normalised path released with string_free(), or NULL on allocation or UTF-8 copying failure.
 * @note The result is independent of the input's lifetime. Call serially with configuration operations.
 */
string_t *test_config_normalise_file_path(const string_t *file);

#endif /* MARS_SHARED_TEST_CONFIG_INTERNAL_H */
