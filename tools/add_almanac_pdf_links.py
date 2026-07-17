#!/usr/bin/env python3
"""Add internal links to page-number references in the assembled almanac PDF."""

from __future__ import annotations

import argparse
import re
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path

import pikepdf
from lxml import etree


FRONT_LABELS = [
    "vii",
    "viii",
    "ix",
    "x",
    "xi",
    "xii",
    "xiii",
    "xiv",
    "xv",
    "xvi",
    "xvii",
    "xviii",
    "xix",
    "xxi",
    "xxii",
    "xxiii",
    "xxv",
    "xxvi",
    "xxvii",
    "xxviii",
    "xxix",
]
OMITTED_ASSEMBLY_PAGES = {324}
LINK_NAME_PREFIX = "almanac-page-link-"
NUMBER_TOKEN = re.compile(r"^[\d\s,;\-–—().]+$")
NUMBER = re.compile(r"\d{1,3}")


@dataclass(frozen=True)
class Word:
    text: str
    x0: float
    y0: float
    x1: float
    y1: float


@dataclass(frozen=True)
class Link:
    source: int
    target: int
    rect: tuple[float, float, float, float]
    label: str
    category: str


def page_number(path: Path) -> int:
    return int(path.stem.split("-")[1])


def body_page_map(page_dir: Path, pdf_page_count: int) -> dict[int, int]:
    front_count = len(FRONT_LABELS)
    numbered = sorted(
        (page_number(path) for path in page_dir.glob("page-[0-9][0-9][0-9].pdf"))
    )
    retained = [number for number in numbered if number not in OMITTED_ASSEMBLY_PAGES]
    expected = pdf_page_count - front_count
    if len(retained) != expected:
        raise RuntimeError(
            f"assembled PDF has {expected} body pages, but {len(retained)} retained "
            "source pages were found"
        )
    return {number: front_count + offset for offset, number in enumerate(retained)}


def extract_words(pdf_path: Path) -> list[list[list[Word]]]:
    with tempfile.NamedTemporaryFile(suffix=".html") as output:
        subprocess.run(
            ["pdftotext", "-bbox-layout", str(pdf_path), output.name], check=True
        )
        parser = etree.XMLParser(recover=True, huge_tree=True)
        document = etree.parse(output.name, parser)

    pages: list[list[list[Word]]] = []
    for page in document.xpath('//*[local-name()="page"]'):
        lines: list[list[Word]] = []
        for line in page.xpath('.//*[local-name()="line"]'):
            words = []
            for element in line.xpath('./*[local-name()="word"]'):
                text = "".join(element.itertext()).strip()
                if not text:
                    continue
                words.append(
                    Word(
                        text=text,
                        x0=float(element.attrib["xMin"]),
                        y0=float(element.attrib["yMin"]),
                        x1=float(element.attrib["xMax"]),
                        y1=float(element.attrib["yMax"]),
                    )
                )
            if words:
                lines.append(sorted(words, key=lambda word: word.x0))
        pages.append(lines)
    return pages


def number_links_for_word(
    word: Word,
    source: int,
    body_map: dict[int, int],
    category: str,
) -> list[Link]:
    if not NUMBER_TOKEN.fullmatch(word.text):
        return []

    links = []
    width = max(word.x1 - word.x0, 1.0)
    text_length = max(len(word.text), 1)
    for match in NUMBER.finditer(word.text):
        number = int(match.group())
        target = body_map.get(number)
        if target is None:
            continue
        x0 = word.x0 + width * match.start() / text_length
        x1 = word.x0 + width * match.end() / text_length
        links.append(
            Link(
                source=source,
                target=target,
                rect=(x0, word.y0, x1, word.y1),
                label=str(number),
                category=category,
            )
        )
    return links


