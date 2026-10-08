/**
 * @file test_checks_controls.c
 * @brief Isolated compliance, distribution, file and subprocess regressions.
 *
 * Uses disposable trees, real Git indexes without commits and known SHA-256
 * vectors. Positive and negative policies are checked independently of the
 * working repository. Subprocess tests use argument vectors without a shell.
 * A native fork fixture and Linux subreaper verify descendant termination and
 * reap adopted children before restoring the caller's subreaper setting.
 */
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "file.h"
#include "test_harness.h"
#include "checks_process.h"
#include "checks_controls.h"
#include "test_checks.h"

static string_t *test_checks_inventory(void)
{
    return checks_text("SPDXVersion: SPDX-2.3\nDataLicense: CC0-1.0\nSPDXID: SPDXRef-DOCUMENT\n"
                       "SPDXID: SPDXRef-Package-MARS\nSPDXID: SPDXRef-Package-SQLCipher\n"
                       "SPDXID: SPDXRef-Package-SQLite\nSPDXID: SPDXRef-Package-TZDB\n"
                       "SPDXID: SPDXRef-Package-Unicode-CLDR\nSPDXID: SPDXRef-Package-WeatherAPI\n"
                       "SPDXID: SPDXRef-Package-DE440\nSPDXID: SPDXRef-Package-DE440s\n"
                       "SPDXID: SPDXRef-Package-NAIF-Auxiliary-Kernels\n"
                       "Relationship: SPDXRef-DOCUMENT DESCRIBES SPDXRef-Package-MARS\n");
}

static void test_checks_spdx_valid_and_invalid(void)
{
    string_t *source = test_checks_inventory();
    checks_strings_t *errors = checks_strings_new();
    checks_spdx(source, errors);
    bool ok = checks_strings_count(errors) == 0;
    string_append_cstr(source, "SPDXID: SPDXRef-Package-MARS\nRelationship: missing USES absent\n");
    checks_spdx(source, errors);
    ok = checks_strings_count(errors) == 3 && ok;
    checks_strings_free(errors);
    string_free(source);
    source = checks_text("SPDXVersion: SPDX-2.2\nDataLicense: private\n");
    errors = checks_strings_new();
    checks_spdx(source, errors);
    ok = checks_strings_count(errors) == 11 && ok;
    checks_strings_free(errors);
    string_free(source);
    TEST_ASSERT_TRUE(ok,
                     "SPDX checks versions, licensing, duplicates, mandatory packages and both relationship endpoints");
}

static void test_checks_installation_variables(void)
{
    string_t *source = checks_text("LEGAL_ROOT_DOCUMENTS := DEPENDENCIES.spdx LICENSE THIRD_PARTY_NOTICES.md\n"
                                   "LEGAL_GUIDE_DOCUMENTS := docs/almanac-data-provenance.md docs/compliance-status.md "
                                   "docs/licensing.md docs/privacy.md docs/visual-asset-provenance.md\n");
    checks_strings_t *errors = checks_strings_new();
    checks_installed_documents(source, errors);
    bool ok = checks_strings_count(errors) == 0;
    string_replace(source, "LICENSE ", "");
    checks_installed_documents(source, errors);
    ok = checks_strings_count(errors) == 1 &&
         checks_equal(checks_strings_get(errors, 0), "legal root document is not installed: LICENSE") && ok;
    checks_strings_free(errors);
    string_free(source);
    TEST_ASSERT_TRUE(ok, "installation variables recognise tokens and reject missing required documents");
}

static void test_checks_checksum_rows(void)
{
    string_t *root = test_checks_root();
    bool ok = test_checks_write(root, "data/sample", "abc");
    string_t *path = checks_path(root, "data/sample");
    string_t *digest = checks_sha256(path);
    ok = checks_equal(digest, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") && ok;
    string_t *rows =
        checks_text("| `data/sample` | `ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad` |\n");
    checks_strings_t *errors = checks_strings_new();
    ok = checks_provenance(root, rows, errors) && !checks_strings_count(errors) && ok;
    ok = test_checks_write(root, "data/sample", "changed") && ok;
    ok = checks_provenance(root, rows, errors) && checks_strings_count(errors) == 1 && ok;
    string_replace(rows, "data/sample", "data/absent");
    ok = checks_provenance(root, rows, errors) && checks_strings_count(errors) == 2 && ok;
    string_clear(rows);
    ok = checks_provenance(root, rows, errors) && checks_strings_count(errors) == 3 && ok;
    string_free(rows);
    string_free(digest);
    string_free(path);
    checks_strings_free(errors);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "known digest, changed data, missing provenance files and absent rows are distinguished");
}

