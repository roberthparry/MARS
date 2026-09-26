# Ophelia: An Executable Expression Language

## Status and Decision

The language is named **Ophelia**. MARS remains its mathematical engine; the
language frontend and any future standalone project carry the Ophelia name.

This note records an agreed direction, not an implemented language or a final
grammar. Prototype a small language frontend alongside MARS first, then consider
moving it into a separate project once its semantics and public API requirements
are stable. The existing `style_FUNCTION` representation is the starting point,
not a promise that every generated programme is already executable.

The language should treat mathematical expressions as values. Constructing an
expression, evaluating it with bindings, differentiating it into another
expression, integrating it and applying an integral transform are distinct
operations. Variables are algebraic and differentiable from the outset; only an
explicit `const` declaration makes a named parameter a mathematical constant.
Users should be able to compose these operations without converting results to
text and parsing them again.

## Project Boundary

MARS remains the mathematical engine. It owns expression construction,
simplification, numerical evaluation, forward- and reverse-mode automatic
differentiation, integration, integral transforms, mathematical domain handling
and output rendering. The frontend owns source parsing, lexical scope,
declarations, execution contexts and source-position diagnostics.

Keep the frontend in a distinct build target with a dependency on MARS's public
API. Do not copy the algebra implementation or depend on private node layouts.
Where the prototype exposes a missing capability, design an appropriate public
API rather than reaching into another module's implementation.

MARS should continue to generate Function-style text, so existing clients need
not depend on the language runtime merely to display an expression. Agree a
versioned source contract between the renderer and frontend before treating
generated source as a durable interchange format.

The language should support functions, conditionals, loops and module reuse,
introduced incrementally after the expression-construction core. Shell execution,
network access, unrestricted file access and a package installation system are
outside the initial scope. A command-line runner is sufficient to establish the
semantics before adding a Lab or notebook interface.

## Expression and Evaluation Semantics

Construction produces a symbolic value; it must not implicitly substitute the
current numerical bindings. Evaluation takes an expression, bindings and an
explicit precision context and determines whether a numerical result is
available, distinguishing an unresolved symbolic result from an execution error.
Symbolic substitution, if offered, must be a separate explicit operation.

Passing an expression to `output` requests evaluation using the current numerical
bindings in the execution context. If a numerical value can be determined,
`output` prints it at the active precision. Otherwise, it prints the algebraic
expression using MARS's native `style_EXPRESSION` rendering. Missing or NaN
bindings must not cause the algebraic expression to be replaced with a NaN
literal. Numerical output remains appropriate when simplification determines a
value independently of any missing bindings.

Neither numerical evaluation nor symbolic fallback replaces the stored
expression or discards its dependencies. Subsequent calls to `output` use the
current bindings, so the same expression may first produce algebraic output and
later produce a numerical value. Symbolic fallback does not suppress genuine
execution errors or cancellation diagnostics.

`outputa` explicitly requests algebraic output. It always renders its argument
using MARS's native `style_EXPRESSION` form, even when sufficient numerical
bindings are available to evaluate it. It does not substitute those bindings into
the algebraic expression or switch to numerical output. Both output operations
leave the stored expression and its bindings unchanged.

The agreed derivative call is `derivative(expression, variable, order)`, using
the lowercase spelling emitted by `style_FUNCTION`. It returns an expression
representing the specified derivative; it does not discard dependencies when
numerical bindings are present. The explicit order is a non-negative integer,
with zero returning the original expression.

The agreed mixed-derivative form accepts additional variable/order pairs:

```text
derivative(s,x,2,y,1)
```

Its result is the algebraic expression obtained by differentiating `s` twice with
respect to `x`, then differentiating that result once with respect to `y`. It is
equivalent to `derivative(derivative(s,x,2),y,1)` and does not itself print output.
Further variable/order pairs extend the sequence in the same way, applied from
left to right. Each order is a non-negative integer; a zero order leaves the
intermediate expression unchanged. Preserve the specified differentiation order
unless mathematical conditions justify exchanging it.

The result can be passed to `output` for numerical evaluation with algebraic
fallback, or to `outputa` for algebraic output regardless of available numerical
bindings. This multi-pair syntax is an agreed language-design extension; the
existing native three-argument constructor and nested-call rendering do not yet
implement it.

