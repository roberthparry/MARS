/**
 * @file cfg_settings.c
 * @brief Literal environment-file parsing and atomic installer configuration publication.
 *
 * Reads bounded configuration strings, understands concatenated quotes and escaped
 * characters, and never executes substitutions. Duplicate conflicting assignments
 * are rejected. Publication uses file.h and an exclusive private staging file.
 */
#include <sodium.h>
#include <string.h>
#include <sys/stat.h>

#include "cfg_storage.h"
#include "ustring.h"

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

/* Read one setting, retaining literal contents rather than interpreting shell syntax. */
string_t *cfg_storage_setting(file_t *file, const char *name)
{
    string_t *text = cfg_storage_read_configuration(file);
    string_t *separator = string_new_with("\n");
    size_t count = 0;
    string_t **lines = text && separator ? string_split_string(text, separator, &count) : NULL;
    string_t *result = lines ? string_new() : NULL;
    bool matched = false;
    for (size_t i = 0; result && i < count; ++i) {
        string_trim(lines[i]);
        string_cursor_t *cursor = string_cursor_new(lines[i]);
        if (!cursor) {
            string_free(result);
            result = NULL;
            break;
        }
        if (string_cursor_consume(cursor, "export "))
            string_cursor_skip_spaces(cursor);
        bool found = string_cursor_consume(cursor, name);
        string_cursor_skip_spaces(cursor);
        found = found && string_cursor_consume(cursor, "=");
        if (found) {
            string_cursor_skip_spaces(cursor);
            string_t *raw =
                string_cursor_slice_between(string_cursor_position(cursor), string_cursor_end_position(cursor), cursor);
            string_t *value = raw ? string_unquote_shell(raw) : NULL;
            string_free(raw);
            if (value && matched && string_compare(result, value)) {
                string_free(value);
                value = NULL;
            }
            string_free(result);
            result = value;
            matched = true;
        }
        string_cursor_free(cursor);
    }
    string_split_free(lines, count);
    string_free(separator);
    string_free(text);
    return result;
}

/* Produce shell-safe literal text; never permit another configuration line. */
string_t *cfg_storage_quote(const string_t *value)
{
    string_t *quoted = value ? string_new_with("'") : NULL;
    string_view_t view = string_view_all(value);
    size_t start = 0, length = string_view_length(view);
    bool ok = quoted != NULL;
    for (size_t i = 0; ok && i < length; ++i) {
        unsigned char ch = 0;
        bool ascii = string_view_peek_ascii(view, i, &ch);
        if (ascii && (!ch || ch == '\r' || ch == '\n'))
            ok = false;
        else if (ascii && ch == '\'') {
            ok = !string_append_utf8_exact(quoted, string_c_str(value) + start, i - start) &&
                 !string_append_cstr(quoted, "'\\''");
            start = i + 1;
        }
    }
    if (ok)
        ok = !string_append_utf8_exact(quoted, string_c_str(value) + start, length - start) &&
             !string_append_char(quoted, '\'');
    if (!ok) {
        string_free(quoted);
        quoted = NULL;
    }
    return quoted;
}

/* Publish only a fully written, synced private file. */
bool cfg_storage_publish(file_t *destination, const string_t *directory, const string_t *body)
{
    bool exists = false;
    if (!body || sodium_init() < 0 || !cfg_storage_safe_regular(destination, &exists) ||
        !cfg_storage_private_directory(directory, true))
        return false;
    unsigned char nonce[16];
    char suffix[33];
    randombytes_buf(nonce, sizeof(nonce));
    sodium_bin2hex(suffix, sizeof(suffix), nonce, sizeof(nonce));
    string_t *path = string_sprintf("%s/.mars-config-%s", string_c_str(directory), suffix);
    file_t *temporary = path ? file_new(path) : NULL;
    mode_t previous = umask(0077);
    bool created = temporary && file_open(temporary, FILE_MODE_CREATE_NEW, FILE_ACCESS_WRITE);
    umask(previous);
    bool ok = created && file_write_text(temporary, body) && file_sync(temporary, false);
    bool closed = !created || file_close(temporary);
    ok = ok && closed && cfg_storage_safe_regular(destination, &exists) && file_move(temporary, destination, true);
    if (ok) {
        file_t *folder = file_new(directory);
        ok = folder && file_open_directory(folder) && file_sync(folder, false);
        file_free(folder);
    }
    if (created && !ok)
        file_delete(temporary);
    file_free(temporary);
    string_free(path);
    return ok;
}
