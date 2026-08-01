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
linear equations are normalized to self-adjoint Sturm–Liouville form, with
closed-form solutions for constant coefficients and affine Riccati
factorizations.
Constant-coefficient linear ODEs of arbitrary order are solved through their
characteristic polynomial and variation of parameters.
The first PDE solver handles two-variable, constant-coefficient homogeneous
transport equations with explicit axis-aligned boundary data.

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
- `de_solve(...)` returns an owning `diffequ_solve_result_t *`, released with
  `de_solve_result_free(...)`.
- Equations returned by `de_solve_result_at(...)` are borrowed from the solve
  result.

The public declarations are in:

```c
#include "diffequation.h"
```

## Input Forms

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
is normalized to the formal derivative notation used internally:

```text
y'(0) = 1  →  Dx(y)(0) = 1
```

For ordinary differential equations in `x`, prime notation can also be used
directly in the equation:

```text
y'' + 4y = e^x
→ Dxx(y) + 4y = e^x
→ y = ⅕·exp(x) + C₁·cos(2x) + C₂·sin(2x)
```

The standard forms `dy/dx`, `d²y/dx²`, and `d³y/dx³` are accepted as
equivalent input aliases. In the Mars Lab, ordinary derivatives are displayed
this way in both the rendered equation and the differential-equation card.
The canonical expression form remains `Dx(y)`, `Dxx(y)`, and `Dxxx(y)`.

Additive forcing terms are solved independently and then combined:

```text
y'' + 4y = e^x + x^3
→ y = ⅕·exp(x) + ¼x³ - ⅜x + C₁·cos(2x) + C₂·sin(2x)
```

Autonomous first-order equations that are quadratic in the derivative can
produce multiple implicit branches and a singular solution:

```text
(y')^2 = y' + 2y
→ x = ½·(√(8y + 1) - ln(|½·(√(8y + 1) + 1)|) + 1) + C
→ x = ½·(1 - √(8y + 1) - ln(|½·(1 - √(8y + 1))|)) + C
→ y = 0
```

Exact third-order nonlinear forms can be integrated once and linearized. For
example,

```text
y''' + y''*y' = 3x^2
→ y = 2·ln(|Σ_(n=0)^∞ c_(n)·x^n|)
  c_(0) = C₂
  c_(1) = C₃
  c_(-1) = c_(-2) = c_(-3) = 0
  c_(n + 2) = (C₁·c_(n) + c_(n - 3))/(2·(n + 2)·(n + 1))
```

Here the original left side is
`Dx(y'' + ½(y')²)`. After one integration, the substitution
`u = exp(y/2)` cancels the quadratic derivative term and gives
`u'' = ½(x³ + C₁)u`. The displayed recurrence is obtained by substituting
`u = Σ c_n x^n`; it is a complete convergent power-series solution and does
not introduce nonstandard special-function names. The independent constants
`C₁`, `C₂`, and `C₃` provide the three arbitrary constants required by the
original third-order equation.

When `x` is the dependent variable, prime notation defaults to differentiation
with respect to time so that the two variables do not collide:

```text
x'' + x = 0
→ Dtt(x) + x = 0
→ x = C₁·cos(t) + C₂·sin(t)
```

## Mars Lab

The **Differential Equation** tab accepts the same shorthand. Evaluation is a
thin-client call to the native `diffequation` module: the Lab displays the
normalized problem, selected solver family, diagnostic, and every symbolic
solution returned by `de_solve(...)`. **Use as input** restores the original
problem, including its initial or boundary conditions.

## First-Order Transport PDEs

Ordinary and partial differential equations share the same `diffequ_t`.
Multiple declared independent variables make the derivative notation partial:

```text
u_x       Dx(u)
u_y       Dy(u)
u_xy      Dxy(u) = Dy(Dx(u))
u_xx      Dxx(u)
phi_x     Dx(@phi)
xzz_x     x*z*Dx(z)
```

