# Differential Equation Syntax

> Design note: construction, parsing, inspection, formatting, and the first
> symbolic separable, linear, homogeneous, affine-substitution,
> linear-coordinate-transformation, quadratic Bernoulli, and
> arbitrary-order constant-coefficient linear solvers are implemented.
> The first PDE solver handles constant-coefficient transport equations,
> arbitrary-function characteristic families, selected nonlinear and
> variable-coefficient characteristic equations, and parameter-dependent
> first-order linear PDEs. General
> variable-coefficient bases, numerical integration, systems, and broader PDE
> solving remain future work.

Differential-equation support sits above the existing expression/equation
split:

1. `expr_t` remains the evaluable and transformable expression DAG.
2. `equation_t` remains the relationship-solving object built from expressions.
3. `diffequ_t` represents a differential-equation problem: a base equation,
   independent variables, constant bindings, and initial/boundary conditions.

The implementation module should live in `src/diffequation/`, with a public
`include/diffequation.h` header. The public type is opaque:

```c
typedef struct diffequ_t diffequ_t;
```

Public differential-equation functions use the `de_` prefix. Construction,
destruction, and borrowed accessors form the base API:

```c
diffequ_t *de_new(const equation_t *equation);
diffequ_t *de_from_string(const char *text);
diffequ_t *de_from_text(const string_t *text);
void de_free(diffequ_t *de);

const equation_t *de_equation(const diffequ_t *de);

size_t de_independent_count(const diffequ_t *de);
const expr_t *de_independent_at(const diffequ_t *de, size_t index);

expr_bindings_t *de_constants(const diffequ_t *de);
expr_t *de_constant(const diffequ_t *de, const char *name);

size_t de_condition_count(const diffequ_t *de);
const equation_t *de_condition_at(const diffequ_t *de, size_t index);
```

`de_new()` borrows its argument for the duration of the call and constructs its
own equation wrapper. The wrapper retains the source expression DAG, following
the existing expression/equation ownership model, so callers may free the
source equation after construction. Serialising and reparsing is not used as a
substitute for ownership.

All accessor results are borrowed and remain owned by `diffequ_t`. In
particular, callers must not free the equation, independent-variable
expressions, constant bindings, or condition equations returned by these
functions. The initial implementation is immutable after construction. Builder
or mutation functions can be added later if a concrete use case requires them.

An internal representation can mirror the source syntax directly:

```c
struct diffequ_t {
    equation_t *equation;

    expr_t **independent_vars;
    size_t independent_count;

    expr_bindings_t *constants;

    equation_t **conditions;
    size_t condition_count;
};
```

The public type remains opaque, so this representation can later change to
hold a system of equations without breaking callers. The first implementation
supports one base equation. Systems of coupled equations should eventually use
an ordered collection of base equations rather than encode several relations
inside one expression.

This keeps constant bindings separate from the base equation bindings. In

```text
{ Dxx(y) + y = 0 | x = ?; A = 1, C = 0; y(0) = A, Dx(y)(0) = C }
```

`x = ?` declares an independent variable, `A` and `C` are fixed problem
parameters, and the final section contains equations that constrain the
solution.

The equation, constants, and conditions must share one symbol table. Every
occurrence of `x`, `y`, `A`, or `C` in the problem therefore resolves to the
same corresponding expression node. Parsing each section independently and
joining the resulting objects afterwards would break substitution, evaluation,
and derivative identity.

Symbols have distinct roles:

1. An **independent variable** is declared with `?` in the first section.
2. A **problem constant** is declared in the second section.
3. A **dependent function** is inferred from formal derivative applications or
   function-value conditions such as `Dx(y)` or `y(0)`.
4. A **standard mathematical constant**, such as `@pi`, remains a standard
   expression constant and is never added to the problem constants.

The `?` marker belongs to the differential-equation and Lab syntax. It means
"declared but not assigned" and must not change the expression module's use of
`NAN` for an unavailable numeric value.

For ordinary differential equations, a compact input form avoids restating
information already present in the derivative operator:

