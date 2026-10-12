/**
 * @file computation-t.h
 * @brief Shared cooperative cancellation, monotonic deadlines and calculation budgets.
 *
 * computation_t controls work, independently of mathematical assumptions in
 * expr_context_t or execution ownership in thread_t. Retained references share
 * one budget and sticky terminal status across nested operations and native
 * threads. Checkpoints reserve work before it starts; they do not interrupt an
 * operation already in progress. Only the top-level owner declares completion
 * or mathematical failure. No background thread or timer is created.
 *
 * Configuration is immutable. Atomic state supports concurrent checkpoints,
 * cancellation and inspection; protect a reference's lifetime during access.
 * Callbacks run synchronously on reporting threads, without an internal lock,
 * and must be thread-safe if reports can overlap. This native implementation
 * uses CLOCK_MONOTONIC; it is not a browser worker or a signal-handler API.
 */
#ifndef MARS_COMPUTATION_H
#define MARS_COMPUTATION_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Opaque shared calculation control; release each owned reference with computation_free(). */
typedef struct _computation_t computation_t;

/** @brief The first terminal transition wins; ACTIVE does not imply a worker is running. */
typedef enum {
    COMPUTATION_INVALID = -1,
    COMPUTATION_ACTIVE,
    COMPUTATION_COMPLETED,
    COMPUTATION_CANCELLED,
    COMPUTATION_TIMED_OUT,
    COMPUTATION_BUDGET_EXHAUSTED,
    COMPUTATION_FAILED
} computation_status_t;

/** @brief Borrowed progress notification; total == 0 means the stage's total work is unknown. */
typedef struct {
    const char *stage; /**< Borrowed label, valid only during the callback. */
    uint64_t completed; /**< Caller-supplied stage work, not a percentage. */
    uint64_t total; /**< Known stage total, or zero; distinct from the control's work limit. */
    uint64_t work_used; /**< Aggregate reserved work at notification time. */
    computation_status_t status; /**< Status at notification time; concurrent changes may follow. */
} computation_progress_t;

/**
 * @brief Observe progress on the reporting thread; the notification must not be retained.
 * May cancel or inspect the control, but must not recursively report or destroy the caller's reference.
 * The callback and borrowed argument must remain valid until all reporting calls have returned.
 */
typedef void (*computation_progress_callback_t)(const computation_progress_t *progress, void *argument);

/**
 * @brief Poll external cancellation on a checkpoint thread; true requests cancellation.
 * Must be quick, thread-safe when shared, and must not call computation APIs or mathematical code.
 * This runs in ordinary code, never from a signal handler; a signal handler may only set a signal-safe flag.
 */
typedef bool (*computation_cancel_callback_t)(void *argument);

/** @brief Scoped native work; return success and release temporary resources normally, without longjmp or thread exit. */
typedef bool (*computation_callback_t)(void *argument);

/** @brief Immutable constructor options; zero-initialisation gives unlimited work/time and no callback. */
typedef struct {
    uint64_t work_limit; /**< Zero means unlimited; work units are defined by the consuming algorithm. */
    uint64_t timeout_ms; /**< Zero means unlimited; otherwise measured from construction, not first use. */
    computation_progress_callback_t progress; /**< Optional synchronous observer. */
    void *argument; /**< Borrowed callback context, never freed by computation_t. */
    computation_cancel_callback_t cancelled; /**< Optional external cancellation poll, sharing argument. */
} computation_options_t;

/**
 * @brief Create an owned control from copied options, or unlimited defaults for NULL.
 * Returns NULL with ENOMEM, EOVERFLOW for an unrepresentable deadline, or a clock error.
 */
computation_t *computation_new(const computation_options_t *options);

/** @brief Retain shared state, not a snapshot; NULL fails with EINVAL and reference overflow with EOVERFLOW. */
computation_t *computation_retain(computation_t *computation);

/** @brief Release an owned reference, accepting NULL; never cancels, completes or waits for work. */
void computation_free(computation_t *computation);

/**
 * @brief Poll for a stop and atomically reserve work; zero only polls.
 * A reservation exceeding the remaining limit fails without consuming any of it.
 * Exactly using the limit is allowed; the next positive reservation exhausts it.
 * Unlimited counters saturate at UINT64_MAX. Concurrent stopping may leave already
 * reserved work counted even when it is not executed; the counter is not a completion count.
 * False sets EINVAL, EALREADY, ECANCELED, ETIMEDOUT, E2BIG or EIO according to status.
 */
bool computation_checkpoint(computation_t *computation, uint64_t work);

/** @brief Request cooperative cancellation; NULL is harmless and a prior terminal status is preserved. */
void computation_cancel(computation_t *computation);

/** @brief Declare a mathematical/algorithmic failure; NULL is harmless and prior terminal status is preserved. */
void computation_fail(computation_t *computation);

/**
 * @brief Declare top-level success if still active and within its deadline.
 * Returns false with the same errno policy as computation_checkpoint() if already stopped/completed.
 * Join or otherwise coordinate workers before completing; this does not wait for in-flight work.
 */
bool computation_complete(computation_t *computation);

/** @brief Poll external cancellation and deadline; NULL returns COMPUTATION_INVALID with EINVAL. */
computation_status_t computation_status(computation_t *computation);

/** @brief Read aggregate reserved work; NULL returns zero with EINVAL. */
uint64_t computation_work_used(const computation_t *computation);

/**
 * @brief Report a stage and poll again after the callback, allowing the observer to cancel.
 * A stopped control still supplies a final notification when explicitly reported.
 * Returns whether work may continue, with checkpoint errno meanings. NULL stage or a
 * completed count larger than a non-zero total fails with EINVAL without a callback.
 * Notifications are explicit, not automatically emitted for every checkpoint or state change.
 */
bool computation_report(computation_t *computation, const char *stage, uint64_t completed, uint64_t total);

/**
 * @brief Run a callback under a retained, thread-local control inherited by cooperating native modules.
 * Returns false if callback fails or the control stops; outputs must then be discarded by their owner.
 * Does not complete or fail the control automatically. Nested scopes must use the same control,
 * otherwise EBUSY prevents bypassing the outer budget. NULL control/callback gives EINVAL.
 * Restores the previous scope on normal return. Not a licence to share mutable mathematical graphs
 * between threads. Observers must not re-enter mathematical work under this scope.
 */
bool computation_run(computation_t *computation, computation_callback_t callback, void *argument);

/** @brief Borrow the current thread's scoped control, or NULL outside computation_run(); never release this reference. */
computation_t *computation_current(void);

#endif
