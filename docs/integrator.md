# `integrator_t`

`integrator_t` provides adaptive `number_t`-based quadrature over finite
intervals `[a, b]`, with automatic subinterval bisection and error control:

| Function | Domain |
|---|---|
| `intg_integral` | One-dimensional `expr_t` expression |
| `intg_double_integral` | Two-dimensional rectangle |
| `intg_triple_integral` | Three-dimensional rectangular box |
| `intg_integral_multi` | N-dimensional rectangular domain, with recognised symbolic fast paths |

All numerical entry points share the `number_t` engine described below.
`src/integrator/integrator_core.c` owns configuration and the public
multidimensional wrappers; `integrator_mp.c` owns numerical quadrature and
`integrator_special.c` owns recognised structural fast paths.

---

## Cooperative calculation control

`intg_set_computation(ig, control)` retains a [`computation_t`](computation.md)
reference, replacing the previous reference. Pass NULL to detach. Configure only
while idle; sharing a control does not make integrators or expressions safe for
concurrent mutation. The control and integrator have independent lifetimes.

All handle-based numerical entry points share the control across dimensions and
successive calls. Each symbolic fast-path attempt and each fresh leaf integrand
evaluation reserves one work unit. Reused tanh–sinh nodes do not consume this
budget again, although they still count towards the integrator's existing logical
subdivision limit. Thus the two limits measure different things. No component
of a nested integration resets or completes the shared control.

Checks bracket symbolic attempts and numerical evaluations, and occur during
refinement. An attached control alone brackets expression calls. To check inside
those calls too, use `computation_run()`: unattached integrators inherit its control,
and expression visits and sum/product iterations also consume the shared budget.
Standalone `intg_integrate_iterated_symbolic()` and
`intg_integrate_iterated_symbolic_best_effort()` benefit through scoped expression
checkpoints despite having no integrator handle. Individual numerical primitives
remain non-interruptible. Retain a process timeout for a hard outer deadline.

Return status **-2** means the attached control is no longer active: inspect
`computation_status()` for cancellation, timeout, exhausted work or another
terminal state. On -2, the numerical result and error are NaN, interval count is
zero and the exact-result cache is cleared. Do not treat these as a partial
estimate. Existing statuses remain 0 for convergence, 1 for the subdivision cap
before convergence and -1 for invalid input or numerical/internal failure.
Ordinary failures do not automatically mark a shared calculation as failed.
An explicitly attached control different from the current scope also returns -2
with unavailable outputs and `errno = EBUSY`, without stopping either control.

Progress stages are `integration.symbolic`, `integration.tanh_sinh`,
`integration.gauss_kronrod` and `integration.return`. Totals are unknown;
completed counts describe the current stage, not an overall percentage. The
aggregate `work_used` counter spans nested calls. Reports run synchronously on
the integration thread. An observer may cancel but must not re-enter, reconfigure
or destroy the integrator, or change its input expressions or numeric precision.
An `integration.return` report is not a declaration of convergence or completion.

### README example: a shared work limit

The one-unit budget permits the symbolic attempt but no numerical integrand
evaluations. The result is explicitly unavailable when quadrature needs more work.

```c
#include <inttypes.h>
#include <stdio.h>

#include "computation-t.h"
#include "integrator-t.h"

int main(void)
{
    computation_options_t options = {.work_limit = 1};
    computation_t *control = computation_new(&options);
    integrator_t *ig = intg_new();
    expr_bindings_t *bindings = NULL;
    expr_t *formula = expr_from_string("sin(x*x)", &bindings);
    expr_t *x = expr_bindings_get(bindings, "x");
    if (!control || !ig || !formula || !x || !intg_set_computation(ig, control)) {
        expr_free(formula);
        expr_bindings_free(bindings);
        intg_free(ig);
        computation_free(control);
        return 1;
    }
    number_t value = NUM_NAN;
    int result = intg_integral(ig, formula, x, NUM_ZERO, NUM_ONE, &value, NULL);
    bool stopped = result == -2 && computation_status(control) == COMPUTATION_BUDGET_EXHAUSTED;
    printf("Budget exhausted: %d; work: %" PRIu64 "; value unavailable: %d\n",
           stopped, computation_work_used(control), num_is_nan(value));
    num_destroy(&value);
    expr_free(formula);
    expr_bindings_free(bindings);
    intg_free(ig);
    computation_free(control);
    return stopped ? 0 : 1;
}
```

