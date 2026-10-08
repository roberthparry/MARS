/**
 * @file test_checks_coverage.c
 * @brief Synthetic coverage-policy regressions for the native developer checker.
 *
 * Disposable repository trees contain small GCC-shaped JSON reports compressed
 * by a shell-free gzip invocation. Cases exercise exact policy boundaries,
 * source and function inventories, malformed records, duplicate identities and
 * counter limits. The parent test executable registers these ordinary cases
 * before README examples; no real coverage files or source trees are modified.
 */
#include <stddef.h>

#include "test_harness.h"
#include "checks_process.h"
#include "checks_coverage.h"
#include "test_checks.h"

static bool test_checks_coverage_tree(const string_t *root)
{
    return test_checks_write(root, "src/file/file_one.c", "int file_one(void) { return 1; }\n") &&
           test_checks_write(root, "include/file.h", "int file_one(void);\nint file_one(void);\n");
}

static string_t *test_checks_coverage_json(unsigned covered, unsigned lines, unsigned taken, unsigned branches,
                                          const char *execution)
{
    string_t *text = string_sprintf("{\"files\":[{\"file\":\"src/file/file_one.c\",\"functions\":["
                                    "{\"name\":\"file_one\",\"execution_count\":%s}],\"lines\":[", execution);
    for (unsigned i = 0; i < lines; ++i) {
        string_t *line = string_sprintf("%s{\"line_number\":%u,\"count\":%u,\"branches\":[",
                                        i ? "," : "", i + 1, i < covered ? 1u : 0u);
        checks_append(text, line);
        string_free(line);
        for (unsigned b = 0; !i && b < branches; ++b) {
            string_t *branch = string_sprintf("%s{\"count\":%u}", b ? "," : "", b < taken ? 1u : 0u);
            checks_append(text, branch);
            string_free(branch);
        }
        string_append_cstr(text, "]}");
    }
    string_append_cstr(text, "]}]}");
    return text;
}

static bool test_checks_coverage_compress(const string_t *root, const char *relative, const string_t *json)
{
    bool ok = test_checks_write(root, relative, string_c_str(json));
    checks_strings_t *arguments = checks_strings_new();
    checks_strings_add(arguments, checks_text("gzip"));
    checks_strings_add(arguments, checks_text("-f"));
    checks_strings_add(arguments, checks_text("--"));
    checks_strings_add(arguments, checks_path(root, relative));
    string_t *output = NULL, *errors = NULL;
    int status = -1;
    ok = ok && checks_process_run(arguments, NULL, 10, &output, &errors, &status) && !status;
    checks_strings_free(arguments);
    string_free(output);
    string_free(errors);
    return ok;
}

static int test_checks_coverage_run(const string_t *root)
{
    /* Deliberate shell metacharacters must remain literal directory characters. */
    char directory[] = "reports ; $literal";
    char *arguments[] = {directory};
    return checks_coverage(root, 1, arguments);
}

static bool test_checks_coverage_case(string_t *text, int expected)
{
    string_t *root = test_checks_root();
    bool ok = test_checks_coverage_tree(root) &&
              test_checks_coverage_compress(root, "reports ; $literal/file_one.gcov.json", text);
    if (ok)
        ok = test_checks_coverage_run(root) == expected;
    string_free(text);
    return test_checks_finish(root) && ok;
}

static void test_checks_coverage_thresholds(void)
{
    bool ok = test_checks_coverage_case(test_checks_coverage_json(9, 10, 4, 5, "1"), 0);
    ok = test_checks_coverage_case(test_checks_coverage_json(8, 10, 4, 5, "1"), 1) && ok;
    ok = test_checks_coverage_case(test_checks_coverage_json(9, 10, 3, 5, "1"), 1) && ok;
    ok = test_checks_coverage_case(test_checks_coverage_json(8, 9, 4, 5, "1"), 1) && ok;
    ok = test_checks_coverage_case(test_checks_coverage_json(9, 10, 3, 4, "1"), 1) && ok;
    ok = test_checks_coverage_case(test_checks_coverage_json(1, 1, 1, 1, "18446744073709551615"), 0) && ok;
    TEST_ASSERT_TRUE(ok, "coverage accepts exact 90/80 boundaries and rejects sub-threshold integer ratios");
}

static void test_checks_coverage_zero_and_unexecuted(void)
{
    bool ok = test_checks_coverage_case(test_checks_coverage_json(1, 1, 1, 1, "0"), 1);
    ok = test_checks_coverage_case(test_checks_coverage_json(0, 0, 0, 0, "1"), 1) && ok;
    ok = test_checks_coverage_case(test_checks_coverage_json(1, 1, 0, 0, "1"), 1) && ok;
    string_t *json = test_checks_coverage_json(1, 1, 1, 1, "1");
    string_replace(json, "\"file_one\"", "\"private_helper\"");
    ok = test_checks_coverage_case(json, 1) && ok;
    TEST_ASSERT_TRUE(ok, "zero denominators and unexecuted or absent public functions fail coverage");
}

