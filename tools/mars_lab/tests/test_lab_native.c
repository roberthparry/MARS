/**
 * @file test_lab_native.c
 * @brief Sequential native MARS Lab process, input and cancellation regressions.
 *
 * Exercises argument-vector execution, merged output, exit codes, working directories,
 * byte limits, UTF-8 boundaries, file-backed standard input and worker shutdown.
 * Linux subreaper fixtures let the suite verify and reap terminated descendants
 * without relying on an external init process. Signal dispositions and subreaper
 * state are restored before assertions, including on failed process operations.
 *
 * Used by the normal tests_main harness; this is test support, not installed code.
 * Explicit /bin/sh invocations provide child fixtures only: the process helper must
 * never interpret argument strings itself. All cases run sequentially.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "file.h"
#include "lab_process.h"
#include "test_harness.h"
#include "test_lab_support.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

static bool output_equals(const string_t *output, const char *expected)
{
    return output && string_byte_length(output) == strlen(expected) &&
           memcmp(string_c_str(output), expected, strlen(expected)) == 0;
}

static bool worker_path_checks(const char *directory)
{
    (void)directory;
#ifdef DEBUG
    const char *configuration = "debug";
#else
    const char *configuration = "release";
#endif
    static const struct {
        const char *name;
        const char *environment;
    } cases[] = {{"mars_lab", "MARS_LAB_BINARY"},
                 {"equation_lab", "MARS_LAB_EQUATION_BINARY"},
                 {"diffequation_lab", "MARS_LAB_DIFFEQUATION_BINARY"},
                 {"matrix_lab", "MARS_LAB_MATRIX_BINARY"},
                 {"integrator_lab", "MARS_LAB_INTEGRATOR_BINARY"},
                 {"datetime_lab", "MARS_LAB_DATETIME_BINARY"},
                 {"almanac_lab", "MARS_LAB_ALMANAC_BINARY"},
                 {"almanac_event_lab", "MARS_LAB_ALMANAC_EVENT_BINARY"},
                 {"holiday_lab", "MARS_LAB_HOLIDAY_BINARY"},
                 {"ophelia", "MARS_LAB_OPHELIA_BINARY"}};
    bool ok = true;
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); ++i) {
        bool cleared = unsetenv(cases[i].environment) == 0;
        string_t *fallback = cleared ? lab_proc_worker_path(cases[i].name) : NULL;
        string_t *suffix = string_sprintf("/tools/mars_lab/build/%s/mars_lab", configuration);
        ok = fallback && suffix && string_ends_with(fallback, string_c_str(suffix)) && ok;
        file_t *executable = fallback ? file_new(fallback) : NULL;
        file_info_t *info = executable ? file_get_info(executable) : NULL;
        ok = info && file_info_type(info) == FILE_TYPE_REGULAR && file_info_size(info) > 0 &&
             (file_info_permissions(info) & 0111u) && ok;
        file_info_free(info);
        file_free(executable);
        bool changed = setenv(cases[i].environment, "/custom path/worker;$literal", 1) == 0;
        string_t *override = changed ? lab_proc_worker_path(cases[i].name) : NULL;
        ok = output_equals(override, "/custom path/worker;$literal") && ok;
        changed = setenv(cases[i].environment, "", 1) == 0;
        string_t *empty = changed ? lab_proc_worker_path(cases[i].name) : NULL;
        ok = fallback && empty && string_compare(fallback, empty) == 0 && ok;
        /* Returned paths remain owned copies after their environment variable changes. */
        ok = output_equals(override, "/custom path/worker;$literal") && ok;
        string_free(empty);
        string_free(override);
        string_free(suffix);
        string_free(fallback);
    }
    const char *invalid[] = {NULL, "", "unknown", "../mars_lab", "/bin/mars_lab"};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i) {
        string_t *path = lab_proc_worker_path(invalid[i]);
        ok = !path && errno == EINVAL && ok;
        string_free(path);
    }
    return ok;
}

static void test_process_worker_paths(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(worker_path_checks),
                     "all ten calculation modes resolve to the server; overrides and invalid names are handled");
}

