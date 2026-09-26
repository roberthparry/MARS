"""Vertical-line gamma transforms preserve their Cartesian coordinates on copying."""

import cmath
import math
import unittest

from test_fourier_tanh import algebra, fields, number, simpson


class GammaContourTests(unittest.TestCase):
    def test_default_coordinate_and_conditions(self):
        result = fields("@F{gamma(x)}")
        self.assertEqual(algebra(result), "2π·exp(k·Re(x) - exp(k))")
        self.assertIn(r"\operatorname{Im}(x)\to k", result["transform_identity_TeX"])
        self.assertIn("Re(x) > 0", result["expression"])
        self.assertIn("k ∈ ℝ", result["expression"])
        self.assertIn("realpart(x) > 0", result["function"])
        self.assertIn("holding the real coordinate fixed", result["value_note"])
        explicit = fields("Fourier(gamma(x),Im(x),k)")
        self.assertEqual(algebra(explicit), algebra(result))

    def test_copied_spectra_restore_complex_argument(self):
        for coordinate, frequency in (("x", "k"), ("t", "ω"), ("y", "m"), ("z", "n")):
            spectrum = fields(f"@F{{gamma({coordinate})}}", frequency)
            for copied in (algebra(spectrum), spectrum["expression"]):
                restored = fields("@Finv{"+copied+"}", coordinate)
                self.assertEqual(algebra(restored), f"Γ({coordinate})")
                self.assertIn(f"Re({coordinate}) > 0", restored["expression"])
                self.assertNotIn(f"{coordinate} ∈ ℝ", restored["expression"])
                for point in ("1+0.4i", "2-0.7i"):
                    evaluated = fields(restored["expression"].replace(f"{coordinate} = NAN", f"{coordinate} = {point}"), coordinate)
                    expected = number(fields(f"gamma({point})", coordinate))
                    self.assertLess(abs(number(evaluated)-expected), 1e-12)

    def test_both_directions_and_affine_arguments(self):
        for argument in ("x", "2*x+1", "-2*x+1", "2*x+1+i/3"):
            for forward, inverse in (("Fourier", "InverseFourier"), ("InverseFourier", "Fourier")):
                result = fields(f"{forward}(gamma({argument}),Im(x),k)")
                restored = fields(f"{inverse}("+result["expression"]+",k,Im(x))", "x")
                self.assertNotIn("fourier(", restored["function"])
                for point in ("0.2+0.7i", "0.1-0.4i"):
                    actual = number(fields(restored["expression"].replace("x = NAN", f"x = {point}"), "x"))
                    expected = number(fields("{gamma("+argument+") | x="+point+"}", "x"))
                    self.assertLess(abs(actual-expected), 1e-12)

    def test_numeric_spectrum_and_domain(self):
        for point in ("2+0.3i", "2-4i"):
            result = fields("{@F{gamma(x)} | x="+point+"; k=1}")
            self.assertLess(abs(number(result)-2*math.pi*math.exp(2-math.e)), 1e-12)
            self.assertIn("Re(x)", result["expression"])
        for point, frequency in (("-1+i", "1"), ("i", "1"), ("2+i", "i")):
            self.assertEqual(fields("{@F{gamma(x)} | x="+point+"; k="+frequency+"}")["value"], "NAN")

    def test_independent_inverse_integral(self):
        for point in (1+0.4j, 2-0.7j):
            integral = simpson(lambda k: cmath.exp(point.real*k-math.exp(k)+1j*point.imag*k), -40, 6)
            spectrum = "2*@pi*exp(k*Re(x)-exp(k))"
            restored = fields("@Finv{"+spectrum+"}", "x")
            value = str(point).strip("()").replace("j", "i")
            actual = number(fields(restored["expression"].replace("x = NAN", "x = "+value), "x"))
            self.assertLess(abs(actual-integral), 2e-11)

    def test_aliases_and_real_axis_are_not_reinterpreted(self):
        for alias in ("Re", "realpart", "real_part", "ℜ"):
            self.assertEqual(number(fields(alias+"(2+3i)")), 2)
        for alias in ("Im", "imagpart", "imag_part", "ℑ"):
            self.assertEqual(number(fields(alias+"(2+3i)")), 3)
            self.assertEqual(algebra(fields(f"Fourier(gamma(x),{alias}(x),k)")),
                             "2π·exp(k·Re(x) - exp(k))")
        expected = algebra(fields("@F{gamma(a+ix)}"))
        self.assertEqual(algebra(fields("@F{gamma(a+x*i)}")), expected)
        self.assertIn("ξ", fields("a+xi")["expression"])
        result = fields("Fourier(gamma(x),x,k)")
        self.assertEqual(result["value"], "NAN")
        self.assertIn("no ordinary or tempered-distribution", result["value_note"])


class ZZGammaContourReadmeExamples(unittest.TestCase):
    def test_readme_vertical_line_gamma_pair(self):
        # README examples: docs/expression.md, vertical-line gamma table and explicit coordinate.
        for source in ("@F{gamma(x)}", "Fourier(gamma(x),Im(x),k)"):
            result = fields(source)
            self.assertEqual(algebra(result), "2π·exp(k·Re(x) - exp(k))")
            self.assertIn("Re(x) > 0", result["expression"])
        result = fields("@Finv{2*@pi*exp(k*Re(x)-exp(k))}", "x")
        self.assertEqual(algebra(result), "Γ(x)")
        self.assertIn("Re(x) > 0", result["expression"])


if __name__ == "__main__":
    unittest.main()
