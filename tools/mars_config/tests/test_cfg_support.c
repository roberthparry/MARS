/**
 * @file test_cfg_support.c
 * @brief Disposable process and filesystem fixtures for native installer tests.
 *
 * Owns only directories created by mkdtemp in /tmp. Runs one child at a time,
 * then removes its bounded tree through file.h without following symlinks.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "file.h"
#include "test_cfg_support.h"
#include "ustring.h"

/* Used only beneath this fixture's successful mkdtemp directory, after its child has exited. */
static bool test_cfg_remove_tree(file_t *directory, unsigned depth, size_t *remaining)
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
        /* Filesystem names are exact bytes: do not compose a decomposed Unicode fixture path. */
        const char *parent = file_path(directory), *name = file_info_name(entry);
        string_t *path = string_new();
        bool joined = path && string_append_utf8_exact(path, parent, strlen(parent)) == 0 &&
                      string_append_utf8_exact(path, "/", 1) == 0 &&
                      string_append_utf8_exact(path, name, strlen(name)) == 0;
        file_t *child = joined ? file_new(path) : NULL;
        bool removed =
            child && (file_info_type(entry) == FILE_TYPE_DIRECTORY ? test_cfg_remove_tree(child, depth + 1, remaining)
                                                                   : file_delete(child));
        ok = removed && ok;
        file_free(child);
        string_free(path);
        file_info_free(entry);
    }
    bool closed = file_close(directory);
    return ok && closed && file_remove_directory(directory);
}

/* Isolate installer environment changes and reclaim only this fixture's tree. */
bool test_cfg_isolated(bool (*callback)(const char *directory))
{
    char directory[] = "/tmp/mars-config-test-XXXXXX";
    if (!mkdtemp(directory))
        return false;
    fflush(NULL);
    pid_t child = fork();
    if (!child) {
        bool ready = !setenv("MARS_HOME", directory, 1) && !unsetenv("MARS_WEATHER_API_KEY") &&
                     !unsetenv("WEATHERAPI_KEY") && !unsetenv("MARS_JURISDICTION_DB_PATH") &&
                     !unsetenv("MARS_JURISDICTION_DB_KEY") && !unsetenv("MARS_ALMANAC_DB_PATH") &&
                     !unsetenv("MARS_ALMANAC_DB_KEY") && !unsetenv("MARS_CALENDAR_LOCATION") &&
                     !unsetenv("MARS_CALENDAR_LANGUAGE");
        bool ok = ready && callback(directory);
        fflush(NULL);
        _exit(ok ? 0 : 1);
    }
    int status = 0;
    pid_t waited = -1;
    if (child > 0) {
        do {
            waited = waitpid(child, &status, 0);
        } while (waited < 0 && errno == EINTR);
    }
    bool ok = waited == child && child > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    file_t *root = file_new_cstr(directory);
    size_t remaining = 2048;
    bool removed = root && test_cfg_remove_tree(root, 0, &remaining);
    file_free(root);
    return ok && removed;
}
