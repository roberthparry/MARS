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

To regenerate the docs-ready timing table directly from the release bench:

```sh
MARS_BENCH_FORMAT=md ./build/release/bench/mint/bench_mint_arith
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
right-operand bit band, and each cell reports a robust median across
representative exact integers in the bucket, with each representative input
measured from `51` timed batches.

Recent measured timings on this tree, in nanoseconds per operation, were:

<table>
  <thead>
    <tr>
      <th rowspan="2">Operation</th>
      <th rowspan="2">Left bits</th>
      <th colspan="6" style="text-align: center;">Right bits</th>
    </tr>
    <tr>
      <th><strong><code>     1-256</code></strong></th>
      <th><strong><code>   257-512</code></strong></th>
      <th><strong><code>   513-768</code></strong></th>
      <th><strong><code>  769-1024</code></strong></th>
      <th><strong><code> 1025-2048</code></strong></th>
      <th><strong><code> 2049-4096</code></strong></th>
    </tr>
  </thead>
  <tbody>
    <tr><td rowspan="6"><code>mi_add</code></td><td><strong><code>1-256</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">31.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">44.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">45.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">50.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">64.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">123.1 ns</span></td></tr>
    <tr><td><strong><code>257-512</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">41.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">45.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">54.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">57.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">65.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">78.6 ns</span></td></tr>
    <tr><td><strong><code>513-768</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">40.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">36.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">48.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">61.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">68.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">97.9 ns</span></td></tr>
    <tr><td><strong><code>769-1024</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">37.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">42.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">41.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">55.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">64.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">130.2 ns</span></td></tr>
    <tr><td><strong><code>1025-2048</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">57.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">48.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">51.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">54.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">87.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">144.8 ns</span></td></tr>
    <tr><td><strong><code>2049-4096</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">61.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">52.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">76.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">60.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">63.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">104.6 ns</span></td></tr>
    <tr><td rowspan="6"><code>mi_sub</code></td><td><strong><code>1-256</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">53.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">73.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">76.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">89.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">111.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">152.8 ns</span></td></tr>
    <tr><td><strong><code>257-512</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">55.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">76.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">81.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">95.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">115.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">154.1 ns</span></td></tr>
    <tr><td><strong><code>513-768</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">72.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">63.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">81.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">91.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">109.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">289.7 ns</span></td></tr>
    <tr><td><strong><code>769-1024</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">96.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">67.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">85.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">98.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">159.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">243.7 ns</span></td></tr>
    <tr><td><strong><code>1025-2048</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">105.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">149.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">157.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">155.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">172.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">187.1 ns</span></td></tr>
    <tr><td><strong><code>2049-4096</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">138.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">176.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">142.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">140.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">138.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">178.8 ns</span></td></tr>
    <tr><td rowspan="6"><code>mi_mul</code></td><td><strong><code>1-256</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">73.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">88.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">101.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">139.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">177.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">266.1 ns</span></td></tr>
    <tr><td><strong><code>257-512</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">88.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">139.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">369.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">398.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">415.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">446.0 ns</span></td></tr>
    <tr><td><strong><code>513-768</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">73.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">120.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">166.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">213.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">338.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">625.2 ns</span></td></tr>
    <tr><td><strong><code>769-1024</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">100.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">156.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">205.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">271.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">440.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">889.5 ns</span></td></tr>
    <tr><td><strong><code>1025-2048</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">156.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">247.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">362.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">494.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">731.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1284.2 ns</span></td></tr>
    <tr><td><strong><code>2049-4096</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">220.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">392.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1479.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1878.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">2719.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">5424.1 ns</span></td></tr>
    <tr><td rowspan="6"><code>mi_div</code></td><td><strong><code>1-256</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">231.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">134.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">138.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">138.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">142.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">89.7 ns</span></td></tr>
    <tr><td><strong><code>257-512</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">181.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">188.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">58.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">50.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">50.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">50.7 ns</span></td></tr>
    <tr><td><strong><code>513-768</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">225.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">289.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">201.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">52.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">52.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">52.3 ns</span></td></tr>
    <tr><td><strong><code>769-1024</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">287.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">356.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">386.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">235.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">54.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">54.2 ns</span></td></tr>
    <tr><td><strong><code>1025-2048</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">415.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">580.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">599.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">785.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">336.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">66.0 ns</span></td></tr>
    <tr><td><strong><code>2049-4096</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">883.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1428.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1378.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1042.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1304.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">222.5 ns</span></td></tr>
    <tr><td rowspan="6"><code>mi_mod</code></td><td><strong><code>1-256</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">43.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">8.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">8.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">8.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">8.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">8.1 ns</span></td></tr>
    <tr><td><strong><code>257-512</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">86.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">90.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">8.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">8.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">8.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">8.1 ns</span></td></tr>
    <tr><td><strong><code>513-768</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">123.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">157.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">103.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">8.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">8.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">8.1 ns</span></td></tr>
    <tr><td><strong><code>769-1024</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">158.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">199.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">210.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">105.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">8.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">8.1 ns</span></td></tr>
    <tr><td><strong><code>1025-2048</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">241.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">342.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">356.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">373.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">130.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">8.1 ns</span></td></tr>
    <tr><td><strong><code>2049-4096</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">452.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">709.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">853.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">936.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">989.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">179.4 ns</span></td></tr>
    <tr><td rowspan="6"><code>mi_gcd</code></td><td><strong><code>1-256</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">69.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">237.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">256.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">327.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">419.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">652.3 ns</span></td></tr>
    <tr><td><strong><code>257-512</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">214.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">479.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">521.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">694.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">684.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1127.9 ns</span></td></tr>
    <tr><td><strong><code>513-768</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">329.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">555.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">543.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">672.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">875.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1504.9 ns</span></td></tr>
    <tr><td><strong><code>769-1024</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">344.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1073.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1805.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1047.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">2286.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">2692.9 ns</span></td></tr>
    <tr><td><strong><code>1025-2048</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">749.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1242.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1395.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1472.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1462.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">2554.3 ns</span></td></tr>
    <tr><td><strong><code>2049-4096</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1034.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1703.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">2140.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">2326.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1852.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">2106.1 ns</span></td></tr>
    <tr><td rowspan="6"><code>mi_lcm</code></td><td><strong><code>1-256</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">164.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">344.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">401.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">463.0 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">591.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">859.8 ns</span></td></tr>
    <tr><td><strong><code>257-512</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">346.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">623.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">696.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">797.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">961.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1531.5 ns</span></td></tr>
    <tr><td><strong><code>513-768</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">429.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">721.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">806.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1016.4 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1415.9 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">2402.3 ns</span></td></tr>
    <tr><td><strong><code>769-1024</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">937.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">940.7 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1144.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">921.2 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1569.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">2541.1 ns</span></td></tr>
    <tr><td><strong><code>1025-2048</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">664.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1055.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1432.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">2489.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">2360.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">6378.2 ns</span></td></tr>
    <tr><td><strong><code>2049-4096</code></strong></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">1804.6 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">3910.1 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">4332.3 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">4833.8 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">7407.5 ns</span></td><td style="text-align: center;"><span style="font-family: monospace; font-size: 0.74em;">11013.2 ns</span></td></tr>
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
