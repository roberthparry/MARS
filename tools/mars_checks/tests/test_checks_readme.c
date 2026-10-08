/**
 * @file test_checks_readme.c
 * @brief README runner integration regressions, registered last in the suite.
 *
 * Compiles tiny disposable C programmes only when the parent runs this suite.
 * Uses the same global configuration harness, with private repository fixtures
 * supplying controlled README selection settings. No repository files are edited.
 */
/* The build supplies the selected mode; retain a useful editor-only default. */
#ifndef MARS_HTTP_FIXTURE_PATH
#define MARS_HTTP_FIXTURE_PATH "tests/build/release/http/fixtures/http_fixture"
#endif

#include <errno.h>
#include <signal.h>
#include <sys/wait.h>

#include "file.h"
#include "test_harness.h"
#include "checks_process.h"
#include "checks_readme.h"
#include "../src/fixtures/checks_fixtures_internal.h"
#include "test_checks.h"

static bool test_checks_descriptor_count(size_t *count)
{
    file_t *directory = file_new_cstr("/proc/self/fd");
    bool ok = directory && file_open_directory(directory);
    *count = 0;
    /* The sequential test process has a small descriptor set. Enumerating it
     * observes real leaks without predicting the descriptor chosen by spawn. */
    while (ok) {
        file_info_t *entry = NULL;
        ok = file_read_directory(directory, &entry);
        if (!entry)
            break;
        ++*count;
        file_info_free(entry);
    }
    file_free(directory);
    return ok;
}

static void test_checks_readme_fixture_startup_cleanup(void)
{
    string_t *root = test_checks_root();
    string_t *executable = checks_path(root, "fixture-executable");
    checks_fixtures_t *fixtures = checks_fixtures_new_with_executable(root, executable);
    string_t *path = checks_text("docs/http.md");
    string_t *code = checks_text("https://httpbin.org/get?message=MARS");
    size_t baseline = 0, current = 0;
    bool ok = test_checks_descriptor_count(&baseline);
    for (unsigned attempt = 0; attempt < 16; ++attempt) {
        checks_strings_t *arguments = checks_strings_new();
        errno = 0;
        bool started = checks_fixtures_arguments(fixtures, path, code, arguments);
        int error = errno;
        ok = !started && error == ENOENT && checks_strings_count(arguments) == 0 &&
             test_checks_descriptor_count(&current) && current == baseline && ok;
        checks_strings_free(arguments);
    }
    /* A real non-executable file exercises a distinct spawn failure using the
     * same owner; no production executable or process-global limits change. */
    ok = test_checks_write(root, "fixture-executable", "not an executable\n") && ok;
    file_t *non_executable = file_new(executable);
    ok = non_executable && file_chmod(non_executable, 0600) && ok;
    file_free(non_executable);
    for (unsigned attempt = 0; attempt < 16; ++attempt) {
        checks_strings_t *arguments = checks_strings_new();
        errno = 0;
        bool started = checks_fixtures_arguments(fixtures, path, code, arguments);
        int error = errno;
        ok = !started && error == EACCES && checks_strings_count(arguments) == 0 &&
             test_checks_descriptor_count(&current) && current == baseline && ok;
        checks_strings_free(arguments);
    }
    file_t *link = file_new(executable), *target = file_new_cstr(MARS_HTTP_FIXTURE_PATH);
    bool repaired = link && target && file_delete(link) && file_create_symlink(target, link);
    file_free(target);
    file_free(link);
    checks_strings_t *arguments = checks_strings_new();
    bool recovered = repaired && checks_fixtures_arguments(fixtures, path, code, arguments);
    ok = recovered && checks_strings_count(arguments) == 1 &&
         test_checks_descriptor_count(&current) && current == baseline + 1 && ok;
    checks_strings_free(arguments);
    checks_fixtures_free(fixtures);
    ok = test_checks_descriptor_count(&current) && current == baseline && ok;
    errno = 0;
    ok = waitpid(-1, NULL, WNOHANG) < 0 && errno == ECHILD && ok;
    string_free(code);
    string_free(path);
    string_free(executable);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "README fixture ENOENT/EACCES retries leak no listeners, recover on the same owner "
                         "and reap the recovered child");
}

