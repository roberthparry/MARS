"""Real translations of arbitrary functions retain the unilateral history term."""

import math
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import mars_lab


class LaplaceTimeTranslationTests(unittest.TestCase):
    def fields(self, source):
        fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, "s", "evaluate")
        self.assertEqual(code, 0, raw)
        return fields

    def test_delayed_function_retains_history(self):
        fields = self.fields("@L{u(t-1/4)}")
        self.assertIn("laplace(u(t), t, s)", fields["function"])
        self.assertIn("integral(-1/4, 0, u(t).exp(-s.t), t)", fields["function"])
        self.assertIn("exp(-1/4.s)", fields["function"])
        self.assertNotIn("realpart(s)", fields["function"])
        self.assertIn(" = ", fields["transform_identity_TeX"])
        self.assertEqual(fields["value"], "NAN")
        self.assertEqual(self.fields(fields["unbound"])["tex"], fields["tex"])

    def test_weighted_translation_and_factor_association(self):
        expected = self.fields("@L{16t^2u(t-1/4)}")
        self.assertIn("16.derivative(", expected["function"])
        self.assertIn(", s, 2)", expected["function"])
        self.assertIn(r"\frac{d^{2}}{d s^{2}}\left[", expected["tex"])
        self.assertIn("integral(-1/4, 0,", expected["function"])
        for source in ("@L{16*(t^2*u(t-1/4))}", "@L{u(t-1/4)*(16*t^2)}"):
            self.assertEqual(self.fields(source)["tex"], expected["tex"])
        copied = self.fields(expected["unbound"])
        self.assertIn("derivative(laplace(u(t), t, s), s, 2)", copied["function"])
        self.assertIn("integral(-1/4, 0,", copied["function"])

    def test_zero_shift_advances_and_function_names(self):
        self.assertEqual(self.fields("@L{u(t-0)}")["tex"], self.fields("@L{u(t)}")["tex"])
        fields = self.fields("@L{g(t+2)}")
        self.assertIn("exp(2.s)", fields["function"])
        self.assertIn("integral(2, 0, g(t).exp(-s.t), t)", fields["function"])
        fields = self.fields("@L{t*u(t)}")
        self.assertIn("-derivative(laplace(u(t), t, s), s, 1)", fields["function"])
        self.assertNotIn("integral(", fields["function"])

    def test_symbolic_shift_and_explicit_variables(self):
        fields = self.fields("@L(u(x-a),x,p)")
        self.assertIn("laplace(u(x), x, p)", fields["function"])
        self.assertIn("integral(-a, 0, u(x).exp(-p.x), x)", fields["function"])
        self.assertIn(r"\in\mathbb{R}", fields["tex"])
        self.assertNotIn("realpart(p) >", fields["function"])
        self.assertIn("integral(-a, 0,", self.fields(fields["unbound"])["function"])
        # The shift parameter must not be captured by a newly chosen integration dummy.
        fields = self.fields("@L(g(x-t),x,p)")
        self.assertIn("integral(-t, 0, g(x).exp(-p.x), x)", fields["function"])

    def test_derivative_order_limits_and_symbolic_bindings(self):
        fields = self.fields("@L{t^3*u(t-1)}")
        self.assertIn("-derivative(", fields["function"])
        self.assertIn(", s, 3)", fields["function"])
        self.assertIn(r"\frac{d^{3}}{d s^{3}}", fields["tex"])
        fields = self.fields("@L{t^64*u(t-1)}")
        self.assertIn(r"\frac{d^{64}}{d s^{64}}", fields["tex"])
        fields = self.fields("@L{t^65*u(t-1)}")
        self.assertNotIn("integral(", fields["function"])
        fields = self.fields("{@L{t^2*u(t-a)} | s=3; a=1/4}")
        self.assertIn("derivative(", fields["function"])
        self.assertIn(", s, 2)", fields["function"])
        self.assertIn("integral(-a, 0,", fields["function"])
        self.assertIn(r"\int_{-a}^{0}", fields["tex"])
        self.assertNotIn(r"\frac{1}{4}", fields["tex"])

    def test_history_is_required_by_noncausal_numeric_examples(self):
        result = self.fields("@L{16t^2u(t-1/4)}")["unbound"]
        for function, transform, expected in (("1", "1/s", 32/3**3),
                                              ("t", "1/s^2", 96/3**4-8/3**3),
                                              ("exp(t)", "1/(s-1)", 32*math.exp(-0.25)/2**3)):
            with self.subTest(function=function):
                # Specialise the arbitrary function consistently in the base transform and history.
                specialised = result.replace("ℒ(u(t), t, s)", transform).replace("u(t)", function)
                source = "{" + specialised + " | s=3}"
                fields = self.fields(source)
                self.assertAlmostEqual(float(fields["value"]), expected, places=11)

    def test_unsupported_compositions_stay_formal(self):
        for source in ("@L{u(2*t-1)}", "@L{u(t^2-1)}", "@L{u(t-1,t)}", "@L{u(t+i)}"):
            with self.subTest(source=source):
                fields = self.fields(source)
                self.assertNotIn("integral(", fields["function"])
                self.assertNotIn(" = ", fields["transform_identity_TeX"])
        _, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, "@L(t*u(t-s),t,s)", 40)
        self.assertNotEqual(code, 0)
        self.assertIn("distinct target", raw)


class ZZLaplaceTimeTranslationReadmeExamples(unittest.TestCase):
    def test_readme_time_translations(self):
        # README examples: docs/expression.md and the integral-transform design note.
        for source, expected in (
            ("@L{u(t-1/4)}", "exp(-¼s)·(ℒ(u(t), t, s) + ∫^0_-¼ u(t)·exp(-st)·dt)"),
            ("@L{16t^2u(t-1/4)}", "16·Dss(exp(-¼s)·(ℒ(u(t), t, s) + ∫^0_-¼ u(t)·exp(-st)·dt))"),
        ):
            fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, "s", "evaluate")
            self.assertEqual(code, 0, raw)
            self.assertEqual(fields["unbound"], expected)


if __name__ == "__main__":
    unittest.main()
