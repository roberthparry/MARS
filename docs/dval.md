# `dval_t`

`dval_t` is a reference-counted expression DAG for differentiable values.
Expressions are built from constants, variables, and operator nodes; each node
carries a vtable that knows how to evaluate itself and construct its derivative.
Evaluation uses `qfloat` throughout for ~106-bit precision.

## Capabilities

- expression construction from constants, variables, and operators
- lazy evaluation with result caching
- symbolic differentiation to arbitrary order
- elementary and special functions mirroring the `qfloat` API
- expression parsing from and formatting to strings

## Example: Constructing an Expression

```c
#include <stdio.h>
#include "dval.h"

/* f(x) = exp(sin(x)) + 3*x^2 - 7 */
static dval_t *make_f(dval_t *x) {
    dval_t *sinx   = dv_sin(x);
    dval_t *exp_sx = dv_exp(sinx);
    dval_t *x2     = dv_pow_d(x, 2.0);
    dval_t *term2  = dv_mul_d(x2, 3.0);
    dval_t *f0     = dv_add(exp_sx, term2);
    dval_t *f      = dv_sub_d(f0, 7.0);

    dv_free(sinx);
    dv_free(exp_sx);
    dv_free(x2);
    dv_free(term2);
    dv_free(f0);

    return f;
}

int main(void) {
    dval_t *x = dv_new_named_var_d(1.25, "x");
    dval_t *f = make_f(x);

    dval_t *df_dx = dv_create_deriv(f);
    const dval_t *d2f_dx = dv_get_deriv(df_dx);

    printf("f(x)    = "); dv_print(f);
    printf("f'(x)   = "); dv_print(df_dx);
    printf("f''(x)  = "); dv_print(d2f_dx);

    qf_printf("\nAt x = 1.25:\n");
    qf_printf("f(x)    = %.34q\n", dv_eval(f));
    qf_printf("f'(x)   = %.34q\n", dv_eval(df_dx));
    qf_printf("f''(x)  = %.34q\n", dv_eval(d2f_dx));

    dv_free(df_dx);
    dv_free(f);
    dv_free(x);

    return 0;
}
```

## Example: Parsing from a String

```c
#include <stdio.h>
#include "dval.h"

int main(void) {
    dval_t *f = dval_from_string("{ exp(sin(x)) + 3*x^2 - 7 | x = 1.25 }");
    if (!f) return 1;

    dval_t *df_dx = dv_create_deriv(f);
    const dval_t *d2f_dx = dv_get_deriv(df_dx);

    printf("f(x)    = "); dv_print(f);
    printf("f'(x)   = "); dv_print(df_dx);
    printf("f''(x)  = "); dv_print(d2f_dx);

    qf_printf("\nAt x = 1.25:\n");
    qf_printf("f(x)    = %.34q\n", dv_eval(f));
    qf_printf("f'(x)   = %.34q\n", dv_eval(df_dx));
    qf_printf("f''(x)  = %.34q\n", dv_eval(d2f_dx));

    dv_free(df_dx);
    dv_free(f);

    return 0;
}
```

## Design Notes

### Node Model

Expressions are stored as a directed acyclic graph. Each node is one of:

- **constant** — a fixed `qfloat` value, optionally named
- **variable** — a mutable `qfloat` value, optionally named; changing it via
  `dv_set_val()` invalidates the cached primal and derivative values in all
  ancestor nodes
- **unary operator** — wraps one child (e.g. `sin`, `exp`, `sqrt`)
- **binary operator** — wraps two children (e.g. `+`, `*`, `pow`)

Shared subexpressions are represented once and referenced from multiple places
in the graph without duplication.

### Differentiation

Each node's vtable provides a `deriv` function that returns a new expression
graph representing the derivative of that node with respect to its input.
Calling `dv_create_deriv(f)` recursively applies the chain rule across the
graph to produce `df/dx` as a new owning expression. Higher-order derivatives
are obtained by differentiating derivative expressions again with
`dv_create_nth_deriv()`.

### Ownership and Reference Counting

Every owning handle has reference count ≥ 1. Arithmetic and function builders
retain their children (increment their refcounts) but do not steal ownership.
`dv_free()` decrements the refcount and recursively frees when it reaches zero.
`dv_get_deriv()` returns a *borrowed* pointer — do not free it.

