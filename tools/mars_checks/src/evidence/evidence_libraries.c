/**
 * @file evidence_libraries.c
 * @brief Dynamic dependency and Debian package evidence collection.
 *
 * Parses ldd records using native strings, resolves dependency links through
 * file.h, and hashes regular files. Package metadata is optional; failure to
 * hash a present copyright record or any required library aborts collection.
 */
#include "evidence_private.h"

static json_t *evidence_package(const string_t *root, const string_t *path)
{
    const char *const owner_argv[] = {"dpkg-query", "-S", string_c_str(path), NULL};
    string_t *owner = evidence_run(root, owner_argv, false);
    string_t *spec = NULL;
    if (!owner || !checks_match(owner, "^([^\n]+): /", 1, &spec)) {
        string_free(owner);
        return json_new_null();
    }
    string_free(owner);
    string_trim(spec);
    json_t *result = json_new_object();
    evidence_text(result, "package", spec);
    const char *const details_argv[] = {
        "dpkg-query", "-W", "-f=${binary:Package}\t${Version}\t${Architecture}\n", string_c_str(spec), NULL};
    string_t *details = evidence_run(root, details_argv, false);
    string_free(spec);
    if (!details)
        return result;
    string_trim(details);
    checks_strings_t *fields = checks_split(details, '\t');
    string_free(details);
    if (checks_strings_count(fields) != 3) {
        checks_strings_free(fields);
        return result;
    }
    const string_t *package = checks_strings_get(fields, 0);
    const string_t *version = checks_strings_get(fields, 1);
    const string_t *architecture = checks_strings_get(fields, 2);
    if (!checks_match(package, "^[a-z0-9][a-z0-9+.-]*(:[a-z0-9-]+)?$", 0, NULL) ||
        !string_byte_length(version) || !checks_match(architecture, "^[a-z0-9-]+$", 0, NULL)) {
        checks_strings_free(fields);
        return result;
    }
    evidence_text(result, "package", package);
    evidence_text(result, "version", version);
    evidence_text(result, "architecture", architecture);
    string_t *base = NULL;
    checks_match(package, "^([^:]+)", 1, &base);
    string_t *copyright = string_sprintf("/usr/share/doc/%s/copyright", string_c_str(base));
    string_free(base);
    checks_strings_free(fields);
    if (checks_is_file(copyright)) {
        string_t *resolved = evidence_resolve(copyright);
        string_t *hash = resolved ? evidence_hash(resolved) : NULL;
        string_free(resolved);
        if (!hash) {
            string_free(copyright);
            json_free(result);
            return NULL;
        }
        evidence_text(result, "copyright_file", copyright);
        evidence_text(result, "copyright_sha256", hash);
        string_free(hash);
    }
    string_free(copyright);
    return result;
}

static json_t *evidence_library(const string_t *root, const string_t *soname, const string_t *path)
{
    string_t *rooted = checks_path(root, string_c_str(path));
    string_t *resolved = evidence_resolve(rooted);
    string_free(rooted);
    string_t *hash = resolved ? evidence_hash(resolved) : NULL;
    if (!hash) {
        string_free(resolved);
        return NULL;
    }
    json_t *package = evidence_package(root, resolved);
    json_t *result = NULL;
    if (package) {
        result = json_new_object();
        evidence_text(result, "soname", soname);
        evidence_text(result, "resolved_path", resolved);
        evidence_text(result, "sha256", hash);
        evidence_put(result, "system_package", package);
    }
    string_free(hash);
    string_free(resolved);
    return result;
}

json_t *evidence_libraries(const string_t *root, const string_t *artefact)
{
    const char *const argv[] = {"ldd", string_c_str(artefact), NULL};
    string_t *output = evidence_run(root, argv, false);
    if (!output)
        return NULL;
    checks_strings_t *lines = checks_split(output, '\n');
    string_free(output);
    json_t *result = json_new_array();
    bool ok = true;
    const char *mapping = "^([^[:space:]]+)[[:space:]]+=>[[:space:]]+([^[:space:]]+)"
                          "[[:space:]]+\\(0x[0-9a-fA-F]+\\)$";
    const char *loader = "^(/[^[:space:]]+)[[:space:]]+\\(0x[0-9a-fA-F]+\\)$";
    for (size_t i = 0; ok && i < checks_strings_count(lines); ++i) {
        string_t *line = string_clone(checks_strings_get(lines, i));
        string_trim(line);
        if (!string_byte_length(line) ||
            checks_match(line, "^linux-(vdso|gate)[^[:space:]]*[[:space:]]+\\(0x[0-9a-fA-F]+\\)$", 0, NULL)) {
            string_free(line);
            continue;
        }
        string_t *soname = NULL, *path = NULL;
        if (checks_match(line, mapping, 1, &soname)) {
            ok = checks_match(line, mapping, 2, &path);
        } else if (checks_match(line, loader, 1, &path)) {
            ok = checks_match(path, "/([^/]+)$", 1, &soname);
        } else {
            ok = false;
        }
        json_t *entry = ok ? evidence_library(root, soname, path) : NULL;
        ok = entry && json_array_append(result, entry);
        json_free(entry);
        string_free(line);
        string_free(soname);
        string_free(path);
    }
    checks_strings_free(lines);
    if (!ok || !json_array_size(result)) {
        json_free(result);
        return NULL;
    }
    return result;
}