```text
Dxx(y) = y; y(0) = 1; y'(0) = 1
```

The derivative identifies `x` as the independent variable. Semicolon-separated
relations after the base equation are conditions, and prime notation is
accepted as convenient condition syntax. The parser normalises this example
to the explicit representation:

```text
{ Dxx(y) = y | x = ?;; y(0) = 1, Dx(y)(0) = 1 }
```

`Dx(y)(0)` is accepted directly as well. The explicit braced form remains
necessary when constants or other problem metadata must be declared.

Derivative syntax is implemented first at expression level. `Dx(expr)` means
"differentiate `expr` with respect to `x`". If the argument is an evaluable
expression, the expression parser may lower it immediately:

```text
Dx(exp(x^2)) -> 2x·exp(x²)
Dxx(x^3)    -> 6x
Dxy(x^2y^3) -> 6xy²
```

Formal derivatives of unknown dependent functions, such as `Dx(y)`, are
preserved as formal derivative expression nodes.

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

## Parsing and Round Trips

The differential-equation parser owns the outer braces and the three
semicolon-delimited sections. It delegates ordinary expressions and relations
to the expression and equation parsers while supplying their shared symbol
table and the symbol roles established by the outer syntax.

The canonical expression-style representation is:

```text
{ equation | independent variables; constant bindings; conditions }
```

Serialised output must parse back to an equivalent problem without changing:

- derivative order;
- symbol roles or identity;
- the order of independent variables and conditions;
- exact constants into decimal approximations; or
- an unassigned `?` into `NAN`.

Whitespace and line breaks are insignificant outside identifiers. A
semicolon always separates the three outer sections; commas separate entries
within a section. Nested expression parentheses and function arguments do not
participate in those outer separators.

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

The differential-equation input layer also accepts conventional subscript
notation:

```text
u_x         Dx(u)
u_y         Dy(u)
u_xy        Dxy(u) = Dy(Dx(u))
u_xx        Dxx(u)
phi_x       Dx(@phi)
xzz_x       x*z*Dx(z)
```

For shorthand problems, the suffix letters infer the independent variables.
For fully specified problem strings, every suffix letter must name a declared
independent variable. Normalization happens only in the differential-equation
parser so it does not reinterpret underscore-bearing identifiers in the
general expression module. When a compact product begins with a declared
coordinate, the final base letter is the differentiated field and the leading
letters remain an implicit product. Thus `xzz_x` means `x*z*Dx(z)`, not
`Dx(xzz)`.

The canonical and unbound text forms retain the `D...(...)` notation so they
round-trip through the parser. When a differential equation has more than one
independent variable, its TeX renderer instead emits standard partial
fractions: `\\partial u/\\partial x`, `\\partial^2u/\\partial x^2`, and
`\\partial^2u/(\\partial y\\,\\partial x)`. This display choice is scoped to
the differential-equation renderer and does not alter expression trees.

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

The explicit `D...(...)` notation remains available when the variable of
differentiation would otherwise be ambiguous.

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
recognised derivative-variable names and ASCII or superscript repetition
digits between them. If it finds one, the sequence is parsed as a derivative
application:

```text
Dx(y)
Dx²y³(u)
```

If another character intervenes, or no opening `(` follows, the sequence is
not recognised as the canonical derivative form. In particular, bare `Dx`
remains an ordinary symbol, such as the `x` component of a vector `D`.

### Formal Derivative Representation

An immediately evaluable derivative and a formal derivative are different
operations:

```text
Dx(exp(x^2))    immediately differentiates a known expression
Dx(y)           denotes a derivative of the unknown dependent function y
```

The expression layer therefore needs formal function-application and formal
derivative nodes. A formal derivative node records:

- the dependent function;
- the ordered list of differentiation variables; and
- the total derivative order.

It must not store only a count for each variable, because that would lose the
declared order of mixed derivatives such as `Dxyx(u)`.

