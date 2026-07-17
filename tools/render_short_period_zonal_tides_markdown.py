#!/usr/bin/env python3
"""Render Table 4.37.1 and its argument notes as Markdown."""

from __future__ import annotations

import argparse
from pathlib import Path


START_MARKER = "<!-- page-252 -->"
END_MARKER = "<!-- page-253 -->"


def extract_rows(tex_path: Path) -> list[list[str]]:
    source = tex_path.read_text(encoding="utf-8")
    table_start = source.index(r"\begin{tabular}{rrrrr r rrr}")
    rows_start = source.index(r"\midrule", table_start) + len(r"\midrule")
    rows_end = source.index(r"\bottomrule", rows_start)

    rows: list[list[str]] = []
    for line in source[rows_start:rows_end].splitlines():
        line = line.strip()
        if not line.endswith(r"\\"):
            continue
        fields = [field.strip() for field in line[:-2].split("&")]
        if len(fields) != 9:
            raise ValueError(f"expected 9 fields, found {len(fields)}: {line}")
        rows.append(fields)
    if len(rows) != 41:
        raise ValueError(f"expected 41 rows, found {len(rows)}")
    return rows


def build_fragment(rows: list[list[str]]) -> str:
    lines = [
        START_MARKER,
        "",
        "**Table 4.37.1**",
        "",
        "Zonal Tide Terms with Periods Up to 35 Days",
        "",
        "| $l$ | $l'$ | $F$ | $D$ | $\\Omega$ | Period (days) | "
        "$\\mathrm{UT1}-\\mathrm{UT1R}$ coefficient of $\\sin(\\mathrm{argument})$ | "
        "$\\Delta-\\Delta R$ coefficient of $\\cos(\\mathrm{argument})$ | "
        "$\\omega-\\omega R$ coefficient of $\\cos(\\mathrm{argument})$ |",
        "|---:|---:|---:|---:|---:|---:|---:|---:|---:|",
    ]
    lines.extend("| " + " | ".join(row) + " |" for row in rows)
    lines.extend(
        [
            "",
            "\\* Argument definitions:",
            "",
            "- **$l$:** $134.96^{\\circ}+13.064993^{\\circ}"
            "(\\mathrm{MJD}-51544.5)$; mean anomaly of the Moon.",
            "- **$l'$:** $357.53^{\\circ}+0.985600^{\\circ}"
            "(\\mathrm{MJD}-51544.5)$; mean anomaly of the Sun.",
            "- **$F$:** $93.27^{\\circ}+13.229350^{\\circ}"
            "(\\mathrm{MJD}-51544.5)$; $L-\\Omega$, mean longitude of the Moon.",
            "- **$D$:** $297.85^{\\circ}+12.190749^{\\circ}"
            "(\\mathrm{MJD}-51544.5)$; mean elongation of the Moon from the Sun.",
            "- **$\\Omega$:** $125.04^{\\circ}-0.052954^{\\circ}"
            "(\\mathrm{MJD}-51544.5)$; mean longitude of the ascending node of the Moon.",
            "",
            "",
        ]
    )
    return "\n".join(lines)


def replace_table(tex_path: Path, markdown_path: Path) -> None:
    source = markdown_path.read_text(encoding="utf-8")
    start = source.index(START_MARKER)
    end = source.index(END_MARKER, start)
    markdown_path.write_text(
        source[:start] + build_fragment(extract_rows(tex_path)) + source[end:],
        encoding="utf-8",
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("tex", type=Path)
    parser.add_argument("markdown", type=Path)
    args = parser.parse_args()
    replace_table(args.tex, args.markdown)


if __name__ == "__main__":
    main()
