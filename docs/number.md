# `number_t`

`number_t` is MARS's generic numeric value cluster.

## Implementation organisation

Related algorithms are grouped by function family, with common numerical kernels shared where needed:

- `src/number/number_bessel.c` contains the principal Bessel K and Y algorithms.
  `number_cylindrical_series.c` contains Bessel I and Struve H/L,
  their shared guarded series and cylindrical precision selection. Integral and half-integral
  normalisation uses factorial identities; other complex orders share the
  number module's guarded logarithmic-gamma implementation.
- `src/number/number_special.c` contains Chebyshev and Hermite recurrences
  together with the scalar signal functions. Both groups use the existing
  generic numeric arithmetic and precision policy.
- `src/number/number_appell.c` contains all four Appell functions, including F₁'s
  linear total-degree recurrence and the guarded F₂–F₄ recurrences and finite polynomials.
- `src/number/number_gamma.c` groups guarded complex gamma and logarithmic gamma,
  staged inverse-gamma refinement, and checked binary splitting for the lower incomplete-gamma series.
- `src/number/number_polygamma.c` selects bounded coefficient precision for asymptotic
  digamma and polygamma corrections and supplies guarded reciprocal-power blocks for finite recurrences.
- `src/number/number_lerch.c` groups real defining sums, weighted binomial blocks and
  finite rational evaluations at non-positive integer orders. Real sums stay in MPFR arithmetic;
  complex kernels retain their guarded MPC arithmetic and bounded result reuse.
- `src/number/number_zeta.c` groups real and complex Euler–Maclaurin tails, finite Dirichlet
  sums, shifted-sum blocks and bounded shifted-logarithm reuse. Values and order derivatives share
  their existing convergence bounds, coefficient budgets and cache ownership.
- `src/number/number_coefficients.c` owns bounded coefficient reuse;
  `number_complex_bounds.c` supplies conservative convergence bounds.
  `number_chi.c` contains direct Legendre chi summation.
- `src/number/number_series_power.c` supplies checked exact machine-word
  denominators for polylogarithm and Legendre chi series, with arbitrary-precision
  fallback when the integer power does not fit.
