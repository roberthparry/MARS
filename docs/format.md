# Typed formatting

The `format` module provides typed brace formatting and named interpolation.
It creates strings, appends to strings or builders, and writes to `FILE` streams.

`fmt_print()` and `fmt_fprint()` reuse up to eight small compiled templates per
thread. Cache entries contain owned template text and binding layouts, never
bound values. Templates and names are compared by content, so changing a buffer
or reusing its address remains safe. Only templates of at most 512 bytes, with
at most 16 arguments and 512 binding-name bytes, enter this bounded cache;
larger calls use a temporary plan. Worker-thread exit releases its entries.
Common scalar fields and up to 1024 output bytes use stack storage. Larger
results grow temporary storage, and Unicode precision, calendar masks and
module-owned renderers retain their normal formatting rules. Every call stages
its complete result before writing, including calls that reuse a template.

Use `FMT(value)` to bind a variable by its own name, or `FMT(name, expression)`
to give a computed value an explicit label. Named fields make argument order
irrelevant. Each value is evaluated once; labels are captured at compilation
without runtime string parsing. Use `fmt_named()` when the name itself comes
from a runtime C string.

Include `format-t.h` explicitly for formatting, together with the owning header
of each native type being formatted. No additional integration header is required.
`datetime-t.h` registers its adapter without importing the formatting API;
including `format-t.h` enables it in either header order.

## Migrating stdout formatting

The module-specific stdout functions `qf_printf()`, `qc_printf()`, `num_printf()`,
`mat_printf()`, `expr_printf()`, `equ_printf()`, `ts_printf()` and `string_printf()`
are deprecated. They remain functional for compatibility, and supported compilers
warn at call sites with a recommendation to use `fmt_print()` and `FMT()`.
Ordinary library, application and example output uses the typed route; dedicated
compatibility tests still exercise the retained functions. The `*_sprintf*()`
and `*_vsprintf*()` interfaces remain available, including for legacy runtime
precision controls. Include `format-t.h` explicitly when migrating stdout calls.
`attributes-t.h` supplies the shared `MARS_DEPRECATED(message)` annotation;
compilers without deprecation attributes retain the documented interfaces.

## Named bindings

```c
#include "format-t.h"
#include "qfloat-t.h"

int main(void)
{
    const char *name = "Ada";
    qfloat_t x = qf_from_double(1.25);
    int bytes = fmt_print("{name}: x={x:.3f}\n", FMT(name), FMT(x));
    return bytes < 0;
}
```

Output:

```text
Ada: x=1.250
```

`FMT(x)` captures the spelling `x`, selects a typed constructor with C17
`_Generic`, and evaluates the value once. Use `FMT(result, a + b)`
to give an expression an explicit name. Arguments are stored in a typed array,
including calls with no bindings. C variables must be supplied explicitly:
names inside a template do not access C scope or evaluate C expressions.
Inserted text is data, never another template. Percent signs need no escaping.

`fmt_arg_t` is an opaque value with fixed-size storage. Copy it, return it or
keep it in an argument array; create bindings with `FMT()` or `fmt_arg_*()` and
rename them with `fmt_named()`. Its tag, name and payload are private. Capturing
or copying a binding allocates no memory and requires no release. A
zero-initialised binding is invalid and is rejected when compiling a template.

## Fields and options

| Syntax | Meaning |
|---|---|
| `{}` | Next automatic positional argument |
| `{2}` | Argument at zero-based index two |
| `{name}` | Binding with that exact UTF-8 name |
| `{{` and `}}` | Literal opening and closing braces |
| `{name:>12}` | Right-aligned field of at least 12 visible characters |
| `{name:*^12}` | Centred field padded with ASCII `*` |
| `{value:+08d}` | Signed decimal integer with sign-aware zero padding |
| `{value:#x}` | Hexadecimal integer with a base prefix |
| `{value:.3f}` | Three digits after the decimal point |
| `{text:.4s}` | At most four user-visible text characters |

Options follow `[fill][alignment][sign][#][0][width][.precision][presentation]`.
Alignment is `<`, `>`, `^`, or `=` for padding after a numeric sign/base prefix.
Default alignment is right for numerical values and left for text and objects.
Fill is one printable ASCII character other than a brace. Text width and text
precision count the string module's user-visible characters, including combined
accents and emoji sequences. Returned output counts measure encoded bytes.

