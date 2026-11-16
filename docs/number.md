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
  `number_polygamma_coefficients.c` selects bounded coefficient precision for
  the asymptotic digamma and polygamma corrections.
  `number_zeta_real.c` and `number_lerch_real.c` keep real sums and their guarded
  convergence checks in MPFR arithmetic.
  `number_zeta_blocks.c` supplies finite shifted-sum blocks shared by real and
  complex Hurwitz zeta and its order derivative.
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
| `num_zeta_pair(s, &derivative)` | Returns `ζ(s)` and writes its independently owned derivative, sharing multiprecision work. |
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
The computation is bounded to 200 000 series terms; exhaustion or an
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
The complex series excludes non-positive integer orders, limits `abs(x)` to 8 000,
uses at most 100 000 terms and limits guarded work to 65536 bits. Unsupported
inputs or unresolved cancellation return an invalid result. The fixed-precision
qfloat backend retains its existing implementation.

#### Precision-preserving series and inverse functions

Multiprecision `num_hypergeometric_pFq` retains upper parameters, lower parameters
and the argument at the widest participating precision. Its defining series supports
`p <= q`, `p = q + 1` with `abs(z) < 1`, and terminating polynomials. It rejects
denominator poles, unsupported continuation and exhausted convergence limits.
The native series uses at most 200 000 terms and 65536 guarded working bits.
Exact parameters and arguments contribute the current default working precision.