static void test_checks_private_path_policy(void)
{
    const char *const paths[] = {"docs/books/esaa/chapter.md",
                                 ".\\.\\DOCS\\BOOKS\\ESAA\\chapter.md",
                                 "DOCS/EXPLANATORY SUPPLEMENT TO THE ASTRONOMICAL ALMANAC.MD",
                                 "src/almanac/explanatory supplement to the astronomical almanac.pdf",
                                 "docs/books/eſaa/chapter.md",
                                 "docs/books/esaa",
                                 "docs/books/esaa-public/a.md",
                                 "other/docs/books/esaa/a.md",
                                 "README.md"};
    bool ok = true;
    for (size_t i = 0; i < sizeof(paths) / sizeof(*paths); ++i) {
        string_t *path = checks_text(paths[i]);
        ok = (checks_forbidden_path(path) == (i < 5)) && ok;
        string_free(path);
    }
    TEST_ASSERT_TRUE(ok,
                     "private policy normalises separators, prefixes and case without rejecting adjacent public paths");
}

static bool test_checks_run_words(const string_t *root, const char *command)
{
    string_t *text = checks_text(command);
    checks_strings_t *arguments = checks_strings_new();
    bool ok = checks_shell_words(text, arguments);
    string_t *output = NULL, *errors = NULL;
    int status = -1;
    ok = ok && checks_process_run(arguments, root, 5, &output, &errors, &status) && status == 0;
    string_free(text);
    string_free(output);
    string_free(errors);
    checks_strings_free(arguments);
    return ok;
}

static void test_checks_git_index_and_staged_paths(void)
{
    string_t *root = test_checks_root();
    bool ok = test_checks_run_words(root, "git init --quiet") && test_checks_write(root, "README.md", "public\n") &&
              test_checks_run_words(root, "git add -- README.md");
    ok = ok && checks_public_distribution(root, false) == 0;
    ok = test_checks_write(root, "docs/books/esaa/private\nname.md", "private\n") && ok;
    ok = test_checks_run_words(root, "git add -- docs/books/esaa") && ok;
    checks_strings_t *paths = checks_git_paths(root, true);
    ok = paths && checks_strings_count(paths) == 2 && checks_strings_has(paths, "docs/books/esaa/private\nname.md") &&
         ok;
    ok = checks_public_distribution(root, true) == 1 && checks_public_distribution(root, false) == 1 && ok;
    checks_strings_free(paths);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok,
                     "NUL-delimited Git paths preserve embedded newlines and both index modes reject private files");
}

static void test_checks_process_capture_timeout_and_errors(void)
{
    checks_strings_t *arguments = checks_strings_new();
    checks_strings_add(arguments, checks_text("/usr/bin/printf"));
    checks_strings_add(arguments, checks_text("%s"));
    checks_strings_add(arguments, checks_text("literal ; $(touch unwanted)"));
    string_t *output = NULL, *errors = NULL;
    int status = -1;
    bool ok = checks_process_run(arguments, NULL, 3, &output, &errors, &status) && !status &&
              checks_equal(output, "literal ; $(touch unwanted)") && checks_equal(errors, "");
    string_free(output);
    string_free(errors);
    checks_strings_free(arguments);
    arguments = checks_strings_new();
    checks_strings_add(arguments, checks_text("/bin/sleep"));
    checks_strings_add(arguments, checks_text("10"));
    bool ran = checks_process_run(arguments, NULL, 0.03, &output, &errors, &status);
    ok = !ran && errno == ETIMEDOUT && status < 0 && ok;
    string_free(output);
    string_free(errors);
    checks_strings_free(arguments);
    arguments = checks_strings_new();
    checks_strings_add(arguments, checks_text("/no/such/checks-executable"));
    ran = checks_process_run(arguments, NULL, 1, &output, &errors, &status);
    ok = !ran && status == -1 && ok;
    string_free(output);
    string_free(errors);
    checks_strings_free(arguments);
    TEST_ASSERT_TRUE(ok, "subprocess arguments are literal, timeouts terminate children and missing executables fail");
}

