/**
 * @file checks_policy.c
 * @brief Repository traversal and command dispatch for native source policies.
 *
 * Enumerates project-owned C sources through file.h, pruning generated and
 * third-party directories before descent and ignoring symlinks. The source-policy
 * command can select inline, functiontables or python audits, or run all by default.
 * Python source suffixes and extensionless interpreter shebangs are rejected;
 * historical prose and provenance records are not interpreted as executable code.
 * Diagnostics retain repository-relative filenames and deterministic order.
 */
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "file.h"
#include "checks_policy.h"

static bool policy_excluded(const char *name)
{
    /* A fixed directory policy; no lookup cost grows with the repository. */
    return !strcmp(name, "build") || !strcmp(name, "vendor") || !strcmp(name, "third_party") ||
           !strcmp(name, "thirdparty") || !strcmp(name, "node_modules") || !strcmp(name, "__pycache__") ||
           !strcmp(name, ".git") || !strcmp(name, ".codex") || !strcmp(name, ".agents");
}

static bool policy_collect(const string_t *root, const char *relative, bool python, checks_strings_t *paths)
{
    string_t *path = checks_path(root, relative);
    file_t *directory = file_new(path);
    string_free(path);
    if (!directory || !file_open_directory(directory)) {
        bool absent = *relative && directory && file_last_error(directory) == ENOENT;
        if (!absent)
            fprintf(stderr, "%s: cannot list source directory\n", relative);
        file_free(directory);
        return absent;
    }
    bool ok = true;
    file_info_t *entry = NULL;
    while (file_read_directory(directory, &entry) && entry) {
        const char *name = file_info_name(entry);
        string_t *child = *relative ? string_sprintf("%s/%s", relative, name) : checks_text(name);
        if (!child)
            checks_fatal("allocating policy path");
        file_type_t type = file_info_type(entry);
        if (type == FILE_TYPE_DIRECTORY && !policy_excluded(name))
            ok = policy_collect(root, string_c_str(child), python, paths) && ok;
        else if (type == FILE_TYPE_REGULAR &&
                 (python ? (string_ends_with(child, ".py") || string_ends_with(child, ".pyw") ||
                            string_ends_with(child, ".pyi") || !strchr(name + (name[0] == '.'), '.')) :
                           (string_ends_with(child, ".c") || string_ends_with(child, ".h")))) {
            checks_strings_add(paths, child);
            child = NULL;
        }
        string_free(child);
        file_info_free(entry);
        entry = NULL;
    }
    if (file_last_error(directory)) {
        fprintf(stderr, "%s: cannot read source directory\n", relative);
        ok = false;
    }
    file_free(directory);
    return ok;
}

static bool policy_python_file(const string_t *root, const string_t *relative)
{
    if (string_ends_with(relative, ".py") || string_ends_with(relative, ".pyw") || string_ends_with(relative, ".pyi")) {
        fprintf(stderr, "%s: Python source is not permitted\n", string_c_str(relative));
        return false;
    }
    string_t *path = checks_path(root, string_c_str(relative));
    file_t *file = file_new(path);
    string_free(path);
    unsigned char prefix[256];
    size_t count = 0;
    bool ok = file && file_open_read(file) && file_read(file, prefix, sizeof(prefix), &count);
    file_free(file);
    if (!ok) {
        fprintf(stderr, "%s: cannot inspect interpreter header\n", string_c_str(relative));
        return false;
    }
    if (count < 2 || prefix[0] != '#' || prefix[1] != '!')
        return true;
    /* Linux inspects at most 256 bytes for a shebang. Do not decode unrelated binary payloads. */
    string_t *line = checks_text("");
    for (size_t i = 2; i < count && prefix[i] && prefix[i] != '\n'; ++i)
        string_append_char(line, prefix[i] < 128 ? prefix[i] : '?');
    bool python = checks_match(line,
        "(^|[[:space:]/])(pythonw?|pypy)([0-9]+([.][0-9]+)*)?([[:space:]]|$)", 0, NULL);
    string_free(line);
    if (python)
        fprintf(stderr, "%s: Python interpreter shebang is not permitted\n", string_c_str(relative));
    return !python;
}

static bool policy_audit_file(const string_t *root, const char *relative, bool table, bool binding)
{
    string_t *path = checks_path(root, relative), *source = checks_read(path);
    string_free(path);
    if (!source) {
        fprintf(stderr, "%s: cannot read source\n", relative);
        return false;
    }
    checks_strings_t *errors = checks_strings_new();
    if (table)
        checks_policy_function_table(source, binding, errors);
    else
        checks_policy_inline(source, errors);
    bool ok = !checks_strings_count(errors);
    for (size_t i = 0; i < checks_strings_count(errors); ++i)
        fprintf(stderr, "%s: %s\n", relative, string_c_str(checks_strings_get(errors, i)));
    checks_strings_free(errors);
    string_free(source);
    return ok;
}

/* Run the requested source audits without invoking compilers or interpreters. */
int checks_policy(const string_t *root, int argc, char **argv)
{
    bool inline_policy = argc == 0, tables = argc == 0, python = argc == 0;
    for (int i = 0; i < argc; ++i) {
        if (!strcmp(argv[i], "inline") || !strcmp(argv[i], "--inline"))
            inline_policy = true;
        else if (!strcmp(argv[i], "functiontables") || !strcmp(argv[i], "--functiontables"))
            tables = true;
        else if (!strcmp(argv[i], "python") || !strcmp(argv[i], "--python"))
            python = true;
        else {
            fprintf(stderr, "source-policy: expected inline, functiontables or python, got %s\n", argv[i]);
            return 2;
        }
    }
    bool ok = true;
    if (inline_policy) {
        const char *const directories[] = {"include", "src", "tests", "scratch", "bench", "tools"};
        checks_strings_t *paths = checks_strings_new();
        for (size_t i = 0; i < sizeof(directories) / sizeof(*directories); ++i)
            ok = policy_collect(root, directories[i], false, paths) && ok;
        checks_strings_sort(paths);
        if (!checks_strings_count(paths)) {
            fprintf(stderr, "source-policy: no project-owned C sources found\n");
            ok = false;
        }
        for (size_t i = 0; i < checks_strings_count(paths); ++i)
            ok = policy_audit_file(root, string_c_str(checks_strings_get(paths, i)), false, false) && ok;
        checks_strings_free(paths);
    }
    if (tables) {
        ok = policy_audit_file(root, "src/expression/expr_stringin.c", true, false) && ok;
        ok = policy_audit_file(root, "src/expression/expr_bindings.c", true, true) && ok;
    }
    if (python) {
        checks_strings_t *paths = checks_strings_new();
        ok = policy_collect(root, "", true, paths) && ok;
        checks_strings_sort(paths);
        for (size_t i = 0; i < checks_strings_count(paths); ++i)
            ok = policy_python_file(root, checks_strings_get(paths, i)) && ok;
        checks_strings_free(paths);
    }
    return ok ? 0 : 1;
}
