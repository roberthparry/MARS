/**
 * @file test_checks_policy.c
 * @brief Native inline scanner, expression registry and parser benchmark checks.
 *
 * Preserves the former policy scanner fixtures and audits the real repository's
 * inline definitions and both expression perfect hashes. Negative registry
 * fixtures operate on owned source copies. The public parser benchmark README
 * example has a separate registration entry point so it runs after ordinary
 * regressions; the parent Make graph builds its executable and supplies the
 * build-mode path through MARS_CHECKS_BENCHMARK_PATH.
 */
#include <stdio.h>

#include "file.h"
#include "test_harness.h"
#include "checks_process.h"
#include "checks_policy.h"
#include "test_checks.h"

static bool policy_scanner_case(const char *text, const char *span_text, const char *macro_text,
                                size_t violations, bool complete)
{
    string_t *source = checks_text(text), *expected_spans = checks_text(span_text), *expected_macros = checks_text(macro_text);
    checks_strings_t *spans = checks_strings_new(), *macros = checks_strings_new(), *errors = checks_strings_new();
    checks_strings_t *wanted_spans = checks_split(expected_spans, ','), *wanted_macros = checks_split(expected_macros, ',');
    bool ok = checks_policy_scan_inline(source, spans, macros, errors) == complete;
    ok = checks_strings_count(spans) == checks_strings_count(wanted_spans) &&
         checks_strings_count(macros) == checks_strings_count(wanted_macros) &&
         checks_strings_count(errors) == (complete ? 0u : 1u) && ok;
    for (size_t i = 0; i < checks_strings_count(wanted_spans); ++i)
        ok = checks_equal(checks_strings_get(spans, i), string_c_str(checks_strings_get(wanted_spans, i))) && ok;
    for (size_t i = 0; i < checks_strings_count(wanted_macros); ++i)
        ok = checks_equal(checks_strings_get(macros, i), string_c_str(checks_strings_get(wanted_macros, i))) && ok;
    if (!complete)
        ok = checks_equal(checks_strings_get(errors, 0), "unterminated inline definition at line 1") && ok;
    checks_strings_free(errors);
    errors = checks_strings_new();
    checks_policy_inline(source, errors);
    ok = checks_strings_count(errors) == violations && ok;
    checks_strings_free(errors);
    checks_strings_free(spans);
    checks_strings_free(macros);
    checks_strings_free(wanted_spans);
    checks_strings_free(wanted_macros);
    string_free(expected_spans);
    string_free(expected_macros);
    string_free(source);
    return ok;
}

static void test_policy_comments_and_literals(void)
{
    bool ok = policy_scanner_case(
        "/* static inline int fake(void) {\nreturn 1;\n} */\n"
        "// inline int fake(void) {} \\\ncontinued inline int fake(void) {}\n"
        "const char *text = \"inline int fake(void) { \\\"quoted\\\" }\";\n"
        "const char brace = '}';\n"
        "#define TEXT \"static inline int fake(void) {}\"\n"
        "#define NUMBER 1 /* inline is only documentation */\n", "", "", 0, true);
    TEST_ASSERT_TRUE(ok, "comments, continued comments and escaped literals are not inline definitions or macros");
}

static void test_policy_three_line_limit(void)
{
    const char *const keywords[] = {"__inline", "__inline__", "inline"};
    bool ok = true;
    for (size_t i = 0; i < sizeof(keywords) / sizeof(*keywords); ++i) {
        string_t *short_source = string_sprintf("static %s int f(void) {\n    return 1;\n}\n", keywords[i]);
        string_t *long_source = string_sprintf("static\n%s int f(void) {\n    return 1;\n}\n", keywords[i]);
        ok = policy_scanner_case(string_c_str(short_source), "1:3", "", 0, true) && ok;
        ok = policy_scanner_case(string_c_str(long_source), "1:4", "", 1, true) && ok;
        string_free(short_source);
        string_free(long_source);
    }
    TEST_ASSERT_TRUE(ok, "all three inline spellings count the complete physical declaration");
}

