#!/usr/bin/env python3
"""Replace the scanned ocean-loading tables with searchable Markdown tables."""

from __future__ import annotations

import argparse
from pathlib import Path


START_MARKER = "<!-- page-245 -->"
END_MARKER = "<!-- page-248 -->"
TIDES = (
    r"M_2",
    r"S_2",
    r"K_1",
    r"O_1",
    r"N_2",
    r"P_1",
    r"K_2",
    r"Q_1",
    r"M_f",
    r"M_m",
    r"S_{sa}",
)


def extract_rows(path: Path) -> list[list[str]]:
    source = path.read_text(encoding="utf-8")
    table_start = source.index(r"\begin{tabular}")
    rows_start = source.index(r"\hline", table_start) + len(r"\hline")
    table_end = source.index(r"\end{tabular}", rows_start)

    rows: list[list[str]] = []
    for line in source[rows_start:table_end].splitlines():
        line = line.strip()
        if not line.endswith(r"\\"):
            continue
        fields = [field.strip() for field in line[:-2].split("&")]
        if len(fields) != 23:
            raise ValueError(f"{path}: expected 23 fields, found {len(fields)}")
        rows.append(fields)
    return rows


def markdown_table(rows: list[list[str]]) -> list[str]:
    headers = ["**Site**"]
    for tide in TIDES:
        headers.extend((f"${tide}$ AMP", f"${tide}$ PHAS"))
    lines = ["| " + " | ".join(headers) + " |"]
    lines.append("|:---|" + "---:|" * 22)

    for fields in rows:
        fields[0] = f"**{fields[0]}**"
        lines.append("| " + " | ".join(fields) + " |")
    return lines


def build_fragment(page_rows: dict[int, list[list[str]]]) -> str:
    lines: list[str] = []
    for page in range(245, 248):
        title = "Table 4.351.1" if page == 245 else "Table 4.351.1, continued"
        lines.extend(
            [
                f"<!-- page-{page} -->",
                "",
                f"**{title}**",
                "",
                "Displacement Due to Ocean Loading "
                "(cm in amplitude and degree in phase)",
                "",
            ]
        )
        lines.extend(markdown_table(page_rows[page]))
        lines.extend(("", ""))
    return "\n".join(lines)


def replace_table(tex_directory: Path, markdown_path: Path) -> None:
    page_rows = {
        page: extract_rows(tex_directory / f"page-{page}.tex")
        for page in range(245, 248)
    }
    total_rows = sum(len(rows) for rows in page_rows.values())
    if total_rows != 96:
        raise ValueError(f"expected 96 station rows, found {total_rows}")

    source = markdown_path.read_text(encoding="utf-8")
    start = source.index(START_MARKER)
    end = source.index(END_MARKER, start)
    fragment = build_fragment(page_rows)
    markdown_path.write_text(
        source[:start] + fragment + source[end:], encoding="utf-8"
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("tex_directory", type=Path)
    parser.add_argument("markdown", type=Path)
    args = parser.parse_args()
    replace_table(args.tex_directory, args.markdown)


if __name__ == "__main__":
    main()
