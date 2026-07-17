#!/usr/bin/env python3
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent / "Explanatory Supplement Astronomical Almanac TeX"
SOURCE_PDF = Path(__file__).resolve().parent / "Explanatory Supplement Astronomical Almanac.pdf"
PDF_DIR = ROOT / "pdf"
TEX_DIR = ROOT / "tex"
OCR_DIR = ROOT / "ocr"
ORIGINALS_DIR = ROOT / "images" / "originals"

CHAPTER_9_TITLE = "9 / ASTRONOMICAL PHENOMENA"
CHAPTER_10_TITLE = "10 / STARS AND STELLAR SYSTEMS"
CHAPTER_11_TITLE = "11 / MATHEMATICAL TECHNIQUES"
CHAPTER_12_TITLE = "12 / CALENDARS"
CHAPTER_13_TITLE = "13 / HISTORICAL INFORMATION"
CHAPTER_14_TITLE = "14 / RELATED PUBLICATIONS"
CHAPTER_15_TITLE = "15 / REFERENCE DATA"
FULL_IMAGE_PAGES = {667}


def run(cmd: list[str], cwd: Path | None = None) -> None:
    subprocess.run(cmd, check=True, cwd=cwd)


def capture_text(cmd: list[str], cwd: Path | None = None) -> str:
    return subprocess.run(
        cmd, check=True, text=True, capture_output=True, cwd=cwd, encoding="utf-8", errors="replace"
    ).stdout


def run_result(cmd: list[str], cwd: Path | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        cmd, cwd=cwd, text=True, capture_output=True, encoding="utf-8", errors="replace"
    )


def normalize_text(text: str) -> str:
    replacements = {
        "\u2018": "'",
        "\u2019": "'",
        "\u201c": '"',
        "\u201d": '"',
        "\u2013": "--",
        "\u2014": "---",
        "\u2212": "-",
        "\u00a0": " ",
        "\ufb01": "fi",
        "\ufb02": "fl",
    }
    for src, dst in replacements.items():
        text = text.replace(src, dst)
    word_join_fixes = {
        "Mercuryand": "Mercury and",
        "Marsand": "Mars and",
        "NauticalAlmanac": "Nautical Almanac",
        "Semidiametersat": "Semidiameters at",
        "MinorPlanets": "Minor Planets",
        "Apparentobliquity": "Apparent obliquity",
        "Siderealtime": "Sidereal time",
    }
    for src, dst in word_join_fixes.items():
        text = text.replace(src, dst)
    text = text.replace("\r\n", "\n").replace("\r", "\n")
    return text


def strip_headers(text: str, page: int) -> str:
    lines = [line.rstrip() for line in normalize_text(text).splitlines()]
    filtered: list[str] = []
    seen_body = False
    for line in lines:
        raw = line.strip()
        if not raw:
            filtered.append("")
            continue
        leading = len(line) - len(line.lstrip(" "))
        if seen_body and leading > 58 and len(raw) <= 2:
            continue
        compact = re.sub(r"\s+", " ", raw)
        upper = compact.upper()
        if raw == "\x0c":
            continue
        if not seen_body and (
            str(page) in raw
            or "EXPLANATORY" in upper
            or "SUPPLEMENT" in upper
            or "ECLIPSES" in upper
            or "SUN AND MOON" in upper
            or "ASTRONOMICAL" in upper
            or "PHENOMENA" in upper
            or "STARS" in upper
            or "STELLAR" in upper
            or "SYSTEMS" in upper
            or "CALENDARS" in upper
            or "HISTORICAL" in upper
            or "INFORMATION" in upper
        ):
            continue
        seen_body = True
        filtered.append(line)
    body = "\n".join(filtered).replace("\x0c", "")
    body = re.sub(r"([A-Za-z])-\n([a-z])", r"\1\2", body)
    body = re.sub(r"\n{3,}", "\n\n", body).strip()
    return body


def tex_escape(text: str) -> str:
    mapping = {
        "\\": r"\textbackslash{}",
        "{": r"\{",
        "}": r"\}",
        "%": r"\%",
        "&": r"\&",
        "#": r"\#",
        "_": r"\_",
        "$": r"\$",
        "^": r"\^{}",
        "~": r"\~{}",
    }
    return "".join(mapping.get(ch, ch) for ch in text)


