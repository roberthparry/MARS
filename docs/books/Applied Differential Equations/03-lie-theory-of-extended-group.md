<a id="lie-theory-of-extended-group"></a>
<h1 style="text-align: center;">1 LIE THEORY OF EXTENDED GROUP</h1>

In this chapter we outline the salient features of Lie theory which are of direct relevance to the following chapters. Other topics which enter naturally are discussed as the occasion arises.

<a id="ordinary-differential-equations"></a>
<h2 style="text-align: center;">1.1 Ordinary Differential Equations</h2>

In the study of the Lie theory of extended group, relevant to single second-order ordinary differential equations, a one-parameter point transformation acts on solution curves in $(t,q)$ space and transforms solution curves into solution curves which possess the same value of the associated invariant (Lutzky 1979). This is equivalent to the requirement that the equation be form-invariant under the transformation. The set of all such one-parameter transformations for a particular differential equation forms a group. For such a monoparametric transformation group, the group operator is

$\quad\begin{array}{l}\displaystyle G(t,q)=\xi(t,q)\frac{\partial}{\partial t} +\eta(t,q)\frac{\partial}{\partial q}. \qquad\text{(1.1)}\end{array}$

For a function involving the $n$th ($n=1,2$ in the present study) derivative, the $n$-times extended group operator is given by

$\quad\begin{array}{l}\displaystyle G^{(n)}=G^{(n-1)} +\eta^{(i)}\frac{\partial}{\partial q^{(i)}}, \qquad\text{(1.2)}\end{array}$

where

$\quad\begin{array}{l}\displaystyle \begin{aligned} \eta^{(k)} &=\frac{d\eta^{(k-1)}}{dt} -q^{(k)}\frac{d\xi}{dt}, \qquad k=1,2,\\[6pt] \frac d{dt} &=\frac{\partial}{\partial t} +\dot q\frac{\partial}{\partial q} +\ddot q\frac{\partial}{\partial\dot q}. \end{aligned} \qquad\text{(1.3)}\end{array}$

The finite transformations of the group can be obtained by exponentiation of the group operator

$\quad\begin{array}{l}\displaystyle \bar t=(\exp\alpha G)t, \qquad \bar q=(\exp\alpha G)q, \qquad\text{(1.4)}\end{array}$

where $\alpha$ is the group parameter, or by integration of the system of differential equations

$\quad\begin{array}{l}\displaystyle \frac{d\bar t}{\xi(\bar t,\bar q)} =\frac{d\bar q}{\eta(\bar t,\bar q)} =d\alpha, \qquad\text{(1.5)}\end{array}$

<div style="break-after: page;"></div>

subject to the initial conditions

$\quad\begin{array}{l}\displaystyle \bar t=t, \qquad \bar q=q \qquad\text{when}\qquad \alpha=0.\end{array}$

We obtain the infinitesimal transformation in $(t,q)$ space by using (1.4). Employing the infinitesimal notation $\delta\alpha$ for the value of $\alpha$ in the immediate neighbourhood of $\alpha=0$ and neglecting $O(\delta\alpha^2)$ terms, we can write (1.4) as

$\quad\begin{array}{l}\displaystyle \bar t=t+\xi\,\delta\alpha, \qquad \bar q=q+\eta\,\delta\alpha. \qquad\text{(1.6)}\end{array}$

The induced variations in the higher derivatives are likewise given by

$\quad\begin{array}{l}\displaystyle \bar q'=\dot q+\eta^{(1)}\delta\alpha, \qquad \bar q''=\ddot q+\eta^{(2)}\delta\alpha, \qquad\text{(1.7)}\end{array}$

where $'$ denotes $d/dt$. This is easily verified by utilising the finite transformations of the first and second extended group

$\quad\begin{array}{l}\displaystyle \bar q'=(\exp\alpha G^{(1)})\dot q, \qquad \bar q''=(\exp\alpha G^{(2)})\ddot q. \qquad\text{(1.8)}\end{array}$

