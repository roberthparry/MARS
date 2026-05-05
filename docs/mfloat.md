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
    x = mf_create_string("2.3");
    y = mf_create_string("2.3");
    if (!x || !y)
        return 1;

    if (mf_gamma(x) != 0 || mf_lgamma(y) != 0)
        return 1;

    mf_sprintf(buf, sizeof(buf), "%.77mf", x);
    printf("gamma(2.3)  = %s\n", buf);

    mf_sprintf(buf, sizeof(buf), "%.77mf", y);
    printf("lgamma(2.3) = %s\n", buf);

    mf_free(x);
    mf_free(y);
    return 0;
}
```

```text
gamma(2.3)  = 1.16671190519816034504188144120291793853399434971946889397020666387299161947176
lgamma(2.3) = 0.15418945495963058108991791148922317269570397608961402272570768556406857691921
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

The dedicated `mfloat` gamma benchmark includes direct `2.3` timing cases at:

- `256` bits
- `512` bits
- `768` bits

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

| Case | `256` bits | `512` bits | `768` bits |
|---|---:|---:|---:|
| `mf_exp(1.23456789)` | `6.681 ms` | `38.681 ms` | `630.834 ms` |
| `mf_log(1.23456789)` | `3.426 ms` | `11.252 ms` | `52.357 ms` |
| `mf_sin(0.7)` | `11.172 ms` | `17.415 ms` | `256.148 ms` |
| `mf_asin(0.5)` | `122.371 ms` | `547.263 ms` | `401.761 ms` |
| `mf_erf(0.5)` | `6.288 µs` | `7.197 µs` | `6.174 µs` |
| `mf_lambert_w0(1)` | `6.423 µs` | `7.707 µs` | `6.934 µs` |
| `mf_lambert_wm1(-0.1)` | `11.498 µs` | `8.568 µs` | `7.590 µs` |

For broader benchmark notes, see [`docs/benchmarks.md`](benchmarks.md).
