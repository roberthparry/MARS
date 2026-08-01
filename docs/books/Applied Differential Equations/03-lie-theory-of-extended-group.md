<a id="lie-theory-of-extended-group"></a>
<h1 style="text-align: center;">1 LIE THEORY OF EXTENDED GROUP</h1>

In this chapter we outline the salient features of Lie theory which are of direct relevance to the following chapters. Other topics which enter naturally are discussed as the occasion arises.

<a id="ordinary-differential-equations"></a>
<h2 style="text-align: center;">1.1 Ordinary Differential Equations</h2>

In the study of the Lie theory of extended group, relevant to single second-order ordinary differential equations, a one-parameter point transformation acts on solution curves in $(t,q)$ space and transforms solution curves into solution curves which possess the same value of the associated invariant (Lutzky 1979). This is equivalent to the requirement that the equation be form-invariant under the transformation. The set of all such one-parameter transformations for a particular differential equation forms a group. For such a monoparametric transformation group, the group operator is

$$
G(t,q)=\xi(t,q)\frac{\partial}{\partial t}
       +\eta(t,q)\frac{\partial}{\partial q}.
\tag{1.1}
$$

For a function involving the $n$th ($n=1,2$ in the present study) derivative, the $n$-times extended group operator is given by

$$
G^{(n)}=G^{(n-1)}
       +\eta^{(i)}\frac{\partial}{\partial q^{(i)}},
\tag{1.2}
$$

where

$$
\begin{aligned}
\eta^{(k)}
  &=\frac{d\eta^{(k-1)}}{dt}
    -q^{(k)}\frac{d\xi}{dt},
    \qquad k=1,2,\\[6pt]
\frac d{dt}
  &=\frac{\partial}{\partial t}
    +\dot q\frac{\partial}{\partial q}
    +\ddot q\frac{\partial}{\partial\dot q}.
\end{aligned}
\tag{1.3}
$$

The finite transformations of the group can be obtained by exponentiation of the group operator

$$
\bar t=(\exp\alpha G)t,
\qquad
\bar q=(\exp\alpha G)q,
\tag{1.4}
$$

where $\alpha$ is the group parameter, or by integration of the system of differential equations

$$
\frac{d\bar t}{\xi(\bar t,\bar q)}
=\frac{d\bar q}{\eta(\bar t,\bar q)}
=d\alpha,
\tag{1.5}
$$

<div style="break-after: page;"></div>

subject to the initial conditions

$$
\bar t=t,
\qquad
\bar q=q
\qquad\text{when}\qquad
\alpha=0.
$$

We obtain the infinitesimal transformation in $(t,q)$ space by using (1.4). Employing the infinitesimal notation $\delta\alpha$ for the value of $\alpha$ in the immediate neighbourhood of $\alpha=0$ and neglecting $O(\delta\alpha^2)$ terms, we can write (1.4) as

$$
\bar t=t+\xi\,\delta\alpha,
\qquad
\bar q=q+\eta\,\delta\alpha.
\tag{1.6}
$$

The induced variations in the higher derivatives are likewise given by

$$
\bar q'=\dot q+\eta^{(1)}\delta\alpha,
\qquad
\bar q''=\ddot q+\eta^{(2)}\delta\alpha,
\tag{1.7}
$$

where $'$ denotes $d/dt$. This is easily verified by utilising the finite transformations of the first and second extended group

$$
\bar q'=(\exp\alpha G^{(1)})\dot q,
\qquad
\bar q''=(\exp\alpha G^{(2)})\ddot q.
\tag{1.8}
$$

For a general second-order differential equation

$$
N(\ddot q,\dot q,q,t)=0,
\tag{1.9}
$$

the second extended operator $G^{(2)}$ is used. An operator $G$ is said to be the generator of a one-parameter symmetry group for (1.9) if, whenever (1.9) is satisfied,

$$
G^{(2)}N(\ddot q,\dot q,q,t)=0,
$$

or equivalently

$$
\xi\frac{\partial N}{\partial t}
+\eta\frac{\partial N}{\partial q}
+\eta^{(1)}\frac{\partial N}{\partial\dot q}
+\eta^{(2)}\frac{\partial N}{\partial\ddot q}=0.
\tag{1.10}
$$

Writing

$$
N=\ddot q-M(\dot q,q,t),
\tag{1.11}
$$

