/**
 * @file test_cfg_support.h
 * @brief Private fixtures and suite registrations for native installer tests.
 *
 * All callbacks run sequentially in child processes with disposable MARS_HOME
 * directories. Cleanup is bounded and never follows symbolic links.
 */
#ifndef MARS_TEST_CFG_SUPPORT_H
#define MARS_TEST_CFG_SUPPORT_H

#include <stdbool.h>

/**
 * @brief Create compact valid image fixtures beneath a caller-owned disposable root.
 * @param directory Borrowed root path; its owner removes the generated package after use.
 * @return True on success; false on image-generation, file or metadata failure.
 */
bool test_cfg_copy_textures(const char *directory);

/**
 * @brief Obtain the process-owned compact texture package used by installer regressions.
 * @return Borrowed root path valid until process exit, or NULL on failure; never free or modify it.
 * @note Tests call this serially; the parent process releases its private storage at normal exit.
 */
const char *test_cfg_texture_root(void);

/** @brief Register encrypted configuration and relocatable resource regressions. */
void test_cfg_paths_cases(void);

/** Register photographic storage, validation and rollback regressions. */
void test_cfg_textures_cases(void);

/**
 * @brief Run transactional source-pyramid tests against disposable databases, serially.
 * @return No value; failures are recorded by the shared test harness. No installed data is accessed.
 */
void test_cfg_pyramids_cases(void);

/** Run the documented photographic metadata example after ordinary tests. */
void test_cfg_textures_readme_cases(void);

/** Run callback beneath a private temporary directory; return success after cleanup. */
bool test_cfg_isolated(bool (*callback)(const char *directory));

/**
 * @brief Run one content-store fixture with encrypted settings confined to a helper process.
 * @param[in] callback Borrowed fixture callback receiving its disposable home for the call's lifetime.
 * @return True after the callback passes and all private files are removed; false on setup, test or cleanup failure.
 * @note Invoke sequentially. Uses synthetic keys and owns/reaps the settings helper before removing fixture files.
 */
bool test_cfg_content_isolated(bool (*callback)(const char *directory));

/** Register literal configuration and path tests. */
void test_cfg_storage_cases(void);

/** Register weather configuration tests. */
void test_cfg_weather_cases(void);

/** Register live progress and redirected-output tests. */
void test_cfg_progress_cases(void);

/** Register database installation tests. */
void test_cfg_database_cases(void);

/**
 * @brief Register sequential one-time release-import replacement and recovery regressions.
 * @return No value. Cases use disposable databases and synthetic settings, and run before README examples.
 * No user files are read or changed; the shared harness owns registration and execution.
 */
void test_cfg_reimport_cases(void);

/** Register installer-owned OSM river import and runtime-query regressions. */
void test_cfg_rivers_cases(void);

/** Register calendar selection and population tests. */
void test_cfg_calendar_cases(void);

/** Register interactive terminal installation and cancellation tests. */
void test_cfg_terminal_cases(void);

/** Register independent calendar rendering and field regressions. */
void test_cfg_rendering_cases(void);

/** Register native locale generation, input validation and SQL parity tests. */
void test_cfg_locales_cases(void);

/** Exit in the explicitly requested Make executable-fixture mode; otherwise return. */
void test_cfg_make_self_fixture(void);

/**
 * @brief Dispatch an explicitly marked subprocess command with an isolated settings key.
 * @return No value; exits after command dispatch in fixture mode, otherwise returns unchanged.
 * @note Uses only process-owned synthetic credentials; never accesses the desktop key store.
 */
void test_cfg_command_self_fixture(void);

/** Register root Make installer dispatch and forwarding tests. */
void test_cfg_make_cases(void);

/** Run documented translated-calendar examples after every ordinary test group. */
void test_cfg_calendar_readme_cases(void);

/** Run documented calendar-field examples after every ordinary test group. */
void test_cfg_rendering_readme_cases(void);

/** Run the documented native locale check after all ordinary tests. */
void test_cfg_locales_readme_cases(void);

#endif
