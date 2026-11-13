/**
 * @file checks_readme_progress.c
 * @brief One replacing README progress line, driven synchronously by compiler and programme observations.
 *
 * A monotonic clock covers every selected example and final evidence writing. Terminals refresh at most four
 * times per second; redirected output receives ten-second checkpoints and one final summary. Failure diagnostics
 * can clear the live row without being overwritten. This display creates no threads, files or persistent cache.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include "checks_support.h"
#define MARS_CHECKS_README_INTERNAL_ACCESS
#include "checks_readme_internal.h"

struct _checks_readme_progress_t {
    size_t total, completed, failed;
    struct timespec started;
    double next_display;
    unsigned frame;
    bool terminal, drawn;
};

/* Keep the clock independent of subprocess timing and wall-clock adjustments. */
static double checks_readme_progress_seconds(const checks_readme_progress_t *progress)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now)) checks_fatal("reading README elapsed time");
    return (double)(now.tv_sec - progress->started.tv_sec) + (double)(now.tv_nsec - progress->started.tv_nsec) / 1e9;
}

static void checks_readme_progress_duration(double elapsed)
{
    double tenths = floor(elapsed * 10 + .5), minutes = floor(tenths / 600);
    if (minutes > 0) printf("%.0fm %04.1fs", minutes, (tenths - minutes * 600) / 10);
    else printf("%.1fs", tenths / 10);
}

/* Clear only the live terminal row; permanent diagnostics and final summaries are never erased. */
void checks_readme_progress_clear(checks_readme_progress_t *progress)
{
    if (progress->drawn) {
        fputs("\r\033[K", stdout);
        progress->drawn = false;
    }
}

static void checks_readme_progress_render(checks_readme_progress_t *progress, bool finished, bool success)
{
    double elapsed = checks_readme_progress_seconds(progress);
    if (!finished && elapsed < progress->next_display && (!progress->terminal || progress->drawn)) return;
    progress->next_display = elapsed + (progress->terminal ? .25 : 10);
    struct winsize window = {0};
    unsigned columns = progress->terminal && !ioctl(STDOUT_FILENO, TIOCGWINSZ, &window) && window.ws_col ?
                       window.ws_col : 80;
    unsigned width = columns >= 110 ? 20 : columns >= 90 ? 14 : columns >= 55 ? 8 : 4;
    unsigned filled = progress->total ? (unsigned)((double)progress->completed * width / progress->total) :
                                       finished ? width : 0;
    unsigned percent = progress->total ? (unsigned)(100.0 * progress->completed / progress->total) :
                                        finished ? 100 : 0;
    static const char *const frames[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
    const char *frame = finished ? (success ? "✓" : "✗") : frames[progress->frame++ % 10];
    checks_readme_progress_clear(progress);
    printf("  %s README \033[1;36m[", frame);
    for (unsigned cell = 0; cell < width; ++cell) fputs(cell < filled ? "█" : " ", stdout);
    printf("]\033[0m %zu/%zu %3u%% ", progress->completed, progress->total, percent);
    if (finished) {
        const char *state = !success ? "FAIL" : progress->total ? "PASS" : "SKIPPED";
        const char *colour = !success ? "31" : progress->total ? "32" : "33";
        printf("\033[1;%sm%s\033[0m (", colour, state);
    } else fputs("elapsed ", stdout);
    checks_readme_progress_duration(elapsed);
    if (finished) printf("; %zu passed, %zu failed)", progress->completed - progress->failed, progress->failed);
    if (finished || !progress->terminal) putchar('\n');
    else progress->drawn = true;
    fflush(stdout);
}

/* Allocate one display for the selected batch; disabled cases never enlarge its denominator. */
checks_readme_progress_t *checks_readme_progress_new(size_t total)
{
    checks_readme_progress_t *progress = calloc(1, sizeof *progress);
    if (!progress) checks_fatal("allocating README progress");
    progress->total = total;
    progress->terminal = isatty(STDOUT_FILENO);
    if (clock_gettime(CLOCK_MONOTONIC, &progress->started)) checks_fatal("starting README elapsed time");
    checks_readme_progress_render(progress, false, false);
    return progress;
}

/* Retain only counters; a successful example needs no separate terminal row. */
void checks_readme_progress_update(checks_readme_progress_t *progress, size_t completed, size_t failed)
{
    progress->completed = completed;
    progress->failed = failed;
    checks_readme_progress_render(progress, false, false);
}

/* The process owner provides the heartbeat; this callback neither reads capture files nor changes deadlines. */
void checks_readme_progress_observe(const string_t *output_path, double elapsed, void *context)
{
    (void)output_path;
    (void)elapsed;
    checks_readme_progress_render(context, false, false);
}

/* Include final evidence writing and fixture cleanup in the elapsed duration. */
void checks_readme_progress_finish(checks_readme_progress_t *progress, bool success)
{
    checks_readme_progress_render(progress, true, success);
}

/* Release a display without removing a summary that has already ended with a newline. */
void checks_readme_progress_free(checks_readme_progress_t *progress)
{
    if (!progress) return;
    checks_readme_progress_clear(progress);
    free(progress);
}
