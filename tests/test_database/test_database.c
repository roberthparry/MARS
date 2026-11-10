/**
 * @file test_database.c
 * @brief Verify lazy test storage, lossless catalogue synchronisation, selection snapshots and isolated fixtures.
 *
 * Exercises test-only storage through its support API, with raw SQLite reference queries and deliberate private
 * transaction fixtures. Each case owns a disposable repository and database. Shared database selection is disabled;
 * optional reporting occurs afterwards and cannot change the console result. The testing guide example runs last.
 */
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <wchar.h>

#include "array-t.h"
#include "file-t.h"
#include "string-t.h"
#include "test_harness.h"
#include "test_database.h"
#define MARS_SHARED_TEST_DATABASE_INTERNAL_ACCESS
#include "internal/test_database_internal.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);
/* Keep the test-management implementation independent of its own reporting hooks. */
bool test_suite_database_selection_enabled(void)
{
    return false;
}

typedef struct {
    string_t *root;
    string_t *path;
    test_database_t *database;
} database_fixture_t;

static bool write_text(const string_t *path, const char *text)
{
    file_t *file = file_new(path);
    string_t *content = string_new();
    bool ok = file && content && !string_append_bytes(content, text, strlen(text)) &&
              file_write_all_text(file, content);
    string_free(content);
    file_free(file);
    return ok;
}

static bool configure(database_fixture_t *fixture, const char *json)
{
    string_t *path = string_sprintf("%s/tests/test_config.json", string_c_str(fixture->root));
    bool ok = path && write_text(path, json);
    string_free(path);
    return ok;
}

static void fixture_close(database_fixture_t *fixture)
{
    test_database_close(fixture->database);
    string_free(fixture->root);
    string_free(fixture->path);
    *fixture = (database_fixture_t){0};
}

static bool fixture_open(database_fixture_t *fixture, bool open_database)
{
    const char *root = getenv("MARS_ROOT");
    fixture->root = string_new_with(test_case_temp_dir());
    fixture->path = string_sprintf("%s/test.db", test_case_temp_dir());
    string_t *source_root = string_new_with(root && *root ? root : ".");
    bool ok = fixture->root && fixture->path && source_root &&
              test_database_sources_copy(source_root, fixture->root) &&
              configure(fixture, "{\"tests/example.c\":{\"group\":{\"enabled\":true,\"original\":false}}}");
    if (ok && open_database)
        ok = (fixture->database = test_database_open(fixture->root, fixture->path, true, NULL)) != NULL;
    string_free(source_root);
    if (!ok)
        fixture_close(fixture);
    return ok;
}

static sqlite3_int64 scalar(database_fixture_t *fixture, const char *sql)
{
    sqlite3_stmt *statement = test_database_prepare(fixture->database, sql);
    sqlite3_int64 value = statement && sqlite3_step(statement) == SQLITE_ROW ? sqlite3_column_int64(statement, 0) : -1;
    sqlite3_finalize(statement);
    return value;
}

static test_database_result_t result_record(void)
{
    return (test_database_result_t){.source = "tests/example.c", .path = "group.original", .name = "original",
        .parent = "group", .tags = "fixture", .outcome = "pass", .enabled = true, .elapsed_ms = 2.5};
}

static void test_catalogue_sync_preserves_every_identity(void)
{
    database_fixture_t fixture = {0};
    TEST_ASSERT_TRUE(fixture_open(&fixture, true), "create isolated storage");
    test_database_t *database = fixture.database;
    test_database_result_t record = result_record();
    bool ok = test_database_import(database, fixture.root, "saved", true) &&
              test_database_run_begin(database, "fixture", "saved", false) &&
              test_database_run_record(database, &record) && test_database_run_finish(database, 0, 3.0) &&
              test_database_exec(database,
                  "UPDATE expression_cases SET expected = 'preserved edit' WHERE name = 'exact_addition';\n"
                  "INSERT INTO expression_cases(name, operation, input, expected, enabled)\n"
                  "VALUES ('user_case', 'evaluate', '2+2', '4', 0);");
    sqlite3_int64 before = scalar(&fixture, "SELECT count(*) FROM test_catalogue");
    ok = ok && configure(&fixture, "{\"tests/example.c\":{\"group\":{\"renamed\":true}},"
                                   "\"tests/other.c\":{\"group\":{\"original\":true}}}") &&
         test_database_import(database, fixture.root, NULL, true) &&
         test_database_import(database, fixture.root, NULL, true) &&
         scalar(&fixture, "SELECT count(*) FROM test_catalogue") >= before + 4 &&
         scalar(&fixture, "SELECT confirmed FROM test_catalogue WHERE source = 'tests/example.c'"
                          " AND path = 'group.original'") == 0 &&
         scalar(&fixture, "SELECT confirmed FROM test_catalogue WHERE source = 'tests/other.c'"
                          " AND path = 'group.original'") == 1 &&
         scalar(&fixture, "SELECT enabled FROM test_selection WHERE profile = 'saved'"
                          " AND source = 'tests/example.c' AND path = 'group.original'") == 0 &&
         scalar(&fixture, "SELECT count(*) FROM test_result") == 1 &&
         scalar(&fixture, "SELECT count(*) FROM expression_cases WHERE name = 'user_case' AND enabled = 0") == 1;
    sqlite3_int64 after = scalar(&fixture, "SELECT count(*) FROM test_catalogue");
    ok = ok && configure(&fixture, "{}") && test_database_import(database, fixture.root, NULL, true) &&
         scalar(&fixture, "SELECT count(*) FROM test_catalogue") == after;
    test_database_close(fixture.database);
    fixture.database = test_database_open(fixture.root, fixture.path, true, NULL);
    ok = ok && fixture.database && scalar(&fixture,
        "SELECT count(*) FROM expression_cases WHERE name = 'exact_addition' AND expected = 'preserved edit'") == 1;
    fixture_close(&fixture);
    TEST_ASSERT_TRUE(ok, "renames, absent sources, empty imports, disabled rows, user edits and history survive sync");
}

static void test_catalogue_failure_rolls_back(void)
{
    database_fixture_t fixture = {0};
    TEST_ASSERT_TRUE(fixture_open(&fixture, true), "create isolated storage");
    bool ok = test_database_import(fixture.database, fixture.root, "snapshot", true);
    sqlite3_int64 count = scalar(&fixture, "SELECT count(*) FROM test_catalogue");
    ok = ok && configure(&fixture, "{\"tests/example.c\":{\"new_case\":true,\"bad_case\":7}}") &&
         !test_database_import(fixture.database, fixture.root, "snapshot", true) &&
         scalar(&fixture, "SELECT count(*) FROM test_catalogue") == count &&
         scalar(&fixture, "SELECT count(*) FROM test_catalogue WHERE path = 'new_case'") == 0 &&
         scalar(&fixture, "SELECT enabled FROM test_selection WHERE profile = 'snapshot'"
                          " AND path = 'group.original'") == 0 &&
         scalar(&fixture, "SELECT count(*) FROM test_catalogue WHERE confirmed = 0") == 0;
    fixture_close(&fixture);
    TEST_ASSERT_TRUE(ok, "failed sync restores previous catalogue, confirmation and complete profile snapshot");
}

static void test_profile_selection_snapshot(void)
{
    database_fixture_t fixture = {0};
    TEST_ASSERT_TRUE(fixture_open(&fixture, true), "create isolated storage");
    test_database_t *database = fixture.database;
    bool ok = test_database_profile_create(database, "select 'one'", false) &&
              test_database_profile_set(database, "select 'one'", "a.c", "outer.inner.case", true) &&
              test_database_profile_set(database, "select 'one'", "a.c", "outer.blocked", false) &&
              test_database_profile_set(database, "select 'one'", "a.c", "outer.blocked.child", true) &&
              test_database_profile_set(database, "select 'one'", "a.c", "outer.inner.other", true) &&
              test_database_profile_create(database, "select 'one'", true);
    test_profile_t *profile = ok ? test_database_profile_load(database, "select 'one'") : NULL;
    ok = ok && profile && !test_database_profile_load(database, "missing");
    test_database_close(fixture.database);
    fixture.database = NULL;
    ok = ok && test_profile_enabled(profile, "a.c", "outer") &&
         test_profile_enabled(profile, "a.c", "outer.inner") &&
         test_profile_enabled(profile, "a.c", "outer.inner.case") &&
         !test_profile_enabled(profile, "a.c", "outer.inner.unselected") &&
         !test_profile_enabled(profile, "a.c", "outer.blocked.child") &&
         !test_profile_enabled(profile, "b.c", "outer.inner.case");
    test_profile_free(profile);
    fixture_close(&fixture);
    TEST_ASSERT_TRUE(ok, "dictionary snapshot survives close, honours disabled parents and separates source identities");
}

