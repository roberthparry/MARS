/**
 * @file test_cfg_make.c
 * @brief Root Make installer dispatch and argument-forwarding regressions.
 *
 * Dry runs retain the root Makefile's goal validation. Real recipe runs replace
 * CONFIG_PROGRAM with this executable in a private self-fixture mode, avoiding
 * another jurisdiction import. Only build/dependency prerequisites are bypassed;
 * the installer recipe and its target-specific exports remain authoritative.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "file.h"
#include "lab_process.h"
#include "test_harness.h"
#include "test_cfg_support.h"

/* Exit before the harness can save configuration or recursively register cases. */
void test_cfg_make_self_fixture(void)
{
    const char *program = getenv("MARS_TEST_MAKE_PROGRAM");
    if (!program)
        return;
    const char *location = getenv("MARS_CALENDAR_LOCATION_ARGUMENT");
    const char *language = getenv("MARS_CALENDAR_LANGUAGE_ARGUMENT");
    const char *expected_location = getenv("MARS_TEST_MAKE_LOCATION");
    const char *expected_language = getenv("MARS_TEST_MAKE_LANGUAGE");
    file_t *command = file_new_cstr("/proc/self/cmdline");
    char bytes[4096];
    size_t length = 0, prefix = strlen(program) + 1;
    bool ok = command && file_open_read(command) && file_read(command, bytes, sizeof(bytes), &length) &&
              length == prefix + sizeof("jurisdiction") && prefix <= sizeof(bytes) && !memcmp(bytes, program, prefix) &&
              !memcmp(bytes + prefix, "jurisdiction", sizeof("jurisdiction")) && location && expected_location &&
              !strcmp(location, expected_location) && language && expected_language &&
              !strcmp(language, expected_language);
    file_free(command);
    if (ok) {
        fputs("make fixture verified\n", stdout);
        fflush(stdout);
    }
    _exit(ok ? 0 : 93);
}

static bool test_cfg_make_environment(void)
{
    /* Do not inherit a parent Make jobserver, goals, overrides or injected makefile. */
    return !unsetenv("MAKEFLAGS") && !unsetenv("MFLAGS") && !unsetenv("MAKEOVERRIDES") && !unsetenv("MAKEFILES") &&
           !unsetenv("MARS_TEST_MAKE_PROGRAM");
}

static bool test_cfg_make_run(bool dry, const char *program, const char *first, const char *second, const char *third,
                              const char *location, const char *language, bool success)
{
    const char *args[24];
    size_t count = 0;
    args[count++] = "make";
    args[count++] = "--no-print-directory";
    args[count++] = "-j1";
    if (dry)
        args[count++] = "-n";
    args[count++] = "-o";
    args[count++] = "native-config";
    args[count++] = "-o";
    args[count++] = "check-deps";
    args[count++] = "JURISDICTION_RULES_SOURCES=";
    args[count++] = program;
    args[count++] = location;
    args[count++] = language;
    args[count++] = first;
    if (second)
        args[count++] = second;
    if (third)
        args[count++] = third;
    args[count] = NULL;
    string_t *output = NULL;
    int status = -1;
    bool ok = lab_proc_run(args, MARS_CONFIG_ROOT_DIR, 30000, 262144, &output, &status) &&
              (success ? status == 0 : status != 0);
    if (ok && success)
        ok = output && string_find(output, dry ? "\" jurisdiction" : "make fixture verified\n") >= 0;
    if (!ok)
        fprintf(stderr, "Make entrypoint %s %s failed (status %d):\n%s\n", first, second ? second : "", status,
                output ? string_c_str(output) : "no output");
    string_free(output);
    return ok;
}

