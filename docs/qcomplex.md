# `qcomplex_t`

`qcomplex_t` is a double-double complex number with `qfloat_t` real and imaginary parts, providing approximately 106 bits of precision (~31–32 decimal digits) across the full complex plane.

`qc_round(z)` rounds the real and imaginary components independently to their
nearest integers, with exact halves away from zero. This is component-wise
rounding, not rounding the magnitude; each component follows `qf_round`.

`qc_roundn(z, int n)` rounds each component to decimal places, and
`qc_rounds(z, unsigned int n)` rounds each component to significant decimal digits.
The latter rejects zero digits with NaN components. These follow the corresponding
qfloat functions, not the magnitude or phase of the complex number.

## Implementation organisation


The implementation groups Bessel J/I/K/Y, Lommel s and Struve H/L adapters in
`src/qcomplex/qcomplex_cylindrical.c` and polynomial and signal functions in
`qcomplex_special.c`. These groups preserve the existing conversions, branch
behaviour and numeric-scope cleanup. Core value operations, constants and
larger mathematical algorithms remain separate. Error-function adapters use
`qcomplex_error.c` and the checked native number-layer evaluator.

## Signal functions

`qc_sinc` is the entire complex continuation of normalised sinc, with value one
at zero. `qc_sgn`, `qc_step`, `qc_rect`, `qc_tri` and `qc_circ` follow the
[real signal definitions](expression.md#signal-functions-and-distributions)
and return NaN for non-real arguments; they are not applied separately to the
real and imaginary components.

`qc_sgn(z)` returns a purely real -1, zero or +1 according to the sign of its
real argument, including signed infinities. Either signed zero in the imaginary
component is accepted; NaN inputs remain NaN. This is real signum, not a complex
phase normalisation.

README signum example:

```c
#include <stdio.h>
#include "qcomplex-t.h"

int main(void)
{
    qcomplex_t sign = qc_sgn(qc_make(QF_NEG_ONE, QF_ZERO));
    printf("sgn(-1 + 0i) = %.0f + %.0fi\n", qf_to_double(qc_real(sign)), qf_to_double(qc_imag(sign)));
    return 0;
}
```

Output:

```text
sgn(-1 + 0i) = -1 + 0i
```

README example:

```c
#include <stdio.h>
#include "qcomplex-t.h"

int main(void)
{
    qcomplex_t value = qc_sinc(qc_make(QF_ZERO, QF_ZERO));
    printf("sinc(0) = %.0f + %.0fi\n", qf_to_double(qc_real(value)), qf_to_double(qc_imag(value)));
    return 0;
}
```

Output:

```text
sinc(0) = 1 + 0i
```

## Representation

```c
typedef struct {
    qfloat_t re;  /* real part */
    qfloat_t im;  /* imaginary part */
} qcomplex_t;
```

In day-to-day code, prefer the semantic helpers:

- `qc_real(z)` for the real component
- `qc_imag(z)` for the imaginary component

That keeps call sites aligned with the public API even though `qcomplex_t`
currently remains a transparent by-value struct.

## Capabilities

- ~106 bits of precision (~31–32 decimal digits)
- arithmetic: add, subtract, multiply, divide, negate, conjugate
- magnitude and argument
- elementary functions: exp, natural logarithm, common logarithm, pow, sqrt
- trigonometric, versine/haversine, and hyperbolic functions (and their inverses)
- special functions: erf, gamma, polygamma, beta, Lambert W, incomplete gamma,
  exponential integrals, polylogarithms, generalised hypergeometric pFq,
  Lauricella F_D, Appell F₁, Bessel J/Y/I/K, Struve H/L, Riemann and Hurwitz zeta, and the normal
  distribution
- polar form conversion
- parsing from string
- printf-style formatting and printing

## Constants

Common real constants are also available on the real axis, including
`QC_PI`, `QC_E`, `QC_LN2`, `QC_LN10`, `QC_SQRT2`, and `QC_LN_2PI`.

## Example

```c
#include <stdio.h>
#include "qcomplex-t.h"
#include "qfloat-t.h"

int main(void) {
    /* Euler's identity: exp(iπ) + 1 = 0 */
    qcomplex_t z   = qc_make(qf_from_double(0.0), QF_PI);
    qcomplex_t r   = qc_add(qc_exp(z), qc_make(qf_from_double(1.0), qf_from_double(0.0)));
    qfloat_t real  = qc_real(r);
    qfloat_t imag  = qc_imag(r);

    qc_printf("exp(iπ) + 1 = %.34z\n", r);
    qf_printf("real part    = %.34q\n", real);
    qf_printf("imag part    = %.34q\n", imag);
    return 0;
}
```

```text
exp(iπ) + 1 = 0
real part    = 0
imag part    = 0
```

---

## API Reference

All declarations are in `include/qcomplex-t.h`.

### Construction

- `qcomplex_t qc_make(qfloat_t re, qfloat_t im)` — construct from real and imaginary parts (inline).

### Component Access

- `qfloat_t qc_real(qcomplex_t z)` — return the real component.
- `qfloat_t qc_imag(qcomplex_t z)` — return the imaginary component.

### Basic Arithmetic

| Function | Description |
|---|---|
| `qc_add(a, b)` | `a + b` |
| `qc_sub(a, b)` | `a - b` |
| `qc_mul(a, b)` | `a * b` |
| `qc_div(a, b)` | `a / b` |
| `qc_neg(a)` | `-a` |
| `qc_conj(a)` | complex conjugate `re - i*im` |
| `qc_conjugate(a)` | long-form alias of `qc_conj(a)` |

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
| `qc_ln(z)` | shorthand for `qc_log(z)` |
| `qc_log10(z)` | principal common logarithm `lg(z)` |
| `qc_lg(z)` | shorthand for `qc_log10(z)` |
| `qc_pow(a, b)` | `a^b = exp(b * ln(a))` (principal branch) |
| `qc_sqrt(z)` | principal square root |
| `qc_cubrt(z)` | single principal cube root, equivalent to `qc_root(z, 3)` |
| `qc_root(z, n)` | single principal `n`-th root for an integer order greater than one |

`qc_sqrt`, `qc_cubrt`, and `qc_root` are single-valued. They use the principal logarithmic branch, with argument in
`(-π, π]`; they do not return the complete family of roots.

### Trigonometric Functions

| Function | Description |
|---|---|
| `qc_sin(z)` | sine |
| `qc_cos(z)` | cosine |
| `qc_tan(z)` | tangent |
| `qc_sec(z)` | secant `1/cos(z)` |
| `qc_cosec(z)` | cosecant `1/sin(z)` |
| `qc_cot(z)` | cotangent `1/tan(z)` |
| `qc_versin(z)` | versed sine `1 - cos(z)` |
| `qc_vercos(z)` | versed cosine `1 + cos(z)` |
| `qc_coversin(z)` | coversed sine `1 - sin(z)` |
| `qc_covercos(z)` | coversed cosine `1 + sin(z)` |
| `qc_haversin(z)` | haversine `(1 - cos(z)) / 2` |
| `qc_havercos(z)` | havercosine `(1 + cos(z)) / 2` |
| `qc_hacoversin(z)` | hacoversine `(1 - sin(z)) / 2` |
| `qc_hacovercos(z)` | hacovercosine `(1 + sin(z)) / 2` |
| `qc_asin(z)` | arcsine (principal branch) |
| `qc_acos(z)` | arccosine (principal branch) |
| `qc_atan(z)` | arctangent |
| `qc_asec(z)` | arcsecant (principal branch) |
| `qc_acosec(z)` | arccosecant (principal branch) |
| `qc_acot(z)` | arccotangent (principal branch) |
| `qc_arcversin(z)` | inverse versed sine `acos(1 - z)` |
| `qc_arcvercos(z)` | inverse versed cosine `acos(z - 1)` |
| `qc_arccoversin(z)` | inverse coversed sine `asin(1 - z)` |
| `qc_arccovercos(z)` | inverse coversed cosine `asin(z - 1)` |
| `qc_archaversin(z)` | inverse haversine `acos(1 - 2z)` |
| `qc_archavercos(z)` | inverse havercosine `acos(2z - 1)` |
| `qc_archacoversin(z)` | inverse hacoversine `asin(1 - 2z)` |
| `qc_archacovercos(z)` | inverse hacovercosine `asin(2z - 1)` |
| `qc_atan2(y, x)` | four-quadrant arctangent |

### Hyperbolic Functions

| Function | Description |
|---|---|
| `qc_sinh(z)` | hyperbolic sine |
| `qc_cosh(z)` | hyperbolic cosine |
| `qc_tanh(z)` | hyperbolic tangent |
| `qc_sech(z)` | hyperbolic secant `1/cosh(z)` |
| `qc_cosech(z)` | hyperbolic cosecant `1/sinh(z)` |
| `qc_coth(z)` | hyperbolic cotangent `1/tanh(z)` |
| `qc_asinh(z)` | inverse hyperbolic sine |
| `qc_acosh(z)` | inverse hyperbolic cosine (principal branch) |
| `qc_atanh(z)` | inverse hyperbolic tangent |
| `qc_asech(z)` | inverse hyperbolic secant (principal branch) |
| `qc_acosech(z)` | inverse hyperbolic cosecant (principal branch) |
| `qc_acoth(z)` | inverse hyperbolic cotangent (principal branch) |

Principal inverse functions follow the upper-side convention when an exactly
real input lies on a branch cut. In particular, `qc_asin(2)` is
`1.570796326794896619231321691639751 + 1.316957896924816708625046347307968i`,
while `qc_acos(2)` is
`-1.316957896924816708625046347307968i`.

### Special Functions

| Function | Description |
|---|---|
| `qc_erf(z)` | error function |
| `qc_erfc(z)` | complementary error function `1 - erf(z)` |
| `qc_erfinv(z)` | inverse error function |
| `qc_erfcinv(z)` | inverse complementary error function |
| `qc_bessel_J(order, z)` | principal Bessel function of the first kind |
| `qc_lommel_S(mu, nu, z)` | principal Lommel s function |
| `qc_gamma(z)` | Γ(z) |
| `qc_lgamma(z)` | ln Γ(z) |
| `qc_digamma(z)` | ψ(z) = Γ′(z)/Γ(z) |
| `qc_qdigamma(q, z)` | q-digamma ψ_q(z), defined by its principal complex series for `abs(q) < 1`, reciprocal-q continuation for `abs(q) > 1`, and ordinary digamma at `q = 1` |
| `qc_trigamma(z)` | ψ₁(z) = ψ′(z) |
| `qc_tetragamma(z)` | ψ₂(z) = −ψ^(2)(z) (positive, = +2Σ 1/(z+k)³) |
| `qc_polygamma(n, z)` | ψ⁽ⁿ⁾(z) |
| `qc_zeta(s)` | Riemann zeta ζ(s), analytically continued away from `s = 1` |
| `qc_zetap(s)` | derivative ζ′(s) with respect to `s` |
| `qc_zetah(s, a)` | Hurwitz zeta ζ(s, a) |
| `qc_zatahp(s, a)` | partial derivative ∂ζ(s, a)/∂s |
| `qc_gammainv(z)` | inverse of the gamma function (principal branch, Re z ≥ 1.46) |
| `qc_beta(a, b)` | B(a,b) = Γ(a)Γ(b)/Γ(a+b) |
| `qc_logbeta(a, b)` | ln B(a,b) |
| `qc_binomial(a, b)` | generalised binomial coefficient Γ(a+1)/(Γ(b+1)Γ(a−b+1)) |
| `qc_beta_pdf(x, a, b)` | beta distribution PDF |
| `qc_logbeta_pdf(x, a, b)` | log beta distribution PDF |
| `qc_normal_pdf(z)` | standard normal PDF φ(z) |
| `qc_normal_cdf(z)` | standard normal CDF Φ(z) |
| `qc_normal_logpdf(z)` | log standard normal PDF |
| `qc_lambert_Wm1(z)` | Lambert W branch W₋₁(z) |
| `qc_productlog(z)` | principal Lambert W function W₀(z): W·e^W = z |
| `qc_gammainc_lower(s, x)` | lower incomplete gamma γ(s,x) |
| `qc_gammainc_upper(s, x)` | upper incomplete gamma Γ(s,x) |
| `qc_gammainc_P(s, x)` | regularised lower incomplete gamma P(s,x) = γ(s,x)/Γ(s) |
| `qc_gammainc_Q(s, x)` | regularised upper incomplete gamma Q(s,x) = Γ(s,x)/Γ(s) |
| `qc_Ei(z)` | exponential integral Ei(z) |
| `qc_Li(z)` | logarithmic integral Li(z) = Ei(log(z)), using the principal branch |
| `qc_E1(z)` | exponential integral E₁(z); satisfies E₁(z) = −Ei(−z) for real z > 0 |
| `qc_dilog(z)` | principal dilogarithm Li₂(z) |
| `qc_clausen2(z)` | order-two Clausen function, with a real-axis bridge |
| `qc_clausen(order, z)` | positive integer-order Clausen continuation, analytic between its vertical branch cuts |
| `qc_polylog1(z)` | order-one polylogarithm Li₁(z) = −Log(1−z) on the principal branch |
| `qc_polylog(s, z)` | polylogarithm Li_s(z) for integer real orders currently supported by the implementation |
| `qc_lerch_phi(z, s, a)` | Lerch transcendent Φ(z,s,a) in the defining disc `abs(z) < 1`, with exact reductions at z = 0, z = 1 and s = 0 |
| `qc_harmonic_poly(degree, z)` | finite harmonic polynomial Hₙ(z) = Σₖ₌₁ⁿ zᵏ/k for an `unsigned long` degree |
| `qc_legendre_chi(s, z)` | Legendre chi χ_s(z) for integer real orders currently supported by the implementation |
| `qc_hypergeometric_pFq(upper, upper_count, lower, lower_count, argument)` | generalised hypergeometric pFq; either parameter array may be `NULL` when its count is zero |
| `qc_lauricella_F(a, b, c, x, variable_count)` | Lauricella F_D in `variable_count` variables within the implemented convergence polydisc |
| `qc_appell_F1(a, b1, b2, c, x, y)` | Appell F₁, implemented as the two-variable Lauricella F_D member |

The Riemann and Hurwitz zeta Euler–Maclaurin paths reuse successive rising
products and endpoint powers, and omit derivative work for value-only calls.
The truncation and branch conventions are unchanged; real arguments still
use the real implementation where applicable.

#### Clausen functions

`qc_clausen(order, z)` takes a positive integer order and an angle in radians.
`qc_clausen2(z)` is exactly `qc_clausen(2, z)`. The C API requires the order
in `qc_clausen`; MARS Lab's one-argument `clausen(z)` instead aliases
`clausen2(z)`. Use `Cl(n,z)` for an explicit order in
[expression input](expression.md#clausen-functions).

Real arguments use the corresponding real, periodic `qf_clausen` function.
For complex arguments, even orders use
`(Li_order(exp(i*z)) - Li_order(exp(-i*z)))/(2*i)` and odd orders use the
half-sum, with principal polylogarithms. These are analytic continuations,
not the imaginary or real part of a single polylogarithm at an arbitrary
complex argument.

The continuation is holomorphic inside each strip
`2*k*pi < Re(z) < 2*(k+1)*pi`; nonreal points on the strip boundaries are
excluded and return NaN. In the central strip, order one is
`-log(2*sin(z/2))`. For orders `n >= 2`, differentiation with respect to the
argument gives `(-1)^n*Cl_(n-1)(z)`. Order zero and non-finite inputs return
NaN. `qc_clausen2` uses the same branch and domain for order two.

### Bessel Y

`qc_bessel_Y(order, argument)` evaluates the principal Bessel function of
the second kind for real or complex orders and arguments, including Y₀.
It uses the [native arbitrary-precision evaluator](number.md#bessel-y),
then rounds the real and imaginary components to qfloat precision.
The branch is `-pi < arg(z) <= pi`; an exactly negative real argument
takes the upper-bank value, including a negative imaginary zero.
Values approaching the cut from below retain their lower-bank limit.
Negative integer orders obey `Y_-n(z) = (-1)^n*Y_n(z)`.

Negative half-integer orders return zero at the origin; other zero limits,
including Y₀, return NaN. Order magnitudes must be at most 1000.
Non-integer orders or genuinely complex arguments also require argument
magnitude at most 1000. Larger real arguments are supported for integer
orders. Non-finite inputs and exhausted numerical guards return NaN.
The shared evaluator limits work precision to 65536 bits and series to
20000 terms; unscaled output remains subject to qfloat's exponent range.

```c
#include <stdio.h>
#include "qcomplex-t.h"

int main(void)
{
    qcomplex_t value = qc_bessel_Y(QC_ZERO, QC_ONE);
    printf("Y_0(1) = %.15f + %.0fi\n", qf_to_double(qc_real(value)), qf_to_double(qc_imag(value)));
    return 0;
}
```

```text
Y_0(1) = 0.088256964215677 + 0i
```

### Modified Bessel K

`qc_bessel_K(order, argument)` evaluates the principal modified Bessel function
of the second kind for finite real or complex operands and a non-zero argument.
Both operand magnitudes must be at most 1000. A zero argument, non-finite input,
unsupported magnitude or exhausted numerical guard produces a NaN result.

The wrapper calls [the number-layer evaluator](number.md#modified-bessel-k)
and rounds its real and imaginary components to qfloat precision. Complex branch
values are preserved rather than routing every real argument through the
positive-argument-only qfloat wrapper. The public result remains `qcomplex_t`;
the working multiprecision representation is internal to the number layer.
Integer-order evaluations reuse MPC scratch values throughout the harmonic
series and recurrence before rounding the completed value to double-double
components.

### Modified Bessel I

`qc_bessel_I(order, argument)` evaluates the principal modified Bessel
function of the first kind for complex orders and arguments. Order zero
selects `I_0(z)`. The leading power `(z/2)^order` uses
`-pi < arg(z) <= pi`; an exactly negative real argument takes the upper-bank
value, including negative imaginary zero. Approaching from below gives the
lower-bank limit. Integral orders have no cut and satisfy `I_-n(z) = I_n(z)`.
See [DLMF 10.25.2](https://dlmf.nist.gov/10.25.E2).

At zero, order zero gives one, `Re(order) > 0` and negative integer orders
give zero, and other orders return NaN, including non-zero imaginary orders.
The [number-layer kernel](number.md#modified-bessel-i) preserves complex
coefficient precision before rounding to qcomplex. It shares its gamma and
series machinery with Struve L and accepts input magnitudes at most 1000.
Work above 65536 bits, series beyond index 20000, non-finite inputs,
undefined zero limits and exhausted numerical guards return NaN. Results
are unscaled and conversion remains subject to qfloat's exponent range;
there is no asymptotic large-argument implementation.

```c
#include <stdio.h>
#include "qcomplex-t.h"

int main(void)
{
    qcomplex_t bessel = qc_bessel_I(QC_ZERO, QC_ONE);
    qc_printf("I_0(1) = %.16z\n", bessel);
    return 0;
}
```

```text
I_0(1) = 1.2660658777520083
```

### Ordinary Struve H

`qc_struve_H(order, argument)` accepts finite complex orders and arguments
and returns the unscaled principal ordinary Struve function. The leading
power `(z/2)^(order+1)` uses `-pi < arg(z) <= pi`. Exactly negative real
arguments use the upper bank, even with negative imaginary zero; approaches
from below give the lower-bank limit. Integral orders have no branch cut.

The [number-layer kernel](number.md#ordinary-struve-h) evaluates the
alternating series in [DLMF 11.2.1](https://dlmf.nist.gov/11.2.E1) directly,
without rotating an argument across a power branch. Orders `-3/2, -5/2, ...`
retain their non-zero tail after reciprocal-gamma zeros. At zero, the result
is zero for `Re(order) > -1` and these exceptional half-integers; real order
`-1` gives `2/pi`. Other zero limits, including oscillatory complex limits,
return NaN.

Both input magnitudes must be at most 1000. Work above 65536 bits, series
beyond index 20000, non-finite inputs, unresolved zeros, exhausted numerical
guards and number-layer exponent overflow/underflow return NaN. Results
are rounded to complex double-double and remain subject to qfloat's exponent
range. No scaled or asymptotic kernel is provided, and not every point
inside the input bounds is guaranteed to pass the numerical guards.

```c
#include <stdio.h>
#include "qcomplex-t.h"

int main(void)
{
    qcomplex_t ordinary = qc_struve_H(QC_ZERO, QC_ONE);
    qc_printf("H_0(1) = %.15z\n", ordinary);
    return 0;
}
```

```text
H_0(1) = 0.568656627048288
```

### Modified Struve L

`qc_struve_L(order, argument)` accepts finite complex orders and arguments.
It evaluates the principal value, with the power `(z/2)^(order+1)` defined
using `-pi < arg(z) <= pi`. An exactly negative real argument takes the upper
bank, including an imaginary component of negative zero; approaching from
below gives the lower-bank limit. Integral orders have no branch cut.
This convention follows [DLMF 11.2.2](https://dlmf.nist.gov/11.2.E2).

The [number-layer implementation](number.md#modified-struve-l) supplies a
guarded multiprecision result before conversion to complex double-double.
Orders `-3/2, -5/2, ...` include the non-zero series tail after the initial
reciprocal-gamma zeros. At zero the result is zero when `Re(order) > -1` or
the order is one of these negative half-integers; real order `-1` gives `2/pi`.
All remaining zero limits, including `Re(order) = -1` with non-zero imaginary
part, return NaN.

Both input magnitudes must be at most 1000. Non-finite inputs, exhausted
convergence or cancellation guards, and number-layer exponent overflow or
underflow return NaN. Conversion of the unscaled result to qcomplex remains
subject to qfloat's exponent range. No large-argument asymptotic kernel is
provided. See the number guide for the work-precision and iteration limits.

```c
#include <stdio.h>
#include "qcomplex-t.h"

int main(void)
{
    qcomplex_t value = qc_struve_L(QC_ZERO, QC_ONE);
    qc_printf("L_0(1) = %.16z\n", value);
    return 0;
}
```

```text
L_0(1) = 0.7102431859378909
```

Complex erf/erfc and their inverses use the guarded number-layer evaluator and
round back to double-double components. Real inputs retain the direct qfloat
path. The complex forward evaluator supports `|z| <= 140`, subject to its
65536-bit work limit. Inverses use checked Halley refinement with increasing
working precision and reject an
unresolved forward residual. Failed guards return `QC_NAN`.

`qc_bessel_J` and `qc_lommel_S` also round guarded number-layer series. Their
complex argument magnitudes are limited to 1000, with a 65536-bit work limit.
Principal powers use the upper bank on the negative real axis. Singular
parameters, undefined zero limits and exhausted guards return `QC_NAN`.

The normal CDF uses `erfc(-z/sqrt(2))/2`, including the real qfloat path, to
retain small negative-tail probabilities. Generic `number_t` calls to the
Gaussian PDF, log PDF and CDF use native MPC when complex multiprecision is
requested rather than rounding through this fixed-precision interface.

### Utility

| Function | Description |
|---|---|
| `qc_ldexp(z, k)` | `z * 2^k` (exact, no rounding) |
| `qc_floor(z)` | component-wise floor |
| `qc_hypot(x, y)` | `sqrt(|x|² + |y|²)` |

### Additional Utilities

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
| `string_t *qc_to_text(qcomplex_t z);` | Returns a newly allocated human-readable representation of `z`; release it with `string_free()`. |
| `string_t *qc_to_string(qcomplex_t z);` | Convenience alias for `qc_to_text(...)`. |
| `qcomplex_t qc_from_string(const char *s);` | Parses a complex number from a string (e.g. `"3+4i"`, `"2e-5-1.2e3i"`, `"5i"`, `"7"`). |

### Printf-style Formatting

| Function | Description |
|---|---|
| `string_t *qc_vsprintf_text(const char *fmt, va_list ap);` | String-owning formatter with full `qcomplex_t` and `qfloat_t` support. |
| `string_t *qc_sprintf_text(const char *fmt, ...);` | String-owning formatter with full `qcomplex_t` and `qfloat_t` support. |
| `int qc_vsprintf(char *out, size_t out_size, const char *fmt, va_list ap);` | Compatibility formatter for caller-supplied C buffers. |
| `int qc_sprintf(char *out, size_t out_size, const char *fmt, ...);` | Printf-style formatter with full `qcomplex_t` and `qfloat_t` support. |
| `int qc_printf(const char *fmt, ...);` | Printf to stdout with full `qcomplex_t` and `qfloat_t` support. |

---

### `qc_from_text()`

Creates or reconstructs the public value described by from text.

```c
qcomplex_t qc_from_text(const string_t *text);
```

### `qc_lambert_Wn()`

Returns the public result described by lambert wn.

```c
qcomplex_t qc_lambert_Wn(int branch, qcomplex_t z);
```

## Implementation Notes

- **Precision:** All arithmetic and elementary functions operate at full `qfloat_t` precision (~31–32 decimal digits, ~106 bits), both for real and complex arguments, unless otherwise noted.
- **Special functions:** Real-valued fast paths are used where the function's domain and branch permit them. Real arguments can still require complex results. Bessel J/Y/I/K, Lommel s and Struve H/L use guarded number-layer evaluators before rounding to double-double components. Complex error functions and their inverses use the same native precision and convergence policy. Numerical limits are documented with each family.
- **Gamma and polygamma:** Implemented using high-precision algorithms (e.g., Lanczos, asymptotic expansions) to maintain as much precision as possible for both real and complex arguments.
- **Lambert W, incomplete gamma, exponential integrals, polylogarithms and the hypergeometric families:** Use iterative or dynamically converged series/continued-fraction methods adapted for complex arguments, with careful attention to branch cuts and principal values. Appell F₁ delegates to the two-variable Lauricella F_D implementation.
- **Parsing and formatting:** Parsing from string and printf-style formatting are supported for all complex numbers, with full control over decimal/scientific notation and alignment.
- **Numeric boundary:** Storage and core arithmetic use `qfloat_t`. The qcomplex module has no direct MPFR/MPC includes or calls, but the delegated special functions depend indirectly on those libraries through `number_t`. Multiprecision work stays behind the number module's public API; it does not change the public qcomplex representation.

If you need details about the implementation of a specific function or want to know about accuracy in a particular region of the complex plane, see the source code or contact the maintainers.

---

## Benchmark Coverage

`qcomplex_t` has a focused arithmetic and special-functions benchmark here:

- [bench/qcomplex/bench_qcomplex_maths.c](/home/rparry/Projects/MARS/bench/qcomplex/bench_qcomplex_maths.c)

Run it with:

```sh
make bench_qcomplex_maths
```

Persistent timings are now recorded by `make bench-db ARGS='run'` and published
with `make bench-db ARGS='update-docs'`. These tables identify the exact run,
machine, backend and precision; the focused benchmark above remains available
for separate investigations. See [the benchmark guide](benchmarks.md#persistent-mathematical-benchmarks)
for selection, interruption recovery and raw sample storage.

<!-- MARS-BENCHMARKS:BEGIN -->
Benchmark run **31**, 2026-11-12T01:25:53.508Z (UTC).

Machine: Intel(R) N95.

System: Linux 7.0.0-27-generic x86_64.

Compiler: 15.2.0; build flags: gcc -Wall -Wextra -Werror -O2 -fPIC -D_GNU_SOURCE -pthread -DHAVE_UNISTRING -I/usr/include/sqlcipher -DSQLITE_HAS_CODEC   -I/usr/include/x86_64-linux-gnu -isystem /usr/include/mit-krb5 -isystem /usr/include/mit-krb5 -I/usr/include/p11-kit-1.

Median per call from 7 timed batches.

Timing method: CLOCK_MONOTONIC; warmup=100ms; target_batch=5ms; max_batch=1048576; storage_chunk=512.

Argument parsing, output validation, disposal and database writes are outside timing.
Inputs show the actual operands, including +0.125i for permitted complex arguments; integer orders remain real.
`number-double` includes generic number dispatch; the other fixed profiles call qfloat/qcomplex directly.
Requested multiprecision bits do not guarantee that every algorithm achieves that accuracy.
Operations and precision columns with only unsupported results are omitted; — denotes an unsupported cell.
Failures and timeouts remain visible. † marks a retained slowdown outlier.

### `qcomplex`

| Operation | Inputs | 106 bits |
|---|---|---:|
| E1 | 2.345678+0.125i | 67.1 µs |
| Ei | 2.345678+0.125i | 80.8 µs † |
| Li | 2.345+0.125i | 56.6 µs |
| abs | 0.7+0.125i | 597.8 ns |
| acos | 0.7+0.125i | 11.4 µs |
| acosec | 2.345+0.125i | 11.3 µs |
| acosech | 0.7+0.125i | 10.5 µs |
| acosh | 2.345+0.125i | 13.7 µs |
| acot | 0.7+0.125i | 13.4 µs |
| acoth | 2.345+0.125i | 5.6 µs |
| add | 0.7+0.125i, 0.7+0.125i | 18.3 ns |
| appell_F1 | <span style="display:inline-block;width:40ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">0.5+0.125i, 0.7+0.125i, 0.8+0.125i, 2.5+<br/>0.125i, 0.2+0.125i, 0.3+0.125i</span> | 693.3 µs |
| arccovercos | 0.5+0.125i | 9.7 µs |
| arccoversin | 0.5+0.125i | 9.5 µs |
| archacovercos | 0.5+0.125i | 6.6 µs |
| archacoversin | 0.5+0.125i | 6.7 µs |
| archavercos | 0.5+0.125i | 5.9 µs |
| archaversin | 0.5+0.125i | 9.5 µs † |
| arcvercos | 0.5+0.125i | 13.7 µs † |
| arcversin | 0.5+0.125i | 11.1 µs |
| arg | 0.7+0.125i | 1.5 µs |
| asec | 2.345+0.125i | 11.0 µs |
| asech | 0.7+0.125i | 14.4 µs |
| asin | 0.7+0.125i | 10.8 µs |
| asinh | 0.7+0.125i | 10.9 µs |
| atan | 0.7+0.125i | 12.7 µs |
| atan2 | 0.5+0.125i, -0.75+0.125i | 12.6 µs |
| atanh | 0.7+0.125i | 6.7 µs |
| bessel_I | 2, 0.7+0.125i | 112.2 µs |
| bessel_J | 2, 0.7+0.125i | 130.7 µs |
| bessel_K | 2, 0.7+0.125i | 86.3 µs |
| bessel_Y | 2, 0.7+0.125i | 107.1 µs |
| beta | 2.3+0.125i, 4.5+0.125i | 41.2 µs |
| beta_pdf | 0.5+0.125i, 2.5+0.125i, 3.5+0.125i | 60.9 µs |
| binomial | 5+0.125i, 2+0.125i | 33.6 µs |
| chebyshev_Tn | 2, 0.7+0.125i | 194.3 ns |
| chebyshev_Un | 2, 0.7+0.125i | 188.5 ns |
| circ | 0.7 | 20.3 ns |
| clausen | 3, 0.7+0.125i | 410.1 µs |
| clausen2 | 0.7+0.125i | 28.8 µs |
| conj | 0.7+0.125i | 4.0 ns |
| cos | 0.7+0.125i | 11.1 µs |
| cosec | 0.7+0.125i | 9.6 µs |
| cosech | 0.7+0.125i | 9.8 µs |
| cosh | 0.7+0.125i | 9.3 µs |
| cot | 0.7+0.125i | 21.3 µs |
| coth | 0.7+0.125i | 19.4 µs |
| covercos | 0.7+0.125i | 9.5 µs |
| coversin | 0.7+0.125i | 11.6 µs |
| cubrt | 0.7+0.125i | 8.8 µs |
| digamma | 2.345+0.125i | 16.0 µs |
| dilog | 0.7+0.125i | 114.1 µs |
| div | 0.7+0.125i, 0.7+0.125i | 174.9 ns |
| erf | 0.7+0.125i | 61.6 µs |
| erfc | 0.7+0.125i | 59.1 µs |
| erfcinv | 0.5+0.125i | 378.9 µs |
| erfinv | 0.5+0.125i | 411.9 µs |
| exp | 1.23456789+0.125i | 3.9 µs |
| floor | 0.7+0.125i | 60.1 ns |
| gamma | 2.345+0.125i | 16.1 µs |
| gammainc_P | 2.3+0.125i, 1.4+0.125i | 81.0 µs |
| gammainc_Q | 2.3+0.125i, 1.4+0.125i | 79.9 µs |
| gammainc_lower | 2.3+0.125i, 1.4+0.125i | 54.2 µs |
| gammainc_upper | 2.3+0.125i, 1.4+0.125i | 73.7 µs |
| gammainv | 9.5+0.125i | 142.3 µs |
| hacovercos | 0.7+0.125i | 9.5 µs |
| hacoversin | 0.7+0.125i | 10.6 µs |
| harmonic_poly | 2, 0.7+0.125i | 665.6 ns |
| havercos | 0.7+0.125i | 10.7 µs |
| haversin | 0.7+0.125i | 12.5 µs † |
| hermite_Hn | 2, 0.7+0.125i | 224.0 ns |
| hypergeometric_pFq | 0.7+0.125i, 2.5+0.125i, 0.3+0.125i | 37.8 µs |
| hypot | 0.7+0.125i, 1.2+0.125i | 1.8 µs |
| imag_part | 0.7+0.125i | 3.3 ns |
| inv | 0.7+0.125i | 199.8 ns |
| lambert_W0 | 0.7+0.125i | 36.0 µs |
| lambert_Wm1 | -0.2+0.125i | 47.6 µs † |
| lambert_Wn | 1, 0.7+0.125i | 36.4 µs |
| lauricella_F | 0.5+0.125i, 0.3+0.125i, 2.5+0.125i | 205.6 µs |
| ldexp | 1.23456789+0.125i, 3 | 32.8 ns |
| legendre_chi | 2, 0.7+0.125i | 401.6 µs |
| lerch_phi | 0.3+0.125i, 2+0.125i, 1.4+0.125i | 814.5 µs |
| lg | 2.345678+0.125i | 4.7 µs |
| lgamma | 2.345+0.125i | 12.4 µs |
| ln | 2.345678+0.125i | 5.1 µs |
| log | 2.345678+0.125i | 5.5 µs |
| log10 | 2.345678+0.125i | 5.8 µs |
| logbeta | 2.3+0.125i, 4.5+0.125i | 34.4 µs |
| logbeta_pdf | 0.5+0.125i, 2.5+0.125i, 3.5+0.125i | 48.2 µs |
| lommel_S | 0.5+0.125i, 0.7+0.125i, 1.2+0.125i | 143.7 µs |
| mul | 0.7+0.125i, 0.7+0.125i | 61.3 ns |
| neg | 0.7+0.125i | 7.5 ns |
| normal_cdf | 0.7+0.125i | 46.6 µs |
| normal_logpdf | 0.7+0.125i | 175.0 ns |
| normal_pdf | 0.7+0.125i | 4.0 µs |
| polygamma | 3, 2.345+0.125i | 14.7 µs |
| polylog | 2, 0.7+0.125i | 112.4 µs |
| polylog1 | 0.7+0.125i | 8.4 µs † |
| pow | 1.23456789+0.125i, 3.5+0.125i | 12.1 µs † |
| productlog | 0.7+0.125i | 36.7 µs |
| qdigamma | 0.5+0.125i, 2.3+0.125i | 178.6 µs |
| real_part | 0.7+0.125i | 2.9 ns |
| rect | 0.7 | 26.3 ns |
| root | 1.2345+0.125i, 3 | 9.2 µs |
| round | 0.7+0.125i | 79.5 ns |
| roundn | 1.23456789+0.125i, 3 | 11.3 µs |
| rounds | 1.23456789+0.125i, 3 | 36.5 µs |
| sec | 0.7+0.125i | 10.2 µs |
| sech | 0.7+0.125i | 10.2 µs |
| sgn | 0.7 | 11.8 ns |
| sin | 0.567+0.125i | 9.2 µs |
| sinc | 0.7+0.125i | 9.5 µs |
| sinh | 0.7+0.125i | 11.8 µs † |
| sqr | 0.7+0.125i | 57.5 ns |
| sqrt | 1.23456789+0.125i | 4.1 µs |
| step | 0.7 | 14.8 ns |
| struve_H | 2, 0.7+0.125i | 175.6 µs |
| struve_L | 2, 0.7+0.125i | 123.2 µs |
| sub | 0.7+0.125i, 0.7+0.125i | 18.1 ns |
| tan | 0.7+0.125i | 22.4 µs |
| tanh | 0.7+0.125i | 18.7 µs |
| tetragamma | 2.345+0.125i | 4.6 µs |
| tri | 0.7 | 19.1 ns |
| trigamma | 2.345+0.125i | 3.5 µs |
| vercos | 0.7+0.125i | 9.0 µs |
| versin | 0.7+0.125i | 10.3 µs |
| zatahp | 2.3+0.125i, 1.4+0.125i | 571.8 µs |
| zeta | 2.345+0.125i | 411.7 µs |
| zetah | 2.3+0.125i, 1.4+0.125i | 683.5 µs |
| zetap | 2.345+0.125i | 478.0 µs |

<!-- MARS-BENCHMARKS:END -->

For a broader benchmark overview, see
[`docs/benchmarks.md`](./benchmarks.md).


## Chebyshev and Hermite polynomials

| API | Family |
| :--- | :--- |
| `qc_chebyshev_Tn` | First-kind Chebyshev, $T_0=1$, $T_1=x$ |
| `qc_chebyshev_Un` | Second-kind Chebyshev, $U_0=1$, $U_1=2x$ |
| `qc_hermite_Hn` | Physicists' Hermite, $H_0=1$, $H_1=2x$ |

These APIs take an unsigned long degree followed by the argument and evaluate
the three-term polynomial recurrence.
The existing harmonic-polynomial API is unchanged. See the
[expression naming table](expression.md#chebyshev-and-hermite-polynomials)
for `Tn`, `Un` and the distinct script-H Hermite notation.

README example:

```c
#include <stdio.h>
#include "qcomplex-t.h"

int main(void)
{
    qcomplex_t value = qc_hermite_Hn(3, qc_make(QF_ZERO, QF_ONE));
    qc_printf("Hermite H3(i) = %.0z\n", value);
    return 0;
}
```

Output:

```text
Hermite H3(i) = -20i
```

<style data-mars-formula-preview>
/* Formula placement belongs to the preview container, not to the alignment
 * of columns inside a TeX array. Apply this to every Markdown guide. */
body .katex-display {
    margin: 1em 0;
    padding-left: 1em;
    text-align: left;
    overflow-x: auto;
    overflow-y: hidden;
}

body .katex-display > .katex {
    text-align: left;
    white-space: nowrap;
}

/* Standalone inline formulae already contain their small TeX indentation.
 * Do not add padding here or prevent ordinary prose from wrapping. */
body p:has(> .katex) {
    text-align: left;
}
</style>