static bool test_checks_reap_descendant(pid_t descendant)
{
    if (descendant <= 1)
        return false;
    for (unsigned attempt = 0; attempt < 200; ++attempt) {
        int status = 0;
        pid_t result = waitpid(descendant, &status, WNOHANG);
        if (result == descendant)
            return WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL;
        if (result < 0 && errno != EINTR)
            return false;
        struct timespec delay = {.tv_nsec = 10000000};
        nanosleep(&delay, NULL);
    }
    /* Failure cleanup must still terminate and reap the test's adopted child. */
    kill(descendant, SIGKILL);
    while (waitpid(descendant, NULL, 0) < 0 && errno == EINTR) {
    }
    return false;
}

static void test_checks_process_descendant_cleanup(void)
{
    int previous_subreaper = 0;
    bool configured = prctl(PR_GET_CHILD_SUBREAPER, &previous_subreaper) == 0;
    configured = configured && prctl(PR_SET_CHILD_SUBREAPER, 1) == 0;
    bool ok = configured;
    for (size_t mode = 0; configured && mode < 4; ++mode) {
        checks_strings_t *arguments = checks_strings_new();
        checks_strings_add(arguments, checks_text(MARS_CHECKS_PROCESS_FIXTURE_PATH));
        for (size_t i = 0; i < mode; ++i)
            checks_strings_add(arguments, checks_text("fixture-mode"));
        string_t *output = NULL, *errors = NULL;
        int status = -1;
        bool ran = checks_process_run(arguments, NULL, mode == 2 ? 2 : 5, &output, &errors, &status);
        int execution_error = errno;
        long reported = output ? strtol(string_c_str(output), NULL, 10) : -1;
        pid_t descendant = reported > 1 && reported <= INT_MAX ? (pid_t)reported : -1;
        bool reaped = test_checks_reap_descendant(descendant);
        bool expected = mode < 2    ? ran && status == (mode == 1 ? 7 : 0)
                        : mode == 2 ? !ran && execution_error == ETIMEDOUT && status == -SIGKILL
                                    : !ran && execution_error == EILSEQ && status == 0;
        ok = reaped && expected && ok;
        string_free(output);
        string_free(errors);
        checks_strings_free(arguments);
    }
    if (configured)
        ok = prctl(PR_SET_CHILD_SUBREAPER, previous_subreaper) == 0 && ok;
    TEST_ASSERT_TRUE(ok, "owned descendants receive SIGKILL after success, failed exit, timeout and capture failure; "
                         "the leader's exit status is preserved");
}

static void test_checks_process_interrupt_cleanup(void)
{
    int previous_subreaper = 0;
    bool configured = prctl(PR_GET_CHILD_SUBREAPER, &previous_subreaper) == 0;
    configured = configured && prctl(PR_SET_CHILD_SUBREAPER, 1) == 0;
    bool ok = configured;
    for (size_t mode = 0; configured && mode < 2; ++mode) {
        int signal_number = mode ? SIGTERM : SIGINT;
        struct sigaction before, after;
        bool inspected = sigaction(signal_number, NULL, &before) == 0;
        bool active = inspected && checks_process_interrupt_begin();
        checks_strings_t *arguments = checks_strings_new();
        checks_strings_add(arguments, checks_text(MARS_CHECKS_PROCESS_FIXTURE_PATH));
        for (size_t i = 0; i < 4 + mode; ++i)
            checks_strings_add(arguments, checks_text("interrupt-mode"));
        string_t *output = NULL, *errors = NULL;
        int status = -1;
        bool ran = active && checks_process_run(arguments, NULL, 5, &output, &errors, &status);
        int execution_error = errno;
        long reported = output ? strtol(string_c_str(output), NULL, 10) : -1;
        pid_t descendant = reported > 1 && reported <= INT_MAX ? (pid_t)reported : -1;
        bool reaped = active && test_checks_reap_descendant(descendant);
        ok = active && !ran && execution_error == ECANCELED && status == -SIGKILL && reaped &&
             checks_process_interrupt_signal() == signal_number && ok;
        string_free(output);
        string_free(errors);
        if (active) {
            ran = checks_process_run(arguments, NULL, 5, &output, &errors, &status);
            ok = !ran && errno == ECANCELED && status == -1 && !output && !errors && ok;
            string_free(output);
            string_free(errors);
            checks_process_interrupt_end();
            bool restored = sigaction(signal_number, NULL, &after) == 0;
            ok = restored && after.sa_handler == before.sa_handler && !checks_process_interrupt_signal() && ok;
        }
        checks_strings_free(arguments);
    }
    if (configured)
        ok = prctl(PR_SET_CHILD_SUBREAPER, previous_subreaper) == 0 && ok;
    TEST_ASSERT_TRUE(ok, "SIGINT and SIGTERM cancel execution, kill owned descendants, reject further spawns "
                         "and restore the caller's signal dispositions");
}

