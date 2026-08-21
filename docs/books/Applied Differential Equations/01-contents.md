<a id="solution-of-differential-equations"></a>
# Solution of differential equations

<a id="preface"></a>
## Preface

These notes were transcribed by the author from blackboard lectures delivered by Professor P. G. L. Leach in 1986,
during the author's honours year studying Applied Mathematics at the University of the Witwatersrand. They are the
author's contemporaneous record of those lectures, subsequently organised, typeset, and edited into their present
form.

The mathematical exposition, derivations, and examples recorded here originate in Professor Leach's lectures unless
another source is expressly acknowledged. The transcription, subsequent organisation, and editorial presentation
are the author's own. These notes are not an official publication of the University of the Witwatersrand and have
not been endorsed by either the University or Professor Leach.

<a id="contents"></a>
## Contents

- [Linearisation of non-linear differential equations](./02-linearisation-of-non-linear-odes.md#linearisation-of-non-linear-differential-equations)
  - [Point transformation](./02-linearisation-of-non-linear-odes.md#point-transformation)
  - [Lie algebra](./02-linearisation-of-non-linear-odes.md#lie-algebra)
  - [How to deal with derivatives](./02-linearisation-of-non-linear-odes.md#how-to-deal-with-derivatives)
    - [Notes](./02-linearisation-of-non-linear-odes.md#derivative-notes)
  - [Symmetries of a second-order ODE](./02-linearisation-of-non-linear-odes.md#symmetries-of-a-second-order-ode)
  - [Relationship between symmetries and first integrals](./02-linearisation-of-non-linear-odes.md#symmetries-and-first-integrals)
  - [What nonlinear differential equations can have $SL(3,\mathbb R)$ symmetry?](./02-linearisation-of-non-linear-odes.md#nonlinear-equations-with-sl3-symmetry)
  - [What second-order ODEs can be linearised?](./02-linearisation-of-non-linear-odes.md#linearisable-second-order-odes)
    - [Classify by extra information](./02-linearisation-of-non-linear-odes.md#classify-by-extra-information)
      - [Existence of one symmetry](./02-linearisation-of-non-linear-odes.md#existence-of-one-symmetry)
      - [Existence of two symmetries](./02-linearisation-of-non-linear-odes.md#existence-of-two-symmetries)
      - [Two commuting symmetries](./02-linearisation-of-non-linear-odes.md#two-commuting-symmetries)
      - [Two non-commuting symmetries](./02-linearisation-of-non-linear-odes.md#two-non-commuting-symmetries)
    - [Proposition](./02-linearisation-of-non-linear-odes.md#linearisation-proposition)
    - [Proof](./02-linearisation-of-non-linear-odes.md#linearisation-proposition-proof)
    - [Can one do better?](./02-linearisation-of-non-linear-odes.md#can-one-do-better)
  - [Type III](./02-linearisation-of-non-linear-odes.md#type-iii)
  - [Linearisation of a system of equations](./02-linearisation-of-non-linear-odes.md#linearisation-of-a-system-of-equations)
    - [Theorem](./02-linearisation-of-non-linear-odes.md#system-linearisation-theorem)
    - [Proof](./02-linearisation-of-non-linear-odes.md#system-linearisation-proof)
- [Lie Theory of Extended Group](./03-lie-theory-of-extended-group.md#lie-theory-of-extended-group)
  - [Ordinary Differential Equations](./03-lie-theory-of-extended-group.md#ordinary-differential-equations)
  - [Lie Algebra](./03-lie-theory-of-extended-group.md#lie-algebra-extended-group)
  - [The Free Particle](./03-lie-theory-of-extended-group.md#the-free-particle)
- [Linear second-order ODEs](./04-linear-second-order-odes.md#linear-second-order-odes)
  - [Factorisation technique](./04-linear-second-order-odes.md#factorisation-technique)
  - [Existence of solutions](./04-linear-second-order-odes.md#existence-of-solutions)
  - [Standard form of the eigenvalue differential equation](./04-linear-second-order-odes.md#standard-eigenvalue-form)
  - [Example](./04-linear-second-order-odes.md#harmonic-oscillator)
  - [Orthogonal polynomials](./04-linear-second-order-odes.md#orthogonal-polynomials)
  - [Definition of factorisation](./04-linear-second-order-odes.md#definition-of-factorisation)
  - [Associated spherical harmonics](./04-linear-second-order-odes.md#associated-spherical-harmonics)
  - [Eigenfunctions of the spherical harmonics associated with $SO(3)$](./04-linear-second-order-odes.md#spherical-harmonics-so3)
- [Index](./05-index.md#index)

---

<nav aria-label="Section navigation" style="display: grid; grid-template-columns: minmax(0, 1fr) auto; column-gap: 2em; align-items: start;">
<div style="display: grid; grid-template-columns: 6em minmax(0, 1fr); row-gap: 0.25em;">
<span>NEXT:</span><a href="02-linearisation-of-non-linear-odes.md">Linearisation of non-linear ODEs</a>
<span>PREVIOUS:</span><a href="05-index.md">Index</a>
</div>
<a href="05-index.md" style="justify-self: end; text-align: right;">INDEX</a>
</nav>
