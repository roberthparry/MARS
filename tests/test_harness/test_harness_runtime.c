/**
 * @file test_harness_runtime.c
 * @brief Test execution, reporting and suite entry point.
 *
 * Runs registered cases, records timings, reports missing configuration keys and prints summaries. Individual
 * suites supply their registration routines while this runtime supplies the common executable main.
 * One private, process-lifetime state object owns counters, diagnostics, records, case resources and selection
 * snapshots. The harness remains sequential; nested cases save and restore their active state explicitly.
 *
 * Linked into the relevant test executables as shared support. Production code should not depend on this test-only
 * implementation.
 */

#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "test_harness.h"
#include "test_harness_adapter.h"
#define MARS_TEST_HARNESS_INTERNAL_ACCESS
#include "test_harness_internal.h"

typedef enum { TEST_OUTCOME_PASS, TEST_OUTCOME_FAIL, TEST_OUTCOME_SKIP } test_outcome_t;

typedef struct {
    test_text_t *file;
    test_text_t *name;
    test_text_t *parent;
    test_text_t *path;
    test_text_t *tags;
    test_text_t *failure_file;
    test_text_t *failure_detail;
    test_outcome_t outcome;
    bool enabled;
    int is_group;
    int is_output;
    int declaration_line;
    int failure_line;
    int group_passed;
    int group_failed;
    int group_skipped;
    double ms;
    char finished_utc[32];
} test_record_t;

typedef struct {
    test_text_t *name;
    const test_validity_contract_t *contract;
} test_validity_registry_entry_t;

typedef void (*test_cleanup_fn)(void *ctx);

typedef struct {
    test_cleanup_fn fn;
    void *ctx;
} test_cleanup_entry_t;

test_suite_setup_fn test_suite_setup_hook __attribute__((weak));
test_fixture_setup_fn test_fixture_setup_hook __attribute__((weak));
test_fixture_teardown_fn test_fixture_teardown_hook __attribute__((weak));
test_post_summary_fn test_post_summary_hook __attribute__((weak));
/* Suites that test storage override this hook before any database is opened. */
__attribute__((weak)) bool test_suite_database_selection_enabled(void)
{
    return true;
}

/** @brief Private singleton state for one sequential test process; no suite owns or exposes this object. */
typedef struct _test_runtime_t {
    int run_count;
    int group_count;
    int failure_count;
    int skip_count;
    int passed_cases;
    int failed_cases;
    double total_ms;
    int nested_run_count;
    int output_count;
    int output_pass_count;
    int output_failure_count;
    int output_skip_count;
    int missing_config_count;
    int abort_requested;

    const test_text_t *current_path; /**< Caller-owned active path, restored after a nested case. */
    test_text_t *last_fail_file; /**< Owned latest failure location, released at shutdown. */
    int last_fail_line;
    test_text_t *last_fail_detail; /**< Owned latest failure description. */
    int skip_requested;
    test_text_t *last_skip_file; /**< Owned latest skip location. */
    int last_skip_line;
    test_text_t *last_skip_detail; /**< Owned latest skip description. */
    test_text_t **missing_config_paths; /**< Owned array of owned configuration keys. */
    size_t missing_config_path_count;
    size_t missing_config_path_cap;

    test_record_t *records; /**< Owned completed records and their copied strings. */
    size_t record_count;
    size_t record_cap;
    test_validity_registry_entry_t *validity_registry; /**< Owned names; contracts remain caller-owned. */
    size_t validity_registry_count;
    size_t validity_registry_cap;
    test_cleanup_entry_t *case_cleanups; /**< Owned callback stack, unwound at the end of each case. */
    size_t case_cleanup_count;
    size_t case_cleanup_cap;
    const test_text_t *case_temp_dir; /**< Owned by the active cleanup stack; cleared when that stack unwinds. */

    const test_harness_adapter_t *adapter; /**< Optional caller-independent service table. */
    size_t planned_cases; /**< Initial selected leaf count from the module's JSON configuration. */
    size_t completed_cases; /**< Completed non-skipped leaves; disabled cases and group wrappers are excluded. */
    bool plan_known; /**< False during discovery or when configuration cannot supply a reliable total. */
    bool plan_only; /**< True prints the configuration plan without executing or reporting a test run. */
    const void *case_data; /**< Caller-owned active row, restored after a nested data case. */
    void (*data_check)(const void *); /**< Caller-owned active callback, paired with case_data. */
} test_runtime_t;

/* Static storage supplies the singleton's initial zero state without allocation or a public accessor. */
static test_runtime_t test_runtime;

static bool test_database_selected(const test_text_t *file, const test_text_t *path, bool fallback)
{
    return test_runtime.adapter && test_runtime.adapter->selected ?
        test_runtime.adapter->selected(test_text_c_str(file), test_text_c_str(path), fallback) : fallback;
}

static void test_progress_discover(bool configured)
{
    if (!configured && test_runtime.plan_known) {
        test_runtime.plan_known = false;
        puts("PLAN: discovery");
    }
}

static void test_progress_complete(bool skipped)
{
    if (skipped) {
        if (test_runtime.plan_known && test_runtime.planned_cases) --test_runtime.planned_cases;
    } else ++test_runtime.completed_cases;
    if (test_runtime.plan_known && test_runtime.completed_cases > test_runtime.planned_cases)
        test_progress_discover(false);
    if (test_runtime.plan_known)
        printf("PROGRESS: %zu/%zu\n", test_runtime.completed_cases, test_runtime.planned_cases);
    fflush(stdout);
}

static void test_data_case_callback(void)
{
    test_runtime.data_check(test_runtime.case_data);
}

/* Data tables supply case identities and values; the module callback supplies the actual assertions. */
void test_run_data_case(const char *file, int line, const char *name, const char *parent, const char *tags,
                         const void *data, void (*check)(const void *data))
{
    if (!check) {
        test_mark_failure(file, line, "A data case requires a checking callback.");
        return;
    }
    const void *previous_data = test_runtime.case_data;
    void (*previous_check)(const void *) = test_runtime.data_check;
    test_runtime.case_data = data;
    test_runtime.data_check = check;
    test_run_in_group(file, line, name, parent, tags, test_data_case_callback);
    test_runtime.case_data = previous_data;
    test_runtime.data_check = previous_check;
}

/* Optional storage snapshots never become part of the core execution machinery. */
void test_run_database_cases(const char *file, int line, const char *module, const char *parent,
                              void (*check)(const void *data))
{
    if (test_runtime.adapter && test_runtime.adapter->run_cases)
        test_runtime.adapter->run_cases(file, line, module, parent ? parent :
            test_text_c_str(test_runtime.current_path), check);
    else
        test_mark_failure(file, line, "Database cases require the optional storage adapter.");
}

static int test_run_case_fixture_setup(const char *file, int line);
static void test_run_case_fixture_teardown(const char *file, int line);
static void test_run_in_group_text(const test_text_t *file_text, int line, const test_text_t *name_text,
                                   const test_text_t *parent_text, const test_text_t *tags_text, const char *file_boundary,
                                   test_fn fn);
static int test_case_register_cleanup(test_cleanup_fn fn, void *ctx);
static void test_case_cleanup_all(void);
static void test_case_cleanup_string(void *ctx);
static void test_case_cleanup_temp_tree(void *ctx);
static const test_text_t *test_case_temp_dir_text(void);
static int test_remove_tree_string(const test_text_t *path_text);

static test_text_t *test_string_clone_or_empty(const test_text_t *text)
{
    return text ? test_text_clone(text) : test_text_new_with("");
}

static test_text_t *test_string_from_boundary(const char *text)
{
    return test_text_new_with(text ? text : "");
}

static int test_boundary_text_equals(const char *left, const char *right)
{
    return strcmp(left ? left : "", right ? right : "") == 0;
}

