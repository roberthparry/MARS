/**
 * @file cfg_progress.c
 * @brief Threaded terminal bars and bounded-frequency installation logs.
 *
 * Reports elapsed monotonic time even during blocking SQL or calendar work.
 * Percentages describe only the current source, not an invented installation
 * estimate. Byte and item progress stay below 100 until the caller confirms success.
 * Both reporter forms show the processed and expected counts with their units.
 * Remaining-time estimates use observations from the current source only. Unchanged
 * counts and unmeasured stages are identified explicitly; a reporting heartbeat
 * does not establish that the underlying operation is making progress.
 * No numeric-library or database operations run on the reporting thread.
 */
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include "thread-t.h"
#include "cfg_progress.h"

struct _cfg_progress_t {
    thread_t *thread;
    lock_t *mutex;
    lock_t *worker_mutex;
    lock_t *wake;
    const char *label;
    const char *unit;
    uint64_t completed;
    uint64_t total;
    uint64_t initial_completed;
    uint64_t advances;
    struct timespec started;
    struct timespec source_started;
    struct timespec advanced;
    unsigned tick;
    bool terminal;
};

static double cfg_progress_seconds(const struct timespec *first, const struct timespec *last)
{
    return (double)(last->tv_sec - first->tv_sec) + (double)(last->tv_nsec - first->tv_nsec) / 1e9;
}

static void cfg_progress_remaining(char *text, size_t capacity, long double seconds, bool compact)
{
    if (seconds >= 86400)
        snprintf(text, capacity, "%s", compact ? "; >1d left" : "; over 1 day remaining");
    else if (seconds >= 3600)
        snprintf(text, capacity, compact ? "; ~%uh%um left" : "; ~%uh %um remaining", (unsigned)(seconds / 3600),
                 (unsigned)(seconds / 60) % 60);
    else if (seconds >= 60)
        snprintf(text, capacity, compact ? "; ~%um%us left" : "; ~%um %us remaining",
                 (unsigned)(seconds / 60), (unsigned)seconds % 60);
    else {
        unsigned rounded = (unsigned)(seconds + .5L);
        snprintf(text, capacity, compact ? "; ~%us left" : "; ~%us remaining", rounded ? rounded : 1);
    }
}

static void cfg_progress_render(const cfg_progress_t *progress, bool finished, bool success)
{
    struct timespec now;
    bool timed = !clock_gettime(CLOCK_MONOTONIC, &now);
    double seconds = timed ? cfg_progress_seconds(&progress->started, &now) : 0;
    double source_seconds = timed ? cfg_progress_seconds(&progress->source_started, &now) : 0;
    double unchanged = timed ? cfg_progress_seconds(&progress->advanced, &now) : 0;
    unsigned percent = progress->total ? (unsigned)(100.0L * progress->completed / progress->total) : 0;
    if (percent > 99)
        percent = 99;
    if (finished && success)
        percent = 100;
    char bar[21];
    for (unsigned i = 0; i < 20; ++i)
        bar[i] = progress->total || finished ? (i < percent / 5 ? '=' : ' ')
                                            : (i == progress->tick % 20 ? '>' : ' ');
    if (!finished && progress->total) bar[percent / 5] = "|/-\\"[progress->tick % 4];
    bar[20] = '\0';
    char counter[128] = {0};
    if (progress->unit && progress->total)
        snprintf(counter, sizeof counter, " %" PRIu64 "/%" PRIu64 " %s",
                 progress->completed, progress->total, progress->unit);
    char percentage[8] = {0}, status[64], detail[96] = {0};
    if (progress->total && !finished) snprintf(percentage, sizeof percentage, " %u%%", percent);
    snprintf(status, sizeof status, " — %s (%.0fs)", finished ? (success ? "done" : "failed") : "working", seconds);
    if (!finished && progress->total && progress->completed == progress->total && source_seconds >= 5)
        snprintf(detail, sizeof detail, "%s", progress->terminal ? "; finishing" : "; awaiting stage completion");
    else if (!finished && progress->total && unchanged >= 5)
        snprintf(detail, sizeof detail, progress->terminal ? "; unchanged %.0fs" : "; count unchanged for %.0fs",
                 unchanged);
    else if (!finished && !progress->total && source_seconds >= 5)
        snprintf(detail, sizeof detail, "%s", progress->terminal ? "; no count available" :
                                                                 "; no intermediate count available");
    else if (!finished && progress->total > progress->completed && progress->advances >= 3 && source_seconds >= 3) {
        long double remaining = (long double)(progress->total - progress->completed) * source_seconds /
                                (progress->completed - progress->initial_completed);
        cfg_progress_remaining(detail, sizeof detail, remaining, progress->terminal);
    }
    flockfile(stderr);
    if (progress->terminal)
        fputs("\r\033[2K", stderr);
    struct winsize window = {0};
    int label_width = 200;
    if (progress->terminal && !ioctl(STDERR_FILENO, TIOCGWINSZ, &window) && window.ws_col) {
        /* The status's UTF-8 dash occupies one column; leave one spare column to avoid terminal wrapping. */
        size_t reserved = 23 + strlen(counter) + strlen(percentage) + strlen(status) + strlen(detail) - 2 + 1;
        if (!finished && reserved >= window.ws_col) {
            snprintf(status, sizeof status, " (%.0fs)", seconds);
            reserved = 23 + strlen(counter) + strlen(percentage) + strlen(status) + strlen(detail) + 1;
            if (reserved >= window.ws_col && progress->unit && progress->total) {
                size_t noun = strcspn(progress->unit, " ");
                snprintf(counter, sizeof counter, " %" PRIu64 "/%" PRIu64 " %.*s",
                         progress->completed, progress->total, (int)noun, progress->unit);
                reserved = 23 + strlen(counter) + strlen(percentage) + strlen(status) + strlen(detail) + 1;
            }
            if (reserved >= window.ws_col) {
                detail[0] = '\0';
                reserved = 23 + strlen(counter) + strlen(percentage) + strlen(status) + 1;
            }
        }
        label_width = window.ws_col > reserved ? (int)(window.ws_col - reserved) : 0;
    }
    fprintf(stderr, "[%s] %.*s%s%s%s%s%s", bar, label_width, progress->label, counter, percentage, status, detail,
            !progress->terminal || finished ? "\n" : "");
    fflush(stderr);
    funlockfile(stderr);
}

