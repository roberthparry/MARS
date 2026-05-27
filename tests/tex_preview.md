# TeX Preview Experiment

If your Markdown viewer supports maths, the blocks below should render as
typeset mathematics rather than raw TeX source.

## expr

<table style="border-collapse: collapse; border: none;">
  <tr>
    <td style="padding-left: 2em; text-align: left; border: none;">

$$
\left\{ x_{0} \;\middle|\; x_{0} = 42 \right\}
$$

  </td>
  </tr>
</table>

<table style="border-collapse: collapse; border: none;">
  <tr>
    <td style="padding-left: 2em; text-align: left; border: none;">

$$
\left\{ \exp(\sin(x_{0}y_{1})) + x_{0} \cdot \ln(y_{1}) \;\middle|\; x_{0} = 1, y_{1} = 2 \right\}
$$

  </td>
  </tr>
</table>

<table style="border-collapse: collapse; border: none;">
  <tr>
    <td style="padding-left: 2em; text-align: left; border: none;">

$$ 
\left\{ \ln(\frac{x_{0}^{2} + y_{1}^{2}}{y_{1} + 1}) \;\middle|\; x_{0} = 2, y_{1} = 3 \right\}
$$

  </td>
  </tr>
</table>

## matrix

<table style="border-collapse: collapse; border: none;">
  <tr>
    <td style="padding-left: 2em; text-align: left; border: none;">

$$
\begin{bmatrix}1 & 2 \\ 3 & 4\end{bmatrix}
$$

  </td>
  </tr>
</table>

<table style="border-collapse: collapse; border: none;">
  <tr>
    <td style="padding-left: 2em; text-align: left; border: none;">

$$ 
\left\{ \begin{bmatrix}\sin(x_{0}) & \exp(c_{1}) \\ \ln(x_{0}) & c_{1}^{2}\end{bmatrix} \;\middle|\; x_{0} = 2, c_{1} = 5 \right\}
$$

  </td>
  </tr>
</table>

<table style="border-collapse: collapse; border: none;">
  <tr>
    <td style="padding-left: 2em; text-align: left; border: none;">

$$ 
\begin{bmatrix}\sin(x_{0}) & \exp(c_{1}) \\ \ln(x_{0}) & c_{1}^{2}\end{bmatrix}
$$

  </td>
  </tr>
</table>

## layout experiments

These are here to see whether the Markdown previewer honours any surrounding
layout for display maths.

### blockquote

> $$
> \left\{ \exp(\sin(x_{0}y_{1})) + x_{0} \cdot \ln(y_{1}) \;\middle|\; x_{0} = 1, y_{1} = 2 \right\}
> $$

### list indent

> $$
> \begin{bmatrix}\sin(x_{0}) & \exp(c_{1}) \\ \ln(x_{0}) & c_{1}^{2}\end{bmatrix}
> $$

### html table

<table style="border-collapse: collapse; border: none;">
  <tr>
    <td style="padding-left: 2em; text-align: left; border: none;">

$$ 
\left\{ \begin{bmatrix}\sin(x_{0}) & \exp(c_{1}) \\ \ln(x_{0}) & c_{1}^{2}\end{bmatrix} \;\middle|\; x_{0} = 2, c_{1} = 5 \right\}
$$

  </td>
  </tr>
</table>

### inline fallback

This is inline maths, so it should flow with the text rather than centring:
$\left\{ x_{0} \;\middle|\; x_{0} = 42 \right\}$.