static void test_policy_prototypes_and_ordinary_functions(void)
{
    TEST_ASSERT_TRUE(policy_scanner_case(
        "static inline int prototype(int (*callback)(int));\n"
        "int ordinary(void) {\n    return 1;\n}\n"
        "static inline int actual(void) { return 2; }\n"
        "inline int another(void) { return 3; }\n", "5:5,6:6", "", 0, true),
        "prototypes and ordinary functions are ignored before adjacent inline definitions");
}

static void test_policy_nested_braces(void)
{
    TEST_ASSERT_TRUE(policy_scanner_case(
        "static inline int f(int x) {\n"
        "    if (x) { const char *s = \"}\"; /* } */ return s[0]; }\n"
        "    return '}';\n}\n", "1:4", "", 1, true),
        "nested blocks, comments and brace literals do not prematurely close the body");
}

static void test_policy_blank_and_comment_lines(void)
{
    TEST_ASSERT_TRUE(policy_scanner_case(
        "inline int f(void) {\n\n    /* Explanation. */\n    return 1;\n}\n", "1:5", "", 1, true),
        "blank and comment lines inside an inline body count towards the limit");
}

static void test_policy_concealed_inline_macros(void)
{
    const char *const definitions[] = {
        "#define MAKE(name) static inline int name(void) { return 1; }\n",
        "#define MAKE(name) \\\n    static inline int name(void) { \\\n    return 1; }\n",
        "#define LOCAL_INLINE static __inline__\n",
    };
    bool ok = true;
    for (size_t i = 0; i < sizeof(definitions) / sizeof(*definitions); ++i)
        ok = policy_scanner_case(definitions[i], "", "1", 1, true) && ok;
    TEST_ASSERT_TRUE(ok, "even short and continued macros must not conceal inline code or declarations");
}

static void test_policy_preprocessor_branches(void)
{
    TEST_ASSERT_TRUE(policy_scanner_case(
        "#if 0\ninline int f(void) {\n    return 1;\n}\n#endif\n", "2:4", "", 0, true),
        "inactive preprocessor branches are audited");
}

static void test_policy_unterminated_inline(void)
{
    TEST_ASSERT_TRUE(policy_scanner_case("inline int f(void) {\n", "", "", 1, false),
                     "an unterminated inline definition reports its start line");
}

static void test_policy_crlf_and_identifier_boundaries(void)
{
    bool ok = policy_scanner_case(
        "// inline fake \\\r\ncontinued inline fake {}\r\n"
        "  # define MAKE \\\r\nstatic __inline int f(void) {}\r\n"
        "int inline_name(void) {}\r\n"
        "inline int f(void) {\r\nreturn 1;\r\n}\r\n", "6:8", "3", 1, true);
    TEST_ASSERT_TRUE(ok, "CRLF continuations preserve line numbers and inline substrings are not keywords");
}

static string_t *policy_read_registry(bool binding)
{
    string_t *root = checks_text(MARS_CHECKS_ROOT_DIR);
    string_t *path = checks_path(root, binding ? "src/expression/expr_bindings.c" : "src/expression/expr_stringin.c");
    string_t *source = checks_read(path);
    string_free(path);
    string_free(root);
    return source;
}

static bool policy_registry_ok(bool binding)
{
    string_t *source = policy_read_registry(binding);
    checks_strings_t *errors = checks_strings_new();
    if (source)
        checks_policy_function_table(source, binding, errors);
    bool ok = source && !checks_strings_count(errors);
    for (size_t i = 0; i < checks_strings_count(errors); ++i)
        fprintf(stderr, "%s\n", string_c_str(checks_strings_get(errors, i)));
    checks_strings_free(errors);
    string_free(source);
    return ok;
}

static void test_policy_inline_perfect_hash_and_columns(void)
{
    TEST_ASSERT_TRUE(policy_registry_ok(false),
                     "inline table has unique aliases and slots, correct hashes, unused buckets and Unicode columns");
}

static void test_policy_binding_perfect_hash(void)
{
    TEST_ASSERT_TRUE(policy_registry_ok(true), "binding registry hashes, displacement bytes and sign aliases agree");
}

