# TeX notation

`TeX_t`, declared in `TeX-t.h`, is an opaque, immutable mathematical notation
value. It separates typesetting syntax from algebra and application code.
Structured constructors emit notation without simplifying expressions.
`TeX_from_expression` additionally delegates ordinary mathematical input to the
expression module for parsing, simplification and readable arrangement. No
constructor invokes a TeX engine.

`TeX_fraction`, `TeX_root`, `TeX_scripts`, `TeX_sequence`, `TeX_equation` and `TeX_matrix`
compose borrowed values into independent owned values. Inputs can be freed
immediately afterwards. `TeX_scripts` groups the complete base before adding
subscripts or superscripts. Matrices accept row-major cells, bracketed,
parenthesised or plain styles, and extra row spacing in points; null cells are
empty entries. Traversal is proportional to the notation being emitted.

`TeX_equation(left, right, style)` joins borrowed mathematical sides and returns
independently owned notation. `TeX_EQUATION_INLINE` inserts an equals sign;
`TeX_EQUATION_ALIGNED` uses a top-aligned equation and incorporates a native
wrapped right side without nesting its alignment environment. It neither breaks
lines nor changes either side's mathematics. Invalid sides or styles return null.

`TeX_text(const string_t *text)` borrows literal UTF-8 text without modifying or
retaining it and returns independently owned escaped notation. It uses the
string cursor API and copies ordinary text in spans, with direct ASCII-indexed
escape lookup. Empty strings are valid; null input and embedded NUL characters
are rejected with `EINVAL` rather than silently truncating text.
`TeX_raw` is exclusively for trusted TeX,
including output from existing native renderers; it neither validates nor
sanitises commands. `TeX_source` borrows the resulting source, while `TeX_render`
returns an independent `string_t`. Free values with `TeX_free` and rendered
strings with `string_free`. Constructors return null on invalid arguments or
allocation failure. Read-only sharing is supported, but destruction must be
synchronised by the caller.

## Composition example

Each C block below is a complete programme with its own `main`, headers,
failure reporting and cleanup. The Markdown example tests extract and compile
these exact blocks, run them and compare their output with the accompanying
text. They are registered with `make -j1 test-readme-examples`, which runs after
the ordinary suites in `make -j1 test`.
When invoking the example runner from the repository root, use
`MARS_ROOT="$PWD" make -j1 test-readme-examples` so its temporary working
directories do not affect executable lookup. A successful TeX-only run reports
`README programs: 12 checked, 0 failed`; other enabled examples are counted too.

```c
/**
 * @file TeX_example_1.c
 * @brief Standalone demonstration of mathematical TeX composition and ownership.
 *
 * Prints TeX source, releases owned values and reports construction or output failure.
 */
#include <stdio.h>
#include <stdlib.h>

#include "TeX-t.h"

int main(void)
{
    int status = EXIT_FAILURE;
    TeX_t *x = TeX_raw("x");
    TeX_t *two = TeX_raw("2");
    TeX_t *fraction = TeX_fraction(x, two);
    TeX_t *root = TeX_root(fraction, NULL);
    if (x && two && fraction && root)
        status = puts(TeX_source(root)) == EOF ? EXIT_FAILURE : EXIT_SUCCESS;
    TeX_free(root);
    TeX_free(fraction);
    TeX_free(two);
    TeX_free(x);
    if (status != EXIT_SUCCESS)
        fputs("Could not construct or print TeX.\n", stderr);
    return status;
}
```

Output:

```text
\sqrt{\frac{x}{2}}
```


## Indexed expression with nested roots

This combines subscripts and superscripts, sequences, an indexed cube root, a
square root, a fraction and escaped literal text. Only trusted mathematical
symbols and punctuation use `TeX_raw`; the caption goes through `TeX_text`.

