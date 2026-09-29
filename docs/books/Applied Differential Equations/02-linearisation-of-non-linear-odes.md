<a id="linearisation-of-non-linear-differential-equations"></a>
## Linearisation of non-linear differential equations

Confine our attention to $2^\mathrm{nd}$ order ODEs

Consider

$\quad\begin{array}{l}\displaystyle y''+3yy'+y^3=0\end{array}$

under appropriate constraints on domain, linear equations
have two linearly independent solutions

$\quad\begin{array}{l}\displaystyle Ly=0 \qquad L\text{ }2^\mathrm{nd}\text{ order linear operator}\end{array}$

$\quad\begin{array}{l}\displaystyle \{y=y_1(x),y_2(x)\}\end{array}$

$\quad\begin{array}{l}\displaystyle y''+xy=0\end{array}$

$\quad\begin{array}{l}\displaystyle \begin{aligned} y_1(0)&=1, & y_2(0)&=0,\\ y_1'(0)&=0, & y_2'(0)&=1 \end{aligned}\end{array}$

$\quad\begin{array}{l}\displaystyle y_1(x) \qquad y_2(x)\end{array}$

$\quad\begin{array}{l}\displaystyle y(x)=a_1y_1(x)+a_2y_2(x)\end{array}$

Two equations are equivalent if they possess the same Lie algebra
of symmetries i.e. there exists a point transformation which
transforms one equation to the other.

<a id="point-transformation"></a>
### Point transformation

$\quad\begin{array}{l}\displaystyle x,y\longmapsto X,Y, \qquad X=F(x,y);\quad Y=G(x,y)\end{array}$

other transforms depend upon the derivatives e.g. contact

$\quad\begin{array}{l}\displaystyle x,y\longmapsto X,Y, \qquad X=F(x,y,y');\quad Y=G(x,y,y')\end{array}$

<a id="lie-algebra"></a>
### Lie algebra

$\{G_i,\ i=1,\ldots,n\}$ forms a Lie algebra if

$\quad\begin{array}{l}\displaystyle [G_i,G_j]=C_{ijk}G_k\end{array}$

where the $C_{ijk}$ are constants (called structure constants)
and $[\ ,\ ]$ is a skew symmetric operator

$\quad\begin{array}{l}\displaystyle [G_i,G_j]=G_iG_j-G_jG_i\end{array}$

so that it is obvious that

$\quad\begin{array}{l}\displaystyle [G_i,G_j]=-[G_j,G_i]\end{array}$

Note: the structure constants are invariant under a point
transformation. i.e. the algebra is independent of the co-ordinate
representation being used.


<a id="infinitesimal-point-transformations"></a>
In general a symmetry exists if a "system" is invariant under the
transformation induced by the symmetry. An infinitessimal point transformation of
$x,y$ to $\bar{x},\bar{y}$ is represented by

$\quad\begin{array}{l}\displaystyle \bar x=x+\varepsilon\xi(x,y), \qquad \bar y=y+\varepsilon\eta(x,y),\end{array}$

where $\varepsilon$ is the infinitesimal parameter. For $f(x,y)$ an
infinitesimal point transformation produces

$\quad\begin{array}{l}\displaystyle \begin{aligned} f(\bar x,\bar y) &=f\bigl(x+\varepsilon\xi,y+\varepsilon\eta\bigr)\\ &=f(x,y)+\varepsilon \left(\xi\frac{\partial f}{\partial x} +\eta\frac{\partial f}{\partial y}\right)+O(\varepsilon^2)\\ &=f(x,y)+\varepsilon Gf. \end{aligned}\end{array}$

where

$\quad\begin{array}{l}\displaystyle G=\xi\frac{\partial}{\partial x} +\eta\frac{\partial}{\partial y}\end{array}$

is the generator of the infinitesimal point transformation.

$\quad\begin{array}{l}\displaystyle \left\{ G_i=\xi_i\frac{\partial}{\partial x} +\eta_i\frac{\partial}{\partial y}, \quad i=1,2,\ldots,n \right\}\end{array}$

constitutes a Lie algebra if

$\quad\begin{array}{l}\displaystyle \begin{aligned} [G_i,G_j] &=\left[ \xi_i\frac{\partial}{\partial x} +\eta_i\frac{\partial}{\partial y}, \xi_j\frac{\partial}{\partial x} +\eta_j\frac{\partial}{\partial y} \right]\\ &=\left( \xi_i\frac{\partial\xi_j}{\partial x} +\eta_i\frac{\partial\xi_j}{\partial y} -\xi_j\frac{\partial\xi_i}{\partial x} -\eta_j\frac{\partial\xi_i}{\partial y} \right)\frac{\partial}{\partial x}\\ &\quad+\left( \xi_i\frac{\partial\eta_j}{\partial x} +\eta_i\frac{\partial\eta_j}{\partial y} -\xi_j\frac{\partial\eta_i}{\partial x} -\eta_j\frac{\partial\eta_i}{\partial y} \right)\frac{\partial}{\partial y}\\ &=C_{ijk}\left( \xi_k\frac{\partial}{\partial x} +\eta_k\frac{\partial}{\partial y} \right). \end{aligned}\end{array}$

<a id="how-to-deal-with-derivatives"></a>
### How to deal with $f(x,y,y')$ and $f(x,y,y',y'')$

How do $y',y'',\ldots$ transform?

