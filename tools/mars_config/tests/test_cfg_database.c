/**
 * @file test_cfg_database.c
 * @brief Isolated native database installation, streaming and failure regressions.
 *
 * Synthetic SQL fixtures exercise SQLCipher imports, recursive source reads,
 * trigger completeness, private publication and literal credential precedence.
 * Packaged almanac coverage checks the complete coefficient imports and encrypted
 * reopening. The jurisdiction fixture checks saved choices and failed replacements.
 * Every fixture runs beneath the
 * disposable test_cfg_isolated home; no user's configuration is accessed.
 */
#include <stdlib.h>
#include <string.h>

#include "../src/database/cfg_database_internal.h"
#include "cfg_database.h"
#include "cfg_storage.h"
#include "file.h"
#include "sqlite.h"
#include "test_cfg_support.h"
#include "test_harness.h"

static file_t *test_cfg_database_file(const char *directory, const char *name)
{
    string_t *path = string_sprintf("%s/%s", directory, name);
    file_t *file = path ? file_new(path) : NULL;
    string_free(path);
    return file;
}

static bool test_cfg_database_write(const char *directory, const char *name, const char *text)
{
    file_t *file = test_cfg_database_file(directory, name);
    string_t *body = string_new_with(text);
    bool ok = file && body && file_write_all_text(file, body);
    string_free(body);
    file_free(file);
    return ok;
}

static bool test_cfg_database_fixture(const char *directory, const char *sql)
{
    file_t *folder = test_cfg_database_file(directory, "packaging/almanac-db");
    bool ok = folder && file_create_directory(folder, 0700, true) && !setenv("MARS_ROOT", directory, 1) &&
              test_cfg_database_write(directory, "packaging/almanac-db/mars_almanac.sql", sql);
    file_free(folder);
    return ok;
}

static bool test_cfg_database_text(const string_t *actual, const char *expected)
{
    return actual && string_view_equals_literal(string_view_all(actual), expected);
}

static bool test_cfg_database_setting(file_t *file, const char *name, const char *expected)
{
    string_t *value = cfg_storage_setting(file, name);
    bool ok = test_cfg_database_text(value, expected);
    string_free(value);
    return ok;
}

static sqlite_t *test_cfg_database_open(file_t *file, const char *password)
{
    string_t *path = string_new_with(file_path(file)), *key = string_new_with(password);
    sqlite_t *db = path && key ? sqlite_open_encrypted(path, key) : NULL;
    string_free(key);
    string_free(path);
    return db;
}

static bool test_cfg_database_query(sqlite_t *db, const char *query, const char *expected)
{
    sqlite_stmt_t *statement = db ? sqlite_stmt_prepare(db, query) : NULL;
    bool ok = statement && sqlite_stmt_step(statement) == SQLITE_STEP_ROW;
    const char *value = ok ? sqlite_stmt_column_text(statement, 0) : NULL;
    ok = ok && value && !strcmp(value, expected) && sqlite_stmt_step(statement) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(statement);
    return ok;
}

static bool test_cfg_database_mode(file_t *file, unsigned mode)
{
    file_info_t *info = file_get_info(file);
    bool ok = info && file_info_permissions(info) == mode;
    file_info_free(info);
    return ok;
}

static bool test_cfg_database_equal(file_t *left, file_t *right)
{
    bool ok = file_open_read(left) && file_open_read(right);
    char left_bytes[16384], right_bytes[16384];
    size_t left_count = 0, right_count = 0;
    while (ok) {
        ok = file_read(left, left_bytes, sizeof(left_bytes), &left_count) &&
             file_read(right, right_bytes, sizeof(right_bytes), &right_count) && left_count == right_count &&
             !memcmp(left_bytes, right_bytes, left_count);
        if (!ok || !left_count)
            break;
    }
    if (file_is_open(left))
        ok = file_close(left) && ok;
    if (file_is_open(right))
        ok = file_close(right) && ok;
    return ok;
}

