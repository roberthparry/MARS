# `qcomplex_t`

`qcomplex_t` is a double-double complex number with `qfloat_t` real and imaginary parts, providing approximately 106 bits of precision (~31–32 decimal digits) across the full complex plane.

## Representation

```c
typedef struct {
    qfloat_t re;  /* real part */
    qfloat_t im;  /* imaginary part */
} qcomplex_t;
```

## Capabilities

- ~106 bits of precision (~31–32 decimal digits)
- arithmetic: add, subtract, multiply, divide, negate, conjugate
- magnitude and argument
- elementary functions: exp, log, pow, sqrt
- trigonometric and hyperbolic functions (and their inverses)
- special functions: erf, gamma, digamma, beta, Lambert W, incomplete gamma, exponential integrals, normal distribution
- polar form conversion
- parsing from string
- printf-style formatting and printing

## Example

```c
#include <stdio.h>
#include "qcomplex.h"
#include "qfloat.h"

int main(void) {
    /* Euler's identity: exp(iπ) + 1 = 0 */
    qcomplex_t z   = qc_make(qf_from_double(0.0), QF_PI);
    qcomplex_t r   = qc_add(qc_exp(z), qc_make(qf_from_double(1.0), qf_from_double(0.0)));

    qc_printf("exp(iπ) + 1 = %.34z\n", r);
    return 0;
}
```

```text
exp(iπ) + 1 = 0.0000000000000000000000000000000000 + 0.0000000000000000000000000000000000i
```

---

## API Reference

All declarations are in `include/qcomplex.h`.

### Construction

- `qcomplex_t qc_make(qfloat_t re, qfloat_t im)` — construct from real and imaginary parts (inline).

### Basic Arithmetic

| Function | Description |
|---|---|
| `qc_add(a, b)` | `a + b` |
| `qc_sub(a, b)` | `a - b` |
| `qc_mul(a, b)` | `a * b` |
| `qc_div(a, b)` | `a / b` |
| `qc_neg(a)` | `-a` |
| `qc_conj(a)` | complex conjugate `re - i*im` |

### Magnitude and Argument

| Function | Description |
|---|---|
| `qc_abs(z)` → `qfloat_t` | `|z|` — modulus |
| `qc_arg(z)` → `qfloat_t` | `arg(z)` — principal argument in `(-π, π]` |

### Polar Form

| Function | Description |
|---|---|
| `qc_from_polar(r, theta)` | construct from polar coordinates |
| `qc_to_polar(z, &r, &theta)` | extract polar coordinates |

### Elementary Functions

| Function | Description |
|---|---|
| `qc_exp(z)` | `e^z` |
| `qc_log(z)` | principal logarithm `ln(z)` |
| `qc_pow(a, b)` | `a^b = exp(b * log(a))` (principal branch) |
| `qc_sqrt(z)` | principal square root |

### Trigonometric Functions

| Function | Description |
|---|---|
| `qc_sin(z)` | sine |
| `qc_cos(z)` | cosine |
| `qc_tan(z)` | tangent |
| `qc_asin(z)` | arcsine (principal branch) |
| `qc_acos(z)` | arccosine (principal branch) |
| `qc_atan(z)` | arctangent |
| `qc_atan2(y, x)` | four-quadrant arctangent |

### Hyperbolic Functions

| Function | Description |
|---|---|
| `qc_sinh(z)` | hyperbolic sine |
| `qc_cosh(z)` | hyperbolic cosine |
| `qc_tanh(z)` | hyperbolic tangent |
| `qc_asinh(z)` | inverse hyperbolic sine |
| `qc_acosh(z)` | inverse hyperbolic cosine (principal branch) |
| `qc_atanh(z)` | inverse hyperbolic tangent |

### Special Functions

| Function | Description |
|---|---|
| `qc_erf(z)` | error function |
| `qc_erfc(z)` | complementary error function `1 - erf(z)` |
| `qc_erfinv(z)` | inverse error function |
| `qc_erfcinv(z)` | inverse complementary error function |
| `qc_gamma(z)` | Γ(z) |
| `qc_lgamma(z)` | ln Γ(z) |
| `qc_digamma(z)` | ψ(z) = Γ′(z)/Γ(z) |
| `qc_trigamma(z)` | ψ₁(z) = ψ′(z) |
| `qc_tetragamma(z)` | ψ₂(z) = −ψ^(2)(z) (positive, = +2Σ 1/(z+k)³) |
| `qc_gammainv(z)` | inverse of the gamma function (principal branch, Re z ≥ 1.46) |
| `qc_beta(a, b)` | B(a,b) = Γ(a)Γ(b)/Γ(a+b) |
| `qc_logbeta(a, b)` | ln B(a,b) |
| `qc_binomial(a, b)` | generalised binomial coefficient Γ(a+1)/(Γ(b+1)Γ(a−b+1)) |
| `qc_beta_pdf(x, a, b)` | beta distribution PDF |
| `qc_logbeta_pdf(x, a, b)` | log beta distribution PDF |
| `qc_normal_pdf(z)` | standard normal PDF φ(z) |
| `qc_normal_cdf(z)` | standard normal CDF Φ(z) |
| `qc_normal_logpdf(z)` | log standard normal PDF |
| `qc_productlog(z)` | principal Lambert W function W₀(z): W·e^W = z |
| `qc_gammainc_lower(s, x)` | lower incomplete gamma γ(s,x) |
| `qc_gammainc_upper(s, x)` | upper incomplete gamma Γ(s,x) |
| `qc_gammainc_P(s, x)` | regularised lower incomplete gamma P(s,x) = γ(s,x)/Γ(s) |
| `qc_gammainc_Q(s, x)` | regularised upper incomplete gamma Q(s,x) = Γ(s,x)/Γ(s) |
| `qc_ei(z)` | exponential integral Ei(z) |
| `qc_e1(z)` | exponential integral E₁(z); satisfies E₁(z) = −Ei(−z) for real z > 0 |

