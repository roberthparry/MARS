"""Native sign-function parsing, calculus, transforms and executable Function output."""
import json
import math
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import mars_lab


class SgnTests(unittest.TestCase):
    def fields(self, source, variable="x", action="evaluate"):
        fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, variable, action)
        self.assertEqual(code, 0, raw)
        return fields

    def test_aliases_and_real_domain(self):
        for alias in ("sgn", "sign", "signum"):
            for point, expected in (("-3", "-1"), ("0", "0"), ("3", "1"), ("1e-80", "1")):
                with self.subTest(alias=alias, point=point):
                    self.assertEqual(self.fields(alias + "(" + point + ")")["value"], expected)
            fields = self.fields(alias + "(x)")
            self.assertIn("sgn(x)", fields["unbound"])
            self.assertIn("sgn(x)", fields["function"])
            self.assertIn(r"\operatorname{sgn}", fields["tex"])
        self.assertEqual(self.fields("sgn(i)")["value"], "NAN")

    def test_fourier_and_independent_inverse(self):
        fields = self.fields("@F{sgn(t)}", "ω")
        self.assertNotIn("fourier(", fields["function"])
        self.assertNotIn("principal value", fields["expression"])
        self.assertIn("ω ≠ 0", fields["expression"])
        self.assertIn("ω ∈ ℝ", fields["expression"])
        for frequency in (-3, -1, 1, 77):
            actual = self.fields("{@F{sgn(t)} | ω=" + str(frequency) + "}", "ω")["value"]
            self.assertAlmostEqual(complex(actual.replace(" ", "").replace("i", "j")).imag,
                                   -2 / frequency, places=13)
        self.assertEqual(self.fields("{@F{sgn(t)} | ω=0}", "ω")["value"], "NAN")
        inverse = self.fields("InverseFourier(" + fields["unbound"] + ",ω,t)", "t")
        self.assertIn("sgn(t)", inverse["unbound"])
        for point in (-2, 0, 2):
            actual = self.fields("{InverseFourier(-2i/ω,ω,t) | t=" + str(point) + "}", "t")
            self.assertEqual(float(actual["value"]), -1 if point < 0 else 1 if point > 0 else 0)

    def test_affine_fourier_round_trips(self):
        for argument in ("t", "-t", "2t+3", "-2t+3"):
            spectrum = self.fields("@F{sgn(" + argument + ")}", "ω")
            for point in (-3, 0, 3):
                inverse = self.fields("{InverseFourier(" + spectrum["unbound"] + ",ω,t) | t=" + str(point) + "}", "t")
                original = self.fields("{sgn(" + argument + ") | t=" + str(point) + "}", "t")
                self.assertEqual(inverse["value"], original["value"])

    def test_laplace_real_affine(self):
        for argument, expected in (("t", .5), ("-t", -.5), ("t-1", -.5 + math.exp(-2)),
                                   ("1-t", .5 - math.exp(-2))):
            fields = self.fields("{@L{sgn(" + argument + ")} | s=2}", "s")
            self.assertAlmostEqual(float(fields["value"]), expected, places=13)

    def test_symbolic_derivative_and_finite_sum(self):
        self.assertIn("2·δ(x)", self.fields("Dx(sgn(x))")["unbound"])
        for point in (-2, 2):
            self.assertEqual(self.fields("{Dx(sgn(x)) | x=" + str(point) + "}")["value"], "0")
        self.assertEqual(self.fields("{Dx(sgn(x)) | x=0}")["value"], "NAN")
        for source, expected in (("sum(k,-2,2,sgn(k))", "0"), ("sum(k,-2,3,sgn(k))", "1"),
                                 ("sum(k,0,4,sgn(2k-3))", "1"),
                                 ("sum(k,-1000000,1000001,sgn(k))", "1")):
            self.assertEqual(self.fields(source)["value"], expected)

    def test_real_primitive_domain(self):
        primitive = self.fields("@S sgn(2x-1) dx")
        self.assertIn("|2x - 1|/2", primitive["unbound"])
        self.assertIn("x ∈ ℝ", primitive["unbound"])
        for point, expected in (("-2", "2"), ("0", "0"), ("2", "2"), ("i", "NAN")):
            fields = self.fields("{@S^x_0 sgn(t) dt | x=" + point + "}")
            self.assertEqual(fields["value"], expected)

    def test_aliases_in_bindings(self):
        for alias in ("sgn", "sign", "signum"):
            self.assertEqual(self.fields("{c+x | c=" + alias + "(-3); x=2}")["value"], "1")

    def test_unrelated_domain_is_not_discarded(self):
        for condition in ("Re(ω)>0", "ω-1 != 0", "a.ω != 0"):
            fields = self.fields("InverseFourier(-2i/ω where (" + condition + "),ω,t)", "t")
            self.assertIn("inversefourier(", fields["function"])

    def test_function_run(self):
        fields = self.fields("{sgn(x) | x=-2}")
        result = mars_lab.run_function_programme(fields["function"], 40)
        self.assertTrue(result["ok"], result)
        self.assertEqual(result["output"].strip(), "-1")
        fields = self.fields("{@F{sgn(t)} | ω=77}", "ω")
        result = mars_lab.run_function_programme(fields["operation_function"], 40)
        self.assertTrue(result["ok"], result)
        self.assertAlmostEqual(complex(result["output"].strip().replace("i", "j")).imag, -2/77, places=13)


class ZZSgnReadmeExamples(unittest.TestCase):
    fields = SgnTests.fields
    # README examples are selected explicitly after ordinary tests.
    def test_readme_sgn(self):
        for source, field, expected in (("sgn(-2)", "value", "-1"), ("sgn(0)", "value", "0"),
                                       ("@F{sgn(t)}", "unbound", "-2i/ω where (ω ∈ ℝ; ω ≠ 0)"),
                                       ("InverseFourier(-2i/ω,ω,t)", "unbound", "sgn(t) where (t ∈ ℝ)"),
                                       ("sum(k,-2,3,sgn(k))", "value", "1")):
            self.assertEqual(self.fields(source)[field], expected)


if __name__ == "__main__":
    focus = json.loads((ROOT / "tests/test_config.json").read_text()).get("sgn_focus")
    suite = unittest.defaultTestLoader.loadTestsFromNames(
        [name if "." in name else "SgnTests." + name for name in focus], sys.modules[__name__]
    ) if focus else unittest.defaultTestLoader.loadTestsFromModule(sys.modules[__name__])
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    raise SystemExit(not result.wasSuccessful())
