/**
 * @file test_lab_support.c
 * @brief JSON assertions and private filesystem fixtures for native Lab tests.
 *
 * Isolates environment changes in a forked child so tests cannot read or overwrite
 * the user's worksheets. The parent reaps each fixture and removes its private
 * tree through the file module, without following symbolic links. Traversal is
 * bounded by depth and entry count. No cases are run concurrently.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "file.h"
#include "sqlite.h"
#include "ustring.h"

#include "test_lab_support.h"

/* Parse a fixture literal using the public JSON API. */
json_t *test_lab_json(const char *text)
{
    string_t *source = string_new_with(text);
    json_t *result = source ? json_from_text(source) : NULL;
    string_free(source);
    return result;
}

/* Borrow a keyed value without retaining a temporary key. */
const json_t *test_lab_member(const json_t *object, const char *key)
{
    string_t *name = string_new_with(key);
    const json_t *result = name ? json_object_get(object, name) : NULL;
    string_free(name);
    return result;
}

/* Borrow response text for immediate assertions. */
const char *test_lab_text(const json_t *object, const char *key)
{
    const string_t *value = json_string_value(test_lab_member(object, key));
    return value ? string_c_str(value) : "";
}

/* Require a boolean response flag, rather than treating missing values as false. */
bool test_lab_ok(const json_t *object, bool expected)
{
    bool value = !expected;
    return json_bool_value(test_lab_member(object, "ok"), &value) && value == expected;
}

/* Used only beneath this fixture's private directory, after its child has exited. */
static bool remove_fixture_tree(file_t *directory, unsigned depth, size_t *remaining)
{
    if (depth > 8 || !file_open_directory(directory))
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
        bool removed =
            child && (file_info_type(entry) == FILE_TYPE_DIRECTORY ? remove_fixture_tree(child, depth + 1, remaining)
                                                                   : file_delete(child));
        ok = removed && ok;
        file_free(child);
        string_free(path);
        file_info_free(entry);
    }
    bool closed = file_close(directory);
    return ok && closed && file_remove_directory(directory);
}

/* Install only the small private SQL fixture; never copy or alter the user's database. */
bool test_lab_catalogue_database(const char *directory)
{
    string_t *path = string_sprintf("%s/jurisdiction.db", directory);
    string_t *key = string_new_with("lab-catalogue-fixture-key");
    file_t *source = file_new_cstr("tools/mars_lab/tests/fixtures/jurisdiction.sql");
    string_t *sql = source ? file_read_all_text(source) : NULL;
    sqlite_t *db = path && key ? sqlite_open_encrypted(path, key) : NULL;
    bool ok = db && sql && sqlite_exec(db, sql);
    sqlite_close(db);
    if (ok)
        ok = !setenv("MARS_JURISDICTION_DB_PATH", string_c_str(path), 1) &&
             !setenv("MARS_JURISDICTION_DB_KEY", string_c_str(key), 1);
    file_free(source);
    string_free(sql);
    string_free(key);
    string_free(path);
    return ok;
}

/* Isolate state and environment changes in a disposable, sequential child fixture. */
bool test_lab_isolated(bool (*callback)(const char *directory))
{
    const char *temporary = getenv("TMPDIR");
    string_t *parent = string_new_with(temporary && *temporary ? temporary : "/tmp");
    file_t *root = parent ? file_create_temp_directory(parent) : NULL;
    string_free(parent);
    if (!root)
        return false;
    const char *directory = file_path(root);
    pid_t child = fork();
    if (!child) {
        char *root = getcwd(NULL, 0);
        string_t *state = string_sprintf("%s/state.json", directory);
        string_t *asset = root ? string_sprintf("%s/tools/mars_lab/assets/index.html", root) : NULL;
        bool ready = state && asset && !setenv("MARS_HOME", directory, 1) &&
                     !setenv("MARS_LAB_STATE_FILE", string_c_str(state), 1) &&
                     !setenv("MARS_LAB_ASSET_FILE", string_c_str(asset), 1) &&
                     !setenv("MARS_LAB_APP_NAME", "MARS Lab", 1) && !unsetenv("MARS_LAB_SUBTITLE") &&
                     !unsetenv("MARS_LAB_CONTROL_QUERY_PARAM");
        string_free(state);
        string_free(asset);
        free(root);
        _exit(ready && callback(directory) ? 0 : 1);
    }
    int status = 0;
    pid_t waited = -1;
    if (child > 0) {
        do {
            waited = waitpid(child, &status, 0);
        } while (waited < 0 && errno == EINTR);
    }
    bool ok = waited == child && child > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    if (!ok)
        string_fprintf(stderr, "Lab isolated fixture: child=%ld, waited=%ld, wait status=%d\n", (long)child,
                       (long)waited, status);
    size_t remaining = 1024;
    bool removed = root && remove_fixture_tree(root, 0, &remaining);
    if (!removed)
        string_fprintf(stderr, "Lab private fixture cleanup failed: %s, errno=%d\n", directory, errno);
    file_free(root);
    return removed && ok;
}
