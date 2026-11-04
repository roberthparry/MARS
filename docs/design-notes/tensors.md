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

The concrete proposed Lab workflows are recorded in:

- [Coordinate transformations](#first-useful-workflow-change-coordinates),
  including custom toroidal coordinates and worked component transformations.
- [Line elements and field equations](#line-elements-field-equations-and-solutions),
  including the distinction between changing coordinates and solving for a metric.
- [Physical predictions](#physical-predictions-from-a-supplied-metric), including
  Mercury's orbit, Doppler shifts, clock comparisons and length measurements.

Their solver requirements are also recorded in the
[differential-equation design notes](./differential-equations.md#tensor-and-geometric-systems).

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
${}^{\star}({}^{\star}\omega)=(-1)^{p(n-p)+s}\omega$. Display each Hodge star as a
left superscript. Keep the volume tensor distinct from
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
| `hodge(F)` | ${}^{\star}F$ under the selected metric and orientation |
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

### First useful workflow: change coordinates

Make **Change coordinates** a named operation with a small structured form. A
user supplies two charts, their coordinate relationship, the tensor's ordered
slot types and its source components. MARS constructs the Jacobians and applies
the transformation. Users need not write the indexed transformation law or
declare a metric for an ordinary tensor transformation.

This is a proposed interface and a set of future acceptance examples, not
currently supported Lab input or a final declaration grammar. Coordinate
transformation, tensor simplification, curvature calculation and verification of
supplied fields can make the initial mode useful before coupled differential
equations can be solved. Geodesic and transport solving depend on coupled ODE
support; more general Maxwell, Killing and Einstein systems need separately
supported PDE methods. Keep derived equations, verified candidates and solved
systems distinct in the result cards.

#### Problem entry and reusable transformations

The form should contain:

- **Source and target charts:** ordered coordinate names and optional descriptive
  names. The coordinate lists determine the dimension and component ordering.
- **Coordinate relations:** equations expressing either the source coordinates
  in terms of the target coordinates or the target coordinates in terms of the
  source coordinates. Label the supplied direction explicitly.
- **Object and slots:** a vector, one-form, metric or general tensor, with ordered
  upper and lower slots such as `V[^a]`, `omega[_a]` or `T[^a, _b]`.
- **Source components and frame:** a component table attached to the source
  coordinate basis by default. Use a matrix grid for two slots and indexed
  component selection for higher ranks. Unknown entries remain distinct from
  zero entries; declared symmetries can reduce the entries required.
- **Domain and assumptions:** coordinate ranges, angular branches, excluded
  singularities and assumptions needed for simplification or invertibility.
- **Action:** transform to the selected target chart, with optional derivation
  steps and numerical evaluation at a supplied point.

Provide Cartesian, polar, cylindrical and spherical presets that populate the
coordinate relations and their domain restrictions. Custom charts use the same
form. A saved transformation should be reusable for several tensors without
re-entering its equations. Changing a tensor's variance must change its actual
type and transformation rule, not merely its displayed index position.

The initial workflow uses coordinate bases. An orthonormal-frame option would
add an explicit frame transformation and its geometric data; selecting polar
coordinates alone must not silently select unit radial and angular basis
vectors. Label the chart and frame on input, component results and copied output.

#### Worked vector example: Cartesian to polar

Proposed problem fields:

| Field | Input |
| --- | --- |
| Source chart | Cartesian, ordered coordinates `x, y` |
| Target chart | Polar, ordered coordinates `r, phi` |
| Coordinate relations | `x = r*cos(phi)` and `y = r*sin(phi)` |
| Relation direction | Source coordinates expressed in target coordinates |
| Object | Vector `V[^a]` |
| Source components | $V^x=-y$, $V^y=x$ |
| Source and target frames | Their respective coordinate bases |
| Domain | $r>0$, $-\pi<\phi<\pi$; this branch excludes the origin and the negative Cartesian $x$-axis |
| Action | Transform to polar coordinates |

The authored source field is

$$
V=-y\,\partial_x+x\,\partial_y.
$$

Expected result, labelled **Polar coordinate-basis components**:

$$
V^r=0,\qquad V^\phi=1,\qquad V=\partial_\phi.
$$

For comparison, if the Euclidean metric and the orthonormal polar frame are
explicitly selected, the expected components are $(0,r)$. The two component
lists represent the same field in different bases. The
[SageMath coordinate and frame tutorial](https://doc.sagemath.org/html/en/thematic_tutorials/vector_calculus/vector_calc_change.html)
provides an independent reference for making these distinctions visible.

#### Reuse for one-forms and metrics

With the same coordinate relations, enter the one-form `omega[_a]` with
Cartesian components $\omega_x=-y$, $\omega_y=x$. Its expected output is

$$
\omega=-y\,dx+x\,dy=r^2\,d\phi,
\qquad \omega_r=0,\quad \omega_\phi=r^2.
$$

This deliberately uses the same source component values as the vector example:
the different slot variance requires a different transformation.

Select the metric `g[_a, _b]` and supply its Cartesian component matrix

$$
g_{\mathrm{Cartesian}}=
\begin{pmatrix}1&0\\0&1\end{pmatrix}.
$$

Expected polar coordinate-basis output:

$$
g_{\mathrm{polar}}=
\begin{pmatrix}1&0\\0&r^2\end{pmatrix},
\qquad ds^2=dr^2+r^2\,d\phi^2.
$$

The general tensor workflow uses the same chart relationship and an arbitrary
ordered slot declaration. Each upper slot receives a forward Jacobian and each
lower slot an inverse Jacobian, as specified in the
[mixed-tensor transformation](#mixed-tensor-coordinate-transformation).

#### Custom chart example: toroidal coordinates

The **Custom coordinates** option accepts coordinate relations and parameter
domains without requiring a built-in name for the chart. For example, take
Cartesian source coordinates $(t,x,y,z)$, toroidal target coordinates
$(t,u,v,\phi)$, unchanged time, a constant scale $a>0$ and the relations

$$
D=\cosh v-\cos u,\qquad
x=\frac{a\sinh v\cos\phi}{D},\quad
y=\frac{a\sinh v\sin\phi}{D},\quad
z=\frac{a\sin u}{D}.
$$

Here $v>0$ and $u,\phi$ have explicitly chosen open angular branches, for example
$(0,2\pi)$. The focal circle, axis and chart seams require other patches when
they are part of the requested domain. This follows the coordinate ordering in
the [MathWorld toroidal-coordinate reference](https://mathworld.wolfram.com/ToroidalCoordinates.html).

With flat spacetime as the supplied source geometry, the expected transformed
line element is

$$
ds^2=-dt^2+\frac{a^2}{D^2}
\left(du^2+dv^2+\sinh^2v\,d\phi^2\right).
$$

The connection coefficients need not vanish, while the Riemann tensor remains
zero. MARS should derive these results from the coordinate relations and source
metric. A custom chart alone supplies neither a metric nor axial symmetry,
stationarity or a matter model. For an unknown geometry, the user instead
supplies unknown metric components or a declared metric ansatz in that chart.

#### Native calculation and result cards

When source coordinates are supplied as functions of target coordinates,
differentiate those functions to obtain

$$
K^a{}_{b'}=\frac{\partial x^a}{\partial x^{b'}},
\qquad J^{a'}{}_b=(K^{-1})^{a'}{}_b.
$$

Substitute the supplied coordinate relations into the source component
functions and apply the appropriate factors of $J$ and $K$. In the polar
example, $\det K=r$, so the specified domain permits inversion of the Jacobian
matrix without solving explicitly for an inverse angle function. Matrix
inversion supplies the local inverse differential; it does not establish a
global inverse coordinate map. Where the supplied direction does not allow
component functions to be expressed in target coordinates, request inverse
relations or display the remaining source-coordinate dependencies explicitly.

The native result should supply the transformed components, their rendered
tensor expression, chart and frame labels, domain restrictions and, when
requested, both Jacobians, determinant and transformation steps. All tensor
slots and Jacobian factors must refer to the same geometric point. The browser
displays these results without deriving matrices, substituting expressions or
simplifying mathematics itself.

History entries retain both chart declarations, the relation direction and
equations, domain, source tensor and frame, selected target frame and requested
action. Restoring the entry must reproduce the transformation independently of
later edits to another problem's charts or tensors.

### Line elements, field equations and solutions

The expression $ds^2=g_{ab}\,dx^a dx^b$ specifies a metric. Its role depends on
the requested operation:

| Requested operation | Supplied information | Result |
| --- | --- | --- |
| Change coordinates | Known metric and chart transition | The same geometry expressed in another chart |
| Expand field equations | Metric with unknown functions, connection choice and tensor equations | A component differential-equation system with its assumptions and constraints |
| Verify a proposed solution | Candidate metric, matter fields and governing equations | Simplified residuals and the domain on which they vanish |
| Solve for a metric | Unknown metric functions, matter model, equations and appropriate conditions | Supported solution branches, their conditions and residual verification |

Thus the flat toroidal line element above is a coordinate-transformation result.
It also satisfies the vacuum Einstein equations when $T_{ab}=0$ and
$\Lambda=0$, because the source geometry was already flat. Changing its
coordinates did not discover a new gravitational solution. An unknown line
element becomes a verified solution only after its functions and accompanying
fields satisfy the specified equations and conditions.

### Component reduction example: spherical Einstein equations

The user could select spherical polar coordinates $(t,r,\theta,\phi)$ and
declare the additional assumption of spherical symmetry. On a patch admitting
diagonal areal-radius coordinates, supply the unknown metric

$$
ds^2=-e^{2\nu(t,r)}dt^2+e^{2\lambda(t,r)}dr^2
     +r^2\left(d\theta^2+\sin^2\theta\,d\phi^2\right).
$$

Choosing spherical coordinates alone must not impose this ansatz. With the MTW
profile and $G=c=1$, request expansion of
$G_{ab}+\Lambda g_{ab}=8\pi T_{ab}$. Write primes for partial differentiation
with respect to $r$ and dots for partial differentiation with respect to $t$.
Expected normalised non-trivial component equations are

$$
\begin{aligned}
e^{-2\lambda}\left(\frac2r\lambda'-\frac1{r^2}\right)
  +\frac1{r^2}-\Lambda&=8\pi e^{-2\nu}T_{tt},\\[6pt]
e^{-2\lambda}\left(\frac2r\nu'+\frac1{r^2}\right)
  -\frac1{r^2}+\Lambda&=8\pi e^{-2\lambda}T_{rr},\\[6pt]
\frac2r\dot\lambda&=8\pi T_{tr}.
\end{aligned}
$$

The angular component is

$$
\begin{aligned}
&e^{-2\lambda}\left[\nu''+(\nu')^2-\nu'\lambda'
                     +\frac1r(\nu'-\lambda')\right]\\
&\quad-e^{-2\nu}\left[\ddot\lambda+(\dot\lambda)^2
                        -\dot\nu\dot\lambda\right]+\Lambda
  =\frac{8\pi}{r^2}T_{\theta\theta}.
\end{aligned}
$$

Before normalisation, the $\phi\phi$ equation is the $\theta\theta$ equation
multiplied by $\sin^2\theta$, requiring
$T_{\phi\phi}=\sin^2\theta\,T_{\theta\theta}$;
the other mixed stress components vanish. Show component relations and Bianchi
dependencies rather than calling all displayed equations independent. A matter
model and its evolution or conservation equations are additional problem data.

For $T_{ab}=0$ and $\Lambda=0$, the mixed component gives $\dot\lambda=0$.
After the remaining equations and a time-coordinate normalisation, the exterior
solution is $e^{2\nu}=e^{-2\lambda}=1-2M/r$. The
[Cambridge black-hole notes](https://www.damtp.cam.ac.uk/user/tong/gr/six.pdf)
provide a reference derivation. Keep this proposed spherical reduction
distinct from a general Einstein PDE solver.

### Physical predictions from a supplied metric

A useful mode should derive observables from known metrics before it can solve
general field equations. Treat the following as planned problem templates and
future acceptance examples, not implemented solvers. Geometric units and the
signature $(-,+,+,+)$ apply unless a displayed formula explicitly restores $c$
or $G$.

| Operation | Additional user input | Requested output |
| --- | --- | --- |
| Derive or integrate an orbit | Body, metric, initial event and timelike initial velocity, or specified orbital turning points | Geodesic equations, first integrals, reduced orbit equation and supported trajectory plot |
| Measure a frequency shift | Emission and reception events, observer four-velocities and the connecting light ray | Emitted and received frequencies, their ratio and redshift |
| Compare clocks | Clock worldlines, comparison events and any clock-synchronisation prescription | Proper times and a clearly defined rate or elapsed-time comparison |
| Measure a length | Rod endpoint worldlines or a specified spatial curve, measuring observer and simultaneity prescription | Proper rest length or measured length under the selected protocol |

#### Deriving Mercury's orbit

Use the exterior Schwarzschild metric as a supplied or independently verified
geometry. Approximate Mercury as a test body around an isolated, non-rotating,
spherical Sun, with $\Lambda=0$. This establishes a leading relativistic
comparison case, not a complete planetary ephemeris. Set

$$
f(r)=1-\frac{2M}{r},\qquad
ds^2=-f(r)dt^2+f(r)^{-1}dr^2
     +r^2(d\theta^2+\sin^2\theta\,d\phi^2),
$$

where $M$ is the Sun's geometric mass parameter; physical input supplies
$M=GM_\odot/c^2$. Work in the exterior $r>2M$ and choose the orbital plane
$\theta=\pi/2$. From the metric,
derive the timelike geodesic equations and normalisation

$$
\frac{dU^a}{d\tau}+\Gamma^a{}_{bc}U^bU^c=0,
\qquad U^a=\frac{dx^a}{d\tau},\qquad g_{ab}U^aU^b=-1.
$$

The time-translation and rotation symmetries give conserved specific energy
$E=f\dot t$ and specific angular momentum $\ell=r^2\dot\phi$, where dots
in this subsection mean proper-time derivatives. Expected radial first integral:

$$
\dot r^2+f(r)\left(1+\frac{\ell^2}{r^2}\right)=E^2.
$$

For a non-radial orbit with $\ell\ne0$, define $u=1/r$. Reducing the geodesic
equations gives the expected orbit equation

$$
\boxed{\frac{d^2u}{d\phi^2}+u=\frac{M}{\ell^2}+3Mu^2.}
$$

Retain circular solutions and extend the equation through turning points;
division by $du/d\phi$ must not discard them. A numerical first-order system is
$du/d\phi=w$, $dw/d\phi=M/\ell^2+3Mu^2-u$. Perihelion and aphelion radii
can determine $E,\ell$ from the radial first integral at both turning points.
Record how any supplied orbital elements are converted into those radii.

The native derivation should expose the Newtonian limit obtained by omitting
$3Mu^2$. Starting with $u_0=(1+e\cos\phi)/p$, where
$p=\ell^2/M$ at Newtonian order, perturbation of the orbit equation produces
a secular term. Its leading precession can be written

$$
u(\phi)\simeq\frac1p\left[1+e\cos\left((1-\delta)\phi\right)\right],
\qquad \delta=\frac{3M}{p}.
$$

This expression retains the leading secular effect and omits bounded
first-order corrections. With $p\simeq a(1-e^2)$, the expected advance is

$$
\Delta\phi\simeq\frac{6\pi GM_\odot}{a(1-e^2)c^2}
\quad\text{radians per orbit}.
$$

For representative Mercury data $a=57.909\times10^9$ metres,
$e=0.2056$ and period $87.969$ days, use the nominal solar mass parameter
$GM_\odot=1.3271244\times10^{20}\ \mathrm{m^3\,s^{-2}}$ and
$c=299792458\ \mathrm{m\,s^{-1}}$. The expected relativistic contribution is
approximately $0.1035$ arcseconds per orbit, or $43$ arcseconds per Julian
century of $36525$ days. Keep this contribution
separate from Newtonian planetary perturbations, solar multipoles and the total
observed precession. Label approximation order, element source and epoch;
numerical plots should locate successive perihelia and compare the measured
advance with this leading formula. Use
[Cambridge's orbit derivation](https://www.damtp.cam.ac.uk/user/tong/gr/grhtml/S1.html),
the [Edinburgh derivation](https://www.roe.ac.uk/japwww/teaching/gr/GR_notes_2021.pdf)
and [NASA's Mercury parameters](https://nssdc.gsfc.nasa.gov/planetary/factsheet/mercuryfact.html)
as independent references for the future tests. The nominal solar parameter is
defined by [IAU 2015 Resolution B3](https://iauarchive.eso.org/static/resolutions/IAU2015_English.pdf);
distinguish a nominal conversion constant from fitted ephemeris parameters.

#### Doppler shifts and gravitational redshift

Work in vacuum geometric optics. Specify future-directed unit observer
four-velocities $U^a$, emission and reception events, and a connecting null ray
with wave covector $k_a$. Expected local angular frequency and frequency ratio:

$$
\omega=-k_aU^a,\qquad
\frac{\omega_{\mathrm r}}{\omega_{\mathrm e}}
=\frac{(-k_aU^a)_{\mathrm r}}{(-k_aU^a)_{\mathrm e}},
\qquad 1+z=\frac{\omega_{\mathrm e}}{\omega_{\mathrm r}}.
$$

Use the same propagated ray at both endpoints. Its affine tangent obeys
$k^b\nabla_bk^a=0$ and $g_{ab}k^ak^b=0$; it is not a photon four-velocity.
The emission frequency fixes its scale, while the ratio is invariant under a
common constant rescaling. Observer worldlines may be prescribed or accelerated;
do not require every observer to be geodesic. This invariant contraction is the
common foundation for motion and gravitational frequency shifts. See
[Kasai's covariant treatment](https://arxiv.org/html/2302.11146v3).

For longitudinal recession in flat spacetime, use the emitter's inertial frame
and choose the positive $x$ direction along the light ray. With relative speed
$0<\beta=v/c<1$ and $\gamma=(1-\beta^2)^{-1/2}$, the native derivation should
construct

$$
U_{\mathrm e}^a=(1,0,0,0),\quad
U_{\mathrm r}^a=\gamma(1,\beta,0,0),\quad
k_a=\omega_{\mathrm e}(-1,1,0,0).
$$

Contracting gives $\omega_{\mathrm r}=\gamma(1-\beta)\omega_{\mathrm e}$,
and the expected ratio is

$$
\frac{\omega_{\mathrm r}}{\omega_{\mathrm e}}
=\sqrt{\frac{1-\beta}{1+\beta}}.
$$

At $\beta=3/5$ this is $1/2$; the corresponding approaching case gives $2$.
The [Einstein Online Doppler discussion](https://www.einstein-online.info/en/spotlight/doppler/)
provides a physical reference for the recession/approach convention.

For static observers in the Schwarzschild exterior, conservation of the ray's
time-translation energy $E_\gamma=-k_t$ and the observer normalisation
$U^t=f(r)^{-1/2}$ give $\omega=E_\gamma f(r)^{-1/2}$. The expected
gravitational ratio is

$$
\frac{\omega_{\mathrm r}}{\omega_{\mathrm e}}
=\sqrt{\frac{f(r_{\mathrm e})}{f(r_{\mathrm r})}}.
$$

Emission at $r_{\mathrm e}=4M$ and reception at $r_{\mathrm r}=8M$ give
$\sqrt{2/3}<1$. The result card should identify the observers, events, ray and
sign of the shift; separating motion and gravity is optional and requires a
specified reference observer family. Arbitrary connecting rays may require a
boundary-value solver, beyond the initial ODE initial-value scope.

#### Proper time and time dilation

Calculate elapsed clock time along the specified timelike worldline from

$$
d\tau^2=-g_{ab}\,dx^a dx^b.
$$

With a monotonic coordinate time $t$ and spatial coordinate velocities
$v^i=dx^i/dt$, the expected rate is

$$
\frac{d\tau}{dt}
=\sqrt{-g_{tt}-2g_{ti}v^i-g_{ij}v^iv^j}.
$$

Integrate this rate over the requested worldline segment. Identify the event
pair and comparison protocol; two clocks departing from the same event and
reuniting at another event have directly comparable accumulated proper times.

For constant inertial speed in Minkowski spacetime, restore $c$ to obtain
$\gamma=(1-v^2/c^2)^{-1/2}$ and the expected relation
$\Delta t=\gamma\Delta\tau$. At $v=3c/5$, a moving clock accumulates
$\Delta\tau=4\Delta t/5$. For a static Schwarzschild clock,
$d\tau=\sqrt{f(r)}\,dt$. Comparing static clocks at $4M$ and $8M$ over the
same Killing-time interval gives $d\tau_{4M}/d\tau_{8M}=\sqrt{2/3}$.
Keep coordinate time, proper time and light-signal arrival intervals labelled.
References include the [proper-time and redshift treatment](https://arxiv.org/html/2302.11146v3)
and [Einstein Online's clock discussion](https://www.einstein-online.info/en/relativity_space_time/).

#### Length measurements and contraction

The measurement definition is part of the input. A rod's rest length, a
coordinate separation, a length measured using simultaneous endpoint events
and an optical image are different outputs.

For an observer with $U^aU_a=-1$, construct the local rest-space projector and
its covariant spatial metric

$$
h^a{}_b=\delta^a{}_b+U^aU_b,\qquad h_{ab}=g_{ab}+U_aU_b.
$$

For an infinitesimal simultaneous displacement satisfying $U_a\,dx^a=0$,
the expected measured length is $d\ell^2=h_{ab}\,dx^a dx^b$. For a finite
rod moving inertially in flat spacetime, first choose endpoint events that are
simultaneous in the measuring inertial frame, then evaluate their spatial
separation. The expected result for motion parallel to the rod is

$$
L=\frac{L_0}{\gamma},\qquad \gamma=(1-v^2/c^2)^{-1/2},
$$

where $L_0$ is its proper rest length. At $v=3c/5$, $L=4L_0/5$; transverse
dimensions are unchanged. To derive this, let primed coordinates denote the
rod's rest frame and unprimed coordinates the measuring frame. The Lorentz
relation $\Delta x'=\gamma(\Delta x-v\Delta t)$, evaluated at
$\Delta t=0$, gives $L_0=\gamma L$. Transforming a pair of events simultaneous
only in the rod's rest frame cannot by itself produce the measuring frame's
rod length.
The [UNSW simultaneity and length discussion](https://www.phys.unsw.edu.au/einsteinlight/jw/module4_time_dilation.htm)
provides a reference for this measurement prescription.

In curved spacetime, a local projector does not alone select a global surface
of simultaneity. Extended lengths need an explicit spatial slice or operational
definition. For example, on a static Schwarzschild $t=\mathrm{constant}$ slice,
the expected radial proper distance and circumferential length are

$$
\ell_{12}=\int_{r_1}^{r_2}f(r)^{-1/2}\,dr,
\qquad C(r)=2\pi r,\qquad r_2>r_1>2M.
$$

This describes a spatial measurement on that slice; it is not a universal
material stretching law. Return the observer, slice, endpoint events and units
alongside every reported length.

#### Solver and presentation boundaries

The tensor module derives connections, first integrals, observer contractions
and typed component systems. Supported evolution and trajectory calculations
belong to the [differential-equation module](./differential-equations.md);
endpoint, turning-point and intersection conditions require suitable event or
boundary-value handling. Algebraic frequency ratios and Lorentz transformations
can be useful before trajectory integration is available.

Native results should provide assumptions, conserved quantities, reduced
equations, measurement definitions, symbolic and numerical results, approximation
orders and residual or invariant checks. Plotting consumes native trajectory
data. The browser must not implement geodesic integration, tensor contractions,
frequency-shift formulae or mathematical simplification. Persist the geometry,
observers, conditions and measurement protocol with the problem history.

## Delivery and acceptance

Implement in independently useful stages, with ordinary tests first and matching
README examples last. Every syntax example above is a future acceptance fixture;
turn it into an executable test when its parser and operation land. Do not
present unimplemented design examples as successful runtime tests.
The coordinate and physical-prediction examples also require executable native
tests when their operations land, including the stated input assumptions,
measurement definitions and expected outputs.

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
   interface and the coordinate-transformation workflow can precede the complete
   geometry controls and differential-equation system solvers.
7. **Further geometry:** action variations, perturbations, hypersurfaces, ADM
   decompositions and separately designed spinor/gauge-bundle extensions.

Required independent reference cases include:

| Geometry or operation | Expected result |
| --- | --- |
| Cartesian rotation field $V^x=-y$, $V^y=x$, transformed to polar coordinates | Coordinate-basis components $(0,1)$; explicitly selected Euclidean orthonormal polar components $(0,r)$ |
| One-form $\omega_x=-y$, $\omega_y=x$ under the same transformation | Polar coordinate-basis components $(0,r^2)$, preserving the distinction from the vector |
| Cartesian metric $\operatorname{diag}(1,1)$ transformed to polar coordinates | Polar coordinate-basis metric $\operatorname{diag}(1,r^2)$; the chart transformation itself requires no supplied metric |
| Flat spacetime transformed to the custom toroidal chart | The displayed toroidal line element with zero Riemann curvature, despite non-zero connection coefficients |
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
| Schwarzschild equatorial timelike geodesic reduction | $u''+u=M/\ell^2+3Mu^2$, with correct radial first integral and circular/turning-point branches retained |
| Weak-field Mercury orbit | Approximately $43$ arcseconds per Julian century for the relativistic perihelion contribution, with the approximation and input data recorded |
| Longitudinal flat-spacetime Doppler shift at speed $3c/5$ | Received/emitted frequency ratios $1/2$ for recession and $2$ for approach |
| Static Schwarzschild emission at $4M$, reception at $8M$ | Frequency ratio $\sqrt{2/3}$; coordinate changes preserve the observer contractions |
| Clock moving inertially at speed $3c/5$ | Proper elapsed time $4/5$ of the measuring frame's coordinate-time interval |
| Rod moving parallel to its length at speed $3c/5$ | Measured length $4/5$ of its rest length, using endpoint events simultaneous in the measuring frame |
| Local observer rest-space projector | $h_{ab}U^b=0$ and $h^a{}_ch^c{}_b=h^a{}_b$, with explicitly selected simultaneity for finite-length measurements |

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
