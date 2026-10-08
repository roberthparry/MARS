/**
 * @file checks_support.c
 * @brief Owned collections and file.h-backed repository traversal for check tools.
 *
 * These adapters preserve exact source bytes, sort paths deterministically and
 * keep filesystem handles private. Traversal never descends through symlinks.
 * Syntax routines operate on MARS strings through their public byte views.
 */
#include <ctype.h>
#include <errno.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "array.h"
#include "file.h"
#include "checks_support.h"

struct checks_strings {
    string_t **values;
    size_t count;
    size_t capacity;
};

/* Report unrecoverable infrastructure errors. */
void checks_fatal(const char *message)
{
    fprintf(stderr, "mars_checks: %s: %s\n", message, strerror(errno));
    exit(2);
}

/* Construct an owned text value. */
string_t *checks_text(const char *text)
{
    string_t *result = string_new();
    if (!result || string_append_utf8_exact(result, text, strlen(text)))
        checks_fatal("allocating text");
    return result;
}

/* Append without changing the spelling of source text. */
void checks_append(string_t *destination, const string_t *source)
{
    if (string_append_utf8_exact(destination, string_c_str(source), string_byte_length(source)))
        checks_fatal("appending exact text");
}

/* Allocate an owned collection. */
checks_strings_t *checks_strings_new(void)
{
    checks_strings_t *items = calloc(1, sizeof(*items));
    if (!items)
        checks_fatal("allocating collection");
    return items;
}

/* Release a collection and its contents. */
void checks_strings_free(checks_strings_t *items)
{
    if (!items)
        return;
    for (size_t i = 0; i < items->count; ++i)
        string_free(items->values[i]);
    free(items->values);
    free(items);
}

/* Transfer one string to the collection. */
void checks_strings_add(checks_strings_t *items, string_t *text)
{
    if (!text)
        checks_fatal("allocating collection text");
    if (items->count == items->capacity) {
        size_t capacity = items->capacity ? items->capacity * 2 : 16;
        string_t **values = realloc(items->values, capacity * sizeof(*values));
        if (!values)
            checks_fatal("growing collection");
        items->values = values;
        items->capacity = capacity;
    }
    items->values[items->count++] = text;
}

/* Borrow an indexed value. */
const string_t *checks_strings_get(const checks_strings_t *items, size_t index)
{
    return items && index < items->count ? items->values[index] : NULL;
}

/* Report collection size. */
size_t checks_strings_count(const checks_strings_t *items)
{
    return items ? items->count : 0;
}

static int checks_support_compare_strings(const void *left, const void *right)
{
    return string_compare(*(string_t *const *)left, *(string_t *const *)right);
}

/* Establish lexical order for reporting and binary search. */
void checks_strings_sort(checks_strings_t *items)
{
    if (items->count > 1)
        qsort(items->values, items->count, sizeof(*items->values), checks_support_compare_strings);
}

/* Query sorted collections without a linear scan. */
bool checks_strings_has(const checks_strings_t *items, const char *text)
{
    string_t *key = checks_text(text);
    bool found =
        items->count && bsearch(&key, items->values, items->count, sizeof(key), checks_support_compare_strings);
    string_free(key);
    return found;
}

/* Copy an exact byte interval. */
string_t *checks_slice(const string_t *text, size_t start, size_t length)
{
    string_t *result = checks_text("");
    size_t total = string_byte_length(text);
    if (start > total || length > total - start) {
        errno = EINVAL;
        checks_fatal("invalid text interval");
    }
    if (string_append_utf8_exact(result, string_c_str(text) + start, length))
        checks_fatal("copying text interval");
    return result;
}

/* Inspect ASCII syntax while retaining string ownership. */
unsigned char checks_byte(const string_t *text, size_t offset)
{
    unsigned char byte = 0;
    if (offset >= string_byte_length(text))
        return 0;
    return string_view_peek_ascii(string_view_all(text), offset, &byte) ? byte : 128;
}

/* Compare a bounded byte view with an ASCII literal. */
bool checks_at(const string_t *text, size_t offset, const char *literal)
{
    size_t length = strlen(literal), total = string_byte_length(text);
    return offset <= total && length <= total - offset &&
           string_view_equals_literal(string_view_slice(string_view_all(text), offset, length), literal);
}

/* Compare a complete string with a literal. */
bool checks_equal(const string_t *text, const char *literal)
{
    return text && string_view_equals_literal(string_view_all(text), literal);
}

