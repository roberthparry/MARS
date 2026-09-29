<a id="linear-second-order-odes"></a>
## Linear second-order ODEs

<a id="factorisation-technique"></a>
### Factorisation technique

For example,

$\quad\begin{array}{l}\displaystyle y''+y=0.\end{array}$

$\quad\begin{array}{l}\displaystyle \begin{aligned} (D^2+1)y&=0,\\ (D+i)(D-i)y&=0. \end{aligned}\end{array}$

Let

$\quad\begin{array}{l}\displaystyle (D-i)y=z.\end{array}$

$\quad\begin{array}{l}\displaystyle (D+i)z=0,\end{array}$

$\quad\begin{array}{l}\displaystyle z'+iz=0.\end{array}$

Integrating factor: $e^{ix}$.

$\quad\begin{array}{l}\displaystyle (ze^{ix})'=0,\end{array}$

$\quad\begin{array}{l}\displaystyle ze^{ix}=-2iB.\end{array}$

$\quad\begin{array}{l}\displaystyle z=-2iBe^{-ix}.\end{array}$

$\quad\begin{array}{l}\displaystyle y'-iy=-2iBe^{-ix}.\end{array}$

Integrating factor: $e^{-ix}$.

$\quad\begin{array}{l}\displaystyle (ye^{-ix})'=-2iBe^{-2ix}.\end{array}$

$\quad\begin{array}{l}\displaystyle ye^{-ix}=A+Be^{-2ix},\end{array}$

$\quad\begin{array}{l}\displaystyle y=Ae^{ix}+Be^{-ix},\end{array}$

or

$\quad\begin{array}{l}\displaystyle y=C\sin x+D\cos x.\end{array}$

Equally could use

$\quad\begin{array}{l}\displaystyle (D-i)(D+i)y=0.\end{array}$

For a less trivial example consider

$\quad\begin{array}{l}\displaystyle y''-(x^2+1)y=0.\end{array}$

$\quad\begin{array}{l}\displaystyle (D+x)(D-x)y=0.\end{array}$

Note this is not the same as

$\quad\begin{array}{l}\displaystyle (D-x)(D+x)=D^2-(x^2-1).\end{array}$

Let

$\quad\begin{array}{l}\displaystyle z=(D-x)y.\end{array}$

$\quad\begin{array}{l}\displaystyle (D+x)z=0.\end{array}$

Integrating factor: $e^{x^2/2}$.

$\quad\begin{array}{l}\displaystyle (ze^{x^2/2})'=0,\end{array}$

$\quad\begin{array}{l}\displaystyle z=Be^{-x^2/2}.\end{array}$

$\quad\begin{array}{l}\displaystyle (D-x)y=Be^{-x^2/2}.\end{array}$

Integrating factor: $e^{-x^2/2}$.

$\quad\begin{array}{l}\displaystyle y=Ae^{x^2/2}+Be^{x^2/2}\int e^{-x^2}\,dx.\end{array}$

Consider

$\quad\begin{array}{l}\displaystyle y''-f(x)y=0\end{array}$

and the factorisation

$\quad\begin{array}{l}\displaystyle (D+\alpha(x))(D+\beta(x)).\end{array}$

$\quad\begin{array}{l}\displaystyle (D+\alpha)(D+\beta) =D^2+(\alpha+\beta)D+\alpha\beta+\beta',\end{array}$

$\quad\begin{array}{l}\displaystyle \beta=-\alpha, \qquad \alpha'+\alpha^2=f.\end{array}$

where $\alpha'+\alpha^2=f$ is Riccati's equation.

$\quad\begin{array}{l}\displaystyle (D+\alpha)(D-\alpha)y=0,\end{array}$

$\quad\begin{array}{l}\displaystyle z=(D-\alpha)y.\end{array}$

$\quad\begin{array}{l}\displaystyle z=B e^{\int\alpha\,dx},\end{array}$

leads to

$\quad\begin{array}{l}\displaystyle y=Ae^{-\int\alpha\,dx} +Be^{-\int\alpha\,dx}\int e^{2\int\alpha\,dx}\,dx.\end{array}$

<a id="existence-of-solutions"></a>
### Existence of solutions

Initial value problem:

$\quad\begin{array}{l}\displaystyle y''-f(x)y=0, \qquad y(0)=\alpha, \qquad y'(0)=\beta,\end{array}$

has a continuous solution with continuous derivative if $f(x)$ is
continuous.

Boundary value problem:

$\quad\begin{array}{l}\displaystyle y''-f(x)y=0,\end{array}$

$\quad\begin{array}{l}\displaystyle \begin{aligned} \alpha_{11}y(a)+\alpha_{12}y'(a) +\beta_{11}y(b)+\beta_{12}y'(b)&=0,\\ \alpha_{21}y(a)+\alpha_{22}y'(a) +\beta_{21}y(b)+\beta_{22}y'(b)&=0. \end{aligned}\end{array}$

where the two conditions are independent. This may or may not have a
non-trivial solution. For example,

$\quad\begin{array}{l}\displaystyle y''+y=0, \qquad y(0)=0, \qquad y'(\pi)=0.\end{array}$

$\quad\begin{array}{l}\displaystyle y=A\sin x+B\cos x,\end{array}$

$\quad\begin{array}{l}\displaystyle y(0)=0\quad\Longrightarrow\quad B=0,\end{array}$

and

$\quad\begin{array}{l}\displaystyle y'(\pi)=0\quad\Longrightarrow\quad -A=0.\end{array}$

There is no non-trivial solution.

However, consider

$\quad\begin{array}{l}\displaystyle y''+\lambda y=0, \qquad y(0)=0, \qquad y'(\pi)=0.\end{array}$

For $\lambda=-\omega^2$,

$\quad\begin{array}{l}\displaystyle y=A\sinh\omega x+B\cosh\omega x,\end{array}$

$\quad\begin{array}{l}\displaystyle y(0)=0\quad\Longrightarrow\quad B=0,\end{array}$

$\quad\begin{array}{l}\displaystyle y'(\pi)=0\quad\Longrightarrow\quad A=0.\end{array}$

For $\lambda=0$,

$\quad\begin{array}{l}\displaystyle y=Ax+B,\end{array}$

$\quad\begin{array}{l}\displaystyle y(0)=0\quad\Longrightarrow\quad B=0,\end{array}$

$\quad\begin{array}{l}\displaystyle y'(\pi)=0\quad\Longrightarrow\quad A=0.\end{array}$

For $\lambda=\omega^2$,

$\quad\begin{array}{l}\displaystyle y=A\sin\omega x+B\cos\omega x.\end{array}$

$\quad\begin{array}{l}\displaystyle y(0)=0\quad\Longrightarrow\quad B=0,\end{array}$

$\quad\begin{array}{l}\displaystyle y'(\pi)=0\quad\Longrightarrow\quad \omega A\cos(\omega\pi)=0.\end{array}$

that is,

$\quad\begin{array}{l}\displaystyle \cos\omega\pi=0,\end{array}$

$\quad\begin{array}{l}\displaystyle \omega=n+\frac12.\end{array}$

Solution is

$\quad\begin{array}{l}\displaystyle y=A\sin\left(n+\frac12\right)x.\end{array}$

$n+\frac12$ is called the eigenvalue and
$\sin\left(n+\frac12\right)x$ is called the eigenfunction, sometimes
the eigenstate.

<a id="standard-eigenvalue-form"></a>
### Standard form of the eigenvalue differential equation

Given

$\quad\begin{array}{l}\displaystyle \frac{d}{dx}\left(f\frac{dy}{dx}\right)+gy+\lambda hy=0,\end{array}$

where $f$, $g$, and $h$ are functions of $x$, this can be transformed
to the standard form

$\quad\begin{array}{l}\displaystyle \frac{d^2Y}{dX^2}+F(X)Y+\lambda Y=0\end{array}$

by the transformation

$\quad\begin{array}{l}\displaystyle Y=(fh)^{1/4}y, \qquad dX=\left(\frac hf\right)^{1/2}dx.\end{array}$

$f$ and $h$ must be of the same sign and $h/f$ must exist everywhere
in the domain of applicability of the equation. The standard form is
called the normal form.

<a id="harmonic-oscillator"></a>
### Example

$\quad\begin{array}{l}\displaystyle y''-x^2y+\lambda y=0,\end{array}$

Write as

$\quad\begin{array}{l}\displaystyle Hy=\lambda y, \qquad H=x^2-D^2.\end{array}$

Let

$\quad\begin{array}{l}\displaystyle a=x+D, \qquad a^\dagger=x-D.\end{array}$

Using $\alpha=x$ and $\alpha=-x$ gives

$\quad\begin{array}{l}\displaystyle \begin{aligned} (x-D)(x+D)y&=(\lambda+1)y,\\ (x+D)(x-D)y&=(\lambda-1)y. \end{aligned}\end{array}$

$\quad\begin{array}{l}\displaystyle \begin{aligned} [a,a^\dagger] &=(x+D)(x-D)-(x-D)(x+D)\\ &=x^2+1-D^2-(x^2-1-D^2)\\ &=2, \end{aligned}\end{array}$

$\quad\begin{array}{l}\displaystyle \begin{aligned} [H,a] &=(x^2-D^2)(x+D)-(x+D)(x^2-D^2)\\ &=x^3+x^2D-D^2x-D^3 -x^3+xD^2-Dx^2+D^3\\ &=x^2D-D^2x+xD^2-Dx^2\\ &=-2x-2D\\ &=-2a, \end{aligned}\end{array}$