static bool test_cfg_make_dry_fixture(const char *directory)
{
    (void)directory;
    return test_cfg_make_environment() &&
           test_cfg_make_run(true, "CONFIG_PROGRAM=/not-executed/native-config", "install-jurisdiction-db", "limmen",
                             NULL, "LOCATION=", "CALENDAR_LANGUAGE=", true) &&
           test_cfg_make_run(true, "CONFIG_PROGRAM=/not-executed/native-config", "install-jurisdiction-db", NULL, NULL,
                             "LOCATION=", "CALENDAR_LANGUAGE=", true) &&
           test_cfg_make_run(true, "CONFIG_PROGRAM=/not-executed/native-config", "install-jurisdiction-db", NULL, NULL,
                             "LOCATION=New York, US-NY", "CALENDAR_LANGUAGE=es_US", true);
}

static bool test_cfg_make_reject_fixture(const char *directory)
{
    (void)directory;
    static const char *const rejected[] = {"clean", "release-evidence", "test_jurisdiction", "../town"};
    bool ok = test_cfg_make_environment();
    for (size_t i = 0; i < sizeof(rejected) / sizeof(*rejected) && ok; ++i)
        ok = test_cfg_make_run(true, "CONFIG_PROGRAM=/not-executed/native-config", "install-jurisdiction-db",
                               rejected[i], NULL, "LOCATION=", "CALENDAR_LANGUAGE=", false);
    return ok &&
           test_cfg_make_run(true, "CONFIG_PROGRAM=/not-executed/native-config", "install-jurisdiction-db", "limmen",
                             "test", "LOCATION=", "CALENDAR_LANGUAGE=", false) &&
           test_cfg_make_run(true, "CONFIG_PROGRAM=/not-executed/native-config", "not-a-mars-target", NULL, NULL,
                             "LOCATION=", "CALENDAR_LANGUAGE=", false);
}

static bool test_cfg_make_forward_fixture(const char *directory)
{
    (void)directory;
    /* The isolated parent stays alive while Make invokes its executable through procfs. */
    string_t *executable = string_sprintf("/proc/%ld/exe", (long)getpid());
    string_t *override = executable ? string_sprintf("CONFIG_PROGRAM=%s", string_c_str(executable)) : NULL;
    bool ok = executable && override && test_cfg_make_environment() &&
              !setenv("MARS_TEST_MAKE_PROGRAM", string_c_str(executable), 1);
    static const struct {
        const char *town, *location, *language, *expected_location, *expected_language;
    } cases[] = {{"limmen", "LOCATION=", "CALENDAR_LANGUAGE=fy_NL", "limmen", "fy_NL"},
                 {NULL, "LOCATION=New York, US-NY", "CALENDAR_LANGUAGE=es_US", "New York, US-NY", "es_US"},
                 {"limmen", "LOCATION=Québec, CA-QC", "CALENDAR_LANGUAGE=French", "Québec, CA-QC", "French"},
                 {NULL, "LOCATION=", "CALENDAR_LANGUAGE=", "", ""}};
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases) && ok; ++i)
        ok = !setenv("MARS_TEST_MAKE_LOCATION", cases[i].expected_location, 1) &&
             !setenv("MARS_TEST_MAKE_LANGUAGE", cases[i].expected_language, 1) &&
             test_cfg_make_run(false, string_c_str(override), "install-jurisdiction-db", cases[i].town, NULL,
                               cases[i].location, cases[i].language, true);
    string_free(override);
    string_free(executable);
    return ok;
}

static void test_cfg_make_dry(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(test_cfg_make_dry_fixture), "root Make accepts supported installer goals");
}

static void test_cfg_make_reject(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(test_cfg_make_reject_fixture), "town shorthand cannot mask other Make targets");
}

static void test_cfg_make_forward(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(test_cfg_make_forward_fixture),
                     "actual root recipe preserves argv, town precedence, spaces, accents and language exports");
}