static void test_checks_coverage_counter_spelling(void)
{
    const char *const invalid[] = {"-1", "1.5", "1e2", "true", "null", "\"1\"", "{}", "[]",
                                  "{\"$mars.number\":\"1\"}",
                                  "18446744073709551616", "999999999999999999999999999999"};
    bool ok = true;
    for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i) {
        ok = test_checks_coverage_case(test_checks_coverage_json(1, 1, 1, 1, invalid[i]), 1) && ok;
        string_t *json = test_checks_coverage_json(1, 1, 1, 1, "1");
        string_t *replacement = string_sprintf("\"count\":%s", invalid[i]);
        string_replace(json, "\"count\":1", string_c_str(replacement));
        string_free(replacement);
        ok = test_checks_coverage_case(json, 1) && ok;
        json = test_checks_coverage_json(1, 1, 1, 1, "1");
        replacement = string_sprintf("\"branches\":[{\"count\":%s}]", invalid[i]);
        string_replace(json, "\"branches\":[{\"count\":1}]", string_c_str(replacement));
        string_free(replacement);
        ok = test_checks_coverage_case(json, 1) && ok;
    }
    string_t *json = test_checks_coverage_json(1, 1, 1, 1, "1");
    string_replace(json, "\"count\":1", "\"count\":18446744073709551615");
    ok = test_checks_coverage_case(json, 0) && ok;
    TEST_ASSERT_TRUE(ok, "all counter sites require exact unsigned integers and reject overflow without wrapping");
}

static void test_checks_coverage_malformed_and_duplicate_records(void)
{
    const char *const invalid[] = {
        "", "{", "[]", "{}", "{\"files\":{}}", "{\"files\":[null]}",
        "{\"files\":[{\"file\":4}]}", "{\"files\":[{\"file\":\"src/file/file_one.c\"}]}",
        "{\"files\":[],\"files\":[]}", "{\"files\":[],\"f\\u0069les\":[]}",
        "{\"files\":[]} trailing"
    };
    bool ok = true;
    for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i)
        ok = test_checks_coverage_case(checks_text(invalid[i]), 1) && ok;
    const char *const from[] = {
        "\"line_number\":2", "\"line_number\":1", "\"line_number\":1", "\"count\":1",
        "\"branches\":[{\"count\":1}]", "\"functions\":[{\"name\":\"file_one\",\"execution_count\":1}]",
        "\"name\":\"file_one\",\"execution_count\":1"
    };
    const char *const to[] = {
        "\"line_number\":1", "\"line_number\":0", "\"line_number\":18446744073709551616",
        "\"count\":0,\"count\":1", "\"branches\":{}", "\"functions\":{}",
        "\"name\":\"file_one\",\"execution_count\":1},{\"name\":\"file_one\",\"execution_count\":1"
    };
    for (size_t i = 0; i < sizeof(from) / sizeof(*from); ++i) {
        string_t *json = test_checks_coverage_json(2, 2, 1, 1, "1");
        string_replace(json, from[i], to[i]);
        ok = test_checks_coverage_case(json, 1) && ok;
    }
    string_t *single = test_checks_coverage_json(1, 1, 1, 1, "1");
    /* Extract the one complete file object and repeat it in the same report. */
    string_t *item = checks_slice(single, 10, string_byte_length(single) - 12);
    string_t *duplicate = string_sprintf("{\"files\":[%s,%s]}", string_c_str(item), string_c_str(item));
    string_free(item);
    string_free(single);
    ok = test_checks_coverage_case(duplicate, 1) && ok;
    string_t *nested = checks_text("");
    for (size_t i = 0; i < 65; ++i)
        string_append_char(nested, '[');
    for (size_t i = 0; i < 65; ++i)
        string_append_char(nested, ']');
    ok = test_checks_coverage_case(nested, 1) && ok;
    TEST_ASSERT_TRUE(ok, "malformed structure, duplicate JSON keys, duplicate lines and functions fail closed");
}

static void test_checks_coverage_inventory(void)
{
    string_t *root = test_checks_root();
    string_t *json = test_checks_coverage_json(1, 1, 1, 1, "1");
    bool ok = test_checks_coverage_tree(root) &&
              test_checks_coverage_compress(root, "reports ; $literal/file_z.gcov.json", json);
    ok = ok && test_checks_coverage_run(root) == 0;
    ok = test_checks_coverage_compress(root, "reports ; $literal/file_a.gcov.json", json) && ok;
    ok = test_checks_coverage_run(root) == 1 && ok;
    string_replace(json, "file_one.c", "file_two.c");
    ok = test_checks_coverage_compress(root, "reports ; $literal/file_a.gcov.json", json) && ok;
    ok = test_checks_coverage_run(root) == 1 && ok;
    ok = test_checks_write(root, "src/file/file_two.c", "/* synthetic second source */\n") && ok;
    ok = test_checks_coverage_run(root) == 0 && ok;
    ok = test_checks_write(root, "src/file/file_three.c", "/* missing report */\n") && ok;
    ok = test_checks_coverage_run(root) == 1 && ok;
    string_free(json);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "reports require exactly one record for every source regardless of report filename order");
}