static bool test_cfg_database_stream_fixture(const char *directory)
{
    const char *sql = "-- root source\nBEGIN TRANSACTION;\n"
                      "CREATE TABLE source(value TEXT);\nCREATE TABLE audit(value TEXT);\n"
                      "CREATE TRIGGER source_audit AFTER INSERT ON source BEGIN\n"
                      " INSERT INTO audit VALUES(CASE WHEN new.value <> '' THEN 'semi;colon' ELSE 'empty' END);\n"
                      " INSERT INTO audit VALUES('it''s another; statement');\nEND;\n"
                      "/* comment before nested read; */\n"
                      ".read 'packaging/almanac-db/nested file.sql'\n"
                      "ROLLBACK;\n"
                      "CREATE TABLE retained(value TEXT);\n"
                      "INSERT INTO retained VALUES('committed; text');\n";
    bool ok = test_cfg_database_fixture(directory, sql) &&
              test_cfg_database_write(directory, "packaging/almanac-db/nested file.sql",
                                      "INSERT INTO source VALUES('a; quoted value');\n"
                                      ".read packaging/almanac-db/nested.sql\n") &&
              test_cfg_database_write(directory, "packaging/almanac-db/nested.sql",
                                      "INSERT INTO source VALUES('Québec');\n") &&
              test_cfg_database_write(directory, "packaging/almanac-db/mars_almanac_chebyshev.sql",
                                      "INSERT INTO retained VALUES('supplement one');\n") &&
              test_cfg_database_write(directory, "packaging/almanac-db/mars_almanac_frame_rotation.sql",
                                      "INSERT INTO retained VALUES('supplement two')") &&
              !cfg_database_run(false, NULL, "fixture-'\"-$(false)-`false`-\\-clé", NULL, NULL, false);
    file_t *database = test_cfg_database_file(directory, "almanac/almanac.db");
    file_t *configuration = test_cfg_database_file(directory, "config/almanac-db.env");
    file_t *folder = test_cfg_database_file(directory, "almanac");
    sqlite_t *db = ok ? test_cfg_database_open(database, "fixture-'\"-$(false)-`false`-\\-clé") : NULL;
    ok = ok && db && test_cfg_database_query(db, "SELECT count(*) FROM retained", "3") &&
         test_cfg_database_query(db, "SELECT count(*) FROM sqlite_master WHERE name IN ('source','audit')", "0") &&
         test_cfg_database_setting(configuration, "MARS_ALMANAC_DB_KEY", "fixture-'\"-$(false)-`false`-\\-clé") &&
         test_cfg_database_mode(database, 0600) && test_cfg_database_mode(configuration, 0600) &&
         test_cfg_database_mode(folder, 0700);
    sqlite_close(db);
    /* Reload the apostrophe-containing key exclusively from stored configuration. */
    ok = ok && !cfg_database_run(false, NULL, NULL, NULL, NULL, false);
    file_free(folder);
    file_free(configuration);
    file_free(database);
    return ok;
}

static bool test_cfg_database_precedence_fixture(const char *directory)
{
    bool ok = test_cfg_database_fixture(directory, "CREATE TABLE fixture(value); INSERT INTO fixture VALUES(7);\n");
    file_t *configuration = test_cfg_database_file(directory, "config/almanac-db.env");
    string_t *legacy = string_sprintf("%s/almanac.db", directory);
    string_t *default_path = string_sprintf("%s/almanac/almanac.db", directory);
    string_t *explicit_path = string_sprintf("%s/explicit.db", directory);
    string_t *environment_path = string_sprintf("%s/environment.db", directory);
    ok = ok && configuration && legacy && default_path && explicit_path && environment_path &&
         !setenv("MARS_ALMANAC_DB_KEY", "environment-key", 1) &&
         !setenv("MARS_ALMANAC_DB_PATH", string_c_str(environment_path), 1) &&
         !cfg_database_run(false, string_c_str(explicit_path), " explicit-key ", NULL, NULL, false) &&
         test_cfg_database_setting(configuration, "MARS_ALMANAC_DB_PATH", string_c_str(explicit_path)) &&
         test_cfg_database_setting(configuration, "MARS_ALMANAC_DB_KEY", "explicit-key") &&
         !cfg_database_run(false, NULL, NULL, NULL, NULL, false) &&
         test_cfg_database_setting(configuration, "MARS_ALMANAC_DB_PATH", string_c_str(environment_path)) &&
         test_cfg_database_setting(configuration, "MARS_ALMANAC_DB_KEY", "environment-key") &&
         !unsetenv("MARS_ALMANAC_DB_KEY") && !unsetenv("MARS_ALMANAC_DB_PATH") &&
         !cfg_database_run(false, NULL, NULL, NULL, NULL, false) &&
         test_cfg_database_setting(configuration, "MARS_ALMANAC_DB_KEY", "environment-key") &&
         !cfg_database_run(false, string_c_str(legacy), NULL, NULL, NULL, false) &&
         test_cfg_database_setting(configuration, "MARS_ALMANAC_DB_PATH", string_c_str(default_path));
    file_t *old = legacy ? file_new(legacy) : NULL;
    ok = ok && old && !file_exists(old) && !file_last_error(old);
    file_free(old);
    string_free(legacy);
    string_free(default_path);
    string_free(explicit_path);
    string_free(environment_path);
    file_free(configuration);
    return ok;
}

