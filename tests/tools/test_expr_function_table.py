"""Keep inline function registries aligned and their perfect hashes collision-free."""

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class FunctionTableLayoutTests(unittest.TestCase):
    def read_registry(self, filename, table_name, displacement_name, size_name):
        source = (ROOT / "src/expression" / filename).read_text(encoding="utf-8")
        table = re.search(r"static const \w+ " + table_name + r"\[\w+\] = \{\n(.*?)\n\};", source, re.S)
        self.assertIsNotNone(table)
        entries = re.findall(r'\[\s*(\d+)\] = \{ \.kw = "([^"]+)",(.*?) \},', table.group(1))
        self.assertEqual(len(entries), len(table.group(1).splitlines()))
        self.assertEqual(len({int(slot) for slot, _, _ in entries}), len(entries))
        self.assertEqual(len({kw for _, kw, _ in entries}), len(entries))
        displacement = re.search(displacement_name + r"\[\w+\] = \{\n(.*?)\n\};", source, re.S)
        self.assertIsNotNone(displacement)
        shifts = [int(value) for value in re.findall(r"\d+", displacement.group(1))]
        self.assertTrue(all(0 <= shift <= 255 for shift in shifts))
        size = int(re.search(r"#define " + size_name + r" (\d+)", source).group(1))
        for slot, _, _ in entries:
            self.assertLess(int(slot), size)
        return source, entries, shifts, size

    def test_inline_perfect_hash(self):
        source, entries, shifts, size = self.read_registry(
            "expr_stringin.c", "s_funcs", "s_func_displacements", "FUNC_TABLE_SIZE")
        bucket_count = int(re.search(r"#define FUNC_HASH_BUCKETS (\d+)", source).group(1))
        max_bytes = int(re.search(r"#define FUNC_KEYWORD_MAX_BYTES (\d+)", source).group(1))
        seed = int(re.search(r"unsigned hash = \((\d+)u \^", source).group(1))
        self.assertEqual(len(shifts), bucket_count)
        self.assertEqual(len(entries), size)
        used_buckets = set()
        for slot, kw, _ in entries:
            with self.subTest(keyword=kw):
                raw = kw.encode("utf-8")
                length = len(raw)
                self.assertLessEqual(length, max_bytes)
                value = ((seed ^ length) * 16777619) & 0xffffffff
                # Match the native fixed byte samples, including continuation bytes
                # and unsigned underflow for positions before the start of a name.
                for pos in (0, length - 1, length - 3, 1, 3, 2):
                    sample = 0
                    if 0 <= pos < length and not 0x80 <= raw[pos] <= 0xbf:
                        sample = ord(raw[pos:].decode("utf-8")[0])
                    value = ((value ^ sample) * 16777619) & 0xffffffff
                bucket = (value >> 16) % bucket_count
                used_buckets.add(bucket)
                self.assertEqual((value % size + shifts[bucket]) % size, int(slot))
        for bucket, shift in enumerate(shifts):
            if bucket not in used_buckets:
                self.assertEqual(shift, 0)
        handlers = {kw: fields for _, kw, fields in entries}
        for alias in ("sgn", "sign", "signum"):
            self.assertIn(alias, handlers)
            self.assertRegex(handlers[alias], r"\.arity = 1u,\s+\.ufn = expr_sgn,\s+\.ops = &ops_sgn$")

    def test_binding_perfect_hash(self):
        source, entries, shifts, size = self.read_registry(
            "expr_bindings.c", "s_binding_funcs", "s_binding_func_displacements", "BINDING_FUNC_TABLE_SIZE")
        self.assertEqual(len(shifts), size)
        seeds = [int(seed, 16) for seed in re.findall(r"return binding_func_hash_values\(kw, (0x[0-9a-f]+)u\);", source)]
        self.assertEqual(len(seeds), 2)

        def hash_value(kw, seed):
            value = seed ^ len(kw.encode("utf-8"))
            for index, char in enumerate(kw, 1):
                value ^= (ord(char) + 0x9e3779b9 + index * 0x85ebca6b) & 0xffffffff
                value = (value * 16777619) & 0xffffffff
                value ^= value >> 13
            return value % size

        for slot, kw, _ in entries:
            with self.subTest(keyword=kw):
                bucket = hash_value(kw, seeds[0])
                self.assertEqual((hash_value(kw, seeds[1]) + shifts[bucket]) % size, int(slot))
        handlers = {kw: fields for _, kw, fields in entries}
        for alias in ("sgn", "sign", "signum"):
            self.assertIn(alias, handlers)
            self.assertRegex(handlers[alias], r"\.is_binary = false,\s+\.ops = &ops_sgn$")

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
