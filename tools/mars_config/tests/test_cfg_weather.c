/**
 * @file test_cfg_weather.c
 * @brief Native weather installer regressions in disposable, isolated homes.
 *
 * Checks skipped setup, credential precedence, private permissions, literal
 * persistence, rejected input and command-line dispatch. Synthetic keys only;
 * no provider request or running Lab is involved. Uses the public file module.
 */
#include <stdlib.h>

#include "file-t.h"
#include "lab_process.h"
#include "mars_settings.h"
#include "cfg_weather.h"
#include "test_harness.h"
#include "test_cfg_support.h"

static file_t *test_cfg_weather_file(const char *directory, const char *name)
{
    string_t *path = string_sprintf("%s/%s", directory, name);
    file_t *file = path ? file_new(path) : NULL;
    string_free(path);
    return file;
}

static bool test_cfg_weather_matches(file_t *file, const char *key)
{
    (void)file;
    string_t *contents = mars_settings_read("MARS_WEATHER_API_KEY");
    string_t *expected = string_new_with(key);
    bool ok = contents && expected && !string_compare(contents, expected);
    string_free(contents); string_free(expected);
    return ok;
}

static bool test_cfg_weather_storage_fixture(const char *directory)
{
    file_t *folder = test_cfg_weather_file(directory, "config");
    file_t *file = test_cfg_weather_file(directory, "mars.db");
    bool ok = folder && file && !unsetenv("MARS_WEATHER_API_KEY") && !unsetenv("WEATHERAPI_KEY") &&
              !cfg_weather_run(NULL, false) && !file_exists(folder) && !file_last_error(folder);
    ok = ok && !setenv("MARS_WEATHER_API_KEY", "fixture-primary", 1) && !cfg_weather_run(NULL, false) &&
         test_cfg_weather_matches(file, "fixture-primary");
    ok = ok && !cfg_weather_run("  fixture-explicit  ", false) && test_cfg_weather_matches(file, "fixture-explicit");
    ok = ok && !unsetenv("MARS_WEATHER_API_KEY") && !unsetenv("WEATHERAPI_KEY") && !cfg_weather_run(NULL, false) &&
         test_cfg_weather_matches(file, "fixture-explicit");
    file_info_t *info = file_get_info(file), *folder_info = file_get_info(folder);
    ok = ok && info && folder_info && (file_info_permissions(info) & 0777) == 0600 &&
         (file_info_permissions(folder_info) & 0777) == 0700;
    file_info_free(info);
    file_info_free(folder_info);
    const char *invalid[] = {"key\nexport EVIL=1", "clé"};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i)
        ok = ok && cfg_weather_run(invalid[i], false) == 1 && test_cfg_weather_matches(file, "fixture-explicit");
    /* Shell-looking content stays literal JSON data. */
    ok = ok && !cfg_weather_run("fixture-$(false)-`false`", false) && !cfg_weather_run(NULL, false) &&
         test_cfg_weather_matches(file, "fixture-$(false)-`false`");
    file_free(file);
    file_free(folder);
    return ok;
}

static bool test_cfg_weather_rejection_fixture(const char *directory)
{
    file_t *folder = test_cfg_weather_file(directory, "config");
    file_t *file = test_cfg_weather_file(directory, "mars.db");
    bool ok = folder && file && file_create_directory(folder, 0700, false) &&
              file_create_directory(file, 0700, false) && cfg_weather_run("fixture", false) == 1;
    file_info_t *info = file_get_info(file);
    ok = ok && info && file_info_type(info) == FILE_TYPE_DIRECTORY;
    file_info_free(info);
    file_t *target = test_cfg_weather_file(directory, "target");
    string_t *contents = string_new_with("unchanged");
    ok = ok && target && contents && file_remove_directory(file) && file_write_all_text(target, contents) &&
         file_create_symlink(target, file) && cfg_weather_run("fixture", false) == 1;
    string_t *after = file_read_all_text(target);
    ok = ok && after && contents && !string_compare(after, contents);
    string_free(after);
    ok = ok && file_delete(file) && file_create_hard_link(target, file) && cfg_weather_run("fixture", false) == 1;
    after = file_read_all_text(target);
    ok = ok && after && contents && !string_compare(after, contents);
    string_free(after);
    string_free(contents);
    file_free(target);
    file_free(file);
    file_free(folder);
    return ok;
}

static bool test_cfg_weather_command_fixture(const char *directory)
{
    const char *binary = MARS_CONFIG_PROGRAM;
    const char *args[] = {binary, "weather", "--noninteractive", "--weather-key", "fixture-cli", NULL};
    string_t *output = NULL;
    int status = -1;
    bool ok = lab_proc_run(args, NULL, 5000, 4096, &output, &status) && status == 0 && output &&
              string_view_equals_literal(string_view_all(output), "Weather API configuration saved.\n");
    if (!ok) fprintf(stderr, "Weather command fixture: status %d; output: %s\n", status,
                     output ? string_c_str(output) : "<none>");
    string_free(output);
    file_t *file = test_cfg_weather_file(directory, "mars.db");
    file_t *cache = test_cfg_weather_file(directory, "lab");
    ok = ok && file && cache && test_cfg_weather_matches(file, "fixture-cli") && !file_exists(cache);
    const char *invalid[] = {binary, "weather", "--weather-key", NULL};
    output = NULL;
    ok = ok && lab_proc_run(invalid, NULL, 5000, 4096, &output, &status) && status == 2 &&
         test_cfg_weather_matches(file, "fixture-cli");
    string_free(output);
    file_free(cache);
    file_free(file);
    return ok;
}

static void test_cfg_weather_storage(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(test_cfg_weather_storage_fixture),
                     "weather keys have private, literal, stable persistence");
}

static void test_cfg_weather_rejection(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(test_cfg_weather_rejection_fixture), "unsafe destinations remain unchanged");
}

static void test_cfg_weather_command(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(test_cfg_weather_command_fixture),
                     "native setup CLI exits without starting the Lab");
}

/* Register weather installation checks before the suite's README examples. */
void test_cfg_weather_cases(void)
{
    TEST_RUN_IN_GROUP(test_cfg_weather_storage, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_weather_rejection, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_weather_command, tests, NULL);
}
