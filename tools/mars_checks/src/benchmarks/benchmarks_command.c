/**
 * @file benchmarks_command.c
 * @brief Explicit benchmark catalogue, run and report command dispatch.
 *
 * Read-only commands never initialise storage. Measurements and Markdown refreshes are separate operations so
 * opening a report cannot launch expensive work or silently change a document. All command arguments are literal.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "checks_benchmarks.h"
#define MARS_BENCHMARKS_INTERNAL_ACCESS
#include "benchmarks_internal.h"

static int benchmark_status(sqlite_t *db)
{
    sqlite_stmt_t *rows = sqlite_stmt_prepare(db,
        "SELECT b.id, b.started_utc, b.status, count(r.operation), "
        "sum(r.outcome = 'pass'), sum(r.outcome = 'unsupported'), sum(r.outcome = 'pending'), "
        "sum(r.outcome IN ('fail', 'timeout')) "
        "FROM benchmark_run AS b LEFT JOIN benchmark_result AS r ON r.run_id = b.id "
        "GROUP BY b.id ORDER BY b.id DESC LIMIT 20");
    bool ok = rows != NULL;
    puts("Run  Started (UTC)             State        Cases  Passed  Unsupported  Pending  Failed");
    sqlite_step_result_t step = SQLITE_STEP_DONE;
    while (ok && (step = sqlite_stmt_step(rows)) == SQLITE_STEP_ROW) {
        printf("%-4lld %-25s %-12s %5d %7d %12d %8d %7d\n",
               (long long)sqlite_stmt_column_int64(rows, 0), sqlite_stmt_column_text(rows, 1),
               sqlite_stmt_column_text(rows, 2), sqlite_stmt_column_int(rows, 3), sqlite_stmt_column_int(rows, 4),
               sqlite_stmt_column_int(rows, 5), sqlite_stmt_column_int(rows, 6), sqlite_stmt_column_int(rows, 7));
    }
    sqlite_stmt_finalize(rows);
    return ok && step == SQLITE_STEP_DONE && !ferror(stdout) ? 0 : 1;
}

/* Keep benchmark storage and execution opt-in and independent of ordinary builds. */
int checks_benchmarks(const string_t *root, int argc, char **argv)
{
    const char *command = argc ? argv[0] : "status";
    if (!strcmp(command, "help")) {
        puts("bench-db sync             Merge definitions and apply stored history limits.\n"
             "bench-db status           List recent runs without creating storage.\n"
             "bench-db stats [OPTIONS]  Browse a saved run; defaults to the latest, including pending cases.\n"
             "  --run RUN                    Select an earlier run.\n"
             "  --backend NAME | --bits N | --operation NAME  Restrict displayed cases.\n"
             "  --summary                    Compact timing tables with arguments (default).\n"
             "  --details                    Add timestamps, calibration and failure explanations.\n"
             "  --include-skipped            Include unsupported cases; hidden by default in every format.\n"
             "  --format=grid|markdown|tsv    Unicode grid by default; exports contain no terminal escapes.\n"
             "  Example: make bench-db ARGS='stats --operation zeta --backend mpc --bits 4096' | less -R\n"
             "  Pipe the grid to less -R to retain colours; NO_COLOR disables them.\n"
             "bench-db run [OPTIONS]    Measure every selected operation and precision sequentially.\n"
             "  --backend NAME               number-double, qfloat, qcomplex, mpfr, mpc or exact-integer\n"
             "  --bits N                     Restrict the configured precision profiles.\n"
             "  --operation NAME             Select one canonical mathematical operation.\n"
             "  --samples N                  Timed batches per case, 3..101 (default 7).\n"
             "  --timeout SECONDS            Hard per-case deadline, 1..300 (default 15).\n"
             "  --resume RUN                 Continue pending cases of an interrupted run.\n"
             "  --verbose                    List every case instead of compact live progress.\n"
             "bench-db report [RUN]     Print Markdown from one complete run.\n"
             "bench-db update-docs [RUN] Refresh marked tables from that same stored run.\n"
             "bench-db retention        Show the stored history and storage limits without changing them.\n"
             "  --max-mib N                  Database cap, 1..4096 MiB (default 32).\n"
             "  --keep-runs N                Retain 1..1000 recent runs (default 20).\n"
             "  --max-days N                 Expire runs older than 1..3650 days (default 90).\n"
             "  --raw-runs N                 Raw samples for 0..keep-runs recent runs (default 5).\n"
             "  Supplying policy options saves them and prunes history; summaries keep their outlier flags.\n"
             "bench-db prune            Apply stored limits and reclaim disk space without measurements.\n"
             "Storage: bench/benchmarks.db. Builds and make clean leave it alone.");
        return 0;
    }
    if (!strcmp(command, "measure")) return benchmark_measure(root, argc - 1, argv + 1);
    bool sync = !strcmp(command, "sync"), run = !strcmp(command, "run");
    bool status = !strcmp(command, "status"), report = !strcmp(command, "report");
    bool update = !strcmp(command, "update-docs"), stats = !strcmp(command, "stats");
    bool retention = !strcmp(command, "retention"), prune = !strcmp(command, "prune");
    if (!sync && !run && !status && !report && !update && !stats && !retention && !prune) return 2;
    if ((sync || status || prune) && argc > 1) return 2;
    long id = 0;
    if (report || update) {
        if (argc > 2) return 2;
        if (argc == 2) {
            char *end = NULL;
            id = strtol(argv[1], &end, 10);
            if (!end || *end || id <= 0) return 2;
        }
    }
    sqlite_t *db = benchmark_database_open(root, sync || run || prune || (retention && argc > 1));
    if (!db) {
        fputs("No readable benchmark database; run make bench-db ARGS='sync' to create it.\n", stderr);
        return 1;
    }
    int result;
    if (sync) {
        result = benchmark_retention_command(root, db, 0, NULL, true);
        if (!result) puts("Benchmark catalogue synchronised; stored history limits applied.");
    }
    else if (run) result = benchmark_run(root, db, argc - 1, argv + 1);
    else if (retention || prune) result = benchmark_retention_command(root, db, argc - 1, argv + 1, prune);
    else if (status) result = benchmark_status(db);
    else if (stats) result = benchmark_stats(db, argc - 1, argv + 1);
    else result = benchmark_report(root, db, id, update);
    sqlite_close(db);
    return result;
}
