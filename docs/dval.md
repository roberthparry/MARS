# `dval_t`

`dval_t` is a reference-counted expression DAG for differentiable values.

## Capabilities

- expression construction from constants, variables, and operators
- first derivatives
- second derivatives
- integration with `qfloat`
- expression parsing and formatting

## Example: Constructing an Expression

```c
#include <stdio.h>
#include "dval.h"

/* f(x) = exp(sin(x)) + 3*x^2 - 7 */
static dval_t *make_f(dval_t *x) {
    dval_t *sinx   = dv_sin(x);
    dval_t *exp_sx = dv_exp(sinx);
    dval_t *x2     = dv_pow_d(x, 2.0);
    dval_t *term2  = dv_mul_d(x2, 3.0);
    dval_t *f0     = dv_add(exp_sx, term2);
    dval_t *f      = dv_sub_d(f0, 7.0);

    dv_free(sinx);
    dv_free(exp_sx);
    dv_free(x2);
    dv_free(term2);
    dv_free(f0);

    return f;
}

int main(void) {
    dval_t *x = dv_new_named_var_d(1.25, "x");
    dval_t *f = make_f(x);

    dval_t *df_dx = dv_create_deriv(f);
    const dval_t *d2f_dx = dv_get_deriv(df_dx);

    printf("f(x)    = "); dv_print(f);
    printf("f'(x)   = "); dv_print(df_dx);
    printf("f''(x)  = "); dv_print(d2f_dx);

    qf_printf("\nAt x = 1.25:\n");
    qf_printf("f(x)    = %.34q\n", dv_eval(f));
    qf_printf("f'(x)   = %.34q\n", dv_eval(df_dx));
    qf_printf("f''(x)  = %.34q\n", dv_eval(d2f_dx));

    dv_free(df_dx);
    dv_free(f);
    dv_free(x);

    return 0;
}
```

## Example: Parsing from a String

```c
#include <stdio.h>
#include "dval.h"

int main(void) {
    dval_t *f = dval_from_string("{ exp(sin(x)) + 3*x^2 - 7 | x = 1.25 }");
    if (!f) return 1;

    dval_t *df_dx = dv_create_deriv(f);
    const dval_t *d2f_dx = dv_get_deriv(df_dx);

    printf("f(x)    = "); dv_print(f);
    printf("f'(x)   = "); dv_print(df_dx);
    printf("f''(x)  = "); dv_print(d2f_dx);

    qf_printf("\nAt x = 1.25:\n");
    qf_printf("f(x)    = %.34q\n", dv_eval(f));
    qf_printf("f'(x)   = %.34q\n", dv_eval(df_dx));
    qf_printf("f''(x)  = %.34q\n", dv_eval(d2f_dx));

    dv_free(df_dx);
    dv_free(f);

    return 0;
}
```

## Design Notes

### Node Model

Expressions are stored as DAG nodes representing:

- constants
- variables
- unary operators
- binary operators

Shared subexpressions are represented once and reused by reference.

### Differentiation

Each node type knows how to:

- evaluate itself
- build its derivative
- release its owned state

Derivative construction produces another expression graph. Higher-order
derivatives are obtained by differentiating derivative expressions again.

### Ownership

Nodes are reference-counted. This allows shared structure without forcing deep
copies for every composed expression.

### Evaluation

Evaluation uses `qfloat` throughout, so function values and derivative values
follow the same numeric path.

## Scope

`dval_t` is best described as a compact differentiable expression system with
high-precision evaluation and repeated symbolic differentiation.

## API Reference

The full public API is declared in `include/dval.h`. The functions below are
the main entry points most users will want first.

### Construction and Lifetime

- `dv_new_named_var_d(double value, const char *name)`  
  Create a named variable node initialized from a `double`.

- `dv_free(dval_t *v)`  
  Release a `dval_t` handle.

### Expression Building

- `dv_add(dval_t *a, dval_t *b)`  
  Construct `a + b`.

- `dv_sub_d(dval_t *a, double b)`  
  Construct `a - b` with a scalar right-hand side.

- `dv_mul_d(dval_t *a, double b)`  
  Construct `a * b` with a scalar right-hand side.

- `dv_pow_d(dval_t *a, double p)`  
  Raise an expression to a scalar power.

- `dv_sin(dval_t *a)`  
  Construct `sin(a)`.

- `dv_exp(dval_t *a)`  
  Construct `exp(a)`.

### Evaluation and Differentiation

- `dv_eval(const dval_t *v)`  
  Evaluate an expression to a `qfloat`.

- `dv_create_deriv(dval_t *v)`  
  Build the first derivative expression.

- `dv_get_deriv(const dval_t *v)`  
  Access a cached derivative expression.

### Parsing and Formatting

- `dval_from_string(const char *text)`  
  Parse an expression from text.

- `dv_to_string(const dval_t *v, dv_style_t style)`  
  Convert an expression to a string in a selected style.

- `dv_print(const dval_t *v)`  
  Print an expression directly.