# `diffequ_t`

`diffequ_t` is the public differential-equation problem type. It sits above
`equation_t` and records:

- the differential equation itself;
- its independent variables;
- fixed problem constants; and
- initial or boundary conditions.

The module supports construction, parsing, inspection, formatting, and
symbolic solvers for first-order separable, linear, homogeneous, affine and
linear-coordinate substitutions, and quadratic Bernoulli ODEs. Second-order
linear equations are normalised to self-adjoint Sturm–Liouville form, with
closed-form solutions for constant coefficients and affine Riccati
factorisations. Selected modified-Emden equations are point-linearised and
reported with their identified `SL(3, ℝ)` symmetry group.
Constant-coefficient linear ODEs of arbitrary order are solved through their
characteristic polynomial and variation of parameters.
The Lie-analysis API constructs prolongations, determining equations and
invariants, and searches bounded polynomial generator spaces. For supported
polynomial second-order ODEs without a closed-form result, a local Taylor
fallback retains arbitrary initial data and reports a separate series status.
PDE solvers include transport and characteristic families, selected
second-order equations, one-dimensional forced wave initial-value problems
using d'Alembert–Duhamel integrals, and the three-dimensional homogeneous wave
equation using Kirchhoff's formula.

## Ownership

- `de_new(...)`, `de_from_string(...)`, and `de_from_text(...)` return owning
  `diffequ_t *` handles.
- `de_free(...)` releases an owning handle and accepts `NULL`.
- `de_equation(...)`, `de_independent_at(...)`, `de_constants(...)`,
  `de_constant(...)`, `de_condition_at(...)`, and
  `de_condition_argument_at(...)` return borrowed objects owned by the
  differential equation.
- `de_to_text(...)` returns an owning `string_t *`, released with
  `string_free(...)`.
- `de_to_string(...)` returns an owning C string, released with `free(...)`.
- `de_solve(...)` and `de_solve_with_options(...)` return an owning
  `diffequ_solve_result_t *`, released with `de_solve_result_free(...)`.
- Equations returned by `de_solve_result_at(...)` are borrowed from the solve
  result.
- Text returned by `de_solve_result_diagnostic(...)`,
  `de_solve_result_steps(...)`, `de_solve_result_steps_TeX(...)`, and
  `de_solve_result_symmetry(...)` is borrowed from the solve result.

The public declarations are in:

```c
#include "diffequation.h"
```

## Rule-Based Solving and Derivations

MARSlib does not select a result by comparing the input with a catalogue of
complete example equations. Each successful solver first extracts a
parameterised mathematical structure from the parsed expression tree: for
example separability, a Bernoulli exponent, a characteristic vector field, an
exact differential, a constant-coefficient characteristic polynomial, a
Laplace operator, or an eigenfunction of a spatial evolution operator. The
coefficients, coordinates, powers, invariants, integrating factors, roots and
initial data used in the answer come from that extraction.

Derivations are deliberately opt-in because most library callers need only the
solution. `de_solve(...)` therefore performs no derivation formatting. A
presenting client can call
`de_solve_with_options(de, DE_SOLVE_OPTION_STEPS)` to request plain-text and
TeX derivations through `de_solve_result_steps(...)` and
`de_solve_result_steps_TeX(...)`. A specialised solver may expose its detailed
intermediate expressions directly. Otherwise the common derivation layer
states the selected family rule, the actual parsed equation and every derived
solution branch. MARS Lab requests and displays these strings; other clients
do not pay their construction cost unless they explicitly opt in.

Regression coverage includes coefficient and exponent variants which differ
from the motivating examples. This guards against accidentally replacing a
family recogniser with a literal one-case match.

## Input Forms

Examples keep inputs in copyable MARS syntax and show their TeX translations
before the typeset solutions. Literal API output is retained in code blocks
where its exact format is being demonstrated.

The explicit form separates independent variables, constants, and conditions:

```text
{ Dx(y) + a*y = x | x = ?; a = 2; y(0) = 1 }
```

For an ordinary differential equation, the derivative operator already
identifies the independent variable, so a shorter form is available:

```text
Dxx(y) = y; y(0) = 1; y'(0) = 1
```

Here `Dxx(y)` identifies `x` as the independent variable. The remaining
semicolon-separated equations are conditions. Prime notation in a condition
is normalised to the formal derivative notation used internally:

| Shorthand | MARS notation | Mathematical notation |
| :--- | :--- | :--- |
| `y'(0) = 1` | `Dx(y)(0) = 1` | $\left.\dfrac{dy}{dx}\right\rvert_{x=0}=1$ |

For ordinary differential equations in `x`, prime notation can also be used
directly in the equation:

```text
y'' + 4y = e^x
```

Normalised equation: `Dxx(y) + 4y = e^x`.

$\displaystyle\quad \begin{aligned} & \frac{d^{2} y}{d x^{2}} + 4\mkern-2mu y = e^{x} \\[1em] & y = \tfrac15 e^x+C_1\cos(2x)+C_2\sin(2x) \end{aligned}$

When `x` is the dependent variable, prime notation defaults to differentiation
with respect to time so that the two variables do not collide:

```text
x'' + x = 0
```

Normalised equation: `Dtt(x) + x = 0`.

$\displaystyle\quad \begin{aligned} & \frac{d^{2} x}{d t^{2}} + x = 0 \\[1em] & x = C_1\cos t+C_2\sin t \end{aligned}$

The standard forms `dy/dx`, `d²y/dx²`, and `d³y/dx³` are accepted as
equivalent input aliases. In the Mars Lab, ordinary derivatives are displayed
this way in both the rendered equation and the differential-equation card.
The canonical expression form remains `Dx(y)`, `Dxx(y)`, and `Dxxx(y)`.

Constant-coefficient polynomial differential operators can be applied to a
dependent variable directly, with or without parentheses around that
variable. Coefficients may be numeric or symbolic. MARS expands the operator
before selecting the ordinary constant-coefficient solver. For example:

```text
input = (Dx^2 + 4Dx + 20)^2(y) = 0
resolved = Dxxxx(y) + 8*Dxxx(y) + 56*Dxx(y) + 160*Dx(y) + 400*y = 0
solution = y = exp(-2x)·(C₁·cos(4x) + C₂·sin(4x)
                         + C₃x·cos(4x) + C₄x·sin(4x))
```

Here `Dx` denotes the differential operator with respect to `x`, rather than
the already-applied formal derivative `Dx(y)`. Literal positive integer powers
through 64 are accepted for numeric or symbolic polynomial operators; the
independent variable is inferred from the operator suffix.

The suffix may be omitted. This is a general parser rule: bare `D` means `Dt`
only when it operates on dependent `x`; for `y`, `z`, or any other dependent
variable it means `Dx`. The operator changes the differentiation coordinate,
never the dependent variable. The rule applies both to individual derivatives
and to polynomial operators:

| Input | Equivalent explicit operator | Equation |
| :--- | :--- | :--- |
| `D(y) = y` | `Dx(y) = y` | $\frac{d y}{d x} = y$ |
| `D(z) = z` | `Dx(z) = z` | $\frac{d z}{d x} = z$ |
| `D(q) = q` | `Dx(q) = q` | $\frac{d q}{d x} = q$ |
| `D^2(y) + y = 0` | `Dxx(y) + y = 0` | $\frac{d^{2} y}{d x^{2}} + y = 0$ |
| `D(x) = x` | `Dt(x) = x` | $\frac{d x}{d t} = x$ |
| `D^2(x) + x = 0` | `Dtt(x) + x = 0` | $\frac{d^{2} x}{d t^{2}} + x = 0$ |
| `(D^2 + 4D + 20)^2(y) = 0` | `(Dx^2 + 4Dx + 20)^2(y) = 0` | $\frac{d^{4} y}{d x^{4}} + 8\mkern-2mu \frac{d^{3} y}{d x^{3}} + 56\mkern-2mu \frac{d^{2} y}{d x^{2}} + 160\mkern-2mu \frac{d y}{d x} + 400\mkern-2mu y = 0$ |
| `(D^2 + 4D + 20)^2(x) = 0` | `(Dt^2 + 4Dt + 20)^2(x) = 0` | $\frac{d^{4} x}{d t^{4}} + 8\mkern-2mu \frac{d^{3} x}{d t^{3}} + 56\mkern-2mu \frac{d^{2} x}{d t^{2}} + 160\mkern-2mu \frac{d x}{d t} + 400\mkern-2mu x = 0$ |

For symbolic frequency $\omega$, the expanded equations are:

| Input | Normalised equation | Equation |
| :--- | :--- | :--- |
| `(D^2 + @omega^2)x = 0` | `Dtt(x) + ω^2*x = 0` | $\frac{d^{2} x}{d t^{2}} + \omega^{2}\mkern-2mu x = 0$ |
| `(D^2 - @omega^2)x = 0` | `Dtt(x) - ω^2*x = 0` | $\frac{d^{2} x}{d t^{2}} - \omega^{2}\mkern-2mu x = 0$ |
| `(D^2 - @omega^2)^2(x) = 0` | `Dtttt(x) - 2ω^2*Dtt(x) + ω^4*x = 0` | $\frac{d^{4} x}{d t^{4}} - 2\mkern-2mu \omega^{2}\mkern-2mu \frac{d^{2} x}{d t^{2}} + \omega^{4}\mkern-2mu x = 0$ |
| `(D^2 + @omega^2)^2(x) = 0` | `Dtttt(x) + 2ω^2*Dtt(x) + ω^4*x = 0` | $\frac{d^{4} x}{d t^{4}} + 2\mkern-2mu \omega^{2}\mkern-2mu \frac{d^{2} x}{d t^{2}} + \omega^{4}\mkern-2mu x = 0$ |
| `(D^2 + @omega^2)^3x = 0` | `Dtttttt(x) + 3ω^2*Dtttt(x) + 3ω^4*Dtt(x) + ω^6*x = 0` | $\frac{d^{6} x}{d t^{6}} + 3\mkern-2mu \omega^{2}\mkern-2mu \frac{d^{4} x}{d t^{4}} + 3\mkern-2mu \omega^{4}\mkern-2mu \frac{d^{2} x}{d t^{2}} + \omega^{6}\mkern-2mu x = 0$ |

Their solutions are:

| Input | Equation | Solution |
| :--- | :--- | :--- |
| `(D^2 + @omega^2)x = 0` | $\frac{d^{2} x}{d t^{2}} + \omega^{2}\mkern-2mu x = 0$ | $x = C_1\cos(\omega t)+C_2\sin(\omega t)$ |
| `(D^2 - @omega^2)x = 0` | $\frac{d^{2} x}{d t^{2}} - \omega^{2}\mkern-2mu x = 0$ | $x = C_1e^{\omega t}+C_2e^{-\omega t}$ |
| `(D^2 - @omega^2)^2(x) = 0` | $\frac{d^{4} x}{d t^{4}} - 2\mkern-2mu \omega^{2}\mkern-2mu \frac{d^{2} x}{d t^{2}} + \omega^{4}\mkern-2mu x = 0$ | $x = (C_1+C_2t)e^{\omega t}+(C_3+C_4t)e^{-\omega t}$ |
| `(D^2 + @omega^2)^2(x) = 0` | $\frac{d^{4} x}{d t^{4}} + 2\mkern-2mu \omega^{2}\mkern-2mu \frac{d^{2} x}{d t^{2}} + \omega^{4}\mkern-2mu x = 0$ | $x = (C_1+C_2t)\cos(\omega t)+(C_3+C_4t)\sin(\omega t)$ |
| `(D^2 + @omega^2)^3x = 0` | $\frac{d^{6} x}{d t^{6}} + 3\mkern-2mu \omega^{2}\mkern-2mu \frac{d^{4} x}{d t^{4}} + 3\mkern-2mu \omega^{4}\mkern-2mu \frac{d^{2} x}{d t^{2}} + \omega^{6}\mkern-2mu x = 0$ | $x = (C_1+C_2t+C_3t^2)\cos(\omega t)+(C_4+C_5t+C_6t^2)\sin(\omega t)$ |

More generally, for any literal positive integer `n` up to 64,

```text
(D^2 + @omega^2)^n x = 0
```

$\displaystyle\quad \begin{aligned} & \left(\frac{d^2}{dt^2}+\omega^2\right)^n x = 0 \\[1em] & x = \left(\sum_{k=0}^{n-1} C_{k+1}t^k\right)\cos(\omega t) + \left(\sum_{k=0}^{n-1} C_{n+k+1}t^k\right)\sin(\omega t) \end{aligned}$

Here `n` describes the general family; an entered operator power must be a
literal integer so MARS can construct the corresponding order `2n` equation.
For `n < 4`, MARS writes every term explicitly; finite-sum notation begins at
`n = 4`.

An explicit suffix always takes precedence.

Additive forcing terms are solved independently and then combined:

```text
y'' + 4y = e^x + x^3
```

$\displaystyle\quad \begin{aligned} & \frac{d^{2} y}{d x^{2}} + 4\mkern-2mu y = e^{x} + x^{3} \\[1em] & y = \tfrac15 e^x+\tfrac14 x^3-\tfrac38 x+C_1\cos(2x)+C_2\sin(2x) \end{aligned}$

Autonomous first-order equations that are quadratic in the derivative can
produce multiple implicit branches and a singular solution:

```text
(y')^2 = y' + 2y
```

