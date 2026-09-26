"""Inverse hyperbolic sine pairs and independent modified-Bessel checks."""

import cmath
import math
import unittest
from decimal import Decimal, localcontext

from test_fourier_tanh import algebra, fields, number, simpson, mars_lab


def k_integral(order, argument):
    return simpson(lambda t: cmath.exp(-argument*math.cosh(t))*cmath.cosh(order*t), 0, 8)


class AsinhFourierTests(unittest.TestCase):
    def test_basic_pair_and_numeric_domain(self):
        result = fields("@F{asinh(x)}")
        self.assertNotIn("fourier(", result["function"])
        self.assertIn("k ≠ 0", result["expression"])
        self.assertIn("k ∈ ℝ", result["expression"])
        self.assertIn("k != 0", result["function"])
        self.assertIn("besselk(0,", result["function"])
        self.assertIn("K_{0}", result["tex"])
        self.assertIn("distributional", result["value_note"])
        for card in ("expression", "function", "tex"):
            self.assertNotIn("principal value", result[card])
            self.assertNotIn("PV(", result[card])
        for operator, factor in (("Fourier", -2j), ("InverseFourier", 1j/math.pi)):
            for k in (-1.3, -0.4, 0.7, 2):
                value = number(fields("{"+operator+f"(asinh(x),x,k) | k={k}"+"}"))
                self.assertLess(abs(value-factor*k_integral(0, abs(k))/k), 2e-12)
        for k in ("0", "i"):
            self.assertEqual(fields("{@F{asinh(x)} | k="+k+"}")["value"], "NAN")

    def test_copied_spectra_both_directions(self):
        for argument in ("x", "2*x+1", "-2*x+1", "x/2-1/3"):
            for forward, inverse in (("Fourier", "InverseFourier"), ("InverseFourier", "Fourier")):
                spectrum = fields(f"{forward}(asinh({argument}),x,k)")
                for copied in (algebra(spectrum), spectrum["expression"]):
                    result = fields(f"{inverse}("+copied+",k,x)", "x")
                    self.assertNotIn("fourier(", result["function"])
                    self.assertNotIn("x != 0", result["function"])
                    for x in (-0.4, 0, 0.7):
                        bound = result["expression"].replace("x = NAN", f"x = {x}")
                        expected = fields("{asinh("+argument+") | x="+str(x)+"}", "x")
                        self.assertLess(abs(number(fields(bound, "x"))-number(expected)), 2e-12)

    def test_symbolic_affine_pair(self):
        spectrum = fields("Fourier(asinh(a*x+b),x,k)")
        for condition in ("a ∈ ℝ", "b ∈ ℝ", "a ≠ 0", "k ≠ 0"):
            self.assertIn(condition, spectrum["expression"])
        restored = fields("InverseFourier("+spectrum["expression"]+",k,x)", "x")
        self.assertNotIn("fourier(", restored["function"])
        for a in (-2, 0.5, 2):
            copied = restored["expression"].replace("x = NAN", "x = 0.3")
            copied = copied.replace("a = NAN", f"a = {a}").replace("b = NAN", "b = 0.2")
            self.assertLess(abs(number(fields(copied, "x"))-math.asinh(a*0.3+0.2)), 2e-12)

    def test_direct_formula_modulation_and_rejections(self):
        for spectrum in ("-2i*K0(abs(k))/k", "-2*i*besselk(0,abs(k))*k^(-1)",
                         "-2i*K_0(abs(k))/k"):
            result = fields("@Finv{"+spectrum+"}", "x")
            self.assertEqual(algebra(result), "asinh(x)")
        for x in (-0.5, 0, 0.7):
            result = fields("{InverseFourier(exp(3*i*k)*K0(2*abs(k))/k,k,x) | x="+str(x)+"}", "x")
            self.assertLess(abs(number(result)-0.5j*math.asinh((x+3)/2)), 2e-12)
        for source in ("@F{asinh(i*x)}", "@F{asinh(x+i)}", "@Finv{K0(abs(k))/k^2}",
                       "@Finv{K0(-abs(k))/k}", "@Finv{K_1(abs(k))/k}",
                       "InverseFourier(K0(abs(k))/k where (k-1 != 0),k,x)"):
            self.assertIn("fourier(", fields(source, "x")["function"])
        self.assertEqual(number(fields("@F{asinh(0*x)}")), 0)

    def test_inverse_by_independent_integral(self):
        # Integrate K0's positive exponential representation first:
        # integral sin(k*x)*exp(-k*cosh(t))/k dk = atan(x/cosh(t)).
        for x in (-1.3, 0, 0.4, 1.2):
            expected = 2/math.pi*simpson(lambda t: math.atan(x/math.cosh(t)), 0, 32)
            actual = number(fields("{@Finv{-2i*K0(abs(k))/k} | x="+str(x)+"}", "x"))
            self.assertLess(abs(actual-expected), 2e-12)