For the expression `s = sqrt(x^2 + y^2 + z^2)` and numerical
bindings `x = 3`, `y = 4`, `z = 5`, the following are language-design examples:

```text
output(derivative(s,x,1)).
outputa(derivative(s,x,1)).
```

The first statement evaluates the derivative using the current bindings, giving
`3/sqrt(50)`. Its expected numerical output, rounded to 15 decimal places, is:

```text
0.424264068711929
```

The second statement prints the algebraic derivative using native
`style_EXPRESSION` rendering. Its mathematical body is
`x/√(x^2 + y^2 + z^2)`, with superscripts, grouping and any binding wrapper supplied
by the native renderer. It must not replace the variables in that body with their
numerical values. If a numerical value cannot be determined, the first statement
also falls back to algebraic output under the usual `output` rule. These examples
specify Ophelia behaviour; changing the native callable spelling does not itself
implement the language runner or its output operations.

Differentiation returns a new expression with respect to a specified variable.
Expose automatic differentiation in both forward and reverse mode from the
outset, retaining algebraic dependencies through intermediate expressions. Neither
mode requires variables to have numerical bindings before constructing a
derivative expression. Numerical evaluation of derivatives uses the full
requested precision of the MARS number system, not finite-difference estimates or
an intermediate machine-precision conversion.

An expression with no `const` declarations is still differentiable: its named
parameters remain algebraic variables, and differentiation produces another
expression. To obtain a numerical value, supply number constants for every
remaining free binding needed by that result. Supplying these values for
evaluation does not redeclare the variables as `const` or change their algebraic
roles. If simplification eliminates every free binding, the resulting constant
expression can be evaluated without additional inputs.

Symbolic integration and integral transforms are likewise available on algebraic
expressions without first supplying numerical bindings. They return expressions,
which may retain an unevaluated operation when no supported closed form is
available. Include forward and inverse Fourier and Laplace transforms through
MARS's public API. Numerical quadrature is a separate operation. Integration
constants, bounds, convergence conditions, domains and branch restrictions must
follow MARS's mathematical semantics rather than being silently invented or
discarded by the frontend.

Retain the existing `style_FUNCTION` integral signatures and argument order as
the agreed language forms:

- `integral(upper, expression, variable)` represents an antiderivative of the
  expression with respect to the variable, evaluated at the supplied upper
  argument. It does not imply a lower bound of zero and does not automatically
  add an arbitrary integration constant.
- `integral(lower, upper, expression, variable)` represents the definite
  integral over the supplied bounds, with the integration variable locally bound
  within the integrand.

Both forms construct integral expressions that can remain unevaluated when a
supported reduction is unavailable. They work with the established `output` and
`outputa` rules. Keep the distinction between the upper-only antiderivative form
and a complete indefinite-integral family containing an arbitrary constant.
There is no agreed two-argument `integral(expression, variable)` overload, and
the existing four-argument form must not be reinterpreted as expression-first.

The current native integral-transform forms in `style_FUNCTION` are:

| Operation | Function-style signature |
| --- | --- |
| Forward Fourier transform | `fourier(expression, source, target)` |
| Inverse Fourier transform | `inversefourier(expression, source, target)` |
| Forward Laplace transform | `laplace(expression, source, target)` |
| Inverse Laplace transform | `inverselaplace(expression, source, target)` |

All four take the operand expression first, the source coordinate second and
the target coordinate or coordinate expression third. The source is locally
bound by the transform; it must not capture an unrelated free variable outside
the operand. Inverse calls supply the transform-domain source and the original
domain's target in that same argument order.

These lowercase, concatenated names are the agreed canonical Function-style
spellings. The capitalised names `Fourier`, `InverseFourier`, `Laplace` and
`InverseLaplace` remain accepted input aliases for compatibility. Function-style
rendering emits the resolved transformed expression when available, otherwise
the explicit transform call. Retain MARS's Fourier normalisation, unilateral
Laplace convention, convergence conditions and domain restrictions. Both resolved
and unresolved results remain expressions and follow the established `output`
and `outputa` rules; an unresolved call does not itself imply non-existence of
the mathematical transform.

