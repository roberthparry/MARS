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

## Example

```c
#include <stdio.h>
#include "qcomplex.h"
#include "qfloat.h"

int main(void) {
    /* Euler's identity: exp(iπ) + 1 = 0 */
    qcomplex_t z   = qc_make(qf_from_double(0.0), QF_PI);
    qcomplex_t r   = qc_add(qc_exp(z), qc_make(qf_from_double(1.0), qf_from_double(0.0)));

    char buf[256];
    qc_to_string(r, buf, sizeof(buf));
    printf("exp(iπ) + 1 = %s\n", buf);
    return 0;
}
```

```text
exp(iπ) + 1 = 0
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

### Comparison and Predicates

| Function | Description |
|---|---|
| `qc_eq(a, b)` → `bool` | bitwise equality of both components |
| `qc_isnan(z)` → `bool` | true if either component is NaN |
| `qc_isinf(z)` → `bool` | true if either component is ±∞ |
| `qc_isposinf(z)` → `bool` | true if real part is +∞ and imaginary part is 0 |
| `qc_isneginf(z)` → `bool` | true if real part is −∞ and imaginary part is 0 |

### Formatting

```c
void qc_to_string(qcomplex_t z, char *out, size_t out_size);
```

Writes a human-readable representation of `z` into `out`.

---

## Implementation Notes

**Real fast paths** — `qc_erf` and `qc_erfc` detect `im == 0` and delegate to `qf_erf` / `qf_erfc`, which give full 30-digit accuracy. The complex path uses a Faddeeva / Weideman rational approximation that tops out near 15 significant digits.

**Gamma** — implemented via Lanczos g=7, which provides ~15 significant digits for complex arguments. Real arguments with `im == 0` use the `qf_gamma` fast path at full qfloat_t precision.

**Digamma and polygamma** — use the asymptotic Stirling–Bernoulli series after argument shifting via the recurrence relation ψ(z+1) = ψ(z) + 1/z.  The tetragamma `qc_tetragamma` returns −ψ^(2)(z), a positive quantity satisfying ψ₂(z+1) = ψ₂(z) − 2/z³.

**Lambert W** — uses Halley's method initialised from a log approximation; converges in a few iterations for |z| > 1, switches to a series near z = 0.

**Incomplete gamma / exponential integrals** — use continued-fraction or power-series representations selected by region, with the same techniques as the real `qf_` counterparts.
