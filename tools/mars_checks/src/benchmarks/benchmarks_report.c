/**
 * @file benchmarks_report.c
 * @brief Database-derived benchmark tables and staged Markdown refreshes.
 *
 * Uses indexed result lookups from one selected run. Precision columns are discovered from that run rather than
 * assumed. Unsupported-only operations and backends are omitted without changing stored observations.
 * Source markers delimit managed tables; all documents are prepared before publication, and unrelated
 * prose is preserved byte for byte. No benchmark is executed while rendering documentation.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file-t.h"
#include "checks_support.h"
#define MARS_BENCHMARKS_INTERNAL_ACCESS
#include "benchmarks_internal.h"

static void benchmark_duration(string_t *out, double ns)
{
    if (ns < 1000) string_append_format(out, "%.1f ns", ns);
    else if (ns < 1000000) string_append_format(out, "%.1f µs", ns / 1000);
    else if (ns < 1000000000) string_append_format(out, "%.3f ms", ns / 1000000);
    else string_append_format(out, "%.3f s", ns / 1000000000);
}

static bool benchmark_backend_table(sqlite_t *db, long run, const char *backend, const char *heading, string_t *text)
{
    sqlite_stmt_t *profiles = sqlite_stmt_prepare(db,
        "SELECT DISTINCT bits\n"
        "FROM benchmark_result\n"
        "WHERE run_id = ?1 AND backend = ?2 AND outcome <> 'unsupported'\n"
        "ORDER BY bits");
    bool ok = profiles && sqlite_stmt_bind_int64(profiles, 1, run) && sqlite_stmt_bind_text(profiles, 2, backend);
    int bits[64];
    unsigned columns = 0;
    sqlite_step_result_t step = SQLITE_STEP_DONE;
    while (ok && (step = sqlite_stmt_step(profiles)) == SQLITE_STEP_ROW) {
        if (columns == sizeof bits / sizeof *bits) { ok = false; break; }
        bits[columns++] = sqlite_stmt_column_int(profiles, 0);
    }
    sqlite_stmt_finalize(profiles);
    if (!ok || step != SQLITE_STEP_DONE) return false;
    if (!columns) return true;
    if (heading) string_append_format(text, "\n### %s\n\n| Operation | Inputs |", heading);
    else string_append_format(text, "\n### `%s`\n\n| Operation | Inputs |", backend);
    for (unsigned i = 0; i < columns; ++i) {
        if (bits[i]) string_append_format(text, " %d bits |", bits[i]);
        else string_append_cstr(text, " Exact |");
    }
    string_append_cstr(text, "\n|---|---|");
    for (unsigned i = 0; i < columns; ++i) string_append_cstr(text, "---:|");
    string_append_cstr(text, "\n");
    sqlite_stmt_t *rows = sqlite_stmt_prepare(db,
        "SELECT operation, arguments, complex_mask\n"
        "FROM benchmark_result\n"
        "WHERE run_id = ?1 AND backend = ?2 AND outcome <> 'unsupported'\n"
        "GROUP BY operation, arguments, complex_mask\n"
        "ORDER BY operation, arguments, complex_mask");
    sqlite_stmt_t *cell = sqlite_stmt_prepare(db,
        "SELECT outcome, median_ns, timing_outlier FROM benchmark_result "
        "WHERE run_id = ?1 AND operation = ?2 AND backend = ?3 AND bits = ?4 "
        "  AND arguments = ?5 AND complex_mask = ?6");
    ok = rows && cell && sqlite_stmt_bind_int64(rows, 1, run) && sqlite_stmt_bind_text(rows, 2, backend);
    while (ok && (step = sqlite_stmt_step(rows)) == SQLITE_STEP_ROW) {
        const char *name = sqlite_stmt_column_text(rows, 0);
        const char *arguments = sqlite_stmt_column_text(rows, 1);
        unsigned mask = (unsigned)sqlite_stmt_column_int(rows, 2);
        string_t *inputs = benchmark_arguments_text(arguments, backend, mask);
        ok = inputs != NULL;
        if (ok) string_append_format(text, "| %s | %s |", name, string_c_str(inputs));
        string_free(inputs);
        for (unsigned i = 0; ok && i < columns; ++i) {
            ok = sqlite_stmt_bind_int64(cell, 1, run) && sqlite_stmt_bind_text(cell, 2, name) &&
                 sqlite_stmt_bind_text(cell, 3, backend) && sqlite_stmt_bind_int(cell, 4, bits[i]) &&
                 sqlite_stmt_bind_text(cell, 5, arguments) && sqlite_stmt_bind_int(cell, 6, (int)mask);
            sqlite_step_result_t value = ok ? sqlite_stmt_step(cell) : SQLITE_STEP_ERROR;
            if (value == SQLITE_STEP_ROW) {
                const char *outcome = sqlite_stmt_column_text(cell, 0);
                string_append_cstr(text, " ");
                if (!strcmp(outcome, "pass")) {
                    benchmark_duration(text, sqlite_stmt_column_double(cell, 1));
                    if (sqlite_stmt_column_int(cell, 2)) string_append_cstr(text, " †");
                } else string_append_cstr(text, !strcmp(outcome, "unsupported") ? "—" : outcome);
                string_append_cstr(text, " |");
            } else if (value == SQLITE_STEP_DONE) string_append_cstr(text, " — |");
            else ok = false;
            sqlite_stmt_reset(cell);
        }
        string_append_cstr(text, "\n");
    }
    sqlite_stmt_finalize(cell);
    sqlite_stmt_finalize(rows);
    return ok && step == SQLITE_STEP_DONE;
}

static string_t *benchmark_document(sqlite_t *db, long run, const char *module)
{
    sqlite_stmt_t *info = sqlite_stmt_prepare(db,
        "SELECT started_utc, machine, cpu, system, compiler, build_flags, requested_samples, clock "
        "FROM benchmark_run WHERE id = ?1 AND status = 'complete'");
    bool ok = info && sqlite_stmt_bind_int64(info, 1, run) && sqlite_stmt_step(info) == SQLITE_STEP_ROW;
    string_t *out = ok ? string_new() : NULL;
    if (out) {
        string_append_format(out, "\nBenchmark run **%ld**, %s (UTC).\n\nMachine: %s; %s.\n\nSystem: %s.\n\n"
            "Compiler: %s; build flags: %s.\n\nMedian per call from %d timed batches.\n\nTiming method: %s.\n\n"
            "Argument parsing, output validation, disposal and database writes are outside timing.\n"
            "Inputs show the actual operands, including +0.125i for permitted complex arguments; integer orders remain real.\n"
            "`number-double` includes generic number dispatch; the other fixed profiles call qfloat/qcomplex directly.\n"
            "Requested multiprecision bits do not guarantee that every algorithm achieves that accuracy.\n"
            "Operations and precision columns with only unsupported results are omitted; — denotes an unsupported cell.\n"
            "Failures and timeouts remain visible. † marks a retained slowdown outlier.\n",
            run, sqlite_stmt_column_text(info, 0), sqlite_stmt_column_text(info, 1), sqlite_stmt_column_text(info, 2),
            sqlite_stmt_column_text(info, 3), sqlite_stmt_column_text(info, 4), sqlite_stmt_column_text(info, 5),
            sqlite_stmt_column_int(info, 6), sqlite_stmt_column_text(info, 7));
    }
    sqlite_stmt_finalize(info);
    static const struct { const char *key, *number_heading; } backends[] = {
        {"number-double", "number / double precision"},
        {"qfloat", NULL},
        {"qcomplex", NULL},
        {"mpfr", "number / multi-precision floating point"},
        {"mpc", "number / multi-precision complex floating point"},
        {"exact-integer", "number / multi-precision integer"}
    };
    bool number_module = !strcmp(module, "number");
    for (size_t i = 0; ok && i < sizeof backends / sizeof *backends; ++i) {
        bool selected = !strcmp(module, "all") || (number_module && backends[i].number_heading) ||
            !strcmp(module, backends[i].key);
        if (selected) ok = benchmark_backend_table(db, run, backends[i].key,
                                                 number_module ? backends[i].number_heading : NULL, out);
    }
    if (ok && number_module) {
        string_append_cstr(out,
            "\n### number / multi-precision fraction\n\n"
            "`number_t` represents exact fractions using GMP rational arithmetic (MPQ). Addition, subtraction,\n"
            "multiplication, division and integer powers retain exact rational results within the configured\n"
            "size budget. Their cost depends on numerator and denominator sizes and fraction reduction,\n"
            "rather than floating-point precision alone. General transcendental functions usually require\n"
            "inexact real or complex results.\n\n"
            "The current benchmark run has no separate fraction measurements. A meaningful fraction table\n"
            "would specify numerator and denominator bit lengths and distinguish exact arithmetic from\n"
            "conversion to floating point; the floating-point tables above do not measure that workload.\n");
    }
    if (!ok) { string_free(out); return NULL; }
    return out;
}

/* Publish only marked benchmark sections, using a single stored run for every table. */
int benchmark_report(const string_t *root, sqlite_t *db, long run, bool update)
{
    if (!run) {
        const char *query = update
            ? "SELECT max(b.id) FROM benchmark_run AS b WHERE b.status = 'complete' AND b.filter = '' "
              "AND (SELECT count(*) FROM benchmark_result WHERE run_id = b.id) = "
              "(SELECT count(*) FROM benchmark_operation) * (SELECT count(*) FROM benchmark_profile)"
            : "SELECT max(id) FROM benchmark_run WHERE status = 'complete'";
        sqlite_stmt_t *latest = sqlite_stmt_prepare(db, query);
        if (latest && sqlite_stmt_step(latest) == SQLITE_STEP_ROW) run = (long)sqlite_stmt_column_int64(latest, 0);
        sqlite_stmt_finalize(latest);
    }
    if (!run) { fputs("No completed benchmark run is available.\n", stderr); return 1; }
    if (!update) {
        string_t *out = benchmark_document(db, run, "all");
        bool ok = out && fputs(string_c_str(out), stdout) != EOF;
        string_free(out);
        return ok ? 0 : 1;
    }
    static const struct { const char *path, *module; } guides[] = {
        {"docs/benchmarks.md", "all"}, {"docs/number.md", "number"},
        {"docs/qfloat.md", "qfloat"}, {"docs/qcomplex.md", "qcomplex"}
    };
    string_t *directory = checks_path(root, "docs");
    file_t *staging = file_create_temp_directory(directory);
    string_free(directory);
    if (!staging) return 1;
    file_t *prepared[4] = {0}, *destinations[4] = {0};
    bool ok = true;
    for (unsigned i = 0; ok && i < 4; ++i) {
        string_t *path = checks_path(root, guides[i].path), *source = checks_read(path);
        string_t *content = benchmark_document(db, run, guides[i].module);
        const char *text = source ? string_c_str(source) : "";
        const char *begin = strstr(text, "<!-- MARS-BENCHMARKS:BEGIN -->");
        const char *end = begin ? strstr(begin, "<!-- MARS-BENCHMARKS:END -->") : NULL;
        ok = content && begin && end && !strstr(begin + 1, "<!-- MARS-BENCHMARKS:BEGIN -->") &&
             !strstr(end + 1, "<!-- MARS-BENCHMARKS:END -->");
        string_t *replacement = ok ? checks_slice(source, 0,
            (size_t)(begin - text) + strlen("<!-- MARS-BENCHMARKS:BEGIN -->")) : NULL;
        if (replacement) {
            checks_append(replacement, content);
            string_append_cstr(replacement, "\n");
            string_t *tail = checks_slice(source, (size_t)(end - text),
                                          string_byte_length(source) - (size_t)(end - text));
            checks_append(replacement, tail);
            string_free(tail);
            string_t *temporary = string_sprintf("%s/%u.md", file_path(staging), i);
            prepared[i] = file_new(temporary);
            destinations[i] = file_new(path);
            ok = prepared[i] && destinations[i] && file_write_all_text(prepared[i], replacement);
            string_free(temporary);
        }
        string_free(replacement); string_free(content); string_free(source); string_free(path);
    }
    for (unsigned i = 0; ok && i < 4; ++i) ok = file_move(prepared[i], destinations[i], true);
    for (unsigned i = 0; i < 4; ++i) {
        if (prepared[i] && file_exists(prepared[i])) (void)file_delete(prepared[i]);
        file_free(prepared[i]); file_free(destinations[i]);
    }
    if (!file_remove_directory(staging)) ok = false;
    file_free(staging);
    if (!ok) fputs("Cannot refresh benchmark tables: check markers, completed run and document permissions.\n", stderr);
    else printf("Benchmark tables refreshed from run %ld.\n", run);
    return ok ? 0 : 1;
}
