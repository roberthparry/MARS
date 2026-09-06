# Literate Comments and Notebook Output

This note records a planned direction for the Ophelia language and MARS Lab. It is
not yet a description of implemented syntax.

## Aim

Ophelia source should be able to carry readable mathematical explanation beside
executable expressions. Mars Lab should eventually render that explanation,
rather than treating every comment as disposable text. The same representation
should also support notebook front ends without requiring a separate language.

## Comment Model

Function-style ordinary comments use backticks and are not rendered. One
backtick opens a delimited comment and the next unescaped backtick closes it;
the comment may span lines. Two consecutive opening backticks introduce a line
comment that ends at a newline or end of input. The lexer uses longest-match,
so two backticks always open a line comment rather than forming an empty
delimited comment. A backslash escapes a literal backtick in a delimited
comment. Generated Function cards must use these forms wherever comments are
appropriate rather than emitting comments borrowed from another language.

Comments behave as whitespace before MARS operators and full-stop statement
termination are interpreted. A distinct documentation-comment form contains
Markdown and is retained as part of the parsed document or cell.

The provisional documentation-comment prefix is `///`. Consecutive documentation
comment lines form one Markdown block associated with the following declaration,
statement or cell. This choice is provisional until the complete source and cell
grammar is designed.

Markdown documentation comments may contain TeX mathematics. Inline mathematics
should use Markdown's TeX-compatible inline form, and display mathematics should
use its display form. TeX therefore does not require an unrelated second comment
language: it is mathematical content embedded naturally in Markdown comments.

Comment scanning must happen before MARS operators and statement terminators are
interpreted. In particular, a full stop or semicolon inside a retained comment
must not be mistaken for multiplication, a statement boundary, a matrix row
separator or tensor-derivative notation.

## Rendering

MARS Lab should eventually render retained Markdown comments and their TeX
mathematics. Rendering must be optional: the original source text remains
available, and plain-text clients must still receive a useful representation.

Rendered commentary should stay connected to the expression, equation, matrix or
result that it explains. It should not be placed in an unrelated global message
area. The precise card or cell layout remains a user-interface decision.

## Notebook Compatibility

Evaluation should produce a bundle of typed representations rather than a single
preformatted string. The intended bundle includes:

- `text/plain` for the ordinary textual result;
- `text/latex` for mathematical rendering;
- `text/markdown` for retained explanatory commentary;
- the MARS Expression and Function representations;
- a structured numeric value or collection of values when evaluation succeeds;
- structured plot data and plot metadata when a result can be graphed;
- `image/svg+xml` or another suitable rendered plot representation for clients
  that cannot draw directly from the structured plot data;
- structured diagnostic information when evaluation fails.

This model maps naturally to notebook rich display and to the separate result
cards in MARS Lab. A future Jupyter kernel or another notebook adapter should be
able to translate the same bundle into the host's native display messages without
re-evaluating or reparsing the result.

Notebook compatibility also requires persistent execution state, ordered cells,
independent Markdown cells, executable Ophelia cells, reproducible cell output and a
clear distinction between source, rendered explanation and computed value.

## Jupyter Kernel Direction

Jupyter is the proposed next interface after the command-line prototype of the
[Ophelia language](./expression-language.md). A dedicated Ophelia kernel should adapt
the same language frontend and execution context to Jupyter, with MARS remaining
the mathematical engine. This is planned work, not an existing kernel.

Code cells should accept the language directly. They should be able to construct
expression values, evaluate them with bindings at the selected precision, and
differentiate or integrate them to produce new expression values. Users should
not need to translate the language into Python or pass generated Function text
through a second mathematical implementation.

The kernel adapter owns notebook protocol handling and translates execution
requests and typed results. Parsing, scope, rebinding and precision semantics
belong in the shared frontend; algebra and calculus belong in MARS. Notebook
support must not introduce a competing evaluator or simplifier.

### Persistent State and Reproducibility

Keep definitions and expression values available to subsequent executions in
the same kernel session. Execution order, rather than a cell's visual position,
determines the current state. Re-executing a definition follows the frontend's
rebinding rules and must not silently mutate previously constructed expressions.
Do not imply automatic dependency tracking or automatic recomputation of later
cells in the first implementation.

Restarting the kernel clears the execution context. Restarting and running all
cells in order should reproduce a notebook's results when its inputs, language
and MARS versions, and precision settings are unchanged. Saved rendered output
is a record of an execution, not a serialised live expression DAG. Record the
relevant version and precision information for reproducibility.

### Cell Results and Explanation

Map the typed result bundle to rich notebook output: rendered TeX, copyable
Expression and Function forms, numerical values at the requested precision,
and plots when graph support is available. Always provide a plain-text fallback.
Distinguish unset bindings, unsupported symbolic operations, domain errors and
resource-limit failures rather than presenting all of them as missing output.

Ordinary notebook Markdown cells provide prose and TeX explanation independently
of executable source. Retained language documentation comments are complementary,
not a prerequisite for writing an explanatory notebook. The first kernel can
provide text and TeX results without waiting for graphing or interactive widgets.

### Interruption and Verification

Provide safe interruption of expensive evaluations and bounded resource use.
Define whether a failed or interrupted cell preserves earlier completed
statements; do not leave that behaviour accidental. Cancellation must release
temporary resources and leave a usable execution context, or explicitly require
a kernel restart if recovery is not safe. Never report an interrupted operation
as a completed mathematical result.

After the command-line frontend is stable, implement minimal cell execution and
plain-text results, then rich output and notebook integration tests. Verify
cross-cell definitions, rebinding, precision changes, restart-and-run-all,
diagnostics, interruption and agreement with command-line results. Completion,
inspection and interactive plotting can follow without changing the underlying
language semantics.

## Graphing

MARS should eventually graph scalar functions, parametric functions, sampled
data, complex-valued functions, implicit equations, vector fields and suitable
matrix or tensor results. Graphing should be an ordinary typed result of
evaluation, not a MARS Lab-only side effect.

The primary graph result should retain its mathematical source, independent and
dependent variables, domains, samples, labels, units and display options. A
front end may then render the same graph interactively or produce SVG for static
display. Keeping the structured representation alongside SVG permits notebooks
to preserve interactive plots while documentation and plain clients receive a
portable result.

Automatic graphing should be conservative. MARS Lab may offer a graph when the
variables and domain are clear, but it should not silently choose a misleading
domain, branch or projection. Explicit graph requests must be able to specify
those choices.

## Implementation Direction

The work should be staged as follows:

1. Define the source-level comment and document grammar, including its interaction
   with the Function-style full-stop terminator.
2. Preserve documentation comments in a document or cell representation without
   adding them to the mathematical expression DAG.
3. Define a result-bundle API with owned, typed output fields.
4. Once the command-line language prototype is stable, add a minimal Jupyter
   kernel over the shared execution context and text/TeX result bundle.
5. Define structured graph results and a portable SVG rendering path, then expose
   them through the same bundle to notebook clients and MARS Lab.
6. Make MARS Lab consume the shared API and render Markdown, TeX and graphs
   safely, without duplicating execution semantics in the browser.

The expression DAG should continue to represent mathematics. Literate source
structure, comments, cell identity and display metadata belong in a layer around
the DAG so symbolic simplification and differentiation remain unaffected.
