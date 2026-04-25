# MARS

Portable C99 library for high-precision numerics, automatic differentiation,
datetime utilities, UTF-8 strings, and generic containers.

## Highlights

- **`qfloat_t`** — double-double arithmetic and special functions (~34 decimal digits)
- **`qcomplex_t`** — double-double complex arithmetic and special functions
- **`matrix_t`** — generic high-precision matrix: eigendecomposition, matrix functions (exp, log, sqrt, trig, …), pluggable element types
- **`dval_t`** — differentiable expression DAGs with first/second derivatives
- **`datetime_t`** — civil and astronomical date/time helpers
- **`dictionary_t` / `set_t` / `array_t`** — generic containers with user-defined ownership
- **`string_t`** — UTF-8-aware dynamic strings and grapheme operations
- **`integrator_t`** — adaptive G7K15 numerical integration at `qfloat_t` precision

## Quick Example

```c
#include <stdio.h>
#include "qfloat.h"

int main(void) {
    qfloat_t x = qf_from_string("1");
    qfloat_t w = qf_lambert_w0(x);

    qf_printf("W0(1) = %.34q\n", w);
    return 0;
}
```

Expected output:

```text
W0(1) = 0.5671432904097838729999686622103575
```

## Modules

- [`qfloat_t`](qfloat.md)
- [`qcomplex_t`](qcomplex.md)
- [`dval_t`](dval.md)
- [`datetime_t`](datetime.md)
- [`dictionary_t`](dictionary.md)
- [`set_t`](set.md)
- [`array_t`](array.md)
- [`string_t`](string.md)
- [`bitset_t`](bitset.md)

## Directory Layout

```text
include/     public headers
src/         implementations
tests/       unit tests
docs/        detailed module documentation
README.md    repository landing page
Makefile     build and test entry points
```

## Build

```sh
make release
```

## Run Tests

```sh
make test
```

Or run individual suites:

```sh
make test_qfloat
make test_qcomplex
make test_matrix
make test_dval
make test_datetime
make test_dictionary
make test_set
make test_array
make test_string
make test_bitset
make test_integrator
```

## Documentation

Detailed architecture notes and longer examples live in `docs/`.

## Licence

MIT Licence. See `LICENSE`.

# MARS Documentation

This directory contains the longer module documentation for MARS.

## Getting Started

- [Building](building.md)
- [Testing](testing.md)

## Modules

- [`qfloat_t`](qfloat.md) — double-double arithmetic and special functions
- [`qcomplex_t`](qcomplex.md) — double-double complex arithmetic and special functions
- [`matrix_t`](matrix.md) — generic high-precision matrix with pluggable element types and storage kinds
- [`dval_t`](dval.md) — differentiable expression DAGs
- [`datetime_t`](datetime.md) — civil and astronomical date/time utilities
- [`dictionary_t`](dictionary.md) — generic key/value storage
- [`set_t`](set.md) — generic set storage
- [`array_t`](array.md) — generic array storage
- [`string_t`](string.md) — UTF-8-aware dynamic strings
- [`bitset_t`](bitset.md) — dynamic thread-safe bitset
- [`integrator_t`](integrator.md) — adaptive G7K15 / Turán T15 numerical integrator

## Guides

- [`dictionary_t` ownership models](dictionary.md#ownership-models)
- [`set_t` ownership models](set.md#ownership-models)
- [`array_t` ownership models](array.md#ownership-models)

## Notes

- The repository landing page is [`../README.md`](../README.md).
- These documents focus on API shape, examples, and implementation notes.