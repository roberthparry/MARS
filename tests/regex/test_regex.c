/**
 * @file test_regex.c
 * @brief Compiled UTF-8 expression, capture, locale and input-policy regressions.
 *
 * Exercises public ownership and Linux matching semantics without sharing native
 * engine types. Runnable Markdown examples execute after ordinary regressions.
 */
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <string.h>

#include "regex-t.h"
#include "test_harness.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

static regex_t *regex_test_compile(const char *pattern, const regex_options_t *options)
{
    string_t *text = string_new();
    regex_t *regex = text && string_append_utf8_exact(text, pattern, strlen(pattern)) == 0
                         ? regex_new(text, options, NULL) : NULL;
    string_free(text);
    return regex;
}

static void test_regex_captures(void)
{
    regex_t *regex = regex_test_compile("(a|aa)(b)?(c*)", NULL);
    string_t *subject = string_new_with("zaa");
    regex_span_t spans[6];
    TEST_ASSERT_TRUE(regex != NULL, "compile captures");
    TEST_ASSERT_INT_EQ(regex_group_count(regex), 3);
    TEST_ASSERT_INT_EQ(regex_search(regex, subject, 0, spans, 6, NULL), REGEX_MATCH);
    TEST_ASSERT_TRUE(spans[0].start == 1 && spans[0].end == 3, "leftmost longest match");
    TEST_ASSERT_TRUE(spans[1].start == 1 && spans[1].end == 3, "longest alternative");
    TEST_ASSERT_TRUE(!spans[2].matched && spans[3].matched && spans[3].start == spans[3].end,
                     "absent and empty groups differ");
    TEST_ASSERT_TRUE(!spans[4].matched && !spans[5].matched, "excess captures cleared");
    string_t *missing = regex_capture(subject, &spans[2]);
    TEST_ASSERT_TRUE(!missing && errno == 0, "missing is not an error");
    string_t *empty = regex_capture(subject, &spans[3]);
    TEST_ASSERT_TRUE(empty && string_byte_length(empty) == 0, "owned empty capture");
    string_free(empty);
    TEST_ASSERT_INT_EQ(regex_search(regex, subject, 3, spans, 6, NULL), REGEX_NO_MATCH);
    TEST_ASSERT_TRUE(!spans[0].matched && spans[0].start == 0, "no-match resets captures");
    regex_free(regex);
    regex_free(NULL);
    TEST_ASSERT_INT_EQ(regex_group_count(NULL), 0);
    string_free(subject);
}

static void test_regex_unicode_locale(void)
{
    locale_t locale = newlocale(LC_ALL_MASK, "C", (locale_t)0);
    TEST_ASSERT_TRUE(locale != (locale_t)0, "fixture locale");
    locale_t previous = uselocale(locale);
    regex_options_t options = regex_options_default();
    options.ignore_case = true;
    regex_t *regex = regex_test_compile("é(.)", &options);
    string_t *subject = string_new_with("xÉ🙂");
    regex_span_t spans[2];
    bool ok = regex && regex_search(regex, subject, 1, spans, 2, NULL) == REGEX_MATCH &&
              spans[0].start == 1 && spans[0].end == 7 && spans[1].start == 3 && spans[1].end == 7 &&
              uselocale((locale_t)0) == locale;
    regex_free(regex);
    uselocale(previous);
    freelocale(locale);
    string_free(subject);
    TEST_ASSERT_TRUE(ok, "UTF-8 scalar matching and caller locale restored");

    subject = string_new();
    TEST_ASSERT_INT_EQ(string_append_utf8_exact(subject, "e\xcc\x81", 3), 0);
    regex = regex_test_compile("(é)", NULL);
    TEST_ASSERT_INT_EQ(regex_search(regex, subject, 0, spans, 2, NULL), REGEX_MATCH);
    string_t *capture = regex_capture(subject, &spans[1]);
    TEST_ASSERT_TRUE(capture && string_byte_length(capture) == 3, "capture preserves decomposed spelling");
    TEST_ASSERT_INT_EQ(regex_search(regex, subject, 2, NULL, 0, NULL), REGEX_ERROR);
    TEST_ASSERT_INT_EQ(errno, EINVAL);
    string_free(capture);
    string_free(subject);
    regex_free(regex);
}