### Evaluation

`dv_eval()` walks the DAG bottom-up, caching the `qfloat` result in each node.
Subsequent calls without any `dv_set_val()` return the cached result immediately.
Setting a variable's value with `dv_set_val()` or `dv_set_val_d()` marks the
cache invalid in the variable node; ancestor caches are invalidated lazily on
the next evaluation pass.

### Simplification

The library applies algebraic simplification rules during derivative
construction. This keeps derivative expressions compact and fast to evaluate.
The simplifier is exposed internally via `dv_simplify()` (see `dval_internal.h`).

---

## API Reference

All public declarations are in `include/dval.h`.

### Constructors — Constants

- `dval_t *dv_new_const_d(double x)` — constant node from a `double`
- `dval_t *dv_new_const(qfloat x)` — constant node from a `qfloat`
- `dval_t *dv_new_named_const(qfloat x, const char *name)` — named constant from a `qfloat`
- `dval_t *dv_new_named_const_d(double x, const char *name)` — named constant from a `double`

### Constructors — Variables

- `dval_t *dv_new_var_d(double x)` — variable node from a `double`
- `dval_t *dv_new_var(qfloat x)` — variable node from a `qfloat`
- `dval_t *dv_new_named_var(qfloat x, const char *name)` — named variable from a `qfloat`
- `dval_t *dv_new_named_var_d(double x, const char *name)` — named variable from a `double`

### Mutators

- `void dv_set_val(dval_t *dv, qfloat value)` — set the primal value (variables only); invalidates caches
- `void dv_set_val_d(dval_t *dv, double value)` — set the primal value from a `double`
- `void dv_set_name(dval_t *dv, const char *name)` — set or replace the symbolic name

### Accessors

- `qfloat dv_get_val(const dval_t *dv)` — return the stored primal value without re-evaluating
- `double dv_get_val_d(const dval_t *dv)` — return the stored primal value as a `double`
- `const dval_t *dv_get_deriv(const dval_t *dv)` — return the cached first derivative node
  (borrowed — do **not** free); built lazily on first call

### Evaluation

- `qfloat dv_eval(const dval_t *dv)` — evaluate the expression and return a `qfloat`; uses cached result if valid
- `double dv_eval_d(const dval_t *dv)` — evaluate and return a `double`

### Derivative Construction (owning)

All returned handles must be freed by the caller.

- `dval_t *dv_create_deriv(dval_t *dv)` — build the first derivative expression
- `dval_t *dv_create_2nd_deriv(dval_t *dv)` — build d²/dx²
- `dval_t *dv_create_3rd_deriv(dval_t *dv)` — build d³/dx³
- `dval_t *dv_create_nth_deriv(unsigned int n, dval_t *dv)` — build the nth derivative

### Arithmetic (graph-building, owning)

All functions return owning handles.

- `dval_t *dv_neg(dval_t *dv)` — `-dv`
- `dval_t *dv_add(dval_t *dv1, dval_t *dv2)` — `dv1 + dv2`
- `dval_t *dv_sub(dval_t *dv1, dval_t *dv2)` — `dv1 - dv2`
- `dval_t *dv_mul(dval_t *dv1, dval_t *dv2)` — `dv1 * dv2`
- `dval_t *dv_div(dval_t *dv1, dval_t *dv2)` — `dv1 / dv2`
- `dval_t *dv_add_d(dval_t *dv, double d)` — `dv + d`
- `dval_t *dv_sub_d(dval_t *dv, double d)` — `dv - d`
- `dval_t *dv_d_sub(double d, dval_t *dv)` — `d - dv`
- `dval_t *dv_mul_d(dval_t *dv, double d)` — `dv * d`
- `dval_t *dv_div_d(dval_t *dv, double d)` — `dv / d`
- `dval_t *dv_d_div(double d, dval_t *dv)` — `d / dv`

### Comparison

- `int dv_cmp(const dval_t *dv1, const dval_t *dv2)` — compare by primal value; returns -1, 0, or 1

### Elementary Functions (owning)

