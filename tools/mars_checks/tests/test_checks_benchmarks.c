/**
 * @file test_checks_benchmarks.c
 * @brief Stateful benchmark catalogue, checkpoint and Markdown publication regressions.
 *
 * Each fixture owns its database and documents. Real mathematical workers exercise the command boundary; fabricated
 * historical rows test storage retention and reporting. README code follows the ordinary regressions.
 * Neither the repository's benchmark history nor its Markdown guides are modified by these tests.
 */
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include "file-t.h"
#include "expression-t.h"
#include "number-t.h"
#include "sqlite-t.h"
#include "xml-t.h"
#include "checks_benchmarks.h"
#include "checks_controls.h"
#include "checks_process.h"
#include "test_harness.h"
#include "test_checks.h"

static bool benchmark_test_environment(const string_t *root, const char *const *options, size_t count,
                                        int wanted, string_t **captured, const char *const *environment, size_t env_count,
                                        string_t **diagnostics)
{
    checks_strings_t *args = checks_strings_new();
    checks_strings_add(args, checks_text("env"));
    checks_strings_add(args, string_sprintf("MARS_ROOT=%s", string_c_str(root)));
    checks_strings_add(args, string_sprintf("MARS_HOME=%s", string_c_str(root)));
    for (size_t i = 0; i < env_count; ++i) checks_strings_add(args, checks_text(environment[i]));
    checks_strings_add(args, checks_text(MARS_CHECKS_PROGRAM_PATH));
    checks_strings_add(args, checks_text("bench-db"));
    for (size_t i = 0; i < count; ++i) checks_strings_add(args, checks_text(options[i]));
    string_t *out = NULL, *errors = NULL;
    int status = -1;
    bool ran = checks_process_run(args, root, 60, &out, &errors, &status);
    bool ok = ran && status == wanted;
    if (!ok) fprintf(stderr, "Benchmark command failed (%d): %s\n%s\n", status,
                     out ? string_c_str(out) : "", errors ? string_c_str(errors) : "");
    if (diagnostics) *diagnostics = errors;
    else string_free(errors);
    checks_strings_free(args);
    if (captured) *captured = out;
    else string_free(out);
    return ok;
}

static bool benchmark_test_command(const string_t *root, const char *const *options, size_t count,
                                    int wanted, string_t **captured)
{
    return benchmark_test_environment(root, options, count, wanted, captured, NULL, 0, NULL);
}

static bool benchmark_test_fixture(const string_t *root)
{
    string_t *repository = checks_text(MARS_CHECKS_ROOT_DIR);
    string_t *schema_path = checks_path(repository, "bench/benchmarks.sql");
    string_t *schema = checks_read(schema_path);
    bool ok = schema && test_checks_write(root, "bench/benchmarks.sql", string_c_str(schema)) &&
        test_checks_write(root, "build/release/libmars.a", "isolated build fingerprint fixture");
    string_free(schema_path);
    string_free(schema);
    schema_path = checks_path(repository, "bench/number/bench_number.sql");
    schema = checks_read(schema_path);
    ok = ok && schema && test_checks_write(root, "bench/number/bench_number.sql", string_c_str(schema));
    string_free(schema_path);
    string_free(schema);
    string_free(repository);
    return ok;
}

static sqlite_t *benchmark_test_open(const string_t *root)
{
    string_t *path = checks_path(root, "bench/benchmarks.db");
    sqlite_t *db = sqlite_open_plain(path);
    string_free(path);
    return db;
}

static long benchmark_test_integer(sqlite_t *db, const char *sql)
{
    sqlite_stmt_t *query = sqlite_stmt_prepare(db, sql);
    long result = query && sqlite_stmt_step(query) == SQLITE_STEP_ROW
        ? (long)sqlite_stmt_column_int64(query, 0) : -1;
    sqlite_stmt_finalize(query);
    return result;
}

static void test_benchmarks_catalogue(void)
{
    string_t *root = test_checks_root();
    const char *status[] = {"status"}, *sync[] = {"sync"}, *stats[] = {"stats"};
    bool ok = benchmark_test_fixture(root) && benchmark_test_command(root, status, 1, 1, NULL) &&
        benchmark_test_command(root, stats, 1, 1, NULL);
    string_t *path = checks_path(root, "bench/benchmarks.db");
    ok = !checks_is_file(path) && ok;
    string_free(path);
    ok = ok && benchmark_test_command(root, sync, 1, 0, NULL);
    sqlite_t *db = benchmark_test_open(root);
    ok = db && benchmark_test_integer(db, "SELECT count(*) FROM benchmark_operation") >= 164 &&
        benchmark_test_integer(db, "SELECT count(*) FROM benchmark_profile") == 17 &&
        benchmark_test_integer(db, "SELECT count(*) FROM benchmark_workload") == 45 &&
        sqlite_exec_cstr(db,
            "INSERT INTO benchmark_operation VALUES ('retained_custom_case', 1, '2', 0, 0, 0);\n"
            "INSERT INTO benchmark_workload VALUES ('retained_custom_case', 'exact-fraction', '2/3');") && ok;
    sqlite_close(db);
    ok = ok && benchmark_test_command(root, sync, 1, 0, NULL);
    db = benchmark_test_open(root);
    ok = db && benchmark_test_integer(db,
        "SELECT count(*) FROM benchmark_operation WHERE name = 'retained_custom_case'") == 1 &&
        benchmark_test_integer(db,
            "SELECT count(*) FROM benchmark_workload\n"
            "WHERE operation = 'retained_custom_case' AND backend = 'exact-fraction' AND arguments = '2/3'") == 1 && ok;
    sqlite_close(db);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "benchmark storage is explicit, profiles cover every documented precision, and sync preserves cases");
}

static bool benchmark_test_stats_fixture(const string_t *root)
{
    const char *sync[] = {"sync"};
    if (!benchmark_test_fixture(root) || !benchmark_test_command(root, sync, 1, 0, NULL)) return false;
    sqlite_t *db = benchmark_test_open(root);
    bool ok = db && sqlite_exec_cstr(db,
        "INSERT INTO benchmark_run(id, status, machine, cpu, system, compiler, build_flags, library_sha256,\n"
        "                          executable_sha256, requested_samples, case_timeout_seconds)\n"
        "VALUES (1, 'complete', 'fixture', 'fixture CPU', 'fixture OS', 'fixture CC', '-O2', 'library', 'binary', 7, 3),\n"
        "       (2, 'running', 'fixture', 'fixture CPU', 'fixture OS', 'fixture CC', '-O2', 'library', 'binary', 7, 3);\n"
        "INSERT INTO benchmark_result(run_id, operation, backend, bits, arguments, complex_mask, sequence, outcome,\n"
        "                             median_ns, mean_ns, minimum_ns, maximum_ns, baseline_median_ns, sample_count,\n"
        "                             timing_outlier, detail)\n"
        "VALUES (1, 'sin', 'qfloat', 106, '0.7', 0, 1, 'pass', 1000, 1200, 900, 1300, 2000, 7, 0, ''),\n"
        "       (1, 'cos', 'qfloat', 106, '0.7', 0, 2, 'pass', 1020, 1100, 900, 1400, 1000, 7, 0, ''),\n"
        "       (1, 'hypergeometric_pFq', 'qfloat', 106, '2,1,0.7', 0, 3, 'pass',\n"
        "           4000, 4100, 3900, 4400, 1000, 7, 1, 'retained slowdown'),\n"
        "       (1, 'acos', 'qfloat', 106, '0.7', 0, 4, 'fail', NULL,NULL,NULL,NULL,NULL,NULL,0,\n"
        "           'non-finite result' || char(10) || char(9) || char(13) || char(27) || '[31m'),\n"
        "       (1, 'atanh', 'qfloat', 106, '0.7', 0, 5, 'timeout', NULL,NULL,NULL,NULL,NULL,NULL,0,'case deadline'),\n"
        "       (1, 'asin', 'qfloat', 106, '0.7', 0, 6, 'unsupported', NULL,NULL,NULL,NULL,NULL,NULL,0,'unavailable'),\n"
        "       (1, 'exp', 'qfloat', 106, '0.7', 0, 7, 'pending', NULL,NULL,NULL,NULL,NULL,NULL,0,''),\n"
        "       (2, 'sin', 'qcomplex', 106, '0.7', 1, 1, 'pass', 2000,2200,1900,2300,NULL,7,0,'');");
    sqlite_close(db);
    return ok;
}

static bool benchmark_test_grid_width(const string_t *text, size_t width)
{
    size_t count = 0;
    string_t **lines = string_split(text, "\n", &count);
    bool ok = lines != NULL;
    size_t last = 0;
    for (size_t i = 0; ok && i < count; ++i) {
        if (!string_starts_with(lines[i], "║") && !string_starts_with(lines[i], "╔") &&
            !string_starts_with(lines[i], "╠") && !string_starts_with(lines[i], "╟") &&
            !string_starts_with(lines[i], "╚")) continue;
        size_t length = string_length(lines[i]);
        ok = length <= width && (!last || last == length);
        last = length;
    }
    string_split_free(lines, count);
    return ok && last > 0;
}

static bool benchmark_test_column_contains(const string_t *text, unsigned index, const char *wanted)
{
    size_t count = 0;
    string_t **lines = string_split(text, "\n", &count), *joined = string_new(), *expected = string_new_with(wanted);
    bool ok = lines && joined && expected;
    for (size_t i = 0; ok && i < count; ++i) {
        if (!string_starts_with(lines[i], "║")) continue;
        const char *line = string_c_str(lines[i]), *column = line + strlen("║");
        for (unsigned j = 0; column && j < index; ++j) {
            column = strstr(column, "│");
            if (column) column += strlen("│");
        }
        if (!column) continue;
        const char *end = strstr(column, "│");
        if (!end) end = strstr(column, "║");
        string_t *value = end ? checks_slice(lines[i], (size_t)(column - line), (size_t)(end - column)) : NULL;
        ok = value != NULL;
        if (value) { string_trim(value); checks_append(joined, value); }
        string_free(value);
    }
    ok = ok && string_replace(joined, " ", "") >= 0 && string_replace(expected, " ", "") >= 0 &&
        string_find(joined, string_c_str(expected)) >= 0;
    string_free(expected);
    string_free(joined);
    string_split_free(lines, count);
    return ok;
}

