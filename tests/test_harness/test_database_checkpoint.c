/**
 * @file test_database_checkpoint.c
 * @brief Content identities and bounded successful-verification checkpoints for explicit test execution.
 *
 * Stored selections and registered case contents join the authored SQL digest. Result history is excluded,
 * so recording a run cannot invalidate its own checkpoint. Checkpoints keep one row per executable or README example;
 * they survive process interruption and make clean without creating fabricated measurements or deleting old evidence.
 */
#include <stdint.h>
#include <string.h>
#include <sodium.h>

#include "string-t.h"
#define MARS_TEST_DATABASE_INTERNAL_ACCESS
#include "test_database_internal.h"

static void test_checkpoint_hash(crypto_hash_sha256_state *hash, const void *data, size_t size)
{
    crypto_hash_sha256_update(hash, (const unsigned char *)&size, sizeof size);
    if (size) crypto_hash_sha256_update(hash, data, size);
}

static bool test_checkpoint_hash_query(test_database_t *database, crypto_hash_sha256_state *hash, const char *sql)
{
    sqlite3_stmt *statement = test_database_prepare(database, sql);
    bool ok = statement != NULL;
    int status = SQLITE_DONE;
    test_checkpoint_hash(hash, sql, strlen(sql));
    while (ok && (status = sqlite3_step(statement)) == SQLITE_ROW) {
        unsigned char row = 1;
        test_checkpoint_hash(hash, &row, sizeof row);
        for (int i = 0; i < sqlite3_column_count(statement); ++i) {
            int type = sqlite3_column_type(statement, i);
            test_checkpoint_hash(hash, &type, sizeof type);
            const void *value = sqlite3_column_blob(statement, i);
            size_t size = (size_t)sqlite3_column_bytes(statement, i);
            ok = (!size || value) && ok;
            if (ok) test_checkpoint_hash(hash, value, size);
        }
    }
    sqlite3_finalize(statement);
    return ok && status == SQLITE_DONE ? true : test_database_fail(database, NULL);
}

/* Read selection and case data independently of changing report/history tables. */
string_t *test_database_inputs_fingerprint(test_database_t *database, const string_t *root)
{
    if (!database) return NULL;
    string_t *sources = test_database_definitions_fingerprint(root);
    crypto_hash_sha256_state hash;
    crypto_hash_sha256_init(&hash);
    bool ok = sources && test_database_exec(database, "BEGIN") &&
        test_checkpoint_hash_query(database, &hash, "SELECT * FROM test_profile ORDER BY name") &&
        test_checkpoint_hash_query(database, &hash, "SELECT * FROM test_selection ORDER BY profile,source,path") &&
        test_checkpoint_hash_query(database, &hash, "SELECT * FROM test_case_runner ORDER BY module");
    if (sources) test_checkpoint_hash(&hash, string_c_str(sources), string_byte_length(sources));
    sqlite3_stmt *tables = ok ? test_database_prepare(database,
        "SELECT DISTINCT table_name FROM test_case_runner ORDER BY table_name") : NULL;
    int status = SQLITE_DONE;
    ok = tables && ok;
    while (ok && (status = sqlite3_step(tables)) == SQLITE_ROW) {
        const char *table = (const char *)sqlite3_column_text(tables, 0);
        char *sql = table ? sqlite3_mprintf("SELECT * FROM \"%w\" ORDER BY name", table) : NULL;
        ok = sql && test_checkpoint_hash_query(database, &hash, sql);
        sqlite3_free(sql);
    }
    sqlite3_finalize(tables);
    if (ok && status == SQLITE_DONE) ok = test_database_exec(database, "COMMIT");
    else { test_database_exec(database, "ROLLBACK"); ok = false; }
    unsigned char digest[crypto_hash_sha256_BYTES];
    char hexadecimal[crypto_hash_sha256_BYTES * 2u + 1u];
    crypto_hash_sha256_final(&hash, digest);
    sodium_bin2hex(hexadecimal, sizeof hexadecimal, digest, sizeof digest);
    string_free(sources);
    return ok ? string_new_with(hexadecimal) : NULL;
}

/* Query the indexed verification identity without inventing a new test result. */
bool test_database_checkpoint_read(test_database_t *database, const char *scope, const char *fingerprint, bool *matched,
                                  size_t *passed)
{
    if (matched) *matched = false;
    if (passed) *passed = 0;
    if (!database || !scope || !*scope || !fingerprint || strlen(fingerprint) != 64 || !matched)
        return test_database_fail(database, "Invalid verification checkpoint lookup.");
    sqlite3_stmt *exists = test_database_prepare(database,
        "SELECT count(*) FROM sqlite_master WHERE type='table' AND name='test_checkpoint'\n"
        "AND (SELECT user_version FROM pragma_user_version) = 8");
    bool ok = exists && sqlite3_step(exists) == SQLITE_ROW;
    bool present = ok && sqlite3_column_int(exists, 0);
    sqlite3_finalize(exists);
    if (!ok || !present) return ok;
    sqlite3_stmt *statement = test_database_prepare(database,
        "SELECT fingerprint=?2,passed_count FROM test_checkpoint WHERE scope=?1");
    if (statement) {
        sqlite3_bind_text(statement, 1, scope, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 2, fingerprint, -1, SQLITE_TRANSIENT);
    }
    int status = statement ? sqlite3_step(statement) : SQLITE_ERROR;
    if (status == SQLITE_ROW) {
        bool known = sqlite3_column_type(statement, 1) == SQLITE_INTEGER;
        sqlite3_int64 count = known ? sqlite3_column_int64(statement, 1) : 0;
        *matched = sqlite3_column_int(statement, 0) != 0 && (!passed || (known && count >= 0 &&
            (uint64_t)count <= SIZE_MAX));
        if (passed && *matched) *passed = (size_t)count;
    }
    sqlite3_finalize(statement);
    return status == SQLITE_ROW || status == SQLITE_DONE ? true : test_database_fail(database, NULL);
}

/* One autocommit update is durable before the next independent programme starts. */
bool test_database_checkpoint_write(test_database_t *database, const char *scope, const char *fingerprint,
                                   const size_t *passed)
{
    if (!database || !scope || !*scope || (fingerprint && strlen(fingerprint) != 64) ||
        (passed && *passed > INT64_MAX))
        return test_database_fail(database, "Invalid verification checkpoint update.");
    sqlite3_stmt *statement = test_database_prepare(database, fingerprint ?
        "INSERT INTO test_checkpoint(scope,fingerprint,passed_count) VALUES (?1,?2,?3)\n"
        "ON CONFLICT(scope) DO UPDATE SET fingerprint=excluded.fingerprint,passed_count=excluded.passed_count,\n"
        "finished_utc=excluded.finished_utc" :
        "DELETE FROM test_checkpoint WHERE scope=?1");
    if (!statement) return false;
    sqlite3_bind_text(statement, 1, scope, -1, SQLITE_TRANSIENT);
    if (fingerprint) sqlite3_bind_text(statement, 2, fingerprint, -1, SQLITE_TRANSIENT);
    if (fingerprint && passed) sqlite3_bind_int64(statement, 3, (sqlite3_int64)*passed);
    return test_database_write(database, statement);
}
