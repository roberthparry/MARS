"""Odd hyperbolic Fourier pairs, independently checked and copied between calls."""

import cmath
import math
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import mars_lab


def fields(source, variable="k"):
    result, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, variable, "evaluate")
    if code:
        raise AssertionError(raw)
    return result


def number(result):
    return complex(result["value"].replace(" ", "").replace("i", "j"))


def algebra(result):
    return result["expression"].split(" | ", 1)[0].removeprefix("{ ")


def simpson(function, left, right, steps=12000):
    step = (right-left)/steps
    total = function(left)+function(right)
    total += sum((4 if index % 2 else 2)*function(left+index*step) for index in range(1, steps))
    return total*step/3


class TanhFourierTests(unittest.TestCase):
    def test_basic_pairs_both_directions(self):
        for operator, coefficient in (("Fourier", -1j*math.pi), ("InverseFourier", 0.5j)):
            for function, dual in (("tanh", lambda k: 1/math.sinh(math.pi*k/2)),
                                   ("csch", lambda k: math.tanh(math.pi*k/2)),
                                   ("coth", lambda k: 1/math.tanh(math.pi*k/2))):
                source = f"{operator}({function}(x),x,k)"
                for point in (-1.1, 0.4, 1.3):
                    with self.subTest(operator=operator, function=function, point=point):
                        result = fields("{"+source+f" | k={point}"+"}")
                        self.assertNotIn("fourier(", result["function"])
                        self.assertLess(abs(number(result)-coefficient*dual(point)), 1e-12)

    def test_numerical_domain_and_conditional(self):
        result = fields("@F{tanh(x)}")
        self.assertIn("k ∈ ℝ", result["expression"])
        self.assertIn("k ≠ 0", result["expression"])
        self.assertIn("k != 0", result["function"])
        self.assertIn("else", result["function"])
        self.assertIn("return @nan.", result["function"])
        self.assertNotIn(r"\left(-i\right)", result["tex"])
        self.assertNotIn("(-i)", result["expression"])
        for card in ("expression", "tex", "function"):
            self.assertNotIn("principal value", result[card])
            self.assertNotIn("PV(", result[card])
        for point in ("0", "i"):
            self.assertEqual(fields("{@F{tanh(x)} | k="+point+"}")["value"], "NAN")
        self.assertEqual(number(fields("{@Finv{-i*@pi*csch(@pi*k/2)} | x=0}", "x")), 0)

    def test_copied_formulas_round_trip(self):
        for function in ("tanh", "csch", "coth"):
            for argument in ("x", "2*x+1", "-2*x+1", "x/2-1/3"):
                for forward, inverse in (("Fourier", "InverseFourier"), ("InverseFourier", "Fourier")):
                    spectrum = fields(f"{forward}({function}({argument}),x,k)")
                    for copied in (algebra(spectrum), spectrum["expression"]):
                        with self.subTest(function=function, argument=argument, direction=forward, copied=copied):
                            result = fields(f"{inverse}("+copied+",k,x)", "x")
                            self.assertNotIn("fourier(", result["function"])
                            self.assertIn(r"k\to x", result["transform_identity_TeX"])
                            self.assertNotIn("NAN", result["transform_identity_TeX"])
                            for point in (-0.3, 0.7):
                                bound = result["expression"].replace("x = NAN", "x = "+str(point))
                                expected = fields("{"+function+"("+argument+") | x="+str(point)+"}", "x")
                                self.assertLess(abs(number(fields(bound, "x"))-number(expected)), 2e-12)
                    if function == "tanh" and argument == "x":
                        self.assertEqual(algebra(result), "tanh(x)")
                    if function == "coth" and argument == "x":
                        self.assertEqual(algebra(result), "coth(x)")
                        self.assertIn("x ≠ 0", result["expression"])

    def test_direct_inverse_and_reciprocal_aliases(self):
        expected = fields("@Finv{-i*@pi*csch(@pi*k/2)}", "x")
        self.assertEqual(expected["unbound"], "tanh(x) where (x ∈ ℝ)")
        for spectrum in ("-i*@pi/sinh(@pi*k/2)", "-i*@pi*sinh(@pi*k/2)^(-1)",
                         "-i*@pi*cosech(@pi*k/2)"):
            self.assertEqual(fields("@Finv{"+spectrum+"}", "x")["tex"], expected["tex"])
        for source in ("csch(x)", "cosech(x)", "1/sinh(x)", "sinh(x)^(-1)"):
            result = fields("@F{"+source+"}")
            self.assertNotIn("fourier(", result["function"])
            self.assertNotIn("k ≠ 0", result["expression"])

    def test_symbolic_scale_shift_and_copied_inverse(self):
        for function in ("tanh", "csch", "coth"):
            spectrum = fields("Fourier("+function+"(a*x+b),x,k)")
            for condition in ("a ∈ ℝ", "b ∈ ℝ", "a ≠ 0"):
                self.assertIn(condition, spectrum["expression"])
            restored = fields("InverseFourier("+spectrum["expression"]+",k,x)", "x")
            self.assertNotIn("fourier(", restored["function"])
            for rate, offset in ((2, 0.3), (-2, 0.3), (0.5, -0.2)):
                source = "{Fourier("+function+"(a*x+b),x,k) | k=0.7; a="+str(rate)+"; b="+str(offset)+"}"
                result = fields(source)
                coordinate = math.pi*0.7/(2*rate)
                duals = {"tanh": lambda v: 1/math.sinh(v), "csch": math.tanh, "coth": lambda v: 1/math.tanh(v)}
                dual = duals[function](coordinate)
                expected = -1j*math.pi/abs(rate)*cmath.exp(1j*0.7*offset/rate)*dual
                self.assertLess(abs(number(result)-expected), 1e-12)
                copied = restored["expression"].replace("x = NAN", "x = 0.7")
                copied = copied.replace("a = NAN", "a = "+str(rate)).replace("b = NAN", "b = "+str(offset))
                originals = {"tanh": math.tanh, "csch": lambda v: 1/math.sinh(v), "coth": lambda v: 1/math.tanh(v)}
                expected_inverse = originals[function](rate*0.7+offset)
                self.assertLess(abs(number(fields(copied, "x"))-expected_inverse), 1e-12)

    def test_quadrature_independent_of_the_transform_formula(self):
        for point in (-1.3, 0.3, 1.2):
            # F(tanh') = i*k*F(tanh); the derivative is absolutely integrable.
            derivative = simpson(lambda x: math.cos(point*x)/math.cosh(x)**2, -24, 24)
            actual = number(fields("{@F{tanh(x)} | k="+str(point)+"}"))
            self.assertLess(abs(actual-derivative/(1j*point)), 3e-10)
            # Odd cancellation leaves a regular integrand with limit k at the origin.
            integral = simpson(lambda x: point if x == 0 else math.sin(point*x)/math.sinh(x), 0, 32)
            actual = number(fields("{@F{csch(x)} | k="+str(point)+"}"))
            self.assertLess(abs(actual+2j*integral), 3e-10)
            # coth(x) = sign(x) + 2*sign(x)/(exp(2*abs(x))-1).
            # Integrate the decaying part after odd cancellation; F(sign)(k) = -2i/k.
            correction = simpson(lambda x: point/2 if x == 0 else math.sin(point*x)/math.expm1(2*x), 0, 32)
            actual = number(fields("{@F{coth(x)} | k="+str(point)+"}"))
            self.assertLess(abs(actual+2j/point+4j*correction), 3e-10)

    def test_coth_aliases_poles_and_conditional(self):
        expected = fields("@F{coth(x)}")
        for source in ("1/tanh(x)", "tanh(x)^(-1)", "cosh(x)/sinh(x)"):
            self.assertEqual(fields("@F{"+source+"}")["tex"], expected["tex"])
        for card in ("expression", "tex", "function"):
            self.assertNotIn("principal value", expected[card])
            self.assertNotIn("PV(", expected[card])
        self.assertIn("k != 0", expected["function"])
        inverse = fields("@Finv{-i*@pi*coth(@pi*k/2)}", "x")
        self.assertEqual(inverse["unbound"], "coth(x) where (x ∈ ℝ; x ≠ 0)")
        self.assertIn("x != 0", inverse["function"])
        for point in ("0", "i"):
            self.assertEqual(fields("{@F{coth(x)} | k="+point+"}")["value"], "NAN")
            self.assertEqual(fields("{@Finv{-i*@pi*coth(@pi*k/2)} | x="+point+"}", "x")["value"], "NAN")
        shifted = fields("Fourier(coth(2*x+1),x,k)")
        restored = fields("InverseFourier("+shifted["expression"]+",k,x)", "x")
        at_pole = restored["expression"].replace("x = NAN", "x = -1/2")
        self.assertEqual(fields(at_pole, "x")["value"], "NAN")
        # General reciprocal/quotient recognition also preserves the existing tanh pair.
        for source in ("1/coth(x)", "coth(x)^(-1)", "sinh(x)/cosh(x)"):
            self.assertEqual(fields("@F{"+source+"}")["tex"], fields("@F{tanh(x)}")["tex"])

    def test_unrelated_domains_and_nonreal_rates_are_not_accepted(self):
        for source in ("Fourier(csch(x) where (x-1 ≠ 0),x,k)", "@F{tanh(i*x)}", "@F{csch(x+i)}",
                       "Fourier(coth(x) where (x-1 ≠ 0),x,k)", "@F{coth(i*x)}", "@F{coth(x+i)}"):
            self.assertIn("fourier(", fields(source)["function"])

    def test_zero_rate_is_a_constant_transform(self):
        result = fields("@F{tanh(0*x+1)}")
        self.assertIn("δ", result["expression"])
        self.assertNotIn("k ≠ 0", result["expression"])
        zero = fields("@F{tanh(0*x)}")
        self.assertEqual(number(zero), 0)
        constant = fields("@F{coth(0*x+1)}")
        self.assertIn("δ", constant["expression"])
        self.assertNotIn("k ≠ 0", constant["expression"])


