"""Genuine, serialised special-function inverse Laplace pairs on positive time.

Requires an already-built native helper. This module never builds or launches
parallel tests. Independent series and quadrature references also check the
recovered functions, rather than trusting agreement between two native rules.
"""

import math
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import mars_lab

from test_laplace_special import (
    bessel_j, bessel_y_zero, incomplete_gamma, positive_ei, quadrature,
)


def e1_reference(x):
    """E1(x)=exp(-x) integral_0^infinity exp(-u)/(x+u) du, for x>0."""
    return math.exp(-x) * quadrature(lambda u: math.exp(-u) / (x + u), 0, 48)


class InverseLaplaceSpecialTests(unittest.TestCase):
    def fields(self, source, variable="t"):
        result, raw, code = mars_lab.run_mars_lab_fields(
            mars_lab.DEFAULT_BIN, source, 40, variable, "evaluate")
        self.assertEqual(code, 0, raw)
        return result

    @staticmethod
    def body(result):
        return result["expression"].split(" | ")[0].removeprefix("{ ")

    def value(self, body, time):
        result = self.fields("{" + body + f" | t={time}" + "}")
        self.assertIn("value", result, result)
        return complex(result["value"].replace(" ", "").replace("−", "-").replace("i", "j"))

    def assert_inverse(self, spectrum, expected):
        inverse = self.fields("InverseLaplace(" + spectrum + ",s,t)")
        self.assertNotIn("inverselaplace(", inverse["function"])
        restored = self.body(inverse)
        # Reparse the inverse result as well: correct internal algebra must not
        # conceal an incorrect sign, branch or grouping in expression output.
        for time in (0.2, 0.7, 1.3):
            with self.subTest(spectrum=spectrum, time=time):
                value = self.value(restored, time)
                reference = expected(time)
                self.assertLess(abs(value - reference), 3e-9 * max(1, abs(reference)))
        return restored

    def assert_round_trip(self, source, expected):
        forward = self.fields("Laplace(" + source + ",t,s)", "s")
        self.assertNotIn("return laplace(", forward["function"])
        # No nested transform node or source-function metadata survives this string.
        spectrum = self.body(forward)
        return self.assert_inverse(spectrum, expected)

    def test_exponential_integrals(self):
        for rate in (0.5, 2):
            for name, sign, reference in (
                    ("E1", 1, e1_reference), ("Ei", -1, lambda x: -e1_reference(x)),
                    ("Ei", 1, positive_ei)):
                with self.subTest(name=name, rate=rate, sign=sign):
                    self.assert_round_trip(f"{name}({sign * rate}*t)", lambda t: reference(rate * t))

    def test_copied_logarithmic_spectra(self):
        for spectrum, reference in (
                ("ln(1+s/2)/s", lambda t: e1_reference(2*t)),
                ("-ln(s/2-1)/s", lambda t: positive_ei(2*t)),
                ("-ln(1+s/2)/s", lambda t: -e1_reference(2*t)),
                ("exp(3*s/2)*E1(3*s/2)/s+ln(3)/s", lambda t: math.log(2*t+3))):
            with self.subTest(spectrum=spectrum):
                self.assert_inverse(spectrum, reference)

    def test_affine_logarithms_and_scalar_multiples(self):
        for source, reference in (
                ("ln(2*t+1)", lambda t: math.log(2*t+1)),
                ("ln(2*t+3)", lambda t: math.log(2*t+3)),
                ("log10(2*t+3)", lambda t: math.log10(2*t+3)),
                ("-3*E1(2*t)+4", lambda t: -3*e1_reference(2*t)+4)):
            with self.subTest(source=source):
                self.assert_round_trip(source, reference)

    def test_bessel_orders_and_scales(self):
        for order in (-3, -0.75, 0, 0.5, 1, 3):
            with self.subTest(order=order):
                self.assert_round_trip(f"bessel_j({order},2*t)", lambda t: bessel_j(order, 2*t))
        self.assert_round_trip("bessel_y(0,2*t)", lambda t: bessel_y_zero(2*t))

    def test_copied_bessel_spectra(self):
        root = "sqrt(s^2+4)"
        for spectrum, reference in (
                ("1/" + root, lambda t: bessel_j(0, 2*t)),
                ("(2/(s+" + root + "))^3/" + root, lambda t: bessel_j(3, 2*t)),
                ("(2/(s+" + root + "))^(1/2)/" + root, lambda t: bessel_j(0.5, 2*t)),
                ("-2*ln((s+" + root + ")/2)/(@pi*" + root + ")", lambda t: bessel_y_zero(2*t))):
            with self.subTest(spectrum=spectrum):
                self.assert_inverse(spectrum, reference)

    def test_incomplete_gamma_families(self):
        for shape in (0.5, 1.5, 3):
            for upper in (False, True):
                for regularised in (False, True):
                    name = ("gammainc_Q" if upper else "gammainc_P") if regularised else (
                        "gammainc_upper" if upper else "gammainc_lower")
                    divisor = math.gamma(shape) if regularised else 1
                    with self.subTest(name=name, shape=shape):
                        self.assert_round_trip(
                            f"{name}({shape},2*t)",
                            lambda t: incomplete_gamma(shape, 2*t, upper) / divisor)

    def test_copied_gamma_spectrum_and_symbolic_conditions(self):
        self.assert_inverse("(2/(s+2))^(3/2)/s", lambda t: incomplete_gamma(1.5, 2*t, False)/math.gamma(1.5))
        result = self.fields("InverseLaplace((a/(s+a))^v/s,s,t)")
        self.assertNotIn("inverselaplace(", result["function"])
        self.assertIn("Re(v) > 0", result["expression"])
        self.assertIn("Re(a) > 0", result["expression"])
        self.assertNotIn("Re(s)", result["expression"])

    def test_copied_upper_gamma_spectra_preserve_small_tails(self):
        # Copied spectral algebra, not a nested-transform cancellation. At these
        # times Q is far smaller than unit-roundoff at the requested precision.
        spectra = (
            ("(1-(2/(s+2))^(3/2))/s", "gammainc_Q(3/2,2*t)"),
            ("gamma(3/2)*(1-(2/(s+2))^(3/2))/s", "gammainc_upper(3/2,2*t)"),
        )
        for spectrum, source in spectra:
            inverse = self.fields("InverseLaplace(" + spectrum + ",s,t)")
            self.assertNotIn("inverselaplace(", inverse["function"])
            self.assertIn("gammaincq(", inverse["function"])
            self.assertNotIn("gammaincp(", inverse["function"])
            for time in (50, 100):
                with self.subTest(source=source, time=time):
                    expected = self.value(source, time)
                    actual = self.value(self.body(inverse), time)
                    self.assertGreater(abs(expected), 0)
                    self.assertLess(abs((actual-expected)/expected), 2e-12)

    def test_copied_lower_gamma_spectra_preserve_small_tails(self):
        spectra = (
            ("(2/(s+2))^(3/2)/s", "gammainc_P(3/2,2*t)"),
            ("gamma(3/2)*(2/(s+2))^(3/2)/s", "gammainc_lower(3/2,2*t)"),
        )
        for spectrum, source in spectra:
            inverse = self.fields("InverseLaplace(" + spectrum + ",s,t)")
            self.assertNotIn("inverselaplace(", inverse["function"])
            self.assertIn("gammaincp(", inverse["function"])
            self.assertNotIn("gammaincq(", inverse["function"])
            # Keep the source's own value non-zero at the Lab's requested precision;
            # these tails still expose catastrophic subtraction from a near-unit Q.
            for time in ("1e-20", "1e-30"):
                with self.subTest(source=source, time=time):
                    expected = self.value(source, time)
                    actual = self.value(self.body(inverse), time)
                    self.assertGreater(abs(expected), 0)
                    self.assertLess(abs((actual-expected)/expected), 2e-12)

    def test_free_parameter_bindings_do_not_remove_guards(self):
        # Commas keep a and v in the free-variable group; semicolons would
        # intentionally declare constant parameters and specialise the formula.
        source = "{InverseLaplace((a/(s+a))^v/s,s,t) | t=?,a=2,v=3/2}"
        result = self.fields(source)
        self.assertIn("Re(v) > 0", result["expression"])
        self.assertIn("Re(a) > 0", result["expression"])

    def test_constant_parameters_are_specialised(self):
        source = "{InverseLaplace((a/(s+a))^v/s,s,t) | t=?; a=2; v=3/2}"
        result = self.fields(source)
        self.assertNotIn("inverselaplace(", result["function"])
        self.assertNotIn("Re(v)", result["expression"])
        self.assertNotIn("Re(a)", result["expression"])


if __name__ == "__main__":
    unittest.main()
