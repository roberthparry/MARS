# Integral Transform Syntax

> Design note: this is the agreed target syntax. An initial one-dimensional
> forward and inverse Laplace implementation is available in Expression mode; see
> [the expression guide](../expression.md#laplace-transforms). Native
> one-dimensional Fourier and inverse Fourier operators now implement the subset
> listed below; inferred multidimensional transforms remain planned.

Integral transforms use explicit forward and inverse operators:

```text
@L       Laplace transform input alias
@Linv    inverse Laplace transform input alias
@F       Fourier transform input alias
@Finv    inverse Fourier transform input alias
```

The aliases normalise to the Unicode script capitals `ℒ` (U+2112) and `ℱ`
(U+2131). Their inverse forms use the superscript `⁻¹`.

## Parameters

Only the expression is required. The source variable and transform variable
are optional:

```text
operator(expression [, source variable [, transform variable]])
```

For example:

```text
@L(f(x), x)          explicit source variable, default transform variable s
@L(f(x))             inferred source variable x, default transform variable s
@L(f(t), t)          default transform variable s
@Linv(F(s), s)       default transform variable t
@F(f(t), t)          default transform variable ω
@Finv(F(ω), ω)       default transform variable t
@F(f(x), x)          default transform variable k
@Finv(F(k), k)       default transform variable x
@F(f(y), y)          default transform variable m
@Finv(F(m), m)       default transform variable y
@F(f(z), z)          default transform variable n
@Finv(F(n), n)       default transform variable z
```

When present, the second parameter is the variable eliminated by the
transform. The optional third parameter is the variable introduced by it.
The source must be a symbolic variable. Laplace operators also accept a target
expression independent of the source, allowing evaluation at a shifted argument.

When the source variable is omitted, every free binding classified as a
variable is transformed. Bindings classified as constants are retained as
parameters. Inference uses the expression's binding classification rather than
assuming that every letter is a transform variable:

```text
@L(a*x + b)          transform x; retain a and b as constants
@L(f(x, y, z))       transform x, y, and z
@F(a*f(t) + c)       transform t; retain a and c as constants
```

If inference finds one variable, the operation is one-dimensional. If it
finds several variables, the operation is a simultaneous multidimensional
transform. If it finds no variables, the transform is rejected unless a
source variable is supplied explicitly.

Inferred variables use canonical binding order, not incidental tree-traversal
order, so simplification and term reordering do not change the mapping.

The defaults are:

```text
@L     any source variable → s
@Linv  s → t
@F     t → ω, x → k, y → m, z → n
@Finv  ω → t, k → x, m → y, n → z
```

The default Laplace transform-domain variable is always `s`, irrespective of
the source-variable name:

```text
@L(f(t), t)     equivalent to @L(f(t), t, s)
@L(g(x), x)     equivalent to @L(g(x), x, s)
```

For an inferred multidimensional Laplace transform, distinct numbered
transform variables are allocated in the same order as the inferred source
variables:

```text
@L(f(x, y, z))
```

is equivalent to transforming:

```text
(x, y, z) → (s₁, s₂, s₃)
```

It is therefore a three-dimensional Laplace transform, not an error.

There is no universal Fourier variable convention. Ordinary frequency `f`
normally accompanies a kernel containing `2πf`; angular frequency `ω` is
conventional for a time-domain source variable. The spatial variables `x`,
`y`, and `z` use the corresponding transform variables `k`, `m`, and `n`.
The recognised forward Fourier defaults are therefore:

```text
@F(f(t), t)     equivalent to @F(f(t), t, ω)
@F(f(x), x)     equivalent to @F(f(x), x, k)
@F(f(y), y)     equivalent to @F(f(y), y, m)
@F(f(z), z)     equivalent to @F(f(z), z, n)
```

For a Fourier transform with any other source-variable name, the transform
variable must be given explicitly. The inverse operator applies the
corresponding reverse mappings. An explicit third parameter overrides a
default:

```text
@F(g(q), q, p)
@F(f(t), t, k)
```

Explicit variables avoid guessing when the expression contains several free
variables. Variables not named as the source variable are treated as
parameters:

```text
@L(f(t, x), t, s)
@F(u(x, t), x, k)
```

Here `x` remains free in the Laplace transform and `t` remains free in the
Fourier transform.

## Definitions

The forward Laplace transform is unilateral:

$$
\mathcal{L}\{f(t)\}(s)
  = \int_0^\infty e^{-st} f(t)\,dt
$$

The inverse Laplace transform is the Bromwich integral:

$$
\mathcal{L}^{-1}\{F(s)\}(t)
  = \frac{1}{2\pi i}
    \int_{\gamma-i\infty}^{\gamma+i\infty} e^{st}F(s)\,ds
$$

The Fourier transform uses angular spatial frequency and the convention:

$$
\mathcal{F}\{f(x)\}(k)
  = \int_{-\infty}^{\infty} f(x)e^{-ikx}\,dx
$$

$$
\mathcal{F}^{-1}\{F(k)\}(x)
  = \frac{1}{2\pi}
    \int_{-\infty}^{\infty} F(k)e^{ikx}\,dk
$$

This fixes the sign and normalisation rather than making them implicit
implementation choices.

## Fourier acceptance criteria

Fourier support is partially implemented. The following is the full acceptance
checklist, **not a claim that every row is complete**. Use the
[tables of important Fourier transforms](https://en.wikipedia.org/wiki/Fourier_transform#Tables_of_important_Fourier_transforms)
as the reference inventory. Match their **non-unitary, angular-frequency**
column to the convention above; do not mix entries from different columns.
Check each identity independently, including its hypotheses and distributional
interpretation, before turning it into a simplification rule.

| Coverage area | Acceptance requirement |
| --- | --- |
| Operator interface | Forward and inverse aliases, explicit and inferred variable mappings, bound-variable protection, and native renderings must work consistently. |
| Functional relationships | Test linearity, translation, modulation, real non-zero scaling, duality, conjugation, derivatives, multiplication by powers, convolution and products. Antiderivatives must retain the appropriate integration constant and distributional terms. |
| Ordinary one-dimensional pairs | Cover the reference pulse, sinc, triangular, one-sided exponential, Gaussian, two-sided exponential, hyperbolic-secant and Hermite–Gaussian families in both directions. |
| Distributional one-dimensional pairs | Cover constants, polynomials, impulses and their derivatives, harmonics, chirps, sign and step functions, the impulse train, and the reference Bessel, power and logarithmic families. Principal values and finite parts need explicit mathematical representations. |
| Two-dimensional pairs | Track the reference Gaussian, circular aperture, radial reciprocal and complex reciprocal pairs separately. A one-dimensional interval rule does not establish circular-aperture support. |
| General-dimensional pairs | Track the reference weighted-ball, radial-power, multivariate-Gaussian and radial-exponential families, together with their inverse pairs and dimensional normalisation. |
| Parameters and singularities | Preserve sufficient conditions, distinguish zero scales from non-zero scales, check negative scales and complex parameters where allowed, and handle removable singularities and endpoint conventions explicitly. |
| Simplification and presentation | Equivalent factored and expanded inputs must agree. Generate all Lab result cards from the same native simplified expression; expose any remaining conditions without internal helper names. |

Every reference family needs an explicit status: implemented with tested
conditions, implemented only for a stated subset, or unsupported. An unevaluated
operator is a safe fallback, not successful coverage. Do not describe the
reference inventory as fully implemented while rows or inverse directions remain
unsupported. In particular, completion of the one-dimensional checklist is not
completion of the multidimensional checklist.

For each implemented family, tests must check the forward and inverse formulas
independently, then check round trips. Round trips alone can conceal matching sign
or normalisation errors. Use numerical quadrature where ordinary convergence
allows it; validate distributions through their action on suitable test
functions or independent distributional identities, not pointwise sampling.
Test equivalent input forms, all output styles, parameter specialisation,
variable binding, invalid domains and symbolic fallback. Run the documented
examples after the ordinary tests.

Supporting functions must have documented definitions. In particular, specify
the sinc normalisation, the pulse width, the circular-aperture radius, and values
at discontinuities. Keep arbitrary `u` distinct from the explicitly named unit
step. An impulse is a distribution and must not acquire an invented finite value
at its support. For reciprocal powers and logarithms, specify the regularisation
rather than silently treating a singular formula as an ordinary function.

### Current Fourier coverage

The following pairs use the convention above and real target coordinates.
The reverse-direction rule includes the inverse normalisation $1/(2\pi)$;
it is not obtained by merely renaming the forward operator. Scalar parameters
must satisfy the stated conditions. Signal definitions and checked parser
examples are in the [expression guide](../expression.md#fourier-transforms).

| Source $f(t)$ | Forward transform | Status |
| --- | --- | --- |
| $e^{-at^2+bt+d}$ | $\sqrt{\pi/a}\exp(d-(\omega+ib)^2/(4a))$ | Both directions, $\Re(a)>0$. |
| $\operatorname{rect}(t)$ | $\operatorname{sinc}(\omega/(2\pi))$ | Both directions; real affine arguments. |
| $\operatorname{tri}(t)$ | $\operatorname{sinc}^2(\omega/(2\pi))$ | Both directions, including real affine scaling of sinc-squared. |
| $\operatorname{sinc}(t)$ | $\operatorname{rect}(\omega/(2\pi))$ | Both directions; normalised sinc. |
| $e^{-a\lvert t\rvert}$ | $2a/(a^2+\omega^2)$ | Both directions, $\Re(a)>0$. Quadratic reciprocals also support real translations. |
| $e^{-at}\operatorname{step}(t)$ | $1/(a+i\omega)$ | Both directions, $\Re(a)>0$; reversed gates and imaginary-rate linear reciprocals are also supported. |
| $\operatorname{sech}(t)$ | $\pi\operatorname{sech}(\pi\omega/2)$ | Both directions; real affine arguments. |
| $J_n(t)$ | $2(-i)^n T_{\lvert n\rvert}(\omega)/\sqrt{1-\omega^2}$ for $\lvert\omega\rvert<1$, zero for $\lvert\omega\rvert>1$ | Integer $n$, including negative orders; real affine arguments in both directions. The inverse transform of $J_n$ uses coefficient $i^n/\pi$. Singular support edges have no finite pointwise value. |
| $T_n(t)\operatorname{rect}(t/2)/\sqrt{1-t^2}$ | $\pi(-i)^nJ_n(\omega)$ | Non-negative integral $n$. The inverse coefficient is $i^n/2$. Recognised directly, including real affine arguments. |
| $e^{-a^2t^2/2}\mathcal H_n(at)$ | $\sqrt{2\pi}(-i)^n e^{-\omega^2/(2a^2)}\mathcal H_n(\omega/a)/\lvert a\rvert$ | Non-negative integral $n$, real non-zero $a$; translations also supported. Inverse coefficient $i^n/(\sqrt{2\pi}\lvert a\rvert)$. $\mathcal H_n$ is physicists' Hermite, distinct from harmonic $H_n$. |
| $\delta(t)$ | $1$ | Both directions; real non-zero affine scaling and translations. |
| $1$ | $2\pi\delta(\omega)$ | Both directions, distributionally. |
| $t^n$ | $2\pi i^n\delta^{(n)}(\omega)$ | Symbolic non-negative integral orders; inverse rule $(-i)^n\delta^{(n)}(t)$. |
| $\operatorname{step}(t)$ | $\pi\delta(\omega)+\operatorname{PV}(1/(i\omega))$ | Both directions, retaining the principal value. |
| $\cos(at+b)$, $\sin(at+b)$, $e^{iat+b}$ | Shifted impulses with their phase factors | Both directions for real harmonic rates. |
| $t^n f(t)$ | $i^n\partial_\omega^n\mathcal F\{f\}(\omega)$ | Integral $0\leq n\leq32$; formal derivatives retained when necessary. |
| $f^{(n)}(t)$ | $(i\omega)^n\mathcal F\{f\}(\omega)$ | Known derivative orders and symbolic non-negative integral orders. |
| $\operatorname{circ}(t)$ | $2\operatorname{sinc}(\omega/\pi)$ | One-dimensional interval profile only; **not** the disk/Bessel pair. |

Linearity, unary real affine changes, modulation, conjugation, duality and
whole-line convolution/product identities are implemented in both directions.
The Hermite–Gaussian family supports symbolic degree through the native
script-H polynomial. General antiderivative identities remain unsupported.
Chirps, impulse trains, non-integral-order and second-kind Bessel functions, regularised powers and logarithms,
all listed two-dimensional pairs and all general-dimensional pairs remain
unsupported. These gaps must remain visible until independently tested rules
replace their symbolic fallback.

### Convolution identities

Here $F=\mathcal F(f)$ and $G=\mathcal F(g)$. An asterisk denotes
whole-line convolution; $*_+$ denotes integration from zero to the output
coordinate. Formal identities require the relevant integrals or distributional
operations to exist.

| Direction | Input | Result |
| :--- | :--- | :--- |
| Fourier | $f*g$ | $FG$ |
| Fourier | $fg$ | $(F*G)/(2\pi)$ |
| Inverse Fourier | $F*G$ | $2\pi fg$ |
| Inverse Fourier | $FG$ | $f*g$ |
| Laplace | $f*_+g$ | $\mathcal L(f)\mathcal L(g)$ |
| Inverse Laplace | $AB$ | $\mathcal L^{-1}(A)*_+\mathcal L^{-1}(B)$ |

Use the native expression operators documented under
[Convolutions](../expression.md#convolutions). Known Laplace convergence
half-planes are combined; arbitrary functions retain their formal transforms.
The function-name and polynomial-convention table is in
[Chebyshev and Hermite polynomials](../expression.md#chebyshev-and-hermite-polynomials).

## Implemented forward Laplace transforms

These tables describe the current one-dimensional rules, rather than the
planned syntax above. Each result is $F(s)=\mathcal L\{f(t)\}(s)$.
Conditions are sufficient domains recognised by MARS, not necessarily maximal
regions of convergence. A *known real* parameter is an actual constant in the
expression, not a free binding with a numerical value. Parameters are independent
of $t$. Square roots and logarithms use their principal branches.

See [Laplace function coverage](../expression.md#laplace-function-coverage) for
the complete inventory, unsupported cases and numerical-backend limitations.
The formulas below may be rendered in an algebraically equivalent form.

### Elementary functions

Write $\sigma=\operatorname{Re}(s)$,
$C(a):\sigma>\operatorname{Re}(ia),\ \sigma>\operatorname{Re}(-ia)$, and
$H(a):\sigma>\operatorname{Re}(a),\ \sigma>\operatorname{Re}(-a)$.
For real $a$, these reduce to $\sigma>0$ and
$\sigma>\lvert a\rvert$, respectively.

| Expression function / source $f(t)$ | Transform $F(s)$ | Conditions / supported form |
| --- | --- | --- |
| Zero | $0$ | Every finite $s$ |
| Constant $c$ | $c/s$ | $\sigma>0$; source-independent functions count as constants |
| `pow`, `pow_xp`: $t^\nu$ | $\Gamma(\nu+1)/s^{\nu+1}$ | $\operatorname{Re}(\nu)>-1,\ \sigma>0$ |
| `sqrt`: $\sqrt t$ | $\sqrt\pi/(2s^{3/2})$ | $\sigma>0$ |
| `cubrt`: $t^{1/3}$ | $\Gamma(4/3)/s^{4/3}$ | $\sigma>0$ |
| `root`: $t^{1/n}$ | $\Gamma(1+1/n)/s^{1+1/n}$ | Where reduced to the power rule; $\operatorname{Re}(1/n)>-1,\ \sigma>0$ |
| `exp`, powers of `e`: $e^{at+b}$ | $e^b/(s-a)$ | $\sigma>\operatorname{Re}(a)$ |
| `sin`: $\sin(at+b)$ | $(a\cos b+s\sin b)/(s^2+a^2)$ | $C(a)$ |
| `cos`: $\cos(at+b)$ | $(s\cos b-a\sin b)/(s^2+a^2)$ | $C(a)$ |
| `sinh`: $\sinh(at+b)$ | $(a\cosh b+s\sinh b)/(s^2-a^2)$ | $H(a)$ |
| `cosh`: $\cosh(at+b)$ | $(s\cosh b+a\sinh b)/(s^2-a^2)$ | $H(a)$ |
| `versin`: $1-\cos(at+b)$ | $1/s-\mathcal L\{\cos(at+b)\}(s)$ | $C(a)$ |
| `vercos`: $1+\cos(at+b)$ | $1/s+\mathcal L\{\cos(at+b)\}(s)$ | $C(a)$ |
| `coversin`: $1-\sin(at+b)$ | $1/s-\mathcal L\{\sin(at+b)\}(s)$ | $C(a)$ |
| `covercos`: $1+\sin(at+b)$ | $1/s+\mathcal L\{\sin(at+b)\}(s)$ | $C(a)$ |
| `haversin`, `havercos`, `hacoversin`, `hacovercos` | Half the corresponding preceding transform | $C(a)$ |
| `ln`: $\ln t$ | $-(\ln s+\gamma)/s$ | $\sigma>0$; $\gamma$ is Euler's constant |
| `ln`: $\ln(at)$ | $(\ln a-\ln s-\gamma)/s$ | Known real $a>0,\ \sigma>0$ |
| `ln`: $\ln(at+b)$ | $(\ln b+e^{bs/a}E_1(bs/a))/s$ | Known real $a,b>0,\ \sigma>0$ |
| `log`, `lg`, `log10` | Corresponding natural-log transform divided by $\ln 10$ | Same argument restrictions |
| `abs`: $\lvert at\rvert$ | $\lvert a\rvert/s^2$ | Complex $a$ allowed; $\sigma>0$ |
| `abs`: $\lvert at+b\rvert$, no positive zero | $\epsilon(a/s^2+b/s)$ | Known real $a,b$, $\epsilon=\operatorname{sgn}(b)$ if $b\ne0$, otherwise $\operatorname{sgn}(a)$; $\sigma>0$ |
| `abs`: $\lvert at+b\rvert$, $ab<0$ | $\operatorname{sgn}(b)(a/s^2+b/s)+2\lvert a\rvert e^{sb/a}/s^2$ | Known real $a,b$, positive zero $-b/a$; $\sigma>0$ |
| `conj`: $\overline{at+b}$ | $\overline a/s^2+\overline b/s$ | $\sigma>0$; $s$ is not conjugated |
| `realpart`: $\operatorname{Re}(at+b)$ | $\operatorname{Re}(a)/s^2+\operatorname{Re}(b)/s$ | $\sigma>0$ |
| `floor`: $\lfloor at\rfloor$ | $1/[s(e^{s/a}-1)]$ | Known real $a>0,\ \sigma>0$ |
| `ceil`: $\lceil at\rceil$ | $1/[s(1-e^{-s/a})]$ | Known real $a>0,\ \sigma>0$ |

For a known negative staircase rate $-a$, use
$\mathcal L\{\lfloor-at\rfloor\}=-\mathcal L\{\lceil at\rceil\}$ and
$\mathcal L\{\lceil-at\rceil\}=-\mathcal L\{\lfloor at\rfloor\}$.
The zero-rate staircase transforms are zero.

### Hyperbolic and inverse functions

Here $\psi$ denotes digamma. Define $q=\sqrt{a^2}$ and $z=s/(4q)$;
for known real $a\ne0$, MARS simplifies $q$ to $\lvert a\rvert$.
For the inverse-function rows, put

$$
A(a,s)=\frac{e^{-is/a}E_1(-is/a)-e^{is/a}E_1(is/a)}{2is}
\qquad (a>0).
$$

| Expression function / source $f(t)$ | Transform $F(s)$ | Conditions / supported form |
| --- | --- | --- |
| `tanh`: $\tanh(at)$ | $[2\psi(z+\tfrac12)-\psi(z)-\psi(z+1)]/(4a)$ | $\sigma>0,\ \operatorname{Re}(q)>0$; $a=0$ gives zero |
| `sech`: $\operatorname{sech}(at)$ | $[\psi(z+\tfrac34)-\psi(z+\tfrac14)]/(2q)$ | $\sigma>-\operatorname{Re}(q),\ \operatorname{Re}(q)>0$; known non-real rates are not matched; $a=0$ gives $1/s$ |
| `atan`: $\arctan(at)$ | $\operatorname{sgn}(a)A(\lvert a\rvert,s)$ | Known real $a\ne0,\ \sigma>0$; $a=0$ gives zero |
| `acot`: $\operatorname{arccot}(at)$ | $\pi/(2s)-\operatorname{sgn}(a)A(\lvert a\rvert,s)$ | Known real $a\ne0,\ \sigma>0$; branch $\operatorname{arccot}x=\pi/2-\arctan x$; $a=0$ gives $\pi/(2s)$ |
| `asinh`: $\operatorname{arsinh}(at)$ | $\operatorname{sgn}(a)\pi[\mathbf H_0(s/\lvert a\rvert)-Y_0(s/\lvert a\rvert)]/(2s)$ | Known real $a\ne0,\ \sigma>0$; $a=0$ gives zero |

For `atanh`, put $q=\lvert a\rvert$ and $w=s/q$ (separate from the $z$ above).

| Expression function / source $f(t)$ | Transform $F(s)$ | Conditions / supported form |
| --- | --- | --- |
| `atanh`: $\operatorname{artanh}(at)$ | $[(a/q)(e^{-w}\operatorname{Ei}(w)+e^w E_1(w))+i\pi e^{-w}]/(2s)$ | Real $a\ne0$, including a guarded symbolic rate; $\sigma>0$; zero rate gives zero |

This uses MARS's real-axis boundary convention: for real $x$ with
$\lvert x\rvert>1$, the imaginary part of `atanh(x)` is $+\pi/2$ on both
exterior cuts. The logarithmic singularity at $t=1/\lvert a\rvert$ is locally
integrable. This is a complex-valued ordinary integral, not a principal-value
integral of its derivative. Complex rates and shifted arguments remain symbolic.
The [inverse-hyperbolic branch definitions](https://dlmf.nist.gov/4.37)
explain why specifying the cut boundary matters.

The Struve function in the `asinh` row is represented using the existing
hypergeometric constructor:
$\mathbf H_0(x)=(2x/\pi)\,{}_1F_2(1;3/2,3/2;-x^2/4)$.
The symbolic formula permits complex $s$, but its Bessel-Y factor currently
has no complex numerical backend.

### Gaussian, error and normal functions

Define the two shared expressions

$$
G(A,B,D;s)=\frac{\sqrt\pi}{2\sqrt A}
 e^{D+(s-B)^2/(4A)}
 \operatorname{erfc}\!\left(\frac{s-B}{2\sqrt A}\right),
$$

$$
R(a,b;s)=\frac{a}{\sqrt{a^2}}\,
 e^{s^2/(4a^2)+bs/a}
 \operatorname{erfc}\!\left(\frac{s}{2\sqrt{a^2}}+
                           \frac{b\sqrt{a^2}}{a}\right).
$$

Here $\phi(x)=e^{-x^2/2}/\sqrt{2\pi}$ and
$\Phi(x)=\operatorname{erfc}(-x/\sqrt2)/2$.
Do not replace $\sqrt{a^2}$ by $a$ without the required branch assumption.

| Expression function / source $f(t)$ | Transform $F(s)$ | Conditions / supported form |
| --- | --- | --- |
| `exp`, powers of `e`: $e^{-At^2+Bt+D}$ | $G(A,B,D;s)$ | $\operatorname{Re}(A)>0$; every finite $s$ |
| `erf`: $\operatorname{erf}(at+b)$ | $[\operatorname{erf}(b)+R(a,b;s)]/s$ | $\operatorname{Re}(a^2)>0,\ \sigma>0$ |
| `erfc`: $\operatorname{erfc}(at+b)$ | $[\operatorname{erfc}(b)-R(a,b;s)]/s$ | $\operatorname{Re}(a^2)>0,\ \sigma>0$ |
| `normal_pdf`, `pdf`: $\phi(at+b)$ | $G(a^2/2,-ab,-b^2/2;s)/\sqrt{2\pi}$ | $\operatorname{Re}(a^2)>0$; every finite $s$ |
| `normal_cdf`, `cdf`: $\Phi(at+b)$ | $[\operatorname{erfc}(-b/\sqrt2)-R(-a/\sqrt2,-b/\sqrt2;s)]/(2s)$ | $\operatorname{Re}(a^2)>0,\ \sigma>0$ |
| `normal_logpdf`, `logpdf`: $\ln\phi(at+b)$ | $-a^2/s^3-ab/s^2-[b^2+\ln(2\pi)]/(2s)$ | $\sigma>0$; native quadratic log-density definition |

Zero rates use the constant rule instead of the expressions containing $1/a$.

### Exponential integrals, incomplete gamma and Bessel functions

In the incomplete-gamma rows, write $P_\nu=(a/(s+a))^\nu$.
In the Bessel rows, write $r=\sqrt{s^2+a^2}$ and $u=a/(s+r)$.

| Expression function / source $f(t)$ | Transform $F(s)$ | Conditions / supported form |
| --- | --- | --- |
| `E1`: $E_1(at)$ | $\ln(1+s/a)/s$ | Known positive real $a$, or symbolic $a$ guarded by $\operatorname{Re}(a)>0$; $\sigma>0$ |
| `Ei`: $\operatorname{Ei}(at)$, $a>0$ | $-\ln(s/a-1)/s$ | Known real $a>0,\ \sigma>a$ |
| `Ei`: $\operatorname{Ei}(at)$, $a<0$ | $-\ln(1+s/\lvert a\rvert)/s$ | Known real $a<0,\ \sigma>0$ |
| `gammainc_lower`: $\gamma(\nu,at)$ | $\Gamma(\nu)P_\nu/s$ | $\operatorname{Re}(\nu)>0,\ \sigma>0$; scale as for `E1` |
| `gammainc_upper`: $\Gamma(\nu,at)$ | $\Gamma(\nu)(1-P_\nu)/s$ | Same restrictions |
| `gammainc_P`: $P(\nu,at)$ | $P_\nu/s$ | Same restrictions |
| `gammainc_Q`: $Q(\nu,at)$ | $(1-P_\nu)/s$ | Same restrictions |
| `bessel_j`: $J_\nu(at)$ | $u^\nu/r$ | Known real $a>0,\ \operatorname{Re}(\nu)>-1,\ \sigma>0$ |
| `bessel_j`: $J_{-n}(at)$ | $(-1)^n u^n/r$ | Known positive integer $n$, known real $a>0,\ \sigma>0$ |
| `bessel_j(0,...)`, `J_0`: $J_0(at)$ | $1/\sqrt{s^2+a^2}$ | $C(a)$; this zero-order rule also supports symbolic and complex scales |
| `bessel_y`: $Y_0(at)$ | $-2\ln((s+r)/a)/(\pi r)$ | Known real $a>0,\ \sigma>0$ |

### Clausen functions

MARS uses the sine Fourier series for even Clausen orders and the cosine
series for odd orders. For $1\le p\le32$, put $k=\lfloor(p-1)/2\rfloor$,

$$
D(z)=\frac{\psi(1+iz)+\psi(1-iz)+2\gamma}{2},\qquad
N_p(z)=(-1)^kD(z)+
\sum_{j=1}^{k}(-1)^{j-1}\zeta(2k+3-2j)z^{2k+2-2j}.
$$

The sum is zero when $k=0$.

| Expression function / source $f(t)$ | Transform $F(s)$ | Conditions / supported form |
| --- | --- | --- |
| `clausen2`: $\operatorname{Cl}_2(at)$ | $\operatorname{sgn}(a)D(z)/(\lvert a\rvert z^2)$, $z=s/\lvert a\rvert$ | Known non-zero real $a,\ \sigma>0$ |
| `clausen`: $\operatorname{Cl}_p(at)$ | $N_p(z)/(\lvert a\rvert z^p)$, multiplied by $\operatorname{sgn}(a)$ for even $p$ | Known integer $1\le p\le32$, known non-zero real $a,\ z=s/\lvert a\rvert,\ \sigma>0$ |

### Powers and transform identities

These rules extend the tables; they do not imply support for arbitrary
products or compositions. Write $F(s)=\mathcal L\{f(t)\}(s)$ and
$G(s)=\mathcal L\{g(t)\}(s)$.

| Source | Transform | Conditions / implementation limits |
| --- | --- | --- |
| $f^{(n)}(t)$, input `@L{f^(n)(t)}` | $s^nF(s)-\sum_{k=0}^{n-1}s^{n-1-k}f^{(k)}(0^+)$ | Non-negative integer order; finite right-hand initial limits and the usual derivative-theorem boundary conditions; `f'(t)` and repeated primes are also accepted |
| $\alpha f(t)+\beta g(t)$ | $\alpha F(s)+\beta G(s)$ | Source-independent coefficients; intersection of convergence domains |
| $\int_0^t f(x)\,dx$ | $F(s)/s$ | Locally integrable integrand and a common convergence half-plane |
| $H(t)=\int^t f(x)\,dx$ | $(F(s)+H(0^+))/s$ | Upper-only chosen primitive; finite initial value retained, not assumed zero |
| $\int_a^t f(x)\,dx$ | $(F(s)+\int_a^0 f(x)\,dx)/s$ | Source-independent lower bound and finite initial integral |
| $e^{at+b}f(t)$ | $e^bF(s-a)$ | Shift every convergence condition by $s\mapsto s-a$; unknown $F$ remains symbolic |
| $f(t-a)$, arbitrary $f$ | $e^{-as}[F(s)+\int_{-a}^{0}e^{-sx}f(x)\,dx]$ | Real shift; finite history integral and convergence of the transforms; no causality assumed |
| $16t^2u(t-\tfrac14)$, arbitrary $u$ | $16\frac{d^2}{ds^2}[e^{-s/4}(\mathcal L\{u(t)\}(s)+\int_{-1/4}^{0}e^{-sx}u(x)\,dx)]$ | History retained; convergence sufficient for differentiation; `u` does not denote a unit step automatically |
| $t^n f(t)$ | $(-1)^n F^{(n)}(s)$ | Recognised finite non-negative integer time powers and an available symbolic derivative |
| $\cos^n(at)$ | $2^{-n}\sum_{j=0}^{n}\binom nj\,s/[s^2+(n-2j)^2a^2]$ | Known real rate $a$, non-negative integer $n$, $\sigma>0$; symbolic $n$ retains an integer guard |
| $\sin^n(at)$ | $(2i)^{-n}\sum_{j=0}^{n}(-1)^j\binom nj/[s-i(n-2j)a]$ | Same restrictions |
| $\cosh^n(at+b)$ | $2^{-n}\sum_{j=0}^{n}\binom nj\,e^{(n-2j)b}/[s-(n-2j)a]$ | Known integer $0\le n\le16$; $\sigma>\operatorname{Re}(na),\ \sigma>\operatorname{Re}(-na)$ |
| $\sinh^n(at+b)$ | $2^{-n}\sum_{j=0}^{n}(-1)^j\binom nj\,e^{(n-2j)b}/[s-(n-2j)a]$ | Same restrictions |

Small integer circular powers may instead be returned as rational functions.
Degenerate constant or zero sources are simplified before applying general
convergence guards. Distributional transforms such as the Dirac impulse are
not claimed by these ordinary-integral tables.

## Expression Rendering

`style_EXPRESSION` renders the operators using Unicode script capitals:

```text
ℒ(f(x,y,z))
ℒ(f(t),t)
ℒ⁻¹(F(s),s)
ℱ(f(x),x)
ℱ⁻¹(F(k),k)
ℱ(g(q),q,p)
```

The parser accepts both these canonical forms and the typeable `@L`, `@Linv`,
`@F`, and `@Finv` aliases, so expression-style output remains round-trippable.
If script-letter output is unavailable, the textual fallbacks are
`Laplace(...)`, `InverseLaplace(...)`, `Fourier(...)`, and
`InverseFourier(...)`.

Inferred variables remain omitted; an explicit source variable is preserved.
The third parameter is included only when it is explicit or differs from the
applicable default.

Plain ASCII `L(...)` and `F(...)` are not transform aliases. Reserving them
would make an ordinary variable or user function named `L` or `F` ambiguous;
`F` also appears in established function notation such as `F₁`. The `@`
aliases provide unambiguous ASCII input, while `ℒ(...)` and `ℱ(...)` provide
unambiguous canonical rendering. Bare `L`, `F`, `ℒ`, and `ℱ` remain ordinary
symbols when they are not followed by transform-call syntax.

Transforms may be nested:

```text
ℱ⁻¹(ℱ(f(x),x,k),k,x)
ℒ(ℱ(u(x,t),x,k),t,s)
```

## TeX Rendering

TeX output uses conventional calligraphic operators:

```text
@L(f(t), t, s)
```

renders as:

$$
\mathcal{L}_{t\to s}\{f(t)\}
$$

The remaining operators render as:

$$
\mathcal{L}^{-1}_{s\to t}\{F(s)\}
$$

$$
\mathcal{F}_{x\to k}\{f(x)\}
$$

$$
\mathcal{F}^{-1}_{k\to x}\{F(k)\}
$$

The variable mapping is retained in rendered output because it is part of the
operation, particularly in expressions containing several free variables.

## Evaluation and Simplification

If a transform is known, evaluation returns the transformed expression in the
third parameter. If no applicable rule is known, the transform remains as an
unevaluated symbolic node rather than producing an error.

Forward and inverse transforms cancel only when their variable mappings match
and the mathematical side conditions are satisfied:

```text
@Linv(@L(f(t),t,s),s,t)
@Finv(@F(f(x),x,k),k,x)
```

Convergence conditions, regions of convergence, distributions, branch
choices, and assumptions such as causality must not be silently discarded.
Where the system cannot establish the required conditions, it retains the
unevaluated transform.
