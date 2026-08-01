<a id="linear-second-order-odes"></a>
## Linear second-order ODEs

<a id="factorisation-technique"></a>
### Factorisation technique

For example,

$$
y''+y=0.
$$

$$
\begin{aligned}
(D^2+1)y&=0,\\
(D+i)(D-i)y&=0.
\end{aligned}
$$

Let

$$
(D-i)y=z.
$$

$$
(D+i)z=0,
$$

$$
z'+iz=0.
$$

Integrating factor: $e^{ix}$.

$$
(ze^{ix})'=0,
$$

$$
ze^{ix}=-2iB.
$$

$$
z=-2iBe^{-ix}.
$$

$$
y'-iy=-2iBe^{-ix}.
$$

Integrating factor: $e^{-ix}$.

$$
(ye^{-ix})'=-2iBe^{-2ix}.
$$

$$
ye^{-ix}=A+Be^{-2ix},
$$

$$
y=Ae^{ix}+Be^{-ix},
$$

or

$$
y=C\sin x+D\cos x.
$$

Equally could use

$$
(D-i)(D+i)y=0.
$$

For a less trivial example consider

$$
y''-(x^2+1)y=0.
$$

$$
(D+x)(D-x)y=0.
$$

Note this is not the same as

$$
(D-x)(D+x)=D^2-(x^2-1).
$$

Let

$$
z=(D-x)y.
$$

$$
(D+x)z=0.
$$

Integrating factor: $e^{x^2/2}$.

$$
(ze^{x^2/2})'=0,
$$

$$
z=Be^{-x^2/2}.
$$

$$
(D-x)y=Be^{-x^2/2}.
$$

Integrating factor: $e^{-x^2/2}$.

$$
y=Ae^{x^2/2}+Be^{x^2/2}\int e^{-x^2}\,dx.
$$

Consider

$$
y''-f(x)y=0
$$

and the factorisation

$$
(D+\alpha(x))(D+\beta(x)).
$$

$$
(D+\alpha)(D+\beta)
=D^2+(\alpha+\beta)D+\alpha\beta+\beta',
$$

$$
\beta=-\alpha,
\qquad
\alpha'+\alpha^2=f.
$$

where $\alpha'+\alpha^2=f$ is Riccati's equation.

$$
(D+\alpha)(D-\alpha)y=0,
$$

$$
z=(D-\alpha)y.
$$

$$
z=B e^{\int\alpha\,dx},
$$

leads to

$$
y=Ae^{-\int\alpha\,dx}
  +Be^{-\int\alpha\,dx}\int e^{2\int\alpha\,dx}\,dx.
$$

<a id="existence-of-solutions"></a>
### Existence of solutions

Initial value problem:

$$
y''-f(x)y=0,
\qquad
y(0)=\alpha,
\qquad
y'(0)=\beta,
$$

has a continuous solution with continuous derivative if $f(x)$ is
continuous.

Boundary value problem:

$$
y''-f(x)y=0,
$$

$$
\begin{aligned}
\alpha_{11}y(a)+\alpha_{12}y'(a)
 +\beta_{11}y(b)+\beta_{12}y'(b)&=0,\\
\alpha_{21}y(a)+\alpha_{22}y'(a)
 +\beta_{21}y(b)+\beta_{22}y'(b)&=0.
\end{aligned}
$$

where the two conditions are independent. This may or may not have a
non-trivial solution. For example,

$$
y''+y=0,
\qquad
y(0)=0,
\qquad
y'(\pi)=0.
$$

$$
y=A\sin x+B\cos x,
$$

$$
y(0)=0\quad\Longrightarrow\quad B=0,
$$

and

$$
y'(\pi)=0\quad\Longrightarrow\quad -A=0.
$$

There is no non-trivial solution.

However, consider

$$
y''+\lambda y=0,
\qquad
y(0)=0,
\qquad
y'(\pi)=0.
$$

For $\lambda=-\omega^2$,

$$
y=A\sinh\omega x+B\cosh\omega x,
$$

$$
y(0)=0\quad\Longrightarrow\quad B=0,
$$

$$
y'(\pi)=0\quad\Longrightarrow\quad A=0.
$$

For $\lambda=0$,

$$
y=Ax+B,
$$

$$
y(0)=0\quad\Longrightarrow\quad B=0,
$$

$$
y'(\pi)=0\quad\Longrightarrow\quad A=0.
$$

For $\lambda=\omega^2$,

$$
y=A\sin\omega x+B\cos\omega x.
$$

$$
y(0)=0\quad\Longrightarrow\quad B=0,
$$

$$
y'(\pi)=0\quad\Longrightarrow\quad \omega A\cos(\omega\pi)=0.
$$

that is,

$$
\cos\omega\pi=0,
$$

$$
\omega=n+\frac12.
$$

Solution is

$$
y=A\sin\left(n+\frac12\right)x.
$$

$n+\frac12$ is called the eigenvalue and
$\sin\left(n+\frac12\right)x$ is called the eigenfunction, sometimes
the eigenstate.

<a id="standard-eigenvalue-form"></a>
### Standard form of the eigenvalue differential equation

Given

$$
\frac{d}{dx}\left(f\frac{dy}{dx}\right)+gy+\lambda hy=0,
$$

where $f$, $g$, and $h$ are functions of $x$, this can be transformed
to the standard form

$$
\frac{d^2Y}{dX^2}+F(X)Y+\lambda Y=0
$$

by the transformation

$$
Y=(fh)^{1/4}y,
\qquad
dX=\left(\frac hf\right)^{1/2}dx.
$$

$f$ and $h$ must be of the same sign and $h/f$ must exist everywhere
in the domain of applicability of the equation. The standard form is
called the normal form.

<a id="harmonic-oscillator"></a>
### Example

$$
y''-x^2y+\lambda y=0,
$$

Write as

$$
Hy=\lambda y,
\qquad
H=x^2-D^2.
$$

Let

$$
a=x+D,
\qquad
a^\dagger=x-D.
$$

Using $\alpha=x$ and $\alpha=-x$ gives

$$
\begin{aligned}
(x-D)(x+D)y&=(\lambda+1)y,\\
(x+D)(x-D)y&=(\lambda-1)y.
\end{aligned}
$$

$$
\begin{aligned}
[a,a^\dagger]
 &=(x+D)(x-D)-(x-D)(x+D)\\
 &=x^2+1-D^2-(x^2-1-D^2)\\
 &=2,
\end{aligned}
$$

$$
\begin{aligned}
[H,a]
 &=(x^2-D^2)(x+D)-(x+D)(x^2-D^2)\\
 &=x^3+x^2D-D^2x-D^3
   -x^3+xD^2-Dx^2+D^3\\
 &=x^2D-D^2x+xD^2-Dx^2\\
 &=-2x-2D\\
 &=-2a,
\end{aligned}
$$

and

$$
\begin{aligned}
[H,a^\dagger]
 &=(x^2-D^2)(x-D)-(x-D)(x^2-D^2)\\
 &=x^3-x^2D-D^2x+D^3
   -x^3+xD^2+Dx^2-D^3\\
 &=-x^2D-D^2x+xD^2+Dx^2\\
 &=2x-2D\\
 &=2a^\dagger.
\end{aligned}
$$

If we include the identity, we have the algebra with elements
$H,a,a^\dagger,1$ such that

$$
[H,a]=-2a,\qquad
[H,a^\dagger]=2a^\dagger,\qquad
[a,a^\dagger]=2\,1,\qquad
[H,1]=0,
$$

etc.

Suppose $\psi$ is a solution of
$\psi''+(\lambda-x^2)\psi=0$ corresponding to the eigenvalue
$\lambda$. Then, denoting it by $\psi_\lambda$, we have

$$
H\psi_\lambda=\lambda\psi_\lambda.
$$

Then

$$
\begin{aligned}
H(a\psi_\lambda)
 &=(Ha)\psi_\lambda\\
 &=([H,a]+aH)\psi_\lambda\\
 &=-2a\psi_\lambda+a\lambda\psi_\lambda\\
 &=(\lambda-2)a\psi_\lambda,
\end{aligned}
$$

that is, $a\psi_\lambda$ is the eigenfunction corresponding to the
eigenvalue $\lambda-2$. Likewise with $a^\dagger$,

