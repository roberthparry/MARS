/**
 * @file test_format_locale.c
 * @brief Isolated jurisdiction catalogue fixture for Lingua interpolation regressions.
 *
 * Loads the maintained SQL catalogue into a disposable database and selects French independently of the host.
 * Saves and restores configuration, clears per-thread presentation snapshots and removes only its own files.
 * Stateful checks verify that immutable presentations survive closing their source engine.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "format-t.h"
#include "datetime-t.h"
#include "file-t.h"
#include "jurisdiction-t.h"
#include "sqlite-t.h"
#include "test_harness.h"
#define MARS_TEST_FORMAT_INTERNAL_ACCESS
#include "test_format_internal.h"

static file_t *locale_directory;
static file_t *locale_database;
static char *locale_saved[2];
static const char *const locale_environment[2] = {"MARS_JURISDICTION_DB_PATH", "MARS_JURISDICTION_DB_KEY"};
static bool locale_changed;

static bool locale_load_sql(sqlite_t *database, const char *path)
{
    file_t *file = file_new_cstr(path);
    string_t *sql = file ? file_read_all_text(file) : NULL;
    bool ok = sql && sqlite_exec(database, sql);
    string_free(sql);
    file_free(file);
    return ok;
}

bool test_format_locale_setup(void)
{
    for (size_t i = 0; i < 2u; ++i) {
        const char *value = getenv(locale_environment[i]);
        locale_saved[i] = value ? strdup(value) : NULL;
        if (value && !locale_saved[i])
            return false;
    }
    string_t *parent = string_new_with("/tmp");
    locale_directory = parent ? file_create_temp_directory(parent) : NULL;
    string_free(parent);
    string_t *path = locale_directory ? fmt_string("{directory}/locale.db", FMT(directory, file_path(locale_directory))) : NULL;
    locale_database = path ? file_new(path) : NULL;
    sqlite_t *database = path ? sqlite_open_plain(path) : NULL;
    string_free(path);
    bool ok = database && locale_load_sql(database, "packaging/jurisdiction-db/mars_calendar_locale_names.sql") &&
              locale_load_sql(database, "packaging/jurisdiction-db/mars_calendar_local.sql") &&
              sqlite_exec_cstr(database, "INSERT INTO calendar_local_settings VALUES "
                  "(1,'Fixture','GB-ENG','Europe/London',0,0,2015,2033,'fr_FR')");
    sqlite_close(database);
    if (!ok)
        return false;
    locale_changed = true;
    jurisdict_date_locale_cache_clear();
    return setenv(locale_environment[0], file_path(locale_database), 1) == 0 && unsetenv(locale_environment[1]) == 0;
}

void test_format_locale_cleanup(void)
{
    jurisdict_date_locale_cache_clear();
    for (size_t i = 0; i < 2u; ++i) {
        if (locale_changed) {
            if (locale_saved[i])
                (void)setenv(locale_environment[i], locale_saved[i], 1);
            else
                (void)unsetenv(locale_environment[i]);
        }
        free(locale_saved[i]);
        locale_saved[i] = NULL;
    }
    locale_changed = false;
    if (locale_database)
        (void)file_delete(locale_database);
    if (locale_directory)
        (void)file_remove_directory(locale_directory);
    file_free(locale_database);
    file_free(locale_directory);
    locale_database = locale_directory = NULL;
}

void test_format_locale_snapshot(void)
{
    jurisdiction_t *engine = jurisdict_open(NULL);
    jurisdict_date_locale_t *locale = jurisdict_date_locale_load(engine, "el_GR");
    TEST_ASSERT_TRUE(!jurisdict_date_locale_load(engine, "zz_ZZ") && *jurisdict_last_error(engine),
                     "unknown locales retain a catalogue diagnostic");
    jurisdict_close(engine);
    datetime_t *date = datetime_init_ymd(datetime_alloc(), 2026, DT_June, 10);
    string_t *text = jurisdict_date_locale_text(locale, date);
    TEST_ASSERT_TRUE(text && !strcmp(string_c_str(text), "10 Ιουνίου 2026"),
                     "immutable Greek presentation survives closing its source engine");
    string_free(text);
    datetime_dealloc(date);
    jurisdict_date_locale_free(locale);
}

void test_format_locale_cache(void)
{
    jurisdict_date_locale_cache_clear();
    datetime_t *date = datetime_init_ymd(datetime_alloc(), 2026, DT_June, 10);
    string_t *first = fmt_string("{date}", FMT(date));
    string_t *path = string_new_with(file_path(locale_database));
    sqlite_t *database = path ? sqlite_open_plain(path) : NULL;
    string_free(path);
    bool ok = first && !strcmp(string_c_str(first), "10 juin 2026") && database &&
              sqlite_exec_cstr(database, "UPDATE calendar_local_settings SET locale='cy_GB' WHERE singleton=1");
    string_t *cached = fmt_string("{date}", FMT(date));
    ok = ok && cached && !strcmp(string_c_str(cached), "10 juin 2026");
    jurisdict_date_locale_cache_clear();
    string_t *changed = fmt_string("{date}", FMT(date));
    ok = ok && changed && !strcmp(string_c_str(changed), "10 Mehefin 2026");
    if (database)
        ok = sqlite_exec_cstr(database, "UPDATE calendar_local_settings SET locale='fr_FR' WHERE singleton=1") && ok;
    sqlite_close(database);
    jurisdict_date_locale_cache_clear();
    string_t *missing_path = fmt_string("{directory}/absent.db", FMT(directory, file_path(locale_directory)));
    file_t *missing_file = missing_path ? file_new(missing_path) : NULL;
    ok = ok && missing_file && setenv(locale_environment[0], string_c_str(missing_path), 1) == 0;
    string_t *fallback = fmt_string("{date}", FMT(date));
    string_t *unavailable = fmt_string("{date:fr_FR}", FMT(date));
    ok = ok && fallback && !strcmp(string_c_str(fallback), "2026-06-10T00:00:00") && !unavailable &&
         fmt_last_error()->code == FMT_ERROR_TYPE && missing_file && !file_exists(missing_file);
    bool restored = setenv(locale_environment[0], file_path(locale_database), 1) == 0;
    jurisdict_date_locale_cache_clear();
    file_free(missing_file);
    string_free(missing_path);
    string_free(first);
    string_free(cached);
    string_free(changed);
    string_free(fallback);
    string_free(unavailable);
    datetime_dealloc(date);
    TEST_ASSERT_TRUE(ok && restored, "locale snapshots refresh explicitly; absent catalogues are never created");
}

void test_format_locale_catalogue(void)
{
    string_t *path = string_new_with(file_path(locale_database));
    sqlite_t *database = path ? sqlite_open_plain(path) : NULL;
    string_free(path);
    bool ok = database && sqlite_exec_cstr(database,
        "INSERT INTO calendar_local_days VALUES ('2026-06-10',NULL,0,0)");
    sqlite_stmt_t *locales = ok ? sqlite_stmt_prepare(database,
        "SELECT locale FROM calendar_locale_name_set ORDER BY locale") : NULL;
    sqlite_stmt_t *change = ok ? sqlite_stmt_prepare(database,
        "UPDATE calendar_local_settings SET locale=?1 WHERE singleton=1") : NULL;
    sqlite_stmt_t *reference = ok ? sqlite_stmt_prepare(database,
        "SELECT [Date Lingua] FROM calendar_local") : NULL;
    jurisdiction_t *engine = jurisdict_open(NULL);
    datetime_t *date = datetime_init_ymd(datetime_alloc(), 2026, DT_June, 10);
    ok = ok && locales && change && reference && engine && date;
    if (!ok)
        fprintf(stderr, "Lingua reference setup: %s\n", string_c_str(sqlite_last_error(database)));
    size_t count = 0;
    sqlite_step_result_t step = SQLITE_STEP_ERROR;
    while (ok && (step = sqlite_stmt_step(locales)) == SQLITE_STEP_ROW) {
        const char *code = sqlite_stmt_column_text(locales, 0);
        sqlite_stmt_reset(change);
        sqlite_stmt_reset(reference);
        ok = sqlite_stmt_bind_text(change, 1, code) && sqlite_stmt_step(change) == SQLITE_STEP_DONE &&
             sqlite_stmt_step(reference) == SQLITE_STEP_ROW;
        jurisdict_date_locale_t *locale = jurisdict_date_locale_load(engine, code);
        string_t *actual = jurisdict_date_locale_text(locale, date);
        const char *expected = ok ? sqlite_stmt_column_text(reference, 0) : NULL;
        ok = ok && actual && expected && !strcmp(string_c_str(actual), expected);
        if (!ok)
            fprintf(stderr, "Lingua %s: native=[%s]; SQL=[%s]; SQL error=%s\n", code,
                    actual ? string_c_str(actual) : "NULL", expected ? expected : "NULL",
                    string_c_str(sqlite_last_error(database)));
        if (!ok)
            test_set_failure_detailf("Lingua %s: native=[%s]; SQL=[%s]", code,
                                    actual ? string_c_str(actual) : "NULL", expected ? expected : "NULL");
        string_free(actual);
        jurisdict_date_locale_free(locale);
        ++count;
    }
    sqlite_stmt_finalize(locales);
    sqlite_stmt_finalize(change);
    sqlite_stmt_finalize(reference);
    if (database)
        (void)sqlite_exec_cstr(database, "UPDATE calendar_local_settings SET locale='fr_FR' WHERE singleton=1");
    sqlite_close(database);
    jurisdict_close(engine);
    datetime_dealloc(date);
    TEST_ASSERT_TRUE(ok && step == SQLITE_STEP_DONE && count > 100u,
                     "all catalogue locales match the independent SQL Lingua calendar view");
}