static bool test_cfg_database_failure_fixture(const char *directory)
{
    bool ok = test_cfg_database_fixture(directory, "CREATE TABLE fixture(value); INSERT INTO fixture VALUES(7);\n") &&
              cfg_database_run(false, NULL, NULL, NULL, NULL, false) == 1 &&
              !cfg_database_run(false, NULL, "original-key", NULL, NULL, false);
    file_t *database = test_cfg_database_file(directory, "almanac/almanac.db");
    file_t *configuration = test_cfg_database_file(directory, "config/almanac-db.env");
    file_t *snapshot = test_cfg_database_file(directory, "snapshot.db");
    file_t *saved_config = test_cfg_database_file(directory, "snapshot.env");
    ok = ok && database && configuration && snapshot && saved_config && file_copy(database, snapshot, false) &&
         file_copy(configuration, saved_config, false);
    static const char *const failures[] = {
        "CREATE TABLE broken(;\n",
        ".read packaging/almanac-db/missing.sql\n",
        ".read packaging/almanac-db/mars_almanac.sql\n",
        ".shell touch must-not-exist\n",
        "BEGIN; CREATE TABLE unfinished(value);\n",
        "CREATE TABLE unfinished(value TEXT); INSERT INTO unfinished VALUES('unterminated;\n"};
    for (size_t i = 0; i < sizeof(failures) / sizeof(*failures) && ok; ++i)
        ok = test_cfg_database_write(directory, "packaging/almanac-db/mars_almanac.sql", failures[i]) &&
             cfg_database_run(false, NULL, "replacement-key", NULL, NULL, false) == 1 &&
             test_cfg_database_equal(database, snapshot) && test_cfg_database_equal(configuration, saved_config);
    ok = ok && cfg_database_run(false, NULL, "bad\nkey", NULL, NULL, false) == 1 &&
         test_cfg_database_equal(database, snapshot) && test_cfg_database_equal(configuration, saved_config);
    file_free(saved_config);
    file_free(snapshot);
    file_free(configuration);
    file_free(database);
    return ok;
}

static bool test_cfg_database_bound_fixture(const char *directory)
{
    bool ok = test_cfg_database_fixture(directory, "CREATE TABLE fixture(value);\n") &&
              !cfg_database_run(false, NULL, "fixture-key", NULL, NULL, false);
    file_t *sql = test_cfg_database_file(directory, "packaging/almanac-db/mars_almanac.sql");
    file_t *database = test_cfg_database_file(directory, "almanac/almanac.db");
    file_t *snapshot = test_cfg_database_file(directory, "snapshot.db");
    ok = ok && sql && database && snapshot && file_copy(database, snapshot, false) && file_open_write(sql);
    char chunk[16384];
    memset(chunk, ' ', sizeof(chunk));
    /* The source exceeds the bounded line size; no unbounded read is permitted. */
    for (size_t i = 0; i < 65 && ok; ++i) {
        size_t written = 0;
        ok = file_write(sql, chunk, sizeof(chunk), &written) && written == sizeof(chunk);
    }
    if (sql && file_is_open(sql))
        ok = file_close(sql) && ok;
    ok = ok && cfg_database_run(false, NULL, NULL, NULL, NULL, false) == 1 &&
         test_cfg_database_equal(database, snapshot);
    file_free(snapshot);
    file_free(database);
    file_free(sql);
    return ok;
}

