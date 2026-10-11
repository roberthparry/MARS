/**
 * @file test_tba_support.c
 * @brief Bounded removal of private, already-stopped forecasting test fixtures.
 *
 * Uses file_t for all I/O and string_t paths; symlinks are unlinked, not followed.
 * Limits accommodate Firefox's disposable profile without accepting arbitrary roots.
 */
#include "string-t.h"
#include "test_tba_support.h"

static bool test_tba_remove(file_t *directory, unsigned depth, size_t *remaining)
{
    if (depth > 16 || !file_open_directory(directory))
        return false;
    bool ok = true;
    for (;;) {
        file_info_t *entry = NULL;
        if (!file_read_directory(directory, &entry)) {
            ok = false;
            break;
        }
        if (!entry)
            break;
        if (!*remaining) {
            file_info_free(entry);
            ok = false;
            break;
        }
        --*remaining;
        string_t *path = string_sprintf("%s/%s", file_path(directory), file_info_name(entry));
        file_t *child = path ? file_new(path) : NULL;
        bool removed = child && (file_info_type(entry) == FILE_TYPE_DIRECTORY ?
                                      test_tba_remove(child, depth + 1, remaining) : file_delete(child));
        ok = removed && ok;
        file_free(child);
        string_free(path);
        file_info_free(entry);
    }
    bool closed = file_close(directory);
    return ok && closed && file_remove_directory(directory);
}

/* Accept only a fixture-named leaf; callers supply their own mkdtemp result. */
bool test_tba_cleanup(file_t *directory)
{
    string_t *path = directory ? string_new_with(file_path(directory)) : NULL;
    string_offset_t start = path ? string_find(path, "/mars-tba-") : -1;
    bool safe = start >= 0 && string_find(path, "..") < 0;
    string_t *name = safe ? string_substr(path, (size_t)start + 1, string_byte_length(path)) : NULL;
    safe = safe && name && string_byte_length(name) > 15 && string_find(name, "/") < 0;
    string_free(name);
    string_free(path);
    size_t remaining = 20000;
    return safe && test_tba_remove(directory, 0, &remaining);
}
