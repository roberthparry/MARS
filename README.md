# MARS

![CI](https://github.com/rparry/MARS/actions/workflows/ci.yml/badge.svg)
![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)
![C99](https://img.shields.io/badge/C-99-blue.svg)

Portable C99 library for high-precision numerics, automatic differentiation,
datetime utilities, UTF-8 strings, and generic containers.

**Tested on:** Linux (GCC, Clang), macOS (Apple Clang), Windows (MSVC 2019+)

## Highlights

- **`mint_t`** — arbitrary-precision signed integers with number theory, combinatorics, and sequence helpers
- **`mfloat_t`** — opaque multiprecision floating-point values with exact conversion, pretty/scientific formatting, and a growing native math layer
- **`qfloat_t`** — double-double arithmetic and special functions (~34 decimal digits of precision)
- **`matrix_t`** — generic high-precision matrix with pluggable element types (`double`, `qfloat_t`, `qcomplex_t`, `dval_t *`), string-based matrix parsing and formatting, symbolic linear algebra support including Schur complements, block inverse/solve, Jordan helpers, entrywise matrix derivatives, Jacobian helpers, and first matrix-calculus helpers for trace, determinant, inverse, block inverse, solve, and block solve, and eigendecomposition at full `qfloat_t` precision
- **`dval_t`** — differentiable expression DAGs with first/second derivatives, symbolic matrix integration, and structural matcher helpers for higher-level symbolic code
- **`datetime_t`** — civil and astronomical date/time helpers
- **`dictionary_t` / `set_t` / `array_t`** — generic containers with user-defined ownership
- **`string_t`** — UTF-8-aware dynamic strings and grapheme operations
- **`bitset_t`** — dynamic thread-safe bitset with bitwise operations
- **`integrator_t`** — adaptive G7K15 / Turan T15/T4 integration with symbolic fast paths for affine-family `dval_t` integrands

## Requirements

- C99-compliant compiler (GCC ≥ 4.8, Clang ≥ 3.5, MSVC ≥ 2019)
- Standard C library plus `libm`
- Optional `libunistring` support for the UTF-8/string layer (`ENABLE_UNISTRING=1` by default in the Makefile)

## Benchmark Highlights

Recent sample benchmarks on this tree show:

- symbolic integrator shortcuts reducing affine-family cases from fallback-style
  tens of milliseconds to single-digit or low-double-digit microseconds
- `affine_square` at about `1.238 µs` versus `near_miss_square` at about
  `179.185 µs`
- `affine_times_exp` at about `19.045 µs` versus `near_miss_exp` at about
  `15188.762 µs`
- symbolic `dval` matrix solve for a dense `3x3` / `2`-RHS case at about
  `561.060 µs`
- symbolic `dval` matrix inverse for a dense `4x4` case at about
  `2525.437 µs`

See [`docs/benchmarks.md`](docs/benchmarks.md) for commands, units, and fuller
sample output.

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

**Automatic differentiation with `dval_t`:**

```c
#include <stdio.h>
#include "dval.h"

/* f(x) = exp(sin(x)) + 3*x^2 - 7 */
static dval_t *make_f(dval_t *x) {
    dval_t *sinx   = dv_sin(x);
    dval_t *exp_sx = dv_exp(sinx);
    dval_t *x2     = dv_pow_d(x, 2.0);
    dval_t *term2  = dv_mul_d(x2, 3.0);
    dval_t *f0     = dv_add(exp_sx, term2);
    dval_t *f      = dv_sub_d(f0, 7.0);

    dv_free(sinx);
    dv_free(exp_sx);
    dv_free(x2);
    dv_free(term2);
    dv_free(f0);

    return f;
}

int main(void) {
    dval_t *x = dv_new_named_var_d(1.25, "x");
    dval_t *f = make_f(x);
    dval_t *df_dx = dv_create_deriv(f, x);
    const dval_t *d2f_dx = dv_get_deriv(df_dx, x);

    printf("f(x)    = "); dv_print(f);
    printf("f'(x)   = "); dv_print(df_dx);
    printf("f''(x)  = "); dv_print(d2f_dx);

    qf_printf("\nAt x = 1.25:\n");
    qf_printf("f(x)    = %.34q\n", dv_eval(f));
    qf_printf("f'(x)   = %.34q\n", dv_eval(df_dx));
    qf_printf("f''(x)  = %.34q\n", dv_eval(d2f_dx));

    dv_free(df_dx);
    dv_free(f);
    dv_free(x);
    return 0;
}
```

```text
f(x)    = { exp(sin(x)) + 3x² - 7 | x = 1.25 }
f'(x)   = { 6x + cos(x)·exp(sin(x)) | x = 1.25 }
f''(x)  = { cos²(x)·exp(sin(x)) - sin(x)·exp(sin(x)) + 6 | x = 1.25 }

At x = 1.25:
f(x)    = 7.2705855122552272007396823028102510
f'(x)   = 7.5000000000000000000000000000000000
f''(x)  = 6.0000000000000000000000000000000000
```

**Symbolic matrix from a string:**

```c
#include <stdio.h>
#include <stdlib.h>
#include "matrix.h"

int main(void) {
    matrix_t *H = mat_from_string(
        "{ (Δ, Ω; Ω, -Δ) | Δ = 1.5; Ω = 0.25 }",
        NULL, NULL);

    mat_printf("%ml\n", H);
    mat_free(H);
    return 0;
}
```

```text
{ (
  Δ    Ω
  Ω   -Δ
) | Δ = 1.5, Ω = 0.25 }
```

**Searching for Mersenne primes with `mint_t` up to `p = 4423`:**

```c
#include <stdio.h>
#include "mint.h"

int main(void) {
    unsigned found = 0;
    unsigned p;

    for (p = 2; p <= 4423; ++p) {
        mint_t *exp = mi_create_long((long)p);
        mint_t *mersenne = NULL;
        mint_t *minus_one = mi_create_long(-1);

        if (!exp || !minus_one) {
            mi_free(exp);
            mi_free(mersenne);
            mi_free(minus_one);
            return 1;
        }

        if (mi_isprime(exp)) {
            mersenne = mi_create_2pow(p);
            if (!mersenne) {
                mi_free(exp);
                mi_free(minus_one);
                return 1;
            }

            if (mi_add(mersenne, minus_one) != 0) {
                mi_free(exp);
                mi_free(mersenne);
                mi_free(minus_one);
                return 1;
            }

            if (mi_isprime(mersenne)) {
                if ((found % 4) == 3)
                    printf("M_%-4u is prime\n", p);
                else
                    printf("M_%-4u is prime    ", p);
                found++;
            }

            mi_free(mersenne);
        }

        mi_free(exp);
        mi_free(minus_one);
    }

    return 0;
}
```

```text
M_2    is prime    M_3    is prime    M_5    is prime    M_7    is prime
M_13   is prime    M_17   is prime    M_19   is prime    M_31   is prime
M_61   is prime    M_89   is prime    M_107  is prime    M_127  is prime
M_521  is prime    M_607  is prime    M_1279 is prime    M_2203 is prime
M_2281 is prime    M_3217 is prime    M_4253 is prime    M_4423 is prime
```

## Modules

| Module | Description | Docs |
|---|---|---|
| `mint_t` | Arbitrary-precision signed integers and number-theory helpers | [`docs/mint.md`](docs/mint.md) |
| `mfloat_t` | Opaque multiprecision floating-point arithmetic | [`docs/mfloat.md`](docs/mfloat.md) |
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

## Run Benchmarks

```sh
make bench_integrator
make bench_matrix_dval
make bench_mint_mul
make bench_mint_div
make bench_mfloat_maths
```

See [`docs/building.md`](docs/building.md) for benchmark and build details.

## Documentation

- [Documentation index](docs/README.md)
- [Building](docs/building.md)
- [Testing](docs/testing.md)
- [Benchmarks](docs/benchmarks.md)
- [Dictionary ownership models](docs/dictionary.md#ownership-models)
- [Set ownership models](docs/set.md#ownership-models)
- [Array ownership models](docs/array.md#ownership-models)

## Directory Layout

```text
include/     public headers
include/internal/  project-internal cross-module headers
src/         implementations
tests/       unit tests
docs/        detailed module documentation
README.md    repository landing page
Makefile     build and test entry points
```

Public consumers should include headers from `include/`. Headers under
`include/internal/` support communication between MARS subsystems and tests,
and are not intended as stable external API.

## License

MIT License. See [`LICENSE`](LICENSE).