Expression values should behave as immutable values from the language user's
perspective. Binding changes and calculus operations must not unexpectedly alter
an earlier expression or another execution context that shares its nodes.
Internal caching and reference-counted sharing remain implementation details.

## Equations and Solving

Equations are first-class values, distinct from assignments and immediate truth
tests. The agreed constructor is `equation(lhs, rhs)`: it records a relationship
between two expressions without solving it or assigning either side to the
other. Equations may be stored, passed to functions, returned and collected into
systems. The syntax for equation collections and structured solution results
remains to be specified.

`solve(e, unknown)` isolates the named unknown in an equation. Solving is not
limited to enumerating numerical roots: its result must compose naturally with
subsequent algebra, calculus and integral transforms. In particular, solving a
Laplace-domain equation for an unknown transform must supply an expression that
can be passed directly to `inverselaplace`.

The following is a language-design example of that linear-ODE workflow, not a
claim that these Ophelia statements are already executable:

```text
e = equation((s^2 + 4.s + 5).Y, 5.s + 15 + 50/s^2).
Y = solve(e,Y).
y = inverselaplace(Y,s,t).
```

Expected effect: these assignments emit no output. The solve result assigned to
`Y` is the expression

\[
\frac{5s+15+50/s^2}{s^2+4s+5},
\qquad s\ne0,\quad s^2+4s+5\ne0.
\]

The final assignment applies the inverse Laplace transform directly to that
expression and stores the resulting expression in `y`. No extraction from a
singleton solution set and no text conversion are required. The full stops
between factors denote multiplication; the final full stop on each line ends
the statement. An asterisk would instead denote convolution.

The result contract distinguishes the following cases:

- For one uniquely determined unknown, return its expression together with all
  necessary domain and parameter conditions, not a singleton set of assignments.
- For multiple solution branches or several unknowns, return structured
  alternatives or bindings, preserving branch conditions and free parameters.
  Do not silently choose a branch.
- Distinguish a proved absence of solutions from an unresolved solve request.
  Failure to find a closed form must not be reported as an empty solution set.

`solve` itself does not overwrite variable bindings. In the example, the explicit
assignment to `Y` stores the returned expression after the right-hand side has
been evaluated; it must not retroactively rewrite the equation held by `e`.
Naming a symbol as an unknown means solving for that algebraic symbol even if
it has a numerical binding. Other symbols remain parameters of the solve rather
than becoming permanently `const` or being silently replaced by their numerical
bindings.

Preserve the equation's domain restrictions and any conditions introduced by
division or other solving steps. An expression that is unique only under stated
conditions must retain those conditions through subsequent operations. Solving
must not silently choose a real or complex domain or claim completeness when it
has not been established. Numerical root-finding remains a separate operation
with explicit starting points or intervals and the active precision context.

### Deferred: Differential-Equation Workflows

Revisit differential-equation representation once Ophelia's core expression and
equation semantics have matured. A dedicated differential-equation solver is not
part of the present scope. No ODE block, prime-notation shorthand or specialised
initial-condition syntax has been agreed; the discussion of these possibilities
does not establish a language feature.

The future design needs to address:

- How to represent unknown functions and their independent variables without
  confusing them with numerical bindings or already-defined expressions.
- How to state an equation containing derivatives readably, without requiring
  the author to write out its Laplace-domain algebra by hand.
- How to express evaluation at a point and attach or supply initial and boundary
  conditions, including values of derivatives.
- Whether and how general transforms act on both sides of an equation, retain
  boundary terms and incorporate supplied conditions without losing domains.
- How to identify the transformed unknown, solve the resulting algebraic
  equation and pass its expression result directly to an inverse transform.

Use the linear-ODE Laplace workflow as a future design test, not a commitment to
a special solver or any of the rejected notation proposals. For now, retain the
general equation, algebraic solving and integral-transform decisions above.

## Scope, Constants and Precision

Separate lexical names from expression bindings. Resolve function parameters and
local temporaries in lexical scope; treat summation and integration indices as
local mathematical binders. Nested scopes must not capture an unrelated symbol
merely because it has the same printed name.