For a general second-order differential equation

$\quad\begin{array}{l}\displaystyle N(\ddot q,\dot q,q,t)=0, \qquad\text{(1.9)}\end{array}$

the second extended operator $G^{(2)}$ is used. An operator $G$ is said to be the generator of a one-parameter symmetry group for (1.9) if, whenever (1.9) is satisfied,

$\quad\begin{array}{l}\displaystyle G^{(2)}N(\ddot q,\dot q,q,t)=0,\end{array}$

or equivalently

$\quad\begin{array}{l}\displaystyle \xi\frac{\partial N}{\partial t} +\eta\frac{\partial N}{\partial q} +\eta^{(1)}\frac{\partial N}{\partial\dot q} +\eta^{(2)}\frac{\partial N}{\partial\ddot q}=0. \qquad\text{(1.10)}\end{array}$

Writing

$\quad\begin{array}{l}\displaystyle N=\ddot q-M(\dot q,q,t), \qquad\text{(1.11)}\end{array}$

(1.10) becomes

$\quad\begin{array}{l}\displaystyle \eta^{(2)}-G^{(1)}M=0. \qquad\text{(1.12)}\end{array}$

Generally, a vector field

$\quad\begin{array}{l}\displaystyle Y=\xi(t,q,\dot q)\frac{\partial}{\partial t} +\eta(t,q,\dot q)\frac{\partial}{\partial q} +\zeta(t,q,\dot q)\frac{\partial}{\partial\dot q}\end{array}$

is said to be a dynamical symmetry of

$\quad\begin{array}{l}\displaystyle \Gamma=\frac{\partial}{\partial t} +\dot q\frac{\partial}{\partial q} +M\frac{\partial}{\partial\dot q}\end{array}$

<div style="break-after: page;"></div>

if

$\quad\begin{array}{l}\displaystyle \mathcal L_Y\Gamma=[Y,\Gamma]=g\Gamma, \qquad\text{(1.13)}\end{array}$

for a suitable function $g$. In this interpretation, the flow of $Y$ maps integral curves of $\Gamma$ into integral curves, subject to a change in the parametrization along the integral curves. Equation (1.13) gives rise to the following conditions (see e.g. Sarlet and Cantrijn 1981)

$\quad\begin{array}{l}\displaystyle \begin{aligned} \zeta&=\Gamma(\eta)-\dot q\,\Gamma(\xi),\\ \Gamma(\zeta)-M\Gamma(\xi)-Y(M)&=0,\\ g&=-\Gamma(\xi), \end{aligned} \qquad\text{(1.14)}\end{array}$

which in turn implies

$\quad\begin{array}{l}\displaystyle \Gamma^2(\eta)-\dot q\,\Gamma^2(\xi) -2M\Gamma(\xi)-Y(M)=0. \qquad\text{(1.15)}\end{array}$

This is precisely the criterion derived by Anderson and Davison (1974) in the context of generalization of the Lie theory. It is also of interest to note that (1.15) is identical to (1.12) when $\xi$ and $\eta$, satisfying (1.15), are independent of the velocity. In view of this, we say that $Y$ (actually $Y^{(0)}=\xi\,\partial/\partial t+\eta\,\partial/\partial q$) is a Lie point symmetry of $\Gamma$.

For given $\xi$ and $\eta$, a first integral may be obtained by imposing the double requirement

$\quad\begin{array}{l}\displaystyle G^{(1)}I(t,q,\dot q)=0, \qquad \frac d{dt}I(t,q,\dot q)=0. \qquad\text{(1.16)}\end{array}$

The function $I(t,q,\dot q)$ satisfying (1.16a) is said to be an invariant of the first extended group. Equation (1.16a) gives rise to the following system of ordinary differential equations

$\quad\begin{array}{l}\displaystyle \frac{dt}{\xi} =\frac{dq}{\eta} =\frac{d\dot q}{\eta^{(1)}}, \qquad\text{(1.17)}\end{array}$

