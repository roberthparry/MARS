#!/usr/bin/env python3
"""Repair corrupted numbered equations in the almanac Markdown from TeX sources."""

from __future__ import annotations

import argparse
import re
from dataclasses import dataclass
from pathlib import Path


TAG = re.compile(r"\\tag\{([^}]+)\}")
DISPLAY_MATH = re.compile(r"(?ms)^[ \t]*\{?\$\$(.*?)\$\$[ \t]*$")
TEX_ENV = re.compile(
    r"\\begin\{(equation\*?|align\*?)\}(.*?)\\end\{\1\}", re.DOTALL
)
TEX_BRACKET_MATH = re.compile(r"\\\[(.*?)\\\]", re.DOTALL)
BAD_MATH = re.compile(
    r"\\begin\{aligned\}(?:ed|edat)"
    r"|\\frac[A-Za-z]"
    r"|\\sqrt[A-Za-z]"
    r"|\\over\b"
    r"|\x08egin"
    r"|\\begin\{bmatrix\}\s*\\begin\{bmatrix\}"
)


@dataclass(frozen=True)
class SourceEquation:
    environment: str
    body: str


def normalize_tag(tag: str) -> str:
    return re.sub(r"\s+", "", tag).replace("--", "-").replace("–", "-")


def strip_comments(text: str) -> str:
    return re.sub(r"(?m)(?<!\\)%.*$", "", text)


def matching_brace(text: str, start: int) -> int | None:
    depth = 0
    for position in range(start, len(text)):
        if text[position] == "{" and (position == 0 or text[position - 1] != "\\"):
            depth += 1
        elif text[position] == "}" and (
            position == 0 or text[position - 1] != "\\"
        ):
            depth -= 1
            if depth == 0:
                return position
    return None


def top_level_over(text: str) -> re.Match[str] | None:
    depth = 0
    position = 0
    while position < len(text):
        character = text[position]
        if character == "{" and (position == 0 or text[position - 1] != "\\"):
            depth += 1
        elif character == "}" and (position == 0 or text[position - 1] != "\\"):
            depth -= 1
        elif depth == 0 and text.startswith(r"\over", position):
            end = position + len(r"\over")
            if end == len(text) or not text[end].isalpha():
                return re.match(r"\\over\b", text[position:])
        position += 1
    return None


def convert_over_fractions(text: str) -> str:
    output: list[str] = []
    position = 0
    while position < len(text):
        if text[position] != "{" or (position and text[position - 1] == "\\"):
            output.append(text[position])
            position += 1
            continue
        end = matching_brace(text, position)
        if end is None:
            output.append(text[position])
            position += 1
            continue
        inner = convert_over_fractions(text[position + 1 : end])
        match = top_level_over(inner)
        if match:
            marker = inner.find(r"\over")
            numerator = inner[:marker].strip()
            denominator = inner[marker + len(r"\over") :].strip()
            output.append(rf"\frac{{{numerator}}}{{{denominator}}}")
        else:
            output.append("{" + inner + "}")
        position = end + 1
    return "".join(output)


def katex_body(source: SourceEquation, tag: str) -> str:
    body = strip_comments(source.body)
    tags = TAG.findall(body)
    if len(tags) > 1:
        body = TAG.sub(lambda match: rf"&& \text{{({match.group(1)})}}", body)
    else:
        body = TAG.sub("", body)
    body = re.sub(r"\\(?:notag|nonumber)\b", "", body)
    body = re.sub(r"\\label\{[^}]*\}", "", body)
    body = re.sub(r"\\hspace\*?\{[^}]*\}", "", body)
    body = re.sub(r"\\(?:vspace|kern)\*?\{[^}]*\}", "", body)
    body = body.replace(r"\hbox", r"\text")
    body = body.replace(r"\bm", r"\mathbf")
    body = re.sub(r"\\(?:vect|vct|matr)\{([^{}]*)\}", r"\\mathbf{\1}", body)
    body = body.replace(r"\rvec", r"\mathbf{r}")
    body = convert_over_fractions(body)
    body = re.sub(r"\\\\\[[^]]*\]", r"\\", body)
    body = re.sub(r"[ \t]+\n", "\n", body)
    body = re.sub(r"\n{3,}", "\n\n", body).strip()

    if source.environment.startswith("align"):
        body = "\\begin{aligned}\n" + body + "\n\\end{aligned}"
    if len(tags) > 1:
        return body
    return f"{body}\n\\tag{{{tag.replace('--', '–')}}}"


def source_equations(tex_dir: Path) -> dict[str, SourceEquation]:
    equations: dict[str, SourceEquation] = {}
    for path in sorted(tex_dir.glob("page-*.tex")):
        text = path.read_text(encoding="utf-8", errors="replace")
        blocks = list(TEX_ENV.finditer(text))
        blocks.extend(TEX_BRACKET_MATH.finditer(text))
        for match in blocks:
            if match.re is TEX_ENV:
                environment, body = match.group(1), match.group(2)
            else:
                environment, body = "equation*", match.group(1)
            for tag_match in TAG.finditer(body):
                key = normalize_tag(tag_match.group(1))
                if key in equations:
                    raise RuntimeError(f"duplicate equation tag {tag_match.group(1)}")
                equations[key] = SourceEquation(environment, body)
    return equations


def repair(
    markdown: str,
    equations: dict[str, SourceEquation],
    repair_lines: set[int] | None = None,
) -> tuple[str, int]:
    repaired = 0

    def replace(match: re.Match[str]) -> str:
        nonlocal repaired
        body = match.group(1)
        tag_match = TAG.search(body)
        line = markdown.count("\n", 0, match.start()) + 1
        selected = repair_lines is not None and line in repair_lines
        if tag_match is None:
            if not selected or "&" not in body or r"\begin{" in body:
                return match.group(0)
            rows = re.split(r"\n[ \t]*\n+", body.strip())
            body = "\\\\\n".join(row.strip() for row in rows if row.strip())
            repaired += 1
            return "$$\n\\begin{aligned}\n" + body + "\n\\end{aligned}\n$$"
        if not selected and BAD_MATH.search(body) is None:
            return match.group(0)
        key = normalize_tag(tag_match.group(1))
        source = equations.get(key)
        if source is None:
            raise RuntimeError(f"no TeX source found for equation {tag_match.group(1)}")
        repaired += 1
        return "$$\n" + katex_body(source, tag_match.group(1)) + "\n$$"

    return DISPLAY_MATH.sub(replace, markdown), repaired


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("markdown", type=Path)
    parser.add_argument("tex_dir", type=Path)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--errors-file", type=Path)
    args = parser.parse_args()

    markdown = args.markdown.read_text(encoding="utf-8")
    repair_lines = None
    if args.errors_file:
        repair_lines = {
            int(match.group(1))
            for match in re.finditer(
                r"^page [^,]+, line (\d+):", args.errors_file.read_text(), re.M
            )
        }
    repaired, count = repair(markdown, source_equations(args.tex_dir), repair_lines)
    print(f"numbered equations repaired: {count}")
    if not args.check and repaired != markdown:
        args.markdown.write_text(repaired, encoding="utf-8")


if __name__ == "__main__":
    main()