def looks_like_equation_block(block: str) -> bool:
    lines = [line.strip() for line in block.splitlines() if line.strip()]
    if not lines:
        return False
    joined = " ".join(lines)
    short_lines = sum(len(line) <= 42 for line in lines)
    if any(looks_like_authority_line(line) for line in lines):
        return False
    if max(len(line) for line in lines) > 72:
        return False
    if any("(8." in line for line in lines) and short_lines >= max(1, len(lines) - 1):
        return True
    if sum(ch in joined for ch in "=+-/*^") >= 2 and short_lines >= max(1, len(lines) - 1):
        return True
    if len(lines) >= 2 and sum(any(c.isdigit() for c in line) for line in lines) >= 2 and short_lines == len(lines):
        return True
    return False


def looks_like_authority_line(line: str) -> bool:
    return bool(
        re.match(r"""^["']?\d{4}[A-Za-z]?(?:\s*[-–—]\s*\d{4}[A-Za-z]?)?\s*:""", line.strip())
    )


def looks_like_authority_block(block: str) -> bool:
    lines = [line.strip() for line in block.splitlines() if line.strip()]
    return bool(lines) and any(looks_like_authority_line(line) for line in lines)


def render_authority_block(block: str) -> str:
    lines = [line.strip() for line in block.splitlines() if line.strip()]
    rendered: list[str] = []
    for line in lines:
        rendered.append(r"\noindent\hangindent=2.2em\hangafter=1 " + tex_escape(line) + r"\par")
    return "\n".join(rendered) + "\n"


def split_equation_tag(line: str) -> tuple[str, str | None]:
    match = re.search(r"(\((?:8|9|10|11|12|13)\.[0-9A-Za-z.,:-]+\))\s*$", line)
    if not match:
        return line, None
    content = line[: match.start()].rstrip(" ,;")
    return content, match.group(1)


def render_heading(block: str) -> str | None:
    single = " ".join(part.strip() for part in block.splitlines() if part.strip())
    match = re.match(r"^((?:8|9|10|11|12|13)\.\d{1,4})\.?\s+([A-Z][A-Za-z' -]+?)(?:\s{2,}|\s+)(.+)$", single)
    if match:
        sec, title, rest = match.groups()
        return (
            rf"\sectionrunin{{{tex_escape(sec)}}}{{{tex_escape(title)}}}"
            rf"{tex_escape(rest)}" "\n"
        )
    match = re.match(r"^((?:8|9|10|11|12|13)\.\d{1,4})\.?\s+(.+)$", single)
    if match and len(single) < 90:
        sec, title = match.groups()
        return rf"\sectionline{{{tex_escape(sec)} \quad {tex_escape(title)}}}" "\n"
    return None


def render_equation_block(block: str) -> str:
    lines = [line.strip() for line in block.splitlines() if line.strip()]
    rendered: list[str] = [r"\begin{center}"]
    tag: str | None = None
    visible_lines: list[str] = []
    for line in lines:
        content, maybe_tag = split_equation_tag(line)
        if maybe_tag:
            tag = maybe_tag
        if content:
            visible_lines.append(content)
    for idx, line in enumerate(visible_lines):
        suffix = r"\\" if idx != len(visible_lines) - 1 else ""
        rendered.append(tex_escape(line) + suffix)
    if tag:
        rendered.append(r"\vspace{0.25em}")
        rendered.append(tex_escape(tag))
    rendered.append(r"\end{center}")
    return "\n".join(rendered) + "\n"


def render_paragraph(block: str) -> str:
    text = " ".join(part.strip() for part in block.splitlines() if part.strip())
    prefix = r"\noindent " if text and not text[0].isupper() else ""
    return prefix + tex_escape(text) + "\n"