### Utility

| Function | Description |
|---|---|
| `qc_ldexp(z, k)` | `z * 2^k` (exact, no rounding) |
| `qc_floor(z)` | component-wise floor |
| `qc_hypot(x, y)` | `sqrt(|x|² + |y|²)` |

### Extractors and Additional Utilities

| Function | Description |
|---|---|
| `qc_real(z)` → `qfloat_t` | real part |
| `qc_imag(z)` → `qfloat_t` | imaginary part |
| `qc_inv(z)` | multiplicative inverse `1/z` |

### Comparison and Predicates

| Function | Description |
|---|---|
| `qc_eq(a, b)` → `bool` | bitwise equality of both components |
| `qc_isnan(z)` → `bool` | true if either component is NaN |
| `qc_isinf(z)` → `bool` | true if either component is ±∞ |
| `qc_isposinf(z)` → `bool` | true if real part is +∞ and imaginary part is 0 |
| `qc_isneginf(z)` → `bool` | true if real part is −∞ and imaginary part is 0 |

### Formatting and Parsing

| Function | Description |
|---|---|
| `void qc_to_string(qcomplex_t z, char *out, size_t out_size);` | Writes a human-readable representation of `z` into `out`. |
| `qcomplex_t qc_from_string(const char *s);` | Parses a complex number from a string (e.g. `"3+4i"`, `"2e-5-1.2e3i"`, `"5i"`, `"7"`). |

### Printf-style Formatting

| Function | Description |
|---|---|
| `int qc_vsprintf(char *out, size_t out_size, const char *fmt, va_list ap);` | Internal printf-style formatter with full `qcomplex_t` and `qfloat_t` support. |
| `int qc_sprintf(char *out, size_t out_size, const char *fmt, ...);` | Printf-style formatter with full `qcomplex_t` and `qfloat_t` support. |
| `int qc_printf(const char *fmt, ...);` | Printf to stdout with full `qcomplex_t` and `qfloat_t` support. |

---

## Implementation Notes

- **Precision:** All arithmetic and elementary functions operate at full `qfloat_t` precision (~31–32 decimal digits, ~106 bits), both for real and complex arguments, unless otherwise noted.
- **Special functions:** For real arguments (`im == 0`), all special functions use the corresponding `qf_` implementation, preserving full precision. For complex arguments, algorithms are chosen to maximize accuracy and stability, but some special functions may have slightly reduced precision due to the complexity of analytic continuation or series evaluation in the complex plane.
- **Gamma and polygamma:** Implemented using high-precision algorithms (e.g., Lanczos, asymptotic expansions) to maintain as much precision as possible for both real and complex arguments.
- **Lambert W, incomplete gamma, exponential integrals:** Use iterative or series/continued-fraction methods adapted for complex arguments, with careful attention to branch cuts and principal values.
- **Parsing and formatting:** Parsing from string and printf-style formatting are supported for all complex numbers, with full control over decimal/scientific notation and alignment.
- **Fast paths:** For all functions, if the imaginary part is zero, the real-valued `qf_` implementation is used for maximum speed and accuracy.

If you need details about the implementation of a specific function or want to know about accuracy in a particular region of the complex plane, see the source code or contact the maintainers.

---

## Benchmark Coverage

`qcomplex_t` has a focused arithmetic and special-functions benchmark here:

- [bench/qcomplex/bench_qcomplex_maths.c](/home/rparry/Projects/MARS/bench/qcomplex/bench_qcomplex_maths.c)

Run it with:

```sh
make bench_qcomplex_maths
```

Sample results below were measured on:

- `Linux x86_64`
- kernel `6.8.0-110-generic`
- `Intel(R) Core(TM) i7-4510U CPU @ 2.00GHz`
- `4` logical CPUs

Current sample results:

| Case | Time |
|---|---:|
| `qc_exp(1+i)` | `3.582 µs` |
| `qc_log(1+i)` | `5.405 µs` |
| `qc_erf(0.5+0.5i)` | `10.404 µs` |
| `qc_erfc(0.5+0.5i)` | `9.357 µs` |
| `qc_gamma(1.5+0.7i)` | `14.554 µs` |
| `qc_lgamma(1.5+0.7i)` | `13.515 µs` |
| `qc_digamma(2+i)` | `14.643 µs` |
| `qc_trigamma(2+0.5i)` | `4.596 µs` |
| `qc_tetragamma(2+0.5i)` | `6.256 µs` |
| `qc_gammainv(3.323350970447842551184064031264648)` | `95.411 µs` |
| `qc_gammainv(qc_gamma(2.5+0.3i))` | `219.491 µs` |
| `qc_productlog(1+i)` | `30.014 µs` |
| `qc_lambert_wm1(-0.2-0.1i)` | `27.295 µs` |
| `qc_ei(1+i)` | `44.938 µs` |
| `qc_e1(1+i)` | `41.641 µs` |
| `qc_beta(1.5+0.5i, 2-0.3i)` | `38.087 µs` |
| `qc_logbeta(1.5+0.5i, 2-0.3i)` | `37.286 µs` |

For a broader benchmark overview, see
[`docs/benchmarks.md`](benchmarks.md).
