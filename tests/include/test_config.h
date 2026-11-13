#ifndef TEST_CONFIG_H
#define TEST_CONFIG_H

/**
 * @file test_config.h
 * @brief Hierarchical per-test enable/disable configuration with JSON persistence.
 *
 * Provides a hierarchical configuration system that lets individual tests or
 * whole groups be skipped without recompilation. State is loaded from JSON,
 * and missing tests/groups default to enabled and can be regenerated into the
 * saved config as explicit default-enabled entries.
 *
 * Three modes are supported (see test_config_mode_t):
 *   TEST_CONFIG_NONE    — disable config I/O entirely
 *   TEST_CONFIG_GLOBAL  — one shared JSON file for all test translation units
 *   TEST_CONFIG_LOCAL   — one JSON file per test source file
 *
 * Typical call sequence (handled automatically by test_harness.h):
 *   test_config_set_mode(mode);   // called once at startup
 *   test_config_is_enabled(file, name, parent);  // called per test
 *   test_config_shutdown();       // free resources
 */

#include <stdbool.h>

#include "json-t.h"
#include "string-t.h"
#include "test_harness.h"

/* ------------------------------------------------------------------------- */
/* Configuration mode                                                        */
/* ------------------------------------------------------------------------- */


/**
 * @brief Resolve an optional selection override while counting configured tests.
 * @param[in] source Borrowed normalised source path, valid during this call.
 * @param[in] path Borrowed complete dotted case or group path, valid during this call.
 * @param enabled Effective JSON selection, including disabled ancestors.
 * @return True selects the entry; false excludes it. The callback must not modify the configuration.
 */
typedef bool (*test_config_selection_fn)(const string_t *source, const string_t *path, bool enabled);

/**
 * @brief Count selected leaf cases for one module without executing tests or opening test storage.
 * @details A global snapshot selects source files in the suite source's directory. A local snapshot contains
 * the suite's own groups. Parent paths are indexed across source files so cross-file groups are not counted
 * as cases. Disabled cases and disabled groups are excluded; README/output cases count as ordinary work items.
 * Configuration absent for this module returns false, allowing callers to show discovery activity instead.
 * @param[in] configuration Borrowed immutable JSON snapshot; not modified or retained.
 * @param[in] suite_source Borrowed primary suite source, identifying the module directory.
 * @param mode GLOBAL or LOCAL; NONE has no configured count.
 * @param[in] selection Optional borrowed callback overriding JSON selection, or NULL to use JSON alone.
 * @param[out] count Required destination, initialised to zero; receives the count on success.
 * @return True when the module is represented, including a valid zero count; false for missing configuration,
 * invalid arguments, allocation failure or excessive nesting. No ownership is transferred. Call serially with
 * configuration changes; callbacks and supplied inputs need only remain valid for this call.
 */
bool test_config_count_enabled(const json_t *configuration, const string_t *suite_source, test_config_mode_t mode,
                               test_config_selection_fn selection, size_t *count);

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
 * so that subsequent calls to test_config_is_enabled(),
 * test_config_has_key_for(), and test_config_save() know which file to
 * consult.
 *
 * It must be called before any other test_config_* function.
 *
 * @param mode  The configuration mode to activate.
 */
void test_config_set_mode(test_config_mode_t mode);

/**
 * @brief Choose whether saving may prune configuration keys not discovered in this session.
 * @param enabled True permits pruning; false preserves undiscovered selections after an interrupted run.
 * @return No value. Changes process-local policy; file I/O happens only when test_config_save is called.
 * @note Sequential use only. No ownership is transferred and no release is required.
 */
void test_config_set_prune_enabled(bool enabled);

/**
 * @brief Release all global resources held by the test configuration system.
 *
 * This function destroys the global configuration dictionary and frees any
 * associated dynamically allocated state, including the cached local filename.
 *
 * It is intended to be called once at program shutdown to ensure that all
 * memory owned by the test configuration subsystem is released. To start a fresh
 * independent configuration session afterwards, call test_config_set_mode() before making another query.
 */
void test_config_shutdown(void);

/* ------------------------------------------------------------------------- */
/* Public API                                                                */
/* ------------------------------------------------------------------------- */

/**
 * @brief Query whether a test is enabled using string_t inputs.
 *
 * This function loads (on first use) the appropriate JSON file based on the
 * active configuration mode, then resolves the enable/disable state for the
 * given test key.
 *
 * Missing keys always default to "enabled = true".
 *
 * Missing keys are treated as enabled by default. When the harness later saves
 * configuration, discovered tests and groups may be written back with those
 * default-enabled states so that a missing config file can be regenerated.
 *
 * If a parent group is provided, the test inherits the effective enabled state
 * of all ancestors.
 *
 * This is the preferred form for harness code that already owns string_t
 * objects.
 *
 * @param file   The source filename of the test.
 * @param func   The test function name.
 * @param parent Optional parent group name (may be NULL). This should be a
 *               test/group identifier or a dot-separated chain of identifiers
 *               for nested groups, not an arbitrary free-form string.
 *
 * @return true if enabled, false if disabled.
 */
bool test_config_is_enabled(const string_t *file, const string_t *func, const string_t *parent);

/**
 * @brief Check whether a configuration key exists using string_t inputs.
 *
 * This checks only for explicit presence — it does *not* imply enabled or
 * disabled state. A missing key still behaves as "enabled" when queried via
 * test_config_is_enabled().
 *
 * @param file   The source filename of the test.
 * @param func   The test function name.
 * @param parent Optional parent group name (may be NULL).
 *
 * @return true if the key exists, false otherwise.
 */
bool test_config_has_key_for(const string_t *file, const string_t *func, const string_t *parent);

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
 * The normal test harness may call this after a run so that missing config
 * files can be regenerated with default-enabled entries for discovered tests
 * and groups.
 */
void test_config_save(void);

#endif /* TEST_CONFIG_H */