Subscript suffixes are read from left to right. The shorthand is local to the
differential-equation parser; ordinary expression identifiers containing an
underscore are unchanged. In compact PDE products such as `xzz_x`, leading
coordinate factors remain multiplicative coefficients and the suffix applies
to the final field: `x*z*z_x`.
The expression and unbound styles retain `Dx(u)` for round-trip input, while
TeX output uses standard partial-derivative fractions such as
`\\frac{\\partial u}{\\partial x}`. Repeated and mixed derivatives render as
`\\frac{\\partial^2 u}{\\partial x^2}` and
`\\frac{\\partial^2 u}{\\partial y\\,\\partial x}`.
The Mars Lab problem card likewise uses the Unicode partial-derivative symbol,
displaying `∂u/∂x`, `∂²u/∂x²`, and `∂²u/∂y∂x`. These standard Unicode
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

```text
a*Dx(u) + b*Dy(u) = q,
```

the characteristic invariant is `b*x - a*y`. For `q = 0`, the value of `u`
is constant along each characteristic. With explicit data on `y = y0`, the
solver transports the boundary expression along those curves. The example
above therefore gives:

```text
u = (x - 2y)²
```

Boundary data on a constant-`x` line is supported as well:

```text
{
    Dx(u) + 3*Dy(u) = 0
    | x = ?, y = ?;
    ;
    u(0, y) = exp(y)
}
→ u = exp(y - 3x)
```

Affine parameterized boundary curves are supported when they are
non-characteristic. For example:

```text
Dx(z) + Dy(z) = 2*z*(x+y); z(x, 1-x) = x^2
→ z = 1/4*(x-y+1)^2*exp(1/2*((x+y)^2-1))
```

Here `x+y` changes along each characteristic while `y-x` is invariant. The
boundary curve `y=1-x` determines the formerly arbitrary function of that
invariant.

Boundary applications retain each coordinate separately. For `u(x, 0)`,
`de_condition_argument_count(...)` returns two, and
`de_condition_argument_at(...)` returns the borrowed `x` and `0` expressions.

Without boundary data, Mars returns the general solution using the ordinary
arbitrary-function notation `F`:

```text
Dt(u) + c*Dx(u) = 0
→ u = F(x - c*t)

Dt(u) + c*Dx(u) = 1
→ u = t + F(x - c*t)

Dx(z) + Dy(z) = z
→ z = exp(x)*F(y - x)

Dx(z) + Dy(z) + z = x
→ z = x - 1 + exp(-x)*F(y - x)

Dx(@phi) - Dy(@phi) = sin(x) + cos(y)
→ φ = F(x + y) - cos(x) - sin(y)

Dx(z) + Dy(z) = cos(x+y)
→ z = F(y - x) + 1/2*sin(x+y)

Dx(z) + 3*Dy(z) - 2*z + 4*y^2 - 22*y + 4*x + 13 = 0
→ z = exp(2*x)*F(y - 3*x) + 2*x - 5*y + 2*y^2

2*Dx(@phi) + Dy(@phi) + 6*@phi = 37*sin(y)
→ φ = exp(-3*x)*F(y - x/2) + 6*sin(y) - cos(y)

Dx(@phi) + Dy(@phi) + Dz(@phi) = @phi
→ φ = exp(x)*F(y - x, z - x)
```

More generally, constant transport accepts
`a*Dx(u) + b*Dy(u) + p*u = q(x,y)`. The reaction coefficient `p` produces
exponential evolution along each characteristic. Mars substitutes the
characteristic path into `q` and applies the one-dimensional integrating
factor along that path. Constant forcing produces the corresponding constant
particular solution. The same evolution is applied when transporting explicit
axis-aligned boundary data with constant forcing.

When `p` is nonzero and `q(x,y)` is polynomial, Mars inverts the transport
operator directly. If `D = a*Dx + b*Dy`, then repeated applications of `D`
eventually annihilate the polynomial, so

```text
(p + D)^(-1)q
```

is evaluated as a finite derivative series. This avoids introducing an
unevaluated characteristic integral for an elementary polynomial solution.
For trigonometric, hyperbolic, and exponential forcing, Mars instead
recognizes a function space closed under the full directional derivative
`D = a*Dx + b*Dy`. If `D^2*f = lambda*f`, it solves the transport equation
algebraically and verifies the result against the original operator. This
works whether the phase uses one coordinate or a mixture such as `x+y`, and
keeps elementary answers elementary rather than exposing an internal
characteristic integral.

