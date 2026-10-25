/**
 * @file test_cfg_paths.c
 * @brief Isolated relocation, encryption and settings-precedence regressions.
 *
 * Uses a native key-provider fixture rather than a real desktop keyring. Verifies that a tiny JSON locator
 * can move an encrypted configuration independently of resource directories and the current working directory.
 */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "file-t.h"
#include "sqlite-t.h"
#include "mars_paths.h"
#include "mars_settings.h"
#include "lab_process.h"
#include "test_harness.h"
#include "test_cfg_support.h"

static string_t *paths_test_key(bool create)
{
    (void)create;
    return string_new_with("isolated-configuration-regression-key");
}

static bool paths_requested_new_key;

static string_t *paths_unavailable_key(bool create)
{
    paths_requested_new_key = create;
    return NULL;
}

static bool paths_write(const string_t *path, const char *value)
{
    file_t *file = path ? file_new(path) : NULL;
    string_t *text = string_new_with(value);
    bool ok = file && text && file_write_all_text(file, text);
    string_free(text); file_free(file);
    return ok;
}

static bool paths_relocation(const char *directory)
{
    mars_settings_key_provider(paths_test_key);
    string_t *parent = string_sprintf("%s/bootstrap", directory), *locator = string_sprintf("%s/mars.json", string_c_str(parent));
    file_t *folder = file_new(parent);
    bool ok = folder && file_create_directory(folder, 0700, true) &&
              paths_write(locator, "{\"database\":\"../settings/mars.db\"}\n") &&
              !setenv("MARS_CONFIG_FILE", string_c_str(locator), 1) && !unsetenv("MARS_ROOT") &&
              !unsetenv("MARS_LAB_ASSET_DIR") && !unsetenv("MARS_GLOBE_DB_PATH");
    char *resource[] = {"set", "lab_assets", "../assets with spaces"};
    char *setting[] = {"set", "MARS_GLOBE_DB_PATH", "world/globe.db"};
    char *secret[] = {"set", "MARS_WEATHER_API_KEY", "fixture-only-key"};
    ok = ok && !mars_paths_command(3, resource) && !mars_settings_command(3, setting) &&
         !mars_settings_command(3, secret) && !chdir("/");
    string_t *path = mars_path_resource(MARS_PATH_ASSETS, NULL, "index.html");
    string_t *expected = string_sprintf("%s/bootstrap/../settings/../assets with spaces/index.html", directory);
    ok = ok && path && !string_compare(path, expected);
    string_t *database = mars_settings_database();
    file_t *file = database ? file_new(database) : NULL;
    unsigned char header[16]; size_t count = 0;
    ok = ok && file && file_open_read(file) && file_read(file, header, sizeof header, &count) && count == sizeof header &&
         memcmp(header, "SQLite format 3", 15);
    file_close(file);
    string_t *wrong_key = string_new_with("incorrect-fixture-key");
    sqlite_t *wrong = sqlite_open_encrypted(database, wrong_key);
    sqlite_stmt_t *query = wrong ? sqlite_stmt_prepare(wrong, "select name from sqlite_master") : NULL;
    ok = ok && (!query || sqlite_stmt_step(query) != SQLITE_STEP_ROW);
    sqlite_stmt_finalize(query); sqlite_close(wrong); string_free(wrong_key);
    ok = ok && !setenv("MARS_WEATHER_API_KEY", "explicit", 1) && mars_settings_load() &&
         !strcmp(getenv("MARS_WEATHER_API_KEY"), "explicit");
    string_t *globe = string_sprintf("%s/bootstrap/../settings/world/globe.db", directory);
    ok = ok && getenv("MARS_GLOBE_DB_PATH") && !strcmp(getenv("MARS_GLOBE_DB_PATH"), string_c_str(globe));
    string_t *moved_path = string_sprintf("%s/relocated", directory);
    file_t *moved_directory = file_new(moved_path);
    string_t *new_database = string_sprintf("%s/new.db", string_c_str(moved_path));
    file_t *moved = file_new(new_database);
    ok = ok && moved_directory && file_create_directory(moved_directory, 0700, true) && file_move(file, moved, false) &&
         paths_write(locator, "{\"database\":\"../relocated/new.db\"}\n");
    string_free(path); path = mars_path_resource(MARS_PATH_ASSETS, NULL, "index.html");
    string_free(expected); expected = string_sprintf("%s/bootstrap/../relocated/../assets with spaces/index.html", directory);
    ok = ok && path && !string_compare(path, expected);
    ok = ok && !setenv("MARS_LAB_ASSET_DIR", directory, 1);
    string_free(path); path = mars_path_resource(MARS_PATH_ASSETS, NULL, "index.html");
    string_free(expected); expected = string_sprintf("%s/index.html", directory);
    ok = ok && path && !string_compare(path, expected);
    ok = ok && paths_write(locator, "{broken json") && !mars_settings_load();
    const char *offline[] = {MARS_CONFIG_PROGRAM, "globe-resume-rivers", "missing-input", "missing-candidate", NULL};
    string_t *diagnostic = NULL;
    int status = -1;
    ok = ok && lab_proc_run(offline, directory, 5000, 4096, &diagnostic, &status) && status == 1 && diagnostic &&
         string_find(diagnostic, "Cannot read MARS configuration") < 0;
    string_free(diagnostic);
    ok = ok && paths_write(locator, "{\"database\":\"missing.db\"}\n") && !mars_settings_load();
    ok = ok && paths_write(locator, "{\"database\":\"../relocated/new.db\"}\n");
    mars_settings_key_provider(paths_unavailable_key);
    sqlite_t *locked = mars_settings_open(true);
    ok = ok && !locked && !paths_requested_new_key;
    sqlite_close(locked);
    file_free(moved); file_free(moved_directory); file_free(file); file_free(folder);
    string_free(new_database); string_free(moved_path); string_free(globe); string_free(database);
    string_free(expected); string_free(path); string_free(parent); string_free(locator);
    return ok;
}

