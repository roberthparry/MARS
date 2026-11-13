/**
 * @file test_database_relocate.c
 * @brief Preserve harness case selections and timing history when regression sources move.
 *
 * Source paths form part of persistent case identities. These explicit relocations update metadata, profile
 * selections, results and timing distributions together, retaining every other field. Conflicting identities
 * abort the transaction rather than silently replacing or dropping either case. Ordinary opens perform only
 * indexed presence checks after the one-off migration has completed.
 */
#include <stddef.h>

#define MARS_TEST_DATABASE_INTERNAL_ACCESS
#include "test_database_internal.h"

typedef struct {
    const char *previous;
    const char *current;
} test_source_move_t;

static bool test_database_source_move(test_database_t *database, const test_source_move_t *move)
{
    static const char *const tables[] = {
        "test_catalogue", "test_selection", "test_result", "test_case_statistics", "test_timing_sample", "test_case_runner"
    };
    for (size_t i = 0; i < sizeof tables / sizeof *tables; ++i) {
        char *sql = sqlite3_mprintf("UPDATE \"%w\" SET source = ?1 WHERE source = ?2", tables[i]);
        sqlite3_stmt *statement = sql ? test_database_prepare(database, sql) : NULL;
        sqlite3_free(sql);
        if (!statement) return false;
        sqlite3_bind_text(statement, 1, move->current, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 2, move->previous, -1, SQLITE_STATIC);
        if (!test_database_write(database, statement)) return false;
    }
    return true;
}

/* Rename only known regression sources, atomically preserving all values and outlier evidence. */
bool test_database_relocate_sources(test_database_t *database)
{
    static const test_source_move_t moves[] = {
        {"tests/test_test_config/test_test_config.c", "tests/test_test_harness/test_test_harness.c"},
        {"tests/test_database/test_database.c", "tests/test_test_harness/test_database.c"}
    };
    sqlite3_stmt *check = test_database_prepare(database,
        "SELECT EXISTS(SELECT 1 FROM test_catalogue WHERE source = ?1 OR source = ?2)\n"
        "    OR EXISTS(SELECT 1 FROM test_case_statistics WHERE source = ?1 OR source = ?2)");
    if (!check) return false;
    sqlite3_bind_text(check, 1, moves[0].previous, -1, SQLITE_STATIC);
    sqlite3_bind_text(check, 2, moves[1].previous, -1, SQLITE_STATIC);
    bool ok = sqlite3_step(check) == SQLITE_ROW;
    bool needed = ok && sqlite3_column_int(check, 0) != 0;
    sqlite3_finalize(check);
    if (!ok || !needed) return ok;
    bool own_transaction = sqlite3_get_autocommit(database->connection) != 0;
    if (own_transaction && !test_database_exec(database, "BEGIN IMMEDIATE")) return false;
    for (size_t i = 0; ok && i < sizeof moves / sizeof *moves; ++i)
        ok = test_database_source_move(database, &moves[i]);
    if (ok && own_transaction) ok = test_database_exec(database, "COMMIT");
    if (!ok && own_transaction) (void)test_database_exec(database, "ROLLBACK");
    return ok;
}
