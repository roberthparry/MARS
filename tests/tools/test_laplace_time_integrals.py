"""Laplace transforms of definite time integrals and chosen primitives."""

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import mars_lab


class LaplaceTimeIntegralTests(unittest.TestCase):
    def fields(self, source):
        fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, "s", "evaluate")
        self.assertEqual(code, 0, raw)
        return fields

    def assert_round_trip(self, fields):
        # Multiplication and quotient spellings may use different TeX spacing, but preserve the formula.
        copied = self.fields(fields["unbound"])
        if "chosen antiderivative" in fields["tex"]:
            for fragment in (r"F\left(0\right)", r"F'(x)=f\left(x\right)"):
                self.assertIn(fragment, copied["tex"])
            self.assertIn("integral(0, f(x), x)", copied["function"])
            self.assertIn("Laplace(f(t), t, s)", copied["function"])
            return
        normalise = lambda text: text.replace(r"\mkern-2mu ", "").replace(r"\,", "")
        self.assertEqual(normalise(fields["tex"]), normalise(copied["tex"]))

    def test_zero_based_integral_spellings(self):
        expected = self.fields("@L{integral(0,t,f(x),x)}")
        self.assertEqual(expected["unbound"], "ℒ(f(t), t, s)/s")
        for source in ("@L{@S_0^t f(x) dx}", "@L{@S^t_0 f(x) dx}", "@L{∫_0^t f(x) dx}"):
            fields = self.fields(source)
            self.assertEqual(fields["tex"], expected["tex"])
            self.assertNotIn("const f", fields["function"])
            self.assertNotIn("where ()", fields["unbound"])

    def test_upper_only_retains_initial_value(self):
        fields = self.fields("@L{@S^t f(x) dx}")
        self.assertIn("Laplace(f(t), t, s)", fields["function"])
        self.assertIn("integral(0, f(x), x)", fields["function"])
        self.assertNotIn("integral_meta", fields["unbound"])
        self.assertIn(r"F\left(0\right)", fields["tex"])
        self.assertIn(r"F'(x)=f\left(x\right)", fields["tex"])
        self.assertIn("chosen antiderivative", fields["tex"])
        self.assertNotIn(r"\int^{0}", fields["tex"])
        self.assertEqual(fields["tex"], self.fields("@L{integral(t,f(x),x)}")["tex"])
        self.assert_round_trip(fields)

    def test_primitive_notation_does_not_capture_existing_F(self):
        fields = self.fields("@L{@S^t f(x) dx + F}")
        self.assertNotIn("chosen antiderivative", fields["tex"])
        self.assertIn(r"\int^{0}", fields["tex"])
        fields = self.fields("@L{@S_0^t f(x) dx}")
        self.assertNotIn("chosen antiderivative", fields["tex"])
        fields = self.fields("@L{@S_2^t f(x) dx}")
        self.assertNotIn("chosen antiderivative", fields["tex"])
        fields = self.fields("@L{@S^t f(x) dx + @S^t g(x) dx}")
        self.assertNotIn("chosen antiderivative", fields["tex"])

    def test_nonzero_lower_bound_and_integration_constant(self):
        fields = self.fields("@L{@S_2^t f(x) dx}")
        self.assertIn("integral(2, 0, f(x), x)", fields["function"])
        self.assert_round_trip(fields)
        fields = self.fields("@L{@S^t f(x) dx + C}")
        self.assertIn("const C", fields["function"])
        self.assertIn("integral(0, f(x), x)", fields["function"])

    def test_explicit_variables_and_same_dummy(self):
        fields = self.fields("@L(@S_0^u f(x) dx,u,p)")
        self.assertIn("Laplace(f(u), u, p)/p", fields["function"])
        self.assertEqual(self.fields("@L{@S_0^t f(t) dt}")["tex"],
                         self.fields("@L{@S_0^t f(x) dx}")["tex"])

    def test_unsupported_kernels_and_ordinary_multiplication(self):
        fields = self.fields("@L{@S_0^t t*f(x) dx}")
        self.assertIn("Laplace(integral(", fields["function"])
        self.assertEqual(self.fields("a(x+1)")["tex"], self.fields("a*(x+1)")["tex"])

    def test_known_integrand(self):
        for integral in ("@S_0^t sin(x) dx", "∫_0^t sin(x) dx", "integral(0,t,sin(x),x)"):
            fields = self.fields("{@L{" + integral + "} | s=2}")
            self.assertAlmostEqual(float(fields["value"]), 0.1)


class ZZLaplaceTimeIntegralReadmeExamples(unittest.TestCase):
    def test_readme_time_integrals(self):
        # README examples: docs/expression.md, Laplace transforms of integrals.
        for source, expected in (
            ("@L{@S_0^t f(x) dx}", "ℒ(f(t), t, s)/s"),
            ("@L{@S^t f(x) dx}", "1/s·(ℒ(f(t), t, s) + ∫^0 f(x)·dx)"),
        ):
            fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, "s", "evaluate")
            self.assertEqual(code, 0, raw)
            self.assertEqual(fields["unbound"], expected)


if __name__ == "__main__":
    unittest.main()
