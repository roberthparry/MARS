"""Ordinary function-derivative notation and the unilateral Laplace theorem."""

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import mars_lab


class LaplaceFunctionDerivativeTests(unittest.TestCase):
    def fields(self, source, action="evaluate", wrt="s"):
        fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, wrt, action)
        self.assertEqual(code, 0, raw)
        return fields

    def test_primes_and_fixed_order_are_equivalent(self):
        self.assertEqual(self.fields("@L(Dt(f(t)))")["tex"], self.fields("@L{f'(t)}")["tex"])
        for order in (1, 2, 3):
            prime = self.fields("@L{f" + "'"*order + "(t)}")
            indexed = self.fields("@L{f^("+str(order)+")(t)}")
            self.assertEqual(prime["tex"], indexed["tex"])
            self.assertIn("f(0)", prime["function"])
            self.assertNotIn("realpart(s)", prime["function"])
            self.assertIn(" = ", prime["transform_identity_TeX"])

    def test_linearity_combines_derivatives_and_unknown_functions(self):
        fields = self.fields("@L{y''(t)+4*y'(t)+5*y(t)-50*t}")
        self.assertIn("ℒ(y(t), t, s)", fields["unbound"])
        self.assertNotIn("Laplace(y''", fields["function"])
        self.assertIn("y'(0)", fields["function"])
        self.assertIn("y(0)", fields["function"])
        self.assertIn(" = ", fields["transform_identity_TeX"])
        # y(t)=t^2 gives L{2-42t+5t^2}=2/s-42/s^2+10/s^3.
        specialised = fields["unbound"].replace("ℒ(y(t), t, s)", "(2/s^3)")
        specialised = specialised.replace("y'(0)", "0").replace("y(0)", "0")
        result = self.fields("{" + specialised + " | s=2}")
        self.assertAlmostEqual(float(result["value"]), -8.25, places=13)
        for source in ("@L{2*u(t)+3*v(t)}", "@L{-u(t)}", "@L{u(t)/2}"):
            result = self.fields(source)
            self.assertIn("Laplace(u(t), t, s)", result["function"])
            self.assertNotIn("realpart(s)", result["function"])

    def test_bare_prime_shorthand_matches_explicit_functions(self):
        pairs = (
            ("@L{y''+4y'+5y-50t}", "@L{y''(t)+4*y'(t)+5*y(t)-50*t}"),
            ("@L{5y+4y'+y''-50t}", "@L{5*y(t)+4*y'(t)+y''(t)-50*t}"),
            ("@L{y''+a*y'+b*y}", "@L{y''(t)+a*y'(t)+b*y(t)}"),
            ("@L(y''+4y'+5y-50x,x,p)", "@L(y''(x)+4*y'(x)+5*y(x)-50*x,x,p)"),
            ("@L{y+z+y'+z''}", "@L{y(t)+z(t)+y'(t)+z''(t)}"),
            ("@L{y+y'(t)}", "@L{y(t)+y'(t)}"),
            ("@L{y'''}", "@L{y'''(t)}"),
            ("Laplace(y'+a*y,t,s)", "Laplace(y'(t)+a*y(t),t,s)"),
        )
        for shorthand, explicit in pairs:
            with self.subTest(source=shorthand):
                got, expected = self.fields(shorthand), self.fields(explicit)
                for key in ("unbound", "tex", "function", "transform_identity_TeX"):
                    self.assertEqual(got[key], expected[key])

    def test_prime_shorthand_is_scoped_to_its_transform(self):
        for shorthand, explicit in (
            ("y+@L{y'+y}", "y+@L{y'(t)+y(t)}"),
            ("y(0)+@L{y'}", "y(0)+@L{y'(t)}"),
            ("@L{y'}+y(0)", "@L{y'(t)}+y(0)"),
            ("@L{y'+y}+@L{y*t}", "@L{y'(t)+y(t)}+@L{y*t}"),
            ("@L{y'+y+[_laplace_prime_0_0]*t}",
             "@L{y'(t)+y(t)+[_laplace_prime_0_0]*t}"),
            ("@L(y'+@L(z',x,p),t,s)", "@L(y'(t)+@L(z'(x),x,p),t,s)"),
        ):
            with self.subTest(source=shorthand):
                self.assertEqual(self.fields(shorthand)["unbound"], self.fields(explicit)["unbound"])
        self.assertNotIn("a(t)", self.fields("@L{a*t}")["unbound"])

    def test_symbolic_order_and_round_trip(self):
        fields = self.fields("@L{f^(n)(t)}")
        self.assertIn(r"\sum_{k=0}^{n - 1}", fields["tex"])
        self.assertIn(r"n\in\mathbb{Z}_{\ge0}", fields["tex"])
        self.assertIn("f^(k)(0)", fields["function"])
        self.assertIn("floor(n) == n", fields["function"])
        copied = self.fields(fields["unbound"])
        self.assertEqual(copied["tex"], fields["tex"])
        for source in ("@L{f'(t)}", "@L{f''(t)}"):
            fields = self.fields(source)
            self.assertEqual(self.fields(fields["unbound"])["tex"], fields["tex"])
        reversed_terms = self.fields("f(0)-s*Laplace(f(t),t,s)")
        self.assertIn("f(0)", reversed_terms["unbound"])
        self.assertEqual(self.fields(reversed_terms["unbound"])["tex"], reversed_terms["tex"])

    def test_initial_values_follow_ascending_transform_powers(self):
        for order in (3, 4, 8):
            for source, variable in (("@L{f^(%d)(t)}" % order, "s"),
                                     ("@L(f^(%d)(x),x,p)" % order, "p")):
                with self.subTest(source=source):
                    fields = self.fields(source)
                    # Read each correction in ascending target power, starting with the constant term.
                    for key in (("unbound", "function", "tex") if order <= 4 else ("unbound", "tex")):
                        text = fields[key]
                        initial = "f" + "'" * (order - 1) if order <= 4 else "f^{(7)}" if key == "tex" else "f^(7)"
                        self.assertIn(initial, text)
                        positions = [text.index(initial)]
                        for power in range(2, order):
                            suffix = "^{%d}" % power if key == "tex" else "^%d" % power
                            positions.append(text.rindex(variable + suffix))
                        self.assertEqual(positions, sorted(positions))
                    copied = self.fields(fields["unbound"])
                    initial_TeX = "f" + "'" * (order - 1) if order <= 4 else "f^{(7)}"
                    self.assertLess(copied["tex"].index(initial_TeX), copied["tex"].rindex(variable + "^{2}"))
        self.assertEqual(self.fields("@L{f'''(t)}")["unbound"],
                         "s^3·ℒ(f(t), t, s) - f''(0) - s·f'(0) - f(0)·s^2")

    def test_order_bindings_zero_and_large_order(self):
        zero = self.fields("@L{f^(0)(t)}")
        self.assertEqual(zero["tex"], self.fields("@L{f(t)}")["tex"])
        fixed = self.fields("{@L{f^(n)(t)} | n=3}")
        self.assertEqual(fixed["tex"], self.fields("@L{f^(n)(t)}")["tex"])
        large = self.fields("@L{f^(30)(t)}")
        self.assertIn(r"\sum_", large["tex"])

    def test_explicit_variables_and_hygienic_sum_index(self):
        fields = self.fields("@L(f^(k)(x),x,p)")
        self.assertIn("Laplace(f(x), x, p)", fields["function"])
        self.assertIn("sum(j,", fields["function"])
        self.assertIn("f^(j)(0)", fields["function"])

    def test_derivative_notation_outside_transform(self):
        fields = self.fields("f'(t)", "derivative", "t")
        self.assertIn("f''", fields["derivative"])
        fields = self.fields("f^(n)(t)", "derivative", "t")
        self.assertIn("n + 1", fields["derivative"])

    def test_ordinary_powers_and_unsupported_cases(self):
        self.assertEqual(self.fields("f^(n)")["tex"], self.fields("f^n")["tex"])
        self.assertEqual(self.fields("sin(t)^2")["tex"], self.fields("sin^2(t)")["tex"])
        self.assertEqual(self.fields("a(t+1)")["tex"], self.fields("a*(t+1)")["tex"])
        for source in ("@L{f^(-1)(t)}", "@L{f^(1/2)(t)}", "@L{f^(t)(t)}", "@L{f'(2*t)}"):
            with self.subTest(source=source):
                self.assertNotIn(" = ", self.fields(source)["transform_identity_TeX"])


class ZZLaplaceDerivativeReadmeExamples(unittest.TestCase):
    def test_readme_derivative_examples(self):
        # README examples: docs/expression.md and the integral-transform design note.
        cases = (
            ("@L{f'(t)}", "s·ℒ(f(t), t, s) - f(0)"),
            ("@L{f'}", "s·ℒ(f(t), t, s) - f(0)"),
            ("@L{f''(t)}", "s^2·ℒ(f(t), t, s) - f'(0) - s·f(0)"),
            ("@L{f^(n)(t)}", "s^n·ℒ(f(t), t, s) - Σ_(k=0)^(n - 1) f^(k)(0)·s^(n - 1 - k) where (nonnegative_integer(n))"),
        )
        for source, expected in cases:
            fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, "s", "evaluate")
            self.assertEqual(code, 0, raw)
            self.assertEqual(fields["unbound"], expected)


if __name__ == "__main__":
    unittest.main()
