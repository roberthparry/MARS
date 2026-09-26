"""Gaussian/error-function inverse pairs from freshly parsed forward formulae."""

import cmath
import math
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import mars_lab


class InverseLaplaceGaussianTests(unittest.TestCase):
    def fields(self, source, variable="t"):
        fields, raw, code = mars_lab.run_mars_lab_fields(
            mars_lab.DEFAULT_BIN, source, 40, variable, "evaluate")
        self.assertEqual(code, 0, raw)
        return fields

    @staticmethod
    def body(fields):
        # Deliberately copy only the algebra, not a nested Laplace node or a cached result.
        return fields["expression"].split(" | ")[0].removeprefix("{ ")

    @staticmethod
    def value(fields):
        return complex(fields["value"].replace(" ", "").replace("i", "j"))

    def check_round_trip(self, source, expected, points=(0, 0.2, 0.75, 1.5)):
        forward = self.fields("Laplace(" + source + ",t,s)", "s")
        self.assertNotIn("laplace(", forward["function"])
        spectrum = self.body(forward)
        inverse_source = "InverseLaplace(" + spectrum + ",s,t)"
        recovered = self.fields(inverse_source)
        self.assertNotIn("inverselaplace(", recovered["function"])
        # Also reparse the inverse's displayed result; evaluating the inverse operator alone
        # would miss a sign, parenthesis or coefficient lost by serialisation.
        restored = self.body(recovered)
        for point in points:
            with self.subTest(source=source, time=point):
                for text in (inverse_source, restored):
                    actual = self.value(self.fields("{" + text + f" | t={point}" + "}"))
                    wanted = expected(point)
                    self.assertLess(abs(actual - wanted), 2e-12 * (1 + abs(wanted)))
        return spectrum, restored

    def test_copied_gaussian_formulae(self):
        cases = (
            ("exp(-t^2)", lambda t: math.exp(-t*t)),
            ("exp(-2*t^2+t+1)", lambda t: math.exp(-2*t*t+t+1)),
            ("exp(-(2*t+1)^2)", lambda t: math.exp(-(2*t+1)**2)),
            ("3*exp(-t^2/4-2*t)", lambda t: 3*math.exp(-t*t/4-2*t)),
            ("exp(-t^2+i*t)", lambda t: cmath.exp(-t*t+1j*t)),
            ("exp(-(1+i/2)*t^2+i*t)", lambda t: cmath.exp(-(1+0.5j)*t*t+1j*t)),
        )
        for source, expected in cases:
            with self.subTest(source=source):
                self.check_round_trip(source, expected)

    def test_copied_error_functions_with_both_scale_signs(self):
        for name, function in (("erf", math.erf), ("erfc", math.erfc)):
            for scale, offset in ((1, 0), (2, 1), (-2, 1), (0.5, -1)):
                source = f"{name}({scale}*t+({offset}))"
                with self.subTest(source=source):
                    self.check_round_trip(source, lambda t: function(scale*t+offset))

    def test_copied_normal_distributions_with_both_scale_signs(self):
        for scale, offset in ((1, 0), (2, 1), (-2, 1), (0.5, -1)):
            pdf = lambda t: math.exp(-(scale*t+offset)**2/2)/math.sqrt(2*math.pi)
            cdf = lambda t: math.erfc(-(scale*t+offset)/math.sqrt(2))/2
            for name, function in (("normal_pdf", pdf), ("normal_cdf", cdf)):
                source = f"{name}({scale}*t+({offset}))"
                with self.subTest(source=source):
                    self.check_round_trip(source, function)

    def test_exact_nested_radical_scales_in_copied_normal_cdf(self):
        # This is the unsimplified reciprocal scale emitted by the normal-CDF rule.
        # Its square must be compared algebraically, not by a floating-point tolerance.
        cases = (
            ("(1+exp(s^2/(4*(-1/sqrt(2))^2))*erfc(s/(2*sqrt((-1/sqrt(2))^2))))/(2*s)",
             lambda t: math.erfc(-t/math.sqrt(2))/2),
            ("exp(s^2/(4*(-1/sqrt(3))^2))*erfc(s/(2*sqrt((-1/sqrt(3))^2)))/s",
             lambda t: math.erf(t/math.sqrt(3))),
            ("exp(s^2/(4*(-2/sqrt(5))^2))*erfc(s/(2*sqrt((-2/sqrt(5))^2)))/s",
             lambda t: math.erf(2*t/math.sqrt(5))),
        )
        for spectrum, expected in cases:
            with self.subTest(spectrum=spectrum):
                result = self.fields("InverseLaplace("+spectrum+",s,t)")
                self.assertNotIn("inverselaplace(", result["function"])
                for point in (0, 0.3, 1.1):
                    actual = self.value(self.fields("{"+self.body(result)+f" | t={point}"+"}"))
                    self.assertLess(abs(actual-expected(point)), 2e-12)

        different = ("exp(s^2/(4*(-1/sqrt(2))^2)+s^2/100000000000000000000)"
                     "*erfc(s/(2*sqrt((-1/sqrt(2))^2)))/s")
        result = self.fields("InverseLaplace("+different+",s,t)")
        self.assertIn("inverselaplace(", result["function"])

    def test_copied_polynomial_weighted_gaussians(self):
        cases = (
            ("t*exp(-t^2)", lambda t: t*math.exp(-t*t)),
            ("t*exp(-2*t^2+t+1)", lambda t: t*math.exp(-2*t*t+t+1)),
            ("t^2*exp(-t^2)", lambda t: t*t*math.exp(-t*t)),
            ("t*exp(-t^2+i*t)", lambda t: t*cmath.exp(-t*t+1j*t)),
        )
        for source, expected in cases:
            with self.subTest(source=source):
                self.check_round_trip(source, expected)

    def test_copied_exponentially_modulated_error_functions(self):
        cases = (
            ("exp(-t)*erf(t)", lambda t: math.exp(-t)*math.erf(t)),
            ("exp(-2*t)*erfc(2*t-1)", lambda t: math.exp(-2*t)*math.erfc(2*t-1)),
            ("exp(t)*erf(-2*t+1)", lambda t: math.exp(t)*math.erf(-2*t+1)),
            ("exp(i*t)*normal_cdf(-2*t+1)",
             lambda t: cmath.exp(1j*t)*math.erfc((2*t-1)/math.sqrt(2))/2),
        )
        for source, expected in cases:
            with self.subTest(source=source):
                self.check_round_trip(source, expected)

    def test_independent_weighted_and_frequency_shifted_formulae(self):
        cases = (
            ("1/2-sqrt(@pi)*s/4*exp(s^2/4)*erfc(s/2)", lambda t: t*math.exp(-t*t)),
            ("sqrt(@pi)/4*(1+s^2/2)*exp(s^2/4)*erfc(s/2)-s/4",
             lambda t: t*t*math.exp(-t*t)),
            ("exp((s+1)^2/4)*erfc((s+1)/2)/(s+1)", lambda t: math.exp(-t)*math.erf(t)),
            ("2*exp((2*s+2)^2/16)*erfc((2*s+2)/4)/(2*s+2)",
             lambda t: math.exp(-t)*math.erf(t)),
            ("-sqrt(2)*sqrt(@pi)/16*((s-1)*erfc((s-1)/(2*sqrt(2)))"
             "*exp(((s-1)/(2*sqrt(2)))^2+1)-2*sqrt(2)*exp(1)/sqrt(@pi))",
             lambda t: t*math.exp(-2*t*t+t+1)),
            ("-¹⁄₁₆√2·√(π)·((s-1)·erfc((s-1)/(2·√(2)))"
             "·exp(((s-1)/(2·√(2)))²+1)-2/√π·exp(1)·√(2))",
             lambda t: t*math.exp(-2*t*t+t+1)),
        )
        for spectrum, expected in cases:
            with self.subTest(spectrum=spectrum):
                result = self.fields("InverseLaplace("+spectrum+",s,t)")
                self.assertNotIn("inverselaplace(", result["function"])
                for point in (0, 0.4, 1.25):
                    actual = self.value(self.fields("{"+self.body(result)+f" | t={point}"+"}"))
                    self.assertLess(abs(actual-expected(point)), 2e-12)

    def test_symbolic_frequency_shift_keeps_the_whole_spectrum_consistent(self):
        source = "exp(-k*t)*erf(t)"
        spectrum = self.body(self.fields("Laplace("+source+",t,s)", "s"))
        result = self.fields("InverseLaplace("+spectrum+",s,t)")
        self.assertNotIn("inverselaplace(", result["function"])
        for rate in ("2", "-1", "i/2"):
            bindings = f" | t=0.4; k={rate}"
            expected = self.value(self.fields("{"+source+bindings+"}"))
            actual = self.value(self.fields("{"+self.body(result)+bindings+"}"))
            self.assertLess(abs(actual-expected), 2e-12)

    def test_cartesian_exponential_spectrum_recovers_complex_gaussian(self):
        # exp((s-i)^2/4) written in the Cartesian style used by native output.
        spectrum = ("sqrt(@pi)/2*exp((s^2-1)/4)*(cos(s/2)-i*sin(s/2))"
                    "*erfc((s-i)/2)")
        result = self.fields("InverseLaplace("+spectrum+",s,t)")
        self.assertNotIn("inverselaplace(", result["function"])
        for point in (0, 0.3, 1.1):
            actual = self.value(self.fields("{"+self.body(result)+f" | t={point}"+"}"))
            self.assertLess(abs(actual-cmath.exp(-point*point+1j*point)), 2e-12)

    def test_symbolic_error_kernel_cancels_reciprocal_radical_factors(self):
        spectrum = ("(erf(b)+a/sqrt(a^2)*exp(s^2/(4*a^2)+b*s/a)"
                    "*erfc(s/(2*sqrt(a^2))+b*sqrt(a^2)/a))/s")
        result = self.fields("InverseLaplace("+spectrum+",s,t)")
        self.assertNotIn("inverselaplace(", result["function"])
        for scale in (-2, 2):
            bindings = f" | t=0.3; a={scale}; b=1"
            actual = self.value(self.fields("{"+self.body(result)+bindings+"}"))
            self.assertLess(abs(actual-math.erf(scale*0.3+1)), 2e-12)

    def test_initial_value_impulses_are_not_silently_discarded(self):
        kernel = "sqrt(@pi)*s/4*exp(s^2/4)*erfc(s/2)"
        for spectrum in (kernel, "1/3-"+kernel,
                         "1/2+1/100000000000000000000-"+kernel):
            with self.subTest(spectrum=spectrum):
                result = self.fields("InverseLaplace("+spectrum+",s,t)")
                # These spectra retain an impulse at the origin. Until distributional
                # Laplace inverses are represented, the result must stay unresolved.
                self.assertIn("inverselaplace(", result["function"])

    def test_independently_written_kernel_and_its_integral(self):
        # L^-1{exp((c*s+d)^2)*erfc(c*s+d)} for c=1/4, d=1.
        spectrum = "exp((s/4+1)^2)*erfc(s/4+1)"
        inverse = self.fields("InverseLaplace(" + spectrum + ",s,t)")
        self.assertNotIn("inverselaplace(", inverse["function"])
        integral = self.fields("InverseLaplace((" + spectrum + ")/s,s,t)")
        self.assertNotIn("inverselaplace(", integral["function"])
        for point in (0.1, 0.5, 1):
            expected = 4/math.sqrt(math.pi)*math.exp(-4*point*point-4*point)
            actual = self.value(self.fields("{" + self.body(inverse) + f" | t={point}" + "}"))
            self.assertLess(abs(actual-expected), 2e-12)
            expected_integral = math.e*(math.erf(2*point+1)-math.erf(1))
            actual = self.value(self.fields("{" + self.body(integral) + f" | t={point}" + "}"))
            self.assertLess(abs(actual-expected_integral), 2e-12)

    def test_symbolic_gaussian_retains_parameter_restrictions(self):
        source = "exp(-a*t^2+b*t+d)"
        forward = self.fields("Laplace("+source+",t,s)", "s")
        spectrum = self.body(forward)
        recovered = self.fields("InverseLaplace("+spectrum+",s,t)")
        self.assertNotIn("inverselaplace(", recovered["function"])
        self.assertIn("realpart", recovered["function"])
        for a, b, d in (("2", "3", "-1"), ("1+i/2", "i", "0")):
            bindings = f" | t=0.4; a={a}; b={b}; d={d}"
            actual = self.value(self.fields("{"+self.body(recovered)+bindings+"}"))
            expected = self.value(self.fields("{"+source+bindings+"}"))
            self.assertLess(abs(actual-expected), 2e-12)
        # The guards themselves must survive a fresh parse, rather than silently returning
        # a growing Gaussian outside the forward family's supported convergence sector.
        copied = recovered["expression"].replace("t = NAN", "t = 0.4").replace("t = ?", "t = 0.4")
        for name, value in (("a", "-1"), ("b", "0"), ("d", "0")):
            copied = copied.replace(name+" = NAN", name+" = "+value).replace(name+" = ?", name+" = "+value)
        self.assertEqual(self.fields(copied)["value"], "NAN")

    def test_symbolic_error_function_formulae(self):
        for name in ("erf", "erfc", "normal_pdf", "normal_cdf"):
            source = name+"(a*t+b)"
            spectrum = self.body(self.fields("Laplace("+source+",t,s)", "s"))
            inverse = "InverseLaplace("+spectrum+",s,t)"
            result = self.fields(inverse)
            self.assertNotIn("inverselaplace(", result["function"])
            for scale in (2, -2):
                bindings = f" | t=0.4; a={scale}; b=1"
                actual = self.value(self.fields("{"+self.body(result)+bindings+"}"))
                expected = self.value(self.fields("{"+source+bindings+"}"))
                self.assertLess(abs(actual-expected), 2e-12)

    def test_nonmatching_exponents_and_wrong_root_branch_remain_symbolic(self):
        for spectrum in (
            "exp(s^2)*erfc(-s)",
            "exp(-s^2)*erfc(i*s)",
            "exp(s^2/4+s)*erfc(s/2)",
            "exp(s^2/3)*erfc(s/2)",
            "exp(s^2/4)*erfc(s/2)^2",
        ):
            with self.subTest(spectrum=spectrum):
                fields = self.fields("InverseLaplace("+spectrum+",s,t)")
                self.assertIn("inverselaplace(", fields["function"])


if __name__ == "__main__":
    unittest.main()
