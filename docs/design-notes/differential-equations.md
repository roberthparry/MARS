# Differential Equation Syntax

> Design note: this syntax is agreed for future ODE and PDE support but is not
> yet implemented.

Differential equations extend the existing bound equation form with a third
section:

```text
{ equation | independent variables; constant bindings; conditions }
```

For example:

```text
{
    Dxx(y) + 3*Dx(y) + 2*y = sin(x)
    | x = ?;
    A = 1, C = 0;
    y(0) = A,
    Dx(y)(0) = C
}
```

The sections after `|` contain:

1. Independent variables, marked with `?`.
2. Fixed symbolic or numeric constants.
3. Initial or boundary conditions.

An empty section remains empty:

```text
{ Dxx(y) + y = 0 | x = ?;; y(0) = 1, Dx(y)(0) = 0 }
```

## Derivatives

Derivative suffixes name the variables in differentiation order:

```text
Dx(u)       first derivative with respect to x
Dxx(u)      second derivative with respect to x
Dxy(u)      Dy(Dx(u))
Dxyz(u)     Dz(Dy(Dx(u)))
```

Thus suffixes are read from left to right. The order is retained internally
and in rendered output.

For an ODE with dependent variable `y(x)`, `Dx(y)` is an ordinary derivative.
For a multivariable function such as `u(x,y)`, `Dx(u)` and `Dy(u)` are partial
derivatives.

For ODEs, prime notation is accepted as shorthand for derivatives with respect
to the independent variable:

```text
y'          Dx(y)
y''         Dxx(y)
y'''        Dxxx(y)
```

The explicit `D...(...)` notation remains available and is required when the
variable of differentiation would otherwise be ambiguous, as in PDEs.

For high-order and mixed derivatives, a bracketed derivative-index
specification provides a compact form:

```text
D[x](y)          Dx(y)
D[xx](y)         Dxx(y)
D[xy](u)         Dxy(u)
D[x^4](y)        Dxxxx(y)
D[x^10](y)       tenth derivative of y with respect to x
D[x^2y^3](u)     Dxxyyy(u)
D[x^2y^3x](u)    Dxxyyyx(u)
```

Within the brackets, entries are read from left to right. A positive integer
exponent repeats the immediately preceding differentiation variable that many
times, so grouping does not discard differentiation order. The `#` in the
general pattern `D[x^#y^#](u)` is a grammar placeholder only; actual input
contains the integer.

Expression-style output omits the square brackets and uses Unicode superscript
digits:

```text
Dx⁴(y)
Dx¹⁰(y)
Dx²y³(u)
```

Thus the brackets delimit the explicit derivative-index specification but are
not part of its canonical `style_EXPRESSION` rendering. The canonical
unbracketed form is also accepted as input so expression-style output remains
round-trippable.

When the parser encounters `D`, it looks ahead for an opening `(` with only
alphanumeric derivative-index characters between them. If it finds one, the
sequence is parsed as a derivative application:

```text
Dx(y)
Dx²y³(u)
```

If a non-alphanumeric character intervenes, or no opening `(` follows, the
sequence is not recognised as the canonical derivative form. In particular,
bare `Dx` remains an ordinary symbol, such as the `x` component of a vector
`D`.

## Conditions

A derivative can be evaluated using the same function-style notation as the
dependent variable:

```text
y(0) = A
Dx(y)(0) = C
u(x, 0) = A
Dy(u)(x, 1) = sin(x)
```

For ODEs, prime notation can be evaluated in the same way and serves as
shorthand for the corresponding explicit derivative condition:

```text
y(0)        y(0)
y'(0)       Dx(y)(0)
y''(0)      Dxx(y)(0)
```

In `Dy(u)(x, 1) = sin(x)`, `x` remains free while `y` is fixed at `1`, so the
condition applies along that boundary. By contrast, the bare equation
`Dy(u) = sin(x)` applies throughout the domain.

A PDE with boundary conditions can therefore be written as:

```text
{
    Dxx(u) + Dyy(u) = 0
    | x = ?, y = ?;
    A = 2, B = 3;
    u(x, 0) = A,
    Dy(u)(x, 1) = sin(x),
    u(0, y) = B
}
```

## TeX Rendering

The renderer should emit conventional mathematics rather than expose the
compact input notation.

For ODEs:

```text
Dxx(y) + 3*Dx(y) + 2*y = sin(x)
```

renders as:

```tex
y'' + 3y' + 2y = \sin x
```

For PDEs:

```text
Dxx(u) + Dyy(u) = 0
```

renders as:

```tex
\frac{\partial^2 u}{\partial x^2}
+ \frac{\partial^2 u}{\partial y^2} = 0
```

Mixed derivatives preserve their declared order:

```text
Dxy(u)
```

renders as:

```tex
\frac{\partial^2 u}{\partial y\,\partial x}
```

A boundary condition such as `Dy(u)(x, 1) = sin(x)` renders as:

```tex
\left.\frac{\partial u}{\partial y}\right|_{y=1} = \sin x
```

The complete equation, constants, and conditions should be displayed as one
aligned mathematical system.