static bool test_checks_readme_result(const string_t *root, size_t count, const char *error)
{
    string_t *path = checks_path(root, "build/readme-examples/results.json");
    json_t *records = json_from_file(path);
    string_free(path);
    bool ok = records && json_array_size(records) == count;
    if (count && records) {
        const json_t *record = json_array_get(records, 0);
        const json_t *message = checks_member(record, "error");
        ok = (error ? message && checks_equal(json_string_value(message), error) : !message) && ok;
    }
    json_free(records);
    return ok;
}

static int test_checks_run_readme(const string_t *root, bool compile_only)
{
    char *arguments[] = {"--cc=cc", "--archive=/dev/null", "--libs=", "--timeout=0.1", "--compile-only"};
    return checks_readme(root, compile_only ? 5 : 4, arguments);
}

static void test_checks_readme_success_and_separate_stderr(void)
{
    string_t *root = test_checks_root();
    bool ok = test_checks_write(root, "tests/test_config.json", "{}\n") &&
              test_checks_write(root, "docs/a guide.md",
                                "```c\n#include <stdio.h>\n"
                                "int main(void) { puts(\"hello\"); fputs(\"diagnostic\\n\", stderr); return 0; }\n```\n"
                                "```text\nhello\n```\n");
    ok = test_checks_run_readme(root, false) == 0 && test_checks_readme_result(root, 1, NULL) && ok;
    string_t *path = checks_path(root, "build/readme-examples/docs_a_guide_001.stderr");
    string_t *text = checks_read(path);
    ok = checks_equal(text, "diagnostic\n") && ok;
    string_free(text);
    string_free(path);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "README example succeeds with exact stdout and separately retained stderr");
}

static void test_checks_readme_negative_results(void)
{
    const char *const programmes[] = {"call();\n",
                                      "int main(void) { syntax error }\n",
                                      "int main(void) { return 3; }\n",
                                      "int main(void) { return 0; }\n",
                                      "#include <stdio.h>\nint main(void) { puts(\"wrong\"); return 0; }\n",
                                      "#include <unistd.h>\nint main(void) { sleep(10); return 0; }\n"};
    const char *const expected_errors[] = {
        "C example has no main()",   "compile/link failure",      "program failed",
        "missing documented output", "documented output differs", "program timed out"};
    bool ok = true;
    for (size_t i = 0; i < sizeof(programmes) / sizeof(*programmes); ++i) {
        string_t *root = test_checks_root();
        string_t *markdown = string_sprintf("```c\n%s```\n%s", programmes[i], i == 3 ? "" : "```text\nexpected\n```\n");
        ok = test_checks_write(root, "tests/test_config.json", "{}\n") &&
             test_checks_write(root, "README.md", string_c_str(markdown)) && ok;
        ok = test_checks_run_readme(root, false) == 1 && test_checks_readme_result(root, 1, expected_errors[i]) && ok;
        string_free(markdown);
        ok = test_checks_finish(root) && ok;
    }
    TEST_ASSERT_TRUE(ok, "README errors distinguish incomplete source, compilation, status, "
                         "missing output, mismatch and timeout");
}

static void test_checks_readme_config_and_compile_only(void)
{
    string_t *root = test_checks_root();
    bool ok = test_checks_write(root, "tests/test_config.json",
                                "{\"tools/mars_checks/src/readme/"
                                "checks_readme.c\":{\"readme_examples\":{\"enabled\":false,\"README_002\":true}}}\n") &&
              test_checks_write(root, "README.md",
                                "```c\ncall();\n```\n"
                                "```c\nint main(void) { return 7; }\n```\n");
    ok = test_checks_run_readme(root, true) == 0 && test_checks_readme_result(root, 1, NULL) && ok;
    ok = test_checks_write(
             root, "tests/test_config.json",
             "{\"tools/mars_checks/src/readme/checks_readme.c\":{\"readme_examples\":{\"enabled\":false}}}\n") &&
         ok;
    ok = test_checks_run_readme(root, false) == 0 && test_checks_readme_result(root, 0, NULL) && ok;
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "README selection honours individual overrides and section defaults; "
                         "compile-only skips execution");
}

