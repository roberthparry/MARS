/**
 * @file benchmarks_retention.c
 * @brief Bounded benchmark history, persisted policy and transactional expiry.
 *
 * Keeps recent raw samples and a larger bounded set of run summaries. Age and count expiry remove complete runs;
 * storage pressure discards older samples before older summaries. The active run is protected. SQLite's page cap
 * stops an oversized run safely, and compaction returns reclaimed space to the filesystem. Only explicit benchmark
 * maintenance and measurement commands mutate storage; browsing and ordinary builds never trigger cleanup.
 */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file-t.h"
#include "checks_support.h"
#define MARS_BENCHMARKS_INTERNAL_ACCESS
#include "benchmarks_internal.h"

typedef struct {
    int max_mib;
    int keep_runs;
    int max_days;
    int raw_runs;
} benchmark_retention_policy_t;

static int64_t benchmark_retention_integer(sqlite_t *db, const char *sql)
{
    sqlite_stmt_t *query = sqlite_stmt_prepare(db, sql);
    int64_t result = query && sqlite_stmt_step(query) == SQLITE_STEP_ROW ? sqlite_stmt_column_int64(query, 0) : -1;
    sqlite_stmt_finalize(query);
    return result;
}

static bool benchmark_retention_policy(sqlite_t *db, benchmark_retention_policy_t *policy)
{
    sqlite_stmt_t *read = sqlite_stmt_prepare(db,
        "SELECT max_mib, keep_runs, max_days, raw_runs\n"
        "FROM benchmark_retention\n"
        "WHERE singleton = 1");
    bool ok = read && sqlite_stmt_step(read) == SQLITE_STEP_ROW;
    if (ok) *policy = (benchmark_retention_policy_t){
        sqlite_stmt_column_int(read, 0), sqlite_stmt_column_int(read, 1),
        sqlite_stmt_column_int(read, 2), sqlite_stmt_column_int(read, 3)
    };
    sqlite_stmt_finalize(read);
    return ok;
}

static bool benchmark_retention_write(sqlite_t *db, const benchmark_retention_policy_t *policy)
{
    sqlite_stmt_t *write = sqlite_stmt_prepare(db,
        "UPDATE benchmark_retention\n"
        "SET max_mib = ?1, keep_runs = ?2, max_days = ?3, raw_runs = ?4\n"
        "WHERE singleton = 1");
    bool ok = write && sqlite_stmt_bind_int(write, 1, policy->max_mib) &&
        sqlite_stmt_bind_int(write, 2, policy->keep_runs) && sqlite_stmt_bind_int(write, 3, policy->max_days) &&
        sqlite_stmt_bind_int(write, 4, policy->raw_runs) && sqlite_stmt_step(write) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(write);
    return ok;
}

static bool benchmark_retention_expire(sqlite_t *db, long protected_run, const benchmark_retention_policy_t *policy)
{
    bool ok = sqlite_exec_cstr(db,
        "CREATE TEMP TABLE benchmark_expired_run (id INTEGER PRIMARY KEY)");
    sqlite_stmt_t *choose = ok ? sqlite_stmt_prepare(db,
        "INSERT INTO benchmark_expired_run\n"
        "SELECT id FROM (\n"
        "    SELECT id, coalesce(finished_utc, started_utc) AS stamp,\n"
        "           row_number() OVER (ORDER BY (id = ?1) DESC, id DESC) AS recent\n"
        "    FROM benchmark_run\n"
        ")\n"
        "WHERE id <> ?1 AND (recent > ?2 OR julianday(stamp) < julianday('now') - ?3)") : NULL;
    ok = choose && sqlite_stmt_bind_int64(choose, 1, protected_run) &&
        sqlite_stmt_bind_int(choose, 2, policy->keep_runs) && sqlite_stmt_bind_int(choose, 3, policy->max_days) &&
        sqlite_stmt_step(choose) == SQLITE_STEP_DONE && ok;
    sqlite_stmt_finalize(choose);
    ok = ok && sqlite_exec_cstr(db,
        "DELETE FROM benchmark_sample WHERE run_id IN (SELECT id FROM benchmark_expired_run);\n"
        "DELETE FROM benchmark_result WHERE run_id IN (SELECT id FROM benchmark_expired_run);\n"
        "DELETE FROM benchmark_run WHERE id IN (SELECT id FROM benchmark_expired_run);\n"
        "DROP TABLE benchmark_expired_run;");
    sqlite_stmt_t *samples = ok ? sqlite_stmt_prepare(db,
        "DELETE FROM benchmark_sample\n"
        "WHERE run_id <> ?1 AND run_id NOT IN (\n"
        "    SELECT id FROM benchmark_run ORDER BY (id = ?1) DESC, id DESC LIMIT ?2\n"
        ")") : NULL;
    ok = samples && sqlite_stmt_bind_int64(samples, 1, protected_run) &&
        sqlite_stmt_bind_int(samples, 2, policy->raw_runs) && sqlite_stmt_step(samples) == SQLITE_STEP_DONE && ok;
    sqlite_stmt_finalize(samples);
    return ok;
}

