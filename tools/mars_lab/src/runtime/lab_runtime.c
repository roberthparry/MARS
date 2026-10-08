/**
 * @file lab_runtime.c
 * @brief Preserve and initialise the Lab object-store environment before fork.
 *
 * Uses public file and string APIs for configuration I/O, literal env-file
 * parsing, advisory locking and atomic publication. The existing cache is never
 * modified here. New URL-safe keys use 48 bytes from libsodium, matching the
 * Python launcher's entropy and encoding. Secret contents are never included in
 * diagnostics. Environment access and umask changes require a single-threaded
 * caller; a separate persistent lock inode serialises concurrent launchers.
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <errno.h>
#include <pwd.h>
#include <sodium.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "file.h"
#include "lab_runtime.h"
#include "ustring.h"

static string_t *lab_runtime_environment_text(const char *name)
{
    const char *value = getenv(name);
    string_t *text = string_new();
    if (text && value && string_append_utf8_exact(text, value, strlen(value))) {
        string_free(text);
        return NULL;
    }
    string_trim(text);
    return text;
}

static string_t *lab_runtime_user_directory(void)
{
    string_t *directory = lab_runtime_environment_text("HOME");
    if (!directory || string_byte_length(directory))
        return directory;
    string_free(directory);
    struct passwd *account = getpwuid(getuid());
    if (!account || !account->pw_dir || !*account->pw_dir) {
        errno = ENOENT;
        return NULL;
    }
    return string_new_with(account->pw_dir);
}

static string_t *lab_runtime_expand_path(const string_t *path)
{
    if (!string_starts_with(path, "~/") && !string_view_equals_literal(string_view_all(path), "~"))
        return string_clone(path);
    string_t *directory = lab_runtime_user_directory();
    string_t *tail = string_substr(path, 1u, string_byte_length(path) - 1u);
    if (!directory || !tail || string_append_string(directory, tail)) {
        string_free(directory);
        directory = NULL;
    }
    string_free(tail);
    return directory;
}

static string_t *lab_runtime_home(void)
{
    string_t *configured = lab_runtime_environment_text("MARS_HOME");
    if (!configured)
        return NULL;
    string_t *path;
    if (string_byte_length(configured)) {
        path = lab_runtime_expand_path(configured);
    } else {
        path = lab_runtime_user_directory();
        if (path && string_append_cstr(path, "/.mars")) {
            string_free(path);
            path = NULL;
        }
    }
    string_free(configured);
    return path;
}

static string_t *lab_runtime_parent_path(const string_t *path)
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

static bool lab_runtime_private_directory(const string_t *path, bool tighten_existing)
{
    file_t *directory = file_new(path);
    bool ok = directory && file_create_directory(directory, 0700u, true);
    if (ok && tighten_existing) {
        file_info_t *info = file_get_info(directory);
        ok = info && file_info_type(info) == FILE_TYPE_DIRECTORY && file_info_owner(info) == geteuid();
        file_info_free(info);
        if (!ok)
            errno = EPERM;
        else
            ok = file_chmod(directory, 0700u);
    }
    int error = errno;
    file_free(directory);
    errno = error;
    return ok;
}

static string_t *lab_runtime_cache_path(const string_t *base, bool *default_parent)
{
    string_t *override = lab_runtime_environment_text("MARS_LAB_OBJECT_STORE_PATH");
    if (!override)
        return NULL;
    *default_parent = false;
    if (string_byte_length(override)) {
        string_t *path = lab_runtime_expand_path(override);
        string_free(override);
        return path;
    }
    string_free(override);
    override = lab_runtime_environment_text("MARS_LAB_CACHE_FILE");
    string_t *expanded = override ? lab_runtime_expand_path(override) : NULL;
    string_free(override);
    if (!expanded)
        return NULL;
    if (string_starts_with(expanded, "/"))
        return expanded;
    *default_parent = !string_byte_length(expanded);
    string_t *path =
        string_sprintf("%s/lab/%s", string_c_str(base),
                       string_byte_length(expanded) ? string_c_str(expanded) : "mars_lab_object_store.sqlite3");
    string_free(expanded);
    return path;
}

static bool lab_runtime_safe_regular(file_t *file, bool *exists)
{
    file_info_t *info = file_get_info(file);
    if (!info) {
        *exists = false;
        return file_last_error(file) == ENOENT;
    }
    *exists = true;
    bool ok = file_info_type(info) == FILE_TYPE_REGULAR && file_info_owner(info) == geteuid() &&
              file_info_link_count(info) == 1u;
    file_info_free(info);
    if (!ok)
        errno = EPERM;
    return ok;
}

static file_t *lab_runtime_configuration_lock(const string_t *directory)
{
    string_t *path = string_sprintf("%s/mars-lab.env.lock", string_c_str(directory));
    file_t *lock = path ? file_new(path) : NULL;
    string_free(path);
    bool exists = false;
    bool ok = lock && lab_runtime_safe_regular(lock, &exists);
    mode_t previous = umask(0077);
    if (ok)
        ok = file_open(lock, FILE_MODE_OPEN_OR_CREATE, FILE_ACCESS_READ_WRITE);
    umask(previous);
    if (ok)
        ok = file_close(lock) && file_chmod(lock, 0600u) && file_open(lock, FILE_MODE_OPEN, FILE_ACCESS_READ_WRITE);
    if (ok) {
        do {
            ok = file_lock(lock, true, true);
        } while (!ok && file_last_error(lock) == EINTR);
    }
    if (!ok) {
        int error = errno;
        file_free(lock);
        errno = error;
        return NULL;
    }
    return lock;
}

static string_t *lab_runtime_read_configuration(file_t *file)
{
    bool exists = false;
    if (!lab_runtime_safe_regular(file, &exists))
        return NULL;
    if (!exists)
        return string_new();
    if (!file_chmod(file, 0600u) || !file_open_read(file))
        return NULL;
    string_t *bytes = string_new();
    char buffer[4096];
    bool ok = bytes != NULL;
    size_t count = 0u;
    while (ok) {
        ok = file_read(file, buffer, sizeof(buffer), &count);
        if (!ok || !count)
            break;
        if (string_byte_length(bytes) + count > 1024u * 1024u) {
            errno = EFBIG;
            ok = false;
            break;
        }
        ok = string_append_chars(bytes, buffer, count) == 0;
    }
    int error = errno;
    sodium_memzero(buffer, sizeof(buffer));
    if (!file_close(file) && ok) {
        error = errno;
        ok = false;
    }
    string_t *text = ok ? string_new() : NULL;
    if (text && string_append_utf8_exact(text, string_c_str(bytes), string_byte_length(bytes))) {
        string_free(text);
        text = NULL;
        error = EILSEQ;
    }
    string_free(bytes);
    errno = error;
    return text;
}

static string_t *lab_runtime_key_assignment(const string_t *line, bool *matched)
{
    string_view_t view = string_view_trim(string_view_all(line));
    string_cursor_t *cursor = string_cursor_new_view(view);
    *matched = false;
    if (!cursor)
        return NULL;
    if (string_cursor_consume(cursor, "export "))
        string_cursor_skip_spaces(cursor);
    *matched = string_cursor_consume(cursor, "MARS_LAB_OBJECT_STORE_KEY=");
    string_t *value = *matched ? string_cursor_slice_between(string_cursor_position(cursor),
                                                             string_cursor_end_position(cursor), cursor)
                               : string_new();
    string_cursor_free(cursor);
    if (!value)
        return NULL;
    string_trim(value);
    if (string_byte_length(value) >= 2u && ((string_starts_with(value, "'") && string_ends_with(value, "'")) ||
                                            (string_starts_with(value, "\"") && string_ends_with(value, "\"")))) {
        string_t *unquoted = string_substr(value, 1u, string_byte_length(value) - 2u);
        string_free(value);
        value = unquoted;
        string_trim(value);
    }
    return value;
}

static string_t *lab_runtime_stored_key(string_t **lines, size_t count)
{
    string_t *key = string_new();
    for (size_t i = 0u; key && i < count; ++i) {
        bool matched = false;
        string_t *value = lab_runtime_key_assignment(lines[i], &matched);
        if (!value || (matched && string_byte_length(value) && string_byte_length(key) && string_compare(value, key))) {
            string_free(value);
            string_free(key);
            errno = EINVAL;
            return NULL;
        }
        if (matched && string_byte_length(value) && !string_byte_length(key)) {
            string_free(key);
            key = value;
        } else {
            string_free(value);
        }
    }
    return key;
}

static bool lab_runtime_publish_key(file_t *configuration, const string_t *directory, string_t **lines, size_t count,
                                    const string_t *key)
{
    string_t *document = string_new();
    bool ok = document != NULL, replaced = false;
    for (size_t i = 0u; ok && i < count; ++i) {
        bool matched = false;
        string_t *value = lab_runtime_key_assignment(lines[i], &matched);
        ok = value != NULL;
        string_free(value);
        if (!ok)
            break;
        if (matched) {
            if (!replaced)
                ok = string_append_format(document, "export MARS_LAB_OBJECT_STORE_KEY='%s'\n", string_c_str(key)) >= 0;
            replaced = true;
        } else {
            ok = !string_append_string(document, lines[i]);
            if (ok && i + 1u < count)
                ok = !string_append_char(document, '\n');
        }
    }
    if (ok && !replaced) {
        if (string_byte_length(document) && !string_ends_with(document, "\n"))
            ok = !string_append_char(document, '\n');
        if (ok)
            ok = string_append_format(document, "export MARS_LAB_OBJECT_STORE_KEY='%s'\n", string_c_str(key)) >= 0;
    }
    unsigned char nonce[16];
    char suffix[33];
    randombytes_buf(nonce, sizeof(nonce));
    sodium_bin2hex(suffix, sizeof(suffix), nonce, sizeof(nonce));
    string_t *path = string_sprintf("%s/.mars-lab.env-%s", string_c_str(directory), suffix);
    file_t *temporary = path ? file_new(path) : NULL;
    string_free(path);
    bool created = false;
    mode_t previous = umask(0077);
    if (ok && temporary)
        created = file_open(temporary, FILE_MODE_CREATE_NEW, FILE_ACCESS_WRITE);
    umask(previous);
    ok = ok && created && file_write_text(temporary, document) && file_sync(temporary, false);
    if (created) {
        int error = errno;
        bool closed = file_close(temporary);
        if (!ok)
            errno = error;
        ok = ok && closed;
    }
    if (ok)
        ok = file_chmod(temporary, 0600u) && file_move(temporary, configuration, true);
    if (ok) {
        file_t *folder = file_new(directory);
        ok = folder && file_open_directory(folder) && file_sync(folder, false);
        int error = errno;
        file_free(folder);
        errno = error;
    }
    int error = errno;
    if (created && temporary)
        file_delete(temporary);
    file_free(temporary);
    string_free(document);
    sodium_memzero(nonce, sizeof(nonce));
    sodium_memzero(suffix, sizeof(suffix));
    errno = error;
    return ok;
}

static string_t *lab_runtime_configuration_key(const string_t *base)
{
    string_t *directory = string_sprintf("%s/config", string_c_str(base));
    file_t *lock =
        directory && lab_runtime_private_directory(directory, true) ? lab_runtime_configuration_lock(directory) : NULL;
    string_t *path = lock ? string_sprintf("%s/mars-lab.env", string_c_str(directory)) : NULL;
    file_t *configuration = path ? file_new(path) : NULL;
    string_free(path);
    string_t *text = configuration ? lab_runtime_read_configuration(configuration) : NULL;
    /* A NUL must never silently shorten an existing secret when exported through setenv. */
    if (text) {
        string_view_t view = string_view_all(text);
        for (size_t i = 0u; i < string_byte_length(text); ++i) {
            unsigned char byte = 0u;
            if (string_view_peek_ascii(view, i, &byte) && byte == 0u) {
                string_free(text);
                text = NULL;
                errno = EINVAL;
                break;
            }
        }
    }
    string_t *separator = string_new_with("\n");
    size_t count = 0u;
    string_t **lines = text && separator ? string_split_string(text, separator, &count) : NULL;
    string_t *key = lines ? lab_runtime_stored_key(lines, count) : NULL;
    if (key && !string_byte_length(key)) {
        unsigned char random[48];
        char encoded[sodium_base64_ENCODED_LEN(48u, sodium_base64_VARIANT_URLSAFE_NO_PADDING)];
        randombytes_buf(random, sizeof(random));
        sodium_bin2base64(encoded, sizeof(encoded), random, sizeof(random), sodium_base64_VARIANT_URLSAFE_NO_PADDING);
        bool ok =
            !string_append_cstr(key, encoded) && lab_runtime_publish_key(configuration, directory, lines, count, key);
        sodium_memzero(random, sizeof(random));
        sodium_memzero(encoded, sizeof(encoded));
        if (!ok) {
            string_free(key);
            key = NULL;
        }
    }
    int error = errno;
    string_split_free(lines, count);
    string_free(separator);
    string_free(text);
    file_free(configuration);
    /* Closing the stable lock inode releases the advisory lock, including on failure. */
    file_free(lock);
    string_free(directory);
    errno = error;
    return key;
}

/* Initialise persistent key material once, then export it for all request workers. */
bool lab_runtime_prepare(void)
{
    if (sodium_init() < 0) {
        errno = EIO;
        return false;
    }
    string_t *base = lab_runtime_home();
    bool default_parent = false;
    string_t *path = base ? lab_runtime_cache_path(base, &default_parent) : NULL;
    string_t *parent = path ? lab_runtime_parent_path(path) : NULL;
    bool ok = parent && lab_runtime_private_directory(parent, default_parent);
    string_t *key = ok ? lab_runtime_environment_text("MARS_LAB_OBJECT_STORE_KEY") : NULL;
    if (key && !string_byte_length(key)) {
        string_free(key);
        key = lab_runtime_configuration_key(base);
    }
    ok = ok && key && string_byte_length(key);
    if (ok)
        ok = setenv("MARS_LAB_OBJECT_STORE_PATH", string_c_str(path), 1) == 0 &&
             setenv("MARS_LAB_OBJECT_STORE_KEY", string_c_str(key), 1) == 0;
    int error = errno;
    string_free(key);
    string_free(parent);
    string_free(path);
    string_free(base);
    errno = error;
    return ok;
}