$$
\begin{aligned}
H(a^\dagger\psi_\lambda)
 &=(Ha^\dagger)\psi_\lambda\\
 &=([H,a^\dagger]+a^\dagger H)\psi_\lambda\\
 &=2a^\dagger\psi_\lambda+a^\dagger\lambda\psi_\lambda\\
 &=(\lambda+2)a^\dagger\psi_\lambda,
\end{aligned}
$$

that is, $a^\dagger\psi_\lambda$ is the eigenfunction corresponding
to eigenvalue $\lambda+2$. $a$ and $a^\dagger$ are called ladder
operators. $a$ is also called a lowering operator and $a^\dagger$ a
raising operator.

Assume that $\psi_\lambda$ is $L^2$ over $(-\infty,\infty)$, that is,

$$
\|\psi_\lambda\|^2
=\int_{-\infty}^{\infty}|\psi_\lambda|^2\,dx<\infty.
$$

$$
\begin{aligned}
(\psi_\lambda,x\psi_\lambda)
 &=\int_{-\infty}^{\infty}\psi_\lambda^*x\psi_\lambda\,dx\\
 &=\int_{-\infty}^{\infty}(x\psi_\lambda)^*\psi_\lambda\,dx\\
 &=(x\psi_\lambda,\psi_\lambda),
\end{aligned}
$$

and

$$
\begin{aligned}
(\psi_\lambda,D\psi_\lambda)
 &=\int_{-\infty}^{\infty}\psi_\lambda^*\psi_\lambda'\,dx\\
 &=\left[\psi_\lambda^*\psi_\lambda\right]_{-\infty}^{\infty}
   -\int_{-\infty}^{\infty}(\psi_\lambda^*)'\psi_\lambda\,dx\\
 &=-(D\psi_\lambda,\psi_\lambda),
\end{aligned}
$$

since $D$ is a real operator.

Hence

$$
\begin{aligned}
(\psi,a\psi)&=(a^\dagger\psi,\psi),\\
(\psi,a^\dagger\psi)&=(a\psi,\psi).
\end{aligned}
$$

We can write

$$
H=a^\dagger a+1,
$$

so

$$
a^\dagger a=H-1,
$$

$$
(\psi_\lambda,a^\dagger a\psi_\lambda)
=(\psi_\lambda,(\lambda-1)\psi_\lambda)
=(\lambda-1)\|\psi_\lambda\|^2\geq0.
$$

where equality is attained only if $\lambda=1$, since $\psi_\lambda$
is assumed to be non-trivial (that is, not identically zero). However,

$$
(\psi_\lambda,a^\dagger a\psi_\lambda)
=(a\psi_\lambda,a\psi_\lambda)
=\|a\psi_\lambda\|^2
=(\lambda-1)\|\psi_\lambda\|^2.
$$

So if $\lambda=1$, then $\|a\psi_\lambda\|^2=0$, that is,

$$
a\psi_\lambda=0.
$$

that is,

$$
(x+D)y=0,
$$

$$
xy+y'=0.
$$

Integrating factor:

$$
\left(ye^{x^2/2}\right)'=0,
$$

$$
y=\text{constant}\,e^{-x^2/2}.
$$

To obtain the constant $K$, impose a value on the norm, usually one:

$$
1=|K|^2\int_{-\infty}^{\infty}e^{-x^2}\,dx
 =|K|^2\sqrt\pi,
$$

so $K=\pi^{-1/4}$.

Call this solution

$$
\psi_0=e^{-x^2/2}
$$

(forget about the constant).

$$
a^\dagger\psi_0
=\left(x-\frac d{dx}\right)e^{-x^2/2}
=2x e^{-x^2/2},
$$

$$
\lambda=1+2=3.
$$

Call this $\psi_1$. Likewise,

$$
\psi_2=a^\dagger\psi_1=(4x^2-2)e^{-x^2/2},
$$

$$
\lambda=3+2=5.
$$

In general,

$$
\psi_n=(a^\dagger)^n\psi_0
=\left(x-\frac d{dx}\right)^n e^{-x^2/2}.
\qquad (*)
$$

and

$$
\lambda_n=2n+1.
$$

$\psi_n$ has the form

$$
\psi_n=H_n(x)e^{-x^2/2}.
\qquad (**)
$$

(Note: normalisation is neglected.)

where $H_n(x)$ is the $n$th Hermite polynomial. From $(*)$ and $(**)$
we obtain the Rodrigues formula for the Hermite polynomials:

$$
H_n(x)=e^{x^2/2}
\left(x-\frac d{dx}\right)^n e^{-x^2/2}.
$$

Note: $a^\dagger a$ is an example of an Hermitian operator, for

$$
\begin{aligned}
(\phi,a^\dagger a\psi)
 &=\int_{-\infty}^{\infty}\phi^*(a^\dagger a\psi)\,dx\\
 &=\int_{-\infty}^{\infty}(a\phi)^*(a\psi)\,dx\\
 &=(a\phi,a\psi)\\
 &=(a^\dagger a\phi,\psi).
\end{aligned}
$$

The eigenvalues of an Hermitian operator are real. Let $O$ be an
Hermitian operator:

$$
O\psi=\lambda\psi,
\qquad
O\phi=\mu\phi,
$$

then

$$
\begin{aligned}
(O\phi,\psi)&=\mu^*(\phi,\psi),\\
(\phi,O\psi)&=\lambda(\phi,\psi).
\end{aligned}
$$

Now $(\phi,O\psi)=(O\phi,\psi)$, hence

$$
(\lambda-\mu^*)(\phi,\psi)=0.
$$

If $\phi=\psi$, then
$(\lambda-\bar\lambda)\|\psi\|^2=0$, that is,
$\lambda=\bar\lambda$, so it is real. If $\phi\ne\psi$ and
$\lambda\ne\mu$, then $(\lambda-\mu)(\phi,\psi)=0$, that is,
$(\phi,\psi)=0$, that is, states are orthogonal.

Hence we have an infinite-dimensional function space spanned by the
set of functions $\psi_n(x)=H_n(x)e^{-x^2/2}$, and these are
orthogonal. This makes the Hermite polynomials a suitable basis for
functions over $(-\infty,\infty)$. That is, one writes a function
$f(x)$ as a Fourier--Hermite series

$$
f(x)=\sum_{n=0}^{\infty}a_nH_n(x)e^{-x^2/2}.
$$

<a id="orthogonal-polynomials"></a>
### Orthogonal polynomials

Orthogonality:

$$
\int_a^b w(x)f_m(x)f_n(x)\,dx=h_m\delta_{mn},
$$

where $w(x)$ is a weight function, $h_m$ is some constant and
$\delta_{mn}$ is the Kronecker delta. Often take $h_m=1$, but
sometimes other values are more convenient.

**Differential equation**

$$
\left[g_2(x)D^2+g_1(x)D+\lambda_n\right]f_n=0,
\qquad \lambda_n=\lambda(n).
$$

**Recurrence**

$$
f_{n+1}=(a_n+xb_n)f_n-c_nf_{n-1}.
$$

**Rodrigues' formula**

$$
f_n(x)=\frac1{e_nw(x)}
\frac{d^n}{dx^n}\left\{w(x)[g(x)]^n\right\}.
$$

where $e_n$ is a constant and $w(x)$ is the weight function (not
always found).

**Generating function**

$$
g(x,z)=\sum_{n=0}^{\infty}a_nf_n(x)z^n.
$$

**Differential relations**

$$
m_2(x)\frac{df_n}{dx}=m_1(x)f_n+m_0(x)f_{n-1}.
$$

**Integral representation**

$$
f_n(x)=\frac{p_0(x)}{2\pi i}
\oint_\Gamma[p_1(z,x)]^n p_2(z,x)\,dz.
$$

where $\Gamma$ is a closed contour taken around some specified point
in the positive sense.

Consider Hermite polynomials.

$$
H_n''-2xH_n'+2nH_n=0,
$$

$H_n(x)$ satisfies the above equation, and
$K_n=H_ne^{-x^2/2}$ satisfies

$$
K_n''+(2n+1-x^2)K_n=0.
$$

Orthogonality already established.

Rodrigues' formula:

Had

