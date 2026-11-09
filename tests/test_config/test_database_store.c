/**
 * @file test_database_store.c
 * @brief Lazy, bounded raw-SQLite storage for the test catalogue and transactional result history.
 *
 * Called only by test execution or explicit administration. Validates the application identifier and schema
 * version before writing, rejects unrelated databases, and uses a separate connection from any test fixture.
 * File access uses the public file module; SQL execution bypasses the MARS sqlite wrapper under test.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "file-t.h"
#define MARS_TEST_DATABASE_INTERNAL_ACCESS
#include "test_database_internal.h"

/* Copy a diagnostic without using the database module's own error machinery. */
bool test_database_fail(test_database_t *database, const char *text)
{
    if (database) {
        string_free(database->error);
        database->error = string_new_with(text ? text : sqlite3_errmsg(database->connection));
    }
    return false;
}

/* Run only trusted schema and housekeeping SQL. */
bool test_database_exec(test_database_t *database, const char *sql)
{
    return database && sqlite3_exec(database->connection, sql, NULL, NULL, NULL) == SQLITE_OK ? true
                                                                                         : test_database_fail(database, NULL);
}

/* Prepare owned statements with a single error boundary. */
sqlite3_stmt *test_database_prepare(test_database_t *database, const char *sql)
{
    sqlite3_stmt *statement = NULL;
    if (!database || sqlite3_prepare_v2(database->connection, sql, -1, &statement, NULL) != SQLITE_OK) {
        sqlite3_finalize(statement);
        test_database_fail(database, NULL);
        return NULL;
    }
    return statement;
}

/* Always release a completed write statement. */
bool test_database_write(test_database_t *database, sqlite3_stmt *statement)
{
    bool ok = statement && sqlite3_step(statement) == SQLITE_DONE;
    if (!ok)
        test_database_fail(database, NULL);
    sqlite3_finalize(statement);
    return ok;
}

static int test_database_integer(test_database_t *database, const char *sql)
{
    sqlite3_stmt *statement = test_database_prepare(database, sql);
    int value = statement && sqlite3_step(statement) == SQLITE_ROW ? sqlite3_column_int(statement, 0) : -1;
    sqlite3_finalize(statement);
    return value;
}

/* Construct no files until an explicit caller opens storage. */
test_database_t *test_database_open(const string_t *root, const string_t *path, bool create, string_t **error)
{
    if (error)
        *error = NULL;
    const char *base = root ? string_c_str(root) : ".";
    string_t *owned_path = path ? string_clone(path) : string_sprintf("%s/tests/tests.db", base);
    test_database_t *database = calloc(1u, sizeof *database);
    bool initialise = false;
    bool ok = owned_path && database && (!root || strlen(base) == string_byte_length(root)) &&
              strlen(string_c_str(owned_path)) == string_byte_length(owned_path);
    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX | (create ? SQLITE_OPEN_CREATE : 0);
    if (ok)
        ok = sqlite3_open_v2(string_c_str(owned_path), &database->connection, flags, NULL) == SQLITE_OK;
    if (ok) {
        sqlite3_busy_timeout(database->connection, 1000);
        int application = test_database_integer(database, "PRAGMA application_id");
        int version = test_database_integer(database, "PRAGMA user_version");
        bool empty = application == 0 && version == 0 &&
                     test_database_integer(database, "SELECT count(*) FROM sqlite_master WHERE name NOT LIKE 'sqlite_%'") == 0;
        initialise = create && (empty || (application == 1297371988 && version > 0 && version < 4));
        ok = (application == 1297371988 && version >= 1 && version <= 4) || (create && empty);
        if (!ok)
            test_database_fail(database, "Not a compatible MARS test database; the file was left unchanged.");
    }
    if (ok) {
        int page_size = test_database_integer(database, "PRAGMA page_size");
        string_t *limit = page_size > 0 ? string_sprintf("PRAGMA max_page_count = %u", 67108864u / (unsigned)page_size) : NULL;
        ok = limit && test_database_exec(database, "PRAGMA foreign_keys = ON") &&
             test_database_exec(database, string_c_str(limit));
        string_free(limit);
    }
    if (ok && initialise)
        ok = test_database_exec(database, "BEGIN IMMEDIATE") && test_database_seed(database, root) &&
             test_database_exec(database, "COMMIT");
    if (ok && create && !path)
        ok = test_database_reports_migrate(database, root);
    string_free(owned_path);
    if (!ok) {
        if (error)
            *error = string_new_with(database && database->error ? string_c_str(database->error) :
                                    database && database->connection ? sqlite3_errmsg(database->connection) :
                                    "Cannot allocate or locate test storage.");
        test_database_close(database);
        return NULL;
    }
    return database;
}

