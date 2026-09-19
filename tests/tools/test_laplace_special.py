"""Special Laplace regressions against independent, standard-library quadrature.

Requires an already-built mars_lab containing expr_laplace_special_rule's wiring.
This script never builds anything and unittest runs its cases sequentially.
Set MARS_LAPLACE_SPECIAL_BINARY to use a different existing helper binary.
The conservative Re(s)>0 restrictions are intentional, including E1 and upper
incomplete gamma: their larger convergence domains need removable values at s=0.
"""

import cmath
import math
import os
from pathlib import Path
import re
import subprocess
import sys
import unittest


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

import mars_lab


BINARY = Path(os.environ.get(
    "MARS_LAPLACE_SPECIAL_BINARY", str(ROOT / "build/release/scratch/mars_lab")))
EULER_GAMMA = 0.5772156649015328606


def quadrature(function, left, right, tolerance=2e-12):
    """Adaptive Simpson quadrature, also accepting complex-valued integrands."""
    middle = (left + right) / 2
    first, centre, last = function(left), function(middle), function(right)
    whole = (right - left) * (first + 4 * centre + last) / 6

    def refine(a, b, fa, fm, fb, estimate, error, depth):
        mid = (a + b) / 2
        fl, fr = function((a + mid) / 2), function((mid + b) / 2)
        lo = (mid - a) * (fa + 4 * fl + fm) / 6
        hi = (b - mid) * (fm + 4 * fr + fb) / 6
        delta = lo + hi - estimate
        if abs(delta) <= 15 * error:
            return lo + hi + delta / 15
        if depth == 0:
            raise AssertionError("Independent quadrature did not converge")
        return (refine(a, mid, fa, fl, fm, lo, error / 2, depth - 1)
                + refine(mid, b, fm, fr, fb, hi, error / 2, depth - 1))

    return refine(left, right, first, centre, last, whole, tolerance, 24)


def laplace_quadrature(function, target, growth=0.0):
    """Use t=u^8 to smooth the integrable origins in the chosen test cases.

    Every case has a positive damping margin. Forty decay lengths make the
    omitted tails negligible compared with the 3e-9 native-value tolerance.
    """
    decay = target.real - growth
    if decay <= 0:
        raise ValueError("Quadrature requires a positive damping margin")
    limit = (40 / decay) ** (1 / 8)

    def integrand(u):
        if u == 0:
            return 0.0
        t = u ** 8
        return 8 * u ** 7 * cmath.exp(-target * t) * function(t)

    return quadrature(integrand, 0.0, limit)


def positive_ei(x):
    """Ei on x>0, from its convergent defining series, not a transform formula."""
    term = x
    total = term
    for k in range(2, 1000):
        term *= x / k
        contribution = term / k
        total += contribution
        if abs(contribution) < 2e-16 * max(1.0, abs(total)):
            return EULER_GAMMA + math.log(x) + total
    raise AssertionError("Ei reference series did not converge")


def bessel_j(order, x):
    """J from its power series, with the exact negative-integer identity."""
    if order < 0 and float(order).is_integer():
        return (-1) ** int(-order) * bessel_j(-order, x)
    term = (x / 2) ** order / math.gamma(order + 1)
    total = term
    for k in range(1, 1000):
        term *= -(x * x / 4) / (k * (k + order))
        total += term
        if abs(term) < 2e-16 * max(1.0, abs(total)):
            return total
    raise AssertionError("Bessel J reference series did not converge")


def bessel_y_zero(x):
    """Y0 from its logarithmic/harmonic-number series."""
    term, harmonic, correction = 1.0, 0.0, 0.0
    for k in range(1, 1000):
        term *= -(x * x / 4) / (k * k)
        harmonic += 1 / k
        contribution = -harmonic * term
        correction += contribution
        if abs(contribution) < 2e-16 * max(1.0, abs(correction)):
            return 2 / math.pi * ((math.log(x / 2) + EULER_GAMMA) * bessel_j(0, x) + correction)
    raise AssertionError("Bessel Y reference series did not converge")


