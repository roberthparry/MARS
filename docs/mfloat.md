# `mfloat_t`

`mfloat_t` is MARS's opaque multiprecision floating-point type.

It stores:

- a finite / NaN / infinity kind
- a sign
- a binary exponent
- a normalised arbitrary-precision mantissa
- a target precision in bits

Conceptually:

```text
value = sign * mantissa * 2^exponent2
```

with the mantissa held as an exact `mint_t` magnitude internally.

## Capabilities

- arbitrary-precision binary floating-point values
- exact construction from:
  - `long`
  - `double`
  - `qfloat_t`
  - decimal strings
- exact outward conversion to:
  - decimal strings
  - `double`
  - `qfloat_t`
- in-place arithmetic:
  - add, subtract, multiply, divide
  - reciprocal
  - integer powers
  - square root
  - `ldexp`
- native multiprecision implementations for:
  - `exp`
  - `log`
  - `pow`
  - `sin`, `cos`, `tan`, `atan`
  - `sinh`, `cosh`, `tanh`
- a broader `qfloat`-style advanced math surface, with some remaining functions
  still delegated through `qfloat_t`
- formatted output through `%mf` and `%MF`

## Constants

The subsystem exposes process-lifetime immortal constants:

- `MF_ZERO`
- `MF_ONE`
- `MF_HALF`
- `MF_TENTH`
- `MF_TEN`
- `MF_PI`
- `MF_E`
- `MF_EULER_MASCHERONI`
- `MF_SQRT2`
- `MF_SQRT_PI`
- `MF_NAN`
- `MF_INF`
- `MF_NINF`

They are exported as:

```c
extern const mfloat_t * const MF_ZERO;
```

and must not be modified or freed by callers.

## Constructors And Core Lifecycle

Core constructors:

- `mf_new()`
- `mf_new_prec(bits)`
- `mf_create_long(value)`
- `mf_create_double(value)`
- `mf_create_qfloat(value)`
- `mf_create_string(text)`

Named-value constructors:

- `mf_pi()`
- `mf_e()`
- `mf_euler_mascheroni()`
- `mf_max()`

Lifecycle helpers:

- `mf_clone(x)`
- `mf_clear(x)`
- `mf_free(x)`

## Precision Model

Each object carries a target precision in bits.

- `mf_new()` uses the current default precision
- `mf_new_prec(bits)` creates a value with an explicit precision
- `mf_set_precision(...)` updates the target precision for later rounded work
- `mf_set_default_precision(...)` changes the constructor precision used by
  `mf_new()`, `mf_create_string()`, and the named constant constructors

Named constants such as `mf_pi()`, `mf_e()`, and `mf_euler_mascheroni()` now
respect the current default precision:

- up to `256` bits they use long seeded constants and round
- above `256` bits they switch to algorithmic construction

## Example

```c
#include <stdio.h>
#include "mfloat.h"

int main(void) {
    mfloat_t *x;
    mfloat_t *y;
    char buf[256];

    mf_set_default_precision(256);
    x = mf_create_string("2.345");
    y = mf_create_string("2.345");
    if (!x || !y)
        return 1;

    if (mf_gamma(x) != 0 || mf_lgamma(y) != 0)
        return 1;

    mf_sprintf(buf, sizeof(buf), "%.77mf", x);
    printf("gamma(2.345)  = %s\n", buf);

    mf_sprintf(buf, sizeof(buf), "%.77mf", y);
    printf("lgamma(2.345) = %s\n", buf);

    mf_free(x);
    mf_free(y);
    return 0;
}
```

```text
gamma(2.345)  = 1.19929782941531928552681533588795691209235255849057037812899793700343786859038
lgamma(2.345) = 0.18173624337757203797862933229995978550118791690492470651875093221924437275615
```

## Formatting

`mfloat_t` has its own print helpers:

- `mf_printf(...)`
- `mf_sprintf(...)`
- `mf_vsprintf(...)`

