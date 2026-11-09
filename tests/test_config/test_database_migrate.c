/**
 * @file test_database_migrate.c
 * @brief Consolidate the former separate result database without losing definitions or duplicating reports.
 *
 * Only recognised legacy test databases are accepted. Definitions and profiles are merged without replacing
 * existing rows; completed reports retain their timestamps and acquire exact lifetime statistics. Content
 * fingerprints make retry safe after a failed cleanup. The source is removed only after the destination commits.
 */
#include <stdint.h>
#include <string.h>
#include <sodium.h>

#include "file-t.h"
#define MARS_TEST_DATABASE_INTERNAL_ACCESS
#include "test_database_internal.h"

static sqlite3_int64 test_migrate_integer(test_database_t *database, const char *sql)
{
    sqlite3_stmt *statement = test_database_prepare(database, sql);
    sqlite3_int64 value = statement && sqlite3_step(statement) == SQLITE_ROW ? sqlite3_column_int64(statement, 0) : -1;
    sqlite3_finalize(statement);
    return value;
}

static bool test_migrate_table(test_database_t *database, test_database_t *source, const char *table, const char *schema)
{
    sqlite3_stmt *check = test_database_prepare(database, "SELECT count(*) FROM sqlite_master WHERE name = ?1");
    if (!check)
        return false;
    sqlite3_bind_text(check, 1, table, -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(check) == SQLITE_ROW;
    bool exists = ok && sqlite3_column_int(check, 0) != 0;
    sqlite3_finalize(check);
    ok = ok && (exists || test_database_exec(database, schema));
    char *query = sqlite3_mprintf("SELECT * FROM \"%w\"", table);
    sqlite3_stmt *rows = ok && query ? test_database_prepare(source, query) : NULL;
    sqlite3_free(query);
    string_t *columns = string_new(), *bindings = string_new();
    for (int i = 0; rows && columns && bindings && i < sqlite3_column_count(rows); ++i) {
        char *quoted = sqlite3_mprintf("%s\"%w\"", i ? ", " : "", sqlite3_column_name(rows, i));
        string_t *parameter = string_sprintf("%s?%d", i ? ", " : "", i + 1);
        ok = quoted && parameter && !string_append_cstr(columns, quoted) &&
             !string_append_string(bindings, parameter);
        sqlite3_free(quoted);
        string_free(parameter);
        if (!ok)
            break;
    }
    query = ok && rows && columns && bindings ? sqlite3_mprintf(
        "INSERT OR IGNORE INTO \"%w\"(%s) VALUES (%s)", table, string_c_str(columns), string_c_str(bindings)) : NULL;
    sqlite3_stmt *insert = query ? test_database_prepare(database, query) : NULL;
    sqlite3_free(query);
    ok = ok && insert;
    int status = SQLITE_DONE;
    while (ok && (status = sqlite3_step(rows)) == SQLITE_ROW) {
        sqlite3_reset(insert);
        for (int i = 0; i < sqlite3_column_count(rows); ++i)
            sqlite3_bind_value(insert, i + 1, sqlite3_column_value(rows, i));
        ok = sqlite3_step(insert) == SQLITE_DONE;
    }
    sqlite3_finalize(insert);
    sqlite3_finalize(rows);
    string_free(columns);
    string_free(bindings);
    return ok && status == SQLITE_DONE;
}

static void test_migrate_hash_row(crypto_hash_sha256_state *hash, sqlite3_stmt *statement)
{
    for (int i = 0; i < sqlite3_column_count(statement); ++i) {
        unsigned char type = (unsigned char)sqlite3_column_type(statement, i);
        const unsigned char *bytes = sqlite3_column_text(statement, i);
        size_t size = (size_t)sqlite3_column_bytes(statement, i);
        unsigned char length[8];
        for (size_t j = 0; j < sizeof length; ++j)
            length[j] = (unsigned char)((uint64_t)size >> (j * 8u));
        crypto_hash_sha256_update(hash, &type, 1u);
        crypto_hash_sha256_update(hash, length, sizeof length);
        if (size)
            crypto_hash_sha256_update(hash, bytes, size);
    }
}

static bool test_migrate_report(test_database_t *database, test_database_t *source, sqlite3_stmt *run)
{
    sqlite3_stmt *rows = test_database_prepare(source,
        "SELECT ordinal, source, path, outcome, enabled, elapsed_ms, detail\n"
        "FROM test_result WHERE run_id = ?1 ORDER BY ordinal");
    if (!rows)
        return false;
    sqlite3_bind_int64(rows, 1, sqlite3_column_int64(run, 0));
    crypto_hash_sha256_state hash;
    crypto_hash_sha256_init(&hash);
    test_migrate_hash_row(&hash, run);
    int status;
    while ((status = sqlite3_step(rows)) == SQLITE_ROW)
        test_migrate_hash_row(&hash, rows);
    unsigned char digest[crypto_hash_sha256_BYTES];
    char fingerprint[crypto_hash_sha256_BYTES * 2u + 1u];
    crypto_hash_sha256_final(&hash, digest);
    sodium_bin2hex(fingerprint, sizeof fingerprint, digest, sizeof digest);
    sqlite3_stmt *seen = test_database_prepare(database,
        "SELECT count(*) FROM test_imported_report WHERE fingerprint = ?1");
    bool ok = seen && status == SQLITE_DONE;
    if (seen)
        sqlite3_bind_text(seen, 1, fingerprint, -1, SQLITE_TRANSIENT);
    ok = ok && sqlite3_step(seen) == SQLITE_ROW;
    bool imported = ok && sqlite3_column_int(seen, 0) != 0;
    sqlite3_finalize(seen);
    if (!ok || imported) {
        sqlite3_finalize(rows);
        return ok;
    }
    sqlite3_stmt *insert = test_database_prepare(database,
        "INSERT INTO test_run(executable, profile, started_utc, finished_utc, exit_code, elapsed_ms, memory_test)\n"
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, 0)");
    for (int i = 1; insert && i <= 6; ++i)
        sqlite3_bind_value(insert, i, sqlite3_column_value(run, i));
    ok = test_database_write(database, insert);
    sqlite3_int64 id = sqlite3_last_insert_rowid(database->connection);
    insert = ok ? test_database_prepare(database,
        "INSERT INTO test_result(run_id, ordinal, source, path, outcome, enabled, elapsed_ms, detail, finished_utc)\n"
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9)") : NULL;
    ok = ok && insert;
    sqlite3_reset(rows);
    while (ok && (status = sqlite3_step(rows)) == SQLITE_ROW) {
        sqlite3_reset(insert);
        sqlite3_bind_int64(insert, 1, id);
        for (int i = 0; i < 7; ++i)
            sqlite3_bind_value(insert, i + 2, sqlite3_column_value(rows, i));
        sqlite3_bind_value(insert, 9, sqlite3_column_value(run, 4));
        test_database_result_t result = {
            .source = (const char *)sqlite3_column_text(rows, 1), .path = (const char *)sqlite3_column_text(rows, 2),
            .outcome = (const char *)sqlite3_column_text(rows, 3), .enabled = sqlite3_column_int(rows, 4) != 0,
            .elapsed_ms = sqlite3_column_double(rows, 5), .finished_utc = (const char *)sqlite3_column_text(run, 4)};
        ok = sqlite3_step(insert) == SQLITE_DONE && test_database_history_record(database, &result, false, false);
    }
    ok = ok && status == SQLITE_DONE;
    sqlite3_finalize(insert);
    sqlite3_finalize(rows);
    insert = ok ? test_database_prepare(database,
        "INSERT INTO test_imported_report(fingerprint, run_id) VALUES (?1, ?2)") : NULL;
    if (insert) {
        sqlite3_bind_text(insert, 1, fingerprint, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(insert, 2, id);
    }
    return test_database_write(database, insert);
}

/* Merge only on an explicit writable open of the default database; custom fixtures never inspect another file. */
bool test_database_reports_migrate(test_database_t *database, const string_t *root)
{
    string_t *path = string_sprintf("%s/tests/test_results.db", root ? string_c_str(root) : ".");
    file_t *file = path ? file_new(path) : NULL;
    if (file && !file_exists(file)) {
        file_free(file);
        string_free(path);
        return true;
    }
    string_t *before = file ? file_sha256(file) : NULL;
    test_database_t source = {0};
    bool ok = before && sqlite3_open_v2(string_c_str(path), &source.connection,
                                       SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX, NULL) == SQLITE_OK;
    sqlite3_int64 version = ok ? test_migrate_integer(&source, "PRAGMA user_version") : -1;
    ok = ok && test_migrate_integer(&source, "PRAGMA application_id") == 1297371988 && version >= 1 && version <= 2 &&
         test_migrate_integer(&source, "SELECT count(*) FROM test_run WHERE finished_utc IS NULL") == 0 &&
         test_database_exec(&source, "BEGIN") && test_database_exec(database, "BEGIN IMMEDIATE");
    sqlite3_stmt *tables = ok ? test_database_prepare(&source,
        "SELECT name, sql FROM sqlite_master\n"
        "WHERE type = 'table' AND name NOT LIKE 'sqlite_%' AND name NOT IN ('test_run', 'test_result')\n"
        "ORDER BY CASE name WHEN 'test_profile' THEN 0 WHEN 'test_catalogue' THEN 1 ELSE 2 END, name") : NULL;
    ok = ok && tables;
    int status = SQLITE_DONE;
    while (ok && (status = sqlite3_step(tables)) == SQLITE_ROW)
        ok = test_migrate_table(database, &source, (const char *)sqlite3_column_text(tables, 0),
                                (const char *)sqlite3_column_text(tables, 1));
    ok = ok && status == SQLITE_DONE;
    sqlite3_finalize(tables);
    sqlite3_stmt *runs = ok ? test_database_prepare(&source,
        "SELECT id, executable, profile, started_utc, finished_utc, exit_code, elapsed_ms\n"
        "FROM test_run ORDER BY finished_utc, id") : NULL;
    ok = ok && runs;
    while (ok && (status = sqlite3_step(runs)) == SQLITE_ROW)
        ok = test_migrate_report(database, &source, runs);
    ok = ok && status == SQLITE_DONE;
    sqlite3_finalize(runs);
    if (ok)
        ok = test_database_exec(database, "COMMIT");
    else
        test_database_exec(database, "ROLLBACK");
    if (source.connection) {
        sqlite3_exec(source.connection, "ROLLBACK", NULL, NULL, NULL);
        sqlite3_close(source.connection);
    }
    string_free(source.error);
    string_t *after = ok ? file_sha256(file) : NULL;
    ok = ok && after && !string_compare(before, after) && file_delete(file);
    if (!ok)
        test_database_fail(database, "Cannot complete legacy report migration; the source file was retained for retry.");
    string_free(after);
    string_free(before);
    file_free(file);
    string_free(path);
    return ok;
}