static void test_checks_markdown_command_failures(void)
{
    string_t *root = test_checks_root();
    bool ok = test_checks_write(root, "include/file.h", "int file_open(void);\n") &&
              test_checks_write(root, "docs/file.md", "`file_open()`\n");
    ok = checks_markdown_api(root) == 0 && ok;
    ok = test_checks_write(root, "docs/file.md", "`file_open_extra()`\n") && ok;
    ok = checks_markdown_api(root) == 1 && ok;
    ok = test_checks_write(root, "include/unknown.h", "int unsupported(void);\n") && ok;
    ok = checks_markdown_api(root) == 1 && ok;
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "command accepts covered functions and rejects absent mentions and unassigned headers");
}

static void test_checks_compliance_allow_untracked_and_safeguards(void)
{
    string_t *root = test_checks_root();
    const char *const required[] = {
        ".githooks/pre-commit",
        "LICENSE",
        "docs/compliance-status.md",
        "docs/licensing.md",
        "docs/visual-asset-provenance.md",
        "tools/mars_checks/Makefile",
        "tools/mars_checks/include/checks_controls.h",
        "tools/mars_checks/include/checks_coverage.h",
        "tools/mars_checks/include/checks_evidence.h",
        "tools/mars_checks/include/checks_fixtures.h",
        "tools/mars_checks/include/checks_policy.h",
        "tools/mars_checks/include/checks_process.h",
        "tools/mars_checks/include/checks_readme.h",
        "tools/mars_checks/include/checks_support.h",
        "tools/mars_checks/src/app/checks_main.c",
        "tools/mars_checks/src/compliance/checks_compliance.c",
        "tools/mars_checks/src/coverage/checks_coverage.c",
        "tools/mars_checks/src/distribution/checks_distribution.c",
        "tools/mars_checks/src/evidence/checks_evidence.c",
        "tools/mars_checks/src/evidence/evidence_libraries.c",
        "tools/mars_checks/src/evidence/evidence_private.h",
        "tools/mars_checks/src/evidence/evidence_probes.c",
        "tools/mars_checks/src/fixtures/checks_fixtures.c",
        "tools/mars_checks/src/markdown/checks_markdown.c",
        "tools/mars_checks/src/policy/checks_function_tables.c",
        "tools/mars_checks/src/policy/checks_inline.c",
        "tools/mars_checks/src/policy/checks_policy.c",
        "tools/mars_checks/src/process/checks_process.c",
        "tools/mars_checks/src/readme/checks_examples.c",
        "tools/mars_checks/src/readme/checks_readme.c",
        "tools/mars_checks/src/support/checks_support.c",
    };
    bool ok = true;
    for (size_t i = 0; i < sizeof(required) / sizeof(*required); ++i)
        ok = test_checks_write(root, required[i], "fixture\n") && ok;
    string_t *inventory = test_checks_inventory();
    ok = test_checks_write(root, "DEPENDENCIES.spdx", string_c_str(inventory)) && ok;
    string_free(inventory);
    ok = test_checks_write(root, "Makefile",
                           "LEGAL_ROOT_DOCUMENTS := DEPENDENCIES.spdx LICENSE THIRD_PARTY_NOTICES.md\n"
                           "LEGAL_GUIDE_DOCUMENTS := docs/almanac-data-provenance.md docs/compliance-status.md "
                           "docs/licensing.md docs/privacy.md docs/visual-asset-provenance.md\n") &&
         ok;
    ok = test_checks_write(root, "data", "abc") &&
         test_checks_write(root, "docs/almanac-data-provenance.md",
                           "| `data` | `ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad` |\n") &&
         ok;
    ok = test_checks_write(
             root, "THIRD_PARTY_NOTICES.md",
             "SQLCipher Community Edition\nUnicode CLDR week data\nAstronomical data and generation tools\n"
             "WeatherAPI.com\nhttps://www.weatherapi.com/terms.aspx\nhttps://naif.jpl.nasa.gov/naif/rules.html\n") &&
         ok;
    ok = test_checks_write(root, "docs/privacy.md", "WeatherAPI\n") &&
         test_checks_write(root, "tools/mars_lab/src/calendar/lab_calendar_weather.c", "MARS_WEATHER_API_KEY\n") &&
         test_checks_write(root, "tools/mars_lab/assets/index.html",
                           "Weather is informational and must not be the sole basis for safety-critical decisions.\n"
                           "https://www.weatherapi.com/\nhttps://www.weatherapi.com/privacy.aspx\n"
                           "https://www.weatherapi.com/terms.aspx\n") &&
         ok;
    ok = checks_compliance(root, true, true) == 0 && ok;
    ok = test_checks_run_words(root, "git init --quiet") && ok;
    ok = checks_compliance(root, false, true) == 1 && ok;
    ok = test_checks_run_words(root, "git add -- .") && checks_compliance(root, false, true) == 0 && ok;
    /* Every application control must remain tracked, and --allow-untracked must
     * still require the file itself. Each removal affects only this private index. */
    for (size_t i = 0; i < sizeof(required) / sizeof(*required); ++i) {
        string_t *relative = checks_text(required[i]);
        bool control = string_starts_with(relative, "tools/mars_checks/");
        string_free(relative);
        if (!control)
            continue;
        string_t *command = string_sprintf("git rm --cached --quiet -- %s", required[i]);
        ok = test_checks_run_words(root, string_c_str(command)) && ok;
        string_free(command);
        ok = checks_compliance(root, false, true) == 1 && checks_compliance(root, true, true) == 0 && ok;
        string_t *path = checks_path(root, required[i]);
        file_t *file = file_new(path);
        ok = file && file_delete(file) && ok;
        file_free(file);
        string_free(path);
        ok = checks_compliance(root, true, true) == 1 && ok;
        ok = test_checks_write(root, required[i], "fixture\n") && ok;
        command = string_sprintf("git add -- %s", required[i]);
        ok = test_checks_run_words(root, string_c_str(command)) && ok;
        string_free(command);
    }
    ok = checks_compliance(root, false, true) == 0 && ok;
    ok = test_checks_write(root, "docs/privacy.md", "missing provider\n") && checks_compliance(root, true, true) == 1 &&
         ok;
    ok = test_checks_write(root, "tools/mars_lab/assets/index.html", "missing safeguards\n") &&
         checks_compliance(root, true, true) == 1 && ok;
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "compliance accepts native controls before tracking only with the explicit option "
                         "and enforces safeguards");
}

/* Register isolated control and execution cases. */
void test_checks_control_cases(void)
{
    TEST_RUN_IN_GROUP(test_checks_spdx_valid_and_invalid, tests, NULL);
    TEST_RUN_IN_GROUP(test_checks_installation_variables, tests, NULL);
    TEST_RUN_IN_GROUP(test_checks_checksum_rows, tests, NULL);
    TEST_RUN_IN_GROUP(test_checks_private_path_policy, tests, NULL);
    TEST_RUN_IN_GROUP(test_checks_git_index_and_staged_paths, tests, NULL);
    TEST_RUN_IN_GROUP(test_checks_process_capture_timeout_and_errors, tests, NULL);
    TEST_RUN_IN_GROUP(test_checks_process_descendant_cleanup, tests, NULL);
    TEST_RUN_IN_GROUP(test_checks_process_interrupt_cleanup, tests, NULL);
    TEST_RUN_IN_GROUP(test_checks_markdown_command_failures, tests, NULL);
    TEST_RUN_IN_GROUP(test_checks_compliance_allow_untracked_and_safeguards, tests, NULL);
}
