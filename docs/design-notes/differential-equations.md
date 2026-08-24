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