static void test_regex_anchors(void)
{
    string_t *subject = string_new_with("x\nAb\n");
    regex_options_t options = regex_options_default();
    options.multiline = true;
    options.ignore_case = true;
    regex_t *regex = regex_test_compile("^ab$", &options);
    regex_span_t span;
    TEST_ASSERT_INT_EQ(regex_search(regex, subject, 2, &span, 1, NULL), REGEX_MATCH);
    TEST_ASSERT_TRUE(span.start == 2 && span.end == 4, "line anchor retains original offsets");
    regex_free(regex);
    regex = regex_test_compile("^b", &options);
    TEST_ASSERT_INT_EQ(regex_search(regex, subject, 3, &span, 1, NULL), REGEX_NO_MATCH);
    regex_free(regex);
    regex = regex_test_compile("^Ab", NULL);
    TEST_ASSERT_INT_EQ(regex_search(regex, subject, 2, &span, 1, NULL), REGEX_NO_MATCH);
    regex_free(regex);
    string_free(subject);
    subject = string_new_with("a\nb");
    const char *patterns[] = {"a.b", "a[^x]b"};
    for (size_t i = 0; i < 2; ++i) {
        regex = regex_test_compile(patterns[i], NULL);
        TEST_ASSERT_INT_EQ(regex_test(regex, subject, NULL), REGEX_MATCH);
        regex_free(regex);
        regex = regex_test_compile(patterns[i], &options);
        TEST_ASSERT_INT_EQ(regex_test(regex, subject, NULL), REGEX_NO_MATCH);
        regex_free(regex);
    }
    regex = regex_test_compile("", NULL);
    TEST_ASSERT_INT_EQ(regex_search(regex, subject, 3, &span, 1, NULL), REGEX_MATCH);
    TEST_ASSERT_TRUE(span.matched && span.start == 3 && span.end == 3, "empty match at EOF");
    regex_free(regex);
    string_free(subject);
}

static void test_regex_invalid(void)
{
    string_t *pattern = string_new_with("[");
    string_t *error = NULL;
    regex_t *regex = regex_new(pattern, NULL, &error);
    TEST_ASSERT_TRUE(!regex && error && errno == EINVAL, "invalid pattern diagnostic");
    string_free(error);
    error = NULL;
    string_free(pattern);
    regex = regex_test_compile(".", NULL);
    const char *invalid[] = {"\x80", "\xc0\xaf", "\xe2\x82", "\xed\xa0\x80", "\xf4\x90\x80\x80", "a\0b"};
    const size_t sizes[] = {1, 2, 2, 3, 4, 3};
    for (size_t i = 0; i < sizeof(sizes) / sizeof(*sizes); ++i) {
        string_t *text = string_new();
        TEST_ASSERT_INT_EQ(string_append_bytes(text, invalid[i], sizes[i]), 0);
        TEST_ASSERT_TRUE(regex_new(text, NULL, &error) == NULL && errno == EILSEQ, "invalid pattern bytes");
        string_free(error);
        error = NULL;
        TEST_ASSERT_INT_EQ(regex_test(regex, text, &error), REGEX_ERROR);
        TEST_ASSERT_TRUE(error && errno == EILSEQ, "invalid subject bytes");
        string_free(error);
        error = NULL;
        string_free(text);
    }
    string_t *subject = string_new_with("x");
    regex_span_t span = {.start = 0, .end = 2, .matched = true};
    TEST_ASSERT_TRUE(!regex_capture(subject, &span) && errno == EINVAL, "invalid capture extent");
    TEST_ASSERT_TRUE(!regex_capture(NULL, &span) && errno == EINVAL, "null capture subject");
    TEST_ASSERT_INT_EQ(regex_search(regex, subject, 2, &span, 1, NULL), REGEX_ERROR);
    TEST_ASSERT_INT_EQ(regex_search(regex, subject, 0, NULL, 1, NULL), REGEX_ERROR);
    TEST_ASSERT_INT_EQ(regex_test(NULL, subject, NULL), REGEX_ERROR);
    TEST_ASSERT_INT_EQ(regex_test(regex, NULL, NULL), REGEX_ERROR);
    TEST_ASSERT_TRUE(!regex_new(NULL, NULL, NULL), "null pattern");
    regex_free(regex);
    string_free(subject);
}