the solution of which produces two characteristics $u(t,q)$ and $v(t,q,\dot q)$, referred to as the group invariant and the first order differential invariant respectively. Thus an arbitrary function $F(u,v)$ of $u$ and $v$ is the solution of (1.16a). Invoking (1.16b) leads to

$\quad\begin{array}{l}\displaystyle \frac{\partial F}{\partial u}\dot u +\frac{\partial F}{\partial v}\dot v=0,\end{array}$

where the dot denotes $d/dt$. Clearly we have

$\quad\begin{array}{l}\displaystyle \frac{dv}{du}=\frac{\dot v}{\dot u}. \qquad\text{(1.18)}\end{array}$

<div style="break-after: page;"></div>

If $M$ (as in (1.11)) is known, the right hand side of (1.18) can be written as a function of $u$ and $v$, say $G(u,v)$, and we have

$\quad\begin{array}{l}\displaystyle \frac{dv}{du}=G(u,v). \qquad\text{(1.19)}\end{array}$

A first integral of the differential equation (1.11) is then obtained by solving (1.19).

The preceding discussion also shows how the knowledge of a point symmetry enables the reduction of a given second-order differential equation to a first-order equation, namely (1.19). Furthermore, the general second-order equation invariant under a given group can be expressed as

$\quad\begin{array}{l}\displaystyle \frac{ \dfrac{\partial v}{\partial t} +\dot q\dfrac{\partial v}{\partial q} +\ddot q\dfrac{\partial v}{\partial\dot q} }{ \dfrac{\partial u}{\partial t} +\dot q\dfrac{\partial u}{\partial q} } =H\bigl(u(t,q),v(t,q,\dot q)\bigr), \qquad\text{(1.20)}\end{array}$

where $H$ is an arbitrary function and the left hand side is the quotient $\dot v/\dot u$ of (1.19).

The following remark is important: If a first integral $I$ (corresponding to $G$) is obtained by integration (in the above sense (1.16)), then it is possible to generate other first integrals and from these further first integrals until all operations $X^{(1)}I$ become meaningless, where $X^{(1)}$ is the first extension of $X\ne G$ belonging to the set of given operators. It is a simple matter to verify this. Indeed

$\quad\begin{array}{l}\displaystyle \Gamma(I)=0 \qquad\text{and}\qquad [X^{(1)},\Gamma]=g\Gamma\end{array}$

implies

$\quad\begin{array}{l}\displaystyle \Gamma(X^{(1)}I) =[\Gamma,X^{(1)}]I+X^{(1)}(\Gamma(I))=0,\end{array}$

so that $X^{(1)}I$ is potentially another first integral.

It is often desirable to introduce new coordinates $Q=F(t,q)$, $T=G(t,q)$ (frequently written as $Q=Q(t,q)$, $T=T(t,q)$ when no confusion arises) in which the generator (1.1) of the group (1.4) appears as a generator of time or space translation (or the generator of any other suitable group). In this respect we have

$\quad\begin{array}{l}\displaystyle \begin{aligned} \bar G(t,q) &=(GT)\frac{\partial}{\partial T} +(GQ)\frac{\partial}{\partial Q}\\ &=\bar\xi(T,Q)\frac{\partial}{\partial T} +\bar\eta(T,Q)\frac{\partial}{\partial Q}. \end{aligned} \qquad\text{(1.21)}\end{array}$

Thus

$\quad\begin{array}{l}\displaystyle \begin{aligned} \xi(t,q)\frac{\partial T}{\partial t} +\eta(t,q)\frac{\partial T}{\partial q} &=\bar\xi(T,Q),\\[6pt] \xi(t,q)\frac{\partial Q}{\partial t} +\eta(t,q)\frac{\partial Q}{\partial q} &=\bar\eta(T,Q), \end{aligned} \qquad\text{(1.22)}\end{array}$

<div style="break-after: page;"></div>

