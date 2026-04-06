# **OOC — A Modular C Library for High‑Precision Numerics, Automatic Differentiation, Navigation‑Grade Datetime handling, UTF‑8 Strings, and Robust Generic Containers**

OOC is a modular, portable C library designed for correctness, clarity, and long‑term maintainability.  
It provides:

- high‑precision arithmetic (`qfloat`)
- differentiable values (`dval_t`)
- a civil & astronomical datetime (`datetime_t`)
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
- [Building](#-building--running-tests)
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

# 🧩 **Core Modules**

---

# 🧮 **High‑Precision Arithmetic (`qfloat`)**

### **Sections**
- Overview  
- Example  
- Internal Architecture  

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
W(x)\, e^{W(x)} = x
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

### 🧩 **Internal Architecture — `qfloat`**

`qfloat` implements **double‑double precision arithmetic**, representing each
high‑precision number as the unevaluated sum of two IEEE‑754 doubles:

```
qfloat = hi + lo
```

where:

- **hi** holds the leading ~53 bits  
- **lo** holds the trailing error term  
- together they provide ~106 bits of precision (~32 decimal digits)

This representation is compact, portable, and requires no special hardware.

---

#### **1. Error‑Free Transformations (EFTs)**

At the core of `qfloat` are the classic Dekker/Knuth algorithms:

- **TwoSum** — exact addition of two doubles  
- **TwoProd** — exact multiplication using FMA when available  
- **QuickTwoSum** — fast normalization when |a| ≥ |b|  

These primitives allow `qfloat` to track rounding error explicitly, producing
a correctly rounded high‑precision result.

All higher‑level operations (addition, subtraction, multiplication, division)
are built on top of these EFTs.

---

#### **2. Normalization**

Every operation produces a temporary `(hi, lo)` pair that may not be in
canonical form. `qfloat` normalizes results so that:

- `hi` contains the dominant magnitude  
- `lo` is small enough that `hi + lo` is exact  
- no precision is silently lost  

Normalization ensures stable behaviour across all operations and prevents
error accumulation.

---

#### **3. Elementary Functions**

Functions such as:

- `exp`, `log`, `sin`, `cos`, `tan`  
- `sqrt`, `pow`  
- `atan2`, `hypot`  

are implemented using:

- argument reduction  
- high‑precision polynomial or rational approximations  
- Newton iterations where appropriate  
- careful reconstruction of the final `(hi, lo)` pair  

This allows `qfloat` to deliver consistent ~106‑bit accuracy across the entire
domain of each function.

---

#### **4. Special Functions**

`qfloat` includes high‑precision implementations of:

- gamma  
- erf / erfc  
- Lambert W  
- incomplete gamma  
- and others  

These functions rely on:

- asymptotic expansions  
- continued fractions  
- Newton refinement  
- high‑precision exponentials and logarithms  

The double‑double core ensures stable convergence even for numerically
difficult inputs.

---

#### **5. Parsing and Formatting**

`qfloat` supports:

- exact decimal parsing  
- high‑precision formatting  
- `%q` / `%Q` printf extensions  

Parsing converts decimal strings into a normalized `(hi, lo)` pair without
losing precision. Formatting uses high‑precision digit extraction to produce
correctly rounded output.

---

#### **Why Double‑Double?**

The double‑double format offers an ideal balance between:

- **precision** (~106 bits)  
- **performance** (2×–5× slower than double, far faster than arbitrary precision)  
- **portability** (pure C, no compiler extensions)  
- **predictability** (deterministic behaviour across platforms)  

This makes `qfloat` suitable for:

- numerical analysis  
- scientific computing  
- root‑finding and optimization  
- high‑precision transcendental functions  
- symbolic differentiation backends  
- anywhere IEEE‑754 double is not quite enough  

---

# 🔧 **Differentiable Values (`dval_t`)**

### **Sections**
- Overview  
- Example  
- Internal Architecture (collapsible)  

A lazy, vtable‑driven, reference‑counted DAG of differentiable expressions (automatic differentiation).

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

<details>
<summary><strong>🧩 Internal Architecture — <code>dval_t</code></strong></summary>

<br>

`dval_t` implements **forward‑mode automatic differentiation** using a compact,
reference‑counted DAG of computation nodes. Each `dval_t` represents a value
in an expression tree, and carries both:

- its **numerical value**, and  
- a pointer to a **derivative node** describing how it was computed  

This design allows derivatives to be computed automatically by propagating
derivative information through the DAG.

---

#### **1. Node Types and Vtables**

Each node in the DAG is represented by a small struct containing:

- the node’s operator type (add, mul, sin, exp, etc.)  
- pointers to child nodes  
- a vtable describing how to:
  - evaluate the node  
  - compute its derivative  
  - destroy it  

This vtable‑driven design makes the system:

- extensible (new operators can be added easily)  
- compact (no giant switch statements)  
- predictable (each operator defines its own derivative rule)

---

#### **2. Value Semantics with Shared Nodes**

A `dval_t` is a lightweight handle containing:

- a `qfloat` numerical value  
- a pointer to a shared DAG node  
- a reference count  

Multiple `dval_t` values may refer to the same node, allowing:

- cheap copies  
- cheap expression composition  
- safe destruction when the last reference is released  

This gives `dval_t` the feel of a value type, while internally using a shared
graph for efficiency.

---

#### **3. Forward‑Mode Derivative Propagation**

When differentiating an expression with respect to a variable `x`, the system
constructs a parallel DAG of derivative nodes.

Each operator defines its own derivative rule. For example:

- addition: `(u + v)' = u' + v'`  
- multiplication: `(u * v)' = u' * v + u * v'`  
- sine: `(sin u)' = cos(u) * u'`  
- exp: `(exp u)' = exp(u) * u'`  

The derivative DAG mirrors the structure of the original expression, and is
evaluated using the same vtable mechanism.

This approach supports:

- nested expressions  
- repeated subexpressions  
- arbitrary operator composition  

---

#### **4. Memory Management and Ownership**

Each node is reference‑counted.  
When a `dval_t` is copied, the node’s refcount increases.  
When a `dval_t` is destroyed, the refcount decreases.

When the count reaches zero:

- the node’s children are released  
- the node’s vtable `destroy` function is called  
- the node’s memory is freed  

This ensures:

- no leaks  
- no double‑frees  
- safe sharing of subexpressions  
- efficient reuse of common subtrees  

---

#### **5. Evaluation**

Evaluating a `dval_t` simply returns its stored `qfloat` value.  
Evaluating a derivative involves:

- recursively evaluating the derivative DAG  
- using high‑precision `qfloat` arithmetic  
- applying each operator’s derivative rule  

Because the DAG is acyclic and nodes are shared, evaluation is efficient and
avoids redundant computation.

---

#### **6. Why a DAG?**

Using a DAG instead of a simple tree provides:

- **common subexpression sharing**  
- **lower memory usage**  
- **faster derivative evaluation**  
- **clean semantics for value copying**  
- **a natural path to future extensions**  
  - multivariate AD  
  - reverse‑mode AD  
  - operator overloading layers  

The current implementation focuses on single‑variable forward‑mode AD, but the
architecture is intentionally general.

---

#### **Summary**

`dval_t` is a compact, extensible automatic‑differentiation engine built on:

- reference‑counted DAG nodes  
- vtable‑driven operator semantics  
- high‑precision `qfloat` arithmetic  
- clean value semantics  
- predictable memory ownership  

This makes it suitable for:

- numerical optimization  
- root‑finding  
- sensitivity analysis  
- symbolic‑numeric hybrid workflows  
- any system requiring reliable derivative information  

</details>

---

# 📅 **Civil & Astronomical Datetime (`datetime_t`)**

### **Sections**
- Overview  
- Example  
- Internal Architecture (collapsible)  

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

<details>
<summary><strong>🧩 Internal Architecture — <code>datetime_t</code></strong></summary>

<br>

`datetime_t` provides a unified, high‑precision system for civil timekeeping,
astronomical time scales, and solar/lunar calculations.

Its **time representation and time‑scale conversions are navigation‑grade**  
(precise, deterministic, suitable for astronomical timekeeping), while its  
**solar and lunar algorithms are civil‑grade**, accurate to about a minute for sunset/sunrise and about 12 hours for moon phase
intended for calendar and observational use—not navigation.

The module is built on **three cooperating layers**:

---

#### **1. Civil Time Layer (Gregorian Calendar)**

The civil layer handles:

- year/month/day arithmetic  
- leap‑year rules (Gregorian, proleptic Gregorian)  
- day‑of‑week computation  
- month and year boundaries  
- safe addition/subtraction of days, months, and years  

All civil operations use deterministic integer arithmetic.  
Conversions between civil dates and Julian Day Numbers (JDN) use the
Fliegel–Van Flandern algorithm for correctness, speed, and portability.

`datetime_t` stores time internally with **millisecond granularity**, using double
precision floating point seconds since a defined epoch. This provides:

- stable, platform‑independent behaviour  
- no floating‑point drift  
- predictable arithmetic over long time spans  

---

#### **2. Astronomical Time Scales (Navigation‑Grade)**

`datetime_t` implements precise, deterministic conversions between the
astronomical time scales used in navigation, ephemeris computation, and
scientific work:

- **UTC** — Coordinated Universal Time  
- **TAI** — International Atomic Time  
- **TT** — Terrestrial Time  
- **UT1** — Earth rotation time  
- **JD / MJD** — Julian and Modified Julian Dates  

The system handles:

- leap seconds  
- TAI ↔ UTC conversion  
- UT1 drift  
- ΔT (TT − UT1)  

These conversions are **navigation‑grade**:  
explicit, deterministic, and independent of OS time libraries or timezone
databases.

---

#### **3. Solar and Lunar Algorithms (Civil‑Grade)**

`datetime_t` includes algorithms for:

- sunrise and sunset  
- solar noon  
- equation of time  
- approximate solar position  
- lunar phase and illumination  

These are based on Meeus‑style trigonometric series and low‑order polynomial
approximations.

They are **civil‑grade**:

- typically accurate to **about one minute**  
- suitable for calendars, planning, and general observational astronomy  
- **not intended for navigation**, ephemeris generation, or applications
  requiring sub‑minute accuracy  

---

#### **4. Value‑Semantic API**

`datetime_t` is a pure value type:

- no hidden allocations  
- no global state  
- no timezone database dependencies  
- deterministic behaviour across platforms  

This makes it suitable for:

- embedded systems  
- deterministic simulations  
- scientific computing  
- long‑running services  

---

#### **Summary**

- **Time representation & time‑scale conversions** → *navigation‑grade*  
- **Solar & lunar algorithms** → *civil‑grade (≈1‑minute accuracy)*  
- **Internal representation** → *millisecond granularity*
- **API** → value‑semantic, deterministic, portable  

This architecture provides a robust foundation for any system requiring
reliable, high‑precision timekeeping.

</details>

---

# 📚 **Generic Dictionary (`dictionary_t`)**

### **Sections**
- Overview  
- Examples  
- Internal Architecture  

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

### 🧩 **Internal Architecture — `dictionary_t`**

`dictionary_t` is built on a compact, arena‑based storage model designed for
predictable performance, stable element lifetimes, and flexible ownership
semantics.

Unlike a naive hash map that stores key/value pairs directly in hash buckets,
`dictionary_t` uses **three cooperating components**:

---

#### **1. Key Arena**

A contiguous arena that stores *copies* of all keys.  
Keys never move once inserted, which makes:

- hashing stable  
- sorted views efficient  
- key handles safe to keep  

Keys are cloned using the user‑supplied `key_clone` callback (or memcpy for
shallow copies).

---

#### **2. Value Arena**

A second arena that stores *copies* of all values.  
Values have independent clone/destroy semantics, allowing:

- deep‑copied values  
- shallow‑copied keys  
- or vice‑versa  

This separation is what enables the dictionary to support fully value‑semantic
behaviour.

---

#### **3. Entry Table (Hash Table)**

A hash table mapping each key to a pair of arena indices:

```
(key_index, value_index)
```

Each entry stores:

- the precomputed hash  
- the index of the key in the key arena  
- the index of the value in the value arena  

This design ensures:

- fast lookups  
- stable storage  
- no pointer invalidation  
- efficient cloning and destruction  

---

#### **Why two arenas?**

Storing keys and values in separate arenas provides:

- independent clone/destroy behaviour  
- stable storage for both keys and values  
- efficient sorted views  
- predictable memory layout  
- clean value‑semantics without hidden allocations  

This makes `dictionary_t` suitable for use in systems where memory ownership,
determinism, and predictable behaviour matter — such as embedded systems,
numerical computing, or long‑running services.

---

# 🧩 **Generic Hash Set (`set_t`)**

### **Sections**
- Overview  
- Example  
- Internal Architecture  

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

### 🧩 **Internal Architecture — `set_t`**

`set_t` is implemented using a compact, arena‑based storage model designed for
predictable performance, stable element lifetimes, and efficient membership
operations. Its design mirrors the dictionary’s arena strategy, but with a
single arena because each element is a standalone value.

The set consists of **two cooperating components**:

---

#### **1. Element Arena**

A contiguous arena that stores *copies* of all elements.  
Elements never move once inserted, which provides:

- stable storage (no pointer invalidation)  
- predictable iteration order (dense, gap‑free)  
- efficient cloning and destruction  
- clean value‑semantics  

Elements are cloned using the user‑supplied `clone` callback (or memcpy for
shallow copies). The arena owns all stored elements and frees them using the
user‑supplied `destroy` callback.

---

#### **2. Hash Table**

A hash table that maps each element’s hash to an index in the arena.

Each entry stores:

- the precomputed hash  
- the index of the element in the arena  

This allows:

- fast membership checks  
- O(1) average insertion  
- O(1) average removal  
- duplicate detection without re‑allocating or re‑cloning  

---

#### **Stable Storage and Deterministic Behaviour**

Because elements live in a dense arena:

- iteration is cache‑friendly  
- removing an element swaps the last element into its slot, keeping the arena compact  
- sorted views can be built lazily without disturbing the underlying storage  

This makes `set_t` suitable for systems where memory ownership, determinism,
and predictable behaviour matter — such as embedded systems, numerical
computing, or long‑running services.

---

#### **Why an Arena?**

Using an arena instead of allocating each element individually provides:

- fewer allocations  
- stable element lifetimes  
- simpler clone/destroy semantics  
- predictable memory layout  
- efficient bulk operations  

The result is a robust, value‑semantic set container that behaves consistently
and avoids the pitfalls of pointer‑based hash sets.

---

# 🔤 **UTF‑8 String Type (`string_t`)**

A dynamic, UTF‑8‑aware string type with safe modification, substring extraction,  
searching, splitting, joining, Unicode normalization, grapheme‑cluster operations,  
and a fixed‑capacity buffer API.

**Features**
- Dynamic UTF‑8 string with automatic growth  
- Append, insert, trim, replace, remove  
- Unicode normalization (NFC, NFD)  
- Grapheme‑cluster iteration  
- Substring extraction by codepoint or grapheme index  
- UTF‑8 validation and repair  
- Builder API for efficient incremental construction  
- Split/join helpers  
- Comparison, hashing, formatting helpers  
- Fixed‑capacity stack buffer API (`string_buf_t`)  

---

### 📘 **Example: Basic UTF‑8 Manipulation**

```c
#include <stdio.h>
#include "string.h"

int main(void) {
    string_t s = string_make("Héllo");

    /* Append UTF‑8 text */
    string_append(&s, " 🌍");

    /* Insert at grapheme index */
    string_insert(&s, 1, "🙂");

    /* Normalize to NFC */
    string_normalize_nfc(&s);

    printf("%s\n", string_cstr(&s));

    string_free(&s);
    return 0;
}
```

#### Example Output

```
H🙂éllo 🌍
```

---

### 📘 **Example: Grapheme Iteration**

```c
#include "string.h"
#include <stdio.h>

int main(void) {
    string_t s = string_make("👨‍👩‍👧‍👦 family");

    size_t count = string_grapheme_count(&s);

    printf("Graphemes: %zu\n", count);

    for (size_t i = 0; i < count; i++) {
        string_t g = string_grapheme_at(&s, i);
        printf("[%zu] %s\n", i, string_cstr(&g));
        string_free(&g);
    }

    string_free(&s);
}
```

#### Example Output

```
Graphemes: 7
[0] 👨‍👩‍👧‍👦
[1]  
[2] f
[3] a
[4] m
[5] i
[6] l
[7] y
```

---

### 📘 **Example: Using the Builder API**

```c
#include "string.h"
#include <stdio.h>

int main(void) {
    string_builder_t b;
    string_builder_init(&b);

    string_builder_append(&b, "Hello");
    string_builder_append(&b, ", ");
    string_builder_append(&b, "世界");

    string_t out = string_builder_finish(&b);

    printf("%s\n", string_cstr(&out));

    string_free(&out);
    return 0;
}
```

#### Example Output

```
Hello, 世界
```

---

### 🧩 **Internal Architecture — `string_t`**

`string_t` is built around a **UTF‑8 aware dynamic buffer** with strict invariants:

- the buffer always contains valid UTF‑8  
- operations preserve grapheme boundaries  
- normalization uses canonical Unicode algorithms  
- growth is amortized and predictable  
- no hidden allocations beyond the buffer itself  

The design emphasizes:

- **safety** (never split a codepoint or grapheme)  
- **performance** (fast ASCII path, cached length)  
- **correctness** (Unicode‑compliant normalization and iteration)  

---

#### **1. Internal Representation**

A `string_t` stores:

- a `char *data` buffer  
- a `size_t length_bytes`  
- a `size_t capacity_bytes`  
- a cached `size_t length_graphemes` (lazy)  

The buffer is always NUL‑terminated for convenience, but the type is not limited to C‑string semantics.

---

#### **2. UTF‑8 Validation and Repair**

All constructors validate UTF‑8 input.  
Invalid sequences are replaced with U+FFFD (replacement character).

This ensures:

- safe iteration  
- predictable normalization  
- no undefined behaviour  

---

#### **3. Grapheme‑Cluster Engine**

Grapheme iteration uses:

- Unicode Standard Annex #29  
- extended grapheme cluster rules  
- correct handling of:
  - emoji sequences  
  - ZWJ sequences  
  - combining marks  
  - Hangul syllables  

This allows operations like:

- `string_grapheme_at`  
- `string_insert`  
- `string_remove_range`  

to behave intuitively.

---

#### **4. Normalization**

Normalization uses:

- canonical decomposition (NFD)  
- canonical composition (NFC)  
- Unicode decomposition tables  
- combining‑class sorting  

This ensures that visually identical strings compare equal.

---

#### **5. Builder API**

`string_builder_t` provides:

- amortized O(1) append  
- no intermediate UTF‑8 validation  
- final validation on `finish()`  

This is ideal for:

- log messages  
- serialization  
- incremental construction  

---

#### **6. Fixed‑Capacity Buffer (`string_buf_t`)**

A stack‑allocated buffer with:

- fixed capacity  
- safe UTF‑8 append  
- automatic truncation at grapheme boundaries  

Useful for:

- formatting  
- temporary strings  
- embedded systems  

---

# 📁 **Directory Structure**

A clean, modular layout designed for clarity and maintainability:

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

Each module is self‑contained and can be used independently.

---

# 🚀 **Getting Started**

### **1. Clone the repository**

```
git clone https://github.com/yourname/ooc.git
cd ooc
```

### **2. Build the library**

```
make release
```

This produces:

- `build/release/libooc.a`  
- module‑specific object files  
- optional test binaries  

### **3. Include the headers you need**

```c
#include "qfloat.h"
#include "dval.h"
#include "datetime.h"
#include "dictionary.h"
#include "set.h"
#include "string.h"
```

### **4. Link with the library**

```
gcc your_program.c -Iinclude -Lbuild/release -looc -lm
```

---

# 🧪 **Building & Running Tests**

### **Build all tests**

```
make test
```

### **Run the full suite**

```
./build/test/all_tests
```

### **Run a specific module’s tests**

```
./build/test/test_qfloat
./build/test/test_dval
./build/test/test_datetime
./build/test/test_dictionary
./build/test/test_set
./build/test/test_string
```

All tests are written in plain C with no external dependencies.

---

# 🗺️ **Roadmap**

Planned enhancements include:

### **qfloat**
- Additional special functions  
- Improved argument‑reduction for trig functions  
- Optional quad‑precision backend  

### **dval_t**
- Reverse‑mode AD  
- Multivariate support  
- JIT‑compiled derivative evaluation  

### **datetime_t**
- More holiday algorithms  
- Optional timezone database integration  
- Ephemeris‑grade solar/lunar algorithms (configurable)  

### **dictionary_t / set_t**
- Ordered dictionary variant  
- Persistent/immutable variants  
- SIMD‑accelerated hashing  

### **string_t**
- Regex engine  
- Rope‑based large‑string backend  
- Locale‑aware collation  

---

# 📄 **License**

MIT License — see `LICENSE` for details.

---

# 🎉 **Thank You**

OOC is designed to be:

- clean  
- predictable  
- portable  
- enjoyable to use  