Variables and parameters are non-constant by default. They denote algebraic,
differentiable quantities unless explicitly declared with `const`; neither their
spelling nor a supplied numerical binding implicitly makes them constant. Do not
inherit implicit constant inference from an input shorthand as a language rule.
An intermediate expression retains its dependencies rather than becoming a new
independent variable merely because it has been assigned a name.

An undeclared name used as a variable implicitly denotes a non-`const`,
uninitialised algebraic variable. Its numerical binding defaults to NaN, just as
for an explicitly declared variable without an initialiser, and it participates
in symbolic calculus immediately. The missing declaration is not itself an error.
Resolve existing scoped bindings before applying this default; using an already
declared `const` name must not silently replace it with a non-constant variable.

The following is a language-design example, not a claim that an Ophelia runner
is already implemented. It is valid even when none of these names has previously
been declared or used:

```text
s = sqrt(x^2 + y^2 + z^2).
```

Expected effect: no output is emitted by the assignment itself. The names `x`,
`y` and `z` denote newly introduced non-`const` algebraic variables, each with a
NaN numerical binding. The name `s` holds the symbolic expression
`sqrt(x^2 + y^2 + z^2)` with its dependencies on those variables; it is not an
independent symbol or an eagerly evaluated NaN result. The final full stop
terminates the statement.

The expression held by `s` can immediately participate in differentiation,
integration and integral transforms under their mathematical domain rules.
Supplying numerical values for `x`, `y` and `z` later enables explicit numerical
evaluation without reconstructing the expression or changing their algebraic
roles.

The following complete design example supplies those bindings after constructing
`s`, then requests its numerical value:

```text
s = sqrt(x^2 + y^2 + z^2).
x = 3.
y = 4.
z = 5.
output(s).
```

Expected numerical output, shown rounded to 15 decimal places:

```text
7.071067811865475
```

This is the numerical value of `sqrt(50)`. The displayed number of digits follows
the active precision and formatting settings; the example does not prescribe a
machine-precision limit. The assignments update the existing numerical bindings
of `x`, `y` and `z`. They do not require redeclaring those variables or rebuilding
`s`, and `output(s)` must not use the NaN bindings that existed when `s` was
constructed. Later binding updates are likewise observed by subsequent calls to
`output(s)`, while `s` retains its algebraic form and remains available for
calculus.

Preserve the distinction already expressed by Function output between
differentiable parameters and explicitly declared `const` parameters. A constant
is independent of the differentiation variables; it need not already have a
numerical value. Number literals are constant values, distinct from named
algebraic variables.

Both variables and `const` parameters may be initialised with numbers, but an
initialiser is optional. Without one, their numerical binding is initialised to
NaN. This default must not replace the algebraic symbol with a NaN literal in an
expression or prevent symbolic calculus. An uninitialised binding is therefore
not zero and does not supply a usable numerical value.

Either kind of binding may subsequently be set to a number so that an expression
can be evaluated to a numerical result, provided its remaining required bindings
and mathematical domain conditions are satisfied. Setting a `const` parameter
later does not make it differentiable; setting a variable does not make it
constant. Numerical binding updates must not implicitly substitute values into
the symbolic expression. Define capture and later rebinding explicitly before
introducing persistent closures or notebook execution state.

Distinguish a binding that cannot be reassigned from a mathematical constant under
differentiation. These are independent properties. Preserve the mathematical
meaning of `const` in generated Function output; decide a separate spelling for
read-only programme bindings rather than silently giving `const` both meanings.

Retain exact integer and rational literals where supported. Carry the requested
precision into numerical evaluation and result formatting without first passing
through a machine-precision approximation. Changing numerical precision must not
rewrite the symbolic source or invalidate its mathematical meaning.

### Numerical Precision Context

Ophelia provides `precision(n)` to set the active numerical precision to `n`
significant decimal digits, and `precision()` to query that setting. This controls
numerical computation, not merely the number of digits displayed. The additional
setter `precisionbits(n)` specifies the working precision in binary bits instead.
These are agreed language facilities, not currently implemented Ophelia syntax.