and

$\quad\begin{array}{l}\displaystyle \begin{aligned} [H,a^\dagger] &=(x^2-D^2)(x-D)-(x-D)(x^2-D^2)\\ &=x^3-x^2D-D^2x+D^3 -x^3+xD^2+Dx^2-D^3\\ &=-x^2D-D^2x+xD^2+Dx^2\\ &=2x-2D\\ &=2a^\dagger. \end{aligned}\end{array}$

If we include the identity, we have the algebra with elements
$H,a,a^\dagger,1$ such that

$\quad\begin{array}{l}\displaystyle [H,a]=-2a,\qquad [H,a^\dagger]=2a^\dagger,\qquad [a,a^\dagger]=2\,1,\qquad [H,1]=0,\end{array}$

etc.

Suppose $\psi$ is a solution of
$\psi''+(\lambda-x^2)\psi=0$ corresponding to the eigenvalue
$\lambda$. Then, denoting it by $\psi_\lambda$, we have

$\quad\begin{array}{l}\displaystyle H\psi_\lambda=\lambda\psi_\lambda.\end{array}$

Then

$\quad\begin{array}{l}\displaystyle \begin{aligned} H(a\psi_\lambda) &=(Ha)\psi_\lambda\\ &=([H,a]+aH)\psi_\lambda\\ &=-2a\psi_\lambda+a\lambda\psi_\lambda\\ &=(\lambda-2)a\psi_\lambda, \end{aligned}\end{array}$

that is, $a\psi_\lambda$ is the eigenfunction corresponding to the
eigenvalue $\lambda-2$. Likewise with $a^\dagger$,

$\quad\begin{array}{l}\displaystyle \begin{aligned} H(a^\dagger\psi_\lambda) &=(Ha^\dagger)\psi_\lambda\\ &=([H,a^\dagger]+a^\dagger H)\psi_\lambda\\ &=2a^\dagger\psi_\lambda+a^\dagger\lambda\psi_\lambda\\ &=(\lambda+2)a^\dagger\psi_\lambda, \end{aligned}\end{array}$

that is, $a^\dagger\psi_\lambda$ is the eigenfunction corresponding
to eigenvalue $\lambda+2$. $a$ and $a^\dagger$ are called ladder
operators. $a$ is also called a lowering operator and $a^\dagger$ a
raising operator.

Assume that $\psi_\lambda$ is $L^2$ over $(-\infty,\infty)$, that is,

$\quad\begin{array}{l}\displaystyle \|\psi_\lambda\|^2 =\int_{-\infty}^{\infty}|\psi_\lambda|^2\,dx<\infty.\end{array}$

$\quad\begin{array}{l}\displaystyle \begin{aligned} (\psi_\lambda,x\psi_\lambda) &=\int_{-\infty}^{\infty}\psi_\lambda^*x\psi_\lambda\,dx\\ &=\int_{-\infty}^{\infty}(x\psi_\lambda)^*\psi_\lambda\,dx\\ &=(x\psi_\lambda,\psi_\lambda), \end{aligned}\end{array}$

and

$\quad\begin{array}{l}\displaystyle \begin{aligned} (\psi_\lambda,D\psi_\lambda) &=\int_{-\infty}^{\infty}\psi_\lambda^*\psi_\lambda'\,dx\\ &=\left[\psi_\lambda^*\psi_\lambda\right]_{-\infty}^{\infty} -\int_{-\infty}^{\infty}(\psi_\lambda^*)'\psi_\lambda\,dx\\ &=-(D\psi_\lambda,\psi_\lambda), \end{aligned}\end{array}$

since $D$ is a real operator.

Hence

$\quad\begin{array}{l}\displaystyle \begin{aligned} (\psi,a\psi)&=(a^\dagger\psi,\psi),\\ (\psi,a^\dagger\psi)&=(a\psi,\psi). \end{aligned}\end{array}$

We can write

$\quad\begin{array}{l}\displaystyle H=a^\dagger a+1,\end{array}$

so

$\quad\begin{array}{l}\displaystyle a^\dagger a=H-1,\end{array}$

$\quad\begin{array}{l}\displaystyle (\psi_\lambda,a^\dagger a\psi_\lambda) =(\psi_\lambda,(\lambda-1)\psi_\lambda) =(\lambda-1)\|\psi_\lambda\|^2\geq0.\end{array}$

where equality is attained only if $\lambda=1$, since $\psi_\lambda$
is assumed to be non-trivial (that is, not identically zero). However,

$\quad\begin{array}{l}\displaystyle (\psi_\lambda,a^\dagger a\psi_\lambda) =(a\psi_\lambda,a\psi_\lambda) =\|a\psi_\lambda\|^2 =(\lambda-1)\|\psi_\lambda\|^2.\end{array}$

So if $\lambda=1$, then $\|a\psi_\lambda\|^2=0$, that is,

$\quad\begin{array}{l}\displaystyle a\psi_\lambda=0.\end{array}$

that is,

$\quad\begin{array}{l}\displaystyle (x+D)y=0,\end{array}$

$\quad\begin{array}{l}\displaystyle xy+y'=0.\end{array}$

Integrating factor:

$\quad\begin{array}{l}\displaystyle \left(ye^{x^2/2}\right)'=0,\end{array}$

$\quad\begin{array}{l}\displaystyle y=\text{constant}\,e^{-x^2/2}.\end{array}$

To obtain the constant $K$, impose a value on the norm, usually one:

$\quad\begin{array}{l}\displaystyle 1=|K|^2\int_{-\infty}^{\infty}e^{-x^2}\,dx =|K|^2\sqrt\pi,\end{array}$

so $K=\pi^{-1/4}$.

Call this solution

$\quad\begin{array}{l}\displaystyle \psi_0=e^{-x^2/2}\end{array}$

(forget about the constant).

$\quad\begin{array}{l}\displaystyle a^\dagger\psi_0 =\left(x-\frac d{dx}\right)e^{-x^2/2} =2x e^{-x^2/2},\end{array}$

$\quad\begin{array}{l}\displaystyle \lambda=1+2=3.\end{array}$

Call this $\psi_1$. Likewise,

$\quad\begin{array}{l}\displaystyle \psi_2=a^\dagger\psi_1=(4x^2-2)e^{-x^2/2},\end{array}$

$\quad\begin{array}{l}\displaystyle \lambda=3+2=5.\end{array}$

In general,

$\quad\begin{array}{l}\displaystyle \psi_n=(a^\dagger)^n\psi_0 =\left(x-\frac d{dx}\right)^n e^{-x^2/2}. \qquad (*)\end{array}$

and

$\quad\begin{array}{l}\displaystyle \lambda_n=2n+1.\end{array}$

$\psi_n$ has the form

$\quad\begin{array}{l}\displaystyle \psi_n=H_n(x)e^{-x^2/2}. \qquad (**)\end{array}$

(Note: normalisation is neglected.)

where $H_n(x)$ is the $n$th Hermite polynomial. From $(*)$ and $(**)$
we obtain the Rodrigues formula for the Hermite polynomials:

$\quad\begin{array}{l}\displaystyle H_n(x)=e^{x^2/2} \left(x-\frac d{dx}\right)^n e^{-x^2/2}.\end{array}$

Note: $a^\dagger a$ is an example of an Hermitian operator, for

$\quad\begin{array}{l}\displaystyle \begin{aligned} (\phi,a^\dagger a\psi) &=\int_{-\infty}^{\infty}\phi^*(a^\dagger a\psi)\,dx\\ &=\int_{-\infty}^{\infty}(a\phi)^*(a\psi)\,dx\\ &=(a\phi,a\psi)\\ &=(a^\dagger a\phi,\psi). \end{aligned}\end{array}$

The eigenvalues of an Hermitian operator are real. Let $O$ be an
Hermitian operator:

$\quad\begin{array}{l}\displaystyle O\psi=\lambda\psi, \qquad O\phi=\mu\phi,\end{array}$

then

$\quad\begin{array}{l}\displaystyle \begin{aligned} (O\phi,\psi)&=\mu^*(\phi,\psi),\\ (\phi,O\psi)&=\lambda(\phi,\psi). \end{aligned}\end{array}$

Now $(\phi,O\psi)=(O\phi,\psi)$, hence

$\quad\begin{array}{l}\displaystyle (\lambda-\mu^*)(\phi,\psi)=0.\end{array}$

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

$\quad\begin{array}{l}\displaystyle f(x)=\sum_{n=0}^{\infty}a_nH_n(x)e^{-x^2/2}.\end{array}$

<a id="orthogonal-polynomials"></a>
### Orthogonal polynomials

Orthogonality:

$\quad\begin{array}{l}\displaystyle \int_a^b w(x)f_m(x)f_n(x)\,dx=h_m\delta_{mn},\end{array}$

where $w(x)$ is a weight function, $h_m$ is some constant and
$\delta_{mn}$ is the Kronecker delta. Often take $h_m=1$, but
sometimes other values are more convenient.

**Differential equation**

$\quad\begin{array}{l}\displaystyle \left[g_2(x)D^2+g_1(x)D+\lambda_n\right]f_n=0, \qquad \lambda_n=\lambda(n).\end{array}$

**Recurrence**

$\quad\begin{array}{l}\displaystyle f_{n+1}=(a_n+xb_n)f_n-c_nf_{n-1}.\end{array}$

**Rodrigues' formula**