static void test_run_report_transactions_and_limits(void)
{
    database_fixture_t fixture = {0};
    TEST_ASSERT_TRUE(fixture_open(&fixture, true), "create isolated storage");
    test_database_result_t record = result_record();
    bool ok = test_database_run_begin(fixture.database, "unfinished", NULL, false) &&
              test_database_run_record(fixture.database, &record);
    test_database_close(fixture.database);
    fixture.database = test_database_open(fixture.root, fixture.path, false, NULL);
    ok = ok && fixture.database && scalar(&fixture, "SELECT count(*) FROM test_run") == 0;
    ok = ok && scalar(&fixture, "SELECT count(*) FROM test_case_statistics") == 0 &&
         scalar(&fixture, "SELECT count(*) FROM test_timing_sample") == 0;
    char detail[5001];
    memset(detail, 'x', sizeof detail - 1u);
    detail[sizeof detail - 1u] = '\0';
    record.detail = detail;
    record.outcome = "fail";
    for (int i = 0; ok && i < 201; ++i)
        ok = test_database_run_begin(fixture.database, "fixture", NULL, false) &&
             test_database_run_record(fixture.database, &record) && test_database_run_finish(fixture.database, 1, 4.0);
    ok = ok && scalar(&fixture, "SELECT count(*) FROM test_run") == 200 &&
         scalar(&fixture, "SELECT count(*) FROM test_result WHERE outcome = 'fail' AND length(detail) = 4096") == 200 &&
         scalar(&fixture, "SELECT count(*) FROM test_run WHERE exit_code = 1 AND elapsed_ms = 4"
                          " AND started_utc <= finished_utc") == 200 &&
         scalar(&fixture, "SELECT normal_count FROM test_case_metrics WHERE path = 'group.original'") == 201 &&
         scalar(&fixture, "SELECT sum(sample_count) FROM test_timing_sample") == 201 &&
         scalar(&fixture, "PRAGMA max_page_count") * scalar(&fixture, "PRAGMA page_size") <= 67108864;
    ok = ok && test_database_run_begin(fixture.database, "many-results", NULL, false) &&
         test_database_exec(fixture.database,
             "WITH RECURSIVE indices(n) AS (VALUES(0) UNION ALL SELECT n+1 FROM indices WHERE n < 50000)\n"
             "INSERT INTO test_result(run_id, ordinal, source, path, outcome, enabled, elapsed_ms)\n"
             "SELECT (SELECT max(id) FROM test_run), n, 'case.c', 'many', 'skip', 0, 0 FROM indices;") &&
         test_database_run_finish(fixture.database, 0, 1.0) &&
         scalar(&fixture, "SELECT count(*) FROM test_result") == 50000 &&
         scalar(&fixture, "SELECT normal_count = 201 AND median_ms = 2.5 AND mean_ms = 2.5"
                          " FROM test_case_metrics WHERE path = 'group.original'") == 1 &&
         scalar(&fixture, "SELECT count(*) FROM test_result WHERE outcome = 'skip'") == 50000 &&
         scalar(&fixture, "SELECT count(*) FROM pragma_foreign_key_check") == 0;
    fixture_close(&fixture);
    TEST_ASSERT_TRUE(ok, "unfinished reports roll back; bounded history retains outcomes, durations and referential integrity");
}

static void test_fixture_and_unrelated_database_protection(void)
{
    database_fixture_t fixture = {0};
    TEST_ASSERT_TRUE(fixture_open(&fixture, false), "prepare isolated repository without opening storage");
    bool ok = test_database_fixture(fixture.root, fixture.path, NULL) &&
              !test_database_fixture(fixture.root, fixture.path, NULL);
    file_t *file = file_new(fixture.path);
    unsigned char header[16] = {0};
    size_t count = 0u;
    ok = ok && file && file_open_read(file) && file_read(file, header, sizeof header, &count) &&
         count == sizeof header && !memcmp(header, "SQLite format 3", 16u);
    file_free(file);
    string_t *unrelated = string_sprintf("%s/unrelated.db", string_c_str(fixture.root));
    sqlite3 *reference = NULL;
    ok = ok && unrelated && sqlite3_open(string_c_str(unrelated), &reference) == SQLITE_OK &&
         sqlite3_exec(reference, "CREATE TABLE precious(value TEXT); INSERT INTO precious VALUES ('keep');",
                      NULL, NULL, NULL) == SQLITE_OK;
    sqlite3_close(reference);
    test_database_t *rejected = test_database_open(fixture.root, unrelated, true, NULL);
    ok = ok && !rejected;
    test_database_close(rejected);
    sqlite3_stmt *statement = NULL;
    reference = NULL;
    ok = ok && sqlite3_open(string_c_str(unrelated), &reference) == SQLITE_OK &&
         sqlite3_prepare_v2(reference, "SELECT value FROM precious", -1, &statement, NULL) == SQLITE_OK &&
         sqlite3_step(statement) == SQLITE_ROW && !strcmp((const char *)sqlite3_column_text(statement, 0), "keep");
    sqlite3_finalize(statement);
    sqlite3_close(reference);
    string_free(unrelated);
    fixture_close(&fixture);
    TEST_ASSERT_TRUE(ok, "fixtures are plain SQLite; existing fixture destinations and unrelated data are never overwritten");
}

static bool report_sample(database_fixture_t *fixture, double milliseconds, const char *outcome,
                           const char *finished_utc, bool memory_test)
{
    test_database_result_t result = result_record();
    result.elapsed_ms = milliseconds;
    result.outcome = outcome;
    result.finished_utc = finished_utc;
    return test_database_run_begin(fixture->database, "statistics-fixture", NULL, memory_test) &&
           test_database_run_record(fixture->database, &result) &&
           test_database_run_finish(fixture->database, !strcmp(outcome, "fail"), milliseconds);
}

static void test_lifetime_statistics_exclude_memory_and_skips(void)
{
    database_fixture_t fixture = {0};
    TEST_ASSERT_TRUE(fixture_open(&fixture, true), "prepare isolated lifetime statistics");
    bool ok = report_sample(&fixture, 10, "pass", "2024-01-01T00:00:00.000Z", false) &&
              report_sample(&fixture, 20, "pass", "2024-01-02T00:00:00.000Z", false) &&
              scalar(&fixture, "SELECT median_ms = 15 AND mean_ms = 15 AND took_longer = 1"
                               " FROM test_case_metrics WHERE path = 'group.original'") == 1 &&
              report_sample(&fixture, 50, "fail", "2024-01-03T00:00:00.000Z", false) &&
              report_sample(&fixture, 30, "pass", "2024-01-04T00:00:00.000Z", false) &&
              scalar(&fixture, "SELECT median_ms = 25 AND mean_ms = 27.5 AND fastest_ms = 10 AND slowest_ms = 50"
                               " AND previous_normal_ms = 50 AND change_ms = -20 AND took_longer = 0"
                               " FROM test_case_metrics WHERE path = 'group.original'") == 1 &&
              report_sample(&fixture, 9000, "pass", "2024-01-05T00:00:00.000Z", true) &&
              report_sample(&fixture, 0, "skip", "2024-01-06T00:00:00.000Z", false) &&
              scalar(&fixture, "SELECT run_count = 5 AND normal_count = 4 AND memory_test_count = 1"
                               " AND median_ms = 25 AND mean_ms = 27.5 AND last_normal_ms = 30"
                               " AND last_elapsed_ms = 9000 AND last_outcome = 'pass'"
                               " AND last_memory_test_utc = '2024-01-05T00:00:00.000Z'"
                               " AND last_run_utc = last_memory_test_utc"
                               " FROM test_case_metrics WHERE path = 'group.original'") == 1 &&
              report_sample(&fixture, 40, "pass", "2024-01-07T00:00:00.000Z", false) &&
              scalar(&fixture, "SELECT median_ms = 30 AND mean_ms = 30 AND normal_count = 5"
                               " AND previous_normal_ms = 30 AND change_ms = 10 AND took_longer = 1"
                               " FROM test_case_metrics WHERE path = 'group.original'") == 1 &&
              report_sample(&fixture, 5, "pass", "2023-12-31T00:00:00.000Z", false) &&
              scalar(&fixture, "SELECT median_ms = 25 AND last_normal_ms = 40 AND previous_normal_ms = 30"
                               " AND fastest_ms = 5 AND slowest_ms = 50"
                               " FROM test_case_metrics WHERE path = 'group.original'") == 1 &&
              report_sample(&fixture, 0, "pass", "2024-01-08T00:00:00.000Z", false) &&
              report_sample(&fixture, 1, "pass", "2024-01-09T00:00:00.000Z", false) &&
              scalar(&fixture, "SELECT fastest_ms = 0 AND median_ms = 15 AND took_longer = 1"
                               " AND previous_normal_ms = 0 AND change_percent IS NULL"
                               " FROM test_case_metrics WHERE path = 'group.original'") == 1;
    test_database_result_t memory_only = result_record();
    memory_only.path = "only_memory";
    memory_only.name = "only_memory";
    memory_only.finished_utc = "2024-02-01T12:00:00.000Z";
    ok = ok && test_database_run_begin(fixture.database, "memory-marker-fixture", NULL, true) &&
         test_database_run_record(fixture.database, &memory_only) &&
         test_database_run_finish(fixture.database, 0, 2.5) &&
         scalar(&fixture, "SELECT normal_count = 0 AND memory_test_count = 1 AND median_ms IS NULL"
                          " AND mean_ms IS NULL AND fastest_ms IS NULL AND slowest_ms IS NULL"
                          " FROM test_case_metrics WHERE path = 'only_memory'") == 1 &&
         scalar(&fixture, "SELECT sum(sample_count) FROM test_timing_sample") == 8;
    fixture_close(&fixture);
    TEST_ASSERT_TRUE(ok, "exact even/odd medians, chronological comparisons and zero times exclude memory tools and skips");
}

