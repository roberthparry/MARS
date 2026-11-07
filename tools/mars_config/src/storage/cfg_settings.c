/**
 * @file cfg_settings.c
 * @brief Literal option text and parent-path handling for native installers.
 *
 * Copies optional argument values and resolves parent directories through string cursors.
 * Persistent settings are owned by the shared encrypted settings service.
 */
#include <string.h>

#include "string-t.h"
#include "cfg_storage.h"

/* Copy and trim optional command-line or environment text. */
string_t *cfg_storage_value(const char *text)
{
    string_t *value = string_new();
    if (value && text && string_append_utf8_exact(value, text, strlen(text))) {
        string_free(value);
        return NULL;
    }
    string_trim(value);
    return value;
}

/* Split a path using string cursors rather than byte-pointer parsing. */
string_t *cfg_storage_parent(const string_t *path)
{
    string_cursor_t *cursor = string_cursor_new(path);
    if (!cursor)
        return NULL;
    string_pos_t start = string_cursor_position(cursor), end = start;
    bool slash = false;
    while (!string_cursor_done(cursor)) {
        string_pos_t position = string_cursor_position(cursor);
        if (string_cursor_consume(cursor, "/")) {
            slash = true;
            end = position == start ? string_cursor_position(cursor) : position;
        } else {
            string_cursor_next(cursor);
        }
    }
    string_t *parent = slash ? string_cursor_slice_between(start, end, cursor) : string_new_with(".");
    string_cursor_free(cursor);
    return parent;
}
