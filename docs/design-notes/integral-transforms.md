# Integral Transform Syntax

> Design note: this syntax is agreed for future Laplace and Fourier transform
> support but is not yet implemented.

Integral transforms use explicit forward and inverse operators:

```text
@L       Laplace transform input alias
@Linv    inverse Laplace transform input alias
@F       Fourier transform input alias
@Finv    inverse Fourier transform input alias
```

The aliases normalise to the Unicode script capitals `ℒ` (U+2112) and `ℱ`
(U+2131). Their inverse forms use the superscript `⁻¹`.

## Parameters

Only the expression is required. The source variable and transform variable
are optional:

```text
operator(expression [, source variable [, transform variable]])
```

For example:

```text
@L(f(x), x)          explicit source variable, default transform variable s
@L(f(x))             inferred source variable x, default transform variable s
@L(f(t), t)          default transform variable s
@Linv(F(s), s)       default transform variable t
@F(f(t), t)          default transform variable ω
@Finv(F(ω), ω)       default transform variable t
@F(f(x), x)          default transform variable k
@Finv(F(k), k)       default transform variable x
@F(f(y), y)          default transform variable m
@Finv(F(m), m)       default transform variable y
@F(f(z), z)          default transform variable n
@Finv(F(n), n)       default transform variable z
```

When present, the second parameter is the variable eliminated by the
transform. The optional third parameter is the variable introduced by it.
Both must be symbolic variables, not general expressions.

When the source variable is omitted, every free binding classified as a
variable is transformed. Bindings classified as constants are retained as
parameters. Inference uses the expression's binding classification rather than
assuming that every letter is a transform variable:

```text
@L(a*x + b)          transform x; retain a and b as constants
@L(f(x, y, z))       transform x, y, and z
@F(a*f(t) + c)       transform t; retain a and c as constants
```

If inference finds one variable, the operation is one-dimensional. If it
finds several variables, the operation is a simultaneous multidimensional
transform. If it finds no variables, the transform is rejected unless a
source variable is supplied explicitly.

Inferred variables use canonical binding order, not incidental tree-traversal
order, so simplification and term reordering do not change the mapping.

The defaults are:

```text
@L     any source variable → s
@Linv  s → t
@F     t → ω, x → k, y → m, z → n
@Finv  ω → t, k → x, m → y, n → z
```

The default Laplace transform-domain variable is always `s`, irrespective of
the source-variable name:

```text
@L(f(t), t)     equivalent to @L(f(t), t, s)
@L(g(x), x)     equivalent to @L(g(x), x, s)
```

For an inferred multidimensional Laplace transform, distinct numbered
transform variables are allocated in the same order as the inferred source
variables:

```text
@L(f(x, y, z))
```

is equivalent to transforming:

```text
(x, y, z) → (s₁, s₂, s₃)
```

It is therefore a three-dimensional Laplace transform, not an error.

There is no universal Fourier variable convention. Ordinary frequency `f`
normally accompanies a kernel containing `2πf`; angular frequency `ω` is
conventional for a time-domain source variable. The spatial variables `x`,
`y`, and `z` use the corresponding transform variables `k`, `m`, and `n`.
The recognised forward Fourier defaults are therefore:

```text
@F(f(t), t)     equivalent to @F(f(t), t, ω)
@F(f(x), x)     equivalent to @F(f(x), x, k)
@F(f(y), y)     equivalent to @F(f(y), y, m)
@F(f(z), z)     equivalent to @F(f(z), z, n)
```

For a Fourier transform with any other source-variable name, the transform
variable must be given explicitly. The inverse operator applies the
corresponding reverse mappings. An explicit third parameter overrides a
default:

```text
@F(g(q), q, p)
@F(f(t), t, k)
```

Explicit variables avoid guessing when the expression contains several free
variables. Variables not named as the source variable are treated as
parameters:

```text
@L(f(t, x), t, s)
@F(u(x, t), x, k)
```

Here `x` remains free in the Laplace transform and `t` remains free in the
Fourier transform.

## Definitions

The forward Laplace transform is unilateral:

```latex
\mathcal{L}\{f(t)\}(s)
  = \int_0^\infty e^{-st} f(t)\,dt
```

The inverse Laplace transform is the Bromwich integral:

```latex
\mathcal{L}^{-1}\{F(s)\}(t)
  = \frac{1}{2\pi i}
    \int_{\gamma-i\infty}^{\gamma+i\infty} e^{st}F(s)\,ds
```

The Fourier transform uses angular spatial frequency and the convention:

```latex
\mathcal{F}\{f(x)\}(k)
  = \int_{-\infty}^{\infty} f(x)e^{-ikx}\,dx
```

```latex
\mathcal{F}^{-1}\{F(k)\}(x)
  = \frac{1}{2\pi}
    \int_{-\infty}^{\infty} F(k)e^{ikx}\,dk
```

This fixes the sign and normalisation rather than making them implicit
implementation choices.

## Expression Rendering

`style_EXPRESSION` renders the operators using Unicode script capitals:

```text
ℒ(f(x,y,z))
ℒ(f(t),t)
ℒ⁻¹(F(s),s)
ℱ(f(x),x)
ℱ⁻¹(F(k),k)
ℱ(g(q),q,p)
```

The parser accepts both these canonical forms and the typeable `@L`, `@Linv`,
`@F`, and `@Finv` aliases, so expression-style output remains round-trippable.
If script-letter output is unavailable, the textual fallbacks are
`Laplace(...)`, `InverseLaplace(...)`, `Fourier(...)`, and
`InverseFourier(...)`.

Inferred variables remain omitted; an explicit source variable is preserved.
The third parameter is included only when it is explicit or differs from the
applicable default.

Plain ASCII `L(...)` and `F(...)` are not transform aliases. Reserving them
would make an ordinary variable or user function named `L` or `F` ambiguous;
`F` also appears in established function notation such as `F₁`. The `@`
aliases provide unambiguous ASCII input, while `ℒ(...)` and `ℱ(...)` provide
unambiguous canonical rendering. Bare `L`, `F`, `ℒ`, and `ℱ` remain ordinary
symbols when they are not followed by transform-call syntax.

Transforms may be nested:

```text
ℱ⁻¹(ℱ(f(x),x,k),k,x)
ℒ(ℱ(u(x,t),x,k),t,s)
```

## TeX Rendering

TeX output uses conventional calligraphic operators:

```text
@L(f(t), t, s)
```

renders as:

```latex
\mathcal{L}_{t\to s}\{f(t)\}
```

The remaining operators render as:

```latex
\mathcal{L}^{-1}_{s\to t}\{F(s)\}
\mathcal{F}_{x\to k}\{f(x)\}
\mathcal{F}^{-1}_{k\to x}\{F(k)\}
```

The variable mapping is retained in rendered output because it is part of the
operation, particularly in expressions containing several free variables.

## Evaluation and Simplification

If a transform is known, evaluation returns the transformed expression in the
third parameter. If no applicable rule is known, the transform remains as an
unevaluated symbolic node rather than producing an error.

Forward and inverse transforms cancel only when their variable mappings match
and the mathematical side conditions are satisfied:

```text
@Linv(@L(f(t),t,s),s,t)
@Finv(@F(f(x),x,k),k,x)
```

Convergence conditions, regions of convergence, distributions, branch
choices, and assumptions such as causality must not be silently discarded.
Where the system cannot establish the required conditions, it retains the
unevaluated transform.
