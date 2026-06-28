#!/usr/bin/env python3
"""
Generate piecewise lunar longitude/latitude/radius residual corrections for MARS.

This keeps the workbook lunar series as the base theory and fits residuals to
DE440 in ecliptic longitude, ecliptic latitude, and geocentric distance.
"""

from __future__ import annotations

import argparse
import math
import os
from pathlib import Path
import sqlite3
import sys

try:
    import numpy as np
except ModuleNotFoundError as exc:  # pragma: no cover
    print(f"Missing Python dependency for lunar correction generation: {exc}", file=sys.stderr)
    raise SystemExit(1)

try:
    import spiceypy as sp
except ModuleNotFoundError as exc:  # pragma: no cover
    print(f"Missing SPICE dependency for lunar correction generation: {exc}", file=sys.stderr)
    raise SystemExit(1)


SPICE_KERNEL_FILENAMES = ("naif0012.tls", "pck00011.tpc", "de440.bsp")
POLY_DEGREE = 7


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


def normalize_degrees(degrees: float) -> float:
    value = math.fmod(degrees, 360.0)
    if value < 0.0:
        value += 360.0
    return value


def angle_delta_degrees(target: float, base: float) -> float:
    return ((target - base + 180.0) % 360.0) - 180.0


class WorkbookMoonModel:
    def __init__(self, sql_path: Path) -> None:
        conn = sqlite3.connect(":memory:")
        conn.executescript(sql_path.read_text())
        self.fund = {
            row[0]: tuple(float(v) for v in row[1:])
            for row in conn.execute(
                "SELECT term_code, c0, c1, c2, c3, c4 "
                "FROM almanac_lunar_fundamental_coeff ORDER BY sort_order, term_code"
            )
        }
        self.lon_rows = list(conn.execute(
            "SELECT multiplier_D, multiplier_M, multiplier_Mp, multiplier_F, "
            "longitude_coeff_microdeg, radius_coeff_millikm "
            "FROM almanac_lunar_longitude_radius_term "
            "ORDER BY sort_order ASC, term_id ASC"
        ))
        self.lat_rows = list(conn.execute(
            "SELECT multiplier_D, multiplier_M, multiplier_Mp, multiplier_F, latitude_coeff_microdeg "
            "FROM almanac_lunar_latitude_term "
            "ORDER BY sort_order ASC, term_id ASC"
        ))
        conn.close()

    @staticmethod
    def poly4(coeffs: tuple[float, ...], x: float) -> float:
        return ((((coeffs[4] * x) + coeffs[3]) * x + coeffs[2]) * x + coeffs[1]) * x + coeffs[0]

    def base_llr(self, jd: float) -> tuple[float, float, float]:
        T = (jd - 2451545.0) / 36525.0
        e = 1.0 - 0.002516 * T - 0.0000074 * T * T
        Lp_deg = self.poly4(self.fund["Lp"], T)
        D_deg = self.poly4(self.fund["D"], T)
        M_deg = self.poly4(self.fund["M"], T)
        Mp_deg = self.poly4(self.fund["Mp"], T)
        F_deg = self.poly4(self.fund["F"], T)
        Lp = math.radians(normalize_degrees(Lp_deg))
        D = math.radians(normalize_degrees(D_deg))
        M = math.radians(normalize_degrees(M_deg))
        Mp = math.radians(normalize_degrees(Mp_deg))
        F = math.radians(normalize_degrees(F_deg))
        A1 = math.radians(normalize_degrees(119.75 + 131.849 * T))
        A2 = math.radians(normalize_degrees(53.09 + 479264.29 * T))
        A3 = math.radians(normalize_degrees(313.45 + 481266.484 * T))
        sl = 0.0
        sr = 0.0
        sb = 0.0

        for d, m, mp, f, lon_coeff, radius_coeff in self.lon_rows:
            arg = d * D + m * M + mp * Mp + f * F
            scale = e ** abs(m)
            sl += scale * lon_coeff * math.sin(arg)
            sr += scale * radius_coeff * math.cos(arg)
        for d, m, mp, f, lat_coeff in self.lat_rows:
            arg = d * D + m * M + mp * Mp + f * F
            scale = e ** abs(m)
            sb += scale * lat_coeff * math.sin(arg)

        sl += 3958.0 * math.sin(A1) + 1962.0 * math.sin(Lp - F) + 318.0 * math.sin(A2)
        sb += (
            -2235.0 * math.sin(Lp)
            + 382.0 * math.sin(A3)
            + 175.0 * math.sin(A1 - F)
            + 175.0 * math.sin(A1 + F)
            + 127.0 * math.sin(Lp - Mp)
            - 115.0 * math.sin(Lp + Mp)
        )
        lon_deg = normalize_degrees(Lp_deg + sl * 1.0e-6)
        lat_deg = sb * 1.0e-6
        radius_km = 385000.56 + sr / 1000.0
        return lon_deg, lat_deg, radius_km


