# Native threads

For a bounded queue over reusable native workers, use [`executor_t`](executor.md).
It builds on `thread_t` and `lock_t`, adding independent task handles, cooperative
controls and structured diagnostics. Direct `thread_t` use remains appropriate
when an application needs to own one callback thread's lifecycle explicitly.

For cooperative cancellation and shared calculation budgets within a callback,
use [`computation_t`](computation.md). It does not replace thread completion or
joining: after requesting cancellation, still join before releasing borrowed
callback arguments. The worker must reach checkpoints to observe a stop request.

Include `thread-t.h` for the opaque `thread_t` callback handle. Synchronisation
belongs to the [`lock_t` class cluster](lock.md); there are no embedded thread
mutex or condition types. Each thread owns a completion-event handle, and the
worker holds an independent reference so detachment cannot invalidate it.

This is Linux pthread support, not browser WebAssembly threading. Compile and
link with `-pthread`; `make check-deps` checks this existing platform dependency.

## API and ownership

| API | Behaviour |
| --- | --- |
| `thread_new(callback, argument)` | Start a required `thread_callback_t` with optional borrowed context. Returns an owned joinable handle or NULL with errno. |
| `thread_wait(thread, milliseconds)` | Observe callback completion through its `lock_t` event without joining. Zero is non-blocking; `LOCK_WAIT_FOREVER` is infinite; other values are monotonic budgets. |
| `thread_check(thread)` | Non-blocking persistent completion snapshot. |
| `thread_join(thread, result)` | Wait for native termination and optionally collect the callback result exactly once. |
| `thread_detach(thread)` | Relinquish join ownership without stopping or waiting for the callback. |
| `thread_free(thread)` | Release a joined/detached handle and its completion reference, or accept NULL. Still-joinable handles fail with `EBUSY`. |

Callbacks may start before construction returns. Context and result storage
remain application-owned; the module copies or frees neither. Callbacks must
return normally: do not call `pthread_exit`, externally cancel the worker, or
terminate with a held lock. Never return a pointer to a callback's local variable.

A completion wait means the callback has returned and published its writes,
not that native thread resources have been reaped. Join to collect its result.
If a wait times out, the callback is still running; keep its context alive and
arrange cooperative shutdown or join before releasing it.

Serialise operations on each thread handle. A join must not overlap another
lifecycle operation. Boolean functions fail with `errno` set; success does not
clear it. Pending checks/zero-time waits use `EBUSY`, finite timeouts use
`ETIMEDOUT`, invalid/consumed join handles use `EINVAL`, and self-join uses
`EDEADLK`. Blocking self-wait is also rejected while the handle remains joinable;
a detached callback must never wait on itself. Failed joins leave result slots
unchanged.

Detaching never cancels work. It remains possible to check/wait for a detached
callback while retaining its handle, but not to join it. If the handle is freed
early, the callback must independently own or retain its context, locks and
notifications. The internal completion reference does not extend application
object lifetimes.

## Wait and collect a result

```c
#include <stdio.h>

#include "thread-t.h"

static void *thread_example_double(void *argument)
{
    int *value = argument;
    *value *= 2;
    return value;
}

int main(void)
{
    int value = 21;
    void *result = NULL;
    thread_t *thread = thread_new(thread_example_double, &value);
    bool completed = thread && thread_wait(thread, 1000);
    bool joined = thread && thread_join(thread, &result);
    bool ok = completed && joined;
    if (ok)
        printf("Result: %d\n", *(int *)result);
    ok = thread_free(thread) && ok;
    return ok ? 0 : 1;
}
```

Output:

```text
Result: 42
```

The join still runs if the bounded wait expires, keeping the stack context alive.

## Protect shared state with lock_t

Each callback owns a clone of the same mutex. `LOCK_AUTO(name)` scope cleanup releases that
handle on every normal callback return, including a return with an acquisition
still held. This example aborts if initial fixtures cannot be allocated; production
callers should apply their own resource-failure policy.

