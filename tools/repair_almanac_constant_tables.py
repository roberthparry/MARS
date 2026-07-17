#!/usr/bin/env python3
"""Replace the corrupted Table 15.1 conversion with native Markdown tables."""

from __future__ import annotations

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MARKDOWN = ROOT / "docs/Explanatory Supplement to the Astronomical Almanac.md"
TEX = ROOT / "src/almanac/Explanatory Supplement Astronomical Almanac TeX/tex"


def brace_argument(text: str, position: int) -> tuple[str, int]:
    while position < len(text) and text[position].isspace():
        position += 1
    if position >= len(text) or text[position] != "{":
        raise ValueError(f"expected brace at {position}")
    depth = 1
    start = position + 1
    position += 1
    while position < len(text) and depth:
        if text[position] == "{" and text[position - 1] != "\\":
            depth += 1
        elif text[position] == "}" and text[position - 1] != "\\":
            depth -= 1
        position += 1
    if depth:
        raise ValueError("unclosed brace argument")
    return text[start : position - 1], position


def unwrap_command(text: str, command: str) -> str:
    marker = "\\" + command
    while marker in text:
        start = text.index(marker)
        argument, end = brace_argument(text, start + len(marker))
        text = text[:start] + argument + text[end:]
    return text


def clean_cell(text: str) -> str:
    text = re.sub(r"\\hspace\*?\{[^{}]*\}", "&nbsp;&nbsp;", text)
    for command in ("ind", "subind"):
        text = unwrap_command(text, command)
    text = text.replace(r"\{e\}", "e")
    text = re.sub(r"\s+", " ", text).strip()
    return text.replace("|", r"\|")


def source_table(page: int) -> str:
    source = (TEX / f"page-{page}.tex").read_text(encoding="utf-8")
    begin = source.index(r"\begin{tabular}")
    body = source[begin : source.index(r"\end{tabular}", begin)]
    rows: list[list[str]] = []
    position = 0
    token = re.compile(r"\\(?:constrow|sectrow|multicolumn\{5\}\{c\}\{\\textbf)")
    while match := token.search(body, position):
        command = match.group(0)
        position = match.end()
        if command == r"\constrow":
            cells = []
            for _ in range(5):
                cell, position = brace_argument(body, position)
                cells.append(clean_cell(cell))
            rows.append(cells)
        elif command == r"\sectrow":
            heading, position = brace_argument(body, position)
            rows.append([f"**{clean_cell(heading)}**", "", "", "", ""])
        else:
            heading, position = brace_argument(body, position)
            if position < len(body) and body[position] == "}":
                position += 1
            rows.append([f"**{clean_cell(heading)}**", "", "", "", ""])

    output = [
        "| Quantity | Symbol | Value | Units | Relative uncertainty (ppm) |",
        "|:--|:--:|:--|:--|:--:|",
    ]
    output.extend("| " + " | ".join(row) + " |" for row in rows)
    return "\n".join(output)


def replace_page(markdown: str, page: int, replacement: str) -> str:
    start_marker = f"<!-- page-{page} -->"
    end_marker = f"<!-- page-{page + 1} -->"
    start = markdown.index(start_marker)
    end = markdown.index(end_marker, start)
    return markdown[:start] + replacement.rstrip() + "\n\n\n" + markdown[end:]


def main() -> None:
    markdown = MARKDOWN.read_text(encoding="utf-8")
    page_693 = f"""<!-- page-693 -->

# 15 Reference Data

**Table 15.1**  
Fundamental Constants (1986 Recommended Values)

{source_table(693)}

*Table 15.1 is continued on next page.*"""
    page_694 = f"""<!-- page-694 -->

694 &nbsp;&nbsp;&nbsp; **EXPLANATORY SUPPLEMENT**

**Table 15.1, continued**  
Fundamental Constants (1986 Recommended Values)

{source_table(694)}"""
    page_695 = f"""<!-- page-695 -->

**Table 15.1, continued**  
Fundamental Constants (1986 Recommended Values)

{source_table(695)}

Note: The digits in parentheses are the one standard-deviation uncertainty in the last digits of the given value."""

    for page, replacement in ((693, page_693), (694, page_694), (695, page_695)):
        markdown = replace_page(markdown, page, replacement)
    MARKDOWN.write_text(markdown, encoding="utf-8")


if __name__ == "__main__":
    main()