Supported specifiers:

- `%mf` — pretty fixed-format output
- `%MF` — scientific-format output

with the same width / flag style used by `qfloat`.

## Queries And Conversion

Precision and setup:

- `mf_set_default_precision(bits)`
- `mf_get_default_precision()`
- `mf_set_precision(x, bits)`
- `mf_get_precision(x)`
- `mf_set_long(x, value)`
- `mf_set_double(x, value)`
- `mf_set_qfloat(x, value)`
- `mf_set_string(x, text)`

Conversions:

- `mf_to_string(x)`
- `mf_to_double(x)`
- `mf_to_qfloat(x)`

Representation queries:

- `mf_is_zero(x)`
- `mf_get_sign(x)`
- `mf_get_exponent2(x)`
- `mf_get_mantissa_bits(x)`
- `mf_get_mantissa_u64(x, &out)`

Comparisons:

- `mf_eq(a, b)`
- `mf_lt(a, b)`
- `mf_le(a, b)`
- `mf_gt(a, b)`
- `mf_ge(a, b)`
- `mf_cmp(a, b)`

## Arithmetic Surface

Basic arithmetic:

- `mf_abs`
- `mf_neg`
- `mf_add`
- `mf_add_long`
- `mf_sub`
- `mf_mul`
- `mf_mul_long`
- `mf_div`
- `mf_inv`
- `mf_pow_int`
- `mf_pow`
- `mf_ldexp`
- `mf_sqrt`

Extended arithmetic and special functions:

- `mf_pow10`
- `mf_sqr`
- `mf_floor`
- `mf_mul_pow10`
- `mf_hypot`
- `mf_exp`
- `mf_log`
- `mf_sin`
- `mf_cos`
- `mf_tan`
- `mf_atan`
- `mf_atan2`
- `mf_asin`
- `mf_acos`
- `mf_sinh`
- `mf_cosh`
- `mf_tanh`
- `mf_asinh`
- `mf_acosh`
- `mf_atanh`
- `mf_gamma`
- `mf_erf`
- `mf_erfc`
- `mf_erfinv`
- `mf_erfcinv`
- `mf_lgamma`
- `mf_digamma`
- `mf_trigamma`
- `mf_tetragamma`
- `mf_gammainv`
- `mf_lambert_w0`
- `mf_lambert_wm1`
- `mf_beta`
- `mf_logbeta`
- `mf_binomial`
- `mf_beta_pdf`
- `mf_logbeta_pdf`
- `mf_normal_pdf`
- `mf_normal_cdf`
- `mf_normal_logpdf`
- `mf_productlog`
- `mf_gammainc_lower`
- `mf_gammainc_upper`
- `mf_gammainc_P`
- `mf_gammainc_Q`
- `mf_ei`
- `mf_e1`

## Performance Notes

Recent work has improved both speed and precision in the native math layer:

- `mf_log()` and `mf_lgamma()` have had substantial native optimisation work
- immortal constants are now truly static-backed rather than heap-built
- mixed-precision arithmetic now trims oversized mantissas more aggressively
- dedicated `256` / `512` / `768` benchmark cases are tracked in the maths bench

There is now a dedicated `mfloat` benchmark binary:

```sh
make bench_mfloat_maths
```

and a compare helper:

```sh
bench/mfloat/compare_mfloat_maths.sh <git-ref>
```

The compare helper expects a reference that already contains the `mfloat`
subsystem.

## Testing

From the repository root:

```sh
make tests/build/release/mfloat/test_mfloat
tests/build/release/mfloat/test_mfloat
```

## Internal Layout

`mfloat` is split into:

- `mfloat_core.c`
- `mfloat_arith.c`
- `mfloat_maths.c`
- `mfloat_string.c`
- `mfloat_print.c`

with shared private declarations in `src/mfloat/mfloat_internal.h`.

The public surface remains in `include/mfloat.h`.

