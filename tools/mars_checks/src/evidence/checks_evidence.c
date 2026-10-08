/**
 * @file checks_evidence.c
 * @brief Native release evidence assembly and atomic publication.
 *
 * Validates arguments and mandatory source, compliance and dependency evidence
 * before creating an output staging directory. A same-filesystem move installs
 * the complete JSON document; failed collection never touches existing output.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <sys/utsname.h>
#include <time.h>

#include "checks_evidence.h"
#include "evidence_private.h"

static json_t *evidence_host(void)
{
    struct utsname host;
    if (uname(&host) != 0)
        return NULL;
    json_t *result = json_new_object();
    string_t *platform = string_sprintf("%s-%s-%s", host.sysname, host.release, host.machine);
    string_t *machine = checks_text(host.machine);
    evidence_text(result, "platform", platform);
    evidence_text(result, "machine", machine);
    string_free(platform);
    string_free(machine);
    return result;
}

static json_t *evidence_document(const string_t *root, const string_t *artefact, bool allow_dirty)
{
    json_t *source = evidence_source(root, allow_dirty);
    if (!source) {
        string_fprintf(stderr, "release evidence requires known Git state and a clean worktree "
                               "(--allow-dirty permits a local dirty trial)\n");
        return NULL;
    }
    string_t *hash = evidence_hash(artefact);
    json_t *libraries = hash ? evidence_libraries(root, artefact) : NULL;
    json_t *compliance = libraries ? evidence_compliance(root) : NULL;
    json_t *host = compliance ? evidence_host() : NULL;
    time_t now = time(NULL);
    struct tm utc;
    char timestamp[40];
    bool timed = now != (time_t)-1 && gmtime_r(&now, &utc) &&
                 strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S+00:00", &utc);
    if (!host || !timed) {
        json_free(source);
        json_free(libraries);
        json_free(compliance);
        json_free(host);
        string_free(hash);
        return NULL;
    }
    json_t *result = json_new_object();
    string_t *schema = checks_text("MARS release dependency evidence 1");
    string_t *generated = checks_text(timestamp);
    evidence_text(result, "schema", schema);
    evidence_text(result, "generated_at_utc", generated);
    string_free(schema);
    string_free(generated);
    evidence_put(result, "source", source);
    evidence_put(result, "host", host);
    evidence_put(result, "tools", evidence_tools(root));
    evidence_put(result, "pkg_config_modules", evidence_modules(root));
    evidence_put(result, "compliance_records", compliance);
    json_t *binary = json_new_object();
    evidence_text(binary, "path", artefact);
    evidence_text(binary, "sha256", hash);
    evidence_put(binary, "dynamic_libraries", libraries);
    evidence_put(result, "artefact", binary);
    string_free(hash);
    string_t *note = checks_text(
        "This supplements DEPENDENCIES.spdx for one built artefact. A distributor bundling any listed library "
        "must include the exact licence and source-availability material required by that library.");
    evidence_text(result, "distribution_note", note);
    string_free(note);
    return result;
}

static bool evidence_output_safe(const string_t *root, const string_t *artefact, const string_t *output)
{
    file_t *destination = file_new(output);
    if (!destination)
        return false;
    bool exists = file_exists(destination);
    if (!exists) {
        bool absent = file_last_error(destination) == 0;
        file_free(destination);
        return absent;
    }
    file_info_t *info = file_get_info(destination);
    /* Reject all output links: canonical paths cannot distinguish hard-link aliases. */
    bool safe = info && file_info_type(info) == FILE_TYPE_REGULAR && file_info_link_count(info) == 1;
    file_info_free(info);
    file_free(destination);
    string_t *resolved = safe ? evidence_resolve(output) : NULL;
    safe = resolved && string_compare(resolved, artefact) != 0;
    static const char *const records[] = {
        "LICENSE", "THIRD_PARTY_NOTICES.md", "DEPENDENCIES.spdx", "docs/compliance-status.md"};
    for (size_t i = 0; safe && i < sizeof(records) / sizeof(*records); ++i) {
        string_t *path = checks_path(root, records[i]);
        string_t *record = evidence_resolve(path);
        safe = record && string_compare(resolved, record) != 0;
        string_free(record);
        string_free(path);
    }
    string_free(resolved);
    return safe;
}