static void test_string_replace(test_text_t **target, test_text_t *replacement)
{
    if (!target)
        return;

    if (!replacement)
        replacement = test_text_new_with("");

    test_text_free(*target);
    *target = replacement;
}

static void test_string_clear_slot(test_text_t **target)
{
    if (!target)
        return;

    if (*target)
        test_text_clear(*target);
    else
        *target = test_text_new();
}

static void test_string_replace_boundary(test_text_t **target, const char *text)
{
    test_string_replace(target, test_string_from_boundary(text));
}

static int test_string_has_text(const test_text_t *text)
{
    return text && test_text_length(text) > 0u;
}

bool test_harness_config_is_enabled(const char *file, const char *func, const char *parent)
{
    return !test_runtime.adapter || !test_runtime.adapter->query ||
        test_runtime.adapter->query(file, func, parent, false);
}

bool test_harness_config_has_key(const char *file, const char *func, const char *parent)
{
    return !test_runtime.adapter || !test_runtime.adapter->query ||
        test_runtime.adapter->query(file, func, parent, true);
}

static double test_elapsed_ms(struct timespec t0, struct timespec t1)
{
    return (t1.tv_sec - t0.tv_sec) * 1000.0 + (t1.tv_nsec - t0.tv_nsec) / 1e6;
}

static void test_print_time(double ms)
{
    printf(C_GREY "  [");
    if (ms < 0.001) {
        long ns = (long)(ms * 1000000.0 + 0.5);

        if (ns < 1)
            printf("< 1 ns");
        else
            printf("%ld ns", ns);
    } else if (ms < 1.0) {
        printf("%.1f µs", ms * 1000.0);
    } else if (ms < 1000.0) {
        printf("%.1f ms", ms);
    } else {
        printf("%.2f s", ms / 1000.0);
    }
    printf("]" C_RESET);
}

void test_section(const char *title)
{
    test_text_t *title_text = test_string_from_boundary(title);

    if (!title_text)
        return;

    printf(C_BOLD C_CYAN "=== %s ===\n" C_RESET, test_text_c_str(title_text));
    test_text_free(title_text);
}

/* Keep the dynamic registry sorted so lookups do not scan every registered checker. */
static size_t test_validity_lower_bound(const char *name)
{
    size_t low = 0, high = test_runtime.validity_registry_count;
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        if (strcmp(test_text_c_str(test_runtime.validity_registry[mid].name), name) < 0) low = mid + 1;
        else high = mid;
    }
    return low;
}

/* Retain the caller-owned contract under a copied, indexed name. */
void test_register_validity_checker(const char *name, const test_validity_contract_t *contract)
{
    test_text_t *name_text;
    size_t i;

    if (!name || !*name || !contract)
        return;

    name_text = test_string_from_boundary(name);
    if (!name_text)
        return;

    i = test_validity_lower_bound(name);
    if (i < test_runtime.validity_registry_count &&
        !strcmp(test_text_c_str(test_runtime.validity_registry[i].name), name)) {
        test_runtime.validity_registry[i].contract = contract;
        test_text_free(name_text);
        return;
    }

    if (test_runtime.validity_registry_count == test_runtime.validity_registry_cap) {
        size_t new_cap = test_runtime.validity_registry_cap ? test_runtime.validity_registry_cap * 2u : 8u;
        test_validity_registry_entry_t *grown = realloc(test_runtime.validity_registry, new_cap * sizeof(*grown));

        if (!grown) {
            test_text_free(name_text);
            return;
        }

        test_runtime.validity_registry = grown;
        test_runtime.validity_registry_cap = new_cap;
    }

    memmove(test_runtime.validity_registry + i + 1, test_runtime.validity_registry + i,
            (test_runtime.validity_registry_count - i) * sizeof(*test_runtime.validity_registry));
    test_runtime.validity_registry[i].name = name_text;
    test_runtime.validity_registry[i].contract = contract;
    test_runtime.validity_registry_count++;
}

/* Find an opaque domain checker by binary search without allocating temporary text. */
const test_validity_contract_t *test_find_validity_checker(const char *name)
{
    if (!name || !*name) return NULL;
    size_t i = test_validity_lower_bound(name);
    return i < test_runtime.validity_registry_count &&
        !strcmp(test_text_c_str(test_runtime.validity_registry[i].name), name) ?
        test_runtime.validity_registry[i].contract : NULL;
}

bool test_require_validity_checker(const char *name, const char *file, int line)
{
    if (test_find_validity_checker(name))
        return true;

    printf(C_RED "    Setup failed at %s:%d: missing validity checker '%s'\n" C_RESET, file, line,
                  name ? name : "(null)");
    test_set_failure_detailf("missing validity checker '%s'", name ? name : "(null)");
    test_runtime.failure_count++;
    test_string_replace_boundary(&test_runtime.last_fail_file, file);
    test_runtime.last_fail_line = line;
    return false;
}

static int test_case_register_cleanup(test_cleanup_fn fn, void *ctx)
{
    if (!fn)
        return 0;

    if (test_runtime.case_cleanup_count == test_runtime.case_cleanup_cap) {
        size_t new_cap = test_runtime.case_cleanup_cap ? test_runtime.case_cleanup_cap * 2u : 8u;
        test_cleanup_entry_t *grown = realloc(test_runtime.case_cleanups, new_cap * sizeof(*grown));

        if (!grown)
            return 0;

        test_runtime.case_cleanups = grown;
        test_runtime.case_cleanup_cap = new_cap;
    }

    test_runtime.case_cleanups[test_runtime.case_cleanup_count].fn = fn;
    test_runtime.case_cleanups[test_runtime.case_cleanup_count].ctx = ctx;
    test_runtime.case_cleanup_count++;
    return 1;
}

static void test_case_cleanup_string(void *ctx)
{
    test_text_free((test_text_t *)ctx);
}

static int test_remove_tree_string(const test_text_t *path_text)
{
    struct stat st;
    const char *path = test_string_has_text(path_text) ? test_text_c_str(path_text) : NULL;

    if (!path)
        return 0;
    if (lstat(path, &st) != 0)
        return errno == ENOENT;

    if (S_ISDIR(st.st_mode)) {
        DIR *dir = opendir(path);
        struct dirent *ent;

        if (!dir)
            return 0;

        while ((ent = readdir(dir)) != NULL) {
            test_text_t *entry_text;
            test_text_t *child_text;

            if (test_boundary_text_equals(ent->d_name, ".") || test_boundary_text_equals(ent->d_name, ".."))
                continue;

            entry_text = test_string_from_boundary(ent->d_name);
            if (!entry_text) {
                closedir(dir);
                return 0;
            }

            child_text = test_text_sprintf("%s/%s", test_text_c_str(path_text), test_text_c_str(entry_text));
            test_text_free(entry_text);
            if (!child_text) {
                closedir(dir);
                return 0;
            }

            if (!test_remove_tree_string(child_text)) {
                test_text_free(child_text);
                closedir(dir);
                return 0;
            }
            test_text_free(child_text);
        }

        closedir(dir);
        return rmdir(path) == 0 || errno == ENOENT;
    }

    return unlink(path) == 0 || errno == ENOENT;
}

static void test_case_cleanup_temp_tree(void *ctx)
{
    test_text_t *path = (test_text_t *)ctx;

    if (path)
        (void)test_remove_tree_string(path);
    test_text_free(path);
}

static void test_case_cleanup_all(void)
{
    while (test_runtime.case_cleanup_count > 0u) {
        test_cleanup_entry_t entry = test_runtime.case_cleanups[--test_runtime.case_cleanup_count];

        if (entry.fn)
            entry.fn(entry.ctx);
    }

    test_runtime.case_temp_dir = NULL;
}

