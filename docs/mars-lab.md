# MARS Lab

MARS Lab is the browser-based graphical client supplied with MARS. It provides
one workspace for expressions, equations, differential equations, matrices,
symbolic and numerical integration, civil date calculations and the
astronomical almanac. The browser is the presentation layer: mathematical work
is sent to the local MARS helper programs, which use MARSlib.

## Installing and starting the Lab

MARS Lab requires Python 3.10 or later and uses only its standard library; no
packages from `pip` are needed. Build MARS and install Python, SQLCipher and the
TeX rendering tools before starting the Lab:

```sh
sudo apt install python3 texlive-latex-base dvisvgm sqlcipher
make mars-lab
```

```text
MARS Lab running at http://localhost:<port>/
Press Ctrl+C to stop.
```

The port is selected automatically. Use `python3 tools/mars_lab.py --help` to
see the command-line options, including a fixed host or port and
`--no-browser`. To install the desktop launcher and its private jurisdiction
database, run `make install-mars-lab` from the repository root.

The `tools/mars-lab` launcher checks the native expression-helper build target
before it starts the client and rebuilds the helper when needed, so a restarted
development instance cannot silently reuse a stale binary.

Use `make mars-lab-stop` to stop a Lab process belonging to the current user,
or `make mars-lab-restart` after changing the native helper or client.

Each mode retains its most recent editor text, binding values and controls
between sessions. Input events save an in-progress edit as well as a submitted
calculation, so moving between modes or restarting the Lab does not restore an
older expression or equation.
The precision buttons change the working precision used by the mathematical
helpers. The result cards provide independent zoom, expansion and copy
controls; **Use as input** returns a suitable result to the editor.

On a desktop-sized screen, each mode now follows the natural page height rather
than forcing the editor and result panels into the remaining viewport. This
keeps the controls directly below their editor and avoids large artificial
blank areas. A resize grip appears only when an editor genuinely runs out of
space. Result cards still scroll or expand independently when their content
requires it.

## Expression mode

Expression mode parses, simplifies and evaluates scalar or matrix-valued
expressions. When variables are present, the buttons beneath the editor can
differentiate or integrate with respect to each variable. **Goal seek** finds a
numeric value for a selected variable.

Native display probes for complex-number formatting do not numerically evaluate
integrals or summations. Numerical quadrature belongs to the Value calculation,
not to the checks that choose a symbolic presentation. High-precision quadrature
can still exceed the Lab's calculation timeout; this separation does not reduce
the requested numerical precision.

Supported elementary functions with an explicit symbolic complex argument are
presented in Cartesian `p + qi` form. The complete set comprises `exp`, `ln`,
`lg`; the circular functions `sin`, `cos`, `tan`, `sec`, `cosec`, `cot`;
their hyperbolic counterparts; and all twelve inverse circular and inverse
hyperbolic functions. This applies to inputs written with both parts—
`exp(x + iy)` is displayed as `exp(x)·cos(y) + exp(x)·sin(y)·i`—and to
pure-imaginary inputs, for which `sin(iy)` is displayed as
`sinh(y)·i`. A zero real component is omitted rather than displayed as `0 +`.

When the symbolic integrator has no supported closed-form primitive, the integral
button retains a formal integral with an arbitrary constant instead of reporting
an error. The Function card still calls `integral(integrand, variable)`; this
operation generates the constant and can be differentiated back to the integrand.

For example, enter `gamma(a+[time]i)` and select the *time* integral button.
The resulting antiderivative family, without its bindings, is:

```text
∫^[time] Γ(t·i + a)·dt + C
```

Here `t` is a dummy integration variable; `[time]` remains the result's free variable.

Logarithm input follows the calculator convention: `log(x)`, `log10(x)`, and
`lg(x)` all mean the base-10 logarithm, while `ln(x)` means the natural
logarithm. Result cards use `lg` and `ln` consistently, regardless of which
accepted alias was entered.

The derivative and integral buttons use the same separated Cartesian algebra
for differentiation or integration with respect to either component. Their
Rendered TeX, Expression and Function cards do not fall back to the original
unsplit function call. The imaginary unit is the final factor of the imaginary
term, and indefinite integrals place their constant of integration last in all
three representations.

Explicit fractional powers retain their complete root family. Their
derivatives likewise show every Cartesian branch; when bindings permit numeric
evaluation, the result card is titled **Values** and contains one value per
branch. Named `sqrt`, `cubrt` and `root` calls remain principal and
single-valued.

Additive ellipses are interpreted by the native expression parser. It first
tries exact geometric and inverse-index-power models, then uses a Lagrange
polynomial fitted to the supplied coefficient terms and verified against the
terminal term. The Rendered TeX derivation shows the inferred sigma before the
simplified formula and value. The browser neither extrapolates nor rewrites the
series.

The same native recognition covers finite `sin`, `cos`, `sinh` and `cosh`
progressions, including inputs such as
`cos(x)+cos(2x)+cos(3x)+...+cos(nx)`. MARS returns the corresponding geometric-
series closed form and uses its continuous value at removable singularities
such as `x = 0`. Integrating the cosine progression produces

$\quad\begin{array}{l}\displaystyle \frac{H_n(e^{ix})-H_n(e^{-ix})}{2i}+C, \qquad H_n(z)=\sum_{k=1}^{n}\frac{z^k}{k}.\end{array}$

Expression input accepts `Hn(n,z)`, `harmonic_poly(n,z)`, and `harmonicpoly(n,z)`. The Function card
uses `harmonicpoly`, and the same native node supports repeated symbolic
differentiation and direct integration.

Formal finite sums and products use `@Z_(k=1)^n term` and `@P_(k=1)^n term`.
The lower-bound parentheses may be omitted, as in `@Z_k=1^n term`;
`@Z_(k=1)^n` is the ASCII replacement for `Σ_(k=1)^n`.
Omitting the upper bound produces the corresponding infinite operator, as in
`@Z_k=1 term` or `@P_k=1 term`. The operator index is local: it does not create
a binding control. Mathematical output renders sums with Σ; Function output
uses `sum(k, 1, n, term)` when the summation itself remains the simplified
result.

Formal `sin(kx)`, `cos(kx)`, `sinh(kx)` and `cosh(kx)` sums from 1 to `n`
use the same native geometric-series identities as recognised ellipsis input.
For example, with supplied `x` and `n`, `@Z_(k=1)^n sin(kx)` displays

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}\sin(kx) =\frac{\sin(nx/2)\sin((n+1)x/2)}{\sin(x/2)},\end{array}$

and supplies its numerical Value without iterating through a large upper
bound.

The exponential progression is geometric as well:

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}e^{kx} =\frac{e^x}{e^x-1}\left(e^{nx}-1\right).\end{array}$

