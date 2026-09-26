"""Hyperbolic Fourier powers: independent integration, branch choices and convergence."""

import cmath
import math
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import mars_lab


def fields(source, variable="ω"):
    result, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, variable, "evaluate")
    if code:
        raise AssertionError(raw)
    return result


def number(result):
    return complex(result["value"].replace(" ", "").replace("i", "j"))


def quadrature(exponent, frequency, scale=1, offset=0, inverse=False, sinh=True, branch=None):
    """Integrate the two half-lines independently after u=r^4 removes the sinh singularity."""
    count = 6000
    endpoint = 160**0.25
    step = endpoint/count
    kernel = 1j if inverse else -1j
    phase = cmath.exp(1j*math.pi*(exponent if branch is None else branch)) if sinh else 1

    def integrand(r):
        if r == 0:
            return (4*(1+phase)*cmath.exp(-kernel*frequency*offset/scale)/abs(scale)
                    if exponent == -0.75 and sinh else 0j)
        u = r**4
        magnitude = cmath.exp(exponent*math.log(math.sinh(u) if sinh else math.cosh(u)))
        positive = cmath.exp(kernel*frequency*(u-offset)/scale)
        negative = cmath.exp(kernel*frequency*(-u-offset)/scale)
        return 4*r**3*magnitude*(positive+phase*negative)/abs(scale)

    total = integrand(0)+integrand(endpoint)
    for index in range(1, count):
        total += (4 if index % 2 else 2)*integrand(index*step)
    return total*step/3/(2*math.pi if inverse else 1)


