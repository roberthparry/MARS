/**
 * @file lock_backends.c
 * @brief Specialised predicates and resource accounting for lock_t backends.
 *
 * Implements mutex, recursive, reader/writer, bounded-permit and event policies.
 * The owning monitor serialises every callback; blocking, reference lifetimes and
 * error reporting stay in lock.c. No backend invokes application callbacks.
 */
#define MARS_LOCK_INTERNAL_ACCESS
#include "lock_internal.h"

static bool lock_mutex_ready(const lock_state_t *state, bool read)
{
    (void)read;
    return !state->value.mutex.depth;
}

static bool lock_recursive_ready(const lock_state_t *state, bool read)
{
    return lock_mutex_ready(state, read) || pthread_equal(state->value.mutex.owner, pthread_self());
}

static void lock_mutex_take(lock_state_t *state, bool read)
{
    (void)read;
    state->value.mutex.owner = pthread_self();
    ++state->value.mutex.depth;
}

static void lock_mutex_drop(lock_state_t *state, bool read, size_t count)
{
    (void)read;
    state->value.mutex.depth -= count;
}

static bool lock_rw_ready(const lock_state_t *state, bool read)
{
    return !state->value.rw.writing && (read || !state->value.rw.readers);
}

static void lock_rw_take(lock_state_t *state, bool read)
{
    if (read)
        ++state->value.rw.readers;
    else {
        state->value.rw.writing = true;
        state->value.rw.owner = pthread_self();
    }
}

static void lock_rw_drop(lock_state_t *state, bool read, size_t count)
{
    if (read)
        state->value.rw.readers -= count;
    else
        state->value.rw.writing = false;
}

static bool lock_semaphore_ready(const lock_state_t *state, bool read)
{
    (void)read;
    return state->value.permits != 0;
}

static void lock_semaphore_take(lock_state_t *state, bool read)
{
    (void)read;
    --state->value.permits;
}

static void lock_semaphore_drop(lock_state_t *state, bool read, size_t count)
{
    (void)read;
    state->value.permits += (unsigned)count;
}

static bool lock_event_ready(const lock_state_t *state, bool read)
{
    (void)read;
    return state->value.signalled;
}

const lock_backend_t lock_backends[LOCK_EVENT + 1] = {
    [LOCK_MUTEX]      = {lock_mutex_ready,     lock_mutex_take,     lock_mutex_drop},
    [LOCK_RECURSIVE]  = {lock_recursive_ready, lock_mutex_take,     lock_mutex_drop},
    [LOCK_READ_WRITE] = {lock_rw_ready,        lock_rw_take,        lock_rw_drop},
    [LOCK_SEMAPHORE]  = {lock_semaphore_ready, lock_semaphore_take, lock_semaphore_drop},
    [LOCK_EVENT]      = {lock_event_ready,     NULL,                NULL},
};
