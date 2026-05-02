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
    mint_t *n = mi_create_long(52);
    mint_t *k = mi_create_long(5);
    mint_t *c = NULL;

    if (!n || !k)
        return 1;

    c = mi_clone(n);
    if (!c || mi_binomial(c, k) != 0)
        return 1;

    printf("C(52, 5) = %s\n", mi_to_string(c));

    mi_free(c);
    mi_free(k);
    mi_free(n);
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
