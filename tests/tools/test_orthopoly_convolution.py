"""Polynomial conventions and independently normalised convolution identities."""

import cmath
import math
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import mars_lab


def hermite(n, x):
    previous, current = 1, 2*x
    if n == 0:
        return previous
    for k in range(1, n):
        previous, current = current, 2*x*current - 2*k*previous
    return current


class PolynomialConvolutionTests(unittest.TestCase):
    def fields(self, source, wrt="x", action="evaluate"):
        fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, wrt, action)
        self.assertEqual(code, 0, raw)
        return fields

    def value(self, source, wrt="x"):
        return complex(self.fields(source, wrt)["value"].replace(" ", "").replace("i", "j"))

    def test_names_and_harmonic_compatibility(self):
        for name in ("Tn", "chebyshev_t", "ChebyshevT"):
            self.assertEqual(self.value(name+"(3,2)"), 26)
        for name in ("ℋ", "hermite_h", "HermiteH"):
            self.assertEqual(self.value(name+"(3,2)"), 40)
            self.assertEqual(self.value("{x | x="+name+"(3,2)}"), 40)
        self.assertEqual(self.value("{x | x=Tn(3,2)}"), 26)
        for source in ("T_3(2)", "T₃(2)", "ℋ_3(2)", "ℋ₃(2)"):
            self.assertEqual(self.value(source), 26 if source.startswith("T") else 40)
        self.assertAlmostEqual(self.value("Hn(3,2)").real, 20/3)
        self.assertIn(r"\mathcal{H}", self.fields("ℋ(n,x)")["tex"])
        for source in ("Tn(n,x)", "Un(n,x)", "ℋ(n,x)"):
            fields = self.fields(source)
            self.assertEqual(self.fields(fields["expression"])["tex"], fields["tex"])

    def test_polynomials_values_and_calculus(self):
        for n in range(7):
            for x in (-1, -0.3, 0, 1, 2):
                with self.subTest(n=n, x=x):
                    self.assertAlmostEqual(self.value(f"ℋ({n},{x})").real, hermite(n, x), places=10)
                    expected_t = math.cos(n*math.acos(x)) if abs(x) <= 1 else math.cosh(n*math.acosh(x))
                    self.assertAlmostEqual(self.value(f"Tn({n},{x})").real, expected_t, places=9)
        self.assertEqual(self.value("ℋ(3,i)"), -20j)
        for source, expected in (("{Tn(3,x)|x=2}", 45), ("{Un(3,x)|x=1}", 20),
                                 ("{ℋ(3,x)|x=2}", 84), ("{ℋ(0,x)|x=0}", 0),
                                 ("{x+Un(n,x+j)|x=1; n=3; j=0}", 21)):
            result = self.fields(source, action="derivative")
            self.assertAlmostEqual(float(result["derivative_value"]), expected)
        for source in ("Tn(3,x)", "Un(3,x)", "ℋ(3,x)"):
            result = self.fields(source, action="integral")
            self.assertIn("integral", result)
            self.assertNotIn("No integral", result.get("integral", ""))

    def test_finite_polynomial_sums(self):
        self.assertEqual(self.value("sum(k,0,3,Tn(k,2))"), 36)
        self.assertEqual(self.value("sum(k,0,3,ℋ(k,2))"), 59)

    def test_hermite_gaussian_both_directions(self):
        for inverse in (False, True):
            operator, source, target = ("@Finv", "ω", "t") if inverse else ("@F", "t", "ω")
            for n in (0, 1, 3, 6):
                for a in (1, 2, -2):
                    for point in (-0.6, 0, 1.1):
                        body = f"ℋ(n,({a})*{source})*exp(-(({a})*{source})^2/2)"
                        expression = "{"+operator+"{"+body+"} | "+f"n={n}; {target}={point}"+"}"
                        value = self.value(expression, target)
                        coefficient = (1j**n)/math.sqrt(2*math.pi) if inverse else math.sqrt(2*math.pi)*(-1j)**n
                        expected = coefficient/abs(a)*hermite(n, point/a)*math.exp(-(point/a)**2/2)
                        self.assertLess(abs(value-expected), 1e-9, expression)
        fields = self.fields("@F{ℋ(n,t)*exp(-t^2/2)}", "ω")
        self.assertNotIn("\ufffd", fields["function"])
        self.assertEqual(self.fields(fields["expression"], "ω")["tex"], fields["tex"])

    def test_bessel_uses_chebyshev(self):
        fields = self.fields("@F{J_n(t)}", "ω")
        self.assertIn("Tn(", fields["expression"])
        self.assertNotIn("acos", fields["expression"])
        for inverse in (False, True):
            operator, source, target = ("@Finv", "ω", "t") if inverse else ("@F", "t", "ω")
            for n in (0, 1, 3):
                expression = f"{operator}{{Tn({n},{source})*rect({source}/2)/sqrt(1-{source}^2)}}"
                fields = self.fields(expression, target)
                self.assertNotIn("fourier(", fields["function"], fields)
                actual = self.value("{"+expression+" | "+target+"=0.7}", target)
                bessel = sum((-1)**k*(0.7/2)**(2*k+n)/(math.factorial(k)*math.factorial(k+n)) for k in range(18))
                expected = (1j**n/2 if inverse else math.pi*(-1j)**n)*bessel
                self.assertLess(abs(actual-expected), 1e-10)

    def test_convolution_theorems(self):
        for operator, source, target in (("@F", "t", "ω"), ("@Finv", "ω", "t")):
            expression = operator+"{convolve(exp(-"+source+"^2),exp(-"+source+"^2),"+source+")}"
            for point in (-1, 0, 0.5):
                actual = self.value("{"+expression+" | "+target+"="+str(point)+"}", target)
                expected = math.pi*math.exp(-point**2/2) if operator == "@F" else 0.5*math.exp(-point**2/2)
                self.assertAlmostEqual(actual.real, expected, places=11)
        for source in ("@F{convolve(f(t),g(t),t)}", "@F{f(t)*g(t)}",
                       "@Finv{f(ω)*g(ω)}", "@L{causal_convolve(f(t),g(t),t)}",
                       "@Linv{F(s)*G(s)}"):
            result = self.fields(source)
            self.assertEqual(self.fields(result["expression"])["tex"], result["tex"])
        inverse_product = self.fields("@Finv{f(ω)*g(ω)}")
        self.assertIn("convolve(", inverse_product["function"])
        self.assertNotIn("@pi", inverse_product["function"])
        for point in (0.5, 1, 3):
            actual = self.value("{@L{causal_convolve(t,t,t)} | s="+str(point)+"}", "s")
            self.assertAlmostEqual(actual.real, 1/point**4, places=10)

    def test_convolution_simplification_and_capture(self):
        for point in (-1.2, 0, 0.2, 1):
            self.assertAlmostEqual(self.value("{convolve(rect(t),rect(t),t) | t="+str(point)+"}", "t").real,
                                   max(0, 1-abs(point)), places=12)
            self.assertAlmostEqual(self.value("{convolve(delta(t-1),sin(t),t) | t="+str(point)+"}", "t").real,
                                   math.sin(point-1), places=12)
        self.assertAlmostEqual(self.value("{causal_convolve(t,t,t)|t=2}", "t").real, 4/3)
        self.assertAlmostEqual(self.value("{causal_convolve(t+τ,t,t)|t=2; τ=3}", "t").real, 22/3)
        self.assertAlmostEqual(self.value("{convolve(exp(-t^2),exp(-t^2),t)|t=1}", "t").real,
                               math.sqrt(math.pi/2)*math.exp(-0.5), places=12)
        derivative = self.fields("convolve(f(t),g(t),t)", "t", "derivative")
        self.assertIn("derivative", derivative)


class ZZPolynomialConvolutionReadmeExamples(unittest.TestCase):
    def test_readme_polynomial_and_convolution_examples(self):
        examples = (("Tn(3,2)", 26), ("Un(3,2)", 56), ("ℋ(3,2)", 40),
                    ("sum(k,0,3,Tn(k,2))", 36), ("sum(k,0,3,ℋ(k,2))", 59),
                    ("{convolve(rect(t),rect(t),t)|t=1/4}", 0.75),
                    ("{causal_convolve(t,t,t)|t=2}", 4/3),
                    ("{@L{causal_convolve(t,t,t)}|s=2}", 1/16))
        for source, expected in examples:
            fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, "t", "evaluate")
            self.assertEqual(code, 0, raw)
            self.assertAlmostEqual(float(fields["value"]), expected, places=12)


if __name__ == "__main__":
    unittest.main()