def true_llr(jd: float) -> tuple[float, float, float]:
    et = sp.unitim(jd, "JDTDB", "ET")
    state, _ = sp.spkezr("MOON", et, "ECLIPJ2000", "NONE", "EARTH")
    x, y, z = state[:3]
    radius = math.sqrt(x * x + y * y + z * z)
    lon_deg = normalize_degrees(math.degrees(math.atan2(y, x)))
    lat_deg = math.degrees(math.asin(z / radius))
    return lon_deg, lat_deg, radius


def fit_segment(model: WorkbookMoonModel, start_jd: float, end_jd: float,
                sample_step_days: float) -> tuple[float, float, np.ndarray, np.ndarray, np.ndarray]:
    reference_jd = 0.5 * (start_jd + end_jd)
    span_days = max((end_jd - start_jd) * 0.5, 1.0)
    fit_jds = np.arange(start_jd, end_jd + 1.0e-9, sample_step_days)
    if fit_jds[-1] < end_jd:
        fit_jds = np.append(fit_jds, end_jd)
    x = (fit_jds - reference_jd) / span_days

    dlon = []
    dlat = []
    dradius = []
    for jd in fit_jds:
        base_lon, base_lat, base_radius = model.base_llr(float(jd))
        true_lon, true_lat, true_radius = true_llr(float(jd))
        dlon.append(angle_delta_degrees(true_lon, base_lon))
        dlat.append(true_lat - base_lat)
        dradius.append(true_radius - base_radius)

    lon_poly = np.polyfit(x, np.array(dlon, dtype=float), POLY_DEGREE)
    lat_poly = np.polyfit(x, np.array(dlat, dtype=float), POLY_DEGREE)
    radius_poly = np.polyfit(x, np.array(dradius, dtype=float), POLY_DEGREE)
    return reference_jd, span_days, lon_poly, lat_poly, radius_poly


def apply_poly(poly_desc: np.ndarray, x: float) -> float:
    return float(np.polyval(poly_desc, x))


def vector_from_llr(lon_deg: float, lat_deg: float, radius_km: float) -> np.ndarray:
    lon = math.radians(lon_deg)
    lat = math.radians(lat_deg)
    cos_lat = math.cos(lat)
    return np.array([
        radius_km * cos_lat * math.cos(lon),
        radius_km * cos_lat * math.sin(lon),
        radius_km * math.sin(lat),
    ], dtype=float)


def segment_error(model: WorkbookMoonModel, start_jd: float, end_jd: float,
                  reference_jd: float, span_days: float,
                  lon_poly: np.ndarray, lat_poly: np.ndarray, radius_poly: np.ndarray,
                  eval_step_days: float) -> tuple[float, float]:
    worst_arcsec = 0.0
    worst_km = 0.0
    jd = start_jd
    while jd < end_jd + 1.0e-9:
        base_lon, base_lat, base_radius = model.base_llr(jd)
        x = (jd - reference_jd) / span_days
        lon_deg = normalize_degrees(base_lon + apply_poly(lon_poly, x))
        lat_deg = base_lat + apply_poly(lat_poly, x)
        radius_km = base_radius + apply_poly(radius_poly, x)
        approx = vector_from_llr(lon_deg, lat_deg, radius_km)
        exact_lon, exact_lat, exact_radius = true_llr(jd)
        exact = vector_from_llr(exact_lon, exact_lat, exact_radius)
        dot = float(np.dot(approx, exact) / (np.linalg.norm(approx) * np.linalg.norm(exact)))
        dot = max(-1.0, min(1.0, dot))
        worst_arcsec = max(worst_arcsec, math.degrees(math.acos(dot)) * 3600.0)
        worst_km = max(worst_km, abs(np.linalg.norm(approx) - np.linalg.norm(exact)))
        jd += eval_step_days
    return worst_arcsec, worst_km


