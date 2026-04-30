# `integrator_t`

`integrator_t` provides adaptive quadrature rules at full `qfloat_t` (~34 decimal digits) precision, integrating a function over a finite interval `[a, b]` with automatic subinterval bisection:

| Function | Rule | Degree | Notes |
|---|---|---|---|
| `ig_integral` | G7K15 (Gauss-Kronrod) | 29 | Callback-based; works for any function; ~21-digit practical accuracy |
| `ig_single_integral` | Turán T15/T4 | 31 | 1-D, `dval_t` expression, full qfloat_t precision |
| `ig_double_integral` | Turán T15/T4 | 31 | 2-D rectangular domain |
| `ig_triple_integral` | Turán T15/T4 | 31 | 3-D rectangular domain |
| `ig_integral_multi` | Turán T15/T4 | 31 | N-D rectangular domain, adaptive in outermost variable, with symbolic fast paths for recognised `dval_t` structure |

---

## Algorithms

### G7K15 (`ig_integral`)

Each subinterval is evaluated with a 15-point Kronrod rule (K15) containing an embedded 7-point Gauss rule (G7). The per-subinterval error estimate is `|K15 − G7|`. The subinterval with the largest error is bisected at each step.  Practical accuracy tops out near 21 digits; use `ig_single_integral` when full qfloat_t precision is required.

### Turán T15/T4 (`ig_single_integral`, `ig_double_integral`, `ig_triple_integral`)

Uses both f(x) and f''(x) at 8 symmetric node positions per subinterval (Turán quadrature), achieving degree-31 polynomial exactness versus degree 29 for G7K15. The second derivative is computed automatically by differentiating the `dval_t` expression graph — no user-supplied derivative is needed. The nested T4 sub-rule (4 of the 8 positions) provides the error estimate.

Because the rule exploits curvature information, smooth integrands typically converge in far fewer subintervals than G7K15. For example, `∫₀¹ exp(x) dx` takes 3 subintervals with Turán T15/T4 at the default 1e-27 tolerance versus 39 with G7K15 at 1e-21 — and the Turán result carries an extra 6 digits of accuracy.

Integrands that are polynomial of degree ≤ 1 (e.g. ∫₀^5 1 dx = 5, ∫₀^5 x dx = 12.5) are evaluated exactly to qfloat_t precision in a single subinterval.  (The G7K15 rule accumulates ~1e-25 floating-point noise even for constants and cannot reach tol27 for these cases.)

Both rules stop when:

```
total_error ≤ max(abs_tol, rel_tol × |result|)
```

or the maximum subinterval count is reached.

### Symbolic Fast Path (`ig_integral_multi`)

Before falling back to general adaptive Turán evaluation, `ig_integral_multi`
tries a symbolic plan for several important `dval_t` expression families:

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
    integrator_t *ig = ig_new();
    /* G7K15 tops out at ~21 digits; tighten tolerance to match */
    ig_set_tolerance(ig, qf_from_string("1e-21"), qf_from_string("1e-21"));

    qfloat_t result, err;
    ig_integral(ig, gaussian, NULL,
                qf_from_double(-3.0), qf_from_double(3.0),
                &result, &err);

    qf_printf("∫₋₃³ exp(-x²) dx ≈ %q\n", result);
    qf_printf("  error estimate   ≈ %q\n", err);
    printf("  subintervals used: %zu\n", ig_get_interval_count_used(ig));

    ig_free(ig);
    return 0;
}
```

```text
∫₋₃³ exp(-x²) dx ≈ 1.772414696519042467789877193922336
  error estimate   ≈ 1.765624419480535291888099331079003e-21
  subintervals used: 259
```

### Turán T15/T4 with automatic differentiation

When the integrand can be expressed as a `dval_t` graph, `ig_single_integral` uses the second derivative automatically and typically needs far fewer subintervals.

```c
#include <stdio.h>
#include "qfloat.h"
#include "integrator.h"
#include "dval.h"

