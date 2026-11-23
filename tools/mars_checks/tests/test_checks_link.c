/**
 * @file test_checks_link.c
 * @brief Disposable regressions for rebuilding and launching the native test runner.
 *
 * Exercises the maintained checker Makefile with a deliberately paused linker, proving that a published executable
 * remains usable during both successful and failed replacements. The root test recipe is also exercised in isolation:
 * an unavailable runner must stop the command promptly, while ordinary suite failures must retain later coverage.
 * All programmes, permission changes and handshakes belong to temporary fixture trees; no host executable is altered.
 */
#include <stdio.h>
#include <string.h>

#include "array-t.h"
#include "file-t.h"
#include "checks_process.h"
#include "test_harness.h"
#include "test_checks.h"

static bool test_link_executable(const string_t *root, const char *relative)
{
    string_t *path = checks_path(root, relative);
    file_t *file = file_new(path);
    bool ok = file && file_chmod(file, 0700);
    file_free(file);
    string_free(path);
    return ok;
}

static checks_strings_t *test_link_make_arguments(void)
{
    checks_strings_t *arguments = checks_strings_new();
    checks_strings_add(arguments, checks_text("env"));
    checks_strings_add(arguments, checks_text("-u"));
    checks_strings_add(arguments, checks_text("MAKEFLAGS"));
    checks_strings_add(arguments, checks_text("-u"));
    checks_strings_add(arguments, checks_text("MFLAGS"));
    checks_strings_add(arguments, checks_text("make"));
    checks_strings_add(arguments, checks_text("--no-print-directory"));
    checks_strings_add(arguments, checks_text("-j1"));
    return arguments;
}

static bool test_link_version(const string_t *root, const char *version)
{
    checks_strings_t *arguments = checks_strings_new();
    checks_strings_add(arguments, checks_path(root, "tools/mars_checks/build/release/mars_checks"));
    string_t *output = NULL, *errors = NULL;
    int status = -1;
    bool ran = checks_process_run(arguments, root, 5, &output, &errors, &status);
    bool ok = ran && status == 0 && output && !strcmp(string_c_str(output), version);
    string_free(errors);
    string_free(output);
    checks_strings_free(arguments);
    return ok;
}

struct test_link_observation {
    const string_t *root;
    bool observed;
    bool usable;
    bool released;
};

static void test_link_observe(const string_t *capture, double elapsed, void *context)
{
    (void)elapsed;
    struct test_link_observation *observation = context;
    if (observation->observed)
        return;
    string_t *output = checks_read(capture);
    bool ready = output && string_find(output, "LINK-READY\n") >= 0;
    string_free(output);
    if (!ready)
        return;
    observation->observed = true;
    observation->usable = test_link_version(observation->root, "old\n");
    observation->released = test_checks_write(observation->root, "continue", "");
}

static bool test_link_replacement(bool fail)
{
    string_t *root = test_checks_root(), *repository = checks_text(MARS_CHECKS_ROOT_DIR);
    string_t *path = checks_path(repository, "tools/mars_checks/Makefile"), *makefile = checks_read(path);
    const char *fixture =
        ".DEFAULT_GOAL := native-checks\n"
        "BUILD_DIR := build/release\n"
        "CC := ./compiler\n"
        "include tools/mars_checks/Makefile\n";
    const char *compiler =
        "#!/bin/sh\n"
        "set -eu\n"
        "output=\n"
        "while [ \"$#\" -gt 0 ]; do\n"
        "    if [ \"$1\" = -o ]; then shift; output=$1; fi\n"
        "    shift\n"
        "done\n"
        "test -n \"$output\"\n"
        "printf '#!/bin/sh\\nprintf \"new\\\\n\"\\n' > \"$output\"\n"
        "chmod 600 \"$output\"\n"
        "printf 'LINK-READY\\n'\n"
        "while [ ! -e continue ]; do sleep 0.05; done\n"
        "if [ -e fail ]; then exit 7; fi\n"
        "chmod 700 \"$output\"\n";
    bool ok = makefile && test_checks_write(root, "tools/mars_checks/Makefile", string_c_str(makefile)) &&
              test_checks_write(root, "Makefile", fixture) && test_checks_write(root, "compiler", compiler) &&
              test_link_executable(root, "compiler") &&
              test_checks_write(root, "tools/mars_checks/build/release/mars_checks", "#!/bin/sh\nprintf 'old\\n'\n") &&
              test_link_executable(root, "tools/mars_checks/build/release/mars_checks");
    if (fail)
        ok = test_checks_write(root, "fail", "") && ok;
    checks_strings_t *arguments = test_link_make_arguments();
    checks_strings_add(arguments, checks_text("-B"));
    checks_strings_add(arguments, checks_text("native-checks"));
    string_t *output = NULL, *errors = NULL;
    int status = -1;
    struct test_link_observation observation = {.root = root};
    bool ran = ok && checks_process_run_observed(arguments, root, 10, &output, &errors, &status,
                                                 test_link_observe, &observation);
    ok = ran && (fail ? status > 0 : status == 0) && observation.observed && observation.usable &&
         observation.released && test_link_version(root, fail ? "old\n" : "new\n");
    string_t *directory_path = checks_path(root, "tools/mars_checks/build/release");
    file_t *directory = file_new(directory_path);
    array_t *entries = directory ? file_list_directory(directory) : NULL;
    ok = entries && array_size(entries) == 1 && ok;
    if (!ok)
        fprintf(stderr, "Replacement fail=%d: ran=%d status=%d observed=%d usable=%d released=%d\n%s\n%s\n",
                fail, ran, status, observation.observed, observation.usable, observation.released,
                output ? string_c_str(output) : "", errors ? string_c_str(errors) : "");
    array_destroy(entries);
    file_free(directory);
    string_free(directory_path);
    string_free(errors);
    string_free(output);
    checks_strings_free(arguments);
    string_free(makefile);
    string_free(path);
    string_free(repository);
    return test_checks_finish(root) && ok;
}

