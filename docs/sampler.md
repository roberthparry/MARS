# Curve sampling

Include `sampler-t.h` for the opaque `sampler_t` class. It produces an owned,
ordered set of numerical points for ordinary or parametric curves. Use
[`plot_t`](plot.md) to render the result, or inspect individual points directly.
Neither module starts threads or invokes an external plotting programme.

## API

| API | Behaviour |
| --- | --- |
| `sampler_options_default()` | Adaptive defaults: 32 initial intervals, 65,537 evaluations, subdivision depth 12, absolute tolerance 0.000001 and relative tolerance 0.001. |
| `sampler_new(callback, argument, lower, upper, options)` | Synchronously evaluate a callback over a finite real increasing parameter range. NULL options selects defaults. |
| `sampler_expression(expression, variable, context, lower, upper, options)` | Sample a real-valued expression using private copies of its graph and bindings. NULL context captures the source bindings. |
| `sampler_count(samples)` | Number of stored points, including gap records. |
| `sampler_evaluations(samples)` | Number of callback evaluations, including refinement probes. |
| `sampler_point(samples, index)` | Borrow an indexed `sampler_point_t` until the sampler is destroyed. |
| `sampler_free(samples)` | Release all owned points; accepts NULL and preserves `errno`. |

Every coordinate and parameter remains a `number_t`. Sampling uses exact interval
fractions and the existing numerical precision policy, not a double-precision
grid. It does not change the default precision. Choose the required precision
before constructing numerical inputs and running the sampler. Extremely small
steps can still collapse at the chosen input precision; uniform sampling then
fails with `ERANGE`, rather than publishing duplicate parameter values.

`SAMPLER_UNIFORM` evaluates the endpoints of each requested interval.
`SAMPLER_ADAPTIVE` additionally evaluates midpoints and refines intervals when
either coordinate differs from its chord midpoint by more than
`absolute_tolerance + relative_tolerance * maximum_coordinate_magnitude`.
Tolerances are finite non-negative doubles representing settings, not sampled
coordinates. At least one adaptive tolerance must be positive.

This is a drawing heuristic, **not a mathematical error bound or proof of
continuity**. Narrow features, rapidly oscillating functions and unsampled domain
holes may be missed. Increase initial intervals and narrow the range when needed.
The two coordinates share tolerances; rescale differently dimensioned parametric
coordinates before sampling when that policy is inappropriate.

Non-real, non-finite or explicitly missing values produce `valid == false`.
`break_before` forbids drawing a segment from the preceding point, even when both
endpoints are finite. Unresolved adaptive edges at the depth limit are broken;
an interval whose three probes are all invalid is not refined further. Maximum
depth is 24. Exhausting `max_evaluations` fails the entire constructor with
`E2BIG`; partial results are never returned.

## Callback ownership and stopping

A `sampler_callback_t` receives a borrowed parameter, two output slots initialised
to `NUM_NAN`, and the caller's borrowed argument. Assign newly owned `number_t`
values to the output slots, at most once each. Return `SAMPLER_VALUE` for a point,
`SAMPLER_GAP` for a domain hole, or `SAMPLER_ERROR` for a fatal failure. Both
outputs are destroyed by the sampler in all three cases. Do not destroy the
borrowed parameter, and do not assign a borrowed heap-backed value without
`num_clone()`. The callback and argument only need to survive the constructor.

Fatal callback errors preserve its `errno`, or use `EIO` if it supplied none.
Invalid arguments give `EINVAL`, invalid point indices `ERANGE`, allocation
failures `ENOMEM`, and unrepresentable allocation sizes `EOVERFLOW`.

[`computation_run()`](computation.md) provides optional cancellation, deadlines
and shared budgets. Each callback consumes one sampler work unit; numerical
algorithms called inside it may consume additional units. Progress is reported
at callback boundaries. A running callback cannot be forcibly interrupted.
No partial result is returned on stopping, and the sampler never completes the
caller's computation control.

The expression adapter replaces only the selected variable's numerical binding;
other bindings are preserved. Unset parameters produce gaps rather than invented
values. Sample values are not symbolic assumptions. Contexts, expression graphs
and number operations retain their existing threading restrictions; using a
sampler does not make shared mathematical graphs thread-safe.

## Sampling an expression

```c
#include <stdbool.h>
#include <stdio.h>

#include "sampler-t.h"

int main(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expression = expr_from_string("x^2", &bindings);
    sampler_options_t options = sampler_options_default();
    options.method = SAMPLER_UNIFORM;
    options.intervals = 2;
    sampler_t *samples = sampler_expression(expression, expr_bindings_get(bindings, "x"), NULL,
                                             NUM_ZERO, NUM_ONE, &options);
    if (samples) {
        printf("Points: %zu\n", sampler_count(samples));
        num_printf("Last point: (%n, %n)\n", sampler_point(samples, 2)->x, sampler_point(samples, 2)->y);
    }
    bool ok = samples != NULL;
    sampler_free(samples);
    expr_free(expression);
    expr_bindings_free(bindings);
    return ok ? 0 : 1;
}
```

Output:

```text
Points: 3
Last point: (1, 1)
```

Mirrored as a README example in `tests/sampler/test_sampler.c`.

## Sampling a parametric callback

The parameter need not be the plotted x coordinate. This callback generates
the curve `(t², t³)` and transfers ownership of both calculated coordinates.

```c
#include <stdbool.h>
#include <stdio.h>

#include "sampler-t.h"

static sampler_status_t parametric_curve(number_t t, number_t *x, number_t *y, void *argument)
{
    (void)argument;
    *x = num_mul(t, t);
    *y = num_mul(*x, t);
    return SAMPLER_VALUE;
}

int main(void)
{
    sampler_options_t options = sampler_options_default();
    options.method = SAMPLER_UNIFORM;
    options.intervals = 2;
    number_t lower = num_create_from_long(-1);
    sampler_t *samples = sampler_new(parametric_curve, NULL, lower, NUM_ONE, &options);
    if (samples) {
        const sampler_point_t *first = sampler_point(samples, 0);
        const sampler_point_t *last = sampler_point(samples, 2);
        num_printf("Curve endpoints: (%n, %n) to (%n, %n)\n", first->x, first->y, last->x, last->y);
    }
    bool ok = samples != NULL;
    sampler_free(samples);
    num_destroy(&lower);
    return ok ? 0 : 1;
}
```

Output:

```text
Curve endpoints: (1, -1) to (1, 1)
```

Also mirrored as a README example in `tests/sampler/test_sampler.c`.
