#!/usr/bin/env python3
"""
Generate piecewise polynomial orbital-element SQL for the MARS almanac database.

This uses SPICE only at build time. The generated output is intended for the
runtime orbital-elements backend, which reconstructs heliocentric x/y/z from
fitted element tables instead of storing direct Chebyshev state vectors.
"""

from __future__ import annotations

import argparse
import math
import os
from dataclasses import dataclass
from pathlib import Path
import sys

try:
    import numpy as np
except ModuleNotFoundError as exc:  # pragma: no cover
    print(f"Missing Python dependency for orbital-element generation: {exc}", file=sys.stderr)
    raise SystemExit(1)

try:
    import spiceypy as sp
except ModuleNotFoundError as exc:  # pragma: no cover
    print(f"Missing SPICE dependency for orbital-element generation: {exc}", file=sys.stderr)
    raise SystemExit(1)


SPICE_KERNEL_FILENAMES = ("naif0012.tls", "pck00011.tpc", "de440.bsp")
SUN_GM_KM3_PER_S2 = 132712440041.93938
TARGET_CODES = {
    "MERCURY": "MERCURY",
    "VENUS": "VENUS",
    "EARTH": "EARTH",
    "MARS": "MARS BARYCENTER",
    "JUPITER": "JUPITER BARYCENTER",
    "SATURN": "SATURN BARYCENTER",
    "URANUS": "URANUS BARYCENTER",
    "NEPTUNE": "NEPTUNE BARYCENTER",
}

BODY_CONFIG = {
    "MERCURY": dict(segment_days=3652.5, min_days=365.25, tolerance_arcsec=45.0, tolerance_au=3e-5),
    "VENUS": dict(segment_days=7305.0, min_days=365.25, tolerance_arcsec=45.0, tolerance_au=3e-5),
    "EARTH": dict(segment_days=7305.0, min_days=365.25, tolerance_arcsec=45.0, tolerance_au=3e-5),
    "MARS": dict(segment_days=3652.5, min_days=365.25, tolerance_arcsec=45.0, tolerance_au=4e-5),
    "JUPITER": dict(segment_days=14610.0, min_days=730.5, tolerance_arcsec=30.0, tolerance_au=8e-5),
    "SATURN": dict(segment_days=14610.0, min_days=730.5, tolerance_arcsec=30.0, tolerance_au=8e-5),
    "URANUS": dict(segment_days=29220.0, min_days=1461.0, tolerance_arcsec=20.0, tolerance_au=1.2e-4),
    "NEPTUNE": dict(segment_days=29220.0, min_days=1461.0, tolerance_arcsec=20.0, tolerance_au=1.2e-4),
}


@dataclass
class FittedSegment:
    body_code: str
    start_jd: float
    end_jd: float
    reference_jd: float
    span_days: float
    coeffs: dict[str, np.ndarray]


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


def vector_state_heliocentric(body_code: str, jd: float) -> np.ndarray:
    et = sp.unitim(jd, "JDTDB", "ET")
    state, _ = sp.spkezr(TARGET_CODES[body_code], et, "ECLIPJ2000", "NONE", "SUN")
    return np.array(state, dtype=float)


def osculating_elements(body_code: str, jd: float, mu_sun: float) -> tuple[float, float, float, float, float, float]:
    state = vector_state_heliocentric(body_code, jd)
    et = sp.unitim(jd, "JDTDB", "ET")
    rp, ecc, inc, lnode, argp, m0, _, _ = sp.oscelt(state, et, mu_sun)
    a = rp / (1.0 - ecc)
    omega = math.degrees(lnode) % 360.0
    inc_deg = math.degrees(inc)
    varpi = (math.degrees(argp + lnode)) % 360.0
    M = math.degrees(m0) % 360.0
    L = (M + varpi) % 360.0
    return omega, inc_deg, varpi, a / 149597870.7, ecc, L


def unwrap_degrees(values: np.ndarray) -> np.ndarray:
    return np.degrees(np.unwrap(np.radians(values)))


