# Bounded native execution

Include `executor-t.h` for opaque `executor_t` pools and reference-counted
`executor_task_t` handles. Fixed [`thread_t`](thread.md) workers consume a bounded
FIFO queue. Synchronisation belongs to [`lock_t`](lock.md); no duplicate pthread
mutex implementation is embedded in the executor.

Each task runs inside [`computation_run()`](computation.md), so cooperating
modules inherit its cancellation, deadline and work budget. Errors and warnings
use immutable [`diagnostic_t`](diagnostic.md) records. No new external dependency
is required beyond existing native pthread support.

This is not a process pool, browser WebAssembly backend or forced-interruption
facility. MARS Lab uses it to supervise independent TeX card subprocesses, not
to replace isolated mathematical workers or browser workers. Scheduling callbacks
does **not** make every MARS module thread-safe: arrange independent thread-safe
data and initialise shared configuration before starting work. Do not change
global number precision concurrently or share mutable mathematical graphs.
Isolation-sensitive calculations may still require separate processes.

## Pool API

| API | Behaviour |
| --- | --- |
| `executor_new(workers, capacity)` | Both sizes must be positive. Capacity bounds queued tasks, separately from running workers. |
| `executor_submit(executor, callback, argument, options)` | Enqueue without waiting for space; return an owned task handle. NULL options use borrowed data and a fresh unlimited control. |
| `executor_shutdown(executor, cancel)` | Close submissions; optionally request cancellation of queued/running tasks. Does not wait. Repeated calls may escalate draining to cancelling. |
| `executor_wait(executor, milliseconds)` | Wait until no queued/running tasks remain; zero checks and `LOCK_WAIT_FOREVER` waits indefinitely. |
| `executor_stats(executor, stats)` | Copy consistent queued, running, completed and accepting fields. |
| `executor_free(executor)` | Close submissions, drain and join workers, then free; accept NULL. Does not automatically cancel. |

Submission takes a mutex briefly: “without waiting” means no wait for space,
not lock-free execution. Full queues give `EAGAIN`, closed pools `ESHUTDOWN`.
Invalid pointers/zero sizes give `EINVAL`, unrepresentable allocation sizes
`EOVERFLOW`; allocation and native-thread errors propagate. Partial construction
joins every started worker.

Queue operations use constant-time ring-buffer accesses. Only explicit shutdown
cancellation visits every outstanding queue entry and fixed worker slot once.
FIFO governs dispatch, not completion order with multiple workers. There are no
priorities, retries or unbounded fallback threads. Capacity is chosen explicitly:
four workers and 64 slots allow at most 68 accepted unfinished tasks. The slots
store pointers, excluding task records and application-owned data.

Submission, shutdown, statistics and waits may overlap on a live pool. Serialise
destruction against all external pool operations. Idle waits are snapshots while
submission remains open; close first if permanent draining is needed. Completed
statistics saturate at `UINT64_MAX` rather than wrapping.

## Task API

| API | Behaviour |
| --- | --- |
| `executor_task_retain(task)` | Retain a task; NULL gives `EINVAL`, reference overflow gives `EOVERFLOW`. |
| `executor_task_free(task)` | Release a reference, accepting NULL; never implicitly cancel or wait. |
| `executor_task_status(task)` | Read queued, running or terminal lifecycle state. |
| `executor_task_wait(task, milliseconds)` | Wait for terminal publication, regardless of success/failure. |
| `executor_task_cancel(task)` | Request cooperative cancellation, accepting NULL. |
| `executor_task_result(task, result)` | Borrow a successful result, including NULL; failed/pending calls preserve the output slot. |
| `executor_task_error(task)` | Return zero for success, the captured failure errno, `EBUSY` for pending or `EINVAL` for NULL. |
| `executor_task_diagnostic(task)` | Retain an optional completed-task diagnostic. NULL with unchanged errno means absent; pending sets `EBUSY`. |
| `executor_task_computation(task)` | Retain the control for progress, cancellation or inspection. |

States are `EXECUTOR_TASK_QUEUED`, `EXECUTOR_TASK_RUNNING`,
`EXECUTOR_TASK_SUCCEEDED`, `EXECUTOR_TASK_FAILED`, `EXECUTOR_TASK_CANCELLED`,
`EXECUTOR_TASK_TIMED_OUT` and `EXECUTOR_TASK_BUDGET_EXHAUSTED`; NULL inspection
returns `EXECUTOR_TASK_INVALID`. Terminal states never change. A terminal state
publishes all output fields and means the callback and argument cleanup have
finished. Multiple retained observers may wait and inspect independently.

