/**
 * @file benchmarks_run.c
 * @brief Sequential benchmark execution, provenance and durable result checkpoints.
 *
 * One mathematical case runs in each bounded subprocess. Every outcome and its raw samples commit together;
 * interruption leaves earlier cases intact and pending cases resumable. Timing comparison uses only compatible
 * machine/build/workload histories and never adopts a flagged slowdown as a baseline.
 */
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>

#include "file-t.h"
#include "checks_process.h"
#include "checks_support.h"
#define MARS_BENCHMARKS_INTERNAL_ACCESS
#include "benchmarks_internal.h"

#ifndef MARS_BENCHMARK_BUILD_FLAGS
#define MARS_BENCHMARK_BUILD_FLAGS "unspecified"
#endif
#ifndef MARS_BENCHMARK_LIBRARY
#define MARS_BENCHMARK_LIBRARY "build/release/libmars.a"
#endif

typedef struct {
    const char *backend;
    const char *operation;
    unsigned bits;
    unsigned samples;
    unsigned timeout;
    long resume;
    bool verbose;
} benchmark_options_t;

static bool benchmark_options(int argc, char **argv, benchmark_options_t *options)
{
    *options = (benchmark_options_t){.backend = "", .operation = "", .samples = 7, .timeout = 15};
    for (int i = 0; i < argc; ++i) {
        if (!strcmp(argv[i], "--verbose")) { options->verbose = true; continue; }
        if (i + 1 >= argc) return false;
        const char *key = argv[i], *value = argv[++i];
        if (!strcmp(key, "--backend")) options->backend = value;
        else if (!strcmp(key, "--operation")) options->operation = value;
        else {
            char *end = NULL;
            errno = 0;
            long n = strtol(value, &end, 10);
            if (errno || !end || *end || n <= 0) return false;
            if (!strcmp(key, "--bits") && n <= 16384) options->bits = (unsigned)n;
            else if (!strcmp(key, "--samples") && n >= 3 && n <= 101) options->samples = (unsigned)n;
            else if (!strcmp(key, "--timeout") && n <= 300) options->timeout = (unsigned)n;
            else if (!strcmp(key, "--resume")) options->resume = n;
            else return false;
        }
    }
    return true;
}

static string_t *benchmark_digest(const string_t *path)
{
    file_t *requested = file_new(path), *resolved = requested ? file_resolve(requested) : NULL;
    string_t *digest = resolved ? file_sha256(resolved) : NULL;
    file_free(resolved);
    file_free(requested);
    return digest;
}

static string_t *benchmark_cpu(void)
{
    string_t *path = string_new_with("/proc/cpuinfo"), *text = checks_read(path);
    string_free(path);
    const char *start = text ? strstr(string_c_str(text), "model name") : NULL;
    const char *colon = start ? strchr(start, ':') : NULL;
    const char *end = colon ? strchr(colon, '\n') : NULL;
    string_t *result = end ? checks_slice(text, (size_t)(colon + 2 - string_c_str(text)),
                                         (size_t)(end - colon - 2)) : string_new_with("unavailable");
    string_free(text);
    return result;
}

