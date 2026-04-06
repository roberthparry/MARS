# **OOC — A Modular C Library for High‑Precision Numerics, Automatic Differentiation,  Navigation‑Grade Datetime handling, UTF‑8 Strings, and Robust Generic Containers**

OOC is a modular, portable C library designed for correctness, clarity, and long‑term maintainability.  
It provides:

- high‑precision arithmetic (`qfloat`)
- differentiable values (`dval_t`)
- civil & astronomical datetime utilities (`datetime_t`)
- generic dictionary and set types
- a UTF‑8 aware dynamic string type
- a suite of supporting algorithms and helpers

Every module is self‑contained, header‑driven, and usable independently.

---

## 🧭 **Table of Contents**

- [Features](#-features)
- [Core Modules](#-core-modules)
- [Directory Structure](#-directory-structure)
- [Getting Started](#-getting-started)
- [Building](#-building)
- [Roadmap](#-roadmap)
- [License](#-license)

---

## 🚀 **Features**

- **High‑precision arithmetic** (double‑double, ~106 bits)  
- **Differentable values** with lazy DAG evaluation  
- **Civil & astronomical datetime** with sunrise/sunset, moon phase, holidays  
- **Generic dictionary & set** with value‑semantics and lazy sorting  
- **UTF‑8 string type** with normalization, grapheme support, and builder API  
- **Clean, portable C99 codebase**  
- **Makefile‑driven build system** (debug/release/test)  
- **Extensive test suite**  

---

## 🧩 **Core Modules**

### **Module Overview**

| Module        | Purpose | Highlights |
|---------------|---------|------------|
| `qfloat`      | Double‑double precision arithmetic | Elementary & special functions, exact parsing, `%q`/`%Q` formatting |
| `dval_t`      | Differentiable values | Lazy DAG, automatic differentiation, reference counting |
| `datetime_t`  | Civil & astronomical datetime | Julian Day, sunrise/sunset, moon phase, formatting engine |
| `dictionary_t`| Generic key/value dictionary | Value‑semantics, stable entry handles, lazy sorted views |
| `set_t`       | Generic hash set | Dense arena, set algebra, lazy sorted iteration |
| `string_t`    | UTF‑8 dynamic string | Normalization, graphemes, builder API, split/join |

---

## 🧮 **High‑Precision Arithmetic (`qfloat`)**

A double‑double precision floating‑point type (~106 bits, ~32 decimal digits) implemented as an unevaluated sum of two IEEE‑754 doubles. Includes:

- error‑free transformations (TwoSum, TwoProd)  
- full suite of elementary functions  
- special functions (gamma, erf, Lambert W, incomplete gamma, etc.)  
- exact decimal parsing and formatting  
- `%q` / `%Q` printf extensions  

---

### 📘 **Example: High‑Precision Lambert W Function**

The Lambert W function solves the equation:



\[
W(x)\,e^{W(x)} = x
\]



and appears in combinatorics, physics, nonlinear equations, and delay‑differential systems.  
`qfloat` computes it to full double‑double precision (~106 bits), making it ideal for numerically sensitive problems.

#### Example Code

```c
#include <stdio.h>
#include "qfloat.h"

int main(void) {
    /* Compute W0(x) for several representative values */
    const char *inputs[] = {
        "0",
        "1e-6",
        "0.1",
        "1",
        "5",
        "-0.3678794411714423215955237701614609", /* -1/e */
        NULL
    };

    char buf[256];

    for (int i = 0; inputs[i] != NULL; i++) {
        /* Parse x from a decimal string */
        qfloat x = qf_from_string(inputs[i]);

        /* Compute the principal branch W0(x) */
        qfloat w = qf_lambert_w0(x);

        /* Convert to a readable high‑precision string */
        qf_to_string(w, buf, sizeof(buf));

        printf("W0(%s) = %s\n", inputs[i], buf);
    }

    return 0;
}
```

#### Example Output

```
W0(0) = 0
W0(1e-6) = 9.999990000014999973333385416558667e-7
W0(0.1) = 0.09127652716086226429989572142317956
W0(1) = 0.56714329040978387299996866221035555
W0(5) = 1.3267246652422002236350992977580797
W0(-0.3678794411714423215955237701614609) = -1
```

---

## 🔧 **Differentiable Values (`dval_t`)**

A lazy, vtable‑driven, reference‑counted DAG of differentiable expressions (automatic differentiation).

- variables, constants, named values  
- arithmetic and transcendental operators  
- lazy derivative construction (`dv_get_deriv`)  
- owning vs borrowed handles with strict lifetime rules  
- symbolic printing and numeric evaluation via `qfloat`  

---

### 📘 **Example: Single‑Variable Expression with First and Second Derivatives**

The following example demonstrates how to:

- create a named variable  
- build an expression  
- compute the first derivative using `dv_create_deriv(x)`  
- compute the second derivative using `dv_get_deriv(df_dx)`  
- assign a value to the variable  
- evaluate the function and its derivatives  
- print symbolic and numeric results  
- correctly free all owning handles  

#### Example Code

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

#### Expected Symbolic Output

```
f(x)    = { exp(sin(x)) + 3x² + 7 | x = 1.25 }
f'(x)   = { cos(x)·exp(sin(x)) + 6x | x = 1.25 }
f''(x)  = { cos²(x)·exp(sin(x)) - sin(x)·exp(sin(x)) + 6 | x = 1.25 }
```

#### Expected Numeric Output at x = 1.25

```
At x = 1.25:
f(x)    = 0.2705855122552273437029639300166272
f'(x)   = 8.3145046259933109960293996152090640
f''(x)  = 3.8055231012396292258221776404244160
```

---

## 📅 **Civil & Astronomical Datetime (`datetime_t`)**

A high‑level civil datetime type with full Gregorian calendar support, Julian Day conversions, timezone/DST helpers, sunrise/sunset algorithms, moon‑phase computation, and a rich formatting engine.

**Highlights**
- Construct from Y/M/D, Y/M/D+time, Julian Day, system clock  
- Holiday algorithms: Easter Sunday, Chinese New Year  
- Astronomical utilities: sunrise/sunset (NOAA), moon phase  
- Full arithmetic: add/subtract years, months, days, hours, minutes, seconds  
- Duration computation with `datetime_span_t`  
- Rich formatting mini‑language (`%ddd`, `%mmmm`, `%O`, `@hh`, etc.)  
- Comparison, hashing, weekday calculation, leap‑year logic  

---

### 📘 **Example: Chinese New Year Calculation**

`datetime_t` includes helpers for culturally and astronomically significant dates.  
The function `datetime_initWithChineseNewYear()` computes the date of Chinese New Year for any supported year (1700–2400).

#### Example Code

```c
#include <stdio.h>
#include "datetime.h"

int main(void) {
    struct {
        int year;
        month_t month;
        unsigned char day;
    } cases[] = {
        {2020, DT_January, 25},
        {2021, DT_February, 12},
        {2022, DT_February, 1},
        {2023, DT_January, 22},
        {2024, DT_February, 10},
        {2025, DT_January, 29},
    };

    for (int i = 0; i < 6; i++) {
        /* Compute Chinese New Year for the given year */
        datetime_t *dt = datetime_initWithChineseNewYear(
            datetime_alloc(),
            cases[i].year
        );

        if (!dt) {
            printf("Year %d is outside the supported range.\n", cases[i].year);
            continue;
        }

        /* Extract components */
        short y = datetime_getYear(dt);
        month_t m = datetime_getMonth(dt);
        unsigned char d = datetime_getDay(dt);

        printf("Chinese New Year %d: %d-%02d-%02d\n",
               cases[i].year, y, (int)m, d);

        datetime_dealloc(dt);
    }

    return 0;
}
```

#### Example Output

```
Chinese New Year 2020: 2020-01-25
Chinese New Year 2021: 2021-02-12
Chinese New Year 2022: 2022-02-01
Chinese New Year 2023: 2023-01-22
Chinese New Year 2024: 2024-02-10
Chinese New Year 2025: 2025-01-29
```

---

## 📚 **Generic Dictionary (`dictionary_t`)**

A typed, generic key/value dictionary with user‑defined hash/compare/clone/destroy callbacks, stable entry handles, and lazy sorted views.

**Features**
- Keys and values stored **by value** (inline)  
- Insert, replace, remove by key  
- Lookup by key (value copied out)  
- Opaque entry handles for efficient repeated access  
- Sorted iteration by key or value (lazy, rebuilt on mutation)  
- Deep or shallow copy semantics  
- Full dictionary cloning  

---

### 📘 **Example: Deep‑Copied Keys and Values**

This example shows how to configure a dictionary where both keys and values  
are deep‑copied using custom clone/destroy callbacks.  
Each element contains a dynamically allocated string, so the dictionary  
owns its own copies.

```c
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "dictionary.h"

struct deep {
    char *name;
    int value;
};

static size_t deep_hash(const void *p) {
    const struct deep *d = p;
    size_t h = 146527;
    const char *s = d->name;
    while (*s) h = (h * 33) ^ (unsigned char)*s++;
    return h ^ (size_t)d->value;
}

static int deep_cmp(const void *a, const void *b) {
    const struct deep *da = a;
    const struct deep *db = b;
    int c = strcmp(da->name, db->name);
    if (c != 0) return c;
    return (da->value > db->value) - (da->value < db->value);
}

static void deep_clone(void *dst, const void *src) {
    const struct deep *s = src;
    struct deep *d = dst;
    d->value = s->value;
    d->name = strdup(s->name);
}

static void deep_destroy(void *elem) {
    struct deep *d = elem;
    free(d->name);
}

int main(void) {
    dictionary_t *dict = dictionary_create(
        sizeof(struct deep), sizeof(struct deep),
        deep_hash, deep_cmp,
        deep_clone, deep_destroy,
        deep_clone, deep_destroy
    );

    struct deep k1 = { "k1", 1 };
    struct deep v1 = { "v1", 10 };

    struct deep k2 = { "k2", 2 };
    struct deep v2 = { "v2", 20 };

    dictionary_set(dict, &k1, &v1);
    dictionary_set(dict, &k2, &v2);

    struct deep out;

    if (dictionary_get(dict, &k1, &out)) {
        printf("Value for key '%s': %s (%d)\n",
               k1.name, out.name, out.value);
        free(out.name);
    }

    if (dictionary_get(dict, &k2, &out)) {
        printf("Value for key '%s': %s (%d)\n",
               k2.name, out.name, out.value);
        free(out.name);
    }

    dictionary_destroy(dict);
    return 0;
}
```

#### Example Output

```
Value for key 'k1': v1 (10)
Value for key 'k2': v2 (20)
```

---

### 📘 **Example: Integer Keys and Deep‑Copied String Values**

This example shows a dictionary where integer keys are stored by value  
(shallow copy), and string values are deep‑copied.

```c
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "dictionary.h"

static size_t int_hash(const void *p) {
    int v;
    memcpy(&v, p, sizeof(int));
    return (size_t)v * 2654435761u;
}

static int int_cmp(const void *a, const void *b) {
    int x, y;
    memcpy(&x, a, sizeof(int));
    memcpy(&y, b, sizeof(int));
    return (x > y) - (x < y);
}

static void str_clone(void *dst, const void *src) {
    const char *s = *(const char **)src;
    *(char **)dst = strdup(s);
}

static void str_destroy(void *elem) {
    free(*(char **)elem);
}

int main(void) {
    dictionary_t *dict = dictionary_create(
        sizeof(int), sizeof(char *),
        int_hash, int_cmp,
        NULL, NULL,
        str_clone, str_destroy
    );

    int k1 = 5, k2 = 6;
    const char *v1 = "hello";
    const char *v2 = "world";

    dictionary_set(dict, &k1, &v1);
    dictionary_set(dict, &k2, &v2);

    char *out;

    if (dictionary_get(dict, &k1, &out)) {
        printf("Value for key %d: %s\n", k1, out);
        free(out);
    }

    if (dictionary_get(dict, &k2, &out)) {
        printf("Value for key %d: %s\n", k2, out);
        free(out);
    }

    dictionary_destroy(dict);
    return 0;
}
```

#### Example Output

```
Value for key 5: hello
Value for key 6: world
```

---

## 🧩 **Generic Hash Set (`set_t`)**

A typed hash set with dense arena storage, precomputed hashes, lazy sorted views,  
and full set algebra.

**Features**
- Add/remove/contains  
- Dense arena (compact, no holes)  
- Lazy sorted iteration  
- Clone, union, intersection, difference  
- Subset testing  
- Value‑semantics with optional deep copy  

---

### 📘 **Example: String Set With Deep‑Copied Elements**

This example demonstrates how to create a set of strings where each element  
is deep‑copied into the set’s arena. The set owns its copies and frees them  
automatically when destroyed.

```c
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "set.h"

/* Hash a char* by hashing the pointed‑to string */
static size_t str_hash(const void *p) {
    const char *s = *(const char * const *)p;
    size_t h = 146527;
    while (*s) {
        h = (h * 33) ^ (unsigned char)*s++;
    }
    return h;
}

/* Compare two char* values by strcmp */
static int str_cmp(const void *a, const void *b) {
    const char *sa = *(const char * const *)a;
    const char *sb = *(const char * const *)b;
    return strcmp(sa, sb);
}

/* Deep clone: duplicate the string */
static void str_clone(void *dst, const void *src) {
    const char *s = *(const char * const *)src;
    char *copy = strdup(s);
    memcpy(dst, &copy, sizeof(char *));
}

/* Destroy: free the owned string */
static void str_destroy(void *elem) {
    char *s = *(char **)elem;
    free(s);
}

int main(void) {
    /* Create a set of strings with deep‑copy semantics */
    set_t *s = set_create(
        sizeof(char *),
        str_hash,
        str_cmp,
        str_clone,
        str_destroy
    );

    const char *a = "hello";
    const char *b = "world";
    const char *c = "hello"; /* duplicate */

    set_add(s, &a);
    set_add(s, &b);

    if (set_contains(s, &a))
        printf("'hello' is in the set\n");

    if (!set_add(s, &c))
        printf("Duplicate 'hello' was not added\n");

    set_remove(s, &a);

    if (!set_contains(s, &a))
        printf("'hello' was removed\n");

    set_destroy(s);
    return 0;
}
```

#### Example Output

```
'hello' is in the set
Duplicate 'hello' was not added
'hello' was removed
```

---

## 🔤 **UTF‑8 String Type (`string_t`)**

A dynamic, UTF‑8‑aware string type with safe modification, substring extraction,  
searching, splitting, joining, Unicode normalization, grapheme‑cluster operations,  
and a fixed‑capacity buffer API.

**Features**
- Dynamic UTF‑8 string with automatic growth  
- Append, insert, trim, replace, reverse  
- UTF‑8 codepoint and grapheme‑cluster iteration  
- Unicode normalization (NFC, NFD, NFKC, NFKD)  
- Split/join (owning and non‑owning views)  
- `string_builder_t` convenience API  
- Fixed‑capacity buffer for stack‑allocated strings  
- Hashing, substring extraction, prefix/suffix tests  

---

### 📘 **Example: Splitting, Trimming, and Joining UTF‑8 Strings**

This example demonstrates how to split a UTF‑8 string, trim whitespace from  
each part, and join the results using a custom separator.

```c
#include <stdio.h>
#include "ustring.h"

int main(void) {
    /* Original string with inconsistent spacing */
    string_t *s = string_new_with("alpha, beta , gamma ,delta");

    /* Split on commas */
    size_t n = 0;
    string_t **parts = string_split(s, ",", &n);

    /* Trim whitespace from each part */
    for (size_t i = 0; i < n; i++)
        string_trim(parts[i]);

    /* Join using a new separator */
    string_t *joined = string_join(parts, n, " | ");

    printf("Joined: %s\n", string_c_str(joined));

    /* Cleanup */
    string_free(joined);
    string_split_free(parts, n);
    string_free(s);

    return 0;
}
```

#### Example Output

```
Joined: alpha | beta | gamma | delta
```

---

## 🗂 **Directory Structure**

```
include/                Public headers
    qfloat.h            High‑precision arithmetic API
    dval.h              Differentiable value API
    datetime.h          Date/time utilities
    dictionary.h        Hash map API
    set.h               Set API
    ustring.h           UTF‑8 string utilities

src/
    qfloat/             High‑precision arithmetic
    dval/               Differentiable value engine
    datetime/           Date arithmetic and astronomy
    dictionary/         Hash map implementation
    set/                Set implementation
    string/             UTF‑8 string implementation

tests/
    test_qfloat.c
    test_dval.c
    test_datetime.c
    test_dictionary.c
    test_set.c
    test_string.c

Makefile                Build rules for debug/release/test
README.md               This file
```

---

## 🧰 **Getting Started**

### 1. Clone the repository
```
git clone https://github.com/yourname/ooc.git
cd ooc
```

### 2. Build the library
```
make release
```

### 3. Run the test suite
```
make debug test_all
```

### 4. Include headers in your project
```c
#include "qfloat.h"
#include "dval.h"
#include "datetime.h"
#include "dictionary.h"
#include "set.h"
#include "ustring.h"
```

---

## 🔧 **Building**

The Makefile supports flexible targets for building, testing, and memory‑checking.

### Basic builds
```
make clean
make release
make debug
```

### Running specific tests
```
make debug test_qfloat
make debug test_dval
make release test_set
```

### Memory‑checking (Valgrind)
```
make debug memtest_dval
make debug memtest_set
```

**Notes**
- `debug` builds include assertions, symbols, and diagnostics  
- `release` builds are optimized and stripped  
- Test binaries appear under `tests/build/<config>/`  

---

## 🛣 **Roadmap**

- A better test suite  
- More simplification rules for symbolic expressions  
- Additional qfloat kernels  
- GitHub Actions CI  
- Documentation site  
- Benchmarks and performance suite  
- More datetime utilities  
- More containers  
- Example gallery based on real test code  

---

## 📄 **License**

MIT (or whichever you choose)