static void test_checks_coverage_aggregate_and_filtering(void)
{
    string_t *root = test_checks_root();
    string_t *first = test_checks_coverage_json(9, 9, 4, 4, "1");
    string_t *second = test_checks_coverage_json(0, 1, 0, 1, "0");
    string_replace(second, "file_one.c", "file_two.c");
    bool ok = test_checks_coverage_tree(root) && test_checks_write(root, "src/file/file_two.c", "/* second */\n") &&
              test_checks_coverage_compress(root, "reports ; $literal/file_z.gcov.json", first) &&
              test_checks_coverage_compress(root, "reports ; $literal/file_a.gcov.json", second) &&
              test_checks_write(root, "reports ; $literal/other.gcov.json.gz", "ignored non-matching name");
    ok = ok && test_checks_coverage_run(root) == 0;
    string_replace(first, "{\"files\":[", "{\"files\":[{\"file\":\"include/file.h\"},");
    string_replace(first, "src/file/file_one.c", "./src//file/./file_one.c");
    ok = test_checks_coverage_compress(root, "reports ; $literal/file_z.gcov.json", first) && ok;
    ok = test_checks_coverage_run(root) == 0 && ok;
    string_free(first);
    string_free(second);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "thresholds use aggregate counts and ignore non-module records and unrelated report names");
}

static void test_checks_coverage_empty_inventories(void)
{
    string_t *root = test_checks_root();
    string_t *directory = checks_path(root, "reports ; $literal");
    bool ok = test_checks_coverage_tree(root) && checks_mkdir(directory);
    ok = ok && test_checks_coverage_run(root) == 1;
    string_free(directory);
    string_t *json = test_checks_coverage_json(1, 1, 1, 1, "1");
    ok = test_checks_coverage_compress(root, "reports ; $literal/file_one.gcov.json", json) && ok;
    ok = test_checks_write(root, "include/file.h", "/* no public functions */\n") && ok;
    ok = test_checks_coverage_run(root) == 1 && ok;
    string_free(json);
    ok = test_checks_finish(root) && ok;
    root = test_checks_root();
    directory = checks_path(root, "src/file");
    ok = checks_mkdir(directory) && test_checks_write(root, "include/file.h", "int file_one(void);\n") && ok;
    string_free(directory);
    json = test_checks_coverage_json(1, 1, 1, 1, "1");
    ok = test_checks_coverage_compress(root, "reports ; $literal/file_one.gcov.json", json) && ok;
    ok = test_checks_coverage_run(root) == 1 && ok;
    string_free(json);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "empty report, public API and source inventories cannot yield vacuous coverage passes");
}

static void test_checks_coverage_usage_and_io(void)
{
    string_t *root = test_checks_root();
    char absent[] = "absent", empty[] = "";
    char *arguments[] = {absent, absent};
    bool ok = checks_coverage(root, 0, NULL) == 2 && checks_coverage(root, 2, arguments) == 2 &&
              checks_coverage(root, 1, NULL) == 2 && checks_coverage(NULL, 1, arguments) == 2;
    arguments[0] = empty;
    ok = checks_coverage(root, 1, arguments) == 2 && ok;
    arguments[0] = absent;
    ok = checks_coverage(root, 1, arguments) == 2 && ok;
    ok = test_checks_coverage_tree(root) &&
         test_checks_write(root, "reports ; $literal/file_one.gcov.json.gz", "not gzip") && ok;
    ok = test_checks_coverage_run(root) == 2 && ok;
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "usage, absent directories and broken compressed reports fail without division or crashes");
}

/* Register ordinary synthetic cases; the parent keeps README examples last. */
void test_checks_coverage_cases(void)
{
    TEST_RUN_IN_GROUP(test_checks_coverage_thresholds, tests, NULL);
    TEST_RUN_IN_GROUP(test_checks_coverage_zero_and_unexecuted, tests, NULL);
    TEST_RUN_IN_GROUP(test_checks_coverage_counter_spelling, tests, NULL);
    TEST_RUN_IN_GROUP(test_checks_coverage_malformed_and_duplicate_records, tests, NULL);
    TEST_RUN_IN_GROUP(test_checks_coverage_inventory, tests, NULL);
    TEST_RUN_IN_GROUP(test_checks_coverage_aggregate_and_filtering, tests, NULL);
    TEST_RUN_IN_GROUP(test_checks_coverage_empty_inventories, tests, NULL);
    TEST_RUN_IN_GROUP(test_checks_coverage_usage_and_io, tests, NULL);
}