$\displaystyle\quad \begin{aligned} & \left(\frac{d y}{d x}\right)^{2} = \frac{d y}{d x} + 2\mkern-2mu y \\[1em] & \begin{aligned} x &= \tfrac12\left(\sqrt{8y+1}-\ln\left\lvert\tfrac12(\sqrt{8y+1}+1)\right\rvert+1\right)+C,\\ x &= \tfrac12\left(1-\sqrt{8y+1}-\ln\left\lvert\tfrac12(1-\sqrt{8y+1})\right\rvert\right)+C,\\ y &= 0. \end{aligned} \end{aligned}$

Exact third-order nonlinear forms can be integrated once and linearised. For
example,

```text
y''' + y''*y' = 3x^2
```

$\displaystyle\quad \begin{aligned} & \frac{d^{3} y}{d x^{3}} + \frac{d^{2} y}{d x^{2}}\mkern-2mu \frac{d y}{d x} = 3\mkern-2mu x^{2} \\[1em] & \begin{aligned} y &= 2\ln\left\lvert\sum_{n=0}^{\infty} c_n x^n\right\rvert,\\ c_0 &= C_2,\qquad c_1 = C_3,\qquad c_{-1}=c_{-2}=c_{-3}=0,\\ c_{n+2} &= \frac{C_1c_n+c_{n-3}}{2(n+2)(n+1)}. \end{aligned} \end{aligned}$

