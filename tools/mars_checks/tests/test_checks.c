/**
 * @file test_checks.c
 * @brief Sequential native checker suite and disposable filesystem support.
 *
 * Uses the existing global test configuration and shared tests_main harness.
 * README programme cases are registered last. Helpers only create and remove
 * uniquely named temporary trees, using the same public file API as the tool.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>

#include "test_harness.h"
#include "test_checks.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

/* Allocate a private fixture tree. */
string_t *test_checks_root(void)
{
    char pattern[] = "/tmp/mars-checks-tests-XXXXXX";
    if (!mkdtemp(pattern))
        checks_fatal("creating checker test fixture");
    return checks_text(pattern);
}

/* Write a fixture through the file.h adapter. */
bool test_checks_write(const string_t *root, const char *path, const char *content)
{
    string_t *relative = checks_text(path);
    size_t parent_end = 0;
    for (size_t i = 0; i < string_byte_length(relative); ++i)
        if (checks_byte(relative, i) == '/')
            parent_end = i;
    bool ok = true;
    if (parent_end) {
        string_t *parent = checks_slice(relative, 0, parent_end);
        string_t *directory = checks_path(root, string_c_str(parent));
        ok = checks_mkdir(directory);
        string_free(directory);
        string_free(parent);
    }
    string_t *destination = checks_path(root, path), *text = checks_text(content);
    ok = ok && checks_write(destination, text);
    string_free(destination);
    string_free(text);
    string_free(relative);
    return ok;
}

/* Clean up before reporting a fixture's assertions. */
bool test_checks_finish(string_t *root)
{
    bool ok = checks_remove_tree(root);
    string_free(root);
    return ok;
}

/* Register all cases, with README examples last. */
int tests_main(void)
{
    TEST_SECTION("Native checker parsing and policy controls");
    test_checks_parser_cases();
    test_checks_control_cases();
    test_checks_coverage_cases();
    test_checks_evidence_cases();
    test_checks_policy_cases();
    TEST_SECTION("Migrated Markdown API coverage and mathematical presentation");
    test_checks_documentation_cases();
    TEST_SECTION("README examples (last)");
    test_checks_readme_cases();
    test_checks_policy_readme_cases();
    return TEST_EXIT_CODE();
}