$\quad\begin{array}{l}\displaystyle f_n(x)=\frac1{e_nw(x)} \frac{d^n}{dx^n}\left\{w(x)[g(x)]^n\right\}.\end{array}$

where $e_n$ is a constant and $w(x)$ is the weight function (not
always found).

**Generating function**

$\quad\begin{array}{l}\displaystyle g(x,z)=\sum_{n=0}^{\infty}a_nf_n(x)z^n.\end{array}$

**Differential relations**

$\quad\begin{array}{l}\displaystyle m_2(x)\frac{df_n}{dx}=m_1(x)f_n+m_0(x)f_{n-1}.\end{array}$

**Integral representation**

$\quad\begin{array}{l}\displaystyle f_n(x)=\frac{p_0(x)}{2\pi i} \oint_\Gamma[p_1(z,x)]^n p_2(z,x)\,dz.\end{array}$

where $\Gamma$ is a closed contour taken around some specified point
in the positive sense.

Consider Hermite polynomials.

$\quad\begin{array}{l}\displaystyle H_n''-2xH_n'+2nH_n=0,\end{array}$

$H_n(x)$ satisfies the above equation, and
$K_n=H_ne^{-x^2/2}$ satisfies

$\quad\begin{array}{l}\displaystyle K_n''+(2n+1-x^2)K_n=0.\end{array}$

Orthogonality already established.

Rodrigues' formula:

Had

$\quad\begin{array}{l}\displaystyle H_n(x)=e^{x^2/2}\left(x-\frac d{dx}\right)^n e^{-x^2/2},\end{array}$

This is not in standard form. The standard form is

$\quad\begin{array}{l}\displaystyle H_n(x)=e^{x^2}\left(-\frac d{dx}\right)^n e^{-x^2}.\end{array}$

Obviously correct for $n=0$. Let $n=1$:

$\quad\begin{array}{l}\displaystyle \begin{aligned} e^{x^2/2}\left(x-\frac d{dx}\right)e^{-x^2/2} &=e^{x^2/2}\left(x-\frac d{dx}\right) \left(e^{x^2/2}e^{-x^2}\right)\\ &=e^{x^2/2} \left[\left(x-\frac d{dx}\right)e^{x^2/2}\right]e^{-x^2} +e^{x^2}\left(-\frac d{dx}\right)e^{-x^2}\\ &=e^{x^2/2}\left[(x-x)e^{x^2/2}\right]e^{-x^2} +e^{x^2}\left(-\frac d{dx}\right)e^{-x^2}\\ &=e^{x^2}\left(-\frac d{dx}\right)e^{-x^2}. \end{aligned}\end{array}$

That is, it is true for $n=1$. Assume it is true for $n=k$. Then for
$n=k+1$,

$\quad\begin{array}{l}\displaystyle \begin{aligned} e^{x^2/2}\left(x-\frac d{dx}\right)^{k+1}e^{-x^2/2} &=e^{x^2/2}\left(x-\frac d{dx}\right) e^{-x^2/2}\left(-\frac d{dx}\right)^k e^{-x^2}\\ &=\left[e^{x^2/2}\left(x-\frac d{dx}\right)e^{x^2/2}\right] \left(-\frac d{dx}\right)^k e^{-x^2}\\ &=e^{x^2}\left(-\frac d{dx}\right)^{k+1}e^{-x^2}. \end{aligned}\end{array}$

Q.E.D.

Recurrence formula:

$\quad\begin{array}{l}\displaystyle \begin{aligned} H_{n+1}(x) &=e^{x^2}\left(-\frac d{dx}\right)^{n+1}e^{-x^2}\\ &=e^{x^2}\left(-\frac d{dx}\right)^n \left(2xe^{-x^2}\right). \end{aligned}\end{array}$

Using Leibniz's rule,

$\quad\begin{array}{l}\displaystyle \begin{aligned} H_{n+1}(x) &=e^{x^2}\sum_{j=0}^{n}\binom nj \left[\left(-\frac d{dx}\right)^j(2x)\right] \left[\left(-\frac d{dx}\right)^{n-j}e^{-x^2}\right]\\ &=e^{x^2}\binom n0(2x) \left(-\frac d{dx}\right)^n e^{-x^2} +e^{x^2}\binom n1(-2) \left(-\frac d{dx}\right)^{n-1}e^{-x^2}\\ &=2xH_n(x)-2nH_{n-1}(x). \end{aligned}\end{array}$

$\quad\begin{array}{l}\displaystyle H_{n+1}=2xH_n-2nH_{n-1},\end{array}$

Differential recurrence relations:

$\quad\begin{array}{l}\displaystyle \begin{aligned} \frac{dH_n}{dx} &=\frac d{dx}\left[ e^{x^2}\left(-\frac d{dx}\right)^n e^{-x^2} \right]\\ &=2x e^{x^2}\left(-\frac d{dx}\right)^n e^{-x^2} -e^{x^2}\left(-\frac d{dx}\right)^{n+1}e^{-x^2}\\ &=2xH_n-H_{n+1}\\ &=2xH_n-\left(2xH_n-2nH_{n-1}\right)\\ &=2nH_{n-1}. \end{aligned}\end{array}$

$\quad\begin{array}{l}\displaystyle H_n'=2nH_{n-1}.\end{array}$

Generating function:

$\quad\begin{array}{l}\displaystyle \begin{aligned} f(x,z) &=\sum_{n=0}^{\infty}a_nH_n(x)z^n\\ &=e^{x^2}\left\{ \sum_{n=0}^{\infty}a_n \left(-z\frac\partial{\partial x}\right)^n \right\}e^{-x^2}. \end{aligned}\end{array}$

Let $a_n=1/n!$:

$\quad\begin{array}{l}\displaystyle \begin{aligned} f(x,z) &=e^{x^2} \exp\left(-z\frac\partial{\partial x}\right)e^{-x^2}\\ &=e^{x^2}\exp\left\{ -x^2+\left[-z\frac\partial{\partial x},-x^2\right] +\frac1{2!}\left[-z\frac\partial{\partial x}, \left[-z\frac\partial{\partial x},-x^2\right]\right] +\cdots\right\}\\ &=e^{2xz-z^2}, \end{aligned}\end{array}$

*[This uses a Baker--Campbell--Hausdorff formula.]*

$\quad\begin{array}{l}\displaystyle \sum_{n=0}^{\infty}H_n(x)\frac{z^n}{n!} =e^{2xz-z^2}.\end{array}$

Integral representation:

we have

$\quad\begin{array}{l}\displaystyle \frac{e^{2xz-z^2}}{z^{m+1}} =\sum_{n=0}^{\infty}\frac1{n!}H_n(x)z^{n-m-1}.\end{array}$

The right-hand side is now a Laurent series about $z=0$.

(A Laurent series $\sum_{n=-k}^{\infty}a_nz^n$, that is, negative
powers.)

The coefficient of $z^{-1}$ is the residue at $z=0$, and

$\quad\begin{array}{l}\displaystyle \oint_\Gamma\frac{e^{2xz-z^2}}{z^{m+1}}\,dz =2\pi i\frac{H_m(x)}{m!},\end{array}$

if $\Gamma$ encloses zero. The coefficient of $z^{-1}$ is

$\quad\begin{array}{l}\displaystyle \frac1{m!}H_m(x),\end{array}$

so

$\quad\begin{array}{l}\displaystyle H_n(x)=\frac{n!}{2\pi i} \oint_\Gamma\frac{e^{2xz-z^2}}{z^{n+1}}\,dz.\end{array}$


<figure data-chart="residue-contour" style="margin: 1.5em auto; max-width: 23rem;">
  <img src="img/residue-contour.svg"
       alt="A positively oriented closed contour Gamma enclosing the poles z one, z two and z three."
       style="display: block; width: 100%; height: auto;" />
</figure>

$f(z)$ has poles at $z_1$, $z_2$ and $z_3$.

Near $z_1$ (say),

$\quad\begin{array}{l}\displaystyle f(z)\sim(z-z_1)^{-m}g(z),\end{array}$

where $g(z_1)$ is finite.

$\quad\begin{array}{l}\displaystyle \oint_\Gamma f(z)\,dz =2\pi i\left[ \operatorname{Res}(z_1)+\operatorname{Res}(z_2) +\operatorname{Res}(z_3) \right].\end{array}$

For a pole of order $m$,

$\quad\begin{array}{l}\displaystyle \operatorname{Res}(z_1) =\lim_{z\to z_1}\frac1{(m-1)!} \frac{d^{m-1}}{dz^{m-1}} \left[(z-z_1)^m f(z)\right].\end{array}$

Example in spherical polars:

$\quad\begin{array}{l}\displaystyle \left[-\nabla^2+f(r)\right]y=\lambda y.\end{array}$

$\quad\begin{array}{l}\displaystyle \left[ -\frac1{r^2}\frac\partial{\partial r} \left(r^2\frac\partial{\partial r}\right) -\frac1{r^2\sin\theta}\frac\partial{\partial\theta} \left(\sin\theta\frac\partial{\partial\theta}\right) -\frac1{r^2\sin^2\theta}\frac{\partial^2}{\partial\phi^2} +f(r) \right]y=\lambda y.\end{array}$

Let

$\quad\begin{array}{l}\displaystyle y=R(r)\Theta(\theta)\Phi(\phi),\end{array}$

