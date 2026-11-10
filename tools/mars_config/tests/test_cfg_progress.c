/**
 * @file test_cfg_progress.c
 * @brief Captured counts, live explanations, remaining-time estimates and failure regressions.
 *
 * A child reports to a pipe while blocked in a timed wait. The parent checks
 * plain redirected output and completion without touching installed databases.
 * Real-terminal rendering is also exercised by test_cfg_terminal.c.
 */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
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
        cfg_progress_explain(progress, "Source processing is complete.");
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
        cfg_progress_update_segments(progress, estimate_label, 0, 100, unit, 10);
        cfg_progress_update_segments(progress, estimate_label, 10, 100, unit, 10);
        cfg_progress_update_segments(progress, estimate_label, 20, 100, unit, 10);
        cfg_progress_update_segments(progress, estimate_label, 30, 100, unit, 10);
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
        cfg_progress_update_segments(NULL, "No reporter", 0, 0, "rows copied", 4096);
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
         string_find(output, "; segment 1/10 - 0s") >= 0 && string_find(output, "; segment 4/10 - ") >= 0 &&
         string_find(output, "unchanged") < 0 &&
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

static void test_cfg_progress_terminal_utf8(void)
{
    int master = posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC);
    bool ready = master >= 0 && !grantpt(master) && !unlockpt(master);
    char *slave = ready ? ptsname(master) : NULL;
    fflush(NULL);
    pid_t child = slave ? fork() : -1;
    if (!child) {
        int terminal = open(slave, O_RDWR | O_NOCTTY);
        struct winsize window = {.ws_row = 24, .ws_col = 100};
        if (terminal < 0 || ioctl(terminal, TIOCSWINSZ, &window) || dup2(terminal, STDERR_FILENO) < 0 ||
            setenv("TERM", "xterm", 1))
            _exit(2);
        close(terminal); close(master);
        cfg_progress_t *progress = cfg_progress_start("Photographic fixture");
        if (!progress) _exit(3);
        cfg_progress_update_items(progress, "Photographic fixture", 231, 450, "tiles imported");
        struct timespec pause = {.tv_nsec = 750000000};
        while (nanosleep(&pause, &pause) && errno == EINTR) {}
        cfg_progress_finish(progress, true);
        progress = cfg_progress_start("Failure fixture");
        if (!progress) _exit(4);
        cfg_progress_update_items(progress, "Failure fixture", 7, 10, "tiles imported");
        cfg_progress_finish(progress, false);
        static const char batch_label[] = "Batch fixture", batch_unit[] = "rows copied";
        progress = cfg_progress_start(batch_label);
        if (!progress) _exit(5);
        uint64_t total = UINT64_C(350) * 4096 + 1;
        cfg_progress_update_segments(progress, batch_label, 0, total, batch_unit, 4096);
        string_t *name = string_new_with("Corsica");
        if (!name) _exit(6);
        cfg_progress_segment_name(progress, name);
        string_free(name);
        cfg_progress_explain(progress, "The named batch is processing.");
        cfg_progress_update_segments(progress, batch_label, 4096, total, batch_unit, 4096);
        cfg_progress_explain(progress, "The first batch is complete.");
        cfg_progress_update_segments(progress, batch_label, total - 1, total, batch_unit, 4096);
        cfg_progress_explain(progress, "The shorter final batch is processing.");
        cfg_progress_finish(progress, false);
        _exit(0);
    }
    char bytes[8192];
    size_t used = 0;
    bool ok = child > 0;
    while (ok && used < sizeof bytes - 1) {
        struct pollfd readable = {.fd = master, .events = POLLIN};
        int available = poll(&readable, 1, 5000);
        if (available < 0 && errno == EINTR) continue;
        if (available <= 0) { ok = false; break; }
        ssize_t count = read(master, bytes + used, sizeof bytes - used - 1);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { ok = !count || errno == EIO; break; }
        used += (size_t)count;
    }
    if (master >= 0) close(master);
    if (child > 0 && (!ok || used == sizeof bytes - 1)) kill(child, SIGKILL);
    int status = 0;
    pid_t waited;
    do { waited = child > 0 ? waitpid(child, &status, 0) : -1; } while (waited < 0 && errno == EINTR);
    bytes[used] = '\0';
    string_t *text = string_new();
    ok = ok && waited == child && WIFEXITED(status) && !WEXITSTATUS(status) && text &&
         !string_append_utf8_exact(text, bytes, used) && strstr(bytes, "⠋") && strstr(bytes, "⠙") &&
         strstr(bytes, "[██████████▏         ]") && strstr(bytes, "[█                   ]") &&
         !strstr(bytes, "░") && !strstr(bytes, "▓") && !strstr(bytes, "▒") &&
         strstr(bytes, "\033[32m✓") && strstr(bytes, "\033[31m✗") &&
         strstr(bytes, "231/450 tiles imported 51%") && strstr(bytes, "7/10 tiles imported") &&
         strstr(bytes, "; segment 1/351 - 0s") && strstr(bytes, "; segment 2/351 - 0s") &&
         strstr(bytes, "; segment 1/351 (Corsica) - 0s") && !strstr(bytes, "segment 2/351 (Corsica)") &&
         strstr(bytes, "; segment 351/351 - 0s") && !strstr(bytes, "segment 352/") &&
         !strstr(bytes, "100%");
    if (!ok) fprintf(stderr, "UTF-8 progress transcript: %s\n", bytes);
    string_free(text);
    TEST_ASSERT_TRUE(ok, "terminal bars use valid UTF-8 fractional blocks, animated Braille, truthful counts and "
                         "coloured completion/failure badges");
}

/* Register before the README example groups. */
void test_cfg_progress_cases(void)
{
    TEST_RUN_IN_GROUP(test_cfg_progress_output, tests, NULL);
    TEST_RUN_IN_GROUP(test_cfg_progress_terminal_utf8, tests, NULL);
}
