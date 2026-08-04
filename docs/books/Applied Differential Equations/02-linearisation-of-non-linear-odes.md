<a id="linearisation-of-non-linear-differential-equations"></a>
## Linearisation of non-linear differential equations

Confine our attention to $2^\mathrm{nd}$ order ODEs

Consider

$$
y''+3yy'+y^3=0
$$

under appropriate constraints on domain, linear equations
have two linearly independent solutions

$$
Ly=0 \qquad L\text{ }2^\mathrm{nd}\text{ order linear operator}
$$

$$
\{y=y_1(x),y_2(x)\}
$$

$$
y''+xy=0
$$

$$
\begin{aligned}
y_1(0)&=1, & y_2(0)&=0,\\
y_1'(0)&=0, & y_2'(0)&=1
\end{aligned}
$$

$$
y_1(x) \qquad y_2(x)
$$

$$
y(x)=a_1y_1(x)+a_2y_2(x)
$$

Two equations are equivalent if they possess the same Lie algebra
of symmetries i.e. there exists a point transformation which
transforms one equation to the other.

<a id="point-transformation"></a>
### Point transformation

$$
x,y\longmapsto X,Y,
\qquad X=F(x,y);\quad Y=G(x,y)
$$

other transforms depend upon the derivatives e.g. contact

$$
x,y\longmapsto X,Y,
\qquad X=F(x,y,y');\quad Y=G(x,y,y')
$$

<a id="lie-algebra"></a>
### Lie algebra

$\{G_i,\ i=1,\ldots,n\}$ forms a Lie algebra if

$$
[G_i,G_j]=C_{ijk}G_k
$$

where the Cᵢⱼₖ are constants (called structure constants)
and $[\ ,\ ]$ is a skew symmetric operator

$$
[G_i,G_j]=G_iG_j-G_jG_i
$$

so that it is obvious that

$$
[G_i,G_j]=-[G_j,G_i]
$$

Note: the structure constants are invariant under a point
transformation. i.e. the algebra is independent of the co-ordinate
representation being used.


<a id="infinitesimal-point-transformations"></a>
In general a symmetry exists if a "system" is invariant under the
transformation induced by the symmetry. An infinitessimal point transformation of
$x,y$ to $\bar{x},\bar{y}$ is represented by

$$
\bar x=x+\varepsilon\xi(x,y),
\qquad
\bar y=y+\varepsilon\eta(x,y),
$$

where $\varepsilon$ is the infinitesimal parameter. For $f(x,y)$ an
infinitesimal point transformation produces

$$
\begin{aligned}
f(\bar x,\bar y)
&=f\bigl(x+\varepsilon\xi,y+\varepsilon\eta\bigr)\\
&=f(x,y)+\varepsilon
\left(\xi\frac{\partial f}{\partial x}
+\eta\frac{\partial f}{\partial y}\right)+O(\varepsilon^2)\\
&=f(x,y)+\varepsilon Gf.
\end{aligned}
$$

where

$$
G=\xi\frac{\partial}{\partial x}
+\eta\frac{\partial}{\partial y}
$$

is the generator of the infinitesimal point transformation.

$$
\left\{
G_i=\xi_i\frac{\partial}{\partial x}
+\eta_i\frac{\partial}{\partial y},
\quad i=1,2,\ldots,n
\right\}
$$

constitutes a Lie algebra if

$$
\begin{aligned}
[G_i,G_j]
&=\left[
\xi_i\frac{\partial}{\partial x}
+\eta_i\frac{\partial}{\partial y},
\xi_j\frac{\partial}{\partial x}
+\eta_j\frac{\partial}{\partial y}
\right]\\
&=\left(
\xi_i\frac{\partial\xi_j}{\partial x}
+\eta_i\frac{\partial\xi_j}{\partial y}
-\xi_j\frac{\partial\xi_i}{\partial x}
-\eta_j\frac{\partial\xi_i}{\partial y}
\right)\frac{\partial}{\partial x}\\
&\quad+\left(
\xi_i\frac{\partial\eta_j}{\partial x}
+\eta_i\frac{\partial\eta_j}{\partial y}
-\xi_j\frac{\partial\eta_i}{\partial x}
-\eta_j\frac{\partial\eta_i}{\partial y}
\right)\frac{\partial}{\partial y}\\
&=C_{ijk}\left(
\xi_k\frac{\partial}{\partial x}
+\eta_k\frac{\partial}{\partial y}
\right).
\end{aligned}
$$

<a id="how-to-deal-with-derivatives"></a>
### How to deal with $f(x,y,y')$ and $f(x,y,y',y'')$

How do $y',y'',\ldots$ transform?