$\quad\begin{array}{l}\displaystyle \begin{aligned} &-\frac1{r^2R}\frac d{dr}\left(r^2\frac{dR}{dr}\right) -\frac1{r^2\Theta\sin\theta} \frac d{d\theta}\left(\sin\theta\frac{d\Theta}{d\theta}\right)\\ &\hspace{5em} -\frac1{r^2\Phi\sin^2\theta}\frac{d^2\Phi}{d\phi^2} +f(r)=\lambda. \end{aligned}\end{array}$

Multiplying by $r^2\sin^2\theta$,

$\quad\begin{array}{l}\displaystyle -\frac{\sin^2\theta}{R}\frac d{dr} \left(r^2\frac{dR}{dr}\right) -\frac{\sin\theta}{\Theta}\frac d{d\theta} \left(\sin\theta\frac{d\Theta}{d\theta}\right) -\frac1\Phi\frac{d^2\Phi}{d\phi^2} +r^2\sin^2\theta f(r) =\lambda r^2\sin^2\theta.\end{array}$

Evidently $\Phi''/\Phi=k$. For $k=\alpha^2$,

$\quad\begin{array}{l}\displaystyle \Phi=A\sinh\alpha\phi+B\cosh\alpha\phi,\end{array}$

but we want $\Phi$ to have period $2\pi$ so that $y$ is a
single-valued function, hence $A=B=0$. For $k=0$,
$\Phi=A\phi+B$ and $A=0$. For $k=-\alpha^2$,

$\quad\begin{array}{l}\displaystyle \Phi=A\sin\alpha\phi+B\cos\alpha\phi,\end{array}$

which has period $2\pi$ if $\alpha=m$, an integer. Hence

$\quad\begin{array}{l}\displaystyle \frac{\Phi''}{\Phi}=-m^2.\end{array}$

$\quad\begin{array}{l}\displaystyle -\frac1R\frac d{dr}\left(r^2\frac{dR}{dr}\right) -\frac1{\Theta\sin\theta}\frac d{d\theta} \left(\sin\theta\frac{d\Theta}{d\theta}\right) +\frac{m^2}{\sin^2\theta} +r^2f(r)=\lambda r^2.\end{array}$

that is,

$\quad\begin{array}{l}\displaystyle -\frac1{\Theta\sin\theta} \frac d{d\theta}\left(\sin\theta\frac{d\Theta}{d\theta}\right) +\frac{m^2}{\sin^2\theta}=\mu\end{array}$

and

$\quad\begin{array}{l}\displaystyle -\frac1R\frac d{dr}\left(r^2\frac{dR}{dr}\right) +r^2f(r)=\mu+\lambda r^2.\end{array}$

Two eigenvalue problems. The second depends upon the function $f(r)$,
whereas the first is purely geometric. Notice that the first also
depends upon the parameter $m$.

Return to the example after a theory break.

Standard form:

$\quad\begin{array}{l}\displaystyle y''+r(x,m)y+\lambda y=0. \qquad\text{(1)}\end{array}$

Note: include $m$ as motivated by example.

<a id="definition-of-factorisation"></a>
Definition of factorisation:

Equation (1) may be factorised if it can be replaced by each of the two
equations

$\quad\begin{array}{l}\displaystyle H_{m+1}H_{m+1}^\dagger y(\lambda,m) =[\lambda-L(m+1)]y(\lambda,m). \qquad\text{(2)}\end{array}$

$\quad\begin{array}{l}\displaystyle H_m^\dagger H_my(\lambda,m) =[\lambda-L(m)]y(\lambda,m). \qquad\text{(3)}\end{array}$

where

$\quad\begin{array}{l}\displaystyle H_m=k(x,m)+D, \qquad H_m^\dagger=k(x,m)-D.\end{array}$

and $L$ is some function of $m$.

**Theorem.** If $y(\lambda,m)$ is a solution of equation (1), then

$\quad\begin{array}{l}\displaystyle y(\lambda,m+1)=H_{m+1}^\dagger y(\lambda,m),\end{array}$

and

$\quad\begin{array}{l}\displaystyle y(\lambda,m-1)=H_my(\lambda,m)\end{array}$

are corresponding to the same $\lambda$, but different $m$.

**Proof.** Multiply equation (2) by $H_{m+1}^\dagger$:

$\quad\begin{array}{l}\displaystyle \begin{aligned} H_{m+1}^\dagger(H_{m+1}H_{m+1}^\dagger y) &=[\lambda-L(m+1)]H_{m+1}^\dagger y,\\ (H_{m+1}^\dagger H_{m+1})(H_{m+1}^\dagger y) &=[\lambda-L(m+1)](H_{m+1}^\dagger y). \end{aligned}\end{array}$

From equation (3),

$\quad\begin{array}{l}\displaystyle H_{m+1}^\dagger y(\lambda,m)=y(\lambda,m+1).\end{array}$

Multiply equation (3) by $H_m$:

$\quad\begin{array}{l}\displaystyle (H_mH_m^\dagger)(H_my) =[\lambda-L(m)](H_my),\end{array}$

Comparing with equation (2),

$\quad\begin{array}{l}\displaystyle H_my(\lambda,m)=y(\lambda,m-1).\end{array}$

$H_m$ and $H_m^\dagger$ are ladder operators with respect to $m$,
$H_m$ going down and $H_m^\dagger$ going up.

**Theorem.** $H_m$ and $H_m^\dagger$ are adjoint operators (for
example, Hermitian) if

$\quad\begin{array}{l}\displaystyle \int_a^b\phi(H_m\psi)\,dx =\int_a^b(H_m^\dagger\phi)\psi\,dx,\end{array}$

where $\phi\psi$ vanishes at the end points and the integrand is
continuous in $(a,b)$.

**Proof.** Already done.

Inner product:

$\quad\begin{array}{l}\displaystyle (\phi,\psi)=\int_a^b \phi^*\psi\,dx.\end{array}$

Norm:

$\quad\begin{array}{l}\displaystyle \|\phi\|^2=(\phi,\phi).\end{array}$

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

$\quad\begin{array}{l}\displaystyle \lambda=\lambda_l=L(l+1), \qquad m=0,1,\ldots,l.\end{array}$

**Proof.** Assume that $y(\lambda,m)$ is $L^2$. Then

$\quad\begin{array}{l}\displaystyle y(\lambda,m+1)=H_{m+1}^\dagger y(\lambda,m)\end{array}$

is also $L^2$ and vanishes at the endpoints (see previous theorem).

$\quad\begin{array}{l}\displaystyle \begin{aligned} \|y(\lambda,m+1)\|^2 &=\bigl(H_{m+1}^\dagger y(\lambda,m), H_{m+1}^\dagger y(\lambda,m)\bigr)\\ &=\bigl(y(\lambda,m), H_{m+1}H_{m+1}^\dagger y(\lambda,m)\bigr)\\ &=[\lambda-L(m+1)]\|y(\lambda,m)\|^2. \end{aligned}\end{array}$

Likewise,

$\quad\begin{array}{l}\displaystyle \begin{aligned} \|y(\lambda,m+2)\|^2 &=[\lambda-L(m+2)]\|y(\lambda,m+1)\|^2\\ &=[\lambda-L(m+2)][\lambda-L(m+1)] \|y(\lambda,m)\|^2. \end{aligned}\end{array}$

$L$ is an increasing function of $m$, and so for some integer $l$ we
will have $\lambda-L(l+1)<0$, that is,
$\|y(\lambda,l+1)\|^2<0$, which is against the rules, so we have

$\quad\begin{array}{l}\displaystyle \lambda=L(l+1), \qquad H_{l+1}^\dagger y(\lambda,l)=0.\end{array}$

This fixes $\lambda$ in terms of $l$, the maximum value of $m$. All
other $m$ are less than $l$, and the eigenvalue is
$L(l+1)-L(m+1)$.

**Theorem.** If $L(m)$ is a decreasing function of $m$ for
$0<m\leq M$ $(\leq\infty)$, and $\lambda<L(\infty)$, a necessary condition
for the existence of an $L^2$ solution is that

$\quad\begin{array}{l}\displaystyle \lambda=\lambda_l=L(l), \qquad m=l,l+1,l+2,\ldots.\end{array}$

**Proof.** Assume that $y(\lambda,m)$ is $L^2$:

$\quad\begin{array}{l}\displaystyle \begin{aligned} \|y(\lambda,m-1)\|^2 &=\bigl(H_my(\lambda,m),H_my(\lambda,m)\bigr)\\ &=\bigl(y(\lambda,m),H_m^\dagger H_my(\lambda,m)\bigr)\\ &=[\lambda-L(m)]\|y(\lambda,m)\|^2, \end{aligned}\end{array}$

Likewise,

$\quad\begin{array}{l}\displaystyle \|y(\lambda,m-2)\|^2 =[\lambda-L(m-1)][\lambda-L(m)]\|y(\lambda,m)\|^2.\end{array}$

Continue until there is an integer $l$ such that $\lambda-L(l)<0$.
This gives a contradiction, so

$\quad\begin{array}{l}\displaystyle \lambda=L(l), \qquad \|y(\lambda,l-1)\|=0.\end{array}$

That is, $y(\lambda,l-1)=0$, or $H_l y(\lambda,l)=0$. $l$ is the
lowest value of $m$, and $m=l,l+1,l+2,\ldots$.

In the above we have taken $m>0$. This is not necessary. $m$ may start
at some $m_0$, not necessarily an integer. Then $|l-m|$ must be an
integer. We now write the solutions as $y_l^m(x)$, or $Y_l^m(x)$ when
normalised.

