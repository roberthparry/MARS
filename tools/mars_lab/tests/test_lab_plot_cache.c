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

#include "lab_process.h"
#include "test_harness.h"
#include "test_lab_support.h"

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

/* Register sequential cache checks with the ordinary native Lab regressions. */
void test_lab_plot_cache_cases(void)
{
    TEST_RUN_IN_GROUP(test_lab_plot_cache, tests, "plot,cache");
}
