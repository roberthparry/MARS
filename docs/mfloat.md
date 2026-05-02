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
- `MF_NAN`
- `MF_INF`
- `MF_NINF`

They are exported as:

```c
extern const mfloat_t * const MF_ZERO;
```

and must not be modified or freed by callers.

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
    char buf[256];

    mf_set_default_precision(256);
    x = mf_pi();
    if (!x)
        return 1;

    mf_sprintf(buf, sizeof(buf), "%.20MF", x);
    printf("pi ~= %s\n", buf);

    mf_free(x);
    return 0;
}
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

## Current Native Math Boundary

The most important native `mfloat` math paths implemented so far are:

- `mf_exp`
- `mf_log`
- `mf_pow`
- `mf_sin`
- `mf_cos`
- `mf_tan`
- `mf_atan`
- `mf_sinh`
- `mf_cosh`
- `mf_tanh`

Some harder inverse and special-function families are still backed by
`qfloat_t` internally for now.

## Performance Notes

Recent work has improved both speed and precision in the native math layer:

- `mf_log()` now works from the normalised binary form rather than repeated
  Newton refinement on `exp`
- `pi` and `ln(2)` are cached internally at working precision
- `mf_pi()` uses a fast fixed-point Machin-style path

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