static long benchmark_start(sqlite_t *db, const benchmark_options_t *options,
                            const string_t *library, const string_t *executable)
{
    if (options->resume) {
        sqlite_stmt_t *read = sqlite_stmt_prepare(db,
            "SELECT requested_samples, case_timeout_seconds FROM benchmark_run "
            "WHERE id = ?1 AND status IN ('running', 'interrupted') "
            "AND library_sha256 = ?2 AND executable_sha256 = ?3");
        bool ok = read && sqlite_stmt_bind_int64(read, 1, options->resume) &&
            sqlite_stmt_bind_text(read, 2, string_c_str(library)) &&
            sqlite_stmt_bind_text(read, 3, string_c_str(executable)) && sqlite_stmt_step(read) == SQLITE_STEP_ROW;
        sqlite_stmt_finalize(read);
        read = ok ? sqlite_stmt_prepare(db,
            "UPDATE benchmark_run SET status = 'running', finished_utc = NULL WHERE id = ?1") : NULL;
        ok = read && sqlite_stmt_bind_int64(read, 1, options->resume) &&
             sqlite_stmt_step(read) == SQLITE_STEP_DONE && ok;
        sqlite_stmt_finalize(read);
        return ok ? options->resume : 0;
    }
    struct utsname machine;
    if (uname(&machine)) return 0;
    string_t *cpu = benchmark_cpu(), *system = string_sprintf("%s %s %s", machine.sysname,
                                                           machine.release, machine.machine);
    sqlite_stmt_t *insert = sqlite_stmt_prepare(db,
        "INSERT INTO benchmark_run(id, status, machine, cpu, system, compiler, build_flags, library_sha256, "
        "executable_sha256, requested_samples, case_timeout_seconds, filter, clock) "
        "VALUES ((SELECT max(last_run_id, (SELECT coalesce(max(id), 0) FROM benchmark_run)) + 1\n"
        "         FROM benchmark_retention WHERE singleton = 1),\n"
        "        'running', ?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11)");
    bool ok = cpu && system && insert && sqlite_exec_cstr(db, "BEGIN IMMEDIATE") &&
        sqlite_stmt_bind_text(insert, 1, machine.nodename) && sqlite_stmt_bind_text(insert, 2, string_c_str(cpu)) &&
        sqlite_stmt_bind_text(insert, 3, string_c_str(system)) && sqlite_stmt_bind_text(insert, 4, __VERSION__) &&
        sqlite_stmt_bind_text(insert, 5, MARS_BENCHMARK_BUILD_FLAGS) &&
        sqlite_stmt_bind_text(insert, 6, string_c_str(library)) &&
        sqlite_stmt_bind_text(insert, 7, string_c_str(executable)) &&
        sqlite_stmt_bind_int(insert, 8, (int)options->samples) &&
        sqlite_stmt_bind_int(insert, 9, (int)options->timeout) &&
        sqlite_stmt_bind_text(insert, 10, options->operation) &&
        sqlite_stmt_bind_text(insert, 11, benchmark_protocol) && sqlite_stmt_step(insert) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(insert);
    string_free(cpu);
    string_free(system);
    sqlite_stmt_t *id = ok ? sqlite_stmt_prepare(db, "SELECT last_insert_rowid()") : NULL;
    long run = id && sqlite_stmt_step(id) == SQLITE_STEP_ROW ? (long)sqlite_stmt_column_int64(id, 0) : 0;
    sqlite_stmt_finalize(id);
    insert = run ? sqlite_stmt_prepare(db,
        "INSERT INTO benchmark_result(run_id, operation, backend, bits, arguments, complex_mask, sequence) "
        "SELECT ?1, o.name, p.backend, p.bits, coalesce(w.arguments, o.arguments),\n"
        "       CASE WHEN w.operation IS NULL THEN o.complex_mask ELSE 0 END,\n"
        "row_number() OVER (ORDER BY CASE p.backend WHEN 'number-double' THEN 0 WHEN 'qfloat' THEN 1 "
        "WHEN 'qcomplex' THEN 2 WHEN 'mpfr' THEN 3 ELSE 4 END, p.bits, o.name) "
        "FROM benchmark_operation AS o CROSS JOIN benchmark_profile AS p\n"
        "LEFT JOIN benchmark_workload AS w ON w.operation = o.name AND w.backend = p.backend\n"
        "WHERE (?2 = '' OR o.name = ?2) AND (?3 = '' OR p.backend = ?3) AND (?4 = 0 OR p.bits = ?4)") : NULL;
    ok = ok && run && insert && sqlite_stmt_bind_int64(insert, 1, run) &&
        sqlite_stmt_bind_text(insert, 2, options->operation) && sqlite_stmt_bind_text(insert, 3, options->backend) &&
        sqlite_stmt_bind_int(insert, 4, (int)options->bits) && sqlite_stmt_step(insert) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(insert);
    id = ok ? sqlite_stmt_prepare(db, "SELECT changes()") : NULL;
    ok = id && sqlite_stmt_step(id) == SQLITE_STEP_ROW && sqlite_stmt_column_int(id, 0) > 0 && ok;
    sqlite_stmt_finalize(id);
    ok = ok && sqlite_exec_cstr(db,
        "UPDATE benchmark_retention\n"
        "SET last_run_id = max(last_run_id, (SELECT max(id) FROM benchmark_run))\n"
        "WHERE singleton = 1");
    if (ok) ok = benchmark_store_displays(db) && sqlite_exec_cstr(db, "COMMIT");
    if (!ok) { (void)sqlite_exec_cstr(db, "ROLLBACK"); return 0; }
    return run;
}