static void test_timing_outliers_preserve_evidence_and_baseline(void)
{
    database_fixture_t fixture = {0};
    TEST_ASSERT_TRUE(fixture_open(&fixture, true), "prepare an isolated performance baseline");
    bool ok = true;
    for (unsigned i = 0; ok && i < 7u; ++i)
        ok = report_sample(&fixture, 10, "pass", "2024-03-01T00:00:00.000Z", false);
    ok = ok && scalar(&fixture, "SELECT count(*) FROM test_result WHERE baseline_median_ms IS NULL") == 7 &&
         report_sample(&fixture, 25, "pass", "2024-03-02T00:00:00.000Z", false) &&
         report_sample(&fixture, 30, "pass", "2024-03-03T00:00:00.000Z", false) &&
         scalar(&fixture, "SELECT normal_count = 7 AND ordinary_count = 9 AND timing_outlier_count = 2"
                          " AND mean_ms = 10 AND median_ms = 10 AND fastest_ms = 10 AND slowest_ms = 30"
                          " AND last_normal_ms = 30 AND last_normal_outlier = 1 AND took_longer = 1"
                          " FROM test_case_metrics WHERE path = 'group.original'") == 1 &&
         scalar(&fixture, "SELECT count(*) FROM test_result WHERE timing_outlier = 1"
                          " AND baseline_median_ms = 10 AND outlier_threshold_ms = 15 AND outcome = 'pass'") == 2 &&
         scalar(&fixture, "SELECT sum(sample_count) FROM test_timing_sample") == 7 &&
         report_sample(&fixture, 10, "pass", "2024-03-04T00:00:00.000Z", false) &&
         report_sample(&fixture, 1000, "pass", "2024-03-05T00:00:00.000Z", true) &&
         scalar(&fixture, "SELECT normal_count = 8 AND ordinary_count = 10 AND timing_outlier_count = 2"
                          " AND last_normal_outlier = 0 AND mean_ms = 10 AND median_ms = 10"
                          " AND last_timing_outlier_ms = 30 AND memory_test_count = 1"
                          " FROM test_case_metrics WHERE path = 'group.original'") == 1 &&
         test_database_exec(fixture.database, "UPDATE test_timing_policy SET median_multiplier = 1.2 WHERE name = 'default'") &&
         test_database_import(fixture.database, fixture.root, NULL, true) &&
         report_sample(&fixture, 14, "pass", "2024-03-06T00:00:00.000Z", false) &&
         scalar(&fixture, "SELECT timing_outlier_count = 3 AND normal_count = 8 AND mean_ms = 10"
                          " FROM test_case_metrics WHERE path = 'group.original'") == 1 &&
         scalar(&fixture, "SELECT count(*) FROM test_result WHERE timing_outlier = 1 AND outlier_threshold_ms = 12") == 1;
    fixture_close(&fixture);
    TEST_ASSERT_TRUE(ok, "slowdowns are flagged without changing outcomes, deleting evidence or poisoning the baseline");
}

static void test_timing_outliers_allow_prior_variability(void)
{
    database_fixture_t fixture = {0};
    TEST_ASSERT_TRUE(fixture_open(&fixture, true), "prepare a naturally variable performance baseline");
    const double ordinary[] = {8, 10, 12, 10, 9, 11, 10};
    bool ok = true;
    for (size_t i = 0; ok && i < sizeof ordinary / sizeof *ordinary; ++i)
        ok = report_sample(&fixture, ordinary[i], "pass", "2024-04-01T00:00:00.000Z", false);
    ok = ok && report_sample(&fixture, 15, "pass", "2024-04-02T00:00:00.000Z", false) &&
         scalar(&fixture, "SELECT timing_outlier_count = 0 AND normal_count = 8"
                          " FROM test_case_metrics WHERE path = 'group.original'") == 1 &&
         report_sample(&fixture, 18, "pass", "2024-04-03T00:00:00.000Z", false) &&
         scalar(&fixture, "SELECT normal_count = 8 AND timing_outlier_count = 1 AND median_ms = 10"
                          " AND mean_ms = 10.625 AND slowest_ms = 18"
                          " FROM test_case_metrics WHERE path = 'group.original'") == 1 &&
         scalar(&fixture, "SELECT count(*) FROM test_result WHERE timing_outlier = 1"
                          " AND baseline_median_ms = 10 AND outlier_threshold_ms = 16") == 1;
    fixture_close(&fixture);
    TEST_ASSERT_TRUE(ok, "weighted median absolute deviation distinguishes ordinary variation from a significant slowdown");
}

static bool create_legacy_reports(const string_t *path)
{
    sqlite3 *connection = NULL;
    bool ok = sqlite3_open(string_c_str(path), &connection) == SQLITE_OK;
    if (ok)
        ok = sqlite3_exec(connection,
            "PRAGMA application_id = 1297371988; PRAGMA user_version = 2;\n"
            "CREATE TABLE test_run(id INTEGER PRIMARY KEY, executable TEXT NOT NULL, profile TEXT NOT NULL,\n"
            "    started_utc TEXT NOT NULL, finished_utc TEXT, exit_code INTEGER, elapsed_ms REAL);\n"
            "CREATE TABLE test_result(run_id INTEGER, ordinal INTEGER, source TEXT, path TEXT, outcome TEXT,\n"
            "    enabled INTEGER, elapsed_ms REAL, detail TEXT, PRIMARY KEY(run_id, ordinal)) WITHOUT ROWID;\n"
            "CREATE TABLE test_catalogue(source TEXT, path TEXT, name TEXT, parent TEXT, tags TEXT, kind TEXT,\n"
            "    declaration_line INTEGER, confirmed INTEGER, PRIMARY KEY(source, path)) WITHOUT ROWID;\n"
            "CREATE TABLE test_profile(name TEXT PRIMARY KEY, default_enabled INTEGER) WITHOUT ROWID;\n"
            "CREATE TABLE test_selection(profile TEXT, source TEXT, path TEXT, enabled INTEGER,\n"
            "    PRIMARY KEY(profile, source, path)) WITHOUT ROWID;\n"
            "CREATE TABLE expression_cases(name TEXT PRIMARY KEY, operation TEXT, input TEXT, expected TEXT,\n"
            "    precision INTEGER, enabled INTEGER) WITHOUT ROWID;\n"
            "INSERT INTO test_run VALUES\n"
            "    (7, 'legacy-fixture', '', '2024-01-01T00:00:00.000Z', '2024-01-01T00:00:01.000Z', 0, 10),\n"
            "    (8, 'legacy-fixture', '', '2024-01-02T00:00:00.000Z', '2024-01-02T00:00:01.000Z', 1, 40);\n"
            "INSERT INTO test_result VALUES\n"
            "    (7, 0, 'tests/legacy.c', 'legacy_case', 'pass', 1, 10, ''),\n"
            "    (8, 0, 'tests/legacy.c', 'legacy_case', 'fail', 1, 40, 'retained failure');\n"
            "INSERT INTO test_catalogue VALUES ('tests/legacy.c', 'legacy_case', 'legacy_case', '', '', 'test', 0, 0);\n"
            "INSERT INTO test_profile VALUES ('legacy_saved', 0);\n"
            "INSERT INTO test_selection VALUES ('legacy_saved', 'tests/legacy.c', 'legacy_case', 0);\n"
            "INSERT INTO expression_cases VALUES ('legacy_custom', 'evaluate', '7+8', '15', 32, 0);\n",
            NULL, NULL, NULL) == SQLITE_OK;
    sqlite3_close(connection);
    return ok;
}

