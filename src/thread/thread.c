/**
 * @file thread.c
 * @brief Native callback lifetimes with completion synchronisation delegated to lock_t.
 *
 * The worker owns an independent completion-event reference, so detaching and
 * freeing the public handle cannot invalidate its callback. Native pthread use
 * is limited to starting, joining, identifying and detaching threads; no mutex
 * or condition implementation is embedded here.
 */
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>

#include "thread-t.h"

typedef struct _thread_t {
    pthread_t native;
    lock_t *completion;
    bool joinable;
} thread_t;

typedef struct {
    thread_callback_t callback;
    void *argument;
    lock_t *completion;
} thread_start_t;

static bool thread_status(int error)
{
    if (error)
        errno = error;
    return error == 0;
}

static void *thread_run(void *argument)
{
    thread_start_t *start = argument;
    void *result = start->callback(start->argument);
    lock_signal(start->completion);
    lock_free(start->completion);
    free(start);
    return result;
}

/* Publish an independently owned completion event before starting the callback. */
thread_t *thread_new(thread_callback_t callback, void *argument)
{
    if (!callback) {
        errno = EINVAL;
        return NULL;
    }
    thread_t *thread = calloc(1, sizeof(*thread));
    thread_start_t *start = calloc(1, sizeof(*start));
    if (!thread || !start) {
        free(thread);
        free(start);
        errno = ENOMEM;
        return NULL;
    }
    thread->completion = lock_new(LOCK_EVENT, 0);
    start->completion = thread->completion ? lock_clone(thread->completion) : NULL;
    int error = start->completion ? 0 : errno;
    if (!error) {
        start->callback = callback;
        start->argument = argument;
        error = pthread_create(&thread->native, NULL, thread_run, start);
    }
    if (error) {
        lock_free(start->completion);
        lock_free(thread->completion);
        free(start);
        free(thread);
        thread_status(error);
        return NULL;
    }
    thread->joinable = true;
    return thread;
}

/* A persistent lock event permits bounded completion waits without consuming join ownership. */
bool thread_wait(thread_t *thread, uint64_t milliseconds)
{
    if (thread && thread->joinable && milliseconds && pthread_equal(thread->native, pthread_self()))
        return thread_status(EDEADLK);
    return thread ? lock_wait_for(thread->completion, milliseconds) : thread_status(EINVAL);
}

/* Completion checks observe the event without joining or changing its state. */
bool thread_check(const thread_t *thread)
{
    return thread ? lock_check(thread->completion) : thread_status(EINVAL);
}

/* Native joining publishes the result and consumes join ownership exactly once. */
bool thread_join(thread_t *thread, void **result)
{
    if (!thread || !thread->joinable)
        return thread_status(EINVAL);
    if (pthread_equal(thread->native, pthread_self()))
        return thread_status(EDEADLK);
    void *value = NULL;
    if (!thread_status(pthread_join(thread->native, &value)))
        return false;
    thread->joinable = false;
    if (result)
        *result = value;
    return true;
}

/* Detachment never changes callback context ownership. */
bool thread_detach(thread_t *thread)
{
    if (!thread || !thread->joinable)
        return thread_status(EINVAL);
    if (!thread_status(pthread_detach(thread->native)))
        return false;
    thread->joinable = false;
    return true;
}

/* The worker's event reference survives releasing a detached public handle. */
bool thread_free(thread_t *thread)
{
    if (thread && thread->joinable)
        return thread_status(EBUSY);
    if (thread && !lock_free(thread->completion))
        return false;
    free(thread);
    return true;
}
