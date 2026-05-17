# `mint_t`

`mint_t` is MARS's opaque arbitrary-precision signed integer type.

It stores an exact integer sign plus a growable multi-limb magnitude and is
intended as the integer foundation for number theory, exact combinatorics, and
higher-precision subsystems such as `mfloat_t`.

## Capabilities

- exact signed integers with unbounded size
- decimal and hexadecimal parsing / formatting
- arithmetic: add, subtract, multiply, divide, modulo, shifts, bitwise ops
- magnitude queries: bit length, floor binary logarithm, and bit tests
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

Recent measured timings on this tree, in nanoseconds per operation, were:

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
    <tr><td rowspan="4"><code>mi_add</code></td><td><strong><code>1-256</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">259.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">305.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">149.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">228.6 ns</span></td></tr>
    <tr><td><strong><code>257-512</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">250.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">302.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">283.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">168.8 ns</span></td></tr>
    <tr><td><strong><code>513-768</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">113.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">124.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">124.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">186.0 ns</span></td></tr>
    <tr><td><strong><code>769-1024</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">185.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">140.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">154.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">175.5 ns</span></td></tr>
    <tr><td rowspan="4"><code>mi_sub</code></td><td><strong><code>1-256</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">115.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">148.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">147.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">193.9 ns</span></td></tr>
    <tr><td><strong><code>257-512</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">119.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">115.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">121.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">201.7 ns</span></td></tr>
    <tr><td><strong><code>513-768</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">127.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">131.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">133.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">159.0 ns</span></td></tr>
    <tr><td><strong><code>769-1024</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">136.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">187.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">258.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">164.4 ns</span></td></tr>
    <tr><td rowspan="4"><code>mi_mul</code></td><td><strong><code>1-256</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">110.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">125.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">143.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">174.7 ns</span></td></tr>
    <tr><td><strong><code>257-512</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">260.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">362.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">302.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">292.3 ns</span></td></tr>
    <tr><td><strong><code>513-768</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">135.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">281.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">535.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">325.5 ns</span></td></tr>
    <tr><td><strong><code>769-1024</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">163.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">209.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">516.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">470.3 ns</span></td></tr>
    <tr><td rowspan="4"><code>mi_div</code></td><td><strong><code>1-256</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">152.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">110.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">110.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">108.1 ns</span></td></tr>
    <tr><td><strong><code>257-512</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">190.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">185.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">107.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">200.4 ns</span></td></tr>
    <tr><td><strong><code>513-768</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">473.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">569.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">420.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">197.5 ns</span></td></tr>
    <tr><td><strong><code>769-1024</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">346.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">373.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">372.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">428.2 ns</span></td></tr>
    <tr><td rowspan="4"><code>mi_mod</code></td><td><strong><code>1-256</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">352.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">98.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">77.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">73.4 ns</span></td></tr>
    <tr><td><strong><code>257-512</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">171.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">184.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">151.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">116.8 ns</span></td></tr>
    <tr><td><strong><code>513-768</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">279.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">260.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">157.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">70.9 ns</span></td></tr>
    <tr><td><strong><code>769-1024</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">268.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">376.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">465.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">228.3 ns</span></td></tr>
    <tr><td rowspan="4"><code>mi_gcd</code></td><td><strong><code>1-256</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">223.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">410.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">532.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">511.4 ns</span></td></tr>
    <tr><td><strong><code>257-512</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">328.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">493.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">851.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">817.8 ns</span></td></tr>
    <tr><td><strong><code>513-768</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">404.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">670.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">547.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">773.0 ns</span></td></tr>
    <tr><td><strong><code>769-1024</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">452.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">792.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1252.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">743.1 ns</span></td></tr>
    <tr><td rowspan="4"><code>mi_lcm</code></td><td><strong><code>1-256</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">275.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">418.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">462.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">606.0 ns</span></td></tr>
    <tr><td><strong><code>257-512</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">532.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">818.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1160.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">883.6 ns</span></td></tr>
    <tr><td><strong><code>513-768</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">517.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1044.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1088.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1242.8 ns</span></td></tr>
    <tr><td><strong><code>769-1024</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">557.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1012.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1401.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1069.5 ns</span></td></tr>
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
