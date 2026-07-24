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
#include "ustring.h"

typedef enum {
    TEST_OUTCOME_PASS,
    TEST_OUTCOME_FAIL,
    TEST_OUTCOME_SKIP
} test_outcome_t;

typedef struct {
    string_t *file;
    string_t *name;
    string_t *parent;
    string_t *path;
    string_t *tags;
    string_t *failure_file;
    string_t *failure_detail;
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
} test_record_t;

typedef struct {
    string_t *name;
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

static int g_test_run_count = 0;
static int g_test_group_count = 0;
static int g_test_failure_count = 0;
static int g_test_skip_count = 0;
static int g_test_passed_cases = 0;
static int g_test_failed_cases = 0;
static double g_test_total_ms = 0.0;
static int g_test_nested_run_count = 0;
static int g_test_output_count = 0;
static int g_test_output_pass_count = 0;
static int g_test_output_failure_count = 0;
static int g_test_output_skip_count = 0;
static int g_test_missing_config_count = 0;
static int g_test_abort_requested = 0;
/* Borrowed from the currently executing case; owned strings are freed below. */
static const string_t *g_test_current_path = NULL;
static string_t *g_test_last_fail_file = NULL;
static int g_test_last_fail_line = 0;
static string_t *g_test_last_fail_detail = NULL;
static int g_test_skip_requested = 0;
static string_t *g_test_last_skip_file = NULL;
static int g_test_last_skip_line = 0;
static string_t *g_test_last_skip_detail = NULL;
static string_t **g_test_missing_config_paths = NULL;
static size_t g_test_missing_config_path_count = 0u;
static size_t g_test_missing_config_path_cap = 0u;

static test_record_t *g_test_records = NULL;
static size_t g_test_record_count = 0u;
static size_t g_test_record_cap = 0u;
static test_validity_registry_entry_t *g_test_validity_registry = NULL;
static size_t g_test_validity_registry_count = 0u;
static size_t g_test_validity_registry_cap = 0u;
static test_cleanup_entry_t *g_test_case_cleanups = NULL;
static size_t g_test_case_cleanup_count = 0u;
static size_t g_test_case_cleanup_cap = 0u;
/* Borrowed from the per-case cleanup stack and cleared after that stack runs. */
static const string_t *g_test_case_temp_dir = NULL;

static int test_run_case_fixture_setup(const char *file, int line);
static void test_run_case_fixture_teardown(const char *file, int line);
static void test_run_in_group_text(const string_t *file_text,
                                   int line,
                                   const string_t *name_text,
                                   const string_t *parent_text,
                                   const string_t *tags_text,
                                   const char *file_boundary,
                                   test_fn fn);
static int test_case_register_cleanup(test_cleanup_fn fn, void *ctx);
static void test_case_cleanup_all(void);
static void test_case_cleanup_string(void *ctx);
static void test_case_cleanup_temp_tree(void *ctx);
static const string_t *test_case_temp_dir_text(void);
static int test_remove_tree_string(const string_t *path_text);

static string_t *test_string_clone_or_empty(const string_t *text)
{
    return text ? string_clone(text) : string_new_with("");
}

static string_t *test_string_from_boundary(const char *text)
{
    return string_new_with(text ? text : "");
}

static int test_boundary_text_equals(const char *left, const char *right)
{
    string_t *left_text = test_string_from_boundary(left);
    string_t *right_text = test_string_from_boundary(right);
    int equal = 0;

    if (left_text && right_text)
        equal = string_compare(left_text, right_text) == 0;

    string_free(left_text);
    string_free(right_text);
    return equal;
}

static void test_string_replace(string_t **target, string_t *replacement)
{
    if (!target)
        return;

    if (!replacement)
        replacement = string_new_with("");

    string_free(*target);
    *target = replacement;
}

static void test_string_clear_slot(string_t **target)
{
    if (!target)
        return;

    if (*target)
        string_clear(*target);
    else
        *target = string_new();
}

static void test_string_replace_boundary(string_t **target, const char *text)
{
    test_string_replace(target, test_string_from_boundary(text));
}

static int test_string_has_text(const string_t *text)
{
    return text && string_length(text) > 0u;
}

typedef bool (*test_config_string_query_fn)(const string_t *file,
                                            const string_t *func,
                                            const string_t *parent);

static bool test_harness_config_query(const char *file,
                                      const char *func,
                                      const char *parent,
                                      bool fallback,
                                      test_config_string_query_fn query)
{
    string_t *file_text = test_string_from_boundary(file);
    string_t *func_text = test_string_from_boundary(func);
    string_t *parent_text = parent ? test_string_from_boundary(parent) : NULL;
    bool result = fallback;

    if (query &&
        file_text &&
        func_text &&
        (!parent || parent_text))
        result = query(file_text, func_text, parent_text);

    string_free(parent_text);
    string_free(func_text);
    string_free(file_text);
    return result;
}

bool test_harness_config_is_enabled(const char *file,
                                    const char *func,
                                    const char *parent)
{
    return test_harness_config_query(file,
                                     func,
                                     parent,
                                     true,
                                     test_config_is_enabled);
}

bool test_harness_config_has_key(const char *file,
                                 const char *func,
                                 const char *parent)
{
    return test_harness_config_query(file,
                                     func,
                                     parent,
                                     false,
                                     test_config_has_key_for);
}

static double test_elapsed_ms(struct timespec t0, struct timespec t1)
{
    return (t1.tv_sec - t0.tv_sec) * 1000.0
         + (t1.tv_nsec - t0.tv_nsec) / 1e6;
}

static void test_print_time(double ms)
{
    string_printf(C_GREY "  [");
    if (ms < 0.001) {
        long ns = (long)(ms * 1000000.0 + 0.5);

        if (ns < 1)
            string_printf("< 1 ns");
        else
            string_printf("%ld ns", ns);
    } else if (ms < 1.0) {
        string_printf("%.1f µs", ms * 1000.0);
    } else if (ms < 1000.0) {
        string_printf("%.1f ms", ms);
    } else {
        string_printf("%.2f s", ms / 1000.0);
    }
    string_printf("]" C_RESET);
}

void test_section(const char *title)
{
    string_t *title_text = test_string_from_boundary(title);

    if (!title_text)
        return;

    string_printf(C_BOLD C_CYAN "=== %S ===\n" C_RESET, title_text);
    string_free(title_text);
}

void test_register_validity_checker(const char *name,
                                    const test_validity_contract_t *contract)
{
    string_t *name_text;
    size_t i;

    if (!name || !*name || !contract)
        return;

    name_text = test_string_from_boundary(name);
    if (!name_text)
        return;

    for (i = 0; i < g_test_validity_registry_count; ++i) {
        if (string_compare(g_test_validity_registry[i].name, name_text) == 0) {
            g_test_validity_registry[i].contract = contract;
            string_free(name_text);
            return;
        }
    }

    if (g_test_validity_registry_count == g_test_validity_registry_cap) {
        size_t new_cap = g_test_validity_registry_cap ? g_test_validity_registry_cap * 2u : 8u;
        test_validity_registry_entry_t *grown =
            realloc(g_test_validity_registry, new_cap * sizeof(*grown));

        if (!grown) {
            string_free(name_text);
            return;
        }

        g_test_validity_registry = grown;
        g_test_validity_registry_cap = new_cap;
    }

    g_test_validity_registry[g_test_validity_registry_count].name = name_text;
    g_test_validity_registry[g_test_validity_registry_count].contract = contract;
    g_test_validity_registry_count++;
}

const test_validity_contract_t *test_find_validity_checker(const char *name)
{
    string_t *name_text;
    const test_validity_contract_t *found = NULL;
    size_t i;

    if (!name || !*name)
        return NULL;

    name_text = test_string_from_boundary(name);
    if (!name_text)
        return NULL;

    for (i = 0; i < g_test_validity_registry_count; ++i) {
        if (string_compare(g_test_validity_registry[i].name, name_text) == 0) {
            found = g_test_validity_registry[i].contract;
            break;
        }
    }

    string_free(name_text);
    return found;
}

bool test_require_validity_checker(const char *name,
                                   const char *file,
                                   int line)
{
    if (test_find_validity_checker(name))
        return true;

    string_printf(C_RED
           "    Setup failed at %s:%d: missing validity checker '%s'\n"
           C_RESET,
           file,
           line,
           name ? name : "(null)");
    test_set_failure_detailf("missing validity checker '%s'",
                             name ? name : "(null)");
    g_test_failure_count++;
    test_string_replace_boundary(&g_test_last_fail_file, file);
    g_test_last_fail_line = line;
    return false;
}

static int test_case_register_cleanup(test_cleanup_fn fn, void *ctx)
{
    if (!fn)
        return 0;

    if (g_test_case_cleanup_count == g_test_case_cleanup_cap) {
        size_t new_cap = g_test_case_cleanup_cap ? g_test_case_cleanup_cap * 2u : 8u;
        test_cleanup_entry_t *grown =
            realloc(g_test_case_cleanups, new_cap * sizeof(*grown));

        if (!grown)
            return 0;

        g_test_case_cleanups = grown;
        g_test_case_cleanup_cap = new_cap;
    }

    g_test_case_cleanups[g_test_case_cleanup_count].fn = fn;
    g_test_case_cleanups[g_test_case_cleanup_count].ctx = ctx;
    g_test_case_cleanup_count++;
    return 1;
}

static void test_case_cleanup_string(void *ctx)
{
    string_free((string_t *)ctx);
}

static int test_remove_tree_string(const string_t *path_text)
{
    struct stat st;
    const char *path = test_string_has_text(path_text)
        ? string_c_str(path_text)
        : NULL;

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
            string_t *entry_text;
            string_t *child_text;

            if (test_boundary_text_equals(ent->d_name, ".") ||
                test_boundary_text_equals(ent->d_name, ".."))
                continue;

            entry_text = test_string_from_boundary(ent->d_name);
            if (!entry_text) {
                closedir(dir);
                return 0;
            }

            child_text = string_sprintf("%S/%S", path_text, entry_text);
            string_free(entry_text);
            if (!child_text) {
                closedir(dir);
                return 0;
            }

            if (!test_remove_tree_string(child_text)) {
                string_free(child_text);
                closedir(dir);
                return 0;
            }
            string_free(child_text);
        }

        closedir(dir);
        return rmdir(path) == 0 || errno == ENOENT;
    }

    return unlink(path) == 0 || errno == ENOENT;
}