class HyperbolicFourierTests(unittest.TestCase):
    def spectrum(self, body, operator="@F", target="ω"):
        # Reparse the displayed algebra: nested transform cancellation must not hide a missing pair.
        return fields(operator+"{"+body+"}", target)["expression"].split(" | ")[0].removeprefix("{ ")

    def test_copied_beta_spectrum_recovers_shifted_sinh(self):
        spectrum = ("2^(-(n+1))*exp(b*ω*i/a)/abs(a)*(exp(i*@pi*n)*"
                    "B(-(n+ω*i/a)/2,n+1)+B((ω*i/a-n)/2,n+1))")
        result = fields("@Finv{"+spectrum+"}", "t")
        self.assertTrue(result["expression"].startswith("{ sinh(at + b)^n |"))
        self.assertNotIn("fourier(", result["function"])
        self.assertNotIn("beta(", result["function"])
        self.assertIn("Re(n + 1) > 0", result["expression"])
        self.assertIn("Re(-n) > 0", result["expression"])
        self.assertEqual(fields(result["expression"], "t")["tex"], result["tex"])

    def test_hyperbolic_beta_round_trips(self):
        for base, exponent in (("sinh", -0.5), ("abs(sinh", -0.5), ("cosech", 0.5),
                               ("cosh", -0.5), ("sech", 0.5)):
            symbolic = base+"(a*t+b)"+("" if base != "abs(sinh" else ")")+"^n"
            spectrum = self.spectrum(symbolic)
            result = fields("@Finv{"+spectrum+"}", "t")
            self.assertNotIn("fourier(", result["function"])
            for scale, offset, target in ((2, 0.3, 0.7), (-2, 0.3, 0.7), (-1, -0.2, -0.6)):
                with self.subTest(base=base, scale=scale, target=target):
                    bindings = f" | t={target}; a={scale}; b={offset}; n={exponent}"
                    restored = number(fields("{@Finv{"+spectrum+"}"+bindings+"}", "t"))
                    expected = number(fields("{"+symbolic+bindings+"}", "t"))
                    self.assertLess(abs(restored-expected), 1e-12)

    def test_beta_duality_and_reverse_round_trip(self):
        forward = self.spectrum("sinh(a*t+b)^n")
        dual = fields("{@F{"+forward+",ω,t} | t=0.7; a=-2; b=0.3; n=-1/2}", "t")
        expected = 2*math.pi*complex(math.sinh(1.7))**(-0.5)
        self.assertLess(abs(number(dual)-expected), 1e-12)
        inverse = self.spectrum("sinh(a*ω+b)^n", "@Finv", "t")
        symbolic = fields("@F{"+inverse+"}")
        self.assertTrue(symbolic["expression"].startswith("{ sinh(b + aω)^n |"))
        result = fields("{@F{"+inverse+"} | ω=0.7; a=2; b=0.3; n=-1/2}")
        self.assertLess(abs(number(result)-complex(math.sinh(1.7))**(-0.5)), 1e-12)

    def test_negative_imaginary_products_survive_expression_round_trip(self):
        for body in ("exp(x*(-i))", "exp(x*(-2*i))", "x*(-i)", "(x+1)*(-3*i)"):
            with self.subTest(body=body):
                original = fields("{"+body+" | x=0.7}", "x")
                reparsed = fields(original["expression"], "x")
                self.assertLess(abs(number(reparsed)-number(original)), 1e-14)

    def test_beta_round_trip_with_complex_power_and_swapped_arguments(self):
        spectrum = ("2^(-(n+1))/abs(a)*(B(n+1,(i*ω/a-n)/2)+"
                    "exp(i*@pi*n)*B(n+1,(-i*ω/a-n)/2))")
        for target in (-0.4, 0.4):
            result = fields("{@Finv{"+spectrum+f"}} | t={target}; a=-2; n=-1/4+i/5"+"}", "t")
            self.assertNotIn("fourier(", result["function"])
            expected = complex(math.sinh(-2*target))**(-0.25+0.2j)
            self.assertLess(abs(number(result)-expected), 1e-12)

    def test_beta_pair_does_not_accept_near_misses_or_invalid_domains(self):
        beta = "B((i*ω/a-n)/2,n+1)"
        reflected = "B((-i*ω/a-n)/2,n+1)"
        for body in (beta+"+2*exp(i*@pi*n)*"+reflected,
                     beta+"+exp(i*@pi*n)*B((-i*ω/a-n)/2,n+2)",
                     beta+"*"+reflected, "1/("+beta+"+exp(i*@pi*n)*"+reflected+")"):
            result = fields("@Finv{"+body+"}", "t")
            self.assertIn("fourier(", result["function"])
        spectrum = self.spectrum("sinh(a*t+b)^n")
        for bindings in ("a=1; b=0; n=2", "a=1; b=0; n=-2", "a=i; b=0; n=-1/2"):
            result = fields("{@Finv{"+spectrum+"} | t=?; "+bindings+"}", "t")
            self.assertIn("fourier(", result["function"])

    def test_beta_symbol_and_callable_aliases(self):
        for alias in ("beta", "B", "Β"):
            result = fields(alias+"(x,y)", "x")
            self.assertIn(r"\mathrm{B}", result["tex"])
            self.assertNotIn(r"\operatorname{beta}", result["tex"])
            self.assertIn("B(", result["expression"])
            self.assertIn("beta(", result["function"])
            self.assertEqual(fields(result["expression"], "x")["tex"], result["tex"])
            self.assertAlmostEqual(number(fields("{"+alias+"(x,y) | x=2; y=3}", "x")).real, 1/12)

    def test_requested_symbolic_power_and_conditions(self):
        result = fields("@F{sinh^n(at+b)}")
        self.assertNotIn("fourier(", result["function"])
        self.assertIn("beta(", result["function"])
        self.assertIn(r"\mathrm{B}", result["tex"])
        self.assertIn("n", result["tex"])
        self.assertIn("realpart", result["function"])
        self.assertIn("Principal powers", result["value_note"])
        self.assertIn("-1 < Re(n) < 0", result["value_note"])
        self.assertEqual(fields(result["expression"])["tex"], result["tex"])
        for source in ("@F{sinh(at+b)^n}", "@F{sinh(a*t+b)^n}"):
            self.assertEqual(fields(source)["tex"], result["tex"])

    def test_sinh_powers_against_independent_quadrature(self):
        for inverse in (False, True):
            operator, source, target = ("@Finv", "ω", "t") if inverse else ("@F", "t", "ω")
            for exponent in (-0.25, -0.5, -0.75):
                for scale, offset, frequency in ((1, 0, 0), (2, 0.3, 0.7), (-1, -0.2, -0.6)):
                    with self.subTest(inverse=inverse, exponent=exponent, scale=scale):
                        text = f"{{{operator}{{sinh(a*{source}+b)^n}} | {target}={frequency}; a={scale}; b={offset}; n={exponent}}}"
                        result = fields(text, target)
                        self.assertNotIn("fourier(", result["function"])
                        expected = quadrature(exponent, frequency, scale, offset, inverse)
                        self.assertLess(abs(number(result)-expected), 2e-9)

    def test_complex_exponent_in_convergence_strip(self):
        for inverse in (False, True):
            operator, source, target = ("@Finv", "ω", "t") if inverse else ("@F", "t", "ω")
            result = fields(f"{{{operator}{{sinh({source})^(-1/4+i/5)}} | {target}=0.6}}", target)
            expected = quadrature(-0.25+0.2j, 0.6, inverse=inverse)
            self.assertLess(abs(number(result)-expected), 2e-8)

    def test_reciprocals_and_absolute_powers_preserve_branches(self):
        for source, exponent, branch in (("1/sqrt(sinh(t))", -0.5, -0.5),
                                         ("sqrt(cosech(t))", -0.5, 0.5),
                                         ("abs(sinh(t))^(-1/2)", -0.5, 0),
                                         ("1/sinh(t)^(1/2)", -0.5, -0.5)):
            result = fields("{@F{"+source+"} | ω=0.4}")
            self.assertNotIn("fourier(", result["function"])
            self.assertLess(abs(number(result)-quadrature(exponent, 0.4, branch=branch)), 2e-9)

    def test_cosh_and_sech_powers(self):
        for inverse in (False, True):
            operator, source, target = ("@Finv", "ω", "t") if inverse else ("@F", "t", "ω")
            for body in (f"cosh(2*{source}+0.3)^(-1/2)", f"sech(2*{source}+0.3)^(1/2)"):
                result = fields("{"+operator+"{"+body+"} | "+target+"=-0.7}", target)
                self.assertNotIn("fourier(", result["function"])
                self.assertLess(abs(number(result)-quadrature(-0.5, -0.7, 2, 0.3, inverse, sinh=False)), 2e-9)

    def test_growth_and_singularities_are_not_unsupported_formula_notes(self):
        for body in ("sinh(2*t+1)^(1/2)", "abs(sinh(t))^(1/2)", "sinh(t)^(1+i)"):
            result = fields("@F{"+body+"}")
            self.assertIn("No ordinary Fourier transform", result["value_note"])
            self.assertIn("grows exponentially", result["value_note"])
            self.assertNotIn("fourier(", result["function"])
            self.assertEqual(result["value"], "NAN")
            self.assertIn("return NAN.", result["function"])
        for body in ("sinh(t)^(-2)", "cosech(t)^2"):
            result = fields("@F{"+body+"}")
            self.assertIn("non-integrable singularity", result["value_note"])
            self.assertIn("prescription", result["value_note"])
        for body in ("sinh(t)^(-1)", "1/sinh(t)", "cosech(2*t+1)"):
            result = fields("@F{"+body+"}")
            self.assertNotIn("fourier(", result["function"])
            self.assertIn("symmetric cancellation", result["value_note"])
        symbolic = fields("@F{sinh(a*t+b)^2}")
        self.assertIn("Extended Fourier transform", symbolic["value_note"])
        self.assertIn("analytic_delta(", symbolic["function"])
        boundary = fields("@F{sinh(t)^i}")
        self.assertNotIn("No ordinary Fourier transform", boundary["value_note"])
        complex_scale = fields("@F{sinh(i*t)^2}")
        self.assertNotIn("grows exponentially", complex_scale.get("value_note", ""))
        integer_root = fields("@F{isqrt(sinh(t))}")
        self.assertIn("fourier(", integer_root["function"])
        self.assertNotIn("beta(", integer_root["function"])

    def test_constant_cases_and_binding_specialisation(self):
        zero_order = fields("{@F{sinh(a*t+b)^n} | ω=?; n=0}")
        self.assertIn("δ(ω)", zero_order["expression"])
        self.assertNotIn("fourier(", zero_order["function"])
        constant = fields("@F{sinh(0*t+1)^(-1/2)}")
        self.assertIn("δ(ω)", constant["expression"])
        self.assertNotIn("fourier(", constant["function"])
        for exponent, fragment in ((2, "Extended Fourier transform"), (-2, "non-integrable singularity")):
            result = fields(f"{{@F{{sinh(a*t+b)^n}} | ω=?; a=1; b=0; n={exponent}}}")
            self.assertIn(fragment, result["value_note"])
        bound = fields("{@F{sinh(a*t+b)^n} | ω=?; a=-2; b=1; n=-1/2}")
        self.assertNotIn("fourier(", bound["function"])
        self.assertNotIn("const n", bound["function"])
        self.assertEqual(fields(bound["expression"])["tex"], bound["tex"])

    def test_proven_growth_is_rejected_in_both_directions(self):
        for operator in ("Fourier", "InverseFourier"):
            for body in ("sinh(x)^(1/2)", "abs(sinh(x))^(1/2)", "sinh(x)^(1+i)"):
                for binding in (False, True):
                    with self.subTest(operator=operator, body=body, binding=binding):
                        source = f"{operator}({body},x,k)"
                        if binding:
                            source = "{"+source+" | k=1}"
                        result = fields(source, "k")
                        self.assertEqual(result["value"], "NAN")
                        self.assertNotIn("fourier(", result["function"])
                        self.assertIn("grows exponentially", result["value_note"])
        for body in ("sinh(0*x)", "sinh(x)^0", "cosh(0*x+1)"):
            result = fields(f"Fourier({body},x,k)", "k")
            self.assertNotIn("grows exponentially", result.get("value_note", ""))
            self.assertNotIn("return @nan.", result["function"].split("else")[0])
        unknown = fields("@F{c*sinh(x)}", "k")
        self.assertIn("analytic_delta(", unknown["function"])
        self.assertEqual(number(fields("{Fourier(c*sinh(x),x,k) | k=1; c=0}", "k")), 0)
        cancelled = fields("{Fourier(sinh(x)-sinh(x),x,k) | k=1}", "k")
        self.assertEqual(number(cancelled), 0)