$\quad\begin{array}{l}\displaystyle \begin{aligned} \frac{d\bar y}{d\bar x} &=\frac{d(y+\varepsilon\eta)}{d(x+\varepsilon\xi)}\\ &=\frac{\dfrac{dy}{dx}+\varepsilon\dfrac{d\eta}{dx}} {1+\varepsilon\dfrac{d\xi}{dx}}\\ &=\frac{y'+\varepsilon\eta'}{1+\varepsilon\xi'}\\ &=y'+\varepsilon(\eta'-y'\xi') \qquad\text{to }1^\mathrm{st}\text{ order in }\varepsilon.\\[1.5ex] \frac{d^2\bar y}{d\bar x^2} &=\frac{d}{d\bar x}\left(\frac{d\bar y}{d\bar x}\right)\\ &=\frac{d\left[y'+\varepsilon(\eta'-y'\xi')\right]} {d(x+\varepsilon\xi)}\\ &=\frac{y''+\varepsilon(\eta''-y''\xi'-y'\xi'')} {1+\varepsilon\xi'}\\ &=y''+\varepsilon(\eta''-2y''\xi'-y'\xi''). \end{aligned}\end{array}$


<a id="derivative-notes"></a>
#### Notes

<a id="prolongation"></a>
1. The process may be extended to higher order derivatives. There
   is a recursion relation. If

   $\quad\begin{array}{l}\displaystyle G=\xi\frac{\partial}{\partial x} +\eta\frac{\partial}{\partial y},\end{array}$

   then

   $\quad\begin{array}{l}\displaystyle G^{(1)}=G+(\eta'-y'\xi')\frac{\partial}{\partial y'},\end{array}$

   $\quad\begin{array}{l}\displaystyle G^{(2)}=G^{(1)}+(\eta''-2y''\xi'-y'\xi'') \frac{\partial}{\partial y''}.\end{array}$

   Let

   $\quad\begin{array}{l}\displaystyle H^{(1)}=\eta'-y'\xi'.\end{array}$

   Then

   $\quad\begin{array}{l}\displaystyle G^{(1)}=G+H^{(1)}\frac{\partial}{\partial y'},\end{array}$

   $\quad\begin{array}{l}\displaystyle G^{(n)}=G^{(n-1)}+H^{(n)}\frac{\partial}{\partial y^{(n)}}.\end{array}$

   $\quad\begin{array}{l}\displaystyle \begin{aligned} H^{(2)}&=(H^{(1)})'-y''\xi',\\ H^{(3)}&=(H^{(2)})'-y'''\xi'. \end{aligned}\end{array}$

   $\quad\begin{array}{l}\displaystyle \vdots\end{array}$

2. The process may be extended to systems of equations
   with independent variables and dependent variables.

<a id="symmetries-of-a-second-order-ode"></a>
### Symmetries of a $2^\mathrm{nd}$ order ODE

$\quad\begin{array}{l}\displaystyle N(x,y,y',y'')=0,\end{array}$

$\quad\begin{array}{l}\displaystyle G=\xi\frac{\partial}{\partial x} +\eta\frac{\partial}{\partial y},\end{array}$

with

$\quad\begin{array}{l}\displaystyle \xi=\xi(x,y), \qquad \eta=\eta(x,y)\end{array}$

(i.e. point transformations), is the generator of a symmetry (i) if

$\quad\begin{array}{l}\displaystyle G^{(2)}N=0 \qquad\text{when }N=0,\end{array}$

where

$\quad\begin{array}{l}\displaystyle G^{(2)} =\xi\frac{\partial}{\partial x} +\eta\frac{\partial}{\partial y} +(\eta'-y'\xi')\frac{\partial}{\partial y'} +(\eta''-2y''\xi'-y'\xi'')\frac{\partial}{\partial y''}.\end{array}$

Example: $y''=0$

$\quad\begin{array}{l}\displaystyle G^{(2)}y''=\eta''-2y''\xi'-y'\xi''\end{array}$

$\quad\begin{array}{l}\displaystyle \displaystyle G^{(2)}y''=0:\end{array}$

$\quad\begin{array}{l}\displaystyle \frac{\partial^2\eta}{\partial x^2} +2y'\frac{\partial^2\eta}{\partial x\partial y} +(y')^2\frac{\partial^2\eta}{\partial y^2} +y''\frac{\partial\eta}{\partial y} -y'\left( \frac{\partial^2\xi}{\partial x^2} +2y'\frac{\partial^2\xi}{\partial x\partial y} +(y')^2\frac{\partial^2\xi}{\partial y^2} +y''\frac{\partial\xi}{\partial y} \right)=0.\end{array}$

Since $\xi(x,y)$ and $\eta(x,y)$ are independent of $y'$, equate
coefficients of independent powers of $y'$ to zero.


$\quad\begin{array}{l}\displaystyle \begin{aligned} (y')^3 &: -\frac{\partial^2\xi}{\partial y^2}=0 &&\text{(ii)}\\[1ex] (y')^2 &: \frac{\partial^2\eta}{\partial y^2} -2\frac{\partial^2\xi}{\partial x\partial y}=0 &&\text{(iii)}\\[1ex] y' &: 2\frac{\partial^2\eta}{\partial x\partial y} -\frac{\partial^2\xi}{\partial x^2}=0 &&\text{(iv)}\\[1ex] (y')^0 &: \frac{\partial^2\eta}{\partial x^2}=0 &&\text{(v)} \end{aligned}\end{array}$

$\quad\begin{array}{l}\displaystyle \text{(ii)}\qquad \xi=a(x)y+b(x).\end{array}$

$\quad\begin{array}{l}\displaystyle \text{(iii)}\qquad \frac{\partial^2\eta}{\partial y^2}=2a',\end{array}$

$\quad\begin{array}{l}\displaystyle \eta=a'y^2+c(x)y+d(x).\end{array}$

$\quad\begin{array}{l}\displaystyle \text{(iv)}\qquad 4a''y+2c'-a''y-b''=0. \qquad\text{(vi)}\end{array}$

$\quad\begin{array}{l}\displaystyle \text{(v)}\qquad a'''y^2+c''y+d''=0. \qquad\text{(vii)}\end{array}$

In (vi) and (vii) equate coefficients of independent powers of $y$ to zero:

$\quad\begin{array}{l}\displaystyle \begin{aligned} y^2 &: a'''=0,\\ y^1 &: c''=0,\\ &\phantom{:}\ a''=0,\\ y^0 &: d''=0,\\ &\phantom{:}\ b''=2c'. \end{aligned}\end{array}$

$\quad\begin{array}{l}\displaystyle \begin{aligned} a&=A_0+A_1x,\\ b&=B_0+B_1x+C_1x^2,\\ c&=C_0+C_1x,\\ d&=D_0+D_1x. \end{aligned}\end{array}$

$\quad\begin{array}{l}\displaystyle \begin{aligned} \xi&=(A_0+A_1x)y+B_0+B_1x+C_1x^2,\\ \eta&=A_1y^2+(C_0+C_1x)y+D_0+D_1x. \end{aligned}\end{array}$

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

$\quad\begin{array}{l}\displaystyle \begin{aligned} G_1&=\frac{\partial}{\partial x} &&: B_0,\\[6pt] G_2&=x\frac{\partial}{\partial x} &&: B_1,\\[6pt] G_3&=x^2\frac{\partial}{\partial x} +xy\frac{\partial}{\partial y} &&: C_1,\\[6pt] G_4&=y\frac{\partial}{\partial x} &&: A_0,\\[6pt] G_5&=xy\frac{\partial}{\partial x} +y^2\frac{\partial}{\partial y} &&: A_1,\\[6pt] G_6&=\frac{\partial}{\partial y} &&: D_0,\\[6pt] G_7&=x\frac{\partial}{\partial y} &&: D_1,\\[6pt] G_8&=y\frac{\partial}{\partial y} &&: C_0. \end{aligned}\end{array}$

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

$\quad\begin{array}{l}\displaystyle \begin{aligned} [G_1,G_2] &=\left[ \frac{\partial}{\partial x}, x\frac{\partial}{\partial x} \right]\\ &=\frac{\partial}{\partial x} \left(x\frac{\partial}{\partial x}\right) -x\frac{\partial}{\partial x} \left(\frac{\partial}{\partial x}\right)\\ &=\frac{\partial}{\partial x} +x\frac{\partial^2}{\partial x^2} -x\frac{\partial^2}{\partial x^2}\\ &=G_1. \end{aligned}\end{array}$


<a id="symmetries-and-first-integrals"></a>
### Relationship between symmetries and first integrals

If $I=I(x,y,y')$ has the property that

$\quad\begin{array}{l}\displaystyle \left.\frac{dI}{dx}\right|_{N(x,y,y',y'')=0}=0,\end{array}$

then $I(x,y,y')$ is a first integral of

$\quad\begin{array}{l}\displaystyle N(x,y,y',y'')=0.\end{array}$

Suppose $G$ is a symmetry of $N(x,y,y',y'')=0$. Then the first integral
associated with $G$ is $I(x,y,y')$:

$\quad\begin{array}{l}\displaystyle G^{(1)}I(x,y,y')=0 \quad\Longrightarrow\quad dI\bigl(G^{(1)}\bigr)=0 \quad\text{for surface }N=0\end{array}$

$\quad\begin{array}{l}\displaystyle \Longrightarrow \left.\frac{dI}{dx}\right|_{N=0}=0.\end{array}$

For $y''=0$, consider

$\quad\begin{array}{l}\displaystyle G_7=x\frac{\partial}{\partial y}.\end{array}$

$\quad\begin{array}{l}\displaystyle \begin{aligned} G&=\xi\frac{\partial}{\partial x} +\eta\frac{\partial}{\partial y},\\ G^{(1)}&=\xi\frac{\partial}{\partial x} +\eta\frac{\partial}{\partial y} +(\eta'-y'\xi')\frac{\partial}{\partial y'}. \end{aligned}\end{array}$

$\quad\begin{array}{l}\displaystyle G_7^{(1)}=x\frac{\partial}{\partial y} +\frac{\partial}{\partial y'}.\end{array}$

$\quad\begin{array}{l}\displaystyle G_7^{(1)}I =0\frac{\partial I}{\partial x} +x\frac{\partial I}{\partial y} +\frac{\partial I}{\partial y'} =0.\end{array}$

<a id="characteristics"></a>
The equations for the characteristics are

$\quad\begin{array}{l}\displaystyle \frac{dx}{0}=\frac{dy}{x}=\frac{dy'}{1}.\end{array}$

$1^\mathrm{st}$ and $2^\mathrm{nd}$:

$\quad\begin{array}{l}\displaystyle \begin{aligned} x\,dx-0\,dy&=0,\\ \frac{1}{2}x^2&=\text{constant},\\ u&=x \qquad \left(\text{allowed to take any function of }\frac{1}{2}x^2\right). \end{aligned}\end{array}$

$2^\mathrm{nd}$ and $3^\mathrm{rd}$:

$\quad\begin{array}{l}\displaystyle \begin{aligned} dy-x\,dy'&=0,\\ dy-u\,dy'&=0,\\ v&=y-uy'=y-xy'. \end{aligned}\end{array}$

i.e.

$\quad\begin{array}{l}\displaystyle I(u,v)=I(x,y-xy').\end{array}$

Imposing the second condition, i.e.

$\quad\begin{array}{l}\displaystyle \left.\frac{dI}{dx}\right|_{N=0}=0,\end{array}$

$\quad\begin{array}{l}\displaystyle \frac{\partial I}{\partial u}u' +\frac{\partial I}{\partial v}v'=0,\end{array}$

for which the characteristic is found from

$\quad\begin{array}{l}\displaystyle \frac{du}{u'}=\frac{dv}{v'}.\end{array}$


$\quad\begin{array}{l}\displaystyle \begin{aligned} \frac{dv}{du} &=\frac{v'}{u'}\\ &=\frac{y'-xy''-y'}{1}=0, \qquad v=\text{constant}. \end{aligned}\end{array}$

$\quad\begin{array}{l}\displaystyle I=f(y-xy').\end{array}$

usually write

$\quad\begin{array}{l}\displaystyle I=y-xy'.\end{array}$

$\quad\begin{array}{l}\displaystyle G_3=x^2\frac{\partial}{\partial x} +xy\frac{\partial}{\partial y}.\end{array}$

$\quad\begin{array}{l}\displaystyle G_3^{(1)}I =x^2\frac{\partial I}{\partial x} +xy\frac{\partial I}{\partial y} +(y-xy')\frac{\partial I}{\partial y'} =0.\end{array}$

Equations for the characteristics are

$\quad\begin{array}{l}\displaystyle \frac{dx}{x^2} =\frac{dy}{xy} =\frac{dy'}{y-xy'}.\end{array}$

$1^\mathrm{st}$ and $2^\mathrm{nd}$:

$\quad\begin{array}{l}\displaystyle \frac{dx}{x^2}=\frac{dy}{xy} \quad\Longrightarrow\quad \frac{dy}{y}-\frac{dx}{x}=0.\end{array}$

$\quad\begin{array}{l}\displaystyle \left(\log y-\log x=\log u\right),\end{array}$

$\quad\begin{array}{l}\displaystyle u=\frac{y}{x}.\end{array}$

$2^\mathrm{nd}$ and $3^\mathrm{rd}$:

$\quad\begin{array}{l}\displaystyle \frac{dy}{xy}=\frac{dy'}{y-xy'}.\end{array}$

$\quad\begin{array}{l}\displaystyle x=\frac{y}{u}.\end{array}$

$\quad\begin{array}{l}\displaystyle \frac{dy}{y^2/u} =\frac{dy'}{y-yy'/u}.\end{array}$

$\quad\begin{array}{l}\displaystyle \frac{dy'}{dy} =\frac{y-\dfrac{yy'}{u}}{y^2/u} =\frac{u-y'}{y}.\end{array}$

$\quad\begin{array}{l}\displaystyle \frac{dy}{y}=\frac{dy'}{u-y'}.\end{array}$

$\quad\begin{array}{l}\displaystyle \begin{aligned} v&=yy'-uy\\ &=yy'-\frac{y^2}{x}. \end{aligned}\end{array}$

$\quad\begin{array}{l}\displaystyle I(u,v)=I\left(\frac{y}{x},yy'-\frac{y^2}{x}\right).\end{array}$

from

$\quad\begin{array}{l}\displaystyle \frac{\partial I}{\partial u}u' +\frac{\partial I}{\partial v}v'=0,\end{array}$

$\quad\begin{array}{l}\displaystyle \frac{dv}{du} =\frac{v'}{u'} =\frac{ (y')^2+yy''-\dfrac{2y'y}{x}+\dfrac{y^2}{x^2} }{ \dfrac{y'}{x}-\dfrac{y}{x^2} }.\end{array}$


$\quad\begin{array}{l}\displaystyle \begin{aligned} &=\frac{ \left(y'-\dfrac{y}{x}\right)^2 }{ \dfrac{1}{x}\left(y'-\dfrac{y}{x}\right) }\\ &=x\left(y'-\frac{y}{x}\right)\\ &=x\frac{v}{y}\\ &=\frac{v}{u}. \end{aligned}\end{array}$

$\quad\begin{array}{l}\displaystyle \frac{dv}{du}=\frac{v}{u} \quad\Longrightarrow\quad w=\frac{v}{u}.\end{array}$

$\quad\begin{array}{l}\displaystyle \begin{aligned} I &=f\left(\frac{v}{u}\right)\\ &=f\left( \frac{yy'-y^2/x}{y/x} \right)\\ &=f(xy'-y). \end{aligned}\end{array}$

$\quad\begin{array}{l}\displaystyle G_1=\frac{\partial}{\partial x}.\end{array}$

$\quad\begin{array}{l}\displaystyle G_1^{(1)}I =\frac{\partial I}{\partial x} +0\frac{\partial I}{\partial y} +0\frac{\partial I}{\partial y'} =0.\end{array}$

$\quad\begin{array}{l}\displaystyle \frac{dx}{1}=\frac{dy}{0}=\frac{dy'}{0}.\end{array}$

$1$ and $2$:

$\quad\begin{array}{l}\displaystyle dy-0\,dx=0, \qquad u=y.\end{array}$

$1$ and $3$:

$\quad\begin{array}{l}\displaystyle dy'-0\,dx=0, \qquad v=y'.\end{array}$

$\quad\begin{array}{l}\displaystyle \frac{dv}{du} =\frac{v'}{u'} =\frac{y''}{y'} =0.\end{array}$

$\quad\begin{array}{l}\displaystyle w=v.\end{array}$

$\quad\begin{array}{l}\displaystyle I=f(v)=f(y').\end{array}$

other form is

$\quad\begin{array}{l}\displaystyle \begin{aligned} I &=f\left(\frac{xy'-y}{y'}\right)\\ &=f\left(x-\frac{y}{y'}\right). \end{aligned}\end{array}$

$\quad\begin{array}{l}\displaystyle I=x-\frac{y}{y'}\end{array}$

is called a resonance integral.

Characteristic structure of a resonance integral is

$\quad\begin{array}{l}\displaystyle I=a(x)+\frac{b(xy)}{y'-c(x,y)}.\end{array}$


Consider

$\quad\begin{array}{l}\displaystyle y''+3yy'+y^3=0.\end{array}$

(Comes from the study of the modified Emden equation

$\quad\begin{array}{l}\displaystyle \ddot q+\alpha(t)\dot q+q^n=0,\end{array}$

which arises in the study of polytropes, i.e. gaseous spheres (models of
stars)).

Let

$\quad\begin{array}{l}\displaystyle G=\xi(x,y)\frac{\partial}{\partial x} +\eta(x,y)\frac{\partial}{\partial y}.\end{array}$

Then

$\quad\begin{array}{l}\displaystyle G^{(2)}\left(y''+3yy'+y^3=0\right)\end{array}$

is

$\quad\begin{array}{l}\displaystyle (\eta''-2y''\xi'-y'\xi'') +3\eta y' +3y(\eta'-y'\xi') +3y^2\eta=0.\end{array}$

$\quad\begin{array}{l}\displaystyle \begin{aligned} &\frac{\partial^2\eta}{\partial x^2} +2y'\frac{\partial^2\eta}{\partial x\partial y} +(y')^2\frac{\partial^2\eta}{\partial y^2} -(3yy'+y^3)\frac{\partial\eta}{\partial y}\\ &\quad +2(3yy'+y^3) \left( \frac{\partial\xi}{\partial x} +y'\frac{\partial\xi}{\partial y} \right)\\ &\quad -y'\left\{ \frac{\partial^2\xi}{\partial x^2} +2y'\frac{\partial^2\xi}{\partial x\partial y} +(y')^2\frac{\partial^2\xi}{\partial y^2} -(3yy'+y^3)\frac{\partial\xi}{\partial y} \right\}\\ &\quad +3y\left\{ \frac{\partial\eta}{\partial x} +y'\frac{\partial\eta}{\partial y} -y'\frac{\partial\xi}{\partial x} -(y')^2\frac{\partial\xi}{\partial y} \right\} +3y^2\eta+3\eta y'=0. \end{aligned}\end{array}$

Equate coefficients of independent powers of $y'$ to zero:

$\quad\begin{array}{l}\displaystyle \begin{array}{rcll} (y')^3 &:& -\dfrac{\partial^2\xi}{\partial y^2}=0 &\qquad\text{(i)} \\[10pt] (y')^2 &:& \dfrac{\partial^2\eta}{\partial y^2} +6y\dfrac{\partial\xi}{\partial y} -2\dfrac{\partial^2\xi}{\partial x\partial y} +\cancel{3y\dfrac{\partial\xi}{\partial y}} -\cancel{3y\dfrac{\partial\xi}{\partial y}}=0 &\qquad\text{(ii)} \\[10pt] y' &:& 2\dfrac{\partial^2\eta}{\partial x\partial y} -\cancel{3y\dfrac{\partial\eta}{\partial y}} +\cancel{6y\dfrac{\partial\xi}{\partial x}} +3y^3\dfrac{\partial\xi}{\partial y} -\dfrac{\partial^2\xi}{\partial x^2} +\cancel{y^3\dfrac{\partial\xi}{\partial y}} +\cancel{3y\dfrac{\partial\eta}{\partial y}} +3y\dfrac{\partial\xi}{\partial x} +3\eta=0 &\qquad\text{(iii)} \\[10pt] (y')^0 &:& \dfrac{\partial^2\eta}{\partial x^2} -y^3\dfrac{\partial\eta}{\partial y} +2y^3\dfrac{\partial\xi}{\partial x} +3y\dfrac{\partial\eta}{\partial x} +3y^2\eta=0 &\qquad\text{(iv)} \end{array}\end{array}$

(i)

$\quad\begin{array}{l}\displaystyle \xi=a(x)y+b(x).\end{array}$

(ii)

$\quad\begin{array}{l}\displaystyle \frac{\partial^2\eta}{\partial y^2}=-6ay+2a',\end{array}$

$\quad\begin{array}{l}\displaystyle \eta=-ay^3+a'y^2+c(x)y+d(x).\end{array}$

(iii)

$\quad\begin{array}{l}\displaystyle \begin{aligned} &2(-3a'y^2+2a''y+c') +3y^3a-a''y-b'' +3a'y^2+3b'y-3ay^3\\ &\qquad +3a'y^2+3cy+3d=0. \end{aligned}\end{array}$

$\quad\begin{array}{l}\displaystyle \begin{aligned} y^1:&\qquad 3a''+3b'+3c=0,\\ y^0:&\qquad 2c'-b''+3d=0. \end{aligned}\end{array}$

(iv)

$\quad\begin{array}{l}\displaystyle \begin{aligned} &-a''y^3+a'''y^2+c''y+d'' -y^3(-3ay^2+2a'y+c) +2y^3(a'y+b')\\ &\qquad +3y(-a'y^3+a''y^2+c'y+d') +3y^2(-ay^3+a'y^2+cy+d)=0. \end{aligned}\end{array}$

$\quad\begin{array}{l}\displaystyle \begin{aligned} y^3:&\qquad -a''-c+2b'+3a''+3c=0,\\ y^2:&\qquad a'''+3c'+3d=0,\\ y^1:&\qquad c''+3d'=0,\\ y^0:&\qquad d''=0. \end{aligned}\end{array}$


$\quad\begin{array}{l}\displaystyle \begin{aligned} a''+b'+c&=0 &&\text{(1)}\\ b''-2c'-3d&=0 &&\text{(2)}\\ a'''+3c'+3d&=0 &&\text{(3)}\\ c''+3d'&=0 &&\text{(4)}\\ d''&=0 &&\text{(5)} \end{aligned}\end{array}$

(5)

$\quad\begin{array}{l}\displaystyle d=D_0+D_1x.\end{array}$

(4)

$\quad\begin{array}{l}\displaystyle c=C_0+C_1x-\frac{3}{2}D_1x^2.\end{array}$

(3)

$\quad\begin{array}{l}\displaystyle a=A_0+A_1x+A_2x^2 -\frac{1}{2}(C_1+D_0)x^3 +\frac{1}{4}D_1x^4.\end{array}$

(2)

$\quad\begin{array}{l}\displaystyle b=B_0+B_1x +\frac{1}{2}(2C_1+3D_0)x^2 -\frac{1}{2}D_1x^3.\end{array}$

check in (1)

$\quad\begin{array}{l}\displaystyle \begin{aligned} &2A_2-3(C_1+D_0)x+3D_1x^2+B_1 +(2C_1+3D_0)x-\frac{3}{2}D_1x^2\\ &\qquad +C_0+C_1x-\frac{3}{2}D_1x^2=0. \end{aligned}\end{array}$

$\quad\begin{array}{l}\displaystyle 2A_2+B_1+C_0=0; \qquad -3C_1-3D_0+2C_1+3D_0+C_1=0.\end{array}$

$\quad\begin{array}{l}\displaystyle 3D_1-\frac{3}{2}D_1-\frac{3}{2}D_1=0.\end{array}$

i.e. there are 8 independent constants and there are 8 symmetries.

$\quad\begin{array}{l}\displaystyle \begin{aligned} G &= (ay+b)\frac{\partial}{\partial x} +(-ay^3+a'y^2+cy+d)\frac{\partial}{\partial y},\\[6pt] G_1 &= \frac{1}{2}x^2y\frac{\partial}{\partial x} +\left(-\frac{1}{2}x^2y^3+xy^2-y\right) \frac{\partial}{\partial y},\\[6pt] G_2 &= y\frac{\partial}{\partial x} -y^3\frac{\partial}{\partial y},\\[6pt] G_3 &= xy\frac{\partial}{\partial x} +(-xy^3+y^2)\frac{\partial}{\partial y},\\[6pt] G_4 &= \left(-\frac{1}{2}x^2y+x\right) \frac{\partial}{\partial x} +\left(\frac{1}{2}x^2y^3-xy^2\right) \frac{\partial}{\partial y},\\[6pt] G_5 &= \left(-\frac{1}{4}x^4y+\frac{1}{3}x^3\right) \frac{\partial}{\partial x} +\left(\frac{1}{4}x^4y^3-x^3y^2 +\frac{3}{2}x^2y-x\right) \frac{\partial}{\partial y},\\[6pt] G_6 &= \left(-\frac{1}{2}x^3y+x^2\right) \frac{\partial}{\partial x} +\left(\frac{1}{2}x^3y^3-\frac{3}{2}x^2y^2+xy\right) \frac{\partial}{\partial y},\\[6pt] G_7 &= \left(-\frac{1}{2}x^3y+x\right) \frac{\partial}{\partial x} +\left(\frac{1}{2}x^3y^3-\frac{3}{2}x^2y^2+1\right) \frac{\partial}{\partial y},\\[6pt] G_8 &= -\frac{\partial}{\partial x}. \end{aligned}\end{array}$


$\quad\begin{array}{l}\displaystyle y''=0.\end{array}$

$\quad\begin{array}{l}\displaystyle I_1=y',\end{array}$

$\quad\begin{array}{l}\displaystyle I_2=y'x-y,\end{array}$

$\quad\begin{array}{l}\displaystyle I_3=x-\frac{y}{y'}.\end{array}$

What generators are associated with $I_1$?

$\quad\begin{array}{l}\displaystyle G^{(1)}I_1=0.\end{array}$

$\quad\begin{array}{l}\displaystyle \left( \xi\frac{\partial}{\partial x} +\eta\frac{\partial}{\partial y} +(\eta'-y'\xi')\frac{\partial}{\partial y'} \right)y'=0.\end{array}$

$\quad\begin{array}{l}\displaystyle \eta'-y'\xi'=0.\end{array}$

$\quad\begin{array}{l}\displaystyle \frac{\partial\eta}{\partial x} +y'\frac{\partial\eta}{\partial y} -y'\frac{\partial\xi}{\partial x} -(y')^2\frac{\partial\xi}{\partial y}=0.\end{array}$

$\quad\begin{array}{l}\displaystyle \displaystyle\begin{aligned}(y')^2 &:\qquad \frac{\partial\xi}{\partial y}=0 &&\text{(i)}\\[6pt](y')^1 &:\qquad \frac{\partial\eta}{\partial y}-\frac{\partial\xi}{\partial x}=0 &&\text{(ii)}\\[6pt](y')^0 &:\qquad \frac{\partial\eta}{\partial x}=0 &&\text{(iii)}\end{aligned}\end{array}$

- **(i)** $\xi=a(x)$.

- **(ii)** $\eta=a'y+b(x)$.

- **(iii)** $a''=0$, $b'=0$.

$\quad\begin{array}{l}\displaystyle a=A_0+A_1x, \qquad b=B_0.\end{array}$

The generators

$\quad\begin{array}{l}\displaystyle \begin{aligned} G_1 &= \frac{\partial}{\partial x}, &\qquad& A_0,\\[4pt] G_2 &= \frac{\partial}{\partial y}, && B_0,\\[4pt] G_3 &= x\frac{\partial}{\partial x} +y\frac{\partial}{\partial y}, && A_1. \end{aligned}\end{array}$

$\quad\begin{array}{l}\displaystyle [G_1,G_2]=0,\end{array}$

$\quad\begin{array}{l}\displaystyle [G_1,G_3]=G_1,\end{array}$

$\quad\begin{array}{l}\displaystyle [G_2,G_3]=G_2.\end{array}$


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

$\quad\begin{array}{l}\displaystyle \begin{array}{c|cccccccc} &G_1&G_2&G_3&G_4&G_5&G_6&G_7&G_8\\ \hline G_1 &0&-G_2&-G_3&0&G_5&0&G_7-2G_6&G_3\\ G_2 &&0&0&G_2&G_1+G_4&G_3&3G_3+G_8&0\\ G_3 &&&0&0&G_6&0&2G_1-G_4&G_2\\ G_4 &&&&0&G_5&G_6&2G_6&-(G_3+G_8)\\ G_5 &&&&&0&0&0&3G_6-G_7\\ G_6 &&&&&&0&G_5&2G_4-G_1\\ G_7 &&&&&&&0&3G_4\\ G_8 &&&&&&&&0 \end{array}\end{array}$

The entries below the main diagonal have been omitted because of the skewness
of the Lie bracket.


i.e. $G_1$, $G_2$, $G_3$ form a closed subgroup. For $I_2$ and $I_3$ the same
result is found. (This gives 9 in all, but one is a linear combination of the
others.) Can use this subalgebra to find a linearising transformation.

From the commutation table for the generators of

$\quad\begin{array}{l}\displaystyle y''+3yy'+y^3=0\end{array}$

we see that

$\quad\begin{array}{l}\displaystyle \begin{aligned} [G_1,G_2]&=-G_2, &\qquad [G_1,G_3]&=-G_3,\\ [G_2,G_3]&=0. \end{aligned}\end{array}$

Define

$\quad\begin{array}{l}\displaystyle \begin{aligned} [\widetilde G_1,\widetilde G_2] &=[G_2,G_3]=0,\\ [\widetilde G_1,\widetilde G_3] &=[G_2,G_1]=G_2=\widetilde G_1,\\ [\widetilde G_2,\widetilde G_3] &=[G_3,G_1]=G_3=\widetilde G_2. \end{aligned}\end{array}$

i.e. we have the correct subalgebra.

In $x,y$ coordinates we have

$\quad\begin{array}{l}\displaystyle y''+3yy'+y^3=0,\end{array}$

$\quad\begin{array}{l}\displaystyle \begin{aligned} \widetilde G_1 &=y\frac{\partial}{\partial x} -y^3\frac{\partial}{\partial y},\\[6pt] \widetilde G_2 &=xy\frac{\partial}{\partial x} -(xy^3-y^2)\frac{\partial}{\partial y},\\[6pt] \widetilde G_3 &=\frac{1}{2}x^2y\frac{\partial}{\partial x} -\left(\frac{1}{2}x^2y^3-xy^2+y\right) \frac{\partial}{\partial y}. \end{aligned}\end{array}$

In $X,Y$ coordinates we have

$\quad\begin{array}{l}\displaystyle Y''=0,\end{array}$

$\quad\begin{array}{l}\displaystyle \begin{aligned} G_1 &=\frac{\partial}{\partial X},\\[4pt] G_2 &=\frac{\partial}{\partial Y},\\[4pt] G_3 &=X\frac{\partial}{\partial X} +Y\frac{\partial}{\partial Y}. \end{aligned}\end{array}$

$\quad\begin{array}{l}\displaystyle X=F(x,y), \qquad Y=G(x,y).\end{array}$

$\quad\begin{array}{l}\displaystyle \frac{\partial}{\partial x} =\frac{\partial F}{\partial x}\frac{\partial}{\partial X} +\frac{\partial G}{\partial x}\frac{\partial}{\partial Y},\end{array}$

$\quad\begin{array}{l}\displaystyle \frac{\partial}{\partial y} =\frac{\partial F}{\partial y}\frac{\partial}{\partial X} +\frac{\partial G}{\partial y}\frac{\partial}{\partial Y}.\end{array}$


**1.**

$\quad\begin{array}{l}\displaystyle \left(y\frac{\partial F}{\partial x} -y^3\frac{\partial F}{\partial y}\right) \frac{\partial}{\partial X} +\left(y\frac{\partial G}{\partial x} -y^3\frac{\partial G}{\partial y}\right) \frac{\partial}{\partial Y} =\frac{\partial}{\partial X}.\end{array}$

$\quad\begin{array}{l}\displaystyle y\frac{\partial G}{\partial x} -y^3\frac{\partial G}{\partial y}=0.\end{array}$

$\quad\begin{array}{l}\displaystyle \frac{dx}{y}=-\frac{dy}{y^3}.\end{array}$

$\quad\begin{array}{l}\displaystyle 0=dx+\frac{dy}{y^2}; \qquad u=x-\frac{1}{y}.\end{array}$

$\quad\begin{array}{l}\displaystyle G=G\left(x-\frac{1}{y}\right) =g\left(x-\frac{1}{y}\right).\end{array}$

$\quad\begin{array}{l}\displaystyle y\frac{\partial F}{\partial x} -y^3\frac{\partial F}{\partial y}=1.\end{array}$

$\quad\begin{array}{l}\displaystyle \frac{dx}{y}=\frac{dy}{-y^3}=\frac{dF}{1}.\end{array}$

$\quad\begin{array}{l}\displaystyle u=x-\frac{1}{y}.\end{array}$

($2^{\mathrm{nd}}$ and $3^{\mathrm{rd}}$):

$\quad\begin{array}{l}\displaystyle v=F-\frac{1}{2y^2}.\end{array}$

$\quad\begin{array}{l}\displaystyle F=\frac{1}{2y^2}+f\left(x-\frac{1}{y}\right).\end{array}$

**2.**

$\quad\begin{array}{l}\displaystyle \left[ xy\frac{\partial F}{\partial x} -(xy^3-y^2)\frac{\partial F}{\partial y} \right]\frac{\partial}{\partial X} +\left[ xy\frac{\partial G}{\partial x} -(xy^3-y^2)\frac{\partial G}{\partial y} \right]\frac{\partial}{\partial Y} =\frac{\partial}{\partial Y}.\end{array}$

$\quad\begin{array}{l}\displaystyle F:\end{array}$

$\quad\begin{array}{l}\displaystyle xyf'-(xy^3-y^2)f'\left(\frac{1}{y^2}\right)=0.\end{array}$

$\quad\begin{array}{l}\displaystyle xyf'-xyf'+f'=0.\end{array}$

$\quad\begin{array}{l}\displaystyle f'=0,\end{array}$

i.e. $f=\text{constant}$ (take as zero).

$\quad\begin{array}{l}\displaystyle xyg'-(xy^3-y^2)g'\left(\frac{1}{y^2}\right)=1.\end{array}$

$\quad\begin{array}{l}\displaystyle g'=1.\end{array}$

$\quad\begin{array}{l}\displaystyle g=x-\frac{1}{y},\end{array}$

to within a forgettable constant.

$\quad\begin{array}{l}\displaystyle X=\frac{1}{2y^2}, \qquad Y=x-\frac{1}{y}.\end{array}$

$\quad\begin{array}{l}\displaystyle y^2=\frac{1}{2X}.\end{array}$

$\quad\begin{array}{l}\displaystyle x=Y+(2X)^{1/2}.\end{array}$

$\quad\begin{array}{l}\displaystyle y=\left(\frac{1}{2X}\right)^{1/2}.\end{array}$


$\quad\begin{array}{l}\displaystyle y' =\frac{-\dfrac{\sqrt{2}}{4X^{3/2}}} {Y'+\dfrac{\sqrt{2}}{2X^{1/2}}}.\end{array}$

$\quad\begin{array}{l}\displaystyle y'' =\frac{1} {\left(Y'+\dfrac{\sqrt{2}}{2X^{1/2}}\right)^3} \left\{ \frac{\sqrt{2}}{4X^{3/2}}Y'' +\frac{3\sqrt{2}}{8X^{5/2}}Y' +\frac{1}{4X^3} \right\}.\end{array}$

$\quad\begin{array}{l}\displaystyle y''+3yy'+y^3=0\end{array}$

$\quad\begin{array}{l}\displaystyle \begin{aligned} Y'' &+\frac{3}{2X}Y' +\frac{\sqrt{2}}{2X^{3/2}}\\ &-\frac{3\sqrt{2}}{2X^{1/2}} \left(Y'+\frac{\sqrt{2}}{2X^{1/2}}\right)^2 +\left(Y'+\frac{\sqrt{2}}{2X^{1/2}}\right)^3=0. \end{aligned}\end{array}$

$\quad\begin{array}{l}\displaystyle \begin{aligned} Y''+(Y')^3 &+(Y')^2(0) +Y'\left(\frac{3}{2X}-\frac{3}{X}+\frac{3}{2X}\right)\\ &+\left( \frac{\sqrt{2}}{2X^{3/2}} -\frac{6\sqrt{2}}{8X^{3/2}} +\frac{2\sqrt{2}}{8X^{3/2}} \right)=0. \end{aligned}\end{array}$

Therefore

$\quad\begin{array}{l}\displaystyle Y''+(Y')^3=0.\end{array}$

This is not linear in the present ordering of the coordinates. The generators
are

$\quad\begin{array}{l}\displaystyle \begin{aligned} G_1 &=\frac{\partial}{\partial X},\\[4pt] G_2 &=\frac{\partial}{\partial Y},\\[4pt] G_3 &=X\frac{\partial}{\partial X} +Y\frac{\partial}{\partial Y}. \end{aligned}\end{array}$

Interchange the dependent and independent variables:

$\quad\begin{array}{l}\displaystyle \mathcal X=Y, \qquad \mathcal Y=X.\end{array}$

Then

$\quad\begin{array}{l}\displaystyle \mathcal Y'=\frac{1}{Y'}, \qquad \mathcal Y''=-\frac{Y''}{(Y')^3}.\end{array}$

Hence

$\quad\begin{array}{l}\displaystyle -\frac{\mathcal Y''}{(\mathcal Y')^3} +\frac{1}{(\mathcal Y')^3}=0,\end{array}$

so

$\quad\begin{array}{l}\displaystyle \mathcal Y''=1,\end{array}$

which is linear. To reduce it to the free-particle equation, put

$\quad\begin{array}{l}\displaystyle W=\mathcal Y-\frac{1}{2}\mathcal X^2.\end{array}$

Then

$\quad\begin{array}{l}\displaystyle W''=0.\end{array}$

<a id="linearising-transformation"></a>
Thus the corrected linearising transformation is

$\quad\begin{array}{l}\displaystyle \mathcal X=x-\frac{1}{y}, \qquad W=\frac{1}{2y^2} -\frac{1}{2}\left(x-\frac{1}{y}\right)^2 =\frac{x}{y}-\frac{x^2}{2}.\end{array}$


$\quad\begin{array}{l}\displaystyle Y(X)=A+BX, \qquad X=A+BY.\end{array}$

$\quad\begin{array}{l}\displaystyle \frac{1}{2y^2}=A+B\left(x-\frac{1}{y}\right),\end{array}$

$\quad\begin{array}{l}\displaystyle (A+Bx)y^2-By-\frac{1}{2}=0,\end{array}$

$\quad\begin{array}{l}\displaystyle y=\frac{B\mathbin{\pm}\sqrt{B^2+2(A+Bx)}}{2(A+Bx)}.\end{array}$

$\quad\begin{array}{l}\displaystyle \begin{aligned} \widetilde G_1 &=y\frac{\partial}{\partial x} -y^3\frac{\partial}{\partial y},\\[4pt] \widetilde G_2 &=xy\frac{\partial}{\partial x} -(xy^3-y^2)\frac{\partial}{\partial y}. \end{aligned}\end{array}$

$\quad\begin{array}{l}\displaystyle X=F(x,y), \qquad Y=G(x,y).\end{array}$

From

$\quad\begin{array}{l}\displaystyle \widetilde G_1\longrightarrow\frac{\partial}{\partial X},\end{array}$

$\quad\begin{array}{l}\displaystyle F=\frac{1}{2y^2}+f\left(x-\frac{1}{y}\right), \qquad G=g\left(x-\frac{1}{y}\right).\end{array}$

$\quad\begin{array}{l}\displaystyle \widetilde G_2=\frac{\partial}{\partial Y}.\end{array}$

$\quad\begin{array}{l}\displaystyle xy\frac{\partial F}{\partial x} -(xy^3-y^2)\frac{\partial F}{\partial y}=0.\end{array}$

$\quad\begin{array}{l}\displaystyle xyf'-(xy^3-y^2) \left(-\frac{1}{y^3}+\frac{1}{y^2}f'\right)=0.\end{array}$

$\quad\begin{array}{l}\displaystyle (xy-xy+1)f'+x-\frac{1}{y}=0.\end{array}$

$\quad\begin{array}{l}\displaystyle f'=-\left(x-\frac{1}{y}\right),\end{array}$

$\quad\begin{array}{l}\displaystyle f=-\frac{1}{2}\left(x-\frac{1}{y}\right)^2.\end{array}$

$\quad\begin{array}{l}\displaystyle \begin{aligned} F &=\frac{1}{2y^2} -\frac{1}{2}\left(x-\frac{1}{y}\right)^2\\ &=-\frac{1}{2}x^2+\frac{x}{y}. \end{aligned}\end{array}$

$\quad\begin{array}{l}\displaystyle xy\frac{\partial G}{\partial x} -(xy^3-y^2)\frac{\partial G}{\partial y}=1.\end{array}$

$\quad\begin{array}{l}\displaystyle xyg'-(xy^3-y^2)\frac{1}{y^2}g'=1.\end{array}$

$\quad\begin{array}{l}\displaystyle g'=1, \qquad g=x-\frac{1}{y}.\end{array}$


Transform is

$\quad\begin{array}{l}\displaystyle X=-\frac{1}{2}x^2+\frac{x}{y}, \qquad Y=x-\frac{1}{y}.\end{array}$

$\quad\begin{array}{l}\displaystyle Y' =\frac{1+\dfrac{y'}{y^2}} {-x+\dfrac{1}{y}-\dfrac{xy'}{y^2}} =\frac{1} {\dfrac{y}{y^2+y'}-x}.\end{array}$

$\quad\begin{array}{l}\displaystyle \begin{aligned} Y'' ={}&\frac{1} {-x+\dfrac{1}{y}-\dfrac{xy'}{y^2}} \left\{ -\frac{1} {\left(\dfrac{y}{y^2+y'}-x\right)^2} \left[ \frac{y'}{y^2+y'} -\frac{y(2yy'+y'')}{(y^2+y')^2} -1 \right] \right\}\\[6pt] ={}&\frac{1}{(\cdots)} \left\{ y^2y'+(y')^2-2y^2y'-yy''-y^4-2y^2y'-(y')^2 \right\}\\[6pt] ={}&\frac{1}{(\cdots)} \left\{-y\left(y''+3yy'+y^3\right)\right\}=0. \end{aligned}\end{array}$

$\quad\begin{array}{l}\displaystyle Y=A+BX.\end{array}$

$\quad\begin{array}{l}\displaystyle x-\frac{1}{y} =A+B\left(-\frac{1}{2}x^2+\frac{x}{y}\right).\end{array}$

$\quad\begin{array}{l}\displaystyle (1+Bx)\frac{1}{y}=\frac{1}{2}Bx^2+x-A.\end{array}$

$\quad\begin{array}{l}\displaystyle \begin{aligned} y &=\frac{1+Bx}{-A+x+\frac{1}{2}Bx^2}\\[4pt] &=\frac{2(I_1+x)}{2I_2+2I_1x+x^2}, \end{aligned}\end{array}$

$\quad\begin{array}{l}\displaystyle I_1=\frac{1}{B}, \qquad I_2=-\frac{A}{B}.\end{array}$

Aside: differentiating $y$ and solving for $I_1$ and $I_2$ between the two
equations gives

$\quad\begin{array}{l}\displaystyle I_1=-x+\frac{y}{y'+y^2} \qquad \left(=\frac{1}{Y'}\right),\end{array}$

and

$\quad\begin{array}{l}\displaystyle I_2=\frac{1}{2}x^2+\frac{1-xy}{y'+y^2},\end{array}$

which are two resonance integrals.


$\quad\begin{array}{l}\displaystyle y''+3yy'+y^3=0.\end{array}$

Is this the only equation of this structure which can be linearised?

Let

$\quad\begin{array}{l}\displaystyle x=\alpha X, \qquad y=\beta Y.\end{array}$

Then

$\quad\begin{array}{l}\displaystyle \frac{\beta}{\alpha^2}Y'' +3\frac{\beta^2}{\alpha}YY' +\beta^3Y^3=0,\end{array}$

or

$\quad\begin{array}{l}\displaystyle Y''+3\alpha\beta YY'+(\alpha\beta)^2Y^3=0.\end{array}$

Any choice of $\alpha\beta$ will give a different equation which can also be
linearised.

Let

$\quad\begin{array}{l}\displaystyle \begin{aligned} y   &=\frac{\omega'}{\omega},\\[4pt] y'  &=\frac{\omega''\omega-(\omega')^2}{\omega^2},\\[4pt] y'' &=\frac{\omega'''\omega^2-3\omega''\omega'\omega+2(\omega')^3} {\omega^3}. \end{aligned}\end{array}$

Thus

$\quad\begin{array}{l}\displaystyle y''+3yy'+y^3\longrightarrow\frac{\omega'''}{\omega}=0,\end{array}$

so

$\quad\begin{array}{l}\displaystyle \omega'''=0, \qquad \omega=A+Bx+Cx^2,\end{array}$

and

$\quad\begin{array}{l}\displaystyle y=\frac{B+2Cx}{A+Bx+Cx^2}.\end{array}$

<a id="riccati-equation"></a>
This is a particular instance of a Riccati hierarchy. Recall the Riccati
equation which is obtained from the general second-order linear differential
equation

$\quad\begin{array}{l}\displaystyle y''+f(x)y'+g(x)y=0.\end{array}$

Let

$\quad\begin{array}{l}\displaystyle \frac{y'}{y}=u.\end{array}$

Then

$\quad\begin{array}{l}\displaystyle \frac{y''}{y}-\frac{(y')^2}{y^2}=u', \qquad \frac{y''}{y}=u'+u^2,\end{array}$

and hence

$\quad\begin{array}{l}\displaystyle u'+u^2+f(x)u+g(x)=0,\end{array}$

the Riccati equation.


<a id="nonlinear-equations-with-sl3-symmetry"></a>
### What nonlinear differential equations can have $SL(3,\mathbb R)$ symmetry?

It must be possible to transform the differential equation to

$\quad\begin{array}{l}\displaystyle Q''=0\end{array}$

by means of a point transformation. The reverse also applies.

Let

$\quad\begin{array}{l}\displaystyle T=F(q,t), \qquad Q=G(q,t).\end{array}$

Then $Q''=0$ becomes

$\quad\begin{array}{l}\displaystyle \begin{aligned} \ddot q {}&+\dot q^3J^{-1} \left( \frac{\partial F}{\partial q}\frac{\partial^2G}{\partial q^2} -\frac{\partial^2F}{\partial q^2}\frac{\partial G}{\partial q} \right)\\[4pt] {}&+\dot q^2J^{-1} \left( \frac{\partial F}{\partial t}\frac{\partial^2G}{\partial q^2} +2\frac{\partial F}{\partial q}\frac{\partial^2G}{\partial q\partial t} -2\frac{\partial^2F}{\partial q\partial t}\frac{\partial G}{\partial q} -\frac{\partial^2F}{\partial q^2}\frac{\partial G}{\partial t} \right)\\[4pt] {}&+\dot qJ^{-1} \left( 2\frac{\partial F}{\partial t}\frac{\partial^2G}{\partial q\partial t} +\frac{\partial F}{\partial q}\frac{\partial^2G}{\partial t^2} -\frac{\partial^2F}{\partial t^2}\frac{\partial G}{\partial q} -2\frac{\partial^2F}{\partial q\partial t}\frac{\partial G}{\partial t} \right)\\[4pt] {}&+J^{-1} \left( \frac{\partial F}{\partial t}\frac{\partial^2G}{\partial t^2} -\frac{\partial^2F}{\partial t^2}\frac{\partial G}{\partial t} \right)=0, \end{aligned}\end{array}$

where

$\quad\begin{array}{l}\displaystyle J=\frac{\partial(F,G)}{\partial(t,q)}\ne0.\end{array}$


The general third-order constant-coefficient equation

$\quad\begin{array}{l}\displaystyle \omega'''+a\omega''+b\omega'+c\omega=0\end{array}$

has a solution. Let

$\quad\begin{array}{l}\displaystyle \frac{\omega'}{\omega}=y.\end{array}$

Then

$\quad\begin{array}{l}\displaystyle \frac{\omega'}{\omega}=y, \qquad \frac{\omega''}{\omega}=y'+y^2,\end{array}$

and

$\quad\begin{array}{l}\displaystyle \begin{aligned} \frac{\omega'''}{\omega} &=y''+2yy'+y(y'+y^2)\\ &=y''+3yy'+y^3. \end{aligned}\end{array}$

Thus

$\quad\begin{array}{l}\displaystyle y''+3yy'+y^3+a(y'+y^2)+by+c=0\end{array}$

can be linearised.

Aside: $\omega'''=0$ has seven symmetries.

<a id="linearisable-second-order-odes"></a>
### What second-order ODEs can be linearised?

$\quad\begin{array}{l}\displaystyle y''=f(x,y,y').\end{array}$

(Transparency.)

<a id="classify-by-extra-information"></a>
#### Classify by extra information

<a id="existence-of-one-symmetry"></a>
##### 1. Existence of one symmetry

Transform the differential equation to
standard form by making the symmetry, say, $\partial/\partial x$. Then the
differential equation is

$\quad\begin{array}{l}\displaystyle y''=f(y,y').\end{array}$

<a id="existence-of-two-symmetries"></a>
##### 2. Existence of two symmetries

The possibilities include

$\quad\begin{array}{l}\displaystyle G_1=x\frac{\partial}{\partial x}, \qquad G_2=\frac{\partial}{\partial x}, \qquad [G_1,G_2]=-G_2,\end{array}$

and

$\quad\begin{array}{l}\displaystyle G_1=\frac{\partial}{\partial x}, \qquad G_2=\frac{\partial}{\partial y}, \qquad [G_1,G_2]=0,\end{array}$

with $G_2\ne f(x,y)G_1$.

<a id="two-commuting-symmetries"></a>
##### Two commuting symmetries

$\quad\begin{array}{l}\displaystyle [G_1,G_2]=0.\end{array}$

Suppose we choose coordinates such that

$\quad\begin{array}{l}\displaystyle G_1=\frac{\partial}{\partial y}, \qquad G_2=\xi\frac{\partial}{\partial x} +\eta\frac{\partial}{\partial y}.\end{array}$

Then

$\quad\begin{array}{l}\displaystyle [G_1,G_2] =\frac{\partial\xi}{\partial y}\frac{\partial}{\partial x} +\frac{\partial\eta}{\partial y}\frac{\partial}{\partial y}=0,\end{array}$

so

$\quad\begin{array}{l}\displaystyle \frac{\partial\xi}{\partial y}=0, \qquad \frac{\partial\eta}{\partial y}=0.\end{array}$


<a id="type-ii"></a>
Observe that Type II is already linear and the solution is reduced to two
quadratures.

<a id="two-non-commuting-symmetries"></a>
##### Two non-commuting symmetries

$\quad\begin{array}{l}\displaystyle [G_1,G_2]=G_1.\end{array}$

Let the coordinates be such that

$\quad\begin{array}{l}\displaystyle G_1=\frac{\partial}{\partial y}, \qquad G_2=\xi\frac{\partial}{\partial x} +\eta\frac{\partial}{\partial y}.\end{array}$

Then $[G_1,G_2]=G_1$ becomes

$\quad\begin{array}{l}\displaystyle \frac{\partial\xi}{\partial y}\frac{\partial}{\partial x} +\frac{\partial\eta}{\partial y}\frac{\partial}{\partial y} =\frac{\partial}{\partial y},\end{array}$

so

$\quad\begin{array}{l}\displaystyle \frac{\partial\xi}{\partial y}=0, \qquad \frac{\partial\eta}{\partial y}=1.\end{array}$

Thus

$\quad\begin{array}{l}\displaystyle \xi=a(x), \qquad \eta=b(x)+y,\end{array}$

and

$\quad\begin{array}{l}\displaystyle G_2=a(x)\frac{\partial}{\partial x} +(b(x)+y)\frac{\partial}{\partial y}.\end{array}$

The two forms written for $G_2$ are

$\quad\begin{array}{l}\displaystyle \begin{aligned} G_2 &=y\frac{\partial}{\partial y}=yG_1,\\ G_2 &=x\frac{\partial}{\partial x} +y\frac{\partial}{\partial y} \ne f(x,y)G_1. \end{aligned}\end{array}$

<a id="types-i-and-iv"></a>
Note that Type IV is linear. Type I,

$\quad\begin{array}{l}\displaystyle y''=f(y'),\end{array}$

and Type III,

$\quad\begin{array}{l}\displaystyle y''=\frac1x f(y'),\end{array}$

are the ones whose linearisation is still an open question.

<a id="linearisation-proposition"></a>
#### Proposition

In order that a second-order ODE have the symmetry algebra
$\mathfrak{sl}(3,\mathbb R)$, and hence be linearisable, it is necessary and
sufficient that it have the three-element subalgebra $\mathcal X$ of generators
$G_1,G_2,G_3$ satisfying

$\quad\begin{array}{l}\displaystyle [G_1,G_2]=0, \qquad [G_2,G_3]=0, \qquad [G_3,G_1]=G_2.\end{array}$

<a id="linearisation-proposition-proof"></a>
#### Proof

For

$\quad\begin{array}{l}\displaystyle y''=f(x,y,y'),\end{array}$

a generator of symmetry is

$\quad\begin{array}{l}\displaystyle G=\xi\frac{\partial}{\partial x} +\eta\frac{\partial}{\partial y}.\end{array}$

We have three such generators, $G_1,G_2,G_3$. We cannot have both

$\quad\begin{array}{l}\displaystyle G_1=\phi(x,y)G_2, \qquad G_3=\psi(x,y)G_2,\end{array}$

for suitable functions $\phi$ and $\psi$.


Suppose they are. Then

$\quad\begin{array}{l}\displaystyle \begin{aligned} [G_1,G_2] &=[\phi G_2,G_2]\\ &=\phi G_2G_2-G_2(\phi G_2)\\ &=\phi G_2G_2-\phi G_2G_2-(G_2\phi)G_2\\ &=0. \end{aligned}\end{array}$

if $G_2\phi=0$. Likewise,

$\quad\begin{array}{l}\displaystyle [G_2,G_3]=0 \quad\Longrightarrow\quad G_2\psi=0.\end{array}$

Then

$\quad\begin{array}{l}\displaystyle \begin{aligned} [G_3,G_1] &=[\psi G_2,\phi G_2]\\ &=\psi G_2(\phi G_2)-\phi G_2(\psi G_2)\\ &=\psi\phi G_2G_2+\psi(G_2\phi)G_2 -\phi\psi G_2G_2-\phi(G_2\psi)G_2\\ &=0\ne G_2. \end{aligned}\end{array}$

We assume that there is no function $\phi(x,y)$ such that

$\quad\begin{array}{l}\displaystyle G_1=\phi G_2.\end{array}$

(We cannot make the statement about both $G_1$ and $G_3$.) Hence there exists
a regular point transformation

$\quad\begin{array}{l}\displaystyle Y=F(x,y), \qquad X=G(x,y),\end{array}$

such that the generators are

$\quad\begin{array}{l}\displaystyle G_1=\frac{\partial}{\partial x}, \qquad G_2=\frac{\partial}{\partial y}.\end{array}$

Write

$\quad\begin{array}{l}\displaystyle G_3=\xi\frac{\partial}{\partial x} +\eta\frac{\partial}{\partial y}.\end{array}$

Then

$\quad\begin{array}{l}\displaystyle \begin{aligned} [G_2,G_3] &=\frac{\partial\xi}{\partial y}\frac{\partial}{\partial x} +\frac{\partial\eta}{\partial y}\frac{\partial}{\partial y} &&=0,\\[6pt] [G_3,G_1] &=-\frac{\partial\xi}{\partial x}\frac{\partial}{\partial x} -\frac{\partial\eta}{\partial x}\frac{\partial}{\partial y} &&=\frac{\partial}{\partial y}. \end{aligned}\end{array}$

That is,

$\quad\begin{array}{l}\displaystyle \frac{\partial\xi}{\partial x}=0, \qquad \frac{\partial\xi}{\partial y}=0, \qquad \frac{\partial\eta}{\partial x}=-1, \qquad \frac{\partial\eta}{\partial y}=0.\end{array}$


Thus

$\quad\begin{array}{l}\displaystyle G_3=A\frac{\partial}{\partial x} +(B-x)\frac{\partial}{\partial y}.\end{array}$

Take

$\quad\begin{array}{l}\displaystyle G_3=-x\frac{\partial}{\partial y}.\end{array}$

The equation

$\quad\begin{array}{l}\displaystyle y''=f(x,y,y')\end{array}$

is invariant under these three generators. From

$\quad\begin{array}{l}\displaystyle G_1^{(2)}[y''-f(x,y,y')]=0\end{array}$

we obtain

$\quad\begin{array}{l}\displaystyle \frac{\partial f}{\partial x}=0.\end{array}$

From

$\quad\begin{array}{l}\displaystyle G_2^{(2)}[y''-f]=0\end{array}$

we obtain

$\quad\begin{array}{l}\displaystyle \frac{\partial f}{\partial y}=0.\end{array}$

Finally,

$\quad\begin{array}{l}\displaystyle G_3^{(2)}[y''-f]=0\end{array}$

gives

$\quad\begin{array}{l}\displaystyle \left(-x\frac{\partial}{\partial y} -\frac{\partial}{\partial y'}\right)[y''-f]=0,\end{array}$

so

$\quad\begin{array}{l}\displaystyle x\frac{\partial f}{\partial y} +\frac{\partial f}{\partial y'}=0.\end{array}$

Therefore $f$ is constant and the differential equation is

$\quad\begin{array}{l}\displaystyle y''=\text{constant},\end{array}$

which is linear. Hence the symmetry of the original differential equation is
$\mathfrak{sl}(3,\mathbb R)$. We have proven sufficiency, i.e. the possession
of $G_1,G_2,G_3$ such that

$\quad\begin{array}{l}\displaystyle [G_1,G_2]=0, \qquad [G_2,G_3]=0, \qquad [G_3,G_1]=G_2.\end{array}$

Necessity follows from $\mathcal X$ being a subalgebra of
$\mathfrak{sl}(3,\mathbb R)$.

<a id="can-one-do-better"></a>
#### Can one do better?

Suppose we have two generators $G_1$ and $G_2$ with the
properties

$\quad\begin{array}{l}\displaystyle [G_1,G_2]=0, \qquad G_2=f(x,y)G_1.\end{array}$

Make a point transformation to $X,Y$ so that

$\quad\begin{array}{l}\displaystyle \overline G_1=\frac{\partial}{\partial Y}, \qquad \overline G_2=F(X,Y)\frac{\partial}{\partial Y}.\end{array}$

Then

$\quad\begin{array}{l}\displaystyle [\overline G_1,\overline G_2] =\left[\frac{\partial}{\partial Y}, F\frac{\partial}{\partial Y}\right] =\frac{\partial F}{\partial Y}\frac{\partial}{\partial Y}=0,\end{array}$

and hence

$\quad\begin{array}{l}\displaystyle \frac{\partial F}{\partial Y}=0.\end{array}$


Without loss of generality, take $F=X$. Thus

$\quad\begin{array}{l}\displaystyle G_1=\frac{\partial}{\partial y}, \qquad G_2=x\frac{\partial}{\partial y},\end{array}$

which means that $y''=F(x,y,y')$ has the form

$\quad\begin{array}{l}\displaystyle y''=F(x),\end{array}$

which is linear and so has $\mathfrak{sl}(3,\mathbb R)$ symmetry.

If we take $G_1$ and $G_2$ such that

$\quad\begin{array}{l}\displaystyle [G_1,G_2]=0, \qquad G_2\ne f(x,y)G_1,\end{array}$

we may take the standard forms

$\quad\begin{array}{l}\displaystyle G_1=\frac{\partial}{\partial x}, \qquad G_2=\frac{\partial}{\partial y}.\end{array}$

The differential equation invariant under $G_1$ and $G_2$ is

$\quad\begin{array}{l}\displaystyle y''=f(y').\end{array}$

We know that linearisation is possible only if the differential equation has
the form

$\quad\begin{array}{l}\displaystyle y''+( )y'^3+( )y'^2+( )y'+( )=0,\end{array}$

where the quantities in parentheses are functions of $x$ and $y$. Hence
$f(y')$ must take the form

$\quad\begin{array}{l}\displaystyle f(y')=a(y')^3+b(y')^2+cy'+d,\end{array}$

where $a,b,c,d$ are constants. Thus

$\quad\begin{array}{l}\displaystyle y''=a(y')^3+b(y')^2+cy'+d, \qquad a\ne0.\end{array}$

Put

$\quad\begin{array}{l}\displaystyle y=\alpha Y+\beta X, \qquad x=\gamma X.\end{array}$

Then

$\quad\begin{array}{l}\displaystyle \frac{\alpha}{\gamma^2}Y'' =a\left(\frac{\alpha}{\gamma}Y'+\beta\right)^3 +b\left(\frac{\alpha}{\gamma}Y'+\beta\right)^2 +c\left(\frac{\alpha}{\gamma}Y'+\beta\right)+d.\end{array}$

The choice

$\quad\begin{array}{l}\displaystyle \beta=-\frac{b}{3a}\end{array}$

removes the $(Y')^2$ term, and

$\quad\begin{array}{l}\displaystyle \frac{a\alpha^2}{\gamma}=1\end{array}$

makes the coefficient of $(Y')^3$ unity. The equation reduces to

$\quad\begin{array}{l}\displaystyle y''=(y')^3+cy'+d.\end{array}$


For this to be linearisable there must be six more generators in addition to
the two we already have. Let

$\quad\begin{array}{l}\displaystyle G=\xi\frac{\partial}{\partial x} +\eta\frac{\partial}{\partial y}\end{array}$

be one of them. Then

$\quad\begin{array}{l}\displaystyle \begin{aligned} {}&\eta_{xx}+2y'\eta_{xy}+(y')^2\eta_{yy} +\bigl((y')^3+cy'+d\bigr)\eta_y\\ {}&\quad-y'\left[ \xi_{xx}+2y'\xi_{xy}+(y')^2\xi_{yy} +\bigl((y')^3+cy'+d\bigr)\xi_y \right]\\ {}&\quad-2\bigl((y')^3+cy'+d\bigr)(\xi_x+y'\xi_y)\\ {}&=(3(y')^2+c) \left(\eta_x+y'\eta_y-y'\xi_x-(y')^2\xi_y\right). \end{aligned}\end{array}$

Equating coefficients of like powers of $y'$ gives

$\quad\begin{array}{l}\displaystyle \xi_{yy}+2\eta_y-\xi_x=0, \qquad\text{(i)}\end{array}$

$\quad\begin{array}{l}\displaystyle 2c\xi_y+3\eta_x-\eta_{yy}+2\xi_{xy}=0, \qquad\text{(ii)}\end{array}$

$\quad\begin{array}{l}\displaystyle c\xi_x+3d\xi_y-2\eta_{xy}+\xi_{xx}=0, \qquad\text{(iii)}\end{array}$

$\quad\begin{array}{l}\displaystyle c\eta_x-d\eta_y+2d\xi_x-\eta_{xx}=0. \qquad\text{(iv)}\end{array}$

From (iv),

$\quad\begin{array}{l}\displaystyle \xi_x=\frac1{2d}\left(\eta_{xx}+d\eta_y-c\eta_x\right),\end{array}$

and from (iii),

$\quad\begin{array}{l}\displaystyle \begin{aligned} \xi_y &=\frac1{3d}\left(2\eta_{xy}-\xi_{xx}-c\xi_x\right)\\ &=\frac1{3d}\left[ 2\eta_{xy} -\frac1{2d}\left(\eta_{xxx}+d\eta_{xy}-c\eta_{xx}\right) -\frac{c}{2d}\left(\eta_{xx}+d\eta_y-c\eta_x\right) \right]. \end{aligned}\end{array}$


Eliminate $\eta$. From (i),

$\quad\begin{array}{l}\displaystyle \eta_y=\frac12(\xi_x-\xi_{yy}), \qquad\text{(v)}\end{array}$

and from (ii),

$\quad\begin{array}{l}\displaystyle \begin{aligned} \eta_x &=\frac13(\eta_{yy}-2c\xi_y-2\xi_{xy})\\ &=-\frac16(\xi_{yyy}+3\xi_{xy}+4c\xi_y), \end{aligned} \qquad\text{(vi)}\end{array}$

using (v). Substitution of (v) and (vi) in (iii) and (iv) gives

$\quad\begin{array}{l}\displaystyle \xi_{xyy}+3d\xi_y+c\xi_x=0, \qquad\text{(vii)}\end{array}$

and

$\quad\begin{array}{l}\displaystyle \xi_{xyyy}-c\xi_{yyy}+3\xi_{xxy} +3d\xi_{yy}+c\xi_{xy}-4c^2\xi_y+9d\xi_x=0. \qquad\text{(viii)}\end{array}$

The consistency condition between (v) and (vi) is

$\quad\begin{array}{l}\displaystyle \frac{\partial}{\partial x}(\eta_y) =\frac{\partial}{\partial y}(\eta_x),\end{array}$

which gives

$\quad\begin{array}{l}\displaystyle \xi_{yyyy}+4c\xi_{yy}+3\xi_{xx}=0. \qquad\text{(ix)}\end{array}$

Using the $y$ derivative of (vii), equation (viii) simplifies to

$\quad\begin{array}{l}\displaystyle -c\xi_{yyy}+3\xi_{xxy}-4c^2\xi_y+9d\xi_x=0. \qquad\text{(x)}\end{array}$

Also,

$\quad\begin{array}{l}\displaystyle \frac{\partial^2}{\partial y^2}(\text{vii}) -\frac{\partial}{\partial x}(\text{ix})\end{array}$

gives

$\quad\begin{array}{l}\displaystyle \xi_{xxx}+c\xi_{xyy}-d\xi_{yyy}=0. \qquad\text{(xi)}\end{array}$

Use (vii), (x), and (xi), which can be shown to be equivalent to (vii),
(viii), and (ix), except when both $c$ and $d$ are zero.

For the case $c=d=0$,

$\quad\begin{array}{l}\displaystyle \xi_{xyy}=0, \qquad\text{(vii)}\end{array}$

and

$\quad\begin{array}{l}\displaystyle \xi_{xyyy}+3\xi_{xxy}=0. \qquad\text{(viii)}\end{array}$



Equation (ix) becomes

$\quad\begin{array}{l}\displaystyle \xi_{yyyy}+3\xi_{xx}=0. \qquad\text{(ix)}\end{array}$

From (vii),

$\quad\begin{array}{l}\displaystyle \xi=F(y)+yG(x)+H(x).\end{array}$

Equation (viii) gives

$\quad\begin{array}{l}\displaystyle G''=0.\end{array}$

In (ix),

$\quad\begin{array}{l}\displaystyle F^{(4)}+3yG''+H''=0.\end{array}$

Therefore

$\quad\begin{array}{l}\displaystyle \begin{aligned} G &=G_0+G_1x,\\ H &=H_0+H_1x+\frac{H_2}{2}x^2,\\ F &=F_1+F_2y+F_3y^2+F_4y^3-\frac{H_2}{24}y^4. \end{aligned}\end{array}$

Thus

$\quad\begin{array}{l}\displaystyle \begin{aligned} \xi={}&F_1+F_2y+F_3y^2+F_4y^3-\frac{H_2}{24}y^4 +G_0y+G_1xy\\ &+H_0+H_1x+\frac{H_2}{2}x^2\\[4pt] ={}&(F_1+H_0)+(F_2+G_0)y+F_3y^2+F_4y^3+G_1xy+H_1x\\ &+H_2\left(\frac12x^2-\frac1{24}y^4\right). \end{aligned}\end{array}$

The eighth generator comes from

$\quad\begin{array}{l}\displaystyle \eta_x=0, \qquad \eta_y=0\end{array}$

when $\xi=0$, i.e. $\eta$ is constant. This gives

$\quad\begin{array}{l}\displaystyle G_1=\frac{\partial}{\partial y}.\end{array}$

For

$\quad\begin{array}{l}\displaystyle y''=(y')^3,\end{array}$

find the linearising transformation.


Note that we have

$\quad\begin{array}{l}\displaystyle \frac{\partial}{\partial x}, \qquad \frac{\partial}{\partial y}, \qquad -y\frac{\partial}{\partial x}.\end{array}$

Let $X=y$ and $Y=x$. Then the algebra consists of

$\quad\begin{array}{l}\displaystyle \frac{\partial}{\partial Y}, \qquad \frac{\partial}{\partial X}, \qquad -X\frac{\partial}{\partial Y},\end{array}$

which is the standard form. Also,

$\quad\begin{array}{l}\displaystyle Y'=\frac1{y'}, \qquad Y''=-\frac{y''}{(y')^3}.\end{array}$

Thus $y''=(y')^3$ becomes

$\quad\begin{array}{l}\displaystyle -\frac{Y''}{(Y')^3}=\frac1{(Y')^3},\end{array}$

or

$\quad\begin{array}{l}\displaystyle Y''=-1.\end{array}$

Hence

$\quad\begin{array}{l}\displaystyle \begin{aligned} Y &=A+BX-\frac12X^2,\\ x &=A+By-\frac12y^2. \end{aligned}\end{array}$

Alternatively,

$\quad\begin{array}{l}\displaystyle \begin{aligned} y''                 &=(y')^3,\\[4pt] \frac{y''}{(y')^2}  &=y',\\[4pt] -\frac1{y'}         &=y+C,\\[4pt] -1                   &=(y+C)y'. \end{aligned}\end{array}$

and

$\quad\begin{array}{l}\displaystyle K-x=\frac12y^2+Cy.\end{array}$


Retain

$\quad\begin{array}{l}\displaystyle \xi_{xxx}+c\xi_{xyy}-d\xi_{yyy}=0, \qquad\text{(xi)}\end{array}$

$\quad\begin{array}{l}\displaystyle \xi_{xyy}+3d\xi_y+c\xi_x=0, \qquad\text{(vii)}\end{array}$

and

$\quad\begin{array}{l}\displaystyle -c\xi_{yyy}+3\xi_{xxy}-4c^2\xi_y+9d\xi_x=0. \qquad\text{(x)}\end{array}$

Observe that (xi) is homogeneous in order. Hence it can be solved by
factorisation. Suppose it factors as

$\quad\begin{array}{l}\displaystyle \left(\frac{\partial}{\partial x} +\alpha\frac{\partial}{\partial y}\right) \left(\frac{\partial}{\partial x} +\beta\frac{\partial}{\partial y}\right) \left(\frac{\partial}{\partial x} +\gamma\frac{\partial}{\partial y}\right)\xi=0.\end{array}$

Corresponding to each factor there is an equation for a characteristic.
However, one must be careful of repeated roots. Observe that

$\quad\begin{array}{l}\displaystyle \alpha\beta\gamma=-d, \qquad \alpha\beta+\beta\gamma+\gamma\alpha=c, \qquad \alpha+\beta+\gamma=0.\end{array}$

A triple root gives $c=d=0$, which has already been treated.

For two roots $\alpha$ and $-2\alpha$,

$\quad\begin{array}{l}\displaystyle c=-3\alpha^2, \qquad d=-2\alpha^3,\end{array}$

and

$\quad\begin{array}{l}\displaystyle \left(\frac{\partial}{\partial x} +\alpha\frac{\partial}{\partial y}\right)^2 \left(\frac{\partial}{\partial x} -2\alpha\frac{\partial}{\partial y}\right)\xi=0.\end{array}$

The characteristic equations give

$\quad\begin{array}{l}\displaystyle u_1=\alpha x-y, \qquad u_2=2\alpha x+y.\end{array}$

The solution is

$\quad\begin{array}{l}\displaystyle \xi=f(\alpha x-y)+g(2\alpha x+y) +(2\alpha x+y)h(\alpha x-y).\end{array}$

Substitution in (xi) and (x) gives

$\quad\begin{array}{l}\displaystyle \begin{aligned} {}&\alpha f'''+2\alpha g'''+2\alpha h''+\alpha h''-\alpha h''' +\alpha(2\alpha x+y)h'''\\ {}&\quad-6\alpha^3[-f'+g'+h-(2\alpha x+y)h']\\ {}&\quad-3\alpha^2[\alpha f''+2\alpha g''+2\alpha h +\alpha(2\alpha x+y)h'']=0. \end{aligned}\end{array}$


The separate coefficients give

$\quad\begin{array}{l}\displaystyle 2\alpha g'''-12\alpha^3g'=0,\end{array}$

$\quad\begin{array}{l}\displaystyle (2\alpha x+y)(h'''+3\alpha^3h')=0,\end{array}$

and

$\quad\begin{array}{l}\displaystyle \alpha f'''+2\alpha h''+3\alpha^3f'=0.\end{array}$

The remaining determining equation is

$\quad\begin{array}{l}\displaystyle \begin{aligned} 0={}&3\alpha^2\left[-f'''+g'''+3h''-(2\alpha x+y)h'''\right]\\ &+3\left[-\alpha^2f'''+4\alpha^2g'''-3\alpha^2h'' -\alpha^2(2\alpha x+y)h'''\right]\\ &+36\alpha^4\left[-f'+g'+h-(2\alpha x+y)h'\right]\\ &-18\alpha^3\left[\alpha f''+2\alpha g''+2\alpha h' +\alpha(2\alpha x+y)h''\right]. \end{aligned} \qquad\text{(ix)}\end{array}$

The correct equations are

$\quad\begin{array}{l}\displaystyle g'''=0, \qquad h'''+9\alpha^2h'=0, \qquad f'''+9\alpha^2f'=0.\end{array}$

The solutions written in the notes are

$\quad\begin{array}{l}\displaystyle \begin{aligned} g &=G_0+G_1(2\alpha x+y)+G_2(2\alpha x+y)^2,\\ h &=\left[H_0+H_1\sin 3\alpha(\alpha x-y) +H_2\cos 3\alpha(\alpha x-y)\right](2\alpha x+y),\\ f &=f_0+F_1(\alpha x-y)+F_2(\alpha x-y)^2. \end{aligned}\end{array}$

There are seven independent solutions and so seven generators. The eighth
comes from $\xi=0$, giving $\eta$ constant.

For three distinct roots,

$\quad\begin{array}{l}\displaystyle -2\alpha, \qquad \alpha+\delta, \qquad \alpha-\delta,\end{array}$

where $\delta\ne0,\pm3\alpha$ to avoid repeated roots, equation (xi) becomes

$\quad\begin{array}{l}\displaystyle \left(\frac{\partial}{\partial x} -2\alpha\frac{\partial}{\partial y}\right) \left(\frac{\partial}{\partial x} +(\alpha+\delta)\frac{\partial}{\partial y}\right) \left(\frac{\partial}{\partial x} +(\alpha-\delta)\frac{\partial}{\partial y}\right)\xi=0,\end{array}$

where

$\quad\begin{array}{l}\displaystyle c=\delta^2-3\alpha^2, \qquad d=2\alpha(\alpha^2-\delta^2).\end{array}$

The characteristics give

$\quad\begin{array}{l}\displaystyle u_1=2\alpha x+y, \qquad u_2=(\alpha+\delta)x-y, \qquad u_3=(\alpha-\delta)x-y,\end{array}$

and

$\quad\begin{array}{l}\displaystyle \xi=f(2\alpha x+y) +g((\alpha+\delta)x-y) +h((\alpha-\delta)x-y).\end{array}$


Substitution into (vii) and (xi) again gives seven generators, which become
eight with $\eta$ constant. Hence

$\quad\begin{array}{l}\displaystyle y''=(y')^3+cy'+d\end{array}$

is always linearisable.

From Type 2, consider

$\quad\begin{array}{l}\displaystyle y''=(y')^2+d.\end{array}$

Invariance under

$\quad\begin{array}{l}\displaystyle G=\xi\frac{\partial}{\partial x} +\eta\frac{\partial}{\partial y}\end{array}$

gives

$\quad\begin{array}{l}\displaystyle \begin{aligned} {}&\eta_{xx}+2y'\eta_{xy}+(y')^2\eta_{yy} +\bigl((y')^2+d\bigr)\eta_y\\ {}&\quad-y'\left[ \xi_{xx}+2y'\xi_{xy}+(y')^2\xi_{yy} +\bigl((y')^2+d\bigr)\xi_y \right]\\ {}&\quad-2\bigl((y')^2+d\bigr)(\xi_x+y'\xi_y)\\ {}&=2y'\left(\eta_x+y'\eta_y-y'\xi_x-(y')^2\xi_y\right). \end{aligned}\end{array}$

Equating powers of $y'$ gives

$\quad\begin{array}{l}\displaystyle \xi_{yy}+\xi_y=0, \qquad\text{(i)}\end{array}$

$\quad\begin{array}{l}\displaystyle \eta_{yy}-\eta_y-2\xi_{xy}=0, \qquad\text{(ii)}\end{array}$

$\quad\begin{array}{l}\displaystyle 2\eta_{xy}-2\eta_x-\xi_{xx}-3d\xi_y=0, \qquad\text{(iii)}\end{array}$

$\quad\begin{array}{l}\displaystyle \eta_{xx}+d\eta_y-2d\xi_x=0. \qquad\text{(iv)}\end{array}$

From (i),

$\quad\begin{array}{l}\displaystyle \xi=a(x)+b(x)e^{-y}.\end{array}$

In (ii),

$\quad\begin{array}{l}\displaystyle \eta_{yy}-\eta_y=-2b'e^{-y},\end{array}$

so

$\quad\begin{array}{l}\displaystyle \eta=c(x)+d(x)e^y-b'(x)e^{-y}.\end{array}$


Substitution in (iii) gives

$\quad\begin{array}{l}\displaystyle 2[d'e^y+b''e^{-y}] -2[c'+d'e^y-b''e^{-y}] -[a''+b''e^{-y}]+3Db e^{-y}=0.\end{array}$

Thus

$\quad\begin{array}{l}\displaystyle 3(b''+Db)=0, \qquad\text{(v)}\end{array}$

$\quad\begin{array}{l}\displaystyle a''+2c'=0. \qquad\text{(vi)}\end{array}$

Substitution in (iv) gives

$\quad\begin{array}{l}\displaystyle c''+d''e^y-b'''e^{-y} +D[de^y+b'e^{-y}]-2D[a'+b'e^{-y}]=0,\end{array}$

so

$\quad\begin{array}{l}\displaystyle b'''+Db'=0 \qquad\text{(redundant)},\end{array}$

$\quad\begin{array}{l}\displaystyle d''+Dd=0, \qquad\text{(vii)}\end{array}$

and

$\quad\begin{array}{l}\displaystyle c''-2Da'=0. \qquad\text{(viii)}\end{array}$

Equations (v), (vi), and (viii) give

$\quad\begin{array}{l}\displaystyle a'''+4Da'=0,\end{array}$

and

$\quad\begin{array}{l}\displaystyle c'=C_1+2Da.\end{array}$

There appear to be nine constants, so one must be lost. Write

$\quad\begin{array}{l}\displaystyle \begin{aligned} a  &=A_0+A_1f_1+A_2f_2,\\ c' &=(C_1+A_0)+A_1f_1+A_2f_2,\\ c  &=C_0+(C_1+A_0)x+A_1\int f_1\,dx+A_2\int f_2\,dx. \end{aligned}\end{array}$

Thus all differential equations of the form

$\quad\begin{array}{l}\displaystyle y''=(y')^2+d\end{array}$

have $\mathfrak{sl}(3,\mathbb R)$ symmetry and are accordingly linearisable.
Writing $D=\omega^2$, the generators are

$\quad\begin{array}{l}\displaystyle \begin{aligned} G_1&=\frac{\partial}{\partial y},\\ G_2&=\frac{\partial}{\partial x},\\ G_3&=e^{-2\omega x}\frac{\partial}{\partial x} +\omega e^{-2\omega x}\frac{\partial}{\partial y},\\ G_4&=e^{2\omega x}\frac{\partial}{\partial x} -\omega e^{2\omega x}\frac{\partial}{\partial y},\\ G_5&=e^{-y+\omega x}\frac{\partial}{\partial x} -\omega e^{-y+\omega x}\frac{\partial}{\partial y},\\ G_6&=e^{-y-\omega x}\frac{\partial}{\partial x} +\omega e^{-y-\omega x}\frac{\partial}{\partial y},\\ G_7&=e^{y-\omega x}\frac{\partial}{\partial y},\\ G_8&=e^{y+\omega x}\frac{\partial}{\partial y}. \end{aligned}\end{array}$


The set $\{G_3,G_6,G_7\}$ has the algebra $\mathcal X$:

$\quad\begin{array}{l}\displaystyle [G_3,G_6]=0, \qquad [G_6,G_7]=-G_3, \qquad [G_7,G_3]=0.\end{array}$

The standard form of $\mathcal X$ is as on page 12:

$\quad\begin{array}{l}\displaystyle [\widetilde G_1,\widetilde G_2]=0, \qquad [\widetilde G_2,\widetilde G_3]=0, \qquad [\widetilde G_3,\widetilde G_1]=\widetilde G_2,\end{array}$

with

$\quad\begin{array}{l}\displaystyle \widetilde G_1=\frac{\partial}{\partial x}, \qquad \widetilde G_2=\frac{\partial}{\partial y}, \qquad \widetilde G_3=-x\frac{\partial}{\partial y}.\end{array}$

Try

$\quad\begin{array}{l}\displaystyle G_6\longrightarrow-\widetilde G_3, \qquad G_7\longrightarrow\widetilde G_1, \qquad G_3\longrightarrow\widetilde G_2.\end{array}$

Let

$\quad\begin{array}{l}\displaystyle X=F(x,y), \qquad Y=G(x,y).\end{array}$

The mapping of $G_7$ gives

$\quad\begin{array}{l}\displaystyle e^{y-\omega x}\left[ \frac{\partial F}{\partial y}\frac{\partial}{\partial X} +\frac{\partial G}{\partial y}\frac{\partial}{\partial Y} \right] =\frac{\partial}{\partial X}.\end{array}$

Hence

$\quad\begin{array}{l}\displaystyle \frac{\partial F}{\partial y}=e^{-y+\omega x}, \qquad F=k(x)-e^{-y+\omega x},\end{array}$

and

$\quad\begin{array}{l}\displaystyle \frac{\partial G}{\partial y}=0, \qquad G=m(x).\end{array}$

The mapping of $G_6$ gives

$\quad\begin{array}{l}\displaystyle e^{-y-\omega x}\left( \frac{\partial F}{\partial x} +\omega\frac{\partial F}{\partial y} \right)=0, \qquad\text{(*)}\end{array}$

and

$\quad\begin{array}{l}\displaystyle e^{-y-\omega x}\left( \frac{\partial G}{\partial x} +\omega\frac{\partial G}{\partial y} \right)=F. \qquad\text{(†)}\end{array}$

Substitution in (*) gives

$\quad\begin{array}{l}\displaystyle k'-\omega e^{-y+\omega x}+\omega e^{-y+\omega x}=0,\end{array}$

so $k'=0$. Equation $\text{(†)}$ gives

$\quad\begin{array}{l}\displaystyle e^{-y-\omega x}m'=k-e^{-y+\omega x},\end{array}$

or

$\quad\begin{array}{l}\displaystyle m'=ke^{y+\omega x}-e^{2\omega x}.\end{array}$

Thus $k=0$, and the notes take

$\quad\begin{array}{l}\displaystyle F=-e^{-y+\omega x}, \qquad G=-e^{2\omega x}.\end{array}$


Check:

$\quad\begin{array}{l}\displaystyle \begin{aligned} Y'  &=\frac{-2\omega e^{2\omega x}} {(y'-\omega)e^{-y+\omega x}},\\[6pt] Y'' &=\frac{ -4\omega^2e^{2\omega x}(y'-\omega)e^{-y+\omega x} +2\omega e^{2\omega x} \left[y''-(y'-\omega)^2\right]e^{-y+\omega x}} {\left[(y'-\omega)e^{-y+\omega x}\right]^3}. \end{aligned}\end{array}$

The numerator is

$\quad\begin{array}{l}\displaystyle \begin{aligned} 2\omega e^{-y+3\omega x} &\left[-2\omega y'+2\omega^2+y''-(y')^2+2\omega y'-\omega^2\right]\\ &=2\omega e^{-y+3\omega x} \left[y''-(y')^2+\omega^2\right], \end{aligned}\end{array}$

as required.

The solution of $Y''=0$ is

$\quad\begin{array}{l}\displaystyle Y=A+BX.\end{array}$

The solution of $y''=(y')^2-\omega^2$ is therefore

$\quad\begin{array}{l}\displaystyle \begin{aligned} -e^{2\omega x} &=A+Be^{-y+\omega x},\\ e^{-y}         &=Ce^{\omega x}+De^{-\omega x},\\ y              &=-\log\left(Ce^{\omega x}+De^{-\omega x}\right). \end{aligned}\end{array}$

Check:

$\quad\begin{array}{l}\displaystyle \begin{aligned} \frac{y''}{(y')^2-\omega^2} &=1,\\[4pt] \frac{d\left((y')^2\right)}{(y')^2-\omega^2} &=2\,dy,\\[4pt] \log\left((y')^2-\omega^2\right) &=2y+k,\\[4pt] (y')^2 &=\omega^2+me^{2y}. \end{aligned}\end{array}$

Thus

$\quad\begin{array}{l}\displaystyle I=\int\frac{dy}{\left(\omega^2+me^{2y}\right)^{1/2}} =\int dx.\end{array}$

Let

$\quad\begin{array}{l}\displaystyle e^{-y}=u, \qquad -e^{-y}\,dy=du, \qquad dy=-\frac{du}{u}.\end{array}$

Then

$\quad\begin{array}{l}\displaystyle I =\int\frac{-u^{-1}\,du} {\left(\omega^2+mu^{-2}\right)^{1/2}} =-\int\frac{du}{\left(\omega^2u^2+m\right)^{1/2}}.\end{array}$


$\quad\begin{array}{l}\displaystyle I=-\frac1\omega \operatorname{arsinh}\left(\frac{\omega u}{m^{1/2}}\right) =x+c.\end{array}$

Therefore

$\quad\begin{array}{l}\displaystyle \begin{aligned} u &=-\frac{m^{1/2}}{\omega}\sinh(\omega x+\alpha),\\[4pt] y &=-\log\left[-\frac{m^{1/2}}{\omega} \sinh(\omega x+\alpha)\right]. \end{aligned}\end{array}$

As an aside,

$\quad\begin{array}{l}\displaystyle \int\frac{dx}{(a^2+e^{2x})^{1/2}} =\int\frac{e^{-x}\,dx}{(a^2e^{-2x}+1)^{1/2}} =\int\frac{-d(e^{-x})}{(a^2e^{-2x}+1)^{1/2}}.\end{array}$

<a id="type-iii"></a>
### Type III

$\quad\begin{array}{l}\displaystyle xy''=f(y'), \qquad f(y')=a(y')^3+b(y')^2+cy'+d.\end{array}$

There are two standard forms:

$\quad\begin{array}{l}\displaystyle \begin{aligned} xy'' &=(y')^3+cy'+d, &&a\ne0,\\ xy'' &=(y')^2+d,     &&a=0. \end{aligned}\end{array}$

Using the same type of analysis, it is found that only

$\quad\begin{array}{l}\displaystyle xy''=(y')^3+y'\end{array}$

is linearisable.

Note that the equation

<a id="sl2r-symmetry"></a>

$\quad\begin{array}{l}\displaystyle xy''=(y')^3-\frac12y'\end{array}$

does have $\mathfrak{sl}(2,\mathbb R)$ symmetry.

<a id="linearisation-of-a-system-of-equations"></a>
### Linearisation of a system of equations

<a id="system-linearisation-theorem"></a>
#### Theorem

A system of $n$ equations of the form

$\quad\begin{array}{l}\displaystyle A\mathbf u''=\mathbf f(x,\mathbf u,\mathbf u'), \qquad\text{(1)}\end{array}$

where $A$ is a constant matrix, is linearisable if it possesses the algebra
$\mathcal N$:

$\quad\begin{array}{l}\displaystyle \begin{aligned} [H,G_i]   &=0,       &&\text{(2)}\\ [G_i,G_j] &=0,       &&\text{(3)}\\ [G_i,X_j] &=0,       &&\text{(4)}\\ [H,X_i]   &=G_i.     &&\text{(5)} \end{aligned}\end{array}$

where $\{H,G_i,X_i\mid i=1,\ldots,n\}$ are symmetries of (1).


<a id="system-linearisation-proof"></a>
#### Proof

We cannot have both

$\quad\begin{array}{l}\displaystyle H=a_iG_i \qquad\text{and}\qquad X_i=b_{ij}G_j\end{array}$

for suitable functions $a_i$ and $b_{ij}$. For, from (2),

$\quad\begin{array}{l}\displaystyle \begin{aligned} [H,G_j] &=[a_iG_i,G_j]\\ &=a_i[G_i,G_j]-(G_ja_i)G_i\\ &=-(G_ja_i)G_i\\ &=0, \end{aligned}\end{array}$

and, from (4),

$\quad\begin{array}{l}\displaystyle \begin{aligned} [G_i,X_j] &=[G_i,b_{jk}G_k]\\ &=[G_i,G_k]b_{jk}+(G_ib_{jk})G_k\\ &=(G_ib_{jk})G_k\\ &=0. \end{aligned}\end{array}$

Then (5) is

$\quad\begin{array}{l}\displaystyle \begin{aligned} [H,X_j] &=[a_iG_i,b_{jk}G_k]\\ &=a_i[G_i,G_k]b_{jk} +a_i(G_ib_{jk})G_k -b_{jk}(G_ka_i)G_i\\ &=0, \end{aligned}\end{array}$

which is a contradiction.

We assume that $H\ne a_iG_i$. Then there exist coordinates $x,u_i$ such that

$\quad\begin{array}{l}\displaystyle H=\frac{\partial}{\partial x}, \qquad G_j=C_{ij}\frac{\partial}{\partial u_i}.\end{array}$

Since

$\quad\begin{array}{l}\displaystyle [H,G_j] =\frac{\partial C_{ij}}{\partial x} \frac{\partial}{\partial u_i}=0,\end{array}$

it follows that

$\quad\begin{array}{l}\displaystyle \frac{\partial C_{ij}}{\partial x}=0,\end{array}$

so $C_{ij}=C_{ij}(\mathbf u)$. Since the $G_i$ are linearly independent, the
matrix $[C_{ij}]$ has rank $n$, and so its inverse exists, perhaps restricted
to a subset of $\mathbb R^n$.


If we change variables from $u_i$ to $v_i$ so that

$\quad\begin{array}{l}\displaystyle v_i=d_i(\mathbf u),\end{array}$

then

$\quad\begin{array}{l}\displaystyle \begin{aligned} G_i &=C_{ik}\frac{\partial}{\partial u_k}\\ &=C_{ik}\frac{\partial d_j}{\partial u_k} \frac{\partial}{\partial v_j}\\ &=\frac{\partial}{\partial v_i}, \end{aligned}\end{array}$

provided

$\quad\begin{array}{l}\displaystyle C_{ik}\frac{\partial d_j}{\partial u_k}=\delta_{ij},\end{array}$

<a id="jacobian"></a>
i.e. the Jacobian of the transformation is the inverse of $[C_{ij}]$. Thus
the transformation is not degenerate. Can functions $d_j(\mathbf u)$ exist
with the property

$\quad\begin{array}{l}\displaystyle \frac{\partial d_i}{\partial u_j}=(C^{-1})_{ij}, \qquad C=[C_{ij}]?\end{array}$

The requirement of consistency, if the integration is to be carried out, is

$\quad\begin{array}{l}\displaystyle \frac{\partial}{\partial u_k}\frac{\partial d_i}{\partial u_j} =\frac{\partial}{\partial u_j}\frac{\partial d_i}{\partial u_k}.\end{array}$

Now

$\quad\begin{array}{l}\displaystyle \frac{\partial C^{-1}}{\partial u_i} =-C^{-1}\frac{\partial C}{\partial u_i}C^{-1}.\end{array}$

The differentiability of $C^{-1}$ depends upon the differentiability of $C$.
Since we need $C''$ in $G^{(2)}$, $C$ must be twice differentiable almost
everywhere, and so $d_i$ is three times differentiable and the mixed
derivatives are equal. Thus a transformation exists which casts $G_i$ in the
form

$\quad\begin{array}{l}\displaystyle G_i=\frac{\partial}{\partial u_i}.\end{array}$

Had

$\quad\begin{array}{l}\displaystyle X_i=b_{ij}\frac{\partial}{\partial u_j},\end{array}$

then

$\quad\begin{array}{l}\displaystyle \begin{aligned} [G_i,X_j] &=\left[\frac{\partial}{\partial u_i}, b_{jk}\frac{\partial}{\partial u_k}\right]\\ &=\frac{\partial b_{jk}}{\partial u_i} \frac{\partial}{\partial u_k}=0. \end{aligned}\end{array}$

Hence

$\quad\begin{array}{l}\displaystyle \frac{\partial b_{jk}}{\partial u_i}=0, \qquad b_{jk}=B_{jk}(x).\end{array}$


From (5),

$\quad\begin{array}{l}\displaystyle \begin{aligned} [H,X_i] &=\left[\frac{\partial}{\partial x}, B_{ij}\frac{\partial}{\partial u_j}\right]\\ &=\frac{dB_{ij}}{dx}\frac{\partial}{\partial u_j} =\frac{\partial}{\partial u_i}. \end{aligned}\end{array}$

Therefore

$\quad\begin{array}{l}\displaystyle B'_{ij}=\delta_{ij}, \qquad B_{ij}(x)=\delta_{ij}x+K_{ij}.\end{array}$

Thus

$\quad\begin{array}{l}\displaystyle X_i=x\frac{\partial}{\partial u_i} +K_{ij}\frac{\partial}{\partial u_j} =x\frac{\partial}{\partial u_i}+K_{ij}G_j.\end{array}$

Drop the second term and take

$\quad\begin{array}{l}\displaystyle X_i=x\frac{\partial}{\partial u_i}.\end{array}$

We have the standard form

$\quad\begin{array}{l}\displaystyle H=\frac{\partial}{\partial x}, \qquad G_i=\frac{\partial}{\partial u_i}, \qquad X_i=x\frac{\partial}{\partial u_i}.\end{array}$

In the coordinates $(x,u_i)$, the differential equation is

$\quad\begin{array}{l}\displaystyle A\mathbf u''=\mathbf f(x,\mathbf u,\mathbf u').\end{array}$

Invariance under $H$, $G_i$, and $X_i$ implies, respectively,

$\quad\begin{array}{l}\displaystyle \frac{\partial\mathbf f}{\partial x}=0, \qquad \frac{\partial\mathbf f}{\partial\mathbf u}=0, \qquad \frac{\partial\mathbf f}{\partial\mathbf u'}=0.\end{array}$

For example, consider

$\quad\begin{array}{l}\displaystyle u''+\frac{u(v')^2}{v^2}=0,\end{array}$

$\quad\begin{array}{l}\displaystyle v''+\frac{2u'v'}{u}-\frac{(v')^2}{v}=0.\end{array}$

Let

$\quad\begin{array}{l}\displaystyle G=\tau(u,v,x)\frac{\partial}{\partial x} +\xi(u,v,x)\frac{\partial}{\partial u} +\eta(u,v,x)\frac{\partial}{\partial v}.\end{array}$

Then

$\quad\begin{array}{l}\displaystyle \begin{aligned} G^{(2)}={}&G +(\xi'-u'\tau')\frac{\partial}{\partial u'} +(\eta'-v'\tau')\frac{\partial}{\partial v'}\\ &+(\xi''-u'\tau''-2u''\tau')\frac{\partial}{\partial u''} +(\eta''-v'\tau''-2v''\tau')\frac{\partial}{\partial v''}. \end{aligned}\end{array}$

For example,

$\quad\begin{array}{l}\displaystyle \eta'=\frac{\partial\eta}{\partial x} +u'\frac{\partial\eta}{\partial u} +v'\frac{\partial\eta}{\partial v},\end{array}$

and

$\quad\begin{array}{l}\displaystyle \begin{aligned} \eta''={}& \frac{\partial^2\eta}{\partial x^2} +2u'\frac{\partial^2\eta}{\partial u\partial x} +2v'\frac{\partial^2\eta}{\partial v\partial x} +(u')^2\frac{\partial^2\eta}{\partial u^2}\\ &+2u'v'\frac{\partial^2\eta}{\partial u\partial v} +(v')^2\frac{\partial^2\eta}{\partial v^2} +u''\frac{\partial\eta}{\partial u} +v''\frac{\partial\eta}{\partial v}. \end{aligned}\end{array}$


The action of $G^{(2)}$ on the two equations, followed by separation by powers
of $u'$ and $v'$, leads to a system of fifteen independent partial differential
equations for $\tau$, $\xi$, and $\eta$, all linear. The solutions give

$\quad\begin{array}{l}\displaystyle \begin{aligned} G_1={}&x\frac{\partial}{\partial x} +2u\frac{\partial}{\partial u} +2v\frac{\partial}{\partial v},\\ G_2={}&\frac{\partial}{\partial x},\\ G_3={}&\frac1v\frac{\partial}{\partial u} +\frac1u\frac{\partial}{\partial v},\\ G_4={}&x\frac{\partial}{\partial x},\\ G_5={}&x^2\frac{\partial}{\partial x} +2xu\frac{\partial}{\partial u} +2xv\frac{\partial}{\partial v},\\ G_6={}&\frac{x}{v}\frac{\partial}{\partial u} +\frac{x}{u}\frac{\partial}{\partial v},\\ G_7={}&xuv\frac{\partial}{\partial x} +2u^2v\frac{\partial}{\partial u} +2uv^2\frac{\partial}{\partial v},\\ G_8={}&uv\frac{\partial}{\partial x},\\ G_9={}&x\frac{\partial}{\partial x} +2u\frac{\partial}{\partial u} -2v\frac{\partial}{\partial v},\\ G_{10}={}&v\frac{\partial}{\partial u} -\frac{v^2}{u}\frac{\partial}{\partial v},\\ G_{11}={}&x^2\frac{\partial}{\partial x} +2ux\frac{\partial}{\partial u} -2vx\frac{\partial}{\partial v},\\ G_{12}={}&vx\frac{\partial}{\partial u} -\frac{v^2x}{u}\frac{\partial}{\partial v},\\ G_{13}={}&\frac{xv}{u}\frac{\partial}{\partial x} +\frac{2u^2}{v}\frac{\partial}{\partial u} -2u\frac{\partial}{\partial v},\\ G_{14}={}&\frac{u}{v}\frac{\partial}{\partial x},\\ G_{15}={}&u\left(v^2+\frac1{v^2}\right) \frac{\partial}{\partial u} -v\left(u^2-\frac1{v^2}\right) \frac{\partial}{\partial v}. \end{aligned}\end{array}$

The system is obviously linearisable, as the fifteen-element algebra is
$\mathfrak{sl}(4,\mathbb R)$, the algebra of the two-dimensional free particle
with equation of motion

<a id="sl4r-symmetry"></a>

$\quad\begin{array}{l}\displaystyle \mathbf r''=0, \qquad \mathbf r\in\mathbb R^2.\end{array}$

Note that $\mathfrak{sl}(4,\mathbb R)$ is not a prerequisite for
linearisation. This is because the system

$\quad\begin{array}{l}\displaystyle \mathbf r''=A\mathbf r, \qquad \mathbf r\in\mathbb R^2,\end{array}$

need not possess $\mathfrak{sl}(4,\mathbb R)$ symmetry.


From the symmetries, one looks to see if the algebra $\mathcal N$ is present.
Recall that $\mathcal N$ has $H,G_i,X_i$ such that

$\quad\begin{array}{l}\displaystyle [H,G_i]=0, \qquad [G_i,G_j]=0,\end{array}$

and

$\quad\begin{array}{l}\displaystyle [G_i,X_j]=0, \qquad [H,X_i]=G_i.\end{array}$

In standard form,

$\quad\begin{array}{l}\displaystyle H=\frac{\partial}{\partial x}, \qquad G_i=\frac{\partial}{\partial u_i}, \qquad X_i=x\frac{\partial}{\partial u_i}.\end{array}$

Noting that

$\quad\begin{array}{l}\displaystyle [G_2,G_3]=0, \qquad [G_2,G_6]=G_3, \qquad [G_3,G_6]=0,\end{array}$

and

$\quad\begin{array}{l}\displaystyle [G_2,G_{10}]=0, \qquad [G_2,G_{12}]=G_{10}, \qquad [G_{10},G_{12}]=0,\end{array}$

we check to see if this is the algebra $\mathcal N$:

$\quad\begin{array}{l}\displaystyle \begin{aligned} [G_3,G_{10}] &=\left[ \frac1v\frac{\partial}{\partial u} +\frac1u\frac{\partial}{\partial v}, v\frac{\partial}{\partial u} -\frac{v^2}{u}\frac{\partial}{\partial v} \right]\\ &=\left(\frac1u-\frac1u\right)\frac{\partial}{\partial u} +\left(\frac{v}{u^2}-\frac{2v}{u^2}+\frac{v}{u^2}\right) \frac{\partial}{\partial v}\\ &=0. \end{aligned}\end{array}$

Likewise,

$\quad\begin{array}{l}\displaystyle [G_6,G_{10}]=0, \qquad [G_6,G_{12}]=0, \qquad [G_3,G_{12}]=0.\end{array}$

Thus we do have $\mathcal N$. Identify

$\quad\begin{array}{l}\displaystyle \begin{aligned} H&=\frac{\partial}{\partial x},\\ G_1&=\frac1v\frac{\partial}{\partial u} +\frac1u\frac{\partial}{\partial v},\\ G_2&=v\frac{\partial}{\partial u} -\frac{v^2}{u}\frac{\partial}{\partial v},\\ X_1&=xG_1,\\ X_2&=xG_2. \end{aligned}\end{array}$

Seek a transformation from $x,u,v$ to $X,U,V$ such that

$\quad\begin{array}{l}\displaystyle H=\frac{\partial}{\partial X}, \qquad G_1=\frac{\partial}{\partial U}, \qquad G_2=\frac{\partial}{\partial V},\end{array}$

$\quad\begin{array}{l}\displaystyle X_1=X\frac{\partial}{\partial U}, \qquad X_2=X\frac{\partial}{\partial V}.\end{array}$

Let

$\quad\begin{array}{l}\displaystyle X=x, \qquad U=F(u,v), \qquad V=G(u,v),\end{array}$

having already realised that $x$ is an appropriate variable.


The derivatives transform according to

$\quad\begin{array}{l}\displaystyle \frac{\partial}{\partial u} \longrightarrow \frac{\partial F}{\partial u}\frac{\partial}{\partial U} +\frac{\partial G}{\partial u}\frac{\partial}{\partial V},\end{array}$

and

$\quad\begin{array}{l}\displaystyle \frac{\partial}{\partial v} \longrightarrow \frac{\partial F}{\partial v}\frac{\partial}{\partial U} +\frac{\partial G}{\partial v}\frac{\partial}{\partial V}.\end{array}$

Thus $G_1$ gives

$\quad\begin{array}{l}\displaystyle \frac1v\frac{\partial F}{\partial u} +\frac1u\frac{\partial F}{\partial v}=1, \qquad\text{(i)}\end{array}$

and

$\quad\begin{array}{l}\displaystyle \frac1v\frac{\partial G}{\partial u} +\frac1u\frac{\partial G}{\partial v}=0. \qquad\text{(ii)}\end{array}$

From $G_2$ we obtain

$\quad\begin{array}{l}\displaystyle v\frac{\partial F}{\partial u} -\frac{v^2}{u}\frac{\partial F}{\partial v}=0, \qquad\text{(iii)}\end{array}$

and

$\quad\begin{array}{l}\displaystyle v\frac{\partial G}{\partial u} -\frac{v^2}{u}\frac{\partial G}{\partial v}=1. \qquad\text{(iv)}\end{array}$

For (ii),

$\quad\begin{array}{l}\displaystyle \frac{du}{1/v}=\frac{dv}{1/u}, \qquad w_1=\frac{u}{v},\end{array}$

so

$\quad\begin{array}{l}\displaystyle G=g\left(\frac uv\right).\end{array}$

In (iv),

$\quad\begin{array}{l}\displaystyle v\frac1v g' -\frac{v^2}{u}\left(-\frac{u}{v^2}\right)g'=1,\end{array}$

so

$\quad\begin{array}{l}\displaystyle g'=\frac12,\end{array}$

and

$\quad\begin{array}{l}\displaystyle G=\frac12\frac uv,\end{array}$

to within a forgettable constant.

For (i),

$\quad\begin{array}{l}\displaystyle \frac{du}{1/v}=\frac{dv}{1/u}=\frac{dF}{1}.\end{array}$

The first two terms give

$\quad\begin{array}{l}\displaystyle w_1=\frac uv.\end{array}$

The first and third terms give

$\quad\begin{array}{l}\displaystyle u\,du=dF.\end{array}$

Hence

$\quad\begin{array}{l}\displaystyle w_2=\frac12uv-F,\end{array}$

and

$\quad\begin{array}{l}\displaystyle F=\frac12uv+f\left(\frac uv\right).\end{array}$

In (iii),

$\quad\begin{array}{l}\displaystyle v\left(\frac12v+\frac1v f'\right) -\frac{v^2}{u}\left(\frac12u-\frac{u}{v^2}f'\right)=0.\end{array}$

Therefore

$\quad\begin{array}{l}\displaystyle 2f'=0,\end{array}$

so $f'=0$, to within a forgettable constant, and

$\quad\begin{array}{l}\displaystyle F=\frac12uv.\end{array}$

Thus

$\quad\begin{array}{l}\displaystyle V=\frac12\frac uv, \qquad U=\frac12uv.\end{array}$


$\quad\begin{array}{l}\displaystyle \begin{aligned} U' &=\frac12u'v+\frac12uv',\\[4pt] U'' &=\frac12u''v+u'v'+\frac12uv''\\ &=\frac12\left[-\frac{u(v')^2}{v^2}\right]v+u'v' +\frac12u\left[ \frac{(v')^2}{v}-\frac{2u'v'}u \right]\\ &=0,\\[4pt] V'' &=0. \end{aligned}\end{array}$

Therefore

$\quad\begin{array}{l}\displaystyle \begin{aligned} U &=A_0+A_1x,\\ V &=B_0+B_1x, \end{aligned}\end{array}$

giving

$\quad\begin{array}{l}\displaystyle \begin{aligned} u^2 &=4(A_0+A_1x)(B_0+B_1x),\\[4pt] v^2 &=\frac{A_0+A_1x}{B_0+B_1x}, \end{aligned}\end{array}$

which is the general solution since there are four arbitrary constants.

---

<nav aria-label="Section navigation" style="display: grid; grid-template-columns: minmax(0, 1fr) auto; column-gap: 2em; align-items: start;">
<div style="display: grid; grid-template-columns: 6em minmax(0, 1fr); row-gap: 0.25em;">
<span>NEXT:</span><a href="03-lie-theory-of-extended-group.md">Lie Theory of Extended Group</a>
<span>PREVIOUS:</span><a href="01-contents.md">Contents</a>
</div>
<a href="05-index.md" style="justify-self: end; text-align: right;">INDEX</a>
</nav>
