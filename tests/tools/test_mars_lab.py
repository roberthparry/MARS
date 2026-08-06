import subprocess
import sys
import unittest
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

import mars_lab


class MobileAccessTests(unittest.TestCase):
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
        self.assertIn("every symbolic solution returned by native MARSlib", help_html)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "diffequation_lab").is_file(),
        "release diffequation_lab helper is not built",
    )
    def test_compact_fourth_order_solution_tex_stays_on_one_line(self) -> None:
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
        solution_tex = fields["solutions_tex"]
        wrapped_tex = fields["solutions_wrapped_tex"]

        self.assertEqual(solution_tex.count(r"\begin{aligned}[t]"), 1)
        self.assertIn(
            r"\left(C_{1} + C_{2} t\right) \cdot e^{\omega t}",
            solution_tex,
        )
        self.assertIn(
            r"\left(C_{3} + C_{4} t\right) \cdot e^{-\omega t}",
            solution_tex,
        )
        self.assertGreater(wrapped_tex.count(r"\begin{aligned}[t]"), 1)

        payload = mars_lab.prepare_diffequation_fields(fields)
        self.assertTrue(payload.get("svg"))
        self.assertTrue(payload.get("wrapped_svg"))

    def test_solution_wrapping_uses_the_available_card_width(self) -> None:
        html = mars_lab.INDEX_HTML

        self.assertIn("function fitDiffequationSolutionToCard()", html)
        self.assertIn("compactWidth > renderedContentWidth() + 1", html)
        self.assertIn("new ResizeObserver", html)
        self.assertIn("data.solutions_wrapped_tex || lastTex", html)
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
        solution_tex = fields["solutions_tex"]

        self.assertIn(r"\frac{d^{6} x}{d t^{6}}", fields["problem_tex"])
        self.assertIn(r"\omega^{6} \cdot x", fields["problem_tex"])
        self.assertNotIn(r"\sum", solution_tex)
        self.assertIn(
            r"\left(C_{1} + C_{2} t + C_{3} t^{2}\right)",
            solution_tex,
        )
        self.assertIn(
            r"\left(C_{4} + C_{5} t + C_{6} t^{2}\right)",
            solution_tex,
        )
        self.assertIn(r"\cos(\omega t)", solution_tex)
        self.assertIn(r"\sin(\omega t)", solution_tex)

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
        solution_tex = fields["solutions_tex"]
        self.assertIn(r"\frac{d^{8} \phi}{d x^{8}}", fields["problem_tex"])
        self.assertNotIn(r"\left[phi\right]", fields["problem_tex"])
        self.assertIn(r"\phi &=", solution_tex)
        self.assertIn(r"\sum_{k=0}^{3}C_{k + 1} \cdot x^{k}", solution_tex)
        self.assertIn(r"\sum_{k=0}^{3}C_{k + 5} \cdot x^{k}", solution_tex)
        self.assertIn(r"\cos(\omega x)", solution_tex)
        self.assertIn(r"\sin(\omega x)", solution_tex)

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
            "data.steps_tex || solverTextToTex(solverDetails)",
            mars_lab.INDEX_HTML,
        )

    def test_problem_display_preserves_native_derivative_notation(self) -> None:
        fields = {
            "input": "(y-x)z_x + (y+x)z_y = (x^2+y^2)/z",
            "problem": (
                "{ (y-x)∂z/∂x + (y+x)∂z/∂y = (x^2+y^2)/z "
                "| x = ?, y = ?; ;  }"
            ),
            "problem_tex": "",
            "solutions": "",
            "solutions_tex": "",
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
        self.assertIn(r"\begin{aligned}", payload["solutions_tex"])
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
        self.assertIn(r"\,dr", payload["problem_tex"])
        self.assertIn(r"\,d\theta", payload["problem_tex"])
        self.assertIn(r"\sin \theta", payload["solutions_tex"])
        self.assertIn(r"\cos^{2} \theta", payload["solutions_tex"])
        self.assertNotIn(r"\sin(\theta)", payload["solutions_tex"])
        self.assertNotIn(r"\cos^{2}(\theta)", payload["solutions_tex"])

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
        self.assertIn(r"\frac{2 x + C_{1}}", payload["solutions_tex"])

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
        self.assertIn(r"\mathrm{SL}(3,\mathrm R)", payload["steps_tex"])
        self.assertIn(r"\frac{d^2Y}{dX^2}=0", payload["steps_tex"])
        self.assertEqual(
            payload["solutions"],
            "y = (x + C₁)/(x² + 2C₁x + 2C₂)",
        )

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
        self.assertEqual(payload["solver"], "hydrogen matrix eigenproblem")
        self.assertIn("u(0) = 0", payload["steps"])
        self.assertIn("E₁ → −13.6057 eV", payload["steps"])
        self.assertIn(r"\begin{aligned}", payload["steps_tex"])
        self.assertIn(r"H_{jj}", payload["steps_tex"])
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
        self.assertIn(r"\frac{d y}{d x}", payload["problem_tex"])
        self.assertNotIn(r"\operatorname{D}^{2}", payload["problem_tex"])
        self.assertIn("y = ?", payload["problem_tex"])
        self.assertNotIn("NAN", payload["problem_tex"])
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

    def test_long_integral_tex_wraps_between_outer_addends(self) -> None:
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

        wrapped = mars_lab.wrap_rendered_tex_additive_lines(tex)

        self.assertGreater(wrapped.count(r"\\"), tex.count(r"\\"))
        self.assertIn(r"\ln(x^{4} - x^{2} + 1)", wrapped)
        self.assertIn(r"\frac{2 x^{2} - 1}{\sqrt{3}}", wrapped)
        self.assertIn(r"&\qquad {} - 2 x", wrapped)
        self.assertNotIn(r"\left", wrapped)
        self.assertIn(r"\bigl", wrapped)

    def test_integral_tex_uses_width_selected_vertical_layout(self) -> None:
        html = mars_lab.INDEX_HTML

        self.assertIn("const integralWrappedTex = data.integral_wrapped_tex", html)
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
        self.assertIn("+ C", fields["integral_tex"])
        self.assertNotIn("C_{0}", fields["integral_tex"])

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


if __name__ == "__main__":
    unittest.main()
