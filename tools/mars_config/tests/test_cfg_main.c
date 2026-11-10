/**
 * @file test_cfg_main.c
 * @brief Sequential entry point for native MARS configuration tests.
 *
 * Exercises installer storage, weather setup, database imports and calendar
 * selection using disposable private directories, never the user's installation.
 * Synthetic encrypted fixtures retain memory security and codec errors. Repeated
 * memory-lock quota warnings are omitted from their captured diagnostics.
 */
#include <stdio.h>
#include <stdlib.h>

#include "string-t.h"
#include "sqlite-t.h"
#include "test_harness.h"
#include "test_cfg_support.h"
#include "test_cfg_calendar_fixture.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

/* Handle installer-fixture commands before the generic harness validates its own command-line options. */
void test_program_setup(void)
{
    /* SQLCipher logging is process-local. Avoid a warning per allocation when a fixture exceeds the memlock quota;
     * this changes only test diagnostics, not encryption, memory wiping or the application's log policy. */
    string_t *path = string_new_with(":memory:");
    sqlite_t *db = path ? sqlite_open_plain(path) : NULL;
    bool ready = db && sqlite_exec_cstr(db, "PRAGMA cipher_log_level = ERROR");
    sqlite_close(db);
    string_free(path);
    if (!ready) {
        fputs("Could not initialise installer-fixture diagnostics.\n", stderr);
        exit(EXIT_FAILURE);
    }
    test_cfg_make_self_fixture();
    test_cfg_command_self_fixture();
}

/* Run ordinary installer tests; documented examples belong after these groups. */
int tests_main(void)
{
    TEST_SECTION("Native MARS configuration");
    test_cfg_progress_cases();
    test_cfg_storage_cases();
    test_cfg_paths_cases();
    test_cfg_weather_cases();
    test_cfg_database_cases();
    test_cfg_reimport_cases();
    test_cfg_rivers_cases();
    test_cfg_textures_cases();
    test_cfg_pyramids_cases();
    test_cfg_calendar_cases();
    test_cfg_terminal_cases();
    test_cfg_rendering_cases();
    test_cfg_locales_cases();
    test_cfg_make_cases();
    TEST_SECTION("README examples (last)");
    test_cfg_calendar_readme_cases();
    test_cfg_rendering_readme_cases();
    test_cfg_locales_readme_cases();
    test_cfg_textures_readme_cases();
    test_cfg_calendar_fixture_finish();
    return TEST_EXIT_CODE();
}
