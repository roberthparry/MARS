/**
 * @file checks_coverage.c
 * @brief Validate and summarise measured file-module GCC coverage reports.
 *
 * This developer command reads measured GCC coverage data. Sorted inventories
 * and binary lookup identify missing, unexpected and duplicate source reports.
 * Counter spelling is checked without floating-point conversion; percentages
 * use integer thresholds, avoiding both rounding passes and multiplication
 * overflow. The JSON preflight bounds nesting and rejects duplicate members
 * before the general JSON parser can replace them. No shell is invoked.
 */
#include <ctype.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "file.h"
#include "checks_process.h"
#include "checks_coverage.h"

typedef struct {
    const string_t *path;
    uint64_t counts[4];
    bool found;
} checks_coverage_source_t;

static bool checks_coverage_unique(checks_strings_t *values)
{
    checks_strings_sort(values);
    for (size_t i = 1; i < checks_strings_count(values); ++i)
        if (!string_compare(checks_strings_get(values, i - 1), checks_strings_get(values, i)))
            return false;
    return true;
}

/* Only lexical structure is checked here; json_from_text validates the grammar. */
static bool checks_coverage_preflight(const string_t *text)
{
    checks_strings_t *keys[64] = {0};
    unsigned char kinds[64] = {0};
    size_t depth = 0, length = string_byte_length(text);
    bool ok = true;
    for (size_t i = 0; ok && i < length; ++i) {
        unsigned char c = checks_byte(text, i);
        if (c == '{' || c == '[') {
            if (depth == 64) {
                ok = false;
                break;
            }
            kinds[depth] = c;
            keys[depth++] = checks_strings_new();
        } else if (c == '}' || c == ']') {
            if (!depth || kinds[depth - 1] != (c == '}' ? '{' : '[')) {
                ok = false;
                break;
            }
            --depth;
            ok = checks_coverage_unique(keys[depth]);
            checks_strings_free(keys[depth]);
            keys[depth] = NULL;
        } else if (c == '"') {
            size_t start = i++;
            while (i < length && checks_byte(text, i) != '"') {
                if (checks_byte(text, i) == '\\' && i + 1 < length)
                    ++i;
                ++i;
            }
            if (i == length) {
                ok = false;
                break;
            }
            size_t next = i + 1;
            while (next < length && isspace(checks_byte(text, next)))
                ++next;
            if (checks_byte(text, next) == ':') {
                string_t *quoted = checks_slice(text, start, i + 1 - start);
                json_t *key = json_from_text(quoted);
                const string_t *name = json_string_value(key);
                /* GCC emits ordinary JSON, never MARS extended numeric objects. */
                ok = depth && kinds[depth - 1] == '{' && name && !checks_equal(name, "$mars.number");
                if (ok)
                    checks_strings_add(keys[depth - 1], string_clone(name));
                json_free(key);
                string_free(quoted);
            }
        }
    }
    ok = ok && !depth;
    while (depth)
        checks_strings_free(keys[--depth]);
    return ok;
}

static bool checks_coverage_integer(const json_t *value, uint64_t *number)
{
    const string_t *text = json_number_text(value);
    size_t length = text ? string_byte_length(text) : 0;
    *number = 0;
    if (!length || length > 20)
        return false;
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = checks_byte(text, i);
        if (c < '0' || c > '9' || *number > (UINT64_MAX - (c - '0')) / 10)
            return false;
        *number = *number * 10 + (c - '0');
    }
    return true;
}

static bool checks_coverage_add(uint64_t *total, uint64_t count)
{
    if (count > UINT64_MAX - *total)
        return false;
    *total += count;
    return true;
}

static checks_strings_t *checks_coverage_files(const string_t *directory, const char *prefix, const char *suffix)
{
    checks_strings_t *paths = checks_strings_new();
    file_t *dir = file_new(directory);
    bool ok = dir && file_open_directory(dir);
    file_info_t *entry = NULL;
    while (ok && file_read_directory(dir, &entry) && entry) {
        string_t *name = checks_text(file_info_name(entry));
        if (file_info_type(entry) == FILE_TYPE_REGULAR && string_starts_with(name, prefix) &&
            string_ends_with(name, suffix))
            checks_strings_add(paths, string_clone(name));
        string_free(name);
        file_info_free(entry);
    }
    ok = ok && !file_last_error(dir);
    file_free(dir);
    if (!ok) {
        checks_strings_free(paths);
        return NULL;
    }
    checks_strings_sort(paths);
    return paths;
}