static bool lab_test_builtin_workers(const char *directory)
{
    static const char *const modes[] = {"mars_lab", "equation_lab", "diffequation_lab", "matrix_lab",
                                        "integrator_lab", "datetime_lab", "almanac_lab", "almanac_event_lab",
                                        "holiday_lab", "ophelia"};
    bool ok = setenv("MARS_ROOT", "/nonexistent-mars-worker-root", 1) == 0;
    for (size_t i = 0; i < sizeof(modes) / sizeof(*modes); ++i) {
        const char *argv[] = {modes[i], "--help", NULL};
        string_t *output = NULL;
        int status = -1;
        bool ran = lab_proc_run_worker(argv, directory, NULL, 5000, 65536, &output, &status);
        ok = ran && status >= 0 && output && string_byte_length(output) > 0 &&
             string_find(output, "Unknown or missing MARS Lab calculation mode") < 0 && ok;
        string_free(output);
    }
    string_t *path = lab_proc_worker_path("mars_lab");
    const char *unknown[] = {path ? string_c_str(path) : "", "--worker", "unknown-mode", NULL};
    const char *missing[] = {path ? string_c_str(path) : "", "--worker", NULL};
    const char *const *commands[] = {unknown, missing};
    for (size_t i = 0; i < sizeof(commands) / sizeof(*commands); ++i) {
        string_t *output = NULL;
        int status = -1;
        bool ran = path && lab_proc_run(commands[i], directory, 5000, 4096, &output, &status);
        ok = ran && status == 2 && output &&
             string_find(output, "Unknown or missing MARS Lab calculation mode") >= 0 && ok;
        string_free(output);
    }
    string_free(path);
    return ok;
}

static void test_process_builtin_workers(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(lab_test_builtin_workers),
                     "all ten calculations are inside the server and dispatch before listener/root setup");
}

static bool lab_test_worker_execution(const char *directory)
{
    const char *argv[] = {"ophelia", "40", NULL};
    string_t *input = string_new_with("output(40+2).");
    string_t *output = NULL;
    int status = -1;
    bool ok = unsetenv("MARS_LAB_OPHELIA_BINARY") == 0 && input &&
              lab_proc_run_worker(argv, directory, input, 5000, 4096, &output, &status) && status == 0 &&
              output_equals(output, "42\n");
    if (!ok)
        string_fprintf(stderr, "Built-in stdin result: status=%d errno=%d output=[%S]\n", status, errno, output);
    string_free(output);
    output = NULL;
    bool ran = lab_proc_run_worker(argv, directory, input, 5000, 1, &output, &status);
    ok = !ran && errno == EFBIG && ok;
    if (!ok)
        string_fprintf(stderr, "Built-in output limit: ran=%d status=%d errno=%d\n", ran, status, errno);
    string_free(output);
    output = NULL;
    bool configured = setenv("MARS_LAB_OPHELIA_BINARY", "/bin/cat", 1) == 0;
    const char *cat[] = {"ophelia", NULL};
    ran = configured && lab_proc_run_worker(cat, directory, input, 5000, 4096, &output, &status);
    ok = ran && status == 0 && output && string_compare(input, output) == 0 && ok;
    if (!ok)
        string_fprintf(stderr, "Worker cat override: ran=%d status=%d errno=%d output=[%S]\n", ran, status, errno, output);
    string_free(output);
    output = NULL;
    configured = setenv("MARS_LAB_OPHELIA_BINARY", "/bin/echo", 1) == 0;
    const char *echo[] = {"ophelia", "literal ; $(not-a-shell)", NULL};
    ran = configured && lab_proc_run_worker(echo, directory, NULL, 5000, 4096, &output, &status);
    ok = ran && status == 0 && output_equals(output, "literal ; $(not-a-shell)\n") && ok;
    if (!ok)
        string_fprintf(stderr, "Worker echo override: ran=%d status=%d errno=%d output=[%S]\n", ran, status, errno, output);
    string_free(output);
    output = NULL;
    configured = setenv("MARS_LAB_OPHELIA_BINARY", "/bin/sleep", 1) == 0;
    const char *sleep[] = {"ophelia", "10", NULL};
    ran = configured && lab_proc_run_worker(sleep, directory, NULL, 50, 4096, &output, &status);
    ok = !ran && errno == ETIMEDOUT && ok;
    if (!ok)
        string_fprintf(stderr, "Worker deadline: ran=%d status=%d errno=%d\n", ran, status, errno);
    string_free(output);
    output = NULL;
    configured = setenv("MARS_LAB_OPHELIA_BINARY", "/missing-lab-worker", 1) == 0;
    ran = configured && lab_proc_run_worker(cat, directory, NULL, 5000, 4096, &output, &status);
    ok = !ran && errno == ENOENT && status == -1 && ok;
    if (!ok)
        string_fprintf(stderr, "Missing worker override: ran=%d status=%d errno=%d\n", ran, status, errno);
    string_free(output);
    string_free(input);
    return ok;
}

