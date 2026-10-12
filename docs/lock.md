# Locks and completion states

Include `lock-t.h`. `lock_t` is an opaque class cluster: every handle has the same
public type, with specialised backend operations selected by `lock_new()`.
It is native, process-local synchronisation built on pthreads, not browser
WebAssembly threading or shared-memory inter-process locking.

[`executor_t`](executor.md) uses mutex clones for its bounded task queue and
persistent events for work availability, idle detection and task completion.
Applications can use the same public lock API to protect data shared by callbacks.

## Choose the purpose

| Kind | Purpose | Initial argument | Wait/unlock behaviour |
| --- | --- | --- | --- |
| `LOCK_MUTEX` | One owner of a critical section | `0` | Wait acquires exclusion; repeated blocking acquisition by the same thread reports `EDEADLK`. |
| `LOCK_RECURSIVE` | Nested critical sections on the owning thread | `0` | Each successful acquisition adds one level; unlock releases one. |
| `LOCK_READ_WRITE` | Concurrent readers, exclusive writers | `0` | `lock_read()` acquires a reader; ordinary wait/try acquires a writer. |
| `LOCK_SEMAPHORE` | Bound concurrent resource use | Positive permit count | Wait takes a permit; unlock returns one. This is a bounded permit pool, not an arbitrary posting counter. |
| `LOCK_EVENT` | Task completion or another persistent state | `0` unset, `1` set | Wait observes completion without consuming it; signal sets it, reset clears it. No acquisition to unlock. |