def collect_links(
    pages: list[list[list[Word]]], body_map: dict[int, int]
) -> list[Link]:
    links: list[Link] = []
    front_map = {label: index for index, label in enumerate(FRONT_LABELS)}

    # Contents pages vii-xvi.
    for source in range(0, 10):
        for line in pages[source]:
            for word in line:
                if word.y0 < 50 or word.x0 < 420:
                    continue
                roman_target = front_map.get(word.text.lower().strip(".,;()"))
                if roman_target is not None:
                    links.append(
                        Link(
                            source,
                            roman_target,
                            (word.x0, word.y0, word.x1, word.y1),
                            word.text,
                            "contents",
                        )
                    )
                else:
                    links.extend(
                        number_links_for_word(word, source, body_map, "contents")
                    )

    # Lists of figures and tables, xvii-xix and xxi-xxiii.
    for source in range(10, 16):
        for line in pages[source]:
            for word in line:
                if word.y0 >= 50 and word.x0 >= 520:
                    links.extend(
                        number_links_for_word(word, source, body_map, "front lists")
                    )

    # Index pages 741-752. Numeric-only tokens are page references here.
    for printed_page in range(741, 753):
        source = body_map.get(printed_page)
        if source is None:
            continue
        for line in pages[source]:
            for word in line:
                if 50 <= word.y0 <= 810:
                    links.extend(number_links_for_word(word, source, body_map, "index"))

    # Explicit internal references written as "page 123" or "pages 123-125".
    index_sources = {body_map[number] for number in range(741, 753) if number in body_map}
    for source in range(len(FRONT_LABELS), len(pages)):
        if source in index_sources:
            continue
        for line in pages[source]:
            line_terms = {
                word.text.lower().strip(".,;:()")
                for word in line
            }
            # Avoid linking citations to pages in other books. The book's explicit
            # internal references identify the local table, figure, section, etc.
            if not line_terms.intersection(
                {"table", "figure", "section", "chapter", "equation", "see"}
            ):
                continue
            for position, word in enumerate(line):
                if word.text.lower().strip(".,;:()") not in {"page", "pages"}:
                    continue
                for following in line[position + 1 : position + 7]:
                    normalized = following.text.lower().strip(".,;:()")
                    if normalized in {"and", "to", "through", "or"}:
                        continue
                    found = number_links_for_word(
                        following, source, body_map, "body reference"
                    )
                    if not found:
                        break
                    links.extend(found)

    unique: dict[tuple[int, int, tuple[float, float, float, float]], Link] = {}
    for link in links:
        unique[(link.source, link.target, link.rect)] = link
    return list(unique.values())


def remove_generated_links(pdf: pikepdf.Pdf) -> None:
    for page in pdf.pages:
        annotations = page.obj.get("/Annots")
        if annotations is None:
            continue
        retained = []
        for annotation in annotations:
            name = str(annotation.get("/NM", ""))
            if not name.startswith(LINK_NAME_PREFIX):
                retained.append(annotation)
        page.obj["/Annots"] = pikepdf.Array(retained)


def insert_links(pdf: pikepdf.Pdf, links: list[Link]) -> None:
    for sequence, link in enumerate(links, 1):
        source_page = pdf.pages[link.source]
        target_page = pdf.pages[link.target]
        height = float(source_page.obj.MediaBox[3])
        target_top = float(target_page.obj.MediaBox[3])
        x0, y0, x1, y1 = link.rect
        rectangle = pikepdf.Array(
            [x0 - 1.2, height - y1 - 1.2, x1 + 1.2, height - y0 + 1.2]
        )
        annotation = pikepdf.Dictionary(
            Type=pikepdf.Name("/Annot"),
            Subtype=pikepdf.Name("/Link"),
            Rect=rectangle,
            Border=pikepdf.Array([0, 0, 0]),
            H=pikepdf.Name("/I"),
            Dest=pikepdf.Array(
                [target_page.obj, pikepdf.Name("/FitH"), target_top]
            ),
            NM=pikepdf.String(f"{LINK_NAME_PREFIX}{sequence}"),
            Contents=pikepdf.String(f"Go to page {link.label}"),
        )
        annotations = source_page.obj.get("/Annots")
        if annotations is None:
            annotations = pikepdf.Array()
            source_page.obj["/Annots"] = annotations
        annotations.append(pdf.make_indirect(annotation))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("pdf", type=Path)
    parser.add_argument("page_dir", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    source = args.pdf.resolve()
    destination = (args.output or args.pdf).resolve()
    with pikepdf.Pdf.open(source) as pdf:
        body_map = body_page_map(args.page_dir.resolve(), len(pdf.pages))
        words = extract_words(source)
        if len(words) != len(pdf.pages):
            raise RuntimeError(
                f"text extraction returned {len(words)} pages for a {len(pdf.pages)}-page PDF"
            )
        links = collect_links(words, body_map)
        remove_generated_links(pdf)
        insert_links(pdf, links)

        if destination == source:
            temporary = destination.with_name(f".{destination.name}.links.tmp")
        else:
            temporary = destination
        pdf.save(temporary, object_stream_mode=pikepdf.ObjectStreamMode.preserve)
        if destination == source:
            temporary.replace(destination)

    categories: dict[str, int] = {}
    for link in links:
        categories[link.category] = categories.get(link.category, 0) + 1
    print(f"links added: {len(links)}")
    for category, count in sorted(categories.items()):
        print(f"{category}: {count}")


if __name__ == "__main__":
    main()
