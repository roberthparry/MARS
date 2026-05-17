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
- `MF_LN10`
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
- `mf_log10`
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
MARS_BENCH_SCALE=3 ./build/release/bench/mfloat/bench_mfloat_maths
```

Current sample results from that command on this tree, measured on:

- `Linux x86_64`
- kernel `6.8.0-110-generic`
- `Intel(R) Core(TM) i7-4510U CPU @ 2.00GHz`
- `4` logical CPUs

Results:

| Case | `256` bits | `512` bits | `768` bits | `1024` bits |
|---|---:|---:|---:|---:|
| `mf_exp(1.23456789)` | `3.386 µs` | `6.294 µs` | `10.632 µs` | `16.068 µs` |
| `mf_log(2.345678)` | `6.189 µs` | `9.245 µs` | `13.228 µs` | `18.592 µs` |
| `mf_sqrt(1.23456789)` | `0.337 µs` | `0.468 µs` | `0.585 µs` | `0.763 µs` |
| `mf_sin(0.567)` | `4.071 µs` | `6.239 µs` | `9.697 µs` | `14.657 µs` |
| `mf_cos(0.567)` | `2.995 µs` | `5.102 µs` | `8.194 µs` | `12.283 µs` |
| `mf_sincos(0.7)` | `5.133 µs` | `6.324 µs` | `9.730 µs` | `14.636 µs` |
| `mf_tan(0.7)` | `5.070 µs` | `6.789 µs` | `12.104 µs` | `15.188 µs` |
| `mf_atan(0.567)` | `17.283 µs` | `26.692 µs` | `35.702 µs` | `52.791 µs` |
| `mf_asin(0.7)` | `20.646 µs` | `29.483 µs` | `38.740 µs` | `55.598 µs` |
| `mf_acos(0.7)` | `20.714 µs` | `31.860 µs` | `76.736 µs` | `54.449 µs` |
| `mf_atan2(0.5,-0.75)` | `18.331 µs` | `26.855 µs` | `70.714 µs` | `51.562 µs` |
| `mf_sinh(0.7)` | `3.476 µs` | `6.388 µs` | `11.731 µs` | `18.074 µs` |
| `mf_cosh(0.7)` | `3.306 µs` | `5.784 µs` | `10.796 µs` | `14.099 µs` |
| `mf_sinhcosh(0.7)` | `3.582 µs` | `6.093 µs` | `11.044 µs` | `14.245 µs` |
| `mf_tanh(0.7)` | `3.500 µs` | `6.170 µs` | `11.104 µs` | `14.726 µs` |
| `mf_asinh(0.5)` | `7.134 µs` | `10.996 µs` | `28.563 µs` | `19.976 µs` |
| `mf_acosh(2)` | `6.966 µs` | `10.570 µs` | `15.842 µs` | `19.303 µs` |
| `mf_atanh(0.5)` | `10.850 µs` | `10.639 µs` | `15.941 µs` | `19.558 µs` |
| `mf_lambert_w0(0.7)` | `44.236 µs` | `89.883 µs` | `135.270 µs` | `255.082 µs` |
| `mf_lambert_wm1(-0.2)` | `51.413 µs` | `105.875 µs` | `156.266 µs` | `254.452 µs` |
| `mf_gamma(2.345)` | `43.651 µs` | `100.045 µs` | `200.043 µs` | `286.938 µs` |
| `mf_lgamma(2.345)` | `51.696 µs` | `100.305 µs` | `185.250 µs` | `279.896 µs` |
| `mf_digamma(2.345)` | `128.809 µs` | `305.628 µs` | `557.540 µs` | `930.219 µs` |
| `mf_trigamma(2.345)` | `259.551 µs` | `331.701 µs` | `400.181 µs` | `580.116 µs` |
| `mf_tetragamma(2.345)` | `268.754 µs` | `389.221 µs` | `520.802 µs` | `651.042 µs` |
| `mf_ei(5)` | `84.481 µs` | `93.002 µs` | `140.138 µs` | `292.348 µs` |
| `mf_e1(5)` | `121.185 µs` | `194.392 µs` | `267.273 µs` | `362.266 µs` |

For broader benchmark notes, see [`docs/benchmarks.md`](benchmarks.md).
