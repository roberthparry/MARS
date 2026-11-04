# Future Differential-Equation Work

Construction, parsing, inspection, formatting, and the current symbolic solver
families are implemented. Their behaviour and public API are documented in
[`docs/diffequation.md`](../diffequation.md). This note records only work that
remains outside the implemented scope.

## Symbolic Scope

Potential extensions include:

- systems of coupled differential equations;
- broader variable-coefficient ordinary differential equations;
- general Bernoulli powers with complete inverse branches;
- singular solutions lost during algebraic division;
- explicit domains, branch conditions, and solution assumptions; and
- broader partial differential-equation families.

Systems should use an ordered collection of base equations rather than encode
several relations inside one expression. Any extension must preserve the shared
symbol identity used by equations, independent variables, constants, and
conditions.

## Tensor and Geometric Systems

Coordinate with the [tensor design and proposed Lab Tensor mode](./tensors.md).
Einstein, Maxwell, Killing, geodesic and transport equations should be
constructible as typed geometric relations before a solver supports their full
solution families. The tensor module owns index algebra, connections and
curvature; the differential-equation module owns supported solving methods.

Component reduction must carry the geometry, chart domain, conventions,
constraints, unknown functions and independent variables into the resulting
system. Preserve tensor symmetries when identifying independent component
equations. Do not report a complete tensor solution after solving only a subset
of components, and distinguish verification of a proposed Killing field from
finding every Killing field.

The [tensor prediction workflows](./tensors.md#physical-predictions-from-a-supplied-metric)
identify timelike and null geodesics as initial coupled-ODE applications. A
four-dimensional geodesic can produce eight first-order state equations for
position and tangent, together with its timelike or null normalisation
constraint. The tensor module supplies the native right-hand sides; the solver
must preserve chart domains and report constraint drift. Mercury's reduced
orbit supplies a smaller two-state reference problem with perihelion event
detection and a known leading precession formula.

Frequency-shift problems with a prescribed ray can use algebraic observer
contractions. Finding a ray connecting two specified events may instead require
a boundary-value or shooting method; an initial-value integrator alone does
not provide that capability. Clock comparisons may accumulate proper time as
an auxiliary state or quadrature. Finite-length measurements also require an
explicit observer and simultaneity or spatial-slice prescription, independently
of numerical integration.

## Numerical Solver Boundary

The first numerical scope should be non-stiff ordinary differential-equation
initial-value problems. Boundary-value problems, differential-algebraic
equations, stiff systems, and numerical partial differential equations require
separate solver families.

Before numerical integration, an explicit higher-order equation should be
reduced to a first-order state system. For a second-order equation in `y`, the
internal solver plan would contain:

```text
Y₀ = y
Y₁ = Dx(y)
Dx(Y₀) = Y₁
```

The original `diffequ_t` must remain unchanged by this reduction.

An adaptive embedded Runge--Kutta method, initially Dormand--Prince 5(4), is a
suitable default for non-stiff problems. Error acceptance should be
component-wise:

```text
|errorᵢ| <= abs_tolᵢ + rel_tol·max(|oldᵢ|, |newᵢ|)
```

All state values, independent-variable values, tolerances, and error estimates
must use `number_t`. An implementation must not silently reduce
arbitrary-precision inputs to `double`. A lower-precision fast path is
appropriate only when the selected numeric representation and requested
tolerance permit it.

A numerical solution object should own:

- accepted independent-variable values;
- every dependent state value at each accepted point;
- local error estimates;
- termination status and a diagnostic message; and
- optional dense-output interpolation data.

Failure must have an explicit status. A numeric `NAN` is not a sufficient
explanation of solver failure.

## Numerical Test Requirements

Numerical solver tests should cover convergence order, tolerance enforcement,
backwards integration, termination diagnostics, and high-precision cases that
would expose an accidental conversion to `double`.
