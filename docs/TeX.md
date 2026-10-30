# TeX notation

`TeX_t`, declared in `TeX-t.h`, is an opaque, immutable mathematical notation
value. Use `TeX_builder_t` to construct one in reverse Polish order: push
operands, then apply an operation. The builder owns every intermediate value;
there is no separate cleanup list. `TeX_builder_finish` transfers the single
completed formula to you. Free it with `TeX_free` and free the builder with
`TeX_builder_free`.

The builder supplies typesetting syntax without doing algebra. Native
mathematical text goes through `TeX_from_expression` or `TeX_push_math`, which
delegate parsing and simplification to the mathematical modules. Neither path
runs a TeX engine. Existing expression, equation, matrix and differential-equation
output APIs continue to manage their rendering contexts internally.

Every push and operation returns a Boolean. A failure leaves the stack unchanged and
saves the first error. Later operations and finish fail until you explicitly
clear that error. This means the examples can check the finished result once,
without accidentally accepting an incomplete formula after an ignored failure.

Arithmetic follows ordinary RPN: push the left operand, push the right operand,
then apply one binary operator. For a further factor, push it and multiply again.
There is no operand-count argument for arithmetic. Argument counts on function
calls describe the function's arity; they are construction metadata, not printed numbers.

`TeX_push_symbol` takes keyboard-friendly names such as `psi`, `mu`, `lambda`,
`Lambda`, `hbar`, `partial` and `infinity`; Unicode is optional. Arithmetic operations
supply their own syntax. `TeX_push_text` escapes literal prose. `TeX_push_raw`
is reserved for embedding source from existing trusted renderers. None retains
its input. `TeX_push_notation` copies an existing value,
including an operand already on the stack. `TeX_source` returns a borrowed
pointer, while `TeX_render` returns an independent `string_t`. Immutable values
may be read concurrently; access to a shared builder and destruction require
external synchronisation.

## Composition example

Each C block is a complete programme. The Markdown tests extract and compile
these exact blocks, run them and compare their output with the accompanying
text. Run them after the ordinary tests with
`MARS_ROOT="$PWD" make -j1 test-readme-examples`. A TeX-only run checks thirteen
programmes; other enabled examples are counted too.

Push the numerator first and the denominator second, then apply the fraction
and square root. Only the completed result and the builder require cleanup.

```c
/**
 * @file TeX_example_1.c
 * @brief Construct a fraction inside a square root in reverse Polish order.
 *
 * The builder owns every intermediate fragment and transfers the finished notation.
 */
#include <stdio.h>
#include <stdlib.h>

#include "TeX-t.h"

int main(void)
{
    TeX_builder_t *builder = TeX_builder_new();
    TeX_push_symbol(builder, "x");
    TeX_push_integer(builder, 2);
    TeX_frac(builder);
    TeX_sqrt(builder);
    TeX_t *formula = TeX_builder_finish(builder);
    TeX_builder_free(builder);
    int status = formula && puts(TeX_source(formula)) != EOF ? EXIT_SUCCESS : EXIT_FAILURE;
    TeX_free(formula);
    if (status != EXIT_SUCCESS)
        fputs("Could not construct or print TeX.\n", stderr);
    return status;
}
```

Output:

```text
\sqrt{\frac{x}{2}}
```

Rendered mathematics:

$\quad\begin{array}{l}\displaystyle \sqrt{\frac{x}{2}}\end{array}$

## Indexed expression with nested roots

Leave the left side on the stack while constructing the right side. Push each
root's radicand before its index; push both fraction operands before applying
the fraction. The caption is escaped literal text, and the wide gap is selected
by name rather than by writing a TeX spacing command.

```c
/**
 * @file TeX_example_2.c
 * @brief Build an indexed equation, nested radicals and a literal caption using RPN.
 *
 * ASCII symbol names and structured arithmetic preserve authored order without TeX source fragments.
 */
#include <stdio.h>
#include <stdlib.h>

#include "TeX-t.h"

int main(void)
{
    TeX_builder_t *builder = TeX_builder_new();
    TeX_push_symbol(builder, "r");
    TeX_push_symbol(builder, "n");
    TeX_subscript(builder);

    TeX_push_integer(builder, 1);
    TeX_push_symbol(builder, "x");
    TeX_push_symbol(builder, "n");
    TeX_push_integer(builder, 2);
    TeX_scripts(builder);
    TeX_add(builder);
    TeX_push_integer(builder, 3);
    TeX_root(builder);

    TeX_push_integer(builder, 1);
    TeX_push_symbol(builder, "lambda");
    TeX_push_integer(builder, 2);
    TeX_pow(builder);
    TeX_add(builder);
    TeX_sqrt(builder);
    TeX_frac(builder);
    TeX_equation(builder, TeX_EQUATION_INLINE);

    TeX_push_space(builder, TeX_SPACE_QUAD);
    string_t *caption = string_new_with("scaled & normalised");
    TeX_push_text(builder, caption);
    string_free(caption);
    TeX_sequence(builder, 3);
    TeX_t *formula = TeX_builder_finish(builder);
    TeX_builder_free(builder);
    int status = formula && puts(TeX_source(formula)) != EOF ? EXIT_SUCCESS : EXIT_FAILURE;
    TeX_free(formula);
    if (status != EXIT_SUCCESS)
        fputs("Could not construct or print TeX.\n", stderr);
    return status;
}
```

Output:

```text
{r}_{n} = \frac{\sqrt[3]{1 + {x}_{n}^{2}}}{\sqrt{1 + {\lambda}^{2}}}\qquad \text{scaled \& normalised}
```

Rendered mathematics:

$\quad\begin{array}{l}\displaystyle {r}_{n} = \frac{\sqrt[3]{1 + {x}_{n}^{2}}}{\sqrt{1 + {\lambda}^{2}}}\qquad \text{scaled \& normalised}\end{array}$

## A symmetric projection matrix

For real $x$, this projects onto the direction $(1,x)$. Matrix cells are pushed
in row-major order. `TeX_dup(builder)` duplicates the top value without
introducing shared ownership, following the ordinary RPN DUP operation.
The scalar and matrix are combined with thin
spacing by the binary `TeX_mul` operation.
In the matrix call, `2, 2` specifies two rows and two columns,
`TeX_MATRIX_BRACKETS` selects square brackets, and `6` requests six TeX points
of extra row spacing. These settings describe the layout, not additional matrix entries.