```c
/**
 * @file TeX_example_2.c
 * @brief Standalone demonstration of mathematical TeX composition and ownership.
 *
 * Prints TeX source, releases owned values and reports construction or output failure.
 */
#include <stdio.h>
#include <stdlib.h>

#include "TeX-t.h"

int main(void)
{
    int status = EXIT_FAILURE;
    string_t *caption = string_new_with("scaled & normalised");
    TeX_t *label = TeX_text(caption);
    string_free(caption); /* TeX_text copied the borrowed string. */

    TeX_t *r = TeX_raw("r");
    TeX_t *x = TeX_raw("x");
    TeX_t *n = TeX_raw("n");
    TeX_t *two = TeX_raw("2");
    TeX_t *three = TeX_raw("3");
    TeX_t *lambda = TeX_raw("\\lambda");
    TeX_t *one_plus = TeX_raw("1 + ");
    TeX_t *equals = TeX_raw(" = ");
    TeX_t *gap = TeX_raw("\\qquad ");
    TeX_t *r_n = TeX_scripts(r, n, NULL);
    TeX_t *x_n_squared = TeX_scripts(x, n, two);
    TeX_t *lambda_squared = TeX_scripts(lambda, NULL, two);

    const TeX_t *top_parts[] = {one_plus, x_n_squared};
    const TeX_t *bottom_parts[] = {one_plus, lambda_squared};
    TeX_t *top_sum = TeX_sequence(top_parts, 2);
    TeX_t *bottom_sum = TeX_sequence(bottom_parts, 2);
    TeX_t *cube_root = TeX_root(top_sum, three);
    TeX_t *square_root = TeX_root(bottom_sum, NULL);
    TeX_t *ratio = TeX_fraction(cube_root, square_root);
    const TeX_t *parts[] = {r_n, equals, ratio, gap, label};
    TeX_t *formula = TeX_sequence(parts, 5);
    string_t *output = TeX_render(formula);
    int complete = label && r && x && n && two && three && lambda && one_plus && equals && gap &&
                   r_n && x_n_squared && lambda_squared && top_sum && bottom_sum && cube_root &&
                   square_root && ratio && formula && output;
    TeX_free(formula); /* The rendered string remains independently owned. */
    if (complete)
        status = puts(string_c_str(output)) == EOF ? EXIT_FAILURE : EXIT_SUCCESS;
    string_free(output);

    TeX_free(ratio);
    TeX_free(square_root);
    TeX_free(cube_root);
    TeX_free(bottom_sum);
    TeX_free(top_sum);
    TeX_free(lambda_squared);
    TeX_free(x_n_squared);
    TeX_free(r_n);
    TeX_free(gap);
    TeX_free(equals);
    TeX_free(one_plus);
    TeX_free(lambda);
    TeX_free(three);
    TeX_free(two);
    TeX_free(n);
    TeX_free(x);
    TeX_free(r);
    TeX_free(label);
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

For real $x$, this constructs the projection onto the direction $(1,x)$.
The caller supplies the mathematics; the TeX module supplies the fraction,
power and matrix syntax. Cells are row-major and the extra row spacing is six
points. A repeated cell is borrowed twice but its original value is freed once.

```c
/**
 * @file TeX_example_3.c
 * @brief Standalone demonstration of mathematical TeX composition and ownership.
 *
 * Prints TeX source, releases owned values and reports construction or output failure.
 */
#include <stdio.h>
#include <stdlib.h>

#include "TeX-t.h"

