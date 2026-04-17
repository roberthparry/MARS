# `qfloat`

`qfloat` is a double-double floating-point type built from the unevaluated sum
of two IEEE-754 `double` values.

## Representation

```text
qfloat = hi + lo
```

`hi` carries the leading 53 bits of precision and `lo` carries the trailing
correction term. Together they provide approximately 106 bits of precision
(about 31–32 decimal digits). The invariant `|lo| <= 0.5 * ulp(hi)` is
maintained after every operation.

## Capabilities

- ~106 bits of precision (~31–32 decimal digits)
- arithmetic: add, subtract, multiply, divide, power
- elementary functions: exp, log, sqrt, sin, cos, tan, atan2, hypot, and inverses
- special functions: gamma, erf, Lambert W, beta, incomplete gamma, exponential integrals, normal distribution
- decimal parsing and formatting
- `printf` support through `%q` and `%Q`

## Example

```c
#include <stdio.h>
#include "qfloat.h"

int main(void) {
    const char *inputs[] = {
        "0", "1e-6", "0.1", "1", "5",
        "-0.3678794411714423215955237701614609",
        NULL
    };
    for (int i = 0; inputs[i] != NULL; i++) {
        qfloat x = qf_from_string(inputs[i]);
        qfloat w = qf_lambert_w0(x);
        qf_printf("W0(%s) = %q\n", inputs[i], w);
    }
    return 0;
}
```

Expected output:

```text
W0(0) = 0
W0(1e-6) = 9.999990000014999973333385416558944e-7
W0(0.1) = 0.09127652716086226429989572142317946
W0(1) = 0.5671432904097838729999686622103575
W0(5) = 1.326724665242200223635099297758082
W0(-0.3678794411714423215955237701614609) = -1
```

## Design Notes

### Arithmetic Core

The implementation is based on standard error-free transformation routines:

- `TwoSum` / `QuickTwoSum` — exact representation of addition error
- `TwoProd` (Dekker) — exact representation of multiplication error

These make the rounding error explicit so that it can be captured in `lo` and
the result renormalised into a stable `(hi, lo)` pair.

### Function Implementation

Functions such as `exp`, `log`, `sin`, `cos`, `sqrt`, `pow`, and `atan2` are
built from argument reduction, polynomial or rational approximation,
Newton-style refinement, and renormalization back into `(hi, lo)` form.
Special functions build on the same arithmetic core.

### Formatting and Parsing

`qfloat` supports decimal parsing and high-precision formatting, making it
suitable for both numeric work and regression tests. The `%q`/`%Q` specifiers
extend `printf` for `qfloat` arguments passed by value.

## Tradeoffs

Compared with `double`, `qfloat` offers more precision at higher runtime cost.
Compared with arbitrary-precision libraries it is lighter-weight and simpler
to integrate into a C99 codebase.

---

## API Reference

All declarations are in `include/qfloat.h`.

### Constants

| Name | Value |
|---|---|
| `QF_NAN` | Not-a-number |
| `QF_INF` | +∞ |
| `QF_NINF` | -∞ |
| `QF_MAX` | Maximum finite `qfloat` value |
| `QF_PI` | π |
| `QF_2PI` | 2π |
| `QF_PI_2` | π/2 |
| `QF_PI_4` | π/4 |
| `QF_3PI_4` | 3π/4 |
| `QF_PI_6` | π/6 |
| `QF_PI_3` | π/3 |
| `QF_2_PI` | 2/π |
| `QF_E` | e |
| `QF_INV_E` | 1/e |
| `QF_LN2` | ln 2 |
| `QF_INVLN2` | 1/ln 2 |
| `QF_SQRT_HALF` | √(1/2) |
| `QF_SQRT1ONPI` | √(1/π) |
| `QF_2_SQRTPI` | 2√(1/π) |
| `QF_LOG_SQRT_2PI` | ln √(2π) |
| `QF_EULER_MASCHERONI` | Euler–Mascheroni constant γ |
| `QF_INV_SQRT_2PI` | 1/√(2π) |
| `QF_LN_2PI` | ln(2π) |

