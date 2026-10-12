/**
 * @file executor-t.h
 * @brief Bounded native callback execution with owned task results and diagnostics.
 *
 * executor_t runs a fixed number of thread_t workers over a bounded FIFO queue.
 * Every accepted task has an independently reference-counted executor_task_t,
 * a computation_t control and lock_t completion event. Submission never waits
 * for space. Tasks may outlive the executor; dropping a handle never cancels it.
 * Cancellation is cooperative, not forced termination. Shutdown rejects new
 * work; destruction joins all workers and can block until callbacks return.
 *
 * This native facility does not make numerical graphs, global precision settings
 * or other modules thread-safe, and is not a browser or process executor. Callers
 * must arrange independent thread-safe work and fixed global configuration.
 * All callbacks, observers and destructors must return normally. Concurrent
 * submission, inspection, waits and shutdown are supported. Serialise executor
 * destruction against all external accesses. Protect task-reference lifetimes.
 */
#ifndef MARS_EXECUTOR_H
#define MARS_EXECUTOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "computation-t.h"
#include "diagnostic-t.h"
#include "lock-t.h"

/** @brief Owned fixed-size native worker pool. */
typedef struct _executor_t executor_t;

/** @brief Owned reference to an accepted task, independent of the executor's lifetime. */
typedef struct _executor_task_t executor_task_t;

/** @brief Lifecycle snapshots; cancellation becomes terminal only after work and argument cleanup stop. */
typedef enum {
    EXECUTOR_TASK_INVALID = -1,
    EXECUTOR_TASK_QUEUED,
    EXECUTOR_TASK_RUNNING,
    EXECUTOR_TASK_SUCCEEDED,
    EXECUTOR_TASK_FAILED,
    EXECUTOR_TASK_CANCELLED,
    EXECUTOR_TASK_TIMED_OUT,
    EXECUTOR_TASK_BUDGET_EXHAUSTED
} executor_task_status_t;

/**
 * @brief Run under computation_current(); return true for success and false for failure.
 * Output slots initially contain NULL. Transfer an optional result and owned diagnostic reference
 * to the task. On failure, set errno (zero becomes EIO); an absent diagnostic is supplied automatically.
 * A diagnostic may accompany success, for example a warning. Do not complete the scoped control.
 */
typedef bool (*executor_callback_t)(void *argument, void **result, diagnostic_t **diagnostic);

/** @brief Dispose of task-owned data; runs without pool locks and must not throw, exit or block on this pool. */
typedef void (*executor_cleanup_t)(void *value);

/** @brief Copied task options; zero initialisation means borrowed argument/result and a fresh unlimited control. */
typedef struct {
    computation_t *computation; /**< Optional retained shared control; the task never completes/fails it automatically. */
    executor_cleanup_t argument_free; /**< Optional ownership transfer on successful submission only. */
    executor_cleanup_t result_free; /**< Optional result destructor, including discarded partial/stopped results. */
} executor_task_options_t;

/** @brief Consistent instantaneous queue statistics; completed saturates at UINT64_MAX. */
typedef struct {
    size_t queued; /**< Accepted callbacks waiting for a worker, excluding running work. */
    size_t running; /**< Tasks dispatched to workers, including cleanup/publication. */
    uint64_t completed; /**< Finished tasks, including skipped or failed callbacks. */
    bool accepting; /**< Whether new submissions are allowed. */
} executor_stats_t;

/** @brief Start workers with a bounded queue; both sizes must be positive, failures set errno and join partial starts. */
executor_t *executor_new(size_t workers, size_t capacity);

/**
 * @brief Enqueue without waiting for capacity; return an owned handle or NULL with errno.
 * EAGAIN means full, ESHUTDOWN means closed, EINVAL means missing pool/callback.
 * All argument/result ownership remains with the caller on submission failure.
 * Accepted argument cleanup runs once before completion, even if a queued callback is skipped.
 * Deadlines of supplied controls include queue time. Options are copied, not retained by address.
 */
executor_task_t *executor_submit(executor_t *executor, executor_callback_t callback, void *argument,
                                 const executor_task_options_t *options);

/**
 * @brief Close submissions, optionally requesting cancellation of all queued/running work; does not wait.
 * Idempotent; a later true call escalates a draining shutdown. New cancellation does not overwrite
 * a prior computation terminal state. Visits each outstanding task once; never invokes user callbacks.
 */
bool executor_shutdown(executor_t *executor, bool cancel);

/** @brief Wait for no queued/running tasks; zero checks, LOCK_WAIT_FOREVER blocks, finite waits use monotonic time. */
bool executor_wait(executor_t *executor, uint64_t milliseconds);

/** @brief Copy a queue snapshot into stats; NULL arguments fail with EINVAL without modifying the output. */
bool executor_stats(executor_t *executor, executor_stats_t *stats);

/**
 * @brief Drain, join and free, accepting NULL; does not cancel unless shutdown(true) was requested earlier.
 * May block indefinitely for uncooperative callbacks. Own-worker destruction fails with EDEADLK,
 * retaining ownership. Exclude all concurrent external pool operations, including another free.
 * Task handles and their results remain valid afterwards.
 */
bool executor_free(executor_t *executor);

/** @brief Retain a task handle; NULL gives EINVAL, reference overflow gives EOVERFLOW. */
executor_task_t *executor_task_retain(executor_task_t *task);

/** @brief Release a reference; final release disposes of any result/diagnostic/control, accepting NULL. */
void executor_task_free(executor_task_t *task);

/** @brief Read lifecycle state; NULL gives EXECUTOR_TASK_INVALID and EINVAL. */
executor_task_status_t executor_task_status(const executor_task_t *task);

/**
 * @brief Wait for terminal publication, not necessarily success; EBUSY/ETIMEDOUT mean still pending.
 * Zero checks; LOCK_WAIT_FOREVER blocks. A worker may only check unfinished tasks, not block on them
 * (EDEADLK), preventing bounded-pool dependency deadlocks. Timeout never cancels work.
 */
bool executor_task_wait(executor_task_t *task, uint64_t milliseconds);

/** @brief Request cooperative cancellation through the task's shared control; NULL is harmless. */
void executor_task_cancel(executor_task_t *task);

/** @brief Borrow a successful result until the task reference is released; pending gives EBUSY, failure its errno. */
bool executor_task_result(const executor_task_t *task, void **result);

/** @brief Return the published failure code, zero for success; pending returns EBUSY, NULL returns EINVAL. */
int executor_task_error(const executor_task_t *task);

/** @brief Retain an optional completed-task diagnostic; NULL with unchanged errno means absent, pending sets EBUSY. */
diagnostic_t *executor_task_diagnostic(const executor_task_t *task);

/** @brief Retain the task's computation control for inspection/progress/cancellation; NULL task gives EINVAL. */
computation_t *executor_task_computation(const executor_task_t *task);

#endif
