/**
 * @file checks_compliance.c
 * @brief Validate legal installation, SPDX references and recorded provenance.
 *
 * Retains the Python control's required notices and weather safeguards, replacing
 * its required Python paths with the native control sources and build definition.
 * SHA-256 streams through file.h and libsodium, already a MARS dependency.
 */
#include <errno.h>
#include <sodium.h>
#include <stdio.h>

#include "file.h"
#include "checks_process.h"
#include "checks_controls.h"

static const char *const root_documents[] = {"DEPENDENCIES.spdx", "LICENSE", "THIRD_PARTY_NOTICES.md"};
static const char *const guide_documents[] = {"docs/almanac-data-provenance.md", "docs/compliance-status.md",
                                              "docs/licensing.md", "docs/privacy.md",
                                              "docs/visual-asset-provenance.md"};
static const char *const packages[] = {"SPDXRef-Package-DE440",     "SPDXRef-Package-DE440s",
                                       "SPDXRef-Package-MARS",      "SPDXRef-Package-NAIF-Auxiliary-Kernels",
                                       "SPDXRef-Package-SQLCipher", "SPDXRef-Package-SQLite",
                                       "SPDXRef-Package-TZDB",      "SPDXRef-Package-Unicode-CLDR",
                                       "SPDXRef-Package-WeatherAPI"};

static string_t *checks_compliance_read_repository(const string_t *root, const char *relative)
{
    string_t *path = checks_path(root, relative);
    string_t *text = checks_read(path);
    string_free(path);
    if (text) {
        string_replace(text, "\r\n", "\n");
        string_replace(text, "\r", "\n");
    }
    return text;
}

static checks_strings_t *checks_compliance_make_paths(const string_t *makefile, const char *variable)
{
    string_t *pattern = string_sprintf("^%s[[:space:]]*:=[[:space:]]*(.+)$", variable), *value = NULL;
    checks_strings_t *paths = checks_strings_new();
    if (checks_match(makefile, string_c_str(pattern), 1, &value)) {
        string_t *normal = checks_normalise_space(value);
        checks_strings_free(paths);
        paths = checks_split(normal, ' ');
        string_free(normal);
    }
    string_free(value);
    string_free(pattern);
    checks_strings_sort(paths);
    return paths;
}

/* Verify installation includes all legal records. */
void checks_installed_documents(const string_t *makefile, checks_strings_t *errors)
{
    checks_strings_t *roots = checks_compliance_make_paths(makefile, "LEGAL_ROOT_DOCUMENTS");
    checks_strings_t *guides = checks_compliance_make_paths(makefile, "LEGAL_GUIDE_DOCUMENTS");
    for (size_t i = 0; i < sizeof(root_documents) / sizeof(*root_documents); ++i)
        if (!checks_strings_has(roots, root_documents[i]))
            checks_strings_add(errors, string_sprintf("legal root document is not installed: %s", root_documents[i]));
    for (size_t i = 0; i < sizeof(guide_documents) / sizeof(*guide_documents); ++i)
        if (!checks_strings_has(guides, guide_documents[i]))
            checks_strings_add(errors, string_sprintf("legal guide document is not installed: %s", guide_documents[i]));
    checks_strings_free(roots);
    checks_strings_free(guides);
}

/* Compute the recorded digest using bounded binary reads. */
string_t *checks_sha256(const string_t *path)
{
    file_t *requested = file_new(path);
    file_t *file = requested ? file_resolve(requested) : NULL;
    file_free(requested);
    if (!file || !file_open_read(file)) {
        file_free(file);
        return NULL;
    }
    crypto_hash_sha256_state state;
    crypto_hash_sha256_init(&state);
    unsigned char buffer[65536], digest[crypto_hash_sha256_BYTES];
    size_t count = 0;
    bool ok;
    while ((ok = file_read(file, buffer, sizeof(buffer), &count)) && count) {
        if (checks_process_interrupt_signal()) {
            ok = false;
            errno = ECANCELED;
            break;
        }
        crypto_hash_sha256_update(&state, buffer, (unsigned long long)count);
    }
    file_free(file);
    if (!ok)
        return NULL;
    crypto_hash_sha256_final(&state, digest);
    string_t *hex = checks_text("");
    for (size_t i = 0; i < sizeof(digest); ++i)
        string_append_format(hex, "%02x", digest[i]);
    return hex;
}