def render_blocks(body: str) -> str:
    blocks = [part.strip("\n") for part in re.split(r"\n\s*\n", body) if part.strip()]
    rendered: list[str] = []
    for block in blocks:
        heading = render_heading(block)
        if heading:
            rendered.append(heading)
        elif looks_like_authority_block(block):
            rendered.append(render_authority_block(block))
        elif looks_like_equation_block(block):
            rendered.append(render_equation_block(block))
        else:
            rendered.append(render_paragraph(block))
    return "\n".join(rendered).strip() + "\n"


def tex_escape_layout_token(text: str) -> str:
    return tex_escape(text).replace("--", r"\textendash{}")


def render_preserved_line(line: str) -> str:
    parts: list[str] = []
    pos = 0
    for match in re.finditer(r" {2,}", line):
        if match.start() > pos:
            parts.append(tex_escape_layout_token(line[pos : match.start()]))
        width = len(match.group(0)) * 0.29
        parts.append(rf"\hspace*{{{width:.2f}em}}")
        pos = match.end()
    if pos < len(line):
        parts.append(tex_escape_layout_token(line[pos:]))
    return "".join(parts)


def render_layout_preserved(body: str, page: int) -> str:
    """Render dense table/index pages as TeX while preserving source layout."""
    cleaned = strip_headers(body, page)
    cleaned = re.sub(r"([A-Za-z])-\n([a-z])", r"\1\2", cleaned)
    lines = [line.rstrip() for line in cleaned.splitlines()]
    rendered: list[str] = [r"\raggedright", r"\setlength{\parskip}{0pt}"]
    blank_count = 0
    for line in lines:
        stripped = line.strip()
        if not stripped:
            blank_count += 1
            if blank_count <= 1:
                rendered.append(r"\vspace{0.24em}")
            continue
        blank_count = 0
        leading = len(line) - len(line.lstrip(" "))
        if leading > 58 and len(stripped) <= 2:
            continue
        if page < 722 and len(stripped) == 1 and leading > 20:
            continue
        if page < 722 and len(stripped) <= 9 and re.search(r"[-_!.]", stripped):
            continue
        rendered.append(r"\noindent " + render_preserved_line(line) + r"\par")
    return "\n".join(rendered).strip() + "\n"


def render_glossary_preserved(body: str, page: int) -> str:
    """Render glossary pages as readable TeX rather than preserving OCR drift."""
    cleaned = strip_headers(body, page)
    cleaned = re.sub(r"([A-Za-z])-\n([a-z])", r"\1\2", cleaned)
    rendered: list[str] = [r"\raggedright", r"\setlength{\parskip}{0pt}"]
    blank_count = 0
    for raw_line in cleaned.splitlines():
        stripped = raw_line.strip()
        if not stripped:
            blank_count += 1
            if blank_count <= 1:
                rendered.append(r"\vspace{0.35em}")
            continue
        if stripped == str(page) or stripped == str(page).replace("9", "g"):
            continue
        if page > 721 and stripped.upper() == "GLOSSARY":
            continue
        blank_count = 0
        leading = len(raw_line) - len(raw_line.lstrip(" "))
        prefix = r"\noindent "
        if leading > 0:
            prefix = r"\noindent\hspace*{1.65em}"
        rendered.append(prefix + tex_escape_layout_token(stripped) + r"\par")
    return "\n".join(rendered).strip() + "\n"


def split_index_line(line: str) -> tuple[str, str]:
    match = re.search(r" {8,}", line)
    if match:
        return line[: match.start()].rstrip(), line[match.end() :].rstrip()
    leading = len(line) - len(line.lstrip(" "))
    if leading > 28:
        return "", line.strip()
    return line.rstrip(), ""


def render_index_preserved(body: str, page: int) -> str:
    cleaned = strip_headers(body, page)
    rows: list[tuple[str, str]] = []
    for raw_line in cleaned.splitlines():
        line = raw_line.rstrip()
        if not line.strip():
            rows.append(("", ""))
            continue
        left, right = split_index_line(line)
        rows.append((left, right))

    rendered: list[str] = [r"\raggedright", r"\renewcommand{\arraystretch}{0.96}", r"\begin{tabular*}{\textwidth}{@{}p{0.47\textwidth}@{\extracolsep{\fill}}p{0.47\textwidth}@{}}"]
    for left, right in rows:
        if not left and not right:
            rendered.append(r"\multicolumn{2}{@{}l@{}}{\vspace{0.15em}}\\")
            continue
        left_tex = render_preserved_line(left) if left else ""
        right_tex = render_preserved_line(right) if right else ""
        rendered.append(left_tex + " & " + right_tex + r"\\")
    rendered.append(r"\end{tabular*}")
    return "\n".join(rendered) + "\n"


