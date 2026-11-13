/**
 * @file checks_test_suite.c
 * @brief Sequential test execution with bounded live progress and retained failure evidence.
 *
 * Observes only newly appended stdout bytes, strips terminal escapes from bounded status lines and reports
 * running cases and completed PASS/FAIL records. Separate suite and module bars use native configuration plans;
 * discovery falls back to activity rather than inventing a total. A monotonic elapsed clock remains
 * visible even while one test is busy; failures are reported immediately rather than at suite completion. The
 * existing process owner handles interruption, descendants and reaping; no monitor thread enters a test process.
 * Final summaries share the executable-label width supplied by Make and right-align suite numbers to the total.
 * Failed output is retained beside the executable, while successful runs remove their previous log.
 */
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "file-t.h"
#include "checks_process.h"

typedef struct _checks_suite_progress_t {
    const char *name;
    file_t *capture;
    char line[256], last[256], current[256];
    size_t length, passed, failed, completed, total;
    size_t suite_index, suite_total;
    unsigned escape;
    bool discard, terminal, total_known, native_progress;
    unsigned drawn_rows;
    double elapsed, next_display, case_started;
    unsigned frame;
} checks_suite_progress_t;

static void checks_suite_duration(double elapsed)
{
    double tenths = floor(elapsed * 10 + .5);
    if (tenths >= 600) {
        double minutes = floor(tenths / 600);
        printf("%.0fm %04.1fs", minutes, (tenths - minutes * 600) / 10);
    } else {
        printf("%.1fs", tenths / 10);
    }
}

static void checks_suite_clear(checks_suite_progress_t *progress)
{
    if (progress->drawn_rows) {
        fputs("\r\033[K", stdout);
        for (unsigned row = 1; row < progress->drawn_rows; ++row)
            fputs("\033[1A\r\033[K", stdout);
        progress->drawn_rows = 0;
    }
}

static void checks_suite_line(checks_suite_progress_t *progress)
{
    progress->line[progress->length] = '\0';
    const char *name = NULL;
    bool failed = false;
    size_t passed_count, failed_count, completed, total;
    if (!progress->discard && !strcmp(progress->line, "PLAN: discovery")) {
        progress->total_known = false;
    } else if (!progress->discard && sscanf(progress->line, "PLAN: %zu tests", &total) == 1) {
        progress->total = total;
        progress->total_known = true;
        progress->next_display = 0;
    } else if (!progress->discard && sscanf(progress->line, "PROGRESS: %zu/%zu", &completed, &total) == 2 &&
               completed <= total) {
        progress->completed = completed;
        progress->total = total;
        progress->total_known = true;
        progress->native_progress = true;
    } else if (!progress->discard && sscanf(progress->line, "COUNTS: %zu passed, %zu failed",
                                     &passed_count, &failed_count) == 2) {
        progress->passed = passed_count;
        progress->failed = failed_count;
    } else if (!progress->discard && !strncmp(progress->line, "RUN: ", 5)) {
        snprintf(progress->current, sizeof progress->current, "%s", progress->line + 5);
        progress->case_started = progress->elapsed;
    } else if (!progress->discard && !strncmp(progress->line, "PASS: ", 6)) {
        ++progress->passed;
        name = progress->line + 6;
    } else if (!progress->discard && !strncmp(progress->line, "FAIL: ", 6)) {
        ++progress->failed;
        name = progress->line + 6;
        failed = true;
    } else if (!progress->discard && !strncmp(progress->line, "OUTPUT PASS: ", 13)) {
        ++progress->passed;
        name = progress->line + 13;
    } else if (!progress->discard && !strncmp(progress->line, "OUTPUT: ", 8)) {
        ++progress->passed;
        name = progress->line + 8;
    } else if (!progress->discard && !strncmp(progress->line, "OUTPUT FAIL: ", 13)) {
        ++progress->failed;
        name = progress->line + 13;
        failed = true;
    }
    if (name) {
        if (!progress->native_progress) progress->completed = progress->passed + progress->failed;
        size_t length = strcspn(name, " \t");
        memcpy(progress->last, name, length);
        progress->last[length] = '\0';
        progress->current[0] = '\0';
        if (failed) {
            checks_suite_clear(progress);
            printf("  \033[1;31mFAIL\033[0m: %s (%s; ", progress->last, progress->name);
            checks_suite_duration(progress->elapsed);
            fputs(" elapsed)\n", stdout);
            fflush(stdout);
        }
    }
    progress->length = 0;
    progress->discard = false;
}

