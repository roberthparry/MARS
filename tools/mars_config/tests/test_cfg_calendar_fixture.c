/**
 * @file test_cfg_calendar_fixture.c
 * @brief One immutable plaintext installation for isolated calendar regression copies.
 *
 * Builds the packaged jurisdiction database once in a disposable child home, then
 * retains only its closed database in a private runner-owned directory. Ordinary
 * and README cases copy that read-only seed through the file module and receive freshly
 * encrypted, home-specific saved configuration. Catalogue copies use the normal plaintext
 * storage mode; separate installer regressions exercise encrypted content. Production installers and SQLCipher
 * security settings are unchanged. Explicit finalisation reports cleanup errors;
 * an owner-PID-guarded exit hook is a fallback for early runner termination.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "file-t.h"
#include "mars_settings.h"
#include "cfg_storage.h"
#include "cfg_database.h"
#include "test_harness.h"
#include "test_cfg_support.h"
#include "test_cfg_calendar_fixture.h"

static file_t *test_calendar_seed_directory;
static file_t *test_calendar_seed;
static pid_t test_calendar_seed_owner;
static bool test_calendar_seed_attempted;
static bool test_calendar_seed_ready;
static const char *test_calendar_seed_password = "";

static bool test_calendar_seed_environment(void)
{
    return !setenv("MARS_ROOT", MARS_CONFIG_ROOT_DIR, 1) && !unsetenv("MARS_CALENDAR_LOCATION_ARGUMENT") &&
           !unsetenv("MARS_CALENDAR_LANGUAGE_ARGUMENT") && !unsetenv("MARS_HOLIDAY_JURISDICTION") &&
           !unsetenv("LC_ALL") && !unsetenv("LC_MESSAGES") && !setenv("LANG", "en_GB.UTF-8", 1);
}

static bool test_calendar_seed_remove(void)
{
    if (!test_calendar_seed_owner || test_calendar_seed_owner != getpid())
        return true;
    bool ok = true;
    if (test_calendar_seed) {
        bool exists = file_exists(test_calendar_seed);
        ok = exists ? file_delete(test_calendar_seed) : !file_last_error(test_calendar_seed);
    }
    if (ok && test_calendar_seed_directory)
        ok = file_remove_directory(test_calendar_seed_directory);
    if (ok) {
        file_free(test_calendar_seed);
        file_free(test_calendar_seed_directory);
        test_calendar_seed = NULL;
        test_calendar_seed_directory = NULL;
        test_calendar_seed_ready = false;
        test_calendar_seed_owner = 0;
    }
    return ok;
}

static void test_calendar_seed_exit(void)
{
    if (!test_calendar_seed_remove())
        fputs("Cannot remove the private calendar regression seed.\n", stderr);
}

static bool test_calendar_seed_build(const char *directory)
{
    string_t *path = string_sprintf("%s/calendar.db", directory);
    bool ok = path && test_calendar_seed_environment() &&
              !cfg_database_run(true, string_c_str(path), test_calendar_seed_password, "Shrewsbury, GB-ENG",
                                "English", false);
    file_t *source = path ? file_new(path) : NULL;
    /* cfg_database_run has closed, checkpointed and published its complete database. */
    ok = ok && source && test_calendar_seed && file_copy(source, test_calendar_seed, false) &&
         file_chmod(test_calendar_seed, 0400);
    file_free(source);
    string_free(path);
    return ok;
}

static bool test_calendar_seed_prepare(void)
{
    if (test_calendar_seed_attempted)
        return test_calendar_seed_ready && test_calendar_seed_owner == getpid();
    test_calendar_seed_attempted = true;
    test_calendar_seed_owner = getpid();
    const char *temporary = getenv("TMPDIR");
    string_t *parent = string_new_with(temporary && *temporary ? temporary : "/tmp");
    test_calendar_seed_directory = parent ? file_create_temp_directory(parent) : NULL;
    string_free(parent);
    if (!test_calendar_seed_directory)
        return false;
    string_t *path = string_sprintf("%s/calendar.db", file_path(test_calendar_seed_directory));
    test_calendar_seed = path ? file_new(path) : NULL;
    string_free(path);
    bool ok = test_calendar_seed_directory && test_calendar_seed && !atexit(test_calendar_seed_exit);
    if (ok)
        ok = test_cfg_content_isolated(test_calendar_seed_build);
    test_calendar_seed_ready = ok;
    if (!ok) {
        fputs("Cannot prepare the shared calendar regression seed; it will not be rebuilt for each case.\n", stderr);
        if (!test_calendar_seed_remove())
            fputs("Cannot clean up the failed calendar regression seed.\n", stderr);
    }
    return ok;
}

