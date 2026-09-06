# `equation_t`

`equation_t` is the public equation wrapper built on top of `expr_t`. It keeps
two expression handles, one for the left-hand side and one for the right-hand
side, and can also retain the bindings discovered while parsing equation text.

The intended public workflow is:

1. Parse or construct an equation.
2. Derive solutions through `equ_derive_solutions(...)`.
3. Inspect the resulting isolated equations with `equ_solutions_at(...)`.

## Ownership

- `equ_new(lhs, rhs)` returns an owning `equation_t *`.
- `equ_free(...)` releases the equation and any bindings it owns.
- `equ_lhs(...)`, `equ_rhs(...)`, `equ_bindings(...)`, and
  `equ_binding(...)` return borrowed views.
- `equ_derive_solutions(...)` returns an owning
  `equation_solutions_t *`, released with `equ_solutions_free(...)`.
- Each solution borrowed from a solution set is itself an `equation_t` in
  isolated form, such as `x = 2` or `E = 2.2749...`.

## Parsing Model

Equation text accepts:

```text
lhs = rhs
{ lhs = rhs }
{ lhs = rhs | x = val, ...; [name] = val, ... }
```

The first form is shorthand handled directly by `equ_from_string(...)` and
`equ_from_text(...)`. For example:

```text
x + y = z
```

is equivalent to:

```text
{ x + y = z | x = ?, y = ?, z = ? }
```

MARS infers one shared binding table before parsing the two sides, so every
occurrence of a name resolves to the same expression leaf. Callers should pass
the bare equation unchanged; Python, scratch programs, and other clients do not
need to add braces or binding declarations.

Calculus operators retain their expression-level scope within an equation.
Consequently, the `x` in `@S_0^1 f(x) dx` is local to the definite integral and
does not enter the equation's shared binding table. The completed indefinite
form `@S f(x) dx` remains a family in `x`, so `x` is included in that table.
An occurrence of `x` elsewhere on either side of the equation is free and is
therefore included normally.

The binding section is significant:

- variable bindings before `;` identify symbols that may be solved for
- constant bindings after `;` stay fixed during solving
- current variable values act as numeric starting points if symbolic isolation
  cannot finish cleanly

That means the equation text itself can express both intent and seed values.

Additive ellipses use the same native series recogniser as `expr_t`; the
equation client does not expand them. MARS first tries exact geometric and
inverse-index-power models. For a general algebraic sequence it constructs the
Lagrange polynomial `P(k)` determined by the supplied coefficient terms,
checks the written final coefficient against `P(N)`, and replaces the sequence
with the exact sigma `Σ(k=1..N) P(k)` before solving. The derivation therefore
shows the inferred sigma step before its simplified finite sum. A series is
rejected when the visible terms and endpoint do not determine and verify one
model, rather than silently extrapolating an inconsistent pattern.

## Solving Model

Open consecutive power series with a symbolic exponent are recognised natively
and retained as sums with their convergence domain. Riemann or Hurwitz zeta
identities are used internally only where the real part of the zeta argument
exceeds one. Equating the full inverse-power series starting at one to zero
therefore has no solutions: zeta has no zeros in this convergence domain.
Neither its trivial nor its non-trivial continued zeros solve that series
equation. Analytic continuation requires an explicit zeta expression.

For an unseeded real affine equation in the Riemann zeta function, the numerical
fallback searches several starting points instead of returning the first root
it encounters. A zero target searches the critical strip up to imaginary
magnitude eighty, returning up to forty roots, and reports the negative even
integers as a separate trivial family. A non-zero target searches real parts
between one and four, with imaginary magnitude at most fifty. Candidates are
refined at the requested precision,
checked against the original residual, deduplicated, and paired with verified
conjugates. The input bindings are restored afterwards. This bounded search
does not certify completeness or assume the Riemann hypothesis. Explicit
starting bindings retain the existing single-root behaviour.

The Lab combines verified numerical conjugates into one `±` row in both the
Rendered TeX and Solutions cards. The native solution set still contains both
roots; pairing uses the numerical values, not their rounded display text.
Symbolic solution formulae and parameterised families remain unchanged.

The native search, family and interpretation notes are available through
`equ_solutions_search_note`, `equ_solutions_family_note` and
`equ_interpretation_note`; clients should display them alongside numerical
results. `equ_solutions_proven_empty` distinguishes a proved empty solution set
from a numerical search that did not find a root.
The Lab reports a proved empty solution set as **No solutions**. It does not
start a separate zeta solve or replace the authored equation.