static void test_print_tags(const test_text_t *tags)
{
    if (!test_string_has_text(tags))
        return;
    printf(" " C_DIM "{%s}" C_RESET, test_text_c_str(tags));
}

static const char *test_outcome_name(test_outcome_t outcome)
{
    switch (outcome) {
        case TEST_OUTCOME_PASS:
            return "pass";
        case TEST_OUTCOME_FAIL:
            return "fail";
        case TEST_OUTCOME_SKIP:
            return "skip";
        default:
            return "unknown";
    }
}

static int test_format_value_fallback(const void *value, test_text_t *out)
{
    if (!out)
        return -1;

    if (!value)
        return test_text_append_cstr(out, "<null>");

    return test_text_append_format(out, "%p", value);
}

static int test_int_equal(const void *got, const void *want, void *ctx)
{
    (void)ctx;
    return *(const int *)got == *(const int *)want;
}

static int test_int_format(const void *value, test_text_t *out, void *ctx)
{
    (void)ctx;
    return test_text_append_format(out, "%d", *(const int *)value);
}

static int test_long_equal(const void *got, const void *want, void *ctx)
{
    (void)ctx;
    return *(const long *)got == *(const long *)want;
}

static int test_long_format(const void *value, test_text_t *out, void *ctx)
{
    (void)ctx;
    return test_text_append_format(out, "%ld", *(const long *)value);
}

static int test_cstr_equal(const void *got, const void *want, void *ctx)
{
    const char *const *got_text = (const char *const *)got;
    const char *const *want_text = (const char *const *)want;
    (void)ctx;

    if (!*got_text && !*want_text)
        return 1;
    if (!*got_text || !*want_text)
        return 0;
    return test_boundary_text_equals(*got_text, *want_text);
}

static int test_cstr_format(const void *value, test_text_t *out, void *ctx)
{
    const char *const *text = (const char *const *)value;
    (void)ctx;

    return test_text_append_format(out, "\"%s\"", *text ? *text : "<null>");
}

const test_validity_contract_t *test_validity_contract_int(void)
{
    static const test_validity_contract_t contract = {"int-equality", test_int_equal, test_int_format, NULL};

    return &contract;
}

const test_validity_contract_t *test_validity_contract_long(void)
{
    static const test_validity_contract_t contract = {"long-equality", test_long_equal, test_long_format, NULL};

    return &contract;
}

const test_validity_contract_t *test_validity_contract_cstr(void)
{
    static const test_validity_contract_t contract = {"cstr-equality", test_cstr_equal, test_cstr_format, NULL};

    return &contract;
}

static const test_text_t *test_case_temp_dir_text(void)
{
    if (test_runtime.case_temp_dir) return test_runtime.case_temp_dir;
    const char *configured = getenv("TMPDIR");
    test_text_t *owned = test_text_sprintf("%s/mars-test-XXXXXX", configured && *configured ? configured : P_tmpdir);
    if (!owned || !mkdtemp((char *)test_text_c_str(owned))) {
        test_text_free(owned);
        return NULL;
    }
    if (!test_case_register_cleanup(test_case_cleanup_temp_tree, owned)) {
        rmdir(test_text_c_str(owned));
        test_text_free(owned);
        return NULL;
    }
    test_runtime.case_temp_dir = owned;
    return owned;
}

const char *test_case_temp_dir(void)
{
    const test_text_t *dir = test_case_temp_dir_text();

    return dir ? test_text_c_str(dir) : NULL;
}

const char *test_case_temp_path(const char *leafname)
{
    const test_text_t *dir = test_case_temp_dir_text();
    test_text_t *leaf_text;
    test_text_t *path_text;

    if (!dir)
        return NULL;

    leaf_text = test_string_from_boundary((leafname && *leafname) ? leafname : "tmp");
    if (!leaf_text)
        return NULL;

    path_text = test_text_sprintf("%s/%s", test_text_c_str(dir), test_text_c_str(leaf_text));
    test_text_free(leaf_text);
    if (!path_text)
        return NULL;

    if (!test_case_register_cleanup(test_case_cleanup_string, path_text)) {
        test_text_free(path_text);
        return NULL;
    }

    return test_text_c_str(path_text);
}

int test_case_begin_stdout_capture(const char *leafname, const char **path_out)
{
    const char *path = test_case_temp_path(leafname ? leafname : "stdout.txt");
    int saved_stdout;

    if (!path)
        return -1;

    saved_stdout = dup(fileno(stdout));
    if (saved_stdout < 0)
        return -1;

    fflush(stdout);
    if (!freopen(path, "w", stdout)) {
        close(saved_stdout);
        return -1;
    }

    if (path_out)
        *path_out = path;

    return saved_stdout;
}

bool test_case_end_stdout_capture(int saved_stdout)
{
    if (saved_stdout < 0)
        return false;

    fflush(stdout);
    if (dup2(saved_stdout, fileno(stdout)) < 0) {
        close(saved_stdout);
        return false;
    }

    close(saved_stdout);
    return true;
}

int test_case_begin_stderr_capture(const char *leafname, const char **path_out)
{
    const char *path = test_case_temp_path(leafname ? leafname : "stderr.txt");
    int saved_stderr;

    if (!path)
        return -1;

    saved_stderr = dup(fileno(stderr));
    if (saved_stderr < 0)
        return -1;

    fflush(stderr);
    if (!freopen(path, "w", stderr)) {
        close(saved_stderr);
        return -1;
    }

    if (path_out)
        *path_out = path;

    return saved_stderr;
}

bool test_case_end_stderr_capture(int saved_stderr)
{
    if (saved_stderr < 0)
        return false;

    fflush(stderr);
    if (dup2(saved_stderr, fileno(stderr)) < 0) {
        close(saved_stderr);
        return false;
    }

    close(saved_stderr);
    return true;
}

static void test_print_capture_file(const char *path)
{
    FILE *f;
    char buffer[4096];
    size_t n;

    if (!path || *path == '\0')
        return;

    f = fopen(path, "rb");
    if (!f)
        return;

    while ((n = fread(buffer, 1u, sizeof(buffer), f)) > 0u)
        fwrite(buffer, 1u, n, stdout);

    fclose(f);
}

static size_t test_missing_config_lower_bound(const test_text_t *full_key)
{
    size_t low = 0, high = test_runtime.missing_config_path_count;
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        if (test_text_compare(test_runtime.missing_config_paths[mid], full_key) < 0) low = mid + 1;
        else high = mid;
    }
    return low;
}

static void test_note_missing_config_path(const test_text_t *file, const test_text_t *path)
{
    test_text_t *full_key;

    if (test_suite_mode == TEST_CONFIG_NONE || !test_string_has_text(file) || !test_string_has_text(path))
        return;

    full_key = test_text_sprintf("%s::%s", test_text_c_str(file), test_text_c_str(path));
    if (!full_key)
        return;

    size_t at = test_missing_config_lower_bound(full_key);
    if (at < test_runtime.missing_config_path_count &&
        !test_text_compare(test_runtime.missing_config_paths[at], full_key)) {
        test_text_free(full_key);
        return;
    }

    if (test_runtime.missing_config_path_count == test_runtime.missing_config_path_cap) {
        size_t new_cap = test_runtime.missing_config_path_cap ? test_runtime.missing_config_path_cap * 2u : 16u;
        test_text_t **grown = realloc(test_runtime.missing_config_paths, new_cap * sizeof(*grown));

        if (!grown) {
            test_text_free(full_key);
            return;
        }

        test_runtime.missing_config_paths = grown;
        test_runtime.missing_config_path_cap = new_cap;
    }

    memmove(test_runtime.missing_config_paths + at + 1, test_runtime.missing_config_paths + at,
            (test_runtime.missing_config_path_count - at) * sizeof(*test_runtime.missing_config_paths));
    test_runtime.missing_config_paths[at] = full_key;
    test_runtime.missing_config_path_count++;
    test_runtime.missing_config_count++;
}

