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
- `a/b` fraction text, Unicode fraction glyphs, or stacked Unicode fractions -> `mrational_t`
- decimal or scientific real text -> `mfloat_t`
- complex text -> `mcomplex_t`

Examples:

- `"42"` -> exact integer
- `"5/6"` -> exact rational
- `"⅚"` -> exact rational
- `"³⁵⁵⁄₁₁₃"` -> exact rational
- `"32.123"` -> multiprecision real
- `"1 + 2i"` -> multiprecision complex

Accepted examples:

- `"1 + i"` -> parsed as `1 + 1i`
- `"1 - i"` -> parsed as `1 - 1i`
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

Named constants such as `NUM_PI`, `NUM_E`, `NUM_LN10`, `NUM_PHI`, and
`NUM_EULER_MASCHERONI` are safe to clear as well:

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
num_scope_t *scope = num_scope_enter();
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
  - `num_log10`
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
2 + 5/6 = ¹⁷⁄₆
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
- `2048` bits
- `4096` bits

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
mfloat chain             manual= 129.082 ms  scoped= 125.772 ms  ratio= 1.026x
mfloat scoped+roll       manual= 129.082 ms  scoped= 110.554 ms  ratio= 1.168x
mcomplex chain           manual= 221.584 ms  scoped= 225.807 ms  ratio= 0.981x
mcomplex scoped+roll     manual= 221.584 ms  scoped= 198.489 ms  ratio= 1.116x
```

Run it from the repository root with:

```sh
make bench_number_maths
MARS_BENCH_SCALE=10 ./build/release/bench/number/bench_number_maths
MARS_BENCH_FORMAT=md ./build/release/bench/number/bench_number_maths
```

Current sample results from that command on this tree, measured on:

- `Linux x86_64`
- kernel `6.8.0-110-generic`
- `Intel(R) Core(TM) i7-4510U CPU @ 2.00GHz`
- `4` logical CPUs

Markdown output uses the release benchmark with `51` timed batches per case,
records the sample median for each row, and keeps at least `3` inner
iterations even for the slowest docs-mode rows.

Results (microseconds per call):

| Case | `256` bits | `512` bits | `768` bits | `1024` bits | `2048` bits | `4096` bits |
|---|---:|---:|---:|---:|---:|---:|
| `num_exp(1.23456789)` | `10.4 µs` | `20.2 µs` | `33.3 µs` | `48.6 µs` | `138.2 µs` | `469.7 µs` |
| `num_log(2.345678)` | `18.0 µs` | `28.8 µs` | `40.6 µs` | `55.7 µs` | `60.2 µs` | `317.4 µs` |
| `num_sqrt(1.23456789)` | `1.2 µs` | `1.7 µs` | `2.0 µs` | `2.5 µs` | `2.0 µs` | `8.6 µs` |
| `num_sin(0.567)` | `11.1 µs` | `19.9 µs` | `29.2 µs` | `41.1 µs` | `46.3 µs` | `300.1 µs` |
| `num_cos(0.7)` | `9.1 µs` | `16.6 µs` | `25.4 µs` | `36.1 µs` | `42.8 µs` | `273.6 µs` |
| `num_sincos(0.7)` | `11.8 µs` | `20.6 µs` | `30.1 µs` | `41.9 µs` | `46.6 µs` | `298.2 µs` |
| `num_tan(0.7)` | `13.0 µs` | `22.0 µs` | `31.7 µs` | `44.1 µs` | `50.0 µs` | `309.9 µs` |
| `num_atan(0.7)` | `50.2 µs` | `84.8 µs` | `108.8 µs` | `161.5 µs` | `165.6 µs` | `943.6 µs` |
| `num_asin(0.7)` | `62.2 µs` | `96.4 µs` | `125.6 µs` | `182.3 µs` | `170.4 µs` | `922.6 µs` |
| `num_acos(0.7)` | `63.2 µs` | `96.1 µs` | `129.9 µs` | `186.2 µs` | `168.9 µs` | `915.5 µs` |
| `num_atan2(0.5,-0.75)` | `48.5 µs` | `88.4 µs` | `116.5 µs` | `169.2 µs` | `166.0 µs` | `883.9 µs` |
| `num_sinh(0.7)` | `10.3 µs` | `20.2 µs` | `33.0 µs` | `45.6 µs` | `65.2 µs` | `418.5 µs` |
| `num_cosh(0.7)` | `10.3 µs` | `20.0 µs` | `32.8 µs` | `45.6 µs` | `65.0 µs` | `416.5 µs` |
| `num_sinhcosh(0.7)` | `10.9 µs` | `20.6 µs` | `33.4 µs` | `46.1 µs` | `65.3 µs` | `418.0 µs` |
| `num_tanh(0.7)` | `10.8 µs` | `21.0 µs` | `33.6 µs` | `45.4 µs` | `66.1 µs` | `416.7 µs` |
| `num_asinh(0.5)` | `20.3 µs` | `32.6 µs` | `44.9 µs` | `59.9 µs` | `62.6 µs` | `299.9 µs` |
| `num_acosh(2)` | `20.0 µs` | `32.6 µs` | `44.8 µs` | `59.9 µs` | `62.5 µs` | `299.4 µs` |
| `num_atanh(0.5)` | `20.2 µs` | `32.7 µs` | `45.2 µs` | `60.5 µs` | `65.2 µs` | `315.7 µs` |
| `num_lambert_w0(0.7)` | `145.2 µs` | `265.5 µs` | `437.7 µs` | `706.3 µs` | `1.788 ms` | `3.371 ms` |
| `num_lambert_wm1(-0.2)` | `166.0 µs` | `310.7 µs` | `490.7 µs` | `715.3 µs` | `1.964 ms` | `4.369 ms` |
| `num_gamma(2.345)` | `127.4 µs` | `314.4 µs` | `572.3 µs` | `912.2 µs` | `1.502 ms` | `11.773 ms` |
| `num_lgamma(2.345)` | `133.6 µs` | `320.7 µs` | `574.4 µs` | `920.4 µs` | `3.013 ms` | `11.805 ms` |
| `num_digamma(2.345)` | `332.9 µs` | `827.4 µs` | `1.524 ms` | `1.699 ms` | `6.677 ms` | `45.697 ms` |
| `num_trigamma(2.345)` | `1.629 ms` | `974.1 µs` | `1.301 ms` | `1.593 ms` | `2.894 ms` | `7.118 ms` |
| `num_tetragamma(2.345)` | `803.1 µs` | `1.156 ms` | `1.445 ms` | `1.801 ms` | `3.171 ms` | `4.119 ms` |
| `num_ei(5)` | `184.5 µs` | `304.5 µs` | `420.3 µs` | `542.4 µs` | `1.082 ms` | `1.282 ms` |
| `num_e1(5)` | `386.5 µs` | `630.2 µs` | `880.3 µs` | `1.124 ms` | `1.081 ms` | `1.481 ms` |

For broader benchmark notes, see [`docs/benchmarks.md`](benchmarks.md).