Suppose $Y_l^m(x)$ is normalised, that is,

$\quad\begin{array}{l}\displaystyle \|Y_l^m(x)\|^2 =\int_a^bY_l^{m*}Y_l^m\,dx =1.\end{array}$

Then

$\quad\begin{array}{l}\displaystyle y_l^{m+1}=H_{m+1}^\dagger Y_l^m,\end{array}$

so that

$\quad\begin{array}{l}\displaystyle \begin{aligned} \|y_l^{m+1}\|^2 &=\|H_{m+1}^\dagger Y_l^m\|^2\\ &=\left(H_{m+1}^\dagger Y_l^m, H_{m+1}^\dagger Y_l^m\right)\\ &=\left(Y_l^m,H_{m+1}H_{m+1}^\dagger Y_l^m\right)\\ &=[L(l+1)-L(m+1)]\|Y_l^m\|^2\\ &=L(l+1)-L(m+1). \end{aligned}\end{array}$

That is, $y_l^{m+1}$ is not normalised. Likewise,

$\quad\begin{array}{l}\displaystyle y_l^{m-1}=H_mY_l^m,\end{array}$

and

$\quad\begin{array}{l}\displaystyle \|y_l^{m-1}\|^2 =[L(l)-L(m)]\|Y_l^m\|^2 =L(l)-L(m),\end{array}$

that is, $y_l^{m-1}$ is not normalised. Overcome the problem by
defining

$\quad\begin{array}{l}\displaystyle \begin{aligned} \mathcal H_l^{\dagger,m+1} &=[L(l+1)-L(m+1)]^{-1/2}H_{m+1}^\dagger,\\ \mathcal H_l^m &=[L(l)-L(m)]^{-1/2}H_m. \end{aligned}\end{array}$

If we calculate $y_l^l$ from either
$H_{l+1}^\dagger y_l^l=0$ (up) or
$H_l y_l^l=0$ (down), and normalise to $Y_l^l$, then
the $Y_l^m$ generated by $\mathcal H^\dagger$ and $\mathcal H$ are
normalised automatically. The differential equation becomes

$\quad\begin{array}{l}\displaystyle \mathcal H_l^{m+1}\mathcal H_l^{\dagger,m+1}Y_l^m=Y_l^m, \qquad \mathcal H_l^{\dagger,m}\mathcal H_l^mY_l^m=Y_l^m.\end{array}$

Chart for class 1 solutions: $L(m)$ is increasing.

<figure data-chart="factorisation-class-i" style="margin: 1.5em auto; max-width: 40rem;">
  <img src="img/factorisation-class-i.svg"
       alt="Chart for class I solutions, with L of m increasing."
       style="display: block; width: 100%; height: auto;" />
</figure>

Basic solution $y_l^l$ comes from

$\quad\begin{array}{l}\displaystyle H_{l+1}^\dagger y_l^l=0,\end{array}$

which is a first-order differential equation:

$\quad\begin{array}{l}\displaystyle \left[k(x,l+1)-\frac d{dx}\right]y_l^l=0.\end{array}$

$\quad\begin{array}{l}\displaystyle y_l^l=C\exp\left(\int k(x,l+1)\,dx\right).\end{array}$

$C$ is found from requiring $\|y_l^l\|=1$. Obtain $Y_l^m$ from

$\quad\begin{array}{l}\displaystyle \mathcal H_l^mY_l^m=Y_l^{m-1}, \qquad m=l,l-1,\ldots,0.\end{array}$

Chart for class II solutions: $L(m)$ is a decreasing function.

<figure data-chart="factorisation-class-ii" style="margin: 1.5em auto; max-width: 40rem;">
  <img src="img/factorisation-class-ii.svg"
       alt="Chart for class II solutions, with L of m decreasing."
       style="display: block; width: 100%; height: auto;" />
</figure>

Basic solutions $Y_l^l$ come from

$\quad\begin{array}{l}\displaystyle H_l y_l^l=0\end{array}$

that is,

$\quad\begin{array}{l}\displaystyle \left[k(x,l)+\frac d{dx}\right]Y_l^l=0,\end{array}$

so

$\quad\begin{array}{l}\displaystyle y_l^l=C\exp\left(-\int k(x,l)\,dx\right),\end{array}$

$C$ is determined by the normalisation condition $\|Y_l^l\|=1$.

Factorisation types:

$\quad\begin{array}{l}\displaystyle [-r(x,m)-D^2]y=\lambda y.\end{array}$

Seek

$\quad\begin{array}{l}\displaystyle H_m=k(x,m)+D, \qquad H_m^\dagger=k(x,m)-D,\end{array}$

such that

$\quad\begin{array}{l}\displaystyle \begin{aligned} H_{m+1}H_{m+1}^\dagger y&=[\lambda-L(m+1)]y,\\ H_m^\dagger H_my&=[\lambda-L(m)]y. \end{aligned}\end{array}$

that is,

$\quad\begin{array}{l}\displaystyle H_{m+1}H_{m+1}^\dagger+L(m+1)=-r(x,m)-D^2,\end{array}$

and

$\quad\begin{array}{l}\displaystyle H_m^\dagger H_m+L(m)=-r(x,m)-D^2,\end{array}$

which is

$\quad\begin{array}{l}\displaystyle \begin{aligned} k^2(x,m+1)+k'(x,m+1)+L(m+1)&=-r(x,m),\\ k^2(x,m)-k'(x,m)+L(m)&=-r(x,m). \end{aligned}\end{array}$

Subtracting,

$\quad\begin{array}{l}\displaystyle k^2(x,m+1)-k^2(x,m) +k'(x,m+1)+k'(x,m)=L(m)-L(m+1).\end{array}$

What $k$'s and $L$'s?

1. Let $k(x,m)=f(m)$. Then $L(m)=-f^2(m)$ and the differential
   equation is

   $\quad\begin{array}{l}\displaystyle y''+\lambda y=0,\end{array}$

   which is a bit simple.

2. Let

$\quad\begin{array}{l}\displaystyle k(x,m)=k_0(x)+mk_1(x).\end{array}$

Substituting

$\quad\begin{array}{l}\displaystyle \begin{aligned} &(m+1)^2(k_1^2+k_1') +2(m+1)(k_0k_1+k_0')\\ &\qquad -m^2(k_1^2+k_1')-2m(k_0k_1+k_0') =L(m)-L(m+1). \end{aligned}\end{array}$

that is,

$\quad\begin{array}{l}\displaystyle L(m)=-\left[ m^2(k_1^2+k_1')+2m(k_0k_1+k_0')+f(m,x) \right],\end{array}$

where $f(m+1,x)=f(m,x)$, that is, $f(m,x)$ has period one in $m$.
Since we are only interested in integer increments in $m$, take
$f(m,x)=f(x)$. Separate by coefficients of powers of $m$:

$\quad\begin{array}{l}\displaystyle k_1^2+k_1'=-a^2\end{array}$

and

$\quad\begin{array}{l}\displaystyle k_0k_1+k_0'= \begin{cases} -ca,&a\ne0,\\ b,&a=0. \end{cases}\end{array}$

For $m^0$, $f(x)$ is constant.

Since $f(x)$ is constant it just shifts $L(m)$ by a constant amount
for any $m$; the constant may take any value and is usually zero.

Case $a\ne0$,

$\quad\begin{array}{l}\displaystyle \begin{array}{ll} \text{A:}& k_1=a\cot a(x+p),\quad k_0=ca\cot a(x+p)+\dfrac d{\sin a(x+p)},\\[8pt] \text{B:}& k_1=ia,\quad k_0=cia+d e^{-iax} \quad\text{(a particular solution)},\\[6pt] \end{array}\end{array}$

Case $a=0$,

$\quad\begin{array}{l}\displaystyle \begin{array}{ll} \text{C:}& k_1=\dfrac1x,\quad k_0=\dfrac12bx+\dfrac dx,\\[8pt] \text{D:}& k_1=0,\quad k_0=bx+d \quad\text{(a particular solution)}. \end{array}\end{array}$

3. Try

$\quad\begin{array}{l}\displaystyle k(x,m)=\sum_{i=0}^n k_i(x)m^i, \qquad n<\infty.\end{array}$

leads to nothing new.

4. Try

$\quad\begin{array}{l}\displaystyle k(x,m)=\frac{k_{-1}(x)}m+k_0(x)+mk_1(x)\end{array}$

leads to

$\quad\begin{array}{l}\displaystyle \begin{array}{ll} \text{E:}& k_1=a\cot a(x+p),\quad k_0=0,\quad k_{-1}=q,\\[6pt] \text{F:}& k_1=\dfrac1x,\quad k_0=0,\quad k_{-1}=q. \end{array}\end{array}$

For E and F,

$\quad\begin{array}{l}\displaystyle L(m)=a^2m^2-\frac{q^2}{m^2},\end{array}$

with $a=0$ in F. Trying

$\quad\begin{array}{l}\displaystyle k(x,m)=\sum_{i=-n}^{1}k_i(x)m^i, \qquad n<\infty,\end{array}$

Nothing new!

Examples:

Type A:

$\quad\begin{array}{l}\displaystyle r(x,m)= \frac{ -a^2(m+c)(m+c+1)+d^2 +2ad\left(m+c+\frac12\right)\cos a(x+p) }{\sin^2a(x+p)}.\end{array}$

$\quad\begin{array}{l}\displaystyle k=(m+c)a\cot a(x+p)+\frac d{\sin a(x+p)},\end{array}$

