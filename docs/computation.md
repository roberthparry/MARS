# `computation_t`

[`executor_t`](executor.md) runs callbacks under this control in a bounded native
worker pool. Tasks inherit it through `computation_run()`; the executor never
automatically completes a shared control.

`computation_t` is an opaque, reference-counted control for cooperative
calculation stopping and progress reporting. Include `computation-t.h`.
It controls **work**, not mathematical assumptions: use
[`expr_context_t`](expr_context.md) for assumptions and binding snapshots, and
[`thread_t`](thread.md) for native worker ownership and joining.

## Ownership and configuration

`computation_new()` copies a `computation_options_t`. A NULL options pointer or
zero-initialised options means unlimited work and time, with no observer.
`work_limit` bounds reserved work units; `timeout_ms` establishes a monotonic
deadline measured from construction, including time spent waiting to start.
Zero disables each limit independently. Deadline overflow fails with
`EOVERFLOW`; allocation or clock failure also fails construction.

`computation_retain()` returns another owned reference to the **same** control,
not a snapshot or independent allowance. Pass retained references to nested
operations and concurrent workers. `computation_free()` releases one reference
without cancelling, waiting for or completing anything. Protect each pointer's
lifetime during access. There is no reset: create a fresh control for new work.

Configuration is immutable. The progress callback argument remains borrowed and
must outlive every reporting call. The control does not own mathematical graphs,
results, threads or callback arguments. Its atomic state supports native threads;
this does not make the algorithms using it thread-safe. In particular, do not
share a mutable integrator or its bound expression between concurrent calls.
This implementation is not a browser WebAssembly backend or a signal-handler API.

## Checkpoints, budgets and termination

`computation_checkpoint(control, units)` polls for stopping and atomically
reserves work **before** it starts. Zero units only polls. Work units are defined
by the consuming algorithm, not elapsed time or a universal cost estimate.
All references share the counter. `computation_work_used()` reads reserved work,
which may include operations stopped after reservation but before execution.

A reservation must fit in full. Consuming the last allowed unit is permitted;
the next positive reservation stops the calculation without consuming more.
An unlimited counter saturates at `UINT64_MAX` instead of wrapping. Algorithms
must place checkpoints inside their expensive loops: attaching a control alone
does not interrupt code that never checks it.

The first published terminal transition wins:

| Status | Meaning | Checkpoint errno |
|---|---|---|
| `COMPUTATION_ACTIVE` | Work may continue; does not imply a running worker | — |
| `COMPUTATION_COMPLETED` | Owner declared success | `EALREADY` |
| `COMPUTATION_CANCELLED` | Cancellation requested | `ECANCELED` |
| `COMPUTATION_TIMED_OUT` | A poll observed the monotonic deadline | `ETIMEDOUT` |
| `COMPUTATION_BUDGET_EXHAUSTED` | A reservation exceeded the remaining budget | `E2BIG` |
| `COMPUTATION_FAILED` | Algorithm declared failure, or deadline clock failed | `EIO` |
| `COMPUTATION_INVALID` | Missing control | `EINVAL` |

`computation_cancel()` and `computation_fail()` publish cancellation and algorithmic
failure respectively, preserving any prior terminal result. They do not poll the
clock first. `computation_status()` polls the deadline before returning the current
status. `computation_complete()` also checks the deadline, then tries to publish
success. Only the owner of the **whole** calculation should call it after
coordinating or joining its workers. Nested consumers must not declare completion.

Stopping is cooperative. Neither cancellation nor a deadline asynchronously interrupts
an individual numerical primitive, blocking call or unchecked algorithm. Keep
process-level timeouts where a hard outer limit is needed. No timer or background
thread is created. Simultaneous stopping requests have no prescribed priority;
the first successful atomic terminal transition is retained.

## Progress callbacks

`computation_report()` supplies a borrowed `computation_progress_t` containing a
stage label, completed work, optional stage total, aggregate reserved work and
current status. A total of zero means **unknown**, not zero per cent. A known
total must not be smaller than the completed count. Stage totals are separate
from the shared work budget. Status and count are observations, not one atomic
snapshot of all concurrent activity.

Observers run synchronously on reporting threads, without an internal lock. They
must handle concurrent reports if callers produce them, and should do little
work. They may inspect or cancel the control, but must not recursively report,
release the caller's reference or re-enter the reporting algorithm. A report
polls again after the callback so observer cancellation takes effect immediately.
Reporting a stopped control still invokes the observer, then returns false.
Reports are explicit: checkpoints and state changes do not automatically emit
callbacks. For a GUI, enqueue notifications for the UI thread.

### README example: report progress and complete

```c
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "computation-t.h"

static void progress(const computation_progress_t *update, void *argument)
{
    (void)argument;
    printf("%s: %" PRIu64 "/%" PRIu64 "\n", update->stage, update->completed, update->total);
}

int main(void)
{
    computation_options_t options = {.work_limit = 3, .progress = progress};
    computation_t *control = computation_new(&options);
    if (!control)
        return 1;
    for (uint64_t i = 0; i < 3; ++i) {
        if (!computation_checkpoint(control, 1) || !computation_report(control, "items", i + 1, 3)) {
            computation_free(control);
            return 1;
        }
    }
    bool completed = computation_complete(control);
    printf("Completed: %d; work: %" PRIu64 "\n", completed, computation_work_used(control));
    computation_free(control);
    return completed ? 0 : 1;
}
```