There is one public solve entry point:

```c
equation_solutions_t *equ_derive_solutions(const equation_t *equation);
```

The solver works in two stages:

1. It tries symbolic isolation for each variable binding while treating the
   other variable bindings symbolically rather than as fixed numeric values.
2. If no concise symbolic solution can be derived, it falls back to numeric
   goal-seeking and uses the current variable binding values as the initial
   guess.

Expanded numeric-coefficient polynomials have no hard-coded maximum degree.
Above degree five, the solver repeatedly finds one real or complex root with
Newton iteration, removes its `(x - root)` factor by synthetic division, and
continues with the deflated polynomial until the quartic/cubic path can finish
the remaining roots. Odd-degree real-coefficient stages first use safeguarded
real Newton iteration for the real root guaranteed by odd degree.

When the active polynomial has real coefficients, finding a non-real root
also supplies its complex conjugate. The solver returns both and divides by
their combined real factor
`x² - 2 Re(root)x + |root|²`. This reduces the degree by two after one Newton
search, preserves real coefficients for later stages, and avoids the numerical
drift caused by deflating the two roots independently.

Deflation is implemented as an iterative constant-stack loop, so increasing
the polynomial degree does not increase solver call-stack depth. Coefficient
storage is allocated from the detected degree rather than a fixed maximum.
Returned roots are polished against the original polynomial, and repeated
roots are represented once. Complex starting seeds allow polynomials with no
real roots to return their complex solutions. As with any numerical root
finder, practical limits are available memory, requested precision, polynomial
conditioning, and Newton convergence rather than a fixed degree cutoff.

This lets equations stay symbolic when they can, while still supporting
seed-driven numeric cases.

## Example: Quadratic Solve

```c
#include <stdio.h>
#include "equation.h"
#include "number.h"

int main(void)
{
    size_t saved_digits = num_get_default_prec_digits();

    num_set_default_prec_digits(72u);

    equation_t *equation = equ_from_string("x^2 - x - 1 = 0");
    equation_solutions_t *solutions = equ_derive_solutions(equation);

    for (size_t i = 0; i < equ_solutions_count(solutions); ++i) {
        const equation_t *solution = equ_solutions_at(solutions, i);
        string_t *text = equ_to_text(solution, style_UNBOUND);

        if (text)
            string_printf("solution[%zu] = %S\n", i, text);
        string_free(text);
    }

    equ_solutions_free(solutions);
    equ_free(equation);
    num_set_default_prec_digits(saved_digits);
    return 0;
}
```

```text
solution[0] = x = φ
solution[1] = x = -0.618033988749894848204586834365638117720309179805762862135448622705260463
```

## Example: Sextic Solve

The same API returns every real and complex root of a higher-degree
polynomial:

```c
#include <stdio.h>
#include "equation.h"
#include "number.h"

int main(void)
{
    size_t saved_digits = num_get_default_prec_digits();

    num_set_default_prec_digits(72u);

    equation_t *equation =
        equ_from_string("x^6 - 3x^5 + 2x^2 + x + 5 = 0");
    equation_solutions_t *solutions = equ_derive_solutions(equation);

    for (size_t i = 0; i < equ_solutions_count(solutions); ++i) {
        const equation_t *solution = equ_solutions_at(solutions, i);
        string_t *text = equ_to_text(solution, style_UNBOUND);

        if (text)
            string_printf("solution[%zu] = %S\n", i, text);
        string_free(text);
    }

    equ_solutions_free(solutions);
    equ_free(equation);
    num_set_default_prec_digits(saved_digits);
    return 0;
}
```

```text
solution[0] = x = 2.87587959287965253104338744502875326643144700900233125903456059327763929
solution[1] = x = 1.48300060305773653925863499958066314569200882396788553213489163994551491
solution[2] = x = 0.152529630613435764402874851539776724292516857918173356861770481909018717 + 1.0197531742297246929794462458263028763323671163738368943478084728827189i
solution[3] = x = 0.152529630613435764402874851539776724292516857918173356861770481909018717 - 1.0197531742297246929794462458263028763323671163738368943478084728827189i
solution[4] = x = -0.831969728582130299553886073844484930354244774403281752446496598520595813 + 0.640725754005657708312285154206959000214229532454964496243619708833644616i
solution[5] = x = -0.831969728582130299553886073844484930354244774403281752446496598520595813 - 0.640725754005657708312285154206959000214229532454964496243619708833644616i
```