static void checks_suite_bar(size_t completed, size_t total, bool known, unsigned frame)
{
    unsigned complete = total ? (unsigned)(14.0 * (completed < total ? completed : total) / total) : 0;
    unsigned offset = (frame / 2) % 22;
    unsigned first = offset > 11 ? 22 - offset : offset;
    fputs("\033[1;36m[", stdout);
    for (unsigned cell = 0; cell < 14; ++cell)
        fputs((known ? cell < complete : cell >= first && cell < first + 3) ? "█" : " ", stdout);
    fputs("]\033[0m", stdout);
}

static void checks_suite_bytes(checks_suite_progress_t *progress, const unsigned char *bytes, size_t count)
{
    /* Each captured byte is visited once. Long diagnostic/JSON lines never enlarge the status buffer. */
    for (size_t i = 0; i < count; ++i) {
        unsigned char byte = bytes[i];
        if (progress->escape) {
            if (progress->escape == 1)
                progress->escape = byte == '[' ? 2 : 0;
            else if (byte >= 0x40 && byte <= 0x7e)
                progress->escape = 0;
        } else if (byte == 27) {
            progress->escape = 1;
        } else if (byte == '\n' || byte == '\r') {
            checks_suite_line(progress);
        } else if (!progress->discard && byte >= 32 && byte != 127) {
            if (progress->length < sizeof progress->line - 1)
                progress->line[progress->length++] = (char)byte;
            else
                progress->discard = true;
        }
    }
}

static void checks_suite_read(checks_suite_progress_t *progress, size_t limit)
{
    if (!progress->capture || !file_seek(progress->capture, 0, FILE_SEEK_CURRENT))
        return;
    unsigned char bytes[4096];
    for (size_t block = 0; block < limit; ++block) {
        size_t count = 0;
        if (!file_read(progress->capture, bytes, sizeof bytes, &count) || !count)
            break;
        checks_suite_bytes(progress, bytes, count);
    }
}

