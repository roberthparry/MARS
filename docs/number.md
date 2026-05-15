# `number_t`

`number_t` is MARS's generic numeric value cluster.

It gives the library one by-value public numeric handle that can represent:

- exact integers via `mint_t`
- exact rationals via `mrational_t`
- fixed-precision real values via `double` and `qfloat_t`
- fixed-precision complex values via `qcomplex_t`
- multiprecision real values via `mfloat_t`
- multiprecision complex values via `mcomplex_t`

The goal is to let callers work at the level of "a number" without having to
manually pick a backend for every operation.

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

- integer text -> `mint_t`
- `a/b` fraction text -> `mrational_t`
- decimal or scientific real text -> `mfloat_t`
- complex text -> `mcomplex_t`

Examples:

- `"42"` -> exact integer
- `"5/6"` -> exact rational
- `"32.123"` -> multiprecision real
- `"1 + 2i"` -> multiprecision complex

Accepted examples:

- `"1 + i"` -> parsed as `1 + 1i`
- `"1 - i"` -> parsed as `1 - 1i`
- `"1e-23 + 2.3e12i"` -> scientific notation in both parts is accepted
- `"1e-23 + (2.3e12)i"` -> parenthesized imaginary coefficients are accepted
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

- `num_create_from_string(...)` for decimal/scientific real and complex input
- `num_create_from_mfloat(...)`
- `num_create_from_mcomplex(...)`

Explicit precision constructors are also available:

- `num_create_from_mfloat_with_prec_bits(...)`
- `num_create_from_mfloat_with_prec_digits(...)`
- `num_create_from_mcomplex_with_prec_bits(...)`
- `num_create_from_mcomplex_with_prec_digits(...)`

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
number_t a = num_create_from_string("2");
number_t b = num_create_from_string("5/6");
number_t c = num_add(a, b);

num_destroy(&a);
num_destroy(&b);
num_destroy(&c);
```

Example with explicit precision:

```c
num_set_default_prec_bits(768);

number_t x = num_create_from_string("1.25");
number_t y = num_create_from_mfloat_with_prec_bits(MF_PI, 512);

printf("default bits: %zu\n", num_get_default_prec_bits());
printf("x bits: %zu\n", num_get_prec_bits(x));
printf("y bits: %zu\n", num_get_prec_bits(y));

num_destroy(&x);
num_destroy(&y);
```

Named constants such as `NUM_PI`, `NUM_E`, `NUM_PHI`, and `NUM_EULER_MASCHERONI`
are safe to clear as well:

```c
number_t pi = NUM_PI;
num_destroy(&pi);
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

Detaching behaves differently depending on how the scoped value is stored:

- plain heap-backed scoped values are transferred out of the scope directly
- arena-backed scoped values are copied into an ordinary owning value so they
  can survive scope teardown safely

Example with the public cleanup macro:

```c
NUM_SCOPE(scope);

number_t a = num_create_from_string("1/3");
number_t b = num_create_from_string("1/6");
number_t sum = num_add(a, b);
number_t kept = num_scope_detach(sum);

/* kept is still live here */
num_destroy(&kept);
num_destroy(&b);
num_destroy(&a);
```

If you need a portable manual form instead of `NUM_SCOPE(...)`, the equivalent
sequence is:

```c
num_scope_t scope = {0};
num_scope_enter(&scope);
/* ... */
num_scope_leave(&scope);
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

- `num_to_string(...)` returns a newly allocated string that must be released
  with `free()`.
- `num_sprintf(...)`, `num_vsprintf(...)`, and `num_printf(...)` follow the
  usual `snprintf` / `printf` return conventions.

Supported format specifiers:

- `%n` — pretty human-readable form
- `%N` — scientific form for floating-point and complex values, otherwise the
  ordinary exact form

Examples:

- integer and rational values print exactly:
  - `42`
  - `5/6`
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

## Arithmetic And Functions

The generic layer exposes:

- core arithmetic:
  - `num_add`
  - `num_sub`
  - `num_mul`
  - `num_div`
  - `num_inv`
  - `num_neg`
- real/complex helpers:
  - `num_abs`
  - `num_conj`
  - `num_real_part`
  - `num_imag_part`
  - `num_arg`
- elementary functions:
  - `num_exp`
  - `num_log`
  - `num_sqrt`
  - `num_sin`, `num_cos`, `num_tan`
  - `num_sinh`, `num_cosh`, `num_tanh`
  - `num_atan2`
- special functions:
  - `num_gamma`
  - `num_lgamma`
  - `num_digamma`
  - `num_trigamma`
  - `num_erf`
  - `num_erfc`
  - `num_lambert_w0`
  - `num_beta`
  - `num_logbeta`
  - `num_binomial`
  - `num_normal_pdf`
  - `num_normal_cdf`
  - `num_e1`
  - `num_ei`

Binary arithmetic mixes supported backends automatically by promoting to a
common target representation before dispatch.

Return conventions in this area:

- functions returning `number_t` return an owning by-value result
- the caller should later pass that result to `num_destroy(...)`
- paired-output helpers such as `num_sincos(...)` and `num_sinhcosh(...)`
  return an `int` status code, with `0` for success and `-1` for invalid input

## Example

```c
#include <stdio.h>
#include <stdlib.h>
#include "number.h"