`number_lerch_phi` and `num_lerch_phi` use native arithmetic for real and complex
arguments in `abs(z) < 1`, with the existing zero-argument, zero-order and Hurwitz
zeta reductions. Integer orders use integer powers. Negative integer orders
`s = -m`, with `1 <= m <= 64`, `abs(z) <= 15/16` and `abs(a) <= 16`, first
attempt the finite rational form `P_m(z)/(1-z)^(m+1)`. Applying `a + z*d/dz`
to the [defining series](https://dlmf.nist.gov/25.14.E1) gives a descending
polynomial coefficient recurrence, without numerical differentiation. Short dyadic
components use exact Gaussian-integer coefficients and rational division; wider
components use guarded MPC arithmetic. Input conversion is repeated at the required
guarded precision, including exact rational operands. A bound covering coefficient
growth, the denominator and cancellation checks the final relative accuracy.
Unsupported inputs and unresolved cancellation retain the checked defining-series
fallback. Integer-power fallback terms also avoid reciprocal operations that cancel
each other. The caller prepares ordinary-series powers only if a specialised attempt fails.

At 512–8192 bits, moderate
non-integral orders and offsets (both of modulus at most 16) and
`0.01 <= abs(z) <= 0.9` use finite weighted binomial blocks. Each block sums 64
consecutive terms around one inverse-power anchor, using the
[binomial expansion](https://dlmf.nist.gov/4.6.E7) and shared geometric moments.
The first 64 terms retain their principal powers; later blocks stay in the right
half-plane. Binomial-weighted moments are generated once per call and reused
by Horner evaluation at each block centre, then released afterwards. Explicit bounds
cover the binomial remainder, defining-series tail and accumulated absolute
roundoff, including shortened moment arithmetic. Unresolved cases fall back to
the ordinary series. One checked result, keyed by exact arguments, signed zeros
and verified precision, retains at most 16 KiB of significands under a mutex;
this avoids repeating unchanged calculations. Cold and changing-input timings
must be measured separately from that reuse.

Geometric weights at equal distances from the centre share even and odd moment
recurrences, halving their updates. Guarded coefficient products share the
native series multiplication helper. The common one-half bound on block radii
also permits progressively shorter moment and binomial-coefficient arithmetic.
Stored coefficients retain at least the precision required by every later
Horner block; accumulation and the final relative error check keep their
guarded precision.
Geometric weights use GMP fixed-point integers, with 96 fractional guard bits
beyond the current moment precision. Their updates contract by a squared distance
below one; the bounded truncation error fits inside the existing absolute error
allowance. Integer sums preserve cancellation until conversion to a floating-point
moment, and weights that reach zero need no later updates. This applies to both
real and complex weights; it does not depend on a previous result being cached.
Block precision accounts for the inverse-power anchor and its phase as well as
`z^k`, so negative orders retain the bits needed for growing powers. The real
defining-series path likewise includes that growth when planning coefficient precision.

Real inputs with positive offsets outside the binomial-block path use MPFR inverse powers and Horner
accumulation, with the same explicit roundoff checks. For non-negative orders,
the real defining-series term ratio is bounded directly by `abs(z)`; negative
orders retain a decreasing bound for the additional power factor. This avoids
complex arithmetic for real sums without weakening the stopping condition.

The ordinary series for general orders reuses shifted
inverse powers across calls: the additional power cache retains
one exact offset/order pair, at most 4 096 entries, 16384 bits per entry and eight
MiB of significands. Entries are copied under a mutex, higher precision can serve
lower requests with the same exact key, and changing either key releases the old
entries. This bounds memory use in a persistent server. First use still calculates
the required powers; subsequent calls can reuse them. Guarded logarithms are shared
when the order changes. As `z^k` decreases, non-integral powers use fewer significant
bits; a separate absolute error bound for every weighted power remains part of the
convergence check. At larger precisions, sixteen-term Horner blocks reduce full-width complex products,
with directed magnitude bounds controlling the same geometric remainder. Each
block also uses its required coefficient precision for Horner evaluation, including
rounded copies of the argument and power; their rounding errors remain in the bound. If
cancellation exhausts that allowance, the calculation retries
at uniform precision. Convergence and accumulated rounding checks apply to both
cached and uncached evaluations. The native benchmark
`build/release/bench/number/bench_number_lerch` measures separate fresh-process
calls and calls changing all three arguments, rather than timing repeated cache
hits as evidence of algorithmic speed.

Complex `num_gamma`, `num_lgamma` and `num_gammainv` retain MPC precision instead
of converting to qcomplex. Gamma uses recurrence and a guarded Stirling expansion;
log gamma tracks the logarithm's branch through that recurrence. Inverse gamma
refines the established logarithmic branch with increasing precision and checks
the final defining residual. Both real and complex inverse gamma plan their precision
stages backwards from the target, avoiding a redundant almost-full-precision stage.
Their Newton slopes use half-stage precision plus guard bits, while the final
correction and residual retain the full accuracy checks. Intermediate stages apply
a correction once half-stage accuracy is established. When a final iterate already passes both the correction and
logarithmic-residual checks, it is retained without a redundant final gamma evaluation.
Working storage is local apart from the bounded, synchronised coefficient cache.
At higher precision, rising factorials use short polynomials with exact integer
coefficients and reusable powers. An argument bound preserves the recurrence's
complete-turn count. The Stirling correction uses rectangular splitting and
shortens coefficient precision using an absolute Bernoulli bound; complex powers
and accumulators retain full working precision.
A rejected input or unresolved calculation
returns an invalid number rather than a lower-precision answer.

The shared Bernoulli cache builds even coefficient prefixes from the
[Bernoulli generating function](https://dlmf.nist.gov/24.2.E1), retaining 96 arithmetic
guard bits and bounding the omitted convolution tail. Normalised coefficients,
ordinary coefficients and the recurrence's inverse factorials share the existing
eight-MiB limit. Oversized requests keep the direct integer-zeta formula. Hurwitz
zeta and its order derivative share Lerch's guarded inverse powers for their finite
sums; their Euler–Maclaurin tails and branch conventions are unchanged.
Small convolution products and Euler–Maclaurin corrections use fewer significant
bits, with absolute roundoff bounds derived from the accumulator scale and the
normalised Bernoulli-coefficient bound. Rising products and final sums retain their
full working precision outside the checked zeta-tail path described below.

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
estimate improves, and verify the final forward residual. The precision schedule
is planned backwards from the final budget, avoiding an almost full-size
intermediate stage. When the current final iterate already passes both the
correction and forward-residual checks, it is returned directly.
Near the real erf endpoints, the solver uses the small complementary probability
as its residual; reflection likewise protects erfc inputs close to two. This
avoids losing that probability when an early, shorter stage rounds erf to one;
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

At 1024–8192 bits, moderate complex derivative parameters also allow shorter rising-product state.
The arithmetic check scales its absolute error by `32^(-k)`, since the normalised
Bernoulli coefficient has magnitude below `4/32^k`. Value and derivative errors
advance together through the quadratic recurrence; zeros require no relative-error
division. Factors and final sums retain their full precision. The shortened path
commits only after convergence and a successful propagated-error check; unsupported
parameters, large intermediate magnitudes and exhausted bounds retain the original
full-state recurrence. Real MPFR kernels and value-only calls retain their ordinary
full-state recurrence, avoiding the extra checking overhead on those paths.

For moderate Hurwitz parameters, the finite shifted sum groups thirty-two
consecutive bases around one inverse-power anchor. The
[binomial expansion](https://dlmf.nist.gov/4.6.E7) supplies coefficients shared
between block centres. Symmetric centres make all odd moments zero, halving the
Horner polynomial; even moments use sixteen positive scalar recurrences. Analytic coefficient derivatives
remain valid at zero and negative integral orders. A common positive-coefficient
majorant bounds both polynomial tails, while guarded arithmetic and explicit
absolute roundoff checks protect the completed sum. The Euler–Maclaurin tail
and branch conventions remain unchanged.
Coefficient and moment precision decrease with degree using a proved
three-eighths bound on block radii and a 320-bit allowance for coefficient growth.
The final sum retains its full guarded precision. Coefficient storage is local,
limited to 4 864 even-degree entries per requested output, and released on return.

The attempt applies to finite-sum precisions of 1024–8320 bits, exponent and
offset moduli at most sixteen, offset real part at least one quarter, and
64–4 096 terms. Other inputs and exhausted bounds retain the ordinary sum.
Public Hurwitz derivative kernels also attempt these blocks at integral orders,
including zero and negative orders: differentiating the rising-factorial
recurrence does not divide by a factor that can vanish. Value-only integral
orders retain the ordinary finite sum. The complex kernel uses direct powers for
value-only machine-word orders; its ordinary derivative fallback shares each
base logarithm between the term and its derivative.
One checked finite-sum entry, using at most sixteen KiB of retained significands,
is keyed by exact inputs, signed zeros, term count and verified precision.
It preserves warmed calls; cold and changing-input benchmarks still execute
the block calculation independently.
For finite positive non-integer real arguments up to 64, working precision of
1024–8192 bits selects an MPFR Euler–Maclaurin kernel. Real Hurwitz zeta at positive
offsets and real order derivatives also use MPFR finite sums and tail recurrences.
Their bounded shifted-power cache keeps its shared exact-key representation.
Integer arguments, other real arguments and other precisions
retain MPFR's direct zeta calculation. An unsuccessful native evaluation falls
back to MPFR without modifying the input. The shared kernel follows the
[Euler–Maclaurin representation](https://dlmf.nist.gov/25.2#iii); selection changes
the algorithm, not the precision or real result type.

`num_zeta_pair(s, &derivative)` shares the finite sum and Euler–Maclaurin
corrections between the value and its analytic derivative for MPFR and MPC
arguments. Other backends use their ordinary value and derivative kernels.
Both outputs belong to the caller and must be released independently. Passing
NULL for the derivative requests only the value; invalid arguments and exhausted
convergence guards retain the ordinary NaN behaviour.

The following README example requests both values at 256 bits:

```c
#include <stdio.h>

#include "number-t.h"

int main(void)
{
    size_t saved = num_get_default_prec_bits();
    num_set_default_prec_bits(256);
    number_t s = num_create_from_string("2.25"), derivative;
    number_t value = num_zeta_pair(s, &derivative);
    printf("zeta(2.25) = %.6f; derivative = %.6f\n",
           num_to_double(value), num_to_double(derivative));
    num_destroy(&value);
    num_destroy(&derivative);
    num_destroy(&s);
    num_set_default_prec_bits(saved);
    return 0;
}
```

```text
zeta(2.25) = 1.460212; derivative = -0.580229
```

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
For complex digamma and both real and complex polygamma orders up to 64,
Bernoulli coefficient precision decreases as the weighted terms shrink.
A directed factorial bound uses `abs(B_(2n)) < 4*(2n)!/32^n`, retaining at least
128 coefficient bits and keeping each shortened coefficient's weighted absolute
error below `2^(-work-32)`. Powers, terms and accumulators retain their full
guarded precision. Working budgets outside 640–65536 bits and higher orders keep
the original full coefficient precision. Real digamma continues to use MPFR.

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

The unrestricted integer cases use 512–1215-bit operands, including a 521-bit prime,
and cover ordinary arithmetic as well as integer-specific functions. The unrestricted
fraction cases time exact rational arithmetic on large numerators and denominators.
Both profiles parse their operands before timing and reject any inexact result.
Factorial, Fibonacci and partition indices remain ordinary integers because those
APIs accept counts; their exact results exceed native integer sizes.

### Cold and changing-input timings at 4096 bits

Measured on 15 November 2026 on the Intel N95, using seven samples per case.
The digamma, trigamma, tetragamma, polygamma, inverse erf/erfc, inverse gamma and
integer-order Hurwitz derivative rows use matched comparison runs: cold **133**
and changing-input **135**. The remaining rows use cold **138** and
changing-input **139**. All four runs use the same library archive as the
full-catalogue run below, with no CPU affinity restriction.
Cold samples fork from an unevaluated worker; changing samples have no warm-up
and increase eligible real parts by `(sample + 1)/128`. Parsing, setup, validation
and disposal are excluded. These are separate protocols from the warmed catalogue.

Complex operands receive `+0.125i` according to the stored mask. Both labelled
Lerch workloads call the public `num_lerch_phi` function, which selects its
algorithm from the parameters. The `lerch_phi` (integer order) workload keeps
order -15 real and changes only `z` and `a`; `lerch_phi` (negative order) permits
the order itself to change. Consequently, only the integer-order workload
consistently exercises the finite rational kernel.
The real `lerch_phi` cold case has integer order 2; changing its order selects the
non-integral block algorithm, so those two columns exercise different paths.
The `zeta_pair` workload requests both the zeta value and its order derivative.

The `zatahp` (integer order) workload keeps order -4 real and varies only the
offset. † marks a retained timing outlier, excluded from baseline statistics.

**Real inputs (MPFR)**

| Operation | Base operands | Cold | Changing inputs |
|---|---|---:|---:|
| `appell_F1` | `0.5, 0.7, 0.8, 2.5, 0.2, 0.3` | 33.188 ms | 36.075 ms |
| `digamma` | `2.345` | 67.141 ms | 21.426 ms |
| `erfcinv` | `0.5` | 8.601 ms | 6.131 ms |
| `erfinv` | `0.5` | 7.662 ms | 6.408 ms |
| `gamma` | `2.345` | 65.841 ms | 6.400 ms |
| `gammainv` | `9.5` | 72.075 ms | 24.476 ms |
| `lerch_phi` | `0.3, 2, 1.4` | 30.115 ms † | 108.044 ms |
| `lerch_phi` (integer order) | `0.3125, -15, 0.375` | 215.7 µs | 55.2 µs |
| `lerch_phi` (negative order) | `0.3125, -15, 0.375` | 325.7 µs † | 127.969 ms † |
| `lgamma` | `2.345` | 91.333 ms † | 7.960 ms † |
| `polygamma` | `3, 2.345` | 106.498 ms | 10.838 ms |
| `tetragamma` | `2.345` | 110.677 ms | 11.885 ms |
| `trigamma` | `2.345` | 106.536 ms | 7.737 ms |
| `zatahp` | `2.3, 1.4` | 298.127 ms † | 136.227 ms |
| `zatahp` (integer order) | `-4, 0.75` | 159.316 ms | 48.205 ms |
| `zeta_pair` | `2.345` | 261.514 ms † | 49.544 ms |
| `zetah` | `2.3, 1.4` | 199.035 ms | 79.820 ms |
| `zetap` | `2.345` | 217.933 ms | 46.511 ms † |

**Complex inputs (MPC)**

| Operation | Base operands | Cold | Changing inputs |
|---|---|---:|---:|
| `appell_F1` | `0.5, 0.7, 0.8, 2.5, 0.2, 0.3` | 165.150 ms | 166.946 ms |
| `digamma` | `2.345` | 123.098 ms | 21.479 ms |
| `erfcinv` | `0.5` | 26.673 ms | 26.476 ms |
| `erfinv` | `0.5` | 26.740 ms | 28.387 ms |
| `gamma` | `2.345` | 111.167 ms † | 11.445 ms |
| `gammainv` | `9.5` | 122.604 ms | 35.889 ms |
| `lerch_phi` | `0.3, 2, 1.4` | 333.877 ms | 331.119 ms |
| `lerch_phi` (integer order) | `0.3125, -15, 0.375` | 288.7 µs | 98.3 µs |
| `lerch_phi` (negative order) | `0.3125, -15, 0.375` | 354.181 ms | 351.249 ms |
| `lgamma` | `2.345` | 98.270 ms | 9.715 ms |
| `polygamma` | `3, 2.345` | 130.513 ms | 31.006 ms |
| `tetragamma` | `2.345` | 132.787 ms | 31.208 ms |
| `trigamma` | `2.345` | 125.635 ms | 27.181 ms |
| `zatahp` | `2.3, 1.4` | 445.181 ms | 355.436 ms |
| `zatahp` (integer order) | `-4, 0.75` | 295.369 ms | 185.889 ms |
| `zeta_pair` | `2.345` | 298.783 ms | 154.218 ms |
| `zetah` | `2.3, 1.4` | 362.444 ms | 254.142 ms |
| `zetap` | `2.345` | 326.049 ms | 142.053 ms |

### Full catalogue, warm protocol

Host scheduling affected this measurement session. Retained slowdown outliers are
marked below and excluded from baseline statistics; they are not accepted new
performance baselines. Use matched runs when attributing a timing change to an
implementation change.

<!-- MARS-BENCHMARKS:BEGIN -->
Benchmark run **140**, 2026-11-15T22:41:00.137Z (UTC).

Machine: Intel(R) N95.

System: Linux 7.0.0-27-generic x86_64.

Compiler: 15.2.0; build flags: gcc -Wall -Wextra -Werror -O2 -fPIC -D_GNU_SOURCE -pthread -DHAVE_UNISTRING -I/usr/include/sqlcipher -DSQLITE_HAS_CODEC   -I/usr/include/x86_64-linux-gnu -isystem /usr/include/mit-krb5 -isystem /usr/include/mit-krb5 -I/usr/include/p11-kit-1.

Median per call from 7 timed batches.

Timing method: CLOCK_MONOTONIC; warmup=100ms; target_batch=5ms; max_batch=1048576; storage_chunk=512.

Argument parsing, output validation, disposal and database writes are outside timing.
Timings use spaces for thousands grouping and a decimal point; bit counts are ungrouped.
Inputs show the actual operands, including +0.125i for permitted complex arguments; integer orders remain real.
Parenthesised qualifiers describe parameter-specific workloads of the same public function.
`number-double` includes generic number dispatch; the other fixed profiles call qfloat/qcomplex directly.
Requested multiprecision bits do not guarantee that every algorithm achieves that accuracy.
Operations and precision columns with only unsupported results are omitted; — denotes an unsupported cell.
Failures and timeouts remain visible. † marks a retained slowdown outlier.

### number / double precision

| Operation | Inputs | 53 bits |
|---|---|---:|
| E1 | 2.345678 | 12.6 µs |
| Ei | 2.345678 | 10.0 µs |
| Li | 2.345 | 11.5 µs † |
| abs | 0.7 | 11.5 ns |
| acos | 0.7 | 28.1 ns |
| acosec | 2.345 | 23.3 ns |
| acosech | 0.7 | 93.6 ns |
| acosh | 2.345 | 46.4 ns |
| acot | 0.7 | 73.1 ns |
| acoth | 2.345 | 40.7 ns |
| add | 0.7, 0.7 | 23.8 ns |
| add_long | 17, 3 | 746.0 ns |
| appell_F1 | 0.5, 0.7, 0.8, 2.5, 0.2, 0.3 | 47.0 µs † |
| arccovercos | 0.5 | 134.5 ns |
| arccoversin | 0.5 | 177.3 ns |
| archacovercos | 0.5 | 326.6 ns |
| archacoversin | 0.5 | 281.0 ns |
| archavercos | 0.5 | 369.0 ns |
| archaversin | 0.5 | 370.8 ns |
| arcvercos | 0.5 | 185.0 ns |
| arcversin | 0.5 | 228.4 ns |
| arg | 0.7 | 136.2 ns |
| asec | 2.345 | 28.1 ns |
| asech | 0.7 | 53.8 ns |
| asin | 0.7 | 30.1 ns |
| asinh | 0.7 | 88.2 ns |
| atan | 0.7 | 48.3 ns |
| atan2 | 0.5, -0.75 | 61.3 ns |
| atanh | 0.7 | 43.0 ns |
| bessel_I | 2, 0.7 | 60.7 µs |
| bessel_J | 2, 0.7 | 7.4 µs |
| bessel_K | 2, 0.7 | 28.7 µs |
| bessel_Y | 2, 0.7 | 24.2 µs |
| beta | 2.3, 4.5 | 188.5 ns |
| beta_pdf | 0.5, 2.5, 3.5 | 262.7 ns |
| binomial | 5, 2 | 277.4 ns |
| ceil | 0.7 | 106.1 ns |
| chebyshev_Tn | 2, 0.7 | 920.0 ns |
| chebyshev_Un | 2, 0.7 | 2.0 µs |
| circ | 0.7 | 352.6 ns |
| clausen | 3, 0.7 | 10.5 µs |
| clausen2 | 0.7 | 11.2 µs |
| conj | 0.7 | 12.8 ns |
| cos | 0.7 | 55.1 ns |
| cosec | 0.7 | 42.5 ns |
| cosech | 0.7 | 138.3 ns |
| cosh | 0.7 | 60.6 ns |
| cot | 0.7 | 64.3 ns |
| coth | 0.7 | 82.6 ns |
| covercos | 0.7 | 202.1 ns |
| coversin | 0.7 | 254.3 ns |
| cubrt | 0.7 | 12.8 µs † |
| digamma | 2.345 | 4.0 µs |
| dilog | 0.7 | 18.1 µs |
| div | 0.7, 0.7 | 16.5 ns |
| erf | 0.7 | 88.4 ns |
| erfc | 0.7 | 100.0 ns |
| erfcinv | 0.5 | 367.2 ns |
| erfinv | 0.5 | 471.9 ns |
| exp | 1.23456789 | 83.6 ns |
| floor | 0.7 | 24.5 ns |
| gamma | 2.345 | 199.4 ns |
| gammainc_P | 2.3, 1.4 | 18.5 µs |
| gammainc_Q | 2.3, 1.4 | 15.8 µs |
| gammainc_lower | 2.3, 1.4 | 18.7 µs |
| gammainc_upper | 2.3, 1.4 | 17.2 µs |
| gammainv | 9.5 | 154.5 µs |
| hacovercos | 0.7 | 320.5 ns |
| hacoversin | 0.7 | 364.4 ns |
| harmonic_poly | 2, 0.7 | 2.0 µs |
| havercos | 0.7 | 351.2 ns |
| haversin | 0.7 | 306.1 ns |
| hermite_Hn | 2, 0.7 | 2.8 µs † |
| hypergeometric_pFq | 0.7, 2.5, 0.3 | 13.3 µs † |
| hypot | 0.7, 1.2 | 32.8 ns |
| imag_part | 0.7 | 42.4 ns |
| inv | 0.7 | 20.5 ns |
| lambert_W0 | 0.7 | 22.0 µs |
| lambert_Wm1 | -0.2 | 26.8 µs † |
| lambert_Wn | 1, 0.7 | 36.5 µs |
| lauricella_F | 0.5, 0.3, 2.5 | 32.2 µs † |
| ldexp | 1.23456789, 3 | 97.4 ns |
| legendre_chi | 2, 0.7 | 38.4 µs |
| lerch_phi | 0.3, 2, 1.4 | 57.2 µs |
| lerch_phi (integer order) | 0.3125, -15, 0.375 | 36.0 µs |
| lerch_phi (negative order) | 0.3125, -15, 0.375 | 45.7 µs † |
| lg | 2.345678 | 47.2 ns |
| lgamma | 2.345 | 84.6 ns |
| ln | 2.345678 | 26.5 ns |
| log | 2.345678 | 22.9 ns |
| log10 | 2.345678 | 33.5 ns |
| logbeta | 2.3, 4.5 | 206.7 ns |
| logbeta_pdf | 0.5, 2.5, 3.5 | 338.9 ns |
| lommel_S | 0.5, 0.7, 1.2 | 10.8 µs |
| mul | 0.7, 0.7 | 19.2 ns |
| mul_long | 17, 3 | 739.5 ns |
| mul_pow10 | 1.23456789, 3 | 129.7 ns |
| neg | 0.7 | 12.7 ns |
| normal_cdf | 0.7 | 114.0 ns |
| normal_logpdf | 0.7 | 27.0 ns |
| normal_pdf | 0.7 | 33.8 ns |
| polygamma | 3, 2.345 | 38.6 µs † |
| polylog | 2, 0.7 | 21.1 µs |
| polylog1 | 0.7 | 244.4 ns |
| pow | 1.23456789, 3.5 | 46.3 ns |
| pow10 | 3 | 284.1 ns |
| pow_int | 1.23456789, 3 | 127.6 ns |
| productlog | 0.7 | 29.3 µs |
| qdigamma | 0.5, 2.3 | 99.0 µs † |
| real_part | 0.7 | 42.3 ns |
| rect | 0.7 | 402.2 ns |
| root | 1.2345, 3 | 11.3 µs |
| round | 0.7 | 550.3 ns |
| roundn | 1.23456789, 3 | 6.4 µs |
| rounds | 1.23456789, 3 | 10.6 µs † |
| sec | 0.7 | 38.9 ns |
| sech | 0.7 | 47.4 ns |
| sgn | 0.7 | 218.1 ns |
| sin | 0.567 | 57.4 ns |
| sinc | 0.7 | 4.5 µs |
| sincos | 0.7 | 1.1 µs |
| sinh | 0.7 | 83.7 ns |
| sinhcosh | 0.7 | 185.5 ns |
| sqr | 0.7 | 12.4 ns |
| sqrt | 1.23456789 | 14.2 ns |
| step | 0.7 | 185.2 ns |
| struve_H | 2, 0.7 | 95.0 µs |
| struve_L | 2, 0.7 | 97.2 µs |
| sub | 0.7, 0.7 | 18.3 ns |
| tan | 0.7 | 11.7 µs † |
| tanh | 0.7 | 80.2 ns |
| tetragamma | 2.345 | 5.1 µs |
| tri | 0.7 | 416.8 ns |
| trigamma | 2.345 | 3.9 µs |
| vercos | 0.7 | 174.6 ns |
| versin | 0.7 | 175.6 ns |
| zatahp | 2.3, 1.4 | 60.0 µs |
| zatahp (integer order) | -4, 0.75 | 60.8 µs |
| zeta | 2.345 | 104.0 µs |
| zeta_pair | 2.345 | 229.0 µs |
| zetah | 2.3, 1.4 | 41.5 µs |
| zetap | 2.345 | 141.4 µs |

### number / multi-precision floating point

| Operation | Inputs | 256 bits | 512 bits | 768 bits | 1024 bits | 2048 bits | 4096 bits |
|---|---|---:|---:|---:|---:|---:|---:|
| E1 | 2.345678 | 78.0 µs † | 85.4 µs | 171.4 µs | 267.0 µs | 763.8 µs | 2.620 ms |
| Ei | 2.345678 | 62.1 µs | 97.3 µs | 159.7 µs | 247.5 µs | 670.8 µs | 2.756 ms |
| Li | 2.345 | 54.6 µs | 93.4 µs | 169.4 µs | 194.3 µs | 616.4 µs | 2.667 ms |
| abs | 0.7 | 112.2 ns | 121.4 ns | 139.2 ns | 139.8 ns | 174.8 ns | 127.8 ns |
| acos | 0.7 | 26.1 µs | 40.8 µs | 69.0 µs † | 79.7 µs | 165.0 µs | 515.4 µs |
| acosec | 2.345 | 27.0 µs | 32.4 µs | 58.1 µs | 74.1 µs | 165.9 µs | 390.1 µs |
| acosech | 0.7 | 7.6 µs | 12.7 µs | 16.0 µs | 23.4 µs | 63.6 µs | 153.4 µs |
| acosh | 2.345 | 8.3 µs | 14.0 µs | 18.7 µs | 21.8 µs | 49.9 µs | 171.4 µs |
| acot | 0.7 | 22.2 µs | 27.4 µs | 40.6 µs | 50.2 µs | 170.5 µs † | 474.3 µs |
| acoth | 2.345 | 9.8 µs | 14.2 µs | 19.3 µs | 19.4 µs | 64.3 µs | 161.2 µs |
| add | 0.7, 0.7 | 168.8 ns | 167.9 ns | 177.7 ns | 184.8 ns | 228.0 ns | 436.9 ns |
| add_long | 17, 3 | 1.1 µs | 989.7 ns | 979.7 ns | 859.4 ns | 1.1 µs | 1.3 µs |
| appell_F1 | 0.5, 0.7, 0.8, 2.5, 0.2, 0.3 | 154.5 µs | 429.5 µs | 905.0 µs | 1.276 ms | 5.399 ms | 27.944 ms |
| arccovercos | 0.5 | 20.7 µs | 37.3 µs | 44.0 µs | 72.1 µs | 155.6 µs | 419.1 µs |
| arccoversin | 0.5 | 63.9 µs † | 27.5 µs | 39.3 µs | 72.4 µs | 141.7 µs | 371.3 µs |
| archacovercos | 0.5 | 2.7 µs † | 1.8 µs | 2.3 µs | 2.0 µs | 2.6 µs | 2.7 µs |
| archacoversin | 0.5 | 2.3 µs † | 2.5 µs † | 2.9 µs | 2.6 µs | 2.3 µs | 2.4 µs |
| archavercos | 0.5 | 3.0 µs † | 2.6 µs † | 2.0 µs | 3.1 µs † | 1.9 µs | 2.2 µs |
| archaversin | 0.5 | 2.2 µs | 2.1 µs | 2.6 µs | 2.5 µs | 2.5 µs | 2.7 µs |
| arcvercos | 0.5 | 22.0 µs | 39.4 µs | 36.5 µs | 53.9 µs | 152.2 µs | 371.0 µs |
| arcversin | 0.5 | 23.8 µs | 88.9 µs † | 44.1 µs | 58.7 µs | 163.4 µs | 348.5 µs |
| arg | 0.7 | 860.4 ns | 620.2 ns | 819.5 ns | 733.1 ns | 738.2 ns | 1.0 µs |
| asec | 2.345 | 34.2 µs | 31.9 µs | 54.4 µs | 76.2 µs | 190.2 µs † | 422.4 µs |
| asech | 0.7 | 7.8 µs | 13.0 µs | 22.7 µs | 20.3 µs | 60.9 µs | 185.5 µs |
| asin | 0.7 | 23.7 µs | 35.1 µs | 55.4 µs | 74.6 µs | 139.8 µs | 430.9 µs |
| asinh | 0.7 | 7.1 µs | 14.2 µs | 16.6 µs | 23.1 µs | 53.6 µs | 133.6 µs |
| atan | 0.7 | 20.3 µs | 40.6 µs † | 42.9 µs | 60.7 µs | 167.4 µs | 540.2 µs † |
| atan2 | 0.5, -0.75 | 18.4 µs | 34.0 µs | 61.1 µs | 71.1 µs | 147.5 µs | 407.4 µs |
| atanh | 0.7 | 7.3 µs | 14.5 µs | 18.5 µs | 22.7 µs | 56.9 µs | 200.9 µs † |
| bessel_I | 2, 0.7 | 144.6 µs | 311.2 µs | 479.1 µs | 753.8 µs | 2.213 ms | 8.990 ms |
| bessel_J | 2, 0.7 | 12.0 µs | 25.0 µs | 35.4 µs | 59.9 µs | 251.1 µs | 1.265 ms |
| bessel_K | 2, 0.7 | 74.3 µs | 129.5 µs | 232.8 µs | 279.7 µs | 1.374 ms | 5.025 ms |
| bessel_Y | 2, 0.7 | 62.2 µs | 144.7 µs | 188.4 µs | 390.5 µs † | 1.026 ms | 5.055 ms |
| beta | 2.3, 4.5 | 168.2 µs | 346.9 µs | 711.4 µs | 901.7 µs | 4.119 ms | 15.584 ms |
| beta_pdf | 0.5, 2.5, 3.5 | 155.9 µs | 299.0 µs | 552.3 µs | 975.3 µs † | 3.276 ms | 11.317 ms |
| binomial | 5, 2 | 139.8 µs | 287.1 µs | 606.6 µs | 1.378 ms | 4.335 ms | 13.929 ms |
| ceil | 0.7 | 835.6 ns | 853.3 ns | 735.3 ns | 790.1 ns | 747.2 ns | 672.1 ns |
| chebyshev_Tn | 2, 0.7 | 4.3 µs | 4.2 µs | 5.5 µs | 5.5 µs | 8.0 µs † | 6.7 µs |
| chebyshev_Un | 2, 0.7 | 6.5 µs † | 6.4 µs | 8.5 µs † | 5.9 µs | 7.0 µs | 7.5 µs |
| circ | 0.7 | 1.3 µs | 1.3 µs | 1.3 µs | 1.7 µs † | 1.5 µs | 1.3 µs |
| clausen | 3, 0.7 | 44.6 µs † | 94.6 µs | 216.7 µs | 301.2 µs | 1.002 ms | 6.165 ms |
| clausen2 | 0.7 | 42.1 µs | 86.6 µs | 203.5 µs | 376.1 µs † | 1.156 ms | 5.085 ms |
| conj | 0.7 | 104.4 ns | 145.0 ns | 125.1 ns | 125.1 ns | 133.5 ns | 159.7 ns |
| cos | 0.7 | 3.9 µs | 6.4 µs | 11.7 µs | 19.5 µs | 52.1 µs | 216.9 µs |
| cosec | 0.7 | 4.7 µs | 9.6 µs | 15.0 µs | 23.0 µs | 69.6 µs † | 205.1 µs |
| cosech | 0.7 | 4.9 µs | 8.4 µs | 15.6 µs | 25.2 µs | 65.8 µs | 197.2 µs |
| cosh | 0.7 | 4.3 µs | 10.1 µs | 16.3 µs † | 21.4 µs | 69.2 µs | 204.3 µs |
| cot | 0.7 | 9.9 µs | 12.7 µs | 26.4 µs | 35.8 µs | 115.7 µs | 578.9 µs † |
| coth | 0.7 | 6.4 µs | 16.0 µs | 24.7 µs | 40.8 µs | 130.1 µs | 456.8 µs |
| covercos | 0.7 | 6.3 µs | 10.7 µs | 12.3 µs | 28.5 µs † | 66.8 µs | 204.7 µs |
| coversin | 0.7 | 5.1 µs | 9.3 µs | 13.9 µs | 20.2 µs | 60.6 µs | 214.8 µs |
| cubrt | 0.7 | 17.8 µs | 21.2 µs | 44.2 µs | 47.5 µs | 133.1 µs | 357.8 µs |
| digamma | 2.345 | 185.0 µs † | 397.4 µs | 897.7 µs | 1.017 ms | 5.043 ms | 21.618 ms |
| dilog | 0.7 | 156.1 µs | 343.1 µs | 824.8 µs | 1.330 ms | 4.746 ms | 22.420 ms |
| div | 0.7, 0.7 | 493.3 ns | 913.9 ns | 1.2 µs | 1.5 µs | 4.9 µs | 15.7 µs |
| erf | 0.7 | 30.0 µs | 55.5 µs | 104.7 µs | 128.9 µs | 751.0 µs | 3.443 ms |
| erfc | 0.7 | 21.9 µs | 56.0 µs | 110.9 µs | 158.1 µs | 710.7 µs | 2.450 ms |
| erfcinv | 0.5 | 162.5 µs | 212.0 µs | 2.136 ms † | 715.0 µs | 2.182 ms | 9.568 ms |
| erfinv | 0.5 | 127.8 µs | 270.6 µs | 1.284 ms † | 986.3 µs | 2.232 ms | 6.706 ms |
| exp | 1.23456789 | 4.4 µs | 8.1 µs | 15.5 µs | 21.1 µs | 65.7 µs | 298.4 µs |
| floor | 0.7 | 139.4 ns | 121.0 ns | 159.8 ns | 111.2 ns | 126.9 ns | 123.6 ns |
| gamma | 2.345 | 54.2 µs | 94.4 µs | 194.7 µs | 382.0 µs | 1.414 ms | 6.205 ms |
| gammainc_P | 2.3, 1.4 | 148.9 µs | 346.9 µs | 679.8 µs | 1.097 ms | 3.448 ms | 30.317 ms † |
| gammainc_Q | 2.3, 1.4 | 170.2 µs | 449.4 µs | 720.9 µs | 1.110 ms | 4.318 ms † | 20.632 ms |
| gammainc_lower | 2.3, 1.4 | 77.0 µs | 263.2 µs | 374.2 µs | 616.8 µs | 2.052 ms | 16.398 ms † |
| gammainc_upper | 2.3, 1.4 | 186.2 µs | 488.1 µs † | 684.8 µs | 928.9 µs | 4.170 ms | 17.620 ms |
| gammainv | 9.5 | 599.4 µs | 1.446 ms | 1.699 ms | 2.369 ms | 8.138 ms | 28.969 ms |
| hacovercos | 0.7 | 6.7 µs | 11.2 µs | 20.6 µs | 20.7 µs | 67.3 µs | 259.1 µs |
| hacoversin | 0.7 | 6.1 µs | 12.0 µs | 14.6 µs | 19.4 µs | 62.5 µs | 222.1 µs |
| harmonic_poly | 2, 0.7 | 6.1 µs | 6.0 µs | 8.1 µs | 7.2 µs | 18.0 µs | 42.6 µs |
| havercos | 0.7 | 4.6 µs | 8.3 µs | 14.1 µs | 25.9 µs | 69.4 µs | 197.5 µs |
| haversin | 0.7 | 5.3 µs | 11.7 µs † | 15.7 µs | 19.8 µs | 51.0 µs | 186.4 µs |
| hermite_Hn | 2, 0.7 | 5.5 µs | 5.4 µs | 5.2 µs | 5.5 µs | 6.3 µs | 8.5 µs |
| hypergeometric_pFq | 0.7, 2.5, 0.3 | 66.4 µs † | 179.7 µs † | 248.3 µs † | 406.4 µs † | 1.617 ms † | 7.605 ms † |
| hypot | 0.7, 1.2 | 938.2 ns | 1.9 µs | 1.6 µs | 2.4 µs | 4.7 µs | 11.7 µs |
| imag_part | 0.7 | 144.3 ns | 148.3 ns | 114.4 ns | 89.6 ns | 111.0 ns | 119.4 ns |
| inv | 0.7 | 297.4 ns | 604.9 ns | 571.0 ns | 742.2 ns | 1.9 µs | 5.0 µs |
| lambert_W0 | 0.7 | 73.0 µs | 151.1 µs | 213.5 µs | 429.0 µs | 1.361 ms | 3.912 ms |
| lambert_Wm1 | -0.2 | 75.1 µs | 150.6 µs | 282.7 µs | 317.0 µs | 1.156 ms | 3.353 ms |
| ldexp | 1.23456789, 3 | 208.3 ns | 151.1 ns | 233.3 ns | 144.7 ns | 225.8 ns | 191.0 ns |
| legendre_chi | 2, 0.7 | 73.1 µs | 138.4 µs | 188.9 µs | 402.1 µs | 982.0 µs | 3.946 ms |
| lerch_phi | 0.3, 2, 1.4 | 153.9 µs | 341.7 µs | 609.4 µs | 1.142 ms † | 4.429 ms † | 19.140 ms † |
| lerch_phi (integer order) | 0.3125, -15, 0.375 | 43.7 µs | 32.0 µs | 41.2 µs | 33.3 µs | 44.5 µs | 44.3 µs |
| lerch_phi (negative order) | 0.3125, -15, 0.375 | 43.8 µs | 39.7 µs | 42.8 µs | 45.0 µs | 39.1 µs | 57.3 µs † |
| lg | 2.345678 | 13.1 µs | 23.2 µs | 43.8 µs | 49.6 µs | 92.8 µs | 384.6 µs |
| lgamma | 2.345 | 47.5 µs | 157.3 µs | 214.7 µs | 352.8 µs | 1.285 ms | 6.765 ms |
| ln | 2.345678 | 6.6 µs | 10.6 µs | 16.2 µs | 24.4 µs | 52.9 µs | 163.9 µs |
| log | 2.345678 | 6.0 µs | 10.4 µs | 12.6 µs | 23.3 µs | 59.1 µs | 144.5 µs |
| log10 | 2.345678 | 16.6 µs | 20.5 µs | 36.0 µs | 46.8 µs | 131.1 µs † | 329.5 µs |
| logbeta | 2.3, 4.5 | 145.4 µs | 317.1 µs | 558.0 µs | 1.387 ms | 4.568 ms | 19.267 ms |
| logbeta_pdf | 0.5, 2.5, 3.5 | 137.4 µs | 251.7 µs | 485.2 µs | 845.8 µs | 2.482 ms | 10.924 ms |
| lommel_S | 0.5, 0.7, 1.2 | 123.7 µs † | 107.6 µs | 180.7 µs | 294.5 µs | 1.029 ms | 4.193 ms |
| mul | 0.7, 0.7 | 185.5 ns | 268.0 ns | 380.3 ns | 389.2 ns | 1.2 µs | 3.1 µs |
| mul_long | 17, 3 | 1.0 µs | 1.6 µs † | 1.3 µs | 1.2 µs | 1.1 µs | 1.0 µs |
| mul_pow10 | 1.23456789, 3 | 581.0 ns | 490.5 ns | 691.5 ns | 857.7 ns | 854.8 ns | 824.5 ns |
| neg | 0.7 | 185.2 ns | 139.4 ns | 144.3 ns | 141.5 ns | 127.3 ns | 149.5 ns |
| normal_cdf | 0.7 | 22.9 µs | 110.1 µs | 83.3 µs | 133.2 µs | 601.8 µs † | 2.749 ms |
| normal_logpdf | 0.7 | 6.9 µs | 10.6 µs | 17.3 µs | 21.2 µs | 54.2 µs | 130.5 µs |
| normal_pdf | 0.7 | 5.5 µs | 8.6 µs | 15.0 µs | 25.1 µs | 84.1 µs | 256.0 µs |
| polygamma | 3, 2.345 | 74.1 µs | 178.4 µs | 343.4 µs | 485.4 µs | 3.002 ms | 12.352 ms |
| polylog | 2, 0.7 | 118.4 µs | 378.9 µs | 621.7 µs | 1.022 ms | 4.779 ms | 22.205 ms |
| polylog1 | 0.7 | 8.2 µs | 13.9 µs | 16.8 µs | 23.4 µs | 42.8 µs | 141.1 µs |
| pow | 1.23456789, 3.5 | 15.6 µs † | 20.8 µs | 35.5 µs | 45.9 µs | 127.3 µs | 341.6 µs |
| pow10 | 3 | 396.9 ns | 458.7 ns | 537.8 ns | 814.5 ns | 525.6 ns | 567.0 ns |
| pow_int | 1.23456789, 3 | 393.6 ns | 471.9 ns | 694.1 ns | 955.7 ns | 3.1 µs | 10.1 µs |
| productlog | 0.7 | 68.5 µs | 145.3 µs | 198.1 µs | 323.1 µs | 1.276 ms | 4.204 ms |
| real_part | 0.7 | 134.6 ns | 118.2 ns | 133.6 ns | 161.6 ns | 107.7 ns | 118.7 ns |
| rect | 0.7 | 1.9 µs | 1.8 µs | 1.5 µs | 1.7 µs | 1.5 µs | 1.6 µs |
| root | 1.2345, 3 | 19.0 µs | 27.9 µs | 36.1 µs | 52.7 µs | 124.8 µs | 385.4 µs |
| round | 0.7 | 3.8 µs | 3.7 µs | 4.0 µs | 3.2 µs | 3.4 µs | 3.2 µs |
| roundn | 1.23456789, 3 | 6.8 µs | 8.5 µs | 10.2 µs | 9.2 µs | 9.5 µs | 12.7 µs |
| rounds | 1.23456789, 3 | 22.2 µs | 38.8 µs | 132.1 µs † | 74.6 µs | 100.7 µs | 314.3 µs |
| sec | 0.7 | 3.4 µs | 5.5 µs | 10.0 µs | 20.2 µs | 59.5 µs | 166.4 µs |
| sech | 0.7 | 4.6 µs | 8.9 µs | 13.4 µs | 22.4 µs | 59.1 µs | 213.6 µs |
| sgn | 0.7 | 281.6 ns | 322.3 ns | 280.9 ns | 280.1 ns | 287.3 ns | 298.3 ns |
| sin | 0.567 | 4.2 µs | 9.7 µs | 15.5 µs | 18.4 µs | 59.9 µs | 239.2 µs |
| sinc | 0.7 | 6.2 µs | 12.9 µs | 18.4 µs | 63.6 µs † | 66.9 µs | 182.3 µs |
| sincos | 0.7 | 13.3 µs | 25.4 µs † | 25.4 µs | 24.9 µs | 83.9 µs | 179.0 µs |
| sinh | 0.7 | 5.0 µs | 9.1 µs | 14.9 µs | 24.7 µs † | 68.5 µs | 186.4 µs |
| sinhcosh | 0.7 | 4.9 µs | 7.9 µs | 13.2 µs | 23.9 µs | 54.7 µs | 183.2 µs |
| sqr | 0.7 | 203.8 ns | 304.8 ns | 479.8 ns | 601.1 ns | 860.0 ns | 2.3 µs |
| sqrt | 1.23456789 | 454.4 ns | 502.6 ns | 655.9 ns | 961.3 ns | 1.5 µs | 3.1 µs |
| step | 0.7 | 340.0 ns | 334.1 ns | 347.9 ns | 419.6 ns | 446.6 ns | 441.6 ns |
| struve_H | 2, 0.7 | 188.7 µs | 531.1 µs | 700.1 µs | 1.165 ms | 3.869 ms | 13.787 ms |
| struve_L | 2, 0.7 | 223.5 µs | 520.2 µs | 950.8 µs | 1.113 ms | 4.972 ms † | 15.303 ms |
| sub | 0.7, 0.7 | 145.8 ns | 130.8 ns | 135.3 ns | 143.1 ns | 210.8 ns | 340.5 ns |
| tan | 0.7 | 18.8 µs | 21.1 µs | 30.3 µs | 31.7 µs | 85.1 µs | 223.1 µs |
| tanh | 0.7 | 4.2 µs | 9.3 µs | 14.4 µs | 21.8 µs † | 142.8 µs † | 186.0 µs |
| tetragamma | 2.345 | 62.2 µs | 144.4 µs | 295.6 µs | 622.6 µs | 2.686 ms | 12.698 ms |
| tri | 0.7 | 1.3 µs | 1.6 µs | 1.9 µs | 1.1 µs | 1.2 µs | 1.4 µs |
| trigamma | 2.345 | 38.4 µs | 139.1 µs | 243.4 µs | 358.9 µs | 2.238 ms | 8.499 ms |
| vercos | 0.7 | 5.0 µs † | 8.6 µs † | 10.8 µs | 19.7 µs | 55.1 µs | 152.6 µs |
| versin | 0.7 | 4.1 µs | 7.2 µs | 10.5 µs | 15.2 µs | 56.9 µs | 470.7 µs † |
| zatahp | 2.3, 1.4 | 108.5 µs | 228.8 µs | 293.0 µs | 477.3 µs | 1.946 ms | 10.176 ms |
| zatahp (integer order) | -4, 0.75 | 99.1 µs | 120.0 µs | 297.8 µs † | 243.5 µs | 1.109 ms | 4.193 ms |
| zeta | 2.345 | 1.271 ms | 5.608 ms | 13.350 ms | 1.611 ms | 6.778 ms | 26.796 ms |
| zeta_pair | 2.345 | 328.2 µs | 582.0 µs | 1.053 ms | 2.189 ms | 8.743 ms | 38.335 ms |
| zetah | 2.3, 1.4 | 48.8 µs | 98.1 µs | 157.1 µs | 203.2 µs | 1.149 ms | 3.693 ms |
| zetap | 2.345 | 345.4 µs | 792.6 µs † | 1.132 ms | 2.115 ms † | 7.489 ms | 34.579 ms |

### number / multi-precision complex floating point

| Operation | Inputs | 256 bits | 512 bits | 768 bits | 1024 bits | 2048 bits | 4096 bits |
|---|---|---:|---:|---:|---:|---:|---:|
| E1 | 2.345678+0.125i | 107.1 µs | 193.3 µs | 401.5 µs | 643.1 µs | 1.961 ms | 9.657 ms |
| Ei | 2.345678+0.125i | 82.8 µs | 228.1 µs | 342.6 µs | 650.9 µs | 2.134 ms | 7.605 ms |
| Li | 2.345+0.125i | 92.2 µs | 184.9 µs | 348.7 µs | 653.8 µs | 3.213 ms | 8.886 ms |
| abs | 0.7+0.125i | 1.2 µs | 1.6 µs | 1.5 µs | 1.6 µs | 3.4 µs | 6.6 µs |
| acos | 0.7+0.125i | 58.5 µs | 110.7 µs | 136.0 µs | 96.3 µs | 231.5 µs | 489.2 µs |
| acosec | 2.345+0.125i | 61.3 µs | 95.5 µs | 124.9 µs | 148.6 µs | 417.5 µs | 1.046 ms |
| acosech | 0.7+0.125i | 53.0 µs | 85.6 µs | 126.8 µs | 161.0 µs | 430.9 µs | 966.6 µs |
| acosh | 2.345+0.125i | 65.1 µs | 85.3 µs | 104.6 µs | 62.9 µs | 421.3 µs | 528.2 µs |
| acot | 0.7+0.125i | 53.8 µs | 89.7 µs | 90.8 µs | 156.4 µs | 381.3 µs | 955.9 µs |
| acoth | 2.345+0.125i | 41.9 µs | 61.0 µs | 90.9 µs | 128.8 µs | 272.0 µs | 927.3 µs |
| add | 0.7+0.125i, 0.7+0.125i | 435.7 ns | 530.8 ns | 502.1 ns | 465.2 ns | 639.2 ns | 699.4 ns |
| add_long | 17+0.125i, 3 | 3.1 µs | 2.9 µs | 3.5 µs | 3.6 µs | 3.6 µs | 3.4 µs |
| appell_F1 | <span style="display:inline-block;width:40ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">0.5+0.125i, 0.7+0.125i, 0.8+0.125i, 2.5+<br/>0.125i, 0.2+0.125i, 0.3+0.125i</span> | 678.3 µs | 1.382 ms | 2.900 ms | 5.450 ms | 25.354 ms | 140.734 ms |
| arccovercos | 0.5+0.125i | 84.3 µs | 97.8 µs | 56.9 µs | 231.8 µs | 182.5 µs | 458.3 µs |
| arccoversin | 0.5+0.125i | 61.9 µs | 44.2 µs | 56.3 µs | 176.9 µs | 163.7 µs | 494.0 µs |
| archacovercos | 0.5+0.125i | 15.5 µs | 18.1 µs | 21.3 µs | 33.3 µs | 53.1 µs | 135.9 µs |
| archacoversin | 0.5+0.125i | 17.8 µs | 23.2 µs | 21.2 µs | 35.5 µs | 50.5 µs | 139.1 µs |
| archavercos | 0.5+0.125i | 18.7 µs | 17.0 µs | 23.4 µs | 29.0 µs | 59.7 µs | 157.4 µs |
| archaversin | 0.5+0.125i | 13.4 µs | 23.8 µs | 25.2 µs | 33.7 µs | 56.9 µs | 130.5 µs |
| arcvercos | 0.5+0.125i | 56.8 µs | 44.2 µs | 116.8 µs | 83.0 µs | 181.4 µs | 446.4 µs |
| arcversin | 0.5+0.125i | 44.0 µs | 66.3 µs | 125.9 µs | 77.0 µs | 170.9 µs | 527.9 µs |
| arg | 0.7+0.125i | 16.5 µs | 24.6 µs | 43.0 µs | 72.9 µs | 108.9 µs | 450.3 µs |
| asec | 2.345+0.125i | 60.8 µs | 96.6 µs | 106.7 µs | 153.5 µs | 356.0 µs | 1.001 ms |
| asech | 0.7+0.125i | 31.7 µs | 48.1 µs | 59.1 µs | 115.7 µs | 186.9 µs | 632.9 µs |
| asin | 0.7+0.125i | 83.4 µs | 90.8 µs | 133.4 µs | 205.2 µs | 382.4 µs | 493.3 µs |
| asinh | 0.7+0.125i | 59.5 µs | 90.7 µs | 121.6 µs | 157.6 µs | 369.5 µs | 926.7 µs |
| atan | 0.7+0.125i | 48.0 µs | 81.0 µs | 109.2 µs | 210.7 µs † | 322.8 µs | 1.016 ms |
| atanh | 0.7+0.125i | 50.3 µs | 92.7 µs | 108.3 µs | 193.1 µs | 316.6 µs | 931.9 µs |
| bessel_I | 2, 0.7+0.125i | 224.7 µs | 426.8 µs | 706.5 µs | 1.353 ms | 4.037 ms | 21.275 ms |
| bessel_J | 2, 0.7+0.125i | 246.1 µs | 522.5 µs | 767.7 µs | 1.324 ms | 5.008 ms | 18.615 ms |
| bessel_K | 2, 0.7+0.125i | 128.0 µs | 267.1 µs | 491.2 µs | 719.7 µs | 3.301 ms | 13.470 ms |
| bessel_Y | 2, 0.7+0.125i | 221.6 µs | 457.7 µs | 715.5 µs | 1.358 ms | 4.697 ms | 20.093 ms |
| ceil | 0.7+0.125i | 2.1 µs | 1.7 µs | 1.7 µs | 2.0 µs | 1.7 µs | 1.8 µs |
| chebyshev_Tn | 2, 0.7+0.125i | 10.7 µs | 10.6 µs | 11.2 µs | 16.7 µs | 14.7 µs | 17.4 µs |
| chebyshev_Un | 2, 0.7+0.125i | 17.7 µs | 14.5 µs | 19.5 µs | 17.9 µs | 17.2 µs | 21.7 µs |
| circ | 0.7 | 2.4 µs | 2.7 µs | 3.0 µs | 4.0 µs | 5.0 µs | 8.1 µs |
| clausen | 3, 0.7+0.125i | 575.0 µs † | 423.3 µs | 723.0 µs | 2.795 ms † | 5.294 ms | 27.414 ms |
| clausen2 | 0.7+0.125i | 195.2 µs | 426.7 µs | 840.5 µs | 1.279 ms | 6.129 ms | 32.607 ms |
| conj | 0.7+0.125i | 351.0 ns | 311.9 ns | 395.6 ns | 348.1 ns | 332.3 ns | 378.5 ns |
| cos | 0.7+0.125i | 23.3 µs | 16.3 µs | 24.9 µs | 45.9 µs | 107.3 µs | 347.2 µs |
| cosec | 0.7+0.125i | 20.7 µs | 20.8 µs | 33.2 µs | 52.6 µs | 114.4 µs | 391.9 µs |
| cosech | 0.7+0.125i | 15.7 µs | 17.6 µs | 26.4 µs | 130.4 µs | 110.5 µs | 360.4 µs |
| cosh | 0.7+0.125i | 10.8 µs | 17.5 µs | 21.7 µs | 118.0 µs | 102.2 µs | 279.7 µs |
| cot | 0.7+0.125i | 46.4 µs | 33.1 µs | 54.2 µs | 101.1 µs | 232.2 µs | 998.6 µs |
| coth | 0.7+0.125i | 22.9 µs | 37.5 µs | 46.4 µs | 178.5 µs | 211.4 µs | 603.9 µs |
| covercos | 0.7+0.125i | 20.9 µs | 18.7 µs | 31.4 µs | 53.2 µs | 130.1 µs | 454.0 µs |
| coversin | 0.7+0.125i | 28.3 µs | 22.5 µs | 27.5 µs | 54.1 µs | 128.0 µs | 456.1 µs |
| cubrt | 0.7+0.125i | 49.1 µs | 79.2 µs | 103.7 µs | 172.3 µs | 407.3 µs | 1.325 ms |
| digamma | 2.345+0.125i | 335.9 µs | 510.8 µs | 874.7 µs | 2.434 ms † | 4.873 ms | 24.558 ms |
| dilog | 0.7+0.125i | 174.8 µs | 470.2 µs | 1.051 ms | 1.931 ms | 6.545 ms | 27.958 ms |
| div | 0.7+0.125i, 0.7+0.125i | 10.3 µs | 13.8 µs | 19.1 µs | 26.5 µs | 44.1 µs | 114.5 µs |
| erf | 0.7+0.125i | 90.6 µs | 226.4 µs | 322.9 µs | 551.8 µs | 2.230 ms | 10.196 ms |
| erfc | 0.7+0.125i | 114.8 µs | 227.1 µs | 391.6 µs | 557.7 µs | 2.182 ms | 11.720 ms |
| erfcinv | 0.5+0.125i | 1.123 ms | 1.250 ms | 1.815 ms | 2.472 ms | 8.579 ms | 27.177 ms |
| erfinv | 0.5+0.125i | 591.5 µs | 1.089 ms | 1.995 ms | 2.711 ms | 7.172 ms | 39.851 ms † |
| exp | 1.23456789+0.125i | 10.7 µs | 21.2 µs | 36.2 µs | 61.3 µs | 204.8 µs | 726.9 µs |
| floor | 0.7+0.125i | 345.8 ns | 316.2 ns | 369.6 ns | 310.3 ns | 326.9 ns | 343.9 ns |
| gamma | 2.345+0.125i | 252.7 µs † | 403.6 µs † | 510.9 µs † | 751.7 µs † | 2.610 ms † | 8.316 ms † |
| gammainc_P | 2.3+0.125i, 1.4+0.125i | 1.133 ms † | 1.204 ms † | 2.122 ms † | 3.062 ms † | 9.068 ms † | 37.741 ms † |
| gammainc_Q | 2.3+0.125i, 1.4+0.125i | 832.7 µs † | 1.515 ms † | 2.468 ms † | 3.525 ms † | 9.067 ms † | 35.509 ms † |
| gammainc_lower | 2.3+0.125i, 1.4+0.125i | 517.0 µs † | 728.6 µs † | 1.281 ms † | 2.046 ms † | 7.098 ms † | 26.929 ms † |
| gammainc_upper | 2.3+0.125i, 1.4+0.125i | 862.8 µs † | 1.294 ms † | 2.129 ms † | 3.410 ms † | 8.991 ms † | 35.074 ms † |
| gammainv | 9.5+0.125i | 3.964 ms † | 4.168 ms † | 4.409 ms † | 6.602 ms † | 16.023 ms † | 39.573 ms † |
| hacovercos | 0.7+0.125i | 24.0 µs | 25.5 µs | 33.0 µs | 54.9 µs | 123.7 µs | 483.4 µs |
| hacoversin | 0.7+0.125i | 27.2 µs | 32.4 µs | 37.5 µs | 52.4 µs | 120.4 µs | 411.2 µs |
| harmonic_poly | 2, 0.7+0.125i | 21.7 µs | 21.0 µs | 23.8 µs | 25.3 µs | 35.4 µs | 80.0 µs |
| havercos | 0.7+0.125i | 25.6 µs | 22.8 µs | 36.0 µs | 48.4 µs | 151.9 µs | 407.4 µs |
| haversin | 0.7+0.125i | 34.3 µs | 23.6 µs | 32.2 µs | 54.6 µs | 138.6 µs | 427.0 µs |
| hermite_Hn | 2, 0.7+0.125i | 19.5 µs | 15.4 µs | 15.2 µs | 21.5 µs | 23.4 µs | 26.4 µs |
| hypergeometric_pFq | 0.7+0.125i, 2.5+0.125i, 0.3+0.125i | 348.4 µs † | 549.5 µs † | 912.0 µs † | 1.948 ms † | 3.876 ms † | 14.722 ms † |
| imag_part | 0.7+0.125i | 144.1 ns | 178.6 ns | 151.9 ns | 164.6 ns | 178.9 ns | 229.4 ns |
| inv | 0.7+0.125i | 1.3 µs | 2.1 µs | 2.6 µs | 2.9 µs | 4.8 µs | 14.9 µs |
| ldexp | 1.23456789+0.125i, 3 | 364.7 ns | 386.5 ns | 359.9 ns | 447.4 ns | 512.1 ns | 405.3 ns |
| legendre_chi | 2, 0.7+0.125i | 134.2 µs | 289.3 µs | 479.4 µs | 694.4 µs | 2.267 ms | 8.087 ms |
| lerch_phi | 0.3+0.125i, 2+0.125i, 1.4+0.125i | 542.0 µs | 10.0 µs | 10.7 µs | 13.8 µs | 11.0 µs | 12.1 µs |
| lerch_phi (integer order) | 0.3125+0.125i, -15, 0.375+0.125i | 45.4 µs | 47.8 µs | 47.9 µs | 47.8 µs | 45.6 µs | 58.0 µs |
| lerch_phi (negative order) | 0.3125+0.125i, -15+0.125i, 0.375+0.125i | 669.8 µs | 13.2 µs | 10.9 µs | 14.1 µs | 13.4 µs | 11.1 µs |
| lg | 2.345678+0.125i | 30.9 µs | 43.6 µs | 70.2 µs | 90.2 µs | 192.9 µs | 587.2 µs |
| lgamma | 2.345+0.125i | 190.1 µs † | 308.5 µs † | 510.7 µs † | 708.2 µs † | 2.116 ms † | 7.221 ms † |
| ln | 2.345678+0.125i | 18.9 µs | 34.5 µs | 41.0 µs | 58.5 µs | 145.1 µs | 430.5 µs |
| log | 2.345678+0.125i | 28.5 µs | 32.7 µs | 43.8 µs | 73.1 µs | 147.3 µs | 436.5 µs |
| log10 | 2.345678+0.125i | 34.4 µs | 50.1 µs | 61.3 µs | 81.6 µs | 260.2 µs | 643.4 µs |
| lommel_S | 0.5+0.125i, 0.7+0.125i, 1.2+0.125i | 290.6 µs | 618.4 µs | 816.7 µs | 1.685 ms | 6.437 ms | 19.330 ms |
| mul | 0.7+0.125i, 0.7+0.125i | 2.3 µs | 1.8 µs | 2.5 µs | 3.8 µs | 4.7 µs | 8.1 µs |
| mul_long | 17+0.125i, 3 | 5.4 µs | 4.5 µs | 4.5 µs | 4.5 µs | 4.4 µs | 4.7 µs |
| mul_pow10 | 1.23456789+0.125i, 3 | 4.0 µs | 3.2 µs | 4.2 µs | 3.5 µs | 4.7 µs | 3.4 µs |
| neg | 0.7+0.125i | 349.6 ns | 328.8 ns | 362.2 ns | 367.1 ns | 362.6 ns | 447.5 ns |
| normal_cdf | 0.7+0.125i | 97.8 µs | 236.6 µs | 409.1 µs | 705.4 µs | 2.153 ms | 10.193 ms |
| normal_logpdf | 0.7+0.125i | 10.0 µs | 13.1 µs | 19.4 µs | 28.9 µs | 49.7 µs | 144.4 µs |
| normal_pdf | 0.7+0.125i | 26.5 µs | 35.2 µs | 57.3 µs | 86.3 µs | 260.9 µs | 1.088 ms |
| polygamma | 3, 2.345+0.125i | 350.9 µs | 914.6 µs | 1.403 ms | 1.983 ms | 8.047 ms | 33.463 ms |
| polylog | 2, 0.7+0.125i | 238.1 µs | 435.3 µs | 871.0 µs | 1.629 ms | 6.082 ms | 28.369 ms |
| polylog1 | 0.7+0.125i | 28.9 µs | 45.2 µs | 55.0 µs | 77.8 µs | 191.9 µs | 474.5 µs |
| pow | 1.23456789+0.125i, 3.5+0.125i | 42.6 µs | 89.6 µs | 101.1 µs | 139.2 µs | 443.8 µs | 1.645 ms |
| pow10 | 3 | 541.8 ns | 613.1 ns | 832.6 ns | 766.5 ns | 600.0 ns | 706.7 ns |
| pow_int | 1.23456789+0.125i, 3 | 9.3 µs | 8.1 µs | 10.8 µs | 10.1 µs | 12.7 µs | 24.9 µs |
| real_part | 0.7+0.125i | 180.1 ns | 165.1 ns | 156.0 ns | 149.8 ns | 140.3 ns | 212.7 ns |
| rect | 0.7 | 3.0 µs | 2.7 µs | 2.9 µs | 3.8 µs | 5.5 µs | 11.0 µs |
| root | 1.2345+0.125i, 3 | 60.7 µs | 70.2 µs | 193.5 µs | 298.3 µs | 442.7 µs | 1.377 ms |
| round | 0.7+0.125i | 14.2 µs | 13.3 µs | 10.7 µs | 11.0 µs | 12.5 µs | 11.2 µs |
| roundn | 1.23456789+0.125i, 3 | 15.3 µs | 21.6 µs | 20.8 µs | 18.6 µs | 25.2 µs | 38.8 µs |
| rounds | 1.23456789+0.125i, 3 | 62.9 µs | 73.2 µs | 82.2 µs | 110.4 µs | 233.8 µs | 548.8 µs |
| sec | 0.7+0.125i | 19.2 µs | 19.6 µs | 27.3 µs | 40.8 µs | 129.6 µs | 394.7 µs |
| sech | 0.7+0.125i | 18.3 µs | 20.1 µs | 29.7 µs | 90.6 µs | 103.9 µs | 325.6 µs |
| sgn | 0.7 | 1.3 µs | 981.7 ns | 1.1 µs | 848.3 ns | 1.3 µs | 1.3 µs |
| sin | 0.567+0.125i | 21.2 µs | 16.4 µs | 27.8 µs | 41.1 µs | 106.6 µs | 378.4 µs |
| sinc | 0.7+0.125i | 19.8 µs | 28.3 µs | 37.8 µs | 54.6 µs | 137.5 µs | 424.4 µs |
| sinh | 0.7+0.125i | 12.3 µs | 14.9 µs | 25.5 µs | 97.6 µs | 85.3 µs | 282.9 µs |
| sqr | 0.7+0.125i | 2.1 µs | 2.0 µs | 2.6 µs | 2.9 µs | 4.1 µs | 9.3 µs |
| sqrt | 1.23456789+0.125i | 2.6 µs | 2.8 µs | 4.0 µs | 5.0 µs | 7.7 µs | 14.7 µs |
| step | 0.7 | 1.2 µs | 982.4 ns | 928.5 ns | 890.5 ns | 1.0 µs | 938.2 ns |
| struve_H | 2, 0.7+0.125i | 288.9 µs | 557.6 µs | 913.1 µs | 1.615 ms | 5.978 ms | 23.284 ms |
| struve_L | 2, 0.7+0.125i | 360.0 µs | 567.2 µs | 1.061 ms | 1.763 ms | 5.762 ms | 22.795 ms |
| sub | 0.7+0.125i, 0.7+0.125i | 447.8 ns | 371.9 ns | 432.3 ns | 481.2 ns | 516.4 ns | 631.5 ns |
| tan | 0.7+0.125i | 18.7 µs | 21.3 µs | 35.1 µs | 51.4 µs | 119.6 µs | 417.3 µs |
| tanh | 0.7+0.125i | 27.0 µs | 30.0 µs | 74.4 µs | 39.3 µs | 98.9 µs | 367.2 µs |
| tetragamma | 2.345+0.125i | 413.6 µs | 858.7 µs | 1.579 ms | 2.172 ms | 7.866 ms | 39.931 ms |
| tri | 0.7 | 3.3 µs | 2.6 µs | 3.4 µs | 3.4 µs | 4.8 µs | 9.3 µs |
| trigamma | 2.345+0.125i | 311.4 µs | 690.8 µs | 1.205 ms | 2.114 ms | 5.914 ms | 29.023 ms |
| vercos | 0.7+0.125i | 37.9 µs † | 18.3 µs | 27.9 µs | 59.1 µs † | 110.7 µs | 460.5 µs |
| versin | 0.7+0.125i | 26.6 µs | 21.1 µs | 37.2 µs | 56.0 µs | 129.6 µs | 420.9 µs |
| zatahp | 2.3+0.125i, 1.4+0.125i | 2.626 ms | 5.274 ms | 9.506 ms | 1.198 ms | 3.110 ms | 12.691 ms |
| zatahp (integer order) | -4, 0.75+0.125i | 2.203 ms | 4.975 ms | 10.554 ms | 779.8 µs | 3.574 ms | 15.856 ms |
| zeta | 2.345+0.125i | 975.1 µs | 1.471 ms | 2.731 ms | 4.335 ms | 19.315 ms | 116.961 ms |
| zeta_pair | 2.345+0.125i | 948.9 µs | 1.794 ms | 3.305 ms | 5.616 ms | 21.954 ms | 125.945 ms |
| zetah | 2.3+0.125i, 1.4+0.125i | 2.174 ms | 5.037 ms | 8.790 ms | 718.8 µs | 3.198 ms | 14.239 ms |
| zetap | 2.345+0.125i | 708.1 µs | 1.503 ms | 3.298 ms | 6.035 ms | 23.591 ms | 131.738 ms |

### number / unrestricted integer

| Operation | Inputs | Exact |
|---|---|---:|
| abs | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">-9712853409152031141207811585727780469484705121206232579607434236969009533618825<br/>97201918971499157056640605432269691427807258223374967278846596194834524005116336<br/>17514882293623272276764857474830401626415841035971458657442133370777607803464071<br/>101158385124186229995073045427448918227959893082282919863982307657776</span> | 109.5 ns |
| add | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 107.2 ns |
| bit_and | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 240.4 ns |
| bit_length | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 164.0 ns |
| bit_not | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 259.3 ns |
| bit_or | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 206.1 ns |
| bit_xor | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 202.5 ns |
| clear_bit | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>701</span> | 406.8 ns |
| div | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>340282366920938463463374607431768211454</span> | 1.2 µs |
| divmod | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 335.4 ns |
| factorial | 5000 | 126.1 µs |
| factors | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">54772119019154636339867862041163448790974484131459861520687327809209914339836012<br/>52275954799541486775711126005967382600118182797233708133775335788335658037065081<br/>31569062299976033041302189833215742992653063887339218309467131548996199881113600<br/>00000000000000000000000000000000000000000000000000000000000000000000000000000000<br/>0000000000000000000000000000000000000000000000</span> | 134.9 µs |
| fibonacci | 10000 | 6.6 µs |
| gcd | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 3.0 µs |
| gcdext | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 3.7 µs |
| is_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 521.4 µs |
| isqrt | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 417.0 ns |
| lcm | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 4.6 µs |
| mod | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 339.1 ns |
| modinv | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 4.0 µs |
| mul | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 219.9 ns |
| neg | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 62.4 ns |
| next_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 5.274 ms |
| partition | 5000 | 7.074 ms |
| pow | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>5</span> | 3.8 µs |
| pow_int | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>5</span> | 2.9 µs |
| powmod | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662, <br/>68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 144.7 µs |
| prev_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 6.317 ms |
| prove_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 4.039 ms |
| set_bit | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>700</span> | 301.1 ns |
| shl | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>257</span> | 438.1 ns |
| shr | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>257</span> | 313.7 ns |
| sqr | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 285.3 ns |
| sub | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 108.1 ns |
| test_bit | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>701</span> | 325.1 ns |

### number / unrestricted fraction

`number_t` uses exact GMP rational arithmetic (MPQ). The operands below have large integer
numerators and denominators; parsing and canonical reduction precede timing. Every result
is checked to remain an exact fraction. Arithmetic cost depends on operand sizes and
cancellation, rather than a floating-point precision. These workloads use a 16384-bit
rational size budget so intermediate results remain exact; general transcendental
functions usually require inexact real or complex results.

| Operation | Inputs | Exact |
|---|---|---:|
| abs | $\frac{\begin{array}{l}\text{-9712853409152031141207811585727780469484705121206232579607434236969009533618825} \\ \text{97201918971499157056640605432269691427807258223374967278846596194834524005116336} \\ \text{17514882293623272276764857474830401626415841035971458657442133370777607803464071} \\ \text{101158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$ | 220.1 ns |
| add | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>$\frac{\begin{array}{l}\text{12498846048367045926370716602609982839721641429222959125376462582674869403443996} \\ \text{972648628699047686839582654905384579341977328653591547736951147466435189662}\end{array}}{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}$ | 4.1 µs |
| div | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>$\frac{\begin{array}{l}\text{12498846048367045926370716602609982839721641429222959125376462582674869403443996} \\ \text{972648628699047686839582654905384579341977328653591547736951147466435189662}\end{array}}{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}$ | 8.7 µs |
| inv | $\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$ | 155.4 ns |
| mul | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>$\frac{\begin{array}{l}\text{12498846048367045926370716602609982839721641429222959125376462582674869403443996} \\ \text{972648628699047686839582654905384579341977328653591547736951147466435189662}\end{array}}{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}$ | 3.3 µs |
| neg | $\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$ | 144.0 ns |
| pow | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>5 | 40.4 µs |
| pow_int | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>5 | 51.2 µs |
| sqr | $\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$ | 615.3 ns |
| sub | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>$\frac{\begin{array}{l}\text{12498846048367045926370716602609982839721641429222959125376462582674869403443996} \\ \text{972648628699047686839582654905384579341977328653591547736951147466435189662}\end{array}}{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}$ | 4.8 µs |

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

Converts a real value to a machine double, including a value stored in any complex
backend whose imaginary component is exactly zero. A nonzero imaginary component
or invalid value returns `NAN`. Conversion rounds to double precision and may
overflow outside its range; it preserves the input and default precision.

```c
double num_to_double(const number_t number);
```

### `num_to_qfloat()`

Converts a real value to a double-double scalar, including real-valued complex
storage. Non-real or invalid values return `QF_NAN`. The input and default precision
are preserved; values outside the scalar's range may overflow.

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