def clean_index_column(text: str, page: int) -> str:
    lines: list[str] = []
    for line in strip_headers(text, page).splitlines():
        line = re.sub(r"^(\s*),\s*", r"\1", line)
        stripped = line.strip()
        if stripped in {"INDEX", str(page)}:
            continue
        if re.fullmatch(r"[,0-9 ]{1,3},?", stripped):
            continue
        lines.append(line.rstrip())
    return "\n".join(lines).strip()


def extract_source_index_columns(page: int) -> tuple[str, str]:
    spread, side = source_spread_for_page(page)
    page_x = 0 if side == "left" else 420
    left = capture_text(
        [
            "pdftotext",
            "-f",
            str(spread),
            "-l",
            str(spread),
            "-layout",
            "-x",
            str(page_x),
            "-y",
            "0",
            "-W",
            "210",
            "-H",
            "612",
            str(SOURCE_PDF),
            "-",
        ]
    )
    right = capture_text(
        [
            "pdftotext",
            "-f",
            str(spread),
            "-l",
            str(spread),
            "-layout",
            "-x",
            str(page_x + 210),
            "-y",
            "0",
            "-W",
            "211",
            "-H",
            "612",
            str(SOURCE_PDF),
            "-",
        ]
    )
    return clean_index_column(left, page), clean_index_column(right, page)


def render_index_column(column: str) -> str:
    rendered: list[str] = [r"\raggedright"]
    blank_count = 0
    for line in column.splitlines():
        stripped = line.strip()
        if not stripped:
            blank_count += 1
            if blank_count <= 1:
                rendered.append(r"\vspace{0.18em}")
            continue
        blank_count = 0
        rendered.append(r"\noindent " + render_preserved_line(line.rstrip()) + r"\par")
    return "\n".join(rendered)


def render_index_columns(left: str, right: str) -> str:
    return (
        r"\noindent\begin{minipage}[t]{0.47\textwidth}"
        "\n"
        + render_index_column(left)
        + "\n"
        + r"\end{minipage}\hfill\begin{minipage}[t]{0.47\textwidth}"
        "\n"
        + render_index_column(right)
        + "\n"
        + r"\end{minipage}"
        "\n"
    )


def chapter_title(page: int) -> str:
    if page >= 743:
        return "INDEX"
    if page <= 503:
        return CHAPTER_9_TITLE
    if page <= 540:
        return CHAPTER_10_TITLE
    if page <= 574:
        return CHAPTER_11_TITLE
    if page <= 608:
        return CHAPTER_12_TITLE
    if page <= 666:
        return CHAPTER_13_TITLE
    if page <= 692:
        return CHAPTER_14_TITLE
    return CHAPTER_15_TITLE


def page_header(page: int) -> str:
    if page in FULL_IMAGE_PAGES:
        return ""
    if page % 2 == 0:
        return rf"\bookhead{{{page}}}{{EXPLANATORY SUPPLEMENT}}"
    return rf"\runningtitle{{{chapter_title(page)}}}{{{page}}}"


