/**
 * @file tba_files.c
 * @brief Bounded regular-file reads and atomic native forecasting state writes.
 *
 * Every byte operation uses file_t. Temporary state siblings are exclusively
 * created with private permissions and renamed only after successful flush/close.
 */
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

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

/* Publish a complete private state file without truncating the existing one. */
bool tba_file_publish(const char *path, const string_t *text)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    string_t *temporary = string_sprintf("%s.%ld.%ld.tmp", path, (long)getpid(), now.tv_nsec);
    file_t *stage = temporary ? file_new(temporary) : NULL;
    file_t *destination = file_new_cstr(path);
    bool created = stage && file_open(stage, FILE_MODE_CREATE_NEW, FILE_ACCESS_WRITE);
    bool ok = created && destination && file_chmod(stage, 0600) && file_write_text(stage, text) &&
              file_sync(stage, false) && file_close(stage) && file_move(stage, destination, true);
    if (created && !ok) {
        file_close(stage);
        file_delete(stage);
    }
    file_free(stage);
    file_free(destination);
    string_free(temporary);
    return ok;
}
