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
- **`number_t`** — generic numeric value cluster spanning exact, fixed-precision, and multiprecision backends behind one by-value public handle
- **`qfloat_t`** — double-double arithmetic and special functions (~34 decimal digits of precision)
- **`matrix_t`** — generic high-precision matrix over numeric `number_t` values or symbolic `dval_t *` entries, with string-based matrix parsing and formatting, symbolic linear algebra support including Schur complements, block inverse/solve, Jordan helpers, entrywise matrix derivatives, Jacobian helpers, first matrix-calculus helpers for trace, determinant, inverse, block inverse, solve, and block solve, and high-precision eigendecomposition and matrix functions through the numeric `number_t` layer
- **`dval_t`** — differentiable expression DAGs with first/second derivatives, symbolic matrix integration, and structural matcher helpers for higher-level symbolic code
- **`datetime_t`** — civil and astronomical date/time helpers
- **`dictionary_t` / `set_t` / `array_t`** — generic containers with user-defined ownership
- **`string_t`** — UTF-8-aware dynamic strings and grapheme operations
- **`bitset_t`** — dynamic thread-safe bitset with bitwise operations
- **`integrator_t`** — adaptive G7K15 / Turan T15/T4 integration with symbolic fast paths for affine-family `dval_t` integrands

## Requirements

- C99-compliant compiler (GCC ≥ 4.8, Clang ≥ 3.5, MSVC ≥ 2019)
- Standard C library plus `libm` and pthreads
- GMP, MPFR, and MPC development libraries
- Optional `libunistring` support for the UTF-8/string layer (`ENABLE_UNISTRING=1` by default in the Makefile)

On Debian/Ubuntu, install the default build requirements with:

```sh
sudo apt install build-essential libgmp-dev libmpfr-dev libmpc-dev libunistring-dev
```

Use `make check-deps` to check for required development headers and link
libraries before building or installing.

## Benchmark Highlights

Recent sample benchmarks on this tree show:

- symbolic integrator shortcuts reducing affine-family cases from fallback-style
  tens of milliseconds to low-hundreds of microseconds
- `affine_square` at about `233.723 µs` versus `near_miss_square` at about
  `1647.171 µs`
- `affine_times_exp` at about `77.678 µs` versus `near_miss_exp` at about
  `49084.036 µs`
- symbolic `dval` matrix solve for a dense `3x3` / `2`-RHS case at about
  `6000.421 µs`
- symbolic `dval` matrix inverse for a dense `4x4` case at about
  `40635.324 µs`
- `qfloat_t` `gamma(2.3)` at about `1.246 µs` and `lgamma(2.3)` at about
  `3.797 µs`
- `qfloat_t` `gammainv(119292.4619946090070787515047110059)` at about
  `82.548 µs`
- `qcomplex_t` `exp(1+i)` at about `2.513 µs` and `log(1+i)` at about
  `3.503 µs`
- `qcomplex_t` `gammainv(qc_gamma(2.5+0.3i))` at about `145.943 µs`

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

**Multiprecision floating-point arithmetic with `mfloat_t`:**

```c
#include <stdio.h>
#include "mfloat.h"

int main(void) {
    mfloat_t *x;
    mfloat_t *y;
    char buf[256];

    mf_set_default_precision(256);
    x = mf_create_string("2.3");
    y = mf_create_string("2.3");
    if (!x || !y)
        return 1;

    if (mf_gamma(x) != 0 || mf_lgamma(y) != 0)
        return 1;

    mf_sprintf(buf, sizeof(buf), "%.77mf", x);
    printf("gamma(2.3)  = %s\n", buf);

    mf_sprintf(buf, sizeof(buf), "%.77mf", y);
    printf("lgamma(2.3) = %s\n", buf);

    mf_free(x);
    mf_free(y);
    return 0;
}
```

```text
gamma(2.3)  = 1.16671190519816034504188144120291793853399434971946889397020666387299161947176
lgamma(2.3) = 0.15418945495963058108991791148922317269570397608961402272570768556406857691921
```

