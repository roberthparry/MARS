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
| `mc_exp(0.567 + 0.321i)` | `50.219 ms` | `67.201 ms` | `64.617 ms` | `99.976 ms` |
| `mc_log(0.567 + 0.321i)` | `3.800 ms` | `9.778 ms` | `12.840 ms` | `19.282 ms` |
| `mc_sqrt(0.567 + 0.321i)` | `0.376 ms` | `0.296 ms` | `0.281 ms` | `0.319 ms` |
| `mc_sin(0.567 + 0.321i)` | `364.620 ms` | `338.797 ms` | `540.834 ms` | `481.361 ms` |
| `mc_cos(0.567 + 0.321i)` | `265.815 ms` | `250.988 ms` | `457.393 ms` | `532.051 ms` |
| `mc_tan(0.567 + 0.321i)` | `126.637 ms` | `177.050 ms` | `211.020 ms` | `256.025 ms` |
| `mc_atan(0.321 + 0.123i)` | `24.396 ms` | `58.699 ms` | `45.628 ms` | `71.085 ms` |
| `mc_asin(0.321 + 0.123i)` | `3538.308 ms` | `7031.276 ms` | `4147.461 ms` | `5251.255 ms` |
| `mc_acos(0.321 + 0.123i)` | `2977.440 ms` | `9329.320 ms` | `4570.793 ms` | `4289.090 ms` |
| `mc_atan2(0.5 + 0.25i, -0.75 + 0.1i)` | `4411.470 ms` | `7728.407 ms` | `8446.029 ms` | `12651.549 ms` |
| `mc_sinh(0.567 + 0.321i)` | `216.454 ms` | `161.867 ms` | `256.865 ms` | `396.328 ms` |
| `mc_cosh(0.567 + 0.321i)` | `289.525 ms` | `172.518 ms` | `250.188 ms` | `463.782 ms` |
| `mc_tanh(0.567 + 0.321i)` | `117.243 ms` | `107.989 ms` | `150.179 ms` | `296.286 ms` |
| `mc_asinh(0.321 + 0.123i)` | `1495.263 ms` | `844.686 ms` | `1478.457 ms` | `1200.181 ms` |
| `mc_acosh(2 + 0.5i)` | `1223.775 ms` | `1122.482 ms` | `1569.504 ms` | `2308.499 ms` |
| `mc_atanh(0.321 + 0.123i)` | `37.828 ms` | `19.932 ms` | `30.703 ms` | `39.643 ms` |
| `mc_lambert_w0(1 + 1i)` | `233.987 ms` | `319.884 ms` | `682.927 ms` | `1012.807 ms` |
| `mc_lambert_wm1(-0.2 - 0.1i)` | `1386.027 ms` | `1190.273 ms` | `1166.291 ms` | `1525.229 ms` |
| `mc_productlog(1 + 1i)` | `365.950 ms` | `328.466 ms` | `739.202 ms` | `1262.173 ms` |
| `mc_gamma(1.5 + 0.7i)` | `253.993 ms` | `262.483 ms` | `390.972 ms` | `520.742 ms` |
| `mc_lgamma(1.5 + 0.7i)` | `253.526 ms` | `271.579 ms` | `364.726 ms` | `488.694 ms` |
| `mc_digamma(2 + 1i)` | `—` | `—` | `—` | `—` |
| `mc_trigamma(2 + 0.5i)` | `—` | `—` | `—` | `—` |
| `mc_tetragamma(2 + 0.5i)` | `—` | `—` | `—` | `—` |
| `mc_ei(1 + 1i)` | `—` | `—` | `—` | `—` |
| `mc_e1(1 + 1i)` | `—` | `—` | `—` | `—` |

For broader benchmark notes, see [`docs/benchmarks.md`](benchmarks.md).
