"""Copied-formula round trips, independent of nested transform cancellation.

Compare values away from jumps and singularities. Unilateral Laplace recovery
only determines positive time. Distributional pairs have separate tests: Dirac
impulses must never be tested by assigning them a finite pointwise value.
"""

import math
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import mars_lab


# Each entry is a distinct implemented source family, not a nested-transform identity.
FOURIER_SOURCES = {
    "zero": "0",
    "constant": "1",
    "linear": "t",
    "step": "step(t)",
    "polynomial": "t^3",
    "gaussian": "exp(-t^2)",
    "shifted_gaussian": "exp(-2*t^2+t+1)",
    "rectangle": "rect(t)",
    "shifted_rectangle": "rect(2*t-1)",
    "triangle": "tri(t)",
    "sinc": "sinc(t)",
    "sinc_squared": "sinc(t)^2",
    "circle_profile": "circ(t)",
    "absolute_exponential": "exp(-2*abs(t))",
    "causal_exponential": "exp(-2*t)*step(t)",
    "reversed_exponential": "exp(2*t)*step(-t)",
    "sech": "sech(t)",
    "tanh": "tanh(t)",
    "csch": "csch(t)",
    "coth": "coth(t)",
    "arctangent": "atan(t)",
    "inverse_hyperbolic_tangent": "atanh(t)",
    "arcsine": "asin(t)",
    "arccosine": "acos(t)",
    "vertical_gamma": "gamma(1+i*t)",
    "cosh_power": "cosh(t)^(-1/2)",
    "sinh_power": "sinh(t)^(-1/2)",
    "absolute_sinh_power": "abs(sinh(t))^(-1/2)",
    "cosine": "cos(2*t+1)",
    "sine": "sin(2*t+1)",
    "harmonic_exponential": "exp(2*i*t)",
    "bessel_zero": "J_0(t)",
    "bessel_integer": "bessel_j(3,t)",
    "logarithm": "ln(abs(t))",
    "hermite_gaussian": "exp(-t^2/2)*HermiteH(3,t)",
    "weighted_gaussian": "t*exp(-t^2)",
    "modulated_gaussian": "exp(i*t)*exp(-t^2)",
    "chebyshev_window": "Tn(3,t)*rect(t/2)/sqrt(1-t^2)",
    "convolution": "convolve(exp(-t^2),exp(-t^2),t)",
}

LAPLACE_SOURCES = {
    "zero": "0",
    "constant": "1",
    "linear": "t",
    "polynomial": "t^3",
    "square_root": "sqrt(t)",
    "cube_root": "cubrt(t)",
    "fractional_power": "t^(2/3)",
    "exponential": "exp(-2*t)",
    "sine": "sin(2*t+1)",
    "cosine": "cos(2*t+1)",
    "sinh": "sinh(2*t+1)",
    "cosh": "cosh(2*t+1)",
    "versine": "versin(t)",
    "vercosine": "vercos(t)",
    "coversine": "coversin(t)",
    "covercosine": "covercos(t)",
    "haversine": "haversin(t)",
    "havercosine": "havercos(t)",
    "hacoversine": "hacoversin(t)",
    "hacovercosine": "hacovercos(t)",
    "logarithm": "ln(t)",
    "shifted_logarithm": "ln(2*t+1)",
    "decimal_logarithm": "log10(t)",
    "absolute_affine": "abs(2*t-1)",
    "floor": "floor(2*t)",
    "ceiling": "ceil(2*t)",
    "tanh": "tanh(2*t)",
    "sech": "sech(2*t)",
    "arctangent": "atan(2*t)",
    "arccotangent": "acot(2*t)",
    "inverse_sinh": "asinh(2*t)",
    "inverse_tanh": "atanh(2*t)",
    "gaussian": "exp(-t^2)",
    "shifted_gaussian": "exp(-2*t^2+t+1)",
    "error_function": "erf(t)",
    "shifted_error_function": "erf(2*t+1)",
    "complementary_error_function": "erfc(t)",
    "normal_density": "normal_pdf(t)",
    "normal_distribution": "normal_cdf(t)",
    "normal_log_density": "normal_logpdf(t)",
    "exponential_integral_e1": "E1(2*t)",
    "exponential_integral_ei_positive": "Ei(2*t)",
    "exponential_integral_ei_negative": "Ei(-2*t)",
    "lower_gamma": "gammainc_lower(3/2,2*t)",
    "upper_gamma": "gammainc_upper(3/2,2*t)",
    "regularised_lower_gamma": "gammainc_P(3/2,2*t)",
    "regularised_upper_gamma": "gammainc_Q(3/2,2*t)",
    "bessel_zero": "J_0(t)",
    "bessel_integer": "bessel_j(3,2*t)",
    "bessel_fractional": "bessel_j(1/2,t)",
    "bessel_second_kind": "bessel_y(0,2*t)",
    "clausen": "clausen2(t)",
    "conjugation": "conj((1+i)*t)",
    # Re is a domain-rendering helper, not a callable parser alias; use its definition.
    "real_part": "((1+i)*t+2+conj((1+i)*t+2))/2",
    "sine_power": "sin(t)^4",
    "cosine_power": "cos(t)^4",
    "weighted_sine": "t*sin(t)",
    "weighted_gaussian": "t*exp(-t^2)",
    "modulated_error_function": "exp(-t)*erf(t)",
    "causal_convolution": "causal_convolve(t,t,t)",
}