**Generic numeric arithmetic with `number_t`:**

```c
#include <stdio.h>
#include <stdlib.h>
#include "number.h"

int main(void) {
    number_t a = num_create_from_string("2");
    number_t b = num_create_from_string("5/6");
    number_t c = num_add(a, b);
    char *text = num_to_string(c);

    if (!text)
        return 1;

    printf("2 + 5/6 = %s\n", text);

    free(text);
    num_destroy(&a);
    num_destroy(&b);
    num_destroy(&c);
    return 0;
}
```

```text
2 + 5/6 = 17/6
```

For temporary-heavy internal code, `number_t` also supports optional lifetime
scopes. The intended fast pattern is to keep short-lived intermediates inside
the active scope while keeping rolling or returned values as ordinary owned
`number_t` results. See [`docs/number.md`](docs/number.md) for the scope
semantics and the `bench_number_scope` benchmark notes.

**Automatic differentiation with `dval_t`:**

```c
#include <stdio.h>
#include "dval.h"
#include "number.h"

/* f(x) = exp(sin(x)) + 3*x^2 - 7 */
static dval_t *make_f(dval_t *x) {
    number_t two = num_create_from_long(2);
    number_t three = num_create_from_long(3);
    number_t seven = num_create_from_long(7);
    dval_t *sinx   = dv_sin(x);
    dval_t *exp_sx = dv_exp(sinx);
    dval_t *x2     = dv_pow(x, &two);
    dval_t *term2  = dv_mul_num(x2, &three);
    dval_t *f0     = dv_add(exp_sx, term2);
    dval_t *f      = dv_sub_num(f0, &seven);

    dv_free(sinx);
    dv_free(exp_sx);
    dv_free(x2);
    dv_free(term2);
    dv_free(f0);
    num_destroy(&two);
    num_destroy(&seven);
    num_destroy(&three);

    return f;
}

int main(void) {
    number_t x0 = num_create_from_string("1.25");
    dval_t *x;
    dval_t *f;
    dval_t *df_dx;
    const dval_t *d2f_dx;
    number_t f_val;
    number_t d1_val;
    number_t d2_val;

    num_set_default_prec_bits(384);
    x = dv_new_named_var(x0, "x");
    num_destroy(&x0);
    f = make_f(x);
    df_dx = dv_create_deriv(f, x);
    d2f_dx = dv_get_deriv(df_dx, x);

    printf("f(x)    = "); dv_print(f);
    printf("f'(x)   = "); dv_print(df_dx);
    printf("f''(x)  = "); dv_print(d2f_dx);

    f_val = dv_eval(f);
    d1_val = dv_eval(df_dx);
    d2_val = dv_eval(d2f_dx);

    printf("\nAt x = 1.25 (384 bits):\n");
    num_printf("f(x)    = %.101N\n", f_val);
    num_printf("f'(x)   = %.101N\n", d1_val);
    num_printf("f''(x)  = %.101N\n", d2_val);

    num_destroy(&d2_val);
    num_destroy(&d1_val);
    num_destroy(&f_val);
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

At x = 1.25 (384 bits):
f(x)    = 0.2705855122552273437029639300167490299999821513753709749690393836985059027675459135561625639872826338
f'(x)   = 8.3145046259933109960293996152090642497796353985778106153481685326015106615810640203903619149909273414
f''(x)  = 3.8055231012396292258221776404244179176545341348683796728986430836039145902039198837528977153587143970
```

**Symbolic matrix from a string:**

