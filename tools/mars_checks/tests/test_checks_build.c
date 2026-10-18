/**
 * @file test_checks_build.c
 * @brief Native regressions for browser compiler dependency diagnostics.
 *
 * Copies the maintained Makefiles into a disposable tree and invokes their
 * dependency and TypeScript targets sequentially. Missing or unusable compilers
 * must fail before compilation and print usable installation instructions,
 * while native-only checks remain independent. The overall success message
 * must appear only after every dependency check has passed.
 * No application assets, repository settings or installed tools are modified.
 */
#include <stdio.h>
#include <string.h>

#include "file-t.h"
#include "checks_process.h"
#include "test_harness.h"
#include "test_checks.h"

static bool test_build_makefiles(const string_t *temporary)
{
    const char *const files[] = {
        "Makefile", "tools/mars_lab/Makefile", "tools/to_be_announced_lab/Makefile",
        "tools/mars_config/Makefile", "tools/mars_checks/Makefile"
    };
    string_t *root = checks_text(MARS_CHECKS_ROOT_DIR);
    bool ok = test_checks_write(temporary, "src/.keep", "") && test_checks_write(temporary, "tests/.keep", "");
    for (size_t i = 0; i < sizeof(files) / sizeof(*files); ++i) {
        string_t *path = checks_path(root, files[i]);
        string_t *source = checks_read(path);
        ok = source && test_checks_write(temporary, files[i], string_c_str(source)) && ok;
        string_free(source);
        string_free(path);
    }
    string_free(root);
    return ok;
}

static bool test_build_compiler(const string_t *root)
{
    bool ok = test_checks_write(root, "compiler", "#!/bin/sh\nprintf 'Version 7.0.2\\n'\n");
    string_t *path = checks_path(root, "compiler");
    file_t *file = file_new(path);
    ok = file && file_chmod(file, 0700) && ok;
    file_free(file);
    string_free(path);
    return ok;
}

static bool test_build_install_help(const string_t *text)
{
    const char *const instructions[] = {
        "node-typescript package requires Node.js",
        "make TSC=/absolute/path/to/tsc",
        "docs/building.md#native-typescript-compiler",
        "https://github.com/microsoft/typescript-go/releases/tag/typescript/v7.0.2"
    };
    bool ok = text != NULL;
    for (size_t i = 0; ok && i < sizeof(instructions) / sizeof(*instructions); ++i)
        ok = string_find(text, instructions[i]) >= 0;
    if (!ok)
        return false;
    bool download = string_find(text, "curl -fL ") >= 0;
    bool other_platform = string_find(text, "Select the archive matching your operating system") >= 0;
    if (!download)
        return other_platform;
    const char *const commands[] = {
        "sudo apt install ca-certificates curl tar",
        "ts_install=$(mktemp -d)",
        "| sha256sum -c -",
        "tar -xzf \"$ts_install/compiler.tgz\" -C \"$ts_install\"",
        "test ! -e \"$HOME/.local/lib/typescript/7.0.2\"",
        "ln -sT \"$HOME/.local/lib/typescript/7.0.2/lib/tsc\" \"$HOME/.local/bin/tsc\"",
        "export PATH=\"$HOME/.local/bin:$PATH\"",
        "make check-deps"
    };
    for (size_t i = 0; i < sizeof(commands) / sizeof(*commands); ++i)
        ok = string_find(text, commands[i]) >= 0 && ok;
    return ok && !other_platform;
}