the solution of which explicitly yields the new coordinates $Q$ and $T$. If the group generated by (1.21) is one of translation, then $Q$ and $T$ are called canonical coordinates. This will be the case when either $\bar\xi=1$ and $\bar\eta=0$ (time-translation) or $\bar\xi=0$ and $\bar\eta=1$ (space-translation).

More details on the Lie theory of extended groups may be found in Dickson (1924) and Bluman and Cole (1974).

<a id="lie-algebra-extended-group"></a>
<h2 style="text-align: center;">1.2 Lie Algebra</h2>

A Lie algebra consists of a vector space $L$ over a field $F$, together with a binary operation of commutation $[\ ,\ ]$ defined on $L$ such that the following axioms are satisfied:

(a) bilinearity: for any $u,v,w\in L$ and $a,b\in F$

$\quad\begin{array}{l}\displaystyle \begin{aligned} [au+bv,w]&=a[u,w]+b[v,w],\\ [u,av+bw]&=a[u,v]+b[u,w]; \end{aligned}\end{array}$

(b) antisymmetry: for any $u,v\in L$

$\quad\begin{array}{l}\displaystyle [u,v]=-[v,u];\end{array}$

(c) the Jacobi identity: for any $u,v,w\in L$

$\quad\begin{array}{l}\displaystyle [[u,v],w]+[[v,w],u]+[[w,u],v]=0.\end{array}$

We shall (somewhat incorrectly) speak of the Lie algebra $L$, where we take $F=\mathbb R$. For our purpose, we define the binary operation of commutation on a Lie algebra $L$ of operators as the commutator

$\quad\begin{array}{l}\displaystyle [X,Y]=XY-YX \qquad\text{for any }X,Y\in L. \qquad\text{(1.23)}\end{array}$

If a differential equation admits the operators $X$ and $Y$ (in the sense mentioned previously), then it also admits their commutator $[X,Y]$ (Ovsiannikov 1978). Consequently the set of all operators admitted by a given differential equation generates a Lie algebra. The largest admitted Lie algebra is called the full Lie algebra of the equation. We encounter, in this work, only finite-dimensional Lie algebras of dimensionality $r$, where $r\leq8$. Thus it is usual to represent a finite-dimensional Lie algebra $L$ by a table of commutators, i.e., by an $r\times r$ matrix in which the commutator $[G_i,G_j]$ ($i,j=1,r$ where $\{G_k\}$ is the basis) is placed at the intersection of the $i$th row and $j$th column. The resulting matrix is antisymmetric and it

<div style="break-after: page;"></div>

is necessary to calculate only its upper half (see e.g. Mahomed and Leach 1985). Therefore we can write

$\quad\begin{array}{l}\displaystyle [G_i,G_j]=C^k{}_{ij}G_k, \qquad\text{(1.24)}\end{array}$

where the numbers $C^k{}_{ij}$ are called the structure constants of the Lie algebra $L$ with respect to the basis $\{G_k\}$. These numbers are antisymmetric relative to the lower indices ($C^k{}_{ij}=-C^k{}_{ji}$) and satisfy the Jacobi identity

$\quad\begin{array}{l}\displaystyle C^i{}_{jk}C^k{}_{lm} +C^i{}_{lk}C^k{}_{mj} +C^i{}_{mk}C^k{}_{jl}=0.\end{array}$

The above properties are useful in constructing a Lie algebra.

It is clear from the relation

$\quad\begin{array}{l}\displaystyle [G_i^{(n)},G_j^{(n)}]=[G_i,G_j]^{(n)}, \qquad n\in\mathbb N, \qquad\text{(1.25)}\end{array}$

that the $n$-times extended operators generate an $r$-dimensional Lie algebra, denoted $L^{(n)}$. Moreover $L^{(n)}$ has the same structure constants as $L$ when referred to basis $\{G_k^{(n)}\}$,