static int benchmark_double_compare(const void *a, const void *b)
{
    double left = *(const double *)a, right = *(const double *)b;
    return (left > right) - (left < right);
}

static double benchmark_median(double *samples, unsigned count)
{
    qsort(samples, count, sizeof *samples, benchmark_double_compare);
    return (samples[(count - 1) / 2] + samples[count / 2]) / 2;
}

static bool benchmark_bind_case(sqlite_stmt_t *statement, long run, const char *operation,
                                const char *backend, int bits)
{
    return statement && sqlite_stmt_bind_int64(statement, 1, run) &&
           sqlite_stmt_bind_text(statement, 2, operation) && sqlite_stmt_bind_text(statement, 3, backend) &&
           sqlite_stmt_bind_int(statement, 4, bits);
}

static bool benchmark_save(sqlite_t *db, long run, const char *operation, const char *backend, int bits,
                            const char *outcome, const string_t *output, const string_t *errors, unsigned expected)
{
    double samples[101], sorted[101], deviations[101], mean = 0, median = 0, mad = 0;
    unsigned count = 0, iterations = 0;
    unsigned long long resolution = 0;
    const char *next = output ? string_c_str(output) : "";
    bool measured = !strcmp(outcome, "pass");
    if (measured) {
        char *end = NULL;
        iterations = (unsigned)strtoul(next, &end, 10);
        next = end;
        resolution = strtoull(next, &end, 10);
        next = end;
        for (; count < expected; ++count) {
            double value = strtod(next, &end);
            if (end == next || !isfinite(value) || value < 0) break;
            next = end;
            samples[count] = sorted[count] = value;
            mean += value / expected;
        }
        if (count != expected || !iterations) { outcome = "fail"; measured = false; }
    }
    if (measured) {
        median = benchmark_median(sorted, count);
        for (unsigned i = 0; i < count; ++i) deviations[i] = fabs(samples[i] - median);
        mad = benchmark_median(deviations, count);
    }
    if (!sqlite_exec_cstr(db, "BEGIN IMMEDIATE"))
        return benchmark_database_error(db, "starting a benchmark checkpoint");
    sqlite_stmt_t *write = measured ? sqlite_stmt_prepare(db,
        "INSERT INTO benchmark_sample(run_id, operation, backend, bits, ordinal, nanoseconds_per_call) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6)") : NULL;
    bool ok = !measured || write;
    for (unsigned i = 0; measured && ok && i < count; ++i) {
        ok = benchmark_bind_case(write, run, operation, backend, bits) &&
            sqlite_stmt_bind_int(write, 5, (int)i) && sqlite_stmt_bind_double(write, 6, samples[i]) &&
            sqlite_stmt_step(write) == SQLITE_STEP_DONE;
        sqlite_stmt_reset(write);
    }
    sqlite_stmt_finalize(write);
    write = sqlite_stmt_prepare(db,
        "UPDATE benchmark_result SET outcome = ?5, finished_utc = strftime('%Y-%m-%dT%H:%M:%fZ', 'now'), "
        "iterations = ?6, resolution_ns = ?7, median_ns = ?8, mean_ns = ?9, minimum_ns = ?10, "
        "maximum_ns = ?11, mad_ns = ?12, sample_count = ?13, detail = ?14 "
        "WHERE run_id = ?1 AND operation = ?2 AND backend = ?3 AND bits = ?4");
    ok = ok && benchmark_bind_case(write, run, operation, backend, bits) &&
        sqlite_stmt_bind_text(write, 5, outcome);
    if (measured) {
        ok = ok && sqlite_stmt_bind_int(write, 6, (int)iterations) &&
             sqlite_stmt_bind_int64(write, 7, (int64_t)resolution) &&
             sqlite_stmt_bind_double(write, 8, median) && sqlite_stmt_bind_double(write, 9, mean) &&
             sqlite_stmt_bind_double(write, 10, sorted[0]) && sqlite_stmt_bind_double(write, 11, sorted[count - 1]) &&
             sqlite_stmt_bind_double(write, 12, mad);
    } else {
        for (int i = 6; ok && i <= 12; ++i) ok = sqlite_stmt_bind_null(write, i);
    }
    ok = ok && sqlite_stmt_bind_int(write, 13, measured ? (int)count : 0) &&
         sqlite_stmt_bind_text(write, 14, errors ? string_c_str(errors) : "") &&
         sqlite_stmt_step(write) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(write);
    /* Indexed history lookup, excluding previous outliers and incompatible machine/build/workload samples. */
    write = sqlite_stmt_prepare(db,
        "UPDATE benchmark_result AS current SET baseline_median_ns = ("
        " SELECT previous.median_ns FROM benchmark_result AS previous "
        " JOIN benchmark_run AS old ON old.id = previous.run_id "
        " JOIN benchmark_run AS now ON now.id = current.run_id "
        " WHERE previous.operation = current.operation AND previous.backend = current.backend "
        " AND previous.bits = current.bits AND previous.arguments = current.arguments "
        " AND previous.complex_mask = current.complex_mask AND previous.run_id < current.run_id "
        " AND previous.outcome = 'pass' AND previous.timing_outlier = 0 "
        " AND old.machine = now.machine AND old.cpu = now.cpu AND old.system = now.system "
        " AND old.compiler = now.compiler AND old.build_flags = now.build_flags AND old.clock = now.clock "
        " ORDER BY previous.run_id DESC LIMIT 1) "
        "WHERE run_id = ?1 AND operation = ?2 AND backend = ?3 AND bits = ?4 AND outcome = 'pass'");
    ok = ok && benchmark_bind_case(write, run, operation, backend, bits) &&
        sqlite_stmt_step(write) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(write);
    write = sqlite_stmt_prepare(db,
        "UPDATE benchmark_result SET timing_outlier = 1 WHERE run_id = ?1 AND operation = ?2 "
        "AND backend = ?3 AND bits = ?4 AND outcome = 'pass' AND baseline_median_ns IS NOT NULL "
        "AND median_ns > baseline_median_ns * 1.5 AND median_ns - baseline_median_ns > 500");
    ok = ok && benchmark_bind_case(write, run, operation, backend, bits) &&
        sqlite_stmt_step(write) == SQLITE_STEP_DONE;
    sqlite_stmt_finalize(write);
    if (ok) ok = sqlite_exec_cstr(db, "COMMIT");
    if (!ok) {
        benchmark_database_error(db, "saving a benchmark checkpoint");
        (void)sqlite_exec_cstr(db, "ROLLBACK");
    }
    return ok;
}