static void test_record_case(const test_text_t *file, int declaration_line, const test_text_t *name, const test_text_t *parent,
                             const test_text_t *path, const test_text_t *tags, test_outcome_t outcome, int enabled,
                             int is_group, int is_output, int group_passed, int group_failed, int group_skipped,
                             double ms)
{
    test_record_t *next;

    if (test_runtime.record_count == test_runtime.record_cap) {
        size_t new_cap = test_runtime.record_cap ? test_runtime.record_cap * 2u : 32u;
        test_record_t *grown = realloc(test_runtime.records, new_cap * sizeof(*grown));

        if (!grown)
            return;

        test_runtime.records = grown;
        test_runtime.record_cap = new_cap;
    }

    next = &test_runtime.records[test_runtime.record_count++];
    next->file = test_string_clone_or_empty(file);
    next->name = test_string_clone_or_empty(name);
    next->parent = test_string_clone_or_empty(parent);
    next->path = test_string_clone_or_empty(path);
    next->tags = test_string_clone_or_empty(tags);
    next->failure_file =
        (outcome == TEST_OUTCOME_FAIL || outcome == TEST_OUTCOME_SKIP)
            ? test_string_clone_or_empty(outcome == TEST_OUTCOME_FAIL ? test_runtime.last_fail_file : test_runtime.last_skip_file)
            : NULL;
    if (outcome == TEST_OUTCOME_FAIL)
        next->failure_detail = test_string_clone_or_empty(test_runtime.last_fail_detail);
    else if (outcome == TEST_OUTCOME_SKIP)
        next->failure_detail = test_string_clone_or_empty(test_runtime.last_skip_detail);
    else
        next->failure_detail = NULL;
    next->outcome = outcome;
    next->enabled = enabled;
    next->is_group = is_group;
    next->is_output = is_output;
    next->declaration_line = declaration_line;
    if (outcome == TEST_OUTCOME_FAIL)
        next->failure_line = test_runtime.last_fail_line;
    else if (outcome == TEST_OUTCOME_SKIP)
        next->failure_line = test_runtime.last_skip_line;
    else
        next->failure_line = 0;
    next->group_passed = group_passed;
    next->group_failed = group_failed;
    next->group_skipped = group_skipped;
    next->ms = ms;
    struct timespec now;
    struct tm utc;
    next->finished_utc[0] = '\0';
    if (!clock_gettime(CLOCK_REALTIME, &now) && gmtime_r(&now.tv_sec, &utc) &&
        strftime(next->finished_utc, sizeof next->finished_utc, "%Y-%m-%dT%H:%M:%S", &utc))
        snprintf(next->finished_utc + 19, sizeof next->finished_utc - 19u, ".%03uZ",
                 (unsigned)(now.tv_nsec / 1000000L));
}

/* Release owned singleton resources; summary counters remain available to the post-summary hook. */
static void test_runtime_release(void)
{
    size_t i;

    if (test_runtime.adapter && test_runtime.adapter->release) test_runtime.adapter->release();

    for (i = 0; i < test_runtime.record_count; ++i) {
        test_text_free(test_runtime.records[i].file);
        test_text_free(test_runtime.records[i].name);
        test_text_free(test_runtime.records[i].parent);
        test_text_free(test_runtime.records[i].path);
        test_text_free(test_runtime.records[i].tags);
        test_text_free(test_runtime.records[i].failure_file);
        test_text_free(test_runtime.records[i].failure_detail);
    }

    free(test_runtime.records);
    test_runtime.records = NULL;
    test_runtime.record_count = 0u;
    test_runtime.record_cap = 0u;

    if (test_runtime.missing_config_paths) {
        size_t i;

        for (i = 0; i < test_runtime.missing_config_path_count; ++i)
            test_text_free(test_runtime.missing_config_paths[i]);
        free(test_runtime.missing_config_paths);
    }
    test_runtime.missing_config_paths = NULL;
    test_runtime.missing_config_path_count = 0u;
    test_runtime.missing_config_path_cap = 0u;

    if (test_runtime.validity_registry) {
        size_t i;

        for (i = 0; i < test_runtime.validity_registry_count; ++i)
            test_text_free(test_runtime.validity_registry[i].name);
        free(test_runtime.validity_registry);
    }
    test_runtime.validity_registry = NULL;
    test_runtime.validity_registry_count = 0u;
    test_runtime.validity_registry_cap = 0u;

    free(test_runtime.case_cleanups);
    test_runtime.case_cleanups = NULL;
    test_runtime.case_cleanup_count = 0u;
    test_runtime.case_cleanup_cap = 0u;

    test_runtime.case_temp_dir = NULL;

    test_text_free(test_runtime.last_fail_detail);
    test_runtime.last_fail_detail = NULL;
    test_text_free(test_runtime.last_fail_file);
    test_runtime.last_fail_file = NULL;
    test_text_free(test_runtime.last_skip_detail);
    test_runtime.last_skip_detail = NULL;
    test_text_free(test_runtime.last_skip_file);
    test_runtime.last_skip_file = NULL;
}

void test_set_failure_detailf(const char *fmt, ...)
{
    va_list ap;
    test_text_t *detail;

    if (!fmt) {
        test_string_clear_slot(&test_runtime.last_fail_detail);
        return;
    }

    va_start(ap, fmt);
    detail = test_text_vsprintf(fmt, ap);
    va_end(ap);

    test_string_replace(&test_runtime.last_fail_detail, detail);
}

void test_clear_failure_detail(void)
{
    test_string_clear_slot(&test_runtime.last_fail_detail);
    test_string_clear_slot(&test_runtime.last_fail_file);
    test_runtime.last_fail_line = 0;
    test_string_clear_slot(&test_runtime.last_skip_detail);
    test_string_clear_slot(&test_runtime.last_skip_file);
    test_runtime.last_skip_line = 0;
    test_runtime.skip_requested = 0;
}

void test_mark_failure(const char *file, int line, const char *detail)
{
    test_runtime.failure_count++;
    test_string_replace_boundary(&test_runtime.last_fail_file, file);
    test_runtime.last_fail_line = line;
    test_set_failure_detailf("%s", detail ? detail : "test failed");
}

void test_fail(const char *file, int line)
{
    test_mark_failure(file, line, "explicit test failure");
}

void test_mark_skip(const char *file, int line, const char *detail)
{
    test_runtime.skip_requested = 1;
    test_string_replace_boundary(&test_runtime.last_skip_file, file);
    test_runtime.last_skip_line = line;
    test_string_replace(&test_runtime.last_skip_detail, test_text_sprintf("%s", detail ? detail : "test skipped"));
}

bool test_request_skip(const char *file, int line, const char *detail)
{
    test_mark_skip(file, line, detail);
    return false;
}

bool test_assert_true(bool expr, const char *file, int line, const char *detail)
{
    if (expr)
        return true;

    printf(C_RED "    Assertion failed at %s:%d: %s\n" C_RESET, file, line, detail ? detail : "want true");
    test_set_failure_detailf("%s", detail ? detail : "want true");
    test_runtime.failure_count++;
    test_string_replace_boundary(&test_runtime.last_fail_file, file);
    test_runtime.last_fail_line = line;
    return false;
}

bool test_assert_false(bool expr, const char *file, int line, const char *detail)
{
    if (!expr)
        return true;

    printf(C_RED "    Assertion failed at %s:%d: %s\n" C_RESET, file, line, detail ? detail : "want false");
    test_set_failure_detailf("%s", detail ? detail : "want false");
    test_runtime.failure_count++;
    test_string_replace_boundary(&test_runtime.last_fail_file, file);
    test_runtime.last_fail_line = line;
    return false;
}

