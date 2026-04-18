# `integrator_t`

`integrator_t` is an adaptive Gauss-Kronrod G7K15 numerical integrator that works at full `qfloat` (~34 decimal digits) precision. It evaluates a user-supplied callback over a finite interval `[a, b]`, automatically subdividing the interval until the estimated error is within the requested tolerance.

---

## Algorithm

Each subinterval is evaluated with a 15-point Kronrod rule (K15) containing an embedded 7-point Gauss rule (G7). The per-subinterval error estimate is `|K15 − G7|`. The subinterval with the largest error is bisected at each step. Iteration continues until

```
total_error ≤ max(abs_tol, rel_tol × |result|)
```

or the maximum subinterval count is reached.

The G7K15 nodes and weights are statically initialised from high-precision decimal strings so that they inherit full `qfloat` accuracy.

---

## Example

### Basic integration

```c
#include <stdio.h>
#include "qfloat.h"
#include "integrator.h"

static qfloat gaussian(qfloat x, void *ctx) {
    (void)ctx;
    return qf_exp(qf_neg(qf_sqr(x)));
}

int main(void) {
    integrator_t *ig = integrator_create();

    qfloat result, err;
    integrator_eval(ig, gaussian, NULL,
                    qf_from_double(-3.0), qf_from_double(3.0),
                    &result, &err);

    qf_printf("∫₋₃³ exp(-x²) dx ≈ %q\n", result);
    qf_printf("error estimate      ≈ %q\n", err);
    printf("subintervals used: %zu\n", integrator_last_intervals(ig));

    integrator_destroy(ig);
    return 0;
}
```

```text
∫₋₃³ exp(-x²) dx ≈ 1.772414696519042467789405388127980e+0
  error estimate   ≈ 2.807878575265393832671772224952451e-21
  subintervals used: 200
```

### Custom context

```c
typedef struct { qfloat exponent; } power_ctx;

static qfloat power_fn(qfloat x, void *ctx) {
    power_ctx *pc = ctx;
    return qf_pow(x, pc->exponent);
}

int main(void) {
    /* ∫₀¹ x^2.5 dx = 1 / 3.5 */
    integrator_t *ig = integrator_create();
    power_ctx ctx = { qf_from_string("2.5") };

    qfloat result, err;
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
∫₀¹ x^2.5 dx ≈ 2.857142857142857142866307551087165e-1
  error estimate   ≈ 9.829684091587462300529671656255823e-22
```

---

## API Reference

All declarations are in `include/integrator.h`.

### Types

- `integrand_fn` — `typedef qfloat (*integrand_fn)(qfloat x, void *ctx)` — integrand callback.
- `integrator_t` — opaque adaptive integrator.

### Lifecycle

- `integrator_t *integrator_create(void)` — create an integrator with default tolerances (1e-21 absolute and relative, 200 max subintervals). Returns NULL on allocation failure.
- `void integrator_destroy(integrator_t *ig)` — free the integrator. Safe to call with NULL.

### Configuration

- `void integrator_set_tol(integrator_t *ig, qfloat abs_tol, qfloat rel_tol)` — override convergence tolerances. Convergence when `total_error ≤ max(abs_tol, rel_tol × |result|)`.
- `void integrator_set_max_intervals(integrator_t *ig, size_t max_intervals)` — override the maximum number of subintervals before the algorithm halts.

### Evaluation

- `int integrator_eval(integrator_t *ig, integrand_fn f, void *ctx, qfloat a, qfloat b, qfloat *result, qfloat *error_est)` — integrate `f` over `[a, b]`.
  - Returns `0` on convergence.
  - Returns `1` if `max_intervals` was reached before convergence.
  - Returns `-1` on internal allocation failure or if `ig`, `f`, or `result` is NULL.
  - `error_est` may be NULL if the error estimate is not needed.
  - Reversed limits (`a > b`) are handled correctly.

### Diagnostics

- `size_t integrator_last_intervals(const integrator_t *ig)` — number of subintervals used in the most recent call to `integrator_eval`.

---

## Design Notes

**Nodes and weights** are stored as static `qfloat` arrays with double-double values pre-computed from the 37-digit reference constants, so all 106 bits of precision are available in the quadrature rule itself with no runtime initialisation cost.

**Subinterval storage** is a dynamically-grown heap-allocated array. Linear search for the maximum-error interval is used; this is O(n) per step but n rarely exceeds tens of intervals for well-behaved integrands.

**Error estimate** is the sum of per-subinterval `|K15 − G7|` values. This is an optimistic bound — for smooth functions K15 is extremely accurate, so the estimate is typically very conservative.

## Tradeoffs

- Only finite intervals `[a, b]` are supported directly. For improper integrals, apply a substitution before passing the transformed integrand.
- Functions with endpoint singularities or sharp peaks may require many subdivisions. Increase `max_intervals` or apply a smoothing substitution.
- The G7K15 rule evaluates the integrand at 15 points per subinterval. For expensive integrands consider caching results yourself or using a lower-order rule.
