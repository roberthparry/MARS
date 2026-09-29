# TeX Preview Experiment

If your Markdown viewer supports maths, the blocks below should render as
typeset mathematics rather than raw TeX source.

## expr

<table style="border-collapse: collapse; border: none;">
  <tr>
    <td style="padding-left: 2em; text-align: left; border: none;">

$\quad\begin{array}{l}\displaystyle \left\{ x_{0} \;\middle|\; x_{0} = 42 \right\}\end{array}$

  </td>
  </tr>
</table>

<table style="border-collapse: collapse; border: none;">
  <tr>
    <td style="padding-left: 2em; text-align: left; border: none;">

$\quad\begin{array}{l}\displaystyle \left\{ \exp(\sin(x_{0}y_{1})) + x_{0} \cdot \ln(y_{1}) \;\middle|\; x_{0} = 1, y_{1} = 2 \right\}\end{array}$

  </td>
  </tr>
</table>

<table style="border-collapse: collapse; border: none;">
  <tr>
    <td style="padding-left: 2em; text-align: left; border: none;">

$\quad\begin{array}{l}\displaystyle \left\{ \ln(\frac{x_{0}^{2} + y_{1}^{2}}{y_{1} + 1}) \;\middle|\; x_{0} = 2, y_{1} = 3 \right\}\end{array}$

  </td>
  </tr>
</table>

## matrix

<table style="border-collapse: collapse; border: none;">
  <tr>
    <td style="padding-left: 2em; text-align: left; border: none;">

$\quad\begin{array}{l}\displaystyle \begin{bmatrix}1 & 2 \\ 3 & 4\end{bmatrix}\end{array}$

  </td>
  </tr>
</table>

<table style="border-collapse: collapse; border: none;">
  <tr>
    <td style="padding-left: 2em; text-align: left; border: none;">

$\quad\begin{array}{l}\displaystyle \left\{ \begin{bmatrix}\sin(x_{0}) & \exp(c_{1}) \\ \ln(x_{0}) & c_{1}^{2}\end{bmatrix} \;\middle|\; x_{0} = 2, c_{1} = 5 \right\}\end{array}$

  </td>
  </tr>
</table>

<table style="border-collapse: collapse; border: none;">
  <tr>
    <td style="padding-left: 2em; text-align: left; border: none;">

$\quad\begin{array}{l}\displaystyle \begin{bmatrix}\sin(x_{0}) & \exp(c_{1}) \\ \ln(x_{0}) & c_{1}^{2}\end{bmatrix}\end{array}$

  </td>
  </tr>
</table>

## layout experiments

These are here to see whether the Markdown previewer honours any surrounding
layout for display maths.

### blockquote

>
> $\quad\begin{array}{l}\displaystyle \left\{ \exp(\sin(x_{0}y_{1})) + x_{0} \cdot \ln(y_{1}) \;\middle|\; x_{0} = 1, y_{1} = 2 \right\}\end{array}$
>

### list indent

>
> $\quad\begin{array}{l}\displaystyle \begin{bmatrix}\sin(x_{0}) & \exp(c_{1}) \\ \ln(x_{0}) & c_{1}^{2}\end{bmatrix}\end{array}$
>

### html table

<table style="border-collapse: collapse; border: none;">
  <tr>
    <td style="padding-left: 2em; text-align: left; border: none;">

$\quad\begin{array}{l}\displaystyle \left\{ \begin{bmatrix}\sin(x_{0}) & \exp(c_{1}) \\ \ln(x_{0}) & c_{1}^{2}\end{bmatrix} \;\middle|\; x_{0} = 2, c_{1} = 5 \right\}\end{array}$

  </td>
  </tr>
</table>

### inline fallback

This is inline maths, so it should flow with the text rather than centring:
$\left\{ x_{0} \;\middle|\; x_{0} = 42 \right\}$.