static void test_legacy_reports_merge_once_and_upgrade(void)
{
    database_fixture_t fixture = {0};
    TEST_ASSERT_TRUE(fixture_open(&fixture, false), "prepare a checkout containing an old separate report database");
    string_t *legacy_path = string_sprintf("%s/tests/test_results.db", string_c_str(fixture.root));
    string_t *copy_path = string_sprintf("%s/legacy-copy.db", string_c_str(fixture.root));
    file_t *legacy = legacy_path ? file_new(legacy_path) : NULL, *copy = copy_path ? file_new(copy_path) : NULL;
    bool ok = legacy && copy && create_legacy_reports(legacy_path) && file_copy(legacy, copy, false);
    fixture.database = ok ? test_database_open(fixture.root, NULL, true, NULL) : NULL;
    ok = ok && fixture.database && !file_exists(legacy) &&
         scalar(&fixture, "SELECT count(*) FROM test_run") == 2 &&
         scalar(&fixture, "SELECT count(*) FROM test_result") == 2 &&
         scalar(&fixture, "SELECT normal_count = 2 AND fastest_ms = 10 AND slowest_ms = 40"
                          " AND mean_ms = 25 AND median_ms = 25 AND took_longer = 1"
                          " AND last_run_utc = '2024-01-02T00:00:01.000Z' AND last_outcome = 'fail'"
                          " FROM test_case_metrics WHERE path = 'legacy_case'") == 1 &&
         scalar(&fixture, "SELECT enabled FROM test_selection WHERE profile = 'legacy_saved'") == 0 &&
         scalar(&fixture, "SELECT enabled FROM expression_cases WHERE name = 'legacy_custom'") == 0 &&
         scalar(&fixture, "SELECT confirmed FROM test_catalogue WHERE path = 'legacy_case'") == 0;
    test_database_close(fixture.database);
    fixture.database = NULL;
    ok = ok && file_copy(copy, legacy, false);
    fixture.database = ok ? test_database_open(fixture.root, NULL, true, NULL) : NULL;
    ok = ok && fixture.database && !file_exists(legacy) &&
         scalar(&fixture, "SELECT count(*) FROM test_run") == 2 &&
         scalar(&fixture, "SELECT normal_count FROM test_case_metrics WHERE path = 'legacy_case'") == 2;
    test_database_close(fixture.database);
    fixture.database = NULL;
    ok = ok && file_copy(copy, legacy, false);
    sqlite3 *broken = NULL;
    ok = ok && sqlite3_open(string_c_str(legacy_path), &broken) == SQLITE_OK &&
         sqlite3_exec(broken, "UPDATE test_result SET outcome = 'invalid' WHERE run_id = 8", NULL, NULL, NULL) == SQLITE_OK;
    sqlite3_close(broken);
    test_database_t *rejected = ok ? test_database_open(fixture.root, NULL, true, NULL) : NULL;
    ok = ok && !rejected && file_exists(legacy);
    test_database_close(rejected);
    string_t *unified = string_sprintf("%s/tests/tests.db", string_c_str(fixture.root));
    fixture.database = test_database_open(fixture.root, unified, false, NULL);
    ok = ok && fixture.database && scalar(&fixture, "SELECT count(*) FROM test_run") == 2 &&
         scalar(&fixture, "SELECT normal_count FROM test_case_metrics WHERE path = 'legacy_case'") == 2;
    test_database_close(fixture.database);
    fixture.database = NULL;
    file_t *old_single = file_new(fixture.path);
    ok = ok && old_single && file_copy(copy, old_single, false);
    fixture.database = test_database_open(fixture.root, fixture.path, true, NULL);
    ok = ok && fixture.database && scalar(&fixture, "PRAGMA user_version") == 5 &&
         scalar(&fixture, "SELECT normal_count = 2 AND median_ms = 25 FROM test_case_metrics"
                          " WHERE path = 'legacy_case'") == 1 &&
         test_database_import(fixture.database, fixture.root, NULL, true) &&
         scalar(&fixture, "SELECT normal_count FROM test_case_metrics WHERE path = 'legacy_case'") == 2;
    file_free(old_single);
    string_free(unified);
    file_free(legacy);
    file_free(copy);
    string_free(legacy_path);
    string_free(copy_path);
    fixture_close(&fixture);
    TEST_ASSERT_TRUE(ok, "legacy data merges once, failed imports retain their source, and schema upgrades replay once");
}

static void test_case_rows_are_owned_and_not_deleted(void)
{
    database_fixture_t fixture = {0};
    TEST_ASSERT_TRUE(fixture_open(&fixture, true), "create isolated storage");
    bool ok = test_database_exec(fixture.database,
        "INSERT INTO expression_cases(name, operation, input, expected) VALUES ('extra', 'evaluate', '3+4', '7');\n"
        "UPDATE expression_cases SET enabled = 0 WHERE name = 'exact_addition';") &&
        test_database_import(fixture.database, fixture.root, NULL, true);
    size_t count = 0u;
    test_database_case_t *rows = test_database_cases_load(fixture.root, fixture.path, "expression", &count, NULL);
    ok = ok && rows && count == 2u && !strcmp(string_c_str(rows[0].name), "extra") &&
         !strcmp(string_c_str(rows[0].expected), "7") &&
         scalar(&fixture, "SELECT count(*) FROM expression_cases WHERE operation = 'evaluate'") == 3;
    size_t invalid_count = 9u;
    test_database_case_t *invalid = test_database_cases_load(fixture.root, fixture.path, "expression;DROP TABLE x",
                                                           &invalid_count, NULL);
    ok = ok && !invalid && invalid_count == 0u;
    string_t *seed_path = string_sprintf("%s/tests/expression/test_expression.sql", string_c_str(fixture.root));
    file_t *seed_file = seed_path ? file_new(seed_path) : NULL;
    string_t *seed = seed_file ? file_read_all_text(seed_file) : NULL;
    ok = ok && seed && !string_append_cstr(seed,
        "\nINSERT OR IGNORE INTO expression_cases(name, operation, input, expected)\n"
        "VALUES ('new_seed', 'evaluate', '4+5', '9');\n") && file_write_all_text(seed_file, seed);
    size_t unchanged_count = 0u;
    test_database_case_t *unchanged = test_database_cases_load(fixture.root, fixture.path, "expression",
                                                             &unchanged_count, NULL);
    ok = ok && unchanged && unchanged_count == 2u &&
         test_database_import(fixture.database, fixture.root, NULL, true) &&
         scalar(&fixture, "SELECT count(*) FROM expression_cases WHERE name = 'new_seed' AND expected = '9'") == 1 &&
         scalar(&fixture, "SELECT enabled FROM expression_cases WHERE name = 'exact_addition'") == 0;
    test_database_cases_free(unchanged, unchanged_count);
    string_free(seed);
    file_free(seed_file);
    string_free(seed_path);
    fixture_close(&fixture);
    ok = ok && rows && !strcmp(string_c_str(rows[0].input), "3+4");
    test_database_cases_free(rows, count);
    test_database_cases_free(invalid, invalid_count);
    TEST_ASSERT_TRUE(ok, "rows and edits survive; seed changes require explicit sync; snapshots own their data");
}

static void test_help_creates_no_database(void)
{
    database_fixture_t fixture = {0};
    TEST_ASSERT_TRUE(fixture_open(&fixture, false), "prepare isolated repository without storage");
    char *arguments[] = {"--database", (char *)string_c_str(fixture.path), "help"};
    int status = test_database_command(fixture.root, 3, arguments);
    file_t *file = file_new(fixture.path);
    bool ok = status == 0 && file && !file_exists(file);
    file_free(file);
    fixture_close(&fixture);
    TEST_ASSERT_TRUE(ok, "administration help never creates test storage");
}

static int conditional_sync(database_fixture_t *fixture)
{
    char *arguments[] = {"--database", (char *)string_c_str(fixture->path), "sync-if-needed"};
    return test_database_command(fixture->root, 3, arguments);
}

static bool append_sql(const string_t *path, const char *sql)
{
    file_t *file = file_new(path);
    string_t *content = file ? file_read_all_text(file) : NULL;
    bool ok = content && !string_append_cstr(content, sql) && file_write_all_text(file, content);
    string_free(content);
    file_free(file);
    return ok;
}

static void test_conditional_sync_never_creates_storage(void)
{
    database_fixture_t fixture = {0};
    TEST_ASSERT_TRUE(fixture_open(&fixture, false), "prepare sources without a database");
    char *arguments[] = {"sync-if-needed"};
    string_t *default_path = string_sprintf("%s/tests/tests.db", string_c_str(fixture.root));
    file_t *default_file = default_path ? file_new(default_path) : NULL;
    file_t *override_file = file_new(fixture.path);
    bool ok = default_file && override_file && test_database_command(fixture.root, 1, arguments) == 0 &&
              !file_exists(default_file) && conditional_sync(&fixture) == 0 && !file_exists(override_file);
    ok = ok && write_text(fixture.path, "not a MARS test database") && conditional_sync(&fixture) == 1;
    string_t *unchanged = file_read_all_text(override_file);
    ok = ok && unchanged && !strcmp(string_c_str(unchanged), "not a MARS test database");
    string_free(unchanged);
    file_free(override_file);
    file_free(default_file);
    string_free(default_path);
    fixture_close(&fixture);
    TEST_ASSERT_TRUE(ok, "release checks never create missing storage or replace unrelated files");
}