static void test_benchmarks_stats(void)
{
    string_t *root = test_checks_root(), *out = NULL;
    const char *plain[] = {"NO_COLOR=1", "COLUMNS=132"};
    const char *colour[] = {"NO_COLOR=", "CLICOLOR_FORCE=1", "TERM=xterm", "COLUMNS=132"};
    const char *stats[] = {"stats", "--run", "1", "--summary"};
    bool ok = benchmark_test_stats_fixture(root) &&
        benchmark_test_environment(root, stats, 4, 0, &out, plain, 2, NULL);
    ok = ok && string_find(out, "╔") >= 0 && string_find(out, "╠") >= 0 && string_find(out, "╚") >= 0 &&
        string_find(out, "Arguments") >= 0 && string_find(out, "Inputs:") < 0 &&
        benchmark_test_column_contains(out, 1, "2,1,0.7") &&
        string_find(out, "Median µs") >= 0 && string_find(out, "Mean µs") >= 0 &&
        string_find(out, "-50 ▲") >= 0 && string_find(out, "2 ▬") >= 0 && string_find(out, "300 ▼ †") >= 0 &&
        string_find(out, "FAIL") >= 0 && string_find(out, "TIMEOUT") >= 0 && string_find(out, "SKIPPED") < 0 &&
        string_find(out, "asin") < 0 &&
        string_find(out, "PENDING") >= 0 && string_find(out, "\033") < 0 && benchmark_test_grid_width(out, 132);
    if (!ok && out) fprintf(stderr, "Plain benchmark grid: %s\n", string_c_str(out));
    string_free(out); out = NULL;
    const char *including_skipped[] = {"stats", "--run", "1", "--summary", "--include-skipped"};
    ok = ok && benchmark_test_environment(root, including_skipped, 5, 0, &out, colour, 4, NULL);
    ok = ok && string_find(out, "\033[1;92mPASS") >= 0 && string_find(out, "\033[1;91mFAIL") >= 0 &&
        string_find(out, "\033[1;93mSKIPPED") >= 0 && string_find(out, "\033[96m") >= 0 &&
        string_find(out, "\033[1;38;2;255;140;0m2 ▬") >= 0 && string_find(out, "\033[1;96m") >= 0;
    if (!ok && out) fprintf(stderr, "Coloured benchmark grid: %s\n", string_c_str(out));
    string_free(out); out = NULL;
    const char *narrow[] = {"NO_COLOR=1", "COLUMNS=60"};
    const char *filtered[] = {"stats", "--run", "1", "--operation", "acos", "--backend", "qfloat", "--bits", "106",
                              "--details"};
    ok = ok && benchmark_test_environment(root, filtered, 10, 0, &out, narrow, 2, NULL) &&
        benchmark_test_column_contains(out, 1, "non-finite result") && string_find(out, "calls/batch") < 0 &&
        benchmark_test_grid_width(out, 60);
    if (!ok && out) fprintf(stderr, "Narrow benchmark panel: %s\n", string_c_str(out));
    string_free(out); out = NULL;
    const char *latest[] = {"stats"};
    ok = ok && benchmark_test_command(root, latest, 1, 0, &out) &&
        string_find(out, "Benchmark run 2 — running") >= 0 && string_find(out, "Backend: qcomplex") >= 0;
    string_free(out);
    sqlite_t *db = benchmark_test_open(root);
    ok = db && benchmark_test_integer(db, "SELECT count(*) FROM benchmark_result") == 8 &&
        benchmark_test_integer(db, "SELECT timing_outlier FROM benchmark_result WHERE operation='hypergeometric_pFq'") == 1 && ok;
    sqlite_close(db);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "benchmark grids align, colour outcomes and trends, wrap narrow views and preserve every observation");
}

static void test_benchmarks_stats_exports(void)
{
    string_t *root = test_checks_root(), *out = NULL;
    const char *colour[] = {"NO_COLOR=", "CLICOLOR_FORCE=1", "TERM=xterm"};
    const char *tsv[] = {"stats", "--run", "1", "--operation", "sin", "--format=tsv", "--details"};
    bool ok = benchmark_test_stats_fixture(root) &&
        benchmark_test_environment(root, tsv, 7, 0, &out, colour, 3, NULL);
    ok = ok && string_find(out, "operation\targuments\tresult") >= 0 &&
        string_find(out, "median_us\tmean_us") >= 0 &&
        string_find(out, "\tsin\t0.7\tPASS\t1\t1.2\t0.9\t1.3\t-50\t7\t0") >= 0 &&
        string_find(out, "\033") < 0 && string_find(out, "▲") < 0;
    string_free(out); out = NULL;
    const char *markdown[] = {"stats", "--run", "1", "--operation", "hypergeometric_pFq", "--format=markdown", "--details"};
    ok = ok && benchmark_test_environment(root, markdown, 7, 0, &out, colour, 3, NULL) &&
        string_find(out, "| **Arguments**") >= 0 && string_find(out, "| **Median µs**") >= 0 &&
        string_find(out, "| **Details**") >= 0 &&
        string_find(out, "hypergeometric\\_pFq") >= 0 && string_find(out, "**PASS**") >= 0 &&
        string_find(out, "retained slowdown") >= 0 && string_find(out, "\033") < 0;
    string_free(out); out = NULL;
    const char *unknown[] = {"stats", "--run", "999"}, *bad[] = {"stats", "--run", "nonsense"};
    const char *empty[] = {"stats", "--backend", "not-a-backend"}, *format[] = {"stats", "--format=html"};
    ok = ok && benchmark_test_command(root, unknown, 3, 1, NULL) && benchmark_test_command(root, bad, 3, 2, NULL) &&
        benchmark_test_command(root, format, 2, 2, NULL) && benchmark_test_command(root, empty, 3, 0, &out) &&
        string_find(out, "No recorded benchmark cases match") >= 0;
    string_free(out); out = NULL;
    const char *empty_tsv[] = {"stats", "--backend", "not-a-backend", "--format=tsv"};
    ok = ok && benchmark_test_command(root, empty_tsv, 4, 0, &out) && string_starts_with(out, "backend\tbits\t") &&
        string_find(out, "No recorded benchmark") < 0;
    string_free(out); out = NULL;
    const char *formats[] = {"--format=grid", "--format=markdown", "--format=tsv"};
    for (size_t i = 0; ok && i < sizeof formats / sizeof *formats; ++i) {
        const char *hidden[] = {"stats", "--run", "1", "--operation", "asin", formats[i], "--details"};
        const char *included[] = {"stats", "--run", "1", "--operation", "asin", formats[i], "--details",
                                  "--include-skipped"};
        ok = benchmark_test_environment(root, hidden, 7, 0, &out, colour, 3, NULL) &&
            string_find(out, "SKIPPED") < 0 && string_find(out, "Backend:") < 0;
        string_free(out); out = NULL;
        ok = ok && benchmark_test_environment(root, included, 8, 0, &out, colour, 3, NULL) &&
            string_find(out, "SKIPPED") >= 0 && string_find(out, "asin") >= 0;
        string_free(out); out = NULL;
    }
    sqlite_t *db = benchmark_test_open(root);
    ok = db && benchmark_test_integer(db,
        "SELECT count(*) FROM benchmark_result WHERE outcome = 'unsupported'") == 1 && ok;
    sqlite_close(db);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "benchmark exports are numeric, escaped and colour-free, and invalid or empty selections are explicit");
}

static void test_benchmarks_stats_control_text(void)
{
    string_t *root = test_checks_root(), *out = NULL;
    const char *environment[] = {"NO_COLOR=1", "COLUMNS=132"};
    const char *formats[] = {"--format=grid", "--format=markdown", "--format=tsv"};
    bool ok = benchmark_test_stats_fixture(root);
    sqlite_t *db = benchmark_test_open(root);
    ok = db && sqlite_exec_cstr(db,
        "UPDATE benchmark_result SET arguments = '0.7' || char(10) || char(9) || char(13) || char(27) || '[31m'\n"
        "WHERE run_id = 1 AND operation = 'acos';") && ok;
    sqlite_close(db);
    for (size_t i = 0; ok && i < sizeof formats / sizeof *formats; ++i) {
        const char *options[] = {"stats", "--run", "1", "--operation", "acos", "--details", formats[i]};
        const char *expected = i == 1 ? "\\\\x0a\\\\x09\\\\x0d\\\\x1b\\[31m" : "\\x0a\\x09\\x0d\\x1b[31m";
        const char *argument = i == 1 ? "0.7\\\\x0a" : "0.7\\x0a";
        ok = benchmark_test_environment(root, options, 7, 0, &out, environment, 2, NULL) &&
             string_find(out, expected) >= 0 && string_find(out, argument) >= 0 && string_find(out, "\033") < 0;
        string_free(out); out = NULL;
    }
    db = benchmark_test_open(root);
    ok = db && benchmark_test_integer(db,
        "SELECT instr(detail, char(10)) FROM benchmark_result WHERE run_id=1 AND operation='acos'") > 0 && ok;
    sqlite_close(db);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "stored multiline failure messages remain reportable and terminal control sequences stay literal");
}

