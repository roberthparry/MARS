/**
 * @file test_checks_parsers.c
 * @brief Positive and negative source, Markdown and compiler-token regressions.
 *
 * Exercises quoted compiler arguments, declaration boundaries, excluded reference
 * fences, stable identifiers and output matching. All input lives in MARS strings;
 * no compiler or shell is needed by these correctness cases.
 */
#include "test_harness.h"
#include "checks_process.h"
#include "checks_controls.h"
#include "checks_readme.h"
#include "test_checks.h"

static void test_checks_shell_words(void)
{
    string_t *source = checks_text("cc 'two words' \"\" -DNAME=\"a b\" escaped\\ space ';$(touch nope)' \"a\\q\"");
    checks_strings_t *words = checks_strings_new();
    bool ok = checks_shell_words(source, words) && checks_strings_count(words) == 7;
    const char *const expected[] = {"cc", "two words", "", "-DNAME=a b", "escaped space", ";$(touch nope)", "a\\q"};
    for (size_t i = 0; ok && i < sizeof(expected) / sizeof(*expected); ++i)
        ok = checks_equal(checks_strings_get(words, i), expected[i]);
    checks_strings_free(words);
    string_free(source);
    TEST_ASSERT_TRUE(ok, "compiler words preserve quoting, empty arguments and literal shell metacharacters");
}

static void test_checks_shell_words_reject_malformed(void)
{
    const char *const invalid[] = {"cc 'unterminated", "cc \"unterminated", "cc trailing\\"};
    bool ok = true;
    for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i) {
        string_t *source = checks_text(invalid[i]);
        checks_strings_t *words = checks_strings_new();
        ok = !checks_shell_words(source, words) && ok;
        checks_strings_free(words);
        string_free(source);
    }
    TEST_ASSERT_TRUE(ok, "unterminated quotes and escapes are rejected without invoking a shell");
}

static void test_checks_extractor_finds_prototypes_and_inline_functions_only(void)
{
    string_t *source = checks_text("typedef int (*callback_t)(int value);\nint public_call(int value);\n"
                                   "static inline int inline_call(int value) { return value + 1; }\n"
                                   "#define FUNCTION_LIKE(value) (value)\n");
    checks_strings_t *names = checks_header_functions(source);
    bool ok = checks_strings_count(names) == 2 && checks_equal(checks_strings_get(names, 0), "inline_call") &&
              checks_equal(checks_strings_get(names, 1), "public_call");
    checks_strings_free(names);
    string_free(source);
    TEST_ASSERT_TRUE(ok, "ported Python regression extracts prototypes and public inline definitions only");
}

static void test_checks_header_comments_directives_duplicates(void)
{
    string_t *source = checks_text("/* int hidden(void); */\n// int another(void);\n"
                                   "#define GENERATED(x) \\\n int not_public(x);\n"
                                   "typedef struct { int member; } object_t;\n"
                                   "_Static_assert(1, \"yes\");\nint visible(void); int visible(void);\n"
                                   "static inline int nested(void) { if (1) { return 1; } return 0; }\n"
                                   "__attribute__((warn_unused_result)) int attributed(void);\n");
    checks_strings_t *names = checks_header_functions(source);
    bool ok = checks_strings_count(names) == 3 && checks_strings_has(names, "visible") &&
              checks_strings_has(names, "nested") && checks_strings_has(names, "attributed");
    checks_strings_free(names);
    string_free(source);
    TEST_ASSERT_TRUE(ok, "comments, continued directives, typedefs and bodies do not leak declarations");
}

static void test_checks_function_identifier_boundaries(void)
{
    string_t *name = checks_text("file_open");
    const char *const sources[] = {"`file_open()`", "file_open", "xfile_open", "file_open_extra", "file_open2"};
    bool ok = true;
    for (size_t i = 0; i < sizeof(sources) / sizeof(*sources); ++i) {
        string_t *source = checks_text(sources[i]);
        ok = (checks_function_mentioned(name, source) == (i < 2)) && ok;
        string_free(source);
    }
    string_free(name);
    TEST_ASSERT_TRUE(ok, "guide mentions require whole ASCII identifiers");
}

