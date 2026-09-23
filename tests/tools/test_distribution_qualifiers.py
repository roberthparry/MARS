"""Occurrence-specific native distribution notation and copied Expression round trips."""

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

    def test_distribution_has_no_pointwise_value(self):
        for operator in ("PV", "finite_part"):
            for operand in ("1", "1/x"):
                result = self.fields("{"+operator+"("+operand+") | x=2}")
                self.assertIn(" : principal value" if operator == "PV" else " : finite part", result["function"])
                self.assertIn(result.get("value", "NAN").upper(), ("NAN", ""))

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
    def test_readme_distribution_notation(self):
        # README examples: docs/expression.md; deliberately run after ordinary tests.
        for source, expected in (("PV(1/x)", "(1/x : principal value)"),
                                 ("Fp(1/abs(x))", "(1/|x| : finite part)")):
            fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, "x", "evaluate")
            self.assertEqual(code, 0, raw)
            self.assertEqual(fields["unbound"], expected)


if __name__ == "__main__":
    unittest.main()