static bool benchmark_retention_remove_run(sqlite_t *db, long run)
{
    static const char *const statements[] = {
        "DELETE FROM benchmark_sample WHERE run_id = ?1",
        "DELETE FROM benchmark_result WHERE run_id = ?1",
        "DELETE FROM benchmark_run WHERE id = ?1"
    };
    /* Foreign keys require the three explicitly ordered levels to be removed together. */
    bool ok = true;
    for (size_t i = 0; ok && i < sizeof statements / sizeof *statements; ++i) {
        sqlite_stmt_t *write = sqlite_stmt_prepare(db, statements[i]);
        ok = write && sqlite_stmt_bind_int64(write, 1, run) && sqlite_stmt_step(write) == SQLITE_STEP_DONE;
        sqlite_stmt_finalize(write);
    }
    return ok;
}

static int64_t benchmark_retention_used_pages(sqlite_t *db)
{
    return benchmark_retention_integer(db,
        "SELECT page_count - freelist_count FROM pragma_page_count(), pragma_freelist_count()");
}

static bool benchmark_retention_fit(sqlite_t *db, long protected_run, int64_t budget, int64_t cap)
{
    int64_t used = benchmark_retention_used_pages(db);
    if (used < 0) return false;
    while (used > budget) {
        sqlite_stmt_t *oldest = sqlite_stmt_prepare(db, "SELECT min(run_id) FROM benchmark_sample WHERE run_id <> ?1");
        bool ok = oldest && sqlite_stmt_bind_int64(oldest, 1, protected_run) &&
            sqlite_stmt_step(oldest) == SQLITE_STEP_ROW;
        long run = ok ? (long)sqlite_stmt_column_int64(oldest, 0) : -1;
        sqlite_stmt_finalize(oldest);
        if (!ok) return false;
        if (!run) break;
        sqlite_stmt_t *samples = sqlite_stmt_prepare(db, "DELETE FROM benchmark_sample WHERE run_id = ?1");
        ok = samples && sqlite_stmt_bind_int64(samples, 1, run) && sqlite_stmt_step(samples) == SQLITE_STEP_DONE;
        sqlite_stmt_finalize(samples);
        if (!ok) return false;
        used = benchmark_retention_used_pages(db);
    }
    /* Remove whole oldest runs only under storage pressure; each step makes indexed, bounded progress. */
    while (used > budget) {
        sqlite_stmt_t *oldest = sqlite_stmt_prepare(db, "SELECT min(id) FROM benchmark_run WHERE id <> ?1");
        bool ok = oldest && sqlite_stmt_bind_int64(oldest, 1, protected_run) &&
            sqlite_stmt_step(oldest) == SQLITE_STEP_ROW;
        long run = ok ? (long)sqlite_stmt_column_int64(oldest, 0) : -1;
        sqlite_stmt_finalize(oldest);
        if (!ok) return false;
        if (!run) break;
        if (!benchmark_retention_remove_run(db, run)) return false;
        used = benchmark_retention_used_pages(db);
    }
    return used >= 0 && used <= cap;
}

static bool benchmark_retention_apply_policy(sqlite_t *db, long protected_run, bool headroom,
                                             const benchmark_retention_policy_t *replacement)
{
    benchmark_retention_policy_t policy = {0};
    if (replacement) policy = *replacement;
    else if (!benchmark_retention_policy(db, &policy)) return false;
    int64_t page_size = benchmark_retention_integer(db, "PRAGMA page_size");
    int64_t cap = page_size > 0 ? (int64_t)policy.max_mib * 1048576 / page_size : 0;
    int64_t runs = benchmark_retention_integer(db, "SELECT count(*) FROM benchmark_run");
    int64_t samples = benchmark_retention_integer(db, "SELECT count(*) FROM benchmark_sample");
    bool ok = cap > 0 && runs >= 0 && samples >= 0 && sqlite_exec_cstr(db, "BEGIN IMMEDIATE");
    if (ok && replacement) ok = benchmark_retention_write(db, &policy);
    ok = ok && sqlite_exec_cstr(db,
        "UPDATE benchmark_retention\n"
        "SET last_run_id = max(last_run_id, (SELECT coalesce(max(id), 0) FROM benchmark_run))\n"
        "WHERE singleton = 1") && benchmark_retention_expire(db, protected_run, &policy) &&
        benchmark_retention_fit(db, protected_run, headroom ? cap * 3 / 4 : cap, cap);
    if (ok) ok = sqlite_exec_cstr(db, "COMMIT");
    if (!ok) { (void)sqlite_exec_cstr(db, "ROLLBACK"); return false; }
    int64_t remaining_runs = benchmark_retention_integer(db, "SELECT count(*) FROM benchmark_run");
    int64_t remaining_samples = benchmark_retention_integer(db, "SELECT count(*) FROM benchmark_sample");
    bool removed = remaining_runs < runs || remaining_samples < samples;
    if (removed || benchmark_retention_integer(db, "PRAGMA page_count") > cap)
        ok = sqlite_exec_cstr(db, "VACUUM");
    string_t *limit = string_sprintf("PRAGMA max_page_count = %lld", (long long)cap);
    int64_t installed = ok && limit ? benchmark_retention_integer(db, string_c_str(limit)) : -1;
    string_free(limit);
    ok = ok && installed > 0 && installed <= cap && remaining_runs >= 0 && remaining_samples >= 0;
    if (ok && removed)
        fprintf(stderr, "Benchmark retention: removed %lld old runs and %lld raw samples; "
                "retained summaries and outlier flags are unchanged.\n",
                (long long)(runs - remaining_runs), (long long)(samples - remaining_samples));
    return ok;
}