Here the original left side is
$\frac{d}{dx}\bigl(y''+\tfrac12(y')^2\bigr)$. After one integration, the substitution
$u=e^{y/2}$ cancels the quadratic derivative term and gives
$u''=\tfrac12(x^3+C_1)u$. The displayed recurrence is obtained by substituting
$u=\sum c_nx^n$; it is a complete convergent power-series solution and does
not introduce nonstandard special-function names. The independent constants
$C_1$, $C_2$, and $C_3$ provide the three arbitrary constants required by the
original third-order equation.

The modified Emden equation is linearised by a logarithmic derivative. This
example keeps the original input alongside the reported solver, symmetry, and
solution:

```text
input = y'' + 3y*y' + y^3 = 0
solver = linear transformation
symmetry = SL(3, ℝ)
solution = y = (2x + C₁)/(x² + C₁x + C₂)
```

Setting $y=u^{-1}u'$ changes the left side into $u^{-1}u'''$. Thus $u'''=0$,
so `u` is quadratic and `y` is its logarithmic derivative. Equivalently, the
point transformation from the linearisation notes,
$X=x-1/y$ and $Y=x/y-x^2/2$, reduces the equation to
$\frac{d^2Y}{dX^2}=0$.

The scaled family is recognised separately:

```text
input = y'' + 6*y*y' + 4*y^3 = 0
solver = linear transformation
symmetry = SL(3, ℝ)
solution = y = (2x + C₁)/(2·(x² + C₁x + C₂))
```

## Lie-Symmetry Analysis

`de_solve_result_symmetry(...)` returns the borrowed UTF-8 name of a symmetry
group identified by the selected solver. It currently returns `SL(3, ℝ)`
for the two modified-Emden inputs shown above and `NULL` when the solver has
not identified a group. The group name is separate from the TeX derivation
available through `de_solve_result_steps_TeX(...)`; mathematically, it is
written $\mathrm{SL}(3,\mathbb R)$.

The group-name metadata is separate from the general `de_lie_*` analysis API.
Following the [extended-group notes](<books/Applied Differential Equations/03-lie-theory-of-extended-group.md>),
MARS constructs the prolongations and determining equation from a scalar
second-order ODE, rather than matching a catalogue of known generators.
The equation must be linear in its highest derivative so that it can be
normalised locally, where the leading coefficient is non-zero.

$\displaystyle\quad y''=f(x,y,p),\qquad p=y',\qquad D=\partial_x+p\partial_y+f\partial_p.$

For a point generator, the components $\xi$ and $\eta$ depend on $x,y$, not on $p$:

$\displaystyle\quad \begin{aligned} &G=\xi(x,y)\partial_x+\eta(x,y)\partial_y,\\[0.5em] &\eta^{(1)}=D\eta-pD\xi,\qquad \eta^{(2)}=D\eta^{(1)}-fD\xi,\\[0.5em] &\eta^{(2)}-\xi f_x-\eta f_y-\eta^{(1)}f_p=0.\end{aligned}$

The final line is the **determining equation**. MARS can construct it with
unknown functions, or substitute supplied components and verify their residual.
Supplied components may be non-polynomial. The automatic search solves for
polynomial components of a requested total degree, from zero through four,
using the native equation module's coefficient collector and the matrix
module's exact nullspace. Every returned generator is checked in the original
determining equation. Brackets are calculated by differentiation, and structure
constants are returned only when the supplied polynomial basis is independent
and its brackets close with constant coefficients.

This is not an unrestricted solver for every determining PDE. The automatic
search requires polynomial residuals of degree at most 15 in each jet coordinate
and finite real numeric coefficients. A successful empty search means no
generator exists **within the requested polynomial space**; an unsupported
search returns `NULL`, not an empty basis. Non-polynomial generators can exist
outside that space. Initial and boundary conditions are not imposed on the
equation's symmetry algebra.

For example, the free-particle equation yields eight generators at degree two,
in agreement with the [linearisation notes](<books/Applied Differential Equations/02-linearisation-of-non-linear-odes.md>):

$\displaystyle\quad \begin{aligned} &y''=0,\\[0.5em] &(\xi,\eta)\in\operatorname{span}\{(1,0),(x,0),(y,0),(0,1),(0,x),(0,y),(x^2,xy),(xy,y^2)\}.\end{aligned}$

MARS also computes both Lie–Tressé relative invariants symbolically:

$\displaystyle\quad \begin{aligned} &I_1=f_{pppp},\\[0.5em] &I_2=D^2f_{pp}-4Df_{yp}-f_pDf_{pp}+6f_{yy}-3f_yf_{pp}+4f_pf_{yp}.\end{aligned}$

Both must vanish identically for local point-linearisation. Their value at one
point is not sufficient. A non-zero invariant rules out that linearisation,
**not all symmetries**, and not all other methods of solving the equation.
The quartic example now has a computed diagnostic and a verified translation
symmetry, rather than a family-specific rejection:

```text
input = y'' + 3*y*y' + y^4 = 0
solution status = local series (not a closed form)
I₁ = 0
I₂ = 36·(y - 2y²)
degree-two generators = (ξ, η) = (1, 0)
```

Because the equation has no explicit dependence on $x$, the translation
symmetry permits an autonomous order reduction. Set $p(y)=dy/dx$, treating
the first derivative as a function of $y$. The chain rule then gives

$\displaystyle\quad \frac{d^2y}{dx^2}=\frac{dp}{dx}
=\frac{dp}{dy}\frac{dy}{dx}=p\frac{dp}{dy}.$

Substituting $y'=p$ and $y''=p\,dp/dy$ into the original equation gives

$\displaystyle\quad p\frac{dp}{dy}=-3yp-y^4.$

`de_lie_autonomous_reduction()` constructs this first-order equation for any
supported autonomous normal form. Using $y$ as a local coordinate requires
$p\ne0$; equilibrium solutions must be checked separately in the original ODE.
A reduction is not itself a closed-form solution. When a second-order solve
cannot be completed, `DE_SOLVE_OPTION_STEPS` includes the available native Lie
analysis without changing the solve status to `SOLVED`.

For polynomial normal forms, the [local Taylor fallback](#local-taylor-series)
can additionally return `DE_SOLVE_STATUS_SERIES`. This does not assert that
the equation is point-linearisable or has an elementary closed form.

## Mars Lab

The **Differential Equation** tab accepts the same shorthand. Evaluation is a
thin-client call to the native `diffequation` module: the Lab displays the
normalised problem, selected solver family, diagnostic, and every symbolic
solution returned by `de_solve(...)`. **Use as input** restores the original
problem, including its initial or boundary conditions.

## Partial Differential Equations

Ordinary and partial differential equations share the same `diffequ_t`.
Subscript derivative notation explicitly denotes a partial derivative, even
when only one differentiation coordinate is inferred. Multiple declared
independent variables likewise make derivative notation partial:

| Shorthand | MARS notation | Mathematical notation |
| :--- | :--- | :--- |
| `u_x` | `Dx(u)` | $\dfrac{\partial u}{\partial x}$ |
| `u_y` | `Dy(u)` | $\dfrac{\partial u}{\partial y}$ |
| `u_xy` | `Dxy(u) = Dy(Dx(u))` | $\dfrac{\partial^2 u}{\partial y\,\partial x}$ |
| `u_xx` | `Dxx(u)` | $\dfrac{\partial^2 u}{\partial x^2}$ |
| `phi_x` | `Dx(@phi)` | $\dfrac{\partial\phi}{\partial x}$ |
| `z_y` | `Dy(z)` | $\dfrac{\partial z}{\partial y}$ |
| `xzz_x` | `x*z*Dx(z)` | $xz\,\dfrac{\partial z}{\partial x}$ |

Subscript suffixes are read from left to right. The shorthand is local to the
differential-equation parser; ordinary expression identifiers containing an
underscore are unchanged. In compact PDE products such as `xzz_x`, leading
coordinate factors remain multiplicative coefficients and the suffix applies
to the final field: `x*z*z_x`.
When an unambiguous shorter field name occurs in a subscript derivative,
parameter prefixes on that same field are treated as coefficients, independently
of term order. For example:

```text
u_y + au_xx + bu_yy = 0
```

$\displaystyle\quad \frac{\partial u}{\partial y}
 +a\frac{\partial^2u}{\partial x^2}+b\frac{\partial^2u}{\partial y^2}=0.$

This is equivalent to writing explicit multiplication signs before the
derivatives. Correct parsing does not imply that a solver supports this family.
Without an unambiguous shorter field, multi-letter field names are retained as
single symbols, not products or unit expressions. Greek field names retain
their Greek meaning. Explicit derivative notation with a bracketed name can
be used to disambiguate an intended multi-letter field.

The expression and unbound styles retain `Dx(u)` for round-trip input, while
TeX output uses standard partial-derivative fractions such as
$\frac{\partial u}{\partial x}$. Repeated and mixed derivatives render as
$\frac{\partial^2 u}{\partial x^2}$ and
$\frac{\partial^2 u}{\partial y\,\partial x}$.
Fractional coefficients appear before derivative fractions, rather than
enclosing them in an outer fraction. The same convention applies to ordinary
derivatives and to multiplicative coefficients of repeated or mixed derivatives.
The Mars Lab problem card likewise preserves the Unicode partial-derivative
symbol, displaying `∂u/∂x`, `∂²u/∂x²`, and `∂²u/∂y∂x`. These standard Unicode
forms are also accepted as input aliases, so copying the displayed problem
back into the editor remains valid. The easily typed `Dx(u)` notation remains
the canonical expression and unbound output form.

```text
{
    2*Dx(u) + Dy(u) = 0
    | x = ?, y = ?;
    ;
    u(x, 0) = x^2
}
```

For the constant-coefficient transport equation

$\displaystyle\quad a\frac{\partial u}{\partial x}+b\frac{\partial u}{\partial y}=q,$

the characteristic invariant is $bx-ay$. For $q=0$, the value of `u`
is constant along each characteristic. With explicit data on $y=y_0$, the
solver transports the boundary expression along those curves. The example
above therefore gives:

$\displaystyle\quad u = (x-2y)^2.$

Boundary data on a constant-`x` line is supported as well:

```text
{
    Dx(u) + 3*Dy(u) = 0
    | x = ?, y = ?;
    ;
    u(0, y) = exp(y)
}
```

$\displaystyle\quad \begin{aligned} & \frac{\partial u}{\partial x} + 3\mkern-2mu \frac{\partial u}{\partial y} = 0 \\[1em] & u = e^{y-3x} \end{aligned}$

Affine parameterised boundary curves are supported when they are
non-characteristic. For example:

```text
Dx(z) + Dy(z) = 2*z*(x+y); z(x, 1-x) = x^2
```

$\displaystyle\quad \begin{aligned} & \frac{\partial z}{\partial x} + \frac{\partial z}{\partial y} = 2\mkern-2mu z\mkern-2mu \left(x + y\right) \\[1em] & z = \tfrac14(x-y+1)^2\exp\!\left(\tfrac12\bigl((x+y)^2-1\bigr)\right) \end{aligned}$

Here $x+y$ changes along each characteristic while $y-x$ is invariant. The
boundary curve $y=1-x$ determines the formerly arbitrary function of that
invariant.

Boundary applications retain each coordinate separately. For `u(x, 0)`,
`de_condition_argument_count(...)` returns two, and
`de_condition_argument_at(...)` returns the borrowed `x` and `0` expressions.

Homogeneous second-order PDEs with real numeric constant coefficients are
reduced to a characteristic quadratic. For $Az_{xx}+Bz_{xy}+Cz_{yy}=0$,
the roots of $Am^2+Bm+C=0$ give the characteristic coordinates.
Distinct roots give $F(m_1x+y)+G(m_2x+y)$; a repeated root gives
$F(mx+y)+xG(mx+y)$. Coordinates are exchanged when necessary, and the
pure mixed-derivative equation gives $F(x)+G(y)$. Both mixed-derivative
orders are accepted and their coefficients are combined. For example:

```text
z_xx - 3z_yx + 2z_yy = 0
```

$\displaystyle\quad \begin{aligned} & \frac{\partial^{2} z}{\partial x^{2}} - 3\mkern-2mu \frac{\partial^{2} z}{\partial x\,\partial y} + 2\mkern-2mu \frac{\partial^{2} z}{\partial y^{2}} = 0 \\[1em] & z = F(x+y)+G(2x+y) \end{aligned}$

Here the operator factors as $(\partial_x-\partial_y)(\partial_x-2\partial_y)$. The native equation solver
supplies the quadratic roots, including exact surds and complex roots.
With real characteristic roots, `F` and `G` are arbitrary twice-differentiable
functions; with complex roots they are analytic functions, related by
conjugacy when a real-valued solution is required. This rule does not apply
boundary data or admit lower-order terms or variable coefficients.

For a non-zero right-hand side, the solver adds a verified particular
solution. For example:

```text
z_xx + 5z_yx + 6z_yy = 2e^(x-y)
```

$\displaystyle\quad \begin{aligned} & \frac{\partial^{2} z}{\partial x^{2}} + 5\mkern-2mu \frac{\partial^{2} z}{\partial x\,\partial y} + 6\mkern-2mu \frac{\partial^{2} z}{\partial y^{2}} = 2\mkern-2mu e^{x - y} \\[1em] & z = F(y-3x)+G(y-2x)+e^{x-y} \end{aligned}$

For exponential forcing $f=Ke^{ax+by+d}$, applying the operator
multiplies `f` by $Aa^2+Bab+Cb^2$. When this is non-zero, division by
that factor gives the particular solution. Resonance is handled by testing
linear and quadratic coordinate multipliers instead. The native solver uses
the fixed multiplier basis $1,x,y,x^2,xy,y^2$, accepts a candidate only
after symbolically verifying its differential equation, and applies
superposition to sums. This also covers constant forcing and non-resonant
affine sine and cosine forcing.

Scaled sums are distributed before superposition, without factoring their
terms back together. This includes polynomial forcing such as:

```text
z_xx - 4z_yx + 4z_yy = 48(x^2+y^2)
```

$\displaystyle\quad \begin{aligned}
&z_{xx}-4z_{xy}+4z_{yy}=48(x^2+y^2),\\[1em]
&z=F(2x+y)+xG(2x+y)+4x^4+y^4.
\end{aligned}$

Here the operator is $(\partial_x-2\partial_y)^2$. Its repeated characteristic
root gives the two arbitrary-function terms. The particular solution
$4x^4+y^4$ contributes $48x^2+48y^2$ when the operator is applied.

When those candidates do not suffice, a forcing term depending on a single
affine phase is reduced to an ordinary differential equation. For
$s=ax+by+c$, the factor $Aa^2+Bab+Cb^2$ multiplies the second
derivative of the particular solution with respect to `s`. The solver
integrates twice when this factor is non-zero, and handles characteristic
phases with coordinate multipliers. For example:

```text
z_xx + 5z_yx + 6z_yy = 2tanh(x-y)
```

$\displaystyle\quad \begin{aligned} & \frac{\partial^{2} z}{\partial x^{2}} + 5\mkern-2mu \frac{\partial^{2} z}{\partial x\,\partial y} + 6\mkern-2mu \frac{\partial^{2} z}{\partial y^{2}} = 2\mkern-2mu \tanh(x - y) \\[1em] & \begin{aligned} z &={} F(y-3x)+G(y-2x)\\ &+\tfrac12\operatorname{Li}_2\!\left(-e^{2(y-x)}\right)-(x-y)\ln 2+\tfrac12(x-y)^2. \end{aligned} \end{aligned}$

Both integrations use the native expression integrator. Its general rule for
the logarithm of a hyperbolic cosine uses the existing dilogarithm `Li2`
(written `Li₂` in expression text). The particular solution is smooth for
every real $x-y$; its additive constant is absorbed into `F` or `G`.
Antiderivatives are checked symbolically by the expression module, and
unsupported integration steps remain exact integral nodes, evaluated from
zero to their displayed upper limits, with collision-free bound variables.
Each phase reduction must remove all
dependence on the original coordinates from the transformed forcing; terms
with genuinely independent coordinate dependence remain unsupported by this
rule.

Tangent forcing uses the native integrator's real [Clausen function](expression.md#clausen-functions)
rule for logarithms of sine and cosine:

```text
z_xx + 5z_yx + 6z_yy = 2tan(x-y)
```

$\displaystyle\quad \begin{aligned} & \frac{\partial^{2} z}{\partial x^{2}} + 5\mkern-2mu \frac{\partial^{2} z}{\partial x\,\partial y} + 6\mkern-2mu \frac{\partial^{2} z}{\partial y^{2}} = 2\mkern-2mu \tan(x - y) \\[1em] & z = F(y-3x)+G(y-2x)+\tfrac12\operatorname{Cl}_2(2x-2y+\pi)+(x-y)\ln 2 \end{aligned}$

This is a real closed form on each region between consecutive poles
$x-y=\pi/2+k\pi$; no solution is asserted across those singularities.
The affine term $\ln(2)(x-y)$ can also be absorbed into the arbitrary
functions. No PDE-specific logarithmic integration is performed.

Inverse-hyperbolic-tangent forcing has an elementary closed form:

```text
z_xx + 5z_yx + 6z_yy = 2atanh(x-y)
```

$\displaystyle\quad \begin{aligned} & \frac{\partial^{2} z}{\partial x^{2}} + 5\mkern-2mu \frac{\partial^{2} z}{\partial x\,\partial y} + 6\mkern-2mu \frac{\partial^{2} z}{\partial y^{2}} = 2\mkern-2mu \tanh^{-1}(x - y) \\[1em] & \begin{aligned} z &={} F(y-3x)+G(y-2x)-\tfrac12 x+\tfrac12 y\\ &+\tfrac12(x-y)\ln\bigl(1-(x-y)^2\bigr) +\tfrac12\bigl((x-y)^2+1\bigr)\operatorname{atanh}(x-y). \end{aligned} \end{aligned}$

The particular solution is real for $\lvert x-y\rvert<1$. Both integrations use
the native expression integrator, whose quadratic-denominator rule selects
an inverse hyperbolic tangent when $4ac-b^2$ is known to be negative.
The affine term can be absorbed into the arbitrary functions.

Inverse-cosine forcing likewise has an elementary closed form:

```text
z_xx + 5z_yx + 6z_yy = 2acos(x-y)
```

$\displaystyle\quad \begin{aligned} & \frac{\partial^{2} z}{\partial x^{2}} + 5\mkern-2mu \frac{\partial^{2} z}{\partial x\,\partial y} + 6\mkern-2mu \frac{\partial^{2} z}{\partial y^{2}} = 2\mkern-2mu \cos^{-1}(x - y) \\[1em] & \begin{aligned} z &={} F(y-3x)+G(y-2x)-\tfrac12\arcsin(x-y)\\ &+\tfrac14\arccos(x-y)\bigl(2(x-y)^2-1\bigr) -\tfrac34(x-y)\sqrt{1-(x-y)^2}. \end{aligned} \end{aligned}$

Writing $s=x-y$ and absorbing an additive constant into the arbitrary
functions, its particular solution is

$\displaystyle\quad \frac{(2s^2+1)\arccos(s)-3s\sqrt{1-s^2}}{4}.$

This is real for $|s|<1$. Inverse-sine forcing uses the same native integration
and radical-simplification rules; neither family needs PDE-specific calculus.

Absolute-value forcing also has a real closed form:

```text
z_xx + 5z_yx + 6z_yy = 2abs(x-y)
```

$\displaystyle\quad \begin{aligned} & \frac{\partial^{2} z}{\partial x^{2}} + 5\mkern-2mu \frac{\partial^{2} z}{\partial x\,\partial y} + 6\mkern-2mu \frac{\partial^{2} z}{\partial y^{2}} = 2\mkern-2mu \left|x - y\right| \\[1em] & z = F(y-3x)+G(y-2x)+\tfrac16(x-y)^2\lvert x-y\rvert \end{aligned}$

The particular solution is $|x-y|^3/6$, written equivalently as
$(x-y)^2|x-y|/6$. It is twice continuously differentiable, including at
$x=y$, and solves the PDE on both sides of that line. The native expression
integrator supplies both antiderivatives through its general real affine
absolute-value rules; the PDE solver needs no special case.

Variable coefficients are supported for the radial Euler family

$\displaystyle\quad A(x^2z_{xx}+2xyz_{xy}+y^2z_{yy})+B(xz_x+yz_y)+Cz=0,$

where $A$, $B$ and $C$ are finite real numeric constants and $A\ne0$.
Writing $E=x\partial_x+y\partial_y$, the second-order radial operator is
$E^2-E$. On the local chart $x>0$, the coordinates $r=\ln(x)$ and
$\eta=y/x$ turn $E$ into $\partial_r$. At each fixed ray $\eta$, the PDE
therefore becomes the ordinary equation

$\displaystyle\quad A z_{rr}+(B-A)z_r+Cz=0.$

The native equation solver supplies the roots of $Am(m-1)+Bm+C=0$.
Distinct roots give $x^{m_1}F(\eta)+x^{m_2}G(\eta)$; a repeated root gives
$x^m\bigl(F(\eta)+\ln(x)G(\eta)\bigr)$. The arbitrary functions are twice
continuously differentiable. For complex conjugate roots, conjugate
arbitrary functions produce a real-valued solution. This rule imposes no
boundary data and currently requires a zero right-hand side. For example:

```text
x^2z_xx + 2xyz_yx + y^2z_yy = 0
```

$\displaystyle\quad \begin{aligned}
&x^2\frac{\partial^2 z}{\partial x^2}
 +2xy\frac{\partial^2 z}{\partial x\,\partial y}
 +y^2\frac{\partial^2 z}{\partial y^2}=0,\\[1em]
&z=F(y/x)+xG(y/x).
\end{aligned}$

Here the roots are $0$ and $1$: along each ray the solution is affine in
$x$. This particular formula also works locally for $x<0$, although the
solver's general logarithmic chart is stated for $x>0$. It does not prescribe
behaviour across $x=0$.

Problem equations remain on one TeX line, with the equals sign after the
complete left-hand side. MARS Lab provides horizontal scrolling when needed;
long solution series retain their separate, explicitly aligned layout.

### Forced One-Dimensional Wave Initial-Value Problems

For the constant-speed wave equation with initial displacement and velocity,

$\displaystyle\quad
u_{tt}-c^2u_{xx}=f(x,t),\qquad u(x,t_0)=g(x),\qquad u_t(x,t_0)=h(x),\quad c>0,$

Mars combines d'Alembert's formula with Duhamel's principle:

$\displaystyle\quad u(x,t)=\tfrac12\bigl[g(x+c(t-t_0))+g(x-c(t-t_0))\bigr]+\frac1{2c}\int_{x-c(t-t_0)}^{x+c(t-t_0)}h(\xi)\,d\xi+\frac1{2c}\int_{t_0}^{t}\int_{x-c(t-s)}^{x+c(t-s)}f(\xi,s)\,d\xi\,ds.$

The first two terms propagate the supplied initial data. The final term
accounts for the forcing inside the characteristic triangle; it and its first
time derivative vanish at $t=t_0$. The forcing and initial data may be given
as symbolic functions, without specifying elementary expressions:

```text
u_tt - c^2u_xx = f(x,t); u(x, 0) = g(x); u_t(x,0) = h(x)
```

$\displaystyle\quad u(x,t)=\tfrac12\bigl[g(x+ct)+g(x-ct)\bigr]+\frac1{2c}\int_{x-ct}^{x+ct}h(\xi)\,d\xi+\frac1{2c}\int_0^t\int_{x-c(t-s)}^{x+c(t-s)}f(\xi,s)\,d\xi\,ds.$

The equation card retains the arguments in $f(x,t)$, and the solution retains
the arguments of every function call. These are genuine symbolic function
nodes, including the two arguments of $f$, not scalar placeholders. In
differential-equation input, an undeclared single-letter or bracketed name
followed by parentheses denotes a symbolic function. Registered functions keep
their usual meanings; declared scalar names remain coefficients, so explicit
`*` is recommended when multiplication is intended. This does not change the
ordinary Expression-mode grammar.

Here $f$, $g$ and $h$ describe prescribed data, not extra unknowns to solve for.
You can instead supply concrete expressions, for example:

```text
u_tt - 4u_xx = x*t; u(x,0) = x^2; u_t(x,0) = 1
```

```text
u = ⅙·(t³x + 24t² + 6t + 6x²)
```

$\displaystyle\quad u(x,t)=x^2+4t^2+t+\frac16xt^3.$

This satisfies $u_{tt}-4u_{xx}=xt$, $u(x,0)=x^2$ and $u_t(x,0)=1$.
Polynomial integrals are evaluated exactly, and other elementary primitives
are checked by differentiation. When no verified elementary primitive is
available, Mars retains the bounded integral rather than reporting the PDE as
unsupported:

```text
u_tt - u_xx = exp(cosh(x)+t^2); u(x,0) = sin(x); u_t(x,0) = exp(cosh(x))
```

$\displaystyle\quad u(x,t)=\tfrac12\bigl[\sin(x+t)+\sin(x-t)\bigr]+\frac12\int_{x-t}^{x+t}e^{\cosh(\xi)}\,d\xi+\frac12\int_0^t\int_{x-(t-s)}^{x+(t-s)}e^{\cosh(\xi)+s^2}\,d\xi\,ds.$

The result has status `DE_SOLVE_STATUS_SOLVED` and solver
`DE_SOLVER_DALEMBERT_DUHAMEL`; this includes exact integral representations,
not just finite elementary expressions. `equ_rhs()` retains the native
expression tree and its actual bounds. Native TeX output keeps the solution on
one line, with the two displacement terms grouped under one half and all
coefficients outside the integrals. Long expressions can scroll horizontally.
Travelling-wave arguments and characteristic endpoints put the spatial
coordinate first: $g(x+ct)$ and $x+ct$, not $g(ct+x)$ and $ct+x$.

The rule accepts two pure second derivatives in any term order, a common
non-zero constant scale, renamed coordinates, and both conditions on a common
real time slice. The velocity derivative identifies the time coordinate;
condition argument order must agree between the two conditions. For
$A u_{tt}+B u_{xx}=R$, Mars uses $c^2=-B/A$ and $f=R/A$.
Coefficients must be finite real non-zero constants and $c^2$ must be positive;
unspecified parameters carry these assumptions. The formula applies on the
whole spatial line for sufficiently smooth data. For a coefficient written as
$c^2$, $c$ may be either positive or negative (but real and non-zero): changing
its sign reverses each integral's limits as well as the prefactor and leaves
the answer unchanged. The physical wave speed is $|c|$. Spatial boundaries,
variable coefficients, mixed or lower-order derivatives, nonlinear forcing,
and data on different time slices are outside this rule. Existing solvers
continue to handle supported equations without initial data.

### Three-Dimensional Wave Equations

The three-dimensional constant-speed wave equation is recognised from its
four pure second derivatives. Three equal spatial coefficients and one time
coefficient identify the spatial coordinates and the time coordinate, without
relying on their names. For example:

```text
psi_xx + psi_yy + psi_zz = 1/v^2psi_tt
```

$\displaystyle\quad \begin{aligned}
&\frac{\partial^2\psi}{\partial x^2}+\frac{\partial^2\psi}{\partial y^2}
 +\frac{\partial^2\psi}{\partial z^2}
 =\frac{1}{v^2}\frac{\partial^2\psi}{\partial t^2},\\[0.7em]
&\psi(\mathbf r,t)=\frac{\partial}{\partial t}\left[t\,\mathcal M_{ct}F(\mathbf r)\right]
  +t\,\mathcal M_{ct}G(\mathbf r),\quad \mathbf r=(x,y,z),\\[0.7em]
&c=|v|,\quad v\in\mathbb R,\quad v\ne0,\\[0.7em]
&\mathcal M_a H(\mathbf r)=\frac1{4\pi}\int_{|\boldsymbol\omega|=1}
  H(\mathbf r+a\boldsymbol\omega)\,d\Omega.
\end{aligned}$

This is Kirchhoff's exact integral representation of the general smooth
whole-space solution. **No initial conditions are required:** the two smooth
functions $F$ and $G$ remain arbitrary. They would be determined by the initial
field and initial time derivative if those data were supplied. The formula
extends smoothly through $t=0$, where $\psi=F$ and $\psi_t=G$; it is not a finite
elementary closed form or a restricted plane-wave ansatz.

For $A\Delta\psi+B\psi_{tt}=0$, the solver derives $c^2=-A/B$. The coefficients
must be finite non-zero constants, and the squared speed must be real and
positive. Unspecified parameter values retain these assumptions explicitly.
For a squared parameter such as $v^2$, the speed is displayed as $|v|$ with
$v\in\mathbb R$ and $v\ne0$: either sign of $v$ is allowed. Zero is excluded
because the input contains $1/v^2$, not because square roots are always strictly
positive. This conditional display does not change the expression module's
complex square-root rules.
Renamed coordinates, reordered terms and common non-zero constant scales are
accepted. Anisotropic spatial coefficients, mixed derivatives, lower-order
terms, forcing, variable speed and supplied boundary or initial data are
outside this rule. The result has status `DE_SOLVE_STATUS_SOLVED` and solver
`DE_SOLVER_KIRCHHOFF`.

The native solution stores ordinary definite integrals, using
$\boldsymbol\omega=(\sin\theta\cos\phi,\sin\theta\sin\phi,\cos\theta)$,
$d\Omega=\sin\theta\,d\phi\,d\theta$, $0\le\phi\le2\pi$ and
$0\le\theta\le\pi$. Bound names are chosen to avoid capturing coordinates or
parameters already in the input. Differentiation uses the expression module;
the shorter spherical-mean notation in both TeX and plain solution output is
native equation display metadata, not an unevaluated placeholder function or
client-side algebra. The solution defines the positive speed once, uses a short
speed symbol in the spherical-mean subscripts, and identifies the arbitrary
functions. If `c` is already a coordinate or parameter, a fresh name is used.
The Solver card contains the sphere-integral definition and explains the two
arbitrary functions. The expression trees remain available through `equ_rhs()`;
expression and function output retain the underlying integrals for calculation.

The identity
$\partial_a^2(a\mathcal M_a H)=a\mathcal M_a(\Delta H)$ establishes that the
formula satisfies the wave equation. See the
[Kirchhoff derivation](https://www.math.toronto.edu/ivrii/PDE-textbook/Chapter9/S9.1.html).

### Laplace Equations

For the two-dimensional Laplace equation without boundary data, Mars returns
the general solution using arbitrary analytic functions `F` and `G`:

```text
phi_xx + phi_yy = 0
```

$\displaystyle\quad \begin{aligned} & \frac{\partial^{2} \phi}{\partial x^{2}} + \frac{\partial^{2} \phi}{\partial y^{2}} = 0 \\[1em] & \phi = F(x+iy)+G(x-iy) \end{aligned}$

This is the general local complex-variable family for the two-dimensional
Laplace equation. The solver recognises the equation from its derivative
structure and equal non-zero numeric coefficients, so a common numeric scale
and a different term order do not affect the result. For a real-valued field,
the two analytic functions are related so that their sum is real. Boundary
data are deliberately left to a separate boundary-value solver.

The equivalent polar form is recognised structurally as well:

```text
phi_rr + 1/r phi_r + 1/r^2 phi_thetatheta = 0
```

$\displaystyle\quad \begin{aligned} & \frac{\partial^{2} \phi}{\partial r^{2}} + \frac{\frac{\partial \phi}{\partial r}}{r} + \frac{\frac{\partial^{2} \phi}{\partial \theta^{2}}}{r^{2}} = 0 \\[1em] & \phi = F(re^{i\theta})+G(re^{-i\theta}) \end{aligned}$

Here `phi_thetatheta` uses the same Greek-name table as `@theta` and is
normalised to the second partial derivative with respect to `θ`. A space
between a coefficient and a derivative term implies multiplication. The solver
checks the polar coefficients against `1`, `1/r`, and `1/r^2`, allowing a
common non-zero numeric scale and any term order. It then verifies the complex
coordinates $re^{i\theta}$ and $re^{-i\theta}$ symbolically before returning the
arbitrary-function family.

### First-Order Transport and Characteristics

Without boundary data, first-order transport equations likewise return their
general solution using arbitrary-function notation:

| Input | Equation | Solution |
| :--- | :--- | :--- |
| `Dt(u) + c*Dx(u) = 0` | $\frac{\partial u}{\partial t} + c\mkern-2mu \frac{\partial u}{\partial x} = 0$ | $u = F(x-ct)$ |
| `Dt(u) + c*Dx(u) = 1` | $\frac{\partial u}{\partial t} + c\mkern-2mu \frac{\partial u}{\partial x} = 1$ | $u = t+F(x-ct)$ |
| `Dx(z) + Dy(z) = z` | $\frac{\partial z}{\partial x} + \frac{\partial z}{\partial y} = z$ | $z = e^x F(y-x)$ |
| `Dx(z) + Dy(z) + z = x` | $\frac{\partial z}{\partial x} + \frac{\partial z}{\partial y} + z = x$ | $z = x-1+e^{-x}F(y-x)$ |
| `Dx(@phi) - Dy(@phi) = sin(x) + cos(y)` | $\frac{\partial \phi}{\partial x} - \frac{\partial \phi}{\partial y} = \sin(x) + \cos(y)$ | $\phi = F(x+y)-\cos x-\sin y$ |
| `Dx(z) + Dy(z) = cos(x+y)` | $\frac{\partial z}{\partial x} + \frac{\partial z}{\partial y} = \cos(x + y)$ | $z = F(y-x)+\tfrac12\sin(x+y)$ |
| `Dx(z) + 3*Dy(z) - 2*z + 4*y^2 - 22*y + 4*x + 13 = 0` | $\frac{\partial z}{\partial x} + 3\mkern-2mu \frac{\partial z}{\partial y} + 4\mkern-2mu x - 22\mkern-2mu y - 2\mkern-2mu z + 4\mkern-2mu y^{2} + 13 = 0$ | $z = e^{2x}F(y-3x)+2x-5y+2y^2$ |
| `2*Dx(@phi) + Dy(@phi) + 6*@phi = 37*sin(y)` | $2\mkern-2mu \frac{\partial \phi}{\partial x} + \frac{\partial \phi}{\partial y} + 6\mkern-2mu \phi = 37\mkern-2mu \sin(y)$ | $\phi = e^{-3x}F(y-\tfrac{x}{2})+6\sin y-\cos y$ |
| `Dx(@phi) + Dy(@phi) + Dz(@phi) = @phi` | $\frac{\partial \phi}{\partial x} + \frac{\partial \phi}{\partial y} + \frac{\partial \phi}{\partial z} = \phi$ | $\phi = e^x F(y-x,z-x)$ |

More generally, constant transport accepts
$a\frac{\partial u}{\partial x}+b\frac{\partial u}{\partial y}+pu=q(x,y)$. The reaction coefficient `p` produces
exponential evolution along each characteristic. Mars substitutes the
characteristic path into `q` and applies the one-dimensional integrating
factor along that path. Constant forcing produces the corresponding constant
particular solution. The same evolution is applied when transporting explicit
axis-aligned boundary data with constant forcing.

When `p` is nonzero and `q(x,y)` is polynomial, Mars inverts the transport
operator directly. If $D=a\partial_x+b\partial_y$, then repeated applications of `D`
eventually annihilate the polynomial, so

$\displaystyle\quad (D+p)^{-1}q$

is evaluated as a finite derivative series. This avoids introducing an
unevaluated characteristic integral for an elementary polynomial solution.
For trigonometric, hyperbolic, and exponential forcing, Mars instead
recognises a function space closed under the full directional derivative
$D=a\partial_x+b\partial_y$. If $D^2f=\lambda f$, it solves the transport equation
algebraically and verifies the result against the original operator. This
works whether the phase uses one coordinate or a mixture such as $x+y$, and
keeps elementary answers elementary rather than exposing an internal
characteristic integral.

When the forcing is an integrable unary function of a phase `g(x,y)` and
`D(g)` is a nonzero coordinate-independent value, Mars instead integrates the
unary function with respect to its phase and divides by `D(g)`. The candidate
is again accepted only after substitution into the complete PDE. For example,

```text
Dx(z) + 2*Dy(z) = tanh(x+y)
```

$\displaystyle\quad \begin{aligned} & \frac{\partial z}{\partial x} + 2\mkern-2mu \frac{\partial z}{\partial y} = \tanh(x + y) \\[1em] & z = F(y-2x)+\tfrac13\ln\bigl(\cosh(x+y)\bigr) \end{aligned}$

The homogeneous constant-transport and constant-forcing rules extend to any
number of independent variables. For

$\displaystyle\quad \sum_{j=1}^{n}a_j\frac{\partial u}{\partial x_j}+pu=q,$

Mars chooses a nonzero transport direction as the characteristic parameter
and constructs the other `n - 1` independent invariants. The arbitrary
function therefore has `n - 1` arguments; it is not collapsed into a
one-variable approximation.

Aliases for standard constants are contextual in derivative operands.
Consequently, `@phi` ordinarily denotes the golden ratio, but in
`Dx(@phi)` or `Dy(@phi)` it denotes the dependent field `φ`. The same rule
allows familiar mathematical symbols to be used for angles and other
dependent quantities without changing their ordinary expression meaning.
Plain, prefixed, and symbolic spellings of a Greek dependent variable are
canonicalised identically: `phi`, `@phi`, and `φ` all render as `φ` throughout
the normalised differential equation and its solution. This also applies when
the dependent variable follows a polynomial differential operator directly.

The characteristic solver also handles these nonlinear and
variable-coefficient forms:

| Input | Equation | Solution |
| :--- | :--- | :--- |
| `x^2*Dx(@psi) - x*y*Dy(@psi) + y*@psi = 0` | $x^{2}\mkern-2mu \frac{\partial \psi}{\partial x} - x\mkern-2mu y\mkern-2mu \frac{\partial \psi}{\partial y} + \psi\mkern-2mu y = 0$ | $\psi = e^{y/(2x)}F(xy)$ |
| `x*Dx(z) - 7*y*Dy(z) = 5*x^2*y` | $x\mkern-2mu \frac{\partial z}{\partial x} - 7\mkern-2mu y\mkern-2mu \frac{\partial z}{\partial y} = 5\mkern-2mu x^{2}\mkern-2mu y$ | $z = F(x^7y)-x^2y$ |
| `x*y*Dx(z) - x^2*Dy(z) + y*z = 3*x^2*y` | $x\mkern-2mu y\mkern-2mu \frac{\partial z}{\partial x} + y\mkern-2mu z - x^{2}\mkern-2mu \frac{\partial z}{\partial y} = 3\mkern-2mu x^{2}\mkern-2mu y$ | $z = \dfrac{F(x^2+y^2)}{x}+x^2$ |
| `Dx(@phi)*sec(x) + Dy(@phi) = cot(y)` | $\frac{\partial \phi}{\partial y} + \sec(x)\mkern-2mu \frac{\partial \phi}{\partial x} = \cot(y)$ | $\phi = F(y-\sin x)+\ln(\sin y)$ |
| `x*(y-z)*z_x + y*(z-x)*z_y = z*(x-y)` | $x\mkern-2mu \left(y - z\right)\mkern-2mu \frac{\partial z}{\partial x} + y\mkern-2mu \left(z - x\right)\mkern-2mu \frac{\partial z}{\partial y} = z\mkern-2mu \left(x - y\right)$ | $F(x+y+z,xyz) = 0$ |
| `x*(y^2-z^2)*z_x + y*(z^2-x^2)*z_y = z*(x^2-y^2)` | $x\mkern-2mu \left(y^{2} - z^{2}\right)\mkern-2mu \frac{\partial z}{\partial x} + y\mkern-2mu \left(z^{2} - x^{2}\right)\mkern-2mu \frac{\partial z}{\partial y} = z\mkern-2mu \left(x^{2} - y^{2}\right)$ | $F(x^2+y^2+z^2,xyz) = 0$ |

For a monomial characteristic field with $\frac{b}{a}=k\frac{y}{x}$, Mars constructs the
invariant $yx^{-k}$. For a homogeneous linear reaction term, it then derives
an exponential multiplier. For an inhomogeneous equation, it symbolically
integrates a candidate particular solution along a nonzero characteristic
direction and permits only a coordinate-independent rescaling. Mars
substitutes every candidate back into the complete transport operator; the
solution is accepted only when that symbolic verification reduces exactly to
zero.

Mars also tests the radial invariant $x^2+y^2$ when the characteristic
field is tangent to its level curves. The invariant, reaction multiplier, and
particular term are each verified against the original differential
operator.

For a positive-integer cyclic Lagrange field proportional to
$\bigl(x(y^n-z^n),y(z^n-x^n),z(x^n-y^n)\bigr)$, Mars finds the two independent
first integrals $x^n+y^n+z^n$ and $xyz$. The general integral is therefore
emitted as the implicit arbitrary relation
$F(x^n+y^n+z^n,xyz)=0$.

Weighted linear cyclic fields
$\bigl(a(y-z),b(z-x),c(x-y)\bigr)$, with nonzero constant weights, also have
two first integrals: $x+\frac{a}{b}y+\frac{a}{c}z$ and
$x^2+\frac{a}{b}y^2+\frac{a}{c}z^2$. Mars derives candidate weights from the
characteristic coefficients and verifies both integrals by exact symbolic
differentiation against the original field. Without boundary conditions,
it returns an implicit relation between these integrals, with `F` an
arbitrary function of two arguments. This describes local solution surfaces
where the integrals are independent and the relation defines the dependent
variable.

For example:

```text
(y-z)z_x - (z-x)z_y = x-y
```

$\displaystyle\quad \begin{aligned} & \left(y - z\right)\mkern-2mu \frac{\partial z}{\partial x} - \left(z - x\right)\mkern-2mu \frac{\partial z}{\partial y} = x - y \\[1em] & F(x-y+z,x^2-y^2+z^2) = 0. \end{aligned}$

For a separable field $a(x)\frac{\partial u}{\partial x}+b(y)\frac{\partial u}{\partial y}$, Mars integrates the coordinate
potentials $A'(x)=1/a(x)$ and $B'(y)=1/b(y)$. Their difference $B-A$ is a
characteristic invariant. Each potential, and the resulting particular term,
is accepted only after differentiation and exact substitution into the
original PDE. Thus $\sec(x)\frac{\partial\phi}{\partial x}+\frac{\partial\phi}{\partial y}=\cot(y)$ uses
$A(x)=\sin(x)$, $B(y)=y$, and the particular integral $\ln(\sin(y))$.

Symbols outside the differentiated coordinates are held as parameters. For
example, the following PDE differentiates with respect to `x` and `t`, with
`y` held fixed:

```text
zz_x - zz_t = y-x
```

$\displaystyle\quad \begin{aligned} & z\mkern-2mu \frac{\partial z}{\partial x} - z\mkern-2mu \frac{\partial z}{\partial t} = y - x \\[1em] & z = \pm\sqrt{F(-t-x)+2xy-x^2}. \end{aligned}$

Setting $w=z^2$ gives $\frac{\partial w}{\partial x}-\frac{\partial w}{\partial t}=2(y-x)$. The arbitrary-function term
is constant along this characteristic direction, and $2xy-x^2$ supplies a
verified particular solution. Verification compares symbolic values even
when one is expanded and the other is factored.

| Input | Equation | Solutions |
| :--- | :--- | :--- |
| `Dx(z) + Dy(z) = 6*(x+y)^2*z^2` | $\frac{\partial z}{\partial x} + \frac{\partial z}{\partial y} = 6\mkern-2mu z^{2}\mkern-2mu \left(x + y\right)^{2}$ | $z = \dfrac{1}{F(x-y)-(x+y)^3}$ or $z=0$ |
| `x^2*Dx(z) + y^2*Dy(z) = z^2` | $x^{2}\mkern-2mu \frac{\partial z}{\partial x} + y^{2}\mkern-2mu \frac{\partial z}{\partial y} = z^{2}$ | $z = \dfrac{1}{F(1/x-1/y)+1/x}$ or $z=0$ |
| `x*z*Dx(z) + y*z*Dy(z) + x^2 + y^2 = 0` | $x\mkern-2mu z\mkern-2mu \frac{\partial z}{\partial x} + y\mkern-2mu z\mkern-2mu \frac{\partial z}{\partial y} + x^{2} + y^{2} = 0$ | $z = \pm\sqrt{F(y/x)-x^2-y^2}$ |
| `z*z_x + z*z_y = y - x` | $z\mkern-2mu \frac{\partial z}{\partial x} + z\mkern-2mu \frac{\partial z}{\partial y} = y - x$ | $z = \pm\sqrt{F(y-x)-x^2+y^2}$ |
| `(y-x)*z_x + (y+x)*z_y = (x^2+y^2)/z` | $\left(y - x\right)\mkern-2mu \frac{\partial z}{\partial x} + \left(x + y\right)\mkern-2mu \frac{\partial z}{\partial y} = \frac{x^{2} + y^{2}}{z}$ | $z = \pm\sqrt{F(x^2+2xy-y^2)+2xy}$ |
| `(x+y)*Dx(z) + (y-x)*Dy(z) = 0` | $\left(x + y\right)\mkern-2mu \frac{\partial z}{\partial x} + \left(y - x\right)\mkern-2mu \frac{\partial z}{\partial y} = 0$ | $z = F\bigl(\operatorname{atan2}(y,x)+\tfrac12\ln(x^2+y^2)\bigr)$ |

When both derivative coefficients contain one factor of the dependent field,
Mars applies the general dependent-square reduction. In
$z(az_x+bz_y)+r=0$, setting $w=z^2$ gives the linear
characteristic equation $aw_x+bw_y+2r=0$. Mars solves that equation
for `w` and returns both square-root branches for `z`.
When the transformed forcing is itself a characteristic invariant, Mars
multiplies it by a verified characteristic parameter. For the second example,
$y-x$ is invariant and $(x+y)/2$ advances at unit rate, giving the particular
term $y^2-x^2$ in `w`.
Mars applies the same substitution when a PDE has the reciprocal form
$az_x+bz_y=r/z$: multiplying by $2z$ gives
$aw_x+bw_y=2r$. For a trace-zero linear characteristic field
$a=Ax+By$, $b=Cx-Ay$, the quadratic
$Cx^2-2Axy-By^2$ is an invariant. Quadratic particular solutions are
matched by exact bivariate polynomial coefficients before they are accepted.

A missing derivative in one coordinate makes that coordinate a parameter:

```text
Dy(z) + 2*y*z = x*y^3
```

$\displaystyle\quad \begin{aligned} & \frac{d z}{d y} + 2\mkern-2mu y\mkern-2mu z = x\mkern-2mu y^{3} \\[1em] & z = \tfrac{x}{2}(y^2-1)+F(x)e^{-y^2} \end{aligned}$

`F` is a genuine arbitrary-function expression node. It renders in plain and
TeX output, participates in substitution, and differentiates by the chain
rule as $\frac{d}{dx}F(g(x))=F'(g(x))g'(x)$.

## Solving

`de_solve(...)` classifies and solves the problem where possible. A
well-formed problem that the current solver cannot handle still returns a
result object, with status `DE_SOLVE_STATUS_UNSUPPORTED`. This keeps
unsupported mathematics distinct from invalid input or allocation failure.

The current symbolic scope is:

- one-variable ODEs and two-variable transport PDEs;
- one dependent function;
- separable equations, including the directly invertible `y`, `ln(y)`, and
  quadratic dependent-factor forms;
- linear equations $\frac{dy}{dx}+P(x)y=Q(x)$, solved with the integrating factor
  $\mu(x)=\exp\!\left(\int P(x)\,dx\right)$;
- homogeneous equations $\frac{dy}{dx}=F(y/x)$, reduced with $y=ux$; and
- equations $\frac{dy}{dx}=F(ax+by+c)$, reduced with
  $u=ax+by+c$;
- equations depending on the ratio of two non-parallel affine expressions,
  translated to their intersection and then reduced to homogeneous form; and
- equations made separable by a nonsingular linear change
  $Y=ax+by$, $X=cx+dy$; and
- quadratic Bernoulli equations, reduced with $v=1/y$ and then solved as
  linear equations; and
- exact third-order forms $ay'''+ky'y''=f(x)$, integrated once and
  linearised with $u=e^{ky/(2a)}$; and
- selected modified-Emden equations, point-linearised to the free-particle
  equation and annotated with `SL(3, ℝ)` symmetry metadata; and
- regular second-order linear equations, normalised to Sturm–Liouville form,
  including affine Riccati factorisations with `erf` bases;
  and
- arbitrary-order constant-coefficient linear ODEs, including repeated and
  complex roots and nonhomogeneous forcing; and
- homogeneous and constant-forced constant-coefficient transport PDEs,
  including arbitrary-function families and explicit axis-aligned boundary
  data;
- the two-dimensional Laplace equation in Cartesian or polar coordinates
  without boundary data, returned as a general harmonic family;
- homogeneous second-order PDEs with real numeric constant coefficients,
  including distinct, repeated and complex characteristic roots, without
  boundary data;
- corresponding forced second-order PDEs with verified particular solutions,
  including affine exponential forcing, resonance and superposition;
- homogeneous radial Euler PDEs with quadratic coordinate coefficients and
  optional radial first-order and constant zeroth-order terms, on $x>0$
  without boundary data;
- the three-dimensional constant-speed wave equation, represented by
  Kirchhoff spherical means with two arbitrary smooth spatial functions;
- selected nonlinear and variable-coefficient first-order characteristic
  PDEs; and
- parameter-dependent first-order linear PDEs.

An initial condition is used to determine the integration constant. Without
one, the solution contains the arbitrary constant `C`. The linear solver uses
the expression module for exact symbolic integration. It may therefore return
elementary functions, supported special functions, or an exact unevaluated
integral when no closed form is available.

For example:

```text
Dx(y) = x*y^2; y(0) = 1
```

$\displaystyle\quad \begin{aligned} & \frac{d y}{d x} = x\mkern-2mu y^{2} \\[1em] & y = -\frac{1}{\tfrac12 x^2-1} \end{aligned}$

and:

```text
Dx(y) + y = x*y^2; y(0) = 1
```

$\displaystyle\quad \begin{aligned} & \frac{d y}{d x} + y = x\mkern-2mu y^{2} \\[1em] & y = \frac{1}{x+1} \end{aligned}$

A non-separable equation that requires an integrating factor is:

```text
Dx(y) + y/x = x^2; y(1) = 1
```

$\displaystyle\quad \begin{aligned} & \frac{d y}{d x} + \frac{y}{x} = x^{2} \\[1em] & y = \frac{x^4+3}{4x} \end{aligned}$

Here $P(x)=1/x$, so $\mu(x)=x$. Variable coefficients are not restricted to
rational functions; for example:

```text
Dx(y) + 2*x*y = exp(-x^2); y(0) = 0
```

$\displaystyle\quad \begin{aligned} & \frac{d y}{d x} + 2\mkern-2mu x\mkern-2mu y = e^{-x^{2}} \\[1em] & y = \frac{x}{e^{x^2}} \end{aligned}$

If the weighted forcing has no supported closed form, the solution remains
exact:

```text
Dx(y) + y = exp(cosh(x)); y(0) = 0
```

$\displaystyle\quad \begin{aligned} & \frac{d y}{d x} + y = e^{\cosh(x)} \\[1em] & y = e^{-x}\int^x e^{\cosh(t)+t}\,dt \end{aligned}$

## First-Order Homogeneous Equations

For an equation of the form

$\displaystyle\quad \frac{dy}{dx}=F\!\left(\frac{y}{x}\right)$

the solver substitutes $y=ux$, so that

$\displaystyle\quad \frac{dy}{dx}=u+x\frac{du}{dx}.$

It then solves the separable relation

$\displaystyle\quad \frac{du}{F(u)-u}=\frac{dx}{x}.$

For example:

```text
Dx(y) = y/x + x/y; y(1) = 1
```

$\displaystyle\quad \begin{aligned} & \frac{d y}{d x} = \frac{y}{x} + \frac{x}{y} \\[1em] & \tfrac12\left(\frac{y}{x}\right)^2 = \ln\lvert x\rvert+\tfrac12 \end{aligned}$

Homogeneous solutions may be implicit when the resulting relation cannot be
inverted uniquely without introducing branches. An initial condition at
$x=0$ is not accepted by this reduction because $y/x$ is undefined there.

## Linear Substitutions

When the right-hand side depends on a single affine combination,

$\displaystyle\quad \frac{dy}{dx}=F(ax+by+c),$

the solver uses

$\displaystyle\quad \begin{aligned}u&=ax+by+c,\\ \frac{du}{dx}&=a+bF(u).\end{aligned}$

The transformed equation is separable. For example:

```text
Dx(y) = (x + y)^2; y(0) = 0
```

$\displaystyle\quad \begin{aligned} & \frac{d y}{d x} = \left(x + y\right)^{2} \\[1em] & \arctan(x+y) = x \end{aligned}$

The same solver handles a nonlinear function of the ratio of two
non-parallel affine expressions. It first translates their intersection to
the origin, then applies the homogeneous substitution. For example:

```text
Dx(y) = ((y - 1)/(x + 2))^2 + (y - 1)/(x + 2)
```

$\displaystyle\quad \begin{aligned} & \frac{d y}{d x} = \frac{y - 1}{x + 2} + \left(\frac{y - 1}{x + 2}\right)^{2} \\[1em] & -\frac{x+2}{y-1} = \ln\lvert x+2\rvert+C \end{aligned}$

As with the homogeneous solver, these reductions generally produce implicit
solutions because solving explicitly for `y` may require branch choices.

## Linear Changes of Variables

For a nonsingular transformation

$\displaystyle\quad \begin{aligned}Y&=ax+by,\\ X&=cx+dy.\end{aligned}$

the solver computes

$\displaystyle\quad \frac{dY}{dX}=\frac{a+b\,dy/dx}{c+d\,dy/dx}.$

It substitutes the inverse transformation into the original right-hand side
and accepts the candidate only when the resulting equation in $X,Y$ is
separable. It then integrates in the transformed coordinates and substitutes
both linear forms back.

For example:

```text
Dx(y) = (1 - (x + y)*exp(x - y))/
        (1 + (x + y)*exp(x - y));
y(0) = 0
```

With $X=x+y$ and $Y=x-y$, this becomes

$\displaystyle\quad \frac{dY}{dX}=Xe^Y,$

and the returned solution is

$\displaystyle\quad \tfrac12(x+y)^2 = 1-e^{y-x}.$

The solver implementations are separated by differential-equation family:

```text
diffequ_solve_separable.c
diffequ_solve_linear.c
diffequ_solve_bernoulli.c
diffequ_solve_homogeneous.c
diffequ_solve_linear_subst.c
diffequ_solve_linear_transform.c
diffequ_solve_sturm_liouville.c
diffequ_solve_constant_linear.c
diffequ_pde_solve.c
diffequ_pde_laplace.c
diffequ_pde_transport.c
diffequ_pde_characteristics.c
diffequ_pde_linear.c
diffequ_pde_support.c
```

All files remain directly under `src/diffequation/`. `diffequ_solve.c`
contains the shared ODE/PDE entry point, while `diffequ_pde_solve.c` owns PDE
classification and dispatch. `diffequ_pde_internal.h` is the private boundary
between the shared entry point and the PDE solvers.

## Second-Order Linear Equations

For a regular second-order linear equation

$\displaystyle\quad A(x)\frac{d^2y}{dx^2}+B(x)\frac{dy}{dx}+C(x)y=R(x),$

the solver constructs the multiplier

$\displaystyle\quad \mu(x)=\exp\!\left(\int\frac{B(x)-A'(x)}{A(x)}\,dx\right).$

With $p=\mu A$, the equation becomes the self-adjoint relation

$\displaystyle\quad \frac{d}{dx}\left(p(x)\frac{dy}{dx}\right)+\mu(x)C(x)y=\mu(x)R(x).$

This Sturm–Liouville normalisation is valid on intervals where $A(x)\ne0$.
It is a canonical representation, not a promise that arbitrary coefficient
functions possess an elementary closed-form basis.

For a homogeneous equation in normal form, the solver recognises the affine
Riccati factorisation

$\displaystyle\quad \begin{aligned}y''-\bigl(\alpha(x)^2+\alpha'(x)\bigr)y&=0,\\ \alpha(x)&=ax+b,\qquad a>0.\end{aligned}$

This is

$\displaystyle\quad \bigl(D+\alpha(x)\bigr)\bigl(D-\alpha(x)\bigr)y=0,\qquad D=\frac{d}{dx}.$

The first basis function is $\exp\!\left(\int\alpha(x)\,dx\right)$; reduction of order
produces the second, which simplifies to an `erf` expression for affine
$\alpha$. For example:

```text
y'' - (x^2+1)*y = 0
```

$\displaystyle\quad \begin{aligned} & \frac{d^{2} y}{d x^{2}} - y\mkern-2mu \left(x^{2} + 1\right) = 0 \\[1em] & y = e^{x^2/2}\bigl(C_1+C_2\operatorname{erf}(x)\bigr) \end{aligned}$

With initial conditions, the arbitrary constants are eliminated in the same
way as for a constant-coefficient problem:

```text
y'' - (x^2+1)*y = 0; y(0) = 1; y'(0) = 0
```

$\displaystyle\quad \begin{aligned} & \frac{d^{2} y}{d x^{2}} - y\mkern-2mu \left(x^{2} + 1\right) = 0 \\[1em] & y = e^{x^2/2} \end{aligned}$

For a homogeneous positive power-law potential,

$\displaystyle\quad y''+ax^my=0,\qquad a>0,\quad m=1,2,\ldots$

the solver derives

$\displaystyle\quad \begin{aligned}\nu&=\frac{1}{m+2},\\ z&=2\nu\sqrt{a}\,x^{1/(2\nu)},\\ y&=\sqrt{x}\,u(z),\end{aligned}$

which reduces the equation to Bessel's equation

$\displaystyle\quad z^2u''+zu'+(z^2-\nu^2)u=0.$

It therefore returns the rule-generated basis

$\displaystyle\quad y=\sqrt{x}\bigl(C_1J_{-\nu}(z)+C_2J_\nu(z)\bigr).$

For example:

```text
y'' + x²*y = 0
```

$\displaystyle\quad \begin{aligned} & \frac{d^{2} y}{d x^{2}} + x^{2}\mkern-2mu y = 0 \\[1em] & y = \sqrt{x}\left(C_1J_{-1/4}\!\left(\tfrac12 x^2\right)+C_2J_{1/4}\!\left(\tfrac12 x^2\right)\right) \end{aligned}$

Here $J_\nu(z)$ denotes `BesselJ(ν, z)`.

The same derived substitution handles monomial forcing rather than rejecting
the equation at the homogeneous boundary. For

$\displaystyle\quad y''+ax^my=bx^n,$

put

$\displaystyle\quad \begin{aligned}p&=\frac{m+2}{2},\\ \mu&=\frac{2n-m+1}{m+2},\\ K&=\frac{b}{p^2}\left(\frac{p}{\sqrt{a}}\right)^{\mu+1}.\end{aligned}$

The transformed equation is the inhomogeneous Bessel equation

$\displaystyle\quad z^2u''+zu'+(z^2-\nu^2)u=Kz^{\mu+1},$

so the rule adds the Lommel particular solution $KS_{\mu,\nu}(z)$. For
example:

```text
y'' + x³*y = x
```

$\displaystyle\quad \begin{aligned} & \frac{d^{2} y}{d x^{2}} + x^{3}\mkern-2mu y = x \\[1em] & \begin{aligned} y = \sqrt{x}\Bigl(&C_1J_{-1/5}\!\left(\tfrac25 x^{5/2}\right) +C_2J_{1/5}\!\left(\tfrac25 x^{5/2}\right)\\ &+\tfrac25 S_{0,1/5}\!\left(\tfrac25 x^{5/2}\right)\Bigr). \end{aligned} \end{aligned}$

Here $S_{\mu,\nu}(z)$ denotes `LommelS(μ, ν, z)`.

Initial or boundary conditions on this special-function family are not yet
eliminated symbolically.

The constant-coefficient second-order family is

$\displaystyle\quad a\frac{d^2y}{dx^2}+b\frac{dy}{dx}+cy=0,$

where `a`, `b`, and `c` are constant. The solver uses the Liouville normal
form, equivalently the roots of $ar^2+br+c=0$. It treats distinct
real roots, a repeated root, and a complex-conjugate pair separately. Real
coefficients with complex roots are returned as real sine and cosine
solutions.

For example:

```text
Dxx(y) = y; y(0) = 1; y'(0) = 1
```

$\displaystyle\quad \begin{aligned} & \frac{d^{2} y}{d x^{2}} = y \\[1em] & y = e^x \end{aligned}$

```text
Dxx(y) + 2*Dx(y) + y = 0; y(0) = 1; y'(0) = 0
```

$\displaystyle\quad \begin{aligned} & \frac{d^{2} y}{d x^{2}} + 2\mkern-2mu \frac{d y}{d x} + y = 0 \\[1em] & y = (x+1)e^{-x} \end{aligned}$

```text
Dxx(y) + y = 0; y(0) = 0; y'(0) = 1
```

$\displaystyle\quad \begin{aligned} & \frac{d^{2} y}{d x^{2}} + y = 0 \\[1em] & y = \sin x \end{aligned}$

If a variable-coefficient equation is successfully normalised but the
module cannot yet construct its fundamental solution basis, `de_solve(...)`
tries the polynomial Taylor fallback before returning
`DE_SOLVE_STATUS_UNSUPPORTED`. Boundary-value spectra,
nonhomogeneous variation of parameters, and special-function bases are the
next layers rather than being guessed from the normalised form.

## Local Taylor Series

When closed-form rules do not complete a scalar second-order ODE, MARS can
construct a local Taylor expansion directly from its polynomial normal form
$y''=f(x,y,p)$, where $p=y'$. This is a general coefficient rule, not a table
of series for particular equations.

$\displaystyle\quad \begin{aligned}
&h=x-x_0,\qquad y(x)=\sum_{n=0}^{\infty}c_nh^n,\\[0.5em]
&c_0=y(x_0),\qquad c_1=y'(x_0),\\[0.5em]
&(n+1)(n+2)c_{n+2}=[h^n]f(x_0+h,y,y').
\end{aligned}$

Here $[h^n]$ means the coefficient of $h^n$. Native truncated polynomial
convolutions compute only the required coefficients.

The equivalent derivative formula uses $D$, the **total derivative along a
solution**, not another unknown. Since $y'=p$ and $p'=f(x,y,p)$, the chain rule
gives, for any function $g(x,y,p)$,

$\displaystyle\quad Dg=\frac{d}{dx}g(x,y(x),p(x))
=\partial_xg+p\partial_yg+f\partial_pg.$

Each partial derivative holds the other coordinates fixed. Applying $D$
repeatedly generates the higher derivatives: $y''=f$, $y'''=Df$, and
$y^{(4)}=D^2f$. Thus $c_n=y^{(n)}(x_0)/n!
=(D^{n-2}f)(x_0,c_0,c_1)/n!$ for $n\ge2$. This explains the coefficients;
the implementation obtains them by the coefficient-matching recurrence above.

The native solver collects coefficients in the initial-data symbols after
each order and displays them beside their powers of $h$, rather than exposing
the nested expressions used to construct them. Series are displayed in ascending
powers of $h$, omitting zero coefficients and placing the remainder last, in
both text and TeX. This also applies when initial data give numeric coefficients.

The automatic fallback retains terms through degree six. Use
`de_solve_series()` to request a degree from two through eight. The result
has status `DE_SOLVE_STATUS_SERIES` and solver `DE_SOLVER_TAYLOR_SERIES`,
and includes an explicit $O(h^{N+1})$ remainder. It is **not an exact finite
polynomial solution or a closed form**. Polynomial normal forms guarantee
local convergence for finite initial data, but MARS does not supply a
convergence radius or a numerical truncation-error bound.

The highest-derivative coefficient must be a finite non-zero numeric
constant, and the normal form must be polynomial of degree at most 15 in
each of $x,y,p$. Other symbols represent finite constant parameters.
Non-polynomial normal forms and singular leading coefficients are not
handled by this fallback. Value and/or slope conditions must be at a single
common point; other conditions, including boundary data at different
points, are not silently discarded. Missing initial values remain arbitrary
constants. With no conditions, the expansion point is zero.

For the quartic equation without conditions, write $a=y(0)$ and $b=y'(0)$.
The series starts with

$\displaystyle\quad y=a+bx-\frac{3ab+a^4}{2}x^2
+\frac{9a^2b-3b^2-4a^3b+3a^5}{6}x^3+O(x^4).$

Both constants are retained: MARS does not assume that every solution
crosses $y=0$. Zero initial value and slope also include the equilibrium
solution. A simpler concrete example, requested through degree eight, is:

```text
y'' + 3*y*y' + y^4 = 0; y(0)=0; y'(0)=1
```

$\displaystyle\quad \begin{aligned}
&y''+3yy'+y^4=0,\qquad y(0)=0,\quad y'(0)=1,\\[1em]
&y=x-\frac{x^3}{2}+\frac{3x^5}{10}-\frac{x^6}{30}
-\frac{51x^7}{280}+\frac{27x^8}{560}+O(x^9).
\end{aligned}$

The MARS Lab displays the native expansion as **Equation and local series**
and reports **Local series**, preserving the distinction from an exact solve.

## Arbitrary-Order Constant-Coefficient Equations

For

$\displaystyle\quad a_n\frac{d^ny}{dx^n}+\cdots+a_1\frac{dy}{dx}+a_0y=f(x),$

the solver constructs $P(r)=a_nr^n+\cdots+a_1r+a_0$ and obtains all
of its roots through the equation module. A real root `r` of multiplicity `m`
contributes

$\displaystyle\quad e^{rx},\quad xe^{rx},\quad\ldots,\quad x^{m-1}e^{rx}.$

A conjugate pair $\alpha\pm\beta i$ contributes the equivalent real sine/cosine basis.
This works above degree four as well; it does not depend on radical formulae.

For nonzero `f(x)`, the solver constructs the Wronskian and applies variation
of parameters. The expression integrator simplifies the resulting integrals
where possible and otherwise retains exact formal integrals.

A complete set of `n` independent initial or boundary conditions is solved as
a symbolic matrix system. With no conditions, the result retains
$C_1,\ldots,C_n$. Underspecified condition sets are not yet parameterised.

For example, distinct real roots give:

```text
Dxxx(y) - 6*Dxx(y) + 11*Dx(y) - 6*y = 0
```

$\displaystyle\quad \begin{aligned} & \frac{d^{3} y}{d x^{3}} - 6\mkern-2mu \frac{d^{2} y}{d x^{2}} + 11\mkern-2mu \frac{d y}{d x} - 6\mkern-2mu y = 0 \\[1em] & y = C_1e^{3x}+C_2e^{2x}+C_3e^x \end{aligned}$

A repeated real root gives:

```text
Dxxx(y) - 3*Dxx(y) + 3*Dx(y) - y = 0
```

$\displaystyle\quad \begin{aligned} & \frac{d^{3} y}{d x^{3}} - 3\mkern-2mu \frac{d^{2} y}{d x^{2}} + 3\mkern-2mu \frac{d y}{d x} - y = 0 \\[1em] & y = e^x(C_3x^2+C_2x+C_1) \end{aligned}$

Mixed real and complex roots give:

```text
Dxxx(y) - Dxx(y) + Dx(y) - y = 0
```

$\displaystyle\quad \begin{aligned} & \frac{d^{3} y}{d x^{3}} - \frac{d^{2} y}{d x^{2}} + \frac{d y}{d x} - y = 0 \\[1em] & y = C_1e^x+C_2\cos x+C_3\sin x \end{aligned}$

A repeated complex-conjugate pair gives:

```text
Dxxxx(y) + 2*Dxx(y) + y = 0
```

$\displaystyle\quad \begin{aligned} & \frac{d^{4} y}{d x^{4}} + 2\mkern-2mu \frac{d^{2} y}{d x^{2}} + y = 0 \\[1em] & y = C_1\cos x+C_2\sin x+C_3x\cos x+C_4x\sin x \end{aligned}$

The sixth-order example exercises real and complex roots together:

```text
Dxxxxxx(y) - 4*Dxxxx(y) - Dxx(y) + 4*y = 0
```

$\displaystyle\quad \begin{aligned} & \frac{d^{6} y}{d x^{6}} - 4\mkern-2mu \frac{d^{4} y}{d x^{4}} - \frac{d^{2} y}{d x^{2}} + 4\mkern-2mu y = 0 \\[1em] & \begin{aligned} y &={} C_1e^x+C_2e^{2x}+C_3e^{-x}+C_4e^{-2x}\\ &+C_5\cos x+C_6\sin x. \end{aligned} \end{aligned}$

Nonhomogeneous equations retain the complementary arbitrary constants:

```text
Dxx(y) - y = exp(2*x)
```

$\displaystyle\quad \begin{aligned} & \frac{d^{2} y}{d x^{2}} - y = e^{2\mkern-2mu x} \\[1em] & y = \tfrac13\left(e^{2x}+3C_1e^x+3C_2e^{-x}\right) \end{aligned}$

Forcing is not restricted to second order:

```text
Dxxx(y) - Dx(y) = exp(2*x)
```

$\displaystyle\quad \begin{aligned} & \frac{d^{3} y}{d x^{3}} - \frac{d y}{d x} = e^{2\mkern-2mu x} \\[1em] & y = \tfrac16\left(e^{2x}+6C_2+6C_1e^x+6C_3e^{-x}\right) \end{aligned}$

Complete initial conditions determine all of those constants. Examples
include:

```text
Dxxx(y) - Dx(y) = 0;
y(0) = 1; y'(0) = 1; y''(0) = 1
```

$\displaystyle\quad \begin{aligned} & \frac{d^{3} y}{d x^{3}} - \frac{d y}{d x} = 0 \\[1em] & y = e^x \end{aligned}$

```text
Dxxxx(y) + 2*Dxx(y) + y = 0;
y(0) = 1; y'(0) = 0; y''(0) = -1; y'''(0) = 0
```

$\displaystyle\quad \begin{aligned} & \frac{d^{4} y}{d x^{4}} + 2\mkern-2mu \frac{d^{2} y}{d x^{2}} + y = 0 \\[1em] & y = \cos x \end{aligned}$

```text
Dxx(y) - y = exp(2*x); y(0) = 0; y'(0) = 0
```

$\displaystyle\quad \begin{aligned} & \frac{d^{2} y}{d x^{2}} - y = e^{2\mkern-2mu x} \\[1em] & y = \tfrac16\left(2e^{2x}-3e^x+e^{-x}\right) \end{aligned}$

## Example: Solving a Separable ODE

```c
#include <stdio.h>
#include <stdlib.h>

#include "diffequation.h"

int main(void)
{
    const char *source = "Dx(y) = x*y; y(0) = 1";
    diffequ_t *ode = de_from_string(source);
    diffequ_solve_result_t *result = de_solve_with_options(
        ode, DE_SOLVE_OPTION_STEPS);
    const equation_t *solution = de_solve_result_at(result, 0);
    char *problem_text = de_to_string(ode, style_EXPRESSION);
    string_t *solution_text = equ_to_text(solution, style_UNBOUND);

    printf("input = %s\n", source);
    printf("problem = %s\n", problem_text);
    printf("solution = %s\n", string_c_str(solution_text));

    string_free(solution_text);
    free(problem_text);
    de_solve_result_free(result);
    de_free(ode);
    return 0;
}
```

```text
input = Dx(y) = x*y; y(0) = 1
problem = { dy/dx = x*y | x = ?; ; y(0) = 1 }
solution = y = exp(½x²)
```

## Example: Linearising a Lie-Symmetric ODE

For the modified Emden equation

$$
y''+3yy'+y^3=0,
$$

introduce an auxiliary function $u(x)$ by setting $y=u^{-1}u'$ on an interval
where $u\ne0$. The left-hand side becomes

$$
y''+3yy'+y^3=u^{-1}u'''.
$$

The equation therefore reduces to $u'''=0$, so $u=Ax^2+Bx+C$. Substituting
back gives

$$
y(x)=\frac{2Ax+B}{Ax^2+Bx+C}.
$$

A common non-zero factor in $A,B,C$ cancels. For $A\ne0$, divide through by
$A$ and write $C_1=B/A$, $C_2=C/A$. MARS returns this two-constant family:

$$
y(x)=\frac{2x+C_1}{x^2+C_1x+C_2},
\qquad x^2+C_1x+C_2\ne0.
$$

The unnormalised form also includes $y=1/(x+C_0)$ when $A=0$, $B\ne0$, and
$y=0$ when $A=B=0$, $C\ne0$. These are not represented by finite constants
in MARS's normalised family.

The reported $\mathrm{SL}(3,\mathbb{R})$ symmetry also admits the local point
transformation

$$
X=x-\frac{1}{y},\qquad Y=\frac{x}{y}-\frac{x^2}{2},
\qquad \frac{d^2Y}{dX^2}=0,
$$

where $y\ne0$ and $X$ can serve as the independent variable. This is an
alternative linearisation; it is not an additional step needed for the
logarithmic-derivative solution above.

The API example prints just the input, symmetry and solution. To retrieve the
derivation as well, use `de_solve_with_options(ode, DE_SOLVE_OPTION_STEPS)`
and `de_solve_result_steps()` or `de_solve_result_steps_TeX()`.

```c
#include <stdio.h>

#include "diffequation.h"

int main(void)
{
    const char *source = "y'' + 3*y*y' + y^3 = 0";
    diffequ_t *ode = de_from_string(source);
    diffequ_solve_result_t *result = de_solve(ode);
    const equation_t *solution = de_solve_result_at(result, 0);
    const char *symmetry = de_solve_result_symmetry(result);
    string_t *solution_text = equ_to_text(solution, style_UNBOUND);

    printf("input = %s\n", source);
    printf("symmetry = %s\n", symmetry);
    printf("solution = %s\n", string_c_str(solution_text));

    string_free(solution_text);
    de_solve_result_free(result);
    de_free(ode);
    return 0;
}
```

```text
input = y'' + 3*y*y' + y^3 = 0
symmetry = SL(3, ℝ)
solution = y = (2x + C₁)/(x² + C₁x + C₂)
```

## Example: Deriving a Lie Algebra

This example derives and verifies the free-particle generators; it does not
retrieve the existing solver's group-name metadata.

```c
#include <stdio.h>
#include "diffequation.h"

int main(void)
{
    diffequ_t *ode = de_from_string("y'' = 0");
    de_lie_t *lie = de_lie_new(ode);
    matrix_t *generators = de_lie_polynomial_generators(lie, 2);
    matrix_t *constants = de_lie_structure_constants(lie, generators);
    int status = 1;

    if (generators && constants) {
        printf("polynomial generators = %zu\n", mat_get_col_count(generators));
        printf("structure constants = %zu x %zu\n",
               mat_get_row_count(constants), mat_get_col_count(constants));
        status = 0;
    }

    mat_free(constants);
    mat_free(generators);
    de_lie_free(lie);
    de_free(ode);
    return status;
}
```

```text
polynomial generators = 8
structure constants = 64 x 8
```

## API Reference

All declarations are in `diffequation.h`. A `diffequ_t` holds the **problem**;
a `diffequ_solve_result_t` holds the **outcome of one solve attempt**.
Parsing successfully does not guarantee that a symbolic solver supports the
equation.

The usual sequence is to parse a problem, solve it, inspect the status, and
then read the solution equations or diagnostic. See the
[complete C example](#example-solving-a-separable-ode) above.

All indices below are zero-based. **Borrowed** objects and strings must not
be freed by the caller; they remain valid only while their owning problem or
result remains alive. Newly allocated objects require the matching release
function described below.

### Creating and releasing a problem

#### `de_from_string()` and `de_from_text()`

```c
diffequ_t *de_from_string(const char *text);
diffequ_t *de_from_text(const string_t *text);
```

Parse a problem from a null-terminated C string or a MARS string, respectively.
Both accept the [input forms](#input-forms) described above, including
independent-variable declarations, constants and initial or boundary conditions.
The parser infers independent variables from derivative notation when they
are not explicitly declared.

Return a newly allocated problem, or `NULL` for invalid input or allocation
failure. The input string remains owned by the caller. Release the returned
problem with `de_free()`.

#### `de_new()`

```c
diffequ_t *de_new(const equation_t *equation);
```

Create a problem from an existing equation. This creates a new equation
wrapper retaining the original left- and right-hand expressions; it does
not take ownership of the supplied equation.

Unlike the parsing functions, this constructor does not infer or add
independent variables, constants or conditions. A problem created this way
therefore has zero entries in those collections and is not, by itself,
ready for the current solvers, which require an independent variable.

Return a newly allocated problem, or `NULL` if `equation` is `NULL` or
allocation fails. Release the problem with `de_free()`.

#### `de_free()`

```c
void de_free(diffequ_t *de);
```

Release a problem and all of its owned equations, bindings, conditions and
coordinate expressions. Passing `NULL` is harmless. Any borrowed pointers
obtained from that problem must no longer be used.

### Inspecting a problem

#### `de_equation()`

```c
const equation_t *de_equation(const diffequ_t *de);
```

Return the borrowed base differential equation, without the surrounding
declarations or conditions. Use the equation module to inspect its two sides
or format it. Return `NULL` when `de` is `NULL`.

#### `de_independent_count()`

```c
size_t de_independent_count(const diffequ_t *de);
```

Return the number of independent coordinates recorded in the problem,
including those inferred by the parser. This is not the order of the
differential equation or the number of arbitrary constants in its solution.

Return zero when `de` is `NULL`. Use `de_independent_at()` to inspect each
coordinate.

#### `de_independent_at()`

```c
const expr_t *de_independent_at(const diffequ_t *de, size_t index);
```

Return the borrowed coordinate expression at `index`. Valid indices are
strictly less than `de_independent_count(de)`. Return `NULL` for a null
problem or an out-of-range index.

#### `de_constants()` and `de_constant()`

```c
expr_bindings_t *de_constants(const diffequ_t *de);
expr_t *de_constant(const diffequ_t *de, const char *name);
```

`de_constants()` returns the borrowed collection of problem-constant
bindings, or `NULL` if no collection exists or the problem is null.

`de_constant()` looks up one constant by its null-terminated name and returns
its borrowed expression. It returns `NULL` if the constant is absent or
either argument is null. These are constants attached to the input problem,
not the arbitrary integration constants introduced by a solver.

### Inspecting initial and boundary conditions

#### `de_condition_count()`

```c
size_t de_condition_count(const diffequ_t *de);
```

Return the number of initial or boundary equations supplied with the problem.
A condition is counted once, regardless of how many coordinate arguments
its function application contains. Return zero for a null problem or one
with no conditions.

#### `de_condition_at()`

```c
const equation_t *de_condition_at(const diffequ_t *de, size_t index);
```

Return the borrowed condition equation at `index`. Valid indices are
strictly less than `de_condition_count(de)`. Return `NULL` for a null
problem or an out-of-range index.

#### `de_condition_argument_count()` and `de_condition_argument_at()`

```c
size_t de_condition_argument_count(const diffequ_t *de, size_t condition_index);
const expr_t *de_condition_argument_at(const diffequ_t *de, size_t condition_index, size_t argument_index);
```

Inspect the coordinate arguments of a condition's function application,
rather than its prescribed value. The boundary example above with
`u(x, 0) = x^2` has two arguments: the expressions `x` and `0`;
its right-hand side is available through `de_condition_at()`.

`de_condition_argument_count()` returns the number of these arguments, or
zero for a null problem, an invalid condition index or a condition without
local arguments.

`de_condition_argument_at()` returns a borrowed argument expression.
It returns `NULL` if the problem is null or either index is out of range.

### Formatting a problem

#### `de_to_text()` and `de_to_string()`

```c
string_t *de_to_text(const diffequ_t *de, style_t style);
char *de_to_string(const diffequ_t *de, style_t style);
```

Format the input problem, not a solver's answer. To format a solution, obtain
its equation with `de_solve_result_at()` and use the equation module.

| Style | Output |
| :--- | :--- |
| `style_EXPRESSION` | Curly-brace problem notation, including independent variables, constants and conditions. Derivatives use ordinary or partial-derivative fractions in Unicode text. |
| `style_UNBOUND` | The base equation alone, using formal derivative notation, without the surrounding declarations or conditions. |
| `style_LATEX` | The base equation as TeX, without declarations or conditions. |

`de_to_text()` returns a newly allocated `string_t`; release it with
`string_free()`. `de_to_string()` returns a newly allocated C string;
release it with `free()`. Allocation failure returns `NULL`.

Passing a null problem produces the literal text `NULL`, provided the
output allocation succeeds; it does not itself cause a null return value.

### Solving a problem

#### `de_solve()` and `de_solve_with_options()`

```c
diffequ_solve_result_t *de_solve(const diffequ_t *de);
diffequ_solve_result_t *de_solve_with_options(const diffequ_t *de, unsigned int options);
```

Attempt a symbolic solve without taking ownership of the problem.
Both return a newly allocated result, which must be released with
`de_solve_result_free()`. Check for `NULL` before inspecting the result:
it means a result object could not be allocated.

`de_solve()` is the default entry point and does not construct presentation
derivations. `de_solve_with_options()` performs the same mathematical solve
and accepts these flags:

| Option | Effect |
| :--- | :--- |
| `DE_SOLVE_OPTION_NONE` | Solve without constructing derivation text. |
| `DE_SOLVE_OPTION_STEPS` | Also construct plain-text and TeX derivations for the result accessors. |

A well-formed but unsupported equation still returns a result object.
A null problem returns an invalid result if that result can be allocated.
Always inspect `de_solve_result_status()`; a non-null result does not
necessarily contain a solution.

#### `de_solve_series()`

```c
diffequ_solve_result_t *de_solve_series(const diffequ_t *de, size_t degree, unsigned int options);
```

Explicitly request the [local Taylor method](#local-taylor-series), even when
another solver could find a closed form. `degree` is the highest retained
power, from 2 through 8 inclusive; invalid degrees or a null problem return
an invalid result. `options` has the same meaning as for
`de_solve_with_options()`. Initial data determine the centre; with no data
the centre is zero and both initial values are arbitrary constants.
Unsupported normal forms or condition sets return an unsupported result.
The owning result must be released with `de_solve_result_free()`.

#### Series result accessors

```c
const expr_t *de_solve_result_series_centre(const diffequ_solve_result_t *result);
size_t de_solve_result_series_degree(const diffequ_solve_result_t *result);
const expr_t *de_solve_result_series_coefficient(const diffequ_solve_result_t *result, size_t index);
```

These expose the expansion point $x_0$, the retained degree $N$, and the
coefficient $c_{\text{index}}$ multiplying $(x-x_0)^{\text{index}}$.
The coefficients do **not** include the remainder; the equation returned by
`de_solve_result_at()` does. Expressions are borrowed and remain valid until
the result is freed. A null or non-series result returns `NULL` for expressions
and zero for the degree. An index greater than the degree returns `NULL`.

#### `de_solve_result_free()`

```c
void de_solve_result_free(diffequ_solve_result_t *result);
```

Release the result, its solution equations and its diagnostic, derivation
and symmetry strings. Passing `NULL` is harmless. Borrowed pointers from
this result must no longer be used.

### Reading the outcome and solutions

#### `de_solve_result_status()`

```c
de_solve_status_t de_solve_result_status(const diffequ_solve_result_t *result);
```

Return the outcome of the solve attempt:

| Status | Meaning |
| :--- | :--- |
| `DE_SOLVE_STATUS_SOLVED` | A symbolic solution was produced. It may be implicit, contain arbitrary functions or constants, or retain exact unevaluated integrals. This does not promise an elementary closed form. |
| `DE_SOLVE_STATUS_SERIES` | A local Taylor expansion was produced, with an explicit remainder. The finite polynomial alone is not asserted to solve the equation exactly. |
| `DE_SOLVE_STATUS_UNSUPPORTED` | The problem is well formed, but the available symbolic rules could not complete it. This does not mean that the equation has no solution. |
| `DE_SOLVE_STATUS_INVALID` | The solve input is invalid. This is also the value returned by this accessor for a null result. |
| `DE_SOLVE_STATUS_FAILED` | The solve encountered an internal construction or allocation failure. Inspect the diagnostic for details. |

#### `de_solve_result_solver()`

```c
de_solver_t de_solve_result_solver(const diffequ_solve_result_t *result);
```

Return the solver-family identifier recorded in the result, such as
`DE_SOLVER_LINEAR`, `DE_SOLVER_CHARACTERISTICS` or
`DE_SOLVER_CONSTANT_COEFFICIENT_LINEAR`. This identifies the method, not
whether it succeeded; use the status accessor for that decision.

Return `DE_SOLVER_NONE` when no family is recorded or the result is null.
The full set of family identifiers is declared by `de_solver_t` in
`diffequation.h`.

#### `de_solve_result_count()`

```c
size_t de_solve_result_count(const diffequ_solve_result_t *result);
```

Return the number of symbolic solution equations stored in the result.
One equation can describe an entire family through arbitrary constants or
functions; this is not a count of individual numerical solutions.

Distinct branches can occupy separate entries. Inspect every entry with
`de_solve_result_at()` rather than assuming that only the first matters.
Return zero when the result is null or contains no solution equations.

#### `de_solve_result_at()`

```c
const equation_t *de_solve_result_at(const diffequ_solve_result_t *result, size_t index);
```

Return the borrowed solution equation at `index`. Valid indices are
strictly less than `de_solve_result_count(result)`. Return `NULL` when
the result is null or the index is out of range.

Do not assume that the dependent variable has been isolated: a solution may
be an implicit equation. The result owns the equation; do not call
`equ_free()` on this borrowed pointer.

### Diagnostics and presentation

#### `de_solve_result_diagnostic()`

```c
const char *de_solve_result_diagnostic(const diffequ_solve_result_t *result);
```

Return a borrowed, null-terminated message describing the outcome. This may
explain a successful method, an unsupported case or a failure. It does not
require `DE_SOLVE_OPTION_STEPS`. Return `NULL` if no diagnostic exists or
the result is null. Use the status enum, not the wording of this message,
for program logic.

#### `de_solve_result_steps()` and `de_solve_result_steps_TeX()`

```c
const char *de_solve_result_steps(const diffequ_solve_result_t *result);
const char *de_solve_result_steps_TeX(const diffequ_solve_result_t *result);
```

Return borrowed derivations for presentation. The first returns multiline
UTF-8 text with Unicode mathematical notation; the second returns TeX.
They explain the selected rule and the result for the supplied equation.

Request these outputs by solving with `DE_SOLVE_OPTION_STEPS`. Without
that flag, or for a null result, they return `NULL`. An unsolved scalar
second-order ODE may still have a Lie-analysis derivation, including computed
invariants, verified polynomial generators and an autonomous order reduction.
Other unsupported problems may have no derivation. Clients should display
the returned mathematics without parsing or reinterpreting it.

#### `de_solve_result_symmetry()`

```c
const char *de_solve_result_symmetry(const diffequ_solve_result_t *result);
```

Return the borrowed UTF-8 name of a symmetry group identified by the solver.
The modified-Emden examples above report `SL(3, ℝ)`. Return `NULL` when
no group was identified or the result is null. An absent name is not proof
that the equation has no symmetry; see [Lie-symmetry analysis](#lie-symmetry-analysis).

### Analysing Lie point symmetries

This API analyses a scalar second-order ODE independently of `de_solve()`.
It does not turn a symmetry certificate into a claim that the ODE has been
solved. The [Lie-algebra example](#example-deriving-a-lie-algebra) shows the
construction and cleanup sequence.

#### `de_lie_new()` and `de_lie_free()`

```c
de_lie_t *de_lie_new(const diffequ_t *de);
void de_lie_free(de_lie_t *lie);
```

Construct an owning normal form for one independent variable and an equation
linear in its second derivative. Return `NULL` for an unsupported normal form,
invalid input or allocation failure. The analysis owns its expressions, so the
original problem may be freed afterwards. Conditions do not constrain this
equation-level analysis. `de_lie_free(NULL)` is harmless.

#### `de_lie_coordinate()` and `de_lie_rhs()`

```c
const expr_t *de_lie_coordinate(const de_lie_t *lie, size_t index);
const expr_t *de_lie_rhs(const de_lie_t *lie);
```

Borrow the independent coordinate (index 0), dependent coordinate (1), velocity
(2), or normalised right-hand side. In the usual notation these are $x,y,p,f$;
the input's actual variable names are preserved and the velocity name avoids
collisions. Invalid indices and null analyses return `NULL`.

Construct supplied generator components using these coordinate expressions,
so their variable identities agree with the analysis. Do not free the borrowed
expressions; they remain valid until the analysis is freed.

#### `de_lie_prolongation()` and `de_lie_residual()`

```c
expr_t *de_lie_prolongation(const de_lie_t *lie,
                          const expr_t *xi, const expr_t *eta, size_t order);
expr_t *de_lie_residual(const de_lie_t *lie, const expr_t *xi, const expr_t *eta);
```

Supply the two components of a point generator. They may use general native
expressions but must not depend on the velocity. `order` is 1 or 2; the returned
prolongation coefficient is evaluated on the equation manifold, $y''=f$.
The residual is the left-hand side of the determining equation above. An
identically zero result verifies the supplied generator. A non-zero symbolic
expression is not a certificate. Results are owning expressions, released with
`expr_free()`; invalid input or failure returns `NULL`.

#### `de_lie_determining_equation()` and `de_lie_invariant()`

```c
equation_t *de_lie_determining_equation(const de_lie_t *lie);
expr_t *de_lie_invariant(const de_lie_t *lie, size_t index);
```

The first function constructs the determining PDE with unknown functions
$\xi(x,y)$ and $\eta(x,y)$; it does not generally solve that PDE. Release the
owning equation with `equ_free()`. The second computes $I_1$ for index 0 and
$I_2$ for index 1. Release its owning expression with `expr_free()`. Both return
`NULL` on invalid input or failure. Vanishing must be an identity, not merely
a numerical value at one point.

#### `de_lie_polynomial_generators()`

```c
matrix_t *de_lie_polynomial_generators(const de_lie_t *lie, size_t degree);
```

Search all polynomial components of total degree at most `degree` (0–4).
Return an owning expression matrix: row 0 contains $\xi$, row 1 contains
$\eta$, and each column is one verified generator. The basis order is not an
API guarantee. A zero-column matrix is a successful empty search; `NULL` means
unsupported input or failure. In particular, non-polynomial residuals or
unresolved symbolic parameters in their coefficients are not silently treated
as zero. The finite real coefficient and degree-15 limits described above
apply. Release the matrix with `mat_free()`.

#### `de_lie_bracket()` and `de_lie_structure_constants()`

```c
matrix_t *de_lie_bracket(const de_lie_t *lie, const matrix_t *generators,
                       size_t i, size_t j);
matrix_t *de_lie_structure_constants(const de_lie_t *lie, const matrix_t *generators);
```

Supply a two-row expression matrix of point-vector-field components using the
analysis coordinates; both dense and sparse matrices may use structural zero
entries. `de_lie_bracket()` returns the two components of
$[G_i,G_j]=G_iG_j-G_jG_i$ as a two-row, one-column matrix. Indices are zero-based.
It accepts non-polynomial components too.

For an independent, closed polynomial basis with $n$ columns,
`de_lie_structure_constants()` returns an $n^2$-by-$n$ matrix. Entry
`(i*n+j, k)` is the constant $C_{ij}^{k}$ in
$[G_i,G_j]=\sum_k C_{ij}^{k}G_k$. The basis may contain at most 30 columns;
the degree-15 and finite real coefficient limits apply. Every reconstructed
bracket is checked exactly. An empty basis returns an empty $0$-by-$0$ matrix.
Dependent or non-closed bases return `NULL`, as do
unsupported inputs or failures. These operations compute vector-field brackets;
use `de_lie_residual()` to verify externally supplied fields as ODE symmetries.
Release either result with `mat_free()`.

#### `de_lie_autonomous_reduction()`

```c
equation_t *de_lie_autonomous_reduction(const de_lie_t *lie);
```

When $f$ is independent of $x$, return the owning reduced equation
$p\,dp/dy=f(y,p)$. The substitution is $p(y)=dy/dx$, so the chain rule gives
$y''=(dp/dy)(dy/dx)=p\,dp/dy$. The velocity is thus treated as a function of
the original dependent coordinate. This is a local order reduction, not a completed solve;
check equilibrium solutions separately. Return `NULL` for a non-autonomous
equation, null analysis or failure. Release the result with `equ_free()`.