(1.10) becomes

$$
\eta^{(2)}-G^{(1)}M=0.
\tag{1.12}
$$

Generally, a vector field

$$
Y=\xi(t,q,\dot q)\frac{\partial}{\partial t}
 +\eta(t,q,\dot q)\frac{\partial}{\partial q}
 +\zeta(t,q,\dot q)\frac{\partial}{\partial\dot q}
$$

is said to be a dynamical symmetry of

$$
\Gamma=\frac{\partial}{\partial t}
      +\dot q\frac{\partial}{\partial q}
      +M\frac{\partial}{\partial\dot q}
$$

<div style="break-after: page;"></div>

if

$$
\mathcal L_Y\Gamma=[Y,\Gamma]=g\Gamma,
\tag{1.13}
$$

for a suitable function $g$. In this interpretation, the flow of $Y$ maps integral curves of $\Gamma$ into integral curves, subject to a change in the parametrization along the integral curves. Equation (1.13) gives rise to the following conditions (see e.g. Sarlet and Cantrijn 1981)

$$
\begin{aligned}
\zeta&=\Gamma(\eta)-\dot q\,\Gamma(\xi),\\
\Gamma(\zeta)-M\Gamma(\xi)-Y(M)&=0,\\
g&=-\Gamma(\xi),
\end{aligned}
\tag{1.14}
$$

which in turn implies

$$
\Gamma^2(\eta)-\dot q\,\Gamma^2(\xi)
-2M\Gamma(\xi)-Y(M)=0.
\tag{1.15}
$$

This is precisely the criterion derived by Anderson and Davison (1974) in the context of generalization of the Lie theory. It is also of interest to note that (1.15) is identical to (1.12) when $\xi$ and $\eta$, satisfying (1.15), are independent of the velocity. In view of this, we say that $Y$ (actually $Y^{(0)}=\xi\,\partial/\partial t+\eta\,\partial/\partial q$) is a Lie point symmetry of $\Gamma$.

For given $\xi$ and $\eta$, a first integral may be obtained by imposing the double requirement

$$
G^{(1)}I(t,q,\dot q)=0,
\qquad
\frac d{dt}I(t,q,\dot q)=0.
\tag{1.16}
$$

The function $I(t,q,\dot q)$ satisfying (1.16a) is said to be an invariant of the first extended group. Equation (1.16a) gives rise to the following system of ordinary differential equations

$$
\frac{dt}{\xi}
=\frac{dq}{\eta}
=\frac{d\dot q}{\eta^{(1)}},
\tag{1.17}
$$

the solution of which produces two characteristics $u(t,q)$ and $v(t,q,\dot q)$, referred to as the group invariant and the first order differential invariant respectively. Thus an arbitrary function $F(u,v)$ of $u$ and $v$ is the solution of (1.16a). Invoking (1.16b) leads to

$$
\frac{\partial F}{\partial u}\dot u
+\frac{\partial F}{\partial v}\dot v=0,
$$

where the dot denotes $d/dt$. Clearly we have

$$
\frac{dv}{du}=\frac{\dot v}{\dot u}.
\tag{1.18}
$$

<div style="break-after: page;"></div>

If $M$ (as in (1.11)) is known, the right hand side of (1.18) can be written as a function of $u$ and $v$, say $G(u,v)$, and we have

$$
\frac{dv}{du}=G(u,v).
\tag{1.19}
$$

A first integral of the differential equation (1.11) is then obtained by solving (1.19).

The preceding discussion also shows how the knowledge of a point symmetry enables the reduction of a given second-order differential equation to a first-order equation, namely (1.19). Furthermore, the general second-order equation invariant under a given group can be expressed as

$$
\frac{
\dfrac{\partial v}{\partial t}
+\dot q\dfrac{\partial v}{\partial q}
+\ddot q\dfrac{\partial v}{\partial\dot q}
}{
\dfrac{\partial u}{\partial t}
+\dot q\dfrac{\partial u}{\partial q}
}
=H\bigl(u(t,q),v(t,q,\dot q)\bigr),
\tag{1.20}
$$

where $H$ is an arbitrary function and the left hand side is the quotient $\dot v/\dot u$ of (1.19).

