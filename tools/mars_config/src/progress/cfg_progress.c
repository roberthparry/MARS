/**
 * @file cfg_progress.c
 * @brief Threaded terminal bars and bounded-frequency installation logs.
 *
 * Reports elapsed monotonic time even during blocking SQL or calendar work.
 * Percentages describe only the current source, not an invented installation
 * estimate. Byte progress stays below 100 until the caller confirms success.
 * No numeric-library or database operations run on the reporting thread.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include "thread-t.h"
#include "cfg_progress.h"

struct cfg_progress {
    thread_t *thread;
    lock_t *mutex;
    lock_t *worker_mutex;
    lock_t *wake;
    const char *label;
    uint64_t completed;
    uint64_t total;
    struct timespec started;
    unsigned tick;
    bool terminal;
};

static void cfg_progress_render(const cfg_progress_t *progress, bool finished, bool success)
{
    struct timespec now;
    double seconds = 0;
    if (!clock_gettime(CLOCK_MONOTONIC, &now))
        seconds = (double)(now.tv_sec - progress->started.tv_sec) +
                  (double)(now.tv_nsec - progress->started.tv_nsec) / 1e9;
    unsigned percent = progress->total ? (unsigned)(100.0L * progress->completed / progress->total) : 0;
    if (percent > 99)
        percent = 99;
    if (finished && success)
        percent = 100;
    char bar[21];
    for (unsigned i = 0; i < 20; ++i)
        bar[i] = progress->total || finished ? (i < percent / 5 ? '=' : ' ')
                                            : (i == progress->tick % 20 ? '>' : ' ');
    bar[20] = '\0';
    flockfile(stderr);
    if (progress->terminal)
        fputs("\r\033[2K", stderr);
    struct winsize window = {0};
    int label_width = 200;
    if (progress->terminal && !ioctl(STDERR_FILENO, TIOCGWINSZ, &window) && window.ws_col)
        label_width = window.ws_col > 50 ? window.ws_col - 50 : 8;
    fprintf(stderr, "[%s] %.*s", bar, label_width, progress->label);
    if (progress->total && !finished)
        fprintf(stderr, " %u%%", percent);
    fprintf(stderr, " — %s (%.0fs)%s", finished ? (success ? "done" : "failed") : "working",
            seconds, !progress->terminal || finished ? "\n" : "");
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

/* Updates are constant-time; the worker never waits for a timer tick. */
void cfg_progress_update(cfg_progress_t *progress, const char *label, uint64_t completed, uint64_t total)
{
    if (!progress || !label)
        return;
    if (!lock_wait(progress->mutex))
        return;
    bool changed = progress->label != label;
    progress->label = label;
    progress->completed = completed > total ? total : completed;
    progress->total = total;
    if (changed)
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