int main(void) {
    /* ∫₀¹ exp(x) dx = e - 1, at default 1e-27 tolerance */
    integrator_t *ig = ig_new();

    dval_t *x    = dv_new_var(qf_from_double(0.0));
    dval_t *expr = dv_exp(x);

    qfloat_t result, err;
    ig_single_integral(ig, expr, x,
                       qf_from_double(0.0), qf_from_double(1.0),
                       &result, &err);

    qf_printf("∫₀¹ exp(x) dx ≈ %q\n", result);
    qf_printf("  error estimate   ≈ %q\n", err);
    printf("  subintervals used: %zu\n", ig_get_interval_count_used(ig));

    dv_free(expr);
    dv_free(x);
    ig_free(ig);
    return 0;
}
```

```text
∫₀¹ exp(x) dx ≈ 1.718281828459045235360287471352664
  error estimate   ≈ 7.623185090994446757792522996692577e-28
  subintervals used: 3
```

### Symbolic fast path example

Recognised affine-family integrands can collapse to one interval even in
multiple dimensions:

```c
#include <stdio.h>
#include "qfloat.h"
#include "integrator.h"
#include "dval.h"

int main(void) {
    integrator_t *ig = ig_new();
    dval_t *x = dv_new_var(qf_from_double(0.0));
    dval_t *y = dv_new_var(qf_from_double(0.0));
    dval_t *two_y = dv_mul_d(y, 2.0);
    dval_t *sum = dv_add(x, two_y);
    dval_t *affine = dv_add_d(sum, 3.0);
    dval_t *exp_affine = dv_exp(affine);
    dval_t *expr = dv_mul(affine, exp_affine);
    dval_t *vars[2] = { x, y };
    qfloat_t lo[2] = { qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[2] = { qf_from_double(1.0), qf_from_double(1.0) };
    qfloat_t result, err;

    ig_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    qf_printf("result = %q\n", result);
    printf("intervals = %zu\n", ig_get_interval_count_used(ig));

    dv_free(expr);
    dv_free(exp_affine);
    dv_free(affine);
    dv_free(sum);
    dv_free(two_y);
    dv_free(y);
    dv_free(x);
    ig_free(ig);
    return 0;
}
```

Typical result:

```text
result = 539.6824667600549348774549946503721
intervals = 1
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
    integrator_t *ig = ig_new();
    power_ctx ctx = { qf_from_string("2.5") };

    qfloat_t result, err;
    ig_integral(ig, power_fn, &ctx,
                qf_from_double(0.0), qf_from_double(1.0),
                &result, &err);

    qf_printf("∫₀¹ x^2.5 dx ≈ %q\n", result);
    qf_printf("  error estimate   ≈ %q\n", err);
    ig_free(ig);
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

- `integrator_t *ig_new(void)` — create an integrator with default tolerances (1e-27 absolute and relative, 500 max subintervals). Returns NULL on allocation failure.
- `void ig_free(integrator_t *ig)` — free the integrator. Safe to call with NULL.

### Configuration

- `void ig_set_tolerance(integrator_t *ig, qfloat_t abs_tol, qfloat_t rel_tol)` — override convergence tolerances. Convergence when `total_error ≤ max(abs_tol, rel_tol × |result|)`.
- `void ig_set_interval_count_max(integrator_t *ig, size_t max_intervals)` — override the maximum number of subintervals before the algorithm halts.

### Evaluation

- `int ig_integral(integrator_t *ig, integrand_fn f, void *ctx, qfloat_t a, qfloat_t b, qfloat_t *result, qfloat_t *error_est)` — integrate `f` over `[a, b]` using G7K15 (lower precision; ~21-digit cap).
  - Returns `0` on convergence, `1` if `max_intervals` was reached, `-1` on allocation failure or NULL argument.
  - `error_est` may be NULL.
  - Reversed limits (`a > b`) are handled correctly.

- `int ig_single_integral(integrator_t *ig, dval_t *expr, dval_t *x_var, qfloat_t a, qfloat_t b, qfloat_t *result, qfloat_t *error_est)` — integrate a `dval_t` expression over `[a, b]` using Turán T15/T4 (full qfloat_t precision).
  - `expr` is the integrand expression; `x_var` is the variable node within it created with `dv_new_var()`.
  - The second derivative is computed automatically; the caller's graph is not permanently modified.
  - Returns `0` on convergence, `1` if `max_intervals` was reached, `-1` on NULL argument or allocation failure.
  - `error_est` may be NULL.
  - Reversed limits (`a > b`) are handled correctly.
  - Requires `#include "dval.h"` (already included transitively via `integrator.h`).

- `int ig_double_integral(integrator_t *ig, dval_t *expr, dval_t *x_var, qfloat_t ax, qfloat_t bx, dval_t *y_var, qfloat_t ay, qfloat_t by, qfloat_t *result, qfloat_t *error_est)` — 2-D Turán T15/T4 over `[ax,bx] × [ay,by]`.  Adapts in y; evaluates the inner x integral at fixed precision.

- `int ig_triple_integral(integrator_t *ig, dval_t *expr, dval_t *x_var, qfloat_t ax, qfloat_t bx, dval_t *y_var, qfloat_t ay, qfloat_t by, dval_t *z_var, qfloat_t az, qfloat_t bz, qfloat_t *result, qfloat_t *error_est)` — 3-D Turán T15/T4 over `[ax,bx] × [ay,by] × [az,bz]`.  Adapts in z.

- `int ig_integral_multi(integrator_t *ig, dval_t *expr, size_t ndim, dval_t * const *vars, const qfloat_t *lo, const qfloat_t *hi, qfloat_t *result, qfloat_t *error_est)` — N-D Turán T15/T4 over a rectangular domain.
  - `vars[0]` is the innermost variable, `vars[ndim-1]` the outermost (adapted by bisection).  `lo[i]` / `hi[i]` are the bounds for `vars[i]`.
  - All 2^N mixed second-derivative expressions are built automatically.
  - Returns `0` on convergence, `1` if `max_intervals` was reached, `-1` on null argument, `ndim == 0`, or allocation failure.
  - `error_est` may be NULL.

### Diagnostics

- `size_t ig_get_interval_count_used(const integrator_t *ig)` — number of subintervals used in the most recent integration call.

---

## Design Notes

**Nodes and weights** are stored as static `qfloat_t` arrays with double-double values pre-computed from high-precision decimal strings, so all 106 bits of precision are available in the quadrature rule itself with no runtime initialisation cost.

**Subinterval storage** is a dynamically-grown heap-allocated array. Linear search for the maximum-error interval is used; this is O(n) per step but n rarely exceeds tens of intervals for well-behaved integrands.

**Turán degree advantage** comes from incorporating f'' directly into the quadrature weights. For an 8-node symmetric rule this raises exactness from degree 15 (f only) to degree 31. The T4 nested sub-rule uses alternating node positions (not consecutive), which keeps all weights positive and the rule well-conditioned.

**Cache coherence** in `ig_single_integral`: `dv_eval` detects variable changes automatically via epoch tracking — each call to `dv_set_val` advances the variable's epoch, and computed nodes recompute when they see a newer epoch from their inputs.

**Threading:** the current `dval_t` and symbolic-integrator path are not yet
internally synchronised. Prefer sequential test and benchmark runs, and do not
share mutable integrator / `dval_t` state across threads without external
locking.

## Tradeoffs

- Only finite intervals `[a, b]` are supported directly. For improper integrals, apply a substitution before passing the transformed integrand.
- `ig_single_integral` (and the multi-dimensional variants) require that the integrand be expressible as a `dval_t` graph. Functions with branches, loops, or external data are better served by `ig_integral`.
- Functions with endpoint singularities or sharp peaks may require many subdivisions. Increase the max interval count via `ig_set_interval_count_max` or apply a smoothing substitution.
- The G7K15 rule evaluates the integrand at 15 points per subinterval; the Turán rule evaluates f and f'' at 8 points (16 evaluations equivalent). For expensive callbacks, the Turán rule's lower subinterval count usually wins despite the per-node overhead.