bool test_assert_int_eq(int got, int want, const char *file, int line)
{
    if (got == want)
        return true;

    printf(C_RED "    Assertion failed at %s:%d: want %d, got %d\n" C_RESET, file, line, want, got);
    test_set_failure_detailf("want %d, got %d", want, got);
    test_runtime.failure_count++;
    test_string_replace_boundary(&test_runtime.last_fail_file, file);
    test_runtime.last_fail_line = line;
    return false;
}

bool test_assert_long_eq(long got, long want, const char *file, int line)
{
    if (got == want)
        return true;

    printf(C_RED "    Assertion failed at %s:%d: want %ld, got %ld\n" C_RESET, file, line, want, got);
    test_set_failure_detailf("want %ld, got %ld", want, got);
    test_runtime.failure_count++;
    test_string_replace_boundary(&test_runtime.last_fail_file, file);
    test_runtime.last_fail_line = line;
    return false;
}

bool test_assert_double_eq(double got, double want, double eps, const char *file, int line)
{
    if (fabs(got - want) <= eps)
        return true;

    printf(C_RED "    Assertion failed at %s:%d: want %.12f, got %.12f\n" C_RESET, file, line, want,
                  got);
    test_set_failure_detailf("want %.12f, got %.12f", want, got);
    test_runtime.failure_count++;
    test_string_replace_boundary(&test_runtime.last_fail_file, file);
    test_runtime.last_fail_line = line;
    return false;
}

bool test_assert_not_null(const void *ptr, const char *file, int line)
{
    if (ptr)
        return true;

    printf(C_RED "    Assertion failed at %s:%d: want non-null pointer\n" C_RESET, file, line);
    test_set_failure_detailf("want non-null pointer");
    test_runtime.failure_count++;
    test_string_replace_boundary(&test_runtime.last_fail_file, file);
    test_runtime.last_fail_line = line;
    return false;
}

bool test_assert_null(const void *ptr, const char *file, int line)
{
    if (!ptr)
        return true;

    printf(C_RED "    Assertion failed at %s:%d: want NULL pointer\n" C_RESET, file, line);
    test_set_failure_detailf("want NULL pointer");
    test_runtime.failure_count++;
    test_string_replace_boundary(&test_runtime.last_fail_file, file);
    test_runtime.last_fail_line = line;
    return false;
}

bool test_assert_validity(const test_validity_contract_t *contract, const void *got, const void *want,
                          const char *file, int line)
{
    test_text_t *got_text = NULL;
    test_text_t *want_text = NULL;
    const char *contract_name;
    int ok;

    if (!contract || !contract->equal) {
        printf(C_RED "    Assertion failed at %s:%d: invalid validity contract\n" C_RESET, file, line);
        test_set_failure_detailf("invalid validity contract");
        test_runtime.failure_count++;
        test_string_replace_boundary(&test_runtime.last_fail_file, file);
        test_runtime.last_fail_line = line;
        return false;
    }

    ok = contract->equal(got, want, contract->ctx);
    if (ok)
        return true;

    got_text = test_text_new();
    want_text = test_text_new();
    if (!got_text || !want_text) {
        test_text_free(got_text);
        test_text_free(want_text);
        printf(C_RED "    Assertion failed at %s:%d: out of memory formatting validity failure\n" C_RESET, file,
                      line);
        test_set_failure_detailf("out of memory formatting validity failure");
        test_runtime.failure_count++;
        test_string_replace_boundary(&test_runtime.last_fail_file, file);
        test_runtime.last_fail_line = line;
        return false;
    }

    if (contract->format) {
        if (contract->format(want, want_text, contract->ctx) < 0) {
            test_text_clear(want_text);
            (void)test_format_value_fallback(want, want_text);
        }
        if (contract->format(got, got_text, contract->ctx) < 0) {
            test_text_clear(got_text);
            (void)test_format_value_fallback(got, got_text);
        }
    } else if (test_format_value_fallback(want, want_text) != 0 ||
               test_format_value_fallback(got, got_text) != 0) {
        test_text_free(got_text);
        test_text_free(want_text);
        printf(C_RED "    Assertion failed at %s:%d: out of memory formatting validity failure\n" C_RESET, file,
                      line);
        test_set_failure_detailf("out of memory formatting validity failure");
        test_runtime.failure_count++;
        test_string_replace_boundary(&test_runtime.last_fail_file, file);
        test_runtime.last_fail_line = line;
        return false;
    }

    contract_name = (contract->name && *contract->name) ? contract->name : "validity";

    printf(C_RED "    Assertion failed at %s:%d [%s]: want %s, got %s\n" C_RESET, file, line,
           contract_name, test_text_c_str(want_text), test_text_c_str(got_text));
    test_set_failure_detailf("[%s] want %s, got %s", contract_name, test_text_c_str(want_text), test_text_c_str(got_text));
    test_text_free(got_text);
    test_text_free(want_text);
    test_runtime.failure_count++;
    test_string_replace_boundary(&test_runtime.last_fail_file, file);
    test_runtime.last_fail_line = line;
    return false;
}

bool test_assert_validity_named(const char *name, const void *got, const void *want, const char *file, int line)
{
    const test_validity_contract_t *contract = test_find_validity_checker(name);

    if (!contract) {
        printf(C_RED "    Assertion failed at %s:%d: missing named validity checker: %s\n" C_RESET, file, line,
                      name ? name : "(null)");
        test_set_failure_detailf("missing named validity checker: %s", name ? name : "(null)");
        test_runtime.failure_count++;
        test_string_replace_boundary(&test_runtime.last_fail_file, file);
        test_runtime.last_fail_line = line;
        return false;
    }

    return test_assert_validity(contract, got, want, file, line);
}

bool test_assert_cstr_eq(const char *got, const char *want, const char *file, int line)
{
    const char *got_text = got;
    const char *want_text = want;

    return test_assert_validity(TEST_VALID_CSTR(), &got_text, &want_text, file, line);
}

void test_run_case(const char *file, int line, const char *name, const char *tags, test_fn fn)
{
    test_run_in_group(file, line, name, NULL, tags, fn);
}

void test_run_subtest(const char *file, int line, const char *name, const char *tags, test_fn fn)
{
    test_text_t *file_text = NULL;
    test_text_t *name_text = NULL;
    test_text_t *tags_text = NULL;

    if (!test_string_has_text(test_runtime.current_path)) {
        printf(
            C_RED "    Harness misuse at %s:%d: test_run_subtest(%s) requires an active parent test/group\n" C_RESET,
            file, line, name ? name : "(unnamed)");
        test_mark_failure(file, line, "test_run_subtest requires an active parent");
        return;
    }

    file_text = test_string_from_boundary(file);
    name_text = test_string_from_boundary(name ? name : "(unnamed)");
    tags_text = test_string_from_boundary(tags);
    if (!file_text || !name_text || !tags_text) {
        test_mark_failure(file, line, "out of memory while preparing subtest strings");
        test_text_free(file_text);
        test_text_free(name_text);
        test_text_free(tags_text);
        return;
    }

    test_run_in_group_text(file_text, line, name_text, test_runtime.current_path, tags_text, file, fn);

    test_text_free(tags_text);
    test_text_free(name_text);
    test_text_free(file_text);
}

