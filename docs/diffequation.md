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
linear equations are normalized to self-adjoint Sturm–Liouville form.
Constant-coefficient linear ODEs of arbitrary order are solved through their
characteristic polynomial and variation of parameters.

## Ownership

- `de_new(...)`, `de_from_string(...)`, and `de_from_text(...)` return owning
  `diffequ_t *` handles.
- `de_free(...)` releases an owning handle and accepts `NULL`.
- `de_equation(...)`, `de_independent_at(...)`, `de_constants(...)`,
  `de_constant(...)`, and `de_condition_at(...)` return borrowed objects owned
  by the differential equation.
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

## Solving

`de_solve(...)` classifies and solves the problem where possible. A
well-formed problem that the current solver cannot handle still returns a
result object, with status `DE_SOLVE_STATUS_UNSUPPORTED`. This keeps
unsupported mathematics distinct from invalid input or allocation failure.

The current symbolic scope is:

- one independent variable;
- one first-order dependent function;
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
- regular second-order linear equations, normalized to Sturm–Liouville form;
  and
- arbitrary-order constant-coefficient linear ODEs, including repeated and
  complex roots and nonhomogeneous forcing.

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

The solver implementations are separated by ODE family:

```text
diffequ_solve_separable.c
diffequ_solve_linear.c
diffequ_solve_bernoulli.c
diffequ_solve_homogeneous.c
diffequ_solve_linear_subst.c
diffequ_solve_linear_transform.c
diffequ_solve_sturm_liouville.c
diffequ_solve_constant_linear.c
```

`diffequ_solve.c` contains the shared classification and dispatch path.

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

The first completed second-order family is

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
problem = { Dx(y) = x*y | x = ?; ; y(0) = 1 }
solution = y = exp(½x²)
```