$\quad\begin{array}{l}\displaystyle [G_i^{(n)},G_j^{(n)}] =C^k{}_{ij}G_k^{(n)}. \qquad\text{(1.26)}\end{array}$

The classification of real low-dimensional Lie algebras was initiated by Lie (Lie 1891). It still engages scores of specialists (see e.g. Patera et al.). For many of them, the interest in studying the classification problem lies in its usefulness in many physical applications. Indeed, the operators admitted by a second-order differential equation generate a finite-dimensional Lie algebra of dimension at most eight. This is a direct consequence of Lie's counting theorem for second-order equations (Anderson and Davison 1974). Presently we concern ourselves with differential equations which admit two-dimensional Lie algebras of operators; deferring discussion on higher dimensional algebras to Chapter 5. There are two Lie algebras of dimension two, one abelian and one solvable (Barut and Raczka 1980):

$\quad\begin{array}{l}\displaystyle [G_1,G_2]=0, \qquad [G_1,G_2]=G_1. \qquad\text{(1.27)}\end{array}$

Lie showed that second-order ordinary differential equations possessing two generators of symmetry have four canonical forms (Lie 1891). They are, with their associated generators in canonical form, given in the table below.

It is observed that there are two canonical forms for each of the Lie algebras (1.27a) and (1.27b).

Lie deduced that if an equation admits a two-dimensional Lie algebra with operators $G_1,G_2$ satisfying $G_2=\rho(t,q)G_1$ (i.e. type 2 or 4) then it is linearizable. This is evident from the above table. Moreover a theorem of Lie states that every linear equation is reducible to the free particle equation. Hence an equation of type 2

<div style="break-after: page;"></div>

or 4 can be transformed to the free particle equation and accordingly admits six additional operators.

**REMARK:** If a second-order equation admits the operators $G_1$ and $G_2$ then it follows from (1.24) that

$\quad\begin{array}{l}\displaystyle [G_1,G_2]=aG_1+bG_2, \qquad a,b\in\mathbb R. \qquad\text{(1.28)}\end{array}$

For $a$ and $b$ both zero, $G_1$ and $G_2$ commute. However, when at least one of $a$ or $b$ is nonzero, we choose the basis $\{V_1,V_2\}$ so that $[V_1,V_2]=V_1$. This is easily done as follows: if $a\ne0$ and $b\ne0$ or $a\ne0$ and $b=0$, we introduce the basis $\{V_1=G_1+(b/a)G_2,V_2=(1/a)G_2\}$. In the case $a=0$ and $b\ne0$, we choose $\{V_1=G_2,V_2=-(1/b)G_1\}$. The original equation admits the operators $V_1$ and $V_2$ since they are merely linear combinations of $G_1$ and $G_2$.

The contents of Table 1 are treated in greater detail in the following chapters. Various new features emerge.

For readable accounts on Lie algebras, the interested reader is referred to Wybourne (1974), Gilmore (1974) and Ovsiannikov (1978).

<a id="the-free-particle"></a>
<h2 style="text-align: center;">1.3 The Free Particle</h2>

The free particle, being the simplest system, is regarded as a paradigm of dynamical systems. It has been discussed by various authors (Lie 1891, Anderson and Davison 1974). Certain features of the problem, however, have received attention only recently (Mahomed and Leach 1985). Conventionally the symmetry generators of a one-dimensional system are used to determine the first integrals associated with the system. In the last cited reference, the reverse procedure was adopted for the free particle.

The one-dimensional free particle has equation

$\quad\begin{array}{l}\displaystyle \ddot q=0, \qquad\text{(1.29)}\end{array}$

where the dot denotes $d/dt$, with Hamiltonian

$\quad\begin{array}{l}\displaystyle H=\frac12p^2, \qquad p=\dot q. \qquad\text{(1.30)}\end{array}$

The two first integrals for (1.29) are easily seen to be

$\quad\begin{array}{l}\displaystyle \begin{aligned} I_1&=p,\\ I_2&=q-tp. \end{aligned} \qquad\text{(1.31)}\end{array}$