/* Close independently of any connection under test. */
void test_database_close(test_database_t *database)
{
    if (!database)
        return;
    if (database->connection) {
        sqlite3_exec(database->connection, "ROLLBACK", NULL, NULL, NULL);
        sqlite3_close(database->connection);
    }
    string_free(database->error);
    free(database);
}

/* Expose a borrowed diagnostic only. */
const char *test_database_error(const test_database_t *database)
{
    return database && database->error ? string_c_str(database->error) : "Test database operation failed.";
}

/* Catalogue keys use the source path and complete group path, never display names alone. */
bool test_database_catalogue(test_database_t *database, const test_database_result_t *result)
{
    if (!result || !result->source || !result->path || !result->name)
        return test_database_fail(database, "Incomplete test catalogue record.");
    sqlite3_stmt *statement = test_database_prepare(database,
        "INSERT INTO test_catalogue(source, path, name, parent, tags, kind, declaration_line)\n"
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)\n"
        "ON CONFLICT(source, path) DO UPDATE SET\n"
        "    confirmed = 1,\n"
        "    tags = CASE WHEN excluded.tags <> '' THEN excluded.tags ELSE test_catalogue.tags END,\n"
        "    kind = CASE WHEN excluded.declaration_line > 0 THEN excluded.kind ELSE test_catalogue.kind END,\n"
        "    declaration_line = CASE WHEN excluded.declaration_line > 0\n"
        "                            THEN excluded.declaration_line ELSE test_catalogue.declaration_line END");
    if (!statement)
        return false;
    sqlite3_bind_text(statement, 1, result->source, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, result->path, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, result->name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 4, result->parent ? result->parent : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 5, result->tags ? result->tags : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 6, result->is_output ? "readme" : result->is_group ? "group" : "test", -1, SQLITE_STATIC);
    sqlite3_bind_int(statement, 7, result->declaration_line);
    bool ok = test_database_write(database, statement);
    if (ok && database->importing) {
        statement = test_database_prepare(database, "INSERT OR IGNORE INTO catalogue_seen(source, path) VALUES (?1, ?2)");
        if (!statement)
            return false;
        sqlite3_bind_text(statement, 1, result->source, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 2, result->path, -1, SQLITE_TRANSIENT);
        ok = test_database_write(database, statement);
    }
    return ok;
}

/* Begin a post-execution report transaction. */
bool test_database_run_begin(test_database_t *database, const char *executable, const char *profile, bool memory_test)
{
    if (!database || database->run_id || !test_database_exec(database, "BEGIN IMMEDIATE"))
        return false;
    sqlite3_stmt *statement = test_database_prepare(database,
        "INSERT INTO test_run(executable, profile, memory_test) VALUES (?1, ?2, ?3)");
    if (!statement)
        return false;
    sqlite3_bind_text(statement, 1, executable ? executable : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, profile ? profile : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 3, memory_test);
    bool ok = test_database_write(database, statement);
    if (ok) {
        database->run_id = sqlite3_last_insert_rowid(database->connection);
        database->ordinal = 0u;
        database->memory_test = memory_test;
    }
    return ok;
}