def emit_segment_sql(model_id: int, start_jd: float, end_jd: float,
                     reference_jd: float, span_days: float,
                     lon_poly: np.ndarray, lat_poly: np.ndarray, radius_poly: np.ndarray) -> str:
    lon = lon_poly[::-1]
    lat = lat_poly[::-1]
    radius = radius_poly[::-1]
    return (
        "INSERT INTO almanac_lunar_correction_model "
        "(model_id, body_code, start_jd, end_jd, reference_jd, span_days, "
        "lon_c0_deg, lon_c1_deg, lon_c2_deg, lon_c3_deg, lon_c4_deg, lon_c5_deg, lon_c6_deg, lon_c7_deg, "
        "lat_c0_deg, lat_c1_deg, lat_c2_deg, lat_c3_deg, lat_c4_deg, lat_c5_deg, lat_c6_deg, lat_c7_deg, "
        "radius_c0_km, radius_c1_km, radius_c2_km, radius_c3_km, radius_c4_km, radius_c5_km, radius_c6_km, radius_c7_km, "
        "sort_order) "
        f"VALUES ({model_id}, 'MOON', "
        f"{start_jd:.8f}, {end_jd:.8f}, {reference_jd:.8f}, {span_days:.8f}, "
        f"{lon[0]:.15f}, {lon[1]:.15f}, {lon[2]:.15f}, {lon[3]:.15f}, {lon[4]:.15f}, {lon[5]:.15f}, {lon[6]:.15f}, {lon[7]:.15f}, "
        f"{lat[0]:.15f}, {lat[1]:.15f}, {lat[2]:.15f}, {lat[3]:.15f}, {lat[4]:.15f}, {lat[5]:.15f}, {lat[6]:.15f}, {lat[7]:.15f}, "
        f"{radius[0]:.15f}, {radius[1]:.15f}, {radius[2]:.15f}, {radius[3]:.15f}, {radius[4]:.15f}, {radius[5]:.15f}, {radius[6]:.15f}, {radius[7]:.15f}, "
        f"{model_id * 10});"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--start-year", type=int, default=1550)
    parser.add_argument("--end-year", type=int, default=2649)
    parser.add_argument("--kernel-dir", default="", help="Directory containing NAIF kernels.")
    parser.add_argument("--sql-path", default="packaging/almanac-db/mars_almanac.sql",
                        help="SQL seed file containing the base workbook lunar tables.")
    parser.add_argument("--segment-days", type=float, default=365.25)
    parser.add_argument("--sample-step-days", type=float, default=1.0)
    parser.add_argument("--eval-step-days", type=float, default=10.0)
    parser.add_argument("--fragment", action="store_true",
                        help="Emit SQL body rows only without transaction prologue.")
    parser.add_argument("--model-id-base", type=int, default=1)
    args = parser.parse_args()

    model = WorkbookMoonModel(Path(args.sql_path))
    load_spice(spice_kernel_dir(args.kernel_dir.strip() or None))

    start_jd = jd_from_gregorian(args.start_year, 1, 1)
    end_jd = jd_from_gregorian(args.end_year + 1, 1, 1)
    cursor = start_jd
    model_id = args.model_id_base
    worst_arcsec = 0.0
    worst_km = 0.0
    rows: list[str] = []

    while cursor < end_jd - 1.0e-9:
        seg_end = min(end_jd, cursor + args.segment_days)
        reference_jd, span_days, lon_poly, lat_poly, radius_poly = fit_segment(
            model, cursor, seg_end, args.sample_step_days
        )
        seg_arcsec, seg_km = segment_error(
            model, cursor, seg_end, reference_jd, span_days,
            lon_poly, lat_poly, radius_poly, args.eval_step_days
        )
        worst_arcsec = max(worst_arcsec, seg_arcsec)
        worst_km = max(worst_km, seg_km)
        rows.append(emit_segment_sql(model_id, cursor, seg_end, reference_jd, span_days,
                                     lon_poly, lat_poly, radius_poly))
        model_id += 1
        cursor = seg_end

    print(
        f"-- MOON residual corrections: {len(rows)} segment(s), "
        f"worst angle {worst_arcsec:.3f} arcsec, worst distance {worst_km:.3f} km",
        file=sys.stderr,
        flush=True,
    )
    if not args.fragment:
        print("BEGIN TRANSACTION;")
        print("DELETE FROM almanac_lunar_correction_model;")
    for row in rows:
        print(row)
    if not args.fragment:
        print("COMMIT;")
    sp.kclear()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
