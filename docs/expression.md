# `expr_t`

`expr_t` is a reference-counted expression DAG for differentiable values.

## Laplace transforms

Expression mode accepts the forward unilateral Laplace operator `@L`, its
canonical spelling `ℒ`, and the function spelling `Laplace`. The source variable
defaults to the free variable `t` when present, otherwise it is inferred when
exactly one free variable is present. Other symbols remain parameters for the
transform without changing their binding kinds. An explicit source overrides
inference, and a distinct target can also be supplied. Multiple free variables
without `t` require an explicit source.

The Rendered TeX card presents evaluated transforms as identities, with the
calligraphic Laplace operator and source expression on the left and the
algebraic result with its convergence conditions on the right. The Expression
and Function cards retain the algebraic result alone. Supplied constant
parameters are specialised consistently on both sides; unresolved transforms
remain a single unevaluated operator.

| Input | Algebraic result |
|---|---|
| `@L(t)` | $1/s^2,\quad \operatorname{Re}(s)>0$ |
| `@L(J_0(t))` | $1/\sqrt{s^2+1},\quad \operatorname{Re}(s)>0$ |
| `@L(sqrt(t))` | $\sqrt{\pi}/(2s^{3/2}),\quad \operatorname{Re}(s)>0$ |
| `@L(cubrt(t))` | $\Gamma(4/3)/s^{4/3},\quad \operatorname{Re}(s)>0$ |
| `@L(exp(-t))` | $1/(s+1),\quad \operatorname{Re}(s)>-1$ |
| `@L(e^(a*t))` | $1/(s-a),\quad \operatorname{Re}(s)>\operatorname{Re}(a)$ |
| `@L(exp(a*t+b),t)` | $e^b/(s-a),\quad \operatorname{Re}(s)>\operatorname{Re}(a)$ |
| `@L(sin(t))` | $1/(s^2+1),\quad \operatorname{Re}(s)>0$ |
| `@L(erf(t))` | $e^{s^2/4}\operatorname{erfc}(s/2)/s,\quad \operatorname{Re}(s)>0$ |
| `@L(erf(t/(2a)))` | $a e^{a^2s^2}\operatorname{erfc}(s\sqrt{a^2})/(s\sqrt{a^2}),\quad \operatorname{Re}(s)>0,\quad \operatorname{Re}(a^2)>0$ |
| `@L(erfc(t))` | $(1-e^{s^2/4}\operatorname{erfc}(s/2))/s,\quad \operatorname{Re}(s)>0$ |
| `@L(tsin(at))` | $2as/(s^2+a^2)^2,\quad \operatorname{Re}(s)>\operatorname{Re}(ai),\quad \operatorname{Re}(s)>\operatorname{Re}(-ai)$ |
| `@L(sinh(at))` | $a/(s^2-a^2),\quad \operatorname{Re}(s)>\operatorname{Re}(a),\quad \operatorname{Re}(s)>\operatorname{Re}(-a)$ |
| `@L(sinh(@omega*t))` | $\omega/(s^2-\omega^2),\quad \operatorname{Re}(s)>\operatorname{Re}(\omega),\quad \operatorname{Re}(s)>\operatorname{Re}(-\omega)$ |
| `@L(cosh(at))` | $s/(s^2-a^2),\quad \operatorname{Re}(s)>\operatorname{Re}(a),\quad \operatorname{Re}(s)>\operatorname{Re}(-a)$ |
| `@L(t,t,p)` | $1/p^2,\quad \operatorname{Re}(p)>0$ |
| `@L(f(t))` | $\mathcal{L}_{t\to s}\{f(t)\}$ |
| `@L(t^n,t)` | $\Gamma(n+1)/s^{n+1},\quad \operatorname{Re}(s)>0,\quad \operatorname{Re}(n)>-1$ |
| `{@L(t^n) \| s=?; n=5}` | $120/s^6,\quad \operatorname{Re}(s)>0$ |
| `{@L(sin^n(t)) \| s=1; n=5}` | $120/((s^2+25)(s^4+10s^2+9)),\quad \operatorname{Re}(s)>0$; value $3/13$ |

