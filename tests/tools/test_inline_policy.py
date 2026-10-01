"""Audit inline definition size and macro visibility; triviality still needs review."""

import re
import unittest
from bisect import bisect_right
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SOURCE_ROOTS = ("include", "src", "tests", "scratch", "bench", "tools")
EXCLUDED_DIRS = {"build", "vendor", "third_party", "node_modules", "__pycache__"}
INLINE_WORDS = {"inline", "__inline", "__inline__"}
LITERALS_AND_COMMENTS = re.compile(
    r'//(?:\\\r?\n|[^\n])*|/\*[\s\S]*?\*/|"(?:\\[\s\S]|[^"\\])*"|'
    r"'(?:\\[\s\S]|[^'\\])*'"
)
DIRECTIVES = re.compile(r"^[ \t]*#(?:[^\n]*\\\r?\n)*[^\n]*", re.M)
TOKENS = re.compile(r"[A-Za-z_]\w*|[^\s]")


def blank_text(text):
    """Keep offsets and physical newlines when masking non-code."""
    return "".join("\n" if char == "\n" else " " for char in text)


def scan_inline_definitions(source):
    """Return inclusive definition line spans and macros containing inline tokens.

    This recognises ordinary C declarations, including split signatures and GNU
    inline spellings. It deliberately inspects every preprocessor branch rather
    than relying on one build configuration.
    """
    masked = LITERALS_AND_COMMENTS.sub(lambda match: blank_text(match.group()), source)
    newlines = [match.start() for match in re.finditer("\n", source)]
    macro_lines = []

    def line_at(offset):
        return bisect_right(newlines, offset) + 1

    def mask_directive(match):
        directive = match.group()
        words = set(re.findall(r"[A-Za-z_]\w*", directive))
        if re.match(r"[ \t]*#[ \t]*define\b", directive) and words & INLINE_WORDS:
            macro_lines.append(line_at(match.start()))
        return blank_text(directive)

    masked = DIRECTIVES.sub(mask_directive, masked)
    spans = []
    declaration = []
    braces = 0
    parentheses = 0
    inline_start = None
    for match in TOKENS.finditer(masked):
        token = match.group()
        if braces:
            if token == "{":
                braces += 1
            elif token == "}":
                braces -= 1
                if not braces:
                    if inline_start is not None:
                        spans.append((inline_start, line_at(match.start())))
                    inline_start = None
                    declaration = []
            continue

        if token == "{":
            words = {word for word, _ in declaration}
            if words & INLINE_WORDS and "(" in words:
                inline_start = line_at(declaration[0][1])
            braces = 1
            parentheses = 0
        elif token == ";" and not parentheses:
            declaration = []
        else:
            declaration.append((token, match.start()))
            parentheses += (token == "(") - (token == ")")
    if inline_start is not None:
        raise ValueError(f"unterminated inline definition at line {inline_start}")
    return spans, macro_lines


def inline_policy_violations(source):
    spans, macro_lines = scan_inline_definitions(source)
    violations = [
        f"line {start}: inline definition occupies {end - start + 1} physical lines (maximum 3)"
        for start, end in spans if end - start + 1 > 3
    ]
    violations.extend(f"line {line}: macro conceals inline code or its declaration" for line in macro_lines)
    return violations


class InlineScannerTests(unittest.TestCase):
    def test_comments_and_literals_are_not_definitions(self):
        source = (
            '/* static inline int fake(void) {\nreturn 1;\n} */\n'
            '// inline int fake(void) {} \\\ncontinued inline int fake(void) {}\n'
            'const char *text = "inline int fake(void) { \\"quoted\\" }";\n'
            "const char brace = '}';\n"
            '#define TEXT "static inline int fake(void) {}"\n'
            '#define NUMBER 1 /* inline is only documentation */\n'
        )
        self.assertEqual(scan_inline_definitions(source), ([], []))

    def test_three_line_limit_and_multiline_declaration(self):
        for keyword in sorted(INLINE_WORDS):
            with self.subTest(keyword=keyword):
                short = f"static {keyword} int f(void) {{\n    return 1;\n}}\n"
                self.assertEqual(scan_inline_definitions(short), ([(1, 3)], []))
                self.assertEqual(inline_policy_violations(short), [])
                long = short.replace(f"static {keyword}", f"static\n{keyword}")
                self.assertEqual(scan_inline_definitions(long), ([(1, 4)], []))
                self.assertEqual(len(inline_policy_violations(long)), 1)

    def test_prototypes_and_ordinary_functions_are_ignored(self):
        source = (
            "static inline int prototype(int (*callback)(int));\n"
            "int ordinary(void) {\n    return 1;\n}\n"
            "static inline int actual(void) { return 2; }\n"
            "inline int another(void) { return 3; }\n"
        )
        self.assertEqual(scan_inline_definitions(source), ([(5, 5), (6, 6)], []))

    def test_nested_braces_and_literals_do_not_end_body(self):
        source = (
            "static inline int f(int x) {\n"
            '    if (x) { const char *s = "}"; /* } */ return s[0]; }\n'
            "    return '}';\n"
            "}\n"
        )
        self.assertEqual(scan_inline_definitions(source), ([(1, 4)], []))
        self.assertEqual(len(inline_policy_violations(source)), 1)

    def test_blank_and_comment_lines_inside_definition_count(self):
        source = "inline int f(void) {\n\n    /* Explanation. */\n    return 1;\n}\n"
        self.assertEqual(scan_inline_definitions(source), ([(1, 5)], []))
        self.assertEqual(len(inline_policy_violations(source)), 1)

    def test_inline_macros_are_rejected_even_when_short(self):
        for definition in (
            "#define MAKE(name) static inline int name(void) { return 1; }\n",
            "#define MAKE(name) \\\n    static inline int name(void) { \\\n    return 1; }\n",
            "#define LOCAL_INLINE static __inline__\n",
        ):
            with self.subTest(definition=definition):
                self.assertEqual(scan_inline_definitions(definition), ([], [1]))
                self.assertEqual(len(inline_policy_violations(definition)), 1)

    def test_all_preprocessor_branches_are_audited(self):
        source = "#if 0\ninline int f(void) {\n    return 1;\n}\n#endif\n"
        self.assertEqual(scan_inline_definitions(source), ([(2, 4)], []))

    def test_unterminated_inline_definition_is_reported(self):
        with self.assertRaisesRegex(ValueError, "unterminated inline definition"):
            scan_inline_definitions("inline int f(void) {\n")


class InlinePolicyTests(unittest.TestCase):
    def test_project_inline_definitions(self):
        files = [
            path
            for directory in SOURCE_ROOTS
            for path in (ROOT / directory).rglob("*")
            if path.suffix in {".c", ".h"} and path.is_file() and not path.is_symlink()
            and not EXCLUDED_DIRS.intersection(path.relative_to(ROOT).parts)
        ]
        self.assertTrue(files, "No project-owned C sources found")
        for path in sorted(files):
            with self.subTest(path=str(path.relative_to(ROOT))):
                violations = inline_policy_violations(path.read_text(encoding="utf-8"))
                self.assertEqual(violations, [], "\n".join(violations))


if __name__ == "__main__":
    unittest.main()