static bool test_cfg_database_large_source(const char *directory, bool rollback, bool broken)
{
    bool ok = test_cfg_database_fixture(directory, "BEGIN; CREATE TABLE coefficients(value BLOB);\n"
                                                   "INSERT INTO coefficients(value) VALUES\n");
    file_t *sql = test_cfg_database_file(directory, "packaging/almanac-db/mars_almanac.sql");
    ok = ok && sql && file_open(sql, FILE_MODE_APPEND, FILE_ACCESS_WRITE);
    char tuple[519];
    tuple[0] = '(';
    tuple[1] = 'X';
    tuple[2] = '\'';
    memset(tuple + 3, '0', 512);
    tuple[515] = '\'';
    tuple[516] = ')';
    tuple[517] = ',';
    tuple[518] = '\n';
    /* One 19.8 MiB INSERT exceeds the general SQL buffer, as the real Chebyshev file does. */
    for (size_t i = 0; i < 40000 && ok; ++i) {
        if (i == 39999)
            tuple[517] = broken ? ',' : ';';
        size_t written = 0;
        ok = file_write(sql, tuple, 519, &written) && written == 519;
    }
    string_t *tail =
        string_new_with(broken     ? "invalid tuple;\n"
                        : rollback ? "ROLLBACK; CREATE TABLE retained(value); INSERT INTO retained VALUES(9);\n"
                                   : "COMMIT;\n");
    ok = ok && tail && file_write_text(sql, tail);
    if (sql && file_is_open(sql))
        ok = file_close(sql) && ok;
    string_free(tail);
    file_free(sql);
    return ok;
}

static bool test_cfg_database_large_fixture(const char *directory)
{
    bool ok = test_cfg_database_large_source(directory, false, false) &&
              !cfg_database_run(false, NULL, "large-fixture-key", NULL, NULL, false);
    file_t *database = test_cfg_database_file(directory, "almanac/almanac.db");
    file_t *snapshot = test_cfg_database_file(directory, "snapshot.db");
    sqlite_t *db = ok ? test_cfg_database_open(database, "large-fixture-key") : NULL;
    ok = ok && db && test_cfg_database_query(db, "SELECT count(*) FROM coefficients", "40000") &&
         test_cfg_database_query(db, "SELECT min(length(value)) FROM coefficients", "256");
    sqlite_close(db);
    ok = ok && file_copy(database, snapshot, false) && test_cfg_database_large_source(directory, false, true) &&
         cfg_database_run(false, NULL, NULL, NULL, NULL, false) == 1 && test_cfg_database_equal(database, snapshot) &&
         test_cfg_database_large_source(directory, true, false) &&
         !cfg_database_run(false, NULL, NULL, NULL, NULL, false);
    db = ok ? test_cfg_database_open(database, "large-fixture-key") : NULL;
    ok = ok && db && test_cfg_database_query(db, "SELECT value FROM retained", "9") &&
         test_cfg_database_query(db, "SELECT count(*) FROM sqlite_master WHERE name='coefficients'", "0");
    sqlite_close(db);
    file_free(snapshot);
    file_free(database);
    return ok;
}