def fit_body_segment(body_code: str, start_jd: float, end_jd: float, mu_sun: float) -> FittedSegment:
    sample_count = max(17, int(math.ceil((end_jd - start_jd) / 45.0)) + 1)
    jds = np.linspace(start_jd, end_jd, sample_count)
    samples = np.array([osculating_elements(body_code, float(jd), mu_sun) for jd in jds], dtype=float)
    reference_jd = 0.5 * (start_jd + end_jd)
    span_days = max((end_jd - start_jd) * 0.5, 1.0)
    x = (jds - reference_jd) / span_days

    coeffs: dict[str, np.ndarray] = {}
    names = ("Omega", "i", "varpi", "a", "e", "L")
    for idx, name in enumerate(names):
        values = samples[:, idx]
        if name in ("Omega", "varpi", "L"):
            values = unwrap_degrees(values)
        coeffs[name] = np.polyfit(x, values, 2)

    return FittedSegment(body_code, start_jd, end_jd, reference_jd, span_days, coeffs)


def eval_poly(coeffs: np.ndarray, x: float) -> float:
    return (coeffs[0] * x + coeffs[1]) * x + coeffs[2]


def solve_kepler(mean_anomaly_radians: float, eccentricity: float) -> float:
    E = mean_anomaly_radians
    for _ in range(20):
        delta = (E - eccentricity * math.sin(E) - mean_anomaly_radians) / (1.0 - eccentricity * math.cos(E))
        E -= delta
        if abs(delta) < 1e-13:
            break
    return E


def position_from_elements(segment: FittedSegment, jd: float) -> np.ndarray:
    x = (jd - segment.reference_jd) / segment.span_days
    Omega = math.radians(eval_poly(segment.coeffs["Omega"], x))
    inc = math.radians(eval_poly(segment.coeffs["i"], x))
    varpi = eval_poly(segment.coeffs["varpi"], x)
    a = eval_poly(segment.coeffs["a"], x)
    e = eval_poly(segment.coeffs["e"], x)
    L = eval_poly(segment.coeffs["L"], x)
    M = math.radians((L - varpi) % 360.0)
    argp = math.radians((varpi - math.degrees(Omega)) % 360.0)
    E = solve_kepler(M, e)
    xv = a * (math.cos(E) - e)
    yv = a * math.sqrt(max(0.0, 1.0 - e * e)) * math.sin(E)
    v = math.atan2(yv, xv)
    r = math.hypot(xv, yv)
    cos_Omega = math.cos(Omega)
    sin_Omega = math.sin(Omega)
    cos_inc = math.cos(inc)
    sin_inc = math.sin(inc)
    cos_vw = math.cos(v + argp)
    sin_vw = math.sin(v + argp)
    return np.array([
        r * (cos_Omega * cos_vw - sin_Omega * sin_vw * cos_inc),
        r * (sin_Omega * cos_vw + cos_Omega * sin_vw * cos_inc),
        r * (sin_vw * sin_inc),
    ], dtype=float)


def vector_angular_error_arcsec(exact: np.ndarray, approx: np.ndarray) -> float:
    exact_unit = exact / np.linalg.norm(exact)
    approx_unit = approx / np.linalg.norm(approx)
    dot = float(np.clip(np.dot(exact_unit, approx_unit), -1.0, 1.0))
    return math.degrees(math.acos(dot)) * 3600.0


def segment_error(segment: FittedSegment) -> tuple[float, float]:
    xs = np.linspace(segment.start_jd, segment.end_jd, max(33, int(math.ceil((segment.end_jd - segment.start_jd) / 30.0)) + 1))
    worst_angle = 0.0
    worst_distance = 0.0
    for jd in xs:
        exact = vector_state_heliocentric(segment.body_code, float(jd))[:3] / 149597870.7
        approx = position_from_elements(segment, float(jd))
        worst_angle = max(worst_angle, vector_angular_error_arcsec(exact, approx))
        worst_distance = max(worst_distance, float(np.linalg.norm(exact - approx)))
    return worst_angle, worst_distance


def generate_segments_for_body(body_code: str, start_jd: float, end_jd: float, mu_sun: float) -> list[FittedSegment]:
    config = BODY_CONFIG[body_code]
    segments: list[FittedSegment] = []

    def recurse(seg_start: float, seg_end: float) -> None:
        segment = fit_body_segment(body_code, seg_start, seg_end, mu_sun)
        worst_angle, worst_distance = segment_error(segment)
        if (
            worst_angle <= float(config["tolerance_arcsec"])
            and worst_distance <= float(config["tolerance_au"])
        ) or (seg_end - seg_start) <= float(config["min_days"]):
            segments.append(segment)
            return
        mid = 0.5 * (seg_start + seg_end)
        recurse(seg_start, mid)
        recurse(mid, seg_end)

    cursor = start_jd
    while cursor < end_jd - 1e-9:
        seg_end = min(end_jd, cursor + float(config["segment_days"]))
        recurse(cursor, seg_end)
        cursor = seg_end
    return segments