```c
/**
 * @file TeX_example_3.c
 * @brief Construct a scaled projection matrix from a row-major RPN stack.
 *
 * Repeated cells use stack duplication; no intermediate notation needs manual cleanup.
 */
#include <stdio.h>
#include <stdlib.h>

#include "TeX-t.h"

int main(void)
{
    TeX_builder_t *builder = TeX_builder_new();
    TeX_push_symbol(builder, "x");
    TeX_function(builder, "P", 1);
    TeX_push_integer(builder, 1);
    TeX_push_integer(builder, 1);
    TeX_push_symbol(builder, "x");
    TeX_push_integer(builder, 2);
    TeX_pow(builder);
    TeX_add(builder);
    TeX_frac(builder);

    TeX_push_integer(builder, 1);
    TeX_push_symbol(builder, "x");
    TeX_dup(builder);
    TeX_dup(builder);
    TeX_push_integer(builder, 2);
    TeX_pow(builder);
    TeX_matrix(builder, 2, 2, TeX_MATRIX_BRACKETS, 6);
    TeX_mul(builder);
    TeX_equation(builder, TeX_EQUATION_INLINE);

    TeX_t *formula = TeX_builder_finish(builder);
    TeX_builder_free(builder);
    int status = formula && puts(TeX_source(formula)) != EOF ? EXIT_SUCCESS : EXIT_FAILURE;
    TeX_free(formula);
    if (status != EXIT_SUCCESS)
        fputs("Could not construct or print TeX.\n", stderr);
    return status;
}
```

Output:

```text
P\left(x\right) = \frac{1}{1 + {x}^{2}}\,\begin{bmatrix}1 & x \\[6pt] x & {x}^{2}\end{bmatrix}
```

Rendered mathematics:

$\quad\begin{array}{l}\displaystyle P\left(x\right) = \frac{1}{1 + {x}^{2}}\,\begin{bmatrix}1 & x \\[6pt] x & {x}^{2}\end{bmatrix}\end{array}$

## Calculus and tensor notation

`TeX_sum` and `TeX_integral` take `TeX_LIMIT_NONE`,
`TeX_LIMIT_LOWER`, `TeX_LIMIT_UPPER` or `TeX_LIMIT_BOTH`. Push the body first;
for an integral push the variable next. Any bounds follow, lower before upper.
Repeated integral operations construct iterated integrals. `TeX_derivative`
consumes the body followed by its variable and takes the positive order and
ordinary/partial presentation as arguments. Keep coefficients beside the
derivative's own fraction rather than placing it inside another fraction.

`TeX_tensor` consumes a base followed by ordered index values, using the
supplied lower/upper positions without contracting indices or checking dimensions.
`TeX_fence` supplies visible mathematical delimiters. These are notation
operations; the native mathematical modules own calculation and interpretation.

## Einstein, Schrödinger and Dirac equations

