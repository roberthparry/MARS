/**
 * @file test_checks_build.c
 * @brief Native regressions for database, HTTP transport and browser compiler dependency diagnostics.
 *
 * Copies the maintained Makefiles into a disposable tree and invokes their
 * dependency and TypeScript targets sequentially. SQLCipher runtime probes reject old libraries and absent
 * search features before release or database-update recipes can run. Missing or unusable compilers
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
        "tools/mars_config/Makefile", "tools/mars_checks/Makefile", "tools/mars_checks/sqlcipher_probe.c",
        "tools/mars_checks/curl_probe.c", "tools/mars_checks/tests/fixtures/checks_curl_fixture.c",
        "tools/mars_checks/tests/fixtures/checks_sqlcipher_fixture.c",
        "docs/building.md",
        "include/array-t.h", "include/string-t.h", "src/array/array.c", "src/array/array_mutex.h"
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
    const char *summary =
        "All dependency checks passed (C development libraries, native TypeScript compilers and WebAssembly runtime).\n";
    if (!diagnostic && strcmp(target, "check-deps") == 0)
        ok = output && string_ends_with(output, summary) &&
             string_find(output, "LAB_TSC: Version 7.0.2\n") >= 0 &&
             string_find(output, "TBA_TSC: Version 7.0.2\n") >= 0 &&
             string_find(output, "MARS Lab WebAssembly compile/link probe passed.\n") >= 0 && ok;
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

static bool test_build_sqlcipher_probe(const string_t *root, const char *target, const char *mode,
                                      const char *diagnostic, string_t **instructions, bool quiet)
{
    checks_strings_t *arguments = checks_strings_new();
    checks_strings_add(arguments, checks_text("env"));
    const char *const inherited[] = {"MAKEFLAGS", "MFLAGS", "MAKEOVERRIDES", "MAKEFILES"};
    for (size_t i = 0; i < sizeof inherited / sizeof *inherited; ++i) {
        checks_strings_add(arguments, checks_text("-u"));
        checks_strings_add(arguments, checks_text(inherited[i]));
    }
    checks_strings_add(arguments, string_sprintf("MARS_TEST_SQLCIPHER=%s", mode));
    checks_strings_add(arguments, checks_text("make"));
    checks_strings_add(arguments, checks_text("--no-print-directory"));
    checks_strings_add(arguments, checks_text("-j1"));
    if (quiet) checks_strings_add(arguments, checks_text("SQLCIPHER_REPORT_LEVEL=outer-release"));
    checks_strings_add(arguments, checks_text(target));
    checks_strings_add(arguments, checks_text("MARS_LAB_LAUNCHER=/bin/true"));
    checks_strings_add(arguments, checks_text(
        "LDFLAGS=tools/mars_checks/tests/fixtures/checks_sqlcipher_fixture.c "
        "-Wl,--wrap=sqlite3_libversion_number -Wl,--wrap=sqlite3_libversion "
        "-Wl,--wrap=sqlite3_exec -Wl,--wrap=sqlite3_prepare_v2"));
    string_t *output = NULL, *errors = NULL;
    int status = -1;
    bool ran = checks_process_run(arguments, root, 30, &output, &errors, &status);
    bool ok = ran && output && errors && string_find(output, "SQLCipher ") < 0;
    if (!quiet || diagnostic)
        ok = ok && string_find(errors, "SQLCipher ") >= 0 && string_find(errors, "SQLite runtime ") >= 0 &&
             string_find(errors, "SQLite headers ") >= 0;
    if (diagnostic)
        ok = status != 0 && errors && string_find(errors, diagnostic) >= 0 &&
             string_find(errors, "Updating the separate sqlite3 package") >= 0 &&
             string_find(errors, "sudo apt install build-essential git pkg-config libssl-dev tcl-dev") >= 0 &&
             string_find(errors, "--branch v4.13.0") >= 0 &&
             string_find(errors, "--enable-fts5 --enable-rtree") >= 0 &&
             string_find(errors, "make check-native-deps") >= 0 &&
             string_find(errors, "SQLCipher version and required SQL features verified.") < 0 &&
             string_find(output, "check-compliance") < 0 && string_find(output, "Updating jurisdiction") < 0 && ok;
    else if (quiet)
        ok = status == 0 && !string_byte_length(errors) && ok;
    else
        ok = status == 0 && string_find(errors, "SQLCipher version and required SQL features verified.") >= 0 && ok;
    if (!ok)
        fprintf(stderr, "SQLCipher dependency probe %s/%s: status %d\n%s\n%s\n", target, mode, status,
                output ? string_c_str(output) : "", errors ? string_c_str(errors) : "");
    string_free(output);
    if (instructions) {
        *instructions = errors;
        errors = NULL;
    }
    string_free(errors);
    checks_strings_free(arguments);
    return ok;
}

static void test_checks_sqlcipher_dependencies(void)
{
    string_t *root = test_checks_root();
    bool ok = test_build_makefiles(root);
    static const struct { const char *target, *mode, *diagnostic; bool quiet; } cases[] = {
        {"check-sqlcipher-deps", "current", NULL, false},
        {"check-sqlcipher-deps", "old", "requires SQLite 3.27.0 or newer", false},
        {"check-sqlcipher-deps", "cipher", "requires SQLCipher 4.1.0 or newer", false},
        {"check-native-deps", "fts5", "required FTS5 with Unicode accent removal", false},
        {"check-deps", "window", "required SQLite window functions", false},
        {"release", "fts5", "required FTS5 with Unicode accent removal", false},
        {"release", "old", "requires SQLite 3.27.0 or newer", false},
        {"release", "cipher", "requires SQLCipher 4.1.0 or newer", false},
        {"release", "window", "required SQLite window functions", false},
        {"update-mars-db", "fts5", "required FTS5 with Unicode accent removal", false},
        {"install-jurisdiction-db", "fts5", "required FTS5 with Unicode accent removal", false},
        {"check-sqlcipher-deps", "current", NULL, true},
        {"check-sqlcipher-deps", "old", "requires SQLite 3.27.0 or newer", true},
        {"check-sqlcipher-deps", "fts5", "required FTS5 with Unicode accent removal", true}
    };
    for (size_t i = 0; ok && i < sizeof cases / sizeof *cases; ++i)
        ok = test_build_sqlcipher_probe(root, cases[i].target, cases[i].mode, cases[i].diagnostic, NULL, cases[i].quiet);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "dependency checks report linked versions and stop unsupported SQLCipher before database work");
}

static bool test_build_curl_probe(const string_t *root, const char *target, const char *mode,
                                 const char *diagnostic, string_t **instructions)
{
    checks_strings_t *arguments = checks_strings_new();
    checks_strings_add(arguments, checks_text("env"));
    const char *const inherited[] = {"MAKEFLAGS", "MFLAGS", "MAKEOVERRIDES", "MAKEFILES"};
    for (size_t i = 0; i < sizeof inherited / sizeof *inherited; ++i) {
        checks_strings_add(arguments, checks_text("-u"));
        checks_strings_add(arguments, checks_text(inherited[i]));
    }
    checks_strings_add(arguments, string_sprintf("MARS_TEST_CURL=%s", mode));
    checks_strings_add(arguments, checks_text("make"));
    checks_strings_add(arguments, checks_text("--no-print-directory"));
    checks_strings_add(arguments, checks_text("-j1"));
    checks_strings_add(arguments, checks_text(target));
    checks_strings_add(arguments, checks_text("MARS_LAB_LAUNCHER=/bin/true"));
    checks_strings_add(arguments, checks_text(
        "CURL_LIBS=$(shell pkg-config --libs libcurl 2>/dev/null || printf '%s' -lcurl) "
        "tools/mars_checks/tests/fixtures/checks_curl_fixture.c "
        "-Wl,--wrap=curl_version_info -Wl,--wrap=curl_url_set"));
    string_t *output = NULL, *errors = NULL;
    int status = -1;
    bool ran = checks_process_run(arguments, root, 30, &output, &errors, &status);
    bool help = !strcmp(target, "curl-install-help");
    bool ok = ran && output && errors;
    if (diagnostic)
        ok = ok && status != 0 && string_find(errors, diagnostic) >= 0 &&
            string_find(errors, "libcurl runtime ") >= 0 && string_find(errors, "; headers ") >= 0 &&
            string_find(errors, "--enable-websockets") >= 0 && string_find(errors, "make check-curl-deps") >= 0 &&
            string_find(errors, "Updating the curl command alone") >= 0 &&
            string_find(output, "check-compliance") < 0 && string_find(output, "Updating jurisdiction") < 0;
    else
        ok = ok && status == 0 && !string_byte_length(errors) &&
            (help ? string_find(output, "--enable-websockets") >= 0 : !string_byte_length(output));
    if (!ok)
        fprintf(stderr, "libcurl dependency probe %s/%s: status %d\n%s\n%s\n", target, mode, status,
                output ? string_c_str(output) : "", errors ? string_c_str(errors) : "");
    if (instructions) {
        *instructions = help ? output : errors;
        if (help) output = NULL;
        else errors = NULL;
    }
    string_free(output);
    string_free(errors);
    checks_strings_free(arguments);
    return ok;
}

static void test_checks_curl_dependencies(void)
{
    string_t *root = test_checks_root();
    bool ok = test_build_makefiles(root);
    static const struct { const char *target, *mode, *diagnostic; } cases[] = {
        {"check-curl-deps", "current", NULL},
        {"check-curl-deps", "old", "libcurl runtime >= 7.86.0"},
        {"check-curl-deps", "no-ws", "required ws protocol support"},
        {"check-curl-deps", "no-wss", "required wss protocol support"},
        {"check-curl-deps", "no-tls", "required TLS support"},
        {"check-curl-deps", "no-threads", "required thread-safe global initialisation support"},
        {"check-curl-deps", "no-http2", "required HTTP/2 support"},
        {"check-curl-deps", "url", "cannot configure WS/WSS transport"},
        {"check-native-deps", "no-ws", "required ws protocol support"},
        {"release", "no-wss", "required wss protocol support"}
    };
    for (size_t i = 0; ok && i < sizeof cases / sizeof *cases; ++i)
        ok = test_build_curl_probe(root, cases[i].target, cases[i].mode, cases[i].diagnostic, NULL);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "dependency checks reject missing linked transport features before compilation or release");
}

static bool test_build_documented_commands(const string_t *root, const string_t *instructions, const char *heading)
{
    string_t *path = checks_path(root, "docs/building.md"), *guide = checks_read(path);
    const char *text = guide ? string_c_str(guide) : "";
    const char *section = strstr(text, heading);
    const char *end = section ? strstr(section, "\n### ") : NULL;
    const char *next = section;
    bool ok = section && end && instructions;
    string_t *commands = checks_text("");
    /* README example: verify all three repair command blocks, without running sudo or modifying the host. */
    for (unsigned block = 0; ok && block < 3; ++block) {
        const char *begin = strstr(next, "```sh\n");
        const char *close = begin ? strstr(begin + 6, "\n```") : NULL;
        ok = begin && close && begin < end && close < end;
        if (!ok)
            break;
        begin += 6;
        string_t *example = checks_slice(guide, (size_t)(begin - text), (size_t)(close - begin + 1));
        ok = example && instructions && string_find(instructions, string_c_str(example)) >= 0;
        if (!ok)
            fprintf(stderr, "Documented commands differ in %s block %u:\n%s\n", heading, block + 1,
                    example ? string_c_str(example) : "cannot read command block");
        if (ok) checks_append(commands, example);
        string_free(example);
        next = close + 4;
    }
    if (ok) {
        ok = commands && test_checks_write(root, "upgrade-commands.sh", string_c_str(commands));
        checks_strings_t *arguments = checks_strings_new();
        checks_strings_add(arguments, checks_text("bash"));
        checks_strings_add(arguments, checks_text("-n"));
        checks_strings_add(arguments, checks_text("upgrade-commands.sh"));
        string_t *output = NULL, *errors = NULL;
        int status = -1;
        ok = checks_process_run(arguments, root, 30, &output, &errors, &status) && status == 0 && ok;
        string_free(errors);
        string_free(output);
        checks_strings_free(arguments);
    }
    string_free(commands);
    string_free(guide);
    string_free(path);
    return ok;
}