static void test_run_in_group_text(const test_text_t *file_text, int line, const test_text_t *name_text,
                                   const test_text_t *parent_text, const test_text_t *tags_text, const char *file_boundary,
                                   test_fn fn)
{
    bool enabled;
    int failure_count_before;
    int skip_count_before;
    int passed_cases_before;
    int failed_cases_before;
    int nested_runs_before;
    int is_group;
    double total_ms_before;
    double ms;
    double disp_ms;
    struct timespec t0;
    struct timespec t1;
    const test_text_t *prev_path;
    test_text_t *full_path_text = NULL;
    const test_text_t *print_file;
    int had_config_key = 1;
    int fixture_ran = 0;
    int fixture_continue = 1;

    if (test_runtime.abort_requested)
        return;

    if (parent_text && test_string_has_text(parent_text))
        full_path_text = test_text_sprintf("%s.%s", test_text_c_str(parent_text), test_text_c_str(name_text));
    else
        full_path_text = test_text_clone(name_text);
    if (!full_path_text) {
        test_mark_failure(file_boundary, line, "out of memory while building test path");
        goto cleanup;
    }

    had_config_key = test_harness_config_has_key(test_text_c_str(file_text), test_text_c_str(name_text),
                                                test_text_c_str(parent_text)) ? 1 : 0;
    enabled = test_harness_config_is_enabled(test_text_c_str(file_text), test_text_c_str(name_text),
                                             test_text_c_str(parent_text));
    enabled = test_database_selected(file_text, full_path_text, enabled);
    if (!had_config_key)
        test_note_missing_config_path(file_text, full_path_text);

    test_runtime.nested_run_count++;

    if (!enabled) {
        if (parent_text)
            printf(C_YELLOW "SKIP: %s (in %s)" C_RESET "\n", test_text_c_str(name_text), test_text_c_str(parent_text));
        else
            printf(C_YELLOW "SKIP: %s" C_RESET "\n", test_text_c_str(name_text));
        test_runtime.skip_count++;
        test_record_case(file_text, line, name_text, parent_text, full_path_text, tags_text, TEST_OUTCOME_SKIP, 0, 0, 0,
                         0, 0, 0, 0.0);
        goto cleanup;
    }

    failure_count_before = test_runtime.failure_count;
    skip_count_before = test_runtime.skip_count;
    passed_cases_before = test_runtime.passed_cases;
    failed_cases_before = test_runtime.failed_cases;
    nested_runs_before = test_runtime.nested_run_count;
    total_ms_before = test_runtime.total_ms;
    prev_path = test_runtime.current_path;

    test_clear_failure_detail();
    test_runtime.current_path = full_path_text;
    test_progress_discover(had_config_key);
    printf("RUN: %s\n", test_text_c_str(full_path_text));
    fflush(stdout);
    clock_gettime(CLOCK_MONOTONIC, &t0);
    if (test_fixture_setup_hook) {
        fixture_ran = 1;
        fixture_continue = test_run_case_fixture_setup(file_boundary, line);
    }
    if (fixture_continue && test_runtime.failure_count == failure_count_before && !test_runtime.skip_requested) {
        fn();
    }
    if (fixture_ran && fixture_continue)
        test_run_case_fixture_teardown(file_boundary, line);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    test_runtime.current_path = prev_path;
    test_case_cleanup_all();

    ms = test_elapsed_ms(t0, t1);
    is_group = (test_runtime.nested_run_count > nested_runs_before);
    disp_ms = is_group ? (test_runtime.total_ms - total_ms_before) : ms;
    if (!is_group)
        test_runtime.total_ms += ms;
    if (is_group)
        test_runtime.group_count++;
    else
        test_runtime.run_count++;

    if (is_group) {
        int skipped = test_runtime.skip_count - skip_count_before;
        int passed = test_runtime.passed_cases - passed_cases_before;
        int failed = test_runtime.failed_cases - failed_cases_before;
        test_outcome_t outcome = failed == 0 ? TEST_OUTCOME_PASS : TEST_OUTCOME_FAIL;

        printf(C_CYAN "GROUP: %s " C_RESET "(" C_BOLD C_GREEN "%d passed" C_RESET "," C_BOLD C_RED " %d failed" C_RESET
                             "," C_YELLOW " %d skipped" C_RESET ")", test_text_c_str(name_text), passed, failed, skipped);
        test_print_tags(tags_text);
        test_record_case(file_text, line, name_text, parent_text, full_path_text, tags_text, outcome, 1, 1, 0, passed,
                         failed, skipped, disp_ms);
    } else if (test_runtime.skip_requested) {
        test_runtime.skip_count++;
        print_file = test_string_has_text(test_runtime.last_skip_file) ? test_runtime.last_skip_file : file_text;
        printf(C_YELLOW "SKIP: %s " C_YELLOW "(%s:%d)" C_RESET, test_text_c_str(name_text),
               test_text_c_str(print_file), test_runtime.last_skip_line);
        if (test_string_has_text(test_runtime.last_skip_detail))
            printf(" " C_GREY "[%s]" C_RESET, test_text_c_str(test_runtime.last_skip_detail));
        test_print_tags(tags_text);
        test_record_case(file_text, line, name_text, parent_text, full_path_text, tags_text, TEST_OUTCOME_SKIP, 1, 0, 0,
                         0, 0, 0, disp_ms);
    } else if (test_runtime.failure_count == failure_count_before) {
        test_runtime.passed_cases++;
        printf(C_BOLD C_GREEN "PASS: " C_RESET "%s", test_text_c_str(name_text));
        test_print_tags(tags_text);
        test_record_case(file_text, line, name_text, parent_text, full_path_text, tags_text, TEST_OUTCOME_PASS, 1, 0, 0,
                         0, 0, 0, disp_ms);
    } else {
        test_runtime.failed_cases++;
        print_file = test_string_has_text(test_runtime.last_fail_file) ? test_runtime.last_fail_file : file_text;
        printf(C_BOLD C_RED "FAIL: " C_RESET "%s " C_RED "(%s:%d)" C_RESET, test_text_c_str(name_text),
               test_text_c_str(print_file), test_string_has_text(test_runtime.last_fail_file) ? test_runtime.last_fail_line : 0);
        test_print_tags(tags_text);
        test_record_case(file_text, line, name_text, parent_text, full_path_text, tags_text, TEST_OUTCOME_FAIL, 1, 0, 0,
                         0, 0, 0, disp_ms);
    }

    test_print_time(disp_ms);
    printf("\n");
    if (!is_group) test_progress_complete(test_runtime.skip_requested != 0);
    fflush(stdout);
cleanup:
    test_text_free(full_path_text);
}

void test_run_in_group(const char *file, int line, const char *name, const char *parent, const char *tags, test_fn fn)
{
    test_text_t *file_text = NULL;
    test_text_t *name_text = NULL;
    test_text_t *parent_text = NULL;
    test_text_t *tags_text = NULL;

    if (test_runtime.abort_requested)
        return;

    file_text = test_string_from_boundary(file);
    name_text = test_string_from_boundary(name ? name : "(unnamed)");
    parent_text = parent ? test_string_from_boundary(parent) : NULL;
    tags_text = test_string_from_boundary(tags);
    if (!file_text || !name_text || !tags_text || (parent && !parent_text)) {
        test_mark_failure(file, line, "out of memory while preparing test strings");
        goto cleanup;
    }

    test_run_in_group_text(file_text, line, name_text, parent_text, tags_text, file, fn);

cleanup:
    test_text_free(tags_text);
    test_text_free(parent_text);
    test_text_free(name_text);
    test_text_free(file_text);
}

void test_run_output_case(const char *file, int line, const char *tags, const char *name, test_fn fn)
{
    test_run_output_in_group(file, line, name, NULL, tags, fn);
}