<div style="break-after: page;"></div>

and we include their quotient

$\quad\begin{array}{l}\displaystyle I_3=\frac{q-tp}{p}, \qquad\text{(1.32)}\end{array}$

since this has been shown by Leach (1980) to be relevant from the corresponding integral for the simple harmonic oscillator.

We seek the set of symmetry generators with which each of $I_1,I_2,I_3$ is associated.

$\xi$ and $\eta$ of the generator

$\quad\begin{array}{l}\displaystyle G=\xi(t,q)\frac{\partial}{\partial t} +\eta(t,q)\frac{\partial}{\partial q} +\zeta(t,q,p)\frac{\partial}{\partial p} \qquad\text{(1.33)}\end{array}$

are determined by the following equations

$\quad\begin{array}{l}\displaystyle \zeta\frac{\partial I}{\partial p} +\eta\frac{\partial I}{\partial q} +\xi\frac{\partial I}{\partial t}=0, \qquad\text{(1.34)}\end{array}$

$\quad\begin{array}{l}\displaystyle \eta^{(1)} -\zeta\frac{\partial^2H}{\partial p^2} -\eta\frac{\partial^2H}{\partial q\partial p} -\xi\frac{\partial^2H}{\partial t\partial p}=0, \qquad\text{(1.35)}\end{array}$

where

$\quad\begin{array}{l}\displaystyle \eta^{(1)}=\dot\eta-\dot\xi\frac{\partial H}{\partial p}, \qquad\text{(1.36)}\end{array}$

by eliminating $\zeta$ between (1.34) and (1.35) and insisting that $\xi$ and $\eta$ be independent of $p$ (Leach 1980).

Thus for the first integral $I_1=p$ we obtain

$\quad\begin{array}{l}\displaystyle \frac{d\eta}{dt}-p\frac{d\xi}{dt}=0, \qquad\text{(1.37)}\end{array}$

where

$\quad\begin{array}{l}\displaystyle \frac d{dt}=\frac{\partial}{\partial t} +p\frac{\partial}{\partial q}.\end{array}$

(1.37) yields a partial differential equation in which the terms are grouped together in powers of $p$. By equating coefficients of separate powers of $p$ to zero we obtain

$\quad\begin{array}{l}\displaystyle \begin{aligned} p^2:&\qquad \xi=a(t),\\ p^1:&\qquad \eta=\dot a q+b(t), \end{aligned} \qquad\text{(1.38)}\end{array}$

and

$\quad\begin{array}{l}\displaystyle \begin{aligned} p^0:&\qquad a=At+B,\\ &\qquad b=C, \end{aligned} \qquad\text{(1.39)}\end{array}$

<div style="break-after: page;"></div>

where $A,B$ and $C$ are constants.

Therefore the triplet of generators with which $I_1$ is associated is

$\quad\begin{array}{l}\displaystyle \begin{aligned} G_1&=t\frac{\partial}{\partial t} +q\frac{\partial}{\partial q}, &G_2&=\frac{\partial}{\partial t},\\[6pt] G_3&=\frac{\partial}{\partial q}. \end{aligned} \qquad\text{(1.40)}\end{array}$

In like manner, for $I_2$, we obtain

$\quad\begin{array}{l}\displaystyle \begin{aligned} G_4&=t\frac{\partial}{\partial t}, &G_5&=t^2\frac{\partial}{\partial t} +tq\frac{\partial}{\partial q},\\[6pt] G_6&=t\frac{\partial}{\partial q}. \end{aligned} \qquad\text{(1.41)}\end{array}$

Similarly, for $I_3$,

$\quad\begin{array}{l}\displaystyle \begin{aligned} G_7&=tq\frac{\partial}{\partial t} +q^2\frac{\partial}{\partial q}, &G_8&=q\frac{\partial}{\partial t},\\[6pt] G_9&=q\frac{\partial}{\partial q}. \end{aligned} \qquad\text{(1.42)}\end{array}$

