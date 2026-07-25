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

## Solving Model

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

This lets equations stay symbolic when they can, while still supporting
seed-driven numeric cases.

## Example: Simple Affine Solve

```c
#include <stdio.h>
#include "equation.h"

int main(void)
{
    equation_t *equation = equ_from_string("{ 2*x + 3 = 7 | x = ? }");
    equation_solutions_t *solutions = equ_derive_solutions(equation);
    const equation_t *first = equ_solutions_at(solutions, 0u);

    if (first)
        equ_print(first);

    equ_solutions_free(solutions);
    equ_free(equation);
    return 0;
}
```

```text
{ x = 2 | x = NAN }
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

- `equ_to_text(..., style_EXPRESSION)` produces a parseable wrapper form
- `equ_to_text(..., style_UNBOUND)` shows the plain equation body
- `equ_to_text(..., style_TEX)` emits TeX-ready display text
- `equ_residual(...)` builds the simplified residual `lhs - rhs`
- `equ_print(...)` prints the `style_EXPRESSION` form to `stdout`

For display code, it is often useful to show the original input on one side and
derived solutions or residuals on the other.

```c
#include "equation.h"

int main(void)
{
    equation_t *equation = equ_from_string("{ 2*x + 3 = 7 | x = ? }");

    equ_print(equation);
    equ_free(equation);
    return 0;
}
```

```text
{ 2x + 3 = 7 | x = NAN }
```