$\quad\begin{array}{l}\displaystyle L(m)=a^2(m+c)^2.\end{array}$

<a id="associated-spherical-harmonics"></a>
### Associated spherical harmonics

The “angle” operator in spherical polars with radial symmetry in the
field is

$\quad\begin{array}{l}\displaystyle \frac1{\sin\theta} \frac d{d\theta}\left(\sin\theta\frac{dP}{d\theta}\right) +\left[\lambda-\frac{m^2}{\sin^2\theta}\right]P=0.\end{array}$

where $0\leq\theta\leq\pi$.

To put into standard form let

$\quad\begin{array}{l}\displaystyle Y=(\sin\theta)^{1/2}P,\end{array}$

which gives

$\quad\begin{array}{l}\displaystyle Y''-\frac{m^2-\tfrac{1}{4}}{\sin^2\theta}Y +\left(\lambda+\frac14\right)Y=0.\end{array}$

Comparing with the standard A form for $r(x,m)$,

$\quad\begin{array}{l}\displaystyle a=1, \qquad p=0, \qquad d=0, \qquad c=-\frac12,\end{array}$

and

$\quad\begin{array}{l}\displaystyle \lambda'=\lambda+\frac14,\end{array}$

so that

$\quad\begin{array}{l}\displaystyle k(\theta,m)=\left(m-\frac12\right)\cot\theta, \qquad L(m)=\left(m-\frac12\right)^2.\end{array}$

Since $L(m)$ is an increasing function of $m$ for $2m>0$, this is a
class 1 problem, that is, there exists a value $m=l$ such that

$\quad\begin{array}{l}\displaystyle \lambda'=L(l+1)=\left(l+\frac12\right)^2,\end{array}$

so

$\quad\begin{array}{l}\displaystyle \lambda=\lambda'-\frac14=l(l+1), \qquad l=0,1,2,\ldots,\quad l\geq m.\end{array}$

Eigenfunctions:

$\quad\begin{array}{l}\displaystyle H_{l+1}^\dagger y_l^l=0.\end{array}$

$\quad\begin{array}{l}\displaystyle \left[k(\theta,l+1)-\frac d{d\theta}\right]y_l^l=0.\end{array}$

$\quad\begin{array}{l}\displaystyle \left[\left(l+\frac12\right)\cot\theta -\frac d{d\theta}\right]Y_l^l=0,\end{array}$

$\quad\begin{array}{l}\displaystyle \left(l+\frac12\right)\frac{\cos\theta}{\sin\theta}\,d\theta =\frac{dY_l^l}{Y_l^l}.\end{array}$

$\quad\begin{array}{l}\displaystyle Y_l^l(\theta)=K\sin^{l+1/2}\theta.\end{array}$

To find the normalised eigenfunction $Y_l^l$,

$\quad\begin{array}{l}\displaystyle \begin{aligned} K^{-2} &=\int_0^\pi (\sin\theta)^{l+1/2}(\sin\theta)^{l+1/2}\,d\theta\\ &=\int_0^\pi(\sin\theta)^{2l+1}\,d\theta\\ &=\frac21\frac23\frac45\cdots\frac{2l}{2l+1}\\ &=\frac21\frac22\frac23\frac44\frac45\cdots \frac{2l}{2l}\frac{2l}{2l+1}\\ &=\frac{2^{2l+1}l!\,l!}{(2l+1)!}. \end{aligned}\end{array}$

$\quad\begin{array}{l}\displaystyle Y_l^l(\theta)= \left[ \frac{(2l+1)!}{2^{2l+1}l!\,l!} \right]^{1/2} \sin^{l+1/2}\theta.\end{array}$

Also,

$\quad\begin{array}{l}\displaystyle Y_l^{m-1}= \frac{ \left[\left(m-\frac12\right)\cot\theta +\dfrac d{d\theta}\right]Y_l^m }{\sqrt{(l+m)(l-m+1)}},\end{array}$

and

$\quad\begin{array}{l}\displaystyle Y_l^{m+1}= \frac{ \left[\left(m+\frac12\right)\cot\theta -\dfrac d{d\theta}\right]Y_l^m }{\sqrt{(l+m+1)(l-m)}}.\end{array}$

Note that replacing $m$ by $-m$ in the original differential equation
does not change the problem. Hence solutions for
$-m$, with $m=0,1,\ldots,l$, exist and

$\quad\begin{array}{l}\displaystyle Y_l^{-m}=(-1)^mY_l^m.\end{array}$

Associated spherical harmonics as a class 2 problem. Go back to

$\quad\begin{array}{l}\displaystyle \frac1{\sin\theta} \frac d{d\theta}\left(\sin\theta\frac{dP_l^m}{d\theta}\right) +\left[l(l+1)-\frac{m^2}{\sin^2\theta}\right]P_l^m=0,\end{array}$

and replace $m^2$ by $\lambda$ and $\lambda$ by $l(l+1)$, so that

$\quad\begin{array}{l}\displaystyle \frac1{\sin\theta} \frac d{d\theta}\left(\sin\theta\frac{dP}{d\theta}\right) -\frac{\lambda}{\sin^2\theta}P+l(l+1)P=0.\end{array}$

Normal form: let

$\quad\begin{array}{l}\displaystyle z=\log\tan\frac\theta2.\end{array}$

and let $P(\theta)\mapsto Q(z)$ to obtain

$\quad\begin{array}{l}\displaystyle \frac{d^2Q}{dz^2} +\frac{l(l+1)}{\cosh^2z}Q+\lambda Q=0,\end{array}$

Comparing with standard form, take

$\quad\begin{array}{l}\displaystyle d=0,\qquad p=\frac\pi2,\qquad a=i,\qquad c=0,\end{array}$

and replace $x,m$ with $z,l$. Then

$\quad\begin{array}{l}\displaystyle k(z,l)=l\tanh z, \qquad L(l)=-l^2.\end{array}$

which is patently class 2.

$\quad\begin{array}{l}\displaystyle \frac{d^2Q}{dz^2} +\frac{l(l+1)}{\cosh^2z}Q+\lambda Q=0.\end{array}$

The bottom of the ladder is reached when
$l$ is some value $m$:

$\quad\begin{array}{l}\displaystyle \lambda_m=L(m)=-m^2, \qquad l-m=0,1,2,\ldots.\end{array}$

Then

$\quad\begin{array}{l}\displaystyle \begin{aligned} Q_m^m &=C\exp\left(-\int m\tanh z\,dz\right)\\ &=C\cosh^{-m}z. \end{aligned}\end{array}$

Also,

$\quad\begin{array}{l}\displaystyle \begin{aligned} \lVert Q_m^m\rVert^2 &=\int_{-\infty}^{\infty}C^2\cosh^{-2m}z\,dz\\ &=2C^2\int_0^\infty\cosh^{-2m}z\,dz\\ &=2C^2 4^{m-1}B(m,m). \qquad (*) \end{aligned}\end{array}$

$\quad\begin{array}{l}\displaystyle I=\int_0^\infty\operatorname{sech}^{2m}z\,dz.\end{array}$

(i)

$\quad\begin{array}{l}\displaystyle \begin{aligned} I &=\int_0^\infty\operatorname{sech}^{2m-2}z \operatorname{sech}^2z\,dz\\ &=\int_0^1(1-\eta^2)^{m-1}\,d\eta\\ &=\sum_{i=0}^{m-1}(-1)^i \binom{m-1}{i}\int_0^1\eta^{2i}\,d\eta\\ &=\sum_{i=0}^{m-1}(-1)^i \frac1{2i+1}\binom{m-1}{i}. \end{aligned}\end{array}$

(ii)

$\quad\begin{array}{l}\displaystyle \begin{aligned} I &=\int_0^{\pi/2}\sin^{2m-1}\theta\,d\theta\\ &=\int_0^{\pi/2}\sin^{2m-2}\theta\sin\theta\,d\theta\\ &=-\left.\cos\theta\sin^{2m-2}\theta\right|_0^{\pi/2} +(2m-2)\int_0^{\pi/2}\sin^{2m-3}\theta\cos^2\theta\,d\theta\\ &=(2m-2)\int_0^{\pi/2}\sin^{2m-3}\theta\,d\theta -(2m-2)\int_0^{\pi/2}\sin^{2m-1}\theta\,d\theta. \end{aligned}\end{array}$

Hence

$\quad\begin{array}{l}\displaystyle I=\frac{2m-2}{2m-1}\frac{2m-4}{2m-3} \frac{2m-6}{2m-5}\cdots\frac43\frac23 =\frac{\{2^{m-1}(m-1)!\}^2}{(2m-1)!}.\end{array}$

From $(*)$, where $B(m,m)$ is the Beta function,

$\quad\begin{array}{l}\displaystyle 1=C^2 2^{2m-1}B(m,m),\end{array}$

$\quad\begin{array}{l}\displaystyle \begin{aligned} C^{-2} &=2^{2m-1}\frac{\Gamma(m)\Gamma(m)}{\Gamma(m+m)}\\ &=2^{2m-1}\frac{(m-1)!(m-1)!}{(2m-1)!}. \end{aligned}\end{array}$

Beta function:

$\quad\begin{array}{l}\displaystyle B(x,y)=\int_0^1t^{x-1}(1-t)^{y-1}\,dt.\end{array}$

