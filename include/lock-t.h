/**
 * @file lock-t.h
 * @brief Opaque process-local synchronisation class cluster with owned acquisitions.
 *
 * One lock_t interface selects an error-checking mutex, recursive mutex,
 * reader/writer lock, bounded semaphore or manual-reset completion event.
 * Clone a handle for each concurrent owner: clones share the underlying state,
 * not ownership of acquisitions. Freeing a handle releases all of its acquired
 * levels or permits on the owning thread. Shared state survives until the last
 * handle is freed, including handles belonging to blocked waiters.
 *
 * Waits sleep rather than spin and timed waits use CLOCK_MONOTONIC. A check is
 * only a snapshot; use lock_try() to reserve availability atomically. Events do
 * not grant ownership: waiting leaves an event signalled until explicit reset.
 * Destroying an event does not falsely signal task completion.
 *
 * These locks do not synchronise separate processes or browser WebAssembly.
 * Do not race freeing a handle with any access through that same pointer, abandon
 * owned acquisitions, externally cancel a waiter, or use these APIs from a signal
 * handler. LOCK_AUTO(name) declares a GCC/Clang scope-cleaned owning handle.
 */
#ifndef MARS_LOCK_H
#define MARS_LOCK_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Opaque owning handle; clone for another concurrent owner and free on the acquiring thread. */
typedef struct _lock_t lock_t;

/** @brief Immutable backend selected at construction. */
typedef enum {
    LOCK_INVALID = -1,     /**< Invalid or absent handle. */
    LOCK_MUTEX,            /**< Non-recursive exclusion with self-deadlock detection. */
    LOCK_RECURSIVE,        /**< Repeated acquisitions by the same thread. */
    LOCK_READ_WRITE,       /**< Concurrent readers or one exclusive writer; no upgrade or downgrade. */
    LOCK_SEMAPHORE,        /**< Bounded permits, returned by unlock or destruction. */
    LOCK_EVENT             /**< Persistent manual-reset completion notification, without ownership. */
} lock_kind_t;

/** @brief Infinite timeout for wait/read operations; zero requests a non-blocking attempt. */
#define LOCK_WAIT_FOREVER UINT64_MAX

/**
 * @brief Construct an unacquired handle with specialised shared state.
 * @param kind Required backend kind; LOCK_INVALID and unknown values are rejected.
 * @param initial Zero for mutex/recursive/read-write; positive permit count for semaphore; zero/one for event state.
 * @return Owned handle, or NULL with errno set. Release with lock_free().
 */
lock_t *lock_new(lock_kind_t kind, unsigned initial);

/**
 * @brief Create an unacquired handle sharing the same synchronisation state.
 * @param lock Required borrowed live handle; its acquisitions are not copied and its lifetime must be protected.
 * @return Independently owned handle, or NULL with errno set; suitable for another thread or scoped guard.
 */
lock_t *lock_clone(const lock_t *lock);

/**
 * @brief Query the immutable backend kind.
 * @param lock Borrowed handle, or NULL.
 * @return Backend kind, or LOCK_INVALID with EINVAL for NULL.
 */
lock_kind_t lock_kind(const lock_t *lock);

/**
 * @brief Acquire exclusively, take a permit, or wait for an event indefinitely.
 * @param lock Required live handle; use a separate clone for each concurrent owner.
 * @return True on acquisition/event completion, or false with errno set; detected self-deadlock uses EDEADLK.
 */
bool lock_wait(lock_t *lock);

/**
 * @brief Wait with one monotonic time budget, including any spurious wakeups.
 * @param lock Required live handle; read-write locks acquire exclusive ownership here.
 * @param milliseconds Zero for a non-blocking attempt, LOCK_WAIT_FOREVER for no deadline, otherwise a finite budget.
 * @return True on acquisition/event completion; false with EBUSY for an unavailable zero-time attempt,
 * ETIMEDOUT for an expired finite wait, EOVERFLOW for an unrepresentable deadline, or another errno on failure.
 */
bool lock_wait_for(lock_t *lock, uint64_t milliseconds);

/**
 * @brief Acquire shared reader ownership on a reader/writer backend.
 * @param lock Required LOCK_READ_WRITE handle; never upgrade a held read lock or downgrade a write lock.
 * @param milliseconds Timeout policy as for lock_wait_for(); repeated reader acquisitions need matching releases.
 * @return True when acquired, or false with errno set; other backend kinds use ENOTSUP.
 */
bool lock_read(lock_t *lock, uint64_t milliseconds);

/**
 * @brief Attempt exclusive/permit acquisition or event observation without blocking.
 * @param lock Required live handle.
 * @return True on success, or false with EBUSY when unavailable or another errno on failure.
 */
bool lock_try(lock_t *lock);

/**
 * @brief Check exclusive/permit availability or event completion without acquiring anything.
 * @param lock Required borrowed live handle.
 * @return True if available now, false with EBUSY if not, or another errno on failure. This reserves nothing.
 */
bool lock_check(const lock_t *lock);

/**
 * @brief Release one acquisition owned by this handle on the calling thread.
 * @param lock Required acquired mutex/read-write/semaphore handle; events have no acquisitions.
 * @return True on success, or false with errno set; EPERM for absent/wrong-thread ownership, ENOTSUP for events.
 */
bool lock_unlock(lock_t *lock);

/**
 * @brief Set a completion event and wake all waiters; the signal persists for later waiters.
 * @param lock Required LOCK_EVENT handle; signalling does not require acquisition.
 * @return True on success, or false with errno set; other kinds use ENOTSUP.
 */
bool lock_signal(lock_t *lock);

/**
 * @brief Reset a completion event for a caller-coordinated subsequent operation.
 * @param lock Required LOCK_EVENT handle; coordinate resets so interested waiters cannot miss completion.
 * @return True on success, or false with errno set; other kinds use ENOTSUP.
 */
bool lock_reset(lock_t *lock);

/**
 * @brief Release this handle's acquisitions and its reference to shared state.
 * @param lock Owned handle, or NULL; no concurrent use of this exact handle is permitted during destruction.
 * @return True on release/NULL; false with errno set and ownership retained, including EPERM for wrong-thread release.
 */
bool lock_free(lock_t *lock);

/**
 * @brief GCC/Clang cleanup callback, also usable for explicit release-and-clear.
 * @param slot Pointer to an owned handle variable, or NULL; cleared after successful lock_free().
 * @return No value. Success preserves errno; misuse leaves the slot unchanged and sets errno. Scope cleanup cannot
 * report failure, so use only on handles whose acquisitions belong to the exiting thread. Not a longjmp/exit hook.
 */
void lock_cleanup(lock_t **slot);

#if defined(__GNUC__) || defined(__clang__)
/**
 * @brief Declare an automatic lock_t pointer cleaned up on normal scope exit, including early return.
 * @param name Local variable identifier; follow the macro with an explicit owning initialiser or NULL.
 * This declares ownership only: it neither clones nor acquires a lock. Available on GCC/Clang; there is no no-op
 * fallback. Acquisitions must belong to the exiting thread. Manual release must also clear the variable.
 */
#define LOCK_AUTO(name) lock_t *name __attribute__((cleanup(lock_cleanup)))
#endif

#endif
