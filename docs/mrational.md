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
  - canonical rational strings
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

- `1/2`
- `1/2`
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
(2/3) * (5/4) = 5/6
```

## Formatting And Conversion

- `mr_to_string(r)` returns a newly allocated canonical string
- integers print without a `/1` suffix
- non-integers print as `numerator/denominator`

Examples:

- `0`
- `-42`
- `5/6`

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