$$
H_n(x)=e^{x^2/2}\left(x-\frac d{dx}\right)^n e^{-x^2/2},
$$

This is not in standard form. The standard form is

$$
H_n(x)=e^{x^2}\left(-\frac d{dx}\right)^n e^{-x^2}.
$$

Obviously correct for $n=0$. Let $n=1$:

$$
\begin{aligned}
e^{x^2/2}\left(x-\frac d{dx}\right)e^{-x^2/2}
 &=e^{x^2/2}\left(x-\frac d{dx}\right)
   \left(e^{x^2/2}e^{-x^2}\right)\\
 &=e^{x^2/2}
   \left[\left(x-\frac d{dx}\right)e^{x^2/2}\right]e^{-x^2}
   +e^{x^2}\left(-\frac d{dx}\right)e^{-x^2}\\
 &=e^{x^2/2}\left[(x-x)e^{x^2/2}\right]e^{-x^2}
   +e^{x^2}\left(-\frac d{dx}\right)e^{-x^2}\\
 &=e^{x^2}\left(-\frac d{dx}\right)e^{-x^2}.
\end{aligned}
$$

That is, it is true for $n=1$. Assume it is true for $n=k$. Then for
$n=k+1$,

$$
\begin{aligned}
e^{x^2/2}\left(x-\frac d{dx}\right)^{k+1}e^{-x^2/2}
 &=e^{x^2/2}\left(x-\frac d{dx}\right)
   e^{-x^2/2}\left(-\frac d{dx}\right)^k e^{-x^2}\\
 &=\left[e^{x^2/2}\left(x-\frac d{dx}\right)e^{x^2/2}\right]
   \left(-\frac d{dx}\right)^k e^{-x^2}\\
 &=e^{x^2}\left(-\frac d{dx}\right)^{k+1}e^{-x^2}.
\end{aligned}
$$

Q.E.D.

Recurrence formula:

$$
\begin{aligned}
H_{n+1}(x)
 &=e^{x^2}\left(-\frac d{dx}\right)^{n+1}e^{-x^2}\\
 &=e^{x^2}\left(-\frac d{dx}\right)^n
   \left(2xe^{-x^2}\right).
\end{aligned}
$$

Using Leibniz's rule,

$$
\begin{aligned}
H_{n+1}(x)
 &=e^{x^2}\sum_{j=0}^{n}\binom nj
   \left[\left(-\frac d{dx}\right)^j(2x)\right]
   \left[\left(-\frac d{dx}\right)^{n-j}e^{-x^2}\right]\\
 &=e^{x^2}\binom n0(2x)
   \left(-\frac d{dx}\right)^n e^{-x^2}
   +e^{x^2}\binom n1(-2)
   \left(-\frac d{dx}\right)^{n-1}e^{-x^2}\\
 &=2xH_n(x)-2nH_{n-1}(x).
\end{aligned}
$$

$$
H_{n+1}=2xH_n-2nH_{n-1},
$$

Differential recurrence relations:

$$
\begin{aligned}
\frac{dH_n}{dx}
 &=\frac d{dx}\left[
   e^{x^2}\left(-\frac d{dx}\right)^n e^{-x^2}
   \right]\\
 &=2x e^{x^2}\left(-\frac d{dx}\right)^n e^{-x^2}
   -e^{x^2}\left(-\frac d{dx}\right)^{n+1}e^{-x^2}\\
 &=2xH_n-H_{n+1}\\
 &=2xH_n-\left(2xH_n-2nH_{n-1}\right)\\
 &=2nH_{n-1}.
\end{aligned}
$$

$$
H_n'=2nH_{n-1}.
$$

Generating function:

$$
\begin{aligned}
f(x,z)
 &=\sum_{n=0}^{\infty}a_nH_n(x)z^n\\
 &=e^{x^2}\left\{
   \sum_{n=0}^{\infty}a_n
   \left(-z\frac\partial{\partial x}\right)^n
   \right\}e^{-x^2}.
\end{aligned}
$$

Let $a_n=1/n!$:

$$
\begin{aligned}
f(x,z)
 &=e^{x^2}
   \exp\left(-z\frac\partial{\partial x}\right)e^{-x^2}\\
 &=e^{x^2}\exp\left\{
   -x^2+\left[-z\frac\partial{\partial x},-x^2\right]
   +\frac1{2!}\left[-z\frac\partial{\partial x},
      \left[-z\frac\partial{\partial x},-x^2\right]\right]
   +\cdots\right\}\\
 &=e^{2xz-z^2},
\end{aligned}
$$

*[This uses a Baker--Campbell--Hausdorff formula.]*

$$
\sum_{n=0}^{\infty}H_n(x)\frac{z^n}{n!}
=e^{2xz-z^2}.
$$

Integral representation:

we have

$$
\frac{e^{2xz-z^2}}{z^{m+1}}
=\sum_{n=0}^{\infty}\frac1{n!}H_n(x)z^{n-m-1}.
$$

The right-hand side is now a Laurent series about $z=0$.

(A Laurent series $\sum_{n=-k}^{\infty}a_nz^n$, that is, negative
powers.)

The coefficient of $z^{-1}$ is the residue at $z=0$, and

$$
\oint_\Gamma\frac{e^{2xz-z^2}}{z^{m+1}}\,dz
=2\pi i\frac{H_m(x)}{m!},
$$

if $\Gamma$ encloses zero. The coefficient of $z^{-1}$ is

$$
\frac1{m!}H_m(x),
$$

so

$$
H_n(x)=\frac{n!}{2\pi i}
\oint_\Gamma\frac{e^{2xz-z^2}}{z^{n+1}}\,dz.
$$


<figure data-chart="residue-contour" style="margin: 1.5em auto; max-width: 23rem;">
  <img src="img/residue-contour.svg"
       alt="A positively oriented closed contour Gamma enclosing the poles z one, z two and z three."
       style="display: block; width: 100%; height: auto;" />
</figure>

$f(z)$ has poles at $z_1$, $z_2$ and $z_3$.

Near $z_1$ (say),

$$
f(z)\sim(z-z_1)^{-m}g(z),
$$

where $g(z_1)$ is finite.

$$
\oint_\Gamma f(z)\,dz
=2\pi i\left[
\operatorname{Res}(z_1)+\operatorname{Res}(z_2)
+\operatorname{Res}(z_3)
\right].
$$

For a pole of order $m$,

$$
\operatorname{Res}(z_1)
=\lim_{z\to z_1}\frac1{(m-1)!}
\frac{d^{m-1}}{dz^{m-1}}
\left[(z-z_1)^m f(z)\right].
$$

Example in spherical polars:

$$
\left[-\nabla^2+f(r)\right]y=\lambda y.
$$

$$
\left[
-\frac1{r^2}\frac\partial{\partial r}
 \left(r^2\frac\partial{\partial r}\right)
-\frac1{r^2\sin\theta}\frac\partial{\partial\theta}
 \left(\sin\theta\frac\partial{\partial\theta}\right)
-\frac1{r^2\sin^2\theta}\frac{\partial^2}{\partial\phi^2}
+f(r)
\right]y=\lambda y.
$$

Let

$$
y=R(r)\Theta(\theta)\Phi(\phi),
$$

$$
\begin{aligned}
&-\frac1{r^2R}\frac d{dr}\left(r^2\frac{dR}{dr}\right)
-\frac1{r^2\Theta\sin\theta}
 \frac d{d\theta}\left(\sin\theta\frac{d\Theta}{d\theta}\right)\\
&\hspace{5em}
-\frac1{r^2\Phi\sin^2\theta}\frac{d^2\Phi}{d\phi^2}
+f(r)=\lambda.
\end{aligned}
$$

Multiplying by $r^2\sin^2\theta$,

$$
-\frac{\sin^2\theta}{R}\frac d{dr}
 \left(r^2\frac{dR}{dr}\right)
-\frac{\sin\theta}{\Theta}\frac d{d\theta}
 \left(\sin\theta\frac{d\Theta}{d\theta}\right)
-\frac1\Phi\frac{d^2\Phi}{d\phi^2}
+r^2\sin^2\theta f(r)
=\lambda r^2\sin^2\theta.
$$

Evidently $\Phi''/\Phi=k$. For $k=\alpha^2$,