static void test_process_worker_execution(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(lab_test_worker_execution),
                     "built-in stdin/output and explicit overrides preserve limits, deadlines and literal arguments");
}

static void test_process_worker_invalid_arguments(void)
{
    const char *unknown[] = {"unknown", NULL};
    const char *const *cases[] = {NULL, unknown};
    bool ok = true;
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); ++i) {
        string_t *output = NULL;
        int status = 99;
        bool ran = lab_proc_run_worker(cases[i], NULL, NULL, 1000, 128, &output, &status);
        ok = !ran && errno == EINVAL && !output && status == -1 && ok;
        string_free(output);
    }
    const char *excess[257];
    excess[0] = "mars_lab";
    for (size_t i = 1; i < 256; ++i)
        excess[i] = "x";
    excess[256] = NULL;
    string_t *output = NULL;
    int status = 99;
    bool ran = lab_proc_run_worker(excess, NULL, NULL, 1000, 128, &output, &status);
    ok = !ran && errno == E2BIG && !output && status == -1 && ok;
    string_free(output);
    TEST_ASSERT_TRUE(ok, "invalid calculation modes and oversized argument vectors are rejected before spawning");
}

static void test_process_success(void)
{
    const char *const argv[] = {"/bin/sh", "-c", "printf 'out'; printf 'err' >&2; printf 'end'", NULL};
    string_t *output = NULL;
    int status = -1;
    bool ran = lab_proc_run(argv, NULL, 3000, 64, &output, &status);
    bool ok = ran && status == 0 && output_equals(output, "outerrend");
    string_free(output);
    TEST_ASSERT_TRUE(ok, "stdout and stderr are captured in their write order");

    const char *const literal[] = {"printf", "%s", "$(printf unexpected); * $HOME", NULL};
    ran = lab_proc_run(literal, NULL, 3000, 64, &output, &status);
    ok = ran && status == 0 && output_equals(output, literal[2]);
    string_free(output);
    TEST_ASSERT_TRUE(ok, "PATH lookup preserves literal argument bytes without shell interpretation");
}

static void test_process_exit_status(void)
{
    const char *const nonzero[] = {"/bin/sh", "-c", "printf 'failure' >&2; exit 23", NULL};
    string_t *output = NULL;
    int status = -1;
    bool ran = lab_proc_run(nonzero, NULL, 3000, 64, &output, &status);
    bool ok = ran && status == 23 && output_equals(output, "failure");
    string_free(output);
    TEST_ASSERT_TRUE(ok, "non-zero exit is a completed run with captured diagnostics");
    const char *const signalled[] = {"/bin/sh", "-c", "kill -KILL $$", NULL};
    ran = lab_proc_run(signalled, NULL, 3000, 64, &output, &status);
    ok = ran && status == 128 + SIGKILL;
    string_free(output);
    TEST_ASSERT_TRUE(ok, "signal exit uses 128 plus the signal number");
}

static void test_process_missing_executable(void)
{
    const char *const argv[] = {"/dev/null/mars-no-such-programme", NULL};
    string_t *output = NULL;
    int status = 0;
    bool ran = lab_proc_run(argv, NULL, 3000, 64, &output, &status);
    int error = errno;
    /* POSIX permits failure before spawn returns, or child exit 127 after it returns. */
    bool failed = (!ran && (error == ENOTDIR || error == ENOENT) && status == -1) || (ran && status == 127);
    bool ok = failed && output_equals(output, "");
    string_free(output);
    TEST_ASSERT_TRUE(ok, "an absent executable reports a spawn error or failed child, never success");
}

static void test_process_invalid_arguments(void)
{
    const char *const empty[] = {NULL};
    const char *const blank[] = {"", NULL};
    const char *const valid[] = {"/bin/true", NULL};
    string_t *output = NULL;
    int status = 99;
    bool ok = !lab_proc_run(NULL, NULL, 100, 8, &output, &status) && errno == EINVAL && !output && status == -1;
    ok = ok && !lab_proc_run(empty, NULL, 100, 8, &output, &status) && errno == EINVAL && !output;
    ok = ok && !lab_proc_run(blank, NULL, 100, 8, &output, &status) && errno == EINVAL && !output;
    ok = ok && !lab_proc_run(valid, NULL, 100, 8, NULL, &status) && errno == EINVAL;
    ok = ok && !lab_proc_run(valid, NULL, 100, 8, &output, NULL) && errno == EINVAL && !output;
    string_free(output);
    TEST_ASSERT_TRUE(ok, "invalid arguments initialise supplied destinations and report EINVAL");
}