int main(void)
{
    int status = EXIT_FAILURE;
    TeX_t *one = TeX_raw("1");
    TeX_t *x = TeX_raw("x");
    TeX_t *two = TeX_raw("2");
    TeX_t *x_squared = TeX_scripts(x, NULL, two);
    TeX_t *one_plus = TeX_raw("1 + ");
    const TeX_t *denominator_parts[] = {one_plus, x_squared};
    TeX_t *denominator = TeX_sequence(denominator_parts, 2);
    TeX_t *scale = TeX_fraction(one, denominator);

    /* A symmetric projection matrix: repeated cells may share a borrowed value. */
    const TeX_t *cells[] = {one, x, x, x_squared};
    TeX_t *matrix = TeX_matrix(cells, 2, 2, TeX_MATRIX_BRACKETS, 6);
    TeX_t *heading = TeX_raw("P(x) = ");
    TeX_t *gap = TeX_raw("\\,");
    const TeX_t *parts[] = {heading, scale, gap, matrix};
    TeX_t *formula = TeX_sequence(parts, 4);

    int complete = one && x && two && x_squared && one_plus && denominator && scale && matrix &&
                   heading && gap && formula;
    /* Composition copied these values; the completed formula survives their release. */
    TeX_free(matrix);
    TeX_free(scale);
    TeX_free(denominator);
    TeX_free(one_plus);
    TeX_free(x_squared);
    TeX_free(two);
    TeX_free(x);
    TeX_free(one);
    TeX_free(gap);
    TeX_free(heading);
    if (complete)
        status = puts(TeX_source(formula)) == EOF ? EXIT_FAILURE : EXIT_SUCCESS;
    TeX_free(formula);
    if (status != EXIT_SUCCESS)
        fputs("Could not construct or print TeX.\n", stderr);
    return status;
}
```

Output:

```text
P(x) = \frac{1}{1 + {x}^{2}}\,\begin{bmatrix}1 & x \\[6pt] x & {x}^{2}\end{bmatrix}
```

Rendered mathematics:

$\quad\begin{array}{l}\displaystyle P(x) = \frac{1}{1 + {x}^{2}}\,\begin{bmatrix}1 & x \\[6pt] x & {x}^{2}\end{bmatrix}\end{array}$

## Calculus and tensor notation

`TeX_sum` and `TeX_integral` accept optional lower and upper bounds. Both bounds
may be null for an unbounded sum or an indefinite integral. Nest integral
constructors for iterated integrals. `TeX_derivative` emits an ordinary or
partial derivative of positive integer order. Keep coefficients alongside
that derivative's fraction, rather than enclosing it in another fraction.
`TeX_fenced` supplies automatically sized parentheses, brackets or absolute-value
bars.

`TeX_tensor` accepts ordered upper and lower index slots. Unlike placing a
subscript and superscript on the same base with `TeX_scripts`, it preserves
horizontal slot order when variance alternates. The indices are notation only:
this module does not perform tensor contractions, check dimensions or calculate
derivatives, integrals or sums. Every new constructor returns an independently
owned value requiring `TeX_free`; its inputs are borrowed and copied.

## Einstein, Schrödinger and Dirac equations

The following single runnable programme prints five expressions:

- Einstein's field equations with the cosmological constant and explicit $c$.
- The time-dependent Schrödinger equation in one spatial dimension for a scalar potential.
- The free Dirac equation in natural units, $\hbar=c=1$, with Minkowski signature
  $(+,-,-,-)$. Its repeated $\mu$ index is summed over spacetime components;
  the gamma symbols represent matrices acting on the spinor.
- A wavefunction expanded in a complete orthonormal discrete basis.
- Normalisation of that wavefunction over the real line.

The equations follow the conventions described in David Tong's lectures on
[general relativity](https://www.damtp.cam.ac.uk/user/tong/gr/grhtml/S4.html),
[quantum mechanics](https://www.damtp.cam.ac.uk/user/tong/qm/qmhtml/S1.html) and
[the Dirac equation](https://www.damtp.cam.ac.uk/user/tong/qft/qfthtml/S4.html).
The gravitational equation restores the explicit factor of $c^{-4}$.

The programme's small ownership list keeps intermediate fragments alive until
cleanup. `keep` records construction failures even when a later constructor
would permit a null optional argument; no incomplete equation is printed.
Trusted raw fragments supply symbols and punctuation, while the structured API
supplies tensor indices, powers, fractions, derivatives, sums, integrals and
delimiters. No extra tensor package is required; the existing matrix examples
use the standard `amsmath` environments.

```c
/**
 * @file TeX_physics.c
 * @brief Runnable field-equation and wavefunction notation examples.
 *
 * Builds mathematical notation with the public TeX API. A small ownership list
 * releases every constructed value once, including intermediate fragments.
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "TeX-t.h"

typedef struct {
    TeX_t *owned[128];
    size_t count;
    bool failed;
} example_t;

/* Adopt a result for cleanup, recording any failed construction. */
static TeX_t *keep(example_t *example, TeX_t *value)
{
    if (!value || example->count == sizeof example->owned / sizeof *example->owned) {
        TeX_free(value);
        example->failed = true;
        return NULL;
    }
    example->owned[example->count++] = value;
    return value;
}

