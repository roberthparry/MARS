# Tensor algebra, geometry and a proposed Tensor mode

## Status and direction

This is a design proposal, not an implemented tensor API or an agreed final
expression grammar. The requested scope is a native MARS tensor module capable
of expressing the tensor mathematics of general relativity and electromagnetism.
The user's notes will refine this design before implementation fixes the details.

Use Misner, Thorne and Wheeler's *Gravitation* (MTW) as the principal reference
for geometric meaning, notation and conventions. Support both geometric objects
and indexed calculations. Retain the user's preferred lower-case Latin abstract
indices, with primed indices for component transformations between charts;
Greek indices and legacy barred-variable presentation remain available.
Typography must not determine which indices are spatial or spacetime indices.

The existing [TeX tensor facilities](../TeX.md#general-tensor-and-connection-notation)
construct notation only. They do not supply tensor algebra. The new module will
own the mathematics and provide semantic information to TeX for rendering.
Expressing a field equation, evaluating its residual and finding all its
solutions are separate capabilities. General Einstein or Killing PDE systems
must remain representable even when no supported solver can complete them.

## Mathematical objects and their identities

Start with opaque `tensor_t` values in `include/tensor-t.h`, implemented in
`src/tensor/`, with associated opaque geometry, chart and connection objects.
The public API should offer straightforward constructors, operations and text
conversion. Formatting contexts and semantic adapters remain internal.

The mathematical model needs the following distinctions from the outset:

| Object | Required meaning |
| --- | --- |
| Scalar field | A scalar expression with a domain and coordinate dependencies; rank zero |
| Vector field | A section of the tangent bundle; type `(1,0)` |
| One-form | A section of the dual bundle; type `(0,1)`, independently of a metric |
| General tensor | An ordered collection of upper and lower slots, with their bundle identities |
| Differential form | A fully antisymmetric covariant tensor with a degree and exterior operations |
| Metric | A non-degenerate symmetric bilinear form, its signature and inverse |
| Connection | A differentiation rule on specified bundles, optionally linked to a metric |
| Connection coefficients | Frame-dependent coefficients with an inhomogeneous transformation law |
| Tensor density | A tensor with an explicit density weight and transformation rule |
| Chart and frame | Coordinates or basis fields, their domains, transitions and dual bases |

An abstract tensor must not require a component array, a coordinate system or a
metric. Components are a representation in a specified frame, not the identity
of the tensor. A matrix can provide component storage without becoming a tensor
merely because it is rectangular. Unknown components are distinct from components
proved to be zero. A zero tensor retains its slot types and domain.

Indices carry stable identities, variance and bundle membership. Component
indices additionally identify a frame and index range. Preserve horizontal slot
order even when upper and lower indices alternate. Rank and dimension are not
limited to Cartesian vectors or four-dimensional spacetime.

Distinguish an abstract index from a numerical component selector. A prime on a
component index can identify a different chart; it is not differentiation.
Abstract indices are independent of charts. Future primed spinor indices would
denote different bundle types, not chart changes, so these concepts must never
share an accidental string-based implementation.

## MTW convention profile

Provide a named MTW profile and record its complete conventions in every
geometry. Check the profile against the chosen edition of *Gravitation* before
freezing the API. In particular, the default four-dimensional Lorentzian metric
has signature `(-,+,+,+)`. General dimensions and other signatures remain
explicit choices, rather than global switches.

Fix curvature by an equation, not just a sign label. For a torsion-free
connection in a coordinate frame, use

$$
(\nabla_c\nabla_d-\nabla_d\nabla_c)V^a=R^a{}_{bcd}V^b,
\qquad R_{bd}=R^a{}_{bad},
\qquad G_{ab}=R_{ab}-\tfrac12 Rg_{ab}.
$$

Equivalently, with $\nabla_c e_b=\Gamma^a{}_{bc}e_a$ in that frame,

$$
R^a{}_{bcd}=\partial_c\Gamma^a{}_{bd}-\partial_d\Gamma^a{}_{bc}
+\Gamma^a{}_{ec}\Gamma^e{}_{bd}-\Gamma^a{}_{ed}\Gamma^e{}_{bc}.
$$

The profile also specifies symmetrisation and antisymmetrisation weights,
orientation, the volume form, Hodge-dual signs, electromagnetic normalisation,
and units. Normalised index brackets include the appropriate factorial divisor.
In geometric gravitational units, `G = c = 1`; retain explicit constants when
another unit system is requested. Do not conflate that choice with setting an
electromagnetic coupling to one.

The MTW electromagnetic acceptance case uses Gaussian normalisation with `c = 1`:
$\nabla_b F^{ab}=4\pi J^a$. The position of the contracted index and the sign
convention for $F=dA$ belong together. Interchanging the two indices of an
antisymmetric field changes a sign, not merely its appearance. The
[MIT exercise referring to MTW Box 16.1](https://dspace.mit.edu/bitstream/handle/1721.1/36859/8-962Spring2002/NR/rdonlyres/Physics/8-962Spring2002/AE0D7E40-C0E2-44E4-AE6A-DA3F4E58530B/0/ps7.pdf)
provides a useful independent check of these equations. SI and other profiles
need their own complete definitions and tests.

## Algebra and summation

Implement addition, scalar multiplication, tensor and outer products,
contraction, traces, permutations, explicit raising and lowering, and metric
inner products. Pairing a one-form with a vector needs no metric. The inner
product of two vectors does. A general tensor product does not automatically
mean a matrix product; distinguish contraction from component-wise operations.
Preserve the slot order of an outer product. An indefinite metric does not
define a positive norm, and a non-zero null vector cannot be normalised by
dividing by its length. Complex conjugation and Hermitian pairing are explicit
operations, not implicit changes to the spacetime metric pairing.

Einstein summation is scoped to each additive term:

- A dummy index occurs exactly twice, once upper and once lower, in compatible
  bundles. More occurrences, or two occurrences with the same variance, require
  an explicit operation or a diagnostic. Never assume a Euclidean metric.
  Component contractions also require compatible frames; transform components
  before combining values expressed in different frames.
- Free indices must match in variance, identity and bundle on every summand and
  on both sides of an equation. Addition does not silently permute slots.
- Capture-free dummy renaming must survive substitution, products, derivatives
  and nested explicit sums. A scalar coordinate named `a` is not the dummy index
  named `a`.
- Explicit component summation and abstract contraction are separate nodes.
  Dimension-dependent identities require a known dimension.
- An index change from lower to upper is an actual metric operation. A requested
  variance view may perform that operation using an explicitly selected metric;
  without one, report the missing metric. Never just move the printed label.

Store tensor symmetries as signed permutation generators. Include symmetric and
antisymmetric blocks, pair interchange, and separate multi-term identities such
as the algebraic Bianchi identity. Permutation symmetries alone do not encode
every Riemann identity. Canonicalisation should recognise dummy-renamed terms,
combine coefficients and detect symmetry-forced zero expressions.
The [Butler–Portugal canonicalisation paper](https://arxiv.org/abs/0803.0862)
is an algorithmic reference; investigate the algorithm without adding a foreign
runtime dependency or assuming its implementation has bounded cost in all cases.

## Differential operations

Support the following operations as mathematical nodes before requiring every
one of them to have an explicit component reduction:

| Operation | Required behaviour |
| --- | --- |
| Scalar differential | `df` is a one-form; it needs no metric |
| Gradient | Raise the differential with the selected inverse metric |
| Partial derivative | Differentiate components in a named chart; it is not generally a tensor derivative |
| Covariant derivative | Act on every tensor slot with the appropriate connection terms and signs |
| Directional derivative | Contract a derivative with its direction vector |
| Lie derivative | Act along a vector field, independently of a metric or affine connection |
| Divergence | Contract the appropriate covariant-derivative slot |
| Exterior derivative | Act on forms, with the correct antisymmetry and frame structure terms |
| Interior product and wedge | Implement contraction with a form and the graded exterior product |
| Hodge dual and codifferential | Require a metric and orientation, preserving signature-dependent signs |
| Laplace–Beltrami and wave operators | Make the metric contraction and scalar/tensor domain explicit |

Include the Lie bracket of vector fields and Cartan's identity on forms,
$\mathcal L_X=i_Xd+di_X$. For a `p`-form in dimension `n` with `s` negative
metric directions, test the Hodge convention
$\star\star\omega=(-1)^{p(n-p)+s}\omega$. Keep the volume tensor distinct from
the alternating permutation symbol: its components involve the metric
determinant and the chosen orientation. Neither can be replaced by a hard-coded
Cartesian epsilon array in a curved chart.

For example, the covariant derivative must distinguish a vector from a one-form:

$$
V^a{}_{;b}=\partial_b V^a+\Gamma^a{}_{cb}V^c,
\qquad
\omega_{a;b}=\partial_b\omega_a-\Gamma^c{}_{ab}\omega_c.
$$

Retain the user's comma and semicolon component notation as presentation options.
Appending derivative indices records their order: $V^a{}_{;b;c}$ means
$\nabla_c(\nabla_b V)^a$, including the connection action on the first derivative's
new lower slot. Do not commute derivatives without the necessary smoothness,
curvature and torsion assumptions. The
[SageManifolds connection documentation](https://doc.sagemath.org/html/en/reference/manifolds/sage/manifolds/differentiable/levi_civita_connection.html)
provides another reference for the appended derivative-slot convention.

Levi–Civita is the convenient default when a metric is supplied, but a connection
is not intrinsically torsion-free or metric-compatible. Keep torsion,
non-metricity, frame commutators and connections on additional bundles
representable. Coordinate formulas may only be applied to coordinate frames;
an orthonormal tetrad in curved spacetime generally requires non-coordinate
frame terms.

## Geometry, relativity and field equations

Calculate metric inverses, Christoffel symbols of both kinds, Riemann and Ricci
curvature, scalar curvature, Einstein and Weyl tensors, curvature invariants,
and algebraic and differential Bianchi identities. Retain dimensional and
connection assumptions. A singular matrix is not an invertible metric, and a
singular chart boundary is not automatically a curvature singularity.

Connection coefficients must not be registered as ordinary tensors. Their
coordinate-transformation rule includes second derivatives of the transition;
the difference of two connections on the same bundle is tensorial. Intermediate
coordinate expressions containing coefficients and partial derivatives need
their own covariance classification, even when their combination is a tensor.
Checking free indices alone cannot certify a transformation law.

Include parallel transport, geodesic equations and geodesic deviation. For a
candidate Killing vector $K$, compute $\mathcal L_Kg$ and the equivalent
Levi–Civita residual $\nabla_aK_b+\nabla_bK_a$. Provide conformal Killing
residuals and conserved contractions along affine geodesics. A Killing vector
need not be covariantly constant. Constructing or verifying a residual is distinct
from solving its entire PDE system. Higher Killing tensors and Killing–Yano
forms are natural later extensions of the same model.

Electromagnetism must include the potential one-form, antisymmetric field
two-form, gauge transformations, both Maxwell equations, current conservation,
the electromagnetic stress–energy tensor and the Lorentz force. For a smooth
potential, construct $F=dA$ and verify $dF=0$. In a Levi–Civita coordinate frame,
the MTW-profile component equations are

$$
F^{ab}{}_{;b}=4\pi J^a,
\qquad F_{ab;c}+F_{bc;a}+F_{ca;b}=0.
$$

The sourced differential-form equation must be derived from the profile's Hodge,
current and orientation definitions; do not borrow its sign from a different
source convention. Distinguish the current vector, its metric-dual one-form and
the current three-form. Converting exterior derivatives to covariant derivatives
must include torsion terms when the connection has torsion.

Build Einstein's equation with cosmological constant, stress–energy conservation
and matter-field residuals using the same operations:

$$G_{ab}+\Lambda g_{ab}=8\pi T_{ab}$$

in geometric gravitational units. Later work can add action variation,
perturbations, hypersurface geometry, extrinsic curvature and ADM decompositions.
Keep room for spinor bundles and tensor-valued forms without pretending that
spinors or all quantum operators are ordinary spacetime tensors.

## Proposed native expression syntax

Use the distinct ASCII indexed form `T[^a, _b]`, preserving the distinction
between tensor indices, scalar powers and subscripted symbol names. Use MTW
notation for mathematical output. The working derivative syntax uses a comma
after the closing bracket for partial differentiation and a semicolon for
covariant differentiation. This remains a provisional design until the complete
grammar has been checked against existing input forms.
The following are proposed input/output contracts, not currently runnable input.
The geometry declares tensor names, slot types, index families, charts, metric,
connection and assumptions before these expressions are parsed.

| Proposed native input | Expected mathematical meaning or result |
| --- | --- |
| `V[^a]` | $V^a$; a vector with free upper index `a` |
| `omega[_a]` | $\omega_a$; a one-form with free lower index `a` |
| `T[^a, _b, ^c, _d]` | $T^a{}_b{}^c{}_d$; preserve the four ordered slots |
| `omega[_a] * V[^a]` | $\omega_aV^a$; scalar pairing by Einstein contraction |
| `V[^a] * W[^b]` | $V^aW^b$; an outer product with two free slots |
| `g[_a, _b] * V[^a] * W[^b]` | $g_{ab}V^aW^b$; metric inner product |
| `T[^a, _a]` | $T^a{}_a$; a scalar trace |
| `V[^a],b` | $V^a{}_{,b}$ in the selected coordinate chart |
| `V[^a];b` | $V^a{}_{;b}$ for the selected connection |
| `omega[_a];b` | $\omega_{a;b}$; the one-form connection sign |
| `[X, Y]` | $[X,Y]$; the Lie bracket in the declared algebra |
| `[X, Y][^a]` | $[X,Y]^a$; the vector-field bracket with a free upper index |
| `gradient(f)` | $g^{ab}\partial_bf$ as a vector field, with output slot chosen explicitly when indexed |
| `lie(K, g)` | $\mathcal L_Kg$; the Killing residual before simplification |
| `exterior(A)` | $dA$; a two-form when `A` is a one-form |
| `wedge(alpha, beta)` | $\alpha\wedge\beta$ with the appropriate form degree |
| `hodge(F)` | $\star F$ under the selected metric and orientation |
| `F[^a, ^b];b = 4*@pi*J[^a]` | The sourced MTW-profile Maxwell equation; only `a` is free |
| `Einstein[_a, _b] + @Lambda*g[_a, _b] = 8*@pi*Stress[_a, _b]` | Einstein's equation in geometric units |

Comma and semicolon derivatives are distinct semantic operations. Explicit
function forms can select a chart or connection when more than one is available,
but ordinary input need not spell out `partial` or `covariant`.
In a geometry with no default connection, the short covariant form is an error,
not a request to guess one. Whole-field operations such as `gradient` produce
typed fields; indexing their result chooses its free index labels.

Postfix derivatives bind to the preceding indexed operand before multiplication,
addition or a relation. Successive derivative marks preserve their authored
order. The grammar must specify grouping where a comma can also separate
function arguments, bindings or language statements; an ambiguous comma must
not silently change the number of arguments or the derivative's operand.

Postfix brackets select tensor slots on an existing object; standalone brackets
can construct a Lie bracket. Its algebra is declared: a vector-field bracket is
the Lie derivative of its second field along its first, whereas a matrix
commutator requires the appropriate associative algebra. These operations need
semantic nodes, not just notation that resembles a bracket.

Standalone `[X, Y]` may collide with existing array literals or bracketed symbol
names. Before freezing the grammar, establish how a tensor-input context or an
explicit disambiguating form preserves those existing meanings. Do not infer
the role from numerical operand values, reinterpret old scalar input, or claim
that separating postfix and standalone brackets alone resolves this collision.

### Mixed-tensor coordinate transformation

`T[^a', _b']` can display the transformed components $T^{a'}{}_{b'}$, but
only after the geometry associates those labels with a target chart and a
transition. Declare `x` as a coordinate collection, with unprimed labels in the
source chart and primed labels in the target chart. The proposed native input is

```text
T[^a', _b'] = x[^a'],c * x[^d],b' * T[^c, _d]
```

Expected mathematical output:

$$T^{a'}{}_{b'}=
\frac{\partial x^{a'}}{\partial x^c}
\frac{\partial x^d}{\partial x^{b'}}T^c{}_d.$$

The first derivative is the forward Jacobian; the second is its inverse.
The unprimed indices `c` and `d` are summed, while `a'` and `b'` are free.
Each upper tensor slot receives a forward Jacobian and each lower slot an
inverse Jacobian. No metric is needed for a coordinate transformation.

Coordinates written with upper labels are coordinate functions, not components
of a contravariant vector. The parser must retain that distinction. Jacobians
carry source and target frame identities, and both factors are evaluated at the
same geometric point. An inverse Jacobian can be obtained by matrix inversion
on a domain where the forward Jacobian is non-singular; do not assume a global
inverse coordinate map exists. Priming a label alone neither differentiates
the tensor nor supplies the transition.

Keep tensor notation scoped to declared geometric objects. Existing scalar
subscript names, quoted symbol names, array bindings, matrix syntax, powers,
prime derivatives and `@` aliases must retain their existing meanings.
Bracketed slots keep tensor indices distinct from scalar powers and subscripted
symbol names. Index labels are complete tokens, so `_ab` must not silently mean
the two indices `_a, _b`. Support Greek aliases such as `^@mu` without requiring
a Unicode keyboard.

The table uses the existing native expression multiplication `*`. The future
[Ophelia language](./expression-language.md#source-and-presentation) reserves
`*` for convolution and uses `.` for multiplication. Its adapter must preserve
that decision; tensor input must not silently redefine either language.
Explicit operations such as `outer` and `contract` offer an unambiguous route
where the surrounding language has different operator meanings.

## Native architecture and performance

The tensor module owns geometry, indexed expression graphs, index validation,
contraction, canonicalisation and differentiation. Reuse `expr_t` for scalar
coefficients and component functions, `number_t` for numerical values, and
`matrix_t` where matrix operations are appropriate. Preserve exact arithmetic
and the selected precision; do not convert component calculations to `double`.

Expression and Ophelia parsers should dispatch declared tensor forms through a
typed native adapter. Keep the scalar expression DAG scalar; mixed expressions
need a checked typed layer rather than undocumented tensor payloads in scalar
nodes. Equation and differential-equation modules consume typed relations or
explicit component equations, without inspecting tensor layouts. Tensor
relations carry their free-index signature and geometry through solving.

TeX receives semantic tensor slots, derivatives and relations through the shared
notation interface. It alone owns TeX syntax. The same mathematical result
supplies Expression, Function and TeX output. Users must not construct a TeX
context to print a tensor, and the TeX RPN builder must not acquire an algebra
engine. A native tensor builder could offer RPN authoring later, but it is a
separate mathematical API rather than an extension of the typesetting stack.

Use dictionary-indexed symbol, bundle, chart and canonical-term identities,
direct slot access, sparse components and immutable shared subgraphs. Cache
derivatives and curvature using keys that include the metric, connection,
chart, assumptions, conventions and precision where relevant. Changing those
inputs must not reuse a stale result. Avoid factorial permutation enumeration
and eager expansion into every component. Component contractions still require
real mathematical work; report and budget it rather than hiding it behind a
lookup table.

All expensive operations use the computation module's cancellation, deadlines,
work limits and progress facilities. Results distinguish a completed reduction,
an unchanged valid expression, unsupported work and an interrupted calculation.
Do not label an expression zero merely because a reduction exhausted its budget.
Owned results must retain or snapshot their geometry safely; concurrent readers
must not observe mutable assumptions or shared numerical bindings changing.

## Proposed MARS Lab Tensor mode

Add a dedicated Tensor mode once the native representation is usable. It should
be possible to start with an abstract expression without first entering a metric.

- **Geometry:** manifold dimension, declared index families, optional chart and
  frame, metric components, connection, MTW convention profile and assumptions.
- **Input:** a tensor expression or relation, declared tensors and symmetries,
  with named actions for contraction, derivatives, curvature and residuals.
- **Results:** rendered TeX, native expression, optional component table,
  free-index/type summary, conditions and requested derivation steps. A scalar
  numerical value appears only when one exists.
- **Geometry actions:** construct Levi–Civita coefficients, curvature, Killing
  residuals, geodesic equations and Maxwell/Einstein component systems using
  native operations, with the assumptions displayed alongside their results.

The browser only gathers input, requests native work and displays native
results. Use asynchronous workers, cancellation and stale-response protection;
expensive canonicalisation must not lock the controls or replace a newer result.
Local native and WASM builds should share the mathematical implementation.

Reuse the existing per-browser, per-mode bounded history mechanism. Persist the
geometry declarations, selected conventions, expression, assumptions and relevant
UI state as versioned reconstructible data. Never store native pointers or depend
on a process-global default geometry. Restoring a history entry restores the
mathematics needed to reproduce it, not just its rendered formula.

## Delivery and acceptance

Implement in independently useful stages, with ordinary tests first and matching
README examples last. Every syntax example above is a future acceptance fixture;
turn it into an executable test when its parser and operation land. Do not
present unimplemented design examples as successful runtime tests.

1. **Typed algebra:** vectors, one-forms, arbitrary slots, pairing, outer products,
   contraction, free-index validation and capture-free dummy renaming.
2. **Symmetry and representation:** permutation canonicalisation, sparse and
   lazy components, charts, Jacobians, metric duals and coordinate changes.
3. **Connections and curvature:** Levi–Civita, ordered covariant derivatives,
   general connection metadata, Riemann/Ricci/Einstein/Weyl and Bianchi checks.
4. **Forms and frames:** exterior calculus, Hodge operations, Lie derivatives,
   tetrads, non-coordinate frames and their structure coefficients.
5. **Relativity and electromagnetism:** Killing verification and supported
   searches, transport, geodesics, Einstein–Maxwell residuals and component PDEs.
6. **Lab integration:** the dedicated mode, reusable geometry input, responsive
   native operations and reproducible bounded history. A minimal abstract-algebra
   interface can precede the complete geometry controls.
7. **Further geometry:** action variations, perturbations, hypersurfaces, ADM
   decompositions and separately designed spinor/gauge-bundle extensions.

Required independent reference cases include:

| Geometry or operation | Expected result |
| --- | --- |
| Flat spacetime in Cartesian coordinates | Zero Levi–Civita coefficients and curvature |
| Flat plane in polar coordinates, `g = diag(1,r^2)` | Non-zero coefficients, including $\Gamma^r{}_{\theta\theta}=-r$, but zero curvature |
| Sphere of radius `L` | Scalar curvature $2/L^2$ under the stated curvature convention |
| Schwarzschild exterior in geometric units | Zero Ricci tensor; Kretschmann scalar $48M^2/r^6$ |
| A polar rotation vector field | Zero Killing residual, without requiring a zero covariant derivative |
| Four-dimensional Minkowski Killing fields | Ten independent generators, with their Lie brackets closing |
| An exact electromagnetic two-form `F = dA` | Zero exterior derivative on a smooth coordinate domain |
| A Maxwell field satisfying its sourced equation | Current-conservation residual zero under the matching assumptions |
| A mixed tensor under a non-linear chart transition | Forward and inverse Jacobian factors, with invariant scalar contractions |
| Connection coefficients under that transition | The required inhomogeneous term; ordinary tensor transformation must fail |
| Symmetric/antisymmetric contracted slots | Zero, with remaining tensor type retained |

Also test invalid repeated indices, incompatible free indices, missing metrics,
non-invertible transitions, coordinate-domain exclusions, torsion and orientation
changes, non-commuting covariant derivatives, high-precision component values,
canonicalisation limits, cancellation, notation round trips and ownership.
Do not restrict correctness checks to identities that the same implementation
used to construct its output.

## Decisions to revisit with the user's notes

Confirm the ASCII grammar and declaration syntax; the preferred balance between
whole-object and indexed input; default component, tetrad and abstract-index
presentations; electromagnetic units and dual conventions; which symmetry
solvers to implement first; and whether spinors, perturbations or action
variation belong in the first substantial release. MTW is the notation baseline;
the final public contract should remain small enough to use without setting up
typesetting machinery or learning internal tensor representations.