A precision-setting statement may execute at any stage, including between
assignments and output statements, after an expression has been defined, and
inside a function. It takes effect immediately for subsequent numerical
evaluations in the active execution context. Numerical evaluation of derivatives,
integrals and integral transforms uses that same context. Output formatting must
respect it without being confused with the precision of the computation itself.

Expressions retain their algebraic definitions and dependencies rather than
capturing the numerical precision at construction time. Evaluating an earlier
expression after changing precision therefore uses the new setting. Increasing
precision requires fresh evaluation from the retained expression; a cached
lower-precision approximation must not be presented as a newly computed
higher-precision result. Exact values remain exact, but increasing precision
cannot recover information already lost from a rounded numerical input.

A function invocation inherits its caller's precision. Precision changes inside
the invocation are local to it, and the caller's setting is restored on return.
At the top level, the selected precision remains active until changed again.

## Programme Control Flow

Ordinary functions execute statements and return typed values, including numbers,
matrices and expressions. Expression-valued functions construct symbolic algebra;
returning an expression must not implicitly evaluate it. Define parameter and
return types, local scope and capture rules before introducing richer execution.

### Conditionals

Provide `if` and `else` for programme execution. Their conditions must resolve to
Boolean values. An unresolved symbolic comparison is a diagnostic, not an implicit
false value or permission to substitute whichever numerical bindings happen to
be available.

Provide a separate mathematical piecewise constructor for symbolic conditions.
It retains its conditions and branches as an expression that MARS can evaluate,
differentiate or integrate where supported. Domain restrictions and behaviour at
branch boundaries belong to the mathematical engine. Differentiating an arbitrary
programme containing control flow is not implied by support for differentiating
expression values returned by that programme.

### Loops

Provide `for` over finite ranges and collections, followed by `while`, `break`
and `continue`. Specify range endpoints, iteration order and collection-mutation
rules explicitly. Loop bodies can construct expressions and update programme
bindings without mutating previously constructed expression values.

Keep mathematical sums and products as native symbolic operations. They must not
require expanding every term into a programme loop or an enormous expression
DAG. A loop that builds such a DAG remains subject to construction resource limits.

All loops, function calls and expensive native operations must participate in
cancellation and execution budgets. An exhausted budget is an execution
diagnostic, not a mathematical result. A `while` loop need not have a statically
known iteration count, but it must remain interruptible.

### First-Class Recurrence Expressions

Recurrences are first-class expressions from the outset. This is an agreed
language-design decision, not an optional extension to executable control flow.
Their representation retains the defining relation, initial or boundary
conditions, symbolic indices and bounds, parameter dependencies and domains.
Unresolved stopping conditions may remain part of the mathematical definition;
retaining them does not require choosing a programme branch or running a loop.

Separate executing a computation from representing its mathematical definition.
A finite loop or a terminating recursive call can construct an algebraic
expression, preserving its dependencies. A symbolic sum, product or recurrence
instead describes a computation without necessarily executing all its steps.
Do not silently interchange these meanings.

Constructing a recurrence expression must not require expanding or executing its
definition. Iteration and recursion are evaluation strategies, not prerequisites
for representing it. A valid symbolic definition may remain useful even when a
particular evaluation does not terminate or exceeds its budget. Such an evaluation
failure must leave the definition intact and must not be reported as proof of
mathematical invalidity. Conversely, representing a definition does not itself
establish existence, uniqueness or convergence of a solution.

### Executable Iteration and Recursion: Working Proposals

The following execution proposals are not yet agreed syntax or an implementation
commitment. They do not restrict the first-class symbolic representation above.

For executable iteration, propose that range bounds and steps resolve to exact
integers, and that continuation tests resolve to Boolean values through explicit
evaluation. The carried state may remain algebraic. Having a concrete iteration
count does not require the expression's other variables to have numerical
bindings, nor does it implicitly declare the counter or any parameter `const`.
Reassignment selects the next expression value; it must not mutate the expression
from the previous step or sever its dependencies.