int main(void) {
    number_t a = num_create_from_string("2");
    number_t b = num_create_from_string("3");
    number_t c = num_create_from_string("5/6");
    number_t sum = num_add(a, c);
    number_t beta = num_beta(a, b);
    char *sum_text = num_to_string(sum);
    char *beta_text = num_to_string(beta);

    if (!sum_text || !beta_text)
        return 1;

    printf("2 + 5/6 = %s\n", sum_text);
    printf("beta(2, 3) = %s\n", beta_text);

    free(sum_text);
    free(beta_text);
    num_destroy(&a);
    num_destroy(&b);
    num_destroy(&c);
    num_destroy(&sum);
    num_destroy(&beta);
    return 0;
}
```

```text
2 + 5/6 = 17/6
beta(2, 3) = 0.083333333333333333333333333333333
```

## Benchmarks

The generic numeric layer has a matching benchmark target:

```sh
make bench_number_maths
```

It mirrors the main `mfloat` maths benchmark through the public `number_t` API,
so it measures both backend maths cost and generic promotion/dispatch overhead
on the same representative workload.

## Benchmark Coverage

The dedicated `number_t` maths benchmark includes matching timing cases at:

- `256` bits
- `512` bits
- `768` bits
- `1024` bits

across the same broad slice used for the native `mfloat` benchmark.

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
mfloat chain             manual=1450.786 ms  scoped=1977.505 ms  ratio= 0.734x
mfloat scoped+roll       manual=1450.786 ms  scoped=1071.234 ms  ratio= 1.354x
mcomplex chain           manual= 422.498 ms  scoped= 968.822 ms  ratio= 0.436x
mcomplex scoped+roll     manual= 422.498 ms  scoped= 332.915 ms  ratio= 1.269x
```

Run it from the repository root with:

```sh
make bench_number_maths
MARS_BENCH_SCALE=10 ./build/release/bench/number/bench_number_maths
```

Current sample results from that command on this tree, measured on:

- `Linux x86_64`
- kernel `6.8.0-110-generic`
- `Intel(R) Core(TM) i7-4510U CPU @ 2.00GHz`
- `4` logical CPUs

Results:

| Case | `256` bits | `512` bits | `768` bits | `1024` bits |
|---|---:|---:|---:|---:|
| `num_exp(1.23456789)` | `2.049 ms` | `8.041 ms` | `5.437 ms` | `4.494 ms` |
| `num_log(2.345678)` | `1.009 ms` | `2.058 ms` | `2.433 ms` | `3.683 ms` |
| `num_sqrt(1.23456789)` | `0.016 ms` | `0.023 ms` | `0.014 ms` | `0.019 ms` |
| `num_sin(0.567)` | `3.004 ms` | `11.982 ms` | `8.111 ms` | `19.987 ms` |
| `num_cos(0.567)` | `1.681 ms` | `5.137 ms` | `4.493 ms` | `10.435 ms` |
| `num_sincos(0.7)` | `6.188 ms` | `10.375 ms` | `11.204 ms` | `17.567 ms` |
| `num_tan(0.7)` | `0.783 ms` | `2.169 ms` | `10.881 ms` | `17.654 ms` |
| `num_atan(0.567)` | `0.830 ms` | `2.499 ms` | `2.753 ms` | `3.795 ms` |
| `num_asin(0.7)` | `2.900 ms` | `11.639 ms` | `4.877 ms` | `13.005 ms` |
| `num_acos(0.7)` | `2.900 ms` | `11.563 ms` | `4.889 ms` | `19.099 ms` |
| `num_atan2(0.5,-0.75)` | `1.026 ms` | `2.258 ms` | `2.582 ms` | `6.191 ms` |
| `num_sinh(0.7)` | `3.050 ms` | `7.868 ms` | `2.995 ms` | `6.757 ms` |
| `num_cosh(0.7)` | `3.065 ms` | `8.015 ms` | `3.000 ms` | `7.466 ms` |
| `num_sinhcosh(0.7)` | `3.067 ms` | `7.606 ms` | `2.997 ms` | `7.872 ms` |
| `num_tanh(0.7)` | `3.002 ms` | `8.016 ms` | `2.977 ms` | `7.964 ms` |
| `num_asinh(0.5)` | `1.269 ms` | `3.240 ms` | `2.727 ms` | `4.391 ms` |
| `num_acosh(2)` | `1.122 ms` | `2.038 ms` | `1.749 ms` | `2.397 ms` |
| `num_atanh(0.5)` | `1.501 ms` | `4.415 ms` | `3.012 ms` | `4.049 ms` |
| `num_lambert_w0(0.7)` | `57.527 ms` | `67.351 ms` | `89.971 ms` | `103.493 ms` |
| `num_lambert_wm1(-0.2)` | `82.290 ms` | `73.982 ms` | `131.892 ms` | `318.031 ms` |
| `num_gamma(2.345)` | `30.452 ms` | `42.240 ms` | `55.464 ms` | `230.718 ms` |
| `num_lgamma(2.345)` | `29.102 ms` | `31.248 ms` | `53.431 ms` | `219.636 ms` |
| `num_digamma(2.345)` | `2.883 ms` | `3.774 ms` | `14.578 ms` | `11.921 ms` |
| `num_trigamma(2.345)` | `2.391 ms` | `2.447 ms` | `10.856 ms` | `5.597 ms` |
| `num_tetragamma(2.345)` | `3.417 ms` | `2.669 ms` | `8.462 ms` | `5.318 ms` |
| `num_ei(1)` | `4.402 ms` | `4.108 ms` | `48.975 ms` | `709.675 ms` |
| `num_e1(1)` | `4.673 ms` | `4.274 ms` | `28.783 ms` | `31.516 ms` |

For broader benchmark notes, see [`docs/benchmarks.md`](benchmarks.md).