static void test_case_cleanup_temp_tree(void *ctx)
{
    string_t *path = (string_t *)ctx;

    if (path)
        (void)test_remove_tree_string(path);
    string_free(path);
}

static void test_case_cleanup_all(void)
{
    while (g_test_case_cleanup_count > 0u) {
        test_cleanup_entry_t entry = g_test_case_cleanups[--g_test_case_cleanup_count];

        if (entry.fn)
            entry.fn(entry.ctx);
    }

    g_test_case_temp_dir = NULL;
}

static void test_print_tags(const string_t *tags)
{
    if (!test_string_has_text(tags))
        return;
    string_printf(" " C_DIM "{%S}" C_RESET, tags);
}

static const char *test_outcome_name(test_outcome_t outcome)
{
    switch (outcome) {
        case TEST_OUTCOME_PASS: return "pass";
        case TEST_OUTCOME_FAIL: return "fail";
        case TEST_OUTCOME_SKIP: return "skip";
        default: return "unknown";
    }
}

static int test_format_value_fallback(const void *value, string_t *out)
{
    if (!out)
        return -1;

    if (!value)
        return string_append_cstr(out, "<null>");

    return string_append_format(out, "%p", value);
}

static int test_int_equal(const void *actual, const void *expected, void *ctx)
{
    (void)ctx;
    return *(const int *)actual == *(const int *)expected;
}