Automatic fields and explicit numeric indices cannot mix. Named fields may
accompany either positional mode. Duplicate names are allowed positionally, but
referencing a duplicate name is an ambiguity error. Names match exact UTF-8
spelling without Unicode normalisation. A binding can be referenced repeatedly.

Width is a literal integer up to `INT_MAX`. Precision is a literal integer from
zero through 10000. Nested replacement fields and dynamic width/precision fields
are not supported. Incompatible presentations and unsupported options produce
checked errors rather than guessing a conversion.
Presentation names contain ASCII letters and underscores.

## Supported values

| C or MARS value | Default and presentations |
|---|---|
| Signed/unsigned integers, including `size_t` | Decimal; `d`, `b`, `o`, `x`, `X`; `c` for a Unicode scalar |
| `bool` | `true`/`false`; `s`, or integer `d`, `b`, `o`, `x`, `X` |
| `float`, `double`, `long double` | General notation retaining source precision; `f`, `F`, `e`, `E`, `g`, `G` |
| C text, `string_t *`, `string_view_t`, `rune_t` | UTF-8; optional `s`; precision truncates by visible characters |
| Ordinary object pointers | Hexadecimal address; optional `p`; never dereferenced |
| `qfloat_t` | Native `q`; `q`, `Q`, `f`, `F`, `e`, `E`, `g`, `G` |
| `qcomplex_t` | Native complex notation; component-wise `q`, `Q`, `f`, `F`, `e`, `E`, `g`, `G` |
| `number_t` | Native `n`; `n`, `N`, preserving backend precision and exact representations |
| `matrix_t *` | Native `expression`; `expression`, `function`, `latex`, `pretty`, `scientific`, `layout`, `pretty_layout`, `scientific_layout` |
| `expr_t *` | Native `expression`; `expression`, `function`, `latex`, `unbound` |
| `equation_t *` | Native `expression`; `expression`, `function`, `latex`, `unbound` |
| `datetime_t *` | Configured Lingua date; locale codes, `iso`, `date`, `time`, native calendar names, or custom date/time codes |
| `TeX_t *` | Native TeX source; optional `latex` |
| `timeseries_t *` | Native `inline`; `inline`, `pretty`, `csv` |

Ordinary floats use their respective decimal precision by default. An explicit
floating presentation defaults to six digits; general precision zero means one
significant digit. Alternate integer forms use `0b`, `0o`, `0x` or `0X`. Decimal
integers do not accept alternate form or floating-style precision.

Mathematical types enable their own `FMT()` selection when their public header
is included, in either order relative to `format-t.h`. Each module owns its
adapter, supported presentations and validation; the core has no mathematical
type list or dependency on those modules. No runtime registration is required.

Explicit qfloat/qcomplex `f`/`e`/`g` modes round the **exact sum of the stored
binary components**, to nearest with ties to even. Native bounded integer
conversion performs this without narrowing to `double` or invoking a configurable
numerical backend. Extra decimal places expose the stored approximation rather
than increasing mathematical accuracy. Established `q`/`Q`, zero and compact
complex unit conventions remain available.

Generic `number_t` values use their existing `n`/`N` presentations. Precision
applies to inexact backends; exact values retain their native form, including
Unicode fractions. Matrix and expression fields delegate to native rendering
without evaluating, substituting or rewriting mathematics. See the
[number](number.md), [qfloat](qfloat.md), [qcomplex](qcomplex.md),
[matrix](matrix.md) and [expression](expression.md) guides.

## Compiled templates

```c
#include "format-t.h"

int main(void)
{
    int count = 7;
    fmt_arg_t arguments[] = {FMT(count)};
    fmt_t *format = fmt_compile("count={count:04d}", arguments, 1u, NULL);
    count = 12;
    arguments[0] = FMT(count);
    string_t *text = fmt_render(format, arguments, 1u, NULL);
    if (text)
        fmt_print("{text}\n", FMT(text));
    int status = !format || !text;
    string_free(text);
    fmt_free(format);
    return status;
}
```

Output:

```text
count=0012
```

