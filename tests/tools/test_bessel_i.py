"""Native modified Bessel I, independent reference values, calculus and aliases."""
import cmath
from decimal import Decimal, localcontext
import math
import unittest
from tests.tools.test_struve_l import fields, number


def reference(n, z):
    """Integer-order Fourier coefficient integral, independent of the native power series."""
    count = 4096
    h = math.pi/count
    def integrand(theta):
        return cmath.exp(z*math.cos(theta))*math.cos(n*theta)
    total = integrand(0)+integrand(math.pi)
    for k in range(1, count):
        total += (4 if k % 2 else 2)*integrand(k*h)
    return total*h/(3*math.pi)


class BesselITests(unittest.TestCase):
    def test_integer_orders_and_complex_arguments(self):
        for n in (0, 1, 2, -1, -3):
            for text, z in (("0", 0), ("1", 1), ("-2", -2), ("1+i", 1+1j)):
                with self.subTest(order=n, argument=z):
                    self.assertLess(abs(number(fields(f"bessel_i({n},{text})"))-reference(n,z)), 2e-12)

    def test_half_integer_formulas(self):
        for z in ("1/5", "2", "1+i"):
            common = f"sqrt(2/(@pi*({z})))"
            for n, expression in (("-1/2", f"{common}*cosh({z})"),
                                  ("1/2", f"{common}*sinh({z})"),
                                  ("3/2", f"{common}*(cosh({z})-sinh({z})/({z}))")):
                self.assertLess(abs(number(fields(f"bessel_i({n},{z})-({expression})", precision=80))), 1e-65)

    def test_precision(self):
        with localcontext() as context:
            context.prec = 100
            term = total = Decimal(1)
            for n in range(1, 90):
                term /= 4*n*n
                total += term
            actual = Decimal(fields("I0(1)", precision=80)["value"])
            self.assertLess(abs(actual-total), Decimal("1e-76"))

    def test_aliases_and_renderings(self):
        for name in ("bessel_i", "besseli", "BesselI"):
            self.assertLess(abs(number(fields(name+"(0,1)"))-reference(0,1)), 2e-13)
        for name in ("I0", "I_0", "I₀"):
            self.assertLess(abs(number(fields(name+"(1)"))-reference(0,1)), 2e-13)
        for source in ("I_n(x)", "I_{n}(x)", "bessel_i(n,x)"):
            result = fields(source)
            self.assertIn("I_{n}", result["tex"])
            self.assertIn("besseli(n, x)", result["function"])
            self.assertEqual(fields(result["expression"])["unbound"], result["unbound"])

    def test_cylindrical_unicode_expression_round_trips(self):
        families = (("besselj", "J"), ("bessely", "Y"), ("besselk", "K"),
                    ("besseli", "I"), ("struvel", "𝐋"), ("struveh", "𝐇"))
        for name, symbol in families:
            for order, index in (("0", "₀"), ("12", "₁₂"), ("-2", "₋₂"),
                                 ("1/2", "_{"), ("n+1", "_{")):
                with self.subTest(name=name, order=order):
                    result = fields(f"{name}({order},x)")
                    # These families simplify negative integral orders before rendering.
                    if name in ("besseli", "besselk", "bessely") and order == "-2":
                        index = "₂"
                    self.assertIn(symbol+index, result["expression"])
                    copied = fields(result["expression"])
                    self.assertEqual(copied["unbound"], result["unbound"])
                    self.assertEqual(copied["tex"], result["tex"])
                    self.assertIn(name+"(", result["function"])
        bound = fields("{bessel_i(n,x) | x=2; n=3}")
        self.assertIn("I_{n}(x)", bound["expression"])

    def test_derivatives_and_finite_sum(self):
        for n in (0, 1, 2, -1):
            value = number(fields("{Dx(bessel_i("+str(n)+",2*x)) | x=1/2}"))
            self.assertLess(abs(value-reference(n-1,1)-reference(n+1,1)), 2e-12)
        self.assertEqual(number(fields("{Dx(I0(x)) | x=0}")), 0)
        self.assertAlmostEqual(number(fields("{Dx(I_1(x)) | x=0}")).real, 0.5, places=14)
        self.assertLess(abs(number(fields("sum(n,0,3,bessel_i(n,1))"))-
                            sum(reference(n,1) for n in range(4))), 2e-12)

    def test_primitives_differentiate_back(self):
        for n in (0, 1, 2, -1, -2):
            result = fields(f"bessel_i({n},2*x+1)", "integral")
            self.assertNotIn("integral(", result["integral_function"])
            primitive = result["integral"].split(" = ", 1)[1].replace("x = NAN", "x = 1")
            differentiated = fields(primitive, "derivative")
            self.assertLess(abs(number(differentiated, "derivative_value")-reference(n,3)), 2e-10)


class BesselIReadmeTests(unittest.TestCase):
    def test_readme_bessel_i(self):
        # README example: docs/expression.md, native modified Bessel I.
        self.assertAlmostEqual(number(fields("I0(1)")).real, 1.266065877752008, places=14)
        self.assertEqual(number(fields("{Dx(I0(x)) | x=0}")), 0)


if __name__ == "__main__":
    unittest.main()