static int test_int_format(const void *value, string_t *out, void *ctx)
{
    (void)ctx;
    return string_append_format(out, "%d", *(const int *)value);
}

static int test_long_equal(const void *actual, const void *expected, void *ctx)
{
    (void)ctx;
    return *(const long *)actual == *(const long *)expected;
}

static int test_long_format(const void *value, string_t *out, void *ctx)
{
    (void)ctx;
    return string_append_format(out, "%ld", *(const long *)value);
}

static int test_cstr_equal(const void *actual, const void *expected, void *ctx)
{
    const char *const *actual_text = (const char *const *)actual;
    const char *const *expected_text = (const char *const *)expected;
    (void)ctx;

    if (!*actual_text && !*expected_text)
        return 1;
    if (!*actual_text || !*expected_text)
        return 0;
    return test_boundary_text_equals(*actual_text, *expected_text);
}

static int test_cstr_format(const void *value, string_t *out, void *ctx)
{
    const char *const *text = (const char *const *)value;
    (void)ctx;

    return string_append_format(out, "\"%s\"", *text ? *text : "<null>");
}

const test_validity_contract_t *test_validity_contract_int(void)
{
    static const test_validity_contract_t contract = {
        "int-equality",
        test_int_equal,
        test_int_format,
        NULL
    };

    return &contract;
}

const test_validity_contract_t *test_validity_contract_long(void)
{
    static const test_validity_contract_t contract = {
        "long-equality",
        test_long_equal,
        test_long_format,
        NULL
    };

    return &contract;
}

const test_validity_contract_t *test_validity_contract_cstr(void)
{
    static const test_validity_contract_t contract = {
        "cstr-equality",
        test_cstr_equal,
        test_cstr_format,
        NULL
    };

    return &contract;
}

static const string_t *test_case_temp_dir_text(void)
{
    char template_buf[] = "/tmp/mars-test-XXXXXX";
    char *created;
    string_t *owned;

    if (g_test_case_temp_dir)
        return g_test_case_temp_dir;

    created = mkdtemp(template_buf);
    if (!created)
        return NULL;

    owned = string_new_with(created);
    if (!owned) {
        (void)rmdir(created);
        return NULL;
    }

    if (!test_case_register_cleanup(test_case_cleanup_temp_tree, owned)) {
        (void)rmdir(created);
        string_free(owned);
        return NULL;
    }

    g_test_case_temp_dir = owned;
    return g_test_case_temp_dir;
}

const char *test_case_temp_dir(void)
{
    const string_t *dir = test_case_temp_dir_text();

    return dir ? string_c_str(dir) : NULL;
}