/* Copy a trusted mathematical atom and register its ownership. */
static TeX_t *atom(example_t *example, const char *source)
{
    return keep(example, TeX_raw(source));
}

int main(void)
{
    example_t example = {0};
    TeX_t *one = atom(&example, "1"), *two = atom(&example, "2"), *four = atom(&example, "4");
    TeX_t *plus = atom(&example, " + "), *minus = atom(&example, " - ");
    TeX_t *equals = atom(&example, " = "), *space = atom(&example, "\\,");
    TeX_t *mu = atom(&example, "\\mu"), *nu = atom(&example, "\\nu");
    TeX_t *ricci = atom(&example, "R"), *metric = atom(&example, "g"), *stress = atom(&example, "T");
    TeX_t *c = atom(&example, "c"), *lambda = atom(&example, "\\Lambda");
    TeX_t *coupling = atom(&example, "8\\pi G");

    /* Einstein equations, with c explicit and a cosmological constant. */
    const TeX_index_t lower_pair[] = {{mu, TeX_INDEX_LOWER}, {nu, TeX_INDEX_LOWER}};
    TeX_t *r_mn = keep(&example, TeX_tensor(ricci, lower_pair, 2));
    TeX_t *g_mn = keep(&example, TeX_tensor(metric, lower_pair, 2));
    TeX_t *t_mn = keep(&example, TeX_tensor(stress, lower_pair, 2));
    TeX_t *half = keep(&example, TeX_fraction(one, two));
    TeX_t *c_fourth = keep(&example, TeX_scripts(c, NULL, four));
    TeX_t *factor = keep(&example, TeX_fraction(coupling, c_fourth));
    const TeX_t *einstein_parts[] = {
        r_mn, minus, half, space, ricci, space, g_mn, plus, lambda, space, g_mn, equals, factor, space, t_mn
    };
    TeX_t *einstein = keep(&example, TeX_sequence(einstein_parts, 15));

    /* One-dimensional time-dependent Schrodinger equation for a scalar potential. */
    TeX_t *psi = atom(&example, "\\psi(x,t)");
    TeX_t *x = atom(&example, "x"), *t = atom(&example, "t");
    TeX_t *hbar = atom(&example, "\\hbar"), *imaginary_hbar = atom(&example, "i\\hbar");
    TeX_t *two_m = atom(&example, "2m"), *potential = atom(&example, "V(x,t)");
    TeX_t *negative = atom(&example, "-");
    TeX_t *time_derivative = keep(&example, TeX_derivative(psi, t, 1, TeX_DERIVATIVE_PARTIAL));
    TeX_t *space_derivative = keep(&example, TeX_derivative(psi, x, 2, TeX_DERIVATIVE_PARTIAL));
    TeX_t *hbar_squared = keep(&example, TeX_scripts(hbar, NULL, two));
    TeX_t *kinetic = keep(&example, TeX_fraction(hbar_squared, two_m));
    const TeX_t *schrodinger_parts[] = {
        imaginary_hbar, space, time_derivative, equals, negative, kinetic, space, space_derivative,
        plus, potential, space, psi
    };
    TeX_t *schrodinger = keep(&example, TeX_sequence(schrodinger_parts, 12));

    /* Free Dirac equation in natural units: hbar = c = 1. */
    TeX_t *gamma = atom(&example, "\\gamma"), *partial = atom(&example, "\\partial");
    TeX_t *imaginary = atom(&example, "i"), *mass = atom(&example, "m");
    TeX_t *spinor = atom(&example, "\\psi"), *zero = atom(&example, "0");
    const TeX_index_t upper_mu[] = {{mu, TeX_INDEX_UPPER}};
    const TeX_index_t lower_mu[] = {{mu, TeX_INDEX_LOWER}};
    TeX_t *gamma_mu = keep(&example, TeX_tensor(gamma, upper_mu, 1));
    TeX_t *partial_mu = keep(&example, TeX_tensor(partial, lower_mu, 1));
    const TeX_t *dirac_operator_parts[] = {imaginary, space, gamma_mu, space, partial_mu, minus, mass};
    TeX_t *dirac_operator = keep(&example, TeX_sequence(dirac_operator_parts, 7));
    TeX_t *group = keep(&example, TeX_fenced(dirac_operator, TeX_FENCE_PARENTHESES));
    const TeX_t *dirac_parts[] = {group, space, spinor, equals, zero};
    TeX_t *dirac = keep(&example, TeX_sequence(dirac_parts, 5));

    /* Expansion in an orthonormal discrete basis, with normalisation on the real line. */
    TeX_t *n = atom(&example, "n"), *coefficient = atom(&example, "a");
    TeX_t *phi = atom(&example, "\\phi"), *argument = atom(&example, "(x)");
    TeX_t *wave = atom(&example, "\\psi(x)");
    TeX_t *lower = atom(&example, "n=0"), *infinity = atom(&example, "\\infty");
    TeX_t *negative_infinity = atom(&example, "-\\infty");
    TeX_t *a_n = keep(&example, TeX_scripts(coefficient, n, NULL));
    TeX_t *phi_n = keep(&example, TeX_scripts(phi, n, NULL));
    const TeX_t *summand_parts[] = {a_n, space, phi_n, argument};
    TeX_t *summand = keep(&example, TeX_sequence(summand_parts, 4));
    TeX_t *sum = keep(&example, TeX_sum(summand, lower, infinity));
    const TeX_t *expansion_parts[] = {wave, equals, sum};
    TeX_t *expansion = keep(&example, TeX_sequence(expansion_parts, 3));
    TeX_t *magnitude = keep(&example, TeX_fenced(wave, TeX_FENCE_ABSOLUTE));
    TeX_t *density = keep(&example, TeX_scripts(magnitude, NULL, two));
    TeX_t *integral = keep(&example, TeX_integral(density, x, negative_infinity, infinity));
    const TeX_t *normalisation_parts[] = {integral, equals, one};
    TeX_t *normalisation = keep(&example, TeX_sequence(normalisation_parts, 3));

    int status = EXIT_FAILURE;
    if (!example.failed && puts(TeX_source(einstein)) != EOF && puts(TeX_source(schrodinger)) != EOF &&
        puts(TeX_source(dirac)) != EOF && puts(TeX_source(expansion)) != EOF &&
        puts(TeX_source(normalisation)) != EOF)
        status = EXIT_SUCCESS;
    /* Ownership traversal, not a lookup: every constructed value must be released. */
    for (size_t i = 0; i < example.count; ++i)
        TeX_free(example.owned[i]);
    if (status != EXIT_SUCCESS)
        fputs("Could not construct or print TeX.\n", stderr);
    return status;
}
```

Output:

```text
{R}_{\mu}{}_{\nu} - \frac{1}{2}\,R\,{g}_{\mu}{}_{\nu} + \Lambda\,{g}_{\mu}{}_{\nu} = \frac{8\pi G}{{c}^{4}}\,{T}_{\mu}{}_{\nu}
i\hbar\,\frac{\partial {\psi(x,t)}}{\partial {t}} = -\frac{{\hbar}^{2}}{2m}\,\frac{\partial^{2} {\psi(x,t)}}{\partial {x}^{2}} + V(x,t)\,\psi(x,t)
\left( i\,{\gamma}^{\mu}\,{\partial}_{\mu} - m \right)\,\psi = 0
\psi(x) = \sum_{n=0}^{\infty} {{a}_{n}\,{\phi}_{n}(x)}
\int_{-\infty}^{\infty} {{\left| \psi(x) \right|}^{2}}\,\mathrm{d}{x} = 1
```

Rendered mathematics:

$\quad\begin{array}{l}\displaystyle {R}_{\mu}{}_{\nu} - \frac{1}{2}\,R\,{g}_{\mu}{}_{\nu} + \Lambda\,{g}_{\mu}{}_{\nu} = \frac{8\pi G}{{c}^{4}}\,{T}_{\mu}{}_{\nu}\end{array}$

$\quad\begin{array}{l}\displaystyle i\hbar\,\frac{\partial {\psi(x,t)}}{\partial {t}} = -\frac{{\hbar}^{2}}{2m}\,\frac{\partial^{2} {\psi(x,t)}}{\partial {x}^{2}} + V(x,t)\,\psi(x,t)\end{array}$

$\quad\begin{array}{l}\displaystyle \left( i\,{\gamma}^{\mu}\,{\partial}_{\mu} - m \right)\,\psi = 0\end{array}$

$\quad\begin{array}{l}\displaystyle \psi(x) = \sum_{n=0}^{\infty} {{a}_{n}\,{\phi}_{n}(x)}\end{array}$

$\quad\begin{array}{l}\displaystyle \int_{-\infty}^{\infty} {{\left| \psi(x) \right|}^{2}}\,\mathrm{d}{x} = 1\end{array}$

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

The result can also be supplied to `TeX_matrix`, `TeX_sequence` or any other
structured constructor, just like a manually constructed fragment. `TeX_text`
continues to escape literal text; `TeX_raw` continues to accept trusted TeX.
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

`TeX_bound` composes a braced body with optional bindings, `TeX_scaled` places a
scalar factor beside a body, and `TeX_cartesian_sum` aligns real and imaginary
components. `TeX_symmetric_pair` composes a common centre plus or minus an offset,
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

These constructors are also used by the native matrix and equation renderers.
The operands can be released before the resulting notation is printed.

```c
/**
 * @file TeX_native_layout_example.c
 * @brief Compose binding, scalar, complex-component and symmetric-pair layouts.
 *
 * Demonstrates independent ownership of notation used by native mathematical renderers.
 */