$\quad\begin{array}{l}\displaystyle \int_0^1(1-\eta^2)^{m-1}\,d\eta =\int_0^1(1-\eta)^{m-1}(1+\eta)^{m-1}\,d\eta.\end{array}$

Plan I:

$\quad\begin{array}{l}\displaystyle 1-\eta=2\xi,\end{array}$

$\quad\begin{array}{l}\displaystyle 1+\eta=2-(1-\eta)=2-2\xi.\end{array}$

$I$ becomes

$\quad\begin{array}{l}\displaystyle \begin{aligned} I &=2\int_{1/2}^1(2-2\xi)^{m-1}(2\xi)^{m-1}\,d\xi\\ &=2^{2m-1}\int_{1/2}^1\xi^{m-1}(1-\xi)^{m-1}\,d\xi. \end{aligned}\end{array}$

(symmetric about $\xi=\frac12$ since both powers are $m-1$)

$\quad\begin{array}{l}\displaystyle \begin{aligned} I &=2^{2m-2}\int_0^1 \xi^{m-1}(1-\xi)^{m-1}\,d\xi\\ &=2^{2m-2}B(m,m). \end{aligned}\end{array}$

$\quad\begin{array}{l}\displaystyle C=\frac{[2(2m-1)!]^{1/2}}{2^m(m-1)!},\end{array}$

so

$\quad\begin{array}{l}\displaystyle Q_m^m(z)= \frac{[2(2m-1)!]^{1/2}}{2^m(m-1)!}\cosh^{-m}z.\end{array}$

Note that

$\quad\begin{array}{l}\displaystyle Q_l^m= \frac{l\tanh z-\dfrac d{dz}} {\sqrt{(l-m)(l+m)}}Q_{l-1}^m, \qquad l-1\geq m.\end{array}$

In the original coordinate,

$\quad\begin{array}{l}\displaystyle P_l^m= \frac{-l\cos\theta-\sin\theta\dfrac d{d\theta}} {\sqrt{(l-m)(l+m)}}P_{l-1}^m,\end{array}$

and

$\quad\begin{array}{l}\displaystyle P_{l-1}^m= \frac{-l\cos\theta+\sin\theta\dfrac d{d\theta}} {\sqrt{(l-m)(l+m)}}P_l^m.\end{array}$

<a id="spherical-harmonics-so3"></a>
### Eigenfunctions of the spherical harmonics associated with $SO(3)$

Rotation about $Oz$:

$\quad\begin{array}{l}\displaystyle \bar x=x\cos\theta+y\sin\theta, \qquad \bar y=-x\sin\theta+y\cos\theta, \qquad \bar z=z.\end{array}$

For some angle $\theta$. an infinitesimal angle $d\theta$,

$\quad\begin{array}{l}\displaystyle \bar x=x+y\,d\theta, \qquad \bar y=y-x\,d\theta, \qquad \bar z=z.\end{array}$

Seek a generator of the infinitesimal transformation.

In general, under an infinitesimal transformation generated by $G$,
$f(x,y,z)$ goes to

$\quad\begin{array}{l}\displaystyle \bar f=(1+\varepsilon G)f, \qquad G=\eta_1\frac\partial{\partial x} +\eta_2\frac\partial{\partial y} +\eta_3\frac\partial{\partial z}.\end{array}$

For $f=x$,

$\quad\begin{array}{l}\displaystyle \bar x=x+\varepsilon\eta_1.\end{array}$

For $f=y$,

$\quad\begin{array}{l}\displaystyle \bar y=y+\varepsilon\eta_2.\end{array}$

For $f=z$,

$\quad\begin{array}{l}\displaystyle \bar z=z+\varepsilon\eta_3.\end{array}$

Comparing,

$\quad\begin{array}{l}\displaystyle \eta_1=y, \qquad \eta_2=-x, \qquad \eta_3=0,\end{array}$

$\quad\begin{array}{l}\displaystyle G_1=y\frac\partial{\partial x}-x\frac\partial{\partial y}.\end{array}$

Similarly, for rotations about $Ox$,

$\quad\begin{array}{l}\displaystyle G_2=z\frac\partial{\partial y}-y\frac\partial{\partial z},\end{array}$

and about $Oy$,

$\quad\begin{array}{l}\displaystyle G_3=x\frac\partial{\partial z}-z\frac\partial{\partial x}.\end{array}$

$\quad\begin{array}{l}\displaystyle \begin{aligned} [G_1,G_2]&=G_3,\\ [G_2,G_3]&=G_1,\\ [G_3,G_1]&=G_2, \end{aligned}\end{array}$

$\quad\begin{array}{l}\displaystyle [G_i,G_j]=\varepsilon_{ijk}G_k,\end{array}$

that is, a standard representation of $SO(3)$.

Interested in invariance under rotation, that is, spherical symmetry.

So convert to spherical polars:

$\quad\begin{array}{l}\displaystyle x=r\sin\theta\cos\phi, \qquad y=r\sin\theta\sin\phi, \qquad z=r\cos\theta.\end{array}$

$\quad\begin{array}{l}\displaystyle \begin{aligned} \frac{\partial}{\partial r} &=\sin\theta\cos\phi\frac{\partial}{\partial x} +\sin\theta\sin\phi\frac{\partial}{\partial y} +\cos\theta\frac{\partial}{\partial z},\\ \frac1r\frac{\partial}{\partial\theta} &=\cos\theta\cos\phi\frac{\partial}{\partial x} +\cos\theta\sin\phi\frac{\partial}{\partial y} -\sin\theta\frac{\partial}{\partial z},\\ \frac1{r\sin\theta}\frac{\partial}{\partial\phi} &=-\sin\phi\frac{\partial}{\partial x} +\cos\phi\frac{\partial}{\partial y}. \end{aligned}\end{array}$

$\quad\begin{array}{l}\displaystyle \begin{pmatrix} \partial_x\\[2pt] \partial_y\\[2pt] \partial_z \end{pmatrix} = \begin{pmatrix} \sin\theta\cos\phi & \sin\theta\sin\phi & \cos\theta\\ \cos\theta\cos\phi & \cos\theta\sin\phi & -\sin\theta\\ -\sin\phi & \cos\phi & 0 \end{pmatrix}^{-1} \begin{pmatrix} \partial_r\\[2pt] r^{-1}\partial_\theta\\[2pt] (r\sin\theta)^{-1}\partial_\phi \end{pmatrix}.\end{array}$

which gives

$\quad\begin{array}{l}\displaystyle \begin{aligned} G_1&=-\frac\partial{\partial\phi},\\ G_2&=\sin\phi\frac\partial{\partial\theta} +\cos\phi\cot\theta\frac\partial{\partial\phi},\\ G_3&=-\cos\phi\frac\partial{\partial\theta} +\sin\phi\cot\theta\frac\partial{\partial\phi}. \end{aligned}\end{array}$

Define:

$\quad\begin{array}{l}\displaystyle \begin{aligned} L_\pm &=i(G_2\pm iG_3)\\ &=i\left[ \sin\phi\frac\partial{\partial\theta} +\cos\phi\cot\theta\frac\partial{\partial\phi} \right]\\ &\quad{} \pm\left[ -\cos\phi\frac\partial{\partial\theta} +\sin\phi\cot\theta\frac\partial{\partial\phi} \right]\\ &=\pm(\cos\phi\pm i\sin\phi) \frac\partial{\partial\theta} +i(\cos\phi\pm i\sin\phi)\cot\theta \frac\partial{\partial\phi}\\ &=e^{\pm i\phi} \left(i\cot\theta\frac\partial{\partial\phi} \pm\frac\partial{\partial\theta}\right), \end{aligned}\end{array}$

$\quad\begin{array}{l}\displaystyle \begin{aligned} [L_+,L_-] &=[i(G_2+iG_3),i(G_2-iG_3)]\\ &=-i[G_3,G_2]+i[G_2,G_3]\\ &=2i[G_2,G_3]\\ &=2iG_1\\ &=-2i\frac\partial{\partial\phi}. \end{aligned}\end{array}$

Define:

$\quad\begin{array}{l}\displaystyle L_3=-i\frac\partial{\partial\phi}=iG_1.\end{array}$

For a spherically symmetric problem involving the Laplacian, the
differential equation is of the form

$\quad\begin{array}{l}\displaystyle \left[ \frac1{r^2}\frac\partial{\partial r} r^2\frac\partial{\partial r} +\frac1{r^2\sin\theta}\frac\partial{\partial\theta} \sin\theta\frac\partial{\partial\theta} +\frac1{r^2\sin^2\theta}\frac{\partial^2}{\partial\phi^2} +f(r) \right]\psi=0.\end{array}$

Let

$\quad\begin{array}{l}\displaystyle \psi=R(r)\Phi(\theta,\phi).\end{array}$

Then

$\quad\begin{array}{l}\displaystyle \frac1R\frac d{dr}\left(r^2\frac{dR}{dr}\right)+r^2f(r) =-\frac1\Phi\left[ \frac1{\sin\theta}\frac\partial{\partial\theta} \left(\sin\theta\frac\partial{\partial\theta}\right) +\frac1{\sin^2\theta}\frac{\partial^2}{\partial\phi^2} \right]\Phi =\lambda,\end{array}$

which gives rise to the eigenvalue problem

$\quad\begin{array}{l}\displaystyle -\left[ \frac1{\sin\theta}\frac\partial{\partial\theta} \sin\theta\frac\partial{\partial\theta} +\frac1{\sin^2\theta}\frac{\partial^2}{\partial\phi^2} \right]\Phi=\lambda\Phi.\end{array}$

