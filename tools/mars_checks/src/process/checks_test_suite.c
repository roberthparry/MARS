/**
 * @file checks_test_suite.c
 * @brief Sequential test execution with bounded live progress and retained failure evidence.
 *
 * Observes only newly appended stdout bytes, strips terminal escapes from bounded status lines and reports
 * completed PASS/FAIL records. A monotonic elapsed clock remains visible even while one test is busy. The
 * existing process owner handles interruption, descendants and reaping; no monitor thread enters a test process.
 * Failed output is retained beside the executable, while successful runs remove their previous log.
 */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "file-t.h"
#include "checks_process.h"

typedef struct _checks_suite_progress_t {
    const char *name;
    file_t *capture;
    char line[256], last[256];
    size_t length, passed, failed;
    unsigned escape;
    bool discard, terminal;
    double elapsed, next_display;
    unsigned frame;
} checks_suite_progress_t;

static void checks_suite_line(checks_suite_progress_t *progress)
{
    progress->line[progress->length] = '\0';
    const char *name = NULL;
    if (!progress->discard && !strncmp(progress->line, "PASS: ", 6)) {
        ++progress->passed;
        name = progress->line + 6;
    } else if (!progress->discard && !strncmp(progress->line, "FAIL: ", 6)) {
        ++progress->failed;
        name = progress->line + 6;
    }
    if (name) {
        size_t length = strcspn(name, " \t");
        memcpy(progress->last, name, length);
        progress->last[length] = '\0';
    }
    progress->length = 0;
    progress->discard = false;
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
    if (progress->terminal)
        fputs("\r\033[K", stdout);
    unsigned seconds = (unsigned)elapsed;
    static const char *const frames[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
    const char *frame = frames[progress->frame++ % (sizeof frames / sizeof *frames)];
    if (progress->terminal) {
        const char *slash = strrchr(progress->name, '/');
        const char *name = slash ? slash + 1 : progress->name;
        struct winsize window = {0};
        unsigned columns = !ioctl(STDOUT_FILENO, TIOCGWINSZ, &window) && window.ws_col ? window.ws_col : 80;
        char fixed[128];
        int reserved = snprintf(fixed, sizeof fixed, "  *  | %02u:%02u | + %zu | - %zu",
                                seconds / 60, seconds % 60, progress->passed, progress->failed);
        int width = columns > (unsigned)reserved + 1 ? (int)columns - reserved - 1 : 0;
        if (width > 32)
            width = 32;
        printf("  %s %.*s │ %02u:%02u │ \033[32m✓ %zu\033[0m │ \033[31m✗ %zu\033[0m", frame, width, name,
               seconds / 60, seconds % 60, progress->passed, progress->failed);
        int used = reserved + (strlen(name) < (size_t)width ? (int)strlen(name) : width);
        int remaining = (int)columns - used - 4;
        if (*progress->last && remaining >= 12)
            printf(" │ %.*s", remaining, progress->last);
    } else {
        printf("  %s %s ... working %02u:%02u │ \033[32m✓ %zu\033[0m │ \033[31m✗ %zu\033[0m", frame,
               progress->name, seconds / 60, seconds % 60, progress->passed, progress->failed);
        if (*progress->last)
            printf("; last: %s", progress->last);
    }
    if (!progress->terminal)
        putchar('\n');
    fflush(stdout);
}

/* Keep child execution independent of summary rendering and the caller's shell. */
int checks_test_suite(const string_t *root, int argc, char **argv)
{
    if (argc < 1 || !argv[0] || !*argv[0]) {
        fputs("test-suite: an executable is required\n", stderr);
        return 2;
    }
    checks_strings_t *arguments = checks_strings_new();
    for (int i = 0; i < argc; ++i)
        checks_strings_add(arguments, checks_text(argv[i]));
    checks_suite_progress_t progress = {.name = argv[0], .terminal = isatty(STDOUT_FILENO)};
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
    if (progress.terminal)
        fputs("\r\033[K", stdout);
    printf("  %s ... \033[%sm%s\033[0m (%.1fs; %zu passed, %zu failed)", argv[0],
           passed && logged ? "32" : "31", passed && logged ? "PASS" : "FAIL",
           progress.elapsed, progress.passed, progress.failed);
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