It is observed that $G_1,G_4$ and $G_9$ are linearly dependent. Indeed we have

$\quad\begin{array}{l}\displaystyle G_1=G_4+G_9. \qquad\text{(1.43)}\end{array}$

We now compare the standard generators (Lie 1891) of the free particle with those obtained here using the reverse procedure. In order to do this, we need to determine the usual free particle generators using the (direct) Lie method of extended group (see Section 1.1).

An operator of the form (1.1) will be a symmetry generator for (1.29) if and only if $\xi$ and $\eta$ satisfy (invariance of (1.29) under $G^{(2)}$)

$\quad\begin{array}{l}\displaystyle \eta^{(2)}=0, \qquad\text{(1.44)}\end{array}$

which, when different powers of $\dot q$ are treated as linearly independent (as they are, since we have a second-order equation) yields, on equating the coefficients of separate powers of $\dot q$ to zero (bearing in mind that $\xi$ and $\eta$ are functions of $t$ and $q$ only), for the third and second powers

$\quad\begin{array}{l}\displaystyle \dot q^3: \qquad \xi=a(t)q+b(t), \qquad\text{(1.45)}\end{array}$

and

$\quad\begin{array}{l}\displaystyle \dot q^2: \qquad \eta=\dot a q^2+c(t)q+d(t), \qquad\text{(1.46)}\end{array}$

<div style="break-after: page;"></div>

where the time-dependent functions $a,b,c,$ and $d$ are solutions of

$\quad\begin{array}{l}\displaystyle \begin{aligned} \dot q^1:&\qquad \ddot a=0, &2\dot c-\ddot b&=0,\\[4pt] \dot q^0:&\qquad \ddot c=0, &\ddot d&=0. \end{aligned} \qquad\text{(1.47)}\end{array}$

We note that this system of equations has eight linearly independent solutions. Hence (1.29) has eight generators of symmetry which correspond identically to the generators $G_2,G_3,\ldots,G_9$ obtained via the reverse procedure.

To illustrate how one obtains a first integral for a given generator, we solve equations (1.16) for $G_4$. The first of (1.16) leads to (cf. (1.17))

$\quad\begin{array}{l}\displaystyle \frac{dt}{t} =\frac{d\dot q}{-\dot q} =\frac{dq}{0}, \qquad\text{(1.48)}\end{array}$

the solution of which produces the characteristics

$\quad\begin{array}{l}\displaystyle u=q, \qquad v=t\dot q.\end{array}$

Applying equation (1.18) we then obtain

$\quad\begin{array}{l}\displaystyle \frac{dv}{du}=1. \qquad\text{(1.49)}\end{array}$

Whence the first integral associated with $G_4$ is

$\quad\begin{array}{l}\displaystyle I_2=q-t\dot q,\end{array}$

as expected. In the same way one can obtain the first integrals $I_1$ and $I_2$ corresponding to the generators given previously.

Of great interest to us are the Lie algebraic properties of the nine generators which in some sense are simpler than those for the usual eight as we shall see below. The commutation relations between the $G_i$'s are given in the following table.

The above table cannot be regarded in the usual sense since there is a linear dependence relation between $G_1,G_4$ and $G_9$. However, there are certain nice features that can be noted as we shall soon discuss. Moreover, the conventional table is still available if we disregard the entries $[G_1,G_j]$ and $[G_i,G_1]$ ($i,j=1,9$).

Each of the triplets of generators $\{G_1,G_2,G_3\}$, $\{G_4,G_5,G_6\}$ and $\{G_7,G_8,G_9\}$, associated with $I_1,I_2$ and $I_3$ respectively, forms a subalgebra. This is evident from the table (see diagonal blocks). Furthermore, each of these subalgebras can be written in the form

$\quad\begin{array}{l}\displaystyle [X_1,X_2]=0, \qquad [X_2,X_3]=X_2, \qquad [X_1,X_3]=X_1. \qquad\text{(1.50)}\end{array}$