```c
#include <stdio.h>
#include <stdlib.h>
#include "matrix.h"

int main(void) {
    mat_bindings_t *bindings = NULL;
    matrix_t *H = mat_from_string(
        "{ (Δ, Ω; Ω, -Δ) | Δ = 1.5; Ω = 0.25 }",
        &bindings);

    mat_printf("%ml\n", H);
    mat_bindings_free(bindings);
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
| `datetime_t` | Civil and astronomical date/time utilities | [`docs/datetime.md`](docs/datetime.md) |
| `string_t` | UTF-8-aware dynamic strings | [`docs/string.md`](docs/string.md) |
| `dictionary_t` | Generic key/value storage with ownership models | [`docs/dictionary.md`](docs/dictionary.md) |
| `set_t` | Generic set storage with ownership models | [`docs/set.md`](docs/set.md) |
| `array_t` | Generic array storage with ownership models | [`docs/array.md`](docs/array.md) |
| `bitset_t` | Dynamic thread-safe bitset | [`docs/bitset.md`](docs/bitset.md) |
| `mint_t` | Arbitrary-precision signed integers and number-theory helpers | [`docs/mint.md`](docs/mint.md) |
| `mrational_t` | Opaque exact rational arithmetic backed by `mint_t` | [`docs/mrational.md`](docs/mrational.md) |
| `number_t` | Generic numeric value cluster over exact, fixed-precision, and multiprecision backends | [`docs/number.md`](docs/number.md) |
| `qfloat_t` | Double-double arithmetic and special functions | [`docs/qfloat.md`](docs/qfloat.md) |
| `qcomplex_t` | Double-double complex arithmetic and special functions | [`docs/qcomplex.md`](docs/qcomplex.md) |
| `mfloat_t` | Opaque multiprecision floating-point arithmetic | [`docs/mfloat.md`](docs/mfloat.md) |
| `mcomplex_t` | Opaque multiprecision complex arithmetic backed by `mfloat_t` | [`docs/mcomplex.md`](docs/mcomplex.md) |
| `matrix_t` | Generic high-precision matrix with numeric and symbolic element types | [`docs/matrix.md`](docs/matrix.md) |
| `dval_t` | Differentiable expression DAGs with matrix integration | [`docs/dval.md`](docs/dval.md) |
| `integrator_t` | Adaptive G7K15 numerical integrator | [`docs/integrator.md`](docs/integrator.md) |

## Build

```sh
make
```

See [`docs/building.md`](docs/building.md) for configuration options and cross-compilation notes.

## Install

Install MARS headers and libraries after building:

```sh
sudo make install
```

To check required development libraries before building or installing:

```sh
make check-deps
```

By default this installs to `/usr/local`, with headers under
`/usr/local/include/mars` and libraries under `/usr/local/lib`. Override
`PREFIX`, `LIBDIR`, `INCLUDEDIR`, or `DESTDIR` for packaged or custom installs.

`make install` installs MARS itself only. It does not install system libraries;
install the requirements above with your system package manager first.

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
make bench_mcomplex_maths
```

See [`docs/building.md`](docs/building.md) for benchmark and build details.

## Documentation

- [Documentation index](docs/README.md)
- [Building](docs/building.md)
- [Testing](docs/testing.md)
- [Benchmarks](docs/benchmarks.md)

## Directory Layout

```text
include/     public headers
src/         implementations
tests/       unit tests
docs/        detailed module documentation
README.md    repository landing page
Makefile     build and test entry points
```

Public consumers should include headers from `include/`. Headers under
`include/internal/` support communication between MARS subsystems and tests,
and are not intended as stable external API.

## Acknowledgements

MARS's arbitrary-precision numeric layers build directly on the GNU
multiprecision ecosystem:

- [GMP](https://gmplib.org/) provides the arbitrary-precision integer and
  rational arithmetic foundation used by `mint_t` and `mrational_t`.
- [MPFR](https://www.mpfr.org/) provides correctly rounded multiprecision
  floating-point arithmetic used by `mfloat_t`.
- [MPC](https://www.multiprecision.org/mpc/) provides multiprecision complex
  arithmetic used by `mcomplex_t`.

These projects do the heavy mathematical lifting for the modern MARS
multiprecision wrappers. MARS depends on their development headers and runtime
libraries when those modules are built.

## License

MIT License. See [LICENSE](https://en.wikipedia.org/wiki/MIT_License).
