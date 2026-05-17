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

#define TEST_COLOR_GREEN   "\x1b[32m"
#define TEST_COLOR_RED     "\x1b[31m"
#define TEST_COLOR_YELLOW  "\x1b[33m"
#define TEST_COLOR_CYAN    "\x1b[36m"
#define TEST_COLOR_RESET   "\x1b[0m"
#define TEST_COLOR_BOLD    "\x1b[1m"
#define TEST_COLOR_DIM     "\x1b[2m"
#define TEST_COLOR_GREY    "\x1b[90m"
#define TEST_COLOR_MAGENTA "\x1b[95m"

typedef enum {
    TEST_OUTCOME_PASS,
    TEST_OUTCOME_FAIL,
    TEST_OUTCOME_SKIP,
    TEST_OUTCOME_LISTED,
    TEST_OUTCOME_FILTERED
} test_outcome_t;

typedef struct {
    char *file;
    char *name;
    char *parent;
    char *path;
    char *tags;
    char *failure_file;
    char *failure_detail;
    test_outcome_t outcome;
    int enabled;
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
    char *name;
    const test_validity_contract_t *contract;
} test_validity_registry_entry_t;

typedef void (*test_cleanup_fn)(void *ctx);

typedef struct {
    test_cleanup_fn fn;
    void *ctx;
} test_cleanup_entry_t;

typedef struct {
    char *name;
    char *old_value;
    int had_old_value;
} test_env_restore_t;

test_suite_setup_fn test_suite_setup_hook __attribute__((weak));
test_fixture_setup_fn test_fixture_setup_hook __attribute__((weak));
test_fixture_teardown_fn test_fixture_teardown_hook __attribute__((weak));

static int g_test_run_count = 0;
static int g_test_group_count = 0;
static int g_test_failure_count = 0;
static int g_test_skip_count = 0;
static int g_test_passed_cases = 0;
static int g_test_failed_cases = 0;
static double g_test_total_ms = 0.0;
static int g_test_nested_run_count = 0;
static int g_test_listed_count = 0;
static int g_test_filtered_count = 0;
static int g_test_output_count = 0;
static int g_test_output_pass_count = 0;
static int g_test_output_failure_count = 0;
static int g_test_output_skip_count = 0;
static int g_test_missing_config_count = 0;
static int g_test_abort_requested = 0;
static const char *g_test_current_path = NULL;
static const char *g_test_last_fail_file = NULL;
static int g_test_last_fail_line = 0;
static const char *g_test_last_fail_detail = NULL;
static char g_test_last_fail_detail_buf[512];
static int g_test_skip_requested = 0;
static const char *g_test_last_skip_file = NULL;
static int g_test_last_skip_line = 0;
static const char *g_test_last_skip_detail = NULL;
static char g_test_last_skip_detail_buf[512];
static char **g_test_missing_config_paths = NULL;
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
static test_env_restore_t *g_test_case_env_restores = NULL;
static size_t g_test_case_env_restore_count = 0u;
static size_t g_test_case_env_restore_cap = 0u;
static char *g_test_case_temp_dir = NULL;

static char *test_strdup_owned(const char *text);
static int test_run_case_fixture_setup(const char *file, int line);
static void test_run_case_fixture_teardown(const char *file, int line);
static int test_case_register_cleanup(test_cleanup_fn fn, void *ctx);
static void test_case_cleanup_all(void);
static void test_case_cleanup_string(void *ctx);
static void test_case_cleanup_temp_tree(void *ctx);
static void test_case_cleanup_env_restore(void *ctx);
static int test_remove_tree(const char *path);

static double test_elapsed_ms(struct timespec t0, struct timespec t1)
{
    return (t1.tv_sec - t0.tv_sec) * 1000.0
         + (t1.tv_nsec - t0.tv_nsec) / 1e6;
}

static void test_print_time(double ms)
{
    printf(TEST_COLOR_GREY "  [");
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
    printf("]" TEST_COLOR_RESET);
}

void test_section(const char *title)
{
    if (!title)
        title = "";
    printf(TEST_COLOR_BOLD TEST_COLOR_CYAN "=== %s ===\n" TEST_COLOR_RESET, title);
}

static int test_env_is_truthy(const char *name)
{
    const char *value = getenv(name);

    if (!value || !*value)
        return 0;

    return strcmp(value, "0") != 0 &&
           strcmp(value, "false") != 0 &&
           strcmp(value, "FALSE") != 0 &&
           strcmp(value, "no") != 0 &&
           strcmp(value, "NO") != 0;
}

static const char *test_filter_text(void)
{
    const char *value = getenv("MARS_TEST_FILTER");
    return (value && *value) ? value : NULL;
}

static const char *test_include_tags_text(void)
{
    const char *value = getenv("MARS_TEST_TAGS");
    return (value && *value) ? value : NULL;
}

static const char *test_exclude_tags_text(void)
{
    const char *value = getenv("MARS_TEST_EXCLUDE_TAGS");
    return (value && *value) ? value : NULL;
}

static int test_list_only_enabled(void)
{
    return test_env_is_truthy("MARS_TEST_LIST");
}

static int test_fail_fast_enabled(void)
{
    return test_env_is_truthy("MARS_TEST_FAIL_FAST");
}

static int test_debug_enabled(void)
{
    return test_env_is_truthy("MARS_TEST_DEBUG");
}

static int test_slowest_count(void)
{
    const char *value = getenv("MARS_TEST_SLOWEST");
    char *end = NULL;
    long parsed;

    if (!value || !*value)
        return 5;

    parsed = strtol(value, &end, 10);
    if (!end || *end != '\0' || parsed < 0)
        return 5;
    if (parsed > 100)
        parsed = 100;
    return (int)parsed;
}

static const char *test_report_json_path(void)
{
    const char *value = getenv("MARS_TEST_REPORT_JSON");
    return (value && *value) ? value : NULL;
}

static const char *test_report_junit_path(void)
{
    const char *value = getenv("MARS_TEST_REPORT_JUNIT");
    return (value && *value) ? value : NULL;
}

static int test_show_pass_location_enabled(void)
{
    return test_env_is_truthy("MARS_TEST_SHOW_PASS_LOCATION");
}

void test_register_validity_checker(const char *name,
                                    const test_validity_contract_t *contract)
{
    size_t i;

    if (!name || !*name || !contract)
        return;

    for (i = 0; i < g_test_validity_registry_count; ++i) {
        if (strcmp(g_test_validity_registry[i].name, name) == 0) {
            g_test_validity_registry[i].contract = contract;
            return;
        }
    }

    if (g_test_validity_registry_count == g_test_validity_registry_cap) {
        size_t new_cap = g_test_validity_registry_cap ? g_test_validity_registry_cap * 2u : 8u;
        test_validity_registry_entry_t *grown =
            realloc(g_test_validity_registry, new_cap * sizeof(*grown));

        if (!grown)
            return;

        g_test_validity_registry = grown;
        g_test_validity_registry_cap = new_cap;
    }

    g_test_validity_registry[g_test_validity_registry_count].name = test_strdup_owned(name);
    if (!g_test_validity_registry[g_test_validity_registry_count].name)
        return;
    g_test_validity_registry[g_test_validity_registry_count].contract = contract;
    g_test_validity_registry_count++;
}