The parser lowers a derivative application immediately only when its argument
is an ordinary evaluable expression. If its argument is a dependent function,
or contains an unresolved formal function application, it preserves a formal
derivative node. Simplification may combine identical formal derivatives but
must never turn them into numeric zero merely because the dependent function
currently has no value.

Postfix evaluation, as in `Dx(y)(0)`, is a formal application with local
argument bindings. It does not mutate the global value of `x`.

## Conditions

A derivative can be evaluated using the same function-style notation as the
dependent variable:

```text
y(0) = A
Dx(y)(0) = C
u(x, 0) = A
Dy(u)(x, 1) = sin(x)
```

Postfix evaluation by itself is still an expression-level operation. For
example:

```text
Dx(f(x))(0)
```

is equivalent to evaluating the derivative expression with a local point
binding:

```text
{ Dx(f(x)) | x = 0 }
```

Once the evaluated derivative is related to another expression, it becomes an
equation condition rather than a plain expression:

```text
Dx(f(x))(0) = 1
```

is represented as:

```text
{ Dx(f(x)) = 1 | ; x = 0 }
```

The empty first binding section is intentional: `x = 0` is not declaring an
independent variable there, it is the local point binding for the condition.

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

## Validation

Construction and parsing should distinguish a malformed problem from a
well-formed problem that no available solver can handle. The former is an
error; the latter remains a valid `diffequ_t`.

Structural validation rejects:

- repeated independent-variable declarations;
- a symbol declared as both an independent variable and a constant;
- derivative indices that do not name declared independent variables;
- conditions referring to an unknown dependent function;
- duplicate constant names with conflicting values; and
- malformed or empty equations.

The validator should also determine and retain useful metadata:

- dependent functions in first-appearance order;
- highest derivative order for each dependent function;
- whether the problem is an ODE or PDE;
- whether all conditions are initial conditions at one point; and
- whether the base relation can be isolated into an explicit first-order
  system.

An underdetermined, overdetermined, implicit, stiff, boundary-value, or PDE
problem is not necessarily structurally invalid. Solver selection reports
unsupported problem classes separately.

## One Representation for ODEs and PDEs

Ordinary and partial differential equations use the same `diffequ_t` and
formal derivative nodes. They are not separate expression languages:

```text
Dx(y)                  ordinary derivative when y = y(x)
Dx(u), Dy(u)           partial derivatives when u = u(x, y)
Dxx(u), Dxy(u), Dyy(u) higher and mixed partial derivatives
```

The distinction follows from the number of independent variables and the
declared arguments of each dependent function, not from a different `D`
operator. Classification metadata selects an appropriate numerical solver or
symbolic analysis; it does not change the stored equation.

This common representation is also needed for future symmetry-based analysis
of difficult ODEs. Lie point-symmetry methods introduce infinitesimal
coefficient functions such as `ξ(x, y)` and `η(x, y)`. Their determining
equations are generally linear PDEs even when the original problem is an ODE.
A symmetry calculation may therefore:

1. construct the prolonged action of a candidate vector field;
2. derive determining PDEs for its infinitesimal coefficients;
3. solve or simplify those PDEs;
4. integrate the characteristic equations to obtain invariant coordinates;
5. construct a point transformation; and
6. substitute the transformation into the original ODE and verify whether the
   transformed equation is linear or otherwise reduced.

The formal derivative representation should consequently be suitable for
finite jet expressions: dependent functions, ordered derivative indices, and
substitution of transformed independent and dependent variables. A later
symbolic-analysis layer will also need internal total-derivative operators for
prolongation. Those total derivatives are analysis operations over a jet
expression and should not be confused with merely evaluating the user-facing
formal node `Dx(u)`.

Mixed derivative order remains preserved in parsed and rendered source. An
analysis may canonicalise `Dxy(u)` and `Dyx(u)` only when it has explicitly
established the smoothness assumptions under which the derivatives commute.

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

## Text Output

`style_EXPRESSION` uses the canonical, round-trippable syntax. `style_TEX`
uses the mathematical rendering described above. A future `style_FUNCTION`
form should remain compact and operate on the complete problem rather than
reconstructing its components in client code:

```text
diffequation de(x) {
    return {
        Dxx(y) + 3 * Dx(y) + 2 * y = sin(x)
        | x = ?;
        A = 1, C = 0;
        y(0) = A, Dx(y)(0) = C
    };
}

output(de(x).solve());
```

The function-style form remains a presentation target; the C API currently
returns the solve result directly.

## Symbolic Solver Boundary

The initial solver scope began with symbolic first-order ODEs. `de_solve()`
isolates one formal first derivative, then attempts these solver families in
order:

1. **Separable:** write `dy/dx = f(x)g(y)`, integrate
   `1/g(y)` with respect to `y` and `f(x)` with respect to `x`, then apply an
   initial condition where present.
2. **Linear:** write `dy/dx + P(x)y = Q(x)`, construct the integrating factor
   `μ(x) = exp(∫P(x)dx)`, and return
   `y = (∫μ(x)Q(x)dx + C)/μ(x)`.
3. **Quadratic Bernoulli:** write
   `dy/dx = A(x)y + Q(x)y²`, substitute `v = 1/y`, solve
   `dv/dx + A(x)v = -Q(x)` with the linear machinery, and transform back with
   `y = 1/v`.
4. **First-order homogeneous:** recognise `dy/dx = F(y/x)` by substituting
   `y = ux` and verifying that the transformed right-hand side is independent
   of `x`. Then integrate
   `du/(F(u) - u) = dx/x` and substitute `u = y/x` back into the result.
5. **Linear substitution:** recognise a right-hand side that becomes a
   function of one temporary variable after
   `u = ax + by + c`. Integrate
   `du/(a + bF(u)) = dx`, then substitute the affine expression back.
6. **Shifted homogeneous:** when the right-hand side is a nonlinear function
   of the ratio of two non-parallel affine expressions, solve their two linear
   equations for the common intersection `(h, k)`. Translate
   `X = x - h`, `Y = y - k`, verify that the transformed right-hand side
   depends only on `Y/X`, and apply the homogeneous reduction.
7. **Linear change of variables:** collect linear forms occurring in the
   equation and test nonsingular ordered pairs
   `Y = ax + by`, `X = cx + dy`. Use the inverse transformation to rewrite
   the right-hand side and compute
   `dY/dX = (a + b(dy/dx))/(c + d(dy/dx))`. Accept the pair only if the
   transformed derivative separates into a factor of `X` and a factor of
   `Y`; integrate the separated equation and substitute the original linear
   forms back.

Second-order linear equations have a separate dispatch path. For

```text
A(x)y'' + B(x)y' + C(x)y = R(x),
```

the leading coefficient must be nonzero on the working interval. The
integrating multiplier

```text
μ(x) = exp(∫(B(x) - A'(x))/A(x) dx)
```

gives `p = μA` and the self-adjoint form

```text
(p(x)y')' + μ(x)C(x)y = μ(x)R(x).
```

This is the Sturm–Liouville representation used by the second-order
classifier. The equivalent Liouville normal form removes the first derivative.
For constant coefficients and `R = 0`, it yields the exact exponential,
repeated-root, or real sine/cosine basis. Two initial conditions at one point
determine both constants. Without conditions, the family retains `C1` and
`C2`.

Self-adjoint normalization and closed-form solution are deliberately separate
claims. Arbitrary `p(x)` and potential terms may require special functions,
an eigenvalue boundary problem, or numerical integration. Until a fundamental
basis is constructible, a successfully normalized variable-coefficient
equation reports `DE_SOLVE_STATUS_UNSUPPORTED`; the module does not fabricate
an elementary solution.

Constant-coefficient equations are not limited to second order. For an
order-`n` equation, the characteristic polynomial is passed to the equation
module's complete polynomial root solver. Root multiplicity generates the
usual polynomial-times-exponential chains; conjugate pairs generate real
sine/cosine pairs. Nonhomogeneous forcing is handled by solving the Wronskian
system for variation of parameters, using exact formal integrals when the
expression integrator has no elementary antiderivative. A complete condition
set is solved through the symbolic matrix module.