The following remark is important: If a first integral $I$ (corresponding to $G$) is obtained by integration (in the above sense (1.16)), then it is possible to generate other first integrals and from these further first integrals until all operations $X^{(1)}I$ become meaningless, where $X^{(1)}$ is the first extension of $X\ne G$ belonging to the set of given operators. It is a simple matter to verify this. Indeed

$$
\Gamma(I)=0
\qquad\text{and}\qquad
[X^{(1)},\Gamma]=g\Gamma
$$

implies

$$
\Gamma(X^{(1)}I)
=[\Gamma,X^{(1)}]I+X^{(1)}(\Gamma(I))=0,
$$

so that $X^{(1)}I$ is potentially another first integral.

It is often desirable to introduce new coordinates $Q=F(t,q)$, $T=G(t,q)$ (frequently written as $Q=Q(t,q)$, $T=T(t,q)$ when no confusion arises) in which the generator (1.1) of the group (1.4) appears as a generator of time or space translation (or the generator of any other suitable group). In this respect we have

$$
\begin{aligned}
\bar G(t,q)
  &=(GT)\frac{\partial}{\partial T}
    +(GQ)\frac{\partial}{\partial Q}\\
  &=\bar\xi(T,Q)\frac{\partial}{\partial T}
    +\bar\eta(T,Q)\frac{\partial}{\partial Q}.
\end{aligned}
\tag{1.21}
$$

Thus

$$
\begin{aligned}
\xi(t,q)\frac{\partial T}{\partial t}
+\eta(t,q)\frac{\partial T}{\partial q}
  &=\bar\xi(T,Q),\\[6pt]
\xi(t,q)\frac{\partial Q}{\partial t}
+\eta(t,q)\frac{\partial Q}{\partial q}
  &=\bar\eta(T,Q),
\end{aligned}
\tag{1.22}
$$

<div style="break-after: page;"></div>

the solution of which explicitly yields the new coordinates $Q$ and $T$. If the group generated by (1.21) is one of translation, then $Q$ and $T$ are called canonical coordinates. This will be the case when either $\bar\xi=1$ and $\bar\eta=0$ (time-translation) or $\bar\xi=0$ and $\bar\eta=1$ (space-translation).

More details on the Lie theory of extended groups may be found in Dickson (1924) and Bluman and Cole (1974).

<a id="lie-algebra-extended-group"></a>
<h2 style="text-align: center;">1.2 Lie Algebra</h2>

A Lie algebra consists of a vector space $L$ over a field $F$, together with a binary operation of commutation $[\ ,\ ]$ defined on $L$ such that the following axioms are satisfied:

(a) bilinearity: for any $u,v,w\in L$ and $a,b\in F$

$$
\begin{aligned}
[au+bv,w]&=a[u,w]+b[v,w],\\
[u,av+bw]&=a[u,v]+b[u,w];
\end{aligned}
$$

(b) antisymmetry: for any $u,v\in L$

$$
[u,v]=-[v,u];
$$

(c) the Jacobi identity: for any $u,v,w\in L$

$$
[[u,v],w]+[[v,w],u]+[[w,u],v]=0.
$$

We shall (somewhat incorrectly) speak of the Lie algebra $L$, where we take $F=\mathbb R$. For our purpose, we define the binary operation of commutation on a Lie algebra $L$ of operators as the commutator

$$
[X,Y]=XY-YX
\qquad\text{for any }X,Y\in L.
\tag{1.23}
$$

If a differential equation admits the operators $X$ and $Y$ (in the sense mentioned previously), then it also admits their commutator $[X,Y]$ (Ovsiannikov 1978). Consequently the set of all operators admitted by a given differential equation generates a Lie algebra. The largest admitted Lie algebra is called the full Lie algebra of the equation. We encounter, in this work, only finite-dimensional Lie algebras of dimensionality $r$, where $r\leq8$. Thus it is usual to represent a finite-dimensional Lie algebra $L$ by a table of commutators, i.e., by an $r\times r$ matrix in which the commutator $[G_i,G_j]$ ($i,j=1,r$ where $\{G_k\}$ is the basis) is placed at the intersection of the $i$th row and $j$th column. The resulting matrix is antisymmetric and it

<div style="break-after: page;"></div>

is necessary to calculate only its upper half (see e.g. Mahomed and Leach 1985). Therefore we can write