<div style="break-after: page;"></div>

This can be accomplished by changes in sign of $G_4$ and $G_9$. With these changes in sign, the three subalgebras become isomorphic to each other.

Let us recall the three triplets of generators (1.40), (1.41) and (1.42). We note that none of them contain operators that are connected to each other i.e. we do not have operators $X_1,X_2,X_3$ such that

$\quad\begin{array}{l}\displaystyle X_1=\rho(t,q)X_2 \qquad\text{and}\qquad X_3=\psi(t,q)X_2\end{array}$

for suitable functions $\rho$ and $\psi$. It follows therefore, from (1.50) that the triplets of generators are equivalent to each other under point transformation. Indeed, by inspection we observe that the $I_2$-generators are equivalent to the $I_3$-generators under the interchange transformation

$\quad\begin{array}{l}\displaystyle T=q, \qquad Q=t. \qquad\text{(1.51)}\end{array}$

The $I_1$-generators are form-invariant under this transformation. By a straightforward calculation we can show that the $I_2$-generators transform into the $I_1$-generators via the point transformation

$\quad\begin{array}{l}\displaystyle T=-\frac1t, \qquad Q=\frac qt. \qquad\text{(1.52)}\end{array}$

Hence using (1.51) together with (1.52) we can deduce that the transformation which reduces the $I_3$-generators to the $I_1$-generators is

$\quad\begin{array}{l}\displaystyle T=-\frac1q, \qquad Q=\frac tq. \qquad\text{(1.53)}\end{array}$

Clearly the above transformations leave the free particle equation invariant.

We do not wish to labour the point in discussing the three-dimensional algebra (1.50) any further since a more general treatment pertaining to it is given in Chapter 5. However, we do remark that the two functionally independent first integrals (any two of $I_1,I_2,I_3$), sufficient for obtaining the complete solution of the free particle equation, each has associated a triplet of operators with isomorphic algebras. The complete solution is essentially provided by any two generators, each belonging to a different class of generators (any two of $I_1$-generators, $I_2$-generators, $I_3$-generators).

We wish to classify the Lie algebra and identify the full symmetry group for the free particle equation (1.29). The metric tensor of the Lie algebra is given by

$\quad\begin{array}{l}\displaystyle g_{ij}=C^m{}_{ik}C^k{}_{jm}, \qquad\text{(1.54)}\end{array}$

where $C^m{}_{ik}$ are the structure constants. Cartan's criterion for semi-simplicity requires that the determinant of $g_{ij}$ be non-vanishing (see e.g. Gilmore 1974). The

<div style="break-after: page;"></div>

metric tensor can be shown to satisfy this requirement. Indeed, the following eight linearly independent linear combinations of the nine operators (1.40), (1.41) and (1.42) are a basis for a Lie algebra having a diagonal metric tensor:

$\quad\begin{array}{l}\displaystyle \begin{aligned} Y_1&=-(1+t^2)\frac{\partial}{\partial t} -tq\frac{\partial}{\partial q},\\[6pt] Y_2&=(1-t^2)\frac{\partial}{\partial t} -tq\frac{\partial}{\partial q},\\[6pt] Y_3&=tq\frac{\partial}{\partial t} +(1+q^2)\frac{\partial}{\partial q},\\[6pt] Y_4&=tq\frac{\partial}{\partial t} +(q^2-1)\frac{\partial}{\partial q},\\[6pt] Y_5&=-q\frac{\partial}{\partial t} +t\frac{\partial}{\partial q},\\[6pt] Y_6&=-t\frac{\partial}{\partial t} -q\frac{\partial}{\partial q},\\[6pt] Y_7&=t\frac{\partial}{\partial t} -q\frac{\partial}{\partial q},\\[6pt] Y_8&=-q\frac{\partial}{\partial t} -t\frac{\partial}{\partial q}. \end{aligned} \qquad\text{(1.55)}\end{array}$

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
