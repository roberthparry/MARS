import datetime as py_datetime
import math
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

import mars_lab


class JurisdictionDefaultTests(unittest.TestCase):
    def test_timezone_defaults_come_from_packaged_jurisdiction_data(self) -> None:
        self.assertEqual(
            mars_lab.jurisdiction_for_timezone(
                "Australia/Sydney",
                mars_lab.JURISDICTION_LOCATION_DEFAULTS,
                mars_lab.JURISDICTION_TOWN_OPTIONS,
            ),
            "AU",
        )
        self.assertEqual(
            mars_lab.jurisdiction_for_timezone(
                "America/Los_Angeles",
                mars_lab.JURISDICTION_LOCATION_DEFAULTS,
                mars_lab.JURISDICTION_TOWN_OPTIONS,
            ),
            "US",
        )
        self.assertEqual(
            mars_lab.jurisdiction_for_timezone(
                "Invalid/Timezone",
                mars_lab.JURISDICTION_LOCATION_DEFAULTS,
                mars_lab.JURISDICTION_TOWN_OPTIONS,
            ),
            "",
        )

    def test_initial_coordinates_match_the_selected_jurisdiction_default(self) -> None:
        location = mars_lab.JURISDICTION_LOCATION_DEFAULTS[mars_lab.DEFAULT_HOLIDAY_JURISDICTION]

        self.assertEqual(mars_lab.DEFAULT_TIMEZONE_LATITUDE, location[0])
        self.assertEqual(mars_lab.DEFAULT_TIMEZONE_LONGITUDE, location[1])


class MobileAccessTests(unittest.TestCase):
    def test_local_holidays_use_available_panel_height_before_page_scrolling(self) -> None:
        html = mars_lab.INDEX_HTML

        self.assertIn(".datetime-local-body {\n      max-height: none;\n      overflow: visible;", html)
        self.assertNotIn(".datetime-local-body {\n      max-height: 20rem;\n      overflow: auto;", html)

    def test_primary_actions_follow_the_editor_without_forcing_panel_height(self) -> None:
        html = mars_lab.INDEX_HTML

        self.assertNotIn("#workspacePanel.viewport-fitted #expr {\n      flex: 1 1 auto;", html)
        self.assertNotIn("#workspacePanel > .controls:not(.derivative-controls) {\n      margin-top: auto;", html)
        self.assertNotIn("#workspacePanel.viewport-fitted,", html)
        self.assertNotIn("--workspace-panel-height", html)
        self.assertNotIn("function fitWorkspacePanelToViewport()", html)

    def test_textarea_resize_grip_appears_only_when_space_runs_out_in_any_mode(self) -> None:
        html = mars_lab.INDEX_HTML

        self.assertIn("textarea.editor-space-limited {\n      resize: vertical;", html)
        self.assertIn("const labTextareas = Array.from(document.querySelectorAll('textarea'));", html)
        self.assertIn("function textareaCanUseConditionalResize(textarea)", html)
        self.assertNotIn("function editorCanUseConditionalResize()", html)
        self.assertIn("const spaceLimited = textarea.scrollHeight > textarea.clientHeight + 1;", html)
        self.assertIn("textarea.classList.toggle('editor-space-limited', spaceLimited);", html)
        self.assertIn("textarea.classList.add('editor-manual-size');", html)
        self.assertIn("labTextareas.forEach((textarea) => expressionEditorResizeObserver.observe(textarea));", html)
        self.assertIn("const maximumTotalExtraHeight = Math.max(96, Math.min(320, window.innerHeight * 0.35));", html)
        self.assertIn("const maximumExtraHeight = maximumTotalExtraHeight / Math.max(1, visibleTextareas.length);", html)
        self.assertIn("const manuallyResized = Math.abs(currentHeight - automaticHeight) > 2;", html)
        self.assertIn("textarea.style.maxHeight = `${automaticHeight + maximumExtraHeight}px`;", html)

    def test_every_scrollable_lab_element_uses_the_MARS_scrollbar_theme(self) -> None:
        html = mars_lab.INDEX_HTML

        self.assertIn("*::-webkit-scrollbar {", html)
        self.assertIn("*::-webkit-scrollbar-track {", html)
        self.assertIn("*::-webkit-scrollbar-thumb {", html)
        self.assertIn("*::-webkit-scrollbar-corner {", html)
        self.assertIn("scrollbar-color: rgba(227, 180, 87, 0.72) rgba(8, 29, 22, 0.62);", html)

    def test_all_buttons_use_the_delegated_MARS_tooltip(self) -> None:
        html = mars_lab.INDEX_HTML

        self.assertIn(".mars-button-tooltip {", html)
        self.assertIn("function buttonTooltipText(button)", html)
        self.assertIn("document.addEventListener('pointerover'", html)
        self.assertIn("document.addEventListener('focusin'", html)
        self.assertIn("return target && typeof target.closest === 'function' ? target.closest('button') : null;", html)

    def test_value_card_wraps_long_unbroken_numbers(self) -> None:
        self.assertIn(
            "#value {\n"
            "      min-width: 0;\n"
            "      max-width: 100%;\n"
            "      overflow-x: hidden;\n"
            "      overflow-y: auto;\n"
            "      white-space: pre-wrap;\n"
            "      overflow-wrap: anywhere;\n"
            "      word-break: break-word;\n"
            "    }",
            mars_lab.INDEX_HTML,
        )

    def test_result_panel_does_not_clip_the_value_card(self) -> None:
        self.assertIn(
            "#resultWorkspacePanel {\n"
            "      min-height: min-content;\n"
            "      overflow: visible;\n"
            "    }",
            mars_lab.INDEX_HTML,
        )
        self.assertIn("valueCard.toggleAttribute('hidden', !visible);", mars_lab.INDEX_HTML)
        self.assertIn("valueCard.style.setProperty('display', 'block', 'important');", mars_lab.INDEX_HTML)
        self.assertLess(
            mars_lab.INDEX_HTML.index('id="functionCard"'),
            mars_lab.INDEX_HTML.index('id="valueCard"'),
        )

    def test_function_cards_apply_safe_syntax_colouring(self) -> None:
        html = mars_lab.INDEX_HTML

        self.assertIn("function renderMarsFunctionSyntax(element, text)", html)
        self.assertIn("'function-token-keyword'", html)
        self.assertIn("'function-token-variable'", html)
        self.assertIn("' function-token-array'", html)
        self.assertIn("'function-token-function'", html)
        self.assertIn("'function-token-bracket'", html)
        self.assertIn("'function-token-number'", html)
        self.assertIn("'function-token-constant'", html)
        self.assertIn("'function-token-comment'", html)
        self.assertIn("fragment.appendChild(document.createTextNode(text));", html)
        self.assertIn("token.textContent = text;", html)
        self.assertIn("element.replaceChildren(fragment);", html)
        self.assertIn("element === functionStyle && looksLikeMarsFunction(text)", html)

    def test_function_syntax_colouring_understands_MARS_comments(self) -> None:
        html = mars_lab.INDEX_HTML

        self.assertIn("source.startsWith('``', index)", html)
        self.assertIn("source[index] === '`'", html)
        self.assertIn("source.startsWith('$[', index)", html)
        self.assertIn("appendFunctionToken(fragment, source.slice(index, next), 'function-token-variable');", html)
        self.assertIn("source[index] === '[' || source[index] === ']'", html)
        self.assertIn("'array', 'const', 'equation', 'expression', 'i', 'matrix', 'return'", html)
        self.assertIn("(?:equation|expression|matrix)", html)
        self.assertIn("const numberMatch = source.slice(index).match", html)

    def test_function_syntax_colouring_uses_the_semantic_palette(self) -> None:
        html = mars_lab.INDEX_HTML

        self.assertIn(
            ".function-token-variable {\n      color: #c7d0cb;\n      font-style: italic;",
            html,
        )
        self.assertIn(
            ".function-token-array {\n      text-decoration: underline;\n      text-underline-offset: 0.16em;",
            html,
        )
        self.assertIn("functionArrayVariables.has(identifier)", html)
        self.assertIn(".function-token-function {\n      color: #72ddd0;\n      font-weight: 700;", html)
        self.assertIn(".function-token-bracket {\n      color: #f3bd68;\n      font-weight: 400;", html)
        self.assertIn(".function-token-number {\n      color: #e78fcb;", html)
        self.assertIn(".function-token-constant {\n      color: #dca3cf;", html)
        self.assertIn("const namedConstantMatch = source.slice(index).match", html)
        self.assertIn(".function-token-comment {\n      color: #83d49b;", html)
        self.assertIn("const functionOpeningBrackets = new Set();", html)
        self.assertIn("const functionBracketStack = [];", html)
        self.assertIn("functionOpeningBrackets.add(following);", html)
        self.assertIn("isFunctionBracket ? 'function-token-bracket' : ''", html)

    def test_function_source_comment_precedes_the_function(self) -> None:
        function = "expression expr() {\n    return root(1 + i, 4).\n}\n\noutput(expr())."

        self.assertEqual(
            mars_lab.function_with_source_comment(function, "root(1+i,4)"),
            "` root(1+i,4) `\n" + function,
        )

    def test_function_source_comment_uses_mathematical_sum_and_product_symbols(self) -> None:
        function = "expression expr() {\n    return 1.\n}\n\noutput(expr())."

        self.assertTrue(
            mars_lab.function_with_source_comment(function, "@Z_(k=1)^n k").startswith("` Σ_(k=1)^n k `\n")
        )
        self.assertTrue(
            mars_lab.function_with_source_comment(function, "@P_(k=1)^n k").startswith("` Π_(k=1)^n k `\n")
        )

    def test_rendered_TeX_uses_a_slightly_larger_default_scale(self) -> None:
        self.assertIn("--render-base-scale: 1.35;", mars_lab.INDEX_HTML)
        self.assertIn("--render-base-scale: 1.05;", mars_lab.INDEX_HTML)

    def test_calendar_opens_below_its_field_without_covering_the_page_header(self) -> None:
        self.assertIn("const top = anchorBottom + 8;", mars_lab.INDEX_HTML)
        self.assertIn("marsDatePicker.style.maxHeight = `${availableHeight}px`;", mars_lab.INDEX_HTML)
        self.assertNotIn("rect.top - pickerRect.height - 8", mars_lab.INDEX_HTML)

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
    def test_expression_state_preserves_series_ellipsis(self) -> None:
        source = "1 + 1/2^2 + 1/3^2 + ... + 1/100000000^2"

        with tempfile.TemporaryDirectory() as directory:
            with mock.patch.object(mars_lab, "STATE_FILE", Path(directory) / "state.json"):
                mars_lab.save_state_data({"expression": source, "expression_updated_at": 123456})
                mars_lab.save_state_data({"expression": "stale expression", "expression_updated_at": 123455})
                mars_lab.save_state_data({"lab_mode": "expression"})

                self.assertEqual(mars_lab.load_state_expression(), source)
                self.assertEqual(mars_lab.load_state_data()["expression_updated_at"], 123456)

                mars_lab.save_state_data({"expression": "new expression", "expression_updated_at": 123457})
                self.assertEqual(mars_lab.load_state_expression(), "new expression")

        self.assertIn("if (saved) {", mars_lab.INDEX_HTML)
        self.assertIn("modeEditorText.expression = saved;", mars_lab.INDEX_HTML)
        self.assertIn("saveLastExpression(expr.value.trim(), {debounce: true});", mars_lab.INDEX_HTML)
        self.assertIn("localExpressionUpdatedAt > serverExpressionUpdatedAt", mars_lab.INDEX_HTML)
        self.assertIn("expression_updated_at: updatedAt", mars_lab.INDEX_HTML)
        self.assertIn("expression_updated_at: lastExpressionUpdatedAt", mars_lab.INDEX_HTML)
        self.assertIn("persist_expression: currentMode() === 'expression'", mars_lab.INDEX_HTML)
        self.assertIn("saveLastExpression(editorText || text);", mars_lab.INDEX_HTML)
        self.assertNotIn("saved && !saved.includes('...')", mars_lab.INDEX_HTML)

    def test_equation_state_preserves_polynomial_series_ellipsis(self) -> None:
        source = "x + 17x + 83x + 259x + ... + 10009x = 10000"

        with tempfile.TemporaryDirectory() as directory:
            with mock.patch.object(mars_lab, "STATE_FILE", Path(directory) / "state.json"):
                mars_lab.save_state_data({"equation": source, "equation_updated_at": 123456})
                mars_lab.save_state_data({"lab_mode": "equation"})

                self.assertEqual(mars_lab.load_state_data()["equation"], source)
                self.assertEqual(mars_lab.load_state_data()["equation_updated_at"], 123456)

        self.assertIn(
            "if (savedEquation)\n        modeEditorText.equation = expressionWithSortedConstants(savedEquation);",
            mars_lab.INDEX_HTML,
        )
        self.assertIn(
            'equation = str(payload.get("equation", "")).strip()\n                if equation:\n'
            '                    updates["equation"] = equation',
            (ROOT / "tools" / "mars_lab.py").read_text(encoding="utf-8"),
        )
        self.assertIn(
            "if (equationText)\n          modeEditorText.equation = expressionWithSortedConstants(equationText);",
            mars_lab.INDEX_HTML,
        )
        self.assertIn("saveLastEquationState({debounce: true});", mars_lab.INDEX_HTML)
        self.assertIn("window.addEventListener('pagehide'", mars_lab.INDEX_HTML)
        self.assertIn("keepalive: !!options.keepalive", mars_lab.INDEX_HTML)
        self.assertIn("localEquationUpdatedAt > serverEquationUpdatedAt", mars_lab.INDEX_HTML)

    def test_function_card_replaces_backtick_binding_hint(self) -> None:
        function = "equation equ(x) {\n    return equation(x = 1).\n}\n\n`` x = ?\noutput(equ(x).solve())."
        bindings = [{"name": "x", "kind": "variable", "value": "3"}]

        self.assertEqual(
            mars_lab.function_for_result_card(function, bindings),
            "equation equ(x) {\n    return equation(x = 1).\n}\n\nx = 3.\noutput(equ(x).solve()).",
        )

    def test_function_card_uses_typeable_infinity_for_constant_binding(self) -> None:
        function = "expression expr(const n) {\n    return n.\n}\n\nconst n = ?.\noutput(expr(n))."
        bindings = [{"name": "n", "kind": "constant", "value": "inf"}]

        self.assertIn("const n = @inf.", mars_lab.function_for_result_card(function, bindings))

    def test_use_as_input_reads_the_equation_card(self) -> None:
        self.assertIn(
            "if (currentMode() === 'equation')\n"
            "        return parsedExpressionText();",
            mars_lab.INDEX_HTML,
        )
        self.assertIn(
            "if (currentMode() === 'equation' || currentMode() === 'diffequation')\n"
            "        setExpressionEditor(resultText);\n"
            "      else if (!await applyMarsBindingExpression(resultText, resultText))",
            mars_lab.INDEX_HTML,
        )
        self.assertIn(
            "setResultInputText(data.editor_expression || data.full_display_expression || data.expression || '');",
            mars_lab.INDEX_HTML,
        )

    def test_multiline_numeric_solutions_reach_payload(self) -> None:
        raw = "\n".join(
            (
                "input       x^2 - 1 = 0",
                "equation    { x² - 1 = 0 | x = NAN }",
                "unbound     x² - 1 = 0",
                "function    equation equ(x) {",
                "    return equation(x^2 - 1 = 0).",
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
            "equation equ(x) {\n    return equation(x^2 - 1 = 0).\n}",
        )
        self.assertEqual(
            payload["solutions"].splitlines(),
            ["x = 1", "x = -1"],
        )
        self.assertEqual(
            payload["numeric_solutions"],
            ["x ≈ 1", "x ≈ -1"],
        )

    def test_numeric_solution_omits_subprecision_imaginary_zero(self) -> None:
        fields = {
            "numeric": "m ≈ 2 + 0E-77i  (n = 0)\n"
            "m ≈ 2.19615871138938 + 3.09867129454835i  (n = 0)"
        }

        self.assertEqual(
            mars_lab.equation_lab_numeric_solution_lines(fields, 78),
            [
                "m ≈ 2  (n = 0)",
                "m ≈ 2.19615871138938 + 3.09867129454835i  (n = 0)",
            ],
        )

    def test_solution_pane_recognises_exact_fraction_literals(self) -> None:
        html = mars_lab.INDEX_HTML

        self.assertIn("[⁰¹²³⁴⁵⁶⁷⁸⁹]+⁄[₀₁₂₃₄₅₆₇₈₉]+", html)
        self.assertIn("[¼½¾⅐⅑⅒⅓⅔⅕⅖⅗⅘⅙⅚⅛⅜⅝⅞]", html)
        self.assertIn("const scalar = `(?:${number}|${fraction})`;", html)

    def test_equation_function_card_preserves_source_line_breaks(self) -> None:
        equation_handler = mars_lab.INDEX_HTML.split("async function evaluateEquation", 1)[1].split(
            "async function evaluateDiffequation", 1
        )[0]

        self.assertNotIn("functionStyle.classList.add('equation-function')", equation_handler)
        self.assertIn("setExpandableText(\n            functionStyle,", equation_handler)

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

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "equation_lab").is_file(),
        "release equation_lab helper is not built",
    )
    def test_repeated_power_complex_families_keep_one_TeX_row_per_solution(self) -> None:
        equation_binary = ROOT / "build" / "release" / "scratch" / "equation_lab"
        completed = subprocess.run(
            [str(equation_binary), "2^(3m) + 2^(2m) + 2^m = 84", "256"],
            check=True,
            capture_output=True,
            text=True,
        )

        fields = mars_lab.parse_equation_lab_output(completed.stdout)
        rows = fields["solutions_TeX"].removeprefix(r"\begin{aligned} ").removesuffix(r" \end{aligned}").split(" \\\\\n")

        self.assertEqual(len(rows), 3)
        self.assertTrue(all(row.startswith("m &=") for row in rows))
        self.assertTrue(all(" + i" in row for row in rows))
        self.assertNotIn("\\\\\n&{} +", fields["solutions_TeX"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "equation_lab").is_file(),
        "release equation_lab helper is not built",
    )
    def test_equation_mode_solves_additive_algebraic_sequence_ellipsis(self) -> None:
        equation_binary = ROOT / "build" / "release" / "scratch" / "equation_lab"
        cases = (
            ("x + 2x + 3x + ... + 10x = 100", "55x = 100", "x = ²⁰⁄₁₁", r"\sum_{n=1}^{10}"),
            ("x + 3x + ... + 9x = 25", "25x = 25", "x = 1", r"\sum_{n=1}^{5}"),
            (
                "x+4x+7x+10x+13x+16x+19x+22x+...+64x = 1430",
                "715x = 1430",
                "x = 2",
                r"\sum_{n=1}^{22}",
            ),
            ("x + 4x + 9x + 16x + ... + 100x = 20", "385x = 20", "x = ⁴⁄₇₇", r"\sum_{n=1}^{10}"),
            ("x + 8x + 27x + ... + 1000x = 2000", "3025x = 2000", "x = ⁸⁰⁄₁₂₁", r"\sum_{n=1}^{10}"),
            (
                "x + 8x + 27x + 64x + ... + 1000x = 2000",
                "3025x = 2000",
                "x = ⁸⁰⁄₁₂₁",
                r"\sum_{n=1}^{10}",
            ),
            (
                "x + 16x + 81x + 256x + ... + 10000x = 2000",
                "25333x = 2000",
                "x = ²⁰⁰⁰⁄₂₅₃₃₃",
                r"\sum_{n=1}^{10}",
            ),
            (
                "x + 17x + 83x + 259x + ... + 10009x = 10000",
                "25378x = 10000",
                "x = ⁵⁰⁰⁰⁄₁₂₆₈₉",
                r"\sum_{n=1}^{10}",
            ),
            (
                "x + 21x + 92x + 275x + ... + 10109x = 10000",
                "25776x = 10000",
                "x = ⁶²⁵⁄₁₆₁₁",
                r"\sum_{n=1}^{10}",
            ),
            (
                "x + x/2 + x/3 + x/4 + ... + x/10 = 1",
                "⁷³⁸¹⁄₂₅₂₀x = 1",
                "x = ²⁵²⁰⁄₇₃₈₁",
                r"\sum_{n=1}^{10}",
            ),
            ("x + 2x + 4x + ... + 64x = 127", "127x = 127", "x = 1", r"\sum_{n=1}^{7}"),
        )

        for source, expected_equation, expected_solution, expected_sum in cases:
            with self.subTest(source=source):
                fields, raw, returncode = mars_lab.run_equation_lab_fields(equation_binary, source, 64)

                self.assertEqual(returncode, 0, raw)
                self.assertEqual(fields["unbound"], expected_equation)
                self.assertEqual(fields["solutions"], expected_solution)
                self.assertIn(expected_sum, fields["tex"])
                self.assertIn(expected_sum, fields["derivation_TeX"])
                self.assertIn("x &=", fields["derivation_TeX"])
                self.assertLess(
                    fields["derivation_TeX"].index(expected_sum),
                    fields["derivation_TeX"].index("x &=")
                )

                payload = mars_lab.prepare_equation_fields(fields, 64)
                self.assertEqual(payload["display_TeX"], fields["derivation_TeX"])