/* Check each precisely formatted provenance checksum row. */
bool checks_provenance(const string_t *root, const string_t *source, checks_strings_t *errors)
{
    const char *pattern =
        "^\\|[[:space:]]*`([^`]+)`[[:space:]]*\\|[[:space:]]*`([0-9a-f]{64})`[[:space:]]*\\|[[:space:]]*$";
    checks_strings_t *lines = checks_split(source, '\n');
    size_t rows = 0;
    bool ok = true;
    for (size_t i = 0; i < checks_strings_count(lines); ++i) {
        const string_t *line = checks_strings_get(lines, i);
        string_t *relative = NULL, *expected = NULL;
        if (!checks_match(line, pattern, 1, &relative))
            continue;
        checks_match(line, pattern, 2, &expected);
        ++rows;
        string_t *path = checks_path(root, string_c_str(relative));
        if (!checks_is_file(path)) {
            checks_strings_add(errors, string_sprintf("provenance file is missing: %s", string_c_str(relative)));
        } else {
            string_t *actual = checks_sha256(path);
            if (!actual)
                ok = false;
            else if (string_compare(actual, expected))
                checks_strings_add(errors, string_sprintf("provenance checksum mismatch for %s: expected %s, found %s",
                                                          string_c_str(relative), string_c_str(expected),
                                                          string_c_str(actual)));
            string_free(actual);
        }
        string_free(path);
        string_free(expected);
        string_free(relative);
        if (!ok)
            break;
    }
    if (!rows)
        checks_strings_add(errors, checks_text("almanac provenance contains no machine-checkable SHA-256 rows"));
    checks_strings_free(lines);
    return ok;
}

/* Validate SPDX inventory structure and all relationship references. */
void checks_spdx(const string_t *source, checks_strings_t *errors)
{
    checks_strings_t *lines = checks_split(source, '\n'), *ids = checks_strings_new();
    bool version = false, licence = false;
    for (size_t i = 0; i < checks_strings_count(lines); ++i) {
        const string_t *line = checks_strings_get(lines, i);
        version |= checks_equal(line, "SPDXVersion: SPDX-2.3");
        licence |= checks_equal(line, "DataLicense: CC0-1.0");
        string_t *id = NULL;
        if (checks_match(line, "^SPDXID:[[:space:]]*([^[:space:]]+)[[:space:]]*$", 1, &id))
            checks_strings_add(ids, id);
    }
    if (!version)
        checks_strings_add(errors, checks_text("dependency inventory is not declared as SPDX 2.3"));
    if (!licence)
        checks_strings_add(errors, checks_text("dependency inventory does not use the required CC0 SPDX data licence"));
    checks_strings_sort(ids);
    for (size_t i = 0; i < checks_strings_count(ids);) {
        const string_t *id = checks_strings_get(ids, i);
        size_t end = i + 1;
        while (end < checks_strings_count(ids) && !string_compare(id, checks_strings_get(ids, end)))
            ++end;
        if (end - i > 1)
            checks_strings_add(errors,
                               string_sprintf("SPDX identifier is declared %zu times: %s", end - i, string_c_str(id)));
        i = end;
    }
    for (size_t i = 0; i < sizeof(packages) / sizeof(*packages); ++i)
        if (!checks_strings_has(ids, packages[i]))
            checks_strings_add(errors, string_sprintf("required SPDX package is missing: %s", packages[i]));
    const char *relationship = "^Relationship:[[:space:]]*([^[:space:]]+)[[:space:]]+"
                               "[^[:space:]]+[[:space:]]+([^[:space:]]+)[[:space:]]*$";
    for (size_t i = 0; i < checks_strings_count(lines); ++i) {
        for (size_t group = 1; group <= 2; ++group) {
            string_t *id = NULL;
            if (checks_match(checks_strings_get(lines, i), relationship, group, &id) &&
                !checks_strings_has(ids, string_c_str(id)))
                checks_strings_add(
                    errors, string_sprintf("SPDX relationship references an unknown identifier: %s", string_c_str(id)));
            string_free(id);
        }
    }
    checks_strings_free(ids);
    checks_strings_free(lines);
}