Output:

```text
Budget exhausted: 1; work: 1; value unavailable: 1
```

## Algorithms

### Multiprecision endpoint handling

The multiprecision numerical path first uses tanh–sinh quadrature. Its nodes
remain strictly inside the integration interval: a transformed node that rounds
onto an endpoint is not evaluated there. The last interior tail contribution
is retained in the error estimate, and refinement preserves the transformed
interval width as the step size decreases. Contributions from opposite tails
are measured separately in magnitude so that cancellation cannot hide an error.

Successive halved-step grids reuse their shared interior nodes through direct
indexing. Cached contributions and absolute tail magnitudes are rescaled for
the new step size; newly inserted nodes are evaluated at the full working
precision. The work counter retains its logical grid-point meaning, including
reused nodes, so existing caps and convergence/error rules are unchanged.
Temporary caches belong to a single integration and are bounded by its work cap.

This permits convergent improper integrals, including algebraic factors damped
by reciprocal exponential decay, without directly evaluating an undefined
endpoint product. It is a numerical convergence check, not a general symbolic
limit prover; a singular endpoint is never unconditionally assigned zero.

### Adaptive Gauss–Kronrod fallback

Each subinterval is evaluated with a 15-point Kronrod rule (K15) containing an
embedded 7-point Gauss rule (G7). The per-subinterval error estimate is
`|K15 − G7|`. The subinterval with the largest error is bisected at each step.
This path is available when the initial tanh–sinh attempt does not supply an
acceptable result. Neither path evaluates second derivatives or implements a
separate Turán rule. Requested numeric precision alone does not guarantee that
the quadrature estimate meets the requested tolerance.

The requested convergence criterion is:

```
total_error ≤ max(abs_tol, rel_tol × |result|)
```

Work limits can stop refinement earlier; inspect the returned status and error
estimate rather than assuming that a returned value has converged.

### Symbolic Fast Path (`intg_integral_multi`)

Before falling back to general adaptive numerical evaluation,
`intg_integral_multi()` tries a symbolic plan for several important `expr_t`
expression families:

- constants and scaled sums/differences of recognised symbolic forms
- affine unary families such as `exp(a)`, `sin(a)`, `cos(a)`, `sinh(a)`, and `cosh(a)`
- degree-4 affine polynomials `P(a)`
- degree-4 affine-polynomial times unary-affine families `P(a) * special(a)`
- separable products, including regrouped products that become separable after flattening the full multiplication tree

For matching inputs, these cases evaluate exactly over rectangular boxes in a
single interval, which can reduce work by orders of magnitude compared with the
generic adaptive path.

---

## Examples

Integral and error outputs receive newly owned numbers; the integrator does
not release a previous value in either output slot. Initialise fresh outputs
with the allocation-free `NUM_ZERO`, and destroy previous results before
reusing their slots. Destroy returned numbers when they are no longer needed.

### Basic integration

```c
#include <stdio.h>
#include "expression-t.h"
#include "integrator-t.h"
#include "number-t.h"

int main(void) {
    integrator_t *ig = intg_new();
    number_t x0 = num_create_from_long(0);
    number_t lo = num_create_from_long(-3);
    number_t hi = num_create_from_long(3);
    expr_t *x = expr_new_named_var(x0, "x");
    expr_t *x2 = expr_mul(x, x);
    expr_t *negx2 = expr_neg(x2);
    expr_t *expr = expr_exp(negx2);
    number_t result = NUM_ZERO;
    number_t err = NUM_ZERO;

    intg_integral(ig, expr, x, lo, hi, &result, &err);

    num_printf("∫₋₃³ exp(-x²) dx ≈ %.64N\n", result);
    num_printf("  error estimate   ≈ %.6N\n", err);
    printf("  subintervals used: %zu\n", intg_get_interval_count_used(ig));

    num_destroy(&err);
    num_destroy(&result);
    expr_free(expr);
    expr_free(negx2);
    expr_free(x2);
    expr_free(x);
    num_destroy(&hi);
    num_destroy(&lo);
    num_destroy(&x0);
    intg_free(ig);
    return 0;
}
```

