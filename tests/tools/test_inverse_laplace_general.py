"""General unilateral inverse pairs, tested from independent serialised spectra."""

import math
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import mars_lab


class InverseLaplaceGeneralTests(unittest.TestCase):
    def fields(self, source, variable="t"):
        result, raw, code = mars_lab.run_mars_lab_fields(
            mars_lab.DEFAULT_BIN, source, 40, variable, "evaluate")
        self.assertEqual(code, 0, raw)
        return result

    @staticmethod
    def algebra(result):
        # Deliberately discard the transform operator, not merely its display label.
        return result["expression"].split(" | ")[0].removeprefix("{ ")

    @staticmethod
    def number(result):
        return complex(result["value"].replace(" ", "").replace("i", "j"))

    def assert_inverse(self, spectrum, expected, points=(0.2, 0.7, 1.6)):
        inverse = self.fields("InverseLaplace("+spectrum+",s,t)")
        self.assertNotIn("inverselaplace(", inverse["function"])
        recovered = self.algebra(inverse)
        for time in points:
            with self.subTest(spectrum=spectrum, time=time):
                # Also reparse the inverse result: a correct tree with wrong signs in its
                # displayed expression is not a successful user-visible round trip.
                actual = self.number(self.fields("{"+recovered+f" | t={time}"+"}"))
                self.assertLess(abs(actual-expected(time)), 2e-11*(1+abs(expected(time))))
        return recovered

    def test_fractional_source_functions_round_trip(self):
        cases = (
            ("sqrt(t)", math.sqrt),
            ("cubrt(t)", lambda t: t**(1/3)),
            ("t^(2/3)", lambda t: t**(2/3)),
            ("t^(-1/3)", lambda t: t**(-1/3)),
            ("3*sqrt(t)", lambda t: 3*math.sqrt(t)),
            ("log10(t)", math.log10),
            ("abs(2*t-1)", lambda t: abs(2*t-1)),
        )
        for source, expected in cases:
            with self.subTest(source=source):
                forward = self.fields("Laplace("+source+",t,s)", "s")
                self.assertNotIn("laplace(", forward["function"])
                self.assert_inverse(self.algebra(forward), expected)

    def test_independent_fractional_and_shifted_spectra(self):
        for spectrum, power, scale, offset in (
            ("1/s^(3/2)", 1.5, 1, 0),
            ("s^(-4/3)", 4/3, 1, 0),
            ("1/sqrt(s)", 0.5, 1, 0),
            ("1/cubrt(s)", 1/3, 1, 0),
            ("(s+2)^(-3/2)", 1.5, 1, 2),
            ("1/(2*s+3)^(5/4)", 1.25, 2, 3),
        ):
            with self.subTest(spectrum=spectrum):
                self.assert_inverse(spectrum, lambda t: (
                    t**(power-1)*math.exp(-offset*t/scale)/(scale**power*math.gamma(power))))

    def test_scalar_association_and_decimal_logarithm(self):
        for spectrum in ("-(ln(s)+@gamma)/(ln(10)*s)",
                         "(-ln(s)-@gamma)/s/ln(10)",
                         "-(ln(s)+@gamma)*(1/(s*ln(10)))"):
            with self.subTest(spectrum=spectrum):
                self.assert_inverse(spectrum, math.log10)
        for spectrum in ("3/(2*s^(3/2))", "(3/2)*s^(-3/2)", "3/s^(3/2)/2"):
            self.assert_inverse(spectrum, lambda t: 3*math.sqrt(t/math.pi))

    def test_nested_integer_powers_of_rational_spectra(self):
        for spectrum in ("(1/s^2)^2", "(s^(-2))^2", "1/(s^2)^2"):
            self.assert_inverse(spectrum, lambda t: t**3/6)
        self.assert_inverse("(1/(s+2))^3", lambda t: t*t*math.exp(-2*t)/2)
        self.assert_inverse("((s+1)/s^2)^2", lambda t: t+t*t+t**3/6)
        self.assert_inverse("((s+1)^2)^(-2)", lambda t: t**3*math.exp(-t)/6)
        forward = self.fields("Laplace(causal_convolve(t,t,t),t,s)", "s")
        self.assertNotIn("laplace(", forward["function"])
        self.assert_inverse(self.algebra(forward), lambda t: t**3/6)

    def test_unbound_hyperbolic_powers_round_trip_through_every_supported_order(self):
        # Do not bind s: the original small-power construction stalled during
        # symbolic rational beautification, which numerical-only tests bypassed.
        for name, function in (("sinh", math.sinh), ("cosh", math.cosh)):
            for order in range(17):
                with self.subTest(name=name, order=order):
                    forward = self.fields(f"Laplace({name}(t)^{order},t,s)", "s")
                    self.assertNotIn("laplace(", forward["function"])
                    if order > 1:
                        self.assertIn("sum(", forward["function"])
                    self.assert_inverse(self.algebra(forward), lambda t: function(t)**order)

    def test_unbound_affine_hyperbolic_powers_and_half_planes(self):
        for name, function in (("sinh", math.sinh), ("cosh", math.cosh)):
            for order, rate, offset in ((3, -2, 0.25), (4, 0.5, -0.25), (16, 1, 0.25)):
                with self.subTest(name=name, order=order, rate=rate):
                    source = f"{name}({rate}*t+({offset}))^{order}"
                    forward = self.fields("Laplace("+source+",t,s)", "s")
                    self.assertNotIn("laplace(", forward["function"])
                    self.assert_inverse(self.algebra(forward), lambda t: function(rate*t+offset)**order)
                    # The original spectral conditions, not a stripped test formula,
                    # must reject targets to the left of n*abs(Re(a)).
                    invalid = self.fields("{"+forward["unbound"]+
                                          f" | s={order*abs(rate)-0.5}"+"}", "s")
                    self.assertEqual(invalid["value"], "NAN")

    def test_independent_finite_sum_inverse_preserves_index_scope(self):
        self.assert_inverse("sum(k,0,3,(-1)^k*binomial(3,k)/(s-(3-2*k)))/8", lambda t: math.sinh(t)**3)
        self.assert_inverse("sum(k,0,3,binomial(3,k)/(s-(3-2*k)))/8", lambda t: math.cosh(t)**3)
        self.assert_inverse("sum(k,1,3,k/(s+k))", lambda t: sum(k*math.exp(-k*t) for k in range(1, 4)))

    def test_causal_delays_and_reciprocal_exponentials(self):
        for spectrum in ("exp(-s/2)/s^2", "1/(exp(s/2)*s^2)", "exp(1-s/2)/(exp(1)*s^2)"):
            with self.subTest(spectrum=spectrum):
                self.assert_inverse(spectrum, lambda t: max(0, t-0.5), points=(0.2, 0.7, 1.6))
        self.assert_inverse("exp(-2*s)/(s+1)",
                            lambda t: math.exp(-(t-2)) if t > 2 else 0, points=(0.2, 1.6, 2.4))
        self.assert_inverse("exp(-s/2)/s^(3/2)",
                            lambda t: 2*math.sqrt((t-0.5)/math.pi) if t > 0.5 else 0)

    def test_linearity_through_a_common_denominator(self):
        self.assert_inverse("(s-2+4*exp(-s/2))/s^2", lambda t: abs(2*t-1))
        self.assert_inverse("(1+exp(-s))/s", lambda t: 1+(t > 1))

    def test_symbolic_power_and_delay_keep_their_domains(self):
        power = self.fields("@Linv{s^(-p)}")
        self.assertNotIn("inverselaplace(", power["function"])
        self.assertIn("Re(p) > 0", power["expression"])
        delay = self.fields("@Linv{exp(-c*s)/s^2}")
        self.assertNotIn("inverselaplace(", delay["function"])
        self.assertIn("Re(c) > 0", delay["expression"])
        self.assertIn("c ∈ ℝ", delay["expression"])
        for time in (0.2, 0.8):
            actual = self.number(self.fields("{@Linv{exp(-c*s)/s^2}"+f" | t={time}; c=1/2"+"}"))
            self.assertLess(abs(actual-max(0, time-0.5)), 1e-12)

    def test_conditioned_spectrum_preserves_parameter_guards(self):
        # Binding blocks are top-level syntax; use an inline domain inside the transform.
        spectrum = "(s^(-p) where (Re(s)>0; Re(p)>0))"
        result = self.fields("InverseLaplace("+spectrum+",s,t)")
        self.assertNotIn("inverselaplace(", result["function"])
        self.assertIn("Re(p) > 0", result["expression"])
        self.assertNotIn("Re(s)", result["expression"])

    def test_unsupported_branch_choices_are_not_assumed(self):
        for spectrum in ("s^(1/2)", "1/(-s+2)^(1/2)", "exp(s)/s", "exp(-i*s)/s"):
            with self.subTest(spectrum=spectrum):
                result = self.fields("@Linv{"+spectrum+"}")
                self.assertIn("inverselaplace(", result["function"])


if __name__ == "__main__":
    unittest.main()
