"""Canonical lowercase transform calls and their retained input aliases."""

import unittest

from test_fourier_tanh import fields


CASES = (
    ("Fourier(f(t), t, @omega)", "fourier(f(t), t, @omega)", "@omega"),
    ("InverseFourier(F(@omega), @omega, t)", "inversefourier(F(@omega), @omega, t)", "t"),
    ("Laplace(f(t), t, s)", "laplace(f(t), t, s)", "s"),
    ("InverseLaplace(F(s), s, t)", "inverselaplace(F(s), s, t)", "t"),
)


class TransformFunctionStyleTests(unittest.TestCase):
    def test_capitalised_aliases_and_lowercase_calls_agree(self):
        for alias, canonical, target in CASES:
            with self.subTest(source=alias):
                original = fields(alias, target)
                copied = fields(canonical, target)
                self.assertIn("return " + canonical + ".", original["function"])
                for key in ("function", "expression", "tex"):
                    self.assertEqual(original[key], copied[key])

    def test_lowercase_calls_still_resolve(self):
        for alias, canonical, target in (
            ("Fourier(exp(-t^2),t,k)", "fourier(exp(-t^2),t,k)", "k"),
            ("InverseFourier(exp(-k^2),k,t)", "inversefourier(exp(-k^2),k,t)", "t"),
            ("Laplace(t,t,s)", "laplace(t,t,s)", "s"),
            ("InverseLaplace(1/s^2,s,t)", "inverselaplace(1/s^2,s,t)", "t"),
        ):
            with self.subTest(source=canonical):
                original = fields(alias, target)
                copied = fields(canonical, target)
                self.assertEqual(original["tex"], copied["tex"])
                self.assertNotIn(canonical.split("(", 1)[0] + "(", copied["function"])


class TransformFunctionStyleReadmeTests(unittest.TestCase):
    def test_readme_transform_function_bodies(self):
        # README examples: docs/expression.md, lowercase transform Function bodies.
        for source, body, target in CASES:
            with self.subTest(source=source):
                self.assertIn("return " + body + ".", fields(source, target)["function"])


if __name__ == "__main__":
    unittest.main()
