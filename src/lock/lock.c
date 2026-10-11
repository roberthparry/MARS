/**
 * @file lock.c
 * @brief Synchronisation class cluster and reference-counted acquisition ownership.
 *
 * Specialised backends implement predicates and resource accounting; a common
 * pthread monitor supplies sleeping waits, monotonic deadlines and atomic state
 * transitions. Acquisition ownership belongs to each opaque handle, allowing a
 * scoped clone to unlock on destruction without destroying another owner's state.
 * This is process-local, non-robust synchronisation, not shared-memory IPC.
 */
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>
#include <time.h>

#define MARS_LOCK_INTERNAL_ACCESS
#include "lock_internal.h"

static bool lock_status(int error)
{
    if (error)
        errno = error;
    return error == 0;
}

/* Construct one selected backend; shared state is never exposed to callers. */
lock_t *lock_new(lock_kind_t kind, unsigned initial)
{
    if ((unsigned)kind >= sizeof(lock_backends) / sizeof(*lock_backends) ||
        (kind == LOCK_SEMAPHORE ? !initial : kind == LOCK_EVENT ? initial > 1 : initial != 0)) {
        errno = EINVAL;
        return NULL;
    }
    lock_t *lock = calloc(1, sizeof(*lock));
    lock_state_t *state = calloc(1, sizeof(*state));
    if (!lock || !state) {
        free(lock);
        free(state);
        errno = ENOMEM;
        return NULL;
    }
    int error = pthread_mutex_init(&state->gate, NULL);
    if (!error) {
        pthread_condattr_t attributes;
        error = pthread_condattr_init(&attributes);
        if (!error) {
            error = pthread_condattr_setclock(&attributes, CLOCK_MONOTONIC);
            if (!error)
                error = pthread_cond_init(&state->changed, &attributes);
            pthread_condattr_destroy(&attributes);
        }
        if (error)
            pthread_mutex_destroy(&state->gate);
    }
    if (error) {
        free(state);
        free(lock);
        lock_status(error);
        return NULL;
    }
    state->backend = &lock_backends[kind];
    state->kind = kind;
    state->references = 1;
    if (kind == LOCK_SEMAPHORE)
        state->value.permits = initial;
    if (kind == LOCK_EVENT)
        state->value.signalled = initial != 0;
    lock->state = state;
    return lock;
}

/* A new handle shares resource state but starts with no acquisitions. */
lock_t *lock_clone(const lock_t *lock)
{
    if (!lock) {
        errno = EINVAL;
        return NULL;
    }
    lock_t *copy = calloc(1, sizeof(*copy));
    if (!copy)
        return NULL;
    lock_state_t *state = lock->state;
    int error = pthread_mutex_lock(&state->gate);
    if (!error) {
        if (state->references == SIZE_MAX)
            error = EOVERFLOW;
        else {
            ++state->references;
            copy->state = state;
        }
        pthread_mutex_unlock(&state->gate);
    }
    if (error) {
        free(copy);
        lock_status(error);
        return NULL;
    }
    return copy;
}

/* Backend identity never changes during the shared state's lifetime. */
lock_kind_t lock_kind(const lock_t *lock)
{
    if (!lock) {
        errno = EINVAL;
        return LOCK_INVALID;
    }
    return lock->state->kind;
}

static int lock_deadline(uint64_t milliseconds, struct timespec *deadline)
{
    if (clock_gettime(CLOCK_MONOTONIC, deadline))
        return errno;
    uint64_t seconds = milliseconds / 1000;
    long nanos = deadline->tv_nsec + (long)(milliseconds % 1000) * 1000000L;
    seconds += nanos / 1000000000L;
    uint64_t maximum = sizeof(time_t) >= sizeof(int64_t) ? INT64_MAX : INT32_MAX;
    if (seconds > maximum - (uint64_t)deadline->tv_sec)
        return EOVERFLOW;
    deadline->tv_sec += (time_t)seconds;
    deadline->tv_nsec = nanos % 1000000000L;
    return 0;
}

static int lock_acquisition_error(const lock_t *lock, bool read, bool blocking)
{
    const lock_state_t *state = lock->state;
    if (read && state->kind != LOCK_READ_WRITE)
        return ENOTSUP;
    if (lock->depth && !pthread_equal(lock->owner, pthread_self()))
        return EPERM;
    if (lock->depth == SIZE_MAX)
        return EOVERFLOW;
    if (state->kind == LOCK_RECURSIVE && state->value.mutex.depth == SIZE_MAX)
        return EOVERFLOW;
    if (state->kind == LOCK_READ_WRITE) {
        if (read && state->value.rw.readers == SIZE_MAX)
            return EOVERFLOW;
        if (lock->depth && (!lock->read || !read))
            return blocking ? EDEADLK : EBUSY;
        if (state->value.rw.writing && pthread_equal(state->value.rw.owner, pthread_self()))
            return blocking ? EDEADLK : EBUSY;
    }
    if (state->kind == LOCK_MUTEX && state->value.mutex.depth &&
        pthread_equal(state->value.mutex.owner, pthread_self()))
        return blocking ? EDEADLK : EBUSY;
    return 0;
}

