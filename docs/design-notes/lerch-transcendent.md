# Lerch Transcendent Support

> Status: implemented for the defining convergence domain, with further
> analytic continuation remaining future work.

MARS supports the Lerch transcendent

\[
\Phi(z, s, a) = \sum_{k=0}^{\infty} \frac{z^k}{(k+a)^s}.
\]

One motivating symbolic-integration case is

\[
\int \sum_{k=1}^{n} \cosh(kx)\,dx
= \sum_{k=1}^{n} \frac{\sinh(kx)}{k} + C.
\]

The finite sum can alternatively be represented as

\[
\frac{1}{2}\left[
\operatorname{Li}_1(e^x)-\operatorname{Li}_1(e^{-x})
-e^{(n+1)x}\Phi(e^x,1,n+1)
+e^{-(n+1)x}\Phi(e^{-x},1,n+1)
\right] + C.
\]

The corresponding weighted cosh sum is

\[
\sum_{k=1}^{n}\frac{\cosh(kx)}{k}
=\frac{1}{2}\left[
\operatorname{Li}_1(e^x)+\operatorname{Li}_1(e^{-x})
-e^{(n+1)x}\Phi(e^x,1,n+1)
-e^{-(n+1)x}\Phi(e^{-x},1,n+1)
\right].
\]

MARS Lab displays the Lerch representation whenever either weighted
hyperbolic sum is recognised. Consequently, a result produced quickly for a
very large endpoint visibly exposes the bounded-work formula; it is never
presented as though every term had been summed directly. Individual terms can
cross complex logarithmic branch cuts even though their combination is real,
so the combined expression controls branch selection and real-result recovery.

## Public Numeric Families

The public implementation families are `qf_lerch_phi()`, `qc_lerch_phi()`,
`number_lerch_phi()`, `num_lerch_phi()`, `expr_lerch_phi()`, and
`mat_lerch_phi()`. The order-one polylogarithm is a distinct native operation,
not a renamed two-argument polylogarithm node; its corresponding public
families are `qf_polylog1()`, `qc_polylog1()`, `num_polylog1()`,
`expr_polylog1()`, and `mat_polylog1()`.

Numerical evaluation currently covers the defining disc \(|z|<1\), together
with the exact reductions at \(z=0\), \(z=1\), and \(s=0\). Values requiring
general analytic continuation return NaN rather than selecting an accidental
branch. The finite weighted sinh and cosh sums have a separate stable numerical
evaluation which works backwards from their dominant endpoint, so a large
bound does not require one operation per term.

The matrix implementation evaluates the convergent power series with matrix
powers and scalar coefficients. It accepts square `number_t` matrices and
returns `NULL` when its preconditions or convergence coverage are not met.

## Expression Notation and Rendering

Expression input accepts `lerch_phi(z,s,a)`, `LerchPhi(z,s,a)`, and
`Φ(z,s,a)`. The collision-free function table also accepts `Li1(z)` and
`Li₁(z)`, alongside `Li2(z)` and `Li₂(z)` for the dilogarithm. Output uses:

- `Φ(z,s,a)` and `Li1(z)` in `style_EXPRESSION`;
- `lerchphi(z, s, a)` and `li1(z)` in `style_FUNCTION`; and
- `\Phi\left(z,s,a\right)` and `\operatorname{Li}_{1}(z)` in TeX.

Function temporaries preserve the reciprocal exponential relationship: after
`v1 = exp(x)`, the matching negative exponential is emitted as `v2 = 1/v1`.
The source annotation is enclosed by single backticks so it remains one
delimited comment when its presentation wraps across lines.

Formal user-input sums and products use `@Z_(k=1)^n term` and
`@P_(k=1)^n term`, with optional lower-bound parentheses. `@Z` is the ASCII
replacement for `Σ`. Omitting `^n` denotes an infinite operator. The index belongs
to the operator and does not become an ordinary expression binding. Sums use
the sigma symbol in mathematical expression output and
`sum(k, 1, n, term)` in Function output when no closed form replaces them.

## Calculus and Round Trips

Li₁ has dedicated forward and reverse derivatives and a direct
antiderivative. Lerch Φ supplies derivatives with respect to its `z`, `s`, and
`a` arguments; the `s` partial remains formal where no simpler native function
represents it. These operations remain available under repeated
differentiation and integration dispatch.

For each weighted hyperbolic identity, the simplifier recognises the complete
Li₁/Lerch expression as the finite source sum. Differentiating the weighted
sinh form therefore returns the existing finite-cosh identity

\[
\sum_{k=1}^{n}\cosh(kx)
=\frac{\sinh(nx/2)\cosh((n+1)x/2)}{\sinh(x/2)},
\]

rather than exposing the term-by-term derivative of the special-function
representation. The weighted cosh form correspondingly returns the existing
finite-sinh identity. Supplied bindings are retained, so each result also has a
numeric derivative value.

MARS Lab's **Use as input** action receives an exact parseable editor
expression from the native result. The client preserves that expression body
and applies its bindings; it does not derive input from rendered TeX, parse the
special functions, simplify the algebra, or perform substitution itself.

Expression construction, simplification, parsing, rendering, multi-argument
differentiation, formal integration dispatch, matrix power-series evaluation,
stable finite-sum evaluation and round-trip recovery are implemented. General
branch-aware analytic continuation remains a future extension.
