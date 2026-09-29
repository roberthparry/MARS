"""Regression tests for complete Markdown public-function coverage."""

from __future__ import annotations

import importlib.util
import json
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "check_markdown_api_coverage", ROOT / "tools" / "check_markdown_api_coverage.py"
)
assert SPEC and SPEC.loader
coverage = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = coverage
SPEC.loader.exec_module(coverage)


class MarkdownApiCoverageTests(unittest.TestCase):
    """Check public declaration extraction and the committed reference."""

    def test_preview_formula_alignment_applies_to_every_guide(self) -> None:
        settings = json.loads((ROOT / ".vscode/settings.json").read_text(encoding="utf-8"))
        self.assertEqual(settings["markdown.styles"][-1], "docs/preview.css")
        stylesheet = (ROOT / "docs/preview.css").read_text(encoding="utf-8")
        stylesheet = re.sub(r"/\*.*?\*/", "", stylesheet, flags=re.DOTALL)
        rules = {
            selector.strip(): dict(re.findall(r"([\w-]+)\s*:\s*([^;]+);", declarations))
            for selector, declarations in re.findall(r"([^{}]+)\{([^{}]*)\}", stylesheet)
        }
        # KaTeX centres both the display container and its inner maths box.
        for selector in ("body .katex-display", "body .katex-display > .katex", "body p:has(> .katex)"):
            with self.subTest(selector=selector):
                self.assertEqual(rules[selector]["text-align"], "left")
        self.assertEqual(rules["body .katex-display"]["padding-left"], "1em")
        self.assertEqual(rules["body .katex-display"]["overflow-x"], "auto")
        self.assertEqual(rules["body .katex-display > .katex"]["white-space"], "nowrap")
        self.assertNotIn("white-space", rules["body p:has(> .katex)"])

    def test_guide_formulae_are_indented_unbroken_paragraphs(self) -> None:
        names = subprocess.check_output(
            ["git", "ls-files", "-z", "--", "*.md"], cwd=ROOT, text=True
        ).rstrip("\0").split("\0")
        formulae = 0
        for name in names:
            with self.subTest(document=name):
                formulae += self.check_indented_formulae(name)
        self.assertGreater(formulae, 0)

    def check_indented_formulae(self, name: str) -> int:
        lines = (ROOT / name).read_text(encoding="utf-8").splitlines()
        prefix = r"$\quad\begin{array}{l}\displaystyle "
        suffix = r"\end{array}$"
        fence = None
        formulae = 0

        def unquote(line: str) -> str:
            return re.sub(r"^\s*(?:>\s*)?", "", line)

        for index, raw in enumerate(lines):
            line = unquote(raw)
            marker = re.match(r"(`{3,}|~{3,})", line)
            if marker:
                token = marker.group(1)
                if fence is None:
                    fence = token
                elif token[0] == fence[0] and len(token) >= len(fence):
                    fence = None
                continue
            if fence is not None:
                continue
            self.assertFalse(line.startswith("$$"), "Standalone formulae must not use centred display blocks")
            self.assertNotIn(line.strip(), (r"\[", r"\]"))
            previous_blank = index == 0 or not unquote(lines[index - 1]).strip()
            next_blank = index + 1 == len(lines) or not unquote(lines[index + 1]).strip()
            standalone = re.fullmatch(r"\$[^$]+\$[.,;:]?", line) and previous_blank and next_blank
            if line.startswith(prefix) or standalone:
                with self.subTest(line=index + 1):
                    self.assertTrue(line.startswith(prefix))
                    self.assertTrue(line.endswith(suffix))
                    self.assertEqual(line.count("$"), 2, "Inline prose must not be wrapped as a formula")
                    self.assertNotIn(r"\tag{", line, "Display-only tags must retain their labels as text")
                    self.assertTrue(previous_blank)
                    self.assertTrue(next_blank)
                formulae += 1
        return formulae

    def test_laplace_documentation_uses_markdown_math_delimiters(self) -> None:
        for name in ("expression.md", "design-notes/integral-transforms.md"):
            fenced = False
            math_rows = 0
            for line in (ROOT / "docs" / name).read_text(encoding="utf-8").splitlines():
                if line.lstrip().startswith(("```", "~~~")):
                    fenced = not fenced
                if fenced:
                    continue
                with self.subTest(document=name, line=line):
                    self.assertNotEqual(line.strip(), "$", "Display maths requires double-dollar delimiters")
                    for delimiter in (r"\(", r"\)", r"\[", r"\]"):
                        self.assertNotIn(delimiter, line)
                    if line.startswith("|") and "$" in line:
                        math_rows += 1
                        self.assertEqual(line.count("$") % 2, 0)
            self.assertGreater(math_rows, 10, name)

    def test_extractor_finds_prototypes_and_inline_functions_only(self) -> None:
        source = """
        typedef int (*callback_t)(int value);
        int public_call(int value);
        static inline int inline_call(int value) { return value + 1; }
        #define FUNCTION_LIKE(value) (value)
        """
        with tempfile.TemporaryDirectory() as directory:
            header = Path(directory) / "sample.h"
            header.write_text(source, encoding="utf-8")
            functions = coverage.public_functions_from_header(header)

        self.assertEqual([function.name for function in functions], ["inline_call", "public_call"])

    def test_every_public_function_is_covered_by_its_module_guide(self) -> None:
        for function in coverage.public_functions():
            guide_name = coverage.MODULE_GUIDES[function.header]
            guide = coverage.REPOSITORY_ROOT / "docs" / guide_name
            with self.subTest(header=function.header, function=function.name):
                self.assertTrue(coverage.function_is_mentioned(function, guide.read_text(encoding="utf-8")))


if __name__ == "__main__":
    unittest.main()
