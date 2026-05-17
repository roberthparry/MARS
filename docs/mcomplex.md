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
```

Current sample results from a fresh local run on this tree, measured on:

- `Linux x86_64`
- kernel `6.8.0-110-generic`
- `Intel(R) Core(TM) i7-4510U CPU @ 2.00GHz`
- `4` logical CPUs

Results with genuinely complex inputs, limited to rows we have measured cleanly
on the current native `mcomplex` implementation:

| Case | `256` bits | `512` bits | `768` bits | `1024` bits |
|---|---:|---:|---:|---:|
| `mc_exp(0.567 + 0.321i)` | `23.137 µs` | `21.237 µs` | `38.396 µs` | `59.296 µs` |
| `mc_log(0.567 + 0.321i)` | `47.274 µs` | `36.909 µs` | `55.903 µs` | `72.414 µs` |
| `mc_sqrt(0.567 + 0.321i)` | `2.835 µs` | `2.034 µs` | `2.535 µs` | `3.358 µs` |
| `mc_sin(0.567 + 0.321i)` | `18.295 µs` | `14.836 µs` | `22.800 µs` | `34.840 µs` |
| `mc_cos(0.567 + 0.321i)` | `17.290 µs` | `16.324 µs` | `21.911 µs` | `32.518 µs` |
| `mc_tan(0.567 + 0.321i)` | `25.668 µs` | `18.081 µs` | `27.527 µs` | `43.040 µs` |
| `mc_atan(0.321 + 0.123i)` | `97.774 µs` | `76.178 µs` | `106.066 µs` | `151.619 µs` |
| `mc_asin(0.321 + 0.123i)` | `385.135 µs` | `579.179 µs` | `1038.105 µs` | `1763.441 µs` |
| `mc_acos(0.321 + 0.123i)` | `240.762 µs` | `829.935 µs` | `1021.245 µs` | `1587.481 µs` |
| `mc_atan2(0.5 + 0.25i, -0.75 + 0.1i)` | `114.100 µs` | `83.223 µs` | `115.801 µs` | `162.695 µs` |
| `mc_sinh(0.567 + 0.321i)` | `10.327 µs` | `16.449 µs` | `23.520 µs` | `68.715 µs` |
| `mc_cosh(0.567 + 0.321i)` | `9.570 µs` | `25.716 µs` | `22.017 µs` | `75.791 µs` |
| `mc_tanh(0.567 + 0.321i)` | `13.063 µs` | `36.992 µs` | `27.253 µs` | `81.414 µs` |
| `mc_asinh(0.321 + 0.123i)` | `233.018 µs` | `551.040 µs` | `1011.296 µs` | `3111.123 µs` |
| `mc_acosh(2 + 0.5i)` | `30.630 µs` | `83.724 µs` | `55.782 µs` | `164.985 µs` |
| `mc_atanh(0.321 + 0.123i)` | `47.050 µs` | `91.844 µs` | `96.921 µs` | `310.425 µs` |
| `mc_lambert_w0(1 + 1i)` | `69.440 µs` | `146.264 µs` | `222.191 µs` | `491.194 µs` |
| `mc_lambert_wm1(-0.2 - 0.1i)` | `76.473 µs` | `151.014 µs` | `230.489 µs` | `463.460 µs` |
| `mc_productlog(1 + 1i)` | `91.017 µs` | `144.221 µs` | `202.022 µs` | `418.586 µs` |
| `mc_gamma(1.5 + 0.7i)` | `11.455 µs` | `10.290 µs` | `10.197 µs` | `11.625 µs` |
| `mc_lgamma(1.5 + 0.7i)` | `8.740 µs` | `8.810 µs` | `8.828 µs` | `9.998 µs` |
| `mc_digamma(2 + 1i)` | `10.510 µs` | `9.742 µs` | `9.785 µs` | `15.055 µs` |
| `mc_trigamma(2 + 0.5i)` | `3.723 µs` | `3.765 µs` | `3.799 µs` | `4.413 µs` |
| `mc_tetragamma(2 + 0.5i)` | `5.634 µs` | `4.570 µs` | `4.611 µs` | `5.330 µs` |
| `mc_ei(1 + 1i)` | `31.479 µs` | `31.214 µs` | `29.853 µs` | `33.287 µs` |
| `mc_e1(1 + 1i)` | `32.661 µs` | `31.427 µs` | `30.100 µs` | `33.631 µs` |

For broader benchmark notes, see [`docs/benchmarks.md`](benchmarks.md).
