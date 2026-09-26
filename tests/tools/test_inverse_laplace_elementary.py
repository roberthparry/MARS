"""Elementary-transcendental inverse pairs, recognised from freshly parsed spectra."""

import math
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import mars_lab


class InverseLaplaceElementaryTests(unittest.TestCase):
    def fields(self, source, variable="t"):
        fields, raw, code = mars_lab.run_mars_lab_fields(
            mars_lab.DEFAULT_BIN, source, 40, variable, "evaluate")
        self.assertEqual(code, 0, raw)
        return fields

    @staticmethod
    def body(fields):
        # Fresh algebra, with no nested transform and no previous-input provenance.
        return fields["expression"].split(" | ")[0].removeprefix("{ ")

    @staticmethod
    def value(fields):
        return complex(fields["value"].replace(" ", "").replace("i", "j"))

    def check_inverse(self, spectrum, expected, points=(0.19, 0.71, 1.31)):
        source = "InverseLaplace(" + spectrum + ",s,t)"
        inverse = self.fields(source)
        self.assertNotIn("inverselaplace(", inverse["function"])
        restored = self.body(inverse)
        for point in points:
            with self.subTest(spectrum=spectrum, time=point):
                # Check both the native inverse and the reparsed inverse output. Their
                # agreement detects lost signs and parentheses in serialisation as well.
                for text in (source, restored):
                    actual = self.value(self.fields("{" + text + f" | t={point}" + "}"))
                    wanted = expected(point)
                    self.assertLess(abs(actual - wanted), 2e-11 * (1 + abs(wanted)))
        return inverse

    def check_round_trip(self, source, expected, points=(0.19, 0.71, 1.31)):
        forward = self.fields("Laplace(" + source + ",t,s)", "s")
        self.assertNotIn("laplace(", forward["function"])
        return self.check_inverse(self.body(forward), expected, points)

    def test_real_scaled_staircases(self):
        # A unilateral transform determines the function almost everywhere, not the
        # chosen value at a staircase jump. Deliberately avoid all jump locations.
        for name, function in (("floor", math.floor), ("ceil", math.ceil)):
            for scale in (2, -2, 0.5, -0.5):
                with self.subTest(function=name, scale=scale):
                    self.check_round_trip(f"{name}({scale}*t)", lambda t: function(scale*t))

    def test_independent_geometric_spectra(self):
        self.check_inverse("1/(s*(exp(s/2)-1))", lambda t: math.floor(2*t))
        self.check_inverse("exp(s/2)/(s*(exp(s/2)-1))", lambda t: math.ceil(2*t))
        self.check_inverse("-exp(2*s)/(s*(exp(2*s)-1))", lambda t: math.floor(-t/2))
        self.check_inverse("exp(2*s)/(s*(1-exp(2*s)))", lambda t: math.floor(-t/2))

    def test_digamma_hyperbolic_pairs(self):
        for scale in (2, -2, 0.5):
            for name, expected in (("tanh", lambda t: math.tanh(scale*t)),
                                   ("sech", lambda t: 1/math.cosh(scale*t))):
                with self.subTest(function=name, scale=scale):
                    self.check_round_trip(f"{name}({scale}*t)", expected)

    def test_independent_digamma_spectra(self):
        self.check_inverse("(2*digamma(s/8+1/2)-digamma(s/8)-digamma(s/8+1))/8",
                           lambda t: math.tanh(2*t))
        self.check_inverse("(digamma(s/8+3/4)-digamma(s/8+1/4))/4",
                           lambda t: 1/math.cosh(2*t))

    def test_inverse_circular_and_asinh_pairs(self):
        for scale in (2, -2, 0.5):
            cases = (("atan", lambda t: math.atan(scale*t)),
                     ("acot", lambda t: math.pi/2-math.atan(scale*t)),
                     ("asinh", lambda t: math.asinh(scale*t)))
            for name, expected in cases:
                with self.subTest(function=name, scale=scale):
                    self.check_round_trip(f"{name}({scale}*t)", expected)

    def test_cancelled_imaginary_units_produce_real_rates(self):
        for source in ("atan(2*t)", "acot(-2*t)", "Cl(3,2*t)"):
            with self.subTest(source=source):
                forward = self.fields("Laplace("+source+",t,s)", "s")
                inverse = self.fields("InverseLaplace("+self.body(forward)+",s,t)")
                self.assertNotIn("inverselaplace(", inverse["function"])
                self.assertNotRegex(self.body(inverse), r"(?:\+|-)\s*0\s*i")
                self.assertNotRegex(inverse["tex"], r"(?:\+|-)\s*0\s*i")

    @staticmethod
    def native_atanh(x):
        if abs(x) < 1:
            return math.atanh(x)
        # MARS takes a positive imaginary boundary value on BOTH exterior real cuts.
        return math.copysign(math.log((abs(x)+1)/(abs(x)-1))/2, x) + 0.5j*math.pi

    def test_atanh_retains_both_exterior_cut_values(self):
        for scale in (2, -2, 0.5, -0.5):
            with self.subTest(scale=scale):
                self.check_round_trip(f"atanh({scale}*t)", lambda t: self.native_atanh(scale*t),
                                      points=(0.19, 0.71, 2.31))

    def test_independent_atanh_branch_formula(self):
        real_part = "exp(-s/2)*Ei(s/2)+exp(s/2)*E1(s/2)"
        for sign in (1, -1):
            spectrum = f"(({sign})*({real_part})+i*@pi*exp(-s/2))/(2*s)"
            self.check_inverse(spectrum, lambda t: self.native_atanh(sign*2*t))

    def test_complete_formula_verification_preserves_changed_branch_terms(self):
        # Matching a single Ei/E1 node must not silently discard the rest of a spectrum.
        for coefficient in (0, 2):
            spectrum = ("(exp(-s/2)*Ei(s/2)+exp(s/2)*E1(s/2)"
                        f"+{coefficient}*i*@pi*exp(-s/2))/(2*s)")
            with self.subTest(coefficient=coefficient):
                result = self.fields("InverseLaplace("+spectrum+",s,t)")
                if "inverselaplace(" not in result["function"]:
                    # Other inverse rules may legitimately resolve this as atanh plus
                    # a delayed constant. If they do, its cut value must be different.
                    self.check_inverse(spectrum, lambda t: self.native_atanh(2*t).real +
                                       (coefficient*0.5j*math.pi if t > 0.5 else 0))

    def test_clausen_orders_inferred_from_spectral_degree(self):
        # Cl_1 has an elementary independent reference on this interval.
        self.check_round_trip("Cl(1,2*t)", lambda t: -math.log(2*abs(math.sin(t))))
        for order, scale in ((2, 1), (2, -2), (3, 2), (6, -0.5), (32, 1)):
            source = f"Cl({order},{scale}*t)"
            with self.subTest(order=order, scale=scale):
                # The higher-order native value is independently implemented by its
                # defining special function, not by either transform matcher.
                self.check_round_trip(source, lambda t: self.value(
                    self.fields("{"+source+f" | t={t}"+"}")))

    def test_scalar_multiples_and_constant_offsets(self):
        self.check_round_trip("3*atan(2*t)+2", lambda t: 3*math.atan(2*t)+2)
        self.check_round_trip("2-3*asinh(t/2)", lambda t: 2-3*math.asinh(t/2))
        self.check_round_trip("3*tanh(2*t)-2", lambda t: 3*math.tanh(2*t)-2)

    def test_asinh_offset_in_independent_common_denominator_spectra(self):
        kernel = "4*s/@pi*hypergeometricpfq(1,2,1,3/2,3/2,-s^2)-bessely(0,2*s)"
        for spectrum in (f"(2-3*@pi/2*({kernel}))/s",
                         f"2/s-3*@pi*({kernel})/(2*s)"):
            with self.subTest(spectrum=spectrum):
                self.check_inverse(spectrum, lambda t: 2-3*math.asinh(t/2))

    def test_clausen_finite_recurrence_is_independent_of_dummy_name(self):
        for index in ("j", "k"):
            spectrum = (f"(2*sum({index},1,1,(-1)^({index}-1)*zeta(5-2*{index})"
                        f"*(s/2)^(4-2*{index}))-digamma(1+i*s/2)-digamma(1-i*s/2)"
                        "-2*@gamma)/(4*(s/2)^3)")
            with self.subTest(index=index):
                self.check_inverse(spectrum, lambda t: self.value(self.fields(f"Cl(3,{2*t})")))


if __name__ == "__main__":
    unittest.main()