static bool test_cfg_make_uninstall_fixture(const char *directory)
{
    static const char *const paths[] = {
        "jurisdiction/mars_jurisdiction_rules.db", "jurisdiction/mars_jurisdiction_rules.db-journal",
        "jurisdiction/mars_jurisdiction_rules.db-wal", "jurisdiction/mars_jurisdiction_rules.db-shm",
        "config/jurisdiction-db.env", "almanac/almanac.db", "config/almanac-db.env", "config/weather.env",
        "jurisdiction/custom.db", "lab-settings.pb"
    };
    bool ok = test_cfg_make_environment();
    static const char *const directories[] = {"jurisdiction", "config", "almanac"};
    for (size_t i = 0; i < sizeof(directories) / sizeof(*directories) && ok; ++i) {
        string_t *path = string_sprintf("%s/.mars/%s", directory, directories[i]);
        file_t *folder = path ? file_new(path) : NULL;
        ok = folder && file_create_directory(folder, 0700, true);
        file_free(folder);
        string_free(path);
    }
    for (size_t i = 0; i < sizeof(paths) / sizeof(*paths) && ok; ++i) {
        string_t *path = string_sprintf("%s/.mars/%s", directory, paths[i]);
        file_t *entry = path ? file_new(path) : NULL;
        ok = entry && file_write_all_bytes(entry, "preserve", 8);
        file_free(entry);
        string_free(path);
    }
    string_t *home_argument = string_sprintf("HOME=%s", directory);
    const char *args[] = {"make", "--no-print-directory", "-j1", "uninstall-jurisdiction-db",
                          home_argument ? string_c_str(home_argument) : "", NULL};
    /* The HOME override is confined to this child Make process and its disposable fixture. */
    for (unsigned pass = 0; pass < 2 && ok; ++pass) {
        string_t *output = NULL;
        int status = -1;
        ok = home_argument && lab_proc_run(args, MARS_CONFIG_ROOT_DIR, 30000, 262144, &output, &status) && status == 0;
        string_free(output);
    }
    for (size_t i = 0; i < sizeof(paths) / sizeof(*paths) && ok; ++i) {
        string_t *path = string_sprintf("%s/.mars/%s", directory, paths[i]);
        file_t *entry = path ? file_new(path) : NULL;
        ok = entry && file_exists(entry) == (i >= 5);
        file_free(entry);
        string_free(path);
    }
    string_free(home_argument);
    return ok;
}

static void test_cfg_make_uninstall(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(test_cfg_make_uninstall_fixture),
                     "jurisdiction removal is idempotent and preserves unrelated databases and settings");
}

static bool test_cfg_make_help_fixture(const char *directory)
{
    (void)directory;
    const char *args[] = {"make", "--no-print-directory", "-j1", "help", NULL};
    string_t *output = NULL;
    int status = -1;
    bool ok = test_cfg_make_environment() &&
              lab_proc_run(args, MARS_CONFIG_ROOT_DIR, 30000, 262144, &output, &status) && status == 0 && output &&
              string_find(output, "make native-lab ") >= 0 && string_find(output, "make test_http_live ") >= 0 &&
              string_find(output, "make test_lab_native ") >= 0 && string_find(output, "make lab-browser-test ") >= 0;
    static const char *const obsolete[] = {
        "lab-workers", "lab-build", "test-http-live", "check-jurisdiction-db-deps",
        "mars_lab", "equation_lab", "diffequation_lab", "matrix_lab", "integrator_lab",
        "datetime_lab", "almanac_lab", "almanac_event_lab", "holiday_lab", "ophelia"
    };
    for (size_t i = 0; i < sizeof(obsolete) / sizeof(*obsolete) && ok; ++i) {
        ok = string_find(output, obsolete[i]) < 0;
        const char *probe[] = {"make", "--no-print-directory", "-j1", "-n", obsolete[i], NULL};
        string_t *rejected = NULL;
        status = 0;
        ok = ok && lab_proc_run(probe, MARS_CONFIG_ROOT_DIR, 30000, 262144, &rejected, &status) && status != 0;
        string_free(rejected);
    }
    string_free(output);
    return ok;
}

static void test_cfg_make_help(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(test_cfg_make_help_fixture),
                     "help lists current targets and obsolete aliases are removed");
}

/* Register ordinary Make integration cases before the README groups. */
void test_cfg_make_cases(void)
{
    TEST_RUN_IN_GROUP(test_cfg_make_dry, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_make_reject, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_make_forward, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_make_uninstall, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_make_help, tests, NULL);
}