static void test_conditional_sync_tracks_contents(void)
{
    database_fixture_t fixture = {0};
    TEST_ASSERT_TRUE(fixture_open(&fixture, true), "prepare an existing database");
    test_database_result_t record = result_record();
    bool ok = test_database_import(fixture.database, fixture.root, "saved", true) &&
              test_database_run_begin(fixture.database, "preserved-report", "saved", false) &&
              test_database_run_record(fixture.database, &record) &&
              test_database_run_finish(fixture.database, 0, 2.5) &&
              test_database_exec(fixture.database,
                  "UPDATE test_source_sync SET synced_utc = '2000-01-01T00:00:00Z' WHERE id = 1;") &&
              conditional_sync(&fixture) == 0 &&
              scalar(&fixture, "SELECT synced_utc = '2000-01-01T00:00:00Z' FROM test_source_sync") == 1;
    ok = ok && configure(&fixture, "{\"tests/example.c\":{\"new_from_pull\":true}}") &&
         conditional_sync(&fixture) == 0 &&
         scalar(&fixture, "SELECT count(*) FROM test_catalogue WHERE path = 'new_from_pull' AND confirmed = 1") == 1;
    string_t *source = string_sprintf("%s/tests/expression/test_expression.sql", string_c_str(fixture.root));
    file_t *source_file = source ? file_new(source) : NULL;
    ok = ok && source_file && append_sql(source,
        "\nINSERT OR IGNORE INTO expression_cases(name, operation, input, expected)\n"
        "VALUES ('conditional_added_case', 'evaluate', '7+9', '16');\n") &&
        file_set_times(source_file, 946684800, 0, 946684800, 0) && conditional_sync(&fixture) == 0 &&
        scalar(&fixture, "SELECT count(*) FROM expression_cases WHERE name = 'conditional_added_case'") == 1 &&
        test_database_exec(fixture.database,
            "UPDATE test_source_sync SET synced_utc = '2000-01-01T00:00:00Z' WHERE id = 1;") &&
        conditional_sync(&fixture) == 0 &&
        scalar(&fixture, "SELECT synced_utc = '2000-01-01T00:00:00Z' FROM test_source_sync") == 1 &&
        scalar(&fixture, "SELECT count(*) FROM test_run") == 1 &&
        scalar(&fixture, "SELECT run_count FROM test_case_statistics WHERE source = 'tests/example.c'") == 1 &&
        scalar(&fixture, "SELECT enabled FROM test_selection WHERE profile = 'saved' AND path = 'group.original'") == 0;
    file_free(source_file);
    string_free(source);
    fixture_close(&fixture);
    TEST_ASSERT_TRUE(ok, "content changes import once while unchanged sources, selections and history remain intact");
}

static void test_conditional_sync_retries_failed_import(void)
{
    database_fixture_t fixture = {0};
    TEST_ASSERT_TRUE(fixture_open(&fixture, true), "prepare a complete synchronisation");
    bool ok = test_database_import(fixture.database, fixture.root, NULL, true) &&
              test_database_exec(fixture.database,
                  "UPDATE test_source_sync SET synced_utc = '2000-01-01T00:00:00Z' WHERE id = 1;");
    string_t *source = string_sprintf("%s/tests/expression/test_expression.sql", string_c_str(fixture.root));
    file_t *file = source ? file_new(source) : NULL;
    string_t *original = file ? file_read_all_text(file) : NULL;
    const char *addition =
        "\nINSERT OR IGNORE INTO expression_cases(name, operation, input, expected)\n"
        "VALUES ('conditional_retry_case', 'evaluate', '8+9', '17');\n";
    ok = ok && original && append_sql(source, addition) && append_sql(source, "INVALID SQL;\n") &&
         conditional_sync(&fixture) == 1 &&
         scalar(&fixture, "SELECT count(*) FROM expression_cases WHERE name = 'conditional_retry_case'") == 0 &&
         scalar(&fixture, "SELECT synced_utc = '2000-01-01T00:00:00Z' FROM test_source_sync") == 1 &&
         file_write_all_text(file, original) && append_sql(source, addition) && conditional_sync(&fixture) == 0 &&
         scalar(&fixture, "SELECT count(*) FROM expression_cases WHERE name = 'conditional_retry_case'") == 1;
    ok = ok && test_database_exec(fixture.database, "DROP TABLE test_source_sync; PRAGMA user_version = 4;") &&
         conditional_sync(&fixture) == 0 && scalar(&fixture, "PRAGMA user_version") == 5 &&
         scalar(&fixture, "SELECT count(*) FROM test_source_sync") == 1;
    string_free(original);
    file_free(file);
    string_free(source);
    fixture_close(&fixture);
    TEST_ASSERT_TRUE(ok, "failed imports stay pending, retry without partial rows and upgrade older schema safely");
}

static void test_module_sources_are_discovered_atomically(void)
{
    database_fixture_t fixture = {0};
    TEST_ASSERT_TRUE(fixture_open(&fixture, true), "prepare existing storage before a new module is authored");
    string_t *directory_path = string_sprintf("%s/tests/probe", string_c_str(fixture.root));
    string_t *source_path = string_sprintf("%s/tests/probe/test_probe.sql", string_c_str(fixture.root));
    file_t *directory = directory_path ? file_new(directory_path) : NULL;
    const char *sql =
        "CREATE TABLE probe_cases(name TEXT PRIMARY KEY, operation TEXT, input TEXT, expected TEXT,\n"
        "    precision INTEGER DEFAULT 32, enabled INTEGER DEFAULT 1);\n"
        "INSERT INTO probe_cases(name, operation, input, expected) VALUES ('exact_bytes', 'bytes', 'e\xcc\x81', 'e\xcc\x81');\n"
        "INSERT INTO test_case_runner(module, source, parent, table_name, operation)\n"
        "VALUES ('probe', 'tests/probe/test_probe.c', 'original_group', 'probe_cases', 'bytes');\n";
    bool ok = directory && source_path && file_create_directory(directory, 0700u, true) &&
              write_text(source_path, sql) && test_database_import(fixture.database, fixture.root, NULL, true);
    size_t count = 0u;
    test_database_case_t *rows = test_database_cases_load(fixture.root, fixture.path, "probe", &count, NULL);
    ok = ok && rows && count == 1u && string_byte_length(rows[0].input) == 3u &&
         !memcmp(string_c_str(rows[0].input), "e\xcc\x81", 3u) &&
         scalar(&fixture, "SELECT count(*) FROM test_catalogue WHERE source = 'tests/probe/test_probe.c'"
                          " AND path = 'original_group.exact_bytes' AND confirmed = 1") == 1;
    sqlite3_int64 before = scalar(&fixture, "SELECT count(*) FROM expression_cases");
    ok = ok && write_text(source_path,
        "INSERT INTO expression_cases(name, operation, input, expected) VALUES ('rolled_back', 'evaluate', '2+2', '4');\n"
        "THIS IS DELIBERATELY INVALID SQL;\n") &&
         !test_database_import(fixture.database, fixture.root, NULL, true) &&
         scalar(&fixture, "SELECT count(*) FROM expression_cases") == before &&
         scalar(&fixture, "SELECT count(*) FROM probe_cases") == 1 &&
         strstr(test_database_error(fixture.database), "test_probe.sql");
    test_database_cases_free(rows, count);
    file_free(directory);
    string_free(source_path);
    string_free(directory_path);
    fixture_close(&fixture);
    TEST_ASSERT_TRUE(ok, "new module sources need no compiled registry; exact bytes and transactional rollback survive");
}