For recursion, propose ordinary named functions with fresh local bindings for
each invocation and explicit terminating branches. Recursive calls use the same
argument, scope and expression-value rules as non-recursive calls. Direct
recursion and mutual recursion need defined name-resolution rules; a recursive
function definition must not create a cyclic expression DAG. Executing a call to
expand a recurrence must terminate before yielding its expanded result; returning
the recurrence expression itself does not require that expansion.

Execution limits apply to recursive calls as well as loops. Exhausting call depth,
work or expression-storage budgets must produce a diagnostic, not a partial
mathematical answer or a host-stack crash. Consider guaranteed tail-call execution
without growing the call stack. Tail calls must still consume the work budget;
bounded call-stack usage does not imply bounded expression storage. Do not add
implicit memoisation before purity, bindings and precision-context rules are
defined.

### Calculus Across Iteration and Recursion

First-class recurrences and expressions constructed by finite iteration or
recursion remain eligible for forward- and reverse-mode differentiation,
integration and integral transforms under the same mathematical rules as other
expressions. Reusing an intermediate must preserve its algebraic dependency, not
treat its current value as a newly independent symbol or a constant.

Where mathematically valid, differentiation can produce a derivative recurrence
by differentiating both the defining relation and its initial or boundary
conditions with respect to the chosen parameter. It need not expand each iterate
first. Retain the resulting dependencies and domain conditions; when a supported
rule is unavailable, preserve the requested operation symbolically rather than
inventing a numerical answer.

Distinguish continuous parameters from discrete recurrence indices.
Differentiation with respect to an integer index has no default ordinary-derivative
meaning: a continuous extension must be specified and need not be unique.
Differences with respect to a discrete index are a distinct mathematical
operation. Being algebraic does not remove these domain restrictions or make an
index implicitly `const`. Differentiable by default means that variables
participate in differentiation, not that every expression has a derivative
everywhere.

Differentiating the expression produced by one execution is distinct from
differentiating the programme's stopping rule, its choice of branches or the limit
of an iterative process. A derivative for the executed branch is not automatically
valid across a branch boundary or a change in iteration count. Likewise, the
derivative of a finite iterate must not be presented as the derivative of its
converged limit without mathematical justification. Exchanging differentiation
and a limit requires its own justification; convergence of the iterates alone is
not sufficient. Forward and reverse execution must preserve these distinctions
and respect the requested precision and resource limits.

### Decisions Still Required

- The source notation and binding rules for recurrence definitions, initial or
  boundary conditions, and explicit evaluation or expansion. First-class status
  and retention of unresolved mathematical conditions are already agreed.
- Range syntax, endpoint inclusion, step validation, traversal order and whether
  collection mutation during iteration is prohibited or has snapshot semantics.
- Whether tail-call behaviour is guaranteed, how mutual recursion is declared,
  and how execution and expression-storage budgets are configured.

## Modules and the `use` Keyword

The agreed module-loading keyword is `use`, with a module name as its operand and
the established full-stop statement terminator. `timeseries` is an intended
module name. This is a recorded language-design decision, not implemented syntax.

`use` describes access to a module's public interface. Avoid `include`, which
suggests textual insertion and risks coupling callers to source layout. `import`
was considered, but `use` is shorter and matches the intended operation. Neither
`include` nor `import` is planned as a synonym.

Loading a module introduces its namespace, not every exported name into the
caller's scope. Select a qualification operator that is unambiguous alongside
the existing multiplication and statement-termination uses of the full stop.
Explicit aliases or selective imports can follow once name resolution is stable.

Support two module implementations behind the same language-facing mechanism:

- Language modules export declared functions, constants and expression
  constructors from source files; implementation names remain private.
- Native modules expose existing MARS modules through checked public-API
  adapters, with defined argument types, results, ownership and diagnostics.
  Users should not handle raw C pointers or reference counts.

Begin native adapters with expressions, numbers, matrices and equations, then
extend to timeseries and the other existing modules as their interfaces are
specified. Module reuse does not mean every C declaration becomes available
automatically or that users can load arbitrary native libraries.

Resolve module names through an explicit registry and configured source roots,
without automatic downloads or arbitrary filesystem searching. Initialise a
module once per execution context; repeated use reuses that module instance.
Detect dependency cycles and report them clearly. Failed initialisation must
not publish a partially initialised namespace. Record resolved module versions
for reproducibility and define compatibility before separate project releases.