Observe that

$\quad\begin{array}{l}\displaystyle L^2=-(G_1^2+G_2^2+G_3^2) =-\left[ \frac1{\sin\theta}\frac\partial{\partial\theta} \sin\theta\frac\partial{\partial\theta} +\frac1{\sin^2\theta}\frac{\partial^2}{\partial\phi^2} \right].\end{array}$

Looking at the problem,

$\quad\begin{array}{l}\displaystyle L^2\psi=\lambda\psi.\end{array}$

Also,

$\quad\begin{array}{l}\displaystyle \begin{aligned} [L^2,L_3] &=[-G_1^2-G_2^2-G_3^2,iG_1]\\ &=-i[G_2^2,G_1]-i[G_3^2,G_1]\\ &=-iG_2[G_2,G_1]-i[G_2,G_1]G_2\\ &\quad{}-iG_3[G_3,G_1]-i[G_3,G_1]G_3\\ &=iG_2G_3+iG_3G_2-iG_3G_2-iG_2G_3\\ &=0, \end{aligned}\end{array}$

This means that $L^2$ and $L_3$ have simultaneous eigenfunctions.

Let $\psi$ be the eigenfunction of $L^2$. Then

$\quad\begin{array}{l}\displaystyle \begin{aligned} L_3L^2\psi &=([L_3,L^2]+L^2L_3)\psi\\ &=L^2L_3\psi, \end{aligned}\end{array}$

so $L^2(L_3\psi)=\lambda(L_3\psi)$. Moreover, $\psi$ and $L_3\psi$
are eigenfunctions to the same eigenvalue $\lambda$.

Write

$\quad\begin{array}{l}\displaystyle L^2\psi_{\lambda,\mu}=\lambda\psi_{\lambda,\mu}, \qquad L_3\psi_{\lambda,\mu}=\mu\psi_{\lambda,\mu}.\end{array}$

Consider

$\quad\begin{array}{l}\displaystyle \begin{aligned} L_3(L_\pm\psi_{\lambda,\mu}) &=\bigl([L_3,L_\pm]+L_\pm L_3\bigr) \psi_{\lambda,\mu}\\ &=(\pm L_\pm+\mu L_\pm)\psi_{\lambda,\mu}\\ &=(\mu\pm1)L_\pm\psi_{\lambda,\mu}, \end{aligned}\end{array}$

that is, $L_\pm$ are ladder operators for $\mu$:

$\quad\begin{array}{l}\displaystyle L_\pm\psi_{\lambda,\mu} =(\text{constant})\psi_{\lambda,\mu\pm1}.\end{array}$

Now

$\quad\begin{array}{l}\displaystyle \begin{aligned} (L_+L_-)^\dagger &=(L_-)^\dagger(L_+)^\dagger =L_+L_-,\\ (L_-L_+)^\dagger &=(L_+)^\dagger(L_-)^\dagger =L_-L_+, \end{aligned}\end{array}$

that is, $L_+L_-$ and $L_-L_+$ are both Hermitian operators. Just as
for matrices of the form $AA^\dagger\sim A^\dagger A$, the
eigenvalues are real and non-negative, having the form
$\bar\lambda\lambda$, that is, $|\lambda|^2$, so $L_+L_-$ and
$L_-L_+$ have non-negative eigenvalues.

Now

$\quad\begin{array}{l}\displaystyle \begin{aligned} L_+L_- &=i(G_2+iG_3)i(G_2-iG_3)\\ &=-G_2^2-G_3^2+i[G_2,G_3]\\ &=-(G_1^2+G_2^2+G_3^2)+G_1^2+iG_1\\ &=L^2-L_3^2+L_3, \end{aligned}\end{array}$

and

$\quad\begin{array}{l}\displaystyle \begin{aligned} L_-L_+ &=i(G_2-iG_3)i(G_2+iG_3)\\ &=L^2-L_3^2-L_3. \end{aligned}\end{array}$

Hence

$\quad\begin{array}{l}\displaystyle \begin{aligned} L_+L_-\psi_{\lambda,\mu} &=(\lambda-\mu^2+\mu)\psi_{\lambda,\mu},\\ L_-L_+\psi_{\lambda,\mu} &=(\lambda-\mu^2-\mu)\psi_{\lambda,\mu}. \end{aligned}\end{array}$

Since $L_+L_-$ and $L_-L_+$ have non-negative eigenvalues,

$\quad\begin{array}{l}\displaystyle \lambda-\mu^2+\mu\geq0, \qquad \lambda-\mu^2-\mu\geq0.\end{array}$

Since $L_-$ is a lowering operator, $\lambda-\mu^2+\mu$ will
eventually become negative unless there exists $\mu'$ such that

$\quad\begin{array}{l}\displaystyle L_-\psi_{\lambda,\mu'}=0\end{array}$

and so

$\quad\begin{array}{l}\displaystyle \lambda-\mu'^2+\mu'=0.\end{array}$

Likewise, $L_+$ is a raising operator and
$\lambda-\mu^2-\mu$ will become negative unless there exists a
$\mu''$ such that

$\quad\begin{array}{l}\displaystyle L_+\psi_{\lambda,\mu''}=0\end{array}$

and

$\quad\begin{array}{l}\displaystyle \lambda-\mu''^2-\mu''=0.\end{array}$

$L_+$ raises ($L_-$ lowers) $\mu$ by integer steps, and so

$\quad\begin{array}{l}\displaystyle \mu''-\mu'=\text{integer}=n\geq0.\end{array}$

At $\mu'$,

$\quad\begin{array}{l}\displaystyle \lambda=\mu'^2-\mu',\end{array}$

and at $\mu''$,

$\quad\begin{array}{l}\displaystyle \lambda=\mu''^2+\mu''.\end{array}$

and the value of $\lambda$ is the same, so

$\quad\begin{array}{l}\displaystyle \begin{aligned} 0 &=(\mu''-\mu')(\mu''+\mu')+(\mu''+\mu')\\ &=(\mu''+\mu')(\mu''-\mu'+1)\\ &=(\mu''+\mu')(n+1), \end{aligned}\end{array}$

so $\mu'=-\mu''$, and so $2\mu''=n$.

Let

$\quad\begin{array}{l}\displaystyle \mu''=l, \qquad \mu'=-l.\end{array}$

$2l$ is an integer and

$\quad\begin{array}{l}\displaystyle \lambda=\mu'^2-\mu'=l(l+1).\end{array}$

Because of the integer steps in $\mu$, $2\mu$ is an integer; now
replace $\mu$ by $m$.

To find the eigenfunction,

$\quad\begin{array}{l}\displaystyle L_3=-i\partial_\phi,\end{array}$

$\quad\begin{array}{l}\displaystyle L_3\psi_{l,m}=m\psi_{l,m},\end{array}$

$\quad\begin{array}{l}\displaystyle -i\frac{\partial\psi_{l,m}}{\partial\phi}=m\psi_{l,m},\end{array}$

so

$\quad\begin{array}{l}\displaystyle \psi_{l,m}=e^{im\phi}f(\theta).\end{array}$

If $\psi$ is to be a single valued function of $\phi$, then the
half-integer values must be excluded.

Now $L_+\psi_{l,l}=0$, so

$\quad\begin{array}{l}\displaystyle \begin{aligned} 0 &=L_+\psi_{l,l}\\ &=e^{i\phi}\left( i\cot\theta\frac\partial{\partial\phi} +\frac\partial{\partial\theta} \right)e^{il\phi}f(\theta)\\ &=e^{i(l+1)\phi}\left[-l\cot\theta f+f'\right]. \end{aligned}\end{array}$

Hence

$\quad\begin{array}{l}\displaystyle f=K\sin^l\theta.\end{array}$

The norm is

$\quad\begin{array}{l}\displaystyle \begin{aligned} \|\psi_{l,l}\|^2 &=\int_0^\pi\sin\theta\,d\theta \int_0^{2\pi}d\phi\, \psi_{l,l}^*\psi_{l,l}\\ &=2\pi|K|^2\int_0^\pi\sin^{2l+1}\theta\,d\theta\\ &=2\pi|K|^2\frac{2^{2l+1}l!\,l!}{(2l+1)!}. \end{aligned}\end{array}$

$\quad\begin{array}{l}\displaystyle \psi_{l,l}\mathrel{\boldsymbol{=}} \frac1{2^{l+1}l!} \left[\frac{(2l+1)!}{\pi}\right]^{1/2} e^{il\phi}\sin^l\theta.\end{array}$

In general,

$\quad\begin{array}{l}\displaystyle \psi_{l,m-1} \mathrel{\boldsymbol{=}}\frac1{\sqrt{l(l+1)-m(m-1)}}L_-\psi_{l,m}.\end{array}$

or

$\quad\begin{array}{l}\displaystyle \psi_{l,m-1}\mathrel{\boldsymbol{=}}\ell_-\psi_{l,m},\end{array}$

where

$\quad\begin{array}{l}\displaystyle \ell_-= \left[l(l+1)-m(m-1)\right]^{-1/2}L_-.\end{array}$

For example,

$\quad\begin{array}{l}\displaystyle \psi_{l,l-1}\mathrel{\boldsymbol{=}} -\frac1{2^l(l-1)!} \left[\frac{(2l+1)!}{2\pi l}\right]^{1/2} e^{i(l-1)\phi}\cos\theta\sin^{l-1}\theta.\end{array}$

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
