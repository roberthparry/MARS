/**
 * @file benchmarks_progress.c
 * @brief Compact benchmark progress with a synchronous worker heartbeat and permanent failure evidence.
 *
 * Terminals use three bounded, replacing rows: a fractional bar, outcome counters and the current case. Captures
 * receive occasional plain-text checkpoints instead. Case counters follow committed database observations, including
 * resumed outcomes. The process observer drives animation without a thread or any work inside the timed worker.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include "checks_process.h"
#define MARS_BENCHMARKS_INTERNAL_ACCESS
#include "benchmarks_internal.h"

struct _benchmark_progress_t {
    benchmark_progress_counts_t counts;
    struct timespec started;
    char operation[96], backend[48], precision[32];
    int bits;
    double timeout, elapsed, case_elapsed, next_display;
    unsigned frame;
    bool terminal, colour, verbose, drawn, active, displayed;
};

static size_t benchmark_progress_completed(const benchmark_progress_t *progress)
{
    const benchmark_progress_counts_t *counts = &progress->counts;
    return counts->passed + counts->failed + counts->timed_out + counts->skipped;
}

static void benchmark_progress_label(char *out, size_t capacity, const char *name)
{
    /* Canonical operation/backend names are ASCII. Bound metadata scanning and neutralise stored control bytes. */
    size_t i = 0;
    for (; name && name[i] && i + 1 < capacity; ++i) {
        unsigned char byte = (unsigned char)name[i];
        out[i] = byte >= 32 && byte < 127 ? (char)byte : '?';
    }
    out[i] = '\0';
}

static void benchmark_progress_clock(benchmark_progress_t *progress)
{
    struct timespec now;
    if (!clock_gettime(CLOCK_MONOTONIC, &now))
        progress->elapsed = (double)(now.tv_sec - progress->started.tv_sec) +
                            (double)(now.tv_nsec - progress->started.tv_nsec) / 1e9;
}

static void benchmark_progress_clear(benchmark_progress_t *progress)
{
    if (progress->drawn) {
        fputs("\r\033[2K\033[1A\r\033[2K\033[1A\r\033[2K", stderr);
        progress->drawn = false;
    }
}

static unsigned benchmark_progress_columns(void)
{
    struct winsize window = {0};
    return !ioctl(STDERR_FILENO, TIOCGWINSZ, &window) && window.ws_col ? window.ws_col : 80;
}

static void benchmark_progress_bar(char *out, unsigned width, double fraction)
{
    static const char *const partial[] = {" ", "▏", "▎", "▍", "▌", "▋", "▊", "▉"};
    unsigned eighths = (unsigned)(fraction * width * 8);
    size_t used = 0;
    for (unsigned i = 0; i < width; ++i) {
        const char *cell = i < eighths / 8 ? "█" : i == eighths / 8 ? partial[eighths % 8] : " ";
        size_t bytes = strlen(cell);
        memcpy(out + used, cell, bytes);
        used += bytes;
    }
    out[used] = '\0';
}

static void benchmark_progress_counts(const benchmark_progress_t *progress, size_t columns)
{
    static const char *const labels[] = {"PASS", "FAIL", "TIMEOUT", "SKIP"};
    static const char *const compact[] = {"P", "F", "T", "S"};
    static const char *const styles[] = {"\033[1;92m", "\033[1;91m", "\033[1;91m", "\033[1;93m"};
    const size_t counts[] = {progress->counts.passed, progress->counts.failed,
                             progress->counts.timed_out, progress->counts.skipped};
    size_t used = 0;
    for (unsigned i = 0; i < 4 && used < columns; ++i) {
        char cell[64];
        int length = snprintf(cell, sizeof cell, "%s%s %zu", i ? "  " : "",
                              columns >= 70 ? labels[i] : compact[i], counts[i]);
        size_t width = (size_t)length < columns - used ? (size_t)length : columns - used;
        if (progress->colour) fputs(styles[i], stderr);
        fprintf(stderr, "%.*s", (int)width, cell);
        if (progress->colour) fputs("\033[0m", stderr);
        used += width;
    }
}