static void *cfg_progress_run(void *argument)
{
    cfg_progress_t *progress = argument;
    while (!lock_wait_for(progress->wake, 1000)) {
        if (errno != ETIMEDOUT || !lock_wait(progress->worker_mutex))
            break;
        if (!lock_check(progress->wake)) {
            ++progress->tick;
            if (progress->terminal || progress->tick % 5 == 0)
                cfg_progress_render(progress, false, false);
        }
        lock_unlock(progress->worker_mutex);
    }
    return NULL;
}

/* Report immediately, then independently of the installer's blocking work. */
cfg_progress_t *cfg_progress_start(const char *label)
{
    cfg_progress_t *progress = label ? calloc(1, sizeof(*progress)) : NULL;
    if (!progress)
        return NULL;
    progress->label = label;
    const char *term = getenv("TERM");
    progress->terminal = isatty(STDERR_FILENO) && (!term || strcmp(term, "dumb"));
    if (clock_gettime(CLOCK_MONOTONIC, &progress->started))
        goto failed;
    progress->source_started = progress->advanced = progress->started;
    progress->mutex = lock_new(LOCK_MUTEX, 0);
    progress->worker_mutex = progress->mutex ? lock_clone(progress->mutex) : NULL;
    progress->wake = lock_new(LOCK_EVENT, 0);
    if (!progress->mutex || !progress->worker_mutex || !progress->wake)
        goto failed;
    cfg_progress_render(progress, false, false);
    progress->thread = thread_new(cfg_progress_run, progress);
    if (progress->thread)
        return progress;
    fputs("\nLive progress unavailable; installation continues.\n", stderr);
failed:
    lock_free(progress->wake);
    lock_free(progress->worker_mutex);
    lock_free(progress->mutex);
    free(progress);
    return NULL;
}

static void cfg_progress_set(cfg_progress_t *progress, const char *label, uint64_t completed, uint64_t total,
                              const char *unit)
{
    if (!progress || !label)
        return;
    if (!lock_wait(progress->mutex))
        return;
    completed = completed > total ? total : completed;
    bool changed = progress->label != label || progress->unit != unit || progress->total != total ||
                   completed < progress->completed;
    struct timespec now;
    bool timed = !clock_gettime(CLOCK_MONOTONIC, &now);
    if (changed) {
        progress->initial_completed = completed;
        progress->advances = 0;
        progress->source_started = progress->advanced = timed ? now : progress->started;
    } else if (completed > progress->completed) {
        ++progress->advances;
        if (timed) progress->advanced = now;
    }
    progress->label = label;
    progress->unit = unit;
    progress->completed = completed;
    progress->total = total;
    if (changed)
        cfg_progress_render(progress, false, false);
    lock_unlock(progress->mutex);
}

/* All measured sources show their byte counts, replacing any previous item unit. */
void cfg_progress_update(cfg_progress_t *progress, const char *label, uint64_t completed, uint64_t total)
{
    cfg_progress_set(progress, label, completed, total, "bytes processed");
}

/* Count updates are constant-time; the worker never waits for a timer tick. */
void cfg_progress_update_items(cfg_progress_t *progress, const char *label, uint64_t completed, uint64_t total,
                               const char *unit)
{
    if (unit) cfg_progress_set(progress, label, completed, total, unit);
}

/* Leave a durable explanation above the live line without exposing caller paths or SQL. */
void cfg_progress_explain(cfg_progress_t *progress, const char *description)
{
    if (!progress || !description || !lock_wait(progress->mutex)) return;
    flockfile(stderr);
    if (progress->terminal) fputs("\r\033[2K", stderr);
    fprintf(stderr, "%s\n  %s\n", progress->label, description);
    fflush(stderr);
    funlockfile(stderr);
    cfg_progress_render(progress, false, false);
    lock_unlock(progress->mutex);
}

/* Wake rather than waiting for the next tick, including on failure paths. */
void cfg_progress_finish(cfg_progress_t *progress, bool success)
{
    if (!progress)
        return;
    lock_signal(progress->wake);
    if (!thread_join(progress->thread, NULL))
        return;
    thread_free(progress->thread);
    cfg_progress_render(progress, true, success);
    lock_free(progress->wake);
    lock_free(progress->worker_mutex);
    lock_free(progress->mutex);
    free(progress);
}