For `{ @Z_(k=1)^n exp(kx) | x=2; n=100000 }`, MARS Lab displays that
identity, Expression style emits
`exp(x)/(exp(x) - 1)·(exp(nx) - 1)`, and Function style evaluates the same
formula using a shared `exp(x)` temporary. Its Value is
`9.110304914770879911502042940141264278041407847643843263784059825E+86858`.
At `x = 0`, MARS evaluates the removable limit directly and returns `n`.

The exponential-sine progression is the imaginary part of a complex geometric
series, so MARS also reduces it without iterating through its terms:

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}e^{kx}\sin(kx) =\frac{e^x\left(\sin x-e^{nx}\sin((n+1)x)+e^{(n+1)x}\sin(nx)\right)} {1-2e^x\cos x+e^{2x}}.\end{array}$

For `{ @Z_(k=1)^n exp(kx)sin(kx) | x=1; n=100000000 }`, the Value begins
`1.804482674473709321302888821113364953E+43429448` at 128-digit precision.

Products inside logarithms reduce the corresponding logarithmic progression:

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}\ln(kx)=n\ln(x)+\ln\Gamma(n+1).\end{array}$

For `{ @Z_(k=1)^n ln(kx) | x=2; n=100000 }`, Expression style emits
`n·ln(x) + lnΓ(n + 1)`, Function style emits `n.ln(x) + lgamma(n + 1)`, and the
Value is
`1120613.939955116396071001320351928512056894536584146906526018233`.

Because `log` denotes the common logarithm, it remains distinct from `ln`:

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}\lg(kx) =n\lg(x)+\frac{\ln\Gamma(n+1)}{\ln(10)}.\end{array}$

For `{ @Z_(k=1)^n log(kx) | x=2; n=100000 }`, the parseable result is
`n·lg(x) + lnΓ(n + 1)/ln(10)` and the Value is
`486676.4504663690278820372835671954350107214367484924045390786799`.

The summand function now owns any exact finite-progression reducer. This keeps
the recogniser independent of function names and lets related functions reuse
the same mathematics. For example,

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}\operatorname{versin}(kx) =n-\frac{\sin(nx/2)\cos((n+1)x/2)}{\sin(x/2)}.\end{array}$

For `{ @Z_(k=1)^n versin(kx) | x=2; n=100000 }`, Expression style emits
`n - sin(nx/2)·cos(x/2·(n + 1))/sin(x/2)` and the Value is
`100000.0242173437116803434689891041295735315676686381885043434219`.
The other seven versed and haversed functions reduce through the same sine and
cosine progression operations.

The progression step may itself be a symbolic product. The recogniser removes
the local index wherever it occurs as one multiplicative factor, so `kax`,
`akx`, and `axk` all have step `ax`. For
`{ @Z_(k=1)^n sin(kax) | a=2; x=3; n=100000 }`, Expression style outputs
`sin(nax/2)·sin(ax/2·(n + 1))/sin(ax/2)` with the supplied bindings,
Function style introduces `v1 = a.x.` and evaluates
`v2 = v1/2.` before returning
`sin(n.v1/2).sin((n + 1).v1/2)/sin(v2)`, and the Value is
`-0.1868614750758504223995060240052491962444814290852506553280866826`.
Rendered TeX shows the original sigma followed by that identity. A nonlinear
argument such as `k²ax` retains another `k` after extraction and therefore
remains a formal sum.

Positive integer scaling also gives exact homogeneous progressions. In
particular,

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}\sqrt{kx} =\sqrt{x}\left(\zeta(-1/2)-\zeta(-1/2,n+1)\right).\end{array}$

For `{ @Z_(k=1)^n sqrt(kx) | x=2; n=100000 }`, Expression style emits
`√(x)·(ζ(-1/2) - ζ(-1/2, n + 1))` and the Value is
`29814463.01298576613569741465397922838928192939324835606473553721`.
The analogous cube-root reducer uses exponent `-1/3`; `abs(kx)` and `conj(kx)`
use the triangular number `n·(n + 1)/2`.

Floor and ceiling use an arithmetic progression for a proved integral step. For
`{ @Z_(k=1)^n floor(kx) | x=2; n=100000 }`, all result cards retain the native
domain-required specialisation `x·n/2·(n + 1)` and the Value is `10000100000`.

A proved small rational step `x = p/q` has a repeating residue pattern. Writing
`m = floor(n/q)` and `r = mod(n, q)`, MARS reduces

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}\left\lfloor\frac{pk}{q}\right\rfloor =m\left(\frac{pq(m-1)}{2}+p+\frac{(p-1)(q-1)}{2}+pr\right) +\sum_{s=1}^{r}\left\lfloor\frac{ps}{q}\right\rfloor .\end{array}$

The native expression expands the last sum into no more than `q - 1` residue
indicators, so it never conceals a large term-by-term loop. For
`{ @Z_(k=1)^n floor(0.6k) | n=100000 }`, Expression style outputs
`{ ⌊n/5⌋·(15/2·(⌊n/5⌋ - 1) + 7 + 3·mod(n, 5)) + (⌊1/5·(mod(n, 5) + 3)⌋ + ⌊1/5·(mod(n, 5) + 2)⌋ + 2·⌊1/5·(mod(n, 5) + 1)⌋) | n = 100000 }`
and the Value is `2999990000`. Rendered TeX shows the original sum followed by
this equality, while Function style emits the same period formula with
temporaries for `n/5`, `floor(n/5)`, and `mod(n, 5)` rather than
`sum(k, 1, n, ...)`.

Non-integral rationals currently require reduced denominator `q <= 32` and
`|p| <= 1000000`. MARS leaves an unset, irrational, or larger rational step
formal rather than claiming a reduction it has not proved. Ceiling uses the
corresponding ceiling residues.

The weighted hyperbolic sums `@Z_k=1^n sinh(kx)/k` and
`@Z_k=1^n cosh(kx)/k` have native Lerch-transcendent forms built from Li₁ and
Φ. MARS Lab always displays the applicable form, including when a stable
large-bound evaluator supplies the Value, so the interface never suggests that
an enormous number of terms was summed directly. TeX stays on one line when
space permits, Expression output uses `Li1` and the capital symbol `Φ`, and
Function output uses `li1` and `lerchphi`. **Use as input** copies that exact
parseable Expression representation back to the editor while retaining the
supplied bindings. Differentiating the copied weighted-sinh form recognises the
identity from which it came and returns

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}\cosh(kx) =\frac{\sinh(nx/2)\cosh((n+1)x/2)}{\sinh(x/2)}.\end{array}$

The Value card evaluates the combined weighted sum through a stable native
path rather than exposing the large cancelling imaginary parts of individual
principal-branch terms. It also avoids iterating through every term for a very
large finite upper bound. The browser receives the simplified algebra,
renderings and value from MARSlib; it does not parse, simplify or substitute
the mathematics itself.