/* Run bounded mathematical cases and retain completed observations when interrupted. */
int benchmark_run(const string_t *root, sqlite_t *db, int argc, char **argv)
{
    benchmark_options_t options;
    if (!benchmark_options(argc, argv, &options)) return 2;
    string_t *db_path = checks_path(root, "bench/benchmarks.db");
    file_t *lock = file_new(db_path);
    string_free(db_path);
    if (!lock || !file_open_read(lock) || !file_lock(lock, true, false)) {
        fputs("Another benchmark process holds the run lock.\n", stderr);
        file_free(lock);
        return 1;
    }
    if (!benchmark_retention_apply(db, options.resume, true)) { file_free(lock); return 1; }
    string_t *self = string_new_with("/proc/self/exe"), *library_path = checks_path(root, MARS_BENCHMARK_LIBRARY);
    string_t *library = benchmark_digest(library_path), *executable = benchmark_digest(self);
    long run = library && executable ? benchmark_start(db, &options, library, executable) : 0;
    string_free(library_path); string_free(library); string_free(executable);
    if (!run) {
        fputs("Cannot start benchmark run: check selection, storage cap, schema and matching build for resume.\n", stderr);
        file_free(lock); string_free(self);
        return 1;
    }
    sqlite_stmt_t *metadata = sqlite_stmt_prepare(db,
        "SELECT b.requested_samples, b.case_timeout_seconds, count(r.operation),\n"
        "       sum(r.outcome = 'pass'), sum(r.outcome = 'fail'),\n"
        "       sum(r.outcome = 'timeout'), sum(r.outcome = 'unsupported')\n"
        "FROM benchmark_run AS b LEFT JOIN benchmark_result AS r ON r.run_id = b.id\n"
        "WHERE b.id = ?1 GROUP BY b.id");
    bool ok = metadata && sqlite_stmt_bind_int64(metadata, 1, run) && sqlite_stmt_step(metadata) == SQLITE_STEP_ROW;
    if (!ok) benchmark_database_error(db, "reading benchmark run metadata");
    unsigned samples = ok ? (unsigned)sqlite_stmt_column_int(metadata, 0) : 0;
    double timeout = ok ? sqlite_stmt_column_double(metadata, 1) : 0;
    benchmark_progress_counts_t counts = {0};
    if (ok) counts = (benchmark_progress_counts_t){
        .total = (size_t)sqlite_stmt_column_int64(metadata, 2),
        .passed = (size_t)sqlite_stmt_column_int64(metadata, 3),
        .failed = (size_t)sqlite_stmt_column_int64(metadata, 4),
        .timed_out = (size_t)sqlite_stmt_column_int64(metadata, 5),
        .skipped = (size_t)sqlite_stmt_column_int64(metadata, 6)
    };
    int total = (int)counts.total, completed = (int)(counts.passed + counts.failed + counts.timed_out + counts.skipped);
    sqlite_stmt_finalize(metadata);
    benchmark_progress_t *progress = ok ? benchmark_progress_new(&counts, timeout, options.verbose) : NULL;
    if (ok && !progress) fputs("Cannot initialise benchmark progress; pending cases are retained.\n", stderr);
    ok = ok && progress;
    int failures = 0;
    printf("Benchmark run %ld: %d cases, sequential; %u samples per case.\n", run, total, samples);
    fflush(stdout);
    while (ok && !checks_process_interrupt_signal()) {
        if (!benchmark_retention_checkpoint(db, run)) { ok = false; break; }
        sqlite_stmt_t *pending = sqlite_stmt_prepare(db,
            "SELECT r.operation, r.backend, r.bits, r.arguments,\n"
            "       w.operation IS NOT NULL AND w.arguments = r.arguments\n"
            "FROM benchmark_result AS r\n"
            "LEFT JOIN benchmark_workload AS w ON w.operation = r.operation AND w.backend = r.backend\n"
            "WHERE r.run_id = ?1 AND r.outcome = 'pending'\n"
            "ORDER BY r.sequence LIMIT 1");
        ok = pending && sqlite_stmt_bind_int64(pending, 1, run);
        sqlite_step_result_t step = ok ? sqlite_stmt_step(pending) : SQLITE_STEP_ERROR;
        if (step != SQLITE_STEP_ROW) {
            ok = step == SQLITE_STEP_DONE;
            if (!ok) benchmark_database_error(db, "selecting the next pending benchmark");
            sqlite_stmt_finalize(pending);
            break;
        }
        string_t *operation = checks_text(sqlite_stmt_column_text(pending, 0));
        string_t *backend = checks_text(sqlite_stmt_column_text(pending, 1));
        int bits = sqlite_stmt_column_int(pending, 2);
        string_t *operands = checks_text(sqlite_stmt_column_text(pending, 3));
        bool supported = sqlite_stmt_column_int(pending, 4) != 0;
        const benchmark_operation_t *native = benchmark_operation(string_c_str(operation));
        supported = !benchmark_backend_exact(string_c_str(backend)) || supported ||
                    (native && native->integer_only && checks_equal(backend, "exact-integer"));
        sqlite_stmt_finalize(pending);
        checks_strings_t *arguments = checks_strings_new();
        checks_strings_add(arguments, string_clone(self));
        checks_strings_add(arguments, checks_text("bench-db"));
        checks_strings_add(arguments, checks_text("measure"));
        checks_strings_add(arguments, string_clone(operation));
        checks_strings_add(arguments, string_clone(backend));
        checks_strings_add(arguments, string_sprintf("%d", bits));
        checks_strings_add(arguments, string_sprintf("%u", samples));
        checks_strings_add(arguments, operands);
        benchmark_progress_case(progress, string_c_str(operation), string_c_str(backend), bits);
        string_t *output = NULL, *errors = NULL;
        int status = supported ? -1 : 3;
        errno = 0;
        bool ran = !supported || checks_process_run_observed(arguments, root, timeout, &output, &errors, &status,
                                                              benchmark_progress_observe, progress);
        const char *outcome = ran && status == 0 ? "pass" : ran && status == 3 ? "unsupported" :
                              errno == ETIMEDOUT ? "timeout" : "fail";
        if (!checks_process_interrupt_signal()) {
            ok = benchmark_save(db, run, string_c_str(operation), string_c_str(backend), bits,
                                outcome, output, errors, samples);
            if (!ok) fputs("The benchmark checkpoint was not committed; its case remains pending.\n", stderr);
            if (ok) {
                benchmark_progress_complete(progress, outcome);
                if (!strcmp(outcome, "fail") || !strcmp(outcome, "timeout")) ++failures;
                ++completed;
            }
        }
        string_free(output); string_free(errors); string_free(operation); string_free(backend);
        checks_strings_free(arguments);
    }
    sqlite_stmt_t *finish = sqlite_stmt_prepare(db,
        "UPDATE benchmark_run SET status = ?2, finished_utc = strftime('%Y-%m-%dT%H:%M:%fZ', 'now') WHERE id = ?1");
    bool finished = finish && sqlite_stmt_bind_int64(finish, 1, run) &&
                    sqlite_stmt_bind_text(finish, 2, completed == total && ok ? "complete" : "interrupted") &&
                    sqlite_stmt_step(finish) == SQLITE_STEP_DONE;
    if (!finished) benchmark_database_error(db, "recording benchmark completion status");
    ok = finished && ok;
    sqlite_stmt_finalize(finish);
    benchmark_progress_finish(progress, completed == total && ok);
    finish = sqlite_stmt_prepare(db,
        "SELECT sum(outcome IN ('fail', 'timeout')), sum(timing_outlier) FROM benchmark_result WHERE run_id = ?1");
    if (finish && sqlite_stmt_bind_int64(finish, 1, run) && sqlite_stmt_step(finish) == SQLITE_STEP_ROW) {
        failures = sqlite_stmt_column_int(finish, 0);
        int outliers = sqlite_stmt_column_int(finish, 1);
        if (outliers) fprintf(stderr, "Warning: %d benchmark slowdown outliers retained; inspect this run before "
                              "accepting a performance change.\n", outliers);
    } else ok = benchmark_database_error(db, "reading benchmark outcome counts");
    sqlite_stmt_finalize(finish);
    if (!benchmark_retention_apply(db, completed == total && ok ? 0 : run, false)) ok = false;
    if (completed != total)
        fprintf(stderr, "Completed checkpoints are preserved. Resume with make bench-db ARGS='run --resume %ld' "
                "using this same build.\n", run);
    file_free(lock); string_free(self);
    return ok && !failures && completed == total ? 0 : 1;
}
