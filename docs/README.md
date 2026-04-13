# MARS

Portable C99 library for high-precision numerics, automatic differentiation,
datetime utilities, UTF-8 strings, and generic containers.

## Highlights

- **`qfloat`** — double-double arithmetic and special functions
- **`dval_t`** — differentiable expression DAGs with first/second derivatives
- **`datetime_t`** — civil and astronomical date/time helpers
- **`dictionary_t` / `set_t`** — generic containers with user-defined ownership
- **`string_t`** — UTF-8-aware dynamic strings and grapheme operations

## Quick Example

```c
#include <stdio.h>
#include "qfloat.h"

int main(void) {
    qfloat x = qf_from_string("1");
    qfloat w = qf_lambert_w0(x);

    qf_printf("W0(1) = %.34q\n", w);
    return 0;
}
```

Expected output:

```text
W0(1) = 0.5671432904097838729999686622103575
```

## Modules

- [`qfloat`](docs/qfloat.md)
- [`dval_t`](docs/dval.md)
- [`datetime_t`](docs/datetime.md)
- [`dictionary_t`](docs/dictionary.md)
- [`set_t`](docs/set.md)
- [`string_t`](docs/string.md)

A module index is also available in [`docs/README.md`](docs/README.md).

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
make test_qfloat
make test_dval
make test_datetime
make test_dictionary
make test_set
make test_string
```

## Documentation

Detailed architecture notes and longer examples live in `docs/`.

## License

MIT License. See `LICENSE`.

# MARS Documentation

This directory contains the longer module documentation for MARS.

## Getting Started

- [Building](building.md)
- [Testing](testing.md)

## Modules

- [`qfloat`](qfloat.md) — double-double arithmetic and special functions
- [`dval_t`](dval.md) — differentiable expression DAGs
- [`datetime_t`](datetime.md) — civil and astronomical date/time utilities
- [`dictionary_t`](dictionary.md) — generic key/value storage
- [`set_t`](set.md) — generic set storage
- [`string_t`](string.md) — UTF-8-aware dynamic strings

## Notes

- The repository landing page is [`../README.md`](../README.md).
- These documents focus on API shape, examples, and implementation notes.