$$
[G_i,G_j]=C^k{}_{ij}G_k,
\tag{1.24}
$$

where the numbers $C^k{}_{ij}$ are called the structure constants of the Lie algebra $L$ with respect to the basis $\{G_k\}$. These numbers are antisymmetric relative to the lower indices ($C^k{}_{ij}=-C^k{}_{ji}$) and satisfy the Jacobi identity

$$
C^i{}_{jk}C^k{}_{lm}
+C^i{}_{lk}C^k{}_{mj}
+C^i{}_{mk}C^k{}_{jl}=0.
$$

The above properties are useful in constructing a Lie algebra.

It is clear from the relation

$$
[G_i^{(n)},G_j^{(n)}]=[G_i,G_j]^{(n)},
\qquad n\in\mathbb N,
\tag{1.25}
$$

that the $n$-times extended operators generate an $r$-dimensional Lie algebra, denoted $L^{(n)}$. Moreover $L^{(n)}$ has the same structure constants as $L$ when referred to basis $\{G_k^{(n)}\}$,

$$
[G_i^{(n)},G_j^{(n)}]
=C^k{}_{ij}G_k^{(n)}.
\tag{1.26}
$$

The classification of real low-dimensional Lie algebras was initiated by Lie (Lie 1891). It still engages scores of specialists (see e.g. Patera et al.). For many of them, the interest in studying the classification problem lies in its usefulness in many physical applications. Indeed, the operators admitted by a second-order differential equation generate a finite-dimensional Lie algebra of dimension at most eight. This is a direct consequence of Lie's counting theorem for second-order equations (Anderson and Davison 1974). Presently we concern ourselves with differential equations which admit two-dimensional Lie algebras of operators; deferring discussion on higher dimensional algebras to Chapter 5. There are two Lie algebras of dimension two, one abelian and one solvable (Barut and Raczka 1980):

$$
[G_1,G_2]=0,
\qquad
[G_1,G_2]=G_1.
\tag{1.27}
$$

Lie showed that second-order ordinary differential equations possessing two generators of symmetry have four canonical forms (Lie 1891). They are, with their associated generators in canonical form, given in the table below.

It is observed that there are two canonical forms for each of the Lie algebras (1.27a) and (1.27b).

Lie deduced that if an equation admits a two-dimensional Lie algebra with operators $G_1,G_2$ satisfying $G_2=\rho(t,q)G_1$ (i.e. type 2 or 4) then it is linearizable. This is evident from the above table. Moreover a theorem of Lie states that every linear equation is reducible to the free particle equation. Hence an equation of type 2

<div style="break-after: page;"></div>

or 4 can be transformed to the free particle equation and accordingly admits six additional operators.

**REMARK:** If a second-order equation admits the operators $G_1$ and $G_2$ then it follows from (1.24) that

$$
[G_1,G_2]=aG_1+bG_2,
\qquad a,b\in\mathbb R.
\tag{1.28}
$$

For $a$ and $b$ both zero, $G_1$ and $G_2$ commute. However, when at least one of $a$ or $b$ is nonzero, we choose the basis $\{V_1,V_2\}$ so that $[V_1,V_2]=V_1$. This is easily done as follows: if $a\ne0$ and $b\ne0$ or $a\ne0$ and $b=0$, we introduce the basis $\{V_1=G_1+(b/a)G_2,V_2=(1/a)G_2\}$. In the case $a=0$ and $b\ne0$, we choose $\{V_1=G_2,V_2=-(1/b)G_1\}$. The original equation admits the operators $V_1$ and $V_2$ since they are merely linear combinations of $G_1$ and $G_2$.

The contents of Table 1 are treated in greater detail in the following chapters. Various new features emerge.

For readable accounts on Lie algebras, the interested reader is referred to Wybourne (1974), Gilmore (1974) and Ovsiannikov (1978).

<a id="the-free-particle"></a>
<h2 style="text-align: center;">1.3 The Free Particle</h2>

The free particle, being the simplest system, is regarded as a paradigm of dynamical systems. It has been discussed by various authors (Lie 1891, Anderson and Davison 1974). Certain features of the problem, however, have received attention only recently (Mahomed and Leach 1985). Conventionally the symmetry generators of a one-dimensional system are used to determine the first integrals associated with the system. In the last cited reference, the reverse procedure was adopted for the free particle.

