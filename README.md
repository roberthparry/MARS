# MARS

![CI](https://github.com/rparry/MARS/actions/workflows/ci.yml/badge.svg)
![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)
![C99](https://img.shields.io/badge/C-99-blue.svg)

Portable C99 library for high-precision numerics, automatic differentiation,
datetime utilities, UTF-8 strings, and generic containers.

**Tested on:** Linux (GCC, Clang), macOS (Apple Clang), Windows (MSVC 2019+)

## Highlights

- **`qfloat_t`** — double-double arithmetic and special functions (~34 decimal digits of precision)
- **`matrix_t`** — generic high-precision matrix with pluggable element types (`double`, `qfloat_t`, `qcomplex_t`, `dval_t *`), symbolic matrix support, and eigendecomposition at full `qfloat_t` precision
- **`dval_t`** — differentiable expression DAGs with first/second derivatives and symbolic matrix integration
- **`datetime_t`** — civil and astronomical date/time helpers
- **`dictionary_t` / `set_t` / `array_t`** — generic containers with user-defined ownership
- **`string_t`** — UTF-8-aware dynamic strings and grapheme operations
- **`bitset_t`** — dynamic thread-safe bitset with bitwise operations
- **`integrator_t`** — adaptive G7K15 numerical integration at qfloat_t precision

## Requirements

- C99-compliant compiler (GCC ≥ 4.8, Clang ≥ 3.5, MSVC ≥ 2019)
- Standard C library only — no external dependencies

## Quick Examples

**High-precision arithmetic with `qfloat_t`:**

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

```text
W0(1) = 0.5671432904097838729999686622103575
```

## Modules

| Module | Description | Docs |
|---|---|---|
| `qfloat_t` | Double-double arithmetic and special functions | [`docs/qfloat.md`](docs/qfloat.md) |
| `qcomplex_t` | Double-double complex arithmetic and special functions | [`docs/qcomplex.md`](docs/qcomplex.md) |
| `matrix_t` | Generic high-precision matrix with numeric and symbolic element types | [`docs/matrix.md`](docs/matrix.md) |
| `dval_t` | Differentiable expression DAGs with matrix integration | [`docs/dval.md`](docs/dval.md) |
| `datetime_t` | Civil and astronomical date/time utilities | [`docs/datetime.md`](docs/datetime.md) |
| `dictionary_t` | Generic key/value storage with ownership models | [`docs/dictionary.md`](docs/dictionary.md) |
| `set_t` | Generic set storage with ownership models | [`docs/set.md`](docs/set.md) |
| `array_t` | Generic array storage with ownership models | [`docs/array.md`](docs/array.md) |
| `string_t` | UTF-8-aware dynamic strings | [`docs/string.md`](docs/string.md) |
| `bitset_t` | Dynamic thread-safe bitset | [`docs/bitset.md`](docs/bitset.md) |
| `integrator_t` | Adaptive G7K15 numerical integrator | [`docs/integrator.md`](docs/integrator.md) |

## Build

```sh
make
```

See [`docs/building.md`](docs/building.md) for configuration options and cross-compilation notes.

## Run Tests

```sh
make test
```

See [`docs/testing.md`](docs/testing.md) for details on individual test suites.

## Documentation

- [Documentation index](docs/README.md)
- [Building](docs/building.md)
- [Testing](docs/testing.md)
- [Dictionary ownership models](docs/dictionary.md#ownership-models)
- [Set ownership models](docs/set.md#ownership-models)
- [Array ownership models](docs/array.md#ownership-models)

## Directory Layout

```text
include/     public headers
src/         implementations
tests/       unit tests
docs/        detailed module documentation
README.md    repository landing page
Makefile     build and test entry points
```

## License

MIT License. See [`LICENSE`](LICENSE).
