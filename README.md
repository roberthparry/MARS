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

- [`qfloat`](docs/qfloat.md) — double-double arithmetic and special functions
- [`dval_t`](docs/dval.md) — differentiable expression DAGs
- [`datetime_t`](docs/datetime.md) — civil and astronomical date/time utilities
- [`dictionary_t`](docs/dictionary.md) — generic key/value storage
- [`set_t`](docs/set.md) — generic set storage
- [`string_t`](docs/string.md) — UTF-8-aware dynamic strings

## Documentation

- [Documentation index](docs/README.md)
- [Building](docs/building.md)
- [Testing](docs/testing.md)
- [Dictionary ownership models](docs/dictionary.md#ownership-models)
- [Set ownership models](docs/set.md#ownership-models)

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

See [`docs/building.md`](docs/building.md).

## Run Tests

See [`docs/testing.md`](docs/testing.md).

## License

MIT License. See `LICENSE`.