The homogeneous solver returns an implicit equation when symbolic inversion
would require selecting branches. It applies initial conditions at nonzero
values of `x`; the substitution is undefined at `x = 0`.

The shifted-homogeneous reduction similarly rejects an initial condition on
`x = h`, where `Y/X` is undefined. Affine candidates and affine ratios are
accepted only after substitution has eliminated the original `x` and `y`;
this verification prevents a coincidental affine subexpression from
misclassifying a more general equation.

Linear-coordinate candidates are bounded to the linear forms already present
in the expression tree; the solver does not search an infinite coefficient
space. A candidate transformation must have nonzero determinant and must pass
the separability check after exact inverse substitution. Fractional
right-hand sides are combined algebraically before simplification so the
Möbius derivative transformation can cancel common denominators reliably.

The implementation reuses the expression module's exact simplification,
substitution, and symbolic integration. It does not convert coefficients,
condition points, or integration results to `double`. If the expression
integrator cannot produce a closed form for a linear coefficient or weighted
forcing term, the solver constructs an exact unevaluated integral instead.
Consequently, a first-order linear equation is not rejected merely because
its integrating-factor solution is non-elementary.

Each solver family lives in its own source file. The central
`diffequ_solve.c` file owns derivative isolation, shared helpers, solver
ordering, and result construction; family files own only their reductions and
solution construction.

The solve API returns an owning result object rather than overloading `NULL`:

```c
diffequ_solve_result_t *de_solve(const diffequ_t *de);
de_solve_status_t de_solve_result_status(
    const diffequ_solve_result_t *result);
de_solver_t de_solve_result_solver(
    const diffequ_solve_result_t *result);
size_t de_solve_result_count(const diffequ_solve_result_t *result);
const equation_t *de_solve_result_at(
    const diffequ_solve_result_t *result,
    size_t index);
void de_solve_result_free(diffequ_solve_result_t *result);
```

The solution collection is an array even though the first implementation
returns one solution family. That leaves room for later branch-producing
inversions without changing the public result shape. Each equation borrowed
from the result is owned by that result.

The first PDE dispatch handles

```text
a*Dx(u) + b*Dy(u) = q
```

for two independent variables and nonzero constant `a` and `b`. With boundary
data `u(x, y0) = g(x)`, characteristics give

```text
u(x, y) = g(x - (a/b)*(y - y0)).
```

The corresponding constant-`x` boundary form is handled symmetrically.
Condition arguments are therefore stored as ordered coordinate arrays rather
than the earlier ODE-only single point. The public condition-argument
accessors expose those coordinates without transferring ownership.

Without boundary data, the solver constructs a genuine arbitrary-function
node and returns `F(y - (b/a)*x)` plus a particular solution when `q` is
constant. Arbitrary-function nodes retain their argument, render as `F(...)`,
and differentiate through the chain rule.

The constant-transport classifier also decomposes a constant linear reaction
term:

```text
a*Dx(u) + b*Dy(u) + p*u = q(x,y).
```

It evolves both arbitrary-function and boundary-data solutions with the
factor `exp(-(p/a)*x)` and includes the constant particular solution `-q/p`
when `p` is nonzero and `q` is constant. Without boundary data,
coordinate-dependent forcing is pulled back to a characteristic and
integrated with the corresponding one-dimensional integrating factor. For
example, `Dx(z) + Dy(z) + z = x` gives
`z = x - 1 + exp(-x)*F(y - x)`.

For zero reaction coefficient, additive forcing `f(x) + g(y)` is handled
without introducing a characteristic integration parameter: each coordinate
term is integrated against its corresponding transport coefficient. Thus
`Dx(φ) - Dy(φ) = sin(x) + cos(y)` gives
`φ = F(x + y) - cos(x) - sin(y)`.

For polynomial forcing with nonzero reaction coefficient, the solver uses the
finite inverse of the constant-coefficient operator. Writing
`D = a*Dx + b*Dy`, the particular solution of
`(D + p)u = -q` is

