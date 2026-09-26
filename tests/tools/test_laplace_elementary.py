"""Elementary Laplace integration tests; use the already built native helper, never build it here.

Run this suite sequentially after the elementary dispatcher has been wired and built by the owner.
These are ordinary regression tests, not README examples; documentation is maintained by the main task.
"""

import cmath
import math
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

import mars_lab


BINARY = ROOT / "build" / "release" / "scratch" / "mars_lab"


def quadrature(function, target, end=32.0, count=20000):
    """Independent composite Simpson integral; callers choose a negligible exponential tail."""
    step = end / count

    def integrand(t):
        return cmath.exp(-target * t) * function(t)

    total = integrand(0) + integrand(end)
    for k in range(1, count):
        total += (4 if k % 2 else 2) * integrand(k * step)
    return total * step / 3


@unittest.skipUnless(BINARY.is_file(), "release mars_lab helper is not built")
class LaplaceElementaryTests(unittest.TestCase):
    def fields(self, source):
        fields, raw, code = mars_lab.run_mars_lab_fields(BINARY, source, 40, "s", "evaluate")
        self.assertEqual(code, 0, raw)
        return fields

    def value(self, fields):
        return complex(fields["value"].replace(" ", "").replace("−", "-").replace("i", "j"))

    def assert_transform(self, operand, target, expected, constants="", places=9):
        bindings = "s=" + target + ("; " + constants if constants else "")
        fields = self.fields("{Laplace(" + operand + ",t,s) | " + bindings + "}")
        self.assertNotIn("laplace(", fields["function"])
        actual = self.value(fields)
        self.assertAlmostEqual(actual.real, complex(expected).real, places=places)
        self.assertAlmostEqual(actual.imag, complex(expected).imag, places=places)
        return fields

    def test_all_eight_circular_reductions(self):
        cases = (
            ("versin", math.cos, -1, 1),
            ("vercos", math.cos, 1, 1),
            ("coversin", math.sin, -1, 1),
            ("covercos", math.sin, 1, 1),
            ("haversin", math.cos, -1, 2),
            ("havercos", math.cos, 1, 2),
            ("hacoversin", math.sin, -1, 2),
            ("hacovercos", math.sin, 1, 2),
        )
        for name, trig, sign, divisor in cases:
            with self.subTest(name=name):
                expected = quadrature(lambda t: (1 + sign * trig(2*t + 0.3)) / divisor, 2)
                self.assert_transform(name + "(2*t+3/10)", "2", expected)

    def test_circular_symbolic_rate_preserves_complex_domain(self):
        fields = self.fields("Laplace(versin(c*t),t,s)")
        self.assertNotIn("laplace(", fields["function"])
        self.assertIn("Re(s)", fields["expression"])
        copied = self.fields("{" + fields["unbound"] + " | s=3; c=1+i}")
        expected = quadrature(lambda t: 1-cmath.cos((1+1j)*t), 3)
        self.assertAlmostEqual(self.value(copied).real, expected.real, places=9)
        self.assertAlmostEqual(self.value(copied).imag, expected.imag, places=9)
        outside = self.fields("{" + fields["unbound"] + " | s=1/2; c=1+i}")
        self.assertEqual(outside["value"], "NAN")

    def test_sech_rates_zero_and_negative_abscissa(self):
        fields = self.fields("Laplace(sech(t),t,s)")
        self.assertNotIn("abs(1)", fields["function"])
        for rate in (1, -1, 2, -2):
            with self.subTest(rate=rate):
                expected = quadrature(lambda t: 1/math.cosh(rate*t), 2)
                self.assert_transform("sech(c*t)", "2", expected, "c=" + str(rate))
        self.assert_transform("sech(t)", "0", math.pi/2)
        self.assert_transform("sech(t)", "-1/2", quadrature(lambda t: 1/math.cosh(t), -0.5, end=80))
        self.assert_transform("sech(0*t)", "2", 0.5)
        fields = self.fields("{Laplace(sech(t),t,s) | s=-1}")
        self.assertEqual(fields["value"], "NAN")

    def test_symbolic_sech_guard_and_round_trip(self):
        fields = self.fields("Laplace(sech(c*t),t,s)")
        self.assertIn("digamma(", fields["function"])
        self.assertIn("sqrt(", fields["function"])
        self.assertNotIn("laplace(", fields["function"])
        copied = self.fields(fields["expression"])
        self.assertEqual(copied["unbound"], fields["unbound"])
        for rate, numeric_rate in (("1+i", 1+1j), ("-1-i", -1-1j)):
            with self.subTest(rate=rate):
                evaluated = self.fields("{" + fields["unbound"] + " | s=2; c=" + rate + "}")
                expected = quadrature(lambda t: 1/cmath.cosh(numeric_rate*t), 2)
                self.assertAlmostEqual(self.value(evaluated).real, expected.real, places=9)
                self.assertAlmostEqual(self.value(evaluated).imag, expected.imag, places=9)
        for bindings in ("s=2; c=i", "s=2; c=0", "s=-2; c=1"):
            with self.subTest(bindings=bindings):
                invalid = self.fields("{" + fields["unbound"] + " | " + bindings + "}")
                self.assertEqual(invalid["value"], "NAN")

    def test_abs_linear_and_affine_crossings(self):
        self.assert_transform("abs((3+4i)*t)", "2", 1.25)
        self.assert_transform("abs(-2*t-3)", "2", 2)
        self.assert_transform("abs(2*t+3)", "2", 2)
        for operand in ("abs(2*t-3)", "abs(3-2*t)"):
            with self.subTest(operand=operand):
                self.assert_transform(operand, "2", 1+math.exp(-3))
        self.assert_transform("abs(a*t+b)", "2", 0.5, "a=-2, b=0")
        fields = self.fields("Laplace(abs(c*t),t,s)")
        self.assertNotIn("laplace(", fields["function"])
        self.assertIn("c", fields["function"])

    def test_conjugation_does_not_conjugate_the_transform_variable(self):
        target = 2+1j
        expected = (1-2j)/(target*target) + (3+1j)/target
        self.assert_transform("conj((1+2i)*t+3-i)", "2+i", expected)
        fields = self.fields("Laplace(conj(a*t+b),t,s)")
        self.assertNotIn("laplace(", fields["function"])
        copied = self.fields("{" + fields["unbound"] + " | s=2+i; a=1+2i, b=3-i}")
        self.assertAlmostEqual(self.value(copied).real, expected.real, places=12)
        self.assertAlmostEqual(self.value(copied).imag, expected.imag, places=12)

    def test_floor_and_ceil_positive_negative_and_zero_rates(self):
        # Integrate each constant staircase interval independently; the omitted tail is below exp(-100).
        for rate in (1, -1, 2, -2):
            for name, function in (("floor", math.floor), ("ceil", math.ceil)):
                with self.subTest(rate=rate, name=name):
                    width = 1/abs(rate)
                    expected = sum(function(rate*(k+0.5)*width) *
                                   (math.exp(-2*k*width)-math.exp(-2*(k+1)*width))/2
                                   for k in range(120))
                    self.assert_transform(name + "(c*t)", "2", expected, "c=" + str(rate), places=12)
        for name in ("floor", "ceil"):
            self.assert_transform(name + "(0*t)", "-2", 0)
            invalid = self.fields("{Laplace(" + name + "(t),t,s) | s=0}")
            self.assertEqual(invalid["value"], "NAN")

    def test_arctangent_E1_branches_for_real_rates_and_complex_targets(self):
        for rate in (1, -1, 2, -2):
            for target_text, target in (("2", 2), ("2+i", 2+1j), ("2-i", 2-1j)):
                for name in ("atan", "acot"):
                    with self.subTest(rate=rate, target=target_text, name=name):
                        function = (lambda t: math.atan(rate*t)) if name == "atan" else (
                            lambda t: math.atan2(1, rate*t))
                        expected = quadrature(function, target)
                        self.assert_transform(name + "(c*t)", target_text, expected, "c=" + str(rate))
        self.assert_transform("atan(0*t)", "-2", 0)
        self.assert_transform("acot(0*t)", "2", math.pi/4)

    def test_affine_logarithms_and_common_logarithms(self):
        # The C expr_log API is natural logarithm, but the parser's "log" alias is base ten.
        for name, divisor in (("ln", 1), ("log", math.log(10)), ("lg", math.log(10)),
                              ("log10", math.log(10))):
            for target_text, target in (("2", 2), ("2+i", 2+1j)):
                with self.subTest(name=name, target=target_text):
                    expected = quadrature(lambda t: math.log(2*t+3)/divisor, target)
                    self.assert_transform(name + "(a*t+b)", target_text, expected, "a=2, b=3")
            expected = (math.log(2)-0.5772156649015328606-math.log(3))/(3*divisor)
            self.assert_transform(name + "(2*t)", "3", expected)
            self.assert_transform(name + "(0*t+3)", "2", math.log(3)/(2*divisor))
        invalid = self.fields("{Laplace(ln(2*t+3),t,s) | s=0}")
        self.assertEqual(invalid["value"], "NAN")

    def test_asinh_struve_formula_real_rates(self):
        for rate in (1, -1, 2, -2):
            with self.subTest(rate=rate):
                expected = quadrature(lambda t: math.asinh(rate*t), 2)
                self.assert_transform("asinh(c*t)", "2", expected, "c=" + str(rate))
        self.assert_transform("asinh(0*t)", "-2", 0)
        invalid = self.fields("{Laplace(asinh(t),t,s) | s=0}")
        self.assertEqual(invalid["value"], "NAN")

    def test_asinh_complex_targets_against_quadrature(self):
        # The native Struve H and Bessel Y backends retain the transform's complex half-plane.
        for rate in (1, -1, 2, -2):
            for target in ("2+i", "2-i"):
                with self.subTest(rate=rate, target=target):
                    fields = self.fields("{Laplace(asinh(c*t),t,s) | s=" + target + "; c=" + str(rate) + "}")
                    self.assertNotIn("laplace(", fields["function"])
                    self.assertIn("bessely(", fields["function"])
                    self.assertIn("struveh(", fields["function"])
                    expected = quadrature(lambda t: math.asinh(rate*t), complex(target.replace("i", "j")))
                    self.assertLess(abs(self.value(fields)-expected), 2e-10)

    def test_small_integer_hyperbolic_powers(self):
        for name, function in (("sinh", math.sinh), ("cosh", math.cosh)):
            for order in (2, 3, 4):
                with self.subTest(name=name, order=order):
                    target = order+2
                    expected = quadrature(lambda t: function(t+0.25)**order, target, end=24)
                    self.assert_transform(f"{name}(t+1/4)^{order}", str(target), expected)
        for order in (5, 8, 16):
            with self.subTest(order=order):
                # Substitution u=exp(-2t) gives this beta integral independently of the finite sum.
                fields = self.assert_transform(f"sinh(t)^{order}", str(order+2),
                                               1/((order+1)*2**(order+1)), places=12)
                self.assertIn("sum(", fields["function"])
        shifted = quadrature(lambda t: math.cosh(t+0.25)**5, 7, end=24)
        self.assert_transform("cosh(t+1/4)^5", "7", shifted)
        fields = self.fields("Laplace(cosh(c*t)^2,t,s)")
        self.assertNotIn("laplace(", fields["function"])
        valid = self.fields("{" + fields["unbound"] + " | s=3; c=-1}")
        self.assertAlmostEqual(self.value(valid).real, 7/15, places=12)
        invalid = self.fields("{" + fields["unbound"] + " | s=1; c=1}")
        self.assertEqual(invalid["value"], "NAN")

    def test_unsupported_parameters_are_not_assumed_real_or_positive(self):
        cases = (
            "floor(c*t)", "ceil(c*t)", "floor(i*t)", "ceil(i*t)",
            "atan(c*t)", "acot(c*t)", "atan(i*t)", "acot(i*t)",
            "asinh(c*t)", "asinh(i*t)", "asinh(t+1)",
            "abs(c*t+d)", "abs(t+i)", "sech(t+1)",
            "ln(c*t+1)", "ln(1-t)", "ln(t+i)",
            "sinh(t)^(1/2)", "cosh(t)^n", "sinh(t^2)^2",
        )
        for operand in cases:
            with self.subTest(operand=operand):
                fields = self.fields("Laplace(" + operand + ",t,s)")
                self.assertIn("laplace(", fields["function"])

    def test_atanh_real_rates_and_complex_targets(self):
        # Independent integration: t=(1 +/- u^2)/|c| removes the logarithmic endpoint.
        for rate, target, target_text in ((1, 1, "1"), (-1, 1, "1"),
                                          (2, 2, "2"), (-2, 1+1j, "1+i")):
            q = abs(rate)
            def integrand(u, exterior, derivative=False):
                if u == 0:
                    return 0
                x = 1 + u*u if exterior else 1-u*u
                t = x/q
                real = math.copysign(1, rate)*math.log((1+x)/(u*u))/2
                source = real + (0.5j*math.pi if exterior else 0)
                return 2*u/q*cmath.exp(-target*t)*source*(-t if derivative else 1)
            expected = quadrature(lambda u: integrand(u, False), 0, end=1, count=40000)
            expected += quadrature(lambda u: integrand(u, True), 0, end=math.sqrt(40*q), count=40000)
            with self.subTest(rate=rate, target=target):
                self.assert_transform("atanh("+str(rate)+"*t)", target_text, expected, places=6)
                derivative = quadrature(lambda u: integrand(u, False, True), 0, end=1, count=40000)
                derivative += quadrature(lambda u: integrand(u, True, True), 0,
                                         end=math.sqrt(40*q), count=40000)
                fields, raw, code = mars_lab.run_mars_lab_fields(
                    BINARY, "{@L(atanh("+str(rate)+"*t)) | s="+target_text+"}", 40, "s", "derivative")
                self.assertEqual(code, 0, raw)
                actual = complex(fields["derivative_value"].replace(" ", "").replace("i", "j"))
                self.assertAlmostEqual(actual.real, derivative.real, places=6)
                self.assertAlmostEqual(actual.imag, derivative.imag, places=6)

    def test_atanh_symbolic_guard_round_trip_and_zero(self):
        fields = self.fields("@L{atanh(ct)}")
        self.assertNotIn("laplace(", fields["function"])
        self.assertIn("realpart(c) == c", fields["function"])
        self.assertIn(r"\in\mathbb{R}", fields["tex"])
        self.assertIn(r"\frac{1}{2\mkern-2mu s}\,\left[", fields["tex"])
        self.assertIn(r"\right]\quad", fields["tex"])
        for rate in ("1", "-1"):
            copied = self.fields("{"+fields["unbound"]+" | s=1; c="+rate+"}")
            direct = self.fields("{@L(atanh("+rate+"*t)) | s=1}")
            self.assertAlmostEqual(self.value(copied), self.value(direct), places=12)
        invalid = self.fields("{"+fields["unbound"]+" | s=1; c=i}")
        self.assertEqual(invalid["value"], "NAN")
        self.assert_transform("atanh(0*t)", "-1", 0)
        self.assertIn("laplace(", self.fields("@L(atanh(i*t))")["function"])


if __name__ == "__main__":
    unittest.main()
