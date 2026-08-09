import subprocess
import sys
import unittest
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

import mars_lab


class MobileAccessTests(unittest.TestCase):
    def test_almanac_time_uses_styled_mobile_input_with_automatic_separators(self) -> None:
        self.assertIn(
            'id="almanacTime" type="text" inputmode="decimal"',
            mars_lab.INDEX_HTML,
        )
        self.assertIn("function formatAlmanacTimeInput(value)", mars_lab.INDEX_HTML)
        self.assertIn("almanacTime.addEventListener('input'", mars_lab.INDEX_HTML)

    def test_wildcard_listener_prefers_private_tailscale_url(self) -> None:
        with (
            mock.patch.object(mars_lab, "tailscale_funnel_enabled", return_value=False),
            mock.patch.object(mars_lab, "tailscale_ipv4", return_value="100.64.0.7"),
            mock.patch.object(
                mars_lab,
                "tailscale_https_host",
                return_value="ophelia.example.ts.net",
            ),
            mock.patch.object(mars_lab, "local_mdns_host", return_value="ophelia.local"),
        ):
            details = mars_lab.mobile_access_details("::", 8765)

        self.assertEqual(details["url"], "https://ophelia.example.ts.net/")
        self.assertEqual(details["title"], "Tailscale access")
        self.assertTrue(details["tailscale"])


class EquationResultTests(unittest.TestCase):
    def test_use_as_input_reads_the_equation_card(self) -> None:
        self.assertIn(
            "if (currentMode() === 'equation')\n"
            "        return parsedExpressionText();",
            mars_lab.INDEX_HTML,
        )
        self.assertIn(
            "if (currentMode() === 'equation' || currentMode() === 'diffequation')\n"
            "        setExpressionEditor(resultText);\n"
            "      else if (!await applyMarsBindingExpression(resultText))",
            mars_lab.INDEX_HTML,
        )

    def test_multiline_numeric_solutions_reach_payload(self) -> None:
        raw = "\n".join(
            (
                "input       x^2 - 1 = 0",
                "equation    { x² - 1 = 0 | x = NAN }",
                "unbound     x² - 1 = 0",
                "function    equation equ(x) {",
                "    return equation(x^2 - 1 = 0);",
                "}",
                "status      solved",
                "solutions   x = 1",
                "            x = -1",
                "numeric     x ≈ 1",
                "            x ≈ -1",
            )
        )

        fields = mars_lab.parse_equation_lab_output(raw)
        payload = mars_lab.prepare_equation_fields(fields, 72)

        self.assertEqual(payload["solution_count"], 2)
        self.assertEqual(
            payload["function"],
            "equation equ(x) {\n    return equation(x^2 - 1 = 0);\n}",
        )
        self.assertEqual(
            payload["solutions"].splitlines(),
            ["x = 1", "x = -1"],
        )
        self.assertEqual(
            payload["numeric_solutions"],
            ["x ≈ 1", "x ≈ -1"],
        )

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "equation_lab").is_file(),
        "release equation_lab helper is not built",
    )
    def test_solution_pane_orders_real_roots_then_complex_pairs(self) -> None:
        equation_binary = (
            ROOT / "build" / "release" / "scratch" / "equation_lab"
        )
        equation = (
            "{ x^9 - 8x^8 + 16x^7 + 2x^6 - 66x^5 + 158x^4"
            " - 16x^3 - 2x^2 + 65x - 150 = 0 | x = NAN }"
        )
        completed = subprocess.run(
            [str(equation_binary), equation, "32"],
            check=True,
            capture_output=True,
            text=True,
        )

        fields = mars_lab.parse_equation_lab_output(completed.stdout)
        payload = mars_lab.prepare_equation_fields(fields, 32)

        self.assertEqual(
            payload["solutions"].splitlines(),
            [
                "x = -2",
                "x = -1",
                "x = 1",
                "x = 3",
                "x = 5",
                "x = i",
                "x = -i",
                "x = 1 + 2i",
                "x = 1 - 2i",
            ],
        )


class MatrixResultTests(unittest.TestCase):
    def test_matrix_function_operations_are_exposed_by_the_client(self) -> None:
        representative_operations = {
            "exp", "log", "sqrt", "sin", "cos", "tan", "sinh", "cosh", "tanh", "erf", "gamma", "lambert_w0",
        }

        self.assertTrue(representative_operations.issubset(mars_lab.MATRIX_OPERATIONS))
        for operation in representative_operations:
            self.assertIn(f'value="{operation}"', mars_lab.INDEX_HTML)
        self.assertIn("genuine matrix functions calculated by MARSlib", mars_lab.INDEX_HTML)
        self.assertIn("sin(1 2; 4 5)", mars_lab.INDEX_HTML)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "matrix_lab").is_file(),
        "release matrix_lab helper is not built",
    )
    def test_native_helper_evaluates_representative_matrix_functions(self) -> None:
        matrix_binary = ROOT / "build" / "release" / "scratch" / "matrix_lab"
        cases = (
            ("(0, 0; 0, 0)", "exp", "(1, 0; 0, 1)"),
            ("(1, 0; 0, e)", "log", "(0, 0; 0, 1)"),
            ("(4, 0; 0, 9)", "sqrt", "(2, 0; 0, 3)"),
            ("(0, 0; 0, 0)", "sin", "(0, 0; 0, 0)"),
        )

        for matrix_text, operation, expected in cases:
            with self.subTest(operation=operation):
                fields, raw, returncode = mars_lab.run_matrix_lab_fields(matrix_binary, matrix_text, operation, 64)

                self.assertEqual(returncode, 0, raw)
                self.assertEqual(fields["operation"], operation)
                self.assertEqual(fields["result"], expected)
                self.assertNotIn("i", fields["result"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "matrix_lab").is_file(),
        "release matrix_lab helper is not built",
    )
    def test_native_helper_accepts_direct_compact_matrix_function_syntax(self) -> None:
        matrix_binary = ROOT / "build" / "release" / "scratch" / "matrix_lab"
        direct_fields, direct_raw, direct_returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary, "sin(1 2; 4 5)", "inverse", 64
        )
        selected_fields, selected_raw, selected_returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary, "(1, 2; 4, 5)", "sin", 64
        )

        self.assertEqual(direct_returncode, 0, direct_raw)
        self.assertEqual(selected_returncode, 0, selected_raw)
        self.assertEqual(direct_fields["operation"], "sin")
        self.assertEqual(direct_fields["result"], selected_fields["result"])
        self.assertEqual(direct_fields["tex"], selected_fields["tex"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "matrix_lab").is_file(),
        "release matrix_lab helper is not built",
    )
    def test_native_helper_explains_that_matrix_functions_require_square_matrices(self) -> None:
        matrix_binary = ROOT / "build" / "release" / "scratch" / "matrix_lab"
        fields, raw, returncode = mars_lab.run_matrix_lab_fields(matrix_binary, "sin(1 2 3; 4 5 6)", "eval", 64)

        self.assertNotEqual(returncode, 0, raw)
        self.assertEqual(fields["operation"], "sin")
        self.assertIn("requires a square matrix; received 2x3", raw)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "matrix_lab").is_file(),
        "release matrix_lab helper is not built",
    )
    def test_native_helper_accepts_direct_symbolic_inverse_and_product(self) -> None:
        matrix_binary = ROOT / "build" / "release" / "scratch" / "matrix_lab"
        inverse_fields, inverse_raw, inverse_returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary, "inverse(a b; c d)", "eval", 64
        )
        product_fields, product_raw, product_returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary, "(a b; c d).(e f; g h)", "eval", 64
        )

        self.assertEqual(inverse_returncode, 0, inverse_raw)
        self.assertEqual(inverse_fields["operation"], "inverse")
        self.assertEqual(inverse_fields["result"], "(d/(ad - bc), -b/(ad - bc); -c/(ad - bc), a/(ad - bc))")
        self.assertEqual(product_returncode, 0, product_raw)
        self.assertEqual(product_fields["operation"], "multiply")
        self.assertEqual(product_fields["result"], "(ae + bg, af + bh; ce + dg, cf + dh)")

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "matrix_lab").is_file(),
        "release matrix_lab helper is not built",
    )
    def test_native_helper_rejects_incompatible_direct_matrix_product(self) -> None:
        matrix_binary = ROOT / "build" / "release" / "scratch" / "matrix_lab"
        _fields, raw, returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary, "(1 2 3; 4 5 6).(1 2; 3 4)", "eval", 64
        )

        self.assertNotEqual(returncode, 0, raw)
        self.assertIn("requires matching inner dimensions; received 2x3 and 2x2", raw)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "matrix_lab").is_file(),
        "release matrix_lab helper is not built",
    )
    def test_native_helper_composes_matrix_inverse_and_product(self) -> None:
        matrix_binary = ROOT / "build" / "release" / "scratch" / "matrix_lab"
        fields, raw, returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary, "inverse(a b; c d).(x; y)", "eval", 64
        )

        self.assertEqual(returncode, 0, raw)
        self.assertEqual(fields["operation"], "multiply")
        self.assertEqual(fields["rows"], "2")
        self.assertEqual(fields["cols"], "1")
        self.assertEqual(fields["result"], "((dx - by)/(ad - bc); (ay - cx)/(ad - bc))")

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "matrix_lab").is_file(),
        "release matrix_lab helper is not built",
    )
    def test_native_helper_differentiates_and_integrates_matrix_entries(self) -> None:
        matrix_binary = ROOT / "build" / "release" / "scratch" / "matrix_lab"
        derivative, derivative_raw, derivative_returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary, "Dx(ax+b cx+d; y xy)", "eval", 64
        )
        integral, integral_raw, integral_returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary, "@S^x((ax+b cx+d; y xy))", "eval", 64
        )

        self.assertEqual(derivative_returncode, 0, derivative_raw)
        self.assertEqual(derivative["result"], "(a, c; 0, y)")
        self.assertEqual(integral_returncode, 0, integral_raw)
        self.assertEqual(integral["rows"], "2")
        self.assertEqual(integral["cols"], "2")
        self.assertEqual(integral["result"], "(½·(ax² + 2bx), ½·(cx² + 2dx); xy, ½x²y)")


