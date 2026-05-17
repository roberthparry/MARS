# `mcomplex_t`

`mcomplex_t` is MARS's opaque multiprecision complex type.

It stores:

- a real part as `mfloat_t`
- an imaginary part as `mfloat_t`
- a shared target precision

Conceptually:

```text
value = real + imag*i
```

with both components held at `mfloat_t` precision.

## Capabilities

- arbitrary-precision complex values backed by `mfloat_t`
- exact construction from:
  - `long` real values
  - `qcomplex_t`
  - decimal complex strings
  - explicit `mfloat_t` real and imaginary parts
- exact outward conversion to:
  - decimal strings
  - `qcomplex_t`
- in-place complex arithmetic:
  - add, subtract, multiply, divide
  - reciprocal
  - integer powers
  - square root
  - conjugation
- native `mfloat`-backed real-axis fast paths for a growing set of elementary
  and special functions
- formatted output through `%mz` and `%MZ`

## Constants

The subsystem exposes process-lifetime immortal constants:

- `MC_ZERO`
- `MC_ONE`
- `MC_HALF`
- `MC_TENTH`
- `MC_TEN`
- `MC_PI`
- `MC_E`
- `MC_LN10`
- `MC_EULER_MASCHERONI`
- `MC_SQRT2`
- `MC_SQRT_PI`
- `MC_NAN`
- `MC_INF`
- `MC_NINF`
- `MC_I`

They are exported as:

```c
extern const mcomplex_t * const MC_ZERO;
```

and must not be modified or freed by callers.

## Constructors And Core Lifecycle

Core constructors:

- `mc_new()`
- `mc_new_prec(bits)`
- `mc_create(real, imag)`
- `mc_create_long(real)`
- `mc_create_qcomplex(value)`
- `mc_create_string(text)`

Lifecycle helpers:

- `mc_clone(z)`
- `mc_clear(z)`
- `mc_free(z)`

## Precision Model

Each object carries a target precision shared by its real and imaginary parts.

- `mc_new()` uses the current `mfloat` default precision
- `mc_new_prec(bits)` creates a value with an explicit bit precision
- `mc_set_precision(...)` updates the target precision in bits
- `mc_set_precision_digits(...)` updates the target precision in significant
  decimal digits
- `mc_get_precision(...)` returns the target precision in bits
- `mc_get_precision_digits(...)` returns the target precision in significant
  decimal digits
- `mf_set_default_precision(...)` controls the default precision used by
  `mc_new()` and `mc_create_string()`
- `mf_set_default_precision_digits(...)` provides the same default setup in
  user-facing decimal digits

Because `mcomplex_t` is built on `mfloat_t`, callers can choose either binary
precision or user-facing significant digits depending on what is more natural.

## Parsing Policy

`mc_create_string(...)` and `mc_set_string(...)` parse literal complex values.

Accepted forms include:

- real literals like `"42"` or `"1e-23"`
- pure imaginary literals like `"3i"`, `"+i"`, or `"-i"`
- cartesian complex literals like `"1 + 2i"` or `"1 - i"`
- parenthesised imaginary coefficients like `"1e-23 + (2.3e12)i"`
- rational real or imaginary parts like `"1/2 - 3/2i"`

This parser does not evaluate expressions, so forms like `"1/i"` are rejected.

## Example

```c
#include <stdio.h>
#include "mcomplex.h"

int main(void) {
    mcomplex_t *z;
    char buf[256];

    mf_set_default_precision_digits(50);
    z = mc_create_string("1 + 1i");
    if (!z)
        return 1;

    if (mc_exp(z) != 0)
        return 1;

    mc_sprintf(buf, sizeof(buf), "%mz", z);
    printf("exp(1 + i) = %s\n", buf);

    mc_free(z);
    return 0;
}
```

```text
exp(1 + i) = 1.468693939915885157138967597326604261326956736629 + 2.287355287178842391208171906700501808955586256668i
```

## Formatting

`mcomplex_t` has its own print helpers:

- `mc_printf(...)`
- `mc_sprintf(...)`
- `mc_vsprintf(...)`

Supported specifiers:

- `%mz` — pretty fixed-format output
- `%MZ` — scientific-format output

Width and precision are applied to the real and imaginary parts individually.
Negative imaginary parts print as `re - imi`, not `re + -imi`.

## Queries And Conversion

Precision and setup:

- `mc_set_precision(z, bits)`
- `mc_get_precision(z)`
- `mc_set_precision_digits(z, digits)`
- `mc_get_precision_digits(z)`
- `mc_set(z, real, imag)`
- `mc_set_qcomplex(z, value)`
- `mc_set_string(z, text)`

Conversions:

- `mc_to_string(z)`
- `mc_to_qcomplex(z)`

Component access:

- `mc_real(z)`
- `mc_imag(z)`

Predicates and comparisons:

- `mc_is_zero(z)`
- `mc_eq(a, b)`
- `mc_isnan(z)`
- `mc_isinf(z)`
- `mc_isposinf(z)`
- `mc_isneginf(z)`

## Arithmetic Surface

Basic arithmetic:

- `mc_abs`
- `mc_neg`
- `mc_conj`
- `mc_add`
- `mc_sub`
- `mc_mul`
- `mc_div`
- `mc_inv`
- `mc_pow_int`
- `mc_pow`
- `mc_ldexp`
- `mc_sqrt`
- `mc_floor`
- `mc_hypot`

Elementary and special functions:

- `mc_exp`
- `mc_log`
- `mc_log10`
- `mc_sin`
- `mc_cos`
- `mc_tan`
- `mc_atan`
- `mc_atan2`
- `mc_asin`
- `mc_acos`
- `mc_sinh`
- `mc_cosh`
- `mc_tanh`
- `mc_asinh`
- `mc_acosh`
- `mc_atanh`
- `mc_gamma`
- `mc_erf`
- `mc_erfc`
- `mc_erfinv`
- `mc_erfcinv`
- `mc_lgamma`
- `mc_digamma`
- `mc_trigamma`
- `mc_tetragamma`
- `mc_gammainv`
- `mc_lambert_w0`
- `mc_lambert_wm1`
- `mc_beta`
- `mc_logbeta`
- `mc_binomial`
- `mc_beta_pdf`
- `mc_logbeta_pdf`
- `mc_normal_pdf`
- `mc_normal_cdf`
- `mc_normal_logpdf`
- `mc_productlog`
- `mc_gammainc_lower`
- `mc_gammainc_upper`
- `mc_gammainc_P`
- `mc_gammainc_Q`
- `mc_ei`
- `mc_e1`

## Performance Notes

Recent work has focused on making `256`-bit and `512`-bit `mcomplex` practical
for real workloads:

- native `mfloat`-backed arithmetic now covers the core complex operators
- real-axis fast paths avoid unnecessary round-trips through `qcomplex_t`
- exact shortcuts are in place for several common special values
- focused benchmark cases now track the hot functions we have been tuning

There is now a dedicated `mcomplex` benchmark binary:

```sh
make bench_mcomplex_maths
```

## Testing

From the repository root:

```sh
make tests/build/release/mcomplex/test_mcomplex
tests/build/release/mcomplex/test_mcomplex
```

## Internal Layout

`mcomplex` is split into:

- `mcomplex_core.c`
- `mcomplex_arith.c`
- `mcomplex_maths.c`
- `mcomplex_string.c`
- `mcomplex_print.c`

with shared private declarations in `src/mcomplex/mcomplex_internal.h`.

The public surface remains in `include/mcomplex.h`.

## Benchmark Coverage

Focused targets:

```sh
make tests/build/release/mcomplex/test_mcomplex
tests/build/release/mcomplex/test_mcomplex

make bench_mcomplex_maths
```

The test suite includes:

- lifecycle and constants
- `qcomplex` conversion coverage
- arithmetic checks
- real-axis elementary and special-function replacements
- strict difficult branch and identity cases

The benchmark suite tracks the same hot spots we have been using during the
native replacement work, including:

- `exp(1+i)` and `log(1+i)`
- `productlog(1+i)`
- pure-real `gamma(2.3 + 0i)` and `lgamma(2.3 + 0i)`
- related special-function probes such as `ei(1+i)` and `e1(1+i)`

Benchmark source:

- [`bench/mcomplex/bench_mcomplex_maths.c`](../bench/mcomplex/bench_mcomplex_maths.c)

Run it from the repository root with:

```sh
make bench_mcomplex_maths
MARS_BENCH_SCALE=5 ./build/release/bench/mcomplex/bench_mcomplex_maths
MARS_BENCH_FORMAT=md ./build/release/bench/mcomplex/bench_mcomplex_maths
```

Current sample results from a fresh local run on this tree, measured on:

- `Linux x86_64`
- kernel `6.8.0-110-generic`
- `Intel(R) Core(TM) i7-4510U CPU @ 2.00GHz`
- `4` logical CPUs

Markdown output uses the release benchmark with `51` timed batches per case,
records the sample median for each row, and keeps at least `3` inner
iterations even for the slowest docs-mode rows.

Results with genuinely complex inputs, limited to rows we have measured cleanly
on the current native `mcomplex` implementation:

| Case | `256` bits | `512` bits | `768` bits | `1024` bits | `2048` bits | `4096` bits |
|---|---:|---:|---:|---:|---:|---:|
| `mc_exp(0.567 + 0.321i)` | `30.3 µs` | `69.7 µs` | `119.6 µs` | `178.0 µs` | `538.3 µs` | `1.820 ms` |
| `mc_log(0.567 + 0.321i)` | `72.6 µs` | `118.1 µs` | `150.6 µs` | `219.1 µs` | `472.0 µs` | `1.098 ms` |
| `mc_sqrt(0.567 + 0.321i)` | `4.4 µs` | `6.0 µs` | `7.5 µs` | `9.6 µs` | `16.2 µs` | `34.4 µs` |
| `mc_sin(0.567 + 0.321i)` | `26.8 µs` | `46.8 µs` | `72.8 µs` | `104.5 µs` | `291.6 µs` | `864.7 µs` |
| `mc_cos(0.567 + 0.321i)` | `26.9 µs` | `46.8 µs` | `72.6 µs` | `104.2 µs` | `291.1 µs` | `866.0 µs` |
| `mc_tan(0.567 + 0.321i)` | `37.2 µs` | `60.3 µs` | `91.6 µs` | `126.6 µs` | `328.9 µs` | `1.072 ms` |
| `mc_atan(0.321 + 0.123i)` | `150.5 µs` | `247.2 µs` | `344.4 µs` | `479.5 µs` | `962.5 µs` | `2.578 ms` |
| `mc_asin(0.321 + 0.123i)` | `3.7 ms` | `1.6 ms` | `4.3 ms` | `5.1 ms` | `12.9 ms` | `101.9 ms` |
| `mc_acos(0.321 + 0.123i)` | `1.4 ms` | `1.7 ms` | `3.9 ms` | `2.6 ms` | `12.7 ms` | `57.4 ms` |
| `mc_atan2(0.5 + 0.25i, -0.75 + 0.1i)` | `176.2 µs` | `247.5 µs` | `392.9 µs` | `532.3 µs` | `1.030 ms` | `2.739 ms` |
| `mc_sinh(0.567 + 0.321i)` | `26.5 µs` | `46.8 µs` | `73.0 µs` | `99.9 µs` | `300.9 µs` | `1.238 ms` |
| `mc_cosh(0.567 + 0.321i)` | `26.5 µs` | `46.7 µs` | `72.9 µs` | `101.5 µs` | `301.8 µs` | `945.7 µs` |
| `mc_tanh(0.567 + 0.321i)` | `36.7 µs` | `60.9 µs` | `89.4 µs` | `122.0 µs` | `345.6 µs` | `1.698 ms` |
| `mc_asinh(0.321 + 0.123i)` | `649.0 µs` | `1.613 ms` | `3.125 ms` | `5.147 ms` | `24.706 ms` | `102.206 ms` |
| `mc_acosh(2 + 0.5i)` | `84.2 µs` | `140.7 µs` | `181.9 µs` | `258.1 µs` | `499.2 µs` | `1.249 ms` |
| `mc_atanh(0.321 + 0.123i)` | `133.8 µs` | `235.1 µs` | `317.5 µs` | `479.5 µs` | `1.715 ms` | `2.456 ms` |
| `mc_lambert_w0(1 + 1i)` | `206.9 µs` | `439.7 µs` | `643.0 µs` | `1.134 ms` | `2.965 ms` | `10.901 ms` |
| `mc_lambert_wm1(-0.2 - 0.1i)` | `213.1 µs` | `463.8 µs` | `674.3 µs` | `1.195 ms` | `3.291 ms` | `11.082 ms` |
| `mc_productlog(1 + 1i)` | `204.4 µs` | `442.4 µs` | `638.6 µs` | `1.131 ms` | `2.896 ms` | `10.818 ms` |
| `mc_gamma(1.5 + 0.7i)` | `20.1 µs` | `20.5 µs` | `20.3 µs` | `20.9 µs` | `21.0 µs` | `23.2 µs` |
| `mc_lgamma(1.5 + 0.7i)` | `18.1 µs` | `18.2 µs` | `18.3 µs` | `18.8 µs` | `18.5 µs` | `19.4 µs` |
| `mc_digamma(2 + 1i)` | `19.7 µs` | `19.3 µs` | `19.4 µs` | `19.9 µs` | `20.0 µs` | `20.3 µs` |
| `mc_trigamma(2 + 0.5i)` | `14.4 µs` | `9.3 µs` | `9.4 µs` | `9.9 µs` | `10.0 µs` | `10.3 µs` |
| `mc_tetragamma(2 + 0.5i)` | `12.5 µs` | `11.0 µs` | `11.0 µs` | `11.6 µs` | `11.0 µs` | `12.1 µs` |
| `mc_ei(1 + 1i)` | `55.1 µs` | `55.1 µs` | `54.4 µs` | `54.6 µs` | `56.2 µs` | `55.2 µs` |
| `mc_e1(1 + 1i)` | `58.6 µs` | `59.2 µs` | `58.3 µs` | `59.5 µs` | `60.0 µs` | `59.1 µs` |

For broader benchmark notes, see [`docs/benchmarks.md`](benchmarks.md).





