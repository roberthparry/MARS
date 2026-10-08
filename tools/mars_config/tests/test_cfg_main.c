/**
 * @file test_cfg_main.c
 * @brief Sequential entry point for native MARS configuration tests.
 *
 * Exercises installer storage, weather setup, database imports and calendar
 * selection using disposable private directories, never the user's installation.
 */
#include "test_cfg_calendar_fixture.h"
#include "test_cfg_support.h"
#include "test_harness.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

/* Run ordinary installer tests; documented examples belong after these groups. */
int tests_main(void)
{
    test_cfg_make_self_fixture();
    TEST_SECTION("Native MARS configuration");
    test_cfg_storage_cases();
    test_cfg_weather_cases();
    test_cfg_database_cases();
    test_cfg_calendar_cases();
    test_cfg_terminal_cases();
    test_cfg_rendering_cases();
    test_cfg_locales_cases();
    test_cfg_make_cases();
    TEST_SECTION("README examples (last)");
    test_cfg_calendar_readme_cases();
    test_cfg_rendering_readme_cases();
    test_cfg_locales_readme_cases();
    test_cfg_calendar_fixture_finish();
    return TEST_EXIT_CODE();
}
