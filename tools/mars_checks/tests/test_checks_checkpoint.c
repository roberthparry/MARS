/**
 * @file test_checks_checkpoint.c
 * @brief Stateful regressions for persistent verification reuse and invalidation.
 *
 * Disposable repositories contain their own test schema, selections and marker programmes. Repeated commands prove
 * that only successes are reused, fresh failures invalidate old passes, changed builds/selections rerun, and README
 * examples resume independently. No shared checkpoint, production selection or application resource is modified.
 */
#include <stdio.h>
#include <string.h>

#include "file-t.h"
#include "checks_process.h"
#include "checks_readme.h"
#include "test_harness.h"
#include "test_database.h"
#define MARS_SHARED_TEST_DATABASE_INTERNAL_ACCESS
#include "internal/test_database_internal.h"
#include "test_checks.h"

static bool test_checkpoint_repository(const string_t *root)
{
    string_t *repository = checks_text(MARS_CHECKS_ROOT_DIR);
    string_t *path = checks_path(repository, "tests/test_database.sql"), *schema = checks_read(path);
    bool ok = schema && test_checks_write(root, "tests/test_database.sql", string_c_str(schema)) &&
              test_checks_write(root, "tests/test_config.json", "{}\n");
    string_free(schema);
    string_free(path);
    string_free(repository);
    return ok;
}

static bool test_checkpoint_executable(const string_t *root, const char *name, const char *text)
{
    bool ok = test_checks_write(root, name, text);
    string_t *path = checks_path(root, name);
    file_t *file = file_new(path);
    ok = file && file_chmod(file, 0700) && ok;
    file_free(file);
    string_free(path);
    return ok;
}

static bool test_checkpoint_count(const string_t *root, const char *name, size_t wanted)
{
    string_t *path = checks_path(root, name), *text = checks_read(path);
    bool ok = text && string_byte_length(text) == wanted * 4;
    if (!ok) fprintf(stderr, "%s: wanted %zu marker lines; got %s\n", name, wanted,
                     text ? string_c_str(text) : "no marker file");
    string_free(text);
    string_free(path);
    return ok;
}

static bool test_checkpoint_suite(const string_t *root, bool resume, bool pass, size_t count, const char *extra)
{
    string_t *path = checks_path(root, "suite");
    char *arguments[] = {"--resume", (char *)string_c_str(path), (char *)extra};
    int status = checks_test_suite(root, (extra ? 3 : 2) - !resume, arguments + !resume);
    string_free(path);
    return (pass ? status == 0 : status != 0) && test_checkpoint_count(root, "calls", count);
}

static void test_checks_checkpoint_suites(void)
{
    string_t *root = test_checks_root();
    const char *programme = "#!/bin/sh\nprintf 'run\\n' >> calls\n"
                            "if [ -e fail ]; then exit 1; fi\n"
                            "if [ -e interrupt ]; then kill -TERM $$; fi\n"
                            "printf 'COUNTS: 1 passed, 0 failed\\n'\n";
    bool ok = test_checkpoint_repository(root) && test_checkpoint_executable(root, "suite", programme) &&
        test_checkpoint_suite(root, true, true, 1, NULL) && test_checkpoint_suite(root, true, true, 1, NULL) &&
        test_checkpoint_suite(root, false, true, 2, NULL) && test_checks_write(root, "fail", "") &&
        test_checkpoint_suite(root, false, false, 3, NULL) && test_checkpoint_suite(root, true, false, 4, NULL);
    string_t *path = checks_path(root, "fail");
    file_t *flag = file_new(path);
    ok = flag && file_delete(flag) && ok;
    file_free(flag);
    string_free(path);
    ok = ok && test_checkpoint_suite(root, true, true, 5, NULL) && test_checkpoint_suite(root, true, true, 5, NULL);
    string_t *changed = string_sprintf("%s# changed executable\n", programme);
    ok = ok && test_checkpoint_executable(root, "suite", string_c_str(changed)) &&
        test_checkpoint_suite(root, true, true, 6, NULL) &&
        test_checks_write(root, "tests/test_config.json", "{\"changed\":false}\n") &&
        test_checkpoint_suite(root, true, true, 7, NULL) &&
        test_checks_write(root, "tests/test_config.json", "{\n  \"changed\": false, \"discovered\": true\n}\n") &&
        test_checkpoint_suite(root, true, true, 7, NULL) && test_checkpoint_suite(root, true, true, 8, "argument") &&
        test_checkpoint_suite(root, true, true, 8, "argument");
    test_database_t *database = test_database_open(root, NULL, true, NULL);
    ok = database && test_database_profile_create(database, "new-selection", true) && ok;
    test_database_close(database);
    ok = ok && test_checkpoint_suite(root, true, true, 9, "argument");
    database = test_database_open(root, NULL, true, NULL);
    ok = database && test_database_exec(database,
        "CREATE TABLE fixture_cases(name TEXT PRIMARY KEY,input TEXT);\n"
        "INSERT INTO fixture_cases VALUES ('case','first');\n"
        "INSERT INTO test_case_runner(module,source,parent,table_name)\n"
        "VALUES ('fixture','fixture.c','cases','fixture_cases');") && ok;
    test_database_close(database);
    ok = ok && test_checkpoint_suite(root, true, true, 10, "argument");
    database = test_database_open(root, NULL, true, NULL);
    ok = database && test_database_exec(database, "UPDATE fixture_cases SET input='changed'") && ok;
    test_database_close(database);
    ok = ok && test_checkpoint_suite(root, true, true, 11, "argument");
    database = test_database_open(root, NULL, true, NULL);
    test_database_result_t result = {.source = "fixture.c", .path = "case", .name = "case", .outcome = "pass",
                                      .enabled = true, .elapsed_ms = 2};
    ok = database && test_database_run_begin(database, "fixture", "", false) &&
         test_database_run_record(database, &result) && test_database_run_finish(database, 0, 2) && ok;
    test_database_close(database);
    ok = ok && test_checkpoint_suite(root, true, true, 11, "argument") &&
        test_checks_write(root, "interrupt", "") && test_checkpoint_suite(root, false, false, 12, "argument") &&
        test_checkpoint_suite(root, true, false, 13, "argument");
    path = checks_path(root, "interrupt");
    flag = file_new(path);
    ok = flag && file_delete(flag) && ok;
    file_free(flag);
    string_free(path);
    ok = ok && test_checkpoint_suite(root, true, true, 14, "argument") &&
        test_checkpoint_suite(root, true, true, 14, "argument");
    ok = ok && test_checkpoint_executable(root, "suite", "#!/bin/sh\nprintf 'run\\n' >> calls\n"
        "printf '# changed during execution\\n' >> \"$0\"\n") &&
        test_checkpoint_suite(root, true, true, 15, "argument") &&
        test_checkpoint_suite(root, true, true, 16, "argument") &&
        test_checkpoint_executable(root, "suite", string_c_str(changed)) &&
        test_checkpoint_suite(root, true, true, 17, "argument") &&
        test_checkpoint_suite(root, true, true, 17, "argument");
    string_free(changed);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "suite checkpoints survive commands, retry failures, invalidate inputs and ignore result history");
}

