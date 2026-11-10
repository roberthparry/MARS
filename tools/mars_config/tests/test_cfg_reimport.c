/**
 * @file test_cfg_reimport.c
 * @brief Isolated fresh-import, retry and one-time request regressions for release preparation.
 *
 * Real staged almanac and jurisdiction imports check replacement rather than appending. Invalid sources must
 * leave old files and the queued stage intact. Resumption skips completed stages and clears the final request.
 * Globe failure leaves its small installed fixture untouched; existing globe replacement regressions cover
 * successful publication and preservation of installed layers. No user configuration is accessed.
 */
#include <stdlib.h>
#include <string.h>

#include "string-t.h"
#include "file-t.h"
#include "sqlite-t.h"
#include "mars_settings.h"
#include "cfg_database.h"
#define MARS_CFG_DATABASE_INTERNAL_ACCESS
#include "cfg_database_internal.h"
#include "test_harness.h"
#include "test_cfg_support.h"

static bool request_import(const char *phase)
{
    string_t *json = string_sprintf("{\"MARS_RELEASE_DB_REIMPORT\":\"%s\"}", phase);
    bool ok = json && mars_settings_sync(json);
    string_free(json);
    return ok;
}

static bool request_is(const char *expected)
{
    string_t *saved = mars_settings_read("MARS_RELEASE_DB_REIMPORT");
    bool ok = saved && !strcmp(string_c_str(saved), expected);
    string_free(saved);
    return ok;
}

static bool write_source(file_t *file, const char *sql)
{
    string_t *text = string_new_with(sql);
    bool ok = text && file_write_all_text(file, text);
    string_free(text);
    return ok;
}

static bool has_replacement(const string_t *path)
{
    sqlite_t *db = sqlite_open_plain(path);
    sqlite_stmt_t *query = db ? sqlite_stmt_prepare(db,
        "SELECT (SELECT count(*) FROM replacement WHERE value=7)=1\n"
        "   AND NOT EXISTS (SELECT 1 FROM sqlite_schema WHERE name='obsolete')") : NULL;
    bool ok = query && sqlite_stmt_step(query) == SQLITE_STEP_ROW && sqlite_stmt_column_int(query, 0) == 1;
    sqlite_stmt_finalize(query); sqlite_close(db);
    return ok;
}

static bool reimport_fixture(const char *directory)
{
    static const char *const variables[] = {"MARS_GLOBE_DB_PATH", "MARS_ALMANAC_DB_PATH", "MARS_JURISDICTION_DB_PATH"};
    string_t *paths[3] = {0};
    file_t *files[3] = {0};
    bool ok = request_import("") && cfg_database_reimport_pending() == -1 &&
              !setenv("MARS_ROOT", directory, 1) && !setenv("MARS_ALMANAC_DB_KEY", "", 1) &&
              !setenv("MARS_GLOBE_DB_KEY", "", 1) && !setenv("MARS_JURISDICTION_DB_KEY", "", 1);
    for (unsigned i = 0; ok && i < 3; ++i) {
        paths[i] = string_sprintf("%s/store-%u.db", directory, i);
        files[i] = paths[i] ? file_new(paths[i]) : NULL;
        sqlite_t *db = paths[i] ? sqlite_open_plain(paths[i]) : NULL;
        ok = db && files[i] && !setenv(variables[i], string_c_str(paths[i]), 1) &&
             sqlite_exec_cstr(db, "CREATE TABLE obsolete(value); INSERT INTO obsolete VALUES(42);");
        sqlite_close(db);
    }
    string_t *globe_before = ok ? file_sha256(files[0]) : NULL;
    string_t *almanac_before = ok ? file_sha256(files[1]) : NULL;
    ok = ok && globe_before && almanac_before && request_import("4") && cfg_databases_ensure() == 1 && request_is("4") &&
         request_import("1") && cfg_databases_ensure() == 1 && request_is("1");
    string_t *folder_path = string_sprintf("%s/packaging/almanac-db", directory);
    file_t *folder = folder_path ? file_new(folder_path) : NULL;
    string_t *sql_path = folder_path ? string_sprintf("%s/mars_almanac.sql", string_c_str(folder_path)) : NULL;
    file_t *source = sql_path ? file_new(sql_path) : NULL;
    ok = ok && folder && source && file_create_directory(folder, 0700, true) &&
         write_source(source, "CREATE TABLE replacement(value); INVALID SQL;") && request_import("2") &&
         cfg_databases_ensure() == 1 && request_is("2");
    string_t *after = ok ? file_sha256(files[1]) : NULL;
    ok = ok && after && !string_compare(after, almanac_before);
    string_free(after);
    ok = ok && write_source(source, "CREATE TABLE replacement(value); INSERT INTO replacement VALUES(7);") &&
         cfg_databases_ensure() == 1 && request_is("3") && has_replacement(paths[1]);
    string_t *completed = ok ? file_sha256(files[1]) : NULL;
    ok = ok && completed && !setenv("MARS_ROOT", MARS_CONFIG_ROOT_DIR, 1) &&
         !setenv("MARS_CALENDAR_LOCATION", "Limmen, NL", 1) && !setenv("MARS_CALENDAR_LANGUAGE", "en_GB", 1) &&
         cfg_databases_ensure() == 0 && request_is("") && cfg_database_reimport_pending() == -1;
    after = ok ? file_sha256(files[1]) : NULL;
    ok = ok && after && !string_compare(after, completed) && has_replacement(paths[1]);
    string_free(after);
    after = ok ? file_sha256(files[0]) : NULL;
    ok = ok && after && !string_compare(after, globe_before);
    string_free(after);
    string_free(completed); string_free(globe_before); string_free(almanac_before);
    file_free(source); file_free(folder); string_free(sql_path); string_free(folder_path);
    for (unsigned i = 0; i < 3; ++i) { file_free(files[i]); string_free(paths[i]); }
    return ok;
}

static void test_cfg_release_reimport(void)
{
    TEST_ASSERT_TRUE(test_cfg_content_isolated(reimport_fixture),
                     "queued imports replace data, retain failed stages, resume completed stages and clear only on success");
}

/* Register the stateful importer regression before README examples. */
void test_cfg_reimport_cases(void)
{
    TEST_RUN_IN_GROUP(test_cfg_release_reimport, tests, NULL);
}