class ZZHyperbolicFourierReadmeExamples(unittest.TestCase):
    """README examples from docs/expression.md; run after ordinary tests."""

    def test_readme_hyperbolic_fourier_examples(self):
        result = fields("{@F{sinh(t)^(-1/2)} | ω=0}")
        self.assertLess(abs(number(result)-complex(3.708149354602744, -3.708149354602744)), 1e-14)
        growing = fields("@F{sinh(t)^2}")
        self.assertNotIn("fourier(", growing["function"])
        self.assertEqual(growing["value"], "NAN")
        self.assertIn("Extended Fourier transform", growing["value_note"])
        singular = fields("@F{sinh(t)^(-1)}")
        self.assertNotIn("fourier(", singular["function"])
        self.assertIn("tanh", singular["expression"])
        self.assertIn("symmetric cancellation", singular["value_note"])
        value = fields("{@F{sinh(t)^(-1)} | ω=0.4}")
        self.assertLess(abs(number(value)+1j*math.pi*math.tanh(math.pi*0.4/2)), 1e-12)

    def test_readme_inverse_beta_spectrum(self):
        source = "@Finv{(B(1/4+i*ω/2,1/2)-i*B(1/4-i*ω/2,1/2))/sqrt(2)}"
        result = fields(source, "t")
        self.assertNotIn("fourier(", result["function"])
        for t in (-0.7, 0.7):
            value = number(fields("{"+source+f" | t={t}"+"}", "t"))
            self.assertLess(abs(value-complex(math.sinh(t))**(-0.5)), 1e-12)


if __name__ == "__main__":
    unittest.main()
