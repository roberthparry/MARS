# Literate Comments and Notebook Output

This note records a planned direction for the MARS language and MARS Lab. It is
not yet a description of implemented syntax.

## Aim

MARS source should be able to carry readable mathematical explanation beside
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
independent Markdown cells, executable MARS cells, reproducible cell output and a
clear distinction between source, rendered explanation and computed value.

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
4. Define structured graph results and a portable SVG rendering path.
5. Make MARS Lab consume that API and render Markdown, TeX and graphs safely.
6. Add a notebook adapter over the same result bundle and execution context.

The expression DAG should continue to represent mathematics. Literate source
structure, comments, cell identity and display metadata belong in a layer around
the DAG so symbolic simplification and differentiation remain unaffected.
