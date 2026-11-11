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
Benchmark run **29**, 2026-11-11T21:56:14.598Z (UTC).

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
| E1 | 2.345678 | 9.1 µs |
| Ei | 2.345678 | 10.7 µs |
| Li | 2.345 | 20.4 µs † |
| abs | 0.7 | 9.6 ns |
| acos | 0.7 | 24.2 ns |
| acosec | 2.345 | 23.6 ns |
| acosech | 0.7 | 70.8 ns |
| acosh | 2.345 | 49.0 ns |
| acot | 0.7 | 65.4 ns |
| acoth | 2.345 | 40.9 ns |
| add | 0.7, 0.7 | 17.4 ns |
| add_long | 17, 3 | 597.3 ns |
| appell_f1 | 0.5, 0.7, 0.8, 2.5, 0.2, 0.3 | 34.8 µs |
| arccovercos | 0.5 | 142.6 ns |
| arccoversin | 0.5 | 145.7 ns |
| archacovercos | 0.5 | 279.4 ns |
| archacoversin | 0.5 | 279.5 ns |
| archavercos | 0.5 | 269.5 ns |
| archaversin | 0.5 | 313.7 ns |
| arcvercos | 0.5 | 166.7 ns |
| arcversin | 0.5 | 169.3 ns |
| arg | 0.7 | 101.1 ns |
| asec | 2.345 | 28.9 ns |
| asech | 0.7 | 48.6 ns |
| asin | 0.7 | 26.8 ns |
| asinh | 0.7 | 68.0 ns |
| atan | 0.7 | 46.5 ns |
| atan2 | 0.5, -0.75 | 54.5 ns |
| atanh | 0.7 | 37.5 ns |
| bessel_i | 2, 0.7 | 53.6 µs |
| bessel_j | 2, 0.7 | 7.7 µs |
| bessel_k | 2, 0.7 | 32.8 µs |
| bessel_y | 2, 0.7 | 25.9 µs |
| beta | 2.3, 4.5 | 206.4 ns |
| beta_pdf | 0.5, 2.5, 3.5 | 240.1 ns |
| binomial | 5, 2 | 216.5 ns |
| ceil | 0.7 | 77.9 ns |
| chebyshev_t | 2, 0.7 | 906.6 ns |
| chebyshev_u | 2, 0.7 | 1.7 µs † |
| circ | 0.7 | 373.0 ns |
| clausen | 3, 0.7 | 10.4 µs |
| clausen2 | 0.7 | 9.5 µs |
| conj | 0.7 | 11.3 ns |
| cos | 0.7 | 51.7 ns |
| cosec | 0.7 | 46.1 ns |
| cosech | 0.7 | 63.3 ns |
| cosh | 0.7 | 46.9 ns |
| cot | 0.7 | 56.5 ns |
| coth | 0.7 | 77.9 ns |
| covercos | 0.7 | 158.8 ns |
| coversin | 0.7 | 185.1 ns |
| cubrt | 0.7 | 10.5 µs † |
| digamma | 2.345 | 3.1 µs |
| dilog | 0.7 | 19.4 µs |
| div | 0.7, 0.7 | 15.6 ns |
| erf | 0.7 | 90.0 ns |
| erfc | 0.7 | 101.1 ns |
| erfcinv | 0.5 | 314.7 µs |
| erfinv | 0.5 | 249.8 µs |
| exp | 1.23456789 | 57.2 ns |
| floor | 0.7 | 12.7 ns |
| gamma | 2.345 | 136.8 ns |
| gammainc_P | 2.3, 1.4 | 15.7 µs |
| gammainc_Q | 2.3, 1.4 | 16.3 µs |
| gammainc_lower | 2.3, 1.4 | 16.5 µs |
| gammainc_upper | 2.3, 1.4 | 20.3 µs † |
| gammainv | 9.5 | 117.7 µs |
| hacovercos | 0.7 | 333.4 ns |
| hacoversin | 0.7 | 333.4 ns |
| harmonic_poly | 2, 0.7 | 2.0 µs |
| havercos | 0.7 | 315.2 ns |
| haversin | 0.7 | 340.8 ns |
| hermite_h | 2, 0.7 | 2.4 µs † |
| hypergeometric_pFq | 0.7, 2.5, 0.3 | 5.7 µs † |
| hypot | 0.7, 1.2 | 34.3 ns |
| imag_part | 0.7 | 29.6 ns |
| inv | 0.7 | 13.6 ns |
| lambert_w0 | 0.7 | 22.0 µs † |
| lambert_wm1 | -0.2 | 18.8 µs |
| lambert_wn | 1, 0.7 | 38.4 µs † |
| lauricella_f | 0.5, 0.3, 2.5 | 26.7 µs † |
| ldexp | 1.23456789, 3 | 70.5 ns |
| legendre_chi | 2, 0.7 | 35.7 µs |
| lerch_phi | 0.3, 2, 1.4 | 340.3 µs |
| lg | 2.345678 | 30.3 ns |
| lgamma | 2.345 | 62.2 ns |
| ln | 2.345678 | 22.1 ns |
| log | 2.345678 | 17.9 ns |
| log10 | 2.345678 | 32.3 ns |
| logbeta | 2.3, 4.5 | 162.6 ns |
| logbeta_pdf | 0.5, 2.5, 3.5 | 230.9 ns |
| lommel_s | 0.5, 0.7, 1.2 | 9.9 µs |
| mul | 0.7, 0.7 | 16.9 ns |
| mul_long | 17, 3 | 642.3 ns |
| mul_pow10 | 1.23456789, 3 | 94.5 ns |
| neg | 0.7 | 12.7 ns |
| normal_cdf | 0.7 | 119.8 ns |
| normal_logpdf | 0.7 | 19.7 ns |
| normal_pdf | 0.7 | 30.1 ns |
| polygamma | 3, 2.345 | 31.9 µs |
| polylog | 2, 0.7 | 16.6 µs |
| polylog1 | 0.7 | 201.3 ns |
| pow | 1.23456789, 3.5 | 49.9 ns |
| pow10 | 3 | 215.2 ns |
| pow_int | 1.23456789, 3 | 95.4 ns |
| productlog | 0.7 | 19.8 µs |
| qdigamma | 0.5, 2.3 | 37.1 µs † |
| real_part | 0.7 | 37.7 ns |
| rect | 0.7 | 422.7 ns |
| root | 1.2345, 3 | 8.4 µs |
| round | 0.7 | 625.9 ns |
| roundn | 1.23456789, 3 | 5.4 µs |
| rounds | 1.23456789, 3 | 9.5 µs † |
| sec | 0.7 | 39.5 ns |
| sech | 0.7 | 58.9 ns |
| sgn | 0.7 | 180.5 ns |
| sin | 0.567 | 57.1 ns |
| sinc | 0.7 | 5.2 µs † |
| sincos | 0.7 | 1.2 µs † |
| sinh | 0.7 | 69.4 ns |
| sinhcosh | 0.7 | 172.7 ns |
| sqr | 0.7 | 11.7 ns |
| sqrt | 1.23456789 | 12.2 ns |
| step | 0.7 | 196.5 ns |
| struve_h | 2, 0.7 | 99.8 µs |
| struve_l | 2, 0.7 | 88.5 µs |
| sub | 0.7, 0.7 | 18.9 ns |
| tan | 0.7 | 8.3 µs |
| tanh | 0.7 | 78.1 ns |
| tetragamma | 2.345 | 5.0 µs † |
| tri | 0.7 | 336.9 ns |
| trigamma | 2.345 | 3.7 µs |
| vercos | 0.7 | 188.2 ns |
| versin | 0.7 | 195.2 ns |
| zatahp | 2.3, 1.4 | 486.6 µs |
| zeta | 2.345 | 89.9 µs |
| zetah | 2.3, 1.4 | 404.4 µs |
| zetap | 2.345 | 180.1 µs |