static void test_benchmarks_stats_default_details(void)
{
    string_t *root = test_checks_root(), *plain = NULL, *explicit = NULL, *summary = NULL;
    const char *environment[] = {"NO_COLOR=1", "COLUMNS=132"};
    const char *defaults[] = {"stats", "--run", "1", "--operation", "sin"};
    const char *details[] = {"stats", "--run", "1", "--operation", "sin", "--details"};
    const char *compact[] = {"stats", "--run", "1", "--operation", "sin", "--summary"};
    bool ok = benchmark_test_stats_fixture(root) &&
        benchmark_test_environment(root, defaults, 5, 0, &plain, environment, 2, NULL) &&
        benchmark_test_environment(root, details, 6, 0, &explicit, environment, 2, NULL) &&
        benchmark_test_environment(root, compact, 6, 0, &summary, environment, 2, NULL);
    ok = ok && string_compare(plain, summary) == 0 && string_find(plain, "Arguments") >= 0 &&
        benchmark_test_column_contains(plain, 1, "0.7") && string_find(plain, "calls/batch:") < 0 &&
        string_find(explicit, "Arguments") >= 0 && benchmark_test_column_contains(explicit, 1, "0.7") &&
        string_find(explicit, "calls/batch:") >= 0 && string_find(explicit, "clock resolution:") >= 0 &&
        string_find(summary, "╠") >= 0 && string_find(summary, "finished:") < 0 &&
        benchmark_test_grid_width(plain, 132) && benchmark_test_grid_width(summary, 132);
    string_free(plain); string_free(explicit); string_free(summary);
    plain = NULL;
    summary = NULL;
    const char *formats[] = {"--format=markdown", "--format=tsv"};
    for (size_t i = 0; ok && i < sizeof formats / sizeof *formats; ++i) {
        const char *export[] = {"stats", "--run", "1", "--operation", "sin", formats[i]};
        const char *brief[] = {"stats", "--run", "1", "--operation", "sin", formats[i], "--summary"};
        ok = benchmark_test_command(root, export, 6, 0, &plain) &&
            benchmark_test_command(root, brief, 7, 0, &summary) &&
            string_compare(plain, summary) == 0 && string_find(plain, "0.7") >= 0 &&
            string_find(plain, i ? "operation\targuments\tresult" : "| **Arguments**") >= 0 &&
            string_find(summary, "finished:") < 0;
        string_free(plain); plain = NULL;
        string_free(summary); summary = NULL;
    }
    const char *last_details[] = {"stats", "--run", "1", "--operation", "sin", "--summary", "--details"};
    const char *last_summary[] = {"stats", "--run", "1", "--operation", "sin", "--details", "--summary"};
    ok = ok && benchmark_test_command(root, last_details, 7, 0, &plain) &&
        benchmark_test_command(root, last_summary, 7, 0, &summary) &&
        string_find(plain, "calls/batch:") >= 0 && string_find(summary, "calls/batch:") < 0 &&
        string_find(summary, "Arguments") >= 0 && benchmark_test_column_contains(summary, 1, "0.7");
    string_free(plain); string_free(summary);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "default summary retains arguments in every format and explicit details add metadata");
}

static bool benchmark_test_wrapped_inputs(const string_t *text, const char *row, const char *expected, size_t width)
{
    const char *start = strstr(string_c_str(text), row);
    start = start ? start + strlen(row) : NULL;
    const char *end = start ? strstr(start, "</span>") : NULL;
    string_t *markup = end ? checks_slice(text, (size_t)(start - string_c_str(text)),
                                         (size_t)(end - start) + strlen("</span>")) : NULL;
    xml_t *document = markup ? xml_from_text(markup) : NULL;
    const xml_t *span = xml_child_at(document, 0);
    string_t *key = string_new_with("style"), *restored = string_new();
    string_t *wanted_width = string_sprintf("width:%zuch", width);
    const string_t *style = xml_get_attribute(span, key);
    bool ok = span && checks_equal(xml_name(span), "span") && style && wanted_width &&
              string_find(style, string_c_str(wanted_width)) >= 0 && restored;
    for (size_t i = 0; ok && i < xml_child_count(span); ++i) {
        const xml_t *child = xml_child_at(span, i);
        if (xml_type(child) == XML_TEXT) {
            const string_t *part = xml_text(child);
            ok = string_length(part) <= width && string_append_cstr(restored, string_c_str(part)) >= 0;
        } else ok = checks_equal(xml_name(child), "br");
    }
    ok = ok && checks_equal(restored, expected);
    string_free(wanted_width); string_free(restored); string_free(key); string_free(markup);
    xml_free(document);
    return ok;
}

static void test_benchmarks_complex_inputs(void)
{
    string_t *root = test_checks_root(), *out = NULL;
    bool ok = benchmark_test_stats_fixture(root);
    sqlite_t *db = benchmark_test_open(root);
    /* Saved operands deliberately differ from the live catalogue; reporting must describe this run. */
    ok = db && sqlite_exec_cstr(db,
        "INSERT INTO benchmark_result(run_id, operation, backend, bits, arguments, complex_mask, sequence,\n"
        "                             outcome, median_ns)\n"
        "VALUES (1, 'sin', 'qcomplex', 106, '-0.25', 1, 8, 'pass', 3000),\n"
        "       (1, 'bessel_j', 'qcomplex', 106, '2,0.7', 2, 9, 'pass', 1000),\n"
        "       (1, 'bessel_j', 'mpc', 256, '2,0.7', 2, 10, 'pass', 2000),\n"
        "       (1, 'bessel_j', 'mpc', 512, '2,0.7', 0, 11, 'pass', 4000),\n"
        "       (1, 'bessel_j', 'qfloat', 106, '2,0.7', 2, 12, 'pass', 5000),\n"
        "       (1, 'appell_f1', 'qcomplex', 106, '0.5,0.7,0.8,2.5,0.2,0.3', 63, 13, 'pass', 6000),\n"
        "       (1, 'polygamma', 'qcomplex', 106, '3,2.345', 2, 14, 'pass', 7000),\n"
        "       (1, 'sgn', 'qcomplex', 106, '-0.7', 0, 15, 'pass', 8000);") && ok;
    sqlite_close(db);
    const char *report[] = {"report", "1"};
    ok = ok && benchmark_test_command(root, report, 2, 0, &out) &&
        string_find(out, "| Operation | Inputs |") >= 0 && string_find(out, "Real input literals") < 0 &&
        string_find(out, "| sin | -0.25+0.125i | 3.0 µs |") >= 0 &&
        string_find(out, "| bessel_j | 2, 0.7+0.125i | 1.0 µs |") >= 0 &&
        string_find(out, "| bessel_j | 2, 0.7+0.125i | 2.0 µs | — |") >= 0 &&
        string_find(out, "| bessel_j | 2,0.7 | — | 4.0 µs |") >= 0 &&
        string_find(out, "| bessel_j | 2,0.7 | 5.0 µs |") >= 0 &&
        string_find(out, "| polygamma | 3, 2.345+0.125i | 7.0 µs |") >= 0 &&
        string_find(out, "| sgn | -0.7 | 8.0 µs |") >= 0 &&
        benchmark_test_wrapped_inputs(out, "| appell_f1 | ",
            "0.5+0.125i, 0.7+0.125i, 0.8+0.125i, 2.5+0.125i, 0.2+0.125i, 0.3+0.125i", 40);
    string_free(out); out = NULL;
    const char *formats[] = {"--format=grid", "--format=markdown", "--format=tsv"};
    const char *environment[] = {"NO_COLOR=1", "COLUMNS=132"};
    const char *views[] = {"--summary", "--details"};
    for (size_t v = 0; ok && v < sizeof views / sizeof *views; ++v) {
        for (size_t i = 0; ok && i < sizeof formats / sizeof *formats; ++i) {
            const char *stats[] = {"stats", "--run", "1", "--backend", "qcomplex", "--operation", "bessel_j",
                                   views[v], formats[i]};
            ok = benchmark_test_environment(root, stats, 9, 0, &out, environment, 2, NULL) &&
                string_find(out, "2, 0.7+0.125i") >= 0 && string_find(out, "2+0.125i") < 0;
            string_free(out); out = NULL;
        }
    }
    const char *long_arguments = "0.5+0.125i, 0.7+0.125i, 0.8+0.125i, 2.5+0.125i, 0.2+0.125i, 0.3+0.125i";
    const char *wide[] = {"stats", "--run", "1", "--backend", "qcomplex", "--operation", "appell_f1"};
    const char *narrow[] = {"NO_COLOR=1", "COLUMNS=60"};
    const char *markdown[] = {"stats", "--run", "1", "--backend", "qcomplex", "--operation", "appell_f1",
                              "--format=markdown"};
    ok = ok && benchmark_test_command(root, markdown, 8, 0, &out) &&
        benchmark_test_wrapped_inputs(out, "| appell\\_f1 | ", long_arguments, 40);
    string_free(out); out = NULL;
    ok = ok && benchmark_test_environment(root, wide, 7, 0, &out, environment, 2, NULL) &&
        string_find(out, "╠") >= 0 && benchmark_test_grid_width(out, 132) &&
        benchmark_test_column_contains(out, 1, long_arguments);
    string_free(out); out = NULL;
    ok = ok && benchmark_test_environment(root, wide, 7, 0, &out, narrow, 2, NULL) &&
        benchmark_test_grid_width(out, 60) && benchmark_test_column_contains(out, 1, long_arguments) &&
        string_find(out, "calls/batch:") < 0;
    string_free(out); out = NULL;
    db = benchmark_test_open(root);
    ok = db && benchmark_test_integer(db,
        "SELECT count(*) FROM benchmark_result\n"
        "WHERE run_id = 1 AND operation = 'bessel_j' AND arguments = '2,0.7'\n"
        "  AND complex_mask = 2 AND median_ns IN (1000, 2000, 5000)") == 3 && ok;
    sqlite_close(db);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "reports show saved complex operands, keep integer orders real and preserve measurements");
}