When the forcing is an integrable unary function of a phase `g(x,y)` and
`D(g)` is a nonzero coordinate-independent value, Mars instead integrates the
unary function with respect to its phase and divides by `D(g)`. The candidate
is again accepted only after substitution into the complete PDE. For example,

```text
Dx(z) + 2*Dy(z) = tanh(x+y)
→ z = F(y - 2x) + 1/3*ln(cosh(x+y))
```

The homogeneous constant-transport and constant-forcing rules extend to any
number of independent variables. For

```text
a1*Dx1(u) + ... + an*Dxn(u) + p*u = q,
```

Mars chooses a nonzero transport direction as the characteristic parameter
and constructs the other `n - 1` independent invariants. The arbitrary
function therefore has `n - 1` arguments; it is not collapsed into a
one-variable approximation.

Aliases for standard constants are contextual in derivative operands.
Consequently, `@phi` ordinarily denotes the golden ratio, but in
`Dx(@phi)` or `Dy(@phi)` it denotes the dependent field `φ`. The same rule
allows familiar mathematical symbols to be used for angles and other
dependent quantities without changing their ordinary expression meaning.

The characteristic solver also handles these nonlinear and
variable-coefficient forms:

```text
x^2*Dx(@psi) - x*y*Dy(@psi) + y*@psi = 0
→ ψ = exp(y/(2*x))*F(x*y)

x*Dx(z) - 7*y*Dy(z) = 5*x^2*y
→ z = F(x^7*y) - x^2*y

x*y*Dx(z) - x^2*Dy(z) + y*z = 3*x^2*y
→ z = F(x^2 + y^2)/x + x^2

Dx(@phi)*sec(x) + Dy(@phi) = cot(y)
→ φ = F(y - sin(x)) + ln(sin(y))

x*(y-z)*z_x + y*(z-x)*z_y = z*(x-y)
→ F(x + y + z, x*y*z) = 0

x*(y^2-z^2)*z_x + y*(z^2-x^2)*z_y = z*(x^2-y^2)
→ F(x^2 + y^2 + z^2, x*y*z) = 0
```

For a monomial characteristic field with `b/a = k*y/x`, Mars constructs the
invariant `y*x^(-k)`. For a homogeneous linear reaction term, it then derives
an exponential multiplier. For an inhomogeneous equation, it symbolically
integrates a candidate particular solution along a nonzero characteristic
direction and permits only a coordinate-independent rescaling. Mars
substitutes every candidate back into the complete transport operator; the
solution is accepted only when that symbolic verification reduces exactly to
zero.

Mars also tests the radial invariant `x^2 + y^2` when the characteristic
field is tangent to its level curves. The invariant, reaction multiplier, and
particular term are each verified against the original differential
operator.

For a positive-integer cyclic Lagrange field proportional to
`(x*(y^n-z^n), y*(z^n-x^n), z*(x^n-y^n))`, Mars finds the two independent
first integrals `x^n+y^n+z^n` and `x*y*z`. The general integral is therefore
emitted as the implicit arbitrary relation
`F(x^n+y^n+z^n, x*y*z) = 0`.

For a separable field `a(x)*u_x + b(y)*u_y`, Mars integrates the coordinate
potentials `A'(x) = 1/a(x)` and `B'(y) = 1/b(y)`. Their difference `B-A` is a
characteristic invariant. Each potential, and the resulting particular term,
is accepted only after differentiation and exact substitution into the
original PDE. Thus `sec(x)*phi_x + phi_y = cot(y)` uses
`A(x) = sin(x)`, `B(y) = y`, and the particular integral `ln(sin(y))`.