/* Resolve paths without changing the caller's working directory. */
string_t *checks_path(const string_t *root, const char *path)
{
    string_t *relative = checks_text(path);
    if (string_starts_with(relative, "/"))
        return relative;
    string_t *result = string_clone(root);
    if (!result)
        checks_fatal("allocating resolved path");
    if (!string_ends_with(root, "/"))
        string_append_char(result, '/');
    checks_append(result, relative);
    string_free(relative);
    return result;
}

/* Read through the public file API. */
string_t *checks_read(const string_t *path)
{
    file_t *requested = file_new(path);
    file_t *file = requested ? file_resolve(requested) : NULL;
    array_t *bytes = file ? file_read_all_bytes(file) : NULL;
    string_t *text = bytes ? string_new() : NULL;
    if (text && string_append_utf8_exact(text, array_size(bytes) ? array_get(bytes, 0) : NULL, array_size(bytes))) {
        string_free(text);
        text = NULL;
        errno = EILSEQ;
    } else if (!text && file) {
        errno = file_last_error(file);
    }
    array_destroy(bytes);
    file_free(file);
    file_free(requested);
    return text;
}

/* Write through the public file API. */
bool checks_write(const string_t *path, const string_t *text)
{
    file_t *file = file_new(path);
    bool ok = file && file_write_all_text(file, text);
    if (!ok && file)
        errno = file_last_error(file);
    file_free(file);
    return ok;
}

/* Test the resolved inode rather than merely path existence. */
bool checks_is_file(const string_t *path)
{
    file_t *file = file_new(path), *resolved = file ? file_resolve(file) : NULL;
    file_info_t *info = resolved ? file_get_info(resolved) : NULL;
    bool result = info && file_info_type(info) == FILE_TYPE_REGULAR;
    file_info_free(info);
    file_free(resolved);
    file_free(file);
    return result;
}

/* Create output directory parents. */
bool checks_mkdir(const string_t *path)
{
    file_t *file = file_new(path);
    bool ok = file && file_create_directory(file, 0700, true);
    file_free(file);
    return ok;
}

/* Remove only the supplied private tree, retaining symlink boundaries. */
bool checks_remove_tree(const string_t *path)
{
    file_t *file = file_new(path);
    file_info_t *info = file ? file_get_info(file) : NULL;
    if (!info) {
        file_free(file);
        return false;
    }
    bool directory = file_info_type(info) == FILE_TYPE_DIRECTORY;
    file_info_free(info);
    bool ok = true;
    if (directory) {
        ok = file_open_directory(file);
        while (ok && file_read_directory(file, &info) && info) {
            string_t *child = checks_path(path, file_info_name(info));
            file_info_free(info);
            ok = checks_remove_tree(child);
            string_free(child);
        }
        ok = file_close(file) && ok;
    }
    ok = ok && (directory ? file_remove_directory(file) : file_delete(file));
    file_free(file);
    return ok;
}

static void checks_support_collect_files(checks_strings_t *items, const string_t *root, const char *relative,
                                         const char *suffix, bool recursive)
{
    string_t *path = checks_path(root, relative);
    file_t *directory = file_new(path);
    string_free(path);
    if (!directory || !file_open_directory(directory))
        checks_fatal("listing repository directory");
    file_info_t *entry = NULL;
    while (file_read_directory(directory, &entry) && entry) {
        string_t *name = checks_text(file_info_name(entry));
        string_t *child = *relative ? string_sprintf("%s/%s", relative, string_c_str(name)) : string_clone(name);
        if (recursive && file_info_type(entry) == FILE_TYPE_DIRECTORY) {
            if (!checks_equal(name, ".git") && !checks_equal(name, "build") && !checks_equal(name, ".codex") &&
                !checks_equal(name, ".agents"))
                checks_support_collect_files(items, root, string_c_str(child), suffix, true);
        } else if (file_info_type(entry) != FILE_TYPE_DIRECTORY && string_ends_with(name, suffix) &&
                   !checks_equal(name, "AGENTS.md")) {
            checks_strings_add(items, string_clone(child));
        }
        string_free(child);
        string_free(name);
        file_info_free(entry);
    }
    if (file_last_error(directory))
        checks_fatal("reading repository directory");
    file_free(directory);
}

