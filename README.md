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
        dval_simplify.c
        ...
    qfloat/             High‑precision arithmetic
        qfloat.c
    datetime/           Date arithmetic and utilities
        datetime.c
        ...
    dictionary/         Hash map implementation
        dictionary.c
    set/                Set implementation
        set.c
    string/             String utilities
        string_core.c
        string_grapheme.c
        string_utf8.c
        ...

tests/
    test_dval.c
    test_qfloat.c
    test_datetime.c
    test_dictionary.c
    test_set.c
    test_string.c

Makefile                Build rules for debug/release/test
README.md               this README file
```

---

## 🔧 Building

The Makefile supports flexible targets for building, testing, and memory‑checking.

### Basic builds
```
make clean
make release
make debug
```

### Running specific tests
You can build and run individual test binaries directly:

```
make debug test_dval
make debug test_qfloat
make release test_set
```

### Memory‑checking (Valgrind)
If your environment supports memory‑checking tools:

```
make debug memtest_dval
make debug memtest_set
```

### Notes
- `debug` builds include assertions, symbols, and diagnostics.
- `release` builds are optimized and stripped.
- Test binaries are placed under `tests/build/<config>/`.

---

## 📘 Example: Single‑Variable Expression with First and Second Derivatives

The following example demonstrates how to:

- create a named variable  
- build a symbolic expression  
- compute the first derivative using `dv_create_deriv(x)`  
- compute the second derivative using `dv_get_deriv(df_dx)`  
- assign a value to the variable  
- evaluate the function and its derivatives  
- print symbolic and numeric results  
- correctly free all owning handles  

### Example Code

```c
#include <stdio.h>
#include "dval.h"

/* Build expression: f(x) = exp(sin(x)) + 3*x^2 - 7 */
dval_t *make_f(dval_t *x) {
    dval_t *sinx   = dv_sin(x);
    dval_t *exp_sx = dv_exp(sinx);
    dval_t *x2     = dv_pow_d(x, 2.0);
    dval_t *term2  = dv_mul_d(x2, 3.0);
    dval_t *f0     = dv_add(exp_sx, term2);
    dval_t *f      = dv_sub_d(f0, 7.0);   /* f = exp(sin(x)) + 3*x^2 - 7 */

    dv_free(sinx);
    dv_free(exp_sx);
    dv_free(x2);
    dv_free(term2);
    dv_free(f0);

    return f;    
}

int main(void) {
    /* Create a named variable x with initial value 1.25 */
    dval_t *x = dv_new_named_var_d(1.25, "x");

    /* Build expression:
         f(x) = exp(sin(x)) + 3*x^2 - 7
    */
    dval_t *f = make_f(x);

    /* First derivative (owning) */
    dval_t *df_dx = dv_create_deriv(f);   /* df/dx */

    /* Second derivative (borrowed) */
    const dval_t *d2f_dx = dv_get_deriv(df_dx);  /* d²f/dx² */

    /* Evaluate */
    qfloat f_val    = dv_eval(f);
    qfloat d1_val   = dv_eval(df_dx);
    qfloat d2_val   = dv_eval(d2f_dx);

    /* Print symbolic forms */
    printf("f(x)    = "); dv_print(f);
    printf("f'(x)   = "); dv_print(df_dx);
    printf("f''(x)  = "); dv_print(d2f_dx);

    /* Print numeric results */
    qf_printf("\nAt x = 1.25:\n");
    qf_printf("f(x)    = %.34q\n", f_val);
    qf_printf("f'(x)   = %.34q\n", d1_val);
    qf_printf("f''(x)  = %.34q\n", d2_val);

    /* Free owning handles */
    dv_free(df_dx);
    dv_free(f);
    dv_free(x);

    return 0;
}
```

### Expected Symbolic Output

```
f(x)    = { exp(sin x) + 3x² - 7 | x = 1.25 }
f'(x)   = { cos(x)*exp(sin(x)) + 6x | x = 1.25 }
f''(x)  = { exp(sin x) cos^2 x - sin x exp(sin x) + 6 | x = 1.25 }
```

### Expected Numeric Output at x = 1.25

```
At x = 1.25:
f(x)    = 0.2705855122552273437029639300166272
f'(x)   = 8.3145046259933109960293996152090640
f''(x)  = 3.8055231012396292258221776404244160
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
