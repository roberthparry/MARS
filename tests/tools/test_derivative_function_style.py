"""Registered derivative calls in native Function-style output."""

import unittest

from test_fourier_tanh import fields


class DerivativeFunctionStyleTests(unittest.TestCase):
    def test_formal_derivative_uses_registered_callable(self):
        for source, call in (
            ("Dk(step(k)*ln(abs(k)))", "derivative(step(k).ln(abs(k)), k, 1)"),
            ("{Dkk(delta(k)) | k=?}", "derivative(delta(k), k, 2)"),
            ("Dω(delta(ω))", "derivative(delta(@omega), @omega, 1)"),
        ):
            with self.subTest(source=source):
                result = fields(source)
                self.assertIn(call, result["function"])
                self.assertNotRegex(result["function"], r"\bD(?:k|x|y|@omega)+\(")
                copied = fields(call.replace(".ln", "*ln"))
                self.assertEqual(copied["tex"], result["tex"])

    def test_explicit_derivative_survives_fourier_inverse(self):
        spectrum = ("2*i*@pi*((ln(2)-@eulermascheroni)*delta(k)"
                    "-besselj(0,k)*derivative(step(k)*ln(abs(k)),k,1))")
        self.assertIn("asin(x)", fields(f"InverseFourier({spectrum},k,x)", "x")["expression"])


class ZZDerivativeFunctionStyleReadmeExamples(unittest.TestCase):
    def test_readme_derivative_function_bodies(self):
        # README examples: docs/expression.md, Function-style derivative table.
        for source, body in (
            ("Dk(step(k)*ln(abs(k)))", "derivative(step(k).ln(abs(k)), k, 1)"),
            ("{Dkk(delta(k)) | k=?}", "derivative(delta(k), k, 2)"),
        ):
            self.assertIn("return " + body + ".", fields(source)["function"])


if __name__ == "__main__":
    unittest.main()
