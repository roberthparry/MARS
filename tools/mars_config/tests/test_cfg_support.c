/**
 * @file test_cfg_support.c
 * @brief Disposable process and filesystem fixtures for native installer tests.
 *
 * Owns only private directories created beneath TMPDIR, or the project's build directory by default.
 * Large database fixtures therefore use the build filesystem rather than memory-backed system temporary space.
 * Runs one child at a time,
 * then removes its bounded tree through the file module without following symlinks.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "string-t.h"
#include "file-t.h"
#include "mars_settings.h"
#include "cfg_command.h"
#include "test_cfg_support.h"

static string_t *test_cfg_key(bool create)
{
    (void)create;
    return string_new_with("isolated-installer-settings-key");
}

/* Subprocess commands use the production dispatcher and a process-local synthetic key source. */
void test_cfg_command_self_fixture(void)
{
    if (!getenv("MARS_TEST_CONFIG_COMMAND")) return;
    file_t *file = file_new_cstr("/proc/self/cmdline");
    char input[16384];
    size_t length = 0;
    bool ok = file && file_open_read(file) && file_read(file, input, sizeof input, &length) &&
              length && length < sizeof input && !input[length - 1];
    char *arguments[64];
    size_t count = 0;
    for (size_t i = 0; ok && i < length;) {
        if (count == sizeof arguments / sizeof *arguments) { ok = false; break; }
        arguments[count++] = input + i;
        i += strlen(input + i) + 1;
    }
    file_free(file);
    mars_settings_key_provider(test_cfg_key);
    int status = ok ? cfg_command_run((int)count, arguments) : 2;
    mars_settings_isolation_close();
    fflush(NULL);
    _exit(status);
}

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
    const char *temporary = getenv("TMPDIR");
    string_t *parent = string_new_with(temporary && *temporary ? temporary : MARS_CONFIG_ROOT_DIR "/build");
    file_t *root = parent ? file_create_temp_directory(parent) : NULL;
    string_free(parent);
    if (!root)
        return false;
    const char *directory = file_path(root);
    fflush(NULL);
    pid_t child = fork();
    if (!child) {
        mars_settings_key_provider(test_cfg_key);
        bool ready = !unsetenv("MARS_CONFIG_FILE") && !setenv("MARS_TEST_CONFIG_COMMAND", "1", 1) &&
                     !setenv("MARS_HOME", directory, 1) && !unsetenv("MARS_WEATHER_API_KEY") &&
                     !unsetenv("WEATHERAPI_KEY") && !unsetenv("MARS_JURISDICTION_DB_PATH") &&
                     !unsetenv("MARS_GLOBE_DB_PATH") && !unsetenv("MARS_GLOBE_DB_KEY") &&
                     !unsetenv("MARS_JURISDICTION_DB_KEY") && !unsetenv("MARS_ALMANAC_DB_PATH") &&
                     !unsetenv("MARS_ALMANAC_DB_KEY") && !unsetenv("MARS_CALENDAR_LOCATION") &&
                     !unsetenv("MARS_CALENDAR_LANGUAGE");
        bool ok = ready && callback(directory);
        mars_settings_isolation_close();
        /* Release the child's inherited handle; only the parent removes the directory. */
        file_free(root);
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
    size_t remaining = 2048;
    bool removed = root && test_cfg_remove_tree(root, 0, &remaining);
    file_free(root);
    return ok && removed;
}

static bool (*test_cfg_content_callback)(const char *);

static bool test_cfg_content_child(const char *directory)
{
    return mars_settings_isolate() && test_cfg_content_callback(directory);
}

/* Keep small encrypted settings allocations out of bulk plaintext fixture processes. */
bool test_cfg_content_isolated(bool (*callback)(const char *directory))
{
    test_cfg_content_callback = callback;
    bool ok = callback && test_cfg_isolated(test_cfg_content_child);
    test_cfg_content_callback = NULL;
    return ok;
}