### number / multi-precision floating point

| Operation | Inputs | 256 bits | 512 bits | 768 bits | 1024 bits | 2048 bits | 4096 bits |
|---|---|---:|---:|---:|---:|---:|---:|
| E1 | 2.345678 | 55.3 µs | 88.7 µs | 155.7 µs | 209.6 µs | 576.2 µs | 2.538 ms |
| Ei | 2.345678 | 55.6 µs | 93.0 µs | 142.8 µs | 216.9 µs | 647.2 µs | 2.598 ms |
| Li | 2.345 | 54.0 µs † | 84.4 µs | 147.5 µs | 202.0 µs | 538.1 µs | 2.218 ms |
| abs | 0.7 | 106.8 ns | 145.8 ns | 106.2 ns | 134.0 ns | 131.1 ns | 122.2 ns |
| acos | 0.7 | 21.7 µs † | 42.5 µs † | 51.9 µs | 70.4 µs | 120.4 µs | 515.6 µs |
| acosec | 2.345 | 21.4 µs | 36.9 µs | 57.6 µs † | 69.8 µs | 163.1 µs | 426.1 µs |
| acosech | 0.7 | 8.2 µs | 11.1 µs | 16.5 µs | 24.7 µs † | 46.6 µs | 155.3 µs |
| acosh | 2.345 | 8.2 µs | 14.3 µs | 16.9 µs | 24.8 µs | 56.4 µs | 165.3 µs |
| acot | 0.7 | 20.2 µs | 32.5 µs | 37.7 µs | 60.5 µs | 146.5 µs | 391.9 µs |
| acoth | 2.345 | 8.4 µs † | 12.4 µs | 15.9 µs | 24.0 µs | 55.9 µs | 138.4 µs |
| add | 0.7, 0.7 | 122.6 ns | 127.3 ns | 153.5 ns | 134.6 ns | 213.0 ns | 347.4 ns |
| add_long | 17, 3 | 826.7 ns | 991.5 ns | 880.5 ns | 1.1 µs | 979.3 ns | 922.6 ns |
| appell_f1 | 0.5, 0.7, 0.8, 2.5, 0.2, 0.3 | 185.0 µs † | 412.8 µs | 714.5 µs | 1.353 ms | 6.215 ms | 30.577 ms |
| arccovercos | 0.5 | 24.1 µs † | 30.0 µs | 35.8 µs | 54.4 µs | 126.0 µs | 337.3 µs |
| arccoversin | 0.5 | 21.5 µs † | 31.9 µs | 43.1 µs | 74.9 µs † | 119.0 µs | 335.2 µs |
| archacovercos | 0.5 | 1.5 µs | 1.7 µs | 2.4 µs | 1.9 µs | 1.9 µs | 2.3 µs † |
| archacoversin | 0.5 | 1.9 µs | 1.9 µs | 1.9 µs | 2.2 µs | 1.7 µs | 2.4 µs † |
| archavercos | 0.5 | 1.7 µs | 1.8 µs | 2.2 µs | 2.1 µs | 2.3 µs | 1.8 µs |
| archaversin | 0.5 | 1.7 µs | 2.1 µs | 2.1 µs | 2.1 µs † | 1.7 µs | 2.6 µs |
| arcvercos | 0.5 | 20.7 µs | 31.5 µs | 37.2 µs | 60.4 µs | 148.0 µs | 410.4 µs † |
| arcversin | 0.5 | 18.4 µs | 35.4 µs † | 55.6 µs † | 61.0 µs | 125.7 µs | 351.8 µs |
| arg | 0.7 | 716.6 ns | 682.8 ns | 646.2 ns | 888.4 ns | 695.2 ns | 653.6 ns |
| asec | 2.345 | 22.0 µs | 41.6 µs † | 49.0 µs | 70.7 µs | 150.5 µs | 444.0 µs |
| asech | 0.7 | 9.1 µs † | 13.4 µs | 17.6 µs | 21.0 µs | 51.9 µs | 157.0 µs |
| asin | 0.7 | 22.5 µs | 38.0 µs | 41.0 µs | 59.5 µs | 137.7 µs | 394.8 µs |
| asinh | 0.7 | 8.0 µs † | 11.9 µs | 17.6 µs | 23.2 µs | 49.4 µs | 139.7 µs |
| atan | 0.7 | 16.4 µs | 29.0 µs | 44.1 µs | 57.7 µs | 117.9 µs | 399.9 µs |
| atan2 | 0.5, -0.75 | 19.8 µs † | 34.7 µs † | 43.6 µs | 54.6 µs | 128.3 µs | 407.6 µs |
| atanh | 0.7 | 7.1 µs | 13.5 µs | 17.6 µs | 22.6 µs | 55.4 µs | 149.5 µs |
| bessel_i | 2, 0.7 | 135.2 µs | 276.4 µs | 409.8 µs | 664.2 µs | 2.132 ms | 11.387 ms |
| bessel_j | 2, 0.7 | 8.4 µs | 20.6 µs | 36.7 µs | 47.9 µs | 232.1 µs | 965.3 µs |
| bessel_k | 2, 0.7 | 62.4 µs | 132.0 µs | 214.9 µs | 311.1 µs | 1.043 ms | 5.183 ms |
| bessel_y | 2, 0.7 | 60.1 µs | 120.1 µs | 188.9 µs | 231.0 µs | 1.334 ms † | 4.699 ms |
| beta | 2.3, 4.5 | 156.6 µs † | 365.6 µs | 630.0 µs | 1.076 ms | 4.311 ms | 17.012 ms |
| beta_pdf | 0.5, 2.5, 3.5 | 141.3 µs | 245.4 µs | 566.3 µs | 654.5 µs | 3.050 ms | 11.572 ms |
| binomial | 5, 2 | 132.3 µs | 310.2 µs | 627.6 µs † | 997.4 µs | 4.150 ms | 14.480 ms |
| ceil | 0.7 | 730.3 ns | 618.8 ns | 675.6 ns | 789.4 ns | 614.6 ns | 738.8 ns |
| chebyshev_t | 2, 0.7 | 3.7 µs † | 4.2 µs | 4.4 µs | 3.4 µs | 5.2 µs | 8.5 µs |
| chebyshev_u | 2, 0.7 | 3.8 µs | 4.7 µs | 4.8 µs | 4.3 µs | 5.6 µs | 8.6 µs |
| circ | 0.7 | 1.1 µs | 1.1 µs | 1.1 µs | 1.1 µs | 1.2 µs | 1.5 µs |
| clausen | 3, 0.7 | 35.0 µs | 95.9 µs † | 190.2 µs † | 284.7 µs | 1.161 ms | 6.679 ms |
| clausen2 | 0.7 | 33.3 µs | 87.0 µs | 197.2 µs | 345.3 µs † | 1.009 ms | 5.709 ms |
| conj | 0.7 | 103.2 ns | 100.5 ns | 144.5 ns | 105.0 ns | 120.7 ns | 134.2 ns |
| cos | 0.7 | 2.8 µs | 5.5 µs | 11.6 µs † | 15.3 µs | 67.1 µs † | 191.4 µs |
| cosec | 0.7 | 4.4 µs | 8.2 µs | 11.5 µs | 17.0 µs | 51.7 µs | 185.0 µs |
| cosech | 0.7 | 4.1 µs † | 7.1 µs | 14.3 µs | 20.7 µs | 61.3 µs | 221.4 µs |
| cosh | 0.7 | 4.1 µs † | 7.6 µs | 12.4 µs | 18.9 µs | 62.5 µs | 247.9 µs |
| cot | 0.7 | 7.0 µs | 12.0 µs | 23.2 µs | 29.6 µs | 118.4 µs † | 439.1 µs |
| coth | 0.7 | 6.9 µs | 17.6 µs | 25.7 µs | 33.8 µs | 136.4 µs † | 534.0 µs |
| covercos | 0.7 | 5.3 µs † | 10.3 µs † | 15.6 µs † | 19.5 µs | 48.4 µs | 212.4 µs |
| coversin | 0.7 | 4.9 µs | 9.6 µs † | 12.3 µs | 16.8 µs | 48.8 µs | 217.4 µs † |
| cubrt | 0.7 | 18.7 µs † | 29.4 µs † | 33.7 µs | 51.4 µs † | 139.2 µs | 415.3 µs † |
| digamma | 2.345 | 148.6 µs † | 326.9 µs | 634.2 µs | 1.252 ms | 5.025 ms | 22.763 ms |
| dilog | 0.7 | 132.8 µs | 305.6 µs | 666.7 µs † | 1.077 ms | 4.724 ms | 23.967 ms |
| div | 0.7, 0.7 | 443.7 ns | 743.2 ns | 982.3 ns | 3.5 µs † | 3.9 µs | 13.3 µs |
| erf | 0.7 | 21.8 µs | 53.4 µs | 92.3 µs | 295.5 µs † | 596.7 µs | 2.556 ms |
| erfc | 0.7 | 22.6 µs | 56.4 µs | 77.2 µs | 403.8 µs † | 608.6 µs | 2.562 ms |
| erfcinv | 0.5 | 529.3 µs † | 702.1 µs | 882.5 µs | 1.540 ms | 4.086 ms | 15.788 ms |
| erfinv | 0.5 | 525.5 µs † | 628.4 µs | 751.8 µs | 1.605 ms † | 3.616 ms | 14.362 ms |
| exp | 1.23456789 | 10.0 µs † | 7.7 µs | 11.9 µs | 23.9 µs | 65.1 µs | 210.2 µs |
| floor | 0.7 | 145.0 ns | 110.5 ns | 111.1 ns | 138.1 ns | 114.2 ns | 140.2 ns |
| gamma | 2.345 | 45.9 µs | 99.0 µs | 179.5 µs | 351.4 µs † | 1.309 ms | 6.765 ms † |
| gammainc_P | 2.3, 1.4 | 160.7 µs † | 351.8 µs | 605.6 µs | 844.5 µs | 3.848 ms | 16.183 ms |
| gammainc_Q | 2.3, 1.4 | 127.5 µs | 305.6 µs | 971.9 µs | 1.475 ms | 7.408 ms † | 30.428 ms |
| gammainc_lower | 2.3, 1.4 | 93.6 µs | 213.0 µs | 369.8 µs † | 599.9 µs | 2.559 ms † | 11.170 ms |
| gammainc_upper | 2.3, 1.4 | 133.5 µs | 326.9 µs | 1.066 ms | 1.810 ms † | 6.654 ms | 31.626 ms |
| gammainv | 9.5 | 930.6 µs | 2.378 ms † | 3.290 ms | 5.735 ms | 17.145 ms | 77.310 ms |
| hacovercos | 0.7 | 6.2 µs † | 11.2 µs † | 18.4 µs † | 25.5 µs † | 58.3 µs | 246.0 µs † |
| hacoversin | 0.7 | 7.6 µs † | 9.5 µs | 14.4 µs | 20.2 µs | 68.4 µs | 194.9 µs |
| harmonic_poly | 2, 0.7 | 6.4 µs † | 5.8 µs † | 7.2 µs | 8.7 µs | 14.1 µs | 40.9 µs |
| havercos | 0.7 | 5.8 µs † | 8.7 µs | 14.0 µs † | 18.9 µs | 54.4 µs | 187.7 µs |
| haversin | 0.7 | 4.9 µs | 7.2 µs | 12.3 µs | 22.9 µs | 48.9 µs | 248.3 µs |
| hermite_h | 2, 0.7 | 3.9 µs | 5.1 µs | 5.5 µs | 4.8 µs | 6.4 µs | 8.3 µs |
| hypergeometric_pFq | 0.7, 2.5, 0.3 | 6.7 µs † | 5.8 µs | 8.3 µs † | 5.5 µs | 6.6 µs | 5.9 µs |
| hypot | 0.7, 1.2 | 1.0 µs | 1.2 µs | 1.5 µs | 2.0 µs | 3.4 µs | 10.5 µs |
| imag_part | 0.7 | 93.2 ns | 95.0 ns | 121.2 ns | 99.2 ns | 115.5 ns | 109.2 ns |
| inv | 0.7 | 264.3 ns | 415.0 ns | 504.8 ns | 607.0 ns | 1.7 µs | 5.2 µs |
| lambert_w0 | 0.7 | 73.4 µs | 136.4 µs † | 228.1 µs | 413.1 µs † | 1.163 ms † | 3.429 ms |
| lambert_wm1 | -0.2 | 81.4 µs † | 135.0 µs | 236.9 µs | 404.2 µs | 1.010 ms | 4.051 ms |
| lambert_wn | 1, 0.7 | 35.4 µs | 40.8 µs † | 39.1 µs | 35.9 µs | 42.2 µs | 40.1 µs |
| lauricella_f | 0.5, 0.3, 2.5 | 30.1 µs † | 24.4 µs | 23.9 µs | 31.6 µs † | 23.6 µs | 25.8 µs |
| ldexp | 1.23456789, 3 | 139.7 ns | 163.3 ns | 181.4 ns | 159.5 ns | 149.6 ns | 199.0 ns |
| legendre_chi | 2, 0.7 | 84.1 µs † | 226.7 µs | 451.4 µs | 620.9 µs | 3.121 ms | 14.439 ms |
| lerch_phi | 0.3, 2, 1.4 | 365.0 µs | 338.6 µs | 401.8 µs | 348.2 µs | 331.9 µs | 360.4 µs |
| lg | 2.345678 | 15.1 µs | 19.5 µs | 37.1 µs † | 42.0 µs | 88.8 µs | 307.6 µs |
| lgamma | 2.345 | 60.9 µs † | 108.0 µs | 207.6 µs | 351.8 µs † | 1.209 ms | 6.306 ms † |
| ln | 2.345678 | 6.0 µs | 11.6 µs † | 13.6 µs | 23.4 µs | 49.9 µs | 145.9 µs † |
| log | 2.345678 | 7.5 µs † | 10.4 µs | 16.0 µs | 22.1 µs † | 56.3 µs | 151.0 µs |
| log10 | 2.345678 | 13.5 µs | 21.5 µs | 30.9 µs | 55.7 µs † | 89.9 µs | 333.4 µs |
| logbeta | 2.3, 4.5 | 170.9 µs † | 366.5 µs † | 703.5 µs † | 1.287 ms † | 3.921 ms | 19.787 ms † |
| logbeta_pdf | 0.5, 2.5, 3.5 | 107.8 µs | 274.6 µs | 439.3 µs | 799.1 µs | 2.587 ms | 13.236 ms † |
| lommel_s | 0.5, 0.7, 1.2 | 49.9 µs † | 94.6 µs | 171.6 µs † | 720.0 µs † | 906.9 µs | 4.267 ms † |
| mul | 0.7, 0.7 | 149.1 ns | 248.3 ns | 348.8 ns | 509.4 ns | 1.3 µs | 3.9 µs |
| mul_long | 17, 3 | 906.6 ns | 1.1 µs | 1.2 µs | 900.1 ns | 1.1 µs | 1.1 µs |
| mul_pow10 | 1.23456789, 3 | 427.8 ns | 496.0 ns | 839.1 ns | 791.5 ns | 649.8 ns | 904.6 ns |
| neg | 0.7 | 104.4 ns | 144.9 ns | 111.3 ns | 130.7 ns | 110.9 ns | 139.9 ns |
| normal_cdf | 0.7 | 17.5 µs | 103.6 µs | 73.7 µs | 125.1 µs | 561.3 µs | 2.624 ms † |
| normal_logpdf | 0.7 | 5.9 µs | 11.6 µs | 16.8 µs | 21.8 µs | 57.7 µs | 163.8 µs |
| normal_pdf | 0.7 | 4.9 µs | 7.9 µs | 14.7 µs | 23.2 µs | 71.2 µs | 220.2 µs |
| polygamma | 3, 2.345 | 54.2 µs | 153.1 µs | 314.3 µs | 441.7 µs | 2.246 ms | 12.257 ms |
| polylog | 2, 0.7 | 120.9 µs | 374.0 µs | 692.4 µs | 1.040 ms | 4.452 ms | 25.598 ms |
| polylog1 | 0.7 | 7.2 µs | 12.8 µs † | 17.1 µs | 24.9 µs | 53.3 µs † | 136.0 µs |
| pow | 1.23456789, 3.5 | 12.5 µs † | 18.0 µs | 34.2 µs † | 49.6 µs † | 132.2 µs † | 333.8 µs |
| pow10 | 3 | 331.5 ns | 461.3 ns | 507.0 ns | 739.7 ns | 526.8 ns | 521.8 ns |
| pow_int | 1.23456789, 3 | 462.5 ns | 590.5 ns | 741.7 ns | 904.5 ns | 2.9 µs | 9.3 µs |
| productlog | 0.7 | 66.6 µs | 152.7 µs † | 297.5 µs † | 292.9 µs | 1.073 ms | 3.355 ms |
| qdigamma | 0.5, 2.3 | 30.1 µs | 39.1 µs | 40.8 µs † | 31.8 µs | 32.6 µs | 34.2 µs |
| real_part | 0.7 | 121.7 ns | 101.0 ns | 118.9 ns | 127.3 ns | 101.8 ns | 118.7 ns |
| rect | 0.7 | 1.2 µs | 1.6 µs † | 1.2 µs | 1.2 µs | 1.3 µs | 1.6 µs † |
| root | 1.2345, 3 | 17.0 µs | 27.3 µs | 34.0 µs | 45.2 µs | 120.9 µs | 378.7 µs † |
| round | 0.7 | 3.0 µs | 3.3 µs | 3.6 µs | 3.7 µs | 3.4 µs | 3.7 µs † |
| roundn | 1.23456789, 3 | 6.3 µs | 7.1 µs | 7.8 µs | 7.7 µs | 9.4 µs | 12.2 µs |
| rounds | 1.23456789, 3 | 27.5 µs † | 34.8 µs | 46.4 µs | 66.8 µs † | 135.2 µs | 350.3 µs † |
| sec | 0.7 | 3.6 µs † | 5.9 µs | 10.6 µs | 13.9 µs | 64.6 µs † | 191.9 µs |
| sech | 0.7 | 4.2 µs † | 8.2 µs | 16.4 µs † | 20.1 µs | 80.3 µs † | 243.3 µs |
| sgn | 0.7 | 267.1 ns | 294.1 ns | 274.6 ns | 341.7 ns | 282.1 ns | 292.2 ns |
| sin | 0.567 | 3.9 µs | 7.4 µs | 12.3 µs | 20.2 µs | 54.5 µs | 169.6 µs |
| sinc | 0.7 | 6.1 µs | 11.0 µs | 13.3 µs | 19.3 µs | 59.6 µs | 217.2 µs |
| sincos | 0.7 | 11.5 µs | 15.5 µs | 23.4 µs | 27.1 µs | 66.3 µs | 191.9 µs |
| sinh | 0.7 | 3.7 µs | 8.2 µs | 12.5 µs | 18.9 µs | 62.7 µs | 245.4 µs † |
| sinhcosh | 0.7 | 3.8 µs | 7.2 µs | 13.9 µs | 17.6 µs | 59.6 µs | 195.8 µs |
| sqr | 0.7 | 189.1 ns | 285.2 ns | 410.2 ns | 765.1 ns | 677.9 ns | 1.8 µs |
| sqrt | 1.23456789 | 413.7 ns | 546.1 ns | 771.4 ns | 1.1 µs | 1.6 µs | 4.3 µs † |
| step | 0.7 | 351.9 ns | 353.7 ns | 300.3 ns | 351.5 ns | 299.6 ns | 275.9 ns |
| struve_h | 2, 0.7 | 186.9 µs | 454.5 µs † | 676.8 µs | 1.146 ms | 3.853 ms | 16.412 ms |
| struve_l | 2, 0.7 | 207.1 µs | 484.5 µs | 719.5 µs | 1.060 ms | 3.869 ms † | 14.652 ms |
| sub | 0.7, 0.7 | 117.1 ns | 105.4 ns | 111.9 ns | 120.7 ns | 202.0 ns | 349.6 ns |
| tan | 0.7 | 16.8 µs | 20.0 µs † | 26.1 µs | 29.6 µs | 72.5 µs | 220.5 µs |
| tanh | 0.7 | 4.6 µs † | 7.7 µs | 13.4 µs | 17.2 µs | 51.5 µs | 243.5 µs † |
| tetragamma | 2.345 | 56.2 µs | 177.2 µs | 318.9 µs | 457.9 µs | 2.330 ms | 14.178 ms |
| tri | 0.7 | 1.3 µs | 1.1 µs | 1.1 µs | 1.5 µs † | 1.4 µs | 1.3 µs |
| trigamma | 2.345 | 37.5 µs | 99.6 µs | 232.9 µs | 326.1 µs | 1.505 ms | 8.480 ms |
| vercos | 0.7 | 4.1 µs † | 5.9 µs | 9.8 µs | 16.7 µs | 55.9 µs | 164.4 µs |
| versin | 0.7 | 3.3 µs | 6.6 µs | 10.8 µs | 16.8 µs | 43.0 µs | 197.9 µs |
| zatahp | 2.3, 1.4 | 798.6 µs | 1.708 ms † | 3.223 ms | 4.665 ms | 26.063 ms | 143.500 ms |
| zeta | 2.345 | 1.215 ms | 5.124 ms | 13.201 ms | 1.981 ms † | 5.687 ms | 30.787 ms |
| zetah | 2.3, 1.4 | 763.7 µs | 1.416 ms | 2.940 ms | 4.549 ms | 25.189 ms | 136.666 ms |
| zetap | 2.345 | 338.4 µs † | 529.5 µs | 1.239 ms † | 1.805 ms | 6.944 ms | 55.089 ms † |