Output:

```text
items: 1/3
items: 2/3
items: 3/3
Completed: 1; work: 3
```

## Integrator adoption

Use `intg_set_computation()` to attach a retained control. One budget covers all
dimensions, symbolic fast-path attempts, quadrature fallback and successive calls
on any integrators sharing it. The integrator does not reset, complete or mark
mathematical failure on the control: its caller decides whether a local failure
is terminal for the whole task. Integration's existing subdivision cap remains
independent. See the [integration example and stopping contract](integrator.md#cooperative-calculation-control).

## Scoped native calculations

`computation_run(control, callback, argument)` retains the control for a synchronous
callback and installs it in thread-local state. `computation_current()` borrows
that control, or returns NULL outside a scope. Normal return restores the previous
scope. Nested scopes must share the same control; replacing it fails with `EBUSY`.
Scopes do not propagate automatically to new threads: enter a scope in each worker
using its retained reference. Callbacks must return normally, without `longjmp` or
thread exit. This does not make expression graphs safe for concurrent mutation.

The return value is false if the callback fails or a checkpoint observes stopping.
Discard any callback outputs in that case, releasing their owned resources. A
callback failure does not automatically mark the control failed; neither does
successful return declare completion. The top-level owner decides those transitions.

Expression evaluation, symbolic differentiation, reverse derivatives, simplification,
beautification, integration dispatch and bounded sum/product loops observe this
scope. Checkpoints reserve algorithm-specific work, including node visits and loop
iterations, not a stable count of arithmetic operations. Interrupted values and
derivatives are not published into caches. Some legacy symbolic APIs can return an
unchanged or partial expression while unwinding: the scope result is authoritative.
Parsing, rendering and individual numerical primitives are not fully interruptible.
Equation root searches and differential-equation method selection also provide
native checkpoints; their nested expression work shares this scope. See the
module adoption section below for the other cooperating algorithms.

An integrator without an explicit control inherits the scope. Its numerical and
expression work then share one budget; counts therefore differ from a standalone
attached integrator. An explicitly attached, different control is rejected with
`EBUSY` and integration status -2. Standalone symbolic integration benefits through
its expression checkpoints when scoped.

The optional `cancelled` constructor callback polls external cancellation in ordinary
code, sharing `argument` with the progress observer. True publishes cancellation.
It must be quick, non-reentrant and thread-safe when shared, and must not call
computation or mathematical APIs. For signals, the handler only sets a
`volatile sig_atomic_t` flag; the callback reads it at checkpoints. Status queries
also poll cancellation. Expression progress reports use `expression.*` stage names
at the first work unit and periodically thereafter, with unknown totals. Reports
do not imply a percentage or stream automatically to a browser.

MARS Lab creates a fresh scoped control in each calculation process, preserving
process deadlines as hard-stop safeguards; see [MARS Lab](mars-lab.md#native-calculation-control).

### README example: stop evaluation and reuse its graph

```c
#include <stdio.h>

#include "computation-t.h"
#include "expression-t.h"

static bool evaluate(void *argument)
{
    number_t value = expr_eval(argument);
    num_destroy(&value);
    return true;
}

int main(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *formula = expr_from_string("x*x*x + 2*x*x + 3*x + 4", &bindings);
    expr_t *x = expr_bindings_get(bindings, "x");
    computation_options_t options = {.work_limit = 1};
    computation_t *control = computation_new(&options);
    if (!formula || !x || !control) {
        expr_free(formula);
        expr_bindings_free(bindings);
        computation_free(control);
        return 1;
    }
    expr_set_val(x, NUM_TWO);
    bool stopped = !computation_run(control, evaluate, formula);
    number_t value = expr_eval(formula), expected = num_create_from_long(26);
    bool reusable = num_eq(value, expected);
    printf("Stopped: %d; graph reusable: %d\n", stopped, reusable);
    num_destroy(&expected);
    num_destroy(&value);
    expr_free(formula);
    expr_bindings_free(bindings);
    computation_free(control);
    return stopped && reusable ? 0 : 1;
}
```

Output:

```text
Stopped: 1; graph reusable: 1
```

## Native module adoption

`sampler_t` reserves one unit per callback evaluation and reports progress at
those boundaries. Its expression adapter also inherits expression work charges.
`plot_t` reserves work while copying points, finding bounds and drawing curves.
Both discard partial outputs on stopping and leave completion to the caller.
See [curve sampling](sampler.md) and [native plots](plot.md).

The scope is also inherited by [Equation](equation.md), [Matrix](matrix.md),
[Differential Equation](diffequation.md), [Almanac](almanac.md),
[Time Series](timeseries.md), [CSV](csv.md) and [SQLite CSV ingestion](sqlite.md).
Their guides describe checkpoint coverage, output ownership and executable
examples. Nested work consumes the same allowance rather than allocating a
fresh budget. Each module keeps its ordinary API usable without a scope.

Cancellation does not interrupt cleanup. In particular, SQLite imports finish
rolling back their savepoint even after the control is terminal. CSV export
and caller-supplied input callbacks cannot undo external I/O already performed.
A native primitive or blocking callback must return before the next checkpoint;
the scope is not a general thread interruption facility.
