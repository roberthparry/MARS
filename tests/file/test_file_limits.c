/**
 * @file test_file_limits.c
 * @brief File resource-limit and delayed-error tests.
 *
 * Checks locking handoff, late write failures and preservation of destinations after failed copies. These cases
 * protect failure semantics rather than only successful content output.
 *
 * Used by the project test harness for regression verification. Select cases through tests/test_config.json and
 * run suites sequentially; this source is not part of the installed library.
 */

#include <errno.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include "file.h"
#include "test_harness.h"

/* Pipes synchronise processes; all regular-file operations still use the file API. */
void test_file_blocking_lock_handoff(void)
{
    file_t *file = file_new_cstr(test_case_temp_path("blocking-lock"));
    ASSERT_NOT_NULL(file);
    ASSERT_TRUE(file_create(file));
    ASSERT_TRUE(file_lock(file, true, false));
    int ready[2];
    ASSERT_EQ_INT(pipe(ready), 0);
    pid_t pid = fork();
    if (pid == 0) {
        close(ready[0]);
        alarm(10);
        bool ok = file_close(file) && file_open_read(file);
        if (write(ready[1], "r", 1) != 1)
            ok = false;
        close(ready[1]);
        ok = ok && file_lock(file, false, true);
        ok = ok && file_unlock(file);
        file_free(file);
        _exit(ok ? 0 : 3);
    }
    close(ready[1]);
    char byte;
    ssize_t received = pid > 0 ? read(ready[0], &byte, 1) : -1;
    close(ready[0]);
    int status = 0;
    pid_t before = pid > 0 ? waitpid(pid, &status, WNOHANG) : -1;
    bool released = file_unlock(file);
    pid_t waited = -1;
    if (pid > 0 && before == 0) {
        do {
            waited = waitpid(pid, &status, 0);
        } while (waited < 0 && errno == EINTR);
    }
    file_free(file);
    ASSERT_TRUE(pid > 0);
    ASSERT_EQ_LONG(received, 1);
    ASSERT_EQ_LONG(before, 0);
    ASSERT_TRUE(released);
    ASSERT_EQ_LONG(waited, pid);
    ASSERT_TRUE(WIFEXITED(status));
    ASSERT_EQ_INT(WEXITSTATUS(status), 0);
}

/* Exercise a real delayed write error without changing the parent process's limits or signals. */
void test_file_delayed_write_failure_is_reported(void)
{
    file_t *file = file_new_cstr(test_case_temp_path("limited.txt"));
    ASSERT_NOT_NULL(file);
    pid_t pid = fork();
    if (pid == 0) {
        struct rlimit limit = {8, 8};
        int result = 2;
        if (setrlimit(RLIMIT_FSIZE, &limit) == 0 && signal(SIGXFSZ, SIG_IGN) != SIG_ERR) {
            bool ok = file_write_all_bytes(file, "0123456789abcdef", 16);
            result = !ok && file_last_error(file) == EFBIG && !file_is_open(file) ? 0 : 3;
        }
        file_free(file);
        _exit(result);
    }
    int status = 0;
    pid_t waited = -1;
    if (pid > 0) {
        do {
            waited = waitpid(pid, &status, 0);
        } while (waited < 0 && errno == EINTR);
    }
    bool removed = file_delete(file);
    file_free(file);
    ASSERT_TRUE(pid >= 0);
    ASSERT_EQ_LONG(waited, pid);
    ASSERT_TRUE(WIFEXITED(status));
    ASSERT_EQ_INT(WEXITSTATUS(status), 0);
    ASSERT_TRUE(removed);
}

/* Failed staging must leave the old destination intact and never consume the source. */
void test_file_copy_failure_preserves_destination(void)
{
    file_t *source = file_new_cstr(test_case_temp_path("large-source.txt"));
    file_t *destination = file_new_cstr(test_case_temp_path("preserved.txt"));
    bool prepared = source && destination && file_write_all_bytes(source, "0123456789abcdef", 16) &&
                    file_write_all_bytes(destination, "old", 3);
    pid_t pid = prepared ? fork() : -1;
    if (pid == 0) {
        struct rlimit limit = {8, 8};
        int result = 2;
        if (setrlimit(RLIMIT_FSIZE, &limit) == 0 && signal(SIGXFSZ, SIG_IGN) != SIG_ERR) {
            bool ok = file_copy(source, destination, true);
            result = !ok && file_last_error(source) == EFBIG ? 0 : 3;
        }
        file_free(destination);
        file_free(source);
        _exit(result);
    }
    int status = 0;
    pid_t waited = -1;
    if (pid > 0) {
        do {
            waited = waitpid(pid, &status, 0);
        } while (waited < 0 && errno == EINTR);
    }
    string_t *text = file_read_all_text(destination);
    bool preserved = text && string_length(text) == 3 && string_starts_with(text, "old");
    string_free(text);
    bool source_exists = file_exists(source);
    bool source_removed = file_delete(source);
    bool destination_removed = file_delete(destination);
    file_free(destination);
    file_free(source);
    ASSERT_TRUE(prepared);
    ASSERT_TRUE(pid >= 0);
    ASSERT_EQ_LONG(waited, pid);
    ASSERT_TRUE(WIFEXITED(status));
    ASSERT_EQ_INT(WEXITSTATUS(status), 0);
    ASSERT_TRUE(preserved);
    ASSERT_TRUE(source_exists);
    ASSERT_TRUE(source_removed);
    ASSERT_TRUE(destination_removed);
}