```text
-q/p + Dq/p^2 - D^2q/p^3 + ...
```

The series terminates exactly because each directional derivative lowers the
total polynomial degree. It is attempted structurally, with a fixed safety
limit, and accepted only after the next directional derivative simplifies to
exact zero. Nonpolynomial forcing continues through characteristic
integration.

Additive forcing that depends on only one coordinate is also solved directly
when it belongs to a derivative-closed space. For

```text
c*P'(v) + p*P(v) = -f(v),    f''(v) = lambda*f(v),
```

the particular solution is

```text
P(v) = (c*f'(v) - p*f(v))/(p^2 - c^2*lambda).
```

This covers sine/cosine, hyperbolic, and exponential forcing whenever the
denominator is nonzero. Resonant cases continue through the integrating-factor
path. The rule is structural and coefficient-independent; it is not a table
of individual PDEs.

Before falling back to characteristic integration, the same derivative-closed
test is applied to the complete transport derivative `D = a*Dx + b*Dy`. When
`D^2*f = lambda*f`, Mars constructs the corresponding particular solution
algebraically and substitutes it into `D(P) + p*P + f`. It accepts the result
only if the residual simplifies exactly to zero. This covers mixed affine
phases without introducing the internal characteristic parameter; for
example,

```text
Dx(z) + Dy(z) = cos(x+y)
z = F(y-x) + 1/2*sin(x+y).
```

The complementary mixed-phase rule handles any unary forcing `f(g(x,y))`
for which the expression integrator knows an antiderivative and the
directional phase derivative `D(g)` is a nonzero coordinate-independent
value. It integrates a dummy unary expression with respect to its phase,
substitutes the original phase back, divides by `D(g)`, and verifies the
candidate against the original operator. Thus

```text
Dx(z) + 2*Dy(z) = tanh(x+y)
z = F(y-2x) + 1/3*ln(cosh(x+y)).
```

This is a structural rule rather than a `tanh`-specific template.

Homogeneous and constant-forced constant-coefficient transport is
dimension-independent. For `n` transport coordinates, the solver selects the
first nonzero direction as its characteristic parameter and constructs
`n - 1` invariants

```text
x_i - (a_i/a_0)*x_0,    i = 1, ..., n - 1.
```

The complementary solution is an arbitrary function of all these invariants,
multiplied by `exp(-(p/a_0)*x_0)` when a reaction term is present. Expression
trees represent the arbitrary function with a true argument list, ensuring
that a three-variable PDE produces `F(I1, I2)` rather than an incomplete
one-variable family.

For two-dimensional variable-coefficient transport, the characteristic
dispatcher also recognizes monomial direction ratios

```text
b(x,y)/a(x,y) = k*y/x.
```

It constructs `I = y*x^(-k)` and searches for an exponent `h` satisfying
`a*h_x + b*h_y = -c`. A candidate obtained by symbolic integration is scaled
only by a coordinate-independent factor, then substituted back into the
operator. If the residual is not exactly zero, the candidate is rejected.
The resulting homogeneous family is `u = exp(h)*F(I)`.

For an inhomogeneous equation

```text
a*u_x + b*u_y + c*u + f = 0,
```

the same dispatch searches along each nonzero characteristic direction for a
symbolically integrable particular candidate. It may rescale that primitive
only by a value independent of both coordinates and the dependent field.
The candidate is accepted only after the full operator
`a*P_x + b*P_y + c*P + f` simplifies exactly to zero. Thus, for example,
`x*z_x - 7*y*z_y = 5*x^2*y` produces
`z = F(x^7*y) - x^2*y` from the general rule, not from an equation-specific
template.

The variable-coefficient dispatcher additionally proposes
`I = x^2 + y^2` when the transport field `(a,b)` is tangent to circles. It
accepts this invariant only if `a*I_x + b*I_y` simplifies exactly to zero.
The same verified reaction and particular-solution machinery then solves, for
example,

```text
x*y*z_x - x^2*z_y + y*z = 3*x^2*y
z = F(x^2 + y^2)/x + x^2.
```

