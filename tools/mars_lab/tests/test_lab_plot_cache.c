/**
 * @file test_lab_plot_cache.c
 * @brief Native solver-plot reuse, invalidation and timing regressions.
 *
 * Compares fresh and cached results, including changed viewports, and verifies
 * sharing between request processes. Timings are reported, not asserted.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "json-t.h"
#include "lab_process.h"
#include "test_harness.h"
#include "test_lab_support.h"
#define MARS_LAB_PROCESS_INTERNAL_ACCESS
#include "process/lab_process_plot_cache.h"

/* Match the plot cache's decoded-output limit. */
static const size_t plot_output_limit = 8 * 1024 * 1024;

static string_t *run(const char *const *args, unsigned timeout, double *seconds)
{
    string_t *output = NULL;
    int status = -1;
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    bool ok = lab_proc_run_worker(args, NULL, NULL, timeout, plot_output_limit, &output, &status);
    clock_gettime(CLOCK_MONOTONIC, &end);
    if (seconds)
        *seconds = end.tv_sec - start.tv_sec + (end.tv_nsec - start.tv_nsec) * 1e-9;
    if (!ok || status || !output || !strstr(string_c_str(output), "plot_svg ")) {
        if (output)
            fprintf(stderr, "Plot cache worker failure (%d): %.300s\n", status, string_c_str(output));
        string_free(output);
        return NULL;
    }
    return output;
}

static bool equal(const string_t *a, const string_t *b)
{
    return a && b && string_byte_length(a) == string_byte_length(b) &&
           !memcmp(string_c_str(a), string_c_str(b), string_byte_length(a));
}

static bool cache_checks(const char *directory)
{
    (void)directory;
    unsetenv("MARS_LAB_BINARY");
    unsetenv("MARS_LAB_WORK_LIMIT");
    lab_proc_plot_cache_init();
    const char *sources[] = {"x^4-x^3-26x^2+4x+8=0", "y''+6yy'+4y^3=0; y(0)=0; y'(0)=1",
                              "y''+6yy'+4y^3=0", "cos(z)-sin(pi-x*y)=0"};
    for (size_t i = 0; i < 4; ++i) {
        const char *args[] = {"mars_lab", sources[i], "x", "78", "plot", "", "", "auto", "", "",
                               "curve", "", "", "", "315", "30", "0", "full",
                               i && i != 3 ? "diffequation" : "equation", NULL};
        if (i == 3) {
            args[2] = "z";
            args[10] = "auto";
        }
        double cold, first, repeated, changed, fresh;
        setenv("MARS_LAB_PLOT_CACHE", "0", 1);
        string_t *baseline = run(args, 30000, &cold);
        unsetenv("MARS_LAB_PLOT_CACHE");
        string_t *initial = run(args, 30000, &first);
        string_t *cached = run(args, 1, &repeated);
        bool ok = equal(baseline, initial) && equal(initial, cached) &&
                  !strstr(string_c_str(initial), "plot_prepared");
        if (i == 3)
            ok = ok && initial && string_byte_length(initial) > 1024 * 1024;
        if (ok && i == 3) {
            pid_t child = fork();
            if (child == 0) {
                string_t *shared = run(args, 1, NULL);
                _exit(equal(initial, shared) ? 0 : 1);
            }
            int status = -1;
            ok = child > 0 && waitpid(child, &status, 0) == child && WIFEXITED(status) && !WEXITSTATUS(status);
        }
        args[5] = i == 3 ? "-1.8" : "-7";
        args[6] = i == 3 ? "1.8" : "7";
        args[7] = i == 3 ? "auto" : "manual";
        args[8] = i == 3 ? "" : i ? "-2" : "-220";
        args[9] = i == 3 ? "" : i ? "2" : "220";
        string_t *redraw = run(args, 30000, &changed);
        setenv("MARS_LAB_PLOT_CACHE", "0", 1);
        string_t *fresh_redraw = run(args, 30000, &fresh);
        unsetenv("MARS_LAB_PLOT_CACHE");
        ok = ok && equal(redraw, fresh_redraw);
        printf("Plot %zu: uncached %.4fs, initial %.4fs, repeat %.6fs, changed viewport %.4fs (uncached %.4fs)\n",
               i, cold, first, repeated, changed, fresh);
        fflush(stdout);
        if (!ok)
            fprintf(stderr, "Plot cache comparison failed for case %zu\n", i);
        string_free(fresh_redraw);
        string_free(redraw);
        string_free(cached);
        string_free(initial);
        string_free(baseline);
        if (!ok)
            return false;
    }
    const char *args[] = {"mars_lab", "x^2=4", "x", "78", "plot", "-3", "3", "auto", "", "",
                           "curve", "", "", "", "315", "30", "0", "full", "equation", NULL};
    string_t *output = run(args, 30000, NULL);
    bool ok = output && strstr(string_c_str(output), "x = 2");
    string_free(output);
    if (!ok)
        return false;
    pid_t child = fork();
    if (child == 0) {
        string_t *shared = run(args, 1, NULL);
        _exit(shared ? 0 : 1);
    }
    int status = -1;
    ok = child > 0 && waitpid(child, &status, 0) == child && WIFEXITED(status) && !WEXITSTATUS(status);
    args[3] = "35";
    output = run(args, 30000, NULL);
    ok = ok && output;
    string_free(output);
    setenv("MARS_LAB_WORK_LIMIT", "1", 1);
    output = NULL;
    status = -1;
    bool limited = lab_proc_run_worker(args, NULL, NULL, 3000, plot_output_limit, &output, &status);
    ok = ok && !limited && errno == E2BIG;
    string_free(output);
    unsetenv("MARS_LAB_WORK_LIMIT");
    return ok;
}

