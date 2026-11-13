# `number_t`

`number_t` is MARS's generic numeric value cluster.

## Implementation organisation

Related function families and reusable numeric kernels have separate implementation units:

- `src/number/number_cylindrical_series.c` contains Bessel I and Struve H/L,
  their shared guarded series and cylindrical precision selection. Bessel K
  and Y retain separate algorithm files. The series-family selector and
  logarithmic-gamma kernel are private to the shared series implementation.
- `src/number/number_special.c` contains Chebyshev and Hermite recurrences
  together with the scalar signal functions. Both groups use the existing
  generic numeric arithmetic and precision policy.
- `src/number/number_gamma_inverse.c` and `number_chi.c` contain staged inverse
  gamma refinement and direct Legendre chi summation.
- `src/number/number_coefficients.c`, `number_zeta_logs.c` and `number_zeta_sum.c`
  own bounded coefficient reuse, shifted logarithm reuse and finite Dirichlet
  sums. `number_complex_bounds.c` supplies conservative convergence bounds.
- `src/number/number_series_power.c` supplies checked exact machine-word
  denominators for polylogarithm and Legendre chi series, with arbitrary-precision
  fallback when the integer power does not fit.

Scoped allocation declarations live in `src/number/number_internal.h`.
Applications continue to use `number-t.h`; this organisation does not change
the public API or ownership rules.

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

- clears the coefficient caches used by higher special functions
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

Cloning a named MPFR mathematical constant retains its identity. An explicit
precision increase can therefore recompute that constant, rather than simply
padding its previously rounded approximation with more digits. Ordinary
computed values retain their existing approximation when widened.

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

The active scope chain indexes backend and payload ownership. A fixed 256-slot
direct index handles small live sets without per-entry allocation; colliding keys
use a dictionary that also accommodates larger scopes.
Registration, duplicate-owner checks, explicit destruction and detachment use
indexed lookup rather than scanning all live temporary numbers. Scope teardown
visits each remaining record once; leaving the outermost scope releases the
index and its storage. Nested scopes retain the same ownership and lifetime rules.

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
| `num_lambert_Wn(k, x)` | Evaluates branch `k` of Lambert W, the inverse of `w e^w`. |
| `num_lambert_W0(x)` | Evaluates the principal branch `W₀(x)`. |
| `num_lambert_Wm1(x)` | Evaluates the real lower branch `W₋₁(x)` where it exists, with principal complex continuation otherwise. |
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
| `num_bessel_J(ν, x)` | Evaluates the Bessel function of the first kind `Jν(x)`. |
| `num_bessel_I(ν, x)` | Evaluates the principal modified Bessel function `Iν(x)`, including `I₀(x)` at order zero. |
| `num_bessel_K(ν, x)` | Evaluates the principal modified Bessel function `Kν(x)` for real or complex order and non-zero argument. |
| `num_bessel_Y(ν, x)` | Evaluates the Bessel function of the second kind `Yν(x)`. |
| `num_struve_L(ν, x)` | Evaluates the principal modified Struve function `Lν(x)` for real or complex inputs. |
| `num_struve_H(ν, x)` | Evaluates the principal ordinary Struve function `Hν(x)` for real or complex inputs. |
| `num_lommel_S(μ, ν, x)` | Evaluates the Lommel function `sμ,ν(x)`. |
| `num_appell_F1(a, b1, b2, c, x, y)` | Evaluates the two-variable Appell `F₁` hypergeometric function. |
| `num_lauricella_F(a, b, c, x, count)` | Evaluates Lauricella `F_D` for the paired parameter and argument arrays. |
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

In this region, `num_gammainc_upper` and `num_gammainc_Q` can also use the lower
series and a guarded complement. They retain 64 extra bits and accept that route
only when subtraction loses fewer than 32 bits; otherwise they use MPFR's direct
upper-tail calculation. An unregularised upper-tail call no longer calculates
an unused complete gamma value. Outside this region the direct upper calculation
and complete-minus-upper calculation remain available.

Complex lower, upper, P and Q calculations use MPC series, principal powers and
the native complex gamma implementation, retaining the participating precision.
They check the remaining geometric tail and cancellation before accepting a result.
The complex series excludes non-positive integer orders, limits `abs(x)` to 8,000,
uses at most 100,000 terms and limits guarded work to 65,536 bits. Unsupported
inputs or unresolved cancellation return an invalid result. The fixed-precision
qfloat backend retains its existing implementation.

#### Precision-preserving series and inverse functions

Multiprecision `num_hypergeometric_pFq` retains upper parameters, lower parameters
and the argument at the widest participating precision. Its defining series supports
`p <= q`, `p = q + 1` with `abs(z) < 1`, and terminating polynomials. It rejects
denominator poles, unsupported continuation and exhausted convergence limits.
The native series uses at most 200,000 terms and 65,536 guarded working bits.
Exact parameters and arguments contribute the current default working precision.

`number_lerch_phi` and `num_lerch_phi` use native arithmetic for real and complex
arguments in `abs(z) < 1`, with the existing zero-argument, zero-order and Hurwitz
zeta reductions. Integer orders use integer powers. General orders reuse shifted
inverse powers across calls: the additional power cache retains
one exact offset/order pair, at most 4,096 entries, 16,384 bits per entry and eight
MiB of significands. Entries are copied under a mutex, higher precision can serve
lower requests with the same exact key, and changing either key releases the old
entries. This bounds memory use in a persistent server. First use still calculates
the required powers; subsequent calls can reuse them. Convergence and accumulated
rounding checks apply to both cached and uncached evaluations.

Complex `num_gamma`, `num_lgamma` and `num_gammainv` retain MPC precision instead
of converting to qcomplex. Gamma uses recurrence and a guarded Stirling expansion;
log gamma tracks the logarithm's branch through that recurrence. Inverse gamma
refines the established logarithmic branch with increasing precision and checks
the final defining residual. Working storage is local apart from the bounded,
synchronised coefficient cache. A rejected input or unresolved calculation
returns an invalid number rather than a lower-precision answer.

Double-backed inverse erf and erfc use cheap starting estimates followed by libc
Halley refinement; very small complementary probabilities use MPFR. Complex inverse
secant and cosecant use guarded logarithmic identities with the same principal
branches. Legendre chi reuses small blocks of powers, with a geometric remainder
bound determining the required terms. Appell F1 also recognises the exact reduction
`F1(a,b1,b2;a;x,y) = (1-x)^(-b1)*(1-y)^(-b2)` before summing its general recurrence.

#### Clausen functions