- `dval_t *dv_sin(dval_t *dv)` — sin
- `dval_t *dv_cos(dval_t *dv)` — cos
- `dval_t *dv_tan(dval_t *dv)` — tan
- `dval_t *dv_sinh(dval_t *dv)` — sinh
- `dval_t *dv_cosh(dval_t *dv)` — cosh
- `dval_t *dv_tanh(dval_t *dv)` — tanh
- `dval_t *dv_asin(dval_t *dv)` — arcsin
- `dval_t *dv_acos(dval_t *dv)` — arccos
- `dval_t *dv_atan(dval_t *dv)` — arctan
- `dval_t *dv_atan2(dval_t *dv1, dval_t *dv2)` — four-quadrant arctan2(dv1, dv2)
- `dval_t *dv_asinh(dval_t *dv)` — inverse hyperbolic sine
- `dval_t *dv_acosh(dval_t *dv)` — inverse hyperbolic cosine
- `dval_t *dv_atanh(dval_t *dv)` — inverse hyperbolic tangent
- `dval_t *dv_exp(dval_t *dv)` — natural exponential
- `dval_t *dv_log(dval_t *dv)` — natural logarithm
- `dval_t *dv_sqrt(dval_t *dv)` — square root
- `dval_t *dv_pow_d(dval_t *dv, double d)` — `dv ^ d` (scalar exponent)
- `dval_t *dv_pow(dval_t *dv1, dval_t *dv2)` — `dv1 ^ dv2`

### Special Functions (owning)

- `dval_t *dv_abs(dval_t *dv)` — absolute value
- `dval_t *dv_hypot(dval_t *dv1, dval_t *dv2)` — sqrt(dv1² + dv2²)
- `dval_t *dv_erf(dval_t *dv)` — error function
- `dval_t *dv_erfc(dval_t *dv)` — complementary error function
- `dval_t *dv_erfinv(dval_t *dv)` — inverse error function
- `dval_t *dv_erfcinv(dval_t *dv)` — inverse complementary error function
- `dval_t *dv_gamma(dval_t *dv)` — Γ(x)
- `dval_t *dv_lgamma(dval_t *dv)` — ln|Γ(x)|
- `dval_t *dv_digamma(dval_t *dv)` — ψ(x) = d/dx ln Γ(x)
- `dval_t *dv_trigamma(dval_t *dv)` — ψ'(x)
- `dval_t *dv_lambert_w0(dval_t *dv)` — Lambert W principal branch W₀(x)
- `dval_t *dv_lambert_wm1(dval_t *dv)` — Lambert W branch W₋₁(x)
- `dval_t *dv_beta(dval_t *dv1, dval_t *dv2)` — B(a, b)
- `dval_t *dv_logbeta(dval_t *dv1, dval_t *dv2)` — ln B(a, b)
- `dval_t *dv_normal_pdf(dval_t *dv)` — standard normal PDF φ(x)
- `dval_t *dv_normal_cdf(dval_t *dv)` — standard normal CDF Φ(x)
- `dval_t *dv_normal_logpdf(dval_t *dv)` — ln φ(x)
- `dval_t *dv_ei(dval_t *dv)` — Ei(x), exponential integral
- `dval_t *dv_e1(dval_t *dv)` — E₁(x), exponential integral

### Lifetime Management

- `void dv_free(dval_t *dv)` — decrement refcount; free the node and recursively its children when it reaches zero. Must be called exactly once per owning handle.

### String Conversion

- `char *dv_to_string(const dval_t *dv, style_t style)` — serialise the expression; `style` is `style_FUNCTION` or `style_EXPRESSION`. Returns a newly allocated C string; the caller must free it.
- `void dv_print(const dval_t *dv)` — print the expression to stdout in `style_EXPRESSION` format

### Parsing

- `dval_t *dval_from_string(const char *s)` — construct a `dval_t` from a string in the format produced by `dv_to_string(..., style_EXPRESSION)`:

  ```
  { expr | x₀ = val, ...; [name] = val, ... }
  ```

  Variables appear before the `;`; named constants appear after it. Returns an owning handle on success, or NULL on error (details written to stderr).

  Accepted shorthand in the string:
  - `x_0` for subscript x₀
  - `*` for explicit multiplication
  - `^N` for integer exponents
  - `[bracket names]` for identifiers that are not single-letter-plus-subscript