static checks_strings_t *checks_coverage_public(const string_t *root)
{
    string_t *path = checks_path(root, "include/file.h");
    string_t *header = checks_read(path);
    string_free(path);
    if (!header)
        return NULL;
    checks_strings_t *names = checks_strings_new();
    size_t length = string_byte_length(header);
    /* Match the original script's file_ identifiers followed by an opening bracket. */
    for (size_t i = 0; i < length;) {
        unsigned char c = checks_byte(header, i);
        if (!isalnum(c) && c != '_') {
            ++i;
            continue;
        }
        size_t start = i++;
        while (isalnum(checks_byte(header, i)) || checks_byte(header, i) == '_')
            ++i;
        size_t end = i, next = i;
        while (isspace(checks_byte(header, next)))
            ++next;
        if (end - start > 5 && checks_at(header, start, "file_") && checks_byte(header, next) == '(')
            checks_strings_add(names, checks_slice(header, start, end - start));
    }
    string_free(header);
    checks_strings_sort(names);
    checks_strings_t *unique = checks_strings_new();
    for (size_t i = 0; i < checks_strings_count(names); ++i) {
        const string_t *name = checks_strings_get(names, i);
        if (!i || string_compare(name, checks_strings_get(names, i - 1)))
            checks_strings_add(unique, string_clone(name));
    }
    checks_strings_free(names);
    return unique;
}

static int checks_coverage_source_compare(const void *key, const void *element)
{
    const checks_coverage_source_t *source = element;
    return string_compare(key, source->path);
}

static int checks_coverage_number_compare(const void *left, const void *right)
{
    uint64_t a = *(const uint64_t *)left, b = *(const uint64_t *)right;
    return (a > b) - (a < b);
}

static bool checks_coverage_lines(const json_t *lines, uint64_t counts[4])
{
    if (!lines || json_type(lines) != JSON_ARRAY)
        return false;
    size_t length = json_array_size(lines);
    if (length > SIZE_MAX / sizeof(uint64_t))
        return false;
    uint64_t *numbers = length ? calloc(length, sizeof(*numbers)) : NULL;
    if (length && !numbers)
        checks_fatal("allocating coverage line numbers");
    bool ok = true;
    for (size_t i = 0; ok && i < length; ++i) {
        const json_t *line = json_array_get(lines, i);
        uint64_t count;
        ok = checks_coverage_integer(checks_member(line, "line_number"), &numbers[i]) && numbers[i] &&
             checks_coverage_integer(checks_member(line, "count"), &count);
        if (!ok)
            break;
        ok = checks_coverage_add(&counts[0], count > 0) && checks_coverage_add(&counts[1], 1);
        const json_t *branches = checks_member(line, "branches");
        if (branches && json_type(branches) != JSON_ARRAY)
            ok = false;
        for (size_t b = 0; ok && b < json_array_size(branches); ++b) {
            ok = checks_coverage_integer(checks_member(json_array_get(branches, b), "count"), &count) &&
                 checks_coverage_add(&counts[2], count > 0) && checks_coverage_add(&counts[3], 1);
        }
    }
    if (ok && length > 1) {
        qsort(numbers, length, sizeof(*numbers), checks_coverage_number_compare);
        for (size_t i = 1; i < length; ++i)
            if (numbers[i - 1] == numbers[i])
                ok = false;
    }
    free(numbers);
    return ok;
}

static bool checks_coverage_functions(const json_t *functions, checks_strings_t *executed)
{
    if (functions && json_type(functions) != JSON_ARRAY)
        return false;
    checks_strings_t *names = checks_strings_new();
    bool ok = true;
    for (size_t i = 0; ok && i < json_array_size(functions); ++i) {
        const json_t *function = json_array_get(functions, i);
        const string_t *name = json_string_value(checks_member(function, "name"));
        uint64_t count;
        ok = name && string_byte_length(name) &&
             checks_coverage_integer(checks_member(function, "execution_count"), &count);
        if (ok) {
            checks_strings_add(names, string_clone(name));
            if (count)
                checks_strings_add(executed, string_clone(name));
        }
    }
    ok = checks_coverage_unique(names) && ok;
    checks_strings_free(names);
    return ok;
}