`num_clausen(order, theta)` takes a positive integer order and an angle in
radians; `num_clausen2(theta)` is exactly `num_clausen(2, theta)`.
The C API requires the order in `num_clausen`. In contrast, MARS Lab's
one-argument input names `clausen(theta)` and `clausen2(theta)` both select
order two; use `Cl(n,theta)` for another order. See the
[expression spellings](expression.md#clausen-functions).

For real angles, even
orders use the sine Fourier series and odd orders the cosine Fourier series.
Order one is `-ln(2*abs(sin(theta/2)))`, with logarithmic poles at multiples
of `2*pi`; higher orders are continuous and periodic on the real axis.

Real inputs are evaluated using real arithmetic. Fixed-precision and
arbitrary-precision inputs retain their native precision. Nonreal inputs use
the [holomorphic Clausen continuation](qcomplex.md#clausen-functions),
including its exclusion of nonreal points on `Re(z) = 2*k*pi`. Order zero
and non-finite inputs return NaN. Each call returns an owning number.

### Bessel Y

`num_bessel_Y(order, argument)` returns an owning principal Bessel function
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
    number_t value = num_bessel_Y(NUM_ZERO, NUM_ONE);
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

`num_bessel_K(order, argument)` returns an owning number containing the
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
series for K₀ and K₁ followed by recurrence. These calculations reuse local MPC
scratch values, avoiding numeric wrapper allocations inside the series loop.
Non-integer orders use two reduced
I-series with gamma factors. Each series is limited to 20000 iterations; failure
to converge returns NaN. The near-integer guard also rejects an unrepresentable
distance estimate or a required distance correction above 65536 bits. This is
not a uniform 65536-bit cap on the total working precision.

The qfloat and qcomplex interfaces delegate to this evaluator and round to
double-double components. Symbolic argument derivatives, affine primitives and
finite sums belong to the [expression layer](expression.md#inverse-hyperbolic-sine-fourier-pair).

### Modified Bessel I

`num_bessel_I(order, argument)` returns an owning number containing the
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
    number_t bessel = num_bessel_I(NUM_ZERO, NUM_ONE);
    num_printf("I_0(1) = %.16n\n", bessel);
    return 0;
}
```

```text
I_0(1) = 1.266065877752008
```

### Ordinary Struve H

`num_struve_H(order, argument)` returns an owning number containing the
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
    number_t ordinary = num_struve_H(NUM_ZERO, NUM_ONE);
    num_printf("H_0(1) = %.15n\n", ordinary);
    return 0;
}
```

```text
H_0(1) = 0.568656627048288
```

### Modified Struve L

`num_struve_L(order, argument)` returns an owning number containing the
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
    number_t value = num_struve_L(NUM_ZERO, NUM_ONE);
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

The scalar integer operations are also exposed by the native expression parser
and MARS Lab, including three-argument `powmod(a,e,m)`. No floating-point
conversion is used for its operands. Multi-result routines use an explicit
component selector in scalar expressions; see the [expression guide](expression.md#value-only-functions-owning)
for the callable spellings and examples.

Binary arithmetic mixes supported backends automatically by promoting to a
common target representation before dispatch.

`num_factors()` honours the current `computation_run()` control during trial
division and repeated factor removal. Cancellation, a deadline or an exhausted
work budget returns `NULL` and discards partial factors. Without a control,
factorisation is unrestricted and a difficult composite can take a long time;
operand size alone does not predict its cost.

Appell F₁ is the two-variable member of `num_lauricella_F(...)`. Its MPFR and
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
estimate with checked Halley iteration, increasing working precision as the
estimate improves, and verify the final forward residual;
the starting estimate does not limit the result to double precision. Inverse
erfc works directly with small complementary probabilities. Non-convergence
returns NaN, rather than an unchecked finite result. Real endpoint values
retain their signed infinities.

MPFR inverse gamma similarly increases Newton working precision in stages and
checks the final logarithmic gamma residual. It retains the existing real branch
selection; an approximate seed never determines the returned accuracy. Newton's
slope uses half the current working precision plus 32 guard bits: its rounding
error is multiplied by the small residual. The correction and final logarithmic
residual still have to satisfy the full result-precision checks.

For non-negative integer orders of at least two, MPFR and MPC Legendre chi use
the direct odd-power series inside `|z| < 0.95`. A geometric tail bound and a
roundoff check control convergence. Outside this region the existing
polylogarithm continuation remains available.

MPFR [Appell F₁](https://dlmf.nist.gov/16.13) uses real arithmetic throughout. Both real and complex paths sum
by total degree and advance their linear coefficients by addition, refreshing
them every 64 terms. Shifted upper and lower parameters retain all their input
bits and every permitted integer shift, while simple dyadic parameters avoid
full-width zero padding. When complex parameter products fit exactly in the
working precision, an exact quadratic recurrence supplies the numerator and
real squared denominator of their ratio. Other parameters retain ordinary
complex division. Terminating numerators, lower-parameter poles, the existing
`|x|, |y| < 0.95` domain and cancellation checks remain supported.

Appell F₁, complex error functions, complex polylogarithms and integer-order Bessel K use directed
upper and lower magnitude bounds for convergence checks where a square root
is unnecessary. These conservative bounds leave the mathematical modulus and
the returned function definitions unchanged.

Appell, Legendre chi and polylogarithm series compare binary exponents of their
directed bounds before accepting a small term. This avoids repeated products of
full-width significands. Chi's exponent test includes an upper bound for its
geometric tail factor. An inconclusive comparison continues summation, so this
shortcut can require an extra term rather than accepting one prematurely.
Dilogarithm and polylogarithm paths propagate NaN components before comparing
inputs with finite special values such as zero and one.

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

The native Euler–Maclaurin zeta paths share a scaled recurrence for the combined
rising product and endpoint power. Its quadratic factor advances by finite
differences, avoiding separate factorial products and powers at each correction.
One endpoint exponential supplies the integral, endpoint and correction terms.
Analytic derivatives advance alongside the value without dividing by a possibly
zero rising product; value-only calls omit derivative work. This also applies to
[Hurwitz zeta](https://dlmf.nist.gov/25.11#iii). Riemann finite sums evaluate prime powers once and
construct composite powers by multiplication, using a directly indexed sieve.
Derivative sums also reuse the prime logarithms, constructing composite
logarithms by addition. Integer logarithms are retained across evaluations in the
shared coefficient cache, including rounded copies from higher working precision.
Temporary power and logarithm storage together are
bounded to eight MiB and 4096 indices; larger sums use direct evaluation.
Above 512 working bits, their finite sums grow with
precision instead of retaining a fixed 128-term length. Both value and derivative
paths also extend the finite sum with the magnitude of the imaginary argument, so higher zero-search windows
do not retain a summation length suited only to small heights. Both paths retain their convergence checks and
precision-dependent Bernoulli corrections; an
exhausted correction allowance still returns NaN rather than an unchecked value.
For finite positive non-integer real arguments up to 64, working precision of
1024–8192 bits selects the same native Euler–Maclaurin kernel, with a zero
imaginary component. Integer arguments, other real arguments and other precisions
retain MPFR's direct zeta calculation. An unsuccessful native evaluation falls
back to MPFR without modifying the input. The shared kernel follows the
[Euler–Maclaurin representation](https://dlmf.nist.gov/25.2#iii); selection changes
the algorithm, not the precision or real result type.

Complex asymptotic corrections test component exponents directly, reserving an
extra bit for their sum. This avoids allocating a magnitude temporary for each
correction. Polylogarithm and Legendre chi series also use exact integer division
when their denominator fits `unsigned long`; checked binary exponentiation detects
overflow and retains the full MPFR denominator path. Complex polylogarithm and
exponential-integral convergence tests use conservative component bounds rather
than repeated square roots. The [polylogarithm series](https://dlmf.nist.gov/25.12#ii)
and its existing continuation and domain restrictions remain in use.

Multiprecision digamma and polygamma balance argument shifts against their
asymptotic correction series. Trigamma and tetragamma share this kernel for MPFR
and MPC operands. Corrections stop below the guarded precision target.

Clausen, zeta and polygamma reuse precision-aware, directly indexed coefficients
and integer logarithms.
Ordinary Bernoulli coefficients and their factorial-normalised equivalents are
cached separately, avoiding repeated factorials in Euler–Maclaurin sums. Integer
zeta coefficients are shared by Clausen evaluations. A mutex protects lazy
precision upgrades; callers receive copies. Cached significands are bounded to
eight MiB, plus fixed index metadata, and are released at process shutdown.
Larger requests are calculated without retention, so cache limits do not cap accuracy.

Hurwitz evaluations also retain `log(a+n)` for the most recently used exact
offset `a`, independently of the varying exponent. Direct indexing avoids a
search. A separate eight-MiB cache retains at most 4096 logarithms and 16384
working bits, plus fixed metadata. Offset changes release its previous entries;
signed zeros distinguish logarithmic branch-cut sides. A mutex protects copied
values, while logarithms are calculated outside the lock. Larger requests use
direct calculation. Both caches are released at process shutdown.

Double-backed Riemann zeta uses MPFR at 53 bits. Its derivative, real Hurwitz
variants and Clausen functions use the corresponding native kernels at that
precision, rather than first performing a qfloat calculation. Other backends
retain their own precision.

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

The unrestricted integer cases use 512–1,215-bit operands, including a 521-bit prime,
and cover ordinary arithmetic as well as integer-specific functions. The unrestricted
fraction cases time exact rational arithmetic on large numerators and denominators.
Both profiles parse their operands before timing and reject any inexact result.
Factorial, Fibonacci and partition indices remain ordinary integers because those
APIs accept counts; their exact results exceed native integer sizes.

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

### number / double precision

| Operation | Inputs | 53 bits |
|---|---|---:|
| E1 | 2.345678 | 7.1 µs |
| Ei | 2.345678 | 8.6 µs |
| Li | 2.345 | 7.1 µs |
| abs | 0.7 | 7.5 ns |
| acos | 0.7 | 18.7 ns |
| acosec | 2.345 | 16.6 ns |
| acosech | 0.7 | 61.3 ns |
| acosh | 2.345 | 28.8 ns |
| acot | 0.7 | 47.1 ns |
| acoth | 2.345 | 29.6 ns |
| add | 0.7, 0.7 | 11.1 ns |
| add_long | 17, 3 | 584.7 ns |
| appell_F1 | 0.5, 0.7, 0.8, 2.5, 0.2, 0.3 | 23.1 µs |
| arccovercos | 0.5 | 113.3 ns |
| arccoversin | 0.5 | 116.1 ns |
| archacovercos | 0.5 | 218.2 ns |
| archacoversin | 0.5 | 232.8 ns |
| archavercos | 0.5 | 226.8 ns |
| archaversin | 0.5 | 216.7 ns |
| arcvercos | 0.5 | 118.0 ns |
| arcversin | 0.5 | 112.2 ns |
| arg | 0.7 | 74.6 ns |
| asec | 2.345 | 18.0 ns |
| asech | 0.7 | 31.3 ns |
| asin | 0.7 | 17.0 ns |
| asinh | 0.7 | 53.7 ns |
| atan | 0.7 | 29.6 ns |
| atan2 | 0.5, -0.75 | 41.2 ns |
| atanh | 0.7 | 25.8 ns |
| bessel_I | 2, 0.7 | 45.4 µs |
| bessel_J | 2, 0.7 | 7.3 µs |
| bessel_K | 2, 0.7 | 26.7 µs |
| bessel_Y | 2, 0.7 | 17.9 µs |
| beta | 2.3, 4.5 | 131.6 ns |
| beta_pdf | 0.5, 2.5, 3.5 | 183.8 ns |
| binomial | 5, 2 | 381.7 ns |
| ceil | 0.7 | 70.8 ns |
| chebyshev_Tn | 2, 0.7 | 768.5 ns |
| chebyshev_Un | 2, 0.7 | 1.1 µs |
| circ | 0.7 | 241.0 ns |
| clausen | 3, 0.7 | 7.1 µs |
| clausen2 | 0.7 | 7.7 µs |
| conj | 0.7 | 8.0 ns |
| cos | 0.7 | 31.5 ns |
| cosec | 0.7 | 34.2 ns |
| cosech | 0.7 | 57.6 ns |
| cosh | 0.7 | 39.5 ns |
| cot | 0.7 | 36.7 ns |
| coth | 0.7 | 58.2 ns |
| covercos | 0.7 | 127.9 ns |
| coversin | 0.7 | 116.8 ns |
| cubrt | 0.7 | 5.7 µs |
| digamma | 2.345 | 2.5 µs |
| dilog | 0.7 | 15.7 µs |
| div | 0.7, 0.7 | 19.3 ns |
| erf | 0.7 | 68.3 ns |
| erfc | 0.7 | 74.2 ns |
| erfcinv | 0.5 | 343.2 ns |
| erfinv | 0.5 | 297.2 ns |
| exp | 1.23456789 | 44.0 ns |
| floor | 0.7 | 8.3 ns |
| gamma | 2.345 | 121.1 ns |
| gammainc_P | 2.3, 1.4 | 13.4 µs |
| gammainc_Q | 2.3, 1.4 | 14.0 µs |
| gammainc_lower | 2.3, 1.4 | 15.3 µs |
| gammainc_upper | 2.3, 1.4 | 17.2 µs |
| gammainv | 9.5 | 98.2 µs |
| hacovercos | 0.7 | 323.1 ns |
| hacoversin | 0.7 | 248.7 ns |
| harmonic_poly | 2, 0.7 | 1.3 µs |
| havercos | 0.7 | 248.1 ns |
| haversin | 0.7 | 219.7 ns |
| hermite_Hn | 2, 0.7 | 1.7 µs |
| hypergeometric_pFq | 0.7, 2.5, 0.3 | 12.5 µs † |
| hypot | 0.7, 1.2 | 22.4 ns |
| imag_part | 0.7 | 26.0 ns |
| inv | 0.7 | 8.5 ns |
| lambert_W0 | 0.7 | 18.5 µs |
| lambert_Wm1 | -0.2 | 15.1 µs |
| lambert_Wn | 1, 0.7 | 31.1 µs |
| lauricella_F | 0.5, 0.3, 2.5 | 19.9 µs |
| ldexp | 1.23456789, 3 | 48.6 ns |
| legendre_chi | 2, 0.7 | 30.4 µs |
| lerch_phi | 0.3, 2, 1.4 | 84.1 µs |
| lg | 2.345678 | 26.4 ns |
| lgamma | 2.345 | 69.9 ns |
| ln | 2.345678 | 18.0 ns |
| log | 2.345678 | 15.8 ns |
| log10 | 2.345678 | 22.3 ns |
| logbeta | 2.3, 4.5 | 129.6 ns |
| logbeta_pdf | 0.5, 2.5, 3.5 | 167.5 ns |
| lommel_S | 0.5, 0.7, 1.2 | 7.1 µs |
| mul | 0.7, 0.7 | 12.3 ns |
| mul_long | 17, 3 | 546.4 ns |
| mul_pow10 | 1.23456789, 3 | 71.6 ns |
| neg | 0.7 | 8.7 ns |
| normal_cdf | 0.7 | 66.6 ns |
| normal_logpdf | 0.7 | 15.8 ns |
| normal_pdf | 0.7 | 25.7 ns |
| polygamma | 3, 2.345 | 24.3 µs |
| polylog | 2, 0.7 | 16.1 µs |
| polylog1 | 0.7 | 170.2 ns |
| pow | 1.23456789, 3.5 | 35.3 ns |
| pow10 | 3 | 177.7 ns |
| pow_int | 1.23456789, 3 | 59.9 ns |
| productlog | 0.7 | 16.6 µs |
| qdigamma | 0.5, 2.3 | 27.1 µs |
| real_part | 0.7 | 30.2 ns |
| rect | 0.7 | 315.1 ns |
| root | 1.2345, 3 | 7.5 µs |
| round | 0.7 | 379.4 ns |
| roundn | 1.23456789, 3 | 4.3 µs |
| rounds | 1.23456789, 3 | 6.0 µs |
| sec | 0.7 | 30.2 ns |
| sech | 0.7 | 32.4 ns |
| sgn | 0.7 | 130.9 ns |
| sin | 0.567 | 32.8 ns |
| sinc | 0.7 | 5.0 µs |
| sincos | 0.7 | 814.6 ns |
| sinh | 0.7 | 62.4 ns |
| sinhcosh | 0.7 | 155.4 ns |
| sqr | 0.7 | 9.4 ns |
| sqrt | 1.23456789 | 9.8 ns |
| step | 0.7 | 124.0 ns |
| struve_H | 2, 0.7 | 62.4 µs |
| struve_L | 2, 0.7 | 70.7 µs |
| sub | 0.7, 0.7 | 32.6 ns |
| tan | 0.7 | 12.9 µs † |
| tanh | 0.7 | 304.3 ns |
| tetragamma | 2.345 | 6.7 µs † |
| tri | 0.7 | 226.3 ns |
| trigamma | 2.345 | 2.9 µs |
| vercos | 0.7 | 161.2 ns |
| versin | 0.7 | 139.7 ns |
| zatahp | 2.3, 1.4 | 314.4 µs |
| zeta | 2.345 | 68.3 µs |
| zetah | 2.3, 1.4 | 360.4 µs |
| zetap | 2.345 | 134.6 µs |

### number / multi-precision floating point

| Operation | Inputs | 256 bits | 512 bits | 768 bits | 1024 bits | 2048 bits | 4096 bits |
|---|---|---:|---:|---:|---:|---:|---:|
| E1 | 2.345678 | 49.8 µs | 83.8 µs | 148.0 µs | 179.4 µs | 716.8 µs | 2.594 ms |
| Ei | 2.345678 | 46.5 µs | 100.5 µs | 158.4 µs | 212.3 µs | 1.318 ms † | 2.724 ms |
| Li | 2.345 | 44.0 µs | 91.2 µs | 152.0 µs | 203.5 µs † | 679.0 µs | 1.857 ms |
| abs | 0.7 | 94.4 ns | 119.3 ns | 102.7 ns | 97.5 ns | 151.3 ns | 117.3 ns |
| acos | 0.7 | 23.0 µs | 31.3 µs | 45.3 µs | 68.1 µs † | 115.0 µs | 399.5 µs |
| acosec | 2.345 | 20.3 µs | 30.4 µs | 50.0 µs | 51.0 µs | 156.1 µs † | 365.0 µs |
| acosech | 0.7 | 6.6 µs | 11.7 µs | 20.5 µs | 19.0 µs | 59.5 µs † | 159.5 µs |
| acosh | 2.345 | 7.4 µs | 12.0 µs | 15.1 µs | 23.6 µs | 72.3 µs † | 137.9 µs |
| acot | 0.7 | 15.0 µs | 26.1 µs | 43.7 µs | 61.3 µs † | 170.4 µs † | 424.8 µs |
| acoth | 2.345 | 7.6 µs | 11.8 µs | 20.0 µs † | 25.7 µs † | 209.7 µs † | 165.4 µs |
| add | 0.7, 0.7 | 133.8 ns | 168.5 ns | 134.4 ns | 347.9 ns | 237.3 ns | 276.5 ns |
| add_long | 17, 3 | 629.2 ns | 924.5 ns | 1.0 µs | 1.8 µs † | 1.1 µs | 832.4 ns |
| appell_F1 | 0.5, 0.7, 0.8, 2.5, 0.2, 0.3 | 138.4 µs | 402.6 µs | 799.0 µs † | 2.250 ms † | 5.402 ms | 25.774 ms |
| arccovercos | 0.5 | 15.3 µs | 34.8 µs † | 47.2 µs | 60.6 µs | 115.3 µs | 392.6 µs |
| arccoversin | 0.5 | 15.7 µs | 26.1 µs | 37.8 µs | 48.6 µs | 96.3 µs | 332.5 µs |
| archacovercos | 0.5 | 1.5 µs | 1.6 µs | 2.4 µs | 1.8 µs | 1.9 µs | 2.3 µs |
| archacoversin | 0.5 | 2.1 µs † | 1.6 µs | 1.8 µs | 1.7 µs | 2.2 µs † | 2.4 µs |
| archavercos | 0.5 | 1.4 µs | 1.9 µs | 1.9 µs | 1.6 µs | 1.7 µs | 2.4 µs |
| archaversin | 0.5 | 1.7 µs | 2.0 µs | 2.0 µs | 2.0 µs | 1.7 µs | 2.2 µs |
| arcvercos | 0.5 | 17.5 µs | 29.7 µs | 34.0 µs | 49.3 µs | 184.8 µs † | 293.1 µs |
| arcversin | 0.5 | 18.6 µs | 26.5 µs | 40.9 µs | 50.2 µs | 140.4 µs | 322.7 µs |
| arg | 0.7 | 524.6 ns | 658.9 ns | 618.9 ns | 632.2 ns | 900.3 ns | 603.2 ns |
| asec | 2.345 | 22.2 µs | 32.7 µs | 40.0 µs | 58.1 µs | 141.4 µs | 349.5 µs |
| asech | 0.7 | 7.0 µs | 10.2 µs | 19.8 µs | 20.3 µs | 59.5 µs | 149.8 µs |
| asin | 0.7 | 17.6 µs | 27.4 µs | 43.9 µs | 68.9 µs | 141.9 µs | 363.1 µs |
| asinh | 0.7 | 7.3 µs | 10.8 µs | 14.5 µs | 19.7 µs | 50.1 µs | 147.8 µs |
| atan | 0.7 | 15.2 µs | 23.1 µs | 38.5 µs | 56.5 µs | 134.9 µs | 285.7 µs |
| atan2 | 0.5, -0.75 | 14.7 µs | 32.1 µs | 29.3 µs | 66.4 µs | 114.4 µs | 360.9 µs |
| atanh | 0.7 | 6.7 µs | 9.9 µs | 15.3 µs | 17.8 µs | 48.8 µs | 163.9 µs |
| bessel_I | 2, 0.7 | 107.7 µs | 230.6 µs | 440.9 µs | 534.6 µs | 2.047 ms | 9.002 ms |
| bessel_J | 2, 0.7 | 8.1 µs | 19.0 µs | 26.5 µs | 60.4 µs † | 181.8 µs | 894.0 µs |
| bessel_K | 2, 0.7 | 65.3 µs | 106.4 µs | 178.9 µs | 274.7 µs | 1.303 ms † | 5.419 ms |
| bessel_Y | 2, 0.7 | 42.6 µs | 98.6 µs | 183.3 µs | 282.3 µs | 1.403 ms † | 5.577 ms † |
| beta | 2.3, 4.5 | 112.2 µs | 300.5 µs | 459.0 µs | 876.7 µs | 3.831 ms | 27.760 ms † |
| beta_pdf | 0.5, 2.5, 3.5 | 103.9 µs | 244.1 µs | 441.0 µs | 585.1 µs | 2.208 ms | 14.646 ms |
| binomial | 5, 2 | 107.4 µs | 276.9 µs | 638.3 µs | 730.1 µs | 3.810 ms | 15.276 ms |
| ceil | 0.7 | 800.9 ns | 746.4 ns | 849.7 ns | 725.1 ns | 659.3 ns | 1.5 µs † |
| chebyshev_Tn | 2, 0.7 | 3.2 µs | 3.0 µs | 4.8 µs | 4.3 µs | 5.0 µs | 8.7 µs |
| chebyshev_Un | 2, 0.7 | 4.0 µs | 3.7 µs | 5.4 µs | 4.1 µs | 18.2 µs † | 9.7 µs † |
| circ | 0.7 | 863.7 ns | 1.2 µs | 2.6 µs † | 1.3 µs | 1.4 µs | 3.1 µs † |
| clausen | 3, 0.7 | 35.8 µs | 72.6 µs | 196.0 µs | 211.9 µs | 1.077 ms | 7.738 ms |
| clausen2 | 0.7 | 26.4 µs | 83.9 µs | 148.8 µs | 273.2 µs | 1.163 ms | 5.814 ms |
| conj | 0.7 | 84.1 ns | 111.9 ns | 88.8 ns | 137.8 ns | 100.1 ns | 152.1 ns |
| cos | 0.7 | 3.1 µs | 4.9 µs | 10.9 µs | 15.6 µs | 48.0 µs | 190.8 µs |
| cosec | 0.7 | 3.7 µs | 7.2 µs | 9.6 µs | 20.5 µs | 53.0 µs | 177.6 µs |
| cosech | 0.7 | 3.5 µs | 6.9 µs | 10.7 µs | 23.1 µs † | 65.5 µs † | 226.1 µs |
| cosh | 0.7 | 3.8 µs | 6.6 µs | 12.6 µs | 19.1 µs | 56.3 µs | 180.8 µs |
| cot | 0.7 | 5.6 µs | 13.0 µs | 20.6 µs | 30.2 µs | 105.9 µs | 446.7 µs |
| coth | 0.7 | 5.9 µs | 16.6 µs | 23.4 µs | 30.5 µs | 98.0 µs | 366.7 µs |
| covercos | 0.7 | 5.0 µs | 6.5 µs | 11.3 µs | 15.8 µs | 53.8 µs | 173.8 µs |
| coversin | 0.7 | 4.0 µs | 6.5 µs | 12.0 µs | 22.2 µs † | 50.4 µs | 244.3 µs |
| cubrt | 0.7 | 13.7 µs | 23.5 µs | 35.9 µs | 43.4 µs | 128.6 µs † | 418.5 µs |
| digamma | 2.345 | 117.4 µs | 312.7 µs | 712.5 µs | 1.028 ms | 4.635 ms | 21.717 ms |
| dilog | 0.7 | 121.4 µs | 231.2 µs | 697.8 µs | 897.7 µs | 4.611 ms | 21.523 ms |
| div | 0.7, 0.7 | 362.9 ns | 707.5 ns | 877.5 ns | 1.3 µs | 4.5 µs | 12.3 µs |
| erf | 0.7 | 21.8 µs | 44.9 µs | 87.7 µs | 131.9 µs | 469.1 µs | 3.078 ms |
| erfc | 0.7 | 23.3 µs | 44.8 µs | 83.9 µs | 169.4 µs † | 638.9 µs | 2.245 ms |
| erfcinv | 0.5 | 162.4 µs | 393.4 µs | 416.7 µs | 784.4 µs | 3.889 ms | 15.085 ms |
| erfinv | 0.5 | 181.0 µs † | 352.6 µs | 450.4 µs | 860.4 µs | 3.554 ms | 16.075 ms |
| exp | 1.23456789 | 3.3 µs | 5.7 µs | 12.9 µs | 20.5 µs | 62.9 µs | 218.8 µs |
| floor | 0.7 | 104.9 ns | 94.4 ns | 124.1 ns | 110.6 ns | 128.1 ns | 152.6 ns |
| gamma | 2.345 | 111.6 µs † | 92.7 µs | 164.2 µs | 335.0 µs | 1.190 ms | 6.718 ms |
| gammainc_P | 2.3, 1.4 | 147.1 µs | 284.8 µs | 489.7 µs | 915.2 µs | 3.230 ms | 15.774 ms |
| gammainc_Q | 2.3, 1.4 | 136.6 µs | 294.1 µs | 622.8 µs | 751.9 µs | 3.312 ms | 16.379 ms |
| gammainc_lower | 2.3, 1.4 | 78.9 µs | 145.2 µs | 339.5 µs | 447.9 µs | 2.117 ms | 9.711 ms |
| gammainc_upper | 2.3, 1.4 | 140.7 µs | 289.8 µs | 574.7 µs | 1.404 ms † | 3.137 ms | 18.009 ms |
| gammainv | 9.5 | 770.8 µs | 1.761 ms | 2.296 ms | 4.979 ms | 11.011 ms | 65.406 ms |
| hacovercos | 0.7 | 5.4 µs | 7.1 µs | 14.3 µs | 19.0 µs | 56.2 µs | 647.4 µs † |
| hacoversin | 0.7 | 5.6 µs | 8.2 µs | 14.5 µs | 18.2 µs | 60.9 µs | 217.9 µs |
| harmonic_poly | 2, 0.7 | 4.6 µs | 6.7 µs | 6.1 µs | 6.8 µs | 12.0 µs | 42.8 µs |
| havercos | 0.7 | 4.4 µs | 6.6 µs | 12.4 µs | 15.7 µs | 54.4 µs | 182.3 µs |
| haversin | 0.7 | 4.2 µs | 6.9 µs | 10.7 µs | 20.9 µs † | 47.9 µs | 218.3 µs |
| hermite_Hn | 2, 0.7 | 4.0 µs | 4.6 µs | 3.9 µs | 4.3 µs | 5.9 µs | 6.7 µs |
| hypergeometric_pFq | 0.7, 2.5, 0.3 | 70.8 µs † | 105.5 µs † | 218.8 µs † | 287.6 µs † | 1.328 ms † | 6.078 ms † |
| hypot | 0.7, 1.2 | 820.9 ns | 948.3 ns | 1.4 µs | 2.1 µs | 3.3 µs | 9.7 µs |
| imag_part | 0.7 | 95.1 ns | 90.1 ns | 85.2 ns | 104.9 ns | 104.6 ns | 87.8 ns |
| inv | 0.7 | 244.1 ns | 317.0 ns | 483.2 ns | 1.5 µs † | 1.7 µs | 5.4 µs |
| lambert_W0 | 0.7 | 56.8 µs | 101.3 µs | 181.3 µs | 375.5 µs | 945.5 µs | 4.059 ms |
| lambert_Wm1 | -0.2 | 75.7 µs | 133.9 µs | 245.7 µs | 350.3 µs | 1.063 ms | 8.568 ms † |
| ldexp | 1.23456789, 3 | 156.9 ns | 131.1 ns | 193.3 ns | 236.6 ns | 167.1 ns | 276.0 ns |
| legendre_chi | 2, 0.7 | 55.2 µs | 131.2 µs | 199.8 µs | 335.8 µs | 909.6 µs | 3.454 ms |
| lerch_phi | 0.3, 2, 1.4 | 195.0 µs | 600.9 µs | 1.162 ms † | 1.408 ms † | 6.013 ms † | 28.873 ms † |
| lg | 2.345678 | 13.2 µs | 23.3 µs | 35.9 µs † | 42.0 µs | 117.6 µs | 322.1 µs |
| lgamma | 2.345 | 39.9 µs | 114.9 µs | 210.5 µs | 255.1 µs | 1.075 ms | 6.535 ms |
| ln | 2.345678 | 5.7 µs | 8.4 µs | 15.5 µs | 18.1 µs | 45.8 µs | 156.0 µs |
| log | 2.345678 | 5.6 µs | 9.4 µs | 14.2 µs | 20.1 µs | 56.8 µs | 155.4 µs |
| log10 | 2.345678 | 14.3 µs | 24.5 µs | 35.9 µs | 39.1 µs | 115.7 µs | 240.8 µs |
| logbeta | 2.3, 4.5 | 114.9 µs | 293.1 µs | 643.5 µs | 947.6 µs | 6.025 ms † | 19.888 ms |
| logbeta_pdf | 0.5, 2.5, 3.5 | 111.2 µs | 213.7 µs | 497.1 µs | 774.9 µs | 2.872 ms | 12.836 ms † |
| lommel_S | 0.5, 0.7, 1.2 | 33.1 µs | 72.8 µs | 149.6 µs | 298.7 µs | 959.1 µs | 3.493 ms |
| mul | 0.7, 0.7 | 122.8 ns | 224.7 ns | 257.2 ns | 475.3 ns | 1.2 µs | 3.0 µs |
| mul_long | 17, 3 | 763.9 ns | 868.2 ns | 1.2 µs | 1.0 µs | 1.3 µs † | 1.0 µs |
| mul_pow10 | 1.23456789, 3 | 380.0 ns | 454.3 ns | 801.2 ns | 599.5 ns | 608.7 ns | 819.7 ns |
| neg | 0.7 | 112.9 ns | 97.2 ns | 95.7 ns | 105.2 ns | 132.7 ns | 113.6 ns |
| normal_cdf | 0.7 | 21.2 µs † | 92.8 µs | 77.9 µs | 134.1 µs | 481.6 µs | 2.214 ms |
| normal_logpdf | 0.7 | 6.6 µs | 10.0 µs | 12.5 µs | 17.9 µs | 47.3 µs | 127.3 µs |
| normal_pdf | 0.7 | 4.2 µs | 6.2 µs | 13.8 µs | 22.9 µs | 57.9 µs | 262.1 µs |
| polygamma | 3, 2.345 | 63.2 µs | 126.7 µs | 331.6 µs | 467.7 µs | 1.860 ms | 12.100 ms |
| polylog | 2, 0.7 | 144.6 µs | 332.2 µs | 690.1 µs | 1.063 ms | 3.522 ms | 18.338 ms |
| polylog1 | 0.7 | 7.6 µs | 12.3 µs | 18.1 µs † | 23.1 µs | 57.2 µs † | 142.6 µs |
| pow | 1.23456789, 3.5 | 11.5 µs | 23.3 µs † | 39.0 µs † | 42.3 µs | 116.0 µs | 314.4 µs |
| pow10 | 3 | 1.0 µs † | 441.3 ns | 622.6 ns | 532.5 ns | 341.9 ns | 753.6 ns |
| pow_int | 1.23456789, 3 | 292.8 ns | 570.3 ns | 889.0 ns | 845.4 ns | 2.7 µs | 9.0 µs |
| productlog | 0.7 | 71.8 µs | 108.5 µs | 230.7 µs | 375.2 µs | 960.8 µs | 4.034 ms |
| real_part | 0.7 | 90.7 ns | 100.7 ns | 88.1 ns | 122.9 ns | 106.0 ns | 111.4 ns |
| rect | 0.7 | 1.3 µs | 986.6 ns | 1.3 µs | 1.5 µs † | 1.3 µs | 1.9 µs † |
| root | 1.2345, 3 | 17.1 µs | 22.7 µs | 43.0 µs | 50.4 µs | 112.4 µs | 418.4 µs |
| round | 0.7 | 2.8 µs | 3.2 µs | 4.4 µs † | 3.6 µs † | 2.5 µs | 2.7 µs |
| roundn | 1.23456789, 3 | 6.5 µs | 5.9 µs | 7.9 µs | 8.9 µs | 9.1 µs | 13.6 µs |
| rounds | 1.23456789, 3 | 23.2 µs | 31.8 µs | 46.2 µs | 44.5 µs | 133.3 µs | 346.0 µs |
| sec | 0.7 | 2.6 µs | 5.6 µs | 8.8 µs | 13.3 µs | 46.0 µs | 223.9 µs |
| sech | 0.7 | 3.2 µs | 6.7 µs | 12.0 µs | 20.3 µs | 64.7 µs | 233.1 µs |
| sgn | 0.7 | 349.5 ns | 459.2 ns | 305.2 ns | 335.4 ns | 310.0 ns | 418.6 ns |
| sin | 0.567 | 4.5 µs † | 9.1 µs † | 10.0 µs | 20.3 µs | 59.1 µs | 177.2 µs |
| sinc | 0.7 | 6.6 µs | 10.4 µs | 14.1 µs | 20.7 µs | 73.5 µs † | 240.7 µs |
| sincos | 0.7 | 14.0 µs | 16.8 µs | 22.9 µs | 22.5 µs | 63.2 µs | 190.1 µs |
| sinh | 0.7 | 3.3 µs | 7.9 µs | 10.6 µs | 14.4 µs | 66.5 µs | 233.8 µs |
| sinhcosh | 0.7 | 3.8 µs | 6.6 µs | 11.8 µs | 19.8 µs | 64.8 µs | 228.0 µs |
| sqr | 0.7 | 204.8 ns | 230.2 ns | 369.9 ns | 539.6 ns | 770.4 ns | 1.7 µs |
| sqrt | 1.23456789 | 320.9 ns | 640.2 ns | 726.8 ns | 735.3 ns | 1.8 µs | 3.3 µs |
| step | 0.7 | 232.5 ns | 246.6 ns | 322.9 ns | 402.7 ns | 310.6 ns | 412.1 ns |
| struve_H | 2, 0.7 | 176.7 µs | 425.4 µs | 852.6 µs | 1.052 ms | 2.903 ms | 17.807 ms |
| struve_L | 2, 0.7 | 188.3 µs | 430.2 µs | 702.1 µs | 1.022 ms | 3.534 ms | 16.416 ms |
| sub | 0.7, 0.7 | 90.6 ns | 117.7 ns | 103.4 ns | 167.4 ns | 180.6 ns | 387.1 ns |
| tan | 0.7 | 15.7 µs | 21.6 µs † | 24.1 µs | 32.2 µs | 56.4 µs | 215.3 µs |
| tanh | 0.7 | 3.2 µs | 6.5 µs | 12.5 µs | 19.0 µs | 62.0 µs | 237.2 µs |
| tetragamma | 2.345 | 50.8 µs | 120.3 µs | 302.5 µs | 512.8 µs | 2.665 ms | 13.530 ms |
| tri | 0.7 | 1.3 µs | 1.2 µs | 1.3 µs | 1.2 µs | 1.3 µs | 1.3 µs |
| trigamma | 2.345 | 43.1 µs | 120.4 µs | 197.2 µs | 382.8 µs | 1.704 ms | 11.466 ms † |
| vercos | 0.7 | 2.9 µs | 6.6 µs | 8.9 µs | 17.8 µs † | 57.6 µs | 216.5 µs † |
| versin | 0.7 | 3.1 µs | 5.3 µs | 11.3 µs | 14.2 µs | 40.7 µs | 169.2 µs |
| zatahp | 2.3, 1.4 | 698.6 µs | 1.445 ms | 2.911 ms | 3.868 ms | 25.893 ms | 148.986 ms |
| zeta | 2.345 | 1.049 ms | 4.071 ms | 9.285 ms | 1.389 ms | 5.634 ms | 38.100 ms † |
| zetah | 2.3, 1.4 | 792.0 µs | 1.509 ms | 6.190 ms † | 17.546 ms † | 18.638 ms | 151.420 ms |
| zetap | 2.345 | 256.0 µs | 748.8 µs † | 1.008 ms | 6.088 ms † | 7.380 ms | 47.600 ms |

### number / multi-precision complex floating point

| Operation | Inputs | 256 bits | 512 bits | 768 bits | 1024 bits | 2048 bits | 4096 bits |
|---|---|---:|---:|---:|---:|---:|---:|
| E1 | 2.345678+0.125i | 208.2 µs † | 214.5 µs | 330.2 µs | 540.3 µs | 1.771 ms | 8.638 ms |
| Ei | 2.345678+0.125i | 89.1 µs | 190.3 µs | 336.7 µs | 536.4 µs | 1.682 ms | 10.283 ms |
| Li | 2.345+0.125i | 93.3 µs | 254.7 µs | 309.0 µs | 705.7 µs | 2.599 ms | 9.278 ms |
| abs | 0.7+0.125i | 1.5 µs † | 1.4 µs | 1.6 µs | 1.5 µs | 2.9 µs | 8.9 µs |
| acos | 0.7+0.125i | 92.0 µs | 128.8 µs † | 119.0 µs | 79.5 µs | 177.3 µs | 525.6 µs |
| acosec | 2.345+0.125i | 49.6 µs | 72.5 µs | 109.1 µs | 210.2 µs | 364.4 µs | 1.188 ms |
| acosech | 0.7+0.125i | 72.4 µs † | 76.7 µs | 89.5 µs | 197.0 µs | 304.9 µs | 1.184 ms |
| acosh | 2.345+0.125i | 69.7 µs | 82.1 µs | 97.2 µs | 64.4 µs | 266.9 µs | 449.4 µs |
| acot | 0.7+0.125i | 81.1 µs † | 91.7 µs | 106.2 µs | 152.5 µs | 427.8 µs | 892.2 µs |
| acoth | 2.345+0.125i | 132.1 µs † | 62.8 µs | 79.4 µs | 107.2 µs | 230.7 µs | 963.9 µs |
| add | 0.7+0.125i, 0.7+0.125i | 798.6 ns | 440.3 ns | 439.5 ns | 406.0 ns | 459.7 ns | 559.4 ns |
| add_long | 17+0.125i, 3 | 4.2 µs † | 3.4 µs | 2.6 µs | 3.4 µs | 3.2 µs | 2.7 µs |
| appell_F1 | <span style="display:inline-block;width:40ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">0.5+0.125i, 0.7+0.125i, 0.8+0.125i, 2.5+<br/>0.125i, 0.2+0.125i, 0.3+0.125i</span> | 758.5 µs † | 1.460 ms | 2.524 ms | 5.192 ms | 21.524 ms | 127.651 ms |
| arccovercos | 0.5+0.125i | 94.0 µs † | 87.7 µs | 68.9 µs | 145.7 µs | 162.1 µs | 434.2 µs |
| arccoversin | 0.5+0.125i | 80.9 µs | 50.7 µs | 58.1 µs | 187.7 µs | 157.6 µs | 523.8 µs |
| archacovercos | 0.5+0.125i | 16.1 µs † | 19.0 µs | 21.2 µs | 27.1 µs | 44.3 µs | 124.7 µs |
| archacoversin | 0.5+0.125i | 20.3 µs † | 14.3 µs | 22.5 µs | 24.6 µs | 45.8 µs | 115.0 µs |
| archavercos | 0.5+0.125i | 19.8 µs † | 21.1 µs | 21.6 µs | 26.3 µs | 52.1 µs | 146.2 µs |
| archaversin | 0.5+0.125i | 22.9 µs † | 15.9 µs | 23.1 µs | 25.2 µs | 49.1 µs | 124.5 µs |
| arcvercos | 0.5+0.125i | 87.9 µs | 44.4 µs | 129.2 µs | 71.1 µs | 157.1 µs | 523.6 µs |
| arcversin | 0.5+0.125i | 90.8 µs † | 50.0 µs | 111.3 µs | 82.6 µs | 202.7 µs | 435.3 µs |
| arg | 0.7+0.125i | 23.9 µs † | 30.6 µs | 34.3 µs | 49.0 µs | 108.9 µs | 290.4 µs |
| asec | 2.345+0.125i | 72.0 µs † | 70.5 µs | 134.4 µs | 146.3 µs | 333.9 µs | 1.065 ms |
| asech | 0.7+0.125i | 43.7 µs † | 56.6 µs | 65.8 µs | 99.6 µs | 225.5 µs | 505.2 µs |
| asin | 0.7+0.125i | 172.1 µs † | 87.4 µs | 109.8 µs | 135.6 µs | 334.0 µs | 481.8 µs |
| asinh | 0.7+0.125i | 77.0 µs | 72.0 µs | 116.2 µs | 137.2 µs | 348.1 µs | 1.122 ms |
| atan | 0.7+0.125i | 92.0 µs † | 96.5 µs | 91.9 µs | 148.7 µs | 341.1 µs | 859.1 µs |
| atanh | 0.7+0.125i | 41.9 µs | 61.2 µs | 92.5 µs | 126.3 µs | 277.1 µs | 870.5 µs |
| bessel_I | 2, 0.7+0.125i | 298.7 µs † | 475.2 µs | 732.3 µs | 1.199 ms | 4.065 ms | 16.836 ms |
| bessel_J | 2, 0.7+0.125i | 258.1 µs | 548.3 µs † | 870.3 µs | 1.226 ms | 4.295 ms | 16.443 ms |
| bessel_K | 2, 0.7+0.125i | 216.7 µs † | 241.4 µs | 478.8 µs | 738.5 µs | 2.338 ms | 12.875 ms |
| bessel_Y | 2, 0.7+0.125i | 406.5 µs † | 412.2 µs | 708.5 µs | 1.097 ms | 4.360 ms | 18.857 ms |
| ceil | 0.7+0.125i | 1.7 µs | 1.5 µs | 1.6 µs | 1.6 µs | 1.7 µs | 1.6 µs |
| chebyshev_Tn | 2, 0.7+0.125i | 12.3 µs | 11.6 µs | 13.1 µs | 11.5 µs | 12.3 µs | 18.6 µs |
| chebyshev_Un | 2, 0.7+0.125i | 14.7 µs | 14.0 µs | 12.2 µs | 16.6 µs | 15.9 µs | 18.5 µs |
| circ | 0.7 | 2.5 µs | 3.0 µs | 2.4 µs | 4.0 µs | 5.5 µs | 8.2 µs |
| clausen | 3, 0.7+0.125i | 229.9 µs † | 371.7 µs | 715.4 µs | 1.184 ms | 4.651 ms | 23.944 ms |
| clausen2 | 0.7+0.125i | 210.3 µs † | 364.9 µs | 648.2 µs | 1.184 ms | 4.947 ms | 23.930 ms |
| conj | 0.7+0.125i | 431.4 ns | 353.8 ns | 310.9 ns | 292.4 ns | 320.1 ns | 344.3 ns |
| cos | 0.7+0.125i | 27.4 µs † | 17.4 µs | 23.6 µs | 35.7 µs | 119.4 µs | 358.3 µs |
| cosec | 0.7+0.125i | 24.4 µs | 17.5 µs | 27.9 µs | 42.0 µs | 107.3 µs | 393.1 µs |
| cosech | 0.7+0.125i | 25.6 µs † | 17.1 µs | 22.8 µs | 97.7 µs | 91.8 µs | 295.8 µs |
| cosh | 0.7+0.125i | 13.5 µs | 14.3 µs | 22.1 µs | 79.1 µs | 84.6 µs | 286.9 µs |
| cot | 0.7+0.125i | 42.7 µs | 38.5 µs | 48.9 µs | 91.1 µs | 222.4 µs | 818.5 µs |
| coth | 0.7+0.125i | 19.6 µs | 31.1 µs | 47.6 µs | 147.8 µs | 203.1 µs | 619.0 µs |
| covercos | 0.7+0.125i | 23.5 µs | 18.1 µs | 33.6 µs | 41.2 µs | 107.9 µs | 354.4 µs |
| coversin | 0.7+0.125i | 20.8 µs | 16.8 µs | 28.0 µs | 50.6 µs | 105.1 µs | 371.8 µs |
| cubrt | 0.7+0.125i | 48.5 µs | 74.3 µs | 88.7 µs | 177.8 µs | 477.2 µs | 1.267 ms |
| digamma | 2.345+0.125i | 281.5 µs | 615.6 µs | 978.4 µs | 1.768 ms | 4.217 ms | 20.915 ms |
| dilog | 0.7+0.125i | 206.6 µs | 448.1 µs | 932.7 µs | 2.250 ms † | 6.381 ms | 25.803 ms |
| div | 0.7+0.125i, 0.7+0.125i | 7.0 µs | 14.5 µs | 14.6 µs | 28.2 µs | 44.8 µs | 104.5 µs |
| erf | 0.7+0.125i | 87.4 µs | 188.7 µs | 312.5 µs | 619.1 µs | 2.006 ms | 8.657 ms |
| erfc | 0.7+0.125i | 80.8 µs | 217.6 µs | 308.6 µs | 693.9 µs | 2.100 ms | 7.949 ms |
| erfcinv | 0.5+0.125i | 764.1 µs | 1.738 ms | 1.913 ms | 3.973 ms | 11.716 ms | 50.151 ms |
| erfinv | 0.5+0.125i | 953.8 µs | 1.685 ms | 2.569 ms | 3.784 ms | 12.417 ms | 47.078 ms |
| exp | 1.23456789+0.125i | 10.9 µs | 21.9 µs | 32.4 µs | 61.1 µs | 177.0 µs | 813.0 µs |
| floor | 0.7+0.125i | 270.8 ns | 312.6 ns | 379.8 ns | 281.1 ns | 327.2 ns | 342.6 ns |
| gamma | 2.345+0.125i | 236.7 µs † | 454.7 µs † | 898.3 µs † | 1.651 ms † | 6.074 ms † | 25.124 ms † |
| gammainc_P | 2.3+0.125i, 1.4+0.125i | 725.5 µs † | 1.439 ms † | 2.522 ms † | 3.612 ms † | 11.988 ms † | 48.070 ms † |
| gammainc_Q | 2.3+0.125i, 1.4+0.125i | 734.1 µs † | 1.767 ms † | 2.622 ms † | 4.192 ms † | 11.122 ms † | 50.357 ms † |
| gammainc_lower | 2.3+0.125i, 1.4+0.125i | 455.5 µs † | 905.2 µs † | 1.525 ms † | 2.606 ms † | 7.079 ms † | 22.421 ms † |
| gammainc_upper | 2.3+0.125i, 1.4+0.125i | 756.9 µs † | 1.420 ms † | 3.278 ms † | 4.077 ms † | 12.761 ms † | 48.500 ms † |
| gammainv | 9.5+0.125i | 4.386 ms † | 7.432 ms † | 13.313 ms † | 18.208 ms † | 81.807 ms † | 272.487 ms † |
| hacovercos | 0.7+0.125i | 25.4 µs | 23.9 µs | 41.5 µs | 40.4 µs | 117.3 µs | 344.2 µs |
| hacoversin | 0.7+0.125i | 23.9 µs | 22.2 µs | 34.9 µs | 43.4 µs | 123.7 µs | 438.6 µs |
| harmonic_poly | 2, 0.7+0.125i | 18.1 µs | 20.4 µs | 17.7 µs | 21.5 µs | 36.5 µs | 80.3 µs |
| havercos | 0.7+0.125i | 29.1 µs | 20.4 µs | 32.5 µs | 46.1 µs | 111.9 µs | 405.0 µs |
| haversin | 0.7+0.125i | 24.7 µs | 24.1 µs | 33.3 µs | 45.2 µs | 113.6 µs | 367.6 µs |
| hermite_Hn | 2, 0.7+0.125i | 13.7 µs | 16.8 µs | 13.6 µs | 13.6 µs | 19.7 µs | 22.0 µs |
| hypergeometric_pFq | 0.7+0.125i, 2.5+0.125i, 0.3+0.125i | 277.0 µs † | 524.0 µs † | 950.0 µs † | 1.526 ms † | 4.071 ms † | 16.523 ms † |
| imag_part | 0.7+0.125i | 144.4 ns | 140.7 ns | 131.5 ns | 132.0 ns | 122.8 ns | 192.9 ns |
| inv | 0.7+0.125i | 1.3 µs | 1.6 µs | 2.1 µs | 2.9 µs | 5.0 µs | 13.3 µs |
| ldexp | 1.23456789+0.125i, 3 | 491.7 ns | 356.5 ns | 330.8 ns | 427.2 ns | 413.7 ns | 338.3 ns |
| legendre_chi | 2, 0.7+0.125i | 163.7 µs | 308.7 µs | 382.9 µs | 696.1 µs | 2.018 ms | 7.355 ms |
| lerch_phi | 0.3+0.125i, 2+0.125i, 1.4+0.125i | 319.8 µs | 861.8 µs | 1.885 ms † | 3.187 ms † | 15.640 ms † | 57.820 ms † |
| lg | 2.345678+0.125i | 26.1 µs | 37.4 µs | 57.1 µs | 93.7 µs | 198.9 µs | 465.2 µs |
| lgamma | 2.345+0.125i | 168.9 µs † | 495.1 µs † | 846.2 µs † | 1.374 ms † | 5.013 ms † | 21.633 ms † |
| ln | 2.345678+0.125i | 20.6 µs | 31.7 µs | 40.2 µs | 57.6 µs | 190.6 µs | 355.4 µs |
| log | 2.345678+0.125i | 24.8 µs | 31.7 µs | 39.5 µs | 65.1 µs | 174.3 µs | 400.6 µs |
| log10 | 2.345678+0.125i | 24.3 µs | 56.0 µs | 55.5 µs | 85.8 µs | 180.7 µs | 572.8 µs |
| lommel_S | 0.5+0.125i, 0.7+0.125i, 1.2+0.125i | 244.6 µs | 432.9 µs | 843.5 µs | 1.334 ms | 7.216 ms † | 21.843 ms |
| mul | 0.7+0.125i, 0.7+0.125i | 1.6 µs | 1.8 µs | 2.0 µs | 3.1 µs | 11.4 µs † | 7.1 µs |
| mul_long | 17+0.125i, 3 | 4.1 µs | 4.1 µs | 3.6 µs | 3.1 µs | 4.9 µs | 5.1 µs |
| mul_pow10 | 1.23456789+0.125i, 3 | 3.4 µs | 3.2 µs | 4.3 µs | 3.1 µs | 4.0 µs | 4.1 µs |
| neg | 0.7+0.125i | 343.0 ns | 288.0 ns | 370.4 ns | 362.3 ns | 354.6 ns | 319.5 ns |
| normal_cdf | 0.7+0.125i | 89.3 µs | 196.5 µs | 408.9 µs | 547.6 µs | 2.271 ms | 8.576 ms |
| normal_logpdf | 0.7+0.125i | 7.3 µs | 13.6 µs | 20.6 µs | 18.5 µs | 40.4 µs | 122.4 µs |
| normal_pdf | 0.7+0.125i | 21.0 µs | 32.0 µs | 55.5 µs | 102.3 µs | 250.9 µs | 940.1 µs |
| polygamma | 3, 2.345+0.125i | 326.8 µs | 748.2 µs | 1.267 ms | 2.267 ms | 8.305 ms | 30.160 ms |
| polylog | 2, 0.7+0.125i | 171.3 µs | 408.3 µs | 927.0 µs | 1.652 ms | 6.076 ms | 26.772 ms |
| polylog1 | 0.7+0.125i | 29.9 µs | 41.0 µs | 70.6 µs | 106.1 µs | 165.7 µs | 434.4 µs |
| pow | 1.23456789+0.125i, 3.5+0.125i | 32.6 µs | 65.4 µs | 90.2 µs | 132.2 µs | 345.1 µs | 1.422 ms |
| pow10 | 3 | 11.5 µs | 18.3 µs | 25.1 µs | 38.4 µs | 87.7 µs † | 189.5 µs |
| pow_int | 1.23456789+0.125i, 3 | 6.8 µs | 7.7 µs | 11.7 µs | 12.0 µs | 12.8 µs | 23.7 µs |
| real_part | 0.7+0.125i | 142.2 ns | 132.3 ns | 147.7 ns | 131.2 ns | 154.6 ns | 188.2 ns |
| rect | 0.7 | 2.6 µs | 3.0 µs | 4.0 µs | 4.0 µs | 4.1 µs | 8.7 µs |
| root | 1.2345+0.125i, 3 | 41.7 µs | 57.1 µs | 186.8 µs | 279.6 µs | 382.0 µs | 1.427 ms |
| round | 0.7+0.125i | 10.3 µs | 11.4 µs | 9.0 µs | 10.1 µs | 10.3 µs | 12.7 µs |
| roundn | 1.23456789+0.125i, 3 | 19.2 µs | 14.5 µs | 18.3 µs | 19.4 µs | 22.8 µs | 34.6 µs |
| rounds | 1.23456789+0.125i, 3 | 43.1 µs | 67.0 µs | 81.6 µs | 88.2 µs | 211.1 µs | 550.8 µs |
| sec | 0.7+0.125i | 19.5 µs | 20.2 µs | 27.4 µs | 34.4 µs | 121.6 µs | 342.2 µs |
| sech | 0.7+0.125i | 14.0 µs | 15.2 µs | 24.8 µs | 85.9 µs | 89.6 µs | 292.0 µs |
| sgn | 0.7 | 788.8 ns | 784.3 ns | 776.7 ns | 969.4 ns | 815.0 ns | 922.0 ns |
| sin | 0.567+0.125i | 17.2 µs | 17.5 µs | 25.9 µs | 39.0 µs | 110.1 µs | 418.2 µs |
| sinc | 0.7+0.125i | 20.9 µs | 30.5 µs | 43.1 µs | 50.0 µs | 129.2 µs | 424.7 µs |
| sinh | 0.7+0.125i | 8.1 µs | 15.8 µs | 28.5 µs | 84.5 µs | 83.6 µs | 281.8 µs |
| sqr | 0.7+0.125i | 1.8 µs | 2.0 µs | 2.2 µs | 3.0 µs | 4.7 µs | 10.6 µs † |
| sqrt | 1.23456789+0.125i | 2.9 µs | 2.5 µs | 3.7 µs | 5.5 µs | 6.7 µs | 15.2 µs |
| step | 0.7 | 820.5 ns | 838.2 ns | 878.9 ns | 954.4 ns | 1.1 µs | 793.2 ns |
| struve_H | 2, 0.7+0.125i | 265.6 µs | 686.6 µs | 945.4 µs | 1.584 ms | 6.746 ms | 21.221 ms |
| struve_L | 2, 0.7+0.125i | 316.5 µs | 589.6 µs | 1.353 ms | 1.687 ms | 5.180 ms | 21.095 ms |
| sub | 0.7+0.125i, 0.7+0.125i | 420.3 ns | 450.0 ns | 371.5 ns | 432.6 ns | 400.7 ns | 571.3 ns |
| tan | 0.7+0.125i | 15.2 µs | 57.3 µs † | 29.2 µs | 47.6 µs | 122.8 µs | 384.9 µs |
| tanh | 0.7+0.125i | 33.4 µs | 23.9 µs | 83.0 µs | 41.5 µs | 99.3 µs | 320.4 µs |
| tetragamma | 2.345+0.125i | 364.9 µs | 792.6 µs | 1.250 ms | 1.997 ms | 6.922 ms | 30.391 ms |
| tri | 0.7 | 2.3 µs | 2.8 µs | 2.8 µs | 2.8 µs | 5.0 µs | 8.0 µs |
| trigamma | 2.345+0.125i | 310.3 µs | 537.4 µs | 1.174 ms | 1.727 ms | 5.537 ms | 27.328 ms |
| vercos | 0.7+0.125i | 17.2 µs | 17.2 µs | 30.2 µs | 40.2 µs | 109.2 µs | 317.7 µs |
| versin | 0.7+0.125i | 22.2 µs | 23.8 µs | 26.4 µs | 46.1 µs | 106.4 µs | 364.4 µs |
| zatahp | 2.3+0.125i, 1.4+0.125i | 2.762 ms | 4.732 ms | 11.396 ms | 14.287 ms | 89.465 ms | 470.201 ms |
| zeta | 2.345+0.125i | 639.8 µs | 1.658 ms † | 2.421 ms | 4.017 ms | 19.855 ms | 96.722 ms |
| zetah | 2.3+0.125i, 1.4+0.125i | 2.280 ms | 15.138 ms † | 9.850 ms | 15.878 ms | 83.732 ms | 436.983 ms |
| zetap | 2.345+0.125i | 988.5 µs † | 1.706 ms | 2.974 ms | 4.932 ms | 26.077 ms | 120.905 ms |

### number / unrestricted integer

| Operation | Inputs | Exact |
|---|---|---:|
| abs | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">-9712853409152031141207811585727780469484705121206232579607434236969009533618825<br/>97201918971499157056640605432269691427807258223374967278846596194834524005116336<br/>17514882293623272276764857474830401626415841035971458657442133370777607803464071<br/>101158385124186229995073045427448918227959893082282919863982307657776</span> | 171.1 ns |
| add | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 121.0 ns |
| bit_and | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 424.5 ns |
| bit_length | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 186.7 ns |
| bit_not | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 358.2 ns |
| bit_or | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 362.0 ns |
| bit_xor | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 509.2 ns |
| clear_bit | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>701</span> | 548.0 ns |
| div | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>340282366920938463463374607431768211454</span> | 1.6 µs |
| divmod | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 482.7 ns |
| factorial | 5000 | 162.2 µs |
| factors | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">54772119019154636339867862041163448790974484131459861520687327809209914339836012<br/>52275954799541486775711126005967382600118182797233708133775335788335658037065081<br/>31569062299976033041302189833215742992653063887339218309467131548996199881113600<br/>00000000000000000000000000000000000000000000000000000000000000000000000000000000<br/>0000000000000000000000000000000000000000000000</span> | 135.0 µs |
| fibonacci | 10000 | 7.7 µs |
| gcd | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 3.0 µs |
| gcdext | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 4.8 µs |
| is_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 549.4 µs |
| isqrt | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 580.6 ns |
| lcm | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 3.2 µs |
| mod | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 467.3 ns |
| modinv | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 5.8 µs † |
| mul | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 318.1 ns |
| neg | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 99.9 ns |
| next_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 5.058 ms |
| partition | 5000 | 7.885 ms |
| pow | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>5</span> | 6.4 µs † |
| pow_int | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>5</span> | 3.1 µs |
| powmod | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662, <br/>68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 151.1 µs † |
| prev_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 8.777 ms † |
| prove_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 4.657 ms |
| set_bit | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>700</span> | 441.6 ns |
| shl | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>257</span> | 575.3 ns |
| shr | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>257</span> | 430.5 ns |
| sqr | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 290.8 ns |
| sub | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 109.0 ns |
| test_bit | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>701</span> | 433.3 ns |

### number / unrestricted fraction

`number_t` uses exact GMP rational arithmetic (MPQ). The operands below have large integer
numerators and denominators; parsing and canonical reduction precede timing. Every result
is checked to remain an exact fraction. Arithmetic cost depends on operand sizes and
cancellation, rather than a floating-point precision. These workloads use a 16,384-bit
rational size budget so intermediate results remain exact; general transcendental
functions usually require inexact real or complex results.

| Operation | Inputs | Exact |
|---|---|---:|
| abs | $\frac{\begin{array}{l}\text{-9712853409152031141207811585727780469484705121206232579607434236969009533618825} \\ \text{97201918971499157056640605432269691427807258223374967278846596194834524005116336} \\ \text{17514882293623272276764857474830401626415841035971458657442133370777607803464071} \\ \text{101158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$ | 350.3 ns |
| add | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>$\frac{\begin{array}{l}\text{12498846048367045926370716602609982839721641429222959125376462582674869403443996} \\ \text{972648628699047686839582654905384579341977328653591547736951147466435189662}\end{array}}{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}$ | 5.2 µs † |
| div | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>$\frac{\begin{array}{l}\text{12498846048367045926370716602609982839721641429222959125376462582674869403443996} \\ \text{972648628699047686839582654905384579341977328653591547736951147466435189662}\end{array}}{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}$ | 10.0 µs † |
| inv | $\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$ | 239.2 ns |
| mul | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>$\frac{\begin{array}{l}\text{12498846048367045926370716602609982839721641429222959125376462582674869403443996} \\ \text{972648628699047686839582654905384579341977328653591547736951147466435189662}\end{array}}{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}$ | 4.0 µs |
| neg | $\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$ | 196.1 ns |
| pow | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>5 | 41.4 µs |
| pow_int | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>5 | 42.6 µs † |
| sqr | $\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$ | 682.2 ns |
| sub | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>$\frac{\begin{array}{l}\text{12498846048367045926370716602609982839721641429222959125376462582674869403443996} \\ \text{972648628699047686839582654905384579341977328653591547736951147466435189662}\end{array}}{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}$ | 5.8 µs |

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
| `num_chebyshev_Tn` | First-kind Chebyshev, $T_0=1$, $T_1=x$ |
| `num_chebyshev_Un` | Second-kind Chebyshev, $U_0=1$, $U_1=2x$ |
| `num_hermite_Hn` | Physicists' Hermite, $H_0=1$, $H_1=2x$ |

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
    number_t value = num_hermite_Hn(num_create_from_long(3), NUM_TWO);
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

/* Keep benchmark fraction digits readable without wrapping math in raw HTML. */
body td > .katex:has(.mfrac) {
    display: inline-block;
    font-size: 1em;
    margin: 0.6em 0;
}
</style>