Compilation owns the template and binding names/categories and borrows immutable
adapter identities; it retains no bound values or custom callbacks. Hashed named
lookup occurs once. Rendering uses direct argument indices and checks referenced
names/types against the compiled layout.
Supply the same argument count and ordering, updating descriptor values as needed.

`fmt_t` is immutable and shareable between threads with independent outputs.
Borrowed objects and callbacks must independently support any concurrent use.
Stream convenience calls use the bounded per-thread cache described above.
Release explicitly compiled templates with `fmt_free()`.

## Explicit names and expressions

Label a calculated value with `FMT(name, expression)`. Fields can repeat a
binding, and descriptors can be supplied in either order. Automatic `{}` and
indexed `{0}` fields remain available when positional formatting is intentional.

```c
#include "format-t.h"

int main(void)
{
    int quantity = 3;
    const char *item = "bolts";
    int first = fmt_print("{quantity} {item}\n", FMT(item), FMT(quantity));
    if (first < 0)
        return 1;
    int bytes = fmt_print("{item}: {quantity}/{quantity}; total={total}; {{ready}}\n",
        FMT(total, quantity * 7),
        FMT(item),
        FMT(quantity));
    return bytes < 0;
}
```

Output:

```text
3 bolts
bolts: 3/3; total=21; {ready}
```

## Creating strings and padding fields

`fmt_string()` returns an owned string. Text precision and width count grapheme
clusters: the accent stays attached to `e`, and `.1s` removes the following
emoji. The width adds three spaces; the hexadecimal field keeps its base prefix
before zero padding.

```c
#include "format-t.h"

int main(void)
{
    const char *word = "é🙂";
    unsigned code = 42u;
    string_t *text = fmt_string("|{word:>4.1s}| {code:#06x}", FMT(word), FMT(code));
    if (!text)
        return 1;
    int bytes = fmt_print("{text}\n", FMT(text));
    string_free(text);
    return bytes < 0;
}
```

Output:

```text
|   é| 0x002a
```