The one-dimensional free particle has equation

$$
\ddot q=0,
\tag{1.29}
$$

where the dot denotes $d/dt$, with Hamiltonian

$$
H=\frac12p^2,
\qquad p=\dot q.
\tag{1.30}
$$

The two first integrals for (1.29) are easily seen to be

$$
\begin{aligned}
I_1&=p,\\
I_2&=q-tp.
\end{aligned}
\tag{1.31}
$$

<div style="break-after: page;"></div>

and we include their quotient

$$
I_3=\frac{q-tp}{p},
\tag{1.32}
$$

since this has been shown by Leach (1980) to be relevant from the corresponding integral for the simple harmonic oscillator.

We seek the set of symmetry generators with which each of $I_1,I_2,I_3$ is associated.

$\xi$ and $\eta$ of the generator

$$
G=\xi(t,q)\frac{\partial}{\partial t}
 +\eta(t,q)\frac{\partial}{\partial q}
 +\zeta(t,q,p)\frac{\partial}{\partial p}
\tag{1.33}
$$

are determined by the following equations

$$
\zeta\frac{\partial I}{\partial p}
+\eta\frac{\partial I}{\partial q}
+\xi\frac{\partial I}{\partial t}=0,
\tag{1.34}
$$

$$
\eta^{(1)}
-\zeta\frac{\partial^2H}{\partial p^2}
-\eta\frac{\partial^2H}{\partial q\partial p}
-\xi\frac{\partial^2H}{\partial t\partial p}=0,
\tag{1.35}
$$

where

$$
\eta^{(1)}=\dot\eta-\dot\xi\frac{\partial H}{\partial p},
\tag{1.36}
$$

by eliminating $\zeta$ between (1.34) and (1.35) and insisting that $\xi$ and $\eta$ be independent of $p$ (Leach 1980).

Thus for the first integral $I_1=p$ we obtain

$$
\frac{d\eta}{dt}-p\frac{d\xi}{dt}=0,
\tag{1.37}
$$

where

$$
\frac d{dt}=\frac{\partial}{\partial t}
             +p\frac{\partial}{\partial q}.
$$

(1.37) yields a partial differential equation in which the terms are grouped together in powers of $p$. By equating coefficients of separate powers of $p$ to zero we obtain

$$
\begin{aligned}
p^2:&\qquad \xi=a(t),\\
p^1:&\qquad \eta=\dot a q+b(t),
\end{aligned}
\tag{1.38}
$$

and

$$
\begin{aligned}
p^0:&\qquad a=At+B,\\
    &\qquad b=C,
\end{aligned}
\tag{1.39}
$$

<div style="break-after: page;"></div>

where $A,B$ and $C$ are constants.

Therefore the triplet of generators with which $I_1$ is associated is

$$
\begin{aligned}
G_1&=t\frac{\partial}{\partial t}
    +q\frac{\partial}{\partial q},
&G_2&=\frac{\partial}{\partial t},\\[6pt]
G_3&=\frac{\partial}{\partial q}.
\end{aligned}
\tag{1.40}
$$

In like manner, for $I_2$, we obtain

$$
\begin{aligned}
G_4&=t\frac{\partial}{\partial t},
&G_5&=t^2\frac{\partial}{\partial t}
     +tq\frac{\partial}{\partial q},\\[6pt]
G_6&=t\frac{\partial}{\partial q}.
\end{aligned}
\tag{1.41}
$$

Similarly, for $I_3$,

$$
\begin{aligned}
G_7&=tq\frac{\partial}{\partial t}
     +q^2\frac{\partial}{\partial q},
&G_8&=q\frac{\partial}{\partial t},\\[6pt]
G_9&=q\frac{\partial}{\partial q}.
\end{aligned}
\tag{1.42}
$$

It is observed that $G_1,G_4$ and $G_9$ are linearly dependent. Indeed we have

$$
G_1=G_4+G_9.
\tag{1.43}
$$

We now compare the standard generators (Lie 1891) of the free particle with those obtained here using the reverse procedure. In order to do this, we need to determine the usual free particle generators using the (direct) Lie method of extended group (see Section 1.1).

An operator of the form (1.1) will be a symmetry generator for (1.29) if and only if $\xi$ and $\eta$ satisfy (invariance of (1.29) under $G^{(2)}$)

