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
#include "string-t.h"
#include "cfg_progress.h"

struct _cfg_progress_t {
    thread_t *thread;
    lock_t *mutex;
    lock_t *worker_mutex;
    lock_t *wake;
    const char *label;
    const char *unit;
    string_t *segment_name;
    uint64_t completed;
    uint64_t total;
    uint64_t segment_size;
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

static void cfg_progress_bar(const cfg_progress_t *progress, unsigned percent, bool finished, char bar[61])
{
    static const char *const fractions[] = {" ", "▏", "▎", "▍", "▌", "▋", "▊", "▉"};
    size_t used = 0;
    unsigned eighths = percent * 160 / 100;
    for (unsigned column = 0; column < 20; ++column) {
        if (!progress->terminal) {
            bar[used++] = progress->total || finished ? (column < percent / 5 ? '=' : ' ')
                                                      : (column == progress->tick % 20 ? '>' : ' ');
            continue;
        }
        const char *cell = " ";
        if (progress->total || finished) {
            if (column < eighths / 8)
                cell = "█";
            else if (column == eighths / 8)
                cell = fractions[eighths % 8];
        } else if (column == progress->tick % 20) {
            cell = "█";
        }
        size_t bytes = strlen(cell);
        memcpy(bar + used, cell, bytes);
        used += bytes;
    }
    if (!progress->terminal && !finished && progress->total)
        bar[percent / 5] = "|/-\\"[progress->tick % 4];
    bar[used] = '\0';
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
    char bar[61];
    cfg_progress_bar(progress, percent, finished, bar);
    char counter[128] = {0};
    if (progress->unit && progress->total)
        snprintf(counter, sizeof counter, " %" PRIu64 "/%" PRIu64 " %s",
                 progress->completed, progress->total, progress->unit);
    char percentage[8] = {0}, status[64], detail[256] = {0};
    if (progress->total && !finished) snprintf(percentage, sizeof percentage, " %u%%", percent);
    snprintf(status, sizeof status, " — %s (%.0fs)", finished ? (success ? "done" : "failed") : "working", seconds);
    if (!finished && progress->total && progress->completed == progress->total && source_seconds >= 5)
        snprintf(detail, sizeof detail, "%s", progress->terminal ? "; finishing" : "; awaiting stage completion");
    else if (!finished && progress->total > progress->completed && progress->segment_size) {
        uint64_t count = progress->total / progress->segment_size + (progress->total % progress->segment_size != 0);
        uint64_t current = progress->completed / progress->segment_size + 1;
        const char *name = progress->segment_name ? string_c_str(progress->segment_name) : NULL;
        snprintf(detail, sizeof detail, "; segment %" PRIu64 "/%" PRIu64 "%s%s%s - %.0fs", current, count,
                 name ? " (" : "", name ? name : "", name ? ")" : "", unchanged);
    } else if (!finished && progress->total && unchanged >= 5)
        snprintf(detail, sizeof detail, "; segment - %.0fs", unchanged);
    else if (!finished && !progress->total && source_seconds >= 5)
        snprintf(detail, sizeof detail, "%s", progress->terminal ? "; no count available" :
                                                                 "; no intermediate count available");
    if (!finished && progress->total > progress->completed && unchanged < 5 &&
        progress->advances >= 3 && source_seconds >= 3) {
        long double remaining = (long double)(progress->total - progress->completed) * source_seconds /
                                (progress->completed - progress->initial_completed);
        size_t used = strlen(detail);
        cfg_progress_remaining(detail + used, sizeof detail - used, remaining, progress->terminal);
    }
    flockfile(stderr);
    if (progress->terminal)
        fputs("\r\033[2K", stderr);
    struct winsize window = {0};
    int label_width = 200;
    size_t bar_columns = progress->terminal ? 25 : 23;
    if (progress->terminal && !ioctl(STDERR_FILENO, TIOCGWINSZ, &window) && window.ws_col) {
        /* The status's UTF-8 dash occupies one column; leave one spare column to avoid terminal wrapping. */
        size_t reserved = bar_columns + strlen(counter) + strlen(percentage) + strlen(status) + strlen(detail) - 2 + 1;
        if (!finished && reserved >= window.ws_col) {
            snprintf(status, sizeof status, " (%.0fs)", seconds);
            reserved = bar_columns + strlen(counter) + strlen(percentage) + strlen(status) + strlen(detail) + 1;
            if (reserved >= window.ws_col && progress->unit && progress->total) {
                size_t noun = strcspn(progress->unit, " ");
                snprintf(counter, sizeof counter, " %" PRIu64 "/%" PRIu64 " %.*s",
                         progress->completed, progress->total, (int)noun, progress->unit);
                reserved = bar_columns + strlen(counter) + strlen(percentage) + strlen(status) + strlen(detail) + 1;
            }
            if (reserved >= window.ws_col) {
                detail[0] = '\0';
                reserved = bar_columns + strlen(counter) + strlen(percentage) + strlen(status) + 1;
            }
        }
        label_width = window.ws_col > reserved ? (int)(window.ws_col - reserved) : 0;
    }
    if (progress->terminal) {
        static const char *const frames[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
        const char *symbol = finished ? (success ? "✓" : "✗") : frames[progress->tick % 10];
        fprintf(stderr, "\033[%sm%s [%s]\033[0m ", finished ? (success ? "32" : "31") : "36", symbol, bar);
    } else {
        fprintf(stderr, "[%s] ", bar);
    }
    fprintf(stderr, "%.*s%s%s%s%s%s", label_width, progress->label, counter, percentage, status, detail,
            !progress->terminal || finished ? "\n" : "");
    fflush(stderr);
    funlockfile(stderr);
}

static void *cfg_progress_run(void *argument)
{
    cfg_progress_t *progress = argument;
    while (!lock_wait_for(progress->wake, progress->terminal ? 250 : 1000)) {
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
                              const char *unit, uint64_t segment_size)
{
    if (!progress || !label)
        return;
    if (!lock_wait(progress->mutex))
        return;
    completed = completed > total ? total : completed;
    bool changed = progress->label != label || progress->unit != unit || progress->total != total ||
                   progress->segment_size != segment_size ||
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
    if (changed || completed > progress->completed) {
        string_free(progress->segment_name);
        progress->segment_name = NULL;
    }
    progress->label = label;
    progress->unit = unit;
    progress->completed = completed;
    progress->total = total;
    progress->segment_size = segment_size;
    if (changed)
        cfg_progress_render(progress, false, false);
    lock_unlock(progress->mutex);
}

/* All measured sources show their byte counts, replacing any previous item unit. */
void cfg_progress_update(cfg_progress_t *progress, const char *label, uint64_t completed, uint64_t total)
{
    cfg_progress_set(progress, label, completed, total, "bytes processed", 0);
}

/* Count updates are constant-time; the worker never waits for a timer tick. */
void cfg_progress_update_items(cfg_progress_t *progress, const char *label, uint64_t completed, uint64_t total,
                               const char *unit)
{
    if (unit) cfg_progress_set(progress, label, completed, total, unit, 1);
}

/* Keep batch counts explicit rather than inferring them from reporting frequency or elapsed time. */
void cfg_progress_update_segments(cfg_progress_t *progress, const char *label, uint64_t completed, uint64_t total,
                                  const char *unit, uint64_t segment_size)
{
    if (unit && segment_size) cfg_progress_set(progress, label, completed, total, unit, segment_size);
}

/* Copy optional display context so catalogue storage can be released independently of the reporting thread. */
void cfg_progress_segment_name(cfg_progress_t *progress, const string_t *name)
{
    if (!progress) return;
    string_t *copy = name && string_byte_length(name) && string_byte_length(name) <= 96 ? string_new() : NULL;
    string_cursor_t *cursor = copy ? string_cursor_new(name) : NULL;
    bool ok = !copy || cursor;
    while (ok && cursor && !string_cursor_done(cursor)) {
        rune_t rune = string_cursor_peek(cursor);
        string_pos_t start = string_cursor_position(cursor);
        string_cursor_next(cursor);
        ok = rune_is_control(rune) ? !string_append_char(copy, ' ') :
             !string_cursor_append_slice_between(copy, start, string_cursor_position(cursor), cursor);
    }
    string_cursor_free(cursor);
    if (!ok) { string_free(copy); copy = NULL; }
    if (!lock_wait(progress->mutex)) { string_free(copy); return; }
    string_free(progress->segment_name);
    progress->segment_name = copy;
    if (progress->completed == progress->initial_completed && progress->total > progress->completed)
        cfg_progress_render(progress, false, false);
    lock_unlock(progress->mutex);
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
    string_free(progress->segment_name);
    free(progress);
}
