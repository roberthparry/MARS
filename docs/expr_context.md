# Expression contexts

Include `expr_context-t.h` for the opaque `expr_context_t` class. A context owns
symbol roles, explicit assumptions and numerical binding snapshots separately
from the original `expr_t` graph. Mathematical rewriting remains in the expression
module; a context is not another parser or simplification engine.

## Policy and ownership

`expr_context_new()` creates an empty context. `expr_context_from_expression(source,
wrt)` captures current bindings: the active variable stays free, other variables
become parameters, and named constants become parameters. With `wrt == NULL`, all
variables stay free. Capture skips unresolved calculus scopes and infinite values.
The context does not retain or mutate the source graph or its binding objects.
Capture rejects inconsistent copies of the same symbol with `EDOM`; it does not
choose one of their conflicting values.

`expr_context_set(context, symbol, role, assumptions, value)` replaces one entry
atomically. The symbol is a variable or mutable named constant, not a compound
expression or built-in constant. `NUM_NAN` means an unset binding. Other supplied
values must be finite. A role controls evidence, not differentiation semantics:

- `EXPR_CONTEXT_FREE`: the current value is used for numerical evaluation only.
- `EXPR_CONTEXT_PARAMETER`: the current value can additionally justify display
  simplifications, without substituting the value into the displayed formula.

Explicit assumptions are independent of the numerical binding and apply to either
role. Combine `EXPR_CONTEXT_REAL`, `EXPR_CONTEXT_NONZERO`, `EXPR_CONTEXT_POSITIVE`
and `EXPR_CONTEXT_NEGATIVE`. Positive and negative imply real and non-zero. Merely
being real does not justify replacing a principal square root of a square by its
base. Contradictory assumptions, or a binding that contradicts an assumption, fail
with `EDOM`; malformed arguments fail with `EINVAL`. Failed updates do not change
the context. To remove a previous assumption, replace the entry with a new mask.

`expr_context_get()` performs a hash-indexed lookup and optionally returns an owned
number, the role and normalised assumptions. Numerical evidence is never promoted
to an assumption. Variables use their stable expression identity: cloning preserves
it, but independently created same-named variables remain distinct. Named constants
use their canonical name in one context namespace; use separate contexts for
separate constant namespaces.

`expr_context_remove()` forgets an entry. `expr_context_clone()` deep-copies the
context and its revision; `expr_context_free()` releases it and accepts NULL.
`expr_context_revision()` advances on successful sets and removals. A caller-owned
cache must also distinguish context instances, source formulae and number precision;
the revision alone is not a universal result-cache key.

## Structural proof queries

`expr_context_proves(context, source, assumptions)` returns true only when it can
establish every requested fact. Use a non-empty combination of the existing
assumption bits. False includes unknown properties and invalid arguments; it does
not prove the opposite. Neither input nor the global numerical precision changes.

The query uses explicit assumptions and supplied parameter values at symbolic
leaves, then structural rules for negation, same-sign sums/differences, products,
quotients, squares and positive principal square roots. It never evaluates a
compound expression numerically to decide whether it is non-zero. For example,
a numerical variable guess cannot authorise division, and a positive argument
does not establish that its sine is non-zero. This is deliberately incomplete,
not a general theorem prover or an interval-arithmetic certificate.

The opt-in [guarded matrix solver](matrix.md#guarded-solving-and-inversion) uses
this query to decide which pivots permit division. Display simplification remains
a separate operation with its existing parameter-evaluation policy.

## Display and evaluation

Equation, differential-equation and matrix modules now consume this shared policy
through `equ_simplify_with_context()`, `equ_derive_solutions_with_context()`,
`de_solve_with_context()` and `mat_simplify_with_context()`. See the owning
[equation](equation.md), [differential-equation](diffequation.md) and
[matrix](matrix.md) guides for examples. Their NULL-context policy captures named
constants while protecting all variables. These are result-simplification APIs,
not assumption-aware branch selection or a proof of matrix invertibility.

`expr_context_simplify()` returns an owned symbolic snapshot. It extracts known
signed real squares from principal roots and removes proven real-domain conditions.
It uses conservative sign inference for negation, products, quotients and squares,
as well as complete numerical parameter evidence. Unknown conditions remain;
known false conditions give `NAN`. This is deliberately not a general assumption
theorem prover. Unresolved calculus scopes are left untouched.

`expr_context_refine()` applies the same context proofs and binding snapshot but
only runs the general beautifier when a proof actually changes the algebra. Use it
for an already arranged solver result: an unrelated context must not expand a
compact exact radical formula. Equation, differential-equation and matrix result
APIs use this operation. Both operations return caller-owned expressions.

`expr_context_evaluate()` simplifies and evaluates a fresh binding snapshot with
`expr_eval()` and returns an owned `number_t`. Proven binding-independent values
can therefore be returned even when a symbol is unset. Both free symbols and parameters use their supplied
values. Symbols absent from the context retain their source bindings, but those
values cannot justify context display proofs. Destroy returned numbers with
`num_destroy()` and expressions with `expr_free()`.

Always recompute from the original formula after a context update, not from an old
simplified result whose conditions have already been discharged. Existing results
remain independent snapshots. Changing the source's bindings does not update a
captured context: explicitly replace entries or capture again.

Contexts are single-threaded, like expression graphs, and need external
synchronisation. They do not change global number precision, impose solver work
limits or implement cancellation. Existing numerical precision and solver controls
remain in their owning APIs.

## README example: change a binding without replacing the formula

```c
#include <stdio.h>
#include <stdlib.h>

#include "expr_context-t.h"

int main(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *source = expr_from_string("{x+sqrt(s^2)/s | x=1; s=15}", &bindings);
    expr_t *x = expr_bindings_get(bindings, "x");
    expr_t *s = expr_bindings_get(bindings, "s");
    expr_context_t *context = expr_context_from_expression(source, x);
    if (!context)
        abort();
    expr_t *display = expr_context_simplify(context, source);
    char *text = expr_to_string(display, style_UNBOUND);
    printf("Positive binding: %s\n", text);
    free(text);
    expr_free(display);

    number_t negative = num_create_from_long(-15);
    if (!expr_context_set(context, s, EXPR_CONTEXT_PARAMETER, 0, negative))
        abort();
    num_destroy(&negative);
    display = expr_context_simplify(context, source);
    text = expr_to_string(display, style_UNBOUND);
    printf("Negative binding: %s\n", text);
    free(text);
    expr_free(display);
    expr_context_free(context);
    expr_free(source);
    expr_bindings_free(bindings);
    return 0;
}
```

Output:

```text
Positive binding: x + 1
Negative binding: x - 1
```

The matching README test runs after the ordinary context tests in
`tests/expr_context/test_expr_context.c`. MARS Lab uses this API for its native
calculus display and Function Run output; the browser does not reinterpret maths.