static void test_lab_plot_cache(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(cache_checks), "native plot cache preserves output and invalidates changed work");
}

static bool equation_evaluation_reuse(const char *directory)
{
    (void)directory;
    unsetenv("MARS_LAB_BINARY");
    unsetenv("MARS_LAB_EQUATION_BINARY");
    unsetenv("MARS_LAB_PLOT_CACHE");
    unsetenv("MARS_LAB_WORK_LIMIT");
    lab_proc_plot_cache_init();
    const char *equation[] = {"equation_lab", "zeta(z)=0", "78", NULL};
    const char *plot[] = {"mars_lab", equation[1], "z", equation[2], "plot", "", "", "auto", "", "",
                          "curve", "", "", "", "315", "30", "0", "full", "equation", NULL};
    string_t *output = NULL;
    int status = -1;
    bool ok = lab_proc_run_worker(equation, NULL, NULL, 30000, plot_output_limit, &output, &status) &&
              !status && output && strstr(string_c_str(output), "numeric_indices 39\n") &&
              !strstr(string_c_str(output), "plot_prepared ");
    string_t *path = lab_proc_worker_path("mars_lab");
    string_t *key = path ? lab_proc_plot_key(plot, NULL, path, true) : NULL;
    string_t *prepared = key ? lab_proc_plot_cache_get(key, true) : NULL;
    string_t *record_key = lab_proc_plot_record_key(prepared, NULL, path);
    json_t *record = prepared ? json_from_text(prepared) : NULL;
    const json_t *results = json_array_get(record, 7);
    const string_t *numeric = json_string_value(json_array_get(results, 0));
    ok = ok && prepared && equal(key, record_key) && json_array_size(record) == 10 && numeric &&
         string_find(numeric, "plot_solutions z ≈ 0.5") >= 0 &&
         json_array_size(json_array_get(results, 1)) == 40;
    /* A test-only protocol tag proves that the next worker uses this retained result, rather than solving again. */
    string_t *tagged = numeric ? string_sprintf("%splot_cache_fixture retained\n", string_c_str(numeric)) : NULL;
    json_t *tag = tagged ? json_new_string(tagged) : NULL;
    json_t *marked_results = json_new_array();
    ok = ok && marked_results && json_array_append(marked_results, tag) &&
         json_array_append(marked_results, json_array_get(results, 1));
    json_t *marked = json_new_array();
    for (size_t i = 0; ok && i < json_array_size(record); ++i)
        ok = json_array_append(marked, i == 7 ? marked_results : json_array_get(record, i));
    string_t *marked_record = ok ? json_to_string(marked) : NULL;
    if (marked_record)
        lab_proc_plot_cache_put(key, marked_record, true);
    pid_t child = ok && marked_record ? fork() : -1;
    if (child == 0) {
        double seconds = 0;
        string_t *drawn = run(plot, 30000, &seconds);
        bool reused = drawn && strstr(string_c_str(drawn), "plot_cache_fixture retained\n") &&
                      strstr(string_c_str(drawn), "data-root-plane=\"true\"") &&
                      strstr(string_c_str(drawn), "40 verified finite roots") &&
                      strstr(string_c_str(drawn), "Re(z)") && strstr(string_c_str(drawn), "Im(z)");
        if (!reused && drawn)
            fprintf(stderr, "Cache replay diagnostic: %.500s\n", string_c_str(drawn));
        printf("Completed zeta evaluation reused by a separate plot request: %.4fs\n", seconds);
        fflush(stdout);
        string_free(drawn);
        _exit(reused ? 0 : 1);
    }
    status = -1;
    ok = ok && child > 0 && waitpid(child, &status, 0) == child && WIFEXITED(status) && !WEXITSTATUS(status);
    plot[3] = "156";
    string_t *changed_key = path ? lab_proc_plot_key(plot, NULL, path, true) : NULL;
    string_t *changed = changed_key ? lab_proc_plot_cache_get(changed_key, true) : NULL;
    ok = ok && !changed && !equal(key, changed_key);
    string_free(changed);
    string_free(changed_key);
    string_free(marked_record);
    json_free(marked);
    json_free(marked_results);
    json_free(tag);
    string_free(tagged);
    json_free(record);
    string_free(record_key);
    string_free(prepared);
    string_free(key);
    string_free(path);
    string_free(output);
    return ok;
}