class MatrixResultTests(unittest.TestCase):
    def test_rendered_tex_svg_ids_are_isolated_between_cards(self) -> None:
        first = (
            "<svg><defs><path id='g2-50' d='first'/></defs>"
            "<use xlink:href='#g2-50'/></svg>"
        )
        second = (
            "<svg><defs><path id='g2-50' d='second'/></defs>"
            "<use xlink:href='#g2-50'/></svg>"
        )

        isolated_first = mars_lab._namespace_svg_ids(first)
        isolated_second = mars_lab._namespace_svg_ids(second)
        first_id = re.search(r"id='([^']+)'", isolated_first)
        second_id = re.search(r"id='([^']+)'", isolated_second)

        self.assertIsNotNone(first_id)
        self.assertIsNotNone(second_id)
        self.assertNotEqual(first_id.group(1), second_id.group(1))
        self.assertIn(f"xlink:href='#{first_id.group(1)}'", isolated_first)
        self.assertIn(f"xlink:href='#{second_id.group(1)}'", isolated_second)

    def test_matrix_determinant_bar_is_not_treated_as_a_binding_separator(self) -> None:
        html = mars_lab.INDEX_HTML

        self.assertIn("if (!bindings || indexOfTopLevel(bindings, '=') < 0)\n        return null;", html)

    def test_matrix_editor_keeps_discovered_unset_bindings_out_of_hidden_input(self) -> None:
        html = mars_lab.INDEX_HTML

        self.assertIn("setExpressionEditor(editorBody, matrixBindings, editorBody);", html)
        self.assertIn("if (currentMode() === 'matrix' && !bindingParts(current))", html)
        self.assertIn(".filter((binding) => binding.name && binding.value);", html)

    def test_scalar_matrix_result_omits_single_unset_variable_wrapper(self) -> None:
        fields = {
            "input": "|(1 2; 3 4) - (lambda 0; 0 lambda)|",
            "operation": "det",
            "result": "(1 - λ)·(4 - λ) - 6",
            "expression": "{ (1 - λ)·(4 - λ) - 6 | λ = ? }",
            "tex": r"\left(1 - \lambda\right) \cdot \left(4 - \lambda\right) - 6",
            "bindings": "variable\tλ\tNAN",
        }

        payload = mars_lab.prepare_matrix_fields(fields, 32)

        self.assertEqual(payload["result"], "(1 - λ)·(4 - λ) - 6")
        self.assertTrue(payload["scalar"])
        self.assertNotIn(r"\middle|", payload["tex"])
        self.assertNotIn(r"\left\{", payload["tex"])
        self.assertEqual(
            payload["binding_values"],
            [{"name": "λ", "value": "NAN", "display": "", "kind": "variable"}],
        )

    def test_scalar_matrix_result_omits_multiple_unset_constant_bindings(self) -> None:
        fields = {
            "input": "trace(a 0; 0 d)",
            "operation": "trace",
            "result": "a+d",
            "expression": "{ a+d | ; a = ?, d = ? }",
            "pretty": "a+d",
            "tex": "a+d",
            "bindings": "constant\ta\tNAN\nconstant\td\tNAN",
        }

        payload = mars_lab.prepare_matrix_fields(fields, 32)

        self.assertEqual(payload["result"], "a+d")
        self.assertEqual(payload["pretty"], "a+d")
        self.assertEqual(payload["tex"], "a+d")
        self.assertEqual([binding["name"] for binding in payload["binding_values"]], ["a", "d"])

    def test_resolved_scalar_matrix_result_includes_its_evaluated_value(self) -> None:
        fields = {
            "input": "{ |(1 2; 3 4) - (lambda 0; 0 lambda)| | lambda = 4 }",
            "operation": "det",
            "result": "(1 - λ)·(4 - λ) - 6",
            "expression": "{ (1 - λ)·(4 - λ) - 6 | λ = 4 }",
            "value": "-6",
            "bindings": "variable\tλ\t4",
        }

        payload = mars_lab.prepare_matrix_fields(fields, 32)

        self.assertEqual(payload["result"], "(1 - λ)·(4 - λ) - 6")
        self.assertEqual(payload["value"], "-6")
        self.assertIn("setValueText(data.value || '');", mars_lab.INDEX_HTML)
        self.assertIn("setValueCardVisible(!!data.value);", mars_lab.INDEX_HTML)

    def test_resolved_matrix_result_includes_an_evaluated_value_matrix(self) -> None:
        fields = {
            "input": "{ (1 2; 3 4) - (lambda 0; 0 lambda) | lambda = 3 }",
            "operation": "eval",
            "kind": "expr",
            "rows": "2",
            "cols": "2",
            "result": "(1 - λ, 2; 3, 4 - λ)",
            "expression_pretty": "{ (\n\t1 - λ,\t2;\n\t3,\t4 - λ\n) | λ = 3 }",
            "pretty": "(\n  1 - λ     2\n      3 4 - λ\n)",
            "value": "(-2, 2; 3, 1)",
            "value_pretty": "(\n  -2 2\n   3 1\n)",
            "value_tex": r"\begin{bmatrix}-2 & 2 \\ 3 & 1\end{bmatrix}",
            "bindings": "variable\tλ\t3",
        }

        payload = mars_lab.prepare_matrix_fields(fields, 32)

        self.assertEqual(payload["result"], "(1 - λ, 2; 3, 4 - λ)")
        self.assertEqual(payload["value"], "(-2, 2; 3, 1)")
        self.assertEqual(payload["value_pretty"], "(\n  -2 2\n   3 1\n)")
        self.assertEqual(payload["expression_pretty"], fields["expression_pretty"])
        self.assertEqual(payload["value_TeX"], fields["value_tex"])
        self.assertIn("<svg", payload["value_svg"])
        self.assertIn("setMatrixValueResult(data);", mars_lab.INDEX_HTML)
        self.assertIn("valueTitle.textContent = 'Value';", mars_lab.INDEX_HTML)

    def test_matrix_value_card_renders_the_native_numeric_tex(self) -> None:
        self.assertIn(
            "#value.matrix-tex-value {\n"
            "      overflow: auto;\n"
            "      white-space: normal;",
            mars_lab.INDEX_HTML,
        )
        self.assertIn("const svg = String(data.value_svg || '');", mars_lab.INDEX_HTML)
        self.assertIn("frame.innerHTML = svg;", mars_lab.INDEX_HTML)

    def test_matrix_value_tex_fits_the_card_at_default_zoom(self) -> None:
        html = mars_lab.INDEX_HTML

        self.assertIn("#value.matrix-tex-value .rendered-zoom-frame {\n      display: inline-block;\n      min-width: 0;", html)
        self.assertIn("const matrixValue = frame.parentElement && frame.parentElement.matches('#value.matrix-tex-value')", html)
        self.assertIn("appliedScale = Math.min(baseScale, availableWidth / width) * zoom;", html)
        self.assertIn("svg.style.transform = `scale(${appliedScale})`;", html)

    def test_matrix_result_uses_distinct_native_expression_and_function_cards(self) -> None:
        fields = {
            "input": "{ (x c; d x^2) | x = 2; c = 3, d = 4 }",
            "operation": "eval",
            "kind": "expr",
            "rows": "2",
            "cols": "2",
            "result": "(x, c; d, x²)",
            "expression": "{ (x, c; d, x²) | x = 2; c = 3, d = 4 }",
            "expression_pretty": "{ (\n\tx,\tc;\n\td,\tx²\n) | x = 2; c = 3, d = 4 }",
            "function": (
                "matrix mat(x, const c, const d) {\n"
                "    return (x, c; d, x^2).\n"
                "}\n\n"
                "x = 2.\nconst c = 3.\nconst d = 4.\noutput(mat(x, c, d))."
            ),
            "tex": r"\begin{bmatrix}x & c \\ d & x^{2}\end{bmatrix}",
            "bindings": "variable\tx\t2\nconstant\tc\t3\nconstant\td\t4",
        }

        payload = mars_lab.prepare_matrix_fields(fields, 32)

        self.assertEqual(payload["expression"], fields["expression"])
        self.assertEqual(payload["function"], fields["function"])
        self.assertIn("setResultTitles('Rendered TeX', 'Expression', 'Function', 'Value');", mars_lab.INDEX_HTML)
        self.assertIn("function setMatrixExpressionResult(data)", mars_lab.INDEX_HTML)
        self.assertIn("data.expression_pretty || data.expression || data.result", mars_lab.INDEX_HTML)
        self.assertIn("setExpandableText(parsed, parsedMore, displayExpression, fullExpression);", mars_lab.INDEX_HTML)
        self.assertIn("data.display_expression || data.expression", mars_lab.INDEX_HTML)
        self.assertIn("data.display_function || data.function", mars_lab.INDEX_HTML)
        matrix_evaluation = mars_lab.INDEX_HTML.split(
            "async function evaluateMatrix(options = {}) {", 1
        )[1].split("\n    async function evaluateEquation", 1)[0]
        self.assertNotIn("setMatrixPrettyResult(data.display_result", matrix_evaluation)

    def test_matrix_expression_card_uses_readable_native_plain_text(self) -> None:
        html = mars_lab.INDEX_HTML

        self.assertIn("#parsed.matrix-expression-text {", html)
        self.assertIn("white-space: pre-wrap;", html)
        self.assertIn("tab-size: 4;", html)
        self.assertNotIn("function renderMatrixExpressionResult", html)

    def test_expanded_matrix_result_card_uses_the_complete_workspace_width(self) -> None:
        html = mars_lab.INDEX_HTML

        self.assertIn('<main id="labWorkspace">', html)
        self.assertIn('<section id="resultWorkspacePanel">', html)
        self.assertIn(
            "body.matrix-mode #labWorkspace.result-card-expanded > #workspacePanel {\n"
            "      display: none;\n"
            "    }",
            html,
        )
        self.assertIn(
            "body.matrix-mode #labWorkspace.result-card-expanded > #resultWorkspacePanel {\n"
            "      grid-column: 1 / -1;\n"
            "    }",
            html,
        )
        self.assertIn("labWorkspace.classList.add('result-card-expanded');", html)
        self.assertIn("labWorkspace.classList.remove('result-card-expanded');", html)
        self.assertIn("document.body.classList.toggle('matrix-mode', matrixMode);", html)

    def test_matrix_function_card_uses_the_shared_function_syntax_highlighter(self) -> None:
        html = mars_lab.INDEX_HTML

        self.assertIn("(?:equation|expression|matrix)", html)
        self.assertIn("'matrix'", html)
        self.assertIn("element === functionStyle && looksLikeMarsFunction(text)", html)

    def test_matrix_scalar_calculus_uses_the_expression_backend(self) -> None:
        html = mars_lab.INDEX_HTML

        self.assertIn("lastMatrixScalarExpression = data.scalar ? (data.result || '') : '';", html)
        self.assertIn("await takeMatrixScalarCalculus(wrt, action, actionButton);", html)
        self.assertIn("const {response, data} = await fetchEvaluation(", html)
        self.assertIn("? integralExpressionFromLine(data.integral)", html)
        self.assertIn("const variables = [...currentVariables];", html)
        self.assertIn("currentVariables = variables;\n        currentDifferentiable = differentiable;", html)

    def test_generated_matrix_integral_request_is_recovered_as_the_original_input(self) -> None:
        self.assertEqual(
            mars_lab.recover_generated_matrix_calculus_input("@S(1 2; 3 4)^xdx"),
            "(1 2; 3 4)^x",
        )
        self.assertEqual(
            mars_lab.recover_generated_matrix_calculus_input("@S(1 2; 3 4)dx"),
            "(1 2; 3 4)",
        )
        self.assertEqual(
            mars_lab.recover_generated_matrix_calculus_input("(1 2; 3 4)^x"),
            "(1 2; 3 4)^x",
        )

    def test_matrix_mode_exposes_native_derivative_and_integral_actions(self) -> None:
        html = mars_lab.INDEX_HTML

        self.assertIn("derivativeButtons.classList.toggle('hidden', !expressionMode && !matrixMode);", html)
        self.assertIn("await takeMatrixCalculus(wrt, 'derivative', actionButton);", html)
        self.assertIn("await takeMatrixCalculus(wrt, 'integral', actionButton);", html)
        self.assertIn("? `@S${source}d${wrt}`\n        : `D${wrt}(${source})`;", html)
        self.assertIn("const sourceText = String(currentExpressionText() || '').trim();", html)
        self.assertIn("const text = expressionBodyForEditor(sourceText);", html)
        self.assertIn("const calculusBody = matrixCalculusInput(text, wrt, action);", html)
        self.assertIn("? `${wrt} integral RESULT`", html)
        self.assertIn(
            "const calculusText = expressionWithBindings(calculusBody, compactExpressionForEditor(sourceText).bindings);",
            html,
        )
        self.assertNotIn(
            "const text = String(resultUseInput.dataset.inputText || currentExpressionText() || '').trim();", html
        )
        self.assertLess(html.index("`${name} derivative`"), html.index("`${name} integral`"))

    def test_use_as_input_restores_the_native_matrix_result_without_scalar_parsing(self) -> None:
        self.assertIn(
            "if (currentMode() === 'matrix') {\n"
            "        const sourceBindings = compactExpressionForEditor(currentExpressionText()).bindings || [];\n"
            "        const bindings = resultInputBindings.length\n"
            "          ? bindingsWithAuthoredValues(resultInputBindings, expressionWithBindings(resultText, sourceBindings))\n"
            "          : sourceBindings;\n"
            "        setExpressionEditor(expressionWithBindings(resultText, bindings), bindings, resultText);\n"
            "        matrixOperation.value = 'eval';\n"
            "        matrixOperand.value = '';",
            mars_lab.INDEX_HTML,
        )

    def test_matrix_calculus_preserves_result_only_integration_constants_for_use_as_input(self) -> None:
        html = mars_lab.INDEX_HTML

        self.assertIn("let resultInputBindings = [];", html)
        self.assertIn("function setResultInputText(text, bindings = null)", html)
        self.assertIn("setResultInputText(data.result || '', data.binding_values || []);", html)
        self.assertIn("transient: !!options.skipSave", html)
        self.assertIn(
            "if not transient:\n                save_state_data({",
            (ROOT / "tools" / "mars_lab.py").read_text(encoding="utf-8"),
        )

    def test_matrix_integral_payload_transports_native_antiderivative_and_constant_matrix(self) -> None:
        fields = {
            "input": "{ @S (x, 2x; 3x, 4x) dx | x = ? }",
            "operation": "eval",
            "kind": "expr",
            "rows": "2",
            "cols": "2",
            "result": "(½x², x²; 3/2x², 2x²) + (C₁₁, C₁₂; C₂₁, C₂₂)",
            "expression": (
                "{ (½x², x²; 3/2x², 2x²) + (C₁₁, C₁₂; C₂₁, C₂₂) | "
                "x = ?; C₁₁ = ?, C₁₂ = ?, C₂₁ = ?, C₂₂ = ? }"
            ),
            "pretty": "unused",
            "tex": (
                r"\begin{bmatrix}\frac{1}{2}x^{2} & x^{2} \\ \frac{3}{2}x^{2} & 2x^{2}\end{bmatrix} + "
                r"\begin{bmatrix}C_{11} & C_{12} \\ C_{21} & C_{22}\end{bmatrix}"
            ),
            "bindings": (
                "variable\tx\tNAN\nconstant\tC₁₁\tNAN\nconstant\tC₁₂\tNAN\n"
                "constant\tC₂₁\tNAN\nconstant\tC₂₂\tNAN"
            ),
        }

        payload = mars_lab.prepare_matrix_fields(fields, 64)

        self.assertEqual(payload["result"], "(½x², x²; 3/2x², 2x²) + (C₁₁, C₁₂; C₂₁, C₂₂)")
        self.assertIn(r"\end{bmatrix} + \begin{bmatrix}C_{11} & C_{12}", payload["tex"])
        source = (ROOT / "tools" / "mars_lab.py").read_text(encoding="utf-8")
        self.assertNotIn("matrix_integral_sum_display(", source)

    def test_matrix_payload_uses_native_matrix_bindings(self) -> None:
        fields = {
            "input": "(xy, c)",
            "operation": "eval",
            "kind": "expr",
            "rows": "1",
            "cols": "2",
            "result": "(xy, c)",
            "pretty": "( xy c )",
            "tex": r"\begin{bmatrix}xy & c\end{bmatrix}",
            "bindings": "variable\tx\tNAN\nvariable\ty\tNAN\nconstant\tc\tNAN",
        }

        payload = mars_lab.prepare_matrix_fields(fields, 53)

        self.assertEqual(
            payload["binding_values"],
            [
                {"name": "x", "value": "NAN", "display": "", "kind": "variable"},
                {"name": "y", "value": "NAN", "display": "", "kind": "variable"},
                {"name": "c", "value": "NAN", "display": "", "kind": "constant"},
            ],
        )

    def test_matrix_payload_compacts_complex_TeX_and_suppresses_round_off_noise(self) -> None:
        complex_fields = {
            "operation": "power",
            "kind": "number",
            "rows": "1",
            "cols": "1",
            "result": "(0.553688567145911196672859 + 0.464394162839070687990736i)",
            "pretty": "( 0.553688567145911196672859 + 0.464394162839070687990736i )",
            "tex": r"\begin{bmatrix}0.553688567145911196672859 + 0.464394162839070687990736i\end{bmatrix}",
        }
        noisy_real_fields = {
            "operation": "multiply",
            "kind": "number",
            "rows": "1",
            "cols": "1",
            "result": "(0.999999999999999999999998 - 9.82276977406720363909798e-25i)",
            "pretty": "( 0.999999999999999999999998 - 9.82276977406720363909798e-25i )",
            "tex": r"\begin{bmatrix}0.999999999999999999999998 - 9.82276977406720363909798e-25i\end{bmatrix}",
        }

        complex_payload = mars_lab.prepare_matrix_fields(complex_fields, 17)
        noisy_real_payload = mars_lab.prepare_matrix_fields(noisy_real_fields, 17)

        self.assertEqual(
            complex_payload["tex"],
            r"\begin{bmatrix}\substack{0.5536885671459112 \\ {}+ 0.46439416283907069i}\end{bmatrix}",
        )
        self.assertEqual(noisy_real_payload["tex"], r"\begin{bmatrix}1\end{bmatrix}")
        self.assertEqual(noisy_real_payload["display_result"], "(1)")
        self.assertNotEqual(complex_payload["tex"], complex_payload["full_TeX"])

    def test_matrix_payload_abbreviates_long_TeX_numbers_but_retains_full_TeX(self) -> None:
        long_number = "-0.011496470345797809322568930188283039555624303287995"
        fields = {
            "input": "(1 2; 3 4)^2 - (1 2; 3 4)^2.001",
            "operation": "eval",
            "kind": "number",
            "rows": "1",
            "cols": "1",
            "result": f"({long_number})",
            "pretty": f"(\n  {long_number}\n)",
            "tex": rf"\begin{{bmatrix}}{long_number}\end{{bmatrix}}",
        }

        payload = mars_lab.prepare_matrix_fields(fields, 270)

        self.assertIn("-0.0114964703457...", payload["tex"])
        self.assertNotIn("...", payload["full_TeX"])
        self.assertIn(long_number, payload["full_TeX"])
        self.assertIn("svg", payload)

    def test_matrix_payload_renders_scientific_notation_as_times_ten(self) -> None:
        fields = {
            "operation": "eval",
            "kind": "number",
            "rows": "1",
            "cols": "1",
            "result": "(1.515888050297733E-7)",
            "pretty": "( 1.515888050297733E-7 )",
            "tex": r"\begin{bmatrix}1.515888050297733E-7\end{bmatrix}",
        }

        payload = mars_lab.prepare_matrix_fields(fields, 270)

        expected = r"1.515888050297733\times 10^{-7}"
        self.assertIn(expected, payload["tex"])
        self.assertIn(expected, payload["full_TeX"])
        self.assertNotIn("E-7", payload["tex"])
        self.assertIn("svg", payload)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "matrix_lab").is_file(),
        "release matrix_lab helper is not built",
    )
    def test_symbolic_matrix_evaluation_preserves_algebra_and_exposes_bindings(self) -> None:
        matrix_binary = ROOT / "build" / "release" / "scratch" / "matrix_lab"
        matrix_text = "(xy + 8x + 1, 2*exp(x)*(5y + 1); 3xy + 16x + 3, 2*exp(x)*(10y + 3))"
        fields, raw, returncode = mars_lab.run_matrix_lab_fields(matrix_binary, matrix_text, "eval", 64)
        payload = mars_lab.prepare_matrix_fields(fields, 53)

        self.assertEqual(returncode, 0, raw)
        self.assertEqual(
            payload["result"],
            "(xy + 8x + 1, 2·exp(x)·(5y + 1); 3xy + 16x + 3, 2·exp(x)·(10y + 3))",
        )
        self.assertEqual(
            payload["binding_values"],
            [
                {"name": "x", "value": "NAN", "display": "", "kind": "variable"},
                {"name": "y", "value": "NAN", "display": "", "kind": "variable"},
            ],
        )

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "matrix_lab").is_file(),
        "release matrix_lab helper is not built",
    )
    def test_exact_matrix_result_round_trips_without_decimal_evaluation(self) -> None:
        matrix_binary = ROOT / "build" / "release" / "scratch" / "matrix_lab"
        matrix_text = (
            "((i·√(2) + 1)/√(2i·√(2) + 5), 2/√(2i·√(2) + 5); "
            "3/√(2i·√(2) + 5), (i·√(2) + 4)/√(2i·√(2) + 5))"
        )
        fields, raw, returncode = mars_lab.run_matrix_lab_fields(matrix_binary, matrix_text, "eval", 64)

        self.assertEqual(returncode, 0, raw)
        self.assertEqual(fields.get("kind"), "expr")
        self.assertIn("√(2)", fields.get("result", ""))
        self.assertNotIn("0.553688", fields.get("result", ""))
        self.assertFalse(fields.get("result", "").startswith("{"))
        self.assertNotIn(r"\left\{", fields.get("tex", ""))
        self.assertEqual(mars_lab.prepare_matrix_fields(fields, 53)["binding_values"], [])

    def test_matrix_functions_use_direct_syntax_while_structural_operations_remain_available(self) -> None:
        representative_operations = {
            "exp", "log", "sqrt", "sin", "cos", "tan", "sinh", "cosh", "tanh", "erf", "gamma", "lambert_w0",
        }
        structural_operations = {
            "eval", "inverse", "multiply", "eigenvalues", "eigendecompose", "charpoly", "det", "trace", "rank",
            "simplify", "solve",
        }

        for operation in representative_operations:
            self.assertNotIn(operation, mars_lab.MATRIX_OPERATIONS)
            self.assertNotIn(f'value="{operation}"', mars_lab.INDEX_HTML)
        for operation in structural_operations:
            self.assertIn(f'value="{operation}"', mars_lab.INDEX_HTML)
        self.assertIn('id="matrixOperationLabel"', mars_lab.INDEX_HTML)
        self.assertIn("matrixControls.classList.toggle('hidden', !matrixMode);", mars_lab.INDEX_HTML)
        self.assertIn('<option value="eval" selected>Evaluate expression</option>', mars_lab.INDEX_HTML)
        self.assertIn("genuine matrix functions calculated by MARSlib", mars_lab.INDEX_HTML)
        self.assertIn("sin(1 2; 4 5)", mars_lab.INDEX_HTML)
        self.assertIn("inv(a b; c d)", mars_lab.INDEX_HTML)
        self.assertIn("Matrix division is not defined", mars_lab.INDEX_HTML)

    def test_help_documents_complete_native_matrix_expression_syntax(self) -> None:
        help_html = mars_lab.INDEX_HTML

        self.assertIn("Using Matrix Mode", help_html)
        self.assertIn("choose the required matrix operation", help_html)
        self.assertIn("<code>Right-hand side matrix</code>", help_html)
        self.assertIn("This changes how MARS treats the symbol without replacing the expression you typed", help_html)
        self.assertIn("same four-card arrangement as Expression mode", help_html)
        self.assertIn("Matrix Notation And Aliases", help_html)
        self.assertIn("Matrix Operation Selector", help_html)
        self.assertIn("<code>Solve A X = B</code>", help_html)
        self.assertIn("((1 2; 3 4) - lambdaI)^x", help_html)
        self.assertIn("<code>A^dagger</code>", help_html)
        self.assertIn("<code>||A||</code>", help_html)
        self.assertIn("<code>determinant(A)</code>", help_html)
        self.assertIn("<code>conjugate_transpose(A)</code>", help_html)
        self.assertIn("Matrix Calculus And Values", help_html)
        self.assertIn("Symbolic Matrix-power Calculus", help_html)
        self.assertIn("<code>(1 2; 3 4)^x</code>", help_html)
        self.assertIn("<code>x derivative</code> gives", help_html)
        self.assertIn("<code>x integral</code> gives", help_html)
        self.assertIn("<code>Dxx(A)</code>", help_html)
        self.assertIn("<code>Dxy(A)</code>", help_html)
        self.assertIn("<code>@S(ax+b cx+d; y xy)dx</code>", help_html)
        self.assertIn("<code>@S^x(ax+b cx+d; y xy)dx</code>", help_html)
        self.assertIn("constant square matrices of any order", help_html)
        self.assertIn("numeric reverse-mode gradient path", help_html)
        self.assertIn("future numeric forward-mode JVP path", help_html)
        self.assertIn("<code>A(x) + C</code>", help_html)
        self.assertIn("<code>Value</code> separately shows", help_html)
        self.assertIn("<code>Use as input</code> copies", help_html)
        self.assertIn("Lambert W, and exponential-integral matrix functions", help_html)
        self.assertIn("<code>conjugate(z)</code>", help_html)
        self.assertIn("<code>|z|</code>", help_html)

    def test_native_helper_delegates_complete_matrix_expression_parsing_to_marslib(self) -> None:
        source = (ROOT / "scratch" / "matrix_lab.c").read_text(encoding="utf-8")

        self.assertIn("mat_expression_evaluate(input, &bindings, &parsed_operation, &matrix, &scalar_result)", source)
        self.assertIn("mat_expression_from_string(operand, NULL, NULL)", source)
        self.assertNotIn("evaluate_matrix_expression_span", source)
        self.assertNotIn("direct_unary_operation_name", source)
        self.assertNotIn("matrix_product_operator", source)
        self.assertNotIn("matrix_unary_operations", source)
        self.assertNotIn("matrix_unary_function_for", source)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "matrix_lab").is_file(),
        "release matrix_lab helper is not built",
    )
    def test_native_helper_emits_symbolic_and_evaluated_bound_matrices(self) -> None:
        matrix_binary = ROOT / "build" / "release" / "scratch" / "matrix_lab"
        fields, raw, returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary,
            "{ (1 2; 3 4) - (lambda 0; 0 lambda) | lambda = 3 }",
            "eval",
            32,
        )

        self.assertEqual(returncode, 0, raw)
        self.assertEqual(fields["result"], "(1 - λ, 2; 3, 4 - λ)")
        self.assertEqual(fields["expression"], "{ (1 - λ, 2; 3, 4 - λ) | λ = 3 }")
        self.assertEqual(
            fields["expression_pretty"],
            "{ (\n\t1 - λ,\t2;\n\t3,\t4 - λ\n) | λ = 3 }",
        )
        self.assertIn("matrix mat(λ)", fields["function"])
        self.assertIn("λ = 3.", fields["function"])
        self.assertEqual(fields["value"], "(-2, 2; 3, 1)")
        self.assertIn("-2 2", fields["value_pretty"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "matrix_lab").is_file(),
        "release matrix_lab helper is not built",
    )
    def test_native_helper_emits_all_result_cards_for_spectral_operations(self) -> None:
        matrix_binary = ROOT / "build" / "release" / "scratch" / "matrix_lab"
        eigenvalues, eigenvalue_raw, eigenvalue_returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary, "(1 2; 3 4)", "eigenvalues", 32
        )
        decomposition, decomposition_raw, decomposition_returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary, "(1 2; 3 4)", "eigendecompose", 32
        )

        self.assertEqual(eigenvalue_returncode, 0, eigenvalue_raw)
        self.assertTrue(eigenvalues["result"].startswith("("))
        self.assertEqual(eigenvalues["expression"], eigenvalues["result"])
        self.assertIn("matrix mat()", eigenvalues["function"])
        self.assertIn("λ1 =", eigenvalues["value"])
        self.assertEqual(decomposition_returncode, 0, decomposition_raw)
        self.assertIn("eigenvalues\n(", decomposition["expression"])
        self.assertIn("eigenvectors\n(", decomposition["expression"])
        self.assertIn("matrix eigenvalues()", decomposition["function"])
        self.assertIn("matrix eigenvectors()", decomposition["function"])
        self.assertIn("eigenvectors", decomposition["value"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "matrix_lab").is_file(),
        "release matrix_lab helper is not built",
    )
    def test_native_helper_numerically_evaluates_a_bound_rational_matrix_power(self) -> None:
        matrix_binary = ROOT / "build" / "release" / "scratch" / "matrix_lab"
        fields, raw, returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary,
            "{ (1 2; 3 4)^x | x = 3/2 }",
            "eval",
            32,
        )

        self.assertEqual(returncode, 0, raw)
        self.assertEqual(fields["operation"], "power")
        self.assertEqual(fields["expression"].count("x ="), 1)
        self.assertIn("x = ³⁄₂", fields["expression"])
        declaration_lines = fields["function"].split("    return ", 1)[0].splitlines()[1:]
        constant_lines = [index for index, line in enumerate(declaration_lines) if line.startswith("    const ")]
        variable_lines = [index for index, line in enumerate(declaration_lines) if line.startswith("    v")]
        self.assertTrue(constant_lines)
        self.assertTrue(variable_lines)
        self.assertLess(max(constant_lines), min(variable_lines))
        self.assertTrue(fields["value"].startswith("(2.97457074818556029658"))
        self.assertNotIn("√", fields["value"])
        self.assertNotIn("^", fields["value"])
        self.assertNotIn(r"\sqrt", fields["value_tex"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "matrix_lab").is_file(),
        "release matrix_lab helper is not built",
    )
    def test_bound_matrix_power_substitutes_only_in_the_value_fields(self) -> None:
        matrix_binary = ROOT / "build" / "release" / "scratch" / "matrix_lab"
        unbound_fields, unbound_raw, unbound_returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary,
            "(1 2; 3 4)^x",
            "eval",
            32,
        )
        bound_fields, bound_raw, bound_returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary,
            "{ (1 2; 3 4)^x | x = -2.5 }",
            "eval",
            32,
        )

        self.assertEqual(unbound_returncode, 0, unbound_raw)
        self.assertEqual(bound_returncode, 0, bound_raw)
        for field in ("result", "pretty", "tex"):
            self.assertEqual(bound_fields[field], unbound_fields[field], field)
        self.assertNotIn("√(128)", bound_fields["result"])
        self.assertNotIn(r"\sqrt{128}", bound_fields["tex"])
        self.assertTrue(bound_fields["value"].startswith("(0.00357099560662556126"))
        self.assertNotIn("^", bound_fields["value"])
        self.assertNotEqual(bound_fields["value"], bound_fields["result"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "matrix_lab").is_file(),
        "release matrix_lab helper is not built",
    )
    def test_native_helper_simplifies_a_direct_rational_matrix_power_denominator(self) -> None:
        matrix_binary = ROOT / "build" / "release" / "scratch" / "matrix_lab"
        fields, raw, returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary,
            "(1 2; 3 4)^(3/2)",
            "eval",
            32,
        )

        self.assertEqual(returncode, 0, raw)
        self.assertIn(r"\sqrt{\sqrt{33} + 5}", fields["tex"])
        self.assertIn(r"\sqrt{\sqrt{33} - 5}", fields["tex"])
        self.assertNotIn(r"\sqrt{155 + 27\mkern-2mu \sqrt{33}}", fields["tex"])
        self.assertNotIn(r"\sqrt{155 - 27\mkern-2mu \sqrt{33}}", fields["tex"])
        self.assertNotIn(r"\left(5 + \sqrt{33}\right)^{\frac{3}{2}}", fields["tex"])
        self.assertNotIn("2^(³⁄₂ - 1)", fields["result"])
        self.assertEqual(fields["tex"].count(r"\frac{1}{\sqrt{66}}"), 1)
        self.assertEqual(fields["result"].count("1/√(66)"), 1)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "matrix_lab").is_file(),
        "release matrix_lab helper is not built",
    )
    def test_native_helper_factors_integer_content_before_matrix_beautification(self) -> None:
        matrix_binary = ROOT / "build" / "release" / "scratch" / "matrix_lab"
        fields, raw, returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary,
            "(1 2; 3 4)^(5/2)",
            "eval",
            32,
        )

        self.assertEqual(returncode, 0, raw)
        self.assertEqual(fields["tex"].count(r"\frac{1}{\sqrt{66}}"), 1)
        self.assertIn(r"2\mkern-2mu \left(5\mkern-2mu \sqrt{2}\mkern-2mu i + 27\right)", fields["tex"])
        self.assertIn(r"2\mkern-2mu \left(11\mkern-2mu \sqrt{2}\mkern-2mu i + 59\right)", fields["tex"])
        self.assertIn("5·√(2)·i", fields["result"])
        self.assertIn("11·√(2)·i", fields["result"])
        self.assertNotIn("10√2", fields["result"])
        self.assertNotIn("22√2", fields["result"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "matrix_lab").is_file(),
        "release matrix_lab helper is not built",
    )
    def test_native_helper_renders_matrix_square_root_radicals_in_TeX(self) -> None:
        matrix_binary = ROOT / "build" / "release" / "scratch" / "matrix_lab"
        fields, raw, returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary,
            "(1 2; 3 4)^(1/2)",
            "eval",
            32,
        )

        self.assertEqual(returncode, 0, raw)
        self.assertIn(r"\frac{1}{\sqrt{66}}", fields["tex"])
        self.assertEqual(fields["tex"].count(r"\frac{1}{\sqrt{66}}"), 1)
        self.assertEqual(fields["result"].count("1/√(66)"), 1)
        self.assertIn(r"\sqrt{2}\mkern-2mu i", fields["tex"])
        self.assertIn(r"\sqrt{\sqrt{33} - 5}\mkern-2mu i", fields["tex"])
        self.assertIn(
            r"\left(1 + \sqrt{2}\mkern-2mu i\right)\mkern-2mu "
            r"\left(\sqrt{\sqrt{33} + 5} - \sqrt{\sqrt{33} - 5}\mkern-2mu i\right)",
            fields["tex"],
        )
        self.assertNotIn(r"\sqrt{2i\mkern-2mu \sqrt{2} + 5}", fields["tex"])
        self.assertNotIn(r"\left(5 + \sqrt{33}\right)^{\frac{1}{2}}", fields["tex"])
        self.assertNotIn(r"\left(5 - \sqrt{33}\right)^{\frac{1}{2}}", fields["tex"])
        self.assertIn("√(√(33) + 5)", fields["result"])
        self.assertIn("√(√(33) - 5)", fields["result"])
        self.assertIn("(1 + √(2)·i)·(√(√(33) + 5) - √(√(33) - 5)·i)", fields["result"])
        self.assertNotIn("i2", fields["result"])
        self.assertNotIn("√(-2)", fields["result"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "matrix_lab").is_file(),
        "release matrix_lab helper is not built",
    )
    def test_native_helper_renders_matrix_cube_root_radicals_in_TeX(self) -> None:
        matrix_binary = ROOT / "build" / "release" / "scratch" / "matrix_lab"
        fields, raw, returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary,
            "(1 2; 3 4)^(1/3)",
            "eval",
            32,
        )

        self.assertEqual(returncode, 0, raw)
        self.assertIn(r"\sqrt[3]{5 + \sqrt{33}}", fields["tex"])
        self.assertIn(r"\sqrt[3]{5 - \sqrt{33}}", fields["tex"])
        self.assertIn(r"\frac{\sqrt[3]{4}}{\sqrt{33}}", fields["tex"])
        self.assertNotIn(r"^{\frac{1}{3}}", fields["tex"])
        self.assertNotIn(r"2^{\frac{1}{3} - 1}", fields["tex"])

    def test_matrix_TeX_places_a_symbolic_power_before_its_radical_factor(self) -> None:
        tex = r"\begin{bmatrix}\frac{1}{\sqrt{33}\mkern-2mu 2^{x - 1}}\end{bmatrix}"

        self.assertEqual(
            mars_lab.matrix_display_TeX(tex, 32),
            r"\begin{bmatrix}\frac{1}{2^{x - 1}\mkern-2mu \sqrt{33}}\end{bmatrix}",
        )

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "matrix_lab").is_file(),
        "release matrix_lab helper is not built",
    )
    def test_symbolic_matrix_power_reduces_residual_common_power_quotients(self) -> None:
        matrix_binary = ROOT / "build" / "release" / "scratch" / "matrix_lab"
        fields, raw, returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary,
            "(1 2; 3 4)^x",
            "eval",
            32,
        )

        self.assertEqual(returncode, 0, raw)
        self.assertTrue(fields["result"].startswith("1/2^x.(½·"))
        self.assertIn("2/√(33)·", fields["result"])
        self.assertNotIn("2^x/2^(x + 1)", fields["result"])
        self.assertNotIn("2^x/(√(33)·2^(x - 1))", fields["result"])
        self.assertIn(r"\frac{2}{\sqrt{33}}", fields["tex"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "matrix_lab").is_file(),
        "release matrix_lab helper is not built",
    )
    def test_bound_rational_matrix_power_derivative_has_a_complex_value(self) -> None:
        matrix_binary = ROOT / "build" / "release" / "scratch" / "matrix_lab"
        fields, raw, returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary,
            "{ Dx((1 2; 3 4)^x) | x = 3/2 }",
            "eval",
            32,
        )

        self.assertEqual(returncode, 0, raw)
        self.assertTrue(fields["value"].startswith("(5.5441400453188723946"))
        self.assertNotIn("√", fields["value"])
        self.assertNotIn("NAN", fields["value"].upper())

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "matrix_lab").is_file(),
        "release matrix_lab helper is not built",
    )
    def test_constant_symbolic_matrix_power_uses_the_spectral_derivative_rule(self) -> None:
        matrix_binary = ROOT / "build" / "release" / "scratch" / "matrix_lab"
        fields, raw, returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary,
            "Dx((1 2; 3 4)^x)",
            "eval",
            32,
        )

        self.assertEqual(returncode, 0, raw)
        self.assertTrue(fields["result"].startswith("1/2^x.(½·"))
        self.assertIn("2/√(33)·", fields["result"])
        self.assertIn("√(³⁄₁₁)·", fields["result"])
        self.assertEqual(fields["result"].count("ln(½·(5 + √(33)))"), 4)
        self.assertEqual(fields["result"].count("ln(½·(5 - √(33)))"), 4)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "matrix_lab").is_file(),
        "release matrix_lab helper is not built",
    )
    def test_readme_examples_for_composite_matrix_notation(self) -> None:
        """README examples: matrix aliases, identity multiples and a symbolic power."""
        matrix_binary = ROOT / "build" / "release" / "scratch" / "matrix_lab"
        cases = (
            ("det((1 2; 3 4) - lambdaI)", "det"),
            ("tr(a b; c d)", "trace"),
            ("(a b; c d)^dagger", "hermitian"),
            ("((1 2; 3 4) - lambdaI)^x", "power"),
        )

        for matrix_text, expected_operation in cases:
            with self.subTest(matrix_text=matrix_text):
                fields, raw, returncode = mars_lab.run_matrix_lab_fields(matrix_binary, matrix_text, "eval", 32)

                self.assertEqual(returncode, 0, raw)
                self.assertEqual(fields["operation"], expected_operation)
                self.assertTrue(fields["result"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "matrix_lab").is_file(),
        "release matrix_lab helper is not built",
    )
    def test_bound_matrix_integral_omits_value_until_constants_are_supplied(self) -> None:
        matrix_binary = ROOT / "build" / "release" / "scratch" / "matrix_lab"
        fields, raw, returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary,
            "{ @S^λ(((1 2; 3 4) - (lambda 0; 0 lambda))) | lambda = 3 }",
            "eval",
            32,
        )
        payload = mars_lab.prepare_matrix_fields(fields, 32)

        self.assertEqual(returncode, 0, raw)
        self.assertNotIn("value", payload)
        self.assertIn("valueTitle.textContent = 'Value';", mars_lab.INDEX_HTML)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "matrix_lab").is_file(),
        "release matrix_lab helper is not built",
    )
    def test_native_helper_returns_determinant_expressions_as_scalars(self) -> None:
        matrix_binary = ROOT / "build" / "release" / "scratch" / "matrix_lab"
        inputs = (
            "det((1 2; 3 4) - (lambda 0; 0 lambda))",
            "|(1 2; 3 4) - (lambda 0; 0 lambda)|",
            "||(1 2; 3 4) - (lambda 0; 0 lambda)||",
            "‖(1 2; 3 4) - (lambda 0; 0 lambda)‖",
        )

        for matrix_text in inputs:
            with self.subTest(matrix_text=matrix_text):
                fields, raw, returncode = mars_lab.run_matrix_lab_fields(matrix_binary, matrix_text, "eval", 32)

                self.assertEqual(returncode, 0, raw)
                self.assertEqual(fields["operation"], "det")
                self.assertNotIn("rows", fields)
                self.assertNotIn("cols", fields)
                self.assertIn("(1 - λ)·(4 - λ) - 6", fields["result"])
                self.assertNotIn("value", fields)
                self.assertIn(r"\lambda", fields["tex"])
                self.assertIn("binding      variable\tλ\tNAN", raw)

        bound_fields, bound_raw, bound_returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary,
            "{ |(1 2; 3 4) - (lambda 0; 0 lambda)| | lambda = 4 }",
            "det",
            32,
        )
        self.assertEqual(bound_returncode, 0, bound_raw)
        self.assertIn("(1 - λ)·(4 - λ) - 6", bound_fields["result"])
        self.assertEqual(bound_fields["value"], "-6")

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "matrix_lab").is_file(),
        "release matrix_lab helper is not built",
    )
    def test_native_helper_evaluates_representative_matrix_functions(self) -> None:
        matrix_binary = ROOT / "build" / "release" / "scratch" / "matrix_lab"
        cases = (
            ("exp(0, 0; 0, 0)", "exp", "(1, 0; 0, 1)"),
            ("ln(1, 0; 0, e)", "ln", "(0, 0; 0, 1)"),
            ("sqrt(4, 0; 0, 9)", "sqrt", "(2, 0; 0, 3)"),
            ("sin(0, 0; 0, 0)", "sin", "(0, 0; 0, 0)"),
        )

        for matrix_text, expected_operation, expected in cases:
            with self.subTest(operation=expected_operation):
                fields, raw, returncode = mars_lab.run_matrix_lab_fields(matrix_binary, matrix_text, "eval", 64)

                self.assertEqual(returncode, 0, raw)
                self.assertEqual(fields["operation"], expected_operation)
                self.assertEqual(fields["result"], expected)
                self.assertNotIn("i", fields["result"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "matrix_lab").is_file(),
        "release matrix_lab helper is not built",
    )
    def test_native_helper_accepts_direct_compact_matrix_function_syntax(self) -> None:
        matrix_binary = ROOT / "build" / "release" / "scratch" / "matrix_lab"
        direct_fields, direct_raw, direct_returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary, "sin(1 2; 4 5)", "eval", 64
        )

        self.assertEqual(direct_returncode, 0, direct_raw)
        self.assertEqual(direct_fields["operation"], "sin")
        self.assertEqual((direct_fields["rows"], direct_fields["cols"]), ("2", "2"))
        self.assertTrue(direct_fields["result"].startswith("(-0.315002573091184"))

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "matrix_lab").is_file(),
        "release matrix_lab helper is not built",
    )
    def test_native_helper_accepts_a_grouped_negated_matrix_function_argument(self) -> None:
        matrix_binary = ROOT / "build" / "release" / "scratch" / "matrix_lab"
        direct_fields, direct_raw, direct_returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary, "exp(-(.1 2; 4 5))", "eval", 64
        )
        explicit_fields, explicit_raw, explicit_returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary, "exp(-.1 -2; -4 -5)", "eval", 64
        )

        self.assertEqual(direct_returncode, 0, direct_raw)
        self.assertEqual(explicit_returncode, 0, explicit_raw)
        self.assertEqual(direct_fields["operation"], "exp")
        self.assertEqual((direct_fields["rows"], direct_fields["cols"]), ("2", "2"))
        self.assertEqual(direct_fields["result"], explicit_fields["result"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "matrix_lab").is_file(),
        "release matrix_lab helper is not built",
    )
    def test_native_helper_accepts_compact_matrix_operation_operands(self) -> None:
        matrix_binary = ROOT / "build" / "release" / "scratch" / "matrix_lab"
        fields, raw, returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary,
            "(1 2; 3 4)",
            "multiply",
            64,
            "(5 6; 7 8)",
        )

        self.assertEqual(returncode, 0, raw)
        self.assertEqual(fields["rows"], "2")
        self.assertEqual(fields["cols"], "2")
        self.assertEqual(fields["result"], "(19, 22; 43, 50)")

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "matrix_lab").is_file(),
        "release matrix_lab helper is not built",
    )
    def test_native_helper_reports_bindings_contributed_by_the_right_operand(self) -> None:
        matrix_binary = ROOT / "build" / "release" / "scratch" / "matrix_lab"
        fields, raw, returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary,
            "(1 2; 3 4)",
            "multiply",
            64,
            "(x+3y x*exp(y); x+y y*exp(y))",
        )

        self.assertEqual(returncode, 0, raw)
        payload = mars_lab.prepare_matrix_fields(fields, 64)
        self.assertEqual(
            [binding["name"] for binding in payload["binding_values"]],
            ["x", "y"],
        )

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
        self.assertEqual(fields["result"], "(1/(ad - bc)·(dx - by); 1/(ad - bc)·(ay - cx))")

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "matrix_lab").is_file(),
        "release matrix_lab helper is not built",
    )
    def test_native_helper_differentiates_and_integrates_matrix_entries(self) -> None:
        matrix_binary = ROOT / "build" / "release" / "scratch" / "matrix_lab"
        derivative, derivative_raw, derivative_returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary, "Dx(ax+b cx+d; y xy)", "eval", 64
        )
        nested_derivative, nested_derivative_raw, nested_derivative_returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary, "Dx(exp(-(1+x 2; 4 5)))", "eval", 64
        )
        second_derivative, second_derivative_raw, second_derivative_returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary, "Dxx(1+x 2e^x; 4x 5)", "eval", 64
        )
        integral, integral_raw, integral_returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary, "@S(ax+b cx+d; y xy)dx", "eval", 64
        )
        antiderivative, antiderivative_raw, antiderivative_returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary, "@S^x(ax+b cx+d; y xy)dx", "eval", 64
        )

        self.assertEqual(derivative_returncode, 0, derivative_raw)
        self.assertEqual(derivative["result"], "(a, c; 0, y)")
        self.assertEqual(nested_derivative_returncode, 0, nested_derivative_raw)
        self.assertEqual(nested_derivative["operation"], "eval")
        self.assertEqual((nested_derivative["rows"], nested_derivative["cols"]), ("2", "2"))
        self.assertIn("x", nested_derivative["result"])
        self.assertEqual(second_derivative_returncode, 0, second_derivative_raw)
        self.assertEqual(second_derivative["result"], "(0, 2·exp(x); 0, 0)")
        self.assertEqual(integral_returncode, 0, integral_raw)
        self.assertEqual(integral["rows"], "2")
        self.assertEqual(integral["cols"], "2")
        self.assertEqual(
            mars_lab.prepare_matrix_fields(integral, 64)["result"],
            "(½·(ax² + 2bx), ½·(cx² + 2dx); xy, ½x²y) + (C₁₁, C₁₂; C₂₁, C₂₂)",
        )
        for constant in ("C₁₁", "C₁₂", "C₂₁", "C₂₂"):
            self.assertIn(f"binding      constant\t{constant}\tNAN", integral_raw)
        self.assertEqual(antiderivative_returncode, 0, antiderivative_raw)
        self.assertEqual(antiderivative["result"], "(½·(ax² + 2bx), ½·(cx² + 2dx); xy, ½x²y)")
        self.assertNotIn("C₁₁", antiderivative_raw)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "matrix_lab").is_file(),
        "release matrix_lab helper is not built",
    )
    def test_native_helper_factors_symbolic_matrix_power_entries(self) -> None:
        matrix_binary = ROOT / "build" / "release" / "scratch" / "matrix_lab"
        power, raw, returncode = mars_lab.run_matrix_lab_fields(matrix_binary, "(1, 2; 3, 4)^x", "eval", 64)

        self.assertEqual(returncode, 0, raw)
        self.assertTrue(power["result"].startswith("1/2^x.("))
        self.assertIn("½·((1 - √(³⁄₁₁))·(5 + √(33))^x", power["result"])
        self.assertIn("2/√(33)·((5 + √(33))^x - (5 - √(33))^x)", power["result"])
        self.assertIn("√(³⁄₁₁)·((5 + √(33))^x - (5 - √(33))^x)", power["result"])
        self.assertNotIn("(½·(5 + √(33)))^x", power["result"])
        self.assertIn("= sqrt(3/11).", power["function"])
        self.assertNotIn("³⁄₁₁", power["function"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "matrix_lab").is_file(),
        "release matrix_lab helper is not built",
    )
    def test_native_helper_integrates_symbolic_matrix_power(self) -> None:
        matrix_binary = ROOT / "build" / "release" / "scratch" / "matrix_lab"
        integral, raw, returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary, "@S(1, 2; 3, 4)^xdx", "eval", 64
        )

        self.assertEqual(returncode, 0, raw)
        self.assertEqual((integral["rows"], integral["cols"]), ("2", "2"))
        self.assertNotIn("∫", integral["result"])
        self.assertIn("ln(", integral["result"])
        self.assertIn("(5 - √(33))", integral["result"])
        self.assertTrue(integral["result"].startswith("1/2^x.("))
        self.assertIn("(1 - √(³⁄₁₁))", integral["result"])
        self.assertIn("(1 + √(³⁄₁₁))", integral["result"])
        self.assertIn("√(³⁄₁₁)·((5 + √(33))^x", integral["result"])
        self.assertIn("(1 - √(³⁄₁₁))·(5 + √(33))^x/ln(½·(5 + √(33)))", integral["result"])
        self.assertIn(" + (C₁₁, C₁₂; C₂₁, C₂₂)", integral["result"])
        self.assertNotIn("1/(1/2^x)", integral["result"])
        self.assertNotIn("½·(√(33) - 3)", integral["result"])
        self.assertTrue(integral["tex"].startswith(r"\frac{1}{2^{x}}\mkern-5mu \begin{bmatrix}"))
        self.assertIn(r"\sqrt{\frac{3}{11}}\mkern-2mu \left(", integral["tex"])
        self.assertIn(
            r"\frac{\left(1 - \sqrt{\frac{3}{11}}\right)\mkern-2mu \left(5 + \sqrt{33}\right)^{x}}{\ln",
            integral["tex"],
        )
        self.assertNotIn(r"\frac{1}{\ln", integral["tex"])
        self.assertNotIn(r"\frac{\frac{1}{2}\mkern-2mu \left(\sqrt{33} - 3\right)", integral["tex"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "matrix_lab").is_file(),
        "release matrix_lab helper is not built",
    )
    def test_native_helper_keeps_symbolic_matrix_power_derivative_in_spectral_form(self) -> None:
        matrix_binary = ROOT / "build" / "release" / "scratch" / "matrix_lab"
        derivative, raw, returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary, "Dx(((1, 2; 3, 4)^x))", "eval", 64
        )

        self.assertEqual(returncode, 0, raw)
        self.assertEqual((derivative["rows"], derivative["cols"]), ("2", "2"))
        self.assertIn("ln(½·(5 + √(33)))", derivative["result"])
        self.assertIn(r"\left(5 + \sqrt{33}\right)^{x}\mkern-2mu \ln", derivative["tex"])
        self.assertNotIn(r"\ln(\frac{1}{2})", derivative["tex"])
        self.assertNotIn("ln(½)·", derivative["result"])
        self.assertTrue(derivative["tex"].startswith(r"\frac{1}{2^{x}}\mkern-5mu \begin{bmatrix}"))
        self.assertIn(r"\frac{1}{2}\mkern-2mu \left(\left(1 - \sqrt{\frac{3}{11}}\right)",
                      derivative["tex"])
        self.assertNotIn(r"\frac{\left(\sqrt{33} - 3\right)", derivative["tex"])
        self.assertIn("2/√(33)·((5 + √(33))^x", derivative["result"])
        self.assertNotIn("√(33)/66", derivative["result"])
        self.assertNotIn("√(33)/33", derivative["result"])
        self.assertNotIn("√(33)/11", derivative["result"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "matrix_lab").is_file(),
        "release matrix_lab helper is not built",
    )
    def test_native_helper_matrix_calculus_keeps_bindings_outside_operation(self) -> None:
        matrix_binary = ROOT / "build" / "release" / "scratch" / "matrix_lab"
        integral, raw, returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary, "{ @S(1, 2; 3, 4)^xdx | x = ? }", "eval", 64
        )
        derivative, derivative_raw, derivative_returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary, "{ Dx((1+x, 2; 3, 4)) | x = ? }", "eval", 64
        )

        self.assertEqual(returncode, 0, raw)
        self.assertEqual((integral["rows"], integral["cols"]), ("2", "2"))
        self.assertIn("binding      variable\tx\tNAN", raw)
        for constant in ("C₁₁", "C₁₂", "C₂₁", "C₂₂"):
            self.assertIn(f"binding      constant\t{constant}\tNAN", raw)
        self.assertNotIn("∫", integral["result"])
        self.assertIn("ln(", integral["result"])
        self.assertEqual(derivative_returncode, 0, derivative_raw)
        self.assertEqual(derivative["result"], "(1, 0; 0, 0)")
        self.assertIn("binding      variable\tx\tNAN", derivative_raw)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "matrix_lab").is_file(),
        "release matrix_lab helper is not built",
    )
    def test_native_helper_supports_ordered_higher_matrix_power_calculus(self) -> None:
        matrix_binary = ROOT / "build" / "release" / "scratch" / "matrix_lab"
        derivative, derivative_raw, derivative_returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary, "Dxx(((1 2; 3 4)^x))", "eval", 64
        )
        integral, integral_raw, integral_returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary, "@S^x^2(((1 2; 3 4)^x))", "eval", 64
        )

        self.assertEqual(derivative_returncode, 0, derivative_raw)
        self.assertNotIn("<expr matrix>", derivative["result"])
        self.assertIn("ln²(", derivative["result"])
        self.assertEqual(integral_returncode, 0, integral_raw)
        self.assertNotIn("<expr matrix>", integral["result"])
        self.assertIn("/ln²(", integral["result"])
        self.assertIn(" + (C₁₁, C₁₂; C₂₁, C₂₂)", integral["result"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "matrix_lab").is_file(),
        "release matrix_lab helper is not built",
    )
    def test_native_helper_supports_mixed_matrix_power_calculus(self) -> None:
        matrix_binary = ROOT / "build" / "release" / "scratch" / "matrix_lab"
        derivative, derivative_raw, derivative_returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary, "Dxy(((1 0; 0 2)^(x+y)))", "eval", 64
        )
        integral, integral_raw, integral_returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary, "@S^xy(((1 0; 0 2)^(x+y)))", "eval", 64
        )

        self.assertEqual(derivative_returncode, 0, derivative_raw)
        self.assertEqual(derivative["result"], "(0, 0; 0, ln²(2)·2^(x + y))")
        self.assertIn("binding      variable\tx\tNAN", derivative_raw)
        self.assertIn("binding      variable\ty\tNAN", derivative_raw)
        self.assertEqual(integral_returncode, 0, integral_raw)
        self.assertIn("(xy, 0; 0, 2^(x + y)/ln²(2))", integral["result"])
        self.assertIn(" + (C₁₁, C₁₂; C₂₁, C₂₂)", integral["result"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "matrix_lab").is_file(),
        "release matrix_lab helper is not built",
    )
    def test_native_helper_applies_matrix_power_calculus_to_larger_square_matrices(self) -> None:
        matrix_binary = ROOT / "build" / "release" / "scratch" / "matrix_lab"
        derivative, derivative_raw, derivative_returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary, "Dx(((1 0 0; 0 2 0; 0 0 3)^x))", "eval", 64
        )
        integral, integral_raw, integral_returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary, "@S(1 0 0; 0 2 0; 0 0 3)^xdx", "eval", 64
        )

        self.assertEqual(derivative_returncode, 0, derivative_raw)
        self.assertEqual((derivative["rows"], derivative["cols"]), ("3", "3"))
        self.assertIn("2^x·ln(2)", derivative["result"])
        self.assertIn("3^x·ln(3)", derivative["result"])
        self.assertEqual(integral_returncode, 0, integral_raw)
        self.assertEqual((integral["rows"], integral["cols"]), ("3", "3"))
        self.assertIn("2^x/ln(2)", integral["result"])
        self.assertIn("3^x/ln(3)", integral["result"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "matrix_lab").is_file(),
        "release matrix_lab helper is not built",
    )
    def test_native_helper_treats_functions_of_constant_square_matrices_as_constant(self) -> None:
        matrix_binary = ROOT / "build" / "release" / "scratch" / "matrix_lab"
        derivative, derivative_raw, derivative_returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary, "Dx(exp(1 0 0; 0 2 0; 0 0 3))", "eval", 64
        )
        integral, integral_raw, integral_returncode = mars_lab.run_matrix_lab_fields(
            matrix_binary, "@S exp(1 0 0; 0 2 0; 0 0 3)dx", "eval", 64
        )

        self.assertEqual(derivative_returncode, 0, derivative_raw)
        self.assertEqual(derivative["result"], "(0, 0, 0; 0, 0, 0; 0, 0, 0)")
        self.assertEqual(integral_returncode, 0, integral_raw)
        self.assertIn("ex", integral["result"])
        self.assertIn("C₃₃", integral["result"])
        self.assertIn("binding      variable\tx\tNAN", integral_raw)


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
            r"\phi &= F\left(x + i\mkern-2mu y\right) + G\left(x - i\mkern-2mu y\right)",
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
            r"u &= F\left(i\mkern-2mu t + s\right) + G\left(s - i\mkern-2mu t\right)",
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
            r"F\left(r\mkern-2mu e^{i\mkern-2mu \theta}\right)",
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
            r"z &= \frac{1}{2}\mkern-2mu x\mkern-2mu \left(y^{2} - 1\right)",
            fields["solutions_TeX"],
        )
        self.assertIn(
            r"\mu&=e^{\int\left(2\mkern-2mu y\right)\,dy}=e^{y^{2}}",
            fields["steps_TeX"],
        )
        self.assertIn(
            r"\frac{\partial\mu}{\partial y}"
            r"&=\left(2\mkern-2mu y\right)\mu",
            fields["steps_TeX"],
        )
        self.assertIn(
            r"\mu\frac{\partial z}{\partial y}"
            r"+\frac{\partial\mu}{\partial y}z",
            fields["steps_TeX"],
        )
        self.assertIn(r"=\mu x\mkern-2mu y^{3}", fields["steps_TeX"])
        self.assertNotIn(r"\mu\cdot x y^{3}", fields["steps_TeX"])
        self.assertNotIn(r"\mu\left(x y^{3}\right)", fields["steps_TeX"])
        self.assertIn(
            r"\mu z&=\frac{1}{2}\mkern-2mu x\mkern-2mu \left(y^{2} - 1\right)\mkern-2mu e^{y^{2}}",
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
            r"\frac{1}{2}\mkern-2mu y^{3}",
            parameter_rate_fields["solutions_TeX"],
        )
        self.assertIn(
            r"F\left(x\right)\mkern-2mu e^{-2\mkern-2mu x\mkern-2mu y}",
            parameter_rate_fields["solutions_TeX"],
        )
        self.assertIn(
            r"\mu&=e^{\int\left(2\mkern-2mu x\right)\,dy}=e^{2\mkern-2mu x\mkern-2mu y}",
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
            r"\left(C_{1} + C_{2}\mkern-2mu t\right)\mkern-2mu e^{\omega\mkern-2mu t}",
            solution_TeX,
        )
        self.assertIn(
            r"\left(C_{3} + C_{4}\mkern-2mu t\right)\mkern-2mu e^{-\omega\mkern-2mu t}",
            solution_TeX,
        )
        self.assertGreater(wrapped_TeX.count(r"\begin{aligned}[t]"), 1)

        payload = mars_lab.prepare_diffequation_fields(fields)
        self.assertTrue(payload.get("svg"))
        self.assertTrue(payload.get("wrapped_svg"))

    def test_solution_wrapping_uses_the_available_card_width(self) -> None:
        html = mars_lab.INDEX_HTML

        self.assertIn("function fitRenderedTeXToCard()", html)
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
        self.assertIn(r"\omega^{6}\mkern-2mu x", fields["problem_TeX"])
        self.assertNotIn(r"\sum", solution_TeX)
        self.assertIn(
            r"\left(C_{1} + C_{2}\mkern-2mu t + C_{3}\mkern-2mu t^{2}\right)",
            solution_TeX,
        )
        self.assertIn(
            r"\left(C_{4} + C_{5}\mkern-2mu t + C_{6}\mkern-2mu t^{2}\right)",
            solution_TeX,
        )
        self.assertIn(r"\cos(\omega\mkern-2mu t)", solution_TeX)
        self.assertIn(r"\sin(\omega\mkern-2mu t)", solution_TeX)

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
        self.assertIn(r"\sum_{k=0}^{3}C_{k + 1}\mkern-2mu x^{k}", solution_TeX)
        self.assertIn(r"\sum_{k=0}^{3}C_{k + 5}\mkern-2mu x^{k}", solution_TeX)
        self.assertIn(r"\cos(\omega\mkern-2mu x)", solution_TeX)
        self.assertIn(r"\sin(\omega\mkern-2mu x)", solution_TeX)

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
            "r = 1/(2·cos²(θ))·(sin(θ) - √(sin²(θ) - C·cos²(θ)))\n"
            "r = 1/(2·cos²(θ))·(sin(θ) + √(sin²(θ) - C·cos²(θ)))",
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
    def test_native_helper_applies_an_initial_condition_to_an_exact_differential_form(
        self,
    ) -> None:
        completed = subprocess.run(
            [
                str(
                    ROOT
                    / "build"
                    / "release"
                    / "scratch"
                    / "diffequation_lab"
                ),
                "(x^2+y^2)dx + 2xy dy = 0; y(2) = 1",
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
        self.assertEqual(payload["solutions"], "y = √(1/(3x)·(14 - x³))")
        self.assertIn(r"14 - x^{3}", payload["solutions_TeX"])
        self.assertNotIn(r"\ln", payload["solutions_TeX"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "diffequation_lab").is_file(),
        "release diffequation_lab helper is not built",
    )
    def test_native_helper_solves_a_leading_divided_differential_form(
        self,
    ) -> None:
        source = (
            "dx/sqrt(x^2+y^2) + "
            "(1/y - x/(y*sqrt(x^2+y^2)))dy = 0; y(2) = 1"
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
            payload["solutions"],
            "y = √((√(5) + 2)·(√(5) - 2x + 2))",
        )
        self.assertIn(r"\frac{1}{\sqrt{x^{2} + y^{2}}}\,dx", payload["problem_TeX"])

        family_completed = subprocess.run(
            [
                str(
                    ROOT
                    / "build"
                    / "release"
                    / "scratch"
                    / "diffequation_lab"
                ),
                source.split(";", 1)[0],
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        family_payload = mars_lab.prepare_diffequation_fields(
            mars_lab.parse_diffequation_lab_output(family_completed.stdout)
        )

        self.assertEqual(family_payload["status"], "solved")
        self.assertEqual(family_payload["solver"], "exact first-order")
        self.assertEqual(family_payload["solutions"], "y = ±C·√(1 - 2x/C)")
        self.assertIn(r"\pm", family_payload["solutions_TeX"])

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
            "y = 1/(x² + C₁x + C₂)·(2x + C₁)",
        )
        self.assertEqual(payload["symmetry"], "SL(3, ℝ)")
        self.assertIn("X = x − 1/y", payload["steps"])
        self.assertIn("Y = x/y − x²/2", payload["steps"])
        self.assertNotIn("General solution", payload["steps"])
        self.assertIn(r"\frac{2\mkern-2mu x + C_{1}}", payload["solutions_TeX"])

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
            "y = 1/(2·(x² + C₁x + C₂))·(2x + C₁)",
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
        self.assertEqual(payload["solutions_wrapped_TeX"], payload["solutions_TeX"])
        self.assertIn(r"\sqrt{x}\mkern-2mu \left(C_{1}\mkern-2mu J_{-\frac{1}{4}}", payload["solutions_TeX"])
        self.assertNotIn(r"\cdot", payload["solutions_TeX"])
        self.assertIn(r"\left(\frac{1}{2}\mkern-2mu x^{2}\right)", payload["solutions_TeX"])
        self.assertNotIn(r"\frac{1}{2} \cdot x^{2}", payload["solutions_TeX"])
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
                "1/(2·(n + 2)·(n + 1))·(C₁·c_(n) + c_(n - 3))",
            ],
        )


class ExpressionResultTests(unittest.TestCase):
    @property
    def expression_binary(self) -> Path:
        return ROOT / "build" / "release" / "scratch" / "mars_lab"

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_function_atomic_negation_omits_redundant_parentheses(self) -> None:
        atomic, atomic_raw, atomic_returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, "-x", 64, "x", "evaluate"
        )

        self.assertEqual(atomic_returncode, 0, atomic_raw)
        self.assertIn("return -x.", atomic["function"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_expression_mode_simplifies_trigamma_at_positive_infinity(self) -> None:
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, "trigamma(inf)", 64, "x", "evaluate"
        )

        self.assertEqual(returncode, 0, raw)
        self.assertEqual(fields["expression"], "0")
        self.assertEqual(fields["unbound"], "0")
        self.assertEqual(fields["tex"], "0")
        self.assertIn("return 0.", fields["function"])
        self.assertEqual(fields["value"], "0")

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_expression_mode_accepts_nested_open_arctangent_series(self) -> None:
        source = "1/2(1-1/(3*2^2)+1/(5*2^4)-....)+1/3(1-1/(3*3^2)+1/(5*3^4)-....)"
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, source, 64, "x", "evaluate"
        )

        self.assertEqual(returncode, 0, raw)
        self.assertEqual(fields["unbound"], "atan(½) + atan(⅓)")
        self.assertIn(r"\arctan(\frac{1}{2}) + \arctan(\frac{1}{3})", fields["tex"])
        self.assertAlmostEqual(float(fields["value"]), math.pi / 4.0, places=15)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_expression_mode_accepts_telescoping_product_ellipsis(self) -> None:
        source = "(1+1/2)(1+1/3)(1+1/4)...(1+1/10)"
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, source, 64, "x", "evaluate"
        )

        self.assertEqual(returncode, 0, raw)
        self.assertEqual(fields["unbound"], "¹¹⁄₂")
        self.assertIn(r"\prod_{k=2}^{10}\left(1+\frac{1}{k}\right)", fields["derivation_TeX"])
        self.assertIn("return 11/2.", fields["function"])
        self.assertEqual(fields["value"], "5.5")

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_expression_mode_accepts_infinite_prime_product_ellipsis(self) -> None:
        source = "(1+1/2)(1+1/3)(1+1/5)(1+1/7)(1+1/11)(1+1/13)(1+1/17)..."
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, source, 64, "x", "evaluate"
        )

        self.assertEqual(returncode, 0, raw)
        self.assertEqual(fields["unbound"], "inf")
        self.assertIn(r"\prod_{p\in\mathbb{P}}\left(1+\frac{1}{p}\right)=+\infty", fields["derivation_TeX"])
        self.assertIn("return @inf.", fields["function"])
        self.assertEqual(fields["value"], "∞")

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_expression_mode_accepts_reciprocal_infinite_prime_product_ellipsis(self) -> None:
        source = "(1-1/2)(1-1/3)(1-1/5)(1-1/7)(1-1/11)(1-1/13)(1-1/17)..."
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, source, 64, "x", "evaluate"
        )

        self.assertEqual(returncode, 0, raw)
        self.assertEqual(fields["unbound"], "0")
        self.assertIn(r"\prod_{p\in\mathbb{P}}\left(1-\frac{1}{p}\right)=0", fields["derivation_TeX"])
        self.assertIn("return 0.", fields["function"])
        self.assertEqual(fields["value"], "0")

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_expression_mode_accepts_finite_nested_arctangent_series(self) -> None:
        source = (
            "{ 2*(1-1/(3*2^2)+1/(5*2^4)-....+(-1)^n/((2n+1)*2^(2*n)))"
            "+4/3*(1-1/(3*3^2)+1/(5*3^4)-....+(-1)^n/((2n+1)*3^(2*n))) | n = 2 }"
        )
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, source, 64, "n", "evaluate"
        )

        self.assertEqual(returncode, 0, raw)
        self.assertIn("hypergeometricpfq", fields["function"])
        self.assertIn(r"{}_{2}F_{1}", fields["tex"])
        self.assertAlmostEqual(float(fields["value"]), 3.1455761316872428, places=15)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_expression_mode_evaluates_infinite_nested_arctangent_series_as_pi(self) -> None:
        source = (
            "{ 2*(1-1/(3*2^2)+1/(5*2^4)-....+(-1)^n/((2n+1)*2^(2*n)))"
            "+4/3*(1-1/(3*3^2)+1/(5*3^4)-....+(-1)^n/((2n+1)*3^(2*n))) | n = inf }"
        )
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, source, 64, "n", "evaluate"
        )

        self.assertEqual(returncode, 0, raw)
        self.assertEqual(fields["unbound"], "π")
        self.assertEqual(fields["tex"], r"\pi")
        self.assertAlmostEqual(float(fields["value"]), math.pi, places=15)

        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, fields, source, 64, save_expression=False, wrt="n", action="evaluate"
        )
        self.assertIn(r"{}_{2}F_{1}", payload["display_TeX"])
        self.assertIn("n = ∞", payload["display_expression"])
        self.assertIn("@pi", payload["display_function"])
        self.assertIn("n = @inf.", payload["display_function"])
        self.assertNotIn("π", payload["display_function"])
        self.assertAlmostEqual(float(payload["value"]), math.pi, places=15)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_expression_mode_hoists_shared_hypergeometric_parameters(self) -> None:
        source = (
            "{ 2*(1-1/(3*2^2)+1/(5*2^4)-....+(-1)^n/((2n+1)*2^(2*n)))"
            "+4/3*(1-1/(3*3^2)+1/(5*3^4)-....+(-1)^n/((2n+1)*3^(2*n)))-pi | ; n = 2000 }"
        )
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, source, 64, "x", "evaluate"
        )

        self.assertEqual(returncode, 0, raw)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, fields, source, 64, save_expression=False, wrt="x", action="evaluate"
        )
        function = str(payload["display_function"])
        half_parameters = re.findall(r"const (c\d+) = c\d+/2\.", function)
        self.assertEqual(len(half_parameters), 2, function)
        for name in half_parameters:
            self.assertEqual(function.count(name), 3, function)
        self.assertNotIn("hypergeometric_pFq_pack", function)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_expression_mode_preserves_the_tiny_finite_machin_remainder(self) -> None:
        source = (
            "{ 2*(1-1/(3*2^2)+1/(5*2^4)-....+(-1)^n/((2n+1)*2^(2*n)))"
            "+4/3*(1-1/(3*3^2)+1/(5*3^4)-....+(-1)^n/((2n+1)*3^(2*n)))-pi | n = 1000 }"
        )
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, source, 64, "n", "evaluate"
        )

        self.assertEqual(returncode, 0, raw)
        self.assertNotIn("atan", fields["unbound"])
        self.assertNotIn("π", fields["unbound"])
        self.assertNotEqual(fields["value"], "0")
        self.assertRegex(fields["value"], r"^1\.739700075321556[0-9]*E-606$")

        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, fields, source, 64, save_expression=False, wrt="n"
        )
        self.assertEqual(payload["value"], fields["value"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_expression_mode_displays_the_finite_leibniz_sum_formula(self) -> None:
        source = "4-4/3+4/5-4/7+...+4(-1)^n/(2n+1)"
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, source, 64, "n", "evaluate"
        )

        self.assertEqual(returncode, 0, raw)
        self.assertIn("π +", fields["unbound"])
        self.assertIn("digamma(n/2 + 5/4) - digamma(n/2 + 3/4)", fields["function"])
        self.assertIn(r"\sum_{k=0}^{n}", fields["derivation_TeX"])
        self.assertIn(r"\psi^{(0)}(\frac{n}{2} + \frac{5}{4})", fields["derivation_TeX"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_expression_mode_accepts_a_symbolic_harmonic_series_endpoint(self) -> None:
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, "1 + 1/2 + 1/3 + ... + 1/N", 64, "x", "evaluate"
        )

        self.assertEqual(returncode, 0, raw)
        self.assertEqual(fields["expression"], "{ ψ⁽⁰⁾(N + 1) + γ | N = NAN }")
        self.assertIn("return digamma(N + 1) + @gamma.", fields["function"])
        self.assertIn(r"\sum_{n=1}^{N}\frac{1}{n}", fields["derivation_TeX"])
        self.assertIn(r"\psi^{(0)}(N + 1) + \gamma", fields["derivation_TeX"])
        self.assertEqual(fields["value"], "NAN")

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_expression_mode_uses_tetragamma_for_a_symbolic_inverse_cube_series(self) -> None:
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, "1 + 1/2^3 + 1/3^3 + ... + 1/N^3", 64, "x", "evaluate"
        )

        self.assertEqual(returncode, 0, raw)
        self.assertEqual(fields["expression"], "{ ζ(3) + ψ⁽²⁾(N + 1)/2 | N = NAN }")
        self.assertIn("return zeta(3) + polygamma(2, N + 1)/2.", fields["function"])
        self.assertIn(r"\sum_{n=1}^{N}\frac{1}{n^{3}}", fields["derivation_TeX"])
        self.assertIn(r"\zeta(3) + \frac{\psi^{(2)}(N + 1)}{2}", fields["derivation_TeX"])
        self.assertEqual(fields["value"], "NAN")

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_expression_mode_expands_series_ellipsis_before_evaluation_and_calculus(self) -> None:
        numeric_source = "1 + 1/2^2 + 1/3^2 + 1/4^2 + ... + 1/2000^2"
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, numeric_source, 64, "x", "evaluate"
        )

        self.assertEqual(returncode, 0, raw)
        self.assertNotIn("...", fields["unbound"])
        self.assertEqual(fields["unbound"], "π²/6 - ψ⁽¹⁾(2001)")
        self.assertIn("return @pi^2/6 - trigamma(2001).", fields["function"])
        self.assertTrue(fields["value"].startswith("1.6444341918273931041807483139675"), raw)
        self.assertIn(r"\sum_{n=1}^{2000}\frac{1}{n^{2}}", fields["derivation_TeX"])
        self.assertIn(r"\frac{\pi^{2}}{6} - \psi^{(1)}(2001)", fields["derivation_TeX"])
        self.assertLess(fields["derivation_TeX"].index(r"\sum_"), fields["derivation_TeX"].index(r"\psi^{(1)}"))

        large_source = "1+1/2^2+1/3^2+...+1/100000000^2"
        large_fields, large_raw, large_returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, large_source, 64, "x", "evaluate"
        )
        self.assertEqual(large_returncode, 0, large_raw)
        self.assertEqual(large_fields["unbound"], "π²/6 - ψ⁽¹⁾(100000001)")
        self.assertIn("return @pi^2/6 - trigamma(100000001).", large_fields["function"])
        self.assertTrue(large_fields["value"].startswith("1.6449340568482264864724149999793"), large_raw)
        self.assertIn(r"\sum_{n=1}^{100000000}\frac{1}{n^{2}}", large_fields["derivation_TeX"])
        self.assertIn(r"\frac{\pi^{2}}{6} - \psi^{(1)}(100000001)", large_fields["derivation_TeX"])
        mars_lab.prepare_evaluation_fields(
            self.expression_binary, large_fields, large_source, 64, save_expression=False
        )
        self.assertEqual(large_fields["full_display_TeX"], large_fields["derivation_TeX"])

        symbolic_source = "x + x/2 + x/3 + x/4 + ... + x/10"
        derivative_fields, derivative_raw, derivative_returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, symbolic_source, 64, "x", "derivative"
        )
        integral_fields, integral_raw, integral_returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, symbolic_source, 64, "x", "integral"
        )

        self.assertEqual(derivative_returncode, 0, derivative_raw)
        self.assertEqual(derivative_fields["derivative"], "d/dx = ⁷³⁸¹⁄₂₅₂₀")
        self.assertEqual(integral_returncode, 0, integral_raw)
        self.assertEqual(integral_fields["integral"], "∫dx = { ⁷³⁸¹⁄₅₀₄₀x² + C | x = NAN; C = NAN }")

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_expression_mode_recognises_a_symbolic_geometric_series_and_refreshes_bindings(self) -> None:
        completed = subprocess.run(
            [
                str(self.expression_binary),
                "a+ar+ar^2+ar^3+...+ar^n",
                "x",
                "80",
                "bindings",
                "{ 1+1/2^p+1/3^p+1/4^p+...+1/n^p | p=1; n=100 }",
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        fields = mars_lab.parse_mars_lab_output(completed.stdout)

        self.assertEqual(fields["unbound"], "a/(r - 1)·(r^(n + 1) - 1)")
        self.assertIn(r"\sum_{k=0}^{n}a\mkern-2mu r^{k}", fields["derivation_TeX"])
        self.assertIn(r"\frac{a}{r - 1}\mkern-2mu \left(r^{n + 1} - 1\right)", fields["derivation_TeX"])
        self.assertEqual(
            fields["bindings"].splitlines(),
            ["constant\ta\tNAN", "variable\tr\tNAN", "constant\tn\t100"],
        )
        self.assertNotIn("\tp\t", fields["bindings"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_elementary_functions_of_symbolic_complex_input_use_cartesian_form(self) -> None:
        expected_fragments = {
            "exp": ("exp(x)·cos(y)", "exp(x)·sin(y)·i"),
            "sin": ("sin(x)·cosh(y)", "cos(x)·sinh(y)·i"),
            "cos": ("cos(x)·cosh(y)", "sin(x)·sinh(y)·i"),
            "tan": ("sin(2x)", "sinh(2y)/(cos(2x) + cosh(2y))·i"),
            "sinh": ("sinh(x)·cos(y)", "cosh(x)·sin(y)·i"),
            "cosh": ("cosh(x)·cos(y)", "sinh(x)·sin(y)·i"),
            "tanh": ("sinh(2x)", "sin(2y)/(cosh(2x) + cos(2y))·i"),
            "atan": ("atan2(2x, 1 - x² - y²)", "¼·ln("),
            "ln": ("ln(x² + y²)", "atan2(y, x)·i"),
            "log10": ("ln(x² + y²)", "atan2(y, x)/ln(10)·i"),
        }

        for function, fragments in expected_fragments.items():
            source = f"{function}(x+i*y)"
            with self.subTest(function=function):
                fields, raw, returncode = mars_lab.run_mars_lab_fields(
                    self.expression_binary, source, 32, "x", "evaluate"
                )
                self.assertEqual(returncode, 0, raw)
                self.assertNotIn(source.replace("*", "·"), fields["unbound"])
                for fragment in fragments:
                    self.assertIn(fragment, fields["unbound"])
                self.assertNotIn("i·", fields["unbound"])
                self.assertTrue(fields["unbound"].endswith("·i"))

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_all_supported_complex_elementary_functions_and_calculus_use_p_plus_qi_form(self) -> None:
        functions = (
            "exp", "ln", "log10",
            "sin", "cos", "tan", "sec", "cosec", "cot",
            "sinh", "cosh", "tanh", "sech", "cosech", "coth",
            "asin", "acos", "atan", "asec", "acosec", "acot",
            "asinh", "acosh", "atanh", "asech", "acosech", "acoth",
        )

        for function in functions:
            source = f"{function}(x+i*y)"
            with self.subTest(function=function, action="evaluate"):
                fields, raw, returncode = mars_lab.run_mars_lab_fields(
                    self.expression_binary, source, 32, "x", "evaluate"
                )
                self.assertEqual(returncode, 0, raw)
                self.assertTrue(fields["unbound"].endswith("·i"), raw)
                self.assertNotIn("i·", fields["unbound"])
                self.assertNotIn("- -", fields["unbound"])

            for wrt in ("x", "y"):
                for action, field, suffix in (
                    ("derivative", "derivative", "·i |"),
                    ("integral", "integral", "·i + C |"),
                ):
                    with self.subTest(function=function, wrt=wrt, action=action):
                        fields, raw, returncode = mars_lab.run_mars_lab_fields(
                            self.expression_binary, source, 32, wrt, action
                        )
                        self.assertEqual(returncode, 0, raw)
                        self.assertIn(field, fields, raw)
                        self.assertIn(suffix, fields[field], raw)
                        self.assertNotIn("no symbolic integral", fields[field])
                        self.assertNotIn("i·", fields[field])
                        self.assertNotIn("- -", fields[field])
                        self.assertNotIn("x + iy", fields[field])
                        self.assertNotIn("x + yi", fields[field])
                        self.assertNotIn("xyi", fields[field])
                        self.assertNotIn("ixy", fields[field])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_pure_imaginary_elementary_functions_and_calculus_use_cartesian_form(self) -> None:
        functions = (
            "exp", "sin", "cos", "tan", "sec", "cosec", "cot",
            "sinh", "cosh", "tanh", "sech", "cosech", "coth",
            "asin", "atan", "asinh", "atanh",
        )

        for function in functions:
            source = f"{function}(i*y)"
            for action, field in (("evaluate", "unbound"), ("derivative", "derivative"), ("integral", "integral")):
                with self.subTest(function=function, action=action):
                    fields, raw, returncode = mars_lab.run_mars_lab_fields(
                        self.expression_binary, source, 32, "y", action
                    )
                    self.assertEqual(returncode, 0, raw)
                    self.assertIn(field, fields, raw)
                    self.assertNotIn(f"{function}(iy)", fields[field])
                    self.assertNotIn("i·", fields[field])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_complex_tanh_derivative_normalises_double_negative_imaginary_term(self) -> None:
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, "tanh(x+i*y)", 32, "x", "derivative"
        )

        self.assertEqual(returncode, 0, raw)
        self.assertNotIn("- -", fields["derivative"])
        self.assertNotIn("- -", fields["derivative_TeX"])
        self.assertNotIn("-  -", fields["derivative_function"])
        self.assertIn(
            "- 2·sinh(2x)·sin(2y)/(cosh(2x) + cos(2y))²·i",
            fields["derivative"],
        )

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_complex_tanh_has_cartesian_integrals_for_both_components(self) -> None:
        for wrt in ("x", "y"):
            with self.subTest(wrt=wrt):
                fields, raw, returncode = mars_lab.run_mars_lab_fields(
                    self.expression_binary, "tanh(x+i*y)", 32, wrt, "integral"
                )
                self.assertEqual(returncode, 0, raw)
                self.assertIn("integral", fields, raw)
                self.assertIn("atan2(sinh(x)·sin(y), cosh(x)·cos(y))", fields["integral"])
                self.assertIn("ln(cosh²(x)·cos²(y) + sinh²(x)·sin²(y))", fields["integral"])
                self.assertIn("C", fields["integral"])
                self.assertNotIn("tanh(x + iy)", fields["integral"])
                if wrt == "y":
                    self.assertNotIn("/i", fields["integral"])
                    self.assertNotIn("/i", fields["integral_function"])
                    self.assertNotIn(r"}{i}", fields["integral_TeX"])
                    self.assertIn("·i + C", fields["integral"])

    def test_evaluation_preserves_user_authored_expression_input(self) -> None:
        self.assertIn(
            "expressionWithBindings(editorBody, bindings) || editorBody",
            mars_lab.INDEX_HTML,
        )
        self.assertIn(
            "const inlineBindings = bindingParts(editedBody);\n"
            "      const editorBody = expressionBodyForEditor(editedBody);\n"
            "      const authoredBindingSource = inlineBindings ? editedBody : sourceExpression;",
            mars_lab.INDEX_HTML,
        )
        self.assertIn("expr.value = displayedExpressionText;", mars_lab.INDEX_HTML)
        self.assertIn(
            "saveLastExpression(fullExpressionText, {debounce: true});",
            mars_lab.INDEX_HTML,
        )
        self.assertIn(
            ": expressionBodyForEditor(editorBodyText);",
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

    def test_large_exact_integers_are_compact_only_outside_the_value_card(self) -> None:
        integer = "1332293859845693485943876543210987654321098765"
        abbreviated = "1.3322938598456934859438...e+45"

        self.assertEqual(mars_lab.compact_long_numeric_tokens(integer), abbreviated)
        compact_expression = mars_lab.compact_display_text(f"{{ x | x = {integer} }}")
        self.assertIn(abbreviated, compact_expression)
        self.assertEqual(
            mars_lab.compact_function_text(f"const n = {integer}."),
            f"const n = {abbreviated}.",
        )
        self.assertEqual(
            mars_lab.compact_display_text("{ ψ⁽⁰⁾(n + 1) + γ | ; n = 1.0000000000000000000000...x10^57 }"),
            "{ ψ⁽⁰⁾(n + 1) + γ | ; n = 1.0000000000000000000000...e+57}",
        )
        self.assertEqual(
            mars_lab.compact_function_text("const n = 1.0000000000000000000000...x10^57."),
            "const n = 1.0000000000000000000000...e+57.",
        )
        self.assertEqual(
            mars_lab.compact_display_TeX(integer),
            r"1.3322938598456934859438...\times 10^{45}",
        )
        self.assertEqual(
            mars_lab.compact_display_TeX("1.0000000000000000000000...x10^57"),
            r"1.0000000000000000000000...\times 10^{57}",
        )
        short_derivation = (
            "\\begin{aligned}[t]\n"
            "&\\sum_{n=1}^{\\infty}\\frac{1}{n^{2}} \\\\\n"
            "&= \\frac{\\pi^{2}}{6}\n"
            "\\end{aligned}"
        )
        self.assertEqual(
            mars_lab.compact_display_TeX(short_derivation),
            r"\sum_{n=1}^{\infty}\frac{1}{n^{2}} = \frac{\pi^{2}}{6}",
        )
        case_derivation = (
            "\\begin{aligned}[t]\n"
            "&\\sum_{k=1}^{n}\\frac{1}{k^{s}} \\\\\n"
            "&= \\begin{cases}\n"
            "\\psi^{(0)}(n + 1) + \\gamma, & s = 1, \\\\\n"
            "\\zeta(s) - \\zeta(s, n + 1), & s \\ne 1.\n"
            "\\end{cases}\n"
            "\\end{aligned}"
        )
        self.assertEqual(
            mars_lab.compact_display_TeX(case_derivation),
            "\\sum_{k=1}^{n}\\frac{1}{k^{s}} = \\begin{cases}\n"
            "\\psi^{(0)}(n + 1) + \\gamma, & s = 1, \\\\\n"
            "\\zeta(s) - \\zeta(s, n + 1), & s \\ne 1.\n"
            "\\end{cases}",
        )
        self.assertIn(
            "compactWidth > renderedContentWidth() + 1",
            mars_lab.INDEX_HTML,
        )
        self.assertIn(
            "rendered.dataset.responsiveFit === 'true'",
            mars_lab.INDEX_HTML,
        )
        self.assertEqual(
            mars_lab.restore_compact_binding_values(
                f"{{ x | x = {abbreviated} }}",
                f"{{ x | x = {integer} }}",
            ),
            f"{{ x | x = {integer} }}",
        )
        self.assertIn("const COMPACT_INTEGER_DIGITS_KEEP = 23;", mars_lab.INDEX_HTML)
        self.assertIn("return `${prefix}${sign}${mantissa}...e+${significantDigits.length - 1}`;", mars_lab.INDEX_HTML)
        self.assertIn("[x×]10\\^(?:\\{([+-]?\\d+)\\}|([+-]?\\d+))/g", mars_lab.INDEX_HTML)
        self.assertIn('id="valueMore">Show more digits</button>', mars_lab.INDEX_HTML)
        self.assertIn("setExpandableText(value, valueMore, full, full);", mars_lab.INDEX_HTML)
        self.assertNotIn("setExpandableText(value, valueMore, compactLongNumericTokens(full), full);", mars_lab.INDEX_HTML)
        self.assertIn("value.dataset.fullText || value.textContent", mars_lab.INDEX_HTML)
        self.assertIn("#parsed {\n      overflow-x: hidden;\n      overflow-wrap: anywhere;", mars_lab.INDEX_HTML)
        self.assertIn("#value {\n      min-width: 0;\n      max-width: 100%;\n      overflow-x: hidden;", mars_lab.INDEX_HTML)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_large_integer_cards_retain_the_exact_expandable_text(self) -> None:
        integer = "1332293859845693485943876543210987654321098765"
        abbreviated = "1.3322938598456934859438...e+45"
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, integer, 64, "x", "evaluate"
        )

        self.assertEqual(returncode, 0, raw)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, fields, integer, 64, False, wrt="x", action="evaluate"
        )
        self.assertEqual(payload["display_expression"], abbreviated)
        self.assertEqual(payload["full_display_expression"], integer)
        self.assertEqual(payload["display_TeX"], r"1.3322938598456934859438...\times 10^{45}")
        self.assertEqual(payload["full_display_TeX"], integer)
        self.assertIn(abbreviated, payload["display_function"])
        self.assertIn(integer, payload["full_display_function"])
        self.assertEqual(payload["value"], integer)

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
            "          Array.isArray(data.binding_values) ? data.binding_values : [],\n"
            "          updated\n"
            "        )",
            mars_lab.INDEX_HTML,
        )

    def test_expression_binding_kind_toggle_round_trips_the_explicit_bindings(self) -> None:
        toggle = mars_lab.INDEX_HTML.split(
            "async function toggleBindingKind(binding) {", 1
        )[1].split("\n    function displayValueForBinding", 1)[0]

        self.assertIn("const updated = replaceBindingKindInExpression(current, name, nextKind);", toggle)
        self.assertIn(
            "if (currentMode() === 'expression') {\n"
            "        await applyMarsBindingExpression(updated, expr.value.trim());\n"
            "        return;\n"
            "      }",
            toggle,
        )
        self.assertLess(
            toggle.index("await applyMarsBindingExpression(updated, expr.value.trim());"),
            toggle.index("applyUpdatedBindingExpression(updated);"),
        )

        apply_binding = mars_lab.INDEX_HTML.split(
            "async function applyMarsBindingExpression(updated, editorBodyText = null) {", 1
        )[1].split("\n    function applyMarsBindingsToEditedExpression", 1)[0]
        self.assertIn("const editorBody = expressionBodyForEditor(editorBodyText);", apply_binding)
        self.assertIn(
            "expressionWithBindings(editorBody, bindings) || editorBody,\n"
            "            bindings,\n"
            "            editorBody,",
            apply_binding,
        )
        self.assertIn("data.expression || updated,", apply_binding)

    def test_expression_value_card_visibility_follows_the_payload(self) -> None:
        expression_evaluation = mars_lab.INDEX_HTML.split(
            "async function evaluateExpression(options = {}) {", 1
        )[1].split("\n    async function evaluateMatrix", 1)[0]

        self.assertIn(
            "setValueCardVisible(Boolean(String(value.textContent || '').trim()));",
            expression_evaluation,
        )
        self.assertIn(
            "setValueCardVisible(Boolean(String(value.textContent || '').trim()));",
            mars_lab.INDEX_HTML,
        )

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_expression_cards_keep_algebra_separate_from_bound_value(self) -> None:
        expression = "{ x^2 + y | x = 3, y = 4 }"
        fields, _, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary,
            expression,
            32,
            "x",
            "evaluate",
        )

        self.assertEqual(returncode, 0)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            fields,
            expression,
            32,
            False,
            action="evaluate",
        )

        self.assertEqual(payload["full_display_TeX"], "x^{2} + y")
        self.assertEqual(payload["full_display_expression"], "{ x² + y | x = 3, y = 4 }")
        self.assertIn("return y + x^2.", payload["full_display_function"])
        self.assertIn("x = 3", payload["full_display_function"])
        self.assertIn("y = 4", payload["full_display_function"])
        self.assertEqual(payload["value"], "13")

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_expression_without_a_numerical_result_omits_value(self) -> None:
        expression = "{ x^2 + y | x = ?, y = ? }"
        fields, _, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary,
            expression,
            32,
            "x",
            "evaluate",
        )

        self.assertEqual(returncode, 0)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            fields,
            expression,
            32,
            False,
            action="evaluate",
        )

        self.assertEqual(payload["full_display_expression"], "{ x² + y | x = ?, y = ? }")
        self.assertIn("x = ?", payload["full_display_function"])
        self.assertIn("y = ?", payload["full_display_function"])
        self.assertNotIn("value", payload)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_large_exponential_sine_sum_has_a_formula_and_value(self) -> None:
        # README example: the exponential-sine progression has a bounded-work formula and Value.
        expression = "{ @Z_(k=1)^n exp(kx)sin(kx) | x = 1; n = 100000000 }"
        fields, _, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary,
            expression,
            128,
            "x",
            "evaluate",
        )

        self.assertEqual(returncode, 0)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            fields,
            expression,
            128,
            False,
            action="evaluate",
        )

        self.assertIn(r"\sum_{k=1}^{n}e^{k\mkern-2mu x}", payload["full_display_TeX"])
        self.assertIn(r"= \frac{e^{x}\mkern-2mu \sin(x) -", payload["full_display_TeX"])
        self.assertIn("exp(x)·sin(x) - exp(x·(n + 1))·sin(x·(n + 1))", payload["full_display_expression"])
        self.assertIn("+ sin(nx)·exp(x·(n + 2))", payload["full_display_expression"])
        self.assertIn("1 - 2·exp(x)·cos(x) + exp(2x)", payload["full_display_expression"])
        self.assertIn("return (v1.sin(x) - exp(v2).sin(v2)", payload["full_display_function"])
        self.assertTrue(payload["full_display_function"].startswith("` Σ_(k=1)^n exp(kx)sin(kx) `\n"))
        self.assertIn("x = 1.", payload["full_display_function"])
        self.assertIn("const n = 100000000.", payload["full_display_function"])
        self.assertTrue(payload["value"].startswith("1.804482674473709321302888821113364953"))
        self.assertTrue(payload["value"].endswith("E+43429448"))

        small_expression = "{ @Z_(k=1)^n exp(kx)sin(kx) | x = 0.2; n = 5 }"
        small_fields, _, small_returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary,
            small_expression,
            64,
            "x",
            "evaluate",
        )
        self.assertEqual(small_returncode, 0)
        expected = sum(math.exp(k * 0.2) * math.sin(k * 0.2) for k in range(1, 6))
        self.assertAlmostEqual(float(small_fields["value"]), expected, places=14)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_oversized_unsupported_sum_explains_why_no_value_was_computed(self) -> None:
        # README example: an oversized sum without a shortcut reports why its Value is unavailable.
        expression = "{ @Z_(k=1)^n asin(kx) | x = 0.2; n = 100000000 }"
        fields, _, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary,
            expression,
            64,
            "x",
            "evaluate",
        )
        self.assertEqual(returncode, 0)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            fields,
            expression,
            64,
            False,
            action="evaluate",
        )

        self.assertNotIn("value", payload)
        self.assertEqual(
            payload["value_note"],
            "Value not computed: the finite sum exceeds the safe direct-evaluation limit and has no supported "
            "numerical shortcut.",
        )

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_hurwitz_zeta_with_supplied_parameters_includes_value(self) -> None:
        cases = {
            "{ zeta(p, a) | p = 2.5; a = 101 }": "0.00066168749945317154206221150147971",
            "{ zetap(p, a) | p = 2.5; a = 101 }": "-0.0034916196565303381067445584043472",
        }

        for expression, expected in cases.items():
            with self.subTest(expression=expression):
                fields, _, returncode = mars_lab.run_mars_lab_fields(
                    self.expression_binary,
                    expression,
                    32,
                    "p",
                    "evaluate",
                )

                self.assertEqual(returncode, 0)
                payload = mars_lab.prepare_evaluation_fields(
                    self.expression_binary,
                    fields,
                    expression,
                    32,
                    False,
                    wrt="p",
                    action="evaluate",
                )

                self.assertEqual(payload["evaluation_ready"], "yes")
                self.assertEqual(payload["value"], expected)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_bound_power_series_keep_generic_algebraic_cards(self) -> None:
        cases = (
            (
                "1+1/2^p+1/3^p+...+1/n^p | p=-2; n=100",
                "{ ζ(p) - ζ(p, n + 1) | p = -2; n = 100 }",
                r"\sum_{k=1}^{n}\frac{1}{k^{p}}",
                r"\zeta(p) - \zeta(p, n + 1)",
                "return zeta(p) - zetah(p, n + 1).",
            ),
            (
                "1+2^p+3^p+4^p+...+n^p | p=2; n=100",
                "{ ζ(-p) - ζ(-p, n + 1) | p = 2; n = 100 }",
                r"\sum_{k=1}^{n}k^{p}",
                r"\zeta(-p) - \zeta(-p, n + 1)",
                "zeta(-p) - zetah(-p, n + 1)",
            ),
        )

        for source, expected_expression, expected_sum, expected_formula, expected_function in cases:
            with self.subTest(source=source):
                fields, raw, returncode = mars_lab.run_mars_lab_fields(
                    self.expression_binary, source, 64, "p", "evaluate"
                )

                self.assertEqual(returncode, 0, raw)
                payload = mars_lab.prepare_evaluation_fields(
                    self.expression_binary, fields, source, 64, False, wrt="p", action="evaluate"
                )

                self.assertEqual(payload["full_display_expression"], expected_expression)
                self.assertIn(expected_sum, payload["full_display_TeX"])
                self.assertIn(expected_formula, payload["full_display_TeX"])
                self.assertNotIn(r"n^{3}", payload["full_display_TeX"])
                self.assertIn(expected_function, payload["full_display_function"])
                self.assertEqual(payload["value"], "338350")

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_symbolic_inverse_power_series_uses_one_outer_equation_line_when_it_fits(self) -> None:
        source = "1+1/2^s+1/3^s+1/4^s+...+1/n^s"
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, source, 64, "s", "evaluate"
        )

        self.assertEqual(returncode, 0, raw)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, fields, source, 64, False, wrt="s", action="evaluate"
        )
        self.assertTrue(
            payload["display_TeX"].startswith(
                r"\sum_{k=1}^{n}\frac{1}{k^{s}} = \begin{cases}"
            )
        )
        self.assertNotIn(r"\begin{aligned}", payload["display_TeX"])
        self.assertTrue(payload["display_wrapped_TeX"].startswith(r"\begin{aligned}[t]"))
        self.assertIn(
            "\\sum_{k=1}^{n}\\frac{1}{k^{s}} \\\\\n&= \\begin{cases}",
            payload["display_wrapped_TeX"],
        )

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_bound_inverse_power_series_derivative_value_uses_the_generic_derivative(self) -> None:
        source = "1+1/2^p+1/3^p+...+1/n^p | p=-2; n=1000"
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, source, 64, "p", "derivative"
        )

        self.assertEqual(returncode, 0, raw)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, fields, source, 64, False, wrt="p", action="derivative"
        )

        self.assertEqual(
            payload["derivative"],
            "d/dp = { ζ'(p) - ζ'(p, n + 1) | p = -2; n = 1000 }",
        )
        self.assertEqual(payload["derivative_TeX"], r"\zeta'(p) - \zeta'(p, n + 1)")
        self.assertIn("p = -2.", payload["full_display_derivative_function"])
        self.assertIn("const n = 1000.", payload["full_display_derivative_function"])
        expected = -sum(k * k * math.log(k) for k in range(1, 1001))
        self.assertAlmostEqual(float(payload["derivative_value"]), expected, delta=abs(expected) * 1e-14)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_inverse_power_series_AD_value_cancels_the_zeta_poles_at_p_equals_one(self) -> None:
        source = "1+1/2^p+1/3^p+1/4^p+...+1/n^p | p=1; n=1000"
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, source, 64, "p", "derivative"
        )

        self.assertEqual(returncode, 0, raw)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, fields, source, 64, False, wrt="p", action="derivative"
        )

        self.assertEqual(
            payload["derivative"],
            "d/dp = { ζ'(p) - ζ'(p, n + 1) | p = 1; n = 1000 }",
        )
        expected = -sum(math.log(k) / k for k in range(1, 1001))
        self.assertAlmostEqual(float(payload["derivative_value"]), expected, delta=abs(expected) * 1e-14)

        huge_n = "1" + "0" * 57
        huge_source = f"1+1/2^p+1/3^p+1/4^p+...+1/n^p | p=1; n={huge_n}"
        huge_fields, huge_raw, huge_returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, huge_source, 64, "p", "derivative"
        )
        self.assertEqual(huge_returncode, 0, huge_raw)
        huge_payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, huge_fields, huge_source, 64, False, wrt="p", action="derivative"
        )
        self.assertAlmostEqual(float(huge_payload["derivative_value"]), -8612.860664626674, places=10)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_inverse_power_series_uses_one_domain_correct_algebra_at_p_equals_one(self) -> None:
        source = "1+1/2^p+1/3^p+1/4^p+...+1/n^p | p=1; n=100"
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, source, 64, "p", "evaluate"
        )

        self.assertEqual(returncode, 0, raw)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, fields, source, 64, False, wrt="p", action="evaluate"
        )
        rendered = payload["full_display_TeX"]
        self.assertEqual(fields["algebraic_specialisation"], "domain-required")
        self.assertIn(r"\sum_{k=1}^{n}\frac{1}{k^{p}}", rendered)
        self.assertIn(r"\psi^{(0)}(n + 1) + \gamma", rendered)
        self.assertNotIn(r"\zeta", rendered)
        self.assertEqual(payload["full_display_expression"], "{ ψ⁽⁰⁾(n + 1) + γ | ; n = 100 }")
        self.assertIn("return digamma(n + 1) + @gamma.", payload["full_display_function"])
        self.assertNotIn("zeta(", payload["full_display_function"])
        self.assertEqual(
            payload["binding_values"],
            [
                {"name": "n", "value": "100", "display": "100", "kind": "constant"},
            ],
        )

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_inverse_square_series_at_infinity_uses_exact_algebra_in_every_card(self) -> None:
        source = "1+1/2^2+1/3^2+1/4^2+...+1/n^2 | n=inf"
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, source, 64, "n", "evaluate"
        )

        self.assertEqual(returncode, 0, raw)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, fields, source, 64, False, wrt="n", action="evaluate"
        )
        self.assertEqual(fields["algebraic_specialisation"], "domain-required")
        self.assertEqual(payload["full_display_expression"], "π²/6")
        self.assertIn(r"\sum_{k=1}^{\infty}\frac{1}{k^{2}}", payload["full_display_TeX"])
        self.assertIn(r"\frac{\pi^{2}}{6}", payload["full_display_TeX"])
        self.assertNotIn(r"\psi", payload["full_display_TeX"])
        self.assertIn("return @pi^2/6.", payload["full_display_function"])
        self.assertNotIn("trigamma", payload["full_display_function"])
        self.assertTrue(payload["value"].startswith("1.64493406684822643647"))

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_inverse_square_series_accepts_a_literal_infinite_terminal_term(self) -> None:
        source = "1+1/2^2+1/3^2+1/4^2+...+1/inf^2"
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, source, 64, "x", "evaluate"
        )

        self.assertEqual(returncode, 0, raw)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, fields, source, 64, False, wrt="x", action="evaluate"
        )
        self.assertEqual(payload["full_display_expression"], "π²/6")
        self.assertIn(r"\sum_{n=1}^{\infty}\frac{1}{n^{2}}", payload["full_display_TeX"])
        self.assertIn(r"\frac{\pi^{2}}{6}", payload["full_display_TeX"])
        self.assertIn("return @pi^2/6.", payload["full_display_function"])
        self.assertTrue(payload["value"].startswith("1.64493406684822643647"))

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_inverse_square_series_accepts_a_trailing_ellipsis(self) -> None:
        source = "1+1/2^2+1/3^2+1/4^2+..."
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, source, 64, "x", "evaluate"
        )

        self.assertEqual(returncode, 0, raw)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, fields, source, 64, False, wrt="x", action="evaluate"
        )
        self.assertEqual(fields["algebraic_specialisation"], "domain-required")
        self.assertEqual(payload["full_display_expression"], "π²/6")
        self.assertEqual(
            payload["display_TeX"],
            r"\sum_{n=1}^{\infty}\frac{1}{n^{2}} = \frac{\pi^{2}}{6}",
        )
        self.assertIn(r"\sum_{n=1}^{\infty}\frac{1}{n^{2}}", payload["full_display_TeX"])
        self.assertIn(r"\frac{\pi^{2}}{6}", payload["full_display_TeX"])
        self.assertIn("return @pi^2/6.", payload["full_display_function"])
        self.assertTrue(payload["value"].startswith("1.64493406684822643647"))

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_sine_progression_ellipsis_uses_the_geometric_series_closed_form(self) -> None:
        source = "sin(1)+sin(2)+sin(3)+sin(4)+...+sin(n)"
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, source, 64, "n", "evaluate"
        )

        self.assertEqual(returncode, 0, raw)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, fields, source, 64, False, wrt="n", action="evaluate"
        )
        self.assertEqual(
            payload["full_display_expression"],
            "{ sin(n/2)·sin(1/2·(n + 1))/sin(½) | ; n = ? }",
        )
        self.assertEqual(
            payload["display_TeX"],
            r"\sum_{k=1}^{n}\sin(k) = "
            r"\frac{\sin(\frac{n}{2})\mkern-2mu \sin(\frac{n + 1}{2})}{\sin(\frac{1}{2})}",
        )
        self.assertIn("return sin(n/2).sin((n + 1)/2)/sin(1/2).", payload["full_display_function"])
        self.assertNotIn("value", payload)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_cosine_progression_with_symbolic_step_uses_the_geometric_series_closed_form(self) -> None:
        source = "cos(x)+cos(2x)+cos(3x)+cos(4x)+...+cos(nx)"
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, source, 64, "x", "evaluate"
        )

        self.assertEqual(returncode, 0, raw)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, fields, source, 64, False, wrt="x", action="evaluate"
        )
        self.assertEqual(
            payload["full_display_expression"],
            "{ sin(nx/2)·cos(x/2·(n + 1))/sin(x/2) | x = ?; n = ? }",
        )
        self.assertEqual(
            payload["display_TeX"],
            r"\sum_{k=1}^{n}\cos(k\mkern-2mu x) = "
            r"\frac{\sin(\frac{n\mkern-2mu x}{2})\mkern-2mu "
            r"\cos(\frac{x}{2}\mkern-2mu \left(n + 1\right))}{\sin(\frac{x}{2})}",
        )
        self.assertIn("v1 = x/2.", payload["full_display_function"])
        self.assertIn("return sin(n.v1).cos((n + 1).v1)/sin(v1).", payload["full_display_function"])

        bound_source = "{ cos(x)+cos(2x)+cos(3x)+cos(4x)+...+cos(nx) | x=0.5; n=4 }"
        bound_fields, bound_raw, bound_returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, bound_source, 64, "x", "evaluate"
        )
        self.assertEqual(bound_returncode, 0, bound_raw)
        bound_payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, bound_fields, bound_source, 64, False, wrt="x", action="evaluate"
        )
        expected = sum(math.cos(k * 0.5) for k in range(1, 5))
        self.assertAlmostEqual(float(bound_payload["value"]), expected, places=15)

        zero_source = "{ cos(x)+cos(2x)+cos(3x)+cos(4x)+...+cos(nx) | x=0; n=4 }"
        zero_fields, zero_raw, zero_returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, zero_source, 64, "x", "evaluate"
        )
        self.assertEqual(zero_returncode, 0, zero_raw)
        zero_payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, zero_fields, zero_source, 64, False, wrt="x", action="evaluate"
        )
        self.assertEqual(zero_payload["value"], "4")

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_tangent_progression_displays_qdigamma_formula_and_large_value(self) -> None:
        source = "{ @Z_(k=1)^n tan(kx) | x=2; n=100000 }"
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, source, 64, "x", "evaluate"
        )

        self.assertEqual(returncode, 0, raw)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, fields, source, 64, False, wrt="x", action="evaluate"
        )
        self.assertIn(r"\sum_{k=1}^{n}\tan(k\mkern-2mu x) =", payload["display_TeX"])
        self.assertIn(r"\psi_{e^{2\mkern-2mu i\mkern-2mu x}}", payload["display_TeX"])
        self.assertIn(r"\psi_{e^{4\mkern-2mu i\mkern-2mu x}}", payload["display_TeX"])
        self.assertNotIn(r"\ln", payload["display_TeX"])
        self.assertIn("ψq(exp(2ix), 1)", payload["full_display_expression"])
        self.assertIn("ψq(exp(4ix), 1)", payload["full_display_expression"])
        self.assertNotIn("ln(", payload["full_display_expression"])
        self.assertIn("qdigamma(", payload["full_display_function"])
        self.assertNotIn("ln(", payload["full_display_function"])
        self.assertTrue(payload["value"].startswith("-220771.582329280672442172970752"))

        round_trip_source = payload["full_display_expression"]
        round_trip_fields, round_trip_raw, round_trip_returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, round_trip_source, 64, "x", "evaluate"
        )
        self.assertEqual(round_trip_returncode, 0, round_trip_raw)
        round_trip_payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            round_trip_fields,
            round_trip_source,
            64,
            False,
            wrt="x",
            action="evaluate",
        )
        self.assertIn(r"\sum_{k=1}^{n}\tan(k\mkern-2mu x) =", round_trip_payload["display_TeX"])
        self.assertEqual(round_trip_payload["value"], payload["value"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_arctangent_progression_displays_sign_aware_loggamma_formula_and_large_real_value(self) -> None:
        source = "{ @Z_(k=1)^n atan(kx) | x=2; n=100000 }"
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, source, 64, "x", "evaluate"
        )

        self.assertEqual(returncode, 0, raw)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, fields, source, 64, False, wrt="x", action="evaluate"
        )
        self.assertIn(r"\sum_{k=1}^{n}\arctan(k\mkern-2mu x) =", payload["display_TeX"])
        self.assertIn(r"\frac{\pi\mkern-2mu n\mkern-2mu x}{2\mkern-2mu \left|x\right|}",
                      payload["display_TeX"])
        self.assertIn(r"\frac{i}{2}\mkern-2mu", payload["display_TeX"])
        self.assertNotIn(r"{2\mkern-2mu i}", payload["display_TeX"])
        self.assertIn(r"\ln\Gamma(n + 1 - \frac{i}{x})", payload["display_TeX"])
        self.assertNotIn(r"\ln(-i\mkern-2mu x)", payload["display_TeX"])
        self.assertIn("πnx/(2·|x|) + i/2·(", payload["full_display_expression"])
        self.assertNotIn("1/(2i)", payload["full_display_expression"])
        self.assertIn("lnΓ(n + 1 - i/x)", payload["full_display_expression"])
        self.assertNotIn("Σ_", payload["full_display_expression"])
        self.assertIn("lgamma(", payload["full_display_function"])
        self.assertIn("abs(x)", payload["full_display_function"])
        self.assertIn("return v2 + v3.", payload["full_display_function"])
        self.assertEqual(
            payload["value"],
            "157073.6321559582734645136309678092183947491788179974462371538223",
        )
        self.assertNotIn("i", payload["value"])

        negative_source = "{ @Z_(k=1)^n atan(kx) | x=-2; n=10 }"
        negative_fields, negative_raw, negative_returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, negative_source, 64, "x", "evaluate"
        )
        self.assertEqual(negative_returncode, 0, negative_raw)
        negative_payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, negative_fields, negative_source, 64, False, wrt="x", action="evaluate"
        )
        self.assertEqual(
            negative_payload["value"],
            "-14.28784026373139711945372609242272757413654508433062231378199652",
        )

        zero_source = "{ @Z_(k=1)^n atan(kx) | x=0; n=100000 }"
        zero_fields, zero_raw, zero_returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, zero_source, 64, "x", "evaluate"
        )
        self.assertEqual(zero_returncode, 0, zero_raw)
        zero_payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, zero_fields, zero_source, 64, False, wrt="x", action="evaluate"
        )
        self.assertEqual(zero_payload["value"], "0")

        derivative_source = "{ @Z_(k=1)^n atan(kx) | x=1; n=1000000000 }"
        derivative_fields, derivative_raw, derivative_returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, derivative_source, 386, "x", "derivative"
        )
        self.assertEqual(derivative_returncode, 0, derivative_raw)
        derivative_payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            derivative_fields,
            derivative_source,
            386,
            False,
            wrt="x",
            action="derivative",
        )
        self.assertNotIn("0 +", derivative_payload["derivative"])
        self.assertNotIn(r"\mkern-2mu i\right)\mkern-2mu i", derivative_payload["derivative_TeX"])
        self.assertIn(r"\psi^{(0)}(n + \frac{i}{x} + 1)", derivative_payload["derivative_TeX"])
        self.assertIn(r"{2\mkern-2mu x^{2}}", derivative_payload["derivative_TeX"])
        self.assertNotIn("i", derivative_payload["derivative_value"])
        self.assertEqual(len(derivative_payload["derivative_value"].replace(".", "")), 386)
        self.assertTrue(derivative_payload["derivative_value"].startswith("20.6286155168239341793067112756040338"))

        integral_fields, integral_raw, integral_returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, derivative_source, 386, "x", "integral"
        )
        self.assertEqual(integral_returncode, 0, integral_raw)
        integral_payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            integral_fields,
            derivative_source,
            386,
            False,
            wrt="x",
            action="integral",
        )
        self.assertIn("Σ_(k=1)^n", integral_payload["integral"])
        self.assertIn("atan(kx)", integral_payload["integral"])
        self.assertIn("ln(k²x² + 1)", integral_payload["integral"])
        self.assertNotIn("integral_value", integral_payload)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_large_arcsine_progression_keeps_its_direct_sum_visible_with_its_value(self) -> None:
        source = "{ @Z_(k=1)^n asin(kx) | x=pi/7; n=100000 }"
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, source, 64, "x", "evaluate"
        )

        self.assertEqual(returncode, 0, raw)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, fields, source, 64, False, wrt="x", action="evaluate"
        )
        self.assertEqual(payload["display_TeX"], r"\sum_{k=1}^{n}\sin^{-1}(k\mkern-2mu x)")
        self.assertIn("return sum(k, 1, n, asin(k.x)).", payload["full_display_function"])
        self.assertTrue(payload["value"])
        self.assertNotEqual(payload["value"], "NAN")

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_all_inverse_progressions_expose_their_formula_or_direct_sum_with_values(self) -> None:
        formula_functions = {"atan", "acot", "atanh", "acoth"}
        functions = (
            "asin",
            "acos",
            "atan",
            "asec",
            "acosec",
            "acot",
            "arcversin",
            "arcvercos",
            "arccoversin",
            "arccovercos",
            "archaversin",
            "archavercos",
            "archacoversin",
            "archacovercos",
            "asinh",
            "acosh",
            "atanh",
            "asech",
            "acosech",
            "acoth",
        )

        for function in functions:
            with self.subTest(function=function):
                source = f"{{ @Z_(k=1)^n {function}(kx) | x=0.00001; n=10001 }}"
                fields, raw, returncode = mars_lab.run_mars_lab_fields(
                    self.expression_binary, source, 64, "x", "evaluate"
                )
                self.assertEqual(returncode, 0, raw)
                payload = mars_lab.prepare_evaluation_fields(
                    self.expression_binary, fields, source, 64, False, wrt="x", action="evaluate"
                )
                self.assertTrue(payload["display_TeX"].startswith(r"\sum_{k=1}^{n}"))
                if function in formula_functions:
                    self.assertIn(" = ", payload["display_TeX"])
                    self.assertIn("lgamma(", payload["full_display_function"])
                    self.assertNotIn("return sum(", payload["full_display_function"])
                    self.assertTrue(payload["value"])
                    self.assertNotEqual(payload["value"], "NAN")
                else:
                    self.assertNotIn(" = ", payload["display_TeX"])
                    self.assertIn("return sum(k, 1, n,", payload["full_display_function"])
                    self.assertTrue(payload["value"])
                    self.assertNotEqual(payload["value"], "NAN")

                small_source = f"{{ @Z_(k=1)^n {function}(kx) | x=0.00001; n=9 }}"
                small_fields, small_raw, small_returncode = mars_lab.run_mars_lab_fields(
                    self.expression_binary, small_source, 64, "x", "evaluate"
                )
                self.assertEqual(small_returncode, 0, small_raw)
                small_payload = mars_lab.prepare_evaluation_fields(
                    self.expression_binary,
                    small_fields,
                    small_source,
                    64,
                    False,
                    wrt="x",
                    action="evaluate",
                )
                self.assertTrue(small_payload["value"])
                self.assertNotEqual(small_payload["value"], "NAN")

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_large_inverse_progression_displays_signed_infinity_at_an_exact_pole(self) -> None:
        # Regression: a known inverse-function pole is a signed infinite Value, not an omitted NaN Value.
        for function, step, expected in (("acoth", "1", "∞"), ("acoth", "-1", "-∞"),
                                         ("atanh", "1/2", "∞")):
            with self.subTest(function=function, step=step):
                source = f"{{ @Z_(k=1)^n {function}(kx) | x={step}; n=1000000000 }}"
                fields, raw, returncode = mars_lab.run_mars_lab_fields(
                    self.expression_binary, source, 64, "x", "evaluate"
                )
                self.assertEqual(returncode, 0, raw)
                payload = mars_lab.prepare_evaluation_fields(
                    self.expression_binary, fields, source, 64, False, wrt="x", action="evaluate"
                )
                self.assertEqual(payload["value"], expected)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_tanh_progression_displays_qdigamma_formula_and_large_value(self) -> None:
        # README example: a large tanh progression exposes and evaluates its q-digamma identity.
        source = "{ @Z_(k=1)^n tanh(kx) | x=2; n=100000 }"
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, source, 64, "x", "evaluate"
        )

        self.assertEqual(returncode, 0, raw)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, fields, source, 64, False, wrt="x", action="evaluate"
        )
        self.assertIn(r"\sum_{k=1}^{n}\tanh(k\mkern-2mu x) =", payload["display_TeX"])
        self.assertIn(r"\psi_{e^{-2\mkern-2mu x}}", payload["display_TeX"])
        self.assertIn("ψq(exp(-2x), 1)", payload["full_display_expression"])
        self.assertIn("qdigamma(v2, 1)", payload["full_display_function"])
        self.assertEqual(
            payload["value"],
            "99999.96334436219613677993759997968616482017617755510022062027824",
        )

        round_trip_source = payload["full_display_expression"]
        round_trip_fields, round_trip_raw, round_trip_returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, round_trip_source, 64, "x", "evaluate"
        )
        self.assertEqual(round_trip_returncode, 0, round_trip_raw)
        round_trip_payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            round_trip_fields,
            round_trip_source,
            64,
            False,
            wrt="x",
            action="evaluate",
        )
        self.assertIn(r"\sum_{k=1}^{n}\tanh(k\mkern-2mu x) =", round_trip_payload["display_TeX"])
        self.assertEqual(round_trip_payload["value"], payload["value"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_quotient_progressions_display_qdigamma_formulae_values_and_round_trip(self) -> None:
        cases = {
            "cot": (r"\cot", lambda angle: 1.0 / math.tan(angle)),
            "sec": (r"\sec", lambda angle: 1.0 / math.cos(angle)),
            "cosec": (r"\operatorname{cosec}", lambda angle: 1.0 / math.sin(angle)),
            "coth": (r"\coth", lambda angle: 1.0 / math.tanh(angle)),
            "sech": (r"\operatorname{sech}", lambda angle: 1.0 / math.cosh(angle)),
            "cosech": (r"\operatorname{cosech}", lambda angle: 1.0 / math.sinh(angle)),
        }

        for function, (function_TeX, numerical_function) in cases.items():
            with self.subTest(function=function):
                source = f"{{ @Z_(k=1)^n {function}(kx) | x=0.2; n=5 }}"
                fields, raw, returncode = mars_lab.run_mars_lab_fields(
                    self.expression_binary, source, 64, "x", "evaluate"
                )
                self.assertEqual(returncode, 0, raw)
                payload = mars_lab.prepare_evaluation_fields(
                    self.expression_binary, fields, source, 64, False, wrt="x", action="evaluate"
                )
                self.assertIn(rf"\sum_{{k=1}}^{{n}}{function_TeX}(k\mkern-2mu x) =", payload["display_TeX"])
                self.assertIn(r"\psi_", payload["display_TeX"])
                self.assertIn("ψq(", payload["full_display_expression"])
                self.assertIn("qdigamma(", payload["full_display_function"])
                expected = sum(numerical_function(k * 0.2) for k in range(1, 6))
                self.assertAlmostEqual(float(payload["value"].split()[0]), expected, places=14)

                round_trip_source = payload["full_display_expression"]
                round_trip_fields, round_trip_raw, round_trip_returncode = mars_lab.run_mars_lab_fields(
                    self.expression_binary, round_trip_source, 64, "x", "evaluate"
                )
                self.assertEqual(round_trip_returncode, 0, round_trip_raw)
                round_trip_payload = mars_lab.prepare_evaluation_fields(
                    self.expression_binary,
                    round_trip_fields,
                    round_trip_source,
                    64,
                    False,
                    wrt="x",
                    action="evaluate",
                )
                self.assertIn(rf"\sum_{{k=1}}^{{n}}{function_TeX}(k\mkern-2mu x) =", round_trip_payload["display_TeX"])
                self.assertAlmostEqual(float(round_trip_payload["value"].split()[0]), expected, places=14)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_exponential_progression_displays_its_geometric_formula_and_large_value(self) -> None:
        # Regression: a large exponential progression exposes its geometric identity and value.
        source = "{ @Z_(k=1)^n exp(kx) | x=2; n=100000 }"
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, source, 64, "x", "evaluate"
        )

        self.assertEqual(returncode, 0, raw)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, fields, source, 64, False, wrt="x", action="evaluate"
        )
        self.assertIn(r"\sum_{k=1}^{n}e^{k\mkern-2mu x} =", payload["display_TeX"])
        self.assertIn(r"\frac{e^{x}}{e^{x} - 1}", payload["display_TeX"])
        self.assertIn("exp(x)/(exp(x) - 1)·(exp(nx) - 1)", payload["full_display_expression"])
        self.assertNotIn("Σ_", payload["full_display_expression"])
        self.assertIn("v1 = exp(x).", payload["full_display_function"])
        self.assertIn("return v1.(exp(n.x) - 1)/(v1 - 1).", payload["full_display_function"])
        self.assertEqual(
            payload["value"],
            "9.110304914770879911502042940141264278041407847643843263784059825E+86858",
        )

        zero_source = "{ @Z_(k=1)^n exp(kx) | x=0; n=100000 }"
        zero_fields, zero_raw, zero_returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, zero_source, 64, "x", "evaluate"
        )
        self.assertEqual(zero_returncode, 0, zero_raw)
        zero_payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, zero_fields, zero_source, 64, False, wrt="x", action="evaluate"
        )
        self.assertEqual(zero_payload["value"], "100000")

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_logarithmic_progression_displays_its_loggamma_formula_and_large_value(self) -> None:
        source = "{ @Z_(k=1)^n ln(kx) | x=2; n=100000 }"
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, source, 64, "x", "evaluate"
        )

        self.assertEqual(returncode, 0, raw)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, fields, source, 64, False, wrt="x", action="evaluate"
        )
        self.assertEqual(
            payload["display_TeX"],
            r"\sum_{k=1}^{n}\ln(k\mkern-2mu x) = n\mkern-2mu \ln(x) + \ln\Gamma(n + 1)",
        )
        self.assertEqual(
            payload["full_display_expression"],
            "{ n·ln(x) + lnΓ(n + 1) | x = 2; n = 100000 }",
        )
        self.assertNotIn("Σ_", payload["full_display_expression"])
        self.assertIn("return n.ln(x) + lgamma(n + 1).", payload["full_display_function"])
        self.assertEqual(
            payload["value"],
            "1120613.939955116396071001320351928512056894536584146906526018233",
        )

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_common_logarithmic_progression_uses_a_distinct_loggamma_formula(self) -> None:
        source = "{ @Z_(k=1)^n log(kx) | x=2; n=100000 }"
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, source, 64, "x", "evaluate"
        )

        self.assertEqual(returncode, 0, raw)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, fields, source, 64, False, wrt="x", action="evaluate"
        )
        self.assertEqual(
            payload["display_TeX"],
            r"\sum_{k=1}^{n}\lg(k\mkern-2mu x) = n\mkern-2mu \lg(x) + "
            r"\frac{\ln\Gamma(n + 1)}{\ln(10)}",
        )
        self.assertEqual(
            payload["full_display_expression"],
            "{ n·lg(x) + lnΓ(n + 1)/ln(10) | x = 2; n = 100000 }",
        )
        self.assertNotIn("Σ_", payload["full_display_expression"])
        self.assertIn("return n.lg(x) + lgamma(n + 1)/ln(10).", payload["full_display_function"])
        self.assertEqual(
            payload["value"],
            "486676.4504663690278820372835671954350107214367484924045390786799",
        )

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_versed_progression_family_reuses_exact_sine_and_cosine_sums(self) -> None:
        cases = {
            "versin": lambda angle: 1.0 - math.cos(angle),
            "vercos": lambda angle: 1.0 + math.cos(angle),
            "coversin": lambda angle: 1.0 - math.sin(angle),
            "covercos": lambda angle: 1.0 + math.sin(angle),
            "haversin": lambda angle: (1.0 - math.cos(angle)) / 2.0,
            "havercos": lambda angle: (1.0 + math.cos(angle)) / 2.0,
            "hacoversin": lambda angle: (1.0 - math.sin(angle)) / 2.0,
            "hacovercos": lambda angle: (1.0 + math.sin(angle)) / 2.0,
        }

        for function, numerical_function in cases.items():
            with self.subTest(function=function):
                source = f"{{ @Z_(k=1)^n {function}(kx) | x=0.2; n=5 }}"
                fields, raw, returncode = mars_lab.run_mars_lab_fields(
                    self.expression_binary, source, 64, "x", "evaluate"
                )
                self.assertEqual(returncode, 0, raw)
                payload = mars_lab.prepare_evaluation_fields(
                    self.expression_binary, fields, source, 64, False, wrt="x", action="evaluate"
                )
                self.assertIn(rf"\sum_{{k=1}}^{{n}}\operatorname{{{function}}}(k\mkern-2mu x) =", payload["display_TeX"])
                self.assertNotIn("Σ_", payload["full_display_expression"])
                expected = sum(numerical_function(k * 0.2) for k in range(1, 6))
                self.assertAlmostEqual(float(payload["value"]), expected, places=14)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_homogeneous_progressions_display_formulae_and_large_values(self) -> None:
        cases = {
            "abs": ("|x|·n/2·(n + 1)", "10000100000"),
            "conj": ("conj(x)·n/2·(n + 1)", "10000100000"),
            "sqrt": (
                "√(x)·(ζ(-1/2) - ζ(-1/2, n + 1))",
                "29814463.01298576613569741465397922838928192939324835606473553721",
            ),
            "cubrt": (
                "cubrt(x)·(ζ(-1/3) - ζ(-1/3, n + 1))",
                "4386055.498082581754130848633263596499425225207414401018027595808",
            ),
        }

        for function, (formula, value_prefix) in cases.items():
            with self.subTest(function=function):
                source = f"{{ @Z_(k=1)^n {function}(kx) | x=2; n=100000 }}"
                fields, raw, returncode = mars_lab.run_mars_lab_fields(
                    self.expression_binary, source, 64, "x", "evaluate"
                )
                self.assertEqual(returncode, 0, raw)
                payload = mars_lab.prepare_evaluation_fields(
                    self.expression_binary, fields, source, 64, False, wrt="x", action="evaluate"
                )
                self.assertIn(formula, payload["full_display_expression"])
                self.assertNotIn("Σ_", payload["full_display_expression"])
                self.assertTrue(payload["value"].startswith(value_prefix), payload["value"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_normal_logpdf_progressions_reduce_to_a_quadratic_sum(self) -> None:
        for function in ("normal_logpdf", "logpdf"):
            with self.subTest(function=function):
                source = f"{{ @Z_(k=1)^n {function}(kx) | x=2; n=100000 }}"
                fields, raw, returncode = mars_lab.run_mars_lab_fields(
                    self.expression_binary, source, 64, "x", "evaluate"
                )
                self.assertEqual(returncode, 0, raw)
                payload = mars_lab.prepare_evaluation_fields(
                    self.expression_binary, fields, source, 64, False, wrt="x", action="evaluate"
                )
                self.assertIn("x²·n·(n + 1)/6·(2n + 1)/2", payload["full_display_expression"])
                self.assertIn("n·ln(2π)/2", payload["full_display_expression"])
                self.assertNotIn("Σ_", payload["full_display_expression"])
                self.assertTrue(payload["value"].startswith("-666676666791893.853320467274178"))

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_floor_and_ceiling_progressions_reduce_for_proved_small_rational_steps(self) -> None:
        for function in ("floor", "ceil"):
            with self.subTest(function=function):
                source = f"{{ @Z_(k=1)^n {function}(kx) | x=2; n=100000 }}"
                fields, raw, returncode = mars_lab.run_mars_lab_fields(
                    self.expression_binary, source, 64, "x", "evaluate"
                )
                self.assertEqual(returncode, 0, raw)
                payload = mars_lab.prepare_evaluation_fields(
                    self.expression_binary, fields, source, 64, False, wrt="x", action="evaluate"
                )
                self.assertEqual(payload["full_display_expression"], "{ x·n/2·(n + 1) | x = 2; n = 100000 }")
                self.assertEqual(payload["value"], "10000100000")

        for function, expected_value in (("floor", "2999990000"), ("ceil", "3000070000")):
            with self.subTest(function=function, rational_step="3/5"):
                source = f"{{ @Z_(k=1)^n {function}(kx) | x=0.6; n=100000 }}"
                fields, raw, returncode = mars_lab.run_mars_lab_fields(
                    self.expression_binary, source, 64, "x", "evaluate"
                )
                self.assertEqual(returncode, 0, raw)
                self.assertEqual(fields["algebraic_specialisation"], "domain-required")
                payload = mars_lab.prepare_evaluation_fields(
                    self.expression_binary, fields, source, 64, False, wrt="x", action="evaluate"
                )
                self.assertIn("⌊n/5⌋", payload["full_display_expression"])
                self.assertIn("mod(n, 5)", payload["full_display_expression"])
                self.assertNotIn("Σ_", payload["full_display_expression"])
                self.assertIn("const c1 = n/5.", payload["function"])
                self.assertIn("const c2 = floor(c1).", payload["function"])
                self.assertNotIn("sum(k, 1, n", payload["function"])
                self.assertIn("\\sum_{k=1}^{n}", payload["tex"])
                self.assertIn(" = ", payload["tex"])
                self.assertEqual(payload["value"], expected_value)

        literal_source = "{ @Z_(k=1)^n floor(0.6k) | n=100000 }"
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, literal_source, 64, "x", "evaluate"
        )
        self.assertEqual(returncode, 0, raw)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, fields, literal_source, 64, False, wrt="x", action="evaluate"
        )
        self.assertIn("⌊n/5⌋·(15/2", payload["full_display_expression"])
        self.assertIn("7 + 3·mod(n, 5)", payload["full_display_expression"])
        self.assertNotIn("0.625", payload["full_display_expression"])
        self.assertNotIn("Σ_", payload["full_display_expression"])
        self.assertNotIn("sum(k, 1, n", payload["function"])
        self.assertIn(r"\sum_{k=1}^{n}\left\lfloor 0.6", payload["tex"])
        self.assertIn(" = ", payload["tex"])
        self.assertEqual(payload["value"], "2999990000")
        round_trip_source = payload["full_display_expression"]
        round_trip_fields, round_trip_raw, round_trip_returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, round_trip_source, 64, "x", "evaluate"
        )
        self.assertEqual(round_trip_returncode, 0, round_trip_raw)
        round_trip_payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            round_trip_fields,
            round_trip_source,
            64,
            False,
            wrt="x",
            action="evaluate",
        )
        self.assertEqual(round_trip_payload["value"], payload["value"])

        irrational_source = "{ @Z_(k=1)^n floor(kx) | x=sqrt(2); n=100000 }"
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, irrational_source, 64, "x", "evaluate"
        )
        self.assertEqual(returncode, 0, raw)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, fields, irrational_source, 64, False, wrt="x", action="evaluate"
        )
        self.assertIn("Σ_(k=1)^n", payload["full_display_expression"])
        self.assertNotIn("value", payload)

        bit_not_source = "{ @Z_(k=1)^n not(kx) | x=2; n=100000 }"
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, bit_not_source, 64, "x", "evaluate"
        )
        self.assertEqual(returncode, 0, raw)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, fields, bit_not_source, 64, False, wrt="x", action="evaluate"
        )
        self.assertEqual(payload["full_display_expression"], "{ -x·n/2·(n + 1) - n | x = 2; n = 100000 }")
        self.assertEqual(payload["value"], "-10000200000")

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_every_registered_progression_reducer_accepts_a_composite_kax_step(self) -> None:
        functions = (
            "sin",
            "cos",
            "tan",
            "sec",
            "cosec",
            "cot",
            "versin",
            "vercos",
            "coversin",
            "covercos",
            "haversin",
            "havercos",
            "hacoversin",
            "hacovercos",
            "sinh",
            "cosh",
            "tanh",
            "sech",
            "cosech",
            "coth",
            "exp",
            "ln",
            "lg",
            "sqrt",
            "cubrt",
            "floor",
            "ceil",
            "abs",
            "conj",
            "normal_logpdf",
            "logpdf",
            "not",
        )

        for function in functions:
            with self.subTest(function=function):
                source = f"{{ @Z_(k=1)^n {function}(kax) | a=2; x=3; n=10001 }}"
                fields, raw, returncode = mars_lab.run_mars_lab_fields(
                    self.expression_binary, source, 64, "x", "evaluate"
                )
                self.assertEqual(returncode, 0, raw)
                payload = mars_lab.prepare_evaluation_fields(
                    self.expression_binary, fields, source, 64, False, wrt="x", action="evaluate"
                )
                self.assertNotIn("Σ_", payload["full_display_expression"])
                self.assertIn("a", payload["full_display_expression"])
                self.assertIn("x", payload["full_display_expression"])
                self.assertNotIn("sum(k, 1, n", payload["function"])
                self.assertIn(r"\sum_{k=1}^{n}", payload["tex"])
                self.assertIn(" = ", payload["tex"])
                self.assertIn("value", payload)

        for argument in ("kax", "akx", "axk"):
            with self.subTest(product_order=argument):
                source = f"{{ @Z_(k=1)^n sin({argument}) | a=2; x=3; n=10001 }}"
                fields, raw, returncode = mars_lab.run_mars_lab_fields(
                    self.expression_binary, source, 64, "x", "evaluate"
                )
                self.assertEqual(returncode, 0, raw)
                payload = mars_lab.prepare_evaluation_fields(
                    self.expression_binary, fields, source, 64, False, wrt="x", action="evaluate"
                )
                self.assertNotIn("Σ_", payload["full_display_expression"])
                self.assertIn("sin(ax/2·(n + 1))", payload["full_display_expression"])

        rational_floor_source = "{ @Z_(k=1)^n floor(kax) | a=0.2; x=3; n=100000 }"
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, rational_floor_source, 64, "x", "evaluate"
        )
        self.assertEqual(returncode, 0, raw)
        self.assertEqual(fields["algebraic_specialisation"], "domain-required")
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, fields, rational_floor_source, 64, False, wrt="x", action="evaluate"
        )
        self.assertIn("25ax/2", payload["full_display_expression"])
        self.assertIn("mod(n, 5)", payload["full_display_expression"])
        self.assertNotIn("Σ_", payload["full_display_expression"])
        self.assertEqual(payload["value"], "2999990000")

        nonlinear_source = "{ @Z_(k=1)^n sin(k*k*a*x) | a=2; x=3; n=10001 }"
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, nonlinear_source, 64, "x", "evaluate"
        )
        self.assertEqual(returncode, 0, raw)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, fields, nonlinear_source, 64, False, wrt="x", action="evaluate"
        )
        self.assertIn("Σ_(k=1)^n", payload["full_display_expression"])
        self.assertNotIn("value", payload)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_product_to_angle_summand_reduces_to_a_scaled_finite_progression(self) -> None:
        source = "{ @Z_(k=1)^n sin(kx)cos(kx) | n=1000000000; x=1 }"
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, source, 386, "x", "evaluate"
        )

        self.assertEqual(returncode, 0, raw)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, fields, source, 386, False, wrt="x", action="evaluate"
        )
        self.assertEqual(
            payload["full_display_expression"],
            "{ ½·sin(nx)·sin(x·(n + 1))/sin(x) | n = 1000000000; x = 1 }",
        )
        self.assertNotIn("Σ_", payload["full_display_expression"])
        self.assertNotIn("sum(k, 1, n", payload["full_display_function"])
        self.assertIn(
            r"\sum_{k=1}^{n}\sin(k\mkern-2mu x)\mkern-2mu \cos(k\mkern-2mu x) =",
            payload["display_TeX"],
        )
        self.assertTrue(payload["value"].startswith("0.324331779782083079598855793137727724891966"))

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_tangent_cosine_summand_reduces_before_finite_sum_evaluation(self) -> None:
        source = "{ @Z_(k=1)^n tan(kx)cos(kx) | x=1; n=1000000000 }"
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, source, 64, "x", "evaluate"
        )

        self.assertEqual(returncode, 0, raw)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, fields, source, 64, False, wrt="x", action="evaluate"
        )
        self.assertNotIn("Σ_", payload["full_display_expression"])
        self.assertNotIn("sum(k, 1, n", payload["full_display_function"])
        self.assertIn(r"\sum_{k=1}^{n}\tan(k\mkern-2mu x)\mkern-2mu \cos(k\mkern-2mu x) =", payload["display_TeX"])
        self.assertTrue(payload["value"].startswith("0.4212944867471369486379515293337067831"))

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_finite_sum_linearity_splits_additive_progressions(self) -> None:
        source = "{ @Z_(k=1)^n (sin(kx)+cos(kx)+tan(kx)) | x=1; n=100000 }"
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, source, 64, "x", "evaluate"
        )

        self.assertEqual(returncode, 0, raw)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, fields, source, 64, False, wrt="x", action="evaluate"
        )
        self.assertNotIn("Σ_", payload["full_display_expression"])
        self.assertNotIn("sum(k, 1, n", payload["full_display_function"])
        self.assertIn("ψq(", payload["full_display_expression"])
        self.assertIn("sin(½nx)/sin(x/2)", payload["full_display_expression"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_cosine_progression_has_a_finite_symbolic_antiderivative(self) -> None:
        source = "{ cos(x)+cos(2x)+cos(3x)+cos(4x)+...+cos(nx) | x=pi/12; n=100 }"
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, source, 3328, "x", "integral"
        )

        self.assertEqual(returncode, 0, raw)
        self.assertIn("Hn(n, cos(x) + sin(x)·i) - Hn(n, cos(x) - sin(x)·i)", fields["integral"])
        self.assertEqual(
            fields["integral_TeX"],
            r"\frac{H_{n}(\cos(x) + \sin(x)\mkern-2mu i) - "
            r"H_{n}(\cos(x) - \sin(x)\mkern-2mu i)}{2\mkern-2mu i} + C",
        )
        self.assertIn(
            "return (harmonicpoly(n, v1 + v3) - harmonicpoly(n, v1 - v3))/(2.i) + C.",
            fields["integral_function"],
        )
        self.assertIn("x = @pi/12.", fields["integral_function"])
        self.assertIn("const n = 100.", fields["integral_function"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_hyperbolic_progression_ellipses_use_geometric_series_closed_forms(self) -> None:
        cases = (
            (
                "sinh",
                "{ sinh(nx/2)·sinh(x/2·(n + 1))/sinh(x/2) | x = ?; n = ? }",
                r"\frac{\sinh(\frac{n\mkern-2mu x}{2})\mkern-2mu "
                r"\sinh(\frac{x}{2}\mkern-2mu \left(n + 1\right))}{\sinh(\frac{x}{2})}",
                0.0,
            ),
            (
                "cosh",
                "{ sinh(nx/2)·cosh(x/2·(n + 1))/sinh(x/2) | x = ?; n = ? }",
                r"\frac{\sinh(\frac{n\mkern-2mu x}{2})\mkern-2mu "
                r"\cosh(\frac{x}{2}\mkern-2mu \left(n + 1\right))}{\sinh(\frac{x}{2})}",
                4.0,
            ),
        )

        for function, expected_expression, expected_formula, expected_at_zero in cases:
            with self.subTest(function=function):
                source = f"{function}(x)+{function}(2x)+{function}(3x)+{function}(4x)+...+{function}(nx)"
                fields, raw, returncode = mars_lab.run_mars_lab_fields(
                    self.expression_binary, source, 64, "x", "evaluate"
                )
                self.assertEqual(returncode, 0, raw)
                payload = mars_lab.prepare_evaluation_fields(
                    self.expression_binary, fields, source, 64, False, wrt="x", action="evaluate"
                )
                self.assertEqual(payload["full_display_expression"], expected_expression)
                self.assertEqual(
                    payload["display_TeX"],
                    rf"\sum_{{k=1}}^{{n}}\{function}(k\mkern-2mu x) = {expected_formula}",
                )
                self.assertIn("v1 = x/2.", payload["full_display_function"])
                self.assertIn(
                    f"return sinh(n.v1).{function}((n + 1).v1)/sinh(v1).",
                    payload["full_display_function"],
                )

                bound_source = (
                    f"{{ {function}(x)+{function}(2x)+{function}(3x)+{function}(4x)+...+{function}(nx) "
                    "| x=0.25; n=4 }"
                )
                bound_fields, bound_raw, bound_returncode = mars_lab.run_mars_lab_fields(
                    self.expression_binary, bound_source, 64, "x", "evaluate"
                )
                self.assertEqual(bound_returncode, 0, bound_raw)
                bound_payload = mars_lab.prepare_evaluation_fields(
                    self.expression_binary, bound_fields, bound_source, 64, False, wrt="x", action="evaluate"
                )
                numeric_function = math.sinh if function == "sinh" else math.cosh
                expected = sum(numeric_function(k * 0.25) for k in range(1, 5))
                self.assertAlmostEqual(float(bound_payload["value"]), expected, places=15)

                zero_source = (
                    f"{{ {function}(x)+{function}(2x)+{function}(3x)+{function}(4x)+...+{function}(nx) "
                    "| x=0; n=4 }"
                )
                zero_fields, zero_raw, zero_returncode = mars_lab.run_mars_lab_fields(
                    self.expression_binary, zero_source, 64, "x", "evaluate"
                )
                self.assertEqual(zero_returncode, 0, zero_raw)
                zero_payload = mars_lab.prepare_evaluation_fields(
                    self.expression_binary, zero_fields, zero_source, 64, False, wrt="x", action="evaluate"
                )
                self.assertEqual(float(zero_payload["value"]), expected_at_zero)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_hyperbolic_progression_integral_round_trips_through_use_as_input_text(self) -> None:
        source = "{ cosh(x)+cosh(2x)+cosh(3x)+cosh(4x)+...+cosh(nx) | x=pi/12; n=100 }"
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, source, 64, "x", "integral"
        )

        self.assertEqual(returncode, 0, raw)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, fields, source, 64, False, wrt="x", action="integral"
        )
        reusable = payload["integral"].split(" = ", 1)[1]
        self.assertIn("Φ(", payload["integral"])
        round_trip_fields, round_trip_raw, round_trip_returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, reusable, 64, "x", "evaluate"
        )
        self.assertEqual(round_trip_returncode, 0, round_trip_raw)
        self.assertIn("Φ(", round_trip_fields["expression"])
        self.assertIn("lerchphi(", round_trip_fields["function"])
        self.assertIn("const C = ?.", round_trip_fields["function"])

        derivative_source = "{ @Z_k=1^n sinh(kx)/k + C | x = pi/12; n = 100; C = ? }"
        derivative_fields, derivative_raw, derivative_returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, derivative_source, 64, "x", "derivative"
        )
        self.assertEqual(derivative_returncode, 0, derivative_raw)
        self.assertIn("sinh(nx/2)·cosh(x/2·(n + 1))/sinh(x/2)", derivative_fields["derivative"])
        self.assertIn("return sinh(n.v1).cosh((n + 1).v1)/sinh(v1).", derivative_fields["derivative_function"])
        self.assertIn(
            r"\sum_{k=1}^{n}\cosh(k\mkern-2mu x) = "
            r"\frac{\sinh(\frac{n\mkern-2mu x}{2})\mkern-2mu "
            r"\cosh(\frac{x}{2}\mkern-2mu \left(n + 1\right))}{\sinh(\frac{x}{2})}",
            derivative_fields["derivative_TeX"],
        )
        self.assertNotIn("0x", derivative_fields["derivative"])
        self.assertNotIn("0\\mkern", derivative_fields["derivative_TeX"])

        reusable_derivative = "{ @Z_k=1^n cosh(kx) | x = pi/12; n = 100 }"
        reused_fields, reused_raw, reused_returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, reusable_derivative, 64, "x", "evaluate"
        )
        self.assertEqual(reused_returncode, 0, reused_raw)
        reused_payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, reused_fields, reusable_derivative, 64, False, wrt="x", action="evaluate"
        )
        self.assertEqual(reused_payload["display_TeX"], derivative_fields["derivative_TeX"])
        self.assertIn("sinh(nx/2)·cosh(x/2·(n + 1))/sinh(x/2)", reused_payload["full_display_expression"])
        self.assertIn("return sinh(n.v1).cosh((n + 1).v1)/sinh(v1).", reused_payload["full_display_function"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_lerch_sum_round_trip_keeps_rendered_TeX_separate_from_expression_style(self) -> None:
        source = "@Z_k=1^n sinh(kx)/k"
        formula = (
            "½·Li1(exp(x)) - ½·Li1(exp(-x)) + "
            "(½·exp(-x)^(n + 1)·Φ(exp(-x), 1, n + 1) - ½·exp(x)^(n + 1)·Φ(exp(x), 1, n + 1))"
        )
        function = (
            "expression expr(x, const n) {\n"
            "    const c1 = n + 1.\n"
            "\n"
            "    v1 = exp(x).\n"
            "    v2 = 1/v1.\n"
            "\n"
            "    return li1(v1)/2 - li1(v2)/2 + "
            "v2^c1.lerchphi(v2, 1, c1)/2 - v1^c1.lerchphi(v1, 1, c1)/2.\n"
            "}\n"
            "\n"
            "x = ?.\n"
            "const n = ?.\n"
            "output(expr(x, n))."
        )
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, source, 64, "x", "evaluate"
        )

        self.assertEqual(returncode, 0, raw)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, fields, source, 64, False, wrt="x", action="evaluate"
        )
        self.assertIn(r"\Phi\left(", payload["display_TeX"])
        self.assertIn(r"\frac{", payload["display_TeX"])
        self.assertIn(r"\left(e^{-x}\right)^{n + 1}", payload["display_TeX"])
        self.assertNotIn(r"\sum", payload["display_TeX"])
        self.assertNotIn("| x =", payload["display_TeX"])
        self.assertNotIn("{ ", payload["display_TeX"])
        self.assertTrue(payload.get("svg"), payload.get("render_error"))
        self.assertEqual(payload["full_display_expression"], f"{{ {formula} | x = ?; n = ? }}")
        self.assertNotIn("Σ_", payload["full_display_expression"])
        self.assertEqual(fields["function"], function)
        self.assertEqual(payload["full_display_function"], f"` Σ_k=1^n sinh(kx)/k `\n{function}")
        self.assertNotIn("return sum(", payload["full_display_function"])
        self.assertEqual(payload["editor_expression"], formula)
        reused_fields, reused_raw, reused_returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, str(payload["editor_expression"]), 64, "x", "evaluate"
        )
        self.assertEqual(reused_returncode, 0, reused_raw)
        reused_bindings = mars_lab.mars_binding_values(reused_fields.get("bindings"))
        self.assertEqual([binding["name"] for binding in reused_bindings], ["x", "n"])

        bound_source = "{ @Z_k=1^n sinh(kx)/k | x = pi/6; n = 10 }"
        bound_fields, bound_raw, bound_returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, bound_source, 64, "x", "evaluate"
        )
        self.assertEqual(bound_returncode, 0, bound_raw)
        bound_payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, bound_fields, bound_source, 64, False, wrt="x", action="evaluate"
        )
        self.assertTrue(str(bound_payload.get("value", "")).startswith("28.715828439312067"))
        self.assertNotIn("i", str(bound_payload.get("value", "")))
        self.assertNotIn(r"\begin{aligned}", bound_payload["display_TeX"])

        bound_editor_expression = f'{{ {bound_payload["editor_expression"]} | x = pi/6; n = 10 }}'
        bound_reused_fields, bound_reused_raw, bound_reused_returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, bound_editor_expression, 64, "x", "evaluate"
        )
        self.assertEqual(bound_reused_returncode, 0, bound_reused_raw)
        bound_reused_payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, bound_reused_fields, bound_editor_expression, 64, False,
            wrt="x", action="evaluate"
        )
        self.assertTrue(str(bound_reused_payload.get("value", "")).startswith("28.715828439312067"))
        self.assertNotIn("i", str(bound_reused_payload.get("value", "")))
        self.assertNotIn(r"\begin{aligned}", bound_reused_payload["display_TeX"])
        bound_reused_function = str(bound_reused_payload["full_display_function"])
        self.assertTrue(bound_reused_function.startswith("` ½·Li1(exp(x))"))
        self.assertIn(")) `\nexpression expr", bound_reused_function)
        self.assertNotIn("`` ½·Li1", bound_reused_function)
        self.assertIn("v1 = exp(x).", bound_reused_function)
        self.assertIn("v2 = 1/v1.", bound_reused_function)
        self.assertNotIn("v2 = exp(-x).", bound_reused_function)
        self.assertIn(
            "return li1(v1)/2 - li1(v2)/2 + "
            "v2^c1.lerchphi(v2, 1, c1)/2 - v1^c1.lerchphi(v1, 1, c1)/2.",
            bound_reused_function,
        )

        derivative_fields, derivative_raw, derivative_returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, bound_editor_expression, 64, "x", "derivative"
        )
        self.assertEqual(derivative_returncode, 0, derivative_raw)
        self.assertIn("sinh(nx/2)·cosh(x/2·(n + 1))/sinh(x/2)", derivative_fields["derivative"])
        self.assertIn(r"\sum_{k=1}^{n}\cosh(k\mkern-2mu x) = ", derivative_fields["derivative_TeX"])
        self.assertNotIn("Li1", derivative_fields["derivative"])
        self.assertNotIn("Φ", derivative_fields["derivative"])
        self.assertNotIn(r"\Phi", derivative_fields["derivative_TeX"])
        self.assertTrue(str(derivative_fields.get("derivative_value", "")))

        large_source = "{ @Z_k=1^n sinh(kx)/k | x = pi/6; n = 100000000 }"
        large_fields, large_raw, large_returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, large_source, 100, "x", "evaluate"
        )
        self.assertEqual(large_returncode, 0, large_raw)
        large_payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, large_fields, large_source, 100, False, wrt="x", action="evaluate"
        )
        self.assertIn("Φ(", large_payload["full_display_expression"])
        self.assertNotIn("Σ_", large_payload["full_display_expression"])
        self.assertNotIn("return sum(", large_payload["full_display_function"])
        self.assertTrue(str(large_payload.get("value", "")).endswith("E+22739597"))

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_weighted_cosh_sum_displays_and_reuses_its_lerch_formula(self) -> None:
        # README example: a large weighted cosh sum exposes the bounded-work formula used for its value.
        source = "{ @Z_k=1^n cosh(kx)/k | x = 2; n = 100000 }"
        formula = (
            "½·Li1(exp(x)) + ½·Li1(exp(-x)) - ½·exp(-x)^(n + 1)·Φ(exp(-x), 1, n + 1) - "
            "½·exp(x)^(n + 1)·Φ(exp(x), 1, n + 1)"
        )
        function = (
            "expression expr(x, const n) {\n"
            "    const c1 = n + 1.\n"
            "\n"
            "    v1 = exp(x).\n"
            "    v2 = 1/v1.\n"
            "\n"
            "    return li1(v1)/2 + li1(v2)/2 - v2^c1.lerchphi(v2, 1, c1)/2 - "
            "v1^c1.lerchphi(v1, 1, c1)/2.\n"
            "}\n"
            "\n"
            "x = 2.\n"
            "const n = 100000.\n"
            "output(expr(x, n))."
        )
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, source, 64, "x", "evaluate"
        )

        self.assertEqual(returncode, 0, raw)
        self.assertEqual(fields["expression"], f"{{ {formula} | x = 2; n = 100000 }}")
        self.assertEqual(fields["unbound"], formula)
        self.assertIn(r"\Phi\left(e^{x}, 1, n + 1\right)", fields["tex"])
        self.assertNotIn(r"\sum", fields["tex"])
        self.assertEqual(fields["function"], function)
        self.assertNotIn("return sum(", fields["function"])
        self.assertTrue(str(fields.get("value", "")).endswith("E+86853"))

        sigma_source = "{ Σ_(k=1)^n cosh(kx)/k | x = 2; n = 100000 }"
        sigma_fields, sigma_raw, sigma_returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, sigma_source, 64, "x", "evaluate"
        )
        self.assertEqual(sigma_returncode, 0, sigma_raw)
        self.assertEqual(sigma_fields["expression"], fields["expression"])
        self.assertEqual(sigma_fields["unbound"], formula)
        self.assertEqual(sigma_fields["function"], function)
        self.assertNotIn(r"\sum", sigma_fields["tex"])
        self.assertNotIn("return sum(", sigma_fields["function"])
        self.assertEqual(sigma_fields["value"], fields["value"])

        round_trip_source = f"{{ {formula} | x = 2; n = 100000 }}"
        round_trip_fields, round_trip_raw, round_trip_returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, round_trip_source, 64, "x", "evaluate"
        )
        self.assertEqual(round_trip_returncode, 0, round_trip_raw)
        self.assertEqual(round_trip_fields["unbound"], formula)
        self.assertEqual(round_trip_fields["value"], fields["value"])

        derivative_fields, derivative_raw, derivative_returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, round_trip_source, 64, "x", "derivative"
        )
        self.assertEqual(derivative_returncode, 0, derivative_raw)
        self.assertIn("sinh(nx/2)·sinh(x/2·(n + 1))/sinh(x/2)", derivative_fields["derivative"])
        self.assertIn(r"\sum_{k=1}^{n}\sinh(k\mkern-2mu x) = ", derivative_fields["derivative_TeX"])

        zero_fields, zero_raw, zero_returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, "{ @Z_k=1^n cosh(kx)/k | x = 0; n = 100000 }", 64, "x", "evaluate"
        )
        self.assertEqual(zero_returncode, 0, zero_raw)
        self.assertEqual(zero_fields["unbound"], formula)
        self.assertTrue(str(zero_fields.get("value", "")).startswith("12.090146129863427"))

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_negated_weighted_cosine_sum_displays_its_formula_implementation_and_value(self) -> None:
        # README example: an outer minus does not hide the weighted circular sum's native formula or value.
        source = "{ -Σ_(k=1)^n cos(kx)/k | x = 2; n = 100000 }"
        formula = (
            "-½·(Li1(exp(ix)) - exp(ix)^(n + 1)·Φ(exp(ix), 1, n + 1) + "
            "(Li1(exp(-ix)) - exp(-ix)^(n + 1)·Φ(exp(-ix), 1, n + 1)))"
        )
        function = (
            "expression expr(x, const n) {\n"
            "    const c1 = n + 1.\n"
            "\n"
            "    v1 = i.x.\n"
            "    v2 = exp(v1).\n"
            "    v3 = 1/v2.\n"
            "\n"
            "    return -1/2.(li1(v2) - v2^c1.lerchphi(v2, 1, c1) + "
            "li1(v3) - v3^c1.lerchphi(v3, 1, c1)).\n"
            "}\n"
            "\n"
            "x = 2.\n"
            "const n = 100000.\n"
            "output(expr(x, n))."
        )
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, source, 64, "x", "evaluate"
        )

        self.assertEqual(returncode, 0, raw)
        self.assertEqual(fields["expression"], f"{{ {formula} | x = 2; n = 100000 }}")
        self.assertEqual(fields["unbound"], formula)
        self.assertEqual(fields["function"], function)
        self.assertIn(r"\Phi\left(e^{i\mkern-2mu x}, 1, n + 1\right)", fields["tex"])
        self.assertNotIn(r"\sum", fields["tex"])
        self.assertNotIn("return -sum(", fields["function"])
        self.assertAlmostEqual(float(fields["value"]), 0.5205386764995077, places=15)

        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, fields, source, 64, False, wrt="x", action="evaluate"
        )
        self.assertEqual(payload["full_display_expression"], fields["expression"])
        self.assertEqual(payload["full_display_function"], f"` {source.split(' | ', 1)[0][2:]} `\n{function}")
        self.assertEqual(payload["value"], fields["value"])

        round_trip_source = f"{{ {formula} | x = 2; n = 100000 }}"
        round_trip_fields, round_trip_raw, round_trip_returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, round_trip_source, 64, "x", "evaluate"
        )
        self.assertEqual(round_trip_returncode, 0, round_trip_raw)
        self.assertEqual(round_trip_fields["expression"], fields["expression"])
        self.assertEqual(round_trip_fields["function"], function)
        self.assertEqual(round_trip_fields["value"], fields["value"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_negative_symbolic_power_series_uses_inverse_power_algebra_and_calculus(self) -> None:
        source = "1+2^-p+3^-p+4^-p+...+n^-p | p=2; n=10"
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, source, 64, "p", "evaluate"
        )

        self.assertEqual(returncode, 0, raw)
        self.assertEqual(fields["unbound"], "ζ(p) - ζ(p, n + 1)")
        self.assertIn(r"\sum_{k=1}^{n}\frac{1}{k^{p}}", fields["derivation_TeX"])
        self.assertEqual(fields["value"], "1.549767731166540690350214159737969261778785588309397833207357017")

        derivative, derivative_raw, derivative_returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, source, 64, "p", "derivative"
        )
        self.assertEqual(derivative_returncode, 0, derivative_raw)
        self.assertEqual(derivative["derivative"], "d/dp = { ζ'(p) - ζ'(p, n + 1) | p = 2; n = 10 }")
        self.assertNotEqual(derivative["derivative_value"], "0")

        integral, integral_raw, integral_returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, source, 64, "p", "integral"
        )
        self.assertEqual(integral_returncode, 0, integral_raw)
        self.assertIn("p - Σ_(k=2)^n k^(-p)/ln(k) + C", integral["integral"])
        self.assertEqual(integral["integral_TeX"], r"p - \sum_{k=2}^{n}\frac{k^{-p}}{\ln(k)} + C")
        self.assertIn("return p - sum(k, 2, n, k^(-p)/ln(k)) + C.", integral["integral_function"])
        self.assertEqual(integral["integral_value"], "NAN")

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_simplified_identity_has_a_value_without_an_input_binding(self) -> None:
        expression = "sin(x)^2 + cos(x)^2"
        fields, _, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary,
            expression,
            32,
            "x",
            "evaluate",
        )

        self.assertEqual(returncode, 0)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            fields,
            expression,
            32,
            False,
            action="evaluate",
        )

        self.assertEqual(payload["full_display_TeX"], "1")
        self.assertEqual(payload["full_display_expression"], "1")
        self.assertIn("expression expr()", payload["full_display_function"])
        self.assertEqual(payload["binding_values"], [])
        self.assertEqual(payload["value"], "1")

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_tangent_addition_identity_is_shared_by_all_algebraic_cards(self) -> None:
        expression = "(tan(cx) + tan(pi*y))/(1 - tan(cx)tan(pi*y))"
        fields, _, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary,
            expression,
            32,
            "x",
            "evaluate",
        )

        self.assertEqual(returncode, 0)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            fields,
            expression,
            32,
            False,
            action="evaluate",
        )

        self.assertEqual(payload["full_display_TeX"], r"\tan(c\mkern-2mu x + \pi\mkern-2mu y)")
        self.assertEqual(payload["full_display_expression"], "{ tan(cx + πy) | x = ?, y = ?; c = ? }")
        self.assertIn("expression expr(x, y, const c)", payload["full_display_function"])
        self.assertIn("return tan(c.x + @pi.y).", payload["full_display_function"])
        self.assertIn("\nconst c = ?.\n", payload["full_display_function"])
        self.assertNotIn("value", payload)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_plain_theta_uses_the_expression_greek_alias_table(self) -> None:
        expression = "cos(theta) + i*sin(theta)"
        fields, _, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary,
            expression,
            32,
            "theta",
            "evaluate",
        )

        self.assertEqual(returncode, 0)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            fields,
            expression,
            32,
            False,
            wrt="theta",
            action="evaluate",
        )

        self.assertEqual(payload["full_display_TeX"], r"\cos \theta + \sin \theta\mkern-2mu i")
        self.assertEqual(payload["full_display_expression"], "{ cos(θ) + sin(θ)·i | θ = ? }")
        self.assertEqual([item["name"] for item in payload["binding_values"]], ["θ"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_logarithm_of_euler_identity_simplifies_in_all_algebraic_cards(self) -> None:
        expression = "{ ln(cos(theta) + isin(theta)) | θ = pi }"
        fields, _, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary,
            expression,
            32,
            "x",
            "evaluate",
        )

        self.assertEqual(returncode, 0)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            fields,
            expression,
            32,
            False,
            action="evaluate",
        )

        self.assertEqual(payload["full_display_TeX"], r"0 + \theta\mkern-2mu i")
        self.assertEqual(payload["full_display_expression"], "{ 0 + θi | θ = π }")
        self.assertIn("return 0 + θ.i.", payload["full_display_function"])
        self.assertIn("θ = @pi", payload["full_display_function"])
        self.assertIn("value", payload)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_symbolic_complex_square_root_uses_cartesian_form_in_all_algebraic_cards(self) -> None:
        expression = "sqrt(x + iy)"
        fields, _, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary,
            expression,
            32,
            "x",
            "evaluate",
        )

        self.assertEqual(returncode, 0)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            fields,
            expression,
            32,
            False,
            action="evaluate",
        )

        self.assertIn(r"\sqrt{x^{2} + y^{2}}", payload["full_display_TeX"])
        self.assertIn(r"\frac{y}{\left|y\right|}", payload["full_display_TeX"])
        self.assertNotIn(r"\begin{aligned}", payload["full_display_TeX"])
        self.assertNotIn(r"\\", payload["full_display_TeX"])
        self.assertIn("·i |", payload["full_display_expression"])
        self.assertIn("abs(y)", payload["full_display_function"])
        self.assertNotIn("value", payload)

        bound_expression = "{ sqrt(x + iy) | x = 3, y = 4 }"
        bound_fields, _, bound_returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary,
            bound_expression,
            32,
            "x",
            "evaluate",
        )
        self.assertEqual(bound_returncode, 0)
        bound_payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            bound_fields,
            bound_expression,
            32,
            False,
            action="evaluate",
        )

        self.assertIn(r"\sqrt{x^{2} + y^{2}}", bound_payload["full_display_TeX"])
        self.assertNotIn("3", bound_payload["full_display_TeX"])
        self.assertEqual(bound_payload["value"], "2 + i")

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_symbolic_complex_square_root_derivative_uses_cartesian_surd_form(self) -> None:
        expression = "(x + iy)^(1/2)"
        fields, _, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary,
            expression,
            32,
            "x",
            "derivative",
        )

        self.assertEqual(returncode, 0)
        self.assertIn(r"\begin{aligned}[t]", fields["derivative_TeX"])
        self.assertIn(r"\\[0.65em]", fields["derivative_TeX"])
        self.assertIn(r"\sqrt{x^{2} + y^{2}}", fields["derivative_TeX"])
        self.assertIn(r"\left|y\right|", fields["derivative_TeX"])
        self.assertIn(r"}{y}", fields["derivative_TeX"])
        self.assertIn(r"\mkern-2mu i", fields["derivative_TeX"])
        self.assertNotIn(r"\frac{1}{\sqrt{x + i\mkern-2mu y}}", fields["derivative_TeX"])
        self.assertIn("√(x² + y²)", fields["derivative"])
        self.assertIn("·i", fields["derivative"])
        self.assertIn("(-1)^k", fields["derivative"])
        self.assertIn("expression roots(x, y, array const k)", fields["derivative_function"])
        self.assertIn("abs(y)", fields["derivative_function"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_symbolic_complex_square_root_y_derivative_uses_both_cartesian_branches(self) -> None:
        expression = "(x + iy)^(1/2)"
        fields, _, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary,
            expression,
            32,
            "y",
            "derivative",
        )

        self.assertEqual(returncode, 0)
        self.assertIn("(-1)^k", fields["derivative"])
        self.assertIn("y/|y|·√(½·(√(x² + y²) - x))", fields["derivative"])
        self.assertIn("√(½·(√(x² + y²) + x))·i", fields["derivative"])
        self.assertIn(r"\begin{aligned}[t]", fields["derivative_TeX"])
        self.assertIn(r"\frac{y}{\left|y\right|}", fields["derivative_TeX"])
        self.assertIn(r"\mkern-2mu i", fields["derivative_TeX"])
        self.assertIn(r"\\[0.65em]", fields["derivative_TeX"])
        self.assertIn("array const k)", fields["derivative_function"])

        bound_expression = "{ (x + iy)^(1/2) | x = 3, y = 4 }"
        bound_fields, _, bound_returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary,
            bound_expression,
            32,
            "y",
            "derivative",
        )
        self.assertEqual(bound_returncode, 0)
        self.assertIn(r"&0.1 + 0.2i\\[0.65em]&-0.1 - 0.2i", bound_fields["derivative_TeX"])
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            bound_fields,
            bound_expression,
            32,
            False,
            wrt="y",
            action="derivative",
        )
        self.assertEqual(payload["derivative_value"], "0.1 + 0.2i\n-0.1 - 0.2i")
        self.assertIn(r"\frac{y}{\left|y\right|}", payload["derivative_TeX"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_sqrt_derivative_remains_principal_and_single_valued(self) -> None:
        expression = "sqrt(x + iy)"
        fields, _, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary,
            expression,
            32,
            "y",
            "derivative",
        )

        self.assertEqual(returncode, 0)
        self.assertNotIn("(-1)^k", fields["derivative"])
        self.assertNotIn("derivative_values", fields)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_exact_complex_square_root_uses_cartesian_surds_in_algebraic_cards(self) -> None:
        expression = "sqrt(2 + 3i)"
        fields, _, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary,
            expression,
            32,
            "x",
            "evaluate",
        )

        self.assertEqual(returncode, 0)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            fields,
            expression,
            32,
            False,
            action="evaluate",
        )

        self.assertEqual(
            payload["full_display_TeX"],
            r"\sqrt{\frac{1}{2}\mkern-2mu \left(\sqrt{13} + 2\right)} + "
            r"\sqrt{\frac{1}{2}\mkern-2mu \left(\sqrt{13} - 2\right)}\mkern-2mu i",
        )
        self.assertEqual(
            payload["full_display_expression"],
            "√(½·(√(13) + 2)) + √(½·(√(13) - 2))·i",
        )
        function = payload["full_display_function"]
        self.assertIn("const c1 = sqrt(13).", function)
        self.assertIn(
            "return sqrt((c1 + 2)/2) + sqrt((c1 - 2)/2).i.",
            function,
        )
        self.assertIn(" + ", payload["value"])
        self.assertTrue(payload["value"].endswith("i"))

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_unit_complex_square_root_keeps_conjugate_surds_together(self) -> None:
        expression = "sqrt(1 + i)"
        fields, _, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary,
            expression,
            32,
            "x",
            "evaluate",
        )

        self.assertEqual(returncode, 0)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            fields,
            expression,
            32,
            False,
            action="evaluate",
        )

        self.assertEqual(
            payload["full_display_TeX"],
            r"\sqrt{\frac{1}{2}\mkern-2mu \left(\sqrt{2} + 1\right)} + "
            r"\sqrt{\frac{1}{2}\mkern-2mu \left(\sqrt{2} - 1\right)}\mkern-2mu i",
        )
        self.assertEqual(
            payload["full_display_expression"],
            "√(½·(√(2) + 1)) + √(½·(√(2) - 1))·i",
        )
        function = payload["full_display_function"]
        self.assertIn("const c1 = sqrt(2).", function)
        self.assertIn(
            "return sqrt((c1 + 1)/2) + sqrt((c1 - 1)/2).i.",
            function,
        )
        self.assertTrue(payload["value"].endswith("i"))

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_unit_complex_cube_root_uses_factored_cartesian_surds(self) -> None:
        expression = "cubrt(1 + i)"
        fields, _, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary,
            expression,
            32,
            "x",
            "evaluate",
        )

        self.assertEqual(returncode, 0)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            fields,
            expression,
            32,
            False,
            action="evaluate",
        )

        self.assertEqual(
            payload["full_display_TeX"],
            r"\frac{1}{2\mkern-2mu \sqrt[3]{2}}\mkern-2mu "
            r"\left(\sqrt{3} + 1 + \left(\sqrt{3} - 1\right)\mkern-2mu i\right)",
        )
        self.assertEqual(
            payload["full_display_expression"],
            "1/(2·cubrt(2))·(√(3) + 1 + (√(3) - 1)·i)",
        )
        function = payload["full_display_function"]
        self.assertIn("const c1 = sqrt(3).", function)
        self.assertIn(
            "return 1/(2.cubrt(2)).(c1 + 1 + (c1 - 1).i).",
            function,
        )
        self.assertEqual(
            payload["value"],
            "1.0842150814913511818796660082611 + 0.29051455550725144450381318862493i",
        )

        root_expression = "root(1 + i, 3)"
        root_fields, _, root_returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary,
            root_expression,
            32,
            "x",
            "evaluate",
        )
        self.assertEqual(root_returncode, 0)
        root_payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            root_fields,
            root_expression,
            32,
            False,
            action="evaluate",
        )
        for field in ("full_display_TeX", "full_display_expression", "value"):
            self.assertEqual(root_payload[field], payload[field])
        self.assertEqual(
            root_payload["full_display_function"].splitlines()[1:],
            payload["full_display_function"].splitlines()[1:],
        )

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_unit_complex_fourth_root_uses_factored_cartesian_surds(self) -> None:
        expression = "root(1 + i, 4)"
        fields, _, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary,
            expression,
            32,
            "x",
            "evaluate",
        )

        self.assertEqual(returncode, 0)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            fields,
            expression,
            32,
            False,
            action="evaluate",
        )

        self.assertEqual(
            payload["full_display_TeX"],
            r"\frac{1}{\sqrt{2}}\mkern-2mu "
            r"\left(\sqrt{\sqrt[4]{2} + \sqrt{\frac{\sqrt{2} + 1}{2}}} + "
            r"\sqrt{\sqrt[4]{2} - \sqrt{\frac{\sqrt{2} + 1}{2}}}\mkern-2mu i\right)",
        )
        self.assertEqual(
            payload["full_display_expression"],
            "1/√(2)·(√(root(2, 4) + √(1/2·(√(2) + 1))) + "
            "√(root(2, 4) - √(1/2·(√(2) + 1)))·i)",
        )
        function = payload["full_display_function"]
        self.assertIn("const c1 = sqrt(2).", function)
        self.assertIn("const c2 = root(2, 4).", function)
        self.assertIn("const c3 = c1 + 1.", function)
        self.assertIn("const c4 = c3/2.", function)
        self.assertIn("const c5 = sqrt(c4).", function)
        self.assertIn(
            "return 1/c1.(sqrt(c2 + c5) + sqrt(c2 - c5).i).",
            function,
        )
        self.assertEqual(
            payload["value"],
            "1.0695539323639858023756790408254 + 0.2127475047267430357507130792184i",
        )

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_explicit_unit_complex_fourth_power_displays_all_roots_as_cartesian_surds(self) -> None:
        expression = "(1 + i)^(1/4)"
        fields, _, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary,
            expression,
            32,
            "x",
            "evaluate",
        )

        self.assertEqual(returncode, 0)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            fields,
            expression,
            32,
            False,
            action="evaluate",
        )

        scale = r"\frac{1}{\sqrt{2}}\mkern-2mu "
        real = r"\sqrt{\sqrt[4]{2} + \sqrt{\frac{\sqrt{2} + 1}{2}}}"
        imaginary = r"\sqrt{\sqrt[4]{2} - \sqrt{\frac{\sqrt{2} + 1}{2}}}"
        negative = r"\mathord{-}\mkern-2mu "
        self.assertEqual(
            payload["full_display_TeX"],
            r"\begin{aligned}[t]&"
            f"{scale}\\left({real} + i\\mkern-2mu {imaginary}\\right)"
            r"\\[0.65em]&"
            f"{scale}\\left({negative}{imaginary} + i\\mkern-2mu {real}\\right)"
            r"\\[0.65em]&"
            f"{scale}\\left({negative}{real} - i\\mkern-2mu {imaginary}\\right)"
            r"\\[0.65em]&"
            f"{scale}\\left({imaginary} - i\\mkern-2mu {real}\\right)"
            r"\end{aligned}",
        )
        self.assertNotIn("e^{", payload["full_display_TeX"])
        self.assertIn("    const c1 = sqrt(2).", payload["full_display_function"])
        self.assertIn("    const c2 = root(2, 4).", payload["full_display_function"])
        self.assertIn("    const c3 = c1 + 1.", payload["full_display_function"])
        self.assertIn("    const c4 = c3/2.", payload["full_display_function"])
        self.assertIn("    const c5 = sqrt(c4).", payload["full_display_function"])
        self.assertIn(
            "    return 1/c1.exp(@pi.i.k/2)."
            "(sqrt(c2 + c5) + sqrt(c2 - c5).i).",
            payload["full_display_function"],
        )
        self.assertTrue(all(len(line) <= 130 for line in payload["full_display_function"].splitlines()))
        self.assertEqual(
            payload["value"],
            "1.0695539323639858023756790408254 + 0.2127475047267430357507130792184i\n"
            "-0.2127475047267430357507130792184 + 1.0695539323639858023756790408254i\n"
            "-1.0695539323639858023756790408254 - 0.2127475047267430357507130792184i\n"
            "0.2127475047267430357507130792184 - 1.0695539323639858023756790408254i",
        )

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_explicit_unit_complex_half_power_displays_both_beautified_roots(self) -> None:
        expression = "(1 + i)^(1/2)"
        fields, _, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary,
            expression,
            32,
            "x",
            "evaluate",
        )

        self.assertEqual(returncode, 0)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            fields,
            expression,
            32,
            False,
            action="evaluate",
        )

        principal_expression = "√(½·(√(2) + 1)) + √(½·(√(2) - 1))·i"
        self.assertEqual(
            payload["full_display_expression"],
            f"{{ ({principal_expression})·(-1)^k | ; k = [0, 1] }}",
        )
        real = r"\sqrt{\frac{1}{2}\mkern-2mu \left(\sqrt{2} + 1\right)}"
        imaginary = r"\sqrt{\frac{1}{2}\mkern-2mu \left(\sqrt{2} - 1\right)}"
        self.assertEqual(
            payload["full_display_TeX"],
            r"\begin{aligned}[t]&"
            f"{real} + i\\mkern-2mu {imaginary}"
            r"\\[0.65em]&\mathord{-}\mkern-2mu "
            f"{real} - i\\mkern-2mu {imaginary}"
            r"\end{aligned}",
        )
        self.assertIn("\nexpression roots(array const k)", payload["full_display_function"])
        self.assertIn("(-1)^k", payload["full_display_function"])
        self.assertIn("const k = [0, 1].", payload["full_display_function"])
        self.assertEqual(
            payload["value"],
            "1.0986841134678099660398011952407 + 0.45508986056222734130435775782247i\n"
            "-1.0986841134678099660398011952407 - 0.45508986056222734130435775782247i",
        )

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_exact_complex_square_root_reduces_perfect_square_components(self) -> None:
        expression = "sqrt(3 + 4i)"
        fields, _, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary,
            expression,
            32,
            "x",
            "evaluate",
        )

        self.assertEqual(returncode, 0)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            fields,
            expression,
            32,
            False,
            action="evaluate",
        )

        self.assertEqual(payload["full_display_TeX"], "2 + i")
        self.assertEqual(payload["full_display_expression"], "2 + i")
        self.assertIn("return 2 + i.", payload["full_display_function"])
        self.assertEqual(payload["value"], "2 + i")

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_pure_imaginary_square_root_reduces_to_cartesian_components(self) -> None:
        expression = "sqrt(2i)"
        fields, _, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary,
            expression,
            32,
            "x",
            "evaluate",
        )

        self.assertEqual(returncode, 0)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            fields,
            expression,
            32,
            False,
            action="evaluate",
        )

        self.assertEqual(payload["full_display_TeX"], "1 + i")
        self.assertEqual(payload["full_display_expression"], "1 + i")
        self.assertIn("return 1 + i.", payload["full_display_function"])
        self.assertEqual(payload["value"], "1 + i")

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_exact_complex_fifth_power_displays_all_five_roots(self) -> None:
        expression = "(-4 + 4i)^(1/5)"
        fields, raw, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary,
            expression,
            32,
            "x",
            "evaluate",
        )

        self.assertEqual(returncode, 0, raw)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            fields,
            expression,
            32,
            False,
            action="evaluate",
        )

        self.assertTrue(payload.get("svg"), payload.get("render_error"))
        self.assertTrue(payload["full_display_TeX"].startswith(r"\begin{aligned}[t]&1 - i"))
        self.assertNotIn(r"\left\{", payload["full_display_TeX"])
        self.assertEqual(payload["full_display_TeX"].count(r"e^{\frac{"), 4)
        self.assertEqual(payload["full_display_expression"].count("exp("), 1)
        self.assertIn("| ; k = [0, 1, 2, 3, 4]", payload["full_display_expression"])
        self.assertIn("\nexpression roots(array const k)", payload["full_display_function"])
        self.assertIn("const k = [0, 1, 2, 3, 4].", payload["full_display_function"])
        self.assertIn("output(roots(k)).", payload["full_display_function"])
        self.assertIn("i", payload["value"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_larger_complex_square_root_reduces_perfect_square_components(self) -> None:
        expression = "sqrt(5 + 12i)"
        fields, _, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary,
            expression,
            32,
            "x",
            "evaluate",
        )

        self.assertEqual(returncode, 0)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            fields,
            expression,
            32,
            False,
            action="evaluate",
        )

        self.assertEqual(payload["full_display_TeX"], r"3 + 2\mkern-2mu i")
        self.assertEqual(payload["full_display_expression"], "3 + 2i")
        self.assertIn("return 3 + 2.i.", payload["full_display_function"])
        self.assertEqual(payload["value"], "3 + 2i")

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_exact_complex_integer_power_folds_to_cartesian_value(self) -> None:
        expression = "(1 + i)^2"
        fields, _, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary,
            expression,
            32,
            "x",
            "evaluate",
        )

        self.assertEqual(returncode, 0)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            fields,
            expression,
            32,
            False,
            action="evaluate",
        )

        self.assertEqual(payload["full_display_TeX"], r"2\mkern-2mu i")
        self.assertEqual(payload["full_display_expression"], "2i")
        self.assertIn("return 2.i.", payload["full_display_function"])
        self.assertEqual(payload["value"], "2i")

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_exact_complex_integer_power_with_negative_real_part_folds_to_cartesian_value(self) -> None:
        expression = "(-3 + 4i)^2"
        fields, _, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary,
            expression,
            32,
            "x",
            "evaluate",
        )

        self.assertEqual(returncode, 0)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            fields,
            expression,
            32,
            False,
            action="evaluate",
        )

        self.assertEqual(payload["full_display_TeX"], r"-7 - 24\mkern-2mu i")
        self.assertEqual(payload["full_display_expression"], "-7 - 24i")
        self.assertIn("return -7 - 24.i.", payload["full_display_function"])
        self.assertEqual(payload["value"], "-7 - 24i")

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_exact_complex_cube_power_displays_all_three_roots(self) -> None:
        expression = "(-2 + 2i)^(1/3)"
        fields, _, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary,
            expression,
            32,
            "x",
            "evaluate",
        )

        self.assertEqual(returncode, 0)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            fields,
            expression,
            32,
            False,
            action="evaluate",
        )

        self.assertTrue(payload["full_display_TeX"].startswith(r"\begin{aligned}[t]&1 + i"))
        self.assertNotIn(r"\left\{", payload["full_display_TeX"])
        self.assertNotIn(r"e^{", payload["full_display_TeX"])
        self.assertIn(r"\sqrt{3}", payload["full_display_TeX"])
        self.assertEqual(payload["full_display_expression"].count("exp("), 1)
        self.assertIn("| ; k = [0, 1, 2]", payload["full_display_expression"])
        self.assertIn("\nexpression roots(array const k)", payload["full_display_function"])
        self.assertIn("const k = [0, 1, 2].", payload["full_display_function"])
        self.assertIn("output(roots(k)).", payload["full_display_function"])
        self.assertNotEqual(payload["value"], payload["full_display_expression"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_exact_complex_sixth_power_displays_every_root_as_cartesian_surds(self) -> None:
        expression = "(117 + 44i)^(1/6)"
        fields, _, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary,
            expression,
            32,
            "x",
            "evaluate",
        )

        self.assertEqual(returncode, 0)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            fields,
            expression,
            32,
            False,
            action="evaluate",
        )

        TeX = payload["full_display_TeX"]
        self.assertTrue(TeX.startswith(r"\begin{aligned}[t]&1 + 2i"))
        self.assertEqual(TeX.count(r"\\[0.65em]"), 5)
        self.assertNotIn(r"e^{", TeX)
        self.assertEqual(TeX.count(r"\sqrt{3}"), 8)
        self.assertIn(r"\frac{1}{2} - \sqrt{3}", TeX)
        self.assertIn(r"-\frac{1}{2} - \sqrt{3}", TeX)
        self.assertIn(r"&-1 - 2\mkern-2mu i", TeX)
        self.assertIn(r"-\frac{1}{2} + \sqrt{3}", TeX)
        self.assertIn(r"\frac{1}{2} + \sqrt{3}", TeX)
        self.assertEqual(payload["value"].count("\n"), 5)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_unit_complex_seventh_power_displays_every_root_in_cartesian_trigonometric_form(self) -> None:
        expression = "(1 + i)^(1/7)"
        fields, _, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary,
            expression,
            32,
            "x",
            "evaluate",
        )

        self.assertEqual(returncode, 0)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            fields,
            expression,
            32,
            False,
            action="evaluate",
        )

        tex = payload["full_display_TeX"]
        roots = tex.removeprefix(r"\begin{aligned}[t]&").removesuffix(r"\end{aligned}").split(r"\\[0.65em]&")
        self.assertEqual(len(roots), 7)
        self.assertTrue(all(r"\sqrt[14]{2}" in root for root in roots))
        self.assertTrue(all(r"\cos\left(" in root and r" + i\mkern-2mu " in root and r"\sin\left(" in root for root in roots))
        self.assertNotIn(r"e^{", tex)
        self.assertIn(r"\frac{\pi}{28}", roots[0])
        self.assertIn(r"\frac{7\pi}{4}", roots[-1])
        self.assertEqual(payload["value"].count("\n"), 6)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_named_sixth_root_displays_one_exact_cartesian_principal_value(self) -> None:
        expression = "root(117 + 44i, 6)"
        fields, _, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary,
            expression,
            32,
            "x",
            "evaluate",
        )

        self.assertEqual(returncode, 0)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            fields,
            expression,
            32,
            False,
            action="evaluate",
        )

        self.assertNotIn(r"\sqrt[6]", payload["full_display_TeX"])
        self.assertIn(r"\sqrt{3}", payload["full_display_TeX"])
        self.assertNotIn("root(", payload["full_display_expression"])
        self.assertIn("√(3)", payload["full_display_expression"])
        self.assertIn("i", payload["full_display_expression"])
        self.assertNotIn("array const k", payload["full_display_function"])
        self.assertEqual(payload["value"].count("\n"), 0)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_sqrt_is_principal_but_explicit_half_power_displays_both_roots(self) -> None:
        sqrt_expression = "sqrt(3 + 4i)"
        power_expression = "(3 + 4i)^(1/2)"

        sqrt_fields, _, sqrt_returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, sqrt_expression, 32, "x", "evaluate"
        )
        power_fields, _, power_returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, power_expression, 32, "x", "evaluate"
        )
        self.assertEqual(sqrt_returncode, 0)
        self.assertEqual(power_returncode, 0)

        sqrt_payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, sqrt_fields, sqrt_expression, 32, False, action="evaluate"
        )
        power_payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, power_fields, power_expression, 32, False, action="evaluate"
        )

        self.assertEqual(sqrt_payload["full_display_TeX"], "2 + i")
        self.assertEqual(sqrt_payload["full_display_expression"], "2 + i")
        self.assertIn("return 2 + i.", sqrt_payload["full_display_function"])
        self.assertEqual(sqrt_payload["value"], "2 + i")
        self.assertEqual(
            power_payload["full_display_TeX"],
            r"\begin{aligned}[t]&2 + i\\[0.65em]&\mathord{-}\mkern-2mu 2 - i\end{aligned}",
        )
        self.assertEqual(power_payload["full_display_expression"], "{ (2 + i)·(-1)^k | ; k = [0, 1] }")
        self.assertIn("\nexpression roots(array const k)", power_payload["full_display_function"])
        self.assertIn("(-1)^k", power_payload["full_display_function"])
        self.assertIn("const k = [0, 1].", power_payload["full_display_function"])
        self.assertIn("output(roots(k)).", power_payload["full_display_function"])
        self.assertEqual(power_payload["value"], "2 + i\n-2 - i")

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_symbolic_explicit_half_power_displays_two_symbolic_roots(self) -> None:
        expression = "(a + bi)^(1/2)"
        fields, _, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary, expression, 32, "x", "evaluate"
        )

        self.assertEqual(returncode, 0)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary, fields, expression, 32, False, action="evaluate"
        )

        self.assertIn("(-1)^k", payload["full_display_expression"])
        self.assertIn("| ; k = [0, 1], a = ?, b = ?", payload["full_display_expression"])
        self.assertIn(r"\sqrt{a^{2} + b^{2}} + a", payload["full_display_TeX"])
        self.assertIn(r"\sqrt{a^{2} + b^{2}} - a", payload["full_display_TeX"])
        self.assertIn("expression roots(array const k, const a, const b)", payload["full_display_function"])
        self.assertIn("(-1)^k", payload["full_display_function"])
        self.assertIn("const k = [0, 1].", payload["full_display_function"])
        self.assertIn("output(roots(k, a, b)).", payload["full_display_function"])
        self.assertNotIn("value", payload)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_symbolic_complex_square_expands_to_cartesian_form(self) -> None:
        expression = "(a + bi)^2"
        fields, _, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary,
            expression,
            32,
            "x",
            "evaluate",
        )

        self.assertEqual(returncode, 0)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            fields,
            expression,
            32,
            False,
            action="evaluate",
        )

        self.assertEqual(
            payload["full_display_TeX"],
            r"a^{2} - b^{2} + 2\mkern-2mu a\mkern-2mu b\mkern-2mu i",
        )
        self.assertEqual(payload["full_display_expression"], "{ a² - b² + 2abi | ; a = ?, b = ? }")
        self.assertIn("return a^2 - b^2 + 2.a.b.i.", payload["full_display_function"])
        self.assertNotIn("value", payload)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_unparenthesised_unit_fraction_displays_the_root_family(self) -> None:
        expression = "(-3 + 4i)^1/3"
        fields, _, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary,
            expression,
            32,
            "x",
            "evaluate",
        )

        self.assertEqual(returncode, 0)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            fields,
            expression,
            32,
            False,
            action="evaluate",
        )

        self.assertTrue(payload["full_display_TeX"].startswith(r"\begin{aligned}[t]&"))
        self.assertNotIn(r"\left\{", payload["full_display_TeX"])
        self.assertEqual(payload["full_display_TeX"].count(r"\\[0.65em]"), 2)
        self.assertIn("| ; k = [0, 1, 2]", payload["full_display_expression"])
        self.assertIn("expression roots(array const k)", payload["full_display_function"])
        self.assertIn("const k = [0, 1, 2].", payload["full_display_function"])
        self.assertEqual(payload["value"].count("\n"), 2)

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_squared_cartesian_root_simplifies_to_the_original_complex_expression(self) -> None:
        expression = (
            "(sqrt(2)/2*sqrt(sqrt(x^2+y^2)+x)"
            "+i*sqrt(2)/2*y*sqrt(sqrt(x^2+y^2)-x)/abs(y))^2"
        )
        fields, _, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary,
            expression,
            32,
            "x",
            "evaluate",
        )

        self.assertEqual(returncode, 0)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            fields,
            expression,
            32,
            False,
            action="evaluate",
        )

        self.assertEqual(payload["full_display_TeX"], r"x + y\mkern-2mu i")
        self.assertEqual(payload["full_display_expression"], "{ x + yi | x = ?, y = ? }")
        self.assertIn("return x + y.i.", payload["full_display_function"])

        bound_expression = "{ " + expression + " | x = 3, y = 2 }"
        bound_fields, _, bound_returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary,
            bound_expression,
            32,
            "x",
            "evaluate",
        )
        self.assertEqual(bound_returncode, 0)
        bound_payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            bound_fields,
            bound_expression,
            32,
            False,
            action="evaluate",
        )

        self.assertEqual(bound_payload["full_display_TeX"], r"x + y\mkern-2mu i")
        self.assertEqual(bound_payload["full_display_expression"], "{ x + yi | x = 3, y = 2 }")
        self.assertEqual(bound_payload["value"], "3 + 2i")

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_reciprocal_cartesian_root_integrates_after_use_as_input_round_trip(self) -> None:
        expression = "1/(√2/2·(√(√(x² + y²) + x) + iy·√(√(x² + y²) - x)/|y|))"
        fields, _, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary,
            expression,
            32,
            "x",
            "integral",
        )

        self.assertEqual(returncode, 0)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            fields,
            expression,
            32,
            False,
            action="integral",
        )

        self.assertEqual(payload["integral_TeX"], r"2\mkern-2mu \sqrt{x + y\mkern-2mu i} + C")
        self.assertEqual(payload["integral"], "∫dx = { 2·√(x + yi) + C | x = ?, y = ?; C = ? }")
        self.assertIn("return 2.(x + y.i)^1/2 + C.", payload["full_display_integral_function"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_translated_tangent_has_a_symbolic_integral(self) -> None:
        expression = "tan(x+y)"
        fields, _, returncode = mars_lab.run_mars_lab_fields(
            self.expression_binary,
            expression,
            78,
            "x",
            "integral",
        )

        self.assertEqual(returncode, 0)
        payload = mars_lab.prepare_evaluation_fields(
            self.expression_binary,
            fields,
            expression,
            78,
            False,
            action="integral",
        )

        self.assertEqual(payload["integral_TeX"], r"-\ln(\cos(x + y)) + C")
        self.assertEqual(
            payload["integral"],
            "∫dx = { -ln(cos(x + y)) + C | x = ?, y = ?; C = ? }",
        )
        self.assertIn("\nconst C = ?.\n", payload["full_display_integral_function"])

    @unittest.skipUnless(
        (ROOT / "build" / "release" / "scratch" / "mars_lab").is_file(),
        "release mars_lab helper is not built",
    )
    def test_cartesian_tanh_integrals_survive_expression_binding_round_trip(self) -> None:
        expression = "{ tanh(x+iy) | x = NAN, y = NAN }"

        for wrt in ("x", "y"):
            with self.subTest(wrt=wrt):
                fields, _, returncode = mars_lab.run_mars_lab_fields(
                    self.expression_binary,
                    expression,
                    32,
                    wrt,
                    "integral",
                )
                self.assertEqual(returncode, 0)
                payload = mars_lab.prepare_evaluation_fields(
                    self.expression_binary,
                    fields,
                    expression,
                    32,
                    False,
                    wrt=wrt,
                    action="integral",
                )

                self.assertTrue(payload["integral"].startswith(f"∫d{wrt} = {{ "))
                self.assertIn("ln(", payload["integral"])
                self.assertIn("atan2(", payload["integral"])
                self.assertIn("i", payload["integral"])
                self.assertNotIn("No integral", payload["integral"])
                if wrt == "y":
                    self.assertNotIn("/i", payload["integral"])
                    self.assertNotIn("/i", payload["full_display_integral_function"])
                    self.assertNotIn(r"}{i}", payload["integral_TeX"])
                self.assertIn("const C = ?.", payload["full_display_integral_function"])
                self.assertTrue(payload["integral"].split(" |", 1)[0].endswith(" + C"))
                self.assertTrue(payload["integral_TeX"].endswith(" + C"))
                return_line = next(
                    line.strip()
                    for line in payload["full_display_integral_function"].splitlines()
                    if line.strip().startswith("return ")
                )
                self.assertTrue(return_line.endswith(" + C."))

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
        self.assertIn("; C = ?", fields["expression"])
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
            r"\frac{1}{2}\mkern-2mu J_{-\frac{5}{4}}\left(x\right) - "
            r"\frac{1}{2}\mkern-2mu J_{\frac{3}{4}}\left(x\right)",
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

    def test_evaluate_captures_expression_binding_values_before_requesting_results(self) -> None:
        visible_commit = mars_lab.INDEX_HTML.split(
            "function commitVisibleBindingInputs() {", 1
        )[1].split(
            "\n    async function toggleBindingKind", 1
        )[0]

        self.assertIn("if (currentMode() === 'expression') {", visible_commit)
        self.assertIn("normalisedBindingInputValue(input)", visible_commit)
        self.assertIn("fullExpressionText = expressionForEditor(updated).trim();", visible_commit)
        self.assertIn("expr.dataset.fullExpression = fullExpressionText;", visible_commit)

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
        self.assertNotIn("value", fields)

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
            "return @S^x exp(cosh(t)) dt.",
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
            "return 5.x^4 - 24.x^3 - 6.x^2 + 72.x + 1.",
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
            "{ 20x³ - 72x² - 12x + 72 | x = π }",
        )
        self.assertIn(
            "return 20.x^3 - 72.x^2 - 12.x + 72.",
            payload["full_display_function"],
        )
        self.assertIn(
            "x = @pi.\noutput(expr(x)).",
            payload["full_display_function"],
        )
        self.assertEqual(payload["binding_values"][0]["value"], "π")


class DatetimeWeatherTests(unittest.TestCase):
    def test_weather_provider_is_not_contacted_without_own_account_key(self) -> None:
        with (
            mock.patch.object(mars_lab, "weather_relative_day_span", return_value=0),
            mock.patch.object(mars_lab, "weather_date_is_supported", return_value=True),
            mock.patch.object(mars_lab, "weather_api_key", return_value=""),
            mock.patch.object(mars_lab.urllib.request, "urlopen") as urlopen,
        ):
            self.assertIsNone(mars_lab.fetch_daily_weather_for_datetime("2026-08-21", 52.7077, -2.7541))

        urlopen.assert_not_called()

    def test_weather_payload_is_ready_for_thin_client_rendering(self) -> None:
        payload = mars_lab.prepare_datetime_weather_fields({
            "weather_min_c": "12°C",
            "weather_max_c": "19°C",
            "weather_humidity": "71%",
            "weather_wind": "18 km/h",
            "weather_rain_chance": "34%",
            "weather_summary": "Min 12°C, max 19°C",
            "weather_source": "WeatherAPI.com",
        })

        self.assertTrue(payload["ok"])
        self.assertTrue(payload["available"])
        self.assertEqual(payload["overview_sections"][0]["title"], "Weather")
        self.assertEqual(
            payload["overview_sections"][0]["rows"],
            [
                {"label": "Minimum", "value": "12°C"},
                {"label": "Maximum", "value": "19°C"},
                {"label": "Humidity", "value": "71%"},
                {"label": "Wind", "value": "18 km/h"},
                {"label": "Chance of rain", "value": "34%"},
                {"label": "Source", "value": "WeatherAPI.com"},
            ],
        )

    def test_weather_payload_explains_unavailable_data(self) -> None:
        payload = mars_lab.prepare_datetime_weather_fields({}, "The weather provider did not return data.")

        self.assertFalse(payload["available"])
        self.assertEqual(
            payload["overview_sections"],
            [{
                "title": "Weather",
                "open": True,
                "rows": [{"label": "Status", "value": "The weather provider did not return data."}],
            }],
        )

    def test_datetime_weather_is_requested_asynchronously_and_rejects_stale_results(self) -> None:
        html = mars_lab.INDEX_HTML

        self.assertIn("fetch('/datetime-weather'", html)
        self.assertIn("void refreshDatetimeWeather(evaluationId, {", html)
        self.assertIn("evaluationId !== datetimeEvaluationSequence", html)
        self.assertIn("const controller = new AbortController();", html)


class AlmanacLocationTests(unittest.TestCase):
    def test_totality_location_accepts_the_current_named_town_payload_only(self) -> None:
        payload = "town\tMálaga\tES\tEurope/Madrid\t36.7202\t-4.4203\t2461620.5\t123.4\t22"

        self.assertIn(
            "Málaga, Spain; 36.7202, -4.4203;",
            mars_lab.format_almanac_totality_location(payload, 0.0, "GB-ENG"),
        )
        self.assertEqual(
            mars_lab.format_almanac_totality_location("36.7202,-4.4203,2461620.5,123.4", 0.0, "GB-ENG"),
            "",
        )

    def test_eclipse_kind_and_magnitude_columns_do_not_overlap(self) -> None:
        self.assertIn('.almanac-event-table .event-kind {\n      width: 5.6rem;', mars_lab.INDEX_HTML)
        self.assertIn('class="event-kind" data-label="Kind"', mars_lab.INDEX_HTML)
        self.assertIn('class="number event-measure" data-label="Magnitude"', mars_lab.INDEX_HTML)

    def test_almanac_event_helper_receives_the_jurisdiction_database_environment(self) -> None:
        completed = subprocess.CompletedProcess(args=[], returncode=0, stdout="", stderr="")

        with (
            mock.patch.object(mars_lab, "mars_lab_object_store_runtime_env", return_value={"MARS_STORE": "store"}),
            mock.patch.object(mars_lab, "jurisdiction_db_runtime_env", return_value={"MARS_TOWNS": "towns"}),
            mock.patch.object(mars_lab.subprocess, "run", return_value=completed) as run,
        ):
            mars_lab.run_almanac_event_lab_rows({"start": "2027-08-01", "end": "2027-08-03", "kind": "solar"})

        child_env = run.call_args.kwargs["env"]
        self.assertEqual(child_env["MARS_STORE"], "store")
        self.assertEqual(child_env["MARS_TOWNS"], "towns")

    def test_almanac_result_identifies_the_selected_town(self) -> None:
        payload = mars_lab.prepare_almanac_fields({
            "date": "2027-08-02",
            "time": "08:48:57",
            "zone": "2.00",
            "latitude": "36.720200",
            "longitude": "-4.420300",
            "town": "Málaga|36.7202|-4.4203|22",
            "jurisdiction": "ES",
            "visibility": "visible",
            "events_cached": "yes",
        })

        self.assertEqual(
            payload["observer_text"],
            "Observer: Málaga, Spain, zone 2.00, latitude 36.720200, longitude -4.420300",
        )

    def test_town_search_matches_names_without_typed_accents(self) -> None:
        malaga = next(town for town in mars_lab.JURISDICTION_TOWN_OPTIONS["ES"] if town["name"] == "Málaga")

        self.assertEqual(malaga["latitude"], "36.7202")
        self.assertIn("function normaliseSelectSearchText(value)", mars_lab.INDEX_HTML)
        self.assertIn(".normalize('NFD')", mars_lab.INDEX_HTML)
        self.assertIn("const query = normaliseSelectSearchText(searchInput && searchInput.value).trim();", mars_lab.INDEX_HTML)

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

    def test_upcoming_almanac_window_excludes_finished_events_and_keeps_in_progress_events(self) -> None:
        def event(name: str, first: str, last: str) -> dict[str, str]:
            first_moment = py_datetime.datetime.fromisoformat(first).replace(tzinfo=py_datetime.timezone.utc)
            last_moment = py_datetime.datetime.fromisoformat(last).replace(tzinfo=py_datetime.timezone.utc)
            greatest = first_moment + (last_moment - first_moment) / 2
            return {
                "category": "Solar" if "solar" in name.lower() else "Lunar",
                "name": name,
                "time": greatest.strftime("%Y-%m-%d %H:%M:%S"),
                "sort_time": greatest.isoformat(),
                "jd": f"{mars_lab.almanac_datetime_jd(greatest):.9f}",
                "first_jd": f"{mars_lab.almanac_datetime_jd(first_moment):.9f}",
                "last_jd": f"{mars_lab.almanac_datetime_jd(last_moment):.9f}",
            }

        events_by_year = {
            2026: [
                event("Finished solar eclipse", "2026-08-12T18:00:00", "2026-08-12T20:00:00"),
                event("Ongoing lunar eclipse", "2026-08-18T21:30:00", "2026-08-18T22:30:00"),
                event("Future lunar eclipse", "2026-08-28T02:00:00", "2026-08-28T08:00:00"),
            ],
            2027: [
                event("Next-year solar eclipse", "2027-08-02T08:00:00", "2027-08-02T11:00:00"),
                event("Beyond-window lunar eclipse", "2027-08-19T08:00:00", "2027-08-19T11:00:00"),
            ],
        }

        with mock.patch.object(
            mars_lab,
            "generate_annual_almanac_events",
            side_effect=lambda year, *args: events_by_year.get(year, []),
        ):
            events, window = mars_lab.generate_upcoming_almanac_events(
                "2026-08-18",
                "22:17:53",
                1.0,
                "GB-ENG",
                "51.5074",
                "-0.1278",
            )

        self.assertEqual(
            [item["name"] for item in events],
            ["Ongoing lunar eclipse", "Future lunar eclipse", "Next-year solar eclipse"],
        )
        self.assertEqual(
            window,
            "2026-08-18T22:17:53+00:00|2027-08-18T22:17:53+00:00",
        )

    def test_old_annual_almanac_cache_is_replaced_by_upcoming_window(self) -> None:
        future_event = {
            "category": "Lunar",
            "name": "Future lunar eclipse",
            "kind": "partial",
            "time": "2026-08-28 05:12:49 GMT+01:00",
        }
        expected_window = "2026-08-18T22:17:53+00:00|2027-08-18T22:17:53+00:00"

        with mock.patch.object(
            mars_lab,
            "generate_upcoming_almanac_events",
            return_value=([future_event], expected_window),
        ) as generate:
            payload = mars_lab.prepare_almanac_fields({
                "date": "2026-08-18",
                "time": "22:17:53",
                "zone": "1.00",
                "latitude": "51.507400",
                "longitude": "-0.127800",
                "jurisdiction": "GB-ENG",
                "events_cached": "yes",
                "event_window": "2026-01-01|2027-01-01",
                "events": "Solar|Solar eclipse|partial|2026-08-12 19:13:16 GMT+01:00|||||||||",
            })

        generate.assert_called_once()
        self.assertEqual([event["name"] for event in payload["events"]], ["Future lunar eclipse"])
        self.assertEqual(payload["event_window"], expected_window)
        self.assertEqual(
            payload["event_title"],
            "Upcoming eclipses and inner planetary transits through 2027-08-18",
        )

    def test_upcoming_almanac_cache_replaces_annual_metadata_and_preserves_contacts(self) -> None:
        event_window = "2026-08-18T22:17:53+00:00|2027-08-18T22:17:53+00:00"
        payload = {
            "event_year": "2026",
            "event_window": event_window,
            "events": [{
                "category": "Lunar",
                "name": "Lunar eclipse",
                "kind": "partial",
                "time": "2026-08-28 05:12:49 GMT+01:00",
                "jd": "2461280.675567",
                "first_jd": "2461280.598773",
                "last_jd": "2461280.793993",
            }],
        }

        output = mars_lab.almanac_output_with_events(
            "date 2026-08-18\ntime 22:17:53\n"
            "event_year 2026\nevent_window 2026-01-01|2027-01-01\n"
            "events_cached yes\nevent Solar|Solar eclipse|partial|old\n",
            {"date": "2026-08-18", "event_year": "2026"},
            payload,
        )
        fields = mars_lab.parse_almanac_lab_output(output)
        events = mars_lab.parse_almanac_event_rows(fields["events"])

        self.assertEqual(fields["event_window_mode"], mars_lab.ALMANAC_EVENT_WINDOW_MODE)
        self.assertEqual(fields["event_window"], event_window)
        self.assertNotIn("2026-01-01|2027-01-01", output)
        self.assertEqual(events[0]["first_jd"], "2461280.598773")
        self.assertEqual(events[0]["last_jd"], "2461280.793993")

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

        # README example: named roots return one exact Cartesian principal value.
        expression, raw, returncode = mars_lab.run_mars_lab_fields(
            scratch / "mars_lab",
            "root(1+i, 4)",
            64,
            "x",
            "evaluate",
        )
        self.assertEqual(returncode, 0, raw)
        self.assertEqual(
            expression["unbound"],
            "1/√(2)·(√(root(2, 4) + √(1/2·(√(2) + 1))) + "
            "√(root(2, 4) - √(1/2·(√(2) + 1)))·i)",
        )
        self.assertEqual(
            expression["value"],
            "1.069553932363985802375679040825362637645245523613176209695385543 + "
            "0.2127475047267430357507130792183971747089746756603813495107210264i",
        )

        # README example: a symbolic inverse-power ellipsis shows its sigma and closed form before evaluation.
        expression, raw, returncode = mars_lab.run_mars_lab_fields(
            scratch / "mars_lab",
            "1+1/2^p+1/3^p+...+1/n^p | p=2.5; n=100",
            64,
            "x",
            "evaluate",
        )
        self.assertEqual(returncode, 0, raw)
        self.assertEqual(expression["unbound"], "ζ(p) - ζ(p, n + 1)")
        self.assertIn(r"\sum_{k=1}^{n}\frac{1}{k^{p}}", expression["derivation_TeX"])
        self.assertIn(r"\zeta(p) - \zeta(p, n + 1)", expression["derivation_TeX"])
        self.assertEqual(
            expression["value"],
            "1.340825569751464008214707481847132424878798298689670562647574709",
        )

        # README example: a symbolic geometric endpoint shows its sigma and exact closed form before evaluation.
        expression, raw, returncode = mars_lab.run_mars_lab_fields(
            scratch / "mars_lab",
            "1+3+9+27+...+3^(n-1) | n=10",
            64,
            "x",
            "evaluate",
        )
        self.assertEqual(returncode, 0, raw)
        self.assertEqual(expression["unbound"], "1/2·(3^n - 1)")
        self.assertIn(r"\sum_{k=0}^{n - 1}3^{k}", expression["derivation_TeX"])
        self.assertIn(r"\frac{3^{n} - 1}{2}", expression["derivation_TeX"])
        self.assertEqual(expression["value"], "29524")

        # README example: a formal sine sum supplies its geometric closed form and large-bound value.
        source = "{ @Z_(k=1)^n sin(kx) | x=pi/6; n=100000 }"
        expression, raw, returncode = mars_lab.run_mars_lab_fields(
            scratch / "mars_lab",
            source,
            64,
            "x",
            "evaluate",
        )
        self.assertEqual(returncode, 0, raw)
        payload = mars_lab.prepare_evaluation_fields(
            scratch / "mars_lab", expression, source, 64, False, wrt="x", action="evaluate"
        )
        self.assertEqual(
            payload["full_display_expression"],
            "{ sin(nx/2)·sin(x/2·(n + 1))/sin(x/2) | x = π/6; n = 100000 }",
        )
        self.assertEqual(
            payload["display_TeX"],
            r"\sum_{k=1}^{n}\sin(k\mkern-2mu x) = "
            r"\frac{\sin(\frac{n\mkern-2mu x}{2})\mkern-2mu "
            r"\sin(\frac{x}{2}\mkern-2mu \left(n + 1\right))}{\sin(\frac{x}{2})}",
        )
        self.assertIn("return sin(n.v1).sin((n + 1).v1)/sin(v1).", payload["full_display_function"])
        self.assertTrue(str(payload.get("value", "")).startswith("3.232050807568877293527446341505872"))

        # README example: a formal exponential sum supplies its geometric closed form and large-bound value.
        source = "{ @Z_(k=1)^n exp(kx) | x=2; n=100000 }"
        expression, raw, returncode = mars_lab.run_mars_lab_fields(
            scratch / "mars_lab",
            source,
            64,
            "x",
            "evaluate",
        )
        self.assertEqual(returncode, 0, raw)
        payload = mars_lab.prepare_evaluation_fields(
            scratch / "mars_lab", expression, source, 64, False, wrt="x", action="evaluate"
        )
        self.assertEqual(
            payload["full_display_expression"],
            "{ exp(x)/(exp(x) - 1)·(exp(nx) - 1) | x = 2; n = 100000 }",
        )
        self.assertIn(r"\sum_{k=1}^{n}e^{k\mkern-2mu x} =", payload["display_TeX"])
        self.assertIn("return v1.(exp(n.x) - 1)/(v1 - 1).", payload["full_display_function"])
        self.assertEqual(
            payload["value"],
            "9.110304914770879911502042940141264278041407847643843263784059825E+86858",
        )

        # README example: a formal logarithmic sum supplies its log-gamma closed form and large-bound value.
        source = "{ @Z_(k=1)^n ln(kx) | x=2; n=100000 }"
        expression, raw, returncode = mars_lab.run_mars_lab_fields(
            scratch / "mars_lab",
            source,
            64,
            "x",
            "evaluate",
        )
        self.assertEqual(returncode, 0, raw)
        payload = mars_lab.prepare_evaluation_fields(
            scratch / "mars_lab", expression, source, 64, False, wrt="x", action="evaluate"
        )
        self.assertEqual(
            payload["full_display_expression"],
            "{ n·ln(x) + lnΓ(n + 1) | x = 2; n = 100000 }",
        )
        self.assertIn(r"\sum_{k=1}^{n}\ln(k\mkern-2mu x) =", payload["display_TeX"])
        self.assertIn("return n.ln(x) + lgamma(n + 1).", payload["full_display_function"])
        self.assertEqual(
            payload["value"],
            "1120613.939955116396071001320351928512056894536584146906526018233",
        )

        # README example: common-log input keeps its base-ten reduction distinct from natural logarithms.
        source = "{ @Z_(k=1)^n log(kx) | x=2; n=100000 }"
        expression, raw, returncode = mars_lab.run_mars_lab_fields(
            scratch / "mars_lab",
            source,
            64,
            "x",
            "evaluate",
        )
        self.assertEqual(returncode, 0, raw)
        payload = mars_lab.prepare_evaluation_fields(
            scratch / "mars_lab", expression, source, 64, False, wrt="x", action="evaluate"
        )
        self.assertEqual(
            payload["full_display_expression"],
            "{ n·lg(x) + lnΓ(n + 1)/ln(10) | x = 2; n = 100000 }",
        )
        self.assertIn(r"\sum_{k=1}^{n}\lg(k\mkern-2mu x) =", payload["display_TeX"])
        self.assertIn("return n.lg(x) + lgamma(n + 1)/ln(10).", payload["full_display_function"])
        self.assertEqual(
            payload["value"],
            "486676.4504663690278820372835671954350107214367484924045390786799",
        )

        # README example: a versed-sine progression reuses the exact cosine progression.
        source = "{ @Z_(k=1)^n versin(kx) | x=2; n=100000 }"
        expression, raw, returncode = mars_lab.run_mars_lab_fields(
            scratch / "mars_lab", source, 64, "x", "evaluate"
        )
        self.assertEqual(returncode, 0, raw)
        payload = mars_lab.prepare_evaluation_fields(
            scratch / "mars_lab", expression, source, 64, False, wrt="x", action="evaluate"
        )
        self.assertEqual(
            payload["full_display_expression"],
            "{ n - sin(nx/2)·cos(x/2·(n + 1))/sin(x/2) | x = 2; n = 100000 }",
        )
        self.assertTrue(payload["value"].startswith("100000.0242173437116803434689891041295735315676686381885043434219"))

        # README example: every registered progression reducer accepts the composite step ax.
        source = "{ @Z_(k=1)^n sin(kax) | a=2; x=3; n=100000 }"
        expression, raw, returncode = mars_lab.run_mars_lab_fields(
            scratch / "mars_lab", source, 64, "x", "evaluate"
        )
        self.assertEqual(returncode, 0, raw)
        payload = mars_lab.prepare_evaluation_fields(
            scratch / "mars_lab", expression, source, 64, False, wrt="x", action="evaluate"
        )
        self.assertIn("sin(ax/2·(n + 1))/sin(ax/2)", payload["full_display_expression"])
        self.assertNotIn("Σ_", payload["full_display_expression"])
        self.assertIn("v1 = a.x.", payload["function"])
        self.assertIn("v2 = v1/2.", payload["function"])
        self.assertIn("return sin(n.v1/2).sin((n + 1).v1/2)/sin(v2).", payload["function"])
        self.assertIn(r"\sum_{k=1}^{n}\sin(a\mkern-2mu k\mkern-2mu x) =", payload["tex"])
        self.assertEqual(
            payload["value"],
            "-0.1868614750758504223995060240052491962444814290852506553280866826",
        )

        # README example: a square-root progression uses the Hurwitz-zeta power sum.
        source = "{ @Z_(k=1)^n sqrt(kx) | x=2; n=100000 }"
        expression, raw, returncode = mars_lab.run_mars_lab_fields(
            scratch / "mars_lab", source, 64, "x", "evaluate"
        )
        self.assertEqual(returncode, 0, raw)
        payload = mars_lab.prepare_evaluation_fields(
            scratch / "mars_lab", expression, source, 64, False, wrt="x", action="evaluate"
        )
        self.assertEqual(
            payload["full_display_expression"],
            "{ √(x)·(ζ(-1/2) - ζ(-1/2, n + 1)) | x = 2; n = 100000 }",
        )
        self.assertIn("const c1 = -1/2.", payload["function"])
        self.assertNotIn("const c2", payload["function"])
        self.assertIn("zeta(c1) - zetah(c1, n + 1)", payload["function"])
        self.assertEqual(payload["value"], "29814463.01298576613569741465397922838928192939324835606473553721")

        # README example: an integral floor step is a domain-required arithmetic progression.
        source = "{ @Z_(k=1)^n floor(kx) | x=2; n=100000 }"
        expression, raw, returncode = mars_lab.run_mars_lab_fields(
            scratch / "mars_lab", source, 64, "x", "evaluate"
        )
        self.assertEqual(returncode, 0, raw)
        self.assertEqual(expression["algebraic_specialisation"], "domain-required")
        payload = mars_lab.prepare_evaluation_fields(
            scratch / "mars_lab", expression, source, 64, False, wrt="x", action="evaluate"
        )
        self.assertEqual(payload["full_display_expression"], "{ x·n/2·(n + 1) | x = 2; n = 100000 }")
        self.assertEqual(payload["value"], "10000100000")

        # README example: a small rational floor step uses complete periods and a bounded remainder.
        source = "{ @Z_(k=1)^n floor(0.6k) | n=100000 }"
        expression, raw, returncode = mars_lab.run_mars_lab_fields(
            scratch / "mars_lab", source, 64, "x", "evaluate"
        )
        self.assertEqual(returncode, 0, raw)
        self.assertEqual(expression["algebraic_specialisation"], "domain-required")
        payload = mars_lab.prepare_evaluation_fields(
            scratch / "mars_lab", expression, source, 64, False, wrt="x", action="evaluate"
        )
        self.assertIn("⌊n/5⌋", payload["full_display_expression"])
        self.assertIn("mod(n, 5)", payload["full_display_expression"])
        self.assertNotIn("Σ_", payload["full_display_expression"])
        self.assertEqual(payload["value"], "2999990000")

        # README examples: symbolic complex elementary functions use Cartesian output.
        for source, expected in (
            ("exp(x+i*y)", "exp(x)·cos(y) + exp(x)·sin(y)·i"),
            ("sin(i*y)", "0 + sinh(y)·i"),
        ):
            with self.subTest(readme_example=source):
                expression, raw, returncode = mars_lab.run_mars_lab_fields(
                    scratch / "mars_lab",
                    source,
                    64,
                    "x",
                    "evaluate",
                )
                self.assertEqual(returncode, 0, raw)
                self.assertEqual(expression["unbound"], expected)

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
            ("inverse(a b; c d).(x; y)", "(1/(ad - bc)·(dx - by); 1/(ad - bc)·(ay - cx))"),
            ("Dx(ax+b cx+d; y xy)", "(a, c; 0, y)"),
            ("Dxx(x^3 xy; y^2 x^2y)", "(6x, 0; 0, 2y)"),
            ("Dxy(x^2y x*y^2; y^3 x^3y)", "(2x, 2y; 0, 3x²)"),
            (
                "@S(ax+b cx+d; y xy)dx",
                "(½·(ax² + 2bx), ½·(cx² + 2dx); xy, ½x²y) + (C₁₁, C₁₂; C₂₁, C₂₂)",
            ),
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
                "date": "2026-08-20",
                "start": "2026-08-20",
                "end": "2027-01-01",
                "year": "2026",
                "lat": "52.7077",
                "lon": "-2.7541",
                "elevation": "75",
                "gmt_offset": "1",
                "jurisdiction": "GB-ENG",
            },
        )
        self.assertEqual(returncode, 0, raw)
        self.assertEqual(datetime["weekday"], "Thursday")
        self.assertEqual(datetime["sunrise"], "2026-08-20 05:59:19")
        self.assertEqual(datetime["moon_phase"], "First Quarter")

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

        documentation = (ROOT / "docs" / "mars-lab.md").read_text(encoding="utf-8")
        self.assertIn("After clicking <strong>Evaluate</strong>", documentation)
        self.assertIn("After clicking <strong>x derivative</strong>", documentation)
        self.assertIn("After clicking <strong>x integral</strong>", documentation)
        for name in ("matrix-power-derivative", "matrix-power-integral"):
            screenshot = ROOT / "docs" / "images" / "mars-lab" / f"{name}.png"
            with self.subTest(screenshot=name):
                self.assertTrue(screenshot.is_file())
                self.assertTrue(screenshot.read_bytes().startswith(b"\x89PNG\r\n\x1a\n"))
                self.assertIn(f"images/mars-lab/{name}.png", documentation)


if __name__ == "__main__":
    unittest.main()
