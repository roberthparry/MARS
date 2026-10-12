/**
 * @file executor.c
 * @brief Bounded FIFO worker scheduling using thread_t and lock_t exclusively.
 *
 * Queue operations are constant-time under an owned mutex clone. A manual-reset
 * event represents available work or shutdown; predicate changes and resets
 * share the queue mutex to prevent lost notifications. Shutdown cancellation
 * visits the bounded outstanding set once; no callback runs under that mutex.
 */
#include <errno.h>
#include <stdlib.h>

#define MARS_EXECUTOR_INTERNAL_ACCESS
#include "executor_internal.h"

typedef struct {
    executor_t *executor;
    thread_t *thread;
    lock_t *gate;
    lock_t *wake;
    executor_task_t *active;
} executor_worker_t;

struct _executor_t {
    lock_t *gate;
    lock_t *wake;
    lock_t *idle;
    executor_worker_t *workers;
    size_t worker_count;
    executor_task_t **queue;
    size_t capacity;
    size_t head;
    size_t tail;
    executor_stats_t stats;
};

static void *executor_worker(void *argument)
{
    executor_worker_t *worker = argument;
    executor_t *executor = worker->executor;
    executor_current = executor;
    for (;;) {
        lock_wait(worker->wake);
        lock_wait(worker->gate);
        if (!executor->stats.queued) {
            bool finished = !executor->stats.accepting;
            lock_unlock(worker->gate);
            if (finished)
                break;
            continue;
        }
        executor_task_t *task = executor->queue[executor->head];
        executor->queue[executor->head] = NULL;
        executor->head = (executor->head + 1) % executor->capacity;
        --executor->stats.queued;
        ++executor->stats.running;
        worker->active = task;
        if (!executor->stats.queued && executor->stats.accepting)
            lock_reset(worker->wake);
        lock_unlock(worker->gate);
        executor_task_execute(task);
        lock_wait(worker->gate);
        worker->active = NULL;
        lock_unlock(worker->gate);
        executor_task_free(task);
        lock_wait(worker->gate);
        --executor->stats.running;
        if (executor->stats.completed != UINT64_MAX)
            ++executor->stats.completed;
        if (!executor->stats.running && !executor->stats.queued)
            lock_signal(executor->idle);
        lock_unlock(worker->gate);
    }
    executor_current = NULL;
    return NULL;
}

static void executor_dispose(executor_t *executor)
{
    for (size_t i = 0; i < executor->worker_count; ++i) {
        lock_free(executor->workers[i].gate);
        lock_free(executor->workers[i].wake);
    }
    lock_free(executor->gate);
    lock_free(executor->wake);
    lock_free(executor->idle);
    free(executor->workers);
    free(executor->queue);
    free(executor);
}

/* Either publish a full pool or close and join every partially started worker. */
executor_t *executor_new(size_t workers, size_t capacity)
{
    if (!workers || !capacity) {
        errno = EINVAL;
        return NULL;
    }
    if (workers > SIZE_MAX / sizeof(executor_worker_t) || capacity > SIZE_MAX / sizeof(executor_task_t *)) {
        errno = EOVERFLOW;
        return NULL;
    }
    executor_t *executor = calloc(1, sizeof(*executor));
    if (!executor)
        return NULL;
    executor->workers = calloc(workers, sizeof(*executor->workers));
    executor->queue = calloc(capacity, sizeof(*executor->queue));
    executor->gate = lock_new(LOCK_MUTEX, 0);
    executor->wake = lock_new(LOCK_EVENT, 0);
    executor->idle = lock_new(LOCK_EVENT, 1);
    if (!executor->workers || !executor->queue || !executor->gate || !executor->wake || !executor->idle) {
        int saved = errno;
        executor_dispose(executor);
        errno = saved;
        return NULL;
    }
    executor->capacity = capacity;
    executor->stats.accepting = true;
    for (size_t i = 0; i < workers; ++i) {
        executor_worker_t *worker = &executor->workers[i];
        worker->executor = executor;
        worker->gate = lock_clone(executor->gate);
        worker->wake = lock_clone(executor->wake);
        executor->worker_count = i + 1;
        if (worker->gate && worker->wake)
            worker->thread = thread_new(executor_worker, worker);
        if (!worker->thread) {
            int saved = errno;
            lock_wait(executor->gate);
            executor->stats.accepting = false;
            lock_signal(executor->wake);
            lock_unlock(executor->gate);
            for (size_t j = 0; j < i; ++j) {
                thread_join(executor->workers[j].thread, NULL);
                thread_free(executor->workers[j].thread);
            }
            executor_dispose(executor);
            errno = saved;
            return NULL;
        }
    }
    return executor;
}

