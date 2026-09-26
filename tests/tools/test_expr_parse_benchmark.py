"""README workload check for the public expression-parser benchmark."""

import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
BINARY = ROOT / "build/release/bench/expression/bench_expr_parse"


class ParserBenchmarkReadmeTests(unittest.TestCase):
    def test_readme_parser_benchmark(self):
        # README example: docs/benchmarks.md, run after ordinary parser regressions.
        result = subprocess.run([str(BINARY), "--check"], cwd=ROOT, capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, "parser benchmark: 24 inputs verified\n")


if __name__ == "__main__":
    unittest.main()
