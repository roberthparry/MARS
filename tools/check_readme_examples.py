#!/usr/bin/env python3
"""Compile and run the complete C programs printed in MARS Markdown guides."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import shlex
import socket
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
FENCE = re.compile(r"^```([^\n]*)\n(.*?)^```[ \t]*$", re.M | re.S)


def examples():
    paths = sorted(p for p in ROOT.rglob("*.md")
                   if not any(part in {".git", "build", ".codex", ".agents"} for part in p.relative_to(ROOT).parts)
                   and p.name != "AGENTS.md")
    for path in paths:
        text = path.read_text()
        blocks = list(FENCE.finditer(text))
        index = 0
        for n, match in enumerate(blocks):
            if match[1].strip() != "c":
                continue
            index += 1
            code = match[2]
            clean = re.sub(r"/\*.*?\*/|//[^\n]*", "", code, flags=re.S)
            declarations_removed = re.sub(r"\btypedef\b(?:[^{};]|\{[^{}]*\})*;", "", clean, flags=re.S)
            has_main = bool(re.search(r"\bmain\s*\(", code))
            executable = has_main or "#include" in clean or (
                re.search(r"(?<![=!<>])=(?!=)", declarations_removed)
                or re.search(r"^\s*\w+\s*\(", declarations_removed, re.M)
                or re.search(r"\b\w+\s*\([^;{}]*\)\s*\{", declarations_removed))
            if not executable:
                continue  # Type declarations and API signatures are references, not programs.
            rel = str(path.relative_to(ROOT))
            ident = re.sub(r"\W+", "_", rel.removesuffix(".md")) + f"_{index:03d}"
            expected = None
            for following in blocks[n + 1:]:
                between = text[match.end():following.start()]
                if following[1].strip() == "c" or re.search(r"^#{1,6} ", between, re.M):
                    break
                if following[1].strip() in {"text", "json", "xml"}:
                    expected = following[2]
                    break
            yield ident, rel, text[:match.start()].count("\n") + 1, code, has_main, expected


def normalise(text):
    return "\n".join(line.rstrip() for line in text.splitlines()).strip("\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default="cc")
    parser.add_argument("--cflags", default="-D_GNU_SOURCE -std=gnu11 -Wall -Wextra -Werror")
    parser.add_argument("--libs", required=True)
    parser.add_argument("--archive", default="build/release/libmars.a")
    parser.add_argument("--compile-only", action="store_true")
    parser.add_argument("--timeout", type=float, default=60)
    parser.add_argument("--output", default="build/readme-examples")
    args = parser.parse_args()
    output = ROOT / args.output
    output.mkdir(parents=True, exist_ok=True)
    config = json.loads((ROOT / "tests/test_config.json").read_text()).get(
        "tools/check_readme_examples.py", {}).get("readme_examples", {})
    records = []
    failed = 0
    server = None
    listener = None
    try:
        for ident, path, line, code, has_main, expected in examples():
            if not config.get(ident, config.get("enabled", True)):
                print(f"SKIP README {ident}", flush=True)
                continue
            record = {"id": ident, "path": path, "line": line}
            records.append(record)
            if not has_main:
                record["error"] = "C example has no main()"
                failed += 1
                print(f"FAIL README {ident}: {record['error']}", flush=True)
                continue
            source = output / (ident + ".c")
            binary = output / ident
            source.write_text(code)
            compile_command = [*shlex.split(args.cc), *shlex.split(args.cflags),
                               "-I" + str(ROOT / "include"), str(source),
                               str(ROOT / args.archive), *shlex.split(args.libs), "-o", str(binary)]
            compiled = subprocess.run(compile_command, capture_output=True, text=True)
            (output / (ident + ".compile.log")).write_text(compiled.stdout + compiled.stderr)
            if compiled.returncode:
                record["error"] = "compile/link failure"
            elif not args.compile_only:
                try:
                    with tempfile.TemporaryDirectory(prefix="mars-readme-") as working:
                        command = [str(binary)]
                        if path == "docs/file.md":
                            arity = re.search(r"argc != (\d+)", code)
                            if arity:
                                count = int(arity[1]) - 1
                                command.extend(["directory", "directory/documents", "directory/shortcut"]
                                               if "symlink_listing_example" in code else
                                               [f"disposable-{i}" for i in range(count)])
                        if "https://httpbin.org/" in code:
                            if server is None:
                                listener = socket.socket()
                                listener.bind(("127.0.0.1", 0))
                                listener.listen(8)
                                server = subprocess.Popen(
                                    ["python3", str(ROOT / "tests/http/http_fixture.py"), str(listener.fileno())],
                                    pass_fds=[listener.fileno()])
                            route = "/get?message=MARS" if "/get?message=MARS" in code else "/post"
                            command.append(f"http://127.0.0.1:{listener.getsockname()[1]}{route}")
                        result = subprocess.run(command, cwd=working, capture_output=True,
                                                text=True, timeout=args.timeout)
                        (output / (ident + ".actual")).write_text(result.stdout)
                        (output / (ident + ".stderr")).write_text(result.stderr)
                        record["status"] = result.returncode
                        if result.returncode:
                            record["error"] = "program failed"
                        elif expected is None:
                            record["error"] = "missing documented output"
                        elif normalise(result.stdout) != normalise(expected):
                            record["error"] = "documented output differs"
                            (output / (ident + ".expected")).write_text(expected)
                except subprocess.TimeoutExpired:
                    record["error"] = "program timed out"
            if "error" in record:
                failed += 1
                print(f"FAIL README {ident}: {record['error']} ({path}:{line})", flush=True)
            else:
                print(f"PASS README {ident}", flush=True)
    finally:
        if server is not None:
            server.terminate()
            server.wait(timeout=5)
        if listener is not None:
            listener.close()
    (output / "results.json").write_text(json.dumps(records, indent=2) + "\n")
    print(f"README programs: {len(records)} checked, {failed} failed", flush=True)
    return bool(failed)


if __name__ == "__main__":
    raise SystemExit(main())
