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
- `2048` bits
- `4096` bits

alongside the existing half-integer spot checks.

Benchmark source:

- [`bench/mfloat/bench_mfloat_gamma_maths.c`](../bench/mfloat/bench_mfloat_gamma_maths.c)

Run it from the repository root with:

```sh
make bench_mfloat_maths
MARS_BENCH_SCALE=3 ./build/release/bench/mfloat/bench_mfloat_maths
MARS_BENCH_FORMAT=md ./build/release/bench/mfloat/bench_mfloat_maths
```

Current sample results from that command on this tree, measured on:

- `Linux x86_64`
- kernel `6.8.0-110-generic`
- `Intel(R) Core(TM) i7-4510U CPU @ 2.00GHz`
- `4` logical CPUs

Markdown output uses the release benchmark with `51` timed batches per case,
records the sample median for each row, and keeps at least `3` inner
iterations even for the slowest docs-mode rows.

Results:

| Case | `256` bits | `512` bits | `768` bits | `1024` bits | `2048` bits | `4096` bits |
|---|---:|---:|---:|---:|---:|---:|
| `mf_exp(1.23456789)` | `10.0 µs` | `8.9 µs` | `26.6 µs` | `36.6 µs` | `69.7 µs` | `243.6 µs` |
| `mf_log(2.345678)` | `17.6 µs` | `13.9 µs` | `19.3 µs` | `27.2 µs` | `60.2 µs` | `166.5 µs` |
| `mf_sqrt(1.23456789)` | `0.9 µs` | `0.6 µs` | `1.4 µs` | `1.7 µs` | `1.9 µs` | `4.0 µs` |
| `mf_sin(0.567)` | `10.7 µs` | `9.1 µs` | `14.3 µs` | `21.3 µs` | `62.2 µs` | `224.8 µs` |
| `mf_cos(0.7)` | `8.7 µs` | `7.7 µs` | `22.0 µs` | `18.7 µs` | `57.8 µs` | `215.8 µs` |
| `mf_sincos(0.7)` | `10.1 µs` | `9.4 µs` | `14.6 µs` | `21.4 µs` | `62.8 µs` | `278.3 µs` |
| `mf_tan(0.7)` | `11.8 µs` | `10.1 µs` | `15.9 µs` | `22.7 µs` | `66.0 µs` | `239.2 µs` |
| `mf_atan(0.7)` | `49.4 µs` | `39.6 µs` | `50.4 µs` | `73.7 µs` | `167.6 µs` | `520.5 µs` |
| `mf_asin(0.7)` | `61.8 µs` | `44.2 µs` | `95.1 µs` | `82.6 µs` | `378.1 µs` | `930.1 µs` |
| `mf_acos(0.7)` | `62.0 µs` | `44.6 µs` | `101.0 µs` | `112.7 µs` | `373.8 µs` | `942.0 µs` |
| `mf_atan2(0.5,-0.75)` | `53.4 µs` | `41.5 µs` | `53.2 µs` | `77.3 µs` | `363.6 µs` | `887.8 µs` |
| `mf_sinh(0.7)` | `9.1 µs` | `8.9 µs` | `27.4 µs` | `21.4 µs` | `131.8 µs` | `421.6 µs` |
| `mf_cosh(0.7)` | `9.8 µs` | `8.9 µs` | `14.8 µs` | `21.4 µs` | `131.9 µs` | `422.0 µs` |
| `mf_sinhcosh(0.7)` | `10.1 µs` | `9.0 µs` | `27.0 µs` | `21.8 µs` | `133.6 µs` | `424.4 µs` |
| `mf_tanh(0.7)` | `10.5 µs` | `9.4 µs` | `15.2 µs` | `21.5 µs` | `134.6 µs` | `422.0 µs` |
| `mf_asinh(0.5)` | `19.7 µs` | `15.8 µs` | `36.7 µs` | `28.9 µs` | `126.7 µs` | `304.2 µs` |
| `mf_acosh(2)` | `19.7 µs` | `15.8 µs` | `36.9 µs` | `29.0 µs` | `125.9 µs` | `304.1 µs` |
| `mf_atanh(0.5)` | `17.4 µs` | `15.9 µs` | `21.8 µs` | `29.5 µs` | `130.9 µs` | `319.6 µs` |
| `mf_lambert_w0(0.7)` | `69.5 µs` | `121.0 µs` | `428.7 µs` | `647.3 µs` | `910.2 µs` | `5.518 ms` |
| `mf_lambert_wm1(-0.2)` | `79.3 µs` | `142.8 µs` | `501.4 µs` | `733.5 µs` | `1.009 ms` | `3.379 ms` |
| `mf_gamma(2.345)` | `123.4 µs` | `147.6 µs` | `407.6 µs` | `433.5 µs` | `3.115 ms` | `12.563 ms` |
| `mf_lgamma(2.345)` | `124.4 µs` | `152.3 µs` | `384.8 µs` | `433.4 µs` | `2.076 ms` | `6.819 ms` |
| `mf_digamma(2.345)` | `333.8 µs` | `807.6 µs` | `1.250 ms` | `1.384 ms` | `5.801 ms` | `28.537 ms` |
| `mf_trigamma(2.345)` | `690.3 µs` | `756.3 µs` | `1.279 ms` | `810.4 µs` | `1.802 ms` | `4.704 ms` |
| `mf_tetragamma(2.345)` | `808.9 µs` | `567.1 µs` | `1.507 ms` | `1.011 ms` | `2.098 ms` | `7.147 ms` |
| `mf_ei(5)` | `82.9 µs` | `142.0 µs` | `356.1 µs` | `254.1 µs` | `872.0 µs` | `1.191 ms` |
| `mf_e1(5)` | `181.3 µs` | `373.2 µs` | `434.8 µs` | `557.4 µs` | `569.4 µs` | `1.202 ms` |

For broader benchmark notes, see [`docs/benchmarks.md`](benchmarks.md).




