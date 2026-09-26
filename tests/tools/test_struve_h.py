"""Ordinary Struve H: native values, calculus, Unicode notation and Laplace inverses."""
import cmath
from decimal import Decimal, localcontext
import math
import unittest

from tests.tools.test_struve_l import fields, number


def reference(nu, z):
    """Independent defining series for real orders, including reciprocal-gamma zeros."""
    z = complex(z)
    total = 0j
    for k in range(150):
        parameter = k + nu + 1.5
        if parameter <= 0 and parameter == math.floor(parameter):
            continue
        term = (-1)**k * (z/2)**(2*k+nu+1)/(math.gamma(k+1.5)*math.gamma(parameter))
        total += term
        if abs(term) < 1e-18 * max(1, abs(total)) and k > 10:
            break
    return total


class StruveHTests(unittest.TestCase):
    def test_real_orders_and_complex_arguments(self):
        for nu in (0, 1, 2, -1, -2, -0.5, -1.5, -2.5, 0.3):
            for z, text in ((0.4, "2/5"), (2, "2"), (1+0.5j, "1+i/2"), (-1+0.5j, "-1+i/2")):
                with self.subTest(order=nu, argument=z):
                    actual = number(fields(f"struve_h({nu},{text})"))
                    expected = reference(nu, z)
                    self.assertLess(abs(actual-expected), 3e-12*(1+abs(expected)))

    def test_half_integer_identities(self):
        for z in ("1/5", "2", "1+i"):
            scale = f"sqrt(2/(@pi*({z})))"
            for nu, expected in (("-1/2", f"{scale}*sin({z})"),
                                 ("1/2", f"{scale}*(1-cos({z}))"),
                                 ("-3/2", f"{scale}*(cos({z})-sin({z})/({z}))")):
                self.assertLess(abs(number(fields(f"struve_h({nu},{z})-({expected})", precision=80))), 1e-65)

    def test_precision(self):
        with localcontext() as context:
            context.prec = 100
            pi = Decimal("3.141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117068")
            term = total = Decimal(2)/pi
            for k in range(1, 90):
                term /= -(2*k+1)**2
                total += term
            actual = Decimal(fields("H0(1)", precision=80)["value"])
            self.assertLess(abs(actual-total), Decimal("1e-76"))

    def test_aliases_renderings_and_hermite_distinction(self):
        for alias in ("struve_h", "struveh", "StruveH", "𝐇"):
            self.assertLess(abs(number(fields(alias+"(0,1)"))-reference(0,1)), 2e-13)
        for alias in ("H0", "H_0", "H₀", "𝐇₀"):
            self.assertLess(abs(number(fields(alias+"(1)"))-reference(0,1)), 2e-13)
        self.assertEqual(number(fields("ℋ₀(1)")), 1)
        for source in ("𝐇₀(x)", "𝐇₋₂(x)", "H_n(x)", "𝐇_{n+1}(x)", "struve_h(1/2,x)"):
            result = fields(source)
            self.assertIn("𝐇", result["expression"])
            self.assertIn(r"\mathbf{H}", result["tex"])
            self.assertIn("struveh(", result["function"])
            self.assertEqual(fields(result["expression"])["unbound"], result["unbound"])

    def test_derivatives_parity_origin_and_sum(self):
        for n in (0, 1, -1, -1.5, 0.25):
            derivative = number(fields("{Dx(struve_h("+str(n)+",x)) | x=1}"))
            expected = (reference(n,1.00001)-reference(n,0.99999))/0.00002
            self.assertLess(abs(derivative-expected), 2e-9)
        for n in (0, 1, 2, -1, -2):
            self.assertLess(abs(number(fields(f"struve_h({n},-1)"))-(-1)**(n+1)*reference(n,1)), 2e-12)
        self.assertAlmostEqual(number(fields("{Dx(H0(x)) | x=0}")).real, 2/math.pi, places=13)
        self.assertEqual(number(fields("H0(0)")), 0)
        self.assertLess(abs(number(fields("sum(n,0,3,struve_h(n,1))"))-
                            sum(reference(n,1) for n in range(4))), 2e-12)

    def test_primitives_differentiate_back(self):
        for order in (0, 1, -1, -1.5, -2.5, 0.25):
            result = fields(f"struve_h({order},2*x+1)", "integral")
            self.assertNotIn("integral(", result["integral_function"])
            primitive = result["integral"].split(" = ",1)[1].replace("x = NAN", "x = 1")
            differentiated = fields(primitive, "derivative")
            self.assertLess(abs(number(differentiated, "derivative_value")-reference(order,3)), 3e-10)

    def test_laplace_native_formula_and_copied_inverse(self):
        result = fields("@L{asinh(t)}")
        self.assertIn("𝐇₀(s)", result["expression"])
        self.assertIn("Y₀(s)", result["expression"])
        self.assertIn("struveh(0, s)", result["function"])
        self.assertNotIn("hypergeometric", result["function"])
        self.assertIn(r"\mathbf{H}_{0}", result["tex"])
        for spectrum in (result["expression"].split(" | ")[0].removeprefix("{ "),
                         "@pi*(struve_h(0,s)-bessely(0,s))/(2*s)",
                         "@pi*(2*s/@pi*hypergeometricpfq(1,2,1,3/2,3/2,-s^2/4)-bessely(0,s))/(2*s)"):
            inverse = fields("InverseLaplace("+spectrum+",s,t)")
            self.assertNotIn("InverseLaplace(", inverse["function"])
            self.assertIn("asinh(t)", inverse["function"])


class StruveHReadmeTests(unittest.TestCase):
    def test_readme_struve_h(self):
        # README examples: docs/expression.md, ordinary Struve H.
        self.assertAlmostEqual(number(fields("H0(1)")).real, 0.568656627048288, places=14)
        self.assertAlmostEqual(number(fields("{Dx(H0(x)) | x=0}")).real, 0.636619772367581, places=14)


if __name__ == "__main__":
    unittest.main()