```text
∫₋₃³ exp(-x²) dx ≈ 1.7724146965190424677889691558236911591392838694905668116893266525E+00
  error estimate   ≈ 3.829382E-290
  subintervals used: 3205
```

### Expression-backed integration

The integrator expects the integrand as an `expr_t` graph and adapts directly
over `number_t` bounds and tolerances.

```c
#include <stdio.h>
#include "expression-t.h"
#include "integrator-t.h"
#include "number-t.h"

int main(void) {
    /* ∫₀¹ exp(x) dx = e - 1, at default 1e-27 tolerance */
    integrator_t *ig = intg_new();
    number_t x0 = num_create_from_long(0);
    number_t lo = num_create_from_long(0);
    number_t hi = num_create_from_long(1);
    expr_t *x = expr_new_var(x0);
    expr_t *expr = expr_exp(x);
    number_t result = NUM_ZERO;
    number_t err = NUM_ZERO;

    intg_integral(ig, expr, x, lo, hi, &result, &err);

    num_printf("∫₀¹ exp(x) dx ≈ %.64N\n", result);
    num_printf("  error estimate   ≈ %.6N\n", err);
    printf("  subintervals used: %zu\n", intg_get_interval_count_used(ig));

    num_destroy(&err);
    num_destroy(&result);
    expr_free(expr);
    expr_free(x);
    num_destroy(&hi);
    num_destroy(&lo);
    num_destroy(&x0);
    intg_free(ig);
    return 0;
}
```

```text
∫₀¹ exp(x) dx ≈ 1.7182818284590452353602874713526624977572470936999595749669676277E+00
  error estimate   ≈ 0
  subintervals used: 1
```

### Symbolic fast path example

Recognised affine-family integrands can collapse to one interval even in
multiple dimensions:

```c
#include <stdio.h>
#include "expression-t.h"
#include "integrator-t.h"
#include "number-t.h"

int main(void) {
    integrator_t *ig = intg_new();
    number_t x0 = num_create_from_long(0);
    number_t y0 = num_create_from_long(0);
    number_t two = num_create_from_long(2);
    number_t three = num_create_from_long(3);
    expr_t *x = expr_new_var(x0);
    expr_t *y = expr_new_var(y0);
    expr_t *two_y = expr_mul_num(y, &two);
    expr_t *sum = expr_add(x, two_y);
    expr_t *affine = expr_add_num(sum, &three);
    expr_t *exp_affine = expr_exp(affine);
    expr_t *expr = expr_mul(affine, exp_affine);
    expr_t *vars[2] = { x, y };
    number_t lo[2] = { num_create_from_long(0), num_create_from_long(0) };
    number_t hi[2] = { num_create_from_long(1), num_create_from_long(1) };
    number_t result = NUM_ZERO;
    number_t err = NUM_ZERO;

    intg_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    num_printf("result = %.64N\n", result);
    printf("intervals = %zu\n", intg_get_interval_count_used(ig));

    num_destroy(&err);
    num_destroy(&result);
    num_destroy(&hi[1]);
    num_destroy(&hi[0]);
    num_destroy(&lo[1]);
    num_destroy(&lo[0]);
    num_destroy(&three);
    num_destroy(&two);
    num_destroy(&y0);
    num_destroy(&x0);
    expr_free(expr);
    expr_free(exp_affine);
    expr_free(affine);
    expr_free(sum);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    intg_free(ig);
    return 0;
}
```

Typical result:

```text
result = 5.3968246676005493487745499465037812032227151020410292800416798214E+02
intervals = 1
```

### Parameterised expression

