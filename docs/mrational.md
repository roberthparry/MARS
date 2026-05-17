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
MARS_BENCH_FORMAT=md ./build/release/bench/mrational/bench_mrational_arith
```

That benchmark compares:

- exact `mrational_t` arithmetic over numerator/denominator bit bands
- one stacked timing table where each operation contributes a `6x6` block
- numerator-bit bands: `1-256`, `257-512`, `513-768`, `769-1024`, `1025-2048`, `2049-4096`
- denominator-bit bands: `1-256`, `257-512`, `513-768`, `769-1024`, `1025-2048`, `2049-4096`

Recent measured timings, as robust medians from representative inputs measured
with `51` timed batches and reported in nanoseconds per operation, were:

<table>
<thead>
<tr>
<th rowspan="2">Operation</th>
<th rowspan="2" style="text-align: center;">Numerator bits</th>
<th colspan="6" style="text-align: center;">Denominator bits</th>
</tr>
<tr>
<th style="text-align: center;"><span style="display: inline-block; text-align: center; font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">     1-256</span></th>
<th style="text-align: center;"><span style="display: inline-block; text-align: center; font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">   257-512</span></th>
<th style="text-align: center;"><span style="display: inline-block; text-align: center; font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">   513-768</span></th>
<th style="text-align: center;"><span style="display: inline-block; text-align: center; font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">  769-1024</span></th>
<th style="text-align: center;"><span style="display: inline-block; text-align: center; font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;"> 1025-2048</span></th>
<th style="text-align: center;"><span style="display: inline-block; text-align: center; font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;"> 2049-4096</span></th>
</tr>
</thead>
<tbody>
<tr><td rowspan="6"><span style="font-size: 0.88em;"><strong>mr_add</strong></span></td><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">1-256</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">686.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1130.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1504.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1879.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">2932.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7264.9 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">257-512</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">849.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1058.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1025.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1207.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">2174.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">5276.3 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">513-768</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">523.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">785.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1053.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1411.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">2485.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">5970.4 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">769-1024</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">591.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">714.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">810.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">999.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1866.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">4178.8 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">1025-2048</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">462.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">765.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1064.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1375.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">2029.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7511.0 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">2049-4096</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">902.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1770.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">2578.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">2968.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">3145.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">6109.9 ns</span></td></tr>
<tr><td rowspan="6"><span style="font-size: 0.88em;"><strong>mr_sub</strong></span></td><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">1-256</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">234.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">362.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">487.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">643.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1255.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">3236.8 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">257-512</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">470.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">641.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">894.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1225.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">2203.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">5480.3 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">513-768</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">445.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">524.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">664.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">975.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1633.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">4818.8 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">769-1024</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">463.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">652.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1045.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1129.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">2902.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">8573.3 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">1025-2048</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">490.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1041.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1477.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1897.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">3187.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">6735.9 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">2049-4096</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">459.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">927.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1432.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1934.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">3156.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">5732.5 ns</span></td></tr>
<tr><td rowspan="6"><span style="font-size: 0.88em;"><strong>mr_mul</strong></span></td><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">1-256</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">396.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1211.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1491.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1766.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">2436.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">5073.5 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">257-512</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1160.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">2750.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">2196.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1905.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">2594.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">4887.7 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">513-768</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1030.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1407.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1792.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">3163.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">3027.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">5589.7 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">769-1024</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1227.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">2114.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">2936.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">2838.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">5167.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7714.9 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">1025-2048</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1622.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">2194.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">2851.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">3137.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">3377.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">6329.4 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">2049-4096</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">3370.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">5190.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">5603.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">5962.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">6619.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">12995.5 ns</span></td></tr>
<tr><td rowspan="6"><span style="font-size: 0.88em;"><strong>mr_div</strong></span></td><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">1-256</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">480.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">590.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">659.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">679.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">846.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1490.7 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">257-512</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">658.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">830.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">978.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1095.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">948.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1466.0 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">513-768</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">504.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">665.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">773.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">893.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1216.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1904.2 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">769-1024</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">572.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">786.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">942.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1046.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1405.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">2354.9 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">1025-2048</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">761.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1023.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1225.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1371.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1823.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">3361.3 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">2049-4096</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1243.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1472.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1823.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">2443.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">3268.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">4832.0 ns</span></td></tr>
<tr><td rowspan="6"><span style="font-size: 0.88em;"><strong>mr_inv</strong></span></td><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">1-256</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7.6 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">257-512</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">6.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7.5 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">513-768</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7.5 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">769-1024</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7.7 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">1025-2048</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7.8 ns</span></td></tr>
<tr><td><span style="font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;">2049-4096</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">8.0 ns</span></td></tr>
</tbody>
</table>

These figures come from the current banded benchmark in
[`bench/mrational/bench_mrational_arith.c`](../bench/mrational/bench_mrational_arith.c).