```c
#include <stdio.h>
#include <stdlib.h>

#include "thread-t.h"

typedef struct {
    lock_t *mutex;
    int *value;
} thread_example_counter_t;

static void thread_example_require(bool success)
{
    if (!success)
        abort();
}

static void *thread_example_increment(void *argument)
{
    thread_example_counter_t *counter = argument;
    LOCK_AUTO(guard) = counter->mutex;
    for (unsigned i = 0; i < 1000; ++i) {
        if (!lock_wait(guard))
            return counter;
        ++*counter->value;
        if (!lock_unlock(guard))
            return counter;
    }
    return NULL;
}

int main(void)
{
    int value = 0;
    lock_t *mutex = lock_new(LOCK_MUTEX, 0);
    thread_example_require(mutex != NULL);
    thread_example_counter_t work[2] = {{lock_clone(mutex), &value}, {lock_clone(mutex), &value}};
    thread_example_require(work[0].mutex && work[1].mutex);
    thread_t *first = thread_new(thread_example_increment, &work[0]);
    if (!first)
        lock_free(work[0].mutex);
    thread_t *second = thread_new(thread_example_increment, &work[1]);
    if (!second)
        lock_free(work[1].mutex);
    bool ok = first && second;
    void *result = NULL;
    if (first) {
        bool joined = thread_join(first, &result);
        ok = joined && !result && ok;
        ok = thread_free(first) && ok;
    }
    if (second) {
        bool joined = thread_join(second, &result);
        ok = joined && !result && ok;
        ok = thread_free(second) && ok;
    }
    ok = lock_free(mutex) && ok;
    if (ok)
        printf("Counter: %d\n", value);
    return ok ? 0 : 1;
}
```

Output:

```text
Counter: 2000
```

## Signal an application completion event

Use `thread_wait()` when you only need to know that a callback has returned.
An explicit `LOCK_EVENT` is useful when the application needs its own notification,
such as publishing a result before the callback's final cleanup. The worker owns
a clone, so its scope cleanup cannot destroy the caller's event.

This example waits without polling. The event stays signalled for a subsequent
`lock_check()`. A timeout does not stop the worker: the caller still joins before
releasing its stack context. An unexpected join failure aborts rather than
allowing that borrowed context to disappear while the worker may still use it.

```c
#include <stdio.h>
#include <stdlib.h>

#include "thread-t.h"

typedef struct {
    lock_t *ready;
    int value;
} thread_example_work_t;

static void *thread_example_publish(void *argument)
{
    thread_example_work_t *work = argument;
    LOCK_AUTO(ready) = work->ready;
    work->value = 6 * 7;
    return lock_signal(ready) ? NULL : work;
}

int main(void)
{
    lock_t *ready = lock_new(LOCK_EVENT, 0);
    thread_example_work_t work = {.ready = ready ? lock_clone(ready) : NULL};
    thread_t *thread = work.ready ? thread_new(thread_example_publish, &work) : NULL;
    if (!thread) {
        lock_free(work.ready);
        lock_free(ready);
        return 1;
    }
    bool signalled = lock_wait_for(ready, 10000);
    void *result = NULL;
    /* Join even after a timeout: the callback still borrows the stack context. */
    if (!thread_join(thread, &result))
        abort();
    bool ok = signalled && !result && lock_check(ready) && work.value == 42;
    ok = thread_free(thread) && ok;
    ok = lock_free(ready) && ok;
    if (ok)
        printf("Completion signalled; result: %d\n", work.value);
    return ok ? 0 : 1;
}
```

Output:

```text
Completion signalled; result: 42
```

All three examples run last in `tests/thread/test_thread.c`; the Markdown runner
compiles and executes the complete programmes. Run `make -j1 test_thread`.

## Existing threading and limitations

The native configuration tool's progress reporter now uses `thread_t`, cloned
`lock_t` mutex handles and a completion event for prompt shutdown. Existing
`array`, `bitset` and HTTP implementation mutexes remain unchanged.

MARS Lab builds freestanding WebAssembly. Large response decoding uses a dedicated
browser Worker with a separate codec instance and private memory; no shared-memory
WASM threads are enabled. UI updates stay on the browser thread, with message-based
completion and a bounded C-owned job queue. This does not use the native pthread
implementation of `thread_t` or `lock_t`, and never blocks the UI on a mutex.
Native request workers continue to use process isolation. See the
[Lab browser implementation](mars-lab.md#background-codec-jobs).

Some expression state is thread-local, but number allocation scopes and other
settings still include process-global mutable state. **Do not run arbitrary MARS
mathematics concurrently** until the relevant modules are audited and their
individual contracts permit it. Independent objects alone do not remove global
races. Neither `thread_t` nor `lock_t` changes numeric defaults or per-thread
precision policy.

This first version supplies no pool, scheduler, priority settings, forced
cancellation or native-handle escape hatch. Use a lock-protected stop predicate or
a completion event for cooperative shutdown, then join. These APIs are not
async-signal-safe. A forked child of a multithreaded process must not use inherited
MARS locks before exec.