### number / multi-precision complex floating point

| Operation | Inputs | 256 bits | 512 bits | 768 bits | 1024 bits | 2048 bits | 4096 bits |
|---|---|---:|---:|---:|---:|---:|---:|
| E1 | 2.345678+0.125i | 104.1 µs | 246.7 µs | 438.8 µs | 658.7 µs | 2.377 ms | 8.771 ms |
| Ei | 2.345678+0.125i | 106.7 µs | 235.3 µs | 447.1 µs | 633.0 µs | 2.084 ms | 10.078 ms |
| Li | 2.345+0.125i | 106.4 µs | 232.9 µs | 397.8 µs | 765.6 µs | 2.687 ms | 11.376 ms |
| abs | 0.7+0.125i | 1.2 µs | 1.3 µs | 2.0 µs | 2.0 µs | 3.7 µs | 9.0 µs |
| acos | 0.7+0.125i | 67.2 µs | 116.7 µs | 136.6 µs | 89.6 µs | 253.8 µs | 640.3 µs † |
| acosec | 2.345+0.125i | 277.8 µs | 800.5 µs | 1.591 ms | 2.719 ms | 11.422 ms | 61.287 ms |
| acosech | 0.7+0.125i | 67.9 µs | 89.4 µs | 131.4 µs | 200.5 µs † | 396.5 µs | 1.168 ms |
| acosh | 2.345+0.125i | 50.6 µs | 90.7 µs | 127.2 µs | 87.0 µs | 393.2 µs | 533.0 µs |
| acot | 0.7+0.125i | 60.7 µs | 97.8 µs | 118.9 µs | 157.5 µs | 389.4 µs | 1.243 ms |
| acoth | 2.345+0.125i | 55.0 µs † | 81.4 µs | 90.4 µs | 135.5 µs | 338.6 µs | 937.9 µs |
| add | 0.7+0.125i, 0.7+0.125i | 464.4 ns | 453.9 ns | 556.7 ns | 587.2 ns | 540.4 ns | 834.2 ns |
| add_long | 17+0.125i, 3 | 3.9 µs † | 3.6 µs | 3.7 µs | 3.8 µs † | 3.9 µs | 4.1 µs † |
| appell_f1 | <span style="display:inline-block;width:40ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">0.5+0.125i, 0.7+0.125i, 0.8+0.125i, 2.5+<br/>0.125i, 0.2+0.125i, 0.3+0.125i</span> | 585.3 µs | 1.616 ms | 3.451 ms | 6.809 ms | 26.972 ms | 145.105 ms |
| arccovercos | 0.5+0.125i | 72.2 µs | 96.4 µs | 76.5 µs | 189.9 µs | 242.5 µs | 536.3 µs |
| arccoversin | 0.5+0.125i | 66.9 µs | 56.3 µs | 76.7 µs † | 187.3 µs | 210.7 µs | 585.5 µs |
| archacovercos | 0.5+0.125i | 18.1 µs | 21.2 µs | 28.7 µs | 28.1 µs | 57.4 µs | 160.7 µs |
| archacoversin | 0.5+0.125i | 13.8 µs | 19.1 µs | 26.1 µs | 33.0 µs † | 65.8 µs | 155.8 µs |
| archavercos | 0.5+0.125i | 14.4 µs | 23.7 µs | 27.6 µs | 31.2 µs † | 67.0 µs | 163.2 µs |
| archaversin | 0.5+0.125i | 17.2 µs | 21.1 µs | 25.9 µs | 31.6 µs | 59.2 µs | 154.2 µs |
| arcvercos | 0.5+0.125i | 66.1 µs | 72.3 µs † | 138.7 µs | 99.9 µs | 185.0 µs | 571.4 µs |
| arcversin | 0.5+0.125i | 37.5 µs | 61.5 µs | 138.8 µs | 108.7 µs | 193.7 µs | 512.1 µs |
| arg | 0.7+0.125i | 16.2 µs | 33.0 µs | 42.0 µs | 60.4 µs | 125.3 µs | 428.4 µs |
| asec | 2.345+0.125i | 262.6 µs | 743.2 µs | 1.539 ms | 2.562 ms | 14.080 ms | 60.247 ms |
| asech | 0.7+0.125i | 34.4 µs | 68.0 µs | 74.2 µs | 88.0 µs | 239.1 µs | 527.4 µs |
| asin | 0.7+0.125i | 64.3 µs | 139.2 µs † | 148.1 µs † | 186.6 µs | 459.1 µs | 661.9 µs |
| asinh | 0.7+0.125i | 66.9 µs | 96.3 µs | 122.9 µs | 175.7 µs | 473.0 µs | 1.228 ms |
| atan | 0.7+0.125i | 62.1 µs | 78.5 µs | 120.4 µs | 225.3 µs † | 388.8 µs | 1.170 ms |
| atan2 | 0.5+0.125i, -0.75+0.125i | 14.8 µs | 14.3 µs | 13.2 µs | 13.3 µs | 15.5 µs | 16.1 µs |
| atanh | 0.7+0.125i | 62.0 µs † | 86.3 µs | 120.4 µs | 168.1 µs | 403.2 µs | 920.4 µs |
| bessel_i | 2, 0.7+0.125i | 267.2 µs | 502.5 µs | 906.3 µs | 1.494 ms | 5.485 ms | 24.644 ms |
| bessel_j | 2, 0.7+0.125i | 262.1 µs | 566.0 µs † | 883.4 µs | 1.438 ms † | 4.684 ms | 18.778 ms |
| bessel_k | 2, 0.7+0.125i | 178.3 µs † | 336.9 µs † | 462.9 µs | 924.8 µs | 3.416 ms | 14.387 ms † |
| bessel_y | 2, 0.7+0.125i | 198.2 µs | 516.2 µs † | 911.1 µs | 1.271 ms | 4.647 ms | 23.270 ms |
| beta | 2.3+0.125i, 4.5+0.125i | 44.6 µs | 52.4 µs | 55.5 µs | 47.2 µs | 55.7 µs | 48.3 µs |
| beta_pdf | 0.5+0.125i, 2.5+0.125i, 3.5+0.125i | 63.1 µs | 82.6 µs | 71.0 µs | 63.5 µs | 67.0 µs | 72.4 µs |
| binomial | 5+0.125i, 2+0.125i | 36.8 µs | 34.9 µs | 33.4 µs | 37.8 µs | 40.1 µs | 37.8 µs |
| ceil | 0.7+0.125i | 1.9 µs | 2.3 µs | 2.0 µs | 1.7 µs | 3.1 µs † | 2.3 µs |
| chebyshev_t | 2, 0.7+0.125i | 12.5 µs | 13.7 µs | 16.6 µs | 15.2 µs † | 19.3 µs † | 22.7 µs |
| chebyshev_u | 2, 0.7+0.125i | 18.4 µs | 21.4 µs † | 22.1 µs † | 18.7 µs | 25.1 µs † | 28.9 µs |
| circ | 0.7 | 3.0 µs | 3.8 µs † | 3.4 µs | 3.9 µs | 5.3 µs | 11.1 µs |
| clausen | 3, 0.7+0.125i | 174.9 µs | 480.6 µs | 769.4 µs | 1.470 ms | 7.250 ms † | 32.291 ms |
| clausen2 | 0.7+0.125i | 201.4 µs † | 568.1 µs † | 940.8 µs † | 1.746 ms | 5.606 ms | 32.418 ms |
| conj | 0.7+0.125i | 401.4 ns | 351.2 ns | 322.8 ns | 410.7 ns | 353.2 ns | 382.7 ns |
| cos | 0.7+0.125i | 21.2 µs | 23.5 µs † | 30.3 µs | 39.3 µs | 131.2 µs | 531.8 µs † |
| cosec | 0.7+0.125i | 24.5 µs | 23.0 µs | 31.2 µs | 53.5 µs | 121.5 µs | 396.1 µs |
| cosech | 0.7+0.125i | 14.2 µs | 17.7 µs | 31.6 µs | 110.1 µs | 121.1 µs | 418.7 µs † |
| cosh | 0.7+0.125i | 11.3 µs | 16.5 µs | 27.9 µs | 104.3 µs | 103.7 µs | 351.2 µs |
| cot | 0.7+0.125i | 47.5 µs † | 38.5 µs | 64.4 µs | 91.4 µs | 331.0 µs † | 983.2 µs |
| coth | 0.7+0.125i | 27.1 µs | 40.5 µs | 49.8 µs | 184.3 µs | 209.8 µs | 690.4 µs |
| covercos | 0.7+0.125i | 23.4 µs | 29.7 µs † | 37.6 µs † | 51.1 µs | 146.4 µs | 431.7 µs |
| coversin | 0.7+0.125i | 26.5 µs | 24.1 µs | 29.5 µs | 45.3 µs | 201.9 µs † | 464.3 µs |
| cubrt | 0.7+0.125i | 50.4 µs | 97.6 µs | 160.7 µs † | 180.5 µs | 570.1 µs | 1.580 ms |
| digamma | 2.345+0.125i | 357.8 µs † | 634.0 µs | 1.138 ms | 1.687 ms | 7.332 ms † | 26.975 ms |
| dilog | 0.7+0.125i | 227.3 µs | 565.7 µs | 989.7 µs | 1.902 ms | 7.282 ms | 35.774 ms |
| div | 0.7+0.125i, 0.7+0.125i | 8.8 µs | 14.0 µs | 20.6 µs | 23.4 µs | 57.6 µs | 144.8 µs |
| erf | 0.7+0.125i | 109.3 µs | 291.4 µs | 445.6 µs | 697.4 µs | 2.807 ms | 11.092 ms |
| erfc | 0.7+0.125i | 106.2 µs | 260.5 µs † | 373.0 µs | 700.4 µs | 2.809 ms | 9.354 ms |
| erfcinv | 0.5+0.125i | 1.041 ms | 2.491 ms † | 2.595 ms | 5.860 ms | 14.565 ms | 66.532 ms |
| erfinv | 0.5+0.125i | 961.6 µs | 2.426 ms † | 2.917 ms | 4.153 ms | 18.271 ms † | 58.316 ms |
| exp | 1.23456789+0.125i | 14.3 µs | 29.0 µs | 40.4 µs | 60.5 µs | 188.6 µs | 691.4 µs |
| floor | 0.7+0.125i | 399.2 ns | 361.6 ns | 334.2 ns | 384.4 ns | 311.7 ns | 526.8 ns |
| gamma | 2.345+0.125i | 20.3 µs † | 21.4 µs † | 18.7 µs | 21.0 µs | 18.4 µs | 20.3 µs |
| gammainc_P | 2.3+0.125i, 1.4+0.125i | 84.8 µs | 96.9 µs | 75.7 µs | 102.2 µs | 78.7 µs | 75.2 µs |
| gammainc_Q | 2.3+0.125i, 1.4+0.125i | 77.5 µs | 86.3 µs | 88.5 µs | 95.2 µs † | 85.6 µs | 73.0 µs |
| gammainc_lower | 2.3+0.125i, 1.4+0.125i | 66.3 µs | 70.1 µs | 66.2 µs | 67.6 µs | 70.8 µs | 65.6 µs |
| gammainc_upper | 2.3+0.125i, 1.4+0.125i | 84.9 µs | 82.9 µs | 95.1 µs | 97.4 µs † | 90.1 µs | 85.0 µs |
| gammainv | 9.5+0.125i | 211.8 µs † | 199.4 µs | 163.6 µs | 199.9 µs | 206.8 µs | 166.3 µs |
| hacovercos | 0.7+0.125i | 35.4 µs † | 31.0 µs | 43.3 µs † | 53.1 µs | 129.8 µs | 494.6 µs |
| hacoversin | 0.7+0.125i | 36.0 µs † | 34.0 µs † | 39.6 µs | 45.6 µs | 131.0 µs | 410.3 µs |
| harmonic_poly | 2, 0.7+0.125i | 24.1 µs † | 26.1 µs | 23.7 µs | 25.7 µs | 42.4 µs | 83.5 µs |
| havercos | 0.7+0.125i | 33.9 µs | 79.2 µs † | 39.7 µs | 56.8 µs | 149.6 µs | 554.6 µs † |
| haversin | 0.7+0.125i | 28.6 µs | 33.2 µs † | 37.6 µs | 62.8 µs | 133.1 µs | 494.3 µs |
| hermite_h | 2, 0.7+0.125i | 18.8 µs † | 19.5 µs † | 21.1 µs † | 16.1 µs | 23.3 µs | 27.0 µs |
| hypergeometric_pFq | 0.7+0.125i, 2.5+0.125i, 0.3+0.125i | 45.3 µs | 52.6 µs | 44.7 µs | 47.2 µs | 40.3 µs | 42.6 µs |
| hypot | 0.7+0.125i, 1.2+0.125i | 3.3 µs | 3.0 µs | 2.9 µs | 2.5 µs | 3.2 µs | 2.5 µs |
| imag_part | 0.7+0.125i | 166.3 ns | 182.0 ns | 190.6 ns | 163.2 ns | 188.2 ns | 268.4 ns |
| inv | 0.7+0.125i | 1.8 µs | 1.7 µs | 3.0 µs † | 2.7 µs | 6.6 µs | 14.7 µs |
| lambert_w0 | 0.7+0.125i | 72.7 µs | 89.6 µs | 103.8 µs | 152.5 µs | 393.7 µs | 1.201 ms |
| lambert_wm1 | -0.2+0.125i | 60.3 µs | 75.9 µs | 96.3 µs | 167.4 µs | 401.2 µs | 1.030 ms |
| lambert_wn | 1, 0.7+0.125i | 44.0 µs | 47.9 µs | 43.2 µs | 37.0 µs | 43.7 µs | 39.5 µs |
| lauricella_f | 0.5+0.125i, 0.3+0.125i, 2.5+0.125i | 190.4 µs | 210.0 µs | 189.2 µs | 226.0 µs | 221.1 µs | 232.5 µs |
| ldexp | 1.23456789+0.125i, 3 | 462.9 ns | 460.3 ns | 379.3 ns | 506.0 ns | 657.4 ns | 422.9 ns |
| legendre_chi | 2, 0.7+0.125i | 317.0 µs | 884.3 µs | 1.732 ms | 3.247 ms | 13.524 ms | 74.096 ms |
| lerch_phi | 0.3+0.125i, 2+0.125i, 1.4+0.125i | 779.6 µs | 800.5 µs | 700.4 µs | 725.8 µs | 792.3 µs | 754.2 µs |
| lg | 2.345678+0.125i | 34.0 µs | 58.7 µs | 71.6 µs | 93.4 µs | 248.2 µs | 621.5 µs |
| lgamma | 2.345+0.125i | 17.6 µs | 17.2 µs | 16.4 µs | 16.8 µs | 19.6 µs † | 18.1 µs |
| ln | 2.345678+0.125i | 23.5 µs | 35.1 µs | 43.7 µs | 67.4 µs | 168.0 µs | 585.3 µs |
| log | 2.345678+0.125i | 20.5 µs | 39.7 µs | 44.9 µs | 81.4 µs | 190.6 µs | 625.1 µs |
| log10 | 2.345678+0.125i | 33.0 µs | 58.4 µs | 63.8 µs | 99.5 µs | 223.7 µs | 738.8 µs |
| logbeta | 2.3+0.125i, 4.5+0.125i | 42.7 µs | 46.7 µs | 52.1 µs † | 47.6 µs | 46.1 µs | 50.3 µs |
| logbeta_pdf | 0.5+0.125i, 2.5+0.125i, 3.5+0.125i | 56.3 µs | 68.3 µs | 76.5 µs † | 58.7 µs | 56.9 µs | 62.5 µs |
| lommel_s | 0.5+0.125i, 0.7+0.125i, 1.2+0.125i | 294.1 µs | 627.7 µs | 1.010 ms | 1.773 ms | 6.353 ms | 22.144 ms |
| mul | 0.7+0.125i, 0.7+0.125i | 2.1 µs | 2.6 µs † | 3.0 µs | 3.5 µs | 4.7 µs | 8.2 µs |
| mul_long | 17+0.125i, 3 | 4.0 µs | 4.8 µs | 5.3 µs | 4.4 µs | 4.4 µs | 4.4 µs |
| mul_pow10 | 1.23456789+0.125i, 3 | 4.5 µs | 3.6 µs | 4.0 µs | 3.6 µs | 3.3 µs | 4.5 µs |
| neg | 0.7+0.125i | 359.0 ns | 446.3 ns | 361.4 ns | 422.5 ns | 401.8 ns | 429.7 ns |
| normal_cdf | 0.7+0.125i | 105.3 µs | 218.3 µs | 424.0 µs | 671.2 µs | 2.910 ms | 10.100 ms |
| normal_logpdf | 0.7+0.125i | 9.9 µs | 17.1 µs | 21.2 µs | 23.9 µs | 47.5 µs | 152.7 µs |
| normal_pdf | 0.7+0.125i | 22.3 µs | 41.7 µs | 79.0 µs † | 112.1 µs | 352.1 µs † | 1.119 ms |
| polygamma | 3, 2.345+0.125i | 1.024 ms † | 943.9 µs | 1.653 ms | 2.006 ms | 7.211 ms | 36.548 ms |
| polylog | 2, 0.7+0.125i | 214.3 µs | 561.3 µs | 1.071 ms | 2.079 ms | 8.759 ms | 34.933 ms |
| polylog1 | 0.7+0.125i | 31.7 µs | 46.7 µs | 70.5 µs | 101.5 µs | 177.8 µs | 632.5 µs |
| pow | 1.23456789+0.125i, 3.5+0.125i | 48.3 µs | 82.5 µs | 144.2 µs | 201.6 µs † | 502.1 µs | 1.551 ms |
| pow10 | 3 | 13.4 µs | 22.6 µs | 34.3 µs | 46.8 µs | 92.2 µs | 231.9 µs |
| pow_int | 1.23456789+0.125i, 3 | 9.8 µs | 10.6 µs | 10.5 µs | 11.3 µs | 20.1 µs | 30.0 µs |
| productlog | 0.7+0.125i | 84.1 µs † | 80.8 µs | 132.3 µs † | 167.8 µs | 427.4 µs | 1.331 ms |
| qdigamma | 0.5+0.125i, 2.3+0.125i | 224.6 µs | 220.9 µs | 241.2 µs | 190.7 µs | 198.2 µs | 223.8 µs |
| real_part | 0.7+0.125i | 162.0 ns | 187.9 ns | 187.2 ns | 162.6 ns | 183.5 ns | 246.9 ns |
| rect | 0.7 | 2.9 µs | 2.9 µs | 3.2 µs | 3.8 µs | 4.9 µs | 10.7 µs |
| root | 1.2345+0.125i, 3 | 68.0 µs † | 76.1 µs | 253.4 µs | 371.4 µs † | 436.7 µs | 1.524 ms |
| round | 0.7+0.125i | 16.3 µs † | 11.7 µs | 10.7 µs | 13.2 µs | 13.9 µs | 12.5 µs |
| roundn | 1.23456789+0.125i, 3 | 26.3 µs † | 20.7 µs | 21.4 µs | 21.9 µs | 27.0 µs | 42.8 µs |
| rounds | 1.23456789+0.125i, 3 | 56.2 µs | 73.2 µs | 101.5 µs | 117.5 µs | 304.7 µs † | 671.8 µs |
| sec | 0.7+0.125i | 23.7 µs | 23.9 µs | 35.5 µs | 46.9 µs | 151.0 µs | 438.3 µs |
| sech | 0.7+0.125i | 13.9 µs | 22.8 µs | 31.8 µs | 113.4 µs | 118.1 µs | 399.2 µs |
| sgn | 0.7 | 1.0 µs | 1.2 µs | 1.4 µs † | 1.1 µs | 1.2 µs | 1.0 µs |
| sin | 0.567+0.125i | 19.7 µs | 25.8 µs † | 29.8 µs | 43.1 µs | 121.3 µs | 458.6 µs |
| sinc | 0.7+0.125i | 31.2 µs † | 34.8 µs | 45.0 µs | 64.2 µs | 155.0 µs | 567.3 µs |
| sinh | 0.7+0.125i | 12.1 µs † | 19.9 µs | 27.5 µs | 120.1 µs | 108.7 µs † | 445.4 µs |
| sqr | 0.7+0.125i | 1.9 µs | 2.7 µs | 2.7 µs | 4.0 µs | 5.2 µs | 9.3 µs |
| sqrt | 1.23456789+0.125i | 3.0 µs | 3.5 µs | 4.2 µs | 5.4 µs | 8.1 µs | 19.7 µs |
| step | 0.7 | 1.1 µs | 976.2 ns | 973.5 ns | 925.1 ns | 1.0 µs | 1.1 µs |
| struve_h | 2, 0.7+0.125i | 265.5 µs | 684.9 µs | 1.039 ms | 1.856 ms | 5.964 ms | 25.378 ms |
| struve_l | 2, 0.7+0.125i | 320.3 µs | 695.0 µs | 1.306 ms † | 1.918 ms † | 6.529 ms | 24.869 ms |
| sub | 0.7+0.125i, 0.7+0.125i | 537.9 ns | 546.0 ns | 453.6 ns | 565.9 ns | 649.6 ns | 650.0 ns |
| tan | 0.7+0.125i | 15.8 µs | 28.7 µs | 39.9 µs | 52.4 µs | 141.8 µs | 512.7 µs |
| tanh | 0.7+0.125i | 29.3 µs | 20.6 µs | 75.4 µs | 45.0 µs | 111.4 µs | 386.5 µs |
| tetragamma | 2.345+0.125i | 399.8 µs | 941.5 µs | 1.613 ms | 3.193 ms † | 10.999 ms † | 40.998 ms |
| tri | 0.7 | 3.2 µs | 3.5 µs | 3.7 µs | 3.6 µs | 5.4 µs | 9.0 µs |
| trigamma | 2.345+0.125i | 389.5 µs † | 652.4 µs | 1.188 ms | 1.866 ms | 6.197 ms | 33.256 ms |
| vercos | 0.7+0.125i | 25.7 µs | 24.2 µs | 30.1 µs | 47.2 µs | 136.5 µs | 433.9 µs |
| versin | 0.7+0.125i | 21.0 µs | 22.1 µs | 31.2 µs | 48.0 µs | 142.3 µs | 469.1 µs |
| zatahp | 2.3+0.125i, 1.4+0.125i | 2.703 ms | 5.927 ms | 11.463 ms | 18.037 ms | 115.908 ms | 633.655 ms |
| zeta | 2.345+0.125i | 889.6 µs † | 1.647 ms | 3.118 ms | 5.649 ms | 28.006 ms | 123.420 ms |
| zetah | 2.3+0.125i, 1.4+0.125i | 2.891 ms | 5.315 ms | 9.635 ms | 19.537 ms | 104.191 ms | 557.533 ms |
| zetap | 2.345+0.125i | 822.6 µs | 1.925 ms | 3.664 ms | 5.910 ms | 32.710 ms | 162.008 ms |