## Benchmark Coverage

The dedicated `mfloat` gamma benchmark includes direct `2.345` timing cases at:

- `256` bits
- `512` bits
- `768` bits
- `1024` bits

alongside the existing half-integer spot checks.

Benchmark source:

- [`bench/mfloat/bench_mfloat_gamma_maths.c`](../bench/mfloat/bench_mfloat_gamma_maths.c)

Run it from the repository root with:

```sh
make bench_mfloat_maths
MARS_BENCH_SCALE=10 ./build/release/bench/mfloat/bench_mfloat_maths
```

Current sample results from that command on this tree, measured on:

- `Linux x86_64`
- kernel `6.8.0-110-generic`
- `Intel(R) Core(TM) i7-4510U CPU @ 2.00GHz`
- `4` logical CPUs

Results:

| Case | `256` bits | `512` bits | `768` bits | `1024` bits |
|---|---:|---:|---:|---:|
| `mf_exp(1.23456789)` | `1.297 ms` | `3.742 ms` | `3.918 ms` | `9.914 ms` |
| `mf_log(2.345678)` | `0.557 ms` | `1.217 ms` | `1.814 ms` | `2.964 ms` |
| `mf_sqrt(1.23456789)` | `0.018 ms` | `0.033 ms` | `0.047 ms` | `0.091 ms` |
| `mf_gamma(2.345)` | `14.573 ms` | `27.320 ms` | `57.996 ms` | `131.598 ms` |
| `mf_lgamma(2.345)` | `13.191 ms` | `23.199 ms` | `52.019 ms` | `119.964 ms` |
| `mf_sin(0.567)` | `3.921 ms` | `10.509 ms` | `6.129 ms` | `15.347 ms` |
| `mf_cos(0.567)` | `1.671 ms` | `3.549 ms` | `10.621 ms` | `13.791 ms` |
| `mf_sincos(0.567)` | `5.477 ms` | `13.407 ms` | `21.026 ms` | `31.081 ms` |
| `mf_tan(0.567)` | `5.473 ms` | `13.464 ms` | `20.653 ms` | `31.333 ms` |
| `mf_atan(0.567)` | `0.623 ms` | `1.386 ms` | `2.578 ms` | `3.995 ms` |
| `mf_asin(0.7)` | `2.540 ms` | `5.519 ms` | `4.533 ms` | `12.346 ms` |
| `mf_acos(0.7)` | `1.360 ms` | `5.628 ms` | `4.574 ms` | `12.330 ms` |
| `mf_atan2(0.5,-0.75)` | `0.781 ms` | `1.106 ms` | `2.522 ms` | `5.348 ms` |
| `mf_sinh(0.7)` | `1.317 ms` | `2.759 ms` | `2.828 ms` | `3.311 ms` |
| `mf_cosh(0.7)` | `1.313 ms` | `2.766 ms` | `2.982 ms` | `3.560 ms` |
| `mf_sinhcosh(0.7)` | `1.361 ms` | `2.743 ms` | `3.136 ms` | `3.248 ms` |
| `mf_tanh(0.7)` | `3.187 ms` | `4.678 ms` | `5.783 ms` | `8.526 ms` |
| `mf_asinh(0.5)` | `0.585 ms` | `1.493 ms` | `2.637 ms` | `4.012 ms` |
| `mf_acosh(2)` | `0.937 ms` | `1.255 ms` | `1.583 ms` | `1.825 ms` |
| `mf_atanh(0.5)` | `0.717 ms` | `3.454 ms` | `4.907 ms` | `3.524 ms` |
| `mf_lambert_w0(0.7)` | `29.242 ms` | `62.759 ms` | `78.455 ms` | `67.229 ms` |
| `mf_lambert_wm1(-0.2)` | `51.342 ms` | `69.362 ms` | `172.352 ms` | `340.897 ms` |

For broader benchmark notes, see [`docs/benchmarks.md`](benchmarks.md).