Weighted circular sums use the corresponding unit-complex Lerch form. For
example, the input

```text
{ -Σ_(k=1)^n cos(kx)/k | x = 2; n = 100000 }
```

has this Expression output:

```text
{ -½·(Li1(exp(ix)) - exp(ix)^(n + 1)·Φ(exp(ix), 1, n + 1) +
  (Li1(exp(-ix)) - exp(-ix)^(n + 1)·Φ(exp(-ix), 1, n + 1))) | x = 2; n = 100000 }
```

Its Function implementation reuses the reciprocal unit exponential:

```text
expression expr(x, const n) {
    const c1 = n + 1.

    v1 = i.x.
    v2 = exp(v1).
    v3 = 1/v2.

    return -1/2.(li1(v2) - v2^c1.lerchphi(v2, 1, c1) +
                  (li1(v3) - v3^c1.lerchphi(v3, 1, c1))).
}

x = 2.
const n = 100000.
output(expr(x, n)).
```

The accompanying Value is approximately
`0.52053867649950756146719704452337066`. An outer sign is retained in the
formula and numerical result instead of hiding recognition of the underlying
weighted sum.

The captured editor input is `1+1/2^p+1/3^p+...+1/n^p`; its binding boxes
supply `p = 2.5` and `n = 100`. MARS recognises

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}\frac{1}{k^p} =\zeta(p)-\zeta(p,n+1),\end{array}$

and the supplied bindings produce approximately
`1.3408255697514640082147074818471`. At `p = 1`, MARS uses the harmonic
formula `digamma(n + 1) + gamma` consistently in the Rendered TeX, Expression
and Function cards, with the sigma summand shown as `1/k`; non-positive integer
exponents use the corresponding Faulhaber polynomial. With `n = inf`, `p = 2`
simplifies exactly to `pi^2/6`,
while another real `p > 1` evaluates to `zeta(p)`,
while a divergent infinite series has no finite Value card. A literal infinite
terminal term such as `1/inf^2` is also accepted.

Riemann zeta accepts `zeta(s)` or `ζ(s)`. Hurwitz zeta accepts the two-argument
forms `zeta(s,a)` and `ζ(s,a)`, as well as `zetah(s,a)` and `zeta2(s,a)`.
Their first-argument derivatives use the trailing-`p` names `zetap`,
`zatahp`, and `zeta2p`.

A finite tangent progression is shown with its q-digamma identity rather than
pretending that a large explicit summation was performed symbolically. MARS
uses the reduced identity

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}\tan(kx) =in+\frac{\psi_{e^{4ix}}(1)-\psi_{e^{4ix}}(n+1)-\psi_{e^{2ix}}(1)+\psi_{e^{2ix}}(n+1)}{x}.\end{array}$

The Expression and Function cards show the same q-digamma formula. The Value
card evaluates the recognised finite combination by a stable native real sum:
the separate q-digamma terms are not assigned artificial values on the unit
circle. Using the displayed formula as input recovers the same summation,
identity, bindings and value.

Inverse circular and inverse hyperbolic progressions use log-gamma identities
where those identities are genuinely shorter than the original sum. Define

$\quad\begin{array}{l}\displaystyle D_n(s)=\ln\Gamma(n+1+s)-\ln\Gamma(1+s) +\ln\Gamma(1-s)-\ln\Gamma(n+1-s).\end{array}$

Then MARS uses

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}\operatorname{acot}(kx)=\frac{D_n(i/x)}{2i}, \qquad \sum_{k=1}^{n}\operatorname{acoth}(kx)=\frac{D_n(1/x)}{2},\end{array}$

and

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}\operatorname{atanh}(kx) =\frac{n\left(\ln x-\ln(-x)\right)+D_n(1/x)}{2}.\end{array}$

For real non-zero `x`, arctangent uses the sign-aware complex-shift log-gamma identity

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}\operatorname{atan}(kx) =\frac{n\pi x}{2|x|}+\frac{i}{2}\left( \ln\Gamma\left(n+1+\frac{i}{x}\right)-\ln\Gamma\left(1+\frac{i}{x}\right) +\ln\Gamma\left(1-\frac{i}{x}\right)-\ln\Gamma\left(n+1-\frac{i}{x}\right) \right).\end{array}$

This uses `ln(-ix) - ln(ix) = -iπx/|x|`, keeps the imaginary unit out of the
denominator, and preserves the principal-logarithm branches for positive and
negative real `x`. The Value card evaluates the supplied real terms directly at
the active precision, while the reduced identity remains visible in every
non-Value card. At `x = 0`, the finite sum evaluates directly to zero. Other
displayed log-gamma formulae may likewise use their finite terms to cross a
removable numerical singularity without hiding the symbolic identity.

Differentiating the arctangent progression uses the compact conjugate-pair form

$\quad\begin{array}{l}\displaystyle \frac{\psi(n+1+i/x)-\psi(1+i/x)+\psi(n+1-i/x)-\psi(1-i/x)}{2x^2} =\sum_{k=1}^{n}\frac{k}{1+k^2x^2}.\end{array}$

MARS evaluates its complex digamma terms through the active arbitrary-precision
backend and returns the mathematically real result. At 386 requested digits,
`{ @Z_(k=1)^n atan(kx) | x=1; n=1000000000 }` therefore returns a 386-digit
derivative Value beginning `20.6286155168239341793067112756040338`, without a
spurious imaginary residue.

The other inverse circular, inverse versed and inverse hyperbolic functions
have no shorter identity in MARS's current special-function vocabulary. Their
Rendered TeX and Expression cards therefore keep the sigma visible, and the
Function card explicitly retains `sum(...)`. With supplied finite bounds of at
most one million terms, the Value card evaluates that displayed finite sum
directly. The unchanged sigma and `sum(...)` make this numerical route visible;
they do not imply an undisplayed closed form. When a sum exceeds the safe direct
evaluation limit and has no supported shortcut, the Value card instead displays
`Value not computed: the finite sum exceeds the safe direct-evaluation limit and
has no supported numerical shortcut.`

Integrating an inverse-function progression likewise operates term by term and
retains the finite sigma when the resulting weighted logarithmic sum has no
shorter supported form. MARS does not attempt a large numerical sum merely to
populate the integral Value.

An exact inverse-function pole remains a numerical result rather than becoming
an absent Value. For example,
`{ @Z_(k=1)^n acoth(kx) | x=1; n=1000000000 }` contains
`acoth(1)` as its first term, so its Value is `∞`; changing `x` to `-1` gives
`-∞`. The same signed-infinity handling applies when a finite `atanh(kx)`
progression reaches `kx = 1` or `kx = -1`.

