"""Regression tests for complete Markdown public-function coverage."""

from __future__ import annotations

import importlib.util
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