### number / unrestricted integer

| Operation | Inputs | Exact |
|---|---|---:|
| abs | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">-9712853409152031141207811585727780469484705121206232579607434236969009533618825<br/>97201918971499157056640605432269691427807258223374967278846596194834524005116336<br/>17514882293623272276764857474830401626415841035971458657442133370777607803464071<br/>101158385124186229995073045427448918227959893082282919863982307657776</span> | 122.4 ns |
| add | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 109.1 ns |
| bit_and | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 315.9 ns |
| bit_length | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 187.9 ns |
| bit_not | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 322.6 ns |
| bit_or | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 294.8 ns |
| bit_xor | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 278.7 ns |
| clear_bit | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>701</span> | 358.5 ns |
| div | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>340282366920938463463374607431768211454</span> | 1.7 µs |
| divmod | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 375.3 ns |
| factorial | 5000 | 127.7 µs |
| factors | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">54772119019154636339867862041163448790974484131459861520687327809209914339836012<br/>52275954799541486775711126005967382600118182797233708133775335788335658037065081<br/>31569062299976033041302189833215742992653063887339218309467131548996199881113600<br/>00000000000000000000000000000000000000000000000000000000000000000000000000000000<br/>0000000000000000000000000000000000000000000000</span> | 103.6 µs |
| fibonacci | 10000 | 6.2 µs |
| gcd | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 3.8 µs |
| gcdext | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 4.3 µs |
| is_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 559.5 µs |
| isqrt | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 439.5 ns |
| lcm | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 3.3 µs |
| mod | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 340.3 ns |
| modinv | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 4.8 µs |
| mul | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 288.1 ns |
| neg | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 82.1 ns |
| next_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 4.507 ms |
| partition | 5000 | 7.163 ms |
| pow | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>5</span> | 4.5 µs |
| pow_int | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>5</span> | 3.5 µs |
| powmod | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662, <br/>68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 120.6 µs |
| prev_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 6.950 ms |
| prove_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 3.704 ms |
| set_bit | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>700</span> | 458.4 ns |
| shl | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>257</span> | 625.6 ns |
| shr | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>257</span> | 400.3 ns |
| sqr | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 299.9 ns |
| sub | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 128.4 ns |
| test_bit | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>701</span> | 509.5 ns |