const char *test_case_temp_path(const char *leafname)
{
    const string_t *dir = test_case_temp_dir_text();
    string_t *leaf_text;
    string_t *path_text;

    if (!dir)
        return NULL;

    leaf_text = test_string_from_boundary((leafname && *leafname) ? leafname : "tmp");
    if (!leaf_text)
        return NULL;

    path_text = string_sprintf("%S/%S", dir, leaf_text);
    string_free(leaf_text);
    if (!path_text)
        return NULL;

    if (!test_case_register_cleanup(test_case_cleanup_string, path_text)) {
        string_free(path_text);
        return NULL;
    }

    return string_c_str(path_text);
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

static int test_missing_config_path_seen(const string_t *full_key)
{
    size_t i;

    for (i = 0; i < g_test_missing_config_path_count; ++i) {
        if (string_compare(g_test_missing_config_paths[i], full_key) == 0)
            return 1;
    }

    return 0;
}

static void test_note_missing_config_path(const string_t *file,
                                          const string_t *path)
{
    string_t *full_key;

    if (test_suite_mode == TEST_CONFIG_NONE ||
        !test_string_has_text(file) ||
        !test_string_has_text(path))
        return;

    full_key = string_sprintf("%S::%S", file, path);
    if (!full_key)
        return;

    if (test_missing_config_path_seen(full_key))
    {
        string_free(full_key);
        return;
    }

    if (g_test_missing_config_path_count == g_test_missing_config_path_cap) {
        size_t new_cap = g_test_missing_config_path_cap ? g_test_missing_config_path_cap * 2u : 16u;
        string_t **grown = realloc(g_test_missing_config_paths, new_cap * sizeof(*grown));

        if (!grown) {
            string_free(full_key);
            return;
        }

        g_test_missing_config_paths = grown;
        g_test_missing_config_path_cap = new_cap;
    }

    g_test_missing_config_paths[g_test_missing_config_path_count++] = full_key;
    g_test_missing_config_count++;
}

static void test_record_case(const string_t *file,
                             int declaration_line,
                             const string_t *name,
                             const string_t *parent,
                             const string_t *path,
                             const string_t *tags,
                             test_outcome_t outcome,
                             int enabled,
                             int is_group,
                             int is_output,
                             int group_passed,
                             int group_failed,
                             int group_skipped,
                             double ms)
{
    test_record_t *next;

    if (g_test_record_count == g_test_record_cap) {
        size_t new_cap = g_test_record_cap ? g_test_record_cap * 2u : 32u;
        test_record_t *grown = realloc(g_test_records, new_cap * sizeof(*grown));

        if (!grown)
            return;

        g_test_records = grown;
        g_test_record_cap = new_cap;
    }

    next = &g_test_records[g_test_record_count++];
    next->file = test_string_clone_or_empty(file);
    next->name = test_string_clone_or_empty(name);
    next->parent = test_string_clone_or_empty(parent);
    next->path = test_string_clone_or_empty(path);
    next->tags = test_string_clone_or_empty(tags);
    next->failure_file = (outcome == TEST_OUTCOME_FAIL || outcome == TEST_OUTCOME_SKIP)
        ? test_string_clone_or_empty(outcome == TEST_OUTCOME_FAIL
                                         ? g_test_last_fail_file
                                         : g_test_last_skip_file)
        : NULL;
    if (outcome == TEST_OUTCOME_FAIL)
        next->failure_detail = test_string_clone_or_empty(g_test_last_fail_detail);
    else if (outcome == TEST_OUTCOME_SKIP)
        next->failure_detail = test_string_clone_or_empty(g_test_last_skip_detail);
    else
        next->failure_detail = NULL;
    next->outcome = outcome;
    next->enabled = enabled;
    next->is_group = is_group;
    next->is_output = is_output;
    next->declaration_line = declaration_line;
    if (outcome == TEST_OUTCOME_FAIL)
        next->failure_line = g_test_last_fail_line;
    else if (outcome == TEST_OUTCOME_SKIP)
        next->failure_line = g_test_last_skip_line;
    else
        next->failure_line = 0;
    next->group_passed = group_passed;
    next->group_failed = group_failed;
    next->group_skipped = group_skipped;
    next->ms = ms;
}

static void test_destroy_records(void)
{
    size_t i;

    for (i = 0; i < g_test_record_count; ++i) {
        string_free(g_test_records[i].file);
        string_free(g_test_records[i].name);
        string_free(g_test_records[i].parent);
        string_free(g_test_records[i].path);
        string_free(g_test_records[i].tags);
        string_free(g_test_records[i].failure_file);
        string_free(g_test_records[i].failure_detail);
    }

    free(g_test_records);
    g_test_records = NULL;
    g_test_record_count = 0u;
    g_test_record_cap = 0u;

    if (g_test_missing_config_paths) {
        size_t i;

        for (i = 0; i < g_test_missing_config_path_count; ++i)
            string_free(g_test_missing_config_paths[i]);
        free(g_test_missing_config_paths);
    }
    g_test_missing_config_paths = NULL;
    g_test_missing_config_path_count = 0u;
    g_test_missing_config_path_cap = 0u;

    if (g_test_validity_registry) {
        size_t i;

        for (i = 0; i < g_test_validity_registry_count; ++i)
            string_free(g_test_validity_registry[i].name);
        free(g_test_validity_registry);
    }
    g_test_validity_registry = NULL;
    g_test_validity_registry_count = 0u;
    g_test_validity_registry_cap = 0u;

    free(g_test_case_cleanups);
    g_test_case_cleanups = NULL;
    g_test_case_cleanup_count = 0u;
    g_test_case_cleanup_cap = 0u;

    g_test_case_temp_dir = NULL;

    string_free(g_test_last_fail_detail);
    g_test_last_fail_detail = NULL;
    string_free(g_test_last_fail_file);
    g_test_last_fail_file = NULL;
    string_free(g_test_last_skip_detail);
    g_test_last_skip_detail = NULL;
    string_free(g_test_last_skip_file);
    g_test_last_skip_file = NULL;
}

void test_set_failure_detailf(const char *fmt, ...)
{
    va_list ap;
    string_t *detail;

    if (!fmt) {
        test_string_clear_slot(&g_test_last_fail_detail);
        return;
    }

    va_start(ap, fmt);
    detail = string_vsprintf(fmt, ap);
    va_end(ap);

    test_string_replace(&g_test_last_fail_detail, detail);
}

void test_clear_failure_detail(void)
{
    test_string_clear_slot(&g_test_last_fail_detail);
    test_string_clear_slot(&g_test_last_fail_file);
    g_test_last_fail_line = 0;
    test_string_clear_slot(&g_test_last_skip_detail);
    test_string_clear_slot(&g_test_last_skip_file);
    g_test_last_skip_line = 0;
    g_test_skip_requested = 0;
}

void test_mark_failure(const char *file, int line, const char *detail)
{
    g_test_failure_count++;
    test_string_replace_boundary(&g_test_last_fail_file, file);
    g_test_last_fail_line = line;
    test_set_failure_detailf("%s", detail ? detail : "test failed");
}

void test_fail(const char *file, int line)
{
    test_mark_failure(file, line, "explicit test failure");
}

void test_mark_skip(const char *file, int line, const char *detail)
{
    g_test_skip_requested = 1;
    test_string_replace_boundary(&g_test_last_skip_file, file);
    g_test_last_skip_line = line;
    test_string_replace(&g_test_last_skip_detail,
                        string_sprintf("%s", detail ? detail : "test skipped"));
}

bool test_request_skip(const char *file, int line, const char *detail)
{
    test_mark_skip(file, line, detail);
    return false;
}

bool test_assert_true(bool expr,
                      const char *file,
                      int line,
                      const char *detail)
{
    if (expr)
        return true;

    string_printf(C_RED "    Assertion failed at %s:%d: %s\n" C_RESET,
           file, line, detail ? detail : "expected true");
    test_set_failure_detailf("%s", detail ? detail : "expected true");
    g_test_failure_count++;
    test_string_replace_boundary(&g_test_last_fail_file, file);
    g_test_last_fail_line = line;
    return false;
}

bool test_assert_false(bool expr,
                       const char *file,
                       int line,
                       const char *detail)
{
    if (!expr)
        return true;

    string_printf(C_RED "    Assertion failed at %s:%d: %s\n" C_RESET,
           file, line, detail ? detail : "expected false");
    test_set_failure_detailf("%s", detail ? detail : "expected false");
    g_test_failure_count++;
    test_string_replace_boundary(&g_test_last_fail_file, file);
    g_test_last_fail_line = line;
    return false;
}

bool test_assert_int_eq(int actual,
                        int expected,
                        const char *file,
                        int line)
{
    if (actual == expected)
        return true;

    string_printf(C_RED "    Assertion failed at %s:%d: expected %d, got %d\n"
           C_RESET,
           file, line, expected, actual);
    test_set_failure_detailf("expected %d, got %d", expected, actual);
    g_test_failure_count++;
    test_string_replace_boundary(&g_test_last_fail_file, file);
    g_test_last_fail_line = line;
    return false;
}

bool test_assert_long_eq(long actual,
                         long expected,
                         const char *file,
                         int line)
{
    if (actual == expected)
        return true;

    string_printf(C_RED "    Assertion failed at %s:%d: expected %ld, got %ld\n"
           C_RESET,
           file, line, expected, actual);
    test_set_failure_detailf("expected %ld, got %ld", expected, actual);
    g_test_failure_count++;
    test_string_replace_boundary(&g_test_last_fail_file, file);
    g_test_last_fail_line = line;
    return false;
}

bool test_assert_double_eq(double actual,
                           double expected,
                           double eps,
                           const char *file,
                           int line)
{
    if (fabs(actual - expected) <= eps)
        return true;

    string_printf(C_RED
           "    Assertion failed at %s:%d: expected %.12f, got %.12f\n"
           C_RESET,
           file, line, expected, actual);
    test_set_failure_detailf("expected %.12f, got %.12f", expected, actual);
    g_test_failure_count++;
    test_string_replace_boundary(&g_test_last_fail_file, file);
    g_test_last_fail_line = line;
    return false;
}

bool test_assert_not_null(const void *ptr,
                          const char *file,
                          int line)
{
    if (ptr)
        return true;

    string_printf(C_RED
           "    Assertion failed at %s:%d: expected non-null pointer\n"
           C_RESET,
           file, line);
    test_set_failure_detailf("expected non-null pointer");
    g_test_failure_count++;
    test_string_replace_boundary(&g_test_last_fail_file, file);
    g_test_last_fail_line = line;
    return false;
}

bool test_assert_null(const void *ptr,
                      const char *file,
                      int line)
{
    if (!ptr)
        return true;

    string_printf(C_RED
           "    Assertion failed at %s:%d: expected NULL pointer\n"
           C_RESET,
           file, line);
    test_set_failure_detailf("expected NULL pointer");
    g_test_failure_count++;
    test_string_replace_boundary(&g_test_last_fail_file, file);
    g_test_last_fail_line = line;
    return false;
}

bool test_assert_validity(const test_validity_contract_t *contract,
                          const void *actual,
                          const void *expected,
                          const char *file,
                          int line)
{
    string_t *actual_text = NULL;
    string_t *expected_text = NULL;
    const char *contract_name;
    int ok;

    if (!contract || !contract->equal) {
        string_printf(C_RED
               "    Assertion failed at %s:%d: invalid validity contract\n"
               C_RESET,
               file, line);
        test_set_failure_detailf("invalid validity contract");
        g_test_failure_count++;
        test_string_replace_boundary(&g_test_last_fail_file, file);
        g_test_last_fail_line = line;
        return false;
    }

    ok = contract->equal(actual, expected, contract->ctx);
    if (ok)
        return true;

    actual_text = string_new();
    expected_text = string_new();
    if (!actual_text || !expected_text) {
        string_free(actual_text);
        string_free(expected_text);
        string_printf(C_RED
               "    Assertion failed at %s:%d: out of memory formatting validity failure\n"
               C_RESET,
               file, line);
        test_set_failure_detailf("out of memory formatting validity failure");
        g_test_failure_count++;
        test_string_replace_boundary(&g_test_last_fail_file, file);
        g_test_last_fail_line = line;
        return false;
    }

    if (contract->format) {
        if (contract->format(expected, expected_text, contract->ctx) != 0) {
            string_clear(expected_text);
            (void)test_format_value_fallback(expected, expected_text);
        }
        if (contract->format(actual, actual_text, contract->ctx) != 0) {
            string_clear(actual_text);
            (void)test_format_value_fallback(actual, actual_text);
        }
    } else if (test_format_value_fallback(expected, expected_text) != 0 ||
               test_format_value_fallback(actual, actual_text) != 0) {
        string_free(actual_text);
        string_free(expected_text);
        string_printf(C_RED
               "    Assertion failed at %s:%d: out of memory formatting validity failure\n"
               C_RESET,
               file, line);
        test_set_failure_detailf("out of memory formatting validity failure");
        g_test_failure_count++;
        test_string_replace_boundary(&g_test_last_fail_file, file);
        g_test_last_fail_line = line;
        return false;
    }

    contract_name = (contract->name && *contract->name)
        ? contract->name
        : "validity";

    string_printf(C_RED
           "    Assertion failed at %s:%d [%s]: expected %S, got %S\n"
           C_RESET,
           file, line, contract_name, expected_text, actual_text);
    test_set_failure_detailf("[%s] expected %S, got %S",
                             contract_name,
                             expected_text,
                             actual_text);
    string_free(actual_text);
    string_free(expected_text);
    g_test_failure_count++;
    test_string_replace_boundary(&g_test_last_fail_file, file);
    g_test_last_fail_line = line;
    return false;
}

bool test_assert_validity_named(const char *name,
                                const void *actual,
                                const void *expected,
                                const char *file,
                                int line)
{
    const test_validity_contract_t *contract = test_find_validity_checker(name);

    if (!contract) {
        string_printf(C_RED
               "    Assertion failed at %s:%d: missing named validity checker: %s\n"
               C_RESET,
               file,
               line,
               name ? name : "(null)");
        test_set_failure_detailf("missing named validity checker: %s",
                                 name ? name : "(null)");
        g_test_failure_count++;
        test_string_replace_boundary(&g_test_last_fail_file, file);
        g_test_last_fail_line = line;
        return false;
    }

    return test_assert_validity(contract, actual, expected, file, line);
}

bool test_assert_cstr_eq(const char *actual,
                         const char *expected,
                         const char *file,
                         int line)
{
    const char *actual_text = actual;
    const char *expected_text = expected;

    return test_assert_validity(TEST_VALID_CSTR(),
                                &actual_text,
                                &expected_text,
                                file,
                                line);
}

void test_run_case(const char *file,
                   int line,
                   const char *name,
                   const char *tags,
                   test_fn fn)
{
    test_run_in_group(file, line, name, NULL, tags, fn);
}

void test_run_subtest(const char *file,
                      int line,
                      const char *name,
                      const char *tags,
                      test_fn fn)
{
    string_t *file_text = NULL;
    string_t *name_text = NULL;
    string_t *tags_text = NULL;

    if (!test_string_has_text(g_test_current_path)) {
        string_printf(C_RED
               "    Harness misuse at %s:%d: test_run_subtest(%s) requires an active parent test/group\n"
               C_RESET,
               file,
               line,
               name ? name : "(unnamed)");
        test_mark_failure(file, line, "test_run_subtest requires an active parent");
        return;
    }

    file_text = test_string_from_boundary(file);
    name_text = test_string_from_boundary(name ? name : "(unnamed)");
    tags_text = test_string_from_boundary(tags);
    if (!file_text || !name_text || !tags_text) {
        test_mark_failure(file, line, "out of memory while preparing subtest strings");
        string_free(file_text);
        string_free(name_text);
        string_free(tags_text);
        return;
    }

    test_run_in_group_text(file_text,
                           line,
                           name_text,
                           g_test_current_path,
                           tags_text,
                           file,
                           fn);

    string_free(tags_text);
    string_free(name_text);
    string_free(file_text);
}

static void test_run_in_group_text(const string_t *file_text,
                                   int line,
                                   const string_t *name_text,
                                   const string_t *parent_text,
                                   const string_t *tags_text,
                                   const char *file_boundary,
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
    const string_t *prev_path;
    string_t *full_path_text = NULL;
    const string_t *print_file;
    int had_config_key = 1;
    int fixture_ran = 0;
    int fixture_continue = 1;

    if (g_test_abort_requested)
        return;

    if (parent_text && test_string_has_text(parent_text))
        full_path_text = string_sprintf("%S.%S", parent_text, name_text);
    else
        full_path_text = string_clone(name_text);
    if (!full_path_text) {
        test_mark_failure(file_boundary, line, "out of memory while building test path");
        goto cleanup;
    }

    had_config_key = test_config_has_key_for(file_text, name_text, parent_text) ? 1 : 0;
    enabled = test_config_is_enabled(file_text, name_text, parent_text);
    if (!had_config_key)
        test_note_missing_config_path(file_text, full_path_text);

    g_test_nested_run_count++;

    if (!enabled) {
        if (parent_text)
            string_printf(C_YELLOW "SKIP: %S (in %S)" C_RESET "\n",
                   name_text, parent_text);
        else
            string_printf(C_YELLOW "SKIP: %S" C_RESET "\n", name_text);
        g_test_skip_count++;
        test_record_case(file_text, line, name_text, parent_text, full_path_text, tags_text,
                         TEST_OUTCOME_SKIP, 0, 0, 0, 0, 0, 0, 0.0);
        goto cleanup;
    }

    failure_count_before = g_test_failure_count;
    skip_count_before = g_test_skip_count;
    passed_cases_before = g_test_passed_cases;
    failed_cases_before = g_test_failed_cases;
    nested_runs_before = g_test_nested_run_count;
    total_ms_before = g_test_total_ms;
    prev_path = g_test_current_path;

    test_clear_failure_detail();
    g_test_current_path = full_path_text;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    if (test_fixture_setup_hook) {
        fixture_ran = 1;
        fixture_continue = test_run_case_fixture_setup(file_boundary, line);
    }
    if (fixture_continue &&
        g_test_failure_count == failure_count_before &&
        !g_test_skip_requested) {
        fn();
    }
    if (fixture_ran && fixture_continue)
        test_run_case_fixture_teardown(file_boundary, line);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    g_test_current_path = prev_path;
    test_case_cleanup_all();

    ms = test_elapsed_ms(t0, t1);
    is_group = (g_test_nested_run_count > nested_runs_before);
    disp_ms = is_group ? (g_test_total_ms - total_ms_before) : ms;
    if (!is_group)
        g_test_total_ms += ms;
    if (is_group)
        g_test_group_count++;
    else
        g_test_run_count++;

    if (is_group) {
        int skipped = g_test_skip_count - skip_count_before;
        int passed = g_test_passed_cases - passed_cases_before;
        int failed = g_test_failed_cases - failed_cases_before;
        test_outcome_t outcome = failed == 0 ? TEST_OUTCOME_PASS : TEST_OUTCOME_FAIL;

        string_printf(C_CYAN "GROUP: %S " C_RESET
               "(" C_GREEN "%d passed" C_RESET
               "," C_RED " %d failed" C_RESET
               "," C_YELLOW " %d skipped" C_RESET ")",
               name_text, passed, failed, skipped);
        test_print_tags(tags_text);
        test_record_case(file_text, line, name_text, parent_text, full_path_text, tags_text,
                         outcome, 1, 1, 0, passed, failed, skipped, disp_ms);
    } else if (g_test_skip_requested) {
        g_test_skip_count++;
        print_file = test_string_has_text(g_test_last_skip_file)
            ? g_test_last_skip_file
            : file_text;
        string_printf(C_YELLOW "SKIP: %S " C_YELLOW "(%S:%d)" C_RESET,
               name_text,
               print_file,
               g_test_last_skip_line);
        if (test_string_has_text(g_test_last_skip_detail))
            string_printf(" " C_GREY "[%S]" C_RESET,
                          g_test_last_skip_detail);
        test_print_tags(tags_text);
        test_record_case(file_text, line, name_text, parent_text, full_path_text, tags_text,
                         TEST_OUTCOME_SKIP, 1, 0, 0, 0, 0, 0, disp_ms);
    } else if (g_test_failure_count == failure_count_before) {
        g_test_passed_cases++;
        string_printf(C_BOLD C_GREEN "PASS: "
               C_RESET "%S", name_text);
        test_print_tags(tags_text);
        test_record_case(file_text, line, name_text, parent_text, full_path_text, tags_text,
                         TEST_OUTCOME_PASS, 1, 0, 0, 0, 0, 0, disp_ms);
    } else {
        g_test_failed_cases++;
        print_file = test_string_has_text(g_test_last_fail_file)
            ? g_test_last_fail_file
            : file_text;
        string_printf(C_BOLD C_RED "FAIL: " C_RESET
               "%S " C_RED "(%S:%d)" C_RESET,
               name_text,
               print_file,
               test_string_has_text(g_test_last_fail_file)
                   ? g_test_last_fail_line
                   : 0);
        test_print_tags(tags_text);
        test_record_case(file_text, line, name_text, parent_text, full_path_text, tags_text,
                         TEST_OUTCOME_FAIL, 1, 0, 0, 0, 0, 0, disp_ms);
    }

    test_print_time(disp_ms);
    string_printf("\n");
cleanup:
    string_free(full_path_text);
}

void test_run_in_group(const char *file,
                       int line,
                       const char *name,
                       const char *parent,
                       const char *tags,
                       test_fn fn)
{
    string_t *file_text = NULL;
    string_t *name_text = NULL;
    string_t *parent_text = NULL;
    string_t *tags_text = NULL;

    if (g_test_abort_requested)
        return;

    file_text = test_string_from_boundary(file);
    name_text = test_string_from_boundary(name ? name : "(unnamed)");
    parent_text = parent ? test_string_from_boundary(parent) : NULL;
    tags_text = test_string_from_boundary(tags);
    if (!file_text || !name_text || !tags_text || (parent && !parent_text)) {
        test_mark_failure(file, line, "out of memory while preparing test strings");
        goto cleanup;
    }

    test_run_in_group_text(file_text,
                           line,
                           name_text,
                           parent_text,
                           tags_text,
                           file,
                           fn);

cleanup:
    string_free(tags_text);
    string_free(parent_text);
    string_free(name_text);
    string_free(file_text);
}

void test_run_output_case(const char *file,
                          int line,
                          const char *tags,
                          const char *name,
                          test_fn fn)
{
    test_run_output_in_group(file, line, name, NULL, tags, fn);
}

void test_run_output_in_group(const char *file,
                              int line,
                              const char *name,
                              const char *parent,
                              const char *tags,
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
    string_t *file_text = NULL;
    string_t *name_text = NULL;
    string_t *parent_text = NULL;
    string_t *tags_text = NULL;
    string_t *full_path_text = NULL;
    const string_t *print_file;
    const char *file_boundary;
    const char *stdout_capture_path = NULL;
    int saved_stdout = -1;

    if (g_test_abort_requested)
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
        full_path_text = string_sprintf("%S.%S", parent_text, name_text);
    else
        full_path_text = string_clone(name_text);
    if (!full_path_text) {
        test_mark_failure(file, line, "out of memory while building output path");
        goto cleanup;
    }

    had_config_key = test_config_has_key_for(file_text, name_text, parent_text) ? 1 : 0;
    enabled = test_config_is_enabled(file_text, name_text, parent_text);
    if (!had_config_key)
        test_note_missing_config_path(file_text, full_path_text);

    g_test_output_count++;

    if (!enabled) {
        g_test_output_skip_count++;
        string_printf(C_YELLOW "OUTPUT SKIP: %S" C_RESET, name_text);
        test_print_tags(tags_text);
        test_record_case(file_text, line, name_text, parent_text, full_path_text, tags_text,
                         TEST_OUTCOME_SKIP, 0, 0, 1, 0, 0, 0, 0.0);
        string_printf("\n");
        goto cleanup;
    }

    failure_count_before = g_test_failure_count;
    test_clear_failure_detail();
    file_boundary = string_c_str(file_text);

    clock_gettime(CLOCK_MONOTONIC, &t0);
    if (test_fixture_setup_hook) {
        fixture_ran = 1;
        fixture_continue = test_run_case_fixture_setup(file_boundary, line);
    }
    if (fixture_continue &&
        g_test_failure_count == failure_count_before &&
        !g_test_skip_requested) {
        saved_stdout = test_case_begin_stdout_capture("output-example-stdout.txt",
                                                      &stdout_capture_path);
    }
    if (fixture_continue &&
        g_test_failure_count == failure_count_before &&
        !g_test_skip_requested) {
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

    if (g_test_skip_requested) {
        g_test_output_skip_count++;
        print_file = test_string_has_text(g_test_last_skip_file)
            ? g_test_last_skip_file
            : file_text;
        string_printf(C_YELLOW "OUTPUT SKIP: %S " C_YELLOW "(%S:%d)" C_RESET,
               name_text,
               print_file,
               g_test_last_skip_line);
        if (test_string_has_text(g_test_last_skip_detail))
            string_printf(" " C_GREY "[%S]" C_RESET,
                          g_test_last_skip_detail);
        test_print_tags(tags_text);
        test_record_case(file_text, line, name_text, parent_text, full_path_text, tags_text,
                         TEST_OUTCOME_SKIP, 1, 0, 1, 0, 0, 0, ms);
    } else if (g_test_failure_count == failure_count_before) {
        g_test_output_pass_count++;
        string_printf(C_BOLD C_CYAN "OUTPUT: "
               C_RESET "%S", name_text);
        test_print_tags(tags_text);
        test_record_case(file_text, line, name_text, parent_text, full_path_text, tags_text,
                         TEST_OUTCOME_PASS, 1, 0, 1, 0, 0, 0, ms);
    } else {
        g_test_output_failure_count++;
        print_file = test_string_has_text(g_test_last_fail_file)
            ? g_test_last_fail_file
            : file_text;
        string_printf(C_BOLD C_RED "OUTPUT FAIL: " C_RESET
               "%S " C_RED "(%S:%d)" C_RESET,
               name_text,
               print_file,
               test_string_has_text(g_test_last_fail_file)
                   ? g_test_last_fail_line
                   : 0);
        test_print_tags(tags_text);
        test_record_case(file_text, line, name_text, parent_text, full_path_text, tags_text,
                         TEST_OUTCOME_FAIL, 1, 0, 1, 0, 0, 0, ms);
    }

    test_print_time(ms);
    string_printf("\n");
    if (stdout_capture_path && *stdout_capture_path)
        test_print_capture_file(stdout_capture_path);
    test_case_cleanup_all();
cleanup:
    string_free(full_path_text);
    string_free(tags_text);
    string_free(parent_text);
    string_free(name_text);
    string_free(file_text);
}

static void test_run_suite_setup_case(void)
{
    int failure_count_before;

    if (!test_suite_setup_hook || g_test_abort_requested)
        return;

    failure_count_before = g_test_failure_count;
    test_clear_failure_detail();

    if (!test_suite_setup_hook() && g_test_failure_count == failure_count_before)
        test_mark_failure(__FILE__, __LINE__, "suite setup returned false");

    if (g_test_failure_count == failure_count_before) {
        string_printf(C_GREEN "validity contract ok - ready\n" C_RESET);
    } else {
        string_printf(C_BOLD C_RED "SETUP FAIL: " C_RESET
               C_RED "(%s:%d)" C_RESET,
               test_string_has_text(g_test_last_fail_file)
                   ? string_c_str(g_test_last_fail_file)
                   : __FILE__,
               g_test_last_fail_line);
        if (test_string_has_text(g_test_last_fail_detail))
            string_printf(" " C_GREY "[%S]" C_RESET,
                          g_test_last_fail_detail);
        string_printf("\n");
    }
}

static int test_run_case_fixture_setup(const char *file, int line)
{
    int failure_count_before;

    if (!test_fixture_setup_hook || g_test_abort_requested)
        return 1;

    failure_count_before = g_test_failure_count;
    if (!test_fixture_setup_hook()) {
        if (!g_test_skip_requested &&
            g_test_failure_count == failure_count_before) {
            test_mark_failure(file, line, "test fixture setup returned false");
        }
        return 0;
    }

    return !g_test_skip_requested;
}

static void test_run_case_fixture_teardown(const char *file, int line)
{
    int failure_count_before;

    if (!test_fixture_teardown_hook || g_test_abort_requested)
        return;

    failure_count_before = g_test_failure_count;
    if (!test_fixture_teardown_hook() &&
        !g_test_skip_requested &&
        g_test_failure_count == failure_count_before) {
        test_mark_failure(file, line, "test fixture teardown returned false");
    }
}

int test_exit_code(void)
{
    return g_test_failure_count ? 1 : 0;
}

static void test_print_slowest_cases(void)
{
    int limit = 5;
    int printed = 0;
    unsigned char *used;

    if (limit <= 0 || g_test_record_count == 0u)
        return;

    used = calloc(g_test_record_count, sizeof(*used));
    if (!used)
        return;

    string_printf(C_CYAN "SLOWEST: " C_RESET "\n");

    while (printed < limit) {
        size_t best_idx = (size_t)-1;
        size_t j;

        for (j = 0; j < g_test_record_count; ++j) {
            if (g_test_records[j].outcome == TEST_OUTCOME_SKIP ||
                used[j])
                continue;

            if (best_idx == (size_t)-1 ||
                g_test_records[j].ms > g_test_records[best_idx].ms)
                best_idx = j;
        }

        if (best_idx == (size_t)-1)
            break;

        string_printf("  %d. %S", printed + 1,
               g_test_records[best_idx].path);
        test_print_tags(g_test_records[best_idx].tags);
        string_printf(" ");
        test_print_time(g_test_records[best_idx].ms);
        string_printf(" " C_GREY "[%s]" C_RESET "\n",
               test_outcome_name(g_test_records[best_idx].outcome));
        used[best_idx] = 1u;
        ++printed;
    }

    free(used);
}

static void test_report_missing_config_keys(void)
{
    size_t i;

    if (test_suite_mode == TEST_CONFIG_NONE || g_test_missing_config_count == 0)
        return;

    string_fprintf(stderr,
            C_YELLOW
            "test_harness: discovered %d test/group path%s without config keys; regenerated them as enabled.\n"
            C_RESET,
            g_test_missing_config_count,
            g_test_missing_config_count == 1 ? "" : "s");
    for (i = 0; i < g_test_missing_config_path_count; ++i)
        string_fprintf(stderr, C_YELLOW "  added: %S\n" C_RESET,
                g_test_missing_config_paths[i]);
}

int main(void)
{
    int rc;
    int exit_code;
    int prune_stale_config;
    test_config_set_mode(test_suite_mode);
    if (test_suite_setup_hook) {
        test_run_suite_setup_case();
        if (g_test_failure_count > 0) {
            rc = 0;
        } else {
            rc = tests_main();
        }
    } else {
        rc = tests_main();
    }
    prune_stale_config = !g_test_abort_requested;
    test_config_set_prune_enabled(prune_stale_config);
    if (test_suite_mode != TEST_CONFIG_NONE)
        test_config_save();
    test_report_missing_config_keys();
    test_config_shutdown();
    exit_code = rc ? rc : test_exit_code();

    string_printf("\n" C_CYAN "SUMMARY: " C_RESET
           "%d run, " C_GREEN "%d passed" C_RESET ", "
           C_RED "%d failed" C_RESET ", "
           C_YELLOW "%d skipped" C_RESET,
           g_test_run_count,
           g_test_passed_cases,
           g_test_failed_cases,
           g_test_skip_count);
    if (g_test_group_count)
        string_printf(", " C_CYAN "%d group%s" C_RESET,
               g_test_group_count,
               g_test_group_count == 1 ? "" : "s");
    if (g_test_output_count)
        string_printf(", " C_CYAN "%d output example%s" C_RESET,
               g_test_output_count,
               g_test_output_count == 1 ? "" : "s");
    if (g_test_output_count)
        string_printf(" (" C_GREEN "%d passed" C_RESET
               ", " C_RED "%d failed" C_RESET
               ", " C_YELLOW "%d skipped" C_RESET ")",
               g_test_output_pass_count,
               g_test_output_failure_count,
               g_test_output_skip_count);
    if (g_test_abort_requested)
        string_printf(", " C_MAGENTA "fail-fast stop" C_RESET);
    test_print_time(g_test_total_ms);
    string_printf("\n");
    test_print_slowest_cases();
    test_destroy_records();
    if (test_post_summary_hook)
        test_post_summary_hook();

    return exit_code;
}
