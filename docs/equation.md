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
{ x = 2 }
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
{ E = 1.516675548329413702316570191036517058365719731753799877808899782 }
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