static void test_benchmarks_integer_inputs(void)
{
    string_t *root = test_checks_root(), *out = NULL, *wide_arguments = NULL;
    const char *sync[] = {"sync"};
    bool ok = benchmark_test_fixture(root) && benchmark_test_command(root, sync, 1, 0, NULL);
    sqlite_t *db = benchmark_test_open(root);
    sqlite_stmt_t *query = db ? sqlite_stmt_prepare(db,
        "SELECT arguments FROM benchmark_operation WHERE name = ?1") : NULL;
    const char *names[] = {"bit_length", "gcd", "is_prime", "factors"};
    const size_t widths[] = {1024, 1024, 521, 1215};
    ok = ok && query;
    for (size_t i = 0; ok && i < sizeof names / sizeof *names; ++i) {
        ok = sqlite_stmt_bind_text(query, 1, names[i]) && sqlite_stmt_step(query) == SQLITE_STEP_ROW;
        const char *arguments = ok ? sqlite_stmt_column_text(query, 0) : "";
        if (ok && i == 1) wide_arguments = string_new_with(arguments);
        const char *comma = strchr(arguments, ',');
        string_t *text = string_new_with(arguments);
        string_t *first = comma ? checks_slice(text, 0, (size_t)(comma - arguments)) : string_clone(text);
        number_t value = num_create_from_text(first);
        ok = num_is_exact(value) && num_is_integer(value) && num_bit_length(value) == widths[i] && ok;
        if (i == 1) ok = comma != NULL && ok;
        if (ok && i == 1) {
            number_t other = num_create_from_string(comma + 1);
            number_t gcd = num_gcd(value, other);
            number_t expected = num_create_from_string("340282366920938463463374607431768211454");
            ok = num_bit_length(other) == 512 && num_eq(gcd, expected);
            num_destroy(&expected); num_destroy(&gcd); num_destroy(&other);
        }
        num_destroy(&value);
        string_free(first); string_free(text);
        sqlite_stmt_reset(query);
    }
    sqlite_stmt_finalize(query); sqlite_close(db);
    const char *run[] = {"run", "--backend", "exact-integer", "--samples", "3", "--timeout", "3"};
    ok = ok && benchmark_test_command(root, run, 7, 0, NULL);
    db = benchmark_test_open(root);
    ok = db && benchmark_test_integer(db,
        "SELECT count(*) FROM benchmark_result WHERE outcome = 'pass'") == 35 &&
        benchmark_test_integer(db, "SELECT count(*) FROM benchmark_sample") == 105 &&
        benchmark_test_integer(db,
            "SELECT count(*) FROM benchmark_result AS r JOIN benchmark_operation AS o ON o.name = r.operation\n"
            "LEFT JOIN benchmark_workload AS w ON w.operation = r.operation AND w.backend = r.backend\n"
            "WHERE r.arguments <> coalesce(w.arguments, o.arguments)\n"
            "   OR r.outcome IN ('fail', 'timeout', 'pending')") == 0 && ok;
    sqlite_close(db);
    const char *report[] = {"report", "1"};
    ok = ok && benchmark_test_command(root, report, 2, 0, &out) &&
        string_find(out, "| factorial | 5000 |") >= 0 &&
        string_find(out, "| fibonacci | 10000 |") >= 0 &&
        string_find(out, "| partition | 5000 |") >= 0 &&
        wide_arguments && benchmark_test_wrapped_inputs(out, "| gcd | ", string_c_str(wide_arguments), 80) &&
        string_find(out, ", <br/>") >= 0 &&
        string_find(out, "| gcd | 17, 5 |") < 0;
    string_free(out); string_free(wide_arguments);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "integer workloads retain multi-limb inputs, a non-trivial GCD and checked exact results");
}

static void test_benchmarks_fraction_inputs(void)
{
    string_t *root = test_checks_root(), *out = NULL;
    const char *sync[] = {"sync"};
    bool ok = benchmark_test_fixture(root) && benchmark_test_command(root, sync, 1, 0, NULL);
    sqlite_t *db = benchmark_test_open(root);
    sqlite_stmt_t *rows = db ? sqlite_stmt_prepare(db,
        "SELECT arguments FROM benchmark_workload\n"
        "WHERE backend = 'exact-fraction' ORDER BY operation") : NULL;
    size_t saved = num_get_default_prec_bits(), count = 0;
    ok = ok && rows && num_set_default_prec_bits(16384) == 0;
    sqlite_step_result_t step = SQLITE_STEP_ERROR;
    while (ok && (step = sqlite_stmt_step(rows)) == SQLITE_STEP_ROW) {
        string_t *arguments = string_new_with(sqlite_stmt_column_text(rows, 0));
        size_t parts_count = 0;
        string_t **parts = arguments ? string_split(arguments, ",", &parts_count) : NULL;
        number_t value = parts_count ? num_create_from_text(parts[0]) : NUM_NAN;
        number_t numerator = {0}, denominator = {0};
        ok = parts && num_is_exact(value) && !num_is_integer(value) &&
             num_get_fraction_parts(value, &numerator, &denominator) &&
             num_bit_length(numerator) == 1024 && num_bit_length(denominator) == 521;
        num_destroy(&numerator); num_destroy(&denominator); num_destroy(&value);
        if (ok && parts_count == 2) {
            string_trim(parts[1]);
            value = num_create_from_text(parts[1]);
            if (num_is_integer(value)) {
                number_t five = num_create_from_long(5);
                ok = num_eq(value, five);
                num_destroy(&five);
            } else {
                ok = num_get_fraction_parts(value, &numerator, &denominator) &&
                     num_bit_length(numerator) == 384 && num_bit_length(denominator) == 896;
            }
            num_destroy(&numerator); num_destroy(&denominator); num_destroy(&value);
        }
        string_split_free(parts, parts_count); string_free(arguments);
        ++count;
    }
    ok = ok && step == SQLITE_STEP_DONE && count == 10;
    num_set_default_prec_bits(saved);
    sqlite_stmt_finalize(rows); sqlite_close(db);
    const char *run[] = {"run", "--backend", "exact-fraction", "--samples", "3", "--timeout", "3"};
    ok = ok && benchmark_test_command(root, run, 7, 0, NULL);
    db = benchmark_test_open(root);
    ok = db && benchmark_test_integer(db,
        "SELECT count(*) FROM benchmark_result WHERE outcome = 'pass'") == 10 &&
        benchmark_test_integer(db, "SELECT count(*) FROM benchmark_sample") == 30 &&
        benchmark_test_integer(db,
            "SELECT count(*) FROM benchmark_result AS r\n"
            "LEFT JOIN benchmark_workload AS w ON w.operation = r.operation AND w.backend = r.backend\n"
            "WHERE r.outcome = 'pass' AND (r.arguments <> w.arguments OR r.bits <> 0 OR r.complex_mask <> 0)") == 0 && ok;
    sqlite_close(db);
    const char *stats[] = {"stats", "--backend", "exact-fraction", "--format=markdown"};
    ok = ok && benchmark_test_command(root, stats, 4, 0, &out) &&
         string_find(out, "number / unrestricted fraction — exact fractions") >= 0 &&
         string_find(out, "width:80ch") >= 0;
    string_free(out); out = NULL;
    const char *report[] = {"report", "1"};
    ok = ok && benchmark_test_command(root, report, 2, 0, &out) &&
         string_find(out, "### number / unrestricted fraction\n") >= 0 &&
         string_find(out, "| add |") >= 0 && string_find(out, "| div |") >= 0 &&
         string_find(out, "| inv |") >= 0 && string_find(out, "| mul |") >= 0;
    string_free(out);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "fraction arithmetic times large exact operands and reports the actual persisted workload");
}

static void test_benchmarks_measurements(void)
{
    string_t *root = test_checks_root();
    const char *run[] = {"run", "--operation", "sin", "--samples", "3", "--timeout", "3"};
    bool ok = benchmark_test_fixture(root) && benchmark_test_command(root, run, 7, 0, NULL);
    sqlite_t *db = benchmark_test_open(root);
    ok = db && benchmark_test_integer(db, "SELECT count(*) FROM benchmark_result WHERE outcome = 'pass'") == 15 &&
        benchmark_test_integer(db, "SELECT count(*) FROM benchmark_sample") == 45 &&
        benchmark_test_integer(db,
            "SELECT count(*) FROM benchmark_result WHERE median_ns > 0 AND minimum_ns <= median_ns "
            "AND maximum_ns >= median_ns AND iterations > 0 AND resolution_ns > 0") == 15 &&
        benchmark_test_integer(db,
            "SELECT length(library_sha256) + length(executable_sha256) FROM benchmark_run WHERE id = 1") == 128 && ok;
    ok = ok && benchmark_test_integer(db,
        "SELECT count(*) FROM benchmark_run WHERE clock LIKE '%warmup=100ms; target_batch=5ms%' AND id = 1") == 1;
    ok = ok && benchmark_test_integer(db,
        "SELECT max(iterations) FROM benchmark_result WHERE run_id = 1 AND backend = 'number-double'") > 512;
    /* Recreate an interrupted checkpoint and prove resume touches only its one pending case. */
    ok = ok && sqlite_exec_cstr(db,
        "DELETE FROM benchmark_sample WHERE backend = 'qfloat'; "
        "UPDATE benchmark_result SET outcome = 'pending' WHERE backend = 'qfloat'; "
        "UPDATE benchmark_run SET status = 'interrupted'");
    sqlite_close(db);
    const char *resume[] = {"run", "--resume", "1"};
    ok = ok && benchmark_test_command(root, resume, 3, 0, NULL);
    db = benchmark_test_open(root);
    ok = db && benchmark_test_integer(db, "SELECT count(*) FROM benchmark_sample") == 45 &&
         benchmark_test_integer(db, "SELECT count(*) FROM benchmark_run WHERE status = 'complete'") == 1 && ok;
    ok = ok && sqlite_exec_cstr(db, "UPDATE benchmark_run SET clock = 'legacy timing protocol' WHERE id = 1");
    sqlite_close(db);
    const char *unsupported[] = {"run", "--operation", "factorial", "--backend", "qfloat", "--samples", "3"};
    ok = ok && benchmark_test_command(root, unsupported, 7, 0, NULL);
    db = benchmark_test_open(root);
    ok = db && benchmark_test_integer(db,
        "SELECT count(*) FROM benchmark_result WHERE operation = 'factorial' AND outcome = 'unsupported'") == 1 && ok;
    ok = ok && benchmark_test_integer(db,
        "SELECT count(*) FROM benchmark_result WHERE outcome = 'unsupported' AND median_ns IS NOT NULL") == 0;
    sqlite_close(db);
    const char *mixed[] = {"run", "--operation", "add_long", "--backend", "number-double", "--samples", "3"};
    ok = ok && benchmark_test_command(root, mixed, 7, 0, NULL);
    db = benchmark_test_open(root);
    ok = db && benchmark_test_integer(db,
        "SELECT count(*) FROM benchmark_result WHERE operation = 'add_long' AND outcome = 'pass'") == 1 && ok;
    sqlite_close(db);
    const char *repeat[] = {"run", "--operation", "sin", "--backend", "qfloat", "--samples", "3"};
    ok = ok && benchmark_test_command(root, repeat, 7, 0, NULL);
    db = benchmark_test_open(root);
    ok = db && benchmark_test_integer(db,
        "SELECT count(*) FROM benchmark_result WHERE run_id = 4 AND baseline_median_ns IS NULL") == 1 && ok;
    sqlite_close(db);
    ok = ok && benchmark_test_command(root, repeat, 7, 0, NULL);
    db = benchmark_test_open(root);
    ok = db && benchmark_test_integer(db,
        "SELECT count(*) FROM benchmark_result WHERE run_id = 5 AND baseline_median_ns > 0") == 1 && ok;
    sqlite_close(db);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "all numeric profiles yield finite timed samples and interrupted runs resume without duplicate data");
}

