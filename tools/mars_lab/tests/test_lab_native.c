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
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

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
        string_t *suffix = string_sprintf("/%s", cases[i].name);
        ok = fallback && suffix && string_ends_with(fallback, string_c_str(suffix)) && ok;
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
                     "all worker overrides, empty defaults, owned paths and invalid basenames are handled");
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
    TEST_SECTION("Native mathematical worker regressions");
    test_lab_math_cases();
    TEST_SECTION("README examples (last)");
    test_lab_function_readme_cases();
    test_lab_math_readme_cases();
    return TEST_EXIT_CODE();
}