static bool test_cfg_database_conflict_source(const char *directory, bool broken)
{
    bool ok =
        test_cfg_database_fixture(directory, "BEGIN;\nCREATE TABLE conflict_rows(id INTEGER PRIMARY KEY,value TEXT);\n"
                                             "INSERT INTO conflict_rows VALUES(1,'original');\n"
                                             "InSeRt\n Or\tIgNoRe\n InTo conflict_rows\n (id, value)\n VaLuEs\n");
    file_t *source = test_cfg_database_file(directory, "packaging/almanac-db/mars_almanac.sql");
    ok = ok && source && file_open(source, FILE_MODE_APPEND, FILE_ACCESS_WRITE);
    const char ignored[] = "(1, 'ignore decomposed e\xcc\x81; it''s literal'),\n";
    const char replaced[] = "(2, 'replace decomposed e\xcc\x81; it''s literal'),\n";
    const char middle[] = "(2, 'inserted');\n"
                          "INSERT\nOR\nREPLACE\nINTO conflict_rows\n(id, value)\nVALUES\n";
    const char tail[] = "(2, 'final e\xcc\x81');\n"
                        "CREATE TABLE literals(value TEXT);\n"
                        "INSERT INTO literals VALUES('semi;colon\n.read never-executed.sql\nquote'';tail');\n"
                        "INSERT INTO literals VALUES('trailing comment'); /* multiple\n"
                        "comment lines ;\n*/\n"
                        "/* a complete comment before a recursive source */\n"
                        ".read packaging/almanac-db/conflict_tail.sql\nCOMMIT;\n";
    const char invalid[] = "(2, 'unterminated\n";
    for (size_t i = 0; i < 4096 && ok; ++i) {
        size_t written = 0;
        ok = file_write(source, ignored, sizeof(ignored) - 1, &written) && written == sizeof(ignored) - 1;
    }
    size_t written = 0;
    ok = ok && file_write(source, middle, sizeof(middle) - 1, &written) && written == sizeof(middle) - 1;
    for (size_t i = 0; i < 4096 && ok; ++i) {
        written = 0;
        ok = file_write(source, replaced, sizeof(replaced) - 1, &written) && written == sizeof(replaced) - 1;
    }
    size_t length = broken ? sizeof(invalid) - 1 : sizeof(tail) - 1;
    ok = ok && file_write(source, broken ? invalid : tail, length, &written) && written == length;
    if (source && file_is_open(source))
        ok = file_close(source) && ok;
    file_free(source);
    /* A valid expression must leave the literal fast path before the first batch and execute unchanged. */
    return ok && test_cfg_database_write(directory, "packaging/almanac-db/conflict_tail.sql",
                                         "INSERT OR IGNORE INTO conflict_rows VALUES\n(3, 2+3);\n");
}

static bool test_cfg_database_conflict_fixture(const char *directory)
{
    bool ok = test_cfg_database_conflict_source(directory, false);
    file_t *source = test_cfg_database_file(directory, "packaging/almanac-db/mars_almanac.sql");
    file_t *saved_source = test_cfg_database_file(directory, "source_snapshot.sql");
    file_t *database = test_cfg_database_file(directory, "almanac/almanac.db");
    file_t *configuration = test_cfg_database_file(directory, "config/almanac-db.env");
    file_t *snapshot = test_cfg_database_file(directory, "snapshot.db");
    file_t *saved_config = test_cfg_database_file(directory, "snapshot.env");
    ok = ok && source && saved_source && database && configuration && snapshot && saved_config &&
         file_copy(source, saved_source, false) &&
         !cfg_database_run(false, NULL, "conflict-fixture", NULL, NULL, false) &&
         test_cfg_database_equal(source, saved_source);
    sqlite_t *db = ok ? test_cfg_database_open(database, "conflict-fixture") : NULL;
    ok = ok && db && test_cfg_database_query(db, "SELECT count(*) FROM conflict_rows", "3") &&
         test_cfg_database_query(db, "SELECT value FROM conflict_rows WHERE id=1", "original") &&
         test_cfg_database_query(db, "SELECT hex(value) FROM conflict_rows WHERE id=2", "66696E616C2065CC81") &&
         test_cfg_database_query(db, "SELECT value FROM conflict_rows WHERE id=3", "5") &&
         test_cfg_database_query(db, "SELECT count(*) FROM literals", "2") &&
         test_cfg_database_query(db, "SELECT value FROM literals WHERE rowid=1",
                                 "semi;colon\n.read never-executed.sql\nquote';tail");
    sqlite_close(db);
    ok = ok && file_copy(database, snapshot, false) && file_copy(configuration, saved_config, false) &&
         test_cfg_database_conflict_source(directory, true) &&
         cfg_database_run(false, NULL, "replacement-key", NULL, NULL, false) == 1 &&
         test_cfg_database_equal(database, snapshot) && test_cfg_database_equal(configuration, saved_config);
    file_free(saved_config);
    file_free(snapshot);
    file_free(configuration);
    file_free(database);
    file_free(saved_source);
    file_free(source);
    return ok;
}

