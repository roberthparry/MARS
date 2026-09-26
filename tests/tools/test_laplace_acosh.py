"""Principal acosh Laplace pair: quadrature and freshly parsed inverse spectra.

Run ordinary tests first; AcoshReadmeTests contains the final README examples.
"""

import cmath
import math
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import mars_lab


def simpson(function, end, count=8000):
    step = end / count
    total = function(0) + function(end)
    for j in range(1, count):
        total += (4 if j % 2 else 2) * function(j * step)
    return total * step / 3


def reference(rate, s):
    """Integrate acosh itself, smoothing the branch point with cos/cosh substitutions."""
    q = abs(rate)
    sign = 1 if rate > 0 else -1

    def interior(theta):
        phase = theta if sign > 0 else math.pi - theta
        return 1j * phase * cmath.exp(-s * math.cos(theta)/q) * math.sin(theta)/q

    def exterior(u):
        value = u if sign > 0 else u + 1j * math.pi
        return value * cmath.exp(-s * math.cosh(u)/q) * math.sinh(u)/q

    return simpson(interior, math.pi/2) + simpson(exterior, 7)


class AcoshHelpers:
    def fields(self, source, variable="s"):
        fields, raw, code = mars_lab.run_mars_lab_fields(
            mars_lab.DEFAULT_BIN, source, 40, variable, "evaluate")
        self.assertEqual(code, 0, raw)
        return fields

    @staticmethod
    def value(fields):
        return complex(fields["value"].replace(" ", "").replace("−", "-").replace("i", "j"))

    @staticmethod
    def body(fields):
        return fields["expression"].split(" | ")[0].removeprefix("{ ")

    def check_inverse(self, spectrum, expected):
        inverse = self.fields("InverseLaplace(" + spectrum + ",s,t)", "t")
        self.assertNotIn("inverselaplace(", inverse["function"])
        for t in (0.19, 0.5, 1, 1.31, 3):
            value = self.value(self.fields("{" + self.body(inverse) + f" | t={t}" + "}", "t"))
            self.assertLess(abs(value - expected(t)), 2e-11 * (1 + abs(expected(t))))
        return inverse


class AcoshLaplaceTests(AcoshHelpers, unittest.TestCase):
    def test_forward_against_independent_quadrature(self):
        for rate in (1, -1, 2, -0.5):
            for text, s in (("2", 2), ("1+i", 1+1j)):
                with self.subTest(rate=rate, s=s):
                    result = self.fields("{Laplace(acosh(" + str(rate) + "*t),t,s) | s=" + text + "}")
                    self.assertNotIn("laplace(", result["function"])
                    self.assertLess(abs(self.value(result)-reference(rate, s)), 2e-10)
                    # Free target bindings never leak into the displayed formula.
                    unbound = self.fields("Laplace(acosh(" + str(rate) + "*t),t,s)")
                    self.assertEqual(result["tex"], unbound["tex"])

    def test_constant_parameter_and_half_plane(self):
        result = self.fields("{Laplace(acosh(c*t),t,s) | s=2; c=-2}")
        self.assertNotIn("laplace(", result["function"])
        self.assertLess(abs(self.value(result)-reference(-2, 2)), 2e-10)
        for s in ("0", "-1", "i"):
            result = self.fields("{Laplace(acosh(t),t,s) | s=" + s + "}")
            self.assertEqual(result["value"], "NAN")
        result = self.fields("{Laplace(acosh(0*t),t,s) | s=2}")
        self.assertLess(abs(self.value(result)-1j*math.pi/4), 2e-12)

    def test_unsupported_arguments_remain_symbolic(self):
        for argument in ("c*t", "i*t", "t+1"):
            with self.subTest(argument=argument):
                result = self.fields("Laplace(acosh(" + argument + "),t,s)")
                self.assertIn("laplace(", result["function"])

    def test_unicode_spectrum_notation(self):
        forward = self.fields("@L{acosh(t)}")
        for symbol in ("K₀(s)", "I₀(s)", "𝐋₀(s)"):
            self.assertIn(symbol, forward["expression"])
        for name in ("BesselK(", "BesselI(", "StruveL("):
            self.assertNotIn(name, forward["expression"])
        self.assertEqual(self.fields(forward["expression"])["unbound"], forward["unbound"])

    def test_copied_spectra_round_trip(self):
        for rate in (1, -1, 2, -0.5):
            with self.subTest(rate=rate):
                forward = self.fields("Laplace(acosh(" + str(rate) + "*t),t,s)")
                self.check_inverse(self.body(forward), lambda t: cmath.acosh(rate*t))

    def test_independent_spectra_and_linear_combination(self):
        for sign in (1, -1):
            spectrum = ("besselk(0,s/2)/s+i*@pi/(2*s)*(1-(" + str(sign) + ")*"
                        "(hypergeometricpfq(0,1,1,s^2/16)-s/@pi*"
                        "hypergeometricpfq(1,2,1,3/2,3/2,s^2/16)))")
            self.check_inverse(spectrum, lambda t: cmath.acosh(sign*2*t))
            self.check_inverse("3*(" + spectrum + ")+2/s", lambda t: 3*cmath.acosh(sign*2*t)+2)

    def test_incomplete_companion_is_not_acosh(self):
        # K0 alone represents only the real tail, not the principal function below t=1.
        inverse = self.fields("InverseLaplace(besselk(0,s)/s,s,t)", "t")
        if "inverselaplace(" not in inverse["function"]:
            result = self.fields("{" + self.body(inverse) + " | t=1/2}", "t")
            self.assertGreater(abs(self.value(result)-cmath.acosh(0.5)), 0.1)

    def test_frequency_shift(self):
        for name, function in (("acosh", cmath.acosh), ("asinh", math.asinh)):
            with self.subTest(function=name):
                forward = self.fields("Laplace(exp(-t)*" + name + "(t),t,s)")
                self.check_inverse(self.body(forward), lambda t: math.exp(-t)*function(t))


class AcoshReadmeTests(AcoshHelpers, unittest.TestCase):
    def test_readme_unicode_expression(self):
        # README example: docs/expression.md, unbound acosh Laplace Expression output.
        result = self.fields("@L{acosh(t)}")
        self.assertEqual(result["unbound"],
                         "1/s·(K₀(s) + 0.5iπ·(1 - I₀(s) + 𝐋₀(s))) where (Re(s) > 0)")

    def test_readme_acosh_laplace(self):
        # README example: docs/expression.md, principal inverse-hyperbolic Laplace pair.
        result = self.fields("{@L{acosh(t)} | s=1}")
        documented = complex(0.421024438240708, 0.697712084144029)
        self.assertLess(abs(self.value(result)-documented), 1e-15)
        self.assertLess(abs(self.value(result)-reference(1, 1)), 2e-10)
        forward = self.fields("@L{acosh(t)}")
        inverse = self.check_inverse(self.body(forward), cmath.acosh)
        self.assertIn("acosh(t)", inverse["function"])


if __name__ == "__main__":
    unittest.main()