class DiffequationResultTests(unittest.TestCase):
    def test_help_documents_native_differential_equation_syntax(self) -> None:
        help_html = mars_lab.INDEX_HTML

        self.assertIn("Bare D And Operator Polynomials", help_html)
        self.assertIn("(D^2 - @omega^2)^2(x) = 0", help_html)
        self.assertIn("(D^2 + @omega^2)^3x = 0", help_html)
        self.assertIn("positive integer powers up to 64", help_html)
        self.assertIn("(Dx^2 + 4Dx + 20)^2(y) = 0", help_html)
        self.assertIn("Greek Symbols And Differential Forms", help_html)
        self.assertIn("d@theta", help_html)
        self.assertIn("phi</code>, <code>@phi</code>, and <code>φ", help_html)
        self.assertIn("Dx(u) + Dy(u) = 0", help_html)
        self.assertIn("phi_xx + phi_yy = 0", help_html)
        self.assertIn("1/r^2 phi_thetatheta", help_html)
        self.assertIn("every symbolic solution returned by native MARSlib", help_html)

    def test_solver_TeX_uses_width_selected_left_aligned_layout(self) -> None:
        tex = (
            r"\begin{aligned}[t]"
            r"\mu z&=\frac{1}{8}x\left("
            r"\frac{4y^3e^{2xy}}{x} + \frac{6ye^{2xy}}{x^3} "
            r"- \frac{6y^2e^{2xy}}{x^2} - \frac{3e^{2xy}}{x^4}"
            r"\right)+F(x)"
            r"\end{aligned}"
        )

        wrapped = mars_lab.wrap_solver_TeX_lines(tex, threshold=60)

        self.assertGreater(wrapped.count(r"\\"), tex.count(r"\\"))
        self.assertIn(r"&\displaystyle \mu z=", wrapped)
        self.assertIn(r"&\displaystyle \qquad {}+", wrapped)
        self.assertNotIn(r"\left", wrapped)
        self.assertIn(r"\bigl", wrapped)
        self.assertIn("data.steps_wrapped_TeX || solverTexSource", mars_lab.INDEX_HTML)
        self.assertIn("compactWidth > solverTexContentWidth()", mars_lab.INDEX_HTML)
        self.assertIn("overflow-x: hidden", mars_lab.INDEX_HTML)
        self.assertIn("overflow-y: auto", mars_lab.INDEX_HTML)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "diffequation_lab").is_file(),
        "release diffequation_lab helper is not built",
    )
    def test_native_helper_solves_two_dimensional_laplace_equation(self) -> None:
        completed = subprocess.run(
            [
                str(
                    ROOT
                    / "build"
                    / "release"
                    / "scratch"
                    / "diffequation_lab"
                ),
                "phi_xx + phi_yy = 0",
            ],
            check=True,
            capture_output=True,
            text=True,
        )

        fields = mars_lab.parse_diffequation_lab_output(completed.stdout)
        self.assertEqual(fields["status"], "solved")
        self.assertEqual(fields["solver"], "Laplace")
        self.assertIn(
            r"\frac{\partial^{2} \phi}{\partial x^{2}}",
            fields["problem_TeX"],
        )
        self.assertIn(
            r"\phi &= F\left(x + i y\right) + G\left(x - i y\right)",
            fields["solutions_TeX"],
        )
        self.assertIn(
            r"\frac{\partial^2 \phi}{\partial x^2}",
            fields["steps_TeX"],
        )
        self.assertIn(
            r"\frac{\partial^2 \phi}{\partial y^2}",
            fields["steps_TeX"],
        )
        self.assertIn(r"\Delta \phi", fields["steps_TeX"])
        self.assertNotIn(
            r"\text{Quod Erat Demonstrandum}",
            fields["steps_TeX"],
        )

        payload = mars_lab.prepare_diffequation_fields(fields)
        self.assertTrue(payload.get("svg"))
        cartesian_steps_svg, cartesian_steps_error = (
            mars_lab.render_TeX_to_svg(
                mars_lab.TeX_for_display(fields["steps_TeX"])
            )
        )
        self.assertIsNone(cartesian_steps_error)
        self.assertTrue(cartesian_steps_svg)

        renamed_completed = subprocess.run(
            [
                str(
                    ROOT
                    / "build"
                    / "release"
                    / "scratch"
                    / "diffequation_lab"
                ),
                "u_ss + u_tt = 0",
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        renamed_fields = mars_lab.parse_diffequation_lab_output(
            renamed_completed.stdout
        )
        self.assertEqual(renamed_fields["status"], "solved")
        self.assertIn(
            r"u &= F\left(i t + s\right) + G\left(s - i t\right)",
            renamed_fields["solutions_TeX"],
        )
        self.assertIn(
            r"\frac{\partial^2 u}{\partial s^2}",
            renamed_fields["steps_TeX"],
        )
        self.assertIn(
            r"\frac{\partial^2 u}{\partial t^2}",
            renamed_fields["steps_TeX"],
        )
        self.assertNotIn(r"\partial x", renamed_fields["steps_TeX"])
        self.assertNotIn(r"\partial y", renamed_fields["steps_TeX"])

        polar_completed = subprocess.run(
            [
                str(
                    ROOT
                    / "build"
                    / "release"
                    / "scratch"
                    / "diffequation_lab"
                ),
                "phi_rr + 1/r phi_r + 1/r^2 phi_thetatheta = 0",
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        polar_fields = mars_lab.parse_diffequation_lab_output(
            polar_completed.stdout
        )
        self.assertEqual(polar_fields["status"], "solved")
        self.assertEqual(polar_fields["solver"], "Laplace")
        self.assertIn(
            r"\frac{\partial^{2} \phi}{\partial \theta^{2}}",
            polar_fields["problem_TeX"],
        )
        self.assertIn(
            r"F\left(r \cdot e^{i \theta}\right)",
            polar_fields["solutions_TeX"],
        )
        self.assertIn(r"z_{\theta\theta}=-z", polar_fields["steps_TeX"])
        self.assertIn(r"u_{\theta\theta}", polar_fields["steps_TeX"])
        self.assertIn(r"\Delta u", polar_fields["steps_TeX"])
        self.assertIn(
            r"\text{Therefore,}\quad u(r,\theta)",
            polar_fields["steps_TeX"],
        )
        steps_svg, steps_error = mars_lab.render_TeX_to_svg(
            mars_lab.TeX_for_display(polar_fields["steps_TeX"])
        )
        self.assertIsNone(steps_error)
        self.assertTrue(steps_svg)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "diffequation_lab").is_file(),
        "release diffequation_lab helper is not built",
    )
    def test_parameter_linear_pde_shows_integrating_factor_steps(self) -> None:
        completed = subprocess.run(
            [
                str(
                    ROOT
                    / "build"
                    / "release"
                    / "scratch"
                    / "diffequation_lab"
                ),
                "z_y + 2*y*z = x*y^3",
            ],
            check=True,
            capture_output=True,
            text=True,
        )

        fields = mars_lab.parse_diffequation_lab_output(completed.stdout)
        self.assertEqual(fields["status"], "solved")
        self.assertEqual(fields["solver"], "parameter-dependent linear PDE")
        self.assertIn(
            r"z &= \frac{1}{2} x \cdot \left(y^{2} - 1\right)",
            fields["solutions_TeX"],
        )
        self.assertIn(
            r"\mu&=e^{\int\left(2 y\right)\,dy}=e^{y^{2}}",
            fields["steps_TeX"],
        )
        self.assertIn(
            r"\frac{\partial\mu}{\partial y}"
            r"&=\left(2 y\right)\mu",
            fields["steps_TeX"],
        )
        self.assertIn(
            r"\mu\frac{\partial z}{\partial y}"
            r"+\frac{\partial\mu}{\partial y}z",
            fields["steps_TeX"],
        )
        self.assertIn(r"=\mu x y^{3}", fields["steps_TeX"])
        self.assertNotIn(r"\mu\cdot x y^{3}", fields["steps_TeX"])
        self.assertNotIn(r"\mu\left(x y^{3}\right)", fields["steps_TeX"])
        self.assertIn(
            r"\mu z&=\frac{1}{2} x \cdot \left(y^{2} - 1\right)",
            fields["steps_TeX"],
        )
        self.assertIn(r"F\left(x\right)", fields["steps_TeX"])
        self.assertNotIn(
            r"\text{Quod Erat Demonstrandum}",
            fields["steps_TeX"],
        )

        steps_svg, steps_error = mars_lab.render_TeX_to_svg(
            mars_lab.TeX_for_display(fields["steps_TeX"])
        )
        self.assertIsNone(steps_error)
        self.assertTrue(steps_svg)

        parameter_rate_completed = subprocess.run(
            [
                str(
                    ROOT
                    / "build"
                    / "release"
                    / "scratch"
                    / "diffequation_lab"
                ),
                "z_y + 2*x*z = x*y^3",
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        parameter_rate_fields = mars_lab.parse_diffequation_lab_output(
            parameter_rate_completed.stdout
        )
        self.assertEqual(parameter_rate_fields["status"], "solved")
        self.assertEqual(
            parameter_rate_fields["solver"],
            "parameter-dependent linear PDE",
        )
        self.assertIn(
            r"\frac{1}{2} y^{3}",
            parameter_rate_fields["solutions_TeX"],
        )
        self.assertIn(
            r"F\left(x\right) \cdot e^{-2 x y}",
            parameter_rate_fields["solutions_TeX"],
        )
        self.assertIn(
            r"\mu&=e^{\int\left(2 x\right)\,dy}=e^{2 x y}",
            parameter_rate_fields["steps_TeX"],
        )

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "diffequation_lab").is_file(),
        "release diffequation_lab helper is not built",
    )
    def test_compact_fourth_order_solution_TeX_stays_on_one_line(self) -> None:
        completed = subprocess.run(
            [
                str(
                    ROOT
                    / "build"
                    / "release"
                    / "scratch"
                    / "diffequation_lab"
                ),
                "(D^2 - @omega^2)^2(x) = 0",
            ],
            check=True,
            capture_output=True,
            text=True,
        )

        fields = mars_lab.parse_diffequation_lab_output(completed.stdout)
        solution_TeX = fields["solutions_TeX"]
        wrapped_TeX = fields["solutions_wrapped_TeX"]

        self.assertEqual(solution_TeX.count(r"\begin{aligned}[t]"), 1)
        self.assertIn(
            r"\left(C_{1} + C_{2} t\right) \cdot e^{\omega t}",
            solution_TeX,
        )
        self.assertIn(
            r"\left(C_{3} + C_{4} t\right) \cdot e^{-\omega t}",
            solution_TeX,
        )
        self.assertGreater(wrapped_TeX.count(r"\begin{aligned}[t]"), 1)

        payload = mars_lab.prepare_diffequation_fields(fields)
        self.assertTrue(payload.get("svg"))
        self.assertTrue(payload.get("wrapped_svg"))

    def test_solution_wrapping_uses_the_available_card_width(self) -> None:
        html = mars_lab.INDEX_HTML

        self.assertIn("function fitDiffequationSolutionToCard()", html)
        self.assertIn("compactWidth > renderedContentWidth() + 1", html)
        self.assertIn("new ResizeObserver", html)
        self.assertIn("data.solutions_wrapped_TeX || lastTex", html)
        self.assertIn("data.wrapped_svg || ''", html)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "diffequation_lab").is_file(),
        "release diffequation_lab helper is not built",
    )
    def test_repeated_quadratic_power_selects_explicit_or_sum_form(self) -> None:
        completed = subprocess.run(
            [
                str(
                    ROOT
                    / "build"
                    / "release"
                    / "scratch"
                    / "diffequation_lab"
                ),
                "(D^2 + @omega^2)^3x = 0",
            ],
            check=True,
            capture_output=True,
            text=True,
        )

        fields = mars_lab.parse_diffequation_lab_output(completed.stdout)
        solution_TeX = fields["solutions_TeX"]

        self.assertIn(r"\frac{d^{6} x}{d t^{6}}", fields["problem_TeX"])
        self.assertIn(r"\omega^{6} \cdot x", fields["problem_TeX"])
        self.assertNotIn(r"\sum", solution_TeX)
        self.assertIn(
            r"\left(C_{1} + C_{2} t + C_{3} t^{2}\right)",
            solution_TeX,
        )
        self.assertIn(
            r"\left(C_{4} + C_{5} t + C_{6} t^{2}\right)",
            solution_TeX,
        )
        self.assertIn(r"\cos(\omega t)", solution_TeX)
        self.assertIn(r"\sin(\omega t)", solution_TeX)

        completed = subprocess.run(
            [
                str(
                    ROOT
                    / "build"
                    / "release"
                    / "scratch"
                    / "diffequation_lab"
                ),
                "(D^2 + @omega^2)^4phi = 0",
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        fields = mars_lab.parse_diffequation_lab_output(completed.stdout)
        solution_TeX = fields["solutions_TeX"]
        self.assertIn(r"\frac{d^{8} \phi}{d x^{8}}", fields["problem_TeX"])
        self.assertNotIn(r"\left[phi\right]", fields["problem_TeX"])
        self.assertIn(r"\phi &=", solution_TeX)
        self.assertIn(r"\sum_{k=0}^{3}C_{k + 1} \cdot x^{k}", solution_TeX)
        self.assertIn(r"\sum_{k=0}^{3}C_{k + 5} \cdot x^{k}", solution_TeX)
        self.assertIn(r"\cos(\omega x)", solution_TeX)
        self.assertIn(r"\sin(\omega x)", solution_TeX)

        payload = mars_lab.prepare_diffequation_fields(fields)
        self.assertTrue(payload.get("svg"))

    def test_solver_steps_are_preserved_for_display(self) -> None:
        output = """\
status solved
solver linear transformation
diagnostic linearized by y = u'/u, then solved as u''' = 0
symmetry SL(3, ℝ)
steps Linearising substitutions:
      X = x − 1/y
      Y = x/y − x²/2
Transformed ODE:
      d²Y/dX² = 0
solutions y = final
"""

        fields = mars_lab.parse_diffequation_lab_output(output)
        payload = mars_lab.prepare_diffequation_fields(fields)

        self.assertEqual(payload["symmetry"], "SL(3, ℝ)")
        self.assertIn("Linearising substitutions:", payload["steps"])
        self.assertIn("X = x − 1/y", payload["steps"])
        self.assertIn("Y = x/y − x²/2", payload["steps"])
        self.assertIn("d²Y/dX² = 0", payload["steps"])
        self.assertNotIn("solutions y = final", payload["steps"])
        self.assertIn("function solverTextToTex(text)", mars_lab.INDEX_HTML)
        self.assertIn(
            "data.steps_left_TeX || data.steps_TeX ||",
            mars_lab.INDEX_HTML,
        )
        self.assertIn("solverTextToTex(solverDetails)", mars_lab.INDEX_HTML)

    def test_problem_display_preserves_native_derivative_notation(self) -> None:
        fields = {
            "input": "(y-x)z_x + (y+x)z_y = (x^2+y^2)/z",
            "problem": (
                "{ (y-x)∂z/∂x + (y+x)∂z/∂y = (x^2+y^2)/z "
                "| x = ?, y = ?; ;  }"
            ),
            "problem_TeX": "",
            "solutions": "",
            "solutions_TeX": "",
            "status": "unsupported",
            "solver": "none",
            "diagnostic": "",
        }

        payload = mars_lab.prepare_diffequation_fields(fields)

        self.assertEqual(
            payload["problem"],
            "{ (y-x)∂z/∂x + (y+x)∂z/∂y = (x^2+y^2)/z "
            "| x = ?, y = ?; ;  }",
        )
        self.assertEqual(payload["input"], fields["input"])

    def test_tab_uses_its_own_native_endpoint_and_input_state(self) -> None:
        self.assertIn('data-mode="diffequation"', mars_lab.INDEX_HTML)
        self.assertIn('id="functionCard"', mars_lab.INDEX_HTML)
        self.assertIn(
            "body.diffequation-mode #functionCard",
            mars_lab.INDEX_HTML,
        )
        self.assertIn("fetch('/diffequation-eval'", mars_lab.INDEX_HTML)
        self.assertIn(
            "modeEditorText.diffequation = text;",
            mars_lab.INDEX_HTML,
        )
        self.assertIn(
            "setResultInputText(data.input || text);",
            mars_lab.INDEX_HTML,
        )

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "diffequation_lab").is_file(),
        "release diffequation_lab helper is not built",
    )
    def test_native_helper_solves_and_formats_a_separable_ode(self) -> None:
        completed = subprocess.run(
            [
                str(
                    ROOT
                    / "build"
                    / "release"
                    / "scratch"
                    / "diffequation_lab"
                ),
                "Dx(y) = x*y; y(0) = 1",
            ],
            check=True,
            capture_output=True,
            text=True,
        )

        fields = mars_lab.parse_diffequation_lab_output(completed.stdout)
        payload = mars_lab.prepare_diffequation_fields(fields)

        self.assertEqual(payload["status"], "solved")
        self.assertEqual(payload["solver"], "separable")
        self.assertEqual(payload["solutions"], "y = exp(½x²)")
        self.assertIn(r"\begin{aligned}", payload["solutions_TeX"])
        self.assertIn("y(0) = 1", payload["problem"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "diffequation_lab").is_file(),
        "release diffequation_lab helper is not built",
    )
    def test_native_helper_preserves_and_solves_an_exact_differential_form(
        self,
    ) -> None:
        source = (
            "(sin(theta)-2r cos^2(theta))dr + "
            "r cos(theta)(2r sin(theta)+1)dtheta = 0"
        )
        completed = subprocess.run(
            [
                str(
                    ROOT
                    / "build"
                    / "release"
                    / "scratch"
                    / "diffequation_lab"
                ),
                source,
            ],
            check=True,
            capture_output=True,
            text=True,
        )

        payload = mars_lab.prepare_diffequation_fields(
            mars_lab.parse_diffequation_lab_output(completed.stdout)
        )

        self.assertEqual(payload["status"], "solved")
        self.assertEqual(payload["solver"], "exact first-order")
        self.assertEqual(
            payload["problem"],
            "{ (sin(θ)-2r cos^2(θ))dr + "
            "r cos(θ)(2r sin(θ)+1)dθ = 0 | θ = ?; ;  }",
        )
        self.assertEqual(
            payload["solutions"],
            "r = (sin(θ) - √(sin²(θ) - C·cos²(θ)))/(2·cos²(θ))\n"
            "r = (sin(θ) + √(sin²(θ) - C·cos²(θ)))/(2·cos²(θ))",
        )
        self.assertIn(r"\,dr", payload["problem_TeX"])
        self.assertIn(r"\,d\theta", payload["problem_TeX"])
        self.assertIn(r"\sin \theta", payload["solutions_TeX"])
        self.assertIn(r"\cos^{2} \theta", payload["solutions_TeX"])
        self.assertNotIn(r"\sin(\theta)", payload["solutions_TeX"])
        self.assertNotIn(r"\cos^{2}(\theta)", payload["solutions_TeX"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "diffequation_lab").is_file(),
        "release diffequation_lab helper is not built",
    )
    def test_native_helper_linearizes_the_modified_emden_equation(self) -> None:
        completed = subprocess.run(
            [
                str(
                    ROOT
                    / "build"
                    / "release"
                    / "scratch"
                    / "diffequation_lab"
                ),
                "y'' + 3yy' + y^3 = 0",
            ],
            check=True,
            capture_output=True,
            text=True,
        )

        fields = mars_lab.parse_diffequation_lab_output(completed.stdout)
        payload = mars_lab.prepare_diffequation_fields(fields)

        self.assertEqual(payload["status"], "solved")
        self.assertEqual(payload["solver"], "linear transformation")
        self.assertEqual(
            payload["solutions"],
            "y = (2x + C₁)/(x² + C₁x + C₂)",
        )
        self.assertEqual(payload["symmetry"], "SL(3, ℝ)")
        self.assertIn("X = x − 1/y", payload["steps"])
        self.assertIn("Y = x/y − x²/2", payload["steps"])
        self.assertNotIn("General solution", payload["steps"])
        self.assertIn(r"\frac{2 x + C_{1}}", payload["solutions_TeX"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "diffequation_lab").is_file(),
        "release diffequation_lab helper is not built",
    )
    def test_native_helper_rejects_quartic_emden_point_linearization(self) -> None:
        completed = subprocess.run(
            [
                str(ROOT / "build" / "release" / "scratch" / "diffequation_lab"),
                "y'' + 3*y*y' + y^4 = 0",
            ],
            check=True,
            capture_output=True,
            text=True,
        )

        payload = mars_lab.prepare_diffequation_fields(
            mars_lab.parse_diffequation_lab_output(completed.stdout)
        )

        self.assertEqual(payload["status"], "unsupported")
        self.assertEqual(payload["solver"], "none")
        self.assertEqual(
            payload["diagnostic"],
            "not point-linearizable: the Lie–Tressé invariant "
            "36y(1 − 2y) is not identically zero",
        )

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "diffequation_lab").is_file(),
        "release diffequation_lab helper is not built",
    )
    def test_native_helper_linearizes_scaled_modified_emden_equation(self) -> None:
        completed = subprocess.run(
            [
                str(ROOT / "build" / "release" / "scratch" / "diffequation_lab"),
                "y'' + 6*y*y' + 4*y^3 = 0",
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        payload = mars_lab.prepare_diffequation_fields(
            mars_lab.parse_diffequation_lab_output(completed.stdout)
        )

        self.assertEqual(payload["status"], "solved")
        self.assertEqual(payload["symmetry"], "SL(3, ℝ)")
        self.assertIn("X = x − 1/(2y)", payload["steps"])
        self.assertIn("Y = x/(2y) − x²/2", payload["steps"])
        self.assertIn(r"\text{Recognise the rule:}", payload["steps_TeX"])
        self.assertIn(r"\frac{d^2Y}{dX^2}=0", payload["steps_TeX"])
        self.assertNotIn(r"\middle|", payload["steps_TeX"])
        self.assertNotIn("NAN", payload["steps_TeX"])
        self.assertEqual(
            payload["solutions"],
            "y = (2x + C₁)/(2·(x² + C₁x + C₂))",
        )

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "diffequation_lab").is_file(),
        "release diffequation_lab helper is not built",
    )
    def test_native_helper_solves_power_law_bessel_equation(self) -> None:
        completed = subprocess.run(
            [
                str(ROOT / "build" / "release" / "scratch" / "diffequation_lab"),
                "y'' + x^2*y = 0",
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        payload = mars_lab.prepare_diffequation_fields(
            mars_lab.parse_diffequation_lab_output(completed.stdout)
        )

        self.assertEqual(payload["status"], "solved")
        self.assertEqual(payload["solver"], "power-law Bessel")
        self.assertIn("BesselJ(-¼, ½·x^2)", payload["solutions"])
        self.assertIn(r"J_{-\frac{1}{4}}", payload["solutions_TeX"])
        self.assertIn("y = sqrt(x)*u(z)", payload["steps"])
        self.assertNotIn(r"\middle|", payload["steps_TeX"])
        self.assertNotIn("NAN", payload["steps_TeX"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "diffequation_lab").is_file(),
        "release diffequation_lab helper is not built",
    )
    def test_native_helper_solves_hydrogen_ground_state(self) -> None:
        source = (
            "i*Dt(@psi) = -1/2*(Dxx(@psi) + Dyy(@psi) + Dzz(@psi)) "
            "- @psi/sqrt(x^2+y^2+z^2); "
            "@psi(x,y,z,0) = exp(-sqrt(x^2+y^2+z^2))/sqrt(pi)"
        )
        completed = subprocess.run(
            [str(ROOT / "build" / "release" / "scratch" / "diffequation_lab"), source],
            check=True,
            capture_output=True,
            text=True,
        )
        payload = mars_lab.prepare_diffequation_fields(
            mars_lab.parse_diffequation_lab_output(completed.stdout)
        )

        self.assertEqual(payload["status"], "solved")
        self.assertEqual(payload["solver"], "stationary eigenfunction")
        self.assertIn("derived constant rate", payload["steps"])
        self.assertIn("λ = −H[ψ₀]/(Aψ₀) = 0.5i", payload["steps"])
        self.assertIn(r"\begin{aligned}", payload["steps_TeX"])
        self.assertIn(r"\lambda&=-\frac{H[", payload["steps_TeX"])
        self.assertEqual(
            payload["solutions"],
            "ψ = exp(0.5it - √(x² + y² + z²))/√(π)",
        )

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "diffequation_lab").is_file(),
        "release diffequation_lab helper is not built",
    )
    def test_native_helper_formats_pde_problem_with_partials(self) -> None:
        completed = subprocess.run(
            [
                str(
                    ROOT
                    / "build"
                    / "release"
                    / "scratch"
                    / "diffequation_lab"
                ),
                "2Dx(z) + 3Dy(z) = z; z(1,y) = y",
            ],
            check=True,
            capture_output=True,
            text=True,
        )

        fields = mars_lab.parse_diffequation_lab_output(completed.stdout)
        payload = mars_lab.prepare_diffequation_fields(fields)

        self.assertEqual(
            fields["problem"],
            "{ 2∂z/∂x + 3∂z/∂y = z "
            "| x = ?, y = ?; ; z(1,y) = y }",
        )
        self.assertEqual(payload["problem"], fields["problem"])

        single_completed = subprocess.run(
            [
                str(
                    ROOT
                    / "build"
                    / "release"
                    / "scratch"
                    / "diffequation_lab"
                ),
                "z_y + 2yz = xy^3",
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        single_fields = mars_lab.parse_diffequation_lab_output(
            single_completed.stdout
        )
        self.assertEqual(single_fields["status"], "solved")
        self.assertEqual(
            single_fields["problem"],
            "{ ∂z/∂y + 2yz = xy^3 | y = ?; ;  }",
        )
        self.assertIn(
            r"\frac{\partial z}{\partial y}",
            single_fields["problem_TeX"],
        )

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "diffequation_lab").is_file(),
        "release diffequation_lab helper is not built",
    )
    def test_native_helper_accepts_prime_ode_notation(self) -> None:
        completed = subprocess.run(
            [
                str(
                    ROOT
                    / "build"
                    / "release"
                    / "scratch"
                    / "diffequation_lab"
                ),
                "y'' + 4y = e^x + x^3",
            ],
            check=True,
            capture_output=True,
            text=True,
        )

        fields = mars_lab.parse_diffequation_lab_output(completed.stdout)
        payload = mars_lab.prepare_diffequation_fields(fields)

        self.assertEqual(payload["status"], "solved")
        self.assertIn(
            "d²y/dx² + 4y = e^x + x^3",
            payload["problem"],
        )
        self.assertEqual(
            payload["solutions"],
            "y = ⅕·exp(x) + ¼x³ - ⅜x + "
            "C₁·cos(2x) + C₂·sin(2x)",
        )

        completed = subprocess.run(
            [
                str(
                    ROOT
                    / "build"
                    / "release"
                    / "scratch"
                    / "diffequation_lab"
                ),
                "x'' + x = 0",
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        fields = mars_lab.parse_diffequation_lab_output(completed.stdout)
        payload = mars_lab.prepare_diffequation_fields(fields)

        self.assertEqual(payload["status"], "solved")
        self.assertIn("d²x/dt² + x = 0", payload["problem"])
        self.assertEqual(
            payload["solutions"],
            "x = C₁·cos(t) + C₂·sin(t)",
        )

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "diffequation_lab").is_file(),
        "release diffequation_lab helper is not built",
    )
    def test_native_helper_solves_polynomial_homogeneous_ode(self) -> None:
        completed = subprocess.run(
            [
                str(
                    ROOT
                    / "build"
                    / "release"
                    / "scratch"
                    / "diffequation_lab"
                ),
                "x(x^3-xy^2+2y^3)y' - y(x^3+2y^3) = 0; y(1) = 1",
            ],
            check=True,
            capture_output=True,
            text=True,
        )

        fields = mars_lab.parse_diffequation_lab_output(completed.stdout)
        payload = mars_lab.prepare_diffequation_fields(fields)

        self.assertEqual(payload["status"], "solved")
        self.assertEqual(payload["solver"], "first-order homogeneous")
        self.assertNotIn("NAN", payload["solutions"])
        self.assertEqual(
            payload["solutions"],
            "-½·1/(y/x)² - ln(y/x) + 2·y/x = ln(|x|) + ³⁄₂",
        )

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "diffequation_lab").is_file(),
        "release diffequation_lab helper is not built",
    )
    def test_native_helper_integrates_rational_homogeneous_ode(self) -> None:
        completed = subprocess.run(
            [
                str(
                    ROOT
                    / "build"
                    / "release"
                    / "scratch"
                    / "diffequation_lab"
                ),
                "y(8x-9y) + 2x(x-3y)y' = 0",
            ],
            check=True,
            capture_output=True,
            text=True,
        )

        fields = mars_lab.parse_diffequation_lab_output(completed.stdout)
        payload = mars_lab.prepare_diffequation_fields(fields)

        self.assertEqual(payload["status"], "solved")
        self.assertEqual(payload["solver"], "first-order homogeneous")
        self.assertNotIn("∫", payload["solutions"])
        self.assertEqual(
            payload["solutions"],
            "y = -⅓·(√(x² - 3C/x³) - x)\n"
            "y = ⅓·(√(x² - 3C/x³) + x)",
        )

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "diffequation_lab").is_file(),
        "release diffequation_lab helper is not built",
    )
    def test_native_helper_solves_secant_cubed_forcing(self) -> None:
        completed = subprocess.run(
            [
                str(
                    ROOT
                    / "build"
                    / "release"
                    / "scratch"
                    / "diffequation_lab"
                ),
                "y'' + y = sec^3(x)",
            ],
            check=True,
            capture_output=True,
            text=True,
        )

        fields = mars_lab.parse_diffequation_lab_output(completed.stdout)
        payload = mars_lab.prepare_diffequation_fields(fields)

        self.assertEqual(payload["status"], "solved")
        self.assertEqual(payload["solver"], "constant-coefficient linear")
        self.assertNotIn("integral_meta", payload["solutions"])
        self.assertNotIn("∫", payload["solutions"])
        self.assertEqual(
            payload["solutions"],
            "y = ½·sec(x) + C₁·cos(x) + C₂·sin(x)",
        )

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "diffequation_lab").is_file(),
        "release diffequation_lab helper is not built",
    )
    def test_native_helper_normalizes_bernoulli_constant(self) -> None:
        completed = subprocess.run(
            [
                str(
                    ROOT
                    / "build"
                    / "release"
                    / "scratch"
                    / "diffequation_lab"
                ),
                "y' - 2y = y^2",
            ],
            check=True,
            capture_output=True,
            text=True,
        )

        fields = mars_lab.parse_diffequation_lab_output(completed.stdout)
        payload = mars_lab.prepare_diffequation_fields(fields)

        self.assertEqual(payload["status"], "solved")
        self.assertEqual(payload["solver"], "Bernoulli")
        self.assertNotIn("2C", payload["solutions"])
        self.assertEqual(
            payload["solutions"],
            "y = 2·exp(2x)/(C - exp(2x))",
        )

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "diffequation_lab").is_file(),
        "release diffequation_lab helper is not built",
    )
    def test_native_helper_solves_derivative_quadratic_ode(self) -> None:
        completed = subprocess.run(
            [
                str(
                    ROOT
                    / "build"
                    / "release"
                    / "scratch"
                    / "diffequation_lab"
                ),
                "(y')^2 = y' + 2y",
            ],
            check=True,
            capture_output=True,
            text=True,
        )

        fields = mars_lab.parse_diffequation_lab_output(completed.stdout)
        payload = mars_lab.prepare_diffequation_fields(fields)

        self.assertEqual(payload["status"], "solved")
        self.assertEqual(payload["solver"], "derivative-quadratic")
        self.assertIn(r"\frac{d y}{d x}", payload["problem_TeX"])
        self.assertNotIn(r"\operatorname{D}^{2}", payload["problem_TeX"])
        self.assertIn("x = ?", fields["problem"])
        self.assertNotIn("NAN", payload["problem_TeX"])
        self.assertEqual(
            payload["solutions"].splitlines(),
            [
                "x = ½·(√(8y + 1) - "
                "ln(|½·(√(8y + 1) + 1)|) + 1) + C",
                "x = ½·(1 - √(8y + 1) - "
                "ln(|½·(1 - √(8y + 1))|)) + C",
                "y = 0",
            ],
        )

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "diffequation_lab").is_file(),
        "release diffequation_lab helper is not built",
    )
    def test_native_helper_linearizes_exact_third_order_ode(self) -> None:
        completed = subprocess.run(
            [
                str(
                    ROOT
                    / "build"
                    / "release"
                    / "scratch"
                    / "diffequation_lab"
                ),
                "y''' + y''*y' = 3x^2",
            ],
            check=True,
            capture_output=True,
            text=True,
        )

        fields = mars_lab.parse_diffequation_lab_output(completed.stdout)
        payload = mars_lab.prepare_diffequation_fields(fields)

        self.assertEqual(payload["status"], "solved")
        self.assertEqual(
            payload["solver"], "exact-derivative linearization"
        )
        self.assertEqual(
            payload["solutions"].splitlines(),
            [
                "y = 2·ln(|Σ_(n=0)^∞ c_(n)·x^n|)",
                "c_(0) = C₂",
                "c_(1) = C₃",
                "c_(-1) = 0",
                "c_(-2) = 0",
                "c_(-3) = 0",
                "c_(n + 2) = "
                "(C₁·c_(n) + c_(n - 3))/(2·(n + 2)·(n + 1))",
            ],
        )