static string_t *checks_coverage_normalise_path(const string_t *path)
{
    checks_strings_t *parts = checks_split(path, '/');
    string_t *normal = checks_text(string_starts_with(path, "/") ? "/" : "");
    for (size_t i = 0; i < checks_strings_count(parts); ++i) {
        const string_t *part = checks_strings_get(parts, i);
        if (!string_byte_length(part) || checks_equal(part, "."))
            continue;
        if (string_byte_length(normal) && !string_ends_with(normal, "/"))
            string_append_char(normal, '/');
        checks_append(normal, part);
    }
    checks_strings_free(parts);
    return normal;
}

static bool checks_coverage_report(const json_t *report, checks_coverage_source_t *sources, size_t count,
                                   checks_strings_t *executed)
{
    const json_t *files = checks_member(report, "files");
    if (!files || json_type(files) != JSON_ARRAY)
        return false;
    for (size_t i = 0; i < json_array_size(files); ++i) {
        const json_t *item = json_array_get(files, i);
        const string_t *path = json_string_value(checks_member(item, "file"));
        if (!path || !string_byte_length(path))
            return false;
        string_t *normal = checks_coverage_normalise_path(path);
        if (!string_starts_with(normal, "src/file/")) {
            string_free(normal);
            continue;
        }
        string_t *name = checks_slice(normal, 9, string_byte_length(normal) - 9);
        string_free(normal);
        /* Match Path.parent == src/file: nested paths belong to other modules. */
        if (string_find(name, "/") >= 0) {
            string_free(name);
            continue;
        }
        checks_coverage_source_t *source = bsearch(name, sources, count, sizeof(*sources), checks_coverage_source_compare);
        string_free(name);
        if (!source || source->found || !checks_coverage_lines(checks_member(item, "lines"), source->counts) ||
            !checks_coverage_functions(checks_member(item, "functions"), executed))
            return false;
        source->found = true;
    }
    return true;
}

static int checks_coverage_load(const string_t *path, checks_coverage_source_t *sources, size_t count,
                                checks_strings_t *executed)
{
    checks_strings_t *arguments = checks_strings_new();
    checks_strings_add(arguments, checks_text("gzip"));
    checks_strings_add(arguments, checks_text("-cd"));
    checks_strings_add(arguments, checks_text("--"));
    checks_strings_add(arguments, string_clone(path));
    string_t *output = NULL, *errors = NULL;
    int status = -1;
    bool ran = checks_process_run(arguments, NULL, 30, &output, &errors, &status);
    checks_strings_free(arguments);
    int result = 2;
    if (ran && !status && output) {
        json_t *report = checks_coverage_preflight(output) ? json_from_text(output) : NULL;
        result = report && checks_coverage_report(report, sources, count, executed) ? 0 : 1;
        json_free(report);
    }
    if (result)
        fprintf(stderr, "file-coverage: %s: %s\n", string_c_str(path),
                result == 2 ? "cannot decompress report" : "malformed, duplicate or unexpected coverage record");
    string_free(output);
    string_free(errors);
    return result;
}

static bool checks_coverage_threshold(uint64_t covered, uint64_t total, uint64_t tenths)
{
    /* ceil(total * tenths / 10), without an overflowing intermediate product. */
    uint64_t minimum = (total / 10) * tenths + ((total % 10) * tenths + 9) / 10;
    return total && covered >= minimum;
}