The initial rule set covers constants, source powers with exponent real part greater than −1,
including square roots, cube roots, integer-order roots and their reciprocals.
Root notation uses the same rule as fractional powers. Half-integer gamma factors
remain exact multiples of $\sqrt{\pi}$; other non-integer gamma factors stay
symbolic. Fractional powers remain explicit when rewriting them as roots could
change the principal complex branch. General root orders must be known integers
greater than one; supplied constant order bindings are accepted.
The rules also cover
linear combinations and source-independent factors, symbolic affine
exponentials, sine and cosine with symbolic affine arguments, including complex
rates and offsets. Both `exp(...)` and powers of Euler's constant
are recognised. Exponential convergence uses the real part of the rate; negative
rates do not acquire an unnecessary restriction to positive target real parts.
For sine and cosine, both exponential components must converge:
$\operatorname{Re}(s)>|\operatorname{Im}(a)|$. This reduces to
$\operatorname{Re}(s)>0$ for real frequencies.
Multiplication by a known positive integer power of the source variable uses
$\mathcal L\{t^m f(t)\}=(-1)^m F^{(m)}(s)$, retaining the original convergence
conditions. Orders up to 64 are expanded by repeated differentiation; unsupported
orders remain symbolic. Supplied constant integer orders are accepted.
Error functions and their complements accept affine arguments. With
$q=\sqrt{a^2}$ on the principal branch, the error-function transform is
$$
\mathcal L\{\operatorname{erf}(at+b)\}
=\frac{\operatorname{erf}(b)+(a/q)e^{s^2/(4a^2)+sb/a}
\operatorname{erfc}(s/(2q)+bq/a)}{s}.
$$
The sufficient conditions are $\operatorname{Re}(s)>0$ and
$\operatorname{Re}(a^2)>0$; both signs of non-zero real rates are covered.
The complementary transform is one over the target variable minus this result.
A supplied zero rate is handled separately. The unshifted positive-rate formula
is [DLMF 7.14.2](https://dlmf.nist.gov/7.14.E2); the affine extension follows by
integration by parts and the Gaussian integral.
Reciprocal argument scales are cancelled before introducing square roots, so
divided arguments do not retain nested reciprocal powers. Principal square roots
remain explicit unless the parameter values justify further simplification;
$\sqrt{a^2}$ must not be replaced by $a$ for an unspecified parameter.
**Known numerical limitation:** the existing complex error-function evaluator
can return incorrect values. The symbolic transform is valid under its stated
conditions, but numerical validation currently covers real rates, offsets and
positive real target values only.
Hyperbolic sine and cosine also accept symbolic affine arguments, including complex
rates and offsets. Their two displayed half-plane conditions are equivalent to
$\operatorname{Re}(s)>|\operatorname{Re}(a)|$, so both exponential components
converge. These are sufficient conditions, retained even for degenerate zero rates.
Non-negative integer powers of sine/cosine with real numerical frequency and zero phase use an exact recurrence
through order 16 and a finite sum for larger or symbolic orders. There is no
order-16 support limit. Supplied constant exponents are resolved before choosing
the rule. A symbolic exponent retains the explicit condition that it is a
non-negative integer; negative, fractional and non-real values fail that condition.

For example, `@L(cos(t)^n,t)` gives the finite-sum result

$$
\frac{1}{2^n}\sum_{k=0}^{n}\binom{n}{k}
\frac{s}{s^2+(n-2k)^2},\qquad \operatorname{Re}(s)>0,\quad n\in\mathbb{Z}_{\ge0}.
$$

Similarly, `@L(sin(t)^n,t)` gives

$$
\frac{1}{(2i)^n}\sum_{k=0}^{n}\frac{(-1)^k\binom{n}{k}}{s-i(n-2k)},
\qquad \operatorname{Re}(s)>0,\quad n\in\mathbb{Z}_{\ge0}.
$$

The renderer currently expresses the binomial coefficients using gamma functions.
Both sums are exact; their integer-order restrictions survive copying through the
parseable `n ∈ ℤ≥0` condition. The Function card checks reality,
non-negativity and integrality before evaluating the sum. Summation-index expressions
remain inside the sum rather than becoming outer function temporaries.
Unsupported expressions retain a symbolic transform. Unknown function calls
inside the operator are preserved as functions, not implicit multiplication.

Recognised transforms simplify to a native formula with convergence restrictions.
Both Laplace directions accept braces as well as parentheses. For example,
`@L{ln(t)}` gives

$$
\mathcal L_{t\to s}\{\ln(t)\}=-\frac{\gamma+\ln(s)}{s},\qquad \operatorname{Re}(s)>0,
$$

where $\gamma$ is the Euler–Mascheroni constant. At `s=1`, the value is
approximately `-0.5772156649015329`.
The inverse pair is also recognised: `@Linv{-(ln(s)+γ)/s}` returns `ln(t)`
for positive time, with `return ln(t).` in the Function card. More generally,
linear combinations of a logarithm and a constant divided by the source
variable are inverted algebraically.

Inverse Laplace transforms use `@Linv`, `ℒ⁻¹`, or `InverseLaplace`, with the
conventional source `s` and target `t`. An explicit source and target are accepted
in the same positions as for the forward operator. The inverse rules cover
linearity and partial fractions over products of linear and quadratic factors,
including repeated factors. Nested rational sums, products, quotients and integer
powers are first combined into a common fraction. Denominators retain their
authored factors; powers of the source variable can also be extracted from an
expanded polynomial. This is not a general high-degree polynomial factoriser.
These rules use the
causal unilateral convention: ordinary formulas describe the result for
non-negative time, not an arbitrary two-sided inverse.
Polynomial degree is limited to 16. Unproved symbolic pole separations,
quadratic frequencies that cannot be established as zero or non-zero, and
distributional inverses remain symbolic.

For example, a rational result copied from Equation mode can be inverted directly:

```text
@Linv{-5*(-s-10/s^2-3)/(s*(s+4)+5)}
```

Unbound expression output:

```text
10t + exp(-2t)·(13·cos(t) + 11·sin(t)) - 8
```

For example, `@Linv((s-a)/((s-a)^2+1))` produces

$$
\mathcal{L}^{-1}_{s\to t}\left\{\frac{s-a}{(s-a)^2+1}\right\}
= e^{at}\cos(t).
$$

The Function card returns `exp(a*t)*cos(t)` (using MARS multiplication syntax),
and numerical bindings `a=2`, `t=0.5` give approximately `2.385516730959136`.
An unknown input such as `@Linv(F(s),s,x)` remains `ℒ⁻¹(F(s), s, x)`;
its Function representation is `InverseLaplace(F(s), s, x)`.
Unsupported inverse transforms remain symbolic; this is not a numerical
Bromwich-integral implementation.

The exponential-shift rule also applies when the base transform is unknown.
For `@L(e^(at)f(t))`, Rendered TeX displays
$\mathcal L\{e^{at}f(t)\}(s)=F(s-a)$, with the local definition
$F(s):=\mathcal L\{f(t)\}(s)$ and the restriction
$s-a\in\operatorname{ROC}(F)$. Here ROC denotes the region of convergence;
no numerical bound is assumed for an unspecified function.
The corresponding unbound Expression output is `ℒ(f(t), t, s - a)` and the
Function return expression is `Laplace(f(t), t, s - a)`, so neither relies on an
undefined global function named `F`. These evaluated transform arguments are
accepted as input. Known base transforms are evaluated and their convergence
restrictions shifted with the formula.

TeX, Expression and Function output all display the same formula. Supplied constant
parameters are specialised consistently across these cards; free variables remain
symbolic even when they have numerical bindings. Conditions proved by the supplied
constants are removed. Expression and Function show readable, parseable conditions,
so restrictions survive copying a result back as input.
For example, `@L(t)` produces `1/s² where (Re(s) > 0)` in unbound Expression output.
The bound Expression card places conditions after the binding bar instead:
`{ 1/s² | s = ?; Re(s) > 0 }`. This form is also accepted as input.
Several conditions are separated by semicolons inside the `where` parentheses.
The Function card checks the restrictions before evaluating the formula and
returns `@nan` otherwise. For `@L(t)`, its function body is:

```text
    if (realpart(s) > 0) {
        return 1/s^2.
    } else {
        return @nan.
    }
```

With several conditions, all must hold; any formula temporaries are evaluated
only inside the successful branch. Non-finite input handling remains in the
native evaluator, rather than appearing as a displayed mathematical condition.
Unsupported transforms retain their original symbolic operation.
The [function-by-function coverage inventory](#laplace-function-coverage) distinguishes
implemented formulas, proven obstructions and well-defined cases still left symbolic.
Hyperbolic tangent, unlike tangent, has a supported digamma transform. For real
$c>0$ and $\operatorname{Re}(s)>0$,

$$
\mathcal L\{\tanh(ct)\}
=\frac1s-\frac{\psi(1+s/(4c))-\psi(1/2+s/(4c))}{2c}.
$$

The native result uses the equivalent symmetric form
$[2\psi(z+1/2)-\psi(z)-\psi(z+1)]/(4c)$, where $z=s/(4c)$.
Applying the digamma recurrence before differentiation removes cancelling terms:

$$
\frac{d}{ds}\mathcal L\{\tanh(ct)\}
=\frac{2\psi^{(1)}(z+1/2)-\psi^{(1)}(z)-\psi^{(1)}(z+1)}{16c^2}.
$$

Negative real coefficients follow by oddness, and a zero coefficient gives
zero without a condition on `s`. With an unspecified coefficient the formula
uses $q=\sqrt{c^2}$ on the principal branch and retains
$\operatorname{Re}(q)>0$; this also covers complex coefficients off the
imaginary axis without assuming $\sqrt{c^2}=c$. For example,
`{@L{tanh(c*t)} | s=2; c=1}` gives $\ln(2)-1/2$, approximately
`0.1931471805599453`. Non-zero imaginary coefficients are excluded by the
convergence guard, since the integrand then has real-axis poles.

Tangent transforms report convergence separately from the numeric Value card.
For example, `@L{tan(t)}` remains symbolic with an evaluation note explaining
that the ordinary Laplace transform does not exist because of poles on the
positive integration axis. With an unspecified coefficient, `@L{tan(c*t)}`
retains the operator and gives a conditional explanation; complex coefficients
are not classified as divergent by this test. `{@L{tan(c*t)} | c=0}` returns
`0`, even without a value for `s`.
Numerical evaluation requires a supplied target strictly inside that half-plane.
Symbolic powers retain the condition on the exponent, including in linear combinations;
numerical evaluation also checks this condition. For non-negative integer exponents,
the gamma factor equals the factorial. An explicit source, as in the power example,
avoids treating a free exponent as another transform variable.
This initial implementation requires one source variable; inferred
multidimensional transforms remain planned. Fourier operators are described below.
Targets that would capture an existing input parameter are rejected.

### Derivatives of unspecified functions

Prime notation and parenthesised derivative orders are accepted for unary
symbolic functions. In this notation, `f^(n)(t)` means the derivative of order
`n`, not the power `f(t)^n`. The derivative order must be a non-negative integer.
Small literal orders expand the initial-value terms; symbolic and larger orders
retain a finite sum. Supplied bindings do not replace the order in the symbolic cards.

Inside a forward transform, bare primes also establish a function of the source
variable: the default is `t`, or the explicitly supplied source when present.
All bare occurrences of that function name in the same operand are interpreted
consistently, even those preceding its first derivative. Scalar coefficients and
bare occurrences outside the transform retain their ordinary meanings. A bare name
without any derivative notation remains a scalar; it is not implicitly a function.

With $F(s)=\mathcal L\{f(t)\}(s)$, the rule is

$$
\mathcal L\{f^{(n)}(t)\}(s)
=s^n F(s)-\sum_{k=0}^{n-1}s^{n-1-k}f^{(k)}(0^+).
$$

This assumes the required derivatives, finite right-hand initial limits and
vanishing exponentially weighted boundary terms at infinity. No numerical
convergence half-plane is invented for an unspecified function. Initial values
written `f(0)`, `f'(0)`, and so on in the output denote those right-hand limits.
Order zero is the original transform, with no initial-value sum. Fractional
derivatives and derivatives evaluated at a nontrivial composed argument are not
covered by this rule.

Expanded initial-value terms are displayed in ascending powers of the transform
variable, after the leading transform term, including in multiline TeX output.

| Input | Unbound expression output |
| --- | --- |
| `@L{f'(t)}` | `s·ℒ(f(t), t, s) - f(0)` |
| `@L{f'}` | `s·ℒ(f(t), t, s) - f(0)` |
| `@L{f''(t)}` | `s^2·ℒ(f(t), t, s) - f'(0) - s·f(0)` |
| `@L{f^(n)(t)}` | `s^n·ℒ(f(t), t, s) - Σ_(k=0)^(n - 1) f^(k)(0)·s^(n - 1 - k) where (n ∈ ℤ≥0)` |

### Laplace transforms of integrals

Inside a transform, an unspecified call such as `f(x)` remains a function in the
integrand; it is not interpreted as multiplication by a scalar `f`.
With $\widehat f(s)=\mathcal L\{f(t)\}(s)$, the zero-based integral transforms to $\widehat f(s)/s$.
An upper-only integral denotes a chosen primitive $G(t)$ and transforms to
$(\widehat f(s)+G(0^+))/s$, retaining its initial value as an upper-only integral at zero.
For a fixed non-zero lower bound $a$, the initial value is $\int_a^0 f(x)\,dx$.
In rendered TeX, a single upper-only initial-value integral is abbreviated as
$F(0)$, with $F'(x)=f(x)$ and a label defining $F$ as the chosen antiderivative.
In that display, $F$ does not denote the Laplace transform. The explicit integral
is retained in Expression and Function output. The abbreviation is not used when
the name $F$ is already present or when multiple initial-value integrals would
make its meaning ambiguous.
These identities apply where the transforms converge and the initial value is
finite; they do not assert a convergence half-plane for an unspecified function.
Integrating an evaluated transform with respect to its target variable retains
the transform's convergence restrictions on the antiderivative.

| Input | Unbound expression output |
| --- | --- |
| `@L{@S_0^t f(x) dx}` | `ℒ(f(t), t, s)/s` |
| `@L{@S^t f(x) dx}` | `1/s·(ℒ(f(t), t, s) + ∫^0 f(x)·dx)` |

### Laplace transforms of translated arbitrary functions

An unspecified function name, including `u`, remains arbitrary: it is not an
alias for the Heaviside unit step. For a real shift $a$, write
$U(s)=\mathcal L\{u(t)\}(s)$. The unilateral translation rule is

$$
\mathcal L\{u(t-a)\}(s)
=e^{-as}\left[U(s)+\int_{-a}^{0}e^{-sx}u(x)\,dx\right].
$$

The finite integral must exist, and the identity applies where the transforms
converge. Negative $a$ gives a time advance, with the same oriented-integral
formula. Symbolic shifts retain a real-parameter condition; literal complex
shifts are left unevaluated. No numerical convergence half-plane is inferred
for an unspecified function.

Multiplication by $t^n$, for integer $1\le n\le64$, applies
$(-1)^n d^n/ds^n$ to the transform. Derivatives involving unspecified transforms
are retained in compact form. The function being transformed must not itself
depend on the transform variable for this differentiation rule to apply.

| Input | Unbound expression output |
| --- | --- |
| `@L{u(t-1/4)}` | `exp(-¼s)·(ℒ(u(t), t, s) + ∫^0_-¼ u(t)·exp(-st)·dt)` |
| `@L{16t^2u(t-1/4)}` | `16·Dss(exp(-¼s)·(ℒ(u(t), t, s) + ∫^0_-¼ u(t)·exp(-st)·dt))` |

Here `Dss` means the second derivative with respect to $s$. Rendered TeX uses a
differential operator with its operand outside the fraction, keeping it full-sized.
Only an additional causal assumption, $u(x)=0$ for $x<0$, removes the history
integral for a non-negative delay. Such an assumption is not inferred from the
function's name. Without it, the second example is not the elementary
unit-step transform.

### Laplace function coverage

The [forward transform tables](design-notes/integral-transforms.md#implemented-forward-laplace-transforms)
give the corresponding formulas and convergence conditions.

This inventory concerns the ordinary unilateral integral over real `t >= 0`.
A singularity is not automatically an obstruction: logarithmic singularities
are locally integrable, whereas a simple pole is not. Exponential damping
cannot repair a non-integrable finite singularity or super-exponential growth.
Distributional and principal-value transforms are separate operations and are
not silently substituted for the ordinary transform.

The inventory covers the mathematical constructors in `include/expression.h`.
Parser aliases share their underlying operation; they do not need separate
transform formulas. Structural operations such as differentiation, integration,
binding, serialisation and parsing are not scalar function families.

#### Supported families

Restrictions below are sufficient domains used by the implementation, not
claims that every listed domain is maximal. A scale described as *known* must
be a supplied constant, not a free variable which happens to have a value.
All three symbolic cards use the same native expression and conditions.

| Functions | Recognised forms and restrictions |
| --- | --- |
| `exp`, powers of `e` | Affine exponents with the shifted convergence half-plane; quadratic exponents `-a*t^2+b*t+d` when `Re(a)>0`, for every finite `s`. |
| `pow`, `pow_xp`, `sqrt`, `cubrt`, `root` | Source powers `t^v`, with `Re(v)>-1` and `Re(s)>0`; root spellings use the same rule. |
| `sin`, `cos`, `sinh`, `cosh` | Affine arguments; both component exponentials must converge. Small integer powers of hyperbolic functions are finite exponential sums. Integer circular powers use recurrences or finite sums. |
| `versin`, `vercos`, `coversin`, `covercos`, `haversin`, `havercos`, `hacoversin`, `hacovercos` | Reductions to sine, cosine and constants, with their convergence conditions. |
| `tanh`, `sech` | Digamma formulas for scaled arguments; branch-safe square-root scales retain sufficient conditions. |
| `log`, `ln`, `log10`, `lg` | Direct logarithms; supported positive affine arguments use exponential integrals. |
| `abs`, `conj` | Homogeneous linear arguments; absolute values additionally cover known real affine arguments, including a zero crossing. |
| `floor`, `ceil` | Homogeneous arguments with known real rates, including negative and zero rates. |
| `atan`, `acot` | Homogeneous arguments with known real rates, using exponential integrals and the native inverse-cotangent branch. |
| `asinh` | Homogeneous arguments with known real rates, using a hypergeometric representation of the Struve function and order-zero Bessel Y. |
| `atanh` | Homogeneous arguments with real rates, including symbolic rates guarded as real and non-zero; exponential integrals retain the native complex boundary value beyond the branch points. Zero rates give zero. |
| `erf`, `erfc` | Affine arguments; Gaussian decay conditions are retained for complex scales. |
| `normal_pdf`, `pdf` | Affine Gaussian density; quadratic decay gives an entire transform in `s`. |
| `normal_cdf`, `cdf` | Affine arguments via the complementary-error-function rule. |
| `normal_logpdf`, `logpdf` | Affine arguments reduce to a quadratic polynomial. |
| `Ei`, `E1` | Homogeneous arguments with supported rates; conservative right half-planes avoid removable quotient singularities at `s=0`. |
| `gammainc_lower`, `gammainc_upper`, `gammainc_P`, `gammainc_Q` | Source in the second argument, with `Re(shape)>0` and a supported positive scale. |
| `bessel_j` | Positive real scales and orders with `Re(order)>-1`; known negative integers use the exact order-reflection identity. The existing zero-order rule also supports complex scales with appropriate bounds. |
| `bessel_y` | Order zero with positive real scale; its logarithmic origin is integrable. Higher orders are not inferred from this rule. |
| `clausen2`, `clausen` | Known integer orders 1–32 and non-zero known real scales; digamma and zeta formulas derived from the periodic Fourier series, with `Re(s)>0`. |

Linearity, source-independent factors, exponential frequency shifts and finite
integer powers of `t` multiplying a supported function reuse these formulas.
Constants, including zero, retain their existing transforms. This does not
mean that arbitrary compositions of supported functions are supported.

The symbolic `asinh` formula is valid on its stated half-plane, but its Bessel-Y
factor currently lacks a complex numeric backend. Complex `s` therefore retains
the formula without a numerical Value-card result. Real positive `s` is checked
numerically. This implementation limitation is not a convergence restriction.

`log` and `lg` are common-logarithm parser aliases; `ln` is the natural-logarithm
spelling. The C constructor `expr_log` denotes the natural logarithm.

#### Ordinary transforms that fail in the direct source argument

| Functions | Obstruction |
| --- | --- |
| `tan`, `sec`, `cosec`, `cot` | Real non-zero-rate arguments encounter poles on the integration axis. |
| `cosech`, `coth` | The unshifted function has a non-integrable origin; shifted cases require separate analysis. |
| `gamma` | Super-exponential growth and a pole at zero. |
| `digamma`, `trigamma`, `polygamma` | Non-integrable origin for the direct argument and admissible fixed derivative order. Translated functions can be integrable. |
| `zeta`, `zetap` | Pole at `t=1`. Source-independent parameters or other argument choices can change this conclusion. |

The native evaluation note diagnoses the direct cases it can prove. A
rejected parameter guard returns `@nan`; this is distinct from proving that
every possible parameter choice is divergent.

#### Well-defined or parameter-dependent cases still left symbolic

The following are audited gaps, not declarations of non-existence. Many have
ordinary transforms on suitable branches, but a correct closed form can need
additional special functions, parameter conditions or continuation machinery.

| Functions | Remaining issue |
| --- | --- |
| `asin`, `acos`, `asec`, `acosec`, `atan2` | Branch boundaries, multi-argument choices and integrable endpoint singularities need explicit treatment. |
| `acosh`, `asech`, `acosech`, `acoth` | Further inverse-hyperbolic transforms require special-function/branch formulas; real-domain and complex-domain interpretations differ. |
| `arcversin`, `arcvercos`, `arccoversin`, `arccovercos`, `archaversin`, `archavercos`, `archacoversin`, `archacovercos` | Reduce to inverse circular functions, so their branch restrictions carry over. |
| `hypot` | Polynomial reductions can work; the general transform needs further radical-function rules. |
| `lgamma` | Logarithmic origin and `t*log(t)` growth allow a transform, but no general closed form is implemented. |
| `Li` | Its logarithmic singularity at `t=1` is integrable; a closed-form rule is not implemented. |
| `erfinv`, `erfcinv`, `gammainv` | The full positive axis can leave a real inverse's domain; complex branches need separate definitions and growth checks. |
| `lambert_w`, `lambert_wn`, `lambert_w0`, `lambert_wm1` | Branch-specific transforms are not currently represented in closed form. |
| `dilog`, `polylog1`, `polylog`, `legendre_chi` | Source on a branch cut needs explicit boundary-value conventions. Exponentially composed arguments are a separate potentially convergent family. |
| `qdigamma` | Depends on the base, poles and the source's position; a direct zero argument can be singular. |
| `zetah`, `zatahp` | Varying the order or the offset gives different convergence and pole conditions. |
| `lerch_phi`, `lommel_s`, `appell_f1`, `lauricella_f`, `hypergeometric_pFq` | Parameter-dependent growth, cuts and singularities prevent a universal formula for arbitrary argument placement. |
| `beta`, `logbeta`, `beta_pdf`, `logbeta_pdf`, `binomial`, `harmonic_poly` | Parameter positions matter. Polynomial specialisations may reduce to existing rules. The current beta-density constructor is an analytic expression, not an automatically compactly supported density. |
| `chebyshev_t`, `chebyshev_u`, `hermite_h` | Fixed non-negative integral degrees give polynomials with ordinary Laplace transforms. Direct named-polynomial Laplace rules are not yet implemented; their Fourier pairs are documented separately. |

The `causal_convolve` constructor implements the unilateral convolution theorem:
its transform is the product of the operands' transforms, combining known
convergence half-planes. The whole-line `convolve` constructor is distinct and
is not automatically treated as causal. See [Convolutions](#convolutions) for
the Fourier rules and the precise integration bounds.

#### Discrete operations

`factorial`, `fibonacci`, `partition`, `isqrt`, `gcd`, `lcm`, `mod`, `modinv`, `is_prime`,
`next_prime`, `prev_prime`, `bit_and`, `bit_or`, `bit_xor`, `bit_not`, `shl`,
`shr` and `factors` do not acquire a continuous-time definition merely by
putting `t` in their argument. A specified step interpolation, analytic
continuation or impulse train would define a different transform problem.
Finite `new_finite_summation_range` and `new_finite_product_range` operations
can reduce algebraically, but no general infinite-transform interchange is
assumed.

#### Checked examples

These inputs have the following numerical Value-card outputs (rounded here).
They are exercised after the ordinary regression tests as README examples.

| Input | Output |
| --- | --- |
| `{@L(exp(-t^2)) \| s=0}` | `0.886226925452758` |
| `{@L(normal_pdf(t)) \| s=0}` | `0.5` |
| `{@L(floor(t)) \| s=1}` | `0.581976706869326` |
| `{@L(sech(t)) \| s=1}` | `0.693147180559945` |
| `{@L(E1(t)) \| s=1}` | `0.693147180559945` |
| `{@L(bessel_j(1,t)) \| s=1}` | `0.292893218813452` |
| `{@L(atanh(t)) \| s=1}` | `0.646761122779130 + 0.577863674895461i` |

#### Formula references

Definitions and transform conventions follow [DLMF §1.14](https://dlmf.nist.gov/1.14).
Exponential-integral formulas can be checked against
[DLMF §6.14](https://dlmf.nist.gov/6.14); Bessel integral identities are collected
in [DLMF §10.22](https://dlmf.nist.gov/10.22). Incomplete-gamma formulas follow
by interchanging their defining integral with the Laplace integral on the
stated absolute-convergence domain. Gaussian formulas follow by completing
the square and analytic continuation within the stated decay domain.

## Fourier transforms

Expression mode supports native one-dimensional `@F` and `@Finv` operators,
also named `ℱ`/`Fourier` and `ℱ⁻¹`/`InverseFourier`. Parentheses and braces are
accepted. The non-unitary angular-frequency convention is

$$
\mathcal F\{f\}(\omega)=\int_{-\infty}^{\infty}f(t)e^{-i\omega t}\,dt,
\qquad
\mathcal F^{-1}\{G\}(t)=\frac{1}{2\pi}\int_{-\infty}^{\infty}G(\omega)e^{i\omega t}\,d\omega.
$$

Both coordinates are real. Default coordinate pairs are `t`/`ω`, `x`/`k`,
`y`/`m` and `z`/`n`; inverse transforms reverse the pair. The three-argument
form takes the operand, bound source variable and free target expression.
Unfamiliar coordinate names require an explicit target. Source coordinates
are not exposed as Lab bindings, and target names must not capture parameters
already present in the operand.

The current rules cover Gaussian, rectangular, triangular, normalised sinc,
two-sided exponential, quadratic-reciprocal and hyperbolic-secant pairs, plus
one-sided exponentials with explicit step factors. Distributional rules cover
constants, impulses, harmonics, the unit step with its principal value,
derivatives and polynomial weighting. Linearity, real affine changes of a
unary function, exponential modulation, conjugation and duality also apply
to supported or unspecified functions. Known non-negative integral time powers
up to 32 generate frequency derivatives. Bare monomials also support symbolic
orders: $\mathcal F\{t^n\}=2\pi i^n\delta^{(n)}(\omega)$ for
$n\in\mathbb Z_{\ge0}$. Symbolic derivative orders retain their
non-negative-integer condition. `Derivative` (also `ordered_derivative`)
preserves an unspecified unary function or Dirac delta and its derivative order
separately; the derivative is taken with respect to that function's argument.
Other built-in functions continue to use ordinary derivative notation. Delta derivatives are
rendered as $\delta^{(n)}$ in TeX and remain distributions, not numerical values.
These identities assume the relevant
ordinary transform or tempered-distribution interpretation exists; they are
not convergence proofs for arbitrary functions.

Unsupported transforms remain symbolic with a native evaluation note.
This is **partial coverage**, not the entire reference table: multidimensional
transforms, circular-aperture disk transforms, chirps, impulse trains,
non-integral-order or second-kind Bessel transforms, general regularised powers
and complex-branch logarithmic families remain unsupported. See the
[coverage inventory](design-notes/integral-transforms.md#fourier-acceptance-criteria).

### Logarithmic distributions

The absolute-value logarithm is supported in both Fourier directions, including
real non-zero affine scales and real translations. The shorthand `ln|x|` has the
same meaning as `ln(abs(x))`; absolute-value arguments without parentheses are
also accepted for other registered unary functions in expression syntax.

| Input | Output (real target coordinate) |
| --- | --- |
| `@F{ln\|x\|}` | $-\pi\bigl(\operatorname{Fp}(1/\lvert k\rvert)+2\gamma\delta(k)\bigr)$ |
| `@F{ln(\|x\|)}` | $-\pi\bigl(\operatorname{Fp}(1/\lvert k\rvert)+2\gamma\delta(k)\bigr)$ |
| `@F{ln(abs(x))}` | $-\pi\bigl(\operatorname{Fp}(1/\lvert k\rvert)+2\gamma\delta(k)\bigr)$ |
| `@Finv{-@pi*finite_part(1/abs(ω))-2*@pi*@eulermascheroni*delta(ω)}` | $\ln\lvert t\rvert$ |

`finite_part`, with alias `Fp` and public constructor `expr_finite_part`, denotes
a Hadamard finite-part distribution. For the reciprocal absolute value, MARS
uses a **unit cutoff**: for a Schwartz test function $\varphi$,

$$
\left\langle\operatorname{Fp}\!\left(\frac1{|k|}\right),\varphi\right\rangle
=\int_{|k|<1}\frac{\varphi(k)-\varphi(0)}{|k|}\,dk
+\int_{|k|\ge1}\frac{\varphi(k)}{|k|}\,dk.
$$

This fixes the delta coefficient; changing the cutoff changes that coefficient.
The reciprocal-absolute-value pair is implemented in both directions. Other
finite-part operands are retained formally, without claiming transform support.
Derivatives and unresolved integrals remain formal, and finite sums retain the
distribution terms. No pointwise numerical value is assigned, even away from
the singularity; scalar and matrix numerical APIs are therefore not provided.
The Expression, Function and TeX cards retain the same distributional result.
An unqualified complex `ln(x)` is different and remains unsupported here.

### Hyperbolic powers and convergence

For real $a\ne0$ and $b$, let $q=\omega/a$ and $A_\pm=(-n\pm iq)/2$.
The principal-power convention gives

$$
\mathcal F\{\sinh^n(at+b)\}(\omega)
=\frac{2^{-(n+1)}e^{ibq}}{|a|}
\left[\mathrm B(A_+,n+1)+e^{i\pi n}\mathrm B(A_-,n+1)\right],
\qquad -1<\Re(n)<0.
$$

The factor $e^{i\pi n}$ is the negative-half-line branch value; it must not be
discarded for fractional or complex powers. Replacing the input by
$|\sinh(at+b)|^n$ replaces this factor by one. For $\cosh^n(at+b)$, the bracket
becomes $\mathrm B(A_+,A_-)$ and the convergence condition is $\Re(n)<0$.
These formulas follow from [Euler's beta integral](https://dlmf.nist.gov/5.12.E1);
the cosh pair also follows from [DLMF 5.12.7](https://dlmf.nist.gov/5.12.E7).
Inverse transforms reverse the sign of $q$ and include $1/(2\pi)$, without
conjugating the principal-power branch factor.

The reverse beta-spectrum pairs are recognised as well: copying either complete
spectrum above into `@Finv{...}` recovers its hyperbolic power, including the
real scale, translation and principal branch. This works with serialised algebra,
not just nested transform operators. The convergence conditions are retained;
changing a beta argument or relative coefficient does not trigger the pair.

Reciprocal forms and powers of `sech` and `cosech` are recognised too.
For $\operatorname{cosech}^p(u)$ the effective sinh exponent is $n=-p$, but
the branch factor is $e^{i\pi p}$, not $e^{-i\pi p}$.
Supplied constant parameters are substituted before selecting a formula:
zero exponent gives $2\pi\delta(\omega)$, and zero scale gives the transform
of a constant whenever the constant itself is defined.

Positive real part of the effective sinh/cosh exponent causes exponential
growth, so neither an ordinary nor a tempered-distribution Fourier transform
exists for real non-zero scale. For sinh powers, $\Re(n)\le-1$ instead gives a
non-integrable singularity: any principal-value or finite-part prescription
must be chosen explicitly, not inferred by continuing the beta formula.
Unimplemented boundary cases with $\Re(n)=0$ remain symbolic. Unknown parameters
retain the convergence conditions rather than being declared divergent.

| Input | Output |
| --- | --- |
| `{@F{sinh(t)^(-1/2)} \| ω=0}` | Approximately $3.708149354602744-3.708149354602744i$ |
| `@F{sinh(t)^2}` | Symbolic transform with an exponential-growth diagnostic: no ordinary or tempered-distribution Fourier transform. |
| `@F{sinh(t)^(-1)}` | Symbolic transform with a non-integrable-singularity diagnostic; a regularisation prescription is required. |
| `@Finv{(B(1/4+i*ω/2,1/2)-i*B(1/4-i*ω/2,1/2))/sqrt(2)}` | $\sinh(t)^{-1/2}$ on the principal branch, for real $t\ne0$. |

### Indexed Bessel transforms

`J_n(x)` denotes `BesselJ(n,x)`, and `Y_n(x)` denotes `BesselY(n,x)`.
Numeric ASCII and Unicode subscripts and braced order expressions are accepted.
The Fourier rule currently covers **integer-order first-kind** Bessel functions,
including negative orders and real non-zero affine scales and translations.
The indexed notation does not imply Fourier support for second-kind Bessel functions.

For $n\in\mathbb Z$, the angular-frequency transform is

$$
\mathcal F_{x\to k}\{J_n(x)\}=
\begin{cases}
2(-i)^n T_{|n|}(k)/\sqrt{1-k^2},&|k|<1,\\
0,&|k|>1.
\end{cases}
$$

The inverse transform of $J_n(\omega)$ has the same compact support in $t$,
with coefficient $i^n/\pi$ instead of $2(-i)^n$. These rules follow from
[Bessel's integral representation](https://dlmf.nist.gov/10.9.E2).
The independently entered compact spectrum also has a Bessel transform;
its inverse is recognised without requiring a nested transform expression.
Native output represents the support with `rect` and excludes the singular
edges: no finite pointwise value is assigned at $k=\pm1$. Symbolic orders retain
the equivalent conditions $n\in\mathbb R$ and $|n|\in\mathbb Z_{\ge0}$.
The default pair is `x` to `k`; supply the third transform argument to use `ω` instead.

README examples (Value-card output):

| Input | Output |
| --- | --- |
| `{@F{J_n(x)} \| k=0; n=0}` | `2` |
| `{@F{J_n(x)} \| k=2; n=3}` | `0` |
| `{@F(J_n(x),x,ω) \| ω=0; n=0}` | `2` |

### Chebyshev and Hermite polynomials

The names distinguish the existing harmonic polynomial from Hermite:

| Input name | Mathematical notation | Convention | Other spellings |
| :--- | :---: | :--- | :--- |
| `Tn(n,x)` | $T_n(x)$ | Chebyshev, first kind; $T_0=1$, $T_1=x$ | `chebyshev_t`, `ChebyshevT`, indexed `T_n(x)` |
| `Un(n,x)` | $U_n(x)$ | Chebyshev, second kind; $U_0=1$, $U_1=2x$ | `chebyshev_u`, `ChebyshevU`, indexed `U_n(x)` |
| `ℋ(n,x)` | $\mathcal H_n(x)$ | Physicists' Hermite; $\mathcal H_0=1$, $\mathcal H_1=2x$ | `hermite_h`, `HermiteH`, indexed `ℋ_n(x)` |
| `Hn(n,x)` | $H_n(x)$ | Existing harmonic polynomial; unchanged | `harmonic_poly`, `harmonicpoly` |

The three orthogonal-polynomial families use non-negative integral degrees.
Real and complex arguments are evaluated by polynomial recurrence, without
inverse-trigonometric branch choices. The number backend retains its working
precision and exact arithmetic; matrix APIs evaluate matrix polynomials, not
entrywise values. C constructors use the names `expr_chebyshev_t`,
`expr_chebyshev_u` and `expr_hermite_h`.

Differentiation uses $T_n'=nU_{n-1}$ and
$\mathcal H_n'=2n\mathcal H_{n-1}$. Second-kind derivatives use polynomial
expansion or a finite sum, retaining regular values at both endpoints.
Hermite and second-kind Chebyshev primitives support symbolic degree and
affine arguments. First-kind primitives currently expand literal degrees
through 64; larger or symbolic degrees may remain formal. Finite sums support
all three families through the ordinary bounded-sum evaluator.
The recurrences and calculus conventions follow
[DLMF §18.9](https://dlmf.nist.gov/18.9).

README examples (Value-card output):

| Input | Output |
| :--- | ---: |
| `Tn(3,2)` | `26` |
| `Un(3,2)` | `56` |
| `ℋ(3,2)` | `40` |
| `sum(k,0,3,Tn(k,2))` | `36` |
| `sum(k,0,3,ℋ(k,2))` | `59` |

The Hermite–Gaussian Fourier rule and its inverse apply to symbolic
non-negative integral degree and real non-zero affine scales. Hermite uses
script $\mathcal H$ in MARS to distinguish it from the harmonic $H$;
it is the physicists' $H_n$ in the reference transform table.

### Convolutions

`convolve(f(t),g(t),t)` denotes whole-line convolution:

$$
(f*g)(t)=\int_{-\infty}^{\infty}f(\tau)g(t-\tau)\,d\tau.
$$

`causal_convolve(f(t),g(t),t)` denotes the zero-based convolution used by
unilateral Laplace transforms:

$$
(f*_+g)(t)=\int_0^t f(\tau)g(t-\tau)\,d\tau.
$$

The aliases are `convolution` and `causal_convolution`; C constructors are
`expr_convolve` and `expr_causal_convolve`. The third argument is an explicit
output coordinate. A separate integration variable is introduced without
capturing parameters, even when the input already contains a parameter named
$\tau$. Ordinary multiplication is never reinterpreted as convolution.

| Operation | Transform identity | Interpretation |
| :--- | :--- | :--- |
| Whole-line convolution | $\mathcal F(f*g)=\mathcal F(f)\mathcal F(g)$ | Ordinary convergence or a valid distributional convolution is required. |
| Ordinary product | $\mathcal F(fg)=(\mathcal F(f)*\mathcal F(g))/(2\pi)$ | Used as a symbolic fallback after more specific rules. |
| Inverse Fourier convolution | $\mathcal F^{-1}(F*G)=2\pi\mathcal F^{-1}(F)\mathcal F^{-1}(G)$ | Same angular-frequency normalisation. |
| Inverse Fourier product | $\mathcal F^{-1}(FG)=\mathcal F^{-1}(F)*\mathcal F^{-1}(G)$ | No extra factor. |
| Causal convolution | $\mathcal L(f*_+g)=\mathcal L(f)\mathcal L(g)$ | Known half-plane conditions are combined. |
| Inverse Laplace product | $\mathcal L^{-1}(FG)=\mathcal L^{-1}(F)*_+\mathcal L^{-1}(G)$ | Used after direct rational rules. |

Arbitrary operands retain formal transforms; these identities do not assert
convergence for arbitrary functions or define products of arbitrary distributions.
Recognised convolution values include rectangular pulses, impulses with
numerical real affine arguments, centred Gaussians, and causal operands whose
finite integral the native integrator can evaluate. Unsupported convolutions
remain symbolic, without inventing numerical values.

README examples (Value-card output):

| Input | Output |
| :--- | ---: |
| `{convolve(rect(t),rect(t),t) \| t=1/4}` | `0.75` |
| `{causal_convolve(t,t,t) \| t=2}` | `1.333333333333…` |
| `{@L{causal_convolve(t,t,t)} \| s=2}` | `0.0625` |

### Signal functions and distributions

The new expression functions are `step`, `rect`, `tri`, `circ`, `sinc`,
`delta` and `principal_value` (`PV`). The ordinary functions also have
`qf_`, `qc_`, `num_` and `mat_` APIs. Their C expression constructors are
`expr_step`, `expr_rect`, `expr_tri`, `expr_circ`, `expr_sinc`, `expr_delta` and
`expr_principal_value`; each retains its argument and returns an owning node.
Their definitions are:

| Function | Definition and endpoint convention |
| --- | --- |
| `step(x)` | Zero for negative real arguments, one for positive arguments and one half at zero. Aliases: `heaviside`, `Heaviside`, `θ`. |
| `rect(x)` | One for $\lvert x\rvert<1/2$, zero outside, one half at either endpoint. Alias: `Π`. |
| `tri(x)` | $\max(1-\lvert x\rvert,0)$ for real arguments. Alias: `Λ`. |
| `circ(x)` | The even, unit-radius profile: one for $\lvert x\rvert<1$, zero outside, one half at either endpoint. Applied to a single real coordinate this is an interval, **not a two-dimensional disk transform**. |
| `sinc(x)` | $\sin(\pi x)/(\pi x)$ with the removable value one at zero; entire for complex arguments. |
| `delta(x)` | The Dirac distribution, with no invented finite pointwise value. Aliases: `δ`, `@delta`, `DiracDelta`. Expression output uses `δ(x)`; Function output uses `delta(x)`. |
| `principal_value(x)` | An explicit principal-value interpretation, not a pointwise numerical regularisation. Alias: `PV`. |

The real piecewise functions reject non-real scalar arguments. Distributional
functions intentionally have no ordinary numeric or matrix API. In particular,
`u(t)` remains an arbitrary function and is never silently interpreted as a step.
Differentiation, affine-argument primitives and finite numerical sums are
available for the ordinary signal functions; step/pulse derivatives retain
their boundary impulses. Sinc calculus uses entire representations that remain
regular at zero. General principal-value integration remains symbolic.

### Checked Fourier examples

These are exact native unbound outputs, exercised as README examples after the
ordinary tests. Conditions are retained in Expression and Function renderings;
TeX uses the corresponding mathematical notation.

| Input | Output |
| --- | --- |
| `@F{exp(-t^2)}` | `√(π)·exp(-¼ω²) where (ω ∈ ℝ)` |
| `@Finv{exp(-ω^2)}` | `½·exp(-¼t²)/√(π) where (t ∈ ℝ)` |
| `@F{rect(t)}` | `sinc(ω/(2π)) where (ω ∈ ℝ)` |
| `@F{delta(t)}` | `1` |
| `@F{step(t)}` | `principal_value(1/(iω)) + π·δ(ω) where (ω ∈ ℝ)` |
| `@F{1+t^n+delta(t)}` | `2π·(δ(ω) + i^n·Derivative(δ(ω), n)) + 1 where (ω ∈ ℝ; n ∈ ℤ≥0)` |
| `@F{f(t)}` | `ℱ(f(t))` |

## Numeric representation

The internal numeric core is now `number_t`-native:

- constant and variable leaves store `number_t`
- cached primal values are `number_t`
- expression evaluation is `number_t`-native
- `expr_eval_derivatives(...)` returns owning `number_t` values

That internal migration does not change the public ownership rules:

- `expr_*` builders still return owning `expr_t *` handles that must be released
  with `expr_free()`
- `expr_get_val(...)`, `expr_eval(...)`, and `expr_eval_derivatives(...)` still
  return owning `number_t` values that must be released with `num_destroy()`
- `expr_get_deriv(...)` still returns a borrowed cached derivative node

Expressions are built from constants, variables, and operator nodes; each node
carries a vtable that knows how to evaluate itself and construct its derivative.

## Threading

`expr_t` is currently a single-threaded type. The expression DAG, cached primal
values, derivative cache, and variable updates are not synchronised internally.
If you need to use an `expr_t` graph from multiple threads, protect it with
external locking and treat concurrent evaluation or mutation as unsupported for
now. That caveat also applies to the symbolic integrator fast path built on top
of these graphs.

## Capabilities

- expression construction from constants, variables, and operators
- native recognition of additive ellipsis series, with exact polynomial
  interpolation, geometric and inverse-index-power models
- lazy evaluation with result caching
- symbolic differentiation to arbitrary order
- Cartesian presentation of supported elementary functions with symbolic
  complex arguments
- symbolic antiderivatives for conservative, reliable expression families,
  including unevaluated integral fallback nodes for partially supported sums
- evaluation of derivatives for scalar outputs
- elementary and special functions exposed through the `expr` builder API and
  evaluated through `number_t`
- value-only integer helpers such as `factorial`, `partition`, primality,
  factorisation, and bit operations
- expression parsing from and formatting to strings
- integration as a symbolic matrix element type through `matrix_t`
- shared structural helper layers for sibling modules, declared through
  `src/internal/expr_internal.h`

## Matrix Integration

`expr_t *` can be used as a `matrix_t` element type via `MAT_TYPE_EXPR`. This
lets you build symbolic matrices whose entries remain differentiable expression
DAGs.

Current matrix integration is strongest for:

- symbolic matrix construction and pretty-printing
- entrywise symbolic algebra through matrix add, subtract, and multiply
- ordered higher and mixed matrix derivatives and iterated antiderivatives
- exact spectral differentiation and integration of constant square-matrix
  powers, with a numeric diagonalisation fallback for supported larger matrices
- exact structured matrix functions
- exact symbolic inverse and solve families
- style-independent expression simplification followed by matrix-wide
  presentation beautification

After a matrix operation, each output entry is still an `expr_t` expression, so
you can differentiate individual matrix entries with the normal `expr` API.

The full numerical matrix toolbox is still separate. General factorisations
and Schur-based matrix functions remain numeric-only unless the `expr` input
first falls into a supported exact structured case.

## Symbolic Integrator Helpers

The `expr` subsystem also exposes a small public helper surface for
higher-level symbolic code:

- `src/internal/expr_internal.h` contains lightweight DAG utilities such as
  exact-zero checks, named-constant checks, and substitution.
- `src/internal/expr_internal.h` also contains the lower-level structural
  recognisers such as constant/variable detection, scaled-expression matching,
  product and sum decomposition, and variable-usage collection.
- `src/internal/expr_internal.h` contains the heavier affine-family
  recognisers used by the integrator, including:
  - unary-affine matching like `exp(a)` and `sin(a)`
  - degree-4 affine polynomial matching `P(a)`
  - degree-4 affine-polynomial times unary-affine matching `P(a) * special(a)`
  - rational affine-factor matching used by the partial-fraction layer

These APIs are intended for sibling library modules rather than ordinary
end-user arithmetic code.

## Symbolic Integration

`expr_integrate(expr, wrt)` tries to build an owning antiderivative expression
with respect to the explicit variable node `wrt`. It returns NULL when no safe
symbolic rule is known for a fully non-additive expression, which lets callers
fall back to the numerical `integrator_t` path without guessing.

The current rule set is intentionally conservative. It covers constants,
sums/differences, constant multiples, powers of the integration variable,
`1/x`, `ln(x)`, affine elementary and inverse-family primitives, selected
u-substitutions, integration-by-parts families such as `x * sin(x)`,
`x * exp(x)`, `x * ln(x)`, and `x * atan(x)`, plus a focused partial-fraction
layer for rational functions whose denominator factors into supported affine
linear terms. Affine `exp`, `sin`, `cos`, `tan`, `sinh`, `cosh`, and `tanh`
terms such as `sin(3*x - 1)` are part of that fast path. The special-function
rules also cover supported power-composed Bessel J, Bessel Y and lower-case
Lommel functions, together with monomial compositions of the generalised
hypergeometric pFq family. These are structural rules rather than matches for
one memorised input. Primitive dispatch preserves a symbolic affine complex
argument long enough to integrate it before presentation expansion; in
particular, both the `x` and `y` antiderivatives of `tanh(x + iy)` are returned
as native Cartesian expressions rather than unevaluated integral nodes.

Quadratic-logarithm and affine-over-quadratic integrals share a discriminant
rule. For `a*x*x+b*x+c`, a known negative `4*a*c-b*b` selects an inverse
hyperbolic tangent; a zero discriminant uses a rational primitive. This avoids
introducing complex inverse tangents for real primitives between the roots.
Logarithms of negated differences are normalised algebraically without
splitting logarithms or changing their branches. These rules also supply the
elementary [inverse-hyperbolic-tangent PDE solution](diffequation.md).

Real affine absolute values and their linear multiples are integrated directly.
For a real affine argument $u=ax+b$, with $a\ne0$, the primitives are
$u|u|/(2a)$ for $|u|$ and $u^2|u|/(3a)$ for $u|u|$.
An arbitrary affine multiplier is first expressed as a constant plus a multiple
of $u$. Negative slopes and crossings of $u=0$ are supported; these are real
calculus rules, not holomorphic identities for complex moduli. The same rules
produce the [absolute-value-forced PDE example](diffequation.md).

Repeated integration of affine inverse sine and inverse cosine remains in
elementary form. Radical arguments are normalised without changing their
branches. Cancellation of opposite additive factors moves a sign only through
an integer power, retaining the base of any fractional power; this also allows
the symbolic verifier to recognise the resulting antiderivatives. See the
[inverse-cosine-forced PDE example](diffequation.md).

Riemann and Hurwitz zeta nodes participate in the same symbolic calculus.
Differentiating `zeta(s)` produces `ζ'(s)` in Expression style and `zetap(s)`
in Function style. For Hurwitz zeta,
`∂ζ(s,a)/∂s = ζ'(s,a)` (`zatahp(s,a)` in Function style) and
`∂ζ(s,a)/∂a = -s·ζ(s+1,a)`, with the chain
rule applied when either argument is itself an expression. Integrating
`zetap(s)` with respect to `s` returns `zeta(s)`, while integrating
`zetah(s,a)` with respect to `a` returns `zetah(s-1,a)/(1-s)` wherever that
identity is valid. Unsupported directions remain explicit integral nodes.

For additive expressions, supported terms may still be integrated even when
other terms are not. In that case the unsupported additive pieces are left as
unevaluated `expr_integral(...)` nodes inside the returned antiderivative. An
unevaluated integral node represents `∫^x f(t)·dt`: it is symbolically
differentiable with respect to its upper variable by the fundamental theorem
of calculus, but direct numeric evaluation of the node itself returns `NaN`.

The implementation is split into logical integration modules:

- `expr_integrate.c` owns the public orchestration and dispatch.
- `expr_integrate_arithmetic.c` handles additive decomposition, constant
  factors, and shared arithmetic flow.
- `expr_integrate_primitives.c`, `expr_integrate_power.c`,
  `expr_integrate_logarithmic.c`, and `expr_integrate_inverse.c` hold local
  primitive antiderivatives.
- `expr_integrate_trigonometric.c`, `expr_integrate_hyperbolic.c`,
  `expr_integrate_exponential.c`, and `expr_integrate_special.c` cover
  function-family rules.
- `expr_integrate_affine.c`, `expr_integrate_substitution.c`,
  `expr_integrate_by_parts.c`, and the quadratic/general-quadratic helpers
  handle structured transformations.
- `expr_integrate_partialfrac.c` holds rational factoring and partial
  fractions.
- `expr_integrate_support.c` and `expr_integrate_owned.c` hold shared helper
  logic.

Rules are dispatched through tables where that keeps the code readable; more
specialised pattern code remains local to the module that owns the rule family.

```c
number_t x0 = num_create_from_double(0.0);
number_t two = num_create_from_long(2);
expr_t *x = expr_new_named_var(x0, "x");
expr_t *two_x = expr_mul_num(x, &two);
expr_t *f = expr_exp(two_x);
expr_t *F = expr_integrate(f, x);  /* exp(2*x) / 2, or NULL if unsupported */

expr_free(F);
expr_free(f);
expr_free(two_x);
expr_free(x);
num_destroy(&two);
num_destroy(&x0);
```

For production code, verify a returned antiderivative the same way the tests
do: differentiate it with `expr_create_deriv(F, wrt)` and compare it with the
original expression at representative points.

The integration tests include round-trip checks of the form
`expr -> antiderivative -> derivative -> expr` at representative numeric
points. Unsupported expressions deliberately return `NULL` rather than an
unsafe symbolic guess, unless they appear as additive subterms inside a larger
expression that can safely return a partial antiderivative with unevaluated
integral nodes.

For example, the parser input and the corresponding symbolic output remain
together here:

```text
Input:  pFq(0, 0, x)
Output: x*pFq(1, 1, 1, 2, x)
```

Differentiating the output reconstructs `pFq(0, 0, x)`. The same round trip is
tested for non-linear monomial arguments, Bessel J, Bessel Y and Lommel s.

## Example: Constructing an Expression

```c
#include <stdio.h>
#include "expression.h"
#include "number.h"

/* f(x) = exp(sin(x)) + 3*x^2 - 7 */
static expr_t *make_f(expr_t *x) {
    number_t two = num_create_from_long(2);
    number_t three = num_create_from_long(3);
    number_t seven = num_create_from_long(7);
    expr_t *sinx   = expr_sin(x);
    expr_t *exp_sx = expr_exp(sinx);
    expr_t *x2     = expr_pow(x, &two);
    expr_t *term2  = expr_mul_num(x2, &three);
    expr_t *f0     = expr_add(exp_sx, term2);
    expr_t *f      = expr_sub_num(f0, &seven);

    expr_free(sinx);
    expr_free(exp_sx);
    expr_free(x2);
    expr_free(term2);
    expr_free(f0);
    num_destroy(&two);
    num_destroy(&seven);
    num_destroy(&three);

    return f;
}

int main(void) {
    number_t x0 = num_create_from_string("1.25");
    expr_t *x;
    expr_t *f;
    expr_t *df_dx;
    const expr_t *d2f_dx;
    number_t f_val;
    number_t d1_val;
    number_t d2_val;

    num_set_default_prec_bits(384);
    x = expr_new_named_var(x0, "x");
    num_destroy(&x0);
    f = make_f(x);
    df_dx = expr_create_deriv(f, x);
    d2f_dx = expr_get_deriv(df_dx, x);

    printf("f(x)    = "); expr_print(f);
    printf("f'(x)   = "); expr_print(df_dx);
    printf("f''(x)  = "); expr_print(d2f_dx);

    f_val = expr_eval(f);
    d1_val = expr_eval(df_dx);
    d2_val = expr_eval(d2f_dx);

    printf("\nAt x = 1.25 (384 bits, %zu significant digits):\n",
           num_get_prec_digits(f_val));
    num_printf("f(x)     = %.114N\n", f_val);
    num_printf("f'(x)    = %.114N\n", d1_val);
    num_printf("f''(x)   = %.114N\n", d2_val);

    num_destroy(&d2_val);
    num_destroy(&d1_val);
    num_destroy(&f_val);
    expr_free(df_dx);
    expr_free(f);
    expr_free(x);

    return 0;
}
```

```text
Example: Constructing an Expression
f(x)    = { exp(sin(x)) + 3x² - 7 | x = 1.25 }
f'(x)   = { 6x + cos(x)·exp(sin(x)) | x = 1.25 }
f''(x)  = { exp(sin(x))·(cos²(x) - sin(x)) + 6 | x = 1.25 }

At x = 1.25 (384 bits, 115 significant digits):
f(x)     = 2.705855122552273437029639300167354701622137229515609890757472472673785676415953638138922546147659851426132733903704E-01
f'(x)    = 8.314504625993310996029399615209018784051045276485022598390329993996280767846549723245286494696735200429525881424219E+00
f''(x)   = 3.805523101239629225822177640424432554942960462475668946332693568943904891124835742842098701664525997316324105458890E+00
```

## Example: Parsing from a String

`expr_from_string` parses an expression and its variable bindings from a single
string. It preserves the expression shape while canonicalising notation, so
`pi` is displayed as `π` and `e` remains symbolic rather than forcing
`e^x` into `exp(x)`. Use `expr_simplify()` explicitly when you want the
simplified equivalent. For differentiation, hold an explicit variable pointer
so you can pass it to the derivative API.

The outer braces and binding footer are optional. A bare expression is
shorthand for a wrapped expression with bindings inferred by MARS:

```text
x + y
```

is equivalent to:

```text
{ x + y | x = ?, y = ? }
```

The parser stores unassigned values as `NaN`; `?` is the input spelling for the
same unassigned state. The caller does not need to add braces or discover
bindings before calling `expr_from_string(...)`.

Variable and constant binding values accept Unicode nonnegative integer
superscripts as well as caret powers, so displayed powers can be copied back
into a binding. Power chains retain the parser's left-associative convention;
superscripts outside the supported integer range are rejected.

An unset named constant remains a symbolic quantity: copies with the same name
and binding combine during simplification. Different named constants remain
independent, and an unnamed numerical `NaN` is not treated as such a symbol.

```c
#include <stdio.h>
#include <stdlib.h>
#include "expression.h"
#include "number.h"

int main(void) {
    expr_bindings_t *bindings = NULL;
    expr_t *f = expr_from_string(
        "{ exp(sin(x)) + 3*x^2 - 7 | x = 1.25 }",
        &bindings
    );
    expr_t *x;
    expr_t *df_dx;
    const expr_t *d2f_dx;
    number_t f_val;
    number_t d1_val;
    number_t d2_val;

    if (!f)
        return 1;

    x = expr_bindings_get(bindings, "x");
    if (!x) {
        expr_free(f);
        expr_bindings_free(bindings);
        return 1;
    }

    num_set_default_prec_bits(384);

    df_dx = expr_create_deriv(f, x);
    d2f_dx = expr_get_deriv(df_dx, x);

    printf("f(x)    = "); expr_print(f);
    printf("f'(x)   = "); expr_print(df_dx);
    printf("f''(x)  = "); expr_print(d2f_dx);

    f_val = expr_eval(f);
    d1_val = expr_eval(df_dx);
    d2_val = expr_eval(d2f_dx);

    printf("\nAt x = 1.25 (384 bits, %zu significant digits):\n",
           num_get_prec_digits(f_val));
    num_printf("f(x)     = %.114N\n", f_val);
    num_printf("f'(x)    = %.114N\n", d1_val);
    num_printf("f''(x)   = %.114N\n", d2_val);

    num_destroy(&d2_val);
    num_destroy(&d1_val);
    num_destroy(&f_val);
    expr_free(df_dx);
    expr_free(f);
    expr_bindings_free(bindings);

    return 0;
}
```

```text
Example: Parsing from a String
f(x)    = { exp(sin(x)) + 3x² - 7 | x = 1.25 }
f'(x)   = { 6x + cos(x)·exp(sin(x)) | x = 1.25 }
f''(x)  = { exp(sin(x))·(cos²(x) - sin(x)) + 6 | x = 1.25 }

At x = 1.25 (384 bits, 115 significant digits):
f(x)     = 2.705855122552273437029639300167354701622137229515609890757472472673785676415953638138922546147659851426132733903704E-01
f'(x)    = 8.314504625993310996029399615209018784051045276485022598390329993996280767846549723245286494696735200429525881424219E+00
f''(x)   = 3.805523101239629225822177640424432554942960462475668946332693568943904891124835742842098701664525997316324105458890E+00
```

## Example: Derivatives

The derivative API always requires an explicit `wrt` variable so the library
knows which variable to differentiate with respect to. Pass the same node
pointer that was used to build the expression.

If `wrt` is a named constant leaf instead, the derivative is treated as
undefined and the derivative routines return a symbolic `NaN` node.

```c
#include <stdio.h>
#include "expression.h"
#include "number.h"

/* f(x, y) = x² + x·y + y² */
int main(void) {
    number_t two = num_create_from_long(2);
    number_t x0 = num_create_from_string("1");
    number_t y0 = num_create_from_string("2");
    expr_t *x  = expr_new_named_var(x0, "x");
    expr_t *y  = expr_new_named_var(y0, "y");

    expr_t *x2 = expr_pow(x, &two);
    expr_t *xy = expr_mul(x, y);
    expr_t *y2 = expr_pow(y, &two);
    expr_t *t0 = expr_add(x2, xy);
    expr_t *f  = expr_add(t0, y2);

    /* First derivatives (owning) */
    expr_t *df_dx = expr_create_deriv(f, x);   /* 2x + y */
    expr_t *df_dy = expr_create_deriv(f, y);   /* x + 2y */

    /* Mixed second derivative ∂²f/∂x∂y (owning) */
    expr_t *d2f_dxdy = expr_create_2nd_deriv(f, x, y);   /* 1 */

    num_set_default_prec_bits(384);
    num_destroy(&two);
    num_destroy(&y0);
    num_destroy(&x0);
    printf("At x=1, y=2 (384 bits):\n");
    num_printf("f          = %.101N\n", expr_eval(f));          /* 7 */
    num_printf("∂f/∂x      = %.101N\n", expr_eval(df_dx));     /* 4 */
    num_printf("∂f/∂y      = %.101N\n", expr_eval(df_dy));     /* 5 */
    num_printf("∂²f/∂x∂y   = %.101N\n", expr_eval(d2f_dxdy)); /* 1 */

    /* Update x=3 — cached partials recompute automatically */
    x0 = num_create_from_string("3");
    expr_set_val(x, x0);
    num_destroy(&x0);
    printf("\nAfter x=3:\n");
    num_printf("∂f/∂x      = %.101N\n", expr_eval(df_dx));     /* 8 */
    num_printf("∂f/∂y      = %.101N\n", expr_eval(df_dy));     /* 7 */

    expr_free(d2f_dxdy);
    expr_free(df_dy);
    expr_free(df_dx);
    expr_free(f);
    expr_free(t0);
    expr_free(y2);
    expr_free(xy);
    expr_free(x2);
    expr_free(y);
    expr_free(x);

    return 0;
}
```

```text
At x=1, y=2 (384 bits, 115 significant digits):
f        = 7.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000E+00
∂f/∂x = 4.00000000000000E+00
∂f/∂y = 5.00000000000000E+00
∂²f/∂x∂y = 1.00000000000000E+00

After x=3:
∂f/∂x = 8.00000000000000E+00
∂f/∂y = 7.00000000000000E+00
```

`expr_get_deriv` returns a *borrowed* pointer to the cached derivative — useful when
you only need to evaluate it and don't want to manage another owning handle:

```c
const expr_t *p = expr_get_deriv(f, x);   /* borrowed — do NOT free */
num_printf("∂f/∂x = %.101N\n", expr_eval(p));
```

The result is cached: repeated calls to `expr_get_deriv` with the same `wrt` variable
return the same node without rebuilding the expression graph.

## Example: Evaluating Derivatives

When one scalar output depends on many input variables, it is often more
efficient to evaluate the expression once and compute all requested derivatives
in a single pass than to build one symbolic derivative expression per variable.

```c
#include <stdio.h>
#include "expression.h"
#include "number.h"

int main(void) {
    number_t x0 = num_create_from_string("1");
    number_t y0 = num_create_from_string("2");
    expr_t *x = expr_new_named_var(x0, "x");
    expr_t *y = expr_new_named_var(y0, "y");
    expr_t *xy = expr_mul(x, y);
    expr_t *sin_xy = expr_sin(xy);
    expr_t *exp_xy = expr_exp(sin_xy);
    expr_t *log_y = expr_log(y);
    expr_t *x_log_y = expr_mul(x, log_y);
    expr_t *f = expr_add(exp_xy, x_log_y);

    const expr_t *vars[2] = { x, y };
    number_t value;
    number_t grads[2];

    num_destroy(&y0);
    num_destroy(&x0);
    num_set_default_prec_bits(384);

    if (expr_eval_derivatives(f, 2, vars, &value, grads) != 0)
        return 1;

    num_printf("f      = %.101N\n", value);
    num_printf("∂f/∂x  = %.101N\n", grads[0]);
    num_printf("∂f/∂y  = %.101N\n", grads[1]);

    num_destroy(&grads[1]);
    num_destroy(&grads[0]);
    num_destroy(&value);
    expr_free(f);
    expr_free(x_log_y);
    expr_free(log_y);
    expr_free(exp_xy);
    expr_free(sin_xy);
    expr_free(xy);
    expr_free(y);
    expr_free(x);
    return 0;
}
```

```text
Example: Evaluating Derivatives
Evaluating derivatives at x=1, y=2 (384 bits, 115 significant digits):
f        = 3.175724908574945831917149463420104893478850175303443992754304840163843037995939274550553871648852515239828467029229E+00
∂f/∂x = -1.373086555431723754562396899591396992661743594292079049199202945953125498212517447107943691791654177506104614887117E+00
∂f/∂y = -5.331168679958345319898145105247867803686218643261671516599414777232595600911060813569035093940364325240530479538437E-01
```

## Design Notes

### Node Model

Expressions are stored as a directed acyclic graph. Each node is one of:

- **constant** — a fixed `number_t` value, optionally named
- **variable** — a mutable `number_t` value, optionally named; changing it via
  `expr_set_val()` invalidates the cached primal and derivative values in all
  ancestor nodes
- **unary operator** — wraps one child (e.g. `sin`, `exp`, `sqrt`)
- **binary operator** — wraps two children (e.g. `+`, `*`, `pow`)

Shared subexpressions are represented once and referenced from multiple places
in the graph without duplication.

### Differentiation

Each node's vtable provides:

- a `deriv` hook that returns a new symbolic derivative subgraph
- a `reverse` hook that contributes adjoints for reverse-mode evaluation

Calling `expr_create_deriv(f, x)` recursively applies the chain rule across the
graph to produce `df/dx` as a new owning expression. Higher-order derivatives
are obtained by differentiating derivative expressions again with
`expr_create_nth_deriv()`. All derivative functions require an explicit `wrt`
variable pointer so the library knows which variable to differentiate with
respect to; all other variable nodes in the graph are treated as constants.

Unknown functions with several arguments retain formal coordinate derivatives;
MARS does not attempt to differentiate the argument list as a scalar. Repeated
and mixed derivatives preserve their coordinate order. A derivative with
respect to a coordinate absent from the explicit arguments is zero, including
when differentiating an already formal derivative. These operations support
the differential-equation module's general Lie determining equations.

This is symbolic differentiation, not forward-mode automatic differentiation:
it constructs another expression DAG. MARS does not currently expose a numeric
forward-mode Jacobian-vector-product (JVP) API. Such an API would propagate one
or more tangents through each node and is most useful when there are few input
directions and many outputs.

`expr_eval_derivatives(...)` does not build one symbolic derivative graph per
requested variable. It evaluates the primal once, then performs a reverse-mode
adjoint sweep over the existing DAG and returns owning `number_t` results for
the primal and requested first derivatives.

Reverse mode naturally computes a vector-Jacobian product (VJP). It is the
appropriate existing numeric path for a scalar output with many inputs. The
symbolic Jacobians built by `matrix_t` are neither JVPs nor VJPs; numeric
forward- or reverse-mode evaluation may consume their scalar expressions, but
does not change the symbolic calculus or simplification rules.

That reverse-mode path is numeric rather than purely symbolic. Unevaluated
integral nodes therefore remain symbolically differentiable through
`expr_create_deriv(...)`, but they are not expected to participate in
reverse-mode evaluation of a numeric primal.

The special-function reverse hooks include Bessel J, Bessel Y, Lommel s,
generalised hypergeometric pFq, Appell F₁ and Lauricella F_D. For the
multivariate families, adjoints are accumulated for every variable argument;
the parameter arguments are treated as constants by the current rules.

### Ownership and Reference Counting

Every owning handle has reference count ≥ 1. Arithmetic and function builders
retain their children (increment their refcounts) but do not steal ownership.
`expr_free()` decrements the refcount and recursively frees when it reaches zero.
`expr_get_deriv(expr, wrt)` returns a *borrowed* pointer — do not free it.

Formal derivatives retain a cache-free copy of their dependent operation's
root, while sharing its operand DAG and binding nodes. This prevents an owning
cycle between an expression's derivative cache and a formal derivative that
refers back to that expression. Cached derivatives remain reusable, and an
owning derivative can outlive the original expression without losing binding
updates.

### Evaluation

`expr_eval()` walks the DAG bottom-up, caching the `number_t` result in each
node. Subsequent calls without any `expr_set_val()` mutation return the cached
result immediately. Setting a variable's value with `expr_set_val()` marks
the cache invalid in the variable node; ancestor caches are invalidated lazily
on the next evaluation pass.

### Simplification

The library applies algebraic simplification rules during derivative
construction. This keeps derivative expressions compact and fast to evaluate.
`expr_simplify()` is the public algebraic pass. `expr_beautify()` first
simplifies and then arranges an equivalent expression for readable
presentation, including symmetric surds and Cartesian complex products. The
beautifier does not select a different expression for TeX output; rendering
style is applied afterwards.

Integrals and formal derivatives rendered as ordinary or partial derivative
fractions are kept outside enclosing algebraic fractions. Fractional
coefficients appear alongside them; reciprocal exponential coefficients use
negative exponents. Grouped inverse powers are used when a denominator itself
contains an integral or derivative. This preserves the expression tree and the
expression, unbound and function output styles.

In TeX products, scalar coefficients precede integrals and ordinary or partial
derivatives, including higher and mixed orders. Parenthesised coefficient
sums stay together before the calculus factor. The ordering is stable within
the coefficient and calculus groups and also applies to negative products;
it changes presentation only. See the triangular-transport example in
[the differential-equation guide](diffequation.md) for a tested PDE display.
Adjacent calculus factors receive an explicit multiplication dot rather than
running derivative fractions or integrals together. The gradient-envelope
example in that guide tests this product display.
Authored differential equations use a scoped native TeX mode that preserves
nested additive order, including coefficient sums, without changing the
normal polynomial ordering of simplified expression results. The coupled-flow
example in the same guide tests this distinction.

Paired symmetric function arguments and definite-integral bounds retain their
common centre first in TeX: $a+b$ together with $a-b$. The formatter recognises
the common term and opposite shifts structurally, independently of function
or variable names. Unpaired sums retain their ordinary ordering, and the
underlying expressions are unchanged.

When an elementary function has a supported symbolic Cartesian identity, the
native display pass separates an explicit `x + iy` argument into real and
imaginary parts. For example, `exp(x + iy)` becomes
`exp(x)·cos(y) + exp(x)·sin(y)·i`. The pass covers `exp`, `ln`, `lg`, all six
circular functions, all six hyperbolic functions, and all twelve inverse
circular and inverse hyperbolic functions. It applies equally to pure-imaginary
arguments. The accepted common-logarithm aliases `log` and `log10` use the same
base-10 operation and are rendered canonically as `lg`.

Differentiation and integration with respect to either Cartesian component
preserve the separated `p + qi` structure. The imaginary unit remains the
final factor of the imaginary term, and an indefinite integration constant is
always the final addend.

Compositions involving complex `Li` or `Ei` use explicit real series when
separating the enclosing algebra. TeX defines the shared real and imaginary
components once beneath the result, rather than inserting a complete identity
inside an exponent or function argument. Expression output expands those same
definitions, while Function output shares repeated subexpressions through
temporary variables and returns the separated real and imaginary coefficients.
Numerical evaluation continues to use the native functions at the selected
precision, rather than a fixed truncation of the displayed series.

TeX uses postfix factorial notation only for arguments known to be non-negative
integers; otherwise it uses gamma notation. Summation and product indices obtain
this guarantee from their bounds, not from their names or supplied binding values.

---

## API Reference

All public declarations are in `include/expression.h`.

### Constructors — Constants

- `EXPR_ZERO`, `EXPR_ONE`, `EXPR_LN10` — process-lifetime singleton constant nodes for common differentiable constants
- `expr_t *expr_new_const(number_t x)` — constant node from a `number_t`
- `expr_t *expr_new_named_const(number_t x, const char *name)` — named constant from a `number_t`

### Constructors — Variables

- `expr_t *expr_new_var(number_t x)` — variable node from a `number_t`
- `expr_t *expr_new_named_var(number_t x, const char *name)` — named variable from a `number_t`

### Mutators

- `void expr_set_val(expr_t *expr, number_t value)` — set the primal value from a `number_t`
- `void expr_set_name(expr_t *expr, const char *name)` — set or replace the symbolic name

### Accessors

- `number_t expr_get_val(const expr_t *expr)` — return the current primal value as an owning `number_t`
- `const expr_t *expr_get_deriv(const expr_t *expr, const expr_t *wrt)` — return the
  cached ∂expr/∂wrt node as a **borrowed** pointer; do **not** free it. Built
  lazily on first call; subsequent calls with the same `wrt` return the cached node.

### Evaluation

- `number_t expr_eval(const expr_t *expr)` — evaluate and return an owning `number_t`
- `int expr_eval_derivatives(const expr_t *expr, size_t nvars, const expr_t *const *vars, number_t *value_out, number_t *derivs_out)` — reverse-mode derivative evaluation returning owning `number_t` primal and derivative results

There is deliberately no forward-mode entry in this list yet. Repeated
symbolic differentiation is not described as forward mode.

### Simplification and presentation

- `expr_t *expr_simplify(const expr_t *expr)` — return an algebraically simplified owning expression
- `expr_t *expr_beautify(const expr_t *expr)` — simplify and arrange an equivalent owning expression for readable presentation

### Derivative Construction (owning)

All returned handles must be freed by the caller. `wrt` must be a variable node
(created with `expr_new_var()` or `expr_new_named_var()`) that appears in the expression
DAG. All other variable nodes in the graph are treated as constants. If `wrt`
is a named constant node, the result is symbolic `NaN`.

- `expr_t *expr_create_deriv(const expr_t *expr, const expr_t *wrt)` — build ∂expr/∂wrt
- `expr_t *expr_create_2nd_deriv(const expr_t *expr, const expr_t *wrt1, const expr_t *wrt2)` — build ∂²expr/(∂wrt1 ∂wrt2)
- `expr_t *expr_create_3rd_deriv(const expr_t *expr, const expr_t *wrt1, const expr_t *wrt2, const expr_t *wrt3)` — build the mixed third derivative
- `expr_t *expr_create_nth_deriv(unsigned int n, const expr_t *expr, const expr_t *wrt)` — apply d/d(wrt) n times

### Arithmetic (graph-building, owning)

All functions return owning handles.

- `expr_t *expr_neg(const expr_t *expr)` — `-expr`
- `expr_t *expr_add(const expr_t *left, const expr_t *right)` — `left + right`
- `expr_t *expr_sub(const expr_t *left, const expr_t *right)` — `left - right`
- `expr_t *expr_mul(const expr_t *left, const expr_t *right)` — `left * right`
- `expr_t *expr_div(const expr_t *left, const expr_t *right)` — `left / right`
- `expr_t *expr_add_num(const expr_t *expr, const number_t *value)` — `expr + value`
- `expr_t *expr_sub_num(const expr_t *expr, const number_t *value)` — `expr - value`
- `expr_t *expr_num_sub(const number_t *value, const expr_t *expr)` — `value - expr`
- `expr_t *expr_mul_num(const expr_t *expr, const number_t *value)` — `expr * value`
- `expr_t *expr_div_num(const expr_t *expr, const number_t *value)` — `expr / value`
- `expr_t *expr_num_div(const number_t *value, const expr_t *expr)` — `value / expr`

### Comparison

- `int expr_cmp(const expr_t *left, const expr_t *right)` — compare by primal numeric value using lexicographic order (real part first, then imaginary part); returns -1, 0, or 1

### Elementary Functions (owning)

- `expr_t *expr_apply_unary_function(const char *name, const expr_t *argument, const char **canonical_name_out)` —
  resolve any registered unary name or alias through the parser's perfect hash and construct the corresponding expression
- `expr_t *expr_sin(const expr_t *expr)` — sin
- `expr_t *expr_cos(const expr_t *expr)` — cos
- `expr_t *expr_tan(const expr_t *expr)` — tan
- `expr_t *expr_sec(const expr_t *expr)` — sec
- `expr_t *expr_cosec(const expr_t *expr)` — cosec
- `expr_t *expr_cot(const expr_t *expr)` — cot
- `expr_t *expr_versin(const expr_t *expr)` — versed sine `1 - cos(x)`
- `expr_t *expr_vercos(const expr_t *expr)` — versed cosine `1 + cos(x)`
- `expr_t *expr_coversin(const expr_t *expr)` — coversed sine `1 - sin(x)`
- `expr_t *expr_covercos(const expr_t *expr)` — coversed cosine `1 + sin(x)`
- `expr_t *expr_haversin(const expr_t *expr)` — haversine `(1 - cos(x)) / 2`
- `expr_t *expr_havercos(const expr_t *expr)` — havercosine `(1 + cos(x)) / 2`
- `expr_t *expr_hacoversin(const expr_t *expr)` — hacoversine `(1 - sin(x)) / 2`
- `expr_t *expr_hacovercos(const expr_t *expr)` — hacovercosine `(1 + sin(x)) / 2`
- `expr_t *expr_sinh(const expr_t *expr)` — sinh
- `expr_t *expr_cosh(const expr_t *expr)` — cosh
- `expr_t *expr_tanh(const expr_t *expr)` — tanh
- `expr_t *expr_sech(const expr_t *expr)` — sech
- `expr_t *expr_cosech(const expr_t *expr)` — cosech
- `expr_t *expr_coth(const expr_t *expr)` — coth
- `expr_t *expr_asin(const expr_t *expr)` — arcsin
- `expr_t *expr_acos(const expr_t *expr)` — arccos
- `expr_t *expr_atan(const expr_t *expr)` — arctan
- `expr_t *expr_asec(const expr_t *expr)` — arcsec
- `expr_t *expr_acosec(const expr_t *expr)` — arccosec
- `expr_t *expr_acot(const expr_t *expr)` — arccot
- `expr_t *expr_arcversin(const expr_t *expr)` — inverse versed sine `acos(1 - x)`
- `expr_t *expr_arcvercos(const expr_t *expr)` — inverse versed cosine `acos(x - 1)`
- `expr_t *expr_arccoversin(const expr_t *expr)` — inverse coversed sine `asin(1 - x)`
- `expr_t *expr_arccovercos(const expr_t *expr)` — inverse coversed cosine `asin(x - 1)`
- `expr_t *expr_archaversin(const expr_t *expr)` — inverse haversine `acos(1 - 2x)`
- `expr_t *expr_archavercos(const expr_t *expr)` — inverse havercosine `acos(2x - 1)`
- `expr_t *expr_archacoversin(const expr_t *expr)` — inverse hacoversine `asin(1 - 2x)`
- `expr_t *expr_archacovercos(const expr_t *expr)` — inverse hacovercosine `asin(2x - 1)`
- `expr_t *expr_atan2(const expr_t *y, const expr_t *x)` — four-quadrant arctan2(y, x)
- `expr_t *expr_asinh(const expr_t *expr)` — inverse hyperbolic sine
- `expr_t *expr_acosh(const expr_t *expr)` — inverse hyperbolic cosine
- `expr_t *expr_atanh(const expr_t *expr)` — inverse hyperbolic tangent
- `expr_t *expr_asech(const expr_t *expr)` — inverse hyperbolic secant
- `expr_t *expr_acosech(const expr_t *expr)` — inverse hyperbolic cosecant
- `expr_t *expr_acoth(const expr_t *expr)` — inverse hyperbolic cotangent
- `expr_t *expr_exp(const expr_t *expr)` — natural exponential
- `expr_t *expr_log(const expr_t *expr)` — natural logarithm
- `expr_t *expr_ln(const expr_t *expr)` — shorthand for `expr_log(expr)`; it constructs the same natural-logarithm
  operation
- `expr_t *expr_log10(const expr_t *expr)` — common logarithm
- `expr_t *expr_lg(const expr_t *expr)` — shorthand for `expr_log10(expr)`; it constructs the same common-logarithm
  operation
- `expr_t *expr_sqrt(const expr_t *expr)` — the single-valued principal square root. For complex `z`, MARS defines
  it as `exp(Log(z)/2)`, with the principal argument in `(-pi, pi]`. Thus the real part is non-negative and a value
  on the negative real axis has a positive imaginary square root.
- `expr_t *expr_cubrt(const expr_t *expr)` — the single-valued principal cube root.
- `expr_t *expr_root(const expr_t *expr, const expr_t *order)` — the single-valued principal integer-order root;
  `order` must evaluate to an integer greater than one.
- Symbolic differentiation supports both functions. For constant integer `n`,
  `d(root(u,n))/dx = root(u,n)u'/(nu)`, and `cubrt(u)` uses the corresponding `n = 3` rule. Reverse-mode
  differentiation uses the same local derivatives.
- Symbolic integration recognises affine radicands: for constant integer `n > 1` and constant non-zero `u'`,
  `integral(root(u,n),x) = n*u*root(u,n)/((n+1)u')`; `cubrt(u)` is the `n = 3` case.
- Explicit `z^(1/n)` syntax denotes the complete family of `n` roots. MARS Lab displays every member of the family;
  for example, `(3+4i)^(1/2)` displays `{ 2+i, -2-i }`, while `sqrt(3+4i)` displays only `2+i`. Numeric evaluation
  of the underlying power node uses the principal member when a scalar value is required. MARS Lab also retains the
  complete branch family when differentiating explicit fractional powers and labels the numeric result card
  **Values** when it contains more than one branch.
- Exact complex principal roots are beautified into Cartesian surds when MARS
  can prove such a representation. For example, `root(117+44i,6)` becomes
  `½(2√3+1) + i(1-√3/2)`, whose numerical value is approximately
  `2.232050807568877 + 0.133974596215561i`. In contrast,
  `(117+44i)^(1/6)` retains the complete six-member root family.
- `expr_t *expr_floor(const expr_t *expr)` — floor
- `expr_t *expr_ceil(const expr_t *expr)` — ceiling
- `expr_t *expr_pow(const expr_t *expr, const number_t *exponent)` — `expr ^ exponent` (borrowed scalar numeric exponent)
- `expr_t *expr_pow_xp(const expr_t *base, const expr_t *exponent)` — `base ^ exponent`
- `expr_t *expr_new_finite_summation_range(const expr_t *term, const expr_t *index, const expr_t *lower, const expr_t *upper)` — construct an unevaluated finite sigma with inclusive bounds
- `expr_t *expr_new_finite_product_range(const expr_t *term, const expr_t *index, const expr_t *lower, const expr_t *upper)` — construct an unevaluated finite product with inclusive bounds

The string grammar accepts `conj(z)` and `conjugate(z)` for complex
conjugation. Postfix `z^*` is equivalent. `abs(z)` and paired bars `|z|` are
also equivalent for every scalar type; for complex values and expressions
they denote the modulus `sqrt(z*z^*)`. An unmatched bar is a syntax error.
These function names are resolved by the native expression parser's
collision-free lookup tables rather than by a client-side rewrite.

### Special Functions (owning)

- `expr_t *expr_abs(const expr_t *expr)` — absolute value
- `expr_t *expr_conj(const expr_t *expr)` — complex conjugate
- `expr_t *expr_hypot(const expr_t *left, const expr_t *right)` — sqrt(left² + right²)
- `expr_t *expr_erf(const expr_t *expr)` — error function
- `expr_t *expr_erfc(const expr_t *expr)` — complementary error function
- `expr_t *expr_erfinv(const expr_t *expr)` — inverse error function
- `expr_t *expr_erfcinv(const expr_t *expr)` — inverse complementary error function
- `expr_t *expr_gamma(const expr_t *expr)` — Γ(x)
- `expr_t *expr_lgamma(const expr_t *expr)` — ln|Γ(x)|
- `expr_t *expr_digamma(const expr_t *expr)` — ψ(x) = d/dx ln Γ(x)
- `expr_t *expr_qdigamma(const expr_t *q, const expr_t *expr)` — q-digamma ψ_q(x), with formal
  partial derivatives in both arguments and a formal primitive when no elementary antiderivative is available
- `expr_t *expr_trigamma(const expr_t *expr)` — ψ'(x)
- `expr_t *expr_polygamma(unsigned int order, const expr_t *expr)` — ψ⁽ⁿ⁾(x)
- `expr_t *expr_zeta(const expr_t *s)` — Riemann zeta ζ(s)
- `expr_t *expr_zetap(const expr_t *s)` — derivative ζ′(s)
- `expr_t *expr_zetah(const expr_t *s, const expr_t *a)` — Hurwitz zeta ζ(s, a)
- `expr_t *expr_zatahp(const expr_t *s, const expr_t *a)` — partial derivative ∂ζ(s, a)/∂s
- `expr_t *expr_gammainv(const expr_t *expr)` — principal Γ⁻¹(x)
- `expr_t *expr_gammainc_lower(const expr_t *s, const expr_t *x)` — lower incomplete gamma γ(s, x)
- `expr_t *expr_gammainc_upper(const expr_t *s, const expr_t *x)` — upper incomplete gamma Γ(s, x)
- `expr_t *expr_gammainc_P(const expr_t *s, const expr_t *x)` — regularised lower incomplete gamma P(s, x)
- `expr_t *expr_gammainc_Q(const expr_t *s, const expr_t *x)` — regularised upper incomplete gamma Q(s, x)
- `expr_t *expr_lambert_w(const expr_t *expr)` — branch-choosing Lambert W/ProductLog helper
- `expr_t *expr_lambert_w0(const expr_t *expr)` — Lambert W principal branch W₀(x)
- `expr_t *expr_lambert_wm1(const expr_t *expr)` — Lambert W branch W₋₁(x)
- `expr_t *expr_beta(const expr_t *left, const expr_t *right)` — $\mathrm B(a,b)$;
  Expression style uses `B`, TeX uses $\mathrm B$, and Function style uses `beta`.
  The parser accepts `beta`, `B`, and Greek capital beta `Β`.
- `expr_t *expr_logbeta(const expr_t *left, const expr_t *right)` — ln B(a, b); Expression style writes
  `lnB(a, b)`, Function style writes `logbeta(a, b)`, and legacy `logbeta(a, b)` expression input remains accepted
- `expr_t *expr_beta_pdf(const expr_t *x, const expr_t *a, const expr_t *b)` — beta distribution PDF
- `expr_t *expr_logbeta_pdf(const expr_t *x, const expr_t *a, const expr_t *b)` — log beta distribution PDF
- `expr_t *expr_binomial(const expr_t *n, const expr_t *k)` — binomial coefficient
- `expr_t *expr_normal_pdf(const expr_t *expr)` — standard normal PDF φ(x)
- `expr_t *expr_normal_cdf(const expr_t *expr)` — standard normal CDF Φ(x)
- `expr_t *expr_normal_logpdf(const expr_t *expr)` — ln φ(x)
- `expr_t *expr_Ei(const expr_t *expr)` — Ei(x), exponential integral
- `expr_t *expr_Li(const expr_t *expr)` — Li(x), logarithmic integral on the principal branch
- `expr_t *expr_E1(const expr_t *expr)` — E₁(x), exponential integral
- `expr_t *expr_dilog(const expr_t *expr)` — principal dilogarithm Li₂(x)
- `expr_t *expr_clausen2(const expr_t *argument)` — order-two Clausen function, with native differentiation, integration and summation support
- `expr_t *expr_clausen(unsigned long order, const expr_t *argument)` — positive integer-order Clausen family
- `expr_t *expr_polylog1(const expr_t *expr)` — order-one polylogarithm Li₁(x) = −Log(1−x), with dedicated derivative, reverse-mode and antiderivative operations
- `expr_t *expr_polylog(unsigned int order, const expr_t *expr)` — polylogarithm Liₙ(x) for non-negative integer orders currently supported by the implementation
- `expr_t *expr_harmonic_poly(const expr_t *degree, const expr_t *argument)` — native harmonic polynomial Hₙ(x) = Σₖ₌₁ⁿ xᵏ/k with a symbolic degree expression
- `expr_t *expr_lerch_phi(const expr_t *z, const expr_t *s, const expr_t *a)` — Lerch transcendent Φ(z,s,a), with derivatives in all three arguments
- `expr_t *expr_legendre_chi(unsigned int order, const expr_t *expr)` — Legendre chi χₙ(x) for non-negative integer orders currently supported by the implementation
- `expr_t *expr_bessel_j(const expr_t *order, const expr_t *argument)` — Bessel function of the first kind J_order(argument), with a symbolic real order
- `expr_t *expr_bessel_y(const expr_t *order, const expr_t *argument)` — Bessel function of the second kind Y_order(argument), with a symbolic real order
- `expr_t *expr_lommel_s(const expr_t *mu, const expr_t *nu, const expr_t *argument)` — lower-case Lommel function s_mu,nu(argument); differentiation with respect to the argument is supported
- `expr_t *expr_appell_f1(const expr_t *a, const expr_t *b1, const expr_t *b2, const expr_t *c, const expr_t *x, const expr_t *y)` — Appell hypergeometric function F₁(a; b₁, b₂; c; x, y)
- `expr_t *expr_lauricella_f(const expr_t *a, size_t variable_count, const expr_t *const *b, const expr_t *c, const expr_t *const *x)` — Lauricella F_D in `variable_count` variables; Appell F₁ is the two-variable member of this family
- `expr_t *expr_hypergeometric_pFq(size_t upper_count, const expr_t *const *upper, size_t lower_count, const expr_t *const *lower, const expr_t *argument)` — generalised hypergeometric pFq with explicit upper and lower parameter arrays

The parser accepts `Li1(x)` and `Li₁(x)` for the dedicated order-one
polylogarithm node. Mathematical styles render Li₁(x), while
`style_FUNCTION` emits `li1(x)`. Its derivative is `x'/(1-x)`, and direct
integration of `Li1(x)` gives `(1-x)ln(1-x)-(1-x)`. The general
`polylog(1,x)` evaluator uses the same numerical identity without replacing
the dedicated node or its calculus operations.

The parser accepts `lerch_phi(z,s,a)`, `LerchPhi(z,s,a)` and `Φ(z,s,a)`.
`style_EXPRESSION` uses the capital-phi form `Φ(z,s,a)`, `style_FUNCTION` uses
`lerchphi(z,s,a)`, and TeX uses $\Phi$. Numerical evaluation is limited to the
implemented defining-series domain and exact reductions; unsupported analytic
continuations remain unavailable instead of silently selecting a branch.

The parser accepts `Hn(n, x)`, `harmonic_poly(n, x)`, and `harmonicpoly(n, x)`. Mathematical
styles render the former as Hₙ(x); `style_FUNCTION` emits the typeable
`harmonicpoly(n, x)` spelling. For a degree independent of `x`, symbolic
differentiation uses `(1 - x^n)/(1 - x)`, and direct integration uses
`x*Hn(n, x) - Hn(n + 1, x) + x`. These rules remain available under repeated
differentiation.

#### Clausen functions

`expr_clausen2(argument)` constructs the order-two Clausen function;
`expr_clausen(order, argument)` constructs a fixed positive integer order.
Arguments are angles in radians. Even orders use the real sine Fourier
series and odd orders the cosine Fourier series; numerical evaluation uses
the real or holomorphic semantics of [the number API](number.md#clausen-functions).

The parser accepts `Cl2`, `cl2`, `Cl₂`, `cl₂`, `clausen`, `Clausen`,
`clausen2`, `Clausen2` and `clausen_2` for order two, and `Cl(n,x)` or
`cl(n,x)` for the general family. Expression text displays `Cl₂`; function
notation uses `clausen2`; TeX uses $\operatorname{Cl}_{2}$. General orders
display the corresponding subscript in TeX. The standard abbreviation is
Latin, so there is no Greek alias.

For `n >= 2`, the derivative is `(-1)^n*Cl(n-1,x)`; the derivative of order
one is `-cot(x/2)/2`. An antiderivative of `Cl(n,x)` is
`(-1)^(n+1)*Cl(n+1,x)`. Affine arguments include their chain-rule factors,
and symbolic and reverse-mode differentiation are supported.

The integrator reduces logarithms of affine sine and cosine to order-two
Clausen functions. Plain real logarithms require positive arguments;
logarithms of absolute values are valid between consecutive zeros.
Differential-equation solvers use this same native integration API; see
the tested [tangent-forced PDE example](diffequation.md) for its real closed
form and domain.

The summation engine recognises the defining infinite Fourier series where
the angle is provably real and the series converges. It also recognises a
complete finite period of equally spaced Clausen arguments. It does not
apply real Fourier identities to unproved complex bindings.

### Value-Only Functions (owning)

These functions evaluate through `number_t` and can be parsed by MARS Lab, but
they are not differentiable. Front-ends can call `expr_is_differentiable(...)` to
decide whether derivative controls should be shown.

- `expr_t *expr_factorial(const expr_t *n)` — exact factorial; parser shorthand `n!` lowers to `gamma(n + 1)` for differentiable symbolic inputs
- `expr_t *expr_fibonacci(const expr_t *n)` — exact Fibonacci number
- `expr_t *expr_partition(const expr_t *n)` — exact integer partition count p(n)
- `expr_t *expr_isqrt(const expr_t *n)` — exact integer square root
- `expr_t *expr_gcd(const expr_t *a, const expr_t *b)` — greatest common divisor
- `expr_t *expr_lcm(const expr_t *a, const expr_t *b)` — least common multiple
- `expr_t *expr_mod(const expr_t *a, const expr_t *b)` — exact integer remainder
- `expr_t *expr_modinv(const expr_t *a, const expr_t *b)` — modular inverse
- `expr_t *expr_is_prime(const expr_t *n)` — primality predicate as a numeric value
- `expr_t *expr_next_prime(const expr_t *n)` — next prime
- `expr_t *expr_prev_prime(const expr_t *n)` — previous prime
- `expr_t *expr_bit_and(const expr_t *a, const expr_t *b)` — bitwise AND
- `expr_t *expr_bit_or(const expr_t *a, const expr_t *b)` — bitwise OR
- `expr_t *expr_bit_xor(const expr_t *a, const expr_t *b)` — bitwise XOR
- `expr_t *expr_bit_not(const expr_t *a)` — bitwise NOT over the active bit width
- `expr_t *expr_shl(const expr_t *a, const expr_t *bits)` — left shift
- `expr_t *expr_shr(const expr_t *a, const expr_t *bits)` — right shift
- `expr_t *expr_factors(const expr_t *n)` — factorise an exact integer and return
  an expression DAG whose constant bindings hold the prime factors

### Lifetime Management

- `void expr_free(expr_t *expr)` — decrement refcount; free the node and recursively its children when it reaches zero. Must be called exactly once per owning handle.

### String Conversion

Explicit univariate power series with an `O(...)` remainder are rendered in
ascending powers of the expansion variable (or its shift), with the remainder
last. This ordering is shared by expression, unbound, TeX, and function styles;
ordinary polynomial expressions retain their usual degree ordering.

- `string_t *expr_to_text(const expr_t *expr, style_t style)` — serialise the expression; `style` is `style_FUNCTION`, `style_EXPRESSION`, `style_LATEX`, or `style_UNBOUND`. In expression style, `sqrt(...)` is printed as `√(...)` and `abs(...)` as `|...|`. `style_UNBOUND` returns the expression body before the `{ body | bindings }` wrapper is added. Returns a newly allocated string; the caller must release it with `string_free(...)`.
- `void expr_print(const expr_t *expr)` — print the expression to stdout in `style_EXPRESSION` format

`style_FUNCTION` prints a small MARS evaluable sketch. Untyped parameters are
treated as differentiable variables, while `const` parameters and declarations
are non-differentiable constants. Scalar and array bindings use the same
structure: array parameters are introduced with `array`, array constants with
`array const`, and both known and unknown bindings are declared before the final
`output(expr(...))` call. A compact `.` denotes multiplication within a line,
and `/` is likewise written without surrounding spaces. A `.` followed by
whitespace or end of input terminates a statement. Consequently, several
statements may share a line when whitespace separates them. Unknown scalars use
`?`; `[]` and `[?]` both denote an
unspecified array and are serialised canonically as `[?]`. Symbolic assignment
values use input aliases such as `@pi`.

Function style writes exact fractions in typeable ASCII `numerator/denominator`
form. For example, a mathematical card may display `³⁄₁₁`, while the corresponding
generated function writes `3/11`.

Before emitting the return statement, Function style traverses the expression
DAG and gives shared non-trivial nodes intermediate names in dependency order.
Constant intermediates use `c1`, `c2`, and so on, while intermediates that
depend on variables use `v1`, `v2`, and so on. Constant intermediates are
grouped before variable-dependent intermediates wherever their dependencies
allow it. This keeps repeated work visible and makes the generated function
suitable for later differentiation. Descriptive identifiers such as
`$[sqrt(2)]` remain valid input aliases for bracketed names and are
canonicalised to `[sqrt(2)]`, but Function-style output does not generate the
`$[...]` form.

Built-in function names in `style_FUNCTION` use canonical concatenated lowercase
spellings, such as `normalpdf`, `gammainclower`, `besselj`, `lerchphi`,
`hypergeometricpfq`, and the bit operations `and`, `or`, `xor`, `not`, `shl`,
and `shr`. Existing mixed-case and underscore spellings remain accepted as
Expression input aliases and retain their mathematical rendering in other
styles.

MARS Lab applies presentation-only syntax colouring to this text: function
names are bold turquoise and their matching call brackets are non-bold gold.
Grouping brackets retain the ordinary text colour, and copied Function text is
unchanged.

Function-style ordinary comments use backticks. One backtick opens a delimited
comment and the next unescaped backtick closes it; the comment may span lines.
Two consecutive opening backticks introduce a line comment that ends at a
newline or end of input. The lexer applies longest-match, so two backticks always
open a line comment rather than representing an empty delimited comment. A
backslash escapes a literal backtick within a delimited comment. Comments behave
as whitespace before full-stop statement termination is interpreted.

The differential-equation module uses a private parser variant that accepts
undeclared symbolic calls while retaining implicit multiplication for scalar
bindings. This leaves ordinary Expression and Function input unchanged; see
[symbolic wave data](diffequation.md#forced-one-dimensional-wave-initial-value-problems).

Unevaluated integral nodes are
printed in function form as `@S^upper integrand d<dummy>` and in
expression form as `∫^upper integrand d<dummy>`; for example,
`∫^x exp(cosh(t))·dt`:

```text
expression expr(x, y, const c₀) {
    return tan(c₀.x.y / 2).
}

x = 3.29929295579108949982756921421358070866178174810740656177232818327906094186165.
y = 3.29929295579108949982756921421358070866178174810740656177232818327906094186165.
const c₀ = @eulermascheroni.
output(expr(x, y, c₀)).
```

### Additive Ellipsis Series

The native parser recognises an ellipsis only when the surrounding additive
terms establish a single exact model. Geometric and inverse-index-power forms
use their corresponding identities. Otherwise, MARS constructs the unique
Lagrange polynomial `P(k)` through the supplied coefficient terms, verifies
that the written terminal term agrees with `P(N)`, and lowers the series to
`Σ(k=1..N) P(k)`. The TeX derivation records that sigma form before the
simplified sum and final value. If an endpoint is already named `n`, the dummy
index changes to `k`, `j`, `m`, or `l` so that the two bindings cannot collide.

For the inverse-power family `Σ(k=1..n) 1/k^p`, the generic finite expression
is `ζ(p) - ζ(p, n + 1)` away from `p = 1`. At `p = 1`, both zeta terms have a
pole and only their combined removable-singularity limit is finite; that limit
is `digamma(n + 1) + gamma`. With `p` unset, the TeX derivation therefore
displays separate `p = 1` and `p != 1` branches while the generic expression
and function use `zeta(p) - zetah(p, n + 1)`. When the input supplies `p = 1`,
native MARS marks that domain-required specialisation and all three algebraic
cards use `digamma(n + 1) + gamma`; its sigma derivation likewise specialises
the summand to `1/k` rather than retaining `1/k^p`. MARS also selects
domain-safe alternatives when applicable: the Faulhaber polynomial at a
non-positive integer `p`, and
the familiar `pi^2/6 - trigamma(n + 1)` at `p = 2`. With `n = inf`, the
inverse-square series simplifies exactly to `pi^2/6`, while another real
exponent `p > 1` evaluates to `zeta(p)`; a non-convergent infinite series has
no finite value. The endpoint may be supplied through a binding such as
`n = inf`, or directly in the terminal term, as in
`1 + 1/2^2 + 1/3^2 + ... + 1/inf^2`.
The direct-power family `Σ(k=1..n) k^p` similarly becomes
`ζ(-p) - ζ(-p, n + 1)`; non-negative integer exponents evaluate through the
corresponding Faulhaber polynomial.
Symbolic exponents accept either a named endpoint or a literal positive-integer
endpoint. The first unit term may retain its explicit power, including reciprocal
and negative-exponent spellings. A literal endpoint must follow the supplied
consecutive prefix; inconsistent exponents, skipped indices and fractional
endpoints are rejected rather than guessed.
An open-ended consecutive prefix also accepts an unset symbolic exponent.
Every supplied term must agree with the inferred consecutive indices. The
result retains a native sum node and its convergence domain. Evaluation uses
Riemann zeta for starts at one or two, or Hurwitz zeta for later starts, only
when the real part of the zeta argument exceeds one. Outside that domain no
convergent numerical value is supplied, including after bindings change.
Explicit `sum` and `@Z` forms follow the same rule. Analytic continuation is
available through an explicit zeta expression, not an implicit reinterpretation
of the sum.
In Expression mode, the Lab's Rendered TeX card also displays the corresponding
Riemann or Hurwitz zeta identity with its real-part convergence restriction.
The Expression card retains the sum. For a recognised infinite power sum at the
function's root, Function output emits an explicit real-part convergence check:
the convergent branch returns the zeta closed form, while the other branch returns
`@nan` with a comment explaining that the series has no convergent value there.
The check uses the symbolic order, not its supplied binding. This guarded
representation does not extend the domain used by the Value card. Finite sums
and unrecognised infinite sums keep their existing Function representations,
except that a finite Riemann/Hurwitz zeta difference emits its removable-pole
case explicitly: at order one it returns the endpoint's digamma plus Euler's
constant, and otherwise returns the zeta difference. This is not an infinite-sum
convergence restriction; finite power sums do not require real part greater than
one. The guard remains symbolic even when an order binding is supplied.
For these finite zeta differences, the Lab's Expression card displays two
conditioned expressions on separate lines. Each uses the differential-equation
layout: body, a vertical bar, variable bindings, constant bindings and conditions,
with semicolons separating the three latter sections. Both lines repeat the
bindings; the first carries order equal to one and the second order unequal to
one. The native renderer supplies the entire display. This is currently a
display-only case listing, not new expression input syntax: the editor and
Use as input retain the original parseable expression.
The editable input bindings remain separate from the result-card bindings:
selecting a special-case result must not remove a parameter from the original
input's binding boxes.
Known limitation: a supplied order of one can still select the harmonic-number
formula before rendering, removing the generic case split from the result cards.
The TeX identity then omits its required order-one condition. The numerical value
is unaffected, but preserving the symbolic conditions across this path remains
outstanding.
Finite prime-power prefixes beginning with the primes two, three and five are
also recognised, provided every written prime is consecutive and the terminal
prime lies beyond the prefix. They become native finite sums filtered by the
existing primality predicate, not ordinary zeta differences. Direct powers,
reciprocal powers and negative symbolic exponents share this recognition;
supplied exponent bindings affect only the numerical Value card.
The endpoint may also be a named prime bound. It remains symbolic in all
algebraic cards, appears as an input binding, and enables numerical evaluation
once its value and the exponent are supplied. The dummy summation index is
chosen independently so it cannot capture the endpoint or exponent binding.
Writing the inverse-power family with negative exponents, as in `1 + 2^-p +
3^-p + ... + n^-p`, produces the same sigma form, closed form and calculus
operations as `1 + 1/2^p + 1/3^p + ... + 1/n^p`.
Symbolic geometric endpoints are also recognised directly: `1 + 3 + 9 + 27 +
... + 3^(n - 1)` becomes `Σ(k=0..n-1) 3^k = (3^n - 1)/2`, and therefore
evaluates to `29524` when `n = 10`.
The Rendered TeX, Expression and Function cards all come from this same native
simplified expression, while only the Value card substitutes supplied
bindings.

### Parsing

- `expr_t *expr_from_string(const char *s, expr_bindings_t **bnd_out)` — construct an `expr_t` from either bare shorthand or the wrapped format produced by `expr_to_text(..., style_EXPRESSION)`. The parser preserves the written expression shape while canonicalising notation; call `expr_simplify(...)` explicitly for algebraic simplification. When `bnd_out` is non-NULL and the parse is symbolic, the parser also returns an opaque bindings object.
- `expr_t *expr_from_string_with_derivation_TeX(const char *s, expr_bindings_t **bnd_out, string_t **derivation_TeX_out)` — parse the same grammar and additionally return the owning native sigma derivation when the input contains a recognised finite ellipsis series; otherwise the derivation output is `NULL`
- `expr_t *expr_bindings_get(expr_bindings_t *bnd, const char *name)` — find a returned symbolic binding by name; lookup accepts the same normalisation rules as parsing, so aliases like `@pi`/`π`, `@phi`/`φ`, `@gamma`/`γ`, and `@tau`/`τ` all resolve to the same binding
- `void expr_bindings_free(expr_bindings_t *bnd)` — destroy a bindings object returned by `expr_from_string(...)`

  ```
  expr
  { expr }
  { expr | x₀ = val, ...; [name] = val, ... }
  ```

  Bare `expr` is equivalent to `{ expr }`. MARS itself infers the binding
  names and classifications; callers should pass the original text unchanged.
  Variables appear before the `;`; named constants appear after it.
  If there is no `;`, all bindings are treated as variables.
  If the binding section begins with `;`, all bindings are treated as named constants.
  If there is no binding section and the expression still contains symbolic
  names, `expr_from_string(...)` infers them from mathematical conventions and
  initialises every discovered symbol to `NaN`.
  Returns an owning handle on success, or NULL on error (details written to stderr).

  Accepted shorthand in the string:
  - `x_0` for subscript x₀
  - trailing ASCII digits are canonicalised to Unicode subscripts, so
    `a1`, `a12`, and `@pi2` normalise to `a₁`, `a₁₂`, and `π₂`
  - `*` for explicit multiplication
  - `.` as an ASCII multiplication stand-in when it appears between factors,
    so `x.y` parses like `x·y` while decimal numerals such as `1.5` remain
    decimal numbers
  - `^N` or `^1.5` for ASCII exponents after a variable, constant, or parenthesised sub-expression
  - `sin^2(x)` style ASCII exponents on function names
  - postfix factorial `x!`, which lowers to `gamma(x + 1)` when it remains
    symbolic and differentiable
  - additive ellipsis series such as `a₁ + a₂ + a₃ + ... + aₙ`; the native
    parser selects an exact geometric, inverse-index-power, or Lagrange-
    interpolated polynomial term model from the supplied terms and endpoint
  - literal unevaluated integral forms `integral(x, f_expr, t)`,
    `∫^x f(t)dt`, `∫^x f(t)*dt`, and `∫^x f(t)·dt`; the spaced form
    `∫^x f(t) dt` is also accepted on input, but the canonical pretty-printed
    form uses `·dt`
  - symbolic integral requests `@S f(x) dx`, `@S^u f(x) dx`,
    `@S^b_a f(x) dx`, and `@S_a^b f(x) dx`; `*`, `.`, or `·` may appear
    before the terminal differential
  - summations `@Z_(k=1)^n f(k)` and products `@P_(k=1)^n f(k)`, whose
    lower-bound parentheses may be omitted and whose Expression output is respectively
    `Σ_(k=1)^n f(k)` and `@P_k=1^n f(k)`; `@Z` is an ASCII replacement for `Σ`,
    while the index is local and is not returned as a binding
  - infinite summations and products omit the upper bound: `@Z_k=1 f(k)` and
    `@P_k=1 f(k)` produce `Σ_(k=1)^∞ f(k)` and `@P_k=1 f(k)` respectively
  - unary operations can opt into finite-progression reduction through their
    expression operation table, rather than through a central list of function
    names. Exact reducers cover `exp`, the circular and hyperbolic sine/cosine
    families, the q-digamma quotient families, all eight versed/haversed
    functions, `abs`, `conj`, `sqrt`, `cubrt`, `normal_logpdf`, and `logpdf`.
    Functions without a genuine general identity remain formal sums
  - the local index may occur anywhere as one factor of the function argument.
    Thus `fn(kax)`, `fn(akx)`, and `fn(axk)` all pass the complete symbolic step
    `ax` to the reducer. The recogniser removes exactly one `k` factor and
    rejects a quotient that still contains the local index, so nonlinear input
    such as `fn(k²ax)` remains formal
  - finite `exp(kx)`, `sin(kx)`, `cos(kx)`, `sinh(kx)` and `cosh(kx)` sums from
    1 to `n` expose their geometric-series closed forms; supplied bindings
    therefore evaluate large upper bounds without term-by-term iteration. The
    exponential form is `exp(x)·(exp(nx) - 1)/(exp(x) - 1)`, with the removable
    `x = 0` value evaluated as `n`
  - the finite logarithmic progression `@Z_(k=1)^n ln(kx)` reduces to
    `n·ln(x) + lnΓ(n + 1)`, so large supplied upper bounds use log-gamma
    rather than term-by-term iteration
  - common-log input remains base ten: `@Z_(k=1)^n log(kx)` reduces to
    `n·lg(x) + lnΓ(n + 1)/ln(10)` rather than using the natural-log formula
  - homogeneous progressions use power-sum identities: `abs(kx)` and
    `conj(kx)` use the triangular number, while `sqrt(kx)` and `cubrt(kx)` use
    the corresponding Riemann-zeta minus Hurwitz-zeta power sum
  - `floor(kx)` and `ceil(kx)` reduce to `x·n·(n + 1)/2` when the supplied
    binding proves that `x` is integral. A proved small reduced rational step
    `x = p/q` instead uses complete periods of length `q` and expands at most
    `q - 1` residue terms; decimal `0.6` is therefore recognised as `3/5`.
    Non-integral rationals currently require `q <= 32` and `|p| <= 1000000`.
    Unary bitwise `not(kx)` similarly uses `-x·n·(n + 1)/2 - n` for an integral
    step. The native result marks these as domain-required specialisations so
    every result card retains the condition; an unset, irrational, or larger
    rational step remains a formal sum
  - `atan(kx)`, `acot(kx)`, `atanh(kx)`, and `acoth(kx)` use finite log-gamma
    identities. Arctangent keeps its real-axis branch explicit through
    `x/abs(x)`, and its derivative reduces to a conjugate pair of digamma
    differences. Other inverse-function progressions remain formal; supplied
    bounds of at most one million terms may still be evaluated directly
  - exact `atanh` and `acoth` progression poles return signed infinity. A
    supplied zero step gives the removable `atan(kx)` value zero, and complex
    digamma evaluation retains the active arbitrary precision
  - `sqrt(x)` or `√(x)` for a single principal square root; `cubrt(x)` and `root(x, n)` for single principal cube
    and integer-order roots
  - `conj(x)` and `conjugate(x)` for complex conjugation, with postfix `x^*` as the equivalent shorthand
  - `ln(x)` for natural logarithm; `log(x)`, `lg(x)`, and `log10(x)` for common logarithm. Result renderings
    use the unambiguous canonical names `ln(x)` and `lg(x)`.
  - `versin(x)`, `vercos(x)`, `coversin(x)`, `covercos(x)`,
    `haversin(x)`, `havercos(x)`, `hacoversin(x)`, and `hacovercos(x)`
    for the versine/haversine family
  - `arcversin(x)`, `arcvercos(x)`, `arccoversin(x)`, `arccovercos(x)`,
    `archaversin(x)`, `archavercos(x)`, `archacoversin(x)`, and
    `archacovercos(x)` for the corresponding inverse functions
  - `gamma(x)` and `Γ(x)` for the gamma function
  - `digamma(x)`, `trigamma(x)`, `polygamma(n, x)`, and `ψ(n, x)` for ψ⁽⁰⁾, ψ⁽¹⁾, and ψ⁽ⁿ⁾
  - `ψq(q, x)` for the q-digamma ψ_q(x), with `qdigamma(q, x)` retained as a word alias; TeX renders the
    base as a subscript
  - `zeta(s)` and `ζ(s)` for Riemann zeta; `zetap(s)` and `ζ'(s)` for its derivative
  - `zeta(s, a)`, `ζ(s, a)`, `zetah(s, a)`, and `zeta2(s, a)` for Hurwitz
    zeta; `zetap(s, a)`, `zatahp(s, a)`, `zeta2p(s, a)`, and `ζ'(s, a)` for its partial
    derivative with respect to `s`
  - `Li1(x)` and `Li₁(x)` for the dedicated order-one polylogarithm;
    `dilog(x)`, `Li2(x)`, and `Li₂(x)` for Li₂(x); and `polylog(n, x)` for Liₙ(x)
  - `lerch_phi(z, s, a)`, `LerchPhi(z, s, a)`, and `Φ(z, s, a)` for the Lerch transcendent
  - `Hn(n, x)` and `harmonic_poly(n, x)` for the finite harmonic polynomial
    Hₙ(x) = Σₖ₌₁ⁿ xᵏ/k
  - `chi(n, x)` and `legendre_chi(n, x)` for the Legendre chi function χₙ(x)
  - `BesselJ(order, x)` or `bessel_j(order, x)` for J_order(x)
  - `BesselY(order, x)` or `bessel_y(order, x)` for Y_order(x)
  - `LommelS(mu, nu, x)` or `lommel_s(mu, nu, x)` for s_mu,nu(x)
  - `pFq(p, q, a1, ..., ap, b1, ..., bq, x)`,
    `HypergeometricpFq(...)`, or `hypergeometric_pFq(...)` for the generalised
    hypergeometric function with `p` upper and `q` lower parameters
  - `LauricellaF(n, a, b1, ..., bn, c, x1, ..., xn)` or
    `lauricella_f(...)` for Lauricella F_D in `n` variables
  - `appell_f1(a, b1, b2, c, x, y)`, `F1(a, b1, b2, c, x, y)`,
    `F_1(a, b1, b2, c, x, y)`, and `F₁(a, b1, b2, c, x, y)` for
    Appell's hypergeometric function F₁(a; b₁, b₂; c; x, y)
  - `W(x)` and `productlog(x)` for branch-choosing Lambert W/ProductLog
  - `W0(x)`, `W_0(x)`, `W₀(x)`, and `lambert_w0(x)` for W₀
  - `W-1(x)`, `W_-1(x)`, `W₋₁(x)`, and `lambert_wm1(x)` for W₋₁
  - exact value-only helpers such as `factorial(n)`, `fibonacci(n)`,
    `partition(n)`, `factors(n)`, `next_prime(n)`, `prev_prime(n)`,
    `AND(a, b)`, `OR(a, b)`, `XOR(a, b)`, `NOT(a)`, `SHL(a, n)`, and
    `SHR(a, n)`
  - `[bracket names]` for identifiers that are not single-letter-plus-subscript

  In the no-binding form, the default inference rule is:
  - constants with built-in values: `e`, `i`, `pi`, `π`, `@pi`, `phi`, `@phi`, `gamma`, `@gamma`, and `@eulermascheroni`.
    Function output uses `@eulermascheroni` for the immortal Euler–Mascheroni constant; the older gamma aliases remain accepted.
  - constant placeholders: `a`, `b`, `c`, `d`, and their indexed forms such as `a₀`, `b_1`, `c₂`, and `d_3`
  - variables: everything else that is a valid symbolic `expr` name, including `x`, `τ`, `@tau`, and bracketed names like `[radius]`

  The built-in-value inference is exact-name only. For example, `@pi` becomes
  the built-in constant `π`, but `@pi1`, `@pi2`, and `@pi_3` normalise to
  `π₁`, `π₂`, and `π₃` and remain ordinary symbolic variables.

  `@S` changes the interpretation from a stored integral node to a symbolic
  integration request:

  ```text
  @S f(x) dx       -> F(x) + C₀
  @S^u f(x) dx     -> F(u)
  @S^5 f(x) dx     -> F(5)
  @S^5_0 f(x) dx   -> F(5) - F(0)
  @S_0^5 f(x) dx   -> F(5) - F(0)
  ```

  The upper-only form evaluates one chosen antiderivative at the supplied
  argument; it does not imply a lower bound of zero. Both two-bound spellings
  denote the same definite integral. The terminal `d<name>` identifies the
  integration variable, except that `di` and `Di` are currently multiplication
  by the built-in imaginary constant `i`, not differentials. Consequently an
  `@S` request cannot use `di` as its terminal differential.

  The terminal differential also establishes the scope of its integration
  variable. In `@S_0^1 f(x) dx`, `x` is bound throughout the integrand and is
  not returned as an input binding. In `@S f(x) dx`, the completed indefinite
  family contains `x`, so `x` is returned as a binding alongside the arbitrary
  constant. A free occurrence outside the integral remains a binding: for
  example, the outer `x` in `exp(x) * @S_0^1 f(x) dx` is not hidden by the
  integral's local `x`.

  Repeated occurrences of the same normalised symbol name within one parsed
  expression resolve to the same underlying leaf node. Reusing the same name as
  both a variable and a constant in one parse is rejected.

  When `bnd_out` is non-NULL, `expr_from_string(...)` returns an opaque
  bindings object. Use `expr_bindings_get(...)` to recover the borrowed `expr_t *`
  leaf for a parsed symbol, then pass that handle to `expr_create_deriv(...)`
  if you want to differentiate with respect to it. Release the bindings
  object later with `expr_bindings_free(...)`. The lookup path uses the same
  name normalisation as parsing, so Greek-style aliases may be queried in
  either form, such as `@pi` or `π`, `@phi` or `φ`, `@gamma` or `γ`, and
  `@tau` or `τ`.

### `expr_bindings_count()`

Returns the public result described by bindings count.

```c
size_t expr_bindings_count(const expr_bindings_t *bnd);
```

### `expr_bindings_expr_at()`

Returns the public result described by bindings expr at.

```c
expr_t *expr_bindings_expr_at(expr_bindings_t *bnd, size_t index);
```

### `expr_bindings_get_text()`

Returns the public result described by bindings get text.

```c
expr_t *expr_bindings_get_text(expr_bindings_t *bnd, const string_t *name);
```

### `expr_bindings_has_symbolic_derivative()`

Reports whether the condition described by bindings has symbolic derivative holds.

```c
bool expr_bindings_has_symbolic_derivative(const expr_bindings_t *bnd);
```

### `expr_bindings_has_symbolic_integral()`

Reports whether the condition described by bindings has symbolic integral holds.

```c
bool expr_bindings_has_symbolic_integral(const expr_bindings_t *bnd);
```

### `expr_bindings_is_constant_at()`

Reports whether the condition described by bindings is constant at holds.

```c
bool expr_bindings_is_constant_at(const expr_bindings_t *bnd, size_t index);
```

### `expr_bindings_name_at()`

Returns the public result described by bindings name at.

```c
const char *expr_bindings_name_at(const expr_bindings_t *bnd, size_t index);
```

### `expr_bindings_name_text_at()`

Returns the public result described by bindings name text at.

```c
const string_t *expr_bindings_name_text_at(const expr_bindings_t *bnd, size_t index);
```

### `expr_cdf()`

Returns the public result described by cdf.

```c
expr_t *expr_cdf(const expr_t *expr);
```

### `expr_clone()`

Creates or reconstructs the public value described by clone.

```c
expr_t *expr_clone(const expr_t *expr);
```

### `expr_contains_integral_operation()`

Reports whether the condition described by contains integral operation holds.

```c
bool expr_contains_integral_operation(const expr_t *expr);
```

### `expr_deserialise()`

Creates or reconstructs the public value described by deserialise.

```c
expr_t *expr_deserialise(const void *data, size_t len, const string_t *type, const string_t *encoding);
```

### `expr_display_expanded()`

Returns the public result described by display expanded.

```c
expr_t *expr_display_expanded(const expr_t *expr);
```

### `expr_display_simplified()`

Returns the public result described by display simplified.

```c
expr_t *expr_display_simplified(const expr_t *expr);
```

### `expr_edit_binding()`

Returns the public result described by edit binding.

```c
expr_t *expr_edit_binding(const expr_t *expr, const expr_bindings_t *bindings, const char *name, const char *value_text, expr_bindings_t **bindings_out);
```

### `expr_from_expression_string()`

Creates or reconstructs the public value described by from expression string.

```c
expr_t *expr_from_expression_string(const char *expr, const char *const *names, expr_t *const *symbols, size_t nsymbols);
```

### `expr_from_expression_text()`

Creates or reconstructs the public value described by from expression text.

```c
expr_t *expr_from_expression_text(const string_t *expr, const string_t *const *names, expr_t *const *symbols, size_t nsymbols);
```

### `expr_from_function_body()`

Creates or reconstructs the public value described by from function body.

```c
expr_t *expr_from_function_body(const char *source, expr_bindings_t **bnd_out);
```

### `expr_from_function_body_text()`

Creates or reconstructs the public value described by from function body text.

```c
expr_t *expr_from_function_body_text(const string_t *text, expr_bindings_t **bnd_out);
```

### `expr_from_text()`

Creates or reconstructs the public value described by from text.

```c
expr_t *expr_from_text(const string_t *text, expr_bindings_t **bnd_out);
```

### `expr_goal_seek()`

Returns the public result described by goal seek.

```c
int expr_goal_seek(expr_t *expr, expr_bindings_t *bindings, number_t target, const expr_goal_seek_options_t *options, expr_goal_seek_result_t *result);
```

### `expr_goal_seek_result_clear()`

Releases or clears the resources associated with goal seek result clear.

```c
void expr_goal_seek_result_clear(expr_goal_seek_result_t *result);
```

### `expr_integral_value_note()`

Reports whether the condition described by integral value note holds.

```c
bool expr_integral_value_note(const expr_t *expr, char *out, size_t out_size);
```

### `expr_integrate_family()`

Returns the public result described by integrate family.

```c
expr_t *expr_integrate_family(const expr_t *expr, const expr_t *wrt);
```

### `expr_is_variable()`

Reports whether the condition described by is variable holds.

```c
bool expr_is_variable(const expr_t *expr);
```

### `expr_lambert_wn()`

Returns the public result described by lambert wn.

```c
expr_t *expr_lambert_wn(const expr_t *branch, const expr_t *expr);
```

### `expr_logpdf()`

Returns the public result described by logpdf.

```c
expr_t *expr_logpdf(const expr_t *expr);
```

### `expr_new_integration_constant()`

Creates or reconstructs the public value described by new integration constant.

```c
expr_t *expr_new_integration_constant(const expr_t *expr, const expr_t *wrt, const expr_t *antiderivative);
```

### `expr_new_named_const_text()`

Creates or reconstructs the public value described by new named const text.

```c
expr_t *expr_new_named_const_text(number_t x, const string_t *name);
```

### `expr_new_named_var_text()`

Creates or reconstructs the public value described by new named var text.

```c
expr_t *expr_new_named_var_text(number_t x, const string_t *name);
```

### `expr_pdf()`

Returns the public result described by pdf.

```c
expr_t *expr_pdf(const expr_t *expr);
```

### `expr_printf()`

Returns the public result described by printf.

```c
int expr_printf(const char *fmt, ...);
```

### `expr_retain()`

Performs the public operation described by retain.

```c
void expr_retain(const expr_t *expr);
```

### `expr_serialize()`

Reports whether the condition described by serialize holds.

```c
bool expr_serialize(const expr_t *expr, string_t **out_type, string_t **out_encoding, void **out_data, size_t *out_len);
```

### `expr_set_name_text()`

Performs the public operation described by set name text.

```c
void expr_set_name_text(expr_t *expr, const string_t *name);
```

### `expr_sprintf()`

Returns the public result described by sprintf.

```c
int expr_sprintf(char *out, size_t out_size, const char *fmt, ...);
```

### `expr_sprintf_text()`

Returns the public result described by sprintf text.

```c
string_t *expr_sprintf_text(const char *fmt, ...);
```

### `expr_substitute()`

Return an owning expression with the requested substitution. The source,
needle and replacement are borrowed. Formal derivative needles are matched
structurally, so a cloned derivative can be replaced without requiring the
same node address. Substituting the dependent variable instead differentiates
its replacement with respect to the derivative's recorded coordinates.

```c
expr_t *expr_substitute(const expr_t *expr, const expr_t *needle, const expr_t *replacement);
```

### `expr_symbol_name()`

Returns the public result described by symbol name.

```c
const char *expr_symbol_name(const expr_t *expr);
```

### `expr_to_TeX_body()`

Returns the public result described by to TeX body.

```c
char *expr_to_TeX_body(const expr_t *expr);
```

### `expr_to_TeX_body_wrapped()`

Returns the public result described by to TeX body wrapped.

```c
char *expr_to_TeX_body_wrapped(const expr_t *expr, size_t line_limit);
```

### `expr_to_function_body()`

Returns the public result described by to function body.

```c
char *expr_to_function_body(const expr_t *expr);
```

### `expr_to_function_body_text()`

Returns the public result described by to function body text.

```c
string_t *expr_to_function_body_text(const expr_t *expr);
```

### `expr_to_string()`

Returns the public result described by to string.

```c
char *expr_to_string(const expr_t *expr, style_t style);
```

### `expr_vsprintf_text()`

Returns the public result described by vsprintf text.

```c
string_t *expr_vsprintf_text(const char *fmt, va_list ap);
```
