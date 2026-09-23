"""Arctangent Fourier pairs, copied spectra and independent numerical verification."""

import cmath
import math
import unittest

from test_fourier_tanh import algebra, fields, number, simpson


class AtanFourierTests(unittest.TestCase):
    def test_basic_pair_both_directions(self):
        for operator, coefficient in (("Fourier", -1j*math.pi), ("InverseFourier", 0.5j)):
            for point in (-1.3, -0.2, 0.4, 2):
                result = fields("{"+operator+"(atan(x),x,k) | k="+str(point)+"}")
                expected = coefficient*math.exp(-abs(point))/point
                self.assertLess(abs(number(result)-expected), 1e-12)
                self.assertNotIn("Fourier(", result["function"])

    def test_copied_formulas_round_trip(self):
        for argument in ("x", "2*x+1", "-2*x+1", "x/2-1/3"):
            for forward, inverse in (("Fourier", "InverseFourier"), ("InverseFourier", "Fourier")):
                original = "atan("+argument+")"
                spectrum = fields(f"{forward}({original},x,k)")
                for copied in (algebra(spectrum), spectrum["expression"]):
                    with self.subTest(argument=argument, direction=forward, copied=copied):
                        result = fields(f"{inverse}("+copied+",k,x)", "x")
                        self.assertNotIn("Fourier(", result["function"])
                        self.assertNotIn("k =", result["expression"])
                        for point in (-0.3, 0, 0.7):
                            bound = result["expression"].replace("x = NAN", "x = "+str(point))
                            expected = fields("{"+original+" | x="+str(point)+"}", "x")
                            self.assertLess(abs(number(fields(bound, "x"))-number(expected)), 2e-12)
                        if argument == "x":
                            self.assertEqual(result["unbound"], "atan(x) where (x ∈ ℝ)")

    def test_symbolic_real_scale_and_shift(self):
        spectrum = fields("Fourier(atan(a*x+b),x,k)")
        for condition in ("a ∈ ℝ", "b ∈ ℝ", "a ≠ 0", "k ≠ 0"):
            self.assertIn(condition, spectrum["expression"])
        restored = fields("InverseFourier("+spectrum["expression"]+",k,x)", "x")
        self.assertNotIn("Fourier(", restored["function"])
        for rate, offset in ((2, 0.3), (-2, 0.3), (0.5, -0.2)):
            result = fields("{Fourier(atan(a*x+b),x,k) | k=0.7; a="+str(rate)+"; b="+str(offset)+"}")
            expected = -1j*math.pi*math.copysign(1, rate)*cmath.exp(-0.7/abs(rate)+1j*0.7*offset/rate)/0.7
            self.assertLess(abs(number(result)-expected), 1e-12)
            copied = restored["expression"].replace("x = NAN", "x = 0.7")
            copied = copied.replace("a = NAN", "a = "+str(rate)).replace("b = NAN", "b = "+str(offset))
            self.assertLess(abs(number(fields(copied, "x"))-math.atan(rate*0.7+offset)), 1e-12)

    def test_direct_inverse_equivalent_spectra(self):
        for spectrum in ("-i*@pi*exp(-abs(k))/k", "-i*@pi/(k*exp(abs(k)))",
                         "-i*@pi*exp(-abs(k))*k^(-1)", "-i*@pi*exp(-abs(k)/2)*exp(-abs(k)/2)/k"):
            with self.subTest(spectrum=spectrum):
                result = fields("@Finv{"+spectrum+"}", "x")
                self.assertEqual(result["unbound"], "atan(x) where (x ∈ ℝ)")

    def test_domains_and_generated_conditional(self):
        result = fields("@F{atan(x)}")
        self.assertIn("k ∈ ℝ", result["expression"])
        self.assertIn("k ≠ 0", result["expression"])
        self.assertIn("k != 0", result["function"])
        self.assertIn("return @nan.", result["function"])
        for card in ("expression", "tex", "function"):
            self.assertNotIn("principal value", result[card])
            self.assertNotIn("PV(", result[card])
        self.assertIn("distributional", result["value_note"])
        for point in ("0", "i"):
            self.assertEqual(fields("{@F{atan(x)} | k="+point+"}")["value"], "NAN")
        inverse = fields("@Finv{-i*@pi*exp(-abs(k))/k}", "x")
        self.assertNotIn("x != 0", inverse["function"])
        self.assertEqual(number(fields("{@Finv{-i*@pi*exp(-abs(k))/k} | x=0}", "x")), 0)

    def test_damped_reciprocal_with_modulation(self):
        for operator, coefficient, direction in (("Fourier", -2j, -1), ("InverseFourier", 1j/math.pi, 1)):
            for point in (-0.3, 0, 0.7):
                source = operator+"(exp(-2*abs(k)+3*i*k)/k,k,x)"
                result = fields("{"+source+" | x="+str(point)+"}", "x")
                self.assertLess(abs(number(result)-coefficient*math.atan((point+direction*3)/2)), 1e-12)

    def test_quadrature_without_the_transform_formula(self):
        for point in (-1.3, 0, 0.4, 1.2):
            # Pairing positive and negative frequencies removes the singularity.
            expected = simpson(lambda k: point if k == 0 else math.exp(-k)*math.sin(k*point)/k, 0, 32)
            actual = number(fields("{@Finv{-i*@pi*exp(-abs(k))/k} | x="+str(point)+"}", "x"))
            self.assertLess(abs(actual-expected), 3e-10)
        # Independently cross-check against the existing rational transform rule for the derivative.
        for point in (-0.7, 0.7):
            rational = fields("{Fourier(1/(1+x^2),x,k) | k="+str(point)+"}")
            transformed = fields("{@F{atan(x)} | k="+str(point)+"}")
            self.assertLess(abs(1j*point*number(transformed)-number(rational)), 1e-12)

    def test_zero_rate_and_rejected_cases(self):
        self.assertEqual(number(fields("@F{atan(0*x)}")), 0)
        constant = fields("@F{atan(0*x+1)}")
        self.assertIn("δ", constant["expression"])
        self.assertNotIn("k ≠ 0", constant["expression"])
        for source in ("@F{atan(i*x)}", "@F{atan(x+i)}", "@Finv{exp(abs(k))/k}",
                       "@Finv{exp(-abs(k))/k^2}",
                       "InverseFourier(exp(-abs(k))/k where (k-1 ≠ 0),k,x)"):
            self.assertIn("Fourier(", fields(source, "x")["function"])


class ZZAtanFourierReadmeExamples(unittest.TestCase):
    def test_readme_atan_fourier_pair(self):
        # README examples: docs/expression.md, arctangent Fourier pair.
        result = fields("@F{atan(x)}")
        self.assertEqual(algebra(result), "-iπ·exp(-|k|)/k")
        self.assertIn("k ∈ ℝ", result["expression"])
        self.assertIn("k ≠ 0", result["expression"])
        result = fields("@Finv{-i*@pi*exp(-abs(k))/k}", "x")
        self.assertEqual(result["unbound"], "atan(x) where (x ∈ ℝ)")


if __name__ == "__main__":
    unittest.main()