For an example that keeps an emoji in the output, see [Printing emoji](#printing-emoji).

## Appending and writing to streams

Build a message incrementally with `fmt_append()`, then choose a `FILE` stream
with `fmt_fprint()`. A builder uses the same string argument handling as `string_t`.

```c
#include "format-t.h"

int main(void)
{
    string_builder_t *out = string_builder_new();
    if (!out)
        return 1;
    int quantity = 3;
    int status = fmt_append(out, "items={quantity}", FMT(quantity)) < 0 ||
                 fmt_append(out, ", total={total}", FMT(total, quantity * 7)) < 0;
    if (!status)
        status = fmt_fprint(stdout, "{out}\n", FMT(out)) < 0;
    string_builder_free(out);
    return status;
}
```

Output:

```text
items=3, total=21
```

## Complex and exact values

Include the headers of the types being formatted. Fixed decimal precision
applies to both complex components; an exact fraction retains its native form.
Keep owning number handles alive for the call and release them afterwards.

```c
#include "format-t.h"
#include "qcomplex-t.h"
#include "number-t.h"

int main(void)
{
    qcomplex_t z = qc_make(qf_from_double(1.25), qf_from_double(-2.5));
    number_t ratio = num_create_from_string("5/6");
    int bytes = fmt_print("z={z:.2f}; ratio={ratio}\n", FMT(z), FMT(ratio));
    num_destroy(&ratio);
    return bytes < 0;
}
```

Output:

```text
z=1.25 - 2.50i; ratio=⅚
```

## API and ownership

| API | Result and ownership |
|---|---|
| `FMT(value)` | Capture a typed value and its source name once |
| `FMT(name, value)` | Capture a typed value once with an explicit label |
| `fmt_named(name, argument)` | Assign an explicit borrowed name |
| `fmt_string(pattern, ...)` | Owned string; release with `string_free()` |
| `fmt_append(out, pattern, ...)` | Append to a string or `string_builder_t`; bytes or `-1` |
| `fmt_print(pattern, ...)` | Write to stdout; bytes or `-1` |
| `fmt_fprint(stream, pattern, ...)` | Write to a borrowed stream; bytes or `-1` |
| `fmt_compile(pattern, arguments, count, error)` | Owned template; release with `fmt_free()` |
| `fmt_render(format, arguments, count, error)` | Owned rendered string |
| `fmt_free(format)` | Release a template; accepts NULL |
| `fmt_last_error()` | Borrow the current thread's latest diagnostic |

The output macros assemble an argument array and count without evaluating values
twice. Explicit-array equivalents are `fmt_string_args()`, `fmt_append_args()`
and `fmt_fprint_args()`. Each accepts an optional caller-owned `fmt_error_t`.

Explicit unnamed constructors are `fmt_arg_signed()`, `fmt_arg_unsigned()`,
`fmt_arg_bool()`, `fmt_arg_float()`, `fmt_arg_double()`, `fmt_arg_long_double()`,
`fmt_arg_cstring()`, `fmt_arg_string()`, `fmt_arg_view()`, `fmt_arg_rune()`,
`fmt_arg_pointer()`. The owning mathematical modules provide `fmt_arg_qfloat()`,
`fmt_arg_qcomplex()`, `fmt_arg_number()`, `fmt_arg_matrix()` and
`fmt_arg_expression()`. Datetimes, equations and notation provide
`fmt_arg_datetime()`, `fmt_arg_equation()`, `fmt_arg_TeX()` and `fmt_arg_timeseries()`. Including a type's header lets `FMT()` select its
constructor automatically. Unsupported value types require an explicit custom adapter.
Values are evaluated once; their evaluation order follows ordinary C rules.

Primitive scalars and qfloat/qcomplex components are copied. Strings, views,
runes, objects, binding names and payloads behind shallow `number_t` handles stay
**borrowed**. Keep them alive and unchanged throughout the call. Formatting
never releases those resources. An owning numerical constructor used as an
expression still needs normal lifetime management; a named variable makes that
ownership explicit. NULL strings render as `(null)`; NULL matrix/expression
pointers, datetime pointers, equation pointers and notation pointers produce an argument error.

Use `fmt_arg_custom(name, value, render)` for application objects. Its
`fmt_custom_fn` receives parsed `fmt_options_t` and returns an owned, unpadded
core string. It handles its presentation, precision and numeric flags; the engine
handles width, alignment and padding. Failure returns NULL and can supply a
static diagnostic message. Custom objects and callbacks remain borrowed.

### Custom object example

This callback borrows a point and returns its owned core text. It accepts the
default presentation and ordinary text alignment, rejecting precision and
numeric flags. The engine applies the requested field width and releases the
returned string; the caller retains the point.

```c
#include "format-t.h"

typedef struct {
    int x;
    int y;
} point_t;

static string_t *point_format(const void *value, const fmt_options_t *options, fmt_error_t *error)
{
    if (*options->presentation || options->precision >= 0 || options->sign != '-' ||
        options->alternate || options->zero || options->alignment == '=') {
        if (error)
            *error = (fmt_error_t){FMT_ERROR_TYPE, SIZE_MAX, SIZE_MAX, "unsupported point options"};
        return NULL;
    }
    const point_t *point = value;
    return fmt_string("({x}, {y})", FMT(x, point->x), FMT(y, point->y));
}

int main(void)
{
    const point_t point = {2, 3};
    fmt_arg_t position = fmt_arg_custom("position", &point, point_format);
    int bytes = fmt_print("point={position:>10}\n", position);
    return bytes < 0;
}
```

Output:

```text
point=    (2, 3)
```

For reusable type adapters, define a distinct immutable `fmt_adapter_t` per
type and capture with `fmt_arg_adapter(adapter, &value, sizeof(value))`. The
core copies up to `FMT_ADAPTER_CAPACITY` bytes into each descriptor without
allocation. Decode them with `memcpy` into a typed local value; do not cast or
retain the payload address. Pointer and shallow-handle payloads still borrow
their referenced resources. Oversized payloads, missing source bytes or a missing
adapter/renderer return an invalid descriptor without changing diagnostics.

An adapter supplies a renderer, an optional validation callback and a `numeric`
flag controlling sign insertion and default padding. Validation receives parsed
options at compilation; a NULL validator accepts all presentations after the
core checks numeric options. Rendering returns owned unpadded text or NULL with
an optional static diagnostic, using the same callback contract as custom
bindings. The immutable adapter must outlive its descriptors and compiled
templates. Compiled templates compare adapter identities before rendering and
reject a replacement value of another adapted type. Concurrent callbacks must
be reentrant; no mutable global registry is involved.

## Errors and atomic output

`fmt_error_t` contains a `fmt_code_t`, template byte offset, argument index and
static message. `SIZE_MAX` denotes an unavailable offset/index. The latest record
is available through `fmt_last_error()` until another compile, render or output
call on that thread. Constructors and `fmt_free()` do not replace it; copy the
record if it must survive later formatting.

Errors distinguish arguments, memory, syntax, missing/ambiguous bindings, types,
ranges and I/O. Output is limited to `INT_MAX` encoded bytes. Appends stage the
complete result, preserving the destination on formatting failure. Streams are
written only after rendering succeeds, but I/O failures can leave partial output.

A failed append preserves its destination. Copy the diagnostic before making
another formatting call, because that call replaces the thread-local record.

```c
#include "format-t.h"

int main(void)
{
    string_t *out = string_new_with("unchanged");
    if (!out)
        return 1;
    int appended = fmt_append(out, " {missing}", FMT(7));
    fmt_error_t failure = *fmt_last_error();
    int bytes = fmt_print("{message}; destination={out}\n", FMT(message, failure.message), FMT(out));
    int status = appended != -1 || failure.code != FMT_ERROR_MISSING || bytes < 0;
    string_free(out);
    return status;
}
```

Output:

```text
unknown named binding; destination=unchanged
```

## Printing emoji

Print an emoji directly as UTF-8 text, or use the `c` presentation to encode its
Unicode scalar value. Both forms below print the same emoji without truncation.

```c
#include "format-t.h"

int main(void)
{
    const char *emoji = "🙂";
    unsigned code_point = 0x1F642u;
    int bytes = fmt_print("Text: {emoji}\nCharacter: {code_point:c}\n", FMT(emoji), FMT(code_point));
    return bytes < 0;
}
```

Output:

```text
Text: 🙂
Character: 🙂
```

The [string module](string.md) supplies UTF-8 storage and builders. Existing
printf-style APIs continue to be available alongside typed formatting.

## Native strings

`string_t` has a built-in typed binding, including const pointers; no adapter or
conversion to a C string is needed. The descriptor borrows the string. Keep it
alive until printing finishes, then release it normally.

```c
#include "format-t.h"
#include "string-t.h"

int main(void)
{
    string_t *greeting = string_new_with("Hello, Ada 🙂");
    if (!greeting)
        return 1;
    int bytes = fmt_print("{greeting}\n", FMT(greeting));
    string_free(greeting);
    return bytes < 0;
}
```

Output:

```text
Hello, Ada 🙂
```

## Datetimes

Include both `format-t.h` and `datetime-t.h` to use `FMT()` with `datetime_t *`
and const pointers. The datetime header alone keeps the formatting API out of
datetime-only programmes. Both include orders work.
The default `{dt}` and `{dt:lingua}` use the installed Jurisdiction Lingua date
presentation. An exact catalogue locale such as `{dt:fr_FR}` overrides that
choice for one field, including its wording and date order. `iso` produces an
ISO civil datetime; `date` and `time` select its individual parts.
Excel custom codes support names, month/minute
context, fractional seconds and twelve-hour time. Native date codes use `MM` for
months and `mm` for minutes, independently of context. Prefix a code with
`excel:` to request Excel's contextual month/minute rules. Native civil components keep
their fractional seconds without first being rounded to a serial-day double.

```c
#include "format-t.h"
#include "datetime-t.h"

int main(void)
{
    datetime_t *when = datetime_init_ymdt(datetime_alloc(), 2026, DT_June, 10, 13, 5, 6.125);
    if (!when)
        return 1;
    int bytes = fmt_print("{when:dddd, do^ MMMM yyyy}\n{when:yyyy-MM-dd hh:mm:ss.000}\n{when:h:mm:ss "
                          "AM/PM}\n{when:h:mm:ssam/pm}\n",
        FMT(when));
    datetime_dealloc(when);
    return bytes < 0;
}
```

Output:

```text
Wednesday, 10ᵗʰ June 2026
2026-06-10 13:05:06.125
1:05:06 PM
1:05:06pm
```

Native date masks support `[lower]`, `[upper]` and `[title]` for subsequent
English weekday/month names and ordinary ordinal suffixes. The default uses
title-case names and lower-case suffixes. Quoted literals and superscript
suffixes retain their spelling. These directives belong to native masks;
`excel:` retains Excel's own syntax.

```c
#include "format-t.h"
#include "datetime-t.h"

int main(void)
{
    datetime_t *dt = datetime_init_ymd(datetime_alloc(), 2026, DT_June, 10);
    if (!dt)
        return 1;
    int bytes = fmt_print("{dt:[upper]dddd do MMMM yyyy}\n{dt:[lower]ddd MMM yyyy}\n"
                          "{dt:[upper]ddd [title]do MMMM yyyy}\n", FMT(dt));
    datetime_dealloc(dt);
    return bytes < 0;
}
```

Output:

```text
WEDNESDAY 10TH JUNE 2026
wed jun 2026
WED 10th June 2026
```

When a mask omits seconds, hidden fractional seconds do not round the displayed
date, hour or minute. When seconds are displayed, rounding follows their selected
precision, including any necessary carry into the next minute or date.
The `iso`, `datetime` and `time` keyword presentations retain whole seconds
without rounding; custom masks can request fractional seconds explicitly.

Locale codes select a Lingua date, rather than translating the month in an
otherwise English mask. This example includes Greek, Cyrillic and Japanese
writing as well as French, Spanish and Welsh. The jurisdiction database must
contain the requested locale data; including `jurisdiction-t.h` is unnecessary
for these ordinary datetime bindings.

```c
#include "format-t.h"
#include "datetime-t.h"

int main(void)
{
    datetime_t *dt = datetime_init_ymd(datetime_alloc(), 2026, DT_June, 10);
    if (!dt)
        return 1;
    int bytes = fmt_print("France: {dt:fr_FR}\nSpain: {dt:es_ES}\nWales: {dt:cy_GB}\n"
                          "Greece: {dt:el_GR}\nRussia: {dt:ru_RU}\nJapan: {dt:ja_JP}\n", FMT(dt));
    datetime_dealloc(dt);
    return bytes < 0;
}
```

Output:

```text
France: 10 juin 2026
Spain: 10 de junio de 2026
Wales: 10 Mehefin 2026
Greece: 10 Ιουνίου 2026
Russia: 10 июня 2026 г.
Japan: 2026年6月10日
```

The catalogue preserves grammatical month forms, such as Greek `Ιουνίου` and
Russian `июня`. French uses `1ᵉʳ` for the first day. Arabic Lingua locales use
the civil Islamic calendar and Arabic-Indic digits; Hebrew Lingua locales use
the fixed Hebrew calendar. These follow Jurisdiction's existing **Date Lingua**
conventions. Time of day is omitted from a Lingua date; request `time`, `iso`
or an explicit date/time mask when needed.

The first use loads an immutable presentation snapshot; repeated rendering
performs no SQL or locale parsing. The per-thread cache holds at most eight
presentations, independently of the number of dates or print calls. It retains
the configuration observed when loaded; applications that change configuration
can call `jurisdict_date_locale_cache_clear()` on each affected thread.
If no usable installed Lingua catalogue exists, the default falls back to ISO
civil datetime. An unavailable explicit locale reports a checked formatting
error. No database is created or updated by printing, and no ICU is required.

## Expressions, equations and TeX

Include each object's owning header. Expression and equation bindings accept
`expression`, `function`, `unbound` and `latex`; they use the native renderers
without evaluating or substituting bindings. The `latex` presentation includes
bindings according to the owning module's style rules. `TeX_t` prints its TeX
source directly, with no added mathematical delimiters.

```c
#include "format-t.h"
#include "expression-t.h"
#include "equation-t.h"
#include "TeX-t.h"

int main(void)
{
    expr_t *expression = expr_from_string("x^2 + 1", NULL);
    equation_t *equation = equ_from_string("x^2 = 4");
    TeX_builder_t *builder = TeX_builder_new();
    TeX_symbol(builder, "alpha");
    TeX_integer(builder, 2);
    TeX_frac(builder);
    TeX_t *notation = TeX_builder_finish(builder);
    int bytes = -1;
    if (expression && equation && notation)
        bytes = fmt_print("Expression: {expression:unbound}\nEquation: {equation:unbound}\nExpression TeX: "
                          "{expression:latex}\nEquation TeX: {equation:latex}\nTeX: {notation}\n",
            FMT(expression),
            FMT(equation),
            FMT(notation));
    TeX_free(notation);
    TeX_builder_free(builder);
    equ_free(equation);
    expr_free(expression);
    return bytes < 0;
}
```

Output:

```text
Expression: x² + 1
Equation: x² = 4
Expression TeX: \left\{ x^{2} + 1 \;\middle|\; x = ? \right\}
Equation TeX: \left\{ x^{2} \;\middle|\; x = ? \right\} = 4
TeX: \frac{\alpha}{2}
```

## Excel custom codes

Use Excel's numeric custom codes directly after a field's colon. Native date
codes distinguish `M` (month) from `m` (minute) by case. `excel:` explicitly
selects Excel compatibility, including case-insensitive date codes and its
contextual interpretation of `m`/`mm`, for example `excel:yyyy-mm-dd hh:mm:ss`.
It also disambiguates codes that resemble an ordinary native presentation.
Prefix an aligned custom field with a width and another colon,
for example `{amount:>12:#,##0.00}`. Compilation resolves the custom code once;
rendering uses its immutable tokens and directly indexed sections.

```c
#include "format-t.h"
#include "number-t.h"

int main(void)
{
    double positive = 1234.5;
    double negative = -1234.5;
    double zero = 0.0;
    double negative_zero = -0.0;
    double fraction = 1.25;
    number_t exact = num_create_from_string("123456789012345678901234567890");
    int bytes = fmt_print("{positive:+#,##0.00;(#,##0.00);\"zero\"}\n{negative:+#,##0.00;(#,##0.00);\"zero\"}\n{"
                          "zero:+#,##0.00;(#,##0.00);\"zero\"}\nSigned zero: {negative_zero:0.00}\nFraction: "
                          "{fraction:# ?/?}\nExact: {exact:#,##0}\n",
        FMT(positive),
        FMT(negative),
        FMT(zero),
        FMT(negative_zero),
        FMT(fraction),
        FMT(exact));
    num_destroy(&exact);
    return bytes < 0;
}
```

Output:

```text
+1,234.50
(1,234.50)
zero
Signed zero: -0.00
Fraction: 1 1/4
Exact: 123,456,789,012,345,678,901,234,567,890
```

| Code family | Behaviour under MARS conventions |
|---|---|
| `positive;negative;zero;text` | Up to four sections; two sections use the first for positive and zero values. Empty sections hide their values. A negative section formats the magnitude. |
| `0`, `#`, `?` | Mandatory digit, optional digit and optional digit reserving a space. |
| `.`, `,`, `%` | Decimal point, grouping/scaling and percentage multiplication; each trailing comma divides by 1000, each percent multiplies by 100. |
| `0.00E+00`, `##0.0E-0` | Scientific or engineering notation; `E-` omits the positive exponent sign. |
| `# ?/?`, `??/??`, `# ?/8` | Mixed, improper or fixed-denominator fractions. |
| `General`, `@` | General numerical presentation and text insertion. |
| `"literal"`, `\x` | Literal text and escaped characters, including braces and semicolons. |
| `_x`, `*x` | Reserve one visible character, or repeat a character to the supplied field width; repeat adds nothing without a width. |
| `[<100]`, `[>=100]` | Conditions in the first two sections; comparisons also accept `<=`, `>`, `=`, `<>` and signed/scientific thresholds. |
| `[Red]`, `[Color3]` | ANSI terminal colours, followed by a reset. The eight Excel colour names and palette indices 1–56 are supported. |
| `[$£-809]` | Preserve the explicit currency symbol; locale selectors do not load external locale data. |
| `d`, `dd`, `ddd`, `dddd` | Day number or English weekday name. |
| `do`, `do^` | MARS ordinal-day extensions: `10th` and `10ᵗʰ`; suffixes handle 1st/2nd/3rd and 11th/12th/13th correctly. |
| `M`, `MM`, `MMM`, `MMMM`, `MMMMM` | Month number, padded number, abbreviated name, full name or initial. |
| `m`, `mm` | Minute, unpadded or padded to two digits; always distinct from the month. |
| `yy`, `yyyy` | Two-digit or four-digit Gregorian year. |
| `h`, `hh`, `s`, `ss` | Hours and seconds. |
| `AM/PM`, `A/P` | Twelve-hour time; lower-case codes produce lower-case periods. |
| `ss.000`, `[s].000` | Fractional seconds, with rounding and carry into the next minute or day. |
| `[h]`, `[m]`, `[s]` | Elapsed hours, minutes or seconds, without wrapping at a day. |
| `[$-F800]`, `[$-F400]` | MARS long date and long time: English weekday/month names and 24-hour time. |

For example, `[Red]0.00` produces the bytes `\x1b[31m1.25\x1b[0m` for 1.25.
Colour directives also apply to `fmt_string()`; alignment counts the visible
text, excluding ANSI SGR sequences. Colours are explicitly requested and are
not automatically disabled when a stream is redirected.

Integer coefficients remain exact. Decimal and scientific modes retain the
stored precision of C reals, qfloat and real `number_t` values, including their
low components or arbitrary-precision backends. Excel codes use rounding to
nearest with halves away from zero; ordinary `f`/`e`/`g` modes retain their
existing rounding. A single numeric section preserves IEEE negative zero.
An explicit zero section selects both signed zeros. Excel numeric codes reject
non-real complex values; use the ordinary complex presentations for those.

MARS owns these conventions: English calendar names, the Gregorian calendar,
`.` for the decimal point, `,` for grouping, and the standard Excel colour
palette. Locale tags preserve explicit currency text; they do not change the
calendar, language, numeral system or separators. Alternative-calendar and
numeral-conversion directives are rejected. No external locale library is
required. Currency placement, parentheses, signs and other text come from the
code itself. Numeric dates use the 1900 serial system, including Excel's
fictitious 1900-02-29 at serial 60; actual `datetime_t` values remain Gregorian.
Serial zero displays 1900-01-00. Negative and out-of-range calendar serials
produce `########`. The format code has no workbook setting for a 1904 epoch.

Decimal precision is bounded at 10000 places; civil years are 1–9999 and
fractional seconds support up to nine places. Fraction search uses native
`long double` arithmetic and at most nine denominator places; a fixed denominator
is at most 1,000,000,000. Decimal coefficients are bounded at 100,000 digits and
a decimal point position of ±100,000. These are checked limits, rather than
silent conversion to a lower-precision numerical backend. This is a text
formatter: it supplies no spreadsheet cell layout, font metrics or workbook
palette overrides. Native non-finite reals display `#NUM!`, `∞` or `-∞`.

The code vocabulary follows Microsoft's [custom number format guidance](https://support.microsoft.com/en-us/excel/review-guidelines-for-customizing-a-number-format)
and [date/time guidance](https://support.microsoft.com/en-us/excel/format-numbers-as-dates-or-times),
with the MARS conventions and limits above.

Adapter authors can use `fmt_excel_decimal()` with unsigned coefficient digits,
a decimal point position and a sign bit; this never parses a numeric value.
`fmt_excel_real()` decomposes a C real directly, while `fmt_excel_datetime()`
accepts civil components. `fmt_excel_precision()` supplies a conversion budget
for non-terminating rational coefficients. All helpers borrow the compiled plan
in `fmt_options_t.excel`, return owned text released with `string_free()` and
leave plan ownership with the template. Ordinary custom callbacks may reject
Excel options explicitly.

## Literal words in date formats

Quote literal words so they are copied as text. `MM` always denotes the month
and `mm` always denotes minutes, regardless of neighbouring fields. For
10 February, ordinary and superscript ordinal forms are:

```c
#include "format-t.h"
#include "datetime-t.h"

int main(void)
{
    datetime_t *when = datetime_init_ymd(datetime_alloc(), 2026, DT_February, 10);
    if (!when)
        return 1;
    int bytes = fmt_print("{when:do \"day of month\" MM}\n{when:do^ \"day of month\" MM}\n", FMT(when));
    datetime_dealloc(when);
    return bytes < 0;
}
```

Output:

```text
10th day of month 02
10ᵗʰ day of month 02
```
