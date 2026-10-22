/**
 * @file computation.c
 * @brief Atomic shared work accounting and cooperative calculation termination.
 *
 * Keeps immutable options beside reference-counted atomic counters and a sticky
 * status. Batched checkpoints amortise monotonic clock reads, while scope and
 * completion boundaries always check the deadline freshly. Reports call
 * application observers directly without locks; this module neither schedules
 * work nor tries to interrupt mathematical operations or reclaim their resources.
 */
#include <errno.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <time.h>

#include "computation-t.h"
#define MARS_SHARED_COMPUTATION_INTERNAL_ACCESS
#include "internal/computation_internal.h"

typedef struct _computation_t {
    atomic_uint_fast64_t references;
    atomic_uint_fast64_t work_used;
    atomic_int status;
    atomic_uint_fast64_t deadline_polls;
    uint64_t deadline_ns;
    computation_options_t options;
} computation_t;

static _Thread_local computation_t *computation_scope;

enum { COMPUTATION_DEADLINE_POLL_INTERVAL = 64 };

static bool computation_now(uint64_t *now)
{
    struct timespec time;
    if (clock_gettime(CLOCK_MONOTONIC, &time) != 0)
        return false;
    if (time.tv_sec < 0 || (uint64_t)time.tv_sec > (UINT64_MAX - (uint64_t)time.tv_nsec) / 1000000000u) {
        errno = EOVERFLOW;
        return false;
    }
    *now = (uint64_t)time.tv_sec * 1000000000u + (uint64_t)time.tv_nsec;
    return true;
}

static bool computation_transition(computation_t *computation, computation_status_t status)
{
    int active = COMPUTATION_ACTIVE;
    return atomic_compare_exchange_strong(&computation->status, &active, status);
}

static bool computation_active(computation_status_t status)
{
    static const int errors[] = {
        [COMPUTATION_ACTIVE]           = 0,
        [COMPUTATION_COMPLETED]        = EALREADY,
        [COMPUTATION_CANCELLED]        = ECANCELED,
        [COMPUTATION_TIMED_OUT]        = ETIMEDOUT,
        [COMPUTATION_BUDGET_EXHAUSTED]  = E2BIG,
        [COMPUTATION_FAILED]           = EIO,
    };
    if (status == COMPUTATION_ACTIVE)
        return true;
    errno = status == COMPUTATION_INVALID ? EINVAL : errors[status];
    return false;
}

/* Copy configuration and establish a deadline once, before publishing shared state. */
computation_t *computation_new(const computation_options_t *options)
{
    computation_t *computation = calloc(1, sizeof(*computation));
    if (!computation)
        return NULL;
    if (options)
        computation->options = *options;
    if (computation->options.timeout_ms) {
        uint64_t now;
        if (!computation_now(&now)) {
            free(computation);
            return NULL;
        }
        if (computation->options.timeout_ms > (UINT64_MAX - now) / 1000000u) {
            free(computation);
            errno = EOVERFLOW;
            return NULL;
        }
        computation->deadline_ns = now + computation->options.timeout_ms * 1000000u;
    }
    atomic_init(&computation->references, 1);
    atomic_init(&computation->work_used, 0);
    atomic_init(&computation->status, COMPUTATION_ACTIVE);
    atomic_init(&computation->deadline_polls, 0);
    return computation;
}

/* Each owner retains the same control, including its budget and stop reason. */
computation_t *computation_retain(computation_t *computation)
{
    if (!computation) {
        errno = EINVAL;
        return NULL;
    }
    uint_fast64_t references = atomic_load(&computation->references);
    do {
        if (references == UINT_FAST64_MAX) {
            errno = EOVERFLOW;
            return NULL;
        }
    } while (!atomic_compare_exchange_weak(&computation->references, &references, references + 1));
    return computation;
}

/* Releasing an owner never changes the state seen by remaining owners. */
void computation_free(computation_t *computation)
{
    if (computation && atomic_fetch_sub(&computation->references, 1) == 1)
        free(computation);
}