def sql_quote(text: str) -> str:
    return "'" + text.replace("'", "''") + "'"


def emit_segment_sql(segment_id: int, segment: FittedSegment) -> str:
    c = segment.coeffs
    return (
        "INSERT INTO almanac_orbital_elements_model "
        "(model_id, body_code, start_jd, end_jd, reference_jd, span_days, "
        "Omega_c0, Omega_c1, Omega_c2, i_c0, i_c1, i_c2, varpi_c0, varpi_c1, varpi_c2, "
        "a_c0, a_c1, a_c2, e_c0, e_c1, e_c2, L_c0, L_c1, L_c2) "
        f"VALUES ({segment_id}, {sql_quote(segment.body_code)}, "
        f"{segment.start_jd:.8f}, {segment.end_jd:.8f}, {segment.reference_jd:.8f}, {segment.span_days:.8f}, "
        f"{c['Omega'][2]:.15f}, {c['Omega'][1]:.15f}, {c['Omega'][0]:.15f}, "
        f"{c['i'][2]:.15f}, {c['i'][1]:.15f}, {c['i'][0]:.15f}, "
        f"{c['varpi'][2]:.15f}, {c['varpi'][1]:.15f}, {c['varpi'][0]:.15f}, "
        f"{c['a'][2]:.15f}, {c['a'][1]:.15f}, {c['a'][0]:.15f}, "
        f"{c['e'][2]:.15f}, {c['e'][1]:.15f}, {c['e'][0]:.15f}, "
        f"{c['L'][2]:.15f}, {c['L'][1]:.15f}, {c['L'][0]:.15f});"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--start-year", type=int, default=1600)
    parser.add_argument("--end-year", type=int, default=2400)
    parser.add_argument("--kernel-dir", default="", help="Directory containing NAIF kernels.")
    parser.add_argument("--body", action="append", dest="bodies", default=[],
                        help="Generate only the named body code. May be supplied more than once.")
    parser.add_argument("--fragment", action="store_true",
                        help="Emit SQL body rows only without transaction prologue.")
    parser.add_argument("--model-id-base", type=int, default=1,
                        help="First orbital model row identifier to use in emitted SQL.")
    args = parser.parse_args()

    kernel_dir = spice_kernel_dir(args.kernel_dir.strip() or None)
    load_spice(kernel_dir)
    mu_sun = SUN_GM_KM3_PER_S2
    start_jd = jd_from_gregorian(args.start_year, 1, 1)
    end_jd = jd_from_gregorian(args.end_year + 1, 1, 1)
    bodies = args.bodies or list(BODY_CONFIG.keys())
    next_model_id = args.model_id_base

    if not args.fragment:
        print("BEGIN TRANSACTION;")
        print("DELETE FROM almanac_orbital_elements_model;")

    for body_code in bodies:
        if body_code not in BODY_CONFIG:
            raise RuntimeError(f"unknown body code: {body_code}")
        print(f"-- Fitting orbital elements for {body_code}", file=sys.stderr, flush=True)
        segments = generate_segments_for_body(body_code, start_jd, end_jd, mu_sun)
        print(f"-- {body_code}: {len(segments)} segment(s)", file=sys.stderr, flush=True)
        for segment in segments:
            print(emit_segment_sql(next_model_id, segment))
            next_model_id += 1
        if body_code == "EARTH":
            for segment in segments:
                sun_segment = FittedSegment(
                    body_code="SUN",
                    start_jd=segment.start_jd,
                    end_jd=segment.end_jd,
                    reference_jd=segment.reference_jd,
                    span_days=segment.span_days,
                    coeffs={name: values.copy() for name, values in segment.coeffs.items()},
                )
                print(emit_segment_sql(next_model_id, sun_segment))
                next_model_id += 1

    if not args.fragment:
        print("COMMIT;")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