static void checks_suite_observe(const string_t *path, double elapsed, void *context)
{
    checks_suite_progress_t *progress = context;
    progress->elapsed = elapsed;
    if (!progress->capture) {
        file_t *capture = file_new(path);
        if (capture && file_open_read(capture))
            progress->capture = capture;
        else
            file_free(capture);
    }
    /* Bound live observation so a noisy child cannot hold up status polling or cancellation. */
    checks_suite_read(progress, 16);
    if (elapsed < progress->next_display)
        return;
    progress->next_display = elapsed + (progress->terminal ? .25 : 10);
    checks_suite_clear(progress);
    unsigned seconds = (unsigned)elapsed;
    static const char *const frames[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
    const char *frame = frames[progress->frame++ % (sizeof frames / sizeof *frames)];
    if (progress->terminal) {
        const char *slash = strrchr(progress->name, '/');
        const char *name = slash ? slash + 1 : progress->name;
        struct winsize window = {0};
        unsigned columns = !ioctl(STDOUT_FILENO, TIOCGWINSZ, &window) && window.ws_col ? window.ws_col : 80;
        char fixed[128];
        int reserved = snprintf(fixed, sizeof fixed, "  * Suites [              ]  | %02u:%02u",
                                seconds / 60, seconds % 60);
        if (progress->suite_total)
            reserved += snprintf(fixed, sizeof fixed, " %zu/%zu", progress->suite_index - 1, progress->suite_total);
        int width = columns > (unsigned)reserved + 1 ? (int)columns - reserved - 1 : 0;
        if (width > 28)
            width = 28;
        if (progress->suite_total) {
            printf("  %s Suites ", frame);
            checks_suite_bar(progress->suite_index - 1, progress->suite_total, true, progress->frame);
            printf(" %zu/%zu %.*s │ %02u:%02u\n", progress->suite_index - 1, progress->suite_total,
                   width, name, seconds / 60, seconds % 60);
        }
        printf("  %s Tests  ", frame);
        checks_suite_bar(progress->completed, progress->total, progress->total_known, progress->frame);
        if (progress->total_known) {
            unsigned percent = progress->total ? (unsigned)(100.0 * progress->completed / progress->total) : 0;
            printf(" %zu/%zu %3u%%", progress->completed, progress->total, percent);
        } else fputs(" discovering", stdout);
        printf(" │ \033[1;32m✓ %zu\033[0m │ \033[1;31m✗ %zu\033[0m", progress->passed, progress->failed);
        int remaining = columns > 12 ? (int)columns - 12 : 0;
        if (*progress->current)
            printf("\n  \033[1;36mRunning:\033[0m %.*s", remaining, progress->current);
        else
            printf("\n  Last: %.*s", remaining, *progress->last ? progress->last : "preparing suite");
        progress->drawn_rows = progress->suite_total ? 3 : 2;
    } else {
        printf("  %s [working] %s ... working %02u:%02u │ \033[1;32m✓ %zu\033[0m │ \033[1;31m✗ %zu\033[0m", frame,
               progress->name, seconds / 60, seconds % 60, progress->passed, progress->failed);
        if (progress->suite_total)
            printf("; %zu/%zu suites completed", progress->suite_index - 1, progress->suite_total);
        if (progress->total_known)
            printf("; tests %zu/%zu (%u%%)", progress->completed, progress->total,
                   progress->total ? (unsigned)(100.0 * progress->completed / progress->total) : 0);
        else fputs("; discovering tests", stdout);
        if (*progress->current) {
            printf("; running: %s (", progress->current);
            checks_suite_duration(elapsed - progress->case_started);
            putchar(')');
        } else if (*progress->last)
            printf("; last: %s", progress->last);
    }
    if (!progress->terminal)
        putchar('\n');
    fflush(stdout);
}

/* Keep child execution independent of summary rendering and the caller's shell. */
int checks_test_suite(const string_t *root, int argc, char **argv)
{
    size_t suite_index = 0, suite_total = 0;
    int summary_width = 0;
    if (argc > 0 && argv && argv[0] && !strcmp(argv[0], "--suite-progress")) {
        char *index_end = NULL, *total_end = NULL;
        errno = 0;
        unsigned long long index = argc >= 4 ? strtoull(argv[1], &index_end, 10) : 0;
        unsigned long long total = argc >= 4 ? strtoull(argv[2], &total_end, 10) : 0;
        if (errno || !index || index > total || total > SIZE_MAX || !index_end || *index_end ||
            !total_end || *total_end || argv[1][0] == '-' || argv[2][0] == '-') {
            fputs("test-suite: --suite-progress requires a suite number and total, followed by an executable\n", stderr);
            return 2;
        }
        suite_index = (size_t)index;
        suite_total = (size_t)total;
        argc -= 3;
        argv += 3;
    }
    if (argc > 0 && argv && argv[0] && !strcmp(argv[0], "--summary-width")) {
        char *end = NULL;
        errno = 0;
        unsigned long width = argc >= 3 ? strtoul(argv[1], &end, 10) : 0;
        if (errno || !width || width > 4096 || !end || *end || argv[1][0] == '-') {
            fputs("test-suite: --summary-width requires a width from 1 to 4096, followed by an executable\n", stderr);
            return 2;
        }
        summary_width = (int)width;
        argc -= 2;
        argv += 2;
    }
    if (argc < 1 || !argv || !argv[0] || !*argv[0]) {
        fputs("test-suite: an executable is required\n", stderr);
        return 2;
    }
    checks_strings_t *arguments = checks_strings_new();
    for (int i = 0; i < argc; ++i)
        checks_strings_add(arguments, checks_text(argv[i]));
    checks_suite_progress_t progress = {.name = argv[0], .terminal = isatty(STDOUT_FILENO),
                                        .suite_index = suite_index, .suite_total = suite_total};
    string_t *output = NULL, *errors = NULL;
    int status = -1;
    bool ran = checks_process_run_observed(arguments, root, 0, &output, &errors, &status,
                                           checks_suite_observe, &progress);
    int process_error = errno;
    /* The child has stopped: consume any final burst without rescanning bytes already observed. */
    checks_suite_read(&progress, SIZE_MAX);
    file_free(progress.capture);
    checks_strings_free(arguments);
    bool passed = ran && status == 0;
    string_t *relative = string_sprintf("%s.log", argv[0]);
    string_t *path = relative ? checks_path(root, string_c_str(relative)) : NULL;
    file_t *log = path ? file_new(path) : NULL;
    bool logged = log != NULL;
    if (passed) {
        if (log && file_exists(log))
            logged = file_delete(log);
    } else {
        if (!output)
            output = string_new();
        if (output && errors)
            checks_append(output, errors);
        if (output && !ran)
            string_append_format(output, "\nTest process could not complete: %s\n", strerror(process_error));
        logged = path && output && checks_write(path, output);
    }
    checks_suite_clear(&progress);
    if (progress.suite_total) {
        int digits = snprintf(NULL, 0, "%zu", progress.suite_total);
        printf("  [%*zu/%zu]", digits, progress.suite_index, progress.suite_total);
    }
    printf("  %-*s ... \033[1;%sm%s\033[0m (", summary_width, argv[0],
           passed && logged ? "32" : "31", passed && logged ? "PASS" : "FAIL");
    checks_suite_duration(progress.elapsed);
    printf("; %zu passed, %zu failed)", progress.passed, progress.failed);
    if (progress.total_known) printf("; tests %zu/%zu", progress.completed, progress.total);
    if (!passed && logged)
        printf(" (details: %s)", string_c_str(relative));
    if (!logged)
        fputs(" (cannot update the test log)", stdout);
    putchar('\n');
    fflush(stdout);
    file_free(log);
    string_free(path);
    string_free(relative);
    string_free(output);
    string_free(errors);
    return passed && logged ? 0 : ran && status > 0 ? status : 1;
}
