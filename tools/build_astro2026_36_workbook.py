#!/usr/bin/env python3
"""Build a refreshed Astro workbook for the 2026-2036 navigation range."""

from __future__ import annotations

import datetime as dt
import math
import os
from pathlib import Path
import sqlite3
import tempfile
import xml.etree.ElementTree as ET
import zipfile

import numpy as np

from repair_astro2002_workbook import repackage_ods


ODS_NS = {
    "office": "urn:oasis:names:tc:opendocument:xmlns:office:1.0",
    "table": "urn:oasis:names:tc:opendocument:xmlns:table:1.0",
    "text": "urn:oasis:names:tc:opendocument:xmlns:text:1.0",
}

BODY_ROWS = {
    "Mercury": 5,
    "Venus": 8,
    "Earth": 11,
    "Mars": 14,
    "Jupiter": 17,
    "Saturn": 20,
}

BODY_CODES = {
    "Mercury": "MERCURY",
    "Venus": "VENUS",
    "Earth": "EARTH",
    "Mars": "MARS",
    "Jupiter": "JUPITER",
    "Saturn": "SATURN",
}

WINDOWS = [
    (dt.date(2026, 1, 1), dt.date(2032, 1, 10)),
    (dt.date(2032, 1, 10), dt.date(2038, 1, 18)),
    (dt.date(2038, 1, 18), dt.date(2044, 1, 27)),
    (dt.date(2044, 1, 27), dt.date(2050, 2, 4)),
]

J2000 = 2451545.0