static bool test_cfg_database_path_fixture(const char *directory)
{
    bool ok = test_cfg_database_fixture(directory, "CREATE TABLE fixture(value);\n") &&
              !cfg_database_run(false, NULL, "fixture-key", NULL, NULL, false);
    file_t *database = test_cfg_database_file(directory, "almanac/almanac.db");
    file_t *configuration = test_cfg_database_file(directory, "config/almanac-db.env");
    file_t *snapshot = test_cfg_database_file(directory, "snapshot.db");
    file_t *saved_config = test_cfg_database_file(directory, "snapshot.env");
    file_t *link = test_cfg_database_file(directory, "link.db");
    ok = ok && database && configuration && snapshot && saved_config && link && file_copy(database, snapshot, false) &&
         file_copy(configuration, saved_config, false) && file_create_symlink(database, link) &&
         cfg_database_run(false, file_path(link), "replacement-key", NULL, NULL, false) == 1 &&
         test_cfg_database_equal(database, snapshot) && test_cfg_database_equal(configuration, saved_config) &&
         cfg_database_run(false, file_path(configuration), "replacement-key", NULL, NULL, false) == 1 &&
         test_cfg_database_equal(configuration, saved_config);
    file_free(link);
    file_free(saved_config);
    file_free(snapshot);
    file_free(configuration);
    file_free(database);
    return ok;
}

static bool test_cfg_database_almanac_coverage(sqlite_t *db, const char *family, const char *expected_series)
{
    /* Each indexed series range must contain every declared segment and correctly sized coefficient blobs. */
    string_t *query = string_sprintf("SELECT count(*) FROM ("
                                     "SELECT s.series_id FROM almanac_%s_series s "
                                     "JOIN almanac_%s_series_segment_count c USING(series_id) "
                                     "JOIN almanac_%s_series_degree d USING(series_id) "
                                     "JOIN almanac_%s_series_component_count n USING(series_id) "
                                     "LEFT JOIN almanac_%s_segment p USING(series_id) "
                                     "GROUP BY s.series_id "
                                     "HAVING count(p.segment_index)=c.segment_count "
                                     "AND min(p.segment_index)=0 AND max(p.segment_index)=c.segment_count-1 "
                                     "AND min(length(p.coefficient_blob))=8*(d.degree+1)*n.component_count "
                                     "AND max(length(p.coefficient_blob))=8*(d.degree+1)*n.component_count)",
                                     family, family, family, family, family);
    bool ok = query && test_cfg_database_query(db, string_c_str(query), expected_series);
    string_free(query);
    return ok;
}

static bool test_cfg_database_packaged_almanac_fixture(const char *directory)
{
    const char *key = "packaged-almanac-'fixture";
    bool ok = !setenv("MARS_ROOT", MARS_CONFIG_ROOT_DIR, 1) && !cfg_database_run(false, NULL, key, NULL, NULL, false);
    file_t *database = test_cfg_database_file(directory, "almanac/almanac.db");
    file_t *configuration = test_cfg_database_file(directory, "config/almanac-db.env");
    ok = ok && database && configuration && test_cfg_database_mode(database, 0600) &&
         test_cfg_database_mode(configuration, 0600) &&
         test_cfg_database_setting(configuration, "MARS_ALMANAC_DB_KEY", key) &&
         test_cfg_database_setting(configuration, "MARS_ALMANAC_DB_PATH", file_path(database));
    sqlite_t *db = ok ? test_cfg_database_open(database, key) : NULL;
    ok = ok && db && test_cfg_database_query(db, "SELECT count(*)>0 FROM almanac_body", "1") &&
         test_cfg_database_query(db, "SELECT count(*) FROM almanac_chebyshev_position_series", "7") &&
         test_cfg_database_query(db, "SELECT count(*) FROM almanac_chebyshev_position_segment", "188332") &&
         test_cfg_database_query(db, "SELECT count(*) FROM almanac_frame_rotation_series", "1") &&
         test_cfg_database_query(db, "SELECT count(*) FROM almanac_frame_rotation_segment", "1100") &&
         test_cfg_database_almanac_coverage(db, "chebyshev_position", "7") &&
         test_cfg_database_almanac_coverage(db, "frame_rotation", "1");
    sqlite_close(db);
    /* Opening with an incorrect key must fail; it must not prevent a later correct-key reopen. */
    db = ok ? test_cfg_database_open(database, "incorrect-packaged-almanac-key") : NULL;
    ok = ok && !db;
    sqlite_close(db);
    db = ok ? test_cfg_database_open(database, key) : NULL;
    ok = ok && db && test_cfg_database_query(db, "SELECT count(*) FROM almanac_chebyshev_position_segment", "188332");
    sqlite_close(db);
    file_free(configuration);
    file_free(database);
    return ok;
}