static bool paths_literal_import(const char *directory)
{
    mars_settings_key_provider(paths_test_key);
    bool ok = !unsetenv("MARS_CONFIG_FILE") && mars_settings_initialise();
    string_t *state = string_sprintf("%s/old state/current.json", directory);
    string_t *cache = string_sprintf("%s/old cache/objects.db", directory);
    char *migrate[] = {"migrate"};
    ok = ok && state && cache && !setenv("MARS_LAB_STATE_FILE", string_c_str(state), 1) &&
         !setenv("MARS_LAB_OBJECT_STORE_PATH", string_c_str(cache), 1) && !mars_settings_command(1, migrate);
    string_t *text = string_new_with("export MARS_WEATHER_API_KEY='literal $(not-a-command)'\n"
                                     "export MARS_ALMANAC_DB_PATH='data/almanac.db'\n");
    ok = ok && mars_settings_import(text, false);
    string_free(text); text = string_new_with("MARS_WEATHER_API_KEY='replacement'\nINVALID_KEY='bad'\n");
    ok = ok && !mars_settings_import(text, true) && !unsetenv("MARS_WEATHER_API_KEY") && mars_settings_load() &&
         !strcmp(getenv("MARS_WEATHER_API_KEY"), "literal $(not-a-command)");
    ok = ok && !unsetenv("MARS_LAB_OBJECT_STORE_PATH") && !unsetenv("MARS_LAB_HISTORY_FILE") && mars_settings_load();
    string_t *history = string_sprintf("%s/old state/mars_lab_history.db", directory);
    ok = ok && getenv("MARS_LAB_HISTORY_FILE") && !strcmp(getenv("MARS_LAB_HISTORY_FILE"), string_c_str(history)) &&
         getenv("MARS_LAB_OBJECT_STORE_PATH") && !strcmp(getenv("MARS_LAB_OBJECT_STORE_PATH"), string_c_str(cache));
    string_free(history); string_free(cache); string_free(state);
    string_free(text);
    return ok;
}

static void test_cfg_paths_relocation(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(paths_relocation),
                     "encrypted settings relocate through JSON without checkout or cwd dependencies");
}

static void test_cfg_settings_import(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(paths_literal_import),
                     "literal imports preserve credentials and roll back invalid assignments");
}

/* Register configuration regressions before documented examples. */
void test_cfg_paths_cases(void)
{
    TEST_RUN_IN_GROUP(test_cfg_paths_relocation, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_settings_import, tests, NULL);
}