$$
\eta^{(2)}=0,
\tag{1.44}
$$

which, when different powers of $\dot q$ are treated as linearly independent (as they are, since we have a second-order equation) yields, on equating the coefficients of separate powers of $\dot q$ to zero (bearing in mind that $\xi$ and $\eta$ are functions of $t$ and $q$ only), for the third and second powers

$$
\dot q^3:
\qquad
\xi=a(t)q+b(t),
\tag{1.45}
$$

and

$$
\dot q^2:
\qquad
\eta=\dot a q^2+c(t)q+d(t),
\tag{1.46}
$$

<div style="break-after: page;"></div>

where the time-dependent functions $a,b,c,$ and $d$ are solutions of

$$
\begin{aligned}
\dot q^1:&\qquad \ddot a=0,
&2\dot c-\ddot b&=0,\\[4pt]
\dot q^0:&\qquad \ddot c=0,
&\ddot d&=0.
\end{aligned}
\tag{1.47}
$$

We note that this system of equations has eight linearly independent solutions. Hence (1.29) has eight generators of symmetry which correspond identically to the generators $G_2,G_3,\ldots,G_9$ obtained via the reverse procedure.

To illustrate how one obtains a first integral for a given generator, we solve equations (1.16) for $G_4$. The first of (1.16) leads to (cf. (1.17))

$$
\frac{dt}{t}
=\frac{d\dot q}{-\dot q}
=\frac{dq}{0},
\tag{1.48}
$$

the solution of which produces the characteristics

$$
u=q,
\qquad
v=t\dot q.
$$

Applying equation (1.18) we then obtain

$$
\frac{dv}{du}=1.
\tag{1.49}
$$

Whence the first integral associated with $G_4$ is

$$
I_2=q-t\dot q,
$$

as expected. In the same way one can obtain the first integrals $I_1$ and $I_2$ corresponding to the generators given previously.

Of great interest to us are the Lie algebraic properties of the nine generators which in some sense are simpler than those for the usual eight as we shall see below. The commutation relations between the $G_i$'s are given in the following table.

The above table cannot be regarded in the usual sense since there is a linear dependence relation between $G_1,G_4$ and $G_9$. However, there are certain nice features that can be noted as we shall soon discuss. Moreover, the conventional table is still available if we disregard the entries $[G_1,G_j]$ and $[G_i,G_1]$ ($i,j=1,9$).

Each of the triplets of generators $\{G_1,G_2,G_3\}$, $\{G_4,G_5,G_6\}$ and $\{G_7,G_8,G_9\}$, associated with $I_1,I_2$ and $I_3$ respectively, forms a subalgebra. This is evident from the table (see diagonal blocks). Furthermore, each of these subalgebras can be written in the form

$$
[X_1,X_2]=0,
\qquad
[X_2,X_3]=X_2,
\qquad
[X_1,X_3]=X_1.
\tag{1.50}
$$

<div style="break-after: page;"></div>

This can be accomplished by changes in sign of $G_4$ and $G_9$. With these changes in sign, the three subalgebras become isomorphic to each other.

Let us recall the three triplets of generators (1.40), (1.41) and (1.42). We note that none of them contain operators that are connected to each other i.e. we do not have operators $X_1,X_2,X_3$ such that

$$
X_1=\rho(t,q)X_2
\qquad\text{and}\qquad
X_3=\psi(t,q)X_2
$$

for suitable functions $\rho$ and $\psi$. It follows therefore, from (1.50) that the triplets of generators are equivalent to each other under point transformation. Indeed, by inspection we observe that the $I_2$-generators are equivalent to the $I_3$-generators under the interchange transformation

$$
T=q,
\qquad
Q=t.
\tag{1.51}
$$

The $I_1$-generators are form-invariant under this transformation. By a straightforward calculation we can show that the $I_2$-generators transform into the $I_1$-generators via the point transformation

$$
T=-\frac1t,
\qquad
Q=\frac qt.
\tag{1.52}
$$

Hence using (1.51) together with (1.52) we can deduce that the transformation which reduces the $I_3$-generators to the $I_1$-generators is

$$
T=-\frac1q,
\qquad
Q=\frac tq.
\tag{1.53}
$$

Clearly the above transformations leave the free particle equation invariant.