The corresponding hyperbolic-tangent progression uses a real q-digamma
identity. With `q = exp(-2x)`, MARS uses

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}\tanh(kx) =n-\frac{2}{\log q}\left(\psi_q(1)-\psi_q(n+1)\right) +\frac{4}{\log(q^2)}\left(\psi_{q^2}(1)-\psi_{q^2}(n+1)\right).\end{array}$

For example, `{ @Z_(k=1)^n tanh(kx) | x=2; n=100000 }` displays this
identity in Rendered TeX, uses `ψq` in Expression style and `qdigamma` in
Function style, and produces
`99999.96334436219613677993759997968616482017617755510022062027824`.
Using that displayed Expression as input recovers the sum identity and the
same value.

The cotangent, secant and cosecant families use the same native q-digamma
machinery. Write

$\quad\begin{array}{l}\displaystyle D_q(a,n)=\psi_q(a)-\psi_q(a+n).\end{array}$

Then the cotangent pair is

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}\cot(kx)=-in-\frac{2i}{\log q}D_q(1,n),\qquad q=e^{2ix},\end{array}$

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}\coth(kx)=n+\frac{2}{\log q}D_q(1,n),\qquad q=e^{-2x}.\end{array}$

The cosecant pair follows from
`cosec(y) = cot(y/2) - cot(y)` and
`cosech(y) = coth(y/2) - coth(y)`:

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}\operatorname{cosec}(kx) =-\frac{2i}{\log q}D_q(1,n)+\frac{2i}{\log(q^2)}D_{q^2}(1,n), \qquad q=e^{ix},\end{array}$

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}\operatorname{cosech}(kx) =\frac{2}{\log q}D_q(1,n)-\frac{2}{\log(q^2)}D_{q^2}(1,n), \qquad q=e^{-x}.\end{array}$

Finally, let `a = pi*i/(2*ln(q))`. The secant identities are

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}\sec(kx) =\frac{i}{\log q}\left(D_q(1-a,n)-D_q(1+a,n)\right), \qquad q=e^{ix},\end{array}$

$\quad\begin{array}{l}\displaystyle \sum_{k=1}^{n}\operatorname{sech}(kx) =\frac{i}{\log q}\left(D_q(1-a,n)-D_q(1+a,n)\right), \qquad q=e^{-x}.\end{array}$

MARS displays each identity in Rendered TeX, writes `ψq` in Expression style
and `qdigamma` in Function style, and recovers the original finite sum when the
displayed Expression is used as input. Circular values and the
shifted-argument `sech` value are evaluated through the recognised real finite
sum, avoiding unsupported unit-circle terms and spurious complex cancellation;
the remaining real-q hyperbolic forms are evaluated directly.

The result cards deliberately show different representations of the native
calculation:

- **Rendered TeX** shows the simplified mathematical result without bindings.
- **Expression** shows the parseable MARS expression, including variable and
  constant bindings when the input has them. Riemann and Hurwitz zeta use the
  shared mathematical symbol `ζ`, distinguished by their one- and two-argument
  forms.
- **Function** shows MARS function source. Its **RUN** control executes the
  initial scalar, equation and matrix Ophelia subset described below. Reused expression-DAG nodes
  are named once as intermediate constants or variables before the return
  expression. A shared subexpression such as `x/2` is assigned once and reused.
  When both `exp(x)` and `exp(-x)` are needed, the second temporary reuses the
  first as `v2 = 1/v1`.
  For a top-level Laplace, Fourier or inverse-transform request, the programme
  preserves the original transform operation: RUN performs it instead of returning
  a precomputed formula. TeX and Expression continue to show the simplified result.
- **Value** appears whenever supplied bindings allow a numerical result. It is
  the only card that substitutes those bindings; it also appears when
  simplification proves a binding-independent value despite an unset binding.

**Use as input** takes its source from the native parseable Expression result,
not from TeX or Function presentation. Its binding-aware transfer preserves
the expression body's order and notation while applying the result bindings to
the editor controls.

Binding controls retain the native Expression spelling internally when
editing, clearing or transferring bindings. Their visible name labels,
tooltips and derivative/integral buttons omit enclosing square brackets.
The calculus buttons italicise only the variable name, leaving the action word
upright. Multi-character names keep their square brackets in Expression syntax, while ordinary Ophelia
identifiers are unbracketed in Function source. Rendered TeX also omits these
brackets, presenting each multi-character name as a single italic identifier.
Native TeX inserts a small centred dot between these names and adjacent factors
to make multiplication clear.
Products of single-letter symbols retain their existing notation.

[![MARS Lab expression mode displaying an inverse-power series as a sigma, Hurwitz-zeta formula and numerical value](images/mars-lab/expression.png?v=20260820-2)](images/mars-lab/expression.png?v=20260820-2)

Function cards use MARS syntax rather than C syntax. A full stop terminates a
statement, `.` within a statement denotes multiplication, and `/` is printed
without surrounding spaces. Constants use `const`; arrays use `array`; and
generated intermediate values use compact names such as `c1` and `v1`.
The source annotation above a generated function is enclosed by single
backticks, so it remains one valid delimited comment even when the card wraps
it over several visual lines. Two backticks remain the separate line-comment
syntax.
Typeable named constants use `@pi`, `@phi`, `@gamma`, `@tau` and `@inf`, with
their own syntax colour. Values edited in Expression-mode binding controls are
committed to the expression before evaluation, differentiation or integration.
Each populated binding input has an × button inside its right edge. It clears
the value using the same rules as deleting it manually. For ordinary bindings,
this leaves the value unset and returns focus to the input. The button is also
accessible by keyboard.
Descriptive `$[...]` identifiers remain accepted as input aliases for
bracketed names and use the subdued off-white italic styling of variable and
constant names, but result cards do not generate the `$[...]` form.
The syntax colouring distinguishes keywords, functions, variables, numbers and
comments. Function-call brackets use the same gold hue as operators without
bold weight, while grouping brackets retain the ordinary text colour. The
colouring does not alter the copyable Function text.

### Running Function cards

Every card titled **Function** has a **RUN** button, including the cards in
Expression, Equation and Matrix modes. RUN submits the card's complete,
unabbreviated source at the selected precision to the native Ophelia prototype.
It does not re-evaluate the editor input, execute JavaScript or change the
Expression and Value cards. Output and source-position diagnostics appear in
the Function card's **Run output** area. Replacing the result or switching modes
clears this area and prevents an older response from appearing on a newer card.
Evaluate again after editing bindings before pressing RUN.