/* Return deterministic repository-relative source paths. */
checks_strings_t *checks_files(const string_t *root, const char *directory, const char *suffix, bool recursive)
{
    checks_strings_t *items = checks_strings_new();
    checks_support_collect_files(items, root, directory, suffix, recursive);
    checks_strings_sort(items);
    return items;
}

/* Split binary-safe Git records or source lines. */
checks_strings_t *checks_split(const string_t *text, unsigned char separator)
{
    checks_strings_t *items = checks_strings_new();
    size_t start = 0, length = string_byte_length(text);
    for (size_t i = 0; i < length; ++i) {
        if (checks_byte(text, i) == separator) {
            checks_strings_add(items, checks_slice(text, start, i - start));
            start = i + 1;
        }
    }
    if (start < length)
        checks_strings_add(items, checks_slice(text, start, length - start));
    return items;
}

/* Adapt POSIX regular expressions to owned MARS strings. */
bool checks_match(const string_t *text, const char *pattern, size_t group, string_t **capture)
{
    regex_t expression;
    regmatch_t matches[8];
    if (capture)
        *capture = NULL;
    if (group >= 8 || regcomp(&expression, pattern, REG_EXTENDED | REG_NEWLINE))
        checks_fatal("invalid checker expression");
    bool found = regexec(&expression, string_c_str(text), 8, matches, 0) == 0;
    if (found && capture && matches[group].rm_so >= 0)
        *capture =
            checks_slice(text, (size_t)matches[group].rm_so, (size_t)(matches[group].rm_eo - matches[group].rm_so));
    regfree(&expression);
    return found;
}

/* Collapse ASCII whitespace in C declarations. */
string_t *checks_normalise_space(const string_t *text)
{
    string_t *result = checks_text("");
    bool space = false;
    size_t length = string_byte_length(text), start = 0;
    for (size_t i = 0; i <= length; ++i) {
        if (i < length && !isspace(checks_byte(text, i)))
            continue;
        if (i > start) {
            if (space && string_byte_length(result))
                string_append_char(result, ' ');
            string_t *word = checks_slice(text, start, i - start);
            checks_append(result, word);
            string_free(word);
        }
        space = true;
        start = i + 1;
    }
    return result;
}

/* Remove the same lexical material as the original header checker. */
string_t *checks_strip_c(const string_t *text, bool directives)
{
    string_t *result = checks_text("");
    size_t length = string_byte_length(text), start = 0;
    bool line_start = true;
    for (size_t i = 0; i < length;) {
        size_t end = i;
        if (checks_at(text, i, "/*")) {
            end = i + 2;
            while (end < length && !checks_at(text, end, "*/"))
                ++end;
            end = end < length ? end + 2 : length;
        } else if (checks_at(text, i, "//")) {
            end = i + 2;
            while (end < length && checks_byte(text, end) != '\n')
                ++end;
        } else if (directives && line_start && checks_byte(text, i) == '#') {
            end = i;
            while (end < length && (checks_byte(text, end) != '\n' || (end && checks_byte(text, end - 1) == '\\')))
                ++end;
        }
        if (end > i) {
            string_t *part = checks_slice(text, start, i - start);
            checks_append(result, part);
            string_free(part);
            string_append_char(result, ' ');
            start = i = end;
            continue;
        }
        unsigned char c = checks_byte(text, i++);
        if (c == '\n')
            line_start = true;
        else if (c != ' ' && c != '\t')
            line_start = false;
    }
    string_t *tail = checks_slice(text, start, length - start);
    checks_append(result, tail);
    string_free(tail);
    return result;
}

/* Borrow an object member through its public API. */
const json_t *checks_member(const json_t *object, const char *key)
{
    if (!object)
        return NULL;
    string_t *name = checks_text(key);
    const json_t *value = json_object_get(object, name);
    string_free(name);
    return value;
}

/* Store a copied string value. */
void checks_json_text(json_t *object, const char *key, const string_t *value)
{
    string_t *name = checks_text(key);
    json_t *item = json_new_string(value);
    if (!item || !json_object_set(object, name, item))
        checks_fatal("creating JSON text member");
    string_free(name);
    json_free(item);
}

/* Store an ordinary JSON integer. */
void checks_json_integer(json_t *object, const char *key, long value)
{
    string_t *name = checks_text(key), *text = string_sprintf("%ld", value);
    json_t *item = json_new_number(text);
    if (!item || !json_object_set(object, name, item))
        checks_fatal("creating JSON integer member");
    json_free(item);
    string_free(text);
    string_free(name);
}