const test_validity_contract_t *test_find_validity_checker(const char *name)
{
    size_t i;

    if (!name || !*name)
        return NULL;

    for (i = 0; i < g_test_validity_registry_count; ++i) {
        if (strcmp(g_test_validity_registry[i].name, name) == 0)
            return g_test_validity_registry[i].contract;
    }

    return NULL;
}

bool test_require_validity_checker(const char *name,
                                   const char *file,
                                   int line)
{
    if (test_find_validity_checker(name))
        return true;

    printf(TEST_COLOR_RED
           "    Setup failed at %s:%d: missing validity checker '%s'\n"
           TEST_COLOR_RESET,
           file,
           line,
           name ? name : "(null)");
    test_set_failure_detailf("missing validity checker '%s'",
                             name ? name : "(null)");
    g_test_failure_count++;
    g_test_last_fail_file = file;
    g_test_last_fail_line = line;
    return false;
}

static char *test_strdup_owned(const char *text)
{
    size_t len;
    char *copy;

    if (!text)
        text = "";

    len = strlen(text);
    copy = malloc(len + 1u);
    if (!copy)
        return NULL;

    memcpy(copy, text, len + 1u);
    return copy;
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
    free(ctx);
}

static int test_remove_tree(const char *path)
{
    struct stat st;

    if (!path || !*path)
        return 0;
    if (lstat(path, &st) != 0)
        return errno == ENOENT;

    if (S_ISDIR(st.st_mode)) {
        DIR *dir = opendir(path);
        struct dirent *ent;

        if (!dir)
            return 0;

        while ((ent = readdir(dir)) != NULL) {
            char child[4096];

            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
                continue;

            snprintf(child, sizeof(child), "%s/%s", path, ent->d_name);
            if (!test_remove_tree(child)) {
                closedir(dir);
                return 0;
            }
        }

        closedir(dir);
        return rmdir(path) == 0 || errno == ENOENT;
    }

    return unlink(path) == 0 || errno == ENOENT;
}

static void test_case_cleanup_temp_tree(void *ctx)
{
    char *path = (char *)ctx;

    if (path)
        (void)test_remove_tree(path);
    free(path);
}

static void test_case_cleanup_env_restore(void *ctx)
{
    test_env_restore_t *restore = (test_env_restore_t *)ctx;

    if (!restore)
        return;

    if (restore->had_old_value) {
        (void)setenv(restore->name, restore->old_value ? restore->old_value : "", 1);
    } else {
        (void)unsetenv(restore->name);
    }

    free(restore->name);
    free(restore->old_value);
    restore->name = NULL;
    restore->old_value = NULL;
    restore->had_old_value = 0;
}

static void test_case_cleanup_all(void)
{
    while (g_test_case_cleanup_count > 0u) {
        test_cleanup_entry_t entry = g_test_case_cleanups[--g_test_case_cleanup_count];

        if (entry.fn)
            entry.fn(entry.ctx);
    }

    g_test_case_temp_dir = NULL;
    g_test_case_env_restore_count = 0u;
}

static void test_json_write_escaped(FILE *f, const char *text)
{
    const unsigned char *p = (const unsigned char *)(text ? text : "");

    fputc('"', f);
    while (*p) {
        switch (*p) {
            case '\\': fputs("\\\\", f); break;
            case '"':  fputs("\\\"", f); break;
            case '\b': fputs("\\b", f); break;
            case '\f': fputs("\\f", f); break;
            case '\n': fputs("\\n", f); break;
            case '\r': fputs("\\r", f); break;
            case '\t': fputs("\\t", f); break;
            default:
                if (*p < 0x20)
                    fprintf(f, "\\u%04x", (unsigned int)*p);
                else
                    fputc((int)*p, f);
                break;
        }
        ++p;
    }
    fputc('"', f);
}

static void test_xml_write_escaped(FILE *f, const char *text)
{
    const unsigned char *p = (const unsigned char *)(text ? text : "");

    while (*p) {
        switch (*p) {
            case '&': fputs("&amp;", f); break;
            case '<': fputs("&lt;", f); break;
            case '>': fputs("&gt;", f); break;
            case '"': fputs("&quot;", f); break;
            case '\'': fputs("&apos;", f); break;
            default:
                if (*p < 0x20 && *p != '\n' && *p != '\r' && *p != '\t')
                    fprintf(f, "&#x%02X;", (unsigned int)*p);
                else
                    fputc((int)*p, f);
                break;
        }
        ++p;
    }
}

