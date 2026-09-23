"""Boundary-value and gamma Fourier pairs, including independently copied spectra."""

import cmath
import math
import unittest

from test_fourier_tanh import algebra, fields, number, simpson


class BranchFourierTests(unittest.TestCase):
    def test_copied_branch_round_trips(self):
        for function in ("atanh", "asin", "acos"):
            for argument in ("x", "2*x+1", "-2*x+1", "x/2-1/3", "a*x+b"):
                for forward, inverse in (("Fourier", "InverseFourier"), ("InverseFourier", "Fourier")):
                    source = f"{function}({argument})"
                    spectrum = fields(f"{forward}({source},x,k)")
                    for copied in (algebra(spectrum), spectrum["expression"]):
                        with self.subTest(source=source, direction=forward, copied=copied):
                            restored = fields(f"{inverse}({copied},k,x)", "x")
                            self.assertNotIn("Fourier(", restored["function"])
                            self.assertNotIn("k =", restored["expression"])
                            for point in (-2.3, -0.2, 0, 0.7, 2.3):
                                bound = restored["expression"].replace("x = NAN", f"x = {point}")
                                bound = bound.replace("a = NAN", "a = -2").replace("b = NAN", "b = 0.3")
                                expected = fields("{"+source+f" | x={point}; a=-2; b=0.3"+"}", "x")
                                actual = fields(bound, "x")
                                if expected["value"] == "NAN" or "∞" in expected["value"]:
                                    self.assertEqual(actual["value"], "NAN")
                                else:
                                    self.assertLess(abs(number(actual)-number(expected)), 2e-12)

    def test_full_distribution_and_mathematical_notation(self):
        for function in ("atanh", "asin", "acos"):
            result = fields(f"@F{{{function}(x)}}")
            self.assertIn("δ(k)", result["expression"])
            self.assertNotIn("k ≠ 0", result["expression"])
            for card in ("expression", "tex", "function"):
                self.assertNotIn(": principal value", result[card])
                self.assertNotIn(": finite part", result[card])
                self.assertNotIn("PV(", result[card])
                self.assertNotIn("Fp(", result[card])
            if function != "atanh":
                self.assertIn("step(k)/k", result["expression"])
                self.assertNotIn("Dk(", result["expression"])
                self.assertIn("ln(2)", result["expression"])
                self.assertIn("γ", result["expression"])
                self.assertIn("@eulermascheroni", result["function"])
                self.assertNotIn(r"\partial", result["tex"])
                self.assertNotIn(r"\operatorname{D}", result["tex"])
                self.assertNotIn("delta(k).ln", result["function"])

    def test_direct_spectra_and_zero_frequency_constants(self):
        cases = (
            ("i*@pi^2*delta(k)-2*i*@pi*step(k)*sinc(k/@pi)", "atanh(x)"),
            ("2*i*@pi*((ln(2)-@eulermascheroni)*delta(k)-besselj(0,k)*step(k)/k)", "asin(x)"),
            ("@pi^2*delta(k)-2*i*@pi*((ln(2)-@eulermascheroni)*delta(k)-besselj(0,k)*step(k)/k)", "acos(x)"),
        )
        for spectrum, original in cases:
            with self.subTest(original=original):
                result = fields(f"InverseFourier({spectrum},k,x)", "x")
                self.assertEqual(algebra(result), original)
                # Removing an impulse must change the recovered constant, not get silently ignored.
                altered = fields(f"InverseFourier(({spectrum})+delta(k),k,x)", "x")
                actual = fields(altered["expression"].replace("x = NAN", "x = 0"), "x")
                expected = number(fields("{"+original+" | x=0}", "x"))+1/(2*math.pi)
                self.assertLess(abs(number(actual)-expected), 2e-12)

    def test_legacy_derivative_spectra(self):
        for rate in (1, 2, -2):
            spectrum = (f"2*i*@pi*((ln(2)-@eulermascheroni)*delta(k)"
                        f"-({rate}/abs({rate}))*besselj(0,k/({rate}))*Dk(step(k/({rate}))*ln(abs(k/({rate})))))")
            result = fields(f"InverseFourier({spectrum},k,x)", "x")
            self.assertNotIn("Fourier(", result["function"])
            for point in (-1.3, 0, 1.3):
                actual = fields(result["expression"].replace("x = NAN", f"x = {point}"), "x")
                expected = fields(f"asin({rate}*({point}))", "x")
                self.assertLess(abs(number(actual)-number(expected)), 2e-12)

    def test_asin_distribution_against_gaussian_test_function(self):
        # Independently test the zero-frequency normalisation against phi(k)=exp(-k^2).
        def j0(x):
            term = total = 1.0
            for n in range(1, 60):
                term *= -x*x/(4*n*n)
                total += term
            return total
        for rate in (0.5, 1, 2):
            low = simpson(lambda k: 0 if k == 0 else (j0(k/rate)*math.exp(-k*k)-1)/k, 0, 1)
            high = simpson(lambda k: j0(k/rate)*math.exp(-k*k)/k, 1, 12)
            gamma = 0.5772156649015328606
            spectral = 2*math.pi*(math.log(2*rate)-gamma-low-high)
            original = 2*math.sqrt(math.pi)*simpson(
                lambda u: u*math.sinh(u)/rate*math.exp(-(math.cosh(u)/rate)**2/4), 0, 6)
            self.assertLess(abs(spectral-original), 2e-10)

    def test_gamma_pairs_and_round_trips(self):
        for argument in ("1+i*x", "2+2*i*x", "1-2*i*x", "a+i*x", "1+i/3+i*x"):
            for forward, inverse in (("Fourier", "InverseFourier"), ("InverseFourier", "Fourier")):
                spectrum = fields(f"{forward}(gamma({argument}),x,k)")
                self.assertNotIn("Fourier(", spectrum["function"])
                for copied in (algebra(spectrum), spectrum["expression"]):
                    with self.subTest(argument=argument, direction=forward):
                        result = fields(f"{inverse}({copied},k,x)", "x")
                        self.assertNotIn("Fourier(", result["function"])
                        for point in (-0.7, 0, 1.2):
                            bound = result["expression"].replace("x = NAN", f"x = {point}").replace("a = NAN", "a = 2")
                            expected = fields("{gamma("+argument+f") | x={point}; a=2"+"}", "x")
                            self.assertLess(abs(number(fields(bound, "x"))-number(expected)), 2e-12)

    def test_gamma_independent_inverse_quadrature(self):
        for point in (-0.7, 0, 1.2):
            expected = simpson(lambda k: cmath.exp(k-math.exp(k)+1j*k*point), -32, 5)
            result = fields("{@Finv{2*@pi*exp(k-exp(k))} | x="+str(point)+"}", "x")
            self.assertLess(abs(number(result)-expected), 2e-10)

    def test_gamma_existence_conditions(self):
        for source in ("gamma(x)", "gamma(-2*x+1)"):
            result = fields("Fourier("+source+",x,k)")
            self.assertEqual(result["value"], "NAN")
            self.assertIn("no ordinary or tempered-distribution", result["value_note"])
            self.assertNotIn("Fourier(", result["function"])
        result = fields("@F{gamma(a+i*b*x)}")
        self.assertIn("Re(a) > 0", result["expression"])
        self.assertIn("b ∈ ℝ", result["expression"])
        self.assertIn("b ≠ 0", result["expression"])
        for source in ("@F{gamma(-1+i*x)}", "@F{asin(x+i)}", "@F{atanh(i*x)}",
                       "@Finv{exp(k+exp(k))}"):
            self.assertIn("Fourier(", fields(source)["function"])


class ZZBranchFourierReadmeExamples(unittest.TestCase):
    def test_readme_gamma_fourier_pair(self):
        # README examples: docs/expression.md, inverse-function and gamma Fourier pairs.
        result = fields("@F{gamma(1+i*x)}")
        self.assertEqual(algebra(result), "2π·exp(k - exp(k))")
        result = fields("@Finv{2*@pi*exp(k-exp(k))}", "x")
        self.assertEqual(algebra(result), "Γ(ix + 1)")
        result = fields("Fourier(gamma(x),x,k)")
        self.assertEqual(result["value"], "NAN")
        self.assertIn("no ordinary or tempered-distribution", result["value_note"])

    def test_readme_branch_fourier_round_trips(self):
        # README examples: docs/expression.md, inverse-function and gamma Fourier pairs.
        for function in ("atanh", "asin", "acos"):
            spectrum = fields(f"@F{{{function}(x)}}")
            result = fields("InverseFourier("+spectrum["expression"]+",k,x)", "x")
            self.assertEqual(algebra(result), function+"(x)")


if __name__ == "__main__":
    unittest.main()
