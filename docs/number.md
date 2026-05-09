# `number_t`

`number_t` is MARS's generic numeric value cluster.

It gives the library one by-value public numeric handle that can represent:

- exact integers via `mint_t`
- exact rationals via `mrational_t`
- fixed-precision real values via `double` and `qfloat_t`
- fixed-precision complex values via `qcomplex_t`
- multiprecision real values via `mfloat_t`
- multiprecision complex values via `mcomplex_t`

The goal is to let callers work at the level of "a number" without having to
manually pick a backend for every operation.

## Public Representation

`number_t` is a fixed-size public storage envelope:

```c
typedef struct _number_t {
    uint64_t storage[5];
} number_t;
```

Callers may:

- pass it by value
- return it by value
- store it on the stack

but must treat the storage as opaque.

## Parsing Policy

`num_create_string(...)` chooses the most suitable backend by syntax:

- integer text -> `mint_t`
- `a/b` fraction text -> `mrational_t`
- decimal or scientific real text -> `mfloat_t`
- complex text -> `mcomplex_t`

Examples:

- `"42"` -> exact integer
- `"5/6"` -> exact rational
- `"32.123"` -> multiprecision real
- `"1 + 2i"` -> multiprecision complex

## Precision Model

For multiprecision construction, `number_t` uses a default working precision of
`1024` bits unless the caller requests something else explicitly.

This applies to:

- `num_create_string(...)` for decimal/scientific real and complex input
- `num_create_mfloat(...)`
- `num_create_mcomplex(...)`

Explicit precision constructors are also available:

- `num_create_mfloat_prec(...)`
- `num_create_mfloat_digits(...)`
- `num_create_mcomplex_prec(...)`
- `num_create_mcomplex_digits(...)`

Exact integer and rational values remain exact rather than being rounded to the
default floating precision.

## Ownership Model

`number_t` uses a by-value public API, but some values may manage heap-backed
internal state.

That means:

- functions returning `number_t` return a live value
- callers should call `num_clear(&value)` when finished with such a value
- passing a `number_t` by value performs only a shallow copy
- use `num_clone(...)` when an independent live copy is required

Example:

```c
number_t a = num_create_string("2");
number_t b = num_create_string("5/6");
number_t c = num_add(a, b);

num_clear(&a);
num_clear(&b);
num_clear(&c);
```

Named constants such as `num_pi()`, `num_e()`, and `num_euler_mascheroni()`
are safe to clear as well:

```c
number_t pi = num_pi();
num_clear(&pi);
```

## Formatting

`number_t` provides its own formatting helpers:

- `num_printf(...)`
- `num_sprintf(...)`
- `num_vsprintf(...)`
- `num_to_string(...)`

Supported format specifiers:

- `%n` — pretty human-readable form
- `%N` — scientific form for floating-point and complex values, otherwise the
  ordinary exact form

Examples:

- integer and rational values print exactly:
  - `42`
  - `5/6`
- inexact floating and complex values print in a human-friendly form rather
  than a full exact binary-to-decimal dump

## Comparisons

`number_t` supports cross-backend equality by promoting operands to a sensible
common representation.

- `num_eq(a, b)` works across mixed backends
- ordering helpers:
  - `num_lt`
  - `num_le`
  - `num_gt`
  - `num_ge`
  - `num_cmp`
  are only meaningful for real values

For non-real values:

- the ordering predicates return `false`
- `num_cmp(...)` returns `0`

This avoids pretending complex values have a natural total order.

## Arithmetic And Functions

The generic layer exposes:

- core arithmetic:
  - `num_add`
  - `num_sub`
  - `num_mul`
  - `num_div`
  - `num_inv`
  - `num_neg`
- real/complex helpers:
  - `num_abs`
  - `num_conj`
  - `num_real_part`
  - `num_imag_part`
  - `num_arg`
- elementary functions:
  - `num_exp`
  - `num_log`
  - `num_sqrt`
  - `num_sin`, `num_cos`, `num_tan`
  - `num_sinh`, `num_cosh`, `num_tanh`
  - `num_atan2`
- special functions:
  - `num_gamma`
  - `num_lgamma`
  - `num_digamma`
  - `num_trigamma`
  - `num_erf`
  - `num_erfc`
  - `num_lambert_w0`
  - `num_beta`
  - `num_logbeta`
  - `num_binomial`
  - `num_normal_pdf`
  - `num_normal_cdf`
  - `num_e1`
  - `num_ei`

Binary arithmetic mixes supported backends automatically by promoting to a
common target representation before dispatch.

## Example

```c
#include <stdio.h>
#include <stdlib.h>
#include "number.h"

int main(void) {
    number_t a = num_create_string("2");
    number_t b = num_create_string("3");
    number_t c = num_create_string("5/6");
    number_t sum = num_add(a, c);
    number_t beta = num_beta(a, b);
    char *sum_text = num_to_string(sum);
    char *beta_text = num_to_string(beta);

    if (!sum_text || !beta_text)
        return 1;

    printf("2 + 5/6 = %s\n", sum_text);
    printf("beta(2, 3) = %s\n", beta_text);

    free(sum_text);
    free(beta_text);
    num_clear(&a);
    num_clear(&b);
    num_clear(&c);
    num_clear(&sum);
    num_clear(&beta);
    return 0;
}
```

```text
2 + 5/6 = 17/6
beta(2, 3) = 0.083333333333333333333333333333333
```
