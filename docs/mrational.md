# `mrational_t`

`mrational_t` is MARS's opaque exact rational type.

It stores:

- a numerator as `mint_t`
- a denominator as `mint_t`
- a normalised sign and reduced fraction form

Conceptually:

```text
value = numerator / denominator
```

with both parts held exactly as arbitrary-precision integers.

## Capabilities

- exact rational values backed by `mint_t`
- exact construction from:
  - `long`
  - `long/long` fractions
  - decimal and fractional strings
- exact outward conversion to:
  - canonical Unicode rational display strings
- in-place exact arithmetic:
  - add, subtract, multiply, divide
  - reciprocal
  - sign flips and absolute value
- exact comparison and ordering helpers
- borrowed read-only access to numerator and denominator as `mint_t`

## Constructors And Core Lifecycle

Core constructors:

- `mr_new()`
- `mr_create_long(value)`
- `mr_create_frac_long(numerator, denominator)`
- `mr_create_string(text)`

Lifecycle helpers:

- `mr_clone(r)`
- `mr_clear(r)`
- `mr_free(r)`

## Normalisation Model

`mrational_t` values are stored in reduced canonical form:

- the denominator is positive
- common factors are removed
- zero is normalised to `0/1`

That means equivalent inputs such as `2/4`, `-3/-6`, and `0/5` collapse to:

- `½`
- `½`
- `0`

## Example

```c
#include <stdio.h>
#include "mrational.h"

int main(void) {
    mrational_t *a = mr_create_frac_long(2, 3);
    mrational_t *b = mr_create_string("5/4");
    char *text;

    if (!a || !b)
        return 1;

    if (mr_mul(a, b) != 0)
        return 1;

    text = mr_to_string(a);
    if (!text)
        return 1;

    printf("(2/3) * (5/4) = %s\n", text);

    free(text);
    mr_free(a);
    mr_free(b);
    return 0;
}
```

```text
(2/3) * (5/4) = ⅚
```

## Formatting And Conversion

- `mr_to_string(r)` returns a newly allocated canonical string
- integers print without a `/1` suffix
- common fractions print as single Unicode fraction glyphs, such as `½` or `⅚`
- other fractions print with superscript numerator, fraction slash, and subscript denominator, such as `³⁵⁵⁄₁₁₃`
- `mr_create_string(...)` accepts both ASCII fractions, such as `355/113`, and the Unicode forms emitted by `mr_to_string(...)`

Examples:

- `0`
- `-42`
- `⅚`
- `³⁵⁵⁄₁₁₃`

## Queries And Accessors

Predicates:

- `mr_is_zero(r)`
- `mr_is_integer(r)`

Borrowed read-only accessors:

- `mr_numerator(r)`
- `mr_denominator(r)`

The returned `mint_t` pointers remain valid while the owning `mrational_t` is
alive and must not be freed or modified by callers.

## Comparisons

- `mr_cmp(a, b)`
- `mr_eq(a, b)`
- `mr_lt(a, b)`
- `mr_le(a, b)`
- `mr_gt(a, b)`
- `mr_ge(a, b)`

## Arithmetic Surface

- `mr_neg`
- `mr_abs`
- `mr_inv`
- `mr_add`
- `mr_sub`
- `mr_mul`
- `mr_div`

## Notes

- `mrational_t` is exact; it does not carry a working precision.
- It is used internally by `mfloat_t` for exact coefficient data such as
  Bernoulli terms.
- Conversion into `mfloat_t` is available through:
  - `mf_create_mrational(...)`
  - `mf_set_mrational(...)`
  - `mf_add_mrational(...)`
  - `mf_mul_mrational(...)`

## Benchmarks

From the repository root:

```sh
make bench_mrational_arith
./build/release/bench/mrational/bench_mrational_arith
```

That benchmark compares:

- exact `mrational_t` arithmetic over numerator/denominator bit bands
- one stacked timing table where each operation contributes a `4x4` block
- numerator-bit bands: `1-256`, `257-512`, `513-768`, `769-1024`
- denominator-bit bands: `1-256`, `257-512`, `513-768`, `769-1024`

Recent measured timings, in nanoseconds per operation, were:

<table>
<thead>
<tr>
<th rowspan="2">Operation</th>
<th rowspan="2" style="text-align: center;">Numerator bits</th>
<th colspan="4" style="text-align: center;">Denominator bits</th>
</tr>
<tr>
<th style="text-align: center;"><span style="display: inline-block; text-align: center; font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">  1 - 256</span></th>
<th style="text-align: center;"><span style="display: inline-block; text-align: center; font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">257 - 512</span></th>
<th style="text-align: center;"><span style="display: inline-block; text-align: center; font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">513 - 768</span></th>
<th style="text-align: center;"><span style="display: inline-block; text-align: center; font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">769 - 1024</span></th>
</tr>
</thead>
<tbody>
<tr><td rowspan="4"><span style="font-size: 0.88em;"><strong>mr_add</strong></span></td><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">1-256</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">541.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">626.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">745.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">973.9 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">257-512</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">471.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">851.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1003.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1300.1 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">513-768</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">476.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1111.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1077.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1361.1 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">769-1024</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">647.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">879.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1905.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1382.5 ns</span></td></tr>
<tr><td rowspan="4"><span style="font-size: 0.88em;"><strong>mr_sub</strong></span></td><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">1-256</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">506.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">726.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">711.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">890.3 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">257-512</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">635.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">740.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">842.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1232.5 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">513-768</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">427.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">733.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">917.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1078.5 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">769-1024</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">530.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">867.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1026.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1504.4 ns</span></td></tr>
<tr><td rowspan="4"><span style="font-size: 0.88em;"><strong>mr_mul</strong></span></td><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">1-256</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">637.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1070.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1182.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1505.0 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">257-512</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">977.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1417.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1733.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1951.1 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">513-768</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1132.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1848.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1735.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">2695.9 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">769-1024</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1352.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">2310.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">2817.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">2853.5 ns</span></td></tr>
<tr><td rowspan="4"><span style="font-size: 0.88em;"><strong>mr_div</strong></span></td><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">1-256</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">841.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">919.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">913.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1096.7 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">257-512</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1027.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1326.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1353.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1469.3 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">513-768</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">938.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1393.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1232.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1760.0 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">769-1024</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1032.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1348.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1625.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1865.7 ns</span></td></tr>
<tr><td rowspan="4"><span style="font-size: 0.88em;"><strong>mr_inv</strong></span></td><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">1-256</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">238.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">215.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">161.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">192.6 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">257-512</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">134.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">164.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">185.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">189.0 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">513-768</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">138.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">166.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">168.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">198.7 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">769-1024</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">208.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">276.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">292.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">193.6 ns</span></td></tr>
</tbody>
</table>

These figures come from the current banded benchmark in
[`bench/mrational/bench_mrational_arith.c`](../bench/mrational/bench_mrational_arith.c).
