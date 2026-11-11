/**
 * @file test_lab_worker_sessions.c
 * @brief Stateful regressions for resident calculations, isolation and bounded worker recovery.
 *
 * Each case runs in a disposable fixture, owns its calculation service and stops it before returning.
 * Linux process observations verify enforced allocation limits, descriptor stability and actual process reuse.
 * These lifecycle tests need executable control and deliberately remain C rather than data-only SQL cases.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
/* POSIX alternate signal stacks and the MARS stack container are independent types. */
#define stack_t posix_signal_stack_t
#include <signal.h>
#include <sys/wait.h>
#undef stack_t
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "array-t.h"
#include "file-t.h"
#include "string-t.h"
#include "lab_process.h"
#include "test_harness.h"
#include "test_lab_support.h"
#define MARS_LAB_PROCESS_INTERNAL_ACCESS
#include "process/lab_process_session.h"

static bool session_output(const char *directory, const char *source, const char *precision, const char *expected)
{
    const char *arguments[] = {"ophelia", precision, NULL};
    string_t *input = string_new_with(source), *output = NULL;
    int status = -1;
    bool ran = input && lab_proc_run_worker(arguments, directory, input, 5000, 65536, &output, &status);
    bool ok = ran && !status && output && !strcmp(string_c_str(output), expected);
    if (!ok) string_fprintf(stderr, "Resident job: ran=%d status=%d errno=%d output=[%S]\n", ran, status, errno, output);
    string_free(output);
    string_free(input);
    return ok;
}

static string_t *session_observation(pid_t pid, const char *name)
{
    string_t *path = string_sprintf("/proc/%ld/%s", (long)pid, name);
    file_t *file = path ? file_new(path) : NULL;
    string_t *text = file ? file_read_all_text(file) : NULL;
    file_free(file);
    string_free(path);
    return text;
}

static size_t session_descriptors(pid_t pid)
{
    string_t *path = string_sprintf("/proc/%ld/fd", (long)pid);
    file_t *directory = path ? file_new(path) : NULL;
    array_t *entries = directory ? file_list_directory(directory) : NULL;
    size_t count = entries ? array_size(entries) : 0;
    array_destroy(entries);
    file_free(directory);
    string_free(path);
    return count;
}

static bool reuse_checks(const char *directory)
{
    lab_proc_workers_stop();
    unsetenv("MARS_LAB_OPHELIA_BINARY");
    unsetenv("MARS_LAB_WORK_LIMIT");
    bool ok = lab_proc_workers_start(0, 128) && session_output(directory, "output(40+2).", "32", "42\n");
    pid_t first = lab_proc_persistent_pid();
    size_t descriptors = session_descriptors(first);
    for (unsigned i = 0; ok && i < 24; ++i) {
        ok = session_output(directory, i % 2 ? "output(7)." : "output(1234567).", i % 2 ? "80" : "20",
                            i % 2 ? "7\n" : "1234567\n") && lab_proc_persistent_pid() == first &&
             session_descriptors(first) == descriptors;
    }
    string_t *limits = session_observation(first, "limits");
    const char *line = limits ? strstr(string_c_str(limits), "Max address space") : NULL;
    unsigned long long soft = 0, hard = 0;
    ok = ok && descriptors && line && sscanf(line, "Max address space %llu %llu", &soft, &hard) == 2 &&
         soft && soft <= lab_proc_memory_budget() && hard == soft;
    string_free(limits);
    return ok;
}

static bool recycle_checks(const char *directory)
{
    lab_proc_workers_stop();
    bool ok = lab_proc_workers_start(0, 2) && session_output(directory, "output(1).", "32", "1\n");
    pid_t first = lab_proc_persistent_pid();
    ok = ok && session_output(directory, "output(2).", "32", "2\n") && lab_proc_persistent_pid() == first &&
         session_output(directory, "output(3).", "32", "3\n") && lab_proc_persistent_pid() != first;
    return ok;
}

static bool recovery_checks(const char *directory)
{
    lab_proc_workers_stop();
    bool ok = lab_proc_workers_start(0, 0) && session_output(directory, "output(1).", "32", "1\n");
    pid_t first = lab_proc_persistent_pid();
    ok = ok && kill(first, SIGKILL) == 0;
    /* The supervisor observes an idle crash and replaces the process before its next job. */
    struct timespec settle = {.tv_nsec = 20000000};
    nanosleep(&settle, NULL);
    ok = ok && session_output(directory, "output(2).", "32", "2\n") && lab_proc_persistent_pid() != first;
    pid_t second = lab_proc_persistent_pid();
    ok = ok && kill(second, SIGSTOP) == 0;
    const char *arguments[] = {"ophelia", "32", NULL};
    string_t *input = string_new_with("output(3)."), *output = NULL;
    int status = -1;
    bool ran = ok && input && lab_proc_run_worker(arguments, directory, input, 80, 65536, &output, &status);
    int error = errno;
    string_free(output);
    string_free(input);
    return ok && !ran && error == ETIMEDOUT && session_output(directory, "output(4).", "32", "4\n") &&
           lab_proc_persistent_pid() != second;
}

