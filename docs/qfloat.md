# `qfloat_t`

`qfloat_t` is a double-double floating-point type built from the unevaluated sum
of two IEEE-754 `double` values.

## Representation

```text
qfloat_t = hi + lo
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
        qfloat_t x = qf_from_string(inputs[i]);
        qfloat_t w = qf_lambert_w0(x);
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

`qfloat_t` supports decimal parsing and high-precision formatting, making it
suitable for both numeric work and regression tests. The `%q`/`%Q` specifiers
extend `printf` for `qfloat_t` arguments passed by value.

## Tradeoffs

Compared with `double`, `qfloat_t` offers more precision at higher runtime cost.
Compared with arbitrary-precision libraries it is lighter-weight and simpler
to integrate into a C99 codebase.

## Platform Notes

`qfloat_t` is built on IEEE-754 `double` arithmetic and relies on the usual
double-double assumptions: predictable round-to-nearest behaviour, stable
error-free transforms, and the ordinary floating-point semantics expected on
modern x86_64-class CPUs.

That makes `qfloat_t` fast and practical, but it also means release-build
optimisation settings are not entirely architecture-neutral. On the current
project machine, plain release flags such as `-O2` and `-O2 -flto` preserve the
expected behaviour, while adding native CPU tuning flags
`-march=native -mtune=native` caused `Ei`/`E1` regressions in the `qfloat`
release tests.

Because of that, the project treats `qfloat` release targets a little more
conservatively than some other subsystems:

- `make test_qfloat`
- `make memtest_qfloat`
- `make bench_qfloat_gamma_maths`

These targets rebuild the `qfloat` path with the safer release profile instead
of the more aggressive project-wide native-tuned flags. This quirk is currently
specific to `qfloat`; other subsystems still use the broader release defaults.

---

## API Reference

All declarations are in `include/qfloat.h`.

### Constants

| Name | Value |
|---|---|
| `QF_NAN` | Not-a-number |
| `QF_INF` | +∞ |
| `QF_NINF` | -∞ |
| `QF_MAX` | Maximum finite `qfloat_t` value |
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
| `QF_SQRT_PI` | √π |
| `QF_SQRT1ONPI` | √(1/π) |
| `QF_2_SQRTPI` | 2√(1/π) |
| `QF_INV_SQRT_2PI` | 1/√(2π) |
| `QF_LOG_SQRT_2PI` | ln √(2π) |
| `QF_LN_2PI` | ln(2π) |
| `QF_EULER_MASCHERONI` | Euler–Mascheroni constant γ |

### Construction and Conversion

- `qfloat_t qf_from_double(double x)`  
  Construct a `qfloat_t` from a `double`. Exact: `hi = x`, `lo = 0`.

- `double qf_to_double(qfloat_t x)`  
  Convert a `qfloat_t` to the nearest `double`.

- `qfloat_t qf_from_string(const char *s)`  
  Parse a decimal string (integer, fractional, optional sign, scientific
  notation). Uses exact decimal accumulation for full precision.

- `void qf_to_string(qfloat_t x, char *out, size_t out_size)`  
  Write normalised scientific notation with 32 significant digits into `out`.

### Comparison and Classification

- `bool qf_eq(qfloat_t a, qfloat_t b)` — true if `a == b`
- `bool qf_lt(qfloat_t a, qfloat_t b)` — true if `a < b`
- `bool qf_le(qfloat_t a, qfloat_t b)` — true if `a <= b`
- `bool qf_gt(qfloat_t a, qfloat_t b)` — true if `a > b`
- `bool qf_ge(qfloat_t a, qfloat_t b)` — true if `a >= b`
- `int  qf_cmp(qfloat_t a, qfloat_t b)` — returns -1, 0, or +1
- `int  qf_signbit(qfloat_t x)` — sign bit of `x`
- `bool qf_isnan(qfloat_t x)` — true if either component is IEEE-754 NaN
- `bool qf_isinf(qfloat_t x)` — true if `x` is ±∞
- `bool qf_isposinf(qfloat_t x)` — true if `x` is +∞
- `bool qf_isneginf(qfloat_t x)` — true if `x` is -∞

### Arithmetic

- `qfloat_t qf_neg(qfloat_t x)` — unary negation `-x`
- `qfloat_t qf_abs(qfloat_t x)` — absolute value `|x|`
- `qfloat_t qf_add(qfloat_t a, qfloat_t b)` — `a + b` using TwoSum + renormalization
- `qfloat_t qf_add_double(qfloat_t x, double y)` — `x + y`
- `qfloat_t qf_sub(qfloat_t a, qfloat_t b)` — `a - b`
- `qfloat_t qf_mul(qfloat_t a, qfloat_t b)` — `a * b` using Dekker TwoProd
- `qfloat_t qf_mul_double(qfloat_t x, double a)` — `x * a`
- `qfloat_t qf_mul_pow10(qfloat_t x, int k)` — `x * 10^k` exactly
- `qfloat_t qf_div(qfloat_t a, qfloat_t b)` — `a / b` via Newton quotient
- `qfloat_t qf_sqr(qfloat_t x)` — `x^2` exactly
- `qfloat_t qf_floor(qfloat_t x)` — floor of `x`
- `qfloat_t qf_ldexp(qfloat_t x, int k)` — `x * 2^k`
- `qfloat_t qf_pow_int(qfloat_t x, int n)` — `x^n` by exponentiation-by-squaring; negative exponents supported
- `qfloat_t qf_pow(qfloat_t x, qfloat_t y)` — `x^y` via `exp(y * log(x))`; returns NaN on domain error
- `qfloat_t qf_pow10(int n)` — `10^n` exactly

### Elementary Functions

- `qfloat_t qf_sqrt(qfloat_t x)` — square root via Newton refinement
- `qfloat_t qf_exp(qfloat_t x)` — natural exponential
- `qfloat_t qf_log(qfloat_t x)` — natural logarithm (`x > 0`)
- `qfloat_t qf_hypot(qfloat_t x, qfloat_t y)` — `sqrt(x^2 + y^2)` without overflow
- `qfloat_t qf_sin(qfloat_t x)` — sine (radians)
- `qfloat_t qf_cos(qfloat_t x)` — cosine (radians)
- `qfloat_t qf_tan(qfloat_t x)` — tangent; NaN at poles
- `qfloat_t qf_asin(qfloat_t x)` — inverse sine
- `qfloat_t qf_acos(qfloat_t x)` — inverse cosine
- `qfloat_t qf_atan(qfloat_t x)` — inverse tangent
- `qfloat_t qf_atan2(qfloat_t y, qfloat_t x)` — four-quadrant inverse tangent
- `qfloat_t qf_sinh(qfloat_t x)` — hyperbolic sine
- `qfloat_t qf_cosh(qfloat_t x)` — hyperbolic cosine
- `qfloat_t qf_tanh(qfloat_t x)` — hyperbolic tangent
- `qfloat_t qf_asinh(qfloat_t x)` — inverse hyperbolic sine
- `qfloat_t qf_acosh(qfloat_t x)` — inverse hyperbolic cosine
- `qfloat_t qf_atanh(qfloat_t x)` — inverse hyperbolic tangent

### Special Functions

**Gamma family**

- `qfloat_t qf_gamma(qfloat_t x)` — Γ(x)
- `qfloat_t qf_lgamma(qfloat_t x)` — ln|Γ(x)|
- `qfloat_t qf_digamma(qfloat_t x)` — ψ(x) = d/dx ln Γ(x)
- `qfloat_t qf_trigamma(qfloat_t x)` — ψ'(x); pole at non-positive integers
- `qfloat_t qf_tetragamma(qfloat_t x)` — ψ''(x); pole at non-positive integers
- `qfloat_t qf_gammainv(qfloat_t y)` — principal branch of Γ⁻¹(y)
- `qfloat_t qf_gammainc_lower(qfloat_t s, qfloat_t x)` — lower incomplete γ(s, x)
- `qfloat_t qf_gammainc_upper(qfloat_t s, qfloat_t x)` — upper incomplete Γ(s, x)
- `qfloat_t qf_gammainc_P(qfloat_t s, qfloat_t x)` — regularized P(s, x) = γ(s,x)/Γ(s)
- `qfloat_t qf_gammainc_Q(qfloat_t s, qfloat_t x)` — regularized Q(s, x) = Γ(s,x)/Γ(s)

**Error functions**

- `qfloat_t qf_erf(qfloat_t x)` — error function
- `qfloat_t qf_erfc(qfloat_t x)` — complementary error function
- `qfloat_t qf_erfinv(qfloat_t x)` — inverse error function
- `qfloat_t qf_erfcinv(qfloat_t x)` — inverse complementary error function

**Lambert W**

- `qfloat_t qf_lambert_w0(qfloat_t x)` — principal branch W₀(x)
- `qfloat_t qf_lambert_wm1(qfloat_t x)` — branch W₋₁(x)
- `qfloat_t qf_productlog(qfloat_t x)` — alias for `qf_lambert_w0`

**Beta and binomial**

- `qfloat_t qf_beta(qfloat_t a, qfloat_t b)` — B(a, b)
- `qfloat_t qf_logbeta(qfloat_t a, qfloat_t b)` — ln B(a, b)
- `qfloat_t qf_binomial(qfloat_t a, qfloat_t b)` — generalized binomial coefficient
- `qfloat_t qf_beta_pdf(qfloat_t x, qfloat_t a, qfloat_t b)` — beta distribution PDF
- `qfloat_t qf_logbeta_pdf(qfloat_t x, qfloat_t a, qfloat_t b)` — log beta distribution PDF

**Normal distribution**

- `qfloat_t qf_normal_pdf(qfloat_t x)` — φ(x), standard normal PDF
- `qfloat_t qf_normal_cdf(qfloat_t x)` — Φ(x), standard normal CDF
- `qfloat_t qf_normal_logpdf(qfloat_t x)` — ln φ(x)

**Exponential integrals**

- `qfloat_t qf_ei(qfloat_t x)` — Ei(x) = −PV∫_{-x}^∞ (e^{-t}/t) dt, principal value; branch cut on (−∞, 0]
- `qfloat_t qf_e1(qfloat_t x)` — E₁(x) = ∫_x^∞ (e^{-t}/t) dt, for x > 0

### Formatted Output

Pass `qfloat_t` values **by value** to `%q`/`%Q` specifiers.

- `int qf_printf(const char *fmt, ...)` — like `printf`; adds `%q` (decimal) and `%Q` (scientific)
- `int qf_sprintf(char *out, size_t n, const char *fmt, ...)` — like `snprintf`
- `int qf_vsprintf(char *out, size_t n, const char *fmt, va_list ap)` — `va_list` variant

```c
qfloat_t x = qf_from_string("1");
qf_printf("W0(1) = %.34q\n", x);   // 34 significant digits
qf_printf("W0(1) = %Q\n",   x);    // scientific notation
```

---

## Benchmark Coverage

`qfloat_t` has a focused arithmetic and special-functions benchmark here:

- [bench/qfloat/bench_qfloat_gamma_maths.c](/home/rparry/Projects/MARS/bench/qfloat/bench_qfloat_gamma_maths.c)

Run it with:

```sh
make bench_qfloat_gamma_maths
```

Sample results below were measured on:

- `Linux x86_64`
- kernel `6.8.0-110-generic`
- `Intel(R) Core(TM) i7-4510U CPU @ 2.00GHz`
- `4` logical CPUs

Current sample results with `MARS_BENCH_SCALE=5`:

| Case | Time |
|---|---:|
| `qf_exp(1)` | `1.146 us` |
| `qf_log(10)` | `2.222 us` |
| `qf_erf(0.5)` | `2.615 us` |
| `qf_erfc(0.5)` | `2.489 us` |
| `qf_gamma(2.3)` | `1.107 us` |
| `qf_lgamma(2.3)` | `3.562 us` |
| `qf_gamma(2.5)` | `1.109 us` |
| `qf_lgamma(2.5)` | `3.073 us` |
| `qf_gamma(3.5)` | `1.129 us` |
| `qf_lgamma(3.5)` | `3.515 us` |
| `qf_digamma(2.3)` | `1.828 us` |
| `qf_trigamma(2.3)` | `3.316 us` |
| `qf_tetragamma(2.3)` | `2.868 us` |
| `qf_gammainv(119292.4619946090070787515047110059)` | `83.440 us` |
| `qf_lambert_w0(1)` | `9.509 us` |
| `qf_lambert_wm1(-0.1)` | `55.965 us` |
| `qf_ei(1)` | `2.857 us` |
| `qf_e1(1)` | `2.903 us` |
| `qf_beta(2.3, 4.5)` | `3.474 us` |
| `qf_logbeta(2.3, 4.5)` | `17.997 us` |

For a broader benchmark overview, see
[docs/benchmarks.md](/home/rparry/Projects/MARS/docs/benchmarks.md).