static void test_policy_registry_rejects_corruption(void)
{
    const char *const from[] = {".kw = \"signum\"", "#define FUNC_KEYWORD_MAX_BYTES 18u", ".ufn = expr_sgn,",
                                "    [  0] = {", "s_func_displacements[FUNC_HASH_BUCKETS] = {\n"};
    const char *const to[] = {".kw = \"sgn\"", "#define FUNC_KEYWORD_MAX_BYTES 1u", ".ufn = expr_cos,",
                              "    [  1] = {", "s_func_displacements[FUNC_HASH_BUCKETS] = {\n256,"};
    string_t *source = policy_read_registry(false);
    bool ok = source != NULL;
    for (size_t i = 0; source && i < sizeof(from) / sizeof(*from); ++i) {
        string_t *changed = string_clone(source);
        checks_strings_t *errors = checks_strings_new();
        string_replace(changed, from[i], to[i]);
        checks_policy_function_table(changed, false, errors);
        ok = checks_strings_count(errors) > 0 && ok;
        checks_strings_free(errors);
        string_free(changed);
    }
    string_free(source);
    TEST_ASSERT_TRUE(ok, "duplicate names and slots, keyword limits, wrong handlers and invalid shifts are rejected");
}

static void test_policy_project_inline_definitions(void)
{
    string_t *root = checks_text(MARS_CHECKS_ROOT_DIR);
    char *arguments[] = {"inline"};
    int status = checks_policy(root, 1, arguments);
    string_free(root);
    TEST_ASSERT_TRUE(status == 0, "all project-owned C sources obey the inline policy");
}

static void test_policy_command_and_exclusions(void)
{
    string_t *root = test_checks_root();
    char *arguments[] = {"inline"}, *invalid[] = {"unknown"};
    const char *bad = "inline int f(void) {\n\nreturn 1;\n}\n";
    bool ok = test_checks_write(root, "src/good.c", "inline int f(void) { return 1; }\n");
    const char *const excluded[] = {"src/build/bad.c", "src/vendor/bad.c", "src/third_party/bad.h",
                                    "src/node_modules/bad.c", "src/__pycache__/bad.c"};
    for (size_t i = 0; i < sizeof(excluded) / sizeof(*excluded); ++i)
        ok = test_checks_write(root, excluded[i], bad) && ok;
    ok = checks_policy(root, 1, arguments) == 0 && checks_policy(root, 1, invalid) == 2 && ok;
    ok = test_checks_write(root, "src/bad.c", bad) && checks_policy(root, 1, arguments) == 1 && ok;
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "source-policy prunes excluded trees, validates selectors and reports source violations");
}

static void test_policy_readme_parser_benchmark(void)
{
    /* README example: docs/benchmarks.md; registered separately after ordinary policy regressions. */
    string_t *root = checks_text(MARS_CHECKS_ROOT_DIR);
    checks_strings_t *arguments = checks_strings_new();
    checks_strings_add(arguments, checks_path(root, MARS_CHECKS_BENCHMARK_PATH));
    checks_strings_add(arguments, checks_text("--check"));
    string_t *output = NULL, *errors = NULL;
    int status = -1;
    bool ok = checks_process_run(arguments, root, 0, &output, &errors, &status);
    ok = ok && status == 0 && checks_equal(output, "parser benchmark: 24 inputs verified\n");
    if (ok)
        fputs(string_c_str(output), stdout);
    else if (errors)
        fputs(string_c_str(errors), stderr);
    string_free(output);
    string_free(errors);
    string_free(root);
    checks_strings_free(arguments);
    TEST_ASSERT_TRUE(ok, "README example: public parser benchmark verifies all 24 documented inputs");
}

static void test_policy_python_sources_and_shebangs(void)
{
    const char *const paths[] = {"src/example.py", "tools/window.pyw", "include/typing.pyi", "tools/direct",
                                 "tools/env", "tools/env_split", "tools/versioned", "tools/pypy", "tools/.hidden"};
    const char *const contents[] = {"# empty module\n", "# window module\n", "# typing module\n",
        "#!/usr/bin/python\n", "#!/usr/bin/env python3\n", "#!/usr/bin/env -S python3 -u\n",
        "#!/opt/interpreters/python3.13\r\n", "#!/usr/bin/pypy3\n", "#!/usr/bin/env python\n"};
    char *arguments[] = {"python"};
    bool ok = true;
    for (size_t i = 0; i < sizeof(paths) / sizeof(*paths); ++i) {
        string_t *root = test_checks_root();
        ok = test_checks_write(root, paths[i], contents[i]) && checks_policy(root, 1, arguments) == 1 && ok;
        ok = test_checks_finish(root) && ok;
    }
    TEST_ASSERT_TRUE(ok, "Python source, stub and window suffixes and direct/env/versioned interpreter headers fail");
}

