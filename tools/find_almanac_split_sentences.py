#!/usr/bin/env python3
"""Find likely mid-sentence paragraph breaks in the almanac Markdown."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


CONTINUATION_WORDS = re.compile(
    r"\b(?:and|or|of|to|the|a|an|in|on|for|with|from|by|as|that|which|"
    r"where|when|if|than|into|between|through|at)$",
    re.IGNORECASE,
)
LOWERCASE_START = re.compile(r"^(?:[a-z]|[;,.)]|\([a-z])")
STANDALONE_BOLD = re.compile(r"^\*\*[^*]+\*\*$")
FENCE = chr(96) * 3
SKIP_PREFIXES = (
    "#",
    "|",
    "<!--",
    "![",
    FENCE,
    "$$",
    "---",
    "—",
    "{",
    "}",
    "\\",
    "- ",
    "* ",
    "> ",
)


def candidates(
    markdown: str, *, strict: bool = False
) -> list[tuple[int, int, str, str, str]]:
    lines = markdown.splitlines()
    results: list[tuple[int, int, str, str, str]] = []
    in_fence = False
    in_display = False
    seen: set[tuple[int, int]] = set()

    for index, line in enumerate(lines):
        if FENCE in line:
            in_fence = not in_fence
        if in_fence:
            continue
        if line.strip() == "$$":
            in_display = not in_display
            continue
        if in_display or line.strip():
            continue

        before = index - 1
        while before >= 0 and not lines[before].strip():
            before -= 1
        after = index + 1
        while after < len(lines) and not lines[after].strip():
            after += 1
        if before < 0 or after >= len(lines):
            continue

        previous = lines[before].strip()
        following = lines[after].strip()
        pair = (before, after)
        if pair in seen:
            continue
        seen.add(pair)
        if previous.startswith(SKIP_PREFIXES) or following.startswith(SKIP_PREFIXES):
            continue
        if STANDALONE_BOLD.fullmatch(previous):
            continue

        lowercase = bool(LOWERCASE_START.match(following))
        hyphen = bool(re.search(r"[A-Za-z]-$", previous))
        continuation_word = bool(CONTINUATION_WORDS.search(previous))
        if not (lowercase or hyphen or continuation_word):
            continue
        if strict:
            markup = ("&", r"\end", r"\begin", "</", FENCE)
            prose_continuation = bool(re.match(r"^[a-z]", following))
            proper_name_continuation = bool(
                continuation_word and re.match(r"^(?:[A-Z]|\*[A-Z])", following)
            )
            if (
                not (prose_continuation or proper_name_continuation)
                or len(previous) < 60
                or previous.endswith((".", "?", "!"))
                or previous.endswith("}")
                or re.fullmatch(r"[ivxlcdm]+", following, re.IGNORECASE)
                or any(token in previous or token in following for token in markup)
            ):
                continue

        reason = "lowercase" if lowercase else "hyphen" if hyphen else "word"
        results.append((before + 1, after + 1, reason, previous, following))

    return results


def page_boundary_candidates(markdown: str) -> list[tuple[int, str, str, str]]:
    """Report page markers preceded by prose that does not end a sentence."""
    lines = markdown.splitlines()
    results: list[tuple[int, str, str, str]] = []
    marker = re.compile(r"<!-- page-([^ ]+) -->")

    for index, line in enumerate(lines):
        match = marker.search(line)
        if not match or line.strip() != match.group(0):
            continue
        before = index - 1
        while before >= 0 and not lines[before].strip():
            before -= 1
        after = index + 1
        while after < len(lines) and not lines[after].strip():
            after += 1
        if before < 0 or after >= len(lines):
            continue
        previous = lines[before].strip()
        following = lines[after].strip()
        if previous.startswith(SKIP_PREFIXES) or previous.endswith((".", "!", "?", ":", ";")):
            continue
        results.append((index + 1, match.group(1), previous, following))
    return results


def safe_page_joins(markdown: str) -> tuple[str, list[tuple[int, str, str, str]]]:
    """Join unambiguous prose continuations separated only by a page marker."""
    lines = markdown.splitlines()
    joins: list[tuple[int, int, int, str, str, str]] = []
    marker = re.compile(r"<!-- page-([^ ]+) -->")

    for index, line in enumerate(lines):
        match = marker.fullmatch(line.strip())
        if not match:
            continue
        if match.group(1).isdigit() and int(match.group(1)) >= 741:
            continue
        before = index - 1
        while before >= 0 and not lines[before].strip():
            before -= 1
        after = index + 1
        while after < len(lines) and not lines[after].strip():
            after += 1
        if before < 0 or after >= len(lines):
            continue
        previous = lines[before].strip()
        following = lines[after].strip()
        plain_following = following.removeprefix("{}").lstrip()
        semantic_previous = previous.rstrip("*_}").rstrip()
        continuation_signal = bool(
            LOWERCASE_START.match(plain_following)
            or CONTINUATION_WORDS.search(previous)
            or re.search(r"[,;:-]$", semantic_previous)
        )
        short_heading = bool(
            len(previous) < 50
            and previous[:1].isupper()
            and not re.search(r"[.,;:$]", previous)
        )
        if (
            len(previous) < 12
            or not continuation_signal
            or short_heading
            or previous.startswith(SKIP_PREFIXES)
            or plain_following.startswith(SKIP_PREFIXES)
            or plain_following.startswith(("**", "Table ", "Figure "))
            or semantic_previous.endswith((".", "!", "?", ":", ";"))
            or previous.endswith((FENCE, "}"))
            or "EXPLANATORY SUPPLEMENT" in previous
            or "EXPLANATORY SUPPLEMENT" in plain_following
            or re.match(r"^\d+\s*/\s*[A-Z]", plain_following)
            or re.fullmatch(r"[ivxlcdm]+|\d+", previous, re.IGNORECASE)
            or re.fullmatch(r"[ivxlcdm]+|\d+", plain_following, re.IGNORECASE)
            or re.match(r"^[IVXLCDM]+\.\s", plain_following)
            or not re.match(r"[A-Za-z0-9*($'\"“]", plain_following)
        ):
            continue
        joins.append((before, index, after, match.group(1), previous, plain_following))

    report = [(index + 1, page, previous, following) for _, index, _, page, previous, following in joins]
    for before, _, after, page, previous, following in reversed(joins):
        if re.search(r"[A-Za-z]-$", previous) and re.match(r"[a-z]", following):
            lines[before] = f"{previous[:-1]}<!-- page-{page} -->{following}"
        else:
            lines[before] = f"{previous} <!-- page-{page} --> {following}"
        del lines[before + 1 : after + 1]
    return "\n".join(lines) + "\n", report


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("markdown", type=Path)
    parser.add_argument("--strict", action="store_true")
    parser.add_argument("--page-boundaries", action="store_true")
    parser.add_argument("--safe-page-joins", action="store_true")
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()

    markdown = args.markdown.read_text(encoding="utf-8")
    if args.safe_page_joins:
        updated, joins = safe_page_joins(markdown)
        for line, page, previous, following in joins:
            print(f"{line} [page-{page}] {previous} || {following}")
        print(f"JOINS {len(joins)}")
        if args.write:
            args.markdown.write_text(updated, encoding="utf-8")
        return 0
    if args.page_boundaries:
        found_pages = page_boundary_candidates(markdown)
        for line, page, previous, following in found_pages:
            print(f"{line} [page-{page}] {previous} || {following}")
        print(f"CANDIDATES {len(found_pages)}")
        return 0

    found = candidates(
        markdown, strict=args.strict
    )
    for before, after, reason, previous, following in found:
        print(f"{before}->{after} [{reason}] {previous} || {following}")
    print(f"CANDIDATES {len(found)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