The first supported subset accepts one expression, equation or matrix function with scalar
parameters, variable and constant assignments, generated intermediate values,
returns, numerically decidable comparisons, symbolic scalar domain guards, and
top-level output calls. Paired-backtick comments and double-backtick line
comments are accepted. A full stop followed by whitespace or the end of the
statement terminates it; an internal full stop multiplies. Commas can separate
ordinary assignments on one line. Function-local constant definitions retain
their exact algebra, including surds, rather than appearing as generated names
with decimal bindings in symbolic results. External parameters and unset local
constants remain bindings; mutable scalar assignments retain their live values.

Bracketed multi-character symbolic names are supported in expressions,
declarations, function parameters and transform coordinates. Their contents are
one name, not a product of letters or an array literal.

For example, RUN executes this Function source:

```text
expression expr(x, y) {
    return sqrt(x^2 + y^2).
}
x = 3.
y = 4.
output(expr(x, y)).
```

Output:

```text
5
```

Derivative and integral Function cards retain the requested operation, so RUN
performs the calculation. This applies both to authored calculus requests and
to the derivative/integral buttons. The TeX and Expression cards still show the
evaluated algebra.

Nested operations remain nested in both the Function programme and the
left-hand side of the transform's TeX identity. An unresolved transform also
retains those operations in TeX, without claiming an evaluated equality.
For input `@L{@S^t sin(x) dx}`,
the Function card contains:

```text
expression expr(s) {
    if (realpart(s) > 0) {
        return laplace(integral(sin(x), x, t), t, s).
    } else {
        return @nan.
    }
}
s = ?.
output(expr(s)).
```

RUN output:

```text
{ -s/(s² + 1) | s = ?; Re(s) > 0 }
```

The inner integral remains visible on the left of the TeX identity; it is not
replaced there by its evaluated primitive. The integration coordinates `x` and
`t` are local to their calls, not programme input bindings.
The Function card carries the same domain restrictions as the result, as an
`if` guard with an `@nan` fallback. Supplying a valid numerical binding does not
remove this guard from the generated function. With unset bindings, RUN retains
the condition on its symbolic result; outside the domain it returns `NAN`.

For an indefinite integral:

```text
expression expr(x, const C) {
    return integral(sin(x), x).
}
x = ?.
const C = ?.
output(expr(x, C)).
```

RUN output:

```text
{ C - cos(x) | x = ?; C = ? }
```

The two-argument call generates the integration constant itself. Its parameter
allows a supplied value of that constant to be used; the programme does not
append `+ C`. Definite integrals retain the four-argument form
`integral(expression, variable, lower, upper)` and generate no arbitrary constant.
Supplied bindings are preserved in the executable programme, including exact
symbolic values. With the indefinite integral above, setting `x` to `pi` and `C`
to zero produces:

```text
expression expr(x, const C) {
    return integral(sin(x), x).
}
x = @pi.
const C = 0.
output(expr(x, C)).
```

RUN output:

```text
1
```

The integrand always comes first and the integration variable second. For example:

```text
output(integral(sin(x), x, 0, @pi)).
```

Output:

```text
2
```

An explicit upper endpoint selects a primitive without adding an arbitrary
constant. Thus `@S^z sin(x) dx` generates this programme, and RUN agrees with
the TeX and Expression cards:

```text
expression expr(z) {
    return integral(sin(x), x, z).
}
z = ?.
output(expr(z)).
```

RUN output:

```text
{ -cos(z) | z = ? }
```

This is not a definite integral from zero: at `z = 0`, this chosen primitive
is `-1`, whereas the definite integral with both limits zero is `0`.

For a derivative:

```text
expression expr(x) {
    return derivative(sin(x), x, 1).
}
x = ?.
output(expr(x)).
```

RUN output:

```text
{ cos(x) | x = ? }
```

For an inverse Laplace request, the Function card keeps the calculation executable:

```text
expression expr(t) {
    return inverselaplace(5.(s + 3 + 10/s^2)/(s^2 + 4.s + 5), s, t).
}
t = ?.
output(expr(t)).
```

RUN output:

```text
{ 10t + exp(-2t)·(13·cos(t) + 11·sin(t)) - 8 | t = ? }
```

Here `s` is the transform's local source coordinate, not an uninitialised function
argument. Changing `t` to a numerical binding allows RUN to evaluate the result.

An unset binding may be written as `?`. Ordinary scalar output falls back to
native algebraic expression output when no numerical value is available;
`outputa` explicitly requests algebraic output. Stored expressions retain their
variable dependencies when numeric bindings are subsequently changed.
RUN displays unset variable and constant bindings as `name = ?`, matching the
Expression card; a standalone undefined numerical result remains `NAN`.
Generated scalar domain guards can retain an unset binding symbolically when
their `else` block returns `@nan`. Supported predicates are strict real-part
lower bounds, nonzero comparisons, real-variable checks and conjunctions of these
predicates. RUN preserves the native domain conditions alongside the returned
expression; it does not assume that an unknown condition is true or false.
For example:

```text
expression expr(s) {
    if (realpart(s) > 0) {
        return 1/s.
    } else {
        return @nan.
    }
}
s = ?.
output(expr(s)).
```

RUN output:

```text
{ 1/s | s = ?; Re(s) > 0 }
```

Supplying a binding evaluates the condition normally; values outside the domain
return `NAN`. Expressions constructed with symbolic guards retain their variable
dependencies for subsequent binding changes. General symbolic branching, including
an unknown condition whose branches return different ordinary expressions, is not
yet supported and produces a diagnostic. Boolean combinations retain normal
short-circuit behaviour, including when a known operand determines their result.

Equation functions return native equation values. `solve` accepts one equation
value and returns the native solver's solution set; output prints every returned
solution rather than choosing one root. Generated equation programmes use
`outputa()` to retain exact fractions and surds; an explicit `output()` still
requests numerical evaluation. For example:

```text
equation equ(Y) {
    return equation(26.Y = 320/9).
}
`` Y = ?
outputa(solve(equ(Y))).
```

Output:

```text
Y = ¹⁶⁰⁄₁₁₇
```

The commented hint leaves `Y` as an implicit uninitialised variable. Constant
parameters remain constants during solving. Equation output preserves the
left-hand side and evaluates the right-hand side when possible; `outputa`
keeps it algebraic. Unsuccessful solving reports that no solution was established,
not that none exists, unless the native solver proves an empty solution set.
Any native search limitations or separate solution-family notes are also printed.

Matrix functions execute the native generated return body, including shared
scalar temporaries, a common scalar prefactor and an additive constant matrix.
Scalar parameters may be real or complex. Matrix Function initialisers retain
authored exact bindings, including named constants, fractions and surds;
decimal input remains decimal input.
Named constants such as pi use mathematical symbols in both Expression layouts
and their Function aliases in Function initialisers, rather than decimal
approximations. `output` evaluates every entry when possible; otherwise it retains
the symbolic matrix and its bindings. `outputa`
always requests algebraic output. Long symbolic results use the native multiline
matrix layout, with shared non-vanishing powers factored out and their bindings
retained. For example:

```text
matrix mat(x) {
    const scale = 2.
    v1 = x^2.
    return scale.(v1, x; x + 1, v1).
}
x = 3.
output(mat(x)).
```

RUN output:

```text
(18, 6; 8, 18)
```

Generated matrix functions use the same principal complex powers as Matrix
mode. Fractional powers of negative eigenvalues can therefore produce complex
entries; they are not coerced to real values.

This is not yet the full Ophelia language. Matrix-valued parameters and assignments,
arrays, convolution, loops, recursion, multiple function definitions, nested
user-function calls, in-programme precision changes and undeclared captures
from an outer scope are not implemented. Equation and solution-set assignments
and the two-argument `solve(e, variable)` design are not implemented yet;
the generated one-argument solve-and-output form is supported. Native mathematical calls retain their
current expression semantics; an unresolved transform or formal derivative can
remain symbolic. Unsupported statement forms report an error rather than being
sent to a shell or silently ignored.

The separate native target is `scratch/ophelia`; the resulting
`build/release/scratch/ophelia` reads programme source from standard input and
accepts an optional decimal-precision argument. Lab requests have a 30-second
execution timeout. The prototype limits source to 64 KiB, scope size to 256
symbols, parameters to 64, nesting to 32, executed statements to 2048 and output
calls to 128. Shell, network and file-access operations are not language features.
The wider design remains in the
[Ophelia design notes](design-notes/expression-language.md).

## Equation mode

Zeta equations with an unset unknown use a bounded multi-start Newton search.
For a zero target, the cards show up to forty numerically verified non-trivial
zeros in the critical strip up to imaginary magnitude eighty; the trivial
negative-even family is labelled separately. Both **Rendered TeX** and
**Solutions** combine numerical conjugates into one `±` row, so twenty rows
can show forty roots at the selected precision. The native backend compares
the actual numbers before pairing; it does not merge rounded display values
or sampled symbolic families. Exact algebraic conjugates are paired structurally,
retaining surds and shared denominators with `=` rather than `≈`. Certified
rational factors preserve short cubic and quartic surds. General exact
real-coefficient cubics use coupled Cardano cube roots; non-degenerate quartics
use Ferrari radicals when no shorter factorisation is certified. RUN output
retains these exact forms too. Long solutions use shared, collision-free
single-letter definitions: the cards show definitions and root rows, while
algebraic RUN output puts the definitions after a binding bar in a curly-braced
solution block. Degenerate cases retain their existing handling, and inexact or
complex coefficients retain the numerical fallback. See the
[equation guide](equation.md) for details and examples.
Both cards omit a unit coefficient before the imaginary unit in conjugate
pairs. The search is not exhaustive. A literal
inverse-power series starting at one retains its convergence domain and, when
equated to zero, reports no solutions in that domain. It does not acquire
analytic-continuation zeros. **Solutions** says only **No solutions** for a
proved empty solution set; an unsuccessful numerical search remains a distinct
status. With no solution rows, **Rendered TeX** retains the entered equation in
summation notation. For a recognised infinite inverse-power sum, it also shows
the native zeta identity with the restriction that the real part of its argument
exceeds one. The identity uses the actual index, exponent and lower bound.
The editor, saved input, bindings, history, Equation and Function cards retain
the original problem. **Evaluate** does not start a separate zeta solve, and
there is no additional root card.
The native backend supplies the mathematics; the client only displays it.
Restart a running Lab server and reload the page after updating the client.

Equation mode tries symbolic isolation first and uses the numeric solver when a
symbolic result is unavailable. Bindings after `|` distinguish variables from
constants and supply starting values for numeric solving.
Equation calculations may run for up to 40 seconds before the Lab reports a
timeout. Other modes retain their own time limits.

The captured input is `atan(2x) + atan(x) = pi/4`. The exact output is
`x = (sqrt(17) - 3)/4`, accompanied by its decimal value.

[![MARS Lab equation mode returning an exact surd solution](images/mars-lab/equation.png?v=20260814-3)](images/mars-lab/equation.png?v=20260814-3)

## Differential-equation mode

Differential-equation mode accepts ordinary and partial derivatives,
polynomial differential operators, differential forms and optional initial or
boundary conditions. MARSlib selects a matching rule and, when derivations are
enabled, returns the rule-derived working shown in the **Solver** card.

The captured input is `y'' + x^2y = 0`. The output is the Bessel basis

$\quad\begin{array}{l}\displaystyle y = \sqrt{x}\left(C_1 J_{-1/4}\!\left(\frac{x^2}{2}\right) + C_2 J_{1/4}\!\left(\frac{x^2}{2}\right)\right).\end{array}$

[![MARS Lab differential-equation mode solving a power-law Bessel equation](images/mars-lab/differential-equation.png?v=20260814-3)](images/mars-lab/differential-equation.png?v=20260814-3)

Select **Help** in this mode for the accepted prime, `D`, partial-derivative and
differential-form notation. Arbitrary constants are preserved when the problem
has no conditions.

## Matrix mode

Matrix mode accepts complete numeric and symbolic matrix expressions. Spaces
separate columns and semicolons separate rows in compact input; comma-separated
entries are also accepted. Write matrix functions directly in the expression,
so `sin(1 2; 4 5)` means the sine of that complete matrix. The **Matrix
operation** selector provides **Evaluate expression**, **Inverse**, **Multiply
by another matrix**, **Eigenvalues**, **Eigendecompose**, **Characteristic
polynomial**, **Determinant**, **Trace**, **Rank**, **Simplify symbolic matrix**
and **Solve A X = B**. Functions such as the inverse, logarithm and
trigonometric families require a square matrix.

MARS Lab passes the entered text unchanged to
`mat_expression_from_string(...)`. MARSlib owns the complete grammar and
performs all matrix parsing and evaluation; neither the browser nor the native
MARS Lab helper interprets matrix-expression syntax.

Rendered matrix values automatically fit the available Value-card width at the
default zoom. Zoom and expansion recalculate that fit, so a wide matrix remains
visible without changing the native matrix output.

### Matrix-expression notation

Matrix expressions may be grouped and composed directly. `.` is matrix
multiplication, while `+` and `-` combine equally sized matrices. Integer,
fractional and symbolic powers use `^`; for example,
`((1 2; 3 4) - lambdaI)^x` is accepted. Where the matrix order is clear,
`lambdaI`, `lambda.I` and `lambda*I` all mean the scalar `lambda` multiplied by
the identity matrix. Matrix division is deliberately not defined because the
side on which an inverse should act would be ambiguous.

The structural function names and their aliases are:

| Operation | Accepted notation |
|---|---|
| Inverse | `inverse(A)`, `inv(A)` |
| Determinant | `det(A)`, `determinant(A)`, `|A|`, `||A||`, `‖A‖` |
| Trace | `trace(A)`, `tr(A)` |
| Transpose | `transpose(A)`, `trans(A)` |
| Conjugate transpose | `hermitian(A)`, `adjoint(A)`, `ctranspose(A)`, `conjtrans(A)`, `conjugate_transpose(A)`, `A^dagger`, `A^H`, `A^*`, `A^†`, `A†` |

Any unary scalar function supported by the native expression registry may be
written around a square matrix. This includes exponential, logarithmic,
trigonometric, inverse-trigonometric, hyperbolic, error, gamma, normal-density,
Lambert W and exponential-integral families. The parser reports the canonical
function name even when an alias such as `log`, `log10`, `Γ` or `productlog`
was entered. In particular, `log` and `log10` report the canonical base-10 name
`lg`, whereas natural logarithms report `ln`. Exact symbolic matrices are
supported where MARSlib has an exact
structured rule; otherwise, bind the entries to obtain a numeric matrix before
applying a general numeric matrix function. Symbolic exponents on supported
constant diagonalizable numeric square matrices are retained for any matrix
order; the exact `1 x 1` and eligible `2 x 2` cases additionally retain exact
spectral projectors.

Determinant bars must be paired: an input beginning with `|` or `||` without
the corresponding closing delimiter is rejected. A determinant is a scalar,
not a one-by-one matrix. The function vocabulary is recognised by MARSlib's
native collision-free lookup table; the browser does not recognise or rewrite
these names.

Scalar entries use the expression grammar. `conj(z)` and `conjugate(z)` are
equivalent to the postfix form `z^*`. Likewise, `abs(z)` and `|z|` denote the
same scalar absolute value. For a complex expression this is the modulus
`sqrt(z*z^*)`. Context distinguishes scalar absolute-value bars from the
determinant bars surrounding a matrix.

`sqrt(z)`, `cubrt(z)`, and `root(z,n)` return one principal scalar root. Exact
complex arguments are written in Cartesian `a + bi` surd form when such a form
is available. Explicit fractional-power syntax such as `z^(1/n)` instead
denotes the complete family of `n` roots when MARS Lab presents an expression
result.

Greek names may be entered as Unicode or through their ASCII aliases. Thus
`lambda`, `@lambda` and `λ` identify the same symbol and are normalised to `λ`
in output. This also applies inside compact matrix literals and identity
multiples.

Entrywise calculus uses `Dx(A)` for differentiation. Write `@S(A)dx` for an
indefinite entrywise integral with an additive constant matrix, or `@S^x(A)dx`
for the corresponding antiderivative without additive constants. Repeating a
derivative variable requests higher-order calculus, as in `Dxx(A)`, while a
suffix containing distinct variables requests ordered mixed calculus, as in
`Dxy(A)`. The variable buttons
below the editor invoke the corresponding first-order native operation. A
matrix antiderivative is displayed as `A(x) + C`, where `C` is a constant
matrix with entries such as `C₁₁`, `C₁₂`, `C₂₁` and `C₂₂`.

| Input | Output |
|---|---|
| `Dxx(x^3 xy; y^2 x^2y)` | `(6x, 0; 0, 2y)` |
| `Dxy(x^2y x*y^2; y^3 x^3y)` | `(2x, 2y; 0, 3x²)` |

For a symbolic matrix-power example, enter `A^x` with `A = (1 2; 3 4)`.
Writing

- `λ₊ = (5 + √33)/2` and `λ₋ = (5 - √33)/2`, and
- `P₊ = (A - λ₋I)/√33` and `P₋ = (λ₊I - A)/√33`,

the evaluated expression is `A^x = λ₊^x P₊ + λ₋^x P₋`. The **x derivative**
button returns
`ln(λ₊)λ₊^x P₊ + ln(λ₋)λ₋^x P₋`. The **x integral** button returns
`λ₊^x P₊/ln(λ₊) + λ₋^x P₋/ln(λ₋) + C`, where `C` is the independent constant
matrix. MARS Lab expands these projector expressions into a `2 x 2` matrix in
the result cards while retaining the exact `√33` terms.

MARSlib applies the spectral scalar rule before reconstructing the matrix. In
compact notation, `d(A^x)/dx = A^x ln(A)` and, when `ln(A)` is invertible,
`∫A^x dx = A^x inverse(ln(A)) + C`. An eigenvalue-one projector is integrated
as a linear term rather than divided by zero. The same machinery supports
ordered higher and mixed derivatives and iterated integrals, and is not limited
to `2 x 2` matrices.

<figure>
  <a href="images/mars-lab/matrix.png?v=20260815-6"><img src="images/mars-lab/matrix.png?v=20260815-6" alt="MARS Lab evaluating the symbolic matrix power (1 2; 3 4) raised to x"></a>
  <figcaption><em>After clicking <strong>Evaluate</strong>.</em></figcaption>
</figure>

<br>

<figure>
  <a href="images/mars-lab/matrix-power-derivative.png?v=20260815-5"><img src="images/mars-lab/matrix-power-derivative.png?v=20260815-5" alt="MARS Lab differentiating the symbolic matrix power with respect to x"></a>
  <figcaption><em>After clicking <strong>x derivative</strong>.</em></figcaption>
</figure>

<br>

<figure>
  <a href="images/mars-lab/matrix-power-integral.png?v=20260815-5"><img src="images/mars-lab/matrix-power-integral.png?v=20260815-5" alt="MARS Lab integrating the symbolic matrix power with respect to x and displaying the constant matrix"></a>
  <figcaption><em>After clicking <strong>x integral</strong>.</em></figcaption>
</figure>

Result cards have distinct purposes:

- **Rendered TeX** shows the exact symbolic result. Long decimal mantissas are
  abbreviated with an ellipsis by default. Large integers use 23 significant
  digits followed by an ellipsis and an `e` exponent in the Expression and
  Function cards, for example `1.3322938598456934859438...e+45`.
  **Show more digits** reveals the complete exact integer in those cards.
  Rendered TeX instead uses a multiplication sign and a power of ten. The
  Value card is never abbreviated and wraps its complete numerical value.
- **Expression** shows the same native simplified matrix expression as a
  bracketed grid. Variable and constant bindings are placed on a separate row
  beneath the matrix, and unset bindings are shown as `?`. Copy still returns
  the complete native expression in reusable curly-brace notation.
- **Function** shows the same result as a native MARS matrix function, followed
  by the declarations and initialisations for its bindings. Repeated
  calculations shared by several entries are assigned once to intermediate
  variables and reused throughout the returned matrix.