### Construction and Conversion

- `qfloat qf_from_double(double x)`  
  Construct a `qfloat` from a `double`. Exact: `hi = x`, `lo = 0`.

- `double qf_to_double(qfloat x)`  
  Convert a `qfloat` to the nearest `double`.

- `qfloat qf_from_string(const char *s)`  
  Parse a decimal string (integer, fractional, optional sign, scientific
  notation). Uses exact decimal accumulation for full precision.

- `void qf_to_string(qfloat x, char *out, size_t out_size)`  
  Write normalised scientific notation with 32 significant digits into `out`.

### Comparison and Classification

- `bool qf_eq(qfloat a, qfloat b)` — true if `a == b`
- `bool qf_lt(qfloat a, qfloat b)` — true if `a < b`
- `bool qf_le(qfloat a, qfloat b)` — true if `a <= b`
- `bool qf_gt(qfloat a, qfloat b)` — true if `a > b`
- `bool qf_ge(qfloat a, qfloat b)` — true if `a >= b`
- `int  qf_cmp(qfloat a, qfloat b)` — returns -1, 0, or +1
- `int  qf_signbit(qfloat x)` — sign bit of `x`
- `bool qf_isnan(qfloat x)` — true if either component is IEEE-754 NaN
- `bool qf_isinf(qfloat x)` — true if `x` is ±∞
- `bool qf_isposinf(qfloat x)` — true if `x` is +∞
- `bool qf_isneginf(qfloat x)` — true if `x` is -∞

### Arithmetic

- `qfloat qf_neg(qfloat x)` — unary negation `-x`
- `qfloat qf_abs(qfloat x)` — absolute value `|x|`
- `qfloat qf_add(qfloat a, qfloat b)` — `a + b` using TwoSum + renormalization
- `qfloat qf_add_double(qfloat x, double y)` — `x + y`
- `qfloat qf_sub(qfloat a, qfloat b)` — `a - b`
- `qfloat qf_mul(qfloat a, qfloat b)` — `a * b` using Dekker TwoProd
- `qfloat qf_mul_double(qfloat x, double a)` — `x * a`
- `qfloat qf_mul_pow10(qfloat x, int k)` — `x * 10^k` exactly
- `qfloat qf_div(qfloat a, qfloat b)` — `a / b` via Newton quotient
- `qfloat qf_sqr(qfloat x)` — `x^2` exactly
- `qfloat qf_floor(qfloat x)` — floor of `x`
- `qfloat qf_ldexp(qfloat x, int k)` — `x * 2^k`
- `qfloat qf_pow_int(qfloat x, int n)` — `x^n` by exponentiation-by-squaring; negative exponents supported
- `qfloat qf_pow(qfloat x, qfloat y)` — `x^y` via `exp(y * log(x))`; returns NaN on domain error
- `qfloat qf_pow10(int n)` — `10^n` exactly

### Elementary Functions

- `qfloat qf_sqrt(qfloat x)` — square root via Newton refinement
- `qfloat qf_exp(qfloat x)` — natural exponential
- `qfloat qf_log(qfloat x)` — natural logarithm (`x > 0`)
- `qfloat qf_hypot(qfloat x, qfloat y)` — `sqrt(x^2 + y^2)` without overflow
- `qfloat qf_sin(qfloat x)` — sine (radians)
- `qfloat qf_cos(qfloat x)` — cosine (radians)
- `qfloat qf_tan(qfloat x)` — tangent; NaN at poles
- `qfloat qf_asin(qfloat x)` — inverse sine
- `qfloat qf_acos(qfloat x)` — inverse cosine
- `qfloat qf_atan(qfloat x)` — inverse tangent
- `qfloat qf_atan2(qfloat y, qfloat x)` — four-quadrant inverse tangent
- `qfloat qf_sinh(qfloat x)` — hyperbolic sine
- `qfloat qf_cosh(qfloat x)` — hyperbolic cosine
- `qfloat qf_tanh(qfloat x)` — hyperbolic tangent
- `qfloat qf_asinh(qfloat x)` — inverse hyperbolic sine
- `qfloat qf_acosh(qfloat x)` — inverse hyperbolic cosine
- `qfloat qf_atanh(qfloat x)` — inverse hyperbolic tangent