```c
#include <stdio.h>
#include "expression-t.h"
#include "integrator-t.h"
#include "number-t.h"

int main(void) {
    /* ∫₀¹ x^2.5 dx = 1 / 3.5 */
    integrator_t *ig = intg_new();
    number_t x0 = num_create_from_long(0);
    number_t lo = num_create_from_long(0);
    number_t hi = num_create_from_long(1);
    number_t exponent = num_create_from_string("2.5");
    expr_t *x = expr_new_named_var(x0, "x");
    expr_t *expr = expr_pow(x, &exponent);
    number_t result = NUM_ZERO;
    number_t err = NUM_ZERO;

    intg_integral(ig, expr, x, lo, hi, &result, &err);

    num_printf("∫₀¹ x^2.5 dx ≈ %.64N\n", result);
    num_printf("  error estimate   ≈ %.6N\n", err);

    num_destroy(&err);
    num_destroy(&result);
    expr_free(expr);
    expr_free(x);
    num_destroy(&exponent);
    num_destroy(&hi);
    num_destroy(&lo);
    num_destroy(&x0);
    intg_free(ig);
    return 0;
}
```

```text
∫₀¹ x^2.5 dx ≈ 2.8571428571428571428571428571428571428571428571428571428571428571E-01
  error estimate   ≈ 2.254686E-356
```

---

## API Reference

All declarations are in `include/integrator-t.h`.

### Types

- `integrator_t` — opaque adaptive integrator.

### Lifecycle

- `integrator_t *intg_new(void)` — create an integrator with default tolerances (`1e-27` absolute and relative, 5000 max subintervals). Returns `NULL` on allocation failure.
- `void intg_free(integrator_t *ig)` — free the integrator. Safe to call with `NULL`.

### Configuration

- `void intg_set_tolerance(integrator_t *ig, number_t abs_tol, number_t rel_tol)` — override convergence tolerances. Convergence is declared when `total_error ≤ max(abs_tol, rel_tol × |result|)`.
- `void intg_set_interval_count_max(integrator_t *ig, size_t max_intervals)` — override the maximum number of subintervals before the algorithm halts.

### Evaluation

- `int intg_integral(integrator_t *ig, expr_t *expr, expr_t *x_var, number_t a, number_t b, number_t *result, number_t *error_est)` — integrate an `expr_t` expression over `[a, b]`.
  - `expr` is the integrand expression; `x_var` is the variable node within it created with `expr_new_var()` or `expr_new_named_var()`.
  - Returns `0` on convergence, `1` if `max_intervals` was reached, `-1` on `NULL` argument or allocation failure.
  - `error_est` may be `NULL`.
  - Reversed limits (`a > b`) are handled correctly.
  - Requires `#include "expression-t.h"`; that is already included transitively via `integrator-t.h`.

- `int intg_double_integral(integrator_t *ig, expr_t *expr, expr_t *x_var, number_t ax, number_t bx, expr_t *y_var, number_t ay, number_t by, number_t *result, number_t *error_est)` — 2-D adaptive quadrature over `[ax,bx] × [ay,by]`. Adapts in `y`; evaluates the inner `x` integral with the same `number_t` engine.

- `int intg_triple_integral(integrator_t *ig, expr_t *expr, expr_t *x_var, number_t ax, number_t bx, expr_t *y_var, number_t ay, number_t by, expr_t *z_var, number_t az, number_t bz, number_t *result, number_t *error_est)` — 3-D adaptive quadrature over `[ax,bx] × [ay,by] × [az,bz]`. Adapts in `z`.

- `int intg_integral_multi(integrator_t *ig, expr_t *expr, size_t ndim, expr_t * const *vars, const number_t *lo, const number_t *hi, number_t *result, number_t *error_est)` — N-D adaptive quadrature over a rectangular domain.
  - `vars[0]` is the innermost variable, `vars[ndim-1]` the outermost, adapted by bisection.
  - `lo[i]` / `hi[i]` are the bounds for `vars[i]`.
  - All `2^N` mixed second-derivative expressions are built automatically.
  - Returns `0` on convergence, `1` if `max_intervals` was reached, `-1` on null argument, `ndim == 0`, or allocation failure.
  - `error_est` may be `NULL`.

### Diagnostics

- `size_t intg_get_interval_count_used(const integrator_t *ig)` — number of subintervals used in the most recent integration call.