static bool bounded_growth_checks(const char *directory)
{
    lab_proc_workers_stop();
    bool ok = lab_proc_workers_start(0, 128) &&
              session_output(directory, "output(sin(0)+42).", "80", "42\n");
    pid_t first = lab_proc_persistent_pid();
    string_t *before = session_observation(first, "status");
    for (unsigned i = 0; ok && i < 48; ++i)
        ok = session_output(directory, "output(40+2).", "32", "42\n") && lab_proc_persistent_pid() == first;
    string_t *after = session_observation(first, "status");
    const char *a = before ? strstr(string_c_str(before), "VmRSS:") : NULL;
    const char *b = after ? strstr(string_c_str(after), "VmRSS:") : NULL;
    unsigned long resident_before = 0, resident_after = 0;
    ok = ok && a && b && sscanf(a, "VmRSS: %lu", &resident_before) == 1 &&
         sscanf(b, "VmRSS: %lu", &resident_after) == 1 && resident_after <= resident_before + 2048;
    fprintf(stdout, "Resident worker: RSS %lu -> %lu KiB after 48 additional jobs\n", resident_before, resident_after);
    string_free(before);
    string_free(after);
    return ok;
}

static bool allocation_ceiling_checks(const char *directory)
{
    (void)directory;
    fflush(NULL);
    pid_t child = fork();
    if (!child) {
        size_t ceiling = 64u * 1024u * 1024u;
        bool capped = lab_proc_memory_limit(ceiling);
        errno = 0;
        void *allocation = capped ? malloc(ceiling * 2u) : NULL;
        bool refused = capped && !allocation && errno == ENOMEM;
        free(allocation);
        _exit(refused ? 0 : 1);
    }
    int status = 0;
    pid_t waited;
    do { waited = waitpid(child, &status, 0); } while (waited < 0 && errno == EINTR);
    return child > 0 && waited == child && WIFEXITED(status) && !WEXITSTATUS(status);
}

static bool coefficient_reuse_checks(const char *directory)
{
    lab_proc_workers_stop();
    const char *arguments[] = {"ophelia", "1234", NULL};
    string_t *input = string_new_with("output(digamma(2.25+0.125i))."), *first = NULL;
    bool ok = input && lab_proc_workers_start(0, 0);
    pid_t pid = -1;
    double milliseconds[2] = {0};
    for (unsigned i = 0; ok && i < 2; ++i) {
        string_t *output = NULL;
        int status = -1;
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);
        bool ran = lab_proc_run_worker(arguments, directory, input, 10000, 65536, &output, &status);
        clock_gettime(CLOCK_MONOTONIC, &end);
        milliseconds[i] = 1000.0 * (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e6;
        ok = ran && !status && output && string_byte_length(output) > 1000;
        if (!i) { pid = lab_proc_persistent_pid(); first = output; }
        else {
            ok = ok && pid == lab_proc_persistent_pid() && !string_compare(first, output);
            string_free(output);
        }
    }
    fprintf(stdout, "Resident digamma (1234 digits): first %.3f ms; repeat %.3f ms; same PID %ld\n",
            milliseconds[0], milliseconds[1], (long)pid);
    string_free(first);
    string_free(input);
    return ok;
}

static void test_worker_session_reuse(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(reuse_checks), "jobs reuse a capped worker without stale stdin or descriptor growth");
}

static void test_worker_session_recycling(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(recycle_checks), "job-count recycling replaces only the calculation process");
}

static void test_worker_session_recovery(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(recovery_checks), "crashed and unresponsive workers are reaped and replaced");
}

static void test_worker_session_bounded_growth(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(bounded_growth_checks), "repeated simple jobs retain bounded resident memory");
}

static void test_worker_session_coefficient_reuse(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(coefficient_reuse_checks), "high-precision coefficients survive identical requests");
}

static void test_worker_session_allocation_ceiling(void)
{
    TEST_ASSERT_TRUE(test_lab_isolated(allocation_ceiling_checks), "the hard address-space limit refuses excess allocation");
}

/* Register stateful process tests in the ordinary-test section, before README examples. */
void test_lab_worker_session_cases(void)
{
    TEST_RUN_IN_GROUP(test_worker_session_reuse, tests, NULL);
    TEST_RUN_IN_GROUP(test_worker_session_recycling, tests, NULL);
    TEST_RUN_IN_GROUP(test_worker_session_recovery, tests, NULL);
    TEST_RUN_IN_GROUP(test_worker_session_bounded_growth, tests, NULL);
    TEST_RUN_IN_GROUP(test_worker_session_allocation_ceiling, tests, NULL);
    TEST_RUN_IN_GROUP(test_worker_session_coefficient_reuse, tests, NULL);
}
