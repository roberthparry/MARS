/**
 * @file cfg_storage.c
 * @brief Private paths and bounded configuration reads for native installers.
 *
 * Resolves MARS_HOME and protects existing user-owned, single-link files before
 * reading them through file.h. Configuration text is handled with string_t.
 * No shell expansion beyond a leading ~/ is performed. Called before threads.
 */
#include <errno.h>
#include <pwd.h>
#include <sodium.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cfg_storage.h"
#include "ustring.h"

static string_t *cfg_storage_environment_text(const char *name)
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

static string_t *cfg_storage_user_directory(void)
{
    string_t *directory = cfg_storage_environment_text("HOME");
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

/* Expand a leading user-home marker, without invoking a shell. */
string_t *cfg_storage_expand_path(const string_t *path)
{
    if (!string_starts_with(path, "~/") && !string_view_equals_literal(string_view_all(path), "~"))
        return string_clone(path);
    string_t *directory = cfg_storage_user_directory();
    string_t *tail = string_substr(path, 1u, string_byte_length(path) - 1u);
    if (!directory || !tail || string_append_string(directory, tail)) {
        string_free(directory);
        directory = NULL;
    }
    string_free(tail);
    return directory;
}

/* Resolve the configured storage root without shell expansion. */
string_t *cfg_storage_home(void)
{
    string_t *configured = cfg_storage_environment_text("MARS_HOME");
    if (!configured)
        return NULL;
    string_t *path;
    if (string_byte_length(configured)) {
        path = cfg_storage_expand_path(configured);
    } else {
        path = cfg_storage_user_directory();
        if (path && string_append_cstr(path, "/.mars")) {
            string_free(path);
            path = NULL;
        }
    }
    string_free(configured);
    return path;
}

/* Create private components and require ownership when tightening an existing directory. */
bool cfg_storage_private_directory(const string_t *path, bool tighten_existing)
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

/* Refuse links, non-regular files and files owned by another user. */
bool cfg_storage_safe_regular(file_t *file, bool *exists)
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

/* Read bounded literal text without leaking file contents in diagnostics. */
string_t *cfg_storage_read_configuration(file_t *file)
{
    bool exists = false;
    if (!cfg_storage_safe_regular(file, &exists))
        return NULL;
    if (!exists)
        return string_new();
    if (!file_chmod(file, 0600u) || !file_open_read(file))
        return NULL;
    const size_t limit = 1024u * 1024u;
    char *buffer = malloc(limit + 1);
    bool ok = buffer != NULL;
    size_t count = 0u, used = 0u;
    while (ok) {
        ok = file_read(file, buffer + used, limit + 1 - used, &count);
        if (!ok || !count)
            break;
        used += count;
        if (used > limit) {
            errno = EFBIG;
            ok = false;
            break;
        }
    }
    int error = errno;
    if (!file_close(file) && ok) {
        error = errno;
        ok = false;
    }
    string_t *text = ok ? string_new() : NULL;
    if (text && string_append_utf8_exact(text, buffer, used)) {
        string_free(text);
        text = NULL;
        error = EILSEQ;
    }
    if (buffer)
        sodium_memzero(buffer, limit + 1);
    free(buffer);
    errno = error;
    return text;
}
