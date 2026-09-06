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
expression and integrating it into another expression are distinct operations.
Users should be able to compose those operations without converting results to
text and parsing them again.

## Project Boundary

MARS remains the mathematical engine. It owns expression construction,
simplification, numerical evaluation, differentiation, integration, mathematical
domain handling and output rendering. The frontend owns source parsing, lexical
scope, declarations, execution contexts and source-position diagnostics.

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
explicit precision context and produces a numerical result or a diagnostic.
Symbolic substitution, if offered, must be a separate explicit operation.

Differentiation returns a new expression with respect to a specified variable.
Symbolic integration likewise returns an expression, which may contain an
unevaluated integral when no supported closed form is available. Numerical
quadrature is a separate operation. Integration constants, bounds, domains and
branch restrictions must follow MARS's mathematical semantics rather than being
silently invented or discarded by the frontend.

Expression values should behave as immutable values from the language user's
perspective. Binding changes and calculus operations must not unexpectedly alter
an earlier expression or another execution context that shares its nodes.
Internal caching and reference-counted sharing remain implementation details.

## Scope, Constants and Precision

Separate lexical names from expression bindings. Resolve function parameters and
local temporaries in lexical scope; treat summation and integration indices as
local mathematical binders. Nested scopes must not capture an unrelated symbol
merely because it has the same printed name.

Preserve the distinction already expressed by Function output between
differentiable parameters and `const` parameters. A constant is independent of the
differentiation variables; it need not already have a numerical value. An unset
binding stays unset rather than becoming zero or a fabricated numerical value.
Define capture and later rebinding explicitly before introducing persistent
closures or notebook execution state.

Distinguish a binding that cannot be reassigned from a mathematical constant under
differentiation. These are independent properties. Preserve the mathematical
meaning of `const` in generated Function output; decide a separate spelling for
read-only programme bindings rather than silently giving `const` both meanings.

Retain exact integer and rational literals where supported. Carry the requested
precision into numerical evaluation and result formatting without first passing
through a machine-precision approximation. Changing numerical precision must not
rewrite the symbolic source or invalidate its mathematical meaning.

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
Specify their lexical ambiguities and diagnostics before extending the grammar.
The spelling of expression-valued calculus and evaluation operations remains an
open design decision; this note deliberately introduces no executable syntax.

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
   value, binding, constant and precision semantics.
2. Build a lexer and parser with source-position diagnostics, then construct
   expressions exclusively through the public MARS API.
3. Add explicit evaluation, differentiation and symbolic integration operations
   that return typed values without text round trips.
4. Verify renderer-to-parser round trips, numerical agreement at multiple
   precisions, constant handling, unset bindings, nested binders and symbolic
   temporary sharing. Test calculus results against the native API, including
   unsupported integrals and domain-sensitive cases.
5. Check ownership on successful execution and all failure paths, including
   malformed source and interrupted evaluation. Define execution resource budgets
   and report exhausted budgets distinctly from mathematical failure. Run any
   explicitly requested memory checks under the limits in
   [the testing guide](../testing.md#resource-bounded-memory-checks).
6. Extend the command-line prototype in the order: scope and types, ordinary
   functions and conditionals, resource-bounded loops, then modules. Verify
   unresolved conditions, piecewise boundaries, cancellation, namespace
   collisions, repeated loading, dependency cycles and failed initialisation.
7. Add the Jupyter adapter over the same execution context, retaining the same
   control-flow, module and precision semantics across cells.
8. Once the public API boundary and source contract are stable, decide whether
   independent releases justify extracting the frontend into a separate project.

Run tests sequentially. Add executable documentation examples with their expected
output only when the syntax exists, and include them in the README-example test
lane after the other tests. This design note does not authorise implementation or
project extraction by itself.
