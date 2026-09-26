"""Inverse-circular Laplace pairs; run README examples after ordinary regressions."""

import cmath
import math
import unittest

from tests.tools.test_laplace_acosh import AcoshHelpers, simpson


def real_cut(name, x):
    """MARS real-cut values, including the positive imaginary asin tails."""
    if abs(x) <= 1:
        sine = complex(math.asin(x))
    else:
        sine = complex(math.copysign(math.pi/2, x), math.acosh(abs(x)))
    return sine if name == "asin" else math.pi/2 - sine


def reference(name, rate, s):
    """Integrate the function itself, smoothing t=1/|rate| on both sides."""
    q = abs(rate)
    sign = 1 if rate > 0 else -1

    def interior(theta):
        sine = sign * (math.pi/2 - theta)
        value = sine if name == "asin" else math.pi/2 - sine
        return value * cmath.exp(-s * math.cos(theta)/q) * math.sin(theta)/q

    def exterior(u):
        sine = complex(sign * math.pi/2, u)
        value = sine if name == "asin" else math.pi/2 - sine
        return value * cmath.exp(-s * math.cosh(u)/q) * math.sinh(u)/q

    return simpson(interior, math.pi/2) + simpson(exterior, 7)


class InverseCircularLaplaceTests(AcoshHelpers, unittest.TestCase):
    def test_forward_against_independent_quadrature(self):
        for name in ("asin", "acos"):
            for rate in (1, -1, 2, -0.5):
                for text, s in (("1", 1), ("2+i", 2+1j), ("2-i", 2-1j)):
                    with self.subTest(name=name, rate=rate, s=s):
                        source = "Laplace(" + name + "(" + str(rate) + "*t),t,s)"
                        forward = self.fields(source)
                        bound = self.fields("{" + source + " | s=" + text + "}")
                        self.assertNotIn("Laplace(", forward["function"])
                        self.assertEqual(forward["tex"], bound["tex"])
                        self.assertLess(abs(self.value(bound)-reference(name, rate, s)), 3e-10)

    def test_copied_spectra_round_trip(self):
        for name in ("asin", "acos"):
            for rate in (1, -1, 2, -0.5):
                with self.subTest(name=name, rate=rate):
                    forward = self.fields("Laplace(" + name + "(" + str(rate) + "*t),t,s)")
                    inverse = self.check_inverse(self.body(forward), lambda t: real_cut(name, rate*t))
                    self.assertIn(name + "(", inverse["function"])
                    self.assertNotIn("acosh(", inverse["function"])

    def test_independent_spectra_and_linearity(self):
        sine = "@pi/(2*s)*(bessel_i(0,s)-struve_l(0,s))+i*besselk(0,s)/s"
        cosine = "@pi/(2*s)*(1-bessel_i(0,s)+struve_l(0,s))-i*besselk(0,s)/s"
        for name, spectrum in (("asin", sine), ("acos", cosine)):
            with self.subTest(name=name):
                self.check_inverse(spectrum, lambda t: real_cut(name, t))
                self.check_inverse("3*(" + spectrum + ")+2/s", lambda t: 3*real_cut(name, t)+2)

    def test_frequency_shift(self):
        for name in ("asin", "acos"):
            with self.subTest(name=name):
                forward = self.fields("Laplace(exp(-t)*" + name + "(t),t,s)")
                inverse = self.check_inverse(self.body(forward),
                                             lambda t: math.exp(-t)*real_cut(name, t))
                self.assertIn(name + "(", inverse["function"])

    def test_domain_and_constant_specialisation(self):
        for name in ("asin", "acos"):
            for s in ("0", "-1", "i"):
                result = self.fields("{Laplace(" + name + "(t),t,s) | s=" + s + "}")
                self.assertEqual(result["value"], "NAN")
            result = self.fields("{Laplace(" + name + "(c*t),t,s) | s=2; c=-2}")
            self.assertNotIn("Laplace(", result["function"])
            self.assertLess(abs(self.value(result)-reference(name, -2, 2)), 3e-10)
            free = self.fields("{Laplace(" + name + "(c*t),t,s) | c=-2, s=2}")
            self.assertIn("Laplace(", free["function"])
            zero = self.fields("{Laplace(" + name + "(0*t),t,s) | s=2}")
            self.assertLess(abs(self.value(zero)-(0 if name == "asin" else math.pi/4)), 1e-14)
            for argument in ("c*t", "i*t", "t+1"):
                result = self.fields("Laplace(" + name + "(" + argument + "),t,s)")
                self.assertIn("Laplace(", result["function"])

    def test_rendering_and_native_branch_values(self):
        for name in ("asin", "acos"):
            forward = self.fields("@L{" + name + "(t)}")
            for symbol in ("I₀(s)", "𝐋₀(s)", "K₀(s)"):
                self.assertIn(symbol, forward["expression"])
            for call in ("besseli(0, s)", "struvel(0, s)", "besselk(0, s)"):
                self.assertIn(call, forward["function"])
            for x in (-2, -1, 0, 1, 2):
                self.assertLess(abs(self.value(self.fields(name + "(" + str(x) + ")"))
                                    - real_cut(name, x)), 1e-14)


class InverseCircularReadmeTests(AcoshHelpers, unittest.TestCase):
    def test_readme_inverse_circular_laplace(self):
        # README examples: docs/expression.md, asin/acos unilateral Laplace values.
        examples = (
            ("asin", complex(0.873084242650868, 0.421024438240708)),
            ("acos", complex(0.697712084144029, -0.421024438240708)),
        )
        for name, expected in examples:
            with self.subTest(name=name):
                result = self.fields("{@L{" + name + "(t)} | s=1}")
                self.assertLess(abs(self.value(result)-expected), 1e-15)
                self.assertLess(abs(self.value(result)-reference(name, 1, 1)), 3e-10)


if __name__ == "__main__":
    unittest.main()
