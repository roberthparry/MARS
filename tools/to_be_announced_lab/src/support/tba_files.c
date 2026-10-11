/**
 * @file tba_files.c
 * @brief Bounded regular-file reads and atomic native forecasting state writes.
 *
 * Every byte operation uses file_t. Temporary state siblings are exclusively
 * created with private permissions and renamed only after successful flush/close.
 */
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "lab_wire.h"
#include "tba_support.h"

#ifndef TBA_ROOT
#define TBA_ROOT "."
#endif

/* Resolve relative paths against the configured repository. */
string_t *tba_path(const char *path)
{
    const char *root = getenv("MARS_ROOT");
    if (!root || !*root)
        root = TBA_ROOT;
    if (!path || !*path)
        return NULL;
    return path[0] == '/' ? string_new_with(path) : string_sprintf("%s/%s", root, path);
}

/* Resolve a packaged client asset. */
string_t *tba_asset_path(const char *name)
{
    string_t *relative = string_sprintf("tools/to_be_announced_lab/assets/%s", name);
    string_t *path = relative ? tba_path(string_c_str(relative)) : NULL;
    string_free(relative);
    return path;
}

/* Bound reads on an opened regular file, including concurrent growth. */
string_t *tba_file_read(const char *path, size_t limit)
{
    file_t *file = file_new_cstr(path);
    if (!file || !file_open_read(file)) {
        file_free(file);
        return NULL;
    }
    string_t *text = string_new();
    char *buffer = limit <= 64u * 1024u * 1024u ? malloc(limit + 1) : NULL;
    size_t size = 0, count = 0;
    bool ok = text && buffer;
    while (ok) {
        size_t capacity = limit + 1 - size;
        ok = file_read(file, buffer + size, capacity > 4096 ? 4096 : capacity, &count);
        if (!ok || !count)
            break;
        if (count > limit - size) {
            errno = EFBIG;
            ok = false;
            break;
        }
        size += count;
    }
    if (ok)
        ok = string_append_utf8_exact(text, buffer, size) == 0;
    free(buffer);
    file_free(file);
    if (!ok) {
        string_free(text);
        text = NULL;
    }
    return text;
}

/* Workers are single-threaded: protect creation itself, not only a later chmod. */
bool tba_file_open_private(file_t *file, file_mode_t mode, file_access_t access)
{
    if (!file || (mode != FILE_MODE_CREATE_NEW && mode != FILE_MODE_OPEN_OR_CREATE))
        return false;
    if (mode == FILE_MODE_OPEN_OR_CREATE && file_exists(file) && !file_chmod(file, 0600))
        return false;
    mode_t previous = umask(0077);
    bool opened = file_open(file, mode, access);
    umask(previous);
    return opened;
}

/* Bound the open-file read, including concurrent growth, before decoding any values. */
json_t *tba_file_read_message(const char *path)
{
    const size_t limit = 4u * 1024u * 1024u;
    file_t *file = file_new_cstr(path);
    bool ok = file && file_open_read(file);
    unsigned char *buffer = ok ? malloc(limit + 1) : NULL;
    size_t size = 0, count = 0;
    ok = ok && buffer;
    while (ok) {
        ok = file_read(file, buffer + size, limit + 1 - size, &count);
        if (!ok || !count)
            break;
        size += count;
        if (size > limit) {
            errno = EFBIG;
            ok = false;
        }
    }
    json_t *result = ok ? lab_wire_decode(buffer, size) : NULL;
    free(buffer);
    file_free(file);
    return result;
}

/* Encode before opening a staging file; publish only after a complete private write. */
bool tba_file_publish_message(const char *path, const json_t *value)
{
    array_t *bytes = path ? lab_wire_encode(value) : NULL;
    if (!bytes)
        return false;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    string_t *temporary = string_sprintf("%s.%ld.%ld.tmp", path, (long)getpid(), now.tv_nsec);
    file_t *stage = temporary ? file_new(temporary) : NULL;
    file_t *destination = file_new_cstr(path);
    bool created = stage && tba_file_open_private(stage, FILE_MODE_CREATE_NEW, FILE_ACCESS_WRITE);
    size_t written = 0;
    bool ok = created && destination && file_write(stage, array_get(bytes, 0), array_size(bytes), &written) &&
              written == array_size(bytes) &&
              file_sync(stage, false) && file_close(stage) && file_move(stage, destination, true);
    if (created && !ok) {
        file_close(stage);
        file_delete(stage);
    }
    file_free(stage);
    file_free(destination);
    string_free(temporary);
    array_destroy(bytes);
    return ok;
}