static void test_benchmarks_progress_capture(void)
{
    string_t *root = test_checks_root(), *errors = NULL;
    const char *run[] = {"run", "--operation", "sin", "--backend", "qfloat", "--samples", "3"};
    bool ok = benchmark_test_fixture(root) &&
        benchmark_test_environment(root, run, 7, 0, NULL, NULL, 0, &errors);
    ok = ok && string_find(errors, "Benchmarks 0/1 (0%)") >= 0 &&
        string_find(errors, "Benchmarks complete: 1/1; PASS 1  FAIL 0  TIMEOUT 0  SKIP 0") >= 0 &&
        string_find(errors, "[1/1]") < 0 && string_find(errors, "\033") < 0 && string_find(errors, "\r") < 0;
    string_free(errors); errors = NULL;
    const char *verbose[] = {"run", "--verbose", "--operation", "sin", "--backend", "qfloat", "--samples", "3"};
    ok = ok && benchmark_test_environment(root, verbose, 8, 0, NULL, NULL, 0, &errors) &&
        string_find(errors, "[1/1] sin / qfloat / 106 bits ... PASS") >= 0;
    string_free(errors); errors = NULL;
    sqlite_t *db = benchmark_test_open(root);
    ok = db && sqlite_exec_cstr(db,
        "DELETE FROM benchmark_sample WHERE run_id = 1;\n"
        "UPDATE benchmark_result SET outcome = 'pending' WHERE run_id = 1;\n"
        "UPDATE benchmark_run SET status = 'interrupted' WHERE id = 1;\n"
        "INSERT INTO benchmark_result(run_id, operation, backend, bits, arguments, complex_mask, sequence, outcome)\n"
        "VALUES (1, 'acos', 'qfloat', 106, '0.7', 0, 2, 'fail'),\n"
        "       (1, 'cos', 'qfloat', 106, '0.7', 0, 3, 'timeout'),\n"
        "       (1, 'tan', 'qfloat', 106, '0.7', 0, 4, 'unsupported');\n"
        "INSERT INTO benchmark_operation VALUES ('fixture_unavailable', 1, '1', 0, 0, 0);") && ok;
    sqlite_close(db);
    const char *resume[] = {"run", "--resume", "1"};
    ok = ok && benchmark_test_environment(root, resume, 3, 1, NULL, NULL, 0, &errors) &&
        string_find(errors, "Benchmarks 3/4 (75%)") >= 0 &&
        string_find(errors, "Benchmarks complete: 4/4; PASS 1  FAIL 1  TIMEOUT 1  SKIP 1") >= 0;
    string_free(errors); errors = NULL;
    const char *failure[] = {"run", "--operation", "fixture_unavailable", "--backend", "number-double", "--samples", "3"};
    ok = ok && benchmark_test_environment(root, failure, 7, 1, NULL, NULL, 0, &errors) &&
        string_find(errors, "FAIL: fixture_unavailable / number-double / 53 bits") >= 0 &&
        string_find(errors, "Benchmarks complete: 1/1; PASS 0  FAIL 1  TIMEOUT 0  SKIP 0") >= 0;
    string_free(errors);
    db = benchmark_test_open(root);
    ok = db && benchmark_test_integer(db, "SELECT count(*) FROM benchmark_sample WHERE run_id = 1") == 3 &&
        benchmark_test_integer(db, "SELECT count(*) FROM benchmark_result WHERE run_id = 1 AND outcome != 'pending'") == 4 && ok;
    sqlite_close(db);
    const char *exact[] = {"run", "--operation", "sin", "--backend", "exact-integer", "--verbose"};
    errors = NULL;
    ok = ok && benchmark_test_environment(root, exact, 6, 0, NULL, NULL, 0, &errors) &&
        string_find(errors, "sin / exact-integer / exact ... SKIPPED") >= 0 && string_find(errors, "0 bits") < 0;
    string_free(errors);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "captured progress stays compact, verbose output is explicit, and resumed/failing counts stay truthful");
}

static bool benchmark_test_terminal(const string_t *root, unsigned columns, bool colour, string_t **captured)
{
    *captured = NULL;
    int master = posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC);
    bool ok = master >= 0 && !grantpt(master) && !unlockpt(master);
    char *slave = ok ? ptsname(master) : NULL;
    fflush(NULL);
    pid_t child = slave ? fork() : -1;
    if (!child) {
        int terminal = open(slave, O_RDWR | O_NOCTTY);
        struct winsize window = {.ws_row = 24, .ws_col = (unsigned short)columns};
        if (terminal < 0 || ioctl(terminal, TIOCSWINSZ, &window) || dup2(terminal, STDERR_FILENO) < 0 ||
            dup2(terminal, STDOUT_FILENO) < 0 || setenv("TERM", "xterm", 1) ||
            setenv("NO_COLOR", colour ? "" : "1", 1) || setenv("MARS_ROOT", string_c_str(root), 1)) _exit(120);
        if (terminal > STDERR_FILENO) close(terminal);
        close(master);
        execl(MARS_CHECKS_PROGRAM_PATH, MARS_CHECKS_PROGRAM_PATH, "bench-db", "run", "--operation", "sin",
              "--samples", "3", "--timeout", "3", (char *)NULL);
        _exit(121);
    }
    char bytes[65536];
    size_t used = 0;
    ok = ok && child > 0;
    while (ok && used < sizeof bytes - 1) {
        struct pollfd readable = {.fd = master, .events = POLLIN};
        int available = poll(&readable, 1, 5000);
        if (available < 0 && errno == EINTR) continue;
        if (available <= 0) { ok = false; break; }
        ssize_t count = read(master, bytes + used, sizeof bytes - used - 1);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { ok = !count || errno == EIO; break; }
        used += (size_t)count;
    }
    if (master >= 0) close(master);
    if (child > 0 && (!ok || used == sizeof bytes - 1)) kill(child, SIGKILL);
    int status = 0;
    pid_t waited;
    do { waited = child > 0 ? waitpid(child, &status, 0) : -1; } while (waited < 0 && errno == EINTR);
    bytes[used] = '\0';
    *captured = string_new_with(bytes);
    return ok && waited == child && WIFEXITED(status) && WEXITSTATUS(status) == 0 && *captured;
}

static void test_benchmarks_progress_terminal(void)
{
    string_t *root = test_checks_root(), *text = NULL;
    bool ok = benchmark_test_fixture(root) && benchmark_test_terminal(root, 80, true, &text);
    ok = ok && string_find(text, "⠋") >= 0 && string_find(text, "⠙") >= 0 && string_find(text, "█") >= 0 &&
        string_find(text, "\033[1A") >= 0 && string_find(text, "\033[2K") >= 0 &&
        string_find(text, "\033[1;92mPASS") >= 0 && string_find(text, "\033[1;93m  SKIP") >= 0 &&
        string_find(text, "elapsed") >= 0 && string_find(text, "case") >= 0 && string_find(text, "3s limit") >= 0 &&
        string_find(text, "Benchmarks complete: 17/17;") >= 0 && string_find(text, "PASS 15") >= 0 &&
        string_find(text, "SKIP 2") >= 0 && string_find(text, "[1/17]") < 0 &&
        string_find(text, "░") < 0 && string_find(text, "▒") < 0 && string_find(text, "▓") < 0;
    if (!ok && text) fprintf(stderr, "Benchmark terminal transcript: %s\n", string_c_str(text));
    string_free(text); text = NULL;
    ok = ok && benchmark_test_terminal(root, 55, false, &text) && string_find(text, "\033[1A") >= 0 &&
        string_find(text, "\033[1;9") < 0 && string_find(text, "Benchmarks complete: 17/17;") >= 0;
    string_free(text);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "terminal progress animates fractional bars and counters without per-case scrolling or forced colour");
}

static void test_benchmarks_api_coverage(void)
{
    string_t *root = test_checks_root(), *repository = checks_text(MARS_CHECKS_ROOT_DIR);
    const char *sync[] = {"sync"};
    bool ok = benchmark_test_fixture(root) && benchmark_test_command(root, sync, 1, 0, NULL);
    string_t *path = checks_path(repository, "include/number-t.h"), *header = checks_read(path);
    const char *begin = header ? strstr(string_c_str(header), "number_t num_neg(") : NULL;
    const char *end = begin ? strstr(begin, "number_t num_E1(") : NULL;
    const char *close = end ? strchr(end, ';') : NULL;
    string_t *region = close ? checks_slice(header, (size_t)(begin - string_c_str(header)),
                                             (size_t)(close - begin + 1)) : NULL;
    checks_strings_t *functions = region ? checks_header_functions(region) : NULL;
    sqlite_t *db = benchmark_test_open(root);
    sqlite_stmt_t *find = db ? sqlite_stmt_prepare(db, "SELECT name FROM benchmark_operation WHERE name = ?1") : NULL;
    ok = ok && functions && find;
    for (size_t i = 0; ok && i < checks_strings_count(functions); ++i) {
        const string_t *name = checks_strings_get(functions, i);
        const char *text = string_c_str(name);
        if (strncmp(text, "num_", 4) || checks_equal(name, "num_factors_free") || string_ends_with(name, "_slow"))
            continue;
        ok = sqlite_stmt_bind_text(find, 1, text + 4) && sqlite_stmt_step(find) == SQLITE_STEP_ROW;
        if (!ok) fprintf(stderr, "Missing mathematical benchmark: %s\n", text);
        sqlite_stmt_reset(find);
    }
    sqlite_stmt_finalize(find); sqlite_close(db);
    checks_strings_free(functions);
    string_free(region); string_free(path); string_free(header); string_free(repository);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "every public numeric mathematical operation has a stable benchmark catalogue entry");
}