static void benchmark_progress_render(benchmark_progress_t *progress, bool force)
{
    if (progress->verbose) return;
    benchmark_progress_clock(progress);
    if (!force && progress->elapsed < progress->next_display) return;
    progress->next_display = progress->elapsed + (progress->terminal ? .25 : 10);
    size_t completed = benchmark_progress_completed(progress);
    double fraction = progress->counts.total ? (double)completed / progress->counts.total : 0;
    unsigned percent = progress->counts.total ? (unsigned)(100.0L * completed / progress->counts.total) : 0;
    unsigned seconds = (unsigned)progress->elapsed;
    if (progress->terminal) {
        static const char *const frames[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
        unsigned columns = benchmark_progress_columns();
        unsigned width = columns >= 100 ? 24 : columns >= 70 ? 16 : columns >= 50 ? 8 : 0;
        char bar[73], line[256];
        benchmark_progress_bar(bar, width, fraction);
        benchmark_progress_clear(progress);
        if (progress->colour) fputs("\033[1;96m", stderr);
        const char *frame = frames[progress->frame++ % (sizeof frames / sizeof *frames)];
        if (width) fprintf(stderr, "%s [%s] ", frame, bar);
        else fprintf(stderr, "%s ", frame);
        if (progress->colour) fputs("\033[0m", stderr);
        snprintf(line, sizeof line, "%zu/%zu  %u%%  elapsed %02u:%02u", completed, progress->counts.total,
                 percent, seconds / 60, seconds % 60);
        unsigned reserved = width ? width + 6 : 2;
        fprintf(stderr, "%.*s\n", columns > reserved ? (int)(columns - reserved - 1) : 0, line);
        benchmark_progress_counts(progress, columns ? columns - 1 : 0);
        fputc('\n', stderr);
        snprintf(line, sizeof line, "%s / %s / %s - case %.1fs / %.0fs limit", progress->backend,
                 progress->precision, progress->operation, progress->case_elapsed, progress->timeout);
        fprintf(stderr, "%.*s", columns > 1 ? (int)columns - 1 : 0, line);
        progress->drawn = true;
    } else {
        fprintf(stderr, "Benchmarks %zu/%zu (%u%%), elapsed %02u:%02u; ", completed, progress->counts.total,
                percent, seconds / 60, seconds % 60);
        benchmark_progress_counts(progress, SIZE_MAX);
        fprintf(stderr, "; %s / %s / %s, case %.1fs / %.0fs limit\n", progress->backend, progress->precision,
                progress->operation, progress->case_elapsed, progress->timeout);
    }
    progress->displayed = true;
    fflush(stderr);
}

/* Initialise progress from persisted outcomes, including when a run is resumed. */
benchmark_progress_t *benchmark_progress_new(const benchmark_progress_counts_t *counts, double timeout, bool verbose)
{
    if (!counts || !isfinite(timeout) || timeout <= 0) return NULL;
    benchmark_progress_t *progress = calloc(1, sizeof *progress);
    if (!progress) return NULL;
    progress->counts = *counts;
    if (benchmark_progress_completed(progress) > counts->total || clock_gettime(CLOCK_MONOTONIC, &progress->started)) {
        free(progress);
        return NULL;
    }
    const char *term = getenv("TERM"), *disabled = getenv("NO_COLOR");
    progress->terminal = !verbose && isatty(STDERR_FILENO) && (!term || strcmp(term, "dumb"));
    progress->colour = progress->terminal && (!disabled || !*disabled);
    progress->verbose = verbose;
    progress->timeout = timeout;
    return progress;
}

/* Copy the current labels before the worker starts; output remains outside its timing intervals. */
void benchmark_progress_case(benchmark_progress_t *progress, const char *operation, const char *backend, int bits)
{
    if (!progress || !operation || !backend) return;
    bool profile_changed = progress->bits != bits || strcmp(progress->backend, backend);
    benchmark_progress_label(progress->operation, sizeof progress->operation, operation);
    benchmark_progress_label(progress->backend, sizeof progress->backend, backend);
    progress->bits = bits;
    if (bits) snprintf(progress->precision, sizeof progress->precision, "%d bits", bits);
    else snprintf(progress->precision, sizeof progress->precision, "exact");
    progress->case_elapsed = 0;
    progress->active = true;
    if (progress->verbose) {
        fprintf(stderr, "[%zu/%zu] %s / %s / %s ... ", benchmark_progress_completed(progress) + 1,
                progress->counts.total, progress->operation, progress->backend, progress->precision);
        fflush(stderr);
    } else benchmark_progress_render(progress, !progress->displayed || (!progress->terminal && profile_changed));
}

/* The existing subprocess observer supplies periodic heartbeats without introducing another timer or thread. */
void benchmark_progress_observe(const string_t *output_path, double elapsed, void *context)
{
    (void)output_path;
    benchmark_progress_t *progress = context;
    if (!progress || !progress->active) return;
    progress->case_elapsed = elapsed;
    benchmark_progress_render(progress, false);
}

/* Advance only after the caller has committed this case and its samples. */
void benchmark_progress_complete(benchmark_progress_t *progress, const char *outcome)
{
    if (!progress || !progress->active || !outcome) return;
    const char *label;
    bool failure = false;
    if (!strcmp(outcome, "pass")) { ++progress->counts.passed; label = "PASS"; }
    else if (!strcmp(outcome, "unsupported")) { ++progress->counts.skipped; label = "SKIPPED"; }
    else if (!strcmp(outcome, "timeout")) { ++progress->counts.timed_out; label = "TIMEOUT"; failure = true; }
    else if (!strcmp(outcome, "fail")) { ++progress->counts.failed; label = "FAIL"; failure = true; }
    else return;
    if (progress->verbose) fprintf(stderr, "%s\n", label);
    else if (failure) {
        benchmark_progress_clear(progress);
        if (progress->colour) fputs("\033[1;91m", stderr);
        fprintf(stderr, "%s: %s / %s / %s (%.1fs)\n", label, progress->operation, progress->backend,
                progress->precision, progress->case_elapsed);
        if (progress->colour) fputs("\033[0m", stderr);
    }
    progress->active = false;
    fflush(stderr);
}

/* Always leave a permanent final summary with truthful completed and pending counts. */
void benchmark_progress_finish(benchmark_progress_t *progress, bool complete)
{
    if (!progress) return;
    benchmark_progress_clock(progress);
    benchmark_progress_clear(progress);
    const char *state = complete ? "complete" : checks_process_interrupt_signal() ? "interrupted" : "stopped";
    fprintf(stderr, "Benchmarks %s: %zu/%zu; ", state,
            benchmark_progress_completed(progress), progress->counts.total);
    benchmark_progress_counts(progress, SIZE_MAX);
    fprintf(stderr, "; elapsed %.1fs\n", progress->elapsed);
    fflush(stderr);
    free(progress);
}
