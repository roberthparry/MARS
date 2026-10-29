/**
 * @file test_cfg_progress.c
 * @brief Captured counts, live explanations, remaining-time estimates and failure regressions.
 *
 * A child reports to a pipe while blocked in a timed wait. The parent checks
 * plain redirected output and completion without touching installed databases.
 * Real-terminal rendering is also exercised by test_cfg_terminal.c.
 */
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "string-t.h"
#include "cfg_progress.h"
#include "test_harness.h"
#include "test_cfg_support.h"

static void test_cfg_progress_output(void)
{
    int channel[2];
    TEST_ASSERT_TRUE(pipe(channel) == 0, "create progress capture pipe");
    fflush(NULL);
    pid_t child = fork();
    if (child == 0) {
        close(channel[0]);
        if (dup2(channel[1], STDERR_FILENO) < 0)
            _exit(2);
        close(channel[1]);
        cfg_progress_t *progress = cfg_progress_start("Import fixture");
        if (!progress)
            _exit(3);
        cfg_progress_update(progress, "SQL fixture", UINT64_MAX, UINT64_MAX);
        struct timespec remaining = {.tv_sec = 6};
        while (nanosleep(&remaining, &remaining) && errno == EINTR) {}
        cfg_progress_finish(progress, true);
        progress = cfg_progress_start("Tile fixture");
        if (!progress)
            _exit(4);
        cfg_progress_update_items(progress, "Tile fixture", 231, 450, "tiles imported");
        cfg_progress_update_items(progress, "Tile fixture", UINT64_MAX, 450, "tiles imported");
        cfg_progress_update_items(progress, "Finalising tiles", 450, 450, "tiles imported");
        cfg_progress_finish(progress, true);
        static const char estimate_label[] = "Estimate fixture", unit[] = "records processed";
        progress = cfg_progress_start(estimate_label);
        if (!progress)
            _exit(7);
        cfg_progress_update_items(progress, estimate_label, 0, 100, unit);
        cfg_progress_update_items(progress, estimate_label, 10, 100, unit);
        cfg_progress_update_items(progress, estimate_label, 20, 100, unit);
        cfg_progress_update_items(progress, estimate_label, 30, 100, unit);
        remaining = (struct timespec){.tv_sec = 3};
        while (nanosleep(&remaining, &remaining) && errno == EINTR) {}
        cfg_progress_explain(progress, "Individual records can take longer than others.");
        remaining = (struct timespec){.tv_sec = 3};
        while (nanosleep(&remaining, &remaining) && errno == EINTR) {}
        cfg_progress_explain(progress, "Waiting for the next completed record.");
        cfg_progress_finish(progress, false);
        progress = cfg_progress_start("Failed fixture");
        if (!progress)
            _exit(5);
        cfg_progress_update_items(progress, "Failed fixture", 7, 10, "tiles imported");
        cfg_progress_finish(progress, false);
        progress = cfg_progress_start("Byte fixture");
        if (!progress)
            _exit(6);
        cfg_progress_update_items(progress, "Byte fixture", 1, 2, "items processed");
        cfg_progress_update(progress, "Byte fixture", 15, 20);
        cfg_progress_finish(progress, false);
        cfg_progress_update(NULL, "No reporter", 0, 0);
        cfg_progress_update_items(NULL, "No reporter", 0, 0, "tiles imported");
        cfg_progress_explain(NULL, "No reporter");
        cfg_progress_finish(NULL, false);
        _exit(0);
    }
    close(channel[1]);
    char bytes[8192];
    size_t used = 0;
    bool ok = child > 0;
    while (ok && used < sizeof(bytes) - 1) {
        struct pollfd readable = {.fd = channel[0], .events = POLLIN};
        int ready = poll(&readable, 1, 10000);
        if (ready < 0 && errno == EINTR)
            continue;
        if (ready <= 0) {
            ok = false;
            break;
        }
        ssize_t count = read(channel[0], bytes + used, sizeof(bytes) - 1 - used);
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0) {
            ok = count == 0;
            break;
        }
        used += (size_t)count;
    }
    close(channel[0]);
    if (child > 0 && (!ok || used == sizeof(bytes) - 1))
        kill(child, SIGKILL);
    int status = 0;
    pid_t waited;
    do {
        waited = child > 0 ? waitpid(child, &status, 0) : -1;
    } while (waited < 0 && errno == EINTR);
    bytes[used] = '\0';
    string_t *output = string_new_with(bytes);
    static const char byte_working[] = "SQL fixture 18446744073709551615/18446744073709551615 "
                                       "bytes processed 99% — working";
    string_offset_t first = output ? string_find(output, byte_working) : -1;
    string_view_t rest = first >= 0 ? string_view_slice(string_view_all(output), (size_t)first + 1,
                                                       string_byte_length(output) - (size_t)first - 1)
                                    : string_view_empty();
    string_t *later = string_from_view(&rest);
    ok = ok && child > 0 && waited == child && WIFEXITED(status) && WEXITSTATUS(status) == 0 && output &&
         later && string_find(later, byte_working) >= 0 &&
         string_find(output, "SQL fixture 18446744073709551615/18446744073709551615 bytes processed — done") >= 0 &&
         string_find(output, "Tile fixture 231/450 tiles imported 51% — working") >= 0 &&
         string_find(output, "Finalising tiles 450/450 tiles imported 99% — working") >= 0 &&
         string_find(output, "Finalising tiles 450/450 tiles imported — done") >= 0 &&
         string_find(output, "awaiting stage completion") >= 0 &&
         string_find(output, "Estimate fixture 30/100 records processed 30% — working") >= 0 &&
         string_find(output, "remaining") >= 0 &&
         string_find(output, "count unchanged for") >= 0 &&
         string_find(output, "Individual records can take longer than others.") >= 0 &&
         string_find(output, "Waiting for the next completed record.") >= 0 &&
         string_find(output, "Failed fixture 7/10 tiles imported — failed") >= 0 &&
         string_find(output, "Byte fixture 15/20 bytes processed — failed") >= 0 &&
         string_find(output, "15/20 items processed") < 0 &&
         string_find(output, "10/10 tiles imported") < 0 &&
         string_find(output, "100%") < 0 && string_find(output, "\r") < 0 && string_find(output, "\033") < 0;
    if (!ok)
        string_printf("Progress transcript: %S\n", output);
    string_free(later);
    string_free(output);
    TEST_ASSERT_TRUE(ok, "live reporting explains pauses, estimates remaining work and preserves truthful counts and failure");
}

/* Register before the README example groups. */
void test_cfg_progress_cases(void)
{
    TEST_RUN_IN_GROUP(test_cfg_progress_output, tests, NULL);
}