static bool checks_compliance_required_paths(const string_t *root, bool allow_untracked, checks_strings_t *errors)
{
    static const char *const controls[] = {
        ".githooks/pre-commit",
        "tools/mars_checks/Makefile",
        "tools/mars_checks/include/checks_controls.h",
        "tools/mars_checks/include/checks_coverage.h",
        "tools/mars_checks/include/checks_evidence.h",
        "tools/mars_checks/include/checks_fixtures.h",
        "tools/mars_checks/include/checks_policy.h",
        "tools/mars_checks/include/checks_process.h",
        "tools/mars_checks/include/checks_readme.h",
        "tools/mars_checks/include/checks_support.h",
        "tools/mars_checks/src/app/checks_main.c",
        "tools/mars_checks/src/compliance/checks_compliance.c",
        "tools/mars_checks/src/coverage/checks_coverage.c",
        "tools/mars_checks/src/distribution/checks_distribution.c",
        "tools/mars_checks/src/evidence/checks_evidence.c",
        "tools/mars_checks/src/evidence/evidence_libraries.c",
        "tools/mars_checks/src/evidence/evidence_private.h",
        "tools/mars_checks/src/evidence/evidence_probes.c",
        "tools/mars_checks/src/fixtures/checks_fixtures.c",
        "tools/mars_checks/src/markdown/checks_markdown.c",
        "tools/mars_checks/src/policy/checks_function_tables.c",
        "tools/mars_checks/src/policy/checks_inline.c",
        "tools/mars_checks/src/policy/checks_policy.c",
        "tools/mars_checks/src/process/checks_process.c",
        "tools/mars_checks/src/readme/checks_examples.c",
        "tools/mars_checks/src/readme/checks_readme.c",
        "tools/mars_checks/src/support/checks_support.c",
    };
    checks_strings_t *required = checks_strings_new();
    for (size_t i = 0; i < sizeof(controls) / sizeof(*controls); ++i)
        checks_strings_add(required, checks_text(controls[i]));
    for (size_t i = 0; i < sizeof(root_documents) / sizeof(*root_documents); ++i)
        checks_strings_add(required, checks_text(root_documents[i]));
    for (size_t i = 0; i < sizeof(guide_documents) / sizeof(*guide_documents); ++i)
        checks_strings_add(required, checks_text(guide_documents[i]));
    checks_strings_sort(required);
    checks_strings_t *tracked = allow_untracked ? NULL : checks_git_paths(root, false);
    if (!allow_untracked && !tracked) {
        checks_strings_free(required);
        return false;
    }
    for (size_t i = 0; i < checks_strings_count(required); ++i) {
        const string_t *relative = checks_strings_get(required, i);
        string_t *path = checks_path(root, string_c_str(relative));
        bool present = allow_untracked ? checks_is_file(path) : checks_strings_has(tracked, string_c_str(relative));
        if (!present)
            checks_strings_add(errors, string_sprintf("required compliance path is not %s: %s",
                                                      allow_untracked ? "present" : "tracked", string_c_str(relative)));
        string_free(path);
    }
    checks_strings_free(tracked);
    checks_strings_free(required);
    return true;
}