def tex_document(page: int, body: str) -> str:
    body_font = r"\marsbodyfont"
    if page >= 695:
        body_font = r"\fontsize{11.4}{12.3}\selectfont"
    if page >= 699:
        body_font = r"\fontsize{9.8}{10.7}\selectfont"
    if 702 <= page <= 719:
        body_font = r"\fontsize{10.9}{11.8}\selectfont"
    if 721 <= page <= 740:
        body_font = r"\fontsize{12.4}{14.0}\selectfont"
    if page >= 722:
        body_font = r"\fontsize{10.8}{11.8}\selectfont"
    if 722 <= page <= 740:
        body_font = r"\fontsize{12.4}{14.0}\selectfont"
    if page >= 743:
        body_font = r"\fontsize{9.7}{10.5}\selectfont"
    return f"""\\documentclass[12pt]{{article}}
\\usepackage[a4paper,top=10mm,bottom=10mm,left=12mm,right=12mm]{{geometry}}
\\usepackage[T1]{{fontenc}}
\\usepackage[utf8]{{inputenc}}
\\usepackage{{amsmath}}
\\usepackage{{array}}
\\usepackage{{graphicx}}
\\usepackage{{mathptmx}}
\\usepackage{{microtype}}
\\usepackage{{ragged2e}}
\\usepackage{{xcolor}}
\\usepackage{{pagecolor}}
\\input{{page-style.tex}}

\\definecolor{{marsbg}}{{HTML}}{{022318}}
\\definecolor{{marstext}}{{HTML}}{{E7EEE6}}
\\definecolor{{marsmuted}}{{HTML}}{{B9C9BE}}
\\pagecolor{{marsbg}}
\\color{{marstext}}
\\pagestyle{{empty}}
\\setlength{{\\parindent}}{{1.2em}}
\\setlength{{\\parskip}}{{0pt}}
\\setlength{{\\emergencystretch}}{{3em}}
\\setlength{{\\abovedisplayskip}}{{0.55em}}
\\setlength{{\\belowdisplayskip}}{{0.55em}}
\\newcommand{{\\bookhead}}[2]{{\\thispagestyle{{empty}}\\noindent\\textcolor{{marsmuted}}{{#1}}\\hfill\\textcolor{{marsmuted}}{{\\textsc{{#2}}}}\\par\\vspace{{1.05em}}}}
\\newcommand{{\\runningtitle}}[2]{{\\thispagestyle{{empty}}\\noindent{{\\textbf{{#1}}}}\\hfill\\textcolor{{marsmuted}}{{#2}}\\par\\vspace{{1.05em}}}}
\\newcommand{{\\sectionline}}[1]{{\\par\\vspace{{0.55em}}\\noindent\\textbf{{#1}}\\par}}
\\newcommand{{\\sectionrunin}}[2]{{\\par\\vspace{{0.55em}}\\noindent\\textbf{{#1}}\\hspace{{0.8em}}\\textbf{{#2}}\\hspace{{0.7em}}}}
\\newcommand{{\\origpage}}[1]{{\\newpage\\thispagestyle{{empty}}\\pagecolor{{marsbg}}\\noindent\\makebox[\\textwidth][c]{{\\includegraphics[height=0.94\\paperheight,keepaspectratio]{{../images/originals/page-#1.png}}}}}}

\\begin{{document}}
\\fontsize{{13.6}}{{16.2}}\\selectfont
{page_header(page)}
{body_font}
\\justifying

{body}
\\origpage{{{page}}}
\\end{{document}}
"""


def source_spread_for_page(page: int) -> tuple[int, str]:
    if page % 2 == 0:
        return (page + 30) // 2, "left"
    return (page + 29) // 2, "right"


def extract_source_crop_text(page: int) -> str:
    spread, side = source_spread_for_page(page)
    x = "0" if side == "left" else "420"
    raw = capture_text(
        [
            "pdftotext",
            "-f",
            str(spread),
            "-l",
            str(spread),
            "-layout",
            "-x",
            x,
            "-y",
            "0",
            "-W",
            "421",
            "-H",
            "612",
            str(SOURCE_PDF),
            "-",
        ]
    )
    return strip_headers(raw, page)


def special_page_body(page: int) -> str | None:
    if page in FULL_IMAGE_PAGES:
        return (
            rf"\noindent\makebox[\textwidth][c]{{"
            rf"\includegraphics[height=0.96\textheight,keepaspectratio]{{../images/page-{page}-full-themed.png}}"
            "}\n"
        )
    if page in {654, 655}:
        return (
            rf"\noindent\makebox[\textwidth][c]{{"
            rf"\includegraphics[height=0.86\paperheight,keepaspectratio]{{../images/page-{page}-table-themed.png}}"
            "}\n"
        )
    return None