```text
Dx(z) + Dy(z) = 6*(x+y)^2*z^2
→ z = 1/(F(x - y) - (x + y)^3)
→ z = 0

x^2*Dx(z) + y^2*Dy(z) = z^2
→ z = 1/(F(1/x - 1/y) + 1/x)
→ z = 0

x*z*Dx(z) + y*z*Dy(z) + x^2 + y^2 = 0
→ z = √(F(y/x) - x^2 - y^2)
→ z = -√(F(y/x) - x^2 - y^2)

z*z_x + z*z_y = y - x
→ z = √(F(y - x) - x^2 + y^2)
→ z = -√(F(y - x) - x^2 + y^2)

(y-x)*z_x + (y+x)*z_y = (x^2+y^2)/z
→ z = √(F(x^2 + 2*x*y - y^2) + 2*x*y)
→ z = -√(F(x^2 + 2*x*y - y^2) + 2*x*y)

(x+y)*Dx(z) + (y-x)*Dy(z) = 0
→ z = F(atan2(y,x) + 1/2*ln(x^2+y^2))
```

When both derivative coefficients contain one factor of the dependent field,
Mars applies the general dependent-square reduction. In
`z*(a*z_x + b*z_y) + r = 0`, setting `w = z^2` gives the linear
characteristic equation `a*w_x + b*w_y + 2*r = 0`. Mars solves that equation
for `w` and returns both square-root branches for `z`.
When the transformed forcing is itself a characteristic invariant, Mars
multiplies it by a verified characteristic parameter. For the second example,
`y-x` is invariant and `(x+y)/2` advances at unit rate, giving the particular
term `y^2-x^2` in `w`.
Mars applies the same substitution when a PDE has the reciprocal form
`a*z_x + b*z_y = r/z`: multiplying by `2*z` gives
`a*w_x + b*w_y = 2*r`. For a trace-zero linear characteristic field
`a = A*x+B*y`, `b = C*x-A*y`, the quadratic
`C*x^2-2*A*x*y-B*y^2` is an invariant. Quadratic particular solutions are
matched by exact bivariate polynomial coefficients before they are accepted.

A missing derivative in one coordinate makes that coordinate a parameter:

```text
Dy(z) + 2*y*z = x*y^3
→ z = x/2*(y^2 - 1) + F(x)*exp(-y^2)
```

`F` is a genuine arbitrary-function expression node. It renders in plain and
TeX output, participates in substitution, and differentiates by the chain
rule as `F'(g(x))*g'(x)`.

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
- linear equations `Dx(y) + P(x)*y = Q(x)`, solved with the integrating factor
  `μ(x) = exp(∫P(x)dx)`;
- homogeneous equations `Dx(y) = F(y/x)`, reduced with `y = u*x`; and
- equations `Dx(y) = F(a*x + b*y + c)`, reduced with
  `u = a*x + b*y + c`;
- equations depending on the ratio of two non-parallel affine expressions,
  translated to their intersection and then reduced to homogeneous form; and
- equations made separable by a nonsingular linear change
  `Y = a*x + b*y`, `X = c*x + d*y`; and
- quadratic Bernoulli equations, reduced with `v = 1/y` and then solved as
  linear equations; and
- exact third-order forms `a*y''' + k*y'*y'' = f(x)`, integrated once and
  linearized with `u = exp(k*y/(2a))`; and
- regular second-order linear equations, normalized to Sturm–Liouville form,
  including affine Riccati factorizations with `erf` bases;
  and
- arbitrary-order constant-coefficient linear ODEs, including repeated and
  complex roots and nonhomogeneous forcing; and