static bool evidence_publish(const string_t *root, const string_t *artefact, const string_t *output,
                             const json_t *document)
{
    string_t *text = json_to_string_pretty(document, 2);
    if (!text)
        return false;
    bool ok = string_append_char(text, '\n') == 0 && evidence_output_safe(root, artefact, output);
    size_t end = string_byte_length(output);
    while (end && checks_byte(output, end - 1) != '/')
        --end;
    string_t *parent = end ? checks_slice(output, 0, end) : checks_text(".");
    ok = ok && checks_mkdir(parent);
    file_t *temporary = ok ? file_create_temp_directory(parent) : NULL;
    string_t *directory = temporary ? checks_text(file_path(temporary)) : NULL;
    string_t *staging = directory ? checks_path(directory, "evidence.json") : NULL;
    file_t *source = staging ? file_new(staging) : NULL;
    file_t *destination = file_new(output);
    ok = temporary && source && destination && file_create_text(source) && file_write_text(source, text) &&
         file_sync(source, false);
    if (source && file_is_open(source))
        ok = file_close(source) && ok;
    if (ok && !checks_process_interrupt_signal() && evidence_output_safe(root, artefact, output))
        ok = file_move(source, destination, true);
    else
        ok = false;
    /* Publication is the commit point; subsequent cleanup cannot reverse success. */
    bool cleaned = !source || file_delete(source);
    if (temporary)
        cleaned = file_remove_directory(temporary) && cleaned;
    if (!cleaned)
        string_fprintf(stderr, "release evidence %s; could not remove staging path %s\n",
                       ok ? "published" : "not published", directory ? string_c_str(directory) : "(unavailable)");
    file_free(source);
    file_free(destination);
    file_free(temporary);
    string_free(staging);
    string_free(directory);
    string_free(parent);
    string_free(text);
    return ok;
}

/* Collect a complete native report before atomically publishing the output. */
int checks_evidence(const string_t *root, int argc, char **argv)
{
    const char *library = "build/release/libmars.so";
    const char *output = "build/compliance/release-evidence.json";
    bool allow_dirty = false;
    for (int i = 0; i < argc; ++i) {
        string_t *argument = checks_text(argv[i]);
        bool library_value = string_starts_with(argument, "--library=");
        bool output_value = string_starts_with(argument, "--output=");
        bool is_library = checks_equal(argument, "--library");
        bool is_output = checks_equal(argument, "--output");
        bool is_dirty = checks_equal(argument, "--allow-dirty");
        bool help = checks_equal(argument, "--help") || checks_equal(argument, "-h");
        string_free(argument);
        if (help) {
            string_printf("mars_checks release-evidence [--library PATH] [--output PATH] [--allow-dirty]\n");
            return 0;
        }
        if ((library_value && argv[i][10]) || (output_value && argv[i][9])) {
            if (library_value)
                library = argv[i] + 10;
            else
                output = argv[i] + 9;
        } else if (is_dirty) {
            allow_dirty = true;
        } else if ((is_library || is_output) && i + 1 < argc && argv[i + 1][0] &&
                   (argv[i + 1][0] != '-' || !argv[i + 1][1])) {
            if (is_library)
                library = argv[++i];
            else
                output = argv[++i];
        } else {
            string_fprintf(stderr, "invalid release-evidence argument: %s\n", argv[i]);
            return 2;
        }
    }
    string_t *requested = checks_path(root, library);
    string_t *artefact = evidence_resolve(requested);
    string_t *destination = checks_path(root, output);
    json_t *document = artefact && evidence_output_safe(root, artefact, destination)
                           ? evidence_document(root, artefact, allow_dirty)
                           : NULL;
    bool ok = document && evidence_publish(root, artefact, destination, document);
    if (ok)
        string_printf("%s\n", string_c_str(destination));
    else
        string_fprintf(stderr, "could not write release evidence: mandatory inspection or atomic publication failed\n");
    json_free(document);
    string_free(destination);
    string_free(artefact);
    string_free(requested);
    return ok ? 0 : 1;
}
