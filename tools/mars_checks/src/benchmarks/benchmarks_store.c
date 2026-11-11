/**
 * @file benchmarks_store.c
 * @brief Explicit schema upgrades and non-destructive catalogue merging for benchmark storage.
 *
 * Uses the public plaintext SQLite API. Definitions are synchronised by stable names without deleting cases or
 * retained observations. The owning command applies the stored history policy under its writer lock.
 * Storage never participates in library construction, application installation or make clean.
 */
#include <stdio.h>

#include "file-t.h"
#include "checks_support.h"
#define MARS_BENCHMARKS_INTERNAL_ACCESS
#include "benchmarks_internal.h"

static bool benchmark_store_schema(sqlite_t *db, const string_t *root)
{
    sqlite_stmt_t *version = sqlite_stmt_prepare(db, "PRAGMA application_id");
    bool ok = version && sqlite_stmt_step(version) == SQLITE_STEP_ROW;
    int id = ok ? sqlite_stmt_column_int(version, 0) : -1;
    sqlite_stmt_finalize(version);
    if (!ok || (id && id != 1296126546)) return false;
    version = sqlite_stmt_prepare(db, "PRAGMA user_version");
    ok = version && sqlite_stmt_step(version) == SQLITE_STEP_ROW && sqlite_stmt_column_int(version, 0) <= 3;
    sqlite_stmt_finalize(version);
    if (!ok) return false;
    string_t *path = checks_path(root, "bench/benchmarks.sql"), *source = checks_read(path);
    ok = source && sqlite_exec_cstr(db, "BEGIN IMMEDIATE") && sqlite_exec(db, source);
    string_free(source);
    string_free(path);
    sqlite_stmt_t *insert = ok ? sqlite_stmt_prepare(db,
        "INSERT INTO benchmark_operation(name, arity, arguments, complex_mask, qfloat_supported, qcomplex_supported) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6) "
        "ON CONFLICT(name) DO UPDATE SET arity = excluded.arity, arguments = excluded.arguments, "
        "complex_mask = excluded.complex_mask, qfloat_supported = excluded.qfloat_supported, "
        "qcomplex_supported = excluded.qcomplex_supported") : NULL;
    sqlite_stmt_t *workload = ok ? sqlite_stmt_prepare(db,
        "INSERT INTO benchmark_workload(operation, backend, arguments)\n"
        "VALUES (?1, 'exact-integer', ?2)\n"
        "ON CONFLICT(operation, backend) DO UPDATE SET arguments = excluded.arguments") : NULL;
    size_t count = 0;
    const benchmark_operation_t *operations = benchmark_operations(&count);
    ok = ok && insert && workload;
    /* Every authored operation must be visited exactly once during catalogue synchronisation. */
    for (size_t i = 0; ok && i < count; ++i) {
        const benchmark_operation_t *op = operations + i;
        string_t *arguments = string_new();
        for (unsigned j = 0; arguments && j < op->arity; ++j) {
            if (j) string_append_cstr(arguments, ", ");
            string_append_cstr(arguments, op->arguments[j]);
        }
        ok = arguments && sqlite_stmt_bind_text(insert, 1, op->name) &&
             sqlite_stmt_bind_int(insert, 2, (int)op->arity) &&
             sqlite_stmt_bind_text(insert, 3, string_c_str(arguments)) &&
             sqlite_stmt_bind_int(insert, 4, (int)op->complex_mask) &&
             sqlite_stmt_bind_int(insert, 5, benchmark_operation_supported(op, false)) &&
             sqlite_stmt_bind_int(insert, 6, benchmark_operation_supported(op, true)) &&
             sqlite_stmt_step(insert) == SQLITE_STEP_DONE;
        sqlite_stmt_reset(insert);
        if (ok && op->integer_only) {
            ok = sqlite_stmt_bind_text(workload, 1, op->name) &&
                 sqlite_stmt_bind_text(workload, 2, string_c_str(arguments)) &&
                 sqlite_stmt_step(workload) == SQLITE_STEP_DONE;
            sqlite_stmt_reset(workload);
        }
        string_free(arguments);
    }
    sqlite_stmt_finalize(insert);
    sqlite_stmt_finalize(workload);
    path = checks_path(root, "bench/number/bench_number.sql");
    source = ok ? checks_read(path) : NULL;
    ok = ok && source && sqlite_exec(db, source);
    string_free(source);
    string_free(path);
    if (ok) ok = sqlite_exec_cstr(db, "COMMIT");
    if (!ok) (void)sqlite_exec_cstr(db, "ROLLBACK");
    return ok;
}

/* Storage is opened only by explicit benchmark commands, and reporting never creates it. */
sqlite_t *benchmark_database_open(const string_t *root, bool create)
{
    string_t *path = checks_path(root, "bench/benchmarks.db");
    sqlite_t *db = create || checks_is_file(path) ? sqlite_open_plain(path) : NULL;
    string_free(path);
    if (!db) return NULL;
    bool ok = create ? benchmark_store_schema(db, root) : true;
    if (!ok) {
        const string_t *error = sqlite_last_error(db);
        fprintf(stderr, "Cannot initialise benchmark storage: %s\n", error ? string_c_str(error) : "invalid schema");
        sqlite_close(db);
        db = NULL;
    }
    return db;
}
