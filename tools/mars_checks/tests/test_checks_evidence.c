/**
 * @file test_checks_evidence.c
 * @brief Isolated native release evidence command regressions.
 *
 * Disposable trees contain shell probe fixtures, legal records and dependency
 * symlinks. Tests exercise schema, package parsing and atomic failure safety
 * without Python, a compiler or host package availability. PATH and CC are
 * restored before each assertion. The parent suite registers these sequentially.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>

#include "file.h"
#include "test_harness.h"
#include "checks_evidence.h"
#include "test_checks.h"

typedef struct evidence_fixture {
    string_t *root;
    string_t *path;
    string_t *cc;
} evidence_fixture_t;

static bool evidence_test_script(const string_t *root, const char *name, const char *script)
{
    bool ok = test_checks_write(root, name, script);
    string_t *path = checks_path(root, name);
    file_t *file = file_new(path);
    ok = ok && file && file_chmod(file, 0700);
    file_free(file);
    string_free(path);
    return ok;
}

static bool evidence_test_begin(evidence_fixture_t *fixture)
{
    fixture->root = test_checks_root();
    fixture->path = getenv("PATH") ? checks_text(getenv("PATH")) : NULL;
    fixture->cc = getenv("CC") ? checks_text(getenv("CC")) : NULL;
    bool ok = test_checks_write(fixture->root, "build/release/libmars.so", "abc") &&
              test_checks_write(fixture->root, "dependency-real.so", "abc") &&
              test_checks_write(fixture->root, "LICENSE", "abc") &&
              test_checks_write(fixture->root, "THIRD_PARTY_NOTICES.md", "abc") &&
              test_checks_write(fixture->root, "DEPENDENCIES.spdx", "abc") &&
              test_checks_write(fixture->root, "docs/compliance-status.md", "abc") &&
              test_checks_write(fixture->root, "git-state", "clean\n") &&
              test_checks_write(fixture->root, "ldd-state", "good\n") &&
              test_checks_write(fixture->root, "package-state", "good\n");
    ok = evidence_test_script(
             fixture->root, "bin/git",
             "#!/bin/sh\nIFS= read -r state < git-state\n"
             "[ \"$state\" = unknown ] && exit 1\n"
             "if [ \"$1\" = rev-parse ]; then\n"
             "  [ \"$state\" = badcommit ] && { printf 'invalid\\n'; exit 0; }\n"
             "  printf '0123456789012345678901234567890123456789\\n'\n"
             "else\n"
             "  [ \"$state\" = statusfail ] && exit 1\n"
             "  [ \"$state\" = dirty ] && printf ' M source.c\\n'\n"
             "fi\nexit 0\n") && ok;
    ok = evidence_test_script(
             fixture->root, "bin/ldd",
             "#!/bin/sh\nIFS= read -r state < ldd-state\n"
             "case \"$state\" in\n"
             " failed) exit 1;;\n"
             " missing) printf 'libmissing.so => not found\\n'; exit 0;;\n"
             " absent) printf 'libgone.so => /nonexistent/mars-library.so (0x1234)\\n'; exit 0;;\n"
             " malformed) printf 'unrecognised dependency output\\n'; exit 0;;\n"
             " empty) exit 0;;\n"
             " relative) printf 'libsample.so => dependency.so (0x1234)\\n'\n"
             "           printf 'libnormalised.so => ./build/../dependency.so (0x4321)\\n'; exit 0;;\n"
             "esac\n"
             "printf 'linux-vdso.so.1 (0x1234)\\n'\n"
             "printf 'libsample.so => %s/dependency.so (0x1234)\\n' \"$PWD\"\n"
             "printf '%s/dependency-real.so (0x4321)\\n' \"$PWD\"\n") && ok;
    ok = evidence_test_script(
             fixture->root, "bin/dpkg-query",
             "#!/bin/sh\nIFS= read -r state < package-state\n"
             "[ \"$state\" = unavailable ] && exit 1\n"
             "if [ \"$1\" = -S ]; then\n"
             "  printf 'mars-evidence-fixture:amd64: %s\\n' \"$2\"\n"
             "else\n"
             "  [ \"$state\" = fallback ] && exit 1\n"
             "  [ \"$state\" = malformed ] && { printf 'broken\\n'; exit 0; }\n"
             "  printf 'mars-evidence-fixture:amd64\\t1.2.3\\tamd64\\n'\n"
             "fi\n") && ok;
    ok = evidence_test_script(fixture->root, "bin/pkg-config",
                              "#!/bin/sh\n[ \"$1\" = --modversion ] && { printf '9.8.7\\n'; exit 0; }\n"
                              "printf '\\n  pkg-config fixture  \\nignored\\n' >&2\n") && ok;
    ok = evidence_test_script(fixture->root, "bin/compiler with spaces",
                              "#!/bin/sh\nprintf '\\n compiler fixture \\nsecond line\\n'\n") && ok;
    ok = evidence_test_script(fixture->root, "bin/python3",
                              "#!/bin/sh\nprintf 'invoked\\n' > python-was-run\nexit 1\n") && ok;
    string_t *target_path = checks_text("dependency-real.so");
    string_t *link_path = checks_path(fixture->root, "dependency.so");
    file_t *target = file_new(target_path), *link = file_new(link_path);
    ok = target && link && file_create_symlink(target, link) && ok;
    file_free(target);
    file_free(link);
    string_free(target_path);
    string_free(link_path);
    string_t *bin = checks_path(fixture->root, "bin");
    string_t *compiler = checks_path(bin, "compiler with spaces");
    ok = setenv("PATH", string_c_str(bin), 1) == 0 && setenv("CC", string_c_str(compiler), 1) == 0 && ok;
    string_free(compiler);
    string_free(bin);
    return ok;
}

static bool evidence_test_end(evidence_fixture_t *fixture)
{
    bool ok = fixture->path ? setenv("PATH", string_c_str(fixture->path), 1) == 0 : unsetenv("PATH") == 0;
    ok = (fixture->cc ? setenv("CC", string_c_str(fixture->cc), 1) == 0 : unsetenv("CC") == 0) && ok;
    string_free(fixture->path);
    string_free(fixture->cc);
    return test_checks_finish(fixture->root) && ok;
}

static bool evidence_test_member(const json_t *object, const char *key, const char *expected)
{
    const json_t *value = checks_member(object, key);
    return value && checks_equal(json_string_value(value), expected);
}

static json_t *evidence_test_report(const string_t *root)
{
    string_t *path = checks_path(root, "build/compliance/release-evidence.json");
    json_t *result = json_from_file(path);
    string_free(path);
    return result;
}

static bool evidence_test_preserved(const string_t *root)
{
    string_t *path = checks_path(root, "build/compliance/release-evidence.json");
    string_t *content = checks_read(path);
    bool ok = content && checks_equal(content, "previous evidence\n");
    string_free(content);
    string_free(path);
    return ok;
}

static bool evidence_test_clean_output_directory(const string_t *root)
{
    string_t *path = checks_path(root, "build/compliance");
    file_t *directory = file_new(path);
    bool ok = directory && file_open_directory(directory);
    size_t reports = 0;
    while (ok) {
        file_info_t *entry = NULL;
        ok = file_read_directory(directory, &entry);
        if (!ok || !entry) {
            file_info_free(entry);
            break;
        }
        string_t *name = checks_text(file_info_name(entry));
        if (!checks_equal(name, ".") && !checks_equal(name, "..")) {
            ok = checks_equal(name, "release-evidence.json") && file_info_type(entry) == FILE_TYPE_REGULAR;
            ++reports;
        }
        string_free(name);
        file_info_free(entry);
    }
    if (directory && file_is_open(directory))
        ok = file_close(directory) && ok;
    file_free(directory);
    string_free(path);
    return ok && reports == 1;
}

static void test_checks_evidence_complete_report(void)
{
    evidence_fixture_t fixture;
    bool ok = evidence_test_begin(&fixture);
    ok = checks_evidence(fixture.root, 0, NULL) == 0 && ok;
    json_t *report = evidence_test_report(fixture.root);
    const char *hash = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    const json_t *artefact = checks_member(report, "artefact");
    const json_t *libraries = checks_member(artefact, "dynamic_libraries");
    const json_t *library = json_array_get(libraries, 0);
    const json_t *package = checks_member(library, "system_package");
    const json_t *modules = checks_member(report, "pkg_config_modules");
    const json_t *tools = checks_member(report, "tools");
    string_t *resolved = checks_path(fixture.root, "dependency-real.so");
    bool dirty = true;
    ok = report && evidence_test_member(report, "schema", "MARS release dependency evidence 1") &&
         evidence_test_member(artefact, "sha256", hash) && json_array_size(libraries) == 2 &&
         evidence_test_member(library, "sha256", hash) &&
         evidence_test_member(library, "resolved_path", string_c_str(resolved)) &&
         evidence_test_member(package, "package", "mars-evidence-fixture:amd64") &&
         evidence_test_member(package, "version", "1.2.3") &&
         evidence_test_member(package, "architecture", "amd64") &&
         evidence_test_member(checks_member(report, "compliance_records"), "LICENSE", hash) &&
         evidence_test_member(tools, "cc", "compiler fixture") &&
         evidence_test_member(tools, "pkg-config", "pkg-config fixture") &&
         json_type(checks_member(tools, "latex")) == JSON_NULL &&
         !checks_member(tools, "python3") &&
         !checks_member(checks_member(report, "host"), "python") &&
         evidence_test_member(modules, "libcurl", "9.8.7") &&
         evidence_test_member(modules, "libzstd", "9.8.7") &&
         evidence_test_member(modules, "libsodium", "9.8.7") &&
         json_bool_value(checks_member(checks_member(report, "source"), "worktree_dirty"), &dirty) && !dirty && ok;
    string_t *python_marker = checks_path(fixture.root, "python-was-run");
    ok = !checks_is_file(python_marker) && evidence_test_clean_output_directory(fixture.root) && ok;
    string_free(python_marker);
    json_free(report);
    static const char *const records[] = {
        "LICENSE", "THIRD_PARTY_NOTICES.md", "DEPENDENCIES.spdx", "docs/compliance-status.md"};
    for (size_t i = 0; i < sizeof(records) / sizeof(*records); ++i) {
        string_t *record_path = checks_path(fixture.root, records[i]);
        string_t *target_path = string_sprintf("%s/legal-record-%zu", string_c_str(fixture.root), i);
        string_t *relative_path = string_sprintf("%slegal-record-%zu", i == 3 ? "../" : "", i);
        file_t *record = file_new(record_path), *target = file_new(target_path), *relative = file_new(relative_path);
        ok = record && target && relative && file_move(record, target, false) &&
             file_create_symlink(relative, record) && ok;
        file_free(record);
        file_free(target);
        file_free(relative);
        string_free(record_path);
        string_free(target_path);
        string_free(relative_path);
    }
    ok = test_checks_write(fixture.root, "ldd-state", "relative\n") && ok;
    ok = checks_evidence(fixture.root, 0, NULL) == 0 && ok;
    report = evidence_test_report(fixture.root);
    libraries = checks_member(checks_member(report, "artefact"), "dynamic_libraries");
    ok = report && json_array_size(libraries) == 2 && ok;
    for (size_t i = 0; i < json_array_size(libraries); ++i) {
        library = json_array_get(libraries, i);
        ok = evidence_test_member(library, "resolved_path", string_c_str(resolved)) &&
             evidence_test_member(library, "sha256", hash) &&
             evidence_test_member(checks_member(library, "system_package"), "version", "1.2.3") && ok;
    }
    const json_t *compliance = checks_member(report, "compliance_records");
    ok = json_object_size(compliance) == 4 && ok;
    for (size_t i = 0; i < sizeof(records) / sizeof(*records); ++i)
        ok = evidence_test_member(compliance, records[i], hash) && ok;
    char *protected_target[] = {"--output", "legal-record-0"};
    ok = checks_evidence(fixture.root, 2, protected_target) == 1 && ok;
    string_t *target_path = checks_path(fixture.root, "legal-record-0");
    string_t *content = checks_read(target_path);
    ok = content && checks_equal(content, "abc") && evidence_test_clean_output_directory(fixture.root) && ok;
    string_free(content);
    string_free(target_path);
    string_free(resolved);
    json_free(report);
    ok = evidence_test_end(&fixture) && ok;
    TEST_ASSERT_TRUE(ok, "release evidence retains schema, hashes, package metadata and native dependency versions");
}

static void test_checks_evidence_git_failure_safety(void)
{
    evidence_fixture_t fixture;
    bool ok = evidence_test_begin(&fixture);
    char *allow[] = {"--allow-dirty"};
    static const char *const states[] = {"dirty\n", "unknown\n", "badcommit\n", "statusfail\n"};
    ok = test_checks_write(fixture.root, "build/compliance/release-evidence.json", "previous evidence\n") && ok;
    for (size_t i = 0; i < sizeof(states) / sizeof(*states); ++i) {
        ok = test_checks_write(fixture.root, "git-state", states[i]) && ok;
        ok = checks_evidence(fixture.root, 0, NULL) == 1 && evidence_test_preserved(fixture.root) && ok;
        if (i)
            ok = checks_evidence(fixture.root, 1, allow) == 1 && evidence_test_preserved(fixture.root) && ok;
    }
    ok = test_checks_write(fixture.root, "git-state", "dirty\n") && ok;
    ok = checks_evidence(fixture.root, 1, allow) == 0 && ok;
    json_t *report = evidence_test_report(fixture.root);
    bool dirty = false;
    ok = json_bool_value(checks_member(checks_member(report, "source"), "worktree_dirty"), &dirty) && dirty && ok;
    json_free(report);
    ok = evidence_test_end(&fixture) && ok;
    TEST_ASSERT_TRUE(ok, "unknown Git state fails closed even with allow-dirty; failed reports preserve old output");
}

static void test_checks_evidence_dependency_failure_safety(void)
{
    evidence_fixture_t fixture;
    bool ok = evidence_test_begin(&fixture);
    static const char *const states[] = {"failed\n", "missing\n", "absent\n", "malformed\n", "empty\n"};
    ok = test_checks_write(fixture.root, "build/compliance/release-evidence.json", "previous evidence\n") && ok;
    for (size_t i = 0; i < sizeof(states) / sizeof(*states); ++i) {
        ok = test_checks_write(fixture.root, "ldd-state", states[i]) && ok;
        ok = checks_evidence(fixture.root, 0, NULL) == 1 && evidence_test_preserved(fixture.root) && ok;
    }
    ok = test_checks_write(fixture.root, "ldd-state", "good\n") && ok;
    string_t *licence = checks_path(fixture.root, "LICENSE");
    file_t *file = file_new(licence);
    ok = file && file_delete(file) && ok;
    ok = checks_evidence(fixture.root, 0, NULL) == 1 && evidence_test_preserved(fixture.root) && ok;
    file_t *missing_target = file_new_cstr("missing-licence-target");
    ok = missing_target && file_create_symlink(missing_target, file) && ok;
    file_free(missing_target);
    file_free(file);
    string_free(licence);
    ok = checks_evidence(fixture.root, 0, NULL) == 1 && evidence_test_preserved(fixture.root) && ok;
    ok = evidence_test_end(&fixture) && ok;
    TEST_ASSERT_TRUE(ok, "failed dependencies and absent or dangling legal records never publish evidence");
}

static void test_checks_evidence_optional_packages(void)
{
    evidence_fixture_t fixture;
    bool ok = evidence_test_begin(&fixture);
    static const char *const states[] = {"unavailable\n", "fallback\n", "malformed\n"};
    for (size_t i = 0; i < sizeof(states) / sizeof(*states); ++i) {
        ok = test_checks_write(fixture.root, "package-state", states[i]) && ok;
        ok = checks_evidence(fixture.root, 0, NULL) == 0 && ok;
        json_t *report = evidence_test_report(fixture.root);
        const json_t *libraries = checks_member(checks_member(report, "artefact"), "dynamic_libraries");
        const json_t *package = checks_member(json_array_get(libraries, 0), "system_package");
        ok = package && (i == 0 ? json_type(package) == JSON_NULL
                               : evidence_test_member(package, "package", "mars-evidence-fixture:amd64") &&
                                     json_object_size(package) == 1) && ok;
        json_free(report);
    }
    ok = evidence_test_end(&fixture) && ok;
    TEST_ASSERT_TRUE(ok, "optional package probes tolerate absent commands and unavailable or malformed details");
}

static void test_checks_evidence_arguments_and_publication(void)
{
    evidence_fixture_t fixture;
    bool ok = evidence_test_begin(&fixture);
    char *missing[] = {"--library"};
    char *unknown[] = {"--unknown"};
    char *absent[] = {"--library", "no such library"};
    char *custom[] = {"--library", "build/release/libmars.so", "--output", "custom dir/report.json"};
    char *blocked[] = {"--output", "blocked/report.json"};
    char *directory[] = {"--output=existing-directory"};
    static const char *const options[] = {"--library", "--output"};
    static const char *const following[] = {"--allow-dirty", "--library", "--output", "--unknown", "-x"};
    for (size_t i = 0; i < sizeof(options) / sizeof(*options); ++i) {
        char *no_value[] = {(char *)options[i]};
        ok = checks_evidence(fixture.root, 1, no_value) == 2 && ok;
        for (size_t j = 0; j < sizeof(following) / sizeof(*following); ++j) {
            char *arguments[] = {(char *)options[i], (char *)following[j]};
            ok = checks_evidence(fixture.root, 2, arguments) == 2 && ok;
        }
    }
    string_t *option_path = checks_path(fixture.root, "--allow-dirty");
    ok = !checks_is_file(option_path) && ok;
    string_free(option_path);
    char *literal_option[] = {"--output=--allow-dirty"};
    ok = checks_evidence(fixture.root, 1, literal_option) == 0 && ok;
    option_path = checks_path(fixture.root, "--allow-dirty");
    json_t *literal_report = json_from_file(option_path);
    ok = literal_report && ok;
    json_free(literal_report);
    string_free(option_path);
    ok = checks_evidence(fixture.root, 1, missing) == 2 && checks_evidence(fixture.root, 1, unknown) == 2 &&
         checks_evidence(fixture.root, 2, absent) == 1 && checks_evidence(fixture.root, 4, custom) == 0 && ok;
    string_t *custom_path = checks_path(fixture.root, "custom dir/report.json");
    json_t *report = json_from_file(custom_path);
    ok = report && ok;
    json_free(report);
    string_free(custom_path);
    ok = test_checks_write(fixture.root, "blocked", "keep this file") && ok;
    ok = checks_evidence(fixture.root, 2, blocked) == 1 && ok;
    string_t *blocked_path = checks_path(fixture.root, "blocked");
    string_t *content = checks_read(blocked_path);
    ok = content && checks_equal(content, "keep this file") && ok;
    string_free(content);
    string_free(blocked_path);
    string_t *directory_path = checks_path(fixture.root, "existing-directory");
    ok = checks_mkdir(directory_path) && test_checks_write(fixture.root, "existing-directory/keep", "retained") && ok;
    ok = checks_evidence(fixture.root, 1, directory) == 1 && ok;
    string_t *retained = checks_path(directory_path, "keep");
    content = checks_read(retained);
    ok = content && checks_equal(content, "retained") && ok;
    string_free(content);
    string_free(retained);
    string_free(directory_path);
    ok = evidence_test_end(&fixture) && ok;
    TEST_ASSERT_TRUE(ok, "arguments, custom paths with spaces and publication failures preserve unrelated files");
}

static void test_checks_evidence_output_aliases(void)
{
    evidence_fixture_t fixture;
    bool ok = evidence_test_begin(&fixture);
    static const char *const inputs[] = {
        "build/release/libmars.so", "LICENSE", "THIRD_PARTY_NOTICES.md", "DEPENDENCIES.spdx",
        "docs/compliance-status.md"};
    string_t *alias_path = checks_path(fixture.root, "output-alias");
    file_t *alias = file_new(alias_path);
    for (size_t i = 0; i < sizeof(inputs) / sizeof(*inputs); ++i) {
        string_t *input_path = checks_path(fixture.root, inputs[i]);
        file_t *input = file_new(input_path);
        char *same[] = {"--output", (char *)string_c_str(input_path)};
        char *linked[] = {"--output", "output-alias"};
        ok = checks_evidence(fixture.root, 2, same) == 1 && ok;
        ok = file_create_hard_link(input, alias) && ok;
        ok = checks_evidence(fixture.root, 2, linked) == 1 && ok;
        file_info_t *info = file_get_info(alias);
        ok = info && file_info_type(info) == FILE_TYPE_REGULAR && file_info_link_count(info) == 2 && ok;
        file_info_free(info);
        ok = file_delete(alias) && ok;
        ok = file_create_symlink(input, alias) && ok;
        ok = checks_evidence(fixture.root, 2, linked) == 1 && ok;
        info = file_get_info(alias);
        ok = info && file_info_type(info) == FILE_TYPE_SYMLINK && ok;
        file_info_free(info);
        ok = file_delete(alias) && ok;
        string_t *content = checks_read(input_path);
        ok = content && checks_equal(content, "abc") && ok;
        string_free(content);
        file_free(input);
        string_free(input_path);
    }
    char *normalised[] = {"--output", "build/release/../release/libmars.so"};
    ok = checks_evidence(fixture.root, 2, normalised) == 1 && ok;
    string_t *build_path = checks_path(fixture.root, "build");
    file_t *build = file_new(build_path);
    ok = file_create_symlink(build, alias) && ok;
    char *parent_link[] = {"--output", "output-alias/release/libmars.so"};
    ok = checks_evidence(fixture.root, 2, parent_link) == 1 && ok;
    ok = file_delete(alias) && ok;
    file_free(build);
    string_free(build_path);
    file_free(alias);
    string_free(alias_path);
    ok = evidence_test_end(&fixture) && ok;
    TEST_ASSERT_TRUE(ok, "output refuses direct, normalised, hard-link and symbolic-link aliases of mandatory inputs");
}

/* Register native release evidence cases before the suite's README examples. */
void test_checks_evidence_cases(void)
{
    TEST_RUN_IN_GROUP(test_checks_evidence_complete_report, tests, NULL);
    TEST_RUN_IN_GROUP(test_checks_evidence_git_failure_safety, tests, NULL);
    TEST_RUN_IN_GROUP(test_checks_evidence_dependency_failure_safety, tests, NULL);
    TEST_RUN_IN_GROUP(test_checks_evidence_optional_packages, tests, NULL);
    TEST_RUN_IN_GROUP(test_checks_evidence_arguments_and_publication, tests, NULL);
    TEST_RUN_IN_GROUP(test_checks_evidence_output_aliases, tests, NULL);
}
