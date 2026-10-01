"""Initial Ophelia scalar/equation runtime and MARS Lab RUN integration."""
import io
import json
import shutil
import subprocess
import sys
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import mars_lab
BINARY = ROOT / "build/release/scratch/ophelia"


class OpheliaTests(unittest.TestCase):
    def run_programme(self, source):
        result = subprocess.run([str(BINARY), "40"], input=source, text=True, capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        return result.stdout.strip()

    def test_complex_zero_output(self):
        source = """expression expr(@omega) {
    if (realpart(@omega) == @omega) {
        return -i.@pi.(delta(@omega - 1) - delta(@omega + 1)).
    } else { return @nan. }
}
@omega = 77.
output(expr(@omega)).
output(0i).
output(-0i).
output(i-i).
output(1e-80i).
"""
        lines = self.run_programme(source).splitlines()
        self.assertEqual(lines[:4], ["0"] * 4)
        self.assertIn("i", lines[4])
        self.assertNotIn(lines[4], ("0", "0i"))

    def test_generated_scalar(self):
        source = """expression expr(x, y) {
    v1 = x^2 + y^2.
    return sqrt(v1).
}
x = 3.
y = 4.
output(expr(x, y)).
"""
        self.assertEqual(float(self.run_programme(source)), 5)

    def test_transform_cards_execute_original_operations(self):
        cases = (
            ("@Linv{5/(s*(s+4)+5)*(s+10/s^2+3)}", "inverselaplace", "t", "s"),
            ("{@Linv{a/(s+a)} | t=0; a=2}", "inverselaplace", "t", "s"),
            ("{@L{t} | s=2}", "laplace", "s", "t"),
            ("{@F{exp(-x^2)} | k=1}", "fourier", "k", "x"),
            ("{@Finv{exp(-k^2)} | x=1}", "inversefourier", "x", "k"),
        )
        for source, operation, target, dummy in cases:
            with self.subTest(source=source):
                fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, target, "evaluate")
                self.assertEqual(code, 0, raw)
                result_programme = fields["function"]
                expression = fields["expression"]
                prepared = mars_lab.prepare_evaluation_fields(
                    mars_lab.DEFAULT_BIN, fields, source, 40, False, target, "evaluate")
                programme = prepared["full_display_function"]
                self.assertIn("return " + operation + "(", programme)
                self.assertIn(", " + dummy + ", " + target + ")", programme)
                self.assertNotIn(dummy + " = ?.", programme)
                self.assertEqual(mars_lab.expression_for_display(fields["expression"]),
                                 mars_lab.expression_for_display(expression))
                actual = self.run_programme(programme)
                if operation in ("fourier", "inversefourier"):
                    self.assertAlmostEqual(float(actual), float(fields["value"]), places=14)
                else:
                    expected = self.run_programme(result_programme)
                    self.assertEqual(actual, expected)

    def test_ordinary_card_does_not_get_an_operation_programme(self):
        fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, "{x^2 | x=3}", 40)
        self.assertEqual(code, 0, raw)
        self.assertNotIn("operation_function", fields)

    def test_nested_calculus_preserves_authored_operations(self):
        cases = (
            ("@L{@S^t sin(x) dx}", "laplace(integral(sin(x), x, t), t, s)", r"\int", "-½"),
            ("@L{@S_0^t sin(x) dx}", "laplace(integral(sin(x), x, 0, t), t, s)", r"\int", "½"),
            ("@L{Dt(sin(t))}", "laplace(derivative(sin(t), t, 1), t, s)", r"\frac", "½"),
        )
        for source, call, operator, value in cases:
            for binding in ("", " | ; s=?", " | s=1"):
                request = "{ " + source + binding + " }"
                with self.subTest(source=request):
                    fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, request, 40, "s")
                    self.assertEqual(code, 0, raw)
                    programme = fields["operation_function"]
                    self.assertIn("return " + call + ".", programme)
                    self.assertIn("if (realpart(s) > 0)", programme)
                    self.assertIn("return @nan.", programme)
                    self.assertNotIn("x =", programme)
                    self.assertNotIn("t =", programme)
                    identity = fields["transform_identity_TeX"]
                    lhs = identity.split(" = ", 1)[0]
                    self.assertIn(r"\mathcal{L}_{t\to s}", lhs)
                    self.assertIn(operator, lhs)
                    self.assertIn(r"\sin", lhs)
                    self.assertNotIn("NAN", identity)
                    self.assertNotIn(r"\cos", lhs)
                    actual = self.run_programme(programme)
                    if "s=1" in binding:
                        self.assertEqual(actual, value)
                    else:
                        self.assertIn("Re(s) > 0", actual)
                        self.assertNotIn("C", actual)

    def test_laplace_of_primitive_with_logarithmic_singularity(self):
        for binding in ("?", "1", "1+i", "1-i", "0", "-1"):
            source = "{ @L{@S^t 1/(1-x^2) dx} | s=" + binding + " }"
            with self.subTest(binding=binding):
                fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, "s")
                self.assertEqual(code, 0, raw)
                self.assertNotIn("laplace(", fields["function"])
                programme = fields["operation_function"]
                self.assertIn("laplace(integral(1/(1 - x^2), x, t), t, s)", programme)
                self.assertIn("if (realpart(s) > 0)", programme)
                identity = fields["transform_identity_TeX"]
                self.assertIn(r"\int", identity.split(" = ", 1)[0])
                self.assertNotIn("NAN", identity)
                actual = self.run_programme(programme)
                if binding == "?":
                    self.assertIn("Re(s) > 0", actual)
                    self.assertNotIn("ℒ", actual)
                else:
                    self.assertEqual(actual, self.run_programme(fields["function"]))

    def test_generated_rational_factors_are_executable(self):
        for coefficient in ("1/4", "-1/4", "2/7", "-5/13"):
            source = "{ ("+coefficient+")*(sin(x)+cos(x)) | x=1 }"
            with self.subTest(coefficient=coefficient):
                fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, "x")
                self.assertEqual(code, 0, raw)
                self.assertAlmostEqual(float(self.run_programme(fields["function"])),
                                       float(fields["value"]), places=14)

    def test_negative_difference_numerator(self):
        for precision in (32, 40):
            for binding in ("?", "2", "0"):
                source = "{ Ds(@L{@S^t sin(x) dx}) | s=" + binding + " }"
                with self.subTest(precision=precision, binding=binding):
                    fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, precision, "s")
                    self.assertEqual(code, 0, raw)
                    self.assertIn(r"\frac{s^{2} - 1}{\left(s^{2} + 1\right)^{2}}", fields["tex"])
                    self.assertIn("(s² - 1)/(s² + 1)²", fields["expression"])
                    self.assertNotIn("1 - s²", fields["expression"])
                    programme = fields["operation_function"]
                    self.assertIn("derivative(laplace(integral(sin(x), x, t), t, s), s, 1)", programme)
                    self.assertIn("if (realpart(s) > 0)", programme)
                    actual = self.run_programme(programme)
                    if binding == "?":
                        self.assertIn("(s² - 1)/(s² + 1)²", actual)
                        self.assertNotIn("1 - s²", actual)
                        self.assertIn("Re(s) > 0", actual)
                    elif binding == "0":
                        self.assertEqual(actual, "NAN")
                    else:
                        self.assertAlmostEqual(float(actual), 3 / 25, places=14)

    def test_negative_difference_factors_are_general(self):
        cases = (
            ("-(1-x^2)/(x^2+1)^2", "x² - 1", "x=2", "3/25"),
            ("-2*(x-y)/(z^2+1)", "y - x", "x=2; y=5; z=1", "3"),
            ("-(x-y)*(z-w)/(x^2+1)", "y - x", "x=2; y=5; z=3; w=1", "6/5"),
            ("-(x+y)/(z^2+1)", "x + y", "x=2; y=5; z=1", "-7/2"),
            ("-1/(x-y)", "x - y", "x=2; y=5", "1/3"),
        )
        for source, difference, bindings, expected in cases:
            with self.subTest(source=source):
                fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, "x")
                self.assertEqual(code, 0, raw)
                self.assertIn(difference, fields["expression"])
                self.assertIn(difference, self.run_programme(fields["function"]))
                bound, raw, code = mars_lab.run_mars_lab_fields(
                    mars_lab.DEFAULT_BIN, "{ " + source + " | " + bindings + " }", 40, "x")
                self.assertEqual(code, 0, raw)
                self.assertEqual(self.run_programme(bound["function"]), self.run_programme("output(" + expected + ")."))

    def test_derivatives_evaluate_known_integral_transforms(self):
        cases = (
            ("Ds(@L(t,t,s))", "s", "2", "-1/4", "laplace(t, t, s)"),
            ("Ds(@L{t})", "s", "2", "-1/4", "laplace(t, t, s)"),
            ("Ds(Ds(@L(t,t,s)))", "s", "2", "3/8", "laplace(t, t, s)"),
            ("Dx(Ds(@L(x*t,t,s)))", "s", "2", "-1/4", "laplace(t.x, t, s)"),
            ("Dt(@Linv(1/s^2,s,t))", "t", "2", "1", "inverselaplace(1/s^2, s, t)"),
            ("Dt(@Linv{1/s^2})", "t", "2", "1", "inverselaplace(1/s^2, s, t)"),
            ("Dk(@F(exp(-x^2),x,k))", "k", "1", "-sqrt(pi)*exp(-1/4)/2", "fourier(exp(-(x^2)), x, k)"),
            ("Dk(@F{exp(-x^2)})", "k", "1", "-sqrt(pi)*exp(-1/4)/2", "fourier(exp(-(x^2)), x, k)"),
            ("Dx(@Finv(exp(-k^2),k,x))", "x", "1", "-exp(-1/4)/(4*sqrt(pi))", "inversefourier(exp(-(k^2)), k, x)"),
            ("Dx(@Finv{exp(-k^2)})", "x", "1", "-exp(-1/4)/(4*sqrt(pi))", "inversefourier(exp(-(k^2)), k, x)"),
        )
        for source, variable, binding, expected, call in cases:
            with self.subTest(source=source):
                fields, raw, code = mars_lab.run_mars_lab_fields(
                    mars_lab.DEFAULT_BIN, "{ "+source+" | "+variable+"="+binding+" }", 40, variable)
                self.assertEqual(code, 0, raw)
                self.assertNotIn("derivative(", fields["function"])
                programme = fields["operation_function"]
                self.assertIn(call, programme)
                self.assertIn("derivative(", programme)
                reference, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, expected, 40)
                self.assertEqual(code, 0, raw)
                expected_value = complex(reference["value"].replace(" ", "").replace("i", "j"))
                actual = self.run_programme(programme)
                # Parse exact vulgar fractions through MARS, not Python's floating-point parser.
                result, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, actual, 40)
                self.assertEqual(code, 0, raw)
                self.assertAlmostEqual(complex(result["value"].replace(" ", "").replace("i", "j")),
                                       expected_value, places=13)

    def test_derivative_of_nested_logarithmic_primitive_transform(self):
        source = "Ds(@L{@S^t 1/(1-x^2) dx})"
        for binding in ("?", "1", "pi", "1+i", "0", "-1"):
            with self.subTest(binding=binding):
                fields, raw, code = mars_lab.run_mars_lab_fields(
                    mars_lab.DEFAULT_BIN, "{ "+source+" | s="+binding+" }", 32, "s")
                self.assertEqual(code, 0, raw)
                self.assertNotIn("derivative(", fields["function"])
                self.assertIn("Re(s) > 0", fields["expression"])
                programme = fields["operation_function"]
                self.assertIn("derivative(laplace(integral(1/(1 - x^2), x, t), t, s), s, 1)", programme)
                self.assertIn("if (realpart(s) > 0)", programme)
                actual = self.run_programme(programme)
                if binding == "?":
                    self.assertNotIn("Ds(", actual)
                    self.assertIn("Re(s) > 0", actual)
                elif binding in ("0", "-1"):
                    self.assertEqual(actual, "NAN")
                else:
                    reference = self.run_programme(fields["function"])
                    self.assertAlmostEqual(complex(actual.replace(" ", "").replace("i", "j")),
                                           complex(reference.replace(" ", "").replace("i", "j")), places=13)

    def test_unknown_transform_derivatives_remain_formal(self):
        for source, variable in (("Ds(@L(f(t),t,s))", "s"), ("Dt(@Linv(F(s),s,t))", "t"),
                                 ("Dk(@F(f(x),x,k))", "k"), ("Dx(@Finv(F(k),k,x))", "x"),
                                 ("Ds(@L(t*f(t),t,s))", "s"), ("Ds(@L(exp(-t)*f(t),t,s))", "s")):
            with self.subTest(source=source):
                fields, raw, code = mars_lab.run_mars_lab_fields(
                    mars_lab.DEFAULT_BIN, "{ "+source+" | "+variable+"=? }", 32, variable)
                self.assertEqual(code, 0, raw)
                self.assertIn("derivative(", fields["function"])
                self.assertIn("derivative(", fields["operation_function"])
                self.run_programme(fields["operation_function"])

    def test_laplace_of_tangent_primitive(self):
        for binding in ("?", "pi", "1+i", "0", "-1"):
            source = "{ @L{@S^t tan(x) dx} | s=" + binding + " }"
            with self.subTest(binding=binding):
                fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, "s")
                self.assertEqual(code, 0, raw)
                self.assertNotIn("laplace(", fields["function"])
                programme = fields["operation_function"]
                self.assertIn("laplace(integral(tan(x), x, t), t, s)", programme)
                self.assertIn("if (realpart(s) > 0)", programme)
                lhs = fields["transform_identity_TeX"].split(" = ", 1)[0]
                self.assertIn(r"\int", lhs)
                self.assertIn(r"\tan", lhs)
                actual = self.run_programme(programme)
                if binding == "?":
                    self.assertIn("Re(s) > 0", actual)
                    self.assertNotIn("ℒ", actual)
                else:
                    self.assertEqual(actual, self.run_programme(fields["function"]))
                if binding == "pi":
                    self.assertIn("s = @pi.", programme)
                    self.assertAlmostEqual(complex(actual.replace(" ", "").replace("i", "j")),
                                           0.0390230861269195 - 0.00719151138794376j, places=14)

    def test_unresolved_transform_keeps_authored_calculus_in_TeX(self):
        for source, operation in (("@L{@S^t ln(1-x) dx}", r"\int"),
                                  ("{ @L{@S^t tan(x+1) dx} | s=pi }", r"\int"),
                                  ("{ @L{@S^t ln(1-x) dx} | ; s=2 }", r"\int"),
                                  ("@L{Dt(exp(t^2))}", r"\frac")):
            with self.subTest(source=source):
                fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40)
                self.assertEqual(code, 0, raw)
                self.assertEqual(fields["transform_identity_TeX"], "")
                self.assertIn(operation, fields["tex"])
                self.assertIn(r"\mathcal{L}_{t\to s}", fields["tex"])
                self.assertNotIn("NAN", fields["tex"])

    def test_nested_transforms_remain_calls_in_source_programme(self):
        fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, "@Linv{@L{sin(t)}}", 40)
        self.assertEqual(code, 0, raw)
        self.assertIn("return inverselaplace(laplace(sin(t), t, s), s, t).", fields["operation_function"])

    def test_operation_programme_retains_domain_for_every_binding(self):
        for binding, expected in (("?", None), ("1", "-½"), ("0", "NAN"), ("-1", "NAN"), ("i", "NAN")):
            source = "{ @L{@S^t sin(x) dx} | s=" + binding + " }"
            with self.subTest(binding=binding):
                fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, "s")
                self.assertEqual(code, 0, raw)
                programme = fields["operation_function"]
                self.assertEqual(programme.count("if (realpart(s) > 0)"), 1)
                self.assertIn("return laplace(integral(sin(x), x, t), t, s).", programme)
                self.assertIn("return @nan.", programme)
                output = self.run_programme(programme)
                if expected is None:
                    self.assertEqual(output.count("Re(s) > 0"), 1)
                    self.assertIn("-s/(s² + 1)", output)
                else:
                    self.assertEqual(output, expected)

    def test_operation_programme_retains_all_domain_predicates(self):
        cases = (
            ("@L{t^a}", "if (realpart(s) > 0 && realpart(a) > -1)", "laplace(t^a, t, s)"),
            ("@F(1/sqrt(abs(t)),t,k)", "if (realpart(k) == k && k != 0)",
             "fourier(1/sqrt(abs(t)), t, k)"),
        )
        for source, guard, call in cases:
            with self.subTest(source=source):
                fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40)
                self.assertEqual(code, 0, raw)
                programme = fields["operation_function"]
                self.assertIn(guard, programme)
                self.assertIn("return " + call + ".", programme)
                self.assertIn("return @nan.", programme)
                output = self.run_programme(programme)
                self.assertNotEqual(output, "NAN")
                if source.startswith("@F"):
                    self.assertIn("k ≠ 0", output)
                    self.assertEqual(self.run_programme(programme.replace("k = ?.", "k = 0.")), "NAN")

    def test_integral_family_conditions_follow_bindings(self):
        cases = (
            ("@L{-cos(t)}", "s", "integral", "Re(s) > 0"),
            ("@S (-s/(s^2+1) where (Re(s)>0)) ds", "s", "evaluate", "Re(s) > 0"),
            ("@S (q where (Re(q)>0; q != 0)) dq", "q", "evaluate", "Re(q) > 0"),
        )
        for source, variable, action, condition in cases:
            with self.subTest(source=source):
                fields, raw, code = mars_lab.run_mars_lab_fields(
                    mars_lab.DEFAULT_BIN, source, 40, variable, action)
                self.assertEqual(code, 0, raw)
                expression = fields["integral"].split(" = ", 1)[1] if action == "integral" else fields["expression"]
                body, bindings = expression.split(" | ", 1)
                self.assertNotIn("where", body)
                self.assertNotIn(condition, body)
                self.assertIn("C", body)
                self.assertIn("C = ", bindings)
                self.assertIn("; " + condition, bindings)
                programme = fields["integral_function" if action == "integral" else "operation_function"]
                self.assertIn("if (realpart(" + variable + ") > 0", programme)
                output = self.run_programme(programme)
                self.assertNotIn("where", output.split(" | ", 1)[0])
                self.assertIn("; " + condition, output)
                for value in ("0", "-1"):
                    invalid = programme.replace(variable + " = ?.", variable + " = " + value + ".")
                    invalid = invalid.replace("const C = ?.", "const C = 7.")
                    self.assertEqual(self.run_programme(invalid), "NAN")

    def test_function_integral_family_preserves_domain_and_bindings(self):
        for value in ("?", "1", "0", "-1"):
            source = ("s = " + value + ".\nconst C = 2.\n"
                      "output(integral(-s/(s^2+1) where (Re(s)>0),s)).")
            with self.subTest(value=value):
                output = self.run_programme(source)
                if value == "?":
                    body, bindings = output.split(" | ", 1)
                    self.assertNotIn("where", body)
                    self.assertIn("4 - ln(s² + 1)", body)
                    self.assertIn("; Re(s) > 0", bindings)
                elif value == "1":
                    self.assertEqual(output, self.run_programme("output(2-ln(2)/2)."))
                else:
                    # Direct output falls back to the restricted expression when no number exists.
                    self.assertIn("s = " + value, output)
                    self.assertIn("; Re(s) > 0", output)
                    self.assertNotIn("where", output)

    def test_integral_expression_first_bounds(self):
        cases = (
            ("output(integral(x, x, 1, 3)).", "4"),
            ("output(integral(x, x, 3, 1)).", "-4"),
            ("output(integral(x, x, 3, 3)).", "0"),
            ("a = 1, b = 3. output(integral(x, x, a, b)).", "4"),
            ("x = 99. output(integral(x, x, 1, 3)).", "4"),
            ("output(integral(integral(x.y, x, 0, 2), y, 0, 3)).", "9"),
        )
        for source, expected in cases:
            with self.subTest(source=source):
                self.assertEqual(self.run_programme(source), expected)

    def test_calculus_cards_preserve_supplied_bindings(self):
        cases = (
            ("{ @S sin(x) dx | x=pi; C=0 }", "evaluate", "full_display_function", "1"),
            ("{ @S sin(x) dx | C=0; x=pi }", "evaluate", "full_display_function", "1"),
            ("{ @S sin(x) dx | x=0; C=7 }", "evaluate", "full_display_function", "6"),
            ("{ @S (a.x + sin(x)) dx | x=0; a=2, C=7 }", "evaluate", "full_display_function", "6"),
            ("{ @S_0^2 x dx | x=99 }", "evaluate", "full_display_function", "2"),
            ("{ Dx(sin(x)) | x=0 }", "evaluate", "full_display_function", "1"),
            ("{ sin(x) | x=0 }", "derivative", "full_display_derivative_function", "1"),
        )
        for source, action, key, expected in cases:
            with self.subTest(source=source):
                fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, "x", action)
                self.assertEqual(code, 0, raw)
                prepared = mars_lab.prepare_evaluation_fields(mars_lab.DEFAULT_BIN, fields, source, 40, False,
                                                             "x", action)
                programme = prepared[key]
                self.assertEqual(self.run_programme(programme), expected)
                if "x=pi" in source:
                    self.assertIn("x = @pi.", programme)
                    self.assertIn("const C = 0.", programme)
                    self.assertIn("return integral(sin(x), x).", programme)
                    self.assertIn("cos", prepared["full_display_TeX"])
                    self.assertEqual(prepared["value"], "1")

    def test_indefinite_integral_partial_bindings_remain_symbolic(self):
        source = "{ @S sin(x) dx | x=?; C=7 }"
        fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40)
        self.assertEqual(code, 0, raw)
        programme = fields["operation_function"]
        self.assertIn("x = ?.", programme)
        self.assertIn("const C = 7.", programme)
        output = self.run_programme(programme)
        self.assertIn("cos(x)", output)
        self.assertIn("C = 7", output)

    def test_integral_rejects_nonvariable_second_argument(self):
        result = subprocess.run([str(BINARY), "40"], input="output(integral(sin(x), 0, x, @pi)).",
                                text=True, capture_output=True, timeout=10)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("second argument must be an integration variable", result.stderr)

    def test_upper_only_integral_does_not_generate_a_constant(self):
        for source in ("@S^z sin(x) dx", "@S^x sin(x) dx", "integral(sin(x), x, x)"):
            with self.subTest(source=source):
                fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40)
                self.assertEqual(code, 0, raw)
                programme = fields["operation_function"]
                coordinate = "z" if "^z" in source else "x"
                self.assertIn("integral(sin(x), x, " + coordinate + ")", programme)
                self.assertNotIn("const C", programme)
                actual = self.run_programme(programme)
                self.assertIn("-cos(" + coordinate + ")", actual)
                self.assertNotIn("C", actual)
        cases = (
            ("output(integral(sin(x), x, 0)).", "-1"),
            ("const C = 7. output(integral(sin(x), x, 0)).", "-1"),
            ("x = @pi. output(integral(sin(x), x, x)).", "1"),
            ("x = 99. z = 0. output(integral(sin(x), x, z)).", "-1"),
            ("output(integral(sin(x), x, 0, 0)).", "0"),
        )
        for source, expected in cases:
            with self.subTest(source=source):
                self.assertEqual(self.run_programme(source), expected)
        unresolved = self.run_programme("output(integral(exp(cosh(x)), x, z)).")
        self.assertNotIn("C", unresolved)

    def test_calculus_cards_execute_the_requested_operation(self):
        cases = (
            ("@S sin(x) dx", "evaluate", "operation_function", "integral(sin(x), x)", "cos"),
            ("Dx(sin(x))", "evaluate", "operation_function", "derivative(sin(x), x, 1)", "cos"),
            ("Dxx(sin(x))", "evaluate", "operation_function", "derivative(sin(x), x, 2)", "sin"),
            ("sin(x)", "derivative", "derivative_function", "derivative(sin(x), x, 1)", "cos"),
            ("sin(x)", "integral", "integral_function", "integral(sin(x), x)", "cos"),
            ("@S_0^@pi sin(x) dx", "evaluate", "operation_function", "integral(sin(x), x, 0, @pi)", None),
        )
        for source, action, field, call, result_function in cases:
            with self.subTest(source=source, action=action):
                fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, "x", action)
                self.assertEqual(code, 0, raw)
                programme = fields[field]
                self.assertIn("return " + call + ".", programme)
                self.assertNotIn(") + C.", programme)
                actual = self.run_programme(programme)
                if result_function:
                    self.assertIn(result_function + "(x)", actual)
                    self.assertNotIn("derivative(", actual)
                    self.assertNotIn("∫", actual)
                    if call.startswith("integral"):
                        self.assertIn("C", actual)
                else:
                    self.assertEqual(actual, "2")

    def test_indefinite_integral_generates_its_own_constant(self):
        symbolic = self.run_programme("output(integral(sin(x), x)).")
        self.assertIn("C - cos(x)", symbolic)
        self.assertEqual(self.run_programme("const C = 7. x = 0. output(integral(sin(x), x))."), "6")
        self.assertEqual(self.run_programme("const C = 7. output(integral(sin(x), x, 0))."), "-1")
        self.assertEqual(self.run_programme("output(integral(sin(x), x, 0, @pi))."), "2")
        unresolved = self.run_programme("output(integral(exp(cosh(x)), x)).")
        self.assertIn("∫", unresolved)
        self.assertIn("C", unresolved)

    def test_derivative_call_evaluates_symbolically_before_substituting_values(self):
        self.assertEqual(self.run_programme("x = 0. output(derivative(sin(x), x, 1))."), "1")
        self.assertEqual(self.run_programme("x = 3. output(derivative(x^3, x, 2))."), "18")
        self.assertEqual(self.run_programme("output(derivative(5, x, 1))."), "0")
        self.assertIn("cos(x)", self.run_programme("outputa(derivative(sin(x), x, 1))."))

    def test_domain_keeps_strongest_translated_half_plane(self):
        cases = (
            ("Re(s)>0; Re(s+2)>0", "Re(s) > 0"),
            ("Re(s+2)>0; Re(s)>0", "Re(s) > 0"),
            ("Re(s)>0; Re(2+s)>0", "Re(s) > 0"),
            ("Re(s)>0; Re(s-2)>0", "Re(s - 2) > 0"),
            ("Re(s)>0; Re(s)>2", "Re(s) > 2"),
            ("Re(s)>2; Re(s)>0", "Re(s) > 2"),
            ("Re(s)>0; Re(s+2i)>0", "Re(s) > 0"),
            ("Re(s)>Re(a); Re(s+2)>Re(a)", "Re(s) > Re(a)"),
        )
        for conditions, expected in cases:
            with self.subTest(conditions=conditions):
                fields, raw, code = mars_lab.run_mars_lab_fields(
                    mars_lab.DEFAULT_BIN, "1 where (" + conditions + ")", 40, "s", "evaluate")
                self.assertEqual(code, 0, raw)
                self.assertEqual(fields["unbound"], "1 where (" + expected + ")")

    def test_domain_retains_unproved_restrictions(self):
        for conditions in ("Re(s)>0; Re(s+a)>0", "Re(s)>0; Re(-s)>0",
                           "Re(s)>0; Re(z)>0", "Re(s)>Re(a); Re(s)>Re(b)"):
            with self.subTest(conditions=conditions):
                fields, raw, code = mars_lab.run_mars_lab_fields(
                    mars_lab.DEFAULT_BIN, "1 where (" + conditions + ")", 40, "s", "evaluate")
                self.assertEqual(code, 0, raw)
                self.assertIn("; ", fields["unbound"])

    def test_domain_implication_preserves_strict_boundary(self):
        for point, expected in (("1+i", "1"), ("0", "NAN"), ("-1", "NAN")):
            source = "{1 where (Re(s+2)>0; Re(s)>0) | s=" + point + "}"
            fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, "s", "evaluate")
            self.assertEqual(code, 0, raw)
            self.assertEqual(fields["value"], expected)

    def test_laplace_cards_and_run_share_minimal_domain(self):
        source = "@L{10t+exp(-2t)*(13*cos(t)+11*sin(t))-8}"
        fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, "s", "evaluate")
        self.assertEqual(code, 0, raw)
        self.assertEqual(fields["expression"].count("Re("), 1)
        self.assertIn("Re(s) > 0", fields["expression"])
        self.assertEqual(fields["tex"].count(r"\operatorname{Re}"), 1)
        self.assertEqual(fields["transform_identity_TeX"].count(r"\operatorname{Re}"), 1)
        self.assertEqual(fields["function"].count("realpart("), 1)
        output = self.run_programme(fields["operation_function"])
        self.assertEqual(output.count("Re("), 1)
        self.assertIn("Re(s) > 0", output)

    def test_equation_returns_and_constants(self):
        lines = self.run_programme("""equation equ(x, const a) {
    v1 = x^2.
    return equation(v1 = a).
}
const a = 4.
output(solve(equ(x,a))).
output(equ(x,a)).
""").splitlines()
        self.assertEqual(set(lines[:2]), {"x = 2", "x = -2"})
        self.assertEqual(lines[2], "x² = 4")

    def test_equation_shared_variables_and_unresolved(self):
        lines = self.run_programme("""output(solve(equation(x + 1 = 2.x))).
output(solve(equation(x = x + 1))).
""").splitlines()
        self.assertEqual(lines, ["x = 1", "No solution established."])

    def test_equation_conditional_return_and_algebraic_output(self):
        lines = self.run_programme("""equation equ(x, const a) {
    if (a > 0) { return equation(x = a + 1). }
    else { return equation(x = a - 1). }
}
const a = -2.
output(solve(equ(x,a))).
outputa(equ(x,a)).
""").splitlines()
        self.assertEqual(lines[0], "x = -3")
        self.assertIn("a", lines[1])

    def test_generated_equation_cards(self):
        for source, expected in (("26.Y = 320/9", {"Y = ¹⁶⁰⁄₁₁₇"}),
                                 ("x^2 = 4", {"x = 2", "x = -2"}),
                                 ("x+1 = 2*x", {"x = 1"})):
            with self.subTest(source=source):
                fields, raw, code = mars_lab.run_equation_lab_fields(mars_lab.DEFAULT_EQUATION_BIN, source, 40)
                self.assertEqual(code, 0, raw)
                result = self.run_programme(mars_lab.function_for_display(fields["function"]))
                self.assertEqual(set(result.splitlines()), expected)

    def test_equation_errors(self):
        for source, diagnostic in (
            ("output(solve(2)).", "solve requires an equation value"),
            ("output(solve(equation(1 = 0))).", "named unknown"),
            ("equation equ(x) { return x. } output(equ(x)).", "declared function type"),
            ("expression expr(x) { return equation(x = 2). } output(expr(x)).", "declared function type"),
            ("output(equation(x == 2)).", "exactly one"),
            ("output(equation(x)).", "lhs = rhs"),
            ("q = equation(x = 2).", "storing equation"),
            ("output(solve(solve(equation(x = 2)))).", "solve requires an equation value"),
        ):
            with self.subTest(source=source):
                result = subprocess.run([str(BINARY)], input=source, text=True, capture_output=True, timeout=10)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(diagnostic, result.stderr)

    def test_late_bindings(self):
        lines = self.run_programme("""s = sqrt(x^2 + y^2).
output(s).
x = 3, y = 4.
output(s).
x = 5, y = 12.
output(s).
outputa(s).
""").splitlines()
        self.assertIn("x", lines[0])
        self.assertEqual(float(lines[1]), 5)
        self.assertEqual(float(lines[2]), 13)
        self.assertIn("x", lines[3])

    def test_conditionals(self):
        for point, expected in (("2", 4), ("-2", -1)):
            source = """expression expr(x) {
    if (realpart(x) == x && x > 0) {
        return x^2.
    } else {
        return -1.
    }
}
x = POINT.
output(expr(x)).
""".replace("POINT", point)
            self.assertEqual(float(self.run_programme(source)), expected)

    def test_symbolic_domain_guard(self):
        source = """expression expr(s) {
    if (realpart(s) > 0) {
        return 10/s^2 - 8/s + (13.s + 37)/((s + 2)^2 + 1).
    } else { return @nan. }
}
s = ?.
output(expr(s)).
s = 1.
output(expr(s)).
s = -1.
output(expr(s)).
s = 0.
output(expr(s)).
"""
        lines = self.run_programme(source).splitlines()
        self.assertIn("; Re(s) > 0 }", lines[0])
        self.assertNotIn("where", lines[0])
        self.assertIn("10/s²", lines[0])
        self.assertEqual(lines[1:], ["7", "NAN", "NAN"])

    def test_generated_domain_guards_round_trip(self):
        cases = (
            "1/s where (Re(s)>0)",
            "1/s where (Re(s)>0; Re(s+2)>0)",
            "1/s where (s != 0)",
            "1/s where (s ∈ ℝ; s != 0)",
            "1/(s+a) where (Re(s)>Re(a); a != 0)",
        )
        for source in cases:
            with self.subTest(source=source):
                fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40)
                self.assertEqual(code, 0, raw)
                actual = self.run_programme(fields["function"])
                self.assertEqual(mars_lab.expression_for_display(actual),
                                 mars_lab.expression_for_display(fields["expression"]))

    def test_symbolic_guard_local_bindings_do_not_leak(self):
        lines = self.run_programme("""expression expr(s) {
    if (realpart(s) > 0) {
        v = s + 1.
        s = 2.
        return v + s.
    } else { return @nan. }
}
s = ?.
output(expr(s)).
output(s).
""").splitlines()
        self.assertIn("Re(s) > 0", lines[0])
        self.assertIn("s = NAN", lines[0])
        self.assertIn("s = NAN", lines[1])
        self.assertNotIn("where", lines[0])

    def test_symbolic_guard_retains_late_bindings(self):
        for condition in ("realpart(s)>0", "s!=0", "realpart(s)==s && s!=0"):
            with self.subTest(condition=condition):
                lines = self.run_programme("expression expr(s) { if (" + condition + """
) { return 1/s. } else { return @nan. } }
s = ?.
v = expr(s).
output(v).
s = 2.
output(v).
s = 0.
output(v).
""").splitlines()
                self.assertIn("s = NAN", lines[0])
                self.assertEqual(lines[1], "½")
                # Stored algebraic values use output's normal fallback when evaluation is undefined.
                self.assertIn("{ 1/s | s = 0;", lines[2])

    def test_unknown_condition_short_circuit(self):
        for condition, expected in (("realpart(s)>0 && 1<0", "0"),
                                    ("realpart(s)>0 || 1>0", "1"),
                                    ("1<0 && realpart(s)>0", "0"),
                                    ("1>0 || realpart(s)>0", "1")):
            with self.subTest(condition=condition):
                source = "expression expr(s) { if (" + condition + ") { return 1. } else { return 0. } }"
                self.assertEqual(self.run_programme(source + "\ns = ?. output(expr(s))."), expected)

    def test_unsupported_symbolic_guard_is_diagnostic(self):
        for condition in ("s>0", "realpart(s)>0 || realpart(s)<-2"):
            with self.subTest(condition=condition):
                source = "expression expr(s) { if (" + condition + ") { return 1. } else { return @nan. } }"
                result = subprocess.run([str(BINARY)], input=source + "\ns = ?. output(expr(s)).",
                                        text=True, capture_output=True, timeout=10)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("condition requires numerical bindings", result.stderr)

    def test_missing_conditional_binding_is_diagnostic(self):
        result = subprocess.run([str(BINARY)], input="""expression expr(x) {
if (x > 0) { return x. } else { return 0. }
}
x = ?.
output(expr(x)).
""", text=True, capture_output=True, timeout=10)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("condition requires numerical bindings", result.stderr)
        self.assertIn("line 2", result.stderr)

    def test_symbolic_fallback(self):
        self.assertIn("sin(x)", self.run_programme(
            "expression expr(x) { return sin(x). }\nx = ?.\noutput(expr(x)).\n"))

    def test_binding_independent_simplification(self):
        self.assertEqual(self.run_programme("output(x-x)."), "0")

    def test_decimal_and_scientific_literals(self):
        self.assertEqual(self.run_programme("x=1.25. output(x.2 + 3e-2)."), "²⁵³⁄₁₀₀")

    def test_comments_and_exact_constants(self):
        result = self.run_programme("""\x60 source illustration: output(999). \x60
expression expr(const a) { return a + 1/3. }
const a = 2/3.
\x60\x60 output(999).
output(expr(a)).
""")
        self.assertEqual(float(result), 1)

    def test_greek_parameters(self):
        self.assertEqual(float(self.run_programme(
            "expression expr(@omega) { return @omega^2. }\n@omega = 3.\noutput(expr(@omega)).\n")), 9)

    def test_bracketed_scalar_names_and_late_bindings(self):
        self.assertEqual(self.run_programme(
            "[distance] = [speed].[time]. [speed] = 3, [time] = 4. output([distance])."), "12")
        for name in ("[time]", "[elapsed time]", "[sqrt(2)]", "[x>y]", "[a,b]", "[a&&b]"):
            with self.subTest(name=name):
                source = ("expression expr(" + name + ", const [offset]) { "
                          "if (" + name + " > 0) { return " + name + " + [offset]. } "
                          "else { return @nan. } }\n" + name + " = 3. const [offset] = 4.\n"
                          "output(expr(" + name + ", [offset])).")
                self.assertEqual(self.run_programme(source), "7")

    def test_bracketed_generated_scalar_cards(self):
        for name in ("[radius]", "[elapsed time]", "[sqrt(2)]"):
            source = "{ " + name + "^2 | " + name + "=3 }"
            with self.subTest(name=name):
                fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40)
                self.assertEqual(code, 0, raw)
                self.assertEqual(self.run_programme(mars_lab.function_for_display(fields["function"])), "9")

    def test_function_output_uses_bare_multicharacter_symbols(self):
        for name in ("time", "radius", "frequency", "distance", "speed"):
            with self.subTest(name=name):
                source = "{ [" + name + "]^2 | [" + name + "]=3 }"
                fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40)
                self.assertEqual(code, 0, raw)
                self.assertIn("[" + name + "]", fields["expression"])
                programme = fields["function"]
                self.assertIn("expression expr(" + name + ")", programme)
                self.assertIn("return " + name + "^2.", programme)
                self.assertIn(name + " = 3.", programme)
                self.assertNotIn("[" + name + "]", programme)
                self.assertEqual(self.run_programme(programme), "9")
        fields, raw, code = mars_lab.run_mars_lab_fields(
            mars_lab.DEFAULT_BIN, "{x+[offset] | x=3; [offset]=?}", 40)
        self.assertEqual(code, 0, raw)
        self.assertIn("const offset", fields["function"])
        self.assertNotIn("[offset]", fields["function"])
        self.assertEqual(self.run_programme(fields["function"].replace("const offset = ?.", "const offset = 4.")), "7")

    def test_function_output_keeps_necessary_identifier_quoting(self):
        for name in ("return", "output", "const", "where", "theta", "elapsed time", "sqrt(2)"):
            with self.subTest(name=name):
                source = "{ [" + name + "]^2 | [" + name + "]=3 }"
                fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40)
                self.assertEqual(code, 0, raw)
                self.assertIn("[" + name + "]", fields["function"])
                self.assertEqual(self.run_programme(fields["function"]), "9")

    def test_unsupported_and_malformed_programmes(self):
        for source in (
            "matrix expr() { return [1,2]. } output(expr()).",
            "expression expr(array x) { return x. }",
            "expression expr(x) { return expr(x). } x=1. output(expr(x)).",
            "while (1) { output(1). }",
            "x = 1", "output(1*2).", "output(1). \x00",
            "[time = 1.", "output([time).", "output([time]]).", "[a}b] = 1.",
        ):
            with self.subTest(source=source):
                result = subprocess.run([str(BINARY)], input=source, text=True, capture_output=True, timeout=10)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("line ", result.stderr)

    def test_generated_native_cards(self):
        for source in ("{sqrt(x^2+y^2) | x=3; y=4}", "{sin(x)+cos(x) | x=0}",
                       "{@F{tanh(x)} | k=1}", "{x^2+@pi | x=2}"):
            with self.subTest(source=source):
                fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40)
                self.assertEqual(code, 0, raw)
                result = self.run_programme(mars_lab.function_for_display(fields["function"]))
                self.assertTrue(result)
                self.assertNotIn("NAN", result)


class FunctionRunTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which("gjs-console") or shutil.which("node"), "JavaScript runtime is not installed")
    def test_run_ui_full_source_errors_and_stale_responses(self):
        start = mars_lab.INDEX_HTML.index("    function clearFunctionRun()")
        end = mars_lab.INDEX_HTML.index("    function setExpandableText(", start)
        script = mars_lab.INDEX_HTML[start:end] + r"""
const classes = () => ({add() {}, remove() {}});
const functionRun = {disabled: false};
const functionTitle = {textContent: 'Function'};
const functionStyle = {dataset: {fullText: 'output(1/3).'}, textContent: 'abbreviated...'};
const functionRunResult = {classList: classes()};
const functionRunOutput = {textContent: ''};
let functionRunSequence = 0, functionRunController = null, request = null;
class AbortController { constructor() {this.signal = {};} abort() {} }
function setActionRunning() {}
function requestedValuePrecision() {return 80;}
function setTimeout() {return 1;}
function clearTimeout() {}
let fetch = async (path, options) => {
    request = {path, ...JSON.parse(options.body)};
    return {ok: true, json: async () => ({ok: true, output: '1/3\n'})};
};
(async () => {
    await runFunctionCard();
    if (request.path !== '/function-run' || request.source !== 'output(1/3).' || request.precision !== 80)
        throw Error('Did not submit the complete card source at its precision');
    if (functionRun.disabled || functionRunOutput.textContent !== '1/3')
        throw Error('Successful execution did not finish cleanly');
    fetch = async () => ({ok: false, json: async () => ({ok: false, error: '<unsupported>'})});
    await runFunctionCard();
    if (functionRunOutput.textContent !== '<unsupported>')
        throw Error('Diagnostic was not displayed as plain text');
    let release;
    fetch = () => new Promise(resolve => {release = resolve;});
    const pending = runFunctionCard();
    clearFunctionRun();
    release({ok: true, json: async () => ({ok: true, output: 'stale'})});
    await pending;
    if (functionRunOutput.textContent || functionRun.disabled)
        throw Error('Stale request changed the new card');
    functionStyle.dataset.fullText = '';
    await runFunctionCard();
    if (!functionRunOutput.textContent.includes('No Function programme'))
        throw Error('Empty card was submitted');
    if (typeof print === 'function') print('RUN UI PASS');
    else console.log('RUN UI PASS');
})().catch(error => {console.error(error.stack);});
"""
        runtime = shutil.which("gjs-console") or shutil.which("node")
        flag = "-c" if Path(runtime).name == "gjs-console" else "-e"
        result = subprocess.run([runtime, flag, script], text=True, capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("RUN UI PASS", result.stdout, result.stderr)

    def test_endpoint_preserves_source_and_precision(self):
        source = "expression expr() { return 1/3. }\noutput(expr())."
        body = json.dumps({"source": source, "precision": 80}).encode()
        handler = object.__new__(mars_lab.MarsLabHandler)
        handler.path = "/function-run"
        handler.headers = {"Content-Length": str(len(body))}
        handler.rfile = io.BytesIO(body)
        handler.request_allowed = mock.Mock(return_value=True)
        handler.send_json = mock.Mock()
        result = {"ok": True, "output": "1/3\n", "error": ""}
        with mock.patch.object(mars_lab, "run_function_programme", return_value=result) as run:
            handler.do_POST()
        run.assert_called_once_with(source, 80)
        handler.send_json.assert_called_once_with(200, result)

    def test_backend_uses_stdin_without_shell(self):
        with mock.patch.object(mars_lab, "ensure_scratch_binary"), mock.patch.object(mars_lab.subprocess, "run") as run:
            run.return_value = subprocess.CompletedProcess([], 0, "2\n", "")
            self.assertTrue(mars_lab.run_function_programme("output(2).", 40)["ok"])
        self.assertEqual(run.call_args.kwargs["input"], "output(2).")
        self.assertNotIn("shell", run.call_args.kwargs)
        self.assertEqual(run.call_args.kwargs["timeout"], 30)

    def test_run_output_uses_expression_card_binding_notation(self):
        raw = "{ t + a + x | t = NAN, x = 3; a = NAN }\nNAN\n"
        diagnostic = "line 3: value = NAN is undefined"
        with mock.patch.object(mars_lab, "ensure_scratch_binary"), mock.patch.object(mars_lab.subprocess, "run") as run:
            run.return_value = subprocess.CompletedProcess([], 1, raw, diagnostic)
            result = mars_lab.run_function_programme("output(t+a+x).", 40)
        self.assertEqual(result["output"], "{ t + a + x | t = ?, x = 3; a = ? }\nNAN\n")
        self.assertEqual(result["output"], mars_lab.expression_for_display(raw))
        self.assertEqual(result["error"], diagnostic)

    def test_run_inverse_laplace_result_has_unknown_time_binding(self):
        source = """expression expr(t) {
    return 10.t + exp(-2.t).(13.cos(t) + 11.sin(t)) - 8.
}
t = ?.
output(expr(t)).
"""
        with mock.patch.object(mars_lab, "ensure_scratch_binary"):
            result = mars_lab.run_function_programme(source, 40)
        self.assertTrue(result["ok"], result["error"])
        self.assertIn("t = ?", result["output"])
        self.assertNotIn("NAN", result["output"])
        self.assertIn("cos(t)", result["output"])

    def test_limits_and_timeout(self):
        for source in ("", "x" * 65537, "output(1).\0", None):
            with self.subTest(source=str(source)[:30]):
                with self.assertRaises(ValueError):
                    mars_lab.run_function_programme(source, 40)
        with mock.patch.object(mars_lab, "ensure_scratch_binary"), mock.patch.object(
            mars_lab.subprocess, "run", side_effect=subprocess.TimeoutExpired("ophelia", 30)
        ):
            self.assertIn("30-second", mars_lab.run_function_programme("output(1).", 40)["error"])

    def test_button_is_on_shared_function_card(self):
        card = mars_lab.INDEX_HTML.split('id="functionCard"', 1)[1].split('id="valueNoteCard"', 1)[0]
        self.assertIn('id="functionRun"', card)
        self.assertIn('>RUN</button>', card)
        self.assertIn('id="functionRunOutput"', card)
        self.assertIn("functionText !== 'Function'", mars_lab.INDEX_HTML)
        self.assertIn("String(functionStyle.dataset.fullText || '').trim()", mars_lab.INDEX_HTML)
        self.assertIn("sequence !== functionRunSequence", mars_lab.INDEX_HTML)
        self.assertNotIn("eval(source)", mars_lab.INDEX_HTML)


class ZZOpheliaReadmeExamples(unittest.TestCase):
    def test_readme_derivative_of_transform(self):
        # README example: docs/expression.md, Differentiating transform results.
        fields, raw, code = mars_lab.run_mars_lab_fields(
            mars_lab.DEFAULT_BIN, "{ Ds(@L{t}) | s=2 }", 32, "s")
        self.assertEqual(code, 0, raw)
        self.assertEqual(mars_lab.expression_for_display(fields["expression"]), "{ -2/s³ | s = 2; Re(s) > 0 }")
        self.assertIn("derivative(laplace(t, t, s), s, 1)", fields["operation_function"])
        with mock.patch.object(mars_lab, "ensure_scratch_binary"):
            result = mars_lab.run_function_programme(fields["operation_function"], 32)
        self.assertTrue(result["ok"], result["error"])
        self.assertEqual(result["output"].strip(), "-0.25")

    def test_readme_laplace_tangent_primitive(self):
        # README example: docs/expression.md, Laplace transforms of integrals.
        source = "{ @L{@S^t tan(x) dx} | s=pi }"
        fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, "s")
        self.assertEqual(code, 0, raw)
        with mock.patch.object(mars_lab, "ensure_scratch_binary"):
            result = mars_lab.run_function_programme(fields["operation_function"], 40)
        self.assertTrue(result["ok"], result["error"])
        value = complex(result["output"].strip().replace(" ", "").replace("i", "j"))
        self.assertAlmostEqual(value, 0.0390230861269195 - 0.00719151138794376j, places=14)

    def test_readme_laplace_logarithmic_primitive(self):
        # README example: docs/expression.md, Laplace transforms of integrals.
        source = "{ @L{@S^t 1/(1-x^2) dx} | s=1 }"
        fields, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, source, 40, "s")
        self.assertEqual(code, 0, raw)
        expected = "{ (exp(s)*E1(s)+exp(-s)*Ei(s)-i*pi*(1-exp(-s)))/(2*s) | s=1 }"
        result, raw, code = mars_lab.run_mars_lab_fields(mars_lab.DEFAULT_BIN, expected, 40, "s")
        self.assertEqual(code, 0, raw)
        value = complex(fields["value"].replace(" ", "").replace("i", "j"))
        reference = complex(result["value"].replace(" ", "").replace("i", "j"))
        self.assertAlmostEqual(value, reference, places=14)
        self.assertAlmostEqual(value, 0.6467611227791301 - 0.9929326518994358j, places=14)
        with mock.patch.object(mars_lab, "ensure_scratch_binary"):
            result = mars_lab.run_function_programme(fields["operation_function"], 40)
        self.assertTrue(result["ok"], result["error"])
        self.assertEqual(result["output"].strip(), fields["value"])

    def test_readme_executable_calculus_cards(self):
        # README examples: docs/mars-lab.md, Running Function cards.
        cases = (
            ("""expression expr(s) {
    if (realpart(s) > 0) {
        return laplace(integral(sin(x), x, t), t, s).
    } else {
        return @nan.
    }
}
s = ?.
output(expr(s)).
""", "{ -s/(s² + 1) | s = ?; Re(s) > 0 }"),
            ("""expression expr(z) {
    return integral(sin(x), x, z).
}
z = ?.
output(expr(z)).
""", "{ -cos(z) | z = ? }"),
            ("""expression expr(x, const C) {
    return integral(sin(x), x).
}
x = @pi.
const C = 0.
output(expr(x, C)).
""", "1"),
            ("output(integral(sin(x), x, 0, @pi)).", "2"),
            ("""expression expr(x, const C) {
    return integral(sin(x), x).
}
x = ?.
const C = ?.
output(expr(x, C)).
""", "{ C - cos(x) | x = ?; C = ? }"),
            ("""expression expr(x) {
    return derivative(sin(x), x, 1).
}
x = ?.
output(expr(x)).
""", "{ cos(x) | x = ? }"),
        )
        for source, expected in cases:
            with self.subTest(source=source):
                with mock.patch.object(mars_lab, "ensure_scratch_binary"):
                    result = mars_lab.run_function_programme(source, 40)
                self.assertTrue(result["ok"], result["error"])
                self.assertEqual(result["output"].strip(), expected)

    def test_readme_symbolic_domain_guard(self):
        # README example: docs/mars-lab.md, Running Function cards.
        source = """expression expr(s) {
    if (realpart(s) > 0) {
        return 1/s.
    } else {
        return @nan.
    }
}
s = ?.
output(expr(s)).
"""
        with mock.patch.object(mars_lab, "ensure_scratch_binary"):
            result = mars_lab.run_function_programme(source, 40)
        self.assertTrue(result["ok"], result["error"])
        self.assertEqual(result["output"].strip(), "{ 1/s | s = ?; Re(s) > 0 }")

    def test_readme_inverse_laplace_programme(self):
        # README example: docs/mars-lab.md, Running Function cards.
        source = """expression expr(t) {
    return inverselaplace(5.(s + 3 + 10/s^2)/(s^2 + 4.s + 5), s, t).
}
t = ?.
output(expr(t)).
"""
        with mock.patch.object(mars_lab, "ensure_scratch_binary"):
            result = mars_lab.run_function_programme(source, 40)
        self.assertTrue(result["ok"], result["error"])
        self.assertEqual(result["output"].strip(), "{ 10t + exp(-2t)·(13·cos(t) + 11·sin(t)) - 8 | t = ? }")

    def test_readme_equation_function(self):
        # README example: docs/mars-lab.md, Running Function cards.
        source = """equation equ(Y) {
    return equation(26.Y = 320/9).
}
`` Y = ?
outputa(solve(equ(Y))).
"""
        result = subprocess.run([str(BINARY), "40"], input=source, text=True, capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.strip(), "Y = ¹⁶⁰⁄₁₁₇")

    def test_readme_scalar_function(self):
        # README example: docs/mars-lab.md, Running Function cards.
        source = """expression expr(x, y) {
    return sqrt(x^2 + y^2).
}
x = 3.
y = 4.
output(expr(x, y)).
"""
        result = subprocess.run([str(BINARY), "40"], input=source, text=True, capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.strip(), "5")


if __name__ == "__main__":
    focus = json.loads((ROOT / "tests/test_config.json").read_text()).get("ophelia_focus")
    suite = unittest.defaultTestLoader.loadTestsFromNames(
        [name if "." in name else "OpheliaTests." + name for name in focus], sys.modules[__name__]
    ) if focus else unittest.defaultTestLoader.loadTestsFromModule(sys.modules[__name__])
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    raise SystemExit(not result.wasSuccessful())
