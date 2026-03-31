#ifndef TEST_CONFIG_H
#define TEST_CONFIG_H

#include <stdbool.h>

/**
 * @brief Query whether a test is enabled.
 *
 * Looks up the configuration entry for @p func within the file @p file.
 * If @p parent is non-NULL, the lookup is performed inside the parent's
 * nested object. Missing keys default to "enabled = true".
 *
 * @param file   The source filename associated with the test.
 * @param func   The test function name.
 * @param parent Optional parent key for nested test groups (may be NULL).
 *
 * @return 1 if enabled, 0 if disabled.
 */
int test_enabled(const char *file, const char *func, const char *parent);

/**
 * @brief Persist the in-memory test configuration to disk.
 *
 * Writes the current configuration to the test_config.json file using
 * an atomic write (via a temporary file + rename). No-op if nothing
 * has changed.
 */
void test_config_save(void);

/**
 * @brief Check whether a configuration key exists.
 *
 * This does not imply the test is enabled or disabled — only that the
 * key is explicitly present in the JSON file.
 *
 * @param file   The source filename associated with the test.
 * @param func   The test function name.
 * @param parent Optional parent key for nested test groups (may be NULL).
 *
 * @return true if the key exists, false otherwise.
 */
bool test_config_has_key(const char *file, const char *func, const char *parent);

#endif