def incomplete_gamma(shape, x, upper):
    """Independent integer/half-integer values from exp, erf and recurrences."""
    if float(shape).is_integer():
        value = math.exp(-x) if upper else -math.expm1(-x)
        current = 1.0
    else:
        if not float(2 * shape).is_integer():
            raise ValueError("Reference only covers integer and half-integer shapes")
        value = math.sqrt(math.pi) * (math.erfc(math.sqrt(x)) if upper else math.erf(math.sqrt(x)))
        current = 0.5
    while current < shape:
        tail = x ** current * math.exp(-x)
        value = current * value + (tail if upper else -tail)
        current += 1
    return value


def literal(value):
    value = complex(value)
    return f"({value.real:.17g}+({value.imag:.17g})*i)"


def clausen_fourier_reference(order, rate, target, tolerance=1e-10):
    """Integrate each Fourier term and bound the remaining infinite series.

    Let b=|rate|. Beyond N>=sqrt(2)*|s|/b, |s^2+b^2*n^2|>=b^2*n^2/2.
    The integral test therefore bounds the absolute tail by
      2/(b*p*N^p)                 for even p,
      2*|s|/(b^2*(p+1)*N^(p+1))  for odd p.
    This reference uses neither digamma/zeta nor the implementation's recurrence.
    """
    if order < 2 or rate == 0 or target.real <= 0:
        raise ValueError("Fourier reference requires p>=2, non-zero real rate and Re(s)>0")
    magnitude = abs(rate)
    even = order % 2 == 0
    exponent = order if even else order + 1
    coefficient = 2 / (magnitude * order) if even else 2 * abs(target) / (magnitude ** 2 * (order + 1))
    count = max(8, math.ceil(math.sqrt(2) * abs(target) / magnitude),
                math.ceil((coefficient / tolerance) ** (1 / exponent)))
    tail_bound = coefficient / count ** exponent
    while tail_bound > tolerance:
        count += 1
        tail_bound = coefficient / count ** exponent
    real_terms, imaginary_terms = [], []
    for n in range(1, count + 1):
        denominator = target * target + (magnitude * n) ** 2
        term = (rate / (n ** (order - 1) * denominator) if even
                else target / (n ** order * denominator))
        real_terms.append(term.real)
        imaginary_terms.append(term.imag)
    return complex(math.fsum(real_terms), math.fsum(imaginary_terms)), tail_bound


def clausen_one_log_reference(rate, target):
    """Integrate -log|2 sin(a*t/2)| over a period, then sum the periods.

    Reflect the second half-period onto the first and use t=(T/2)*u^4;
    the resulting u^3*log(u) endpoints have finite zero limits. Periodicity
    removes the infinite tail exactly, even though the original Fourier series
    is only conditionally convergent. Negative rates give the same Cl1.
    """
    if rate == 0 or target.real <= 0:
        raise ValueError("Logarithmic reference requires non-zero real rate and Re(s)>0")
    magnitude = abs(rate)
    period = 2 * math.pi / magnitude
    half = period / 2

    def integrand(u):
        if u == 0:
            return 0.0
        time = half * u ** 4
        value = -math.log(2 * math.sin(magnitude * time / 2))
        weights = cmath.exp(-target * time) + cmath.exp(-target * (period - time))
        return 4 * half * u ** 3 * weights * value

    return quadrature(integrand, 0.0, 1.0) / (1 - cmath.exp(-target * period))


class LaplaceSpecialTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not BINARY.is_file():
            raise unittest.SkipTest("Build and wire the native helper separately before running these tests")

    def evaluate(self, source):
        try:
            completed = subprocess.run(
                [str(BINARY), source, "s", "40", "evaluate"], cwd=ROOT,
                text=True, capture_output=True, timeout=30)
        except subprocess.TimeoutExpired:
            # unittest prints error details only when the whole suite finishes;
            # preserve this diagnosis even if main interrupts the remaining run.
            print(f"\nNATIVE_TIMEOUT (30s): {source}", file=sys.stderr, flush=True)
            raise
        raw = completed.stdout + ("\n" if completed.stderr else "") + completed.stderr
        if completed.returncode != 0:
            print(f"\nNATIVE_EXIT ({completed.returncode}): {source}\n{raw}", file=sys.stderr, flush=True)
        self.assertEqual(completed.returncode, 0, raw)
        fields = mars_lab.parse_mars_lab_output(raw)
        self.assertIn("function", fields, raw)
        return fields, raw

    def assert_transform(self, operand, target, expected, bindings=""):
        source = "{Laplace(" + operand + ",t,s) | s=" + literal(target) + bindings + "}"
        fields, raw = self.evaluate(source)
        self.assertNotIn("Laplace(", fields["function"], raw)
        try:
            scalar = re.sub(r"\s+", "", fields["value"]).replace("−", "-").replace("i", "j")
            actual = complex(scalar)
        except (KeyError, ValueError) as error:
            print(f"\nNONSCALAR_VALUE: {source}\n{raw}", file=sys.stderr, flush=True)
            self.fail(f"Cannot parse native scalar value: {error}\n{raw}")
        if not (math.isfinite(actual.real) and math.isfinite(actual.imag)):
            print(f"\nNONFINITE_VALUE: {source}\n{raw}", file=sys.stderr, flush=True)
            self.fail(f"Non-finite native transform: {actual}\n{raw}")
        residual = abs(actual - expected)
        tolerance = 3e-9 * max(1.0, abs(expected))
        if residual > tolerance:
            print(f"\nNUMERIC_MISMATCH: {source}\nactual={actual!r}, expected={expected!r}, "
                  f"error={residual:.6g}, tolerance={tolerance:.6g}", file=sys.stderr, flush=True)
        self.assertLessEqual(residual, tolerance, raw)

    def test_exponential_integrals_against_independent_integrals(self):
        for rate in (0.5, 1.0, 2.0):
            for target in (2.5 * rate, (2.5 + 0.75j) * rate):
                with self.subTest(rate=rate, target=target):
                    # Fubini on E1(x)=integral_1^infinity exp(-x*u)/u du,
                    # followed by u=1/v; no logarithmic transform is evaluated.
                    expected = quadrature(lambda v: 1 / (rate + target * v), 0.0, 1.0)
                    self.assert_transform(f"E1({rate}*t)", target, expected)
                    self.assert_transform(f"Ei(-{rate}*t)", target, -expected)
                    positive = laplace_quadrature(lambda t: positive_ei(rate * t), target, rate)
                    self.assert_transform(f"Ei({rate}*t)", target, positive)

    def test_incomplete_gamma_all_four_functions(self):
        for shape in (0.5, 1.0, 1.5, 3.0):
            for upper in (False, True):
                for regularised in (False, True):
                    name = ("gammainc_Q" if upper else "gammainc_P") if regularised else (
                        "gammainc_upper" if upper else "gammainc_lower")
                    with self.subTest(function=name, shape=shape):
                        rate, target = 1.5, 2.75 + 0.5j
                        factor = math.gamma(shape) if regularised else 1.0
                        expected = laplace_quadrature(
                            lambda t: incomplete_gamma(shape, rate * t, upper) / factor, target)
                        self.assert_transform(f"{name}({shape},{rate}*t)", target, expected)

    def test_bessel_j_admissible_and_negative_integer_orders(self):
        for order in (-3.0, -2.0, -1.0, -0.75, -0.5, 0.0, 0.5, 1.0, 2.25):
            for rate in (0.5, 1.5):
                with self.subTest(order=order, rate=rate):
                    target = 3.0 + 0.5j
                    expected = laplace_quadrature(lambda t: bessel_j(order, rate * t), target)
                    self.assert_transform(f"BesselJ({order},{rate}*t)", target, expected)

    def test_bessel_principal_root_with_large_imaginary_frequency(self):
        for target in (0.5 + 2j, 0.5 - 2j):
            for order, circular in ((0.5, math.sin), (-0.5, math.cos)):
                with self.subTest(order=order, target=target):
                    # Half-integer Bessel functions supply an elementary oracle
                    # without cancellation from a power series at large t.
                    expected = laplace_quadrature(
                        lambda t: math.sqrt(2 / (math.pi * t)) * circular(t), target)
                    self.assert_transform(f"BesselJ({order},t)", target, expected)

    def test_bessel_y_zero_logarithmic_origin(self):
        for rate in (0.5, 1.0, 1.5):
            with self.subTest(rate=rate):
                target = 3.0 + 0.5j
                expected = laplace_quadrature(lambda t: bessel_y_zero(rate * t), target)
                self.assert_transform(f"BesselY(0,{rate}*t)", target, expected)

    def test_unscaled_reciprocal_and_supplied_constant_rates(self):
        expected = quadrature(lambda v: 1 / (1 + 2 * v), 0.0, 1.0)
        self.assert_transform("E1(t)", 2, expected)
        expected = quadrature(lambda v: 1 / (0.5 + 2 * v), 0.0, 1.0)
        self.assert_transform("E1(t/2)", 2, expected)
        self.assert_transform("E1(c*t)", 2, expected, "; c=1/2")
        expected = laplace_quadrature(lambda t: bessel_j(-3, 1.5 * t), 3)
        self.assert_transform("BesselJ(n,c*t)", 3, expected, "; n=-3,c=3/2")
        expected = laplace_quadrature(lambda t: bessel_j(0.5, t), 2)
        self.assert_transform("BesselJ(v,t)", 2, expected, ",v=1/2")
        expected = laplace_quadrature(lambda t: incomplete_gamma(1.5, t, False), 2)
        self.assert_transform("gammainc_lower(v,t)", 2, expected, ",v=3/2")

    def test_symbolic_order_and_shape_guards(self):
        for operand, guard in (("BesselJ(v,t)", "realpart(v)>-1"),
                               ("gammainc_lower(v,t)", "realpart(v)>0"),
                               ("gammainc_upper(v,t)", "realpart(v)>0"),
                               ("gammainc_P(v,t)", "realpart(v)>0"),
                               ("gammainc_Q(v,t)", "realpart(v)>0")):
            with self.subTest(operand=operand):
                fields, raw = self.evaluate("Laplace(" + operand + ",t,s)")
                compact = re.sub(r"\s+", "", raw)
                self.assertNotIn("Laplace(", fields["function"], raw)
                self.assertIn(guard, compact)
                self.assertIn("realpart(s)>0", compact)
        # Bind v as a free variable, so the guard remains active at evaluation.
        for operand, value in (("BesselJ(v,t)", "-1"), ("gammainc_lower(v,t)", "0")):
            fields, raw = self.evaluate("{Laplace(" + operand + ",t,s) | s=2,v=" + value + "}")
            self.assertEqual(fields.get("value"), "NAN", raw)

    def test_symbolic_rates_retain_positive_real_part_guards(self):
        operands = ("E1(a*t)", "gammainc_lower(v,a*t)", "gammainc_upper(v,a*t)",
                    "gammainc_P(v,a*t)", "gammainc_Q(v,a*t)")
        for operand in operands:
            with self.subTest(operand=operand):
                fields, raw = self.evaluate("Laplace(" + operand + ",t,s)")
                compact = re.sub(r"\s+", "", raw)
                self.assertNotIn("Laplace(", fields["function"], raw)
                self.assertIn("realpart(a)>0", compact)
                self.assertIn("realpart(s)>0", compact)
                if operand != "E1(a*t)":
                    self.assertIn("realpart(v)>0", compact)
        fields, raw = self.evaluate("{Laplace(E1(a*t),t,s) | s=?; a=?}")
        self.assertNotIn("Laplace(", fields["function"], raw)
        self.assertIn("realpart(a)>0", re.sub(r"\s+", "", raw))
        for operand in ("E1(a*t)", "gammainc_lower(1/2,a*t)", "gammainc_Q(2,a*t)"):
            for rate in ("0", "-1", "i", "-1+i"):
                with self.subTest(operand=operand, rate=rate):
                    fields, raw = self.evaluate(
                        "{Laplace(" + operand + ",t,s) | s=2,a=" + rate + "}")
                    self.assertEqual(fields.get("value"), "NAN", raw)

    def test_symbolic_complex_rates_against_independent_integrals(self):
        for rate, target in ((1.0, 2.0), (1 + 0.5j, 2 + 0.25j), (0.5 + 3j, 0.75 - 9j)):
            with self.subTest(rate=rate, target=target):
                # Last case puts a/(a+s) in the left half-plane, off its cut.
                expected = quadrature(lambda v: 1 / (rate + target * v), 0.0, 1.0)
                self.assert_transform("E1(a*t)", target, expected, ",a=" + literal(rate))
                shape = 0.5
                # Fubini reduces the defining incomplete-gamma integral to a
                # gamma-density moment. Compute that moment by quadrature,
                # without using the implemented ratio-power expression.
                moment = laplace_quadrature(
                    lambda t: t ** (shape - 1) * cmath.exp(-rate * t), target)
                lower = rate ** shape * moment / target
                upper = math.gamma(shape) / target - lower
                for name, value in (("gammainc_lower", lower), ("gammainc_upper", upper),
                                    ("gammainc_P", lower / math.gamma(shape)),
                                    ("gammainc_Q", upper / math.gamma(shape))):
                    self.assert_transform(f"{name}(v,a*t)", target, value,
                                          ",v=1/2,a=" + literal(rate))

    def test_existing_bessel_zero_complex_and_zero_scales(self):
        for rate in (0.0, -1.0, 1j, 1 + 1j):
            with self.subTest(rate=rate):
                target = 3.0 + 0.5j
                expected = laplace_quadrature(lambda t: bessel_j(0, rate * t), target,
                                              abs(complex(rate).imag))
                self.assert_transform("BesselJ(0,a*t)", target, expected, "; a=" + literal(rate))

    def test_clausen_orders_two_through_32_against_bounded_fourier_sums(self):
        for order in range(2, 33):
            with self.subTest(order=order):
                target = 2.0 + 0.75j
                expected, tail = clausen_fourier_reference(order, 1.0, target)
                self.assertLessEqual(tail, 1e-10)
                self.assert_transform(f"Cl({order},t)", target, expected)

    def test_clausen_one_against_periodic_logarithmic_integral(self):
        for rate in (0.5, 1.0, -1.0, -2.0):
            for target in (0.25, 2.0 + 0.75j):
                with self.subTest(rate=rate, target=target):
                    expected = clausen_one_log_reference(rate, target)
                    self.assert_transform(f"Cl(1,{rate}*t)", target, expected)

    def test_clausen_real_scaling_and_negative_rate_parity(self):
        for order in (2, 3, 4, 31, 32):
            for rate in (0.5, -0.5, 1.5, -1.5):
                with self.subTest(order=order, rate=rate):
                    target = 2.75 - 0.5j
                    expected, tail = clausen_fourier_reference(order, rate, target)
                    self.assertLessEqual(tail, 1e-10)
                    self.assert_transform(f"Cl({order},{rate}*t)", target, expected)

    def test_clausen_two_aliases_and_supplied_constant_parameters(self):
        target = 1.5 + 0.5j
        expected, _ = clausen_fourier_reference(2, 1.0, target)
        for name in ("clausen2", "Cl2", "Cl₂", "clausen"):
            with self.subTest(name=name):
                self.assert_transform(f"{name}(t)", target, expected)
        expected, _ = clausen_fourier_reference(2, -0.5, target)
        self.assert_transform("clausen2(-t/2)", target, expected)
        self.assert_transform("Cl(n,a*t)", target, expected, "; n=2,a=-1/2")
        expected, _ = clausen_fourier_reference(5, -0.5, target)
        self.assert_transform("Cl(n,a*t)", target, expected, "; n=5,a=-1/2")

    def test_clausen_high_orders_keep_finite_sum(self):
        for order in (9, 16, 31, 32):
            with self.subTest(order=order):
                fields, raw = self.evaluate(f"Laplace(Cl({order},t),t,s)")
                self.assertNotIn("Laplace(", fields["function"], raw)
                self.assertIn("sum(", fields["function"], raw)
                self.assertLess(len(fields["function"]), 2000, raw)
        # Both a target called j and a supplied rate called j must stay outside
        # the finite sum's index scope. These use the same independent tail bound.
        target = 2.0 + 0.75j
        expected, _ = clausen_fourier_reference(31, 1.5, target)
        self.assert_transform("Cl(31,j*t)", target, expected, "; j=3/2")
        expected, _ = clausen_fourier_reference(32, 1.0, target)
        fields, raw = self.evaluate("{Laplace(Cl(32,t),t,j) | j=" + literal(target) + "}")
        self.assertNotIn("Laplace(", fields["function"], raw)
        actual = complex(re.sub(r"\s+", "", fields["value"]).replace("−", "-").replace("i", "j"))
        self.assertLessEqual(abs(actual - expected), 3e-9 * max(1.0, abs(expected)), raw)

    def test_clausen_frequency_guard_and_unsupported_parameters(self):
        for operand in ("Cl(1,t)", "clausen2(t)", "Cl(3,t)", "Cl(32,-2*t)"):
            fields, raw = self.evaluate("Laplace(" + operand + ",t,s)")
            self.assertNotIn("Laplace(", fields["function"], raw)
            self.assertIn("realpart(s)>0", re.sub(r"\s+", "", raw))
            for target in ("0", "-1", "i"):
                with self.subTest(operand=operand, target=target):
                    fields, raw = self.evaluate("{Laplace(" + operand + ",t,s) | s=" + target + "}")
                    self.assertEqual(fields.get("value"), "NAN", raw)
        for operand in ("Cl(33,t)", "Cl(3/2,t)", "Cl(n,t)", "Cl(t,t)", "Cl(3,a*t)",
                        "clausen2(a*t)", "clausen2(i*t)", "Cl(3,(1+i)*t)", "clausen2(t+1)"):
            with self.subTest(operand=operand):
                fields, raw = self.evaluate("Laplace(" + operand + ",t,s)")
                self.assertIn("Laplace(", fields["function"], raw)
        for source in ("{Laplace(Cl(n,t),t,s) | s=2,n=3}",
                       "{Laplace(clausen2(a*t),t,s) | s=2,a=1}"):
            fields, raw = self.evaluate(source)
            self.assertIn("Laplace(", fields["function"], raw)

    def test_conservative_frequency_domain_rejects_boundary_and_left_half_plane(self):
        for operand, target in (("E1(t)", "0"), ("E1(t)", "-1/2"),
                                ("Ei(-t)", "0"), ("Ei(2*t)", "2"), ("Ei(2*t)", "1"),
                                ("gammainc_upper(2,t)", "0"), ("gammainc_Q(2,t)", "-1/2"),
                                ("BesselJ(1,t)", "-1"), ("BesselY(0,t)", "0")):
            with self.subTest(operand=operand, target=target):
                fields, raw = self.evaluate("{Laplace(" + operand + ",t,s) | s=" + target + "}")
                self.assertEqual(fields.get("value"), "NAN", raw)

    def test_uncertain_branches_and_nonintegrable_origins_stay_symbolic(self):
        operands = (
            "E1(-t)", "E1(i*t)", "Ei(i*t)", "E1(t+1)", "Ei(t^2)",
            "BesselJ(-3/2,t)", "BesselJ(1/2,-t)", "BesselJ(1,i*t)",
            "BesselJ(t,t)", "BesselY(1,t)", "BesselY(-1,t)", "BesselY(v,t)",
            "gammainc_lower(t,t)", "gammainc_upper(-1/2,t)", "gammainc_P(2,-t)",
            "gammainc_Q(2,t+1)", "BesselJ(1,c*t)", "Ei(c*t)",
            "E1((1+i)*t)", "gammainc_P(2,(1+i)*t)",
        )
        for operand in operands:
            with self.subTest(operand=operand):
                fields, raw = self.evaluate("Laplace(" + operand + ",t,s)")
                self.assertIn("Laplace(", fields["function"], raw)
        fields, raw = self.evaluate("{Laplace(Ei(c*t),t,s) | s=2,c=1}")
        self.assertIn("Laplace(", fields["function"], raw)


if __name__ == "__main__":
    unittest.main()
