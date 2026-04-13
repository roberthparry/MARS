# `qfloat`

`qfloat` is a double-double floating-point type built from the unevaluated sum
of two IEEE-754 `double` values.

## Capabilities

- higher precision than plain `double`
- elementary functions
- selected special functions
- decimal parsing and formatting
- `printf` support through `%q` and `%Q`

## Example

```c
#include <stdio.h>
#include "qfloat.h"

int main(void) {
    const char *inputs[] = {
        "0",
        "1e-6",
        "0.1",
        "1",
        "5",
        "-0.3678794411714423215955237701614609",
        NULL
    };

    for (int i = 0; inputs[i] != NULL; i++) {
        qfloat x = qf_from_string(inputs[i]);
        qfloat w = qf_lambert_w0(x);
        qf_printf("W0(%s) = %q\n", inputs[i], w);
    }

    return 0;
}
```

Expected output:

```text
W0(0) = 0
W0(1e-6) = 9.999990000014999973333385416558944e-7
W0(0.1) = 0.09127652716086226429989572142317946
W0(1) = 0.5671432904097838729999686622103575
W0(5) = 1.326724665242200223635099297758082
W0(-0.3678794411714423215955237701614609) = -1
```

## Design Notes

### Representation

Each value is stored as:

```text
qfloat = hi + lo
```

`hi` carries the leading magnitude and `lo` carries the trailing correction.

### Arithmetic Core

The implementation is based on standard error-free transformation routines such
as:

- `TwoSum`
- `QuickTwoSum`
- `TwoProd`

These routines make rounding error explicit and allow the result to be
renormalized into a stable `(hi, lo)` pair.

### Function Implementation

Functions such as `exp`, `log`, `sin`, `cos`, `tan`, `sqrt`, `pow`, `atan2`,
and `hypot` are built from combinations of:

- argument reduction
- polynomial or rational approximation
- Newton-style refinement
- careful reconstruction into `(hi, lo)` form

Special functions build on the same arithmetic core.

### Formatting and Parsing

`qfloat` supports decimal parsing and high-precision formatting. That makes it
usable both for numeric work and for regression tests.

## Tradeoffs

Compared with `double`, `qfloat` offers more precision but with higher runtime
cost. Compared with arbitrary-precision libraries, it is lighter-weight and
simpler to integrate.

## API Reference

The full public API is declared in `include/qfloat.h`. The functions below are
the main entry points most users will want first.

### Construction and Conversion

- `qf_from_string(const char *text)`  
  Parse a decimal string into a `qfloat`.

- `qf_to_double(qfloat x)`  
  Convert a `qfloat` to a `double`.

### Core Arithmetic

- `qf_add(qfloat a, qfloat b)`  
  Add two `qfloat` values.

- `qf_sub(qfloat a, qfloat b)`  
  Subtract one `qfloat` from another.

- `qf_mul(qfloat a, qfloat b)`  
  Multiply two `qfloat` values.

- `qf_div(qfloat a, qfloat b)`  
  Divide one `qfloat` by another.

### Common Functions

- `qf_exp(qfloat x)`  
  Exponential function.

- `qf_log(qfloat x)`  
  Natural logarithm.

- `qf_sqrt(qfloat x)`  
  Square root.

- `qf_sin(qfloat x)`  
  Sine.

- `qf_cos(qfloat x)`  
  Cosine.

- `qf_lambert_w0(qfloat x)`  
  Principal branch of the Lambert W function.

### Formatting

- `qf_printf(const char *fmt, ...)`  
  Print formatted output using `%q` or `%Q` for `qfloat` values.