- `src/number/number_polylog_blocks.c` supplies guarded power blocks for real
  and complex integer polylogarithms.

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
Borrowed shared mathematical constants also grow to the current working precision
when it increases. Explicitly materialised, owned snapshots retain their chosen precision.

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
arguments, pulse functions, Bessel K/Y and exact negative-constant shortcuts;
their internal cleanup must neither free the returned result nor bypass the
caller's automatic cleanup.

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
| `num_lambert_S(a, q, z, r, s, N)` | MARS-defined generalised Lambert sum; all six arguments are `number_t`. See the definition, domains and calculus in [the expression guide](expression.md#mars-defined-lambert-sum). Finite sections and convergent infinite sums preserve the selected real or complex precision. |
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
| `num_appell_F2(a, b1, b2, c1, c2, x, y)` | Appell F₂, with one shared upper parameter and two separate lower parameters. |
| `num_appell_F3(a1, a2, b1, b2, c, x, y)` | Appell F₃, with two separate upper pairs and one shared lower parameter. |
| `num_appell_F4(a, b, c1, c2, x, y)` | Appell F₄, with two shared upper parameters and two separate lower parameters. |
| `num_lauricella_F(a, b, c, x, count)` | Evaluates Lauricella `F_D` for the paired parameter and argument arrays. |
| `num_hypergeometric_pFq(upper, p, lower, q, x)` | Evaluates the generalised hypergeometric function with `p` upper and `q` lower parameters. |

#### Appell functions F₂, F₃ and F₄

The parameter conventions follow the [NIST definitions](https://dlmf.nist.gov/16.13).
The real and complex evaluators support the defining series: `|x| + |y| < 1` for F₂,
`max(|x|, |y|) < 1` for F₃, and `sqrt(|x|) + sqrt(|y|) < 1` for F₄.
Finite terminating polynomials can also be evaluated outside these regions. Lower parameters at
non-positive integer poles are rejected, including apparent upper/lower cancellations.

Exact integer and rational parameters retain their values when converted to the working precision.
Multiprecision evaluation uses guarded total-degree recurrences, with an independent tail bound;
terminating, resonant or unstable auxiliary recurrences use direct triangular summation.
Fixed qfloat and qcomplex inputs stay in the native double-double implementation.
Separate-axis coefficient ratios are calculated once per index and reused across total-degree rows.
The fixed evaluator bounds each term's modulus by the sum of its component magnitudes for the
cancellation check, avoiding a square root per term.
Unsupported continuation, non-finite arguments, excessive parameter bounds, severe fixed-precision
cancellation or exhausted work budgets return NaN. Multiprecision work is limited to 32768 bits,
20000 total degrees and two million direct-series cells; fixed evaluation has smaller bounded budgets.

The following runnable example covers the corresponding [qfloat](qfloat.md), [qcomplex](qcomplex.md),
[expression](expression.md) and [matrix](matrix.md) APIs as well.

```c
#include <stdio.h>
#include "expression-t.h"
#include "matrix-t.h"
#include "number-t.h"
#include "qcomplex-t.h"
#include "qfloat-t.h"

int main(void)
{
    NUM_SCOPE(scope);
    qfloat_t fixed = qf_appell_F2(QF_ONE, QF_ONE, QF_ONE, QF_ONE, QF_ONE,
                                 qf_from_double(0.125), qf_from_double(0.25));
    qcomplex_t minus_one = qc_make(qf_from_double(-1), QF_ZERO);
    qcomplex_t complex_value = qc_appell_F3(minus_one, minus_one, QC_ONE, QC_ONE, QC_ONE,
                                     qc_make(qf_from_double(0.125), QF_ZERO),
                                     qc_make(qf_from_double(0.25), QF_ZERO));
    number_t polynomial = num_appell_F4(NUM_NEG_ONE, NUM_ONE, NUM_ONE, NUM_ONE, NUM_TWO,
                                        num_create_from_long(3));
    expr_t *expression = expr_from_string("appell_F2(1,1,1,1,1,1/8,1/4)", NULL);
    matrix_t *x = mat_from_string("(0,1/16;0,0)");
    matrix_t *y = mat_from_string("(0,1/32;0,0)");
    matrix_t *matrix = mat_appell_F2(NUM_ONE, NUM_ONE, NUM_ONE, NUM_ONE, NUM_ONE, x, y);
    if (!expression || !matrix) {
        mat_free(matrix); mat_free(y); mat_free(x); expr_free(expression);
        return 1;
    }
    printf("qfloat F2 = %.6f\n", qf_to_double(fixed));
    printf("qcomplex F3 = %.6f\n", qf_to_double(qc_real(complex_value)));
    printf("number F4 = %.6f\n", num_to_double(polynomial));
    printf("expression F2 = %.6f\n", num_to_double(expr_eval(expression)));
    printf("matrix F2[0,1] = %.6f\n", num_to_double(mat_get_num(matrix, 0, 1)));
    mat_free(matrix); mat_free(y); mat_free(x); expr_free(expression);
    return 0;
}
```

Output:

```text
qfloat F2 = 1.600000
qcomplex F3 = 0.640625
number F4 = -4.000000
expression F2 = 1.600000
matrix F2[0,1] = 0.093750
```

Fixed-precision timings below are seven-sample medians from benchmark runs **223** (cold)
and **224** (changing), at **106 bits**. Operand parsing, result checks and disposal are
outside the timer. The argument tuples are stored in the benchmark database; complex profiles
add the catalogue's imaginary components. These are current timings, separate from the
earlier full-catalogue tables further below.

| Function | qfloat cold | qcomplex cold | qfloat changing | qcomplex changing |
| --- | ---: | ---: | ---: | ---: |
| `appell_F2` | 1.105 ms | 2.367 ms | 849.4 µs | 1.634 ms |
| `appell_F3` | 1.819 ms | 2.175 ms | 1.169 ms | 1.385 ms |
| `appell_F4` | 394.0 µs | 3.878 ms | 786.5 µs | 4.922 ms |

#### Incomplete gamma precision

For finite real `a > 0` and `0 <= x <= a + 1`, the multiprecision
implementations of `num_gammainc_lower(a, x)` and `num_gammainc_P(a, x)`
evaluate the lower tail directly. They use the positive series with initial
term `1` and recurrence `term[n] = term[n-1] * x / (a + n)`.
The sum is scaled by `exp(a*log(x) - x)/a` for the unnormalised lower gamma,
or by `exp(a*log(x) - x - log(Gamma(a+1)))` for P. Logarithmic scaling avoids
unnecessarily overflowing intermediate powers or complete gamma values.
Both lower-tail functions return zero at `x = 0` in this domain.

At higher precision, a checked binary-splitting attempt forms a common
denominator for consecutive Pochhammer factors. It reuses powers of the argument
and avoids a complex division at each term. This attempt covers orders with
real part in `[0.125,16]`, imaginary part in `[-1,1]`, and argument modulus at
most eight, with at most 8192 terms. Directed absolute-sum majorants and a
geometric tail must pass before the result is accepted; other requests retain
the ordinary recurrence. Both real and complex incomplete-gamma callers use
this kernel within their existing direct-series regions.

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

Bounded positive parameters have additional native evaluation paths. For `1F1`,
ratio polynomials advance by addition between 64-step refreshes; rationalising
a complex lower parameter leaves a real denominator. Late terms may use shorter
significands only while an explicitly propagated absolute error stays within
the final budget. Other small parameter sets can use a balanced product tree,
with a single final division and independent geometric-tail and roundoff checks.
The product tree starts at 1024 working bits when any input is complex and 3072
for wholly real inputs, retaining the faster recurrence below those crossovers.
These paths require parameter real parts in `[1/8,16]`, imaginary magnitudes
at most one, and argument modulus at most eight. The tree admits up to eight
parameters and restricts `p = q + 1` arguments to modulus at most `5/8`;
both kernels use at most 8192 terms and 16384 working bits.
Both accelerated paths use local storage and fall back to the ordinary series
outside their parameter, precision or convergence bounds. Bounded `0F0` and
`1F0` inputs use their exponential and binomial identities instead of summation;
terminating polynomials and denominator poles retain their original handling.

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
bound determining the required terms. Appell F₁ also recognises the exact reduction
`F1(a,b1,b2;a;x,y) = (1-x)^(-b1)*(1-y)^(-b2)` before summing its general recurrence.

#### Clausen functions

`num_clausen(order, theta)` takes a positive integer order and an angle in
radians; `num_clausen2(theta)` is exactly `num_clausen(2, theta)`.
Moderate complex order-two arguments use native dilogarithm identities and
the known logarithm of `exp(i*theta)` in the
reduced strip, avoiding an unnecessarily expensive correctly rounded logarithm
near the unit circle. Real angles and other orders retain their coefficient
series. Even integer-zeta coefficients share the bounded Bernoulli prefix,
while distant isolated requests keep direct evaluation. At 1024 bits and above,
Clausen can prepare a known prefix of 32 through 1024 even-zeta coefficients
using the exact positive tangent-number recurrence described by
[Brent and Harvey](https://maths-people.anu.edu.au/brent/pd/rpb242.pdf).
Guarded conversion also supplies normalised Bernoulli coefficients, and successive
powers replace independent exponentiation for every zeta value. A directed
logarithm estimates the useful prefix length; the series retains its existing
convergence checks. The shared eight MiB cache bound, precision upgrades and
ordinary fallbacks remain in force. Higher-precision entries are preserved,
and small prefix extensions use the ordinary coefficient evaluator.
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

For bounded positive-real-part denominator parameters, high-precision forward
sums use a product tree with one final division. The tree works directly
with the two denominator factors of each Bessel or Struve recurrence.
Directed tail and accumulated roundoff bounds must pass before a sum
is accepted; the ordinary recurrence remains the fallback. Non-negative
integer orders also use integer powers and factorial identities for the
normalisation, avoiding logarithms and exponentials without replacing
division by an approximate reciprocal.

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

Bounded complex Lommel parameters share the cylinder product tree after
factoring the quadratic series denominator into two rising factorials.
Argument derivatives use the same tree with linear term weights and one
final division by the argument. The tail bound includes these weights,
including a vanishing leading derivative term. Unsupported tree parameters
retain the original recurrence, and real Lommel retains its existing real
series, which was faster in the measured workloads.

The complex error functions use an entire Kummer series with additional
precision for exponential cancellation. At high precision and squared argument
modulus at most eight, they share the guarded lower-gamma binary split with
order one-half. This evaluates the same series with one final division rather
than a complex division for every term. The split checks its tail and roundoff;
the error-function caller also checks cancellation after the exponential
prefactor, carrying the split's error allowance through that check. Arguments
outside the split's working limits retain the ordinary recurrence.
Purely imaginary inputs also retain that recurrence below 1536 working bits,
where their real series terms are cheaper than constructing the split tree.
Shared guarded products use two real multiplications when one operand is real,
retaining either input's permitted aliasing. The supported complex magnitude is
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
complex division. When `a` has positive real part, `c-a` is a positive integer
at most eight and both parameters have short binary significands, the ratio instead uses a finite
rising-factorial quotient whose polynomials remain exact. Guarded complex
products use three real multiplications inside the existing absolute roundoff
allowance. Terminating numerators, lower-parameter poles, the existing
`|x|, |y| < 0.95` domain and cancellation checks remain supported.

For those short rising-factorial quotients, both MPFR and MPC kernels can reduce
the precision of late coefficient states at 1024–8320 working bits. The checked
region requires `|x|, |y| <= 7/16`, `|b1| + |b2| <= 8` and
`|x+y| + |x*y| <= 7/8`, using conservative magnitude bounds. Initial terms,
recurrence factors and final accumulators keep their full precision. Directed
absolute errors advance through the two-state recurrence, including input
rounding and the remaining homogeneous error tail. The positive-real-part
rising-factorial quotient has modulus at most one, so these errors also bound
the weighted sum. A failed final error check retries the full-precision
recurrence before writing an aliased output; other parameters retain their
existing evaluator. No result cache is added.

Appell F₁, complex error functions, complex polylogarithms and integer-order Bessel K use directed
upper and lower magnitude bounds for convergence checks where a square root
is unnecessary. These conservative bounds leave the mathematical modulus and
the returned function definitions unchanged.

Appell, Legendre chi and polylogarithm series compare binary exponents of their
directed bounds before accepting a small term. This avoids repeated products of
full-width significands. Chi's exponent test includes an upper bound for its
geometric tail factor. An inconclusive comparison continues summation, so this
shortcut can require an extra term rather than accepting one prematurely.
Integer polylogarithms of orders two through 32 attempt rectangular splitting
at 384 through 16384 result bits when the transformed argument has modulus at
most 0.9. Short powers are reused within each block; late blocks use fewer
relative bits under an explicit absolute roundoff allowance. Directed geometric
tails and cancellation checks validate each result. Real dilogarithms use the
same native block evaluator and retain MPFR outside this region. The complex
continuation chooses a smaller reflected argument before inversion when
available, avoiding repeated reciprocal transformations near the unit circle.

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
At 640–32768 working bits, positive-real-part complex recurrence sums for orders
zero through three use sixteen-factor rising polynomials. Exact integer coefficients
and reusable argument powers provide normalised derivatives; Newton's identities
recover the reciprocal power sums with one reciprocal per block. The 128 working
guard bits cover polynomial and low-order identity cancellation. Real operands
share the zero-imaginary kernel from 3072 output bits, preserving cheaper ordinary
real division below that threshold. Other orders, precisions and left-half-plane
arguments retain individual recurrence shifts. For supported low orders at
512–8192 complex output bits or 3072–8192 real output bits, the cheaper blocks
permit a larger shift target, reducing the required Bernoulli prefix without
excessive work on changing inputs.

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
For guarded polygamma work in 1024–8320 bits, a directed factorial/power bound
estimates the useful prefix length. Known prefixes of at most 1024 ordinary
Bernoulli coefficients can be prepared from exact positive tangent numbers,
without calculating unused zeta values, factorials or powers of pi. Existing
higher-precision entries remain valid; unsupported preparation requests retain
ordinary coefficient lookup.

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
historical full-catalogue run **140**, with no CPU affinity restriction.
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

### Updated cold and changing-input timings at 4096 bits

Measured on 17 November 2026 on the Intel N95, using seven samples per case and
the cold and changing-input protocols described above, with cold run **205**
and changing-input run **206**. Operands follow the
full catalogue below, including its stored masks for complex components.
These measurements use the accepted evaluation paths, including the real
Clausen series with exact preparation of useful coefficient prefixes,
Appell F₁'s checked late-state precision reduction, and polygamma's guarded
polynomial shifts and exact ordinary Bernoulli preparation.

| Operation | Real cold | Complex cold | Real changing | Complex changing |
|---|---:|---:|---:|---:|
| `clausen` (order 3) | 75.755 ms | 96.183 ms | 4.970 ms | 26.916 ms |
| `clausen2` | 80.418 ms † | 13.052 ms | 4.935 ms | 13.049 ms |
| `appell_F1` | 12.737 ms | 38.355 ms | 12.722 ms | 43.789 ms |
| `gammainc_P` | 52.017 ms | 100.929 ms | 11.233 ms | 26.492 ms |
| `struve_H` | 6.436 ms | 10.547 ms | 3.553 ms | 8.564 ms |
| `bessel_I` | 4.894 ms | 10.537 ms | 3.810 ms | 8.380 ms |
| `dilog` | 5.582 ms | 7.290 ms | 5.076 ms | 5.036 ms |
| `digamma` | 64.645 ms | 37.352 ms | 21.662 ms | 12.892 ms |
| `trigamma` | 27.331 ms | 40.004 ms | 6.030 ms | 16.399 ms |
| `tetragamma` | 27.596 ms | 42.702 ms | 6.629 ms | 21.525 ms |
| `polygamma` (order 3) | 29.697 ms | 45.311 ms | 7.792 ms | 26.993 ms |
| `gammainv` | 65.729 ms | 114.959 ms | 19.479 ms | 28.234 ms |

† retains the recorded slowdown flag against an intermediate dilogarithm-based
variant that reduced real cold cost at the expense of changing-input speed.
The accepted Clausen selection preserves the faster changing-input series;
this flagged cold timing remains excluded from baseline statistics.
No threshold or baseline was reset.

Incomplete gamma's first-call cost still includes complete-gamma normalisation;
binary splitting improves its lower-tail series, particularly for changing
inputs. The full catalogue below publishes separate warmed measurements at
every configured precision.

### Updated complex error-function timings at 4096 bits

Measured on 17 November 2026 on the Intel N95, with seven samples per case,
using cold run **234** and changing-input run **235**. These are complex
4096-bit calls; the shared Kummer binary split and real-operand product path
retain the existing convergence and cancellation checks. Inputs are
`0.7 + 0.125i` for `erf`, `erfc` and `normal_cdf`, and `0.5 + 0.125i` for
the inverses. The changing protocol varies their real part as described above.

| Operation | Complex cold | Complex changing |
|---|---:|---:|
| `erf` | 6.080 ms | 4.565 ms |
| `erfc` | 5.977 ms | 4.468 ms |
| `erfinv` | 19.471 ms | 16.870 ms |
| `erfcinv` | 21.173 ms | 15.958 ms |
| `normal_cdf` | 4.901 ms | 4.573 ms |

The complex rows above carry no slowdown flags. They measure local computation, with
argument preparation and disposal outside the timer, and introduce no
input-result cache. Existing mathematical identities and independent Maclaurin
references check accuracy separately from these timing measurements.

### Updated hypergeometric timings at 4096 bits

Measured on 17 November 2026 on the Intel N95, with seven samples per case,
using cold run **242** and changing-input run **243**. The catalogue workload
calls `num_hypergeometric_pFq` with `p = q = 1`: upper parameter `0.7`, lower
parameter `2.5` and argument `0.3`. Its complex profile adds `0.125i` to all
three inputs; the changing protocol varies their real parts as described above.

| Input type | Cold | Changing |
|---|---:|---:|
| Real | 2.817 ms | 3.149 ms |
| Complex | 6.214 ms | 5.901 ms |

These measurements exercise the additive `1F1` ratio recurrence and its checked
late-term precision. They use local scratch storage and no input-result cache.
All four observations passed without a slowdown flag. Independent full-width
series references also cover `0F0`, `0F1`, `1F0`, `2F1`, `2F2` and `3F2`,
including poles, cancellation and fallbacks outside the accelerated bounds.

### Updated cylindrical and Lommel timings at 4096 bits

Measured on 17 November 2026 on the Intel N95, with seven samples per case,
using cold run **250** and changing-input run **251**. Bessel and Struve
operands are order `2` and argument `0.7`; the complex profile adds `0.125i`
to the argument. Lommel operands are `0.5, 0.7, 1.2`, with `0.125i` added to
all three operands in the complex profile. Changing inputs follow the
protocol above.

| Operation | Real cold | Complex cold | Real changing | Complex changing |
|---|---:|---:|---:|---:|
| `bessel_I` | 2.068 ms | 5.796 ms | 1.929 ms | 4.772 ms |
| `bessel_J` | 1.085 ms | 5.328 ms | 1.767 ms † | 5.082 ms |
| `struve_H` | 3.038 ms | 5.234 ms | 1.802 ms | 4.762 ms |
| `struve_L` | 2.120 ms | 4.966 ms | 1.857 ms | 4.608 ms |
| `lommel_S` | 4.554 ms | 15.473 ms | 3.359 ms | 16.872 ms |

The accelerated paths share the checked two-factor product tree and use
integer-order factorial normalisation where applicable. Lommel argument
derivatives use a weighted tree. Real Bessel J and real Lommel retain their
existing evaluation paths; their rows are controls rather than claimed
algorithmic improvements. No input-result cache is introduced.

† The unchanged real Bessel J control retained a slowdown flag in run 251;
its cold measurement passed without a flag. An isolated changing-input repeat
in run **252** measured **0.908 ms**, also without a flag. The original observation
remains stored and excluded from baseline statistics. The other 19 observations carry no
slowdown flags. Independent full-width series references cover both precision
crossovers, complex orders, negative-axis branches, derivative weights and
fallbacks; accepted and rejected attempts also check output aliases.

### MARS-defined Lambert sum timings

Focused benchmark run **254**, measured on 22 November 2026 on the Intel N95,
records three warm samples per case. These are timings for the explicitly
[defined Lambert sum](expression.md#mars-defined-lambert-sum), not for Lambert W.
The infinite workload uses `(a,q,z,r,s,N) = (0.5,0.25,1.25,1,-1,inf)`;
the finite workload uses `(-1,0.7,1,1,-1,1000)`. Complex profiles add `0.125i`
to each of the first three arguments. All 30 supported cases passed, without
timing-outlier flags; all precision profiles remain in the benchmark database.

| Workload | Real, 106 bits | Complex, 106 bits | Real, 4096 bits | Complex, 4096 bits |
|---|---:|---:|---:|---:|
| `lambert_S` (infinite) | 65.2 µs | 92.4 µs | 29.019 ms | 303.228 ms |
| `lambert_S` (finite section) | 238.9 µs | 332.7 µs | 20.452 ms | 121.330 ms |

### Full catalogue, warm protocol

Retained slowdown outliers are marked below and excluded from baseline statistics; they are not accepted new
performance baselines. Use matched runs when attributing a timing change to an
implementation change.

Run 207 retains 57 timing flags. Focused repeats in runs 208–219 reproduced
54 of those cases without a flag. MPFR `asin` at 256 bits, `hacoversin` at
768 bits and `zatahp` at 1024 bits retained warnings; separate before/after
library controls showed timing variation and partly overlapping ranges.
The original observations and flags remain recorded, without resetting their
baselines or replacing the full-catalogue measurements below.

<!-- MARS-BENCHMARKS:BEGIN -->
Benchmark run **207**, 2026-11-17T18:12:25.964Z (UTC).

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
| E1 | 2.345678 | 8.9 µs |
| Ei | 2.345678 | 7.9 µs |
| Li | 2.345 | 7.2 µs |
| abs | 0.7 | 9.0 ns |
| acos | 0.7 | 19.3 ns |
| acosec | 2.345 | 17.8 ns |
| acosech | 0.7 | 50.6 ns |
| acosh | 2.345 | 33.4 ns |
| acot | 0.7 | 45.3 ns |
| acoth | 2.345 | 30.2 ns |
| add | 0.7, 0.7 | 15.5 ns |
| add_long | 17, 3 | 518.4 ns |
| appell_F1 | 0.5, 0.7, 0.8, 2.5, 0.2, 0.3 | 28.9 µs |
| arccovercos | 0.5 | 119.5 ns |
| arccoversin | 0.5 | 117.0 ns |
| archacovercos | 0.5 | 253.1 ns |
| archacoversin | 0.5 | 256.1 ns |
| archavercos | 0.5 | 244.5 ns |
| archaversin | 0.5 | 216.3 ns |
| arcvercos | 0.5 | 117.1 ns |
| arcversin | 0.5 | 109.6 ns |
| arg | 0.7 | 76.0 ns |
| asec | 2.345 | 18.0 ns |
| asech | 0.7 | 36.7 ns |
| asin | 0.7 | 19.1 ns |
| asinh | 0.7 | 52.0 ns |
| atan | 0.7 | 32.1 ns |
| atan2 | 0.5, -0.75 | 43.1 ns |
| atanh | 0.7 | 26.1 ns |
| bessel_I | 2, 0.7 | 22.5 µs |
| bessel_J | 2, 0.7 | 6.4 µs |
| bessel_K | 2, 0.7 | 21.7 µs |
| bessel_Y | 2, 0.7 | 20.3 µs |
| beta | 2.3, 4.5 | 134.9 ns |
| beta_pdf | 0.5, 2.5, 3.5 | 172.3 ns |
| binomial | 5, 2 | 185.3 ns |
| ceil | 0.7 | 116.5 ns |
| chebyshev_Tn | 2, 0.7 | 784.0 ns |
| chebyshev_Un | 2, 0.7 | 1.3 µs |
| circ | 0.7 | 292.9 ns |
| clausen | 3, 0.7 | 6.9 µs |
| clausen2 | 0.7 | 7.9 µs |
| conj | 0.7 | 9.0 ns |
| cos | 0.7 | 39.7 ns |
| cosec | 0.7 | 33.2 ns |
| cosech | 0.7 | 56.5 ns |
| cosh | 0.7 | 41.2 ns |
| cot | 0.7 | 42.9 ns |
| coth | 0.7 | 66.3 ns |
| covercos | 0.7 | 120.0 ns |
| coversin | 0.7 | 121.8 ns |
| cubrt | 0.7 | 7.4 µs |
| digamma | 2.345 | 2.8 µs |
| dilog | 0.7 | 15.3 µs |
| div | 0.7, 0.7 | 12.6 ns |
| erf | 0.7 | 77.0 ns |
| erfc | 0.7 | 74.1 ns |
| erfcinv | 0.5 | 318.2 ns |
| erfinv | 0.5 | 322.8 ns |
| exp | 1.23456789 | 37.9 ns |
| floor | 0.7 | 9.2 ns |
| gamma | 2.345 | 168.1 ns |
| gammainc_P | 2.3, 1.4 | 12.9 µs |
| gammainc_Q | 2.3, 1.4 | 14.6 µs |
| gammainc_lower | 2.3, 1.4 | 15.3 µs |
| gammainc_upper | 2.3, 1.4 | 15.1 µs |
| gammainv | 9.5 | 101.8 µs |
| hacovercos | 0.7 | 232.7 ns |
| hacoversin | 0.7 | 233.9 ns |
| harmonic_poly | 2, 0.7 | 1.8 µs |
| havercos | 0.7 | 231.2 ns |
| haversin | 0.7 | 250.3 ns |
| hermite_Hn | 2, 0.7 | 1.7 µs |
| hypergeometric_pFq | 0.7, 2.5, 0.3 | 12.5 µs |
| hypot | 0.7, 1.2 | 24.2 ns |
| imag_part | 0.7 | 29.7 ns |
| inv | 0.7 | 7.9 ns |
| lambert_W0 | 0.7 | 17.3 µs |
| lambert_Wm1 | -0.2 | 15.1 µs |
| lambert_Wn | 1, 0.7 | 28.4 µs |
| lauricella_F | 0.5, 0.3, 2.5 | 17.9 µs |
| ldexp | 1.23456789, 3 | 42.4 ns |
| legendre_chi | 2, 0.7 | 28.0 µs |
| lerch_phi | 0.3, 2, 1.4 | 45.0 µs |
| lerch_phi (integer order) | 0.3125, -15, 0.375 | 27.2 µs |
| lerch_phi (negative order) | 0.3125, -15, 0.375 | 25.7 µs |
| lg | 2.345678 | 22.9 ns |
| lgamma | 2.345 | 56.4 ns |
| ln | 2.345678 | 15.6 ns |
| log | 2.345678 | 14.3 ns |
| log10 | 2.345678 | 22.7 ns |
| logbeta | 2.3, 4.5 | 144.6 ns |
| logbeta_pdf | 0.5, 2.5, 3.5 | 154.3 ns |
| lommel_S | 0.5, 0.7, 1.2 | 7.3 µs |
| mul | 0.7, 0.7 | 16.0 ns |
| mul_long | 17, 3 | 583.7 ns |
| mul_pow10 | 1.23456789, 3 | 69.5 ns |
| neg | 0.7 | 9.0 ns |
| normal_cdf | 0.7 | 75.5 ns |
| normal_logpdf | 0.7 | 16.5 ns |
| normal_pdf | 0.7 | 25.7 ns |
| polygamma | 3, 2.345 | 30.2 µs |
| polylog | 2, 0.7 | 17.6 µs |
| polylog1 | 0.7 | 132.2 ns |
| pow | 1.23456789, 3.5 | 37.4 ns |
| pow10 | 3 | 182.4 ns |
| pow_int | 1.23456789, 3 | 63.7 ns |
| productlog | 0.7 | 16.9 µs |
| qdigamma | 0.5, 2.3 | 24.9 µs |
| real_part | 0.7 | 27.6 ns |
| rect | 0.7 | 284.2 ns |
| root | 1.2345, 3 | 9.3 µs |
| round | 0.7 | 429.8 ns |
| roundn | 1.23456789, 3 | 5.0 µs |
| rounds | 1.23456789, 3 | 6.5 µs |
| sec | 0.7 | 43.5 ns |
| sech | 0.7 | 35.5 ns |
| sgn | 0.7 | 140.0 ns |
| sin | 0.567 | 32.9 ns |
| sinc | 0.7 | 3.4 µs |
| sincos | 0.7 | 660.0 ns |
| sinh | 0.7 | 58.2 ns |
| sinhcosh | 0.7 | 121.2 ns |
| sqr | 0.7 | 9.4 ns |
| sqrt | 1.23456789 | 9.1 ns |
| step | 0.7 | 130.5 ns |
| struve_H | 2, 0.7 | 30.0 µs |
| struve_L | 2, 0.7 | 28.2 µs |
| sub | 0.7, 0.7 | 13.2 ns |
| tan | 0.7 | 6.4 µs |
| tanh | 0.7 | 62.1 ns |
| tetragamma | 2.345 | 3.5 µs |
| tri | 0.7 | 276.5 ns |
| trigamma | 2.345 | 2.8 µs |
| vercos | 0.7 | 137.1 ns |
| versin | 0.7 | 142.4 ns |
| zatahp | 2.3, 1.4 | 50.4 µs |
| zatahp (integer order) | -4, 0.75 | 44.7 µs |
| zeta | 2.345 | 79.2 µs |
| zeta_pair | 2.345 | 182.9 µs |
| zetah | 2.3, 1.4 | 25.9 µs |
| zetap | 2.345 | 112.3 µs |

### number / multi-precision floating point

| Operation | Inputs | 256 bits | 512 bits | 768 bits | 1024 bits | 2048 bits | 4096 bits |
|---|---|---:|---:|---:|---:|---:|---:|
| E1 | 2.345678 | 52.8 µs | 90.4 µs | 131.9 µs | 172.1 µs | 493.0 µs | 2.213 ms |
| Ei | 2.345678 | 47.7 µs | 94.7 µs † | 119.5 µs | 171.6 µs | 550.3 µs | 2.192 ms |
| Li | 2.345 | 45.8 µs | 71.6 µs | 108.4 µs | 157.6 µs | 495.1 µs | 1.947 ms |
| abs | 0.7 | 106.3 ns | 88.5 ns | 105.0 ns | 98.2 ns | 106.3 ns | 107.0 ns |
| acos | 0.7 | 19.0 µs | 31.1 µs | 36.1 µs | 59.0 µs | 105.2 µs | 295.5 µs |
| acosec | 2.345 | 25.6 µs † | 31.3 µs | 36.3 µs | 52.3 µs | 115.4 µs | 356.0 µs |
| acosech | 0.7 | 6.6 µs | 9.8 µs | 14.4 µs | 18.6 µs | 47.0 µs | 128.1 µs |
| acosh | 2.345 | 7.6 µs | 10.0 µs | 14.5 µs | 16.7 µs | 41.3 µs | 123.1 µs |
| acot | 0.7 | 19.9 µs | 23.5 µs | 38.3 µs | 49.2 µs | 118.2 µs | 357.7 µs |
| acoth | 2.345 | 9.6 µs † | 8.9 µs | 14.4 µs | 20.0 µs | 46.4 µs | 130.9 µs |
| add | 0.7, 0.7 | 136.7 ns | 108.1 ns | 119.2 ns | 117.1 ns | 187.5 ns | 364.9 ns |
| add_long | 17, 3 | 859.2 ns | 732.9 ns | 753.2 ns | 779.2 ns | 767.4 ns | 855.2 ns |
| appell_F1 | 0.5, 0.7, 0.8, 2.5, 0.2, 0.3 | 166.9 µs | 398.1 µs | 686.0 µs | 1.116 ms | 3.176 ms | 11.274 ms |
| arccovercos | 0.5 | 19.4 µs † | 27.2 µs | 32.3 µs | 48.2 µs | 127.9 µs | 296.7 µs |
| arccoversin | 0.5 | 21.0 µs † | 23.9 µs | 32.2 µs | 49.6 µs | 112.6 µs | 308.6 µs |
| archacovercos | 0.5 | 1.6 µs | 1.5 µs | 1.6 µs | 1.4 µs | 2.1 µs | 1.8 µs |
| archacoversin | 0.5 | 2.3 µs † | 1.4 µs | 1.9 µs | 1.5 µs | 1.6 µs | 2.1 µs |
| archavercos | 0.5 | 2.3 µs † | 1.5 µs | 1.7 µs | 1.9 µs | 1.8 µs | 2.3 µs |
| archaversin | 0.5 | 1.8 µs | 1.8 µs | 1.7 µs | 1.7 µs | 1.7 µs | 1.9 µs |
| arcvercos | 0.5 | 18.0 µs | 26.4 µs | 31.5 µs | 52.5 µs | 107.0 µs | 320.5 µs |
| arcversin | 0.5 | 17.0 µs | 26.6 µs | 43.9 µs | 47.9 µs | 110.5 µs | 300.2 µs |
| arg | 0.7 | 713.4 ns | 630.7 ns | 529.6 ns | 546.4 ns | 524.8 ns | 584.6 ns |
| asec | 2.345 | 22.1 µs | 28.4 µs | 39.0 µs | 59.0 µs | 124.8 µs | 353.1 µs |
| asech | 0.7 | 7.3 µs | 12.2 µs | 14.0 µs | 23.0 µs | 43.2 µs | 122.2 µs |
| asin | 0.7 | 25.9 µs † | 29.6 µs | 40.0 µs | 52.7 µs | 117.2 µs | 318.7 µs |
| asinh | 0.7 | 7.1 µs | 11.4 µs | 13.6 µs | 17.0 µs | 41.8 µs | 129.4 µs |
| atan | 0.7 | 15.3 µs | 27.2 µs | 33.5 µs | 45.9 µs | 103.4 µs | 377.2 µs |
| atan2 | 0.5, -0.75 | 16.3 µs | 29.9 µs | 31.3 µs | 48.2 µs | 114.5 µs | 317.6 µs |
| atanh | 0.7 | 8.4 µs | 11.9 µs | 14.8 µs | 18.6 µs | 43.6 µs | 110.8 µs |
| bessel_I | 2, 0.7 | 62.4 µs | 104.3 µs | 152.9 µs | 255.4 µs | 835.6 µs | 3.720 ms |
| bessel_J | 2, 0.7 | 10.1 µs † | 16.5 µs | 25.0 µs | 53.7 µs | 166.6 µs | 815.8 µs |
| bessel_K | 2, 0.7 | 87.3 µs † | 110.1 µs | 166.9 µs | 271.7 µs | 889.2 µs | 4.345 ms |
| bessel_Y | 2, 0.7 | 52.9 µs | 99.7 µs | 142.4 µs | 217.3 µs | 796.5 µs | 4.212 ms |
| beta | 2.3, 4.5 | 146.2 µs † | 284.5 µs | 518.1 µs | 946.6 µs | 3.081 ms | 15.510 ms |
| beta_pdf | 0.5, 2.5, 3.5 | 124.1 µs | 269.1 µs | 355.1 µs | 586.2 µs | 2.077 ms | 11.406 ms |
| binomial | 5, 2 | 146.2 µs | 301.9 µs | 479.5 µs | 742.4 µs | 3.035 ms | 13.847 ms |
| ceil | 0.7 | 683.2 ns | 629.5 ns | 528.7 ns | 517.8 ns | 536.1 ns | 671.8 ns |
| chebyshev_Tn | 2, 0.7 | 4.3 µs † | 3.0 µs | 3.6 µs | 3.3 µs | 4.0 µs | 6.2 µs |
| chebyshev_Un | 2, 0.7 | 5.1 µs † | 4.3 µs | 4.0 µs | 4.2 µs | 4.2 µs | 7.1 µs |
| circ | 0.7 | 1.4 µs † | 901.2 ns | 942.9 ns | 925.8 ns | 1.1 µs | 1.3 µs |
| clausen | 3, 0.7 | 32.5 µs | 74.6 µs | 137.9 µs | 220.2 µs | 975.2 µs | 5.495 ms |
| clausen2 | 0.7 | 36.7 µs | 75.8 µs | 146.2 µs | 229.0 µs | 1.125 ms | 4.790 ms |
| conj | 0.7 | 94.8 ns | 85.4 ns | 90.7 ns | 85.3 ns | 85.2 ns | 108.7 ns |
| cos | 0.7 | 3.0 µs † | 4.7 µs | 8.6 µs | 14.2 µs | 40.7 µs | 144.7 µs |
| cosec | 0.7 | 4.7 µs † | 5.6 µs | 11.0 µs | 15.2 µs | 47.3 µs | 179.1 µs |
| cosech | 0.7 | 3.8 µs | 5.8 µs | 9.7 µs | 15.6 µs | 59.3 µs | 176.2 µs |
| cosh | 0.7 | 3.7 µs | 6.0 µs | 10.4 µs | 14.2 µs | 49.8 µs | 172.1 µs |
| cot | 0.7 | 8.5 µs † | 11.5 µs | 17.5 µs | 28.2 µs | 94.5 µs | 320.8 µs |
| coth | 0.7 | 7.8 µs | 12.3 µs | 20.6 µs | 36.4 µs | 100.0 µs | 365.7 µs |
| covercos | 0.7 | 4.2 µs | 7.0 µs | 10.5 µs | 15.7 µs | 46.3 µs | 188.3 µs |
| coversin | 0.7 | 5.3 µs † | 9.2 µs † | 9.8 µs | 15.9 µs | 42.5 µs | 220.6 µs |
| cubrt | 0.7 | 16.3 µs | 19.8 µs | 31.0 µs | 39.7 µs | 90.2 µs | 323.5 µs |
| digamma | 2.345 | 134.3 µs | 275.2 µs | 508.6 µs | 943.3 µs | 4.152 ms | 21.666 ms |
| dilog | 0.7 | 125.2 µs | 191.8 µs | 400.0 µs | 473.8 µs | 1.328 ms | 4.695 ms |
| div | 0.7, 0.7 | 404.9 ns | 559.4 ns | 974.1 ns | 1.5 µs | 3.4 µs | 11.0 µs |
| erf | 0.7 | 19.8 µs | 41.0 µs | 64.7 µs | 110.7 µs | 482.7 µs | 2.280 ms |
| erfc | 0.7 | 17.9 µs | 45.8 µs | 79.0 µs | 114.1 µs | 469.3 µs | 1.916 ms |
| erfcinv | 0.5 | 97.7 µs | 200.9 µs | 316.3 µs | 566.6 µs | 1.509 ms | 5.738 ms |
| erfinv | 0.5 | 111.2 µs | 203.8 µs | 297.3 µs | 486.3 µs | 1.609 ms | 6.198 ms |
| exp | 1.23456789 | 3.3 µs | 5.6 µs | 9.5 µs | 16.4 µs | 52.3 µs | 189.8 µs |
| floor | 0.7 | 96.5 ns | 93.2 ns | 95.0 ns | 93.6 ns | 116.5 ns | 115.7 ns |
| gamma | 2.345 | 38.3 µs | 101.3 µs | 160.8 µs | 254.2 µs | 996.9 µs | 4.509 ms |
| gammainc_P | 2.3, 1.4 | 113.0 µs | 263.1 µs | 462.4 µs | 635.7 µs | 2.739 ms | 11.351 ms |
| gammainc_Q | 2.3, 1.4 | 152.6 µs | 322.5 µs | 522.0 µs | 863.2 µs | 2.827 ms | 10.792 ms |
| gammainc_lower | 2.3, 1.4 | 70.6 µs | 158.7 µs | 286.0 µs | 432.6 µs | 1.564 ms | 6.298 ms |
| gammainc_upper | 2.3, 1.4 | 134.6 µs | 310.8 µs | 479.3 µs | 805.1 µs | 2.971 ms | 12.024 ms |
| gammainv | 9.5 | 551.5 µs | 1.041 ms | 1.374 ms | 2.178 ms | 5.995 ms | 19.824 ms |
| hacovercos | 0.7 | 5.2 µs | 8.1 µs | 14.2 µs | 16.4 µs | 48.8 µs | 177.8 µs |
| hacoversin | 0.7 | 4.4 µs | 7.9 µs | 25.9 µs † | 19.3 µs | 51.2 µs | 173.9 µs |
| harmonic_poly | 2, 0.7 | 4.0 µs | 6.3 µs | 7.9 µs | 7.4 µs | 12.3 µs | 33.1 µs |
| havercos | 0.7 | 3.5 µs | 6.4 µs | 13.1 µs | 14.4 µs | 56.3 µs | 196.0 µs |
| haversin | 0.7 | 3.5 µs | 6.9 µs | 12.6 µs | 15.3 µs | 47.0 µs | 172.2 µs |
| hermite_Hn | 2, 0.7 | 4.5 µs | 3.9 µs | 6.2 µs † | 4.8 µs | 5.6 µs | 8.7 µs |
| hypergeometric_pFq | 0.7, 2.5, 0.3 | 58.4 µs | 121.5 µs | 197.0 µs | 291.3 µs | 1.336 ms | 7.023 ms |
| hypot | 0.7, 1.2 | 698.8 ns | 1.1 µs | 1.3 µs | 1.7 µs | 3.1 µs | 9.3 µs |
| imag_part | 0.7 | 81.7 ns | 77.0 ns | 82.4 ns | 83.3 ns | 84.4 ns | 83.3 ns |
| inv | 0.7 | 242.5 ns | 303.9 ns | 432.0 ns | 624.9 ns | 1.6 µs | 4.2 µs |
| lambert_W0 | 0.7 | 64.6 µs | 103.9 µs | 185.9 µs | 286.5 µs | 859.5 µs | 3.305 ms |
| lambert_Wm1 | -0.2 | 69.9 µs | 116.2 µs | 209.0 µs | 309.8 µs | 979.2 µs | 3.056 ms |
| ldexp | 1.23456789, 3 | 221.3 ns | 142.3 ns | 216.2 ns | 118.8 ns | 163.6 ns | 133.5 ns |
| legendre_chi | 2, 0.7 | 57.1 µs | 110.1 µs | 183.2 µs | 258.5 µs | 913.4 µs | 3.037 ms |
| lerch_phi | 0.3, 2, 1.4 | 145.7 µs | 242.6 µs | 458.7 µs | 880.8 µs | 3.082 ms | 18.053 ms |
| lerch_phi (integer order) | 0.3125, -15, 0.375 | 32.6 µs | 32.5 µs | 33.9 µs | 34.5 µs | 30.5 µs | 31.1 µs |
| lerch_phi (negative order) | 0.3125, -15, 0.375 | 42.0 µs | 29.7 µs | 29.2 µs | 34.4 µs | 31.3 µs | 40.3 µs |
| lg | 2.345678 | 11.3 µs | 19.0 µs | 26.3 µs | 39.3 µs | 87.6 µs | 249.0 µs |
| lgamma | 2.345 | 37.4 µs | 93.0 µs | 163.2 µs | 290.8 µs | 1.006 ms | 4.367 ms |
| ln | 2.345678 | 5.6 µs | 8.2 µs | 12.2 µs | 17.6 µs | 46.0 µs | 121.6 µs |
| log | 2.345678 | 5.3 µs | 8.9 µs | 12.1 µs | 17.1 µs | 38.7 µs | 118.2 µs |
| log10 | 2.345678 | 12.3 µs | 16.9 µs | 26.1 µs | 44.0 µs | 84.4 µs | 269.1 µs |
| logbeta | 2.3, 4.5 | 124.0 µs | 282.3 µs | 499.4 µs | 801.3 µs | 3.008 ms | 16.288 ms |
| logbeta_pdf | 0.5, 2.5, 3.5 | 97.0 µs | 191.4 µs | 355.1 µs | 565.4 µs | 2.302 ms | 11.201 ms |
| lommel_S | 0.5, 0.7, 1.2 | 29.9 µs | 68.2 µs | 131.7 µs | 212.2 µs | 858.8 µs | 3.511 ms |
| mul | 0.7, 0.7 | 110.7 ns | 191.5 ns | 360.6 ns | 406.1 ns | 1.0 µs | 3.0 µs |
| mul_long | 17, 3 | 742.7 ns | 831.8 ns | 1.1 µs | 769.1 ns | 848.6 ns | 855.1 ns |
| mul_pow10 | 1.23456789, 3 | 352.1 ns | 459.0 ns | 617.4 ns | 614.5 ns | 482.4 ns | 680.9 ns |
| neg | 0.7 | 121.7 ns | 101.4 ns | 97.6 ns | 92.7 ns | 105.7 ns | 99.7 ns |
| normal_cdf | 0.7 | 19.7 µs | 83.9 µs | 79.3 µs | 110.1 µs | 446.7 µs | 1.786 ms |
| normal_logpdf | 0.7 | 5.1 µs | 8.9 µs | 12.7 µs | 17.3 µs | 39.5 µs | 127.6 µs |
| normal_pdf | 0.7 | 3.7 µs | 7.8 µs | 11.6 µs | 16.6 µs | 58.5 µs | 221.5 µs |
| polygamma | 3, 2.345 | 55.1 µs | 136.3 µs | 260.6 µs | 397.4 µs | 2.084 ms | 10.796 ms |
| polylog | 2, 0.7 | 101.3 µs | 189.1 µs | 283.9 µs | 411.5 µs | 1.417 ms | 4.647 ms |
| polylog1 | 0.7 | 6.1 µs | 9.1 µs | 13.3 µs | 18.5 µs | 38.9 µs | 120.3 µs |
| pow | 1.23456789, 3.5 | 10.0 µs | 15.6 µs | 24.2 µs | 42.2 µs | 94.0 µs | 282.3 µs |
| pow10 | 3 | 275.9 ns | 384.4 ns | 390.6 ns | 505.8 ns | 325.9 ns | 619.8 ns |
| pow_int | 1.23456789, 3 | 325.2 ns | 419.4 ns | 592.4 ns | 791.2 ns | 2.6 µs | 7.3 µs |
| productlog | 0.7 | 54.2 µs | 110.9 µs | 181.3 µs | 286.4 µs | 932.8 µs | 3.633 ms |
| real_part | 0.7 | 87.6 ns | 88.1 ns | 84.7 ns | 84.8 ns | 86.1 ns | 171.6 ns |
| rect | 0.7 | 1.0 µs | 1.1 µs | 986.5 ns | 1.2 µs | 1.1 µs | 1.4 µs |
| root | 1.2345, 3 | 13.9 µs | 21.2 µs | 26.2 µs | 39.8 µs | 111.0 µs | 358.1 µs |
| round | 0.7 | 2.5 µs | 2.5 µs | 2.5 µs | 2.9 µs | 3.2 µs | 3.1 µs |
| roundn | 1.23456789, 3 | 5.3 µs | 6.0 µs | 6.2 µs | 6.8 µs | 7.6 µs | 11.4 µs |
| rounds | 1.23456789, 3 | 20.4 µs | 29.8 µs | 42.5 µs | 49.1 µs | 93.4 µs | 239.8 µs |
| sec | 0.7 | 2.4 µs | 6.0 µs | 9.8 µs | 15.2 µs | 42.5 µs | 166.0 µs |
| sech | 0.7 | 3.4 µs | 6.4 µs | 11.8 µs | 15.2 µs | 51.5 µs | 171.6 µs |
| sgn | 0.7 | 266.4 ns | 256.8 ns | 270.1 ns | 244.0 ns | 271.7 ns | 259.6 ns |
| sin | 0.567 | 3.3 µs | 5.5 µs | 9.6 µs | 14.8 µs | 55.2 µs | 167.2 µs |
| sinc | 0.7 | 5.6 µs | 8.7 µs | 12.9 µs | 21.3 µs | 47.5 µs | 188.9 µs |
| sincos | 0.7 | 12.1 µs | 15.1 µs | 19.4 µs | 27.9 µs | 53.0 µs | 181.4 µs |
| sinh | 0.7 | 3.0 µs | 6.3 µs | 9.3 µs | 14.4 µs | 50.9 µs | 181.1 µs |
| sinhcosh | 0.7 | 3.2 µs | 6.8 µs | 9.8 µs | 15.8 µs | 55.8 µs | 187.9 µs |
| sqr | 0.7 | 170.9 ns | 220.1 ns | 352.6 ns | 436.7 ns | 722.5 ns | 2.0 µs |
| sqrt | 1.23456789 | 357.6 ns | 457.6 ns | 609.4 ns | 756.5 ns | 1.6 µs | 3.2 µs |
| step | 0.7 | 241.5 ns | 339.6 ns | 264.1 ns | 290.2 ns | 319.5 ns | 252.3 ns |
| struve_H | 2, 0.7 | 64.3 µs | 113.5 µs | 177.4 µs | 323.0 µs | 877.0 µs | 4.020 ms |
| struve_L | 2, 0.7 | 61.7 µs | 106.9 µs | 176.9 µs | 241.1 µs | 886.3 µs | 3.614 ms |
| sub | 0.7, 0.7 | 84.4 ns | 97.4 ns | 96.8 ns | 105.8 ns | 156.9 ns | 266.5 ns |
| tan | 0.7 | 12.8 µs | 15.1 µs | 23.2 µs | 25.7 µs | 56.1 µs | 202.1 µs |
| tanh | 0.7 | 3.0 µs | 6.6 µs | 10.0 µs | 15.8 µs | 48.0 µs | 173.3 µs |
| tetragamma | 2.345 | 46.0 µs | 138.6 µs | 244.9 µs | 433.4 µs | 2.157 ms | 8.456 ms |
| tri | 0.7 | 941.1 ns | 897.3 ns | 1.1 µs | 993.4 ns | 1.0 µs | 1.3 µs |
| trigamma | 2.345 | 32.8 µs | 102.0 µs | 176.5 µs | 300.1 µs | 1.455 ms | 5.980 ms |
| vercos | 0.7 | 6.7 µs † | 5.6 µs | 9.1 µs | 14.0 µs | 42.3 µs | 170.8 µs |
| versin | 0.7 | 3.3 µs | 5.4 µs | 8.8 µs | 13.8 µs | 41.6 µs | 160.4 µs |
| zatahp | 2.3, 1.4 | 67.8 µs | 144.2 µs | 273.2 µs | 461.6 µs † | 1.966 ms | 8.895 ms |
| zatahp (integer order) | -4, 0.75 | 63.9 µs | 112.0 µs | 172.0 µs | 195.8 µs | 987.0 µs | 3.677 ms |
| zeta | 2.345 | 899.0 µs | 3.963 ms | 9.429 ms | 1.065 ms | 5.121 ms | 27.223 ms |
| zeta_pair | 2.345 | 242.8 µs | 440.7 µs | 842.9 µs | 1.417 ms | 6.110 ms | 31.868 ms |
| zetah | 2.3, 1.4 | 47.0 µs | 85.6 µs | 129.1 µs | 171.1 µs | 729.9 µs | 3.374 ms |
| zetap | 2.345 | 213.6 µs | 412.1 µs | 745.1 µs | 1.427 ms | 6.567 ms | 31.131 ms |

### number / multi-precision complex floating point

| Operation | Inputs | 256 bits | 512 bits | 768 bits | 1024 bits | 2048 bits | 4096 bits |
|---|---|---:|---:|---:|---:|---:|---:|
| E1 | 2.345678+0.125i | 113.9 µs | 209.7 µs | 338.3 µs | 523.4 µs | 2.279 ms | 9.759 ms |
| Ei | 2.345678+0.125i | 108.3 µs | 204.3 µs | 336.4 µs | 578.8 µs | 2.325 ms | 9.650 ms |
| Li | 2.345+0.125i | 107.0 µs | 181.6 µs | 315.3 µs | 543.0 µs | 2.191 ms | 11.392 ms |
| abs | 0.7+0.125i | 1.0 µs | 1.3 µs | 1.5 µs | 1.6 µs | 3.9 µs | 6.8 µs |
| acos | 0.7+0.125i | 72.7 µs | 87.7 µs | 148.9 µs | 75.8 µs | 186.4 µs | 554.9 µs |
| acosec | 2.345+0.125i | 48.4 µs | 75.5 µs | 106.1 µs | 155.3 µs | 316.5 µs | 1.094 ms |
| acosech | 0.7+0.125i | 64.7 µs | 91.5 µs | 108.5 µs | 164.8 µs | 333.7 µs | 1.188 ms |
| acosh | 2.345+0.125i | 46.5 µs | 73.9 µs | 108.2 µs | 62.0 µs | 467.1 µs † | 578.8 µs |
| acot | 0.7+0.125i | 47.7 µs | 76.0 µs | 109.1 µs | 138.3 µs | 386.5 µs | 1.299 ms † |
| acoth | 2.345+0.125i | 34.6 µs | 59.6 µs | 92.2 µs | 121.5 µs | 333.1 µs | 1.095 ms † |
| add | 0.7+0.125i, 0.7+0.125i | 429.7 ns | 500.5 ns | 435.6 ns | 411.4 ns | 485.5 ns | 862.0 ns |
| add_long | 17+0.125i, 3 | 3.2 µs | 2.7 µs | 2.8 µs | 2.7 µs | 3.8 µs | 4.3 µs |
| appell_F1 | <span style="display:inline-block;width:40ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">0.5+0.125i, 0.7+0.125i, 0.8+0.125i, 2.5+<br/>0.125i, 0.2+0.125i, 0.3+0.125i</span> | 505.8 µs | 1.227 ms | 2.519 ms | 5.269 ms | 12.560 ms | 55.101 ms † |
| arccovercos | 0.5+0.125i | 53.5 µs | 87.3 µs | 59.3 µs | 151.6 µs | 218.3 µs | 534.6 µs |
| arccoversin | 0.5+0.125i | 51.0 µs | 43.6 µs | 54.9 µs | 146.3 µs | 209.0 µs | 559.6 µs |
| archacovercos | 0.5+0.125i | 13.0 µs | 18.3 µs | 36.8 µs † | 29.7 µs | 49.2 µs | 135.7 µs |
| archacoversin | 0.5+0.125i | 14.0 µs | 16.6 µs | 22.8 µs | 24.5 µs | 50.4 µs | 125.0 µs |
| archavercos | 0.5+0.125i | 12.7 µs | 18.1 µs | 22.6 µs | 26.1 µs | 55.2 µs | 138.4 µs |
| archaversin | 0.5+0.125i | 12.3 µs | 19.3 µs | 23.6 µs | 25.3 µs | 58.3 µs | 152.0 µs |
| arcvercos | 0.5+0.125i | 53.4 µs | 50.9 µs | 123.2 µs | 81.7 µs | 157.8 µs | 448.7 µs |
| arcversin | 0.5+0.125i | 30.4 µs | 47.5 µs | 113.2 µs | 75.5 µs | 145.8 µs | 544.4 µs |
| arg | 0.7+0.125i | 13.7 µs | 24.6 µs | 34.5 µs | 43.1 µs | 112.8 µs | 453.0 µs |
| asec | 2.345+0.125i | 49.3 µs | 73.6 µs | 103.0 µs | 143.0 µs | 328.2 µs | 1.381 ms † |
| asech | 0.7+0.125i | 31.6 µs | 41.1 µs | 57.9 µs | 97.4 µs | 220.4 µs | 548.9 µs |
| asin | 0.7+0.125i | 64.3 µs | 92.1 µs | 118.5 µs | 160.2 µs | 353.9 µs | 513.9 µs |
| asinh | 0.7+0.125i | 52.3 µs | 77.2 µs | 125.5 µs | 152.2 µs | 322.5 µs | 907.3 µs |
| atan | 0.7+0.125i | 47.7 µs | 80.4 µs | 104.0 µs | 138.3 µs | 325.8 µs | 1.051 ms |
| atanh | 0.7+0.125i | 45.7 µs | 78.5 µs | 97.6 µs | 123.9 µs | 339.4 µs | 989.4 µs |
| bessel_I | 2, 0.7+0.125i | 114.2 µs | 188.4 µs | 341.5 µs | 615.7 µs | 2.314 ms | 10.953 ms |
| bessel_J | 2, 0.7+0.125i | 99.7 µs | 204.6 µs | 384.3 µs | 637.9 µs | 2.103 ms | 9.547 ms |
| bessel_K | 2, 0.7+0.125i | 139.6 µs | 276.1 µs | 435.8 µs | 730.8 µs | 2.735 ms | 10.859 ms |
| bessel_Y | 2, 0.7+0.125i | 199.3 µs | 412.9 µs | 803.3 µs | 1.087 ms | 4.500 ms | 19.465 ms |
| ceil | 0.7+0.125i | 1.6 µs | 1.7 µs | 1.7 µs | 2.0 µs | 1.9 µs | 2.0 µs † |
| chebyshev_Tn | 2, 0.7+0.125i | 14.8 µs † | 11.2 µs | 12.6 µs | 13.3 µs | 14.0 µs | 17.1 µs |
| chebyshev_Un | 2, 0.7+0.125i | 13.2 µs | 14.6 µs | 14.2 µs | 20.4 µs † | 16.6 µs | 22.6 µs |
| circ | 0.7 | 2.4 µs | 3.8 µs † | 14.2 µs † | 3.5 µs | 4.4 µs | 8.7 µs |
| clausen | 3, 0.7+0.125i | 162.2 µs | 359.0 µs | 717.3 µs | 1.338 ms | 5.168 ms | 25.328 ms |
| clausen2 | 0.7+0.125i | 142.3 µs | 473.7 µs | 906.1 µs | 1.191 ms | 4.576 ms | 16.636 ms † |
| conj | 0.7+0.125i | 339.2 ns | 321.9 ns | 324.7 ns | 348.6 ns | 324.8 ns | 344.0 ns |
| cos | 0.7+0.125i | 17.7 µs | 16.3 µs | 24.2 µs | 36.0 µs | 114.6 µs | 391.6 µs |
| cosec | 0.7+0.125i | 19.5 µs | 21.3 µs | 35.1 µs | 36.5 µs | 108.3 µs | 417.4 µs |
| cosech | 0.7+0.125i | 14.2 µs | 19.4 µs | 27.3 µs | 90.2 µs | 105.5 µs | 320.9 µs |
| cosh | 0.7+0.125i | 9.0 µs | 15.7 µs | 23.0 µs | 85.6 µs | 92.9 µs | 263.7 µs |
| cot | 0.7+0.125i | 35.9 µs | 33.8 µs | 65.9 µs | 85.0 µs | 244.0 µs | 774.9 µs |
| coth | 0.7+0.125i | 19.3 µs | 33.1 µs | 48.6 µs | 177.7 µs | 187.5 µs | 817.0 µs |
| covercos | 0.7+0.125i | 20.2 µs | 19.3 µs | 28.7 µs | 39.3 µs | 111.4 µs | 386.5 µs |
| coversin | 0.7+0.125i | 21.0 µs | 18.4 µs | 31.6 µs | 39.6 µs | 110.2 µs | 374.4 µs |
| cubrt | 0.7+0.125i | 41.8 µs | 69.6 µs | 111.1 µs | 162.4 µs | 497.1 µs | 1.759 ms † |
| digamma | 2.345+0.125i | 236.7 µs | 372.6 µs | 632.0 µs | 892.7 µs | 3.560 ms | 14.093 ms |
| dilog | 0.7+0.125i | 171.9 µs | 216.7 µs | 384.6 µs | 533.9 µs | 1.719 ms | 6.261 ms |
| div | 0.7+0.125i, 0.7+0.125i | 7.9 µs | 11.4 µs | 17.6 µs | 21.4 µs | 43.2 µs | 113.7 µs |
| erf | 0.7+0.125i | 88.4 µs | 211.6 µs | 352.4 µs | 597.3 µs | 2.382 ms | 10.912 ms |
| erfc | 0.7+0.125i | 83.5 µs | 183.3 µs | 342.0 µs | 558.1 µs | 1.857 ms | 8.918 ms |
| erfcinv | 0.5+0.125i | 575.3 µs | 1.082 ms | 1.502 ms | 2.462 ms | 6.635 ms | 24.048 ms |
| erfinv | 0.5+0.125i | 570.3 µs | 1.051 ms | 1.662 ms | 2.395 ms | 6.832 ms | 26.521 ms |
| exp | 1.23456789+0.125i | 10.8 µs | 21.8 µs | 37.3 µs | 51.9 µs | 159.8 µs | 744.3 µs |
| floor | 0.7+0.125i | 343.2 ns | 320.1 ns | 303.4 ns | 319.0 ns | 357.4 ns | 301.3 ns |
| gamma | 2.345+0.125i | 225.4 µs | 322.2 µs | 538.0 µs | 779.3 µs | 2.613 ms | 8.203 ms |
| gammainc_P | 2.3+0.125i, 1.4+0.125i | 778.6 µs | 1.005 ms | 1.462 ms | 2.100 ms | 7.137 ms | 31.234 ms |
| gammainc_Q | 2.3+0.125i, 1.4+0.125i | 748.4 µs | 963.9 µs | 1.420 ms | 2.564 ms | 6.293 ms | 24.787 ms |
| gammainc_lower | 2.3+0.125i, 1.4+0.125i | 400.6 µs | 661.1 µs | 829.4 µs | 1.221 ms | 3.995 ms | 16.130 ms |
| gammainc_upper | 2.3+0.125i, 1.4+0.125i | 714.3 µs | 1.008 ms | 1.418 ms | 2.392 ms | 7.083 ms | 27.405 ms |
| gammainv | 9.5+0.125i | 2.758 ms | 3.563 ms | 4.470 ms | 5.406 ms | 12.211 ms | 32.116 ms |
| hacovercos | 0.7+0.125i | 29.8 µs † | 23.2 µs | 35.3 µs | 51.0 µs | 124.1 µs | 484.5 µs |
| hacoversin | 0.7+0.125i | 24.6 µs | 25.7 µs | 35.4 µs | 45.3 µs | 122.9 µs | 426.2 µs |
| harmonic_poly | 2, 0.7+0.125i | 19.8 µs | 19.5 µs | 25.6 µs | 24.7 µs | 41.5 µs | 84.5 µs |
| havercos | 0.7+0.125i | 24.5 µs | 21.1 µs | 33.0 µs | 46.4 µs | 125.7 µs | 383.5 µs |
| haversin | 0.7+0.125i | 24.7 µs | 24.4 µs | 32.6 µs | 45.1 µs | 122.9 µs | 403.6 µs |
| hermite_Hn | 2, 0.7+0.125i | 14.1 µs | 14.2 µs | 15.2 µs | 21.3 µs † | 18.8 µs | 21.5 µs |
| hypergeometric_pFq | 0.7+0.125i, 2.5+0.125i, 0.3+0.125i | 197.8 µs | 444.7 µs | 801.1 µs | 1.475 ms | 3.629 ms | 18.258 ms |
| imag_part | 0.7+0.125i | 142.1 ns | 146.9 ns | 132.2 ns | 165.2 ns | 140.9 ns | 236.0 ns |
| inv | 0.7+0.125i | 1.3 µs | 1.9 µs | 2.0 µs | 2.9 µs | 5.1 µs | 16.9 µs |
| ldexp | 1.23456789+0.125i, 3 | 362.7 ns | 372.3 ns | 353.7 ns | 390.0 ns | 662.7 ns | 514.0 ns |
| legendre_chi | 2, 0.7+0.125i | 116.0 µs | 259.5 µs | 438.7 µs | 717.9 µs | 1.977 ms | 8.171 ms |
| lerch_phi | 0.3+0.125i, 2+0.125i, 1.4+0.125i | 452.7 µs | 12.0 µs | 9.8 µs | 10.7 µs | 9.8 µs | 15.2 µs † |
| lerch_phi (integer order) | 0.3125+0.125i, -15, 0.375+0.125i | 45.9 µs | 61.7 µs | 42.9 µs | 47.9 µs | 52.2 µs | 56.2 µs |
| lerch_phi (negative order) | 0.3125+0.125i, -15+0.125i, 0.375+0.125i | 539.8 µs | 11.0 µs | 9.8 µs | 10.5 µs | 11.9 µs | 15.5 µs † |
| lg | 2.345678+0.125i | 30.0 µs | 48.9 µs | 56.2 µs | 91.6 µs | 200.0 µs | 738.9 µs |
| lgamma | 2.345+0.125i | 167.5 µs | 309.5 µs | 408.2 µs | 791.7 µs | 1.858 ms | 9.079 ms |
| ln | 2.345678+0.125i | 20.1 µs | 30.8 µs | 42.8 µs | 58.6 µs | 151.2 µs | 542.5 µs |
| log | 2.345678+0.125i | 18.2 µs | 30.9 µs | 41.3 µs | 61.0 µs | 129.6 µs | 513.3 µs |
| log10 | 2.345678+0.125i | 28.5 µs | 42.7 µs | 52.8 µs | 78.7 µs | 210.3 µs | 752.6 µs |
| lommel_S | 0.5+0.125i, 0.7+0.125i, 1.2+0.125i | 239.2 µs | 540.1 µs | 803.6 µs | 1.321 ms | 4.909 ms | 22.931 ms |
| mul | 0.7+0.125i, 0.7+0.125i | 1.5 µs | 2.4 µs † | 2.2 µs | 2.8 µs | 4.5 µs | 10.6 µs † |
| mul_long | 17+0.125i, 3 | 3.9 µs | 3.8 µs | 6.3 µs † | 3.9 µs | 4.6 µs | 3.7 µs |
| mul_pow10 | 1.23456789+0.125i, 3 | 3.2 µs | 3.7 µs | 4.3 µs | 3.4 µs | 3.6 µs | 5.8 µs † |
| neg | 0.7+0.125i | 352.1 ns | 306.3 ns | 326.4 ns | 283.9 ns | 379.4 ns | 393.5 ns |
| normal_cdf | 0.7+0.125i | 89.1 µs | 229.3 µs | 321.1 µs | 681.1 µs | 2.066 ms | 8.610 ms |
| normal_logpdf | 0.7+0.125i | 8.3 µs | 12.0 µs | 19.8 µs | 21.4 µs | 42.7 µs | 150.5 µs |
| normal_pdf | 0.7+0.125i | 19.4 µs | 34.3 µs | 65.1 µs | 86.6 µs | 260.8 µs | 936.4 µs |
| polygamma | 3, 2.345+0.125i | 348.6 µs | 594.0 µs | 991.6 µs | 1.839 ms | 6.162 ms | 29.444 ms |
| polylog | 2, 0.7+0.125i | 188.9 µs | 221.6 µs | 428.4 µs | 522.9 µs | 1.579 ms | 6.537 ms |
| polylog1 | 0.7+0.125i | 28.3 µs | 42.4 µs | 54.2 µs | 78.7 µs | 166.0 µs | 509.6 µs |
| pow | 1.23456789+0.125i, 3.5+0.125i | 36.2 µs | 58.8 µs | 98.1 µs | 178.9 µs | 367.3 µs | 1.500 ms |
| pow10 | 3 | 511.0 ns | 608.2 ns | 575.8 ns | 688.7 ns | 625.5 ns | 672.0 ns |
| pow_int | 1.23456789+0.125i, 3 | 6.6 µs | 7.3 µs | 12.0 µs | 10.6 µs | 13.7 µs | 25.4 µs |
| real_part | 0.7+0.125i | 129.5 ns | 137.6 ns | 141.8 ns | 152.2 ns | 153.2 ns | 229.2 ns |
| rect | 0.7 | 2.7 µs | 3.4 µs | 2.9 µs | 3.9 µs | 4.4 µs | 8.3 µs |
| root | 1.2345+0.125i, 3 | 38.2 µs | 73.2 µs | 192.3 µs | 276.6 µs | 383.7 µs | 1.376 ms |
| round | 0.7+0.125i | 9.7 µs | 11.4 µs | 9.5 µs | 10.0 µs | 10.6 µs | 10.7 µs |
| roundn | 1.23456789+0.125i, 3 | 16.0 µs | 21.4 µs | 15.7 µs | 23.9 µs | 21.5 µs | 30.2 µs |
| rounds | 1.23456789+0.125i, 3 | 53.3 µs | 84.2 µs | 88.9 µs | 132.0 µs | 221.8 µs | 744.7 µs |
| sec | 0.7+0.125i | 19.4 µs | 19.0 µs | 26.7 µs | 52.5 µs | 110.8 µs | 382.9 µs |
| sech | 0.7+0.125i | 12.7 µs | 18.1 µs | 26.2 µs | 91.5 µs | 90.7 µs | 312.5 µs |
| sgn | 0.7 | 919.1 ns | 823.4 ns | 1.0 µs | 1.5 µs † | 905.8 ns | 874.8 ns |
| sin | 0.567+0.125i | 18.6 µs | 14.8 µs | 24.3 µs | 62.4 µs † | 101.9 µs | 403.1 µs |
| sinc | 0.7+0.125i | 19.6 µs | 36.7 µs | 37.9 µs | 67.9 µs | 121.4 µs | 421.2 µs |
| sinh | 0.7+0.125i | 9.4 µs | 14.7 µs | 24.2 µs | 109.6 µs | 78.3 µs | 284.6 µs |
| sqr | 0.7+0.125i | 1.5 µs | 2.1 µs | 2.4 µs | 3.1 µs | 4.1 µs | 9.3 µs |
| sqrt | 1.23456789+0.125i | 2.3 µs | 3.3 µs | 3.5 µs | 4.4 µs | 6.5 µs | 17.5 µs |
| step | 0.7 | 844.0 ns | 962.5 ns | 914.7 ns | 808.2 ns | 820.3 ns | 950.4 ns |
| struve_H | 2, 0.7+0.125i | 110.8 µs | 263.3 µs | 414.8 µs | 662.7 µs | 1.941 ms | 9.663 ms |
| struve_L | 2, 0.7+0.125i | 109.1 µs | 219.2 µs | 359.2 µs | 521.7 µs | 1.850 ms | 10.540 ms |
| sub | 0.7+0.125i, 0.7+0.125i | 361.8 ns | 391.4 ns | 372.9 ns | 421.7 ns | 469.8 ns | 628.8 ns |
| tan | 0.7+0.125i | 13.4 µs | 19.2 µs | 30.4 µs | 42.2 µs | 111.3 µs | 539.5 µs |
| tanh | 0.7+0.125i | 27.4 µs | 23.1 µs | 71.3 µs | 78.6 µs † | 93.3 µs | 360.8 µs |
| tetragamma | 2.345+0.125i | 340.5 µs | 525.7 µs | 965.1 µs | 2.357 ms † | 4.797 ms | 28.991 ms † |
| tri | 0.7 | 2.3 µs | 2.8 µs | 3.0 µs | 5.3 µs † | 4.3 µs | 8.7 µs |
| trigamma | 2.345+0.125i | 249.5 µs | 389.8 µs | 772.0 µs | 3.713 ms † | 3.891 ms | 17.884 ms |
| vercos | 0.7+0.125i | 20.0 µs | 18.8 µs | 31.3 µs | 60.4 µs † | 103.9 µs | 460.2 µs |
| versin | 0.7+0.125i | 20.0 µs | 18.9 µs | 25.3 µs | 56.4 µs | 119.1 µs | 386.9 µs |
| zatahp | 2.3+0.125i, 1.4+0.125i | 2.621 ms | 4.752 ms | 9.912 ms | 915.0 µs | 3.958 ms | 13.921 ms |
| zatahp (integer order) | -4, 0.75+0.125i | 2.252 ms | 4.009 ms | 8.994 ms | 666.4 µs | 2.997 ms | 14.910 ms |
| zeta | 2.345+0.125i | 702.5 µs | 1.317 ms | 2.863 ms | 4.189 ms | 25.238 ms | 115.351 ms |
| zeta_pair | 2.345+0.125i | 755.2 µs | 1.459 ms | 2.900 ms | 4.835 ms | 24.295 ms | 130.709 ms |
| zetah | 2.3+0.125i, 1.4+0.125i | 2.146 ms | 4.558 ms | 8.565 ms | 884.1 µs † | 3.300 ms | 17.392 ms |
| zetap | 2.345+0.125i | 827.9 µs | 1.653 ms | 2.951 ms | 4.490 ms | 27.050 ms | 122.284 ms |

### number / unrestricted integer

| Operation | Inputs | Exact |
|---|---|---:|
| abs | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">-9712853409152031141207811585727780469484705121206232579607434236969009533618825<br/>97201918971499157056640605432269691427807258223374967278846596194834524005116336<br/>17514882293623272276764857474830401626415841035971458657442133370777607803464071<br/>101158385124186229995073045427448918227959893082282919863982307657776</span> | 92.5 ns |
| add | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 92.0 ns |
| bit_and | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 265.2 ns |
| bit_length | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 158.2 ns |
| bit_not | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 265.0 ns |
| bit_or | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 194.4 ns |
| bit_xor | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 209.2 ns |
| clear_bit | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>701</span> | 305.6 ns |
| div | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>340282366920938463463374607431768211454</span> | 1.1 µs |
| divmod | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 324.7 ns |
| factorial | 5000 | 109.2 µs |
| factors | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">54772119019154636339867862041163448790974484131459861520687327809209914339836012<br/>52275954799541486775711126005967382600118182797233708133775335788335658037065081<br/>31569062299976033041302189833215742992653063887339218309467131548996199881113600<br/>00000000000000000000000000000000000000000000000000000000000000000000000000000000<br/>0000000000000000000000000000000000000000000000</span> | 95.8 µs |
| fibonacci | 10000 | 6.8 µs |
| gcd | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 3.0 µs |
| gcdext | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 3.6 µs |
| is_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 453.4 µs |
| isqrt | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 363.1 ns |
| lcm | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 2.8 µs |
| mod | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 236.4 ns |
| modinv | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 3.6 µs |
| mul | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 206.1 ns |
| neg | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 67.1 ns |
| next_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 3.923 ms |
| partition | 5000 | 6.747 ms |
| pow | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>5</span> | 3.6 µs |
| pow_int | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>5</span> | 3.2 µs |
| powmod | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662, <br/>68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 100.4 µs |
| prev_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 5.085 ms |
| prove_prime | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">68647976601306097149819007990813932172694353001433054093944634591855431833976560<br/>52122559640661454554977296311391480858037121987999716643812574028291115057151</span> | 3.391 ms |
| set_bit | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>700</span> | 496.0 ns |
| shl | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>257</span> | 366.0 ns |
| shr | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>257</span> | 280.8 ns |
| sqr | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776</span> | 278.1 ns |
| sub | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>12498846048367045926370716602609982839721641429222959125376462582674869403443996<br/>972648628699047686839582654905384579341977328653591547736951147466435189662</span> | 106.4 ns |
| test_bit | <span style="display:inline-block;width:80ch;max-width:100%;white-space:normal;overflow-wrap:anywhere">97128534091520311412078115857277804694847051212062325796074342369690095336188259<br/>72019189714991570566406054322696914278072582233749672788465961948345240051163361<br/>75148822936232722767648574748304016264158410359714586574421333707776078034640711<br/>01158385124186229995073045427448918227959893082282919863982307657776, <br/>701</span> | 279.5 ns |

### number / unrestricted fraction

`number_t` uses exact GMP rational arithmetic (MPQ). The operands below have large integer
numerators and denominators; parsing and canonical reduction precede timing. Every result
is checked to remain an exact fraction. Arithmetic cost depends on operand sizes and
cancellation, rather than a floating-point precision. These workloads use a 16384-bit
rational size budget so intermediate results remain exact; general transcendental
functions usually require inexact real or complex results.

| Operation | Inputs | Exact |
|---|---|---:|
| abs | $\frac{\begin{array}{l}\text{-9712853409152031141207811585727780469484705121206232579607434236969009533618825} \\ \text{97201918971499157056640605432269691427807258223374967278846596194834524005116336} \\ \text{17514882293623272276764857474830401626415841035971458657442133370777607803464071} \\ \text{101158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$ | 195.8 ns |
| add | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>$\frac{\begin{array}{l}\text{12498846048367045926370716602609982839721641429222959125376462582674869403443996} \\ \text{972648628699047686839582654905384579341977328653591547736951147466435189662}\end{array}}{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}$ | 3.6 µs |
| div | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>$\frac{\begin{array}{l}\text{12498846048367045926370716602609982839721641429222959125376462582674869403443996} \\ \text{972648628699047686839582654905384579341977328653591547736951147466435189662}\end{array}}{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}$ | 6.3 µs |
| inv | $\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$ | 157.4 ns |
| mul | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>$\frac{\begin{array}{l}\text{12498846048367045926370716602609982839721641429222959125376462582674869403443996} \\ \text{972648628699047686839582654905384579341977328653591547736951147466435189662}\end{array}}{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}$ | 2.7 µs |
| neg | $\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$ | 166.1 ns |
| pow | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>5 | 27.6 µs |
| pow_int | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>5 | 25.8 µs |
| sqr | $\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$ | 434.8 ns |
| sub | **Input 1**<br/>$\frac{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}{\begin{array}{l}\text{68647976601306097149819007990813932172694353001433054093944634591855431833976560} \\ \text{52122559640661454554977296311391480858037121987999716643812574028291115057151}\end{array}}$<br/><br/>**Input 2**<br/>$\frac{\begin{array}{l}\text{12498846048367045926370716602609982839721641429222959125376462582674869403443996} \\ \text{972648628699047686839582654905384579341977328653591547736951147466435189662}\end{array}}{\begin{array}{l}\text{97128534091520311412078115857277804694847051212062325796074342369690095336188259} \\ \text{72019189714991570566406054322696914278072582233749672788465961948345240051163361} \\ \text{75148822936232722767648574748304016264158410359714586574421333707776078034640711} \\ \text{01158385124186229995073045427448918227959893082282919863982307657776}\end{array}}$ | 3.8 µs |

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