static void test_checks_link_replacement(void)
{
    bool success = test_link_replacement(false);
    bool failure = test_link_replacement(true);
    TEST_ASSERT_TRUE(success && failure, "published checker remains executable while linking and after linker failure");
}

static bool test_link_runner(unsigned mode)
{
    string_t *root = test_checks_root(), *repository = checks_text(MARS_CHECKS_ROOT_DIR);
    string_t *path = checks_path(repository, "Makefile"), *source = checks_read(path);
    const char *text = source ? string_c_str(source) : "";
    const char *start = strstr(text, "\ntest: check-public-distribution $(TEST_BINS)\n");
    const char *recipe = start ? strchr(start + 1, '\n') + 1 : NULL;
    const char *end = recipe ? strstr(recipe, "\n\nmemtest:") : NULL;
    string_t *makefile = checks_text("CHECKS_BIN := ./runner\nTEST_BINS := first second\n"
                                   "README_EXAMPLE_ARGS := readme-examples\n"
                                   "README_EXAMPLE_CHECK := ./runner readme-examples\ntest:\n");
    bool ok = recipe && end;
    if (ok) {
        string_t *body = checks_slice(source, (size_t)(recipe - text), (size_t)(end - recipe + 1));
        checks_append(makefile, body);
        string_free(body);
    }
    string_t *runner = string_sprintf("#!/bin/sh\nprintf '%%s\\n' \"$1\"\n"
        "if [ \"$1\" = source-policy ]; then\n"
        "    if [ %u -eq 2 ]; then chmod 600 \"$0\"; fi\n"
        "    if [ %u -eq 3 ]; then exit 127; fi\n"
        "fi\n"
        "if [ \"$1\" = test-suite ]; then exit 1; fi\n", mode, mode);
    ok = test_checks_write(root, "Makefile", string_c_str(makefile)) &&
         test_checks_write(root, "runner", string_c_str(runner)) && (mode == 1 || test_link_executable(root, "runner")) && ok;
    checks_strings_t *arguments = test_link_make_arguments();
    checks_strings_add(arguments, checks_text("test"));
    string_t *output = NULL, *errors = NULL;
    int status = -1;
    bool ran = ok && checks_process_run(arguments, root, 10, &output, &errors, &status);
    const char *expected = mode == 0 ? "source-policy\ntest-suite\ntest-suite\nreadme-examples\n" :
                           mode == 1 ? "" : "source-policy\n";
    ok = ran && status > 0 && output && !strcmp(string_c_str(output), expected) &&
         (mode == 0 || (errors && string_find(errors, "Cannot execute test runner") >= 0));
    if (!ok)
        fprintf(stderr, "Runner mode=%u status=%d:\n%s\n%s\n", mode, status,
                output ? string_c_str(output) : "", errors ? string_c_str(errors) : "");
    string_free(errors);
    string_free(output);
    checks_strings_free(arguments);
    string_free(runner);
    string_free(makefile);
    string_free(source);
    string_free(path);
    string_free(repository);
    return test_checks_finish(root) && ok;
}

static void test_checks_runner_failure(void)
{
    bool ok = true;
    for (unsigned mode = 0; mode < 4; ++mode)
        ok = test_link_runner(mode) && ok;
    TEST_ASSERT_TRUE(ok, "unavailable checker stops immediately; ordinary suite failures retain later tests and README checks");
}

/* Register process lifecycle checks before the README examples. */
void test_checks_link_cases(void)
{
    TEST_RUN_IN_GROUP(test_checks_link_replacement, tests, "build,process");
    TEST_RUN_IN_GROUP(test_checks_runner_failure, tests, "build,process");
}