def ensure_original_png(page: int) -> None:
    target = ORIGINALS_DIR / f"page-{page}.png"
    if target.exists():
        return
    prefix = ORIGINALS_DIR / f"page-{page}"
    run(
        [
            "pdftoppm",
            "-singlefile",
            "-f",
            "1",
            "-l",
            "1",
            "-png",
            "-r",
            "180",
            str(PDF_DIR / f"page-{page}.pdf"),
            str(prefix),
        ]
    )


def extract_ocr(page: int) -> str:
    out_path = OCR_DIR / f"book-page-{page}.txt"
    image_ocr_path = OCR_DIR / f"image-page-{page}.txt"
    if 721 <= page <= 739 and (ORIGINALS_DIR / f"page-{page}.png").exists():
        if image_ocr_path.exists() and image_ocr_path.stat().st_size > 0:
            return strip_headers(image_ocr_path.read_text(encoding="utf-8"), page)
        raw = capture_text(
            [
                "tesseract",
                str(ORIGINALS_DIR / f"page-{page}.png"),
                "stdout",
                "-l",
                "eng",
                "--psm",
                "4",
            ]
        )
        cleaned = strip_headers(raw, page)
        image_ocr_path.write_text(cleaned + "\n", encoding="utf-8")
        return cleaned
    if page >= 695:
        source_ocr_path = OCR_DIR / f"source-page-{page}.txt"
        if source_ocr_path.exists() and source_ocr_path.stat().st_size > 0:
            return strip_headers(source_ocr_path.read_text(encoding="utf-8"), page)
        cleaned = extract_source_crop_text(page)
        if cleaned:
            source_ocr_path.write_text(cleaned + "\n", encoding="utf-8")
            return cleaned
    if page >= 595 and (ORIGINALS_DIR / f"page-{page}.png").exists():
        if image_ocr_path.exists() and image_ocr_path.stat().st_size > 0:
            return strip_headers(image_ocr_path.read_text(encoding="utf-8"), page)
        raw = capture_text(
            [
                "tesseract",
                str(ORIGINALS_DIR / f"page-{page}.png"),
                "stdout",
                "-l",
                "eng",
                "--psm",
                "4",
            ]
        )
        cleaned = strip_headers(raw, page)
        image_ocr_path.write_text(cleaned + "\n", encoding="utf-8")
        return cleaned
    if out_path.exists():
        return out_path.read_text(encoding="utf-8").strip()
    text = capture_text(["pdftotext", "-layout", str(PDF_DIR / f"page-{page}.pdf"), "-"])
    cleaned = strip_headers(text, page)
    out_path.write_text(cleaned + "\n", encoding="utf-8")
    return cleaned


def build_page(page: int) -> None:
    ensure_original_png(page)
    special = special_page_body(page)
    if special:
        body = special
    elif page >= 743:
        body = render_index_columns(*extract_source_index_columns(page))
    elif 721 <= page <= 740:
        body = render_glossary_preserved(extract_ocr(page), page)
    elif page >= 699:
        body = render_layout_preserved(extract_ocr(page), page)
    else:
        body = render_blocks(extract_ocr(page))
    tex_path = TEX_DIR / f"page-{page}.tex"
    tex_path.write_text(tex_document(page, body), encoding="utf-8")
    result = run_result(
        [
            "pdflatex",
            "-interaction=nonstopmode",
            tex_path.name,
        ],
        cwd=TEX_DIR,
    )
    built_pdf = TEX_DIR / f"page-{page}.pdf"
    if result.returncode != 0 and not built_pdf.exists():
        raise RuntimeError(f"pdflatex failed for page {page}:\n{result.stdout}\n{result.stderr}")
    final_pdf = PDF_DIR / f"page-{page}.pdf"
    final_pdf.write_bytes(built_pdf.read_bytes())


def main(argv: list[str]) -> int:
    if len(argv) != 3:
        print(f"usage: {argv[0]} START END", file=sys.stderr)
        return 2
    start = int(argv[1])
    end = int(argv[2])
    for page in range(start, end + 1):
        build_page(page)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
