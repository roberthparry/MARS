"""Principal Bessel Y: independent quadrature, precision, branches and README examples."""
import cmath
from decimal import Decimal, localcontext
import math
import unittest

from tests.tools.test_struve_l import fields, number


def simpson(function, left, right, count=8192):
    step = (right-left)/count
    total = function(left)+function(right)
    for k in range(1, count):
        total += (4 if k % 2 else 2)*function(left+k*step)
    return total*step/3


def reference(nu, z):
    """DLMF 10.9.7, independent Schlaefli integrals for Re(z)>0."""
    first = simpson(lambda t: cmath.sin(z*math.sin(t)-nu*t), 0, math.pi)
    second = simpson(lambda t: (cmath.exp(nu*t)+cmath.cos(math.pi*nu)*cmath.exp(-nu*t)) *
                     cmath.exp(-z*math.sinh(t)), 0, 8)
    return (first-second)/math.pi


def decimal_y0_two_plus_i():
    """100-digit Decimal reference; logarithm uses ln(5/4)/2 + i*atan(1/2)."""
    with localcontext() as context:
        context.prec = 105
        pi = Decimal("3.141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117067982148")
        gamma = Decimal("0.577215664901532860606512090082402431042159335939923598805767234884867726777664670936947063291746749514631")
        angle = Decimal(0)
        power = Decimal("0.5")
        for k in range(190):
            angle += power/(2*k+1)
            power /= -4
        logarithm = (Decimal("1.25").ln()/2+gamma, angle)
        term = (Decimal(1), Decimal(0))
        j = list(term)
        weighted = [Decimal(0), Decimal(0)]
        harmonic = Decimal(0)
        for k in range(1, 150):
            # -z*z/4 = -3/4-i.
            term = ((-Decimal("0.75")*term[0]+term[1])/(k*k),
                    (-term[0]-Decimal("0.75")*term[1])/(k*k))
            harmonic += Decimal(1)/k
            for part in (0, 1):
                j[part] += term[part]
                weighted[part] += harmonic*term[part]
        real = 2*(logarithm[0]*j[0]-logarithm[1]*j[1]-weighted[0])/pi
        imag = 2*(logarithm[0]*j[1]+logarithm[1]*j[0]-weighted[1])/pi
        return real, imag


