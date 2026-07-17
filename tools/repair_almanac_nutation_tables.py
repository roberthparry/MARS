from pathlib import Path
import re


PATH = Path("docs/Explanatory Supplement to the Astronomical Almanac.md")
TEX_DIR = Path("src/almanac/Explanatory Supplement Astronomical Almanac TeX/tex")


def page_slice(source: str, page: str, next_page: str) -> tuple[int, int]:
    start = source.index(f"<!-- page-{page} -->")
    end = source.index(f"<!-- page-{next_page} -->", start)
    return start, end


def fenced_table(source: str, page: str, next_page: str, title: str) -> tuple[int, int, str]:
    page_start, page_end = page_slice(source, page, next_page)
    start = source.index(title, page_start, page_end)
    fence_start = source.index("```latex", start, page_end)
    fence_end = source.index("```", fence_start + len("```latex"), page_end) + 3
    return start, fence_end, source[fence_start + len("```latex") : fence_end - 3]


def rows(block: str, field_count: int, expected: range) -> list[list[str]]:
    parsed: list[list[str]] = []
    for line in block.splitlines():
        line = re.sub(r"\\+(?:\[[^]]*\])?\s*$", "", line.strip()).strip()
        if not line or not line[0].isdigit() or "&" not in line:
            continue
        fields = [field.strip() for field in line.split("&")]
        if len(fields) != field_count:
            continue
        parsed.append(fields)
    numbers = [int(row[0]) for row in parsed]
    assert numbers == list(expected), (numbers[:5], numbers[-5:], expected)
    return parsed


def clean(cell: str) -> str:
    if cell == r"\blank":
        return ""
    return cell.replace(r"\T", "T")


def markdown_table(headers: list[str], data: list[list[str]]) -> str:
    lines = [
        "| " + " | ".join(headers) + " |",
        "|" + "|".join(["---:"] * len(headers)) + "|",
    ]
    lines.extend("| " + " | ".join(clean(cell) for cell in row) + " |" for row in data)
    return "\n".join(lines)


def replace(source: str, page: str, next_page: str, title: str, replacement: str) -> str:
    start, end, _ = fenced_table(source, page, next_page, title)
    return source[:start] + replacement + source[end:]


source = PATH.read_text(encoding="utf-8")

start, end, block_112 = fenced_table(source, "112", "113", "**Table 3.222.1**")
data_112 = rows((TEX_DIR / "page-112.tex").read_text(encoding="utf-8"), 11, range(1, 56))
table_112 = []
for row in data_112:
    table_112.append(row)
source = source[:start] + (
    "**Table 3.222.1**  \n"
    "Nutation in Longitude and Obliquity Referred to the Mean Ecliptic of Date\n\n"
    + markdown_table(
        ["No.", "$l$", "$l'$", "$F$", "$D$", "$\\Omega$", "Period (days)", "$S_i$", "$S_i(T)$", "$C_i$", "$C_i(T)$"],
        table_112,
    )
) + source[end:]

start, end, block_113 = fenced_table(source, "113", "114", "**Table 3.222.1, continued**")
data_113_raw = rows((TEX_DIR / "page-113.tex").read_text(encoding="utf-8"), 9, range(56, 107))
data_113 = [row[:8] + ["", row[8], ""] for row in data_113_raw]
source = source[:start] + (
    "**Table 3.222.1, continued**  \n"
    "Nutation in Longitude and Obliquity Referred to the Mean Ecliptic of Date\n\n"
    + markdown_table(
        ["No.", "$l$", "$l'$", "$F$", "$D$", "$\\Omega$", "Period (days)", "$S_i$", "$S_i(T)$", "$C_i$", "$C_i(T)$"],
        data_113,
    )
) + source[end:]

start, end, block_116 = fenced_table(source, "116", "117", "**Table 3.224.1**")
data_116 = rows((TEX_DIR / "page-116.tex").read_text(encoding="utf-8"), 11, range(1, 5))
source = source[:start] + (
    "**Table 3.224.1**  \n"
    "Corrections to IAU 1980 Nutation Series\n\n"
    + markdown_table(
        ["No.", "$l$", "$l'$", "$F$", "$D$", "$\\Omega$", "Period (days)", "$LS_n$", "$LC_n$", "$OC_n$", "$OS_n$"],
        data_116,
    )
) + source[end:]

planet_headers = [
    "No.", "$l$", "$F$", "$D$", "$\\Omega$", "$Q$", "$V$", "$E$", "$M$", "$J$", "$S$",
    "Period (days)", "$LS_n$", "$LC_n$", "$OC_n$", "$OS_n$",
]

start, end, block_118 = fenced_table(source, "118", "119", "**Table 3.224.2**")
data_118 = rows((TEX_DIR / "page-118.tex").read_text(encoding="utf-8"), 16, range(1, 46))
source = source[:start] + (
    "**Table 3.224.2**  \n"
    "Planetary Terms in Nutation, Combined Direct and Indirect Effects\n\n"
    + markdown_table(planet_headers, data_118)
) + source[end:]

page_start, page_end = page_slice(source, "119", "121")
start = source.index("**Table 3.224.2, continued**", page_start, page_end)
end = source.index("$$\n\\mathbf{N} =", start, page_end)
data_119 = rows((TEX_DIR / "page-119.tex").read_text(encoding="utf-8"), 16, range(46, 86))
source = source[:start] + (
    "**Table 3.224.2, continued**  \n"
    "Planetary Terms in Nutation, Combined Direct and Indirect Effects\n\n"
    + markdown_table(planet_headers, data_119)
    + "\n\n"
) + source[end:]

PATH.write_text(source, encoding="utf-8")
