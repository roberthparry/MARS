# `integrator_t`

`integrator_t` provides two adaptive quadrature rules at full `qfloat_t` (~34 decimal digits) precision, both integrating a function over a finite interval `[a, b]` with automatic subinterval bisection:

| Function | Rule | Degree | Notes |
|---|---|---|---|
| `integrator_eval` | G7K15 (Gauss-Kronrod) | 29 | Callback-based; works for any function |
| `integrator_eval_dv` | Turán T15/T4 | 31 | Uses `dval_t` expression + automatic differentiation |

---

## Algorithms

### G7K15 (`integrator_eval`)

Each subinterval is evaluated with a 15-point Kronrod rule (K15) containing an embedded 7-point Gauss rule (G7). The per-subinterval error estimate is `|K15 − G7|`. The subinterval with the largest error is bisected at each step.

### Turán T15/T4 (`integrator_eval_dv`)

Uses both f(x) and f''(x) at 8 symmetric node positions per subinterval (Turán quadrature), achieving degree-31 polynomial exactness versus degree 29 for G7K15. The second derivative is computed automatically by differentiating the `dval_t` expression graph — no user-supplied derivative is needed. The nested T4 sub-rule (4 of the 8 positions) provides the error estimate.

Because the rule exploits curvature information, smooth integrands typically converge in far fewer subintervals than G7K15. For example, `∫₀¹ exp(x) dx` takes 3 subintervals with Turán T15/T4 at the default 1e-27 tolerance versus 39 with G7K15 at 1e-21 — and the Turán result carries an extra 6 digits of accuracy.

Both rules stop when:

```
total_error ≤ max(abs_tol, rel_tol × |result|)
```

or the maximum subinterval count is reached.

---

## Examples

### Basic integration (G7K15)

```c
#include <stdio.h>
#include "qfloat.h"
#include "integrator.h"

static qfloat_t gaussian(qfloat_t x, void *ctx) {
    (void)ctx;
    return qf_exp(qf_neg(qf_sqr(x)));
}

int main(void) {
    integrator_t *ig = integrator_create();
    /* G7K15 tops out at ~21 digits; tighten tolerance to match */
    integrator_set_tol(ig, qf_from_string("1e-21"), qf_from_string("1e-21"));

    qfloat_t result, err;
    integrator_eval(ig, gaussian, NULL,
                    qf_from_double(-3.0), qf_from_double(3.0),
                    &result, &err);

    qf_printf("∫₋₃³ exp(-x²) dx ≈ %q\n", result);
    qf_printf("  error estimate   ≈ %q\n", err);
    printf("  subintervals used: %zu\n", integrator_last_intervals(ig));

    integrator_destroy(ig);
    return 0;
}
```

```text
∫₋₃³ exp(-x²) dx ≈ 1.772414696519042467789877193922336
  error estimate   ≈ 1.765624419480535291888099331079003e-21
  subintervals used: 259
```

### Turán T15/T4 with automatic differentiation

When the integrand can be expressed as a `dval_t` graph, `integrator_eval_dv` uses the second derivative automatically and typically needs far fewer subintervals.

```c
#include <stdio.h>
#include "qfloat.h"
#include "integrator.h"
#include "dval.h"

int main(void) {
    /* ∫₀¹ exp(x) dx = e - 1, at default 1e-27 tolerance */
    integrator_t *ig = integrator_create();

    dval_t *x    = dv_new_var(qf_from_double(0.0));
    dval_t *expr = dv_exp(x);

    qfloat_t result, err;
    integrator_eval_dv(ig, expr, x,
                       qf_from_double(0.0), qf_from_double(1.0),
                       &result, &err);

    qf_printf("∫₀¹ exp(x) dx ≈ %q\n", result);
    qf_printf("  error estimate   ≈ %q\n", err);
    printf("  subintervals used: %zu\n", integrator_last_intervals(ig));

    dv_free(expr);
    dv_free(x);
    integrator_destroy(ig);
    return 0;
}
```

```text
∫₀¹ exp(x) dx ≈ 1.718281828459045235360287471352664
  error estimate   ≈ 7.623185090994446757792522996692577e-28
  subintervals used: 3
```

### Custom context