Zero-time pending waits give `EBUSY`; finite monotonic timeouts give `ETIMEDOUT`.
A timeout does not cancel work or permit destruction of borrowed arguments.
Workers cannot block on unfinished task handles (`EDEADLK`), even in another
pool, nor block waiting for or destroy their own pool. They may check or submit
follow-up work, handling rejection. Avoid cyclic cross-pool dependencies yourself.

Tasks can communicate through application-owned shared state or message queues
protected by `lock_t`, or inspect completed task results through retained handles.
There is no built-in mailbox. A task may submit its successor once input is
ready, but the owner must keep submissions open until the chain has finished:
calling `executor_free()` immediately closes the pool, including worker submissions.

## Callback ownership and stopping

`executor_callback_t` receives an argument and initially NULL result/diagnostic
slots. Return true for success, false for failure. Set `errno` on failure; zero
becomes `EIO`. Transfer an owned diagnostic reference to the task. Diagnostics
may accompany success, for example warnings. A missing failure diagnostic is
generated in the `executor` domain; status/errno remain available even if that
allocation fails.

`executor_task_options_t` is copied:

- `computation`: optional retained shared control, otherwise fresh/unlimited.
  Deadlines include prior elapsed and queue time. Keep its observer context alive
  until work finishes.
- `argument_free`: optional ownership transfer **only on successful submission**.
  Runs once on a worker before publication, even if a stopped callback is skipped.
- `result_free`: disposes of partial/stopped output on a worker and successful
  output on final task release, possibly on a worker or observer thread.

Without a destructor the corresponding pointer stays application-managed.
Borrowed inputs must outlive callbacks; borrowed results must outlive readers.
Never return automatic storage or an allocation destroyed by `argument_free`.
The pool retains accepted tasks, so early handle release cannot lose cleanup.
Owned task results survive pool destruction. No callback/destructor runs under
the queue mutex. All callbacks, observers and destructors must return normally;
no `longjmp`, thread exit or external pthread cancellation. Destructors must not
block on their pool.

Cancellation sets the control's sticky stop state. Queued cancelled tasks remain
queued until reaped, so cancellation does not immediately free a slot. Running
callbacks must checkpoint or use cooperating modules. Stopped results are
discarded after callback return. Tasks never automatically complete/fail a shared
control: the top-level owner does so after coordinating all work. Do not complete
the scoped control inside a callback. A previously completed control produces
failure with `EALREADY`.

Stopping is observed at execution/publication boundaries. Later cancellation
cannot revoke published success. Cancelling a shared control affects its other
users. An uncooperative callback can delay shutdown indefinitely; this API never
claims it can safely kill one.

## Result lifetime beyond the pool

```c
#include <stdio.h>

#include "executor-t.h"

static bool executor_identity(void *argument, void **result, diagnostic_t **diagnostic)
{
    (void)diagnostic;
    *result = argument;
    return true;
}

int main(void)
{
    executor_t *pool = executor_new(2, 8);
    int value = 42;
    executor_task_t *task = pool ? executor_submit(pool, executor_identity, &value, NULL) : NULL;
    bool ok = task != NULL;
    ok = executor_free(pool) && ok;
    void *result = NULL;
    if (ok && executor_task_result(task, &result))
        printf("Result: %d\n", *(int *)result);
    else
        ok = false;
    executor_task_free(task);
    return ok ? 0 : 1;
}
```

Output:

```text
Result: 42
```

## Transport a structured failure

```c
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "executor-t.h"

static bool executor_failure(void *argument, void **result, diagnostic_t **diagnostic)
{
    (void)argument;
    *result = malloc(1);
    *diagnostic = diagnostic_new(DIAGNOSTIC_ERROR, "calculation", 12, "Invalid input", NULL, NULL);
    errno = EDOM;
    return false;
}

int main(void)
{
    executor_t *pool = executor_new(1, 4);
    executor_task_options_t options = {.result_free = free};
    executor_task_t *task = pool ? executor_submit(pool, executor_failure, NULL, &options) : NULL;
    bool ok = executor_free(pool) && task;
    diagnostic_t *error = ok ? executor_task_diagnostic(task) : NULL;
    string_t *text = error ? diagnostic_to_string(error) : NULL;
    if (text)
        printf("%s\n", string_c_str(text));
    ok = ok && text && executor_task_error(task) == EDOM;
    string_free(text);
    diagnostic_free(error);
    executor_task_free(task);
    return ok ? 0 : 1;
}
```

Output:

```text
error [calculation:12]: Invalid input
```

The partial result is freed; the diagnostic has a separate retained reference.
Both examples are mirrored at the end of `tests/executor/test_executor.c`.