$$
\Phi=A\sinh\alpha\phi+B\cosh\alpha\phi,
$$

but we want $\Phi$ to have period $2\pi$ so that $y$ is a
single-valued function, hence $A=B=0$. For $k=0$,
$\Phi=A\phi+B$ and $A=0$. For $k=-\alpha^2$,

$$
\Phi=A\sin\alpha\phi+B\cos\alpha\phi,
$$

which has period $2\pi$ if $\alpha=m$, an integer. Hence

$$
\frac{\Phi''}{\Phi}=-m^2.
$$

$$
-\frac1R\frac d{dr}\left(r^2\frac{dR}{dr}\right)
-\frac1{\Theta\sin\theta}\frac d{d\theta}
 \left(\sin\theta\frac{d\Theta}{d\theta}\right)
+\frac{m^2}{\sin^2\theta}
+r^2f(r)=\lambda r^2.
$$

that is,

$$
-\frac1{\Theta\sin\theta}
\frac d{d\theta}\left(\sin\theta\frac{d\Theta}{d\theta}\right)
+\frac{m^2}{\sin^2\theta}=\mu
$$

and

$$
-\frac1R\frac d{dr}\left(r^2\frac{dR}{dr}\right)
+r^2f(r)=\mu+\lambda r^2.
$$

Two eigenvalue problems. The second depends upon the function $f(r)$,
whereas the first is purely geometric. Notice that the first also
depends upon the parameter $m$.

Return to the example after a theory break.

Standard form:

$$
y''+r(x,m)y+\lambda y=0. \tag{1}
$$

Note: include $m$ as motivated by example.

<a id="definition-of-factorisation"></a>
Definition of factorisation:

Equation (1) may be factorised if it can be replaced by each of the two
equations

$$
H_{m+1}H_{m+1}^\dagger y(\lambda,m)
=[\lambda-L(m+1)]y(\lambda,m). \tag{2}
$$

$$
H_m^\dagger H_my(\lambda,m)
=[\lambda-L(m)]y(\lambda,m). \tag{3}
$$

where

$$
H_m=k(x,m)+D,
\qquad
H_m^\dagger=k(x,m)-D.
$$

and $L$ is some function of $m$.

**Theorem.** If $y(\lambda,m)$ is a solution of equation (1), then

$$
y(\lambda,m+1)=H_{m+1}^\dagger y(\lambda,m),
$$

and

$$
y(\lambda,m-1)=H_my(\lambda,m)
$$

are corresponding to the same $\lambda$, but different $m$.

**Proof.** Multiply equation (2) by $H_{m+1}^\dagger$:

$$
\begin{aligned}
H_{m+1}^\dagger(H_{m+1}H_{m+1}^\dagger y)
 &=[\lambda-L(m+1)]H_{m+1}^\dagger y,\\
(H_{m+1}^\dagger H_{m+1})(H_{m+1}^\dagger y)
 &=[\lambda-L(m+1)](H_{m+1}^\dagger y).
\end{aligned}
$$

From equation (3),

$$
H_{m+1}^\dagger y(\lambda,m)=y(\lambda,m+1).
$$

Multiply equation (3) by $H_m$:

$$
(H_mH_m^\dagger)(H_my)
=[\lambda-L(m)](H_my),
$$

Comparing with equation (2),

$$
H_my(\lambda,m)=y(\lambda,m-1).
$$

$H_m$ and $H_m^\dagger$ are ladder operators with respect to $m$,
$H_m$ going down and $H_m^\dagger$ going up.

**Theorem.** $H_m$ and $H_m^\dagger$ are adjoint operators (for
example, Hermitian) if

$$
\int_a^b\phi(H_m\psi)\,dx
=\int_a^b(H_m^\dagger\phi)\psi\,dx,
$$

where $\phi\psi$ vanishes at the end points and the integrand is
continuous in $(a,b)$.

**Proof.** Already done.

Inner product:

$$
(\phi,\psi)=\int_a^b \phi^*\psi\,dx.
$$

Norm:

$$
\|\phi\|^2=(\phi,\phi).
$$

**Theorem.** If $y(\lambda,m)$ is $L^2$ over $(a,b)$ and $L(m)$ is an
increasing function of $m>0$, then the raising operator $H^\dagger$
produces an $L^2$ function which vanishes at the end points. If $L(m)$
is a decreasing function of $m>0$, the lowering operator $H$ produces
an $L^2$ function which vanishes at the end points.

[Just accept as the proof has to be done for every particular case --
not the $L^2$ part but the vanishing.]

**Theorem.** When $L(m)$ is an increasing function of the integer $m$
for $0<m\leq M$ $(\leq\infty)$, and
$\lambda\leq\max[L(m),L(m+1)]$, a necessary condition for $L^2$
solutions is that

$$
\lambda=\lambda_l=L(l+1),
\qquad m=0,1,\ldots,l.
$$

**Proof.** Assume that $y(\lambda,m)$ is $L^2$. Then

$$
y(\lambda,m+1)=H_{m+1}^\dagger y(\lambda,m)
$$

is also $L^2$ and vanishes at the endpoints (see previous theorem).

$$
\begin{aligned}
\|y(\lambda,m+1)\|^2
 &=\bigl(H_{m+1}^\dagger y(\lambda,m),
         H_{m+1}^\dagger y(\lambda,m)\bigr)\\
 &=\bigl(y(\lambda,m),
         H_{m+1}H_{m+1}^\dagger y(\lambda,m)\bigr)\\
 &=[\lambda-L(m+1)]\|y(\lambda,m)\|^2.
\end{aligned}
$$

Likewise,

$$
\begin{aligned}
\|y(\lambda,m+2)\|^2
 &=[\lambda-L(m+2)]\|y(\lambda,m+1)\|^2\\
 &=[\lambda-L(m+2)][\lambda-L(m+1)]
   \|y(\lambda,m)\|^2.
\end{aligned}
$$

$L$ is an increasing function of $m$, and so for some integer $l$ we
will have $\lambda-L(l+1)<0$, that is,
$\|y(\lambda,l+1)\|^2<0$, which is against the rules, so we have

$$
\lambda=L(l+1),
\qquad
H_{l+1}^\dagger y(\lambda,l)=0.
$$

This fixes $\lambda$ in terms of $l$, the maximum value of $m$. All
other $m$ are less than $l$, and the eigenvalue is
$L(l+1)-L(m+1)$.

**Theorem.** If $L(m)$ is a decreasing function of $m$ for
$0<m\leq M$ $(\leq\infty)$, and $\lambda<L(\infty)$, a necessary condition
for the existence of an $L^2$ solution is that

$$
\lambda=\lambda_l=L(l),
\qquad m=l,l+1,l+2,\ldots.
$$

**Proof.** Assume that $y(\lambda,m)$ is $L^2$:

$$
\begin{aligned}
\|y(\lambda,m-1)\|^2
 &=\bigl(H_my(\lambda,m),H_my(\lambda,m)\bigr)\\
 &=\bigl(y(\lambda,m),H_m^\dagger H_my(\lambda,m)\bigr)\\
 &=[\lambda-L(m)]\|y(\lambda,m)\|^2,
\end{aligned}
$$

Likewise,

$$
\|y(\lambda,m-2)\|^2
=[\lambda-L(m-1)][\lambda-L(m)]\|y(\lambda,m)\|^2.
$$

Continue until there is an integer $l$ such that $\lambda-L(l)<0$.
This gives a contradiction, so

$$
\lambda=L(l),
\qquad
\|y(\lambda,l-1)\|=0.
$$

That is, $y(\lambda,l-1)=0$, or $H_l y(\lambda,l)=0$. $l$ is the
lowest value of $m$, and $m=l,l+1,l+2,\ldots$.

In the above we have taken $m>0$. This is not necessary. $m$ may start
at some $m_0$, not necessarily an integer. Then $|l-m|$ must be an
integer. We now write the solutions as $y_l^m(x)$, or $Y_l^m(x)$ when
normalised.

Suppose $Y_l^m(x)$ is normalised, that is,

$$
\|Y_l^m(x)\|^2
=\int_a^bY_l^{m*}Y_l^m\,dx
=1.
$$

Then