static bool test_cfg_database_jurisdiction_fixture(const char *directory)
{
    bool ok = !setenv("MARS_ROOT", MARS_CONFIG_ROOT_DIR, 1) && !unsetenv("MARS_CALENDAR_LOCATION_ARGUMENT") &&
              !unsetenv("MARS_CALENDAR_LANGUAGE_ARGUMENT") && !setenv("LANG", "en_GB.UTF-8", 1) &&
              !cfg_database_run(true, NULL, "jurisdiction-'fixture", "limmen", NULL, false);
    file_t *database = test_cfg_database_file(directory, "jurisdiction/mars_jurisdiction_rules.db");
    file_t *configuration = test_cfg_database_file(directory, "config/jurisdiction-db.env");
    file_t *snapshot = test_cfg_database_file(directory, "snapshot.db");
    file_t *saved_config = test_cfg_database_file(directory, "snapshot.env");
    sqlite_t *db = ok ? test_cfg_database_open(database, "jurisdiction-'fixture") : NULL;
    ok = ok && db && test_cfg_database_setting(configuration, "MARS_CALENDAR_LOCATION", "Limmen, NL") &&
         test_cfg_database_setting(configuration, "MARS_CALENDAR_LANGUAGE", "nl_NL") &&
         test_cfg_database_query(db,
                                 "SELECT location||'|'||jurisdiction||'|'||timezone||'|'||locale "
                                 "FROM calendar_local_settings",
                                 "Limmen|NL|Europe/Amsterdam|nl_NL") &&
         test_cfg_database_query(db,
                                 "SELECT [Day Name]||'|'||[Month Name Abbrev] FROM calendar_local "
                                 "WHERE FullDateAlternateKey='2024-06-21'",
                                 "vrijdag|jun") &&
         test_cfg_database_query(db,
                                 "SELECT CAST(utc_offset_hours AS INTEGER) FROM calendar_local_days "
                                 "WHERE calendar_date='2024-01-01'",
                                 "1") &&
         test_cfg_database_query(db,
                                 "SELECT CAST(utc_offset_hours AS INTEGER) FROM calendar_local_days "
                                 "WHERE calendar_date='2024-06-21'",
                                 "2") &&
         test_cfg_database_mode(database, 0600) && test_cfg_database_mode(configuration, 0600);
    sqlite_close(db);
    ok = ok && !cfg_database_run(true, NULL, NULL, NULL, NULL, false) &&
         test_cfg_database_setting(configuration, "MARS_CALENDAR_LOCATION", "Limmen, NL") &&
         test_cfg_database_setting(configuration, "MARS_CALENDAR_LANGUAGE", "nl_NL") &&
         file_copy(database, snapshot, false) && file_copy(configuration, saved_config, false) &&
         cfg_database_run(true, NULL, NULL, "no-such-town-9182", NULL, false) == 1 &&
         test_cfg_database_equal(database, snapshot) && test_cfg_database_equal(configuration, saved_config) &&
         cfg_database_run(true, NULL, NULL, "limmen", "Latin", false) == 1 &&
         test_cfg_database_equal(database, snapshot) && test_cfg_database_equal(configuration, saved_config);
    file_free(saved_config);
    file_free(snapshot);
    file_free(configuration);
    file_free(database);
    return ok;
}

static void test_cfg_database_stream(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(test_cfg_database_stream_fixture),
                     "native recursive imports preserve triggers, transactions, supplements and literal keys");
}

static void test_cfg_database_precedence(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(test_cfg_database_precedence_fixture),
                     "CLI, environment and saved database settings retain precedence and migrate legacy paths");
}

static void test_cfg_database_failure(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(test_cfg_database_failure_fixture),
                     "SQL, recursive includes and incomplete transactions cannot replace existing private data");
}

