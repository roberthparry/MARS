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

/** Run callback beneath a private temporary directory; return success after cleanup. */
bool test_cfg_isolated(bool (*callback)(const char *directory));

/** Register literal configuration and path tests. */
void test_cfg_storage_cases(void);

/** Register weather configuration tests. */
void test_cfg_weather_cases(void);

/** Register database installation tests. */
void test_cfg_database_cases(void);

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

/** Register root Make installer dispatch and forwarding tests. */
void test_cfg_make_cases(void);

/** Run documented translated-calendar examples after every ordinary test group. */
void test_cfg_calendar_readme_cases(void);

/** Run documented calendar-field examples after every ordinary test group. */
void test_cfg_rendering_readme_cases(void);

/** Run the documented native locale check after all ordinary tests. */
void test_cfg_locales_readme_cases(void);

#endif
