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

static bool benchmark_store_display_column(sqlite_t *db, const char *table)
{
    sqlite_stmt_t *columns = sqlite_stmt_prepare(db,
        "SELECT count(*), coalesce(sum(name = 'arguments_display'), 0)\n"
        "FROM pragma_table_info(?1)");
    bool ok = columns && sqlite_stmt_bind_text(columns, 1, table) && sqlite_stmt_step(columns) == SQLITE_STEP_ROW;
    bool missing = ok && sqlite_stmt_column_int(columns, 0) && !sqlite_stmt_column_int(columns, 1);
    sqlite_stmt_finalize(columns);
    /* Both identifiers are fixed module-owned table names, never user-provided text. */
    string_t *alter = missing ? string_sprintf("ALTER TABLE %s ADD COLUMN arguments_display TEXT", table) : NULL;
    if (missing) ok = alter && sqlite_exec(db, alter);
    string_free(alter);
    return ok;
}

/* Rename catalogue identities and their retained timings together; never merge distinct historical observations. */
static bool benchmark_store_operation_names(sqlite_t *db)
{
    static const char *const names[][2] = {
        {"chebyshev_t", "chebyshev_Tn"},
        {"chebyshev_u", "chebyshev_Un"},
        {"hermite_h", "hermite_Hn"},
        {"appell_f1", "appell_F1"},
        {"bessel_i", "bessel_I"},
        {"bessel_j", "bessel_J"},
        {"bessel_k", "bessel_K"},
        {"bessel_y", "bessel_Y"},
        {"lambert_w0", "lambert_W0"},
        {"lambert_wm1", "lambert_Wm1"},
        {"lambert_wn", "lambert_Wn"},
        {"lauricella_f", "lauricella_F"},
        {"struve_h", "struve_H"},
        {"struve_l", "struve_L"},
        {"lommel_s", "lommel_S"}
    };
    static const char *const tables[][2] = {
        {"benchmark_operation", "name"},
        {"benchmark_result", "operation"},
        {"benchmark_sample", "operation"},
        {"benchmark_workload", "operation"}
    };
    sqlite_stmt_t *exists = sqlite_stmt_prepare(db, "SELECT EXISTS(SELECT 1 FROM benchmark_operation WHERE name = ?1)");
    sqlite_stmt_t *updates[4] = {0};
    bool ok = exists && sqlite_exec_cstr(db, "PRAGMA defer_foreign_keys = ON");
    /* The fixed module-owned table and rename lists bound this one-off catalogue migration. */
    for (size_t i = 0; ok && i < 4; ++i) {
        string_t *sql = string_sprintf("UPDATE %s\nSET %s = ?2\nWHERE %s = ?1", tables[i][0], tables[i][1], tables[i][1]);
        updates[i] = sql ? sqlite_stmt_prepare(db, string_c_str(sql)) : NULL;
        string_free(sql);
        ok = updates[i] != NULL;
    }
    for (size_t i = 0; ok && i < sizeof names / sizeof *names; ++i) {
        ok = sqlite_stmt_bind_text(exists, 1, names[i][1]) && sqlite_stmt_step(exists) == SQLITE_STEP_ROW;
        bool separate_history = ok && sqlite_stmt_column_int(exists, 0);
        sqlite_stmt_reset(exists);
        if (separate_history) continue;
        for (size_t j = 0; ok && j < 4; ++j) {
            ok = sqlite_stmt_bind_text(updates[j], 1, names[i][0]) &&
                 sqlite_stmt_bind_text(updates[j], 2, names[i][1]) && sqlite_stmt_step(updates[j]) == SQLITE_STEP_DONE;
            sqlite_stmt_reset(updates[j]);
        }
    }
    sqlite_stmt_finalize(exists);
    for (size_t i = 0; i < 4; ++i) sqlite_stmt_finalize(updates[i]);
    return ok;
}