static void test_regex_limits(void)
{
    regex_options_t options = regex_options_default();
    options.max_pattern_bytes = 1;
    TEST_ASSERT_TRUE(!regex_test_compile("ab", &options) && errno == EOVERFLOW, "pattern limit");
    options.max_pattern_bytes = 0;
    TEST_ASSERT_TRUE(!regex_test_compile("", &options) && errno == EINVAL, "invalid policy");
    options = regex_options_default();
    options.max_groups = 1;
    TEST_ASSERT_TRUE(!regex_test_compile("(a)(b)", &options) && errno == EOVERFLOW, "group limit");
    options.max_subject_bytes = 1;
    regex_t *regex = regex_test_compile("(a)", &options);
    string_t *subject = string_new_with("aa");
    regex_span_t spans[20];
    TEST_ASSERT_INT_EQ(regex_test(regex, subject, NULL), REGEX_ERROR);
    TEST_ASSERT_INT_EQ(errno, EOVERFLOW);
    TEST_ASSERT_INT_EQ(regex_search(regex, subject, 0, spans, 3, NULL), REGEX_ERROR);
    TEST_ASSERT_INT_EQ(errno, EINVAL);
    regex_free(regex);
    string_free(subject);
    regex = regex_test_compile("(a)(a)(a)(a)(a)(a)(a)(a)(a)(a)(a)(a)(a)(a)(a)(a)(a)", NULL);
    subject = string_new_with("aaaaaaaaaaaaaaaaa");
    TEST_ASSERT_INT_EQ(regex_search(regex, subject, 0, spans, 20, NULL), REGEX_MATCH);
    TEST_ASSERT_TRUE(spans[17].matched && spans[17].start == 16 && spans[17].end == 17 && !spans[18].matched,
                     "heap-backed capture offsets");
    TEST_ASSERT_INT_EQ(regex_search(regex, subject, 0, spans, 1, NULL), REGEX_MATCH);
    regex_free(regex);
    string_free(subject);
}

/* README example: compile and capture UTF-8 text. */
static void example_regex_capture(void)
{
    string_t *pattern = string_new_with("([[:alpha:]]+)=([0-9]+)");
    string_t *subject = string_new_with("Zoë=42");
    regex_t *regex = regex_new(pattern, NULL, NULL);
    regex_span_t spans[3];
    bool ok = regex && subject && regex_search(regex, subject, 0, spans, 3, NULL) == REGEX_MATCH;
    string_t *name = ok ? regex_capture(subject, &spans[1]) : NULL;
    string_t *value = ok ? regex_capture(subject, &spans[2]) : NULL;
    ok = name && value;
    if (ok)
        string_printf("%S: %S\n", name, value);
    bool output = ok && !strcmp(string_c_str(name), "Zoë") && !strcmp(string_c_str(value), "42");
    string_free(value);
    string_free(name);
    regex_free(regex);
    string_free(subject);
    string_free(pattern);
    TEST_ASSERT_TRUE(output, "README capture output");
}

/* README example: reuse a pattern for successive nonempty matches. */
static void example_regex_iteration(void)
{
    string_t *pattern = string_new_with("[0-9]+");
    string_t *subject = string_new_with("item 12, item 34");
    regex_t *regex = regex_new(pattern, NULL, NULL);
    regex_status_t status = REGEX_ERROR;
    regex_span_t span;
    size_t start = 0, count = 0;
    bool output = true;
    if (regex && subject) {
        while ((status = regex_search(regex, subject, start, &span, 1, NULL)) == REGEX_MATCH) {
            string_t *value = regex_capture(subject, &span);
            if (!value) {
                status = REGEX_ERROR;
                break;
            }
            string_printf("%S\n", value);
            output = output && count < 2 && !strcmp(string_c_str(value), count == 0 ? "12" : "34");
            ++count;
            string_free(value);
            start = span.end;
        }
    }
    regex_free(regex);
    string_free(subject);
    string_free(pattern);
    TEST_ASSERT_TRUE(output && count == 2 && status == REGEX_NO_MATCH, "README iteration output");
}

int tests_main(void)
{
    TEST_RUN_CASE(test_regex_captures, NULL);
    TEST_RUN_CASE(test_regex_unicode_locale, NULL);
    TEST_RUN_CASE(test_regex_anchors, NULL);
    TEST_RUN_CASE(test_regex_invalid, NULL);
    TEST_RUN_CASE(test_regex_limits, NULL);
    TEST_SECTION("README examples");
    TEST_RUN_OUTPUT(example_regex_capture);
    TEST_RUN_OUTPUT(example_regex_iteration);
    return TEST_EXIT_CODE();
}