static int test_is_space_char(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static int test_csv_has_token(const char *csv, const char *token, size_t token_len)
{
    const char *p;

    if (!csv || !*csv || !token || token_len == 0u)
        return 0;

    p = csv;
    while (*p) {
        const char *start = p;
        const char *end;

        while (*start && (test_is_space_char(*start) || *start == ','))
            ++start;
        end = start;
        while (*end && *end != ',')
            ++end;

        while (end > start && test_is_space_char(end[-1]))
            --end;

        if ((size_t)(end - start) == token_len &&
            strncmp(start, token, token_len) == 0)
            return 1;

        p = end;
        if (*p == ',')
            ++p;
    }

    return 0;
}

static int test_has_any_excluded_tag(const char *test_tags, const char *excluded_tags)
{
    const char *p;

    if (!excluded_tags || !*excluded_tags)
        return 0;

    p = excluded_tags;
    while (*p) {
        const char *start = p;
        const char *end;

        while (*start && (test_is_space_char(*start) || *start == ','))
            ++start;
        end = start;
        while (*end && *end != ',')
            ++end;
        while (end > start && test_is_space_char(end[-1]))
            --end;

        if ((size_t)(end - start) > 0u &&
            test_csv_has_token(test_tags, start, (size_t)(end - start)))
            return 1;

        p = end;
        if (*p == ',')
            ++p;
    }

    return 0;
}

static int test_has_all_required_tags(const char *test_tags, const char *required_tags)
{
    const char *p;

    if (!required_tags || !*required_tags)
        return 1;

    p = required_tags;
    while (*p) {
        const char *start = p;
        const char *end;

        while (*start && (test_is_space_char(*start) || *start == ','))
            ++start;
        end = start;
        while (*end && *end != ',')
            ++end;
        while (end > start && test_is_space_char(end[-1]))
            --end;

        if ((size_t)(end - start) > 0u &&
            !test_csv_has_token(test_tags, start, (size_t)(end - start)))
            return 0;

        p = end;
        if (*p == ',')
            ++p;
    }

    return 1;
}

static int test_matches_filter(const char *name,
                               const char *parent,
                               const char *path,
                               const char *tags)
{
    const char *filter = test_filter_text();
    const char *required_tags = test_include_tags_text();
    const char *excluded_tags = test_exclude_tags_text();

    if (filter) {
        if (!( (name && strstr(name, filter)) ||
               (parent && strstr(parent, filter)) ||
               (path && strstr(path, filter)) ||
               (tags && strstr(tags, filter)) ))
            return 0;
    }

    if (!test_has_all_required_tags(tags, required_tags))
        return 0;
    if (test_has_any_excluded_tag(tags, excluded_tags))
        return 0;

    return 1;
}

static void test_print_test_name(const char *name, const char *parent)
{
    if (parent)
        printf("%s::%s", parent, name);
    else
        printf("%s", name);
}

static void test_print_tags(const char *tags)
{
    if (!tags || !*tags)
        return;
    printf(" " TEST_COLOR_DIM "{%s}" TEST_COLOR_RESET, tags);
}

static const char *test_outcome_name(test_outcome_t outcome)
{
    switch (outcome) {
        case TEST_OUTCOME_PASS: return "pass";
        case TEST_OUTCOME_FAIL: return "fail";
        case TEST_OUTCOME_SKIP: return "skip";
        case TEST_OUTCOME_LISTED: return "listed";
        case TEST_OUTCOME_FILTERED: return "filtered";
        default: return "unknown";
    }
}

static void test_debug_case(const char *reason,
                            const char *path,
                            const char *tags,
                            int enabled)
{
    if (!test_debug_enabled())
        return;

    fprintf(stderr,
            "test_harness: %s path='%s' tags='%s' enabled=%s filter='%s' include_tags='%s' exclude_tags='%s'\n",
            reason ? reason : "event",
            path ? path : "",
            tags ? tags : "",
            enabled ? "true" : "false",
            test_filter_text() ? test_filter_text() : "",
            test_include_tags_text() ? test_include_tags_text() : "",
            test_exclude_tags_text() ? test_exclude_tags_text() : "");
}

static void test_format_value_fallback(const void *value,
                                       char *buf,
                                       size_t buf_size)
{
    if (!buf || buf_size == 0u)
        return;

    if (!value) {
        snprintf(buf, buf_size, "<null>");
        return;
    }

    snprintf(buf, buf_size, "%p", value);
}

static int test_int_equal(const void *actual, const void *expected, void *ctx)
{
    (void)ctx;
    return *(const int *)actual == *(const int *)expected;
}

static void test_int_format(const void *value, char *buf, size_t buf_size, void *ctx)
{
    (void)ctx;
    snprintf(buf, buf_size, "%d", *(const int *)value);
}

static int test_long_equal(const void *actual, const void *expected, void *ctx)
{
    (void)ctx;
    return *(const long *)actual == *(const long *)expected;
}

static void test_long_format(const void *value, char *buf, size_t buf_size, void *ctx)
{
    (void)ctx;
    snprintf(buf, buf_size, "%ld", *(const long *)value);
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
    return strcmp(*actual_text, *expected_text) == 0;
}

static void test_cstr_format(const void *value, char *buf, size_t buf_size, void *ctx)
{
    const char *const *text = (const char *const *)value;
    (void)ctx;

    snprintf(buf, buf_size, "\"%s\"", *text ? *text : "<null>");
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

const char *test_case_temp_dir(void)
{
    char template_buf[] = "/tmp/mars-test-XXXXXX";
    char *created;
    char *owned;

    if (g_test_case_temp_dir)
        return g_test_case_temp_dir;

    created = mkdtemp(template_buf);
    if (!created)
        return NULL;

    owned = test_strdup_owned(created);
    if (!owned) {
        (void)rmdir(created);
        return NULL;
    }

    if (!test_case_register_cleanup(test_case_cleanup_temp_tree, owned)) {
        (void)rmdir(created);
        free(owned);
        return NULL;
    }

    g_test_case_temp_dir = owned;
    return g_test_case_temp_dir;
}

const char *test_case_temp_path(const char *leafname)
{
    const char *dir = test_case_temp_dir();
    const char *leaf = (leafname && *leafname) ? leafname : "tmp";
    size_t len;
    char *path;

    if (!dir)
        return NULL;

    len = strlen(dir) + 1u + strlen(leaf) + 1u;
    path = malloc(len);
    if (!path)
        return NULL;

    snprintf(path, len, "%s/%s", dir, leaf);
    if (!test_case_register_cleanup(test_case_cleanup_string, path)) {
        free(path);
        return NULL;
    }

    return path;
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

bool test_case_setenv(const char *name, const char *value)
{
    size_t i;
    const char *old_value;
    test_env_restore_t *restore;

    if (!name || !*name || !value)
        return false;

    for (i = 0; i < g_test_case_env_restore_count; ++i) {
        if (strcmp(g_test_case_env_restores[i].name, name) == 0)
            return setenv(name, value, 1) == 0;
    }

    if (g_test_case_env_restore_count == g_test_case_env_restore_cap) {
        size_t new_cap = g_test_case_env_restore_cap ? g_test_case_env_restore_cap * 2u : 8u;
        test_env_restore_t *grown =
            realloc(g_test_case_env_restores, new_cap * sizeof(*grown));

        if (!grown)
            return false;

        g_test_case_env_restores = grown;
        g_test_case_env_restore_cap = new_cap;
    }

    restore = &g_test_case_env_restores[g_test_case_env_restore_count];
    memset(restore, 0, sizeof(*restore));
    old_value = getenv(name);
    restore->name = test_strdup_owned(name);
    restore->old_value = old_value ? test_strdup_owned(old_value) : NULL;
    restore->had_old_value = old_value != NULL;

    if (!restore->name || (old_value && !restore->old_value)) {
        free(restore->name);
        free(restore->old_value);
        memset(restore, 0, sizeof(*restore));
        return false;
    }

    if (!test_case_register_cleanup(test_case_cleanup_env_restore, restore)) {
        free(restore->name);
        free(restore->old_value);
        memset(restore, 0, sizeof(*restore));
        return false;
    }

    g_test_case_env_restore_count++;
    return setenv(name, value, 1) == 0;
}

bool test_case_unsetenv(const char *name)
{
    size_t i;
    const char *old_value;
    test_env_restore_t *restore;

    if (!name || !*name)
        return false;

    for (i = 0; i < g_test_case_env_restore_count; ++i) {
        if (strcmp(g_test_case_env_restores[i].name, name) == 0)
            return unsetenv(name) == 0;
    }

    if (g_test_case_env_restore_count == g_test_case_env_restore_cap) {
        size_t new_cap = g_test_case_env_restore_cap ? g_test_case_env_restore_cap * 2u : 8u;
        test_env_restore_t *grown =
            realloc(g_test_case_env_restores, new_cap * sizeof(*grown));

        if (!grown)
            return false;

        g_test_case_env_restores = grown;
        g_test_case_env_restore_cap = new_cap;
    }

    restore = &g_test_case_env_restores[g_test_case_env_restore_count];
    memset(restore, 0, sizeof(*restore));
    old_value = getenv(name);
    restore->name = test_strdup_owned(name);
    restore->old_value = old_value ? test_strdup_owned(old_value) : NULL;
    restore->had_old_value = old_value != NULL;

    if (!restore->name || (old_value && !restore->old_value)) {
        free(restore->name);
        free(restore->old_value);
        memset(restore, 0, sizeof(*restore));
        return false;
    }

    if (!test_case_register_cleanup(test_case_cleanup_env_restore, restore)) {
        free(restore->name);
        free(restore->old_value);
        memset(restore, 0, sizeof(*restore));
        return false;
    }

    g_test_case_env_restore_count++;
    return unsetenv(name) == 0;
}

static int test_missing_config_path_seen(const char *full_key)
{
    size_t i;

    for (i = 0; i < g_test_missing_config_path_count; ++i) {
        if (strcmp(g_test_missing_config_paths[i], full_key) == 0)
            return 1;
    }

    return 0;
}

static void test_note_missing_config_path(const char *file, const char *path)
{
    char full_key[1536];
    char *copy;

    if (test_suite_mode == TEST_CONFIG_NONE || !file || !path)
        return;

    snprintf(full_key, sizeof(full_key), "%s::%s", file, path);
    if (test_missing_config_path_seen(full_key))
        return;

    if (g_test_missing_config_path_count == g_test_missing_config_path_cap) {
        size_t new_cap = g_test_missing_config_path_cap ? g_test_missing_config_path_cap * 2u : 16u;
        char **grown = realloc(g_test_missing_config_paths, new_cap * sizeof(*grown));

        if (!grown)
            return;

        g_test_missing_config_paths = grown;
        g_test_missing_config_path_cap = new_cap;
    }

    copy = test_strdup_owned(full_key);
    if (!copy)
        return;

    g_test_missing_config_paths[g_test_missing_config_path_count++] = copy;
    g_test_missing_config_count++;
}

static void test_record_case(const char *file,
                             int declaration_line,
                             const char *name,
                             const char *parent,
                             const char *path,
                             const char *tags,
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
    next->file = test_strdup_owned(file);
    next->name = test_strdup_owned(name);
    next->parent = test_strdup_owned(parent);
    next->path = test_strdup_owned(path);
    next->tags = test_strdup_owned(tags);
    next->failure_file = (outcome == TEST_OUTCOME_FAIL)
        ? test_strdup_owned(g_test_last_fail_file) : NULL;
    next->failure_detail = (outcome == TEST_OUTCOME_FAIL)
        ? test_strdup_owned(g_test_last_fail_detail) : NULL;
    next->outcome = outcome;
    next->enabled = enabled;
    next->is_group = is_group;
    next->is_output = is_output;
    next->declaration_line = declaration_line;
    next->failure_line = (outcome == TEST_OUTCOME_FAIL) ? g_test_last_fail_line : 0;
    next->group_passed = group_passed;
    next->group_failed = group_failed;
    next->group_skipped = group_skipped;
    next->ms = ms;
}

static void test_destroy_records(void)
{
    size_t i;

    for (i = 0; i < g_test_record_count; ++i) {
        free(g_test_records[i].file);
        free(g_test_records[i].name);
        free(g_test_records[i].parent);
        free(g_test_records[i].path);
        free(g_test_records[i].tags);
        free(g_test_records[i].failure_file);
        free(g_test_records[i].failure_detail);
    }

    free(g_test_records);
    g_test_records = NULL;
    g_test_record_count = 0u;
    g_test_record_cap = 0u;

    if (g_test_missing_config_paths) {
        size_t i;

        for (i = 0; i < g_test_missing_config_path_count; ++i)
            free(g_test_missing_config_paths[i]);
        free(g_test_missing_config_paths);
    }
    g_test_missing_config_paths = NULL;
    g_test_missing_config_path_count = 0u;
    g_test_missing_config_path_cap = 0u;

    if (g_test_validity_registry) {
        size_t i;

        for (i = 0; i < g_test_validity_registry_count; ++i)
            free(g_test_validity_registry[i].name);
        free(g_test_validity_registry);
    }
    g_test_validity_registry = NULL;
    g_test_validity_registry_count = 0u;
    g_test_validity_registry_cap = 0u;

    free(g_test_case_cleanups);
    g_test_case_cleanups = NULL;
    g_test_case_cleanup_count = 0u;
    g_test_case_cleanup_cap = 0u;

    free(g_test_case_env_restores);
    g_test_case_env_restores = NULL;
    g_test_case_env_restore_count = 0u;
    g_test_case_env_restore_cap = 0u;
    g_test_case_temp_dir = NULL;
}

void test_set_failure_detailf(const char *fmt, ...)
{
    va_list ap;

    if (!fmt) {
        g_test_last_fail_detail_buf[0] = '\0';
        g_test_last_fail_detail = g_test_last_fail_detail_buf;
        return;
    }

    va_start(ap, fmt);
    vsnprintf(g_test_last_fail_detail_buf,
              sizeof(g_test_last_fail_detail_buf),
              fmt,
              ap);
    va_end(ap);

    g_test_last_fail_detail = g_test_last_fail_detail_buf;
}

void test_clear_failure_detail(void)
{
    g_test_last_fail_detail_buf[0] = '\0';
    g_test_last_fail_detail = g_test_last_fail_detail_buf;
    g_test_last_fail_file = NULL;
    g_test_last_fail_line = 0;
    g_test_last_skip_detail_buf[0] = '\0';
    g_test_last_skip_detail = g_test_last_skip_detail_buf;
    g_test_last_skip_file = NULL;
    g_test_last_skip_line = 0;
    g_test_skip_requested = 0;
}

void test_mark_failure(const char *file, int line, const char *detail)
{
    g_test_failure_count++;
    g_test_last_fail_file = file;
    g_test_last_fail_line = line;
    test_set_failure_detailf("%s", detail ? detail : "test failed");
}

void test_mark_skip(const char *file, int line, const char *detail)
{
    g_test_skip_requested = 1;
    g_test_last_skip_file = file;
    g_test_last_skip_line = line;
    snprintf(g_test_last_skip_detail_buf,
             sizeof(g_test_last_skip_detail_buf),
             "%s",
             detail ? detail : "test skipped");
    g_test_last_skip_detail = g_test_last_skip_detail_buf;
}

bool test_assert_true(bool expr,
                      const char *file,
                      int line,
                      const char *detail)
{
    if (expr)
        return true;

    printf(TEST_COLOR_RED "    Assertion failed at %s:%d: %s\n" TEST_COLOR_RESET,
           file, line, detail ? detail : "expected true");
    test_set_failure_detailf("%s", detail ? detail : "expected true");
    g_test_failure_count++;
    g_test_last_fail_file = file;
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

    printf(TEST_COLOR_RED "    Assertion failed at %s:%d: %s\n" TEST_COLOR_RESET,
           file, line, detail ? detail : "expected false");
    test_set_failure_detailf("%s", detail ? detail : "expected false");
    g_test_failure_count++;
    g_test_last_fail_file = file;
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

    printf(TEST_COLOR_RED "    Assertion failed at %s:%d: expected %d, got %d\n"
           TEST_COLOR_RESET,
           file, line, expected, actual);
    test_set_failure_detailf("expected %d, got %d", expected, actual);
    g_test_failure_count++;
    g_test_last_fail_file = file;
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

    printf(TEST_COLOR_RED "    Assertion failed at %s:%d: expected %ld, got %ld\n"
           TEST_COLOR_RESET,
           file, line, expected, actual);
    test_set_failure_detailf("expected %ld, got %ld", expected, actual);
    g_test_failure_count++;
    g_test_last_fail_file = file;
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

    printf(TEST_COLOR_RED
           "    Assertion failed at %s:%d: expected %.12f, got %.12f\n"
           TEST_COLOR_RESET,
           file, line, expected, actual);
    test_set_failure_detailf("expected %.12f, got %.12f", expected, actual);
    g_test_failure_count++;
    g_test_last_fail_file = file;
    g_test_last_fail_line = line;
    return false;
}

bool test_assert_not_null(const void *ptr,
                          const char *file,
                          int line)
{
    if (ptr)
        return true;

    printf(TEST_COLOR_RED
           "    Assertion failed at %s:%d: expected non-null pointer\n"
           TEST_COLOR_RESET,
           file, line);
    test_set_failure_detailf("expected non-null pointer");
    g_test_failure_count++;
    g_test_last_fail_file = file;
    g_test_last_fail_line = line;
    return false;
}

bool test_assert_null(const void *ptr,
                      const char *file,
                      int line)
{
    if (!ptr)
        return true;

    printf(TEST_COLOR_RED
           "    Assertion failed at %s:%d: expected NULL pointer\n"
           TEST_COLOR_RESET,
           file, line);
    test_set_failure_detailf("expected NULL pointer");
    g_test_failure_count++;
    g_test_last_fail_file = file;
    g_test_last_fail_line = line;
    return false;
}

bool test_assert_validity(const test_validity_contract_t *contract,
                          const void *actual,
                          const void *expected,
                          const char *file,
                          int line)
{
    char actual_buf[256];
    char expected_buf[256];
    const char *contract_name;
    int ok;

    if (!contract || !contract->equal) {
        printf(TEST_COLOR_RED
               "    Assertion failed at %s:%d: invalid validity contract\n"
               TEST_COLOR_RESET,
               file, line);
        test_set_failure_detailf("invalid validity contract");
        g_test_failure_count++;
        g_test_last_fail_file = file;
        g_test_last_fail_line = line;
        return false;
    }

    ok = contract->equal(actual, expected, contract->ctx);
    if (ok)
        return true;

    if (contract->format) {
        contract->format(expected, expected_buf, sizeof(expected_buf), contract->ctx);
        contract->format(actual, actual_buf, sizeof(actual_buf), contract->ctx);
    } else {
        test_format_value_fallback(expected, expected_buf, sizeof(expected_buf));
        test_format_value_fallback(actual, actual_buf, sizeof(actual_buf));
    }

    contract_name = (contract->name && *contract->name)
        ? contract->name
        : "validity";

    printf(TEST_COLOR_RED
           "    Assertion failed at %s:%d [%s]: expected %s, got %s\n"
           TEST_COLOR_RESET,
           file, line, contract_name, expected_buf, actual_buf);
    test_set_failure_detailf("[%s] expected %s, got %s",
                             contract_name,
                             expected_buf,
                             actual_buf);
    g_test_failure_count++;
    g_test_last_fail_file = file;
    g_test_last_fail_line = line;
    return false;
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
    if (!g_test_current_path || !*g_test_current_path) {
        printf(TEST_COLOR_RED
               "    Harness misuse at %s:%d: test_run_subtest(%s) requires an active parent test/group\n"
               TEST_COLOR_RESET,
               file,
               line,
               name ? name : "(unnamed)");
        test_mark_failure(file, line, "test_run_subtest requires an active parent");
        return;
    }

    test_run_in_group(file, line, name, g_test_current_path, tags, fn);
}

void test_run_in_group(const char *file,
                       int line,
                       const char *name,
                       const char *parent,
                       const char *tags,
                       test_fn fn)
{
    int enabled;
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
    const char *prev_path;
    char full_path[1024];
    int had_config_key = 1;
    int fixture_ran = 0;
    int fixture_continue = 1;

    if (g_test_abort_requested)
        return;

    if (parent && *parent)
        snprintf(full_path, sizeof(full_path), "%s.%s", parent, name);
    else
        snprintf(full_path, sizeof(full_path), "%s", name);

    if (!test_matches_filter(name, parent, full_path, tags)) {
        g_test_filtered_count++;
        test_debug_case("filtered", full_path, tags, 1);
        test_record_case(file, line, name, parent, full_path, tags,
                         TEST_OUTCOME_FILTERED, 1, 0, 0, 0, 0, 0, 0.0);
        return;
    }

    had_config_key = test_config_has_key(file, name, parent) ? 1 : 0;
    enabled = test_enabled(file, name, parent);
    test_debug_case("selected", full_path, tags, enabled);
    if (!had_config_key)
        test_note_missing_config_path(file, full_path);

    if (test_list_only_enabled()) {
        g_test_listed_count++;
        printf(TEST_COLOR_CYAN "LIST: " TEST_COLOR_RESET);
        test_print_test_name(name, parent);
        test_print_tags(tags);
        printf(" " TEST_COLOR_GREY "[" TEST_COLOR_RESET "%s" TEST_COLOR_GREY "]"
               TEST_COLOR_RESET "\n",
               enabled ? "enabled" : "disabled");
        test_record_case(file, line, name, parent, full_path, tags,
                         TEST_OUTCOME_LISTED, enabled, 0, 0, 0, 0, 0, 0.0);
        return;
    }

    g_test_nested_run_count++;

    if (!enabled) {
        if (parent)
            printf(TEST_COLOR_YELLOW "SKIP: %s (in %s)" TEST_COLOR_RESET "\n",
                   name, parent);
        else
            printf(TEST_COLOR_YELLOW "SKIP: %s" TEST_COLOR_RESET "\n", name);
        g_test_skip_count++;
        test_record_case(file, line, name, parent, full_path, tags,
                         TEST_OUTCOME_SKIP, 0, 0, 0, 0, 0, 0, 0.0);
        return;
    }

    failure_count_before = g_test_failure_count;
    skip_count_before = g_test_skip_count;
    passed_cases_before = g_test_passed_cases;
    failed_cases_before = g_test_failed_cases;
    nested_runs_before = g_test_nested_run_count;
    total_ms_before = g_test_total_ms;
    prev_path = g_test_current_path;

    test_clear_failure_detail();
    g_test_current_path = full_path;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    if (test_fixture_setup_hook) {
        fixture_ran = 1;
        fixture_continue = test_run_case_fixture_setup(file, line);
    }
    if (fixture_continue &&
        g_test_failure_count == failure_count_before &&
        !g_test_skip_requested) {
        fn();
    }
    if (fixture_ran && fixture_continue)
        test_run_case_fixture_teardown(file, line);
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

        printf(TEST_COLOR_CYAN "GROUP: %s " TEST_COLOR_RESET
               "(" TEST_COLOR_GREEN "%d passed" TEST_COLOR_RESET
               "," TEST_COLOR_RED " %d failed" TEST_COLOR_RESET
               "," TEST_COLOR_YELLOW " %d skipped" TEST_COLOR_RESET ")",
               name, passed, failed, skipped);
        test_print_tags(tags);
        test_record_case(file, line, name, parent, full_path, tags,
                         outcome, 1, 1, 0, passed, failed, skipped, disp_ms);
        if (failed > 0 && test_fail_fast_enabled())
            g_test_abort_requested = 1;
    } else if (g_test_skip_requested) {
        g_test_skip_count++;
        printf(TEST_COLOR_YELLOW "SKIP: %s " TEST_COLOR_YELLOW "(%s:%d)" TEST_COLOR_RESET,
               name,
               g_test_last_skip_file ? g_test_last_skip_file : file,
               g_test_last_skip_line);
        if (g_test_last_skip_detail && *g_test_last_skip_detail)
            printf(" " TEST_COLOR_GREY "[%s]" TEST_COLOR_RESET, g_test_last_skip_detail);
        test_print_tags(tags);
        test_record_case(file, line, name, parent, full_path, tags,
                         TEST_OUTCOME_SKIP, 1, 0, 0, 0, 0, 0, disp_ms);
    } else if (g_test_failure_count == failure_count_before) {
        g_test_passed_cases++;
        printf(TEST_COLOR_BOLD TEST_COLOR_GREEN "PASS: "
               TEST_COLOR_RESET "%s", name);
        if (test_show_pass_location_enabled())
            printf(" " TEST_COLOR_GREY "(%s:%d)" TEST_COLOR_RESET, file, line);
        test_print_tags(tags);
        test_record_case(file, line, name, parent, full_path, tags,
                         TEST_OUTCOME_PASS, 1, 0, 0, 0, 0, 0, disp_ms);
    } else {
        g_test_failed_cases++;
        printf(TEST_COLOR_BOLD TEST_COLOR_RED "FAIL: " TEST_COLOR_RESET
               "%s " TEST_COLOR_RED "(%s:%d)" TEST_COLOR_RESET,
               name,
               g_test_last_fail_file ? g_test_last_fail_file : file,
               g_test_last_fail_file ? g_test_last_fail_line : 0);
        test_print_tags(tags);
        test_record_case(file, line, name, parent, full_path, tags,
                         TEST_OUTCOME_FAIL, 1, 0, 0, 0, 0, 0, disp_ms);
        if (test_fail_fast_enabled())
            g_test_abort_requested = 1;
    }

    test_print_time(disp_ms);
    putchar('\n');
}

void test_run_output_case(const char *file,
                          int line,
                          const char *tags,
                          const char *name,
                          test_fn fn)
{
    int failure_count_before;
    int enabled;
    int had_config_key = 1;
    int fixture_ran = 0;
    int fixture_continue = 1;
    double ms;
    struct timespec t0;
    struct timespec t1;
    char full_path[1024];

    if (g_test_abort_requested)
        return;

    if (!name)
        name = "(unnamed-output)";

    snprintf(full_path, sizeof(full_path), "%s", name);

    if (!test_matches_filter(name, NULL, full_path, tags)) {
        g_test_filtered_count++;
        test_debug_case("filtered", full_path, tags, 1);
        test_record_case(file, line, name, NULL, full_path, tags,
                         TEST_OUTCOME_FILTERED, 1, 0, 1, 0, 0, 0, 0.0);
        return;
    }

    had_config_key = test_config_has_key(file, name, NULL) ? 1 : 0;
    enabled = test_enabled(file, name, NULL);
    test_debug_case("selected", full_path, tags, enabled);
    if (!had_config_key)
        test_note_missing_config_path(file, full_path);

    if (test_list_only_enabled()) {
        g_test_listed_count++;
        printf(TEST_COLOR_CYAN "LIST OUTPUT: " TEST_COLOR_RESET "%s", name);
        test_print_tags(tags);
        printf(" " TEST_COLOR_GREY "[" TEST_COLOR_RESET "%s" TEST_COLOR_GREY "]"
               TEST_COLOR_RESET "\n",
               enabled ? "enabled" : "disabled");
        test_record_case(file, line, name, NULL, full_path, tags,
                         TEST_OUTCOME_LISTED, enabled, 0, 1, 0, 0, 0, 0.0);
        return;
    }

    g_test_output_count++;

    if (!enabled) {
        g_test_output_skip_count++;
        printf(TEST_COLOR_YELLOW "OUTPUT SKIP: %s" TEST_COLOR_RESET, name);
        test_print_tags(tags);
        test_record_case(file, line, name, NULL, full_path, tags,
                         TEST_OUTCOME_SKIP, 0, 0, 1, 0, 0, 0, 0.0);
        putchar('\n');
        return;
    }

    failure_count_before = g_test_failure_count;
    test_clear_failure_detail();

    clock_gettime(CLOCK_MONOTONIC, &t0);
    if (test_fixture_setup_hook) {
        fixture_ran = 1;
        fixture_continue = test_run_case_fixture_setup(file, line);
    }
    if (fixture_continue &&
        g_test_failure_count == failure_count_before &&
        !g_test_skip_requested) {
        fn();
    }
    if (fixture_ran && fixture_continue)
        test_run_case_fixture_teardown(file, line);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    test_case_cleanup_all();
    ms = test_elapsed_ms(t0, t1);

    if (g_test_failure_count == failure_count_before) {
        g_test_output_pass_count++;
        printf(TEST_COLOR_BOLD TEST_COLOR_CYAN "OUTPUT: "
               TEST_COLOR_RESET "%s", name);
        if (test_show_pass_location_enabled())
            printf(" " TEST_COLOR_GREY "(%s:%d)" TEST_COLOR_RESET, file, line);
        test_print_tags(tags);
        test_record_case(file, line, name, NULL, full_path, tags,
                         TEST_OUTCOME_PASS, 1, 0, 1, 0, 0, 0, ms);
    } else {
        g_test_output_failure_count++;
        printf(TEST_COLOR_BOLD TEST_COLOR_RED "OUTPUT FAIL: " TEST_COLOR_RESET
               "%s " TEST_COLOR_RED "(%s:%d)" TEST_COLOR_RESET,
               name,
               g_test_last_fail_file ? g_test_last_fail_file : file,
               g_test_last_fail_file ? g_test_last_fail_line : 0);
        test_print_tags(tags);
        test_record_case(file, line, name, NULL, full_path, tags,
                         TEST_OUTCOME_FAIL, 1, 0, 1, 0, 0, 0, ms);
        if (test_fail_fast_enabled())
            g_test_abort_requested = 1;
    }

    test_print_time(ms);
    putchar('\n');
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
        printf(TEST_COLOR_GREEN "validity contract ok - ready\n" TEST_COLOR_RESET);
    } else {
        printf(TEST_COLOR_BOLD TEST_COLOR_RED "SETUP FAIL: " TEST_COLOR_RESET
               TEST_COLOR_RED "(%s:%d)" TEST_COLOR_RESET,
               g_test_last_fail_file ? g_test_last_fail_file : __FILE__,
               g_test_last_fail_line);
        if (g_test_last_fail_detail && *g_test_last_fail_detail)
            printf(" " TEST_COLOR_GREY "[%s]" TEST_COLOR_RESET, g_test_last_fail_detail);
        putchar('\n');
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
    int limit = test_slowest_count();
    int printed = 0;
    unsigned char *used;

    if (limit <= 0 || g_test_record_count == 0u)
        return;

    used = calloc(g_test_record_count, sizeof(*used));
    if (!used)
        return;

    printf(TEST_COLOR_CYAN "SLOWEST: " TEST_COLOR_RESET "\n");

    while (printed < limit) {
        size_t best_idx = (size_t)-1;
        size_t j;

        for (j = 0; j < g_test_record_count; ++j) {
            if (g_test_records[j].outcome == TEST_OUTCOME_FILTERED ||
                g_test_records[j].outcome == TEST_OUTCOME_LISTED ||
                g_test_records[j].outcome == TEST_OUTCOME_SKIP ||
                used[j])
                continue;

            if (best_idx == (size_t)-1 ||
                g_test_records[j].ms > g_test_records[best_idx].ms)
                best_idx = j;
        }

        if (best_idx == (size_t)-1)
            break;

        printf("  %d. %s", printed + 1, g_test_records[best_idx].path);
        test_print_tags(g_test_records[best_idx].tags);
        printf(" ");
        test_print_time(g_test_records[best_idx].ms);
        printf(" " TEST_COLOR_GREY "[%s]" TEST_COLOR_RESET "\n",
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

    fprintf(stderr,
            TEST_COLOR_YELLOW
            "test_harness: discovered %d test/group path%s without config keys; regenerated them as enabled.\n"
            TEST_COLOR_RESET,
            g_test_missing_config_count,
            g_test_missing_config_count == 1 ? "" : "s");
    for (i = 0; i < g_test_missing_config_path_count; ++i)
        fprintf(stderr, TEST_COLOR_YELLOW "  added: %s\n" TEST_COLOR_RESET,
                g_test_missing_config_paths[i]);
}

static void test_write_json_report(int exit_code)
{
    const char *path = test_report_json_path();
    FILE *f;
    size_t i;

    if (!path)
        return;

    f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "test_harness: failed to open JSON report path '%s'\n", path);
        return;
    }

    fputs("{\n", f);
    fputs("  \"summary\": {\n", f);
    fprintf(f, "    \"exit_code\": %d,\n", exit_code);
    fprintf(f, "    \"run\": %d,\n", g_test_run_count);
    fprintf(f, "    \"groups\": %d,\n", g_test_group_count);
    fprintf(f, "    \"passed\": %d,\n", g_test_passed_cases);
    fprintf(f, "    \"failed\": %d,\n", g_test_failed_cases);
    fprintf(f, "    \"skipped\": %d,\n", g_test_skip_count);
    fprintf(f, "    \"listed\": %d,\n", g_test_listed_count);
    fprintf(f, "    \"filtered\": %d,\n", g_test_filtered_count);
    fprintf(f, "    \"output_cases\": %d,\n", g_test_output_count);
    fprintf(f, "    \"output_passed\": %d,\n", g_test_output_pass_count);
    fprintf(f, "    \"output_failures\": %d,\n", g_test_output_failure_count);
    fprintf(f, "    \"output_skipped\": %d,\n", g_test_output_skip_count);
    fprintf(f, "    \"missing_config_keys\": %d,\n", g_test_missing_config_count);
    fprintf(f, "    \"fail_fast\": %s,\n", g_test_abort_requested ? "true" : "false");
    fprintf(f, "    \"total_ms\": %.9f\n", g_test_total_ms);
    fputs("  },\n", f);
    fputs("  \"selection\": {\n", f);
    fputs("    \"filter\": ", f);
    test_json_write_escaped(f, test_filter_text());
    fputs(",\n    \"include_tags\": ", f);
    test_json_write_escaped(f, test_include_tags_text());
    fputs(",\n    \"exclude_tags\": ", f);
    test_json_write_escaped(f, test_exclude_tags_text());
    fputs(",\n    \"list_only\": ", f);
    fputs(test_list_only_enabled() ? "true" : "false", f);
    fputs("\n  },\n", f);
    fputs("  \"tests\": [\n", f);

    for (i = 0; i < g_test_record_count; ++i) {
        const test_record_t *rec = &g_test_records[i];

        fputs("    {\n", f);
        fputs("      \"file\": ", f);
        test_json_write_escaped(f, rec->file);
        fputs(",\n      \"func\": ", f);
        test_json_write_escaped(f, rec->name);
        fputs(",\n      \"parent\": ", f);
        test_json_write_escaped(f, rec->parent);
        fputs(",\n      \"path\": ", f);
        test_json_write_escaped(f, rec->path);
        fputs(",\n      \"tags\": ", f);
        test_json_write_escaped(f, rec->tags);
        fputs(",\n      \"outcome\": ", f);
        test_json_write_escaped(f, test_outcome_name(rec->outcome));
        fprintf(f, ",\n      \"enabled\": %s,\n", rec->enabled ? "true" : "false");
        fprintf(f, "      \"is_group\": %s,\n", rec->is_group ? "true" : "false");
        fprintf(f, "      \"is_output\": %s,\n", rec->is_output ? "true" : "false");
        fprintf(f, "      \"declaration_line\": %d,\n", rec->declaration_line);
        fputs("      \"failure_file\": ", f);
        test_json_write_escaped(f, rec->failure_file);
        fprintf(f, ",\n      \"failure_line\": %d,\n", rec->failure_line);
        fputs("      \"failure_detail\": ", f);
        test_json_write_escaped(f, rec->failure_detail);
        fputs(",\n", f);
        fprintf(f, "      \"group_passed\": %d,\n", rec->group_passed);
        fprintf(f, "      \"group_failed\": %d,\n", rec->group_failed);
        fprintf(f, "      \"group_skipped\": %d,\n", rec->group_skipped);
        fprintf(f, "      \"ms\": %.9f\n", rec->ms);
        fputs(i + 1u == g_test_record_count ? "    }\n" : "    },\n", f);
    }

    fputs("  ]\n", f);
    fputs("}\n", f);
    fclose(f);
}

static void test_write_junit_report(int exit_code)
{
    const char *path = test_report_junit_path();
    FILE *f;
    size_t i;

    if (!path)
        return;

    f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "test_harness: failed to open JUnit report path '%s'\n", path);
        return;
    }

    fputs("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n", f);
    fprintf(f,
            "<testsuite name=\"mars\" tests=\"%d\" failures=\"%d\" skipped=\"%d\" errors=\"0\" time=\"%.9f\">\n",
            g_test_run_count + g_test_output_count,
            g_test_failed_cases + g_test_output_failure_count,
            g_test_skip_count + g_test_output_skip_count,
            g_test_total_ms / 1000.0);

    for (i = 0; i < g_test_record_count; ++i) {
        const test_record_t *rec = &g_test_records[i];

        if (rec->outcome == TEST_OUTCOME_FILTERED || rec->outcome == TEST_OUTCOME_LISTED)
            continue;

        fputs("  <testcase classname=\"", f);
        test_xml_write_escaped(f, rec->file && *rec->file ? rec->file : "mars");
        fputs("\" name=\"", f);
        test_xml_write_escaped(f, rec->path && *rec->path ? rec->path : rec->name);
        fprintf(f, "\" time=\"%.9f\"", rec->ms / 1000.0);
        if (rec->tags && *rec->tags) {
            fputs(" assertions=\"0\">", f);
            fputs("\n    <properties>\n", f);
            fputs("      <property name=\"tags\" value=\"", f);
            test_xml_write_escaped(f, rec->tags);
            fputs("\"/>\n", f);
            if (rec->is_output)
                fputs("      <property name=\"kind\" value=\"output\"/>\n", f);
            else if (rec->is_group)
                fputs("      <property name=\"kind\" value=\"group\"/>\n", f);
            if (rec->parent && *rec->parent) {
                fputs("      <property name=\"parent\" value=\"", f);
                test_xml_write_escaped(f, rec->parent);
                fputs("\"/>\n", f);
            }
            fputs("    </properties>\n", f);
        } else {
            fputs(">", f);
            if ((rec->parent && *rec->parent) || rec->is_output || rec->is_group) {
                fputs("\n    <properties>\n", f);
                if (rec->is_output)
                    fputs("      <property name=\"kind\" value=\"output\"/>\n", f);
                else if (rec->is_group)
                    fputs("      <property name=\"kind\" value=\"group\"/>\n", f);
                fputs("      <property name=\"parent\" value=\"", f);
                test_xml_write_escaped(f, rec->parent ? rec->parent : "");
                fputs("\"/>\n", f);
                if (rec->is_group) {
                    fprintf(f, "      <property name=\"group_passed\" value=\"%d\"/>\n",
                            rec->group_passed);
                    fprintf(f, "      <property name=\"group_failed\" value=\"%d\"/>\n",
                            rec->group_failed);
                    fprintf(f, "      <property name=\"group_skipped\" value=\"%d\"/>\n",
                            rec->group_skipped);
                }
                fputs("    </properties>\n", f);
            }
        }

        if (rec->outcome == TEST_OUTCOME_SKIP) {
            fputs("    <skipped message=\"disabled by test_config\"/>\n", f);
        } else if (rec->outcome == TEST_OUTCOME_FAIL) {
            fputs("    <failure message=\"", f);
            test_xml_write_escaped(f,
                                   rec->failure_detail && *rec->failure_detail
                                       ? rec->failure_detail
                                       : "test failed");
            fputs("\" type=\"assertion\">", f);
            if (rec->failure_file && *rec->failure_file) {
                test_xml_write_escaped(f, rec->failure_file);
                fprintf(f, ":%d", rec->failure_line);
                if (rec->failure_detail && *rec->failure_detail) {
                    fputs(" ", f);
                    test_xml_write_escaped(f, rec->failure_detail);
                }
            } else if (rec->failure_detail && *rec->failure_detail) {
                test_xml_write_escaped(f, rec->failure_detail);
            }
            fputs("</failure>\n", f);
        }

        fputs("  </testcase>\n", f);
    }

    fputs("  <properties>\n", f);
    fprintf(f, "    <property name=\"exit_code\" value=\"%d\"/>\n", exit_code);
    fprintf(f, "    <property name=\"filter\" value=\"");
    test_xml_write_escaped(f, test_filter_text());
    fprintf(f, "\"/>\n");
    fprintf(f, "    <property name=\"include_tags\" value=\"");
    test_xml_write_escaped(f, test_include_tags_text());
    fprintf(f, "\"/>\n");
    fprintf(f, "    <property name=\"exclude_tags\" value=\"");
    test_xml_write_escaped(f, test_exclude_tags_text());
    fprintf(f, "\"/>\n");
    fprintf(f, "    <property name=\"list_only\" value=\"%s\"/>\n",
            test_list_only_enabled() ? "true" : "false");
    fputs("  </properties>\n", f);
    fputs("</testsuite>\n", f);
    fclose(f);
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
    prune_stale_config = !test_list_only_enabled() &&
                         !test_filter_text() &&
                         !test_include_tags_text() &&
                         !test_exclude_tags_text() &&
                         !g_test_abort_requested;
    test_config_set_prune_enabled(prune_stale_config);
    if (test_suite_mode != TEST_CONFIG_NONE)
        test_config_save();
    test_report_missing_config_keys();
    test_config_shutdown();
    exit_code = rc ? rc : test_exit_code();

    if (test_list_only_enabled()) {
        printf("\n" TEST_COLOR_CYAN "LIST: " TEST_COLOR_RESET "%d matched",
               g_test_listed_count);
        if (g_test_filtered_count)
            printf(", " TEST_COLOR_GREY "%d filtered out" TEST_COLOR_RESET,
                   g_test_filtered_count);
        putchar('\n');
        test_write_json_report(exit_code);
        test_write_junit_report(exit_code);
        test_destroy_records();
        return exit_code;
    }

    printf("\n" TEST_COLOR_CYAN "SUMMARY: " TEST_COLOR_RESET
           "%d run, " TEST_COLOR_GREEN "%d passed" TEST_COLOR_RESET ", "
           TEST_COLOR_RED "%d failed" TEST_COLOR_RESET ", "
           TEST_COLOR_YELLOW "%d skipped" TEST_COLOR_RESET,
           g_test_run_count,
           g_test_passed_cases,
           g_test_failed_cases,
           g_test_skip_count);
    if (g_test_group_count)
        printf(", " TEST_COLOR_CYAN "%d group%s" TEST_COLOR_RESET,
               g_test_group_count,
               g_test_group_count == 1 ? "" : "s");
    if (g_test_filtered_count)
        printf(", " TEST_COLOR_GREY "%d filtered out" TEST_COLOR_RESET,
               g_test_filtered_count);
    if (g_test_output_count)
        printf(", " TEST_COLOR_CYAN "%d output example%s" TEST_COLOR_RESET,
               g_test_output_count,
               g_test_output_count == 1 ? "" : "s");
    if (g_test_output_count)
        printf(" (" TEST_COLOR_GREEN "%d passed" TEST_COLOR_RESET
               ", " TEST_COLOR_RED "%d failed" TEST_COLOR_RESET
               ", " TEST_COLOR_YELLOW "%d skipped" TEST_COLOR_RESET ")",
               g_test_output_pass_count,
               g_test_output_failure_count,
               g_test_output_skip_count);
    if (g_test_abort_requested)
        printf(", " TEST_COLOR_MAGENTA "fail-fast stop" TEST_COLOR_RESET);
    test_print_time(g_test_total_ms);
    putchar('\n');
    test_print_slowest_cases();
    test_write_json_report(exit_code);
    test_write_junit_report(exit_code);
    test_destroy_records();

    return exit_code;
}