Importing a module does not grant filesystem, network or shell access. Any such
operation requires a separately defined host capability. This is a design
requirement, not a claim that the prototype already provides a security sandbox.
Jupyter sessions and the command-line runner must share these loading and
initialisation rules.

## Function-Style Round Trips and DAG Sharing

The core acceptance criterion is that parsing generated Function output
reconstructs an equivalent expression, including parameter roles, bindings,
domains and shared symbolic intermediates. It need not reconstruct identical
pointer addresses or identical internal simplification history.

A temporary denotes a symbolic subexpression, not a prematurely evaluated
number. Construct it once and reuse the resulting expression value. Emit
temporaries in dependency order, avoid atom-only aliases that add no clarity,
and respect binder scope when deciding where a temporary can be declared.

Count reuse within the expression being rendered rather than treating a node's
global reference count as its source-level use count. External owners and caches
can also hold references. Pointer identity can identify actual DAG sharing;
structural equivalence and algebraic simplification remain separate concerns.

Derivative caching belongs in MARS. Reuse a cached derivative only when the
variable and any semantic context affecting that derivative match. The frontend
must neither assume that one cached derivative applies to every variable nor
build a competing differentiation cache over rendered text.

## Source and Presentation

Preserve established Function-style declarations, canonical built-in names,
statement termination, multiplication notation and comments where practical.

The agreed operator and statement-termination conventions are:

- `*` means convolution, not multiplication.
- `.` means multiplication within an expression and also terminates a statement.
- `,` separates successive statements in a statement sequence, which ends with a
  full stop.

This design example contains three statements on a single line:

```text
x = 3, y = 4, z = 5.
```

Expected effect: execute the three assignments in source order, leaving the
numerical bindings `x = 3`, `y = 4` and `z = 5`, with no output from the assignments
themselves. This is equivalent to the three separately terminated assignments in
the earlier `output(s)` example, not a tuple or a simultaneous assignment.
Replacing those three lines with this single line leaves that example's numerical
output unchanged. The grammar must distinguish statement-separating commas from
commas within argument lists and other delimited expressions.

Do not inherit an input shorthand's use of `*` as a multiplication alias into
Ophelia. Its convolution meaning must not depend on whether it appears inside an
integral transform. These are language-design decisions, not changes to the
existing MARS parser in this note.

Specify how the grammar distinguishes a multiplication dot from a terminating
full stop and from a decimal point in a number literal. Any role for whitespace,
line breaks or lookahead remains to be decided explicitly. Operator precedence,
associativity and the coordinate and domain rules for convolution also need to
be specified. The numerical and symbolic-fallback behaviour of `output`, and the
explicit algebraic behaviour of `outputa`, are agreed. The derivative and integral
signatures specified above are also agreed. Other spellings of expression-valued
calculus and evaluation operations remain open design decisions.
This note does not claim that the proposed syntax is implemented.

Coordinate source metadata and future rich results with
[literate comments and notebook output](./literate-comments-and-notebooks.md).
Comments, source locations and notebook cells belong outside the mathematical
DAG. Clients consume native results and renderings; they do not reinterpret the
mathematics to execute a Function card.