## Example: Kepler's Equation

Kepler's equation is a good example of why bindings belong in the equation
text:

```c
#include <stdio.h>
#include "equation.h"
#include "number.h"

int main(void)
{
    num_set_default_prec_digits(64);

    equation_t *kepler =
        equ_from_string("{ M = E - e*sin(E) | E = 1.5; M = 1.5, e = 0.0167 }");
    equation_solutions_t *solutions = equ_derive_solutions(kepler);
    const equation_t *first = equ_solutions_at(solutions, 0u);

    if (first)
        equ_print(first);

    equ_solutions_free(solutions);
    equ_free(kepler);
    return 0;
}
```

```text
{ E = 1.5166755483294137023165701910365170583657197317537998778088997827671884494619019320765089129857646158810339559248499853988100044 | E = 1.5166755483294137023165701910365170583657197317537998778088997827671884494619019320765089129857646158810339559248499853988100044 }
```

Here:

- `E = 1.5` marks `E` as the variable to solve and supplies its initial guess
- `M = 1.5` and `e = 0.0167` are constant bindings

If a concise symbolic result is not available, those values are enough for the
numeric fallback to converge on the eccentric anomaly.

The call to `num_set_default_prec_digits(64)` sets the working precision to
about 64 significant decimal digits before the equation is parsed and solved.

## Output Forms

Summation and product indices are local to their terms, not equation unknowns.
This applies both to mathematical notation and to function-call notation,
including finite sums generated from ellipsis sequences. Bounds are parsed in
the surrounding scope, and a nested index may shadow an outer name without
replacing its binding.

- `equ_to_text(..., style_EXPRESSION)` produces a parseable wrapper form
- `equ_to_text(..., style_FUNCTION)` produces an equation-valued callable
  that preserves the left- and right-hand expressions, notes unbound variables
  with a Function-style line comment beginning with two backticks, then uses
  the compact `output(equ(...).solve()).` form
- `equ_to_text(..., style_UNBOUND)` shows the plain equation body
- `equ_to_text(..., style_LATEX)` emits TeX-ready display text
- `equ_to_TeX_body_wrapped(...)` emits an owning aligned TeX body and honours
  native derivation metadata such as a finite sigma step retained while the
  solver uses its expanded value
- `equ_display_expanded(...)` builds a display equation whose polynomial sides
  are collected and written in descending powers of the selected variable
- `equ_residual(...)` builds the simplified residual `lhs - rhs`
- `equ_print(...)` prints the `style_EXPRESSION` form to `stdout`

For display code, it is often useful to show the original input on one side and
derived solutions or residuals on the other.

```c
#include "equation.h"

int main(void)
{
    equation_t *equation = equ_from_string("2*x + 3 = 7");
    string_t *text = equ_to_text(equation, style_UNBOUND);

    if (text)
        string_printf("%S\n", text);

    string_free(text);
    equ_free(equation);
    return 0;
}
```

```text
2x + 3 = 7
```

## API Reference

### `equ_deserialise()`

Creates or reconstructs the public value described by deserialise.

```c
equation_t *equ_deserialise(const void *data, size_t len, const string_t *type, const string_t *encoding);
```

### `equ_is_solved_for()`

Reports whether the condition described by is solved for holds.

```c
bool equ_is_solved_for(const equation_t *equation, const expr_t *wrt);
```

### `equ_printf()`

Returns the public result described by printf.

```c
int equ_printf(const char *fmt, ...);
```

### `equ_serialize()`

Reports whether the condition described by serialize holds.

```c
bool equ_serialize(const equation_t *equation, string_t **out_type, string_t **out_encoding, void **out_data, size_t *out_len);
```

### `equ_sprintf()`

Returns the public result described by sprintf.

```c
int equ_sprintf(char *out, size_t out_size, const char *fmt, ...);
```

### `equ_sprintf_text()`

Returns the public result described by sprintf text.

```c
string_t *equ_sprintf_text(const char *fmt, ...);
```

### `equ_vsprintf_text()`

Returns the public result described by vsprintf text.

```c
string_t *equ_vsprintf_text(const char *fmt, va_list ap);
```