static void test_process_cwd(void)
{
    char directory[] = "/tmp/mars-process-test-XXXXXX";
    TEST_ASSERT_NOT_NULL(mkdtemp(directory));
    char *before = getcwd(NULL, 0);
    const char *const argv[] = {"/bin/pwd", "-P", NULL};
    string_t *output = NULL;
    int status = -1;
    bool ran = lab_proc_run(argv, directory, 3000, 4096, &output, &status);
    string_t *expected = string_sprintf("%s\n", directory);
    char *after = getcwd(NULL, 0);
    bool ok = ran && status == 0 && expected && output_equals(output, string_c_str(expected)) && before && after &&
              strcmp(before, after) == 0;
    string_free(expected);
    string_free(output);
    free(before);
    free(after);
    ok = rmdir(directory) == 0 && ok;
    ran = lab_proc_run(argv, directory, 3000, 64, &output, &status);
    int error = errno;
    bool failed = (!ran && error == ENOENT && status == -1) || (ran && status == 127);
    ok = ok && failed && output_equals(output, "");
    string_free(output);
    TEST_ASSERT_TRUE(ok, "child cwd changes leave the caller unchanged and an invalid cwd prevents execution");
}

/* Reap a known adopted descendant with a bounded wait; kill it as failure cleanup. */
static bool reap_descendant(pid_t child)
{
    if (child <= 0)
        return false;
    for (unsigned attempt = 0; attempt < 200; ++attempt) {
        int status = 0;
        pid_t result = waitpid(child, &status, WNOHANG);
        if (result == child)
            return WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL;
        if (result < 0 && errno != EINTR)
            return false;
        struct timespec delay = {.tv_nsec = 10000000};
        nanosleep(&delay, NULL);
    }
    kill(child, SIGKILL);
    while (waitpid(child, NULL, 0) < 0 && errno == EINTR) {
    }
    return false;
}

static void test_process_timeout(void)
{
    const char *const argv[] = {"/bin/sleep", "30", NULL};
    string_t *output = NULL;
    int status = -1;
    bool ran = lab_proc_run(argv, NULL, 100, 64, &output, &status);
    int error = errno;
    bool ok = !ran && error == ETIMEDOUT && status == 128 + SIGKILL;
    string_free(output);
    TEST_ASSERT_TRUE(ok, "timeout kills and reaps the direct child");
}

static void test_process_descendant_timeout(void)
{
    int previous = 0;
    TEST_ASSERT_INT_EQ(prctl(PR_GET_CHILD_SUBREAPER, &previous), 0);
    TEST_ASSERT_INT_EQ(prctl(PR_SET_CHILD_SUBREAPER, 1), 0);
    /* The leader exits, but its descendant retains stdout: collection must still expire. */
    const char *const argv[] = {"/bin/sh", "-c", "sleep 30 & printf '%s\n' \"$!\"; exit 0", NULL};
    string_t *output = NULL;
    int status = -1;
    bool ran = lab_proc_run(argv, NULL, 500, 64, &output, &status);
    int error = errno;
    pid_t descendant = output ? (pid_t)strtol(string_c_str(output), NULL, 10) : -1;
    bool reaped = reap_descendant(descendant);
    string_free(output);
    bool restored = prctl(PR_SET_CHILD_SUBREAPER, previous) == 0;
    TEST_ASSERT_TRUE(!ran && error == ETIMEDOUT && status == 0 && reaped && restored,
                     "timeout kills descendants even when their group leader has already exited");
}