static void test_lab_equation_evaluation_plot_reuse(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(equation_evaluation_reuse),
                     "automatic equation plotting reuses the completed solve across request processes");
}

static size_t occurrences(const string_t *text, const char *needle)
{
    size_t count = 0, length = strlen(needle);
    const char *cursor = text ? string_c_str(text) : "";
    while ((cursor = strstr(cursor, needle))) {
        ++count;
        cursor += length;
    }
    return count;
}

static bool complex_solution_checks(const char *directory)
{
    (void)directory;
    unsetenv("MARS_LAB_BINARY");
    unsetenv("MARS_LAB_EQUATION_BINARY");
    unsetenv("MARS_LAB_PLOT_CACHE");
    unsetenv("MARS_LAB_WORK_LIMIT");
    lab_proc_plot_cache_init();
    const char *sources[] = {"z^2+1=0", "(z^2+1)*(z^2-1)=0", "z^2=4"};
    const size_t counts[] = {2, 4, 0};
    for (size_t i = 0; i < 3; ++i) {
        const char *equation[] = {"equation_lab", sources[i], "78", NULL};
        const char *plot[] = {"mars_lab", sources[i], "z", "78", "plot", "", "", "auto", "", "",
                              "curve", "", "", "", "315", "30", "0", "full", "equation", NULL};
        string_t *solved = NULL;
        int status = -1;
        bool ok = lab_proc_run_worker(equation, NULL, NULL, 30000, plot_output_limit, &solved, &status) &&
                  !status && solved;
        string_t *drawn = ok ? run(plot, 30000, NULL) : NULL;
        if (counts[i]) {
            ok = ok && drawn && strstr(string_c_str(drawn), "data-root-plane=\"true\"") &&
                 occurrences(drawn, "<circle ") == counts[i] &&
                 strstr(string_c_str(drawn), "Re(z)") && strstr(string_c_str(drawn), "Im(z)");
            /* The same verified points are clipped to a new viewport; no lines or fabricated samples appear. */
            plot[5] = "-0.5";
            plot[6] = "0.5";
            plot[7] = "manual";
            plot[8] = "0.5";
            plot[9] = "1.5";
            string_t *zoomed = run(plot, 30000, NULL);
            ok = ok && zoomed && occurrences(zoomed, "<circle ") == 1 &&
                 strstr(string_c_str(zoomed), "data-x-min=\"-0.5\"") &&
                 strstr(string_c_str(zoomed), "data-y-min=\"0.5\"");
            string_free(zoomed);
            /* A standalone uncached plot follows the same native root path. */
            plot[5] = plot[6] = plot[8] = plot[9] = "";
            plot[7] = "auto";
            setenv("MARS_LAB_PLOT_CACHE", "0", 1);
            string_t *cold = run(plot, 30000, NULL);
            unsetenv("MARS_LAB_PLOT_CACHE");
            ok = ok && equal(cold, drawn);
            string_free(cold);
        } else {
            ok = ok && drawn && !strstr(string_c_str(drawn), "data-root-plane") &&
                 strstr(string_c_str(drawn), "<path ");
        }
        if (!ok && drawn)
            fprintf(stderr, "Complex solution diagram failed for %s: %.400s\n", sources[i], string_c_str(drawn));
        string_free(drawn);
        string_free(solved);
        if (!ok)
            return false;
    }
    return true;
}

static void test_lab_equation_complex_solution_plot(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(complex_solution_checks),
                     "complex roots use discrete native points, preserve real roots and support cached viewport changes");
}

/* Register sequential cache checks with the ordinary native Lab regressions. */
void test_lab_plot_cache_cases(void)
{
    TEST_RUN_IN_GROUP(test_lab_plot_cache, tests, "plot,cache");
    TEST_RUN_IN_GROUP(test_lab_equation_evaluation_plot_reuse, tests, "plot,cache,equation");
    TEST_RUN_IN_GROUP(test_lab_equation_complex_solution_plot, tests, "plot,equation");
}
