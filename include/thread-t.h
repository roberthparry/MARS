/**
 * @file thread-t.h
 * @brief Opaque Linux callback threads with lock-backed completion waits.
 *
 * Start a callback, wait/check for completion, then join to collect its result or
 * explicitly detach. thread_t owns a lock_t completion event; all synchronisation
 * primitives belong to lock-t.h, not to a duplicate thread mutex API.
 *
 * Serialise lifecycle calls on each handle. Join or detach before freeing it;
 * freeing never silently waits, cancels or detaches. Callback arguments/results
 * remain application-owned. A completion wait means the callback has returned,
 * not that native thread resources have been joined. Callbacks must return normally.
 *
 * This native API does not make other MARS modules thread-safe and does not
 * enable browser WebAssembly threads. Link with -pthread. Boolean operations
 * return false with errno set; success does not clear errno.
 */
#ifndef MARS_THREAD_H
#define MARS_THREAD_H

#include <stdbool.h>
#include <stdint.h>

#include "lock-t.h"

/** @brief Owned native thread handle; join or detach, then release with thread_free(). */
typedef struct _thread_t thread_t;

/**
 * @brief Callback executed once on a new native thread.
 * @param argument Borrowed context supplied to thread_new(); may be NULL.
 * @return Caller-managed result collected by thread_join(); never return a pointer to a local variable.
 */
typedef void *(*thread_callback_t)(void *argument);

/**
 * @brief Start a joinable thread; the callback may run before this function returns.
 * @param callback Required entry function; must return normally, not call pthread_exit or enable external cancellation.
 * @param argument Borrowed context, possibly NULL; keep alive until the callback finishes using it.
 * @return Owned handle, or NULL with errno set. No callback starts on failure.
 */
thread_t *thread_new(thread_callback_t callback, void *argument);

/**
 * @brief Wait for callback completion through the thread's lock_t event, without joining.
 * @param thread Required borrowed handle; do not overlap this call with free or other lifecycle operations.
 * @param milliseconds Zero for a non-blocking check, LOCK_WAIT_FOREVER for no deadline, otherwise a monotonic budget.
 * @return True after callback return; false with EBUSY/ETIMEDOUT for pending/expired waits, EDEADLK for blocking
 * self-wait while joinable, or another errno on failure. Detached callbacks must never wait on themselves.
 */
bool thread_wait(thread_t *thread, uint64_t milliseconds);

/**
 * @brief Check the persistent callback-completion event without joining.
 * @param thread Required borrowed live handle.
 * @return True after callback return, or false with EBUSY if pending or EINVAL for NULL.
 */
bool thread_check(const thread_t *thread);

/**
 * @brief Wait for native termination and collect the callback result exactly once.
 * @param thread Required caller-owned joinable handle; serialise all lifecycle operations.
 * @param result Optional output slot; unchanged on failure. The application owns the returned pointer.
 * @return True on success; false with errno set (EINVAL for consumed handles, EDEADLK for self-join).
 */
bool thread_join(thread_t *thread, void **result);

/**
 * @brief Relinquish joining without stopping or waiting for the callback.
 * @param thread Required joinable handle; no concurrent lifecycle operations permitted.
 * @return True on success, or false with errno set. Keep context alive using separate completion synchronisation.
 */
bool thread_detach(thread_t *thread);

/**
 * @brief Release a joined or detached handle and its reference to the completion event.
 * @param thread Owned handle, or NULL; retained on failure. Callback context and results are never freed.
 * @return True on release/NULL; false with EBUSY while still joinable, even if the callback has returned.
 */
bool thread_free(thread_t *thread);

#endif