/* Enforce the persisted policy without changing any retained measurement or outlier flag. */
bool benchmark_retention_apply(sqlite_t *db, long protected_run, bool headroom)
{
    bool ok = benchmark_retention_apply_policy(db, protected_run, headroom, NULL);
    if (!ok) fputs("Cannot enforce benchmark retention or its storage cap; saved checkpoints remain available.\n", stderr);
    return ok;
}

/* Reclaim history near capacity outside the worker's numerical timing intervals. */
bool benchmark_retention_checkpoint(sqlite_t *db, long protected_run)
{
    benchmark_retention_policy_t policy = {0};
    if (!benchmark_retention_policy(db, &policy)) return false;
    int64_t page_size = benchmark_retention_integer(db, "PRAGMA page_size");
    int64_t used = benchmark_retention_used_pages(db);
    int64_t cap = page_size > 0 ? (int64_t)policy.max_mib * 1048576 / page_size : 0;
    return used >= 0 && cap > 0 && (used + 64 < cap || benchmark_retention_apply(db, protected_run, true));
}

static bool benchmark_retention_options(benchmark_retention_policy_t *policy, int argc, char **argv)
{
    bool raw_explicit = false;
    for (int i = 0; i < argc; i += 2) {
        if (i + 1 == argc || !argv[i + 1][0]) return false;
        char *end = NULL;
        errno = 0;
        long n = strtol(argv[i + 1], &end, 10);
        if (errno || !end || *end || n < 0) return false;
        if (!strcmp(argv[i], "--max-mib") && n >= 1 && n <= 4096) policy->max_mib = (int)n;
        else if (!strcmp(argv[i], "--keep-runs") && n >= 1 && n <= 1000) policy->keep_runs = (int)n;
        else if (!strcmp(argv[i], "--max-days") && n >= 1 && n <= 3650) policy->max_days = (int)n;
        else if (!strcmp(argv[i], "--raw-runs") && n <= 1000) { policy->raw_runs = (int)n; raw_explicit = true; }
        else return false;
    }
    if (policy->raw_runs > policy->keep_runs) {
        if (raw_explicit) return false;
        policy->raw_runs = policy->keep_runs;
    }
    return true;
}

/* Show policy without writes, or lock storage before atomically configuring and pruning it. */
int benchmark_retention_command(const string_t *root, sqlite_t *db, int argc, char **argv, bool prune)
{
    benchmark_retention_policy_t policy = {0};
    if (!benchmark_retention_policy(db, &policy)) return 1;
    if ((prune && argc) || !benchmark_retention_options(&policy, argc, argv)) return 2;
    if (argc || prune) {
        string_t *path = checks_path(root, "bench/benchmarks.db");
        file_t *lock = file_new(path);
        string_free(path);
        bool ok = lock && file_open_read(lock) && file_lock(lock, true, false);
        if (!ok) fputs("Another benchmark process holds the run lock; retention was not changed.\n", stderr);
        else ok = benchmark_retention_apply_policy(db, 0, false, argc ? &policy : NULL);
        file_free(lock);
        if (!ok) {
            fputs("Cannot apply benchmark retention; check the limits and database availability.\n", stderr);
            return 1;
        }
    }
    if (!prune)
        printf("Benchmark retention\n  Database cap: %d MiB\n  Recent runs: %d\n  Maximum age: %d days\n"
               "  Raw samples: newest %d runs\n", policy.max_mib, policy.keep_runs, policy.max_days, policy.raw_runs);
    return ferror(stdout) ? 1 : 0;
}