#include <stdio.h>
#include <stdlib.h>

#include "TeX-t.h"

int main(void)
{
    TeX_t *x = TeX_raw("x"), *y = TeX_raw("y"), *two = TeX_raw("2"), *binding = TeX_raw("x = 1");
    TeX_t *layouts[] = {
        TeX_bound(x, binding), TeX_scaled(two, x), TeX_cartesian_sum(x, y),
        TeX_symmetric_pair(x, y, two, true)
    };
    int status = EXIT_SUCCESS;
    TeX_free(binding);
    TeX_free(two);
    TeX_free(y);
    TeX_free(x);
    for (size_t i = 0; i < sizeof layouts / sizeof *layouts; ++i) {
        if (!layouts[i] || puts(TeX_source(layouts[i])) == EOF)
            status = EXIT_FAILURE;
        TeX_free(layouts[i]);
    }
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

## Planned reverse Polish construction

**Design proposal recorded on 29 October 2026; deferred for later implementation.**

Add a stack-based convenience interface with `TeX_push_…` and `TeX_apply_…`
function families. Callers push operands, then apply an operation to the top
operands, replacing them with the resulting notation. This reverse Polish
construction should make the earlier complex examples much shorter, avoiding
separate temporary variables and cleanup calls for every intermediate fragment.
These are proposed API families, not functions available in the current header.

The intended design is:

- Push numbers, symbols, literal text, native mathematical expressions or existing
  notation through appropriately named `TeX_push_…` calls. Copy borrowed inputs
  into construction-owned values, with ownership documented explicitly.
- Apply fractions, roots, powers, indices, equations, sequences, matrices,
  integrals and sums through `TeX_apply_…` calls. TeX supplies the typesetting
  syntax; callers describe the required structure.
- Define operand order and arity explicitly, especially for fractions, indexed
  roots, bounds and row-major matrix cells. Reject stack underflow, invalid
  dimensions and allocation failures without losing the previous valid stack.
- Let one opaque construction object own intermediate fragments. Finalisation
  requires one completed formula and returns an independently owned `TeX_t`,
  released with `TeX_free`. Releasing the construction object cleans up any
  remaining fragments, including after failure.
- Use direct stack indexing and ordinary function dispatch. Keep lifecycle
  names, exact signatures and the error-reporting interface for the later API
  design and implementation.

Preserve the existing module boundary: expression, matrix, equation and
differential equation retain mathematical interpretation and provide semantic
information through their internal callbacks. TeX owns presentation. Ordinary
requests for TeX output continue to manage rendering contexts internally; the
new stack interface is an optional convenience for explicit formula construction.

When this work is implemented, add runnable stack-based alternatives to the
earlier complex examples and their README tests, with matching rendered output
and explicit ownership and failure handling.