static void checks_compliance_notices(const string_t *source, const string_t *weather, const string_t *privacy,
                                      checks_strings_t *errors)
{
    static const struct {
        const char *marker;
        const char *description;
    } required[] = {{"SQLCipher Community Edition", "SQLCipher notice"},
                    {"Unicode CLDR week data", "Unicode CLDR notice"},
                    {"Astronomical data and generation tools", "astronomical-data notice"},
                    {"WeatherAPI.com", "WeatherAPI notice"},
                    {"https://www.weatherapi.com/terms.aspx", "WeatherAPI terms link"},
                    {"https://naif.jpl.nasa.gov/naif/rules.html", "NAIF use-rules link"}};
    static const struct {
        const char *marker;
        const char *description;
    } safeguards[] = {{"MARS_WEATHER_API_KEY", "user-supplied API-key setting"},
                      {"Weather is informational and must not be the sole basis for safety-critical decisions.",
                       "end-user weather safety notice"},
                      {"https://www.weatherapi.com/", "WeatherAPI attribution link"},
                      {"https://www.weatherapi.com/privacy.aspx", "WeatherAPI privacy link"},
                      {"https://www.weatherapi.com/terms.aspx", "WeatherAPI terms link"}};
    /* These short policy inventories have a fixed, bounded size. */
    for (size_t i = 0; i < sizeof(required) / sizeof(*required); ++i)
        if (string_find(source, required[i].marker) < 0)
            checks_strings_add(errors,
                               string_sprintf("third-party notices are missing the %s", required[i].description));
    for (size_t i = 0; i < sizeof(safeguards) / sizeof(*safeguards); ++i)
        if (string_find(weather, safeguards[i].marker) < 0)
            checks_strings_add(errors,
                               string_sprintf("weather integration is missing the %s", safeguards[i].description));
    if (string_find(privacy, "WeatherAPI") < 0)
        checks_strings_add(errors, checks_text("privacy notice does not describe the optional WeatherAPI integration"));
}

/* Execute the complete mechanical compliance policy. */
int checks_compliance(const string_t *root, bool allow_untracked, bool quiet)
{
    checks_strings_t *errors = checks_strings_new();
    bool ok = checks_compliance_required_paths(root, allow_untracked, errors);
    const char *const paths[] = {"Makefile",
                                 "docs/almanac-data-provenance.md",
                                 "DEPENDENCIES.spdx",
                                 "THIRD_PARTY_NOTICES.md",
                                 "tools/mars_lab/src/calendar/lab_calendar_weather.c",
                                 "tools/mars_lab/assets/index.html",
                                 "docs/privacy.md"};
    string_t *texts[7] = {0};
    for (size_t i = 0; ok && i < sizeof(paths) / sizeof(*paths); ++i) {
        texts[i] = checks_compliance_read_repository(root, paths[i]);
        ok = texts[i] != NULL;
    }
    if (ok) {
        checks_installed_documents(texts[0], errors);
        ok = checks_provenance(root, texts[1], errors);
        checks_spdx(texts[2], errors);
        checks_append(texts[4], texts[5]);
        checks_compliance_notices(texts[3], texts[4], texts[6], errors);
    }
    int result = !ok ? 2 : checks_strings_count(errors) ? 1 : 0;
    if (!ok)
        fprintf(stderr, "compliance check could not inspect the repository\n");
    else if (result) {
        fprintf(stderr, "compliance checks failed:\n");
        for (size_t i = 0; i < checks_strings_count(errors); ++i)
            string_fprintf(stderr, "  %s\n", string_c_str(checks_strings_get(errors, i)));
    } else if (!quiet) {
        puts("compliance records, installed notices, SPDX references and provenance checksums are consistent");
    }
    for (size_t i = 0; i < sizeof(texts) / sizeof(*texts); ++i)
        string_free(texts[i]);
    checks_strings_free(errors);
    return result;
}
