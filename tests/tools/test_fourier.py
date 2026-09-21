"""Independent forward/inverse Fourier checks in the angular-frequency convention."""

import cmath
import math
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import mars_lab


class FourierTests(unittest.TestCase):
    def fields(self, source, wrt="ω", action="evaluate"):
        fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, wrt, action)
        self.assertEqual(code, 0, raw)
        return fields

    def assert_formula(self, source, expected, variable="ω", points=(0, 0.3, 1.25)):
        result = self.fields(source, variable)
        self.assertNotIn("Fourier(", result["function"], result)
        for point in points:
            with self.subTest(source=source, point=point):
                actual = self.fields("{" + source + " | " + variable + "=" + str(point) + "}", variable)
                self.assertAlmostEqual(float(actual["value"]), expected(point), places=12)

    def test_gaussians_both_directions(self):
        self.assert_formula("@F{exp(-t^2)}", lambda w: math.sqrt(math.pi)*math.exp(-w*w/4))
        self.assert_formula("@Finv{exp(-ω^2)}", lambda t: math.exp(-t*t/4)/(2*math.sqrt(math.pi)), "t")
        self.assert_formula("@F{exp(-2*t^2+3)}", lambda w: math.sqrt(math.pi/2)*math.exp(3-w*w/8))

    def test_bessel_indexed_parser_and_bindings(self):
        for indexed, canonical in (("J_n(x)", "bessel_j(n,x)"), ("J_{n+1}(x)", "bessel_j(n+1,x)"),
                                   ("J_3(x)", "bessel_j(3,x)"), ("J₃(x)", "bessel_j(3,x)"),
                                   ("J₋₃(x)", "bessel_j(-3,x)"), ("J_-3(x)", "bessel_j(-3,x)"),
                                   ("J3(x)", "bessel_j(3,x)"), ("Y_n(x)", "bessel_y(n,x)"),
                                   ("Y_{n+1}(x)", "bessel_y(n+1,x)"), ("Y₂(x)", "bessel_y(2,x)")):
            with self.subTest(indexed=indexed):
                self.assertEqual(self.fields(indexed, "x")["tex"], self.fields(canonical, "x")["tex"])
        fields = self.fields("@F{J_n(x)}", "k")
        self.assertNotIn("Fourier(", fields["function"])
        self.assertIn("const n", fields["function"])
        self.assertNotIn("const J", fields["function"])
        self.assertNotIn("x = ?", fields["function"])
        self.assertIn("k", fields["expression"])
        self.assertIn("ω", self.fields("@F(J_n(x),x,ω)")["expression"])
        for source in ("@F{J_n(x)}", "@Finv{J_n(ω)}"):
            result = self.fields(source)
            self.assertEqual(self.fields(result["expression"])["tex"], result["tex"])

    def test_bessel_fourier_integer_orders_both_directions(self):
        for order in (0, 1, 2, 3, -1, -3):
            for inverse in (False, True):
                operator, source, target = ("@Finv", "ω", "t") if inverse else ("@F", "x", "k")
                factor = (1j**order)/math.pi if inverse else 2*((-1j)**order)
                for coordinate in (-1.5, -0.7, 0.2, 0.6, 1.5):
                    expression = "{"+operator+"{J_n("+source+")} | "+target+"="+str(coordinate)+"; n="+str(order)+"}"
                    fields = self.fields(expression, target)
                    self.assertNotIn("Fourier(", fields["function"])
                    actual = complex(fields["value"].replace(" ", "").replace("i", "j"))
                    expected = (factor*math.cos(order*math.acos(coordinate))/math.sqrt(1-coordinate**2)
                                if abs(coordinate) < 1 else 0j)
                    self.assertLess(abs(actual-expected), 1e-11)
        for edge in (-1, 1):
            fields = self.fields("{@F{J_n(x)} | k="+str(edge)+"; n=3}", "k")
            self.assertTrue(math.isnan(float(fields["value"])))
            self.assertIn("singularities", fields["value_note"])
        for order in ("1/2", "i"):
            self.assertIn("Fourier(", self.fields("@F{J_{"+order+"}(x)}", "k")["function"])

    def test_bessel_specialised_order_simplifies_absolute_value(self):
        for inverse in (False, True):
            operator, source, target = ("@Finv", "ω", "t") if inverse else ("@F", "x", "k")
            for order in (3, -3):
                expression = f"{{{operator}{{J_n({source})}} | {target}=?; n={order}}}"
                result = self.fields(expression, target)
                self.assertIn("T_{3}", result["tex"])
                self.assertIn("Tn(3,", result["expression"])
                self.assertIn("chebyshev_t(3,", result["function"])
                self.assertNotIn("T_{\\left|", result["transform_identity_TeX"])
            symbolic = self.fields(operator+"{J_n("+source+")}", target)
            self.assertIn(r"T_{\left|n\right|}", symbolic["tex"])
        for literal, expected in (("3", "3"), ("-3", "3"), ("0", "0"), ("-0.75", "3/4"), ("-3/2", "3/2")):
            self.assertEqual(self.fields("abs("+literal+")")["tex"], self.fields(expected)["tex"])
        symbolic = self.fields("x+abs(a)", "x")
        self.assertIn(r"\left|a\right|", symbolic["tex"])

    def test_scaled_chebyshev_spectrum_inverse(self):
        forms = (
            "-2i*Tn(5,ω)*rect(ω/2)/sqrt(1-ω^2)",
            "-2i·Tn(5, ω)·rect(ω/2)/√(1 - ω^2)",
            "rect(ω/2)*(-2i*Tn(5,ω))/sqrt(1-ω^2)",
            "Tn(5,ω)*rect(ω/2)/(i*sqrt(1-ω^2)/2)",
        )
        for body in forms:
            with self.subTest(body=body):
                fields = self.fields("@Finv{"+body+"}", "t")
                self.assertNotIn("InverseFourier(", fields["function"])
                self.assertIn("besselj(5, t)", fields["function"])
                self.assertIn("J_{5}", fields["tex"])
                self.assertEqual(self.fields(fields["expression"], "t")["tex"], fields["tex"])
                for point in (-0.8, 0, 0.7):
                    expected = sum((-1)**k*(point/2)**(2*k+5)/(math.factorial(k)*math.factorial(k+5))
                                   for k in range(18))
                    value = self.fields("{@Finv{"+body+"} | t="+str(point)+"}", "t")["value"]
                    self.assertLess(abs(complex(value.replace(" ", "").replace("i", "j"))-expected), 1e-12)

    def test_scalar_factors_in_fourier_products_and_quotients(self):
        for inverse in (False, True):
            operator, source, target = ("@Finv", "ω", "t") if inverse else ("@F", "t", "ω")
            normalisation = 1/(2*math.pi) if inverse else 1
            direction = -1 if inverse else 1
            cases = (
                (f"a*Tn(2,{source})*rect({source}/2)/(b*sqrt(1-{source}^2))", "a=3+2i; b=4",
                 lambda p: -(3+2j)*math.pi/4*normalisation*sum(
                     (-1)**k*(p/2)**(2*k+2)/(math.factorial(k)*math.factorial(k+2)) for k in range(18))),
                (f"6/(2*(1+{source}^2))", "",
                 lambda p: 3*math.pi*normalisation*math.exp(-abs(p))),
                (f"3*exp(-2*{source})*step({source})", "",
                 lambda p: 3*normalisation/(2+direction*1j*p)),
            )
            for body, bindings, expected in cases:
                for point in (-0.7, 0, 0.6):
                    text = "{"+operator+"{"+body+"} | "+target+"="+str(point)
                    text += ("; "+bindings if bindings else "")+"}"
                    with self.subTest(source=text):
                        fields = self.fields(text, target)
                        self.assertNotIn("Fourier(", fields["function"])
                        actual = complex(fields["value"].replace(" ", "").replace("i", "j"))
                        self.assertLess(abs(actual-expected(point)), 1e-11)

    def test_bessel_affine_scaling(self):
        for inverse in (False, True):
            operator, source, target = ("@Finv", "ω", "t") if inverse else ("@F", "x", "k")
            for rate in (-2, 2):
                for frequency in (-0.8, 0.4, 3):
                    q = frequency/rate
                    factor = 1j/math.pi if inverse else -2j
                    expected = (factor*q/math.sqrt(1-q*q)/abs(rate)
                                *cmath.exp((-1j if inverse else 1j)*q) if abs(q)<1 else 0j)
                    expression = "{"+operator+"{J_1("+str(rate)+"*"+source+"+1)} | "+target+"="+str(frequency)+"}"
                    actual = complex(self.fields(expression, target)["value"].replace(" ", "").replace("i", "j"))
                    self.assertLess(abs(actual-expected), 1e-11)

    def test_bessel_spectrum_against_independent_inverse_quadrature(self):
        # k=cos(theta) removes the endpoint singularities before midpoint quadrature.
        count = 32
        argument = 0.8
        for order in (0, 1, 3, -2):
            total = 0j
            for index in range(count):
                theta = math.pi*(index+0.5)/count
                frequency = math.cos(theta)
                fields = self.fields("{@F{J_n(x)} | k="+repr(frequency)+"; n="+str(order)+"}", "k")
                spectrum = complex(fields["value"].replace(" ", "").replace("i", "j"))
                total += spectrum*cmath.exp(1j*argument*frequency)*math.sin(theta)/(2*count)
            degree = abs(order)
            expected = sum((-1)**j*(argument/2)**(2*j+degree)
                           /(math.factorial(j)*math.factorial(j+degree)) for j in range(20))
            if order < 0:
                expected *= (-1)**degree
            self.assertLess(abs(total-expected), 1e-11)

    def test_pulses_and_sinc(self):
        sinc = lambda x: math.sin(math.pi*x)/(math.pi*x) if x else 1
        self.assert_formula("@F{rect(t)}", lambda w: sinc(w/(2*math.pi)))
        self.assert_formula("@F{tri(t)}", lambda w: sinc(w/(2*math.pi))**2)
        self.assert_formula("@F{circ(t)}", lambda w: 2*sinc(w/math.pi))
        self.assert_formula("@Finv{rect(ω)}", lambda t: sinc(t/(2*math.pi))/(2*math.pi), "t")
        self.assert_formula("@F{sinc(t)}", lambda w: 1 if abs(w)<math.pi else 0, points=(0, 1, 4))
        self.assert_formula("@F{sinc(t)^2}", lambda w: max(1-abs(w)/(2*math.pi), 0), points=(0, 1, 8))
        self.assert_formula("@Finv{sinc(ω/(2*pi))^2}", lambda t: max(1-abs(t), 0), "t", points=(0, 0.5, 2))
        self.assert_formula("@F{sinc(2*t)^2}", lambda w: max(1-abs(w)/(4*math.pi), 0)/2)

    def test_two_sided_exponential_and_rational_pair(self):
        self.assert_formula("@F{exp(-abs(t))}", lambda w: 2/(1+w*w))
        self.assert_formula("@F{1/(t^2+1)}", lambda w: math.pi*math.exp(-abs(w)))
        self.assert_formula("@Finv{1/(ω^2+1)}", lambda t: math.exp(-abs(t))/2, "t")

    def test_hyperbolic_secant(self):
        self.assert_formula("@F{sech(t)}", lambda w: math.pi/math.cosh(math.pi*w/2))
        self.assert_formula("@Finv{sech(ω)}", lambda t: 1/(2*math.cosh(math.pi*t/2)), "t")

    def test_impulses_and_step_distribution(self):
        self.assertEqual(self.fields("@F{delta(t)}")["unbound"], "1")
        self.assert_formula("@Finv{delta(ω)}", lambda _: 1/(2*math.pi), "t")
        step = self.fields("@F{step(t)}")
        self.assertIn("principal_value", step["unbound"])
        self.assertIn("δ(ω)", step["unbound"])
        self.assertIn(r"\operatorname{PV}", step["tex"])
        self.assertIn("δ(ω)", self.fields("@F{1}")["unbound"])
        self.assertTrue(math.isnan(float(self.fields("delta(0)")["value"])))

    def test_log_absolute_spellings_and_cards(self):
        expected = self.fields("-@pi*(finite_part(1/abs(k))+2*@eulermascheroni*delta(k))", "k")
        for source in ("@F{ln|x|}", "@F{ln(|x|)}", "@F{ln(abs(x))}"):
            with self.subTest(source=source):
                result = self.fields(source, "k")
                self.assertEqual(result["tex"].split(r"\quad")[0], expected["tex"])
                self.assertNotIn("Fourier(", result["function"])
                self.assertIn("finite_part", result["expression"])
                self.assertIn(r"\operatorname{Fp}", result["tex"])
                self.assertIn("distribution", result["value_note"])
                self.assertEqual(self.fields(result["expression"], "k")["tex"], result["tex"])
                self.assertNotIn("const x", result["function"])
        for function in ("ln", "sin", "cos", "exp", "sqrt"):
            self.assertEqual(self.fields(function+"|x|", "x")["tex"],
                             self.fields(function+"(abs(x))", "x")["tex"])
        self.assertIn("Fourier(", self.fields("@F{ln(x)}", "k")["function"])

    def test_finite_part_fourier_pair_and_inverse(self):
        gamma = 0.5772156649015328606
        for alias in ("finite_part", "Fp"):
            self.assert_formula("@F{"+alias+"(1/abs(t))}", lambda w: -2*(math.log(abs(w))+gamma),
                                points=(-2, -0.5, 0.3, 1.25))
            self.assert_formula("@Finv{"+alias+"(1/abs(ω))}",
                                lambda t: -(math.log(abs(t))+gamma)/math.pi, "t", points=(-2, 0.3, 1.25))
        self.assert_formula("@Finv{-@pi*finite_part(1/abs(ω))-2*@pi*@eulermascheroni*delta(ω)}",
                            lambda t: math.log(abs(t)), "t", points=(-2, -0.5, 0.3, 1.25))
        inverse = self.fields("@Finv{ln(abs(ω))}", "t")
        self.assertEqual(inverse["tex"].split(r"\quad")[0],
                         r"-\frac{1}{2}\mkern-2mu \left(\operatorname{Fp}(\frac{1}{\left|t\right|})"
                         r" + 2\mkern-2mu \gamma\mkern-2mu \delta(t)\right)")
        for alias in ("finite_part", "Fp"):
            result = self.fields(alias+"(1/abs(x))", "x")
            self.assertIn("distribution", result["value_note"])
            derivative = self.fields(alias+"(1/abs(x))", "x", "derivative")
            self.assertIn("finite_part", derivative["function"])
            for point in (0, 1):
                value = self.fields("{"+alias+"(1/abs(x)) | x="+str(point)+"}", "x")
                self.assertTrue(math.isnan(float(value.get("value", "nan"))))

    def test_log_fourier_action_on_gaussian_test_functions(self):
        # Independent weak-transform check, not pointwise evaluation of a distribution.
        def integrate(function, start, stop):
            count = 8000
            step = (stop-start)/count
            total = function(start)+function(stop)
            for index in range(1, count):
                total += (4 if index % 2 else 2)*function(start+index*step)
            return total*step/3

        for scale in (1, 2, -3):
            result = self.fields("@F{ln(abs("+str(scale)+"*x))}", "k")
            inverse = self.fields("@Finv{ln(abs("+str(scale)+"*ω))}", "t")
            for width in (0.5, 1, 3):
                finite_part = 2*(integrate(lambda u: math.expm1(-width*math.exp(2*u)), -32, 0)
                                 +integrate(lambda u: math.exp(-width*math.exp(2*u)), 0, 6))
                action = result["unbound"].split(" where ")[0]
                action = action.replace("finite_part(1/|k|)", "("+repr(finite_part)+")").replace("δ(k)", "1")
                actual = float(self.fields(action, "k")["value"])
                expected = integrate(lambda u: 2*math.sqrt(math.pi/width)*(u+math.log(abs(scale)))
                                     *math.exp(u-math.exp(2*u)/(4*width)), -32, 6)
                self.assertAlmostEqual(actual, expected, places=8)
                inverse_action = inverse["unbound"].split(" where ")[0]
                inverse_action = inverse_action.replace("finite_part(1/|t|)", "("+repr(finite_part)+")")
                inverse_action = inverse_action.replace("δ(t)", "1")
                self.assertAlmostEqual(float(self.fields(inverse_action, "t")["value"]),
                                       expected/(2*math.pi), places=8)

    def test_log_affine_domains_and_finite_part_calculus(self):
        translated = self.fields("@F{ln(abs(x-2))}", "k")
        self.assertNotIn("Fourier(", translated["function"])
        self.assertIn("exp((-2i)k)", translated["unbound"])
        parameter = self.fields("@F{ln(abs(a*x+b))}", "k")
        self.assertNotIn("Fourier(", parameter["function"])
        self.assertIn("a", parameter["tex"])
        self.assertIn("b", parameter["tex"])
        for source in ("@F{ln(abs(i*x))}", "@F{finite_part(1/abs(x)^2)}"):
            self.assertIn("Fourier(", self.fields(source, "k")["function"])
        summed = self.fields("sum(n,1,2,finite_part(n/abs(x)))", "x")
        self.assertIn("finite_part", summed["function"])
        self.assertIn("distribution", summed["value_note"])
        primitive = self.fields("@S finite_part(1/abs(x)) dx", "x")
        self.assertIn("finite_part", primitive["function"])

    def test_arbitrary_functions_and_scope(self):
        plain = self.fields("@F{f(t)}")
        self.assertIn("Fourier(", plain["function"])
        self.assertIn("Fourier(", self.fields("@F{u(t)}")["function"])
        shifted = self.fields("@F{f(t-2)}")
        self.assertIn("exp((-2i)ω)", shifted["unbound"])
        self.assertNotIn("t = ?", shifted["function"])
        derivative = self.fields("@F{f'(t)}")
        self.assertIn("Fourier(", derivative["function"])
        self.assertNotIn("f(0)", derivative["unbound"])
        nested = self.fields("@F(@F(f(t),t,ω),t,x)")
        self.assertIn("δ(x)", nested["unbound"])
        self.assertNotIn("t = ?", nested["function"])

    def test_parser_aliases_and_explicit_mapping(self):
        expected = self.fields("@F{exp(-t^2)}")["tex"]
        for alias in ("@F", "ℱ", "Fourier"):
            for opening, closing in (("(", ")"), ("{", "}")):
                self.assertEqual(self.fields(alias+opening+"exp(-t^2)"+closing)["tex"], expected)
        self.assert_formula("@F(exp(-q^2),q,p)", lambda p: math.sqrt(math.pi)*math.exp(-p*p/4), "p")
        for alias in ("@Finv", "ℱ⁻¹", "InverseFourier"):
            self.assert_formula(alias+"(exp(-ω^2))", lambda t: math.exp(-t*t/4)/(2*math.sqrt(math.pi)), "t")
        for alias in ("delta", "δ", "@delta", "DiracDelta"):
            self.assertEqual(self.fields("@F{"+alias+"(t)}")["unbound"], "1")
        for alias in ("step", "heaviside", "Heaviside", "θ"):
            self.assertEqual(float(self.fields(alias+"(0)")["value"]), 0.5)
        for source in ("@F(exp(-q^2))", "@F(exp(-t^2),t,t)", "@F{ω*exp(-t^2)}",
                       "Derivative(-t,n)", "Derivative(t+1,n)"):
            fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, "ω", "evaluate")
            self.assertNotEqual(code, 0, raw)

    def test_parameters_are_not_free_variable_samples(self):
        source = "@F{exp(-a*t^2)}"
        symbolic = self.fields(source)
        self.assertIn("a", symbolic["unbound"])
        bound = self.fields("{"+source+" | ω=1; a=2}")
        self.assertAlmostEqual(float(bound["value"]), math.sqrt(math.pi/2)*math.exp(-1/8), places=12)
        self.assertNotIn("a = ?", bound["function"])
        self.assertIn("Fourier(", self.fields("@F{exp(t^2)}")["function"])

    def assert_symbolic_formula(self, source, expected, variable="ω"):
        result = self.fields(source, variable)
        self.assertNotIn("Fourier(", result["function"], result)
        for point in (-1.25, 0, 0.75):
            with self.subTest(source=source, point=point):
                check = "{abs(("+source+")-("+expected+")) | "+variable+"="+str(point)+"}"
                self.assertLess(float(self.fields(check, variable)["value"]), 1e-12)

    def test_affine_scaling_shifts_and_modulation(self):
        self.assert_symbolic_formula("@F{rect(-2*t+4)}", "exp(-2*i*ω)*sinc(ω/(4*pi))/2")
        self.assert_symbolic_formula("@F{exp(2*i*t)*rect(t)}", "sinc((ω-2)/(2*pi))")
        self.assert_symbolic_formula("@Finv{delta(2*ω-4)}", "exp(2*i*t)/(4*pi)", "t")
        self.assert_symbolic_formula("@F{delta(2*t-4)}", "exp(-2*i*ω)/2")

    def test_one_sided_exponentials(self):
        self.assert_symbolic_formula("@F{exp(-t)*step(t)}", "1/(1+i*ω)")
        self.assert_symbolic_formula("@F{exp(t)*step(-t)}", "1/(1-i*ω)")
        self.assert_symbolic_formula("@F{exp(-t)*step(t-2)}", "exp(-2*(1+i*ω))/(1+i*ω)")
        self.assert_symbolic_formula("@Finv{exp(-ω)*step(ω)}", "1/(2*pi*(1-i*t))", "t")
        self.assert_formula("@Finv{1/(1+i*ω)}", lambda t: math.exp(-t) if t>0 else 0.5 if t==0 else 0,
                            "t", points=(-1, 0, 1))
        self.assert_formula("@Finv{1/(1-i*ω)}", lambda t: math.exp(t) if t<0 else 0.5 if t==0 else 0,
                            "t", points=(-1, 0, 1))
        self.assert_formula("@F{1/(1+i*t)}", lambda w: 2*math.pi*math.exp(w) if w<0 else math.pi if w==0 else 0,
                            points=(-1, 0, 1))
        self.assert_formula("@Finv{1/(-1+i*ω)}", lambda t: -math.exp(t) if t<0 else -0.5 if t==0 else 0,
                            "t", points=(-1, 0, 1))

    def test_frequency_derivatives_and_product_association(self):
        self.assert_formula("@F{t^2*exp(-t^2)}", lambda w: math.sqrt(math.pi)*(2-w*w)*math.exp(-w*w/4)/4)
        self.assert_symbolic_formula("@F{exp(2*i*t)*t*exp(-t^2)}",
                                     "-i*sqrt(pi)*(ω-2)*exp(-(ω-2)^2/4)/2")
        for source in ("@F{t*f(t)}", "@Finv{ω*f(ω)}", "@F{t^3}", "@Finv{ω^3}",
                       "@F{exp(2*i*t)*t*f(t)}"):
            fields = self.fields(source)
            self.assertNotIn("_fourier_", fields["function"])
            self.assertNotIn("_fourier_", fields["tex"])
            self.assertIn("D", fields["unbound"])

    def test_round_trips_and_step_distribution_inverse(self):
        self.assert_formula("@Finv{@F{exp(-t^2)}}", lambda t: math.exp(-t*t), "t")
        self.assert_formula("@F{@Finv{exp(-ω^2)}}", lambda w: math.exp(-w*w))
        self.assert_formula("@Finv{pi*delta(ω)+PV(1/(i*ω))}",
                            lambda t: 0 if t<0 else 0.5 if t==0 else 1, "t", points=(-1, 0, 1))

    def test_conditioned_output_round_trips(self):
        for source in ("@F{exp(-a*t^2)}", "@F{rect(t)}", "@Finv{exp(-ω^2)}",
                       "@F{1+t^n+delta(t)}", "@Finv{ω^n}"):
            fields = self.fields(source)
            reparsed = self.fields(fields["expression"])
            self.assertEqual(reparsed["tex"], fields["tex"])
        for source in ("@Finv{sinc(ω/(2*pi))^2}", "@Finv{sinc(2*ω)^2}"):
            for point in (0, 0.5, 2):
                fields = self.fields("{"+source+" | t="+str(point)+"}")
                reparsed = self.fields(fields["expression"])
                self.assertAlmostEqual(float(reparsed["value"]), float(fields["value"]), places=12)

    def test_symbolic_polynomial_distributions(self):
        fields = self.fields("@F{1+t^n+delta(t)}")
        self.assertNotIn("Fourier(", fields["function"])
        self.assertIn("Derivative(δ(ω), n)", fields["expression"])
        self.assertIn("n ∈ ℤ≥0", fields["expression"])
        self.assertIn("delta(@omega)", fields["function"])
        self.assertIn("Derivative(delta(@omega), n)", fields["function"])
        self.assertIn(r"\delta(\omega)", fields["tex"])
        self.assertIn(r"\delta^{(n)}\left(\omega\right)", fields["tex"])
        inverse = self.fields("@Finv{ω^n}", "t")
        self.assertIn("(-i)^n·Derivative(δ(t), n)", inverse["unbound"])
        for source in ("@F{t^33}", "@Finv{ω^33}"):
            self.assertNotIn("Fourier(", self.fields(source)["function"])
        for order in ("-1", "1/2"):
            self.assertIn("Fourier(", self.fields("@F{t^("+order+")}")["function"])
        for order in (0, 1, 2, 3, 33):
            for operator, source, target, factor in (("@F", "t", "ω", "1"),
                                                     ("@Finv", "ω", "t", "1/(2*pi)")):
                sign = "i" if operator == "@F" else "-i"
                transform = operator+"{Derivative(delta("+source+"),"+str(order)+")}"
                self.assert_symbolic_formula(transform, factor+"*("+sign+"*"+target+")^"+str(order), target)
        for order in (0, 1, 2, 3):
            bound = self.fields("{@F{ordered_derivative(delta(t),n)} | n="+str(order)+"; ω=0}")
            self.assertEqual(float(bound["value"]), 1 if order == 0 else 0)

    def test_signal_greek_arguments_remain_function_calls(self):
        for name in ("delta", "step", "rect", "tri", "circ", "sinc", "PV"):
            for suffix in ("", "^2"):
                fields = self.fields(name+"(ω)"+suffix)
                self.assertIn("(\\omega)", fields["tex"])

    def test_gaussian_against_independent_quadrature(self):
        # Composite Simpson quadrature tests the kernel sign independently of the symbolic rule.
        subdivisions = 2048
        step = 16/subdivisions
        for inverse in (False, True):
            source = "@Finv{exp(-ω^2+2*ω)}" if inverse else "@F{exp(-t^2+2*t)}"
            variable = "t" if inverse else "ω"
            for target in (-1, 0.5, 2):
                total = 0j
                for index in range(subdivisions+1):
                    coordinate = -8+index*step
                    weight = 1 if index in (0, subdivisions) else 4 if index % 2 else 2
                    total += weight*cmath.exp(-coordinate**2+2*coordinate
                                              +(1j if inverse else -1j)*target*coordinate)
                expected = total*step/3/(2*math.pi if inverse else 1)
                literal = "("+repr(expected.real)+")+i*("+repr(expected.imag)+")"
                expression = "{abs(("+source+")-("+literal+")) | "+variable+"="+str(target)+"}"
                self.assertLess(float(self.fields(expression, variable)["value"]), 1e-11)