static void test_checks_sqlcipher_readme(void)
{
    string_t *root = test_checks_root(), *instructions = NULL;
    bool ok = test_build_makefiles(root) && test_build_sqlcipher_probe(root, "release", "fts5",
        "required FTS5 with Unicode accent removal", &instructions, false);
    ok = test_build_documented_commands(root, instructions, "#### Updating SQLCipher on Debian or Ubuntu\n") && ok;
    string_free(instructions);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "README SQLCipher repair commands match release diagnostics and parse as shell input");
}

static void test_checks_curl_readme(void)
{
    string_t *root = test_checks_root(), *instructions = NULL;
    bool ok = test_build_makefiles(root) && test_build_curl_probe(root, "curl-install-help", "current", NULL,
                                                                &instructions);
    ok = test_build_documented_commands(root, instructions, "#### Updating libcurl on Debian or Ubuntu\n") && ok;
    string_free(instructions);
    ok = test_checks_finish(root) && ok;
    TEST_ASSERT_TRUE(ok, "README libcurl repair commands match printed help and parse without modifying the host");
}

/* Register database and compiler dependency checks before the README examples. */
void test_checks_build_cases(void)
{
    TEST_RUN_IN_GROUP(test_checks_sqlcipher_dependencies, tests, "build,sqlcipher");
    TEST_RUN_IN_GROUP(test_checks_curl_dependencies, tests, "build,http");
    TEST_RUN_IN_GROUP(test_checks_typescript_dependencies, tests, "build,typescript");
}

/* Check the documented repair commands after ordinary dependency regressions. */
void test_checks_build_readme_cases(void)
{
    TEST_RUN_IN_GROUP(test_checks_sqlcipher_readme, readme_examples, "build,sqlcipher");
    TEST_RUN_IN_GROUP(test_checks_curl_readme, readme_examples, "build,http");
}