class ZZTanhFourierReadmeExamples(unittest.TestCase):
    def test_readme_coth_fourier_pair(self):
        # README examples: docs/expression.md, odd hyperbolic Fourier pairs.
        spectrum = fields("@F{coth(x)}")
        self.assertIn("coth(½πk)", spectrum["expression"])
        self.assertIn("k ∈ ℝ", spectrum["expression"])
        self.assertIn("k ≠ 0", spectrum["expression"])
        inverse = fields("@Finv{-i*@pi*coth(@pi*k/2)}", "x")
        self.assertEqual(inverse["unbound"], "coth(x) where (x ∈ ℝ; x ≠ 0)")

    def test_readme_tanh_fourier_pair(self):
        # README examples: docs/expression.md, odd hyperbolic Fourier pairs.
        spectrum = fields("@F{tanh(x)}")
        self.assertIn("cosech(½πk)", spectrum["expression"])
        self.assertIn("k ∈ ℝ", spectrum["expression"])
        self.assertIn("k ≠ 0", spectrum["expression"])
        for source in ("@Finv{-i*@pi*csch(@pi*k/2)}", "@Finv{-i*@pi/sinh(@pi*k/2)}"):
            self.assertEqual(fields(source, "x")["unbound"], "tanh(x) where (x ∈ ℝ)")


if __name__ == "__main__":
    unittest.main()
