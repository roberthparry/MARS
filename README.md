# OOC — A Modular C Library for Numerical Computing, High‑Precision Arithmetic, and Differentiable Values

OOC is a modular C library providing high‑precision arithmetic (`qfloat`), differentiable values (`dval_t`), symbolic expression trees, datetime utilities, dictionary and set types, and a suite of supporting algorithms. The project emphasizes correctness, maintainability, and extensibility, with a clean directory structure and a Makefile‑driven build system.

---

## 🚀 Features

### High‑Precision Arithmetic (`qfloat`)
- Double‑double precision floating‑point type
- Normalization and error‑controlled arithmetic kernels
- Allocation‑free formatting routines
- Designed for numerical stability and reproducibility

### Differentiable Values (`dval_t`)
- Lazy DAG representation of symbolic expressions
- Automatic differentiation
- Operator‑precedence‑aware string formatting
- Reference‑counted ownership model
- Modular vtable‑driven architecture for extensibility

### Datetime Utilities
- Date arithmetic (addition, subtraction, comparisons)
- Parsing and formatting helpers
- Fully tested for correctness

### Data Structures
- Dictionary (hash map)
- Set type
- String utilities
- All implemented in C with predictable memory ownership

### Build System
- Makefile with `debug` and `release` configurations
- Out‑of‑source build artifacts under `tests/build/`

### Test Suite
- Unit tests for all major subsystems
- Debug and release builds
- Designed for incremental expansion

---

## 📁 Directory Structure

```
include/                Public headers
    dval.h              Differentiable value API
    qfloat.h            High‑precision arithmetic API
    dictionary.h        Hash map API
    set.h               Set API
    string.h            String utilities
    datetime.h          Date/time utilities

src/
    dval/               Differentiable value engine
        dval_core.c
        dval_tostring.c
        dval_ops.c
        ...
    qfloat/             High‑precision arithmetic
        qfloat.c
        qfloat_format.c
        ...
    datetime/           Date arithmetic and utilities
        date_arithmetic.c
        ...
    dictionary/         Hash map implementation
        dictionary.c
    set/                Set implementation
        set.c
    string/             String utilities
        string.c

tests/
    test_dval.c
    test_qfloat.c
    test_datetime.c
    test_dictionary.c
    test_set.c
    test_string.c

tests/build/
    debug/              Debug binaries (ignored)
    release/            Release binaries (ignored)

Makefile                Build rules for debug/release/test
```

---

## 🔧 Building

### Debug build
```
make debug
```

### Release build
```
make release
```

### Clean
```
make clean
```

---

## 🧪 Running Tests

After building:

```
./tests/build/debug/test_dval
./tests/build/debug/test_qfloat
./tests/build/debug/test_datetime
./tests/build/debug/test_dictionary
./tests/build/debug/test_set
./tests/build/debug/test_string
```

(You can add a `make test` target later if you want automation.)

---

## 🧩 Example: Using `dval_t`

```c
#include "dval.h"

int main() {
    dval_t *x = dv_var("x");
    dval_t *expr = dv_add(dv_mul(x, x), dv_const(3.0));

    printf("%s\n", dv_to_string(expr));  // prints something like: { x*x + 3 | x = ? }

    dv_release(expr);
    dv_release(x);
}
```

---

## 🛣 Roadmap

- More simplification rules for symbolic expressions
- Additional qfloat kernels
- GitHub Actions CI
- Documentation site
- Benchmarks and performance suite
- More datetime utilities

---

## 📄 License

MIT (or whichever you choose)