class ModifiedBesselKTests(unittest.TestCase):
    def test_requested_precision_is_preserved(self):
        with localcontext() as context:
            context.prec = 100
            gamma = Decimal("0.5772156649015328606065120900824024310421593359399235988057672348848677267776646709369470632917467495")
            term = i0 = Decimal(1)
            harmonic = weighted = Decimal(0)
            for n in range(1, 140):
                term /= 4*n*n
                harmonic += Decimal(1)/n
                i0 += term
                weighted += harmonic*term
            expected = weighted-(Decimal("0.5").ln()+gamma)*i0
            result, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, "K0(1)", 80, "x", "evaluate")
            self.assertEqual(code, 0, raw)
            self.assertLess(abs(Decimal(result["value"])-expected), Decimal("1e-77"))
        for z in ("0.2", "2", "30", "1+i"):
            source = "besselk(1/2,"+z+")/(sqrt(@pi/(2*("+z+")))*exp(-("+z+")))-1"
            result, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 80, "x", "evaluate")
            self.assertEqual(code, 0, raw)
            self.assertLess(abs(number(result)), 1e-70)

    def test_numeric_orders_and_complex_arguments(self):
        for order in (0, 1, 2, -3, 0.5, 0.3, 1.0000001, 0.3+0.2j):
            for argument in (0.4, 2, 1+0.5j):
                n = str(order).strip("()").replace("j", "i")
                z = str(argument).strip("()").replace("j", "i")
                value = number(fields(f"besselk({n},{z})"))
                expected = k_integral(order, argument)
                self.assertLess(abs(value-expected), 2e-11*max(1, abs(expected)), (order, argument))

    def test_aliases_derivative_integral_and_sum(self):
        for alias in ("K0", "K_0", "K₀"):
            self.assertAlmostEqual(number(fields(alias+"(1)")).real, 0.42102443824070833, places=14)
        for alias in ("besselk", "bessel_k", "BesselK"):
            self.assertAlmostEqual(number(fields(alias+"(0,1)")).real, 0.42102443824070833, places=14)
        derivative = fields("{Dx(K0(x)) | x=1}", "x")
        self.assertLess(abs(number(derivative)+k_integral(1, 1)), 2e-12)
        for n in (0, 1, 2, 3):
            primitive, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, f"K_{n}(x)", 40, "x", "integral")
            self.assertEqual(code, 0, raw)
            self.assertNotIn("integral(", primitive["integral_function"])
            expression = primitive["integral"].split(" = ", 1)[1]
            result, raw, code = mars_lab.run_mars_lab_fields(
                mars_lab.DEFAULT_BIN, expression.replace("x = NAN", "x = 1"), 40, "x", "derivative")
            self.assertEqual(code, 0, raw)
            self.assertLess(abs(complex(result["derivative_value"])-k_integral(n, 1)), 2e-11)
        total = fields("sum(n,0,2,besselk(n,1))")
        self.assertLess(abs(number(total)-sum(k_integral(n, 1) for n in range(3))), 2e-11)


class ZZAsinhReadmeExamples(unittest.TestCase):
    def test_readme_asinh_pair(self):
        # README examples: docs/expression.md, inverse hyperbolic sine Fourier pair.
        result = fields("@F{asinh(x)}")
        self.assertIn("k ≠ 0", result["expression"])
        self.assertIn("K_{0}", result["tex"])
        self.assertEqual(algebra(fields("@Finv{-2i*K0(abs(k))/k}", "x")), "asinh(x)")


if __name__ == "__main__":
    unittest.main()
