/**
 * @file test_diagnostic.c
 * @brief Structured diagnostic ownership, location, cause and concurrent-reader regressions.
 *
 * Exercises the public immutable interface, including bounded deep chains and
 * independent retained references. README examples run after ordinary cases.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "thread-t.h"
#include "diagnostic-t.h"
#include "test_harness.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

static void diagnostic_require(bool ok)
{
    if (!ok)
        abort();
}

static void test_diagnostic_ownership(void)
{
    char message[] = "Expected a value", source[] = "input", domain[] = "parser";
    diagnostic_location_t location = {.source = source, .offset = 4, .length = 2, .line = 1, .column = 5};
    diagnostic_t *cause = diagnostic_new(DIAGNOSTIC_ERROR, domain, 7, message, &location, NULL);
    diagnostic_require(cause != NULL);
    diagnostic_t *outer = diagnostic_new(DIAGNOSTIC_WARNING, "request", 9, "Cannot evaluate", NULL, cause);
    diagnostic_require(outer != NULL);
    diagnostic_free(cause);
    message[0] = source[0] = domain[0] = 'X';
    diagnostic_t *shared = diagnostic_retain(outer);
    diagnostic_free(outer);
    const diagnostic_t *inner = diagnostic_cause(shared);
    bool ok = diagnostic_depth(shared) == 2 && diagnostic_severity(shared) == DIAGNOSTIC_WARNING &&
              diagnostic_code(inner) == 7 && !strcmp(diagnostic_message(inner), "Expected a value") &&
              !strcmp(diagnostic_domain(inner), "parser") && !strcmp(diagnostic_location(inner)->source, "input") &&
              !diagnostic_location(shared);
    string_t *text = diagnostic_to_string(shared);
    diagnostic_require(text != NULL);
    ok = ok && !strcmp(string_c_str(text), "warning [request:9]: Cannot evaluate\n"
                                         "Caused by: error [parser:7]: Expected a value (input:1:5; bytes 4+2)");
    diagnostic_t *retained_cause = diagnostic_retain(inner);
    diagnostic_require(retained_cause != NULL);
    diagnostic_free(shared);
    ok = diagnostic_depth(retained_cause) == 1 && diagnostic_code(retained_cause) == 7 && ok;
    diagnostic_free(retained_cause);
    string_free(text);
    TEST_ASSERT_TRUE(ok, "copy all text, retain causes and render independent output");
}

static void test_diagnostic_locations(void)
{
    diagnostic_location_t location = {.offset = 12, .length = 0};
    diagnostic_t *item = diagnostic_new(DIAGNOSTIC_NOTE, "unicode", -42, "σ: value", &location, NULL);
    diagnostic_require(item != NULL);
    string_t *text = diagnostic_to_string(item);
    bool ok = text && !strcmp(string_c_str(text), "note [unicode:-42]: σ: value (source; bytes 12+0)");
    string_free(text);
    errno = ERANGE;
    diagnostic_free(item);
    ok = ok && errno == ERANGE;
    TEST_ASSERT_TRUE(ok, "UTF-8 messages, signed codes, unknown line and errno-preserving cleanup");
}

static void test_diagnostic_invalid(void)
{
    TEST_ASSERT_TRUE(!diagnostic_new(DIAGNOSTIC_INVALID, "a", 0, "b", NULL, NULL) && errno == EINVAL, "severity");
    TEST_ASSERT_TRUE(!diagnostic_new(DIAGNOSTIC_ERROR, "", 0, "b", NULL, NULL) && errno == EINVAL, "domain");
    TEST_ASSERT_TRUE(!diagnostic_new(DIAGNOSTIC_ERROR, "a", 0, NULL, NULL, NULL) && errno == EINVAL, "message");
    diagnostic_location_t location = {.column = 2};
    TEST_ASSERT_TRUE(!diagnostic_new(DIAGNOSTIC_ERROR, "a", 0, "b", &location, NULL) && errno == EINVAL, "line");
    location = (diagnostic_location_t){.offset = SIZE_MAX, .length = 1};
    TEST_ASSERT_TRUE(!diagnostic_new(DIAGNOSTIC_ERROR, "a", 0, "b", &location, NULL) && errno == EOVERFLOW, "extent");
    TEST_ASSERT_TRUE(!diagnostic_retain(NULL) && errno == EINVAL, "NULL retain");
    TEST_ASSERT_TRUE(diagnostic_severity(NULL) == DIAGNOSTIC_INVALID, "NULL severity");
    TEST_ASSERT_TRUE(!diagnostic_message(NULL) && !diagnostic_domain(NULL), "NULL text");
    TEST_ASSERT_TRUE(!diagnostic_code(NULL) && !diagnostic_depth(NULL), "NULL scalars");
    TEST_ASSERT_TRUE(!diagnostic_location(NULL) && !diagnostic_cause(NULL), "NULL optional fields");
    TEST_ASSERT_TRUE(!diagnostic_to_string(NULL) && errno == EINVAL, "NULL formatting");
    diagnostic_free(NULL);
}

static void test_diagnostic_depth(void)
{
    diagnostic_t *chain = NULL;
    for (size_t i = 0; i < 64; ++i) {
        diagnostic_t *next = diagnostic_new(DIAGNOSTIC_ERROR, "chain", (int64_t)i, "Cause", NULL, chain);
        diagnostic_require(next != NULL);
        diagnostic_free(chain);
        chain = next;
    }
    diagnostic_t *extra = diagnostic_new(DIAGNOSTIC_ERROR, "chain", 64, "Cause", NULL, chain);
    bool ok = !extra && errno == ELOOP && diagnostic_depth(chain) == 64;
    string_t *text = diagnostic_to_string(chain);
    ok = ok && text != NULL;
    string_free(text);
    diagnostic_free(extra);
    diagnostic_free(chain);
    TEST_ASSERT_TRUE(ok, "bound construction and render/release maximal chain without recursion");
}

static void *diagnostic_reader(void *argument)
{
    diagnostic_t *item = argument;
    for (size_t i = 0; i < 10000; ++i) {
        diagnostic_t *reference = diagnostic_retain(item);
        diagnostic_require(reference && diagnostic_code(reference) == 42);
        diagnostic_free(reference);
    }
    return NULL;
}

static void test_diagnostic_concurrent(void)
{
    diagnostic_t *item = diagnostic_new(DIAGNOSTIC_NOTE, "shared", 42, "Ready", NULL, NULL);
    diagnostic_require(item != NULL);
    thread_t *readers[4];
    for (size_t i = 0; i < 4; ++i)
        diagnostic_require((readers[i] = thread_new(diagnostic_reader, item)) != NULL);
    for (size_t i = 0; i < 4; ++i)
        diagnostic_require(thread_join(readers[i], NULL) && thread_free(readers[i]));
    bool ok = diagnostic_code(item) == 42;
    diagnostic_free(item);
    TEST_ASSERT_TRUE(ok, "concurrent retained owners leave the original intact");
}

/* README example: carry a located cause without retaining caller text buffers. */
static void example_diagnostic_chain(void)
{
    diagnostic_location_t location = {.source = "input", .offset = 4, .length = 1, .line = 1, .column = 5};
    diagnostic_t *cause = diagnostic_new(DIAGNOSTIC_ERROR, "parser", 7, "Expected a value", &location, NULL);
    diagnostic_t *error = cause ? diagnostic_new(DIAGNOSTIC_ERROR, "request", 2, "Cannot evaluate", NULL, cause) : NULL;
    diagnostic_free(cause);
    string_t *text = error ? diagnostic_to_string(error) : NULL;
    if (text)
        printf("%s\n", string_c_str(text));
    bool ok = text != NULL;
    string_free(text);
    diagnostic_free(error);
    TEST_ASSERT_TRUE(ok, "README located cause chain");
}

int tests_main(void)
{
    TEST_RUN_CASE(test_diagnostic_ownership, NULL);
    TEST_RUN_CASE(test_diagnostic_locations, NULL);
    TEST_RUN_CASE(test_diagnostic_invalid, NULL);
    TEST_RUN_CASE(test_diagnostic_depth, NULL);
    TEST_RUN_CASE(test_diagnostic_concurrent, NULL);
    TEST_SECTION("README examples");
    TEST_RUN_OUTPUT(example_diagnostic_chain);
    return TEST_EXIT_CODE();
}