static bool test_build_probe(const string_t *root, const char *target, const char *compiler,
                             const char *override, const char *diagnostic)
{
    checks_strings_t *arguments = checks_strings_new();
    checks_strings_add(arguments, checks_text("env"));
    const char *const inherited[] = {"LAB_TSC", "TBA_TSC", "MAKEFLAGS", "MFLAGS"};
    for (size_t i = 0; i < sizeof(inherited) / sizeof(*inherited); ++i) {
        checks_strings_add(arguments, checks_text("-u"));
        checks_strings_add(arguments, checks_text(inherited[i]));
    }
    checks_strings_add(arguments, checks_text("make"));
    checks_strings_add(arguments, checks_text("--no-print-directory"));
    checks_strings_add(arguments, checks_text("-j1"));
    checks_strings_add(arguments, checks_text(target));
    checks_strings_add(arguments, string_sprintf("TSC=%s", compiler));
    if (override)
        checks_strings_add(arguments, checks_text(override));
    string_t *output = NULL, *errors = NULL;
    int status = -1;
    bool ran = checks_process_run(arguments, root, 30, &output, &errors, &status);
    bool ok = ran && (diagnostic ? status > 0 && errors && string_find(errors, diagnostic) >= 0 : status == 0);
    const char *summary = "All dependency checks passed (C development libraries and native TypeScript compilers).\n";
    if (!diagnostic && strcmp(target, "check-deps") == 0)
        ok = output && string_ends_with(output, summary) &&
             string_find(output, "LAB_TSC: Version 7.0.2\n") >= 0 &&
             string_find(output, "TBA_TSC: Version 7.0.2\n") >= 0 && ok;
    else
        ok = (!output || string_find(output, summary) < 0) && ok;
    if (diagnostic)
        ok = test_build_install_help(errors) && ok;
    if (!ok)
        fprintf(stderr, "Build probe %s: status %d\n%s\n%s\n", target, status,
                output ? string_c_str(output) : "", errors ? string_c_str(errors) : "");
    string_free(output);
    string_free(errors);
    checks_strings_free(arguments);
    return ok;
}

static void test_checks_typescript_dependencies(void)
{
    string_t *root = test_checks_root();
    string_t *missing = checks_path(root, "missing compiler");
    bool ok = test_build_makefiles(root);
    const char *const targets[] = {
        "check-deps", "check-lab-deps", "lab-typescript", "lab-test-typescript", "lab-typecheck",
        "tba-typescript", "tba-test-typescript", "tba-typecheck"
    };
    for (size_t i = 0; ok && i < sizeof(targets) / sizeof(*targets); ++i)
        ok = test_build_probe(root, targets[i], string_c_str(missing), NULL, "Missing native TypeScript compiler");
    ok = test_build_probe(root, "check-native-deps", string_c_str(missing), NULL, NULL) && ok;
    ok = test_build_probe(root, "lab-typecheck", "/bin/false", NULL, "Unable to run native TypeScript compiler") && ok;
    ok = test_build_probe(root, "tba-typecheck", "cc", NULL, "requires the native TypeScript 7 compiler or newer") && ok;
    ok = test_build_probe(root, "lab-typecheck", "/bin/false", "LAB_TSC=/missing/lab-compiler",
                          "Missing native TypeScript compiler (LAB_TSC): /missing/lab-compiler") && ok;
    ok = test_build_probe(root, "tba-typecheck", "/bin/false", "TBA_TSC=/missing/second-compiler",
                          "Missing native TypeScript compiler (TBA_TSC): /missing/second-compiler") && ok;
    string_t *compiler = checks_path(root, "compiler");
    ok = test_build_compiler(root) && ok;
    ok = test_build_probe(root, "check-deps", string_c_str(compiler), "TBA_TSC=/missing/second-compiler",
                          "Missing native TypeScript compiler (TBA_TSC): /missing/second-compiler") && ok;
    ok = test_build_probe(root, "check-deps", string_c_str(compiler), NULL, NULL) && ok;
    string_free(compiler);
    string_free(missing);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "compiler checks reject unavailable tools, honour overrides and keep native builds independent");
}

/* Register compiler dependency checks before the README examples. */
void test_checks_build_cases(void)
{
    TEST_RUN_IN_GROUP(test_checks_typescript_dependencies, tests, "build,typescript");
}