def fields(source, variable):
    result, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, variable, "evaluate")
    if code:
        raise AssertionError(raw)
    return result


def algebra(result):
    # Unbound output keeps distribution qualifications local; stripping a bound
    # Expression envelope would discard qualifications stored beside its bindings.
    text = result["unbound"]
    depth = 0
    for index, character in enumerate(text):
        if not depth and text.startswith(" where (", index):
            return text[:index]
        if character in "([":
            depth += 1
        elif character in ")]":
            depth -= 1
    return text


def value(source, point):
    result = fields("{"+source+" | t="+str(point)+"}", "t")
    return complex(result["value"].replace(" ", "").replace("i", "j"))


class TransformRoundTripTests(unittest.TestCase):
    def test_symbolic_power_round_trips(self):
        cases = (
            ("Fourier", "InverseFourier", "ω", (0, 1, 3, 33), (-0.3, 0.7)),
            ("InverseFourier", "Fourier", "ω", (0, 1, 3, 33), (-0.3, 0.7)),
            ("Laplace", "InverseLaplace", "s", (-1/3, 0, 2/3, 3), (0.3, 0.7)),
        )
        for forward, inverse, frequency, orders, points in cases:
            spectrum = algebra(fields(f"{forward}(t^n,t,{frequency})", frequency))
            restored = fields(f"{inverse}({spectrum},{frequency},t)", "t")
            self.assertNotIn("Fourier(", restored["function"])
            self.assertNotIn("Laplace(", restored["function"])
            for order in orders:
                for point in points:
                    with self.subTest(direction=forward, order=order, point=point):
                        result = fields("{"+algebra(restored)+f" | t={point}; n={order}"+"}", "t")
                        actual = complex(result["value"].replace(" ", "").replace("i", "j"))
                        self.assertLess(abs(actual-point**order), 1e-10*(1+abs(point**order)))

    def test_distributional_round_trips(self):
        # Compare canonical distribution expressions, never pointwise values of impulses.
        cases = (
            ("delta(t)", "δ(t)"),
            ("delta(2*t-1)", "½·δ(t - 0.5)"),
            ("Derivative(delta(t),3)", "Derivative(δ(t), 3)"),
            ("Derivative(delta(t),n)", "Derivative(δ(t), n)"),
            ("finite_part(1/abs(t))", "(1/|t| : finite part)"),
        )
        for forward, inverse in (("Fourier", "InverseFourier"), ("InverseFourier", "Fourier")):
            for source, expected in cases:
                with self.subTest(direction=forward, source=source):
                    spectrum = algebra(fields(f"{forward}({source},t,ω)", "ω"))
                    recovered = fields(f"{inverse}({spectrum},ω,t)", "t")
                    self.assertEqual(algebra(recovered), expected)
                    self.assertEqual(algebra(fields(recovered["expression"], "t")), expected)
                    if source.endswith(",n)"):
                        self.assertIn("n ∈ ℤ≥0", recovered["expression"])

    def check_round_trip(self, source, forward, inverse, frequency, points):
        transformed = fields(f"{forward}({source},t,{frequency})", frequency)
        self.assertNotIn("Fourier(", transformed["function"])
        self.assertNotIn("Laplace(", transformed["function"])
        spectrum = algebra(transformed)
        restored = fields(f"{inverse}({spectrum},{frequency},t)", "t")
        self.assertNotIn("Fourier(", restored["function"], restored["expression"])
        self.assertNotIn("Laplace(", restored["function"], restored["expression"])
        recovered = algebra(restored)
        # Reparse the inverse result too: a correct internal tree can be rendered incorrectly.
        for point in points:
            with self.subTest(point=point):
                expected = value(source, point)
                actual = value(recovered, point)
                self.assertTrue(math.isfinite(abs(actual)), restored["expression"])
                self.assertLess(abs(actual-expected), 1e-10*(1+abs(expected)), restored["expression"])

    def test_negative_denominator_rendering(self):
        for spectrum in ("1/(2+i*ω)", "1/(2-i*ω)"):
            restored = fields(f"InverseFourier({spectrum},ω,t)", "t")
            for point in (-0.3, 0.3):
                with self.subTest(spectrum=spectrum, point=point):
                    direct = value(f"InverseFourier({spectrum},ω,t)", point)
                    self.assertLess(abs(value(algebra(restored), point)-direct), 1e-12)


def round_trip_test(source, forward, inverse, frequency, points):
    def test(self):
        self.check_round_trip(source, forward, inverse, frequency, points)
    return test


for name, source in FOURIER_SOURCES.items():
    setattr(TransformRoundTripTests, "test_fourier_"+name,
            round_trip_test(source, "Fourier", "InverseFourier", "ω", (-0.3, 0.3, 1.3)))
    setattr(TransformRoundTripTests, "test_inverse_fourier_"+name,
            round_trip_test(source, "InverseFourier", "Fourier", "ω", (-0.3, 0.3, 1.3)))
for name, source in LAPLACE_SOURCES.items():
    setattr(TransformRoundTripTests, "test_laplace_"+name,
            round_trip_test(source, "Laplace", "InverseLaplace", "s", (0.3, 0.7, 1.3)))


if __name__ == "__main__":
    unittest.main()