static void test_process_output_limits(void)
{
    const char *const exact[] = {"/usr/bin/printf", "1234", NULL};
    const char *const empty[] = {"/bin/true", NULL};
    string_t *output = NULL;
    int status = -1;
    bool ran = lab_proc_run(exact, NULL, 3000, 4, &output, &status);
    bool ok = ran && status == 0 && output_equals(output, "1234");
    string_free(output);
    ran = lab_proc_run(empty, NULL, 3000, 0, &output, &status);
    ok = ok && ran && status == 0 && output_equals(output, "");
    string_free(output);
    ran = lab_proc_run(exact, NULL, 3000, 0, &output, &status);
    int error = errno;
    ok = ok && !ran && error == EFBIG && output_equals(output, "");
    string_free(output);
    const char *const flood[] = {"/bin/sh", "-c", "while :; do printf 'abcdefgh'; done", NULL};
    ran = lab_proc_run(flood, NULL, 3000, 31, &output, &status);
    error = errno;
    ok = ok && !ran && error == EFBIG && status == 128 + SIGKILL && output && string_byte_length(output) == 31;
    string_free(output);
    TEST_ASSERT_TRUE(ok, "exact and zero limits succeed; overflow stays bounded and kills an active writer");
}

static void test_process_descendant_overflow(void)
{
    int previous = 0;
    TEST_ASSERT_INT_EQ(prctl(PR_GET_CHILD_SUBREAPER, &previous), 0);
    TEST_ASSERT_INT_EQ(prctl(PR_SET_CHILD_SUBREAPER, 1), 0);
    const char *const argv[] = {"/bin/sh", "-c", "sleep 30 & printf '%s\n' \"$!\"; while :; do printf 'abcdefgh'; done",
                                NULL};
    string_t *output = NULL;
    int status = -1;
    bool ran = lab_proc_run(argv, NULL, 3000, 64, &output, &status);
    int error = errno;
    pid_t descendant = output ? (pid_t)strtol(string_c_str(output), NULL, 10) : -1;
    bool reaped = reap_descendant(descendant);
    bool bounded = output && string_byte_length(output) == 64;
    string_free(output);
    bool restored = prctl(PR_SET_CHILD_SUBREAPER, previous) == 0;
    TEST_ASSERT_TRUE(!ran && error == EFBIG && status == 128 + SIGKILL && bounded && reaped && restored,
                     "overflow kills the entire group and retains only the permitted output prefix");
}

static void test_process_unicode(void)
{
    /* Split a euro sign across writes and retain a decomposed accent exactly. */
    const char *const argv[] = {"/bin/sh", "-c", "printf '\342'; sleep 0.05; printf '\202\254 e\314\201 😀'", NULL};
    string_t *output = NULL;
    int status = -1;
    bool ran = lab_proc_run(argv, NULL, 3000, 64, &output, &status);
    bool ok = ran && status == 0 && output_equals(output, "€ e\xcc\x81 😀");
    string_free(output);
    const char *const invalid[] = {"/bin/sh", "-c", "printf '\377'", NULL};
    ran = lab_proc_run(invalid, NULL, 3000, 64, &output, &status);
    int error = errno;
    ok = ok && !ran && error == EILSEQ;
    string_free(output);
    const char *const incomplete[] = {"/bin/sh", "-c", "printf '\342\202'", NULL};
    ran = lab_proc_run(incomplete, NULL, 3000, 64, &output, &status);
    error = errno;
    ok = ok && !ran && error == EILSEQ;
    string_free(output);
    TEST_ASSERT_TRUE(ok, "UTF-8 survives read boundaries without NFC conversion and malformed output is rejected");
}

static void test_process_input(void)
{
    const char *const argv[] = {"/bin/cat", NULL};
    string_t *input = string_new();
    char chunk[4096];
    memset(chunk, 'x', sizeof(chunk));
    bool ok = input != NULL;
    for (unsigned i = 0; ok && i < 32; ++i)
        ok = string_append_utf8_exact(input, chunk, sizeof(chunk)) == 0;
    const char tail[] = "\0€ e\xcc\x81";
    ok = ok && string_append_utf8_exact(input, tail, sizeof(tail) - 1) == 0;
    string_t *output = NULL;
    int status = -1;
    bool ran = ok && lab_proc_run_input(argv, "/", input, 3000, 200000, &output, &status);
    ok = ran && status == 0 && output && string_byte_length(output) == string_byte_length(input) &&
         memcmp(string_c_str(output), string_c_str(input), string_byte_length(input)) == 0;
    string_free(input);
    string_free(output);
    TEST_ASSERT_TRUE(ok, "file-backed input exceeds pipe capacity and preserves NUL and exact Unicode bytes");
    ran = lab_proc_run(argv, NULL, 3000, 0, &output, &status);
    ok = ran && status == 0 && output_equals(output, "");
    string_free(output);
    TEST_ASSERT_TRUE(ok, "default stdin is /dev/null and immediately reaches EOF");
}