void test_run_output_in_group(const char *file, int line, const char *name, const char *parent, const char *tags,
                              test_fn fn)
{
    int failure_count_before;
    bool enabled;
    int had_config_key = 1;
    int fixture_ran = 0;
    int fixture_continue = 1;
    double ms;
    struct timespec t0;
    struct timespec t1;
    test_text_t *file_text = NULL;
    test_text_t *name_text = NULL;
    test_text_t *parent_text = NULL;
    test_text_t *tags_text = NULL;
    test_text_t *full_path_text = NULL;
    const test_text_t *print_file;
    const char *file_boundary;
    const char *stdout_capture_path = NULL;
    int saved_stdout = -1;

    if (test_runtime.abort_requested)
        return;

    file_text = test_string_from_boundary(file);
    name_text = test_string_from_boundary(name ? name : "(unnamed-output)");
    parent_text = parent ? test_string_from_boundary(parent) : NULL;
    tags_text = test_string_from_boundary(tags);
    if (!file_text || !name_text || !tags_text || (parent && !parent_text)) {
        test_mark_failure(file, line, "out of memory while preparing output test strings");
        goto cleanup;
    }

    if (parent_text && test_string_has_text(parent_text))
        full_path_text = test_text_sprintf("%s.%s", test_text_c_str(parent_text), test_text_c_str(name_text));
    else
        full_path_text = test_text_clone(name_text);
    if (!full_path_text) {
        test_mark_failure(file, line, "out of memory while building output path");
        goto cleanup;
    }

    had_config_key = test_harness_config_has_key(test_text_c_str(file_text), test_text_c_str(name_text),
                                                test_text_c_str(parent_text)) ? 1 : 0;
    enabled = test_harness_config_is_enabled(test_text_c_str(file_text), test_text_c_str(name_text),
                                             test_text_c_str(parent_text));
    enabled = test_database_selected(file_text, full_path_text, enabled);
    if (!had_config_key)
        test_note_missing_config_path(file_text, full_path_text);

    test_runtime.output_count++;

    if (!enabled) {
        test_runtime.output_skip_count++;
        printf(C_YELLOW "OUTPUT SKIP: %s" C_RESET, test_text_c_str(name_text));
        test_print_tags(tags_text);
        test_record_case(file_text, line, name_text, parent_text, full_path_text, tags_text, TEST_OUTCOME_SKIP, 0, 0, 1,
                         0, 0, 0, 0.0);
        printf("\n");
        goto cleanup;
    }

    failure_count_before = test_runtime.failure_count;
    test_clear_failure_detail();
    file_boundary = test_text_c_str(file_text);

    test_progress_discover(had_config_key);
    printf("RUN: %s\n", test_text_c_str(full_path_text));
    fflush(stdout);
    clock_gettime(CLOCK_MONOTONIC, &t0);
    if (test_fixture_setup_hook) {
        fixture_ran = 1;
        fixture_continue = test_run_case_fixture_setup(file_boundary, line);
    }
    if (fixture_continue && test_runtime.failure_count == failure_count_before && !test_runtime.skip_requested) {
        saved_stdout = test_case_begin_stdout_capture("output-example-stdout.txt", &stdout_capture_path);
    }
    if (fixture_continue && test_runtime.failure_count == failure_count_before && !test_runtime.skip_requested) {
        fn();
    }
    if (saved_stdout >= 0) {
        if (!test_case_end_stdout_capture(saved_stdout))
            test_mark_failure(file, line, "failed to restore stdout after output capture");
        saved_stdout = -1;
    }
    if (fixture_ran && fixture_continue)
        test_run_case_fixture_teardown(file_boundary, line);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    ms = test_elapsed_ms(t0, t1);

    if (test_runtime.skip_requested) {
        test_runtime.output_skip_count++;
        print_file = test_string_has_text(test_runtime.last_skip_file) ? test_runtime.last_skip_file : file_text;
        printf(C_YELLOW "OUTPUT SKIP: %s " C_YELLOW "(%s:%d)" C_RESET, test_text_c_str(name_text),
               test_text_c_str(print_file), test_runtime.last_skip_line);
        if (test_string_has_text(test_runtime.last_skip_detail))
            printf(" " C_GREY "[%s]" C_RESET, test_text_c_str(test_runtime.last_skip_detail));
        test_print_tags(tags_text);
        test_record_case(file_text, line, name_text, parent_text, full_path_text, tags_text, TEST_OUTCOME_SKIP, 1, 0, 1,
                         0, 0, 0, ms);
    } else if (test_runtime.failure_count == failure_count_before) {
        test_runtime.output_pass_count++;
        printf(C_BOLD C_CYAN "OUTPUT: " C_RESET "%s", test_text_c_str(name_text));
        test_print_tags(tags_text);
        test_record_case(file_text, line, name_text, parent_text, full_path_text, tags_text, TEST_OUTCOME_PASS, 1, 0, 1,
                         0, 0, 0, ms);
    } else {
        test_runtime.output_failure_count++;
        print_file = test_string_has_text(test_runtime.last_fail_file) ? test_runtime.last_fail_file : file_text;
        printf(C_BOLD C_RED "OUTPUT FAIL: " C_RESET "%s " C_RED "(%s:%d)" C_RESET, test_text_c_str(name_text),
               test_text_c_str(print_file), test_string_has_text(test_runtime.last_fail_file) ? test_runtime.last_fail_line : 0);
        test_print_tags(tags_text);
        test_record_case(file_text, line, name_text, parent_text, full_path_text, tags_text, TEST_OUTCOME_FAIL, 1, 0, 1,
                         0, 0, 0, ms);
    }

    test_print_time(ms);
    printf("\n");
    test_progress_complete(test_runtime.skip_requested != 0);
    if (stdout_capture_path && *stdout_capture_path)
        test_print_capture_file(stdout_capture_path);
    test_case_cleanup_all();
cleanup:
    test_text_free(full_path_text);
    test_text_free(tags_text);
    test_text_free(parent_text);
    test_text_free(name_text);
    test_text_free(file_text);
}

static void test_run_suite_setup_case(void)
{
    int failure_count_before;

    if (!test_suite_setup_hook || test_runtime.abort_requested)
        return;

    failure_count_before = test_runtime.failure_count;
    test_clear_failure_detail();

    if (!test_suite_setup_hook() && test_runtime.failure_count == failure_count_before)
        test_mark_failure(__FILE__, __LINE__, "suite setup returned false");

    if (test_runtime.failure_count == failure_count_before) {
        printf(C_GREEN "validity contract ok - ready\n" C_RESET);
    } else {
        printf(C_BOLD C_RED "SETUP FAIL: " C_RESET C_RED "(%s:%d)" C_RESET,
                      test_string_has_text(test_runtime.last_fail_file) ? test_text_c_str(test_runtime.last_fail_file) : __FILE__,
                      test_runtime.last_fail_line);
        if (test_string_has_text(test_runtime.last_fail_detail))
            printf(" " C_GREY "[%s]" C_RESET, test_text_c_str(test_runtime.last_fail_detail));
        printf("\n");
    }
}

static int test_run_case_fixture_setup(const char *file, int line)
{
    int failure_count_before;

    if (!test_fixture_setup_hook || test_runtime.abort_requested)
        return 1;

    failure_count_before = test_runtime.failure_count;
    if (!test_fixture_setup_hook()) {
        if (!test_runtime.skip_requested && test_runtime.failure_count == failure_count_before) {
            test_mark_failure(file, line, "test fixture setup returned false");
        }
        return 0;
    }

    return !test_runtime.skip_requested;
}

static void test_run_case_fixture_teardown(const char *file, int line)
{
    int failure_count_before;

    if (!test_fixture_teardown_hook || test_runtime.abort_requested)
        return;

    failure_count_before = test_runtime.failure_count;
    if (!test_fixture_teardown_hook() && !test_runtime.skip_requested && test_runtime.failure_count == failure_count_before) {
        test_mark_failure(file, line, "test fixture teardown returned false");
    }
}

int test_exit_code(void)
{
    return test_runtime.failure_count ? 1 : 0;
}