static void test_benchmarks_expression_coverage(void)
{
    string_t *root = test_checks_root();
    const char *sync[] = {"sync"};
    bool ok = benchmark_test_fixture(root) && benchmark_test_command(root, sync, 1, 0, NULL);
    sqlite_t *db = ok ? benchmark_test_open(root) : NULL;
    sqlite_stmt_t *rows = db ? sqlite_stmt_prepare(db,
        "SELECT name, arguments FROM benchmark_operation ORDER BY name") : NULL;
    size_t count = 0, saved = num_get_default_prec_bits();
    num_set_default_prec_bits(53);
    sqlite_step_result_t step = SQLITE_STEP_ERROR;
    while (rows && (step = sqlite_stmt_step(rows)) == SQLITE_STEP_ROW) {
        const char *name = sqlite_stmt_column_text(rows, 0);
        bool component = !strcmp(name, "divmod") || !strcmp(name, "gcdext") ||
                         !strcmp(name, "sincos") || !strcmp(name, "sinhcosh");
        string_t *source = string_sprintf("%s(%s%s)", name, sqlite_stmt_column_text(rows, 1),
                                           component ? ",0" : "");
        if (!strcmp(name, "hypergeometric_pFq")) {
            string_free(source);
            source = string_sprintf("hypergeometric_pFq(1,1,%s)", sqlite_stmt_column_text(rows, 1));
        } else if (!strcmp(name, "lauricella_f")) {
            string_t *arguments = string_new_with(sqlite_stmt_column_text(rows, 1));
            checks_strings_t *parts = arguments ? checks_split(arguments, ',') : NULL;
            string_free(source);
            source = parts && checks_strings_count(parts) == 3 ?
                string_sprintf("lauricella_f(1,%s,%s,%s,%s)",
                    string_c_str(checks_strings_get(parts, 0)), string_c_str(checks_strings_get(parts, 1)),
                    string_c_str(checks_strings_get(parts, 2)), string_c_str(checks_strings_get(parts, 1))) : NULL;
            checks_strings_free(parts);
            string_free(arguments);
        }
        expr_bindings_t *bindings = NULL;
        expr_t *expression = source ? expr_from_text(source, &bindings) : NULL;
        bool parsed = expression && expr_bindings_count(bindings) == 0;
        string_t *bound_source = source ? string_sprintf("{[result] | [result]=%s}", string_c_str(source)) : NULL;
        expr_t *bound = bound_source ? expr_from_text(bound_source, NULL) : NULL;
        parsed = parsed && bound;
        if (!parsed)
            fprintf(stderr, "Numerical benchmark is unavailable as an expression: %s\n", string_c_str(source));
        ok = parsed && ok;
        ++count;
        expr_free(expression);
        expr_free(bound);
        string_free(bound_source);
        expr_bindings_free(bindings);
        string_free(source);
    }
    num_set_default_prec_bits(saved);
    ok = ok && rows && step == SQLITE_STEP_DONE && count > 150;
    sqlite_stmt_finalize(rows);
    sqlite_close(db);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "every benchmarked mathematical operation is callable through the native expression parser");
}

static void test_benchmarks_documentation(void)
{
    string_t *root = test_checks_root();
    const char *run[] = {"run", "--operation", "sin", "--samples", "3", "--timeout", "3"};
    const char *update[] = {"update-docs", "1"}, *report[] = {"report", "1"};
    const char *paths[] = {"docs/benchmarks.md", "docs/number.md", "docs/qfloat.md", "docs/qcomplex.md"};
    const char *original = "unchanged prefix\n<!-- MARS-BENCHMARKS:BEGIN -->\nold\n"
                           "<!-- MARS-BENCHMARKS:END -->\nunchanged suffix\n";
    bool ok = benchmark_test_fixture(root);
    for (unsigned i = 0; ok && i < 4; ++i) ok = test_checks_write(root, paths[i], original);
    ok = ok && benchmark_test_command(root, run, 7, 0, NULL);
    const char *automatic[] = {"update-docs"};
    ok = ok && benchmark_test_command(root, automatic, 1, 1, NULL);
    string_t *out = NULL;
    ok = ok && benchmark_test_command(root, report, 2, 0, &out) &&
        string_find(out, "qcomplex") >= 0 && string_find(out, "4096 bits") >= 0 &&
        string_find(out, "### `exact-integer`") < 0 && string_find(out, "| unsupported |") < 0;
    string_free(out);
    sqlite_t *db = benchmark_test_open(root);
    ok = db && sqlite_exec_cstr(db,
        "INSERT INTO benchmark_result(run_id, operation, backend, bits, arguments, complex_mask, sequence,\n"
        "                             outcome, median_ns)\n"
        "VALUES (1, 'gcd', 'exact-integer', 0, '17, 5', 0, 17, 'pass', 1000);") && ok;
    sqlite_close(db);
    /* Preflight every document before publication; a broken final marker preserves all originals. */
    ok = ok && test_checks_write(root, paths[3], "missing markers\n") &&
         benchmark_test_command(root, update, 2, 1, NULL);
    string_t *path = checks_path(root, paths[0]), *source = checks_read(path);
    ok = source && checks_equal(source, original) && ok;
    string_free(path); string_free(source);
    ok = ok && test_checks_write(root, paths[3], original) && benchmark_test_command(root, update, 2, 0, NULL);
    for (unsigned i = 0; ok && i < 4; ++i) {
        path = checks_path(root, paths[i]); source = checks_read(path);
        ok = source && string_find(source, "Benchmark run **1**") >= 0 &&
             string_find(source, "unchanged prefix") == 0 && string_find(source, "unchanged suffix") >= 0;
        if (ok && i == 1) {
            ok = string_find(source, "### number / double precision\n") >= 0 &&
                string_find(source, "### number / multi-precision floating point\n") >= 0 &&
                string_find(source, "### number / multi-precision complex floating point\n") >= 0 &&
                string_find(source, "### number / unrestricted integer\n") >= 0 &&
                string_find(source, "### number / unrestricted fraction\n") >= 0 &&
                string_find(source, "GMP rational arithmetic (MPQ)") >= 0 &&
                string_find(source, "16,384-bit") >= 0 &&
                string_find(source, "### `number-double`") < 0 && string_find(source, "### `mpfr`") < 0 &&
                string_find(source, "### `mpc`") < 0 && string_find(source, "### `exact-integer`") < 0 &&
                string_find(source, "17, <br/>5</span> | 1.0 µs |") >= 0 &&
                benchmark_test_wrapped_inputs(source, "| gcd | ", "17, 5", 80);
            if (!ok) fprintf(stderr, "Generated number benchmark document:\n%s\n", string_c_str(source));
        }
        string_free(path); string_free(source);
    }
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "Markdown tables use one persisted run, preserve surrounding prose and preflight all markers");
}

static void test_benchmarks_documentation_supported_rows(void)
{
    string_t *root = test_checks_root(), *out = NULL;
    bool ok = benchmark_test_stats_fixture(root);
    sqlite_t *db = benchmark_test_open(root);
    ok = db && sqlite_exec_cstr(db,
        "INSERT INTO benchmark_result(run_id, operation, backend, bits, arguments, complex_mask, sequence, outcome,\n"
        "                             median_ns)\n"
        "VALUES (1, 'sin', 'mpfr', 256, '0.7', 0, 8, 'pass', 2000),\n"
        "       (1, 'sin', 'mpfr', 512, '0.7', 0, 9, 'unsupported', NULL),\n"
        "       (1, 'cos', 'mpfr', 256, '0.7', 0, 10, 'unsupported', NULL),\n"
        "       (1, 'cos', 'mpfr', 512, '0.7', 0, 11, 'pass', 3000),\n"
        "       (1, 'asin', 'mpfr', 256, '0.7', 0, 12, 'unsupported', NULL),\n"
        "       (1, 'asin', 'mpfr', 512, '0.7', 0, 13, 'unsupported', NULL),\n"
        "       (1, 'sin', 'mpfr', 4096, '0.7', 0, 14, 'unsupported', NULL),\n"
        "       (1, 'sin', 'qcomplex', 106, '0.7', 1, 15, 'unsupported', NULL);") && ok;
    sqlite_close(db);
    const char *report[] = {"report", "1"};
    ok = ok && benchmark_test_command(root, report, 2, 0, &out) &&
        string_find(out, "| asin |") < 0 && string_find(out, "### `qcomplex`") < 0 &&
        string_find(out, "4096 bits") < 0 && string_find(out, "| unsupported |") < 0 &&
        string_find(out, "| sin | 0.7 | 2.0 µs | — |") >= 0 &&
        string_find(out, "| cos | 0.7 | — | 3.0 µs |") >= 0 &&
        string_find(out, "| acos | 0.7 | fail |") >= 0 && string_find(out, "| atanh | 0.7 | timeout |") >= 0 &&
        string_find(out, "| exp | 0.7 | pending |") >= 0 &&
        string_find(out, "| hypergeometric_pFq | 2,1,0.7 | 4.0 µs † |") >= 0;
    string_free(out);
    db = benchmark_test_open(root);
    ok = db && benchmark_test_integer(db, "SELECT count(*) FROM benchmark_result") == 16 &&
        benchmark_test_integer(db, "SELECT count(*) FROM benchmark_result WHERE outcome = 'unsupported'") == 7 &&
        benchmark_test_integer(db, "SELECT count(*) FROM benchmark_result WHERE timing_outlier = 1") == 1 && ok;
    sqlite_close(db);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "Markdown omits unsupported-only entries, retains mixed timings and leaves stored outcomes intact");
}

