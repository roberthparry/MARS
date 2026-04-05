#ifndef TEST_CONFIG_H
#define TEST_CONFIG_H

#include <stdbool.h>

/* ------------------------------------------------------------------------- */
/* Configuration mode                                                        */
/* ------------------------------------------------------------------------- */

/**
 * @enum test_config_mode_t
 * @brief Selects how test enable/disable information is stored and resolved.
 *
 * The test configuration system supports two independent modes:
 *
 * ### TEST_CONFIG_GLOBAL
 * A single shared JSON file is used for all test translation units:
 *
 *     tests/test_config.json
 *
 * The JSON structure is:
 * @code{.json}
 * {
 *     "test_file.c": {
 *         "group": {
 *             "enabled": true,
 *             "test_name": true
 *         },
 *         "another_group": { ... }
 *     }
 * }
 * @endcode
 *
 * ### TEST_CONFIG_LOCAL
 * Each test translation unit uses its own JSON file, derived from `__FILE__`:
 *
 *     tests/<basename>.json
 *
 * The JSON structure omits the filename wrapper:
 * @code{.json}
 * {
 *     "group": {
 *         "enabled": true,
 *         "test_name": true
 *     }
 * }
 * @endcode
 *
 * LOCAL mode is ideal when tests should not interfere with each other’s
 * configuration, while GLOBAL mode is ideal for centralised control.
 */
typedef enum {
    TEST_CONFIG_GLOBAL,  /**< Use tests/test_config.json */
    TEST_CONFIG_LOCAL    /**< Use tests/<basename>.json */
} test_config_mode_t;

/**
 * @brief Default configuration mode if the test file does not override it.
 *
 * Test files may override this by defining TEST_CONFIG_MODE before including
 * this header.
 */
#ifndef TEST_CONFIG_MODE
#define TEST_CONFIG_MODE TEST_CONFIG_GLOBAL
#endif

/**
 * @brief Set the active configuration mode for this test process.
 *
 * This function does not load or parse any JSON. It simply records the mode
 * so that subsequent calls to test_enabled(), test_config_has_key(), and
 * test_config_save() know which file to consult.
 *
 * It must be called before any other test_config_* function.
 *
 * @param mode  The configuration mode to activate.
 */
void test_config_set_mode(test_config_mode_t mode);

/**
 * @brief Release all global resources held by the test configuration system.
 *
 * This function destroys the global configuration dictionary and frees any
 * associated dynamically allocated state, including the cached local filename.
 * 
 * It is intended to be called once at program shutdown to ensure that all
 * memory owned by the test configuration subsystem is released.  After this
 * call, the test configuration API must not be used again.
 */
void test_config_shutdown(void);

/* ------------------------------------------------------------------------- */
/* Public API                                                                */
/* ------------------------------------------------------------------------- */

/**
 * @brief Query whether a test is enabled.
 *
 * This function loads (on first use) the appropriate JSON file based on the
 * active configuration mode, then resolves the enable/disable state for the
 * given test key.
 *
 * Missing keys always default to "enabled = true".
 *
 * If a parent group is provided, the test inherits the effective enabled state
 * of all ancestors.
 *
 * @param file   The source filename of the test (use `__FILE__`).
 * @param func   The test function name (use `__func__` or `#func`).
 * @param parent Optional parent group name (may be NULL).
 *
 * @return 1 if enabled, 0 if disabled.
 */
int test_enabled(const char *file, const char *func, const char *parent);

/**
 * @brief Check whether a configuration key exists in the JSON file.
 *
 * This checks only for explicit presence — it does *not* imply enabled or
 * disabled state. A missing key still behaves as "enabled" when queried via
 * test_enabled().
 *
 * @param file   The source filename of the test.
 * @param func   The test function name.
 * @param parent Optional parent group name.
 *
 * @return true if the key exists, false otherwise.
 */
bool test_config_has_key(const char *file, const char *func, const char *parent);

/**
 * @brief Persist the in-memory configuration to disk.
 *
 * The output path depends on the active mode:
 *
 * - **GLOBAL mode:** `tests/test_config.json`
 * - **LOCAL mode:**  `tests/<basename>.json`
 *
 * Writes are performed atomically using a temporary file followed by rename.
 *
 * This function does nothing if no configuration has been loaded or modified.
 */
void test_config_save(void);


#endif /* TEST_CONFIG_H */
