"""Keep the inline function registry's human-readable columns aligned."""

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class FunctionTableLayoutTests(unittest.TestCase):
    def test_inline_table_columns(self):
        source = (ROOT / "src/expression/expr_stringin.c").read_text(encoding="utf-8")
        table = re.search(r"static const func_entry_t s_funcs\[FUNC_TABLE_SIZE\] = \{\n(.*?)\n\};", source, re.S)
        self.assertIsNotNone(table)
        rows = table.group(1).splitlines()
        size = int(re.search(r"#define FUNC_TABLE_SIZE (\d+)", source).group(1))
        self.assertEqual(len(rows), size)
        columns = {}
        for slot, row in enumerate(rows):
            with self.subTest(slot=slot):
                self.assertTrue(row.startswith(f"    [{slot:3d}] = {{ "))
                self.assertTrue(row.endswith(" },"))
                self.assertNotIn("\t", row)
                self.assertNotRegex(row, r"(?<! )=|=(?! )")
                # All current aliases consist of single-cell Unicode characters;
                # Python counts those characters, not their UTF-8 encoding bytes.
                fields = list(re.finditer(r"\.(kw|arity|[ubtsv]fn|ops) = ", row))
                # The four dual-handler entries need a small width allowance to
                # preserve both aligned columns and one complete entry per line.
                self.assertLessEqual(len(row), 140 if len(fields) == 5 else 130)
                self.assertGreaterEqual(len(fields), 3)
                for index, field in enumerate(fields):
                    name = "handler" if index == 2 else field.group(1)
                    expected = columns.setdefault(name, field.start())
                    self.assertEqual(field.start(), expected, name)


if __name__ == "__main__":
    unittest.main()