- **Value** appears only when every matrix entry can be evaluated numerically.
  It remains hidden while a binding or integration constant is unresolved, and
  its complete numerical entries wrap within their columns rather than being
  abbreviated. For example, setting `lambda` to `3` evaluates
  `(1 2; 3 4) - lambdaI` to `(-2 2; 3 1)` without replacing the symbolic
  algebra in the other cards.

MARSlib creates all four representations from the same simplified matrix. The
browser transports, displays and compacts the native fields; it does not parse,
simplify or reinterpret the matrix mathematics.

Symbolic matrix calculus constructs expression DAGs. It does not depend on the
numeric automatic-differentiation mode: the scalar expression evaluator has a
reverse-mode gradient path, while a numeric forward-mode JVP path is a separate
future facility. The symbolic Jacobian is therefore not described as either a
JVP or a VJP.

**Use as input** copies the reusable result expression back into the Matrix
editor. **Back** and **Forward** navigate the Lab's Matrix workspace history;
the editor, selected operation and right-hand operand are retained between
sessions.

The direct symbolic forms use the same editor:

| Input | Output |
|---|---|
| `inverse(a b; c d)` | `1/(ad - bc).(d, -b; -c, a)` |
| `det((1 2; 3 4) - lambdaI)` | `(1-λ)(4-λ)-6` |
| `tr(a b; c d)` | `a+d` |
| `(a b; c d)^dagger` | `(conj(a) conj(c); conj(b) conj(d))` |
| `(a b; c d).(e f; g h)` | `(ae+bg, af+bh; ce+dg, cf+dh)` |
| `inverse(a b; c d).(x; y)` | `1/(ad - bc).(dx - by; ay - cx)` |
| `Dx(ax+b cx+d; y xy)` | `(a, c; 0, y)` |
| `Dxx(x^3 xy; y^2 x^2y)` | `(6x, 0; 0, 2y)` |
| `Dxy(x^2y x*y^2; y^3 x^3y)` | `(2x, 2y; 0, 3x²)` |
| `@S(ax+b cx+d; y xy)dx` | `(½(ax²+2bx), ½(cx²+2dx); xy, ½x²y) + (C₁₁, C₁₂; C₂₁, C₂₂)` |

## Integrator mode

Integrator mode combines exact symbolic antiderivatives with the numerical
integrator. Add one bound row for each variable to be integrated. Leave both
bounds blank for an antiderivative, or enter lower and upper bounds for a
definite integral. Mark a symbol **Free** when it is a parameter rather than an
integration variable. The work-budget selector limits numerical fallback.

The captured input is `sin(x)^2` with `x` from `0` to `1`. MARS returns the
exact antiderivative `(2x - sin(2x))/4` and the definite output
`(2 - sin(2))/4`.

[![MARS Lab integrator mode returning exact indefinite and definite results](images/mars-lab/integrator.png?v=20260814-3)](images/mars-lab/integrator.png?v=20260814-3)

## Datetime mode

Datetime mode combines civil-calendar, jurisdiction, solar, lunar and optional
weather calculations. Enter a selected date, a date range and an observer
location. A Julian day number may be used in place of the selected civil date.
The GMT offset includes daylight saving when applicable.

The captured request uses 20 August 2026 in Shrewsbury, with a date range
ending on 1 January 2027. Its output includes the weekday, sunrise and sunset,
moonrise and moonset, moon phase, clock changes, the asynchronously added
weather summary, the selected date range and the year's calendar observances.

[![MARS Lab datetime mode showing calendar and astronomical results followed by asynchronously loaded weather](images/mars-lab/datetime.png?v=20260820-1)](images/mars-lab/datetime.png?v=20260820-1)

MARS supplies no shared WeatherAPI account or key. Weather is shown only when
the person installing MARS Lab creates their own WeatherAPI account, configures
that account's key during desktop installation, and the service can be reached.
The calendar and astronomical results do not depend on that optional service.
They are displayed as soon as the native datetime helper completes; weather is
fetched asynchronously and updates its own card afterwards, without delaying
or replacing those results.

Each lookup sends the configured key, selected date, latitude and longitude
from the local Lab server to WeatherAPI.com over HTTPS. The key is not sent to
the browser. MARS does not cache or persist the returned weather response,
although the date and coordinates remain in private local Lab state so that its
inputs can be restored. The weather card identifies WeatherAPI.com as its
source and displays the required end-user warning. Forecasts and conditions are
probabilistic and may be inaccurate for an exact place or time. Do not use them
as the sole basis for personal safety, aviation, marine navigation, emergency
planning or any other safety-critical decision; consult official
meteorological services and authorities. See the [MARS privacy notice](privacy.md),
[WeatherAPI privacy policy](https://www.weatherapi.com/privacy.aspx) and
[WeatherAPI terms](https://www.weatherapi.com/terms.aspx).

## Almanac mode

Almanac mode is the AstroNav worksheet. Enter a GMT date and time, time-zone
offset, latitude, longitude and altitude. The packaged ephemeris covers
1550–2649 GMT. Results include declination, Greenwich hour angle, right
ascension and observer-relative altitude and azimuth for the navigational
bodies.

The captured request is for London at `2026-08-08 09:02:43` GMT. The output is
the navigational-body table headed by the Sun, Moon, Mercury, Venus, Mars,
Jupiter and Saturn.

[![MARS Lab almanac mode showing the navigational-body worksheet](images/mars-lab/almanac.png)](images/mars-lab/almanac.png)

## Mobile and private remote access

When MARS Lab listens on its normal wildcard address, its **Mobile** control
shows the best private access route currently available:

- with Tailscale active, the QR code contains the private Tailscale URL;
- otherwise, it contains a local Wi-Fi URL when one is available;
- when neither route is reachable, no mobile URL is advertised.

For access away from the local network, connect both the MARS computer and the
mobile device to the same Tailscale network, start MARS Lab, open **Mobile** and
scan the displayed code. MARS Lab configures private Tailscale Serve when it
can; it deliberately does not enable public Tailscale Funnel access. A phone
that is not connected to the same Tailscale network cannot use the private QR
code.

## Troubleshooting

- **No rendered mathematics:** install `texlive-latex-base` and `dvisvgm`, then
  restart the Lab.
- **A helper is missing:** run `make release` and restart the Lab from the
  repository root.
- **A mobile QR code is absent or unreachable:** confirm that MARS Lab is using
  the wildcard host, then check that the phone is on the same Wi-Fi network or
  Tailscale network as the computer.
- **A result is too wide:** use the result card's zoom controls. Long rendered
  mathematics is broken over lines where possible and continues vertically.
  Lines that still exceed the card width have a horizontal scrollbar, including
  in expanded cards.
- **A previous result is still visible:** evaluate the new input or use
  **Clear**. **Back** and **Forward** navigate the Lab's own workspace history.