```c
typedef struct { qfloat_t exponent; } power_ctx;

static qfloat_t power_fn(qfloat_t x, void *ctx) {
    power_ctx *pc = ctx;
    return qf_pow(x, pc->exponent);
}

int main(void) {
    /* ∫₀¹ x^2.5 dx = 1 / 3.5 */
    integrator_t *ig = integrator_create();
    power_ctx ctx = { qf_from_string("2.5") };

    qfloat_t result, err;
    integrator_eval(ig, power_fn, &ctx,
                    qf_from_double(0.0), qf_from_double(1.0),
                    &result, &err);

    qf_printf("∫₀¹ x^2.5 dx ≈ %q\n", result);
    qf_printf("  error estimate   ≈ %q\n", err);
    integrator_destroy(ig);
    return 0;
}
```

```text
∫₀¹ x^2.5 dx ≈ 2.857142857142857142857278677175604e-1
  error estimate   ≈ 1.346867791475306539808536874367391e-23
```

---

## API Reference

All declarations are in `include/integrator.h`.

### Types

- `integrand_fn` — `typedef qfloat_t (*integrand_fn)(qfloat_t x, void *ctx)` — integrand callback.
- `integrator_t` — opaque adaptive integrator.

### Lifecycle

- `integrator_t *integrator_create(void)` — create an integrator with default tolerances (1e-27 absolute and relative, 500 max subintervals). Returns NULL on allocation failure.
- `void integrator_destroy(integrator_t *ig)` — free the integrator. Safe to call with NULL.

### Configuration

- `void integrator_set_tol(integrator_t *ig, qfloat_t abs_tol, qfloat_t rel_tol)` — override convergence tolerances. Convergence when `total_error ≤ max(abs_tol, rel_tol × |result|)`.
- `void integrator_set_max_intervals(integrator_t *ig, size_t max_intervals)` — override the maximum number of subintervals before the algorithm halts.

### Evaluation

- `int integrator_eval(integrator_t *ig, integrand_fn f, void *ctx, qfloat_t a, qfloat_t b, qfloat_t *result, qfloat_t *error_est)` — integrate `f` over `[a, b]` using G7K15.
  - Returns `0` on convergence, `1` if `max_intervals` was reached, `-1` on allocation failure or NULL argument.
  - `error_est` may be NULL.
  - Reversed limits (`a > b`) are handled correctly.

- `int integrator_eval_dv(integrator_t *ig, dval_t *expr, dval_t *x_var, qfloat_t a, qfloat_t b, qfloat_t *result, qfloat_t *error_est)` — integrate a `dval_t` expression over `[a, b]` using Turán T15/T4.
  - `expr` is the integrand expression; `x_var` is the variable node within it created with `dv_new_var()`.
  - The second derivative is computed automatically; the caller's graph is not permanently modified.
  - Returns `0` on convergence, `1` if `max_intervals` was reached, `-1` on NULL argument or allocation failure.
  - `error_est` may be NULL.
  - Reversed limits (`a > b`) are handled correctly.
  - Requires `#include "dval.h"` (already included transitively via `integrator.h`).

### Diagnostics

- `size_t integrator_last_intervals(const integrator_t *ig)` — number of subintervals used in the most recent call to `integrator_eval` or `integrator_eval_dv`.

---

## Design Notes

**Nodes and weights** are stored as static `qfloat_t` arrays with double-double values pre-computed from high-precision decimal strings, so all 106 bits of precision are available in the quadrature rule itself with no runtime initialisation cost.

**Subinterval storage** is a dynamically-grown heap-allocated array. Linear search for the maximum-error interval is used; this is O(n) per step but n rarely exceeds tens of intervals for well-behaved integrands.

**Turán degree advantage** comes from incorporating f'' directly into the quadrature weights. For an 8-node symmetric rule this raises exactness from degree 15 (f only) to degree 31. The T4 nested sub-rule uses alternating node positions (not consecutive), which keeps all weights positive and the rule well-conditioned.

**Cache invalidation** in `integrator_eval_dv`: between evaluation points the function calls `dv_invalidate` on the expression and its derivative graph so that `dv_eval` recomputes rather than returning a stale cached value. This is handled internally; callers do not need to call `dv_invalidate` themselves.

## Tradeoffs

- Only finite intervals `[a, b]` are supported directly. For improper integrals, apply a substitution before passing the transformed integrand.
- `integrator_eval_dv` requires that the integrand be expressible as a `dval_t` graph. Functions with branches, loops, or external data are better served by `integrator_eval`.
- Functions with endpoint singularities or sharp peaks may require many subdivisions. Increase `max_intervals` or apply a smoothing substitution.
- The G7K15 rule evaluates the integrand at 15 points per subinterval; the Turán rule evaluates f and f'' at 8 points (16 evaluations equivalent). For expensive callbacks, the Turán rule's lower subinterval count usually wins despite the per-node overhead.
