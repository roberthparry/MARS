/**
 * @file lab_runtime.c
 * @brief Resolve the Lab object-store path and preserve its encrypted configuration key before fork.
 *
 * Startup loads settings from mars.db. An explicitly supplied key remains a diagnostic override;
 * otherwise the settings owner atomically retrieves or creates the persistent key in that database.
 * No environment file, plaintext key file or configuration lock file is read or written.
 * Existing cache contents are untouched. Environment updates require single-threaded startup.
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <errno.h>
#include <pwd.h>
#include <sodium.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "file-t.h"
#include "string-t.h"
#include "mars_settings.h"
#include "lab_runtime.h"

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
    if (!directory || string_byte_length(directory)) return directory;
    string_free(directory);
    struct passwd *account = getpwuid(getuid());
    if (!account || !account->pw_dir || !*account->pw_dir) { errno = ENOENT; return NULL; }
    return string_new_with(account->pw_dir);
}

static string_t *lab_runtime_expand_path(const string_t *path)
{
    if (!string_starts_with(path, "~/") && !string_view_equals_literal(string_view_all(path), "~"))
        return string_clone(path);
    string_t *directory = lab_runtime_user_directory();
    string_t *tail = string_substr(path, 1u, string_byte_length(path) - 1u);
    if (!directory || !tail || string_append_string(directory, tail)) {
        string_free(directory); directory = NULL;
    }
    string_free(tail);
    return directory;
}

static string_t *lab_runtime_home(void)
{
    string_t *configured = lab_runtime_environment_text("MARS_HOME");
    if (!configured) return NULL;
    string_t *path = string_byte_length(configured) ? lab_runtime_expand_path(configured) :
                                                    lab_runtime_user_directory();
    if (path && !string_byte_length(configured) && string_append_cstr(path, "/.mars")) {
        string_free(path); path = NULL;
    }
    string_free(configured);
    return path;
}

static string_t *lab_runtime_parent_path(const string_t *path)
{
    const char *value = path ? string_c_str(path) : "", *slash = strrchr(value, '/');
    return slash ? string_sprintf("%.*s", (int)(slash == value ? 1 : slash - value), value) : string_new_with(".");
}

static bool lab_runtime_private_directory(const string_t *path, bool tighten_existing)
{
    file_t *directory = file_new(path);
    bool ok = directory && file_create_directory(directory, 0700u, true);
    if (ok && tighten_existing) {
        file_info_t *info = file_get_info(directory);
        ok = info && file_info_type(info) == FILE_TYPE_DIRECTORY && file_info_owner(info) == geteuid();
        file_info_free(info);
        if (!ok) errno = EPERM;
        else ok = file_chmod(directory, 0700u);
    }
    int error = errno;
    file_free(directory); errno = error;
    return ok;
}

static string_t *lab_runtime_cache_path(const string_t *base, bool *default_parent)
{
    string_t *override = lab_runtime_environment_text("MARS_LAB_OBJECT_STORE_PATH");
    if (!override) return NULL;
    *default_parent = !string_byte_length(override);
    string_t *path = *default_parent ? string_sprintf("%s/lab/mars_lab_object_store.sqlite3", string_c_str(base)) :
                                     lab_runtime_expand_path(override);
    string_free(override);
    return path;
}

/* Export one stable, database-owned key and resolved path for all subsequently forked workers. */
bool lab_runtime_prepare(void)
{
    if (sodium_init() < 0) { errno = EIO; return false; }
    string_t *base = lab_runtime_home();
    bool default_parent = false;
    string_t *path = base ? lab_runtime_cache_path(base, &default_parent) : NULL;
    string_t *parent = path ? lab_runtime_parent_path(path) : NULL;
    bool ok = parent && lab_runtime_private_directory(parent, default_parent);
    string_t *key = ok ? lab_runtime_environment_text("MARS_LAB_OBJECT_STORE_KEY") : NULL;
    if (key && !string_byte_length(key)) {
        string_free(key);
        key = mars_settings_secret("MARS_LAB_OBJECT_STORE_KEY");
    }
    ok = ok && key && string_byte_length(key);
    if (ok)
        ok = setenv("MARS_LAB_OBJECT_STORE_PATH", string_c_str(path), 1) == 0 &&
             setenv("MARS_LAB_OBJECT_STORE_KEY", string_c_str(key), 1) == 0;
    int error = errno;
    if (key) sodium_memzero((void *)string_c_str(key), string_byte_length(key));
    string_free(key); string_free(parent); string_free(path); string_free(base);
    errno = error;
    return ok;
}
