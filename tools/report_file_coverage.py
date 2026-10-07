#!/usr/bin/env python3
"""Report measured file-module line and branch coverage from GCC JSON output."""

import gzip
import json
import re
import sys
from pathlib import Path


def main():
    directory = Path(sys.argv[1])
    totals = [0, 0, 0, 0]
    found = set()
    executed = set()
    for report in sorted(directory.glob("file_*.gcov.json.gz")):
        with gzip.open(report, "rt", encoding="utf-8") as source:
            data = json.load(source)
        for item in data["files"]:
            path = Path(item["file"])
            if path.parent.as_posix() != "src/file":
                continue
            found.add(path.name)
            executed.update(function["name"] for function in item.get("functions", [])
                            if function["execution_count"] > 0)
            lines = item["lines"]
            branches = [branch for line in lines for branch in line.get("branches", [])]
            counts = [sum(line["count"] > 0 for line in lines), len(lines),
                      sum(branch["count"] > 0 for branch in branches), len(branches)]
            totals = [a + b for a, b in zip(totals, counts)]
            print(f"{path.name}: lines {counts[0]}/{counts[1]}, branch outcomes {counts[2]}/{counts[3]}")
    expected = {path.name for path in Path("src/file").glob("*.c")}
    if found != expected:
        raise SystemExit("Missing or unexpected file-module coverage reports")
    covered, lines, taken, branches = totals
    public = set(re.findall(r"\b(file_\w+)\s*\(", Path("include/file.h").read_text(encoding="utf-8")))
    missing = sorted(public - executed)
    print(f"TOTAL: lines {covered}/{lines} ({100 * covered / lines:.2f}%), "
          f"branch outcomes {taken}/{branches} ({100 * taken / branches:.2f}%)")
    print(f"Public API functions executed: {len(public) - len(missing)}/{len(public)}")
    if missing:
        raise SystemExit("Unexecuted public API functions: " + ", ".join(missing))
    if covered / lines < 0.90 or taken / branches < 0.80:
        raise SystemExit("File coverage requires at least 90% of lines and 80% of branch outcomes")


if __name__ == "__main__":
    main()