static bool benchmark_test_retention_fixture(const string_t *root, unsigned count, unsigned padding)
{
    const char *sync[] = {"sync"};
    if (!benchmark_test_fixture(root) || !benchmark_test_command(root, sync, 1, 0, NULL)) return false;
    sqlite_t *db = benchmark_test_open(root);
    string_t *sql = string_sprintf(
        "WITH RECURSIVE history(id) AS (SELECT 1 UNION ALL SELECT id + 1 FROM history WHERE id < %u)\n"
        "INSERT INTO benchmark_run(id, started_utc, finished_utc, status, machine, cpu, system, compiler, build_flags,\n"
        "                          library_sha256, executable_sha256, requested_samples, case_timeout_seconds)\n"
        "SELECT id, datetime('now', CASE WHEN id <= 2 THEN '-100 days' ELSE '-1 day' END),\n"
        "       datetime('now', CASE WHEN id <= 2 THEN '-100 days' ELSE '-1 day' END),\n"
        "       'complete', 'fixture', 'fixture CPU', 'fixture OS', 'fixture CC', '-O2', 'library', 'binary', 3, 3\n"
        "FROM history;\n"
        "INSERT INTO benchmark_result(run_id, operation, backend, bits, arguments, complex_mask, sequence, outcome,\n"
        "                             median_ns, mean_ns, minimum_ns, maximum_ns, sample_count, timing_outlier,\n"
        "                             baseline_median_ns, detail)\n"
        "SELECT id, 'sin', 'qfloat', 106, '0.7', 0, 1, 'pass', 1500, 1500, 1000, 2000, 3, id = %u, 1000,\n"
        "       hex(zeroblob(%u)) FROM benchmark_run;\n"
        "INSERT INTO benchmark_sample(run_id, operation, backend, bits, ordinal, nanoseconds_per_call)\n"
        "SELECT id, 'sin', 'qfloat', 106, ordinal, 1000 + ordinal * 500\n"
        "FROM benchmark_run CROSS JOIN (SELECT 0 AS ordinal UNION ALL SELECT 1 UNION ALL SELECT 2);",
        count, count - 1, padding);
    bool ok = db && sql && sqlite_exec(db, sql);
    string_free(sql);
    sqlite_close(db);
    return ok;
}

static void test_benchmarks_retention_policy(void)
{
    string_t *root = test_checks_root(), *out = NULL;
    const char *show[] = {"retention"}, *sync[] = {"sync"};
    bool ok = benchmark_test_fixture(root) && benchmark_test_command(root, show, 1, 1, NULL) &&
        benchmark_test_command(root, sync, 1, 0, NULL) && benchmark_test_command(root, show, 1, 0, &out);
    ok = ok && string_find(out, "Database cap: 32 MiB") >= 0 && string_find(out, "Recent runs: 20") >= 0 &&
        string_find(out, "Maximum age: 90 days") >= 0 && string_find(out, "Raw samples: newest 5 runs") >= 0;
    string_free(out);
    const char *configure[] = {"retention", "--max-mib", "2", "--keep-runs", "3", "--max-days", "30"};
    ok = ok && benchmark_test_command(root, configure, 7, 0, NULL) && benchmark_test_command(root, sync, 1, 0, NULL);
    const char *invalid[][3] = {
        {"retention", "--max-mib", "0"}, {"retention", "--max-mib", "9999999999999999999999"},
        {"retention", "--raw-runs", "4"}, {"retention", "--max-days", "-1"},
        {"retention", "--unknown", "1"}, {"retention", "--keep-runs", ""}
    };
    for (size_t i = 0; ok && i < sizeof invalid / sizeof *invalid; ++i)
        ok = benchmark_test_command(root, invalid[i], 3, 2, NULL);
    sqlite_t *db = benchmark_test_open(root);
    ok = db && benchmark_test_integer(db,
        "SELECT count(*) FROM benchmark_retention WHERE max_mib=2 AND keep_runs=3 AND max_days=30 AND raw_runs=3") == 1 &&
        benchmark_test_integer(db, "PRAGMA user_version") == 3 && ok;
    sqlite_close(db);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok,
        "retention is configurable, validates bounds, persists across sync and never creates storage on display");
}

static void test_benchmarks_retention_expiry(void)
{
    string_t *root = test_checks_root();
    bool ok = benchmark_test_retention_fixture(root, 10, 0);
    const char *age[] = {"retention", "--max-days", "30", "--raw-runs", "2"};
    ok = ok && benchmark_test_command(root, age, 5, 0, NULL);
    sqlite_t *db = benchmark_test_open(root);
    ok = db && benchmark_test_integer(db, "SELECT count(*) FROM benchmark_run") == 8 &&
        benchmark_test_integer(db, "SELECT min(id) FROM benchmark_run") == 3 &&
        benchmark_test_integer(db, "SELECT count(*) FROM benchmark_sample") == 6 && ok;
    sqlite_close(db);
    const char *count[] = {"retention", "--keep-runs", "4"};
    ok = ok && benchmark_test_command(root, count, 3, 0, NULL);
    db = benchmark_test_open(root);
    ok = db && benchmark_test_integer(db, "SELECT count(*) FROM benchmark_run") == 4 &&
        benchmark_test_integer(db, "SELECT min(id) FROM benchmark_run") == 7 &&
        benchmark_test_integer(db, "SELECT count(*) FROM benchmark_sample WHERE run_id < 9") == 0 &&
        benchmark_test_integer(db, "SELECT count(*) FROM benchmark_result WHERE sample_count=3 AND median_ns=1500") == 4 &&
        benchmark_test_integer(db, "SELECT timing_outlier FROM benchmark_result WHERE run_id=9") == 1 &&
        benchmark_test_integer(db, "SELECT baseline_median_ns FROM benchmark_result WHERE run_id=9") == 1000 &&
        benchmark_test_integer(db, "SELECT count(*) FROM pragma_foreign_key_check") == 0 && ok;
    sqlite_close(db);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "age and count expiry remove complete runs; older summaries preserve timing and outlier evidence");
}

static void test_benchmarks_retention_rollback(void)
{
    string_t *root = test_checks_root();
    bool ok = benchmark_test_retention_fixture(root, 4, 0);
    sqlite_t *db = benchmark_test_open(root);
    ok = db && sqlite_exec_cstr(db,
        "CREATE TRIGGER reject_retention BEFORE DELETE ON benchmark_run BEGIN\n"
        "    SELECT RAISE(ABORT, 'deliberate retention rollback');\n"
        "END;") && ok;
    sqlite_close(db);
    const char *configure[] = {"retention", "--keep-runs", "1", "--raw-runs", "0"};
    ok = ok && benchmark_test_command(root, configure, 5, 1, NULL);
    db = benchmark_test_open(root);
    ok = db && benchmark_test_integer(db, "SELECT count(*) FROM benchmark_run") == 4 &&
        benchmark_test_integer(db, "SELECT count(*) FROM benchmark_result") == 4 &&
        benchmark_test_integer(db, "SELECT count(*) FROM benchmark_sample") == 12 &&
        benchmark_test_integer(db, "SELECT keep_runs FROM benchmark_retention") == 20 &&
        benchmark_test_integer(db, "SELECT timing_outlier FROM benchmark_result WHERE run_id=3") == 1 && ok;
    sqlite_close(db);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "a failed deletion rolls back the policy, raw samples, summaries and outlier flags together");
}

static void test_benchmarks_retention_size(void)
{
    string_t *root = test_checks_root();
    bool ok = benchmark_test_retention_fixture(root, 8, 85000);
    const char *configure[] = {"retention", "--max-mib", "1", "--max-days", "365"};
    ok = ok && benchmark_test_command(root, configure, 5, 0, NULL);
    sqlite_t *db = benchmark_test_open(root);
    long runs = db ? benchmark_test_integer(db, "SELECT count(*) FROM benchmark_run") : -1;
    ok = db && runs > 0 && runs < 8 &&
        benchmark_test_integer(db, "SELECT count(*) FROM benchmark_run WHERE id=8") == 1 &&
        benchmark_test_integer(db, "SELECT count(*) FROM benchmark_sample") == 0 &&
        benchmark_test_integer(db, "SELECT page_count * page_size FROM pragma_page_count(), pragma_page_size()") <= 1048576 &&
        benchmark_test_integer(db, "SELECT count(*) FROM pragma_foreign_key_check") == 0 && ok;
    sqlite_close(db);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "storage pressure drops older samples before summaries and compacts the file below its cap");
}

static void test_benchmarks_retention_capacity(void)
{
    string_t *root = test_checks_root();
    const char *sync[] = {"sync"}, *configure[] = {"retention", "--max-mib", "1"}, *run[] = {"run"};
    bool ok = benchmark_test_fixture(root) && benchmark_test_command(root, sync, 1, 0, NULL) &&
        benchmark_test_command(root, configure, 3, 0, NULL);
    sqlite_t *db = benchmark_test_open(root);
    ok = db && sqlite_exec_cstr(db,
        "WITH RECURSIVE cases(id) AS (SELECT 1 UNION ALL SELECT id + 1 FROM cases WHERE id < 300)\n"
        "INSERT INTO benchmark_operation\n"
        "SELECT printf('retained_case_%04d', id), 1, hex(zeroblob(200)), 0, 0, 0 FROM cases") && ok;
    sqlite_close(db);
    ok = ok && benchmark_test_command(root, run, 1, 1, NULL);
    db = benchmark_test_open(root);
    ok = db && benchmark_test_integer(db, "SELECT count(*) FROM benchmark_run") == 0 &&
        benchmark_test_integer(db, "SELECT count(*) FROM benchmark_result") == 0 &&
        benchmark_test_integer(db, "SELECT count(*) FROM benchmark_operation WHERE name LIKE 'retained_case_%'") == 300 &&
        benchmark_test_integer(db, "SELECT page_count * page_size FROM pragma_page_count(), pragma_page_size()") <= 1048576 && ok;
    sqlite_close(db);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "SQLite rejects an oversized run atomically rather than growing beyond the configured file cap");
}