static void test_checks_readme_option_errors(void)
{
    string_t *root = test_checks_root();
    char *missing[] = {"--compile-only"};
    char *bad_timeout[] = {"--libs=", "--timeout=invalid"};
    char *bad_option[] = {"--libs=", "--unknown=yes"};
    bool ok = checks_readme(root, 1, missing) == 2 && checks_readme(root, 2, bad_timeout) == 2 &&
              checks_readme(root, 2, bad_option) == 2;
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "README CLI rejects missing libraries, invalid deadlines and unknown options");
}

static void test_checks_readme_interrupt_cleanup(void)
{
    file_t *fixture = file_new_cstr(MARS_HTTP_FIXTURE_PATH);
    bool available = fixture && file_check_access(fixture, false, false, true);
    file_free(fixture);
    bool ok = available;
    const int signals[] = {SIGINT, SIGTERM};
    for (size_t i = 0; available && i < 2; ++i) {
        string_t *root = test_checks_root();
        string_t *marker = checks_path(root, "working-directory");
        string_t *markdown =
            string_sprintf("```c\n#include <signal.h>\n#include <stdio.h>\n#include <unistd.h>\n"
                           "/* https://httpbin.org/get?message=MARS activates the existing local fixture. */\n"
                           "int main(void) { char cwd[4096]; FILE *f = fopen(\"%s\", \"w\");\n"
                           "if (!f || !getcwd(cwd, sizeof(cwd))) return 2;\n"
                           "if (fputs(cwd, f) < 0 || fclose(f)) return 3;\n"
                           "kill(getppid(), %d); for (;;) pause(); }\n```\n"
                           "```c\nint main(void) { return 0; }\n```\n```text\n```\n",
                           string_c_str(marker), signals[i]);
        bool ready = test_checks_write(root, "tests/test_config.json", "{}\n") &&
                     test_checks_write(root, "README.md", string_c_str(markdown));
        bool active = ready && checks_process_interrupt_begin();
        char *arguments[] = {"--cc=cc", "--archive=/dev/null", "--libs=", "--timeout=5"};
        int status = active ? checks_readme(root, 4, arguments) : -1;
        if (active)
            checks_process_interrupt_end();
        ok = active && status == 128 + signals[i] &&
             test_checks_readme_result(root, 1, "could not execute programme") && ok;
        string_t *working = checks_read(marker);
        file_t *directory = working ? file_new(working) : NULL;
        ok = directory && !file_exists(directory) && ok;
        file_free(directory);
        string_free(working);
        /* Sequential execution owns no unrelated children: both the interrupted
         * programme and the lazily started HTTP fixture must already be reaped. */
        pid_t remaining = waitpid(-1, NULL, WNOHANG);
        ok = remaining < 0 && errno == ECHILD && ok;
        string_free(markdown);
        string_free(marker);
        ok = test_checks_finish(root) && ok;
    }
    TEST_ASSERT_TRUE(ok, "README SIGINT/SIGTERM stop iteration, remove the working directory, reap the HTTP fixture "
                         "and return the interrupted exit status");
}

/* Register README cases after all ordinary correctness and coverage tests. */
void test_checks_readme_cases(void)
{
    TEST_RUN_IN_GROUP(test_checks_readme_fixture_startup_cleanup, tests, NULL);
    TEST_RUN_IN_GROUP(test_checks_readme_option_errors, readme_examples, NULL);
    TEST_RUN_IN_GROUP(test_checks_readme_config_and_compile_only, readme_examples, NULL);
    TEST_RUN_IN_GROUP(test_checks_readme_negative_results, readme_examples, NULL);
    TEST_RUN_IN_GROUP(test_checks_readme_interrupt_cleanup, readme_examples, NULL);
    TEST_RUN_IN_GROUP(test_checks_readme_success_and_separate_stderr, readme_examples, NULL);
}
