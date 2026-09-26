"""Analytic-functional Fourier spectra, distinct from ordinary Dirac distributions."""

import math
import unittest

from test_fourier_hyperbolic import fields, number


def spectrum(body, inverse=False):
    operator, target = ("@Finv", "t") if inverse else ("@F", "ω")
    result = fields(operator+"{"+body+"}", target)
    return result["expression"].split(" | ")[0].removeprefix("{ ")


class AnalyticFourierTests(unittest.TestCase):
    def test_requested_sinh_pair_is_visible_and_copies(self):
        result = fields("@F{sinh(t)}")
        self.assertIn(r"\delta(", result["tex"])
        self.assertNotIn(r"\delta_", result["tex"])
        self.assertIn("analytic_delta(", result["expression"])
        self.assertIn("analytic_delta(", result["function"])
        self.assertIn("Extended Fourier transform", result["value_note"])
        self.assertEqual(fields(result["expression"])["tex"], result["tex"])
        self.assertNotIn(r"\left(i\right)", result["tex"])
        self.assertNotIn("(i)", result["expression"])
        copied = spectrum("sinh(t)")
        recovered = fields("@Finv{"+copied+"}", "t")
        self.assertIn("sinh(t)", recovered["expression"])
        self.assertNotIn("Fourier(", recovered["function"])

    def test_finite_exponential_families_round_trip_both_directions(self):
        cases = ("sinh(z)", "cosh(z)", "sinh(2*z+1)", "cosh(-2*z+1)",
                 "sinh(z)^2", "cosh(z)^3", "sech(z)^(-2)", "cosech(z)^(-2)",
                 "exp(z)", "exp(-2*z+1)", "exp((1+i)*z)", "3*sinh(z)")
        for inverse in (False, True):
            source = "ω" if inverse else "t"
            opposite = "@F" if inverse else "@Finv"
            for case in cases:
                body = case.replace("z", source)
                with self.subTest(inverse=inverse, body=body):
                    copied = spectrum(body, inverse)
                    self.assertIn("analytic_delta", copied)
                    for point in (-0.7, 0.4):
                        result = fields("{"+opposite+"{"+copied+"} | "+source+"="+str(point)+"}", source)
                        expected = fields("{"+body+" | "+source+"="+str(point)+"}", source)
                        self.assertNotIn("Fourier(", result["function"])
                        self.assertLess(abs(number(result)-number(expected)), 1e-11)

    def test_evaluation_action_and_duality(self):
        for alias in ("analytic_delta", "AnalyticDelta", "δℂ"):
            for point in (-0.6, 0, 0.7):
                forward = fields("{@F{"+alias+"(t-i)} | ω="+str(point)+"}")
                inverse = fields("{@Finv{"+alias+"(ω-i)} | t="+str(point)+"}", "t")
                self.assertAlmostEqual(number(forward).real, math.exp(point))
                self.assertAlmostEqual(number(inverse).real, math.exp(-point)/(2*math.pi))
        copied = spectrum("sinh(t)")
        result = fields("{@F{"+copied+",ω,t} | t=0.4}", "t")
        self.assertAlmostEqual(number(result).real, -2*math.pi*math.sinh(0.4))

    def test_no_pointwise_value_even_at_real_regular_points(self):
        for body in ("@F{sinh(t)}", "analytic_delta(ω+i)", "analytic_delta(ω)",
                     "analytic_delta(ω-i)", "Derivative(analytic_delta(ω+i),ω,1)"):
            for point in (0, 1, -1):
                result = fields("{"+body+" | ω="+str(point)+"}")
                self.assertEqual(result["value"], "NAN")
                self.assertIn("analytic", result["value_note"])
        ordinary = fields("{delta(ω) | ω=1}")
        self.assertEqual(number(ordinary), 0)

    def test_no_unsupported_delta_scaling_or_non_entire_extension(self):
        for body in ("analytic_delta(2*ω+i)", "analytic_delta(ω^2+i)", "delta(ω+i)"):
            result = fields("@Finv{"+body+"}", "t")
            self.assertTrue("Fourier(" in result["function"] or result["value"] == "NAN")
        for body in ("sinh(t)^(1/2)", "sinh(t)^(1+i)", "exp(t^2)"):
            result = fields("@F{"+body+"}")
            self.assertNotIn("analytic_delta", result["expression"])

    def test_symbolic_affine_parameters_and_cancellation(self):
        copied = spectrum("sinh(a*t+b)")
        for a, b in ((2, 1), (-2, -1), (0, 1)):
            result = fields("{@Finv{"+copied+"} | t=0.4; a="+str(a)+"; b="+str(b)+"}", "t")
            self.assertAlmostEqual(number(result).real, math.sinh(a*0.4+b))
        result = fields("{Fourier(c*sinh(t),t,ω) | ω=1; c=0}")
        self.assertEqual(number(result), 0)

    def test_polynomial_multiplier_and_derivative_round_trip(self):
        copied = spectrum("t*sinh(t)")
        result = fields("{@Finv{"+copied+"} | t=0.4}", "t")
        self.assertAlmostEqual(number(result).real, 0.4*math.sinh(0.4))

    def test_imaginary_shifts_do_not_keep_factor_parentheses(self):
        for body in ("sinh(t)", "sinh(2*t)", "cosh(3*t)", "exp(-2*t)"):
            with self.subTest(body=body):
                result = fields("@F{"+body+"}")
                self.assertNotRegex(result["tex"], r"\\left\([123]?i\\right\)")
                self.assertNotRegex(result["expression"], r"\([123]?i\)")
                self.assertEqual(fields(result["expression"])["tex"], result["tex"])

    def test_required_complex_grouping_preserves_values(self):
        for body, expected in (("x-(2+3*i)", -1-3j), ("x*(-2*i)", -2j),
                               ("x/(2*i)", -0.5j), ("(-2*i)^2*x", -4),
                               ("x+(-2*i)", 1-2j), ("x-(-2*i)", 1+2j)):
            with self.subTest(body=body):
                result = fields("{"+body+" | x=1}", "x")
                self.assertLess(abs(number(result)-expected), 1e-14)
                self.assertLess(abs(number(fields(result["expression"], "x"))-expected), 1e-14)


class ZZAnalyticFourierReadmeExamples(unittest.TestCase):
    """README examples from docs/expression.md, run after all ordinary tests."""

    def test_readme_analytic_sinh(self):
        result = fields("@F{sinh(t)}")
        self.assertIn("analytic_delta", result["expression"])
        recovered = fields("@Finv{@pi*(analytic_delta(ω+i)-analytic_delta(ω-i))}", "t")
        self.assertIn("sinh(t)", recovered["expression"])
        self.assertNotIn("Fourier(", recovered["function"])


if __name__ == "__main__":
    unittest.main()