class ZZFourierReadmeExamples(unittest.TestCase):
    """README examples from docs/expression.md; run after ordinary suites."""

    def test_readme_logarithmic_fourier_examples(self):
        for source in ("@F{ln|x|}", "@F{ln(|x|)}", "@F{ln(abs(x))}"):
            fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, "k", "evaluate")
            self.assertEqual(code, 0, raw)
            self.assertEqual(fields["unbound"], "-π·(finite_part(1/|k|) + 2γ·δ(k)) where (k ∈ ℝ)")
        source = "@Finv{-@pi*finite_part(1/abs(ω))-2*@pi*@eulermascheroni*delta(ω)}"
        fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, "t", "evaluate")
        self.assertEqual(code, 0, raw)
        self.assertEqual(fields["unbound"], "ln(|t|) where (t ∈ ℝ)")

    def test_readme_bessel_fourier_examples(self):
        examples = (("{@F{J_n(x)} | k=0; n=0}", "2"),
                    ("{@F{J_n(x)} | k=2; n=3}", "0"),
                    ("{@F(J_n(x),x,ω) | ω=0; n=0}", "2"))
        for source, expected in examples:
            with self.subTest(source=source):
                fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, "k", "evaluate")
                self.assertEqual(code, 0, raw)
                self.assertEqual(fields["value"], expected)

    def test_readme_fourier_examples(self):
        examples = (
            ("@F{exp(-t^2)}", "√(π)·exp(-¼ω²) where (ω ∈ ℝ)"),
            ("@Finv{exp(-ω^2)}", "½·exp(-¼t²)/√(π) where (t ∈ ℝ)"),
            ("@F{rect(t)}", "sinc(ω/(2π)) where (ω ∈ ℝ)"),
            ("@F{delta(t)}", "1"),
            ("@F{step(t)}", "principal_value(1/(iω)) + π·δ(ω) where (ω ∈ ℝ)"),
            ("@F{1+t^n+delta(t)}", "2π·(δ(ω) + i^n·Derivative(δ(ω), n)) + 1 where (ω ∈ ℝ; n ∈ ℤ≥0)"),
            ("@F{f(t)}", "ℱ(f(t))"),
        )
        for source, expected in examples:
            with self.subTest(source=source):
                fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, "ω", "evaluate")
                self.assertEqual(code, 0, raw)
                self.assertEqual(fields["unbound"], expected)


if __name__ == "__main__":
    unittest.main()