static int checks_coverage_summary(checks_coverage_source_t *sources, size_t count,
                                   checks_strings_t *public, checks_strings_t *executed)
{
    uint64_t totals[4] = {0};
    for (size_t i = 0; i < count; ++i) {
        if (!sources[i].found) {
            fprintf(stderr, "Missing file-module coverage report: %s\n", string_c_str(sources[i].path));
            return 1;
        }
        for (size_t c = 0; c < 4; ++c)
            if (!checks_coverage_add(&totals[c], sources[i].counts[c])) {
                fprintf(stderr, "file-coverage: aggregate count overflow\n");
                return 1;
            }
        printf("%s: lines %" PRIu64 "/%" PRIu64 ", branch outcomes %" PRIu64 "/%" PRIu64 "\n",
               string_c_str(sources[i].path), sources[i].counts[0], sources[i].counts[1],
               sources[i].counts[2], sources[i].counts[3]);
    }
    if (!totals[1] || !totals[3]) {
        fprintf(stderr, "file-coverage: zero line or branch outcome total\n");
        return 1;
    }
    checks_strings_sort(executed);
    checks_strings_t *missing = checks_strings_new();
    size_t functions = checks_strings_count(public);
    for (size_t i = 0; i < functions; ++i) {
        const string_t *name = checks_strings_get(public, i);
        if (!checks_strings_has(executed, string_c_str(name)))
            checks_strings_add(missing, string_clone(name));
    }
    printf("TOTAL: lines %" PRIu64 "/%" PRIu64 " (%.2Lf%%), branch outcomes %" PRIu64 "/%" PRIu64 " (%.2Lf%%)\n",
           totals[0], totals[1], 100.0L * totals[0] / totals[1], totals[2], totals[3], 100.0L * totals[2] / totals[3]);
    printf("Public API functions executed: %zu/%zu\n", functions - checks_strings_count(missing), functions);
    bool ok = !checks_strings_count(missing);
    if (!ok) {
        fprintf(stderr, "Unexecuted public API functions: ");
        for (size_t i = 0; i < checks_strings_count(missing); ++i)
            fprintf(stderr, "%s%s", i ? ", " : "", string_c_str(checks_strings_get(missing, i)));
        fputc('\n', stderr);
    }
    checks_strings_free(missing);
    if (!checks_coverage_threshold(totals[0], totals[1], 9) || !checks_coverage_threshold(totals[2], totals[3], 8)) {
        fprintf(stderr, "File coverage requires at least 90%% of lines and 80%% of branch outcomes\n");
        ok = false;
    }
    return ok ? 0 : 1;
}

/* Run the file-module coverage policy against an isolated or real repository. */
int checks_coverage(const string_t *root, int argc, char **argv)
{
    if (!root || argc != 1 || !argv || !argv[0] || !argv[0][0]) {
        fprintf(stderr, "usage: mars_checks file-coverage DIR\n");
        return 2;
    }
    string_t *directory = checks_path(root, argv[0]);
    string_t *source_dir = checks_path(root, "src/file");
    checks_strings_t *reports = checks_coverage_files(directory, "file_", ".gcov.json.gz");
    checks_strings_t *expected = checks_coverage_files(source_dir, "", ".c");
    checks_strings_t *public = checks_coverage_public(root);
    checks_strings_t *executed = checks_strings_new();
    size_t count = checks_strings_count(expected);
    checks_coverage_source_t *sources = NULL;
    int result = 2;
    if (!reports || !expected || !public) {
        fprintf(stderr, "file-coverage: cannot read report directory, src/file or include/file.h\n");
        goto done;
    }
    result = 1;
    if (!count || !checks_strings_count(reports) || !checks_strings_count(public)) {
        fprintf(stderr, "file-coverage: empty source, report or public API inventory\n");
        goto done;
    }
    if (count > SIZE_MAX / sizeof(*sources)) {
        fprintf(stderr, "file-coverage: source inventory overflow\n");
        goto done;
    }
    sources = calloc(count, sizeof(*sources));
    if (!sources)
        checks_fatal("allocating coverage source inventory");
    for (size_t i = 0; i < count; ++i)
        sources[i].path = checks_strings_get(expected, i);
    for (size_t i = 0; i < checks_strings_count(reports); ++i) {
        string_t *path = checks_path(directory, string_c_str(checks_strings_get(reports, i)));
        result = checks_coverage_load(path, sources, count, executed);
        string_free(path);
        if (result)
            goto done;
    }
    result = checks_coverage_summary(sources, count, public, executed);
done:
    free(sources);
    checks_strings_free(executed);
    checks_strings_free(public);
    checks_strings_free(expected);
    checks_strings_free(reports);
    string_free(source_dir);
    string_free(directory);
    return result;
}
