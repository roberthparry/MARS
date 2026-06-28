#!/usr/bin/env python3
"""
Generate piecewise lunar-fundamentals SQL for the MARS almanac database.

The fit uses DE440 geocentric Moon vectors and the workbook lunar periodic-term
tables as a fixed series. It solves for local polynomial coefficients for the
five lunar fundamental angles over recursively-split date segments.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import math
import os
from pathlib import Path
import sys
import xml.etree.ElementTree as ET
import zipfile

try:
    import numpy as np
except ModuleNotFoundError as exc:  # pragma: no cover
    print(f"Missing Python dependency for lunar-fundamentals generation: {exc}", file=sys.stderr)
    raise SystemExit(1)

try:
    import spiceypy as sp
except ModuleNotFoundError as exc:  # pragma: no cover
    print(f"Missing SPICE dependency for lunar-fundamentals generation: {exc}", file=sys.stderr)
    raise SystemExit(1)


SPICE_KERNEL_FILENAMES = ("naif0012.tls", "pck00011.tpc", "de440.bsp")
KM_PER_AU = 149597870.7
J2000_JD = 2451545.0
GM_SUN_KM3_PER_S2 = 132712440041.93938
GM_EARTH_KM3_PER_S2 = 398600.4354360959
ODS_NS = {
    "office": "urn:oasis:names:tc:opendocument:xmlns:office:1.0",
    "table": "urn:oasis:names:tc:opendocument:xmlns:table:1.0",
    "text": "urn:oasis:names:tc:opendocument:xmlns:text:1.0",
}
ANGLE_NAMES = ("Lp", "D", "M", "Mp", "F")
DEFAULT_SEGMENT_DAYS = 3652.5
DEFAULT_MIN_DAYS = 365.25
DEFAULT_TOLERANCE_ARCSEC = 60.0
DEFAULT_TOLERANCE_KM = 15.0


@dataclass
class LunarTerms:
    global_fundamentals: dict[str, np.ndarray]
    lon_multipliers: np.ndarray
    lon_coeff_microdeg: np.ndarray
    radius_coeff_millikm: np.ndarray
    lat_multipliers: np.ndarray
    lat_coeff_microdeg: np.ndarray


@dataclass
class FittedLunarSegment:
    start_jd: float
    end_jd: float
    reference_jd: float
    span_days: float
    coeffs: dict[str, np.ndarray]
    worst_angle_arcsec: float
    worst_distance_km: float


def spice_kernel_dir(explicit: str | None) -> Path:
    if explicit:
        return Path(explicit).expanduser()
    env_value = os.environ.get("MARS_ALMANAC_KERNEL_DIR", "").strip()
    if env_value:
        return Path(env_value).expanduser()
    if (Path.home() / ".mars" / "almanac-kernels").exists():
        return Path.home() / ".mars" / "almanac-kernels"
    return Path("/tmp/mars-almanac-kernels")


def load_spice(kernel_dir: Path) -> None:
    missing = [name for name in SPICE_KERNEL_FILENAMES if not (kernel_dir / name).exists()]
    if missing:
        raise RuntimeError(f"missing SPICE kernels in {kernel_dir}: {', '.join(missing)}")
    sp.kclear()
    for name in SPICE_KERNEL_FILENAMES:
        sp.furnsh(str(kernel_dir / name))


def jd_from_gregorian(year: int, month: int, day: int) -> float:
    a = (14 - month) // 12
    y = year + 4800 - a
    m = month + 12 * a - 3
    jdn = day + ((153 * m + 2) // 5) + 365 * y + y // 4 - y // 100 + y // 400 - 32045
    return float(jdn) - 0.5


def ods_cell_text(cell: ET.Element) -> str:
    parts: list[str] = []
    for p in cell.findall(".//text:p", ODS_NS):
        parts.append(p.text or "")
    return "".join(parts)


def load_workbook_terms(path: Path) -> LunarTerms:
    with zipfile.ZipFile(path) as zf:
        root = ET.parse(zf.open("content.xml")).getroot()
    moon_table = None
    for table in root.findall(".//table:table", ODS_NS):
        if table.attrib.get(f"{{{ODS_NS['table']}}}name") == "Moon Data":
            moon_table = table
            break
    if moon_table is None:
        raise RuntimeError(f"Moon Data table not found in {path}")

    rows: list[list[str]] = []
    for row in moon_table.findall("table:table-row", ODS_NS):
        expanded: list[str] = []
        for cell in row.findall("table:table-cell", ODS_NS):
            repeat = int(cell.attrib.get(f"{{{ODS_NS['table']}}}number-columns-repeated", "1"))
            value = cell.attrib.get(f"{{{ODS_NS['office']}}}value")
            text = value if value is not None else ods_cell_text(cell)
            expanded.extend([text] * repeat)
        rows.append(expanded)

    fundamentals = {
        code: np.array([float(rows[r][c]) for r in range(2, 7)], dtype=float)
        for c, code in enumerate(ANGLE_NAMES)
    }
    lon_multipliers = np.array(
        [[int(rows[r][6]), int(rows[r][7]), int(rows[r][8]), int(rows[r][9])] for r in range(2, 62)],
        dtype=int,
    )
    lon_coeff_microdeg = np.array([float(rows[r][10]) for r in range(2, 62)], dtype=float)
    radius_coeff_millikm = np.array([float(rows[r][11]) for r in range(2, 62)], dtype=float)
    lat_multipliers = np.array(
        [[int(rows[r][13]), int(rows[r][14]), int(rows[r][15]), int(rows[r][16])] for r in range(2, 62)],
        dtype=int,
    )
    lat_coeff_microdeg = np.array([float(rows[r][17]) for r in range(2, 62)], dtype=float)
    return LunarTerms(
        global_fundamentals=fundamentals,
        lon_multipliers=lon_multipliers,
        lon_coeff_microdeg=lon_coeff_microdeg,
        radius_coeff_millikm=radius_coeff_millikm,
        lat_multipliers=lat_multipliers,
        lat_coeff_microdeg=lat_coeff_microdeg,
    )


def eval_poly4(coeffs: np.ndarray, x: np.ndarray) -> np.ndarray:
    return ((((coeffs[4] * x) + coeffs[3]) * x + coeffs[2]) * x + coeffs[1]) * x + coeffs[0]


def polyfit_local(values: np.ndarray, x: np.ndarray) -> np.ndarray:
    fitted = np.polyfit(x, values, 4)
    return np.array([fitted[4], fitted[3], fitted[2], fitted[1], fitted[0]], dtype=float)


def unwrap_degrees(values: np.ndarray) -> np.ndarray:
    return np.degrees(np.unwrap(np.radians(values)))


def earth_mean_anomaly_degrees(jd: float) -> float:
    et = sp.unitim(jd, "JDTDB", "ET")
    state, _ = sp.spkezr("EARTH", et, "ECLIPJ2000", "NONE", "SUN")
    rp, ecc, inc, lnode, argp, m0, _, _ = sp.oscelt(state, et, GM_SUN_KM3_PER_S2)
    _ = (rp, ecc, inc, lnode, argp)
    return math.degrees(m0 + math.pi)


def base_initial_coeffs(terms: LunarTerms, start_jd: float, end_jd: float) -> tuple[dict[str, np.ndarray], float, float]:
    reference_jd = 0.5 * (start_jd + end_jd)
    span_days = max(0.5 * (end_jd - start_jd), 1.0)
    sample_jds = np.linspace(start_jd, end_jd, 17)
    x = (sample_jds - reference_jd) / span_days
    coeffs: dict[str, np.ndarray] = {}

    for name in ANGLE_NAMES:
        if name == "M":
            values = np.array([earth_mean_anomaly_degrees(float(jd)) for jd in sample_jds], dtype=float)
            coeffs[name] = polyfit_local(unwrap_degrees(values), x)
            continue
        T = (sample_jds - J2000_JD) / 36525.0
        global_values = eval_poly4(terms.global_fundamentals[name], T)
        coeffs[name] = polyfit_local(global_values, x)

    return coeffs, reference_jd, span_days


def model_vectors_from_coeffs(coeffs: dict[str, np.ndarray], terms: LunarTerms, jds: np.ndarray,
                              reference_jd: float, span_days: float) -> np.ndarray:
    x = (jds - reference_jd) / span_days
    T = (jds - J2000_JD) / 36525.0
    e = 1.0 - 0.002516 * T - 0.0000074 * T * T
    abs_lon_m = np.abs(terms.lon_multipliers[:, 1]).astype(float)
    abs_lat_m = np.abs(terms.lat_multipliers[:, 1]).astype(float)

    Lp_deg = eval_poly4(coeffs["Lp"], x)
    D_deg = eval_poly4(coeffs["D"], x)
    M_deg = eval_poly4(coeffs["M"], x)
    Mp_deg = eval_poly4(coeffs["Mp"], x)
    F_deg = eval_poly4(coeffs["F"], x)

    Lp = np.radians(np.mod(Lp_deg, 360.0))
    D = np.radians(np.mod(D_deg, 360.0))
    M = np.radians(np.mod(M_deg, 360.0))
    Mp = np.radians(np.mod(Mp_deg, 360.0))
    F = np.radians(np.mod(F_deg, 360.0))
    A1 = np.radians(np.mod(119.75 + 131.849 * T, 360.0))
    A2 = np.radians(np.mod(53.09 + 479264.29 * T, 360.0))
    A3 = np.radians(np.mod(313.45 + 481266.484 * T, 360.0))

    lon_args = (
        np.outer(D, terms.lon_multipliers[:, 0])
        + np.outer(M, terms.lon_multipliers[:, 1])
        + np.outer(Mp, terms.lon_multipliers[:, 2])
        + np.outer(F, terms.lon_multipliers[:, 3])
    )
    lon_scale = e[:, None] ** abs_lon_m[None, :]
    sl = np.sum(lon_scale * terms.lon_coeff_microdeg[None, :] * np.sin(lon_args), axis=1)
    sr = np.sum(lon_scale * terms.radius_coeff_millikm[None, :] * np.cos(lon_args), axis=1)

    lat_args = (
        np.outer(D, terms.lat_multipliers[:, 0])
        + np.outer(M, terms.lat_multipliers[:, 1])
        + np.outer(Mp, terms.lat_multipliers[:, 2])
        + np.outer(F, terms.lat_multipliers[:, 3])
    )
    lat_scale = e[:, None] ** abs_lat_m[None, :]
    sb = np.sum(lat_scale * terms.lat_coeff_microdeg[None, :] * np.sin(lat_args), axis=1)

    sl += 3958.0 * np.sin(A1) + 1962.0 * np.sin(Lp - F) + 318.0 * np.sin(A2)
    sb += (
        -2235.0 * np.sin(Lp)
        + 382.0 * np.sin(A3)
        + 175.0 * np.sin(A1 - F)
        + 175.0 * np.sin(A1 + F)
        + 127.0 * np.sin(Lp - Mp)
        - 115.0 * np.sin(Lp + Mp)
    )

    lon = np.radians(np.mod(Lp_deg + sl * 1.0e-6, 360.0))
    lat = np.radians(sb * 1.0e-6)
    distance_km = 385000.56 + sr / 1000.0
    cos_lat = np.cos(lat)
    return np.column_stack((
        distance_km * cos_lat * np.cos(lon),
        distance_km * cos_lat * np.sin(lon),
        distance_km * np.sin(lat),
    ))


def exact_moon_vectors_km(jds: np.ndarray) -> np.ndarray:
    rows = []
    for jd in jds:
        et = sp.unitim(float(jd), "JDTDB", "ET")
        state, _ = sp.spkezr("MOON", et, "ECLIPJ2000", "NONE", "EARTH")
        rows.append(state[:3])
    return np.array(rows, dtype=float)


def coeff_matrix(coeffs: dict[str, np.ndarray]) -> np.ndarray:
    return np.vstack([coeffs[name] for name in ANGLE_NAMES])


def coeff_dict(matrix: np.ndarray) -> dict[str, np.ndarray]:
    return {name: np.array(matrix[idx], dtype=float) for idx, name in enumerate(ANGLE_NAMES)}


def solve_segment_coeffs(terms: LunarTerms, start_jd: float, end_jd: float,
                         fit_sample_count: int = 41, iterations: int = 8) -> tuple[dict[str, np.ndarray], float, float]:
    coeffs, reference_jd, span_days = base_initial_coeffs(terms, start_jd, end_jd)
    params = coeff_matrix(coeffs)
    optimize_mask = np.ones_like(params, dtype=bool)
    optimize_mask[2, :] = False

    jds = np.linspace(start_jd, end_jd, fit_sample_count)
    exact = exact_moon_vectors_km(jds)

    def residual_vector(matrix: np.ndarray) -> np.ndarray:
        approx = model_vectors_from_coeffs(coeff_dict(matrix), terms, jds, reference_jd, span_days)
        return (exact - approx).reshape(-1)

    residual = residual_vector(params)
    best_error = float(np.dot(residual, residual))
    damping = 1.0

    for _ in range(iterations):
        columns: list[np.ndarray] = []
        positions: list[tuple[int, int]] = []
        for row in range(params.shape[0]):
            for col in range(params.shape[1]):
                if not optimize_mask[row, col]:
                    continue
                trial = params.copy()
                step = max(1.0e-6, abs(params[row, col]) * 1.0e-8)
                trial[row, col] += step
                trial_residual = residual_vector(trial)
                columns.append((trial_residual - residual) / step)
                positions.append((row, col))
        if not columns:
            break

        jacobian = np.column_stack(columns)
        lhs = jacobian.T @ jacobian + damping * np.identity(jacobian.shape[1])
        rhs = jacobian.T @ residual
        try:
            delta = np.linalg.solve(lhs, rhs)
        except np.linalg.LinAlgError:
            break

        accepted = False
        for scale in (1.0, 0.5, 0.25, 0.125):
            trial = params.copy()
            for value, (row, col) in zip(delta, positions):
                trial[row, col] += scale * value
            trial_residual = residual_vector(trial)
            trial_error = float(np.dot(trial_residual, trial_residual))
            if trial_error < best_error:
                params = trial
                residual = trial_residual
                best_error = trial_error
                damping = max(damping * 0.5, 1.0e-6)
                accepted = True
                break
        if not accepted:
            damping = min(damping * 4.0, 1.0e8)
            if damping >= 1.0e7:
                break

    return coeff_dict(params), reference_jd, span_days


def vector_angular_error_arcsec(exact: np.ndarray, approx: np.ndarray) -> float:
    exact_unit = exact / np.linalg.norm(exact)
    approx_unit = approx / np.linalg.norm(approx)
    dot = float(np.clip(np.dot(exact_unit, approx_unit), -1.0, 1.0))
    return math.degrees(math.acos(dot)) * 3600.0


def segment_error(terms: LunarTerms, coeffs: dict[str, np.ndarray], start_jd: float, end_jd: float,
                  reference_jd: float, span_days: float) -> tuple[float, float]:
    samples = np.linspace(start_jd, end_jd, max(49, int(math.ceil((end_jd - start_jd) / 15.0)) + 1))
    exact = exact_moon_vectors_km(samples)
    approx = model_vectors_from_coeffs(coeffs, terms, samples, reference_jd, span_days)
    worst_angle = 0.0
    worst_distance = 0.0
    for idx in range(samples.shape[0]):
        worst_angle = max(worst_angle, vector_angular_error_arcsec(exact[idx], approx[idx]))
        worst_distance = max(worst_distance, float(abs(np.linalg.norm(exact[idx]) - np.linalg.norm(approx[idx]))))
    return worst_angle, worst_distance


def fit_segment(terms: LunarTerms, start_jd: float, end_jd: float) -> FittedLunarSegment:
    coeffs, reference_jd, span_days = solve_segment_coeffs(terms, start_jd, end_jd)
    worst_angle, worst_distance = segment_error(terms, coeffs, start_jd, end_jd, reference_jd, span_days)
    return FittedLunarSegment(
        start_jd=start_jd,
        end_jd=end_jd,
        reference_jd=reference_jd,
        span_days=span_days,
        coeffs=coeffs,
        worst_angle_arcsec=worst_angle,
        worst_distance_km=worst_distance,
    )


def generate_segments(terms: LunarTerms, start_jd: float, end_jd: float,
                      segment_days: float, min_days: float,
                      tolerance_arcsec: float, tolerance_km: float) -> list[FittedLunarSegment]:
    segments: list[FittedLunarSegment] = []

    def recurse(seg_start: float, seg_end: float) -> None:
        segment = fit_segment(terms, seg_start, seg_end)
        width = seg_end - seg_start
        if (
            (segment.worst_angle_arcsec <= tolerance_arcsec and segment.worst_distance_km <= tolerance_km)
            or width <= min_days + 1.0e-9
        ):
            segments.append(segment)
            return
        mid = 0.5 * (seg_start + seg_end)
        recurse(seg_start, mid)
        recurse(mid, seg_end)

    cursor = start_jd
    while cursor < end_jd - 1.0e-9:
        seg_end = min(end_jd, cursor + segment_days)
        recurse(cursor, seg_end)
        cursor = seg_end
    return segments


def sql_quote(text: str) -> str:
    return "'" + text.replace("'", "''") + "'"


def emit_segment_sql(model_id: int, segment: FittedLunarSegment) -> str:
    c = segment.coeffs
    return (
        "INSERT INTO almanac_lunar_fundamentals_model "
        "(model_id, body_code, start_jd, end_jd, reference_jd, span_days, "
        "Lp_c0, Lp_c1, Lp_c2, Lp_c3, Lp_c4, "
        "D_c0, D_c1, D_c2, D_c3, D_c4, "
        "M_c0, M_c1, M_c2, M_c3, M_c4, "
        "Mp_c0, Mp_c1, Mp_c2, Mp_c3, Mp_c4, "
        "F_c0, F_c1, F_c2, F_c3, F_c4, sort_order) "
        f"VALUES ({model_id}, 'MOON', "
        f"{segment.start_jd:.8f}, {segment.end_jd:.8f}, {segment.reference_jd:.8f}, {segment.span_days:.8f}, "
        f"{c['Lp'][0]:.15f}, {c['Lp'][1]:.15f}, {c['Lp'][2]:.15f}, {c['Lp'][3]:.15f}, {c['Lp'][4]:.15f}, "
        f"{c['D'][0]:.15f}, {c['D'][1]:.15f}, {c['D'][2]:.15f}, {c['D'][3]:.15f}, {c['D'][4]:.15f}, "
        f"{c['M'][0]:.15f}, {c['M'][1]:.15f}, {c['M'][2]:.15f}, {c['M'][3]:.15f}, {c['M'][4]:.15f}, "
        f"{c['Mp'][0]:.15f}, {c['Mp'][1]:.15f}, {c['Mp'][2]:.15f}, {c['Mp'][3]:.15f}, {c['Mp'][4]:.15f}, "
        f"{c['F'][0]:.15f}, {c['F'][1]:.15f}, {c['F'][2]:.15f}, {c['F'][3]:.15f}, {c['F'][4]:.15f}, "
        f"{model_id * 10});"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--start-year", type=int, default=1550)
    parser.add_argument("--end-year", type=int, default=2649)
    parser.add_argument("--kernel-dir", default="", help="Directory containing NAIF kernels.")
    parser.add_argument("--workbook", default="src/almanac/Astro2002.ods",
                        help="ODS workbook containing Moon Data named ranges.")
    parser.add_argument("--segment-days", type=float, default=DEFAULT_SEGMENT_DAYS)
    parser.add_argument("--min-days", type=float, default=DEFAULT_MIN_DAYS)
    parser.add_argument("--tolerance-arcsec", type=float, default=DEFAULT_TOLERANCE_ARCSEC)
    parser.add_argument("--tolerance-km", type=float, default=DEFAULT_TOLERANCE_KM)
    parser.add_argument("--fragment", action="store_true",
                        help="Emit SQL body rows only without transaction prologue.")
    parser.add_argument("--model-id-base", type=int, default=1,
                        help="First lunar fundamentals model row identifier to use in emitted SQL.")
    args = parser.parse_args()

    kernel_dir = spice_kernel_dir(args.kernel_dir.strip() or None)
    workbook = Path(args.workbook).expanduser()
    terms = load_workbook_terms(workbook)
    load_spice(kernel_dir)

    start_jd = jd_from_gregorian(args.start_year, 1, 1)
    end_jd = jd_from_gregorian(args.end_year + 1, 1, 1)
    segments = generate_segments(
        terms,
        start_jd,
        end_jd,
        args.segment_days,
        args.min_days,
        args.tolerance_arcsec,
        args.tolerance_km,
    )

    print(
        f"-- MOON: {len(segments)} segment(s), "
        f"worst angle {max(s.worst_angle_arcsec for s in segments):.3f} arcsec, "
        f"worst distance {max(s.worst_distance_km for s in segments):.3f} km",
        file=sys.stderr,
        flush=True,
    )

    if not args.fragment:
        print("BEGIN TRANSACTION;")
        print("DELETE FROM almanac_lunar_fundamentals_model;")
    next_model_id = args.model_id_base
    for segment in segments:
        print(emit_segment_sql(next_model_id, segment))
        next_model_id += 1
    if not args.fragment:
        print("COMMIT;")
    sp.kclear()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