/* Materialise display text separately from the literal operands used by timing and comparison. */
bool benchmark_store_displays(sqlite_t *db)
{
    /* UNION materialises distinct groups before updates, so changing the missing-display index cannot skip rows. */
    sqlite_stmt_t *groups = sqlite_stmt_prepare(db,
        "SELECT arguments, backend, complex_mask\n"
        "FROM benchmark_result\n"
        "WHERE arguments_display IS NULL\n"
        "UNION\n"
        "SELECT arguments, backend, 0\n"
        "FROM benchmark_workload\n"
        "WHERE arguments_display IS NULL");
    sqlite_stmt_t *results = sqlite_stmt_prepare(db,
        "UPDATE benchmark_result\n"
        "SET arguments_display = ?1\n"
        "WHERE arguments = ?2 AND backend = ?3 AND complex_mask = ?4 AND arguments_display IS NULL");
    sqlite_stmt_t *workloads = sqlite_stmt_prepare(db,
        "UPDATE benchmark_workload\n"
        "SET arguments_display = ?1\n"
        "WHERE arguments = ?2 AND backend = ?3 AND arguments_display IS NULL");
    bool ok = groups && results && workloads;
    sqlite_step_result_t step = SQLITE_STEP_ERROR;
    while (ok && (step = sqlite_stmt_step(groups)) == SQLITE_STEP_ROW) {
        const char *arguments = sqlite_stmt_column_text(groups, 0), *backend = sqlite_stmt_column_text(groups, 1);
        unsigned mask = (unsigned)sqlite_stmt_column_int(groups, 2);
        string_t *display = benchmark_arguments_display(arguments, backend, mask);
        ok = display && sqlite_stmt_bind_text(results, 1, string_c_str(display)) &&
            sqlite_stmt_bind_text(results, 2, arguments) && sqlite_stmt_bind_text(results, 3, backend) &&
            sqlite_stmt_bind_int(results, 4, (int)mask) && sqlite_stmt_step(results) == SQLITE_STEP_DONE &&
            sqlite_stmt_bind_text(workloads, 1, string_c_str(display)) &&
            sqlite_stmt_bind_text(workloads, 2, arguments) && sqlite_stmt_bind_text(workloads, 3, backend) &&
            sqlite_stmt_step(workloads) == SQLITE_STEP_DONE;
        string_free(display);
        sqlite_stmt_reset(results); sqlite_stmt_reset(workloads);
    }
    sqlite_stmt_finalize(groups); sqlite_stmt_finalize(results); sqlite_stmt_finalize(workloads);
    return ok && step == SQLITE_STEP_DONE;
}

static bool benchmark_store_schema(sqlite_t *db, const string_t *root)
{
    sqlite_stmt_t *version = sqlite_stmt_prepare(db, "PRAGMA application_id");
    bool ok = version && sqlite_stmt_step(version) == SQLITE_STEP_ROW;
    int id = ok ? sqlite_stmt_column_int(version, 0) : -1;
    sqlite_stmt_finalize(version);
    if (!ok || (id && id != 1296126546)) return benchmark_database_error(db, "checking benchmark database identity");
    version = sqlite_stmt_prepare(db, "PRAGMA user_version");
    ok = version && sqlite_stmt_step(version) == SQLITE_STEP_ROW && sqlite_stmt_column_int(version, 0) <= 4;
    sqlite_stmt_finalize(version);
    if (!ok) return benchmark_database_error(db, "checking benchmark schema version");
    string_t *path = checks_path(root, "bench/benchmarks.sql"), *source = checks_read(path);
    ok = source && sqlite_exec_cstr(db, "BEGIN IMMEDIATE") &&
        benchmark_store_display_column(db, "benchmark_result") &&
        benchmark_store_display_column(db, "benchmark_workload") && sqlite_exec(db, source) &&
        benchmark_store_operation_names(db);
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
        "ON CONFLICT(operation, backend) DO UPDATE SET\n"
        "arguments_display = CASE WHEN benchmark_workload.arguments = excluded.arguments\n"
        "                         THEN benchmark_workload.arguments_display END,\n"
        "arguments = excluded.arguments") : NULL;
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
    if (ok) ok = benchmark_store_displays(db) && sqlite_exec_cstr(db, "COMMIT");
    if (!ok) {
        benchmark_database_error(db, "initialising benchmark catalogue");
        (void)sqlite_exec_cstr(db, "ROLLBACK");
    }
    return ok;
}

/* Preserve the useful SQLite explanation before transactional cleanup changes the connection diagnostic. */
bool benchmark_database_error(sqlite_t *db, const char *operation)
{
    const string_t *error = sqlite_last_error(db);
    fprintf(stderr, "Benchmark database failure while %s: %s.\n", operation,
            error && string_length(error) ? string_c_str(error) : "invalid storage state or allocation failure");
    return false;
}

/* Storage is opened only by explicit benchmark commands, and reporting never creates it. */
sqlite_t *benchmark_database_open(const string_t *root, bool create)
{
    string_t *path = checks_path(root, "bench/benchmarks.db");
    sqlite_t *db = create || checks_is_file(path) ? sqlite_open_plain(path) : NULL;
    string_free(path);
    if (!db) return NULL;
    bool ok = sqlite_exec_cstr(db, "PRAGMA busy_timeout = 5000");
    if (!ok) benchmark_database_error(db, "configuring benchmark lock waiting");
    if (ok && create) ok = benchmark_store_schema(db, root);
    if (!ok) {
        sqlite_close(db);
        db = NULL;
    }
    return db;
}