$$
y_l^{m+1}=H_{m+1}^\dagger Y_l^m,
$$

so that

$$
\begin{aligned}
\|y_l^{m+1}\|^2
 &=\|H_{m+1}^\dagger Y_l^m\|^2\\
 &=\left(H_{m+1}^\dagger Y_l^m,
          H_{m+1}^\dagger Y_l^m\right)\\
 &=\left(Y_l^m,H_{m+1}H_{m+1}^\dagger Y_l^m\right)\\
 &=[L(l+1)-L(m+1)]\|Y_l^m\|^2\\
 &=L(l+1)-L(m+1).
\end{aligned}
$$

That is, $y_l^{m+1}$ is not normalised. Likewise,

$$
y_l^{m-1}=H_mY_l^m,
$$

and

$$
\|y_l^{m-1}\|^2
=[L(l)-L(m)]\|Y_l^m\|^2
=L(l)-L(m),
$$

that is, $y_l^{m-1}$ is not normalised. Overcome the problem by
defining

$$
\begin{aligned}
\mathcal H_l^{\dagger,m+1}
 &=[L(l+1)-L(m+1)]^{-1/2}H_{m+1}^\dagger,\\
\mathcal H_l^m
 &=[L(l)-L(m)]^{-1/2}H_m.
\end{aligned}
$$

If we calculate $y_l^l$ from either
$H_{l+1}^\dagger y_l^l=0$ (up) or
$H_l y_l^l=0$ (down), and normalise to $Y_l^l$, then
the $Y_l^m$ generated by $\mathcal H^\dagger$ and $\mathcal H$ are
normalised automatically. The differential equation becomes

$$
\mathcal H_l^{m+1}\mathcal H_l^{\dagger,m+1}Y_l^m=Y_l^m,
\qquad
\mathcal H_l^{\dagger,m}\mathcal H_l^mY_l^m=Y_l^m.
$$

Chart for class 1 solutions: $L(m)$ is increasing.

<figure data-chart="factorisation-class-i" style="margin: 1.5em auto; max-width: 40rem;">
  <img src="img/factorisation-class-i.svg"
       alt="Chart for class I solutions, with L of m increasing."
       style="display: block; width: 100%; height: auto;" />
</figure>

Basic solution $y_l^l$ comes from

$$
H_{l+1}^\dagger y_l^l=0,
$$

which is a first-order differential equation:

$$
\left[k(x,l+1)-\frac d{dx}\right]y_l^l=0.
$$

$$
y_l^l=C\exp\left(\int k(x,l+1)\,dx\right).
$$

$C$ is found from requiring $\|y_l^l\|=1$. Obtain $Y_l^m$ from

$$
\mathcal H_l^mY_l^m=Y_l^{m-1},
\qquad m=l,l-1,\ldots,0.
$$

Chart for class II solutions: $L(m)$ is a decreasing function.

<figure data-chart="factorisation-class-ii" style="margin: 1.5em auto; max-width: 40rem;">
  <img src="img/factorisation-class-ii.svg"
       alt="Chart for class II solutions, with L of m decreasing."
       style="display: block; width: 100%; height: auto;" />
</figure>

Basic solutions $Y_l^l$ come from

$$
H_l y_l^l=0
$$

that is,

$$
\left[k(x,l)+\frac d{dx}\right]Y_l^l=0,
$$

so

$$
y_l^l=C\exp\left(-\int k(x,l)\,dx\right),
$$

$C$ is determined by the normalisation condition $\|Y_l^l\|=1$.

Factorisation types:

$$
[-r(x,m)-D^2]y=\lambda y.
$$

Seek

$$
H_m=k(x,m)+D,
\qquad
H_m^\dagger=k(x,m)-D,
$$

such that

$$
\begin{aligned}
H_{m+1}H_{m+1}^\dagger y&=[\lambda-L(m+1)]y,\\
H_m^\dagger H_my&=[\lambda-L(m)]y.
\end{aligned}
$$

that is,

$$
H_{m+1}H_{m+1}^\dagger+L(m+1)=-r(x,m)-D^2,
$$

and

$$
H_m^\dagger H_m+L(m)=-r(x,m)-D^2,
$$

which is

$$
\begin{aligned}
k^2(x,m+1)+k'(x,m+1)+L(m+1)&=-r(x,m),\\
k^2(x,m)-k'(x,m)+L(m)&=-r(x,m).
\end{aligned}
$$

Subtracting,

$$
k^2(x,m+1)-k^2(x,m)
+k'(x,m+1)+k'(x,m)=L(m)-L(m+1).
$$

What $k$'s and $L$'s?

1. Let $k(x,m)=f(m)$. Then $L(m)=-f^2(m)$ and the differential
   equation is

   $$
   y''+\lambda y=0,
   $$

   which is a bit simple.

2. Let

$$
k(x,m)=k_0(x)+mk_1(x).
$$

Substituting