static void test_print_slowest_cases(void)
{
    int limit = 5;
    int printed = 0;
    unsigned char *used;

    if (limit <= 0 || test_runtime.record_count == 0u)
        return;

    used = calloc(test_runtime.record_count, sizeof(*used));
    if (!used)
        return;

    printf(C_CYAN "SLOWEST: " C_RESET "\n");

    while (printed < limit) {
        size_t best_idx = (size_t)-1;
        size_t j;

        for (j = 0; j < test_runtime.record_count; ++j) {
            if (test_runtime.records[j].outcome == TEST_OUTCOME_SKIP || used[j])
                continue;

            if (best_idx == (size_t)-1 || test_runtime.records[j].ms > test_runtime.records[best_idx].ms)
                best_idx = j;
        }

        if (best_idx == (size_t)-1)
            break;

        printf("  %d. %s", printed + 1, test_text_c_str(test_runtime.records[best_idx].path));
        test_print_tags(test_runtime.records[best_idx].tags);
        printf(" ");
        test_print_time(test_runtime.records[best_idx].ms);
        bool passed = test_runtime.records[best_idx].outcome == TEST_OUTCOME_PASS;
        printf(passed ? " " C_BOLD C_GREEN "[PASS]" C_RESET "\n"
                             : " " C_BOLD C_RED "[FAIL]" C_RESET "\n");
        used[best_idx] = 1u;
        ++printed;
    }

    free(used);
}

static void test_report_missing_config_keys(void)
{
    size_t i;

    if (test_suite_mode == TEST_CONFIG_NONE || test_runtime.missing_config_count == 0)
        return;

    fprintf(
        stderr,
        C_YELLOW
        "test_harness: discovered %d test/group path%s without config keys; regenerated them as enabled.\n" C_RESET,
        test_runtime.missing_config_count, test_runtime.missing_config_count == 1 ? "" : "s");
    for (i = 0; i < test_runtime.missing_config_path_count; ++i)
        fprintf(stderr, C_YELLOW "  added: %s\n" C_RESET, test_text_c_str(test_runtime.missing_config_paths[i]));
}

/* Suites may override this early entry to dispatch their native fixture subprocesses. */
__attribute__((weak)) void test_program_setup(void)
{
}

/* A plain core executable needs no configuration files, library or database. */
__attribute__((weak)) const test_harness_adapter_t *test_harness_adapter(void)
{
    return NULL;
}

static int test_runtime_arguments(int argc, char **argv)
{
    if (test_runtime.adapter && test_runtime.adapter->start)
        return test_runtime.adapter->start(argc, argv, test_suite_mode, &test_runtime.plan_only);
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--help")) {
            puts("Native tests: [--test-plan] [--test-memory]\nNo optional configuration or storage adapter is linked.");
            return 1;
        }
        if (!strcmp(argv[i], "--test-memory")) continue;
        if (!strcmp(argv[i], "--test-plan")) test_runtime.plan_only = true;
        else { fprintf(stderr, "Unknown test argument: %s\n", argv[i]); return -1; }
    }
    return 0;
}

static void test_runtime_report(const char *executable, int exit_code, double elapsed_ms)
{
    const test_harness_adapter_t *adapter = test_runtime.adapter;
    if (!adapter || !adapter->report_begin || !adapter->report_record || !adapter->report_finish ||
        !test_runtime.record_count || !adapter->report_begin(executable)) return;
    for (size_t i = 0; i < test_runtime.record_count; ++i) {
        const test_record_t *record = &test_runtime.records[i];
        test_harness_record_t view = {
            .source = test_text_c_str(record->file), .name = test_text_c_str(record->name),
            .path = test_text_c_str(record->path), .parent = test_text_c_str(record->parent),
            .tags = test_text_c_str(record->tags), .detail = test_text_c_str(record->failure_detail),
            .outcome = test_outcome_name(record->outcome), .declaration_line = record->declaration_line,
            .enabled = record->enabled, .is_group = record->is_group, .is_output = record->is_output,
            .elapsed_ms = record->ms, .finished_utc = record->finished_utc};
        adapter->report_record(&view);
    }
    adapter->report_finish(exit_code, elapsed_ms);
}

int main(int argc, char **argv)
{
    int rc;
    int exit_code;
    test_program_setup();
    test_runtime.adapter = test_harness_adapter();
    int arguments = test_runtime_arguments(argc, argv);
    if (arguments) {
        test_runtime_release();
        return arguments > 0 ? 0 : 2;
    }
    struct timespec started, finished;
    clock_gettime(CLOCK_MONOTONIC, &started);
    test_runtime.plan_known = test_runtime.adapter && test_runtime.adapter->plan &&
        test_runtime.adapter->plan(test_suite_source, &test_runtime.planned_cases);
    if (test_runtime.plan_known) printf("PLAN: %zu tests\n", test_runtime.planned_cases);
    else puts("PLAN: discovery");
    fflush(stdout);
    if (test_runtime.plan_only) {
        if (test_runtime.adapter && test_runtime.adapter->finish_selection)
            test_runtime.adapter->finish_selection(true);
        test_runtime_release();
        return 0;
    }
    if (test_suite_setup_hook) {
        test_run_suite_setup_case();
        if (test_runtime.failure_count > 0) {
            rc = 0;
        } else {
            rc = tests_main();
        }
    } else {
        rc = tests_main();
    }
    if (test_runtime.adapter && test_runtime.adapter->finish_selection)
        test_runtime.adapter->finish_selection(test_runtime.abort_requested != 0);
    if (test_runtime.adapter && test_runtime.adapter->report_missing && test_runtime.adapter->report_missing())
        test_report_missing_config_keys();
    exit_code = rc ? rc : test_exit_code();

    /* Reconcile observers when a case's diagnostic output did not end on a fresh line. */
    putchar('\n');
    if (!test_runtime.abort_requested && !rc && (test_runtime.completed_cases || !test_runtime.failure_count))
        printf("PROGRESS: %zu/%zu\n", test_runtime.completed_cases, test_runtime.completed_cases);
    else if (test_runtime.plan_known)
        printf("PROGRESS: %zu/%zu\n", test_runtime.completed_cases, test_runtime.planned_cases);
    printf("COUNTS: %d passed, %d failed\n", test_runtime.passed_cases + test_runtime.output_pass_count,
                  test_runtime.failed_cases + test_runtime.output_failure_count);
    printf("\n" C_CYAN "SUMMARY: " C_RESET "%d run, " C_BOLD C_GREEN "%d passed" C_RESET ", " C_BOLD C_RED "%d failed" C_RESET
                  ", " C_YELLOW "%d skipped" C_RESET,
                  test_runtime.run_count, test_runtime.passed_cases, test_runtime.failed_cases, test_runtime.skip_count);
    if (test_runtime.group_count)
        printf(", " C_CYAN "%d group%s" C_RESET, test_runtime.group_count, test_runtime.group_count == 1 ? "" : "s");
    if (test_runtime.output_count)
        printf(", " C_CYAN "%d output example%s" C_RESET, test_runtime.output_count,
                      test_runtime.output_count == 1 ? "" : "s");
    if (test_runtime.output_count)
        printf(" (" C_BOLD C_GREEN "%d passed" C_RESET ", " C_BOLD C_RED "%d failed" C_RESET ", " C_YELLOW "%d skipped" C_RESET
                      ")",
                      test_runtime.output_pass_count, test_runtime.output_failure_count, test_runtime.output_skip_count);
    if (test_runtime.abort_requested)
        printf(", " C_MAGENTA "fail-fast stop" C_RESET);
    test_print_time(test_runtime.total_ms);
    printf("\n");
    test_print_slowest_cases();
    fflush(stdout);
    clock_gettime(CLOCK_MONOTONIC, &finished);
    test_runtime_report(argv[0], exit_code, test_elapsed_ms(started, finished));
    test_runtime_release();
    if (test_post_summary_hook)
        test_post_summary_hook();

    return exit_code;
}