### number / unrestricted fraction

`number_t` uses exact GMP rational arithmetic (MPQ). The operands below have large integer
numerators and denominators; parsing and canonical reduction precede timing. Every result
is checked to remain an exact fraction. Arithmetic cost depends on operand sizes and
cancellation, rather than a floating-point precision. These workloads use a 16,384-bit
rational size budget so intermediate results remain exact; general transcendental
functions usually require inexact real or complex results.

| Operation | Inputs | Exact |
|---|---|---:|
| abs | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">-9712853409152031141207811585727780469484705121206232579607434236969009533618825<br/>97201918971499157056640605432269691427807258223374967278846596194834524005116336<br/>17514882293623272276764857474830401626415841035971458657442133370777607803464071<br/>101158385124186229995073045427448918227959893082282919863982307657776/6864797660<br/>13060971498190079908139321726943530014330540939446345918554318339765605212255964<br/>0661454554977296311391480858037121987999716643812574028291115057151</span> | 242.0 ns |
| add | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776/68647976601<br/>30609714981900799081393217269435300143305409394463459185543183397656052122559640<br/>661454554977296311391480858037121987999716643812574028291115057151, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662/9712<br/>85340915203114120781158572778046948470512120623257960743423696900953361882597201<br/>91897149915705664060543226969142780725822337496727884659619483452400511633617514<br/>88229362327227676485747483040162641584103597145865744213337077760780346407110115<br/>8385124186229995073045427448918227959893082282919863982307657776</span> | 4.8 µs |
| div | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776/68647976601<br/>30609714981900799081393217269435300143305409394463459185543183397656052122559640<br/>661454554977296311391480858037121987999716643812574028291115057151, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662/9712<br/>85340915203114120781158572778046948470512120623257960743423696900953361882597201<br/>91897149915705664060543226969142780725822337496727884659619483452400511633617514<br/>88229362327227676485747483040162641584103597145865744213337077760780346407110115<br/>8385124186229995073045427448918227959893082282919863982307657776</span> | 7.3 µs |
| inv | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776/68647976601<br/>30609714981900799081393217269435300143305409394463459185543183397656052122559640<br/>661454554977296311391480858037121987999716643812574028291115057151</span> | 166.3 ns |
| mul | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776/68647976601<br/>30609714981900799081393217269435300143305409394463459185543183397656052122559640<br/>661454554977296311391480858037121987999716643812574028291115057151, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662/9712<br/>85340915203114120781158572778046948470512120623257960743423696900953361882597201<br/>91897149915705664060543226969142780725822337496727884659619483452400511633617514<br/>88229362327227676485747483040162641584103597145865744213337077760780346407110115<br/>8385124186229995073045427448918227959893082282919863982307657776</span> | 3.2 µs |
| neg | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776/68647976601<br/>30609714981900799081393217269435300143305409394463459185543183397656052122559640<br/>661454554977296311391480858037121987999716643812574028291115057151</span> | 189.6 ns |
| pow | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776/68647976601<br/>30609714981900799081393217269435300143305409394463459185543183397656052122559640<br/>661454554977296311391480858037121987999716643812574028291115057151, <br/>5</span> | 34.2 µs |
| pow_int | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776/68647976601<br/>30609714981900799081393217269435300143305409394463459185543183397656052122559640<br/>661454554977296311391480858037121987999716643812574028291115057151, <br/>5</span> | 37.0 µs |
| sqr | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776/68647976601<br/>30609714981900799081393217269435300143305409394463459185543183397656052122559640<br/>661454554977296311391480858037121987999716643812574028291115057151</span> | 539.0 ns |
| sub | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776/68647976601<br/>30609714981900799081393217269435300143305409394463459185543183397656052122559640<br/>661454554977296311391480858037121987999716643812574028291115057151, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662/9712<br/>85340915203114120781158572778046948470512120623257960743423696900953361882597201<br/>91897149915705664060543226969142780725822337496727884659619483452400511633617514<br/>88229362327227676485747483040162641584103597145865744213337077760780346407110115<br/>8385124186229995073045427448918227959893082282919863982307657776</span> | 5.0 µs |

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
