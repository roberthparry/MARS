/**
 * @file evidence_probes.c
 * @brief Shell-free release probes and compliance hashing.
 *
 * Implements native Git validation, optional tool and pkg-config discovery,
 * and mandatory file digests without invoking an interpreter. All returned
 * strings and JSON trees are owned.
 */
#include <stdlib.h>

#include "evidence_private.h"

void evidence_put(json_t *object, const char *key, json_t *value)
{
    string_t *name = checks_text(key);
    bool ok = value && json_object_set(object, name, value);
    string_free(name);
    json_free(value);
    if (!ok)
        checks_fatal("cannot allocate release evidence");
}

void evidence_text(json_t *object, const char *key, const string_t *value)
{
    evidence_put(object, key, value ? json_new_string(value) : json_new_null());
}

string_t *evidence_run(const string_t *root, const char *const *argv, bool include_errors)
{
    checks_strings_t *arguments = checks_strings_new();
    for (size_t i = 0; argv[i]; ++i)
        checks_strings_add(arguments, checks_text(argv[i]));
    string_t *output = NULL, *errors = NULL;
    int status = -1;
    bool ok = checks_process_run(arguments, root, 30, &output, &errors, &status) && status == 0;
    checks_strings_free(arguments);
    if (ok && include_errors && errors) {
        string_append_char(output, '\n');
        checks_append(output, errors);
    }
    string_free(errors);
    if (!ok) {
        string_free(output);
        return NULL;
    }
    return output;
}

string_t *evidence_resolve(const string_t *path)
{
    file_t *source = file_new(path);
    file_t *resolved = source ? file_resolve(source) : NULL;
    string_t *result = resolved ? checks_text(file_path(resolved)) : NULL;
    file_free(resolved);
    file_free(source);
    return result;
}

string_t *evidence_hash(const string_t *path)
{
    file_t *source = file_new(path);
    string_t *result = source ? file_sha256(source) : NULL;
    file_free(source);
    return result;
}

json_t *evidence_source(const string_t *root, bool allow_dirty)
{
    const char *const revision[] = {"git", "rev-parse", "--verify", "HEAD", NULL};
    const char *const state[] = {"git", "status", "--porcelain", "--untracked-files=normal", NULL};
    string_t *commit = evidence_run(root, revision, false);
    string_t *status = evidence_run(root, state, false);
    if (commit)
        string_trim(commit);
    size_t length = commit ? string_byte_length(commit) : 0;
    bool valid = commit && status && (length == 40 || length == 64);
    for (size_t i = 0; valid && i < length; ++i) {
        unsigned char byte = checks_byte(commit, i);
        valid = (byte >= '0' && byte <= '9') || (byte >= 'a' && byte <= 'f');
    }
    bool dirty = status && string_byte_length(status) != 0;
    json_t *result = NULL;
    if (valid && (allow_dirty || !dirty)) {
        result = json_new_object();
        evidence_text(result, "commit", commit);
        evidence_put(result, "worktree_dirty", json_new_bool(dirty));
    }
    string_free(commit);
    string_free(status);
    return result;
}

static string_t *evidence_first_line(const string_t *root, const char *const *argv)
{
    string_t *output = evidence_run(root, argv, true);
    if (!output)
        return NULL;
    checks_strings_t *lines = checks_split(output, '\n');
    string_t *result = NULL;
    for (size_t i = 0; i < checks_strings_count(lines); ++i) {
        string_t *line = string_clone(checks_strings_get(lines, i));
        string_trim(line);
        if (string_byte_length(line)) {
            result = line;
            break;
        }
        string_free(line);
    }
    checks_strings_free(lines);
    string_free(output);
    return result;
}

json_t *evidence_tools(const string_t *root)
{
    static const char *const names[] = {"cc", "dvisvgm", "latex", "pkg-config", "sqlcipher", "curl", "zstd"};
    json_t *result = json_new_object();
    for (size_t i = 0; i < sizeof(names) / sizeof(*names); ++i) {
        const char *tool = names[i];
        if (i == 0) {
            tool = getenv("CC");
            if (!tool || !*tool)
                tool = "gcc";
        }
        const char *const argv[] = {tool, "--version", NULL};
        string_t *version = evidence_first_line(root, argv);
        evidence_text(result, names[i], version);
        string_free(version);
    }
    return result;
}

json_t *evidence_modules(const string_t *root)
{
    static const char *const modules[] = {
        "gmp", "mpfr", "mpc", "libunistring", "sqlcipher", "libcurl", "libzstd", "libsodium"};
    json_t *result = json_new_object();
    for (size_t i = 0; i < sizeof(modules) / sizeof(*modules); ++i) {
        const char *const argv[] = {"pkg-config", "--modversion", modules[i], NULL};
        string_t *version = evidence_first_line(root, argv);
        evidence_text(result, modules[i], version);
        string_free(version);
    }
    return result;
}

json_t *evidence_compliance(const string_t *root)
{
    static const char *const paths[] = {
        "LICENSE", "THIRD_PARTY_NOTICES.md", "DEPENDENCIES.spdx", "docs/compliance-status.md"};
    json_t *result = json_new_object();
    for (size_t i = 0; i < sizeof(paths) / sizeof(*paths); ++i) {
        string_t *path = checks_path(root, paths[i]);
        string_t *resolved = evidence_resolve(path);
        string_t *hash = resolved ? evidence_hash(resolved) : NULL;
        string_free(resolved);
        string_free(path);
        if (!hash) {
            json_free(result);
            return NULL;
        }
        evidence_text(result, paths[i], hash);
        string_free(hash);
    }
    return result;
}
