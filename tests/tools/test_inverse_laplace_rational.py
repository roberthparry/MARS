"""Exact rational normalisation and real-factor inverse Laplace decomposition."""

import math
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import mars_lab


class InverseLaplaceRationalTests(unittest.TestCase):
    def fields(self, source, action="evaluate", wrt="t"):
        fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, wrt, action)
        self.assertEqual(code, 0, raw)
        return fields

    def test_equation_solver_output_and_equivalent_fractions(self):
        expected = "10t + exp(-2t)·(13·cos(t) + 11·sin(t)) - 8"
        operands = (
            "-5*(-s-10/s^2-3)/(s*(s+4)+5)",
            "(5*s+15+50/s^2)/(s^2+4*s+5)",
            "(5*s^3+15*s^2+50)/(s^2*(s^2+4*s+5))",
            "(5*s^3+15*s^2+50)/(s^4+4*s^3+5*s^2)",
            "10/s^2-8/s+(13*(s+2)+11)/((s+2)^2+1)",
        )
        for operand in operands:
            with self.subTest(operand=operand):
                fields = self.fields("@Linv{" + operand + "}")
                self.assertEqual(fields["unbound"], expected)
                self.assertNotIn("InverseLaplace(", fields["function"])
                self.assertIn(" = ", fields["transform_identity_TeX"])
                self.assertEqual(self.fields(fields["unbound"])["tex"], fields["tex"])
        solved, raw, code = mars_lab.run_equation_lab_fields(
            mars_lab.DEFAULT_EQUATION_BIN,
            "{(s^2+4*s+5)*Y=5*s+15+50/s^2 | Y=?; s=?}", 40)
        self.assertEqual(code, 0, raw)
        operand = solved["solutions"].split(" = ", 1)[1]
        self.assertEqual(operand, "5/(s·(s + 4) + 5)·(s + 10/s² + 3)")
        self.assertNotIn(r"\times 1", solved["solutions_TeX"])
        self.assertNotIn("&= -", solved["solutions_TeX"])
        simplified = self.fields(operand)
        self.assertEqual(simplified["unbound"], operand)
        self.assertIn(simplified["tex"], solved["solutions_TeX"])
        self.assertEqual(self.fields("@Linv{" + operand + "}")["unbound"], expected)

    def test_mixed_and_repeated_factors(self):
        cases = (
            ("1/(s*(s^2+1))", lambda t: 1-math.cos(t)),
            ("1/((s+1)*(s^2+1))", lambda t: (math.exp(-t)-math.cos(t)+math.sin(t))/2),
            ("1/((s^2+1)*(s^2+4))", lambda t: (math.sin(t)-math.sin(2*t)/2)/3),
            ("1/(s^2+1)^2", lambda t: (math.sin(t)-t*math.cos(t))/2),
            ("1/(s^2+1)^3", lambda t: ((3-t*t)*math.sin(t)-3*t*math.cos(t))/8),
            ("1/(s^2+4)^2", lambda t: (math.sin(2*t)-2*t*math.cos(2*t))/16),
            ("1/((s+2)^2+4)^2", lambda t: math.exp(-2*t)*(math.sin(2*t)-2*t*math.cos(2*t))/16),
            ("s/(s^2+1)^2", lambda t: t*math.sin(t)/2),
            ("1/(s^2-1)^2", lambda t: (t*math.cosh(t)-math.sinh(t))/2),
            ("1/(s^2+2*s+1)^2", lambda t: t**3*math.exp(-t)/6),
            ("1/((s+1)*(s^2+2*s+1))", lambda t: t*t*math.exp(-t)/2),
            ("1/(s*(s^2+s))", lambda t: t-1+math.exp(-t)),
            ("1/((2*s+2)*(3*s^2+3))", lambda t: (math.exp(-t)-math.cos(t)+math.sin(t))/12),
            ("(1+1/s)^2/(s+1)^3", lambda t: t-1+math.exp(-t)),
            ("1/(s/(s^2+1))/((s^2+1)^2)", lambda t: 1-math.cos(t)),
        )
        for operand, expected in cases:
            with self.subTest(operand=operand):
                symbolic = self.fields("@Linv{" + operand + "}")
                self.assertNotIn("InverseLaplace(", symbolic["function"])
                for time in (0, 0.5, 1.25):
                    actual = self.fields("{@Linv{" + operand + "} | t=" + str(time) + "}")
                    self.assertAlmostEqual(float(actual["value"]), expected(time), places=12)

    def test_higher_repeated_quadratics_round_trip_and_series(self):
        for power in (4, 8):
            inverse = self.fields("@Linv{1/(s^2+1)^" + str(power) + "}")
            self.assertNotIn("InverseLaplace(", inverse["function"])
            # Independent convergent series from s^(-2m)*(1+s^(-2))^(-m).
            for time in (0.5, 1.25):
                expected = sum((-1)**k*math.comb(power+k-1, k)*time**(2*power+2*k-1)
                               / math.factorial(2*power+2*k-1) for k in range(30))
                actual = self.fields("{@Linv{1/(s^2+1)^"+str(power)+"} | t="+str(time)+"}")
                self.assertAlmostEqual(float(actual["value"])/expected, 1, places=11)
            if power == 4:
                forward = self.fields("{@L{" + inverse["unbound"] + "} | s=3}")
                self.assertAlmostEqual(float(forward["value"])*10**power, 1, places=11)

    def test_parameters_and_explicit_coordinates(self):
        source = "@Linv((a*p+b)/(p*(p^2+1)),p,x)"
        symbolic = self.fields(source)
        self.assertNotIn("InverseLaplace(", symbolic["function"])
        for a, b in ((2, 3), (-3, 1)):
            bound = self.fields("{" + source + " | x=0.75; a=" + str(a) + ",b=" + str(b) + "}")
            self.assertAlmostEqual(float(bound["value"]), a*math.sin(0.75)+b*(1-math.cos(0.75)), places=12)

    def test_initial_conditions_and_differentiation(self):
        source = "@Linv{-5*(-s-10/s^2-3)/(s*(s+4)+5)}"
        value = self.fields("{" + source + " | t=0}")
        derivative = self.fields("{" + source + " | t=0}", "derivative")
        self.assertAlmostEqual(float(value["value"]), 5)
        self.assertAlmostEqual(float(derivative["derivative_value"]), -5)

    def test_unproved_and_out_of_scope_cases_remain_symbolic(self):
        for source in ("@Linv{1/((s-a)*(s-b)*(s^2+1))}", "@Linv{1/(s^2+a)^2}",
                       "@Linv{(s+1)/s}", "@Linv{1/(s^2+1)^9}"):
            with self.subTest(source=source):
                self.assertIn("InverseLaplace(", self.fields(source)["function"])


class ZZInverseLaplaceRationalReadmeExamples(unittest.TestCase):
    def test_readme_rational_inverse(self):
        # README example: docs/expression.md, copied directly from Equation mode's solution.
        source = "@Linv{-5*(-s-10/s^2-3)/(s*(s+4)+5)}"
        fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, "t", "evaluate")
        self.assertEqual(code, 0, raw)
        self.assertEqual(fields["unbound"], "10t + exp(-2t)·(13·cos(t) + 11·sin(t)) - 8")


if __name__ == "__main__":
    unittest.main()