static bool lock_acquire(lock_t *lock, bool read, uint64_t milliseconds)
{
    if (!lock)
        return lock_status(EINVAL);
    lock_state_t *state = lock->state;
    int error = pthread_mutex_lock(&state->gate);
    if (error)
        return lock_status(error);
    error = lock_acquisition_error(lock, read, milliseconds != 0);
    struct timespec deadline = {0};
    if (!error && milliseconds && milliseconds != LOCK_WAIT_FOREVER)
        error = lock_deadline(milliseconds, &deadline);
    while (!error && !state->backend->ready(state, read)) {
        if (!milliseconds) {
            error = EBUSY;
            break;
        }
        error = milliseconds == LOCK_WAIT_FOREVER ? pthread_cond_wait(&state->changed, &state->gate)
                                                 : pthread_cond_timedwait(&state->changed, &state->gate, &deadline);
        if (error == ETIMEDOUT && state->backend->ready(state, read))
            error = 0;
    }
    if (!error && state->backend->take) {
        state->backend->take(state, read);
        lock->owner = pthread_self();
        lock->read = read;
        ++lock->depth;
    }
    pthread_mutex_unlock(&state->gate);
    return lock_status(error);
}

/* Wait indefinitely for an exclusive acquisition, permit or completion event. */
bool lock_wait(lock_t *lock)
{
    return lock_acquire(lock, false, LOCK_WAIT_FOREVER);
}

/* Reuse one deadline throughout the native condition-wait loop. */
bool lock_wait_for(lock_t *lock, uint64_t milliseconds)
{
    return lock_acquire(lock, false, milliseconds);
}

/* Shared readers use the same owned-handle destruction rules as writers. */
bool lock_read(lock_t *lock, uint64_t milliseconds)
{
    return lock_acquire(lock, true, milliseconds);
}

/* Non-blocking check-and-acquire, unlike the observational lock_check. */
bool lock_try(lock_t *lock)
{
    return lock_acquire(lock, false, 0);
}

/* Observe availability without changing any acquisition or event state. */
bool lock_check(const lock_t *lock)
{
    if (!lock)
        return lock_status(EINVAL);
    lock_state_t *state = lock->state;
    int error = pthread_mutex_lock(&state->gate);
    if (error)
        return lock_status(error);
    bool ready = state->backend->ready(state, false);
    pthread_mutex_unlock(&state->gate);
    return lock_status(ready ? 0 : EBUSY);
}

/* Release only acquisitions that belong to this handle and calling thread. */
bool lock_unlock(lock_t *lock)
{
    if (!lock)
        return lock_status(EINVAL);
    lock_state_t *state = lock->state;
    int error = pthread_mutex_lock(&state->gate);
    if (error)
        return lock_status(error);
    if (!state->backend->drop)
        error = ENOTSUP;
    else if (!lock->depth || !pthread_equal(lock->owner, pthread_self()))
        error = EPERM;
    else {
        state->backend->drop(state, lock->read, 1);
        --lock->depth;
        pthread_cond_broadcast(&state->changed);
    }
    pthread_mutex_unlock(&state->gate);
    return lock_status(error);
}

static bool lock_event_set(lock_t *lock, bool signalled)
{
    if (!lock)
        return lock_status(EINVAL);
    lock_state_t *state = lock->state;
    if (state->kind != LOCK_EVENT)
        return lock_status(ENOTSUP);
    int error = pthread_mutex_lock(&state->gate);
    if (error)
        return lock_status(error);
    state->value.signalled = signalled;
    if (signalled)
        pthread_cond_broadcast(&state->changed);
    pthread_mutex_unlock(&state->gate);
    return true;
}

/* A completion remains visible to late waiters until explicitly reset. */
bool lock_signal(lock_t *lock)
{
    return lock_event_set(lock, true);
}

/* Resetting is an application-coordinated generation boundary. */
bool lock_reset(lock_t *lock)
{
    return lock_event_set(lock, false);
}

/* Release every owned acquisition, then reclaim state only after the last reference. */
bool lock_free(lock_t *lock)
{
    if (!lock)
        return true;
    lock_state_t *state = lock->state;
    int error = pthread_mutex_lock(&state->gate);
    if (error)
        return lock_status(error);
    if (lock->depth && !pthread_equal(lock->owner, pthread_self())) {
        pthread_mutex_unlock(&state->gate);
        return lock_status(EPERM);
    }
    if (lock->depth) {
        state->backend->drop(state, lock->read, lock->depth);
        pthread_cond_broadcast(&state->changed);
    }
    bool last = --state->references == 0;
    pthread_mutex_unlock(&state->gate);
    if (last) {
        pthread_cond_destroy(&state->changed);
        pthread_mutex_destroy(&state->gate);
        free(state);
    }
    free(lock);
    return true;
}

/* Normal lexical scope exits can release an owning variable and clear it. */
void lock_cleanup(lock_t **slot)
{
    int saved = errno;
    if (!slot || lock_free(*slot)) {
        if (slot)
            *slot = NULL;
        errno = saved;
    }
}