Jupyter is the proposed next interface after the command-line prototype. A
dedicated kernel should reuse the frontend's persistent execution context and
typed results, not introduce notebook-specific language semantics. See the
[Jupyter kernel direction](./literate-comments-and-notebooks.md#jupyter-kernel-direction)
for cell state, rich output, reproducibility and interruption requirements.

## Staged Prototype and Verification

1. Specify the smallest supported subset of current Function output and its
   value, binding, constant and precision semantics, together with first-class
   recurrence representation and its mathematical binders.
2. Build a lexer and parser with source-position diagnostics, then construct
   expressions exclusively through the public MARS API.
3. Add explicit evaluation, forward- and reverse-mode automatic differentiation,
   symbolic integration and integral-transform operations that return typed
   values without text round trips.
4. Verify renderer-to-parser round trips, numerical agreement at multiple
   precisions, constant handling, implicit undeclared variables, optional
   initialisers, NaN defaults, nested binders and symbolic temporary sharing.
   Verify precision changes before and after expression construction and between
   successive evaluations, both decimal-digit and binary-bit settings, precision
   queries, and restoration of the caller's setting after a function returns.
   Check that higher-precision evaluation recomputes retained expressions without
   claiming to recover precision lost from already-rounded numerical inputs.
   Use the undeclared-coordinate assignment above as a frontend acceptance case:
   check its lack of implicit output, the three NaN-bound non-constant variables,
   and the retained symbolic expression and dependencies held by `s` before
   binding values and evaluating it. Verify that the complete example above
   prints the numerical value of `sqrt(50)` through `output(s)`, with output
   matching the selected precision and formatting. Before supplying the bindings,
   verify that `output(s)` instead uses native `style_EXPRESSION` output of the
   algebraic expression, without replacing its symbols with NaN. Verify numerical
   output when simplification determines a value despite missing bindings, and
   check that actual execution errors remain distinguishable from symbolic
   fallback. Verify that `outputa` retains native `style_EXPRESSION` rendering
   before and after numerical bindings are supplied, without substituting those
   values into the algebraic expression. Check that subsequent binding changes
   affect evaluation without rebuilding `s`. Verify that comma-separated
   assignments execute as separate statements in source order and give the same
   result as separately terminated assignments. Turn these design examples into
   README tests when the frontend supports the statements. Include the derivative
   output examples: numerical evaluation of the derivative at the supplied
   bindings and algebraic output of the same derivative without substitution.
   Verify that the mixed-derivative example agrees with its nested-call form,
   including left-to-right ordering, zero orders and additional coordinate pairs.
   Verify round trips for both retained integral signatures, including argument
   order, bound-variable scope and the distinction between an absent lower bound
   and an explicit zero lower bound. Do not invent an integration constant for
   the upper-only form.
   Verify the four native transform call signatures and their source/target
   order, including inverse calls, bound-coordinate scope and preservation of
   unresolved transform expressions.
   Use the Laplace-domain equation example as a frontend acceptance case: verify
   that solving for `Y` returns a directly composable expression with its domain
   restrictions, that assignment does not rewrite the stored equation, and that
   `inverselaplace` consumes the result without solution-set extraction. Turn the
   example into a README test when the frontend supports these statements.
   Check multiple branches, parameter conditions, explicit unknowns with existing
   numerical bindings, and the distinction between unresolved solving and a
   proved absence of solutions.
   Verify subsequent numerical assignment to both variables and `const`
   parameters, preserving their algebraic roles
   and expressions. Check that a default NaN binding does not prevent symbolic
   calculus. Verify that undeclared constant roles are never inferred,
   unbound variables can be differentiated, and numerical evaluation of derivative
   expressions requires only their remaining bindings. Check agreement between
   both differentiation modes at the requested precision. Test calculus results
   against the native API, including unsupported integrals, transform round trips
   and domain-sensitive cases. Verify symbolic recurrence round trips, derivative
   recurrences including their initial conditions, and discrete-index handling.
   Ensure that evaluation failure preserves a recurrence definition and that
   derivatives of finite iterates are not silently identified with derivatives
   of limits.
5. Check ownership on successful execution and all failure paths, including
   malformed source and interrupted evaluation. Define execution resource budgets
   and report exhausted budgets distinctly from mathematical failure. Run any
   explicitly requested memory checks under the limits in
   [the testing guide](../testing.md#resource-bounded-memory-checks).
6. Extend the command-line prototype in the order: scope and types, ordinary
   functions and conditionals, resource-bounded loops and recursive calls, then
   modules. Verify unresolved conditions, piecewise boundaries, cancellation,
   namespace collisions, repeated loading, dependency cycles and failed
   initialisation.
7. Add the Jupyter adapter over the same execution context, retaining the same
   control-flow, module and precision semantics across cells.
8. Once the public API boundary and source contract are stable, decide whether
   independent releases justify extracting the frontend into a separate project.

Run tests sequentially. Add executable documentation examples with their expected
output only when the syntax exists, and include them in the README-example test
lane after the other tests. This design note does not authorise implementation or
project extraction by itself.