/* Capacity bounds waiting callbacks, separately from the fixed running-worker bound. */
executor_task_t *executor_submit(executor_t *executor, executor_callback_t callback, void *argument,
                                 const executor_task_options_t *options)
{
    if (!executor || !callback) {
        errno = EINVAL;
        return NULL;
    }
    LOCK_AUTO(gate) = lock_clone(executor->gate);
    if (!gate || !lock_wait(gate))
        return NULL;
    if (!executor->stats.accepting || executor->stats.queued == executor->capacity) {
        errno = executor->stats.accepting ? EAGAIN : ESHUTDOWN;
        return NULL;
    }
    executor_task_t *task = executor_task_create(callback, argument, options);
    if (!task)
        return NULL;
    executor_task_retain(task);
    executor->queue[executor->tail] = task;
    executor->tail = (executor->tail + 1) % executor->capacity;
    ++executor->stats.queued;
    lock_reset(executor->idle);
    lock_signal(executor->wake);
    return task;
}

/* The only task traversal is the explicitly requested cancellation sweep. */
bool executor_shutdown(executor_t *executor, bool cancel)
{
    if (!executor) {
        errno = EINVAL;
        return false;
    }
    LOCK_AUTO(gate) = lock_clone(executor->gate);
    if (!gate || !lock_wait(gate))
        return false;
    executor->stats.accepting = false;
    if (cancel) {
        size_t slot = executor->head;
        for (size_t i = 0; i < executor->stats.queued; ++i) {
            executor_task_cancel(executor->queue[slot]);
            slot = (slot + 1) % executor->capacity;
        }
        for (size_t i = 0; i < executor->worker_count; ++i)
            executor_task_cancel(executor->workers[i].active);
    }
    return lock_signal(executor->wake);
}

/* Idle is a snapshot unless submissions have already been closed. */
bool executor_wait(executor_t *executor, uint64_t milliseconds)
{
    if (!executor || (executor_current == executor && milliseconds)) {
        errno = executor ? EDEADLK : EINVAL;
        return false;
    }
    LOCK_AUTO(idle) = lock_clone(executor->idle);
    return idle && lock_wait_for(idle, milliseconds);
}

/* Copy all counters while holding a single queue acquisition. */
bool executor_stats(executor_t *executor, executor_stats_t *stats)
{
    if (!executor || !stats) {
        errno = EINVAL;
        return false;
    }
    LOCK_AUTO(gate) = lock_clone(executor->gate);
    if (!gate || !lock_wait(gate))
        return false;
    *stats = executor->stats;
    return true;
}

/* Destruction drains by default and never invalidates independently retained tasks. */
bool executor_free(executor_t *executor)
{
    if (!executor)
        return true;
    if (executor_current == executor) {
        errno = EDEADLK;
        return false;
    }
    /* External lifecycle exclusion permits the owner's unacquired handle here;
     * destruction must not allocate a fresh mutex clone just to close the queue. */
    if (!lock_wait(executor->gate))
        return false;
    executor->stats.accepting = false;
    lock_signal(executor->wake);
    lock_unlock(executor->gate);
    for (size_t i = 0; i < executor->worker_count; ++i) {
        thread_t *thread = executor->workers[i].thread;
        if (thread) {
            if (!thread_join(thread, NULL))
                return false;
            thread_free(thread);
            executor->workers[i].thread = NULL;
        }
    }
    executor_dispose(executor);
    return true;
}