def jd_from_date(value: dt.date) -> float:
    a = (14 - value.month) // 12
    y = value.year + 4800 - a
    m = value.month + 12 * a - 3
    jdn = value.day + ((153 * m + 2) // 5) + 365 * y + y // 4 - y // 100 + y // 400 - 32045
    return float(jdn) - 0.5


def format_workbook_date(value: dt.date) -> str:
    return value.strftime("%d-%b-%y")


def load_model_db() -> sqlite3.Connection:
    sql = Path("packaging/almanac-db/mars_almanac.sql").read_text()
    db_path = "/tmp/astro2026_36_workbook.db"
    try:
        os.remove(db_path)
    except FileNotFoundError:
        pass
    con = sqlite3.connect(db_path)
    con.executescript(sql)
    return con


def eval_segment(seg: sqlite3.Row, jd: float) -> dict[str, float]:
    x = (jd - seg["reference_jd"]) / seg["span_days"]

    def poly(prefix: str) -> float:
        return seg[f"{prefix}_c0"] + seg[f"{prefix}_c1"] * x + seg[f"{prefix}_c2"] * x * x

    return {
        "a": poly("a"),
        "e": poly("e"),
        "i": poly("i"),
        "L": poly("L"),
        "varpi": poly("varpi"),
        "Omega": poly("Omega"),
    }


def unwrap_degrees(values: np.ndarray) -> np.ndarray:
    return np.degrees(np.unwrap(np.radians(values)))


def fit_window(con: sqlite3.Connection, body_code: str, start_jd: float) -> dict[str, np.ndarray]:
    con.row_factory = sqlite3.Row
    cur = con.cursor()
    sample_jds = np.linspace(start_jd, start_jd + 2200.0, 97)
    p = (sample_jds - start_jd) / 2200.0
    samples = {"a": [], "e": [], "i": [], "L": [], "varpi": [], "Omega": []}

    for jd in sample_jds:
        seg = cur.execute(
            """
            SELECT *
            FROM almanac_orbital_elements_model
            WHERE body_code = ? AND start_jd <= ? AND end_jd >= ?
            ORDER BY start_jd ASC
            LIMIT 1
            """,
            (body_code, float(jd), float(jd)),
        ).fetchone()
        if seg is None:
            raise RuntimeError(f"no orbital segment found for {body_code} at JD {jd}")
        values = eval_segment(seg, float(jd))
        for key, value in values.items():
            samples[key].append(value)

    coeffs: dict[str, np.ndarray] = {}
    for key, raw_values in samples.items():
        values = np.array(raw_values, dtype=float)
        if key in {"L", "varpi", "Omega"}:
            values = unwrap_degrees(values)
        poly = np.polyfit(p, values, 2)
        coeff = np.array([poly[2], poly[1], poly[0]], dtype=float)
        if key in {"L", "varpi", "Omega"}:
            coeff[0] = coeff[0] % 360.0
        coeffs[key] = coeff
    return coeffs


def cell_text(cell: ET.Element) -> str:
    parts = []
    for p in cell.findall(".//text:p", ODS_NS):
        parts.append("".join(p.itertext()))
    return "".join(parts)


def set_cell_string(cell: ET.Element, text_value: str) -> None:
    cell.attrib.pop(f"{{{ODS_NS['office']}}}value", None)
    cell.attrib.pop(f"{{{ODS_NS['office']}}}date-value", None)
    cell.attrib.pop(f"{{{ODS_NS['office']}}}time-value", None)
    cell.attrib[f"{{{ODS_NS['office']}}}value-type"] = "string"
    p = cell.find("text:p", ODS_NS)
    if p is None:
        p = ET.SubElement(cell, f"{{{ODS_NS['text']}}}p")
    p.text = text_value


def set_cell_date(cell: ET.Element, value: dt.date) -> None:
    cell.attrib.pop(f"{{{ODS_NS['office']}}}value", None)
    cell.attrib.pop(f"{{{ODS_NS['office']}}}time-value", None)
    cell.attrib[f"{{{ODS_NS['office']}}}value-type"] = "date"
    cell.attrib[f"{{{ODS_NS['office']}}}date-value"] = value.isoformat()
    p = cell.find("text:p", ODS_NS)
    if p is None:
        p = ET.SubElement(cell, f"{{{ODS_NS['text']}}}p")
    p.text = format_workbook_date(value)


def set_cell_number(cell: ET.Element, value: float, text_value: str | None = None) -> None:
    cell.attrib.pop(f"{{{ODS_NS['office']}}}date-value", None)
    cell.attrib.pop(f"{{{ODS_NS['office']}}}time-value", None)
    cell.attrib[f"{{{ODS_NS['office']}}}value-type"] = "float"
    cell.attrib[f"{{{ODS_NS['office']}}}value"] = f"{value:.10f}".rstrip("0").rstrip(".")
    p = cell.find("text:p", ODS_NS)
    if p is None:
        p = ET.SubElement(cell, f"{{{ODS_NS['text']}}}p")
    p.text = text_value if text_value is not None else str(value)


def expanded_cells(row: ET.Element) -> list[ET.Element]:
    cells: list[ET.Element] = []
    for child in row:
        if child.tag not in {
            f"{{{ODS_NS['table']}}}table-cell",
            f"{{{ODS_NS['table']}}}covered-table-cell",
        }:
            continue
        repeat = int(child.attrib.get(f"{{{ODS_NS['table']}}}number-columns-repeated", "1"))
        cells.extend([child] * repeat)
    return cells


def row_at(rows: list[ET.Element], row_number: int) -> ET.Element:
    return rows[row_number - 1]


def update_window_cells(rows: list[ET.Element], row_base: int, col_base: int,
                        start_date: dt.date, end_date: dt.date, coeffs_by_body: dict[str, dict[str, np.ndarray]]) -> None:
    header_row = expanded_cells(row_at(rows, row_base))
    jd_row = expanded_cells(row_at(rows, row_base + 1))
    set_cell_date(header_row[col_base + 1], start_date)
    set_cell_date(header_row[col_base + 5], end_date)
    set_cell_number(jd_row[col_base + 1], jd_from_date(start_date), f"{jd_from_date(start_date):.1f}")
    set_cell_number(jd_row[col_base + 5], jd_from_date(end_date), f"{jd_from_date(end_date):.1f}")

    for display_name, start_row in BODY_ROWS.items():
        coeffs = coeffs_by_body[display_name]
        r0 = expanded_cells(row_at(rows, row_base + (start_row - 5) + 3))
        r1 = expanded_cells(row_at(rows, row_base + (start_row - 5) + 4))
        r2 = expanded_cells(row_at(rows, row_base + (start_row - 5) + 5))
        set_cell_string(r0[col_base], display_name)
        for offset, key in enumerate(["a", "e", "i", "L", "varpi", "Omega"], start=1):
            set_cell_number(r0[col_base + offset], float(coeffs[key][0]), f"{coeffs[key][0]:.6f}" if key == "a" else f"{coeffs[key][0]:.5f}")
            set_cell_number(r1[col_base + offset], float(coeffs[key][1]), f"{coeffs[key][1]:.6f}")
            set_cell_number(r2[col_base + offset], float(coeffs[key][2]), f"{coeffs[key][2]:.6f}")


def update_location_defaults(root: ET.Element) -> None:
    for table in root.findall(".//table:table", ODS_NS):
        if table.attrib.get(f"{{{ODS_NS['table']}}}name") != "Location of Navigational Bodies":
            continue
        rows = table.findall("table:table-row", ODS_NS)
        row1 = expanded_cells(row_at(rows, 1))
        row2 = expanded_cells(row_at(rows, 2))
        row3 = expanded_cells(row_at(rows, 3))
        set_cell_date(row1[2], dt.date(2026, 1, 1))
        set_cell_string(row2[2], "12:00 PM")
        set_cell_string(row1[5], "0")
        set_cell_string(row2[5], "52.7077")
        set_cell_string(row3[5], "-2.7541")
        break


def main() -> None:
    source = Path("src/almanac/Astro2002.ods")
    target = Path("src/almanac/Astro2026-36.ods")
    con = load_model_db()

    with tempfile.TemporaryDirectory(prefix="astro2026-36-") as tmp:
        tmpdir = Path(tmp)
        with zipfile.ZipFile(source) as zf:
            zf.extractall(tmpdir)

        content_path = tmpdir / "content.xml"
        tree = ET.parse(content_path)
        root = tree.getroot()

        planet_table = None
        for table in root.findall(".//table:table", ODS_NS):
            if table.attrib.get(f"{{{ODS_NS['table']}}}name") == "Planet Data":
                planet_table = table
                break
        if planet_table is None:
            raise RuntimeError("Planet Data sheet not found")

        rows = planet_table.findall("table:table-row", ODS_NS)
        window_specs = [
            (2, 0, WINDOWS[0]),
            (2, 8, WINDOWS[1]),
            (24, 0, WINDOWS[2]),
            (24, 8, WINDOWS[3]),
        ]
        for row_base, col_base, (start_date, end_date) in window_specs:
            coeffs_by_body = {
                name: fit_window(con, BODY_CODES[name], jd_from_date(start_date))
                for name in BODY_CODES
            }
            update_window_cells(rows, row_base, col_base, start_date, end_date, coeffs_by_body)

        update_location_defaults(root)
        tree.write(content_path, encoding="UTF-8", xml_declaration=True)
        repackage_ods(tmpdir, target)

    con.close()


if __name__ == "__main__":
    main()