We do not wish to labour the point in discussing the three-dimensional algebra (1.50) any further since a more general treatment pertaining to it is given in Chapter 5. However, we do remark that the two functionally independent first integrals (any two of $I_1,I_2,I_3$), sufficient for obtaining the complete solution of the free particle equation, each has associated a triplet of operators with isomorphic algebras. The complete solution is essentially provided by any two generators, each belonging to a different class of generators (any two of $I_1$-generators, $I_2$-generators, $I_3$-generators).

We wish to classify the Lie algebra and identify the full symmetry group for the free particle equation (1.29). The metric tensor of the Lie algebra is given by

$$
g_{ij}=C^m{}_{ik}C^k{}_{jm},
\tag{1.54}
$$

where $C^m{}_{ik}$ are the structure constants. Cartan's criterion for semi-simplicity requires that the determinant of $g_{ij}$ be non-vanishing (see e.g. Gilmore 1974). The

<div style="break-after: page;"></div>

metric tensor can be shown to satisfy this requirement. Indeed, the following eight linearly independent linear combinations of the nine operators (1.40), (1.41) and (1.42) are a basis for a Lie algebra having a diagonal metric tensor:

$$
\begin{aligned}
Y_1&=-(1+t^2)\frac{\partial}{\partial t}
      -tq\frac{\partial}{\partial q},\\[6pt]
Y_2&=(1-t^2)\frac{\partial}{\partial t}
      -tq\frac{\partial}{\partial q},\\[6pt]
Y_3&=tq\frac{\partial}{\partial t}
      +(1+q^2)\frac{\partial}{\partial q},\\[6pt]
Y_4&=tq\frac{\partial}{\partial t}
      +(q^2-1)\frac{\partial}{\partial q},\\[6pt]
Y_5&=-q\frac{\partial}{\partial t}
      +t\frac{\partial}{\partial q},\\[6pt]
Y_6&=-t\frac{\partial}{\partial t}
      -q\frac{\partial}{\partial q},\\[6pt]
Y_7&=t\frac{\partial}{\partial t}
      -q\frac{\partial}{\partial q},\\[6pt]
Y_8&=-q\frac{\partial}{\partial t}
      -t\frac{\partial}{\partial q}.
\end{aligned}
\tag{1.55}
$$

This follows from the work of Wulfman and Wybourne (1974) on the harmonic oscillator. The Lie algebra of the operators $Y_i$ is isomorphic to the Lie algebra of the harmonic oscillator operators which has diagonal metric. Moreover, the metric tensor is indefinite and so the Lie group generated by the Lie algebra is non-compact. It should also be noted that $\{Y_1,Y_3,Y_5\}$ constitutes a compact subalgebra, associated with a negative definite metric $g_{ij}=-2\delta_{ij}$, which generates a compact Lie group $SO(3)$.

We can select linear combinations of the $Y_i$ so that the Lie algebra is cast into the Cartan-Weyl standard form (Wybourne 1974), thus leading to its identification as a non-compact form of Cartan's $A_2$ algebra. The $A_2$ algebra can only generate the three Lie groups $SU(3)$, $SU(2,1)$ or $SL(3,\mathbb R)$. Since only the last is both non-compact and in possession of an $SO(3)$ subgroup, we identify the full symmetry group of the free particle equation (1.29) as the Lie group $SL(3,\mathbb R)$.

We may regard the free particle equation (1.29) as a canonical form for dynamical systems, linear as well as nonlinear, possessing the full symmetry group $SL(3,\mathbb R)$. Thus we can map any nonlinear differential equation having $SL(3,\mathbb R)$ symmetry into the free particle equation in a one-one manner. In view of this, the invariance properties of the free particle equation are injected into the nonlinear equation.

---

<nav aria-label="Section navigation" style="display: grid; grid-template-columns: minmax(0, 1fr) auto; column-gap: 2em; align-items: start;">
<div style="display: grid; grid-template-columns: 6em minmax(0, 1fr); row-gap: 0.25em;">
<span>NEXT:</span><a href="04-linear-second-order-odes.md">Linear second-order ODEs</a>
<span>PREVIOUS:</span><a href="02-linearisation-of-non-linear-odes.md">Linearisation of non-linear ODEs</a>
</div>
<a href="05-index.md" style="justify-self: end; text-align: right;">INDEX</a>
</nav>