/* Cancellation remains eager; only repeated deadline reads are amortised. */
static computation_status_t computation_poll(computation_t *computation, bool force_clock)
{
    if (!computation) {
        errno = EINVAL;
        return COMPUTATION_INVALID;
    }
    if (atomic_load(&computation->status) == COMPUTATION_ACTIVE && computation->options.cancelled &&
        computation->options.cancelled(computation->options.argument))
        computation_transition(computation, COMPUTATION_CANCELLED);
    if (atomic_load(&computation->status) == COMPUTATION_ACTIVE && computation->deadline_ns &&
        (force_clock || (atomic_fetch_add_explicit(&computation->deadline_polls, 1, memory_order_relaxed) %
                        COMPUTATION_DEADLINE_POLL_INTERVAL) == 0)) {
        uint64_t now;
        if (!computation_now(&now))
            computation_transition(computation, COMPUTATION_FAILED);
        else if (now >= computation->deadline_ns)
            computation_transition(computation, COMPUTATION_TIMED_OUT);
    }
    return atomic_load(&computation->status);
}

/* Explicit inspection always observes the current monotonic deadline. */
computation_status_t computation_status(computation_t *computation)
{
    return computation_poll(computation, true);
}

/* Reserve before work starts, with a single shared bound across concurrent callers. */
bool computation_checkpoint(computation_t *computation, uint64_t work)
{
    if (!computation_active(computation_poll(computation, false)))
        return false;
    if (work) {
        uint_fast64_t used = atomic_load(&computation->work_used);
        uint64_t limit = computation->options.work_limit;
        do {
            if (limit && work > limit - used) {
                computation_transition(computation, COMPUTATION_BUDGET_EXHAUSTED);
                return computation_active(atomic_load(&computation->status));
            }
            uint64_t next = work > UINT64_MAX - used ? UINT64_MAX : used + work;
            if (atomic_compare_exchange_weak(&computation->work_used, &used, next))
                break;
            if (!computation_active(computation_poll(computation, false)))
                return false;
        } while (true);
    }
    return computation_active(atomic_load(&computation->status));
}

/* Cancellation never overwrites a previously published terminal result. */
void computation_cancel(computation_t *computation)
{
    if (computation)
        computation_transition(computation, COMPUTATION_CANCELLED);
}

/* Algorithms can distinguish mathematical failure from external stopping. */
void computation_fail(computation_t *computation)
{
    if (computation)
        computation_transition(computation, COMPUTATION_FAILED);
}

/* Only the owner of the whole operation should publish successful completion. */
bool computation_complete(computation_t *computation)
{
    if (!computation_active(computation_status(computation)))
        return false;
    if (computation_transition(computation, COMPUTATION_COMPLETED))
        return true;
    return computation_active(computation_status(computation));
}

/* Report reserved work, which can include operations cancelled after reservation. */
uint64_t computation_work_used(const computation_t *computation)
{
    if (!computation) {
        errno = EINVAL;
        return 0;
    }
    return atomic_load(&computation->work_used);
}

/* Observers run outside synchronisation and can request cancellation themselves. */
bool computation_report(computation_t *computation, const char *stage, uint64_t completed, uint64_t total)
{
    if (!computation || !stage || (total && completed > total)) {
        errno = EINVAL;
        return false;
    }
    if (!computation->options.progress)
        return computation_checkpoint(computation, 0);
    computation_progress_t progress = {
        .stage = stage,
        .completed = completed,
        .total = total,
        .work_used = computation_work_used(computation),
        .status = computation_status(computation),
    };
    computation->options.progress(&progress, computation->options.argument);
    return computation_active(computation_status(computation));
}

/* Scope native work without allowing a nested caller to replace its outer allowance. */
bool computation_run(computation_t *computation, computation_callback_t callback, void *argument)
{
    if (!computation || !callback) {
        errno = EINVAL;
        return false;
    }
    if (computation_scope && computation_scope != computation) {
        errno = EBUSY;
        return false;
    }
    if (!computation_active(computation_status(computation)) || !computation_retain(computation))
        return false;
    computation_t *previous = computation_scope;
    computation_scope = computation;
    bool ok = callback(argument);
    bool active = computation_active(computation_status(computation));
    int error = errno;
    computation_scope = previous;
    computation_free(computation);
    errno = error;
    return ok && active;
}

/* Borrow only for the lifetime of the current scope. */
computation_t *computation_current(void)
{
    return computation_scope;
}

/* Shared optional checkpoint for native algorithm boundaries, not cleanup paths. */
bool computation_step_current(const char *stage)
{
    return !computation_scope || (computation_checkpoint(computation_scope, 1) &&
        (!computation_scope->options.progress ||
         computation_report(computation_scope, stage, computation_work_used(computation_scope), 0)));
}

/* Consumers discard stopped outputs before returning to their caller. */
bool computation_poll_current(void)
{
    return !computation_scope || computation_checkpoint(computation_scope, 0);
}