static void test_policy_python_exclusions_and_prose(void)
{
    string_t *root = test_checks_root();
    const char *const excluded[] = {"build/generated.py", "thirdparty/imported.pyw", "vendor/imported.pyi",
        ".git/hooks/historic.py", "src/third_party/library.py", "tools/node_modules/package.py",
        "tools/__pycache__/cached.py", "vendor/script"};
    bool ok = true;
    for (size_t i = 0; i < sizeof(excluded) / sizeof(*excluded); ++i)
        ok = test_checks_write(root, excluded[i], "#!/usr/bin/python3\n") && ok;
    ok = test_checks_write(root, "docs/provenance.md", "Historically generated with Python.\n#!/usr/bin/python3\n") && ok;
    ok = test_checks_write(root, "NOTICE", "Historical Python licensing notice.\n") && ok;
    ok = test_checks_write(root, "tools/shell", "#!/bin/sh\necho Python\n") && ok;
    ok = test_checks_write(root, "tools/similar", "#!/usr/bin/python_helper\n") && ok;
    ok = test_checks_write(root, "tools/plain", "Python is mentioned in this ordinary text.\n") && ok;
    string_t *target_path = checks_path(root, "vendor/imported.pyi"), *link_path = checks_path(root, "linked.py");
    file_t *target = file_new(target_path), *link = file_new(link_path);
    ok = target && link && file_create_symlink(target, link) && ok;
    file_free(target);
    file_free(link);
    string_free(target_path);
    string_free(link_path);
    char *arguments[] = {"python"};
    ok = checks_policy(root, 1, arguments) == 0 && ok;
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "excluded trees, symlinks, ordinary text and non-Python shebangs remain permitted");
}

/* Register ordinary policy cases before any README workloads. */
void test_checks_policy_cases(void)
{
    TEST_RUN_IN_GROUP(test_policy_comments_and_literals, tests, NULL);
    TEST_RUN_IN_GROUP(test_policy_three_line_limit, tests, NULL);
    TEST_RUN_IN_GROUP(test_policy_prototypes_and_ordinary_functions, tests, NULL);
    TEST_RUN_IN_GROUP(test_policy_nested_braces, tests, NULL);
    TEST_RUN_IN_GROUP(test_policy_blank_and_comment_lines, tests, NULL);
    TEST_RUN_IN_GROUP(test_policy_concealed_inline_macros, tests, NULL);
    TEST_RUN_IN_GROUP(test_policy_preprocessor_branches, tests, NULL);
    TEST_RUN_IN_GROUP(test_policy_unterminated_inline, tests, NULL);
    TEST_RUN_IN_GROUP(test_policy_crlf_and_identifier_boundaries, tests, NULL);
    TEST_RUN_IN_GROUP(test_policy_inline_perfect_hash_and_columns, tests, NULL);
    TEST_RUN_IN_GROUP(test_policy_binding_perfect_hash, tests, NULL);
    TEST_RUN_IN_GROUP(test_policy_registry_rejects_corruption, tests, NULL);
    TEST_RUN_IN_GROUP(test_policy_project_inline_definitions, tests, NULL);
    TEST_RUN_IN_GROUP(test_policy_command_and_exclusions, tests, NULL);
    TEST_RUN_IN_GROUP(test_policy_python_sources_and_shebangs, tests, NULL);
    TEST_RUN_IN_GROUP(test_policy_python_exclusions_and_prose, tests, NULL);
}

/* Keep this README benchmark fixture after all ordinary native checks. */
void test_checks_policy_readme_cases(void)
{
    TEST_RUN_IN_GROUP(test_policy_readme_parser_benchmark, readme_examples, NULL);
}
