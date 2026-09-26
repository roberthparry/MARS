"""Occurrence-specific native distribution notation and copied Expression round trips."""

import math
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import mars_lab


class DistributionQualifierTests(unittest.TestCase):
    def fields(self, source, variable="x"):
        result, raw, code = mars_lab.run_mars_lab_fields(
            mars_lab.DEFAULT_BIN, source, 40, variable, "evaluate")
        self.assertEqual(code, 0, raw)
        return result

    def assert_round_trip(self, source, variable="x"):
        original = self.fields(source, variable)
        copied = self.fields(original["expression"], variable)
        local = self.fields(original["unbound"], variable)
        self.assertEqual(copied["function"], original["function"])
        for candidate in (copied, local):
            self.assertEqual(candidate["tex"], original["tex"])
            self.assertEqual(candidate["unbound"], original["unbound"])
        for name in ("principal_value(", "finite_part(", "PV(", "Fp("):
            self.assertNotIn(name, original["expression"])
            self.assertNotIn(name, original["function"])
        self.assertNotIn(r"\operatorname{PV}", original["tex"])
        self.assertNotIn(r"\operatorname{Fp}", original["tex"])
        return original

    def test_single_qualification_shares_domain_area(self):
        for source, label in (("PV(1/x)", "principal value"), ("Fp(1/abs(x))", "finite part")):
            with self.subTest(source=source):
                result = self.assert_round_trip(source+" where (x ∈ ℝ)")
                body, qualifications = result["expression"].split(" | ", 1)
                self.assertNotIn(label, body)
                self.assertIn("x ∈ ℝ", qualifications)
                self.assertIn(" : "+label, qualifications)
                self.assertIn(" : "+label, result["function"])
                self.assertIn(r"\text{"+label+"}", result["tex"])

    def test_aliases(self):
        for first, second in (("PV", "principal_value"), ("Fp", "finite_part")):
            self.assertEqual(self.fields(first+"(1/x)")["expression"],
                             self.fields(second+"(1/x)")["expression"])

    def test_duplicates_mixed_and_nested_operators(self):
        cases = (
            "PV(1/x)+1/x", "PV(1/x)-1/x", "PV(1/x)+PV(1/x)",
            "PV(1/x)+finite_part(1/x)", "PV(1/x)*finite_part(1/x)",
            "PV(finite_part(1/x))", "finite_part(PV(1/x))", "PV(PV(1/x))",
            "PV(1/x+1/(x+1))", "PV(1/x)^2", "1/PV(1/x)",
            "sin(PV(1/x))", "PV(sin(x))+sin(x)", "PV(x*y)+y*x",
            "PV(1/x)+finite_part(1/abs(x))+1/x", "finite_part(1/abs(x))-1/abs(x)",
        )
        for source in cases:
            with self.subTest(source=source):
                result = self.assert_round_trip(source)
                self.assertTrue(" : principal value" in result["function"] or " : finite part" in result["function"])

    def test_unwrapped_duplicate_remains_unwrapped(self):
        mixed = self.assert_round_trip("PV(1/x)-1/x")
        self.assertEqual(mixed["function"].count(" : principal value"), 1)
        self.assertIn(" : principal value", mixed["expression"].split(" | ", 1)[0])
        self.assertIn(r"\underbrace{", mixed["tex"])
        self.assertNotEqual(mixed["unbound"], "0")

    def test_nested_order_is_preserved(self):
        first = self.assert_round_trip("PV(finite_part(1/x))")
        second = self.assert_round_trip("finite_part(PV(1/x))")
        self.assertNotEqual(first["function"], second["function"])
        self.assertIn("((1/x : finite part) : principal value)", first["function"])
        self.assertIn("((1/x : principal value) : finite part)", second["function"])

    def test_grouping_preserves_scope_before_simplification(self):
        for source in ("{ (1/x)-1/x | x=?; 1/x : principal value }",
                       "{ (1/x)^2 | x=?; 1/x : principal value }",
                       "{ 1/(1/x) | x=?; 1/x : finite part }"):
            with self.subTest(source=source):
                result = self.assert_round_trip(source)
                self.assertTrue(" : principal value" in result["function"] or " : finite part" in result["function"])

    def test_invalid_or_ambiguous_qualifications_are_rejected(self):
        for source in ("{ (1/x)+(1/x) | x=?; 1/x : principal value }",
                       "{ 1/x | x=?; 1/x : principal value }",
                       "{ (1/x) | x=?; 1/x : unknown }",
                       "(1/x : principal value extra)"):
            with self.subTest(source=source):
                _, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, "x", "evaluate")
                self.assertNotEqual(code, 0, raw)

    def test_distribution_regular_values_preserve_symbolic_nodes(self):
        for operator in ("PV", "finite_part"):
            for operand, expected in (("1", 1), ("1/x", 0.5), ("i/x", 0.5j)):
                result = self.fields("{"+operator+"("+operand+") | x=2}")
                if operand != "1":
                    self.assertIn(" : principal value" if operator == "PV" else " : finite part", result["function"])
                self.assertEqual(self.value(result), expected)
                self.assertNotIn("value_note", result)
            for point in ("0", "?", "NAN", "inf"):
                # Infinity is not a finite coordinate at which to evaluate an impulse.
                source = "delta(x)" if point == "inf" else operator+"(1/x)"
                result = self.fields("{"+source+" | x="+point+"}")
                self.assertEqual(result["value"], "NAN")

    @staticmethod
    def value(result):
        return complex(result["value"].replace(" ", "").replace("i", "j"))

    def test_impulse_support_and_derivatives(self):
        for source in ("delta(x-2)", "delta(3*x-6)", "Derivative(delta(x-2),3)",
                       "Derivative(delta(x-2),x,1)", "Derivative(delta(x-2),x,2)",
                       "Derivative(delta(x-2),n)"):
            for point in (-1, 0, 2, 3):
                with self.subTest(source=source, point=point):
                    result = self.fields("{"+source+f" | x={point}; n=3"+"}")
                    if point == 2:
                        self.assertEqual(result["value"], "NAN")
                    else:
                        self.assertEqual(self.value(result), 0)
                    self.assertIn("δ(", result["expression"])
        for point in ("?", "i", "inf"):
            self.assertEqual(self.fields("{delta(x) | x="+point+"}")["value"], "NAN")
        for order in ("?", "-1", "1/2", "i"):
            self.assertEqual(self.fields("{Derivative(delta(x),n) | x=2; n="+order+"}")["value"], "NAN")
        self.assertEqual(self.value(self.fields("delta(1)")), 0)

    def test_regularised_derivatives_and_finite_sums(self):
        for source, expected in (("Derivative(PV(1/x),x,1)", -0.25),
                                 ("Derivative(finite_part(1/abs(x)),x,1)", -0.25),
                                 ("Derivative(PV(1/x),x,2)", 0.25),
                                 ("sum(n,1,2,finite_part(n/abs(x)))", 1.5)):
            with self.subTest(source=source):
                result = self.fields("{"+source+" | x=2}")
                self.assertEqual(self.value(result), expected)
                self.assertEqual(self.fields("{"+source+" | x=0}")["value"], "NAN")
        # Do not drop the original pole when differentiating a constant-numerator reciprocal.
        self.assertEqual(self.fields("{Derivative(PV(1/x),x,1) | x=?}")["value"], "NAN")

    def test_transform_values_off_singular_support(self):
        for source, point, expected in (
                ("@F{acosh(x)}", 1, -4.807878861268826),
                ("@F{acosh(x)}", -1, 0),
                ("@F{asin(x)}", 1, -4.807878861268826j),
                ("@F{acos(x)}", 1, 4.807878861268826j),
                ("@F{atanh(x)}", 1, -2j*math.pi*math.sin(1)),
                ("@F{ln(abs(x))}", 2, -math.pi/2),
                ("@F{step(x)}", 2, -0.5j),
                ("@F{1}", 2, 0),
                ("@F{x^3}", 2, 0),
                ("@F{sin(x)}", 2, 0)):
            # The constant input needs an explicit source coordinate; other inputs infer x -> k.
            formula = "Fourier(1,x,k)" if source == "@F{1}" else source
            with self.subTest(source=source, point=point):
                unbound = self.fields(formula, "k")
                bound = self.fields("{"+formula+f" | k={point}"+"}", "k")
                self.assertLess(abs(self.value(bound)-expected), 2e-12)
                self.assertNotIn("value_note", bound)
                self.assertEqual(bound["tex"], unbound["tex"])
                self.assertEqual(bound["unbound"], unbound["unbound"])
                copied = self.fields(bound["expression"], "k")
                self.assertLess(abs(self.value(copied)-expected), 2e-12)
        for formula, point in (("@F{acosh(x)}", 0), ("@F{ln(abs(x))}", 0),
                               ("@F{step(x)}", 0), ("@F{sin(x)}", 1), ("@F{cos(2*x)}", -2)):
            result = self.fields("{"+formula+f" | k={point}"+"}", "k")
            self.assertEqual(result["value"], "NAN")
            self.assertIn("singular", result["value_note"])

    def test_bound_spectrum_retains_inverse_information(self):
        spectrum = self.fields("{@F{acosh(x)} | k=1}", "k")
        inverse = self.fields("InverseFourier("+spectrum["expression"]+",k,x)")
        self.assertIn("acosh(x)", inverse["expression"])
        self.assertNotIn("Fourier(", inverse["function"])
        for point, expected in ((1, 0), (0, 0.5j*math.pi), (-1, 1j*math.pi)):
            bound = inverse["expression"].replace("x = NAN", f"x = {point}")
            self.assertLess(abs(self.value(self.fields(bound))-expected), 2e-12)

    def test_complete_copied_expression_inside_transform(self):
        for source in ("finite_part(1/abs(x))", "PV(tan(x))"):
            copied = self.fields(source)["expression"]
            direct = self.fields("Fourier("+source+",x,ω)", "ω")
            nested = self.fields("Fourier("+copied+",x,ω)", "ω")
            self.assertEqual(nested["unbound"], direct["unbound"])
            self.assertEqual(nested["tex"], direct["tex"])

    def test_bound_symbols_and_conditions_survive(self):
        result = self.assert_round_trip("{PV(1/(x+a)) | x=2; a=1/3; x ∈ ℝ}")
        self.assertIn("x = 2", result["expression"])
        self.assertIn("⅓", result["expression"])
        self.assertIn("x ∈ ℝ", result["expression"])

    def test_transform_shorthand_braces_keep_free_symbols(self):
        for shorthand, explicit, variable in (("@F{step(t)}", "Fourier(step(t),t,ω)", "ω"),
                                                ("@Finv{ln(abs(ω))}", "InverseFourier(ln(abs(ω)),ω,t)", "t")):
            self.assertEqual(self.fields(shorthand, variable)["unbound"],
                             self.fields(explicit, variable)["unbound"])


class ZZDistributionQualifierReadmeExamples(unittest.TestCase):
    def test_readme_distribution_regular_values(self):
        # README examples: docs/expression.md, numerical evaluation of distributions.
        for source, expected in (("{@F{acosh(x)} | k=1}", -4.807878861268826),
                                 ("{@F{acosh(x)} | k=-1}", 0),
                                 ("{@F{ln(abs(x))} | k=2}", -math.pi/2)):
            result, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, "k", "evaluate")
            self.assertEqual(code, 0, raw)
            self.assertLess(abs(DistributionQualifierTests.value(result)-expected), 2e-12)

    def test_readme_distribution_notation(self):
        # README examples: docs/expression.md; deliberately run after ordinary tests.
        for source, expected in (("PV(1/x)", "(1/x : principal value)"),
                                 ("Fp(1/abs(x))", "(1/|x| : finite part)")):
            fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, "x", "evaluate")
            self.assertEqual(code, 0, raw)
            self.assertEqual(fields["unbound"], expected)


if __name__ == "__main__":
    unittest.main()
