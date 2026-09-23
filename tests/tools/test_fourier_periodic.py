"""Principal-value periodic Fourier pairs, with genuinely copied spectra."""

import math
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import mars_lab


def fields(source, variable="ω"):
    result, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, variable, "evaluate")
    if code:
        raise AssertionError(raw)
    return result


def algebra(result):
    return result["expression"].split(" | ", 1)[0].removeprefix("{ ")


def numerical(source, point, bindings=""):
    result = fields("{"+source+f" | t={point}"+bindings+"}", "t")
    return complex(result["value"].replace(" ", "").replace("i", "j"))


class PeriodicFourierTests(unittest.TestCase):
    def copied_inverse(self, source, forward="Fourier", inverse="InverseFourier"):
        spectrum = fields(f"{forward}({source},t,ω)")
        self.assertNotIn("Fourier(", spectrum["function"])
        self.assertIn("\\sum", spectrum["tex"])
        result = fields(f"{inverse}("+algebra(spectrum)+",ω,t)", "t")
        self.assertNotIn("Fourier(", result["function"])
        self.assertNotIn("principal_value(", result["tex"])
        self.assertIn("≠ 0", result["expression"])
        for card in ("expression", "tex", "function"):
            self.assertNotIn("principal value", result[card])
        return spectrum, result

    def test_copied_periodic_pairs_both_directions(self):
        for function in ("tan", "cot"):
            for forward, inverse in (("Fourier", "InverseFourier"), ("InverseFourier", "Fourier")):
                for argument in ("t", "2*t+1", "-2*t+1", "t/2-1/3"):
                    with self.subTest(function=function, argument=argument, direction=forward):
                        spectrum, result = self.copied_inverse(f"{function}({argument})", forward, inverse)
                        self.assertIn(function+"(", result["expression"])
                        reparsed = fields(result["expression"], "t")
                        self.assertEqual(result["tex"], reparsed["tex"])
                        # The inverse is usable as an ordinary function on its stated domain.
                        for point in (-0.3, 0.7):
                            expected = numerical(f"{function}({argument})", point)
                            self.assertLess(abs(numerical(algebra(result), point)-expected),
                                            1e-12*(1+abs(expected)))
                        # Copy the complete domain before transforming again.
                        repeated = fields(f"{forward}("+result["expression"]+",t,ω)")
                        self.assertNotIn("Fourier(", repeated["function"])

    def test_symbolic_real_scale_and_shift(self):
        for function in ("tan", "cot"):
            spectrum, result = self.copied_inverse(f"{function}(a*t+b)")
            self.assertIn("a ∈ ℝ", spectrum["expression"])
            self.assertIn("b ∈ ℝ", spectrum["expression"])
            self.assertNotIn("n =", result["expression"])
            for scale, shift in ((2, 0.3), (-2, 0.3), (0.5, -0.2)):
                bindings = f"; a={scale}; b={shift}"
                expected = numerical(f"{function}(a*t+b)", 0.7, bindings)
                self.assertLess(abs(numerical(algebra(result), 0.7, bindings)-expected), 1e-12)

    def test_spatial_coordinates_round_trip_with_inferred_and_explicit_inverse(self):
        for coordinate, frequency in (("x", "k"), ("y", "m"), ("z", "n")):
            spectrum = fields("@F{tan("+coordinate+")}", frequency)
            for inverse in ("@Finv{"+algebra(spectrum)+"}",
                            "InverseFourier("+algebra(spectrum)+","+frequency+","+coordinate+")"):
                result = fields(inverse, coordinate)
                self.assertNotIn("Fourier(", result["function"])
                self.assertIn("tan("+coordinate+")", result["expression"])
                self.assertIn("cos("+coordinate+") ≠ 0", result["expression"])
                self.assertNotIn("principal value", result["function"])

    def test_native_pole_conditions_and_values(self):
        for function, denominator in (("tan", "cos"), ("cot", "sin")):
            _, result = self.copied_inverse(function+"(t)")
            self.assertIn(denominator+"(t) ≠ 0", result["expression"])
            self.assertIn(denominator+"(t) != 0", result["function"])
            self.assertIn("return "+function+"(t).", result["function"])
            self.assertIn(r"\ne 0", result["tex"])
            for point in (-0.3, 0.7):
                # Evaluate the complete copied result, including its domain, not an unqualified projection.
                bound = result["expression"].replace("t = NAN", "t = "+str(point))
                actual = float(fields(bound, "t")["value"])
                expected = math.tan(point) if function == "tan" else 1/math.tan(point)
                self.assertAlmostEqual(actual, expected, places=12)
            if function == "cot":
                at_pole = result["expression"].replace("t = NAN", "t = 0")
                self.assertEqual(fields(at_pole, "t")["value"], "NAN")

    def test_nonzero_condition_syntax_round_trips(self):
        for condition in ("x ≠ 0", "x != 0", "Re(abs(x)) > 0"):
            result = fields("{1/x | x=2; "+condition+"}", "x")
            self.assertIn("x ≠ 0", result["expression"])
            self.assertEqual(result["value"], "0.5")
            self.assertEqual(fields(result["expression"], "x")["tex"], result["tex"])

    def test_arbitrary_domain_restrictions_are_not_discarded(self):
        result = fields("Fourier(tan(t) where (Re(t)>0),t,ω)")
        self.assertIn("Fourier(", result["function"])

    def test_parameter_pole_conditions_are_retained(self):
        result = fields("Fourier(tan(a) where (a ∈ ℝ; cos(a) ≠ 0),t,ω)")
        self.assertIn("cos(a) ≠ 0", result["expression"])
        self.assertIn("cos(a) != 0", result["function"])

    def test_symbolic_free_binding_is_not_substituted(self):
        result = fields("{Fourier(tan(a*t),t,ω) | a=@pi/3}")
        body, bindings = result["expression"].split(" | ", 1)
        self.assertIn("ω/a", body)
        self.assertIn("a = π/3", bindings)
        self.assertIn("a ∈ ℝ", bindings)

    def test_symbolic_numeric_quotient_keeps_denominator_grouping(self):
        result = fields("{Fourier(tan(a*t),t,ω) | ; a=@pi/3}")
        self.assertIn("ω/(π/3)", result["expression"])
        copied = fields(result["expression"])
        self.assertEqual(copied["tex"], result["tex"])

    def test_independent_handwritten_spectra(self):
        for function, coefficient, weight in (("tan", "2*@pi*i", "(-1)^j"), ("cot", "-2*@pi*i", "1")):
            term = f"{weight}*(delta(ω-2*j)-delta(ω+2*j))"
            spectrum = coefficient+"*sum(j,1,@inf,"+term+")"
            result = fields("InverseFourier("+spectrum+",ω,t)", "t")
            self.assertNotIn("Fourier(", result["function"])
            self.assertIn(function+"(t)", result["expression"])

    def test_fourier_coefficients_by_independent_quadrature(self):
        # On one period the sine cancels each simple pole. Simpson quadrature
        # checks the sign, alternating weights and 2*pi normalisation independently.
        steps = 4096
        for function in ("tan", "cot"):
            spectrum = fields("Fourier("+function+"(t),t,ω)")
            self.assertTrue(spectrum["expression"].startswith("{ "+("2iπ" if function == "tan" else "-2iπ")))
            self.assertIn("δ(ω - 2n)", spectrum["expression"])
            self.assertIn("δ(ω + 2n)", spectrum["expression"])
            self.assertEqual("(-1)^n" in spectrum["expression"], function == "tan")
            for order in range(1, 7):
                start = -math.pi/2 if function == "tan" else 0
                step = math.pi/steps
                def integrand(index):
                    if index in (0, steps):
                        return -2*order*(-1)**order if function == "tan" else 2*order
                    x = start+index*step
                    return (math.tan(x) if function == "tan" else 1/math.tan(x))*math.sin(2*order*x)
                integral = integrand(0)+integrand(steps)
                integral += sum((4 if j % 2 else 2)*integrand(j) for j in range(1, steps))
                coefficient = -2j*integral*step/3
                expected = 2j*math.pi*(-1)**order if function == "tan" else -2j*math.pi
                self.assertLess(abs(coefficient-expected), 1e-10)

    def test_finite_or_altered_series_are_not_matched(self):
        cases = (
            "sum(j,1,4,(-1)^j*(delta(ω-2*j)-delta(ω+2*j)))",
            "sum(j,1,@inf,(-1)^j*(delta(ω-2*j)+delta(ω+2*j)))",
            "sum(j,1,@inf,(-1)^j*(delta(ω-2*j)-delta(ω+3*j)))",
            "sum(j,2,@inf,(-1)^j*(delta(ω-2*j)-delta(ω+2*j)))",
        )
        for spectrum in cases:
            with self.subTest(spectrum=spectrum):
                result = fields("InverseFourier("+spectrum+",ω,t)", "t")
                self.assertNotIn("tan(", result["function"])
                self.assertNotIn("cot(", result["function"])

    def test_series_dummy_does_not_capture_a_parameter(self):
        spectrum = fields("Fourier(tan(t+n),t,ω)")
        self.assertNotIn("Σ_(n=", spectrum["expression"])
        restored = fields("InverseFourier("+algebra(spectrum)+",ω,t)", "t")
        self.assertNotIn("Fourier(", restored["function"])
        self.assertIn("n", restored["expression"])

    def test_principal_value_diagnostic(self):
        for function in ("tan", "cot"):
            result = fields("@F{"+function+"(x)}", "k")
            self.assertIn("principal values", result["value_note"])
            self.assertIn("distributional", result["value_note"])

    def test_nonreal_rates_do_not_use_real_pole_prescription(self):
        for source in ("tan(i*t)", "cot(t+i)"):
            result = fields("Fourier("+source+",t,ω)")
            self.assertIn("Fourier(", result["function"])
            self.assertNotIn("\\sum", result["tex"])


class ZZPeriodicReadmeExamples(unittest.TestCase):
    def test_readme_tangent_pair(self):
        # README example: docs/expression.md, periodic principal-value transforms.
        spectrum = fields("@F{tan(x)}", "k")
        self.assertIn("δ(k - 2n)", spectrum["expression"])
        self.assertIn("(-1)^n", spectrum["expression"])
        inverse = fields("InverseFourier("+algebra(spectrum)+",k,x)", "x")
        self.assertIn("tan(x)", inverse["expression"])
        self.assertIn("cos(x) ≠ 0", inverse["expression"])
        self.assertNotIn("principal value", inverse["expression"])
        self.assertNotIn("Fourier(", inverse["function"])


if __name__ == "__main__":
    unittest.main()