static void test_cfg_database_bounds(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(test_cfg_database_bound_fixture),
                     "an excessive SQL line fails with the previous encrypted database unchanged");
}

static void test_cfg_database_jurisdiction(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(test_cfg_database_jurisdiction_fixture),
                     "native jurisdiction installation persists choices and preserves files after invalid selections");
}

static void test_cfg_database_large(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(test_cfg_database_large_fixture),
                     "large literal INSERTs use bounded batches and preserve rollback and failed replacements");
}

static void test_cfg_database_paths(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(test_cfg_database_path_fixture),
                     "symlink database destinations and aliased configuration paths cannot replace private files");
}

static void test_cfg_database_packaged_almanac(void)
{
    TEST_ASSERT_TRUE(
        test_cfg_isolated(test_cfg_database_packaged_almanac_fixture),
        "actual packaged almanac imports complete coefficient coverage and rejects incorrect encryption keys");
}

static void test_cfg_database_conflicts(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(test_cfg_database_conflict_fixture),
                     "multiline conflict headers retain conflict policies, exact SQL text and failure preservation");
}

static void test_cfg_database_trace(void)
{
    static const char *const sources[] = {
        "packaging/almanac-db/mars_almanac.sql",
        "packaging/almanac-db/mars_almanac_chebyshev.sql",
        "packaging/almanac-db/mars_almanac_frame_rotation.sql",
        "packaging/jurisdiction-db/mars_holiday_rules.sql",
        "packaging/jurisdiction-db/mars_country_jurisdictions.sql",
        "packaging/jurisdiction-db/mars_target_subdivisions.sql",
        "packaging/jurisdiction-db/mars_timezone_rules.sql",
        "packaging/jurisdiction-db/mars_jurisdiction_location_defaults.sql",
        "packaging/jurisdiction-db/mars_jurisdiction_towns.sql",
        "packaging/jurisdiction-db/mars_generated_first_class_rules.sql",
        "packaging/jurisdiction-db/mars_manual_first_class_rules.sql",
        "packaging/jurisdiction-db/mars_holiday_localized_names.sql",
        "packaging/jurisdiction-db/mars_calendar_locale_names.sql",
        "packaging/jurisdiction-db/mars_calendar_local.sql",
    };
    bool recognised = true, redacted = true;
    for (size_t i = 0; i < sizeof(sources) / sizeof(*sources); ++i) {
        string_t *path = string_new_with(sources[i]);
        recognised = recognised && path && strcmp(cfg_database_trace_label(path), sources[i]) == 0;
        string_free(path);
        /* Changing only the first byte preserves both hash inputs. */
        path = string_sprintf("X%s", sources[i] + 1);
        redacted = redacted && path && strcmp(cfg_database_trace_label(path), "unlisted SQL source") == 0;
        string_free(path);
    }
    static const char *const unknown[] = {
        "", "short.sql", "packaging/almanac-db/mars_almanac.sql.extra",
        "packaging/jurisdiction-db/mars_jurisdiction_location_defaults.sql.extra",
        "packaging/almanac-db/mars_almanac.éql"
    };
    for (size_t i = 0; i < sizeof(unknown) / sizeof(*unknown); ++i) {
        string_t *path = string_new_with(unknown[i]);
        redacted = redacted && path && strcmp(cfg_database_trace_label(path), "unlisted SQL source") == 0;
        string_free(path);
    }
    TEST_ASSERT_TRUE(recognised, "every allowlisted SQL source has its exact diagnostic label");
    TEST_ASSERT_TRUE(redacted, "colliding, unknown and out-of-range source paths remain redacted");
    TEST_ASSERT_TRUE(strcmp(cfg_database_trace_label(NULL), "unlisted SQL source") == 0,
                     "a missing source path remains redacted");
}

/* Called only by the parent's sequential native configuration test runner. */
void test_cfg_database_cases(void)
{
    TEST_RUN_IN_GROUP(test_cfg_database_trace, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_database_stream, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_database_precedence, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_database_failure, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_database_bounds, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_database_large, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_database_conflicts, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_database_paths, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_database_packaged_almanac, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_database_jurisdiction, tests, NULL);
}
