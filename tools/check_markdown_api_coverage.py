#!/usr/bin/env python3
"""Generate or verify complete public-function coverage in the module guides."""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parent.parent
INCLUDE_DIRECTORY = REPOSITORY_ROOT / "include"

MODULE_GUIDES = {
    "almanac.h": "almanac.md",
    "array.h": "array.md",
    "bitset.h": "bitset.md",
    "datetime.h": "datetime.md",
    "dictionary.h": "dictionary.md",
    "diffequation.h": "diffequation.md",
    "equation.h": "equation.md",
    "expression.h": "expression.md",
    "integrator.h": "integrator.md",
    "json.h": "json.md",
    "jurisdiction.h": "jurisdiction.md",
    "matrix.h": "matrix.md",
    "number.h": "number.md",
    "qcomplex.h": "qcomplex.md",
    "qfloat.h": "qfloat.md",
    "set.h": "set.md",
    "sqlite.h": "sqlite.md",
    "timeseries.h": "timeseries.md",
    "ustring.h": "string.md",
}


@dataclass(frozen=True)
class PublicFunction:
    """One public function declaration extracted from an installed header."""

    header: str
    name: str
    declaration: str


def remove_comments_and_directives(source: str) -> str:
    """Remove comments and preprocessor directives while preserving token separation."""
    source = re.sub(r"/\*.*?\*/", " ", source, flags=re.DOTALL)
    source = re.sub(r"//[^\n]*", " ", source)
    source = re.sub(r"^[ \t]*#(?:[^\n]|\\\n)*", "", source, flags=re.MULTILINE)
    return source


def normalise_declaration(declaration: str) -> str:
    """Collapse declaration whitespace without changing C tokens."""
    return re.sub(r"\s+", " ", declaration).strip()


def declaration_function_name(declaration: str) -> str | None:
    """Return the declared function name, excluding typedefs and function-pointer objects."""
    declaration = normalise_declaration(declaration)
    if not declaration or re.match(r"^(?:typedef|_Static_assert)\b", declaration):
        return None
    if "(*" in declaration.split("(", 1)[0]:
        return None
    matches = list(re.finditer(r"\b([A-Za-z_]\w*)\s*\(", declaration))
    if not matches:
        return None
    excluded = {"if", "for", "while", "switch", "sizeof", "_Alignof", "__attribute__"}
    for match in matches:
        if match.group(1) not in excluded:
            return match.group(1)
    return None


def public_functions_from_header(path: Path) -> list[PublicFunction]:
    """Extract top-level prototypes and public static-inline definitions from one header."""
    source = remove_comments_and_directives(path.read_text(encoding="utf-8"))
    functions: list[PublicFunction] = []
    statement: list[str] = []
    brace_depth = 0

    def record(candidate: str, definition: bool = False) -> None:
        candidate = normalise_declaration(candidate)
        name = declaration_function_name(candidate)
        if not name:
            return
        declaration = candidate.rstrip(" ;") + ";" if definition else candidate
        functions.append(PublicFunction(path.name, name, declaration))

    for character in source:
        if brace_depth:
            if character == "{":
                brace_depth += 1
            elif character == "}":
                brace_depth -= 1
                if brace_depth == 0:
                    statement.clear()
            continue
        if character == "{":
            candidate = "".join(statement)
            if ")" in candidate and not re.match(r"^\s*typedef\b", candidate):
                record(candidate, definition=True)
            statement.clear()
            brace_depth = 1
            continue
        statement.append(character)
        if character == ";":
            record("".join(statement))
            statement.clear()

    unique: dict[str, PublicFunction] = {}
    for function in functions:
        unique.setdefault(function.name, function)
    return sorted(unique.values(), key=lambda function: function.name)


def public_functions() -> list[PublicFunction]:
    """Return every public function declared by the installed MARS headers."""
    functions: list[PublicFunction] = []
    for path in sorted(INCLUDE_DIRECTORY.glob("*.h")):
        functions.extend(public_functions_from_header(path))
    return functions


def purpose_for(function: PublicFunction) -> str:
    """Return a concise index description derived from the stable public spelling."""
    words = function.name.split("_")
    if len(words) > 1:
        words.pop(0)
    operation = " ".join(words) if words else function.name
    destructive_suffixes = ("clear", "close", "dealloc", "destroy", "free")
    constructor_words = {"alloc", "clone", "create", "deserialise", "from", "new", "open", "parse"}
    predicate_words = {"can", "contains", "has", "is", "matches", "valid"}

    if any(function.name.endswith(f"_{suffix}") for suffix in destructive_suffixes):
        return f"Releases or clears the resources associated with {operation}."
    if any(word in constructor_words for word in words[:2]):
        return f"Creates or reconstructs the public value described by {operation}."
    if any(word in predicate_words for word in words[:2]) or function.declaration.startswith("bool "):
        return f"Reports whether the condition described by {operation} holds."
    if function.declaration.startswith("void "):
        return f"Performs the public operation described by {operation}."
    return f"Returns the public result described by {operation}."


def function_is_mentioned(function: PublicFunction, text: str) -> bool:
    """Return whether a guide names a public function unambiguously."""
    return re.search(rf"(?<![A-Za-z0-9_]){re.escape(function.name)}(?![A-Za-z0-9_])", text) is not None


def main() -> int:
    """Verify that each module guide covers every function in its public header."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args()
    functions = public_functions()
    missing: list[PublicFunction] = []
    for function in functions:
        guide_name = MODULE_GUIDES.get(function.header)
        if not guide_name:
            print(f"no Markdown module guide is assigned to {function.header}", file=sys.stderr)
            return 1
        text = (REPOSITORY_ROOT / "docs" / guide_name).read_text(encoding="utf-8")
        if not function_is_mentioned(function, text):
            missing.append(function)
    if missing:
        print("Markdown public API coverage is incomplete:", file=sys.stderr)
        for function in missing:
            print(f"  {function.header}: {function.name}()", file=sys.stderr)
        return 1
    print(f"Markdown module guides cover {len(functions)} public functions")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