The following programme prints Einstein's field equations with explicit $c$,
the one-dimensional time-dependent Schrödinger equation, the free Dirac equation
in natural units, a wavefunction expansion and its normalisation. The Dirac
convention is Minkowski signature $(+,-,-,-)$, with repeated $\mu$ summed.
The equations follow David Tong's lectures on
[general relativity](https://www.damtp.cam.ac.uk/user/tong/gr/grhtml/S4.html),
[quantum mechanics](https://www.damtp.cam.ac.uk/user/tong/qm/qmhtml/S1.html) and
[the Dirac equation](https://www.damtp.cam.ac.uk/user/tong/qft/qfthtml/S4.html).

One reusable builder replaces the ownership list. A small helper pushes a
repeated lower-index pair; another prints and frees each finished formula.
Plain symbols and RPN operations supply every mathematical component, including
arithmetic and function calls. No TeX command fragments are needed. No extra tensor package
is needed; matrices use the standard `amsmath` environments.

```c
/**
 * @file TeX_physics.c
 * @brief Construct field equations and wavefunction notation from structured RPN operations.
 *
 * Symbols use ASCII names; arithmetic, calls and calculus supply their own presentation syntax.
 */
#include <stdio.h>
#include <stdlib.h>

#include "TeX-t.h"

static void lower_pair(TeX_builder_t *builder, const char *symbol)
{
    const TeX_index_position_t positions[] = {TeX_INDEX_LOWER, TeX_INDEX_LOWER};
    TeX_push_symbol(builder, symbol);
    TeX_push_symbol(builder, "mu");
    TeX_push_symbol(builder, "nu");
    TeX_tensor(builder, positions, 2);
}

static void wavefunction(TeX_builder_t *builder)
{
    TeX_push_symbol(builder, "x");
    TeX_push_symbol(builder, "t");
    TeX_function(builder, "psi", 2);
}

static bool print_formula(TeX_builder_t *builder)
{
    TeX_t *formula = TeX_builder_finish(builder);
    bool ok = formula && puts(TeX_source(formula)) != EOF;
    TeX_free(formula);
    return ok;
}

int main(void)
{
    TeX_builder_t *b = TeX_builder_new();
    int status = EXIT_FAILURE;

    lower_pair(b, "R");
    TeX_push_integer(b, 1);
    TeX_push_integer(b, 2);
    TeX_frac(b);
    TeX_push_symbol(b, "R");
    TeX_mul(b);
    lower_pair(b, "g");
    TeX_mul(b);
    TeX_sub(b);
    TeX_push_symbol(b, "Lambda");
    lower_pair(b, "g");
    TeX_mul(b);
    TeX_add(b);
    TeX_push_integer(b, 8);
    TeX_push_symbol(b, "pi");
    TeX_mul(b);
    TeX_push_symbol(b, "G");
    TeX_mul(b);
    TeX_push_symbol(b, "c");
    TeX_push_integer(b, 4);
    TeX_pow(b);
    TeX_frac(b);
    lower_pair(b, "T");
    TeX_mul(b);
    TeX_equation(b, TeX_EQUATION_INLINE);
    if (!print_formula(b)) goto done;

    TeX_push_symbol(b, "i");
    TeX_push_symbol(b, "hbar");
    TeX_mul(b);
    wavefunction(b);
    TeX_push_symbol(b, "t");
    TeX_derivative(b, 1, TeX_DERIVATIVE_PARTIAL);
    TeX_mul(b);
    TeX_push_symbol(b, "hbar");
    TeX_push_integer(b, 2);
    TeX_pow(b);
    TeX_push_integer(b, 2);
    TeX_push_symbol(b, "m");
    TeX_mul(b);
    TeX_frac(b);
    wavefunction(b);
    TeX_push_symbol(b, "x");
    TeX_derivative(b, 2, TeX_DERIVATIVE_PARTIAL);
    TeX_mul(b);
    TeX_neg(b);
    TeX_push_symbol(b, "x");
    TeX_push_symbol(b, "t");
    TeX_function(b, "V", 2);
    wavefunction(b);
    TeX_mul(b);
    TeX_add(b);
    TeX_equation(b, TeX_EQUATION_INLINE);
    if (!print_formula(b)) goto done;

    const TeX_index_position_t upper[] = {TeX_INDEX_UPPER}, lower[] = {TeX_INDEX_LOWER};
    TeX_push_symbol(b, "i");
    TeX_push_symbol(b, "gamma");
    TeX_push_symbol(b, "mu");
    TeX_tensor(b, upper, 1);
    TeX_mul(b);
    TeX_push_symbol(b, "partial");
    TeX_push_symbol(b, "mu");
    TeX_tensor(b, lower, 1);
    TeX_mul(b);
    TeX_push_symbol(b, "m");
    TeX_sub(b);
    TeX_fence(b, TeX_FENCE_PARENTHESES);
    TeX_push_symbol(b, "psi");
    TeX_mul(b);
    TeX_push_integer(b, 0);
    TeX_equation(b, TeX_EQUATION_INLINE);
    if (!print_formula(b)) goto done;

    TeX_push_symbol(b, "x");
    TeX_function(b, "psi", 1);
    TeX_push_symbol(b, "a");
    TeX_push_symbol(b, "n");
    TeX_subscript(b);
    TeX_push_symbol(b, "phi");
    TeX_push_symbol(b, "n");
    TeX_subscript(b);
    TeX_push_symbol(b, "x");
    TeX_call(b, 1);
    TeX_mul(b);
    TeX_push_symbol(b, "n");
    TeX_push_integer(b, 0);
    TeX_equation(b, TeX_EQUATION_INLINE);
    TeX_push_symbol(b, "infinity");
    TeX_sum(b, TeX_LIMIT_BOTH);
    TeX_equation(b, TeX_EQUATION_INLINE);
    if (!print_formula(b)) goto done;

    TeX_push_symbol(b, "x");
    TeX_function(b, "psi", 1);
    TeX_fence(b, TeX_FENCE_ABSOLUTE);
    TeX_push_integer(b, 2);
    TeX_pow(b);
    TeX_push_symbol(b, "x");
    TeX_push_symbol(b, "infinity");
    TeX_neg(b);
    TeX_push_symbol(b, "infinity");
    TeX_integral(b, TeX_LIMIT_BOTH);
    TeX_push_integer(b, 1);
    TeX_equation(b, TeX_EQUATION_INLINE);
    if (!print_formula(b)) goto done;
    status = EXIT_SUCCESS;
done:
    TeX_builder_free(b);
    if (status != EXIT_SUCCESS)
        fputs("Could not construct or print TeX.\n", stderr);
    return status;
}
```

Output:

```text
{R}_{\mu}{}_{\nu} - \frac{1}{2}\,R\,{g}_{\mu}{}_{\nu} + \Lambda\,{g}_{\mu}{}_{\nu} = \frac{8\,\pi\,G}{{c}^{4}}\,{T}_{\mu}{}_{\nu}
i\,\hbar\,\frac{\partial {\psi\left(x, t\right)}}{\partial {t}} = -\frac{{\hbar}^{2}}{2\,m}\,\frac{\partial^{2} {\psi\left(x, t\right)}}{\partial {x}^{2}} + V\left(x, t\right)\,\psi\left(x, t\right)
\left( i\,{\gamma}^{\mu}\,{\partial}_{\mu} - m \right)\,\psi = 0
\psi\left(x\right) = \sum_{n = 0}^{\infty} {{a}_{n}\,{\phi}_{n}\left(x\right)}
\int_{-\infty}^{\infty} {{\left| \psi\left(x\right) \right|}^{2}}\,\mathrm{d}{x} = 1
```

Rendered mathematics:

$\quad\begin{array}{l}\displaystyle {R}_{\mu}{}_{\nu} - \frac{1}{2}\,R\,{g}_{\mu}{}_{\nu} + \Lambda\,{g}_{\mu}{}_{\nu} = \frac{8\,\pi\,G}{{c}^{4}}\,{T}_{\mu}{}_{\nu}\end{array}$

$\quad\begin{array}{l}\displaystyle i\,\hbar\,\frac{\partial {\psi\left(x, t\right)}}{\partial {t}} = -\frac{{\hbar}^{2}}{2\,m}\,\frac{\partial^{2} {\psi\left(x, t\right)}}{\partial {x}^{2}} + V\left(x, t\right)\,\psi\left(x, t\right)\end{array}$

$\quad\begin{array}{l}\displaystyle \left( i\,{\gamma}^{\mu}\,{\partial}_{\mu} - m \right)\,\psi = 0\end{array}$

$\quad\begin{array}{l}\displaystyle \psi\left(x\right) = \sum_{n = 0}^{\infty} {{a}_{n}\,{\phi}_{n}\left(x\right)}\end{array}$

$\quad\begin{array}{l}\displaystyle \int_{-\infty}^{\infty} {{\left| \psi\left(x\right) \right|}^{2}}\,\mathrm{d}{x} = 1\end{array}$

## Ordinary mathematical text without TeX commands

Use `TeX_from_expression` when the input is a mathematical expression rather
than literal prose or trusted TeX source. It delegates to `expr_from_text_preserving_calculus`,
the expression module's body renderer, and `expr_beautify` for algebra-only input. Fractions, powers,
function names and grouping are handled by the existing mathematical engine.
The input is borrowed; free the returned object with `TeX_free`.

Algebra-only input deliberately includes native simplification, so it is not a
verbatim transcription of the source. It inherits the expression module's
domain conventions. Calculus operations are retained for typesetting instead
of being evaluated. It does not solve arbitrary equations or infer tensor semantics.
Malformed expressions return null and use the expression parser's diagnostics.
Null input and embedded NUL characters are rejected.

In the example below, `1/2(...)` means `(1/2) * (...)`. Write parentheses
explicitly when the whole product belongs in the denominator. MARS uses `log`
for base ten (rendered as `lg`); use `ln` for the natural logarithm.

```c
/**
 * @file TeX_expression_example.c
 * @brief Render ordinary mathematical text without writing TeX commands.
 *
 * Delegates parsing and readable arrangement to the native expression module,
 * prints the resulting TeX and releases both independently owned objects.
 */
#include <stdio.h>
#include <stdlib.h>

#include "TeX-t.h"

int main(void)
{
    string_t *input = string_new_with("1/2(x^2 - 4)/(x-1)*sin(x)/x*log(1/x)");
    TeX_t *notation = TeX_from_expression(input);
    string_free(input);
    int status = EXIT_FAILURE;
    if (notation)
        status = puts(TeX_source(notation)) == EOF ? EXIT_FAILURE : EXIT_SUCCESS;
    TeX_free(notation);
    if (status != EXIT_SUCCESS)
        fputs("Could not parse or print the expression.\n", stderr);
    return status;
}
```

Output:

```text
\frac{\left(x^{2} - 4\right)\mkern-2mu \sin(x)\mkern-2mu \lg(\frac{1}{x})}{2\mkern-2mu x\mkern-2mu \left(x - 1\right)}
```

Rendered mathematics:

$\quad\begin{array}{l}\displaystyle \frac{\left(x^{2} - 4\right)\mkern-2mu \sin(x)\mkern-2mu \lg(\frac{1}{x})}{2\mkern-2mu x\mkern-2mu \left(x - 1\right)}\end{array}$

Push the result with `TeX_push_notation` to use it in a matrix, sequence or any
other builder operation. `TeX_push_text` escapes literal text;
`TeX_push_raw` accepts trusted TeX.
Neither of those constructors interprets an ordinary mathematical expression.

## Native calculus shorthand

`TeX_from_expression` retains calculus notation instead of calculating it.
Use a parenthesised operand after a derivative prefix. The native parser accepts
`d/dx(...)`, `d^2/dx^2(...)` and `d^2/dxy(...)` alongside `Dx(...)`, `Dxy(...)`,
`Dxyx(...)` and `D[x^2y](...)`. Numerator and denominator derivative orders must
agree and must be positive integers. Mixed derivative variables retain their
application order; the operator denominator is written in reverse order, as
in composition of differential operators. These coordinate derivatives use
partial-derivative notation, including when only one coordinate is specified.

`@F` and `@Finv` retain forward and inverse Fourier transforms; `@L` and `@Linv`
retain Laplace transforms. `@S` retains an integral sign, with optional bounds,
`@Z` retains its sum and limits, and `@P` retains its product even when the result
could be computed. All registered MARS function names and aliases use the native
parser and its existing TeX renderings, including special functions and probability
distributions. There is no separate function allowlist in TeX. Use the expression
module's documented spellings and argument conventions. Algebra-only input still
uses the native beautifier. For evaluation, use the expression module's ordinary
parser and simplifier instead. No calculus grammar is duplicated in TeX.

```c
/**
 * @file TeX_calculus_example.c
 * @brief Render native integral, sum and derivative shorthand without evaluating it.
 *
 * Each source is parsed through the public TeX convenience API. The programme
 * prints one mathematical expression per line and releases all owned values.
 */
#include <stdio.h>
#include <stdlib.h>

#include "TeX-t.h"

int main(void)
{
    const char *sources[] = {
        "@F{exp(-x^2)}",
        "@Finv{exp(-k^2)}",
        "@L{sin(t)}",
        "@Linv{1/(s+1)}",
        "@P_(k=1)^3 k",
        "gamma(x)",
        "BesselJ(2,x)",
        "lambertw0(x)",
        "@S x^2 dx",
        "@S_0^1 x^2 dx",
        "@Z_(k=1)^3 k^2",
        "d/dx(sin(x))",
        "d^2/dx^2(sin(x))",
        "d^2/dxy(x^2*y^3)",
        "Dxy(x^2*y^3)",
        "Dxyx(x^3*y^2)"
    };
    for (size_t i = 0; i < sizeof sources / sizeof *sources; ++i) {
        string_t *input = string_new_with(sources[i]);
        TeX_t *notation = TeX_from_expression(input);
        string_free(input);
        int printed = notation ? puts(TeX_source(notation)) : EOF;
        TeX_free(notation);
        if (printed == EOF) {
            fputs("Could not render calculus notation.\n", stderr);
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
```

Output:

```text
\mathcal{F}_{x\to k}\{e^{-x^{2}}\}
\mathcal{F}^{-1}_{k\to x}\{e^{-k^{2}}\}
\mathcal{L}_{t\to s}\{\sin(t)\}
\mathcal{L}^{-1}_{s\to t}\{\frac{1}{s + 1}\}
\prod_{k=1}^{3}k
\Gamma(x)
J_{2}\left(x\right)
W_{0}(x)
\int x^{2}\, dx
\int_{0}^{1} x^{2}\, dx
\sum_{k=1}^{3}k^{2}
\frac{\partial}{\partial x}\left[\sin(x)\right]
\frac{\partial^{2}}{\partial x^{2}}\left[\sin(x)\right]
\frac{\partial^{2}}{\partial y\,\partial x}\left[x^{2}\mkern-2mu y^{3}\right]
\frac{\partial^{2}}{\partial y\,\partial x}\left[x^{2}\mkern-2mu y^{3}\right]
\frac{\partial^{3}}{\partial x\,\partial y\,\partial x}\left[x^{3}\mkern-2mu y^{2}\right]
```

Rendered mathematics, in the same order as the programme output:

$\quad\begin{array}{l}\displaystyle \mathcal{F}_{x\to k}\{e^{-x^{2}}\}\end{array}$

$\quad\begin{array}{l}\displaystyle \mathcal{F}^{-1}_{k\to x}\{e^{-k^{2}}\}\end{array}$

$\quad\begin{array}{l}\displaystyle \mathcal{L}_{t\to s}\{\sin(t)\}\end{array}$

$\quad\begin{array}{l}\displaystyle \mathcal{L}^{-1}_{s\to t}\{\frac{1}{s + 1}\}\end{array}$

$\quad\begin{array}{l}\displaystyle \prod_{k=1}^{3}k\end{array}$

$\quad\begin{array}{l}\displaystyle \Gamma(x)\end{array}$

$\quad\begin{array}{l}\displaystyle J_{2}\left(x\right)\end{array}$

$\quad\begin{array}{l}\displaystyle W_{0}(x)\end{array}$

$\quad\begin{array}{l}\displaystyle \int x^{2}\, dx\end{array}$

$\quad\begin{array}{l}\displaystyle \int_{0}^{1} x^{2}\, dx\end{array}$

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{3}k^{2}\end{array}$

$\quad\begin{array}{l}\displaystyle \frac{\partial}{\partial x}\left[\sin(x)\right]\end{array}$

$\quad\begin{array}{l}\displaystyle \frac{\partial^{2}}{\partial x^{2}}\left[\sin(x)\right]\end{array}$

$\quad\begin{array}{l}\displaystyle \frac{\partial^{2}}{\partial y\,\partial x}\left[x^{2}\mkern-2mu y^{3}\right]\end{array}$

$\quad\begin{array}{l}\displaystyle \frac{\partial^{2}}{\partial y\,\partial x}\left[x^{2}\mkern-2mu y^{3}\right]\end{array}$

$\quad\begin{array}{l}\displaystyle \frac{\partial^{3}}{\partial x\,\partial y\,\partial x}\left[x^{3}\mkern-2mu y^{2}\right]\end{array}$

## Existing renderers

The [expression](expression.md), [matrix](matrix.md), [equation](equation.md) and
[differential-equation](diffequation.md) output APIs retain their existing contracts.
TeX owns typesetting syntax, symbol spellings, grouping, delimiters and layout.
The mathematical modules supply owned semantic snapshots through callbacks; TeX
does not inspect their private objects or parse rendered output to recover meaning.
Expression selects operation semantics, binding values, constant identities,
series identities and temporary names. Matrix selects factors, additive constants
and native real/imaginary components. Equation and differential equation select
their solutions, conditions and derivation steps.

Rendering contexts and providers are private implementation details. Ordinary
callers continue to request `style_LATEX`, `MAT_STRING_LATEX` or the documented
`*_to_TeX_*` APIs; they never construct or manage a rendering context. The native
text convenience entry point delegates parsing to those same mathematical modules.

Wrapped and unwrapped expressions share one semantic renderer. TeX requests
native-selected additive terms only when the ordinary body exceeds its line
budget. For wide complex matrices, it requests native numeric components and
arranges them as real and imaginary matrices. Neither operation reparses numeric
or TeX strings. Each traversal has independent memoisation and cycle detection;
shared children render once. Native formatting and temporary-name scopes remain
thread-local. Recognised numeric constants use retained constant IDs directly,
with a perfect-hash candidate lookup and full-precision equality for untagged
values. Parsing, evaluation and solving remain native mathematical operations.

`TeX_bound` composes a braced body with optional bindings, `TeX_scaled`
places a scalar factor beside a body, and `TeX_cartesian_sum` aligns real
and imaginary components. `TeX_symmetric_pair` composes a common centre plus or minus an offset,
with optional offset grouping and a common denominator. These constructors borrow
their inputs and return independent values released with `TeX_free`.

## Converting native expression output

Pass the `string_t` returned by `expr_to_text(expression, style_EXPRESSION)`
directly to `TeX_from_expression`. The same native parser accepts braces,
variable and constant bindings, array bindings, Unicode operators, indexed
identifiers, registered functions and domain conditions. The rendered result
contains the symbolic body and its conditions, not the binding declarations;
bound values are not substituted into ordinary algebra.

This is a conversion of the emitted mathematics, not recovery of its history.
If the expression renderer has already evaluated a Fourier transform, the TeX
adapter renders that result. Pass the original transform request to retain its
operator notation, as in the runnable calculus example above.

```c
/**
 * @file TeX_expression_style_example.c
 * @brief Convert native expression output into independently owned TeX notation.
 *
 * The input includes an array binding and a constant binding. The symbolic body
 * remains symbolic; binding values are not substituted into the rendered formula.
 */
#include <stdio.h>
#include <stdlib.h>

#include "expression-t.h"
#include "TeX-t.h"

int main(void)
{
    expr_t *expression = expr_from_string("{x^2+c | x=[1,2,3]; c=4}", NULL);
    string_t *source = expression ? expr_to_text(expression, style_EXPRESSION) : NULL;
    TeX_t *notation = source ? TeX_from_expression(source) : NULL;
    int status = EXIT_FAILURE;
    if (source && notation && puts(string_c_str(source)) != EOF && puts(TeX_source(notation)) != EOF)
        status = EXIT_SUCCESS;
    TeX_free(notation);
    string_free(source);
    expr_free(expression);
    if (status != EXIT_SUCCESS)
        fputs("Could not convert native expression output.\n", stderr);
    return status;
}
```

Output:

```text
{ x² + c | x = [1, 2, 3]; c = 4 }
x^{2} + c
```

## Text-based alternatives to structured composition

The following complete programme repeats the root, indexed expression and
projection matrix examples using complete mathematical strings.
It also renders the one-dimensional Schrödinger equation from a single string.
Here `h` denotes the reduced Planck constant, `psi` depends on
`x` and `t`, and `V` is the scalar potential. Their arguments are suppressed
in the displayed equation. The native derivative syntax retains differentiation.

The same examples now use an array of native mathematical strings and one loop
calling `TeX_from_expression`. Symbolic indices use `r_n` or `r_(n+1)`;
this is scalar indexing, not tensor syntax. Whole equations retain their two
sides, and explicit scalar–matrix products retain the matrix layout. Undeclared
function calls on equation sides, such as `P(x)`, remain symbolic; write `P*x`
when multiplication is intended. Captions remain separate literal text.

Mathematical interpretation belongs to the native modules. The equation adapter
asks an operand-reader callback to interpret each side, and TeX supplies the
composition facilities. The algebraic beautifier may reorder equivalent terms.
Tensor syntax remains deferred.

```c
/**
 * @file TeX_text_alternatives_example.c
 * @brief Render native mathematical strings with one convenience call per formula.
 *
 * Native parsers interpret indices, equations, scalar–matrix products and calculus.
 * Captions remain separate literal text. Here h denotes the reduced Planck constant.
 */
#include <stdio.h>
#include <stdlib.h>

#include "TeX-t.h"

int main(void)
{
    const char *formulae[] = {
        "sqrt(x/2)",
        "r_(n) = root(1+x_(n)^2,3)/sqrt(1+@lambda^2)",
        "P(x) = 1/(1+x^2)*(1,x;x,x^2)",
        "i*h*Dt(@psi) = -h^2/(2*m)*Dxx(@psi)+V*@psi"
    };
    const char *captions[] = {NULL, "scaled & normalised", NULL, NULL};
    for (size_t i = 0; i < sizeof formulae / sizeof *formulae; ++i) {
        string_t *input = string_new_with(formulae[i]);
        TeX_t *notation = input ? TeX_from_expression(input) : NULL;
        string_free(input);
        bool ok = notation && puts(TeX_source(notation)) != EOF;
        TeX_free(notation);
        if (ok && captions[i])
            ok = puts(captions[i]) != EOF;
        if (!ok) {
            fputs("Could not render mathematical text.\n", stderr);
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
```

Output:

```text
\sqrt{\frac{x}{2}}
r_{n} = \frac{\sqrt[3]{x_{n}^{2} + 1}}{\sqrt{\lambda^{2} + 1}}
scaled & normalised
P\left(x\right) = \frac{1}{x^{2} + 1}\mkern-5mu \begin{bmatrix}1 & x \\[10pt] x & x^{2}\end{bmatrix}
i\mkern-2mu h\mkern-2mu \frac{\partial}{\partial t}\left[\psi\right] = -\frac{h^{2}}{2\mkern-2mu m}\mkern-2mu \frac{\partial^{2}}{\partial x^{2}}\left[\psi\right] + V\mkern-2mu \psi
```

Rendered mathematics, in the same order (the caption is separate text):

$\quad\begin{array}{l}\displaystyle \sqrt{\frac{x}{2}}\end{array}$

$\quad\begin{array}{l}\displaystyle r_{n} = \frac{\sqrt[3]{x_{n}^{2} + 1}}{\sqrt{\lambda^{2} + 1}}\end{array}$

$\quad\begin{array}{l}\displaystyle P\left(x\right) = \frac{1}{x^{2} + 1}\mkern-5mu \begin{bmatrix}1 & x \\[10pt] x & x^{2}\end{bmatrix}\end{array}$

$\quad\begin{array}{l}\displaystyle i\mkern-2mu h\mkern-2mu \frac{\partial}{\partial t}\left[\psi\right] = -\frac{h^{2}}{2\mkern-2mu m}\mkern-2mu \frac{\partial^{2}}{\partial x^{2}}\left[\psi\right] + V\mkern-2mu \psi\end{array}$

## Native constants and infinity

The convenience API uses the native aliases: `@inf` and `@infinity` render
as infinity, while `@eulermascheroni` and `@gamma` render as Euler–Mascheroni
γ. `@pi`, `@phi` and `@tau` retain their Greek notation. Numerical meanings
and symbol bindings follow the expression parser's rules; a Greek symbol alone
is not a new numerical-constant definition. Euler's number and the imaginary
unit use the existing spellings `e` and `i`, not `@e` or `@i`.

```c
/**
 * @file TeX_constants_example.c
 * @brief Render native constant aliases and an infinite integral bound.
 *
 * Uses the same parser as ordinary expressions, retaining calculus notation.
 * Every returned notation value and input string is released after use.
 */
#include <stdio.h>
#include <stdlib.h>

#include "TeX-t.h"

int main(void)
{
    const char *sources[] = {
        "@inf", "@infinity", "-@inf", "@eulermascheroni", "@gamma",
        "@pi", "@phi", "@tau", "e", "i", "@S_0^(@inf) exp(-x) dx"
    };
    for (size_t i = 0; i < sizeof sources / sizeof *sources; ++i) {
        string_t *input = string_new_with(sources[i]);
        TeX_t *notation = input ? TeX_from_expression(input) : NULL;
        string_free(input);
        int printed = notation ? puts(TeX_source(notation)) : EOF;
        TeX_free(notation);
        if (printed == EOF) {
            fputs("Could not render a constant alias.\n", stderr);
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
```

Output:

```text
\infty
\infty
-\infty
\gamma
\gamma
\pi
\phi
\tau
e
i
\int_{0}^{\infty} e^{-x}\, dx
```

Rendered mathematics, in the same order:

$\quad\begin{array}{l}\displaystyle \infty,\quad \infty,\quad -\infty,\quad \gamma,\quad \gamma,\quad \pi,\quad \phi,\quad \tau,\quad e,\quad i,\quad \int_{0}^{\infty} e^{-x}\, dx\end{array}$

## Matrix literals and symbolic powers

`TeX_from_expression` also accepts `(1, 2; 3, 4)^x`. The literal must contain
an explicit column comma or row semicolon so it is distinguishable from scalar
parentheses. Matrix entries are parsed by `mat_from_text_expr`; the resulting
body uses the matrix module's square-bracket notation. A compound exponent must
be parenthesised, as in `(1, 2; 3, 4)^(x+1)`.

The power is displayed without evaluating the matrix operation: `^2` remains
a superscript rather than multiplying the matrix. Scalar entries retain the
native matrix parser's normal simplification. This convenience form accepts a
single literal with an optional scalar exponent and outer scalar bindings, not
general matrix arithmetic. Thus `(1, 2; 3, 4)^x+1` is rejected rather than
silently treating `x+1` as its exponent. Ordinary scalar parentheses retain their
existing meaning.

```c
/**
 * @file TeX_matrix_power_example.c
 * @brief Render a matrix literal raised to a symbolic power without evaluating it.
 *
 * Matrix entries and the scalar exponent use their native parsers. The returned
 * notation owns its contents independently of the input string.
 */
#include <stdio.h>
#include <stdlib.h>

#include "TeX-t.h"

int main(void)
{
    string_t *input = string_new_with("(1, 2; 3, 4)^x");
    TeX_t *notation = input ? TeX_from_expression(input) : NULL;
    string_free(input);
    int printed = notation ? puts(TeX_source(notation)) : EOF;
    TeX_free(notation);
    if (printed == EOF) {
        fputs("Could not render the matrix power.\n", stderr);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
```

Output:

```text
{\begin{bmatrix}1 & 2 \\[10pt] 3 & 4\end{bmatrix}}^{x}
```

Rendered mathematics:

$\quad\begin{array}{l}\displaystyle {\begin{bmatrix}1 & 2 \\[10pt] 3 & 4\end{bmatrix}}^{x}\end{array}$

## Bound matrix powers and differential-equation problems

The same convenience API accepts outer bindings on a matrix literal or power.
Bindings are parsed by the native matrix and expression parsers, but symbolic
entries and exponents stay symbolic: `x = 3` does not replace the exponent `x`.
This also applies to a lone scalar expression such as `{ x | x = 3 }`.

Differential-equation inputs use `de_from_text` and `de_problem_to_TeX`.
The condition-only form below infers the independent variable `x` and treats the
residual as an equation equal to zero. Canonical `style_EXPRESSION` problem
strings with independent-variable, constant and condition sections are also
accepted. Initial and boundary conditions remain visible as evaluation bars;
no solver is invoked. The existing derivative renderer uses ordinary derivative
fractions for this ODE rather than prime notation.

```c
/**
 * @file TeX_bound_problems_example.c
 * @brief Render a bound matrix power and an ODE with initial conditions.
 *
 * Scalar bindings remain symbolic. Initial conditions are displayed explicitly,
 * without solving the differential equation or evaluating the matrix power.
 */
#include <stdio.h>
#include <stdlib.h>

#include "TeX-t.h"

int main(void)
{
    const char *sources[] = {
        "{ (1, 2; 3, 4)^x | x = 3 }",
        "{ y'' + 3y' + 40y | y'(0) = 1, y(0) = 0 }"
    };
    for (size_t i = 0; i < sizeof sources / sizeof *sources; ++i) {
        string_t *input = string_new_with(sources[i]);
        TeX_t *notation = input ? TeX_from_expression(input) : NULL;
        string_free(input);
        int printed = notation ? puts(TeX_source(notation)) : EOF;
        TeX_free(notation);
        if (printed == EOF) {
            fputs("Could not render the bound problem.\n", stderr);
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
```

Output:

```text
{\begin{bmatrix}1 & 2 \\[10pt] 3 & 4\end{bmatrix}}^{x}
\frac{d^{2} y}{d x^{2}} + 3\mkern-2mu \frac{d y}{d x} + 40\mkern-2mu y = 0,\qquad \left. \frac{d y}{d x}\right|_{x=0} = 1,\qquad \left. y\right|_{x=0} = 0
```

Rendered mathematics:

$\quad\begin{array}{l}\displaystyle {\begin{bmatrix}1 & 2 \\[10pt] 3 & 4\end{bmatrix}}^{x}\end{array}$

$\quad\begin{array}{l}\displaystyle \frac{d^{2} y}{d x^{2}} + 3\mkern-2mu \frac{d y}{d x} + 40\mkern-2mu y = 0,\qquad \left. \frac{d y}{d x}\right|_{x=0} = 1,\qquad \left. y\right|_{x=0} = 0\end{array}$

## Native layout composition

These layouts are also used by the native matrix and equation renderers.
The same builder can be reused immediately after each completed value is transferred.

```c
/**
 * @file TeX_native_layout_example.c
 * @brief Compose binding, scalar, complex-component and symmetric-pair layouts.
 *
 * Reuses an RPN builder, transferring and freeing one completed value at a time.
 */
#include <stdio.h>
#include <stdlib.h>

#include "TeX-t.h"

static bool print_layout(TeX_builder_t *builder)
{
    TeX_t *layout = TeX_builder_finish(builder);
    bool ok = layout && puts(TeX_source(layout)) != EOF;
    TeX_free(layout);
    return ok;
}

int main(void)
{
    TeX_builder_t *b = TeX_builder_new();
    int status = EXIT_FAILURE;
    TeX_push_symbol(b, "x");
    TeX_push_symbol(b, "x");
    TeX_push_integer(b, 1);
    TeX_equation(b, TeX_EQUATION_INLINE);
    TeX_bound(b, true);
    if (!print_layout(b)) goto done;
    TeX_push_integer(b, 2);
    TeX_push_symbol(b, "x");
    TeX_scaled(b);
    if (!print_layout(b)) goto done;
    TeX_push_symbol(b, "x");
    TeX_push_symbol(b, "y");
    TeX_cartesian_sum(b);
    if (!print_layout(b)) goto done;
    TeX_push_symbol(b, "x");
    TeX_push_symbol(b, "y");
    TeX_push_integer(b, 2);
    TeX_symmetric_pair(b, true, true);
    if (!print_layout(b)) goto done;
    status = EXIT_SUCCESS;
done:
    TeX_builder_free(b);
    if (status != EXIT_SUCCESS)
        fputs("Could not compose native mathematical layouts.\n", stderr);
    return status;
}
```

Output:

```text
\left\{ x \;\middle|\; x = 1 \right\}
2\mkern-5mu x
\begin{aligned}&x \\[10pt] &{}+ i\mkern-2mu y\end{aligned}
\frac{x \pm \left(y\right)}{2}
```

Rendered:

$\quad\begin{array}{l}\displaystyle \left\{ x \;\middle|\; x = 1 \right\}\end{array}$

$\quad\begin{array}{l}\displaystyle 2\mkern-5mu x\end{array}$

$\quad\begin{array}{l}\displaystyle \begin{aligned}&x \\[10pt] &{}+ i\mkern-2mu y\end{aligned}\end{array}$

$\quad\begin{array}{l}\displaystyle \frac{x \pm \left(y\right)}{2}\end{array}$

## Reverse Polish construction contract

The reverse Polish design uses `TeX_push_…` followed by concise operations such
as `TeX_add`, `TeX_mul`, `TeX_frac` and `TeX_pow` as the public construction
interface. The former fragment constructors are private TeX
implementation details. Each operation consumes the stated tail of the stack,
in push order, replacing it with one independently owned fragment. Stack
selection uses direct indexing. ROLL shifts only the selected pointer range;
ROT moves three pointers in constant time. Neither operation copies notation.

| Purpose | API and operand order, oldest first |
| --- | --- |
| Lifetime | `TeX_builder_new`, `TeX_builder_free`; `TeX_builder_finish` transfers exactly one value |
| Inspection | `TeX_builder_count`, `TeX_builder_error`, `TeX_builder_peek` with level one at the top |
| Recovery | `TeX_builder_clear_error` retains operands; `TeX_builder_clear` discards operands and resets errors |
| Symbols, integers | `TeX_push_symbol` takes ASCII names such as psi, mu or hbar; `TeX_push_integer` takes a long |
| Trusted renderer output | `TeX_push_raw` embeds existing source; it is unnecessary for structured mathematics |
| Native mathematics | `TeX_push_expression` takes `string_t`; `TeX_push_math` takes a C string |
| Text and existing notation | `TeX_push_text` escapes prose; `TeX_push_notation` copies notation |
| Spacing | `TeX_push_space` selects `TeX_SPACE_THIN` or `TeX_SPACE_QUAD` |
| Stack operations | `TeX_dup`: a → a a; `TeX_rot`: a b c → b c a; push a level number then `TeX_roll` |
| Arithmetic | `TeX_add`, `TeX_sub`, `TeX_mul`: left, right; `TeX_neg`: body |
| Function calls | `TeX_function(name, count)` consumes arguments; `TeX_call(count)` consumes function, then arguments |
| Fractions and roots | `TeX_frac`: numerator, denominator; `TeX_sqrt`: body; `TeX_root`: body, index |
| Scripts | `TeX_pow`: base, exponent; `TeX_subscript`: base, lower; `TeX_scripts`: base, lower, upper |
| Presentation sequences | `TeX_sequence(count)` concatenates already constructed presentation items |
| Equations and delimiters | `TeX_equation(style)`: left, right; `TeX_fence(fence)`: body |
| Tensors | `TeX_tensor(positions, count)`: base followed by ordered index values |
| Sums | `TeX_sum(limits)`: body followed by selected lower and/or upper bounds |
| Integrals | `TeX_integral(limits)`: body, variable, then selected bounds |
| Derivatives | `TeX_derivative(order, style)`: body, variable |
| Matrices | `TeX_matrix(rows, columns, style, row_spacing)`: row-major cells |
| Bindings and scalar layout | `TeX_bound(has_bindings)`: body, optional bindings; `TeX_scaled`: factor, body |
| Component and pair layouts | `TeX_cartesian_sum`: real, imaginary; `TeX_symmetric_pair`: centre, offset, optional denominator |

Pushes copy their inputs; the caller retains ownership of strings and existing
notation. The builder owns all pushed and intermediate values. A borrowed
`TeX_builder_peek` result must not be freed; it remains valid through further
pushes, but not after consumption, clearing or builder destruction.
`TeX_builder_finish` transfers ownership without copying. Free its result with
`TeX_free`; the builder is then empty and immediately reusable.

All 24 Greek names are available in lowercase and with an initial capital for
uppercase letters, together with `varepsilon`, `vartheta`, `varkappa`, `varpi`,
`varrho`, `varsigma` and `varphi`. `hbar`, `partial`, `infinity` and `infty` select
common mathematical symbols. These use a compact perfect hash rather than a scan.
Other identifier names remain available. `TeX_function(b, "psi", 1)` uses
the same ASCII naming; recognised standard functions retain their established
spelling, so `gamma` as a function denotes the gamma function while
`TeX_push_symbol(b, "gamma")` denotes the Greek letter. No special keyboard is needed.

Failures preserve the previous operands and save the first error:
`ERANGE` for stack underflow, `EINVAL` for invalid input or incomplete finish,
`EOVERFLOW` for dimensions that cannot fit and `ENOMEM` for allocation failure.
Later mutations and finish remain blocked until `TeX_builder_clear_error` or
`TeX_builder_clear` is called. Inspecting the stack remains possible after an
error. Invalid peek requests report `errno` without changing the saved error.

Structured arithmetic retains operand precedence without parsing or simplifying
algebra. Sums and differences used as factors or power bases are visibly grouped;
subtraction preserves the scope of its right operand. Stack duplication and
`TeX_push_notation` retain this structure. Raw source from another renderer has
unknown structure and is conservatively grouped when used in arithmetic. Native
mathematical text continues to obtain its meaning from the expression, matrix,
equation and differential-equation modules through their internal callbacks.
TeX supplies presentation; it does not parse TeX back into mathematics.

## HP48-style stack operations

Levels count down from one at the top, as in the [HP48 user guide, chapter 3](https://www.hp.com/ctg/Manual/c00442262.pdf).
`TeX_dup` copies the top value. `TeX_rot` rotates the top three values;
it has the same effect as pushing `3` and calling `TeX_roll`.
ROLL consumes its level number, then moves that level of the remaining data
to the top. With the top at the right, `a b c 3 ROLL` becomes `b c a`.
Push `2` then ROLL to exchange the top two values. Lower levels are untouched.

Push a ROLL control with `TeX_push_integer`; DUP and `TeX_push_notation`
preserve that literal integer. The supported level range is one through the
remaining stack depth. ROLL neither evaluates arithmetic notation nor parses
rendered TeX to obtain the level. An invalid control leaves the entire stack,
including the control, intact and saves an error for inspection or recovery.

This programme demonstrates DUP, ROT and a four-level ROLL. Each row shows
the stack from bottom to top; inspecting it does not transfer ownership.

```c
/**
 * @file TeX_stack_example.c
 * @brief Demonstrate HP48-style stack manipulation without exposing notation ownership.
 *
 * Prints stack levels in push order after DUP, ROT and a level-controlled ROLL.
 */
#include <stdio.h>
#include <stdlib.h>

#include "TeX-t.h"

static int show(const TeX_builder_t *builder)
{
    if (TeX_builder_error(builder))
        return -1;
    for (size_t level = TeX_builder_count(builder); level; --level) {
        const TeX_t *value = TeX_builder_peek(builder, level);
        if (!value || printf("%s%s", TeX_source(value), level == 1 ? "\n" : " ") < 0)
            return -1;
    }
    return 0;
}

int main(void)
{
    TeX_builder_t *builder = TeX_builder_new();
    TeX_push_integer(builder, 12);
    TeX_push_integer(builder, 34);
    TeX_dup(builder);
    int status = show(builder) ? EXIT_FAILURE : EXIT_SUCCESS;

    TeX_builder_clear(builder);
    TeX_push_integer(builder, 12);
    TeX_push_integer(builder, 34);
    TeX_push_integer(builder, 56);
    TeX_rot(builder);
    if (show(builder))
        status = EXIT_FAILURE;

    TeX_builder_clear(builder);
    TeX_push_integer(builder, 555);
    TeX_push_integer(builder, 444);
    TeX_push_integer(builder, 333);
    TeX_push_integer(builder, 222);
    TeX_push_integer(builder, 4);
    TeX_roll(builder);
    if (show(builder))
        status = EXIT_FAILURE;
    TeX_builder_free(builder);
    return status;
}
```

Output:

```text
12 34 34
34 56 12
444 333 222 555
```