### Special Functions

**Gamma family**

- `qfloat qf_gamma(qfloat x)` — Γ(x)
- `qfloat qf_lgamma(qfloat x)` — ln|Γ(x)|
- `qfloat qf_digamma(qfloat x)` — ψ(x) = d/dx ln Γ(x)
- `qfloat qf_trigamma(qfloat x)` — ψ'(x); pole at non-positive integers
- `qfloat qf_tetragamma(qfloat x)` — ψ''(x); pole at non-positive integers
- `qfloat qf_gammainv(qfloat y)` — principal branch of Γ⁻¹(y)
- `qfloat qf_gammainc_lower(qfloat s, qfloat x)` — lower incomplete γ(s, x)
- `qfloat qf_gammainc_upper(qfloat s, qfloat x)` — upper incomplete Γ(s, x)
- `qfloat qf_gammainc_P(qfloat s, qfloat x)` — regularized P(s, x) = γ(s,x)/Γ(s)
- `qfloat qf_gammainc_Q(qfloat s, qfloat x)` — regularized Q(s, x) = Γ(s,x)/Γ(s)

**Error functions**

- `qfloat qf_erf(qfloat x)` — error function
- `qfloat qf_erfc(qfloat x)` — complementary error function
- `qfloat qf_erfinv(qfloat x)` — inverse error function
- `qfloat qf_erfcinv(qfloat x)` — inverse complementary error function

**Lambert W**

- `qfloat qf_lambert_w0(qfloat x)` — principal branch W₀(x)
- `qfloat qf_lambert_wm1(qfloat x)` — branch W₋₁(x)
- `qfloat qf_productlog(qfloat x)` — alias for `qf_lambert_w0`

**Beta and binomial**

- `qfloat qf_beta(qfloat a, qfloat b)` — B(a, b)
- `qfloat qf_logbeta(qfloat a, qfloat b)` — ln B(a, b)
- `qfloat qf_binomial(qfloat a, qfloat b)` — generalized binomial coefficient
- `qfloat qf_beta_pdf(qfloat x, qfloat a, qfloat b)` — beta distribution PDF
- `qfloat qf_logbeta_pdf(qfloat x, qfloat a, qfloat b)` — log beta distribution PDF

**Normal distribution**

- `qfloat qf_normal_pdf(qfloat x)` — φ(x), standard normal PDF
- `qfloat qf_normal_cdf(qfloat x)` — Φ(x), standard normal CDF
- `qfloat qf_normal_logpdf(qfloat x)` — ln φ(x)

**Exponential integrals**

- `qfloat qf_ei(qfloat x)` — Ei(x) = −PV∫_{-x}^∞ (e^{-t}/t) dt, principal value; branch cut on (−∞, 0]
- `qfloat qf_e1(qfloat x)` — E₁(x) = ∫_x^∞ (e^{-t}/t) dt, for x > 0

### Formatted Output

Pass `qfloat` values **by value** to `%q`/`%Q` specifiers.

- `int qf_printf(const char *fmt, ...)` — like `printf`; adds `%q` (decimal) and `%Q` (scientific)
- `int qf_sprintf(char *out, size_t n, const char *fmt, ...)` — like `snprintf`
- `int qf_vsprintf(char *out, size_t n, const char *fmt, va_list ap)` — `va_list` variant

```c
qfloat x = qf_from_string("1");
qf_printf("W0(1) = %.34q\n", x);   // 34 significant digits
qf_printf("W0(1) = %Q\n",   x);    // scientific notation
```