static void test_default_storage_survives_build_cleanup(void)
{
    database_fixture_t fixture = {0};
    TEST_ASSERT_TRUE(fixture_open(&fixture, false), "prepare an isolated checkout without test storage");
    string_t *case_path = string_sprintf("%s/tests/tests.db", string_c_str(fixture.root));
    string_t *report_path = string_sprintf("%s/tests/test_results.db", string_c_str(fixture.root));
    string_t *build_path = string_sprintf("%s/build", string_c_str(fixture.root));
    string_t *object_path = string_sprintf("%s/build/compiler-output", string_c_str(fixture.root));
    file_t *cases_file = case_path ? file_new(case_path) : NULL;
    file_t *report_file = report_path ? file_new(report_path) : NULL;
    file_t *build_directory = build_path ? file_new(build_path) : NULL;
    file_t *object_file = object_path ? file_new(object_path) : NULL;
    bool ok = cases_file && report_file && build_directory && object_file &&
              !file_exists(cases_file) && !file_exists(report_file);
    fixture.database = test_database_open(fixture.root, NULL, true, NULL);
    ok = ok && fixture.database && test_database_exec(fixture.database,
        "INSERT INTO expression_cases(name, operation, input, expected)\n"
        "VALUES ('portable_case', 'evaluate', '3+4', '7');");
    test_database_result_t record = result_record();
    test_database_t *reports = test_database_open(fixture.root, NULL, true, NULL);
    ok = ok && reports && test_database_run_begin(reports, "fixture", NULL, false) &&
         test_database_run_record(reports, &record) && test_database_run_finish(reports, 0, 1.0);
    test_database_close(reports);
    size_t count = 0u;
    test_database_case_t *rows = test_database_cases_load(fixture.root, NULL, "expression", &count, NULL);
    ok = ok && rows && count == 3u &&
         scalar(&fixture, "SELECT count(*) FROM test_run") == 1 &&
         scalar(&fixture, "SELECT normal_count FROM test_case_metrics WHERE path = 'group.original'") == 1 &&
         scalar(&fixture, "SELECT count(*) FROM expression_cases WHERE name = 'exact_addition' AND expected = '1/2'") == 1 &&
         file_create_directory(build_directory, 0700u, true) && write_text(object_path, "temporary object") &&
         file_delete(object_file) && file_remove_directory(build_directory) &&
         file_exists(cases_file) && !file_exists(report_file);
    test_database_close(fixture.database);
    fixture.database = test_database_open(fixture.root, NULL, false, NULL);
    ok = ok && fixture.database && scalar(&fixture,
        "SELECT count(*) FROM expression_cases WHERE name = 'portable_case' AND expected = '7'") == 1;
    test_database_cases_free(rows, count);
    file_free(cases_file);
    file_free(report_file);
    file_free(build_directory);
    file_free(object_file);
    string_free(case_path);
    string_free(report_path);
    string_free(build_path);
    string_free(object_path);
    fixture_close(&fixture);
    TEST_ASSERT_TRUE(ok, "one database retains cases, statistics and reports through build cleanup; definitions stay intact");
}

static string_t *capture_stats(database_fixture_t *fixture, const char *leaf, const char *format,
                                const char *extra, int *status)
{
    const char *path = NULL;
    int saved = test_case_begin_stdout_capture(leaf, &path);
    if (saved < 0) return NULL;
    char *arguments[] = {"--database", (char *)string_c_str(fixture->path), "stats", (char *)format, (char *)extra};
    *status = test_database_command(fixture->root, extra ? 5 : format ? 4 : 3, arguments);
    bool restored = test_case_end_stdout_capture(saved);
    file_t *file = restored && path ? file_new_cstr(path) : NULL;
    string_t *output = file ? file_read_all_text(file) : NULL;
    file_free(file);
    return output;
}

/* Count visible columns independently, including Unicode text and excluding terminal colour/weight sequences. */
static bool grid_aligned(const string_t *output, size_t limit)
{
    locale_t utf8 = newlocale(LC_CTYPE_MASK, "C.UTF-8", (locale_t)0);
    locale_t previous = utf8 ? uselocale(utf8) : (locale_t)0;
    if (!previous) { if (utf8) freelocale(utf8); return false; }
    const char *p = string_c_str(output);
    size_t columns = 0, expected = 0, lines = 0;
    bool table = false, ok = true;
    mbstate_t state = {0};
    while (ok && *p) {
        if (*p == '\033' && p[1] == '[') {
            const char *end = p + 2 + strspn(p + 2, "0123456789;");
            if (*end != 'm') { ok = false; break; }
            p = end + 1;
            continue;
        }
        wchar_t rune = 0;
        size_t bytes = mbrtowc(&rune, p, MB_CUR_MAX, &state);
        if (!bytes || bytes == (size_t)-1 || bytes == (size_t)-2) { ok = false; break; }
        if (!columns) table = rune == L'╔' || rune == L'╠' || rune == L'╟' || rune == L'╚' || rune == L'║';
        if (rune == L'\n') {
            if (table) {
                if (!expected) expected = columns;
                ok = columns == expected && columns <= limit;
                ++lines;
            }
            columns = 0;
        } else {
            int width = wcwidth(rune);
            if (width < 0) ok = false;
            else columns += (size_t)width;
        }
        p += bytes;
    }
    uselocale(previous); freelocale(utf8);
    return ok && lines >= 5;
}

static void test_statistics_formats_and_reader_detection(void)
{
    database_fixture_t fixture = {0};
    TEST_ASSERT_TRUE(fixture_open(&fixture, true), "prepare isolated report storage");
    test_database_result_t record = result_record();
    record.path = "group.escaping_|_漢字_e\xcc\x81_with_a_very_long_name_that_wraps_without_losing_its_end\t\n\033[31m";
    bool ok = test_database_run_begin(fixture.database, "fixture", NULL, false) &&
              test_database_run_record(fixture.database, &record) &&
              test_database_run_finish(fixture.database, 0, 2.5) &&
              test_database_run_begin(fixture.database, "memory fixture", NULL, true) &&
              test_database_run_record(fixture.database, &record) &&
              test_database_run_finish(fixture.database, 0, 2.5);
    char *saved_path = getenv("PATH") ? strdup(getenv("PATH")) : NULL;
    char *saved_columns = getenv("COLUMNS") ? strdup(getenv("COLUMNS")) : NULL;
    char *saved_disabled = getenv("NO_COLOR") ? strdup(getenv("NO_COLOR")) : NULL;
    char *saved_term = getenv("TERM") ? strdup(getenv("TERM")) : NULL;
    char *saved_force = getenv("CLICOLOR_FORCE") ? strdup(getenv("CLICOLOR_FORCE")) : NULL;
    ok = ok && !setenv("PATH", string_c_str(fixture.root), 1) && !setenv("COLUMNS", "200", 1) &&
         !setenv("NO_COLOR", "1", 1);
    int status = -1;
    string_t *grid = ok ? capture_stats(&fixture, "stats-grid.txt", NULL, NULL, &status) : NULL;
    ok = ok && status == 0 && grid && grid_aligned(grid, 200) && string_find(grid, "╤") >= 0 &&
         string_find(grid, "╪") >= 0 && string_find(grid, "╧") >= 0 &&
         string_find(grid, "Change %") >= 0 && string_find(grid, "Samples") >= 0 &&
         string_find(grid, "end") >= 0 && string_find(grid, "漢字") >= 0 && string_find(grid, "\t") < 0 &&
         string_find(grid, "\\x09\\x0a\\x1b[31m") >= 0;
    string_free(grid);
    ok = ok && !setenv("COLUMNS", "80", 1);
    string_t *details = ok ? capture_stats(&fixture, "stats-narrow.txt", "--format=grid", NULL, &status) : NULL;
    ok = ok && status == 0 && details && grid_aligned(details, 80) &&
         string_find(details, "Last memory test (UTC)") >= 0 && string_find(details, "Timing outliers") >= 0 &&
         string_find(details, "┼") >= 0;
    string_free(details);
    static const char *const readers[] = {"glow", "mdcat", "mdless"};
    for (size_t i = 0; ok && i < sizeof readers / sizeof *readers; ++i) {
        string_t *path = string_sprintf("%s/%s", string_c_str(fixture.root), readers[i]);
        file_t *file = path ? file_new(path) : NULL;
        ok = file && write_text(path, "Reader detection must never execute this file.\n") &&
             !chmod(string_c_str(path), 0700);
        grid = ok ? capture_stats(&fixture, "stats-reader.txt", NULL, NULL, &status) : NULL;
        ok = ok && status == 0 && grid && grid_aligned(grid, 80) && string_find(grid, "╔") >= 0;
        string_free(grid);
        string_t *markdown = ok ? capture_stats(&fixture, "stats-override.txt", "--format=markdown", NULL, &status) : NULL;
        ok = ok && status == 0 && markdown && string_find(markdown, "| **Test** | **Result** |") >= 0 &&
             string_find(markdown, "escaping\\_\\|\\_漢字") >= 0 && string_find(markdown, "╔") < 0;
        string_free(markdown);
        ok = ok && !unsetenv("NO_COLOR") && !unsetenv("CLICOLOR_FORCE") && !setenv("TERM", "xterm", 1);
        grid = ok ? capture_stats(&fixture, "stats-reader-colour.txt", NULL, NULL, &status) : NULL;
        ok = ok && status == 0 && grid && string_find(grid, "\033[1;92mPASS\033[0m") >= 0 &&
             string_find(grid, "**PASS**") < 0;
        string_free(grid);
        ok = ok && !setenv("CLICOLOR_FORCE", "1", 1);
        markdown = ok ? capture_stats(&fixture, "stats-plain-export.txt", "--format=markdown", NULL, &status) : NULL;
        ok = ok && status == 0 && markdown && string_find(markdown, "**PASS**") >= 0 &&
             string_find(markdown, "\033") < 0;
        string_free(markdown);
        ok = ok && !setenv("NO_COLOR", "1", 1);
        ok = file && file_delete(file) && ok;
        file_free(file); string_free(path);
    }
    string_t *markdown = ok ? capture_stats(&fixture, "stats-details.txt", "--format=markdown", "--details", &status) : NULL;
    ok = ok && status == 0 && markdown && string_find(markdown, "| **Metric** | **Value** |") >= 0 &&
         string_find(markdown, "Last memory test (UTC)") >= 0 && string_find(markdown, "Last run (UTC)") >= 0;
    string_free(markdown);
    string_t *tsv = ok ? capture_stats(&fixture, "stats-tsv.txt", "--format=tsv", NULL, &status) : NULL;
    ok = ok && status == 0 && tsv && string_find(tsv, "source\tpath\tlast_run_utc\tlast_outcome") == 0 &&
         string_find(tsv, "\tlast_memory_test_utc\n") >= 0 && string_find(tsv, "Test statistics") < 0;
    string_free(tsv);
    string_t *empty = ok ? capture_stats(&fixture, "stats-empty.txt", "--format=markdown", "not-a-source", &status) : NULL;
    ok = ok && status == 0 && empty && string_find(empty, "No recorded tests match") >= 0 &&
         scalar(&fixture, "SELECT count(*) FROM test_run") == 2 &&
         scalar(&fixture, "SELECT count(*) FROM test_result") == 2 &&
         scalar(&fixture, "SELECT normal_count FROM test_case_metrics WHERE run_count > 0") == 1;
    string_free(empty);
    if (saved_path) { ok = !setenv("PATH", saved_path, 1) && ok; free(saved_path); }
    else ok = !unsetenv("PATH") && ok;
    if (saved_columns) { ok = !setenv("COLUMNS", saved_columns, 1) && ok; free(saved_columns); }
    else ok = !unsetenv("COLUMNS") && ok;
    if (saved_disabled) { ok = !setenv("NO_COLOR", saved_disabled, 1) && ok; free(saved_disabled); }
    else ok = !unsetenv("NO_COLOR") && ok;
    if (saved_term) { ok = !setenv("TERM", saved_term, 1) && ok; free(saved_term); }
    else ok = !unsetenv("TERM") && ok;
    if (saved_force) { ok = !setenv("CLICOLOR_FORCE", saved_force, 1) && ok; free(saved_force); }
    else ok = !unsetenv("CLICOLOR_FORCE") && ok;
    fixture_close(&fixture);
    TEST_ASSERT_TRUE(ok, "installed readers retain the default grid; Markdown exports stay plain even with forced colour");
}

