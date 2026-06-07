/* test_config_paths.c - test configuration path helpers */

#include <string.h>

#include "test_config_internal.h"

string_t *test_config_normalise_file_path(const char *file)
{
    const char *tests = file ? strstr(file, "tests/") : NULL;
    const char *slash;
    string_t *path = string_new();

    if (!path)
        return NULL;

    if (tests) {
        string_append_cstr(path, tests);
        return path;
    }

    slash = file ? strrchr(file, '/') : NULL;
    string_append_cstr(path, "tests/");
    string_append_cstr(path, slash ? slash + 1 : (file ? file : ""));
    return path;
}

string_t *test_config_flattened_file_path(const char *file)
{
    const char *slash = file ? strrchr(file, '/') : NULL;
    string_t *path = string_new();

    if (!path)
        return NULL;

    string_append_cstr(path, "tests/");
    string_append_cstr(path, slash ? slash + 1 : (file ? file : ""));
    return path;
}

string_t *test_config_compute_global_path(void)
{
    const char *file = __FILE__;
    const char *src = strstr(file, "tests/");
    string_t *path = string_new();

    if (!path)
        return NULL;

    if (!src) {
        string_append_cstr(path, "tests/test_config.json");
        return path;
    }

    string_append_format(path, "%.*s", (int)(src - file), file);
    string_append_cstr(path, "tests/test_config.json");
    return path;
}

string_t *test_config_compute_local_path(const char *file)
{
    string_t *normalised = test_config_normalise_file_path(file);
    const char *full;
    const char *dot;
    string_t *path;

    if (!normalised)
        return NULL;

    full = string_c_str(normalised);
    dot = strrchr(full, '.');
    path = string_new();
    if (path)
        string_append_format(path,
                             "%.*s.json",
                             (int)(dot ? (size_t)(dot - full) : strlen(full)),
                             full);

    string_free(normalised);
    return path;
}
