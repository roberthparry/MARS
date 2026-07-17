#!/usr/bin/env python3
"""Replace scanned GEM-T1 table pages with searchable Markdown tables."""

from __future__ import annotations

import argparse
from decimal import Decimal
from pathlib import Path


START_MARKER = "<!-- page-228 -->"
END_MARKER = "<!-- page-233 -->"
SCALE = Decimal("1000000")
PRECISION = Decimal("0.0000001")
MINUS = "\N{MINUS SIGN}"


def parse_coefficients(path: Path) -> dict[tuple[int, int], tuple[Decimal, Decimal]]:
    coefficients: dict[tuple[int, int], tuple[Decimal, Decimal]] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        fields = line.split()
        if not fields or fields[0] != "gfc":
            continue
        degree, order = int(fields[1]), int(fields[2])
        if 2 <= degree <= 36:
            coefficients[(degree, order)] = (Decimal(fields[3]), Decimal(fields[4]))
    return coefficients


def format_coefficient(value: Decimal) -> str:
    rendered = format((value * SCALE).quantize(PRECISION), ".7f")
    return MINUS + rendered[1:] if rendered.startswith("-") else rendered


def coefficient_table(rows: list[tuple[int, int, Decimal, Decimal]]) -> list[str]:
    lines = [
        "| $n$ | $m$ | $\\bar C$ | $\\bar S$ |",
        "|---:|---:|---:|---:|",
    ]
    for degree, order, cosine, sine in rows:
        marker = "\\*\\* " if (degree, order) == (3, 1) else ""
        lines.append(
            f"| {marker}{degree} | {order} | {format_coefficient(cosine)} | "
            f"{format_coefficient(sine)} |"
        )
    return lines


def page_for(degree: int, order: int) -> int:
    if order == 1 or order == 2 or (order == 3 and degree <= 23):
        return 228
    if order == 3 or 4 <= order <= 7 or (order == 8 and degree <= 18):
        return 229
    if order == 8 or 9 <= order <= 13 or (order == 14 and degree <= 15):
        return 230
    if order == 14 or 15 <= order <= 20 or (order == 21 and degree <= 32):
        return 231
    return 232


def build_fragment(
    coefficients: dict[tuple[int, int], tuple[Decimal, Decimal]],
) -> str:
    zonals = [
        (degree, 0, coefficients[(degree, 0)][0])
        for degree in range(2, 37)
    ]
    nonzonals = [
        (degree, order, *coefficients[(degree, order)])
        for order in range(1, 37)
        for degree in range(max(2, order), 37)
        if (degree, order) != (2, 1)
    ]
    pages = {
        page: [row for row in nonzonals if page_for(row[0], row[1]) == page]
        for page in range(228, 233)
    }

    lines = [
        START_MARKER,
        "",
        "**Table 4.32.1**",
        "",
        "GEM-T1 Normalized Coefficients ($\\times 10^6$)",
        "",
        "**Zonals**",
        "",
        "| $n$ | $m$ | **Value** |",
        "|---:|---:|---:|",
    ]
    for degree, order, value in zonals:
        marker = "\\* " if degree == 2 else ""
        lines.append(f"| {marker}{degree} | {order} | {format_coefficient(value)} |")

    lines.extend(["", "**Sectorials and Tesserals**", ""])
    lines.extend(coefficient_table(pages[228]))

    for page in range(229, 233):
        lines.extend(
            [
                "",
                f"<!-- page-{page} -->",
                "",
                "**Table 4.32.1, continued**",
                "",
                "GEM-T1 Normalized Coefficients ($\\times 10^6$)",
                "",
            ]
        )
        lines.extend(coefficient_table(pages[page]))

    lines.extend(
        [
            "",
            "\\* $\\bar C_{20}$ does not include the zero-frequency term; "
            "see Equation 4.332-5 for the adjusted value.",
            "",
            "\\*\\* $\\bar C_{21}$ and $\\bar S_{21}$ should be the IERS "
            "values; see Equations 4.32-3 and 4.32-4 for recommended values.",
            "",
            "",
        ]
    )
    return "\n".join(lines)


def replace_tables(model_path: Path, markdown_path: Path) -> None:
    coefficients = parse_coefficients(model_path)
    if len(coefficients) != 700:
        raise ValueError(f"expected 700 degree/order pairs, found {len(coefficients)}")

    source = markdown_path.read_text(encoding="utf-8")
    start = source.index(START_MARKER)
    end = source.index(END_MARKER, start)
    fragment = build_fragment(coefficients)
    markdown_path.write_text(source[:start] + fragment + source[end:], encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("model", type=Path)
    parser.add_argument("markdown", type=Path)
    args = parser.parse_args()
    replace_tables(args.model, args.markdown)


if __name__ == "__main__":
    main()