/* Prepare in the runner, so its immutable pathname survives all forked test callbacks. */
bool test_cfg_calendar_isolated(bool (*callback)(const char *directory))
{
    return callback && test_calendar_seed_prepare() && test_cfg_content_isolated(callback);
}

/* Each test owns its copy and saved choices; neither can modify the shared source. */
sqlite_t *test_cfg_calendar_database(const char *directory, string_t **path, string_t **key)
{
    if (!path || !key || path == key)
        return NULL;
    *path = NULL;
    *key = NULL;
    if (!directory || !test_calendar_seed_ready || !test_calendar_seed || test_calendar_seed_owner == getpid())
        return NULL;
    *path = string_sprintf("%s/calendar.db", directory);
    *key = string_new_with(test_calendar_seed_password);
    file_t *destination = *path ? file_new(*path) : NULL;
    string_t *body = *path && *key ? string_sprintf(
        "{\"MARS_JURISDICTION_DB_PATH\":\"%s\",\"MARS_JURISDICTION_DB_KEY\":\"%s\","
        "\"MARS_CALENDAR_LOCATION\":\"Shrewsbury, GB-ENG\",\"MARS_CALENDAR_LANGUAGE\":\"en_GB\"}",
        string_c_str(*path), string_c_str(*key)) : NULL;
    bool ok = destination && body && test_calendar_seed_environment() &&
              file_copy(test_calendar_seed, destination, false) && file_chmod(destination, 0600) &&
              mars_settings_sync(body);
    sqlite_t *db = ok ? sqlite_open_plain(*path) : NULL;
    if (db && !sqlite_exec_cstr(db, "PRAGMA cache_size=-8192; PRAGMA temp.cache_size=-512;")) {
        sqlite_close(db);
        db = NULL;
    }
    string_free(body);
    file_free(destination);
    if (!db) {
        string_free(*path);
        string_free(*key);
        *path = NULL;
        *key = NULL;
    }
    return db;
}

/* The parent runner calls this after all README groups, before calculating its final status. */
void test_cfg_calendar_fixture_finish(void)
{
    TEST_ASSERT_TRUE(test_calendar_seed_remove(), "remove immutable calendar regression fixture");
}

/* Selection cases need the core town catalogue, complete languages and real schema/manual rules.
 * The immutable calendar seed separately retains the full worldwide-town and generated-rule imports. */
bool test_cfg_calendar_sources(const char *directory)
{
    string_t *root = string_sprintf("%s/selection-sources", directory);
    string_t *folder_path = root ? string_sprintf("%s/packaging/jurisdiction-db", string_c_str(root)) : NULL;
    file_t *folder = folder_path ? file_new(folder_path) : NULL;
    bool ok = folder && file_create_directory(folder, 0700, true);
    static const char *const sources[] = {"mars_holiday_rules.sql",
                                          "mars_country_jurisdictions.sql",
                                          "mars_target_subdivisions.sql",
                                          "mars_timezone_rules.sql",
                                          "mars_jurisdiction_location_defaults.sql",
                                          "mars_jurisdiction_towns.sql",
                                          "town_catalogue_seed.sql",
                                          "location_additions.sql",
                                          "location_subdivisions.sql",
                                          "town_search.sql",
                                          "mars_manual_first_class_rules.sql",
                                          "mars_holiday_localized_names.sql",
                                          "mars_calendar_locale_names.sql",
                                          "mars_calendar_local.sql"};
    for (size_t i = 0; i < sizeof(sources) / sizeof(*sources) && ok; ++i) {
        string_t *source_path = string_sprintf("%s/packaging/jurisdiction-db/%s", MARS_CONFIG_ROOT_DIR, sources[i]);
        string_t *target_path = string_sprintf("%s/%s", string_c_str(folder_path), sources[i]);
        file_t *source = source_path ? file_new(source_path) : NULL;
        file_t *target = target_path ? file_new(target_path) : NULL;
        ok = source && target && file_copy(source, target, false);
        file_free(source);
        file_free(target);
        string_free(source_path);
        string_free(target_path);
    }
    static const char *const omitted[] = {
        "mars_generated_first_class_rules.sql", "mars_jurisdiction_world_towns.sql"
    };
    string_t *empty = string_new_with("-- Core selection fixture; the complete calendar seed covers bulk imports.\n");
    for (size_t i = 0; ok && i < sizeof omitted / sizeof *omitted; ++i) {
        string_t *path = string_sprintf("%s/%s", string_c_str(folder_path), omitted[i]);
        file_t *file = path ? file_new(path) : NULL;
        ok = file && empty && file_write_all_text(file, empty);
        file_free(file); string_free(path);
    }
    ok = ok && !setenv("MARS_ROOT", string_c_str(root), 1);
    string_free(empty);
    file_free(folder);
    string_free(folder_path);
    string_free(root);
    return ok;
}
