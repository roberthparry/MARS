# `number_t`

`number_t` is MARS's generic numeric value cluster.

## Implementation organisation

Small related function families are grouped in two implementation units:

- `src/number/number_cylindrical_series.c` contains Bessel I and Struve H/L,
  their shared guarded series and cylindrical precision selection. Bessel K
  and Y retain separate algorithm files. The series-family selector and
  logarithmic-gamma kernel are private to the shared series implementation.
- `src/number/number_special.c` contains Chebyshev and Hermite recurrences
  together with the scalar signal functions. Both groups use the existing
  generic numeric arithmetic and precision policy.

Scoped allocation declarations live in `src/number/number_internal.h`.
Applications continue to use `number-t.h`; this organisation does not change
the public API, numerical algorithms or ownership rules.

## Signal functions

The signal APIs `num_sgn`, `num_step`, `num_rect`, `num_tri`, `num_circ` and `num_sinc`
follow the [expression-level definitions](expression.md#signal-functions-and-distributions),
using the existing numeric precision policy. Piecewise functions require real
arguments; sinc has an entire complex continuation and an exact removable value
of one at zero. Distributional impulses and principal values are not numeric APIs.

`num_sgn(x)` compares real inputs at their existing precision and returns an
owning exact integer -1, zero or +1. Either signed zero gives zero, and signed
infinities give their signs. Purely real complex values are accepted; NaN and
non-real inputs return NaN. The input is unchanged.

README signum example:

```c
#include <stdio.h>
#include "number-t.h"

int main(void)
{
    NUM_SCOPE(scope);
    number_t sign = num_sgn(num_create_from_frac(-2, 3));
    printf("sgn(-2/3) = %.0f\n", num_to_double(sign));
    return 0;
}
```

Output:

```text
sgn(-2/3) = -1
```

README example (the scope owns temporary numbers):

```c
#include <stdio.h>
#include "number-t.h"

int main(void)
{
    NUM_SCOPE(scope);
    number_t edge = num_rect(NUM_HALF);
    printf("rect(1/2) = %.1f\n", num_to_double(edge));
    return 0;
}
```

Output:

```text
rect(1/2) = 0.5
```

It gives the library a single by-value public numeric handle that can represent:

- exact integers via an internal MPZ-backed representation
- exact rationals via an internal MPQ-backed representation
- fixed-precision real values via `double` and `qfloat_t`
- fixed-precision complex values via `double _Complex` and `qcomplex_t`
- multiprecision real values via an internal MPFR-backed representation
- multiprecision complex values via an internal `complex_t` with MPC-backed
  acceleration for inexact complex operations

The goal is to let callers work at the level of "a number" without having to
manually pick a backend for every operation.

## Process Shutdown

Most `number_t` values are ordinary owning values and should be released with
`num_destroy(...)` when the caller is finished with them. Built-in constants
such as `NUM_ZERO`, `NUM_HALF`, `NUM_PI`, `NUM_INF`, `NUM_I`, and their related
tables are intentionally immortal during normal execution so shared constants
cannot be cleared accidentally by ordinary value destruction.

At process shutdown, the number layer now runs an automatic destructor that:

- clears the Bernoulli cache used by higher special functions
- clears any initialised GMP/MPFR backing storage owned by immortal `NUM_*`
  constants
- clears MPC cache state attached to the immortal imaginary constants
- calls `mpfr_free_cache()` to release MPFR's process-local cache allocations

This keeps normal ownership simple while allowing Valgrind to report that the
test process exits with no reachable heap blocks left behind.

## Public Representation

`number_t` is a fixed-size public storage envelope:

```c
typedef struct _number_t {
    uint64_t storage[5];
} number_t;
```

Callers may:

- pass it by value
- return it by value
- store it on the stack

but must treat the storage as opaque.

## Parsing Policy

`num_create_from_string(...)` chooses the most suitable backend by syntax:

- integer text -> exact integer
- `a/b` fraction text, Unicode fraction glyphs, or stacked Unicode fractions
  -> exact rational
- decimal or scientific real text -> multiprecision real
- complex text -> internal complex representation, preserving exact real and
  imaginary parts where possible

Examples:

- `"42"` -> exact integer
- `"5/6"` -> exact rational
- `"⅚"` -> exact rational
- `"³⁵⁵⁄₁₁₃"` -> exact rational
- `"32.123"` -> multiprecision real
- `"1 + 2i"` -> multiprecision complex

Accepted examples:

- `"1 + i"` -> parsed as `1 + i`
- `"1 - i"` -> parsed as `1 - i`
- `"1e-23 + 2.3e12i"` -> scientific notation in both parts is accepted
- `"1e-23 + (2.3e12)i"` -> parenthesised imaginary coefficients are accepted
- `"1/2 - 3/2i"` -> rational real and imaginary parts are accepted
- `"3i"` -> pure imaginary form is accepted

Not currently accepted:

- `"1/i"` -> algebraic expression syntax

Return convention:

- on success, returns a live `number_t` by value
- on parse failure, returns an invalid `number_t`

## Precision Model

For multiprecision construction, `number_t` uses a default working precision of
`1024` bits unless the caller requests something else explicitly.

This applies to:

- `num_create_from_string(...)` for decimal/scientific real input
- inexact components of `num_create_from_string(...)` complex input
- `num_new(...)`
- `num_const(...)` when materialising inexact constants such as `NUM_PI`
- promotion from exact values into multiprecision real or complex work

Fixed-precision constructors deliberately keep their fixed precision:

- `num_create_from_double(...)` creates a double-precision value
- `num_create_from_cdouble(...)` creates a double-complex value
- `num_create_from_qfloat(...)` creates a `qfloat_t` precision value
- `num_create_from_qcomplex(...)` creates a `qcomplex_t` precision value

`NUM_SQRT_PI_OVER_TWO` and its `sqrt(pi/2)`, `sqrt(@pi/2)` and
`√(π/2)` aliases denote √(π/2), approximately 1.2533141373, at the
requested precision. The division by two is inside the square root.

Explicit precision materialisation is available for constants and existing
values:

- `num_new_with_prec_bits(...)`
- `num_const_prec(...)`
- `num_const_prec_digits(...)`

Precision control helpers use the same naming convention:

- `num_set_default_prec_bits(...)`
- `num_get_default_prec_bits(...)`
- `num_set_default_prec_digits(...)`
- `num_get_default_prec_digits(...)`
- `num_set_prec_bits(...)`
- `num_get_prec_bits(...)`
- `num_set_prec_digits(...)`
- `num_get_prec_digits(...)`

Return conventions in this area:

- `num_set_default_prec_bits(...)`, `num_set_default_prec_digits(...)`,
  `num_set_prec_bits(...)`, and `num_set_prec_digits(...)` return `0` on
  success and `-1` on invalid input or unsupported conversion.
- `num_get_default_prec_bits(...)` and `num_get_default_prec_digits(...)`
  return the current default precision directly.
- `num_get_prec_bits(...)` and `num_get_prec_digits(...)` return the current
  value precision directly, or `0` when precision is not meaningful for the
  current backend.

Exact integer and rational values remain exact rather than being rounded to the
default floating precision.

Setter precision follows the current value where that makes sense:

- `num_set_long(...)` and `num_set_frac(...)` replace the destination with exact
  integer or rational storage.
- `num_set_double(...)`, `num_set_cdouble(...)`, `num_set_qfloat(...)`, and
  `num_set_qcomplex(...)` preserve an existing multiprecision destination's
  precision instead of silently dropping it to the source precision.
- If the destination is already fixed precision, the matching fixed-precision
  setter keeps that fixed-precision backend.

## Ownership Model

`number_t` uses a by-value public API, but some values may manage heap-backed
internal state.

That means:

- functions returning `number_t` return a live value
- callers should call `num_destroy(&value)` when finished with such a value
- passing a `number_t` by value performs only a shallow copy
- use `num_clone(...)` when an independent live copy is required

Example:

```c
#include <stdio.h>
#include "number-t.h"

int main(void)
{
    NUM_SCOPE(scope);
    number_t a = num_create_from_string("2");
    number_t b = num_create_from_string("5/6");
    number_t c = num_add(a, b);

    num_printf("sum = %n\n", c);
    num_destroy(&a);
    num_destroy(&b);
    num_destroy(&c);
    return 0;
}
```

Output:

```text
sum = ¹⁷⁄₆
```

Example with explicit precision:

```c
#include <stdio.h>
#include "number-t.h"

int main(void)
{
    NUM_SCOPE(scope);
    num_set_default_prec_bits(768);

    number_t x = num_create_from_string("1.25");
    number_t y = num_const_prec(NUM_PI, 512);

    printf("default bits: %zu\n", num_get_default_prec_bits());
    printf("x bits: %zu\n", num_get_prec_bits(x));
    printf("y bits: %zu\n", num_get_prec_bits(y));

    num_destroy(&x);
    num_destroy(&y);
    return 0;
}
```

Output:

```text
default bits: 768
x bits: 768
y bits: 512
```

Named constants such as `NUM_PI`, `NUM_E`, `NUM_LN10`, `NUM_PHI`, and
`NUM_EULER_MASCHERONI` are safe to clear as well:

```c
#include <stdio.h>
#include "number-t.h"

int main(void)
{
    NUM_SCOPE(scope);
    number_t pi = NUM_PI;
    num_destroy(&pi);
    printf("constant cleared safely\n");
    return 0;
}
```

Output:

```text
constant cleared safely
```

## Temporary Scopes

`number_t` also supports optional temporary scopes for heap-backed results.

Outside any active scope, constructors and arithmetic helpers behave normally:

- functions returning `number_t` return an owning live value
- callers later release that value with `num_destroy(...)`

Inside an active scope:

- newly created heap-backed temporary results are reclaimed automatically by
  `num_scope_leave(...)`
- scoped temporaries must not outlive the scope unless they are detached first
  with `num_scope_detach(...)`

Arithmetic helpers that use an internal scope return their results to the
caller's scope. This includes rounding, integer addition and multiplication,
arguments and pulse functions; their internal cleanup must neither free the
returned result nor bypass the caller's automatic cleanup.

Detaching behaves differently depending on how the scoped value is stored:

- plain heap-backed scoped values are transferred out of the scope directly
- arena-backed scoped values are copied into an ordinary owning value so they
  can survive scope teardown safely

Example with the public cleanup macro:

```c
#include "number-t.h"

number_t make_sum(void) {
    NUM_SCOPE(scope);

    number_t a = num_create_from_string("1/3");
    number_t b = num_create_from_string("1/6");
    number_t sum = num_add(a, b);
    return num_scope_detach(sum);
}

int main(void) {
    number_t kept = make_sum();
    num_printf("kept = %n\n", kept);
    num_destroy(&kept);
    return 0;
}
```

Output:

```text
kept = ½
```

If you need a portable manual form instead of `NUM_SCOPE(...)`, the equivalent
sequence is:

```c
#include <stdio.h>
#include "number-t.h"

int main(void)
{
    num_scope_t *scope = num_scope_enter();
    number_t sum = num_add(NUM_ONE, NUM_HALF);
    num_printf("sum = %n\n", sum);
    num_scope_leave(&scope);
    return 0;
}
```

Output:

```text
sum = ³⁄₂
```

The intended fast pattern is:

- put only short-lived intermediates inside the scope
- keep rolling state or returned values as ordinary owned `number_t` values
- destroy that rolling state explicitly when replacing it

That avoids repeated detach/copy work and lets the scope reclaim the cheap
temporary churn.

## Formatting

`number_t` provides its own formatting helpers:

- `num_printf(...)`
- `num_sprintf(...)`
- `num_vsprintf(...)`
- `num_to_string(...)`

Return conventions:

- `num_to_string(...)` returns a newly allocated `string_t *` that must be
  released with `string_free()`.
- `num_sprintf(...)`, `num_vsprintf(...)`, and `num_printf(...)` follow the
  usual `snprintf` / `printf` return conventions.

Supported format specifiers:

- `%n` — pretty human-readable form
- `%N` — scientific form for floating-point and complex values, otherwise the
  ordinary exact form

Both specifiers render an exactly zero complex value as `0`. A nonzero real
value stored in a complex backend is formatted as its real component, without
a redundant zero imaginary suffix. Small nonzero imaginary components
are not treated as zero; field width, padding and explicit sign flags still apply.

Examples:

- integer and rational values print exactly:
  - `42`
  - `⅚`
  - `³⁵⁵⁄₁₁₃`
- inexact floating and complex values print in a human-friendly form rather
  than a full exact binary-to-decimal dump

## Comparisons

`number_t` supports cross-backend equality by promoting operands to a sensible
common representation.

- `num_eq(a, b)` works across mixed backends
- ordering helpers:
  - `num_lt`
  - `num_le`
  - `num_gt`
  - `num_ge`
  - `num_cmp`
  are only meaningful for real values

For non-real values:

- the ordering predicates return `false`
- `num_cmp(...)` returns `0`

This avoids pretending complex values have a natural total order.

Return conventions in this area:

- predicate helpers such as `num_is_real(...)`, `num_is_zero(...)`, and
  `num_eq(...)` return `true` or `false`
- scalar query helpers such as `num_get_sign(...)` and `num_get_exponent2(...)`
  return the requested property directly
- `num_cmp(...)` returns an integer ordering result for real values

## Arithmetic and Functions

The generic layer promotes compatible mixed backends before dispatch. Unless a
status or predicate return is stated explicitly, each operation returns a new
owning `number_t`.

### Core Arithmetic and Components

| Function | Meaning |
| --- | --- |
| `num_add(a, b)` | Returns the sum `a + b`. |
| `num_sub(a, b)` | Returns the difference `a - b`. |
| `num_mul(a, b)` | Returns the product `a b`. |
| `num_div(a, b)` | Returns the quotient `a / b`. |
| `num_inv(x)` | Returns the reciprocal `1 / x`. |
| `num_neg(x)` | Returns the additive inverse `-x`. |
| `num_add_long(x, n)` | Adds the machine integer `n` without requiring the caller to construct a `number_t`. |
| `num_mul_long(x, n)` | Multiplies by the machine integer `n`. |
| `num_pow(x, y)` | Returns the principal value of `x` raised to the `y` power, including complex results for negative real bases and non-integer exponents on inexact backends. |
| `num_pow_int(x, n)` | Raises `x` to an integer power; negative powers return the reciprocal of the corresponding positive power. |
| `num_ldexp(x, n)` | Multiplies `x` by the exact binary scale `2^n`. |
| `num_mul_pow10(x, n)` | Multiplies `x` by the decimal scale `10^n`. |
| `num_sqr(x)` | Returns `x²`, preserving exact integer and rational arithmetic within the precision budget. |
| `num_abs(x)` | Returns the real magnitude `\|x\|`. |
| `num_conj(x)` | Returns the complex conjugate; real inputs are unchanged. |
| `num_real_part(x)` | Extracts the real component. |
| `num_imag_part(x)` | Extracts the imaginary component. |
| `num_arg(x)` | Returns the principal complex argument in radians. |
| `num_floor(x)` | Returns the greatest integer not exceeding a real input. |
| `num_round(x)` | Returns the nearest integer, with exact halves away from zero; complex components are rounded independently. Exact integers and rationals are not converted to double, and non-finite inputs are preserved. |
| `num_roundn(x, n)` | Rounds to signed decimal places; negative n rounds to powers of ten. Exact halves round away from zero. |
| `num_rounds(x, n)` | Rounds to an unsigned count of significant decimal digits; zero digits returns NaN. Complex components are rounded independently. |
| `num_ceil(x)` | Returns the least integer not less than a real input. |
| `num_hypot(x, y)` | Returns `sqrt(x² + y²)` with scaling that avoids needless overflow or underflow. |

The `num_add_slow`, `num_sub_slow`, `num_mul_slow`, and `num_div_slow`
variants perform the same four operations through the full generic dispatcher.
Their existing names remain available for compatibility and parity checks.
The ordinary arithmetic APIs now use the same out-of-line implementations,
including their direct paths for equal fixed-precision backends. Their precision,
promotion and arithmetic rules are unchanged. `num_destroy` likewise uses its
existing library implementation, so callers must link the MARS library.

### Elementary, Trigonometric, and Hyperbolic Functions

| Function | Meaning |
| --- | --- |
| `num_exp(x)` | Returns the natural exponential `e^x`. |
| `num_log(x)` | Returns the principal natural logarithm. |
| `num_ln(x)` | Returns `num_log(x)`; this is the conventional natural-logarithm shorthand. |
| `num_log10(x)` | Returns the principal base-10 logarithm. |
| `num_lg(x)` | Returns `num_log10(x)`; this is the conventional common-logarithm shorthand. |
| `num_sqrt(x)` | Returns the principal square root. |
| `num_cubrt(x)` | Returns the principal cube root. |
| `num_root(x, n)` | Returns the principal `n`th root, where `n` is an exact integer greater than one. |
| `num_sin(x)` | Returns the sine. |
| `num_cos(x)` | Returns the cosine. |
| `num_tan(x)` | Returns the tangent, `sin(x) / cos(x)`. |
| `num_sincos(x, &s, &c)` | Computes sine and cosine together; returns `0` on success and `-1` for invalid output pointers. |
| `num_sec(x)` | Returns the secant, `1 / cos(x)`. |
| `num_cosec(x)` | Returns the cosecant, `1 / sin(x)`. |
| `num_cot(x)` | Returns the cotangent, `1 / tan(x)`. |
| `num_versin(x)` | Returns the versed sine, `1 - cos(x)`. |
| `num_vercos(x)` | Returns the versed cosine, `1 + cos(x)`. |
| `num_coversin(x)` | Returns the coversed sine, `1 - sin(x)`. |
| `num_covercos(x)` | Returns the coversed cosine, `1 + sin(x)`. |
| `num_haversin(x)` | Returns the haversine, `(1 - cos(x)) / 2`. |
| `num_havercos(x)` | Returns the havercosine, `(1 + cos(x)) / 2`. |
| `num_hacoversin(x)` | Returns the hacoversine, `(1 - sin(x)) / 2`. |
| `num_hacovercos(x)` | Returns the hacovercosine, `(1 + sin(x)) / 2`. |
| `num_asin(x)` | Returns the principal inverse sine. |
| `num_acos(x)` | Returns the principal inverse cosine. |
| `num_atan(x)` | Returns the principal inverse tangent. |
| `num_atan2(y, x)` | Returns the quadrant-aware angle of the real coordinate pair `(x, y)`. |
| `num_asec(x)` | Returns the principal inverse secant. |
| `num_acosec(x)` | Returns the principal inverse cosecant. |
| `num_acot(x)` | Returns the principal inverse cotangent. |
| `num_arcversin(x)` | Returns the principal inverse of `versin`. |
| `num_arcvercos(x)` | Returns the principal inverse of `vercos`. |
| `num_arccoversin(x)` | Returns the principal inverse of `coversin`. |
| `num_arccovercos(x)` | Returns the principal inverse of `covercos`. |
| `num_archaversin(x)` | Returns the principal inverse haversine. |
| `num_archavercos(x)` | Returns the principal inverse havercosine. |
| `num_archacoversin(x)` | Returns the principal inverse hacoversine. |
| `num_archacovercos(x)` | Returns the principal inverse hacovercosine. |
| `num_sinh(x)` | Returns the hyperbolic sine. |
| `num_cosh(x)` | Returns the hyperbolic cosine. |
| `num_sinhcosh(x, &s, &c)` | Computes hyperbolic sine and cosine together; returns `0` on success and `-1` for invalid output pointers. |
| `num_tanh(x)` | Returns the hyperbolic tangent, `sinh(x) / cosh(x)`. |
| `num_sech(x)` | Returns the hyperbolic secant, `1 / cosh(x)`. |
| `num_cosech(x)` | Returns the hyperbolic cosecant, `1 / sinh(x)`. |
| `num_coth(x)` | Returns the hyperbolic cotangent, `1 / tanh(x)`. |
| `num_asinh(x)` | Returns the principal inverse hyperbolic sine. |
| `num_acosh(x)` | Returns the principal inverse hyperbolic cosine. |
| `num_atanh(x)` | Returns the principal inverse hyperbolic tangent. |
| `num_asech(x)` | Returns the principal inverse hyperbolic secant. |
| `num_acosech(x)` | Returns the principal inverse hyperbolic cosecant. |
| `num_acoth(x)` | Returns the principal inverse hyperbolic cotangent. |

`num_sqrt`, `num_cubrt`, and `num_root` return one principal value.
`num_root(number, order)` promotes a negative real radicand to a complex result
when the selected principal root is not real.

The arbitrary-precision `number_t` backends likewise promote real inputs across
the principal branch cuts of inverse circular and hyperbolic functions. Thus
`num_asin(2)` and `num_acosh(1/2)` return complex principal values rather than
discarding the imaginary component. The double and double-double backends keep
their existing backend-specific precision and representation.

### Special Functions

| Function | Meaning |
| --- | --- |
| `num_gamma(x)` | Evaluates Euler's gamma function `Γ(x)`. |
| `num_lgamma(x)` | Evaluates the principal logarithm of `Γ(x)`. |
| `num_digamma(x)` | Evaluates `ψ(x)`, the logarithmic derivative of gamma; arbitrary-precision complex inputs retain the active precision and use recurrence and reflection across the complex plane. |
| `number_qdigamma(q, x)` | Evaluates the q-digamma `ψ_q(x)` while preserving the active real or complex backend. |
| `num_qdigamma(q, x)` | Compatibility spelling for `number_qdigamma(q, x)`. |
| `num_trigamma(x)` | Evaluates `ψ⁽¹⁾(x)`, the derivative of digamma. |
| `num_tetragamma(x)` | Evaluates `ψ⁽²⁾(x)`, the derivative of trigamma. |
| `num_polygamma(m, x)` | Evaluates the order-`m` polygamma function `ψ⁽ᵐ⁾(x)`. |
| `num_zeta(s)` | Evaluates the analytically continued Riemann zeta function `ζ(s)`. |
| `num_zetap(s)` | Evaluates the derivative `ζ′(s)` with respect to `s`. |
| `num_zetah(s, a)` | Evaluates the analytically continued Hurwitz zeta function `ζ(s, a)`. |
| `num_zatahp(s, a)` | Evaluates the derivative of Hurwitz zeta with respect to its first argument. |
| `num_gammainv(x)` | Evaluates the inverse gamma function used by MARS, returning the principal inverse branch. |
| `num_erf(x)` | Evaluates the Gaussian error function. |
| `num_erfc(x)` | Evaluates the complementary error function `1 - erf(x)`. |
| `num_erfinv(x)` | Evaluates the principal inverse error function. |
| `num_erfcinv(x)` | Evaluates the principal inverse complementary error function. |
| `num_lambert_wn(k, x)` | Evaluates branch `k` of Lambert W, the inverse of `w e^w`. |
| `num_lambert_w0(x)` | Evaluates the principal branch `W₀(x)`. |
| `num_lambert_wm1(x)` | Evaluates the real lower branch `W₋₁(x)` where it exists, with principal complex continuation otherwise. |
| `num_productlog(x)` | Evaluates the principal product logarithm, an alias of principal Lambert W. |
| `num_beta(a, b)` | Evaluates Euler's beta function `B(a, b)`. |
| `num_logbeta(a, b)` | Evaluates the principal logarithm of `B(a, b)`. |
| `num_binomial(n, k)` | Evaluates the generalised binomial coefficient. |
| `num_beta_pdf(x, a, b)` | Evaluates the beta-distribution probability density at `x`. |
| `num_logbeta_pdf(x, a, b)` | Evaluates the natural logarithm of the beta density. |
| `num_normal_pdf(x)` | Evaluates the standard normal probability density. |
| `num_normal_cdf(x)` | Evaluates the standard normal cumulative distribution. |
| `num_normal_logpdf(x)` | Evaluates the natural logarithm of the standard normal density. |
| `num_gammainc_lower(s, x)` | Evaluates the lower incomplete gamma function `γ(s, x)`. |
| `num_gammainc_upper(s, x)` | Evaluates the upper incomplete gamma function `Γ(s, x)`. |
| `num_gammainc_P(s, x)` | Evaluates the regularised lower incomplete gamma function. |
| `num_gammainc_Q(s, x)` | Evaluates the regularised upper incomplete gamma function. |
| `num_E1(x)` | Evaluates the exponential integral `E₁(x)`. |
| `num_Ei(x)` | Evaluates the principal exponential integral `Ei(x)`. |
| `num_Li(x)` | Evaluates the principal logarithmic integral `Li(x) = Ei(log(x))`. |
| `num_dilog(x)` | Evaluates the dilogarithm `Li₂(x)`. |
| `num_clausen2(theta)` | Evaluates the Clausen function `Cl₂(theta)` at native precision. |
| `num_clausen(order, theta)` | Evaluates the positive integer-order Clausen family, with real arithmetic for real inputs. |
| `num_polylog1(x)` | Evaluates the order-one polylogarithm `Li₁(x) = −Log(1−x)` on the active backend's principal branch. |
| `num_polylog(s, x)` | Evaluates the polylogarithm `Liₛ(x)`. |
| `number_lerch_phi(z, s, a)` | Evaluates the Lerch transcendent `Φ(z,s,a)` while preserving the active real or complex backend. |
| `num_lerch_phi(z, s, a)` | Compatibility spelling for `number_lerch_phi(z, s, a)`. |
| `num_harmonic_poly(degree, x)` | Evaluates the finite harmonic polynomial Hₙ(x) = Σₖ₌₁ⁿ xᵏ/k; the degree must be an exact non-negative integer. |
| `num_legendre_chi(s, x)` | Evaluates Legendre's chi function `χₛ(x)`. |
| `num_bessel_j(ν, x)` | Evaluates the Bessel function of the first kind `Jν(x)`. |
| `num_bessel_i(ν, x)` | Evaluates the principal modified Bessel function `Iν(x)`, including `I₀(x)` at order zero. |
| `num_bessel_k(ν, x)` | Evaluates the principal modified Bessel function `Kν(x)` for real or complex order and non-zero argument. |
| `num_bessel_y(ν, x)` | Evaluates the Bessel function of the second kind `Yν(x)`. |
| `num_struve_l(ν, x)` | Evaluates the principal modified Struve function `Lν(x)` for real or complex inputs. |
| `num_struve_h(ν, x)` | Evaluates the principal ordinary Struve function `Hν(x)` for real or complex inputs. |
| `num_lommel_s(μ, ν, x)` | Evaluates the Lommel function `sμ,ν(x)`. |
| `num_appell_f1(a, b1, b2, c, x, y)` | Evaluates the two-variable Appell `F₁` hypergeometric function. |
| `num_lauricella_f(a, b, c, x, count)` | Evaluates Lauricella `F_D` for the paired parameter and argument arrays. |
| `num_hypergeometric_pFq(upper, p, lower, q, x)` | Evaluates the generalised hypergeometric function with `p` upper and `q` lower parameters. |

#### Incomplete gamma precision

For finite real `a > 0` and `0 <= x <= a + 1`, the multiprecision
implementations of `num_gammainc_lower(a, x)` and `num_gammainc_P(a, x)`
evaluate the lower tail directly. They use the positive series with initial
term `1` and recurrence `term[n] = term[n-1] * x / (a + n)`.
The sum is scaled by `exp(a*log(x) - x)/a` for the unnormalised lower gamma,
or by `exp(a*log(x) - x - log(Gamma(a+1)))` for P. Logarithmic scaling avoids
unnecessarily overflowing intermediate powers or complete gamma values.
Both lower-tail functions return zero at `x = 0` in this domain.

This preserves small tails that would otherwise lose significant digits when
subtracting the upper incomplete gamma from the complete gamma. Working
precision includes 64 guard bits, plus the positive binary exponent of `a`
when needed for the logarithmic prefactor. Convergence uses a geometric bound
on the entire remaining positive tail and the shared relative series criterion,
requiring more than the requested precision plus 16 bits of exponent separation.
The computation is bounded to 200,000 series terms; exhaustion or an
unrepresentable working precision returns an invalid numeric result rather than
silently reverting to cancellation-prone subtraction. Ordinary MPFR exponent-range
underflow and overflow still apply.

Outside this lower-tail region the existing complete-minus-upper calculation
remains in use. `num_gammainc_upper` and `num_gammainc_Q` retain their direct
MPFR upper-tail calculation, and existing non-finite, non-positive-parameter
and complex-domain behaviour is unchanged. The fixed-precision qfloat backend
already evaluates its lower tail directly.

#### Clausen functions

`num_clausen(order, theta)` takes a positive integer order and an angle in
radians; `num_clausen2(theta)` selects order two. For real angles, even
orders use the sine Fourier series and odd orders the cosine Fourier series.
Order one is `-ln(2*abs(sin(theta/2)))`, with logarithmic poles at multiples
of `2*pi`; higher orders are continuous and periodic on the real axis.

Real inputs are evaluated using real arithmetic. Fixed-precision and
arbitrary-precision inputs retain their native precision. Nonreal inputs use
the [holomorphic Clausen continuation](qcomplex.md#clausen-functions),
including its exclusion of nonreal points on `Re(z) = 2*k*pi`. Order zero
and non-finite inputs return NaN. Each call returns an owning number.

### Bessel Y

`num_bessel_y(order, argument)` returns an owning principal Bessel function
of the second kind, including Y₀. Real and complex inputs use MPFR/MPC at
the greatest input precision; exact inputs contribute the current default
precision. The calculation does not change the input values or the default
precision setting. Positive real arguments and real orders return real
numbers; other non-zero arguments can return complex numbers.

The branch is `-pi < arg(z) <= pi`. An exactly negative real argument uses
the upper bank even if its imaginary zero is negative. Approaching the cut
from below gives the lower-bank limit. For integer `n`,
`Y_-n(z) = (-1)^n*Y_n(z)` and
`Y_n(-x+i0) = (-1)^n*(Y_n(x)+2*i*J_n(x))`, where `x > 0`.
At the origin, negative half-integer orders have the finite value zero.
Other origin values, including Y₀, return NaN.

Integer orders and real arguments use MPFR. Complex integer-order values
use the logarithmic and harmonic-number series, avoiding division by
`sin(pi*order)`. Non-integer orders use the J/Y connection formula and the
native arbitrary-precision I series, with rotations towards the right
half-plane and explicit principal-branch phase corrections. Extra bits
resolve orders close to integers. The series checks its remaining tail
and accumulated cancellation before returning a value.

Order magnitudes must be at most 1000. Complex arguments and non-integer
orders also require argument magnitude at most 1000. Real arguments with
integer orders can exceed that argument limit. Work precision is limited
to 65536 bits, including any nested I evaluation; series stop after 20000
terms. Non-finite inputs, unresolved cancellation, unsupported ranges and
exhausted guards return NaN. Results are unscaled. The qfloat and qcomplex
wrappers additionally have the exponent range of their component type.

```c
#include <stdio.h>
#include "number-t.h"

int main(void)
{
    NUM_SCOPE(scope);
    number_t value = num_bessel_y(NUM_ZERO, NUM_ONE);
    printf("Y_0(1) = %.15f\n", num_to_double(value));
    num_destroy(&value);
    return 0;
}
```

```text
Y_0(1) = 0.088256964215677
```

The result is detached from numeric scopes and must be released explicitly.
The example rounds only its displayed output to double precision. See also
the [qfloat](qfloat.md) and [qcomplex](qcomplex.md#bessel-y) APIs.

### Modified Bessel K

`num_bessel_k(order, argument)` returns an owning number containing the
principal modified Bessel function of the second kind. Both operands may be
real or complex. Non-finite operands, a zero argument, or an operand magnitude
greater than 1000 return NaN. Real orders with positive real arguments return
real values; other supported arguments follow the principal logarithm and power
branches and may return complex values. The input values are not modified.
The result is detached from numeric scopes and must be released explicitly
with `num_destroy(...)`.

The result uses the greatest input precision, with exact inputs contributing
the current default precision, including exact Cartesian components. Evaluation
adds working precision for cancellation and proximity to integer orders, then
rounds the result back to the requested precision. Integer orders use harmonic
series for K₀ and K₁ followed by recurrence; non-integer orders use two reduced
I-series with gamma factors. Each series is limited to 20000 iterations; failure
to converge returns NaN. The near-integer guard also rejects an unrepresentable
distance estimate or a required distance correction above 65536 bits. This is
not a uniform 65536-bit cap on the total working precision.

The qfloat and qcomplex interfaces delegate to this evaluator and round to
double-double components. Symbolic argument derivatives, affine primitives and
finite sums belong to the [expression layer](expression.md#inverse-hyperbolic-sine-fourier-pair).

### Modified Bessel I

`num_bessel_i(order, argument)` returns an owning number containing the
unscaled modified Bessel function of the first kind. Order zero selects
`I_0(z)`. Real, complex, exact rational and fixed-precision inputs use the
widest input precision; exact inputs contribute the current default precision.
The result is a multiprecision real or complex number, except for exact
zero-argument limits. Inputs and the default precision setting are preserved.
The [qfloat](qfloat.md#modified-bessel-i) and
[qcomplex](qcomplex.md#modified-bessel-i) APIs round this same kernel.

The defining series, from [DLMF 10.25.2](https://dlmf.nist.gov/10.25.E2), is

$\quad\begin{array}{l}\displaystyle I_\nu(z)=\sum_{k=0}^{\infty} \frac{(z/2)^{\nu+2k}}{\Gamma(k+1)\Gamma(k+\nu+1)}.\end{array}$

Its principal power uses `-pi < arg(z) <= pi`. An exactly negative real
argument takes the upper-bank value irrespective of signed zero. Real
integral orders give real results on either real half-axis; other real
orders can give complex results on the negative axis. The shifted series
retains the non-zero tail at negative integer orders, satisfying
`I_-n(z) = I_n(z)`, and preserves small coefficients at nearby orders.

At zero, order zero gives one; orders with positive real part and negative
integer orders give zero. Other zero limits return NaN, including purely
imaginary non-zero orders and negative non-integral real orders.

Bessel I and [Struve L](#modified-struve-l) share one guarded series engine
and private multiprecision complex gamma calculation. Both input magnitudes
must be at most 1000. Work precision adds 96 bits plus
`ceil(4*(abs(order)+abs(argument)))`, with additional bits near non-positive
half-integers, and may not exceed 65536 bits. The forward sum stops by series
index 20000. Tail and cancellation checks reject unresolved values, and
exponent overflow/underflow or non-finite inputs return NaN. These bounds
describe supported numerical coverage, not a guarantee of convergence at
every point. No scaled or large-argument asymptotic evaluator is provided.

```c
#include <stdio.h>
#include "number-t.h"

int main(void)
{
    NUM_SCOPE(scope);
    number_t bessel = num_bessel_i(NUM_ZERO, NUM_ONE);
    num_printf("I_0(1) = %.16n\n", bessel);
    return 0;
}
```

```text
I_0(1) = 1.266065877752008
```

### Ordinary Struve H

`num_struve_h(order, argument)` returns an owning number containing the
unscaled principal ordinary Struve function. Real and complex orders and
arguments are accepted, including exact integer and rational inputs. The
result retains the greater input precision; exact inputs contribute the
current default precision. Neither inputs nor the default precision setting
are changed. Results use multiprecision real or complex storage, except for
exact zero limits. The [qfloat](qfloat.md#ordinary-struve-h) and
[qcomplex](qcomplex.md#ordinary-struve-h) APIs round the same kernel.

The defining series is

$\quad\begin{array}{l}\displaystyle \mathbf H_\nu(z)=\sum_{k=0}^{\infty} \frac{(-1)^k(z/2)^{\nu+1+2k}}{\Gamma(k+3/2)\Gamma(k+\nu+3/2)}.\end{array}$

See [DLMF 11.2.1](https://dlmf.nist.gov/11.2.E1). The principal power uses
the original argument, with `-pi < arg(z) <= pi`; the negative axis itself
uses the upper bank, irrespective of signed zero. Integral real orders
give real results on either real half-axis. Other orders can produce
complex values for negative real arguments.

H shares the guarded gamma and series engine with [Struve L](#modified-struve-l)
and [Bessel I](#modified-bessel-i). It negates the recurrence's squared
argument and includes the shifted seed's `(-1)^k` factor, rather than using
a branch-sensitive power rotation. Backwards multiplication crosses
reciprocal-gamma zeros without division by poles. In particular,
`H_(-m-1/2)(z) = (-1)^m J_(m+1/2)(z)` for integers `m >= 0`.
Nearby orders retain their small, non-zero coefficients. Complex gamma
coefficients use the guarded multiprecision expansion, not a fixed-precision
complex gamma fallback.

At zero, `Re(order) > -1` and real orders `-3/2, -5/2, ...` give zero;
real order `-1` gives `2/pi`. Other zero limits return NaN, including
divergent real cases and oscillatory complex cases.

Finite input magnitudes are limited to 1000. Work precision adds 96 bits
plus `ceil(4*(abs(order)+abs(argument)))` and extra bits near non-positive
half-integer orders, up to 65536 total bits. The forward series is limited
to index 20000. Decreasing term ratios bound the remaining tail; the absolute
term sum checks cancellation, including the alternating real-axis case.
Failed numerical guards, unresolved zeros and exponent overflow/underflow
return NaN. There is no scaled or asymptotic evaluator, so the input bounds
do not guarantee success at every point.

```c
#include <stdio.h>
#include "number-t.h"

int main(void)
{
    NUM_SCOPE(scope);
    number_t ordinary = num_struve_h(NUM_ZERO, NUM_ONE);
    num_printf("H_0(1) = %.15n\n", ordinary);
    return 0;
}
```

```text
H_0(1) = 0.568656627048288
```

### Modified Struve L

`num_struve_l(order, argument)` returns an owning number containing the
unscaled principal modified Struve function. It accepts real and complex
orders and arguments, including exact integers and rationals, and uses the
greater input precision; exact inputs contribute the current default
precision. Inputs and the default precision setting are preserved. The result
uses a multiprecision real or complex backend, except for exact zero limits.
The [qfloat](qfloat.md#modified-struve-l) and
[qcomplex](qcomplex.md#modified-struve-l) entry points round this same kernel.

The defining series is

$\quad\begin{array}{l}\displaystyle \mathbf L_\nu(z)=\sum_{k=0}^{\infty} \frac{(z/2)^{\nu+1+2k}}{\Gamma(k+3/2)\Gamma(k+\nu+3/2)}.\end{array}$

See [DLMF 11.2.2](https://dlmf.nist.gov/11.2.E2). The principal power uses
`-pi < arg(z) <= pi`; the negative real axis itself takes the upper-bank
value, irrespective of signed zero. Real integral orders give real values
on either real half-axis. Real non-integral orders can give complex results
on the negative axis.

The series shares its engine with [Bessel I](#modified-bessel-i) and is
seeded where both gamma arguments have positive real parts,
then summed backwards and forwards. Consequently reciprocal-gamma poles
contribute zero coefficients, rather than making the function undefined.
In particular, `L_(-m-1/2)(z) = I_(m+1/2)(z)` for integers `m >= 0`.
Nearby orders retain their small, non-zero coefficients. Complex gamma factors
use a private shifted Stirling expansion with a remainder bound, based on
[DLMF 5.11](https://dlmf.nist.gov/5.11); they do not pass through fixed-precision
complex gamma dispatch.

At `z = 0`, the result is zero for `Re(order) > -1` and real orders
`-3/2, -5/2, ...`; real order `-1` gives `2/pi`. Other zero limits return NaN,
including divergent real cases and oscillatory complex cases.

Numerical coverage is restricted to finite inputs with `abs(order) <= 1000`
and `abs(argument) <= 1000`. Work precision adds 96 guard bits plus
`ceil(4*(abs(order)+abs(argument)))`, with extra bits for the distance of
non-positive orders from half-integers. Work above 65536 bits is rejected.
The forward sum is limited to series index 20000. A decreasing ratio bounds
the remaining tail, and the absolute term sum checks whether cancellation
has consumed the guard precision. Failure of these checks, unresolved zeros,
or exponent overflow/underflow returns NaN. There is no scaled or asymptotic
large-argument evaluator, and the finite input bounds do not guarantee that
every point passes the numerical guards.

```c
#include <stdio.h>
#include "number-t.h"

int main(void)
{
    NUM_SCOPE(scope);
    number_t value = num_struve_l(NUM_ZERO, NUM_ONE);
    num_printf("L_0(1) = %.16n\n", value);
    return 0;
}
```

```text
L_0(1) = 0.7102431859378909
```

### Exact Integer and Number-Theory Functions

| Function | Meaning |
| --- | --- |
| `num_factorial(n)` | Returns the exact factorial `n!`. |
| `num_fibonacci(n)` | Returns the exact `n`th Fibonacci number. |
| `num_partition(n)` | Returns the exact number of unrestricted integer partitions of `n`. |
| `num_gcd(a, b)` | Returns the greatest common divisor. |
| `num_lcm(a, b)` | Returns the least common multiple. |
| `num_mod(a, m)` | Returns the canonical remainder modulo `m`. |
| `num_divmod(a, b, &q, &r)` | Produces quotient `q` and remainder `r`; returns `0` on success. |
| `num_gcdext(a, b, &g, &x, &y)` | Produces Bézout coefficients satisfying `ax + by = g = gcd(a, b)`; returns `0` on success. |
| `num_powmod(a, e, m)` | Returns modular exponentiation `a^e mod m`. |
| `num_modinv(a, m)` | Returns the multiplicative inverse of `a` modulo `m`, or an invalid value when none exists. |
| `num_is_prime(n)` | Returns `true` when `n` passes the library's primality test. |
| `num_prove_prime(n)` | Returns `PRIME`, `COMPOSITE`, or `UNKNOWN` according to the proof result. |
| `num_next_prime(n)` | Returns the least prime strictly greater than `n`. |
| `num_prev_prime(n)` | Returns the greatest prime strictly less than `n`. |
| `num_factors(n)` | Returns the exact prime factorisation as an owned `number_factors_t`. |
| `num_bit_length(n)` | Returns the number of significant bits in the absolute integer value. |
| `num_test_bit(n, i)` | Tests whether bit `i` is set. |
| `num_set_bit(n, i)` | Returns `n` with bit `i` set. |
| `num_clear_bit(n, i)` | Returns `n` with bit `i` cleared. |
| `num_bit_not(n)` | Returns the bitwise complement. |
| `num_bit_and(a, b)` | Returns the bitwise AND. |
| `num_bit_or(a, b)` | Returns the bitwise OR. |
| `num_bit_xor(a, b)` | Returns the bitwise exclusive OR. |
| `num_shl(n, bits)` | Shifts left; a negative count delegates to a right shift. |
| `num_shr(n, bits)` | Shifts right; a negative count delegates to a left shift. |
| `num_isqrt(n)` | Returns `floor(sqrt(n))` exactly for a non-negative integer. |

Binary arithmetic mixes supported backends automatically by promoting to a
common target representation before dispatch.

Appell F₁ is the two-variable member of `num_lauricella_f(...)`. Its MPFR and
MPC paths group the defining double series by total degree and generate the
coefficients with a three-term recurrence. This avoids nested summation and
retains the participating precision for real and complex parameters. The
convergent-series domain is `|x|, |y| < 0.95`; poles, exhausted iteration or
cancellation guards, and work above 65536 bits return NaN. General pFq and
higher-dimensional Lauricella calls use the native qfloat or qcomplex
implementation after ordinary `number_t` promotion.

Complex Bessel J shares the guarded cylindrical series used for Bessel I and
Struve H/L. Complex Lommel s uses its defining power series and differentiates
the same terms for argument derivatives. Both support argument magnitudes up
to 1000 with a 65536-bit work limit; singular parameters, undefined limits and
failed convergence or cancellation guards return NaN. Principal powers take
the upper bank on the negative real axis.

The complex error functions use an entire Kummer series with additional
precision for exponential cancellation. The supported complex magnitude is
at most 140, subject to a 65536-bit work limit. Real erf/erfc use MPFR at the
participating precision. Real and complex inverse erf/erfc refine an initial
estimate with checked Halley iteration and verify the forward residual;
the starting estimate does not limit the result to double precision. Inverse
erfc works directly with small complementary probabilities. Non-convergence
returns NaN, rather than an unchecked finite result. Real endpoint values
retain their signed infinities.

The related Gaussian PDF, log PDF and CDF also retain complex working precision.
The CDF uses `erfc(-x/sqrt(2))/2` so small negative-tail probabilities survive
instead of cancelling in `1 + erf(x/sqrt(2))`.

Return conventions in this area:

- functions returning `number_t` return an owning by-value result
- the caller should later pass that result to `num_destroy(...)`
- paired-output helpers such as `num_sincos(...)` and `num_sinhcosh(...)`
  return an `int` status code, with `0` for success and `-1` for invalid input

## Examples

### Basic Arithmetic and Beta

```c
#include <stdio.h>
#include "number-t.h"
#include "string-t.h"

int main(void) {
    number_t a = num_create_from_string("2");
    number_t b = num_create_from_string("3");
    number_t c = num_create_from_string("5/6");
    number_t sum = num_add(a, c);
    number_t beta = num_beta(a, b);
    string_t *sum_text = num_to_string(sum);
    string_t *beta_text = num_to_string(beta);

    if (!sum_text || !beta_text)
        return 1;

    printf("2 + 5/6 = %s\n", string_c_str(sum_text));
    num_printf("beta(2, 3) = %.32n\n", beta);

    string_free(sum_text);
    string_free(beta_text);
    num_destroy(&a);
    num_destroy(&b);
    num_destroy(&c);
    num_destroy(&sum);
    num_destroy(&beta);
    return 0;
}
```

```text
2 + 5/6 = ¹⁷⁄₆
beta(2, 3) = 0.083333333333333333333333333333333
```

### Runnable README Output Examples

These examples are mirrored by `tests/number/test_number_readme.c`, so the
documented output stays tied to the public `number_t` API.

### Exact Rational Arithmetic

```c
#include "string-t.h"
#include <stdio.h>
#include "number-t.h"

int main(void)
{
    NUM_SCOPE(scope);
    number_t a = num_create_from_frac(2, 3);
    number_t b = num_create_from_string("5/4");
    number_t product = num_mul(a, b);
    string_t *text = num_to_string(product);

    printf("(2/3) * (5/4) = %s\n", string_c_str(text));
    string_free(text);
    return 0;
}
```

```text
(2/3) * (5/4) = ⅚
```

### Exact Combinatorics

```c
#include "string-t.h"
#include <stdio.h>
#include "number-t.h"

int main(void)
{
    NUM_SCOPE(scope);
    number_t n = num_create_from_long(52);
    number_t k = num_create_from_long(5);
    number_t c = num_binomial(n, k);
    string_t *text = num_to_string(c);

    printf("C(52, 5) = %s\n", string_c_str(text));
    string_free(text);
    return 0;
}
```

```text
C(52, 5) = 2598960
```

### Multiprecision Special Functions

The generic special-function layer includes `num_zeta(s)` and `num_zetap(s)`
for the Riemann zeta function and its derivative, together with
`num_zetah(s, a)` and `num_zatahp(s, a)` for the Hurwitz zeta function and its
partial derivative with respect to `s`. The trailing `p` denotes a derivative.
Dispatch preserves the active `number_t` backend, including qfloat, qcomplex,
MPFR and MPC values.

The native Euler–Maclaurin zeta paths reuse successive rising products and
endpoint powers, and omit derivative work for value-only calls. This also
applies to Hurwitz zeta. Above 512 working bits, their finite sums grow with
precision instead of retaining a fixed 128-term length. Both value and derivative
paths also extend the finite sum with the magnitude of the imaginary argument, so higher zero-search windows
do not retain a summation length suited only to small heights. Both paths retain their convergence checks and
precision-dependent Bernoulli corrections; an
exhausted correction allowance still returns NaN rather than an unchecked value.
Arbitrary-precision real Riemann zeta continues to use MPFR.

Multiprecision digamma and polygamma use a precision-dependent argument shift
and correction allowance. Trigamma and tetragamma use this same kernel for MPFR
and MPC operands. Corrections stop below the guarded precision target; the
260-entry exact Bernoulli cache is extended numerically when higher precision
requires more terms. The cache size therefore does not cap achievable accuracy.

```c
#include <stdio.h>
#include "number-t.h"

int main(void)
{
    NUM_SCOPE(scope);
    num_set_default_prec_bits(256);

    number_t x = num_create_from_string("2.345");
    number_t gamma_x = num_gamma(x);
    number_t lgamma_x = num_lgamma(x);

    char gamma_text[256];
    char lgamma_text[256];

    num_sprintf(gamma_text, sizeof(gamma_text), "%.77n", gamma_x);
    num_sprintf(lgamma_text, sizeof(lgamma_text), "%.77n", lgamma_x);

    printf("gamma(2.345)  = %s\n", gamma_text);
    printf("lgamma(2.345) = %s\n", lgamma_text);
    return 0;
}
```

```text
gamma(2.345)  = 1.1992978294153192855268153358879569120923525584905703781289979370034378685904
lgamma(2.345) = 0.18173624337757203797862933229995978550118791690492470651875093221924437275614
```

### Multiprecision Complex Functions

```c
#include <stdio.h>
#include "number-t.h"

int main(void)
{
    NUM_SCOPE(scope);
    num_set_default_prec_digits(50);

    number_t z = num_create_from_string("1 + i");
    number_t exp_z = num_exp(z);

    char text[256];
    num_sprintf(text, sizeof(text), "%n", exp_z);

    printf("exp(1 + i) = %s\n", text);
    return 0;
}
```

```text
exp(1 + i) = 1.468693939915885157138967597326604261326956736629 + 2.2873552871788423912081719067005018089555862566684i
```

## Benchmarks

The generic numeric layer has a matching benchmark target:

```sh
make bench_number_maths
```

It exercises the public `number_t` API across the representative arithmetic,
elementary, and special-function workload used to track numeric performance.

## Benchmark Coverage

The dedicated `number_t` maths benchmark includes matching timing cases at:

- `256` bits
- `512` bits
- `768` bits
- `1024` bits
- `2048` bits
- `4096` bits

across the broad maths slice used to track the multiprecision backend.

Benchmark source:

- [`bench/number/bench_number_maths.c`](../bench/number/bench_number_maths.c)

There is also a scope-focused benchmark target:

```sh
make bench_number_scope
```

That benchmark compares:

- ordinary manual ownership with explicit `num_destroy(...)`
- broad whole-scope temporary accumulation
- the preferred `scoped temporaries + owned rolling state` pattern

Recent sample results on this tree showed the preferred rolling pattern beating
the fully manual path, while the "scope everything and destroy nothing until
leave" pattern remained slower:

```text
real chain               manual= 129.082 ms  scoped= 125.772 ms  ratio= 1.026x
real scoped+roll         manual= 129.082 ms  scoped= 110.554 ms  ratio= 1.168x
complex chain            manual= 221.584 ms  scoped= 225.807 ms  ratio= 0.981x
complex scoped+roll      manual= 221.584 ms  scoped= 198.489 ms  ratio= 1.116x
```

Run it from the repository root with:

```sh
make bench_number_maths
MARS_BENCH_SCALE=10 ./build/release/bench/number/bench_number_maths
MARS_BENCH_FORMAT=md ./build/release/bench/number/bench_number_maths
```

Persistent timings are now recorded by `make bench-db ARGS='run'` and published
with `make bench-db ARGS='update-docs'`. These tables identify the exact run,
machine, backend and precision; the focused benchmark above remains available
for separate investigations. See [the benchmark guide](benchmarks.md#persistent-mathematical-benchmarks)
for selection, interruption recovery and raw sample storage.

<!-- MARS-BENCHMARKS:BEGIN -->
Benchmark run **11**, 2026-11-10T10:25:23.939Z (UTC).

Machine: ophelia; Intel(R) N95.

System: Linux 7.0.0-27-generic x86_64.

Compiler: 15.2.0; build flags: gcc -Wall -Wextra -Werror -O2 -fPIC -D_GNU_SOURCE -pthread -DHAVE_UNISTRING -I/usr/include/sqlcipher -DSQLITE_HAS_CODEC   -I/usr/include/x86_64-linux-gnu -isystem /usr/include/mit-krb5 -isystem /usr/include/mit-krb5 -I/usr/include/p11-kit-1.

Median per call from 7 timed batches.

Timing method: CLOCK_MONOTONIC; warmup=20ms; target_batch=250us; max_batch=512.

Argument parsing, output validation, disposal and database writes are outside timing.
Inputs show the actual operands, including +0.125i for permitted complex arguments; integer orders remain real.
`number-double` includes generic number dispatch; the other fixed profiles call qfloat/qcomplex directly.
Requested multiprecision bits do not guarantee that every algorithm achieves that accuracy.
Operations and precision columns with only unsupported results are omitted; — denotes an unsupported cell.
Failures and timeouts remain visible. † marks a retained slowdown outlier.

### number / double precision

| Operation | Inputs | 53 bits |
|---|---|---:|
| E1 | 2.345678 | 7.8 µs |
| Ei | 2.345678 | 7.8 µs |
| Li | 2.345 | 7.1 µs |
| abs | 0.7 | 8.7 ns |
| acos | 0.7 | 20.4 ns |
| acosec | 2.345 | 15.8 ns |
| acosech | 0.7 | 57.1 ns |
| acosh | 2.345 | 26.4 ns |
| acot | 0.7 | 40.9 ns |
| acoth | 2.345 | 24.1 ns |
| add | 0.7, 0.7 | 17.6 ns |
| add_long | 17, 3 | 510.5 ns |
| appell_f1 | 0.5, 0.7, 0.8, 2.5, 0.2, 0.3 | 44.9 µs |
| arccovercos | 0.5 | 92.4 ns |
| arccoversin | 0.5 | 90.1 ns |
| archacovercos | 0.5 | 204.5 ns |
| archacoversin | 0.5 | 198.4 ns |
| archavercos | 0.5 | 185.2 ns |
| archaversin | 0.5 | 177.6 ns |
| arcvercos | 0.5 | 93.6 ns |
| arcversin | 0.5 | 119.6 ns |
| arg | 0.7 | 61.7 ns |
| asec | 2.345 | 19.4 ns |
| asech | 0.7 | 35.8 ns |
| asin | 0.7 | 31.7 ns |
| asinh | 0.7 | 54.8 ns |
| atan | 0.7 | 31.1 ns |
| atan2 | 0.5, -0.75 | 35.0 ns |
| atanh | 0.7 | 22.8 ns |
| bessel_i | 2, 0.7 | 34.5 µs |
| bessel_j | 2, 0.7 | 5.6 µs |
| bessel_k | 2, 0.7 | 142.3 µs |
| bessel_y | 2, 0.7 | 16.6 µs |
| beta | 2.3, 4.5 | 116.0 ns |
| beta_pdf | 0.5, 2.5, 3.5 | 142.5 ns |
| binomial | 5, 2 | 145.4 ns |
| ceil | 0.7 | 143.6 ns |
| chebyshev_t | 2, 0.7 | 745.5 ns |
| chebyshev_u | 2, 0.7 | 1.2 µs |
| circ | 0.7 | 277.0 ns |
| clausen | 3, 0.7 | 419.0 µs † |
| clausen2 | 0.7 | 5.9 µs |
| conj | 0.7 | 9.2 ns |
| cos | 0.7 | 36.3 ns |
| cosec | 0.7 | 35.7 ns |
| cosech | 0.7 | 59.3 ns |
| cosh | 0.7 | 40.9 ns |
| cot | 0.7 | 42.8 ns |
| coth | 0.7 | 67.6 ns |
| covercos | 0.7 | 128.9 ns |
| coversin | 0.7 | 124.6 ns |
| cubrt | 0.7 | 6.0 µs |
| digamma | 2.345 | 3.5 µs † |
| dilog | 0.7 | 16.0 µs |
| div | 0.7, 0.7 | 12.5 ns |
| erf | 0.7 | 74.3 ns |
| erfc | 0.7 | 73.5 ns |
| erfcinv | 0.5 | 224.1 µs |
| erfinv | 0.5 | 188.8 µs |
| exp | 1.23456789 | 40.5 ns |
| floor | 0.7 | 9.2 ns |
| gamma | 2.345 | 87.7 ns |
| gammainc_P | 2.3, 1.4 | 11.5 µs |
| gammainc_Q | 2.3, 1.4 | 19.7 µs † |
| gammainc_lower | 2.3, 1.4 | 12.3 µs |
| gammainc_upper | 2.3, 1.4 | 15.6 µs |
| gammainv | 9.5 | 110.5 µs |
| hacovercos | 0.7 | 193.8 ns |
| hacoversin | 0.7 | 196.1 ns |
| harmonic_poly | 2, 0.7 | 1.6 µs |
| havercos | 0.7 | 239.1 ns |
| haversin | 0.7 | 190.5 ns |
| hermite_h | 2, 0.7 | 1.3 µs |
| hypergeometric_pFq | 0.7, 2.5, 0.3 | 4.4 µs |
| hypot | 0.7, 1.2 | 39.9 ns |
| imag_part | 0.7 | 18.9 ns |
| inv | 0.7 | 6.9 ns |
| lambert_w0 | 0.7 | 13.5 µs |
| lambert_wm1 | -0.2 | 13.1 µs |
| lambert_wn | 1, 0.7 | 23.4 µs |
| lauricella_f | 0.5, 0.3, 2.5 | 16.2 µs |
| ldexp | 1.23456789, 3 | 33.7 ns |
| legendre_chi | 2, 0.7 | 32.6 µs |
| lerch_phi | 0.3, 2, 1.4 | 242.0 µs |
| lg | 2.345678 | 20.1 ns |
| lgamma | 2.345 | 54.4 ns |
| ln | 2.345678 | 17.5 ns |
| log | 2.345678 | 12.1 ns |
| log10 | 2.345678 | 18.2 ns |
| logbeta | 2.3, 4.5 | 103.8 ns |
| logbeta_pdf | 0.5, 2.5, 3.5 | 132.3 ns |
| lommel_s | 0.5, 0.7, 1.2 | 7.2 µs |
| mul | 0.7, 0.7 | 32.4 ns |
| mul_long | 17, 3 | 381.3 ns |
| mul_pow10 | 1.23456789, 3 | 52.5 ns |
| neg | 0.7 | 7.2 ns |
| normal_cdf | 0.7 | 59.2 ns |
| normal_logpdf | 0.7 | 13.0 ns |
| normal_pdf | 0.7 | 19.8 ns |
| polygamma | 3, 2.345 | 20.6 µs |
| polylog | 2, 0.7 | 12.8 µs |
| polylog1 | 0.7 | 94.4 ns |
| pow | 1.23456789, 3.5 | 60.0 ns |
| pow10 | 3 | 212.1 ns |
| pow_int | 1.23456789, 3 | 67.0 ns |
| productlog | 0.7 | 13.7 µs |
| qdigamma | 0.5, 2.3 | 21.8 µs |
| real_part | 0.7 | 25.2 ns |
| rect | 0.7 | 216.4 ns |
| root | 1.2345, 3 | 6.2 µs |
| round | 0.7 | 315.6 ns |
| roundn | 1.23456789, 3 | 5.3 µs † |
| rounds | 1.23456789, 3 | 6.6 µs |
| sec | 0.7 | 32.6 ns |
| sech | 0.7 | 29.4 ns |
| sgn | 0.7 | 99.1 ns |
| sin | 0.567 | 28.4 ns |
| sinc | 0.7 | 3.5 µs |
| sincos | 0.7 | 480.8 ns |
| sinh | 0.7 | 47.0 ns |
| sinhcosh | 0.7 | 92.1 ns |
| sqr | 0.7 | 7.2 ns |
| sqrt | 1.23456789 | 9.9 ns |
| step | 0.7 | 113.1 ns |
| struve_h | 2, 0.7 | 54.5 µs |
| struve_l | 2, 0.7 | 54.6 µs |
| sub | 0.7, 0.7 | 10.0 ns |
| tan | 0.7 | 4.9 µs |
| tanh | 0.7 | 52.6 ns |
| tetragamma | 2.345 | 3.5 µs |
| tri | 0.7 | 203.5 ns |
| trigamma | 2.345 | 2.3 µs |
| vercos | 0.7 | 100.9 ns |
| versin | 0.7 | 102.1 ns |
| zatahp | 2.3, 1.4 | 321.7 µs |
| zeta | 2.345 | 326.9 µs |
| zetah | 2.3, 1.4 | 337.6 µs |
| zetap | 2.345 | 261.7 µs |

### number / multi-precision floating point

| Operation | Inputs | 256 bits | 512 bits | 768 bits | 1024 bits | 2048 bits | 4096 bits |
|---|---|---:|---:|---:|---:|---:|---:|
| E1 | 2.345678 | 31.3 µs | 59.2 µs | 125.8 µs | 176.0 µs | 445.3 µs | 1.725 ms |
| Ei | 2.345678 | 32.3 µs | 58.8 µs | 112.9 µs | 135.4 µs | 425.3 µs | 1.713 ms |
| Li | 2.345 | 79.7 µs † | 55.5 µs | 111.5 µs | 124.4 µs | 409.0 µs | 1.564 ms |
| abs | 0.7 | 69.8 ns | 67.8 ns | 69.1 ns | 70.9 ns | 71.0 ns | 78.5 ns |
| acos | 0.7 | 13.9 µs | 21.5 µs | 28.6 µs | 42.2 µs | 124.4 µs | 263.8 µs |
| acosec | 2.345 | 14.5 µs | 22.0 µs | 38.1 µs | 42.8 µs | 119.2 µs | 349.8 µs |
| acosech | 0.7 | 4.8 µs | 7.7 µs | 13.6 µs | 14.4 µs | 48.4 µs | 130.6 µs |
| acosh | 2.345 | 5.1 µs | 7.8 µs | 15.0 µs | 17.8 µs | 46.5 µs | 97.5 µs |
| acot | 0.7 | 11.6 µs | 24.6 µs | 32.2 µs | 39.0 µs | 122.3 µs | 259.3 µs |
| acoth | 2.345 | 5.2 µs | 8.2 µs | 15.4 µs | 15.4 µs | 44.7 µs | 108.5 µs |
| add | 0.7, 0.7 | 78.8 ns | 80.9 ns | 109.8 ns | 100.1 ns | 170.4 ns | 651.7 ns |
| add_long | 17, 3 | 498.7 ns | 513.5 ns | 700.7 ns | 527.9 ns | 720.5 ns | 629.0 ns |
| appell_f1 | 0.5, 0.7, 0.8, 2.5, 0.2, 0.3 | 202.8 µs | 517.7 µs | 1.066 ms | 1.383 ms | 7.481 ms | 35.083 ms |
| arccovercos | 0.5 | 15.2 µs | 20.1 µs | 25.8 µs | 37.2 µs | 106.3 µs | 243.7 µs |
| arccoversin | 0.5 | 12.1 µs | 20.0 µs | 25.0 µs | 37.0 µs | 105.3 µs | 307.4 µs |
| archacovercos | 0.5 | 1.1 µs | 1.2 µs | 1.6 µs | 1.1 µs | 1.2 µs | 1.3 µs |
| archacoversin | 0.5 | 1.1 µs | 1.1 µs | 2.9 µs † | 1.1 µs | 1.4 µs | 1.6 µs |
| archavercos | 0.5 | 1.1 µs | 1.2 µs | 1.7 µs | 1.5 µs | 1.2 µs | 1.7 µs |
| archaversin | 0.5 | 1.1 µs | 1.2 µs | 1.9 µs | 1.2 µs | 1.3 µs | 2.1 µs † |
| arcvercos | 0.5 | 12.2 µs | 20.2 µs | 33.1 µs | 36.7 µs | 93.8 µs | 308.2 µs |
| arcversin | 0.5 | 12.3 µs | 19.9 µs | 38.2 µs | 37.1 µs | 81.6 µs | 303.0 µs |
| arg | 0.7 | 399.4 ns | 405.7 ns | 512.2 ns | 415.2 ns | 525.4 ns | 441.0 ns |
| asec | 2.345 | 15.1 µs | 22.2 µs | 30.3 µs | 43.8 µs | 90.9 µs | 270.6 µs |
| asech | 0.7 | 5.3 µs | 8.2 µs | 14.8 µs | 19.0 µs | 35.5 µs | 105.8 µs |
| asin | 0.7 | 23.0 µs † | 27.1 µs | 36.3 µs | 44.2 µs | 245.3 µs † | 263.8 µs |
| asinh | 0.7 | 6.3 µs | 7.5 µs | 10.6 µs | 18.0 µs | 59.4 µs † | 149.5 µs † |
| atan | 0.7 | 10.8 µs | 26.4 µs | 30.5 µs | 35.3 µs | 87.1 µs | 317.2 µs |
| atan2 | 0.5, -0.75 | 14.5 µs | 24.2 µs | 28.0 µs | 96.2 µs † | 92.0 µs | 329.8 µs |
| atanh | 0.7 | 6.4 µs | 10.5 µs | 11.1 µs | 19.2 µs | 36.1 µs | 285.7 µs † |
| bessel_i | 2, 0.7 | 79.7 µs | 224.2 µs | 348.1 µs | 562.9 µs | 1.542 ms | 7.514 ms |
| bessel_j | 2, 0.7 | 5.8 µs | 18.4 µs | 21.6 µs | 34.2 µs | 142.5 µs | 873.2 µs |
| bessel_k | 2, 0.7 | 298.2 µs | 761.4 µs | 882.8 µs | 1.192 ms | 3.405 ms | 13.203 ms |
| bessel_y | 2, 0.7 | 48.3 µs | 169.0 µs † | 118.0 µs | 467.8 µs † | 851.4 µs | 4.011 ms |
| beta | 2.3, 4.5 | 89.5 µs | 300.2 µs | 385.5 µs | 622.5 µs | 2.415 ms | 13.004 ms |
| beta_pdf | 0.5, 2.5, 3.5 | 89.1 µs | 171.7 µs | 384.8 µs | 467.2 µs | 1.710 ms | 10.748 ms |
| binomial | 5, 2 | 115.7 µs | 280.3 µs | 481.8 µs | 645.0 µs | 2.971 ms | 12.000 ms |
| ceil | 0.7 | 486.8 ns | 467.4 ns | 431.2 ns | 381.3 ns | 410.8 ns | 560.3 ns |
| chebyshev_t | 2, 0.7 | 2.6 µs | 6.3 µs † | 3.2 µs | 2.3 µs | 2.8 µs | 6.1 µs |
| chebyshev_u | 2, 0.7 | 3.0 µs | 2.8 µs | 2.9 µs | 2.8 µs | 4.2 µs | 6.1 µs |
| circ | 0.7 | 859.7 ns | 1.9 µs † | 677.6 ns | 722.1 ns | 709.1 ns | 1.1 µs |
| clausen | 3, 0.7 | 1.331 ms | 5.257 ms | 10.510 ms | 22.032 ms | 145.463 ms | 986.164 ms |
| clausen2 | 0.7 | 1.346 ms | 5.149 ms | 10.507 ms | 22.351 ms | 151.191 ms | 1.002 s |
| conj | 0.7 | 81.6 ns | 94.1 ns | 93.2 ns | 66.1 ns | 65.8 ns | 126.5 ns |
| cos | 0.7 | 2.4 µs | 4.9 µs | 7.6 µs | 11.8 µs | 40.5 µs | 126.0 µs |
| cosec | 0.7 | 3.3 µs | 6.6 µs | 10.1 µs | 12.2 µs | 36.5 µs | 137.4 µs |
| cosech | 0.7 | 3.1 µs | 5.0 µs | 10.9 µs | 15.4 µs | 40.6 µs | 371.3 µs † |
| cosh | 0.7 | 3.0 µs | 4.7 µs | 10.7 µs | 12.3 µs | 39.4 µs | 180.5 µs |
| cot | 0.7 | 4.5 µs | 11.0 µs | 18.9 µs | 29.7 µs | 71.8 µs | 263.9 µs |
| coth | 0.7 | 6.2 µs | 12.1 µs | 20.9 µs | 31.6 µs | 106.7 µs | 288.1 µs |
| covercos | 0.7 | 3.9 µs | 6.9 µs | 22.3 µs † | 15.7 µs | 44.9 µs | 133.5 µs |
| coversin | 0.7 | 3.9 µs | 5.4 µs | 9.9 µs | 16.2 µs | 47.0 µs | 133.4 µs |
| cubrt | 0.7 | 28.6 µs † | 15.7 µs | 28.8 µs | 31.3 µs | 110.1 µs | 642.7 µs † |
| digamma | 2.345 | 119.4 µs | 285.6 µs | 658.7 µs | 797.0 µs | 4.862 ms | 17.217 ms |
| dilog | 0.7 | 164.8 µs † | 220.2 µs | 544.2 µs | 681.5 µs | 3.757 ms | 16.204 ms |
| div | 0.7, 0.7 | 339.2 ns | 559.9 ns | 882.4 ns | 1.0 µs | 2.8 µs | 8.6 µs |
| erf | 0.7 | 13.7 µs | 32.7 µs | 71.3 µs | 90.4 µs | 575.4 µs | 2.025 ms |
| erfc | 0.7 | 18.0 µs | 35.0 µs | 61.2 µs | 119.1 µs | 396.9 µs | 2.017 ms |
| erfcinv | 0.5 | 271.0 µs | 521.0 µs † | 600.3 µs † | 980.5 µs † | 2.728 ms † | 11.389 ms † |
| erfinv | 0.5 | 277.6 µs | 400.4 µs † | 834.2 µs † | 737.6 µs † | 2.670 ms † | 11.957 ms † |
| exp | 1.23456789 | 2.9 µs | 4.5 µs | 8.1 µs | 13.0 µs | 43.2 µs | 190.7 µs |
| floor | 0.7 | 92.5 ns | 67.5 ns | 70.4 ns | 69.0 ns | 71.8 ns | 103.1 ns |
| gamma | 2.345 | 35.3 µs | 70.8 µs | 126.3 µs | 269.3 µs | 860.0 µs | 4.271 ms |
| gammainc_P | 2.3, 1.4 | 111.0 µs | 204.2 µs | 361.2 µs | 778.3 µs | 2.681 ms | 14.079 ms |
| gammainc_Q | 2.3, 1.4 | 114.7 µs | 205.5 µs | 657.9 µs | 1.217 ms | 5.682 ms | 25.823 ms |
| gammainc_lower | 2.3, 1.4 | 61.8 µs | 163.6 µs | 226.0 µs | 469.5 µs | 2.365 ms | 8.397 ms |
| gammainc_upper | 2.3, 1.4 | 85.0 µs | 205.5 µs | 758.3 µs | 1.330 ms | 5.549 ms | 23.756 ms |
| gammainv | 9.5 | 1.145 ms † | 2.469 ms | 4.746 ms | 11.973 ms | 57.555 ms | 298.933 ms |
| hacovercos | 0.7 | 3.7 µs | 6.4 µs | 9.4 µs | 23.2 µs † | 50.7 µs | 179.8 µs |
| hacoversin | 0.7 | 3.7 µs | 6.2 µs | 9.4 µs | 17.7 µs | 50.0 µs | 166.5 µs |
| harmonic_poly | 2, 0.7 | 3.0 µs | 3.7 µs | 5.3 µs | 6.2 µs | 12.1 µs | 32.6 µs |
| havercos | 0.7 | 3.0 µs | 5.1 µs | 8.0 µs | 15.2 µs | 45.8 µs | 177.8 µs |
| haversin | 0.7 | 3.7 µs | 7.1 µs | 8.0 µs | 32.0 µs † | 44.3 µs | 179.1 µs |
| hermite_h | 2, 0.7 | 2.7 µs | 3.1 µs | 3.1 µs | 7.6 µs † | 5.3 µs | 7.1 µs |
| hypergeometric_pFq | 0.7, 2.5, 0.3 | 3.8 µs | 5.0 µs | 3.8 µs | 4.8 µs | 5.0 µs | 5.0 µs |
| hypot | 0.7, 1.2 | 534.2 ns | 945.7 ns | 945.2 ns | 1.6 µs | 3.3 µs | 7.2 µs |
| imag_part | 0.7 | 63.2 ns | 82.3 ns | 61.6 ns | 85.2 ns | 90.9 ns | 62.6 ns |
| inv | 0.7 | 169.7 ns | 249.1 ns | 321.7 ns | 561.5 ns | 1.4 µs | 3.3 µs |
| lambert_w0 | 0.7 | 130.7 µs † | 102.3 µs | 160.9 µs | 224.7 µs | 894.7 µs | 3.252 ms |
| lambert_wm1 | -0.2 | 49.9 µs | 102.0 µs | 155.8 µs | 449.1 µs † | 1.034 ms | 2.708 ms |
| lambert_wn | 1, 0.7 | 24.1 µs | 30.8 µs | 24.1 µs | 24.4 µs | 32.1 µs | 24.8 µs |
| lauricella_f | 0.5, 0.3, 2.5 | 32.2 µs † | 17.6 µs | 16.5 µs | 16.8 µs | 19.1 µs | 16.7 µs |
| ldexp | 1.23456789, 3 | 98.6 ns | 121.0 ns | 95.5 ns | 129.1 ns | 226.9 ns | 101.7 ns |
| legendre_chi | 2, 0.7 | 183.8 µs | 574.3 µs | 864.4 µs | 2.055 ms | 6.697 ms | 36.818 ms |
| lerch_phi | 0.3, 2, 1.4 | 242.5 µs | 308.7 µs | 243.2 µs | 312.9 µs | 306.1 µs | 306.4 µs |
| lg | 2.345678 | 12.4 µs | 18.1 µs | 20.7 µs | 34.1 µs | 89.0 µs | 241.2 µs |
| lgamma | 2.345 | 29.0 µs | 93.6 µs | 159.3 µs | 268.7 µs | 879.3 µs | 3.737 ms |
| ln | 2.345678 | 4.1 µs | 6.8 µs | 9.3 µs | 17.9 µs | 42.8 µs | 110.7 µs |
| log | 2.345678 | 5.1 µs | 8.5 µs | 9.3 µs | 17.9 µs | 43.8 µs | 93.5 µs |
| log10 | 2.345678 | 8.9 µs | 14.3 µs | 20.3 µs | 36.0 µs | 83.9 µs | 190.1 µs |
| logbeta | 2.3, 4.5 | 118.7 µs | 237.2 µs | 397.2 µs | 826.1 µs | 3.271 ms | 14.394 ms |
| logbeta_pdf | 0.5, 2.5, 3.5 | 73.8 µs | 167.8 µs | 312.7 µs | 469.6 µs | 2.093 ms | 9.682 ms |
| lommel_s | 0.5, 0.7, 1.2 | 23.6 µs | 55.5 µs | 101.1 µs | 174.6 µs | 812.5 µs | 3.589 ms |
| mul | 0.7, 0.7 | 94.5 ns | 181.0 ns | 224.1 ns | 352.8 ns | 1.0 µs | 2.8 µs |
| mul_long | 17, 3 | 597.8 ns | 608.5 ns | 904.5 ns | 830.6 ns | 576.3 ns | 652.5 ns |
| mul_pow10 | 1.23456789, 3 | 278.4 ns | 354.2 ns | 508.5 ns | 652.6 ns | 363.7 ns | 691.2 ns |
| neg | 0.7 | 72.4 ns | 82.5 ns | 71.6 ns | 93.3 ns | 75.0 ns | 102.2 ns |
| normal_cdf | 0.7 | 12.9 µs | 66.1 µs | 51.8 µs | 109.1 µs | 381.0 µs | 1.701 ms |
| normal_logpdf | 0.7 | 4.0 µs | 9.2 µs | 10.0 µs | 17.3 µs | 70.8 µs † | 105.7 µs |
| normal_pdf | 0.7 | 2.9 µs | 7.4 µs | 9.2 µs | 19.4 µs | 47.1 µs | 169.1 µs |
| polygamma | 3, 2.345 | 141.3 µs | 322.3 µs | 687.9 µs | 1.184 ms | 7.759 ms | 100.409 ms |
| polylog | 2, 0.7 | 85.2 µs | 213.1 µs | 400.7 µs | 884.3 µs | 3.749 ms | 17.572 ms |
| polylog1 | 0.7 | 4.6 µs | 7.4 µs | 10.1 µs | 15.2 µs | 42.9 µs | 126.3 µs |
| pow | 1.23456789, 3.5 | 7.3 µs | 12.5 µs | 19.1 µs | 29.2 µs | 93.2 µs | 311.6 µs |
| pow10 | 3 | 192.0 ns | 240.0 ns | 301.8 ns | 1.1 µs † | 347.4 ns | 462.4 ns |
| pow_int | 1.23456789, 3 | 223.7 ns | 325.5 ns | 461.2 ns | 762.4 ns | 2.6 µs | 7.5 µs |
| productlog | 0.7 | 42.6 µs | 81.2 µs | 137.9 µs | 278.5 µs | 913.2 µs | 2.866 ms |
| qdigamma | 0.5, 2.3 | 22.0 µs | 22.2 µs | 22.0 µs | 27.9 µs | 34.4 µs † | 28.2 µs |
| real_part | 0.7 | 65.2 ns | 63.3 ns | 65.1 ns | 86.0 ns | 86.0 ns | 94.5 ns |
| rect | 0.7 | 714.3 ns | 757.7 ns | 769.1 ns | 961.1 ns | 1.0 µs | 1.2 µs |
| root | 1.2345, 3 | 11.4 µs | 16.4 µs | 22.8 µs | 40.2 µs | 201.5 µs † | 307.5 µs |
| round | 0.7 | 1.9 µs | 1.9 µs | 1.8 µs | 1.9 µs | 2.4 µs | 2.6 µs |
| roundn | 1.23456789, 3 | 4.6 µs | 5.2 µs | 4.1 µs | 4.7 µs | 5.3 µs | 10.2 µs |
| rounds | 1.23456789, 3 | 19.9 µs | 21.4 µs | 76.2 µs † | 34.5 µs | 75.1 µs | 201.0 µs |
| sec | 0.7 | 2.0 µs | 5.2 µs | 6.7 µs | 26.1 µs † | 33.2 µs | 163.2 µs |
| sech | 0.7 | 2.4 µs | 4.9 µs | 8.4 µs | 12.8 µs | 112.0 µs † | 160.1 µs |
| sgn | 0.7 | 179.6 ns | 176.3 ns | 444.3 ns | 178.1 ns | 178.1 ns | 229.9 ns |
| sin | 0.567 | 3.3 µs | 4.7 µs | 7.6 µs | 11.8 µs | 88.5 µs † | 145.5 µs |
| sinc | 0.7 | 4.1 µs | 6.7 µs | 9.9 µs | 14.5 µs | 52.0 µs | 186.9 µs |
| sincos | 0.7 | 7.9 µs | 10.1 µs | 13.2 µs | 17.1 µs | 41.4 µs | 188.0 µs |
| sinh | 0.7 | 2.4 µs | 4.8 µs | 19.7 µs † | 12.2 µs | 50.2 µs | 180.9 µs |
| sinhcosh | 0.7 | 2.5 µs | 5.0 µs | 8.4 µs | 12.4 µs | 51.8 µs | 179.9 µs |
| sqr | 0.7 | 173.4 ns | 218.3 ns | 248.1 ns | 364.8 ns | 634.6 ns | 1.5 µs |
| sqrt | 1.23456789 | 259.9 ns | 364.1 ns | 593.2 ns | 565.2 ns | 1.1 µs | 2.4 µs |
| step | 0.7 | 185.5 ns | 202.4 ns | 179.3 ns | 330.0 ns | 177.0 ns | 243.3 ns |
| struve_h | 2, 0.7 | 124.3 µs | 270.9 µs | 430.0 µs | 691.9 µs | 3.042 ms | 12.529 ms |
| struve_l | 2, 0.7 | 124.2 µs | 270.0 µs | 422.7 µs | 655.7 µs | 2.509 ms | 12.749 ms |
| sub | 0.7, 0.7 | 74.6 ns | 67.6 ns | 83.0 ns | 202.9 ns | 113.7 ns | 213.6 ns |
| tan | 0.7 | 9.1 µs | 11.4 µs | 14.6 µs | 19.0 µs | 47.3 µs | 189.7 µs |
| tanh | 0.7 | 2.4 µs | 13.3 µs † | 16.2 µs † | 12.5 µs | 43.9 µs | 187.1 µs |
| tetragamma | 2.345 | 110.1 µs | 426.1 µs † | 947.3 µs † | 1.305 ms † | 7.346 ms † | 108.400 ms † |
| tri | 0.7 | 705.4 ns | 1.1 µs | 712.9 ns | 911.2 ns | 946.7 ns | 1.1 µs |
| trigamma | 2.345 | 75.8 µs | 224.7 µs | 453.1 µs † | 848.1 µs † | 4.811 ms † | 82.123 ms † |
| vercos | 0.7 | 2.9 µs | 4.1 µs | 7.0 µs | 10.5 µs | 42.2 µs | 149.8 µs |
| versin | 0.7 | 2.2 µs | 4.0 µs | 6.8 µs | 10.5 µs | 32.6 µs | 126.7 µs |
| zatahp | 2.3, 1.4 | 1.359 ms | 2.557 ms | 5.027 ms | 9.361 ms | 88.971 ms | 1.010 s |
| zeta | 2.345 | 739.9 µs | 3.248 ms | 7.931 ms | 19.826 ms | 117.768 ms | 691.499 ms |
| zetah | 2.3, 1.4 | 1.300 ms | 2.623 ms | 4.826 ms | 8.846 ms | 85.575 ms | 872.560 ms |
| zetap | 2.345 | 2.322 ms | 5.231 ms | 8.900 ms | 15.359 ms | 110.676 ms | 1.170 s |

### number / multi-precision complex floating point

| Operation | Inputs | 256 bits | 512 bits | 768 bits | 1024 bits | 2048 bits | 4096 bits |
|---|---|---:|---:|---:|---:|---:|---:|
| E1 | 2.345678+0.125i | 175.3 µs | 379.4 µs | 546.8 µs | 1.292 ms | 4.276 ms | 16.771 ms |
| Ei | 2.345678+0.125i | 162.5 µs | 379.1 µs | 697.4 µs | 1.171 ms | 4.449 ms | 16.740 ms |
| Li | 2.345+0.125i | 115.6 µs | 463.3 µs † | 465.7 µs | 1.050 ms | 3.545 ms | 17.322 ms |
| abs | 0.7+0.125i | 842.1 ns | 1.3 µs | 1.1 µs | 1.6 µs | 2.8 µs | 6.6 µs |
| acos | 0.7+0.125i | 58.3 µs | 91.9 µs | 89.6 µs | 83.0 µs | 183.5 µs | 496.5 µs |
| acosec | 2.345+0.125i | 210.1 µs | 1.383 ms † | 935.8 µs | 2.452 ms | 10.564 ms | 46.571 ms |
| acosech | 0.7+0.125i | 39.6 µs | 84.7 µs | 199.7 µs † | 153.8 µs | 326.2 µs | 950.5 µs |
| acosh | 2.345+0.125i | 46.2 µs | 87.1 µs | 92.6 µs | 77.0 µs | 254.2 µs | 458.5 µs |
| acot | 0.7+0.125i | 50.5 µs | 85.9 µs | 97.9 µs | 150.3 µs | 278.4 µs | 962.1 µs |
| acoth | 2.345+0.125i | 35.1 µs | 64.4 µs | 76.1 µs | 116.8 µs | 210.9 µs | 774.6 µs |
| add | 0.7+0.125i, 0.7+0.125i | 363.6 ns | 424.6 ns | 391.3 ns | 310.4 ns | 340.7 ns | 432.5 ns |
| add_long | 17+0.125i, 3 | 2.5 µs | 5.4 µs † | 2.2 µs | 3.5 µs | 2.0 µs | 2.1 µs |
| appell_f1 | 0.5+0.125i, 0.7+0.125i, 0.8+0.125i, 2.5+0.125i, 0.2+0.125i, 0.3+0.125i | 1.189 ms | 3.656 ms | 6.156 ms | 11.562 ms | 40.844 ms | 193.985 ms |
| arccovercos | 0.5+0.125i | 56.0 µs | 81.6 µs | 54.0 µs | 146.4 µs | 321.4 µs † | 355.9 µs |
| arccoversin | 0.5+0.125i | 42.7 µs | 35.3 µs | 49.3 µs | 156.1 µs | 136.5 µs | 353.2 µs |
| archacovercos | 0.5+0.125i | 12.6 µs | 27.5 µs † | 21.0 µs | 24.4 µs | 46.0 µs | 101.7 µs |
| archacoversin | 0.5+0.125i | 26.8 µs † | 16.4 µs | 16.4 µs | 23.7 µs | 44.8 µs | 102.2 µs |
| archavercos | 0.5+0.125i | 10.0 µs | 16.4 µs | 16.4 µs | 24.8 µs | 42.0 µs | 101.8 µs |
| archaversin | 0.5+0.125i | 12.4 µs | 20.2 µs † | 16.3 µs | 25.0 µs | 50.9 µs | 129.7 µs |
| arcvercos | 0.5+0.125i | 55.8 µs | 37.9 µs | 87.4 µs | 74.8 µs | 169.0 µs | 463.7 µs |
| arcversin | 0.5+0.125i | 25.1 µs | 43.8 µs | 107.3 µs | 81.9 µs | 169.3 µs | 437.8 µs |
| arg | 0.7+0.125i | 13.8 µs | 29.7 µs | 37.3 µs | 37.0 µs | 112.2 µs | 312.5 µs |
| asec | 2.345+0.125i | 270.2 µs † | 603.2 µs | 1.234 ms | 2.223 ms | 9.771 ms | 47.318 ms |
| asech | 0.7+0.125i | 62.4 µs † | 35.9 µs | 54.2 µs | 63.4 µs | 184.3 µs | 412.8 µs |
| asin | 0.7+0.125i | 78.2 µs † | 192.7 µs † | 97.6 µs | 156.1 µs | 351.1 µs | 435.3 µs |
| asinh | 0.7+0.125i | 37.3 µs | 158.7 µs † | 99.3 µs | 110.9 µs | 338.6 µs | 764.6 µs |
| atan | 0.7+0.125i | 47.2 µs | 77.9 µs | 77.9 µs | 107.8 µs | 327.0 µs | 745.8 µs |
| atan2 | 0.5+0.125i, -0.75+0.125i | 11.2 µs | 9.1 µs | 10.4 µs | 9.1 µs | 11.6 µs | 9.0 µs |
| atanh | 0.7+0.125i | 45.8 µs | 67.5 µs | 72.5 µs | 126.2 µs | 252.3 µs | 803.6 µs |
| bessel_i | 2, 0.7+0.125i | 238.7 µs † | 458.5 µs | 708.6 µs | 1.126 ms | 3.625 ms | 18.437 ms |
| bessel_j | 2, 0.7+0.125i | 246.1 µs † | 395.5 µs | 785.4 µs | 1.006 ms | 4.086 ms | 17.274 ms |
| bessel_k | 2, 0.7+0.125i | 2.743 ms | 5.110 ms | 7.501 ms | 9.752 ms | 30.079 ms | 106.762 ms |
| bessel_y | 2, 0.7+0.125i | 219.8 µs † | 376.7 µs | 676.5 µs | 1.015 ms | 4.449 ms | 17.442 ms |
| beta | 2.3+0.125i, 4.5+0.125i | 49.9 µs † | 32.4 µs | 32.4 µs | 34.2 µs | 41.0 µs | 32.6 µs |
| beta_pdf | 0.5+0.125i, 2.5+0.125i, 3.5+0.125i | 60.8 µs | 116.4 µs † | 59.7 µs | 48.6 µs | 60.1 µs | 47.4 µs |
| binomial | 5+0.125i, 2+0.125i | 30.8 µs | 31.2 µs | 31.8 µs | 28.3 µs | 30.8 µs | 24.2 µs |
| ceil | 0.7+0.125i | 2.1 µs † | 1.4 µs | 1.6 µs | 1.1 µs | 1.2 µs | 1.1 µs |
| chebyshev_t | 2, 0.7+0.125i | 7.4 µs | 9.5 µs | 9.1 µs | 8.3 µs | 12.5 µs | 13.0 µs |
| chebyshev_u | 2, 0.7+0.125i | 13.6 µs | 28.2 µs † | 14.0 µs | 10.5 µs | 15.8 µs | 15.1 µs |
| circ | 0.7 | 1.9 µs | 1.8 µs | 2.6 µs | 2.1 µs | 4.2 µs | 7.9 µs |
| clausen | 3, 0.7+0.125i | 1.318 ms | 5.436 ms | 13.188 ms | 25.447 ms | 159.600 ms | 1.001 s |
| clausen2 | 0.7+0.125i | 1.490 ms | 5.670 ms | 12.402 ms | 24.261 ms | 159.577 ms | 1.001 s |
| conj | 0.7+0.125i | 228.0 ns | 302.0 ns | 451.4 ns | 222.1 ns | 219.6 ns | 503.3 ns |
| cos | 0.7+0.125i | 17.0 µs | 14.2 µs | 26.9 µs | 36.6 µs | 83.1 µs | 394.3 µs |
| cosec | 0.7+0.125i | 21.2 µs | 38.3 µs † | 29.7 µs | 40.7 µs | 88.6 µs | 416.3 µs |
| cosech | 0.7+0.125i | 12.1 µs | 17.7 µs | 30.8 µs † | 89.0 µs | 70.6 µs | 300.3 µs |
| cosh | 0.7+0.125i | 9.3 µs | 14.3 µs | 23.2 µs | 69.8 µs | 65.6 µs | 290.8 µs |
| cot | 0.7+0.125i | 47.9 µs † | 75.9 µs † | 54.8 µs | 76.8 µs | 173.9 µs | 772.4 µs |
| coth | 0.7+0.125i | 19.1 µs | 25.1 µs | 79.2 µs † | 151.0 µs | 138.6 µs | 592.4 µs |
| covercos | 0.7+0.125i | 39.3 µs † | 18.4 µs | 25.0 µs | 32.4 µs | 85.7 µs | 381.7 µs |
| coversin | 0.7+0.125i | 20.2 µs | 19.6 µs | 57.4 µs † | 41.3 µs | 85.6 µs | 354.7 µs |
| cubrt | 0.7+0.125i | 88.9 µs † | 68.2 µs | 99.5 µs | 138.6 µs | 311.0 µs | 1.322 ms |
| digamma | 2.345+0.125i | 718.2 µs | 1.529 ms | 3.072 ms | 4.764 ms | 12.199 ms | 127.862 ms |
| dilog | 0.7+0.125i | 343.3 µs | 1.232 ms | 2.431 ms | 4.081 ms | 16.600 ms | 93.743 ms |
| div | 0.7+0.125i, 0.7+0.125i | 11.0 µs † | 26.8 µs † | 17.0 µs | 18.3 µs | 78.7 µs † | 112.9 µs |
| erf | 0.7+0.125i | 303.4 µs † | 358.4 µs † | 734.1 µs † | 834.4 µs † | 3.032 ms † | 14.910 ms † |
| erfc | 0.7+0.125i | 206.6 µs † | 292.1 µs † | 614.7 µs † | 959.2 µs † | 3.080 ms † | 14.572 ms † |
| erfcinv | 0.5+0.125i | 1.200 ms | 2.882 ms | 4.403 ms | 7.270 ms | 26.087 ms | 139.874 ms |
| erfinv | 0.5+0.125i | 1.333 ms † | 2.807 ms † | 4.125 ms † | 7.524 ms † | 23.547 ms † | 134.729 ms † |
| exp | 1.23456789+0.125i | 11.7 µs | 42.1 µs † | 28.9 µs | 54.2 µs | 178.2 µs | 485.0 µs |
| floor | 0.7+0.125i | 678.7 ns | 298.0 ns | 273.2 ns | 213.1 ns | 227.9 ns | 229.4 ns |
| gamma | 2.345+0.125i | 15.6 µs | 17.4 µs | 15.0 µs | 14.8 µs | 12.0 µs | 15.1 µs |
| gammainc_P | 2.3+0.125i, 1.4+0.125i | 69.3 µs | 136.5 µs † | 73.2 µs | 70.0 µs | 63.3 µs | 55.0 µs |
| gammainc_Q | 2.3+0.125i, 1.4+0.125i | 119.5 µs † | 70.5 µs | 71.5 µs | 68.8 µs | 54.9 µs | 55.2 µs |
| gammainc_lower | 2.3+0.125i, 1.4+0.125i | 64.6 µs † | 50.9 µs | 45.0 µs | 58.0 µs | 54.2 µs | 54.1 µs |
| gammainc_upper | 2.3+0.125i, 1.4+0.125i | 152.0 µs † | 56.4 µs | 69.5 µs | 67.6 µs | 57.4 µs | 68.9 µs |
| gammainv | 9.5+0.125i | 153.4 µs | 152.3 µs | 300.9 µs † | 121.0 µs | 122.0 µs | 120.9 µs |
| hacovercos | 0.7+0.125i | 24.0 µs | 50.3 µs † | 26.5 µs | 35.1 µs | 118.8 µs | 413.7 µs |
| hacoversin | 0.7+0.125i | 25.5 µs | 17.7 µs | 32.8 µs | 43.6 µs | 120.7 µs | 412.3 µs |
| harmonic_poly | 2, 0.7+0.125i | 16.0 µs | 16.3 µs | 16.0 µs | 16.5 µs | 27.4 µs | 63.2 µs |
| havercos | 0.7+0.125i | 25.4 µs | 17.8 µs | 25.0 µs | 35.4 µs | 96.2 µs | 366.2 µs |
| haversin | 0.7+0.125i | 25.5 µs | 22.9 µs | 25.0 µs | 38.9 µs | 99.2 µs | 401.0 µs |
| hermite_h | 2, 0.7+0.125i | 15.5 µs † | 12.7 µs | 13.5 µs | 10.9 µs | 12.7 µs | 22.3 µs |
| hypergeometric_pFq | 0.7+0.125i, 2.5+0.125i, 0.3+0.125i | 40.6 µs | 36.5 µs | 36.7 µs | 35.1 µs | 28.8 µs | 32.7 µs |
| hypot | 0.7+0.125i, 1.2+0.125i | 2.8 µs † | 3.8 µs † | 1.7 µs | 1.8 µs | 2.0 µs | 2.7 µs |
| imag_part | 0.7+0.125i | 110.3 ns | 142.9 ns | 125.7 ns | 96.4 ns | 100.4 ns | 139.1 ns |
| inv | 0.7+0.125i | 1.2 µs | 1.9 µs † | 1.9 µs | 1.9 µs | 3.8 µs | 9.5 µs |
| lambert_w0 | 0.7+0.125i | 55.0 µs | 67.9 µs | 91.8 µs | 94.5 µs | 243.4 µs | 994.1 µs |
| lambert_wm1 | -0.2+0.125i | 52.0 µs | 67.5 µs | 137.0 µs † | 110.3 µs | 239.6 µs | 783.5 µs |
| lambert_wn | 1, 0.7+0.125i | 28.3 µs | 26.9 µs | 27.3 µs | 25.7 µs | 25.8 µs | 25.9 µs |
| lauricella_f | 0.5+0.125i, 0.3+0.125i, 2.5+0.125i | 174.4 µs | 137.3 µs | 176.7 µs | 174.9 µs | 217.0 µs † | 145.4 µs |
| ldexp | 1.23456789+0.125i, 3 | 563.8 ns | 313.4 ns | 240.1 ns | 250.6 ns | 305.5 ns | 257.1 ns |
| legendre_chi | 2, 0.7+0.125i | 1.880 ms | 4.883 ms | 8.592 ms | 13.877 ms | 65.145 ms | 336.717 ms |
| lerch_phi | 0.3+0.125i, 2+0.125i, 1.4+0.125i | 1.103 ms † | 645.7 µs | 654.8 µs | 527.2 µs | 639.0 µs | 507.7 µs |
| lg | 2.345678+0.125i | 26.6 µs | 44.8 µs | 60.1 µs | 61.9 µs | 191.9 µs | 441.1 µs |
| lgamma | 2.345+0.125i | 26.0 µs † | 24.5 µs † | 12.9 µs | 12.7 µs | 12.6 µs | 12.0 µs |
| ln | 2.345678+0.125i | 19.9 µs | 25.5 µs | 31.3 µs | 46.1 µs | 122.9 µs | 357.6 µs |
| log | 2.345678+0.125i | 20.7 µs | 25.3 µs | 33.7 µs | 45.8 µs | 136.5 µs | 334.8 µs |
| log10 | 2.345678+0.125i | 55.5 µs † | 33.8 µs | 58.3 µs | 79.0 µs | 190.5 µs | 498.7 µs |
| logbeta | 2.3+0.125i, 4.5+0.125i | 37.5 µs | 29.7 µs | 29.6 µs | 37.4 µs | 30.4 µs | 37.3 µs |
| logbeta_pdf | 0.5+0.125i, 2.5+0.125i, 3.5+0.125i | 48.4 µs | 40.5 µs | 97.2 µs † | 47.9 µs | 40.7 µs | 48.5 µs |
| lommel_s | 0.5+0.125i, 0.7+0.125i, 1.2+0.125i | 257.8 µs | 402.7 µs | 630.7 µs | 1.295 ms | 3.999 ms | 18.828 ms |
| mul | 0.7+0.125i, 0.7+0.125i | 1.5 µs | 1.3 µs | 1.7 µs | 2.8 µs | 3.7 µs | 7.0 µs |
| mul_long | 17+0.125i, 3 | 3.3 µs | 2.5 µs | 3.1 µs | 2.8 µs | 2.5 µs | 2.7 µs |
| mul_pow10 | 1.23456789+0.125i, 3 | 2.9 µs | 2.4 µs | 2.6 µs | 2.4 µs | 2.3 µs | 2.6 µs |
| neg | 0.7+0.125i | 295.7 ns | 224.2 ns | 221.8 ns | 248.8 ns | 223.3 ns | 291.4 ns |
| normal_cdf | 0.7+0.125i | 168.3 µs † | 278.5 µs † | 639.8 µs † | 800.3 µs † | 2.907 ms † | 17.297 ms † |
| normal_logpdf | 0.7+0.125i | 8.7 µs † | 9.5 µs † | 15.6 µs † | 15.5 µs † | 35.3 µs † | 123.7 µs † |
| normal_pdf | 0.7+0.125i | 23.4 µs † | 34.6 µs † | 63.2 µs † | 66.7 µs † | 205.3 µs † | 959.7 µs † |
| polygamma | 3, 2.345+0.125i | 1.302 ms | 2.404 ms | 4.764 ms | 6.060 ms | 23.295 ms | 176.331 ms |
| polylog | 2, 0.7+0.125i | 430.2 µs | 1.570 ms † | 2.137 ms | 3.340 ms | 16.910 ms | 91.256 ms |
| polylog1 | 0.7+0.125i | 29.2 µs | 46.2 µs | 79.7 µs | 58.6 µs | 132.6 µs | 473.1 µs |
| pow | 1.23456789+0.125i, 3.5+0.125i | 64.9 µs † | 62.7 µs | 97.1 µs | 105.2 µs | 392.9 µs | 1.033 ms |
| pow10 | 3 | 11.3 µs | 18.5 µs | 22.9 µs | 30.1 µs | 53.5 µs | 152.2 µs |
| pow_int | 1.23456789+0.125i, 3 | 17.0 µs † | 7.8 µs | 6.1 µs | 6.9 µs | 9.7 µs | 19.4 µs |
| productlog | 0.7+0.125i | 111.1 µs † | 67.8 µs | 71.1 µs | 95.0 µs | 327.5 µs | 895.1 µs |
| qdigamma | 0.5+0.125i, 2.3+0.125i | 141.6 µs | 154.2 µs | 181.0 µs | 151.5 µs | 182.6 µs | 171.2 µs |
| real_part | 0.7+0.125i | 127.1 ns | 284.0 ns | 101.0 ns | 95.9 ns | 101.7 ns | 146.9 ns |
| rect | 0.7 | 5.4 µs † | 2.5 µs | 3.5 µs † | 2.4 µs | 4.2 µs | 9.0 µs |
| root | 1.2345+0.125i, 3 | 33.0 µs | 118.9 µs † | 203.4 µs | 284.7 µs | 392.9 µs | 1.483 ms |
| round | 0.7+0.125i | 9.4 µs | 7.3 µs | 21.8 µs † | 7.1 µs | 7.9 µs | 10.6 µs |
| roundn | 1.23456789+0.125i, 3 | 36.4 µs † | 12.0 µs | 17.5 µs | 12.6 µs | 15.8 µs | 31.6 µs |
| rounds | 1.23456789+0.125i, 3 | 74.5 µs † | 62.4 µs | 89.7 µs | 95.8 µs | 159.4 µs | 536.0 µs |
| sec | 0.7+0.125i | 18.8 µs | 39.1 µs † | 29.0 µs | 32.3 µs | 89.6 µs | 390.0 µs |
| sech | 0.7+0.125i | 12.1 µs | 17.7 µs | 26.3 µs | 90.3 µs | 70.9 µs | 302.2 µs |
| sgn | 0.7 | 750.1 ns | 1.3 µs † | 888.7 ns | 623.9 ns | 635.6 ns | 669.0 ns |
| sin | 0.567+0.125i | 20.5 µs † | 13.2 µs | 19.2 µs | 28.8 µs | 106.1 µs | 293.8 µs |
| sinc | 0.7+0.125i | 21.0 µs | 21.1 µs | 75.5 µs † | 51.7 µs | 102.2 µs | 336.8 µs |
| sinh | 0.7+0.125i | 9.3 µs | 25.7 µs † | 38.6 µs † | 68.8 µs | 65.6 µs | 225.2 µs |
| sqr | 0.7+0.125i | 1.5 µs | 1.7 µs | 2.1 µs | 2.2 µs | 3.3 µs | 5.6 µs |
| sqrt | 1.23456789+0.125i | 2.7 µs | 2.2 µs | 3.3 µs | 3.2 µs | 5.0 µs | 14.5 µs |
| step | 0.7 | 948.7 ns | 777.4 ns | 1.5 µs † | 625.6 ns | 625.6 ns | 628.0 ns |
| struve_h | 2, 0.7+0.125i | 659.5 µs † | 592.7 µs | 1.079 ms | 1.147 ms | 4.362 ms | 18.497 ms |
| struve_l | 2, 0.7+0.125i | 268.8 µs | 559.2 µs | 955.0 µs | 1.170 ms | 4.086 ms | 21.965 ms |
| sub | 0.7+0.125i, 0.7+0.125i | 427.4 ns | 371.8 ns | 304.4 ns | 284.2 ns | 313.4 ns | 578.8 ns |
| tan | 0.7+0.125i | 14.7 µs | 51.6 µs † | 23.8 µs | 34.7 µs | 100.2 µs | 412.0 µs |
| tanh | 0.7+0.125i | 30.6 µs | 20.6 µs | 94.9 µs † | 33.4 µs | 77.3 µs | 321.7 µs |
| tetragamma | 2.345+0.125i | 1.204 ms † | 2.962 ms † | 4.341 ms † | 7.242 ms † | 25.777 ms † | 179.401 ms † |
| tri | 0.7 | 2.3 µs | 2.4 µs | 2.2 µs | 3.0 µs | 3.4 µs | 6.3 µs |
| trigamma | 2.345+0.125i | 945.0 µs † | 2.400 ms † | 3.702 ms † | 4.796 ms † | 19.422 ms † | 137.650 ms † |
| vercos | 0.7+0.125i | 22.8 µs | 19.3 µs | 21.3 µs | 82.8 µs † | 85.9 µs | 298.1 µs |
| versin | 0.7+0.125i | 20.5 µs | 19.8 µs | 27.3 µs | 39.0 µs | 95.1 µs | 299.1 µs |
| zatahp | 2.3+0.125i, 1.4+0.125i | 6.193 ms | 10.345 ms | 18.520 ms | 32.955 ms | 174.526 ms | 1.346 s |
| zeta | 2.345+0.125i | 4.857 ms | 8.309 ms | 18.160 ms | 28.883 ms | 150.910 ms | 1.387 s |
| zetah | 2.3+0.125i, 1.4+0.125i | 5.194 ms | 9.356 ms | 14.706 ms | 28.410 ms | 152.053 ms | 1.510 s |
| zetap | 2.345+0.125i | 4.655 ms | 7.432 ms | 19.859 ms | 34.716 ms | 164.699 ms | 1.449 s |

### number / multi-precision integer

| Operation | Inputs | Exact |
|---|---|---:|
| bit_and | 17, 5 | 209.5 ns |
| bit_length | 17 | 139.0 ns |
| bit_not | 17 | 191.8 ns |
| bit_or | 17, 5 | 192.8 ns |
| bit_xor | 17, 5 | 213.3 ns |
| clear_bit | 17, 3 | 263.4 ns |
| divmod | 17, 5 | 123.6 ns |
| factorial | 17 | 222.5 ns |
| factors | 17 | 255.8 ns |
| fibonacci | 17 | 173.1 ns |
| gcd | 17, 5 | 76.4 ns |
| gcdext | 17, 5 | 490.2 ns |
| is_prime | 17 | 121.3 ns |
| isqrt | 17 | 101.5 ns |
| lcm | 17, 5 | 69.9 ns |
| mod | 17, 5 | 81.7 ns |
| modinv | 17, 5 | 236.9 ns |
| next_prime | 17 | 123.2 ns |
| partition | 17 | 1.3 µs |
| powmod | 7, 5, 13 | 152.2 ns |
| prev_prime | 17 | 223.5 ns |
| prove_prime | 17 | 111.4 ns |
| set_bit | 17, 3 | 238.5 ns |
| shl | 17, 3 | 186.8 ns |
| shr | 17, 3 | 252.3 ns |
| test_bit | 17, 3 | 266.9 ns |

### number / multi-precision fraction

`number_t` represents exact fractions using GMP rational arithmetic (MPQ). Addition, subtraction,
multiplication, division and integer powers retain exact rational results within the configured
size budget. Their cost depends on numerator and denominator sizes and fraction reduction,
rather than floating-point precision alone. General transcendental functions usually require
inexact real or complex results.

The current benchmark run has no separate fraction measurements. A meaningful fraction table
would specify numerator and denominator bit lengths and distinguish exact arithmetic from
conversion to floating point; the floating-point tables above do not measure that workload.

<!-- MARS-BENCHMARKS:END -->

For broader benchmark notes, see [`docs/benchmarks.md`](./benchmarks.md).

## API Reference

### `num_constant_name()`

Returns the public result described by constant name.

```c
const char *num_constant_name(number_t value);
```

### `num_constant_value()`

Reports whether the condition described by constant value holds.

```c
bool num_constant_value(const char *text, number_t *out);
```

### `num_constant_value_text()`

Reports whether the condition described by constant value text holds.

```c
bool num_constant_value_text(const string_t *text, number_t *out);
```

### `num_create_from_text()`

Creates or reconstructs the public value described by create from text.

```c
number_t num_create_from_text(const string_t *text);
```

### `num_destroy_slow()`

Performs the public operation described by destroy slow.

```c
void num_destroy_slow(number_t *number);
```

### `num_factors_free()`

Releases or clears the resources associated with factors free.

```c
void num_factors_free(number_factors_t *factors);
```

### `num_get_effective_prec_bits()`

Returns the public result described by get effective prec bits.

```c
size_t num_get_effective_prec_bits(const number_t number);
```

### `num_get_mantissa_bits()`

Returns the public result described by get mantissa bits.

```c
size_t num_get_mantissa_bits(const number_t number);
```

### `num_get_mantissa_u64()`

Reports whether the condition described by get mantissa u64 holds.

```c
bool num_get_mantissa_u64(const number_t number, uint64_t *out);
```

### `num_is_exact()`

Reports whether the condition described by is exact holds.

```c
bool num_is_exact(const number_t number);
```

### `num_is_finite()`

Reports whether the condition described by is finite holds.

```c
bool num_is_finite(const number_t number);
```

### `num_is_inf()`

Reports whether the condition described by is inf holds.

```c
bool num_is_inf(const number_t number);
```

### `num_is_integer()`

Reports whether the condition described by is integer holds.

```c
bool num_is_integer(const number_t number);
```

### `num_is_nan()`

Reports whether the condition described by is nan holds.

```c
bool num_is_nan(const number_t number);
```

### `num_is_one()`

Reports whether the condition described by is one holds.

```c
bool num_is_one(const number_t number);
```

### `num_pow10()`

Returns the public result described by pow10.

```c
number_t num_pow10(int exponent10);
```

### `num_scope_is_active()`

Reports whether the condition described by scope is active holds.

```c
bool num_scope_is_active(void);
```

### `num_set_from_string()`

Creates or reconstructs the public value described by set from string.

```c
int num_set_from_string(number_t *number, const char *text);
```

### `num_set_from_text()`

Creates or reconstructs the public value described by set from text.

```c
int num_set_from_text(number_t *number, const string_t *text);
```

### `num_sign()`

Returns the public result described by sign.

```c
int num_sign(const number_t number);
```

### `num_sprintf_text()`

Returns the public result described by sprintf text.

```c
string_t *num_sprintf_text(const char *fmt, ...);
```

### `num_to_double()`

Returns the public result described by to double.

```c
double num_to_double(const number_t number);
```

### `num_to_qfloat()`

Returns the public result described by to qfloat.

```c
qfloat_t num_to_qfloat(const number_t number);
```

### `num_vsprintf_text()`

Returns the public result described by vsprintf text.

```c
string_t *num_vsprintf_text(const char *fmt, va_list ap);
```

### `number_inline_kind()`

Returns the storage kind tag without validating the value. This trivial
accessor remains inline.

```c
static inline uint32_t number_inline_kind(number_t number);
```

### `number_inline_make_qcomplex()`

Constructs a fixed-precision complex value without changing its components.
This trivial wrapper remains inline and calls the out-of-line component constructor.

```c
static inline number_t number_inline_make_qcomplex(qcomplex_t value);
```

### `number_inline_make_qcomplex_parts()`

Constructs a fixed-precision complex value from its real and imaginary
components. The implementation is out of line; the existing name is retained.

```c
number_t number_inline_make_qcomplex_parts(qfloat_t real, qfloat_t imag);
```

### `number_inline_make_qfloat()`

Constructs a fixed-precision real value without changing its double-double
components. The implementation is out of line; the existing name is retained.

```c
number_t number_inline_make_qfloat(qfloat_t value);
```

### `number_inline_qcomplex()`

Extracts the stored complex value from a number using the qcomplex backend.
The implementation is out of line; the existing name is retained.

```c
qcomplex_t number_inline_qcomplex(number_t number);
```

### `number_inline_qcomplex_imag()`

Extracts the imaginary component from a number using the qcomplex backend.
The implementation is out of line; the existing name is retained.

```c
qfloat_t number_inline_qcomplex_imag(number_t number);
```

### `number_inline_qcomplex_real()`

Extracts the real component from a number using the qcomplex backend.
The implementation is out of line; the existing name is retained.

```c
qfloat_t number_inline_qcomplex_real(number_t number);
```

### `number_inline_qfloat()`

Extracts the stored double-double value from a number using the qfloat backend.
The implementation is out of line; the existing name is retained.

```c
qfloat_t number_inline_qfloat(number_t number);
```


## Chebyshev and Hermite polynomials

| API | Family |
| :--- | :--- |
| `num_chebyshev_t` | First-kind Chebyshev, $T_0=1$, $T_1=x$ |
| `num_chebyshev_u` | Second-kind Chebyshev, $U_0=1$, $U_1=2x$ |
| `num_hermite_h` | Physicists' Hermite, $H_0=1$, $H_1=2x$ |

These APIs take the degree followed by the argument. Degrees must be real,
non-negative integers no larger than INT_MAX; invalid degrees return NaN.
Ordinary number arithmetic preserves exact values and the chosen precision.
The existing harmonic-polynomial API is unchanged. See the
[expression naming table](expression.md#chebyshev-and-hermite-polynomials)
for `Tn`, `Un` and the distinct script-H Hermite notation.

README example:

```c
#include <stdio.h>
#include "number-t.h"

int main(void)
{
    NUM_SCOPE(scope);
    number_t value = num_hermite_h(num_create_from_long(3), NUM_TWO);
    num_printf("Hermite H3(2) = %n\n", value);
    return 0;
}
```

Output:

```text
Hermite H3(2) = 40
```

## Physical immortal constants

These `number_t` globals are exact, process-lifetime scalar definitions, alongside
the mathematical immortals. They carry no dimensions; use
[quantity_t](quantity.md#shared-constants-and-selected-precision) to attach units.

| Constant | Exact SI value | Unit |
| --- | --- | --- |
| `NUM_SPEED_OF_LIGHT` | 299792458 | m/s |
| `NUM_ELEMENTARY_CHARGE` | 1.602176634 × 10⁻¹⁹ | C (A s) |
| `NUM_PLANCK` | 6.62607015 × 10⁻³⁴ | J s |
| `NUM_BOLTZMANN` | 1.380649 × 10⁻²³ | J/K |
| `NUM_AVOGADRO` | 6.02214076 × 10²³ | mol⁻¹ |
| `NUM_STANDARD_GRAVITY` | 9.80665 | m/s² |
| `NUM_ASTRONOMICAL_UNIT` | 149597870700 | m |
| `NUM_JULIAN_YEAR` | 31557600 | s |

The first five follow the [BIPM defining constants](https://www.bipm.org/en/measurement-units/si-defining-constants).
Standard gravity is the [NIST conventional acceleration](https://physics.nist.gov/cgi-bin/cuu/Value?gn),
not a prediction of local gravity. Astronomical units and Julian years follow
the [IAU conventions](https://iauarchive.eso.org/public/themes/measuring/).
No measured constant is presented as having unlimited physical accuracy.

Copies can be passed to `num_destroy()` safely. Never pass the global objects
themselves to mutating APIs. Exact definitions survive precision changes and
the existing numeric shutdown/reinitialisation lifecycle. They are C API names,
not new mathematical parser aliases; this avoids ambiguous symbols such as c,
e and h.

`num_const()` materialises at the current precision. Integers and rationals
within its size budget stay exact; larger fractions become rounded numeric
values. `num_const_prec()` applies the explicitly requested precision and
rational budget independently of the default. The immortal source is unchanged.

`num_get_fraction_parts(value, &numerator, &denominator)` extracts independently
owned exact integer components without applying the floating-point budget.
It accepts exact integers and rationals, with a positive denominator; inexact,
complex or invalid input fails without changing the outputs. Initialise both
outputs and pass distinct addresses. Release them with `num_destroy()`.

README example: retrieve the elementary charge's exact ratio even at 53 bits.

```c
#include <string.h>

#include "string-t.h"
#include "number-t.h"

int main(void)
{
    size_t saved = num_get_default_prec_bits();
    num_set_default_prec_bits(53);
    number_t numerator = {0}, denominator = {0};
    bool ok = num_get_fraction_parts(NUM_ELEMENTARY_CHARGE, &numerator, &denominator);
    if (!ok)
        return 1;
    string_t *top = num_to_string(numerator), *bottom = num_to_string(denominator);
    string_printf("elementary charge: %S/%S C\n", top, bottom);
    if (!top || !bottom || strcmp(string_c_str(top), "801088317") ||
        strcmp(string_c_str(bottom), "5000000000000000000000000000"))
        return 1;
    string_free(top);
    string_free(bottom);
    num_destroy(&numerator);
    num_destroy(&denominator);
    num_set_default_prec_bits(saved);
    return 0;
}
```

Output:

```text
elementary charge: 801088317/5000000000000000000000000000 C
```

## Bounded Mersenne prime search

This README example searches exponents from 2 to 2203, finding the first 16
Mersenne primes. The bound keeps the corresponding sequential memory test
manageable; it is not a limit of the number API.

```c
#include <stdio.h>

#include "number-t.h"

int main(void)
{
    number_t two = num_create_from_long(2);
    number_t one = num_create_from_long(1);
    unsigned found = 0u;

    for (unsigned p = 2u; p <= 2203u; ++p) {
        number_t exponent = num_create_from_long((long)p);
        if (num_is_prime(exponent)) {
            number_t mersenne_base = num_pow_int(two, (int)p);
            number_t mersenne = num_sub(mersenne_base, one);
            if (num_is_prime(mersenne)) {
                if ((found % 4u) == 3u)
                    printf("M_%-4u is prime\n", p);
                else
                    printf("M_%-4u is prime    ", p);
                found++;
            }
            num_destroy(&mersenne);
            num_destroy(&mersenne_base);
        }
        num_destroy(&exponent);
    }
    num_destroy(&two);
    num_destroy(&one);
    return 0;
}
```

Output:

```text
M_2    is prime    M_3    is prime    M_5    is prime    M_7    is prime
M_13   is prime    M_17   is prime    M_19   is prime    M_31   is prime
M_61   is prime    M_89   is prime    M_107  is prime    M_127  is prime
M_521  is prime    M_607  is prime    M_1279 is prime    M_2203 is prime
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
