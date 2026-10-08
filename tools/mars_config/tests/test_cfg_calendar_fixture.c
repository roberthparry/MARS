/**
 * @file test_cfg_calendar_fixture.c
 * @brief One immutable encrypted installation for isolated calendar regression copies.
 *
 * Builds the packaged jurisdiction database once in a disposable child home, then
 * retains only its closed database in a private runner-owned directory. Ordinary
 * and README cases copy that read-only seed through file.h and receive freshly
 * quoted, home-specific saved configuration. Production installers and SQLCipher
 * security settings are unchanged. Explicit finalisation reports cleanup errors;
 * an owner-PID-guarded exit hook is a fallback for early runner termination.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "cfg_database.h"
#include "cfg_storage.h"
#include "file.h"
#include "test_cfg_calendar_fixture.h"
#include "test_cfg_support.h"
#include "test_harness.h"

static file_t *test_calendar_seed_directory;
static file_t *test_calendar_seed;
static pid_t test_calendar_seed_owner;
static bool test_calendar_seed_attempted;
static bool test_calendar_seed_ready;
static const char *test_calendar_seed_password = "calendar-regression-only";

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
              !cfg_database_run(true, string_c_str(path), test_calendar_seed_password, "Shrewsbury", "English", false);
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
    char directory[] = "/tmp/mars-calendar-seed-XXXXXX";
    if (!mkdtemp(directory))
        return false;
    test_calendar_seed_directory = file_new_cstr(directory);
    string_t *path = string_sprintf("%s/calendar.db", directory);
    test_calendar_seed = path ? file_new(path) : NULL;
    string_free(path);
    bool ok = test_calendar_seed_directory && test_calendar_seed && !atexit(test_calendar_seed_exit);
    if (ok)
        ok = test_cfg_isolated(test_calendar_seed_build);
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
    return callback && test_calendar_seed_prepare() && test_cfg_isolated(callback);
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
    string_t *config_directory = string_sprintf("%s/config", directory);
    string_t *config_path = string_sprintf("%s/config/jurisdiction-db.env", directory);
    file_t *config = config_path ? file_new(config_path) : NULL;
    string_t *quoted_path = *path ? cfg_storage_quote(*path) : NULL;
    string_t *quoted_key = *key ? cfg_storage_quote(*key) : NULL;
    string_t *body =
        quoted_path && quoted_key
            ? string_sprintf("# Generated by mars_config jurisdiction\nexport MARS_JURISDICTION_DB_PATH=%s\n"
                             "export MARS_JURISDICTION_DB_KEY=%s\nexport MARS_CALENDAR_LOCATION='Shrewsbury, GB-ENG'\n"
                             "export MARS_CALENDAR_LANGUAGE='en_GB'\n",
                             string_c_str(quoted_path), string_c_str(quoted_key))
            : NULL;
    bool ok = destination && config_directory && config && body && test_calendar_seed_environment() &&
              file_copy(test_calendar_seed, destination, false) && file_chmod(destination, 0600) &&
              cfg_storage_publish(config, config_directory, body);
    sqlite_t *db = ok ? sqlite_open_encrypted(*path, *key) : NULL;
    string_free(body);
    string_free(quoted_key);
    string_free(quoted_path);
    file_free(config);
    string_free(config_path);
    string_free(config_directory);
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