$$
\begin{aligned}
&(m+1)^2(k_1^2+k_1')
+2(m+1)(k_0k_1+k_0')\\
&\qquad
-m^2(k_1^2+k_1')-2m(k_0k_1+k_0')
=L(m)-L(m+1).
\end{aligned}
$$

that is,

$$
L(m)=-\left[
m^2(k_1^2+k_1')+2m(k_0k_1+k_0')+f(m,x)
\right],
$$

where $f(m+1,x)=f(m,x)$, that is, $f(m,x)$ has period one in $m$.
Since we are only interested in integer increments in $m$, take
$f(m,x)=f(x)$. Separate by coefficients of powers of $m$:

$$
k_1^2+k_1'=-a^2
$$

and

$$
k_0k_1+k_0'=
\begin{cases}
-ca,&a\ne0,\\
b,&a=0.
\end{cases}
$$

For $m^0$, $f(x)$ is constant.

Since $f(x)$ is constant it just shifts $L(m)$ by a constant amount
for any $m$; the constant may take any value and is usually zero.

Case $a\ne0$,

$$
\begin{array}{ll}
\text{A:}&
k_1=a\cot a(x+p),\quad
k_0=ca\cot a(x+p)+\dfrac d{\sin a(x+p)},\\[8pt]
\text{B:}&
k_1=ia,\quad
k_0=cia+d e^{-iax}
\quad\text{(a particular solution)},\\[6pt]
\end{array}
$$

Case $a=0$,

$$
\begin{array}{ll}
\text{C:}&
k_1=\dfrac1x,\quad
k_0=\dfrac12bx+\dfrac dx,\\[8pt]
\text{D:}&
k_1=0,\quad
k_0=bx+d
\quad\text{(a particular solution)}.
\end{array}
$$

3. Try

$$
k(x,m)=\sum_{i=0}^n k_i(x)m^i,
\qquad n<\infty.
$$

leads to nothing new.

4. Try

$$
k(x,m)=\frac{k_{-1}(x)}m+k_0(x)+mk_1(x)
$$

leads to

$$
\begin{array}{ll}
\text{E:}&
k_1=a\cot a(x+p),\quad k_0=0,\quad k_{-1}=q,\\[6pt]
\text{F:}&
k_1=\dfrac1x,\quad k_0=0,\quad k_{-1}=q.
\end{array}
$$

For E and F,

$$
L(m)=a^2m^2-\frac{q^2}{m^2},
$$

with $a=0$ in F. Trying

$$
k(x,m)=\sum_{i=-n}^{1}k_i(x)m^i,
\qquad n<\infty,
$$

Nothing new!

Examples:

Type A:

$$
r(x,m)=
\frac{
-a^2(m+c)(m+c+1)+d^2
+2ad\left(m+c+\frac12\right)\cos a(x+p)
}{\sin^2a(x+p)}.
$$

$$
k=(m+c)a\cot a(x+p)+\frac d{\sin a(x+p)},
$$

$$
L(m)=a^2(m+c)^2.
$$

<a id="associated-spherical-harmonics"></a>
### Associated spherical harmonics

The “angle” operator in spherical polars with radial symmetry in the
field is

$$
\frac1{\sin\theta}
\frac d{d\theta}\left(\sin\theta\frac{dP}{d\theta}\right)
+\left[\lambda-\frac{m^2}{\sin^2\theta}\right]P=0.
$$

where $0\leq\theta\leq\pi$.

To put into standard form let

$$
Y=(\sin\theta)^{1/2}P,
$$

which gives

$$
Y''-\frac{m^2-\tfrac{1}{4}}{\sin^2\theta}Y
+\left(\lambda+\frac14\right)Y=0.
$$

Comparing with the standard A form for $r(x,m)$,

$$
a=1,
\qquad p=0,
\qquad d=0,
\qquad c=-\frac12,
$$

and

$$
\lambda'=\lambda+\frac14,
$$

so that

$$
k(\theta,m)=\left(m-\frac12\right)\cot\theta,
\qquad
L(m)=\left(m-\frac12\right)^2.
$$

Since $L(m)$ is an increasing function of $m$ for $2m>0$, this is a
class 1 problem, that is, there exists a value $m=l$ such that

$$
\lambda'=L(l+1)=\left(l+\frac12\right)^2,
$$

so

$$
\lambda=\lambda'-\frac14=l(l+1),
\qquad l=0,1,2,\ldots,\quad l\geq m.
$$

Eigenfunctions:

$$
H_{l+1}^\dagger y_l^l=0.
$$

$$
\left[k(\theta,l+1)-\frac d{d\theta}\right]y_l^l=0.
$$

$$
\left[\left(l+\frac12\right)\cot\theta
-\frac d{d\theta}\right]Y_l^l=0,
$$

$$
\left(l+\frac12\right)\frac{\cos\theta}{\sin\theta}\,d\theta
=\frac{dY_l^l}{Y_l^l}.
$$

$$
Y_l^l(\theta)=K\sin^{l+1/2}\theta.
$$

To find the normalised eigenfunction $Y_l^l$,

$$
\begin{aligned}
K^{-2}
 &=\int_0^\pi
   (\sin\theta)^{l+1/2}(\sin\theta)^{l+1/2}\,d\theta\\
 &=\int_0^\pi(\sin\theta)^{2l+1}\,d\theta\\
 &=\frac21\frac23\frac45\cdots\frac{2l}{2l+1}\\
 &=\frac21\frac22\frac23\frac44\frac45\cdots
   \frac{2l}{2l}\frac{2l}{2l+1}\\
 &=\frac{2^{2l+1}l!\,l!}{(2l+1)!}.
\end{aligned}
$$

$$
Y_l^l(\theta)=
\left[
\frac{(2l+1)!}{2^{2l+1}l!\,l!}
\right]^{1/2}
\sin^{l+1/2}\theta.
$$

Also,

$$
Y_l^{m-1}=
\frac{
\left[\left(m-\frac12\right)\cot\theta
+\dfrac d{d\theta}\right]Y_l^m
}{\sqrt{(l+m)(l-m+1)}},
$$

and

$$
Y_l^{m+1}=
\frac{
\left[\left(m+\frac12\right)\cot\theta
-\dfrac d{d\theta}\right]Y_l^m
}{\sqrt{(l+m+1)(l-m)}}.
$$

Note that replacing $m$ by $-m$ in the original differential equation
does not change the problem. Hence solutions for
$-m$, with $m=0,1,\ldots,l$, exist and

$$
Y_l^{-m}=(-1)^mY_l^m.
$$

Associated spherical harmonics as a class 2 problem. Go back to

$$
\frac1{\sin\theta}
\frac d{d\theta}\left(\sin\theta\frac{dP_l^m}{d\theta}\right)
+\left[l(l+1)-\frac{m^2}{\sin^2\theta}\right]P_l^m=0,
$$

and replace $m^2$ by $\lambda$ and $\lambda$ by $l(l+1)$, so that

$$
\frac1{\sin\theta}
\frac d{d\theta}\left(\sin\theta\frac{dP}{d\theta}\right)
-\frac{\lambda}{\sin^2\theta}P+l(l+1)P=0.
$$

Normal form: let

$$
z=\log\tan\frac\theta2.
$$

and let $P(\theta)\mapsto Q(z)$ to obtain

$$
\frac{d^2Q}{dz^2}
+\frac{l(l+1)}{\cosh^2z}Q+\lambda Q=0,
$$

Comparing with standard form, take

$$
d=0,\qquad p=\frac\pi2,\qquad a=i,\qquad c=0,
$$

and replace $x,m$ with $z,l$. Then

$$
k(z,l)=l\tanh z,
\qquad
L(l)=-l^2.
$$

which is patently class 2.

$$
\frac{d^2Q}{dz^2}
+\frac{l(l+1)}{\cosh^2z}Q+\lambda Q=0.
$$

The bottom of the ladder is reached when
$l$ is some value $m$:

$$
\lambda_m=L(m)=-m^2,
\qquad l-m=0,1,2,\ldots.
$$

Then

$$
\begin{aligned}
Q_m^m
 &=C\exp\left(-\int m\tanh z\,dz\right)\\
 &=C\cosh^{-m}z.
\end{aligned}
$$

Also,

$$
\begin{aligned}
\lVert Q_m^m\rVert^2
 &=\int_{-\infty}^{\infty}C^2\cosh^{-2m}z\,dz\\
 &=2C^2\int_0^\infty\cosh^{-2m}z\,dz\\
 &=2C^2 4^{m-1}B(m,m). \qquad (*)
\end{aligned}
$$

$$
I=\int_0^\infty\operatorname{sech}^{2m}z\,dz.
$$

(i)

$$
\begin{aligned}
I
 &=\int_0^\infty\operatorname{sech}^{2m-2}z
   \operatorname{sech}^2z\,dz\\
 &=\int_0^1(1-\eta^2)^{m-1}\,d\eta\\
 &=\sum_{i=0}^{m-1}(-1)^i
   \binom{m-1}{i}\int_0^1\eta^{2i}\,d\eta\\
 &=\sum_{i=0}^{m-1}(-1)^i
   \frac1{2i+1}\binom{m-1}{i}.
\end{aligned}
$$

(ii)

$$
\begin{aligned}
I
 &=\int_0^{\pi/2}\sin^{2m-1}\theta\,d\theta\\
 &=\int_0^{\pi/2}\sin^{2m-2}\theta\sin\theta\,d\theta\\
 &=-\left.\cos\theta\sin^{2m-2}\theta\right|_0^{\pi/2}
 +(2m-2)\int_0^{\pi/2}\sin^{2m-3}\theta\cos^2\theta\,d\theta\\
 &=(2m-2)\int_0^{\pi/2}\sin^{2m-3}\theta\,d\theta
 -(2m-2)\int_0^{\pi/2}\sin^{2m-1}\theta\,d\theta.
\end{aligned}
$$

Hence

$$
I=\frac{2m-2}{2m-1}\frac{2m-4}{2m-3}
\frac{2m-6}{2m-5}\cdots\frac43\frac23
=\frac{\{2^{m-1}(m-1)!\}^2}{(2m-1)!}.
$$

From $(*)$, where $B(m,m)$ is the Beta function,

$$
1=C^2 2^{2m-1}B(m,m),
$$

$$
\begin{aligned}
C^{-2}
 &=2^{2m-1}\frac{\Gamma(m)\Gamma(m)}{\Gamma(m+m)}\\
 &=2^{2m-1}\frac{(m-1)!(m-1)!}{(2m-1)!}.
\end{aligned}
$$

Beta function:

$$
B(x,y)=\int_0^1t^{x-1}(1-t)^{y-1}\,dt.
$$

$$
\int_0^1(1-\eta^2)^{m-1}\,d\eta
=\int_0^1(1-\eta)^{m-1}(1+\eta)^{m-1}\,d\eta.
$$

Plan I:

$$
1-\eta=2\xi,
$$

$$
1+\eta=2-(1-\eta)=2-2\xi.
$$

$I$ becomes

$$
\begin{aligned}
I
 &=2\int_{1/2}^1(2-2\xi)^{m-1}(2\xi)^{m-1}\,d\xi\\
 &=2^{2m-1}\int_{1/2}^1\xi^{m-1}(1-\xi)^{m-1}\,d\xi.
\end{aligned}
$$

(symmetric about $\xi=\frac12$ since both powers are $m-1$)

$$
\begin{aligned}
I
 &=2^{2m-2}\int_0^1
   \xi^{m-1}(1-\xi)^{m-1}\,d\xi\\
 &=2^{2m-2}B(m,m).
\end{aligned}
$$

$$
C=\frac{[2(2m-1)!]^{1/2}}{2^m(m-1)!},
$$

so

$$
Q_m^m(z)=
\frac{[2(2m-1)!]^{1/2}}{2^m(m-1)!}\cosh^{-m}z.
$$

Note that

$$
Q_l^m=
\frac{l\tanh z-\dfrac d{dz}}
{\sqrt{(l-m)(l+m)}}Q_{l-1}^m,
\qquad l-1\geq m.
$$

In the original coordinate,

$$
P_l^m=
\frac{-l\cos\theta-\sin\theta\dfrac d{d\theta}}
{\sqrt{(l-m)(l+m)}}P_{l-1}^m,
$$

and

$$
P_{l-1}^m=
\frac{-l\cos\theta+\sin\theta\dfrac d{d\theta}}
{\sqrt{(l-m)(l+m)}}P_l^m.
$$

<a id="spherical-harmonics-so3"></a>
### Eigenfunctions of the spherical harmonics associated with $SO(3)$

Rotation about $Oz$:

$$
\bar x=x\cos\theta+y\sin\theta,
\qquad
\bar y=-x\sin\theta+y\cos\theta,
\qquad
\bar z=z.
$$

For some angle $\theta$. an infinitesimal angle $d\theta$,

$$
\bar x=x+y\,d\theta,
\qquad
\bar y=y-x\,d\theta,
\qquad
\bar z=z.
$$

Seek a generator of the infinitesimal transformation.

In general, under an infinitesimal transformation generated by $G$,
$f(x,y,z)$ goes to

$$
\bar f=(1+\varepsilon G)f,
\qquad
G=\eta_1\frac\partial{\partial x}
 +\eta_2\frac\partial{\partial y}
 +\eta_3\frac\partial{\partial z}.
$$

For $f=x$,

$$
\bar x=x+\varepsilon\eta_1.
$$

For $f=y$,

$$
\bar y=y+\varepsilon\eta_2.
$$

For $f=z$,

$$
\bar z=z+\varepsilon\eta_3.
$$

Comparing,

$$
\eta_1=y,
\qquad
\eta_2=-x,
\qquad
\eta_3=0,
$$

$$
G_1=y\frac\partial{\partial x}-x\frac\partial{\partial y}.
$$

Similarly, for rotations about $Ox$,

$$
G_2=z\frac\partial{\partial y}-y\frac\partial{\partial z},
$$

and about $Oy$,

$$
G_3=x\frac\partial{\partial z}-z\frac\partial{\partial x}.
$$

$$
\begin{aligned}
[G_1,G_2]&=G_3,\\
[G_2,G_3]&=G_1,\\
[G_3,G_1]&=G_2,
\end{aligned}
$$

$$
[G_i,G_j]=\varepsilon_{ijk}G_k,
$$

that is, a standard representation of $SO(3)$.

Interested in invariance under rotation, that is, spherical symmetry.

So convert to spherical polars:

$$
x=r\sin\theta\cos\phi,
\qquad
y=r\sin\theta\sin\phi,
\qquad
z=r\cos\theta.
$$

$$
\begin{aligned}
\frac{\partial}{\partial r}
 &=\sin\theta\cos\phi\frac{\partial}{\partial x}
  +\sin\theta\sin\phi\frac{\partial}{\partial y}
  +\cos\theta\frac{\partial}{\partial z},\\
\frac1r\frac{\partial}{\partial\theta}
 &=\cos\theta\cos\phi\frac{\partial}{\partial x}
  +\cos\theta\sin\phi\frac{\partial}{\partial y}
  -\sin\theta\frac{\partial}{\partial z},\\
\frac1{r\sin\theta}\frac{\partial}{\partial\phi}
 &=-\sin\phi\frac{\partial}{\partial x}
  +\cos\phi\frac{\partial}{\partial y}.
\end{aligned}
$$

$$
\begin{pmatrix}
\partial_x\\[2pt]
\partial_y\\[2pt]
\partial_z
\end{pmatrix}
=
\begin{pmatrix}
\sin\theta\cos\phi & \sin\theta\sin\phi & \cos\theta\\
\cos\theta\cos\phi & \cos\theta\sin\phi & -\sin\theta\\
-\sin\phi & \cos\phi & 0
\end{pmatrix}^{-1}
\begin{pmatrix}
\partial_r\\[2pt]
r^{-1}\partial_\theta\\[2pt]
(r\sin\theta)^{-1}\partial_\phi
\end{pmatrix}.
$$

which gives

$$
\begin{aligned}
G_1&=-\frac\partial{\partial\phi},\\
G_2&=\sin\phi\frac\partial{\partial\theta}
 +\cos\phi\cot\theta\frac\partial{\partial\phi},\\
G_3&=-\cos\phi\frac\partial{\partial\theta}
 +\sin\phi\cot\theta\frac\partial{\partial\phi}.
\end{aligned}
$$

Define:

$$
\begin{aligned}
L_\pm
 &=i(G_2\pm iG_3)\\
 &=i\left[
   \sin\phi\frac\partial{\partial\theta}
   +\cos\phi\cot\theta\frac\partial{\partial\phi}
   \right]\\
 &\quad{}
 \pm\left[
   -\cos\phi\frac\partial{\partial\theta}
   +\sin\phi\cot\theta\frac\partial{\partial\phi}
   \right]\\
 &=\pm(\cos\phi\pm i\sin\phi)
   \frac\partial{\partial\theta}
   +i(\cos\phi\pm i\sin\phi)\cot\theta
   \frac\partial{\partial\phi}\\
 &=e^{\pm i\phi}
   \left(i\cot\theta\frac\partial{\partial\phi}
   \pm\frac\partial{\partial\theta}\right),
\end{aligned}
$$

$$
\begin{aligned}
[L_+,L_-]
 &=[i(G_2+iG_3),i(G_2-iG_3)]\\
 &=-i[G_3,G_2]+i[G_2,G_3]\\
 &=2i[G_2,G_3]\\
 &=2iG_1\\
 &=-2i\frac\partial{\partial\phi}.
\end{aligned}
$$

Define:

$$
L_3=-i\frac\partial{\partial\phi}=iG_1.
$$

For a spherically symmetric problem involving the Laplacian, the
differential equation is of the form

$$
\left[
\frac1{r^2}\frac\partial{\partial r}
 r^2\frac\partial{\partial r}
+\frac1{r^2\sin\theta}\frac\partial{\partial\theta}
 \sin\theta\frac\partial{\partial\theta}
+\frac1{r^2\sin^2\theta}\frac{\partial^2}{\partial\phi^2}
+f(r)
\right]\psi=0.
$$

Let

$$
\psi=R(r)\Phi(\theta,\phi).
$$

Then

$$
\frac1R\frac d{dr}\left(r^2\frac{dR}{dr}\right)+r^2f(r)
=-\frac1\Phi\left[
\frac1{\sin\theta}\frac\partial{\partial\theta}
 \left(\sin\theta\frac\partial{\partial\theta}\right)
+\frac1{\sin^2\theta}\frac{\partial^2}{\partial\phi^2}
\right]\Phi
=\lambda,
$$

which gives rise to the eigenvalue problem

$$
-\left[
\frac1{\sin\theta}\frac\partial{\partial\theta}
 \sin\theta\frac\partial{\partial\theta}
+\frac1{\sin^2\theta}\frac{\partial^2}{\partial\phi^2}
\right]\Phi=\lambda\Phi.
$$

Observe that

$$
L^2=-(G_1^2+G_2^2+G_3^2)
=-\left[
\frac1{\sin\theta}\frac\partial{\partial\theta}
 \sin\theta\frac\partial{\partial\theta}
+\frac1{\sin^2\theta}\frac{\partial^2}{\partial\phi^2}
\right].
$$

Looking at the problem,

$$
L^2\psi=\lambda\psi.
$$

Also,

$$
\begin{aligned}
[L^2,L_3]
 &=[-G_1^2-G_2^2-G_3^2,iG_1]\\
 &=-i[G_2^2,G_1]-i[G_3^2,G_1]\\
 &=-iG_2[G_2,G_1]-i[G_2,G_1]G_2\\
 &\quad{}-iG_3[G_3,G_1]-i[G_3,G_1]G_3\\
 &=iG_2G_3+iG_3G_2-iG_3G_2-iG_2G_3\\
 &=0,
\end{aligned}
$$

This means that $L^2$ and $L_3$ have simultaneous eigenfunctions.

Let $\psi$ be the eigenfunction of $L^2$. Then

$$
\begin{aligned}
L_3L^2\psi
 &=([L_3,L^2]+L^2L_3)\psi\\
 &=L^2L_3\psi,
\end{aligned}
$$

so $L^2(L_3\psi)=\lambda(L_3\psi)$. Moreover, $\psi$ and $L_3\psi$
are eigenfunctions to the same eigenvalue $\lambda$.

Write

$$
L^2\psi_{\lambda,\mu}=\lambda\psi_{\lambda,\mu},
\qquad
L_3\psi_{\lambda,\mu}=\mu\psi_{\lambda,\mu}.
$$

Consider

$$
\begin{aligned}
L_3(L_\pm\psi_{\lambda,\mu})
 &=\bigl([L_3,L_\pm]+L_\pm L_3\bigr)
   \psi_{\lambda,\mu}\\
 &=(\pm L_\pm+\mu L_\pm)\psi_{\lambda,\mu}\\
 &=(\mu\pm1)L_\pm\psi_{\lambda,\mu},
\end{aligned}
$$

that is, $L_\pm$ are ladder operators for $\mu$:

$$
L_\pm\psi_{\lambda,\mu}
=(\text{constant})\psi_{\lambda,\mu\pm1}.
$$

Now

$$
\begin{aligned}
(L_+L_-)^\dagger
 &=(L_-)^\dagger(L_+)^\dagger
  =L_+L_-,\\
(L_-L_+)^\dagger
 &=(L_+)^\dagger(L_-)^\dagger
  =L_-L_+,
\end{aligned}
$$

that is, $L_+L_-$ and $L_-L_+$ are both Hermitian operators. Just as
for matrices of the form $AA^\dagger\sim A^\dagger A$, the
eigenvalues are real and non-negative, having the form
$\bar\lambda\lambda$, that is, $|\lambda|^2$, so $L_+L_-$ and
$L_-L_+$ have non-negative eigenvalues.

Now

$$
\begin{aligned}
L_+L_-
 &=i(G_2+iG_3)i(G_2-iG_3)\\
 &=-G_2^2-G_3^2+i[G_2,G_3]\\
 &=-(G_1^2+G_2^2+G_3^2)+G_1^2+iG_1\\
 &=L^2-L_3^2+L_3,
\end{aligned}
$$

and

$$
\begin{aligned}
L_-L_+
 &=i(G_2-iG_3)i(G_2+iG_3)\\
 &=L^2-L_3^2-L_3.
\end{aligned}
$$

Hence

$$
\begin{aligned}
L_+L_-\psi_{\lambda,\mu}
  &=(\lambda-\mu^2+\mu)\psi_{\lambda,\mu},\\
L_-L_+\psi_{\lambda,\mu}
  &=(\lambda-\mu^2-\mu)\psi_{\lambda,\mu}.
\end{aligned}
$$

Since $L_+L_-$ and $L_-L_+$ have non-negative eigenvalues,

$$
\lambda-\mu^2+\mu\geq0,
\qquad
\lambda-\mu^2-\mu\geq0.
$$

Since $L_-$ is a lowering operator, $\lambda-\mu^2+\mu$ will
eventually become negative unless there exists $\mu'$ such that

$$
L_-\psi_{\lambda,\mu'}=0
$$

and so

$$
\lambda-\mu'^2+\mu'=0.
$$

Likewise, $L_+$ is a raising operator and
$\lambda-\mu^2-\mu$ will become negative unless there exists a
$\mu''$ such that

$$
L_+\psi_{\lambda,\mu''}=0
$$

and

$$
\lambda-\mu''^2-\mu''=0.
$$

$L_+$ raises ($L_-$ lowers) $\mu$ by integer steps, and so

$$
\mu''-\mu'=\text{integer}=n\geq0.
$$

At $\mu'$,

$$
\lambda=\mu'^2-\mu',
$$

and at $\mu''$,

$$
\lambda=\mu''^2+\mu''.
$$

and the value of $\lambda$ is the same, so

$$
\begin{aligned}
0
 &=(\mu''-\mu')(\mu''+\mu')+(\mu''+\mu')\\
 &=(\mu''+\mu')(\mu''-\mu'+1)\\
 &=(\mu''+\mu')(n+1),
\end{aligned}
$$

so $\mu'=-\mu''$, and so $2\mu''=n$.

Let

$$
\mu''=l,
\qquad
\mu'=-l.
$$

$2l$ is an integer and

$$
\lambda=\mu'^2-\mu'=l(l+1).
$$

Because of the integer steps in $\mu$, $2\mu$ is an integer; now
replace $\mu$ by $m$.

To find the eigenfunction,

$$
L_3=-i\partial_\phi,
$$

$$
L_3\psi_{l,m}=m\psi_{l,m},
$$

$$
-i\frac{\partial\psi_{l,m}}{\partial\phi}=m\psi_{l,m},
$$

so

$$
\psi_{l,m}=e^{im\phi}f(\theta).
$$

If $\psi$ is to be a single valued function of $\phi$, then the
half-integer values must be excluded.

Now $L_+\psi_{l,l}=0$, so

$$
\begin{aligned}
0
 &=L_+\psi_{l,l}\\
 &=e^{i\phi}\left(
   i\cot\theta\frac\partial{\partial\phi}
   +\frac\partial{\partial\theta}
   \right)e^{il\phi}f(\theta)\\
 &=e^{i(l+1)\phi}\left[-l\cot\theta f+f'\right].
\end{aligned}
$$

Hence

$$
f=K\sin^l\theta.
$$

The norm is

$$
\begin{aligned}
\|\psi_{l,l}\|^2
 &=\int_0^\pi\sin\theta\,d\theta
   \int_0^{2\pi}d\phi\,
   \psi_{l,l}^*\psi_{l,l}\\
 &=2\pi|K|^2\int_0^\pi\sin^{2l+1}\theta\,d\theta\\
 &=2\pi|K|^2\frac{2^{2l+1}l!\,l!}{(2l+1)!}.
\end{aligned}
$$

$$
\psi_{l,l}\mathrel{\boldsymbol{=}}
\frac1{2^{l+1}l!}
\left[\frac{(2l+1)!}{\pi}\right]^{1/2}
e^{il\phi}\sin^l\theta.
$$

In general,

$$
\psi_{l,m-1}
\mathrel{\boldsymbol{=}}\frac1{\sqrt{l(l+1)-m(m-1)}}L_-\psi_{l,m}.
$$

or

$$
\psi_{l,m-1}\mathrel{\boldsymbol{=}}\ell_-\psi_{l,m},
$$

where

$$
\ell_-=
\left[l(l+1)-m(m-1)\right]^{-1/2}L_-.
$$

For example,

$$
\psi_{l,l-1}\mathrel{\boldsymbol{=}}
-\frac1{2^l(l-1)!}
\left[\frac{(2l+1)!}{2\pi l}\right]^{1/2}
e^{i(l-1)\phi}\cos\theta\sin^{l-1}\theta.
$$

And so on.

<figure style="margin: 1.5em auto; max-width: 46rem;">
  <img src="img/spherical-harmonic-ladder.svg"
       alt="Triangular spherical-harmonic ladder of the allowed integer states l and m, bounded by the known solutions m equals plus or minus l."
       style="display: block; width: 100%; height: auto;" />
</figure>

---

<nav aria-label="Section navigation" style="display: grid; grid-template-columns: minmax(0, 1fr) auto; column-gap: 2em; align-items: start;">
<div style="display: grid; grid-template-columns: 6em minmax(0, 1fr); row-gap: 0.25em;">
<span>NEXT:</span><a href="05-index.md">Index</a>
<span>PREVIOUS:</span><a href="03-lie-theory-of-extended-group.md">Lie Theory of Extended Group</a>
</div>
<a href="05-index.md" style="justify-self: end; text-align: right;">INDEX</a>
</nav>