static void test_statistics_invalid_format_creates_no_database(void)
{
    database_fixture_t fixture = {0};
    TEST_ASSERT_TRUE(fixture_open(&fixture, false), "prepare a checkout without test storage");
    int status = -1;
    string_t *output = capture_stats(&fixture, "stats-invalid.txt", "--format=html", NULL, &status);
    file_t *file = file_new(fixture.path);
    bool ok = output && status == 2 && file && !file_exists(file);
    string_free(output); file_free(file); fixture_close(&fixture);
    TEST_ASSERT_TRUE(ok, "invalid format options are rejected before any test database is created");
}

static void test_statistics_bold_headers_and_uppercase_outcomes(void)
{
    database_fixture_t fixture = {0};
    TEST_ASSERT_TRUE(fixture_open(&fixture, true), "prepare isolated styled statistics");
    static const struct { const char *path, *outcome; double previous_ms, elapsed_ms; } records[] = {
        {"group.passing",     "pass", 3.125,  2.5},
        {"group.failing",     "fail", 2,      2.5},
        {"group.skipped",     "pass", 2.5,    2.5},
        {"group.lower_limit", "pass", 10,     9.5},
        {"group.upper_limit", "pass", 10,    10.5},
        {"group.unknown",     "pass", 0,      2.5},
    };
    bool ok = true;
    for (unsigned run = 0; ok && run < 2; ++run) {
        ok = test_database_run_begin(fixture.database, "fixture", NULL, false);
        double total_ms = 0;
        for (size_t i = 0; ok && i < sizeof records / sizeof *records; ++i) {
            test_database_result_t record = result_record();
            record.path = records[i].path;
            record.outcome = records[i].outcome;
            record.elapsed_ms = run ? records[i].elapsed_ms : records[i].previous_ms;
            total_ms += record.elapsed_ms;
            ok = test_database_run_record(fixture.database, &record);
        }
        ok = ok && test_database_run_finish(fixture.database, 1, total_ms);
    }
    /* Skips do not accumulate timings; seed that supported display label in this presentation-only fixture. */
    ok = ok && test_database_exec(fixture.database,
        "UPDATE test_case_statistics SET last_outcome = 'skip' WHERE path = 'group.skipped';");
    char *saved_force = getenv("CLICOLOR_FORCE") ? strdup(getenv("CLICOLOR_FORCE")) : NULL;
    char *saved_disabled = getenv("NO_COLOR") ? strdup(getenv("NO_COLOR")) : NULL;
    char *saved_term = getenv("TERM") ? strdup(getenv("TERM")) : NULL;
    ok = ok && !unsetenv("CLICOLOR_FORCE") && !unsetenv("NO_COLOR") && !setenv("TERM", "xterm", 1);
    int status = -1;
    string_t *grid = ok ? capture_stats(&fixture, "stats-bold.txt", "--format=grid", NULL, &status) : NULL;
    ok = ok && status == 0 && grid && string_find(grid, "\033[1mTest\033[0m") >= 0 &&
         string_find(grid, "\033[1mResult\033[0m") >= 0 && string_find(grid, "\033[1;92mPASS\033[0m") >= 0 &&
         string_find(grid, "\033[1;91mFAIL\033[0m") >= 0 && string_find(grid, "\033[1;93mSKIPPED\033[0m") >= 0 &&
         string_find(grid, "\033[1;93mSource: tests/example.c\033[0m") >= 0 &&
         string_find(grid, "\033[96mgroup.passing\033[0m") >= 0 &&
         string_find(grid, "\033[1;96m2.8125\033[0m") >= 0 &&
         string_find(grid, "\033[1;93m2.8125\033[0m") >= 0 &&
         string_find(grid, "\033[1;92m-20 ▲\033[0m") >= 0 && string_find(grid, "\033[1;91m25 ▼\033[0m") >= 0 &&
         string_find(grid, "\033[1;38;2;255;140;0m0 ▬\033[0m") >= 0 &&
         string_find(grid, "\033[1;38;2;255;140;0m-5 ▬\033[0m") >= 0 &&
         string_find(grid, "\033[1;38;2;255;140;0m5 ▬\033[0m") >= 0 &&
         string_find(grid, "\033[1;38;2;255;140;0m-\033[0m") < 0 &&
         grid_aligned(grid, 200);
    if (!ok) fprintf(stderr, "Styled statistics status %d:\n%s\n", status, grid ? string_c_str(grid) : "no output");
    string_free(grid);
    grid = ok ? capture_stats(&fixture, "stats-bold-details.txt", "--format=grid", "--details", &status) : NULL;
    ok = ok && status == 0 && grid && string_find(grid, "\033[1mMetric\033[0m") >= 0 &&
         string_find(grid, "\033[1;93mtests/example.c\033[0m") >= 0 &&
         string_find(grid, "\033[96mgroup.passing\033[0m") >= 0 &&
         string_find(grid, "\033[1;96m2.8125\033[0m") >= 0 &&
         string_find(grid, "\033[1;93m2.8125\033[0m") >= 0 &&
         string_find(grid, "\033[1;92m-20 ▲\033[0m") >= 0 && string_find(grid, "\033[1;91m25 ▼\033[0m") >= 0 &&
         string_find(grid, "\033[1;38;2;255;140;0m-5 ▬\033[0m") >= 0 &&
         string_find(grid, "\033[1;38;2;255;140;0m5 ▬\033[0m") >= 0 &&
         grid_aligned(grid, 200);
    string_free(grid);
    ok = ok && !setenv("CLICOLOR_FORCE", "1", 1);
    string_t *markdown = ok ? capture_stats(&fixture, "stats-bold-md.txt", "--format=markdown", NULL, &status) : NULL;
    ok = ok && status == 0 && markdown && string_find(markdown, "| **Test** | **Result** |") >= 0 &&
         string_find(markdown, "**PASS**") >= 0 && string_find(markdown, "**FAIL**") >= 0 &&
         string_find(markdown, "**SKIPPED**") >= 0 && string_find(markdown, "\033") < 0 &&
         string_find(markdown, "▲") < 0 && string_find(markdown, "▼") < 0 && string_find(markdown, "▬") < 0 &&
         string_find(markdown, "| -20 |") >= 0 && string_find(markdown, "| 25 |") >= 0;
    string_free(markdown);
    string_t *tsv = ok ? capture_stats(&fixture, "stats-raw-tsv.txt", "--format=tsv", NULL, &status) : NULL;
    ok = ok && status == 0 && tsv && string_find(tsv, "\tpass\t") >= 0 && string_find(tsv, "\tfail\t") >= 0 &&
         string_find(tsv, "\tskip\t") >= 0 && string_find(tsv, "\033") < 0 &&
         string_find(tsv, "▲") < 0 && string_find(tsv, "▼") < 0 && string_find(tsv, "▬") < 0;
    string_free(tsv);
    ok = ok && !setenv("NO_COLOR", "1", 1);
    grid = ok ? capture_stats(&fixture, "stats-no-colour.txt", "--format=grid", NULL, &status) : NULL;
    ok = ok && status == 0 && grid && string_find(grid, "\033") < 0 && grid_aligned(grid, 200) &&
         string_find(grid, "-20 ▲") >= 0 && string_find(grid, "25 ▼") >= 0 && string_find(grid, "0 ▬") >= 0;
    string_free(grid);
    ok = ok && !unsetenv("NO_COLOR") && !setenv("TERM", "dumb", 1);
    grid = ok ? capture_stats(&fixture, "stats-dumb.txt", "--format=grid", NULL, &status) : NULL;
    ok = ok && status == 0 && grid && string_find(grid, "\033") < 0 &&
         scalar(&fixture, "SELECT count(*) FROM test_result WHERE outcome IN ('pass', 'fail', 'skip')") == 12;
    string_free(grid);
    if (saved_force) { ok = !setenv("CLICOLOR_FORCE", saved_force, 1) && ok; free(saved_force); }
    else ok = !unsetenv("CLICOLOR_FORCE") && ok;
    if (saved_disabled) { ok = !setenv("NO_COLOR", saved_disabled, 1) && ok; free(saved_disabled); }
    else ok = !unsetenv("NO_COLOR") && ok;
    if (saved_term) { ok = !setenv("TERM", saved_term, 1) && ok; free(saved_term); }
    else ok = !unsetenv("TERM") && ok;
    fixture_close(&fixture);
    TEST_ASSERT_TRUE(ok, "bold respects terminal settings; displayed outcomes are uppercase and stored/TSV values unchanged");
}

