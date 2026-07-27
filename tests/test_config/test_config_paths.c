/* test_config_paths.c - test configuration path helpers */

#define MARS_TEST_CONFIG_INTERNAL_ACCESS
#include "test_config_internal.h"

static string_t *test_config_empty_path(void)
{
    return string_new_with("");
}

static string_t *test_config_basename(const string_t *file)
{
    string_cursor_t *cursor;
    string_pos_t segment_start;
    string_t *basename;

    if (!file)
        return test_config_empty_path();

    cursor = string_cursor_new(file);
    if (!cursor)
        return NULL;

    segment_start = string_cursor_position(cursor);
    while (!string_cursor_done(cursor)) {
        if (string_cursor_match(cursor, "/")) {
            (void)string_cursor_next(cursor);
            segment_start = string_cursor_position(cursor);
            continue;
        }
        (void)string_cursor_next(cursor);
    }

    basename = string_cursor_extract(segment_start, cursor);
    string_cursor_free(cursor);
    return basename;
}

static string_t *test_config_prefix_before_tests_dir(const string_t *file)
{
    string_cursor_t *cursor;
    string_t *prefix = NULL;

    if (!file)
        return NULL;

    cursor = string_cursor_new(file);
    if (!cursor)
        return NULL;

    while (!string_cursor_done(cursor)) {
        string_pos_t pos = string_cursor_position(cursor);

        if (string_cursor_match(cursor, "tests/")) {
            prefix = string_cursor_slice_between(0u, pos, cursor);
            break;
        }
        (void)string_cursor_next(cursor);
    }

    string_cursor_free(cursor);
    return prefix;
}

string_t *test_config_normalise_file_path(const string_t *file)
{
    string_offset_t tests_pos;
    string_t *path = string_new();

    if (!path)
        return NULL;

    tests_pos = file ? string_find(file, "tests/") : -1;
    if (tests_pos >= 0) {
        string_cursor_t *cursor = string_cursor_new(file);

        if (!cursor) {
            string_free(path);
            return NULL;
        }

        if (string_cursor_append_slice_between(path,
                                               (string_pos_t)tests_pos,
                                               string_cursor_end_position(cursor),
                                               cursor) != 0) {
            string_cursor_free(cursor);
            string_free(path);
            return NULL;
        }
        string_cursor_free(cursor);
        return path;
    }

    if (string_append_cstr(path, "tests/") != 0) {
        string_free(path);
        return NULL;
    }

    {
        string_t *basename = test_config_basename(file);

        if (!basename || string_append_string(path, basename) != 0) {
            string_free(basename);
            string_free(path);
            return NULL;
        }
        string_free(basename);
    }

    return path;
}

string_t *test_config_flattened_file_path(const string_t *file)
{
    string_t *path = string_new_with("tests/");
    string_t *basename;

    if (!path)
        return NULL;

    basename = test_config_basename(file);
    if (!basename || string_append_string(path, basename) != 0) {
        string_free(basename);
        string_free(path);
        return NULL;
    }

    string_free(basename);
    return path;
}

string_t *test_config_compute_global_path(void)
{
    string_t *file = string_new_with(__FILE__);
    string_t *prefix = test_config_prefix_before_tests_dir(file);
    string_t *path = string_new();

    string_free(file);
    if (!path) {
        string_free(prefix);
        return NULL;
    }

    if (prefix && string_append_string(path, prefix) != 0) {
        string_free(prefix);
        string_free(path);
        return NULL;
    }
    string_free(prefix);

    if (string_append_cstr(path, "tests/test_config.json") != 0) {
        string_free(path);
        return NULL;
    }

    return path;
}

string_t *test_config_compute_local_path(const string_t *file)
{
    string_t *normalised = test_config_normalise_file_path(file);
    string_cursor_t *cursor;
    string_pos_t extension_start = 0u;
    string_t *path;

    if (!normalised)
        return NULL;

    cursor = string_cursor_new(normalised);
    if (!cursor) {
        string_free(normalised);
        return NULL;
    }

    extension_start = string_cursor_end_position(cursor);
    while (!string_cursor_done(cursor)) {
        if (string_cursor_match(cursor, "."))
            extension_start = string_cursor_position(cursor);
        (void)string_cursor_next(cursor);
    }

    path = string_cursor_slice_between(0u, extension_start, cursor);
    if (path && string_append_cstr(path, ".json") != 0) {
        string_free(path);
        path = NULL;
    }

    string_cursor_free(cursor);
    string_free(normalised);
    return path;
}