static void test_benchmarks_retention_sample_pressure(void)
{
    string_t *root = test_checks_root();
    bool ok = benchmark_test_retention_fixture(root, 8, 0);
    sqlite_t *db = benchmark_test_open(root);
    ok = db && sqlite_exec_cstr(db,
        "DELETE FROM benchmark_sample;\n"
        "UPDATE benchmark_result SET sample_count=6000;\n"
        "WITH RECURSIVE samples(ordinal) AS (SELECT 0 UNION ALL SELECT ordinal+1 FROM samples WHERE ordinal<5999)\n"
        "INSERT INTO benchmark_sample\n"
        "SELECT id, 'sin', 'qfloat', 106, ordinal, 1500 FROM benchmark_run CROSS JOIN samples;") && ok;
    sqlite_close(db);
    const char *configure[] = {"retention", "--max-mib", "1", "--max-days", "365", "--raw-runs", "8"};
    ok = ok && benchmark_test_command(root, configure, 7, 0, NULL);
    db = benchmark_test_open(root);
    long samples = db ? benchmark_test_integer(db, "SELECT count(*) FROM benchmark_sample") : -1;
    ok = db && samples > 0 && samples < 48000 &&
        benchmark_test_integer(db, "SELECT count(*) FROM benchmark_run") == 8 &&
        benchmark_test_integer(db, "SELECT min(run_id) FROM benchmark_sample") > 1 &&
        benchmark_test_integer(db, "SELECT count(*) FROM benchmark_sample WHERE run_id=8") == 6000 &&
        benchmark_test_integer(db, "SELECT page_count * page_size FROM pragma_page_count(), pragma_page_size()") <= 1048576 && ok;
    sqlite_close(db);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "capacity cleanup keeps the newest raw batches and every summary when removing older batches suffices");
}

static void test_benchmarks_retention_sequence(void)
{
    string_t *root = test_checks_root();
    bool ok = benchmark_test_retention_fixture(root, 4, 0);
    sqlite_t *db = benchmark_test_open(root);
    ok = db && sqlite_exec_cstr(db,
        "UPDATE benchmark_run SET started_utc=datetime('now','-100 days'), finished_utc=datetime('now','-100 days')") && ok;
    sqlite_close(db);
    const char *prune[] = {"prune"}, *run[] = {"run", "--operation", "sin", "--backend", "qfloat", "--samples", "3"};
    ok = ok && benchmark_test_command(root, prune, 1, 0, NULL);
    db = benchmark_test_open(root);
    ok = db && benchmark_test_integer(db, "SELECT count(*) FROM benchmark_run") == 0 &&
        benchmark_test_integer(db, "SELECT last_run_id FROM benchmark_retention") == 4 && ok;
    sqlite_close(db);
    ok = ok && benchmark_test_command(root, run, 7, 0, NULL);
    db = benchmark_test_open(root);
    ok = db && benchmark_test_integer(db, "SELECT id FROM benchmark_run") == 5 &&
        benchmark_test_integer(db, "SELECT count(*) FROM benchmark_sample") == 3 && ok;
    sqlite_close(db);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "expired runs can all be removed without ever recycling their published run identifiers");
}

static void test_benchmarks_retention_resume(void)
{
    string_t *root = test_checks_root();
    const char *run[] = {"run", "--operation", "sin", "--backend", "qfloat", "--samples", "3"};
    bool ok = benchmark_test_fixture(root) && benchmark_test_command(root, run, 7, 0, NULL);
    sqlite_t *db = benchmark_test_open(root);
    ok = db && sqlite_exec_cstr(db,
        "UPDATE benchmark_run SET status='interrupted', started_utc=datetime('now','-100 days'), finished_utc=NULL;\n"
        "UPDATE benchmark_retention SET max_days=1, keep_runs=1, raw_runs=0;") && ok;
    sqlite_close(db);
    const char *resume[] = {"run", "--resume", "1"};
    ok = ok && benchmark_test_command(root, resume, 3, 0, NULL);
    db = benchmark_test_open(root);
    ok = db && benchmark_test_integer(db, "SELECT count(*) FROM benchmark_run WHERE id=1 AND status='complete'") == 1 &&
        benchmark_test_integer(db, "SELECT sample_count FROM benchmark_result WHERE run_id=1") == 3 &&
        benchmark_test_integer(db, "SELECT count(*) FROM benchmark_sample") == 0 && ok;
    sqlite_close(db);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "a requested resumable run survives age expiry and keeps its summary when raw retention is zero");
}

static void test_benchmarks_retention_lock_and_migration(void)
{
    string_t *root = test_checks_root();
    bool ok = benchmark_test_stats_fixture(root);
    sqlite_t *db = benchmark_test_open(root);
    ok = db && sqlite_exec_cstr(db, "DROP TABLE benchmark_retention; PRAGMA user_version=1") && ok;
    sqlite_close(db);
    const char *sync[] = {"sync"};
    ok = ok && benchmark_test_command(root, sync, 1, 0, NULL);
    string_t *path = checks_path(root, "bench/benchmarks.db");
    file_t *lock = file_new(path);
    string_free(path);
    ok = lock && file_open_read(lock) && file_lock(lock, true, false) && ok;
    const char *configure[] = {"retention", "--keep-runs", "1"}, *prune[] = {"prune"};
    ok = ok && benchmark_test_command(root, configure, 3, 1, NULL) && benchmark_test_command(root, prune, 1, 1, NULL);
    file_free(lock);
    db = benchmark_test_open(root);
    ok = db && benchmark_test_integer(db, "PRAGMA user_version") == 3 &&
        benchmark_test_integer(db, "SELECT keep_runs FROM benchmark_retention") == 20 &&
        benchmark_test_integer(db, "SELECT count(*) FROM benchmark_result") == 8 &&
        benchmark_test_integer(db, "SELECT timing_outlier FROM benchmark_result WHERE operation='hypergeometric_pFq'") == 1 && ok;
    sqlite_close(db);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "version-one history migrates intact and an active benchmark excludes policy changes and pruning");
}

static void readme_example_benchmark_retention(void)
{
    string_t *root = test_checks_root(), *out = NULL;
    const char *sync[] = {"sync"}, *show[] = {"retention"};
    const char *expected = "Benchmark retention\n  Database cap: 32 MiB\n  Recent runs: 20\n  Maximum age: 90 days\n"
                           "  Raw samples: newest 5 runs\n";
    bool ok = benchmark_test_fixture(root) && benchmark_test_command(root, sync, 1, 0, NULL) &&
        benchmark_test_command(root, show, 1, 0, &out) && checks_equal(out, expected);
    if (ok) ok = fputs(string_c_str(out), stdout) != EOF;
    string_free(out);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "README benchmark retention command prints the documented persisted defaults");
}

/* Register stateful storage and real worker checks with the ordinary sequential test harness. */
void test_checks_benchmark_cases(void)
{
    TEST_RUN_IN_GROUP(test_benchmarks_catalogue, tests, "benchmark,storage");
    TEST_RUN_IN_GROUP(test_benchmarks_api_coverage, tests, "benchmark,coverage");
    TEST_RUN_IN_GROUP(test_benchmarks_expression_coverage, tests, "benchmark,expression,coverage");
    TEST_RUN_IN_GROUP(test_benchmarks_measurements, tests, "benchmark,timing");
    TEST_RUN_IN_GROUP(test_benchmarks_integer_inputs, tests, "benchmark,integer,timing");
    TEST_RUN_IN_GROUP(test_benchmarks_fraction_inputs, tests, "benchmark,fraction,timing");
    TEST_RUN_IN_GROUP(test_benchmarks_documentation, tests, "benchmark,markdown");
    TEST_RUN_IN_GROUP(test_benchmarks_documentation_supported_rows, tests, "benchmark,markdown");
    TEST_RUN_IN_GROUP(test_benchmarks_stats, tests, "benchmark,statistics");
    TEST_RUN_IN_GROUP(test_benchmarks_stats_exports, tests, "benchmark,statistics");
    TEST_RUN_IN_GROUP(test_benchmarks_stats_control_text, tests, "benchmark,statistics");
    TEST_RUN_IN_GROUP(test_benchmarks_stats_default_details, tests, "benchmark,statistics");
    TEST_RUN_IN_GROUP(test_benchmarks_complex_inputs, tests, "benchmark,statistics,complex");
    TEST_RUN_IN_GROUP(test_benchmarks_progress_capture, tests, "benchmark,progress");
    TEST_RUN_IN_GROUP(test_benchmarks_progress_terminal, tests, "benchmark,progress");
    TEST_RUN_IN_GROUP(test_benchmarks_retention_policy, tests, "benchmark,retention");
    TEST_RUN_IN_GROUP(test_benchmarks_retention_expiry, tests, "benchmark,retention");
    TEST_RUN_IN_GROUP(test_benchmarks_retention_rollback, tests, "benchmark,retention");
    TEST_RUN_IN_GROUP(test_benchmarks_retention_size, tests, "benchmark,retention");
    TEST_RUN_IN_GROUP(test_benchmarks_retention_capacity, tests, "benchmark,retention");
    TEST_RUN_IN_GROUP(test_benchmarks_retention_sample_pressure, tests, "benchmark,retention");
    TEST_RUN_IN_GROUP(test_benchmarks_retention_sequence, tests, "benchmark,retention");
    TEST_RUN_IN_GROUP(test_benchmarks_retention_resume, tests, "benchmark,retention");
    TEST_RUN_IN_GROUP(test_benchmarks_retention_lock_and_migration, tests, "benchmark,retention");
}

/* Run the documented policy display after ordinary benchmark regressions. */
void test_checks_benchmark_readme_cases(void)
{
    TEST_RUN_IN_GROUP(readme_example_benchmark_retention, readme_examples, "benchmark,retention,readme");
}