/* README example from docs/testing.md: Markdown statistics retain aligned, scriptable column definitions. */
static void test_readme_statistics_formats(void)
{
    database_fixture_t fixture = {0};
    TEST_ASSERT_TRUE(fixture_open(&fixture, true), "create isolated README statistics");
    test_database_result_t record = result_record();
    bool ok = test_database_run_begin(fixture.database, "fixture", NULL, false) &&
              test_database_run_record(fixture.database, &record) &&
              test_database_run_finish(fixture.database, 0, 2.5);
    int status = -1;
    string_t *output = ok ? capture_stats(&fixture, "readme-stats.txt", "--format=markdown", NULL, &status) : NULL;
    const char *example =
        "| **Test** | **Result** | **Last ms** | **Change %** | **Fastest ms** | **Slowest ms** | "
        "**Mean ms** | **Median ms** | **Samples** |\n"
        "| :--- | :--- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |\n"
        "| group.original | **PASS** | 2.5 | - | 2.5 | 2.5 | 2.5 | 2.5 | 1 |\n";
    const char *root = getenv("MARS_ROOT");
    string_t *path = string_sprintf("%s/docs/testing.md", root && *root ? root : ".");
    file_t *file = path ? file_new(path) : NULL;
    string_t *guide = file ? file_read_all_text(file) : NULL;
    ok = ok && output && status == 0 && string_find(output, example) >= 0 && guide && string_find(guide, example) >= 0;
    if (ok) fputs(example, stdout);
    string_free(guide); file_free(file); string_free(path);
    string_free(output); fixture_close(&fixture);
    TEST_ASSERT_TRUE(ok, "README Markdown table is emitted by the real statistics command");
}

/* README example from docs/testing.md: a new row reuses an existing native checking operation. */
static void test_readme_case_table(void)
{
    database_fixture_t fixture = {0};
    TEST_ASSERT_TRUE(fixture_open(&fixture, true), "create isolated README fixture");
    const char *root = getenv("MARS_ROOT");
    string_t *document_path = string_sprintf("%s/docs/testing.md", root && *root ? root : ".");
    string_t *programme_path = string_sprintf("%s/tests/test_database/readme_case_table.c", root && *root ? root : ".");
    file_t *document_file = document_path ? file_new(document_path) : NULL;
    file_t *programme_file = programme_path ? file_new(programme_path) : NULL;
    string_t *document = document_file ? file_read_all_text(document_file) : NULL;
    string_t *programme = programme_file ? file_read_all_text(programme_file) : NULL;
    bool ok = document && programme && string_find(document, string_c_str(programme)) >= 0 &&
              string_find(document, "```text\nanother_sum|5/6\n```") >= 0;
    string_free(document);
    string_free(programme);
    file_free(document_file);
    file_free(programme_file);
    string_free(document_path);
    string_free(programme_path);
    ok = ok && test_database_exec(fixture.database,
        "INSERT INTO expression_cases(name, operation, input, expected)\n"
        "VALUES ('another_sum', 'evaluate', '1/2+1/3', '5/6');");
    sqlite3_stmt *statement = test_database_prepare(fixture.database,
        "SELECT name, expected FROM expression_cases WHERE name = 'another_sum'");
    ok = ok && statement && sqlite3_step(statement) == SQLITE_ROW &&
         !strcmp((const char *)sqlite3_column_text(statement, 0), "another_sum") &&
         !strcmp((const char *)sqlite3_column_text(statement, 1), "5/6");
    if (ok)
        puts("another_sum|5/6");
    sqlite3_finalize(statement);
    fixture_close(&fixture);
    TEST_ASSERT_TRUE(ok, "README case-table output agrees with the testing guide");
}

int tests_main(void)
{
    TEST_SECTION("Test database safety");
    TEST_RUN_CASE(test_catalogue_sync_preserves_every_identity, "database,sync,preservation");
    TEST_RUN_CASE(test_catalogue_failure_rolls_back, "database,sync,rollback");
    TEST_RUN_CASE(test_profile_selection_snapshot, "database,selection");
    TEST_RUN_CASE(test_run_report_transactions_and_limits, "database,reports,limits");
    TEST_RUN_CASE(test_lifetime_statistics_exclude_memory_and_skips, "database,statistics,median,memory-exclusion");
    TEST_RUN_CASE(test_timing_outliers_preserve_evidence_and_baseline, "database,statistics,outliers,preservation");
    TEST_RUN_CASE(test_timing_outliers_allow_prior_variability, "database,statistics,outliers,variation");
    TEST_RUN_CASE(test_legacy_reports_merge_once_and_upgrade, "database,migration,statistics,preservation");
    TEST_RUN_CASE(test_fixture_and_unrelated_database_protection, "database,fixtures");
    TEST_RUN_CASE(test_case_rows_are_owned_and_not_deleted, "database,cases");
    TEST_RUN_CASE(test_help_creates_no_database, "database,lazy");
    TEST_RUN_CASE(test_conditional_sync_never_creates_storage, "database,sync,lazy");
    TEST_RUN_CASE(test_conditional_sync_tracks_contents, "database,sync,history");
    TEST_RUN_CASE(test_conditional_sync_retries_failed_import, "database,sync,rollback");
    TEST_RUN_CASE(test_module_sources_are_discovered_atomically, "database,modules,discovery,rollback");
    TEST_RUN_CASE(test_default_storage_survives_build_cleanup, "database,paths,persistence");
    TEST_RUN_CASE(test_statistics_formats_and_reader_detection, "database,statistics,formatting,unicode");
    TEST_RUN_CASE(test_statistics_invalid_format_creates_no_database, "database,statistics,lazy");
    TEST_RUN_CASE(test_statistics_bold_headers_and_uppercase_outcomes, "database,statistics,formatting,bold");
    TEST_RUN_OUTPUT_TAGS(test_readme_statistics_formats, "README,database,statistics");
    TEST_RUN_OUTPUT_TAGS(test_readme_case_table, "README,database");
    return TEST_EXIT_CODE();
}