---

### `intg_get_exact_result()`

Returns the public result described by get exact result.

```c
const expr_t *intg_get_exact_result(const integrator_t *ig);
```

### `intg_integrand_has_unbound_parameters()`

Reports whether the condition described by integrand has unbound parameters holds.

```c
bool intg_integrand_has_unbound_parameters(const expr_t *integrand, size_t ndim, expr_t *const *vars);
```

### `intg_integrate_iterated_symbolic()`

Returns the public result described by integrate iterated symbolic.

```c
expr_t *intg_integrate_iterated_symbolic(const expr_t *integrand, size_t ndim, expr_t *const *vars, const intg_bound_kind_t *kinds, expr_t *const *lo, expr_t *const *hi, size_t max_steps, size_t *completed_steps_out, expr_t **first_antiderivative_out);
```

### `intg_integrate_iterated_symbolic_best_effort()`

Returns the public result described by integrate iterated symbolic best effort.

```c
expr_t *intg_integrate_iterated_symbolic_best_effort(const expr_t *integrand, size_t ndim, expr_t *const *vars, const intg_bound_kind_t *kinds, expr_t *const *lo, expr_t *const *hi, size_t *completed_steps_out, size_t *remaining_ndim_out, expr_t **remaining_vars_out, number_t *remaining_lo_num_out, number_t *remaining_hi_num_out, const number_t *lo_num, const number_t *hi_num);
```

## Design Notes

**Nodes and weights** are stored as static high-precision quadrature tables, so
the adaptive engine can reuse them without runtime reinitialisation.

**Subinterval storage** is a dynamically grown heap-allocated array. Linear
search for the maximum-error interval is used; this is `O(n)` per step, but
`n` rarely exceeds tens of intervals for well-behaved integrands.

**Cache coherence** in `intg_integral`: `expr_eval` detects variable changes
automatically via epoch tracking. Each call to `expr_set_val()` advances the
variable's epoch, and computed nodes recompute when they see a newer epoch from
their inputs.

**Threading:** the current `expr_t` and symbolic-integrator path are not yet
internally synchronised. Prefer sequential test and benchmark runs, and do not
share mutable integrator or `expr_t` state across threads without external
locking.

## Tradeoffs

- Only finite intervals `[a, b]` are supported directly. For improper integrals, apply a substitution before passing the transformed integrand.
- `intg_integral` and the multi-dimensional variants require the integrand to be expressible as an `expr_t` graph.
- Functions with endpoint singularities or sharp peaks may require many subdivisions. Increase the max interval count via `intg_set_interval_count_max()` or apply a smoothing substitution.
- Tanh–sinh refinement and the 15-point Gauss–Kronrod fallback have different sampling costs. Difficult integrands can exhaust the configured work limit without meeting tolerance.

## Benchmark Coverage

The dedicated integrator benchmark focuses on symbolic fast paths in
`intg_integral_multi()` and compares them with nearby fallback cases that miss
the exact matcher.

Benchmark source:

- [`bench/integrator/bench_integrator.c`](../bench/integrator/bench_integrator.c)

Run it from the repository root with:

```sh
make bench_integrator
./build/release/bench/integrator/bench_integrator
```

Current sample cases covered there include:

- matched shortcut families:
  - `affine_exp`
  - `affine_square`
  - `affine_quartic`
  - `affine_times_exp`
  - `affine_times_sin`
  - `affine_times_sinh`
- nearby fallback cases:
  - `near_miss_square`
  - `near_miss_quartic`
  - `near_miss_exp`
  - `near_miss_sin`
  - `near_miss_sinh`

Sample timings from [`docs/benchmarks.md`](./benchmarks.md) are:

| Case | Intervals | Avg ms |
|---|---:|---:|
| `affine_square` | `1` | `0.001` |
| `affine_times_exp` | `1` | `0.019` |
| `near_miss_square` | `1` | `0.179` |
| `near_miss_quartic` | `1` | `5.977` |
| `near_miss_exp` | `2` | `15.189` |

For the full integrator benchmark discussion and fuller sample output, see
[`docs/benchmarks.md`](./benchmarks.md).