Derivative operands also provide a contextual symbol declaration. A built-in
alias such as `@phi` remains the golden ratio in an ordinary expression, but
inside `Dx(@phi)` or `Dy(@phi)` it is a dependent variable. The parser adds a
temporary variable binding while constructing the formal derivative graph;
that binding is not added to the equation's independent-variable list.

The next characteristic dispatch recognizes nonlinear evolution along
constant vector fields and selected variable-coefficient vector fields. It
currently covers

```text
Dx(z) + Dy(z) = 6*(x+y)^2*z^2
x^2*Dx(z) + y^2*Dy(z) = z^2
(x*z)*Dx(z) + (y*z)*Dy(z) + x^2 + y^2 = 0
z*Dx(z) + z*Dy(z) = y - x
(x+y)*Dx(z) + (y-x)*Dy(z) = 0
(y-x)*Dx(z) + (y+x)*Dy(z) = (x^2+y^2)/z
```

including the singular solution `z = 0` in both reciprocal-quadratic
families. For a
separable characteristic field, Mars constructs coordinate potentials whose
directional derivatives are exactly one. Their difference is the
characteristic invariant, and reciprocal quadratic evolution is then solved
algebraically. Each potential is accepted only after exact symbolic
verification. A
parameter-dependent linear dispatch distinguishes a passive coordinate from
an ODE integration constant, so `Dy(z) + 2*y*z = x*y^3` returns `F(x)`.

A separate structural reduction handles
`z*(a(x,y)*z_x + b(x,y)*z_y) + r(x,y) = 0`. It removes the common dependent
factor from both derivative coefficients, substitutes `w = z^2`, and solves
the resulting linear equation
`a*w_x + b*w_y + 2*r = 0` with the ordinary characteristic machinery. The
forcing particular is accepted only when applying the complete transformed
transport operator reproduces `-2*r` exactly. The solver then returns both
branches `z = ±sqrt(w)`; it does not invent `z = 0` when zero is not a
solution of the original equation.
The reciprocal form
`a(x,y)*z_x + b(x,y)*z_y = r(x,y)/z` uses the same reduction after
multiplication by `2*z`, producing `a*w_x+b*w_y=2*r`.
If the transformed forcing has zero directional derivative, it is constant
on characteristics. Mars then multiplies it by a characteristic parameter
whose directional derivative is one. When both coordinate potentials exist,
their average is preferred because it commonly yields the symmetric
particular solution; `z*z_x + z*z_y = y-x`, for example, reduces to
`w_x+w_y=2(y-x)` and obtains `w_p=y^2-x^2`.

For a homogeneous trace-zero linear characteristic field

```text
dx/ds = A*x + B*y
dy/ds = C*x - A*y,
```

Mars constructs the exact quadratic invariant
`C*x^2-2*A*x*y-B*y^2`. Polynomial particular trials are compared by exact
bivariate coefficients through total degree two, rather than by rendered or
tree shape. Consequently
`(y-x)*z_x+(y+x)*z_y=(x^2+y^2)/z` reduces to
`w=F(x^2+2*x*y-y^2)+2*x*y` and returns both square-root branches.

The same separable-potential construction is used for general linear
characteristic equations whose transport coefficients split as `a(x)` and
`b(y)`. Mars constructs `A'(x)=1/a(x)` and `B'(y)=1/b(y)`, verifies both
derivatives exactly, and uses `B(y)-A(x)` as the invariant. For example,

```text
sec(x)*phi_x + phi_y = cot(y)
phi = F(y - sin(x)) + ln(sin(y)).
```

The particular term is independently substituted into the complete PDE
operator before the solution is accepted.

A well-formed problem outside the implemented mathematical scope reports
`DE_SOLVE_STATUS_UNSUPPORTED`. Failure after a solver has matched reports
`DE_SOLVE_STATUS_FAILED`, with a diagnostic. Invalid API input reports
`DE_SOLVE_STATUS_INVALID`.