static volatile sig_atomic_t cancellation;

static void request_cancellation(int signal_number)
{
    (void)signal_number;
    cancellation = 1;
}

static void test_process_cancellation(void)
{
    struct sigaction action = {.sa_handler = request_cancellation}, previous_action;
    sigemptyset(&action.sa_mask);
    int previous_subreaper = 0;
    TEST_ASSERT_INT_EQ(prctl(PR_GET_CHILD_SUBREAPER, &previous_subreaper), 0);
    TEST_ASSERT_INT_EQ(sigaction(SIGTERM, &action, &previous_action), 0);
    bool subreaper = prctl(PR_SET_CHILD_SUBREAPER, 1) == 0;
    cancellation = 0;
    lab_proc_set_cancel_flag(&cancellation);
    const char *const argv[] = {"/bin/sh", "-c",
                                "sleep 30 & printf '%s\n' \"$!\"; sleep 0.1; kill -TERM \"$PPID\"; wait", NULL};
    string_t *output = NULL;
    int status = -1;
    bool ran = subreaper && lab_proc_run(argv, NULL, 3000, 64, &output, &status);
    int error = errno;
    pid_t descendant = output ? (pid_t)strtol(string_c_str(output), NULL, 10) : -1;
    bool reaped = reap_descendant(descendant);
    string_free(output);
    bool ok = subreaper && !ran && error == ECANCELED && cancellation && status == 128 + SIGKILL && reaped;
    /* A set flag must reject a later run before a child is spawned. */
    const char *const never[] = {"/bin/true", NULL};
    ran = lab_proc_run(never, NULL, 3000, 0, &output, &status);
    error = errno;
    ok = ok && !ran && error == ECANCELED && status == -1;
    string_free(output);
    lab_proc_set_cancel_flag(NULL);
    cancellation = 0;
    bool restored = sigaction(SIGTERM, &previous_action, NULL) == 0;
    restored = prctl(PR_SET_CHILD_SUBREAPER, previous_subreaper) == 0 && restored;
    TEST_ASSERT_TRUE(ok && restored, "worker SIGTERM cancels, kills the group and reaps before returning");
}

int tests_main(void)
{
    TEST_SECTION("Native MARS Lab child processes");
    TEST_RUN_IN_GROUP(test_process_worker_paths, tests, NULL);
    TEST_RUN_IN_GROUP(test_process_builtin_workers, tests, NULL);
    TEST_RUN_IN_GROUP(test_process_worker_execution, tests, NULL);
    TEST_RUN_IN_GROUP(test_process_worker_invalid_arguments, tests, NULL);
    TEST_RUN_IN_GROUP(test_process_success, tests, NULL);
    TEST_RUN_IN_GROUP(test_process_exit_status, tests, NULL);
    TEST_RUN_IN_GROUP(test_process_missing_executable, tests, NULL);
    TEST_RUN_IN_GROUP(test_process_invalid_arguments, tests, NULL);
    TEST_RUN_IN_GROUP(test_process_cwd, tests, NULL);
    TEST_RUN_IN_GROUP(test_process_timeout, tests, NULL);
    TEST_RUN_IN_GROUP(test_process_descendant_timeout, tests, NULL);
    TEST_RUN_IN_GROUP(test_process_output_limits, tests, NULL);
    TEST_RUN_IN_GROUP(test_process_descendant_overflow, tests, NULL);
    TEST_RUN_IN_GROUP(test_process_unicode, tests, NULL);
    TEST_RUN_IN_GROUP(test_process_input, tests, NULL);
    TEST_RUN_IN_GROUP(test_process_cancellation, tests, NULL);
    TEST_SECTION("Native MARS Lab integration");
    test_lab_evaluation_cases();
    test_lab_evaluation_extra_cases();
    test_lab_function_cases();
    test_lab_calendar_extra_cases();
    test_lab_state_cases();
    test_lab_mobile_cases();
    test_lab_runtime_cases();
    test_lab_route_cases();
    test_lab_wire_cases();
    test_lab_presentation_cases();
    test_lab_forms_cases();
    test_lab_syntax_cases();
    test_lab_almanac_presentation_cases();
    TEST_SECTION("Native mathematical worker regressions");
    test_lab_math_cases();
    TEST_SECTION("README examples (last)");
    test_lab_function_readme_cases();
    test_lab_math_readme_cases();
    return TEST_EXIT_CODE();
}