- homogeneous and constant-forced constant-coefficient transport PDEs,
  including arbitrary-function families and explicit axis-aligned boundary
  data;
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
→ y = -1/(½x² - 1)
```

and:

```text
Dx(y) + y = x*y^2; y(0) = 1
→ y = 1/(x + 1)
```

A non-separable equation that requires an integrating factor is:

```text
Dx(y) + y/x = x^2; y(1) = 1
→ y = (x⁴ + 3)/(4x)
```

Here `P(x) = 1/x`, so `μ(x) = x`. Variable coefficients are not restricted to
rational functions; for example:

```text
Dx(y) + 2*x*y = exp(-x^2); y(0) = 0
→ y = x/exp(x²)
```

If the weighted forcing has no supported closed form, the solution remains
exact:

```text
Dx(y) + y = exp(cosh(x)); y(0) = 0
→ y = ∫^x exp(cosh(t) + t)·dt/exp(x)
```

## First-Order Homogeneous Equations

For an equation of the form

```text
Dx(y) = F(y/x)
```

the solver substitutes `y = u*x`, so that

```text
Dx(y) = u + x*Dx(u).
```

It then solves the separable relation

```text
du/(F(u) - u) = dx/x.
```

For example:

```text
Dx(y) = y/x + x/y; y(1) = 1
→ ½·(y/x)² = ln(|x|) + 0.5
```

Homogeneous solutions may be implicit when the resulting relation cannot be
inverted uniquely without introducing branches. An initial condition at
`x = 0` is not accepted by this reduction because `y/x` is undefined there.

## Linear Substitutions

When the right-hand side depends on a single affine combination,

```text
Dx(y) = F(a*x + b*y + c),
```

the solver uses

```text
u = a*x + b*y + c,
du/dx = a + b*F(u).
```

The transformed equation is separable. For example:

```text
Dx(y) = (x + y)^2; y(0) = 0
→ atan(x + y) = x
```

The same solver handles a nonlinear function of the ratio of two
non-parallel affine expressions. It first translates their intersection to
the origin, then applies the homogeneous substitution. For example:

```text
Dx(y) = ((y - 1)/(x + 2))^2 + (y - 1)/(x + 2)
→ -(x + 2)/(y - 1) = ln(|x + 2|) + C
```

As with the homogeneous solver, these reductions generally produce implicit
solutions because solving explicitly for `y` may require branch choices.

## Linear Changes of Variables

For a nonsingular transformation

```text
Y = a*x + b*y
X = c*x + d*y
```

the solver computes

```text
dY/dX = (a + b*Dx(y))/(c + d*Dx(y)).
```

It substitutes the inverse transformation into the original right-hand side
and accepts the candidate only when the resulting equation in `X,Y` is
separable. It then integrates in the transformed coordinates and substitutes
both linear forms back.

For example:

```text
Dx(y) = (1 - (x + y)*exp(x - y))/
        (1 + (x + y)*exp(x - y));