static void test_checks_markdown_fences_and_reference_indices(void)
{
    string_t *path = checks_text("docs/sample-guide.md");
    string_t *text = checks_text("# Guide\n```c\ntypedef enum { FIRST = 1 } item_t;\n```\n"
                                 "```c\nint main(void) { return 0; }\n```\n"
                                 "An explanation.\n```sh\nignored\n```\n```text\nok\n```\n"
                                 "```c\ncall();\n```\n# Next section\n```text\nnot associated\n```\n");
    checks_examples_t *examples = checks_examples_parse(path, text);
    const checks_example_t *first = checks_examples_get(examples, 0), *second = checks_examples_get(examples, 1);
    bool ok = checks_examples_count(examples) == 2 && first && second &&
              checks_equal(checks_example_id(first), "docs_sample_guide_002") && checks_example_line(first) == 5 &&
              checks_example_has_main(first) && checks_equal(checks_example_expected(first), "ok\n") &&
              !checks_example_has_main(second) && !checks_example_expected(second);
    checks_examples_free(examples);
    string_free(path);
    string_free(text);
    TEST_ASSERT_TRUE(ok, "reference fences retain ordinal positions; headings terminate output association");
}

static void test_checks_markdown_missing_output_and_fences(void)
{
    string_t *path = checks_text("README.md");
    string_t *text = checks_text("```c\nint main(void) {}\n```\n```c\n#include <stdio.h>\n```\n"
                                 "```xml\n<a/>\n```\n```c\nint absent(void);\n```\n"
                                 "```c\nint main(void) {}\n");
    checks_examples_t *examples = checks_examples_parse(path, text);
    bool ok = checks_examples_count(examples) == 2 && !checks_example_expected(checks_examples_get(examples, 0)) &&
              checks_equal(checks_example_expected(checks_examples_get(examples, 1)), "<a/>\n");
    checks_examples_free(examples);
    string_free(path);
    string_free(text);
    TEST_ASSERT_TRUE(ok, "a later C fence ends output search; unclosed fences and API signatures are references");
}

static void test_checks_output_normalisation(void)
{
    string_t *source = checks_text("\n\r\n  first \t\r\n\r\nsecond\t\n\n");
    string_t *normal = checks_output_normalise(source);
    bool ok = checks_equal(normal, "  first\n\nsecond");
    string_free(source);
    string_free(normal);
    source = checks_text("e\xcc\x81\n");
    normal = checks_output_normalise(source);
    ok = checks_equal(normal, "e\xcc\x81") && ok;
    string_free(source);
    string_free(normal);
    TEST_ASSERT_TRUE(ok, "normalisation preserves indentation, interior blank lines and Unicode spelling");
}

/* Register parser correctness cases under the global configuration. */
void test_checks_parser_cases(void)
{
    TEST_RUN_IN_GROUP(test_checks_shell_words, tests, NULL);
    TEST_RUN_IN_GROUP(test_checks_shell_words_reject_malformed, tests, NULL);
    TEST_RUN_IN_GROUP(test_checks_extractor_finds_prototypes_and_inline_functions_only, tests, NULL);
    TEST_RUN_IN_GROUP(test_checks_header_comments_directives_duplicates, tests, NULL);
    TEST_RUN_IN_GROUP(test_checks_function_identifier_boundaries, tests, NULL);
    TEST_RUN_IN_GROUP(test_checks_markdown_fences_and_reference_indices, tests, NULL);
    TEST_RUN_IN_GROUP(test_checks_markdown_missing_output_and_fences, tests, NULL);
    TEST_RUN_IN_GROUP(test_checks_output_normalisation, tests, NULL);
}
