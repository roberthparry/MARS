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
| `mc_exp(0.567 + 0.321i)` | `25.188 ms` | `47.681 ms` | `50.811 ms` | `88.553 ms` |
| `mc_log(0.567 + 0.321i)` | `1.884 ms` | `7.492 ms` | `7.873 ms` | `19.746 ms` |
| `mc_sqrt(0.567 + 0.321i)` | `0.146 ms` | `0.223 ms` | `0.151 ms` | `0.225 ms` |
| `mc_gamma(1.5 + 0.7i)` | `234.588 ms` | `210.509 ms` | `318.797 ms` | `528.668 ms` |
| `mc_lgamma(1.5 + 0.7i)` | `115.514 ms` | `180.500 ms` | `287.406 ms` | `428.251 ms` |
| `mc_sin(0.567 + 0.321i)` | `213.353 ms` | `231.506 ms` | `415.407 ms` | `495.885 ms` |
| `mc_cos(0.567 + 0.321i)` | `183.085 ms` | `241.061 ms` | `653.262 ms` | `656.497 ms` |
| `mc_tan(0.567 + 0.321i)` | `87.707 ms` | `141.090 ms` | `285.126 ms` | `282.177 ms` |
| `mc_atan(0.321 + 0.123i)` | `16.494 ms` | `45.587 ms` | `58.711 ms` | `90.792 ms` |
| `mc_asin(0.321 + 0.123i)` | `3019.372 ms` | `4790.967 ms` | `4271.541 ms` | `5269.515 ms` |
| `mc_acos(0.321 + 0.123i)` | `2547.742 ms` | `9478.670 ms` | `4906.479 ms` | `4487.979 ms` |
| `mc_atan2(0.5 + 0.25i, -0.75 + 0.1i)` | `3222.167 ms` | `5548.154 ms` | `9477.184 ms` | `13505.528 ms` |
| `mc_sinh(0.567 + 0.321i)` | `233.555 ms` | `149.351 ms` | `348.086 ms` | `363.259 ms` |
| `mc_cosh(0.567 + 0.321i)` | `170.854 ms` | `235.618 ms` | `310.209 ms` | `374.846 ms` |
| `mc_tanh(0.567 + 0.321i)` | `85.775 ms` | `124.224 ms` | `135.241 ms` | `254.067 ms` |
| `mc_asinh(0.321 + 0.123i)` | `1414.516 ms` | `1189.113 ms` | `1870.392 ms` | `1876.379 ms` |
| `mc_acosh(2 + 0.5i)` | `955.470 ms` | `1424.566 ms` | `2049.363 ms` | `3197.080 ms` |
| `mc_atanh(0.321 + 0.123i)` | `16.632 ms` | `23.110 ms` | `30.002 ms` | `27.889 ms` |
| `mc_lambert_w0(1 + 1i)` | `183.363 ms` | `281.232 ms` | `726.151 ms` | `835.125 ms` |
| `mc_lambert_wm1(-0.2 - 0.1i)` | `713.656 ms` | `1533.902 ms` | `1781.926 ms` | `1632.533 ms` |
| `mc_productlog(1 + 1i)` | `264.177 ms` | `307.523 ms` | `802.481 ms` | `836.262 ms` |

For broader benchmark notes, see [`docs/benchmarks.md`](benchmarks.md).
