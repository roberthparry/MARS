"""Native Gaussian transforms and the distinction between divergence and unsupported formulas."""

import math
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import mars_lab


class LaplaceGaussianTests(unittest.TestCase):
    def test_transform_antiderivatives_retain_their_domains(self):
        for source in ("@L{sin^2(1/2@pit)}", "@L{sin^2((@pi*t)/2)}", "@L{exp(-t)}", "@L{sin(t)}"):
            with self.subTest(source=source):
                fields = self.evaluate(source, "integral")
                self.assertTrue(fields["integral"].startswith("∫ds = "), fields["integral"])
                self.assertIn("Re(s)", fields["integral"])
                self.assertIn("C", fields["integral"])
                copied = fields["integral"].split(" = ", 1)[1].replace("s = NAN", "s = 2")
                derivative = self.evaluate(copied, "derivative")
                expected = self.evaluate("{" + source + " | s=2}")
                self.assertAlmostEqual(float(derivative["derivative_value"]), float(expected["value"]), places=13)
                outside = copied.replace("s = 2", "s = -2").replace("C = NAN", "C = 0")
                self.assertEqual(self.evaluate(outside)["value"], "NAN")

    def test_trigonometric_powers_preserve_exact_affine_rates(self):
        for rate_text, rate in (("@pi/2", math.pi/2), ("sqrt(2)/3", math.sqrt(2)/3)):
            for function in ("sin", "cos"):
                with self.subTest(function=function, rate=rate_text):
                    source = "@L{" + function + "^2((" + rate_text + ")*t)}"
                    fields = self.evaluate("{" + source + " | s=2}")
                    numerator = 2*rate**2 + (4 if function == "cos" else 0)
                    expected = numerator/(2*(4+4*rate**2))
                    self.assertNotIn("laplace(", fields["function"])
                    self.assertAlmostEqual(float(fields["value"]), expected, places=13)
                    copied = self.evaluate("{" + fields["unbound"] + " | s=2}")
                    self.assertAlmostEqual(float(copied["value"]), expected, places=13)
        fields = self.evaluate("@L{sin^2((@pi*t)/2)}")
        self.assertIn(r"\pi", fields["tex"])
        self.assertNotIn("3.14159", fields["function"])
        fields = self.evaluate("@L{sin((@pi*t)/2)^n}")
        self.assertIn("sum(", fields["function"])
        self.assertIn("@pi", fields["function"])
        self.assertIn("laplace(", self.evaluate("@L{sin((@pi*t^2)/2)^2}")["function"])

    def test_public_mathematical_function_inventory_is_audited(self):
        import re
        header = (ROOT / "include/expression.h").read_text()
        section = header[header.index("expr_t *expr_sin("):header.index("expr_t *expr_E1(")]
        names = set(re.findall(r"expr_t \*expr_(\w+)\(", section)) | {"E1"}
        guide = (ROOT / "docs/expression.md").read_text()
        inventory = guide.split("### Laplace function coverage", 1)[1].split("## Numeric representation", 1)[0]
        missing = sorted(name for name in names if not re.search(r"`"+re.escape(name)+r"`", inventory))
        self.assertEqual(missing, [])

    def test_transform_tables_cover_supported_function_families(self):
        import re
        guide = (ROOT / "docs/expression.md").read_text()
        inventory = guide.split("#### Supported families", 1)[1].split("Linearity,", 1)[0]
        names = set()
        for line in inventory.splitlines():
            if line.startswith("| `"):
                names.update(re.findall(r"`(\w+)`", line.split("|")[1]))
        note = (ROOT / "docs/design-notes/integral-transforms.md").read_text()
        tables = note.split("## Implemented forward Laplace transforms", 1)[1].split("## Expression Rendering", 1)[0]
        self.assertEqual(sorted(name for name in names if "`" + name + "`" not in tables), [])
        for line in tables.splitlines():
            if line.startswith("|"):
                self.assertEqual(line.count("|"), 4, line)

    def evaluate(self, source, action="evaluate"):
        fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, "s", action)
        self.assertEqual(code, 0, raw)
        return fields

    def test_gaussian_entire_transform_and_aliases(self):
        for target in (0, 1, -2):
            expected = math.sqrt(math.pi)/2*math.exp(target*target/4)*math.erfc(target/2)
            for source in ("exp(-t^2)", "e^(-t^2)"):
                fields = self.evaluate("{@L("+source+") | s="+str(target)+"}")
                self.assertAlmostEqual(float(fields["value"]), expected, places=13)
                self.assertNotIn("laplace(", fields["function"])
                self.assertNotIn("Re(s)", fields["expression"])
        fields = self.evaluate("{@L(exp(-(2*t+1)^2)) | s=1}")
        z = 1.25
        self.assertAlmostEqual(float(fields["value"]), math.sqrt(math.pi)/4*math.exp(z*z-1)*math.erfc(z), places=13)

    def test_normal_family_and_aliases(self):
        cases = (
            ("normal_pdf(t)", 0, 0.5),
            ("pdf(t)", -1, math.exp(0.5)*math.erfc(-1/math.sqrt(2))/2),
            ("normal_pdf(2*t+1)", 0, math.erfc(1/math.sqrt(2))/4),
            ("normal_cdf(t)", 1, (1+math.exp(0.5)*math.erfc(1/math.sqrt(2)))/2),
            ("cdf(-t)", 1, (1-math.exp(0.5)*math.erfc(1/math.sqrt(2)))/2),
            ("normal_logpdf(t)", 2, -1/8-math.log(2*math.pi)/4),
            ("logpdf(2*t+1)", 2, -1-(1+math.log(2*math.pi))/4),
        )
        for operand, target, expected in cases:
            with self.subTest(operand=operand, target=target):
                fields = self.evaluate("{@L("+operand+") | s="+str(target)+"}")
                self.assertNotIn("laplace(", fields["function"])
                self.assertAlmostEqual(float(fields["value"]), expected, places=13)

    def test_symbolic_gaussian_guards_and_derivative(self):
        fields = self.evaluate("@L(exp(-a*t^2))")
        self.assertNotIn("laplace(", fields["function"])
        self.assertIn("realpart(a) > 0", fields["function"])
        self.assertNotIn("realpart(s)", fields["function"])
        fields = self.evaluate("{@L(exp(-a*t^2)) | s=1; a=-1}")
        self.assertEqual(fields["value"], "NAN")
        fields = self.evaluate("{@L(exp(-t^2)) | s=0}", "derivative")
        self.assertAlmostEqual(float(fields["derivative_value"]), -0.5, places=13)

    def test_proven_divergence_is_not_confused_with_missing_rules(self):
        for operand in ("sec(t)", "cosec(t)", "cot(t)", "cosech(t)", "coth(t)",
                        "gamma(t)", "digamma(t)", "trigamma(t)", "zeta(t)", "exp(t^2)", "e^(t^2)"):
            with self.subTest(operand=operand):
                fields = self.evaluate("@L("+operand+")")
                self.assertIn("No ordinary Laplace transform", fields["value_note"])
        fields = self.evaluate("@L(lgamma(t))")
        self.assertIn("does not imply", fields["value_note"])

    def test_new_formulas_reuse_native_derivatives(self):
        cases = (
            ("E1(t)", 0.5-math.log(2)),
            ("bessel_j(1,t)", -1/(2*math.sqrt(2))),
            ("floor(t)", -1/(math.e-1)-math.e/(math.e-1)**2),
        )
        for operand, expected in cases:
            with self.subTest(operand=operand):
                fields = self.evaluate("{@L("+operand+") | s=1}", "derivative")
                self.assertAlmostEqual(float(fields["derivative_value"]), expected, places=12)


class ZZLaplaceCoverageReadmeExamples(unittest.TestCase):
    def test_readme_coverage_examples(self):
        # README examples: docs/expression.md, Laplace coverage, kept after ordinary regressions.
        cases = (
            ("{@L(exp(-t^2)) | s=0}", 0.886226925452758),
            ("{@L(normal_pdf(t)) | s=0}", 0.5),
            ("{@L(floor(t)) | s=1}", 0.581976706869326),
            ("{@L(sech(t)) | s=1}", 0.693147180559945),
            ("{@L(E1(t)) | s=1}", 0.693147180559945),
            ("{@L(bessel_j(1,t)) | s=1}", 0.292893218813452),
            ("{@L(atanh(t)) | s=1}", 0.646761122779130 + 0.577863674895461j),
        )
        for source, expected in cases:
            fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, "s", "evaluate")
            self.assertEqual(code, 0, raw)
            actual = complex(fields["value"].replace(" ", "").replace("i", "j"))
            self.assertAlmostEqual(actual, expected, places=14)


if __name__ == "__main__":
    unittest.main()