$$
\begin{aligned}
\frac{d\bar y}{d\bar x}
&=\frac{d(y+\varepsilon\eta)}{d(x+\varepsilon\xi)}\\
&=\frac{\dfrac{dy}{dx}+\varepsilon\dfrac{d\eta}{dx}}
{1+\varepsilon\dfrac{d\xi}{dx}}\\
&=\frac{y'+\varepsilon\eta'}{1+\varepsilon\xi'}\\
&=y'+\varepsilon(\eta'-y'\xi')
\qquad\text{to }1^\mathrm{st}\text{ order in }\varepsilon.\\[1.5ex]
\frac{d^2\bar y}{d\bar x^2}
&=\frac{d}{d\bar x}\left(\frac{d\bar y}{d\bar x}\right)\\
&=\frac{d\left[y'+\varepsilon(\eta'-y'\xi')\right]}
{d(x+\varepsilon\xi)}\\
&=\frac{y''+\varepsilon(\eta''-y''\xi'-y'\xi'')}
{1+\varepsilon\xi'}\\
&=y''+\varepsilon(\eta''-2y''\xi'-y'\xi'').
\end{aligned}
$$


<a id="derivative-notes"></a>
#### Notes

<a id="prolongation"></a>
1. The process may be extended to higher order derivatives. There
   is a recursion relation. If

   $$
   G=\xi\frac{\partial}{\partial x}
   +\eta\frac{\partial}{\partial y},
   $$

   then

   $$
   G^{(1)}=G+(\eta'-y'\xi')\frac{\partial}{\partial y'},
   $$

   $$
   G^{(2)}=G^{(1)}+(\eta''-2y''\xi'-y'\xi'')
   \frac{\partial}{\partial y''}.
   $$

   Let

   $$
   H^{(1)}=\eta'-y'\xi'.
   $$

   Then

   $$
   G^{(1)}=G+H^{(1)}\frac{\partial}{\partial y'},
   $$

   $$
   G^{(n)}=G^{(n-1)}+H^{(n)}\frac{\partial}{\partial y^{(n)}}.
   $$

   $$
   \begin{aligned}
   H^{(2)}&=(H^{(1)})'-y''\xi',\\
   H^{(3)}&=(H^{(2)})'-y'''\xi'.
   \end{aligned}
   $$

   $$
   \vdots
   $$

2. The process may be extended to systems of equations
   with independent variables and dependent variables.

<a id="symmetries-of-a-second-order-ode"></a>
### Symmetries of a $2^\mathrm{nd}$ order ODE

$$
N(x,y,y',y'')=0,
$$

$$
G=\xi\frac{\partial}{\partial x}
+\eta\frac{\partial}{\partial y},
$$

with

$$
\xi=\xi(x,y),
\qquad
\eta=\eta(x,y)
$$

(i.e. point transformations), is the generator of a symmetry (i) if

$$
G^{(2)}N=0
\qquad\text{when }N=0,
$$

where

$$
G^{(2)}
=\xi\frac{\partial}{\partial x}
+\eta\frac{\partial}{\partial y}
+(\eta'-y'\xi')\frac{\partial}{\partial y'}
+(\eta''-2y''\xi'-y'\xi'')\frac{\partial}{\partial y''}.
$$

Example: $y''=0$

$$
G^{(2)}y''=\eta''-2y''\xi'-y'\xi''
$$

$\displaystyle G^{(2)}y''=0:$

$$
\frac{\partial^2\eta}{\partial x^2}
+2y'\frac{\partial^2\eta}{\partial x\partial y}
+(y')^2\frac{\partial^2\eta}{\partial y^2}
+y''\frac{\partial\eta}{\partial y}
-y'\left(
\frac{\partial^2\xi}{\partial x^2}
+2y'\frac{\partial^2\xi}{\partial x\partial y}
+(y')^2\frac{\partial^2\xi}{\partial y^2}
+y''\frac{\partial\xi}{\partial y}
\right)=0.
$$

Since $\xi(x,y)$ and $\eta(x,y)$ are independent of $y'$, equate
coefficients of independent powers of $y'$ to zero.


$$
\begin{aligned}
(y')^3 &: -\frac{\partial^2\xi}{\partial y^2}=0
&&\text{(ii)}\\[1ex]
(y')^2 &: \frac{\partial^2\eta}{\partial y^2}
-2\frac{\partial^2\xi}{\partial x\partial y}=0
&&\text{(iii)}\\[1ex]
y' &: 2\frac{\partial^2\eta}{\partial x\partial y}
-\frac{\partial^2\xi}{\partial x^2}=0
&&\text{(iv)}\\[1ex]
(y')^0 &: \frac{\partial^2\eta}{\partial x^2}=0
&&\text{(v)}
\end{aligned}
$$

$$
\text{(ii)}\qquad \xi=a(x)y+b(x).
$$

$$
\text{(iii)}\qquad
\frac{\partial^2\eta}{\partial y^2}=2a',
$$

$$
\eta=a'y^2+c(x)y+d(x).
$$

$$
\text{(iv)}\qquad
4a''y+2c'-a''y-b''=0. \tag{vi}
$$

$$
\text{(v)}\qquad
a'''y^2+c''y+d''=0. \tag{vii}
$$

In (vi) and (vii) equate coefficients of independent powers of $y$ to zero:

$$
\begin{aligned}
y^2 &: a'''=0,\\
y^1 &: c''=0,\\
    &\phantom{:}\ a''=0,\\
y^0 &: d''=0,\\
    &\phantom{:}\ b''=2c'.
\end{aligned}
$$

$$
\begin{aligned}
a&=A_0+A_1x,\\
b&=B_0+B_1x+C_1x^2,\\
c&=C_0+C_1x,\\
d&=D_0+D_1x.
\end{aligned}
$$

$$
\begin{aligned}
\xi&=(A_0+A_1x)y+B_0+B_1x+C_1x^2,\\
\eta&=A_1y^2+(C_0+C_1x)y+D_0+D_1x.
\end{aligned}
$$

Observations:

1. There are 8 generators which leave $y''=0$ invariant.

2. To obtain all of the symmetries it is necessary to be
   able to solve the original differential equation.


$G^{(2)}N=0$ is always linear in $\xi$ and $\eta$.

For linear equations in general:

1. There are 8 symmetries.

2. The symmetries close under commutation and have the Lie algebra
   $\operatorname{sl}(3,R)$.

3. Every linear equation may be transformed into every other linear equation
   by means of a point transformation.

   [Algebras are invariant under point transformations]

4. The transformation is found by comparing the generators of the two
   equations. More specifically one looks for a subset.

   For the free particle

<div data-aligned-equations="generator-definitions" style="display: flex; justify-content: flex-start;">

$$
\begin{aligned}
G_1&=\frac{\partial}{\partial x} &&: B_0,\\[6pt]
G_2&=x\frac{\partial}{\partial x} &&: B_1,\\[6pt]
G_3&=x^2\frac{\partial}{\partial x}
     +xy\frac{\partial}{\partial y} &&: C_1,\\[6pt]
G_4&=y\frac{\partial}{\partial x} &&: A_0,\\[6pt]
G_5&=xy\frac{\partial}{\partial x}
     +y^2\frac{\partial}{\partial y} &&: A_1,\\[6pt]
G_6&=\frac{\partial}{\partial y} &&: D_0,\\[6pt]
G_7&=x\frac{\partial}{\partial y} &&: D_1,\\[6pt]
G_8&=y\frac{\partial}{\partial y} &&: C_0.
\end{aligned}
$$

</div>

<a id="free-particle-commutators"></a>

| $[G_i,G_j]$ | $G_1$ | $G_2$ | $G_3$ | $G_4$ | $G_5$ | $G_6$ | $G_7$ | $G_8$ |
|:--|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| $G_1$ | $0$ | $G_1$ | $2G_2+G_8$ | $0$ | $G_4$ | $0$ | $G_6$ | $0$ |
| $G_2$ |  | $0$ | $G_3$ | $-G_4$ | $0$ | $0$ | $G_7$ | $0$ |
| $G_3$ |  |  | $0$ | $-G_5$ | $0$ | $-G_6$ | $-G_3$ | $0$ |
| $G_4$ |  |  |  | $0$ | $0$ | $-G_1$ | $-G_2$ | $-G_4$ |
| $G_5$ |  |  |  |  | $0$ | $-G_2-2G_8$ | $-G_3$ | $-G_5$ |
| $G_6$ |  |  |  |  |  | $0$ | $0$ | $G_6$ |
| $G_7$ |  |  |  |  |  |  | $0$ | $G_7$ |
| $G_8$ |  |  |  |  |  |  |  | $0$ |

$$
\begin{aligned}
[G_1,G_2]
&=\left[
    \frac{\partial}{\partial x},
    x\frac{\partial}{\partial x}
  \right]\\
&=\frac{\partial}{\partial x}
  \left(x\frac{\partial}{\partial x}\right)
  -x\frac{\partial}{\partial x}
  \left(\frac{\partial}{\partial x}\right)\\
&=\frac{\partial}{\partial x}
  +x\frac{\partial^2}{\partial x^2}
  -x\frac{\partial^2}{\partial x^2}\\
&=G_1.
\end{aligned}
$$


<a id="symmetries-and-first-integrals"></a>
### Relationship between symmetries and first integrals

If $I=I(x,y,y')$ has the property that

$$
\left.\frac{dI}{dx}\right|_{N(x,y,y',y'')=0}=0,
$$

then $I(x,y,y')$ is a first integral of

$$
N(x,y,y',y'')=0.
$$

Suppose $G$ is a symmetry of $N(x,y,y',y'')=0$. Then the first integral
associated with $G$ is $I(x,y,y')$:

$$
G^{(1)}I(x,y,y')=0
\quad\Longrightarrow\quad
dI\bigl(G^{(1)}\bigr)=0
\quad\text{for surface }N=0
$$

$$
\Longrightarrow
\left.\frac{dI}{dx}\right|_{N=0}=0.
$$

For $y''=0$, consider

$$
G_7=x\frac{\partial}{\partial y}.
$$

$$
\begin{aligned}
G&=\xi\frac{\partial}{\partial x}
  +\eta\frac{\partial}{\partial y},\\
G^{(1)}&=\xi\frac{\partial}{\partial x}
  +\eta\frac{\partial}{\partial y}
  +(\eta'-y'\xi')\frac{\partial}{\partial y'}.
\end{aligned}
$$

$$
G_7^{(1)}=x\frac{\partial}{\partial y}
          +\frac{\partial}{\partial y'}.
$$

$$
G_7^{(1)}I
=0\frac{\partial I}{\partial x}
 +x\frac{\partial I}{\partial y}
 +\frac{\partial I}{\partial y'}
=0.
$$

<a id="characteristics"></a>
The equations for the characteristics are

$$
\frac{dx}{0}=\frac{dy}{x}=\frac{dy'}{1}.
$$

$1^\mathrm{st}$ and $2^\mathrm{nd}$:

$$
\begin{aligned}
x\,dx-0\,dy&=0,\\
\frac{1}{2}x^2&=\text{constant},\\
u&=x
\qquad
\left(\text{allowed to take any function of }\frac{1}{2}x^2\right).
\end{aligned}
$$

$2^\mathrm{nd}$ and $3^\mathrm{rd}$:

$$
\begin{aligned}
dy-x\,dy'&=0,\\
dy-u\,dy'&=0,\\
v&=y-uy'=y-xy'.
\end{aligned}
$$

i.e.

$$
I(u,v)=I(x,y-xy').
$$

Imposing the second condition, i.e.

$$
\left.\frac{dI}{dx}\right|_{N=0}=0,
$$

$$
\frac{\partial I}{\partial u}u'
+\frac{\partial I}{\partial v}v'=0,
$$

for which the characteristic is found from

$$
\frac{du}{u'}=\frac{dv}{v'}.
$$


$$
\begin{aligned}
\frac{dv}{du}
&=\frac{v'}{u'}\\
&=\frac{y'-xy''-y'}{1}=0,
\qquad v=\text{constant}.
\end{aligned}
$$

$$
I=f(y-xy').
$$

usually write

$$
I=y-xy'.
$$

$$
G_3=x^2\frac{\partial}{\partial x}
    +xy\frac{\partial}{\partial y}.
$$

$$
G_3^{(1)}I
=x^2\frac{\partial I}{\partial x}
 +xy\frac{\partial I}{\partial y}
 +(y-xy')\frac{\partial I}{\partial y'}
=0.
$$

Equations for the characteristics are

$$
\frac{dx}{x^2}
=\frac{dy}{xy}
=\frac{dy'}{y-xy'}.
$$

$1^\mathrm{st}$ and $2^\mathrm{nd}$:

$$
\frac{dx}{x^2}=\frac{dy}{xy}
\quad\Longrightarrow\quad
\frac{dy}{y}-\frac{dx}{x}=0.
$$

$$
\left(\log y-\log x=\log u\right),
$$

$$
u=\frac{y}{x}.
$$

$2^\mathrm{nd}$ and $3^\mathrm{rd}$:

$$
\frac{dy}{xy}=\frac{dy'}{y-xy'}.
$$

$$
x=\frac{y}{u}.
$$

$$
\frac{dy}{y^2/u}
=\frac{dy'}{y-yy'/u}.
$$

$$
\frac{dy'}{dy}
=\frac{y-\dfrac{yy'}{u}}{y^2/u}
=\frac{u-y'}{y}.
$$

$$
\frac{dy}{y}=\frac{dy'}{u-y'}.
$$

$$
\begin{aligned}
v&=yy'-uy\\
 &=yy'-\frac{y^2}{x}.
\end{aligned}
$$

$$
I(u,v)=I\left(\frac{y}{x},yy'-\frac{y^2}{x}\right).
$$

from

$$
\frac{\partial I}{\partial u}u'
+\frac{\partial I}{\partial v}v'=0,
$$

$$
\frac{dv}{du}
=\frac{v'}{u'}
=\frac{
    (y')^2+yy''-\dfrac{2y'y}{x}+\dfrac{y^2}{x^2}
  }{
    \dfrac{y'}{x}-\dfrac{y}{x^2}
  }.
$$


$$
\begin{aligned}
&=\frac{
    \left(y'-\dfrac{y}{x}\right)^2
  }{
    \dfrac{1}{x}\left(y'-\dfrac{y}{x}\right)
  }\\
&=x\left(y'-\frac{y}{x}\right)\\
&=x\frac{v}{y}\\
&=\frac{v}{u}.
\end{aligned}
$$

$$
\frac{dv}{du}=\frac{v}{u}
\quad\Longrightarrow\quad
w=\frac{v}{u}.
$$

$$
\begin{aligned}
I
&=f\left(\frac{v}{u}\right)\\
&=f\left(
    \frac{yy'-y^2/x}{y/x}
  \right)\\
&=f(xy'-y).
\end{aligned}
$$

$$
G_1=\frac{\partial}{\partial x}.
$$

$$
G_1^{(1)}I
=\frac{\partial I}{\partial x}
 +0\frac{\partial I}{\partial y}
 +0\frac{\partial I}{\partial y'}
=0.
$$

$$
\frac{dx}{1}=\frac{dy}{0}=\frac{dy'}{0}.
$$

$1$ and $2$:

$$
dy-0\,dx=0,
\qquad
u=y.
$$

$1$ and $3$:

$$
dy'-0\,dx=0,
\qquad
v=y'.
$$

$$
\frac{dv}{du}
=\frac{v'}{u'}
=\frac{y''}{y'}
=0.
$$

$$
w=v.
$$

$$
I=f(v)=f(y').
$$

other form is

$$
\begin{aligned}
I
&=f\left(\frac{xy'-y}{y'}\right)\\
&=f\left(x-\frac{y}{y'}\right).
\end{aligned}
$$

$$
I=x-\frac{y}{y'}
$$

is called a resonance integral.

Characteristic structure of a resonance integral is

$$
I=a(x)+\frac{b(xy)}{y'-c(x,y)}.
$$


Consider

$$
y''+3yy'+y^3=0.
$$

(Comes from the study of the modified Emden equation

$$
\ddot q+\alpha(t)\dot q+q^n=0,
$$

which arises in the study of polytropes, i.e. gaseous spheres (models of
stars)).

Let

$$
G=\xi(x,y)\frac{\partial}{\partial x}
 +\eta(x,y)\frac{\partial}{\partial y}.
$$

Then

$$
G^{(2)}\left(y''+3yy'+y^3=0\right)
$$

is

$$
(\eta''-2y''\xi'-y'\xi'')
+3\eta y'
+3y(\eta'-y'\xi')
+3y^2\eta=0.
$$

$$
\begin{aligned}
&\frac{\partial^2\eta}{\partial x^2}
+2y'\frac{\partial^2\eta}{\partial x\partial y}
+(y')^2\frac{\partial^2\eta}{\partial y^2}
-(3yy'+y^3)\frac{\partial\eta}{\partial y}\\
&\quad
+2(3yy'+y^3)
 \left(
   \frac{\partial\xi}{\partial x}
   +y'\frac{\partial\xi}{\partial y}
 \right)\\
&\quad
-y'\left\{
  \frac{\partial^2\xi}{\partial x^2}
  +2y'\frac{\partial^2\xi}{\partial x\partial y}
  +(y')^2\frac{\partial^2\xi}{\partial y^2}
  -(3yy'+y^3)\frac{\partial\xi}{\partial y}
\right\}\\
&\quad
+3y\left\{
  \frac{\partial\eta}{\partial x}
  +y'\frac{\partial\eta}{\partial y}
  -y'\frac{\partial\xi}{\partial x}
  -(y')^2\frac{\partial\xi}{\partial y}
\right\}
+3y^2\eta+3\eta y'=0.
\end{aligned}
$$

Equate coefficients of independent powers of $y'$ to zero:

$$
\begin{array}{rcll}
(y')^3
&:&
-\dfrac{\partial^2\xi}{\partial y^2}=0
&\qquad\text{(i)}
\\[10pt]
(y')^2
&:&
\dfrac{\partial^2\eta}{\partial y^2}
+6y\dfrac{\partial\xi}{\partial y}
-2\dfrac{\partial^2\xi}{\partial x\partial y}
+\cancel{3y\dfrac{\partial\xi}{\partial y}}
-\cancel{3y\dfrac{\partial\xi}{\partial y}}=0
&\qquad\text{(ii)}
\\[10pt]
y'
&:&
2\dfrac{\partial^2\eta}{\partial x\partial y}
-\cancel{3y\dfrac{\partial\eta}{\partial y}}
+\cancel{6y\dfrac{\partial\xi}{\partial x}}
+3y^3\dfrac{\partial\xi}{\partial y}
-\dfrac{\partial^2\xi}{\partial x^2}
+\cancel{y^3\dfrac{\partial\xi}{\partial y}}
+\cancel{3y\dfrac{\partial\eta}{\partial y}}
+3y\dfrac{\partial\xi}{\partial x}
+3\eta=0
&\qquad\text{(iii)}
\\[10pt]
(y')^0
&:&
\dfrac{\partial^2\eta}{\partial x^2}
-y^3\dfrac{\partial\eta}{\partial y}
+2y^3\dfrac{\partial\xi}{\partial x}
+3y\dfrac{\partial\eta}{\partial x}
+3y^2\eta=0
&\qquad\text{(iv)}
\end{array}
$$

(i)

$$
\xi=a(x)y+b(x).
$$

(ii)

$$
\frac{\partial^2\eta}{\partial y^2}=-6ay+2a',
$$

$$
\eta=-ay^3+a'y^2+c(x)y+d(x).
$$

(iii)

$$
\begin{aligned}
&2(-3a'y^2+2a''y+c')
+3y^3a-a''y-b''
+3a'y^2+3b'y-3ay^3\\
&\qquad
+3a'y^2+3cy+3d=0.
\end{aligned}
$$

$$
\begin{aligned}
y^1:&\qquad 3a''+3b'+3c=0,\\
y^0:&\qquad 2c'-b''+3d=0.
\end{aligned}
$$

(iv)

$$
\begin{aligned}
&-a''y^3+a'''y^2+c''y+d''
-y^3(-3ay^2+2a'y+c)
+2y^3(a'y+b')\\
&\qquad
+3y(-a'y^3+a''y^2+c'y+d')
+3y^2(-ay^3+a'y^2+cy+d)=0.
\end{aligned}
$$

$$
\begin{aligned}
y^3:&\qquad -a''-c+2b'+3a''+3c=0,\\
y^2:&\qquad a'''+3c'+3d=0,\\
y^1:&\qquad c''+3d'=0,\\
y^0:&\qquad d''=0.
\end{aligned}
$$


$$
\begin{aligned}
a''+b'+c&=0 &&\text{(1)}\\
b''-2c'-3d&=0 &&\text{(2)}\\
a'''+3c'+3d&=0 &&\text{(3)}\\
c''+3d'&=0 &&\text{(4)}\\
d''&=0 &&\text{(5)}
\end{aligned}
$$

(5)

$$
d=D_0+D_1x.
$$

(4)

$$
c=C_0+C_1x-\frac{3}{2}D_1x^2.
$$

(3)

$$
a=A_0+A_1x+A_2x^2
-\frac{1}{2}(C_1+D_0)x^3
+\frac{1}{4}D_1x^4.
$$

(2)

$$
b=B_0+B_1x
+\frac{1}{2}(2C_1+3D_0)x^2
-\frac{1}{2}D_1x^3.
$$

check in (1)

$$
\begin{aligned}
&2A_2-3(C_1+D_0)x+3D_1x^2+B_1
 +(2C_1+3D_0)x-\frac{3}{2}D_1x^2\\
&\qquad
 +C_0+C_1x-\frac{3}{2}D_1x^2=0.
\end{aligned}
$$

$$
2A_2+B_1+C_0=0;
\qquad
-3C_1-3D_0+2C_1+3D_0+C_1=0.
$$

$$
3D_1-\frac{3}{2}D_1-\frac{3}{2}D_1=0.
$$

i.e. there are 8 independent constants and there are 8 symmetries.

$$
\begin{aligned}
G
 &= (ay+b)\frac{\partial}{\partial x}
    +(-ay^3+a'y^2+cy+d)\frac{\partial}{\partial y},\\[6pt]
G_1
 &= \frac{1}{2}x^2y\frac{\partial}{\partial x}
    +\left(-\frac{1}{2}x^2y^3+xy^2-y\right)
     \frac{\partial}{\partial y},\\[6pt]
G_2
 &= y\frac{\partial}{\partial x}
    -y^3\frac{\partial}{\partial y},\\[6pt]
G_3
 &= xy\frac{\partial}{\partial x}
    +(-xy^3+y^2)\frac{\partial}{\partial y},\\[6pt]
G_4
 &= \left(-\frac{1}{2}x^2y+x\right)
    \frac{\partial}{\partial x}
    +\left(\frac{1}{2}x^2y^3-xy^2\right)
    \frac{\partial}{\partial y},\\[6pt]
G_5
 &= \left(-\frac{1}{4}x^4y+\frac{1}{3}x^3\right)
    \frac{\partial}{\partial x}
    +\left(\frac{1}{4}x^4y^3-x^3y^2
    +\frac{3}{2}x^2y-x\right)
    \frac{\partial}{\partial y},\\[6pt]
G_6
 &= \left(-\frac{1}{2}x^3y+x^2\right)
    \frac{\partial}{\partial x}
    +\left(\frac{1}{2}x^3y^3-\frac{3}{2}x^2y^2+xy\right)
    \frac{\partial}{\partial y},\\[6pt]
G_7
 &= \left(-\frac{1}{2}x^3y+x\right)
    \frac{\partial}{\partial x}
    +\left(\frac{1}{2}x^3y^3-\frac{3}{2}x^2y^2+1\right)
    \frac{\partial}{\partial y},\\[6pt]
G_8
 &= -\frac{\partial}{\partial x}.
\end{aligned}
$$


$$
y''=0.
$$

$$
I_1=y',
$$

$$
I_2=y'x-y,
$$

$$
I_3=x-\frac{y}{y'}.
$$

What generators are associated with $I_1$?

$$
G^{(1)}I_1=0.
$$

$$
\left(
 \xi\frac{\partial}{\partial x}
 +\eta\frac{\partial}{\partial y}
 +(\eta'-y'\xi')\frac{\partial}{\partial y'}
\right)y'=0.
$$

$$
\eta'-y'\xi'=0.
$$

$$
\frac{\partial\eta}{\partial x}
+y'\frac{\partial\eta}{\partial y}
-y'\frac{\partial\xi}{\partial x}
-(y')^2\frac{\partial\xi}{\partial y}=0.
$$

$\displaystyle\begin{aligned}(y')^2 &:\qquad \frac{\partial\xi}{\partial y}=0 &&\text{(i)}\\[6pt](y')^1 &:\qquad \frac{\partial\eta}{\partial y}-\frac{\partial\xi}{\partial x}=0 &&\text{(ii)}\\[6pt](y')^0 &:\qquad \frac{\partial\eta}{\partial x}=0 &&\text{(iii)}\end{aligned}$

- **(i)** $\xi=a(x)$.

- **(ii)** $\eta=a'y+b(x)$.

- **(iii)** $a''=0$, $b'=0$.

$$
a=A_0+A_1x,
\qquad
b=B_0.
$$

The generators

$$
\begin{aligned}
G_1
 &= \frac{\partial}{\partial x},
 &\qquad& A_0,\\[4pt]
G_2
 &= \frac{\partial}{\partial y},
 && B_0,\\[4pt]
G_3
 &= x\frac{\partial}{\partial x}
    +y\frac{\partial}{\partial y},
 && A_1.
\end{aligned}
$$

$$
[G_1,G_2]=0,
$$

$$
[G_1,G_3]=G_1,
$$

$$
[G_2,G_3]=G_2.
$$


<a id="classical-groups"></a>
We wish to identify the complete symmetry group for the non-linear system
(2.22). The generators of (2.22) given in the previous section should have
commutation relations appropriate to one of the eight-parameter groups
$SL(3,R)$, $SU(3)$ or $GL(2,C)$. The latter two being complex.

<a id="commutation-table"></a>
The commutation relations are given in Table 2. These relations are appropriate
to the symmetry group $SL(3,R)$. Therefore the complete symmetry group for
(2.22) is $SL(3,R)$. This will become apparent later.

**Table 2**

The entry in row $i$ and column $j$ is the Lie bracket $[G_i,G_j]$.

$$
\begin{array}{c|cccccccc}
 &G_1&G_2&G_3&G_4&G_5&G_6&G_7&G_8\\ \hline
G_1
 &0&-G_2&-G_3&0&G_5&0&G_7-2G_6&G_3\\
G_2
 &&0&0&G_2&G_1+G_4&G_3&3G_3+G_8&0\\
G_3
 &&&0&0&G_6&0&2G_1-G_4&G_2\\
G_4
 &&&&0&G_5&G_6&2G_6&-(G_3+G_8)\\
G_5
 &&&&&0&0&0&3G_6-G_7\\
G_6
 &&&&&&0&G_5&2G_4-G_1\\
G_7
 &&&&&&&0&3G_4\\
G_8
 &&&&&&&&0
\end{array}
$$

The entries below the main diagonal have been omitted because of the skewness
of the Lie bracket.


i.e. $G_1$, $G_2$, $G_3$ form a closed subgroup. For $I_2$ and $I_3$ the same
result is found. (This gives 9 in all, but one is a linear combination of the
others.) Can use this subalgebra to find a linearising transformation.

From the commutation table for the generators of

$$
y''+3yy'+y^3=0
$$

we see that

$$
\begin{aligned}
[G_1,G_2]&=-G_2,
&\qquad
[G_1,G_3]&=-G_3,\\
[G_2,G_3]&=0.
\end{aligned}
$$

Define

$$
\begin{aligned}
[\widetilde G_1,\widetilde G_2]
 &=[G_2,G_3]=0,\\
[\widetilde G_1,\widetilde G_3]
 &=[G_2,G_1]=G_2=\widetilde G_1,\\
[\widetilde G_2,\widetilde G_3]
 &=[G_3,G_1]=G_3=\widetilde G_2.
\end{aligned}
$$

i.e. we have the correct subalgebra.

In $x,y$ coordinates we have

$$
y''+3yy'+y^3=0,
$$

$$
\begin{aligned}
\widetilde G_1
 &=y\frac{\partial}{\partial x}
   -y^3\frac{\partial}{\partial y},\\[6pt]
\widetilde G_2
 &=xy\frac{\partial}{\partial x}
   -(xy^3-y^2)\frac{\partial}{\partial y},\\[6pt]
\widetilde G_3
 &=\frac{1}{2}x^2y\frac{\partial}{\partial x}
   -\left(\frac{1}{2}x^2y^3-xy^2+y\right)
    \frac{\partial}{\partial y}.
\end{aligned}
$$

In $X,Y$ coordinates we have

$$
Y''=0,
$$

$$
\begin{aligned}
G_1
 &=\frac{\partial}{\partial X},\\[4pt]
G_2
 &=\frac{\partial}{\partial Y},\\[4pt]
G_3
 &=X\frac{\partial}{\partial X}
   +Y\frac{\partial}{\partial Y}.
\end{aligned}
$$

$$
X=F(x,y),
\qquad
Y=G(x,y).
$$

$$
\frac{\partial}{\partial x}
=\frac{\partial F}{\partial x}\frac{\partial}{\partial X}
 +\frac{\partial G}{\partial x}\frac{\partial}{\partial Y},
$$

$$
\frac{\partial}{\partial y}
=\frac{\partial F}{\partial y}\frac{\partial}{\partial X}
 +\frac{\partial G}{\partial y}\frac{\partial}{\partial Y}.
$$


**1.**

$$
\left(y\frac{\partial F}{\partial x}
      -y^3\frac{\partial F}{\partial y}\right)
      \frac{\partial}{\partial X}
+\left(y\frac{\partial G}{\partial x}
      -y^3\frac{\partial G}{\partial y}\right)
      \frac{\partial}{\partial Y}
=\frac{\partial}{\partial X}.
$$

$$
y\frac{\partial G}{\partial x}
-y^3\frac{\partial G}{\partial y}=0.
$$

$$
\frac{dx}{y}=-\frac{dy}{y^3}.
$$

$$
0=dx+\frac{dy}{y^2};
\qquad
u=x-\frac{1}{y}.
$$

$$
G=G\left(x-\frac{1}{y}\right)
 =g\left(x-\frac{1}{y}\right).
$$

$$
y\frac{\partial F}{\partial x}
-y^3\frac{\partial F}{\partial y}=1.
$$

$$
\frac{dx}{y}=\frac{dy}{-y^3}=\frac{dF}{1}.
$$

$$
u=x-\frac{1}{y}.
$$

($2^{\mathrm{nd}}$ and $3^{\mathrm{rd}}$):

$$
v=F-\frac{1}{2y^2}.
$$

$$
F=\frac{1}{2y^2}+f\left(x-\frac{1}{y}\right).
$$

**2.**

$$
\left[
  xy\frac{\partial F}{\partial x}
  -(xy^3-y^2)\frac{\partial F}{\partial y}
\right]\frac{\partial}{\partial X}
+\left[
  xy\frac{\partial G}{\partial x}
  -(xy^3-y^2)\frac{\partial G}{\partial y}
\right]\frac{\partial}{\partial Y}
=\frac{\partial}{\partial Y}.
$$

$F$:

$$
xyf'-(xy^3-y^2)f'\left(\frac{1}{y^2}\right)=0.
$$

$$
xyf'-xyf'+f'=0.
$$

$$
f'=0,
$$

i.e. $f=\text{constant}$ (take as zero).

$$
xyg'-(xy^3-y^2)g'\left(\frac{1}{y^2}\right)=1.
$$

$$
g'=1.
$$

$$
g=x-\frac{1}{y},
$$

to within a forgettable constant.

$$
X=\frac{1}{2y^2},
\qquad
Y=x-\frac{1}{y}.
$$

$$
y^2=\frac{1}{2X}.
$$

$$
x=Y+(2X)^{1/2}.
$$

$$
y=\left(\frac{1}{2X}\right)^{1/2}.
$$


$$
y'
=\frac{-\dfrac{\sqrt{2}}{4X^{3/2}}}
       {Y'+\dfrac{\sqrt{2}}{2X^{1/2}}}.
$$

$$
y''
=\frac{1}
       {\left(Y'+\dfrac{\sqrt{2}}{2X^{1/2}}\right)^3}
 \left\{
   \frac{\sqrt{2}}{4X^{3/2}}Y''
   +\frac{3\sqrt{2}}{8X^{5/2}}Y'
   +\frac{1}{4X^3}
 \right\}.
$$

$$
y''+3yy'+y^3=0
$$

$$
\begin{aligned}
Y''
&+\frac{3}{2X}Y'
 +\frac{\sqrt{2}}{2X^{3/2}}\\
&-\frac{3\sqrt{2}}{2X^{1/2}}
 \left(Y'+\frac{\sqrt{2}}{2X^{1/2}}\right)^2
 +\left(Y'+\frac{\sqrt{2}}{2X^{1/2}}\right)^3=0.
\end{aligned}
$$

$$
\begin{aligned}
Y''+(Y')^3
&+(Y')^2(0)
 +Y'\left(\frac{3}{2X}-\frac{3}{X}+\frac{3}{2X}\right)\\
&+\left(
 \frac{\sqrt{2}}{2X^{3/2}}
 -\frac{6\sqrt{2}}{8X^{3/2}}
 +\frac{2\sqrt{2}}{8X^{3/2}}
 \right)=0.
\end{aligned}
$$

Therefore

$$
Y''+(Y')^3=0.
$$

This is not linear in the present ordering of the coordinates. The generators
are

$$
\begin{aligned}
G_1
 &=\frac{\partial}{\partial X},\\[4pt]
G_2
 &=\frac{\partial}{\partial Y},\\[4pt]
G_3
 &=X\frac{\partial}{\partial X}
   +Y\frac{\partial}{\partial Y}.
\end{aligned}
$$

Interchange the dependent and independent variables:

$$
\mathcal X=Y,
\qquad
\mathcal Y=X.
$$

Then

$$
\mathcal Y'=\frac{1}{Y'},
\qquad
\mathcal Y''=-\frac{Y''}{(Y')^3}.
$$

Hence

$$
-\frac{\mathcal Y''}{(\mathcal Y')^3}
+\frac{1}{(\mathcal Y')^3}=0,
$$

so

$$
\mathcal Y''=1,
$$

which is linear. To reduce it to the free-particle equation, put

$$
W=\mathcal Y-\frac{1}{2}\mathcal X^2.
$$

Then

$$
W''=0.
$$

<a id="linearising-transformation"></a>
Thus the corrected linearising transformation is

$$
\mathcal X=x-\frac{1}{y},
\qquad
W=\frac{1}{2y^2}
  -\frac{1}{2}\left(x-\frac{1}{y}\right)^2
  =\frac{x}{y}-\frac{x^2}{2}.
$$


$$
Y(X)=A+BX,
\qquad
X=A+BY.
$$

$$
\frac{1}{2y^2}=A+B\left(x-\frac{1}{y}\right),
$$

$$
(A+Bx)y^2-By-\frac{1}{2}=0,
$$

$$
y=\frac{B\mathbin{\pm}\sqrt{B^2+2(A+Bx)}}{2(A+Bx)}.
$$

$$
\begin{aligned}
\widetilde G_1
 &=y\frac{\partial}{\partial x}
   -y^3\frac{\partial}{\partial y},\\[4pt]
\widetilde G_2
 &=xy\frac{\partial}{\partial x}
   -(xy^3-y^2)\frac{\partial}{\partial y}.
\end{aligned}
$$

$$
X=F(x,y),
\qquad
Y=G(x,y).
$$

From

$$
\widetilde G_1\longrightarrow\frac{\partial}{\partial X},
$$

$$
F=\frac{1}{2y^2}+f\left(x-\frac{1}{y}\right),
\qquad
G=g\left(x-\frac{1}{y}\right).
$$

$$
\widetilde G_2=\frac{\partial}{\partial Y}.
$$

$$
xy\frac{\partial F}{\partial x}
 -(xy^3-y^2)\frac{\partial F}{\partial y}=0.
$$

$$
xyf'-(xy^3-y^2)
 \left(-\frac{1}{y^3}+\frac{1}{y^2}f'\right)=0.
$$

$$
(xy-xy+1)f'+x-\frac{1}{y}=0.
$$

$$
f'=-\left(x-\frac{1}{y}\right),
$$

$$
f=-\frac{1}{2}\left(x-\frac{1}{y}\right)^2.
$$

$$
\begin{aligned}
F
 &=\frac{1}{2y^2}
   -\frac{1}{2}\left(x-\frac{1}{y}\right)^2\\
 &=-\frac{1}{2}x^2+\frac{x}{y}.
\end{aligned}
$$

$$
xy\frac{\partial G}{\partial x}
 -(xy^3-y^2)\frac{\partial G}{\partial y}=1.
$$

$$
xyg'-(xy^3-y^2)\frac{1}{y^2}g'=1.
$$

$$
g'=1,
\qquad
g=x-\frac{1}{y}.
$$


Transform is

$$
X=-\frac{1}{2}x^2+\frac{x}{y},
\qquad
Y=x-\frac{1}{y}.
$$

$$
Y'
=\frac{1+\dfrac{y'}{y^2}}
       {-x+\dfrac{1}{y}-\dfrac{xy'}{y^2}}
=\frac{1}
       {\dfrac{y}{y^2+y'}-x}.
$$

$$
\begin{aligned}
Y''
={}&\frac{1}
 {-x+\dfrac{1}{y}-\dfrac{xy'}{y^2}}
 \left\{
 -\frac{1}
  {\left(\dfrac{y}{y^2+y'}-x\right)^2}
 \left[
  \frac{y'}{y^2+y'}
  -\frac{y(2yy'+y'')}{(y^2+y')^2}
  -1
 \right]
 \right\}\\[6pt]
={}&\frac{1}{(\cdots)}
 \left\{
  y^2y'+(y')^2-2y^2y'-yy''-y^4-2y^2y'-(y')^2
 \right\}\\[6pt]
={}&\frac{1}{(\cdots)}
 \left\{-y\left(y''+3yy'+y^3\right)\right\}=0.
\end{aligned}
$$

$$
Y=A+BX.
$$

$$
x-\frac{1}{y}
=A+B\left(-\frac{1}{2}x^2+\frac{x}{y}\right).
$$

$$
(1+Bx)\frac{1}{y}=\frac{1}{2}Bx^2+x-A.
$$

$$
\begin{aligned}
y
 &=\frac{1+Bx}{-A+x+\frac{1}{2}Bx^2}\\[4pt]
 &=\frac{2(I_1+x)}{2I_2+2I_1x+x^2},
\end{aligned}
$$

$$
I_1=\frac{1}{B},
\qquad
I_2=-\frac{A}{B}.
$$

Aside: differentiating $y$ and solving for $I_1$ and $I_2$ between the two
equations gives

$$
I_1=-x+\frac{y}{y'+y^2}
\qquad
\left(=\frac{1}{Y'}\right),
$$

and

$$
I_2=\frac{1}{2}x^2+\frac{1-xy}{y'+y^2},
$$

which are two resonance integrals.


$$
y''+3yy'+y^3=0.
$$

Is this the only equation of this structure which can be linearised?

Let

$$
x=\alpha X,
\qquad
y=\beta Y.
$$

Then

$$
\frac{\beta}{\alpha^2}Y''
+3\frac{\beta^2}{\alpha}YY'
+\beta^3Y^3=0,
$$

or

$$
Y''+3\alpha\beta YY'+(\alpha\beta)^2Y^3=0.
$$

Any choice of $\alpha\beta$ will give a different equation which can also be
linearised.

Let

$$
\begin{aligned}
y   &=\frac{\omega'}{\omega},\\[4pt]
y'  &=\frac{\omega''\omega-(\omega')^2}{\omega^2},\\[4pt]
y'' &=\frac{\omega'''\omega^2-3\omega''\omega'\omega+2(\omega')^3}
           {\omega^3}.
\end{aligned}
$$

Thus

$$
y''+3yy'+y^3\longrightarrow\frac{\omega'''}{\omega}=0,
$$

so

$$
\omega'''=0,
\qquad
\omega=A+Bx+Cx^2,
$$

and

$$
y=\frac{B+2Cx}{A+Bx+Cx^2}.
$$

<a id="riccati-equation"></a>
This is a particular instance of a Riccati hierarchy. Recall the Riccati
equation which is obtained from the general second-order linear differential
equation

$$
y''+f(x)y'+g(x)y=0.
$$

Let

$$
\frac{y'}{y}=u.
$$

Then

$$
\frac{y''}{y}-\frac{(y')^2}{y^2}=u',
\qquad
\frac{y''}{y}=u'+u^2,
$$

and hence

$$
u'+u^2+f(x)u+g(x)=0,
$$

the Riccati equation.


<a id="nonlinear-equations-with-sl3-symmetry"></a>
### What nonlinear differential equations can have $SL(3,\mathbb R)$ symmetry?

It must be possible to transform the differential equation to

$$
Q''=0
$$

by means of a point transformation. The reverse also applies.

Let

$$
T=F(q,t),
\qquad
Q=G(q,t).
$$

Then $Q''=0$ becomes

$$
\begin{aligned}
\ddot q
{}&+\dot q^3J^{-1}
 \left(
  \frac{\partial F}{\partial q}\frac{\partial^2G}{\partial q^2}
  -\frac{\partial^2F}{\partial q^2}\frac{\partial G}{\partial q}
 \right)\\[4pt]
{}&+\dot q^2J^{-1}
 \left(
  \frac{\partial F}{\partial t}\frac{\partial^2G}{\partial q^2}
  +2\frac{\partial F}{\partial q}\frac{\partial^2G}{\partial q\partial t}
  -2\frac{\partial^2F}{\partial q\partial t}\frac{\partial G}{\partial q}
  -\frac{\partial^2F}{\partial q^2}\frac{\partial G}{\partial t}
 \right)\\[4pt]
{}&+\dot qJ^{-1}
 \left(
  2\frac{\partial F}{\partial t}\frac{\partial^2G}{\partial q\partial t}
  +\frac{\partial F}{\partial q}\frac{\partial^2G}{\partial t^2}
  -\frac{\partial^2F}{\partial t^2}\frac{\partial G}{\partial q}
  -2\frac{\partial^2F}{\partial q\partial t}\frac{\partial G}{\partial t}
 \right)\\[4pt]
{}&+J^{-1}
 \left(
  \frac{\partial F}{\partial t}\frac{\partial^2G}{\partial t^2}
  -\frac{\partial^2F}{\partial t^2}\frac{\partial G}{\partial t}
 \right)=0,
\end{aligned}
$$

where

$$
J=\frac{\partial(F,G)}{\partial(t,q)}\ne0.
$$


The general third-order constant-coefficient equation

$$
\omega'''+a\omega''+b\omega'+c\omega=0
$$

has a solution. Let

$$
\frac{\omega'}{\omega}=y.
$$

Then

$$
\frac{\omega'}{\omega}=y,
\qquad
\frac{\omega''}{\omega}=y'+y^2,
$$

and

$$
\begin{aligned}
\frac{\omega'''}{\omega}
 &=y''+2yy'+y(y'+y^2)\\
 &=y''+3yy'+y^3.
\end{aligned}
$$

Thus

$$
y''+3yy'+y^3+a(y'+y^2)+by+c=0
$$

can be linearised.

Aside: $\omega'''=0$ has seven symmetries.

<a id="linearisable-second-order-odes"></a>
### What second-order ODEs can be linearised?

$$
y''=f(x,y,y').
$$

(Transparency.)

<a id="classify-by-extra-information"></a>
#### Classify by extra information

<a id="existence-of-one-symmetry"></a>
##### 1. Existence of one symmetry

Transform the differential equation to
standard form by making the symmetry, say, $\partial/\partial x$. Then the
differential equation is

$$
y''=f(y,y').
$$

<a id="existence-of-two-symmetries"></a>
##### 2. Existence of two symmetries

The possibilities include

$$
G_1=x\frac{\partial}{\partial x},
\qquad
G_2=\frac{\partial}{\partial x},
\qquad
[G_1,G_2]=-G_2,
$$

and

$$
G_1=\frac{\partial}{\partial x},
\qquad
G_2=\frac{\partial}{\partial y},
\qquad
[G_1,G_2]=0,
$$

with $G_2\ne f(x,y)G_1$.

<a id="two-commuting-symmetries"></a>
##### Two commuting symmetries

$$
[G_1,G_2]=0.
$$

Suppose we choose coordinates such that

$$
G_1=\frac{\partial}{\partial y},
\qquad
G_2=\xi\frac{\partial}{\partial x}
    +\eta\frac{\partial}{\partial y}.
$$

Then

$$
[G_1,G_2]
=\frac{\partial\xi}{\partial y}\frac{\partial}{\partial x}
 +\frac{\partial\eta}{\partial y}\frac{\partial}{\partial y}=0,
$$

so

$$
\frac{\partial\xi}{\partial y}=0,
\qquad
\frac{\partial\eta}{\partial y}=0.
$$


<a id="type-ii"></a>
Observe that Type II is already linear and the solution is reduced to two
quadratures.

<a id="two-non-commuting-symmetries"></a>
##### Two non-commuting symmetries

$$
[G_1,G_2]=G_1.
$$

Let the coordinates be such that

$$
G_1=\frac{\partial}{\partial y},
\qquad
G_2=\xi\frac{\partial}{\partial x}
    +\eta\frac{\partial}{\partial y}.
$$

Then $[G_1,G_2]=G_1$ becomes

$$
\frac{\partial\xi}{\partial y}\frac{\partial}{\partial x}
 +\frac{\partial\eta}{\partial y}\frac{\partial}{\partial y}
=\frac{\partial}{\partial y},
$$

so

$$
\frac{\partial\xi}{\partial y}=0,
\qquad
\frac{\partial\eta}{\partial y}=1.
$$

Thus

$$
\xi=a(x),
\qquad
\eta=b(x)+y,
$$

and

$$
G_2=a(x)\frac{\partial}{\partial x}
 +(b(x)+y)\frac{\partial}{\partial y}.
$$

The two forms written for $G_2$ are

$$
\begin{aligned}
G_2 &=y\frac{\partial}{\partial y}=yG_1,\\
G_2 &=x\frac{\partial}{\partial x}
      +y\frac{\partial}{\partial y}
      \ne f(x,y)G_1.
\end{aligned}
$$

<a id="types-i-and-iv"></a>
Note that Type IV is linear. Type I,

$$
y''=f(y'),
$$

and Type III,

$$
y''=\frac1x f(y'),
$$

are the ones whose linearisation is still an open question.

<a id="linearisation-proposition"></a>
#### Proposition

In order that a second-order ODE have the symmetry algebra
$\mathfrak{sl}(3,\mathbb R)$, and hence be linearisable, it is necessary and
sufficient that it have the three-element subalgebra $\mathcal X$ of generators
$G_1,G_2,G_3$ satisfying

$$
[G_1,G_2]=0,
\qquad
[G_2,G_3]=0,
\qquad
[G_3,G_1]=G_2.
$$

<a id="linearisation-proposition-proof"></a>
#### Proof

For

$$
y''=f(x,y,y'),
$$

a generator of symmetry is

$$
G=\xi\frac{\partial}{\partial x}
 +\eta\frac{\partial}{\partial y}.
$$

We have three such generators, $G_1,G_2,G_3$. We cannot have both

$$
G_1=\phi(x,y)G_2,
\qquad
G_3=\psi(x,y)G_2,
$$

for suitable functions $\phi$ and $\psi$.


Suppose they are. Then

$$
\begin{aligned}
[G_1,G_2]
 &=[\phi G_2,G_2]\\
 &=\phi G_2G_2-G_2(\phi G_2)\\
 &=\phi G_2G_2-\phi G_2G_2-(G_2\phi)G_2\\
 &=0.
\end{aligned}
$$

if $G_2\phi=0$. Likewise,

$$
[G_2,G_3]=0
\quad\Longrightarrow\quad
G_2\psi=0.
$$

Then

$$
\begin{aligned}
[G_3,G_1]
 &=[\psi G_2,\phi G_2]\\
 &=\psi G_2(\phi G_2)-\phi G_2(\psi G_2)\\
 &=\psi\phi G_2G_2+\psi(G_2\phi)G_2
   -\phi\psi G_2G_2-\phi(G_2\psi)G_2\\
 &=0\ne G_2.
\end{aligned}
$$

We assume that there is no function $\phi(x,y)$ such that

$$
G_1=\phi G_2.
$$

(We cannot make the statement about both $G_1$ and $G_3$.) Hence there exists
a regular point transformation

$$
Y=F(x,y),
\qquad
X=G(x,y),
$$

such that the generators are

$$
G_1=\frac{\partial}{\partial x},
\qquad
G_2=\frac{\partial}{\partial y}.
$$

Write

$$
G_3=\xi\frac{\partial}{\partial x}
    +\eta\frac{\partial}{\partial y}.
$$

Then

$$
\begin{aligned}
[G_2,G_3]
 &=\frac{\partial\xi}{\partial y}\frac{\partial}{\partial x}
   +\frac{\partial\eta}{\partial y}\frac{\partial}{\partial y}
 &&=0,\\[6pt]
[G_3,G_1]
 &=-\frac{\partial\xi}{\partial x}\frac{\partial}{\partial x}
   -\frac{\partial\eta}{\partial x}\frac{\partial}{\partial y}
 &&=\frac{\partial}{\partial y}.
\end{aligned}
$$

That is,

$$
\frac{\partial\xi}{\partial x}=0,
\qquad
\frac{\partial\xi}{\partial y}=0,
\qquad
\frac{\partial\eta}{\partial x}=-1,
\qquad
\frac{\partial\eta}{\partial y}=0.
$$


Thus

$$
G_3=A\frac{\partial}{\partial x}
 +(B-x)\frac{\partial}{\partial y}.
$$

Take

$$
G_3=-x\frac{\partial}{\partial y}.
$$

The equation

$$
y''=f(x,y,y')
$$

is invariant under these three generators. From

$$
G_1^{(2)}[y''-f(x,y,y')]=0
$$

we obtain

$$
\frac{\partial f}{\partial x}=0.
$$

From

$$
G_2^{(2)}[y''-f]=0
$$

we obtain

$$
\frac{\partial f}{\partial y}=0.
$$

Finally,

$$
G_3^{(2)}[y''-f]=0
$$

gives

$$
\left(-x\frac{\partial}{\partial y}
      -\frac{\partial}{\partial y'}\right)[y''-f]=0,
$$

so

$$
x\frac{\partial f}{\partial y}
+\frac{\partial f}{\partial y'}=0.
$$

Therefore $f$ is constant and the differential equation is

$$
y''=\text{constant},
$$

which is linear. Hence the symmetry of the original differential equation is
$\mathfrak{sl}(3,\mathbb R)$. We have proven sufficiency, i.e. the possession
of $G_1,G_2,G_3$ such that

$$
[G_1,G_2]=0,
\qquad
[G_2,G_3]=0,
\qquad
[G_3,G_1]=G_2.
$$

Necessity follows from $\mathcal X$ being a subalgebra of
$\mathfrak{sl}(3,\mathbb R)$.

<a id="can-one-do-better"></a>
#### Can one do better?

Suppose we have two generators $G_1$ and $G_2$ with the
properties

$$
[G_1,G_2]=0,
\qquad
G_2=f(x,y)G_1.
$$

Make a point transformation to $X,Y$ so that

$$
\overline G_1=\frac{\partial}{\partial Y},
\qquad
\overline G_2=F(X,Y)\frac{\partial}{\partial Y}.
$$

Then

$$
[\overline G_1,\overline G_2]
=\left[\frac{\partial}{\partial Y},
 F\frac{\partial}{\partial Y}\right]
=\frac{\partial F}{\partial Y}\frac{\partial}{\partial Y}=0,
$$

and hence

$$
\frac{\partial F}{\partial Y}=0.
$$


Without loss of generality, take $F=X$. Thus

$$
G_1=\frac{\partial}{\partial y},
\qquad
G_2=x\frac{\partial}{\partial y},
$$

which means that $y''=F(x,y,y')$ has the form

$$
y''=F(x),
$$

which is linear and so has $\mathfrak{sl}(3,\mathbb R)$ symmetry.

If we take $G_1$ and $G_2$ such that

$$
[G_1,G_2]=0,
\qquad
G_2\ne f(x,y)G_1,
$$

we may take the standard forms

$$
G_1=\frac{\partial}{\partial x},
\qquad
G_2=\frac{\partial}{\partial y}.
$$

The differential equation invariant under $G_1$ and $G_2$ is

$$
y''=f(y').
$$

We know that linearisation is possible only if the differential equation has
the form

$$
y''+( )y'^3+( )y'^2+( )y'+( )=0,
$$

where the quantities in parentheses are functions of $x$ and $y$. Hence
$f(y')$ must take the form

$$
f(y')=a(y')^3+b(y')^2+cy'+d,
$$

where $a,b,c,d$ are constants. Thus

$$
y''=a(y')^3+b(y')^2+cy'+d,
\qquad a\ne0.
$$

Put

$$
y=\alpha Y+\beta X,
\qquad
x=\gamma X.
$$

Then

$$
\frac{\alpha}{\gamma^2}Y''
=a\left(\frac{\alpha}{\gamma}Y'+\beta\right)^3
 +b\left(\frac{\alpha}{\gamma}Y'+\beta\right)^2
 +c\left(\frac{\alpha}{\gamma}Y'+\beta\right)+d.
$$

The choice

$$
\beta=-\frac{b}{3a}
$$

removes the $(Y')^2$ term, and

$$
\frac{a\alpha^2}{\gamma}=1
$$

makes the coefficient of $(Y')^3$ unity. The equation reduces to

$$
y''=(y')^3+cy'+d.
$$


For this to be linearisable there must be six more generators in addition to
the two we already have. Let

$$
G=\xi\frac{\partial}{\partial x}
 +\eta\frac{\partial}{\partial y}
$$

be one of them. Then

$$
\begin{aligned}
{}&\eta_{xx}+2y'\eta_{xy}+(y')^2\eta_{yy}
 +\bigl((y')^3+cy'+d\bigr)\eta_y\\
{}&\quad-y'\left[
 \xi_{xx}+2y'\xi_{xy}+(y')^2\xi_{yy}
 +\bigl((y')^3+cy'+d\bigr)\xi_y
 \right]\\
{}&\quad-2\bigl((y')^3+cy'+d\bigr)(\xi_x+y'\xi_y)\\
{}&=(3(y')^2+c)
 \left(\eta_x+y'\eta_y-y'\xi_x-(y')^2\xi_y\right).
\end{aligned}
$$

Equating coefficients of like powers of $y'$ gives

$$
\xi_{yy}+2\eta_y-\xi_x=0,
\tag{i}
$$

$$
2c\xi_y+3\eta_x-\eta_{yy}+2\xi_{xy}=0,
\tag{ii}
$$

$$
c\xi_x+3d\xi_y-2\eta_{xy}+\xi_{xx}=0,
\tag{iii}
$$

$$
c\eta_x-d\eta_y+2d\xi_x-\eta_{xx}=0.
\tag{iv}
$$

From (iv),

$$
\xi_x=\frac1{2d}\left(\eta_{xx}+d\eta_y-c\eta_x\right),
$$

and from (iii),

$$
\begin{aligned}
\xi_y
 &=\frac1{3d}\left(2\eta_{xy}-\xi_{xx}-c\xi_x\right)\\
 &=\frac1{3d}\left[
 2\eta_{xy}
 -\frac1{2d}\left(\eta_{xxx}+d\eta_{xy}-c\eta_{xx}\right)
 -\frac{c}{2d}\left(\eta_{xx}+d\eta_y-c\eta_x\right)
 \right].
\end{aligned}
$$


Eliminate $\eta$. From (i),

$$
\eta_y=\frac12(\xi_x-\xi_{yy}),
\tag{v}
$$

and from (ii),

$$
\begin{aligned}
\eta_x
 &=\frac13(\eta_{yy}-2c\xi_y-2\xi_{xy})\\
 &=-\frac16(\xi_{yyy}+3\xi_{xy}+4c\xi_y),
\end{aligned}
\tag{vi}
$$

using (v). Substitution of (v) and (vi) in (iii) and (iv) gives

$$
\xi_{xyy}+3d\xi_y+c\xi_x=0,
\tag{vii}
$$

and

$$
\xi_{xyyy}-c\xi_{yyy}+3\xi_{xxy}
+3d\xi_{yy}+c\xi_{xy}-4c^2\xi_y+9d\xi_x=0.
\tag{viii}
$$

The consistency condition between (v) and (vi) is

$$
\frac{\partial}{\partial x}(\eta_y)
=\frac{\partial}{\partial y}(\eta_x),
$$

which gives

$$
\xi_{yyyy}+4c\xi_{yy}+3\xi_{xx}=0.
\tag{ix}
$$

Using the $y$ derivative of (vii), equation (viii) simplifies to

$$
-c\xi_{yyy}+3\xi_{xxy}-4c^2\xi_y+9d\xi_x=0.
\tag{x}
$$

Also,

$$
\frac{\partial^2}{\partial y^2}(\text{vii})
-\frac{\partial}{\partial x}(\text{ix})
$$

gives

$$
\xi_{xxx}+c\xi_{xyy}-d\xi_{yyy}=0.
\tag{xi}
$$

Use (vii), (x), and (xi), which can be shown to be equivalent to (vii),
(viii), and (ix), except when both $c$ and $d$ are zero.

For the case $c=d=0$,

$$
\xi_{xyy}=0,
\tag{vii}
$$

and

$$
\xi_{xyyy}+3\xi_{xxy}=0.
\tag{viii}
$$



Equation (ix) becomes

$$
\xi_{yyyy}+3\xi_{xx}=0.
\tag{ix}
$$

From (vii),

$$
\xi=F(y)+yG(x)+H(x).
$$

Equation (viii) gives

$$
G''=0.
$$

In (ix),

$$
F^{(4)}+3yG''+H''=0.
$$

Therefore

$$
\begin{aligned}
G &=G_0+G_1x,\\
H &=H_0+H_1x+\frac{H_2}{2}x^2,\\
F &=F_1+F_2y+F_3y^2+F_4y^3-\frac{H_2}{24}y^4.
\end{aligned}
$$

Thus

$$
\begin{aligned}
\xi={}&F_1+F_2y+F_3y^2+F_4y^3-\frac{H_2}{24}y^4
       +G_0y+G_1xy\\
     &+H_0+H_1x+\frac{H_2}{2}x^2\\[4pt]
={}&(F_1+H_0)+(F_2+G_0)y+F_3y^2+F_4y^3+G_1xy+H_1x\\
 &+H_2\left(\frac12x^2-\frac1{24}y^4\right).
\end{aligned}
$$

The eighth generator comes from

$$
\eta_x=0,
\qquad
\eta_y=0
$$

when $\xi=0$, i.e. $\eta$ is constant. This gives

$$
G_1=\frac{\partial}{\partial y}.
$$

For

$$
y''=(y')^3,
$$

find the linearising transformation.


Note that we have

$$
\frac{\partial}{\partial x},
\qquad
\frac{\partial}{\partial y},
\qquad
-y\frac{\partial}{\partial x}.
$$

Let $X=y$ and $Y=x$. Then the algebra consists of

$$
\frac{\partial}{\partial Y},
\qquad
\frac{\partial}{\partial X},
\qquad
-X\frac{\partial}{\partial Y},
$$

which is the standard form. Also,

$$
Y'=\frac1{y'},
\qquad
Y''=-\frac{y''}{(y')^3}.
$$

Thus $y''=(y')^3$ becomes

$$
-\frac{Y''}{(Y')^3}=\frac1{(Y')^3},
$$

or

$$
Y''=-1.
$$

Hence

$$
\begin{aligned}
Y &=A+BX-\frac12X^2,\\
x &=A+By-\frac12y^2.
\end{aligned}
$$

Alternatively,

$$
\begin{aligned}
y''                 &=(y')^3,\\[4pt]
\frac{y''}{(y')^2}  &=y',\\[4pt]
-\frac1{y'}         &=y+C,\\[4pt]
-1                   &=(y+C)y'.
\end{aligned}
$$

and

$$
K-x=\frac12y^2+Cy.
$$


Retain

$$
\xi_{xxx}+c\xi_{xyy}-d\xi_{yyy}=0,
\tag{xi}
$$

$$
\xi_{xyy}+3d\xi_y+c\xi_x=0,
\tag{vii}
$$

and

$$
-c\xi_{yyy}+3\xi_{xxy}-4c^2\xi_y+9d\xi_x=0.
\tag{x}
$$

Observe that (xi) is homogeneous in order. Hence it can be solved by
factorisation. Suppose it factors as

$$
\left(\frac{\partial}{\partial x}
      +\alpha\frac{\partial}{\partial y}\right)
\left(\frac{\partial}{\partial x}
      +\beta\frac{\partial}{\partial y}\right)
\left(\frac{\partial}{\partial x}
      +\gamma\frac{\partial}{\partial y}\right)\xi=0.
$$

Corresponding to each factor there is an equation for a characteristic.
However, one must be careful of repeated roots. Observe that

$$
\alpha\beta\gamma=-d,
\qquad
\alpha\beta+\beta\gamma+\gamma\alpha=c,
\qquad
\alpha+\beta+\gamma=0.
$$

A triple root gives $c=d=0$, which has already been treated.

For two roots $\alpha$ and $-2\alpha$,

$$
c=-3\alpha^2,
\qquad
d=-2\alpha^3,
$$

and

$$
\left(\frac{\partial}{\partial x}
      +\alpha\frac{\partial}{\partial y}\right)^2
\left(\frac{\partial}{\partial x}
      -2\alpha\frac{\partial}{\partial y}\right)\xi=0.
$$

The characteristic equations give

$$
u_1=\alpha x-y,
\qquad
u_2=2\alpha x+y.
$$

The solution is

$$
\xi=f(\alpha x-y)+g(2\alpha x+y)
 +(2\alpha x+y)h(\alpha x-y).
$$

Substitution in (xi) and (x) gives

$$
\begin{aligned}
{}&\alpha f'''+2\alpha g'''+2\alpha h''+\alpha h''-\alpha h'''
 +\alpha(2\alpha x+y)h'''\\
{}&\quad-6\alpha^3[-f'+g'+h-(2\alpha x+y)h']\\
{}&\quad-3\alpha^2[\alpha f''+2\alpha g''+2\alpha h
 +\alpha(2\alpha x+y)h'']=0.
\end{aligned}
$$


The separate coefficients give

$$
2\alpha g'''-12\alpha^3g'=0,
$$

$$
(2\alpha x+y)(h'''+3\alpha^3h')=0,
$$

and

$$
\alpha f'''+2\alpha h''+3\alpha^3f'=0.
$$

The remaining determining equation is

$$
\begin{aligned}
0={}&3\alpha^2\left[-f'''+g'''+3h''-(2\alpha x+y)h'''\right]\\
&+3\left[-\alpha^2f'''+4\alpha^2g'''-3\alpha^2h''
          -\alpha^2(2\alpha x+y)h'''\right]\\
&+36\alpha^4\left[-f'+g'+h-(2\alpha x+y)h'\right]\\
&-18\alpha^3\left[\alpha f''+2\alpha g''+2\alpha h'
                   +\alpha(2\alpha x+y)h''\right].
\end{aligned}
\tag{ix}
$$

The correct equations are

$$
g'''=0,
\qquad
h'''+9\alpha^2h'=0,
\qquad
f'''+9\alpha^2f'=0.
$$

The solutions written in the notes are

$$
\begin{aligned}
g &=G_0+G_1(2\alpha x+y)+G_2(2\alpha x+y)^2,\\
h &=\left[H_0+H_1\sin 3\alpha(\alpha x-y)
            +H_2\cos 3\alpha(\alpha x-y)\right](2\alpha x+y),\\
f &=f_0+F_1(\alpha x-y)+F_2(\alpha x-y)^2.
\end{aligned}
$$

There are seven independent solutions and so seven generators. The eighth
comes from $\xi=0$, giving $\eta$ constant.

For three distinct roots,

$$
-2\alpha,
\qquad
\alpha+\delta,
\qquad
\alpha-\delta,
$$

where $\delta\ne0,\pm3\alpha$ to avoid repeated roots, equation (xi) becomes

$$
\left(\frac{\partial}{\partial x}
      -2\alpha\frac{\partial}{\partial y}\right)
\left(\frac{\partial}{\partial x}
      +(\alpha+\delta)\frac{\partial}{\partial y}\right)
\left(\frac{\partial}{\partial x}
      +(\alpha-\delta)\frac{\partial}{\partial y}\right)\xi=0,
$$

where

$$
c=\delta^2-3\alpha^2,
\qquad
d=2\alpha(\alpha^2-\delta^2).
$$

The characteristics give

$$
u_1=2\alpha x+y,
\qquad
u_2=(\alpha+\delta)x-y,
\qquad
u_3=(\alpha-\delta)x-y,
$$

and

$$
\xi=f(2\alpha x+y)
 +g((\alpha+\delta)x-y)
 +h((\alpha-\delta)x-y).
$$


Substitution into (vii) and (xi) again gives seven generators, which become
eight with $\eta$ constant. Hence

$$
y''=(y')^3+cy'+d
$$

is always linearisable.

From Type 2, consider

$$
y''=(y')^2+d.
$$

Invariance under

$$
G=\xi\frac{\partial}{\partial x}
 +\eta\frac{\partial}{\partial y}
$$

gives

$$
\begin{aligned}
{}&\eta_{xx}+2y'\eta_{xy}+(y')^2\eta_{yy}
 +\bigl((y')^2+d\bigr)\eta_y\\
{}&\quad-y'\left[
 \xi_{xx}+2y'\xi_{xy}+(y')^2\xi_{yy}
 +\bigl((y')^2+d\bigr)\xi_y
 \right]\\
{}&\quad-2\bigl((y')^2+d\bigr)(\xi_x+y'\xi_y)\\
{}&=2y'\left(\eta_x+y'\eta_y-y'\xi_x-(y')^2\xi_y\right).
\end{aligned}
$$

Equating powers of $y'$ gives

$$
\xi_{yy}+\xi_y=0,
\tag{i}
$$

$$
\eta_{yy}-\eta_y-2\xi_{xy}=0,
\tag{ii}
$$

$$
2\eta_{xy}-2\eta_x-\xi_{xx}-3d\xi_y=0,
\tag{iii}
$$

$$
\eta_{xx}+d\eta_y-2d\xi_x=0.
\tag{iv}
$$

From (i),

$$
\xi=a(x)+b(x)e^{-y}.
$$

In (ii),

$$
\eta_{yy}-\eta_y=-2b'e^{-y},
$$

so

$$
\eta=c(x)+d(x)e^y-b'(x)e^{-y}.
$$


Substitution in (iii) gives

$$
2[d'e^y+b''e^{-y}]
-2[c'+d'e^y-b''e^{-y}]
-[a''+b''e^{-y}]+3Db e^{-y}=0.
$$

Thus

$$
3(b''+Db)=0,
\tag{v}
$$

$$
a''+2c'=0.
\tag{vi}
$$

Substitution in (iv) gives

$$
c''+d''e^y-b'''e^{-y}
+D[de^y+b'e^{-y}]-2D[a'+b'e^{-y}]=0,
$$

so

$$
b'''+Db'=0
\qquad\text{(redundant)},
$$

$$
d''+Dd=0,
\tag{vii}
$$

and

$$
c''-2Da'=0.
\tag{viii}
$$

Equations (v), (vi), and (viii) give

$$
a'''+4Da'=0,
$$

and

$$
c'=C_1+2Da.
$$

There appear to be nine constants, so one must be lost. Write

$$
\begin{aligned}
a  &=A_0+A_1f_1+A_2f_2,\\
c' &=(C_1+A_0)+A_1f_1+A_2f_2,\\
c  &=C_0+(C_1+A_0)x+A_1\int f_1\,dx+A_2\int f_2\,dx.
\end{aligned}
$$

Thus all differential equations of the form

$$
y''=(y')^2+d
$$

have $\mathfrak{sl}(3,\mathbb R)$ symmetry and are accordingly linearisable.
Writing $D=\omega^2$, the generators are

$$
\begin{aligned}
G_1&=\frac{\partial}{\partial y},\\
G_2&=\frac{\partial}{\partial x},\\
G_3&=e^{-2\omega x}\frac{\partial}{\partial x}
    +\omega e^{-2\omega x}\frac{\partial}{\partial y},\\
G_4&=e^{2\omega x}\frac{\partial}{\partial x}
    -\omega e^{2\omega x}\frac{\partial}{\partial y},\\
G_5&=e^{-y+\omega x}\frac{\partial}{\partial x}
    -\omega e^{-y+\omega x}\frac{\partial}{\partial y},\\
G_6&=e^{-y-\omega x}\frac{\partial}{\partial x}
    +\omega e^{-y-\omega x}\frac{\partial}{\partial y},\\
G_7&=e^{y-\omega x}\frac{\partial}{\partial y},\\
G_8&=e^{y+\omega x}\frac{\partial}{\partial y}.
\end{aligned}
$$


The set $\{G_3,G_6,G_7\}$ has the algebra $\mathcal X$:

$$
[G_3,G_6]=0,
\qquad
[G_6,G_7]=-G_3,
\qquad
[G_7,G_3]=0.
$$

The standard form of $\mathcal X$ is as on page 12:

$$
[\widetilde G_1,\widetilde G_2]=0,
\qquad
[\widetilde G_2,\widetilde G_3]=0,
\qquad
[\widetilde G_3,\widetilde G_1]=\widetilde G_2,
$$

with

$$
\widetilde G_1=\frac{\partial}{\partial x},
\qquad
\widetilde G_2=\frac{\partial}{\partial y},
\qquad
\widetilde G_3=-x\frac{\partial}{\partial y}.
$$

Try

$$
G_6\longrightarrow-\widetilde G_3,
\qquad
G_7\longrightarrow\widetilde G_1,
\qquad
G_3\longrightarrow\widetilde G_2.
$$

Let

$$
X=F(x,y),
\qquad
Y=G(x,y).
$$

The mapping of $G_7$ gives

$$
e^{y-\omega x}\left[
 \frac{\partial F}{\partial y}\frac{\partial}{\partial X}
 +\frac{\partial G}{\partial y}\frac{\partial}{\partial Y}
\right]
=\frac{\partial}{\partial X}.
$$

Hence

$$
\frac{\partial F}{\partial y}=e^{-y+\omega x},
\qquad
F=k(x)-e^{-y+\omega x},
$$

and

$$
\frac{\partial G}{\partial y}=0,
\qquad
G=m(x).
$$

The mapping of $G_6$ gives

$$
e^{-y-\omega x}\left(
 \frac{\partial F}{\partial x}
 +\omega\frac{\partial F}{\partial y}
\right)=0,
\tag{*}
$$

and

$$
e^{-y-\omega x}\left(
 \frac{\partial G}{\partial x}
 +\omega\frac{\partial G}{\partial y}
\right)=F.
\tag{\text{†}}
$$

Substitution in (*) gives

$$
k'-\omega e^{-y+\omega x}+\omega e^{-y+\omega x}=0,
$$

so $k'=0$. Equation $\text{(†)}$ gives

$$
e^{-y-\omega x}m'=k-e^{-y+\omega x},
$$

or

$$
m'=ke^{y+\omega x}-e^{2\omega x}.
$$

Thus $k=0$, and the notes take

$$
F=-e^{-y+\omega x},
\qquad
G=-e^{2\omega x}.
$$


Check:

$$
\begin{aligned}
Y'  &=\frac{-2\omega e^{2\omega x}}
           {(y'-\omega)e^{-y+\omega x}},\\[6pt]
Y'' &=\frac{
       -4\omega^2e^{2\omega x}(y'-\omega)e^{-y+\omega x}
       +2\omega e^{2\omega x}
        \left[y''-(y'-\omega)^2\right]e^{-y+\omega x}}
       {\left[(y'-\omega)e^{-y+\omega x}\right]^3}.
\end{aligned}
$$

The numerator is

$$
\begin{aligned}
2\omega e^{-y+3\omega x}
&\left[-2\omega y'+2\omega^2+y''-(y')^2+2\omega y'-\omega^2\right]\\
&=2\omega e^{-y+3\omega x}
  \left[y''-(y')^2+\omega^2\right],
\end{aligned}
$$

as required.

The solution of $Y''=0$ is

$$
Y=A+BX.
$$

The solution of $y''=(y')^2-\omega^2$ is therefore

$$
\begin{aligned}
-e^{2\omega x} &=A+Be^{-y+\omega x},\\
e^{-y}         &=Ce^{\omega x}+De^{-\omega x},\\
y              &=-\log\left(Ce^{\omega x}+De^{-\omega x}\right).
\end{aligned}
$$

Check:

$$
\begin{aligned}
\frac{y''}{(y')^2-\omega^2}
  &=1,\\[4pt]
\frac{d\left((y')^2\right)}{(y')^2-\omega^2}
  &=2\,dy,\\[4pt]
\log\left((y')^2-\omega^2\right)
  &=2y+k,\\[4pt]
(y')^2
  &=\omega^2+me^{2y}.
\end{aligned}
$$

Thus

$$
I=\int\frac{dy}{\left(\omega^2+me^{2y}\right)^{1/2}}
  =\int dx.
$$

Let

$$
e^{-y}=u,
\qquad
-e^{-y}\,dy=du,
\qquad
dy=-\frac{du}{u}.
$$

Then

$$
I
=\int\frac{-u^{-1}\,du}
           {\left(\omega^2+mu^{-2}\right)^{1/2}}
=-\int\frac{du}{\left(\omega^2u^2+m\right)^{1/2}}.
$$


$$
I=-\frac1\omega
   \operatorname{arsinh}\left(\frac{\omega u}{m^{1/2}}\right)
  =x+c.
$$

Therefore

$$
\begin{aligned}
u &=-\frac{m^{1/2}}{\omega}\sinh(\omega x+\alpha),\\[4pt]
y &=-\log\left[-\frac{m^{1/2}}{\omega}
                      \sinh(\omega x+\alpha)\right].
\end{aligned}
$$

As an aside,

$$
\int\frac{dx}{(a^2+e^{2x})^{1/2}}
=\int\frac{e^{-x}\,dx}{(a^2e^{-2x}+1)^{1/2}}
=\int\frac{-d(e^{-x})}{(a^2e^{-2x}+1)^{1/2}}.
$$

<a id="type-iii"></a>
### Type III

$$
xy''=f(y'),
\qquad
f(y')=a(y')^3+b(y')^2+cy'+d.
$$

There are two standard forms:

$$
\begin{aligned}
xy'' &=(y')^3+cy'+d, &&a\ne0,\\
xy'' &=(y')^2+d,     &&a=0.
\end{aligned}
$$

Using the same type of analysis, it is found that only

$$
xy''=(y')^3+y'
$$

is linearisable.

Note that the equation

<a id="sl2r-symmetry"></a>
$$
xy''=(y')^3-\frac12y'
$$

does have $\mathfrak{sl}(2,\mathbb R)$ symmetry.

<a id="linearisation-of-a-system-of-equations"></a>
### Linearisation of a system of equations

<a id="system-linearisation-theorem"></a>
#### Theorem

A system of $n$ equations of the form

$$
A\mathbf u''=\mathbf f(x,\mathbf u,\mathbf u'),
\tag{1}
$$

where $A$ is a constant matrix, is linearisable if it possesses the algebra
$\mathcal N$:

$$
\begin{aligned}
[H,G_i]   &=0,       &&\text{(2)}\\
[G_i,G_j] &=0,       &&\text{(3)}\\
[G_i,X_j] &=0,       &&\text{(4)}\\
[H,X_i]   &=G_i.     &&\text{(5)}
\end{aligned}
$$

where $\{H,G_i,X_i\mid i=1,\ldots,n\}$ are symmetries of (1).


<a id="system-linearisation-proof"></a>
#### Proof

We cannot have both

$$
H=a_iG_i
\qquad\text{and}\qquad
X_i=b_{ij}G_j
$$

for suitable functions $a_i$ and $b_{ij}$. For, from (2),

$$
\begin{aligned}
[H,G_j]
 &=[a_iG_i,G_j]\\
 &=a_i[G_i,G_j]-(G_ja_i)G_i\\
 &=-(G_ja_i)G_i\\
 &=0,
\end{aligned}
$$

and, from (4),

$$
\begin{aligned}
[G_i,X_j]
 &=[G_i,b_{jk}G_k]\\
 &=[G_i,G_k]b_{jk}+(G_ib_{jk})G_k\\
 &=(G_ib_{jk})G_k\\
 &=0.
\end{aligned}
$$

Then (5) is

$$
\begin{aligned}
[H,X_j]
 &=[a_iG_i,b_{jk}G_k]\\
 &=a_i[G_i,G_k]b_{jk}
   +a_i(G_ib_{jk})G_k
   -b_{jk}(G_ka_i)G_i\\
 &=0,
\end{aligned}
$$

which is a contradiction.

We assume that $H\ne a_iG_i$. Then there exist coordinates $x,u_i$ such that

$$
H=\frac{\partial}{\partial x},
\qquad
G_j=C_{ij}\frac{\partial}{\partial u_i}.
$$

Since

$$
[H,G_j]
=\frac{\partial C_{ij}}{\partial x}
 \frac{\partial}{\partial u_i}=0,
$$

it follows that

$$
\frac{\partial C_{ij}}{\partial x}=0,
$$

so $C_{ij}=C_{ij}(\mathbf u)$. Since the $G_i$ are linearly independent, the
matrix $[C_{ij}]$ has rank $n$, and so its inverse exists, perhaps restricted
to a subset of $\mathbb R^n$.


If we change variables from $u_i$ to $v_i$ so that

$$
v_i=d_i(\mathbf u),
$$

then

$$
\begin{aligned}
G_i
 &=C_{ik}\frac{\partial}{\partial u_k}\\
 &=C_{ik}\frac{\partial d_j}{\partial u_k}
   \frac{\partial}{\partial v_j}\\
 &=\frac{\partial}{\partial v_i},
\end{aligned}
$$

provided

$$
C_{ik}\frac{\partial d_j}{\partial u_k}=\delta_{ij},
$$

<a id="jacobian"></a>
i.e. the Jacobian of the transformation is the inverse of $[C_{ij}]$. Thus
the transformation is not degenerate. Can functions $d_j(\mathbf u)$ exist
with the property

$$
\frac{\partial d_i}{\partial u_j}=(C^{-1})_{ij},
\qquad C=[C_{ij}]?
$$

The requirement of consistency, if the integration is to be carried out, is

$$
\frac{\partial}{\partial u_k}\frac{\partial d_i}{\partial u_j}
=\frac{\partial}{\partial u_j}\frac{\partial d_i}{\partial u_k}.
$$

Now

$$
\frac{\partial C^{-1}}{\partial u_i}
=-C^{-1}\frac{\partial C}{\partial u_i}C^{-1}.
$$

The differentiability of $C^{-1}$ depends upon the differentiability of $C$.
Since we need $C''$ in $G^{(2)}$, $C$ must be twice differentiable almost
everywhere, and so $d_i$ is three times differentiable and the mixed
derivatives are equal. Thus a transformation exists which casts $G_i$ in the
form

$$
G_i=\frac{\partial}{\partial u_i}.
$$

Had

$$
X_i=b_{ij}\frac{\partial}{\partial u_j},
$$

then

$$
\begin{aligned}
[G_i,X_j]
 &=\left[\frac{\partial}{\partial u_i},
          b_{jk}\frac{\partial}{\partial u_k}\right]\\
 &=\frac{\partial b_{jk}}{\partial u_i}
   \frac{\partial}{\partial u_k}=0.
\end{aligned}
$$

Hence

$$
\frac{\partial b_{jk}}{\partial u_i}=0,
\qquad
b_{jk}=B_{jk}(x).
$$


From (5),

$$
\begin{aligned}
[H,X_i]
 &=\left[\frac{\partial}{\partial x},
          B_{ij}\frac{\partial}{\partial u_j}\right]\\
 &=\frac{dB_{ij}}{dx}\frac{\partial}{\partial u_j}
  =\frac{\partial}{\partial u_i}.
\end{aligned}
$$

Therefore

$$
B'_{ij}=\delta_{ij},
\qquad
B_{ij}(x)=\delta_{ij}x+K_{ij}.
$$

Thus

$$
X_i=x\frac{\partial}{\partial u_i}
    +K_{ij}\frac{\partial}{\partial u_j}
   =x\frac{\partial}{\partial u_i}+K_{ij}G_j.
$$

Drop the second term and take

$$
X_i=x\frac{\partial}{\partial u_i}.
$$

We have the standard form

$$
H=\frac{\partial}{\partial x},
\qquad
G_i=\frac{\partial}{\partial u_i},
\qquad
X_i=x\frac{\partial}{\partial u_i}.
$$

In the coordinates $(x,u_i)$, the differential equation is

$$
A\mathbf u''=\mathbf f(x,\mathbf u,\mathbf u').
$$

Invariance under $H$, $G_i$, and $X_i$ implies, respectively,

$$
\frac{\partial\mathbf f}{\partial x}=0,
\qquad
\frac{\partial\mathbf f}{\partial\mathbf u}=0,
\qquad
\frac{\partial\mathbf f}{\partial\mathbf u'}=0.
$$

For example, consider

$$
u''+\frac{u(v')^2}{v^2}=0,
$$

$$
v''+\frac{2u'v'}{u}-\frac{(v')^2}{v}=0.
$$

Let

$$
G=\tau(u,v,x)\frac{\partial}{\partial x}
 +\xi(u,v,x)\frac{\partial}{\partial u}
 +\eta(u,v,x)\frac{\partial}{\partial v}.
$$

Then

$$
\begin{aligned}
G^{(2)}={}&G
 +(\xi'-u'\tau')\frac{\partial}{\partial u'}
 +(\eta'-v'\tau')\frac{\partial}{\partial v'}\\
&+(\xi''-u'\tau''-2u''\tau')\frac{\partial}{\partial u''}
 +(\eta''-v'\tau''-2v''\tau')\frac{\partial}{\partial v''}.
\end{aligned}
$$

For example,

$$
\eta'=\frac{\partial\eta}{\partial x}
      +u'\frac{\partial\eta}{\partial u}
      +v'\frac{\partial\eta}{\partial v},
$$

and

$$
\begin{aligned}
\eta''={}&
 \frac{\partial^2\eta}{\partial x^2}
 +2u'\frac{\partial^2\eta}{\partial u\partial x}
 +2v'\frac{\partial^2\eta}{\partial v\partial x}
 +(u')^2\frac{\partial^2\eta}{\partial u^2}\\
&+2u'v'\frac{\partial^2\eta}{\partial u\partial v}
 +(v')^2\frac{\partial^2\eta}{\partial v^2}
 +u''\frac{\partial\eta}{\partial u}
 +v''\frac{\partial\eta}{\partial v}.
\end{aligned}
$$


The action of $G^{(2)}$ on the two equations, followed by separation by powers
of $u'$ and $v'$, leads to a system of fifteen independent partial differential
equations for $\tau$, $\xi$, and $\eta$, all linear. The solutions give

$$
\begin{aligned}
G_1={}&x\frac{\partial}{\partial x}
      +2u\frac{\partial}{\partial u}
      +2v\frac{\partial}{\partial v},\\
G_2={}&\frac{\partial}{\partial x},\\
G_3={}&\frac1v\frac{\partial}{\partial u}
      +\frac1u\frac{\partial}{\partial v},\\
G_4={}&x\frac{\partial}{\partial x},\\
G_5={}&x^2\frac{\partial}{\partial x}
      +2xu\frac{\partial}{\partial u}
      +2xv\frac{\partial}{\partial v},\\
G_6={}&\frac{x}{v}\frac{\partial}{\partial u}
      +\frac{x}{u}\frac{\partial}{\partial v},\\
G_7={}&xuv\frac{\partial}{\partial x}
      +2u^2v\frac{\partial}{\partial u}
      +2uv^2\frac{\partial}{\partial v},\\
G_8={}&uv\frac{\partial}{\partial x},\\
G_9={}&x\frac{\partial}{\partial x}
      +2u\frac{\partial}{\partial u}
      -2v\frac{\partial}{\partial v},\\
G_{10}={}&v\frac{\partial}{\partial u}
          -\frac{v^2}{u}\frac{\partial}{\partial v},\\
G_{11}={}&x^2\frac{\partial}{\partial x}
          +2ux\frac{\partial}{\partial u}
          -2vx\frac{\partial}{\partial v},\\
G_{12}={}&vx\frac{\partial}{\partial u}
          -\frac{v^2x}{u}\frac{\partial}{\partial v},\\
G_{13}={}&\frac{xv}{u}\frac{\partial}{\partial x}
          +\frac{2u^2}{v}\frac{\partial}{\partial u}
          -2u\frac{\partial}{\partial v},\\
G_{14}={}&\frac{u}{v}\frac{\partial}{\partial x},\\
G_{15}={}&u\left(v^2+\frac1{v^2}\right)
             \frac{\partial}{\partial u}
          -v\left(u^2-\frac1{v^2}\right)
             \frac{\partial}{\partial v}.
\end{aligned}
$$

The system is obviously linearisable, as the fifteen-element algebra is
$\mathfrak{sl}(4,\mathbb R)$, the algebra of the two-dimensional free particle
with equation of motion

<a id="sl4r-symmetry"></a>

$$
\mathbf r''=0,
\qquad
\mathbf r\in\mathbb R^2.
$$

Note that $\mathfrak{sl}(4,\mathbb R)$ is not a prerequisite for
linearisation. This is because the system

$$
\mathbf r''=A\mathbf r,
\qquad
\mathbf r\in\mathbb R^2,
$$

need not possess $\mathfrak{sl}(4,\mathbb R)$ symmetry.


From the symmetries, one looks to see if the algebra $\mathcal N$ is present.
Recall that $\mathcal N$ has $H,G_i,X_i$ such that

$$
[H,G_i]=0,
\qquad
[G_i,G_j]=0,
$$

and

$$
[G_i,X_j]=0,
\qquad
[H,X_i]=G_i.
$$

In standard form,

$$
H=\frac{\partial}{\partial x},
\qquad
G_i=\frac{\partial}{\partial u_i},
\qquad
X_i=x\frac{\partial}{\partial u_i}.
$$

Noting that

$$
[G_2,G_3]=0,
\qquad
[G_2,G_6]=G_3,
\qquad
[G_3,G_6]=0,
$$

and

$$
[G_2,G_{10}]=0,
\qquad
[G_2,G_{12}]=G_{10},
\qquad
[G_{10},G_{12}]=0,
$$

we check to see if this is the algebra $\mathcal N$:

$$
\begin{aligned}
[G_3,G_{10}]
 &=\left[
   \frac1v\frac{\partial}{\partial u}
   +\frac1u\frac{\partial}{\partial v},
   v\frac{\partial}{\partial u}
   -\frac{v^2}{u}\frac{\partial}{\partial v}
   \right]\\
 &=\left(\frac1u-\frac1u\right)\frac{\partial}{\partial u}
  +\left(\frac{v}{u^2}-\frac{2v}{u^2}+\frac{v}{u^2}\right)
   \frac{\partial}{\partial v}\\
 &=0.
\end{aligned}
$$

Likewise,

$$
[G_6,G_{10}]=0,
\qquad
[G_6,G_{12}]=0,
\qquad
[G_3,G_{12}]=0.
$$

Thus we do have $\mathcal N$. Identify

$$
\begin{aligned}
H&=\frac{\partial}{\partial x},\\
G_1&=\frac1v\frac{\partial}{\partial u}
    +\frac1u\frac{\partial}{\partial v},\\
G_2&=v\frac{\partial}{\partial u}
    -\frac{v^2}{u}\frac{\partial}{\partial v},\\
X_1&=xG_1,\\
X_2&=xG_2.
\end{aligned}
$$

Seek a transformation from $x,u,v$ to $X,U,V$ such that

$$
H=\frac{\partial}{\partial X},
\qquad
G_1=\frac{\partial}{\partial U},
\qquad
G_2=\frac{\partial}{\partial V},
$$

$$
X_1=X\frac{\partial}{\partial U},
\qquad
X_2=X\frac{\partial}{\partial V}.
$$

Let

$$
X=x,
\qquad
U=F(u,v),
\qquad
V=G(u,v),
$$

having already realised that $x$ is an appropriate variable.


The derivatives transform according to

$$
\frac{\partial}{\partial u}
\longrightarrow
\frac{\partial F}{\partial u}\frac{\partial}{\partial U}
+\frac{\partial G}{\partial u}\frac{\partial}{\partial V},
$$

and

$$
\frac{\partial}{\partial v}
\longrightarrow
\frac{\partial F}{\partial v}\frac{\partial}{\partial U}
+\frac{\partial G}{\partial v}\frac{\partial}{\partial V}.
$$

Thus $G_1$ gives

$$
\frac1v\frac{\partial F}{\partial u}
+\frac1u\frac{\partial F}{\partial v}=1,
\tag{i}
$$

and

$$
\frac1v\frac{\partial G}{\partial u}
+\frac1u\frac{\partial G}{\partial v}=0.
\tag{ii}
$$

From $G_2$ we obtain

$$
v\frac{\partial F}{\partial u}
-\frac{v^2}{u}\frac{\partial F}{\partial v}=0,
\tag{iii}
$$

and

$$
v\frac{\partial G}{\partial u}
-\frac{v^2}{u}\frac{\partial G}{\partial v}=1.
\tag{iv}
$$

For (ii),

$$
\frac{du}{1/v}=\frac{dv}{1/u},
\qquad
w_1=\frac{u}{v},
$$

so

$$
G=g\left(\frac uv\right).
$$

In (iv),

$$
v\frac1v g'
-\frac{v^2}{u}\left(-\frac{u}{v^2}\right)g'=1,
$$

so

$$
g'=\frac12,
$$

and

$$
G=\frac12\frac uv,
$$

to within a forgettable constant.

For (i),

$$
\frac{du}{1/v}=\frac{dv}{1/u}=\frac{dF}{1}.
$$

The first two terms give

$$
w_1=\frac uv.
$$

The first and third terms give

$$
u\,du=dF.
$$

Hence

$$
w_2=\frac12uv-F,
$$

and

$$
F=\frac12uv+f\left(\frac uv\right).
$$

In (iii),

$$
v\left(\frac12v+\frac1v f'\right)
-\frac{v^2}{u}\left(\frac12u-\frac{u}{v^2}f'\right)=0.
$$

Therefore

$$
2f'=0,
$$

so $f'=0$, to within a forgettable constant, and

$$
F=\frac12uv.
$$

Thus

$$
V=\frac12\frac uv,
\qquad
U=\frac12uv.
$$


$$
\begin{aligned}
U'
 &=\frac12u'v+\frac12uv',\\[4pt]
U''
 &=\frac12u''v+u'v'+\frac12uv''\\
 &=\frac12\left[-\frac{u(v')^2}{v^2}\right]v+u'v'
   +\frac12u\left[
      \frac{(v')^2}{v}-\frac{2u'v'}u
    \right]\\
 &=0,\\[4pt]
V''
 &=0.
\end{aligned}
$$

Therefore

$$
\begin{aligned}
U &=A_0+A_1x,\\
V &=B_0+B_1x,
\end{aligned}
$$

giving

$$
\begin{aligned}
u^2 &=4(A_0+A_1x)(B_0+B_1x),\\[4pt]
v^2 &=\frac{A_0+A_1x}{B_0+B_1x},
\end{aligned}
$$

which is the general solution since there are four arbitrary constants.

---

<nav aria-label="Section navigation" style="display: grid; grid-template-columns: minmax(0, 1fr) auto; column-gap: 2em; align-items: start;">
<div style="display: grid; grid-template-columns: 6em minmax(0, 1fr); row-gap: 0.25em;">
<span>NEXT:</span><a href="03-lie-theory-of-extended-group.md">Lie Theory of Extended Group</a>
<span>PREVIOUS:</span><a href="01-contents.md">Contents</a>
</div>
<a href="05-index.md" style="justify-self: end; text-align: right;">INDEX</a>
</nav>