/* Copy one result without retaining test-owned strings or handles. */
bool test_database_run_record(test_database_t *database, const test_database_result_t *result)
{
    if (!database || !database->run_id || !result || !isfinite(result->elapsed_ms) || result->elapsed_ms < 0 ||
        !test_database_catalogue(database, result))
        return false;
    test_database_result_t completed = *result;
    char timestamp[32];
    if (!completed.finished_utc) {
        struct timespec now;
        struct tm utc;
        if (clock_gettime(CLOCK_REALTIME, &now) || !gmtime_r(&now.tv_sec, &utc) ||
            !strftime(timestamp, sizeof timestamp, "%Y-%m-%dT%H:%M:%S", &utc))
            return test_database_fail(database, "Cannot determine the case completion time.");
        snprintf(timestamp + 19, sizeof timestamp - 19u, ".%03uZ", (unsigned)(now.tv_nsec / 1000000L));
        completed.finished_utc = timestamp;
    }
    test_timing_assessment_t timing = {0};
    if (!test_database_history_assess(database, &completed, database->memory_test, &timing))
        return false;
    sqlite3_stmt *statement = test_database_prepare(database,
        "INSERT INTO test_result(\n"
        "    run_id, ordinal, source, path, outcome, enabled, elapsed_ms, detail, finished_utc,\n"
        "    timing_outlier, baseline_median_ms, outlier_threshold_ms\n"
        ") VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, substr(?8, 1, 4096), ?9, ?10, ?11, ?12)");
    if (!statement)
        return false;
    sqlite3_bind_int64(statement, 1, database->run_id);
    sqlite3_bind_int64(statement, 2, (sqlite3_int64)database->ordinal++);
    sqlite3_bind_text(statement, 3, result->source, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 4, result->path, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 5, result->outcome, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 6, result->enabled);
    sqlite3_bind_double(statement, 7, result->elapsed_ms);
    sqlite3_bind_text(statement, 8, result->detail ? result->detail : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 9, completed.finished_utc, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 10, timing.outlier);
    if (timing.available) {
        sqlite3_bind_double(statement, 11, timing.median_ms);
        sqlite3_bind_double(statement, 12, timing.threshold_ms);
    }
    return test_database_write(database, statement) &&
           test_database_history_record(database, &completed, database->memory_test, timing.outlier);
}

/* Retention and the new report commit together; failures roll back on close. */
bool test_database_run_finish(test_database_t *database, int exit_code, double elapsed_ms)
{
    if (!database || !database->run_id || !isfinite(elapsed_ms) || elapsed_ms < 0)
        return false;
    sqlite3_stmt *statement = test_database_prepare(database,
        "UPDATE test_run SET finished_utc = strftime('%Y-%m-%dT%H:%M:%fZ', 'now'),\n"
        "    started_utc = strftime('%Y-%m-%dT%H:%M:%fZ', 'now', printf('-%f seconds', ?2 / 1000.0)),\n"
        "    exit_code = ?1, elapsed_ms = ?2 WHERE id = ?3");
    if (!statement)
        return false;
    sqlite3_bind_int(statement, 1, exit_code);
    sqlite3_bind_double(statement, 2, elapsed_ms);
    sqlite3_bind_int64(statement, 3, database->run_id);
    bool ok = test_database_write(database, statement) && test_database_exec(database,
        "DELETE FROM test_run WHERE id IN (SELECT id FROM test_run ORDER BY id DESC LIMIT -1 OFFSET 200);\n"
        "DELETE FROM test_result WHERE (run_id, ordinal) IN (\n"
        "    SELECT run_id, ordinal FROM test_result ORDER BY run_id DESC, ordinal DESC LIMIT -1 OFFSET 50000\n"
        ");") && test_database_exec(database, "COMMIT");
    if (ok)
        database->run_id = 0;
    return ok;
}

/* Fixtures are created only at explicit, test-owned paths and are never copied from shared history. */
bool test_database_fixture(const string_t *root, const string_t *destination, string_t **error)
{
    file_t *file = destination ? file_new(destination) : NULL;
    bool available = file && !file_exists(file);
    file_free(file);
    if (!available) {
        if (error)
            *error = string_new_with("The disposable test fixture destination already exists or is invalid.");
        return false;
    }
    test_database_t *database = test_database_open(root, destination, true, error);
    bool ok = database != NULL;
    test_database_close(database);
    return ok;
}