POSIX distinguishes normal, error-checking and recursive mutexes; reader/writer
locks and semaphores serve different purposes. MARS defaults to checked mutex
semantics rather than exposing undefined wrong-owner unlocking. Robust owner-death
recovery is a separate protocol requiring application data repair; it is not
silently approximated here. See the [POSIX mutex types](https://pubs.opengroup.org/onlinepubs/7908799/xsh/pthread_mutexattr_settype.html)
and [Linux robust-mutex contract](https://man7.org/linux/man-pages/man3/pthread_mutexattr_getrobust.3.html).

The backends implement resource predicates and acquisition accounting; a common
pthread mutex/condition monitor makes those transitions atomic and sleeps while
unavailable. There are no spinning wait loops, raw pthread handles or growing
kind-dispatch switches in the public API. This is not a promise of FIFO fairness,
writer preference, real-time scheduling or starvation freedom.

## Ownership, destruction and scope cleanup

`lock_new(kind, initial)` returns one owned, initially unacquired handle.
`lock_clone(lock)` returns another unacquired handle referencing the **same**
state. Clone before giving a handle to another concurrent owner or using it as a
scoped guard. This is not an independent mutex and does not copy acquisitions.

Acquisitions belong to the handle and the acquiring thread. Use separate clones
for concurrent owners. `lock_free(lock)` releases **all** acquisitions belonging
to that handle, then drops its state reference. It does not release another
handle's acquisitions. A mutex or semaphore held by another thread cannot be
unlocked by destroying its handle: this fails with `EPERM`, retaining ownership.
Unacquired handles may be transferred between threads.

The shared state is reclaimed only after its last handle is released. Thus an
owner can free its acquired handle, waking a waiter whose separate handle keeps
the state alive. Protect each exact handle's lifetime: never race its destruction
with a waiter, clone, check or any other operation through that same pointer.

`LOCK_AUTO(name)` declares an automatic `lock_t *` variable with GCC/Clang's
`cleanup(lock_cleanup)` attribute. Always initialise it explicitly, for example
`LOCK_AUTO(guard) = lock_clone(mutex);` or `LOCK_AUTO(guard) = NULL;`. The macro
neither clones nor acquires implicitly; keep those operations visible. It is
defined only for GCC/Clang, without a silent no-op fallback on other compilers.
On normal lexical scope exit, including early return, the
callback frees that handle and releases its acquisitions. Explicit `lock_cleanup`
also clears the variable on success and preserves `errno`. Do not also call
`lock_free()` on an attributed pointer without setting it to NULL.

Cleanup must run on the acquiring thread. On misuse it leaves the pointer
unchanged and sets `errno`; an automatic cleanup cannot report that failure to
its caller. It is not a `longjmp`, process-exit, signal or forced-cancellation
hook. Cleanup callbacks return normally, as required by the
[GCC cleanup attribute](https://gcc.gnu.org/onlinedocs/gcc-15.2.0/gcc/Common-Variable-Attributes.html#index-cleanup).

Do not exit a thread holding an acquisition without releasing it. These are not
robust locks, and owner death is not repaired. No reader-to-writer upgrade or
writer-to-reader downgrade is supported, including through another clone; release
first and revalidate shared data after reacquisition. Nested reader acquisitions
on one handle require matching unlocks, or one final destruction.

## API and errors

Constructors return NULL on failure. Boolean functions return false with the
calling thread's `errno` set; success does not clear `errno`. NULL is accepted by
`lock_free()` and `lock_cleanup()`, but otherwise invalid. `lock_kind(NULL)`
returns `LOCK_INVALID` and sets `EINVAL`.

| API | Contract |
| --- | --- |
| `lock_new(kind, initial)` | Construct a selected backend; validate the initial argument as above. |
| `lock_clone(lock)` | Share state with independent acquisition ownership. |
| `lock_kind(lock)` | Return the immutable backend kind. |
| `lock_wait(lock)` | Wait indefinitely for exclusion, one permit, or a set event. |
| `lock_wait_for(lock, milliseconds)` | Zero means non-blocking; `LOCK_WAIT_FOREVER` means infinite; otherwise one monotonic millisecond budget. |
| `lock_read(lock, milliseconds)` | Same timeout policy, shared ownership for `LOCK_READ_WRITE` only. |
| `lock_try(lock)` | Atomic non-blocking exclusive/permit acquisition, or observation of a set event. |
| `lock_check(lock)` | Snapshot of exclusive/permit availability or event completion, with no acquisition or mutation. |
| `lock_unlock(lock)` | Release one acquisition owned by this handle and thread. |
| `lock_signal(lock)` | Set a manual-reset event and wake all interested waiters. |
| `lock_reset(lock)` | Clear an event; coordinate reset with all observers. |
| `lock_free(lock)` | Release all this handle's acquisitions and its state reference; NULL succeeds. |
| `lock_cleanup(slot)` | Free-and-clear through an owning pointer; suitable for GCC/Clang scope cleanup. |
| `LOCK_AUTO(name)` | Declare a scope-cleaned owning pointer; supply an explicit initialiser and acquire separately. |

An unavailable try/check uses `EBUSY`. Finite waits that expire use `ETIMEDOUT`.
Invalid arguments use `EINVAL`; unsupported backend operations use `ENOTSUP`;
wrong-owner releases use `EPERM`. Detectable blocking self-deadlocks use
`EDEADLK`; general multi-lock cycles are the application's responsibility.
An unrepresentable absolute deadline or ownership/reference count uses
`EOVERFLOW`. Allocation failure uses `ENOMEM`.

Checks reserve nothing. Another thread can acquire after a successful check;
use `lock_try()` when availability must be reserved atomically. Successful waits
and acquisitions establish visibility of writes published before the corresponding
unlock or event signal. Spurious native condition wakes are handled internally,
without restarting the timeout. Wall-clock changes do not alter the budget.

Completion events remain set, so a late waiter does not miss a signal. However,
signal immediately followed by reset can still be missed; reset requires an
application-level generation protocol. Destroying an event does **not** signal
completion: a vanished notifier is not proof of successful work. Keep a live
notifier or use a finite wait. To observe actual OS process exit, a supervising
caller must first reap/wait for that process and then signal an event; these
handles alone do not monitor PIDs or operate across address spaces.

## Scoped critical section

```c
#include <stdio.h>

#include "lock-t.h"

static bool lock_example_increment(lock_t *mutex, int *value)
{
    LOCK_AUTO(guard) = lock_clone(mutex);
    if (!guard || !lock_wait(guard))
        return false;
    ++*value;
    return true;
}

int main(void)
{
    lock_t *mutex = lock_new(LOCK_MUTEX, 0);
    int value = 41;
    bool ok = mutex && lock_example_increment(mutex, &value) && lock_try(mutex);
    if (ok)
        printf("Value: %d; lock available again\n", value);
    ok = lock_free(mutex) && ok;
    return ok ? 0 : 1;
}
```

Output:

```text
Value: 42; lock available again
```

## Persistent completion state

This minimal example demonstrates observation rather than mutual exclusion.
For completion signalled from a real worker, see the [thread examples](thread.md).

```c
#include <errno.h>
#include <stdio.h>

#include "lock-t.h"

int main(void)
{
    lock_t *event = lock_new(LOCK_EVENT, 0);
    bool ok = event && !lock_check(event) && errno == EBUSY;
    if (ok)
        puts("Pending");
    ok = ok && lock_signal(event) && lock_wait_for(event, 1000);
    if (ok)
        puts("Complete");
    ok = lock_free(event) && ok;
    return ok ? 0 : 1;
}
```

Output:

```text
Pending
Complete
```

Run `make -j1 test_lock`. Tests cover all five backends, timed waits, wrong-owner
errors, cloned lifetimes, concurrent readers, permit accounting, normal/early scope
exit and both README examples last. Neither this API nor automatic unlocking
makes arbitrary MARS mathematical state safe to share across threads.
