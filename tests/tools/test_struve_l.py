"""Modified Struve L APIs through native expression, calculus and display paths."""
import cmath
from decimal import Decimal, localcontext
import math
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import mars_lab


def fields(source, operation="evaluate", precision=40):
    result, raw, code = mars_lab.run_mars_lab_fields(
        mars_lab.DEFAULT_BIN, source, precision, "x", operation)
    if code:
        raise AssertionError(raw)
    return result


def number(result, key="value"):
    return complex(result[key].replace(" ", "").replace("−", "-").replace("i", "j"))


def reference(nu, z):
    """Independent real-order series; reciprocal gamma vanishes at its non-positive integer poles."""
    total = 0j
    z = complex(z)
    for k in range(150):
        a = k + nu + 1.5
        if a <= 0 and a == math.floor(a):
            continue
        term = (z/2)**(2*k+nu+1)/(math.gamma(k+1.5)*math.gamma(a))
        total += term
        if abs(term) < 1e-17 * max(1, abs(total)) and k > 10:
            break
    return total


class StruveLTests(unittest.TestCase):
    def test_real_orders_and_complex_arguments(self):
        for nu in (0, 1, 2, -1, -2, -0.5, -1.5, -2.5, 0.3):
            for z, text in ((0.4, "2/5"), (2, "2"), (1+0.5j, "1+i/2")):
                with self.subTest(order=nu, z=z):
                    actual = number(fields(f"struve_l({nu},{text})"))
                    wanted = reference(nu, z)
                    self.assertLess(abs(actual-wanted), 3e-12*(1+abs(wanted)))

    def test_integer_parity_and_origin(self):
        for nu in (0, 1, 2, -1, -2):
            positive = number(fields(f"struve_l({nu},1)"))
            negative = number(fields(f"struve_l({nu},-1)"))
            self.assertLess(abs(negative-(-1)**(nu+1)*positive), 2e-12)
        for nu in (0, 1, 2, -0.5, -1.5, -2.5):
            self.assertEqual(number(fields(f"struve_l({nu},0)")), 0)
        self.assertAlmostEqual(number(fields("struve_l(-1,0)")).real, 2/math.pi, places=13)

    def test_half_integer_closed_forms(self):
        for z in ("1/5", "2", "1+i"):
            common = f"sqrt(2/(@pi*({z})))"
            for nu, expression in (("-1/2", f"{common}*sinh({z})"),
                                   ("1/2", f"{common}*(cosh({z})-1)"),
                                   ("-3/2", f"{common}*(cosh({z})-sinh({z})/({z}))")):
                result = fields(f"struve_l({nu},{z})-({expression})", precision=80)
                self.assertLess(abs(number(result)), 1e-65)

    def test_precision(self):
        with localcontext() as context:
            context.prec = 100
            pi = Decimal("3.141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117068")
            term = total = Decimal(2)/pi
            for k in range(1, 90):
                term /= (2*k+1)**2
                total += term
            actual = Decimal(fields("struve_l(0,1)", precision=80)["value"])
            self.assertLess(abs(actual-total), Decimal("1e-76"))

    def test_aliases_and_rendering(self):
        for alias in ("struve_l", "struvel", "StruveL", "𝐋"):
            self.assertLess(abs(number(fields(alias+"(0,1)"))-reference(0, 1)), 2e-13)
        for alias in ("L0", "L_0", "L₀", "𝐋₀"):
            self.assertLess(abs(number(fields(alias+"(1)"))-reference(0, 1)), 2e-13)
        for text in ("L_n(x)", "L_{n}(x)", "𝐋_n(x)", "struve_l(n,x)"):
            result = fields(text)
            self.assertIn(r"\mathbf{L}_{n}", result["tex"])
            self.assertIn("struvel(n, x)", result["function"])
            self.assertEqual(fields(result["expression"])["unbound"], result["unbound"])

    def test_derivative_and_sum(self):
        for nu in (0, 1, 2, -1, -1.5):
            result = fields("{Dx(struve_l("+str(nu)+",2*x)) | x=1/2}")
            h = 1e-5
            wanted = (reference(nu, 1+h)-reference(nu, 1-h))/h
            self.assertLess(abs(number(result)-wanted), 2e-9)
        self.assertAlmostEqual(number(fields("{Dx(L0(x)) | x=0}")).real, 2/math.pi, places=12)
        self.assertLess(abs(number(fields("sum(n,0,3,struve_l(n,1))")) -
                            sum(reference(nu, 1) for nu in range(4))), 2e-12)

    def test_affine_primitives_differentiate_back(self):
        for nu in (0, 1, 2, -0.5, -1.5):
            result = fields(f"struve_l({nu},2*x+1)", "integral")
            self.assertNotIn("integral(", result["integral_function"])
            primitive = result["integral"].split(" = ", 1)[1].replace("x = NAN", "x = 1")
            differentiated = fields(primitive, "derivative")
            self.assertLess(abs(number(differentiated, "derivative_value")-reference(nu, 3)), 2e-10)

    def test_laplace_notation_and_round_trip(self):
        result = fields("@L{acosh(t)}")
        self.assertIn(r"\mathbf{L}_{0}", result["tex"])
        self.assertIn("I_{0}", result["tex"])
        self.assertNotIn("F_{", result["tex"])
        self.assertIn("struvel(0,", result["function"])
        body = result["expression"].split(" | ")[0].removeprefix("{ ")
        inverse = fields("InverseLaplace("+body+",s,t)")
        self.assertNotIn("InverseLaplace(", inverse["function"])
        self.assertIn("acosh(t)", inverse["function"])

    def test_general_hypergeometric_notation(self):
        ordinary = fields("2*x*hypergeometricpfq(1,2,2,3,4,x)")
        self.assertIn(r"\cdot", ordinary["tex"])
        self.assertIn("F_{2}", ordinary["tex"])
        self.assertIn("I_{0}", fields("I0(x)")["tex"])
        self.assertIn("besseli(0, x)", fields("I0(x)")["function"])
        self.assertIn("F_{1}", fields("hypergeometricpfq(0,1,2,x^2/4)")["tex"])


class StruveLReadmeTests(unittest.TestCase):
    def test_readme_struve_l(self):
        # README example: docs/expression.md, modified Struve function.
        self.assertAlmostEqual(number(fields("struve_l(0,1)")).real, 0.710243185937891, places=14)
        self.assertAlmostEqual(number(fields("{Dx(L0(x)) | x=0}")).real, 0.636619772367581, places=14)


if __name__ == "__main__":
    unittest.main()
