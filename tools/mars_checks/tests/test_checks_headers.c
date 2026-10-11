/**
 * @file test_checks_headers.c
 * @brief Compiler regressions for explicit private-header access boundaries.
 *
 * Discovers implementation headers and checks authorised access, rejected direct
 * includes, and editor-only direct opening. Fixtures use owned temporary files;
 * compiler processes run sequentially and never link or execute application code.
 */
#include <stdio.h>

#include "checks_process.h"
#include "test_harness.h"
#include "test_checks.h"

static bool test_headers_private(const string_t *path)
{
    if (string_find(path, "/vendor/") >= 0 || string_find(path, "/third_party/") >= 0)
        return false;
    return string_starts_with(path, "src/") || string_find(path, "/src/") >= 0 ||
           string_ends_with(path, "_internal.h") || string_ends_with(path, "_private.h");
}

static checks_strings_t *test_headers_collect(const string_t *root)
{
    const char *const directories[] = {"src", "tools", "tests"};
    checks_strings_t *headers = checks_strings_new();
    for (size_t i = 0; i < sizeof(directories) / sizeof(*directories); ++i) {
        checks_strings_t *paths = checks_files(root, directories[i], ".h", true);
        for (size_t j = 0; j < checks_strings_count(paths); ++j) {
            const string_t *path = checks_strings_get(paths, j);
            if (test_headers_private(path))
                checks_strings_add(headers, string_clone(path));
        }
        checks_strings_free(paths);
    }
    checks_strings_sort(headers);
    return headers;
}

static bool test_headers_preprocess(const string_t *root, const string_t *input, bool editor,
                                    const string_t *expected_error)
{
    const char *const options[] = {
        "cc", "-E", "-x", "c", "-D_GNU_SOURCE", "-I.", "-Iinclude", "-Isrc", "-Itests/include",
        "-Itools/mars_checks/include", "-Itools/mars_config/include", "-Itools/mars_lab/include",
        "-Itools/to_be_announced_lab/include", "-o", "/dev/null"
    };
    checks_strings_t *arguments = checks_strings_new();
    for (size_t i = 0; i < sizeof(options) / sizeof(*options); ++i)
        checks_strings_add(arguments, checks_text(options[i]));
    if (editor)
        checks_strings_add(arguments, checks_text("-D__INTELLISENSE__"));
    checks_strings_add(arguments, string_clone(input));
    string_t *output = NULL, *errors = NULL;
    int status = -1;
    bool ran = checks_process_run(arguments, root, 10, &output, &errors, &status);
    bool ok = ran && (expected_error ? status > 0 && errors &&
                                     string_find(errors, string_c_str(expected_error)) >= 0 : status == 0);
    string_free(errors);
    string_free(output);
    checks_strings_free(arguments);
    return ok;
}

static bool test_headers_probe(const string_t *root, const string_t *temporary, const string_t *header)
{
    string_t *path = checks_path(root, string_c_str(header));
    string_t *source = checks_read(path), *macro = NULL;
    bool ok = source && checks_match(source, "^#if !defined\\((MARS_[A-Z_]+_INTERNAL_ACCESS)\\)", 1, &macro);
    string_t *probe = checks_path(temporary, "probe.c");
    string_t *allowed = macro ? string_sprintf("#define %S\n#include \"%S\"\n", macro, header) : NULL;
    string_t *denied = string_sprintf("#include \"%S\"\n", header);
    string_t *diagnostic = macro ? string_sprintf("Private header requires %S.", macro) : NULL;
    ok = ok && allowed && denied && diagnostic && checks_write(probe, allowed) &&
         test_headers_preprocess(root, probe, false, NULL);
    ok = ok && checks_write(probe, denied) && test_headers_preprocess(root, probe, false, diagnostic);
    /* Editor mode must not authorise an include from another translation unit. */
    ok = ok && test_headers_preprocess(root, probe, true, diagnostic);
    /* Opening the header itself in an editor still permits its nested includes. */
    ok = ok && test_headers_preprocess(root, path, true, NULL);
    string_free(diagnostic);
    string_free(denied);
    string_free(allowed);
    string_free(probe);
    string_free(macro);
    string_free(source);
    string_free(path);
    return ok;
}

static void test_checks_private_header_access(void)
{
    string_t *root = checks_text(MARS_CHECKS_ROOT_DIR), *temporary = test_checks_root();
    checks_strings_t *headers = test_headers_collect(root);
    bool ok = checks_strings_count(headers) > 0;
    for (size_t i = 0; i < checks_strings_count(headers); ++i) {
        bool passed = test_headers_probe(root, temporary, checks_strings_get(headers, i));
        if (!passed)
            fprintf(stderr, "Private-header access probe %zu failed\n", i + 1);
        ok = passed && ok;
    }
    printf("Checked %zu private headers with four compiler probes each\n", checks_strings_count(headers));
    checks_strings_free(headers);
    string_free(root);
    ok = test_checks_finish(temporary) && ok;
    TEST_ASSERT_TRUE(ok, "private headers require explicit access without disabling direct editor support");
}

/* Run compiler boundary regressions before the suite's README examples. */
void test_checks_header_cases(void)
{
    TEST_RUN_IN_GROUP(test_checks_private_header_access, tests, NULL);
}
