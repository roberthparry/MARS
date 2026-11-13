/**
 * @file test_harness_bootstrap.c
 * @brief Bootstrap verification of the harness using C/POSIX facilities and no libmars linkage.
 *
 * Exercises assertions, opaque validity callbacks, nested registration, case resources and output capture.
 * Subprocess probes verify failure/skip accounting and diagnostics independently of MARS configuration or
 * storage. This executable intentionally creates no test catalogue or persistent results database.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "test_harness.h"
#include "test_harness_adapter.h"

TEST_SUITE_CONFIG(TEST_CONFIG_NONE);

static int independent_equal(const void *got, const void *want, void *ctx)
{
    return *(const int *)got + *(const int *)ctx == *(const int *)want;
}

static int independent_format(const void *value, test_text_t *out, void *ctx)
{
    (void)ctx;
    return test_text_append_cstr(out, "value=") || test_text_append_format(out, "%d", *(const int *)value);
}

static int offset = 1;
static const test_validity_contract_t independent_contract = {
    "independent", independent_equal, independent_format, &offset
};

static void test_core_assertions(void)
{
    TEST_ASSERT_NULL(test_harness_adapter());
    TEST_ASSERT_INT_EQ(42, 42);
    TEST_ASSERT_LONG_EQ(123456L, 123456L);
    TEST_ASSERT_DOUBLE_EQ(0.125, 1.0 / 8.0, 1e-12);
    TEST_ASSERT_STR_EQ("plain C", "plain C");
    int got = 4, want = 5;
    TEST_ASSERT_TRUE(test_assert_validity(&independent_contract, &got, &want, __FILE__, __LINE__), "opaque values");
}

static void test_core_resources(void)
{
    const char *directory = test_case_temp_dir(), *path = test_case_temp_path("capture.txt");
    TEST_ASSERT_NOT_NULL(directory);
    TEST_ASSERT_NOT_NULL(path);
    struct stat info;
    TEST_ASSERT_INT_EQ(stat(directory, &info), 0);
    TEST_ASSERT_TRUE(S_ISDIR(info.st_mode), "case owns a private temporary directory");
    const char *captured = NULL;
    int saved = test_case_begin_stdout_capture("capture.txt", &captured);
    TEST_ASSERT_TRUE(saved >= 0, "capture starts without MARS file handling");
    puts("independent output");
    bool restored = test_case_end_stdout_capture(saved);
    TEST_ASSERT_TRUE(restored, "capture restores stdout");
    FILE *input = fopen(captured, "r");
    char text[64] = {0};
    bool read = input && fgets(text, sizeof text, input);
    if (input) fclose(input);
    TEST_ASSERT_TRUE(read, "capture is readable using stdio");
    TEST_ASSERT_STR_EQ(text, "independent output\n");
}

static void test_core_nested_leaf(void)
{
    TEST_ASSERT_TRUE(true, "nested callback executed");
}

static void test_core_nested(void)
{
    TEST_RUN_SUBTEST(test_core_nested_leaf, "core");
}

static void test_core_registry(void)
{
    char name[32];
    for (unsigned i = 0; i < 23; ++i) {
        snprintf(name, sizeof name, "checker-%02u", (17 * i) % 23);
        test_register_validity_checker(name, test_validity_contract_int());
    }
    for (unsigned i = 0; i < 23; ++i) {
        snprintf(name, sizeof name, "checker-%02u", i);
        TEST_ASSERT_TRUE(test_find_validity_checker(name) == test_validity_contract_int(), "copied and indexed name");
    }
    test_register_validity_checker("checker-11", &independent_contract);
    TEST_ASSERT_TRUE(test_find_validity_checker("checker-11") == &independent_contract, "duplicate replaces contract");
    TEST_ASSERT_NULL(test_find_validity_checker("absent"));
}

static void core_probe_failure(void)
{
    int got = 7, want = 90;
    (void)test_assert_validity(&independent_contract, &got, &want, __FILE__, __LINE__);
}

static void core_probe_skip(void)
{
    test_mark_skip(__FILE__, __LINE__, "intentional bootstrap skip");
}

static void test_core_subprocesses(void)
{
    const char *modes[] = {"fail", "skip"};
    for (size_t i = 0; i < 2; ++i) {
        const char *path = test_case_temp_path(i ? "skip.txt" : "fail.txt");
        TEST_ASSERT_NOT_NULL(path);
        pid_t child = fork();
        TEST_ASSERT_TRUE(child >= 0, "fork bootstrap probe");
        if (!child) {
            FILE *output = freopen(path, "w", stdout);
            if (!output || setenv("MARS_TEST_CORE_PROBE", modes[i], 1)) _exit(120);
            execl("/proc/self/exe", "test_test_harness", (char *)NULL);
            _exit(121);
        }
        int status = 0;
        pid_t waited;
        do waited = waitpid(child, &status, 0); while (waited < 0 && errno == EINTR);
        TEST_ASSERT_TRUE(waited == child && WIFEXITED(status), "probe completed normally");
        TEST_ASSERT_INT_EQ(WEXITSTATUS(status), i ? 0 : 1);
        FILE *input = fopen(path, "r");
        TEST_ASSERT_NOT_NULL(input);
        char line[4096];
        bool found = false;
        while (fgets(line, sizeof line, input))
            if (strstr(line, i ? "intentional bootstrap skip" : "want value=90, got value=7")) found = true;
        fclose(input);
        TEST_ASSERT_TRUE(found, "independent formatter and skip diagnostics survive reporting");
    }
}

/* Register bootstrap cases without a configuration or database service. */
int tests_main(void)
{
    const char *probe = getenv("MARS_TEST_CORE_PROBE");
    if (probe) {
        if (!strcmp(probe, "fail")) TEST_RUN_CASE(core_probe_failure, "probe");
        else TEST_RUN_CASE(core_probe_skip, "probe");
        return 0;
    }
    TEST_RUN_CASE(test_core_assertions, "core");
    TEST_RUN_CASE(test_core_resources, "core");
    TEST_RUN_CASE(test_core_nested, "core");
    TEST_RUN_CASE(test_core_registry, "core");
    TEST_RUN_CASE(test_core_subprocesses, "core");
    return 0;
}