static bool test_checkpoint_readme(const string_t *root, bool resume, bool pass, size_t count)
{
    char *arguments[] = {"--resume", "--cc=./compiler", "--cflags=-std=gnu17", "--libs=", "--archive=empty.a"};
    int status = checks_readme(root, 5 - !resume, arguments + !resume);
    return (pass ? status == 0 : status != 0) && test_checkpoint_count(root, "compiles", count);
}

static void test_checks_checkpoint_readme(void)
{
    string_t *root = test_checks_root();
    const char *first = "# Examples\n\n```c\n#include <stdio.h>\n"
                        "int main(void) { puts(\"first\"); return 0; }\n```\n\n```text\nfirst\n```\n\n"
                        "```c\n#include <stdio.h>\nint main(void) { puts(\"second\"); return 0; }\n```\n\n```text\n";
    string_t *broken = string_sprintf("%swrong\n```\n", first), *correct = string_sprintf("%ssecond\n```\n", first);
    bool ok = test_checkpoint_repository(root) && test_checkpoint_executable(root, "compiler",
        "#!/bin/sh\nprintf 'run\\n' >> compiles\nexec gcc \"$@\"\n") &&
        test_checks_write(root, "empty.a", "!<arch>\n") &&
        test_checks_write(root, "docs/examples.md", string_c_str(broken)) &&
        test_checkpoint_readme(root, true, false, 2) && test_checkpoint_readme(root, true, false, 3) &&
        test_checks_write(root, "docs/examples.md", string_c_str(correct)) &&
        test_checkpoint_readme(root, true, true, 4) && test_checkpoint_readme(root, true, true, 4);
    string_t *path = checks_path(root, "build/readme-examples/results.json"), *evidence = checks_read(path);
    ok = evidence && string_find(evidence, "\"reused\": true") >= 0 && ok;
    string_free(evidence);
    string_free(path);
    ok = ok && test_checkpoint_readme(root, false, true, 6);
    string_free(correct);
    string_free(broken);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "README checkpoints retain earlier successes, retry failed examples and label reused evidence");
}

static void test_checks_checkpoint_help(void)
{
    string_t *root = test_checks_root(), *programme = checks_path(root, "suite");
    bool ok = test_checkpoint_repository(root) && test_checkpoint_executable(root, "suite",
        "#!/bin/sh\nprintf 'run\\n' >> calls\nprintf 'fixture help\\n'\n");
    for (size_t attempt = 1; ok && attempt <= 2; ++attempt) {
        checks_strings_t *arguments = checks_strings_new();
        checks_strings_add(arguments, checks_text("env"));
        checks_strings_add(arguments, string_sprintf("MARS_ROOT=%s", string_c_str(root)));
        checks_strings_add(arguments, checks_text(MARS_CHECKS_PROGRAM_PATH));
        checks_strings_add(arguments, checks_text("test-suite"));
        checks_strings_add(arguments, checks_text("--resume"));
        checks_strings_add(arguments, string_clone(programme));
        checks_strings_add(arguments, checks_text("--help"));
        string_t *output = NULL, *errors = NULL;
        int status = -1;
        ok = checks_process_run(arguments, root, 10, &output, &errors, &status) && status == 0 &&
             output && string_find(output, "fixture help") >= 0 && test_checkpoint_count(root, "calls", attempt);
        string_free(errors);
        string_free(output);
        checks_strings_free(arguments);
    }
    string_t *database = checks_path(root, "tests/tests.db");
    ok = !checks_is_file(database) && ok;
    string_free(database);
    string_free(programme);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "suite help is forwarded on every invocation and never creates verification storage");
}

/* Register checkpoint lifecycle regressions before the ordinary suite's README examples. */
void test_checks_checkpoint_cases(void)
{
    TEST_RUN_IN_GROUP(test_checks_checkpoint_suites, tests, "build,process,checkpoint");
    TEST_RUN_IN_GROUP(test_checks_checkpoint_readme, tests, "build,readme,checkpoint");
    TEST_RUN_IN_GROUP(test_checks_checkpoint_help, tests, "build,process,checkpoint");
}