class ExpressionResultTests(unittest.TestCase):
    @property
    def expression_binary(self) -> Path:
        return ROOT / "build" / "release" / "scratch" / "mars_lab"

    def test_evaluation_preserves_user_authored_expression_input(self) -> None:
        self.assertIn(
            "expressionWithBindings(editedBody, bindings) || editedBody",
            mars_lab.INDEX_HTML,
        )
        self.assertIn(
            "const updatedSource = replaceBindingValueInExpression(\n"
            "          current,\n"
            "          kind,\n"
            "          name,\n"
            "          valueText\n"
            "        );\n"
            "        if (!updatedSource || updatedSource === current)\n"
            "          return;\n"
            "\n"
            "        fullExpressionText = expressionForEditor(updatedSource).trim();",
            mars_lab.INDEX_HTML,
        )
        self.assertIn(
            "if (data.expression && !data.partial_error)\n"
            "          setExpressionEditor(\n"
            "            editorText || text,\n"
            "            bindingsWithAuthoredValues(data.binding_values, editorText || text),",
            mars_lab.INDEX_HTML,
        )
        self.assertIn("lastEvaluationInputText = text;", mars_lab.INDEX_HTML)

    def test_long_integral_TeX_wraps_between_outer_addends(self) -> None:
        tex = (
            r"\begin{aligned}[t]"
            "\n"
            r"&\frac{1}{8} \cdot \left(4 x - 2 x \cdot "
            r"\ln(x^{4} - x^{2} + 1) - 4 \sqrt{3} x \cdot "
            r"\arctan(\frac{2 x^{2} - 1}{\sqrt{3}}) + "
            r"2 \cdot \left(2 x^{2} + 1\right) \cdot "
            r"\arctan(\frac{x}{1 - x^{2}})\right) \\"
            "\n"
            r"&{} + C"
            "\n"
            r"\end{aligned}"
        )

        wrapped = mars_lab.wrap_rendered_TeX_additive_lines(tex)

        self.assertGreater(wrapped.count(r"\\"), tex.count(r"\\"))
        self.assertIn(r"\ln(x^{4} - x^{2} + 1)", wrapped)
        self.assertIn(r"\frac{2 x^{2} - 1}{\sqrt{3}}", wrapped)
        self.assertIn(r"&\qquad {} - 2 x", wrapped)
        self.assertNotIn(r"\left", wrapped)
        self.assertIn(r"\bigl", wrapped)

    def test_integral_TeX_uses_width_selected_vertical_layout(self) -> None:
        html = mars_lab.INDEX_HTML

        self.assertIn("const integralWrappedTex = data.integral_wrapped_TeX", html)
        self.assertIn("const integralWrappedSvg = data.integral_wrapped_svg", html)
        self.assertIn("/integral\\s+result$/i.test", html)
        self.assertIn("vertically-wrapped-tex", html)
        self.assertIn("overflow-x: hidden", html)
        self.assertIn("overflow-y: auto", html)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_indefinite_integral_uses_plain_constant(self) -> None:
        completed = subprocess.run(
            [
                str(self.expression_binary),
                "cos(2*x)/(sin(x)+cos(x))",
                "x",
                "72",
                "integral",
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        fields = mars_lab.parse_mars_lab_output(completed.stdout)

        self.assertIn("sin(x) + cos(x) + C", fields["integral"])
        self.assertNotIn("C₀", fields["integral"])
        self.assertIn("+ C", fields["integral_TeX"])
        self.assertNotIn("C_{0}", fields["integral_TeX"])

    def test_expression_binding_display_preserves_authored_constants(self) -> None:
        self.assertIn(
            "function bindingsWithAuthoredValues(bindings, sourceExpression) {",
            mars_lab.INDEX_HTML,
        )
        self.assertIn(
            "value: sourceBinding.value,\n"
            "          display: sourceBinding.display",
            mars_lab.INDEX_HTML,
        )
        self.assertIn(
            "applyMarsBindingsToEditedExpression(editedBody, sourceExpression, data);",
            mars_lab.INDEX_HTML,
        )
        self.assertIn(
            "bindingsWithAuthoredValues(\n"
            "            Array.isArray(data.binding_values) ? data.binding_values : [],\n"
            "            data.expression || updated\n"
            "          )",
            mars_lab.INDEX_HTML,
        )

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_symbolic_pi_binding_value_is_preserved(self) -> None:
        expression = "{ atan(x/(1-x^2)) + C | x = pi/2 }"
        completed = subprocess.run(
            [str(self.expression_binary), expression, "x", "64", "bindings"],
            check=True,
            capture_output=True,
            text=True,
        )
        fields = mars_lab.parse_mars_lab_output(completed.stdout)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            fields,
            expression,
            64,
            False,
        )

        self.assertIn("x = π/2", fields["expression"])
        self.assertIn("; C = NAN", fields["expression"])
        self.assertEqual(payload["binding_values"][0]["name"], "x")
        self.assertEqual(payload["binding_values"][0]["value"], "π/2")
        self.assertEqual(payload["binding_values"][0]["display"], "π/2")
        self.assertEqual(payload["binding_values"][1]["name"], "C")
        self.assertEqual(payload["binding_values"][1]["kind"], "constant")

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_bessel_derivative_is_rendered_as_TeX(self) -> None:
        expression = "{ BesselJ(-1/4, x) | x = pi/2 }"
        completed = subprocess.run(
            [str(self.expression_binary), expression, "x", "256", "derivative"],
            check=True,
            capture_output=True,
            text=True,
        )
        fields = mars_lab.parse_mars_lab_output(completed.stdout)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            fields,
            expression,
            256,
            False,
        )

        self.assertEqual(
            payload["derivative_TeX"],
            r"\frac{1}{2} \cdot J_{-\frac{5}{4}}\left(x\right) - "
            r"\frac{1}{2} \cdot J_{\frac{3}{4}}\left(x\right)",
        )
        self.assertTrue(payload.get("derivative_svg"))
        self.assertNotIn("derivative_render_error", payload)

    def test_expression_binding_edit_does_not_recalculate_symbolically(self) -> None:
        binding_commit = mars_lab.INDEX_HTML.split(
            "async function commitBindingInput(input) {", 1
        )[1].split(
            "\n    function commitVisibleBindingInputs()", 1
        )[0]

        expression_branch = binding_commit.split(
            "if (currentMode() === 'expression') {", 1
        )[1].split("\n      }", 1)[0]
        self.assertNotIn("fetchEvaluation(", expression_branch)
        self.assertNotIn("'binding-edit'", expression_branch)
        self.assertIn(
            "fullExpressionText = expressionForEditor(updatedSource).trim();",
            expression_branch,
        )

    def test_calculus_errors_clear_stale_result_cards(self) -> None:
        self.assertGreaterEqual(
            mars_lab.INDEX_HTML.count(
                "clearResultDetails({keepBindings: true});\n"
                "          setRenderedError("
            ),
            2,
        )

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_symbolic_expression_can_be_evaluated_with_unset_values(self) -> None:
        ready_function = mars_lab.INDEX_HTML.split(
            "function expressionReadyToEvaluate() {", 1
        )[1].split("\n    }", 1)[0]

        self.assertIn("return Boolean(currentExpressionText());", ready_function)
        self.assertNotIn("bindingRefreshValid", ready_function)
        self.assertNotIn("evaluationReady", ready_function)

        completed = subprocess.run(
            [
                str(self.expression_binary),
                "z^2 + C_0",
                "z",
                "72",
                "evaluate",
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        fields = mars_lab.parse_mars_lab_output(completed.stdout)

        self.assertEqual(fields["evaluation_ready"], "yes")
        self.assertEqual(fields["value"], "NAN")

        mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            fields,
            "z^2 + C_0",
            72,
            save_expression=False,
            wrt="z",
        )
        self.assertEqual(fields["value"], "?")

    def test_lab_displays_nan_numeric_values_as_unknown(self) -> None:
        for value in ("NAN", "nan", "+NAN", "-nan"):
            self.assertEqual(mars_lab.numeric_value_for_display(value), "?")
        self.assertEqual(mars_lab.numeric_value_for_display("∞"), "∞")
        self.assertEqual(mars_lab.numeric_value_for_display("1 + 2i"), "1 + 2i")

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_literal_integral_is_not_treated_as_a_polynomial(self) -> None:
        completed = subprocess.run(
            [
                str(self.expression_binary),
                "∫^x exp(cosh(t)) dt",
                "x",
                "72",
                "evaluate",
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        fields = mars_lab.parse_mars_lab_output(completed.stdout)

        self.assertEqual(
            fields["unbound"],
            "∫^x exp(cosh(t))·dt",
        )
        self.assertIn(
            "return @S^x exp(cosh(t)) dt;",
            fields["function"],
        )
        self.assertNotIn("integral_meta", completed.stdout)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_expression_integral_budget_meets_requested_precision(self) -> None:
        expected_prefix = (
            "3.282019361716804048203463607517520581316512948427583727395760606137631894"
            "015783088440537589426605283721284926282407788662778821709500932676521399"
            "828735288142794080090483179244993998261773966813476082324682815018690186"
            "492563007652463798476124904692754615156634336326824749216188283034513313"
        )
        completed = subprocess.run(
            [
                str(self.expression_binary),
                "{ ∫^x exp(cosh(t)) dt | x = 1 }",
                "x",
                "320",
                "evaluate",
            ],
            check=True,
            capture_output=True,
            text=True,
            timeout=30,
        )
        fields = mars_lab.parse_mars_lab_output(completed.stdout)

        self.assertTrue(fields["value"].startswith(expected_prefix))

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_builtin_pi_integral_is_not_an_editable_binding(self) -> None:
        expected_prefix = (
            "10392.468738822600980420744462117841678573566419462679673329278087957084"
            "631825897792198285830927860943628524047869766350810032768610623859138947"
            "497641471731730305680684554908473134666515367819801469199656685560045049"
            "151577259888029495308795693406074695607243495921403465215672885517849177"
            "337682115675"
        )
        completed = subprocess.run(
            [
                str(self.expression_binary),
                "∫^@pi exp(cosh(t)) dt",
                "x",
                "347",
                "evaluate",
            ],
            check=True,
            capture_output=True,
            text=True,
            timeout=30,
        )
        fields = mars_lab.parse_mars_lab_output(completed.stdout)
        mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            fields,
            "∫^@pi exp(cosh(t)) dt",
            320,
            save_expression=False,
        )

        self.assertTrue(fields["value"].startswith(expected_prefix))
        self.assertEqual(fields["binding_values"], [])
        self.assertNotIn("binding              constant\tπ", completed.stdout)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_derivative_collects_numeric_polynomial(self) -> None:
        completed = subprocess.run(
            [
                str(self.expression_binary),
                "(x - 1)*(x^2 - 3*x - 10)*(x^2 - 2*x - 3)",
                "x",
                "72",
                "derivative",
            ],
            check=True,
            capture_output=True,
            text=True,
        )

        fields = mars_lab.parse_mars_lab_output(completed.stdout)

        self.assertEqual(
            fields["derivative"],
            "d/dx = { 5x⁴ - 24x³ - 6x² + 72x + 1 | x = NAN }",
        )
        self.assertIn(
            "return 5 * x^4 - 24 * x^3 - 6 * x^2 + 72 * x + 1;",
            fields["derivative_function"],
        )

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_binding_edit_preserves_pi_and_collects_polynomial(self) -> None:
        expression = (
            "2*((x - 1)*((x - 1)*(2*x - 3) - 3*x + x^2 - 10)"
            " + (3*x - 4)*(x^2 - 2*x - 3)"
            " + (x - 1)*((x - 1)*(2*x - 3) - 6*x + 2*x^2 - 20))"
        )
        completed = subprocess.run(
            [
                str(self.expression_binary),
                expression,
                "x",
                "72",
                "binding-edit",
                "pi",
            ],
            check=True,
            capture_output=True,
            text=True,
        )

        fields = mars_lab.parse_mars_lab_output(completed.stdout)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            fields,
            expression,
            72,
            False,
        )

        self.assertEqual(
            fields["expression"],
            "{ 20x³ - 72x² - 12x + 72 | x = π }",
        )
        self.assertEqual(
            payload["full_display_expression"],
            "20x³ - 72x² - 12x + 72",
        )
        self.assertIn(
            "return 20 * x^3 - 72 * x^2 - 12 * x + 72;",
            payload["full_display_function"],
        )
        self.assertIn(
            "x = @pi\noutput(expr(x));",
            payload["full_display_function"],
        )
        self.assertEqual(payload["binding_values"][0]["value"], "π")


class AlmanacLocationTests(unittest.TestCase):
    def test_unmatched_coordinates_do_not_display_the_first_town(self) -> None:
        selected_option = mars_lab.INDEX_HTML.split(
            "function selectedOption() {", 1
        )[1].split("\n      }", 1)[0]

        self.assertIn(
            "return select.selectedOptions[0] || "
            "select.options[select.selectedIndex] || null;",
            selected_option,
        )
        self.assertNotIn("select.options[0]", selected_option)
        self.assertEqual(
            mars_lab.INDEX_HTML.count("placeholder: 'Custom location'"),
            2,
        )

    def test_shrewsbury_town_data_has_observer_coordinates(self) -> None:
        shrewsbury = next(
            town
            for town in mars_lab.JURISDICTION_TOWN_OPTIONS["GB-ENG"]
            if town["name"] == "Shrewsbury"
        )

        self.assertEqual(shrewsbury["latitude"], "52.7077")
        self.assertEqual(shrewsbury["longitude"], "-2.7541")
        self.assertEqual(shrewsbury["elevation"], "75")
        self.assertTrue(shrewsbury["default"])

    def test_annual_events_use_one_native_estimate_and_refine_search(self) -> None:
        searched_kinds: list[str] = []

        def record_search(options: dict[str, str], timeout_seconds=None):
            del timeout_seconds
            searched_kinds.append(options["kind"])
            return []

        with (
            mock.patch.object(
                mars_lab,
                "DEFAULT_ALMANAC_EVENT_BIN",
                ROOT / "Makefile",
            ),
            mock.patch.object(
                mars_lab,
                "run_almanac_event_lab_rows",
                side_effect=record_search,
            ),
        ):
            mars_lab.generate_annual_almanac_events(
                2031,
                2.0,
                "ZA",
                "-33.9258",
                "18.4232",
            )

        self.assertEqual(searched_kinds, ["all"])


# README examples: this class is named to sort after the ordinary regressions
# and deliberately runs the examples documented in docs/mars-lab.md last.
class ZZMarsLabReadmeExamples(unittest.TestCase):
    @unittest.skipUnless(
        all(
            (ROOT / "build" / "release" / "scratch" / name).is_file()
            for name in (
                "mars_lab",
                "equation_lab",
                "diffequation_lab",
                "matrix_lab",
                "integrator_lab",
                "datetime_lab",
                "almanac_lab",
            )
        ),
        "release MARS Lab helpers are not built",
    )
    def test_mars_lab_readme_examples_run_last(self) -> None:
        scratch = ROOT / "build" / "release" / "scratch"

        expression, raw, returncode = mars_lab.run_mars_lab_fields(
            scratch / "mars_lab",
            "{ sin(x)^2 + cos(x)^2 | x = pi/7 }",
            64,
        )
        self.assertEqual(returncode, 0, raw)
        self.assertEqual(expression["value"], "1")

        equation, raw, returncode = mars_lab.run_equation_lab_fields(
            scratch / "equation_lab",
            "atan(2x) + atan(x) = pi/4",
            64,
        )
        self.assertEqual(returncode, 0, raw)
        self.assertEqual(equation["solutions"], "x = ¼·(√(17) - 3)")

        diffequation, raw, returncode = mars_lab.run_diffequation_lab_fields(
            scratch / "diffequation_lab",
            "y'' + x^2y = 0",
        )
        self.assertEqual(returncode, 0, raw)
        self.assertEqual(diffequation["solver"], "power-law Bessel")
        self.assertIn("BesselJ(-¼, ½·x^2)", diffequation["solutions"])

        matrix, raw, returncode = mars_lab.run_matrix_lab_fields(
            scratch / "matrix_lab",
            "sin(1 2; 4 5)",
            "eval",
            53,
        )
        self.assertEqual(returncode, 0, raw)
        self.assertEqual(matrix["operation"], "sin")
        self.assertEqual((matrix["rows"], matrix["cols"]), ("2", "2"))
        self.assertTrue(matrix["result"].startswith("(-0.315002573091184"))

        symbolic_matrix_examples = (
            ("inverse(a b; c d)", "(d/(ad - bc), -b/(ad - bc); -c/(ad - bc), a/(ad - bc))"),
            ("(a b; c d).(e f; g h)", "(ae + bg, af + bh; ce + dg, cf + dh)"),
            ("inverse(a b; c d).(x; y)", "((dx - by)/(ad - bc); (ay - cx)/(ad - bc))"),
            ("Dx(ax+b cx+d; y xy)", "(a, c; 0, y)"),
            ("@S^x((ax+b cx+d; y xy))", "(½·(ax² + 2bx), ½·(cx² + 2dx); xy, ½x²y)"),
        )
        for source, expected in symbolic_matrix_examples:
            with self.subTest(matrix_source=source):
                fields, raw, returncode = mars_lab.run_matrix_lab_fields(
                    scratch / "matrix_lab", source, "eval", 64
                )
                self.assertEqual(returncode, 0, raw)
                self.assertEqual(fields["result"], expected)

        integral, raw, returncode = mars_lab.run_integrator_lab_fields(
            scratch / "integrator_lab",
            "sin^2(x)",
            [{"name": "x", "lo": "0", "hi": "1"}],
            64,
        )
        self.assertEqual(returncode, 0, raw)
        self.assertEqual(integral["antiderivative"], "¼·(2x - sin(2x))")
        self.assertEqual(integral["symbolic"], "¼·(2 - sin(2))")

        datetime, raw, returncode = mars_lab.run_datetime_lab_fields(
            scratch / "datetime_lab",
            {
                "date": "2026-08-08",
                "start": "2026-08-08",
                "end": "2026-08-15",
                "year": "2026",
                "lat": "51.5074",
                "lon": "-0.1278",
                "elevation": "0",
                "gmt_offset": "1",
                "jurisdiction": "GB-ENG",
            },
        )
        self.assertEqual(returncode, 0, raw)
        self.assertEqual(datetime["weekday"], "Saturday")
        self.assertEqual(datetime["sunrise"], "2026-08-08 05:34:54")
        self.assertEqual(datetime["moon_phase"], "Waning Crescent")

        almanac, raw, returncode = mars_lab.run_almanac_lab_fields(
            scratch / "almanac_lab",
            {
                "date": "2026-08-08",
                "time": "09:02:43",
                "zone": "0",
                "lat": "51.5074",
                "lon": "-0.1278",
                "body": "Sun",
            },
        )
        self.assertEqual(returncode, 0, raw)
        self.assertEqual(almanac["selected_name"], "Sun")
        self.assertEqual(almanac["selected_visible"], "YES")
        self.assertIn("SUN|Sun|sun", almanac["snapshot"])

        for mode in (
            "expression",
            "equation",
            "differential-equation",
            "matrix",
            "integrator",
            "datetime",
            "almanac",
        ):
            screenshot = ROOT / "docs" / "images" / "mars-lab" / f"{mode}.png"
            with self.subTest(screenshot=mode):
                self.assertTrue(screenshot.is_file())
                self.assertTrue(screenshot.read_bytes().startswith(b"\x89PNG\r\n\x1a\n"))


if __name__ == "__main__":
    unittest.main()
