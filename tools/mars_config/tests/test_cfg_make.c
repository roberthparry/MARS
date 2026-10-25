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

#include "file-t.h"
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
    if (getenv("MARS_TEST_MAKE_RELEASE")) {
        bool ok = command && file_open_read(command) && file_read(command, bytes, sizeof(bytes), &length) &&
                  length == prefix + sizeof("databases-ensure") && prefix <= sizeof(bytes) &&
                  !memcmp(bytes, program, prefix) &&
                  !memcmp(bytes + prefix, "databases-ensure", sizeof("databases-ensure"));
        file_free(command);
        if (ok) { puts("release update fixture verified"); fflush(stdout); }
        _exit(ok && !getenv("MARS_TEST_MAKE_FAIL") ? 0 : 93);
    }
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
    /* Resolve in this process: under instrumentation the raw procfs link can name the checker itself. */
    file_t *self = file_new_cstr("/proc/self/exe");
    char *target = NULL;
    string_t *executable = self && file_read_link(self, &target) ? string_new_with(target) : NULL;
    free(target);
    file_free(self);
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

static bool test_cfg_make_uninstall_check(const char *directory, bool all)
{
    static const char *const paths[] = {
        "jurisdiction/mars_jurisdiction_rules.db", "jurisdiction/mars_jurisdiction_rules.db-journal",
        "jurisdiction/mars_jurisdiction_rules.db-wal", "jurisdiction/mars_jurisdiction_rules.db-shm",
        "config/jurisdiction-db.env", "almanac/almanac.db", "almanac/almanac.db-journal",
        "almanac/almanac.db-wal", "almanac/almanac.db-shm", "config/almanac-db.env",
        "globe/globe.db", "globe/globe.db-journal", "globe/globe.db-wal", "globe/globe.db-shm",
        "config/globe-db.env", "config/weather.env", "jurisdiction/custom.db", "almanac/custom.db",
        "globe/custom.db", "lab/mars_lab_state.json", "lab/mars_lab_history.db", "lab/mars_lab_history.db-journal",
        "globe/rivers-checkpoint.sqlite3"
    };
    bool ok = test_cfg_make_environment();
    static const char *const directories[] = {"jurisdiction", "config", "almanac", "globe", "lab"};
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
    const char *args[] = {"make", "--no-print-directory", "-j1",
                          all ? "uninstall-mars-db" : "uninstall-jurisdiction-db",
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
        ok = entry && file_exists(entry) == (i >= (all ? 15 : 5));
        file_free(entry);
        string_free(path);
    }
    string_free(home_argument);
    return ok;
}

static bool test_cfg_make_uninstall_fixture(const char *directory)
{
    return test_cfg_make_uninstall_check(directory, false);
}

static bool test_cfg_make_uninstall_all_fixture(const char *directory)
{
    return test_cfg_make_uninstall_check(directory, true);
}

static void test_cfg_make_uninstall(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(test_cfg_make_uninstall_fixture),
                     "jurisdiction removal is idempotent and preserves unrelated databases and settings");
}

static void test_cfg_make_uninstall_all(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(test_cfg_make_uninstall_all_fixture),
                     "combined removal is idempotent and preserves history, settings, custom stores and river checkpoints");
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
              string_find(output, "make test_lab_native ") >= 0 && string_find(output, "make lab-browser-test ") >= 0 &&
              string_find(output, "make install-mars-db ") >= 0 && string_find(output, "make uninstall-mars-db ") >= 0 &&
              string_find(output, "also install missing globe, almanac and jurisdiction databases") >= 0 &&
              string_find(output, "Missing databases are skipped") < 0 &&
              string_find(output, "Reserved; currently refuses changes") < 0;
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

static bool test_cfg_make_databases_fixture(const char *directory)
{
    (void)directory;
    const char *args[] = {"make", "--no-print-directory", "-n", "-j1", "-o", "native-config",
                         "-o", "check-native-deps", "CONFIG_PROGRAM=/not-executed/native-config",
                         "ALMANAC_RULES_SOURCES=", "JURISDICTION_RULES_SOURCES=", "install-mars-db", NULL};
    string_t *output = NULL;
    int status = -1;
    bool ok = test_cfg_make_environment() && lab_proc_run(args, MARS_CONFIG_ROOT_DIR, 30000, 1048576, &output, &status) &&
              status == 0 && output;
    ok = ok && string_find(output, "\" databases") >= 0 &&
         string_find(output, "\" globe-install") < 0 && string_find(output, "\" almanac") < 0 &&
         string_find(output, "\" jurisdiction") < 0;
    string_free(output);
    return ok;
}

static void test_cfg_make_databases(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(test_cfg_make_databases_fixture),
                     "combined database target dispatches one shared-password installer");
}

static bool test_cfg_make_release_fixture(const char *directory)
{
    file_t *self = file_new_cstr("/proc/self/exe");
    char *target = NULL;
    bool ok = test_cfg_make_environment() && self && file_read_link(self, &target);
    string_t *program = ok ? string_sprintf("CONFIG_PROGRAM=%s", target) : NULL;
    string_t *launcher_path = string_sprintf("%s/launcher", directory);
    string_t *launcher_arg = launcher_path ? string_sprintf("MARS_LAB_LAUNCHER=%s", string_c_str(launcher_path)) : NULL;
    file_t *launcher = launcher_path ? file_new(launcher_path) : NULL;
    ok = ok && program && launcher_arg && launcher && !setenv("MARS_TEST_MAKE_PROGRAM", target, 1) &&
         !setenv("MARS_TEST_MAKE_RELEASE", "1", 1) && !unsetenv("MARS_TEST_MAKE_FAIL");
    const char *args[] = {"make", "--no-print-directory", "-j1", "-o", "check-compliance",
        "MAKE=make --no-print-directory -j1 -o all -o native-config -o check-native-deps",
        program ? string_c_str(program) : "", launcher_arg ? string_c_str(launcher_arg) : "", "release", NULL};
    for (unsigned pass = 0; pass < 4 && ok; ++pass) {
        if (pass == 1) ok = file_write_all_bytes(launcher, "fixture", 7) && file_chmod(launcher, 0600);
        if (pass == 2) ok = file_chmod(launcher, 0700);
        if (pass == 3) ok = !setenv("MARS_TEST_MAKE_FAIL", "1", 1);
        string_t *output = NULL;
        int status = -1;
        ok = ok && lab_proc_run(args, MARS_CONFIG_ROOT_DIR, 30000, 262144, &output, &status) && output &&
             ((status == 0) == (pass != 3)) &&
             ((string_find(output, "release update fixture verified") >= 0) == (pass >= 2));
        if (!ok) fprintf(stderr, "Release dispatch pass %u failed: %s\n", pass, output ? string_c_str(output) : "no output");
        string_free(output);
    }
    free(target); file_free(self); file_free(launcher);
    string_free(program); string_free(launcher_arg); string_free(launcher_path);
    return ok;
}

static void test_cfg_make_release(void)
{
    TEST_ASSERT_TRUE(test_cfg_isolated(test_cfg_make_release_fixture),
                     "release updates only with an executable installed launcher and propagates migration failure");
}

/* Register ordinary Make integration cases before the README groups. */
void test_cfg_make_cases(void)
{
    TEST_RUN_IN_GROUP(test_cfg_make_release, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_make_databases, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_make_dry, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_make_reject, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_make_forward, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_make_uninstall, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_make_uninstall_all, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_make_help, tests, NULL);
}
