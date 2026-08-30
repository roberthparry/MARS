# `integrator_t`

`integrator_t` provides adaptive `number_t`-based quadrature over finite
intervals `[a, b]`, with automatic subinterval bisection and error control:

| Function | Rule | Degree | Notes |
|---|---|---|---|
| `intg_integral` | Turán T15/T4 | 31 | 1-D, `expr_t` expression, full `number_t` precision |
| `intg_double_integral` | Turán T15/T4 | 31 | 2-D rectangular domain |
| `intg_triple_integral` | Turán T15/T4 | 31 | 3-D rectangular domain |
| `intg_integral_multi` | Turán T15/T4 | 31 | N-D rectangular domain, adaptive in the outermost variable, with symbolic fast paths for recognised `expr_t` structure |

---

## Algorithms

### G7K15 Background

Each subinterval is evaluated with a 15-point Kronrod rule (K15) containing an
embedded 7-point Gauss rule (G7). The per-subinterval error estimate is
`|K15 − G7|`. The subinterval with the largest error is bisected at each step.
Practical accuracy tops out near 21 digits, which is why the public `expr_t`
integrator uses the higher-degree Turán path for full `number_t` precision.

### Turán T15/T4 (`intg_integral`, `intg_double_integral`, `intg_triple_integral`)

Uses both `f(x)` and `f''(x)` at 8 symmetric node positions per subinterval
(Turán quadrature), achieving degree-31 polynomial exactness versus degree 29
for G7K15. The second derivative is computed automatically by differentiating
the `expr_t` expression graph, so no user-supplied derivative is needed. The
nested T4 sub-rule (4 of the 8 positions) provides the error estimate.

Because the rule exploits curvature information, smooth integrands typically
converge in far fewer subintervals than G7K15. For example, `∫₀¹ exp(x) dx`
takes 3 subintervals with Turán T15/T4 at the default `1e-27` tolerance
versus 39 with G7K15 at `1e-21`, and the Turán result carries an extra 6
digits of accuracy.

Integrands that are polynomial of degree ≤ 1, such as `∫₀⁵ 1 dx = 5` and
`∫₀⁵ x dx = 12.5`, are evaluated exactly to `number_t` precision in a single
subinterval. The G7K15 rule accumulates about `1e-25` floating-point noise
even for constants and cannot reach `1e-27` tolerance for these cases.

Both rules stop when:

```
total_error ≤ max(abs_tol, rel_tol × |result|)
```

or the maximum subinterval count is reached.

### Symbolic Fast Path (`intg_integral_multi`)

Before falling back to general adaptive Turán evaluation,
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

### Basic integration

```c
#include <stdio.h>
#include "expression.h"
#include "integrator.h"
#include "number.h"

int main(void) {
    integrator_t *ig = intg_new();
    number_t x0 = num_create_from_long(0);
    number_t lo = num_create_from_long(-3);
    number_t hi = num_create_from_long(3);
    expr_t *x = expr_new_named_var(x0, "x");
    expr_t *x2 = expr_mul(x, x);
    expr_t *negx2 = expr_neg(x2);
    expr_t *expr = expr_exp(negx2);
    number_t result = num_new();
    number_t err = num_new();

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
  error estimate   ≈ 5.107323E-53
  subintervals used: 5000
```

### Expression-backed integration

The integrator expects the integrand as an `expr_t` graph and adapts directly
over `number_t` bounds and tolerances.

```c
#include <stdio.h>
#include "expression.h"
#include "integrator.h"
#include "number.h"

int main(void) {
    /* ∫₀¹ exp(x) dx = e - 1, at default 1e-27 tolerance */
    integrator_t *ig = intg_new();
    number_t x0 = num_create_from_long(0);
    number_t lo = num_create_from_long(0);
    number_t hi = num_create_from_long(1);
    expr_t *x = expr_new_var(x0);
    expr_t *expr = expr_exp(x);
    number_t result = num_new();
    number_t err = num_new();

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
#include "expression.h"
#include "integrator.h"
#include "number.h"

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
    number_t result = num_new();
    number_t err = num_new();

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
#include "expression.h"
#include "integrator.h"
#include "number.h"

int main(void) {
    /* ∫₀¹ x^2.5 dx = 1 / 3.5 */
    integrator_t *ig = intg_new();
    number_t x0 = num_create_from_long(0);
    number_t lo = num_create_from_long(0);
    number_t hi = num_create_from_long(1);
    number_t exponent = num_create_from_string("2.5");
    expr_t *x = expr_new_named_var(x0, "x");
    expr_t *expr = expr_pow(x, &exponent);
    number_t result = num_new();
    number_t err = num_new();

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
  error estimate   ≈ 6.654124E-125
```

---

## API Reference

All declarations are in `include/integrator.h`.

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
  - Requires `#include "expression.h"`; that is already included transitively via `integrator.h`.

- `int intg_double_integral(integrator_t *ig, expr_t *expr, expr_t *x_var, number_t ax, number_t bx, expr_t *y_var, number_t ay, number_t by, number_t *result, number_t *error_est)` — 2-D Turán T15/T4 over `[ax,bx] × [ay,by]`. Adapts in `y`; evaluates the inner `x` integral with the same `number_t` engine.

- `int intg_triple_integral(integrator_t *ig, expr_t *expr, expr_t *x_var, number_t ax, number_t bx, expr_t *y_var, number_t ay, number_t by, expr_t *z_var, number_t az, number_t bz, number_t *result, number_t *error_est)` — 3-D Turán T15/T4 over `[ax,bx] × [ay,by] × [az,bz]`. Adapts in `z`.

- `int intg_integral_multi(integrator_t *ig, expr_t *expr, size_t ndim, expr_t * const *vars, const number_t *lo, const number_t *hi, number_t *result, number_t *error_est)` — N-D Turán T15/T4 over a rectangular domain.
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

**Turán degree advantage** comes from incorporating `f''` directly into the
quadrature weights. For an 8-node symmetric rule this raises exactness from
degree 15 (`f` only) to degree 31. The T4 nested sub-rule uses alternating
node positions rather than consecutive ones, which keeps all weights positive
and the rule well conditioned.

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
- The G7K15 rule evaluates the integrand at 15 points per subinterval; the Turán rule evaluates `f` and `f''` at 8 points, which is 16 evaluations in effect. For expensive point evaluations, the Turán rule's lower subinterval count usually wins despite the per-node overhead.

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