The separable inversion recognises forms that isolate `y` directly, through
`ln(y)`, or through the reciprocal arising from a quadratic dependent factor.
More general inverse relations, singular solutions lost by division, domains,
and branch conditions need explicit representation before the separable
solver claims them.

## Future Numerical Solver Boundary

The first numerical scope should be non-stiff ordinary differential-equation
initial-value problems. Boundary-value problems, differential-algebraic
equations, stiff systems, and PDEs remain valid problem objects but require
later solver families.

Before numerical integration, an explicit higher-order ODE is reduced to a
first-order state system. For example, a second-order equation in `y` uses the
state:

```text
Y₀ = y
Y₁ = Dx(y)
Dx(Y₀) = Y₁
```

The original `diffequ_t` remains unchanged; reduction produces an internal
solver plan.

An adaptive embedded Runge--Kutta method, initially Dormand--Prince 5(4), is a
suitable default for non-stiff problems. The stepping loop should be iterative,
not recursively subdivided. Error acceptance is component-wise:

```text
|errorᵢ| <= abs_tolᵢ + rel_tol·max(|oldᵢ|, |newᵢ|)
```

All state values, times, tolerances, and error estimates use `number_t`.
Implementations must not silently reduce arbitrary-precision inputs to
`double`. A solver may use a lower-precision fast path only when the caller's
selected numeric representation and requested tolerance permit it.

A future solution object should own:

- accepted independent-variable values;
- all dependent state values at each accepted point;
- local error estimates;
- termination status and a diagnostic message; and
- optional dense-output interpolation data.

Failure must be represented by an explicit status. `NAN` may occur as a numeric
result, but it is not a sufficient explanation of solver failure.

## Implementation Order

1. Add `diffequ_t`, its private guarded header, ownership rules, and borrowed
   accessors. **Implemented.**
2. Add the formal derivative expression representation needed by the parser.
   **Implemented for the current ODE syntax.**
3. Add shared-symbol parsing for the outer differential-equation form.
   **Implemented.**
4. Add expression-style round-trip output. **Implemented.**
5. Add explicit solve-result ownership, status, and diagnostics.
   **Implemented.**
6. Add first-order separable recognition and symbolic solving.
   **Implemented for directly invertible `y` and `ln(y)` integrals.**
7. Add first-order linear recognition and integrating-factor solving.
   **Implemented.**
8. Add quadratic separable inversion and quadratic Bernoulli reduction.
   **Implemented.**
9. Generalise Bernoulli reduction to exact non-unit powers while preserving
   all inverse branches.
10. Complete classification metadata, TeX output, and immutable
    serialisation.
11. Implement the first non-stiff numerical ODE initial-value solver.
12. Add multi-coordinate condition storage and constant-coefficient
    first-order transport PDEs. **Implemented.**
13. Keep the module layout flat while separating PDE dispatch, transport,
    characteristic, parameter-linear, and shared-support implementations into
    `diffequ_pde_*.c` files. **Implemented.**

The module should follow the repository's private-header convention:

```c
#if !defined(MARS_DIFFEQUATION_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || \
     (defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__ > 0))
#error "diffequ_internal.h is private to the diffequation module; include diffequation.h instead."
#endif
```

## Test Requirements

The initial test suite should cover:

- construction ownership and destruction;
- borrowed accessor bounds;
- shared symbol identity across every section;
- `?` remaining distinct from expression-level `NAN`;
- exact preservation of `@pi` and other named constants;
- all derivative spellings and mixed-derivative order;
- immediate versus formal derivative lowering;
- condition-local bindings;
- malformed-problem diagnostics;
- expression-style parse/render/parse equivalence; and
- TeX rendering for ODEs, PDEs, and boundary conditions.

Symbolic solver tests include known analytic separable and linear initial-value
problems, the selected solver family, solution ownership, visible expected and
actual output, and rejection of unsupported higher-order problems. Numerical
solver tests should later add convergence-order checks, tolerance enforcement,
backward integration, and high-precision cases that would expose an accidental
conversion to `double`.
