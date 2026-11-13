/**
 * @file test_harness_mars.c
 * @brief Optional MARS configuration and SQLite reporting adapter for native tests.
 *
 * This adapter is explicitly linked by ordinary MARS suites. The execution core uses only its plain C service
 * table and remains independently buildable. Profiles and data rows are snapshotted before callbacks; reporting
 * opens storage after execution. Storage implementation suites can disable selection independently of reporting.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__has_include)
#if __has_include(<valgrind/valgrind.h>)
#include <valgrind/valgrind.h>
#endif
#endif

#include "string-t.h"
#include "test_harness.h"
#include "test_harness_adapter.h"
#include "test_database.h"
#define MARS_SHARED_TEST_CONFIG_INTERNAL_ACCESS
#include "internal/test_config_internal.h"
#define MARS_SHARED_TEST_DATABASE_INTERNAL_ACCESS
#include "internal/test_database_internal.h"

typedef struct _test_mars_adapter_state_t {
    test_profile_t *profile; /**< Owned immutable selection snapshot, released at shutdown. */
    string_t *database_root; /**< Owned repository path. */
    string_t *database_path; /**< Owned optional test-storage path. */
    const char *profile_name; /**< Caller-owned command-line argument, valid until process exit. */
    bool database_disabled;
    bool database_selection_disabled;
    bool memory_test;

    bool plan_only;
    test_database_t *report_database;
    bool report_ok;
} test_mars_adapter_state_t;

static test_mars_adapter_state_t test_mars_state;

static bool test_database_selected(const string_t *file, const string_t *path, bool fallback)
{
    if (!test_mars_state.profile)
        return fallback;
    string_t *source = test_config_normalise_file_path(file);
    bool enabled = source && test_profile_enabled(test_mars_state.profile, string_c_str(source), string_c_str(path));
    string_free(source);
    return enabled;
}

/* A table snapshot owns the inputs until every selected synchronous callback has finished. */
static void test_adapter_run_cases(const char *file, int line, const char *module, const char *parent,
                              void (*check)(const void *data))
{
    string_t *memory = test_mars_state.database_disabled || test_mars_state.database_selection_disabled ?
        string_new_with(":memory:") : NULL, *error = NULL;
    size_t count = 0u;
    test_database_case_t *cases = test_database_cases_load(test_mars_state.database_root,
        memory ? memory : test_mars_state.database_path, module, &count, &error);
    if (!cases)
        test_mark_failure(file, line, error ? string_c_str(error) : "Cannot load module test rows.");
    else if (!count)
        test_mark_skip(file, line, "The module table has no enabled data cases.");
    const char *group = parent;
    for (size_t i = 0u; cases && i < count; ++i)
        test_run_data_case(file, line, string_c_str(cases[i].name), group, "database-case", &cases[i], check);
    test_database_cases_free(cases, count);
    string_free(error);
    string_free(memory);
}

static int test_adapter_start(int argc, char **argv, test_config_mode_t mode, bool *plan_only)
{
    test_mars_state.database_selection_disabled = !test_suite_database_selection_enabled();
#ifdef RUNNING_ON_VALGRIND
    test_mars_state.memory_test = RUNNING_ON_VALGRIND != 0;
#endif
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--help")) {
            puts("Native MARS tests: [--test-profile NAME] [--test-database PATH|off] [--test-memory] [--test-plan]\n"
                 "Without a profile, tests/test_config.json controls selection.\n"
                 "--test-plan prints the configured enabled-case count without running tests or saving results.\n"
                 "Test storage is created only by test execution or explicit test-db commands.");
            return 1;
        }
        if (!strcmp(argv[i], "--test-memory"))
            test_mars_state.memory_test = true;
        else if (!strcmp(argv[i], "--test-plan"))
            test_mars_state.plan_only = true;
        else if (!strcmp(argv[i], "--test-profile") && i + 1 < argc)
            test_mars_state.profile_name = argv[++i];
        else if (!strcmp(argv[i], "--test-database") && i + 1 < argc) {
            const char *path = argv[++i];
            if (!strcmp(path, "off"))
                test_mars_state.database_disabled = true;
            else {
                string_free(test_mars_state.database_path);
                test_mars_state.database_path = string_new_with(path);
                if (!test_mars_state.database_path)
                    return -1;
            }
        } else {
            fprintf(stderr, "Unknown or incomplete test argument: %s\n", argv[i]);
            return -1;
        }
    }
    if ((test_mars_state.database_disabled || test_mars_state.database_selection_disabled) && test_mars_state.profile_name) {
        fputs("This suite disables shared database selections; select its cases with test_config.json.\n", stderr);
        return -1;
    }
    const char *root = getenv("MARS_ROOT");
    test_mars_state.database_root = string_new_with(root && *root ? root : ".");
    if (!test_mars_state.database_root)
        return -1;
    if (test_mars_state.profile_name) {
        string_t *error = NULL;
        test_database_t *database = test_database_open(test_mars_state.database_root, test_mars_state.database_path,
                                                      false, &error);
        test_mars_state.profile = database ? test_database_profile_load(database, test_mars_state.profile_name) : NULL;
        if (!test_mars_state.profile)
            fprintf(stderr, "Cannot select requested test profile: %s\n",
                    error ? string_c_str(error) : test_database_error(database));
        string_free(error);
        test_database_close(database);
        if (!test_mars_state.profile)
            return -1;
    }
    *plan_only = test_mars_state.plan_only;
    test_config_set_mode(mode);
    return 0;
}

