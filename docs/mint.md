# `mint_t`

`mint_t` is MARS's opaque arbitrary-precision signed integer type.

It stores an exact integer sign plus a growable multi-limb magnitude and is
intended as the integer foundation for number theory, exact combinatorics, and
higher-precision subsystems such as `mfloat_t`.

## Capabilities

- exact signed integers with unbounded size
- decimal and hexadecimal parsing / formatting
- arithmetic: add, subtract, multiply, divide, modulo, shifts, bitwise ops
- convenience helpers for native `long` / `unsigned long`
- integer powers, square root, `powmod`, and `divmod`
- number theory:
  - `gcd`, `lcm`, extended gcd, modular inverse
  - primality testing and next/previous prime
  - factorisation
- combinatorics and sequences:
  - factorial
  - Fibonacci
  - binomial coefficients
- formatted output through the public `mi_*` API

## Public Naming

The public function namespace is `mi_*`.

Examples:

- `mi_create_long(...)`
- `mi_add(...)`
- `mi_mul_long(...)`
- `mi_isprime(...)`
- `mi_factorial(...)`

The type name remains `mint_t`.

## Constants

The integer subsystem exposes process-lifetime convenience constants:

- `MI_ZERO`
- `MI_ONE`
- `MI_TEN`

They are exported as read-only pointers and must not be modified or freed by
callers.

## Example

```c
#include <stdio.h>
#include "mint.h"

int main(void) {
    mint_t *c = mi_new();

    if (!c)
        return 1;

    if (mi_binomial(c, 52ul, 5ul) != 0)
        return 1;

    printf("C(52, 5) = %s\n", mi_to_string(c));

    mi_free(c);
    return 0;
}
```

Expected result:

```text
C(52, 5) = 2598960
```

## Performance Notes

Recent work on `mint_t` has focused on the core arithmetic paths:

- faster native-word add/sub/mul/div helpers
- a stronger multi-limb division core
- a Lehmer-style front end for `gcd`
- wider benchmark coverage under `bench/mint`

The library now includes focused benchmark binaries for:

- add/sub
- mul/square
- div/mod
- gcd/lcm/modinv
- combinatorics

## Testing

From the repository root:

```sh
make test_mint
```

The long README Mersenne-prime search remains in the tree but is currently
disabled in `tests/test_config.json` for normal test runs.

## Benchmarks

From the repository root:

```sh
make bench_mint_arith
make bench_mint_add
make bench_mint_mul
make bench_mint_div
make bench_mint_gcd
make bench_mint_combinatorics
```

There is also a compare helper for the division benchmark:

```sh
bench/mint/compare_mint_div.sh <git-ref>
```

The new banded arithmetic benchmark reports one combined timing table for:

- `mi_add`
- `mi_sub`
- `mi_mul`
- `mi_div`
- `mi_mod`
- `mi_gcd`
- `mi_lcm`

Rows are grouped by operation and left-operand bit band, columns by
right-operand bit band, and each cell reports the average time per call over
representative exact integers in the bucket.

Recent measured timings on this tree were:

<table>
  <thead>
    <tr>
      <th rowspan="2">Operation</th>
      <th rowspan="2">Left bits</th>
      <th colspan="4" style="text-align: center;">Right bits</th>
    </tr>
    <tr>
      <th><strong><code>  1 - 256</code></strong></th>
      <th><strong><code>257 - 512</code></strong></th>
      <th><strong><code>513 - 768</code></strong></th>
      <th><strong><code>769 - 1024</code></strong></th>
    </tr>
  </thead>
  <tbody>
    <tr><td rowspan="4"><code>mi_add</code></td><td><strong><code>  1 - 256</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.426 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.634 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.702 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.754 µs</span></td></tr>
    <tr><td><strong><code>257 - 512</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.422 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.453 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.841 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.786 µs</span></td></tr>
    <tr><td><strong><code>513 - 768</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.417 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.371 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.460 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.465 µs</span></td></tr>
    <tr><td><strong><code>769 - 1024</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.458 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.467 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.472 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.527 µs</span></td></tr>
    <tr><td rowspan="4"><code>mi_sub</code></td><td><strong><code>  1 - 256</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.479 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.407 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.789 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.803 µs</span></td></tr>
    <tr><td><strong><code>257 - 512</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.442 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.418 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.810 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.893 µs</span></td></tr>
    <tr><td><strong><code>513 - 768</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.449 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.403 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.450 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.511 µs</span></td></tr>
    <tr><td><strong><code>769 - 1024</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.389 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.400 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.488 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.479 µs</span></td></tr>
    <tr><td rowspan="4"><code>mi_mul</code></td><td><strong><code>  1 - 256</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.405 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.490 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.573 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.711 µs</span></td></tr>
    <tr><td><strong><code>257 - 512</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.443 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.714 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.786 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.981 µs</span></td></tr>
    <tr><td><strong><code>513 - 768</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.544 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.779 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1.048 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1.237 µs</span></td></tr>
    <tr><td><strong><code>769 - 1024</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.640 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.976 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1.350 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1.410 µs</span></td></tr>
    <tr><td rowspan="4"><code>mi_div</code></td><td><strong><code>  1 - 256</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1.090 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1.235 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1.226 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1.157 µs</span></td></tr>
    <tr><td><strong><code>257 - 512</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1.654 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1.502 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1.179 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1.220 µs</span></td></tr>
    <tr><td><strong><code>513 - 768</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1.881 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">2.557 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1.607 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1.190 µs</span></td></tr>
    <tr><td><strong><code>769 - 1024</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1.880 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">2.452 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1.557 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.800 µs</span></td></tr>
    <tr><td rowspan="4"><code>mi_mod</code></td><td><strong><code>  1 - 256</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.404 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.426 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.421 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.425 µs</span></td></tr>
    <tr><td><strong><code>257 - 512</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.695 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.947 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.848 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.756 µs</span></td></tr>
    <tr><td><strong><code>513 - 768</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1.275 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1.543 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1.012 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">0.765 µs</span></td></tr>
    <tr><td><strong><code>769 - 1024</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1.543 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">2.225 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">2.028 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1.325 µs</span></td></tr>
    <tr><td rowspan="4"><code>mi_gcd</code></td><td><strong><code>  1 - 256</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">3.505 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">6.200 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7.719 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">8.958 µs</span></td></tr>
    <tr><td><strong><code>257 - 512</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">4.897 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">8.727 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">12.097 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">12.926 µs</span></td></tr>
    <tr><td><strong><code>513 - 768</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">5.388 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">12.965 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">12.005 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">16.585 µs</span></td></tr>
    <tr><td><strong><code>769 - 1024</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7.038 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7.458 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">15.097 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">12.660 µs</span></td></tr>
    <tr><td rowspan="4"><code>mi_lcm</code></td><td><strong><code>  1 - 256</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">4.585 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7.087 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">8.226 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">9.954 µs</span></td></tr>
    <tr><td><strong><code>257 - 512</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">6.155 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">9.712 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">13.497 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">14.349 µs</span></td></tr>
    <tr><td><strong><code>513 - 768</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">6.587 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">11.595 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">14.271 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">30.662 µs</span></td></tr>
    <tr><td><strong><code>769 - 1024</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">11.286 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">15.538 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">25.718 µs</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">23.061 µs</span></td></tr>
  </tbody>
</table>

## Internal Layout

`mint` is now split into broad internal source groups:

- `mint_core.c`
- `mint_arith.c`
- `mint_div.c`
- `mint_ntheory.c`
- `mint_string.c`

with shared private declarations in `src/mint/mint_internal.h`.

This split is internal only; the stable public surface remains in
`include/mint.h`.