y(0) = 0
```

With `X = x + y` and `Y = x - y`, this becomes

```text
dY/dX = X*exp(Y),
```

and the returned solution is

```text
½·(x + y)² = 1 - exp(-(x - y)).
```

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

```text
A(x)*Dxx(y) + B(x)*Dx(y) + C(x)*y = R(x),
```

the solver constructs the multiplier

```text
μ(x) = exp(∫(B(x) - A'(x))/A(x) dx).
```

With `p = μ*A`, the equation becomes the self-adjoint relation

```text
(p(x)*Dx(y))' + μ(x)*C(x)*y = μ(x)*R(x).
```

This Sturm–Liouville normalization is valid on intervals where `A(x) != 0`.
It is a canonical representation, not a promise that arbitrary coefficient
functions possess an elementary closed-form basis.

For a homogeneous equation in normal form, the solver recognizes the affine
Riccati factorization

```text
y'' - (alpha(x)^2 + alpha'(x))*y = 0,
alpha(x) = a*x + b,  a > 0.
```

This is

```text
(D + alpha(x))*(D - alpha(x))*y = 0.
```

The first basis function is `exp(integral(alpha(x), x))`; reduction of order
produces the second, which simplifies to an `erf` expression for affine
`alpha`. For example:

```text
y'' - (x^2+1)*y = 0
→ y = exp(½x²)·(C₁ + C₂·erf(x))
```

With initial conditions, the arbitrary constants are eliminated in the same
way as for a constant-coefficient problem:

```text
y'' - (x^2+1)*y = 0; y(0) = 1; y'(0) = 0
→ y = exp(½x²)
```

The constant-coefficient second-order family is

```text
a*Dxx(y) + b*Dx(y) + c*y = 0,
```

where `a`, `b`, and `c` are constant. The solver uses the Liouville normal
form, equivalently the roots of `a*r^2 + b*r + c = 0`. It treats distinct
real roots, a repeated root, and a complex-conjugate pair separately. Real
coefficients with complex roots are returned as real sine and cosine
solutions.

For example:

```text
Dxx(y) = y; y(0) = 1; y'(0) = 1
→ y = exp(x)
```

```text
Dxx(y) + 2*Dx(y) + y = 0; y(0) = 1; y'(0) = 0
→ y = (x + 1)*exp(-x)
```

```text
Dxx(y) + y = 0; y(0) = 0; y'(0) = 1
→ y = sin(x)
```

If a variable-coefficient equation is successfully normalized but the
module cannot yet construct its fundamental solution basis, `de_solve(...)`
returns `DE_SOLVE_STATUS_UNSUPPORTED`. Boundary-value spectra,
nonhomogeneous variation of parameters, and special-function bases are the
next layers rather than being guessed from the normalized form.

## Arbitrary-Order Constant-Coefficient Equations

For

```text
a[n]*D[x^n](y) + ... + a[1]*Dx(y) + a[0]*y = f(x),
```

the solver constructs `P(r) = a[n]*r^n + ... + a[1]*r + a[0]` and obtains all
of its roots through the equation module. A real root `r` of multiplicity `m`
contributes

```text
exp(r*x), x*exp(r*x), ..., x^(m-1)*exp(r*x).
```

A conjugate pair `α ± βi` contributes the equivalent real sine/cosine basis.
This works above degree four as well; it does not depend on radical formulae.

For nonzero `f(x)`, the solver constructs the Wronskian and applies variation
of parameters. The expression integrator simplifies the resulting integrals
where possible and otherwise retains exact formal integrals.

A complete set of `n` independent initial or boundary conditions is solved as
a symbolic matrix system. With no conditions, the result retains
`C1, ..., Cn`. Underspecified condition sets are not yet parameterized.

For example, distinct real roots give:

```text
Dxxx(y) - 6*Dxx(y) + 11*Dx(y) - 6*y = 0
→ y = C₁·exp(3x) + C₂·exp(2x) + C₃·exp(x)
```

A repeated real root gives:

```text
Dxxx(y) - 3*Dxx(y) + 3*Dx(y) - y = 0
→ y = exp(x)·(C₃x² + C₂x + C₁)
```

Mixed real and complex roots give:

```text
Dxxx(y) - Dxx(y) + Dx(y) - y = 0
→ y = C₁·exp(x) + C₂·cos(x) + C₃·sin(x)
```

A repeated complex-conjugate pair gives:

```text
Dxxxx(y) + 2*Dxx(y) + y = 0
→ y = C₁·cos(x) + C₂·sin(x) + C₃x·cos(x) + C₄x·sin(x)
```

The sixth-order example exercises real and complex roots together:

```text
Dxxxxxx(y) - 4*Dxxxx(y) - Dxx(y) + 4*y = 0
→ y = C₁·exp(x) + C₂·exp(2x) + C₃·exp(-x) + C₄·exp(-2x)
    + C₅·cos(x) + C₆·sin(x)
```

Nonhomogeneous equations retain the complementary arbitrary constants:

```text
Dxx(y) - y = exp(2*x)
→ y = ⅓·(exp(2x) + 3C₁·exp(x) + 3C₂·exp(-x))
```

Forcing is not restricted to second order:

```text
Dxxx(y) - Dx(y) = exp(2*x)
→ y = ⅙·(exp(2x) + 6C₂ + 6C₁·exp(x) + 6C₃·exp(-x))
```

Complete initial conditions determine all of those constants. Examples
include:

```text
Dxxx(y) - Dx(y) = 0;
y(0) = 1; y'(0) = 1; y''(0) = 1
→ y = exp(x)
```

```text
Dxxxx(y) + 2*Dxx(y) + y = 0;
y(0) = 1; y'(0) = 0; y''(0) = -1; y'''(0) = 0
→ y = cos(x)
```

```text
Dxx(y) - y = exp(2*x); y(0) = 0; y'(0) = 0
→ y = ⅙*(2*exp(2*x) - 3*exp(x) + exp(-x))
```

## Example: Solving a Separable ODE

```c
#include <stdio.h>
#include <stdlib.h>

#include "diffequation.h"

int main(void)
{
    const char *source = "Dx(y) = x*y; y(0) = 1";
    diffequ_t *ode = de_from_string(source);
    diffequ_solve_result_t *result = de_solve(ode);
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