static bool test_adapter_selected(const char *file, const char *path, bool fallback)
{
    if (!test_mars_state.profile) return fallback;
    string_t *source = string_new_with(file), *name = string_new_with(path);
    bool enabled = source && name && test_database_selected(source, name, fallback);
    string_free(name);
    string_free(source);
    return enabled;
}

static bool test_adapter_query(const char *file, const char *name, const char *parent, bool key_only)
{
    string_t *source = string_new_with(file ? file : ""), *key = string_new_with(name ? name : "");
    string_t *group = parent && *parent ? string_new_with(parent) : NULL;
    bool ready = source && key && (!(parent && *parent) || group);
    bool result = ready ? (key_only ? test_config_has_key_for(source, key, group) :
        test_config_is_enabled(source, key, group)) : !key_only;
    if (!ready) test_mark_failure(file, 0, "Cannot allocate optional test-selection inputs.");
    string_free(group);
    string_free(key);
    string_free(source);
    return result;
}

static bool test_adapter_plan(const char *source, size_t *count)
{
    string_t *text = string_new_with(source);
    bool known = text && test_config_count_current(text, test_database_selected, count);
    string_free(text);
    return known;
}

static void test_adapter_finish_selection(bool aborted)
{
    test_config_set_prune_enabled(!aborted);
    if (test_suite_mode != TEST_CONFIG_NONE && !test_mars_state.profile && !test_mars_state.plan_only)
        test_config_save();
    test_config_shutdown();
}

static bool test_adapter_report_missing(void)
{
    return !test_mars_state.profile;
}

static bool test_adapter_report_begin(const char *executable)
{
    if (test_mars_state.database_disabled) return false;
    string_t *error = NULL;
    test_mars_state.report_database = test_database_open(test_mars_state.database_root, test_mars_state.database_path,
                                                         true, &error);
    test_mars_state.report_ok = test_mars_state.report_database &&
        test_database_run_begin(test_mars_state.report_database, executable, test_mars_state.profile_name,
                                test_mars_state.memory_test);
    if (!test_mars_state.report_ok)
        fprintf(stderr, "Test database report unavailable (console results retained): %s\n",
            error ? string_c_str(error) : test_database_error(test_mars_state.report_database));
    string_free(error);
    return test_mars_state.report_ok;
}

static void test_adapter_report_record(const test_harness_record_t *record)
{
    if (!test_mars_state.report_ok) return;
    string_t *file = string_new_with(record->source);
    string_t *source = file ? test_config_normalise_file_path(file) : NULL;
    test_database_result_t result = {.source = source ? string_c_str(source) : NULL,
        .path = record->path, .name = record->name, .parent = record->parent, .tags = record->tags,
        .detail = record->detail, .outcome = record->outcome, .declaration_line = record->declaration_line,
        .enabled = record->enabled, .is_group = record->is_group, .is_output = record->is_output,
        .elapsed_ms = record->elapsed_ms, .finished_utc = record->finished_utc[0] ? record->finished_utc : NULL};
    test_mars_state.report_ok = source && test_database_run_record(test_mars_state.report_database, &result);
    string_free(source);
    string_free(file);
}

static void test_adapter_report_finish(int exit_code, double elapsed_ms)
{
    test_database_t *database = test_mars_state.report_database;
    if (test_mars_state.report_ok) test_mars_state.report_ok = test_database_report_complete(database, exit_code, elapsed_ms);
    if (!test_mars_state.report_ok)
        fprintf(stderr, "Test database report unavailable (console results retained): %s\n", test_database_error(database));
    test_database_close(database);
    test_mars_state.report_database = NULL;
}

static void test_adapter_release(void)
{
    test_database_close(test_mars_state.report_database);
    test_profile_free(test_mars_state.profile);
    string_free(test_mars_state.database_path);
    string_free(test_mars_state.database_root);
    memset(&test_mars_state, 0, sizeof test_mars_state);
}

/* Supply storage/configuration services only when this optional object is explicitly linked. */
const test_harness_adapter_t *test_harness_adapter(void)
{
    static const test_harness_adapter_t adapter = {
        .start = test_adapter_start, .plan = test_adapter_plan, .query = test_adapter_query,
        .selected = test_adapter_selected, .finish_selection = test_adapter_finish_selection,
        .report_missing = test_adapter_report_missing, .report_begin = test_adapter_report_begin,
        .report_record = test_adapter_report_record, .report_finish = test_adapter_report_finish,
        .release = test_adapter_release, .run_cases = test_adapter_run_cases
    };
    return &adapter;
}