class BesselYTests(unittest.TestCase):
    def test_independent_quadrature(self):
        for text, nu in (("0", 0), ("1", 1), ("-2", -2), ("1/3", 1/3),
                         ("1/4+3*i/8", 0.25+0.375j), ("-5/4+i/2", -1.25+0.5j)):
            for argument, z in (("1", 1), ("2+i", 2+1j), ("2-i", 2-1j)):
                with self.subTest(order=text, argument=argument):
                    expected = reference(nu, z)
                    actual = number(fields(f"bessely({text},{argument})"))
                    self.assertLess(abs(actual-expected), 2e-11*(1+abs(expected)))

    def test_y0_complex_precision(self):
        real, imag = decimal_y0_two_plus_i()
        for sign in (1, -1):
            residual = fields(f"Y0(2+({sign})*i)-(({real})+({sign})*({imag})*i)", precision=80)
            self.assertLess(abs(number(residual)), 1e-75)

    def test_half_integer_principal_values(self):
        for z in ("1/5", "2+i", "2-i", "-2", "-2+i/5", "-2-i/5"):
            common = f"sqrt(2/@pi)/sqrt({z})"
            for order, formula in (("1/2", f"-{common}*cos({z})"),
                                   ("-1/2", f"{common}*sin({z})"),
                                   ("3/2", f"-{common}*(cos({z})/({z})+sin({z}))"),
                                   ("-3/2", f"{common}*(cos({z})-sin({z})/({z}))")):
                with self.subTest(order=order, argument=z):
                    residual = number(fields(f"bessely({order},{z})-({formula})", precision=80))
                    self.assertLess(abs(residual), 1e-70)

    def test_near_integer_orders(self):
        # The exact denominator must fit the expression evaluator's rational precision budget.
        precision = 140
        for n in (0, 1, -2):
            for offset in ("1/10^100", "-1/10^100", "i/10^100"):
                displacement = number(fields(f"({n}+({offset}))-({n})", precision=precision))
                self.assertNotEqual(displacement, 0, (n, offset))
                source = f"bessely({n}+({offset}),2+i)-bessely({n},2+i)"
                self.assertLess(abs(number(fields(source, precision=precision))), 1e-72)

    def test_tiny_negative_half_order(self):
        source = "bessely(-1/2,1/10^100)/(sqrt(2/@pi)*sqrt(1/10^100))-1"
        self.assertLess(abs(number(fields(source, precision=80))), 1e-72)

    def test_integer_cut_and_parity(self):
        for n in (0, 1, 2, 3):
            upper = number(fields(f"bessely({n},-2)"))
            near_upper = number(fields(f"bessely({n},-2+i/10^30)"))
            lower = number(fields(f"bessely({n},-2-i/10^30)"))
            self.assertLess(abs(upper-near_upper), 1e-25)
            self.assertLess(abs(upper.conjugate()-lower), 1e-25)
            difference = f"bessely(-{n},2+i)-(-1)^{n}*bessely({n},2+i)"
            self.assertLess(abs(number(fields(difference))), 1e-30)

    def test_origin_limits_and_recurrence(self):
        for order in ("-1/2", "-3/2", "-5/2"):
            self.assertEqual(number(fields(f"bessely({order},0)")), 0)
        self.assertIn("NAN", fields("Y0(0)")["value"].upper())
        for order in ("0", "2", "1/3", "1/4+i/3"):
            source = f"bessely(({order})-1,2+i)+bessely(({order})+1,2+i)-2*({order})/(2+i)*bessely({order},2+i)"
            self.assertLess(abs(number(fields(source, precision=80))), 1e-70)

    def test_complex_derivatives_and_finite_sum(self):
        for order in (0, 1, -0.5, -1.5, -2.5):
            source = "{Dx(bessely("+str(order)+",x))|x=2+i}"
            actual = number(fields(source))
            expected = (reference(order,2.00001+1j)-reference(order,1.99999+1j))/0.00002
            self.assertLess(abs(actual-expected), 3e-9)
        for order in ("-3/2", "-5/2"):
            self.assertEqual(number(fields("{Dx(bessely("+order+",x))|x=0}")), 0)
        self.assertLess(abs(number(fields("sum(n,0,3,bessely(n,2+i))"))-
                            sum(reference(n,2+1j) for n in range(4))), 2e-10)

    def test_integer_primitives_differentiate_back(self):
        for order in (0, 1, 2, -1, -2):
            result = fields(f"bessely({order},2*x+1)", "integral")
            self.assertNotIn("integral(", result["integral_function"])
            primitive = result["integral"].split(" = ",1)[1].replace("x = NAN", "x = 1+i")
            derivative = fields(primitive, "derivative")
            expected = number(fields(f"bessely({order},3+2*i)", precision=80))
            self.assertLess(abs(number(derivative, "derivative_value")-expected), 2e-12)

    def test_aliases(self):
        for alias in ("Y0", "Y_0", "Y₀"):
            self.assertAlmostEqual(number(fields(alias+"(1)")).real, 0.08825696421567696, places=15)


class BesselYReadmeTests(unittest.TestCase):
    def test_readme_expression_y0(self):
        # README example: docs/expression.md. Run README cases after ordinary tests.
        self.assertAlmostEqual(number(fields("Y0(1)")).real, 0.088256964215677, places=14)
        self.assertAlmostEqual(number(fields("{Dx(Y0(x))|x=1}")).real, 0.781212821300289, places=14)


def load_tests(loader, tests, pattern):
    # Preserve README-last ordering for unittest discovery as well as direct execution.
    return unittest.TestSuite((
        loader.loadTestsFromTestCase(BesselYTests),
        loader.loadTestsFromTestCase(BesselYReadmeTests),
    ))


if __name__ == "__main__":
    unittest.main()
